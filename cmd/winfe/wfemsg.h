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
MSG_Master *WFE_MSGGetMaster();							// In msgfrm.cpp
void WFE_MSGDestroyMaster();							// In msgfrm.cpp

// Pref API
void WFE_MSGInit();										// In mailmisc.cpp
void WFE_MSGShutdown();
BOOL WFE_MSGCheckWizard( CWnd *pParent = NULL );		// In mailfrm.cpp

void WFE_MSGOpenSearch();
void WFE_MSGSearchClose();
void WFE_MSGOpenLDAPSearch();
void WFE_MSGLDAPSearchClose();
void WFE_MSGOpenAB();
void WFE_MSGABClose();

#endif         
