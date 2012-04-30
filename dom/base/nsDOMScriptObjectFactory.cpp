/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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

#include "nsDOMScriptObjectFactory.h"
#include "xpcexception.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsJSEnvironment.h"
#include "nsJSEventListener.h"
#include "nsGlobalWindow.h"
#include "nsIJSContextStack.h"
#include "nsISupportsPrimitives.h"
#include "nsDOMException.h"
#include "nsCRT.h"
#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif
#include "nsThreadUtils.h"

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

nsIExceptionProvider* gExceptionProvider = nsnull;

nsDOMScriptObjectFactory::nsDOMScriptObjectFactory()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }

  nsCOMPtr<nsIExceptionProvider> provider = new nsDOMExceptionProvider();
  nsCOMPtr<nsIExceptionService> xs =
    do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);

  if (xs) {
    xs->RegisterExceptionProvider(provider, NS_ERROR_MODULE_DOM);
    xs->RegisterExceptionProvider(provider, NS_ERROR_MODULE_SVG);
    xs->RegisterExceptionProvider(provider, NS_ERROR_MODULE_DOM_XPATH);
    xs->RegisterExceptionProvider(provider, NS_ERROR_MODULE_DOM_INDEXEDDB);
    xs->RegisterExceptionProvider(provider, NS_ERROR_MODULE_XPCONNECT);
  }

  NS_ASSERTION(!gExceptionProvider, "Registered twice?!");
  provider.swap(gExceptionProvider);

  // And pre-create the javascript language.
  NS_CreateJSRuntime(getter_AddRefs(mJSRuntime));
}

NS_INTERFACE_MAP_BEGIN(nsDOMScriptObjectFactory)
  NS_INTERFACE_MAP_ENTRY(nsIDOMScriptObjectFactory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMScriptObjectFactory)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMScriptObjectFactory)
NS_IMPL_RELEASE(nsDOMScriptObjectFactory)

NS_IMETHODIMP_(nsISupports *)
nsDOMScriptObjectFactory::GetClassInfoInstance(nsDOMClassInfoID aID)
{
  return NS_GetDOMClassInfoInstance(aID);
}

NS_IMETHODIMP_(nsISupports *)
nsDOMScriptObjectFactory::GetExternalClassInfoInstance(const nsAString& aName)
{
  nsScriptNameSpaceManager *nameSpaceManager = nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, nsnull);

  const nsGlobalNameStruct *globalStruct = nameSpaceManager->LookupName(aName);
  if (globalStruct) {
    if (globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfoCreator) {
      nsresult rv;
      nsCOMPtr<nsIDOMCIExtension> creator(do_CreateInstance(globalStruct->mCID, &rv));
      NS_ENSURE_SUCCESS(rv, nsnull);

      rv = creator->RegisterDOMCI(NS_ConvertUTF16toUTF8(aName).get(), this);
      NS_ENSURE_SUCCESS(rv, nsnull);

      globalStruct = nameSpaceManager->LookupName(aName);
      NS_ENSURE_TRUE(globalStruct, nsnull);

      NS_ASSERTION(globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo,
                   "The classinfo data for this class didn't get registered.");
    }
    if (globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      return nsDOMClassInfo::GetClassInfoInstance(globalStruct->mData);
    }
  }
  return nsnull;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::Observe(nsISupports *aSubject,
                                  const char *aTopic,
                                  const PRUnichar *someData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
#ifdef MOZ_XUL
    // Flush the XUL cache since it holds JS roots, and we're about to
    // start the final GC.
    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();

    if (cache)
      cache->Flush();
#endif

    nsGlobalWindow::ShutDown();
    nsDOMClassInfo::ShutDown();

    if (gExceptionProvider) {
      nsCOMPtr<nsIExceptionService> xs =
        do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);

      if (xs) {
        xs->UnregisterExceptionProvider(gExceptionProvider,
                                        NS_ERROR_MODULE_DOM);
        xs->UnregisterExceptionProvider(gExceptionProvider,
                                        NS_ERROR_MODULE_SVG);
        xs->UnregisterExceptionProvider(gExceptionProvider,
                                        NS_ERROR_MODULE_DOM_XPATH);
        xs->UnregisterExceptionProvider(gExceptionProvider,
                                        NS_ERROR_MODULE_XPCONNECT);
      }

      NS_RELEASE(gExceptionProvider);
    }
  }

  return NS_OK;
}

static nsresult
CreateXPConnectException(nsresult aResult, nsIException *aDefaultException,
                         nsIException **_retval)
{
  // See whether we already have a useful XPConnect exception.  If we
  // do, let's not create one with _less_ information!
  nsCOMPtr<nsIXPCException> exception(do_QueryInterface(aDefaultException));
  if (!exception) {
    nsresult rv = NS_OK;
    exception = do_CreateInstance("@mozilla.org/js/xpc/Exception;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = exception->Initialize(nsnull, aResult, nsnull, nsnull, nsnull,
                               nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  exception.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::RegisterDOMClassInfo(const char *aName,
					       nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
					       const nsIID *aProtoChainInterface,
					       const nsIID **aInterfaces,
					       PRUint32 aScriptableFlags,
					       bool aHasClassInterface,
					       const nsCID *aConstructorCID)
{
  nsScriptNameSpaceManager *nameSpaceManager = nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return nameSpaceManager->RegisterDOMCIData(aName,
                                             aConstructorFptr,
                                             aProtoChainInterface,
                                             aInterfaces,
                                             aScriptableFlags,
                                             aHasClassInterface,
                                             aConstructorCID);
}


// Factories
nsresult
NS_GetJSRuntime(nsIScriptRuntime** aLanguage)
{
  nsCOMPtr<nsIDOMScriptObjectFactory> factory =
    do_GetService(kDOMScriptObjectFactoryCID);
  NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aLanguage = factory->GetJSRuntime());
  return NS_OK;
}

nsresult NS_GetScriptRuntime(const nsAString &aLanguageName,
                             nsIScriptRuntime **aLanguage)
{
  *aLanguage = NULL;

  NS_ENSURE_TRUE(aLanguageName.EqualsLiteral("application/javascript"),
                 NS_ERROR_FAILURE);

  return NS_GetJSRuntime(aLanguage);
}

nsresult NS_GetScriptRuntimeByID(PRUint32 aScriptTypeID,
                                 nsIScriptRuntime **aLanguage)
{
  *aLanguage = NULL;

  NS_ENSURE_TRUE(aScriptTypeID == nsIProgrammingLanguage::JAVASCRIPT,
                 NS_ERROR_FAILURE);

  return NS_GetJSRuntime(aLanguage);
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMExceptionProvider, nsIExceptionProvider)

NS_IMETHODIMP
nsDOMExceptionProvider::GetException(nsresult result,
                                     nsIException *aDefaultException,
                                     nsIException **_retval)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  switch (NS_ERROR_GET_MODULE(result))
  {
    case NS_ERROR_MODULE_SVG:
      return NS_NewSVGException(result, aDefaultException, _retval);
    case NS_ERROR_MODULE_DOM_XPATH:
      return NS_NewXPathException(result, aDefaultException, _retval);
    case NS_ERROR_MODULE_XPCONNECT:
      return CreateXPConnectException(result, aDefaultException, _retval);
    default:
      MOZ_ASSERT(NS_ERROR_GET_MODULE(result) == NS_ERROR_MODULE_DOM ||
          NS_ERROR_GET_MODULE(result) == NS_ERROR_MODULE_DOM_FILE ||
          NS_ERROR_GET_MODULE(result) == NS_ERROR_MODULE_DOM_INDEXEDDB,
          "Trying to create an exception for the wrong error module.");
      return NS_NewDOMException(result, aDefaultException, _retval);
  }
  NS_NOTREACHED("Not reached");
  return NS_ERROR_UNEXPECTED;
}
