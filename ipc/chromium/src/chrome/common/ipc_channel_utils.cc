/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_channel_utils.h"

#include "mozilla/ProfilerMarkers.h"
#include "chrome/common/ipc_message.h"

namespace IPC {

void AddIPCProfilerMarker(const Message& aMessage, int32_t aOtherPid,
                          mozilla::ipc::MessageDirection aDirection,
                          mozilla::ipc::MessagePhase aPhase) {
  if (aMessage.routing_id() != MSG_ROUTING_NONE &&
      profiler_feature_active(ProfilerFeature::IPCMessages)) {
    if (aOtherPid == -1) {
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
    [[maybe_unused]] const mozilla::TimeStamp now =
        mozilla::TimeStamp::NowUnfuzzed();
    PROFILER_MARKER("IPC", IPC, mozilla::MarkerTiming::InstantAt(now),
                    IPCMarker, now, now, aOtherPid, aMessage.seqno(),
                    aMessage.type(), mozilla::ipc::UnknownSide, aDirection,
                    aPhase, aMessage.is_sync());
  }
}

}  // namespace IPC
