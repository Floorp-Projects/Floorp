/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsScreen_h___
#define nsScreen_h___

#include "mozilla/Attributes.h"
#include "mozilla/dom/ScreenBinding.h"
#include "mozilla/dom/ScreenLuminance.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsCOMPtr.h"
#include "nsGlobalWindowOuter.h"
#include "mozilla/gfx/Rect.h"
#include "Units.h"

class nsDeviceContext;

namespace mozilla {
enum class RFPTarget : uint64_t;
}

// Script "screen" object
class nsScreen : public mozilla::DOMEventTargetHelper {
  using ErrorResult = mozilla::ErrorResult;

 public:
  static already_AddRefed<nsScreen> Create(nsPIDOMWindowInner* aWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsScreen,
                                           mozilla::DOMEventTargetHelper)

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  nsPIDOMWindowOuter* GetOuter() const;

  int32_t GetTop(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetRect(rect);
    return rect.y;
  }

  int32_t GetLeft(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetRect(rect);
    return rect.x;
  }

  int32_t GetWidth(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetRect(rect);
    return rect.Width();
  }

  int32_t GetHeight(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetRect(rect);
    return rect.Height();
  }

  int32_t GetPixelDepth(ErrorResult& aRv);
  int32_t GetColorDepth(ErrorResult& aRv) { return GetPixelDepth(aRv); }

  int32_t GetAvailTop(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetAvailRect(rect);
    return rect.y;
  }

  int32_t GetAvailLeft(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetAvailRect(rect);
    return rect.x;
  }

  int32_t GetAvailWidth(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetAvailRect(rect);
    return rect.Width();
  }

  int32_t GetAvailHeight(ErrorResult& aRv) {
    mozilla::CSSIntRect rect;
    aRv = GetAvailRect(rect);
    return rect.Height();
  }

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

  bool MozLockOrientation(const nsAString& aOrientation, ErrorResult& aRv);
  bool MozLockOrientation(const mozilla::dom::Sequence<nsString>& aOrientations,
                          ErrorResult& aRv);
  void MozUnlockOrientation();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  mozilla::dom::ScreenOrientation* Orientation() const;
  mozilla::dom::ScreenOrientation* GetOrientationIfExists() const {
    return mScreenOrientation.get();
  }

 protected:
  nsDeviceContext* GetDeviceContext() const;
  nsresult GetRect(mozilla::CSSIntRect& aRect);
  nsresult GetAvailRect(mozilla::CSSIntRect& aRect);
  nsresult GetWindowInnerRect(mozilla::CSSIntRect& aRect);

 private:
  explicit nsScreen(nsPIDOMWindowInner* aWindow);
  virtual ~nsScreen();

  bool ShouldResistFingerprinting(mozilla::RFPTarget aTarget) const;

  mozilla::dom::Document* TopContentDocumentInRDMPane() const;

  RefPtr<mozilla::dom::ScreenOrientation> mScreenOrientation;
};

#endif /* nsScreen_h___ */
