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
#include "olehelp.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COleHelp

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(COleHelp, CCmdTarget)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

COleHelp::COleHelp()
{
	EnableAutomation();
	
	// To keep the application running as long as an OLE automation 
	//	object is active, the constructor calls AfxOleLockApp.
    TRACE("Creating COleHelp\n");
	
	AfxOleLockApp();

    //  Create a slave context for us to use.
    m_pSlaveCX = new CStubsCX(::HtmlHelp, MWContextHTMLHelp);
}

COleHelp::~COleHelp()
{
	// To terminate the application when all objects created with
	// 	with OLE automation, the destructor calls AfxOleUnlockApp.
    TRACE("Destroying COleHelp\n");
	
	AfxOleUnlockApp();

    //  Destroy the slave context.
    if(m_pSlaveCX)  {
        m_pSlaveCX->NiceDestroyContext();
    }
}


void COleHelp::OnFinalRelease()
{
	// When the last reference for an automation object is released
	// OnFinalRelease is called.  The base class will automatically
	// deletes the object.  Add additional cleanup required for your
	// object before calling the base class.

	CCmdTarget::OnFinalRelease();
}


BEGIN_MESSAGE_MAP(COleHelp, CCmdTarget)
	//{{AFX_MSG_MAP(COleHelp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(COleHelp, CCmdTarget)
	//{{AFX_DISPATCH_MAP(COleHelp)
	DISP_FUNCTION(COleHelp, "HtmlHelp", HtmlHelp, VT_EMPTY, VTS_BSTR VTS_BSTR VTS_BSTR)
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_IOleHelp to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

#ifndef MOZ_COMMUNICATOR_IIDS
/* 2e696aa0-f8ad-11d0-b09a-00805f8a1db7 */
static const IID IID_IOleHelp =
{ 0x2e696aa0, 0xf8ad, 0x11d0, {0xb0, 0x9a, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7} };
#else
// {60403D80-872B-11CF-ACC8-0080C82BE3B6}
static const IID IID_IOleHelp =
{ 0x60403d80, 0x872b, 0x11cf, { 0xac, 0xc8, 0x0, 0x80, 0xc8, 0x2b, 0xe3, 0xb6 } };
#endif /* MOZ_COMMUNICATOR_IIDS */

BEGIN_INTERFACE_MAP(COleHelp, CCmdTarget)
	INTERFACE_PART(COleHelp, IID_IOleHelp, Dispatch)
END_INTERFACE_MAP()

#ifndef MOZ_COMMUNICATOR_IIDS
// {60403D81-872B-11CF-ACC8-0080C82BE3B6}
IMPLEMENT_OLECREATE(COleHelp, "Netscape.Help.1", 0xd0b2c060, 0xf8ad, 0x11d0, 0xb0, 0x9a, 0x0, 0x80, 0x5f, 0x8a, 0x1d, 0xb7)
#else
// {60403D81-872B-11CF-ACC8-0080C82BE3B6}
IMPLEMENT_OLECREATE(COleHelp, "Netscape.Help.1", 0x60403d81, 0x872b, 0x11cf, 0xac, 0xc8, 0x0, 0x80, 0xc8, 0x2b, 0xe3, 0xb6)
#endif // MOZ_COMMUNICATOR_IIDS

/////////////////////////////////////////////////////////////////////////////
// COleHelp message handlers

void COleHelp::HtmlHelp(LPCTSTR pMapFileUrl, LPCTSTR pId, LPCTSTR pSearch) 
{
    //  Make sure we have a slave context for us to use.
    if(m_pSlaveCX && !m_pSlaveCX->IsDestroyed())  {
        char *p_Map = NULL;
        char *p_Id = NULL;
        char *p_Search = NULL;

        if(pMapFileUrl) {
            p_Map = XP_STRDUP(pMapFileUrl);

            CString csUrl;
	        WFE_ConvertFile2Url(csUrl, p_Map);
	        XP_FREE(p_Map);
	        p_Map = XP_STRDUP(csUrl);
        }
        if(pId) {
            p_Id = XP_STRDUP(pId);
        }
        if(pSearch) {
            p_Search = XP_STRDUP(pSearch);
        }

        NET_GetHTMLHelpFileFromMapFile(m_pSlaveCX->GetContext(), p_Map, p_Id, p_Search);

        if(p_Map) {
            XP_FREE(p_Map);
        }
        if(p_Id) {
            XP_FREE(p_Id);
        }
        if(p_Search) {
            XP_FREE(p_Search);
        }
    }
}
