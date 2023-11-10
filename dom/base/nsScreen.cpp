/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsScreen.h"
#include "mozilla/dom/Document.h"
#include "nsGlobalWindowInner.h"
#include "nsGlobalWindowOuter.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIDocShellTreeItem.h"
#include "nsLayoutUtils.h"
#include "nsDeviceContext.h"
#include "mozilla/widget/ScreenManager.h"

using namespace mozilla;
using namespace mozilla::dom;

nsScreen::nsScreen(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mScreenOrientation(new ScreenOrientation(aWindow, this)) {}

nsScreen::~nsScreen() = default;

// QueryInterface implementation for nsScreen
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsScreen)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsScreen, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsScreen, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsScreen, DOMEventTargetHelper,
                                   mScreenOrientation)

int32_t nsScreen::PixelDepth() {
  // Return 24 to prevent fingerprinting.
  if (ShouldResistFingerprinting(RFPTarget::ScreenPixelDepth)) {
    return 24;
  }
  nsDeviceContext* context = GetDeviceContext();
  if (NS_WARN_IF(!context)) {
    return 24;
  }
  return context->GetDepth();
}

nsPIDOMWindowOuter* nsScreen::GetOuter() const {
  if (nsPIDOMWindowInner* inner = GetOwner()) {
    return inner->GetOuterWindow();
  }
  return nullptr;
}

nsDeviceContext* nsScreen::GetDeviceContext() const {
  return nsLayoutUtils::GetDeviceContextForScreenInfo(GetOuter());
}

CSSIntRect nsScreen::GetRect() {
  // Return window inner rect to prevent fingerprinting.
  if (ShouldResistFingerprinting(RFPTarget::ScreenRect)) {
    return GetWindowInnerRect();
  }

  // Here we manipulate the value of aRect to represent the screen size,
  // if in RDM.
  if (nsPIDOMWindowInner* owner = GetOwner()) {
    if (Document* doc = owner->GetExtantDoc()) {
      Maybe<CSSIntSize> deviceSize =
          nsGlobalWindowOuter::GetRDMDeviceSize(*doc);
      if (deviceSize.isSome()) {
        const CSSIntSize& size = deviceSize.value();
        return {0, 0, size.width, size.height};
      }
    }
  }

  nsDeviceContext* context = GetDeviceContext();
  if (NS_WARN_IF(!context)) {
    return {};
  }

  nsRect r;
  context->GetRect(r);
  return CSSIntRect::FromAppUnitsRounded(r);
}

CSSIntRect nsScreen::GetAvailRect() {
  // Return window inner rect to prevent fingerprinting.
  if (ShouldResistFingerprinting(RFPTarget::ScreenAvailRect)) {
    return GetWindowInnerRect();
  }

  // Here we manipulate the value of aRect to represent the screen size,
  // if in RDM.
  if (nsPIDOMWindowInner* owner = GetOwner()) {
    if (Document* doc = owner->GetExtantDoc()) {
      Maybe<CSSIntSize> deviceSize =
          nsGlobalWindowOuter::GetRDMDeviceSize(*doc);
      if (deviceSize.isSome()) {
        const CSSIntSize& size = deviceSize.value();
        return {0, 0, size.width, size.height};
      }
    }
  }

  nsDeviceContext* context = GetDeviceContext();
  if (NS_WARN_IF(!context)) {
    return {};
  }

  nsRect r;
  context->GetClientRect(r);
  return CSSIntRect::FromAppUnitsRounded(r);
}

uint16_t nsScreen::GetOrientationAngle() const {
  nsDeviceContext* context = GetDeviceContext();
  if (context) {
    return context->GetScreenOrientationAngle();
  }
  RefPtr<widget::Screen> s =
      widget::ScreenManager::GetSingleton().GetPrimaryScreen();
  return s->GetOrientationAngle();
}

hal::ScreenOrientation nsScreen::GetOrientationType() const {
  nsDeviceContext* context = GetDeviceContext();
  if (context) {
    return context->GetScreenOrientationType();
  }
  RefPtr<widget::Screen> s =
      widget::ScreenManager::GetSingleton().GetPrimaryScreen();
  return s->GetOrientationType();
}

ScreenOrientation* nsScreen::Orientation() const { return mScreenOrientation; }

void nsScreen::GetMozOrientation(nsString& aOrientation,
                                 CallerType aCallerType) const {
  switch (mScreenOrientation->DeviceType(aCallerType)) {
    case OrientationType::Portrait_primary:
      aOrientation.AssignLiteral("portrait-primary");
      break;
    case OrientationType::Portrait_secondary:
      aOrientation.AssignLiteral("portrait-secondary");
      break;
    case OrientationType::Landscape_primary:
      aOrientation.AssignLiteral("landscape-primary");
      break;
    case OrientationType::Landscape_secondary:
      aOrientation.AssignLiteral("landscape-secondary");
      break;
    default:
      MOZ_CRASH("Unacceptable screen orientation type.");
  }
}

/* virtual */
JSObject* nsScreen::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return Screen_Binding::Wrap(aCx, this, aGivenProto);
}

CSSIntRect nsScreen::GetWindowInnerRect() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return {};
  }
  double width;
  double height;
  if (NS_FAILED(win->GetInnerWidth(&width)) ||
      NS_FAILED(win->GetInnerHeight(&height))) {
    return {};
  }
  return {0, 0, int32_t(std::round(width)), int32_t(std::round(height))};
}

bool nsScreen::ShouldResistFingerprinting(RFPTarget aTarget) const {
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  return owner &&
         nsGlobalWindowInner::Cast(owner)->ShouldResistFingerprinting(aTarget);
}
