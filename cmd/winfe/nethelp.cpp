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
#include "nethelp.h"

/////////////////////////////////////////////////////////////////////////////
// IOleHelp operations

void IOleHelp::HtmlHelp(LPCTSTR pMapFileUrl, LPCTSTR pId, LPCTSTR pSearch)
{
	static BYTE parms[] =
		VTS_BSTR VTS_BSTR VTS_BSTR;
	InvokeHelper(0x1, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 pMapFileUrl, pId, pSearch);
}


// Called from mkhelp.c to get the standard location of the NetHelp folder as a URL
char * FE_GetNetHelpDir()
{
	CString		nethelpDirectory;
	
#ifdef XP_WIN32
	CString	regKey = FEU_GetCurrentRegistry(szLoadString(IDS_NETHELP_REGISTRY));
    CString installDirectory = FEU_GetInstallationDirectory(regKey, szLoadString(IDS_NETHELP_DIRECTORY)) + "\\";
#else ifdef XP_WIN16
	CString installDirectory = FEU_GetInstallationDirectory(szLoadString(IDS_NETSCAPE_REGISTRY), szLoadString(IDS_NETHELP_DIRECTORY)) + "\\";
#endif 

	WFE_ConvertFile2Url(nethelpDirectory, installDirectory);
	
	if (nethelpDirectory.IsEmpty()) {
		return NULL;
	} else {
		return XP_STRDUP(nethelpDirectory);
	}
}

MWContext *FE_GetNetHelpContext()
{
    MWContext *pActiveContext = NULL;

    if(theApp.m_pSlaveCX) {
        pActiveContext = theApp.m_pSlaveCX->GetContext();
    }

    return pActiveContext;
}


/* Left in for compatibility, for the moment */

void NetHelp( const char *topic )
{
    if( topic ) {
        MWContext *pNetHelp = FE_GetNetHelpContext();
        if(pNetHelp) {
            XP_NetHelp( pNetHelp, topic );
        }
    }
}


