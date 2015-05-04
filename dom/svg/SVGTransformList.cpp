/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGTransformList.h"
#include "SVGTransformListParser.h"
#include "nsString.h"
#include "nsError.h"

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

  result = mItems[0].GetMatrix();

  if (mItems.Length() == 1)
    return result;

  for (uint32_t i = 1; i < mItems.Length(); ++i) {
    result.PreMultiply(mItems[i].GetMatrix());
  }

  return result;
}

nsresult
SVGTransformList::CopyFrom(const SVGTransformList& rhs)
{
  return CopyFrom(rhs.mItems);
}

nsresult
SVGTransformList::CopyFrom(const nsTArray<nsSVGTransform>& aTransformArray)
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
  uint32_t last = mItems.Length() - 1;
  for (uint32_t i = 0; i < mItems.Length(); ++i) {
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
  SVGTransformListParser parser(aValue);
  if (!parser.Parse()) {
    // there was a parse error.
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  return CopyFrom(parser.GetTransformList());
}

} // namespace mozilla
