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



// CFolderThreadController.cp

#include "CFolderThreadController.h"

#include "CMessageFolderView.h"
#include "CThreadView.h"
#include "MailNewsgroupWindow_Defines.h"
#include "divview.h"
#include "CTargetFramer.h"
#include "CNetscapeWindow.h"

#include "resgui.h" // for cmd_ShowLocationBar

#include "prefapi.h"

const char* Pref_MailShowLocationBarFolders = "mailnews.chrome.show_url_bar.folders";
const char* Pref_MailShowLocationBarNoFolders = "mailnews.chrome.show_url_bar.nofolders";

//----------------------------------------------------------------------------------------
CFolderThreadController::CFolderThreadController(
	LDividedView* inDividedView
,	CNSContext* inThreadContext
,	CMessageFolderView* inFolderView
,	CThreadView* inThreadView
)
//----------------------------------------------------------------------------------------
:	mDividedView(inDividedView)
,	mThreadContext(inThreadContext)
,	mFolderView(inFolderView)
,	mThreadView(inThreadView)
{
	Assert_(mDividedView);
	Assert_(mThreadContext);
	Assert_(mThreadView);
	Assert_(mFolderView);
	inDividedView->SetCollapseByDragging(true, true);
	mDividedView->AddListener(this); // for msg_DividerChangedPosition
	mFolderView->AddListener(this);
	mFolderView->SetRightmostVisibleColumn(1); // hide the count columns
} // CFolderThreadController::CFolderThreadController

//----------------------------------------------------------------------------------------
CFolderThreadController::~CFolderThreadController()
//----------------------------------------------------------------------------------------
{
	// See comment in FinishCreateSelf.  Destroy the folder and thread views explicitly
	// here, so that it's done in the right order.  Because of the tab order requirement,
	// LCommander::~LCommander would otherwise be deleting these in the opposite order
	// to the tab order, namely message/thread/folder.  Boom.
	delete mFolderView;
	mFolderView = nil;
	delete mThreadView;
	mThreadView = nil;
	// The message view remains a subcommander, so will be deleted in the base class
	// destructor.
} // CFolderThreadController::~CFolderThreadController

//----------------------------------------------------------------------------------------
void CFolderThreadController::FinishCreateSelf()
//----------------------------------------------------------------------------------------
{
	// It's critical the order we do this.  These are added to the end of the
	// supercommander's subcommander list, and destroyed in the opposite order.
	// Since we have to destroy in the order folder/thread/message, we would like to add
	// here in the order message/thread/folder.  But unfortunately, the order we add them
	// also affects the tab order, which we would like to be folder/thread/message.  So
	// the order here is for the benefit of the tab order.  See the destructor code above.
	mFolderView->SetSuperCommander(this);
	mThreadView->SetSuperCommander(this);
	CTargetFramer* framer = new CTargetFramer();
	mThreadView->AddAttachment(framer);
	framer = new CTargetFramer();
	mFolderView->AddAttachment(framer);
	SetLatentSub(mFolderView);
	
	mFolderView->SetFancyDoubleClick(true);
} // CFolderThreadController::FinishCreateSelf

//----------------------------------------------------------------------------------------
void CFolderThreadController::ListenToMessage(MessageT inMessage, void *ioParam)
//----------------------------------------------------------------------------------------
{
	switch (inMessage)
	{
		case msg_DividerChangedPosition:
		{
			// Don't take any action during FinishCreate(): assume that the panes
			// will be constructed in the same correct positions that they were saved in.
			if (mDividedView->IsVisible() && (LDividedView*)ioParam == mDividedView)
				NoteDividerChanged();
			break;
		}
		case CStandardFlexTable::msg_SelectionChanged:
		{
			Assert_(ioParam == mFolderView);
			MSG_FolderInfo* info = nil;
			if (mFolderView->GetSelectedRowCount() == 1)
			{
				// See also CMessageFolderView::OpenRow
				info = mFolderView->GetSelectedFolder();
				CMessageFolder folder(info);
				if (folder.IsMailServer()
				|| folder.IsNewsHost()
				|| !folder.CanContainThreads())
				{
					info = nil;
				}
			}
			mThreadView->LoadMessageFolder(mThreadContext, info, false /* delay: don't load now */);
			break;
		}
		default:
			break;
	}
} // CFolderThreadController::ListenToMessage

//----------------------------------------------------------------------------------------
void CFolderThreadController::FindCommandStatus(
	CommandT inCommand,
	Boolean &outEnabled,
	Boolean &outUsesMark,
	Char16 &outMark,
	Str255 outName)
//----------------------------------------------------------------------------------------
{
	switch(inCommand)
	{
		case cmd_ToggleFolderPane:
			outEnabled = (mDividedView != nil);
			outUsesMark = false;
			if (outEnabled && mDividedView->IsFirstPaneCollapsed())
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, SHOW_FOLDERPANE_STRING);
			else
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, HIDE_FOLDERPANE_STRING);
			break;
		default:
			LTabGroup::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
} // CFolderThreadController::FindCommandStatus

//----------------------------------------------------------------------------------------
Boolean CFolderThreadController::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
//----------------------------------------------------------------------------------------
{
	switch (inCommand)
	{
		case cmd_ToggleFolderPane:
			if (mDividedView)
				mDividedView->ToggleFirstPane();
			// force menu items to update show "Show" and "Hide" string changes are reflected
			LCommander::SetUpdateCommandStatus(true);
			return true;
		case cmd_RelocateViewToFolder:
			if (mFolderView)
				mFolderView->SelectFolder((MSG_FolderInfo*)ioParam);
			else
				mThreadView->RelocateViewToFolder((MSG_FolderInfo*)ioParam);
			return true;
		case msg_TabSelect:
			// Subcommanders (thread/folder/message) will kick this upstairs here.
			return true;
	}
	return LTabGroup::ObeyCommand(inCommand, ioParam);
} // CFolderThreadController::ObeyCommand

//----------------------------------------------------------------------------------------
void CFolderThreadController::ReadStatus(LStream *inStatusData)
//----------------------------------------------------------------------------------------
{
	mDividedView->RestorePlace(inStatusData);
} // CFolderThreadController::ReadWindowStatus

//----------------------------------------------------------------------------------------
void CFolderThreadController::WriteStatus(LStream *outStatusData)
//----------------------------------------------------------------------------------------
{
	mDividedView->SavePlace(outStatusData);
} // CFolderThreadController::WriteWindowStatus

//----------------------------------------------------------------------------------------
void CFolderThreadController::NoteDividerChanged()
//----------------------------------------------------------------------------------------
{
	Boolean foldersCollapsed = mDividedView->IsFirstPaneCollapsed();
	const char* prefName = foldersCollapsed ?
		Pref_MailShowLocationBarNoFolders
		: Pref_MailShowLocationBarFolders;
	XP_Bool doShow;
	if (PREF_GetBoolPref(prefName, &doShow) != PREF_NOERROR)
	{
		// If the preference is not yet set, the default is to show iff folders are collapsed
		doShow = foldersCollapsed;
	}
	// These commands will be handled by CMailNewsWindow.  The values will be written out to the
	// prefs file as a side effect of ToggleDragBar, using the virtual method
	// GetLocationBarPrefName() which we have provided.
	if (doShow)
		ObeyCommand(cmd_ShowLocationBar, nil);
	else
		ObeyCommand(cmd_HideLocationBar, nil);
} // CFolderThreadController::NoteDividerChanged

//----------------------------------------------------------------------------------------
const char* CFolderThreadController::GetLocationBarPrefName() const
//----------------------------------------------------------------------------------------
{
	if (!mDividedView)
		return nil;
	if (mDividedView->IsFirstPaneCollapsed())
		return Pref_MailShowLocationBarNoFolders;
	return Pref_MailShowLocationBarFolders;
}

