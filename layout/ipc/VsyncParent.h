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
}

namespace layout {

// Use PBackground thread in the main process to send vsync notifications to
// content process. This actor will be released when its parent protocol calls
// DeallocPVsyncParent().
class VsyncParent MOZ_FINAL : public PVsyncParent,
                              public VsyncObserver
{
  friend class mozilla::ipc::BackgroundParentImpl;

private:
  static already_AddRefed<VsyncParent> Create();

  VsyncParent();
  virtual ~VsyncParent();

  virtual bool NotifyVsync(TimeStamp aTimeStamp) MOZ_OVERRIDE;

  virtual bool RecvObserve() MOZ_OVERRIDE;
  virtual bool RecvUnobserve() MOZ_OVERRIDE;
  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) MOZ_OVERRIDE;

  void DispatchVsyncEvent(TimeStamp aTimeStamp);

  bool mObservingVsync;
  bool mDestroyed;
  nsCOMPtr<nsIThread> mBackgroundThread;
  nsRefPtr<RefreshTimerVsyncDispatcher> mVsyncDispatcher;
};

} //layout
} //mozilla

#endif  //mozilla_layout_ipc_VsyncParent_h
