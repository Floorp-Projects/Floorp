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
// Miscellaneous RDF utility methods
//

#include "URDFUtilities.h"

#include "xp_mem.h"
#include "xp_rgb.h"
#include "ufilemgr.h" // for CFileMgr::GetURLFromFileSpec
#include "uprefd.h" // for CPrefs::GetFolderSpec & globHistoryName
#include "PascalString.h" // for CStr255
#include "macgui.h"

#include "CBrowserWindow.h"
#include "CNSContext.h"


Boolean URDFUtilities::sIsInited = false;


//
// StartupRDF
//
// Initialize RDF if it has not already been initialized. This method can be
// called over and over again without bad side effects because it protects the init
// code with a static variable.
//
void URDFUtilities::StartupRDF()
{
	if (!sIsInited)
	{		
		// Point RDF to user's "Navigator Ä" preference folder and turn the FSSpec into
		// a URL
		FSSpec prefFolder = CPrefs::GetFolderSpec(CPrefs::MainFolder);
		char* url = CFileMgr::GetURLFromFileSpec(prefFolder);
		CStr255 urlStr(url);
		XP_FREE(url);
		
		// fill in RDF params structure with appropriate file paths
		CStr255 str;
		RDF_InitParamsStruct initParams;
		initParams.profileURL = (char*)urlStr;
		::GetIndString(str, 300, bookmarkFile);
		initParams.bookmarksURL = (char*)(urlStr + "/" + str);
		::GetIndString(str, 300, globHistoryName);
		initParams.globalHistoryURL = (char*)(urlStr + "/" + str);
		
//		url = NET_UnEscape(url);
	    RDF_Init(&initParams);
	    sIsInited = true;
	}
}


//
// ShutdownRDF
//
// Opposite of above routine. Again, can be called multiple times w/out bad side
// effects.
//
void URDFUtilities::ShutdownRDF()
{
	if (sIsInited)
	{
	    RDF_Shutdown();
	    sIsInited = false;
	}
}

Uint32 URDFUtilities::GetContainerSize(HT_Resource container)
{
	Uint32 result = 0;
	// trust but verify...
	if (HT_IsContainer(container))
	{
		HT_Cursor cursor = HT_NewCursor(container);
		if (cursor)
		{
			HT_Resource node = HT_GetNextItem(cursor);
			while (node != NULL)
			{
				++result;
				if (HT_IsContainer(node) && HT_IsContainerOpen(node))
					result += GetContainerSize(node);
				
				node = HT_GetNextItem(cursor);
			}
		}
	}
	return result;
}


//
// SetupBackgroundColor
//
// Given a HT token for a particular color and an AppearanceManager theme brush, set the 
// background color to the appropriate color (using HT color first, and AM color as a fallback).
//
bool
URDFUtilities :: SetupBackgroundColor ( HT_Resource inNode, void* inHTToken, ThemeBrush inBrush )
{
	RGBColor theBackground;
	bool usingHTColor = GetColor(inNode, inHTToken, &theBackground);
	if ( usingHTColor )
		::RGBBackColor ( &theBackground );
	else
		::SetThemeBackground ( inBrush, 8, false );		

	return usingHTColor;
	
} // SetupBackgroundColor


//
// SetupForegroundColor
//
// Given a HT token for a particular color and an AppearanceManager theme brush, set the 
// foreground color to the appropriate color (using HT color first, and AM color as a fallback).
//
bool
URDFUtilities :: SetupForegroundColor ( HT_Resource inNode, void* inHTToken, ThemeBrush inBrush )
{
	RGBColor theForeground;
	bool usingHTColor = GetColor(inNode, inHTToken, &theForeground);
	if ( usingHTColor )
		::RGBForeColor ( &theForeground );
	else
		::SetThemePen ( inBrush, 8, false );		

	return usingHTColor;

} // SetupForegroundColor


//
// SetupForegroundTextColor
//
// Given a HT token for a particular color and an AppearanceManager theme brush, set the 
// foreground color to the appropriate color (using HT color first, and AM color as a fallback).
//
bool
URDFUtilities :: SetupForegroundTextColor ( HT_Resource inNode, void* inHTToken, ThemeTextColor inBrush )
{
	RGBColor theForeground;
	bool usingHTColor = GetColor(inNode, inHTToken, &theForeground);
	if ( usingHTColor )
		::RGBForeColor ( &theForeground );
	else
		::SetThemeTextColor ( inBrush, 8, false );		

	return usingHTColor;

} // SetupForegroundTextColor


//
// GetColor
//
// Given a node and a property (that should be a color name), returns if this node has
// that property set and if so, make an RGBColor out of it. |outColor| is not modified
// unless this function returns true.
//
bool
URDFUtilities :: GetColor ( HT_Resource inNode, void* inHTToken, RGBColor* outColor )
{
	Assert_(inNode != NULL);
	Assert_(inHTToken != NULL);
	
	bool usingHTColor = false;

	if ( inNode && inHTToken ) {
		char* color = NULL;
		PRBool success = HT_GetNodeData ( inNode, inHTToken, HT_COLUMN_STRING, &color );
		if ( success && color ) {
			uint8 red, green, blue;
			if ( XP_ColorNameToRGB(color, &red, &green, &blue) == 0 ) {
				*outColor = UGraphics::MakeRGBColor(red, green, blue);\
				usingHTColor = true;
			}	
		}
	}
		
	return usingHTColor;

} // GetColor


//
// PropertyValueBool
//
// Gets the value of the given token and determines if it is "[Y|y]es" or "[N|n]o" based
// on the first character. Returns the appropriate boolean value (yes = true).
//
bool
URDFUtilities :: PropertyValueBool ( HT_Resource inNode, void* inHTToken )
{
	char* value = NULL;
	
	PRBool success = HT_GetNodeData ( inNode, inHTToken, HT_COLUMN_STRING, &value );
	if ( success && value )
		return (*value == 'y' || *value == 'Y') ? true : false;

	return false;
	
} // PropertyValueBool

//
// LaunchNode
// LaunchURL
//
// Passes the node/url to HT which will try to handle loading it
//

bool
URDFUtilities :: LaunchNode ( HT_Resource inNode )
{
	MWContext* topContext = FindBrowserContext();	
	return HT_Launch ( inNode, topContext );

} // LaunchNode

bool
URDFUtilities :: LaunchURL ( const char* inURL, CBrowserWindow* inBrowser )
{
	MWContext* currentContext = NULL;
	HT_Pane paneToDisplayThings = NULL;
	
	// if no browser is passed in, try to find the topmost one
	if ( !inBrowser ) {
		CWindowMediator* theMediator = CWindowMediator::GetWindowMediator();
		inBrowser = dynamic_cast<CBrowserWindow*>(theMediator->FetchTopWindow(WindowType_Browser, regularLayerType, false));
	}
	
	// Even though we looked, there may not be a browser open. If we have one, extract
	// the relevants bits from it.
	if ( inBrowser ) {
		paneToDisplayThings = inBrowser->HTPane();
		currentContext = *(inBrowser->GetWindowContext());
	}
	
	return HT_LaunchURL ( paneToDisplayThings, const_cast<char*>(inURL), currentContext ) ;

} // LaunchURL


//
// FindBrowserContext
//
// Tries to find a top-level context that is a browser window. If one cannot be found, 
// returns NULL
//
MWContext*
URDFUtilities :: FindBrowserContext ( )
{
	MWContext* currentContext = NULL;
	CWindowMediator* theMediator = CWindowMediator::GetWindowMediator();
	CNetscapeWindow* theTopWindow =
			dynamic_cast<CNetscapeWindow*>(theMediator->FetchTopWindow(WindowType_Browser, regularLayerType, false));
	if (theTopWindow)
		return *(theTopWindow->GetWindowContext());

	return NULL;

} // FindBrowserContext


//
// StHTEventMasking
//
// Change HT's event masking in the given pane for the lifetime of this class. This is
// meant to be used as a stack-based class. The new notification mask is set in the contsructor
// and is reset to the original mask in the destructor.
// 

URDFUtilities::StHTEventMasking :: StHTEventMasking ( HT_Pane inPane, HT_NotificationMask inNewMask )
	: mPane(inPane)
{
	HT_GetNotificationMask ( mPane, &mOldMask );
	HT_SetNotificationMask ( mPane, inNewMask );
}


URDFUtilities::StHTEventMasking :: ~StHTEventMasking ( ) 
{
	HT_SetNotificationMask ( mPane, mOldMask );
}
