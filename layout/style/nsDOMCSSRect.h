/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing rectangle values in DOM computed style */

#ifndef nsDOMCSSRect_h_
#define nsDOMCSSRect_h_

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"

class nsROCSSPrimitiveValue;

class nsDOMCSSRect final : public mozilla::RefCounted<nsDOMCSSRect>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(nsDOMCSSRect);

  nsDOMCSSRect(nsROCSSPrimitiveValue* aTop,
               nsROCSSPrimitiveValue* aRight,
               nsROCSSPrimitiveValue* aBottom,
               nsROCSSPrimitiveValue* aLeft);

  nsROCSSPrimitiveValue* Top() const { return mTop; }
  nsROCSSPrimitiveValue* Right() const { return mRight; }
  nsROCSSPrimitiveValue* Bottom() const { return mBottom; }
  nsROCSSPrimitiveValue* Left() const { return mLeft; }

  ~nsDOMCSSRect();

private:
  RefPtr<nsROCSSPrimitiveValue> mTop;
  RefPtr<nsROCSSPrimitiveValue> mRight;
  RefPtr<nsROCSSPrimitiveValue> mBottom;
  RefPtr<nsROCSSPrimitiveValue> mLeft;
};

#endif /* nsDOMCSSRect_h_ */
