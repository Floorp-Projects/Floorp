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

// oleregis.cpp : implementation file
//

#include "stdafx.h"

#include "oleregis.h"

#include "regproto.h"
#include "olectc.h"
#include "presentm.h"
#include "olestart.h"
#include "oleshut.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COleRegistry

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(COleRegistry, CCmdTarget)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

COleRegistry::COleRegistry()
{
	EnableAutomation();
	
	// To keep the application running as long as an OLE automation 
	//	object is active, the constructor calls AfxOleLockApp.
	
	AfxOleLockApp();
}

COleRegistry::~COleRegistry()
{
	// To terminate the application when all objects created with
	// 	with OLE automation, the destructor calls AfxOleUnlockApp.
	
	AfxOleUnlockApp();
}

void COleRegistry::OnFinalRelease()
{
	// When the last reference for an automation object is released
	//	OnFinalRelease is called.  This implementation deletes the 
	//	object.  Add additional cleanup required for your object before
	//	deleting it from memory.

	delete this;
}

void COleRegistry::RegisterIniProtocolHandlers()    {
//	Purpose:    Read in the INI entries, and register them as valid automated handlers.
//	Arguments:  void
//	Returns:    void

    //  Over 2K is a nightmare.
    char *aBuffer = new char[4096];
    theApp.GetPrivateProfileString("Automation Protocols", NULL, "", aBuffer, 4096, AfxGetApp()->m_pszProfileName);

    //  We have a double null terminated list of mime types here.
    //  Go ahead and go through each one, and then obtain the value for each.
    char *pTraverse = aBuffer;
    CString csServerName;
    while(*pTraverse != '\0')   {
        //  Get the entry for this mime type.
        csServerName = theApp.GetProfileString("Automation Protocols", pTraverse);
        if(csServerName.IsEmpty() != TRUE)  {
            //  Have a valid entry.  Cause it to be registered.
            CString csProtocol = pTraverse;
            COLEProtocolItem::OLERegister(csProtocol, csServerName);
        }

        //  Go on to the next entry.
        while(*pTraverse != '\0')   {
            pTraverse++;
        }
        pTraverse++;
    }

	delete[] aBuffer;
}

void COleRegistry::RegisterIniViewers() {
//	Purpose:    Read in the INI entries, and register them as valid automated viewers.
//	Arguments:  void
//	Returns:    void
//	Comments:   This should really only be called after all other normal viewers are registered.

    //  Over 2K is a nightmare.
    char *aBuffer = new char[4096];
    theApp.GetPrivateProfileString("Automation Viewers", NULL, "", aBuffer, 4096, AfxGetApp()->m_pszProfileName);

    //  We have a double null terminated list of mime types here.
    //  Go ahead and go through each one, and then obtain the value for each.
    char *pTraverse = aBuffer;
    CString csServerName;
    while(*pTraverse != '\0')   {
        //  Get the entry for this mime type.
        csServerName = theApp.GetProfileString("Automation Viewers", pTraverse);
        if(csServerName.IsEmpty() != TRUE)  {
            //  Have a valid entry.  Cause it to be registered.
            COLEStreamData *pOData = new COLEStreamData(csServerName, pTraverse);
            if(FALSE == WPM_RegisterContentTypeConverter(pTraverse, FO_PRESENT, (void *)pOData, ole_stream, TRUE))  {
                //  Couldn't do it.
                delete pOData;
            }
        }

        //  Go on to the next entry.
        while(*pTraverse != '\0')   {
            pTraverse++;
        }
        pTraverse++;
    }

	delete[] aBuffer;
}

//  Cause all startup watchers to be fired.
//  Depending on the number, this could decrease startup time significantly.
//  Be sure to always have the splash screen say what's going on, so the user
//      knows it's the fault of another application.
void COleRegistry::Startup()
{
    //  Go through all entries under "Automation Startup"
    char *aBuffer = new char[4096];
    theApp.GetPrivateProfileString("Automation Startup", NULL, "", aBuffer, 4096, AfxGetApp()->m_pszProfileName);

    //  We have a double null terminated list of OLE objects here.
    //  Go ahead and go through each one.
    char *pTraverse = aBuffer;
    CString csServerName;
    while(*pTraverse != '\0')   {
        //  Get the entry for this object.
        csServerName = pTraverse;
        if(csServerName.IsEmpty() != TRUE)  {
            //  Update the splash screen if present.
            if(theApp.m_splash.m_hWnd && theApp.m_splash.IsWindowVisible())   {
                theApp.m_splash.DisplayStatus((LPCSTR)csServerName);
            }

            //  start up a conversation with out startup wannabe.
            IStartup isNotify;
            TRY {
                isNotify.CreateDispatch(csServerName);
                isNotify.Initialize((long)theApp.m_hInstance);
            }
            CATCH(CException, e)    {
                //  Couldn't inform/create the object.
                TRACE("Couldn't notify startup object %s\n", (const char *)csServerName);
            }
            END_CATCH
        }

        //  Go on to the next entry.
        while(*pTraverse != '\0')   {
            pTraverse++;
        }
        pTraverse++;
    }

	delete[] aBuffer;
}

//  Cause all startup watchers to be fired.
//  Depending on the number, this could decrease shutup time significantly.
void COleRegistry::Shutdown()
{
    //  Go through all entries under "Automation Shutdown"
    char *aBuffer = new char[4096];
    theApp.GetPrivateProfileString("Automation Shutdown", NULL, "", aBuffer, 4096, AfxGetApp()->m_pszProfileName);

    //  We have a double null terminated list of OLE objects here.
    //  Go ahead and go through each one.
    char *pTraverse = aBuffer;
    CString csServerName;
    while(*pTraverse != '\0')   {
        //  Get the entry for this object.
        csServerName = pTraverse;
        if(csServerName.IsEmpty() != TRUE)  {
            //  start up a conversation with out startup wannabe.
            IShutdown isNotify;
            TRY {
                isNotify.CreateDispatch(csServerName);
                isNotify.Initialize((long)theApp.m_hInstance);
            }
            CATCH(CException, e)    {
                //  Couldn't inform/create the object.
                TRACE("Couldn't notify shutdown object %s\n", (const char *)csServerName);
            }
            END_CATCH
        }

        //  Go on to the next entry.
        while(*pTraverse != '\0')   {
            pTraverse++;
        }
        pTraverse++;
    }

	delete[] aBuffer;
}

BEGIN_MESSAGE_MAP(COleRegistry, CCmdTarget)
	//{{AFX_MSG_MAP(COleRegistry)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(COleRegistry, CCmdTarget)
	//{{AFX_DISPATCH_MAP(COleRegistry)
	DISP_FUNCTION(COleRegistry, "RegisterViewer", RegisterViewer, VT_BOOL, VTS_BSTR VTS_BSTR)
	DISP_FUNCTION(COleRegistry, "RegisterProtocol", RegisterProtocol, VT_BOOL, VTS_BSTR VTS_BSTR)
	DISP_FUNCTION(COleRegistry, "RegisterStartup", RegisterStartup, VT_BOOL, VTS_BSTR)
	DISP_FUNCTION(COleRegistry, "RegisterShutdown", RegisterShutdown, VT_BOOL, VTS_BSTR)
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

IMPLEMENT_OLECREATE(COleRegistry, "Netscape.Registry.1", 0xe67d6a10, 0x4438, 0x11ce, 0x8c, 0xe4, 0x0, 0x20, 0xaf, 0x18, 0xf9, 0x5)

/////////////////////////////////////////////////////////////////////////////
// COleRegistry message handlers

BOOL COleRegistry::RegisterViewer(LPCTSTR pMimeType, LPCTSTR pRegistryName) {
//	Purpose:    Register a particular autmation server as an External Viewer.
//	Arguments:  pMimeType   The mime type they'd like to handle.
//              pRegistryName   The name of the registered automation server, as known to the system registry.
//	Returns:    BOOL    TRUE    The registration was a success.
//                      FALSE   The registration failed, another OLE automation server is already registered for said
//                                  MimeType.
//	Comments:   One way registration; is probably wrong, but future revisions will address this problem.

    TRACE("RegisterViewer(%s, %s)\n", pMimeType, pRegistryName);

    //  If it's not a valid request, reject it right now.
    if(pMimeType == NULL || pRegistryName == NULL)  {
        return(FALSE);
    }

    //  Allright, create a new item to attempt to add to the list of registered viewers.
    COLEStreamData *pOData = new COLEStreamData(pRegistryName, pMimeType);
    if(FALSE == WPM_RegisterContentTypeConverter((char *)pMimeType, FO_PRESENT, (void *)pOData, ole_stream, TRUE))  {
        //  Couldn't do it.
        //  Tell them that a converter is already registered, be it DDE or OLE.
        delete pOData;
        return(FALSE);
    }


    //  Add the string to the INI file for retrieval on startup.
    theApp.WriteProfileString("Automation Viewers", pMimeType, pRegistryName);

	return(TRUE);
}

BOOL COleRegistry::RegisterProtocol(LPCTSTR pProtocol, LPCTSTR pRegistryName)    {
//	Purpose:    Register a particular autmation server as an External Protocol handler.
//	Arguments:  pProtocol   The protocol they'd like to handle.
//              pRegistryName   The name of the registered automation server, as known to the system registry.
//	Returns:    BOOL    TRUE    The registration was a success.
//                      FALSE   The registration failed, another OLE automation server is already registered for said
//                                  Protocol.
//	Comments:   One way registration; is probably wrong, but future revisions will address this problem.

    TRACE("RegisterProtocol(%s, %s)\n", pProtocol, pRegistryName);

    //  First, return failure on any stupid cases.
    if(pProtocol == NULL || pRegistryName == NULL)  {
        return(FALSE);
    }

    CString csProtocol = pProtocol;
    CString csServiceName = pRegistryName;
	return(COLEProtocolItem::OLERegister(csProtocol, csServiceName));
}

//  New startup object being registered.
//  Save to Registry/INI.
BOOL COleRegistry::RegisterStartup(LPCTSTR pRegistryName)
{
    theApp.WriteProfileString("Automation Startup", pRegistryName, "");

    //  Always succeed for now.
    return (TRUE);
}

//  New shutdown object being registered.
//  Save to Registry/INI.
BOOL COleRegistry::RegisterShutdown(LPCTSTR pRegistryName)
{
    theApp.WriteProfileString("Automation Shutdown", pRegistryName, "");

    //  Always succeed for now.
    return (TRUE);
}
