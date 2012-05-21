/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing rectangle values in DOM computed style */

#ifndef nsDOMCSSRect_h_
#define nsDOMCSSRect_h_

#include "nsISupports.h"
#include "nsIDOMRect.h"
#include "nsCOMPtr.h"
class nsIDOMCSSPrimitiveValue;

class nsDOMCSSRect : public nsIDOMRect {
public:
  nsDOMCSSRect(nsIDOMCSSPrimitiveValue* aTop,
               nsIDOMCSSPrimitiveValue* aRight,
               nsIDOMCSSPrimitiveValue* aBottom,
               nsIDOMCSSPrimitiveValue* aLeft);
  virtual ~nsDOMCSSRect(void);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMRECT

private:
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mTop;
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mRight;
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mBottom;
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mLeft;
};

#endif /* nsDOMCSSRect_h_ */
