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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CThreadMessageController.h

#pragma once

#include "CExpandoDivider.h"
#include "MailNewsCallbacks.h"
#include "CSecurityButton.h"
#include "cstring.h"

class CThreadView;
class CMessageView;
class CMessageAttachmentView;
class CExpandoDivider;
class CSpinningN;
class CBrowserContext;
class CProgressListener;
class CKeyStealingAttachment;

//======================================
class CThreadWindowExpansionData : public CExpansionData
//======================================
{
public:
	virtual void	ReadStatus(LStream* inStream);
	virtual void	WriteStatus(LStream* inStream);
}; // class CThreadWindowExpansionData

//======================================
class CThreadMessageController
	:	public CExpandoListener
	,	public CMailCallbackListener
	,	public LCommander
//	This manages the thread and message panes in the thread window.  In 4.0x, this
//	was all handled by CThreadWindow.  This code moved here on 97/09/30.
//======================================
{
public:
// Construction/Destruction
									CThreadMessageController(
										CExpandoDivider* inExpandoDivider
									,	CThreadView* inThreadView
									,	CMessageView* inMessageView
									,	CMessageAttachmentView* inAttachmentView
									#if !ONECONTEXTPERWINDOW
									,	LListener**	inMessageContextListeners
											// Null-terminated list of pointers, owned by
											// this controller henceforth.
									#endif
									);
	virtual								~CThreadMessageController();
	
	void							ReadStatus(LStream *inStatusData);
	void							WriteStatus(LStream *inStatusData);
	void							InitializeDimensions();
	void							FinishCreateSelf();
	void							InstallMessagePane(
										Boolean inInstall);
	LWindow*						FindOwningWindow() const;

// Accessors/Mutators
	CMessageView*					GetMessageView() const { return mMessageView; }
	cstring							GetCurrentURL() const;
	void							SetStoreStatusEnabled(Boolean inEnabled)
										{ mStoreStatusEnabled = inEnabled; }
	void							SetThreadView(CThreadView* inThreadView)
										{ mThreadView = inThreadView; }							

// CMailCallbackListener overrides:
protected:
	virtual void					ChangeStarting(
										MSG_Pane*		inPane,
										MSG_NOTIFY_CODE	inChangeCode,
										TableIndexT		inStartRow,
										SInt32			inRowCount);
	virtual void					ChangeFinished(
										MSG_Pane*		inPane,
										MSG_NOTIFY_CODE	inChangeCode,
										TableIndexT		inStartRow,
										SInt32			inRowCount);
	virtual void					PaneChanged(
										MSG_Pane*		inPane,
										MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
										int32 value);

// LCommander Overrides
protected:
	virtual void					FindCommandStatus(
										CommandT	inCommand,
										Boolean		&outEnabled,
										Boolean		&outUsesMark,
										Char16		&outMark,
										Str255		outName);
	virtual Boolean					ObeyCommand(
										CommandT	inCommand,
										void		*ioParam);
// command helpers
protected:
	Boolean							CommandDelegatesToMessagePane(CommandT inCommand);

// CExpandoListener overrides:
protected:
	virtual void					ListenToMessage(MessageT inMessage, void *ioParam);

// CExpandable overrides:
protected:
	virtual void					SetExpandState(ExpandStateT inExpanded);
	virtual void					StoreDimensions(CExpansionData&);
	virtual void					RecallDimensions(const CExpansionData&);

// Data
protected:
	CExpandoDivider*				mExpandoDivider;
	CThreadView*					mThreadView;
	CMessageView*					mMessageView;
	CMessageAttachmentView*			mAttachmentView;
#if !ONECONTEXTPERWINDOW
	LListener**						mMessageContextListeners;
#endif
	CThreadWindowExpansionData		mClosedData, mOpenData;
	Boolean							mStoreStatusEnabled;
	CBrowserContext*				mMessageContext;
	CMailSecurityListener			mSecurityListener;
	CKeyStealingAttachment*			mThreadKeyStealingAttachment; // installed when window expanded.
	Str255							mThreadStatisticsFormat; // format string
}; // class CThreadMessageController
