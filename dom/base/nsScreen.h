/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsScreen_h___
#define nsScreen_h___

#include "mozilla/Attributes.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsIDOMScreen.h"
#include "nsCOMPtr.h"
#include "nsRect.h"

class nsDeviceContext;

// Script "screen" object
class nsScreen : public mozilla::DOMEventTargetHelper
               , public nsIDOMScreen
{
  typedef mozilla::ErrorResult ErrorResult;
public:
  static already_AddRefed<nsScreen> Create(nsPIDOMWindowInner* aWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSCREEN
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsScreen, mozilla::DOMEventTargetHelper)
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(mozilla::DOMEventTargetHelper)

  nsPIDOMWindowInner* GetParentObject() const
  {
    return GetOwner();
  }

  nsPIDOMWindowOuter* GetOuter() const;

  int32_t GetTop(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetRect(rect);
    return rect.y;
  }

  int32_t GetLeft(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetRect(rect);
    return rect.x;
  }

  int32_t GetWidth(ErrorResult& aRv)
  {
    nsRect rect;
    if (IsDeviceSizePageSize()) {
      if (nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner()) {
        int32_t innerWidth = 0;
        aRv = owner->GetInnerWidth(&innerWidth);
        return innerWidth;
      }
    }

    aRv = GetRect(rect);
    return rect.width;
  }

  int32_t GetHeight(ErrorResult& aRv)
  {
    nsRect rect;
    if (IsDeviceSizePageSize()) {
      if (nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner()) {
        int32_t innerHeight = 0;
        aRv = owner->GetInnerHeight(&innerHeight);
        return innerHeight;
      }
    }

    aRv = GetRect(rect);
    return rect.height;
  }

  int32_t GetPixelDepth(ErrorResult& aRv);
  int32_t GetColorDepth(ErrorResult& aRv)
  {
    return GetPixelDepth(aRv);
  }

  int32_t GetAvailTop(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.y;
  }

  int32_t GetAvailLeft(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.x;
  }

  int32_t GetAvailWidth(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.width;
  }

  int32_t GetAvailHeight(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.height;
  }

  // Deprecated
  void GetMozOrientation(nsString& aOrientation,
                         mozilla::dom::CallerType aCallerType) const;

  IMPL_EVENT_HANDLER(mozorientationchange)

  bool MozLockOrientation(const nsAString& aOrientation, ErrorResult& aRv);
  bool MozLockOrientation(const mozilla::dom::Sequence<nsString>& aOrientations, ErrorResult& aRv);
  void MozUnlockOrientation();

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  mozilla::dom::ScreenOrientation* Orientation() const;

protected:
  nsDeviceContext* GetDeviceContext();
  nsresult GetRect(nsRect& aRect);
  nsresult GetAvailRect(nsRect& aRect);
  nsresult GetWindowInnerRect(nsRect& aRect);

private:
  explicit nsScreen(nsPIDOMWindowInner* aWindow);
  virtual ~nsScreen();

  bool IsDeviceSizePageSize();

  bool ShouldResistFingerprinting() const;

  RefPtr<mozilla::dom::ScreenOrientation> mScreenOrientation;
};

#endif /* nsScreen_h___ */
