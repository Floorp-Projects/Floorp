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

//
// Handle navigation specific tasks in the frame
//

#include "stdafx.h"

#include "mainfrm.h"
#include "prefapi.h"
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Handle and give visual cues to user about forward and back
//

//
// Pull up the Netscape Communications Home Page
//
void CMainFrame::OnNetscapeHome()
{
	if(GetMainContext())	{
		GetMainContext()->NormalGetUrl("http://home.netscape.com/");
	}
}

void CMainFrame::LoadShortcut(int iShortcutID)
{
	char * url=NULL;
	int bOK = PREF_CopyIndexConfigString("toolbar.places.item",iShortcutID, "url",&url);
	if (bOK == PREF_NOERROR) {
		if(GetMainContext())	{
			GetMainContext()->NormalGetUrl(url);
		}
	}
	if (url) XP_FREE(url);
}

// If you click the guide button it will load a default url.
// If you hold it down it will display a menu
void CMainFrame::OnGuide()
{
	char * url=NULL;
	int bOK = PREF_CopyConfigString("toolbar.places.default_url",&url);
	if (bOK == PREF_NOERROR) {
		if(GetMainContext())	{
			GetMainContext()->NormalGetUrl(url);
		}
	}
	if (url) XP_FREE(url);
}

//
// Load a local help file
//
void CMainFrame::OnLocalHelp()
{
	if(GetMainContext())	{
		CString file;
	  
		file = theApp.GetProfileString("Main", "Local Help File", szLoadString(IDS_LOCAL_HELP));
		GetMainContext()->NormalGetUrl(file);
	}
}

//
// Goto hardcoded security page
//                             
void CMainFrame::OnHelpSecurity()
{
	if(GetMainContext())	{
  		GetMainContext()->NormalGetUrl(szLoadString(IDS_HELP_SECURITY));
	}
}

//
// show hardcoded plugins help page
//                             
void CMainFrame::OnAboutPlugins()
{
	if(GetMainContext())	{
  		GetMainContext()->NormalGetUrl(szLoadString(IDS_ABOUT_PLUGINS));
	}
}
