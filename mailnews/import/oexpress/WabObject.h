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

#ifndef WabObject_h___
#define WabObject_h___

#include "nscore.h"
#include "prtypes.h"
#include "nsString.h"
#include "nsIFileSpec.h"

#include <windows.h>
#include <wab.h>


class CWabIterator {
public:
	virtual PRBool	EnumUser( const PRUnichar *pName, LPENTRYID pEid, ULONG cbEid) = 0;
	virtual PRBool	EnumList( const PRUnichar *pName, LPENTRYID pEid, ULONG cbEid) = 0;
};


class CWAB
{
public:
    CWAB( nsIFileSpec *fileName);
    ~CWAB();
    
	PRBool		Loaded( void) { return( m_bInitialized);}

	HRESULT		IterateWABContents(CWabIterator *pIter, int *pDone);
	
	// Methods for User entries
	LPDISTLIST		GetDistList( ULONG cbEid, LPENTRYID pEid);
	void			ReleaseDistList( LPDISTLIST pList) { if (pList) pList->Release();}
	LPMAILUSER		GetUser( ULONG cbEid, LPENTRYID pEid);
	void			ReleaseUser( LPMAILUSER pUser) { if (pUser) pUser->Release();}
	LPSPropValue	GetUserProperty( LPMAILUSER pUser, ULONG tag);
	LPSPropValue	GetListProperty( LPDISTLIST pList, ULONG tag);
	void			FreeProperty( LPSPropValue pVal) { if (pVal) m_lpWABObject->FreeBuffer( pVal);}
	void			GetValueString( LPSPropValue pVal, nsString& val);

	void			CStrToUnicode( const char *pStr, nsString& result);

	// Utility stuff used by iterate
	void			FreeProws(LPSRowSet prows);


private:
	PRUnichar *	m_pUniBuff;
	int			m_uniBuffLen;
	PRBool      m_bInitialized;
    HINSTANCE   m_hinstWAB;
    LPWABOPEN   m_lpfnWABOpen;
    LPADRBOOK   m_lpAdrBook; 
    LPWABOBJECT m_lpWABObject;
};

#endif // WABOBJECT_INCLUDED


