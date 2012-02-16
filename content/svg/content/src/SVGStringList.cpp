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
 * The Initial Developer of the Original Code is
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include "mozilla/Util.h"

#include "SVGStringList.h"
#include "nsSVGElement.h"
#include "nsDOMError.h"
#include "nsString.h"
#include "nsSVGUtils.h"
#include "nsTextFormatter.h"
#include "nsWhitespaceTokenizer.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsMathUtils.h"

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
  PRUint32 last = mStrings.Length() - 1;
  for (PRUint32 i = 0; i < mStrings.Length(); ++i) {
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
    if (tokenizer.lastTokenEndedWithSeparator()) {
      return NS_ERROR_DOM_SYNTAX_ERR; // trailing comma
    }
  } else {
    nsWhitespaceTokenizer tokenizer(aValue);

    while (tokenizer.hasMoreTokens()) {
      if (!temp.AppendItem(tokenizer.nextToken())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return CopyFrom(temp);
}

} // namespace mozilla
