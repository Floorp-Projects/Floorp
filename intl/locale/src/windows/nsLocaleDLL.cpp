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
#include "nsIFactory.h"

#include "nsILocaleFactory.h"
#include "nsLocaleFactory.h"
#include "nsLocaleCID.h"
#include "nsCollationWin.h"
#include "nsDateTimeFormatWin.h"
#include "nsLocaleFactoryWin.h"
#include "nsIWin32LocaleImpl.h"
#include "nsIWin32LocaleFactory.h"
#include "nsDateTimeFormatCID.h"
#include "nsCollationCID.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

//
// kLocaleFactory for the nsILocaleFactory interface and friends
//
NS_DEFINE_IID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
NS_DEFINE_IID(kILocaleFactoryIID,NS_ILOCALEFACTORY_IID);
NS_DEFINE_IID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
NS_DEFINE_IID(kIWin32LocaleIID,NS_IWIN32LOCALE_IID);

//
// for the collation and formatting interfaces
//
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);                                                         
NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);                                                         
NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);
NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

//
// private entry points used for debugging
//
#ifdef DEBUG
void test_internal_tables(void);
#endif

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	nsIFactory*	factoryInstance;
	nsresult		res;

#ifdef DEBUG_tague
	test_internal_tables();
#endif

	if (aFactory == NULL) return NS_ERROR_NULL_POINTER;

	//
	// first check for the nsILocaleFactory interfaces
	//  
	if (aClass.Equals(kLocaleFactoryCID))
	{
		factoryInstance = new nsLocaleFactory();
		res = factoryInstance->QueryInterface(kILocaleFactoryIID, (void **) aFactory);

		if (NS_FAILED(res))
		{
			*aFactory = NULL;
			delete factoryInstance;
		}

			return res;
	}
	
	//
	// okay about bout nsIWin32LocaleManager
	//
	if (aClass.Equals(kWin32LocaleFactoryCID))
	{
		factoryInstance = new nsIWin32LocaleFactory();
		res = factoryInstance->QueryInterface(kIFactoryIID,(void**)aFactory);
		if (NS_FAILED(res))
		{
			*aFactory=NULL;
			delete factoryInstance;
		}

		return res;
	}
	//
	// let the nsLocaleFactoryWin logic take over from here
	//
	factoryInstance = new nsLocaleWinFactory(aClass);

	if(NULL == factoryInstance) 
	{
		return NS_ERROR_OUT_OF_MEMORY;  
	}

	res = factoryInstance->QueryInterface(kIFactoryIID, (void**)aFactory);
	if (NS_FAILED(res))
	{
		*aFactory = NULL;
		delete factoryInstance;
	}

	return res;
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

	//
	// register the generic factory
	//
	rv = compMgr->RegisterComponent(kLocaleFactoryCID,NULL,NULL,path,PR_TRUE,PR_TRUE);
	NS_ASSERTION(rv==NS_OK,"nsLocaleTest: RegisterFactory failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

	//
	// register the windows specific factory
	//
	rv = compMgr->RegisterComponent(kWin32LocaleFactoryCID,NULL,NULL,path,PR_TRUE,PR_TRUE);
	NS_ASSERTION(rv==NS_OK,"nsLocaleTest: Register nsIWin32LocaleFactory failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

	//
	// register the collation factory
	//
	rv = compMgr->RegisterComponent(kCollationFactoryCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
	NS_ASSERTION(rv==NS_OK,"nsLocaleTest: Register CollationFactory failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;
	
	//
	// register the collation interface
	//
	rv = compMgr->RegisterComponent(kCollationCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
	NS_ASSERTION(rv==NS_OK,"nsLocaleTest: Register Collation failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;
	
	//
	// register the date time formatter
	//
	rv = compMgr->RegisterComponent(kDateTimeFormatCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
	NS_ASSERTION(rv==NS_OK,"nsLocaleTest: Register DateTimeFormat failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

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

	rv = compMgr->UnregisterFactory(kLocaleFactoryCID, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kWin32LocaleFactoryCID, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kCollationFactoryCID, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kCollationCID, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kDateTimeFormatCID, path);
	if (NS_FAILED(rv)) goto done;

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
