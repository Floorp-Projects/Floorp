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

// CTalkSiteMgr and CTalkNav: implementation file
//
#include "stdafx.h"

#ifdef EDITOR
#ifdef _WIN32

#include "Talk.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////
//   Callback to find an instance of SiteManager
//
BOOL CALLBACK EXPORT FindSiteMgr( HWND hwnd, LPARAM lParam )
{
    if ( 0x015DEAD0 == ::SendMessage( hwnd, WM_SITE_MANAGER, SM_QUERY_WINDOW, 0 ) )
    {
        // We found it quit enumerating
        *((BOOL*)lParam) = TRUE;
        return FALSE;
    }
    return TRUE;
}

//  Called at app startup during InitInstance().
//  Return TRUE if we found sitemanager running
BOOL FE_FindSiteMgr()
{
    BOOL bRetVal = 0;
    EnumWindows( FindSiteMgr, LPARAM(&bRetVal) );
    return bRetVal;
}

/////////////////////////////////////////////////////////////////////////////
// CTalkNav  OLE Automation Server so SiteManager can invoke us

IMPLEMENT_DYNCREATE(CTalkNav, CCmdTarget)

CTalkNav::CTalkNav()
{
	EnableAutomation();
	
	// To keep the application running as long as an OLE automation 
	//	object is active, the constructor calls AfxOleLockApp.
	AfxOleLockApp();
}

CTalkNav::~CTalkNav()
{
	// To terminate the application when all objects created with
	// 	with OLE automation, the destructor calls AfxOleUnlockApp.
	
	AfxOleUnlockApp();
}


void CTalkNav::OnFinalRelease()
{
	// When the last reference for an automation object is released
	// OnFinalRelease is called.  The base class will automatically
	// deletes the object.  Add additional cleanup required for your
	// object before calling the base class.

	CCmdTarget::OnFinalRelease();
}


BEGIN_MESSAGE_MAP(CTalkNav, CCmdTarget)
	//{{AFX_MSG_MAP(CTalkNav)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CTalkNav, CCmdTarget)
	//{{AFX_DISPATCH_MAP(CTalkNav)
	DISP_FUNCTION(CTalkNav, "BrowseURL", BrowseURL, VT_I4, VTS_BSTR)
	DISP_FUNCTION(CTalkNav, "EditURL", EditURL, VT_I4, VTS_BSTR)
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_ITalkNav to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

// {E328732B-9DC9-11CF-92D0-004095E27A10}
static const IID IID_ITalkNav =
{ 0xe328732b, 0x9dc9, 0x11cf, { 0x92, 0xd0, 0x0, 0x40, 0x95, 0xe2, 0x7a, 0x10 } };

BEGIN_INTERFACE_MAP(CTalkNav, CCmdTarget)
	INTERFACE_PART(CTalkNav, IID_ITalkNav, Dispatch)
END_INTERFACE_MAP()

// {E328732C-9DC9-11CF-92D0-004095E27A10}
IMPLEMENT_OLECREATE(CTalkNav, "Netscape.TalkNav.1", 0xe328732c, 0x9dc9, 0x11cf, 0x92, 0xd0, 0x0, 0x40, 0x95, 0xe2, 0x7a, 0x10)

/////////////////////////////////////////////////////////////////////////////
// CTalkNav message handlers

long CTalkNav::BrowseURL(LPCTSTR url) 
{
    FE_LoadUrl( (char*)url, LOAD_URL_NAVIGATOR);
	return 1;
}

long CTalkNav::EditURL(LPCTSTR url) 
{
    FE_LoadUrl( (char*)url, LOAD_URL_COMPOSER);
	return 1;
}
////////////////////////////////////////////////////////////
//
// Sitemanager has an OLE automation server that responds to this
//
/////////////////////////////////////////////////////////////////////////////
// ITalkSMClient operations

ITalkSMClient::ITalkSMClient(void) :
    m_alive(FALSE),
    m_connected(FALSE),
    m_retried(FALSE)
{
    CLSID clsid;
    m_registered = 0;
#if defined(MSVC4) 
    USES_CONVERSION;
    LPCOLESTR pOleStr = A2CW("Netscape.TalkSiteMgr.1");
#else
    const char * pOleStr[] = "Netscape.TalkSiteMgr.1";
#endif

    if (S_OK == CLSIDFromProgID(pOleStr, &clsid))
        m_registered = 1;
}

ITalkSMClient::~ITalkSMClient(void)
{
    Disconnect();
}

BOOL ITalkSMClient::Connect(void)
{
    m_connected = CreateDispatch("Netscape.TalkSiteMgr.1");
    m_retried = FALSE;
    m_alive = m_connected;
    return m_connected;
}

void ITalkSMClient::Disconnect(void)
{
    if (m_connected) ReleaseDispatch();
    m_connected = FALSE;
}

BOOL ITalkSMClient::Reconnect(void)
{
    if (m_retried) return FALSE;
    Disconnect();
    Connect();
    m_retried = TRUE;
    return m_connected;
}

BOOL ITalkSMClient::IsConnected(void)
{
    if (!m_registered) return FALSE;
    if (!m_connected) Connect();
    return m_connected;
}

long ITalkSMClient::LoadingURL(LPCTSTR url)
{
    if (!m_alive) return 0;
    if (!IsConnected()) return 0;

    BOOL failed;

Retry:
    long result = 0;
    static BYTE BASED_CODE parms[] = VTS_BSTR;
    TRY
    {
        failed = FALSE;
        InvokeHelper(0x1, DISPATCH_METHOD, VT_I4, (void*)&result, parms, url);
    }
    CATCH(CException, e)
    {
        failed = TRUE;
    }
    END_CATCH;

    if (!failed) return result;
    if (!Reconnect()) return 0;
    goto Retry;
}

long ITalkSMClient::SavedURL(LPCTSTR url)
{
    if (!m_alive) return 0;
    if (!IsConnected()) return 0;

    BOOL failed;

Retry:
    long result = 0;
    static BYTE BASED_CODE parms[] = VTS_BSTR;
    TRY
    {
        failed = FALSE;
        InvokeHelper(0x2, DISPATCH_METHOD, VT_I4, (void*)&result, parms, url);
    }
    CATCH(CException, e)
    {
        failed = TRUE;
    }
    END_CATCH;

    if (!failed) return result;
    if (!Reconnect()) return 0;
    goto Retry;
}

long ITalkSMClient::BecomeActive()
{
    if (!IsConnected()) return 0;

    BOOL failed;

Retry:
    long result = 0;
    TRY
    {
        failed = FALSE;
        InvokeHelper(0x3, DISPATCH_METHOD, VT_I4, (void*)&result, NULL);
    }
    CATCH(CException, e)
    {
        failed = TRUE;
    }
    END_CATCH;

    if (!failed) return result;
    if (!Reconnect()) return 0;
    goto Retry;
}

#endif // _WIN32
#endif // EDITOR
