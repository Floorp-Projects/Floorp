/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsEntityConverter.h"
#include "nsIProperties.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNeckoUtil.h"

//
// guids
//
NS_DEFINE_IID(kIEntityConverterIID,NS_IENTITYCONVERTER_IID);
NS_DEFINE_IID(kIFactoryIID,NS_IFACTORY_IID);
NS_DEFINE_IID(kIPersistentPropertiesIID,NS_IPERSISTENTPROPERTIES_IID);

//
// implementation methods
//
nsEntityConverter::nsEntityConverter()
:	mEntityList(nsnull),
	mEntityListLength(0)
{
	NS_INIT_REFCNT();
	nsIPersistentProperties* pEntities = nsnull;
	nsIURI* url = nsnull;
	nsIInputStream* in = nsnull;
	nsresult	res;
	nsString	aUrl("resource:/res/entityTables/html40Latin1.properties");

	res = NS_NewURI(&url,aUrl,nsnull);
	if (NS_FAILED(res)) return;

	res = NS_OpenURI(&in,url);
	NS_RELEASE(url);
	if (NS_FAILED(res)) return;

	res = nsComponentManager::CreateInstance(NS_PERSISTENTPROPERTIES_PROGID,nsnull,
                                             kIPersistentPropertiesIID, 
                                             (void**)&pEntities);
	if(NS_SUCCEEDED(res) && in) {
		res = pEntities->Load(in);
		LoadEntityProperties(pEntities);
		pEntities->Release();
	}


}

nsEntityConverter::~nsEntityConverter()
{

}

void
nsEntityConverter::LoadEntityProperties(nsIPersistentProperties* pEntities)
{
	nsString key, value, temp;
	PRInt32	i;
	PRInt32	result;

	key="entity.list.name";
	nsresult rv = pEntities->GetProperty(key,mEntityListName);
	NS_ASSERTION(NS_SUCCEEDED(rv),"nsEntityConverter: malformed entity table\n");
	if (NS_FAILED(rv)) return;

	key="entity.list.length";
	rv = pEntities->GetProperty(key,value);
	NS_ASSERTION(NS_SUCCEEDED(rv),"nsEntityConverter: malformed entity table\n");
	if (NS_FAILED(rv)) return;
	mEntityListLength = value.ToInteger(&result);

	//
	// allocate the table
	//
	mEntityList = new nsEntityEntry[mEntityListLength];
	if (!mEntityList) return;

	for(i=1;i<=mEntityListLength;i++)
	{
		key="entity.";
		key.Append(i,10);
		rv = pEntities->GetProperty(key,value);
		if (NS_FAILED(rv)) return;

		PRInt32 offset = value.Find(":");
		value.Left(temp,offset);
		mEntityList[i-1].mChar = (PRUnichar)temp.ToInteger(&result);

		value.Mid(temp,offset+1,-1);
		mEntityList[i-1].mEntityValue = temp.ToNewUnicode();
	
	}


}
//
// nsISupports methods
//
NS_IMPL_ISUPPORTS(nsEntityConverter,kIEntityConverterIID)


//
// nsIEntityConverter
//
NS_IMETHODIMP
nsEntityConverter::ConvertToEntity(PRUnichar character, PRUnichar **_retval)
{
	PRInt32		i;

	for(i=0;i<mEntityListLength;i++)
	{
		if (mEntityList[i].mChar==character) {
				nsString entity = mEntityList[i].mEntityValue;
				*_retval = entity.ToNewUnicode();
				return NS_OK;
		}
	}

	return NS_ERROR_ILLEGAL_VALUE;
}


nsEntityConverterFactory::nsEntityConverterFactory()
{
  NS_INIT_REFCNT();
}

nsEntityConverterFactory::~nsEntityConverterFactory()
{

}

NS_IMPL_ISUPPORTS(nsEntityConverterFactory,kIFactoryIID)

//
// nsIFactory methods
//
NS_IMETHODIMP
nsEntityConverterFactory::CreateInstance(nsISupports* aOuter,REFNSIID aIID,void** aResult)
{
	nsEntityConverter*	entityConverter;

	entityConverter = new nsEntityConverter();
	if (entityConverter)
		return entityConverter->QueryInterface(aIID,aResult);

	*aResult=nsnull;
	return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsEntityConverterFactory::LockFactory(PRBool aLock)
{
	return NS_OK;
}

