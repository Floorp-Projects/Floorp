/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/***********************************************************************\
*                                                                       *
* WINNLS.H - Far East input method editor (DBCS_IME) definitions        *
*                                                                       *
* History:                                                              *
* 18-Jub-1992   yutakan                                                 *
*               API definitions for final JAPAN spec.                   *
* 21-Oct-1991   bent                                                    *
*               initial merge of Far East 3.0 versions                  *
*               Should be updated to resolve local inconsistencies.     *
*                                                                       *
* Copyright (c) 1990-1992  Microsoft Corporation                        *
*                                                                       *
\***********************************************************************/

typedef struct _tagDATETIME {
    WORD        year;
    WORD        month;
    WORD        day;
    WORD        hour;
    WORD        min;
    WORD        sec;
} DATETIME;

typedef struct _tagIMEPRO {
    HWND        hWnd;
    DATETIME    InstDate;
    UINT        wVersion;
    BYTE        szDescription[50];
    BYTE        szName[80];
    BYTE        szOptions[30];
} IMEPRO;
typedef IMEPRO      *PIMEPRO;
typedef IMEPRO near *NPIMEPRO;
typedef IMEPRO far  *LPIMEPRO;

BOOL WINAPI IMPGetIME( HWND, LPIMEPRO );
BOOL WINAPI IMPQueryIME( LPIMEPRO );
BOOL WINAPI IMPSetIME( HWND, LPIMEPRO );

BOOL WINAPI WINNLSEnableIME( HWND, BOOL );
BOOL WINAPI WINNLSGetEnableStatus( HWND );


UINT WINAPI WINNLSGetIMEHotkey( HWND );


BOOL WINAPI WINNLSDefIMEProc(HWND, HDC, WPARAM, WPARAM,  LPARAM, LPARAM);
LRESULT WINAPI ControlIMEMessage(HWND, LPIMEPRO, WPARAM, WPARAM, LPARAM);
BOOL WINAPI IMPRetrieveIME(LPIMEPRO, WPARAM);
