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


/*

  A sample of XPConnect. This file contains an implementation of
  nsISample.

*/
#include "nscore.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsIImportFieldMap.h"
#include "nsABBaseCID.h"
#include "nsIAbCard.h"

#include "nsOEAddressIterator.h"

#include "OEDebugLog.h"

static NS_DEFINE_CID(kAbCardPropertyCID,	NS_ABCARDPROPERTY_CID);
static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);

typedef struct {
	PRInt32		mozField;
	PRInt32		multiLine;
	ULONG		mapiTag;
} MAPIFields;

/*
	Fields in MAPI, not in Mozilla
	PR_OFFICE_LOCATION
	FIX - PR_BIRTHDAY - stored as PT_SYSTIME - FIX to extract for moz address book birthday
	PR_DISPLAY_NAME_PREFIX - Mr., Mrs. Dr., etc.
	PR_SPOUSE_NAME
	PR_GENDER - integer, not text
	FIX - PR_CONTACT_EMAIL_ADDRESSES - multiuline strings for email addresses, needs
		parsing to get secondary email address for mozilla
*/

#define kIsMultiLine	-2
#define	kNoMultiLine	-1

static MAPIFields	gMapiFields[] = {
	{ 35, kIsMultiLine, PR_COMMENT},
	{ 6, kNoMultiLine, PR_BUSINESS_TELEPHONE_NUMBER},
	{ 7, kNoMultiLine, PR_HOME_TELEPHONE_NUMBER},
	{ 25, kNoMultiLine, PR_COMPANY_NAME},
	{ 23, kNoMultiLine, PR_TITLE},
	{ 10, kNoMultiLine, PR_CELLULAR_TELEPHONE_NUMBER},
	{ 9, kNoMultiLine, PR_PAGER_TELEPHONE_NUMBER},
	{ 8, kNoMultiLine, PR_BUSINESS_FAX_NUMBER},
	{ 8, kNoMultiLine, PR_HOME_FAX_NUMBER},
	{ 22, kNoMultiLine, PR_COUNTRY},
	{ 19, kNoMultiLine, PR_LOCALITY},
	{ 20, kNoMultiLine, PR_STATE_OR_PROVINCE},
	{ 17, 18, PR_STREET_ADDRESS},
	{ 21, kNoMultiLine, PR_POSTAL_CODE},
	{ 27, kNoMultiLine, PR_PERSONAL_HOME_PAGE},
	{ 26, kNoMultiLine, PR_BUSINESS_HOME_PAGE},
	{ 13, kNoMultiLine, PR_HOME_ADDRESS_CITY},
	{ 16, kNoMultiLine, PR_HOME_ADDRESS_COUNTRY},
	{ 15, kNoMultiLine, PR_HOME_ADDRESS_POSTAL_CODE},
	{ 14, kNoMultiLine, PR_HOME_ADDRESS_STATE_OR_PROVINCE},
	{ 11, 12, PR_HOME_ADDRESS_STREET},
	{ 24, kNoMultiLine, PR_DEPARTMENT_NAME}
};

nsOEAddressIterator::nsOEAddressIterator( CWAB *pWab, nsIAddrDatabase *database)
{
	m_pWab = pWab;
	m_database = database;
	NS_IF_ADDREF( m_database);
}

nsOEAddressIterator::~nsOEAddressIterator()
{
	NS_IF_RELEASE( m_database);
}

PRBool nsOEAddressIterator::EnumUser( const PRUnichar * pName, LPENTRYID pEid, ULONG cbEid)
{
	IMPORT_LOG1( "User: %S\n", pName);
	
	nsresult 	rv = NS_OK;
	
	if (m_database) {
		LPMAILUSER	pUser = m_pWab->GetUser( cbEid, pEid);
		if (pUser) {
			// Get a new row from the database!
			nsIMdbRow* newRow = nsnull;
			m_database->GetNewRow( &newRow); 
			// FIXME: Check with Candice about releasing the newRow if it
			// isn't added to the database.  Candice's code in nsAddressBook
			// never releases it but that doesn't seem right to me!
			if (newRow) {
				if (BuildCard( pName, newRow, pUser)) {
					m_database->AddCardRowToDB( newRow);
					IMPORT_LOG0( "* Added entry to address book database\n");
				}
			}
			m_pWab->ReleaseUser( pUser);
		}
	}	
	
	return( PR_TRUE);
}

PRBool nsOEAddressIterator::EnumList( const PRUnichar * pName, LPENTRYID pEid, ULONG cbEid)
{
	IMPORT_LOG1( "List: %S\n", pName);
	return( PR_TRUE);
}

void nsOEAddressIterator::SanitizeValue( nsString& val)
{
	val.ReplaceSubstring(NS_ConvertASCIItoUCS2("\x0D\x0A"), NS_ConvertASCIItoUCS2(", "));
	val.ReplaceChar( 13, ',');
	val.ReplaceChar( 10, ',');
}

void nsOEAddressIterator::SplitString( nsString& val1, nsString& val2)
{
	nsString	temp;

	// Find the last line if there is more than one!
	PRInt32 idx = val1.RFind( "\x0D\x0A");
	PRInt32	cnt = 2;
	if (idx == -1) {
		cnt = 1;
		idx = val1.RFindChar( 13);
	}
	if (idx == -1)
		idx= val1.RFindChar( 10);
	if (idx != -1) {
		val1.Right( val2, val1.Length() - idx - cnt);
		val1.Left( temp, idx);
		val1 = temp;
		SanitizeValue( val1);
	}
}

PRBool nsOEAddressIterator::BuildCard( const PRUnichar * pName, nsIMdbRow *newRow, LPMAILUSER pUser)
{
	
	nsString		lastName;
	nsString		firstName;
	nsString		eMail;
	nsString		nickName;
	nsString		middleName;

	LPSPropValue	pProp = m_pWab->GetUserProperty( pUser, PR_EMAIL_ADDRESS);
	if (pProp) {
		m_pWab->GetValueString( pProp, eMail);
		SanitizeValue( eMail);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_GIVEN_NAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, firstName);
		SanitizeValue( firstName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_SURNAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, lastName);
		SanitizeValue( lastName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_MIDDLE_NAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, middleName);
		SanitizeValue( middleName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_NICKNAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, nickName);
		SanitizeValue( nickName);
		m_pWab->FreeProperty( pProp);
	}
	
	// The idea here is that firstName and lastName cannot both be empty!
	if (firstName.IsEmpty() && lastName.IsEmpty())
		firstName = pName;

	nsString	displayName;
	pProp = m_pWab->GetUserProperty( pUser, PR_DISPLAY_NAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, displayName);
		SanitizeValue( displayName);
		m_pWab->FreeProperty( pProp);
	}
	if (displayName.IsEmpty()) {
		if (firstName.IsEmpty())
			displayName = pName;
		else {
			displayName = firstName;
			if (!middleName.IsEmpty()) {
				displayName.AppendWithConversion( ' ');
				displayName.Append( middleName);
			}
			if (!lastName.IsEmpty()) {
				displayName.AppendWithConversion( ' ');
				displayName.Append( lastName);
			}
		}
	}
	
	// We now have the required fields
	// write them out followed by any optional fields!
	if (!displayName.IsEmpty()) {
		m_database->AddDisplayName( newRow, NS_ConvertUCS2toUTF8(displayName).get());
	}
	if (!firstName.IsEmpty()) {
		m_database->AddFirstName( newRow, NS_ConvertUCS2toUTF8(firstName).get());
	}
	if (!lastName.IsEmpty()) {
		m_database->AddLastName( newRow, NS_ConvertUCS2toUTF8(lastName).get());
	}
	if (!nickName.IsEmpty()) {
		m_database->AddNickName( newRow, NS_ConvertUCS2toUTF8(nickName).get());
	}
	if (!eMail.IsEmpty()) {
		m_database->AddPrimaryEmail( newRow, NS_ConvertUCS2toUTF8(eMail).get());
	}

	// Do all of the extra fields!

	nsString	value;
	nsString	line2;
	nsresult	rv;
	// Create a field map

	nsCOMPtr<nsIImportService> impSvc(do_GetService(kImportServiceCID, &rv));
	if (NS_SUCCEEDED( rv)) {
		nsIImportFieldMap *		pFieldMap = nsnull;
		rv = impSvc->CreateNewFieldMap( &pFieldMap);
		if (NS_SUCCEEDED( rv) && pFieldMap) {
			int max = sizeof( gMapiFields) / sizeof( MAPIFields);
			for (int i = 0; i < max; i++) {
				pProp = m_pWab->GetUserProperty( pUser, gMapiFields[i].mapiTag);
				if (pProp) {
					m_pWab->GetValueString( pProp, value);
					m_pWab->FreeProperty( pProp);
					if (!value.IsEmpty()) {
						if (gMapiFields[i].multiLine == kNoMultiLine) {
							SanitizeValue( value);
							pFieldMap->SetFieldValue( m_database, newRow, gMapiFields[i].mozField, value.get());
						}
						else if (gMapiFields[i].multiLine == kIsMultiLine) {
							pFieldMap->SetFieldValue( m_database, newRow, gMapiFields[i].mozField, value.get());
						}
						else {
							line2.Truncate();
							SplitString( value, line2);
							if (!value.IsEmpty())
								pFieldMap->SetFieldValue( m_database, newRow, gMapiFields[i].mozField, value.get());
							if (!line2.IsEmpty())
								pFieldMap->SetFieldValue( m_database, newRow, gMapiFields[i].multiLine, line2.get());
						}
					}
				}
			}
			// call fieldMap SetFieldValue based on the table of fields

			NS_RELEASE( pFieldMap);
		}
	}


	return( PR_TRUE);
}
