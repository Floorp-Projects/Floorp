/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsBaseDLL.h"
#include "nscore.h"
#include "nsIProperties.h"
#include "nsProperties.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#ifdef XP_PC
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIObserver.h"
#include "nsObserver.h"
#endif

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#ifdef XP_PC
static NS_DEFINE_IID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);
static NS_DEFINE_IID(kObserverCID, NS_OBSERVER_CID);
#endif

PRInt32 gLockCount = 0;

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kPropertiesCID, NULL, NULL,
                                  path, PR_TRUE, PR_TRUE);
#ifdef XP_PC
  rv = compMgr->RegisterComponent(kObserverServiceCID,
                                   "ObserverService", 
                                   NS_OBSERVERSERVICE_PROGID,
                                   path,PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kObserverCID, 
                                   "Observer", 
                                   NS_OBSERVER_PROGID,
                                   path,PR_TRUE, PR_TRUE);
#endif

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterFactory(kPropertiesCID, path);

#ifdef XP_PC
  rv = compMgr->UnregisterFactory(kObserverServiceCID, path);
  rv = compMgr->UnregisterFactory(kObserverCID, path);
#endif

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* aServMgr, 
             const nsCID& aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory** aFactory)
{
  nsresult  res;

  if (!aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aClass.Equals(kPropertiesCID)) {
    nsPropertiesFactory *propsFactory = new nsPropertiesFactory();
    if (!propsFactory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    res = propsFactory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = nsnull;
      delete propsFactory;
    }

    return res;
  }
#ifdef XP_PC
  else if (aClass.Equals(kObserverServiceCID)) {
      nsObserverServiceFactory *observerServiceFactory = new nsObserverServiceFactory();

      if (observerServiceFactory == nsnull)
          return NS_ERROR_OUT_OF_MEMORY;

      res = observerServiceFactory->QueryInterface(kIFactoryIID, (void**) aFactory);
      if (NS_FAILED(res)) {
          *aFactory = nsnull;
          delete observerServiceFactory;
      }

    return res;
  } else if (aClass.Equals(kObserverCID)) {
      nsObserverFactory *observerFactory = new nsObserverFactory();

      if (observerFactory == nsnull)
          return NS_ERROR_OUT_OF_MEMORY;

      res = observerFactory->QueryInterface(kIFactoryIID, (void**) aFactory);
      if (NS_FAILED(res)) {
          *aFactory = nsnull;
          delete observerFactory;
      }

    return res;
  }
#endif

  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
  return PR_FALSE;      // XXX can we unload this?
}
