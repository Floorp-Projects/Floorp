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



// CThreadWindow.h

#pragma once
// Mac UI Lib
#include "CMailNewsWindow.h"
#include "msgcom.h"
#include "cstring.h"

class CMessageView;
class CThreadView;
class CMessageFolderView;
class CThreadMessageController;
class CFolderThreadController;

typedef struct MSG_ResultElement MSG_ResultElement;

//======================================
class CThreadWindow
	:	public CMailNewsWindow
// This window can have up to three contexts (folder, thread, message).  The
// one managed by the base class is the THREAD context.
//======================================
{
private:
	typedef CMailNewsWindow Inherited;
public:
	enum { class_ID = 'thWN', res_ID = 10509, res_ID_Alt = 10508 };
	

									CThreadWindow(LStream *inStream);
	virtual							~CThreadWindow();
	virtual void					FinishCreateSelf();

	virtual void					AboutToClose(); // place to put common code from [Attempt|Do]Close()

	static CThreadWindow*			ShowInbox(CommandT inCommand);

	enum { kDontMakeNew = false, kMakeNew = true };
	static CThreadWindow*			FindAndShow(
										const MSG_FolderInfo* inFolderInfo,
										Boolean makeNew = kDontMakeNew,
										CommandT inCommand = cmd_Nothing,
										Boolean forceNewWindow = false);

	static CThreadWindow*			FindOrCreate(
										const MSG_FolderInfo* inFolderInfo,
										CommandT inCommand = cmd_Nothing);

	static CThreadWindow*			OpenFromURL(const char* url);
	cstring							GetCurrentURL() const;									
	static void						CloseAll(const MSG_FolderInfo* inFolderInfo);
	virtual CNSContext*				CreateContext() const;
	virtual void					StopAllContexts();
	virtual Boolean					IsAnyContextBusy();
	virtual CMailFlexTable*			GetActiveTable();
		// Return the currently active table in the window, nil if none
	virtual CMailFlexTable*			GetSearchTable();
	CThreadView*					GetThreadView();
	CMessageFolderView*				GetFolderView();
	CMessageView*					GetMessageView();
		// Return the message view.  Note: there may be no MSG_Pane in it!
	void							SetFolderName(const char* inFolderName, Boolean inIsNewsgroup);

// CSaveWindowStatus Overrides:
	virtual void					ReadWindowStatus(LStream *inStatusData);
									// Overridden to stagger in the default case.
	virtual void					WriteWindowStatus(LStream *inStatusData);
	virtual UInt16					GetValidStatusVersion() const;
	virtual ResIDT					GetStatusResID() const;

	// I18N stuff
	virtual Int16 					DefaultCSIDForNewWindow(void);	
	void							SetDefaultCSID(Int16 default_csid);
	void							ShowMessageKey(MessageKey inKey);

protected:
	
	void							SetStatusResID(ResIDT id) { mStatusResID = id; }

	void							UpdateFilePopupCurrentItem();
	virtual void					ActivateSelf(void);

	virtual void					AdaptToolbarToFolder(void);

// LCommander overrides
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

// CNetscapeWindow overrides
protected:
	virtual URL_Struct*				CreateURLForProxyDrag(char* outTitle = nil);
	virtual CExtraFlavorAdder*		CreateExtraFlavorAdder() const;

// CMailNewsWindow overrides
protected:
	virtual const char*				GetLocationBarPrefName() const;

//DATA:
protected:
	ResIDT							mStatusResID;
	CThreadView*					mThreadView;
	CThreadMessageController*		mThreadMessageController;
	CMessageFolderView*				mFolderView;
	CMailNewsContext*				mFolderContext;
	CFolderThreadController*		mFolderThreadController;
}; // class CThreadWindow

