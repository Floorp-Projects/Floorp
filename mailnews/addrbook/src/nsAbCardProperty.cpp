/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsReadableUtils.h"

#include "nsIPref.h"
#include "nsIAbDirectory.h"
#include "nsIAddressBook.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsAbCardProperty::nsAbCardProperty(void)
{
	NS_INIT_REFCNT();

	m_LastModDate = 0;

	m_PreferMailFormat = nsIAbPreferMailFormat::unknown;
	m_bIsMailList = PR_FALSE;
	m_MailListURI = 0;


}

nsAbCardProperty::~nsAbCardProperty(void)
{
  PR_FREEIF(m_MailListURI);
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
		*aName = ToNewUnicode(value);
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

NS_IMETHODIMP nsAbCardProperty::GetPreferMailFormat(PRUint32 *aFormat)
{
	*aFormat = m_PreferMailFormat;	
	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetPreferMailFormat(PRUint32 aFormat)
{
	m_PreferMailFormat = aFormat;
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

NS_IMETHODIMP nsAbCardProperty::GetMailListURI(char **aMailListURI)
{
	if (aMailListURI)
	{
		if (m_MailListURI)
			*aMailListURI = nsCRT::strdup(m_MailListURI);
		else
			*aMailListURI = nsCRT::strdup("");

		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::SetMailListURI(const char *aMailListURI)
{
	if (aMailListURI)
	{
		nsCRT::free (m_MailListURI);
		m_MailListURI = nsCRT::strdup(aMailListURI);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
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
    else if (!PL_strcmp(attrname, kPreferMailFormatColumn))
    {
        PRUint32 format = nsIAbPreferMailFormat::unknown;
        GetPreferMailFormat(&format);
        nsString formatStr;
        switch (format) {
        case nsIAbPreferMailFormat::unknown :
            formatStr.AssignWithConversion("unknown");
            break;
        case nsIAbPreferMailFormat::plaintext :
            formatStr.AssignWithConversion("plaintext");
            break;
        case nsIAbPreferMailFormat::html :
            formatStr.AssignWithConversion("html");
            break;
        default :
            formatStr.AssignWithConversion("unknown");
            break;
        }
        *value = ToNewUnicode(formatStr);
    }
    
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
    else if (!PL_strcmp(attrname, kPreferMailFormatColumn))
   	{
        PRUint32 format = nsIAbPreferMailFormat::unknown;
        nsString formatStr(value);
        if (formatStr.CompareWithConversion("unknown", PR_TRUE))
            format = nsIAbPreferMailFormat::unknown;
        if (formatStr.CompareWithConversion("plaintext", PR_TRUE))
            format = nsIAbPreferMailFormat::plaintext;
        if (formatStr.CompareWithConversion("html", PR_TRUE))
            format = nsIAbPreferMailFormat::html;
        SetPreferMailFormat(format);
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
nsAbCardProperty::SetName(const PRUnichar * aName)
{
	return NS_OK;
}
NS_IMETHODIMP 
nsAbCardProperty::GetName(PRUnichar * *aName)
{
    nsresult rv = NS_OK;
	// get name depend on "mail.addr_book.lastnamefirst" 
	// 0= displayname, 1= lastname first, 2=firstname first
	nsCOMPtr<nsIPref> pPref = do_GetService(NS_PREF_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

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
      nsXPIDLString str;
			GetFirstName(getter_Copies(str));
			if (str)
			{
				firstName = str;
			}
			GetLastName(getter_Copies(str));
			if (str)
			{
				lastName = str;
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
				
			*aName = ToNewUnicode(name);
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

NS_IMETHODIMP nsAbCardProperty::Copy(nsIAbCard* srcCard)
{
	nsXPIDLString str;
	srcCard->GetFirstName(getter_Copies(str));
	SetFirstName(str);

	srcCard->GetLastName(getter_Copies(str));
	SetLastName(str);
	srcCard->GetDisplayName(getter_Copies(str));
	SetDisplayName(str);
	srcCard->GetNickName(getter_Copies(str));
	SetNickName(str);
	srcCard->GetPrimaryEmail(getter_Copies(str));
	SetPrimaryEmail(str);
	srcCard->GetSecondEmail(getter_Copies(str));
	SetSecondEmail(str);

	PRUint32 format = nsIAbPreferMailFormat::unknown;
	srcCard->GetPreferMailFormat(&format);
	SetPreferMailFormat(format);

	srcCard->GetWorkPhone(getter_Copies(str));
	SetWorkPhone(str);
	srcCard->GetHomePhone(getter_Copies(str));
	SetHomePhone(str);
	srcCard->GetFaxNumber(getter_Copies(str));
	SetFaxNumber(str);
	srcCard->GetPagerNumber(getter_Copies(str));
	SetPagerNumber(str);
	srcCard->GetCellularNumber(getter_Copies(str));
	SetCellularNumber(str);
	srcCard->GetHomeAddress(getter_Copies(str));
	SetHomeAddress(str);
	srcCard->GetHomeAddress2(getter_Copies(str));
	SetHomeAddress2(str);
	srcCard->GetHomeCity(getter_Copies(str));
	SetHomeCity(str);
	srcCard->GetHomeState(getter_Copies(str));
	SetHomeState(str);
	srcCard->GetHomeZipCode(getter_Copies(str));
	SetHomeZipCode(str);
	srcCard->GetHomeCountry(getter_Copies(str));
	SetHomeCountry(str);
	srcCard->GetWorkAddress(getter_Copies(str));
	SetWorkAddress(str);
	srcCard->GetWorkAddress2(getter_Copies(str));
	SetWorkAddress2(str);
	srcCard->GetWorkCity(getter_Copies(str));
	SetWorkCity(str);
	srcCard->GetWorkState(getter_Copies(str));
	SetWorkState(str);
	srcCard->GetWorkZipCode(getter_Copies(str));
	SetWorkZipCode(str);
	srcCard->GetWorkCountry(getter_Copies(str));
	SetWorkCountry(str);
	srcCard->GetJobTitle(getter_Copies(str));
	SetJobTitle(str);
	srcCard->GetDepartment(getter_Copies(str));
	SetDepartment(str);
	srcCard->GetCompany(getter_Copies(str));
	SetCompany(str);
	srcCard->GetWebPage1(getter_Copies(str));
	SetWebPage1(str);
	srcCard->GetWebPage2(getter_Copies(str));
	SetWebPage2(str);
	srcCard->GetBirthYear(getter_Copies(str));
	SetBirthYear(str);
	srcCard->GetBirthMonth(getter_Copies(str));
	SetBirthMonth(str);
	srcCard->GetBirthDay(getter_Copies(str));
	SetBirthDay(str);
	srcCard->GetCustom1(getter_Copies(str));
	SetCustom1(str);
	srcCard->GetCustom2(getter_Copies(str));
	SetCustom2(str);
	srcCard->GetCustom3(getter_Copies(str));
	SetCustom3(str);
	srcCard->GetCustom4(getter_Copies(str));
	SetCustom4(str);
	srcCard->GetNotes(getter_Copies(str));
	SetNotes(str);

	return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::AddCardToDatabase(const char *uri, nsIAbCard **_retval)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uri, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAbCard> cardInstance;
	rv = directory->AddCard (this, getter_AddRefs (cardInstance));
	
	*_retval = cardInstance;
	NS_IF_ADDREF(*_retval);
	return rv;
}

NS_IMETHODIMP nsAbCardProperty::DropCardToDatabase(const char *uri, nsIAbCard **_retval)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uri, getter_AddRefs(res));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
	NS_ENSURE_SUCCESS(rv, rv);       

	nsCOMPtr<nsIAbCard> cardInstance;
	rv = directory->DropCard (this, getter_AddRefs (cardInstance));
	
	*_retval = cardInstance;
	NS_IF_ADDREF(*_retval);
	return rv;
}

NS_IMETHODIMP nsAbCardProperty::GetCollationKey(const PRUnichar *str, PRUnichar **key)
{
	nsresult rv;

	if (!addressBook)
	{
		// Keep a local copy of the addressBook service
		// for performance reasons
		addressBook = do_GetService (NS_ADDRESSBOOK_CONTRACTID, &rv);
		NS_ENSURE_SUCCESS(rv, rv);       
	}

	rv = addressBook->CreateCollationKey(str, key);

	return rv;
}

// nsIAbCard NOT IMPLEMENTED methods

NS_IMETHODIMP nsAbCardProperty::EditCardToDatabase(const char *uri)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbCardProperty::GetPrintCardUrl(char * *aPrintCardUrl)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
