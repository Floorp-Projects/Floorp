/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsICharsetConverterManager.h"
#include "nsCharsetConverterManager.h"
#include "nsIUnicodeDecodeHelper.h"
#include "nsUnicodeDecodeHelper.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsUnicodeEncodeHelper.h"
#include "nsIPlatformCharset.h"
#include "nsPlatformCharsetFactory.h"
#include "nsICharsetAlias.h"
#include "nsCharsetAliasFactory.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

//----------------------------------------------------------------------
// Global functions and data [declaration]

extern "C" PRInt32 g_InstanceCount = 0;
extern "C" PRInt32 g_LockCount = 0;

//----------------------------------------------------------------------
// Global functions and data [implementation]

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr)
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) return NS_ERROR_NULL_POINTER;

  // the converter manager
  if (aClass.Equals(kCharsetConverterManagerCID)) {
    nsManagerFactory *factory = new nsManagerFactory();
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  // the Unicode Decode helper
  if (aClass.Equals(kUnicodeDecodeHelperCID)) {
    nsDecodeHelperFactory *factory = new nsDecodeHelperFactory();
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  // the Unicode Encode helper
  if (aClass.Equals(kUnicodeEncodeHelperCID)) {
    nsEncodeHelperFactory *factory = new nsEncodeHelperFactory();
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  if (aClass.Equals(kPlatformCharsetCID)) {
    nsIFactory *factory = NEW_PLATFORMCHARSETFACTORY();
	nsresult res = factory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  if (aClass.Equals(kCharsetAliasCID)) {
    nsIFactory *factory = NEW_CHARSETALIASFACTORY();
	nsresult res = factory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char * path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kUnicodeDecodeHelperCID, NULL, NULL,
      path, PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kUnicodeEncodeHelperCID, NULL, NULL,
      path, PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kCharsetAliasCID, NULL, NULL, path, 
      PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kCharsetConverterManagerCID, NULL, NULL,
      path, PR_TRUE, PR_TRUE);
  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  rv = compMgr->RegisterComponent(kPlatformCharsetCID, NULL, NULL, path, 
      PR_TRUE, PR_TRUE);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char * path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterFactory(kUnicodeDecodeHelperCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterFactory(kUnicodeEncodeHelperCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterFactory(kCharsetAliasCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterFactory(kCharsetConverterManagerCID, path);
  if(NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterFactory(kPlatformCharsetCID, path);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
