/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMWorkerSecurityManager.h"

// Interfaces
#include "nsIClassInfo.h"

// Other includes
#include "jsapi.h"
#include "nsDOMError.h"
#include "nsThreadUtils.h"

// DOMWorker includes
#include "nsDOMThreadService.h"
#include "nsDOMWorker.h"

#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

class nsDOMWorkerPrincipal
{
public:
  static void Destroy(JSContext*, JSPrincipals*) {
    // nothing
  }

  static JSBool Subsume(JSPrincipals*, JSPrincipals*) {
    return JS_TRUE;
  }
};

static JSPrincipals gWorkerPrincipal =
{ "domworkerthread" /* codebase */,
  NULL /* getPrincipalArray */,
  NULL /* globalPrivilegesEnabled */,
  1 /* refcount */,
  nsDOMWorkerPrincipal::Destroy /* destroy */,
  nsDOMWorkerPrincipal::Subsume /* subsume */ };

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerSecurityManager,
                              nsIXPCSecurityManager)

NS_IMETHODIMP
nsDOMWorkerSecurityManager::CanCreateWrapper(JSContext* aCx,
                                             const nsIID& aIID,
                                             nsISupports* aObj,
                                             nsIClassInfo* aClassInfo,
                                             void** aPolicy)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerSecurityManager::CanCreateInstance(JSContext* aCx,
                                              const nsCID& aCID)
{
  return CanGetService(aCx, aCID);
}

NS_IMETHODIMP
nsDOMWorkerSecurityManager::CanGetService(JSContext* aCx,
                                          const nsCID& aCID)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsDOMWorker* worker = static_cast<nsDOMWorker*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  return worker->IsPrivileged() ? NS_OK : NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED;
}

NS_IMETHODIMP
nsDOMWorkerSecurityManager::CanAccess(PRUint32 aAction,
                                      nsAXPCNativeCallContext* aCallContext,
                                      JSContext* aJSContext,
                                      JSObject* aJSObject,
                                      nsISupports* aObj,
                                      nsIClassInfo* aClassInfo,
                                      jsid aName,
                                      void** aPolicy)
{
  return NS_OK;
}

JSPrincipals*
nsDOMWorkerSecurityManager::WorkerPrincipal()
{
  return &gWorkerPrincipal;
}

JSBool
nsDOMWorkerSecurityManager::JSCheckAccess(JSContext* aCx,
                                          JSObject* aObj,
                                          jsid aId,
                                          JSAccessMode aMode,
                                          jsval* aVp)
{
  return JS_TRUE;
}

JSPrincipals*
nsDOMWorkerSecurityManager::JSFindPrincipal(JSContext* aCx, JSObject* aObj)
{
  return WorkerPrincipal();
}

JSBool
nsDOMWorkerSecurityManager::JSTranscodePrincipals(JSXDRState* aXdr,
                                                 JSPrincipals** aJsprinp)
{
  NS_NOTREACHED("Shouldn't ever call this!");
  return JS_FALSE;
}
