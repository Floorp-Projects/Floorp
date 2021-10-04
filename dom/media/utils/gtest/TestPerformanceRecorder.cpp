/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <chrono>
#include <thread>

#include "PerformanceRecorder.h"
#include "gtest/gtest.h"
#include "nsString.h"

using Stage = mozilla::PerformanceRecorder::Stage;

class PerformanceRecorderWrapper : public mozilla::PerformanceRecorder {
 public:
  PerformanceRecorderWrapper(Stage aStage, int32_t aHeight)
      : PerformanceRecorder(aStage, aHeight) {}

  const char* Resolution() const { return FindMediaResolution(mHeight); }

  static void EnableMeasurementOnNonMarkerSituation() {
    sEnableMeasurementForTesting = true;
  }
};

TEST(PerformanceRecorder, TestResolution)
{
  static const struct {
    const int32_t mH;
    const char* mRes;
  } resolutions[] = {{0, "A:0"},
                     {240, "V:0<h<=240"},
                     {480, "V:240<h<=480"},
                     {576, "V:480<h<=576"},
                     {720, "V:576<h<=720"},
                     {1080, "V:720<h<=1080"},
                     {1440, "V:1080<h<=1440"},
                     {2160, "V:1440<h<=2160"},
                     {4320, "V:h>2160"}};

  const Stage stage = Stage::RequestDecode;
  for (auto&& res : resolutions) {
    PerformanceRecorderWrapper w(stage, res.mH);
    ASSERT_STREQ(w.Resolution(), res.mRes);
  }
}

TEST(PerformanceRecorder, TestMoveOperation)
{
  PerformanceRecorderWrapper::EnableMeasurementOnNonMarkerSituation();

  const Stage stage = Stage::RequestDecode;
  const uint32_t resolution = 1080;
  PerformanceRecorderWrapper w1(stage, resolution);
  w1.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // w1 has been moved which won't continue measuring data.
  PerformanceRecorderWrapper w2(std::move(w1));
  ASSERT_DOUBLE_EQ(w1.End(), 0.0);
  ASSERT_TRUE(w2.End() > 0.0);
}
