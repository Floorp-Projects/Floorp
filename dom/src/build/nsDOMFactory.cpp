/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */
#include "nscore.h"
#include "nsIGenericFactory.h"

#include "nsJSEnvironment.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIScriptEventListener.h"
#include "nsIJSEventListener.h"
#include "nsIScriptContext.h"
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIJSContextStack.h"


extern nsresult NS_CreateScriptContext(nsIScriptGlobalObject *aGlobal,
                                       nsIScriptContext **aContext);

extern nsresult NS_NewJSEventListener(nsIDOMEventListener **aInstancePtrResult,
                                      nsIScriptContext *aContext,
                                      nsISupports *aObject);

extern nsresult NS_NewScriptGlobalObject(nsIScriptGlobalObject **aGlobal);


//////////////////////////////////////////////////////////////////////

class nsDOMSOFactory : public nsIDOMScriptObjectFactory,
                       public nsIObserver
{
public:
  nsDOMSOFactory();
  virtual ~nsDOMSOFactory();

  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  NS_IMETHOD NewScriptContext(nsIScriptGlobalObject *aGlobal,
                              nsIScriptContext **aContext);

  NS_IMETHOD NewJSEventListener(nsIScriptContext *aContext,
                                nsISupports* aObject,
                                nsIDOMEventListener ** aInstancePtrResult);

  NS_IMETHOD NewScriptGlobalObject(nsIScriptGlobalObject **aGlobal);

  NS_IMETHOD_(nsISupports *)GetClassInfoInstance(nsDOMClassInfoID aID);
};

nsDOMSOFactory::nsDOMSOFactory()
{
  NS_INIT_REFCNT();

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  }
}

nsDOMSOFactory::~nsDOMSOFactory()
{
}


NS_INTERFACE_MAP_BEGIN(nsDOMSOFactory)
  NS_INTERFACE_MAP_ENTRY(nsIDOMScriptObjectFactory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMScriptObjectFactory)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMSOFactory);
NS_IMPL_RELEASE(nsDOMSOFactory);


NS_IMETHODIMP
nsDOMSOFactory::NewScriptContext(nsIScriptGlobalObject *aGlobal,
                                 nsIScriptContext **aContext)
{
  return NS_CreateScriptContext(aGlobal, aContext);
}

NS_IMETHODIMP
nsDOMSOFactory::NewJSEventListener(nsIScriptContext *aContext,
                                   nsISupports *aObject,
                                   nsIDOMEventListener **aInstancePtrResult)
{
  return NS_NewJSEventListener(aInstancePtrResult, aContext, aObject);
}

NS_IMETHODIMP
nsDOMSOFactory::NewScriptGlobalObject(nsIScriptGlobalObject **aGlobal)
{
  return NS_NewScriptGlobalObject(aGlobal);
}

NS_IMETHODIMP_(nsISupports *)
nsDOMSOFactory::GetClassInfoInstance(nsDOMClassInfoID aID)
{
  return nsDOMClassInfo::GetClassInfoInstance(aID);
}

NS_IMETHODIMP
nsDOMSOFactory::Observe(nsISupports *aSubject, 
                        const char *aTopic,
                        const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    nsCOMPtr<nsIThreadJSContextStack> stack =
      do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      JSContext *cx = nsnull;

      stack->GetSafeJSContext(&cx);

      if (cx) {
        // Do one final GC to clean things up before shutdown.

        ::JS_GC(cx);
      }
    }

    GlobalWindowImpl::ShutDown();
    nsDOMClassInfo::ShutDown();
  }

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMSOFactory);

static nsModuleComponentInfo gDOMModuleInfo[] = {
  { "Script Object Factory",
    NS_DOM_SCRIPT_OBJECT_FACTORY_CID,
    nsnull,
    nsDOMSOFactoryConstructor
  }
};

void PR_CALLBACK
DOMModuleDestructor(nsIModule *self)
{
  GlobalWindowImpl::ShutDown();
  nsDOMClassInfo::ShutDown();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(DOM_components, gDOMModuleInfo,
                              DOMModuleDestructor)


