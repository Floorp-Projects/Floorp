/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#ifndef nsDOMCSSRGBColor_h__
#define nsDOMCSSRGBColor_h__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsROCSSPrimitiveValue;

class nsDOMCSSRGBColor : public nsISupports,
                         public nsWrapperCache
{
public:
  nsDOMCSSRGBColor(nsROCSSPrimitiveValue* aRed,
                   nsROCSSPrimitiveValue* aGreen,
                   nsROCSSPrimitiveValue* aBlue,
                   nsROCSSPrimitiveValue* aAlpha,
                   bool aHasAlpha);

  virtual ~nsDOMCSSRGBColor(void);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMCSSRGBColor)

  bool HasAlpha() const { return mHasAlpha; }

  // RGBColor webidl interface
  nsROCSSPrimitiveValue* Red() const
  {
    return mRed;
  }
  nsROCSSPrimitiveValue* Green() const
  {
    return mGreen;
  }
  nsROCSSPrimitiveValue* Blue() const
  {
    return mBlue;
  }
  nsROCSSPrimitiveValue* Alpha() const
  {
    return mAlpha;
  }

  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  virtual JSObject *WrapObject(JSContext *cx, JSObject *aScope, bool *aTried)
    MOZ_OVERRIDE MOZ_FINAL;

private:
  nsRefPtr<nsROCSSPrimitiveValue> mRed;
  nsRefPtr<nsROCSSPrimitiveValue> mGreen;
  nsRefPtr<nsROCSSPrimitiveValue> mBlue;
  nsRefPtr<nsROCSSPrimitiveValue> mAlpha;
  bool mHasAlpha;
};

#endif // nsDOMCSSRGBColor_h__
