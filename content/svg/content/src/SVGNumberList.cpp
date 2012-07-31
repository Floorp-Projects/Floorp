/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "SVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "nsSVGElement.h"
#include "nsDOMError.h"
#include "nsString.h"
#include "nsSVGUtils.h"
#include "string.h"
#include "prdtoa.h"
#include "nsTextFormatter.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsMathUtils.h"

namespace mozilla {

nsresult
SVGNumberList::CopyFrom(const SVGNumberList& rhs)
{
  if (!mNumbers.SetCapacity(rhs.Length())) {
    // Yes, we do want fallible alloc here
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mNumbers = rhs.mNumbers;
  return NS_OK;
}

void
SVGNumberList::GetValueAsString(nsAString& aValue) const
{
  aValue.Truncate();
  PRUnichar buf[24];
  PRUint32 last = mNumbers.Length() - 1;
  for (PRUint32 i = 0; i < mNumbers.Length(); ++i) {
    // Would like to use aValue.AppendPrintf("%f", mNumbers[i]), but it's not
    // possible to always avoid trailing zeros.
    nsTextFormatter::snprintf(buf, ArrayLength(buf),
                              NS_LITERAL_STRING("%g").get(),
                              double(mNumbers[i]));
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(buf);
    if (i != last) {
      aValue.Append(' ');
    }
  }
}

nsresult
SVGNumberList::SetValueFromString(const nsAString& aValue)
{
  SVGNumberList temp;

  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
    tokenizer(aValue, ',', nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);

  nsCAutoString str;  // outside loop to minimize memory churn

  while (tokenizer.hasMoreTokens()) {
    CopyUTF16toUTF8(tokenizer.nextToken(), str); // NS_ConvertUTF16toUTF8
    const char *token = str.get();
    if (*token == '\0') {
      return NS_ERROR_DOM_SYNTAX_ERR; // nothing between commas
    }
    char *end;
    float num = float(PR_strtod(token, &end));
    if (*end != '\0' || !NS_finite(num)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    if (!temp.AppendItem(num)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  if (tokenizer.lastTokenEndedWithSeparator()) {
    return NS_ERROR_DOM_SYNTAX_ERR; // trailing comma
  }
  return CopyFrom(temp);
}

} // namespace mozilla
