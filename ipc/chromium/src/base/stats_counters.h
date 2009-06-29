// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BASE_STATS_COUNTERS_H__
#define BASE_STATS_COUNTERS_H__

#include <string>
#include "base/stats_table.h"
#include "base/time.h"

// StatsCounters are dynamically created values which can be tracked in
// the StatsTable.  They are designed to be lightweight to create and
// easy to use.
//
// Since StatsCounters can be created dynamically by name, there is
// a hash table lookup to find the counter in the table.  A StatsCounter
// object can be created once and used across multiple threads safely.
//
// Example usage:
//    {
//      StatsCounter request_count("RequestCount");
//      request_count.Increment();
//    }
//
// Note that creating counters on the stack does work, however creating
// the counter object requires a hash table lookup.  For inner loops, it
// may be better to create the counter either as a member of another object
// (or otherwise outside of the loop) for maximum performance.
//
// Internally, a counter represents a value in a row of a StatsTable.
// The row has a 32bit value for each process/thread in the table and also
// a name (stored in the table metadata).
//
// NOTE: In order to make stats_counters usable in lots of different code,
// avoid any dependencies inside this header file.
//

//------------------------------------------------------------------------------
// Define macros for ease of use. They also allow us to change definitions
// as the implementation varies, or depending on compile options.
//------------------------------------------------------------------------------
// First provide generic macros, which exist in production as well as debug.
#define STATS_COUNTER(name, delta) do { \
  static StatsCounter counter(name); \
  counter.Add(delta); \
} while (0)

#define SIMPLE_STATS_COUNTER(name) STATS_COUNTER(name, 1)

#define RATE_COUNTER(name, duration) do { \
  static StatsRate hit_count(name); \
  hit_count.AddTime(duration); \
} while (0)

// Define Debug vs non-debug flavors of macros.
#ifndef NDEBUG

#define DSTATS_COUNTER(name, delta) STATS_COUNTER(name, delta)
#define DSIMPLE_STATS_COUNTER(name) SIMPLE_STATS_COUNTER(name)
#define DRATE_COUNTER(name, duration) RATE_COUNTER(name, duration)

#else  // NDEBUG

#define DSTATS_COUNTER(name, delta) do {} while (0)
#define DSIMPLE_STATS_COUNTER(name) do {} while (0)
#define DRATE_COUNTER(name, duration) do {} while (0)

#endif  // NDEBUG

//------------------------------------------------------------------------------
// StatsCounter represents a counter in the StatsTable class.
class StatsCounter {
 public:
  // Create a StatsCounter object.
  explicit StatsCounter(const std::string& name)
       : counter_id_(-1) {
    // We prepend the name with 'c:' to indicate that it is a counter.
    name_ = "c:";
    name_.append(name);
  };

  virtual ~StatsCounter() {}

  // Sets the counter to a specific value.
  void Set(int value) {
    int* loc = GetPtr();
    if (loc) *loc = value;
  }

  // Increments the counter.
  void Increment() {
    Add(1);
  }

  virtual void Add(int value) {
    int* loc = GetPtr();
    if (loc)
      (*loc) += value;
  }

  // Decrements the counter.
  void Decrement() {
    Add(-1);
  }

  void Subtract(int value) {
    Add(-value);
  }

  // Is this counter enabled?
  // Returns false if table is full.
  bool Enabled() {
    return GetPtr() != NULL;
  }

  int value() {
    int* loc = GetPtr();
    if (loc) return *loc;
    return 0;
  }

 protected:
  StatsCounter()
    : counter_id_(-1) {
  }

  // Returns the cached address of this counter location.
  int* GetPtr() {
    StatsTable* table = StatsTable::current();
    if (!table)
      return NULL;

    // If counter_id_ is -1, then we haven't looked it up yet.
    if (counter_id_ == -1) {
      counter_id_ = table->FindCounter(name_);
      if (table->GetSlot() == 0) {
        if (!table->RegisterThread("")) {
          // There is no room for this thread.  This thread
          // cannot use counters.
          counter_id_ = 0;
          return NULL;
        }
      }
    }

    // If counter_id_ is > 0, then we have a valid counter.
    if (counter_id_ > 0)
      return table->GetLocation(counter_id_, table->GetSlot());

    // counter_id_ was zero, which means the table is full.
    return NULL;
  }

  std::string name_;
  // The counter id in the table.  We initialize to -1 (an invalid value)
  // and then cache it once it has been looked up.  The counter_id is
  // valid across all threads and processes.
  int32 counter_id_;
};


// A StatsCounterTimer is a StatsCounter which keeps a timer during
// the scope of the StatsCounterTimer.  On destruction, it will record
// its time measurement.
class StatsCounterTimer : protected StatsCounter {
 public:
  // Constructs and starts the timer.
  explicit StatsCounterTimer(const std::string& name) {
    // we prepend the name with 't:' to indicate that it is a timer.
    name_ = "t:";
    name_.append(name);
  }

  // Start the timer.
  void Start() {
    if (!Enabled())
      return;
    start_time_ = base::TimeTicks::Now();
    stop_time_ = base::TimeTicks();
  }

  // Stop the timer and record the results.
  void Stop() {
    if (!Enabled() || !Running())
      return;
    stop_time_ = base::TimeTicks::Now();
    Record();
  }

  // Returns true if the timer is running.
  bool Running() {
    return Enabled() && !start_time_.is_null() && stop_time_.is_null();
  }

  // Accept a TimeDelta to increment.
  virtual void AddTime(base::TimeDelta time) {
    Add(static_cast<int>(time.InMilliseconds()));
  }

 protected:
  // Compute the delta between start and stop, in milliseconds.
  void Record() {
    AddTime(stop_time_ - start_time_);
  }

  base::TimeTicks start_time_;
  base::TimeTicks stop_time_;
};

// A StatsRate is a timer that keeps a count of the number of intervals added so
// that several statistics can be produced:
//    min, max, avg, count, total
class StatsRate : public StatsCounterTimer {
 public:
  // Constructs and starts the timer.
  explicit StatsRate(const char* name)
      : StatsCounterTimer(name),
      counter_(name),
      largest_add_(std::string(" ").append(name).append("MAX").c_str()) {
  }

  virtual void Add(int value) {
    counter_.Increment();
    StatsCounterTimer::Add(value);
    if (value > largest_add_.value())
      largest_add_.Set(value);
  }

 private:
  StatsCounter counter_;
  StatsCounter largest_add_;
};


// Helper class for scoping a timer or rate.
template<class T> class StatsScope {
 public:
  explicit StatsScope<T>(T& timer)
      : timer_(timer) {
    timer_.Start();
  }

  ~StatsScope() {
    timer_.Stop();
  }

  void Stop() {
    timer_.Stop();
  }

 private:
  T& timer_;
};

#endif  // BASE_STATS_COUNTERS_H__
