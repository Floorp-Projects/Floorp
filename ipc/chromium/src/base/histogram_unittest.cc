// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of Histogram class

#include "base/metrics/histogram.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class HistogramTest : public testing::Test {
};

// Check for basic syntax and use.
TEST(HistogramTest, StartupShutdownTest) {
  // Try basic construction
  Histogram* histogram(Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), histogram);
  Histogram* histogram1(Histogram::FactoryGet(
      "Test1Histogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), histogram1);
  EXPECT_NE(histogram, histogram1);


  Histogram* linear_histogram(LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), linear_histogram);
  Histogram* linear_histogram1(LinearHistogram::FactoryGet(
      "Test1LinearHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), linear_histogram1);
  EXPECT_NE(linear_histogram, linear_histogram1);

  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  custom_ranges.push_back(10);
  custom_ranges.push_back(20);
  custom_ranges.push_back(30);
  Histogram* custom_histogram(CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), custom_histogram);
  Histogram* custom_histogram1(CustomHistogram::FactoryGet(
      "Test1CustomHistogram", custom_ranges, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), custom_histogram1);

  // Use standard macros (but with fixed samples)
  HISTOGRAM_TIMES("Test2Histogram", TimeDelta::FromDays(1));
  HISTOGRAM_COUNTS("Test3Histogram", 30);

  DHISTOGRAM_TIMES("Test4Histogram", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS("Test5Histogram", 30);

  HISTOGRAM_ENUMERATION("Test6Histogram", 129, 130);

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
  Histogram* histogram(Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), histogram);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(1U, histograms.size());
  Histogram* histogram1(Histogram::FactoryGet(
      "Test1Histogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), histogram1);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(2U, histograms.size());

  Histogram* linear_histogram(LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), linear_histogram);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(3U, histograms.size());

  Histogram* linear_histogram1(LinearHistogram::FactoryGet(
      "Test1LinearHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), linear_histogram1);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(4U, histograms.size());

  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  custom_ranges.push_back(10);
  custom_ranges.push_back(20);
  custom_ranges.push_back(30);
  Histogram* custom_histogram(CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), custom_histogram);
  Histogram* custom_histogram1(CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), custom_histogram1);

  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(5U, histograms.size());

  // Use standard macros (but with fixed samples)
  HISTOGRAM_TIMES("Test2Histogram", TimeDelta::FromDays(1));
  HISTOGRAM_COUNTS("Test3Histogram", 30);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(7U, histograms.size());

  HISTOGRAM_ENUMERATION("TestEnumerationHistogram", 20, 200);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(8U, histograms.size());

  DHISTOGRAM_TIMES("Test4Histogram", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS("Test5Histogram", 30);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
#ifndef NDEBUG
  EXPECT_EQ(10U, histograms.size());
#else
  EXPECT_EQ(8U, histograms.size());
#endif
}

TEST(HistogramTest, RangeTest) {
  StatisticsRecorder recorder;
  StatisticsRecorder::Histograms histograms;

  recorder.GetHistograms(&histograms);
  EXPECT_EQ(0U, histograms.size());

  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));  // As per header file.
  // Check that we got a nice exponential when there was enough rooom.
  EXPECT_EQ(0, histogram->ranges(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, histogram->ranges(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(INT_MAX, histogram->ranges(8));

  Histogram* short_histogram(Histogram::FactoryGet(
      "Histogram Shortened", 1, 7, 8, Histogram::kNoFlags));
  // Check that when the number of buckets is short, we get a linear histogram
  // for lack of space to do otherwise.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, short_histogram->ranges(i));
  EXPECT_EQ(INT_MAX, short_histogram->ranges(8));

  Histogram* linear_histogram(LinearHistogram::FactoryGet(
      "Linear", 1, 7, 8, Histogram::kNoFlags));
  // We also get a nice linear set of bucket ranges when we ask for it
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, linear_histogram->ranges(i));
  EXPECT_EQ(INT_MAX, linear_histogram->ranges(8));

  Histogram* linear_broad_histogram(LinearHistogram::FactoryGet(
      "Linear widened", 2, 14, 8, Histogram::kNoFlags));
  // ...but when the list has more space, then the ranges naturally spread out.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(2 * i, linear_broad_histogram->ranges(i));
  EXPECT_EQ(INT_MAX, linear_broad_histogram->ranges(8));

  Histogram* transitioning_histogram(Histogram::FactoryGet(
      "LinearAndExponential", 1, 32, 15, Histogram::kNoFlags));
  // When space is a little tight, we transition from linear to exponential.
  EXPECT_EQ(0, transitioning_histogram->ranges(0));
  EXPECT_EQ(1, transitioning_histogram->ranges(1));
  EXPECT_EQ(2, transitioning_histogram->ranges(2));
  EXPECT_EQ(3, transitioning_histogram->ranges(3));
  EXPECT_EQ(4, transitioning_histogram->ranges(4));
  EXPECT_EQ(5, transitioning_histogram->ranges(5));
  EXPECT_EQ(6, transitioning_histogram->ranges(6));
  EXPECT_EQ(7, transitioning_histogram->ranges(7));
  EXPECT_EQ(9, transitioning_histogram->ranges(8));
  EXPECT_EQ(11, transitioning_histogram->ranges(9));
  EXPECT_EQ(14, transitioning_histogram->ranges(10));
  EXPECT_EQ(17, transitioning_histogram->ranges(11));
  EXPECT_EQ(21, transitioning_histogram->ranges(12));
  EXPECT_EQ(26, transitioning_histogram->ranges(13));
  EXPECT_EQ(32, transitioning_histogram->ranges(14));
  EXPECT_EQ(INT_MAX, transitioning_histogram->ranges(15));

  std::vector<int> custom_ranges;
  custom_ranges.push_back(0);
  custom_ranges.push_back(9);
  custom_ranges.push_back(10);
  custom_ranges.push_back(11);
  custom_ranges.push_back(300);
  Histogram* test_custom_histogram(CustomHistogram::FactoryGet(
      "TestCustomRangeHistogram", custom_ranges, Histogram::kNoFlags));

  EXPECT_EQ(custom_ranges[0], test_custom_histogram->ranges(0));
  EXPECT_EQ(custom_ranges[1], test_custom_histogram->ranges(1));
  EXPECT_EQ(custom_ranges[2], test_custom_histogram->ranges(2));
  EXPECT_EQ(custom_ranges[3], test_custom_histogram->ranges(3));
  EXPECT_EQ(custom_ranges[4], test_custom_histogram->ranges(4));

  recorder.GetHistograms(&histograms);
  EXPECT_EQ(6U, histograms.size());
}

TEST(HistogramTest, CustomRangeTest) {
  StatisticsRecorder recorder;
  StatisticsRecorder::Histograms histograms;

  // Check that missing leading zero is handled by an auto-insertion.
  std::vector<int> custom_ranges;
  // Don't include a zero.
  custom_ranges.push_back(9);
  custom_ranges.push_back(10);
  custom_ranges.push_back(11);
  Histogram* test_custom_histogram(CustomHistogram::FactoryGet(
      "TestCustomRangeHistogram", custom_ranges, Histogram::kNoFlags));

  EXPECT_EQ(0, test_custom_histogram->ranges(0));  // Auto added
  EXPECT_EQ(custom_ranges[0], test_custom_histogram->ranges(1));
  EXPECT_EQ(custom_ranges[1], test_custom_histogram->ranges(2));
  EXPECT_EQ(custom_ranges[2], test_custom_histogram->ranges(3));

  // Check that unsorted data with dups is handled gracefully.
  const int kSmall = 7;
  const int kMid = 8;
  const int kBig = 9;
  custom_ranges.clear();
  custom_ranges.push_back(kBig);
  custom_ranges.push_back(kMid);
  custom_ranges.push_back(kSmall);
  custom_ranges.push_back(kSmall);
  custom_ranges.push_back(kMid);
  custom_ranges.push_back(0);  // Push an explicit zero.
  custom_ranges.push_back(kBig);

  Histogram* unsorted_histogram(CustomHistogram::FactoryGet(
      "TestCustomUnsortedDupedHistogram", custom_ranges, Histogram::kNoFlags));
  EXPECT_EQ(0, unsorted_histogram->ranges(0));
  EXPECT_EQ(kSmall, unsorted_histogram->ranges(1));
  EXPECT_EQ(kMid, unsorted_histogram->ranges(2));
  EXPECT_EQ(kBig, unsorted_histogram->ranges(3));
}


// Make sure histogram handles out-of-bounds data gracefully.
TEST(HistogramTest, BoundsTest) {
  const size_t kBucketCount = 50;
  Histogram* histogram(Histogram::FactoryGet(
      "Bounded", 10, 100, kBucketCount, Histogram::kNoFlags));

  // Put two samples "out of bounds" above and below.
  histogram->Add(5);
  histogram->Add(-50);

  histogram->Add(100);
  histogram->Add(10000);

  // Verify they landed in the underflow, and overflow buckets.
  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(2, sample.counts(0));
  EXPECT_EQ(0, sample.counts(1));
  size_t array_size = histogram->bucket_count();
  EXPECT_EQ(kBucketCount, array_size);
  EXPECT_EQ(0, sample.counts(array_size - 2));
  EXPECT_EQ(2, sample.counts(array_size - 1));
}

// Check to be sure samples land as expected is "correct" buckets.
TEST(HistogramTest, BucketPlacementTest) {
  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));  // As per header file.

  // Check that we got a nice exponential since there was enough rooom.
  EXPECT_EQ(0, histogram->ranges(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, histogram->ranges(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(INT_MAX, histogram->ranges(8));

  // Add i+1 samples to the i'th bucket.
  histogram->Add(0);
  power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    for (int j = 0; j <= i; j++)
      histogram->Add(power_of_2);
    power_of_2 *= 2;
  }
  // Leave overflow bucket empty.

  // Check to see that the bucket counts reflect our additions.
  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(INT_MAX, histogram->ranges(8));
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i + 1, sample.counts(i));
}

}  // namespace

//------------------------------------------------------------------------------
// We can't be an an anonymous namespace while being friends, so we pop back
// out to the base namespace here.  We need to be friends to corrupt the
// internals of the histogram and/or sampleset.
TEST(HistogramTest, CorruptSampleCounts) {
  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));  // As per header file.

  EXPECT_EQ(0, histogram->sample_.redundant_count());
  histogram->Add(20);  // Add some samples.
  histogram->Add(40);
  EXPECT_EQ(2, histogram->sample_.redundant_count());

  Histogram::SampleSet snapshot;
  histogram->SnapshotSample(&snapshot);
  EXPECT_EQ(Histogram::NO_INCONSISTENCIES, 0);
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));  // No default corruption.
  EXPECT_EQ(2, snapshot.redundant_count());

  snapshot.counts_[3] += 100;  // Sample count won't match redundant count.
  EXPECT_EQ(Histogram::COUNT_LOW_ERROR, histogram->FindCorruption(snapshot));
  snapshot.counts_[2] -= 200;
  EXPECT_EQ(Histogram::COUNT_HIGH_ERROR, histogram->FindCorruption(snapshot));

  // But we can't spot a corruption if it is compensated for.
  snapshot.counts_[1] += 100;
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));
}

TEST(HistogramTest, CorruptBucketBounds) {
  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));  // As per header file.

  Histogram::SampleSet snapshot;
  histogram->SnapshotSample(&snapshot);
  EXPECT_EQ(Histogram::NO_INCONSISTENCIES, 0);
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));  // No default corruption.

  std::swap(histogram->ranges_[1], histogram->ranges_[2]);
  EXPECT_EQ(Histogram::BUCKET_ORDER_ERROR | Histogram::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(snapshot));

  std::swap(histogram->ranges_[1], histogram->ranges_[2]);
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));

  ++histogram->ranges_[3];
  EXPECT_EQ(Histogram::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(snapshot));

  // Show that two simple changes don't offset each other
  --histogram->ranges_[4];
  EXPECT_EQ(Histogram::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(snapshot));

  // Repair histogram so that destructor won't DCHECK().
  --histogram->ranges_[3];
  ++histogram->ranges_[4];
}

// Table was generated similarly to sample code for CRC-32 given on:
// http://www.w3.org/TR/PNG/#D-CRCAppendix.
TEST(HistogramTest, Crc32TableTest) {
  for (int i = 0; i < 256; ++i) {
    uint32 checksum = i;
    for (int j = 0; j < 8; ++j) {
      const uint32 kReversedPolynomial = 0xedb88320L;
      if (checksum & 1)
        checksum = kReversedPolynomial ^ (checksum >> 1);
      else
        checksum >>= 1;
    }
    EXPECT_EQ(Histogram::kCrcTable[i], checksum);
  }
}

}  // namespace base
