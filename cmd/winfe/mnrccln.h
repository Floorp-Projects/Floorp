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

/* mailnews resource client wrapper */
/*Dynamic Library wrapper for loading on call*/
#ifndef _MNRCCLN_H

#define _MNRCCLN_H

class CMailNewsResourceDll
{
    static HINSTANCE s_dllinstance;
    static unsigned int s_refcount;

public:
    HINSTANCE switchResources();
    CMailNewsResourceDll();
    ~CMailNewsResourceDll();
};

//thread safe resource handle switcher. also reference counts the dll. ya!
class CMailNewsResourceSwitcher:public CMailNewsResourceDll
{
    HINSTANCE m_oldresourcehandle;
public:
    CMailNewsResourceSwitcher(){m_oldresourcehandle=switchResources();}
    void Reset(){AfxSetResourceHandle(m_oldresourcehandle);}
    ~CMailNewsResourceSwitcher(){Reset();}
};

#endif //_MNRCCLN_H

