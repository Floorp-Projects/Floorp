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

/* mail news client wrapper */
/*Dynamic Library wrapper for loading on call*/

#include "stdafx.h"
#include "mnrccln.h"

#ifdef WIN32
#define MAILNEWSDLL "MNRC32.DLL"
#else
#define MAILNEWSDLL "MNRC16.DLL"
#endif

HINSTANCE CMailNewsResourceDll::s_dllinstance=NULL;
unsigned int CMailNewsResourceDll::s_refcount=0;

CMailNewsResourceDll::CMailNewsResourceDll()
{
    // Running under Win3.1, if the default drive is A or B and the disk
    //  was removed, this tries to load from the default drive.
    //  So change to C before loading. (A=1, B=2, C=3)
    if( _getdrive() < 3 ){
        _chdrive(3);
    }
    if (s_refcount)
    {
        s_dllinstance=LoadLibrary(MAILNEWSDLL);
        s_refcount++;
        return;
    }
    else
    {
        s_dllinstance=LoadLibrary(MAILNEWSDLL);
        XP_ASSERT(s_dllinstance);
        s_refcount++;
        if(!s_dllinstance)
            return;
    }
}

CMailNewsResourceDll::~CMailNewsResourceDll()
{
#ifdef WIN32
    if (!FreeLibrary(s_dllinstance))
        XP_ASSERT(0);
#else
    FreeLibrary(s_dllinstance);
#endif
       s_refcount--;
}

HINSTANCE
CMailNewsResourceDll::switchResources()
{
    XP_ASSERT(s_refcount);
    HINSTANCE t_hinstance=AfxGetResourceHandle();
    AfxSetResourceHandle(s_dllinstance);
    return t_hinstance;
}
