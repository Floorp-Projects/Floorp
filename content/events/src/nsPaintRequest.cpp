/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPaintRequest.h"

#include "nsDOMClassInfoID.h"
#include "nsIFrame.h"
#include "nsContentUtils.h"
#include "mozilla/dom/PaintRequestBinding.h"
#include "mozilla/dom/PaintRequestListBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

DOMCI_DATA(PaintRequest, nsPaintRequest)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPaintRequest, mParent)

NS_INTERFACE_TABLE_HEAD(nsPaintRequest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(nsPaintRequest, nsIDOMPaintRequest)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsPaintRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PaintRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPaintRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPaintRequest)

/* virtual */ JSObject*
nsPaintRequest::WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
{
  return PaintRequestBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

already_AddRefed<nsClientRect>
nsPaintRequest::ClientRect()
{
  nsRefPtr<nsClientRect> clientRect = new nsClientRect();
  clientRect->SetLayoutRect(mRequest.mRect);
  return clientRect.forget();
}

NS_IMETHODIMP
nsPaintRequest::GetClientRect(nsIDOMClientRect** aResult)
{
  nsRefPtr<nsClientRect> clientRect = ClientRect();
  clientRect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPaintRequest::GetXPCOMReason(nsAString& aResult)
{
  GetReason(aResult);
  return NS_OK;
}

DOMCI_DATA(PaintRequestList, nsPaintRequestList)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPaintRequestList, mParent)

NS_INTERFACE_TABLE_HEAD(nsPaintRequestList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(nsPaintRequestList, nsIDOMPaintRequestList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsPaintRequestList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PaintRequestList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPaintRequestList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPaintRequestList)

JSObject*
nsPaintRequestList::WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap)
{
  return mozilla::dom::PaintRequestListBinding::Wrap(cx, scope, this,
                                                     triedToWrap);
}

NS_IMETHODIMP    
nsPaintRequestList::GetLength(uint32_t* aLength)
{
  *aLength = Length();
  return NS_OK;
}

NS_IMETHODIMP    
nsPaintRequestList::Item(uint32_t aIndex, nsIDOMPaintRequest** aReturn)
{
  NS_IF_ADDREF(*aReturn = Item(aIndex));
  return NS_OK;
}
