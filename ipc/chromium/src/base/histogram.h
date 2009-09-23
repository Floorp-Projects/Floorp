// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
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

#ifndef BASE_HISTOGRAM_H_
#define BASE_HISTOGRAM_H_

#include <map>
#include <string>
#include <vector>

#include "base/lock.h"
#include "base/time.h"

//------------------------------------------------------------------------------
// Provide easy general purpose histogram in a macro, just like stats counters.
// The first four macros use 50 buckets.

#define HISTOGRAM_TIMES(name, sample) do { \
    static Histogram counter((name), base::TimeDelta::FromMilliseconds(1), \
                             base::TimeDelta::FromSeconds(10), 50); \
    counter.AddTime(sample); \
  } while (0)

#define HISTOGRAM_COUNTS(name, sample) do { \
    static Histogram counter((name), 1, 1000000, 50); \
    counter.Add(sample); \
  } while (0)

#define HISTOGRAM_COUNTS_100(name, sample) do { \
    static Histogram counter((name), 1, 100, 50); \
    counter.Add(sample); \
  } while (0)

#define HISTOGRAM_COUNTS_10000(name, sample) do { \
    static Histogram counter((name), 1, 10000, 50); \
    counter.Add(sample); \
  } while (0)

#define HISTOGRAM_PERCENTAGE(name, under_one_hundred) do { \
    static LinearHistogram counter((name), 1, 100, 101); \
    counter.Add(under_one_hundred); \
  } while (0)

// For folks that need real specific times, use this, but you'll only get
// samples that are in the range (overly large samples are discarded).
#define HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) do { \
    static Histogram counter((name), min, max, bucket_count); \
    if ((sample) < (max)) counter.AddTime(sample); \
  } while (0)

//------------------------------------------------------------------------------
// This macro set is for a histogram that can support both addition and removal
// of samples. It should be used to render the accumulated asset allocation
// of some samples.  For example, it can sample memory allocation sizes, and
// memory releases (as negative samples).
// To simplify the interface, only non-zero values can be sampled, with positive
// numbers indicating addition, and negative numbers implying dimunition
// (removal).
// Note that the underlying ThreadSafeHistogram() uses locking to ensure that
// counts are precise (no chance of losing an addition or removal event, due to
// multithread racing). This precision is required to prevent missed-counts from
// resulting in drift, as the calls to Remove() for a given value should always
// be equal in number or fewer than the corresponding calls to Add().

#define ASSET_HISTOGRAM_COUNTS(name, sample) do { \
    static ThreadSafeHistogram counter((name), 1, 1000000, 50); \
    if (0 == sample) break; \
    if (sample >= 0) \
      counter.Add(sample); \
    else\
      counter.Remove(-sample); \
  } while (0)

//------------------------------------------------------------------------------
// Define Debug vs non-debug flavors of macros.
#ifndef NDEBUG

#define DHISTOGRAM_TIMES(name, sample) HISTOGRAM_TIMES(name, sample)
#define DHISTOGRAM_COUNTS(name, sample) HISTOGRAM_COUNTS(name, sample)
#define DASSET_HISTOGRAM_COUNTS(name, sample) ASSET_HISTOGRAM_COUNTS(name, \
                                                                     sample)
#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) HISTOGRAM_PERCENTAGE(\
    name, under_one_hundred)
#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count)

#else  // NDEBUG

#define DHISTOGRAM_TIMES(name, sample) do {} while (0)
#define DHISTOGRAM_COUNTS(name, sample) do {} while (0)
#define DASSET_HISTOGRAM_COUNTS(name, sample) do {} while (0)
#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) do {} while (0)
#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    do {} while (0)

#endif  // NDEBUG

//------------------------------------------------------------------------------
// The following macros provide typical usage scenarios for callers that wish
// to record histogram data, and have the data submitted/uploaded via UMA.
// Not all systems support such UMA, but if they do, the following macros
// should work with the service.

static const int kUmaTargetedHistogramFlag = 0x1;

// This indicates the histogram is shadow copy of renderer histrogram
// constructed by unpick method and updated regularly from renderer upload
// of histograms.
static const int kRendererHistogramFlag = 1 << 4;

#define UMA_HISTOGRAM_TIMES(name, sample) do { \
    static Histogram counter((name), base::TimeDelta::FromMilliseconds(1), \
                             base::TimeDelta::FromSeconds(10), 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.AddTime(sample); \
  } while (0)

#define UMA_HISTOGRAM_MEDIUM_TIMES(name, sample) do { \
    static Histogram counter((name), base::TimeDelta::FromMilliseconds(10), \
                             base::TimeDelta::FromMinutes(3), 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.AddTime(sample); \
  } while (0)

// Use this macro when times can routinely be much longer than 10 seconds.
#define UMA_HISTOGRAM_LONG_TIMES(name, sample) do { \
    static Histogram counter((name), base::TimeDelta::FromMilliseconds(1), \
                             base::TimeDelta::FromHours(1), 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.AddTime(sample); \
  } while (0)

#define UMA_HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) do { \
    static Histogram counter((name), min, max, bucket_count); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    if ((sample) < (max)) counter.AddTime(sample); \
  } while (0)

#define UMA_HISTOGRAM_COUNTS(name, sample) do { \
    static Histogram counter((name), 1, 1000000, 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.Add(sample); \
  } while (0)

#define UMA_HISTOGRAM_COUNTS_100(name, sample) do { \
    static Histogram counter((name), 1, 100, 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.Add(sample); \
  } while (0)

#define UMA_HISTOGRAM_COUNTS_10000(name, sample) do { \
    static Histogram counter((name), 1, 10000, 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.Add(sample); \
  } while (0)

#define UMA_HISTOGRAM_MEMORY_KB(name, sample) do { \
    static Histogram counter((name), 1000, 500000, 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.Add(sample); \
  } while (0)

#define UMA_HISTOGRAM_MEMORY_MB(name, sample) do { \
    static Histogram counter((name), 1, 1000, 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.Add(sample); \
  } while (0)

#define UMA_HISTOGRAM_PERCENTAGE(name, under_one_hundred) do { \
    static LinearHistogram counter((name), 1, 100, 101); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.Add(under_one_hundred); \
  } while (0)

//------------------------------------------------------------------------------

class Pickle;

class Histogram {
 public:
  typedef int Sample;  // Used for samples (and ranges of samples).
  typedef int Count;  // Used to count samples in a bucket.
  static const Sample kSampleType_MAX = INT_MAX;

  typedef std::vector<Count> Counts;
  typedef std::vector<Sample> Ranges;

  static const int kHexRangePrintingFlag;

  enum BucketLayout {
    EXPONENTIAL,
    LINEAR
  };

  //----------------------------------------------------------------------------
  // Statistic values, developed over the life of the histogram.

  class SampleSet {
   public:
    explicit SampleSet();
    // Adjust size of counts_ for use with given histogram.
    void Resize(const Histogram& histogram);
    void CheckSize(const Histogram& histogram) const;

    // Accessor for histogram to make routine additions.
    void Accumulate(Sample value, Count count, size_t index);

    // Accessor methods.
    Count counts(size_t i) const { return counts_[i]; }
    Count TotalCount() const;
    int64 sum() const { return sum_; }
    int64 square_sum() const { return square_sum_; }

    // Arithmetic manipulation of corresponding elements of the set.
    void Add(const SampleSet& other);
    void Subtract(const SampleSet& other);

    bool Serialize(Pickle* pickle) const;
    bool Deserialize(void** iter, const Pickle& pickle);

   protected:
    // Actual histogram data is stored in buckets, showing the count of values
    // that fit into each bucket.
    Counts counts_;

    // Save simple stats locally.  Note that this MIGHT get done in base class
    // without shared memory at some point.
    int64 sum_;         // sum of samples.
    int64 square_sum_;  // sum of squares of samples.
  };
  //----------------------------------------------------------------------------

  Histogram(const char* name, Sample minimum,
            Sample maximum, size_t bucket_count);
  Histogram(const char* name, base::TimeDelta minimum,
            base::TimeDelta maximum, size_t bucket_count);
  virtual ~Histogram();

  void Add(int value);
  // Accept a TimeDelta to increment.
  void AddTime(base::TimeDelta time) {
    Add(static_cast<int>(time.InMilliseconds()));
  }

  void AddSampleSet(const SampleSet& sample);

  // The following methods provide graphical histogram displays.
  void WriteHTMLGraph(std::string* output) const;
  void WriteAscii(bool graph_it, const std::string& newline,
                  std::string* output) const;

  // Support generic flagging of Histograms.
  // 0x1 Currently used to mark this histogram to be recorded by UMA..
  // 0x8000 means print ranges in hex.
  void SetFlags(int flags) { flags_ |= flags; }
  void ClearFlags(int flags) { flags_ &= ~flags; }
  int flags() const { return flags_; }

  virtual BucketLayout histogram_type() const { return EXPONENTIAL; }

  // Convenience methods for serializing/deserializing the histograms.
  // Histograms from Renderer process are serialized and sent to the browser.
  // Browser process reconstructs the histogram from the pickled version
  // accumulates the browser-side shadow copy of histograms (that mirror
  // histograms created in the renderer).

  // Serialize the given snapshot of a Histogram into a String. Uses
  // Pickle class to flatten the object.
  static std::string SerializeHistogramInfo(const Histogram& histogram,
                                            const SampleSet& snapshot);
  // The following method accepts a list of pickled histograms and
  // builds a histogram and updates shadow copy of histogram data in the
  // browser process.
  static void DeserializeHistogramList(
      const std::vector<std::string>& histograms);
  static bool DeserializeHistogramInfo(const std::string& state);


  //----------------------------------------------------------------------------
  // Accessors for serialization and testing.
  //----------------------------------------------------------------------------
  const std::string histogram_name() const { return histogram_name_; }
  Sample declared_min() const { return declared_min_; }
  Sample declared_max() const { return declared_max_; }
  virtual Sample ranges(size_t i) const { return ranges_[i];}
  virtual size_t bucket_count() const { return bucket_count_; }
  // Snapshot the current complete set of sample data.
  // Override with atomic/locked snapshot if needed.
  virtual void SnapshotSample(SampleSet* sample) const;

 protected:
  // Method to override to skip the display of the i'th bucket if it's empty.
  virtual bool PrintEmptyBucket(size_t index) const { return true; }

  //----------------------------------------------------------------------------
  // Methods to override to create histogram with different bucket widths.
  //----------------------------------------------------------------------------
  // Initialize ranges_ mapping.
  virtual void InitializeBucketRange();
  // Find bucket to increment for sample value.
  virtual size_t BucketIndex(Sample value) const;
  // Get normalized size, relative to the ranges_[i].
  virtual double GetBucketSize(Count current, size_t i) const;

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

 private:
  // Post constructor initialization.
  void Initialize();

  //----------------------------------------------------------------------------
  // Helpers for emitting Ascii graphic.  Each method appends data to output.

  // Find out how large the (graphically) the largest bucket will appear to be.
  double GetPeakBucketSize(const SampleSet& snapshot) const;

  // Write a common header message describing this histogram.
  void WriteAsciiHeader(const SampleSet& snapshot,
                        Count sample_count, std::string* output) const;

  // Write information about previous, current, and next buckets.
  // Information such as cumulative percentage, etc.
  void WriteAsciiBucketContext(const int64 past, const Count current,
                               const int64 remaining, const size_t i,
                               std::string* output) const;

  // Write textual description of the bucket contents (relative to histogram).
  // Output is the count in the buckets, as well as the percentage.
  void WriteAsciiBucketValue(Count current, double scaled_sum,
                             std::string* output) const;

  // Produce actual graph (set of blank vs non blank char's) for a bucket.
  void WriteAsciiBucketGraph(double current_size, double max_size,
                             std::string* output) const;

  //----------------------------------------------------------------------------
  // Invariant values set at/near construction time

  // ASCII version of original name given to the constructor.  All identically
  // named instances will  be coalesced cross-project TODO(jar).
  // If a user needs one histogram name to be called by several places in a
  // single process, a central function should be defined by teh user, which
  // defins the single declared instance of the named histogram.
  const std::string histogram_name_;
  Sample declared_min_;  // Less than this goes into counts_[0]
  Sample declared_max_;  // Over this goes into counts_[bucket_count_ - 1].
  size_t bucket_count_;  // Dimension of counts_[].

  // Flag the histogram for recording by UMA via metric_services.h.
  int flags_;

  // For each index, show the least value that can be stored in the
  // corresponding bucket. We also append one extra element in this array,
  // containing kSampleType_MAX, to make calculations easy.
  // The dimension of ranges_ is bucket_count + 1.
  Ranges ranges_;

  // Finally, provide the state that changes with the addition of each new
  // sample.
  SampleSet sample_;

  // Indicate if successfully registered.
  bool registered_;

  DISALLOW_COPY_AND_ASSIGN(Histogram);
};

//------------------------------------------------------------------------------

// LinearHistogram is a more traditional histogram, with evenly spaced
// buckets.
class LinearHistogram : public Histogram {
 public:
  struct DescriptionPair {
    Sample sample;
    const char* description;  // Null means end of a list of pairs.
  };
  LinearHistogram(const char* name, Sample minimum,
                  Sample maximum, size_t bucket_count);

  LinearHistogram(const char* name, base::TimeDelta minimum,
                  base::TimeDelta maximum, size_t bucket_count);
  ~LinearHistogram() {}

  // Store a list of number/text values for use in rendering the histogram.
  // The last element in the array has a null in its "description" slot.
  void SetRangeDescriptions(const DescriptionPair descriptions[]);

  virtual BucketLayout histogram_type() const { return LINEAR; }

 protected:
  // Initialize ranges_ mapping.
  virtual void InitializeBucketRange();
  // Find bucket to increment for sample value.
  virtual size_t BucketIndex(Sample value) const;
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
  explicit BooleanHistogram(const char* name)
    : LinearHistogram(name, 0, 2, 3) {
  }

  void AddBoolean(bool value) { Add(value ? 1 : 0); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BooleanHistogram);
};

//------------------------------------------------------------------------------
// This section provides implementation for ThreadSafeHistogram.
//------------------------------------------------------------------------------

class ThreadSafeHistogram : public Histogram {
 public:
  ThreadSafeHistogram(const char* name, Sample minimum,
                      Sample maximum, size_t bucket_count);

  // Provide the analog to Add()
  void Remove(int value);

 protected:
  // Provide locked versions to get precise counts.
  virtual void Accumulate(Sample value, Count count, size_t index);

  virtual void SnapshotSample(SampleSet* sample) const;

 private:
  mutable Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeHistogram);
};

//------------------------------------------------------------------------------
// StatisticsRecorder handles all histograms in the system.  It provides a
// general place for histograms to register, and supports a global API for
// accessing (i.e., dumping, or graphing) the data in all the histograms.

class StatisticsRecorder {
 public:
  typedef std::vector<Histogram*> Histograms;

  StatisticsRecorder();

  ~StatisticsRecorder();

  // Find out if histograms can now be registered into our list.
  static bool WasStarted();

  // Register, or add a new histogram to the collection of statistics.
  // Return true if registered.
  static bool Register(Histogram* histogram);
  // Unregister, or remove, a histogram from the collection of statistics.
  static void UnRegister(Histogram* histogram);

  // Methods for printing histograms.  Only histograms which have query as
  // a substring are written to output (an empty string will process all
  // registered histograms).
  static void WriteHTMLGraph(const std::string& query, std::string* output);
  static void WriteGraph(const std::string& query, std::string* output);

  // Method for extracting histograms which were marked for use by UMA.
  static void GetHistograms(Histograms* output);

  // Find a histogram by name.  This method is thread safe.
  static Histogram* GetHistogram(const std::string& query);

  static void set_dump_on_exit(bool enable) { dump_on_exit_ = enable; }

  // GetSnapshot copies some of the pointers to registered histograms into the
  // caller supplied vector (Histograms).  Only histograms with names matching
  // query are returned. The query must be a substring of histogram name for its
  // pointer to be copied.
  static void GetSnapshot(const std::string& query, Histograms* snapshot);


 private:
  // We keep all registered histograms in a map, from name to histogram.
  typedef std::map<std::string, Histogram*> HistogramMap;

  static HistogramMap* histograms_;

  // lock protects access to the above map.
  static Lock* lock_;

  // Dump all known histograms to log.
  static bool dump_on_exit_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsRecorder);
};

#endif  // BASE_HISTOGRAM_H_
