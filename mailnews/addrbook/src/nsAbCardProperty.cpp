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
#include "rdf.h"

#include "nsAddrDatabase.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddressBookDB, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

/* The definition is nsAddrDatabase.cpp */
extern const char *kFirstNameColumn;
extern const char *kLastNameColumn;
extern const char *kDisplayNameColumn;
extern const char *kNicknameColumn;
extern const char *kPriEmailColumn;
extern const char *k2ndEmailColumn;
extern const char *kPlainTextColumn;
extern const char *kWorkPhoneColumn;
extern const char *kHomePhoneColumn;
extern const char *kFaxColumn;
extern const char *kPagerColumn;
extern const char *kCellularColumn;
extern const char *kHomeAddressColumn;
extern const char *kHomeAddress2Column;
extern const char *kHomeCityColumn;
extern const char *kHomeStateColumn;
extern const char *kHomeZipCodeColumn;
extern const char *kHomeCountryColumn;
extern const char *kWorkAddressColumn;
extern const char *kWorkAddress2Column;
extern const char *kWorkCityColumn;
extern const char *kWorkStateColumn;
extern const char *kWorkZipCodeColumn;
extern const char *kWorkCountryColumn;
extern const char *kJobTitleColumn;
extern const char *kDepartmentColumn;
extern const char *kCompanyColumn;
extern const char *kWebPage1Column;
extern const char *kWebPage2Column;
extern const char *kBirthYearColumn;
extern const char *kBirthMonthColumn;
extern const char *kBirthDayColumn;
extern const char *kCustom1Column;
extern const char *kCustom2Column;
extern const char *kCustom3Column;
extern const char *kCustom4Column;
extern const char *kNotesColumn;
/* end */

nsAbCardProperty::nsAbCardProperty(void)
{
	NS_INIT_REFCNT();

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

	m_pHomeAddress = nsnull;
	m_pHomeAddress2 = nsnull;
	m_pHomeCity = nsnull;
	m_pHomeState = nsnull;
	m_pHomeZipCode = nsnull;
	m_pHomeCountry = nsnull;
	m_pWorkAddress = nsnull;
	m_pWorkAddress2 = nsnull;
	m_pWorkCity = nsnull;
	m_pWorkState = nsnull;
	m_pWorkZipCode = nsnull;
	m_pWorkCountry = nsnull;
	m_pJobTitle = nsnull;
	m_pDepartment = nsnull;
	m_pCompany = nsnull;
	m_pWebPage1 = nsnull;
	m_pWebPage2 = nsnull;
	m_pBirthYear = nsnull;
	m_pBirthMonth = nsnull;
	m_pBirthDay = nsnull;
	m_pCustom1 = nsnull;
	m_pCustom2 = nsnull;
	m_pCustom3 = nsnull;
	m_pCustom4 = nsnull;
	m_pNote = nsnull;

	m_bSendPlainText = PR_FALSE;

	m_dbTableID = 0;
	m_dbRowID = 0;
}

nsAbCardProperty::~nsAbCardProperty(void)
{
	if(mDatabase)
		mDatabase->RemoveListener(this);

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
	PR_FREEIF(m_pHomeAddress);
	PR_FREEIF(m_pHomeAddress2);
	PR_FREEIF(m_pHomeCity);
	PR_FREEIF(m_pHomeState);
	PR_FREEIF(m_pHomeZipCode);
	PR_FREEIF(m_pHomeCountry);
	PR_FREEIF(m_pWorkAddress);
	PR_FREEIF(m_pWorkAddress2);
	PR_FREEIF(m_pWorkCity);
	PR_FREEIF(m_pWorkState);
	PR_FREEIF(m_pWorkZipCode);
	PR_FREEIF(m_pWorkCountry);
	PR_FREEIF(m_pJobTitle);
	PR_FREEIF(m_pDepartment);
	PR_FREEIF(m_pCompany);
	PR_FREEIF(m_pWebPage1);
	PR_FREEIF(m_pWebPage2);
	PR_FREEIF(m_pBirthYear);
	PR_FREEIF(m_pBirthMonth);
	PR_FREEIF(m_pBirthDay);
	PR_FREEIF(m_pCustom1);
	PR_FREEIF(m_pCustom2);
	PR_FREEIF(m_pCustom3);
	PR_FREEIF(m_pCustom4);
	PR_FREEIF(m_pNote);

}

NS_IMPL_ADDREF(nsAbCardProperty)
NS_IMPL_RELEASE(nsAbCardProperty)

NS_IMETHODIMP nsAbCardProperty::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsCOMTypeInfo<nsIAbCard>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIAddrDBListener>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
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

nsresult nsAbCardProperty::GetAttributeName(char **aName, char* pValue)
{
	if (aName)
	{
		if (pValue)
			*aName = PL_strdup(pValue);
		else
			*aName = PL_strdup("");
		return NS_OK;
	}
	else
		return NS_RDF_NO_VALUE;

}

nsresult nsAbCardProperty::SetAttributeName(char *aName, char **arrtibute)
{
	if (aName)
	{
		char *pValue = *arrtibute;
		PR_FREEIF(pValue);
		*arrtibute = PL_strdup(aName);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetSendPlainText(PRBool *aSendPlainText)
{
	*aSendPlainText = m_bSendPlainText;
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetSendPlainText(PRBool aSendPlainText)
{
	m_bSendPlainText = aSendPlainText;
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
    if (!PL_strcmp(attrname, kFirstNameColumn))
		GetFirstName(value);
    else if (!PL_strcmp(attrname, kLastNameColumn))
		GetLastName(value);
    else if (!PL_strcmp(attrname, kDisplayNameColumn))
		GetDisplayName(value);
    else if (!PL_strcmp(attrname, kNicknameColumn))
		GetNickName(value);
    else if (!PL_strcmp(attrname, kPriEmailColumn))
		GetPrimaryEmail(value);
    else if (!PL_strcmp(attrname, k2ndEmailColumn))
		GetSecondEmail(value);
    else if (!PL_strcmp(attrname, kWorkPhoneColumn))
		GetWorkPhone(value);
    else if (!PL_strcmp(attrname, kHomePhoneColumn))
		GetHomePhone(value);
    else if (!PL_strcmp(attrname, kFaxColumn))
		GetFaxNumber(value);
    else if (!PL_strcmp(attrname, kPagerColumn))
		GetPagerNumber(value);
    else if (!PL_strcmp(attrname, kCellularColumn))
		GetCellularNumber(value);
    else if (!PL_strcmp(attrname, kHomeAddressColumn))
		GetHomeAddress(value);
    else if (!PL_strcmp(attrname, kHomeAddress2Column))
		GetHomeAddress2(value);
    else if (!PL_strcmp(attrname, kHomeCityColumn))
		GetHomeCity(value);
    else if (!PL_strcmp(attrname, kHomeStateColumn))
		GetHomeState(value);
    else if (!PL_strcmp(attrname, kHomeZipCodeColumn))
		GetHomeZipCode(value);
    else if (!PL_strcmp(attrname, kHomeCountryColumn))
		GetHomeCountry(value);
    else if (!PL_strcmp(attrname, kWorkAddressColumn))
		GetWorkAddress(value);
    else if (!PL_strcmp(attrname, kWorkAddress2Column))
		GetWorkAddress2(value);
    else if (!PL_strcmp(attrname, kWorkCityColumn))
		GetWorkCity(value);
    else if (!PL_strcmp(attrname, kWorkStateColumn))
		GetWorkState(value);
    else if (!PL_strcmp(attrname, kWorkZipCodeColumn))
		GetWorkZipCode(value);
    else if (!PL_strcmp(attrname, kWorkCountryColumn))
		GetWorkCountry(value);
    else if (!PL_strcmp(attrname, kWebPage1Column))
		GetWebPage1(value);
    else if (!PL_strcmp(attrname, kWebPage2Column))
		GetWebPage2(value);
    else if (!PL_strcmp(attrname, kBirthYearColumn))
		GetBirthYear(value);
    else if (!PL_strcmp(attrname, kBirthMonthColumn))
		GetBirthMonth(value);
    else if (!PL_strcmp(attrname, kBirthDayColumn))
		GetBirthDay(value);
    else if (!PL_strcmp(attrname, kCustom1Column))
		GetCustom1(value);
    else if (!PL_strcmp(attrname, kCustom2Column))
		GetCustom2(value);
    else if (!PL_strcmp(attrname, kCustom3Column))
		GetCustom3(value);
    else if (!PL_strcmp(attrname, kCustom4Column))
		GetCustom4(value);
    else if (!PL_strcmp(attrname, kNotesColumn))
		GetNotes(value);
	/* else handle pass down attribute */

	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetCardValue(const char *attrname, const char *value)
{
	nsAutoString cardValue(value);
	char* valueStr = cardValue.ToNewCString();

    if (!PL_strcmp(attrname, kFirstNameColumn))
		SetFirstName(valueStr);
    else if (!PL_strcmp(attrname, kLastNameColumn))
		SetLastName(valueStr);
    else if (!PL_strcmp(attrname, kDisplayNameColumn))
		SetDisplayName(valueStr);
    else if (!PL_strcmp(attrname, kNicknameColumn))
		SetNickName(valueStr);
    else if (!PL_strcmp(attrname, kPriEmailColumn))
		SetPrimaryEmail(valueStr);
    else if (!PL_strcmp(attrname, k2ndEmailColumn))
		SetSecondEmail(valueStr);
    else if (!PL_strcmp(attrname, kWorkPhoneColumn))
		SetWorkPhone(valueStr);
    else if (!PL_strcmp(attrname, kHomePhoneColumn))
		SetHomePhone(valueStr);
    else if (!PL_strcmp(attrname, kFaxColumn))
		SetFaxNumber(valueStr);
    else if (!PL_strcmp(attrname, kPagerColumn))
		SetPagerNumber(valueStr);
    else if (!PL_strcmp(attrname, kCellularColumn))
		SetCellularNumber(valueStr);
    else if (!PL_strcmp(attrname, kHomeAddressColumn))
		SetHomeAddress(valueStr);
    else if (!PL_strcmp(attrname, kHomeAddress2Column))
		SetHomeAddress2(valueStr);
    else if (!PL_strcmp(attrname, kHomeCityColumn))
		SetHomeCity(valueStr);
    else if (!PL_strcmp(attrname, kHomeStateColumn))
		SetHomeState(valueStr);
    else if (!PL_strcmp(attrname, kHomeZipCodeColumn))
		SetHomeZipCode(valueStr);
    else if (!PL_strcmp(attrname, kHomeCountryColumn))
		SetHomeCountry(valueStr);
    else if (!PL_strcmp(attrname, kWorkAddressColumn))
		SetWorkAddress(valueStr);
    else if (!PL_strcmp(attrname, kWorkAddress2Column))
		SetWorkAddress2(valueStr);
    else if (!PL_strcmp(attrname, kWorkCityColumn))
		SetWorkCity(valueStr);
    else if (!PL_strcmp(attrname, kWorkStateColumn))
		SetWorkState(valueStr);
    else if (!PL_strcmp(attrname, kWorkZipCodeColumn))
		SetWorkZipCode(valueStr);
    else if (!PL_strcmp(attrname, kWorkCountryColumn))
		SetWorkCountry(valueStr);
    else if (!PL_strcmp(attrname, kWebPage1Column))
		SetWebPage1(valueStr);
    else if (!PL_strcmp(attrname, kWebPage2Column))
		SetWebPage2(valueStr);
    else if (!PL_strcmp(attrname, kBirthYearColumn))
		SetBirthYear(valueStr);
    else if (!PL_strcmp(attrname, kBirthMonthColumn))
		SetBirthMonth(valueStr);
    else if (!PL_strcmp(attrname, kBirthDayColumn))
		SetBirthDay(valueStr);
    else if (!PL_strcmp(attrname, kCustom1Column))
		SetCustom1(valueStr);
    else if (!PL_strcmp(attrname, kCustom2Column))
		SetCustom2(valueStr);
    else if (!PL_strcmp(attrname, kCustom3Column))
		SetCustom3(valueStr);
    else if (!PL_strcmp(attrname, kCustom4Column))
		SetCustom4(valueStr);
    else if (!PL_strcmp(attrname, kNotesColumn))
		SetNotes(valueStr);
	/* else handle pass down attribute */

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
