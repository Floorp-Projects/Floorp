/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#ifndef nsOEAddressIterator_h___
#define nsOEAddressIterator_h___

#include "WabObject.h"
#include "nsIAddrDatabase.h"
#include "nsString.h"

class nsOEAddressIterator : public CWabIterator {
public:
	nsOEAddressIterator( CWAB *pWab, nsIAddrDatabase *database);
	~nsOEAddressIterator();
	
	virtual PRBool	EnumUser( const PRUnichar * pName, LPENTRYID pEid, ULONG cbEid);
	virtual PRBool	EnumList( const PRUnichar * pName, LPENTRYID pEid, ULONG cbEid);

private:
	PRBool		BuildCard( const PRUnichar * pName, nsIMdbRow *card, LPMAILUSER pUser);
	void		SanitizeValue( nsString& val);
	void		SplitString( nsString& val1, nsString& val2);

	CWAB *				m_pWab;
	nsIAddrDatabase	*	m_database;
	
};

#endif 
