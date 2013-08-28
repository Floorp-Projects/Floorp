/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMWindowResizeEventDetail.h"
#include "nsDOMClassInfoID.h" // DOMCI_DATA

DOMCI_DATA(DOMWindowResizeEventDetail, nsDOMWindowResizeEventDetail)

NS_INTERFACE_MAP_BEGIN(nsDOMWindowResizeEventDetail)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMWindowResizeEventDetail)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMWindowResizeEventDetail)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMWindowResizeEventDetail)
NS_IMPL_RELEASE(nsDOMWindowResizeEventDetail)

NS_IMETHODIMP
nsDOMWindowResizeEventDetail::GetWidth(int32_t* aOut)
{
  *aOut = mSize.width;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowResizeEventDetail::GetHeight(int32_t* aOut)
{
  *aOut = mSize.height;
  return NS_OK;
}
