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



// CThreadMessageController.cp

#include "CThreadMessageController.h"

#include "CMessageView.h"
#include "CMailNewsWindow.h"
#include "CThreadView.h"
#include "CBrowserContext.h"
#include "CProgressListener.h"
#include "UMailSelection.h"

#include "macutil.h"
#include "secnav.h"
#include "prefapi.h"

// Command numbers
#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"

const Int16 kBottomBevelHeight = 0;

#define THREE_PANE 1

#include "CKeyStealingAttachment.h"

#pragma mark -

//-----------------------------------
void CThreadWindowExpansionData::ReadStatus(LStream* inStream)
//-----------------------------------
{
	if (!inStream) return;
//	*inStream >> mHeight >> mExpandoHeight;
} // CThreadWindowExpansionData::ReadStatus

//-----------------------------------
void CThreadWindowExpansionData::WriteStatus(LStream* inStream)
//-----------------------------------
{
	if (!inStream) return;
//	*inStream << mHeight << mExpandoHeight;
} // CThreadWindowExpansionData::WriteStatus

#pragma mark -

//-----------------------------------
CThreadMessageController::CThreadMessageController(
	CExpandoDivider* inExpandoDivider
,	CThreadView* inThreadView
,	CMessageView* inMessageView
,	CMessageAttachmentView* inAttachmentView
#if !ONECONTEXTPERWINDOW
,	LListener**	inMessageContextListeners
		// Null-terminated list of pointers, owned by
		// this controller henceforth.
#endif
)
//-----------------------------------
:	CExpandoListener(&mClosedData, &mOpenData)
,	mExpandoDivider(inExpandoDivider)
,	mThreadView(inThreadView)
,	mMessageView(inMessageView)
,	mAttachmentView(inAttachmentView)
,	mMessageContext(nil)
,	mStoreStatusEnabled(true)
,	mThreadKeyStealingAttachment(nil)
#if !ONECONTEXTPERWINDOW
,	mMessageContextListeners(inMessageContextListeners)
#endif
{
	Assert_(mExpandoDivider);
	Assert_(mThreadView);
	Assert_(mMessageView);
} // CThreadMessageController::CThreadMessageController

//-----------------------------------
CThreadMessageController::~CThreadMessageController()
//-----------------------------------
{
#if !ONECONTEXTPERWINDOW
	delete [] mMessageContextListeners;
#endif
} // CThreadMessageController::~CThreadMessageController

//-----------------------------------
void CThreadMessageController::InitializeDimensions()
//-----------------------------------
{
	// In case this is the first instantiation, set up the defaults
	StoreCurrentDimensions(); // get the closed state from PPOb
	// Set the default open state depending on the initial closed state
	*(CThreadWindowExpansionData*)mStates[open_state]
	 = *(CThreadWindowExpansionData*)mStates[closed_state];
//	ExpandoHeight(open_state) -= kBottomBevelHeight;
} // CThreadMessageController::InitializeDimensions

//-----------------------------------
void CThreadMessageController::FinishCreateSelf()
//-----------------------------------
{
	mMessageView->SetMasterCommander(mThreadView);
	LWindow* win = FindOwningWindow();
	LCaption* statisticsCaption = (LCaption*)win->FindPaneByID('Tstt');
	// Initialize the format string.
	if (statisticsCaption)
	{
		statisticsCaption->GetDescriptor(mThreadStatisticsFormat);
		statisticsCaption->Hide();
	}
} // CThreadMessageController::FinishCreateSelf

//-----------------------------------
void CThreadMessageController::ReadStatus(LStream *inStatusData)
//-----------------------------------
{
	mExpandoDivider->ReadStatus(inStatusData);
	CExpandable::ReadStatus(inStatusData);
} // CThreadMessageController::ReadWindowStatus

//-----------------------------------
void CThreadMessageController::WriteStatus(LStream *inStatusData)
//-----------------------------------
{
	mExpandoDivider->WriteStatus(inStatusData);
	CExpandable::WriteStatus(inStatusData);
} // CThreadMessageController::WriteWindowStatus

//-----------------------------------
LWindow* CThreadMessageController::FindOwningWindow() const
//-----------------------------------
{
	return LWindow::FetchWindowObject(mMessageView->GetMacPort());
}

//-----------------------------------
void CThreadMessageController::SetExpandState(ExpandStateT inExpand)
//-----------------------------------
{
	if (!mExpandoDivider) return;
	if (mStoreStatusEnabled) // don't do this during the reading-in phase
		StoreCurrentDimensions();
	mExpandState = inExpand;
	Rect myRect;
	mExpandoDivider->CalcPortFrameRect(myRect); // relative is fine
	::EraseRect(&myRect);
	LWindow* win = FindOwningWindow();
	win->Refresh();
	if (inExpand)
	{
		// Ordering is important!  If we resize the expando pane
		// when in the closed position, we change the first frame size.  So
		// always do the "recall" step when the expando pane has the "open"
		// bindings.
		mExpandoDivider->SetExpandState(inExpand); // set expando properties
		RecallCurrentDimensions();  // resize myself and the expando pane
	}
	else
	{
		// Ordering is important!
		RecallCurrentDimensions(); // resize myself and the expando pane
		mExpandoDivider->SetExpandState(inExpand);
	}
	InstallMessagePane(inExpand);

	if (inExpand)
	{
		// Since "expanding" means that the threadview gets smaller, it often happens
		// that the user's selection is out of the frame after "expansion".
		mThreadView->ScrollSelectionIntoFrame();
	}
	
	// Set the fancy double-click behavior, which is only needed for the expanded state
	mThreadView->SetFancyDoubleClick(inExpand);
	
	// Set the "select after deletion" behavior of the thread view.  The correct macui
	// behavior is NOT to do this.  But when the thread view is controlling the message
	// view, it makes sense to go along with the "Netscape" behavior, ie the wintel
	// behavior.
	Boolean doSelect = inExpand; // (As I just said)
	// ... but some big customer may want the Macintosh behavior in both cases, so check
	// for the special preference.
	XP_Bool noNextRowSelection;
	if (PREF_GetBoolPref("mail.no_select_after_delete", &noNextRowSelection) == PREF_NOERROR
		&& noNextRowSelection)
		doSelect = false; // no matter what.
	mThreadView->SetSelectAfterDelete(doSelect);
	// The kludge to fix the bottom of the message view has moved to CExpandoDivider.
} // CThreadMessageController::SetExpandState

//-----------------------------------
void CThreadMessageController::StoreDimensions(CExpansionData&)
//-----------------------------------
{
} // CThreadMessageController::StoreDimensions()

//-----------------------------------
void CThreadMessageController::RecallDimensions(const CExpansionData&)
//-----------------------------------
{
} // CThreadMessageController::RecallDimensions()

//-----------------------------------
cstring	CThreadMessageController::GetCurrentURL() const
//-----------------------------------
{
	if (mMessageContext)
		return mMessageContext->GetCurrentURL();
	else
		return cstring("");
}

//-----------------------------------
void CThreadMessageController::InstallMessagePane(
	Boolean inInstall)
//-----------------------------------
{
	// Disable the message view to prevent a crash in layout code,
	// called from CHTMLView::AdjustCursorSelf()
	Assert_(mMessageView);
	mMessageView->Disable();
	OSErr err = noErr;
	CMailNewsWindow* win = dynamic_cast<CMailNewsWindow*>(FindOwningWindow());
	ThrowIfNil_(win);
#ifndef THREE_PANE
	SwitchTarget(nil); // necessary paranoia if closing window.
		// Do before changing the supercommander chain, so that powerplant will correctly
		// take the current chain off duty.
#endif
	if (inInstall)
	{
		Try_
		{
#if ONECONTEXTPERWINDOW
			mMessageContext = dynamic_cast<CBrowserContext*>(win->GetWindowContext());
			ThrowIfNil_(mMessageContext);
			((MWContext*)*mMessageContext)->type = MWContextMailMsg;
#else
			mMessageContext = new CBrowserContext(MWContextMailMsg);
#endif
			StSharer theShareLock(mMessageContext);
			mMessageContext->AddUser(this);
			//mMessageContext->AddListener(this);
			mMessageView->SetContext(mMessageContext);
			// This call links up the model object hierarchy for any potential
			// sub model that gets created within the scope of the html view.
			if (win)
				mMessageView->SetFormElemBaseModel(win); // ? Probably wrong.
			mThreadView->AddListener((CExpandoListener*)this); // for msg_SelectionChanged
			// When the message view is installed, we still want the thread view to
			// have first crack at all the commands, and then to pass to message view if
			// not handled.  So we insert the message view between us and the thread view,
			// and set the target to the thread view.
#ifndef THREE_PANE
			mThreadView->SetSuperCommander(mMessageView);
			mMessageView->SetSuperCommander(this);
#endif
			CNSContext* context = mThreadView->GetContext();
			if (context)
				mMessageView->SetDefaultCSID(context->GetDefaultCSID());
			// Add the attachment that will divert certain key events to the message pane.
			// This is added first so that it is executed first, and can disable normal
			// response to keypresses.
			mThreadKeyStealingAttachment = new CKeyStealingAttachment(mMessageView);
			mThreadView->AddAttachmentFirst(mThreadKeyStealingAttachment);
#ifndef THREE_PANE
			mThreadKeyStealingAttachment->StealKey(char_Home);
			mThreadKeyStealingAttachment->StealKey(char_End);
			mThreadKeyStealingAttachment->StealKey(char_PageUp);
			mThreadKeyStealingAttachment->StealKey(char_PageDown);
#endif // THREE_PANE
			mThreadKeyStealingAttachment->StealKey(' ');

			// Tell message view about its attachment pane and hide it
			if (mAttachmentView)
			{
				mMessageView->SetAttachmentView(mAttachmentView);
				//mAttachmentView->Remove();
			}
			// Tell the thread view its selection changed, so it will broadcast.
			// If a message was selected, this will cause it to display in the message pane.
			mThreadView->SelectionChanged();
			mMessageView->Enable();
#if !ONECONTEXTPERWINDOW
			LListener** nextListener = mMessageContextListeners;
			while (*nextListener)
				mMessageContext->AddListener(*nextListener++);
#endif
			mSecurityListener.SetMessageView( mMessageView );
			mMessageContext->AddListener( &mSecurityListener);
		}
		Catch_(localErr)
		{
			inInstall = false;
		}
		EndCatch_
	}
	if (!inInstall) // called with de-install, or threw above...
	{
		if (mThreadView)
		{
			mThreadView->RemoveListener((CMailCallbackListener*)mMessageView); // for msg_SelectionChanged
			// While the message view is installed, it is inserted in the command chain
			// between this and the message view.
#ifndef THREE_PANE
			mThreadView->SetSuperCommander(this);
#endif
			// If mThreadView is nil, we have deleted it and the attachment will be deleted, too.
			delete mThreadKeyStealingAttachment; //... which will remove itself as an attachment
			mThreadKeyStealingAttachment = nil;
		}
		// If a message is loading when we de-install the pane we want to stop the load
		if (mMessageContext) 
			XP_InterruptContext(*mMessageContext); 
		
		if (mMessageView)
		{
			mMessageView->SetFormElemBaseModel(nil); // ? Probably wrong.
			mMessageView->SetContext(nil);
		}
		if (mMessageContext)
		{
#if !ONECONTEXTPERWINDOW
			LListener** nextListener = mMessageContextListeners;
			while (*nextListener)
				mMessageContext->RemoveListener(*nextListener++);
#endif			
			mSecurityListener.SetMessageView( NULL );
			mMessageContext->RemoveListener( &mSecurityListener);
			USecurityIconHelpers::SetNoMessageLoadedSecurityState( win );
			
			((MWContext*)*mMessageContext)->type = MWContextMail; // see hack in mimemoz.c
			mMessageContext->RemoveUser(this); // and delete.
			mMessageContext = nil;
		}
	}
#ifndef THREE_PANE
	if (mThreadView)
		SwitchTarget(mThreadView);
#endif
} // CThreadMessageController::InstallMessagePane

//-----------------------------------
void CThreadMessageController::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
		case CStandardFlexTable::msg_SelectionChanged:
			if (!mThreadView)
				return;
			LWindow* win = FindOwningWindow();
			USecurityIconHelpers::SetNoMessageLoadedSecurityState(win);
			if (GetExpandState() == open_state)
			{
				if (mMessageView)
				{
					CMailSelection selection;
					mThreadView->GetSelection(selection);
					MSG_FolderInfo* folder = mThreadView->GetOwningFolder();
					if (folder && selection.selectionSize == 1)
					{
						LCommander::SetUpdateCommandStatus(true);
						CMessage message(*selection.GetSelectionList() + 1, selection.xpPane);
							// (add one to convert MSG_ViewIndex to TableIndexT)
						MessageKey id = message.GetMessageKey();
						mMessageView->ShowMessage(
							CMailNewsContext::GetMailMaster(),
							folder,
							id,
							false); // load on next idle.

						if (MSG_GetBacktrackState(mThreadView->GetMessagePane()) == MSG_BacktrackIdle)
							MSG_AddBacktrackMessage(mThreadView->GetMessagePane(), folder, id);
						else
							MSG_SetBacktrackState(mThreadView->GetMessagePane(), MSG_BacktrackIdle);
					}
					else
					{
						// blank out the message view.
						mMessageView->
							ShowMessage(
								CMailNewsContext::GetMailMaster(),
								folder, MSG_MESSAGEKEYNONE, false); // But on next idle! #107826
					}
				}
			}
#if ONECONTEXTPERWINDOW
			else
#endif // with multiple contexts, always do this. If shared, only when message pane not there.
			mThreadView->UpdateHistoryEntry();
			break;
		case CMailCallbackManager::msg_ChangeStarting:
		case CMailCallbackManager::msg_ChangeFinished:
		case CMailCallbackManager::msg_PaneChanged:
			if (mThreadView != nil)
			{
				// Set the pane in order to receive XP messages.
				// Note: SetPane() can't be called in FinishCreateSelf(), instead we 
				// have to wait until we are notified that a folder is first loaded into
				// the mThreadView. Well, we just chose to call it on every notification...
				CMailCallbackListener::SetPane(mThreadView->GetMessagePane());
				if (IsMyPane(ioParam))
				{
					// Check if the notification is about _my_ folder
					SPaneChangeInfo * callbackInfo = reinterpret_cast<SPaneChangeInfo *>(ioParam);
					MSG_FolderInfo * folderInfo = reinterpret_cast<MSG_FolderInfo *>(callbackInfo->value);
					if (mThreadView->GetOwningFolder() == folderInfo)
						CMailCallbackListener::ListenToMessage(inMessage, ioParam);
				}
			}
			break;
		
		default:
			CExpandoListener::ListenToMessage(inMessage, ioParam);
			break;
	}
} // CThreadMessageController::ListenToMessage

//-----------------------------------
void CThreadMessageController::ChangeStarting(
		MSG_Pane*		,
		MSG_NOTIFY_CODE	,
		TableIndexT		,
		SInt32			/*inRowCount*/)
//-----------------------------------
{
}

//-----------------------------------
void CThreadMessageController::ChangeFinished(
		MSG_Pane*		,
		MSG_NOTIFY_CODE	,
		TableIndexT		,
		SInt32			/*inRowCount*/)
//-----------------------------------
{
}

//-----------------------------------
void CThreadMessageController::PaneChanged(
	MSG_Pane*		,
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 			/*value*/)
//-----------------------------------
{
	switch (inNotifyCode)
	{
		case MSG_PaneNotifyMessageLoaded:
		case MSG_PaneNotifyMessageDeleted:
		case MSG_PaneNotifyLastMessageDeleted:
			// nothing to do for now
			break;
		case MSG_PaneNotifyFolderDeleted:
#if !THREE_PANE
			LWindow* win = FindOwningWindow();
			win->AttemptClose();
#endif // THREE_PANE
			return; // duh!
	}
	if (inNotifyCode != MSG_PaneNotifyMessageDeleted)
	{
		// Refresh the statistics on the location bar.
		LWindow* win = FindOwningWindow();
		if (win)
		{
			LCaption* statisticsCaption = (LCaption*)win->FindPaneByID('Tstt');
			if (statisticsCaption && mThreadView)
			{
				CStr255 tempString = mThreadStatisticsFormat;
				CMessageFolder folder(mThreadView->GetOwningFolder());
				SInt32 total = folder.CountMessages(), unread = folder.CountUnseen();
				if (total >= 0 && unread >= 0)
					::StringParamText(tempString, total, unread, 0, 0);
				else
					::GetIndString(tempString, 7099, 8); // " Message counts unavailable"
				statisticsCaption->SetDescriptor(tempString);
				statisticsCaption->Show();
			}
		}
	}
} // CThreadMessageController::PaneChanged

//-----------------------------------
Boolean CThreadMessageController::CommandDelegatesToMessagePane(CommandT inCommand)
//-----------------------------------
{
	Boolean isOneLineSelected = mThreadView && mThreadView->GetSelectedRowCount() == 1;
	if (GetExpandState() == open_state && isOneLineSelected)
		switch (inCommand)
		{
			case cmd_Copy:
			case cmd_Print:
			case cmd_PrintOne:
			case cmd_SecurityInfo:
			case cmd_MarkReadByDate:
			case cmd_Stop:
			case cmd_LoadImages:
			case cmd_Reload:
			case cmd_ViewSource:
			case cmd_DocumentInfo:
			case cmd_WrapLongLines:
			case cmd_Find:
			case cmd_FindAgain:
				return true;
			
			// Plus Commands msglib can tell us about.
			case cmd_Back:
			case cmd_GoForward:
			case cmd_NextMessage:
			case cmd_NextUnreadMessage:
			case cmd_PreviousMessage:
			case cmd_PreviousUnreadMessage:

			case cmd_NextUnreadThread:
			case cmd_NextUnreadGroup:
			case cmd_NextFolder:
			case cmd_FirstFlagged:
			case cmd_PreviousFlagged:
			case cmd_NextFlagged:

			case cmd_GetMoreMessages:
			case cmd_Clear:
			case cmd_ReplyToSender:
			case cmd_PostReply:
			case cmd_PostAndMailReply:
			case cmd_PostNew:
			case cmd_ReplyToAll:
			case cmd_ForwardMessage:
			case cmd_ForwardMessageQuoted:

			case cmd_ShowMicroMessageHeaders:
			case cmd_ShowSomeMessageHeaders:
			case cmd_ShowAllMessageHeaders:
			
			case cmd_AttachmentsAsLinks:
			case cmd_AttachmentsInline:
				return true;
		}
	return false;
} // CThreadMessageController::CommandDelegatesToMessagePane

//-----------------------------------
void CThreadMessageController::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
// The message pane, if it exists, is never the target (the thread pane is).
// Therefore, some commands must be explicitly passed down to the message pane
// by this, the window object.
//-----------------------------------
{
#if 0
	switch (inCommand)
	{
		case cmd_SelectSelection:
			outEnabled = true;
			return;
	}
#endif // 0
	if (CommandDelegatesToMessagePane(inCommand))
	{
		if (mMessageView)
		{
			// Set the super commander of the message view to nil, because otherwise
			// when it calls the inherited method, we'll get called here again, with
			// infinite recursion!
			LCommander* savedCommander = mMessageView->GetSuperCommander();
			mMessageView->SetSuperCommander(nil);
			mMessageView->FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			mMessageView->SetSuperCommander(savedCommander);
		}
		return;
	}
	LCommander::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
} // CThreadMessageController::FindCommandStatus

//-----------------------------------
Boolean CThreadMessageController::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
//-----------------------------------
{
#if 0
	switch (inCommand)
	{
		case cmd_RelocateViewToFolder:
			// Command was not (could not be) handled by the ThreadView,
			// so we leave the window as it is and just update the Location popup.
			UpdateFilePopupCurrentItem();
			return true;

		case cmd_SelectSelection:
			// This command comes from the context menu, as the default in the thread view.
			LControl* twistie = dynamic_cast<LControl*>(FindPaneByID(kTwistieID));
			if (twistie)
				twistie->SetValue(true);
			// and nature will take its course...
			return true;
	}
#endif // 0
	if (CommandDelegatesToMessagePane(inCommand))
	{
		if (mMessageView)
		{
			// Set the super commander of the message view to nil, because otherwise
			// when it calls the inherited method, we'll get called here again, with
			// infinite recursion!
			LCommander* savedCommander = mMessageView->GetSuperCommander();
			mMessageView->SetSuperCommander(nil);
			LCommander* savedMasterCommander = mMessageView->GetMasterCommander();
			mMessageView->SetMasterCommander(nil);
			Boolean result = mMessageView->ObeyCommand(inCommand, ioParam);
			if (result)
			{
				// This doesn't work, because the load does not complete.
				STableCell cell(mMessageView->GetCurMessageViewIndex() + 1,1);
				mThreadView->SetNotifyOnSelectionChange(false);
				mThreadView->UnselectAllCells();
				mThreadView->SetNotifyOnSelectionChange(true);
				mThreadView->SelectCell(cell);
			}
			mMessageView->SetMasterCommander(savedMasterCommander);
			mMessageView->SetSuperCommander(savedCommander);
			return result;
		}
	}
	if ( inCommand == cmd_SecurityInfo )
	{
//		MWContext* context = *GetWindowContext();
		MWContext* context = *mThreadView->GetContext();
		SECNAV_SecurityAdvisor(context, NULL );
		return true;
	}
	return LCommander::ObeyCommand(inCommand, ioParam);
} // CThreadMessageController::ObeyCommand
