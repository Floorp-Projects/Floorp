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

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <tchar.h>
#include "trace.h"

#ifdef _DEBUG 

#ifndef WIN16
void CDECL AfxTrace(LPCTSTR lpszFormat, ...)
#else                                       
void CDECL AfxTrace(LPCSTR lpszFormat, ...)
#endif
{
  va_list args;
	va_start(args, lpszFormat);

	int nBuf;
	TCHAR szBuffer[512];

	nBuf = _vstprintf(szBuffer, lpszFormat, args);
	va_end(args);

//  MessageBox(GetDesktopWindow(),
//    szBuffer,
//    "Debug Output",
//    MB_ICONINFORMATION);	

  OutputDebugString(szBuffer); 
  return;
}

BOOL AfxAssertFailedLine(LPCSTR lpszFileName, int nLine)
{    
	return TRUE;
}

#endif
