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
// This is a header file for the confhook.cpp file. 
// Written by: Rich Pizzarro (rhp)
//

#ifndef _CONFHOOK
#define _CONFHOOK


//
// Defines needed for registry key names...
//
#define IDS_CONFAPP_FULLPATHNAME	"FullPathName"
#define IDS_CONFAPP_NOBANNER		"BannerSuppressionFlag"
#define IDS_CONFAPP_MIMEFILE		"MIMEFileFlag"
#define IDS_CONFAPP_DIRECTIP		"DirectIPFlag"
#define IDS_CONFAPP_EMAIL			"EmailCallFlag"
#define IDS_CONFAPP_SERVER			"QueryServerFlag"
#define IDS_CONFAPP_USERNAME		"UserNameFlag"

//
// This routine will launch the conferencing endpoint define to the
// machine.
//
// Arguments:
//      commandLine: This is the command line to pass into the
//                   conferencing application. 
//
//
void
LaunchConfEndpoint(char *commandLine, HWND parentWnd = NULL);

//
// This routine is specific to looking up values in the new ConfApp 
// registry/ini file section.
//
CString 
FEU_GetConfAppProfileString(const CString &queryString);


//
// This function is needed to see if a conferencing endpoint
// is defined on the machine. Return true if there is or false
// if it doesn't exist.
//
BOOL
FEU_IsConfAppAvailable(void);


#endif	// _CONFHOOK
