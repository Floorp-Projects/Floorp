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

#include "stdafx.h"
#include "imewrap.h"
#include "edtiface.h"//for the callbacks
/*imewrap.h edtor client wrapper */
/*Dynamic Library wrapper for loading on call*/


HINSTANCE CIMEDll::s_dllinstance=NULL;
unsigned int CIMEDll::s_refcount=0;


IMMNOTIFYIME                CIMEDll::s_ImmNotifyIME=NULL;
IMMGETCONTEXT               CIMEDll::s_ImmGetContext=NULL;
IMMRELEASECONTEXT           CIMEDll::s_ImmReleaseContext=NULL;
IMMGETCOMPOSITIONSTRING     CIMEDll::s_ImmGetCompositionString=NULL;
IMMSETCANDIDATEWINDOW       CIMEDll::s_ImmSetCandidateWindow=NULL;
IMMSETCOMPOSITIONWINDOW     CIMEDll::s_ImmSetCompositionWindow=NULL;
IMMSETCOMPOSITIONFONT       CIMEDll::s_ImmSetCompositionFont=NULL;
IMMSETCOMPOSITIONSTRING     CIMEDll::s_ImmSetCompositionString=NULL;
IMMGETCONVERSIONSTATUS      CIMEDll::s_ImmGetConversionStatus=NULL;
IMMSETCONVERSIONSTATUS      CIMEDll::s_ImmSetConversionStatus=NULL;
IMEESCAPE                   CIMEDll::s_ImmEscape=NULL;

#ifdef WIN32
#define XP_ASSERT(x) assert(x)
#else
#define XP_ASSERT(x) 
#endif

CIMEDll::CIMEDll()
{
    HINSTANCE t_instance=LoadLibrary("IMM32.DLL");
    if(!t_instance)
    {
        s_dllinstance=NULL;
        s_ImmNotifyIME=NULL;
        s_ImmGetContext=NULL;
        s_ImmReleaseContext=NULL;
        s_ImmGetCompositionString=NULL;
        s_ImmSetCandidateWindow=NULL;
        s_ImmSetCompositionWindow=NULL;
        s_ImmSetCompositionFont=NULL;
        s_ImmSetCompositionString=NULL;
        s_ImmGetConversionStatus=NULL;
        s_ImmSetConversionStatus=NULL;
        s_ImmEscape=NULL;
        return;//this is OK! this must be an english version of NT3.5
    }
    s_refcount++;
    if (t_instance==s_dllinstance)
        return;
    else
    {
        s_dllinstance=t_instance;
        //retrieve all proc addresses and place them into holders
        s_ImmNotifyIME= (IMMNOTIFYIME)GetProcAddress(s_dllinstance,"ImmNotifyIME");
        XP_ASSERT(s_ImmNotifyIME);

        s_ImmGetContext= (IMMGETCONTEXT)GetProcAddress(s_dllinstance,"ImmGetContext");
        XP_ASSERT(s_ImmGetContext);

        s_ImmReleaseContext= (IMMRELEASECONTEXT)GetProcAddress(s_dllinstance,"ImmReleaseContext");
        XP_ASSERT(s_ImmReleaseContext);

        #ifdef UNICODE_ONLY
        s_ImmGetCompositionString= (IMMGETCOMPOSITIONSTRING)GetProcAddress(s_dllinstance,"ImmGetCompositionStringW");
        #else //ANSI ONLY
        s_ImmGetCompositionString= (IMMGETCOMPOSITIONSTRING)GetProcAddress(s_dllinstance,"ImmGetCompositionStringA");
        #endif //!ANSI_ONLY
        XP_ASSERT(s_ImmGetCompositionString);

        #ifdef UNICODE_ONLY
        s_ImmSetCompositionFont= (IMMSETCOMPOSITIONFONT)GetProcAddress(s_dllinstance,"ImmSetCompositionFontW");
        #else //ANSI ONLY
        s_ImmSetCompositionFont= (IMMSETCOMPOSITIONFONT)GetProcAddress(s_dllinstance,"ImmSetCompositionFontA");
        #endif //!ANSI_ONLY
        XP_ASSERT(s_ImmSetCompositionFont);

        s_ImmSetCandidateWindow= (IMMSETCANDIDATEWINDOW)GetProcAddress(s_dllinstance,"ImmSetCandidateWindow");
        XP_ASSERT(s_ImmSetCandidateWindow);

        s_ImmSetCompositionWindow= (IMMSETCOMPOSITIONWINDOW)GetProcAddress(s_dllinstance,"ImmSetCompositionWindow");
        XP_ASSERT(s_ImmSetCompositionWindow);

        #ifdef UNICODE_ONLY
        s_ImmSetCompositionString= (IMMSETCOMPOSITIONSTRING)GetProcAddress(s_dllinstance,"ImmSetCompositionStringW");
        #else //ANSI ONLY
        s_ImmSetCompositionString= (IMMSETCOMPOSITIONSTRING)GetProcAddress(s_dllinstance,"ImmSetCompositionStringA");
        #endif //!ANSI_ONLY
        XP_ASSERT(s_ImmSetCompositionString);

        s_ImmGetConversionStatus= (IMMGETCONVERSIONSTATUS)GetProcAddress(s_dllinstance,"ImmGetConversionStatus");
        XP_ASSERT(s_ImmGetConversionStatus);

        s_ImmSetConversionStatus= (IMMSETCONVERSIONSTATUS)GetProcAddress(s_dllinstance,"ImmSetConversionStatus");
        XP_ASSERT(s_ImmSetConversionStatus);

        #ifdef UNICODE_ONLY
        s_ImmEscape= (IMEESCAPE)GetProcAddress(s_dllinstance,"ImmEscapeW");
        #else //ANSI ONLY
        s_ImmEscape= (IMEESCAPE)GetProcAddress(s_dllinstance,"ImmEscapeA");
        #endif //!ANSI_ONLY
        XP_ASSERT(s_ImmEscape);

    }
}

CIMEDll::~CIMEDll()
{
    if (s_dllinstance)
    {
        if (!FreeLibrary(s_dllinstance))
            XP_ASSERT(0);
        s_refcount--;
    }
    if (!s_refcount)
        s_dllinstance=NULL;
}

BOOL
CIMEDll::ImmNotifyIME(HIMC hIMC,DWORD dwAction,DWORD dwIndex,DWORD dwValue)
{
    if (s_ImmNotifyIME)
        return (*s_ImmNotifyIME)(hIMC,dwAction,dwIndex,dwValue);
    else
        return FALSE;
}

HIMC
CIMEDll::ImmGetContext(HWND hWnd)
{
    if (s_ImmGetContext)
        return (*s_ImmGetContext)(hWnd);
    else
        return FALSE;
}

BOOL 
CIMEDll::ImmReleaseContext(HWND hWnd,HIMC hIMC)
{
    if (s_ImmReleaseContext)
        return (*s_ImmReleaseContext)(hWnd,hIMC);
    else
        return FALSE;
}

LONG 
CIMEDll::ImmGetCompositionString(HIMC hIMC,DWORD dwIndex,LPVOID lpBuf,DWORD dwBufLen)
{
    if (s_ImmGetCompositionString)
        return (*s_ImmGetCompositionString)(hIMC,dwIndex,lpBuf,dwBufLen);
    else
        return FALSE;
}

BOOL 
CIMEDll::ImmSetCompositionFont(HIMC hIMC,LPLOGFONT lpLogFont)
{
    if (s_ImmSetCompositionFont)
        return (*s_ImmSetCompositionFont)(hIMC,lpLogFont);
    else
        return FALSE;
}

BOOL 
CIMEDll::ImmSetCandidateWindow(HIMC hIMC, LPCANDIDATEFORM lpcandidate)
{
    if (s_ImmSetCandidateWindow)
        return (*s_ImmSetCandidateWindow)(hIMC,lpcandidate);
    else
        return FALSE;
}

BOOL 
CIMEDll::ImmSetCompositionWindow(HIMC hIMC,LPCOMPOSITIONFORM lpcomposition)
{
    if (s_ImmSetCompositionWindow)
        return (*s_ImmSetCompositionWindow)(hIMC,lpcomposition);
    else
        return FALSE;
}

BOOL
CIMEDll::ImmSetCompositionString(HIMC hIMC,DWORD dwIndex,LPVOID lpComp,DWORD dwCompLen,LPVOID lpRead,DWORD dwReadLen)
{
    if (s_ImmSetCompositionString)
        return (*s_ImmSetCompositionString)(hIMC,dwIndex,lpComp,dwCompLen,lpRead,dwReadLen);
    else
        return FALSE;
}


BOOL
CIMEDll::ImmSetConversionStatus(HIMC hIMC,DWORD fdwConversion,DWORD fdwSentence)
{
    if (s_ImmSetConversionStatus)
        return (*s_ImmSetConversionStatus)(hIMC,fdwConversion,fdwSentence);
    else
        return FALSE;
}

BOOL
CIMEDll::ImmGetConversionStatus(HIMC hIMC,LPDWORD lpfdwConversion,LPDWORD lpfdwSentence)
{
    if (s_ImmGetConversionStatus)
        return (*s_ImmGetConversionStatus)(hIMC,lpfdwConversion,lpfdwSentence);
    else
        return FALSE;
}

LRESULT 
CIMEDll::ImeEscape(HKL hKL,HIMC hIMC,UINT uEscape,LPVOID lpData)
{
    if (s_ImmEscape)
        return (*s_ImmEscape)(hKL,hIMC,uEscape,lpData);
    else
        return FALSE;
}

