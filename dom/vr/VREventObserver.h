/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VREventObserver_h
#define mozilla_dom_VREventObserver_h

#include "mozilla/dom/VRDisplayEventBinding.h"

class nsGlobalWindow;

namespace mozilla {
namespace dom {

class VREventObserver final
{
public:
  ~VREventObserver();
  explicit VREventObserver(nsGlobalWindow* aGlobalWindow);

  void NotifyVRDisplayMounted(uint32_t aDisplayID);
  void NotifyVRDisplayUnmounted(uint32_t aDisplayID);
  void NotifyVRDisplayNavigation(uint32_t aDisplayID);
  void NotifyVRDisplayRequested(uint32_t aDisplayID);
  void NotifyVRDisplayConnect();
  void NotifyVRDisplayDisconnect();
  void NotifyVRDisplayPresentChange();

private:
  // Weak pointer, instance is owned by mWindow.
  nsGlobalWindow* MOZ_NON_OWNING_REF mWindow;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_VREventObserver_h
