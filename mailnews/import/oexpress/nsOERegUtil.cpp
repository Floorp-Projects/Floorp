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
#include "nsOERegUtil.h"

#include "OEDebugLog.h"

BYTE * nsOERegUtil::GetValueBytes( HKEY hKey, const char *pValueName)
{
	LONG	err;
	DWORD	bufSz;
	LPBYTE	pBytes = NULL;
	DWORD	type = 0;

	err = ::RegQueryValueEx( hKey, pValueName, NULL, &type, NULL, &bufSz); 
	if (err == ERROR_SUCCESS) {
		pBytes = new BYTE[bufSz];
		err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, pBytes, &bufSz);
		if (err != ERROR_SUCCESS) {
			delete [] pBytes;
			pBytes = NULL;
		}
		else {
			if (type == REG_EXPAND_SZ) {
				DWORD sz = bufSz;
				LPBYTE pExpand = NULL;
				DWORD	rSz;
				
				do {
					if (pExpand)
						delete [] pExpand;
					sz += 1024;
					pExpand = new BYTE[sz];
					rSz = ::ExpandEnvironmentStrings( (LPCSTR) pBytes, (LPSTR) pExpand, sz);
				} while (rSz > sz);

				delete [] pBytes;

				return( pExpand);
			}
		}
	}

	return( pBytes);
}

void nsOERegUtil::FreeValueBytes( BYTE *pBytes)
{
	if (pBytes)
		delete [] pBytes;
}

