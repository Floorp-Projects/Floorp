/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClientRect.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"

#include "nsPresContext.h"
#include "dombindings.h"

DOMCI_DATA(ClientRect, nsClientRect)

NS_INTERFACE_TABLE_HEAD(nsClientRect)
  NS_INTERFACE_TABLE1(nsClientRect, nsIDOMClientRect)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ClientRect)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsClientRect)
NS_IMPL_RELEASE(nsClientRect)

nsClientRect::nsClientRect()
  : mX(0.0), mY(0.0), mWidth(0.0), mHeight(0.0)
{
}

NS_IMETHODIMP
nsClientRect::GetLeft(float* aResult)
{
  *aResult = mX;
  return NS_OK;
}

NS_IMETHODIMP
nsClientRect::GetTop(float* aResult)
{
  *aResult = mY;
  return NS_OK;
}

NS_IMETHODIMP
nsClientRect::GetRight(float* aResult)
{
  *aResult = mX + mWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsClientRect::GetBottom(float* aResult)
{
  *aResult = mY + mHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsClientRect::GetWidth(float* aResult)
{
  *aResult = mWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsClientRect::GetHeight(float* aResult)
{
  *aResult = mHeight;
  return NS_OK;
}

DOMCI_DATA(ClientRectList, nsClientRectList)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsClientRectList, mParent)

NS_INTERFACE_TABLE_HEAD(nsClientRectList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(nsClientRectList, nsIDOMClientRectList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsClientRectList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ClientRectList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsClientRectList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsClientRectList)


NS_IMETHODIMP    
nsClientRectList::GetLength(PRUint32* aLength)
{
  *aLength = mArray.Count();
  return NS_OK;
}

NS_IMETHODIMP    
nsClientRectList::Item(PRUint32 aIndex, nsIDOMClientRect** aReturn)
{
  NS_IF_ADDREF(*aReturn = nsClientRectList::GetItemAt(aIndex));
  return NS_OK;
}

nsIDOMClientRect*
nsClientRectList::GetItemAt(PRUint32 aIndex)
{
  return mArray.SafeObjectAt(aIndex);
}

JSObject*
nsClientRectList::WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap)
{
  return mozilla::dom::binding::ClientRectList::create(cx, scope, this,
                                                       triedToWrap);
}

static double
RoundFloat(double aValue)
{
  return floor(aValue + 0.5);
}

void
nsClientRect::SetLayoutRect(const nsRect& aLayoutRect)
{
  double scale = 65536.0;
  // Round to the nearest 1/scale units. We choose scale so it can be represented
  // exactly by machine floating point.
  double scaleInv = 1/scale;
  double t2pScaled = scale/nsPresContext::AppUnitsPerCSSPixel();
  double x = RoundFloat(aLayoutRect.x*t2pScaled)*scaleInv;
  double y = RoundFloat(aLayoutRect.y*t2pScaled)*scaleInv;
  SetRect(x, y, RoundFloat(aLayoutRect.XMost()*t2pScaled)*scaleInv - x,
          RoundFloat(aLayoutRect.YMost()*t2pScaled)*scaleInv - y);
}
