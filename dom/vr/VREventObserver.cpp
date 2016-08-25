/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VREventObserver.h"

#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "VRManagerChild.h"

namespace mozilla {
namespace dom {

using namespace gfx;

/**
 * This class is used by nsGlobalWindow to implement window.onvrdisplayconnected,
 * window.onvrdisplaydisconnected, and window.onvrdisplaypresentchange.
 */
VREventObserver::VREventObserver(nsGlobalWindow* aGlobalWindow)
  : mWindow(aGlobalWindow)
{
  MOZ_ASSERT(aGlobalWindow && aGlobalWindow->IsInnerWindow());

  VRManagerChild* vmc = VRManagerChild::Get();
  if (vmc) {
    vmc->AddListener(this);
  }
}

VREventObserver::~VREventObserver()
{
  VRManagerChild* vmc = VRManagerChild::Get();
  if (vmc) {
    vmc->RemoveListener(this);
  }
}

void
VREventObserver::NotifyVRDisplayConnect()
{
  /**
   * We do not call nsGlobalWindow::NotifyActiveVRDisplaysChanged here, as we
   * can assume that a newly enumerated display is not presenting WebVR
   * content.
   */
  if (mWindow->AsInner()->IsCurrentInnerWindow()) {
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mWindow->GetOuterWindow()->DispatchCustomEvent(
      NS_LITERAL_STRING("vrdisplayconnected"));
  }
}

void
VREventObserver::NotifyVRDisplayDisconnect()
{
  if (mWindow->AsInner()->IsCurrentInnerWindow()) {
    mWindow->NotifyActiveVRDisplaysChanged();
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mWindow->GetOuterWindow()->DispatchCustomEvent(
      NS_LITERAL_STRING("vrdisplaydisconnected"));
  }
}

void
VREventObserver::NotifyVRDisplayPresentChange()
{
  if (mWindow->AsInner()->IsCurrentInnerWindow()) {
    mWindow->NotifyActiveVRDisplaysChanged();
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mWindow->GetOuterWindow()->DispatchCustomEvent(
      NS_LITERAL_STRING("vrdisplaypresentchange"));
  }
}

} // namespace dom
} // namespace mozilla
