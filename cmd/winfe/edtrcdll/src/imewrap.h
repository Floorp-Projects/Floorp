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

#ifndef _IMEWRAP_H
#define _IMEWRAP_H

//THIS IS DUMMIED UP BECAUSE WIN16 DOES NOT USE IME INPUT!

#include "edtdlgs.h"

/*note!
this dll may not be present! this is OK!!! all functions will return FALSE
*/

#if WIN32
#include "imm.h"

typedef BOOL (WINAPI *IMMNOTIFYIME)(HIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue);
typedef HIMC (WINAPI *IMMGETCONTEXT)(HWND);
typedef BOOL (WINAPI *IMMRELEASECONTEXT)(HWND,HIMC);
typedef LONG (WINAPI *IMMGETCOMPOSITIONSTRING)(HIMC,DWORD,LPVOID,DWORD);
typedef BOOL (WINAPI *IMMSETCANDIDATEWINDOW)(HIMC, LPCANDIDATEFORM);
typedef BOOL (WINAPI *IMMSETCOMPOSITIONWINDOW)(HIMC, LPCOMPOSITIONFORM);
typedef BOOL (WINAPI *IMMSETCOMPOSITIONFONT)(HIMC, LPLOGFONT);
typedef BOOL (WINAPI *IMMSETCOMPOSITIONSTRING)(HIMC,DWORD,LPVOID,DWORD,LPVOID,DWORD);
typedef BOOL (WINAPI *IMMGETCONVERSIONSTATUS)(HIMC,LPDWORD,LPDWORD);
typedef BOOL (WINAPI *IMMSETCONVERSIONSTATUS)(HIMC,DWORD,DWORD);
typedef LRESULT (WINAPI *IMEESCAPE)(HKL,HIMC,UINT,LPVOID);

#else //WIN16

#include "ime16.h"

typedef BOOL (WINAPI *SENDIMEMESSAGE)(HWND, LPARAM);
typedef BOOL (WINAPI *SENDIMEMESSAGEEX)(HWND, LPARAM);
typedef BOOL (WINAPI *IMPGETIME)( HWND, LPIMEPRO );
#endif //WIN32

class CIMEDll:public IIMEDll
{
    static HINSTANCE s_dllinstance;
#if WIN32
    static unsigned int s_refcount;
    //placeholders for exported functions
    static IMMNOTIFYIME s_ImmNotifyIME;
    static IMMGETCONTEXT s_ImmGetContext;
    static IMMRELEASECONTEXT s_ImmReleaseContext;
    static IMMGETCOMPOSITIONSTRING s_ImmGetCompositionString;
    static IMMSETCANDIDATEWINDOW s_ImmSetCandidateWindow;
    static IMMSETCOMPOSITIONWINDOW s_ImmSetCompositionWindow;
    static IMMSETCOMPOSITIONFONT s_ImmSetCompositionFont;
    static IMMSETCOMPOSITIONSTRING s_ImmSetCompositionString;
    static IMMGETCONVERSIONSTATUS s_ImmGetConversionStatus;
    static IMMSETCONVERSIONSTATUS s_ImmSetConversionStatus;
    static IMEESCAPE s_ImmEscape;
#else
    static unsigned int s_refcount;
    static SENDIMEMESSAGE s_SendIMEMessage;
    static SENDIMEMESSAGEEX s_SendIMEMessageEx;
    static IMPGETIME s_IMPGetIME;
#endif //WIN32
public:
    CIMEDll();
    ~CIMEDll();
#ifdef WIN32
    BOOL ImmNotifyIME(HIMC,DWORD,DWORD,DWORD);
    HIMC ImmGetContext(HWND);
    BOOL ImmReleaseContext(HWND,HIMC);
    LONG ImmGetCompositionString(HIMC,DWORD,LPVOID,DWORD);
    BOOL ImmSetCompositionString(HIMC,DWORD,LPVOID,DWORD,LPVOID,DWORD);
    BOOL ImmGetConversionStatus(HIMC,LPDWORD,LPDWORD);
    BOOL ImmSetConversionStatus(HIMC,DWORD,DWORD);
    BOOL ImmSetCandidateWindow(HIMC, LPCANDIDATEFORM);
    BOOL ImmSetCompositionWindow(HIMC,LPCOMPOSITIONFORM);
    BOOL ImmSetCompositionFont(HIMC,LPLOGFONT);
    LRESULT ImeEscape(HKL hKL,HIMC hIMC,UINT uEscape,LPVOID lpData);
#else //WIN16
    WORD    LOADDS SendIMEMessage( HWND, LPARAM );
    LRESULT LOADDS SendIMEMessageEx( HWND, LPARAM ); /* New for win3.1 */
    BOOL    LOADDS IMPGetIME( HWND, LPIMEPRO );
#endif //WIN32
};


#endif //_IMEWRAP_H
