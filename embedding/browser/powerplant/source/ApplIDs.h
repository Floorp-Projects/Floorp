/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    wind_DownloadProgress = 1305
};

// Views
enum {
    view_BrowserStatusBar = 130,
    view_BrowserToolBar = 131,
    view_DownloadProgressItem = 1306
};

// Dialogs
enum {
    dlog_FindDialog = 1281
};

// nsIPrompt
enum {
    dlog_Alert = 1282,
    dlog_AlertCheck = 1289,
    dlog_Confirm = 1283,
    dlog_ConfirmCheck = 1284,
    dlog_Prompt = 1285,
    dlog_PromptNameAndPass = 1286,
    dlog_PromptPassword = 1287,
    dlog_ConfirmEx = 1288
};

// Printing
enum {
    dlog_OS9PrintProgress = 1290
};

// Profile Mgmt
enum {
    dlog_ManageProfiles = 1300,
    dlog_NewProfile = 1301
};

// Alerts
enum {
    alrt_ConfirmProfileSwitch   = 1500,
    alrt_ConfirmLogout          = 1501
};

// MENUs
enum {
    menu_Buzzwords              = 1000
};

// Mcmds
enum {
    mcmd_BrowserShellContextMenuCmds = 1200
};

// STR#s

enum {
    STRx_FileLocProviderStrings = 5000,
    
    str_AppRegistryName         = 1,
    str_ProfilesRootDirName,
    str_DefaultsDirName,
    str_PrefDefaultsDirName,
    str_ProfileDefaultsDirName,
    str_ResDirName,
    str_ChromeDirName,
    str_PlugInsDirName,
    str_SearchPlugInsDirName
};

enum {
    STRx_StdButtonTitles        = 5001,
    
    str_Blank                   = 1,
    str_OK,
    str_Cancel,
    str_Yes,
    str_No,
    str_Save,
    str_DontSave,
    str_Revert,
    str_Allow,
    str_DenyAll
};

enum {
    STRx_StdAlertStrings        = 5002,
    
    str_OpeningPopupWindow      = 1,
    str_OpeningPopupWindowExp,  
    str_ConfirmCloseDownloads,
    str_ConfirmCloseDownloadsExp
};

enum {
    STRx_DownloadStatus         = 5003,
    
    str_ProgressFormat          = 1,
    
    str_About5Seconds,
    str_About10Seconds,
    str_LessThan1Minute,
    str_About1Minute,
    str_AboutNMinutes,
    str_About1Hour,
    str_AboutNHours
};

// Icons
enum {
    icon_LockInsecure       = 1320,
    icon_LockSecure,
    icon_LockBroken
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
	cmd_ViewBackgroundImage     = 'VBIm',
	
	cmd_CopyImage               = 'CpIm',

	cmd_CopyLinkLocation        = 'CLnk',
	cmd_CopyImageLocation       = 'CImg',

    cmd_SaveFormData            = 'SFrm',
    cmd_PrefillForm             = 'PFFm',
    
    
    cmd_ManageProfiles          = 'MPrf',
    cmd_Logout                  = 'LOut',
    
    cmd_SaveLinkTarget          = 'DnlL',
    cmd_SaveImage               = 'DlIm'
};

//*****************************************************************************
//***    Apple Events
//*****************************************************************************

enum {
    // A key that we use internally with kAEGetURL
    keyGetURLReferrer           = 'refe'
};

#endif // __ApplIDs__
