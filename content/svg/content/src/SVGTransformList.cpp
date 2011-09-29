/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#include "SVGTransformList.h"
#include "SVGTransformListParser.h"

namespace mozilla {

gfxMatrix
SVGTransformList::GetConsolidationMatrix() const
{
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  gfxMatrix result;

  if (mItems.IsEmpty())
    return result;

  result = mItems[0].Matrix();

  if (mItems.Length() == 1)
    return result;

  for (PRUint32 i = 1; i < mItems.Length(); ++i) {
    result.PreMultiply(mItems[i].Matrix());
  }

  return result;
}

nsresult
SVGTransformList::CopyFrom(const SVGTransformList& rhs)
{
  return CopyFrom(rhs.mItems);
}

nsresult
SVGTransformList::CopyFrom(const nsTArray<SVGTransform>& aTransformArray)
{
  if (!mItems.SetCapacity(aTransformArray.Length())) {
    // Yes, we do want fallible alloc here
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mItems = aTransformArray;
  return NS_OK;
}

void
SVGTransformList::GetValueAsString(nsAString& aValue) const
{
  aValue.Truncate();
  PRUint32 last = mItems.Length() - 1;
  for (PRUint32 i = 0; i < mItems.Length(); ++i) {
    nsAutoString length;
    mItems[i].GetValueAsString(length);
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(length);
    if (i != last) {
      aValue.Append(' ');
    }
  }
}

nsresult
SVGTransformList::SetValueFromString(const nsAString& aValue)
{
  SVGTransformListParser parser;
  nsresult rv = parser.Parse(aValue);

  if (NS_FAILED(rv)) {
    // there was a parse error.
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  return CopyFrom(parser.GetTransformList());
}

} // namespace mozilla
