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

/********************************************************************************************************
 
   Interface for representing Address Book Person Card Property
 
*********************************************************************************************************/

#ifndef nsAbCardProperty_h__
#define nsAbCardProperty_h__

#include "nsIAbCard.h"  
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsIAddressBook.h"
#include "nsIAddrDatabase.h"

 /* 
  * Address Book Card Property
  */ 

class nsAbCardProperty: public nsIAbCard
{
public: 

	NS_DECL_ISUPPORTS
	NS_DECL_NSIABCARD

	nsAbCardProperty(void);
	virtual ~nsAbCardProperty(void);

protected:

	nsresult GetAttributeName(PRUnichar **aName, nsString& value);
	nsresult SetAttributeName(const PRUnichar *aName, nsString& arrtibute);

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
	PRUint32 m_Key;
	PRUint32 m_PreferMailFormat;

	PRBool   m_bIsMailList;
	char*    m_MailListURI;

	nsCOMPtr<nsIAddressBook> addressBook;
};

#endif
