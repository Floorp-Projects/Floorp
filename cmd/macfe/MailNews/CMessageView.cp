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



// CMessageView.cp

#include "CMessageView.h"

#include "CMailNewsContext.h"
#include "CBrowserContext.h"

#include "resgui.h"
#include "Netscape_Constants.h"
#include "MailNewsgroupWindow_Defines.h"

#include "CNewsSubscriber.h"
#include "UMessageLibrary.h"
#include "UMailFolderMenus.h"
#include "CMailFolderButtonPopup.h"
#include "CMailProgressWindow.h"
#include "CHTMLClickRecord.h"
#include "CHyperScroller.h"
#include "UOffline.h"

#include "msg_srch.h"

#include "uprefd.h"
#include "libi18n.h" // for INTL_CanAutoSelect proto
#include "secnav.h"
#include "CMessageAttachmentView.h"
#include "CApplicationEventAttachment.h"
#include "macutil.h"	// TrySetCursor
#include "uerrmgr.h"	// GetPString prototype
#include "ufilemgr.h"
#include "URobustCreateWindow.h"
#include "CUrlDispatcher.h"
#include "UDeferredTask.h"

#include "shist.h"
#include "prefapi.h"

#include "CURLDispatcher.h"
// The msglib callbacks
// The call backs
extern "C" 
{

	void MacFe_AttachmentCount(MSG_Pane *messagepane, void* closure,
								    int32 numattachments, XP_Bool finishedloading);
	void MacFe_UserWantsToSeeAttachments(MSG_Pane* messagepane, void* closure);
}								    

//----------------------------------------------------------------------------------------
void MacFe_AttachmentCount(MSG_Pane *messagepane, void* /*closure*/,
								    int32 numattachments, XP_Bool finishedloading)
//----------------------------------------------------------------------------------------
{
	MWContext * context = MSG_GetContext( messagepane);
	CMessageView* messageView =dynamic_cast<CMessageView*>( context->fe.newView);
	
	if( messageView )
	{	
		LWindow* window = LWindow::FetchWindowObject(messageView->GetMacPort());
		
		CMessageAttachmentView* attachmentView	=
			dynamic_cast<CMessageAttachmentView*>(window->FindPaneByID('MATv'));
			
			if( attachmentView )
			{
				#if 0 // code automatically shows attachment pane
					  // should be disabled when the attachment button works	
					if( numattachments >0 )
						attachmentView->Show();
				#endif
				if( finishedloading  || attachmentView->IsVisible() )
				{
						attachmentView->SetMessageAttachmentList(messagepane, numattachments);
				}
			}
	}
}

//----------------------------------------------------------------------------------------
void MacFe_UserWantsToSeeAttachments(MSG_Pane* messagepane, void* /*closure*/)
//----------------------------------------------------------------------------------------
{
	MWContext * context = MSG_GetContext( messagepane);
	CMessageView* messageView =dynamic_cast<CMessageView*>( context->fe.newView);
			
	if( messageView )
	{	
		LWindow* window = LWindow::FetchWindowObject(messageView->GetMacPort());
		
		CMessageAttachmentView* attachmentView	=
				 dynamic_cast<CMessageAttachmentView*>(window->FindPaneByID('MATv'));
		if( attachmentView )
				attachmentView->ToggleVisibility();
	}
}

MSG_MessagePaneCallbacks MsgPaneCallBacks = {
	MacFe_AttachmentCount,
	MacFe_UserWantsToSeeAttachments
};

#pragma mark -

//========================================================================================
class CDeferredMessageViewTask
//========================================================================================
:	public CDeferredTask
{
	public:
								CDeferredMessageViewTask(CMessageView* inView);
	protected:
		Boolean					AvailableForDeferredTask() const;
// data
	protected:
		CMessageView*			mMessageView;
		UInt32					mEarliestExecuteTime;
}; // class CDeferredLoadKeyTask

		// It's expensive to load a message, and if the user is nervously clicking
		// on multiple messages we only want to load the last one.
#define LOAD_MESSAGE_DELAY	15		// was GetDblTime()

//----------------------------------------------------------------------------------------
CDeferredMessageViewTask::CDeferredMessageViewTask(CMessageView* inView)
//----------------------------------------------------------------------------------------
:	mMessageView(inView)
,	mEarliestExecuteTime(::TickCount() + LOAD_MESSAGE_DELAY)
{
}

//----------------------------------------------------------------------------------------
Boolean CDeferredMessageViewTask::AvailableForDeferredTask() const
//----------------------------------------------------------------------------------------
{
	// Wait till any pending URLs are finished
	CNSContext* context = mMessageView->GetContext();
	if (!context)
		return false;
	if (XP_IsContextBusy((MWContext*)(*context)))
		return false;
	if (CMailProgressWindow::GetModal())
		return false; // wait until other modal tasks are done.
	// Wait for the earliest execute time.
	if (::TickCount() < mEarliestExecuteTime)
		return false;
	return true;
} // CDeferredMessageViewTask::AvailableForDeferredTask

#pragma mark -

//========================================================================================
class CDeferredLoadKeyTask
//========================================================================================
:	public CDeferredMessageViewTask
{
	public:
								CDeferredLoadKeyTask(
									CMessageView*		inView,
									MSG_FolderInfo*		inFolder,
									MessageKey			inKey);
	protected:
		virtual ExecuteResult	ExecuteSelf();
// data
	protected:
		MSG_FolderInfo*			mFolderToLoadAtIdle;
		MessageKey				mMessageToLoadAtIdle;
}; // class CDeferredLoadKeyTask

//----------------------------------------------------------------------------------------
CDeferredLoadKeyTask::CDeferredLoadKeyTask(
	CMessageView*		inView,
	MSG_FolderInfo*		inFolder,
	MessageKey			inKey)
//----------------------------------------------------------------------------------------
:	CDeferredMessageViewTask(inView)
,	mFolderToLoadAtIdle(inFolder)
,	mMessageToLoadAtIdle(inKey)
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredLoadKeyTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	if (!AvailableForDeferredTask())
		return eWaitStayFront;
	mMessageView->ShowMessage(
		CMailNewsContext::GetMailMaster(),
		mFolderToLoadAtIdle,
		mMessageToLoadAtIdle,
		true);
	return eDoneDelete;
} // CDeferredLoadKeyTask::ExecuteSelf

#pragma mark -

//========================================================================================
class CDeferredLoadURLTask
//========================================================================================
:	public CDeferredMessageViewTask
{
	public:
								CDeferredLoadURLTask(
									CMessageView*		inView,
									const char*			inURLToLoadAtIdle);
		virtual					~CDeferredLoadURLTask();
	protected:
		virtual ExecuteResult	ExecuteSelf();
// data
	protected:
		const char*				mURLToLoadAtIdle;
}; // class CDeferredLoadURLTask

//----------------------------------------------------------------------------------------
CDeferredLoadURLTask::CDeferredLoadURLTask(
	CMessageView*		inView,
	const char*			inURLToLoadAtIdle)
//----------------------------------------------------------------------------------------
:	CDeferredMessageViewTask(inView)
,	mURLToLoadAtIdle(XP_STRDUP(inURLToLoadAtIdle))
{
}

//----------------------------------------------------------------------------------------
CDeferredLoadURLTask::~CDeferredLoadURLTask()
//----------------------------------------------------------------------------------------
{
	XP_FREEIF((char*)mURLToLoadAtIdle);
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredLoadURLTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	if (!AvailableForDeferredTask())
		return eWaitStayFront;
	mMessageView->ShowURLMessage(mURLToLoadAtIdle, true);
	return eDoneDelete;
} // CDeferredLoadURLTask::ExecuteSelf

#pragma mark -

//----------------------------------------------------------------------------
CMessageView::CMessageView(LStream* inStream)
// ---------------------------------------------------------------------------
#ifdef INHERIT_FROM_BROWSERVIEW
:	CBrowserView(inStream)
#else
:	CHTMLView(inStream)
#endif
,	mMessagePane(nil)
,	mMasterCommander(nil)
,	mClosing(false)
,	mAttachmentView(nil)
,	mLoadingNakedURL(false)
, 	mMotionPendingCommand((MSG_MotionType)-1)
,	mDeferredCloseTask(nil)
{
} // CMessageView::CMessageView

// ---------------------------------------------------------------------------
CMessageView::~CMessageView()
// ---------------------------------------------------------------------------
{
	mClosing = true;
		
	if (mMessagePane)
	{
		MSG_SetMessagePaneCallbacks( mMessagePane, NULL, NULL);
		MSG_DestroyPane(mMessagePane);
	}
} // CMessageView::~CMessageView

// ---------------------------------------------------------------------------
void CMessageView::FinishCreateSelf()
// ---------------------------------------------------------------------------
{
	Inherited::FinishCreateSelf();
	SetDefaultScrollMode(LO_SCROLL_YES);	// always display scrollbars
	SetEraseBackground(FALSE);				// don't erase background when browsing mails
}

// ---------------------------------------------------------------------------
Boolean CMessageView::IsDueToCloseLater() const
// ---------------------------------------------------------------------------
{
	return mDeferredCloseTask != nil;
}

// ---------------------------------------------------------------------------
void CMessageView::SetDueToCloseLater()
// ---------------------------------------------------------------------------
{
	mDeferredCloseTask = CDeferredCloseTask::DeferredClose(this);
}


// ---------------------------------------------------------------------------
void CMessageView::ShowMessage(
	MSG_Master* inMsgMaster,
	MSG_FolderInfo* inMsgFolderInfo,
	MessageKey inMessageKey,
	Boolean inLoadNow)
// ---------------------------------------------------------------------------
{
	// There has to be a folder, unless we're clearing the message.
	Assert_(inMsgMaster && (inMsgFolderInfo || inMessageKey == MSG_MESSAGEKEYNONE));
	if (!inLoadNow)
	{
		CDeferredLoadKeyTask* task = new CDeferredLoadKeyTask(this, inMsgFolderInfo, inMessageKey);
		CDeferredTaskManager::Post1(task, this);
		return;
	}
	if (!mMessagePane)
	{
		mMessagePane = ::MSG_CreateMessagePane(*mContext, inMsgMaster);
		ThrowIfNULL_(mMessagePane);
		::MSG_SetFEData(mMessagePane, CMailCallbackManager::Get());
		CMailCallbackListener::SetPane( mMessagePane );
		::MSG_SetMessagePaneCallbacks( mMessagePane, &MsgPaneCallBacks, NULL);
	
	}
	if (inMessageKey != GetCurMessageKey())
	{	
		if( mAttachmentView )
		{
			mAttachmentView->ClearMessageAttachmentView();
			mAttachmentView->Hide();
		}
		// ::SetCursor(*::GetCursor(watchCursor));
		mLoadingNakedURL = false;

		if (::MSG_LoadMessage(mMessagePane, inMsgFolderInfo, inMessageKey) != 0)
		{
			CloseLater();
			throw (OSErr)memFullErr;
		}
		if (inMessageKey == MSG_MESSAGEKEYNONE)
			ClearMessageArea(); // please read comments in that routine.
	}
} // CMessageView::ShowMessage

// ---------------------------------------------------------------------------
void CMessageView::ClearMessageArea()
// MSG_LoadMessage, with MSG_MESSAGEKEYNONE calls msg_ClearMessageArea, which
// has a bug (drawing the background in grey).  So I just changed MSG_LoadMessage() to
// do nothing for XP_MAC except set its m_Key to MSG_MESSAGEKEYNONE and exit.  This transfers
// the responsibility for clearing the message area to the front end.  So far, so good.
//
// Now, it's no good just painting the area, because the next refresh will redraw using the
// existing history entry.  So somehow we have to remove the history entry.
// There's no API for doing this, except calling SHIST_AddDocument with an entry whose
// address string is null or empty.
//
// If this state of affairs changes, this code will break, but I put in asserts to
// notify us about it. 98/01/21
// ---------------------------------------------------------------------------
{
	if ( XP_IsContextBusy((MWContext*)(*mContext)) ) // #107826
		return;
	LO_DiscardDocument(*mContext); // Necessary cleanup.  See also CThreadView::UpdateHistoryEntry().
	// To create a URL_Struct, you have to pass a non-null string for the address, or it returns nil.
	// Next-best thing is an empty string, which works.
	URL_Struct* url = NET_CreateURLStruct("", NET_NORMAL_RELOAD);
	Assert_(url);
	History_entry* newEntry = ::SHIST_CreateHistoryEntry(url, "Nobody Home");
	XP_FREEIF(url);
	Assert_(newEntry);
	// Using an empty address string will cause "AddDocument" to do a removal of the old entry,
	// then delete the new entry, and exit.
	::SHIST_AddDocument(*mContext, newEntry);
	Assert_(!mContext->GetCurrentHistoryEntry()); // Yay!  That was the point of all this.
	Refresh();
	// HTMLView will now clear the background on redraw.
} // CMessageView::ClearMessageArea

// ---------------------------------------------------------------------------
void CMessageView::AdjustCursorSelf(Point inPortPt, const EventRecord& inMacEvent)
// ---------------------------------------------------------------------------
{
	if (GetCurMessageKey() == MSG_MESSAGEKEYNONE)
		::SetCursor(&qd.arrow);
	else if (mContext && XP_IsContextBusy((MWContext*)(*mContext)))
		::SetCursor(*::GetCursor(watchCursor));
	else
		Inherited::AdjustCursorSelf(inPortPt, inMacEvent);
} // CMessageView::AdjustCursorSelf

// ---------------------------------------------------------------------------
void CMessageView::ShowURLMessage(
	const char* inURL,
	Boolean inLoadNow)
// ---------------------------------------------------------------------------
{
	Assert_(inURL);
	if (!inLoadNow)
	{
		CDeferredLoadURLTask* task = new CDeferredLoadURLTask(this, inURL);
		CDeferredTaskManager::Post1(task, this);
		return;
	}
	if (!mMessagePane)
	{
		mMessagePane = MSG_CreateMessagePane(*mContext, CMailNewsContext::GetMailMaster());
		ThrowIfNULL_(mMessagePane);
		MSG_SetFEData(mMessagePane, CMailCallbackManager::Get());
		CMailCallbackListener::SetPane( mMessagePane );
		MSG_SetMessagePaneCallbacks(mMessagePane, &MsgPaneCallBacks, NULL);
	}
	::SetCursor(*::GetCursor(watchCursor));
	SetDefaultCSID(mContext->GetDefaultCSID());
	URL_Struct* urls = NET_CreateURLStruct(inURL, NET_DONT_RELOAD); 
	ThrowIfNULL_(urls);
	urls->msg_pane = mMessagePane;
	mContext->SwitchLoadURL(urls, FO_CACHE_AND_PRESENT);
	mLoadingNakedURL = true; // so we can synthesise FE_PaneChanged on AllConnectionsComplete.
	//FE_PaneChanged(GetMessagePane(), false, MSG_PaneNotifyMessageLoaded, 0);
	
} // CMessageView::ShowURLMessage

// ---------------------------------------------------------------------------
void CMessageView::ShowSearchMessage(
	MSG_Master *inMsgMaster,
	MSG_ResultElement *inResult,
	Boolean inNoFolder)
// ---------------------------------------------------------------------------
{
	Assert_(inMsgMaster);
	if (!mMessagePane)
	{
		mMessagePane = MSG_CreateMessagePane(*mContext, inMsgMaster);
		ThrowIfNULL_(mMessagePane);
		MSG_SetFEData(mMessagePane, CMailCallbackManager::Get());
	}
	mLoadingNakedURL = inNoFolder;
	MSG_SearchError error = MSG_OpenResultElement(inResult, mMessagePane);
	if ( error != SearchError_Success ) {
		if ( (error == SearchError_OutOfMemory) || (error == SearchError_NullPointer) ) {
			FailOSErr_(memFullErr);
		} else {
			FailOSErr_(paramErr);
		}
	}
} // CMessageView::ShowSearchMessage

// ---------------------------------------------------------------------------
void CMessageView::FileMessageToSelectedPopupFolder(const char *ioFolderName, 
													Boolean inMoveMessages)
// File selected messages to the specified BE folder. If inMoveMessages is true, move
// the messages, otherwise copy them.
// ---------------------------------------------------------------------------
{
#ifdef Debug_Signal
	const char *curName = ::MSG_GetFolderNameFromID(GetFolderInfo());
	Assert_(strcasecomp(curName, ioFolderName) != 0);
		// Specified folder should not be the same as this folder
#endif // Debug_Signal

	if ( GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP )
		inMoveMessages = false;

	MSG_ViewIndex index = GetCurMessageViewIndex();
	if ( inMoveMessages ) {
		::MSG_MoveMessagesInto(GetMessagePane(), &index, 1, ioFolderName);
	} else {
		::MSG_CopyMessagesInto(GetMessagePane(), &index, 1, ioFolderName);
	}
} // CThreadWindow::FileMessagesToSelectedPopupFolder

// ---------------------------------------------------------------------------
void CMessageView::YieldToMaster()
// An ugly solution, but after trying many, it's the only one that worked.
// make sure this view isn't target if there's a thread view in the same window.
// ---------------------------------------------------------------------------
{
	if (mMasterCommander)
		SwitchTarget(mMasterCommander);
}

// ---------------------------------------------------------------------------
void CMessageView::ClickSelf(const SMouseDownEvent& where)
// make sure a message view isn't target if there's a thread view.
// 97/10/21.  In Gromit (v5.0), we don't do this any more.
// ---------------------------------------------------------------------------
{	
	Inherited::ClickSelf(where);
//	YieldToMaster();
} // CMessageView::ClickSelf

// ---------------------------------------------------------------------------
void CMessageView::FindCommandStatus(
	CommandT inCommand,
	Boolean& outEnabled,
	Boolean& outUsesMark, 
	Char16& outMark,
	Str255 outName)
// ---------------------------------------------------------------------------
{
	outUsesMark = false;
	outEnabled = false;
	if (mClosing)	// don't respond to BE callbacks when being destroyed
		return;
	// Need to this up here other wise the HTMLView gets a crack at this command
	if( inCommand == cmd_SecurityInfo )
	{
		if( mMessagePane )
			outEnabled = ( GetCurMessageKey() != MSG_MESSAGEKEYNONE );
		return;
	}

	if (mMessagePane && mContext)
	{
		if (XP_IsContextBusy((MWContext*)(*mContext)))
		{
			// Disable everything except the "stop" button
			// (and cmd_About: kludge for BUG#68886)
			//
			// It is of course really bad that the recalculation
			// of command status is being short-circuited like this.
			// TODO: Someone should fix this the right way.
			if (inCommand == cmd_Stop)
			{
				outEnabled = true;
				::GetIndString(outName, STOP_STRING_LIST, STOP_LOADING_INDEX );
			}
			else if (inCommand == cmd_About)
			{
				outEnabled = true;
			}
			return;
		}
		
		
		switch (inCommand)
		{
			// Should always be enabled
			case cmd_SubscribeNewsgroups:
				outEnabled = true;
				return;

			case cmd_ViewSource:
			case cmd_DocumentInfo:
				if (GetCurMessageKey() == MSG_MESSAGEKEYNONE)
					return;
				break;
		
			case cmd_Stop:
			{
				// Note that the context is not busy, but "stop" might mean "stop animations"
				if (mContext->IsContextLooping())
				{
					outEnabled = true;
					::GetIndString(outName, STOP_STRING_LIST, STOP_ANIMATIONS_INDEX );
				}
				return;
			}

		} // switch
		if (inCommand >= ENCODING_BASE && inCommand < ENCODING_CEILING)
		{
			// yeah, I know, this code isn't pretty, but I copied and
			// pasted this from Akbar (v3.0)
			outEnabled = true;
			outUsesMark = true;
			
			Int16 csid = CPrefs::CmdNumToDocCsid( inCommand );
			outMark = (csid == mContext->GetDefaultCSID()) ? checkMark : ' ';
			return;
		} 
		if (mLoadingNakedURL)
		{
			// Do NOT allow any message library commands
			Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, 
										 outMark, outName);
			return;
		}
		if (UMessageLibrary::FindMessageLibraryCommandStatus(
				GetMessagePane(),
				NULL,
				0,
				inCommand,
				outEnabled,
				outUsesMark,
				outMark,
				outName))
			return;
		MSG_MotionType cmd = UMessageLibrary::GetMotionType(inCommand);
		if (UMessageLibrary::IsValidMotion(cmd))
		{
			XP_Bool selectable;
			outEnabled = (
				mMessagePane
				&& MSG_NavigateStatus(mMessagePane, cmd, GetCurMessageViewIndex(), &selectable, NULL) == 0
				&& selectable
				);
			outUsesMark = false;
			return;
		}
		else if ( (inCommand == cmd_MoveMailMessages) || (inCommand == cmd_CopyMailMessages) ||
				 CMailFolderSubmenu::IsMailFolderCommand(&inCommand) )
		{
			// Mail folder commands
			outEnabled = true;
			return;
		}
	}
	Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, 
								 outMark, outName);
} // CMessageView::FindCommandStatus

// ---------------------------------------------------------------------------
Boolean CMessageView::ObeyMotionCommand(MSG_MotionType cmd)
// ---------------------------------------------------------------------------
{
	if (!mMessagePane) return false;
	Assert_(UMessageLibrary::IsValidMotion(cmd));
	try
	{
		MSG_ViewIndex resultIndex;
		MessageKey resultKey;
		MSG_FolderInfo* finfo;
		if (MSG_ViewNavigate(
				mMessagePane,
				cmd,
				GetCurMessageViewIndex(),
				&resultKey,
				&resultIndex,
				NULL,
				&finfo) == 0)
		{
			/*ThrowIfError_(MSG_LoadMessage(
				mMessagePane,
				finfo ? finfo : GetFolderInfo(),
				resultKey));*/
			if (resultKey != MSG_MESSAGEKEYNONE)
			{
				ShowMessage( 
					CMailNewsContext::GetMailMaster(),
					finfo ? finfo : GetFolderInfo(),
					resultKey);
			}
			else if ( finfo != NULL )
			{
				switch( cmd )
				{
					case MSG_NextFolder:
					case MSG_NextMessage:
						 mMotionPendingCommand = MSG_FirstMessage;
						break;
					case MSG_NextUnreadMessage:
					case MSG_NextUnreadThread:
					case MSG_NextUnreadGroup:
					case MSG_LaterMessage:
					case (MSG_MotionType)MSG_ToggleThreadKilled:
						mMotionPendingCommand = MSG_NextUnreadMessage;
						break;
					default:
						break;
				}
				MSG_LoadFolder(	mMessagePane, finfo );
			}
			return true;
		}
	}
	catch (...)
	{
		SysBeep(1);
	}
	return false;
	
} // CMessageView::ObeyMotionCommand

//----------------------------------------------------------------------------------------
void CMessageView::CloseLater()
//----------------------------------------------------------------------------------------
{
	// Oh, I wish:
	//((MWContext*)*mContext)->msgCopyInfo->moveState.nextKeyToLoad = MSG_MESSAGEKEYNONE;
	SetDueToCloseLater(); // delete me next time
	StartIdling();
} // CMessageView::CloseLater

//----------------------------------------------------------------------------------------
Boolean CMessageView::MaybeCloseLater(CommandT inCommand)
// This checks that we are a stand-alone message window, then, according to the preference,
// marks the window for death on the next idle.
//----------------------------------------------------------------------------------------
{
	// If we are not a stand-alone pane, don't do this.  To detect if we're stand-alone, use
	// mMasterCommander, which will be set to the thread view if we're in a multipane view.
	if (mMasterCommander)
		return false;
	const char* prefName = nil;
	if (inCommand == cmd_Clear)
		prefName = "mail.close_message_window.on_delete";
	else if (inCommand == cmd_MoveMailMessages || inCommand == cmd_CopyMailMessages)
		prefName = "mail.close_message_window.on_file";
	else
		return false;
	XP_Bool closeOption;		
	if (PREF_GetBoolPref("mail.close_message_window.on_delete", &closeOption) == PREF_NOERROR
	&& closeOption)
	{
		CloseLater();
		return true;
	}
	return false;
} // CMessageView::MaybeCloseLater

// ---------------------------------------------------------------------------
Boolean CMessageView::ObeyCommand(CommandT inCommand, void *ioParam)
// ---------------------------------------------------------------------------
{
	if (!mContext)
		return false;
	if (mClosing)	// don't respond to BE callbacks when being destroyed
		return false;
	if (inCommand != cmd_Stop && XP_IsContextBusy((MWContext*)(*mContext)))
		return false;
		
	// If you're reading a discussion message, and you want to compose a new message, the new
	// message is, by default, addressed to that discussion group... this test and reassignment
	// are necessary to make this happen
	if ( (inCommand == cmd_NewMailMessage) && (GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP) ) {
			inCommand = cmd_PostNew;
	}

	switch (inCommand)
	{
		case msg_TabSelect:
			return (GetCurMessageKey()); // Allow selection only if a message is loaded.
		case cmd_Stop:
		{
			TrySetCursor(watchCursor);
			XP_InterruptContext(*mContext);
			SetCursor( &qd.arrow );
			return true;
		}

		// Implement this here instead of browser view since for browser windows we want
		// the benefit of AE recording. The AE sending/receiving code is currenty in
		// CBrowserWindow and we don't want the view to know about the window.
		case cmd_Reload:
		{
			if (GetContext())
			{
				CBrowserContext* theTopContext = GetContext()->GetTopContext();

				if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey) ||
					CApplicationEventAttachment::CurrentEventHasModifiers(shiftKey))
				{
					theTopContext->LoadHistoryEntry(CBrowserContext::index_Reload, true);
				}
				else
				{
					theTopContext->LoadHistoryEntry(CBrowserContext::index_Reload);
				}
			}
			
			return true;
		}
		break;
		
		case cmd_SecurityInfo:
		{
			URL_Struct*  url = MSG_ConstructUrlForMessage(GetMessagePane(), GetCurMessageKey());
			SECNAV_SecurityAdvisor((MWContext*)*(GetContext()), url );
			NET_FreeURLStruct(url);
			return true;
		}
		
		case cmd_SubscribeNewsgroups:
		{
			MSG_Host* selectedHost =  MSG_GetHostForFolder(GetFolderInfo());
			CNewsSubscriber::DoSubscribeNewsGroup(selectedHost);
			return true;
		}
		

		#if 0 // CHTMLView handles this
		case cmd_SAVE_LINK_AS:
		{
	
			// Convert cmd_SAVE_LINK_AS to cmd_OPEN_LINK for an attachment link.  
			// This is a temporary fix for the issue that "Save link as" will save
			// mime otherwise.  A better solution would be to have a different command
			// in the context menu (cmd_SAVE_ATTACHMENT_AS).
			CHTMLClickRecord* cr = mCurrentClickRecord; // for brevity.
			const char* url = cr->GetClickURL();
			if (!strncasecomp (url, "mailbox:", 8))
			{
				if (XP_STRSTR(url, "?part=") || XP_STRSTR(url, "&part="))
				{
					// Yep, mail attachment.
					URL_Struct* theURL = NET_CreateURLStruct( url, NET_DONT_RELOAD);
					ThrowIfNULL_(theURL);
					FSSpec locationSpec;
					memset(&locationSpec, 0, sizeof(locationSpec));
					CURLDispatcher::GetURLDispatcher()->DispatchToStorage(
						theURL, locationSpec, FO_SAVE_AS, true);
					return true;
				}
			}
			break;
		
		}
		#endif // 0
	} // switch
	Boolean cmdHandled = false;
	MSG_CommandType cmd = UMessageLibrary::GetMSGCommand(inCommand);
	if (UMessageLibrary::IsValidCommand(cmd))
	{
		 // If there is a the thread view (we're not really supposed to know about it, :-) )
		 // then give it first crack.
		if (mMasterCommander && mMasterCommander->ObeyCommand(inCommand, ioParam))
			return true;
		Boolean enabled; Boolean usesMark; Char16 mark; CStr255 commandName;

		if (! UMessageLibrary::FindMessageLibraryCommandStatus(
					GetMessagePane(),
					(MSG_ViewIndex*)nil,
					0,
					inCommand, enabled, usesMark, mark, commandName)
			|| !enabled)
			return false;
		try
		{
			if (cmd == MSG_GetNewMail || cmd == MSG_GetNextChunkMessages)
			{
				if (NET_IsOffline())
				{
					// Bug #105393.  This fails unhelpfully if the user is offline.  There
					// used to be a test for this here, but for some reason it was
					// removed.  This being so, the newly agreed-upon fix is that, if
					// the user requests new messages while offline, we should instead
					// present the "Go Online" dialog. See also CMailFlexTable.cp.
					//		- 98/02/10 jrm.
					PREF_SetBoolPref("offline.download_discussions", cmd==MSG_GetNextChunkMessages);
					PREF_SetBoolPref("offline.download_mail", cmd==MSG_GetNewMail);
					UOffline::ObeySynchronizeCommand();
				}
				else
				{
					CMailProgressWindow::ObeyMessageLibraryCommand(
						CMailProgressWindow::res_ID_modeless,
						GetMessagePane(),
						cmd,
						commandName);
				}
			}
			else
			{
				MSG_Command(GetMessagePane(), cmd, nil, 0);
				MaybeCloseLater(inCommand);
			}
		}
		catch(...)
		{
		}
		cmdHandled = true;
	}
	if (!cmdHandled)
	{
		MSG_MotionType mcmd = UMessageLibrary::GetMotionType(inCommand);
		if (UMessageLibrary::IsValidMotion(mcmd))
		{
			if (mMasterCommander) // give these to the thread view (for select after delete)
				return mMasterCommander->ObeyCommand(inCommand, ioParam);
			cmdHandled = ObeyMotionCommand(mcmd);
		}
		
	}
	if (!cmdHandled)
	{
		// Mail folder commands.  We come here either from a button (broadcast) or
		// from a menu command (synthetic).
		const char* folderPath = nil;
		if (inCommand == cmd_MoveMailMessages || inCommand == cmd_CopyMailMessages)
		{
			// Button case
			if ( ioParam )
				folderPath = ::MSG_GetFolderNameFromID((MSG_FolderInfo*)ioParam);
		}
		else if (!CMailFolderSubmenu::IsMailFolderCommand(&inCommand, &folderPath)) // menu case
		{
			folderPath = nil; // any other case.
		}
		if (folderPath && *folderPath)
		{
			if (mMasterCommander) // give these to the thread view (for select after delete)
				return mMasterCommander->ObeyCommand(inCommand, ioParam);
			FileMessageToSelectedPopupFolder(
				folderPath, 
				inCommand == cmd_MoveMailMessages);
			MaybeCloseLater(inCommand);
			return true;
		}
	}

	if (!cmdHandled)
	{
		switch ( inCommand )
		{
			case cmd_SaveDefaultCharset:
			{
				Int32 default_csid = mContext->GetDefaultCSID();
				CPrefs::SetLong(default_csid, CPrefs::DefaultCharSetID);
				cmdHandled = true;
			}
			break;
			
			default:
			{
				if ( inCommand > ENCODING_BASE && inCommand < ENCODING_CEILING )
				{
					SetDefaultCSID(CPrefs::CmdNumToDocCsid(inCommand));
					cmdHandled = true;
				}
			}
		}
	}
	if (!cmdHandled)
		cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
	return cmdHandled;
} // CMessageView::ObeyCommand

// ---------------------------------------------------------------------------
void CMessageView::ListenToMessage(MessageT inMessage, void* ioParam)
// ---------------------------------------------------------------------------
{
	switch(inMessage)
	{
		case msg_NSCAllConnectionsComplete:
			SetCursor(&UQDGlobals::GetQDGlobals()->arrow);
			if (mLoadingNakedURL)
				FE_PaneChanged(GetMessagePane(), false, MSG_PaneNotifyMessageLoaded, 0);
			if( mMotionPendingCommand != (MSG_MotionType)-1 )
			{
				ObeyMotionCommand( mMotionPendingCommand );
				mMotionPendingCommand = (MSG_MotionType)-1;
			}
			// Need to enable the menus since we may just have loaded a new message using imap
			SetUpdateCommandStatus( true );
			break;
		
		/*
		case cmd_SecurityInfo:
		case cmd_Reload:
		case cmd_Stop:
			ObeyCommand(inMessage, ioParam);
			return;
		*/
		
		default:
			if (!IsOnDuty() || !ObeyCommand(inMessage, ioParam))
				Inherited::ListenToMessage(inMessage, ioParam);
	} // switch
	
	CMailCallbackListener::ListenToMessage(inMessage, ioParam);
} // CMessageView::ListenToMessage

// ---------------------------------------------------------------------------
Boolean CMessageView::HandleKeyPress(const EventRecord& inKeyEvent)
// ---------------------------------------------------------------------------
{
	if (inKeyEvent.what == keyUp)
		return false;
	Char16	c = inKeyEvent.message & charCodeMask;
	switch (c)
	{
		case char_Backspace:
		case char_FwdDelete:
			ObeyCommand(cmd_Clear, NULL);
			return true;
			
		case char_Space:
			CHyperScroller*	messageViewScroller = GetScroller();
			Boolean			shiftKeyDown = (inKeyEvent.modifiers & shiftKey) != 0;
			
			if ( messageViewScroller )
			{
				if ( !shiftKeyDown && messageViewScroller->ScrolledToMaxVerticalExtent() )
				{
					CDeferredCommand*	deferredGoCommand = new CDeferredCommand(mMasterCommander, cmd_NextMessage, NULL);
					CDeferredTaskManager::Post1(deferredGoCommand, this);
					return true;
				}
				
				if ( shiftKeyDown && messageViewScroller->ScrolledToMinVerticalExtent() )
				{
					CDeferredCommand*	deferredGoCommand = new CDeferredCommand(mMasterCommander, cmd_PreviousMessage, NULL);
					CDeferredTaskManager::Post1(deferredGoCommand, this);
					return true;
				}
			}
			// FALL THROUGH
			
		default:
			Inherited::HandleKeyPress(inKeyEvent);
			return true;
	} // switch
	return false;
} // CMessageView::HandleKeyPress

// ---------------------------------------------------------------------------
MessageKey CMessageView::GetCurMessageKey() const
// ---------------------------------------------------------------------------
{
	MessageKey result = MSG_MESSAGEKEYNONE;
	if (mMessagePane && !mLoadingNakedURL)
		::MSG_GetCurMessage(mMessagePane, NULL, &result, NULL);
	return result;
} // CMessageView::GetCurMessageKey

// ---------------------------------------------------------------------------
MSG_FolderInfo* CMessageView::GetFolderInfo() const
// ---------------------------------------------------------------------------
{
	MSG_FolderInfo* result = NULL;
	if (mMessagePane)
		::MSG_GetCurMessage(mMessagePane, &result, NULL, NULL);
	return result;
} // CMessageView::GetFolderInfo

// ---------------------------------------------------------------------------
MSG_ViewIndex CMessageView::GetCurMessageViewIndex() const
// ---------------------------------------------------------------------------
{
	MSG_ViewIndex result;
	::MSG_GetCurMessage(mMessagePane, NULL, NULL, &result);
	return result;
} // CMessageView::GetCurMessageViewIndex

// ---------------------------------------------------------------------------
uint32 CMessageView::GetFolderFlags() const
// ---------------------------------------------------------------------------
{
	MSG_FolderInfo* folderInfo = GetFolderInfo();
	if (folderInfo)
	{
		MSG_FolderLine folderLine;
		MSG_GetFolderLineById(CMailNewsContext::GetMailMaster(), folderInfo, &folderLine);
		return folderLine.flags;
	}
	return 0;
} // CMessageView::GetFolderFlags

// ---------------------------------------------------------------------------
uint32 CMessageView::GetCurMessageFlags() const
// ---------------------------------------------------------------------------
{
	MessageKey key = GetCurMessageKey();
	if (key != MSG_MESSAGEKEYNONE)
	{
		MSG_MessageLine info;
		::MSG_GetThreadLineById(mMessagePane, key, &info);
		return info.flags;
	}
	return 0;
} // CMessageView::GetCurMessageFlags

enum { MSG_PaneNotifyOKToLoadNewMessage = 999 }; // also in msgmpane.cpp

// ---------------------------------------------------------------------------
void CMessageView::PaneChanged(
	MSG_Pane*,
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 value)
// ---------------------------------------------------------------------------
{
	switch (inNotifyCode)
	{	
		case MSG_PaneNotifyOKToLoadNewMessage:
			*(XP_Bool*)value = !IsDueToCloseLater();
			return;
		case MSG_PaneNotifyFolderLoaded:
			/*if (mMotionPendingCommand != (MSG_MotionType)-1)
			{
				ObeyMotionCommand( mMotionPendingCommand );
				mMotionPendingCommand = (MSG_MotionType)-1;
			}*/
			break;

		case MSG_PaneNotifyMessageLoaded:
			if (!mMasterCommander)
			{
				// If we're in a a stand-alone message window, we're responsible for
				// this.  Otherwise, the thread view is.
				if (MSG_GetBacktrackState(GetMessagePane()) == MSG_BacktrackIdle)
				{
					MSG_AddBacktrackMessage(
						GetMessagePane(),
							GetFolderInfo(),
								GetCurMessageKey());
				}
				else
					MSG_SetBacktrackState(GetMessagePane(), MSG_BacktrackIdle);
				// Need to enable the menus since we may just have loaded a new message using imap
				SetUpdateCommandStatus(true);
			}
			break;
		case MSG_PaneNotifyFolderDeleted:
			if ((MSG_FolderInfo*)value != GetFolderInfo())
				break;
			// else fall through
		case MSG_PaneNotifyMessageDeleted:
			// As of 98/01/23, this notification is never received.
			LWindow* win = LWindow::FetchWindowObject(GetMacPort());
			if (win)
				win->AttemptClose();
			break;
		case MSG_PaneNotifyCopyFinished:
			// This means that a copy that was run in this messagepane has completed.
			MaybeCloseLater(cmd_MoveMailMessages);
			break;
		default:
			break;
	}
} // CMessageView::PaneChanged

// ---------------------------------------------------------------------------
void CMessageView::SetContext(CBrowserContext* inNewContext)
// ---------------------------------------------------------------------------
{
	if (mAttachmentView)
	{
		mAttachmentView->ClearMessageAttachmentView(); // before we destroy the pane!
		mAttachmentView->Hide();
	}
	// If this is a "set to nil" call, then we must call MSG_DestroyPane
	// before calling the inherited method, because MSG_Lib will attempt
	// to call back into the context during destruction....
	if (!inNewContext && mMessagePane)
	{
		// Interrupting the context later after the message pane is destroyed can
		// cause problems.
		// Bug #106218 (crash closing before load finished).
		// Before calling interrupt context, make sure it doesn't call us back.  Previously,
		// this next call was after the call to XP_InterruptContext() -- jrm 98/02/13
		::MSG_SetMessagePaneCallbacks(mMessagePane, NULL, NULL);
		if( mContext )
			XP_InterruptContext(*mContext);
		::MSG_DestroyPane(mMessagePane);
		mMessagePane = nil;
	}
	Inherited::SetContext(inNewContext);
} // CMessageView::SetContext

// ---------------------------------------------------------------------------
void CMessageView::InstallBackgroundColor()
// Overridden to use a white background
// ---------------------------------------------------------------------------
{
	// е install the user's default solid background color & pattern
	memset(&mBackgroundColor, 0xFF, sizeof(mBackgroundColor)); // white is default
}

// ---------------------------------------------------------------------------
void CMessageView::SetBackgroundColor(
	Uint8 					/*inRed*/,
	Uint8					/*inGreen*/,
	Uint8 					/*inBlue*/)
// Overridden to use a white background
// ---------------------------------------------------------------------------
{
	// Do not allow the layout weenies to clobber mail message backgrounds.
	Inherited::SetBackgroundColor(0xFF, 0xFF, 0xFF); // white is default
}

// ---------------------------------------------------------------------------
Boolean CMessageView::SetDefaultCSID(Int16 default_csid, Boolean forceRepaginate /* = false */)
// ---------------------------------------------------------------------------
{
	if (mContext && default_csid != mContext->GetDefaultCSID())
		mContext->SetDocCSID(default_csid);
	
	Inherited::SetDefaultCSID(default_csid);		// еее do we want the repaginate fix??
	return true;
} // CMessageView::SetDefaultCSID

// ---------------------------------------------------------------------------
void CMessageView::GetDefaultFileNameForSaveAs(URL_Struct* url, CStr31& defaultName)
// ---------------------------------------------------------------------------
{
	// Overridden by CMessageView to use the message subject.
	MSG_MessageLine info;
	MSG_GetThreadLineById(GetMessagePane(), GetCurMessageKey(), &info);
	// remove useless and bad characters.
	char buf[32];
	char* dst = buf;
	char* src = info.subject;
	size_t len = strlen(src);
	char* srcEnd = src + len - 1;
	while (*srcEnd == ']')
	{
		*srcEnd-- = '\0';
		len--;
	}
	while (true)
	{
		char* bracket = strchr(src, '[');
		if (bracket)
		{
			len -= (bracket - src + 1);
			src = bracket + 1;
			continue;
		}
		break;
	}

	char* lastPlaceInBuffer = buf+31;
	char c;
	do
	{
		c = *src++;
		// rough stab at removing junk from the subject.
		if (c == ':' || c == '[' || c == ']')
			continue;
		*dst++ = c;
	} while (c && dst < lastPlaceInBuffer );
	defaultName = buf; // CStr31 does the truncation.
	if (defaultName.Length() == 0)
		Inherited::GetDefaultFileNameForSaveAs(url, defaultName);
} // CMessageView::GetDefaultFileNameForSaveAs


// 97-06-18
// This method gets called when window changes size.
// Make sure we call mContext->Repaginate()
// In mail/news call reload method to NET_NORMAL_RELOAD
// ** NOTE: **
// This method goes around CHTMLView::AdaptToSuperFrameSize, so if that code
// changes, we should make the appropriate changes here also.
// ---------------------------------------------------------------------------
void CMessageView::AdaptToSuperFrameSize(
// ---------------------------------------------------------------------------
	Int32					inSurrWidthDelta,
	Int32					inSurrHeightDelta,
	Boolean					inRefresh)
{
	LView::AdaptToSuperFrameSize(inSurrWidthDelta, inSurrHeightDelta, inRefresh);

	if (IsRootHTMLView())
		{
		if ((mContext != NULL) && ((inSurrWidthDelta != 0) || (inSurrHeightDelta != 0)))
			mContext->Repaginate(NET_NORMAL_RELOAD);
		}
}

// ---------------------------------------------------------------------------
void CMessageView::DispatchURL(
	URL_Struct* inURLStruct,
	CNSContext* inTargetContext,
	Boolean inDelay,
	Boolean inForceCreate,
	FO_Present_Types inOutputFormat)
// ---------------------------------------------------------------------------
{
	Inherited::DispatchURL(inURLStruct, inTargetContext, inDelay, inForceCreate, inOutputFormat);
}

// ---------------------------------------------------------------------------
void CMessageView::DispatchURL(CURLDispatchInfo* inDispatchInfo)
// ---------------------------------------------------------------------------
{
	Boolean	openBrowserWindow = true;
	const char* urlOfLink = inDispatchInfo->GetURL();

	char* mimeType = MimeGetURLContentType((MWContext *)(*mContext), urlOfLink);
	
	if (mimeType)
	{
		CMimeMapper* mapper = CPrefs::sMimeTypes.FindMimeType( mimeType );
		if (mapper)
			switch (mapper->GetLoadAction()) {
				case CMimeMapper::Save:
				case CMimeMapper::Launch:
				case CMimeMapper::Unknown:
					openBrowserWindow = false;
					break;
			}
		if (!XP_STRCMP( mimeType, MULTIPART_APPLEDOUBLE ))
			openBrowserWindow = false;
	
		XP_FREE(mimeType);
	}
	
	if (!openBrowserWindow) {
		// Decide where to put the file...
		CStr31 		fileName;
		OSErr		err;
		FSSpec		parentFolder;
		FSSpec		destSpec;
		
		fe_FileNameFromContext(*mContext, urlOfLink, fileName);		
		parentFolder = CPrefs::GetFilePrototype( CPrefs::DownloadFolder );
		err = CFileMgr::UniqueFileSpec( parentFolder, fileName, destSpec );
		ThrowIfError_(err);

		
		
		inDispatchInfo->SetFileSpec(destSpec);
		CBrowserContext* theContext = nil;
		CDownloadProgressWindow* theProgressWindow = nil;
		try
		{
			theContext = new CBrowserContext(MWContextSaveToDisk);
			ThrowIfNULL_(theContext);
			theProgressWindow = dynamic_cast<CDownloadProgressWindow*>(
				URobustCreateWindow::CreateWindow(WIND_DownloadProgress, LCommander::GetTopCommander()));
			ThrowIfNULL_(theProgressWindow);
			
			theProgressWindow->SetWindowContext(theContext);
			
			theProgressWindow->Show();
			theProgressWindow->Select();
			
			theContext->ImmediateLoadURL(inDispatchInfo->ReleaseURLStruct(), FO_CACHE_AND_PRESENT);
		}
		catch(...)
		{
			delete theContext;
			delete theProgressWindow;
		}
	} else {

		// Set target context to nil if we're not dispatching to an internal anchor
		// so that we don't lay out into message view.  An internal anchor will START
		// with a '#'.  Note that dispatching to a new window from a message view 
		// is never wrong.  But loading weird stuff into the same message view IS.
		if (!XP_STRCHR(urlOfLink, '#')) 
			inDispatchInfo->SetTargetContext(nil); // not an anchor of any kind
		else if (urlOfLink[0] != '#') // If starts with '#', definitely internal - doesn't occur in practice.
		{
			// It's an anchor.  But is it internal?
			URL_Struct* urlStructForMessage = MSG_ConstructUrlForMessage(GetMessagePane(), GetCurMessageKey());
			const char* urlForMessage = urlStructForMessage->address;
			if (strstr(urlOfLink, urlForMessage) != urlOfLink)
			{
				// it's not a link to THIS message
				inDispatchInfo->SetTargetContext(nil);
			}
			NET_FreeURLStruct(urlStructForMessage);
		}
		Inherited::DispatchURL(inDispatchInfo);
	}
} // CMessageView::DispatchURL
