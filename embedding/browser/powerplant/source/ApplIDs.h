/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com>
 */

/*
 * Declares application dependent IDs for resources, messages, on so on
 * It is meant to be replaced or added to by any application using the
 * classes in this project. The file name name should not be changed though or
 * source files that include it will have to be changed.
 */
 
#ifndef __ApplIDs__
#define __ApplIDs__
 
//*****************************************************************************
//***    Resource IDs
//*****************************************************************************

// Windows
enum {
    wind_BrowserWindow = 129
};

// Dialogs
enum {
    dlog_FindDialog = 1281
};

// nsIPrompt
enum {
    dlog_Alert = 1282,
    dlog_Confirm = 1283,
    dlog_ConfirmCheck = 1284,
    dlog_Prompt = 1285,
    dlog_PromptNameAndPass = 1286,
    dlog_PromptPassword = 1287,
    dlog_Universal = 1288
};

// Profile Mgmt
enum {
    dlog_ManageProfiles = 1300,
    dlog_NewProfile = 1301
};

// Alerts
enum {
    alrt_ConfirmProfileSwitch = 1500
};

// MENUs
enum {
    menu_Buzzwords              = 1000
};

// Mcmds
enum {
    mcmd_ContextLinkCmds        = 1200,
    mcmd_ContextImageCmds       = 1201,
    mcmd_ContextDocumentCmds    = 1202,
    mcmd_ContextTextCmds        = 1203
};


//*****************************************************************************
//***    Message IDs
//*****************************************************************************

enum {
    msg_OnStartLoadDocument 	= 1000,
    msg_OnEndLoadDocument 	    = 1001
};

//*****************************************************************************
//***    Command IDs
//*****************************************************************************

enum {
    cmd_OpenDirectory           = 'ODir',
    cmd_OpenLinkInNewWindow     = 'OLnN',

	cmd_Back                    = 'Back',
	cmd_Forward                 = 'Forw',
	cmd_Reload                  = 'Rlod',
	cmd_Stop                    = 'Stop',

	cmd_Find                    = 'Find',
	cmd_FindNext                = 'FNxt',

	cmd_ViewPageSource          = 'VSrc',
	cmd_ViewImage               = 'VImg',

	cmd_CopyLinkLocation        = 'CLnk',
	cmd_CopyImageLocation       = 'CImg',

    cmd_SaveFormData            = 'SFrm',
    cmd_PrefillForm             = 'PFFm',
    
    
    cmd_ManageProfiles          = 'MPrf'
};

#endif // __ApplIDs__
