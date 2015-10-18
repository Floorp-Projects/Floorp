/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PaintRequest, mParent)

NS_INTERFACE_TABLE_HEAD(PaintRequest)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(PaintRequest, nsIDOMPaintRequest)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(PaintRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PaintRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PaintRequest)

/* virtual */ JSObject*
PaintRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PaintRequestBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMRect>
PaintRequest::ClientRect()
{
  RefPtr<DOMRect> clientRect = new DOMRect(this);
  clientRect->SetLayoutRect(mRequest.mRect);
  return clientRect.forget();
}

NS_IMETHODIMP
PaintRequest::GetClientRect(nsIDOMClientRect** aResult)
{
  RefPtr<DOMRect> clientRect = ClientRect();
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PaintRequestList, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaintRequestList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PaintRequestList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PaintRequestList)

JSObject*
PaintRequestList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PaintRequestListBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
