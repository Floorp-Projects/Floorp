/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
	virtual PRBool	EnumUser( LPCTSTR pName, LPENTRYID pEid, ULONG cbEid) = 0;
	virtual PRBool	EnumList( LPCTSTR pName, LPENTRYID pEid, ULONG cbEid) = 0;
};


class CWAB
{
public:
    CWAB( nsIFileSpec *fileName);
    ~CWAB();
    
	PRBool		Loaded( void) { return( m_bInitialized);}

	HRESULT		IterateWABContents(CWabIterator *pIter);
	
	// Methods for User entries
	LPDISTLIST		GetDistList( ULONG cbEid, LPENTRYID pEid);
	void			ReleaseDistList( LPDISTLIST pList) { if (pList) pList->Release();}
	LPMAILUSER		GetUser( ULONG cbEid, LPENTRYID pEid);
	void			ReleaseUser( LPMAILUSER pUser) { if (pUser) pUser->Release();}
	LPSPropValue	GetUserProperty( LPMAILUSER pUser, ULONG tag);
	LPSPropValue	GetListProperty( LPDISTLIST pList, ULONG tag);
	void			FreeProperty( LPSPropValue pVal) { if (pVal) m_lpWABObject->FreeBuffer( pVal);}
	void			GetValueString( LPSPropValue pVal, nsString& val);

	// Utility stuff used by iterate
	void			FreeProws(LPSRowSet prows);


private:
 
	PRBool      m_bInitialized;
    HINSTANCE   m_hinstWAB;
    LPWABOPEN   m_lpfnWABOpen;
    LPADRBOOK   m_lpAdrBook; 
    LPWABOBJECT m_lpWABObject;
};

#endif // WABOBJECT_INCLUDED


