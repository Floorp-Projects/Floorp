/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPaintRequest.h"

#include "nsIFrame.h"
#include "mozilla/dom/PaintRequestBinding.h"
#include "mozilla/dom/PaintRequestListBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPaintRequest, mParent)

NS_INTERFACE_TABLE_HEAD(nsPaintRequest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(nsPaintRequest, nsIDOMPaintRequest)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsPaintRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPaintRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPaintRequest)

/* virtual */ JSObject*
nsPaintRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return PaintRequestBinding::Wrap(aCx, aScope, this);
}

already_AddRefed<nsClientRect>
nsPaintRequest::ClientRect()
{
  nsRefPtr<nsClientRect> clientRect = new nsClientRect(this);
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPaintRequestList, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPaintRequestList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPaintRequestList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPaintRequestList)

JSObject*
nsPaintRequestList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return PaintRequestListBinding::Wrap(aCx, aScope, this);
}
