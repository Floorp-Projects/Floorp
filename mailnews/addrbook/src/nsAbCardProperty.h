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

/********************************************************************************************************
 
   Interface for representing Address Book Person Card Property
 
*********************************************************************************************************/

#ifndef nsAbCardProperty_h__
#define nsAbCardProperty_h__

#include "nsIAbCard.h"  
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"

 /* 
  * Address Book Card Property
  */ 

class nsAbCardProperty: public nsIAbCard
{
public: 

	NS_DECL_ISUPPORTS

	nsAbCardProperty(void);
	virtual ~nsAbCardProperty(void);

	// nsIAbCard methods:
	NS_IMETHOD GetFirstName(PRUnichar * *aFirstName) { return GetAttributeName(aFirstName, m_FirstName); }
	NS_IMETHOD GetLastName(PRUnichar * *aLastName) { return GetAttributeName(aLastName, m_LastName); }
	NS_IMETHOD GetDisplayName(PRUnichar * *aDisplayName) { return GetAttributeName(aDisplayName, m_DisplayName); }
	NS_IMETHOD GetNickName(PRUnichar * *aNickName) { return GetAttributeName(aNickName, m_NickName); }
	NS_IMETHOD GetPrimaryEmail(PRUnichar * *aPrimaryEmail) { return GetAttributeName(aPrimaryEmail, m_PrimaryEmail); }
	NS_IMETHOD GetSecondEmail(PRUnichar * *aSecondEmail) { return GetAttributeName(aSecondEmail, m_SecondEmail); }
	NS_IMETHOD GetWorkPhone(PRUnichar * *aWorkPhone) { return GetAttributeName(aWorkPhone, m_WorkPhone); }
	NS_IMETHOD GetHomePhone(PRUnichar * *aHomePhone) { return GetAttributeName(aHomePhone, m_HomePhone); }
	NS_IMETHOD GetFaxNumber(PRUnichar * *aFaxNumber) { return GetAttributeName(aFaxNumber, m_FaxNumber); }
	NS_IMETHOD GetPagerNumber(PRUnichar * *aPagerNumber) { return GetAttributeName(aPagerNumber, m_PagerNumber); }
	NS_IMETHOD GetCellularNumber(PRUnichar * *aCellularNumber) { return GetAttributeName(aCellularNumber, m_CellularNumber); }
	NS_IMETHOD GetHomeAddress(PRUnichar * *aHomeAddress) { return GetAttributeName(aHomeAddress, m_HomeAddress); }
	NS_IMETHOD GetHomeAddress2(PRUnichar * *aHomeAddress2) { return GetAttributeName(aHomeAddress2, m_HomeAddress2); }
	NS_IMETHOD GetHomeCity(PRUnichar * *aHomeCity) { return GetAttributeName(aHomeCity, m_HomeCity); }
	NS_IMETHOD GetHomeState(PRUnichar * *aHomeState) { return GetAttributeName(aHomeState, m_HomeState); }
	NS_IMETHOD GetHomeZipCode(PRUnichar * *aHomeZipCode) { return GetAttributeName(aHomeZipCode, m_HomeZipCode); }
	NS_IMETHOD GetHomeCountry(PRUnichar * *aHomecountry) { return GetAttributeName(aHomecountry, m_HomeCountry); }
	NS_IMETHOD GetWorkAddress(PRUnichar * *aWorkAddress) { return GetAttributeName(aWorkAddress, m_WorkAddress); }
	NS_IMETHOD GetWorkAddress2(PRUnichar * *aWorkAddress2) { return GetAttributeName(aWorkAddress2, m_WorkAddress2); }
	NS_IMETHOD GetWorkCity(PRUnichar * *aWorkCity) { return GetAttributeName(aWorkCity, m_WorkCity); }
	NS_IMETHOD GetWorkState(PRUnichar * *aWorkState) { return GetAttributeName(aWorkState, m_WorkState); }
	NS_IMETHOD GetWorkZipCode(PRUnichar * *aWorkZipCode) { return GetAttributeName(aWorkZipCode, m_WorkZipCode); }
	NS_IMETHOD GetWorkCountry(PRUnichar * *aWorkCountry) { return GetAttributeName(aWorkCountry, m_WorkCountry); }
	NS_IMETHOD GetJobTitle(PRUnichar * *aJobTitle) { return GetAttributeName(aJobTitle, m_JobTitle); }
	NS_IMETHOD GetDepartment(PRUnichar * *aDepartment) { return GetAttributeName(aDepartment, m_Department); }
	NS_IMETHOD GetCompany(PRUnichar * *aCompany) { return GetAttributeName(aCompany, m_Company); }
	NS_IMETHOD GetWebPage1(PRUnichar * *aWebPage1) { return GetAttributeName(aWebPage1, m_WebPage1); }
	NS_IMETHOD GetWebPage2(PRUnichar * *aWebPage2) { return GetAttributeName(aWebPage2, m_WebPage2); }
	NS_IMETHOD GetBirthYear(PRUnichar * *aBirthYear) { return GetAttributeName(aBirthYear, m_BirthYear); }
	NS_IMETHOD GetBirthMonth(PRUnichar * *aBirthMonth) { return GetAttributeName(aBirthMonth, m_BirthMonth); }
	NS_IMETHOD GetBirthDay(PRUnichar * *aBirthDay) { return GetAttributeName(aBirthDay, m_BirthDay); }
	NS_IMETHOD GetCustom1(PRUnichar * *aCustom1) { return GetAttributeName(aCustom1, m_Custom1); }
	NS_IMETHOD GetCustom2(PRUnichar * *aCustom2) { return GetAttributeName(aCustom2, m_Custom2); }
	NS_IMETHOD GetCustom3(PRUnichar * *aCustom3) { return GetAttributeName(aCustom3, m_Custom3); }
	NS_IMETHOD GetCustom4(PRUnichar * *aCustom4) { return GetAttributeName(aCustom4, m_Custom4); }
	NS_IMETHOD GetNotes(PRUnichar * *aNotes) { return GetAttributeName(aNotes, m_Note); }
	NS_IMETHOD GetLastModifiedDate(PRUint32 *aLastModifiedDate) { return *aLastModifiedDate = m_LastModDate; }

	NS_IMETHOD SetFirstName(PRUnichar * aFirstName) { return SetAttributeName(aFirstName, m_FirstName); }
	NS_IMETHOD SetLastName(PRUnichar * aLastName) { return SetAttributeName(aLastName, m_LastName); }
	NS_IMETHOD SetDisplayName(PRUnichar * aDisplayName) { return SetAttributeName(aDisplayName, m_DisplayName); }
	NS_IMETHOD SetNickName(PRUnichar * aNickName) { return SetAttributeName(aNickName, m_NickName); }
	NS_IMETHOD SetPrimaryEmail(PRUnichar * aPrimaryEmail) { return SetAttributeName(aPrimaryEmail, m_PrimaryEmail); }
	NS_IMETHOD SetSecondEmail(PRUnichar * aSecondEmail) { return SetAttributeName(aSecondEmail, m_SecondEmail); }
	NS_IMETHOD SetWorkPhone(PRUnichar * aWorkPhone) { return SetAttributeName(aWorkPhone, m_WorkPhone); }
	NS_IMETHOD SetHomePhone(PRUnichar * aHomePhone) { return SetAttributeName(aHomePhone, m_HomePhone); }
	NS_IMETHOD SetFaxNumber(PRUnichar * aFaxNumber) { return SetAttributeName(aFaxNumber, m_FaxNumber); }
	NS_IMETHOD SetPagerNumber(PRUnichar * aPagerNumber) { return SetAttributeName(aPagerNumber, m_PagerNumber); }
	NS_IMETHOD SetCellularNumber(PRUnichar * aCellularNumber) { return SetAttributeName(aCellularNumber, m_CellularNumber); }
	NS_IMETHOD SetHomeAddress(PRUnichar * aHomeAddress) { return SetAttributeName(aHomeAddress, m_HomeAddress); }
	NS_IMETHOD SetHomeAddress2(PRUnichar * aHomeAddress2) { return SetAttributeName(aHomeAddress2, m_HomeAddress2); }
	NS_IMETHOD SetHomeCity(PRUnichar * aHomeCity) { return SetAttributeName(aHomeCity, m_HomeCity); }
	NS_IMETHOD SetHomeState(PRUnichar * aHomeState) { return SetAttributeName(aHomeState, m_HomeState); }
	NS_IMETHOD SetHomeZipCode(PRUnichar * aHomeZipCode) { return SetAttributeName(aHomeZipCode, m_HomeZipCode); }
	NS_IMETHOD SetHomeCountry(PRUnichar * aHomecountry) { return SetAttributeName(aHomecountry, m_HomeCountry); }
	NS_IMETHOD SetWorkAddress(PRUnichar * aWorkAddress) { return SetAttributeName(aWorkAddress, m_WorkAddress); }
	NS_IMETHOD SetWorkAddress2(PRUnichar * aWorkAddress2) { return SetAttributeName(aWorkAddress2, m_WorkAddress2); }
	NS_IMETHOD SetWorkCity(PRUnichar * aWorkCity) { return SetAttributeName(aWorkCity, m_WorkCity); }
	NS_IMETHOD SetWorkState(PRUnichar * aWorkState) { return SetAttributeName(aWorkState, m_WorkState); }
	NS_IMETHOD SetWorkZipCode(PRUnichar * aWorkZipCode) { return SetAttributeName(aWorkZipCode, m_WorkZipCode); }
	NS_IMETHOD SetWorkCountry(PRUnichar * aWorkCountry) { return SetAttributeName(aWorkCountry, m_WorkCountry); }
	NS_IMETHOD SetJobTitle(PRUnichar * aJobTitle) { return SetAttributeName(aJobTitle, m_JobTitle); }
	NS_IMETHOD SetDepartment(PRUnichar * aDepartment) { return SetAttributeName(aDepartment, m_Department); }
	NS_IMETHOD SetCompany(PRUnichar * aCompany) { return SetAttributeName(aCompany, m_Company); }
	NS_IMETHOD SetWebPage1(PRUnichar * aWebPage1) { return SetAttributeName(aWebPage1, m_WebPage1); }
	NS_IMETHOD SetWebPage2(PRUnichar * aWebPage2) { return SetAttributeName(aWebPage2, m_WebPage2); }
	NS_IMETHOD SetBirthYear(PRUnichar * aBirthYear) { return SetAttributeName(aBirthYear, m_BirthYear); }
	NS_IMETHOD SetBirthMonth(PRUnichar * aBirthMonth) { return SetAttributeName(aBirthMonth, m_BirthMonth); }
	NS_IMETHOD SetBirthDay(PRUnichar * aBirthDay) { return SetAttributeName(aBirthDay, m_BirthDay); }
	NS_IMETHOD SetCustom1(PRUnichar * aCustom1) { return SetAttributeName(aCustom1, m_Custom1); }
	NS_IMETHOD SetCustom2(PRUnichar * aCustom2) { return SetAttributeName(aCustom2, m_Custom2); }
	NS_IMETHOD SetCustom3(PRUnichar * aCustom3) { return SetAttributeName(aCustom3, m_Custom3); }
	NS_IMETHOD SetCustom4(PRUnichar * aCustom4) { return SetAttributeName(aCustom4, m_Custom4); }
	NS_IMETHOD SetNotes(PRUnichar * aNotes) { return SetAttributeName(aNotes, m_Note); }
	NS_IMETHOD SetLastModifiedDate(PRUint32 aLastModifiedDate) { return m_LastModDate = aLastModifiedDate; }

	NS_IMETHOD GetSendPlainText(PRBool *aSendPlainText);
	NS_IMETHOD SetSendPlainText(PRBool aSendPlainText);

	NS_IMETHOD GetDbTableID(PRUint32 *aDbTableID);
	NS_IMETHOD SetDbTableID(PRUint32 aDbTableID);
	NS_IMETHOD GetDbRowID(PRUint32 *aDbRowID);
	NS_IMETHOD SetDbRowID(PRUint32 aDbRowID);

	NS_IMETHOD GetCardValue(const char *attrname, PRUnichar **value);
	NS_IMETHOD SetCardValue(const char *attrname, const PRUnichar *value);
	NS_IMETHOD SetAbDatabase(nsIAddrDatabase* database);

	NS_IMETHOD GetAnonymousStrAttrubutesList(nsVoidArray **attrlist);
	NS_IMETHOD GetAnonymousStrValuesList(nsVoidArray **valuelist);
	NS_IMETHOD GetAnonymousIntAttrubutesList(nsVoidArray **attrlist);
	NS_IMETHOD GetAnonymousIntValuesList(nsVoidArray **valuelist);
	NS_IMETHOD GetAnonymousBoolAttrubutesList(nsVoidArray **attrlist);
	NS_IMETHOD GetAnonymousBoolValuesList(nsVoidArray **valuelist);

	NS_IMETHOD SetAnonymousStringAttribute(const char *attrname, const char *value);
	NS_IMETHOD SetAnonymousIntAttribute(const char *attrname, PRUint32 value);
	NS_IMETHOD SetAnonymousBoolAttribute(const char *attrname, PRBool value);
	NS_IMETHOD AddAnonymousAttributesToDB();
	NS_IMETHOD EditAnonymousAttributesInDB();

	NS_IMETHOD GetCardURI(char **uri);
	NS_IMETHOD AddCardToDatabase(const char *uri);
	NS_IMETHOD EditCardToDatabase(const char *uri);
	NS_IMETHOD CopyCard(nsIAbCard* srcCard);

	NS_IMETHOD GetCollationKey(const PRUnichar *str, PRUnichar **key);


protected:

	nsresult GetCardDatabase(const char *uri);
	nsresult GetAttributeName(PRUnichar **aName, nsString& value);
	nsresult SetAttributeName(PRUnichar *aName, nsString& arrtibute);
	nsresult RemoveAnonymousList(nsVoidArray* pArray);
	nsresult SetAnonymousAttribute(nsVoidArray** pAttrAray, 
					nsVoidArray** pValueArray, void *attrname, void *value);

	nsString m_FirstName;
	nsString m_LastName;
	nsString m_DisplayName;
	nsString m_NickName;
	nsString m_PrimaryEmail;
	nsString m_SecondEmail;
	nsString m_WorkPhone;
	nsString m_HomePhone;
	nsString m_FaxNumber;
	nsString m_PagerNumber;
	nsString m_CellularNumber;
	nsString m_HomeAddress;
	nsString m_HomeAddress2;
	nsString m_HomeCity;
	nsString m_HomeState;
	nsString m_HomeZipCode;
	nsString m_HomeCountry;
	nsString m_WorkAddress;
	nsString m_WorkAddress2;
	nsString m_WorkCity;
	nsString m_WorkState;
	nsString m_WorkZipCode;
	nsString m_WorkCountry;
	nsString m_JobTitle;
	nsString m_Department;
	nsString m_Company;
	nsString m_WebPage1;
	nsString m_WebPage2;
	nsString m_BirthYear;
	nsString m_BirthMonth;
	nsString m_BirthDay;
	nsString m_Custom1;
	nsString m_Custom2;
	nsString m_Custom3;
	nsString m_Custom4;
	nsString m_Note;
	PRUint32 m_LastModDate;

	PRBool   m_bSendPlainText;

	PRUint32 m_dbTableID;
	PRUint32 m_dbRowID;

	nsCOMPtr<nsIAddrDatabase> mCardDatabase;  

	nsVoidArray* m_pAnonymousStrAttributes;
	nsVoidArray* m_pAnonymousStrValues;
	nsVoidArray* m_pAnonymousIntAttributes;
	nsVoidArray* m_pAnonymousIntValues;
	nsVoidArray* m_pAnonymousBoolAttributes;
	nsVoidArray* m_pAnonymousBoolValues;

};

#endif
