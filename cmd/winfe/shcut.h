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

#ifndef __SHCUT_H
#define __SHCUT_H

#ifdef XP_WIN32
#ifndef _OBJBASE_H_
 #include <objbase.h>
#endif
#ifndef __INTSHCUT_H__
 #include "intshcut.h"
#endif
#ifndef _SHLOBJ_H_
 #include "shlobj.h"
#endif
#endif /* XP_WIN32 */
#ifndef __AFXOLE_H__
 #include "afxole.h"
#endif
 
class CInternetShortcut {
protected:
	CString m_URL;
	CString m_filename;
public:
	CInternetShortcut ( const char * filename, char * url = 0 );
	CInternetShortcut ( void );

	BOOL ShellSupport ( );
	void GetURL ( char * urlBuffer, int urlBufferSize );
};

HRESULT ResolveShortCut ( HWND hwnd, LPCSTR pszShortcutFile, LPSTR pszPath);
void DragInternetShortcut ( COleDataSource * pDataSource,
    LPCSTR lpszTitle, LPCSTR lpszAddress );
void DragMultipleShortcuts(COleDataSource* pDataSource, CString* titleArray, 
						   CString* urlArray, int count);
#endif
