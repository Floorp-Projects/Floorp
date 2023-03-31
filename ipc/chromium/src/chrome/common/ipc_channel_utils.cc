/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_channel_utils.h"

#include "mozilla/ProfilerMarkers.h"
#include "chrome/common/ipc_message.h"
#include "base/process.h"

namespace IPC {

void AddIPCProfilerMarker(const Message& aMessage, int32_t aOtherPid,
                          mozilla::ipc::MessageDirection aDirection,
                          mozilla::ipc::MessagePhase aPhase) {
  if (aMessage.routing_id() != MSG_ROUTING_NONE &&
      profiler_feature_active(ProfilerFeature::IPCMessages)) {
    if (static_cast<base::ProcessId>(aOtherPid) == base::kInvalidProcessId) {
      DLOG(WARNING) << "Unable to record IPC profile marker, other PID not set";
      return;
    }

    if (profiler_is_locked_on_current_thread()) {
      // One of the profiler mutexes is locked on this thread, don't record
      // markers, because we don't want to expose profiler IPCs due to the
      // profiler itself, and also to avoid possible re-entrancy issues.
      return;
    }

    // The current timestamp must be given to the `IPCMarker` payload.
    [[maybe_unused]] const mozilla::TimeStamp now = mozilla::TimeStamp::Now();
    bool isThreadBeingProfiled =
        profiler_thread_is_being_profiled_for_markers();
    PROFILER_MARKER(
        "IPC", IPC,
        mozilla::MarkerOptions(
            mozilla::MarkerTiming::InstantAt(now),
            // If the thread is being profiled, add the marker to
            // the current thread. If the thread is not being
            // profiled, add the marker to the main thread. It will
            // appear in the main thread's IPC track. Profiler
            // analysis UI correlates all the IPC markers from
            // different threads and generates processed markers.
            isThreadBeingProfiled ? mozilla::MarkerThreadId::CurrentThread()
                                  : mozilla::MarkerThreadId::MainThread()),
        IPCMarker, now, now, aOtherPid, aMessage.seqno(), aMessage.type(),
        mozilla::ipc::UnknownSide, aDirection, aPhase, aMessage.is_sync(),
        // aOriginThreadId: If the thread is being profiled, do not include a
        // thread ID, as it's the same as the markers. Only include this field
        // when the marker is being sent from another thread.
        isThreadBeingProfiled ? mozilla::MarkerThreadId{}
                              : mozilla::MarkerThreadId::CurrentThread());
  }
}

}  // namespace IPC
