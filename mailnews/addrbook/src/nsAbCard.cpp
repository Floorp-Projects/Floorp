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

#include "msgCore.h"    // precompiled header...

#include "nsAbCard.h"	 
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddressBookDB, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);


// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsABCard::nsABCard(void)
  : nsRDFResource(), mListeners(nsnull),
    mInitialized(PR_FALSE), 
    mCsid(0), mDepth(0), mPrefFlags(0)
{
//  NS_INIT_REFCNT(); done by superclass

	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));

	//The rdf:addresscard datasource is going to be a listener to all nsIMsgFolders, so add
	//it as a listener
 /*   nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFDataSource> datasource;
		rv = rdfService->GetDataSource("rdf:addresscard", getter_AddRefs(datasource));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIAbListener> directoryListener(do_QueryInterface(datasource, &rv));
			if(NS_SUCCEEDED(rv))
			{
				AddAddrBookListener(directoryListener);
			}
		}
	}*/

	m_pFirstName = nsnull;
	m_pLastName = nsnull;
	m_pDisplayName = nsnull;
	m_pPrimaryEmail = nsnull;
	m_pSecondEmail = nsnull;
	m_pWorkPhone = nsnull;
	m_pHomePhone = nsnull;
	m_pFaxNumber = nsnull;
	m_pPagerNumber = nsnull;
	m_pCellularNumber = nsnull;

	m_dbRowID = -1;
}

nsABCard::~nsABCard(void)
{
	if(mSubDirectories)
	{
		PRUint32 count;
		nsresult rv = mSubDirectories->Count(&count);
		NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
		PRInt32 i;
		for (i = count - 1; i >= 0; i--)
			mSubDirectories->RemoveElementAt(i);
	}

	if(mDatabase)
		mDatabase->RemoveListener(this);

	if (mListeners) 
	{
		PRInt32 i;
		for (i = mListeners->Count() - 1; i >= 0; --i) 
			mListeners->RemoveElementAt(i);
		delete mListeners;
	}

	PR_FREEIF(m_pFirstName);
	PR_FREEIF(m_pLastName);
	PR_FREEIF(m_pDisplayName);
	PR_FREEIF(m_pPrimaryEmail);
	PR_FREEIF(m_pSecondEmail);
	PR_FREEIF(m_pWorkPhone);
	PR_FREEIF(m_pHomePhone);
	PR_FREEIF(m_pFaxNumber);
	PR_FREEIF(m_pPagerNumber);
	PR_FREEIF(m_pCellularNumber);
}

NS_IMPL_ISUPPORTS_INHERITED(nsABCard, nsRDFResource, nsIAbCard)

////////////////////////////////////////////////////////////////////////////////

typedef PRBool
(*nsArrayFilter)(nsISupports* element, void* data);

#if 0
static nsresult
nsFilterBy(nsISupportsArray* array, nsArrayFilter filter, void* data,
           nsISupportsArray* *result)
{
  nsCOMPtr<nsISupportsArray> f;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(f));
  if (NS_FAILED(rv)) return rv;
  PRUint32 i, count;
  rv = array->Count(&count);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> element = getter_AddRefs(array->ElementAt(i));
    if (filter(element, data)) {
      rv = f->AppendElement(element);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  *result = f;
  return NS_OK;
}
#endif 

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsABCard::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsABCard::OnCardEntryChange
(PRUint32 abCode, PRUint32 entryID, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsABCard::OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsABCard::AddUnique(nsISupports* element)
{
  // XXX fix this
  return mSubDirectories->AppendElement(element);
}

NS_IMETHODIMP
nsABCard::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsABCard::GetChildNodes(nsIEnumerator* *result)
{
  if (!mInitialized) 
  {
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1") ||
        !PL_strcmp(mURI, "abcard://Pab1/Card2") ||
        !PL_strcmp(mURI, "abcard://Pab2/Card1") ||
        !PL_strcmp(mURI, "abcard://Pab2/Card2") ||
        !PL_strcmp(mURI, "abcard://Pab3/Card1") ||
        !PL_strcmp(mURI, "abcard://Pab3/Card2"))
	{	
		PRInt32 i;
		for (i= 0; i < 6; i++)
		{
			nsCOMPtr<nsIAbCard> card;
			if (i == 0)
				AddSubNode("PersonName", getter_AddRefs(card));
			if (i == 1)
				AddSubNode("Email", getter_AddRefs(card));
			if (i == 2)
				AddSubNode("WorkPhone", getter_AddRefs(card));
			if (i == 3)
				AddSubNode("Organization", getter_AddRefs(card));
			if (i == 4)
				AddSubNode("Nickname", getter_AddRefs(card));
			if (i == 5)
				AddSubNode("City", getter_AddRefs(card));
		}
	}
    mInitialized = PR_TRUE;
  }
  return mSubDirectories->Enumerate(result);
}

nsresult nsABCard::AddSubNode(nsAutoString name, nsIAbCard **childCard)
{
	if(!childCard)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

	if(NS_FAILED(rv))
		return rv;

	nsAutoString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriStr, getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;
	nsCOMPtr<nsIAbCard> card(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;        
	delete[] uriStr;

	mSubDirectories->AppendElement(card);
	*childCard = card;
	NS_ADDREF(*childCard);

    (void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	return rv;
}

NS_IMETHODIMP nsABCard::AddAddrBookListener(nsIAbListener * listener)
{
  if (! mListeners)
	{
		mListeners = new nsVoidArray();
		if(!mListeners)
			return NS_ERROR_OUT_OF_MEMORY;
  }
  mListeners->AppendElement(listener);
  return NS_OK;
}

NS_IMETHODIMP nsABCard::RemoveAddrBookListener(nsIAbListener * listener)
{
  if (! mListeners)
    return NS_OK;
  mListeners->RemoveElement(listener);
  return NS_OK;

}

NS_IMETHODIMP nsABCard::GetPersonName(char **name)
{
	nsString tempName;
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1"))
		tempName.Append("John");
    if (!PL_strcmp(mURI, "abcard://Pab1/Card2"))
		tempName.Append("Mary");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card1"))
		tempName.Append("Lisa");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card2"))
		tempName.Append("Frank");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card1"))
		tempName.Append("Teri");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card2"))
		tempName.Append("Ted");
	*name = tempName.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetListName(char **listname)
{
	nsString tempName;
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1"))
		tempName.Append("John");
    if (!PL_strcmp(mURI, "abcard://Pab1/Card2"))
		tempName.Append("Mary");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card1"))
		tempName.Append("Lisa");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card2"))
		tempName.Append("Frank");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card1"))
		tempName.Append("Teri");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card2"))
		tempName.Append("Ted");
	*listname = tempName.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetEmail(char **email)
{
	nsString tempName;
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1"))
		tempName.Append("john@foo.com");
    if (!PL_strcmp(mURI, "abcard://Pab1/Card2"))
		tempName.Append("mary@foo.com");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card1"))
		tempName.Append("lisa@foo.com");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card2"))
		tempName.Append("frank@foo.com");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card1"))
		tempName.Append("teri@foo.com");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card2"))
		tempName.Append("ted@foo.com");
	*email = tempName.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetCity(char **city)
{
	nsString tempName;
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1"))
		tempName.Append("Mountian View");
    if (!PL_strcmp(mURI, "abcard://Pab1/Card2"))
		tempName.Append("San Francisco");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card1"))
		tempName.Append("San Jose");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card2"))
		tempName.Append("San Jose");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card1"))
		tempName.Append("Sunnyvale");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card2"))
		tempName.Append("Sunnyvale");
	*city = tempName.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetFirstName(char * *aFirstName)
{
	if (aFirstName && m_pFirstName)
		*aFirstName = PL_strdup(m_pFirstName);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetLastName(char * *aLastName)
{
	if (aLastName && m_pLastName)
		*aLastName = PL_strdup(m_pLastName);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetDisplayName(char * *aDisplayName)
{
	if (aDisplayName && m_pDisplayName)
		*aDisplayName = PL_strdup(m_pDisplayName);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetPrimaryEmail(char * *aPrimaryEmail)
{
	if (aPrimaryEmail && m_pPrimaryEmail)
		*aPrimaryEmail = PL_strdup(m_pPrimaryEmail);
	return NS_OK;
}
NS_IMETHODIMP nsABCard::GetSecondEmail(char * *aSecondEmail)
{
	if (aSecondEmail && m_pSecondEmail)
		*aSecondEmail = PL_strdup(m_pSecondEmail);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetWorkPhone(char * *aWorkPhone)
{
	nsString tempName;
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1"))
		tempName.Append("1111");
    if (!PL_strcmp(mURI, "abcard://Pab1/Card2"))
		tempName.Append("6666");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card1"))
		tempName.Append("2222");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card2"))
		tempName.Append("4444");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card1"))
		tempName.Append("7777");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card2"))
		tempName.Append("3333");
	*aWorkPhone = tempName.ToNewCString();
	return NS_OK;

	if (aWorkPhone && m_pWorkPhone)
		*aWorkPhone = PL_strdup(m_pWorkPhone);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetHomePhone(char * *aHomePhone)
{
	if (aHomePhone && m_pHomePhone)
		*aHomePhone = PL_strdup(m_pHomePhone);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetFaxNumber(char * *aFaxNumber)
{
	if (aFaxNumber && m_pFaxNumber)
		*aFaxNumber = PL_strdup(m_pFaxNumber);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetPagerNumber(char * *aPagerNumber)
{
	if (aPagerNumber && m_pPagerNumber)
		*aPagerNumber = PL_strdup(m_pPagerNumber);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetCellularNumber(char * *aCellularNumber)
{
	if (aCellularNumber && m_pCellularNumber)
		*aCellularNumber = PL_strdup(m_pCellularNumber);
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetWorkCity(char * *aWorkCity)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::GetOrganization(char * *aOrganization)
{
	nsString tempName;
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1"))
		tempName.Append("Market");
    if (!PL_strcmp(mURI, "abcard://Pab1/Card2"))
		tempName.Append("Sales");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card1"))
		tempName.Append("Engineer");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card2"))
		tempName.Append("Finance");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card1"))
		tempName.Append("Human Resource");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card2"))
		tempName.Append("Payroll");
	*aOrganization = tempName.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetNickName(char * *aNickName)
{
	nsString tempName;
    if (!PL_strcmp(mURI, "abcard://Pab1/Card1"))
		tempName.Append("John");
    if (!PL_strcmp(mURI, "abcard://Pab1/Card2"))
		tempName.Append("Mary");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card1"))
		tempName.Append("Lisa");
    if (!PL_strcmp(mURI, "abcard://Pab2/Card2"))
		tempName.Append("Frank");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card1"))
		tempName.Append("Teri");
    if (!PL_strcmp(mURI, "abcard://Pab3/Card2"))
		tempName.Append("Ted");
	*aNickName = tempName.ToNewCString();
	return NS_OK;
}


NS_IMETHODIMP nsABCard::SetPersonName(char * aPersonName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::SetListName(char * aListName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::SetEmail(char * aEmail)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::SetCity(char * aCity)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::SetFirstName(char * aFirstName)
{
	if (aFirstName)
	{
		PR_FREEIF(m_pFirstName);
		m_pFirstName = PL_strdup(aFirstName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetLastName(char * aLastName)
{
	if (aLastName)
	{
		PR_FREEIF(m_pLastName);
		m_pLastName = PL_strdup(aLastName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetDisplayName(char * aDisplayName)
{
	if (aDisplayName)
	{
		PR_FREEIF(m_pDisplayName);
		m_pDisplayName = PL_strdup(aDisplayName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetPrimaryEmail(char * aPrimaryEmail)
{
	if (aPrimaryEmail)
	{
		PR_FREEIF(m_pPrimaryEmail);
		m_pPrimaryEmail = PL_strdup(aPrimaryEmail);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetSecondEmail(char * aSecondEmail)
{
	if (aSecondEmail)
	{
		PR_FREEIF(m_pSecondEmail);
		m_pSecondEmail = PL_strdup(aSecondEmail);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetWorkPhone(char * aWorkPhone)
{
	if (aWorkPhone)
	{
		PR_FREEIF(m_pWorkPhone);
		m_pWorkPhone = PL_strdup(aWorkPhone);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetHomePhone(char * aHomePhone)
{
	if (aHomePhone)
	{
		PR_FREEIF(m_pHomePhone);
		m_pHomePhone = PL_strdup(aHomePhone);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetFaxNumber(char * aFaxNumber)
{
	if (aFaxNumber)
	{
		PR_FREEIF(m_pFaxNumber);
		m_pFaxNumber = PL_strdup(aFaxNumber);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetPagerNumber(char * aPagerNumber)
{
	if (aPagerNumber)
	{
		PR_FREEIF(m_pPagerNumber);
		m_pPagerNumber = PL_strdup(aPagerNumber);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetCellularNumber(char * aCellularNumber)
{
	if (aCellularNumber)
	{
		PR_FREEIF(m_pCellularNumber);
		m_pCellularNumber = PL_strdup(aCellularNumber);
	}
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetWorkCity(char * aWorkCity)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::SetOrganization(char * aOrganization)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::SetNickName(char * aNickName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::GetDbRowID(PRUint32 *aDbRowID)
{
	*aDbRowID = m_dbRowID;
	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetDbRowID(PRUint32 aDbRowID)
{
	m_dbRowID = aDbRowID;
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetCardValue(const char *attrname, char **value)
{
    if (!PL_strcmp(attrname, "firstname"))
		GetFirstName(value);
    else if (!PL_strcmp(attrname, "lastname"))
		GetLastName(value);
    else if (!PL_strcmp(attrname, "displayname"))
		GetDisplayName(value);
    else if (!PL_strcmp(attrname, "nickname"))
		GetNickName(value);
    else if (!PL_strcmp(attrname, "primaryemail"))
		GetPrimaryEmail(value);
    else if (!PL_strcmp(attrname, "secondemail"))
		GetSecondEmail(value);
    else if (!PL_strcmp(attrname, "workphone"))
		GetWorkPhone(value);
    else if (!PL_strcmp(attrname, "homephone"))
		GetHomePhone(value);
    else if (!PL_strcmp(attrname, "faxnumber"))
		GetFaxNumber(value);
    else if (!PL_strcmp(attrname, "pagernumber"))
		GetPagerNumber(value);
    else if (!PL_strcmp(attrname, "cellularnumber"))
		GetCellularNumber(value);

	return NS_OK;
}

NS_IMETHODIMP nsABCard::SetCardValue(const char *attrname, const char *value)
{
	nsAutoString cardValue(value);
	char* valueStr = cardValue.ToNewCString();
    if (!PL_strcmp(attrname, "firstname"))
		SetFirstName(valueStr);
    else if (!PL_strcmp(attrname, "lastname"))
		SetLastName(valueStr);
    else if (!PL_strcmp(attrname, "displayname"))
		SetDisplayName(valueStr);
    else if (!PL_strcmp(attrname, "nickname"))
		SetNickName(valueStr);
    else if (!PL_strcmp(attrname, "primaryemail"))
		SetPrimaryEmail(valueStr);
    else if (!PL_strcmp(attrname, "secondemail"))
		SetSecondEmail(valueStr);
    else if (!PL_strcmp(attrname, "workphone"))
		SetWorkPhone(valueStr);
    else if (!PL_strcmp(attrname, "homephone"))
		SetHomePhone(valueStr);
    else if (!PL_strcmp(attrname, "faxnumber"))
		SetFaxNumber(valueStr);
    else if (!PL_strcmp(attrname, "pagernumber"))
		SetPagerNumber(valueStr);
    else if (!PL_strcmp(attrname, "cellularnumber"))
		SetCellularNumber(valueStr);

	delete[] valueStr;

	return NS_OK;
}

NS_IMETHODIMP nsABCard::AddCardToDatabase()
{
	// find out which database, which directory to add
	// get RDF directory selected node

	nsresult openAddrDB = NS_OK;
	if (!mDatabase)
	{
		nsresult rv = NS_ERROR_FAILURE;

		NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
		if (NS_FAILED(rv))
			return rv;

		nsIFileSpec* userdir;
		rv = locator->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, &userdir);
		if (NS_FAILED(rv))
			return rv;
		nsServiceManager::ReleaseService(kFileLocatorCID, locator);
		
		nsFileSpec dbPath;
		userdir->GetFileSpec(&dbPath);
		dbPath += "abook.mab";

		nsCOMPtr<nsIAddrDatabase> addrDBFactory;
		rv = nsComponentManager::CreateInstance(kAddressBookDB, nsnull, nsIAddrDatabase::GetIID(), 
												(void **) getter_AddRefs(addrDBFactory));
		if (NS_SUCCEEDED(rv) && addrDBFactory)
			openAddrDB = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(mDatabase), PR_TRUE);

		if (mDatabase)
		{
			mDatabase->AddListener(this);
			mDatabase->CreateNewCardAndAddToDB(this, PR_TRUE);
			mDatabase->Close(PR_TRUE);
		}
	}
	return NS_OK;

}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsABCard::GetName(char **name)
{
  SetName("Personal Address Book");
  *name = mCardName.ToNewCString();
  return NS_OK;
}

NS_IMETHODIMP nsABCard::SetName(char * name)
{
	mCardName = name;
	return NS_OK;
}

NS_IMETHODIMP nsABCard::GetParent(nsIAbBase* *parent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::SetParent(nsIAbBase *parent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::GetChildNamed(const char* name, nsISupports* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABCard::ContainsChildNamed(const char *name, PRBool* containsChild)
{
	nsCOMPtr<nsISupports> child;
	
	if(containsChild)
	{
		*containsChild = PR_FALSE;
		if(NS_SUCCEEDED(GetChildNamed(name, getter_AddRefs(child))))
		{
			*containsChild = child != nsnull;
		}
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsABCard::FindParentOf(nsIAbCard * aDirectory, nsIAbCard ** aParent)
{
	if(!aParent)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	*aParent = nsnull;

	PRUint32 i, j, count;
	rv = mSubDirectories->Count(&count);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
	nsCOMPtr<nsISupports> supports;
	nsCOMPtr<nsIAbCard> child;
	for (i = 0; i < count && *aParent == NULL; i++)
	{
		supports = getter_AddRefs(mSubDirectories->ElementAt(i));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			if (aDirectory == child.get())
			{
				*aParent = this;
				NS_ADDREF(*aParent);
				return NS_OK;
			}
		}
	}
	for (j = 0; j < count && *aParent == NULL; j++)
	{
/*
		supports = getter_AddRefs(mSubDirectories->ElementAt(j));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			rv = child->FindParentOf(aDirectory, aParent);
			if(NS_SUCCEEDED(rv))
				return rv;
		}
*/
	}

	return rv;
}

NS_IMETHODIMP nsABCard::IsParentOf(nsIAbCard *child, PRBool deep, PRBool *isParent)
{
	if(!isParent)
		return NS_ERROR_NULL_POINTER;
	
	nsresult rv = NS_OK;

	PRUint32 i, count;
	rv = mSubDirectories->Count(&count);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
 
	for (i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubDirectories->ElementAt(i));
		nsCOMPtr<nsIAbCard> directory(do_QueryInterface(supports, &rv));
		if(NS_SUCCEEDED(rv))
		{
			if (directory.get() == child )
				*isParent = PR_TRUE;
			else if(deep)
			{;
//				directory->IsParentOf(child, deep, isParent);
			}
		}
		if(*isParent)
			return NS_OK;
    }
	*isParent = PR_FALSE;
	return rv;
}


#ifdef HAVE_DB
NS_IMETHOD GetTotalPersonsInDB(PRUint32 *totalPersons) const;					// How many messages in database.
#endif
	
