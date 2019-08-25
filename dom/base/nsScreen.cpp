/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsScreen.h"
#include "mozilla/dom/Document.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIDocShellTreeItem.h"
#include "nsLayoutUtils.h"
#include "nsJSUtils.h"
#include "nsDeviceContext.h"

using namespace mozilla;
using namespace mozilla::dom;

/* static */
already_AddRefed<nsScreen> nsScreen::Create(nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(aWindow);

  if (!aWindow->GetDocShell()) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sgo, nullptr);

  RefPtr<nsScreen> screen = new nsScreen(aWindow);
  return screen.forget();
}

nsScreen::nsScreen(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mScreenOrientation(new ScreenOrientation(aWindow, this)) {}

nsScreen::~nsScreen() {}

// QueryInterface implementation for nsScreen
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsScreen)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsScreen, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsScreen, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsScreen, DOMEventTargetHelper,
                                   mScreenOrientation)

int32_t nsScreen::GetPixelDepth(ErrorResult& aRv) {
  // Return 24 to prevent fingerprinting.
  if (ShouldResistFingerprinting()) {
    return 24;
  }

  nsDeviceContext* context = GetDeviceContext();

  if (!context) {
    aRv.Throw(NS_ERROR_FAILURE);
    return -1;
  }

  uint32_t depth;
  context->GetDepth(depth);
  return depth;
}

nsPIDOMWindowOuter* nsScreen::GetOuter() const {
  if (nsPIDOMWindowInner* inner = GetOwner()) {
    return inner->GetOuterWindow();
  }

  return nullptr;
}

nsDeviceContext* nsScreen::GetDeviceContext() {
  return nsLayoutUtils::GetDeviceContextForScreenInfo(GetOuter());
}

nsresult nsScreen::GetRect(nsRect& aRect) {
  // Return window inner rect to prevent fingerprinting.
  if (ShouldResistFingerprinting()) {
    return GetWindowInnerRect(aRect);
  }

  // Here we manipulate the value of aRect to represent the screen size,
  // if in RDM.
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (owner) {
    mozilla::dom::Document* doc = owner->GetExtantDoc();
    if (doc) {
      Maybe<mozilla::CSSIntSize> deviceSize =
          nsGlobalWindowOuter::GetRDMDeviceSize(*doc);
      if (deviceSize.isSome()) {
        const mozilla::CSSIntSize& size = deviceSize.value();
        aRect.SetRect(0, 0, size.width, size.height);
        return NS_OK;
      }
    }
  }

  nsDeviceContext* context = GetDeviceContext();

  if (!context) {
    return NS_ERROR_FAILURE;
  }

  context->GetRect(aRect);
  LayoutDevicePoint screenTopLeftDev = LayoutDevicePixel::FromAppUnits(
      aRect.TopLeft(), context->AppUnitsPerDevPixel());
  DesktopPoint screenTopLeftDesk =
      screenTopLeftDev / context->GetDesktopToDeviceScale();

  aRect.x = NSToIntRound(screenTopLeftDesk.x);
  aRect.y = NSToIntRound(screenTopLeftDesk.y);

  aRect.SetHeight(nsPresContext::AppUnitsToIntCSSPixels(aRect.Height()));
  aRect.SetWidth(nsPresContext::AppUnitsToIntCSSPixels(aRect.Width()));

  return NS_OK;
}

nsresult nsScreen::GetAvailRect(nsRect& aRect) {
  // Return window inner rect to prevent fingerprinting.
  if (ShouldResistFingerprinting()) {
    return GetWindowInnerRect(aRect);
  }

  // Here we manipulate the value of aRect to represent the screen size,
  // if in RDM.
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (owner) {
    mozilla::dom::Document* doc = owner->GetExtantDoc();
    if (doc) {
      Maybe<mozilla::CSSIntSize> deviceSize =
          nsGlobalWindowOuter::GetRDMDeviceSize(*doc);
      if (deviceSize.isSome()) {
        const mozilla::CSSIntSize& size = deviceSize.value();
        aRect.SetRect(0, 0, size.width, size.height);
        return NS_OK;
      }
    }
  }

  nsDeviceContext* context = GetDeviceContext();

  if (!context) {
    return NS_ERROR_FAILURE;
  }

  nsRect r;
  context->GetRect(r);
  LayoutDevicePoint screenTopLeftDev = LayoutDevicePixel::FromAppUnits(
      r.TopLeft(), context->AppUnitsPerDevPixel());
  DesktopPoint screenTopLeftDesk =
      screenTopLeftDev / context->GetDesktopToDeviceScale();

  context->GetClientRect(aRect);

  aRect.x = NSToIntRound(screenTopLeftDesk.x) +
            nsPresContext::AppUnitsToIntCSSPixels(aRect.x - r.x);
  aRect.y = NSToIntRound(screenTopLeftDesk.y) +
            nsPresContext::AppUnitsToIntCSSPixels(aRect.y - r.y);

  aRect.SetHeight(nsPresContext::AppUnitsToIntCSSPixels(aRect.Height()));
  aRect.SetWidth(nsPresContext::AppUnitsToIntCSSPixels(aRect.Width()));

  return NS_OK;
}

mozilla::dom::ScreenOrientation* nsScreen::Orientation() const {
  return mScreenOrientation;
}

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

static void UpdateDocShellOrientationLock(nsPIDOMWindowInner* aWindow,
                                          hal::ScreenOrientation aOrientation) {
  if (!aWindow) {
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeItem> root;
  docShell->GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIDocShell> rootShell(do_QueryInterface(root));
  if (!rootShell) {
    return;
  }

  rootShell->SetOrientationLock(aOrientation);
}

bool nsScreen::MozLockOrientation(const nsAString& aOrientation,
                                  ErrorResult& aRv) {
  nsString orientation(aOrientation);
  Sequence<nsString> orientations;
  if (!orientations.AppendElement(orientation, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return false;
  }
  return MozLockOrientation(orientations, aRv);
}

bool nsScreen::MozLockOrientation(const Sequence<nsString>& aOrientations,
                                  ErrorResult& aRv) {
  if (ShouldResistFingerprinting()) {
    return false;
  }
  hal::ScreenOrientation orientation = hal::eScreenOrientation_None;

  for (uint32_t i = 0; i < aOrientations.Length(); ++i) {
    const nsString& item = aOrientations[i];

    if (item.EqualsLiteral("portrait")) {
      orientation |= hal::eScreenOrientation_PortraitPrimary |
                     hal::eScreenOrientation_PortraitSecondary;
    } else if (item.EqualsLiteral("portrait-primary")) {
      orientation |= hal::eScreenOrientation_PortraitPrimary;
    } else if (item.EqualsLiteral("portrait-secondary")) {
      orientation |= hal::eScreenOrientation_PortraitSecondary;
    } else if (item.EqualsLiteral("landscape")) {
      orientation |= hal::eScreenOrientation_LandscapePrimary |
                     hal::eScreenOrientation_LandscapeSecondary;
    } else if (item.EqualsLiteral("landscape-primary")) {
      orientation |= hal::eScreenOrientation_LandscapePrimary;
    } else if (item.EqualsLiteral("landscape-secondary")) {
      orientation |= hal::eScreenOrientation_LandscapeSecondary;
    } else if (item.EqualsLiteral("default")) {
      orientation |= hal::eScreenOrientation_Default;
    } else {
      // If we don't recognize the token, we should just return 'false'
      // without throwing.
      return false;
    }
  }

  switch (mScreenOrientation->GetLockOrientationPermission(false)) {
    case ScreenOrientation::LOCK_DENIED:
      return false;
    case ScreenOrientation::LOCK_ALLOWED:
      UpdateDocShellOrientationLock(GetOwner(), orientation);
      return mScreenOrientation->LockDeviceOrientation(orientation, false, aRv);
    case ScreenOrientation::FULLSCREEN_LOCK_ALLOWED:
      UpdateDocShellOrientationLock(GetOwner(), orientation);
      return mScreenOrientation->LockDeviceOrientation(orientation, true, aRv);
  }

  // This is only for compilers that don't understand that the previous switch
  // will always return.
  MOZ_CRASH("unexpected lock orientation permission value");
}

void nsScreen::MozUnlockOrientation() {
  if (ShouldResistFingerprinting()) {
    return;
  }
  UpdateDocShellOrientationLock(GetOwner(), hal::eScreenOrientation_None);
  mScreenOrientation->UnlockDeviceOrientation();
}

/* virtual */
JSObject* nsScreen::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return Screen_Binding::Wrap(aCx, this, aGivenProto);
}

nsresult nsScreen::GetWindowInnerRect(nsRect& aRect) {
  aRect.x = 0;
  aRect.y = 0;
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = win->GetInnerWidth(&aRect.width);
  NS_ENSURE_SUCCESS(rv, rv);
  return win->GetInnerHeight(&aRect.height);
}

bool nsScreen::ShouldResistFingerprinting() const {
  bool resist = false;
  nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
  if (owner) {
    resist = nsContentUtils::ShouldResistFingerprinting(owner->GetDocShell());
  }
  return resist;
}
