// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACKED_OBJECTS_H_
#define BASE_TRACKED_OBJECTS_H_

//------------------------------------------------------------------------------
#include <map>
#include <string>
#include <vector>

#include "base/lock.h"
#include "mozilla/StaticMutex.h"
#include "base/message_loop.h"
#include "base/thread_local_storage.h"
#include "base/tracked.h"


namespace tracked_objects {

//------------------------------------------------------------------------------
// For a specific thread, and a specific birth place, the collection of all
// death info (with tallies for each death thread, to prevent access conflicts).
class ThreadData;
class BirthOnThread {
 public:
  explicit BirthOnThread(const Location& location);

  const Location location() const { return location_; }
  const ThreadData* birth_thread() const { return birth_thread_; }

 private:
  // File/lineno of birth.  This defines the essence of the type, as the context
  // of the birth (construction) often tell what the item is for.  This field
  // is const, and hence safe to access from any thread.
  const Location location_;

  // The thread that records births into this object.  Only this thread is
  // allowed to access birth_count_ (which changes over time).
  const ThreadData* birth_thread_;  // The thread this birth took place on.

  DISALLOW_COPY_AND_ASSIGN(BirthOnThread);
};

//------------------------------------------------------------------------------
// A class for accumulating counts of births (without bothering with a map<>).

class Births: public BirthOnThread {
 public:
  explicit Births(const Location& location);

  int birth_count() const { return birth_count_; }

  // When we have a birth we update the count for this BirhPLace.
  void RecordBirth() { ++birth_count_; }

  // When a birthplace is changed (updated), we need to decrement the counter
  // for the old instance.
  void ForgetBirth() { --birth_count_; }  // We corrected a birth place.

 private:
  // The number of births on this thread for our location_.
  int birth_count_;

  DISALLOW_COPY_AND_ASSIGN(Births);
};

//------------------------------------------------------------------------------
// Basic info summarizing multiple destructions of an object with a single
// birthplace (fixed Location).  Used both on specific threads, and also used
// in snapshots when integrating assembled data.

class DeathData {
 public:
  // Default initializer.
  DeathData() : count_(0), square_duration_(0) {}

  // When deaths have not yet taken place, and we gather data from all the
  // threads, we create DeathData stats that tally the number of births without
  // a corrosponding death.
  explicit DeathData(int count) : count_(count), square_duration_(0) {}

  void RecordDeath(const base::TimeDelta& duration);

  // Metrics accessors.
  int count() const { return count_; }
  base::TimeDelta life_duration() const { return life_duration_; }
  int64_t square_duration() const { return square_duration_; }
  int AverageMsDuration() const;
  double StandardDeviation() const;

  // Accumulate metrics from other into this.
  void AddDeathData(const DeathData& other);

  // Simple print of internal state.
  void Write(std::string* output) const;

  void Clear();

 private:
  int count_;                // Number of destructions.
  base::TimeDelta life_duration_;    // Sum of all lifetime durations.
  int64_t square_duration_;  // Sum of squares in milliseconds.
};

//------------------------------------------------------------------------------
// A temporary collection of data that can be sorted and summarized.  It is
// gathered (carefully) from many threads.  Instances are held in arrays and
// processed, filtered, and rendered.
// The source of this data was collected on many threads, and is asynchronously
// changing.  The data in this instance is not asynchronously changing.

class Snapshot {
 public:
  // When snapshotting a full life cycle set (birth-to-death), use this:
  Snapshot(const BirthOnThread& birth_on_thread, const ThreadData& death_thread,
           const DeathData& death_data);

  // When snapshotting a birth, with no death yet, use this:
  Snapshot(const BirthOnThread& birth_on_thread, int count);


  const ThreadData* birth_thread() const { return birth_->birth_thread(); }
  const Location location() const { return birth_->location(); }
  const BirthOnThread& birth() const { return *birth_; }
  const ThreadData* death_thread() const {return death_thread_; }
  const DeathData& death_data() const { return death_data_; }
  const std::string DeathThreadName() const;

  int count() const { return death_data_.count(); }
  base::TimeDelta life_duration() const { return death_data_.life_duration(); }
  int64_t square_duration() const { return death_data_.square_duration(); }
  int AverageMsDuration() const { return death_data_.AverageMsDuration(); }

  void Write(std::string* output) const;

  void Add(const Snapshot& other);

 private:
  const BirthOnThread* birth_;  // Includes Location and birth_thread.
  const ThreadData* death_thread_;
  DeathData death_data_;
};
//------------------------------------------------------------------------------
// DataCollector is a container class for Snapshot and BirthOnThread count
// items.  It protects the gathering under locks, so that it could be called via
// Posttask on any threads, such as all the target threads in parallel.

class DataCollector {
 public:
  typedef std::vector<Snapshot> Collection;

  // Construct with a list of how many threads should contribute.  This helps us
  // determine (in the async case) when we are done with all contributions.
  DataCollector();

  // Add all stats from the indicated thread into our arrays.  This function is
  // mutex protected, and *could* be called from any threads (although current
  // implementation serialized calls to Append).
  void Append(const ThreadData& thread_data);

  // After the accumulation phase, the following access is to process data.
  Collection* collection();

  // After collection of death data is complete, we can add entries for all the
  // remaining living objects.
  void AddListOfLivingObjects();

 private:
  // This instance may be provided to several threads to contribute data.  The
  // following counter tracks how many more threads will contribute.  When it is
  // zero, then all asynchronous contributions are complete, and locked access
  // is no longer needed.
  int count_of_contributing_threads_;

  // The array that we collect data into.
  Collection collection_;

  // The total number of births recorded at each location for which we have not
  // seen a death count.
  typedef std::map<const BirthOnThread*, int> BirthCount;
  BirthCount global_birth_count_;

  Lock accumulation_lock_;  // Protects access during accumulation phase.

  DISALLOW_COPY_AND_ASSIGN(DataCollector);
};

//------------------------------------------------------------------------------
// Aggregation contains summaries (totals and subtotals) of groups of Snapshot
// instances to provide printing of these collections on a single line.

class Aggregation: public DeathData {
 public:
  Aggregation() : birth_count_(0) {}

  void AddDeathSnapshot(const Snapshot& snapshot);
  void AddBirths(const Births& births);
  void AddBirth(const BirthOnThread& birth);
  void AddBirthPlace(const Location& location);
  void Write(std::string* output) const;
  void Clear();

 private:
  int birth_count_;
  std::map<std::string, int> birth_files_;
  std::map<Location, int> locations_;
  std::map<const ThreadData*, int> birth_threads_;
  DeathData death_data_;
  std::map<const ThreadData*, int> death_threads_;

  DISALLOW_COPY_AND_ASSIGN(Aggregation);
};

//------------------------------------------------------------------------------
// Comparator does the comparison of Snapshot instances.  It is
// used to order the instances in a vector.  It orders them into groups (for
// aggregation), and can also order instances within the groups (for detailed
// rendering of the instances).

class Comparator {
 public:
  enum Selector {
    NIL = 0,
    BIRTH_THREAD = 1,
    DEATH_THREAD = 2,
    BIRTH_FILE = 4,
    BIRTH_FUNCTION = 8,
    BIRTH_LINE = 16,
    COUNT = 32,
    AVERAGE_DURATION = 64,
    TOTAL_DURATION = 128
  };

  explicit Comparator();

  // Reset the comparator to a NIL selector.  Reset() and recursively delete any
  // tiebreaker_ entries.  NOTE: We can't use a standard destructor, because
  // the sort algorithm makes copies of this object, and then deletes them,
  // which would cause problems (either we'd make expensive deep copies, or we'd
  // do more thna one delete on a tiebreaker_.
  void Clear();

  // The less() operator for sorting the array via std::sort().
  bool operator()(const Snapshot& left, const Snapshot& right) const;

  void Sort(DataCollector::Collection* collection) const;

  // Check to see if the items are sort equivalents (should be aggregated).
  bool Equivalent(const Snapshot& left, const Snapshot& right) const;

  // Check to see if all required fields are present in the given sample.
  bool Acceptable(const Snapshot& sample) const;

  // A comparator can be refined by specifying what to do if the selected basis
  // for comparison is insufficient to establish an ordering.  This call adds
  // the indicated attribute as the new "least significant" basis of comparison.
  void SetTiebreaker(Selector selector, const std::string required);

  // Indicate if this instance is set up to sort by the given Selector, thereby
  // putting that information in the SortGrouping, so it is not needed in each
  // printed line.
  bool IsGroupedBy(Selector selector) const;

  // Using the tiebreakers as set above, we mostly get an ordering, which
  // equivalent groups.  If those groups are displayed (rather than just being
  // aggregated, then the following is used to order them (within the group).
  void SetSubgroupTiebreaker(Selector selector);

  // Output a header line that can be used to indicated what items will be
  // collected in the group.  It lists all (potentially) tested attributes and
  // their values (in the sample item).
  bool WriteSortGrouping(const Snapshot& sample, std::string* output) const;

  // Output a sample, with SortGroup details not displayed.
  void WriteSnapshot(const Snapshot& sample, std::string* output) const;

 private:
  // The selector directs this instance to compare based on the specified
  // members of the tested elements.
  enum Selector selector_;

  // For filtering into acceptable and unacceptable snapshot instance, the
  // following is required to be a substring of the selector_ field.
  std::string required_;

  // If this instance can't decide on an ordering, we can consult a tie-breaker
  // which may have a different basis of comparison.
  Comparator* tiebreaker_;

  // We or together all the selectors we sort on (not counting sub-group
  // selectors), so that we can tell if we've decided to group on any given
  // criteria.
  int combined_selectors_;

  // Some tiebreakrs are for subgroup ordering, and not for basic ordering (in
  // preparation for aggregation).  The subgroup tiebreakers are not consulted
  // when deciding if two items are in equivalent groups.  This flag tells us
  // to ignore the tiebreaker when doing Equivalent() testing.
  bool use_tiebreaker_for_sort_only_;
};


//------------------------------------------------------------------------------
// For each thread, we have a ThreadData that stores all tracking info generated
// on this thread.  This prevents the need for locking as data accumulates.

class ThreadData {
 public:
  typedef std::map<Location, Births*> BirthMap;
  typedef std::map<const Births*, DeathData> DeathMap;

  ThreadData();

  // Using Thread Local Store, find the current instance for collecting data.
  // If an instance does not exist, construct one (and remember it for use on
  // this thread.
  // If shutdown has already started, and we don't yet have an instance, then
  // return null.
  static ThreadData* current();

  // In this thread's data, find a place to record a new birth.
  Births* FindLifetime(const Location& location);

  // Find a place to record a death on this thread.
  void TallyADeath(const Births& lifetimes, const base::TimeDelta& duration);

  // (Thread safe) Get start of list of instances.
  static ThreadData* first();
  // Iterate through the null terminated list of instances.
  ThreadData* next() const { return next_; }

  MessageLoop* message_loop() const { return message_loop_; }
  const std::string ThreadName() const;

  // Using our lock, make a copy of the specified maps.  These calls may arrive
  // from non-local threads.
  void SnapshotBirthMap(BirthMap *output) const;
  void SnapshotDeathMap(DeathMap *output) const;

  static void RunOnAllThreads(void (*Func)());

  // Set internal status_ to either become ACTIVE, or later, to be SHUTDOWN,
  // based on argument being true or false respectively.
  // IF tracking is not compiled in, this function will return false.
  static bool StartTracking(bool status);
  static bool IsActive();

#ifdef OS_WIN
  // WARNING: ONLY call this function when all MessageLoops are still intact for
  // all registered threads.  IF you call it later, you will crash.
  // Note: You don't need to call it at all, and you can wait till you are
  // single threaded (again) to do the cleanup via
  // ShutdownSingleThreadedCleanup().
  // Start the teardown (shutdown) process in a multi-thread mode by disabling
  // further additions to thread database on all threads.  First it makes a
  // local (locked) change to prevent any more threads from registering.  Then
  // it Posts a Task to all registered threads to be sure they are aware that no
  // more accumulation can take place.
  static void ShutdownMultiThreadTracking();
#endif

  // WARNING: ONLY call this function when you are running single threaded
  // (again) and all message loops and threads have terminated.  Until that
  // point some threads may still attempt to write into our data structures.
  // Delete recursively all data structures, starting with the list of
  // ThreadData instances.
  static void ShutdownSingleThreadedCleanup();

 private:
  // Current allowable states of the tracking system.  The states always
  // proceed towards SHUTDOWN, and never go backwards.
  enum Status {
    UNINITIALIZED,
    ACTIVE,
    SHUTDOWN
  };

  // A class used to count down which is accessed by several threads.  This is
  // used to make sure RunOnAllThreads() actually runs a task on the expected
  // count of threads.
  class ThreadSafeDownCounter {
   public:
    // Constructor sets the count, once and for all.
    explicit ThreadSafeDownCounter(size_t count);

    // Decrement the count, and return true if we hit zero.  Also delete this
    // instance automatically when we hit zero.
    bool LastCaller();

   private:
    size_t remaining_count_;
    Lock lock_;  // protect access to remaining_count_.
  };

#ifdef OS_WIN
  // A Task class that runs a static method supplied, and checks to see if this
  // is the last tasks instance (on last thread) that will run the method.
  // IF this is the last run, then the supplied event is signalled.
  class RunTheStatic : public Task {
   public:
    typedef void (*FunctionPointer)();
    RunTheStatic(FunctionPointer function,
                 HANDLE completion_handle,
                 ThreadSafeDownCounter* counter);
    // Run the supplied static method, and optionally set the event.
    void Run();

   private:
    FunctionPointer function_;
    HANDLE completion_handle_;
    // Make sure enough tasks are called before completion is signaled.
    ThreadSafeDownCounter* counter_;

    DISALLOW_COPY_AND_ASSIGN(RunTheStatic);
  };
#endif

  // Each registered thread is called to set status_ to SHUTDOWN.
  // This is done redundantly on every registered thread because it is not
  // protected by a mutex.  Running on all threads guarantees we get the
  // notification into the memory cache of all possible threads.
  static void ShutdownDisablingFurtherTracking();

  // We use thread local store to identify which ThreadData to interact with.
  static TLSSlot tls_index_ ;

  // Link to the most recently created instance (starts a null terminated list).
  static ThreadData* first_;
  // Protection for access to first_.
  static mozilla::StaticMutex list_lock_;


  // We set status_ to SHUTDOWN when we shut down the tracking service. This
  // setting is redundantly established by all participating
  // threads so that we are *guaranteed* (without locking) that all threads
  // can "see" the status and avoid additional calls into the  service.
  static Status status_;

  // Link to next instance (null terminated list). Used to globally track all
  // registered instances (corresponds to all registered threads where we keep
  // data).
  ThreadData* next_;

  // The message loop where tasks needing to access this instance's private data
  // should be directed.  Since some threads have no message loop, some
  // instances have data that can't be (safely) modified externally.
  MessageLoop* message_loop_;

  // A map used on each thread to keep track of Births on this thread.
  // This map should only be accessed on the thread it was constructed on.
  // When a snapshot is needed, this structure can be locked in place for the
  // duration of the snapshotting activity.
  BirthMap birth_map_;

  // Similar to birth_map_, this records informations about death of tracked
  // instances (i.e., when a tracked instance was destroyed on this thread).
  DeathMap death_map_;

  // Lock to protect *some* access to BirthMap and DeathMap.  We only use
  // locking protection when we are growing the maps, or using an iterator.  We
  // only do writes to members from this thread, so the updates of values are
  // atomic.  Folks can read from other threads, and get (via races) new or old
  // data, but that is considered acceptable errors (mis-information).
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ThreadData);
};


//------------------------------------------------------------------------------
// Provide simple way to to start global tracking, and to tear down tracking
// when done.  Note that construction and destruction of this object must be
// done when running in single threaded mode (before spawning a lot of threads
// for construction, and after shutting down all the threads for destruction).

class AutoTracking {
 public:
  AutoTracking() { ThreadData::StartTracking(true); }

  ~AutoTracking() {
#ifndef NDEBUG  // Don't call these in a Release build: they just waste time.
    // The following should ONLY be called when in single threaded mode. It is
    // unsafe to do this cleanup if other threads are still active.
    // It is also very unnecessary, so I'm only doing this in debug to satisfy
    // purify (if we need to!).
    ThreadData::ShutdownSingleThreadedCleanup();
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoTracking);
};


}  // namespace tracked_objects

#endif  // BASE_TRACKED_OBJECTS_H_
