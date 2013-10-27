/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StkCommandEvent.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSON.h"
#include "SimToolKit.h"

namespace mozilla {
namespace dom {

already_AddRefed<StkCommandEvent>
StkCommandEvent::Create(EventTarget* aOwner,
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

} // namespace dom
} // namespace mozilla
