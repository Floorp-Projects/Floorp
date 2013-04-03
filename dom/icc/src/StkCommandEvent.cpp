/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMClassInfo.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"
#include "SimToolKit.h"
#include "StkCommandEvent.h"

#include "nsJSON.h"
#include "jsapi.h"
#include "jsfriendapi.h"

DOMCI_DATA(MozStkCommandEvent, mozilla::dom::icc::StkCommandEvent)

namespace mozilla {
namespace dom {
namespace icc {

already_AddRefed<StkCommandEvent>
StkCommandEvent::Create(mozilla::dom::EventTarget* aOwner,
                        const nsAString& aMessage)
{
  nsRefPtr<StkCommandEvent> event = new StkCommandEvent(aOwner);
  event->mCommand = aMessage;
  return event.forget();
}

NS_IMPL_ADDREF_INHERITED(StkCommandEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(StkCommandEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(StkCommandEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozStkCommandEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozStkCommandEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
StkCommandEvent::GetCommand(JSContext* aCx, JS::Value* aCommand)

{
  nsCOMPtr<nsIJSON> json(new nsJSON());

  if (!mCommand.IsEmpty()) {
    nsresult rv = json->DecodeToJSVal(mCommand, aCx, aCommand);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    *aCommand = JSVAL_VOID;
  }

  return NS_OK;
}

}
}
}
