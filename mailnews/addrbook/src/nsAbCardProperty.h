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
	NS_IMETHOD GetFirstName(char * *aFirstName) { return GetAttributeName(aFirstName, m_pFirstName); }
	NS_IMETHOD GetLastName(char * *aLastName) { return GetAttributeName(aLastName, m_pLastName); }
	NS_IMETHOD GetDisplayName(char * *aDisplayName) { return GetAttributeName(aDisplayName, m_pDisplayName); }
	NS_IMETHOD GetNickName(char * *aNickName) { return GetAttributeName(aNickName, m_pNickName); }
	NS_IMETHOD GetPrimaryEmail(char * *aPrimaryEmail) { return GetAttributeName(aPrimaryEmail, m_pPrimaryEmail); }
	NS_IMETHOD GetSecondEmail(char * *aSecondEmail) { return GetAttributeName(aSecondEmail, m_pSecondEmail); }
	NS_IMETHOD GetWorkPhone(char * *aWorkPhone) { return GetAttributeName(aWorkPhone, m_pWorkPhone); }
	NS_IMETHOD GetHomePhone(char * *aHomePhone) { return GetAttributeName(aHomePhone, m_pHomePhone); }
	NS_IMETHOD GetFaxNumber(char * *aFaxNumber) { return GetAttributeName(aFaxNumber, m_pFaxNumber); }
	NS_IMETHOD GetPagerNumber(char * *aPagerNumber) { return GetAttributeName(aPagerNumber, m_pPagerNumber); }
	NS_IMETHOD GetCellularNumber(char * *aCellularNumber) { return GetAttributeName(aCellularNumber, m_pCellularNumber); }
	NS_IMETHOD GetHomeAddress(char * *aHomeAddress) { return GetAttributeName(aHomeAddress, m_pHomeAddress); }
	NS_IMETHOD GetHomeAddress2(char * *aHomeAddress2) { return GetAttributeName(aHomeAddress2, m_pHomeAddress2); }
	NS_IMETHOD GetHomeCity(char * *aHomeCity) { return GetAttributeName(aHomeCity, m_pHomeCity); }
	NS_IMETHOD GetHomeState(char * *aHomeState) { return GetAttributeName(aHomeState, m_pHomeState); }
	NS_IMETHOD GetHomeZipCode(char * *aHomeZipCode) { return GetAttributeName(aHomeZipCode, m_pHomeZipCode); }
	NS_IMETHOD GetHomeCountry(char * *aHomecountry) { return GetAttributeName(aHomecountry, m_pHomeCountry); }
	NS_IMETHOD GetWorkAddress(char * *aWorkAddress) { return GetAttributeName(aWorkAddress, m_pWorkAddress); }
	NS_IMETHOD GetWorkAddress2(char * *aWorkAddress2) { return GetAttributeName(aWorkAddress2, m_pWorkAddress2); }
	NS_IMETHOD GetWorkCity(char * *aWorkCity) { return GetAttributeName(aWorkCity, m_pWorkCity); }
	NS_IMETHOD GetWorkState(char * *aWorkState) { return GetAttributeName(aWorkState, m_pWorkState); }
	NS_IMETHOD GetWorkZipCode(char * *aWorkZipCode) { return GetAttributeName(aWorkZipCode, m_pWorkZipCode); }
	NS_IMETHOD GetWorkCountry(char * *aWorkCountry) { return GetAttributeName(aWorkCountry, m_pWorkCountry); }
	NS_IMETHOD GetJobTitle(char * *aJobTitle) { return GetAttributeName(aJobTitle, m_pJobTitle); }
	NS_IMETHOD GetDepartment(char * *aDepartment) { return GetAttributeName(aDepartment, m_pDepartment); }
	NS_IMETHOD GetCompany(char * *aCompany) { return GetAttributeName(aCompany, m_pCompany); }
	NS_IMETHOD GetWebPage1(char * *aWebPage1) { return GetAttributeName(aWebPage1, m_pWebPage1); }
	NS_IMETHOD GetWebPage2(char * *aWebPage2) { return GetAttributeName(aWebPage2, m_pWebPage2); }
	NS_IMETHOD GetBirthYear(char * *aBirthYear) { return GetAttributeName(aBirthYear, m_pBirthYear); }
	NS_IMETHOD GetBirthMonth(char * *aBirthMonth) { return GetAttributeName(aBirthMonth, m_pBirthMonth); }
	NS_IMETHOD GetBirthDay(char * *aBirthDay) { return GetAttributeName(aBirthDay, m_pBirthDay); }
	NS_IMETHOD GetCustom1(char * *aCustom1) { return GetAttributeName(aCustom1, m_pCustom1); }
	NS_IMETHOD GetCustom2(char * *aCustom2) { return GetAttributeName(aCustom2, m_pCustom2); }
	NS_IMETHOD GetCustom3(char * *aCustom3) { return GetAttributeName(aCustom3, m_pCustom3); }
	NS_IMETHOD GetCustom4(char * *aCustom4) { return GetAttributeName(aCustom4, m_pCustom4); }
	NS_IMETHOD GetNotes(char * *aNotes) { return GetAttributeName(aNotes, m_pNote); }
	NS_IMETHOD GetLastModifiedDate(char * *aLastModifiedDate) { return GetAttributeName(aLastModifiedDate, m_pLastModDate); }

	NS_IMETHOD SetFirstName(char * aFirstName) { return SetAttributeName(aFirstName, &m_pFirstName); }
	NS_IMETHOD SetLastName(char * aLastName) { return SetAttributeName(aLastName, &m_pLastName); }
	NS_IMETHOD SetDisplayName(char * aDisplayName) { return SetAttributeName(aDisplayName, &m_pDisplayName); }
	NS_IMETHOD SetNickName(char * aNickName) { return SetAttributeName(aNickName, &m_pNickName); }
	NS_IMETHOD SetPrimaryEmail(char * aPrimaryEmail) { return SetAttributeName(aPrimaryEmail, &m_pPrimaryEmail); }
	NS_IMETHOD SetSecondEmail(char * aSecondEmail) { return SetAttributeName(aSecondEmail, &m_pSecondEmail); }
	NS_IMETHOD SetWorkPhone(char * aWorkPhone) { return SetAttributeName(aWorkPhone, &m_pWorkPhone); }
	NS_IMETHOD SetHomePhone(char * aHomePhone) { return SetAttributeName(aHomePhone, &m_pHomePhone); }
	NS_IMETHOD SetFaxNumber(char * aFaxNumber) { return SetAttributeName(aFaxNumber, &m_pFaxNumber); }
	NS_IMETHOD SetPagerNumber(char * aPagerNumber) { return SetAttributeName(aPagerNumber, &m_pPagerNumber); }
	NS_IMETHOD SetCellularNumber(char * aCellularNumber) { return SetAttributeName(aCellularNumber, &m_pCellularNumber); }
	NS_IMETHOD SetHomeAddress(char * aHomeAddress) { return SetAttributeName(aHomeAddress, &m_pHomeAddress); }
	NS_IMETHOD SetHomeAddress2(char * aHomeAddress2) { return SetAttributeName(aHomeAddress2, &m_pHomeAddress2); }
	NS_IMETHOD SetHomeCity(char * aHomeCity) { return SetAttributeName(aHomeCity, &m_pHomeCity); }
	NS_IMETHOD SetHomeState(char * aHomeState) { return SetAttributeName(aHomeState, &m_pHomeState); }
	NS_IMETHOD SetHomeZipCode(char * aHomeZipCode) { return SetAttributeName(aHomeZipCode, &m_pHomeZipCode); }
	NS_IMETHOD SetHomeCountry(char * aHomecountry) { return SetAttributeName(aHomecountry, &m_pHomeCountry); }
	NS_IMETHOD SetWorkAddress(char * aWorkAddress) { return SetAttributeName(aWorkAddress, &m_pWorkAddress); }
	NS_IMETHOD SetWorkAddress2(char * aWorkAddress2) { return SetAttributeName(aWorkAddress2, &m_pWorkAddress2); }
	NS_IMETHOD SetWorkCity(char * aWorkCity) { return SetAttributeName(aWorkCity, &m_pWorkCity); }
	NS_IMETHOD SetWorkState(char * aWorkState) { return SetAttributeName(aWorkState, &m_pWorkState); }
	NS_IMETHOD SetWorkZipCode(char * aWorkZipCode) { return SetAttributeName(aWorkZipCode, &m_pWorkZipCode); }
	NS_IMETHOD SetWorkCountry(char * aWorkCountry) { return SetAttributeName(aWorkCountry, &m_pWorkCountry); }
	NS_IMETHOD SetJobTitle(char * aJobTitle) { return SetAttributeName(aJobTitle, &m_pJobTitle); }
	NS_IMETHOD SetDepartment(char * aDepartment) { return SetAttributeName(aDepartment, &m_pDepartment); }
	NS_IMETHOD SetCompany(char * aCompany) { return SetAttributeName(aCompany, &m_pCompany); }
	NS_IMETHOD SetWebPage1(char * aWebPage1) { return SetAttributeName(aWebPage1, &m_pWebPage1); }
	NS_IMETHOD SetWebPage2(char * aWebPage2) { return SetAttributeName(aWebPage2, &m_pWebPage2); }
	NS_IMETHOD SetBirthYear(char * aBirthYear) { return SetAttributeName(aBirthYear, &m_pBirthYear); }
	NS_IMETHOD SetBirthMonth(char * aBirthMonth) { return SetAttributeName(aBirthMonth, &m_pBirthMonth); }
	NS_IMETHOD SetBirthDay(char * aBirthDay) { return SetAttributeName(aBirthDay, &m_pBirthDay); }
	NS_IMETHOD SetCustom1(char * aCustom1) { return SetAttributeName(aCustom1, &m_pCustom1); }
	NS_IMETHOD SetCustom2(char * aCustom2) { return SetAttributeName(aCustom2, &m_pCustom2); }
	NS_IMETHOD SetCustom3(char * aCustom3) { return SetAttributeName(aCustom3, &m_pCustom3); }
	NS_IMETHOD SetCustom4(char * aCustom4) { return SetAttributeName(aCustom4, &m_pCustom4); }
	NS_IMETHOD SetNotes(char * aNotes) { return SetAttributeName(aNotes, &m_pNote); }
	NS_IMETHOD SetLastModifiedDate(char * aLastModifiedDate) { return SetAttributeName(aLastModifiedDate, &m_pLastModDate); }

	NS_IMETHOD GetSendPlainText(PRBool *aSendPlainText);
	NS_IMETHOD SetSendPlainText(PRBool aSendPlainText);

	NS_IMETHOD GetDbTableID(PRUint32 *aDbTableID);
	NS_IMETHOD SetDbTableID(PRUint32 aDbTableID);
	NS_IMETHOD GetDbRowID(PRUint32 *aDbRowID);
	NS_IMETHOD SetDbRowID(PRUint32 aDbRowID);

	NS_IMETHOD GetCardValue(const char *attrname, char **value);
	NS_IMETHOD SetCardValue(const char *attrname, const char *value);
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


protected:

	nsresult GetCardDatabase(const char *uri);
	nsresult GetAttributeName(char **aName, char* pValue);
	nsresult SetAttributeName(char *aName, char** arrtibute);
	nsresult RemoveAnonymousList(nsVoidArray* pArray);
	nsresult SetAnonymousAttribute(nsVoidArray** pAttrAray, 
					nsVoidArray** pValueArray, void *attrname, void *value);

	char* m_pFirstName;
	char* m_pLastName;
	char* m_pDisplayName;
	char* m_pNickName;
	char* m_pPrimaryEmail;
	char* m_pSecondEmail;
	char* m_pWorkPhone;
	char* m_pHomePhone;
	char* m_pFaxNumber;
	char* m_pPagerNumber;
	char* m_pCellularNumber;
	char* m_pHomeAddress;
	char* m_pHomeAddress2;
	char* m_pHomeCity;
	char* m_pHomeState;
	char* m_pHomeZipCode;
	char* m_pHomeCountry;
	char* m_pWorkAddress;
	char* m_pWorkAddress2;
	char* m_pWorkCity;
	char* m_pWorkState;
	char* m_pWorkZipCode;
	char* m_pWorkCountry;
	char* m_pJobTitle;
	char* m_pDepartment;
	char* m_pCompany;
	char* m_pWebPage1;
	char* m_pWebPage2;
	char* m_pBirthYear;
	char* m_pBirthMonth;
	char* m_pBirthDay;
	char* m_pCustom1;
	char* m_pCustom2;
	char* m_pCustom3;
	char* m_pCustom4;
	char* m_pNote;
	char* m_pLastModDate;

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
