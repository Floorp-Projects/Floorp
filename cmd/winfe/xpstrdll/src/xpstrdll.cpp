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

// xpstrdll.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "xpstrdll.h"

HINSTANCE g_instance; //global instance of this dll

#ifdef WIN32

BOOL   WINAPI
DllMain (HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    g_instance=(HINSTANCE)hInst;
    switch( ul_reason_for_call ) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
    default:break; 
    }
    return TRUE;
}

#endif //WIN32



#ifndef WIN32
extern "C" int CALLBACK
LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeap, LPSTR lpszCmdLine )
{
    g_instance=hinst;
    return TRUE;

}

#endif //NOT WIN32
