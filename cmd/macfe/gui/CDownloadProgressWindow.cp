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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDownloadProgressWindow.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CDownloadProgressWindow.h"
#include "CNSContext.h"
#include "CProgressBar.h"
#include "COffscreenCaption.h"
#include "Netscape_Constants.h"
#include "PascalString.h"

#include "mkgeturl.h"
#include "resgui.h"

#include <PP_Messages.h>

#define MIN_TICKS	(60/4)	// Don't refresh the progress bar more than 4x /sec.

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDownloadProgressWindow::CDownloadProgressWindow(LStream* inStream)
	:	CMediatedWindow(inStream, WindowType_Progress)
	,	CSaveWindowStatus(this)
{
	mContext = NULL;
	mClosing = false;
	mMessageLastTicks = 0;
	mPercentLastTicks = 0;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDownloadProgressWindow::~CDownloadProgressWindow()
{
	SetWindowContext(NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

ResIDT CDownloadProgressWindow::GetStatusResID() const
{
	return WIND_DownloadProgress;
} // client must provide!

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt16 CDownloadProgressWindow::GetValidStatusVersion() const
{
	return 0x0001;
} // CDownloadProgressWindow::GetValidStatusVersion

//-----------------------------------
void CDownloadProgressWindow::AttemptClose()
//-----------------------------------
{
	CSaveWindowStatus::AttemptCloseWindow();
	Inherited::AttemptClose();
}

//-----------------------------------
void CDownloadProgressWindow::DoClose()
//-----------------------------------
{
	CSaveWindowStatus::AttemptCloseWindow();
	Inherited::DoClose();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDownloadProgressWindow::FinishCreateSelf(void)
{
	CMediatedWindow::FinishCreateSelf();
	
	mBar = dynamic_cast<CProgressBar*>(FindPaneByID(PaneID_ProgressBar));
	Assert_(mBar != NULL);
	
	mMessage = dynamic_cast<COffscreenCaption*>(FindPaneByID(PaneID_ProgressMessage));
	Assert_(mMessage != NULL);

	mComment = dynamic_cast<COffscreenCaption*>(FindPaneByID(PaneID_ProgressComment));	
	Assert_(mComment != NULL);

	LControl* theButton = dynamic_cast<LControl*>(FindPaneByID(PaneID_ProgressCancelButton));
	Assert_(theButton != NULL);
	theButton->AddListener(this);
	CSaveWindowStatus::FinishCreateWindow();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDownloadProgressWindow::SetWindowContext(CNSContext* inContext)
{
	if (mContext != NULL)
		mContext->RemoveUser(this);
		
	mContext = inContext;
	
	if (mContext != NULL)
		{
		mContext->AddListener(this);
		mContext->AddUser(this);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CNSContext* CDownloadProgressWindow::GetWindowContext(void)
{
	return mContext;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CDownloadProgressWindow::HandleKeyPress(
	const EventRecord&	inKeyEvent)
{
	Boolean		keyHandled = false;
	LControl	*keyButton = nil;
	
	switch (inKeyEvent.message & charCodeMask)
		{
		case char_Escape:
			if ((inKeyEvent.message & keyCodeMask) == vkey_Escape)
				keyButton =  (LControl*)FindPaneByID(PaneID_ProgressCancelButton);
			break;

		default:
			if (UKeyFilters::IsCmdPeriod(inKeyEvent))
				keyButton =  (LControl*)FindPaneByID(PaneID_ProgressCancelButton);
			else
				keyHandled = CMediatedWindow::HandleKeyPress(inKeyEvent);
			break;
		}
			
	if (keyButton != nil)
		{
		keyButton->SimulateHotSpotClick(kControlButtonPart);
		keyHandled = true;
		}
	
	return keyHandled;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDownloadProgressWindow::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
{
	if (inCommand == cmd_Stop)
	{
		outEnabled = true;
		return;
	}
	CMediatedWindow::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CDownloadProgressWindow::ObeyCommand(
	CommandT			inCommand,
	void*				/*ioParam*/)
{
	if (inCommand == cmd_Stop)
	{
		LControl *keyButton = (LControl*)FindPaneByID(PaneID_ProgressCancelButton);
		if (keyButton != nil)
			keyButton->SimulateHotSpotClick(kControlButtonPart);
		return true;
	}
	return false;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDownloadProgressWindow::ListenToMessage(
	MessageT			inMessage,
	void*				ioParam)
{
	if (mClosing)
		return;

	switch (inMessage)
		{
		case msg_Cancel:
//			NET_SilentInterruptWindow(*mContext);
//				replaced the line above with the following ones...	
			mClosing = true;
			if (mContext) {
				XP_InterruptContext(*mContext);
				SetWindowContext(NULL);		// calls RemoveUser() and destroys context
			}
			DoClose();
			break;

		case msg_NSCAllConnectionsComplete:
			DoClose();
			break;
			
		case msg_NSCProgressBegin:
			NoteProgressBegin(*(CContextProgress*)ioParam);
			break;
			
		case msg_NSCProgressUpdate:
			NoteProgressUpdate(*(CContextProgress*)ioParam);
			break;
			
		case msg_NSCProgressEnd:
			NoteProgressEnd(*(CContextProgress*)ioParam);
			break;

		case msg_NSCProgressMessageChanged:
//			CContextProgress* theProgress = (CContextProgress*)ioParam;
//			Assert_(theProgress != NULL);
//			mMessage->SetDescriptor(theProgress->mMessage);
			if (::TickCount() - mMessageLastTicks >= MIN_TICKS)
			{
				mMessageLastTicks = ::TickCount();
				mMessage->SetDescriptor((const char*)ioParam);
			}
			break;
			
		case msg_NSCProgressPercentChanged:
			if (::TickCount() - mPercentLastTicks >= MIN_TICKS)
			{
				mPercentLastTicks = ::TickCount();
				mBar->SetValue(*(Int32*)ioParam);
			}
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDownloadProgressWindow::NoteProgressBegin(const CContextProgress& inProgress)
{
	SetDescriptor(CStr255(inProgress.mAction));

	mMessage->SetDescriptor(inProgress.mMessage);
	mComment->SetDescriptor(inProgress.mComment);
	Show();			
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDownloadProgressWindow::NoteProgressUpdate(const CContextProgress& inProgress)
{
	if (inProgress.mPercent != mBar->GetValue())
		mBar->SetValue(inProgress.mPercent);
			
	mMessage->SetDescriptor(inProgress.mMessage);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CDownloadProgressWindow::NoteProgressEnd(const CContextProgress& )
{
	Hide();
}

