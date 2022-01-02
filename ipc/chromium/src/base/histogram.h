/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Histogram is an object that aggregates statistics, and can summarize them in
// various forms, including ASCII graphical, HTML, and numerically (as a
// vector of numbers corresponding to each of the aggregating buckets).

// It supports calls to accumulate either time intervals (which are processed
// as integral number of milliseconds), or arbitrary integral units.

// The default layout of buckets is exponential.  For example, buckets might
// contain (sequentially) the count of values in the following intervals:
// [0,1), [1,2), [2,4), [4,8), [8,16), [16,32), [32,64), [64,infinity)
// That bucket allocation would actually result from construction of a histogram
// for values between 1 and 64, with 8 buckets, such as:
// Histogram count(L"some name", 1, 64, 8);
// Note that the underflow bucket [0,1) and the overflow bucket [64,infinity)
// are not counted by the constructor in the user supplied "bucket_count"
// argument.
// The above example has an exponential ratio of 2 (doubling the bucket width
// in each consecutive bucket.  The Histogram class automatically calculates
// the smallest ratio that it can use to construct the number of buckets
// selected in the constructor.  An another example, if you had 50 buckets,
// and millisecond time values from 1 to 10000, then the ratio between
// consecutive bucket widths will be approximately somewhere around the 50th
// root of 10000.  This approach provides very fine grain (narrow) buckets
// at the low end of the histogram scale, but allows the histogram to cover a
// gigantic range with the addition of very few buckets.

// Histograms use a pattern involving a function static variable, that is a
// pointer to a histogram.  This static is explicitly initialized on any thread
// that detects a uninitialized (NULL) pointer.  The potentially racy
// initialization is not a problem as it is always set to point to the same
// value (i.e., the FactoryGet always returns the same value).  FactoryGet
// is also completely thread safe, which results in a completely thread safe,
// and relatively fast, set of counters.  To avoid races at shutdown, the static
// pointer is NOT deleted, and we leak the histograms at process termination.

#ifndef BASE_METRICS_HISTOGRAM_H_
#define BASE_METRICS_HISTOGRAM_H_
#pragma once

#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"

#include <map>
#include <string>

#include "base/time.h"

#include "nsTArray.h"

namespace base {

//------------------------------------------------------------------------------

class BooleanHistogram;
class CustomHistogram;
class Histogram;
class LinearHistogram;

class Histogram {
 public:
  typedef int Sample;  // Used for samples (and ranges of samples).
  typedef int Count;   // Used to count samples in a bucket.
  static const Sample kSampleType_MAX = INT_MAX;
  // Initialize maximum number of buckets in histograms as 16,384.
  static const size_t kBucketCount_MAX;

  typedef nsTArray<Count> Counts;
  typedef const Sample* Ranges;

  // These enums are used to facilitate deserialization of renderer histograms
  // into the browser.
  enum ClassType {
    HISTOGRAM,
    LINEAR_HISTOGRAM,
    BOOLEAN_HISTOGRAM,
    FLAG_HISTOGRAM,
    COUNT_HISTOGRAM,
    CUSTOM_HISTOGRAM,
    NOT_VALID_IN_RENDERER
  };

  enum BucketLayout { EXPONENTIAL, LINEAR, CUSTOM };

  enum Flags {
    kNoFlags = 0,
    kUmaTargetedHistogramFlag = 0x1,  // Histogram should be UMA uploaded.

    kHexRangePrintingFlag = 0x8000  // Fancy bucket-naming supported.
  };

  enum Inconsistencies {
    NO_INCONSISTENCIES = 0x0,
    RANGE_CHECKSUM_ERROR = 0x1,
    BUCKET_ORDER_ERROR = 0x2,
    COUNT_HIGH_ERROR = 0x4,
    COUNT_LOW_ERROR = 0x8,

    NEVER_EXCEEDED_VALUE = 0x10
  };

  struct DescriptionPair {
    Sample sample;
    const char* description;  // Null means end of a list of pairs.
  };

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  bool is_empty() const {
    return this->sample_.counts(0) == 0 && this->sample_.sum() == 0;
  }

  //----------------------------------------------------------------------------
  // Statistic values, developed over the life of the histogram.

  class SampleSet {
   public:
    explicit SampleSet();
    ~SampleSet();
    SampleSet(SampleSet&&) = default;

    // None of the methods in this class are thread-safe.  Callers
    // must deal with locking themselves.

    // Adjust size of counts_ for use with given histogram.
    void Resize(const Histogram& histogram);

    // Accessor for histogram to make routine additions.
    void Accumulate(Sample value, Count count, size_t index);

    // Arithmetic manipulation of corresponding elements of the set.
    void Add(const SampleSet& other);

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);

    Count counts(size_t i) const { return counts_[i]; }
    Count TotalCount() const;
    int64_t sum() const { return sum_; }
    int64_t redundant_count() const { return redundant_count_; }
    size_t size() const { return counts_.Length(); }
    SampleSet Clone() const {
      SampleSet result;
      result.counts_ = counts_.Clone();
      result.sum_ = sum_;
      result.redundant_count_ = redundant_count_;
      return result;
    }
    void Clear() {
      sum_ = 0;
      redundant_count_ = 0;
      for (int& i : counts_) {
        i = 0;
      }
    }

   protected:
    // Actual histogram data is stored in buckets, showing the count of values
    // that fit into each bucket.
    Counts counts_;

    // Save simple stats locally.  Note that this MIGHT get done in base class
    // without shared memory at some point.
    int64_t sum_;  // sum of samples.

    // To help identify memory corruption, we reduntantly save the number of
    // samples we've accumulated into all of our buckets.  We can compare this
    // count to the sum of the counts in all buckets, and detect problems.  Note
    // that due to races in histogram accumulation (if a histogram is indeed
    // updated on several threads simultaneously), the tallies might mismatch,
    // and also the snapshotting code may asynchronously get a mismatch (though
    // generally either race based mismatch cause is VERY rare).
    int64_t redundant_count_;
  };

  //----------------------------------------------------------------------------
  // minimum should start from 1. 0 is invalid as a minimum. 0 is an implicit
  // default underflow bucket.
  static Histogram* FactoryGet(Sample minimum, Sample maximum,
                               size_t bucket_count, Flags flags,
                               const int* buckets);

  virtual ~Histogram();

  void Add(int value);
  void Subtract(int value);

  // This method is an interface, used only by BooleanHistogram.
  virtual void AddBoolean(bool value);

  // Accept a TimeDelta to increment.
  void AddTime(TimeDelta time) { Add(static_cast<int>(time.InMilliseconds())); }

  virtual void AddSampleSet(const SampleSet& sample);

  virtual void Clear();

  // This method is an interface, used only by LinearHistogram.
  virtual void SetRangeDescriptions(const DescriptionPair descriptions[]);

  // Support generic flagging of Histograms.
  // 0x1 Currently used to mark this histogram to be recorded by UMA..
  // 0x8000 means print ranges in hex.
  void SetFlags(Flags flags) { flags_ = static_cast<Flags>(flags_ | flags); }
  void ClearFlags(Flags flags) { flags_ = static_cast<Flags>(flags_ & ~flags); }
  int flags() const { return flags_; }

  // Check to see if bucket ranges, counts and tallies in the snapshot are
  // consistent with the bucket ranges and checksums in our histogram.  This can
  // produce a false-alarm if a race occurred in the reading of the data during
  // a SnapShot process, but should otherwise be false at all times (unless we
  // have memory over-writes, or DRAM failures).
  virtual Inconsistencies FindCorruption(const SampleSet& snapshot) const;

  //----------------------------------------------------------------------------
  // Accessors for factory constuction, serialization and testing.
  //----------------------------------------------------------------------------
  virtual ClassType histogram_type() const;
  Sample declared_min() const { return declared_min_; }
  Sample declared_max() const { return declared_max_; }
  virtual Sample ranges(size_t i) const;
  uint32_t range_checksum() const { return range_checksum_; }
  virtual size_t bucket_count() const;

  virtual SampleSet SnapshotSample() const;

  virtual bool HasConstructorArguments(Sample minimum, Sample maximum,
                                       size_t bucket_count);

  virtual bool HasConstructorTimeDeltaArguments(TimeDelta minimum,
                                                TimeDelta maximum,
                                                size_t bucket_count);
  // Return true iff the range_checksum_ matches current ranges_ vector.
  bool HasValidRangeChecksum() const;

 protected:
  Histogram(Sample minimum, Sample maximum, size_t bucket_count);
  Histogram(TimeDelta minimum, TimeDelta maximum, size_t bucket_count);

  // Initialize ranges_ mapping from raw data.
  void InitializeBucketRangeFromData(const int* buckets);

  // Method to override to skip the display of the i'th bucket if it's empty.
  virtual bool PrintEmptyBucket(size_t index) const;

  //----------------------------------------------------------------------------
  // Methods to override to create histogram with different bucket widths.
  //----------------------------------------------------------------------------
  // Find bucket to increment for sample value.
  virtual size_t BucketIndex(Sample value) const;
  // Get normalized size, relative to the ranges_[i].
  virtual double GetBucketSize(Count current, size_t i) const;

  // Recalculate range_checksum_.
  void ResetRangeChecksum();

  // Return a string description of what goes in a given bucket.
  // Most commonly this is the numeric value, but in derived classes it may
  // be a name (or string description) given to the bucket.
  virtual const std::string GetAsciiBucketRange(size_t it) const;

  //----------------------------------------------------------------------------
  // Methods to override to create thread safe histogram.
  //----------------------------------------------------------------------------
  // Update all our internal data, including histogram
  virtual void Accumulate(Sample value, Count count, size_t index);

  // Validate that ranges_ was created sensibly (top and bottom range
  // values relate properly to the declared_min_ and declared_max_)..
  bool ValidateBucketRanges() const;

  virtual uint32_t CalculateRangeChecksum() const;

  // Finally, provide the state that changes with the addition of each new
  // sample.
  SampleSet sample_;

 private:
  // Post constructor initialization.
  void Initialize();

  // Checksum function for accumulating range values into a checksum.
  static uint32_t Crc32(uint32_t sum, Sample range);

  //----------------------------------------------------------------------------
  // Helpers for emitting Ascii graphic.  Each method appends data to output.

  // Find out how large the (graphically) the largest bucket will appear to be.
  double GetPeakBucketSize(const SampleSet& snapshot) const;

  //----------------------------------------------------------------------------
  // Table for generating Crc32 values.
  static const uint32_t kCrcTable[256];
  //----------------------------------------------------------------------------
  // Invariant values set at/near construction time

  Sample declared_min_;  // Less than this goes into counts_[0]
  Sample declared_max_;  // Over this goes into counts_[bucket_count_ - 1].
  size_t bucket_count_;  // Dimension of counts_[].

  // Flag the histogram for recording by UMA via metric_services.h.
  Flags flags_;

  // For each index, show the least value that can be stored in the
  // corresponding bucket. We also append one extra element in this array,
  // containing kSampleType_MAX, to make calculations easy.
  // The dimension of ranges_ is bucket_count + 1.
  Ranges ranges_;

  // For redundancy, we store a checksum of all the sample ranges when ranges
  // are generated.  If ever there is ever a difference, then the histogram must
  // have been corrupted.
  uint32_t range_checksum_;

  DISALLOW_COPY_AND_ASSIGN(Histogram);
};

//------------------------------------------------------------------------------

// LinearHistogram is a more traditional histogram, with evenly spaced
// buckets.
class LinearHistogram : public Histogram {
 public:
  virtual ~LinearHistogram();

  /* minimum should start from 1. 0 is as minimum is invalid. 0 is an implicit
     default underflow bucket. */
  static Histogram* FactoryGet(Sample minimum, Sample maximum,
                               size_t bucket_count, Flags flags,
                               const int* buckets);

  // Overridden from Histogram:
  virtual ClassType histogram_type() const override;

  virtual void Accumulate(Sample value, Count count, size_t index) override;

  // Store a list of number/text values for use in rendering the histogram.
  // The last element in the array has a null in its "description" slot.
  virtual void SetRangeDescriptions(
      const DescriptionPair descriptions[]) override;

 protected:
  LinearHistogram(Sample minimum, Sample maximum, size_t bucket_count);

  LinearHistogram(TimeDelta minimum, TimeDelta maximum, size_t bucket_count);

  virtual double GetBucketSize(Count current, size_t i) const override;

  // If we have a description for a bucket, then return that.  Otherwise
  // let parent class provide a (numeric) description.
  virtual const std::string GetAsciiBucketRange(size_t i) const override;

  // Skip printing of name for numeric range if we have a name (and if this is
  // an empty bucket).
  virtual bool PrintEmptyBucket(size_t index) const override;

 private:
  // For some ranges, we store a printable description of a bucket range.
  // If there is no desciption, then GetAsciiBucketRange() uses parent class
  // to provide a description.
  typedef std::map<Sample, std::string> BucketDescriptionMap;
  BucketDescriptionMap bucket_description_;

  DISALLOW_COPY_AND_ASSIGN(LinearHistogram);
};

//------------------------------------------------------------------------------

// BooleanHistogram is a histogram for booleans.
class BooleanHistogram : public LinearHistogram {
 public:
  static Histogram* FactoryGet(Flags flags, const int* buckets);

  virtual ClassType histogram_type() const override;

  virtual void AddBoolean(bool value) override;

  virtual void Accumulate(Sample value, Count count, size_t index) override;

 protected:
  explicit BooleanHistogram();

  DISALLOW_COPY_AND_ASSIGN(BooleanHistogram);
};

//------------------------------------------------------------------------------

// FlagHistogram is like boolean histogram, but only allows a single off/on
// value.
class FlagHistogram : public BooleanHistogram {
 public:
  static Histogram* FactoryGet(Flags flags, const int* buckets);

  virtual ClassType histogram_type() const override;

  virtual void Accumulate(Sample value, Count count, size_t index) override;

  virtual void AddSampleSet(const SampleSet& sample) override;

  virtual void Clear() override;

 private:
  explicit FlagHistogram();
  bool mSwitched;

  DISALLOW_COPY_AND_ASSIGN(FlagHistogram);
};

// CountHistogram only allows a single monotic counter value.
class CountHistogram : public LinearHistogram {
 public:
  static Histogram* FactoryGet(Flags flags, const int* buckets);

  virtual ClassType histogram_type() const override;

  virtual void Accumulate(Sample value, Count count, size_t index) override;

  virtual void AddSampleSet(const SampleSet& sample) override;

 private:
  explicit CountHistogram();

  DISALLOW_COPY_AND_ASSIGN(CountHistogram);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_H_
