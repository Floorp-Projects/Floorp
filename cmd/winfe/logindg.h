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

// LoginDlg.h : header file
//

extern char *GetASWURL();
extern int login_QueryForCurrentProfile(); 
extern char * login_QueryNewProfile(BOOL bUpgrade,CWnd *parent);
extern int login_CreateNewUserKey(const char *username,const char* directory);
extern char * login_GetCurrentUser();
extern int	login_UpdateFilesToNewLocation(const char * path,CWnd *pParent,BOOL bCopyDontMove);
extern int login_DeleteUserKey(const char *username);
extern int login_UpdatePreferencesToJavaScript(const char * path);
extern void login_CopyStarterFiles(const char * dst, CWnd *pParent);
extern void login_CreateEmptyProfileDir(const char * dst, CWnd * pParent, BOOL bExistingDir);
extern int login_RenameUserKey(const char *oldName, const char* newName);
extern Bool login_ProfileSelectedCompleteTheLogin(const char * szProfileName, const char * szProfileDir);
extern "C" char * login_GetUserProfileDir();
#ifdef XP_WIN16
extern void login_GetIniFilePath(CString &csNSCPini);
#endif
