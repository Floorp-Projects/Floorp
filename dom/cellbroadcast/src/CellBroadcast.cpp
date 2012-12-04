/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CellBroadcast.h"
#include "nsDOMClassInfo.h"

DOMCI_DATA(MozCellBroadcast, mozilla::dom::CellBroadcast)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(CellBroadcast)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CellBroadcast,
                                                  nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CellBroadcast,
                                                nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CellBroadcast)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozCellBroadcast)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozCellBroadcast)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozCellBroadcast)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(CellBroadcast, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(CellBroadcast, nsDOMEventTargetHelper)

CellBroadcast::CellBroadcast(nsPIDOMWindow *aWindow)
{
  BindToOwner(aWindow);
}

NS_IMPL_EVENT_HANDLER(CellBroadcast, received)

} // namespace dom
} // namespace mozilla

nsresult
NS_NewCellBroadcast(nsPIDOMWindow* aWindow,
                    nsIDOMMozCellBroadcast** aCellBroadcast)
{
  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
    aWindow :
    aWindow->GetCurrentInnerWindow();

  nsRefPtr<mozilla::dom::CellBroadcast> cb =
    new mozilla::dom::CellBroadcast(innerWindow);
  cb.forget(aCellBroadcast);

  return NS_OK;
}
