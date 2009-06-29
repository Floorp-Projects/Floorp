// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of Histogram class

#include "base/histogram.h"
#include "base/string_util.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {

class HistogramTest : public testing::Test {
};

// Check for basic syntax and use.
TEST(HistogramTest, StartupShutdownTest) {
  // Try basic construction
  Histogram histogram("TestHistogram", 1, 1000, 10);
  Histogram histogram1("Test1Histogram", 1, 1000, 10);

  LinearHistogram linear_histogram("TestLinearHistogram", 1, 1000, 10);
  LinearHistogram linear_histogram1("Test1LinearHistogram", 1, 1000, 10);

  // Use standard macros (but with fixed samples)
  HISTOGRAM_TIMES("Test2Histogram", TimeDelta::FromDays(1));
  HISTOGRAM_COUNTS("Test3Histogram", 30);

  DHISTOGRAM_TIMES("Test4Histogram", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS("Test5Histogram", 30);

  ASSET_HISTOGRAM_COUNTS("Test6Histogram", 129);

  // Try to construct samples.
  Histogram::SampleSet sample1;
  Histogram::SampleSet sample2;

  // Use copy constructor of SampleSet
  sample1 = sample2;
  Histogram::SampleSet sample3(sample1);

  // Finally test a statistics recorder, without really using it.
  StatisticsRecorder recorder;
}

// Repeat with a recorder present to register with.
TEST(HistogramTest, RecordedStartupTest) {
  // Test a statistics recorder, by letting histograms register.
  StatisticsRecorder recorder;  // This initializes the global state.

  StatisticsRecorder::Histograms histograms;
  EXPECT_EQ(0U, histograms.size());
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(0U, histograms.size());

  // Try basic construction
  Histogram histogram("TestHistogram", 1, 1000, 10);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(1U, histograms.size());
  Histogram histogram1("Test1Histogram", 1, 1000, 10);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(2U, histograms.size());

  LinearHistogram linear_histogram("TestLinearHistogram", 1, 1000, 10);
  LinearHistogram linear_histogram1("Test1LinearHistogram", 1, 1000, 10);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(4U, histograms.size());

  // Use standard macros (but with fixed samples)
  HISTOGRAM_TIMES("Test2Histogram", TimeDelta::FromDays(1));
  HISTOGRAM_COUNTS("Test3Histogram", 30);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(6U, histograms.size());

  ASSET_HISTOGRAM_COUNTS("TestAssetHistogram", 1000);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(7U, histograms.size());

  DHISTOGRAM_TIMES("Test4Histogram", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS("Test5Histogram", 30);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
#ifndef NDEBUG
  EXPECT_EQ(9U, histograms.size());
#else
  EXPECT_EQ(7U, histograms.size());
#endif
}

TEST(HistogramTest, RangeTest) {
  StatisticsRecorder recorder;
  StatisticsRecorder::Histograms histograms;

  recorder.GetHistograms(&histograms);
  EXPECT_EQ(0U, histograms.size());

  Histogram histogram("Histogram", 1, 64, 8);  // As mentioned in header file.
  // Check that we got a nice exponential when there was enough rooom.
  EXPECT_EQ(0, histogram.ranges(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, histogram.ranges(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(INT_MAX, histogram.ranges(8));

  Histogram short_histogram("Histogram Shortened", 1, 7, 8);
  // Check that when the number of buckets is short, we get a linear histogram
  // for lack of space to do otherwise.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, short_histogram.ranges(i));
  EXPECT_EQ(INT_MAX, short_histogram.ranges(8));

  LinearHistogram linear_histogram("Linear", 1, 7, 8);
  // We also get a nice linear set of bucket ranges when we ask for it
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, linear_histogram.ranges(i));
  EXPECT_EQ(INT_MAX, linear_histogram.ranges(8));

  LinearHistogram linear_broad_histogram("Linear widened", 2, 14, 8);
  // ...but when the list has more space, then the ranges naturally spread out.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(2 * i, linear_broad_histogram.ranges(i));
  EXPECT_EQ(INT_MAX, linear_broad_histogram.ranges(8));

  ThreadSafeHistogram threadsafe_histogram("ThreadSafe", 1, 32, 15);
  // When space is a little tight, we transition from linear to exponential.
  // This is what happens in both the basic histogram, and the threadsafe
  // variant (which is derived).
  EXPECT_EQ(0, threadsafe_histogram.ranges(0));
  EXPECT_EQ(1, threadsafe_histogram.ranges(1));
  EXPECT_EQ(2, threadsafe_histogram.ranges(2));
  EXPECT_EQ(3, threadsafe_histogram.ranges(3));
  EXPECT_EQ(4, threadsafe_histogram.ranges(4));
  EXPECT_EQ(5, threadsafe_histogram.ranges(5));
  EXPECT_EQ(6, threadsafe_histogram.ranges(6));
  EXPECT_EQ(7, threadsafe_histogram.ranges(7));
  EXPECT_EQ(9, threadsafe_histogram.ranges(8));
  EXPECT_EQ(11, threadsafe_histogram.ranges(9));
  EXPECT_EQ(14, threadsafe_histogram.ranges(10));
  EXPECT_EQ(17, threadsafe_histogram.ranges(11));
  EXPECT_EQ(21, threadsafe_histogram.ranges(12));
  EXPECT_EQ(26, threadsafe_histogram.ranges(13));
  EXPECT_EQ(32, threadsafe_histogram.ranges(14));
  EXPECT_EQ(INT_MAX, threadsafe_histogram.ranges(15));

  recorder.GetHistograms(&histograms);
  EXPECT_EQ(5U, histograms.size());
}

// Make sure histogram handles out-of-bounds data gracefully.
TEST(HistogramTest, BoundsTest) {
  const size_t kBucketCount = 50;
  Histogram histogram("Bounded", 10, 100, kBucketCount);

  // Put two samples "out of bounds" above and below.
  histogram.Add(5);
  histogram.Add(-50);

  histogram.Add(100);
  histogram.Add(10000);

  // Verify they landed in the underflow, and overflow buckets.
  Histogram::SampleSet sample;
  histogram.SnapshotSample(&sample);
  EXPECT_EQ(2, sample.counts(0));
  EXPECT_EQ(0, sample.counts(1));
  size_t array_size = histogram.bucket_count();
  EXPECT_EQ(kBucketCount, array_size);
  EXPECT_EQ(0, sample.counts(array_size - 2));
  EXPECT_EQ(2, sample.counts(array_size - 1));
}

// Check to be sure samples land as expected is "correct" buckets.
TEST(HistogramTest, BucketPlacementTest) {
  Histogram histogram("Histogram", 1, 64, 8);  // As mentioned in header file.

  // Check that we got a nice exponential since there was enough rooom.
  EXPECT_EQ(0, histogram.ranges(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, histogram.ranges(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(INT_MAX, histogram.ranges(8));

  // Add i+1 samples to the i'th bucket.
  histogram.Add(0);
  power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    for (int j = 0; j <= i; j++)
      histogram.Add(power_of_2);
    power_of_2 *= 2;
  }
  // Leave overflow bucket empty.

  // Check to see that the bucket counts reflect our additions.
  Histogram::SampleSet sample;
  histogram.SnapshotSample(&sample);
  EXPECT_EQ(INT_MAX, histogram.ranges(8));
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i + 1, sample.counts(i));
}

static const char kAssetTestHistogramName[] = "AssetCountTest";
static const char kAssetTestDebugHistogramName[] = "DAssetCountTest";
void AssetCountFunction(int sample) {
  ASSET_HISTOGRAM_COUNTS(kAssetTestHistogramName, sample);
  DASSET_HISTOGRAM_COUNTS(kAssetTestDebugHistogramName, sample);
}
// Check that asset can be added and removed from buckets.
TEST(HistogramTest, AssetCountTest) {
  // Start up a recorder system to identify all histograms.
  StatisticsRecorder recorder;

  // Call through the macro to instantiate the static variables.
  AssetCountFunction(100);  // Put a sample in the bucket for 100.

  // Find the histogram.
  StatisticsRecorder::Histograms histogram_list;
  StatisticsRecorder::GetHistograms(&histogram_list);
  ASSERT_NE(0U, histogram_list.size());
  const Histogram* our_histogram = NULL;
  const Histogram* our_debug_histogram = NULL;
  for (StatisticsRecorder::Histograms::iterator it = histogram_list.begin();
       it != histogram_list.end();
       ++it) {
    if (!(*it)->histogram_name().compare(kAssetTestHistogramName))
      our_histogram = *it;
    else if (!(*it)->histogram_name().compare(kAssetTestDebugHistogramName)) {
      our_debug_histogram = *it;
    }
  }
  ASSERT_TRUE(our_histogram);
#ifndef NDEBUG
  EXPECT_TRUE(our_debug_histogram);
#else
  EXPECT_FALSE(our_debug_histogram);
#endif
  // Verify it has a 1 in exactly one bucket (where we put the sample).
  Histogram::SampleSet sample;
  our_histogram->SnapshotSample(&sample);
  int match_count = 0;
  for (size_t i = 0; i < our_histogram->bucket_count(); ++i) {
    if (sample.counts(i) > 0) {
      EXPECT_LT(++match_count, 2) << "extra count in bucket " << i;
    }
  }
  EXPECT_EQ(1, match_count);

  // Remove our sample.
  AssetCountFunction(-100);  // Remove a sample from the bucket for 100.
  our_histogram->SnapshotSample(&sample);  // Extract data set.

  // Verify that the bucket is now empty, as are all the other buckets.
  for (size_t i = 0; i < our_histogram->bucket_count(); ++i) {
    EXPECT_EQ(0, sample.counts(i)) << "extra count in bucket " << i;
  }

  if (!our_debug_histogram)
    return;  // This is a production build.

  // Repeat test with debug histogram.  Note that insertion and deletion above
  // should have cancelled each other out.
  AssetCountFunction(100);  // Add a sample into the bucket for 100.
  our_debug_histogram->SnapshotSample(&sample);
  match_count = 0;
  for (size_t i = 0; i < our_debug_histogram->bucket_count(); ++i) {
    if (sample.counts(i) > 0) {
      EXPECT_LT(++match_count, 2) << "extra count in bucket " << i;
    }
  }
  EXPECT_EQ(1, match_count);

  // Remove our sample.
  AssetCountFunction(-100);  // Remove a sample from the bucket for 100.
  our_debug_histogram->SnapshotSample(&sample);  // Extract data set.

  // Verify that the bucket is now empty, as are all the other buckets.
  for (size_t i = 0; i < our_debug_histogram->bucket_count(); ++i) {
    EXPECT_EQ(0, sample.counts(i)) << "extra count in bucket " << i;
  }
}

}  // namespace
