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
 * Copyright (C) 1998,1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsAbBaseCID.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"


/* Include all of the interfaces our factory can generate components for */

#include "nsDirectoryDataSource.h"
#include "nsCardDataSource.h"
#include "nsAbDirectory.h"
#include "nsAbCard.h"
#include "nsAddrDatabase.h"
#include "nsAddressBook.h"
#include "nsAddrBookSession.h"
#include "nsAbDirProperty.h"
#include "nsAbAutoCompleteSession.h"
#include "nsAbAddressCollecter.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kAddressBookCID, NS_ADDRESSBOOK_CID);
static NS_DEFINE_CID(kAbDirectoryDataSourceCID, NS_ABDIRECTORYDATASOURCE_CID);
static NS_DEFINE_CID(kAbDirectoryCID, NS_ABDIRECTORYRESOURCE_CID); 
static NS_DEFINE_CID(kAbCardDataSourceCID, NS_ABCARDDATASOURCE_CID);
static NS_DEFINE_CID(kAbCardCID, NS_ABCARDRESOURCE_CID); 
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kAbCardPropertyCID, NS_ABCARDPROPERTY_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAbDirPropertyCID, NS_ABDIRPROPERTY_CID);
static NS_DEFINE_CID(kAbAutoCompleteSessionCID ,NS_ABAUTOCOMPLETESESSION_CID);

static NS_DEFINE_CID(kAbAddressCollecterCID, NS_ABADDRESSCOLLECTER_CID);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsAbFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

	nsAbFactory(const nsCID &aClass, const char* aClassName, const char* aProgID); 

	// nsIFactory methods   
	NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
	NS_IMETHOD LockFactory(PRBool aLock);   

protected:
	virtual ~nsAbFactory();   

	nsCID mClassID;
	char* mClassName;
	char* mProgID;

//	nsIServiceManager* mServiceManager;
};   

nsAbFactory::nsAbFactory(const nsCID &aClass, const char* aClassName, const char* aProgID)
  : mClassID(aClass), mClassName(nsCRT::strdup(aClassName)), mProgID(nsCRT::strdup(aProgID))
{   
	NS_INIT_REFCNT();
	// store a copy of the 
//	compMgrSupports->QueryInterface(nsIServiceManager::GetIID(),
//                                  (void **)&mServiceManager);
}   

nsAbFactory::~nsAbFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
//	NS_IF_RELEASE(mServiceManager);
	PL_strfree(mClassName);
	PL_strfree(mProgID);
}   

nsresult nsAbFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
	if (aResult == NULL) 
		return NS_ERROR_NULL_POINTER;  

	// Always NULL result, in case of failure   
	*aResult = NULL;   

	// we support two interfaces....nsISupports and nsFactory.....
	if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))    
		*aResult = (void *)(nsISupports*)this;   
	else if (aIID.Equals(nsCOMTypeInfo<nsIFactory>::GetIID()))   
		*aResult = (void *)(nsIFactory*)this;   

	if (*aResult == NULL)
		return NS_NOINTERFACE;

	AddRef(); // Increase reference count for caller   
	return NS_OK;   
}   


NS_IMPL_ADDREF(nsAbFactory)
NS_IMPL_RELEASE(nsAbFactory)

nsresult nsAbFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
    
	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	if (mClassID.Equals(kAddressBookCID)) 
	{
		nsresult rv;
		nsAddressBook * addressBook = new nsAddressBook();
		if (addressBook)
			rv = addressBook->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_NOT_INITIALIZED;

		if (NS_FAILED(rv) && addressBook)
			delete addressBook;
		return rv;
	}
	else if (mClassID.Equals(kAbDirectoryDataSourceCID)) 
	{
		return NS_NewAbDirectoryDataSource(aIID, aResult);
	}
	else if (mClassID.Equals(kAbDirectoryCID)) 
	{
		nsresult rv;
		nsAbDirectory * directory = new nsAbDirectory();
		if (directory)
			rv = directory->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && directory)
			delete directory;
		return rv;
	}
	else if (mClassID.Equals(kAbCardDataSourceCID)) 
	{
		return NS_NewAbCardDataSource(aIID, aResult);
	}
	else if (mClassID.Equals(kAbCardCID)) 
	{
		nsresult rv;
		nsAbCard * card = new nsAbCard();
		if (card)
			rv = card->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && card)
			delete card;
		return rv;
	}
	else if (mClassID.Equals(kAddressBookDBCID)) 
	{
		nsresult rv;
		nsAddrDatabase * abDatabase = new nsAddrDatabase();
		if (abDatabase)
			rv = abDatabase->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && abDatabase)
			delete abDatabase;
		return rv;
	} 
	else if (mClassID.Equals(kAbCardPropertyCID)) 
	{
		nsresult rv;
		nsAbCardProperty * abCardProperty = new nsAbCardProperty();
		if (abCardProperty)
			rv = abCardProperty->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && abCardProperty)
			delete abCardProperty;
		return rv;
	} 
	else if (mClassID.Equals(kAbDirPropertyCID)) 
	{
		nsresult rv;
		nsAbDirProperty * abDirProperty = new nsAbDirProperty();
		if (abDirProperty)
			rv = abDirProperty->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && abDirProperty)
			delete abDirProperty;
		return rv;
	} 
	else if (mClassID.Equals(kAddrBookSessionCID)) 
	{
		nsresult rv;
		nsAddrBookSession * abSession = new nsAddrBookSession();
		if (abSession)
			rv = abSession->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && abSession)
			delete abSession;
		return rv;
	}  
	else if (mClassID.Equals(kAbAutoCompleteSessionCID))
	{
		return NS_NewAbAutoCompleteSession(aIID, aResult);
	}
	else if (mClassID.Equals(kAbAddressCollecterCID))
	{
		nsresult rv;
		nsAbAddressCollecter * abAddressCollecter = new nsAbAddressCollecter;
		if (abAddressCollecter)
			rv = abAddressCollecter->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && abAddressCollecter)
			delete abAddressCollecter;
		return rv;
	}
	return NS_NOINTERFACE;  
}  

nsresult nsAbFactory::LockFactory(PRBool aLock)  
{  
	if (aLock)
		PR_AtomicIncrement(&g_LockCount); 
	else
		PR_AtomicDecrement(&g_LockCount); 

  return NS_OK;
}  

// return the proper factory to the caller. 
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

	*aFactory = new nsAbFactory(aClass, aClassName, aProgID);

	if (aFactory)
		return (*aFactory)->QueryInterface(nsCOMTypeInfo<nsIFactory>::GetIID(), (void**)aFactory); // they want a Factory Interface so give it to them
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr) 
{
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv = NS_OK;
	nsresult finalResult = NS_OK;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kAddressBookCID,
								  "Address Book DOM interaction object",
								  "component://netscape/addressbook",
								  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

	// register our RDF datasources:
	rv = compMgr->RegisterComponent(kAbDirectoryDataSourceCID, 
							  "Mail/News Address Book Directory Data Source",
							  NS_RDF_DATASOURCE_PROGID_PREFIX "addressdirectory",
							  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kAbDirectoryCID,
								  "Mail/News Address Book Directory Factory",
								  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "abdirectory",
								  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kAbCardDataSourceCID, 
							  "Mail/News Address Book Card Data Source",
							  NS_RDF_DATASOURCE_PROGID_PREFIX "addresscard",
							  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kAbCardCID,
								  "Mail/News Address Book Card Factory",
								  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "abcard",
								  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;
	
	rv = compMgr->RegisterComponent(kAddressBookDBCID, 
								   "Mail/News Address Book Card Database", 
								   "component://netscape/addressbook/carddatabase",
								   path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv))finalResult = rv;

	rv = compMgr->RegisterComponent(kAbCardPropertyCID,
								  "Mail/News Address Book Card Property",
								  "component://netscape/addressbook/cardproperty",
								  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;
	
	rv = compMgr->RegisterComponent(kAbDirPropertyCID,
								  "Mail/News Address Book Directory Property",
								  "component://netscape/addressbook/directoryproperty",
								  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;
	
	rv = compMgr->RegisterComponent(kAddrBookSessionCID,
								  "Address Book Session",
								  "component://netscape/addressbook/services/session",
								  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kAbAutoCompleteSessionCID,
								  "Address Book Auto Completion Session",
								  "component://netscape/messenger/autocomplete&type=addrbook",
								  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kAbAddressCollecterCID,
								  "Address Collecter Service",
								  "component://netscape/addressbook/services/addressCollecter",
								  path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;

	return finalResult;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv = NS_OK;
	nsresult finalResult = NS_OK;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kAddressBookCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAbDirectoryDataSourceCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAbDirectoryCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAbCardDataSourceCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAbCardCID, path);
	if (NS_FAILED(rv)) finalResult = rv;
	
	rv = compMgr->UnregisterComponent(kAddressBookDBCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAbCardPropertyCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAbDirPropertyCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAddrBookSessionCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kAbAutoCompleteSessionCID, path);
	if (NS_FAILED(rv)) finalResult = rv;
	
	rv = compMgr->UnregisterComponent(kAbAddressCollecterCID, path);
	if (NS_FAILED(rv)) finalResult = rv;
	
	return finalResult;
}
