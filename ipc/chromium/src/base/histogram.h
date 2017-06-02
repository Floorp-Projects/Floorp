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
#include <vector>

#include "base/time.h"
#include "base/lock.h"

namespace base {

//------------------------------------------------------------------------------
// Provide easy general purpose histogram in a macro, just like stats counters.
// The first four macros use 50 buckets.

#define HISTOGRAM_TIMES(name, sample) HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromSeconds(10), 50)

#define HISTOGRAM_COUNTS(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000000, 50)

#define HISTOGRAM_COUNTS_100(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 100, 50)

#define HISTOGRAM_COUNTS_10000(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 10000, 50)

#define HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::Histogram::FactoryGet(name, min, max, bucket_count, \
                                            base::Histogram::kNoFlags); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->Add(sample); \
  } while (0)

#define HISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    HISTOGRAM_ENUMERATION(name, under_one_hundred, 101)

// For folks that need real specific times, use this to select a precise range
// of times you want plotted, and the number of buckets you want used.
#define HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
                                                base::Histogram::kNoFlags); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->AddTime(sample); \
  } while (0)

// DO NOT USE THIS.  It is being phased out, in favor of HISTOGRAM_CUSTOM_TIMES.
#define HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
                                                base::Histogram::kNoFlags); \
    DCHECK_EQ(name, counter->histogram_name()); \
    if ((sample) < (max)) counter->AddTime(sample); \
  } while (0)

// Support histograming of an enumerated value.  The samples should always be
// less than boundary_value.

#define HISTOGRAM_ENUMERATION(name, sample, boundary_value) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::LinearHistogram::FactoryGet(name, 1, boundary_value, \
          boundary_value + 1, base::Histogram::kNoFlags); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->Add(sample); \
  } while (0)

#define HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::CustomHistogram::FactoryGet(name, custom_ranges, \
                                                  base::Histogram::kNoFlags); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->Add(sample); \
  } while (0)


//------------------------------------------------------------------------------
// Define Debug vs non-debug flavors of macros.
#ifndef NDEBUG

#define DHISTOGRAM_TIMES(name, sample) HISTOGRAM_TIMES(name, sample)
#define DHISTOGRAM_COUNTS(name, sample) HISTOGRAM_COUNTS(name, sample)
#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) HISTOGRAM_PERCENTAGE(\
    name, under_one_hundred)
#define DHISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count)
#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count)
#define DHISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count)
#define DHISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    HISTOGRAM_ENUMERATION(name, sample, boundary_value)
#define DHISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges)

#else  // NDEBUG

#define DHISTOGRAM_TIMES(name, sample) do {} while (0)
#define DHISTOGRAM_COUNTS(name, sample) do {} while (0)
#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) do {} while (0)
#define DHISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    do {} while (0)
#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    do {} while (0)
#define DHISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    do {} while (0)
#define DHISTOGRAM_ENUMERATION(name, sample, boundary_value) do {} while (0)
#define DHISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    do {} while (0)

#endif  // NDEBUG

//------------------------------------------------------------------------------
// The following macros provide typical usage scenarios for callers that wish
// to record histogram data, and have the data submitted/uploaded via UMA.
// Not all systems support such UMA, but if they do, the following macros
// should work with the service.

#define UMA_HISTOGRAM_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromSeconds(10), 50)

#define UMA_HISTOGRAM_MEDIUM_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(10), \
    base::TimeDelta::FromMinutes(3), 50)

// Use this macro when times can routinely be much longer than 10 seconds.
#define UMA_HISTOGRAM_LONG_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromHours(1), 50)

#define UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
            base::Histogram::kUmaTargetedHistogramFlag); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->AddTime(sample); \
  } while (0)

// DO NOT USE THIS.  It is being phased out, in favor of HISTOGRAM_CUSTOM_TIMES.
#define UMA_HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
           base::Histogram::kUmaTargetedHistogramFlag); \
    DCHECK_EQ(name, counter->histogram_name()); \
    if ((sample) < (max)) counter->AddTime(sample); \
  } while (0)

#define UMA_HISTOGRAM_COUNTS(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000000, 50)

#define UMA_HISTOGRAM_COUNTS_100(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 100, 50)

#define UMA_HISTOGRAM_COUNTS_10000(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 10000, 50)

#define UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::Histogram::FactoryGet(name, min, max, bucket_count, \
          base::Histogram::kUmaTargetedHistogramFlag); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->Add(sample); \
  } while (0)

#define UMA_HISTOGRAM_MEMORY_KB(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1000, 500000, 50)

#define UMA_HISTOGRAM_MEMORY_MB(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000, 50)

#define UMA_HISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    UMA_HISTOGRAM_ENUMERATION(name, under_one_hundred, 101)

#define UMA_HISTOGRAM_BOOLEAN(name, sample) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::BooleanHistogram::FactoryGet(name, \
          base::Histogram::kUmaTargetedHistogramFlag); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->AddBoolean(sample); \
  } while (0)

#define UMA_HISTOGRAM_ENUMERATION(name, sample, boundary_value) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::LinearHistogram::FactoryGet(name, 1, boundary_value, \
          boundary_value + 1, base::Histogram::kUmaTargetedHistogramFlag); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->Add(sample); \
  } while (0)

#define UMA_HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) do { \
    static base::Histogram* counter(NULL); \
    if (!counter) \
      counter = base::CustomHistogram::FactoryGet(name, custom_ranges, \
          base::Histogram::kUmaTargetedHistogramFlag); \
    DCHECK_EQ(name, counter->histogram_name()); \
    counter->Add(sample); \
  } while (0)

//------------------------------------------------------------------------------

class BooleanHistogram;
class CustomHistogram;
class Histogram;
class LinearHistogram;

class Histogram {
 public:
  typedef int Sample;  // Used for samples (and ranges of samples).
  typedef int Count;  // Used to count samples in a bucket.
  static const Sample kSampleType_MAX = INT_MAX;
  // Initialize maximum number of buckets in histograms as 16,384.
  static const size_t kBucketCount_MAX;

  typedef std::vector<Count> Counts;
  typedef std::vector<Sample> Ranges;

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

  enum BucketLayout {
    EXPONENTIAL,
    LINEAR,
    CUSTOM
  };

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

  //----------------------------------------------------------------------------
  // Statistic values, developed over the life of the histogram.

  class SampleSet {
   public:
    explicit SampleSet();
    ~SampleSet();

    // None of the methods in this class are thread-safe.  Callers
    // must deal with locking themselves.

    // Adjust size of counts_ for use with given histogram.
    void Resize(const Histogram& histogram);

    // Accessor for histogram to make routine additions.
    void Accumulate(Sample value, Count count, size_t index);

    // Arithmetic manipulation of corresponding elements of the set.
    void Add(const SampleSet& other);

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);

    Count counts(size_t i) const {
       return counts_[i];
    }
    Count TotalCount() const;
    int64_t sum() const {
       return sum_;
    }
    int64_t redundant_count() const {
       return redundant_count_;
    }
    size_t size() const {
       return counts_.size();
    }

   protected:
    // Actual histogram data is stored in buckets, showing the count of values
    // that fit into each bucket.
    Counts counts_;

    // Save simple stats locally.  Note that this MIGHT get done in base class
    // without shared memory at some point.
    int64_t sum_;         // sum of samples.

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
  static Histogram* FactoryGet(const std::string& name,
                               Sample minimum,
                               Sample maximum,
                               size_t bucket_count,
                               Flags flags);
  static Histogram* FactoryTimeGet(const std::string& name,
                                   base::TimeDelta minimum,
                                   base::TimeDelta maximum,
                                   size_t bucket_count,
                                   Flags flags);

  void Add(int value);
  void Subtract(int value);

  // TODO: Currently recording_enabled_ is not used by any Histogram class, but
  //       rather examined only by the telemetry code (via IsRecordingEnabled).
  //       Move handling to Histogram's Add() etc after simplifying Histogram.
  void SetRecordingEnabled(bool aEnabled) { recording_enabled_ = aEnabled; };
  bool IsRecordingEnabled() const { return recording_enabled_; };

  // This method is an interface, used only by BooleanHistogram.
  virtual void AddBoolean(bool value);

  // Accept a TimeDelta to increment.
  void AddTime(TimeDelta time) {
    Add(static_cast<int>(time.InMilliseconds()));
  }

  virtual void AddSampleSet(const SampleSet& sample);

  virtual void Clear();

  // This method is an interface, used only by LinearHistogram.
  virtual void SetRangeDescriptions(const DescriptionPair descriptions[]);

  // The following methods provide graphical histogram displays.
  void WriteHTMLGraph(std::string* output) const;
  void WriteAscii(bool graph_it, const std::string& newline,
                  std::string* output) const;

  // Support generic flagging of Histograms.
  // 0x1 Currently used to mark this histogram to be recorded by UMA..
  // 0x8000 means print ranges in hex.
  void SetFlags(Flags flags) { flags_ = static_cast<Flags> (flags_ | flags); }
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
  const std::string& histogram_name() const { return histogram_name_; }
  Sample declared_min() const { return declared_min_; }
  Sample declared_max() const { return declared_max_; }
  virtual Sample ranges(size_t i) const;
  uint32_t range_checksum() const { return range_checksum_; }
  virtual size_t bucket_count() const;

  // Do a safe atomic snapshot of sample data.  The caller is assumed to
  // have exclusive access to the destination, |*sample|, and no locking
  // of it is done here.
  virtual void SnapshotSample(SampleSet* sample) const;

  virtual bool HasConstructorArguments(Sample minimum, Sample maximum,
                                       size_t bucket_count);

  virtual bool HasConstructorTimeDeltaArguments(TimeDelta minimum,
                                                TimeDelta maximum,
                                                size_t bucket_count);
  // Return true iff the range_checksum_ matches current ranges_ vector.
  bool HasValidRangeChecksum() const;

 protected:
  Histogram(const std::string& name, Sample minimum,
            Sample maximum, size_t bucket_count);
  Histogram(const std::string& name, TimeDelta minimum,
            TimeDelta maximum, size_t bucket_count);

  virtual ~Histogram();

  // Initialize ranges_ mapping.
  void InitializeBucketRange();

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

  //----------------------------------------------------------------------------
  // Accessors for derived classes.
  //----------------------------------------------------------------------------
  void SetBucketRange(size_t i, Sample value);

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

  // Write a common header message describing this histogram.
  void WriteAsciiHeader(const SampleSet& snapshot,
                        Count sample_count, std::string* output) const;

  // Write information about previous, current, and next buckets.
  // Information such as cumulative percentage, etc.
  void WriteAsciiBucketContext(const int64_t past, const Count current,
                               const int64_t remaining, const size_t i,
                               std::string* output) const;

  // Write textual description of the bucket contents (relative to histogram).
  // Output is the count in the buckets, as well as the percentage.
  void WriteAsciiBucketValue(Count current, double scaled_sum,
                             std::string* output) const;

  // Produce actual graph (set of blank vs non blank char's) for a bucket.
  void WriteAsciiBucketGraph(double current_size, double max_size,
                             std::string* output) const;

  //----------------------------------------------------------------------------
  // Table for generating Crc32 values.
  static const uint32_t kCrcTable[256];
  //----------------------------------------------------------------------------
  // Invariant values set at/near construction time

  // ASCII version of original name given to the constructor.  All identically
  // named instances will be coalesced cross-project.
  const std::string histogram_name_;
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

  // When false, new samples are completely ignored.
  mozilla::Atomic<bool, mozilla::Relaxed> recording_enabled_;

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
  static Histogram* FactoryGet(const std::string& name,
                               Sample minimum,
                               Sample maximum,
                               size_t bucket_count,
                               Flags flags);
  static Histogram* FactoryTimeGet(const std::string& name,
                                   TimeDelta minimum,
                                   TimeDelta maximum,
                                   size_t bucket_count,
                                   Flags flags);

  // Overridden from Histogram:
  virtual ClassType histogram_type() const;

  virtual void Accumulate(Sample value, Count count, size_t index);

  // Store a list of number/text values for use in rendering the histogram.
  // The last element in the array has a null in its "description" slot.
  virtual void SetRangeDescriptions(const DescriptionPair descriptions[]);

 protected:
  LinearHistogram(const std::string& name, Sample minimum,
                  Sample maximum, size_t bucket_count);

  LinearHistogram(const std::string& name, TimeDelta minimum,
                  TimeDelta maximum, size_t bucket_count);

  // Initialize ranges_ mapping.
  void InitializeBucketRange();
  virtual double GetBucketSize(Count current, size_t i) const;

  // If we have a description for a bucket, then return that.  Otherwise
  // let parent class provide a (numeric) description.
  virtual const std::string GetAsciiBucketRange(size_t i) const;

  // Skip printing of name for numeric range if we have a name (and if this is
  // an empty bucket).
  virtual bool PrintEmptyBucket(size_t index) const;

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
  static Histogram* FactoryGet(const std::string& name, Flags flags);

  virtual ClassType histogram_type() const;

  virtual void AddBoolean(bool value);

  virtual void Accumulate(Sample value, Count count, size_t index);

 protected:
  explicit BooleanHistogram(const std::string& name);

  DISALLOW_COPY_AND_ASSIGN(BooleanHistogram);
};

//------------------------------------------------------------------------------

// FlagHistogram is like boolean histogram, but only allows a single off/on value.
class FlagHistogram : public BooleanHistogram
{
public:
  static Histogram *FactoryGet(const std::string &name, Flags flags);

  virtual ClassType histogram_type() const;

  virtual void Accumulate(Sample value, Count count, size_t index);

  virtual void AddSampleSet(const SampleSet& sample);

  virtual void Clear();

private:
  explicit FlagHistogram(const std::string &name);
  bool mSwitched;

  DISALLOW_COPY_AND_ASSIGN(FlagHistogram);
};

// CountHistogram only allows a single monotic counter value.
class CountHistogram : public LinearHistogram
{
public:
  static Histogram *FactoryGet(const std::string &name, Flags flags);

  virtual ClassType histogram_type() const;

  virtual void Accumulate(Sample value, Count count, size_t index);

  virtual void AddSampleSet(const SampleSet& sample);

private:
  explicit CountHistogram(const std::string &name);

  DISALLOW_COPY_AND_ASSIGN(CountHistogram);
};

//------------------------------------------------------------------------------

// CustomHistogram is a histogram for a set of custom integers.
class CustomHistogram : public Histogram {
 public:

  static Histogram* FactoryGet(const std::string& name,
                               const std::vector<Sample>& custom_ranges,
                               Flags flags);

  // Overridden from Histogram:
  virtual ClassType histogram_type() const;

 protected:
  CustomHistogram(const std::string& name,
                  const std::vector<Sample>& custom_ranges);

  // Initialize ranges_ mapping.
  void InitializedCustomBucketRange(const std::vector<Sample>& custom_ranges);
  virtual double GetBucketSize(Count current, size_t i) const;

  DISALLOW_COPY_AND_ASSIGN(CustomHistogram);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_H_
