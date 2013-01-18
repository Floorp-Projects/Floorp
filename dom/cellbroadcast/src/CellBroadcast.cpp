/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CellBroadcast.h"
#include "nsIDOMMozCellBroadcastEvent.h"
#include "nsIDOMMozCellBroadcastMessage.h"
#include "mozilla/Services.h"
#include "nsDOMClassInfo.h"
#include "nsRadioInterfaceLayer.h"
#include "GeneratedEvents.h"

DOMCI_DATA(MozCellBroadcast, mozilla::dom::CellBroadcast)

namespace mozilla {
namespace dom {

/**
 * CellBroadcastCallback Implementation.
 */

class CellBroadcastCallback : public nsIRILCellBroadcastCallback
{
private:
  CellBroadcast* mCellBroadcast;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIRILCELLBROADCASTCALLBACK(mCellBroadcast->)

  CellBroadcastCallback(CellBroadcast* aCellBroadcast);
};

NS_IMPL_ISUPPORTS1(CellBroadcastCallback, nsIRILCellBroadcastCallback)

CellBroadcastCallback::CellBroadcastCallback(CellBroadcast* aCellBroadcast)
  : mCellBroadcast(aCellBroadcast)
{
  MOZ_ASSERT(mCellBroadcast, "Null pointer!");
}

/**
 * CellBroadcast Implementation.
 */

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

CellBroadcast::CellBroadcast(nsPIDOMWindow *aWindow,
                             nsIRILContentHelper *aRIL)
  : mRIL(aRIL)
{
  BindToOwner(aWindow);

  mCallback = new CellBroadcastCallback(this);

  nsresult rv = mRIL->RegisterCellBroadcastCallback(mCallback);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Failed registering Cell Broadcast callback with RIL");

  rv = mRIL->RegisterCellBroadcastMsg();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Failed registering Cell Broadcast callback with RIL");
}

CellBroadcast::~CellBroadcast()
{
  MOZ_ASSERT(mRIL && mCallback, "Null pointer!");

  mRIL->UnregisterCellBroadcastCallback(mCallback);
}

NS_IMPL_EVENT_HANDLER(CellBroadcast, received)

// Forwarded nsIRILCellBroadcastCallback methods

NS_IMETHODIMP
CellBroadcast::NotifyMessageReceived(nsIDOMMozCellBroadcastMessage* aMessage)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMMozCellBroadcastEvent(getter_AddRefs(event), nullptr, nullptr);

  nsCOMPtr<nsIDOMMozCellBroadcastEvent> ce = do_QueryInterface(event);
  nsresult rv = ce->InitMozCellBroadcastEvent(NS_LITERAL_STRING("received"),
                                              true, false, aMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchTrustedEvent(ce);
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewCellBroadcast(nsPIDOMWindow* aWindow,
                    nsIDOMMozCellBroadcast** aCellBroadcast)
{
  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
    aWindow :
    aWindow->GetCurrentInnerWindow();

  nsCOMPtr<nsIRILContentHelper> ril =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_STATE(ril);

  nsRefPtr<mozilla::dom::CellBroadcast> cb =
    new mozilla::dom::CellBroadcast(innerWindow, ril);
  cb.forget(aCellBroadcast);

  return NS_OK;
}
