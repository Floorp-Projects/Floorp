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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Paul Sandoz
 */

#ifndef nsAbMDBCardProperty_h__
#define nsAbMDBCardProperty_h__

#include "nsIAbMDBCard.h"  
#include "nsAbCardProperty.h"  
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"

class nsAbMDBCardProperty : public nsIAbMDBCard, public nsAbCardProperty
{
public:
	NS_DECL_ISUPPORTS_INHERITED
	NS_DECL_NSIABMDBCARD

	nsAbMDBCardProperty(void);
	virtual ~nsAbMDBCardProperty();

	// nsIAbCard methods
	NS_IMETHODIMP GetPrintCardUrl(char * *aPrintCardUrl);
	NS_IMETHODIMP EditCardToDatabase(const char *uri);

protected:
	nsresult GetCardDatabase(const char *uri);

	PRUint32 m_Key;
	PRUint32 m_dbTableID;
	PRUint32 m_dbRowID;

	nsCOMPtr<nsIAddrDatabase> mCardDatabase;  

	nsresult RemoveAnonymousList(nsVoidArray* pArray);
	nsresult SetAnonymousAttribute(nsVoidArray** pAttrAray, 
					nsVoidArray** pValueArray, void *attrname, void *value);

	nsVoidArray* m_pAnonymousStrAttributes;
	nsVoidArray* m_pAnonymousStrValues;
	nsVoidArray* m_pAnonymousIntAttributes;
	nsVoidArray* m_pAnonymousIntValues;
	nsVoidArray* m_pAnonymousBoolAttributes;
	nsVoidArray* m_pAnonymousBoolValues;

};

#endif // nsAbMDBCardProperty_h__
