/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


/*

  A sample of XPConnect. This file contains an implementation of
  nsISample.

*/
#include "nscore.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsABBaseCID.h"
#include "nsIAbCard.h"

#include "nsOEAddressIterator.h"

#include "OEDebugLog.h"

static NS_DEFINE_CID(kAbCardCID,			NS_ABCARDRESOURCE_CID);
static NS_DEFINE_CID(kAbCardPropertyCID,	NS_ABCARDPROPERTY_CID);


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

PRBool nsOEAddressIterator::EnumUser( LPCTSTR pName, LPENTRYID pEid, ULONG cbEid)
{
	IMPORT_LOG1( "User: %s\n", pName);
	
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
				}
			}
			m_pWab->ReleaseUser( pUser);
		}
	}	
	
	return( PR_TRUE);
}

PRBool nsOEAddressIterator::EnumList( LPCTSTR pName, LPENTRYID pEid, ULONG cbEid)
{
	IMPORT_LOG1( "List: %s\n", pName);
	return( PR_TRUE);
}

void nsOEAddressIterator::SanitizeValue( nsString& val)
{
	/*
	val.TrimLeft();
	val.TrimRight();
	int idx = val.FindOneOf( "\x0D\x0A");
	while (idx != -1) {
		val = val.Left( idx) + ", " + val.Right( val.GetLength() - idx - 1);
		idx = val.FindOneOf( "\x0D\x0A");
	}
	*/
}

PRBool nsOEAddressIterator::BuildCard( LPCTSTR pName, nsIMdbRow *newRow, LPMAILUSER pUser)
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
	if (nickName.IsEmpty())
		nickName = pName;
	if (firstName.IsEmpty()) {
		firstName = nickName;
		middleName.Truncate();
		lastName.Truncate();
	}
	if (lastName.IsEmpty())
		middleName.Truncate();

	if (eMail.IsEmpty())
		eMail = nickName;

	nsString	displayName;
	if (firstName.IsEmpty())
		displayName = nickName;
	else {
		displayName = firstName;
		if (!middleName.IsEmpty()) {
			displayName.Append( ' ');
			displayName.Append( middleName);
		}
		if (!lastName.IsEmpty()) {
			displayName.Append( ' ');
			displayName.Append( lastName);
		}
	}
	
	char *pCStr;
	// We now have the required fields
	// write them out followed by any optional fields!
	m_database->AddDisplayName( newRow, pCStr = displayName.ToNewCString());
	nsCRT::free( pCStr);
	if (!firstName.IsEmpty()) {
		m_database->AddFirstName( newRow, pCStr = firstName.ToNewCString());
		nsCRT::free( pCStr);
	}
	if (!lastName.IsEmpty()) {
		m_database->AddLastName( newRow, pCStr = lastName.ToNewCString());
		nsCRT::free( pCStr);
	}
	m_database->AddNickName( newRow, pCStr = nickName.ToNewCString());
	nsCRT::free( pCStr);
	
	m_database->AddPrimaryEmail( newRow, pCStr = eMail.ToNewCString());
	nsCRT::free( pCStr);



	// Do all of the extra fields!
/*
	CString	value;
	BOOL	encoded = FALSE;
	for (int i = 0; i < kExtraUserFields; i++) {
		value.Empty();
		pProp = m_pWab->GetUserProperty( pUser, extraUserFields[i].tag);
		if (pProp) {
			m_pWab->GetValueString( pProp, value);
			m_pWab->FreeProperty( pProp);
		}
		if (extraUserFields[i].multiLine) {
			encoded = SanitizeMultiLine( value);
		}
		else
			SanitizeValue( value);
		if (!value.IsEmpty()) {
			line = extraUserFields[i].pLDIF;
			if (encoded) {
				line += ": ";
				encoded = FALSE;
			}
			else
				line += ' ';
			line += value;
			result = result && m_out.WriteStr( line);
			result = result && m_out.WriteEol();
		}
	}
*/

	return( PR_TRUE);
}
