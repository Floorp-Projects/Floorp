/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOpenWindowEventDetail.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMClassInfo.h"
#include "nsIClassInfo.h"
#include "nsDOMClassInfo.h"

NS_IMPL_CYCLE_COLLECTION_1(nsOpenWindowEventDetail, mFrameElement)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsOpenWindowEventDetail)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsOpenWindowEventDetail)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsOpenWindowEventDetail)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIOpenWindowEventDetail)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(OpenWindowEventDetail)
NS_INTERFACE_MAP_END

DOMCI_DATA(OpenWindowEventDetail, nsOpenWindowEventDetail)

NS_IMETHODIMP
nsOpenWindowEventDetail::GetUrl(nsAString& aOut)
{
  aOut.Assign(mURL);
  return NS_OK;
}

NS_IMETHODIMP
nsOpenWindowEventDetail::GetName(nsAString& aOut)
{
  aOut.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsOpenWindowEventDetail::GetFeatures(nsAString& aOut)
{
  aOut.Assign(mFeatures);
  return NS_OK;
}

NS_IMETHODIMP
nsOpenWindowEventDetail::GetFrameElement(nsIDOMNode** aOut)
{
  nsCOMPtr<nsIDOMNode> out = mFrameElement;
  out.forget(aOut);
  return NS_OK;
}
