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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMessageWindow.h

#pragma once

#include "CMailNewsWindow.h"

#include <LListener.h>

#include "CSaveWindowStatus.h"
#include "MailNewsCallbacks.h"
#include "CSecurityButton.h"
#include "cstring.h"

const ResIDT cMessageWindowPPobID = 10667;

class CBrowserContext;
class CNSContext;
class CMessageView;
class CMailSelection;
struct SPaneChangeInfo;

//======================================
class CMessageWindow
	: public CMailNewsWindow
	, public CMailCallbackListener
//======================================
{
private:
	typedef CMailNewsWindow Inherited; // trick suggested by the ANSI committee.
public:
	enum { class_ID = 'MsgW', res_ID = cMessageWindowPPobID };

							CMessageWindow(LStream* inStream);
	virtual					~CMessageWindow();
	virtual void			FinishCreateSelf();
	
// MY VERSION
	virtual	void			SetWindowContext(CBrowserContext* inContext);
	virtual CNSContext*		GetWindowContext() const { return (CNSContext*)mContext; }
public:
	CMessageView*			GetMessageView();
	static CMessageWindow*	FindAndShow(MessageKey inMessageKey);
	static void				OpenFromURL(const char* url);
	static void				CloseAll(MessageKey inMessageKey);
	static void				NoteSelectionFiledOrDeleted(const CMailSelection& inSelection);
	virtual void			ActivateSelf(void);
	cstring					GetCurrentURL();
									 
// CMailCallbackListener overrides:
	virtual void			ChangeStarting(
								MSG_Pane*		inPane,
								MSG_NOTIFY_CODE	inChangeCode,
								TableIndexT		inStartRow,
								SInt32			inRowCount);
	virtual void			ChangeFinished(
								MSG_Pane*		inPane,
								MSG_NOTIFY_CODE	inChangeCode,
								TableIndexT		inStartRow,
								SInt32			inRowCount);
	virtual void			PaneChanged(
								MSG_Pane*		inPane,
								MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
								int32 value);
	virtual void			ListenToMessage(MessageT inMessage,void *ioParam);
/*
	virtual	void	SetWindowContext(CBrowserContext* inContext);

	// Info about the parent folder: must be queried, not cached!
	MessageKey		GetMessageKey() const;
	MSG_FolderInfo*	GetFolderInfo() const;
	MSG_ViewIndex	GetViewIndex() const;
	uint16			GetFolderFlags() const;

	void			ShowMessage(MSG_Master* inMsgMaster,
								MSG_FolderInfo* inMsgFolderInfo,
								MessageKey inMessageKey);
	void			ShowSearchMessage(MSG_Master *inMsgMaster,
								  MSG_ResultElement *inResult);
*/

	// I18N stuff
	virtual Int16 				DefaultCSIDForNewWindow(void);
		
// CNetscapeWindow overrides
	virtual CExtraFlavorAdder*	CreateExtraFlavorAdder() const;

	//-----------------------------------
	// Window Saving - overrides for CSaveWindowStatus.
	//-----------------------------------
protected:
	virtual ResIDT	GetStatusResID(void) const { return res_ID; }
	virtual UInt16	GetValidStatusVersion(void) const { return 0x0113; }
	virtual void	AdaptToolbarToMessage(void);

protected:
	CBrowserContext*		mContext;
	CMailSecurityListener	mSecurityListener;
};

