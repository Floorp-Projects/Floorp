/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <chrono>
#include <thread>

#include "PerformanceRecorder.h"
#include "gtest/gtest.h"
#include "nsString.h"

using namespace mozilla;

class PerformanceRecorderWrapper : public PerformanceRecorder<PlaybackStage> {
 public:
  PerformanceRecorderWrapper(MediaStage aStage, int32_t aHeight)
      : PerformanceRecorder(aStage, aHeight) {}

  static void EnableMeasurementOnNonMarkerSituation() {
    sEnableMeasurementForTesting = true;
  }
};

TEST(PerformanceRecorder, TestResolution)
{
  PerformanceRecorderWrapper::EnableMeasurementOnNonMarkerSituation();

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

  const MediaStage stage = MediaStage::RequestDecode;
  for (auto&& res : resolutions) {
    PerformanceRecorderWrapper w(stage, res.mH);
    nsCString name;
    w.Record([&](auto& aStage) { name = nsCString(aStage.Name()); });
    ASSERT_NE(name.Find(res.mRes), kNotFound);
  }
}

TEST(PerformanceRecorder, TestMoveOperation)
{
  PerformanceRecorderWrapper::EnableMeasurementOnNonMarkerSituation();

  const MediaStage stage = MediaStage::RequestDecode;
  const uint32_t resolution = 1080;
  PerformanceRecorderWrapper w1(stage, resolution);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // w1 has been moved which won't continue measuring data.
  PerformanceRecorderWrapper w2(std::move(w1));
  ASSERT_DOUBLE_EQ(w1.Record(), 0.0);
  ASSERT_TRUE(w2.Record() > 0.0);
}

TEST(PerformanceRecorder, TestRecordInvalidation)
{
  PerformanceRecorderWrapper::EnableMeasurementOnNonMarkerSituation();

  const MediaStage stage = MediaStage::RequestDecode;
  const uint32_t resolution = 1080;
  PerformanceRecorderWrapper w(stage, resolution);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  ASSERT_TRUE(w.Record() > 0.0);

  w.Record();
  // w has been recorded and won't continue measuring data.
  ASSERT_DOUBLE_EQ(w.Record(), 0.0);
}

TEST(PerformanceRecorder, TestMultipleRecords)
{
  PerformanceRecorderWrapper::EnableMeasurementOnNonMarkerSituation();

  const MediaStage stage = MediaStage::RequestDecode;
  PerformanceRecorderMulti<PlaybackStage> r;

  r.Start(1, stage, 1);
  r.Start(2, stage, 2);
  r.Start(3, stage, 3);

  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // id 0 wasn't started
  EXPECT_DOUBLE_EQ(r.Record(0), 0.0);

  // id 1 gets recorded normally
  EXPECT_TRUE(r.Record(1) > 0.0);

  // id 1 was already recorded
  EXPECT_DOUBLE_EQ(r.Record(1), 0.0);

  // id 2 gets recorded normally
  EXPECT_TRUE(r.Record(2) > 0.0);

  // id 4 wasn't started
  EXPECT_DOUBLE_EQ(r.Record(4), 0.0);

  // All lower ids got discarded
  EXPECT_DOUBLE_EQ(r.Record(3), 0.0);
}
