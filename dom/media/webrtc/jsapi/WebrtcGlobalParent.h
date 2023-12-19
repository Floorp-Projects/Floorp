/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_PARENT_H_
#define _WEBRTC_GLOBAL_PARENT_H_

#include "mozilla/dom/PWebrtcGlobalParent.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class WebrtcParents;

class WebrtcGlobalParent : public PWebrtcGlobalParent {
  friend class ContentParent;
  friend class WebrtcGlobalInformation;
  friend class WebrtcContentParents;

  bool mShutdown;
  nsTHashSet<nsString> mPcids;

  MOZ_IMPLICIT WebrtcGlobalParent();

  static WebrtcGlobalParent* Alloc();
  static bool Dealloc(WebrtcGlobalParent* aActor);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual mozilla::ipc::IPCResult Recv__delete__() override;
  // Notification that a PeerConnection exists, and stats polling can begin
  // if it hasn't already begun due to a previously created PeerConnection.
  virtual mozilla::ipc::IPCResult RecvPeerConnectionCreated(
      const nsAString& aPcId, const bool& aIsLongTermStatsDisabled) override;
  // Notification that a PeerConnection no longer exists, and stats polling
  // can end if there are no other PeerConnections.
  virtual mozilla::ipc::IPCResult RecvPeerConnectionDestroyed(
      const nsAString& aPcid) override;
  // Ditto but we have final stats
  virtual mozilla::ipc::IPCResult RecvPeerConnectionFinalStats(
      const RTCStatsReportInternal& aFinalStats) override;
  virtual ~WebrtcGlobalParent();

 public:
  NS_INLINE_DECL_REFCOUNTING(WebrtcGlobalParent)

  bool IsActive() { return !mShutdown; }
};

}  // namespace mozilla::dom

#endif  // _WEBRTC_GLOBAL_PARENT_H_
