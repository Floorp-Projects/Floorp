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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsAbCardProperty.h"	 
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsAbBaseCID.h"
#include "prmem.h"	 
#include "prlog.h"	 
#include "prprf.h"	 
#include "rdf.h"
#include "nsCOMPtr.h"

#include "nsAddrDatabase.h"
#include "nsIAddrBookSession.h"
#include "nsIPref.h"
#include "nsIAddressBook.h"

static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);


/* The definition is nsAddressBook.cpp */
extern const char *kDirectoryDataSourceRoot;
extern const char *kCardDataSourceRoot;

/* The definition is nsAddrDatabase.cpp */
extern const char *kMainPersonalAddressBook;
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
extern const char *kLastModifiedDateColumn;
/* end */

nsAbCardProperty::nsAbCardProperty(void)
{
	NS_INIT_REFCNT();

	m_LastModDate = 0;
	m_Key = 0;

	m_bSendPlainText = PR_TRUE;
	m_bIsMailList = PR_FALSE;

	m_dbTableID = 0;
	m_dbRowID = 0;

	m_pAnonymousStrAttributes = nsnull;
	m_pAnonymousStrValues = nsnull;
	m_pAnonymousIntAttributes = nsnull;
	m_pAnonymousIntValues = nsnull;
	m_pAnonymousBoolAttributes = nsnull;
	m_pAnonymousBoolValues = nsnull;
}

nsAbCardProperty::~nsAbCardProperty(void)
{
	if (m_pAnonymousStrAttributes)
		RemoveAnonymousList(m_pAnonymousStrAttributes);
	if (m_pAnonymousIntAttributes)
		RemoveAnonymousList(m_pAnonymousIntAttributes);
	if (m_pAnonymousBoolAttributes)
		RemoveAnonymousList(m_pAnonymousBoolAttributes);

	if (m_pAnonymousStrValues)
		RemoveAnonymousList(m_pAnonymousStrValues);
	if (m_pAnonymousIntValues)
		RemoveAnonymousList(m_pAnonymousIntValues);
	if (m_pAnonymousBoolValues)
		RemoveAnonymousList(m_pAnonymousBoolValues);

	if (mCardDatabase)
		mCardDatabase = null_nsCOMPtr();
}

nsresult nsAbCardProperty::RemoveAnonymousList(nsVoidArray* pArray)
{
	if (pArray)
	{
		PRUint32 count = pArray->Count();
		for (int i = count - 1; i >= 0; i--)
		{
			void* pPtr = pArray->ElementAt(i);
			PR_FREEIF(pPtr);
			pArray->RemoveElementAt(i);
		}
		delete pArray;
	}
	return NS_OK;
}

NS_IMPL_ADDREF(nsAbCardProperty)
NS_IMPL_RELEASE(nsAbCardProperty)

NS_IMETHODIMP nsAbCardProperty::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(NS_GET_IID(nsIAbCard)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsIAbCard*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

////////////////////////////////////////////////////////////////////////////////

nsresult nsAbCardProperty::GetAttributeName(PRUnichar **aName, nsString& value)
{
	if (aName)
	{
		*aName = value.ToNewUnicode();
		if (!(*aName)) 
			return NS_ERROR_OUT_OF_MEMORY;
		else
			return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;

}

nsresult nsAbCardProperty::SetAttributeName(const PRUnichar *aName, nsString& arrtibute)
{
	if (aName)
		arrtibute = aName;
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

NS_IMETHODIMP nsAbCardProperty::GetIsMailList(PRBool *aIsMailList)
{
	*aIsMailList = m_bIsMailList;
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetIsMailList(PRBool aIsMailList)
{
	m_bIsMailList = aIsMailList;
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

NS_IMETHODIMP nsAbCardProperty::GetCardValue(const char *attrname, PRUnichar **value)
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
    else if (!PL_strcmp(attrname, kDepartmentColumn))
		GetDepartment(value);
    else if (!PL_strcmp(attrname, kCompanyColumn))
		GetCompany(value);
    else if (!PL_strcmp(attrname, kJobTitleColumn))
		GetJobTitle(value);
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

NS_IMETHODIMP nsAbCardProperty::SetCardValue(const char *attrname, const PRUnichar *value)
{
	if (!attrname && !value)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;

    if (!PL_strcmp(attrname, kFirstNameColumn))
		rv = SetFirstName((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kLastNameColumn))
		rv = SetLastName((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kDisplayNameColumn))
		rv = SetDisplayName((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kNicknameColumn))
		rv = SetNickName((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kPriEmailColumn))
		rv = SetPrimaryEmail((PRUnichar *)value);
    else if (!PL_strcmp(attrname, k2ndEmailColumn))
		rv = SetSecondEmail((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWorkPhoneColumn))
		rv = SetWorkPhone((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kHomePhoneColumn))
		rv = SetHomePhone((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kFaxColumn))
		rv = SetFaxNumber((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kPagerColumn))
		rv = SetPagerNumber((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kCellularColumn))
		rv = SetCellularNumber((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kHomeAddressColumn))
		rv = SetHomeAddress((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kHomeAddress2Column))
		rv = SetHomeAddress2((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kHomeCityColumn))
		rv = SetHomeCity((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kHomeStateColumn))
		rv = SetHomeState((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kHomeZipCodeColumn))
		rv = SetHomeZipCode((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kHomeCountryColumn))
		rv = SetHomeCountry((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWorkAddressColumn))
		rv = SetWorkAddress((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWorkAddress2Column))
		rv = SetWorkAddress2((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWorkCityColumn))
		rv = SetWorkCity((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWorkStateColumn))
		rv = SetWorkState((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWorkZipCodeColumn))
		rv = SetWorkZipCode((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWorkCountryColumn))
		rv = SetWorkCountry((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWebPage1Column))
		rv = SetWebPage1((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kWebPage2Column))
		rv = SetWebPage2((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kBirthYearColumn))
		rv = SetBirthYear((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kBirthMonthColumn))
		rv = SetBirthMonth((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kBirthDayColumn))
		rv = SetBirthDay((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kCustom1Column))
		rv = SetCustom1((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kCustom2Column))
		rv = SetCustom2((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kCustom3Column))
		rv = SetCustom3((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kCustom4Column))
		rv = SetCustom4((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kNotesColumn))
		rv = SetNotes((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kDepartmentColumn))
		rv = SetDepartment((PRUnichar *)value);
    else if (!PL_strcmp(attrname, kCompanyColumn))
		rv = SetCompany((PRUnichar *)value);
	else
	{
		nsAutoString cardValue(value);
    char* valueStr = cardValue.ToNewUTF8String();
		rv = SetAnonymousStringAttribute(attrname, valueStr);
		nsMemory::Free(valueStr);
	}
	return rv;
}

NS_IMETHODIMP nsAbCardProperty::SetAbDatabase(nsIAddrDatabase* database)
{
	mCardDatabase = database;
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetAnonymousStrAttrubutesList(nsVoidArray **attrlist)
{
	if (attrlist && m_pAnonymousStrAttributes)
	{
		*attrlist = m_pAnonymousStrAttributes;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::GetAnonymousStrValuesList(nsVoidArray **valuelist)
{
	if (valuelist && m_pAnonymousStrValues)
	{
		*valuelist = m_pAnonymousStrValues;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::GetAnonymousIntAttrubutesList(nsVoidArray **attrlist)
{
	if (attrlist && m_pAnonymousIntAttributes)
	{
		*attrlist = m_pAnonymousIntAttributes;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::GetAnonymousIntValuesList(nsVoidArray **valuelist)
{
	if (valuelist && m_pAnonymousIntValues)
	{
		*valuelist = m_pAnonymousIntValues;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::GetAnonymousBoolAttrubutesList(nsVoidArray **attrlist)
{
	if (attrlist && m_pAnonymousBoolAttributes)
	{
		*attrlist = m_pAnonymousBoolAttributes;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::GetAnonymousBoolValuesList(nsVoidArray **valuelist)
{
	if (valuelist && m_pAnonymousBoolValues)
	{
		*valuelist = m_pAnonymousBoolValues;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsAbCardProperty::SetAnonymousAttribute
(nsVoidArray** pAttrAray, nsVoidArray** pValueArray, void *attrname, void *value)
{
	nsresult rv = NS_OK;
	nsVoidArray* pAttributes = *pAttrAray;
	nsVoidArray* pValues = *pValueArray; 

	if (!pAttributes && !pValues)
	{
		pAttributes = new nsVoidArray();
		pValues = new nsVoidArray();
		*pAttrAray = pAttributes;
		*pValueArray = pValues;
	}
	if (pAttributes && pValues)
	{
		if (attrname && value)
		{
			pAttributes->AppendElement(attrname);
			pValues->AppendElement(value);
		}
	}
	else
	{ 
		rv = NS_ERROR_FAILURE;
	}

	return rv;
}	


NS_IMETHODIMP nsAbCardProperty::SetAnonymousStringAttribute
(const char *attrname, const char *value)
{
	nsresult rv = NS_OK;

	char* pAttribute = PL_strdup(attrname);
	char* pValue = PL_strdup(value);
	if (pAttribute && pValue)
	{
		rv = SetAnonymousAttribute(&m_pAnonymousStrAttributes, 
			&m_pAnonymousStrValues, pAttribute, pValue);
	}
	else
	{
		PR_FREEIF(pAttribute);
		PR_FREEIF(pValue);
		rv = NS_ERROR_NULL_POINTER;
	}
	return rv;
}	

NS_IMETHODIMP nsAbCardProperty::SetAnonymousIntAttribute
(const char *attrname, PRUint32 value)
{
	nsresult rv = NS_OK;

	char* pAttribute = PL_strdup(attrname);
	PRUint32* pValue = (PRUint32 *)PR_Calloc(1, sizeof(PRUint32));
	*pValue = value;
	if (pAttribute && pValue)
	{
		rv = SetAnonymousAttribute(&m_pAnonymousIntAttributes, 
			&m_pAnonymousIntValues, pAttribute, pValue);
	}
	else
	{
		PR_FREEIF(pAttribute);
		PR_FREEIF(pValue);
		rv = NS_ERROR_NULL_POINTER;
	}
	return rv;
}	

NS_IMETHODIMP nsAbCardProperty::SetAnonymousBoolAttribute
(const char *attrname, PRBool value)
{
	nsresult rv = NS_OK;

	char* pAttribute = PL_strdup(attrname);
	PRBool* pValue = (PRBool *)PR_Calloc(1, sizeof(PRBool));
	*pValue = value;
	if (pAttribute && pValue)
	{
		rv = SetAnonymousAttribute(&m_pAnonymousBoolAttributes, 
			&m_pAnonymousBoolValues, pAttribute, pValue);
	}
	else
	{
		PR_FREEIF(pAttribute);
		PR_FREEIF(pValue);
		rv = NS_ERROR_NULL_POINTER;
	}
	return rv;
}

NS_IMETHODIMP nsAbCardProperty::AddAnonymousAttributesToDB()
{
	nsresult rv = NS_OK;
	if (mCardDatabase)
		mCardDatabase = null_nsCOMPtr();
	rv = GetCardDatabase(kPersonalAddressbookUri);
	if (NS_SUCCEEDED(rv) && mCardDatabase)
		rv = mCardDatabase->AddAnonymousAttributesFromCard(this);
	return rv;
}

NS_IMETHODIMP nsAbCardProperty::EditAnonymousAttributesInDB()
{
	nsresult rv = NS_OK;
	if (mCardDatabase)
		mCardDatabase = null_nsCOMPtr();
	rv = GetCardDatabase(kPersonalAddressbookUri);
	if (NS_SUCCEEDED(rv) && mCardDatabase)
		rv = mCardDatabase->EditAnonymousAttributesFromCard(this);
	return rv;
}

/* caller need to PR_smprintf_free *uri */
NS_IMETHODIMP nsAbCardProperty::GetCardURI(char **uri)
{
	char* cardURI = nsnull;
	nsFileSpec  *filePath = nsnull;
	if (mCardDatabase)
	{
		mCardDatabase->GetDbPath(&filePath);
		if (filePath)
		{
			char* file = nsnull;
			file = filePath->GetLeafName();
			if (file && m_dbRowID)
			{
				if (m_bIsMailList)
					cardURI = PR_smprintf("%s%s/ListCard%ld", kCardDataSourceRoot, file, m_dbRowID);
				else
					cardURI = PR_smprintf("%s%s/Card%ld", kCardDataSourceRoot, file, m_dbRowID);
			}
			if (file)
				nsCRT::free(file);
			delete filePath;
		}
	}
	if (cardURI)
	{
		*uri = cardURI;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsAbCardProperty::GetCardDatabase(const char *uri)
{
	nsresult rv = NS_OK;

	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if (NS_SUCCEEDED(rv))
	{
		nsFileSpec* dbPath;
		abSession->GetUserProfileDirectory(&dbPath);

		const char* file = nsnull;
		file = &(uri[PL_strlen(kDirectoryDataSourceRoot)]);
		(*dbPath) += file;
		
		if (dbPath->Exists())
		{
			NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

			if (NS_SUCCEEDED(rv) && addrDBFactory)
				rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(mCardDatabase), PR_TRUE);
		}
		else
			rv = NS_ERROR_FAILURE;
		delete dbPath;
	}
	return rv;
}

NS_IMETHODIMP nsAbCardProperty::AddCardToDatabase(const char *uri)
{
	nsresult rv = NS_OK;
	PRBool bInMailList = PR_FALSE;

	if (!mCardDatabase && uri)
		rv = GetCardDatabase(uri);
	if (NS_FAILED(rv)) //maillist
	{
		NS_WITH_SERVICE(nsIAddressBook, addresBook, kAddrBookCID, &rv); 
		if (NS_SUCCEEDED(rv))
			rv = addresBook->GetAbDatabaseFromURI(uri, getter_AddRefs(mCardDatabase));
		bInMailList = PR_TRUE;
	}

	if (mCardDatabase)
	{
		if (bInMailList)
		{
			char* listString = PL_strrstr(uri, "MailList");
			if (listString)
			{
				listString += PL_strlen("MailList");
				PRInt32 listID = atoi(listString);
				mCardDatabase->CreateNewListCardAndAddToDB(listID, this, PR_TRUE);
			}
			else
				return NS_ERROR_FAILURE;
		}
		else
			mCardDatabase->CreateNewCardAndAddToDB(this, PR_TRUE);
		mCardDatabase->Commit(kLargeCommit);
		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbCardProperty::DropCardToDatabase(const char *uri)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrDatabase>  dropDatabase;  

	NS_WITH_SERVICE(nsIAddressBook, addresBook, kAddrBookCID, &rv); 
	if (NS_SUCCEEDED(rv))
		rv = addresBook->GetAbDatabaseFromURI(uri, getter_AddRefs(dropDatabase));

	if (dropDatabase)
	{
		dropDatabase->CreateNewCardAndAddToDB(this, PR_FALSE);
		dropDatabase->Commit(kLargeCommit);
		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbCardProperty::EditCardToDatabase(const char *uri)
{
	if (!mCardDatabase && uri)
		GetCardDatabase(uri);

	if (mCardDatabase)
	{
		mCardDatabase->EditCard(this, PR_TRUE);
		mCardDatabase->Commit(kLargeCommit);
		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbCardProperty::CopyCard(nsIAbCard* srcCard)
{
	PRUnichar *str = nsnull;
	srcCard->GetFirstName(&str);
	SetFirstName(str);
	PR_FREEIF(str);

	srcCard->GetLastName(&str);
	SetLastName(str);
	PR_FREEIF(str);
	srcCard->GetDisplayName(&str);
	SetDisplayName(str);
	PR_FREEIF(str);
	srcCard->GetNickName(&str);
	SetNickName(str);
	PR_FREEIF(str);
	srcCard->GetPrimaryEmail(&str);
	SetPrimaryEmail(str);
	PR_FREEIF(str);
	srcCard->GetSecondEmail(&str);
	SetSecondEmail(str);
	PR_FREEIF(str);

	PRBool bValue = PR_FALSE;
	srcCard->GetSendPlainText(&bValue);
	SetSendPlainText(bValue);

	srcCard->GetWorkPhone(&str);
	SetWorkPhone(str);
	PR_FREEIF(str);
	srcCard->GetHomePhone(&str);
	SetHomePhone(str);
	PR_FREEIF(str);
	srcCard->GetFaxNumber(&str);
	SetFaxNumber(str);
	PR_FREEIF(str);
	srcCard->GetPagerNumber(&str);
	SetPagerNumber(str);
	PR_FREEIF(str);
	srcCard->GetCellularNumber(&str);
	SetCellularNumber(str);
	PR_FREEIF(str);
	srcCard->GetHomeAddress(&str);
	SetHomeAddress(str);
	PR_FREEIF(str);
	srcCard->GetHomeAddress2(&str);
	SetHomeAddress2(str);
	PR_FREEIF(str);
	srcCard->GetHomeCity(&str);
	SetHomeCity(str);
	PR_FREEIF(str);
	srcCard->GetHomeState(&str);
	SetHomeState(str);
	PR_FREEIF(str);
	srcCard->GetHomeZipCode(&str);
	SetHomeZipCode(str);
	PR_FREEIF(str);
	srcCard->GetHomeCountry(&str);
	SetHomeCountry(str);
	PR_FREEIF(str);
	srcCard->GetWorkAddress(&str);
	SetWorkAddress(str);
	PR_FREEIF(str);
	srcCard->GetWorkAddress2(&str);
	SetWorkAddress2(str);
	PR_FREEIF(str);
	srcCard->GetWorkCity(&str);
	SetWorkCity(str);
	PR_FREEIF(str);
	srcCard->GetWorkState(&str);
	SetWorkState(str);
	PR_FREEIF(str);
	srcCard->GetWorkZipCode(&str);
	SetWorkZipCode(str);
	PR_FREEIF(str);
	srcCard->GetWorkCountry(&str);
	SetWorkCountry(str);
	PR_FREEIF(str);
	srcCard->GetJobTitle(&str);
	SetJobTitle(str);
	PR_FREEIF(str);
	srcCard->GetDepartment(&str);
	SetDepartment(str);
	PR_FREEIF(str);
	srcCard->GetCompany(&str);
	SetCompany(str);
	PR_FREEIF(str);
	srcCard->GetWebPage1(&str);
	SetWebPage1(str);
	PR_FREEIF(str);
	srcCard->GetWebPage2(&str);
	SetWebPage2(str);
	PR_FREEIF(str);
	srcCard->GetBirthYear(&str);
	SetBirthYear(str);
	PR_FREEIF(str);
	srcCard->GetBirthMonth(&str);
	SetBirthMonth(str);
	PR_FREEIF(str);
	srcCard->GetBirthDay(&str);
	SetBirthDay(str);
	PR_FREEIF(str);
	srcCard->GetCustom1(&str);
	SetCustom1(str);
	PR_FREEIF(str);
	srcCard->GetCustom2(&str);
	SetCustom2(str);
	PR_FREEIF(str);
	srcCard->GetCustom3(&str);
	SetCustom3(str);
	PR_FREEIF(str);
	srcCard->GetCustom4(&str);
	SetCustom4(str);
	PR_FREEIF(str);
	srcCard->GetNotes(&str);
	SetNotes(str);
	PR_FREEIF(str);

	PRUint32 tableID, rowID;
	srcCard->GetDbTableID(&tableID);
	SetDbTableID(tableID);
	srcCard->GetDbRowID(&rowID);
	SetDbRowID(rowID);

	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetCollationKey(const PRUnichar *str, PRUnichar **key)
{
	nsresult rv = NS_OK;
	nsAutoString resultStr;

	if (mCardDatabase)
	{
		rv = mCardDatabase->CreateCollationKey(str, resultStr);
		*key = resultStr.ToNewUnicode();
	}
	else
		rv = NS_ERROR_FAILURE;

	return rv;
}

NS_IMETHODIMP
nsAbCardProperty::GetFirstName(PRUnichar * *aFirstName)
{ return GetAttributeName(aFirstName, m_FirstName); }

NS_IMETHODIMP
nsAbCardProperty::GetLastName(PRUnichar * *aLastName)
{ return GetAttributeName(aLastName, m_LastName); }

NS_IMETHODIMP
nsAbCardProperty::GetDisplayName(PRUnichar * *aDisplayName)
{ return GetAttributeName(aDisplayName, m_DisplayName); }

NS_IMETHODIMP
nsAbCardProperty::GetNickName(PRUnichar * *aNickName)
{ return GetAttributeName(aNickName, m_NickName); }

NS_IMETHODIMP
nsAbCardProperty::GetPrimaryEmail(PRUnichar * *aPrimaryEmail)
{ return GetAttributeName(aPrimaryEmail, m_PrimaryEmail); }

NS_IMETHODIMP
nsAbCardProperty::GetSecondEmail(PRUnichar * *aSecondEmail)
{ return GetAttributeName(aSecondEmail, m_SecondEmail); }

NS_IMETHODIMP
nsAbCardProperty::GetWorkPhone(PRUnichar * *aWorkPhone)
{ return GetAttributeName(aWorkPhone, m_WorkPhone); }

NS_IMETHODIMP
nsAbCardProperty::GetHomePhone(PRUnichar * *aHomePhone)
{ return GetAttributeName(aHomePhone, m_HomePhone); }

NS_IMETHODIMP
nsAbCardProperty::GetFaxNumber(PRUnichar * *aFaxNumber)
{ return GetAttributeName(aFaxNumber, m_FaxNumber); }

NS_IMETHODIMP
nsAbCardProperty::GetPagerNumber(PRUnichar * *aPagerNumber)
{ return GetAttributeName(aPagerNumber, m_PagerNumber); }

NS_IMETHODIMP
nsAbCardProperty::GetCellularNumber(PRUnichar * *aCellularNumber)
{ return GetAttributeName(aCellularNumber, m_CellularNumber); }

NS_IMETHODIMP
nsAbCardProperty::GetHomeAddress(PRUnichar * *aHomeAddress)
{ return GetAttributeName(aHomeAddress, m_HomeAddress); }

NS_IMETHODIMP
nsAbCardProperty::GetHomeAddress2(PRUnichar * *aHomeAddress2)
{ return GetAttributeName(aHomeAddress2, m_HomeAddress2); }

NS_IMETHODIMP
nsAbCardProperty::GetHomeCity(PRUnichar * *aHomeCity)
{ return GetAttributeName(aHomeCity, m_HomeCity); }

NS_IMETHODIMP
nsAbCardProperty::GetHomeState(PRUnichar * *aHomeState)
{ return GetAttributeName(aHomeState, m_HomeState); }

NS_IMETHODIMP
nsAbCardProperty::GetHomeZipCode(PRUnichar * *aHomeZipCode)
{ return GetAttributeName(aHomeZipCode, m_HomeZipCode); }

NS_IMETHODIMP
nsAbCardProperty::GetHomeCountry(PRUnichar * *aHomecountry)
{ return GetAttributeName(aHomecountry, m_HomeCountry); }

NS_IMETHODIMP
nsAbCardProperty::GetWorkAddress(PRUnichar * *aWorkAddress)
{ return GetAttributeName(aWorkAddress, m_WorkAddress); }

NS_IMETHODIMP
nsAbCardProperty::GetWorkAddress2(PRUnichar * *aWorkAddress2)
{ return GetAttributeName(aWorkAddress2, m_WorkAddress2); }

NS_IMETHODIMP
nsAbCardProperty::GetWorkCity(PRUnichar * *aWorkCity)
{ return GetAttributeName(aWorkCity, m_WorkCity); }

NS_IMETHODIMP
nsAbCardProperty::GetWorkState(PRUnichar * *aWorkState)
{ return GetAttributeName(aWorkState, m_WorkState); }

NS_IMETHODIMP
nsAbCardProperty::GetWorkZipCode(PRUnichar * *aWorkZipCode)
{ return GetAttributeName(aWorkZipCode, m_WorkZipCode); }

NS_IMETHODIMP
nsAbCardProperty::GetWorkCountry(PRUnichar * *aWorkCountry)
{ return GetAttributeName(aWorkCountry, m_WorkCountry); }

NS_IMETHODIMP
nsAbCardProperty::GetJobTitle(PRUnichar * *aJobTitle)
{ return GetAttributeName(aJobTitle, m_JobTitle); }

NS_IMETHODIMP
nsAbCardProperty::GetDepartment(PRUnichar * *aDepartment)
{ return GetAttributeName(aDepartment, m_Department); }

NS_IMETHODIMP
nsAbCardProperty::GetCompany(PRUnichar * *aCompany)
{ return GetAttributeName(aCompany, m_Company); }

NS_IMETHODIMP
nsAbCardProperty::GetWebPage1(PRUnichar * *aWebPage1)
{ return GetAttributeName(aWebPage1, m_WebPage1); }

NS_IMETHODIMP
nsAbCardProperty::GetWebPage2(PRUnichar * *aWebPage2)
{ return GetAttributeName(aWebPage2, m_WebPage2); }

NS_IMETHODIMP
nsAbCardProperty::GetBirthYear(PRUnichar * *aBirthYear)
{ return GetAttributeName(aBirthYear, m_BirthYear); }

NS_IMETHODIMP
nsAbCardProperty::GetBirthMonth(PRUnichar * *aBirthMonth)
{ return GetAttributeName(aBirthMonth, m_BirthMonth); }

NS_IMETHODIMP
nsAbCardProperty::GetBirthDay(PRUnichar * *aBirthDay)
{ return GetAttributeName(aBirthDay, m_BirthDay); }

NS_IMETHODIMP
nsAbCardProperty::GetCustom1(PRUnichar * *aCustom1)
{ return GetAttributeName(aCustom1, m_Custom1); }

NS_IMETHODIMP
nsAbCardProperty::GetCustom2(PRUnichar * *aCustom2)
{ return GetAttributeName(aCustom2, m_Custom2); }

NS_IMETHODIMP
nsAbCardProperty::GetCustom3(PRUnichar * *aCustom3)
{ return GetAttributeName(aCustom3, m_Custom3); }

NS_IMETHODIMP
nsAbCardProperty::GetCustom4(PRUnichar * *aCustom4)
{ return GetAttributeName(aCustom4, m_Custom4); }

NS_IMETHODIMP
nsAbCardProperty::GetNotes(PRUnichar * *aNotes)
{ return GetAttributeName(aNotes, m_Note); }

NS_IMETHODIMP
nsAbCardProperty::GetLastModifiedDate(PRUint32 *aLastModifiedDate)
{ *aLastModifiedDate = m_LastModDate; return NS_OK; }

NS_IMETHODIMP 
nsAbCardProperty::GetKey(PRUint32 *aKey)
{ *aKey = m_Key; return NS_OK; }

NS_IMETHODIMP 
nsAbCardProperty::SetRecordKey(PRUint32 key)
{ m_Key = key; return NS_OK; }

NS_IMETHODIMP
nsAbCardProperty::SetFirstName(const PRUnichar * aFirstName)
{ return SetAttributeName(aFirstName, m_FirstName); }

NS_IMETHODIMP
nsAbCardProperty::SetLastName(const PRUnichar * aLastName)
{ return SetAttributeName(aLastName, m_LastName); }

NS_IMETHODIMP
nsAbCardProperty::SetDisplayName(const PRUnichar * aDisplayName)
{ return SetAttributeName(aDisplayName, m_DisplayName); }

NS_IMETHODIMP
nsAbCardProperty::SetNickName(const PRUnichar * aNickName)
{ return SetAttributeName(aNickName, m_NickName); }

NS_IMETHODIMP
nsAbCardProperty::SetPrimaryEmail(const PRUnichar * aPrimaryEmail)
{ return SetAttributeName(aPrimaryEmail, m_PrimaryEmail); }

NS_IMETHODIMP
nsAbCardProperty::SetSecondEmail(const PRUnichar * aSecondEmail)
{ return SetAttributeName(aSecondEmail, m_SecondEmail); }

NS_IMETHODIMP
nsAbCardProperty::SetWorkPhone(const PRUnichar * aWorkPhone)
{ return SetAttributeName(aWorkPhone, m_WorkPhone); }

NS_IMETHODIMP
nsAbCardProperty::SetHomePhone(const PRUnichar * aHomePhone)
{ return SetAttributeName(aHomePhone, m_HomePhone); }

NS_IMETHODIMP
nsAbCardProperty::SetFaxNumber(const PRUnichar * aFaxNumber)
{ return SetAttributeName(aFaxNumber, m_FaxNumber); }

NS_IMETHODIMP
nsAbCardProperty::SetPagerNumber(const PRUnichar * aPagerNumber)
{ return SetAttributeName(aPagerNumber, m_PagerNumber); }

NS_IMETHODIMP
nsAbCardProperty::SetCellularNumber(const PRUnichar * aCellularNumber)
{ return SetAttributeName(aCellularNumber, m_CellularNumber); }

NS_IMETHODIMP
nsAbCardProperty::SetHomeAddress(const PRUnichar * aHomeAddress)
{ return SetAttributeName(aHomeAddress, m_HomeAddress); }

NS_IMETHODIMP
nsAbCardProperty::SetHomeAddress2(const PRUnichar * aHomeAddress2)
{ return SetAttributeName(aHomeAddress2, m_HomeAddress2); }

NS_IMETHODIMP
nsAbCardProperty::SetHomeCity(const PRUnichar * aHomeCity)
{ return SetAttributeName(aHomeCity, m_HomeCity); }

NS_IMETHODIMP
nsAbCardProperty::SetHomeState(const PRUnichar * aHomeState)
{ return SetAttributeName(aHomeState, m_HomeState); }

NS_IMETHODIMP
nsAbCardProperty::SetHomeZipCode(const PRUnichar * aHomeZipCode)
{ return SetAttributeName(aHomeZipCode, m_HomeZipCode); }

NS_IMETHODIMP
nsAbCardProperty::SetHomeCountry(const PRUnichar * aHomecountry)
{ return SetAttributeName(aHomecountry, m_HomeCountry); }

NS_IMETHODIMP
nsAbCardProperty::SetWorkAddress(const PRUnichar * aWorkAddress)
{ return SetAttributeName(aWorkAddress, m_WorkAddress); }

NS_IMETHODIMP
nsAbCardProperty::SetWorkAddress2(const PRUnichar * aWorkAddress2)
{ return SetAttributeName(aWorkAddress2, m_WorkAddress2); }

NS_IMETHODIMP
nsAbCardProperty::SetWorkCity(const PRUnichar * aWorkCity)
{ return SetAttributeName(aWorkCity, m_WorkCity); }

NS_IMETHODIMP
nsAbCardProperty::SetWorkState(const PRUnichar * aWorkState)
{ return SetAttributeName(aWorkState, m_WorkState); }

NS_IMETHODIMP
nsAbCardProperty::SetWorkZipCode(const PRUnichar * aWorkZipCode)
{ return SetAttributeName(aWorkZipCode, m_WorkZipCode); }

NS_IMETHODIMP
nsAbCardProperty::SetWorkCountry(const PRUnichar * aWorkCountry)
{ return SetAttributeName(aWorkCountry, m_WorkCountry); }

NS_IMETHODIMP
nsAbCardProperty::SetJobTitle(const PRUnichar * aJobTitle)
{ return SetAttributeName(aJobTitle, m_JobTitle); }

NS_IMETHODIMP
nsAbCardProperty::SetDepartment(const PRUnichar * aDepartment)
{ return SetAttributeName(aDepartment, m_Department); }

NS_IMETHODIMP
nsAbCardProperty::SetCompany(const PRUnichar * aCompany)
{ return SetAttributeName(aCompany, m_Company); }

NS_IMETHODIMP
nsAbCardProperty::SetWebPage1(const PRUnichar * aWebPage1)
{ return SetAttributeName(aWebPage1, m_WebPage1); }

NS_IMETHODIMP
nsAbCardProperty::SetWebPage2(const PRUnichar * aWebPage2)
{ return SetAttributeName(aWebPage2, m_WebPage2); }

NS_IMETHODIMP
nsAbCardProperty::SetBirthYear(const PRUnichar * aBirthYear)
{ return SetAttributeName(aBirthYear, m_BirthYear); }

NS_IMETHODIMP
nsAbCardProperty::SetBirthMonth(const PRUnichar * aBirthMonth)
{ return SetAttributeName(aBirthMonth, m_BirthMonth); }

NS_IMETHODIMP
nsAbCardProperty::SetBirthDay(const PRUnichar * aBirthDay)
{ return SetAttributeName(aBirthDay, m_BirthDay); }

NS_IMETHODIMP
nsAbCardProperty::SetCustom1(const PRUnichar * aCustom1)
{ return SetAttributeName(aCustom1, m_Custom1); }

NS_IMETHODIMP
nsAbCardProperty::SetCustom2(const PRUnichar * aCustom2)
{ return SetAttributeName(aCustom2, m_Custom2); }

NS_IMETHODIMP
nsAbCardProperty::SetCustom3(const PRUnichar * aCustom3)
{ return SetAttributeName(aCustom3, m_Custom3); }

NS_IMETHODIMP
nsAbCardProperty::SetCustom4(const PRUnichar * aCustom4)
{ return SetAttributeName(aCustom4, m_Custom4); }

NS_IMETHODIMP
nsAbCardProperty::SetNotes(const PRUnichar * aNotes)
{ return SetAttributeName(aNotes, m_Note); }

NS_IMETHODIMP
nsAbCardProperty::SetLastModifiedDate(PRUint32 aLastModifiedDate)
{ return m_LastModDate = aLastModifiedDate; }

NS_IMETHODIMP 
nsAbCardProperty::GetName(PRUnichar * *aName)
{
    nsresult rv = NS_OK;
	// get name depend on "mail.addr_book.lastnamefirst" 
	// 0= displayname, 1= lastname first, 2=firstname first
    NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
    if (NS_FAILED(rv) || !pPref) 
		return NS_ERROR_FAILURE;

	PRInt32 lastNameFirst = 0;
    rv = pPref->GetIntPref("mail.addr_book.lastnamefirst", &lastNameFirst);
	if (lastNameFirst == 0)
		GetDisplayName(aName);
	else
	{
		if (aName)
		{
			nsString name;
			nsString firstName;
			nsString lastName;
			PRUnichar *str = nsnull;
			GetFirstName(&str);
			if (str)
			{
				firstName = str;
				PR_FREEIF(str);
			}
			GetLastName(&str);
			if (str)
			{
				lastName = str;
				PR_FREEIF(str);
			}

			if (lastName.Length() == 0)
				name = firstName;
			else if (firstName.Length() == 0)
				name = lastName;
			else
			{
				if (lastNameFirst == 1)
				{
					name = lastName;
					name.AppendWithConversion(", ");
					name += firstName;
				}
				else
				{
					name = firstName;
					name.AppendWithConversion(" ");
					name += lastName;
				}
			}
				
			*aName = name.ToNewUnicode();
			if (!(*aName)) 
				return NS_ERROR_OUT_OF_MEMORY;
			else
				return NS_OK;
		}
		else
			return NS_ERROR_NULL_POINTER;

	}

	return NS_OK;
}

NS_IMETHODIMP 
nsAbCardProperty::SetName(const PRUnichar * aName)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsAbCardProperty::GetPrintCardUrl(char * *aPrintCardUrl)
{
static const char *kAbPrintUrlFormat = "addbook:printone?email=%s&folder=%s";

	if (!aPrintCardUrl)
		return NS_OK;

	PRUnichar *email = nsnull;
	GetPrimaryEmail(&email);
	nsString emailStr(email);

	if (emailStr.Length() == 0)
	{
		*aPrintCardUrl = PR_smprintf("");
		return NS_OK;
	}
	PRUnichar *dirName = nsnull;
	if (mCardDatabase)
		mCardDatabase->GetDirectoryName(&dirName);
	nsString dirNameStr(dirName);
	if (dirNameStr.Length() == 0)
	{
		*aPrintCardUrl = PR_smprintf("");
		return NS_OK;
	}
	dirNameStr.ReplaceSubstring(NS_ConvertASCIItoUCS2(" "), NS_ConvertASCIItoUCS2("%20"));

  char *emailCharStr = emailStr.ToNewUTF8String();
  char *dirCharStr = dirNameStr.ToNewUTF8String();

	*aPrintCardUrl = PR_smprintf(kAbPrintUrlFormat, emailCharStr, dirCharStr);

	nsMemory::Free(emailCharStr);
	nsMemory::Free(dirCharStr);

	PR_FREEIF(dirName);
	PR_FREEIF(email);

	return NS_OK;
}

