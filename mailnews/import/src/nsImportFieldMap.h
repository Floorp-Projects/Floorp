/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsImportFieldMap_h___
#define nsImportFieldMap_h___

#include "nscore.h"
#include "nsString.h"
#include "nsIImportFieldMap.h"
#include "nsIAddrDatabase.h"
#include "nsVoidArray.h"


////////////////////////////////////////////////////////////////////////


class nsImportFieldMap : public nsIImportFieldMap
{
public: 
	NS_DECL_ISUPPORTS

	NS_DECL_NSIIMPORTFIELDMAP

	nsImportFieldMap();
	virtual ~nsImportFieldMap();

 	static NS_METHOD Create( nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
	nsresult	Allocate( PRInt32 newSize);
	PRInt32		FindFieldNum( const PRUnichar *pDesc);


private:
	PRInt32		m_numFields;
	PRInt32	*	m_pFields;
	PRInt32		m_allocated;
	nsVoidArray	m_descriptions;
	PRInt32		m_mozFieldCount;
};


#endif
