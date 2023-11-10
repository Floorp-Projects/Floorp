/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsScreen_h___
#define nsScreen_h___

#include "mozilla/dom/ScreenBinding.h"
#include "mozilla/dom/ScreenLuminance.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/StaticPrefs_media.h"
#include "Units.h"

class nsDeviceContext;

namespace mozilla {
enum class RFPTarget : uint64_t;
}

// Script "screen" object
class nsScreen : public mozilla::DOMEventTargetHelper {
 public:
  explicit nsScreen(nsPIDOMWindowInner* aWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsScreen,
                                           mozilla::DOMEventTargetHelper)

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  nsPIDOMWindowOuter* GetOuter() const;

  int32_t Top() { return GetRect().y; }
  int32_t Left() { return GetRect().x; }
  int32_t Width() { return GetRect().Width(); }
  int32_t Height() { return GetRect().Height(); }

  int32_t AvailTop() { return GetAvailRect().y; }
  int32_t AvailLeft() { return GetAvailRect().x; }
  int32_t AvailWidth() { return GetAvailRect().Width(); }
  int32_t AvailHeight() { return GetAvailRect().Height(); }

  int32_t PixelDepth();
  int32_t ColorDepth() { return PixelDepth(); }

  // Media Capabilities extension
  mozilla::dom::ScreenColorGamut ColorGamut() const {
    return mozilla::dom::ScreenColorGamut::Srgb;
  }

  already_AddRefed<mozilla::dom::ScreenLuminance> GetLuminance() const {
    return nullptr;
  }

  static bool MediaCapabilitiesEnabled(JSContext* aCx, JSObject* aGlobal) {
    return mozilla::StaticPrefs::media_media_capabilities_screen_enabled();
  }

  IMPL_EVENT_HANDLER(change);

  uint16_t GetOrientationAngle() const;
  mozilla::hal::ScreenOrientation GetOrientationType() const;

  // Deprecated
  void GetMozOrientation(nsString& aOrientation,
                         mozilla::dom::CallerType aCallerType) const;

  IMPL_EVENT_HANDLER(mozorientationchange)

  // This function is deprecated, use ScreenOrientation API instead.
  bool MozLockOrientation(const nsAString&) { return false; };
  bool MozLockOrientation(const mozilla::dom::Sequence<nsString>&) {
    return false;
  }
  void MozUnlockOrientation() {}

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  mozilla::dom::ScreenOrientation* Orientation() const;
  mozilla::dom::ScreenOrientation* GetOrientationIfExists() const {
    return mScreenOrientation.get();
  }

 protected:
  nsDeviceContext* GetDeviceContext() const;
  mozilla::CSSIntRect GetRect();
  mozilla::CSSIntRect GetAvailRect();
  mozilla::CSSIntRect GetWindowInnerRect();

 private:
  virtual ~nsScreen();

  bool ShouldResistFingerprinting(mozilla::RFPTarget aTarget) const;

  mozilla::dom::Document* TopContentDocumentInRDMPane() const;

  RefPtr<mozilla::dom::ScreenOrientation> mScreenOrientation;
};

#endif /* nsScreen_h___ */
