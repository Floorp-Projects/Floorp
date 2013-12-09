/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "SVGPointList.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsTextFormatter.h"
#include "SVGContentUtils.h"

namespace mozilla {

nsresult
SVGPointList::CopyFrom(const SVGPointList& rhs)
{
  if (!SetCapacity(rhs.Length())) {
    // Yes, we do want fallible alloc here
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mItems = rhs.mItems;
  return NS_OK;
}

void
SVGPointList::GetValueAsString(nsAString& aValue) const
{
  aValue.Truncate();
  PRUnichar buf[50];
  uint32_t last = mItems.Length() - 1;
  for (uint32_t i = 0; i < mItems.Length(); ++i) {
    // Would like to use aValue.AppendPrintf("%f,%f", item.mX, item.mY),
    // but it's not possible to always avoid trailing zeros.
    nsTextFormatter::snprintf(buf, ArrayLength(buf),
                              NS_LITERAL_STRING("%g,%g").get(),
                              double(mItems[i].mX), double(mItems[i].mY));
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(buf);
    if (i != last) {
      aValue.Append(' ');
    }
  }
}

nsresult
SVGPointList::SetValueFromString(const nsAString& aValue)
{
  // The spec says that the list is parsed and accepted up to the first error
  // encountered, so we must call CopyFrom even if an error occurs. We still
  // want to throw any error code from setAttribute if there's a problem
  // though, so we must take care to return any error code.

  nsresult rv = NS_OK;

  SVGPointList temp;

  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
    tokenizer(aValue, ',', nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);

  while (tokenizer.hasMoreTokens()) {

    const nsAString& token = tokenizer.nextToken();

    RangedPtr<const PRUnichar> iter =
      SVGContentUtils::GetStartRangedPtr(token);
    const RangedPtr<const PRUnichar> end =
      SVGContentUtils::GetEndRangedPtr(token);

    float x;
    if (!SVGContentUtils::ParseNumber(iter, end, x)) {
      rv = NS_ERROR_DOM_SYNTAX_ERR;
      break;
    }

    float y;
    if (iter == end) {
      if (!tokenizer.hasMoreTokens() ||
          !SVGContentUtils::ParseNumber(tokenizer.nextToken(), y)) {
        rv = NS_ERROR_DOM_SYNTAX_ERR;
        break;
      }
    } else {
      // It's possible for the token to be 10-30 which has
      // no separator but needs to be parsed as 10, -30
      const nsAString& leftOver = Substring(iter.get(), end.get());
      if (leftOver[0] != '-' || !SVGContentUtils::ParseNumber(leftOver, y)) {
        rv = NS_ERROR_DOM_SYNTAX_ERR;
        break;
      }
    }
    temp.AppendItem(SVGPoint(x, y));
  }
  if (tokenizer.separatorAfterCurrentToken()) {
    rv = NS_ERROR_DOM_SYNTAX_ERR; // trailing comma
  }
  nsresult rv2 = CopyFrom(temp);
  if (NS_FAILED(rv2)) {
    return rv2; // prioritize OOM error code over syntax errors
  }
  return rv;
}

} // namespace mozilla
