/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Browser_Constants.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <PP_Resources.h> // for MBAR_Initial

// MenuBar MBAR ResIDT's
const ResIDT cBrowserMenubar =		MBAR_Initial;
const ResIDT cEditorMenubar =		129;
const ResIDT cMailNewsMenubar =		130;
const ResIDT cAddressBookMenubar =	131;
const ResIDT cComposeMenubar =		132;
const ResIDT cBookmarksMenubar =	133;
const ResIDT cHistoryMenubar =		134;
const ResIDT cMinimalMenubar = 		135;

// Menu ResIDT's
const ResIDT cDirectoryMenuID =		10;

//	WARNING: All other hierarchical menu IDs must be below 75
//	or bad things will happen!

//	Set aside 10 menu IDs for the Editor plugins hierarchical menus.
const ResIDT cEditorPluginsFirstHierMenuID =	75;
const ResIDT cEditorPluginsLastHierMenuID =		84;

//	Set aside 150 menu IDs for the Bookmarks hierarchical menus.
//	(That's 85 to 235 inclusive minus menu ID 128 for the "Apple" menu.)
const ResIDT cBookmarksFirstHierMenuID =		85;
const ResIDT cBookmarksLastHierMenuID =			235;

const ResIDT cHistoryMenuID =		303;
const ResIDT cBookmarksMenuID =		304;
const ResIDT cWindowMenuID =		305;
const ResIDT cToolsMenuID =			405;
const ResIDT cFontMenuID =			13;

// Browser mediated window types
enum {
	WindowType_Browser			=	'Brwz',
	WindowType_Download			=	'Down',
	WindowType_MailNews			=	'MaNw',
	WindowType_MailThread		=	'MaTh',
	WindowType_Message			=	'Mesg',
	WindowType_Newsgroup		=	'News',
	WindowType_NavCenter		=	'NavW',
	WindowType_Address			=	'Addr',
	WindowType_Editor			=	'Edit',
	WindowType_SearchMailNews	=	'Srch',
	WindowType_SearchAddresses	=	'SAdr',
	WindowType_Compose			=	'Comp',
	WindowType_MailFilters		=	'Filt',
	WindowType_Progress			=	'Prog',
	WindowType_AddressPerson 	= 	'Adpr',
	WindowType_AddressList 		= 	'Adls',
	WindowType_SubscribeNews	=	'Subs',
	WindowType_NameCompletion	= 	'NmPk',
	WindowType_OfflinePicker	=	'OfPk'
};

// Browser File Menu cmd id's
const CommandT cmd_OpenURL		=	1032;

// Browser Go Menu cmd id's
const CommandT cmd_GoForward	=	1005;
const CommandT cmd_GoBack		=	1006;
const CommandT cmd_Home			=	1009;

// other cmd id's
const CommandT cmd_PageServices		=	1195;

//
// NavCenter specific
//
const CommandT cmd_NCToggle				= 5101;		// show/hide pane
const CommandT cmd_NCNewWindow		 	= 5100;		// create a new navcenter window

//
// NavCenter XP menu commands. 
//
// These constants can be computed by doing
//		cmd_NavCenterBase + (corresponding HT command)
// but since we need to know the exact command number to put in constructor, I think
// it's clearer to just hardcode the constants in this file.
//
// To get the HT command back from the corresponding command id, just subtract off 
// cmd_NavCenterBase.
//
const CommandT cmd_NavCenterBase		= 5000;

const CommandT cmd_NCOpen				= 5001;
const CommandT cmd_NCOpenFile			= 5002;
const CommandT cmd_NCPrint				= 5003;
const CommandT cmd_NCOpenNewWindow		= 5004;
const CommandT cmd_NCOpenComposer		= 5005;
const CommandT cmd_NCOpenAsWorkspace	= 5006;
const CommandT cmd_NCNewItem			= 5007;
const CommandT cmd_NCNewFolder			= 5008;
const CommandT cmd_NCNewSeparator		= 5009;
const CommandT cmd_NCMakeAlias			= 5010;
const CommandT cmd_NCAddToBookmarks		= 5011;
const CommandT cmd_NCSaveAs				= 5012;
const CommandT cmd_NCCreateShortcut		= 5013;
const CommandT cmd_NCSetToolbarFolder	= 5014;
const CommandT cmd_NCSetBMMenu			= 5015;
const CommandT cmd_NCSetNewBMFolder		= 5016;
const CommandT cmd_NCCut				= 5017;
const CommandT cmd_NCCopy				= 5018;
const CommandT cmd_NCPaste				= 5019;
const CommandT cmd_NCDeleteFile			= 5020;
const CommandT cmd_NCDeleteFolder		= 5021;
const CommandT cmd_NCRevealInFinder		= 5022;
const CommandT cmd_NCGetInfo			= 5023;
const CommandT cmd_NCCopyLinkLocation	= 5024;
const CommandT cmd_NCNewPane			= 5025;
const CommandT cmd_NCRenamePane			= 5026;
const CommandT cmd_NCDeletePane			= 5027;
const CommandT cmd_NCFind				= 5050;		// find in workspace

const CommandT cmd_NavCenterCap			= 5099;


// Clicks
enum EClickKind {
	eWaiting,					// Uninitialized
	eNone,						// No anchors
	eTextAnchor,				// Plain text anchor
	eImageAnchor,				// Image that is a simple anchor
	eImageIsmap,				// Image that is ISMAP
	eImageForm,					// Image that is an ISMAP form
	eImageIcon,					// Image that is an icon (delayed)
	eImageAltText,				// Alt text part of the image
	eEdge						// Edge
};
