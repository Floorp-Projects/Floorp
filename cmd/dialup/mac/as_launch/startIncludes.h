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

//#include <AddressXlation.h>
#include <AEObjects.h>
#include <AERegistry.h>
#include <Aliases.h>
#include <AppleEvents.h>
//#include <Devices.h>
#include <Dialogs.h>
#include <Errors.h>
#include <Events.h>
#include <Files.h>
#include <Finder.h>
#include <Folders.h>
#include <Fonts.h>
//#include <Gestalt.h>
//#include <LowMem.h>
//#include <MacTCP.h>
#include <Memory.h>
#include <MixedMode.h>
//#include <OSUtils.h>
#include <Processes.h>
#include <TextUtils.h>
//#include <ToolUtils.h>
#include <Resources.h>
//#include <SegLoad.h>
#include <StandardFile.h>
//#include <Strings.h>
#include <TextEdit.h>

#include "prototypes.h"

#ifndef	FALSE
#define	FALSE	0
#define	TRUE	(!FALSE)
#endif

#ifndef	PtoCstr
#define	PtoCstr	P2CStr
#define	CtoPstr	C2PStr
#endif

#define	TIME_TO_PAUSE				(30L)			// WaitNextEvent time
#define	TIME_TO_DIE				(60L*60L)		// how long to wait for Netscape to quit
#define	TIME_TO_STARTUP				(120L*60L)		// how long to wait for Netscape to start

#define START_ACCTSETUP_NAME			"\p:Netscape Account Setup Temp"
#define START_ACCTSETUP_NAME_B3			"\p:Account Setup Manager"
#define START_PROFILE_NAME			"\p:Netscape Profiles Temp"
#define CUSTOMGETFILE_RESID			6042

#define	NETSCAPE_SIGNATURE			'MOSS'
#define	MAGIC_ACCTSETUP_SIGNATURE		'ASWl'
#define	MAGIC_PROFILE_SIGNATURE			'PRFL'
#define	APPLICATION_TYPE			'APPL'

#define	VERS_RESOURCE				'vers'
#define	VERS_RESOURCE_ID			1

#define	TEXT_RESOURCE				'TEXT'
#define	MAC_PREFS_TEXT_RES_ID			3015

#define	MIN_NETSCAPE_VERSION			4

#define	DOCK_UNDOCKED	"pref(\"taskbar.mac.is_open\",					true);"
#define	DOCK_DOCKED	"pref(\"taskbar.mac.is_open\",					false);"

#define	APP_NAME_STR_RESID			128
#define	APP_NAME_STR_ID				1

#define	ERROR_DIALOG_RESID			256
#define	ERROR_STR_RESID				256
#define WHERE_IS_STR_ID				5

#define	NETSCAPE_VERS_TOO_OLD_ERR		1
#define	UNABLE_TO_LOCATE_NETSCAPE_APP_ERR	2
#define	UNABLE_TO_LOCATE_PREFS_FOLDER_ERR	3
#define	UNABLE_TO_LOCATE_ACCOUNT_SETUP_ERR	4
