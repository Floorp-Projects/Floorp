/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_MutexIDs_h
#define vm_MutexIDs_h

#include "threading/Mutex.h"

// Central definition point for mutex ordering.

#define FOR_EACH_MUTEX(_)             \
  _(TestMutex,                   100) \
  _(ShellContextWatchdog,        100) \
  _(ShellWorkerThreads,          100) \
  _(ShellThreadCooperation,      100) \
  _(ShellArrayBufferMailbox,     100) \
                                      \
  _(RuntimeExclusiveAccess,      200) \
                                      \
  _(GlobalHelperThreadState,     300) \
                                      \
  _(InternalAsyncTasks,          350) \
                                      \
  _(GCLock,                      400) \
                                      \
  _(WasmInitBuiltinThunks,       450) \
                                      \
  _(SharedImmutableStringsCache, 500) \
  _(FutexThread,                 500) \
  _(PromiseTaskPtrVector,        500) \
  _(GeckoProfilerStrings,        500) \
  _(ProtectedRegionTree,         500) \
  _(WasmSigIdSet,                500) \
  _(ShellOffThreadState,         500) \
  _(SimulatorCacheLock,          500) \
  _(Arm64SimulatorLock,          500) \
  _(IonSpewer,                   500) \
  _(PerfSpewer,                  500) \
  _(CacheIRSpewer,               500) \
  _(TraceLoggerThreadState,      500) \
  _(DateTimeInfoMutex,           500) \
  _(IcuTimeZoneStateMutex,       500) \
                                      \
  /* ProcessExecutableRegion > PromiseTaskPtrVector */ \
  /* WasmCodeProfilingLabels > PromiseTaskPtrVector */ \
  _(ProcessExecutableRegion,     550) \
  _(WasmCodeProfilingLabels,     550) \
                                      \
  _(TraceLoggerGraphState,       600) \
  _(VTuneLock,                   600)

namespace js {
namespace mutexid {

#define DEFINE_MUTEX_ID(name, order)  \
static const MutexId name { #name, order };
FOR_EACH_MUTEX(DEFINE_MUTEX_ID)
#undef DEFINE_MUTEX_ID

} // namespace mutexid
} // namespace js

#endif // vm_MutexIDs_h
