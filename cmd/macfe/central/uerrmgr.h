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

/******************************************************************************
 *	uerrmgr.h, Mac front end
 *	
 *	Created 6/18/94 by atotic
 *	
 *	Definiton of ErrorManager, a utility class that handles display of error
 *  messages.
 *****************************************************************************/

#pragma once

#include <Types.h>

struct CStr255;

/*----------------------------------------------------------------------------
	class ErrorManager - Utility class, no instances are created
	
	Function:
	Error manager provides utility routines for displaying
	dialog boxes:
	
	PrepareToInteract -- makes sure that Netscape is frontmost application
	
	Implementation:
	
	All plain alerts (simple text + buttons) use standard Toolbox 
	alert routines.
	If we are in the background, notification manager is used
	to notify the user to bring application to front.
	No instances of ErrorManager should be created.
 -----------------------------------------------------------------------------*/
#include "reserr.h"

// STILL UNDER CONSTRUCTION. FUNCTIONS WILL BE DEFINED AS NEEDED.
class ErrorManager	{
public:
	static const CStr255 OSNumToStr(OSErr err);

	// Call this before displaying any dialogs. It makes sure that the
	// application is in the foreground. The routine will not 
	// return until application is in the foreground
	static void PrepareToInteract();
	
	// Just like PrepareToInteract, except that it returns FALSE
	// if application has not been brought to foreground within wait seconds
	static Boolean TryToInteract(long wait);
	
	// Displays a vanilla alert. All strings inside the alert are 
	// supplied by caller
	static void PlainAlert (const CStr255& s1, 
						const char * s2 = NULL, 
						const char * s3 = NULL, 
						const char * s4 = NULL);
						
	// Displays the alert specified by resID
	static void	PlainAlert( short resID );

	// Yes or No box. All strings supplied by the caller
	static Boolean PlainConfirm(const char * s1, 
						const char * s2 = NULL, 
						const char * s3 = NULL, 
						const char * s4 = NULL);
	// Prints a string "message :err"
	static void ErrorNotify(OSErr err, const CStr255& message);
	static OSType sAlertApp;	// Application that handles our alerts
};

extern "C" char * XP_GetString( int resID );
extern "C" char * GetCString( short resID );
CStr255 GetPString( ResIDT id );
void	MoveResourceMapBelowApp();

