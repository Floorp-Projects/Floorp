/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_VsyncChild_h
#define mozilla_dom_ipc_VsyncChild_h

#include "mozilla/dom/PVsyncChild.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsTObserverArray.h"

namespace mozilla {

class VsyncObserver;

namespace dom {

// The PVsyncChild actor receives a vsync event from the main process and
// delivers it to the child process. Currently this is restricted to the main
// thread only. The actor will stay alive until the process dies or its
// PVsyncParent actor dies.
class VsyncChild final : public PVsyncChild {
  NS_INLINE_DECL_REFCOUNTING(VsyncChild)

  friend class PVsyncChild;

 public:
  VsyncChild();

  void AddChildRefreshTimer(VsyncObserver* aVsyncObserver);
  void RemoveChildRefreshTimer(VsyncObserver* aVsyncObserver);

  TimeDuration GetVsyncRate();

 private:
  virtual ~VsyncChild();

  mozilla::ipc::IPCResult RecvNotify(const VsyncEvent& aVsync,
                                     const float& aVsyncRate);
  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  bool mIsShutdown;
  TimeDuration mVsyncRate;
  nsTObserverArray<VsyncObserver*> mObservers;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ipc_VsyncChild_h
