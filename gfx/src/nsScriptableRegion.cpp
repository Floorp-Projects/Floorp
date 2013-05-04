/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptableRegion.h"
#include "nsCOMPtr.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "jsapi.h"

nsScriptableRegion::nsScriptableRegion()
{
}

NS_IMPL_ISUPPORTS1(nsScriptableRegion, nsIScriptableRegion)

NS_IMETHODIMP nsScriptableRegion::Init()
{
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SetToRegion(nsIScriptableRegion *aRegion)
{
  aRegion->GetRegion(&mRegion);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SetToRect(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight)
{
  mRegion = nsIntRect(aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IntersectRegion(nsIScriptableRegion *aRegion)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  mRegion.And(mRegion, region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IntersectRect(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight)
{
  mRegion.And(mRegion, nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::UnionRegion(nsIScriptableRegion *aRegion)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  mRegion.Or(mRegion, region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::UnionRect(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight)
{
  mRegion.Or(mRegion, nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SubtractRegion(nsIScriptableRegion *aRegion)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  mRegion.Sub(mRegion, region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SubtractRect(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight)
{
  mRegion.Sub(mRegion, nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IsEmpty(bool *isEmpty)
{
  *isEmpty = mRegion.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IsEqualRegion(nsIScriptableRegion *aRegion, bool *isEqual)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  *isEqual = mRegion.IsEqual(region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::GetBoundingBox(int32_t *aX, int32_t *aY, int32_t *aWidth, int32_t *aHeight)
{
  nsIntRect boundRect = mRegion.GetBounds();
  *aX = boundRect.x;
  *aY = boundRect.y;
  *aWidth = boundRect.width;
  *aHeight = boundRect.height;
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::Offset(int32_t aXOffset, int32_t aYOffset)
{
  mRegion.MoveBy(aXOffset, aYOffset);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::ContainsRect(int32_t aX, int32_t aY, int32_t aWidth, int32_t aHeight, bool *containsRect)
{
  *containsRect = mRegion.Contains(nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}


NS_IMETHODIMP nsScriptableRegion::GetRegion(nsIntRegion* outRgn)
{
  *outRgn = mRegion;
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::GetRects(JSContext* aCx, JS::Value* aRects)
{
  uint32_t numRects = mRegion.GetNumRects();

  if (!numRects) {
    *aRects = JSVAL_NULL;
    return NS_OK;
  }

  JS::Rooted<JSObject*> destArray(aCx, JS_NewArrayObject(aCx, numRects * 4, nullptr));
  if (!destArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aRects = OBJECT_TO_JSVAL(destArray);

  uint32_t n = 0;
  nsIntRegionRectIterator iter(mRegion);
  const nsIntRect *rect;

  while ((rect = iter.Next())) {
    if (!JS_DefineElement(aCx, destArray, n, INT_TO_JSVAL(rect->x), NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineElement(aCx, destArray, n + 1, INT_TO_JSVAL(rect->y), NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineElement(aCx, destArray, n + 2, INT_TO_JSVAL(rect->width), NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineElement(aCx, destArray, n + 3, INT_TO_JSVAL(rect->height), NULL, NULL, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    n += 4;
  }

  return NS_OK;
}
