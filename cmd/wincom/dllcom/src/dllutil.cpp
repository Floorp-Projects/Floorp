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

#include "dllcom.h"
#include <assert.h>

// Converts an OLE string (UNICODE) to an ANSI string. The caller
// must use CoTaskMemFree to free the memory
LPSTR
AllocTaskAnsiString(LPOLESTR lpszString)
{
	LPSTR	lpszResult;
	UINT	nBytes;

	if (lpszString == NULL)
		return NULL;

#ifdef _WIN32
	nBytes = (wcslen(lpszString) + 1) * 2;
#else
	nBytes = lstrlen(lpszString) + 1;  // Win 16 doesn't use any UNICODE
#endif
	lpszResult = (LPSTR)CoTaskMemAlloc(nBytes);

	if (lpszResult) {
#ifdef _WIN32
		WideCharToMultiByte(CP_ACP, 0, lpszString, -1, lpszResult, nBytes, NULL, NULL);
#else
		lstrcpy(lpszResult, lpszString);
#endif
	}

	return lpszResult;
}

// The caller must free the returned string using CoTaskMemFree. The
// returned string is an ANSI string
LPSTR
DLL_StringFromGUID(REFGUID rGuid)
{
#ifdef DLL_WIN32
	OLECHAR	szGuidID[80];
	int 	iConvert = StringFromGUID2(rGuid, szGuidID, 80);

	assert(iConvert > 0);
	return iConvert > 0 ? AllocTaskAnsiString(szGuidID) : NULL;
#else
	LPSTR	lpszGuidID;

	if (!SUCCEEDED(StringFromCLSID(rGuid, &lpszGuidID)))
		return NULL;

	return lpszGuidID;
#endif
}
