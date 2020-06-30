/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/common/ipc_channel_utils.h"

#include "chrome/common/ipc_message.h"

#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif

namespace IPC {

void AddIPCProfilerMarker(const Message& aMessage, int32_t aOtherPid,
                          mozilla::ipc::MessageDirection aDirection,
                          mozilla::ipc::MessagePhase aPhase) {
#ifdef MOZ_GECKO_PROFILER
  if (aMessage.routing_id() != MSG_ROUTING_NONE &&
      profiler_feature_active(ProfilerFeature::IPCMessages)) {
    if (aOtherPid == -1) {
      DLOG(WARNING) << "Unable to record IPC profile marker, other PID not set";
      return;
    }

    PROFILER_ADD_MARKER_WITH_PAYLOAD(
        "IPC", IPC, IPCMarkerPayload,
        (aOtherPid, aMessage.seqno(), aMessage.type(),
         mozilla::ipc::UnknownSide, aDirection, aPhase, aMessage.is_sync(),
         mozilla::TimeStamp::NowUnfuzzed()));
  }
#endif
}

}  // namespace IPC
