/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#ifndef nsDOMCSSRGBColor_h__
#define nsDOMCSSRGBColor_h__

#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"

class nsROCSSPrimitiveValue;

class nsDOMCSSRGBColor : public nsWrapperCache
{
public:
  nsDOMCSSRGBColor(nsROCSSPrimitiveValue* aRed,
                   nsROCSSPrimitiveValue* aGreen,
                   nsROCSSPrimitiveValue* aBlue,
                   nsROCSSPrimitiveValue* aAlpha,
                   bool aHasAlpha);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsDOMCSSRGBColor)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(nsDOMCSSRGBColor)

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

  virtual JSObject *WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
    override final;

private:
  virtual ~nsDOMCSSRGBColor(void);

  RefPtr<nsROCSSPrimitiveValue> mRed;
  RefPtr<nsROCSSPrimitiveValue> mGreen;
  RefPtr<nsROCSSPrimitiveValue> mBlue;
  RefPtr<nsROCSSPrimitiveValue> mAlpha;
  bool mHasAlpha;
};

#endif // nsDOMCSSRGBColor_h__
