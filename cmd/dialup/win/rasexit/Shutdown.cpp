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
// Shutdown.cpp : implementation file
//

#include "stdafx.h"
#include "rasexit.h"
#include "Shutdown.h"
#include <ras.h>
#include <raserror.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Shutdown

IMPLEMENT_DYNCREATE(Shutdown, CCmdTarget)

Shutdown::Shutdown()
{
	EnableAutomation();
	
	// To keep the application running as long as an OLE automation 
	//	object is active, the constructor calls AfxOleLockApp.
	TRACE("Createing Shudown\n");
	AfxOleLockApp();
}

Shutdown::~Shutdown()
{
	// To terminate the application when all objects created with
	// 	with OLE automation, the destructor calls AfxOleUnlockApp.
	TRACE("Destroying shutdown\n");
	AfxOleUnlockApp();
}


void Shutdown::OnFinalRelease()
{
	// When the last reference for an automation object is released
	// OnFinalRelease is called.  The base class will automatically
	// deletes the object.  Add additional cleanup required for your
	// object before calling the base class.

	CCmdTarget::OnFinalRelease();
}


BEGIN_MESSAGE_MAP(Shutdown, CCmdTarget)
	//{{AFX_MSG_MAP(Shutdown)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(Shutdown, CCmdTarget)
	//{{AFX_DISPATCH_MAP(Shutdown)
	DISP_FUNCTION(Shutdown, "Initialize", Initialize, VT_EMPTY, VTS_I4)
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_IShutdown to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

// {54680788-CB5B-11CF-893E-0800091AC64E}
static const IID IID_IShutdown =
{ 0x54680788, 0xcb5b, 0x11cf, { 0x89, 0x3e, 0x8, 0x0, 0x9, 0x1a, 0xc6, 0x4e } };

// do we need to change this too?
// {7C851530-EC12-11d0-A36B-00805F36BC04}
// static const IID IID_IShutdown = 
// { 0x7c851530, 0xec12, 0x11d0, { 0xa3, 0x6b, 0x0, 0x80, 0x5f, 0x36, 0xbc, 0x4 } };


BEGIN_INTERFACE_MAP(Shutdown, CCmdTarget)
	INTERFACE_PART(Shutdown, IID_IShutdown, Dispatch)
END_INTERFACE_MAP()


//*************************/
// PE 3.0 guid!
// {CB452A09-CB55-11CF-893E-0800091AC64E}
// IMPLEMENT_OLECREATE(Shutdown, "NNPE.SHUTDOWN.1", 0xcb452a09, 0xcb55, 0x11cf, 0x89, 0x3e, 0x8, 0x0, 0x9, 0x1a, 0xc6, 0x4e)
//**************************/

// PE 4.0 guid!
// {7C851530-EC12-11d0-A36B-00805F36BC04}
IMPLEMENT_OLECREATE(Shutdown, "NNPE.SHUTDOWN.1", 0x7c851530, 0xec12, 0x11d0, 0xa3, 0x6b, 0x0, 0x80, 0x5f, 0x36, 0xbc, 0x4);


/////////////////////////////////////////////////////////////////////////////
// Shutdown message handlers

void Shutdown::Initialize(long instaceID) 
{
    RASCONN *Info = NULL, *lpTemp = NULL;
    DWORD code,  count = 0;
    DWORD dSize = sizeof (RASCONN);
    int i;
    char szMessage[256]="";

    // try to get a buffer to receive the connection data
    if(!(Info = (RASCONN *)LocalAlloc(LPTR, dSize)))
    	return;

    // see if there are any open connections
    Info->dwSize = sizeof (RASCONN);
    code = RasEnumConnections (Info, &dSize, &count);

    if(ERROR_BUFFER_TOO_SMALL == code) {

        // free the old buffer
        LocalFree(Info);

        // allocate a new bigger one
        Info = (RASCONN *)LocalAlloc(LPTR, dSize);
        if(!Info)
            return;

        // try to enumerate again
        Info->dwSize = dSize;
        if(RasEnumConnections(Info, &dSize, &count) != 0) {
            LocalFree(Info);
            return;
        }
    }

    // check for no connections
    if(0 == count) {
        LocalFree(Info);
        return;
    }

    // ask user if they want to hang up
    if(IDNO == MessageBox(NULL, "There are open modem connections.  Would you like to close them?", "Dial-Up Networking", MB_YESNO)) {
        LocalFree(Info);
        return;
    }

    // just hang up everything
    for(i = 0; i < (int) count; i++)
        RasHangUp(Info[i].hrasconn);
   
    LocalFree (Info);

}
