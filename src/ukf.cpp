#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // State dimension
  n_x_ = 5;
  // Augmented state dimension
  n_aug_ = 7;
  // Sigma point spreading parameter
  lambda_ = 3 - n_aug_;
  //  initially set to false, set to true in first call of ProcessMeasurement
  is_initialized_ = false;
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);
  x_ << 0, 0, 5, 0, 0;

  // initial covariance matrix
  P_ = MatrixXd(5, 5);
  P_ << 0.01, 0, 0, 0,0,
        0, 0.01, 0, 0,0,
        0, 0, 1, 0,0,
        0, 0, 0, 0.01,0,
        0, 0, 0, 0,0.01;
  //  predicted sigma points matrix
  Xsig_pred_ = MatrixXd (n_x_, 2 * n_aug_ + 1);
  //  time when the state is true, in us
  time_us_=0.0;
  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.3;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.15;

  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  ///* weights of sigma Points
  weights_ = VectorXd(2 * n_aug_ +1);
  // calc weights
  double weight_0 = lambda_ / (lambda_ +n_aug_);
  double weight = 0.5 / (n_aug_+lambda_);
  weights_(0) = weight_0;
  for (int i = 1; i < 2 *n_aug_+1; ++i)
  {
    weights_(i) = weight;
  }
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.

  /**
  TODO:
  Complete the initialization. See ukf.h for other member properties.
  Hint: one or more values initialized above might be wildly off...
  */
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:
  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if (!is_initialized_)
  { 
    time_us_ = meas_package.timestamp_;
    if (meas_package.sensor_type_ == MeasurementPackage::LASER)
    {
      x_(0) = meas_package.raw_measurements_(0);
      x_(1) = meas_package.raw_measurements_(1);
    } else if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
    {
      float ro = meas_package.raw_measurements_(0);
      float phi = meas_package.raw_measurements_(1);
      float ro_dot = meas_package.raw_measurements_(2);
      x_(0) = ro * cos(phi);
      x_(1) = ro * sin(phi);
    }
    is_initialized_ = true;
    return;
  }
  /******************************************************
  *  Prediction
  *******************************************************/
  // timestamp_ in seconds
  double dt = (meas_package.timestamp_ - time_us_) / 1000000.0; // in sec
  //cout<< dt<<endl;
  time_us_ = meas_package.timestamp_;
	Prediction(dt);  // pred x and P
  /******************************************************
  *  Update
  *******************************************************/
  if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_){
    UpdateLidar(meas_package);
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_){
    UpdateRadar(meas_package);
  }
  return;
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  /************************
  *Augment Sigma Points *
   ************************/
   // augmented mean vector
   VectorXd x_aug = VectorXd(7);
   // augmented state covariance
   MatrixXd P_aug = MatrixXd(7, 7);
   // augmented sigma point matrix and lambda_
   MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ +1);
   lambda_ = 3- n_aug_;
   // augmented mean state
   x_aug.head(5) = x_;
   x_aug(5) = 0.0;
   x_aug(6) = 0.0;
   // create augmented covariance matrix
   P_aug.fill(0.0);
   P_aug.topLeftCorner(5, 5) = P_;
   P_aug(5, 5) = std_a_*std_a_;
   P_aug(6, 6) = std_yawdd_*std_yawdd_;
   // augmented covariance matrix
   MatrixXd L = P_aug.llt().matrixL();
   // Remaining augmented sigma points
   Xsig_aug.col(0) = x_aug;
   for (int i = 0; i < n_aug_; i++){
     Xsig_aug.col(i+1) = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
     Xsig_aug.col(i+1+n_aug_) = x_aug -sqrt(lambda_ + n_aug_) * L.col(i);
   }
   /************************
   *Predict sigma points *
    ************************/
    //Predict sigma points
   for (int i = 0; i < 2*n_aug_ + 1; i++){
      // parameter
      double p_x      = Xsig_aug(0, i);
      double p_y      = Xsig_aug(1, i);
      double v        = Xsig_aug(2, i);
      double yaw      = Xsig_aug(3, i);
      double yawd     = Xsig_aug(4, i);
      double nu_a     = Xsig_aug(5, i);
      double nu_yawdd = Xsig_aug(6, i);

      // Prediction all the parameter
      //// px_p & py_p -- avoid division by zero  +  other parameter
      double px_p, py_p;
      if (fabs(yawd) > 0.001){
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p= p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
      }
      else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
      }
      double v_p = v;
      double yaw_p = yaw + yawd * delta_t;
      double yawd_p = yawd;
      //// add noise to every column
      px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
      py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
      v_p  = v_p + nu_a*delta_t;
      yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
      yawd_p = yawd_p + nu_yawdd*delta_t;
      //////　predicted sigma point matrix
      Xsig_pred_(0, i) = px_p;
      Xsig_pred_(1, i) = py_p;
      Xsig_pred_(2, i) = v_p;
      Xsig_pred_(3, i) = yaw_p;
      Xsig_pred_(4, i) = yawd_p;
    }
    /****************************************************
    * Convert Predicted Sigma Points to Mean/Covariance *
     ****************************************************/
     // predicted state mean
     x_.fill(0.0);
     for (int i = 0; i< 2* n_aug_ +1; i++){ //iterate over sigma points
       x_ = x_ + weights_(i) * Xsig_pred_.col(i);
     }
     // predict state covariance matrix
     P_.fill(0.0);
     for (int i = 0; i < 2 * n_aug_ + 1; i++){
       // state difference
       VectorXd x_diff = Xsig_pred_.col(i) - x_;
       //angle normalization
       while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
       while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;
       P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
     }
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  // extract measurement
  // VectorXd z = meas_package.raw_measurements_;
  // lidar, dimension = 2, px py
  int n_z = 2;
  // measurement sigma point = predict sigma point
  MatrixXd Zsig = MatrixXd(n_z, 2*n_aug_+1);
  for (int i = 0; i < 2*n_aug_+1; ++i){
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    Zsig(0, i) = p_x;
    Zsig(1, i) = p_y;
  }
  // mean
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i = 0; i < 2*n_aug_+1; ++i){
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }
  // covariance S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);
  /****************************
   **** Update for Lidar ******
   ****************************/
   // cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_,n_z);
  Tc.fill(0.0);
  for (int i = 0; i<2*n_aug_+1; i++){
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;
    S = S + weights_(i) * z_diff * z_diff.transpose();
    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
  //add noise
  MatrixXd R = MatrixXd (n_z, n_z);
  R << std_laspx_ * std_laspx_, 0,
      0, std_laspy_ * std_laspy_;
  S = S + R;
  // kalman gain K
  MatrixXd K = Tc * S.inverse();
  VectorXd z = VectorXd(n_z);
  z << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1);
  // residuals
  VectorXd z_diff = z - z_pred;
  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
  // Update state
  x_ = x_ + K * z_diff;
  // Update Covariance Matrix
  P_ = P_ - K*S*K.transpose();
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  // extract measurement
  // VectorXd Z = meas_package.raw_measurements_;
  // Radar, dimension = 3, r, phi, r_dot
  int n_z = 3;
  // measurement sigma point = predict sigma point
  VectorXd z_pred = VectorXd(n_z);
  MatrixXd Zsig = MatrixXd(n_z, 2*n_aug_+1);
  for (int i = 0; i < 2*n_aug_+1; i++){
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3, i);
    double v1 = cos(yaw) * v;
    double v2 = sin(yaw) * v;
    // coordinate transformation
    Zsig(0, i) = sqrt(p_x*p_x + p_y*p_y);
    Zsig(1, i) = atan2(p_y,p_x);
    if (Zsig(0, i) < 0.0001) Zsig(0, i) = 0.0001;
    Zsig(2, i) = (p_x*v1 +p_y*v2) / Zsig(0, i);
  }
  // mean
  
  z_pred.fill(0.0);
  for (int i = 0; i < 2*n_aug_+1; i++){
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }
  // covariance S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);

  /****************************
   **** Update for Radar ******
   ****************************/
   // cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_,n_z);
  Tc.fill(0.0);
  for (int i = 0; i<2*n_aug_+1; i++){
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)< -M_PI) z_diff(1)+=2.*M_PI;
    while (x_diff(3)>M_PI) x_diff(3) -=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3) +=2.*M_PI;
    S = S + weights_(i) * z_diff * z_diff.transpose();
    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
  //add noise
  MatrixXd R = MatrixXd (n_z, n_z);
  R << std_radr_ * std_radr_,             0,                      0,
      0,                      std_radphi_ * std_radphi_,          0,
      0,                                  0,              std_radrd_ * std_radrd_;
  S = S + R;
  // kalman gain K
  MatrixXd K = Tc * S.inverse();
  VectorXd z = VectorXd(3);
  z << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1), meas_package.raw_measurements_(2);
  //residual
  VectorXd z_diff = z - z_pred;
  while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;
  // Update state
  x_ = x_ + K * z_diff;
  // Update Covariance Matrix
  P_ = P_ - K*S*K.transpose();

}
