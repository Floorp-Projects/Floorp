/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_hal_Types_h
#define mozilla_hal_Types_h

#include "ipc/EnumSerializer.h"
#include "mozilla/BitSet.h"
#include "mozilla/Observer.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace hal {

/**
 * These constants specify special values for content process IDs.  You can get
 * a content process ID by calling ContentChild::GetID() or
 * ContentParent::GetChildID().
 */
const uint64_t CONTENT_PROCESS_ID_UNKNOWN = uint64_t(-1);
const uint64_t CONTENT_PROCESS_ID_MAIN = 0;

// Note that we rely on the order of this enum's entries.  Higher priorities
// should have larger int values.
enum ProcessPriority {
  PROCESS_PRIORITY_UNKNOWN = -1,
  PROCESS_PRIORITY_BACKGROUND,
  PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE,
  PROCESS_PRIORITY_FOREGROUND_KEYBOARD,
  // The special class for the preallocated process, high memory priority but
  // low CPU priority.
  PROCESS_PRIORITY_PREALLOC,
  // Any priority greater than or equal to FOREGROUND is considered
  // "foreground" for the purposes of priority testing, for example
  // CurrentProcessIsForeground().
  PROCESS_PRIORITY_FOREGROUND,
  PROCESS_PRIORITY_FOREGROUND_HIGH,
  PROCESS_PRIORITY_PARENT_PROCESS,
  NUM_PROCESS_PRIORITY
};

/**
 * Convert a ProcessPriority enum value to a string.  The strings returned by
 * this function are statically allocated; do not attempt to free one!
 *
 * If you pass an unknown process priority, we fatally assert in debug
 * builds and otherwise return "???".
 */
const char* ProcessPriorityToString(ProcessPriority aPriority);

/**
 * Used by ModifyWakeLock
 */
enum WakeLockControl {
  WAKE_LOCK_REMOVE_ONE = -1,
  WAKE_LOCK_NO_CHANGE = 0,
  WAKE_LOCK_ADD_ONE = 1,
  NUM_WAKE_LOCK
};

/**
 * Represents a workload shared by a group of threads that should be completed
 * in a target duration each cycle.
 *
 * This is created using hal::CreatePerformanceHintSession(). Each cycle, the
 * actual work duration should be reported using ReportActualWorkDuration(). The
 * system can then adjust the scheduling accordingly in order to achieve the
 * target.
 */
class PerformanceHintSession {
 public:
  virtual ~PerformanceHintSession() = default;

  // Updates the session's target work duration for each cycle.
  virtual void UpdateTargetWorkDuration(TimeDuration aDuration) = 0;

  // Reports the session's actual work duration for a cycle.
  virtual void ReportActualWorkDuration(TimeDuration aDuration) = 0;
};

/**
 * Categorizes the CPUs on the system in to big, medium, and little classes.
 *
 * A set bit in each bitset indicates that the CPU of that index belongs to that
 * class. If the CPUs are fully homogeneous they are all categorized as big. If
 * there are only 2 classes, they are categorized as either big or little.
 * Finally, if there are >= 3 classes, the remainder will be categorized as
 * medium.
 *
 * If there are more than MAX_CPUS present we are unable to represent this
 * information.
 */
struct HeterogeneousCpuInfo {
  // We use a max of 32 because currently this is only implemented for Android
  // where we are unlikely to need more CPUs than that, and it simplifies
  // dealing with cpu_set_t as CPU_SETSIZE is 32 on 32-bit Android.
  static const size_t MAX_CPUS = 32;
  size_t mTotalNumCpus;
  mozilla::BitSet<MAX_CPUS> mLittleCpus;
  mozilla::BitSet<MAX_CPUS> mMediumCpus;
  mozilla::BitSet<MAX_CPUS> mBigCpus;
};

}  // namespace hal
}  // namespace mozilla

namespace IPC {

/**
 * WakeLockControl serializer.
 */
template <>
struct ParamTraits<mozilla::hal::WakeLockControl>
    : public ContiguousEnumSerializer<mozilla::hal::WakeLockControl,
                                      mozilla::hal::WAKE_LOCK_REMOVE_ONE,
                                      mozilla::hal::NUM_WAKE_LOCK> {};

template <>
struct ParamTraits<mozilla::hal::ProcessPriority>
    : public ContiguousEnumSerializer<mozilla::hal::ProcessPriority,
                                      mozilla::hal::PROCESS_PRIORITY_UNKNOWN,
                                      mozilla::hal::NUM_PROCESS_PRIORITY> {};

}  // namespace IPC

#endif  // mozilla_hal_Types_h
