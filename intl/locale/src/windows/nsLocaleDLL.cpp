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

#include "nsRepository.h"
#include "nsIFactory.h"

#include "nsILocaleFactory.h"
#include "nsLocaleFactory.h"
#include "nsLocaleCID.h"
#include "nsCollationWin.h"
#include "nsDateTimeFormatWin.h"
#include "nsLocaleFactoryWin.h"
#include "nsIWin32LocaleImpl.h"
#include "nsIWin32LocaleFactory.h"

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

extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aCID, nsISupports* serviceMgr,
                                           nsIFactory **aFactory)
{
	nsIFactory*	factoryInstance;
	nsresult		res;

	if (aFactory == NULL) return NS_ERROR_NULL_POINTER;

	//
	// first check for the nsILocaleFactory interfaces
	//  
	if (aCID.Equals(kLocaleFactoryCID))
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
	if (aCID.Equals(kWin32LocaleFactoryCID))
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
	factoryInstance = new nsLocaleWinFactory(aCID);

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
