/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VREventObserver_h
#define mozilla_dom_VREventObserver_h

#include "mozilla/dom/VRDisplayEventBinding.h"
#include "nsISupportsImpl.h" // for NS_INLINE_DECL_REFCOUNTING

class nsGlobalWindowInner;

namespace mozilla {
namespace dom {

class VREventObserver final
{
public:
  NS_INLINE_DECL_REFCOUNTING(VREventObserver)
  explicit VREventObserver(nsGlobalWindowInner* aGlobalWindow);

  void NotifyAfterLoad();
  void NotifyVRDisplayMounted(uint32_t aDisplayID);
  void NotifyVRDisplayUnmounted(uint32_t aDisplayID);
  void NotifyVRDisplayNavigation(uint32_t aDisplayID);
  void NotifyVRDisplayRequested(uint32_t aDisplayID);
  void NotifyVRDisplayConnect(uint32_t aDisplayID);
  void NotifyVRDisplayDisconnect(uint32_t aDisplayID);
  void NotifyVRDisplayPresentChange(uint32_t aDisplayID);

  void DisconnectFromOwner();
  void UpdateSpentTimeIn2DTelemetry(bool aUpdate);

private:
  ~VREventObserver();

  RefPtr<nsGlobalWindowInner> mWindow;
  // For WebVR telemetry for tracking users who view content
  // in the 2D view.
  TimeStamp mSpendTimeIn2DView;
  bool mIs2DView;
  bool mHasReset;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_VREventObserver_h
