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



// CMessageSearchWindow.h

#pragma once

#include "CSearchWindowBase.h"

class LGAPushButton;
class CFolderScopeGAPopup;
class CFolderMoveGAPopup;

//======================================
class CMessageSearchWindow : public CSearchWindowBase
//======================================
{
private:
	typedef CSearchWindowBase Inherited;				  
public:

	enum { class_ID = 'SchW', pane_ID = class_ID, res_ID = 8600 };
		
	enum { 	 
			 paneID_GoToFolder = 'GOTO'
			,paneID_FilePopup = 'FILm'
			,paneID_MsgScopePopup = 'SCOP'
		 };
		 
	enum { 
			 msg_FilePopup = 'FILm'
		   	,msg_GoToFolder = 'GOTO'
			,msg_MsgScope = 'ScMs'
		 };

						CMessageSearchWindow(LStream *inStream);
	virtual				~CMessageSearchWindow();
	virtual void		SetUpBeforeSelecting();

protected:
	virtual	MSG_ScopeAttribute	GetWindowScope() const;
	
	virtual void		FinishCreateSelf();
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual ResIDT		GetStatusResID() const { return res_ID; }
	virtual void		MessageWindStop(Boolean inUserAborted);
	virtual void		MessageWindSearch();
	virtual void		UpdateTableStatusDisplay();

private:
	void					EnableSelectionControls();
	void					DisableSelectionControls();

	CFolderScopeGAPopup		*mMsgScopePopup;
	LGAPushButton			*mGoToFolderBtn;
	CFolderMoveGAPopup		*mFileMessagePopup;
	MSG_FolderInfo 			*scopeInfo;
	
}; // class CMessageSearchWindow
