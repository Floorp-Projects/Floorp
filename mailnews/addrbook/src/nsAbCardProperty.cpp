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

#include "nsAbCardProperty.h"	 
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsXPIDLString.h"
#include "nsAbBaseCID.h"
#include "prmem.h"	 
#include "prlog.h"	 

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddressBookDB, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsAbCardProperty::nsAbCardProperty(void)
{
	NS_INIT_REFCNT();

	m_pListName = nsnull;
	m_pWorkCity = nsnull;
	m_pOrganization = nsnull;

	m_pFirstName = nsnull;
	m_pLastName = nsnull;
	m_pDisplayName = nsnull;
	m_pNickName = nsnull;
	m_pPrimaryEmail = nsnull;
	m_pSecondEmail = nsnull;
	m_pWorkPhone = nsnull;
	m_pHomePhone = nsnull;
	m_pFaxNumber = nsnull;
	m_pPagerNumber = nsnull;
	m_pCellularNumber = nsnull;

	m_dbTableID = -1;
	m_dbRowID = -1;
}

nsAbCardProperty::~nsAbCardProperty(void)
{
	if(mDatabase)
		mDatabase->RemoveListener(this);

	PR_FREEIF(m_pListName);
	PR_FREEIF(m_pWorkCity);
	PR_FREEIF(m_pOrganization);

	PR_FREEIF(m_pFirstName);
	PR_FREEIF(m_pLastName);
	PR_FREEIF(m_pDisplayName);
	PR_FREEIF(m_pNickName);
	PR_FREEIF(m_pPrimaryEmail);
	PR_FREEIF(m_pSecondEmail);
	PR_FREEIF(m_pWorkPhone);
	PR_FREEIF(m_pHomePhone);
	PR_FREEIF(m_pFaxNumber);
	PR_FREEIF(m_pPagerNumber);
	PR_FREEIF(m_pCellularNumber);
}

NS_IMPL_ADDREF(nsAbCardProperty)
NS_IMPL_RELEASE(nsAbCardProperty)

NS_IMETHODIMP nsAbCardProperty::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsIAbCard::GetIID()) ||
        aIID.Equals(nsIAddrDBListener::GetIID()) ||
        aIID.Equals(::nsISupports::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIAbCard*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAbCardProperty::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::OnCardEntryChange
(PRUint32 abCode, PRUint32 entryID, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetListName(char **listname)
{
	SetListName("List1");
	if (listname && m_pListName)
		*listname = PL_strdup(m_pListName);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetWorkCity(char **city)
{
	SetWorkCity("Mountain View");
	if (city && m_pWorkCity)
		*city = PL_strdup(m_pWorkCity);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetFirstName(char * *aFirstName)
{
	if (aFirstName && m_pFirstName)
		*aFirstName = PL_strdup(m_pFirstName);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetLastName(char * *aLastName)
{
	if (aLastName && m_pLastName)
		*aLastName = PL_strdup(m_pLastName);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetDisplayName(char * *aDisplayName)
{
	if (aDisplayName && m_pDisplayName)
		*aDisplayName = PL_strdup(m_pDisplayName);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetPrimaryEmail(char * *aPrimaryEmail)
{
	if (aPrimaryEmail && m_pPrimaryEmail)
		*aPrimaryEmail = PL_strdup(m_pPrimaryEmail);
	return NS_OK;
}
NS_IMETHODIMP nsAbCardProperty::GetSecondEmail(char * *aSecondEmail)
{
	if (aSecondEmail && m_pSecondEmail)
		*aSecondEmail = PL_strdup(m_pSecondEmail);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetWorkPhone(char * *aWorkPhone)
{
	if (aWorkPhone && m_pWorkPhone)
		*aWorkPhone = PL_strdup(m_pWorkPhone);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetHomePhone(char * *aHomePhone)
{
	if (aHomePhone && m_pHomePhone)
		*aHomePhone = PL_strdup(m_pHomePhone);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetFaxNumber(char * *aFaxNumber)
{
	if (aFaxNumber && m_pFaxNumber)
		*aFaxNumber = PL_strdup(m_pFaxNumber);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetPagerNumber(char * *aPagerNumber)
{
	if (aPagerNumber && m_pPagerNumber)
		*aPagerNumber = PL_strdup(m_pPagerNumber);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetCellularNumber(char * *aCellularNumber)
{
	if (aCellularNumber && m_pCellularNumber)
		*aCellularNumber = PL_strdup(m_pCellularNumber);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetOrganization(char * *aOrganization)
{
	SetOrganization("Mail");
	if (aOrganization && m_pOrganization)
		*aOrganization = PL_strdup(m_pOrganization);
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetNickName(char * *aNickName)
{
	if (aNickName && m_pNickName)
		*aNickName = PL_strdup(m_pNickName);
	return NS_OK;
}


NS_IMETHODIMP nsAbCardProperty::SetListName(char * aListName)
{
	if (aListName)
	{
		PR_FREEIF(m_pListName);
		m_pListName = PL_strdup(aListName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetFirstName(char * aFirstName)
{
	if (aFirstName)
	{
		PR_FREEIF(m_pFirstName);
		m_pFirstName = PL_strdup(aFirstName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetLastName(char * aLastName)
{
	if (aLastName)
	{
		PR_FREEIF(m_pLastName);
		m_pLastName = PL_strdup(aLastName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetDisplayName(char * aDisplayName)
{
	if (aDisplayName)
	{
		PR_FREEIF(m_pDisplayName);
		m_pDisplayName = PL_strdup(aDisplayName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetPrimaryEmail(char * aPrimaryEmail)
{
	if (aPrimaryEmail)
	{
		PR_FREEIF(m_pPrimaryEmail);
		m_pPrimaryEmail = PL_strdup(aPrimaryEmail);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetSecondEmail(char * aSecondEmail)
{
	if (aSecondEmail)
	{
		PR_FREEIF(m_pSecondEmail);
		m_pSecondEmail = PL_strdup(aSecondEmail);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetWorkPhone(char * aWorkPhone)
{
	if (aWorkPhone)
	{
		PR_FREEIF(m_pWorkPhone);
		m_pWorkPhone = PL_strdup(aWorkPhone);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetHomePhone(char * aHomePhone)
{
	if (aHomePhone)
	{
		PR_FREEIF(m_pHomePhone);
		m_pHomePhone = PL_strdup(aHomePhone);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetFaxNumber(char * aFaxNumber)
{
	if (aFaxNumber)
	{
		PR_FREEIF(m_pFaxNumber);
		m_pFaxNumber = PL_strdup(aFaxNumber);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetPagerNumber(char * aPagerNumber)
{
	if (aPagerNumber)
	{
		PR_FREEIF(m_pPagerNumber);
		m_pPagerNumber = PL_strdup(aPagerNumber);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetCellularNumber(char * aCellularNumber)
{
	if (aCellularNumber)
	{
		PR_FREEIF(m_pCellularNumber);
		m_pCellularNumber = PL_strdup(aCellularNumber);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetWorkCity(char * aWorkCity)
{
	if (aWorkCity)
	{
		PR_FREEIF(m_pWorkCity);
		m_pWorkCity = PL_strdup(aWorkCity);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetOrganization(char * aOrganization)
{
	if (aOrganization)
	{
		PR_FREEIF(m_pOrganization);
		m_pOrganization = PL_strdup(aOrganization);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetNickName(char * aNickName)
{
	if (aNickName)
	{
		PR_FREEIF(m_pNickName);
		m_pNickName = PL_strdup(aNickName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetDbTableID(PRUint32 *aDbTableID)
{
	*aDbTableID = m_dbTableID;
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetDbTableID(PRUint32 aDbTableID)
{
	m_dbTableID = aDbTableID;
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetDbRowID(PRUint32 *aDbRowID)
{
	*aDbRowID = m_dbRowID;
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetDbRowID(PRUint32 aDbRowID)
{
	m_dbRowID = aDbRowID;
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetCardValue(const char *attrname, char **value)
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

NS_IMETHODIMP nsAbCardProperty::SetCardValue(const char *attrname, const char *value)
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

NS_IMETHODIMP nsAbCardProperty::AddCardToDatabase()
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
		
		nsFileSpec dbPath;
		userdir->GetFileSpec(&dbPath);
		dbPath += "abook.mab";

		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDB, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			openAddrDB = addrDBFactory->Open(&dbPath, PR_TRUE, getter_AddRefs(mDatabase), PR_TRUE);

		if (mDatabase)
		{
			mDatabase->CreateNewCardAndAddToDB(this, PR_TRUE);
			mDatabase->Close(PR_TRUE);
			mDatabase = null_nsCOMPtr();
		}
	}
	return NS_OK;

}
