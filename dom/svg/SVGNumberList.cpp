/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGNumberList.h"

#include "mozilla/ArrayUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsTextFormatter.h"
#include "SVGContentUtils.h"

namespace mozilla {

nsresult SVGNumberList::CopyFrom(const SVGNumberList& rhs) {
  if (!mNumbers.Assign(rhs.mNumbers, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void SVGNumberList::GetValueAsString(nsAString& aValue) const {
  aValue.Truncate();
  char16_t buf[24];
  uint32_t last = mNumbers.Length() - 1;
  for (uint32_t i = 0; i < mNumbers.Length(); ++i) {
    // Would like to use aValue.AppendPrintf("%f", mNumbers[i]), but it's not
    // possible to always avoid trailing zeros.
    nsTextFormatter::snprintf(buf, ArrayLength(buf), u"%g",
                              double(mNumbers[i]));
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(buf);
    if (i != last) {
      aValue.Append(' ');
    }
  }
}

nsresult SVGNumberList::SetValueFromString(const nsAString& aValue) {
  SVGNumberList temp;

  nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace,
                                   nsTokenizerFlags::SeparatorOptional>
      tokenizer(aValue, ',');

  while (tokenizer.hasMoreTokens()) {
    float num;
    if (!SVGContentUtils::ParseNumber(tokenizer.nextToken(), num)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    if (!temp.AppendItem(num)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  if (tokenizer.separatorAfterCurrentToken()) {
    return NS_ERROR_DOM_SYNTAX_ERR;  // trailing comma
  }
  return CopyFrom(temp);
}

}  // namespace mozilla
