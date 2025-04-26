#include <geometry/triangulation.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <Eigen/Dense>

MatX3d generateNoisyBearings(const MatX3d &bearings, double maxNoise) {
  MatX3d bearings_noisy = MatX3d::Zero(bearings.rows(), 3);
  for (int i = 0; i < bearings.rows(); ++i) {
    const Vec3d bearing = bearings.row(i);
    const Vec3d noise = maxNoise * Vec3d::Random();
    bearings_noisy.row(i) = (bearing + noise).normalized();
  }
  return bearings_noisy;
}

std::vector<Mat34d> generateRts(const MatX3d &centers) {
  const int num_cameras = centers.rows();
  std::vector<Mat34d> Rts(num_cameras);
  for (int i = 0; i < num_cameras; ++i) {
    const Vec3d center = centers.row(i);
    Rts[i] << 1.0, 0.0, 0.0, -center(0),  // clang-format off
              0.0, 1.0, 0.0, -center(1),
              0.0, 0.0, 1.0, -center(2);  // clang-format on
  }
  return Rts;
}

void generate_triangulation_data(const MatX3d &centers, MatX3d &bearings,
                                 MatX3d &bearings_noisy,
                                 std::vector<Mat34d> &Rts, Vec3d &gt_point) {
  const int num_cameras = centers.rows();

  gt_point = Vec3d(0.0, 0.0, 1.0);

  bearings = MatX3d::Zero(num_cameras, 3);
  for (int i = 0; i < num_cameras; ++i) {
    const Vec3d center = centers.row(i);
    bearings.row(i) = (gt_point - center).normalized();
  }

  bearings_noisy = generateNoisyBearings(bearings, 0.001);

  Rts = generateRts(centers);
}

class TwoCamsFixture : public ::testing::Test {
 public:
  TwoCamsFixture() {
    centers = MatX3d::Zero(2, 3);
    centers << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0;

    generate_triangulation_data(centers, bearings, bearings_noisy, Rts,
                                gt_point);
  }

  Vec3d gt_point;
  MatX3d centers;
  MatX3d bearings;
  MatX3d bearings_noisy;
  std::vector<Mat34d> Rts;

  const double threshold = 0.01;
  const std::vector<double> thresholds = {0.01, 0.01};
  const double min_angle = 2.0 * M_PI / 180.0;
  const double min_depth = 1e-6;
};

class FiveCamsFixture : public ::testing::Test {
 public:
  FiveCamsFixture() {
    const int num_cameras = 5;
    centers = MatX3d::Zero(num_cameras, 3);
    for (int i = 0; i < num_cameras; ++i) {
      centers.row(i) << 0.5 * i / num_cameras, 0.1 * i / num_cameras, 0.0;
    }
    generate_triangulation_data(centers, bearings, bearings_noisy, Rts,
                                gt_point);
  }

  Vec3d gt_point;
  MatX3d centers;
  MatX3d bearings;
  MatX3d bearings_noisy;
  std::vector<Mat34d> Rts;

  const double threshold = 0.01;
  const std::vector<double> thresholds = {0.01, 0.01, 0.01, 0.01, 0.01};
  const double min_angle = 2.0 * M_PI / 180.0;
  const double min_depth = 1e-6;
};

// This fixture is designed to test a specific failure case:
// When two cameras have the same center, but a third one has a
// different center. This would happen for example when two images
// are taken by the front and back cameras of a 360 camera and
// another image with a different camera or timestamp. As long as
// the bearings are consistent, we want triangulation to succeed
// in that case.
class ThreeCamsSameCenterFixture : public ::testing::Test {
 public:
  ThreeCamsSameCenterFixture() {
    centers = MatX3d::Zero(3, 3);
    centers << 0.0, 0.0, 0.0,  // clang-format off
               0.0, 0.0, 0.0,
               1.0, 0.0, 0.0;  // clang-format on

    generate_triangulation_data(centers, bearings, bearings_noisy, Rts,
                                gt_point);
  }

  Vec3d gt_point;
  MatX3d centers;
  MatX3d bearings;
  MatX3d bearings_noisy;
  std::vector<Mat34d> Rts;

  const double threshold = 0.01;
  const std::vector<double> thresholds = {0.01, 0.01, 0.01};
  const double min_angle = 2.0 * M_PI / 180.0;
  const double min_depth = 1e-6;
};

// This fixture is designed to test a specific failure case:
// When two cameras have the same center, we want the triangulation
// to fail. However, if the bearings are different, the triangulation
// may find a solution by putting the point at the center of the
// cameras. This is a bad solution and we want the functions to
// return non-success in this case.
class TwoCamsSameCenterFixture : public ::testing::Test {
 public:
  TwoCamsSameCenterFixture() {
    centers = MatX3d::Zero(2, 3);
    centers << 1.0, 0.0, 0.0,  // clang-format off
               1.0, 0.0, 0.0;  // clang-format on

    bearings = MatX3d::Zero(2, 3);
    bearings << 0.0, 0.0, 1.0,  // clang-format off
                1.0, 0.0, 0.0;  // clang-format on

    bearings_noisy = generateNoisyBearings(bearings, 0.001);

    Rts = generateRts(centers);
  }

  MatX3d centers;
  MatX3d bearings;
  MatX3d bearings_noisy;
  std::vector<Mat34d> Rts;

  const double threshold = 0.01;
  const std::vector<double> thresholds = {0.01, 0.01};
  const double min_angle = 2.0 * M_PI / 180.0;
  const double min_depth = 1e-6;
};

class TwoCamsManyPointsFixture : public ::testing::Test {
 public:
  TwoCamsManyPointsFixture() {
    gt_points.emplace_back(0.0, 0.0, 1.0);
    gt_points.emplace_back(1.0, 2.0, 3.0);

    rotation_1_2 = Eigen::AngleAxisd(0.1, Vec3d::UnitY()).toRotationMatrix();
    translation_1_2 << -1.0, 2.0, 0.2;

    bearings1 = MatX3d(gt_points.size(), 3);
    bearings2 = MatX3d(gt_points.size(), 3);
    for (int i = 0; i < gt_points.size(); ++i) {
      const Vec3d &gt_point = gt_points[i];
      bearings1.row(i) = gt_point.normalized();
      bearings2.row(i) =
          (rotation_1_2.transpose() * (gt_point - translation_1_2))
              .normalized();
    }

    bearings1_noisy = generateNoisyBearings(bearings1, 0.001);
    bearings2_noisy = generateNoisyBearings(bearings2, 0.001);
  }

  std::vector<Vec3d> gt_points;
  Mat3d rotation_1_2;
  Vec3d translation_1_2;
  MatX3d bearings1;
  MatX3d bearings2;
  MatX3d bearings1_noisy;
  MatX3d bearings2_noisy;
};

TEST_F(TwoCamsFixture, TriangulateBearingsDLT) {
  const auto [success, point] = geometry::TriangulateBearingsDLT(
      Rts, bearings, threshold, min_angle, min_depth);
  ASSERT_TRUE(success);
  ASSERT_LT((point - gt_point).norm(), 1e-6);

  const auto [success_noisy, point_noisy] = geometry::TriangulateBearingsDLT(
      Rts, bearings_noisy, threshold, min_angle, min_depth);
  ASSERT_TRUE(success_noisy);
  ASSERT_LT((point_noisy - gt_point).norm(), 0.01);
}

TEST_F(FiveCamsFixture, TriangulateBearingsDLT) {
  const auto [success, point] = geometry::TriangulateBearingsDLT(
      Rts, bearings, threshold, min_angle, min_depth);
  ASSERT_TRUE(success);
  ASSERT_LT((point - gt_point).norm(), 1e-6);

  const auto [success_noisy, point_noisy] = geometry::TriangulateBearingsDLT(
      Rts, bearings_noisy, threshold, min_angle, min_depth);
  ASSERT_TRUE(success_noisy);
  ASSERT_LT((point_noisy - gt_point).norm(), 0.01);
}

TEST_F(ThreeCamsSameCenterFixture, TriangulateBearingsDLT) {
  const auto [success, point] = geometry::TriangulateBearingsDLT(
      Rts, bearings, threshold, min_angle, min_depth);
  ASSERT_TRUE(success);
  ASSERT_LT((point - gt_point).norm(), 1e-6);

  const auto [success_noisy, point_noisy] = geometry::TriangulateBearingsDLT(
      Rts, bearings_noisy, threshold, min_angle, min_depth);
  ASSERT_TRUE(success_noisy);
  ASSERT_LT((point_noisy - gt_point).norm(), 0.01);
}

TEST_F(TwoCamsSameCenterFixture, TriangulateBearingsDLT) {
  const auto [success, point] = geometry::TriangulateBearingsDLT(
      Rts, bearings, threshold, min_angle, min_depth);
  ASSERT_FALSE(success);  // Expect failure due to coincident camera centers

  const auto [success_noisy, point_noisy] = geometry::TriangulateBearingsDLT(
      Rts, bearings_noisy, threshold, min_angle, min_depth);
  ASSERT_FALSE(
      success_noisy);  // Expect failure due to coincident camera centers

  // Test that without the positive depth constraint, triangulation succeeds
  // and returns the center of the cameras.
  const double negative_min_depth = -1e-6;
  const auto [success_no_depth_check, point_no_depth_check] =
      geometry::TriangulateBearingsMidpoint(centers, bearings, thresholds,
                                            min_angle, negative_min_depth);
  ASSERT_TRUE(success_no_depth_check);
  const Vec3d expected_point = centers.row(0);
  ASSERT_LT((point_no_depth_check - expected_point).norm(), 1e-6);
}

TEST_F(TwoCamsFixture, TriangulateBearingsMidpoint) {
  const auto [success, point] = geometry::TriangulateBearingsMidpoint(
      centers, bearings, thresholds, min_angle, min_depth);
  ASSERT_TRUE(success);
  ASSERT_LT((point - gt_point).norm(), 1e-6);

  const auto [success_noisy, point_noisy] =
      geometry::TriangulateBearingsMidpoint(centers, bearings_noisy, thresholds,
                                            min_angle, min_depth);
  ASSERT_TRUE(success_noisy);
  ASSERT_LT((point_noisy - gt_point).norm(), 0.01);
}

TEST_F(FiveCamsFixture, TriangulateBearingsMidpoint) {
  const auto [success, point] = geometry::TriangulateBearingsMidpoint(
      centers, bearings, thresholds, min_angle, min_depth);
  ASSERT_TRUE(success);
  ASSERT_LT((point - gt_point).norm(), 1e-6);

  const auto [success_noisy, point_noisy] =
      geometry::TriangulateBearingsMidpoint(centers, bearings_noisy, thresholds,
                                            min_angle, min_depth);
  ASSERT_TRUE(success_noisy);
  ASSERT_LT((point_noisy - gt_point).norm(), 0.01);
}

TEST_F(ThreeCamsSameCenterFixture, TriangulateBearingsMidpoint) {
  const auto [success, point] = geometry::TriangulateBearingsMidpoint(
      centers, bearings, thresholds, min_angle, min_depth);
  ASSERT_TRUE(success);
  ASSERT_LT((point - gt_point).norm(), 1e-6);

  const auto [success_noisy, point_noisy] =
      geometry::TriangulateBearingsMidpoint(centers, bearings_noisy, thresholds,
                                            min_angle, min_depth);
  ASSERT_TRUE(success_noisy);
  ASSERT_LT((point_noisy - gt_point).norm(), 0.01);
}

TEST_F(TwoCamsSameCenterFixture, TriangulateBearingsMidpoint) {
  const auto [success, point] = geometry::TriangulateBearingsMidpoint(
      centers, bearings, thresholds, min_angle, min_depth);
  ASSERT_FALSE(success);  // Expect failure due to coincident camera centers

  const auto [success_noisy, point_noisy] =
      geometry::TriangulateBearingsMidpoint(centers, bearings, thresholds,
                                            min_angle, min_depth);
  ASSERT_FALSE(
      success_noisy);  // Expect failure due to coincident camera centers

  // Test that without the positive depth constraint, triangulation succeeds
  // and returns the center of the cameras.
  const double negative_min_depth = -1e-6;
  const auto [success_no_depth_check, point_no_depth_check] =
      geometry::TriangulateBearingsMidpoint(centers, bearings, thresholds,
                                            min_angle, negative_min_depth);
  ASSERT_TRUE(success_no_depth_check);
  const Vec3d expected_point = centers.row(0);
  ASSERT_LT((point_no_depth_check - expected_point).norm(), 1e-6);
}

TEST_F(TwoCamsManyPointsFixture, TriangulateTwoBearingsMidpointMany) {
  const auto results = geometry::TriangulateTwoBearingsMidpointMany(
      bearings1, bearings2, rotation_1_2, translation_1_2);

  for (int i = 0; i < gt_points.size(); ++i) {
    const auto [success, point] = results[i];
    ASSERT_TRUE(success);
    ASSERT_LT((point - gt_points[i]).norm(), 1e-6);
  }

  const auto results_noisy = geometry::TriangulateTwoBearingsMidpointMany(
      bearings1_noisy, bearings2_noisy, rotation_1_2, translation_1_2);

  for (int i = 0; i < gt_points.size(); ++i) {
    const auto [success, point] = results_noisy[i];
    ASSERT_TRUE(success);
    ASSERT_LT((point - gt_points[i]).norm(), 0.01);
  }
}

TEST_F(TwoCamsManyPointsFixture, EpipolarAngleTwoBearingsMany) {
  MatXd angles = geometry::EpipolarAngleTwoBearingsMany(
      bearings1, bearings2, rotation_1_2, translation_1_2);
  ASSERT_EQ(angles.rows(), gt_points.size());
  ASSERT_EQ(angles.cols(), gt_points.size());
  for (int i = 0; i < gt_points.size(); ++i) {
    for (int j = 0; j < gt_points.size(); ++j) {
      if (i == j) {
        ASSERT_LT(angles(i, j), 1e-6);
      } else {
        ASSERT_GT(angles(i, j), 1e-6);
      }
    }
  }
}

TEST_F(TwoCamsFixture, PointRefinement) {
  const Vec3d initial_point = gt_point + Vec3d(0.1, 0.2, 0.3);
  const Vec3d refined =
      geometry::PointRefinement(centers, bearings, initial_point, 10);
  ASSERT_LT((refined - gt_point).norm(), 1e-6);
}
