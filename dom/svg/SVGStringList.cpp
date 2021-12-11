/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGStringList.h"
#include "nsError.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsWhitespaceTokenizer.h"
#include "SVGContentUtils.h"

namespace mozilla {

nsresult SVGStringList::CopyFrom(const SVGStringList& rhs) {
  if (!mStrings.Assign(rhs.mStrings, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mIsSet = true;
  return NS_OK;
}

void SVGStringList::GetValue(nsAString& aValue) const {
  aValue = StringJoin(mIsCommaSeparated ? u", "_ns : u" "_ns, mStrings);
}

nsresult SVGStringList::SetValue(const nsAString& aValue) {
  SVGStringList temp;

  if (mIsCommaSeparated) {
    nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace>
        tokenizer(aValue, ',');

    while (tokenizer.hasMoreTokens()) {
      if (!temp.AppendItem(tokenizer.nextToken())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    if (tokenizer.separatorAfterCurrentToken()) {
      return NS_ERROR_DOM_SYNTAX_ERR;  // trailing comma
    }
  } else {
    nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tokenizer(
        aValue);

    while (tokenizer.hasMoreTokens()) {
      if (!temp.AppendItem(tokenizer.nextToken())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return CopyFrom(temp);
}

}  // namespace mozilla
