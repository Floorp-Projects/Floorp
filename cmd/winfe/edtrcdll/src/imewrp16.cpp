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
#include "assert.h"

/*imewrap.h edtor client wrapper */
/*Dynamic Library wrapper for loading on call*/

HINSTANCE           CIMEDll::s_dllinstance=NULL;
unsigned int        CIMEDll::s_refcount=0;
SENDIMEMESSAGE      CIMEDll::s_SendIMEMessage=NULL;
SENDIMEMESSAGEEX    CIMEDll::s_SendIMEMessageEx=NULL;
IMPGETIME           CIMEDll::s_IMPGetIME=NULL;


CIMEDll::CIMEDll()
{
    HINSTANCE t_instance=LoadLibrary("WINNLS.DLL");
    if (t_instance<=HINSTANCE_ERROR)
    {
        s_dllinstance=NULL;
        s_refcount=0;
        s_SendIMEMessage=NULL;
        s_SendIMEMessageEx=NULL;
        s_IMPGetIME=NULL;
        return;
    }
    s_refcount++;
    //retrieve all proc addresses and place them into holders
    s_IMPGetIME= (IMPGETIME)GetProcAddress(t_instance,"IMPGetIME");
    assert(s_IMPGetIME);

    s_SendIMEMessageEx= (SENDIMEMESSAGEEX)GetProcAddress(t_instance,"SendIMEMessageEx");
    assert(s_SendIMEMessageEx);

    s_SendIMEMessage= (SENDIMEMESSAGE)GetProcAddress(t_instance,"SendIMEMessage");
    assert(s_SendIMEMessage);
    if (!s_SendIMEMessage||!s_SendIMEMessageEx||!s_IMPGetIME)
	    return;
    s_dllinstance=t_instance;
}

CIMEDll::~CIMEDll()
{
    if (s_dllinstance!=NULL)
    {
        FreeLibrary(s_dllinstance);
        s_refcount--;
    }
    if (!s_refcount)
        s_dllinstance=NULL;
}


BOOL
__loadds CIMEDll::IMPGetIME(HWND hWnd,LPIMEPRO p_lpimepro)
{
    if (s_IMPGetIME)
        return (*s_IMPGetIME)(hWnd,p_lpimepro);
    else
        return FALSE;
}


WORD
__loadds CIMEDll::SendIMEMessage(HWND hWnd,LPARAM p_lparam)
{
    if (s_SendIMEMessage)
        return (*s_SendIMEMessage)(hWnd,p_lparam);
    else
        return FALSE;
}


LRESULT
__loadds CIMEDll::SendIMEMessageEx(HWND hWnd,LPARAM p_lparam)
{
    if (s_SendIMEMessageEx)
        return (*s_SendIMEMessageEx)(hWnd,p_lparam);
    else
        return FALSE;
}
