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
#include "nsScriptNameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsJSEnvironment.h"
#include "nsGlobalWindow.h"
#include "nsCRT.h"
#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif
#include "nsThreadUtils.h"

using mozilla::dom::GetNameSpaceManager;

nsDOMScriptObjectFactory::nsDOMScriptObjectFactory()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }

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
  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
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
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMScriptObjectFactory::RegisterDOMClassInfo(const char *aName,
					       nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
					       const nsIID *aProtoChainInterface,
					       const nsIID **aInterfaces,
					       uint32_t aScriptableFlags,
					       bool aHasClassInterface,
					       const nsCID *aConstructorCID)
{
  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return nameSpaceManager->RegisterDOMCIData(aName,
                                             aConstructorFptr,
                                             aProtoChainInterface,
                                             aInterfaces,
                                             aScriptableFlags,
                                             aHasClassInterface,
                                             aConstructorCID);
}
