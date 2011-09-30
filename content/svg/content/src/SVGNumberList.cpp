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
    nsTextFormatter::snprintf(buf, NS_ARRAY_LENGTH(buf),
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
    if (token == '\0') {
      return NS_ERROR_DOM_SYNTAX_ERR; // nothing between commas
    }
    char *end;
    float num = float(PR_strtod(token, &end));
    if (*end != '\0' || !NS_finite(num)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    temp.AppendItem(num);
  }
  if (tokenizer.lastTokenEndedWithSeparator()) {
    return NS_ERROR_DOM_SYNTAX_ERR; // trailing comma
  }
  return CopyFrom(temp);
}

} // namespace mozilla
