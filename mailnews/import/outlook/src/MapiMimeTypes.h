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

#ifndef MapiMimeTypes_h___
#define MapiMimeTypes_h___

#include <windows.h>

#define kMaxMimeTypeSize	256

class CMimeTypes {
public:

static PRUint8 *	GetMimeType( nsCString& theExt);
	
protected:
	// Registry stuff
static BOOL	GetKey( HKEY root, LPCTSTR pName, PHKEY pKey);
static BOOL	GetValueBytes( HKEY rootKey, LPCTSTR pValName, LPBYTE *ppBytes);
static void	ReleaseValueBytes( LPBYTE pBytes);
static BOOL	GetMimeTypeFromReg( const nsCString& ext, LPBYTE *ppBytes);


static PRUint8					m_mimeBuffer[kMaxMimeTypeSize];
};

#endif /* MapiMimeTypes_h__ */

