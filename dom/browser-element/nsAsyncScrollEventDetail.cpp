/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAsyncScrollEventDetail.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMClassInfo.h"
#include "nsIClassInfo.h"
#include "nsDOMClassInfo.h"

NS_IMPL_ADDREF(nsAsyncScrollEventDetail)
NS_IMPL_RELEASE(nsAsyncScrollEventDetail)
NS_INTERFACE_MAP_BEGIN(nsAsyncScrollEventDetail)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncScrollEventDetail)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(AsyncScrollEventDetail)
NS_INTERFACE_MAP_END

DOMCI_DATA(AsyncScrollEventDetail, nsAsyncScrollEventDetail)

/* readonly attribute float top; */
NS_IMETHODIMP nsAsyncScrollEventDetail::GetTop(float *aTop)
{
  *aTop = mTop;
  return NS_OK;
}

/* readonly attribute float left; */
NS_IMETHODIMP nsAsyncScrollEventDetail::GetLeft(float *aLeft)
{
  *aLeft = mLeft;
  return NS_OK;
}

/* readonly attribute float width; */
NS_IMETHODIMP nsAsyncScrollEventDetail::GetWidth(float *aWidth)
{
  *aWidth = mWidth;
  return NS_OK;
}

/* readonly attribute float height; */
NS_IMETHODIMP nsAsyncScrollEventDetail::GetHeight(float *aHeight)
{
  *aHeight = mHeight;
  return NS_OK;
}

/* readonly attribute float scrollWidth; */
NS_IMETHODIMP nsAsyncScrollEventDetail::GetScrollWidth(float *aScrollWidth)
{
  *aScrollWidth = mScrollWidth;
  return NS_OK;
}

/* readonly attribute float scrollHeight; */
NS_IMETHODIMP nsAsyncScrollEventDetail::GetScrollHeight(float *aScrollHeight)
{
  *aScrollHeight = mScrollHeight;
  return NS_OK;
}

