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

#include "ipframe.h"
#include "netsdoc.h"
#include "mainfrm.h"
#include "netsvw.h"
#include "template.h"
#ifdef MOZ_MAIL_NEWS
#include "addrfrm.h"
#include "compfrm.h"
#endif
#ifdef EDITOR
#include "edview.h"
#include "edframe.h"
#endif

/////////////////////////////////////////////
//
// Initialize our templates here so that we
// Don't have to include every doc, view, and
// frame header in netscape.cpp
//
// Returns our choice of startup template,
// NULL on failure
//

CDocTemplate *WFE_InitializeTemplates()
{
    // Register the application's document templates.  Document templates
    //	serve as the connection between documents, frame windows and views.
    theApp.m_ViewTmplate = new CNetscapeDocTemplate(IDR_MAINFRAME,
	    RUNTIME_CLASS(CNetscapeDoc),
	    RUNTIME_CLASS(CMainFrame),	   // main SDI frame window
	    RUNTIME_CLASS(CNetscapeView));

#ifdef MOZ_MAIL_NEWS
	theApp.m_AddrTemplate = new CNetscapeAddrTemplate(IDR_ADDRESS_MENU,
	    RUNTIME_CLASS(CNetscapeDoc),
	    RUNTIME_CLASS(CAddrFrame),			// address book window
	    RUNTIME_CLASS(CNetscapeView));

    theApp.m_TextComposeTemplate = new CNetscapeTextComposeTemplate(IDR_COMPOSEPLAIN,
	    RUNTIME_CLASS(CNetscapeDoc),
	    RUNTIME_CLASS(CComposeFrame),
	    RUNTIME_CLASS(CNetscapeEditView));

    theApp.m_ComposeTemplate = new CNetscapeComposeTemplate(IDR_COMPOSEFRAME,
	    RUNTIME_CLASS(CNetscapeDoc),
	    RUNTIME_CLASS(CComposeFrame),
	    RUNTIME_CLASS(CNetscapeEditView));
#endif // MOZ_MAIL_NEWS

#ifdef EDITOR
// CLM - Template for Editor frame and view - both derived from browser
    theApp.m_EditTmplate = new CNetscapeEditTemplate(IDR_EDITFRAME,
	    RUNTIME_CLASS(CNetscapeDoc),
	    RUNTIME_CLASS(CEditFrame),		// Derived from CMainFrame
	    RUNTIME_CLASS(CNetscapeEditView));	// Derived from CNetscapeView
#endif // EDITOR

#ifdef MOZ_MAIL_NEWS
	if (!theApp.m_AddrTemplate) return NULL;
    if (!theApp.m_ComposeTemplate) return NULL;
    if (!theApp.m_TextComposeTemplate) return NULL;
#endif // MOZ_MAIL_NEWS
    if (!theApp.m_ViewTmplate) return NULL;

#ifdef EDITOR
    if (!theApp.m_EditTmplate) return NULL;
#endif

    theApp.AddDocTemplate(theApp.m_ViewTmplate);
#ifdef MOZ_MAIL_NEWS
    theApp.AddDocTemplate(theApp.m_ComposeTemplate);
    theApp.AddDocTemplate(theApp.m_TextComposeTemplate);
    theApp.AddDocTemplate(theApp.m_AddrTemplate);
#endif // MOZ_MAIL_NEWS

#ifdef EDITOR
    theApp.AddDocTemplate(theApp.m_EditTmplate);
#endif

	// Select which template to use at startup
	CDocTemplate *pTemplate;
#ifdef EDITOR
	BOOL bEditFrame = (bIsGold && theApp.m_bCmdEdit);
	if( bEditFrame ){
		pTemplate = (CDocTemplate*)theApp.m_EditTmplate;
	} else 
#endif
    {
		pTemplate = (CDocTemplate*)theApp.m_ViewTmplate;
	}

	//  In Place Container menu and accelerators....
    pTemplate->SetContainerInfo(IDR_NETSCAPETYPE_CNTR_IP);

	//	See srvritem.cpp for this code (delays loading menus until needed).
    //	Register the OLE server portion.
//#ifdef DEBUG
	//	To avoid assertion in debug mode, let it load from the start.
    pTemplate->SetServerInfo(IDR_SRVR_EMBEDDED, IDR_SRVR_INPLACE,
		    RUNTIME_CLASS(CInPlaceFrame), RUNTIME_CLASS(CNetscapeView));
//#else
//	pTemplate->SetServerInfo(NULL, NULL,
//	RUNTIME_CLASS(CInPlaceFrame), RUNTIME_CLASS(CNetscapeView));
//#endif

	return pTemplate;
}
