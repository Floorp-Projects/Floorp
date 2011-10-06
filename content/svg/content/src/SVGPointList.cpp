/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "SVGPointList.h"
#include "SVGAnimatedPointList.h"
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
  PRUint32 last = mItems.Length() - 1;
  for (PRUint32 i = 0; i < mItems.Length(); ++i) {
    // Would like to use aValue.AppendPrintf("%f,%f", item.mX, item.mY),
    // but it's not possible to always avoid trailing zeros.
    nsTextFormatter::snprintf(buf, NS_ARRAY_LENGTH(buf),
                              NS_LITERAL_STRING("%g,%g").get(),
                              double(mItems[i].mX), double(mItems[i].mY));
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(buf);
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

  nsCAutoString str1, str2;  // outside loop to minimize memory churn

  while (tokenizer.hasMoreTokens()) {
    CopyUTF16toUTF8(tokenizer.nextToken(), str1);
    const char *token1 = str1.get();
    if (*token1 == '\0') {
      rv = NS_ERROR_DOM_SYNTAX_ERR;
      break;
    }
    char *end;
    float x = float(PR_strtod(token1, &end));
    if (end == token1 || !NS_finite(x)) {
      rv = NS_ERROR_DOM_SYNTAX_ERR;
      break;
    }
    const char *token2;
    if (*end == '-') {
      // It's possible for the token to be 10-30 which has
      // no separator but needs to be parsed as 10, -30
      token2 = end;
    } else {
      if (!tokenizer.hasMoreTokens()) {
        rv = NS_ERROR_DOM_SYNTAX_ERR;
        break;
      }
      CopyUTF16toUTF8(tokenizer.nextToken(), str2);
      token2 = str2.get();
      if (*token2 == '\0') {
        rv = NS_ERROR_DOM_SYNTAX_ERR;
        break;
      }
    }

    float y = float(PR_strtod(token2, &end));
    if (*end != '\0' || !NS_finite(y)) {
      rv = NS_ERROR_DOM_SYNTAX_ERR;
      break;
    }

    temp.AppendItem(SVGPoint(x, y));
  }
  if (tokenizer.lastTokenEndedWithSeparator()) {
    rv = NS_ERROR_DOM_SYNTAX_ERR; // trailing comma
  }
  nsresult rv2 = CopyFrom(temp);
  if (NS_FAILED(rv2)) {
    return rv2; // prioritize OOM error code over syntax errors
  }
  return rv;
}

} // namespace mozilla
