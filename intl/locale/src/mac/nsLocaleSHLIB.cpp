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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"

#include "nsILocaleFactory.h"
#include "nsLocaleFactory.h"
#include "nsILocaleService.h"
#include "nsLocaleCID.h"
#include "nsCollationMac.h"
#include "nsIScriptableDateFormat.h"
#include "nsDateTimeFormatMac.h"
#include "nsLocaleFactoryMac.h"
#include "nsDateTimeFormatCID.h"
#include "nsCollationCID.h"
#include "nsIServiceManager.h"
#include "nsMacLocaleFactory.h"
#include "nsILanguageAtomService.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

//
// kLocaleFactory for the nsILocaleFactory interface
//
NS_DEFINE_IID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
NS_DEFINE_IID(kILocaleFactoryIID,NS_ILOCALEFACTORY_IID);
NS_DEFINE_IID(kMacLocaleFactoryCID,NS_MACLOCALEFACTORY_CID);
NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

//
// for the language atom service
//
NS_DEFINE_CID(kLanguageAtomServiceCID, NS_LANGUAGEATOMSERVICE_CID);

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
NS_DEFINE_CID(kScriptableDateFormatCID, NS_SCRIPTABLEDATEFORMAT_CID);

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory)
{
	nsIFactory*	factoryInstance;
	nsresult		res;

	if (aFactory == NULL) return NS_ERROR_NULL_POINTER;
	*aFactory = NULL;
	//
	// first check for the nsILocaleFactory interfaces
	//  
	if (aClass.Equals(kLocaleFactoryCID))
	{
		nsLocaleFactory *factory = new nsLocaleFactory();
		if (NULL==factory) 
		{
			*aFactory=NULL;
			return NS_ERROR_OUT_OF_MEMORY;
		}
		
		res = factory->QueryInterface(kILocaleFactoryIID, (void **) aFactory);

		if (NS_FAILED(res))
		{
			*aFactory = NULL;
			delete factory;
		}
			printf("returning nsLocaleFactory\n");
			return res;
	}

	if (aClass.Equals(kLocaleServiceCID)) { 
    	factoryInstance = new nsLocaleServiceFactory(); 
    	res = factoryInstance->QueryInterface(kIFactoryIID,(void**)aFactory); 
    	if (NS_FAILED(res)) { *aFactory=NULL; delete factoryInstance; } 
    	return res; 
    } 
	
	if (aClass.Equals(kMacLocaleFactoryCID))
	{
		nsMacLocaleFactory	*mac_factory = new nsMacLocaleFactory();
		if (NULL==mac_factory)
		{
			*aFactory=NULL;
			return NS_ERROR_OUT_OF_MEMORY;
		}
		
		res = mac_factory->QueryInterface(kIFactoryIID,(void**) aFactory);
		
		if (NS_FAILED(res))
		{
			*aFactory = NULL;
			delete mac_factory;
		}
		
			return res;
	}
	//
	// let the nsLocaleFactoryWin logic take over from here
	//
	factoryInstance = new nsLocaleMacFactory(aClass);

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
                           NS_GET_IID(nsIComponentManager), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  // 
  // register the generic factory 
  // 
  rv = compMgr->RegisterComponent(kLocaleFactoryCID,"nsLocale component", 
                                  NS_LOCALE_CONTRACTID,path,PR_TRUE,PR_TRUE); 
  NS_ASSERTION(NS_SUCCEEDED(rv),"nsLocaleTest: RegisterFactory failed."); 
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done; 
 
  // 
  // register the service  
  // 
  rv = compMgr->RegisterComponent(kLocaleServiceCID,"nsLocaleService component", 
                                  NS_LOCALESERVICE_CONTRACTID,path,PR_TRUE,PR_TRUE); 
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done; 

  //
  // register the generic factory
  //
  rv = compMgr->RegisterComponent(kMacLocaleFactoryCID,NULL,NULL,path,PR_TRUE,PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv),"nsLocaleTest: RegisterFactory failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

  //
  // register the collation factory
  //
  rv = compMgr->RegisterComponent(kCollationFactoryCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv),"nsLocaleTest: Register CollationFactory failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;
  
  //
  // register the collation interface
  //
  rv = compMgr->RegisterComponent(kCollationCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv),"nsLocaleTest: Register Collation failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;
  
  //
  // register the date time formatter
  //
  rv = compMgr->RegisterComponent(kDateTimeFormatCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv),"nsLocaleTest: Register DateTimeFormat failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;

	//
	// register the scriptable date time formatter
	//
	rv = compMgr->RegisterComponent(kScriptableDateFormatCID, "Scriptable Date Format", 
    NS_SCRIPTABLEDATEFORMAT_CONTRACTID, path, PR_TRUE, PR_TRUE);
	NS_ASSERTION(NS_SUCCEEDED(rv),"nsLocaleTest: Register ScriptableDateFormat failed.");
  if (NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv)) goto done;
  
  //
  // register the language atom service
  //
  rv = compMgr->RegisterComponent(kLanguageAtomServiceCID, "Language Atom Service", 
  NS_LANGUAGEATOMSERVICE_CONTRACTID, path, PR_TRUE, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv),"nsLocaleTest: Register LanguageAtomService failed.");
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
                           NS_GET_IID(nsIComponentManager), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kLocaleFactoryCID, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kCollationFactoryCID, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kCollationCID, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kDateTimeFormatCID, path);
  if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterComponent(kScriptableDateFormatCID, path);
	if (NS_FAILED(rv)) goto done;
	
  rv = compMgr->UnregisterComponent(kLanguageAtomServiceCID, path);
  if (NS_FAILED(rv)) goto done;

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
