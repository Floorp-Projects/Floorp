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
//*****************************************************************************
// nodl.cpp
// 
// Adding a NODL table
//
//
// This module generates the nodl_table containing ALL java native methods.
// This table is used by NSPR when looking up symbols in the DLL.
//
// Since Win16 GetProcAddress() is NOT case-sensitive, it is necessary to
// use the nodl table to maintain the case-sensitive java native method names...
//
//*****************************************************************************


// java include 
#include "netscape_npasw_SetupPlugin.h"
 
//****************************************************************
// adding NODL Table
//****************************************************************

struct PRStaticLinkTableStr {
    char *name;
    void (*fp)(void);
};

typedef struct PRStaticLinkTableStr PRStaticLinkTable;
extern "C" PRStaticLinkTable FAR awt_nodl_tab[];

extern "C" PRStaticLinkTable * CALLBACK __export __loadds NODL_TABLE(void)
{
    return awt_nodl_tab;
}


extern "C" 
{
// file I/O
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetNameValuePair_stub();
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fSetNameValuePair_stub();
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetFolderContents_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetExternalEditor_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fOpenFileWithEditor_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fSaveTextToFile_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fReadFile_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fWriteFile_stub();    

// Modem/RAS
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fOpenModemWizard_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fIsModemWizardOpen_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fCloseModemWizard_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetModemList_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetModemType_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentModemName_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerConfig_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerConnect_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerHangup_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fIsDialerConnected_stub();    

// Desktop/OS
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fNeedReboot_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fReboot_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fDesktopConfig_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fSetKiosk_stub();
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fMilan_stub();
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fCheckEnvironment_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fQuitNavigator_stub();    

// Profile
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileDirectory_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileName_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fSetCurrentProfileName_stub();    

// Misc
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fGetRegInfo_stub();    
extern void Java_netscape_npasw_SetupPlugin_SECURE_0005fEncryptString_stub();    


PRStaticLinkTable FAR awt_nodl_tab[] = 
{
	// file I/O
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetNameValuePair_stub",   Java_netscape_npasw_SetupPlugin_SECURE_0005fGetNameValuePair_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fSetNameValuePair_stub",   Java_netscape_npasw_SetupPlugin_SECURE_0005fSetNameValuePair_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetFolderContents_stub",  Java_netscape_npasw_SetupPlugin_SECURE_0005fGetFolderContents_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetExternalEditor_stub",  Java_netscape_npasw_SetupPlugin_SECURE_0005fGetExternalEditor_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fOpenFileWithEditor_stub", Java_netscape_npasw_SetupPlugin_SECURE_0005fOpenFileWithEditor_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fSaveTextToFile_stub",     Java_netscape_npasw_SetupPlugin_SECURE_0005fSaveTextToFile_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fReadFile_stub",     		Java_netscape_npasw_SetupPlugin_SECURE_0005fReadFile_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fWriteFile_stub",     		Java_netscape_npasw_SetupPlugin_SECURE_0005fWriteFile_stub } ,
   
   // Modem/RAS
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fOpenModemWizard_stub",			Java_netscape_npasw_SetupPlugin_SECURE_0005fOpenModemWizard_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fIsModemWizardOpen_stub",		Java_netscape_npasw_SetupPlugin_SECURE_0005fIsModemWizardOpen_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fCloseModemWizard_stub",		Java_netscape_npasw_SetupPlugin_SECURE_0005fCloseModemWizard_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetModemList_stub",				Java_netscape_npasw_SetupPlugin_SECURE_0005fGetModemList_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetModemType_stub",      		Java_netscape_npasw_SetupPlugin_SECURE_0005fGetModemType_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentModemName_stub",	Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentModemName_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerConfig_stub",				Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerConfig_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerConnect_stub",			Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerConnect_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerHangup_stub",				Java_netscape_npasw_SetupPlugin_SECURE_0005fDialerHangup_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fIsDialerConnected_stub",		Java_netscape_npasw_SetupPlugin_SECURE_0005fIsDialerConnected_stub } ,

	// Desktop/OS
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fNeedReboot_stub",    		Java_netscape_npasw_SetupPlugin_SECURE_0005fNeedReboot_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fReboot_stub",	       		Java_netscape_npasw_SetupPlugin_SECURE_0005fReboot_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fDesktopConfig_stub", 		Java_netscape_npasw_SetupPlugin_SECURE_0005fDesktopConfig_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fSetKiosk_stub",      		Java_netscape_npasw_SetupPlugin_SECURE_0005fSetKiosk_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fMilan_stub",      	 		Java_netscape_npasw_SetupPlugin_SECURE_0005fMilan_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fCheckEnvironment_stub",	Java_netscape_npasw_SetupPlugin_SECURE_0005fCheckEnvironment_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fQuitNavigator_stub", 		Java_netscape_npasw_SetupPlugin_SECURE_0005fQuitNavigator_stub } ,

	// Profile
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileDirectory_stub", Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileDirectory_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileName_stub",	     Java_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileName_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fSetCurrentProfileName_stub",      Java_netscape_npasw_SetupPlugin_SECURE_0005fSetCurrentProfileName_stub } ,
   
   // Miscel
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fGetRegInfo_stub",		Java_netscape_npasw_SetupPlugin_SECURE_0005fGetRegInfo_stub } ,
	{"Java_netscape_npasw_SetupPlugin_SECURE_0005fEncryptString_stub",	Java_netscape_npasw_SetupPlugin_SECURE_0005fEncryptString_stub } ,
};
} // extern "C"               

