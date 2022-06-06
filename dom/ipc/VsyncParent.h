/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_VsyncParent_h
#define mozilla_dom_ipc_VsyncParent_h

#include "mozilla/dom/PVsyncParent.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

class nsIThread;

namespace mozilla::dom {

// Use PBackground thread in the main process to send vsync notifications to
// content process. This actor will be released when its parent protocol calls
// DeallocPVsyncParent().
class VsyncParent final : public PVsyncParent, public VsyncObserver {
  friend class PVsyncParent;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncParent, override)

 public:
  VsyncParent();
  void UpdateVsyncDispatcher(const RefPtr<VsyncDispatcher>& aVsyncDispatcher);

 private:
  virtual ~VsyncParent() = default;

  void NotifyVsync(const VsyncEvent& aVsync) override;
  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  mozilla::ipc::IPCResult RecvObserve();
  mozilla::ipc::IPCResult RecvUnobserve();

  void DispatchVsyncEvent(const VsyncEvent& aVsync);
  void UpdateVsyncRate();

  bool IsOnInitialThread();
  void AssertIsOnInitialThread();

  bool mObservingVsync;
  bool mDestroyed;
  nsCOMPtr<nsIThread> mInitialThread;
  RefPtr<VsyncDispatcher> mVsyncDispatcher;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ipc_VsyncParent_h
