/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PaintRequest.h"

#include "mozilla/dom/PaintRequestBinding.h"
#include "mozilla/dom/PaintRequestListBinding.h"
#include "mozilla/dom/DOMRect.h"

namespace mozilla {
namespace dom {

/******************************************************************************
 * mozilla::dom::PaintRequest
 *****************************************************************************/

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(PaintRequest, mParent)

NS_INTERFACE_TABLE_HEAD(PaintRequest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE1(PaintRequest, nsIDOMPaintRequest)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(PaintRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PaintRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PaintRequest)

/* virtual */ JSObject*
PaintRequest::WrapObject(JSContext* aCx)
{
  return PaintRequestBinding::Wrap(aCx, this);
}

already_AddRefed<DOMRect>
PaintRequest::ClientRect()
{
  nsRefPtr<DOMRect> clientRect = new DOMRect(this);
  clientRect->SetLayoutRect(mRequest.mRect);
  return clientRect.forget();
}

NS_IMETHODIMP
PaintRequest::GetClientRect(nsIDOMClientRect** aResult)
{
  nsRefPtr<DOMRect> clientRect = ClientRect();
  clientRect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
PaintRequest::GetXPCOMReason(nsAString& aResult)
{
  GetReason(aResult);
  return NS_OK;
}

/******************************************************************************
 * mozilla::dom::PaintRequestList
 *****************************************************************************/

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(PaintRequestList, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaintRequestList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PaintRequestList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PaintRequestList)

JSObject*
PaintRequestList::WrapObject(JSContext* aCx)
{
  return PaintRequestListBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
