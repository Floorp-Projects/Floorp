/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowsLocationChild_h__
#define mozilla_dom_WindowsLocationChild_h__

#include "mozilla/dom/PWindowsLocationChild.h"
#include "mozilla/WeakPtr.h"

class ILocation;

namespace mozilla::dom {

// Geolocation actor in utility process.
class WindowsLocationChild final : public PWindowsLocationChild,
                                   public SupportsWeakPtr {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WindowsLocationChild, override);

 public:
  WindowsLocationChild();

  using IPCResult = ::mozilla::ipc::IPCResult;

  IPCResult RecvStartup();
  IPCResult RecvRegisterForReport();
  IPCResult RecvUnregisterForReport();
  IPCResult RecvSetHighAccuracy(bool aEnable);
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~WindowsLocationChild() override;

  // The COM object the actors are proxying calls for.
  RefPtr<ILocation> mLocation;

  bool mHighAccuracy = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WindowsLocationChild_h__
