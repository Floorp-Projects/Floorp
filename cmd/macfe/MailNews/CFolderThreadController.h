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



// CFolderThreadController.h

#include <LTabGroup.h>
#include <LListener.h>

class CMessageFolderView;
class CThreadView;
class CNSContext;
class LDividedView;

//======================================
class CFolderThreadController
	:	public LListener
	,	public LTabGroup
// This class is here to mediate between the folder pane and the thread pane in a 3-pane
// window.  Its function is to allow the thread view class not to know anything about
// the folder view.
//======================================
{
	public:
										CFolderThreadController(
											LDividedView* inDividedView
										,	CNSContext* inThreadContext
										,	CMessageFolderView* inFolderView
										,	CThreadView* inThreadView
										);
		virtual							~CFolderThreadController();
	// LListener overrides:
	protected:
		virtual void					ListenToMessage(MessageT inMessage, void *ioParam);

	// LCommander overrides:
	protected:
		virtual void					FindCommandStatus(
											CommandT inCommand,
											Boolean &outEnabled,
											Boolean &outUsesMark,
											Char16 &outMark,
											Str255 outName);
		virtual Boolean					ObeyCommand(CommandT inCommand, void *ioParam);

	// CSaveWindowStatus helpers:
	public:
		void							ReadStatus(LStream *inStatusData);
		void							WriteStatus(LStream *outStatusData);
		const char*						GetLocationBarPrefName() const;

	// Specials
	public:
		void							FinishCreateSelf();
		void							NoteDividerChanged();

	// Data
	protected:
		LDividedView*					mDividedView;
		CMessageFolderView*				mFolderView;
		CThreadView*					mThreadView;
		CNSContext*						mThreadContext;
}; // class CFolderThreadController
