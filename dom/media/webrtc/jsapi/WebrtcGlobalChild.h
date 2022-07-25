/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_CHILD_H_
#define _WEBRTC_GLOBAL_CHILD_H_

#include "mozilla/dom/PWebrtcGlobalChild.h"

namespace mozilla::dom {

class WebrtcGlobalChild : public PWebrtcGlobalChild {
  friend class ContentChild;

  bool mShutdown;

  MOZ_IMPLICIT WebrtcGlobalChild();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvGetStats(
      const nsAString& aPcIdFilter, GetStatsResolver&& aResolve) override;
  virtual mozilla::ipc::IPCResult RecvClearStats() override;
  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we can't do MOZ_CAN_RUN_SCRIPT in
  // ipdl-generated things yet.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual mozilla::ipc::IPCResult RecvGetLog(
      GetLogResolver&& aResolve) override;
  virtual mozilla::ipc::IPCResult RecvClearLog() override;
  virtual mozilla::ipc::IPCResult RecvSetAecLogging(
      const bool& aEnable) override;
  virtual mozilla::ipc::IPCResult RecvSetDebugMode(const int& aLevel) override;

 public:
  virtual ~WebrtcGlobalChild();
  static WebrtcGlobalChild* Create();
};

}  // namespace mozilla::dom

#endif  // _WEBRTC_GLOBAL_CHILD_H_
