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

#ifndef _WFEMSG_H
#define _WFEMSG_H

class C3PaneMailFrame;
class CFolderFrame;
class CMessageFrame;
class MSG_Master;

// Window Open API
C3PaneMailFrame *WFE_MSGOpenInbox( BOOL bGetNew = FALSE);	// In thrdfrm.cpp
CFolderFrame *WFE_MSGOpenFolders();						// In fldrfrm.cpp
CFolderFrame *WFE_MSGOpenNews();						// In fldrfrm.cpp

// Master API
//gets current master.  If there isn't one it creates
// one
MSG_Master *WFE_MSGGetMaster();							// In msgfrm.cpp
//gets current master.  If there isn't one it returns NULL.
MSG_Master* WFE_MSGGetMasterValue();
void WFE_MSGDestroyMaster();							// In msgfrm.cpp

//building the add address book popup					   In addrfrm.cpp
//bCreateMenuItem is TRUE if the add menu items don't already exist.
void WFE_MSGBuildAddAddressBookPopups(HMENU hMenu, int nStartPosition,
									  BOOL bCreateMenuItem, MWContext *pContext);

void WFE_MSGLaunchMigrationUtility(HWND parent, int calledFromAddressBook,
								   char* directory);

// Pref API
void WFE_MSGInit();										// In mailmisc.cpp
void WFE_MSGShutdown();
BOOL WFE_MSGCheckWizard( CWnd *pParent = NULL );		// In mailfrm.cpp

void WFE_MSGOpenSearch();
void WFE_MSGSearchClose();
void WFE_MSGOpenAB();
void WFE_MSGABClose();

void WFE_Synchronize(CWnd *pParent, BOOL bExitAfterSynchronizing);	//in offlndlg.cpp


//in mnprefs.cpp

typedef enum
{
  CHAR_USERNAME,
  BOOL_REMEMBER_PASSWORD,
  BOOL_CHECK_NEW_MAIL,
  INT_CHECK_TIME,
  BOOL_OFFLINE_DOWNLOAD,
  INT_DELETE_MODEL,
  BOOL_IS_SECURE,
  CHAR_PERAONAL_DIR,
  CHAR_PUBLIC_DIR,
  CHAR_OTHER_USER_DIR,
  BOOL_OVERRIDE_NAMESPACES,
  BOOL_EMPTY_TRASH_ON_EXIT,
  BOOL_CLEANUP_INBOX_ON_EXIT

}IMAP_PREF;

BOOL IMAP_PrefIsLocked(const char *pHostName, int nID);
void IMAP_SetCharPref(const char *pHostName, int nID, const char* pValue);
void IMAP_SetIntPref(const char *pHostName, int nID, int32 lValue);
void IMAP_SetBoolPref(const char *pHostName, int nID, XP_Bool bValue); 
void IMAP_GetCharPref(const char *pHostName, int nID, char **hBuffer); 
void IMAP_GetIntPref(const char *pHostName, int nID, int32 *pInt); 	
void IMAP_GetBoolPref(const char *pHostName, int nID, XP_Bool *pBool); 	

#endif         
