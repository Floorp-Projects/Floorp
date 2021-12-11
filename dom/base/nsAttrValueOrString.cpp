/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAttrValueOrString.h"
#include "nsAttrValueInlines.h"

const nsAString& nsAttrValueOrString::String() const {
  if (mStringPtr) {
    return *mStringPtr;
  }

  if (!mAttrValue) {
    mStringPtr = &mCheapString;
    return *mStringPtr;
  }

  if (mAttrValue->Type() == nsAttrValue::eString) {
    mCheapString = mAttrValue->GetStringValue();
    mStringPtr = &mCheapString;
    return *mStringPtr;
  }

  mAttrValue->ToString(mCheapString);
  mStringPtr = &mCheapString;
  return *mStringPtr;
}
