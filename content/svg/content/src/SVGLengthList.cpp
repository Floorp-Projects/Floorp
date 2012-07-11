/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGLengthList.h"
#include "SVGAnimatedLengthList.h"
#include "SVGLength.h"
#include "nsSVGElement.h"
#include "nsDOMError.h"
#include "nsString.h"
#include "nsSVGUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "string.h"

namespace mozilla {

nsresult
SVGLengthList::CopyFrom(const SVGLengthList& rhs)
{
  if (!mLengths.SetCapacity(rhs.Length())) {
    // Yes, we do want fallible alloc here
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mLengths = rhs.mLengths;
  return NS_OK;
}

void
SVGLengthList::GetValueAsString(nsAString& aValue) const
{
  aValue.Truncate();
  PRUint32 last = mLengths.Length() - 1;
  for (PRUint32 i = 0; i < mLengths.Length(); ++i) {
    nsAutoString length;
    mLengths[i].GetValueAsString(length);
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(length);
    if (i != last) {
      aValue.Append(' ');
    }
  }
}

static inline char* SkipWhitespace(char* str)
{
  while (IsSVGWhitespace(*str))
    ++str;
  return str;
}

nsresult
SVGLengthList::SetValueFromString(const nsAString& aValue)
{
  SVGLengthList temp;

  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
    tokenizer(aValue, ',', nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);

  nsCAutoString str;  // outside loop to minimize memory churn

  while (tokenizer.hasMoreTokens()) {
    SVGLength length;
    if (!length.SetValueFromString(tokenizer.nextToken())) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    if (!temp.AppendItem(length)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  if (tokenizer.lastTokenEndedWithSeparator()) {
    return NS_ERROR_DOM_SYNTAX_ERR; // trailing comma
  }
  return CopyFrom(temp);
}

bool
SVGLengthList::operator==(const SVGLengthList& rhs) const
{
  if (Length() != rhs.Length()) {
    return false;
  }
  for (PRUint32 i = 0; i < Length(); ++i) {
    if (!(mLengths[i] == rhs.mLengths[i])) {
      return false;
    }
  }
  return true;
}

} // namespace mozilla
