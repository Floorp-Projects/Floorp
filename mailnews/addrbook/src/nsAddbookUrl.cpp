/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsAddbookUrl.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsAbBaseCID.h"
#include "nsEscape.h"

const char *kWorkAddressBook = "AddbookWorkAddressBook";

 
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kAbCardProperty, NS_ABCARDPROPERTY_CID);


/////////////////////////////////////////////////////////////////////////////////////
// addbook url definition
/////////////////////////////////////////////////////////////////////////////////////
nsAddbookUrl::nsAddbookUrl()
{
  NS_INIT_ISUPPORTS();
  nsComponentManager::CreateInstance(kSimpleURICID, nsnull, 
                                     NS_GET_IID(nsIURI), 
                                     (void **) getter_AddRefs(m_baseURL));
  nsComponentManager::CreateInstance(kAbCardProperty, nsnull, NS_GET_IID(nsAbCardProperty), 
                                     (void **) getter_AddRefs(mAbCardProperty));

  mOperationType = nsIAddbookUrlOperation::InvalidUrl; 
}

nsAddbookUrl::~nsAddbookUrl()
{
}

NS_IMPL_ISUPPORTS1(nsAddbookUrl, nsIURI)

//
// This will do the parsing for a single address book entry print operation.
// In this case, you will have address book folder and email:
//
// addbook:add?vcard=begin%3Avcard%0Afn%3ARichard%20Pizzarro%0Aemail%3Binternet%3Arhp%40netscape.com%0Aend%3Avcard%0A
//
NS_IMETHODIMP 
nsAddbookUrl::CrackAddURL(const char *searchPart)
{
  return NS_OK;
}

//
// This will do the parsing for a single address book entry print operation.
// In this case, you will have address book folder and email:
//
// addbook:printall?folder=Netscape%20Address%20Book
// addbook:printall?email=rhp@netscape.com&folder=Netscape%20Address%20Book
//
NS_IMETHODIMP 
nsAddbookUrl::CrackPrintURL(const char *searchPart, PRInt32 aOperation)
{
  nsCString       emailAddr;
  nsCString       folderName;

	char *rest = NS_CONST_CAST(char*, searchPart);

  // okay, first, free up all of our old search part state.....
	CleanupAddbookState();

  // Start past the '?'...
	if (rest && *rest == '?')
		rest++;

	if (rest)
	{
    char *token = nsCRT::strtok(rest, "&", &rest);
		while (token && *token)
		{
			char *value = 0;
      char *eq = PL_strchr(token, '=');
			if (eq)
			{
				value = eq+1;
				*eq = 0;
			}
			
			switch (nsCRT::ToUpper(*token))
			{
				case 'E':
          if (!nsCRT::strcasecmp (token, "email"))
					  emailAddr = value;
				  break;
				case 'F':
            if (!nsCRT::strcasecmp (token, "folder"))
					    folderName = value;
					break;
      } // end of switch statement...
			
			if (eq)
				  *eq = '='; // put it back
				token = nsCRT::strtok(rest, "&", &rest);
		} // while we still have part of the url to parse...
  } // if rest && *rest

	// Now unescape any fields that need escaped...
  if (emailAddr.IsEmpty() && (aOperation == nsIAddbookUrlOperation::PrintIndividual))
    return NS_ERROR_FAILURE;

  if (!emailAddr.IsEmpty())
  {
    nsUnescape(NS_CONST_CAST(char*, emailAddr.get()));
    mAbCardProperty->SetCardValue(kPriEmailColumn, NS_ConvertASCIItoUCS2(emailAddr).get());
  }

  if (!folderName.IsEmpty())
  {
    nsUnescape(NS_CONST_CAST(char*, folderName.get()));
    mAbCardProperty->SetCardValue(kWorkAddressBook, NS_ConvertASCIItoUCS2(folderName).get());
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsAddbookUrl::SetSpec(const char * aSpec)
{
  m_baseURL->SetSpec(aSpec);
	return ParseUrl();
}

nsresult nsAddbookUrl::CleanupAddbookState()
{
  PRUnichar *tString = nsnull;

  mAbCardProperty->SetCardValue(kFirstNameColumn, tString);
  mAbCardProperty->SetCardValue(kLastNameColumn, tString);
  mAbCardProperty->SetCardValue(kDisplayNameColumn, tString);
  mAbCardProperty->SetCardValue(kNicknameColumn, tString);
  mAbCardProperty->SetCardValue(kPriEmailColumn, tString);
  mAbCardProperty->SetCardValue(k2ndEmailColumn, tString);
  mAbCardProperty->SetCardValue(kPreferMailFormatColumn, tString);
  mAbCardProperty->SetCardValue(kWorkPhoneColumn, tString);
  mAbCardProperty->SetCardValue(kHomePhoneColumn, tString);
  mAbCardProperty->SetCardValue(kFaxColumn, tString);
  mAbCardProperty->SetCardValue(kPagerColumn, tString);
  mAbCardProperty->SetCardValue(kCellularColumn, tString);
  mAbCardProperty->SetCardValue(kHomeAddressColumn, tString);
  mAbCardProperty->SetCardValue(kHomeAddress2Column, tString);
  mAbCardProperty->SetCardValue(kHomeCityColumn, tString);
  mAbCardProperty->SetCardValue(kHomeStateColumn, tString);
  mAbCardProperty->SetCardValue(kHomeZipCodeColumn, tString);
  mAbCardProperty->SetCardValue(kHomeCountryColumn, tString);
  mAbCardProperty->SetCardValue(kWorkAddressColumn, tString);
  mAbCardProperty->SetCardValue(kWorkAddress2Column, tString);
  mAbCardProperty->SetCardValue(kWorkCityColumn, tString);
  mAbCardProperty->SetCardValue(kWorkStateColumn, tString);
  mAbCardProperty->SetCardValue(kWorkZipCodeColumn, tString);
  mAbCardProperty->SetCardValue(kWorkCountryColumn, tString);
  mAbCardProperty->SetCardValue(kJobTitleColumn, tString);
  mAbCardProperty->SetCardValue(kDepartmentColumn, tString);
  mAbCardProperty->SetCardValue(kCompanyColumn, tString);
  mAbCardProperty->SetCardValue(kWebPage1Column, tString);
  mAbCardProperty->SetCardValue(kWebPage2Column, tString);
  mAbCardProperty->SetCardValue(kBirthYearColumn, tString);
  mAbCardProperty->SetCardValue(kBirthMonthColumn, tString);
  mAbCardProperty->SetCardValue(kBirthDayColumn, tString);
  mAbCardProperty->SetCardValue(kCustom1Column, tString);
  mAbCardProperty->SetCardValue(kCustom2Column, tString);
  mAbCardProperty->SetCardValue(kCustom3Column, tString);
  mAbCardProperty->SetCardValue(kCustom4Column, tString);
  mAbCardProperty->SetCardValue(kNotesColumn, tString);
  mAbCardProperty->SetCardValue(kLastModifiedDateColumn, tString);
  return NS_OK;
}

nsresult nsAddbookUrl::ParseUrl()
{
	nsresult      rv = NS_OK;
  nsCAutoString searchPart;

  // we can get the path from the simple url.....
  nsXPIDLCString aPath;
  m_baseURL->GetPath(getter_Copies(aPath));
  if (aPath)
    mOperationPart.Assign(aPath);

  PRInt32 startOfSearchPart = mOperationPart.FindChar('?');
  if (startOfSearchPart > 0)
  {
    // now parse out the search field...
    PRUint32 numExtraChars = mOperationPart.Right(searchPart, 
                                                  mOperationPart.Length() -
                                                  startOfSearchPart);
    if (!searchPart.IsEmpty())
    {
      // now we need to strip off the search part from the
      // to part....
      mOperationPart.Cut(startOfSearchPart, numExtraChars);
    }
	}
  else if (!mOperationPart.IsEmpty())
  {
    nsUnescape(NS_CONST_CAST(char*, mOperationPart.get()));
  }

  mOperationPart.ToLowerCase();
  // Now, figure out what we are supposed to be doing?
  if (!nsCRT::strcmp(mOperationPart.get(), "printone"))
  {
    mOperationType = nsIAddbookUrlOperation::PrintIndividual;
    rv = CrackPrintURL(searchPart.get(), mOperationType); 
  }
  else if (!nsCRT::strcmp(mOperationPart.get(), "printall"))
  {
    mOperationType = nsIAddbookUrlOperation::PrintAddressBook;
    rv = CrackPrintURL(searchPart.get(), mOperationType); 
  }
  else if (!nsCRT::strcmp(mOperationPart.get(), "add"))
  {
    mOperationType = nsIAddbookUrlOperation::AddToAddressBook;
    rv = CrackAddURL(searchPart.get()); 
  }
  else
    mOperationType = nsIAddbookUrlOperation::InvalidUrl;
  
  if (NS_FAILED(rv))
    mOperationType = nsIAddbookUrlOperation::InvalidUrl;
  
  return rv;
}

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIURI support
////////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP nsAddbookUrl::GetSpec(char * *aSpec)
{
	return m_baseURL->GetSpec(aSpec);
}

NS_IMETHODIMP nsAddbookUrl::GetPrePath(char * *aPrePath)
{
	return m_baseURL->GetPrePath(aPrePath);
}

NS_IMETHODIMP nsAddbookUrl::SetPrePath(const char * aPrePath)
{
	return m_baseURL->SetPrePath(aPrePath);
}

NS_IMETHODIMP nsAddbookUrl::GetScheme(char * *aScheme)
{
	return m_baseURL->GetScheme(aScheme);
}

NS_IMETHODIMP nsAddbookUrl::SetScheme(const char * aScheme)
{
	return m_baseURL->SetScheme(aScheme);
}

NS_IMETHODIMP nsAddbookUrl::GetPreHost(char * *aPreHost)
{
	return m_baseURL->GetPreHost(aPreHost);
}

NS_IMETHODIMP nsAddbookUrl::SetPreHost(const char * aPreHost)
{
	return m_baseURL->SetPreHost(aPreHost);
}

NS_IMETHODIMP nsAddbookUrl::GetUsername(char * *aUsername)
{
	return m_baseURL->GetUsername(aUsername);
}

NS_IMETHODIMP nsAddbookUrl::SetUsername(const char * aUsername)
{
	return m_baseURL->SetUsername(aUsername);
}

NS_IMETHODIMP nsAddbookUrl::GetPassword(char * *aPassword)
{
	return m_baseURL->GetPassword(aPassword);
}

NS_IMETHODIMP nsAddbookUrl::SetPassword(const char * aPassword)
{
	return m_baseURL->SetPassword(aPassword);
}

NS_IMETHODIMP nsAddbookUrl::GetHost(char * *aHost)
{
	return m_baseURL->GetHost(aHost);
}

NS_IMETHODIMP nsAddbookUrl::SetHost(const char * aHost)
{
	return m_baseURL->SetHost(aHost);
}

NS_IMETHODIMP nsAddbookUrl::GetPort(PRInt32 *aPort)
{
	return m_baseURL->GetPort(aPort);
}

NS_IMETHODIMP nsAddbookUrl::SetPort(PRInt32 aPort)
{
	return m_baseURL->SetPort(aPort);
}

NS_IMETHODIMP nsAddbookUrl::GetPath(char * *aPath)
{
	return m_baseURL->GetPath(aPath);
}

NS_IMETHODIMP nsAddbookUrl::SetPath(const char * aPath)
{
	return m_baseURL->SetPath(aPath);
}

NS_IMETHODIMP nsAddbookUrl::SchemeIs(const char *aScheme, PRBool *_retval)
{
	return m_baseURL->SchemeIs(aScheme, _retval);
}

NS_IMETHODIMP nsAddbookUrl::Equals(nsIURI *other, PRBool *_retval)
{
	return m_baseURL->Equals(other, _retval);
}

NS_IMETHODIMP nsAddbookUrl::Clone(nsIURI **_retval)
{
	return m_baseURL->Clone(_retval);
}	

NS_IMETHODIMP nsAddbookUrl::Resolve(const char *relativePath, char **result) 
{
	return m_baseURL->Resolve(relativePath, result);
}

//
// Specific nsAddbookUrl operations
//
NS_IMETHODIMP 
nsAddbookUrl::GetAddbookOperation(PRInt32 *_retval)
{
  *_retval = mOperationType;
  return NS_OK;
}

NS_IMETHODIMP 
nsAddbookUrl::GetAbCard(nsIAbCard **aAbCard)
{
  if (!mAbCardProperty)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAbCard> tCard = do_QueryInterface(mAbCardProperty);
  if (!tCard)
    return NS_ERROR_FAILURE;

  *aAbCard = tCard;
  NS_ADDREF(*aAbCard);
  return NS_OK;
}
