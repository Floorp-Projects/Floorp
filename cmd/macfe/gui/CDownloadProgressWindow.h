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

#pragma once

#include "CWindowMediator.h"
#include "CNSContext.h"
#include "CSaveWindowStatus.h"
#include <LListener.h>

class CProgressBar;
class COffscreenCaption;

const ResIDT	WIND_DownloadProgress		=		1011;
const PaneIDT 	PaneID_ProgressCancelButton =		'cncl';
const PaneIDT	PaneID_ProgressMessage		=		'pgms';
const PaneIDT	PaneID_ProgressComment		=		'pgcm';
const PaneIDT	PaneID_ProgressBar			=		'pgbr';

class CDownloadProgressWindow : 
			public CMediatedWindow,
			public LListener,
			public CSaveWindowStatus
{
	private:
		typedef CMediatedWindow Inherited;
	public:
		enum { class_ID = 'PgWd' };
		
									CDownloadProgressWindow(LStream* inStream);
		virtual						~CDownloadProgressWindow();

		virtual	void				AttemptClose();
		virtual	void				DoClose();
		virtual ResIDT				GetStatusResID() const; 		// client must provide!
		virtual UInt16				GetValidStatusVersion() const; 	// client must provide!
		
		virtual	void				SetWindowContext(CNSContext* inContext);
		virtual	CNSContext*			GetWindowContext(void);
	
		virtual Boolean				HandleKeyPress(
											const EventRecord&	inKeyEvent);

		virtual void				FindCommandStatus(
											CommandT			inCommand,
											Boolean				&outEnabled,
											Boolean				&outUsesMark,
											Char16				&outMark,
											Str255				outName);

		virtual Boolean				ObeyCommand(
											CommandT			inCommand,
											void				*ioParam = nil);

		virtual	void				ListenToMessage(
											MessageT			inMessage,
											void*				ioParam);
	
	protected:
	
		virtual void				FinishCreateSelf(void);

		virtual	void				NoteProgressBegin(const CContextProgress& inProgress);
		virtual	void				NoteProgressUpdate(const CContextProgress& inProgress);
		virtual	void				NoteProgressEnd(const CContextProgress& inProgress);

		CNSContext*					mContext;
		CProgressBar*				mBar;
		COffscreenCaption*			mMessage;
		COffscreenCaption*			mComment;		
		Boolean						mClosing;
		unsigned long				mMessageLastTicks;
		unsigned long				mPercentLastTicks;
};

