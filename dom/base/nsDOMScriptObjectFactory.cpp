/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

nsIExceptionProvider* gExceptionProvider = nullptr;

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
    xs->RegisterExceptionProvider(provider, NS_ERROR_MODULE_DOM_FILEHANDLE);
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
  NS_ENSURE_TRUE(nameSpaceManager, nullptr);

  const nsGlobalNameStruct *globalStruct = nameSpaceManager->LookupName(aName);
  if (globalStruct) {
    if (globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfoCreator) {
      nsresult rv;
      nsCOMPtr<nsIDOMCIExtension> creator(do_CreateInstance(globalStruct->mCID, &rv));
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = creator->RegisterDOMCI(NS_ConvertUTF16toUTF8(aName).get(), this);
      NS_ENSURE_SUCCESS(rv, nullptr);

      globalStruct = nameSpaceManager->LookupName(aName);
      NS_ENSURE_TRUE(globalStruct, nullptr);

      NS_ASSERTION(globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo,
                   "The classinfo data for this class didn't get registered.");
    }
    if (globalStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      return nsDOMClassInfo::GetClassInfoInstance(globalStruct->mData);
    }
  }
  return nullptr;
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
                                        NS_ERROR_MODULE_DOM_INDEXEDDB);
        xs->UnregisterExceptionProvider(gExceptionProvider,
                                        NS_ERROR_MODULE_DOM_FILEHANDLE);
      }

      NS_RELEASE(gExceptionProvider);
    }
  }

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
    case NS_ERROR_MODULE_DOM:
    case NS_ERROR_MODULE_SVG:
    case NS_ERROR_MODULE_DOM_XPATH:
    case NS_ERROR_MODULE_DOM_FILE:
    case NS_ERROR_MODULE_DOM_INDEXEDDB:
    case NS_ERROR_MODULE_DOM_FILEHANDLE:
      return NS_NewDOMException(result, aDefaultException, _retval);
    default:
      NS_WARNING("Trying to create an exception for the wrong error module.");
      return NS_ERROR_FAILURE;
  }
  NS_NOTREACHED("Not reached");
  return NS_ERROR_UNEXPECTED;
}
