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

#pragma once

//======================================
struct PrefPaneID
// Here are the IDs of the prefs panes.
// These ID's are used for
//		(1) the class ID for the pane's mediator (used for creation of the mediator by URegistrar)
//		(2) the resID of the pane, (used for reanimation of the pane)
//		(3) the command that is stored in the menu that defines the prefs structure.
// For a given prefs pane, the code assumes that these numbers are all the same (and in the range
// 12000-12099.  99 panes should be enough.

//	The ordering is entirely determined from the menu hierarchy.  In particular, unlike 4.0,
//	you no longer have to follow a consecutive number scheme, with min and max ranges.
//======================================
{
	enum ID {
		eNoPaneSpecified							= 0		// used in calls to DoPrefsWindow()
	,	eWindowPPob								= 12000

	,	eAppearance_Main						= 12050
	,	eAppearance_Fonts						= 12051
	,	eAppearance_Colors						= 12052

	,	eBrowser_Main							= 12053
	,	eBrowser_Languages						= 12054
	,	eBrowser_Applications					= 12055

	,	eMailNews_Identity						= 12057
		// Leave this in for Navigator only build to get email addr pref
	,	eMailNews_Main							= 12056
	#ifdef MOZ_MAIL_NEWS
	,	eMailNews_Messages						= 12058
	,	eMailNews_HTMLFormatting				= 12072
	,	eMailNews_Outgoing						= 12073		
	,	eMailNews_MailServer					= 12059
	,	eMailNews_NewsServer					= 12060
	,	eMailNews_Directory						= 12061
	,	eMailNews_Receipts						= 12070
	,	eMailNews_Addressing					= 12071
	#endif // MOZ_MAIL_NEWS
	
	#ifdef EDITOR
	,	eEditor_Main							= 12062
	,	eEditor_Publish							= 12063
	#endif // EDITOR
	#ifdef MOZ_LOC_INDEP
	,	eLocationIndependence					= 12074
	,	eLocationIndependence_Server			= 12075
	,	eLocationIndependence_File				= 12076 //<-еее Current max.  Please move as nec.!
	#endif // MOZ_LOC_INDEP

	#ifdef MOZ_MAIL_NEWS
	,	eOffline_Main							= 12064
	,	eOffline_News							= 12065
	#endif // MOZ_MAIL_NEWS
							
	,	eAdvanced_Main							= 12066
	,	eAdvanced_Cache							= 12067
	,	eAdvanced_Proxies						= 12068
	#ifdef MOZ_MAIL_NEWS
	,	eAdvanced_DiskSpace						= 12069	
	#endif // MOZ_MAIL_NEWS
	};
};
