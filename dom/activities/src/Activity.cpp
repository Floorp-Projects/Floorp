/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Activity.h"

#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsIConsoleService.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMActivityOptions.h"

using namespace mozilla::dom;

#ifdef MOZ_SYS_MSG
DOMCI_DATA(MozActivity, Activity)
#endif

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Activity)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozActivity)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
#ifdef MOZ_SYS_MSG
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozActivity)
#endif
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(Activity, DOMRequest)
NS_IMPL_RELEASE_INHERITED(Activity, DOMRequest)

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(Activity, DOMRequest,
                                     mProxy)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Activity, DOMRequest)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMETHODIMP
Activity::Initialize(nsISupports* aOwner,
                     JSContext* aContext,
                     JSObject* aObject,
                     uint32_t aArgc,
                     JS::Value* aArgv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aOwner);
  NS_ENSURE_TRUE(window, NS_ERROR_UNEXPECTED);

  Init(window);

  nsCOMPtr<nsIDocument> document = do_QueryInterface(window->GetExtantDoc());

  bool isActive;
  window->GetDocShell()->GetIsActive(&isActive);

  if (!isActive &&
      !nsContentUtils::IsChromeDoc(document)) {
    nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService("@mozilla.org/dom/dom-request-service;1");
    rs->FireErrorAsync(static_cast<DOMRequest*>(this),
                       NS_LITERAL_STRING("NotUserInput"));

    nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));
    NS_ENSURE_TRUE(console, NS_OK);

    nsString message =
      NS_LITERAL_STRING("Can start activity from non user input or chrome code");
    console->LogStringMessage(message.get());

    return NS_OK;
  }

  // We expect a single argument, which is a nsIDOMMozActivityOptions.
  if (aArgc != 1 || !aArgv[0].isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISupports> tmp;
  nsContentUtils::XPConnect()->WrapJS(aContext, aArgv[0].toObjectOrNull(),
                                      NS_GET_IID(nsIDOMMozActivityOptions),
                                      getter_AddRefs(tmp));
  nsCOMPtr<nsIDOMMozActivityOptions> options = do_QueryInterface(tmp);
  if (!options) {
    return NS_ERROR_INVALID_ARG;
  }

  // Instantiate a JS proxy that will do the child <-> parent communication
  // with the JS implementation of the backend.
  nsresult rv;
  mProxy = do_CreateInstance("@mozilla.org/dom/activities/proxy;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mProxy->StartActivity(this, options, window);
  return NS_OK;
}

Activity::~Activity()
{
  if (mProxy) {
    mProxy->Cleanup();
  }
}

Activity::Activity()
  : DOMRequest()
{
  // Unfortunately we must explicitly declare the default constructor in order
  // to prevent an implicitly deleted constructor in DOMRequest compile error
  // in GCC 4.6.
}

