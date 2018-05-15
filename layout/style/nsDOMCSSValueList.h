/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing lists of values in DOM computed style */

#ifndef nsDOMCSSValueList_h___
#define nsDOMCSSValueList_h___

#include "CSSValue.h"
#include "nsTArray.h"

class nsDOMCSSValueList final : public mozilla::dom::CSSValue
{
public:
  // nsDOMCSSValueList
  nsDOMCSSValueList(bool aCommaDelimited, bool aReadonly);

  /**
   * Adds a value to this list.
   */
  void AppendCSSValue(already_AddRefed<CSSValue> aValue);

  void GetCssText(nsString& aText, mozilla::ErrorResult& aRv) final;
  void SetCssText(const nsAString& aText, mozilla::ErrorResult& aRv) final;
  uint16_t CssValueType() const final;

  void GetCssText(nsAString& aText);

  CSSValue* IndexedGetter(uint32_t aIdx, bool& aFound) const
  {
    aFound = aIdx <= Length();
    return Item(aIdx);
  }

  CSSValue* Item(uint32_t aIndex) const
  {
    return mCSSValues.SafeElementAt(aIndex);
  }

  uint32_t Length() const
  {
    return mCSSValues.Length();
  }

protected:
  virtual ~nsDOMCSSValueList();

  bool                        mCommaDelimited;  // some value lists use a comma
                                                // as the delimiter, some just use
                                                // spaces.

  bool                        mReadonly;    // Are we read-only?

  InfallibleTArray<RefPtr<CSSValue> > mCSSValues;
};

#endif /* nsDOMCSSValueList_h___ */
