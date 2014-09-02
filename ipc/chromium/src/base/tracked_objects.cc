// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracked_objects.h"

#include <math.h>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/string_util.h"

using base::TimeDelta;

namespace tracked_objects {

// A TLS slot to the TrackRegistry for the current thread.
// static
TLSSlot ThreadData::tls_index_(base::LINKER_INITIALIZED);

//------------------------------------------------------------------------------
// Death data tallies durations when a death takes place.

void DeathData::RecordDeath(const TimeDelta& duration) {
  ++count_;
  life_duration_ += duration;
  int64_t milliseconds = duration.InMilliseconds();
  square_duration_ += milliseconds * milliseconds;
}

int DeathData::AverageMsDuration() const {
  return static_cast<int>(life_duration_.InMilliseconds() / count_);
}

double DeathData::StandardDeviation() const {
  double average = AverageMsDuration();
  double variance = static_cast<float>(square_duration_)/count_
                    - average * average;
  return sqrt(variance);
}


void DeathData::AddDeathData(const DeathData& other) {
  count_ += other.count_;
  life_duration_ += other.life_duration_;
  square_duration_ += other.square_duration_;
}

void DeathData::Write(std::string* output) const {
  if (!count_)
    return;
  if (1 == count_)
    StringAppendF(output, "(1)Life in %dms ", AverageMsDuration());
  else
    StringAppendF(output, "(%d)Lives %dms/life ", count_, AverageMsDuration());
}

void DeathData::Clear() {
  count_ = 0;
  life_duration_ = TimeDelta();
  square_duration_ = 0;
}

//------------------------------------------------------------------------------

BirthOnThread::BirthOnThread(const Location& location)
    : location_(location),
      birth_thread_(ThreadData::current()) { }

//------------------------------------------------------------------------------
Births::Births(const Location& location)
    : BirthOnThread(location),
      birth_count_(0) { }

//------------------------------------------------------------------------------
// ThreadData maintains the central data for all births and death.

// static
ThreadData* ThreadData::first_ = NULL;
// static
Lock ThreadData::list_lock_;

// static
ThreadData::Status ThreadData::status_ = ThreadData::UNINITIALIZED;

ThreadData::ThreadData() : next_(NULL), message_loop_(MessageLoop::current()) {}

// static
ThreadData* ThreadData::current() {
  if (!tls_index_.initialized())
    return NULL;

  ThreadData* registry = static_cast<ThreadData*>(tls_index_.Get());
  if (!registry) {
    // We have to create a new registry for ThreadData.
    bool too_late_to_create = false;
    {
      registry = new ThreadData;
      AutoLock lock(list_lock_);
      // Use lock to insure we have most recent status.
      if (!IsActive()) {
        too_late_to_create = true;
      } else {
        // Use lock to insert into list.
        registry->next_ = first_;
        first_ = registry;
      }
    }  // Release lock.
    if (too_late_to_create) {
      delete registry;
      registry = NULL;
    } else {
      tls_index_.Set(registry);
    }
  }
  return registry;
}

Births* ThreadData::FindLifetime(const Location& location) {
  if (!message_loop_)  // In case message loop wasn't yet around...
    message_loop_ = MessageLoop::current();  // Find it now.

  BirthMap::iterator it = birth_map_.find(location);
  if (it != birth_map_.end())
    return it->second;
  Births* tracker = new Births(location);

  // Lock since the map may get relocated now, and other threads sometimes
  // snapshot it (but they lock before copying it).
  AutoLock lock(lock_);
  birth_map_[location] = tracker;
  return tracker;
}

void ThreadData::TallyADeath(const Births& lifetimes,
                             const TimeDelta& duration) {
  if (!message_loop_)  // In case message loop wasn't yet around...
    message_loop_ = MessageLoop::current();  // Find it now.

  DeathMap::iterator it = death_map_.find(&lifetimes);
  if (it != death_map_.end()) {
    it->second.RecordDeath(duration);
    return;
  }

  AutoLock lock(lock_);  // Lock since the map may get relocated now.
  death_map_[&lifetimes].RecordDeath(duration);
}

// static
ThreadData* ThreadData::first() {
  AutoLock lock(list_lock_);
  return first_;
}

const std::string ThreadData::ThreadName() const {
  if (message_loop_)
    return message_loop_->thread_name();
  return "ThreadWithoutMessageLoop";
}

// This may be called from another thread.
void ThreadData::SnapshotBirthMap(BirthMap *output) const {
  AutoLock lock(*const_cast<Lock*>(&lock_));
  for (BirthMap::const_iterator it = birth_map_.begin();
       it != birth_map_.end(); ++it)
    (*output)[it->first] = it->second;
}

// This may be called from another thread.
void ThreadData::SnapshotDeathMap(DeathMap *output) const {
  AutoLock lock(*const_cast<Lock*>(&lock_));
  for (DeathMap::const_iterator it = death_map_.begin();
       it != death_map_.end(); ++it)
    (*output)[it->first] = it->second;
}

#ifdef OS_WIN
void ThreadData::RunOnAllThreads(void (*function)()) {
  ThreadData* list = first();  // Get existing list.

  std::vector<MessageLoop*> message_loops;
  for (ThreadData* it = list; it; it = it->next()) {
    if (current() != it && it->message_loop())
      message_loops.push_back(it->message_loop());
  }

  ThreadSafeDownCounter* counter =
    new ThreadSafeDownCounter(message_loops.size() + 1);  // Extra one for us!

  HANDLE completion_handle = CreateEvent(NULL, false, false, NULL);
  // Tell all other threads to run.
  for (size_t i = 0; i < message_loops.size(); ++i)
    message_loops[i]->PostTask(FROM_HERE,
        new RunTheStatic(function, completion_handle, counter));

  // Also run Task on our thread.
  RunTheStatic local_task(function, completion_handle, counter);
  local_task.Run();

  WaitForSingleObject(completion_handle, INFINITE);
  int ret_val = CloseHandle(completion_handle);
  DCHECK(ret_val);
}
#endif

// static
bool ThreadData::StartTracking(bool status) {
#ifndef TRACK_ALL_TASK_OBJECTS
  return false;  // Not compiled in.
#else
  if (!status) {
    AutoLock lock(list_lock_);
    DCHECK(status_ == ACTIVE || status_ == SHUTDOWN);
    status_ = SHUTDOWN;
    return true;
  }
  AutoLock lock(list_lock_);
  DCHECK(status_ == UNINITIALIZED);
  CHECK(tls_index_.Initialize(NULL));
  status_ = ACTIVE;
  return true;
#endif
}

// static
bool ThreadData::IsActive() {
  return status_ == ACTIVE;
}

#ifdef OS_WIN
// static
void ThreadData::ShutdownMultiThreadTracking() {
  // Using lock, guarantee that no new ThreadData instances will be created.
  if (!StartTracking(false))
    return;

  RunOnAllThreads(ShutdownDisablingFurtherTracking);

  // Now the *only* threads that might change the database are the threads with
  // no messages loops.  They might still be adding data to their birth records,
  // but since no objects are deleted on those threads, there will be no further
  // access to to cross-thread data.
  // We could do a cleanup on all threads except for the ones without
  // MessageLoops, but we won't bother doing cleanup (destruction of data) yet.
  return;
}
#endif

// static
void ThreadData::ShutdownSingleThreadedCleanup() {
  // We must be single threaded... but be careful anyway.
  if (!StartTracking(false))
    return;
  ThreadData* thread_data_list;
  {
    AutoLock lock(list_lock_);
    thread_data_list = first_;
    first_ = NULL;
  }

  while (thread_data_list) {
    ThreadData* next_thread_data = thread_data_list;
    thread_data_list = thread_data_list->next();

    for (BirthMap::iterator it = next_thread_data->birth_map_.begin();
         next_thread_data->birth_map_.end() != it; ++it)
      delete it->second;  // Delete the Birth Records.
    next_thread_data->birth_map_.clear();
    next_thread_data->death_map_.clear();
    delete next_thread_data;  // Includes all Death Records.
  }

  CHECK(tls_index_.initialized());
  tls_index_.Free();
  DCHECK(!tls_index_.initialized());
  status_ = UNINITIALIZED;
}

// static
void ThreadData::ShutdownDisablingFurtherTracking() {
  // Redundantly set status SHUTDOWN on this thread.
  if (!StartTracking(false))
    return;
}


//------------------------------------------------------------------------------

ThreadData::ThreadSafeDownCounter::ThreadSafeDownCounter(size_t count)
    : remaining_count_(count) {
  DCHECK(remaining_count_ > 0);
}

bool ThreadData::ThreadSafeDownCounter::LastCaller() {
  {
    AutoLock lock(lock_);
    if (--remaining_count_)
      return false;
  }  // Release lock, so we can delete everything in this instance.
  delete this;
  return true;
}

//------------------------------------------------------------------------------
#ifdef OS_WIN
ThreadData::RunTheStatic::RunTheStatic(FunctionPointer function,
                                       HANDLE completion_handle,
                                       ThreadSafeDownCounter* counter)
    : function_(function),
      completion_handle_(completion_handle),
      counter_(counter) {
}

void ThreadData::RunTheStatic::Run() {
  function_();
  if (counter_->LastCaller())
    SetEvent(completion_handle_);
}
#endif

//------------------------------------------------------------------------------
// Individual 3-tuple of birth (place and thread) along with death thread, and
// the accumulated stats for instances (DeathData).

Snapshot::Snapshot(const BirthOnThread& birth_on_thread,
                   const ThreadData& death_thread,
                   const DeathData& death_data)
    : birth_(&birth_on_thread),
      death_thread_(&death_thread),
      death_data_(death_data) {
}

Snapshot::Snapshot(const BirthOnThread& birth_on_thread, int count)
    : birth_(&birth_on_thread),
      death_thread_(NULL),
      death_data_(DeathData(count)) {
}

const std::string Snapshot::DeathThreadName() const {
  if (death_thread_)
    return death_thread_->ThreadName();
  return "Still_Alive";
}

void Snapshot::Write(std::string* output) const {
  death_data_.Write(output);
  StringAppendF(output, "%s->%s ",
                birth_->birth_thread()->ThreadName().c_str(),
                death_thread_->ThreadName().c_str());
  birth_->location().Write(true, true, output);
}

void Snapshot::Add(const Snapshot& other) {
  death_data_.AddDeathData(other.death_data_);
}

//------------------------------------------------------------------------------
// DataCollector

DataCollector::DataCollector() {
  DCHECK(ThreadData::IsActive());

  ThreadData* my_list = ThreadData::current()->first();

  count_of_contributing_threads_ = 0;
  for (ThreadData* thread_data = my_list;
       thread_data;
       thread_data = thread_data->next()) {
    ++count_of_contributing_threads_;
  }

  // Gather data serially.  A different constructor could be used to do in
  // parallel, and then invoke an OnCompletion task.
  for (ThreadData* thread_data = my_list;
       thread_data;
       thread_data = thread_data->next()) {
    Append(*thread_data);
  }
}

void DataCollector::Append(const ThreadData& thread_data) {
  // Get copy of data (which is done under ThreadData's lock).
  ThreadData::BirthMap birth_map;
  thread_data.SnapshotBirthMap(&birth_map);
  ThreadData::DeathMap death_map;
  thread_data.SnapshotDeathMap(&death_map);

  // Use our lock to protect our accumulation activity.
  AutoLock lock(accumulation_lock_);

  DCHECK(count_of_contributing_threads_);

  for (ThreadData::DeathMap::const_iterator it = death_map.begin();
       it != death_map.end(); ++it) {
    collection_.push_back(Snapshot(*it->first, thread_data, it->second));
    global_birth_count_[it->first] -= it->first->birth_count();
  }

  for (ThreadData::BirthMap::const_iterator it = birth_map.begin();
       it != birth_map.end(); ++it) {
    global_birth_count_[it->second] += it->second->birth_count();
  }

  --count_of_contributing_threads_;
}

DataCollector::Collection* DataCollector::collection() {
  DCHECK(!count_of_contributing_threads_);
  return &collection_;
}

void DataCollector::AddListOfLivingObjects() {
  DCHECK(!count_of_contributing_threads_);
  for (BirthCount::iterator it = global_birth_count_.begin();
       it != global_birth_count_.end(); ++it) {
    if (it->second > 0)
      collection_.push_back(Snapshot(*it->first, it->second));
  }
}

//------------------------------------------------------------------------------
// Aggregation

void Aggregation::AddDeathSnapshot(const Snapshot& snapshot) {
  AddBirth(snapshot.birth());
  death_threads_[snapshot.death_thread()]++;
  AddDeathData(snapshot.death_data());
}

void Aggregation::AddBirths(const Births& births) {
  AddBirth(births);
  birth_count_ += births.birth_count();
}
void Aggregation::AddBirth(const BirthOnThread& birth) {
  AddBirthPlace(birth.location());
  birth_threads_[birth.birth_thread()]++;
}

void Aggregation::AddBirthPlace(const Location& location) {
  locations_[location]++;
  birth_files_[location.file_name()]++;
}

void Aggregation::Write(std::string* output) const {
  if (locations_.size() == 1) {
    locations_.begin()->first.Write(true, true, output);
  } else {
    StringAppendF(output, "%d Locations. ", locations_.size());
    if (birth_files_.size() > 1)
      StringAppendF(output, "%d Files. ", birth_files_.size());
    else
      StringAppendF(output, "All born in %s. ",
                    birth_files_.begin()->first.c_str());
  }

  if (birth_threads_.size() > 1)
    StringAppendF(output, "%d BirthingThreads. ", birth_threads_.size());
  else
    StringAppendF(output, "All born on %s. ",
                  birth_threads_.begin()->first->ThreadName().c_str());

  if (death_threads_.size() > 1) {
    StringAppendF(output, "%d DeathThreads. ", death_threads_.size());
  } else {
    if (death_threads_.begin()->first)
      StringAppendF(output, "All deleted on %s. ",
                  death_threads_.begin()->first->ThreadName().c_str());
    else
      output->append("All these objects are still alive.");
  }

  if (birth_count_ > 1)
    StringAppendF(output, "Births=%d ", birth_count_);

  DeathData::Write(output);
}

void Aggregation::Clear() {
  birth_count_ = 0;
  birth_files_.clear();
  locations_.clear();
  birth_threads_.clear();
  DeathData::Clear();
  death_threads_.clear();
}

//------------------------------------------------------------------------------
// Comparison object for sorting.

Comparator::Comparator()
    : selector_(NIL),
      tiebreaker_(NULL),
      combined_selectors_(0),
      use_tiebreaker_for_sort_only_(false) {}

void Comparator::Clear() {
  if (tiebreaker_) {
    tiebreaker_->Clear();
    delete tiebreaker_;
    tiebreaker_ = NULL;
  }
  use_tiebreaker_for_sort_only_ = false;
  selector_ = NIL;
}

void Comparator::Sort(DataCollector::Collection* collection) const {
  std::sort(collection->begin(), collection->end(), *this);
}


bool Comparator::operator()(const Snapshot& left,
                            const Snapshot& right) const {
  switch (selector_) {
    case BIRTH_THREAD:
      if (left.birth_thread() != right.birth_thread() &&
          left.birth_thread()->ThreadName() !=
          right.birth_thread()->ThreadName())
        return left.birth_thread()->ThreadName() <
            right.birth_thread()->ThreadName();
      break;

    case DEATH_THREAD:
      if (left.death_thread() != right.death_thread() &&
          left.DeathThreadName() !=
          right.DeathThreadName()) {
        if (!left.death_thread())
          return true;
        if (!right.death_thread())
          return false;
        return left.DeathThreadName() <
             right.DeathThreadName();
      }
      break;

    case BIRTH_FILE:
      if (left.location().file_name() != right.location().file_name()) {
        int comp = strcmp(left.location().file_name(),
                          right.location().file_name());
        if (comp)
          return 0 > comp;
      }
      break;

    case BIRTH_FUNCTION:
      if (left.location().function_name() != right.location().function_name()) {
        int comp = strcmp(left.location().function_name(),
                          right.location().function_name());
        if (comp)
          return 0 > comp;
      }
      break;

    case BIRTH_LINE:
      if (left.location().line_number() != right.location().line_number())
        return left.location().line_number() <
            right.location().line_number();
      break;

    case COUNT:
      if (left.count() != right.count())
        return left.count() > right.count();  // Sort large at front of vector.
      break;

    case AVERAGE_DURATION:
      if (left.AverageMsDuration() != right.AverageMsDuration())
        return left.AverageMsDuration() > right.AverageMsDuration();
      break;

    default:
      break;
  }
  if (tiebreaker_)
    return tiebreaker_->operator()(left, right);
  return false;
}

bool Comparator::Equivalent(const Snapshot& left,
                            const Snapshot& right) const {
  switch (selector_) {
    case BIRTH_THREAD:
      if (left.birth_thread() != right.birth_thread() &&
          left.birth_thread()->ThreadName() !=
              right.birth_thread()->ThreadName())
        return false;
      break;

    case DEATH_THREAD:
      if (left.death_thread() != right.death_thread() &&
          left.DeathThreadName() != right.DeathThreadName())
        return false;
      break;

    case BIRTH_FILE:
      if (left.location().file_name() != right.location().file_name()) {
        int comp = strcmp(left.location().file_name(),
                          right.location().file_name());
        if (comp)
          return false;
      }
      break;

    case BIRTH_FUNCTION:
      if (left.location().function_name() != right.location().function_name()) {
        int comp = strcmp(left.location().function_name(),
                          right.location().function_name());
        if (comp)
          return false;
      }
      break;

    case COUNT:
      if (left.count() != right.count())
        return false;
      break;

    case AVERAGE_DURATION:
      if (left.life_duration() != right.life_duration())
        return false;
      break;

    default:
      break;
  }
  if (tiebreaker_ && !use_tiebreaker_for_sort_only_)
    return tiebreaker_->Equivalent(left, right);
  return true;
}

bool Comparator::Acceptable(const Snapshot& sample) const {
  if (required_.size()) {
    switch (selector_) {
      case BIRTH_THREAD:
        if (sample.birth_thread()->ThreadName().find(required_) ==
            std::string::npos)
          return false;
        break;

      case DEATH_THREAD:
        if (sample.DeathThreadName().find(required_) == std::string::npos)
          return false;
        break;

      case BIRTH_FILE:
        if (!strstr(sample.location().file_name(), required_.c_str()))
          return false;
        break;

      case BIRTH_FUNCTION:
        if (!strstr(sample.location().function_name(), required_.c_str()))
          return false;
        break;

      default:
        break;
    }
  }
  if (tiebreaker_ && !use_tiebreaker_for_sort_only_)
    return tiebreaker_->Acceptable(sample);
  return true;
}

void Comparator::SetTiebreaker(Selector selector, const std::string required) {
  if (selector == selector_ || NIL == selector)
    return;
  combined_selectors_ |= selector;
  if (NIL == selector_) {
    selector_ = selector;
    if (required.size())
      required_ = required;
    return;
  }
  if (tiebreaker_) {
    if (use_tiebreaker_for_sort_only_) {
      Comparator* temp = new Comparator;
      temp->tiebreaker_ = tiebreaker_;
      tiebreaker_ = temp;
    }
  } else {
    tiebreaker_ = new Comparator;
    DCHECK(!use_tiebreaker_for_sort_only_);
  }
  tiebreaker_->SetTiebreaker(selector, required);
}

bool Comparator::IsGroupedBy(Selector selector) const {
  return 0 != (selector & combined_selectors_);
}

void Comparator::SetSubgroupTiebreaker(Selector selector) {
  if (selector == selector_ || NIL == selector)
    return;
  if (!tiebreaker_) {
    use_tiebreaker_for_sort_only_ = true;
    tiebreaker_ = new Comparator;
    tiebreaker_->SetTiebreaker(selector, "");
  } else {
    tiebreaker_->SetSubgroupTiebreaker(selector);
  }
}

bool Comparator::WriteSortGrouping(const Snapshot& sample,
                                       std::string* output) const {
  bool wrote_data = false;
  switch (selector_) {
    case BIRTH_THREAD:
      StringAppendF(output, "All new on %s ",
                    sample.birth_thread()->ThreadName().c_str());
      wrote_data = true;
      break;

    case DEATH_THREAD:
      if (sample.death_thread())
        StringAppendF(output, "All deleted on %s ",
                      sample.DeathThreadName().c_str());
      else
        output->append("All still alive ");
      wrote_data = true;
      break;

    case BIRTH_FILE:
      StringAppendF(output, "All born in %s ",
                    sample.location().file_name());
      break;

    case BIRTH_FUNCTION:
      output->append("All born in ");
      sample.location().WriteFunctionName(output);
      output->push_back(' ');
      break;

    default:
      break;
  }
  if (tiebreaker_ && !use_tiebreaker_for_sort_only_) {
    wrote_data |= tiebreaker_->WriteSortGrouping(sample, output);
  }
  return wrote_data;
}

void Comparator::WriteSnapshot(const Snapshot& sample,
                               std::string* output) const {
  sample.death_data().Write(output);
  if (!(combined_selectors_ & BIRTH_THREAD) ||
      !(combined_selectors_ & DEATH_THREAD))
    StringAppendF(output, "%s->%s ",
                  (combined_selectors_ & BIRTH_THREAD) ? "*" :
                    sample.birth().birth_thread()->ThreadName().c_str(),
                  (combined_selectors_ & DEATH_THREAD) ? "*" :
                    sample.DeathThreadName().c_str());
  sample.birth().location().Write(!(combined_selectors_ & BIRTH_FILE),
                                  !(combined_selectors_ & BIRTH_FUNCTION),
                                  output);
}

}  // namespace tracked_objects
