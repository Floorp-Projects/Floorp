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
#include "slavewnd.h"

#ifndef WIN32
#include "ddeml2.h"
#else
#include <ddeml.h>
#endif // WIN32

/*-----------------------------------------------------------------------**
** Purpose of this file is to provide a DDE server which we use to catch **
**     shell events like shell\open\command, etc.                        **
**-----------------------------------------------------------------------*/

class CNSShell {
public:
    CNSShell();
    ~CNSShell();
private:
    UINT m_uServerActive;
    DWORD m_dwID;
    HSZ m_hszServerName;
    HDDEDATA m_hddNameService;
    void *m_pSlaveCookie;
public:
    HDDEDATA shell(UINT type, UINT fmt, HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hData, DWORD dwData1, DWORD dwData2);
    void ExitInstance();
};

//  One global.
CNSShell nsshell;

//  Dde callback function.
HDDEDATA CALLBACK 
#ifndef _WIN32
_export 
#endif
_nsshell(UINT type, UINT fmt, HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hData, DWORD dwData1, DWORD dwData2)   {
    HDDEDATA hRetval = NULL;
    hRetval = nsshell.shell(type, fmt, hconv, hsz1, hsz2, hData, dwData1, dwData2);
    return(hRetval);
}

//  slavewnd callback function.
void nsshell_ExitInstance(UINT uMessage, WPARAM wParam, LPARAM lParam) {
    ASSERT(SLAVE_EXITINSTANCE == uMessage);
    if(SLAVE_EXITINSTANCE == uMessage) {
        nsshell.ExitInstance();
    }
}

CNSShell::CNSShell() {
    //  Initialize variables.
    m_uServerActive = 0;
    m_dwID = 0;
    m_hszServerName = NULL;
    m_hddNameService = NULL;
    m_pSlaveCookie = NULL;
    
    //  Register to receive the exit instance event, such that we know
    //      to turn off the DDE server.
    m_pSlaveCookie = slavewnd.Register(SLAVE_EXITINSTANCE, nsshell_ExitInstance);
    
    //  Initialize a DDE server.
    m_uServerActive = ::DdeInitialize(&m_dwID, _nsshell, APPCLASS_STANDARD, 0);
    if(DMLERR_NO_ERROR == m_uServerActive) {
        m_hszServerName = ::DdeCreateStringHandle(m_dwID, "nsshell", CP_WINANSI);
        if(m_hszServerName) {
            m_hddNameService = ::DdeNameService(m_dwID, m_hszServerName, NULL, DNS_REGISTER);
        }
    }
}

CNSShell::~CNSShell() {
    //  Remove our slave window callback.
    if(m_pSlaveCookie) {
        BOOL bAssert = slavewnd.UnRegister(m_pSlaveCookie);
        ASSERT(bAssert);
        m_pSlaveCookie = NULL;
    }

    //  Ensure ExitInstance was called.
    if(m_dwID) {
        ExitInstance();
    }
}

void CNSShell::ExitInstance() {
    //  De-initialize DDE server.
    if(DMLERR_NO_ERROR == m_uServerActive) {
        if(m_dwID) {
            if(m_hszServerName) {
                if(m_hddNameService) {
                    ::DdeNameService(m_dwID, m_hszServerName, NULL, DNS_UNREGISTER);
                    m_hddNameService = NULL;
                }
                ::DdeFreeStringHandle(m_dwID, m_hszServerName);
                m_hszServerName = NULL;
            }
            DdeUninitialize(m_dwID);
            m_dwID = 0;
        }
    }
    m_uServerActive = 0;
}

HDDEDATA CNSShell::shell(UINT type, UINT fmt, HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hData, DWORD dwData1, DWORD dwData2) {
    HDDEDATA hResult = NULL;

    switch(type)    {
    //  Only need to handle execute type transactions from shell.
    case XTYP_EXECUTE:  {
        hResult = (HDDEDATA)DDE_FNOTPROCESSED;
        if(hData) {
            DWORD dwSize = ::DdeGetData(hData, NULL, 0, 0);
            if(dwSize) {
                char *pMemory = new char[dwSize];
                if(pMemory) {
                    if(::DdeGetData(hData, (BYTE *)pMemory, dwSize, 0)) {
                        //  Don't hand over to netscape if in init instance
                        //      (assume command is on command line).
                        //  Acknowledge the request however.
                        if(theApp.m_bInInitInstance) {
                            hResult = (HDDEDATA)DDE_FACK;
                        }
                        //  Don't handle if we are exiting also.
                        //  No warning is given that nothing will happen,
                        //      but we avoid crashing.
                        else if(theApp.m_bExitStatus) {
                            hResult = (HDDEDATA)DDE_FACK;
                        }
                        //  We receive URLs when there is no leading '['.
                        //  Add [openurl("")] to it if so.
                        else if('[' != pMemory[0]) {
                            char *pArg = new char[dwSize + 13];
                            if(pArg) {
                                strcpy(pArg, "[openurl(");
                                if('\"' != pMemory[0]) {
                                    strcat(pArg, "\"");
                                }
                                strcat(pArg, pMemory);
                                if('\"' != pMemory[0]) {
                                    strcat(pArg, "\"");
                                }
                                strcat(pArg, ")]");
                                
                                //  Have Netscape handle it.
                                if(theApp.OnDDECommand(pArg)) {
                                    hResult = (HDDEDATA)DDE_FACK;
                                }
                                
                                delete[] pArg;
                                pArg = NULL;
                            }
                        }
                        //  Have Netscape handle it as is.
                        else {
                            if(theApp.OnDDECommand(pMemory)) {
                                hResult = (HDDEDATA)DDE_FACK;
                            }
                        }
                    }
                    delete[] pMemory;
                    pMemory = NULL;
                }
            }
        }
        
        break;
                        }
    //  Accept any connection.
    case XTYP_CONNECT:  {
        hResult = (HDDEDATA)TRUE;
        break;
                        }
    }

    return(hResult);
}

