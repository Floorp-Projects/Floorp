/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAbCardProperty.h"	 
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsAbBaseCID.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIAbDirectory.h"
#include "plbase64.h"
#include "nsIAddrBookSession.h"
#include "nsIStringBundle.h"

#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"

#include "nsEscape.h"
#include "nsVCardObj.h"

#include "mozITXTToHTMLConv.h"

#define PREF_MAIL_ADDR_BOOK_LASTNAMEFIRST "mail.addr_book.lastnamefirst"

const char sAddrbookProperties[] = "chrome://messenger/locale/addressbook/addressBook.properties";

struct AppendItem;

typedef nsresult (AppendCallback) (nsAbCardProperty *aCard, AppendItem *aItem, mozITXTToHTMLConv *aConv, nsString &aResult);

struct AppendItem {
  const char *mColumn;
  const char *mLabel;
  AppendCallback *mCallback;
};

nsresult AppendLine(nsAbCardProperty *aCard, AppendItem *aItem, mozITXTToHTMLConv *aConv, nsString &aResult);
nsresult AppendLabel(nsAbCardProperty *aCard, AppendItem *aItem, mozITXTToHTMLConv *aConv, nsString &aResult);
nsresult AppendCityStateZip(nsAbCardProperty *aCard, AppendItem *aItem, mozITXTToHTMLConv *aConv, nsString &aResult);

static AppendItem NAME_ATTRS_ARRAY[] = { 
	{kDisplayNameColumn, "propertyDisplayName", AppendLabel},   
	{kNicknameColumn, "propertyNickname", AppendLabel},
	{kPriEmailColumn, "", AppendLine},       
	{k2ndEmailColumn, "", AppendLine},
  {kAimScreenNameColumn, "propertyScreenName", AppendLabel},
};

static AppendItem PHONE_ATTRS_ARRAY[] = { 
	{kWorkPhoneColumn, "propertyWork", AppendLabel},   
	{kHomePhoneColumn, "propertyHome", AppendLabel},
	{kFaxColumn, "propertyFax", AppendLabel},       
	{kPagerColumn, "propertyPager", AppendLabel},
	{kCellularColumn, "propertyCellular", AppendLabel}
};

static AppendItem HOME_ATTRS_ARRAY[] = { 
	{kHomeAddressColumn, "", AppendLine},   
	{kHomeAddress2Column, "", AppendLine},
	{kHomeCityColumn, "", AppendCityStateZip},       
	{kHomeCountryColumn, "", AppendLine},
	{kWebPage2Column, "", AppendLine}
};

static AppendItem WORK_ATTRS_ARRAY[] = { 
	{kJobTitleColumn, "", AppendLine},   
	{kDepartmentColumn, "", AppendLine},
	{kCompanyColumn, "", AppendLine},
	{kWorkAddressColumn, "", AppendLine},   
	{kWorkAddress2Column, "", AppendLine},
	{kWorkCityColumn, "", AppendCityStateZip},       
	{kWorkCountryColumn, "", AppendLine},
	{kWebPage1Column, "", AppendLine}
};

static AppendItem CUSTOM_ATTRS_ARRAY[] = { 
	{kCustom1Column, "propertyCustom1", AppendLabel},   
	{kCustom2Column, "propertyCustom2", AppendLabel},
	{kCustom3Column, "propertyCustom3", AppendLabel},       
	{kCustom4Column, "propertyCustom4", AppendLabel},
	{kNotesColumn, "", AppendLine}
};

nsAbCardProperty::nsAbCardProperty(void)
{
	m_LastModDate = 0;

	m_PreferMailFormat = nsIAbPreferMailFormat::unknown;
	m_IsMailList = PR_FALSE;
}

nsAbCardProperty::~nsAbCardProperty(void)
{
}

NS_IMPL_ISUPPORTS1(nsAbCardProperty, nsIAbCard)

////////////////////////////////////////////////////////////////////////////////

nsresult nsAbCardProperty::GetCardTypeFromString(const char *aCardTypeStr, PRBool aEmptyIsTrue, PRBool *aValue)
{
  NS_ENSURE_ARG_POINTER(aCardTypeStr);
  NS_ENSURE_ARG_POINTER(aValue);

  *aValue = PR_FALSE;
  nsXPIDLString cardType;
  nsresult rv = GetCardType(getter_Copies(cardType));
  NS_ENSURE_SUCCESS(rv,rv);

  *aValue = ((aEmptyIsTrue && cardType.IsEmpty()) || cardType.Equals(NS_ConvertASCIItoUCS2(aCardTypeStr)));
  return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetIsANormalCard(PRBool *aIsNormal)
{
  return GetCardTypeFromString(AB_CARD_IS_NORMAL_CARD, PR_TRUE, aIsNormal);
}

NS_IMETHODIMP nsAbCardProperty::GetIsASpecialGroup(PRBool *aIsSpecailGroup)
{
  return GetCardTypeFromString(AB_CARD_IS_AOL_GROUPS, PR_FALSE, aIsSpecailGroup);
}

NS_IMETHODIMP nsAbCardProperty::GetIsAnEmailAddress(PRBool *aIsEmailAddress)
{
  return GetCardTypeFromString(AB_CARD_IS_AOL_ADDITIONAL_EMAIL, PR_FALSE, aIsEmailAddress);
}

nsresult nsAbCardProperty::GetAttributeName(PRUnichar **aName, nsString& value)
{
  NS_ENSURE_ARG_POINTER(aName);
  *aName = ToNewUnicode(value);
  return (*aName) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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
    *aIsMailList = m_IsMailList;
    return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::SetIsMailList(PRBool aIsMailList)
{
    m_IsMailList = aIsMailList;
    return NS_OK;
}

NS_IMETHODIMP nsAbCardProperty::GetMailListURI(char **aMailListURI)
{
  if (aMailListURI)
  {
    *aMailListURI = ToNewCString(m_MailListURI);
    if (*aMailListURI)
      return NS_OK;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::SetMailListURI(const char *aMailListURI)
{
  if (aMailListURI)
  {
    m_MailListURI = aMailListURI;
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbCardProperty::GetCardValue(const char *attrname, PRUnichar **value)
{
  NS_ENSURE_ARG_POINTER(attrname);
  NS_ENSURE_ARG_POINTER(value);

  nsresult rv = NS_OK;

  switch (attrname[0]) {
    case '_': // _AimScreenName
      rv = GetAimScreenName(value);
      break;
    case 'A':
      // AnniversaryYear, AnniversaryMonth, AnniversaryDay
      switch (attrname[11]) {
        case 'Y':
          rv = GetAnniversaryYear(value);
          break;
        case 'M':
          rv = GetAnniversaryMonth(value);
          break;
        case 'D':
          rv = GetAnniversaryDay(value);
          break;
        default:
      rv = NS_ERROR_UNEXPECTED;
      break;
      }
      break;
    case 'B':
      // BirthYear, BirthMonth, BirthDay
      switch (attrname[5]) {
        case 'Y':
          rv = GetBirthYear(value);
          break;
        case 'M':
          rv = GetBirthMonth(value);
          break;
        case 'D':
          rv = GetBirthDay(value);
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'C':
      switch (attrname[1]) {
        case 'o':
          rv = GetCompany(value);
          break;
        case 'a': // CardType, Category
          if (attrname[2] == 't')
            rv = GetCategory(value);
          else
            rv = GetCardType(value);
          break;
        case 'e':
          if (strlen(attrname) <= 14)
          rv = GetCellularNumber(value);
          else
            rv = GetCellularNumberType(value);
          break;
        case 'u':
          switch (attrname[6]) {
            case '1':
              rv = GetCustom1(value);
              break;
            case '2':
              rv = GetCustom2(value);
              break;
            case '3':
              rv = GetCustom3(value);
              break;
            case '4':
              rv = GetCustom4(value);
              break;
            default:
              rv = NS_ERROR_UNEXPECTED;
              break;
          }
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'D':
      if (attrname[1] == 'i') 
        rv = GetDisplayName(value);
      else if (attrname[2] == 'f')
      {
        if ((attrname[7] == 'E'))
          rv = GetDefaultEmail(value);
        else
          rv = GetDefaultAddress(value);
      }
      else 
        rv = GetDepartment(value);
      break;
    case 'F':
      switch (attrname[1]) {
      case 'i':
        rv = GetFirstName(value);
        break;
      case 'a':
        if ((attrname[2] == 'x'))
          if (strlen(attrname) <= 9)
        rv = GetFaxNumber(value);
          else
            rv = GetFaxNumberType(value);
        else
          rv = GetFamilyName(value);
        break;
      default:
        rv = NS_ERROR_UNEXPECTED;
        break;
      }
      break;
    case 'H':
      switch (attrname[4]) {
        case 'A':
          if (attrname[11] == '\0')
            rv = GetHomeAddress(value);
          else 
            rv = GetHomeAddress2(value);
          break;
        case 'C':
          if (attrname[5] == 'i')
            rv = GetHomeCity(value);
          else 
            rv = GetHomeCountry(value);
          break;
        case 'P':
          if (strlen(attrname) <= 9)
          rv = GetHomePhone(value);
          else
            rv = GetHomePhoneType(value);
          break;
        case 'S':
          rv = GetHomeState(value);
          break;
        case 'Z':
          rv = GetHomeZipCode(value);
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'J':
      rv = GetJobTitle(value);
      break;
    case 'L':
      if (attrname[1] == 'a') {
        if (attrname[4] == 'N') 
          rv = GetLastName(value);
        else {
          // XXX todo
          // fix me?  LDAP code gets here
          PRUint32 lastModifiedDate;
          rv = GetLastModifiedDate(&lastModifiedDate);
          *value = nsCRT::strdup(NS_LITERAL_STRING("0Z").get()); 
        }
      }
      else
        rv = NS_ERROR_UNEXPECTED;
      break;
    case 'N':
      if (attrname[1] == 'o')
        rv = GetNotes(value);
      else 
        rv = GetNickName(value);
      break;
    case 'P':
      switch (attrname[2]) { 
        case 'e':
	  PRUint32 format;
          rv = GetPreferMailFormat(&format);
          const PRUnichar *formatStr;

          switch (format) {
            case nsIAbPreferMailFormat::html:
              formatStr = NS_LITERAL_STRING("html").get();
              break;
            case nsIAbPreferMailFormat::plaintext :
              formatStr = NS_LITERAL_STRING("plaintext").get();
              break;
            case nsIAbPreferMailFormat::unknown:
            default :
              formatStr = NS_LITERAL_STRING("unknown").get();
              break;
          }
          *value = nsCRT::strdup(formatStr);
          break;
        case 'g':
          if (strlen(attrname) <= 11)
          rv = GetPagerNumber(value);
          else
            rv = GetPagerNumberType(value);
          break;
        case 'i':
          rv = GetPrimaryEmail(value);
          break;
        case 'o':
          if (attrname[8] == 'L')
            rv = GetPhoneticLastName(value);
          else if (attrname[8] == 'F')
            rv = GetPhoneticFirstName(value);
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'S':
      if (attrname[1] == 'e')
      rv = GetSecondEmail(value);
      else
        rv = GetSpouseName(value);
      break;
    case 'W': 
      if (attrname[1] == 'e') {
        if (attrname[7] == '1')
          rv = GetWebPage1(value);
        else 
          rv = GetWebPage2(value);
      }
      else {
        switch (attrname[4]) {
          case 'A':
            if (attrname[11] == '\0')
              rv = GetWorkAddress(value);
            else 
              rv = GetWorkAddress2(value);
            break;
          case 'C':
            if (attrname[5] == 'i')
              rv = GetWorkCity(value);
            else 
              rv = GetWorkCountry(value);
            break;
          case 'P':
            if (strlen(attrname) <= 9)
            rv = GetWorkPhone(value);
            else
              rv = GetWorkPhoneType(value);
            break;
          case 'S':
            rv = GetWorkState(value);
            break;
          case 'Z':
            rv = GetWorkZipCode(value);
            break;
          default:
            rv = NS_ERROR_UNEXPECTED;
            break;
        }
      }
      break;
    default:
      rv = NS_ERROR_UNEXPECTED;
      break;
  }

  // don't assert here, as failure is expected in certain cases
  // we call GetCardValue() from nsAbView::Init() to determine if the 
  // saved sortColumn is valid or not.
  return rv;
}

NS_IMETHODIMP nsAbCardProperty::SetCardValue(const char *attrname, const PRUnichar *value)
{
  NS_ENSURE_ARG_POINTER(attrname);
  NS_ENSURE_ARG_POINTER(value);

  nsresult rv = NS_OK;

  switch (attrname[0]) {
    case '_': // _AimScreenName
      rv = SetAimScreenName(value);
      break;
    case 'A':
      // AnniversaryYear, AnniversaryMonth, AnniversaryDay
      switch (attrname[5]) {
        case 'Y':
          rv = SetAnniversaryYear(value);
          break;
        case 'M':
          rv = SetAnniversaryMonth(value);
          break;
        case 'D':
          rv = SetAnniversaryDay(value);
          break;
        default:
      rv = NS_ERROR_UNEXPECTED;
      break;
      }
      break;
    case 'B':
      // BirthYear, BirthMonth, BirthDay
      switch (attrname[5]) {
        case 'Y':
          rv = SetBirthYear(value);
          break;
        case 'M':
          rv = SetBirthMonth(value);
          break;
        case 'D':
          rv = SetBirthDay(value);
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'C':
      switch (attrname[1]) {
        case 'o':
          rv = SetCompany(value);
          break;
        case 'a': // CardType, Category
          if (attrname[2] == 't')
            rv = SetCategory(value);
          else
            rv = SetCardType(value);
          break;
        case 'e':
          if (strlen(attrname) <= 14)
          rv = SetCellularNumber(value);
          else
            rv = SetCellularNumberType(value);
          break;
        case 'u':
          switch (attrname[6]) {
            case '1':
              rv = SetCustom1(value);
              break;
            case '2':
              rv = SetCustom2(value);
              break;
            case '3':
              rv = SetCustom3(value);
              break;
            case '4':
              rv = SetCustom4(value);
              break;
            default:
              rv = NS_ERROR_UNEXPECTED;
              break;
          }
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'D':
      if (attrname[1] == 'i') 
        rv = SetDisplayName(value);
      else if (attrname[2] == 'f')
      {
        if ((attrname[7] == 'E'))
          rv = SetDefaultEmail(value);
        else
          rv = SetDefaultAddress(value);
      }
      else 
        rv = SetDepartment(value);
      break;
    case 'F':
      switch (attrname[1]) {
      case 'i':
        rv = SetFirstName(value);
        break;
      case 'a':
        if ((attrname[2] == 'x'))
          if (strlen(attrname) <= 9)
        rv = SetFaxNumber(value);
          else
            rv = SetFaxNumberType(value);
        else
          rv = SetFamilyName(value);
        break;
      default:
        rv = NS_ERROR_UNEXPECTED;
        break;
      }
      break;
    case 'H':
      switch (attrname[4]) {
        case 'A':
          if (attrname[11] == '\0')
            rv = SetHomeAddress(value);
          else 
            rv = SetHomeAddress2(value);
          break;
        case 'C':
          if (attrname[5] == 'i')
            rv = SetHomeCity(value);
          else 
            rv = SetHomeCountry(value);
          break;
        case 'P':
          if (strlen(attrname) <= 9)
          rv = SetHomePhone(value);
          else
            rv = SetHomePhoneType(value);
          break;
        case 'S':
          rv = SetHomeState(value);
          break;
        case 'Z':
          rv = SetHomeZipCode(value);
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'J':
      rv = SetJobTitle(value);
      break;
    case 'L':
      if (attrname[1] == 'a') {
        if (attrname[4] == 'N') 
          rv = SetLastName(value);
        else {
          // XXX todo 
          // fix me?  LDAP code gets here
          rv = SetLastModifiedDate(0);
        }
      }
      else
        rv = NS_ERROR_UNEXPECTED;
      break;
    case 'N':
      if (attrname[1] == 'o')
        rv = SetNotes(value);
      else 
        rv = SetNickName(value);
      break;
    case 'P':
      switch (attrname[2]) { 
        case 'e':
          switch (value[0]) {
            case 't':    // "true"
            case 'T':
              rv = SetPreferMailFormat(nsIAbPreferMailFormat::html);
              break;
            case 'f':    // "false"
            case 'F':
              rv = SetPreferMailFormat(nsIAbPreferMailFormat::plaintext);
              break;
            default:
              rv = SetPreferMailFormat(nsIAbPreferMailFormat::unknown);
              break;
          }
          break;
        case 'g':
          if (strlen(attrname) <= 11)
          rv = SetPagerNumber(value);
          else
            rv = SetPagerNumberType(value);
          break;
        case 'i':
          rv = SetPrimaryEmail(value);
          break;
        case 'o':
          if (attrname[8] == 'L')
            rv = SetPhoneticLastName(value);
          else if (attrname[8] == 'F')
            rv = SetPhoneticFirstName(value);
          break;
        default:
          rv = NS_ERROR_UNEXPECTED;
          break;
      }
      break;
    case 'S':
      if (attrname[1] == 'e')
      rv = SetSecondEmail(value);
      else
        rv = SetSpouseName(value);
      break;
    case 'W': 
      if (attrname[1] == 'e') {
        if (attrname[7] == '1')
          rv = SetWebPage1(value);
        else 
          rv = SetWebPage2(value);
      }
      else {
        switch (attrname[4]) {
          case 'A':
            if (attrname[11] == '\0')
              rv = SetWorkAddress(value);
            else 
              rv = SetWorkAddress2(value);
            break;
          case 'C':
            if (attrname[5] == 'i')
              rv = SetWorkCity(value);
            else 
              rv = SetWorkCountry(value);
            break;
          case 'P':
            if (strlen(attrname) <= 9)
            rv = SetWorkPhone(value);
            else
              rv = SetWorkPhoneType(value);
            break;
          case 'S':
            rv = SetWorkState(value);
            break;
          case 'Z':
            rv = SetWorkZipCode(value);
            break;
          default:
            rv = NS_ERROR_UNEXPECTED;
            break;
        }
      }
      break;
    default:
      rv = NS_ERROR_UNEXPECTED;
      break;
  }

  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP
nsAbCardProperty::GetFirstName(PRUnichar * *aFirstName)
{ return GetAttributeName(aFirstName, m_FirstName); }

NS_IMETHODIMP
nsAbCardProperty::GetLastName(PRUnichar * *aLastName)
{ return GetAttributeName(aLastName, m_LastName); }

NS_IMETHODIMP
nsAbCardProperty::GetPhoneticFirstName(PRUnichar * *aPhoneticFirstName)
{ return GetAttributeName(aPhoneticFirstName, m_PhoneticFirstName); }

NS_IMETHODIMP
nsAbCardProperty::GetPhoneticLastName(PRUnichar * *aPhoneticLastName)
{ return GetAttributeName(aPhoneticLastName, m_PhoneticLastName); }

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
nsAbCardProperty::GetDefaultEmail(PRUnichar * *aDefaultEmail)
{ return GetAttributeName(aDefaultEmail, m_DefaultEmail); }

NS_IMETHODIMP
nsAbCardProperty::GetCardType(PRUnichar * *aCardType)
{ return GetAttributeName(aCardType, m_CardType); }

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
nsAbCardProperty::GetWorkPhoneType(PRUnichar * *aWorkPhoneType)
{ return GetAttributeName(aWorkPhoneType, m_WorkPhoneType); }

NS_IMETHODIMP
nsAbCardProperty::GetHomePhoneType(PRUnichar * *aHomePhoneType)
{ return GetAttributeName(aHomePhoneType, m_HomePhoneType); }

NS_IMETHODIMP
nsAbCardProperty::GetFaxNumberType(PRUnichar * *aFaxNumberType)
{ return GetAttributeName(aFaxNumberType, m_FaxNumberType); }

NS_IMETHODIMP
nsAbCardProperty::GetPagerNumberType(PRUnichar * *aPagerNumberType)
{ return GetAttributeName(aPagerNumberType, m_PagerNumberType); }

NS_IMETHODIMP
nsAbCardProperty::GetCellularNumberType(PRUnichar * *aCellularNumberType)
{ return GetAttributeName(aCellularNumberType, m_CellularNumberType); }

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
nsAbCardProperty::GetAimScreenName(PRUnichar * *aAimScreenName)
{ return GetAttributeName(aAimScreenName, m_AimScreenName); }

NS_IMETHODIMP
nsAbCardProperty::GetAnniversaryYear(PRUnichar * *aAnniversaryYear)
{ return GetAttributeName(aAnniversaryYear, m_AnniversaryYear); }

NS_IMETHODIMP
nsAbCardProperty::GetAnniversaryMonth(PRUnichar * *aAnniversaryMonth)
{ return GetAttributeName(aAnniversaryMonth, m_AnniversaryMonth); }

NS_IMETHODIMP
nsAbCardProperty::GetAnniversaryDay(PRUnichar * *aAnniversaryDay)
{ return GetAttributeName(aAnniversaryDay, m_AnniversaryDay); }

NS_IMETHODIMP
nsAbCardProperty::GetSpouseName(PRUnichar * *aSpouseName)
{ return GetAttributeName(aSpouseName, m_SpouseName); }

NS_IMETHODIMP
nsAbCardProperty::GetFamilyName(PRUnichar * *aFamilyName)
{ return GetAttributeName(aFamilyName, m_FamilyName); }

NS_IMETHODIMP
nsAbCardProperty::GetDefaultAddress(PRUnichar * *aDefaultAddress)
{ return GetAttributeName(aDefaultAddress, m_DefaultAddress); }

NS_IMETHODIMP
nsAbCardProperty::GetCategory(PRUnichar * *aCategory)
{ return GetAttributeName(aCategory, m_Category); }

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
nsAbCardProperty::SetPhoneticLastName(const PRUnichar * aPhoneticLastName)
{ return SetAttributeName(aPhoneticLastName, m_PhoneticLastName); }

NS_IMETHODIMP
nsAbCardProperty::SetPhoneticFirstName(const PRUnichar * aPhoneticFirstName)
{ return SetAttributeName(aPhoneticFirstName, m_PhoneticFirstName); }

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
nsAbCardProperty::SetDefaultEmail(const PRUnichar * aDefaultEmail)
{ return SetAttributeName(aDefaultEmail, m_DefaultEmail); }

NS_IMETHODIMP
nsAbCardProperty::SetCardType(const PRUnichar * aCardType)
{ return SetAttributeName(aCardType, m_CardType); }

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
nsAbCardProperty::SetWorkPhoneType(const PRUnichar * aWorkPhoneType)
{ return SetAttributeName(aWorkPhoneType, m_WorkPhoneType); }

NS_IMETHODIMP
nsAbCardProperty::SetHomePhoneType(const PRUnichar * aHomePhoneType)
{ return SetAttributeName(aHomePhoneType, m_HomePhoneType); }

NS_IMETHODIMP
nsAbCardProperty::SetFaxNumberType(const PRUnichar * aFaxNumberType)
{ return SetAttributeName(aFaxNumberType, m_FaxNumberType); }

NS_IMETHODIMP
nsAbCardProperty::SetPagerNumberType(const PRUnichar * aPagerNumberType)
{ return SetAttributeName(aPagerNumberType, m_PagerNumberType); }

NS_IMETHODIMP
nsAbCardProperty::SetCellularNumberType(const PRUnichar * aCellularNumberType)
{ return SetAttributeName(aCellularNumberType, m_CellularNumberType); }

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
nsAbCardProperty::SetAimScreenName(const PRUnichar *aAimScreenName)
{ return SetAttributeName(aAimScreenName, m_AimScreenName); }

NS_IMETHODIMP
nsAbCardProperty::SetAnniversaryYear(const PRUnichar * aAnniversaryYear)
{ return SetAttributeName(aAnniversaryYear, m_AnniversaryYear); }

NS_IMETHODIMP
nsAbCardProperty::SetAnniversaryMonth(const PRUnichar * aAnniversaryMonth)
{ return SetAttributeName(aAnniversaryMonth, m_AnniversaryMonth); }

NS_IMETHODIMP
nsAbCardProperty::SetAnniversaryDay(const PRUnichar * aAnniversaryDay)
{ return SetAttributeName(aAnniversaryDay, m_AnniversaryDay); }

NS_IMETHODIMP
nsAbCardProperty::SetSpouseName(const PRUnichar * aSpouseName)
{ return SetAttributeName(aSpouseName, m_SpouseName); }

NS_IMETHODIMP
nsAbCardProperty::SetFamilyName(const PRUnichar * aFamilyName)
{ return SetAttributeName(aFamilyName, m_FamilyName); }

NS_IMETHODIMP
nsAbCardProperty::SetDefaultAddress(const PRUnichar * aDefaultAddress)
{ return SetAttributeName(aDefaultAddress, m_DefaultAddress); }

NS_IMETHODIMP
nsAbCardProperty::SetCategory(const PRUnichar * aCategory)
{ return SetAttributeName(aCategory, m_Category); }

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

NS_IMETHODIMP nsAbCardProperty::Copy(nsIAbCard* srcCard)
{
	nsXPIDLString str;
	srcCard->GetFirstName(getter_Copies(str));
	SetFirstName(str);

	srcCard->GetLastName(getter_Copies(str));
	SetLastName(str);
	srcCard->GetPhoneticFirstName(getter_Copies(str));
	SetPhoneticFirstName(str);
	srcCard->GetPhoneticLastName(getter_Copies(str));
	SetPhoneticLastName(str);
	srcCard->GetDisplayName(getter_Copies(str));
	SetDisplayName(str);
	srcCard->GetNickName(getter_Copies(str));
	SetNickName(str);
	srcCard->GetPrimaryEmail(getter_Copies(str));
	SetPrimaryEmail(str);
	srcCard->GetSecondEmail(getter_Copies(str));
	SetSecondEmail(str);
  srcCard->GetDefaultEmail(getter_Copies(str));
  SetDefaultEmail(str);
  srcCard->GetCardType(getter_Copies(str));
  SetCardType(str);

	PRUint32 format;
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
  srcCard->GetWorkPhoneType(getter_Copies(str));
  SetWorkPhoneType(str);
  srcCard->GetHomePhoneType(getter_Copies(str));
  SetHomePhoneType(str);
  srcCard->GetFaxNumberType(getter_Copies(str));
  SetFaxNumberType(str);
  srcCard->GetPagerNumberType(getter_Copies(str));
  SetPagerNumberType(str);
  srcCard->GetCellularNumberType(getter_Copies(str));
  SetCellularNumberType(str);
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
  srcCard->GetAimScreenName(getter_Copies(str));
  SetAimScreenName(str);

  srcCard->GetAnniversaryYear(getter_Copies(str));
  SetAnniversaryYear(str);
  srcCard->GetAnniversaryMonth(getter_Copies(str));
  SetAnniversaryMonth(str);
  srcCard->GetAnniversaryDay(getter_Copies(str));
  SetAnniversaryDay(str);
  srcCard->GetSpouseName(getter_Copies(str));
  SetSpouseName(str);
  srcCard->GetFamilyName(getter_Copies(str));
  SetFamilyName(str);
  srcCard->GetDefaultAddress(getter_Copies(str));
  SetDefaultAddress(str);
  srcCard->GetCategory(getter_Copies(str));
  SetCategory(str);

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

NS_IMETHODIMP nsAbCardProperty::EditCardToDatabase(const char *uri)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbCardProperty::Equals(nsIAbCard *card, PRBool *result)
{
  *result = (card == this);
  return NS_OK;
}

static VObject* myAddPropValue(VObject *o, const char *propName, const PRUnichar *propValue, PRBool *aCardHasData)
{
    if (aCardHasData)
        *aCardHasData = PR_TRUE;
    return addPropValue(o, propName, NS_ConvertUCS2toUTF8(propValue).get());
}

NS_IMETHODIMP nsAbCardProperty::ConvertToEscapedVCard(char **aResult)
{
    nsXPIDLString str;
    PRBool vCardHasData = PR_FALSE;
    VObject* vObj = newVObject(VCCardProp);
    VObject* t;
    
    // [comment from 4.x]
    // Big flame coming....so Vobject is not designed at all to work with  an array of 
    // attribute values. It wants you to have all of the attributes easily available. You
    // cannot add one attribute at a time as you find them to the vobject. Why? Because
    // it creates a property for a particular type like phone number and then that property
    // has multiple values. This implementation is not pretty. I can hear my algos prof
    // yelling from here.....I have to do a linear search through my attributes array for
    // EACH vcard property we want to set. *sigh* One day I will have time to come back
    // to this function and remedy this O(m*n) function where n = # attribute values and
    // m = # of vcard properties....  

    (void)GetDisplayName(getter_Copies(str));
    if (!str.IsEmpty()) {
        myAddPropValue(vObj, VCFullNameProp, str.get(), &vCardHasData);
    }
    
    (void)GetLastName(getter_Copies(str));
    if (!str.IsEmpty()) {
        t = isAPropertyOf(vObj, VCNameProp);
        if (!t)
            t = addProp(vObj, VCNameProp);
        myAddPropValue(t, VCFamilyNameProp, str.get(), &vCardHasData);
    }
    
    (void)GetFirstName(getter_Copies(str));
    if (!str.IsEmpty()) {
        t = isAPropertyOf(vObj, VCNameProp);
        if (!t)
            t = addProp(vObj, VCNameProp);
        myAddPropValue(t, VCGivenNameProp, str.get(), &vCardHasData);
    }

    (void)GetCompany(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCOrgProp);
        if (!t)
            t = addProp(vObj, VCOrgProp);
        myAddPropValue(t, VCOrgNameProp, str.get(), &vCardHasData);
    }

    (void)GetDepartment(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCOrgProp);
        if (!t)
            t = addProp(vObj, VCOrgProp);
        myAddPropValue(t, VCOrgUnitProp, str.get(), &vCardHasData);
    }
 
    (void)GetWorkAddress2(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCAdrProp);
        if  (!t)
            t = addProp(vObj, VCAdrProp);
        myAddPropValue(t, VCPostalBoxProp, str.get(), &vCardHasData);  
    }

    (void)GetWorkAddress(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCAdrProp);
        if  (!t)
            t = addProp(vObj, VCAdrProp);
        myAddPropValue(t, VCStreetAddressProp, str.get(), &vCardHasData);
    }

    (void)GetWorkCity(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCAdrProp);
        if  (!t)
            t = addProp(vObj, VCAdrProp);
        myAddPropValue(t, VCCityProp, str.get(), &vCardHasData);
    }

    (void)GetWorkState(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCAdrProp);
        if  (!t)
            t = addProp(vObj, VCAdrProp);
        myAddPropValue(t, VCRegionProp, str.get(), &vCardHasData);
    }

    (void)GetWorkZipCode(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCAdrProp);
        if  (!t)
            t = addProp(vObj, VCAdrProp);
        myAddPropValue(t, VCPostalCodeProp, str.get(), &vCardHasData);
    }

    (void)GetWorkCountry(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = isAPropertyOf(vObj, VCAdrProp);
        if  (!t)
            t = addProp(vObj, VCAdrProp);
        myAddPropValue(t, VCCountryNameProp, str.get(), &vCardHasData);
    }
    else
    {
        // only add this if VCAdrProp already exists
        t = isAPropertyOf(vObj, VCAdrProp);
        if (t)
        {
            addProp(t, VCDomesticProp);
        }
    }

    (void)GetPrimaryEmail(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = myAddPropValue(vObj, VCEmailAddressProp, str.get(), &vCardHasData);  
        addProp(t, VCInternetProp);
    }
 
    (void)GetJobTitle(getter_Copies(str));
    if (!str.IsEmpty())
    {
        myAddPropValue(vObj, VCTitleProp, str.get(), &vCardHasData);
    }

    (void)GetWorkPhone(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = myAddPropValue(vObj, VCTelephoneProp, str.get(), &vCardHasData);
        addProp(t, VCWorkProp);
    }

    (void)GetFaxNumber(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = myAddPropValue(vObj, VCTelephoneProp, str.get(), &vCardHasData);
        addProp(t, VCFaxProp);
    }

    (void)GetPagerNumber(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = myAddPropValue(vObj, VCTelephoneProp, str.get(), &vCardHasData);
        addProp(t, VCPagerProp);
    }
    
    (void)GetHomePhone(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = myAddPropValue(vObj, VCTelephoneProp, str.get(), &vCardHasData);
        addProp(t, VCHomeProp);
    }

    (void)GetCellularNumber(getter_Copies(str));
    if (!str.IsEmpty())
    {
        t = myAddPropValue(vObj, VCTelephoneProp, str.get(), &vCardHasData);
        addProp(t, VCCellularProp);
    }

    (void)GetNotes(getter_Copies(str));
    if (!str.IsEmpty())
    {
        myAddPropValue(vObj, VCNoteProp, str.get(), &vCardHasData);
    }
    
    PRUint32 format;
    (void)GetPreferMailFormat(&format);
    if (format == nsIAbPreferMailFormat::html) {
        myAddPropValue(vObj, VCUseHTML, NS_LITERAL_STRING("TRUE").get(), &vCardHasData);
    }
    else if (format == nsIAbPreferMailFormat::plaintext) {
        myAddPropValue(vObj, VCUseHTML, NS_LITERAL_STRING("FALSE").get(), &vCardHasData);
    }

    (void)GetWebPage1(getter_Copies(str));
    if (!str.IsEmpty())
    {
        myAddPropValue(vObj, VCURLProp, str.get(), &vCardHasData);
    }
    
    myAddPropValue(vObj, VCVersionProp, NS_LITERAL_STRING("2.1").get(), nsnull);

    if (!vCardHasData) {
        *aResult = PL_strdup("");
        return NS_OK;
    }

    int len = 0;
    char *vCard = writeMemVObject(0, &len, vObj);
    if (vObj)
        cleanVObject(vObj);

    *aResult = nsEscape(vCard, url_Path);
    return (*aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

NS_IMETHODIMP nsAbCardProperty::ConvertToBase64EncodedXML(char **result)
{
  nsresult rv;
  nsString xmlStr;

  xmlStr.Append(NS_LITERAL_STRING("<?xml version=\"1.0\"?>\n").get());
  xmlStr.Append(NS_LITERAL_STRING("<?xml-stylesheet type=\"text/css\" href=\"chrome://messenger/content/addressbook/print.css\"?>\n").get());
  xmlStr.Append(NS_LITERAL_STRING("<directory>\n").get());

  // Get Address Book string and set it as title of XML document
  nsCOMPtr<nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv); 
  if (NS_SUCCEEDED(rv)) {
    rv = stringBundleService->CreateBundle(sAddrbookProperties, getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv)) {
      nsXPIDLString addrBook;
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("addressBook").get(), getter_Copies(addrBook));
      if (NS_SUCCEEDED(rv)) {
        xmlStr.Append(NS_LITERAL_STRING("<title xmlns=\"http://www.w3.org/1999/xhtml\">").get());
        xmlStr.Append(addrBook);
        xmlStr.Append(NS_LITERAL_STRING("</title>\n").get());
      }
    }
  }

  nsXPIDLString xmlSubstr;
  rv = ConvertToXMLPrintData(getter_Copies(xmlSubstr));
  NS_ENSURE_SUCCESS(rv,rv);

  xmlStr.Append(xmlSubstr.get());
  xmlStr.Append(NS_LITERAL_STRING("</directory>\n").get());

  *result = PL_Base64Encode(NS_ConvertUCS2toUTF8(xmlStr).get(), 0, nsnull);
  return (*result ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
}

NS_IMETHODIMP nsAbCardProperty::ConvertToXMLPrintData(PRUnichar **aXMLSubstr)
{
  NS_ENSURE_ARG_POINTER(aXMLSubstr);

  nsresult rv;
  nsString xmlStr;

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  PRInt32 generatedNameFormat;
  rv = prefBranch->GetIntPref(PREF_MAIL_ADDR_BOOK_LASTNAMEFIRST, &generatedNameFormat);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsXPIDLString generatedName;
  rv = abSession->GenerateNameFromCard(this, generatedNameFormat, getter_Copies(generatedName));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<mozITXTToHTMLConv> conv = do_CreateInstance(MOZ_TXTTOHTMLCONV_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  xmlStr.Append(NS_LITERAL_STRING("<GeneratedName>\n").get());

  nsCOMPtr<nsIStringBundle> bundle;

  nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);

  rv = stringBundleService->CreateBundle(sAddrbookProperties, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv,rv); 
  
  nsXPIDLString heading;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("headingCardFor").get(), getter_Copies(heading));
  NS_ENSURE_SUCCESS(rv, rv);

  xmlStr.Append(heading);

  xmlStr.Append(NS_LITERAL_STRING(" ").get());

  // use ScanTXT to convert < > & to safe values.
  nsXPIDLString safeText;
  if (!generatedName.IsEmpty()) {
    rv = conv->ScanTXT(generatedName.get(), mozITXTToHTMLConv::kEntities , getter_Copies(safeText));
    NS_ENSURE_SUCCESS(rv,rv);
  }

  if (!safeText.IsEmpty())
    xmlStr.Append(safeText.get());
  else {
    nsXPIDLString primaryEmail;
    rv = GetCardValue(kPriEmailColumn, getter_Copies(primaryEmail));
    NS_ENSURE_SUCCESS(rv,rv);

    // use ScanTXT to convert < > & to safe values.
    nsXPIDLString safeText;
    rv = conv->ScanTXT(primaryEmail.get(), mozITXTToHTMLConv::kEntities, getter_Copies(safeText));
    NS_ENSURE_SUCCESS(rv,rv);

    xmlStr.Append(safeText.get());
  }
          
  xmlStr.Append(NS_LITERAL_STRING("</GeneratedName>\n").get());

  xmlStr.Append(NS_LITERAL_STRING("<table><tr><td>").get());

  rv = AppendSection(NAME_ATTRS_ARRAY, sizeof(NAME_ATTRS_ARRAY)/sizeof(AppendItem), nsnull, conv, xmlStr);

  xmlStr.Append(NS_LITERAL_STRING("</td></tr>").get());
  xmlStr.Append(NS_LITERAL_STRING("<tr><td>").get());

  rv = AppendSection(PHONE_ATTRS_ARRAY, sizeof(PHONE_ATTRS_ARRAY)/sizeof(AppendItem), NS_LITERAL_STRING("headingPhone").get(), conv, xmlStr);

  if (!m_IsMailList) {
    rv = AppendSection(CUSTOM_ATTRS_ARRAY, sizeof(CUSTOM_ATTRS_ARRAY)/sizeof(AppendItem), NS_LITERAL_STRING("headingOther").get(), conv, xmlStr);
  }
  else {

    rv = AppendSection(CUSTOM_ATTRS_ARRAY, sizeof(CUSTOM_ATTRS_ARRAY)/sizeof(AppendItem), NS_LITERAL_STRING("headingDescription").get(), conv, xmlStr);
    
    xmlStr.Append(NS_LITERAL_STRING("<section><sectiontitle>").get());

    rv = bundle->GetStringFromName(NS_LITERAL_STRING("headingAddresses").get(), getter_Copies(heading));
    NS_ENSURE_SUCCESS(rv, rv);

    xmlStr.Append(heading);
    xmlStr.Append(NS_LITERAL_STRING("</sectiontitle>").get());

    nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
    NS_ENSURE_SUCCESS(rv,rv);
      
    nsCOMPtr <nsIRDFResource> resource;
    rv = rdfService->GetResource(m_MailListURI, getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr <nsIAbDirectory> mailList = do_QueryInterface(resource, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsISupportsArray> addresses;
    rv = mailList->GetAddressLists(getter_AddRefs(addresses));
    if (addresses) {
      PRUint32 total = 0;
      addresses->Count(&total);		            
      if (total) {
        PRUint32 i;
        nsXPIDLString displayName;
        nsXPIDLString primaryEmail;
        for (i = 0; i < total; i++) {
          nsCOMPtr <nsIAbCard> listCard = do_QueryElementAt(addresses, i, &rv);
          NS_ENSURE_SUCCESS(rv,rv);

          xmlStr.Append(NS_LITERAL_STRING("<PrimaryEmail>\n").get());

          rv = listCard->GetDisplayName(getter_Copies(displayName));
          NS_ENSURE_SUCCESS(rv,rv);

          // use ScanTXT to convert < > & to safe values.
          nsXPIDLString safeText;
          rv = conv->ScanTXT(displayName.get(), mozITXTToHTMLConv::kEntities, getter_Copies(safeText));
          NS_ENSURE_SUCCESS(rv,rv);
          xmlStr.Append(safeText.get());

          xmlStr.Append(NS_LITERAL_STRING(" &lt;").get());
          
          rv = listCard->GetPrimaryEmail(getter_Copies(primaryEmail));
          NS_ENSURE_SUCCESS(rv,rv);

          // use ScanTXT to convert < > & to safe values.
          rv = conv->ScanTXT(primaryEmail.get(), mozITXTToHTMLConv::kEntities, getter_Copies(safeText));
          NS_ENSURE_SUCCESS(rv,rv);
          xmlStr.Append(safeText.get());

          xmlStr.Append(NS_LITERAL_STRING("&gt;").get());
          
          xmlStr.Append(NS_LITERAL_STRING("</PrimaryEmail>\n").get());
        }
      }
    }
    xmlStr.Append(NS_LITERAL_STRING("</section>").get());
  }

  xmlStr.Append(NS_LITERAL_STRING("</td><td>").get());

  rv = AppendSection(HOME_ATTRS_ARRAY, sizeof(HOME_ATTRS_ARRAY)/sizeof(AppendItem), NS_LITERAL_STRING("headingHome").get(), conv, xmlStr);
  rv = AppendSection(WORK_ATTRS_ARRAY, sizeof(WORK_ATTRS_ARRAY)/sizeof(AppendItem), NS_LITERAL_STRING("headingWork").get(), conv, xmlStr);
  
  xmlStr.Append(NS_LITERAL_STRING("</td></tr></table>").get());

  *aXMLSubstr = ToNewUnicode(xmlStr);

  return NS_OK;
}

nsresult nsAbCardProperty::AppendData(const char *aAttrName, mozITXTToHTMLConv *aConv, nsString &aResult)
{
  nsXPIDLString attrValue;
  nsresult rv = GetCardValue(aAttrName, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  if (attrValue.IsEmpty())
    return NS_OK;

  nsAutoString attrNameStr;
  attrNameStr.AssignWithConversion(aAttrName);
  
  aResult.Append(NS_LITERAL_STRING("<").get());
  aResult.Append(attrNameStr.get());
  aResult.Append(NS_LITERAL_STRING(">").get());
  
  // use ScanTXT to convert < > & to safe values.
  nsXPIDLString safeText;
  rv = aConv->ScanTXT(attrValue.get(), mozITXTToHTMLConv::kEntities, getter_Copies(safeText));
  NS_ENSURE_SUCCESS(rv,rv);
  aResult.Append(safeText.get());

  aResult.Append(NS_LITERAL_STRING("</").get());
  aResult.Append(attrNameStr.get());
  aResult.Append(NS_LITERAL_STRING(">").get());

  return NS_OK;
}

nsresult nsAbCardProperty::AppendSection(AppendItem *aArray, PRInt16 aCount, const PRUnichar *aHeading, mozITXTToHTMLConv *aConv, nsString &aResult) 
{
  nsresult rv;

  aResult.Append(NS_LITERAL_STRING("<section>").get());

  nsXPIDLString attrValue;
  PRBool sectionIsEmpty = PR_TRUE;

  PRInt16 i = 0;
  for (i=0;i<aCount;i++) {
    rv = GetCardValue(aArray[i].mColumn, getter_Copies(attrValue));
    NS_ENSURE_SUCCESS(rv,rv);
    sectionIsEmpty &= attrValue.IsEmpty();
  }

  if (!sectionIsEmpty && aHeading) {
	  nsCOMPtr<nsIStringBundle> bundle;

    nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv); 
    NS_ENSURE_SUCCESS(rv,rv);

    rv = stringBundleService->CreateBundle(sAddrbookProperties, getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv,rv); 
  
    nsXPIDLString heading;
    rv = bundle->GetStringFromName(aHeading, getter_Copies(heading));
    NS_ENSURE_SUCCESS(rv, rv);

    aResult.Append(NS_LITERAL_STRING("<sectiontitle>").get());
    aResult.Append(heading);
    aResult.Append(NS_LITERAL_STRING("</sectiontitle>").get());
  }

  for (i=0;i<aCount;i++) {
	  rv = aArray[i].mCallback(this, &aArray[i], aConv, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "append callback failed");
  }

  aResult.Append(NS_LITERAL_STRING("</section>").get());

  return NS_OK;
}

nsresult AppendLine(nsAbCardProperty *aCard, AppendItem *aItem, mozITXTToHTMLConv *aConv, nsString &aResult)
{
  nsXPIDLString attrValue;
  nsresult rv = aCard->GetCardValue(aItem->mColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  if (attrValue.IsEmpty())
    return NS_OK; 

  nsAutoString attrNameStr;
  attrNameStr.AssignWithConversion(aItem->mColumn);
  
  aResult.Append(NS_LITERAL_STRING("<").get());
  aResult.Append(attrNameStr.get());
  aResult.Append(NS_LITERAL_STRING(">").get());
  
  // use ScanTXT to convert < > & to safe values.
  nsXPIDLString safeText;
  rv = aConv->ScanTXT(attrValue.get(), mozITXTToHTMLConv::kEntities, getter_Copies(safeText));
  NS_ENSURE_SUCCESS(rv,rv);
  aResult.Append(safeText.get());

  aResult.Append(NS_LITERAL_STRING("</").get());
  aResult.Append(attrNameStr.get());
  aResult.Append(NS_LITERAL_STRING(">").get());

  return NS_OK;
}

nsresult AppendLabel(nsAbCardProperty *aCard, AppendItem *aItem, mozITXTToHTMLConv *aConv, nsString &aResult)
{
  nsresult rv;
  
  nsCOMPtr<nsIStringBundle> bundle;

  nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);

  rv = stringBundleService->CreateBundle(sAddrbookProperties, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv,rv); 
  
  nsXPIDLString label;
  
  nsXPIDLString attrValue;

  rv = aCard->GetCardValue(aItem->mColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  if (attrValue.IsEmpty())
    return NS_OK;

  rv = bundle->GetStringFromName(NS_ConvertASCIItoUCS2(aItem->mLabel).get(), getter_Copies(label));
  NS_ENSURE_SUCCESS(rv, rv);

  aResult.Append(NS_LITERAL_STRING("<labelrow><label>").get());

  aResult.Append(label.get());
  aResult.Append(NS_LITERAL_STRING(": ").get());

  aResult.Append(NS_LITERAL_STRING("</label>").get());

  rv = AppendLine(aCard, aItem, aConv, aResult);
  NS_ENSURE_SUCCESS(rv,rv);

  aResult.Append(NS_LITERAL_STRING("</labelrow>").get());
  
  return NS_OK;
}

nsresult AppendCityStateZip(nsAbCardProperty *aCard, AppendItem *aItem, mozITXTToHTMLConv *aConv, nsString &aResult) 
{
  nsresult rv;

  nsXPIDLString attrValue;
  
  AppendItem item;
  
  const char *stateCol, *zipCol;

  if (strcmp(aItem->mColumn, kHomeCityColumn) == 0) {
    stateCol = kHomeStateColumn;
    zipCol = kHomeZipCodeColumn;
  }
  else {
    stateCol = kWorkStateColumn;
    zipCol = kWorkZipCodeColumn;
  }

  nsAutoString cityResult, stateResult, zipResult;

  rv = AppendLine(aCard, aItem, aConv, cityResult);
  NS_ENSURE_SUCCESS(rv,rv);
  
  item.mColumn = stateCol;
  item.mLabel = "";

  rv = AppendLine(aCard, &item, aConv, stateResult);
  NS_ENSURE_SUCCESS(rv,rv);

  item.mColumn = zipCol;

  rv = AppendLine(aCard, &item, aConv, zipResult);
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLString formattedString;

  nsCOMPtr<nsIStringBundle> bundle;

  nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);

  rv = stringBundleService->CreateBundle(sAddrbookProperties, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv,rv); 

  if (!cityResult.IsEmpty() && !stateResult.IsEmpty() && !zipResult.IsEmpty()) {
    const PRUnichar *formatStrings[3] = { cityResult.get(), stateResult.get(), zipResult.get() };
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("cityAndStateAndZip").get(), formatStrings, 3, getter_Copies(formattedString));
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else if (!cityResult.IsEmpty() && !stateResult.IsEmpty() && zipResult.IsEmpty()) {
    const PRUnichar *formatStrings[2] = { cityResult.get(), stateResult.get() };
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("cityAndStateNoZip").get(), formatStrings, 2, getter_Copies(formattedString));
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else if ((!cityResult.IsEmpty() && stateResult.IsEmpty() && !zipResult.IsEmpty()) ||
          (cityResult.IsEmpty() && !stateResult.IsEmpty() && !zipResult.IsEmpty())) {
    const PRUnichar *formatStrings[2] = { cityResult.IsEmpty() ? stateResult.get() : cityResult.get(), zipResult.get() };
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("cityOrStateAndZip").get(), formatStrings, 2, getter_Copies(formattedString));
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else {
    if (!cityResult.IsEmpty()) 
      formattedString = cityResult;
    else if (!stateResult.IsEmpty()) 
      formattedString = stateResult;
    else 
      formattedString = zipResult;
  }

  aResult.Append(formattedString);

  return NS_OK;
}
