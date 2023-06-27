/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing lists of values in DOM computed style */

#include "nsDOMCSSValueList.h"

#include <utility>

#include "mozilla/ErrorResult.h"
#include "nsString.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMCSSValueList::nsDOMCSSValueList(bool aCommaDelimited)
    : CSSValue(), mCommaDelimited(aCommaDelimited) {}

nsDOMCSSValueList::~nsDOMCSSValueList() = default;

void nsDOMCSSValueList::AppendCSSValue(already_AddRefed<CSSValue> aValue) {
  RefPtr<CSSValue> val = aValue;
  mCSSValues.AppendElement(std::move(val));
}

void nsDOMCSSValueList::GetCssText(nsAString& aCssText) {
  aCssText.Truncate();

  uint32_t count = mCSSValues.Length();

  nsAutoString separator;
  if (mCommaDelimited) {
    separator.AssignLiteral(", ");
  } else {
    separator.Assign(char16_t(' '));
  }

  nsAutoString tmpStr;
  for (uint32_t i = 0; i < count; ++i) {
    CSSValue* cssValue = mCSSValues[i];
    NS_ASSERTION(cssValue,
                 "Eek!  Someone filled the value list with null CSSValues!");
    if (cssValue) {
      cssValue->GetCssText(tmpStr);
      if (tmpStr.IsEmpty()) {
#ifdef DEBUG_caillon
        NS_ERROR("Eek!  An empty CSSValue!  Bad!");
#endif
        continue;
      }
      // If this isn't the first item in the list, then
      // it's ok to append a separator.
      if (!aCssText.IsEmpty()) {
        aCssText.Append(separator);
      }
      aCssText.Append(tmpStr);
    }
  }
}
