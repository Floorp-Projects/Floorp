/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A wrapper to contain either an nsAttrValue or an nsAString. This is useful
 * because constructing an nsAttrValue from an nsAString can be expensive when
 * the buffer of the string is not shared.
 *
 * This treats nsAttrValueOrString(nullptr) as the empty string,
 * to help with contexts where a null pointer denotes an empty value.
 *
 * Since a raw pointer to the passed-in string is kept, this class should only
 * be used on the stack.
 */

#ifndef nsAttrValueOrString_h___
#define nsAttrValueOrString_h___

#include "nsString.h"
#include "nsAttrValue.h"

class MOZ_STACK_CLASS nsAttrValueOrString {
 public:
  explicit nsAttrValueOrString(const nsAString& aValue)
      : mAttrValue(nullptr), mStringPtr(&aValue), mCheapString(nullptr) {}

  explicit nsAttrValueOrString(const nsAString* aValue)
      : mAttrValue(nullptr), mStringPtr(aValue), mCheapString(nullptr) {}

  explicit nsAttrValueOrString(const nsAttrValue& aValue)
      : mAttrValue(&aValue), mStringPtr(nullptr), mCheapString(nullptr) {}

  explicit nsAttrValueOrString(const nsAttrValue* aValue)
      : mAttrValue(aValue), mStringPtr(nullptr), mCheapString(nullptr) {}

  void ResetToAttrValue(const nsAttrValue& aValue) {
    mAttrValue = &aValue;
    mStringPtr = nullptr;
    // No need to touch mCheapString here.  If we need to use it, we will reset
    // it to the rigthe value anyway.
  }

  /**
   * Returns a reference to the string value of the contents of this object.
   *
   * When this object points to a string or an nsAttrValue of string or atom
   * type this should be fairly cheap. Other nsAttrValue types will be
   * serialized the first time this is called and cached from thereon.
   */
  const nsAString& String() const;

  /**
   * Compares the string representation of this object with the string
   * representation of an nsAttrValue.
   */
  bool EqualsAsStrings(const nsAttrValue& aOther) const {
    if (mStringPtr) {
      return aOther.Equals(*mStringPtr, eCaseMatters);
    }
    return aOther.EqualsAsStrings(*mAttrValue);
  }

  /*
   * Returns true if the value stored is empty
   */
  bool IsEmpty() const {
    if (mStringPtr) {
      return mStringPtr->IsEmpty();
    }
    if (mAttrValue) {
      return mAttrValue->IsEmptyString();
    }
    return true;
  }

 protected:
  const nsAttrValue* mAttrValue;
  mutable const nsAString* mStringPtr;
  mutable nsCheapString mCheapString;
};

#endif  // nsAttrValueOrString_h___
