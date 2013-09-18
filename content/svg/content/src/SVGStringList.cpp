/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGStringList.h"
#include "nsError.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsString.h"
#include "nsWhitespaceTokenizer.h"
#include "SVGContentUtils.h"

namespace mozilla {

nsresult
SVGStringList::CopyFrom(const SVGStringList& rhs)
{
  if (!mStrings.SetCapacity(rhs.Length())) {
    // Yes, we do want fallible alloc here
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mStrings = rhs.mStrings;
  mIsSet = true;
  return NS_OK;
}

void
SVGStringList::GetValue(nsAString& aValue) const
{
  aValue.Truncate();
  uint32_t last = mStrings.Length() - 1;
  for (uint32_t i = 0; i < mStrings.Length(); ++i) {
    aValue.Append(mStrings[i]);
    if (i != last) {
      if (mIsCommaSeparated) {
        aValue.Append(',');
      }
      aValue.Append(' ');
    }
  }
}

nsresult
SVGStringList::SetValue(const nsAString& aValue)
{
  SVGStringList temp;

  if (mIsCommaSeparated) {
    nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
      tokenizer(aValue, ',');

    while (tokenizer.hasMoreTokens()) {
      if (!temp.AppendItem(tokenizer.nextToken())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    if (tokenizer.separatorAfterCurrentToken()) {
      return NS_ERROR_DOM_SYNTAX_ERR; // trailing comma
    }
  } else {
    nsWhitespaceTokenizerTemplate<IsSVGWhitespace> tokenizer(aValue);

    while (tokenizer.hasMoreTokens()) {
      if (!temp.AppendItem(tokenizer.nextToken())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return CopyFrom(temp);
}

} // namespace mozilla
