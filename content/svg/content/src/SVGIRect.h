/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGIRect_h
#define mozilla_dom_SVGIRect_h

#include "nsIDOMSVGRect.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/SVGRectBinding.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class SVGIRect : public nsIDOMSVGRect
{
public:
  JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return SVGRectBinding::Wrap(aCx, aScope, this);
  }

  virtual float X() const = 0;

  NS_IMETHOD GetX(float *aX) MOZ_OVERRIDE MOZ_FINAL
  {
    *aX = X();
    return NS_OK;
  }

  virtual void SetX(float aX, ErrorResult& aRv) = 0;

  NS_IMETHOD SetX(float aX) MOZ_OVERRIDE MOZ_FINAL
  {
    NS_ENSURE_FINITE(aX, NS_ERROR_ILLEGAL_VALUE);

    ErrorResult rv;
    SetX(aX, rv);
    return rv.ErrorCode();
  }

  virtual float Y() const = 0;

  NS_IMETHOD GetY(float *aY) MOZ_OVERRIDE MOZ_FINAL
  {
    *aY = Y();
    return NS_OK;
  }

  virtual void SetY(float aY, ErrorResult& aRv) = 0;

  NS_IMETHOD SetY(float aY) MOZ_OVERRIDE MOZ_FINAL
  {
    NS_ENSURE_FINITE(aY, NS_ERROR_ILLEGAL_VALUE);

    ErrorResult rv;
    SetY(aY, rv);
    return rv.ErrorCode();
  }

  virtual float Width() const = 0;

  NS_IMETHOD GetWidth(float *aWidth) MOZ_OVERRIDE MOZ_FINAL
  {
    *aWidth = Width();
    return NS_OK;
  }

  virtual void SetWidth(float aWidth, ErrorResult& aRv) = 0;

  NS_IMETHOD SetWidth(float aWidth) MOZ_OVERRIDE MOZ_FINAL
  {
    NS_ENSURE_FINITE(aWidth, NS_ERROR_ILLEGAL_VALUE);

    ErrorResult rv;
    SetWidth(aWidth, rv);
    return rv.ErrorCode();
  }

  virtual float Height() const = 0;

  NS_IMETHOD GetHeight(float *aHeight) MOZ_OVERRIDE MOZ_FINAL
  {
    *aHeight = Height();
    return NS_OK;
  }

  virtual void SetHeight(float aHeight, ErrorResult& aRv) = 0;

  NS_IMETHOD SetHeight(float aHeight) MOZ_OVERRIDE MOZ_FINAL
  {
    NS_ENSURE_FINITE(aHeight, NS_ERROR_ILLEGAL_VALUE);

    ErrorResult rv;
    SetHeight(aHeight, rv);
    return rv.ErrorCode();
  }
};

} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_SVGIRect_h

