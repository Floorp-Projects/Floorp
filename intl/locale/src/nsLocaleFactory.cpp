/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

 
#include "nsILocaleFactory.h"
#include "nsLocaleFactory.h"
#include "nsILocale.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"


NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
NS_DEFINE_IID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);

NS_IMPL_ISUPPORTS(nsLocaleFactory,kILocaleFactoryIID)


char* localeCatagoryList[6] = { "NSILOCALE_TIME",
								"NSILOCALE_COLLATE",
								"NSILOCALE_CTYPE",
								"NSILOCALE_MONETARY",
								"NSILOCALE_MESSAGES",
								"NSILOCALE_NUMERIC"
};

nsLocaleFactory::nsLocaleFactory(void)
:	fSystemLocale(NULL),
	fApplicationLocale(NULL)
{
  int	i;

  NS_INIT_REFCNT();

  fCatagoryList = new nsString*[6];

  for(i=0;i<6;i++)
	  fCatagoryList[i] = new nsString(localeCatagoryList[i]);

}

nsLocaleFactory::~nsLocaleFactory(void)
{
	int i;

	for(i=0;i<6;i++)
		delete fCatagoryList[i];

	delete []fCatagoryList;
}

NS_IMETHODIMP
nsLocaleFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult)
{
	nsresult	result;

	result = this->GetApplicationLocale((nsILocale**)aResult);

	return result;
}

NS_IMETHODIMP
nsLocaleFactory::LockFactory(PRBool	aBool)
{
	return NS_OK;
}

NS_IMETHODIMP
nsLocaleFactory::NewLocale(nsString** catagoryList,nsString**  
      valueList, PRUint8 count, nsILocale** locale)
{
	nsLocale*	newLocale;

	newLocale = new nsLocale(catagoryList,valueList,count);
	NS_ASSERTION(newLocale!=NULL,"nsLocaleFactory: failed to create nsLocale instance");
	newLocale->AddRef();

	*locale = (nsILocale*)newLocale;

	return NS_OK;
}

NS_IMETHODIMP
nsLocaleFactory::NewLocale(const nsString* localeName, nsILocale** locale)
{
	int	i;
	nsString**	valueList;
	nsLocale*	newLocale;

	valueList = new nsString*[6];
	for(i=0;i<6;i++)
		valueList[i] = new nsString(*localeName);

	newLocale = new nsLocale(fCatagoryList,valueList,6);
	NS_ASSERTION(newLocale!=NULL,"nsLocaleFactory: failed to create nsLocale");
	newLocale->AddRef();

	//
	// delete temporary strings
	//
	for(i=0;i<6;i++)
		delete valueList[i];
	delete [] valueList;

	*locale = (nsILocale*)newLocale;

	return NS_OK;
}

NS_IMETHODIMP
nsLocaleFactory::GetSystemLocale(nsILocale** systemLocale)
{
	nsString*	systemLocaleName;
	nsresult	result;

	if (fSystemLocale!=NULL)
	{
		fSystemLocale->AddRef();
		*systemLocale = fSystemLocale;
		return NS_OK;
	}
	
	//
	// for now
	//
	systemLocaleName = new nsString("en-US");
	result = this->NewLocale(systemLocaleName,systemLocale);

	delete systemLocaleName;

	return result;
}

NS_IMETHODIMP
nsLocaleFactory::GetApplicationLocale(nsILocale** applicationLocale)
{
	nsString*	applicationLocaleName;
	nsresult	result;

	if (fApplicationLocale!=NULL)
	{
		fApplicationLocale->AddRef();
		*applicationLocale = fApplicationLocale;
		return NS_OK;
	}
	
	//
	// for now
	//
	applicationLocaleName = new nsString("en-US");
	result = this->NewLocale(applicationLocaleName,applicationLocale);

	delete applicationLocaleName;

	return result;
}

