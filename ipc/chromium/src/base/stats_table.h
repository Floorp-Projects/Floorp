// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A StatsTable is a table of statistics.  It can be used across multiple
// processes and threads, maintaining cheap statistics counters without
// locking.
//
// The goal is to make it very cheap and easy for developers to add
// counters to code, without having to build one-off utilities or mechanisms
// to track the counters, and also to allow a single "view" to display
// the contents of all counters.
//
// To achieve this, StatsTable creates a shared memory segment to store
// the data for the counters.  Upon creation, it has a specific size
// which governs the maximum number of counters and concurrent
// threads/processes which can use it.
//

#ifndef BASE_STATS_TABLE_H__
#define BASE_STATS_TABLE_H__

#include <string>
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/thread_local_storage.h"

class StatsTablePrivate;

namespace {
struct StatsTableTLSData;
}

class StatsTable {
 public:
  // Create a new StatsTable.
  // If a StatsTable already exists with the specified name, this StatsTable
  // will use the same shared memory segment as the original.  Otherwise,
  // a new StatsTable is created and all counters are zeroed.
  //
  // name is the name of the StatsTable to use.
  //
  // max_threads is the maximum number of threads the table will support.
  // If the StatsTable already exists, this number is ignored.
  //
  // max_counters is the maximum number of counters the table will support.
  // If the StatsTable already exists, this number is ignored.
  StatsTable(const std::string& name, int max_threads, int max_counters);

  // Destroys the StatsTable.  When the last StatsTable is destroyed
  // (across all processes), the StatsTable is removed from disk.
  ~StatsTable();

  // For convenience, we create a static table.  This is generally
  // used automatically by the counters.
  static StatsTable* current() { return global_table_; }

  // Set the global table for use in this process.
  static void set_current(StatsTable* value) { global_table_ = value; }

  // Get the slot id for the calling thread. Returns 0 if no
  // slot is assigned.
  int GetSlot() const;

  // All threads that contribute data to the table must register with the
  // table first.  This function will set thread local storage for the
  // thread containing the location in the table where this thread will
  // write its counter data.
  //
  // name is just a debugging tag to label the thread, and it does not
  // need to be unique.  It will be truncated to kMaxThreadNameLength-1
  // characters.
  //
  // On success, returns the slot id for this thread.  On failure,
  // returns 0.
  int RegisterThread(const std::string& name);

  // Returns the number of threads currently registered.  This is really not
  // useful except for diagnostics and debugging.
  int CountThreadsRegistered() const;

  // Find a counter in the StatsTable.
  //
  // Returns an id for the counter which can be used to call GetLocation().
  // If the counter does not exist, attempts to create a row for the new
  // counter.  If there is no space in the table for the new counter,
  // returns 0.
  int FindCounter(const std::string& name);

  // TODO(mbelshe): implement RemoveCounter.

  // Gets the location of a particular value in the table based on
  // the counter id and slot id.
  int* GetLocation(int counter_id, int slot_id) const;

  // Gets the counter name at a particular row.  If the row is empty,
  // returns NULL.
  const char* GetRowName(int index) const;

  // Gets the sum of the values for a particular row.
  int GetRowValue(int index) const;

  // Gets the sum of the values for a particular row for a given pid.
  int GetRowValue(int index, int pid) const;

  // Gets the sum of the values for a particular counter.  If the counter
  // does not exist, creates the counter.
  int GetCounterValue(const std::string& name);

  // Gets the sum of the values for a particular counter for a given pid.
  // If the counter does not exist, creates the counter.
  int GetCounterValue(const std::string& name, int pid);

  // The maxinum number of counters/rows in the table.
  int GetMaxCounters() const;

  // The maxinum number of threads/columns in the table.
  int GetMaxThreads() const;

  // The maximum length (in characters) of a Thread's name including
  // null terminator, as stored in the shared memory.
  static const int kMaxThreadNameLength = 32;

  // The maximum length (in characters) of a Counter's name including
  // null terminator, as stored in the shared memory.
  static const int kMaxCounterNameLength = 32;

  // Convenience function to lookup a counter location for a
  // counter by name for the calling thread.  Will register
  // the thread if it is not already registered.
  static int* FindLocation(const char *name);

 private:
  // Returns the space occupied by a thread in the table.  Generally used
  // if a thread terminates but the process continues.  This function
  // does not zero out the thread's counters.
  // Cannot be used inside a posix tls destructor.
  void UnregisterThread();

  // This variant expects the tls data to be passed in, so it is safe to
  // call from inside a posix tls destructor (see doc for pthread_key_create).
  void UnregisterThread(StatsTableTLSData* tls_data);

  // The SlotReturnFunction is called at thread exit for each thread
  // which used the StatsTable.
  static void SlotReturnFunction(void* data);

  // Locates a free slot in the table.  Returns a number > 0 on success,
  // or 0 on failure.  The caller must hold the shared_memory lock when
  // calling this function.
  int FindEmptyThread() const;

  // Locates a counter in the table or finds an empty row.  Returns a
  // number > 0 on success, or 0 on failure.  The caller must hold the
  // shared_memory_lock when calling this function.
  int FindCounterOrEmptyRow(const std::string& name) const;

  // Internal function to add a counter to the StatsTable.  Assumes that
  // the counter does not already exist in the table.
  //
  // name is a unique identifier for this counter, and will be truncated
  // to kMaxCounterNameLength-1 characters.
  //
  // On success, returns the counter_id for the newly added counter.
  // On failure, returns 0.
  int AddCounter(const std::string& name);

  // Get the TLS data for the calling thread.  Returns NULL if none is
  // initialized.
  StatsTableTLSData* GetTLSData() const;

  typedef base::hash_map<std::string, int> CountersMap;

  StatsTablePrivate* impl_;
  // The counters_lock_ protects the counters_ hash table.
  Lock counters_lock_;
  // The counters_ hash map is an in-memory hash of the counters.
  // It is used for quick lookup of counters, but is cannot be used
  // as a substitute for what is in the shared memory.  Even though
  // we don't have a counter in our hash table, another process may
  // have created it.
  CountersMap counters_;
  TLSSlot tls_index_;

  static StatsTable* global_table_;

  DISALLOW_EVIL_CONSTRUCTORS(StatsTable);
};

#endif  // BASE_STATS_TABLE_H__
