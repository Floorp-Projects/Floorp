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

#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "MapiMimeTypes.h"

PRUint8 CMimeTypes::m_mimeBuffer[kMaxMimeTypeSize];


BOOL CMimeTypes::GetKey( HKEY root, LPCTSTR pName, PHKEY pKey)
{
	LONG result = RegOpenKeyEx( root, pName, 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, pKey);
	return (result == ERROR_SUCCESS);
}

BOOL CMimeTypes::GetValueBytes( HKEY rootKey, LPCTSTR pValName, LPBYTE *ppBytes)
{
	LONG	err;
	DWORD	bufSz;
	
	*ppBytes = NULL;
	// Get the installed directory
	err = RegQueryValueEx( rootKey, pValName, NULL, NULL, NULL, &bufSz); 
	if (err == ERROR_SUCCESS) {
		*ppBytes = new BYTE[bufSz];
		err = RegQueryValueEx( rootKey, pValName, NULL, NULL, *ppBytes, &bufSz);
		if (err == ERROR_SUCCESS) {
			return( TRUE);
		}
		delete *ppBytes;
		*ppBytes = NULL;
	}
	return( FALSE);
}

void CMimeTypes::ReleaseValueBytes( LPBYTE pBytes)
{
	if (pBytes)
		delete pBytes;
}

BOOL CMimeTypes::GetMimeTypeFromReg( const nsCString& ext, LPBYTE *ppBytes)
{
	HKEY	extensionKey;
	BOOL	result = FALSE;


	*ppBytes = NULL;
	if (GetKey( HKEY_CLASSES_ROOT, ext, &extensionKey)) {
		result = GetValueBytes( extensionKey, "Content Type", ppBytes);
		RegCloseKey( extensionKey);
	}
	
	return( result);
}



PRUint8 * CMimeTypes::GetMimeType( nsCString& theExt)
{
	nsCString	ext = theExt;
	if (ext.Length()) {
		if (ext.First() != '.') {
			ext = ".";
			ext += theExt;
		}
	}
	
	
	BOOL	result = FALSE;  
	int		len;

	if (!ext.Length())
		return( NULL);
	LPBYTE	pByte;
	if (GetMimeTypeFromReg( ext, &pByte)) {
		len = nsCRT::strlen( (const char *) pByte);
		if (len && (len < kMaxMimeTypeSize)) {
			nsCRT::memcpy( m_mimeBuffer, pByte, len);
			m_mimeBuffer[len] = 0;
			result = TRUE;
		}
		ReleaseValueBytes( pByte);
	}

	if (result)
		return( m_mimeBuffer);

	return( NULL);
}


