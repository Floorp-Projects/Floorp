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
 ////////////////////////////////////////////////////////////////
// progman.h
// helper routines for adding/deleting program items
//
// Revision History
// Date        Author         Reason
// -------------------------------------------------------------
// 01/28/97    xxxxxxxxxxxxxx Definition
////////////////////////////////////////////////////////////////

#ifndef __INC_HELPER_H__
#define __INC_HELPER_H__

#define INI_NETSCAPE_FILE           "nscp.ini"
#define INI_NAVIGATOR_SECTION       "Netscape Navigator"
#define INI_CURRENTVERSION_KEY      "CurrentVersion"
#define INI_INSTALL_DIR_KEY         "Install Directory"
#define INI_NS_CURRVER_DEFLT			"4.0"
#define INI_NS_INSTALLDIR_DEFLT		"c:\\netscape"
#define INI_NS_APPNAME_PREFIX			"Netscape Navigator-"
#define INI_NS_PROGGRPNAME_KEY		"Program Folder"
#define INI_NS_PROGGRPNAME_DEFLT		"Netscape Communicator"

#define DDE_WAIT_TIMER					3								// wait timer in seconds per DDE connections

// ShivaRemote constants..
#define SR_DFLT_INSTALL_PATH			"c:\\Netscape"
#define SR_CONNFILE_EXT					".sr"							// Shiva connection file extension
#define SR_ALL_CONNFILES				"*.sr"						// all Shiva connection files
#define INI_SR_CONNWZ_SECTION			"ConnectW Config"
#define INI_SR_CONFIG_SECTION			"Dial-In Configuration"	// Shiva config section name
#define INI_SR_DIALER_SECTION			"ConnectW Config"			// Shiva INI section name

#define INI_SR_PREFFILE_KEY			"preferred file" 
#define INI_SR_DFLT_PREFFILE			"sremote.ini"
#define INI_SR_FILENAME_KEY			"preferred file"			// Shiva INI section name
#define INI_SR_MODEM_KEY				"Modem"						// Shiva key for storing configured modem
#define INI_SR_PORT_KEY					"Port"						// Shiva key for storing port number
#define INI_SR_BPS_KEY					"BPSRate"					// Shiva key for BPS rate setting
#define INI_SR_DIALSTR_KEY				"DialString"				// Shiva key for modem dial string

#include <tchar.h>

//////////////////////////////////////////////////////////////////
// Sleeps for the specified number of seconds
//////////////////////////////////////////////////////////////////
void Sleep(UINT);

//***************************************************************
//***************************************************************
// Helper Routines dealing paths..
//***************************************************************
//***************************************************************

//////////////////////////////////////////////////////////////////
// Get Shiva install path.
// path name is copied into csFilePath
// return TRUE if successful
//////////////////////////////////////////////////////////////////
BOOL GetShivaInstallPath(TCHAR* csFilePath);

//////////////////////////////////////////////////////////////////
// Get ShivaRemote Configuration file:
// path name is copied into csFilePath
// return TRUE if successful
//////////////////////////////////////////////////////////////////
BOOL GetShivaSRemoteConfigFile(TCHAR* csFilePath);

////////////////////////////////////////////////////////////////
// Returns a Netscape Communicator program group name
////////////////////////////////////////////////////////////////
BOOL GetNetscapeProgramGroupName(TCHAR* lpBuff);

////////////////////////////////////////////////////////////////
// Returns a full path to the install directory for Netscape
////////////////////////////////////////////////////////////////
BOOL GetNetscapeInstallPath(TCHAR* lpBuff);

//********************************************************************************
// Get Shiva SR connection file path name base on an connection (account) name.
// path name is copied into csFilePath
// return TRUE if successful
//********************************************************************************
BOOL GetConnectionFilePath(LPCSTR AccountName, TCHAR* csFilePath, BOOL BIncludePath = TRUE);


//***************************************************************
//***************************************************************
// Helper Routines for editing program group items
//***************************************************************
//***************************************************************

/////////////////////////////////////////////////////////////////
// Sends the given command string to the Program Manager
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL SendShellCommand(DWORD ddeInst, LPCSTR lpszCommand);

/////////////////////////////////////////////////////////////////
// Add an item to a program group
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL AddProgramItem(DWORD  ddeInst,               // DDE Instance
                    LPCSTR lpszItemPath,          // command line arguement                 
                    LPCSTR lpszItemTitle,         // program item title
                    LPCSTR lpszItemIconPath=NULL);// icon path
               
///////////////////////////////////////////////////////////////// 
// Delete an item from a program group (caller should select active
// destination program group prior to calling this function)
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL DeleteProgramItem(DWORD  ddeInst,          // DDE Instance
                       LPCSTR lpszItemTitle);   // program item title

//////////////////////////////////////////////////////////////////
// Set a program group active
//////////////////////////////////////////////////////////////////
BOOL MakeActiveGroup(DWORD ddeInst, const char * lpszFolder);

///////////////////////////////////////////////////////////////////////
// Add a program item to a program group
///////////////////////////////////////////////////////////////////////
BOOL AddProgramGroupItem(LPCSTR lpszProgramGroup,       // program group name
                         LPCSTR lpszItemPath,           // path of item to be added
                         LPCSTR lpszItemTitle,          // program group item title
                         LPCSTR lpszItemIconPath=NULL); // program group item icon path

///////////////////////////////////////////////////////////////// 
// Delete a program item from a program group
// return TRUE if successful
/////////////////////////////////////////////////////////////////
BOOL DeleteProgramGroupItem(LPCSTR lpszProgramGroup,    // program group name
                            LPCSTR lpszItemTitle);      // program group item title

/////////////////////////////////////////////////////////////////
// parse win16 name (filename or program item name) to get rid
// of invalid chars.
// - pName is original filename
// - nMaxNameSize is max size of final parsed name 
// - bFileName indicates whether pName is a filename in that case
//   the function will only parse the filename but not the path and
//   nMaxNameSize indicates max size of filename (pathname not included).
//   i.e. if *pName = "c:\test\test,it.txt" becomes "c:\test\testit.txt"
/////////////////////////////////////////////////////////////////
void ParseWin16BadChar(char *pName, BOOL bFileName = TRUE, int nMaxNameSize = 12);


#endif // __INC_HELPER_H__
