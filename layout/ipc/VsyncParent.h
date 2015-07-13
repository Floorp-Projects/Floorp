/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ipc_VsyncParent_h
#define mozilla_layout_ipc_VsyncParent_h

#include "mozilla/layout/PVsyncParent.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsCOMPtr.h"
#include "nsRefPtr.h"

class nsIThread;

namespace mozilla {

namespace ipc {
class BackgroundParentImpl;
} // namespace ipc

namespace layout {

// Use PBackground thread in the main process to send vsync notifications to
// content process. This actor will be released when its parent protocol calls
// DeallocPVsyncParent().
class VsyncParent final : public PVsyncParent,
                          public VsyncObserver
{
  friend class mozilla::ipc::BackgroundParentImpl;

private:
  static already_AddRefed<VsyncParent> Create();

  VsyncParent();
  virtual ~VsyncParent();

  virtual bool NotifyVsync(TimeStamp aTimeStamp) override;

  virtual bool RecvObserve() override;
  virtual bool RecvUnobserve() override;
  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  void DispatchVsyncEvent(TimeStamp aTimeStamp);

  bool mObservingVsync;
  bool mDestroyed;
  nsCOMPtr<nsIThread> mBackgroundThread;
  nsRefPtr<RefreshTimerVsyncDispatcher> mVsyncDispatcher;
};

} // namespace layout
} // namespace mozilla

#endif  //mozilla_layout_ipc_VsyncParent_h
