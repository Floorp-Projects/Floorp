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



// CMessageWindow.cp

#include "CMessageWindow.h"

#include "CMessageAttachmentView.h"
#include "CThreadView.h" // for CMessage

#include "uapp.h"
#include "ntypes.h"
#include "macutil.h"
#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"

#include "CBrowserContext.h"
#include "CMessageView.h"
#include "CProgressListener.h"
#include "CMailFolderButtonPopup.h"
#include "CThreadWindow.h"
#include "CMailNewsContext.h"
#include "CSpinningN.h"
#include "prefapi.h"
#include "URobustCreateWindow.h"
#include "CApplicationEventAttachment.h"
#include "CProxyPane.h"
#include "CProxyDragTask.h"
#include "UMailSelection.h"
#include "CSearchTableView.h"

#include "libi18n.h"

#include <LIconPane.h>

//----------------------------------------------------------------------------------------
CMessageWindow::CMessageWindow(LStream* inStream)
//----------------------------------------------------------------------------------------
:	CMailNewsWindow(inStream, WindowType_Message)
,	mContext(NULL)
{
}

//----------------------------------------------------------------------------------------
CMessageWindow::~CMessageWindow()
//----------------------------------------------------------------------------------------
{
	SetWindowContext(nil);
}

//----------------------------------------------------------------------------------------
void CMessageWindow::FinishCreateSelf()
//----------------------------------------------------------------------------------------
{
	UReanimator::LinkListenerToControls(this, this, res_ID);
	Inherited::FinishCreateSelf();
	CMessageView* messageView = GetMessageView();
	Assert_(messageView);
	SetLatentSub(messageView);
	CMessageAttachmentView* attachmentView	=
			dynamic_cast<CMessageAttachmentView*>( FindPaneByID('MATv') );
	if( attachmentView )
	{
		messageView->SetAttachmentView( attachmentView );
		attachmentView->ClearMessageAttachmentView();
		attachmentView->Hide();
	}
	
	// the location toolbar is a waste of space and should be hidden by default
	LPane *locationBar = FindPaneByID(cMailNewsLocationToolbar);
	if (mToolbarShown[CMailNewsWindow::LOCATION_TOOLBAR])
		ToggleDragBar(cMailNewsLocationToolbar, CMailNewsWindow::LOCATION_TOOLBAR);

	USecurityIconHelpers::AddListenerToSmallButton(
		this /*LWindow**/,
		(CHTMLView*)messageView /*LListener**/);
}

//----------------------------------------------------------------------------------------
CMessageView* CMessageWindow::GetMessageView()
//----------------------------------------------------------------------------------------
{
	return dynamic_cast<CMessageView*>(FindPaneByID(CMessageView::class_ID));
}

//----------------------------------------------------------------------------------------
void CMessageWindow::SetWindowContext(CBrowserContext* inContext)
//----------------------------------------------------------------------------------------
{
	CBrowserContext* oldContext = mContext; // save for below.
	CSpinningN* theN = dynamic_cast<CSpinningN*>(FindPaneByID(CSpinningN::class_ID));
	mContext = inContext;	
	if (mContext != NULL)
	{
		mContext->AddListener(this);
		mContext->AddUser(this);
		Assert_(mProgressListener != NULL);
		mContext->AddListener(mProgressListener);
		mContext->AddListener( &mSecurityListener);
		if (theN)
			mContext->AddListener(theN);
	}
	CMessageView* theMessageView = GetMessageView();
	Assert_(theMessageView); // Can happen in lowmem, if creation fails
	if (theMessageView)
	{
		theMessageView->SetContext(mContext);
		mSecurityListener.SetMessageView( theMessageView );
		// This call links up the model object hierarchy for any potential
		// sub model that gets created within the scope of the html view.
		theMessageView->SetFormElemBaseModel(this);
	}
	// Now it's safe to delete the old context, because the view's done with it.
	if (oldContext)
	{
		oldContext->RemoveListener( &mSecurityListener );
		mSecurityListener.SetMessageView( NULL );
		oldContext->RemoveListener(this);
		if (theN)
			oldContext->RemoveListener(theN);
		oldContext->RemoveUser(this); // and delete, probably.
	}
} // CMessageWindow::SetWindowContext

//----------------------------------------------------------------------------------------
cstring	CMessageWindow::GetCurrentURL()
//----------------------------------------------------------------------------------------
{
	if (mContext)
		return mContext->GetCurrentURL();
	else
		return cstring("");
}

//----------------------------------------------------------------------------------------
Int16 CMessageWindow::DefaultCSIDForNewWindow()
//----------------------------------------------------------------------------------------
{
	Int16 csid = 0;
	if (mContext != NULL)
		csid = mContext->GetDefaultCSID();
	if(0 == csid)
	{
		CMessageView* theMessageView = GetMessageView();
		Assert_(theMessageView != NULL);
		csid = theMessageView->DefaultCSIDForNewWindow();
	}
	return csid;		
} // CMessageWindow::DefaultCSIDForNewWindow

//----------------------------------------------------------------------------------------
void CMessageWindow::ListenToMessage(MessageT inMessage, void* ioParam)
//----------------------------------------------------------------------------------------
{
	switch (inMessage)
		{
		case msg_NSCDocTitleChanged:
		{
			CStr255 theTitle((const char*)ioParam);
			// if we have a message view use the subject as the title
			CMessageView* messageView = GetMessageView();
			if (messageView)
			{
				MessageKey id = messageView->GetCurMessageKey();
				if (id != MSG_MESSAGEKEYNONE )
				{
					MSG_MessageLine messageLine;
					::MSG_GetThreadLineById(messageView->GetMessagePane(), id, &messageLine);
					char buffer[256];
					const char* raw = CMessage::GetSubject(&messageLine, buffer, sizeof(buffer)-1);
					char* conv = IntlDecodeMimePartIIStr(raw, GetWindowContext()->GetWinCSID(), FALSE);
					theTitle = CStr255((conv != NULL) ? conv : raw);
					if (conv)
						XP_FREE(conv);
				}
			}
			SetDescriptor(theTitle);
			CProxyPane* proxy = dynamic_cast<CProxyPane*>(FindPaneByID(CProxyPane::class_ID));
			if (proxy)
				proxy->ListenToMessage(inMessage, (char*)theTitle);
			break;
		}
		case msg_NSCLayoutNewDocument:
			//NoteBeginLayout();
			break;
		
		case msg_NSCFinishedLayout:
			//NoteFinishedLayout();
			break;

		case msg_NSCAllConnectionsComplete:
			//NoteAllConnectionsComplete();
			CSpinningN* theN = dynamic_cast<CSpinningN*>(FindPaneByID(CSpinningN::class_ID));
			if (theN)
				theN->StopSpinningNow();
			break;
		case CMailCallbackManager::msg_ChangeStarting:
		case CMailCallbackManager::msg_ChangeFinished:
		case CMailCallbackManager::msg_PaneChanged:
			CMailCallbackListener::SetPane(GetMessageView()->GetMessagePane());
			if (IsMyPane(ioParam))
				CMailCallbackListener::ListenToMessage(inMessage, ioParam);
			break;
		default:
			// assume it's a button message.
			GetMessageView()->ObeyCommand(inMessage, ioParam);
		}
} // CMessageWindow::ListenToMessage

//----------------------------------------------------------------------------------------
void CMessageWindow::ChangeStarting(
	MSG_Pane*		/*inPane*/,
	MSG_NOTIFY_CODE	/*inChangeCode*/,
	TableIndexT		/*inStartRow*/,
	SInt32			/*inRowCount*/)
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CMessageWindow::ChangeFinished(
	MSG_Pane*		/*inPane*/,
	MSG_NOTIFY_CODE	/*inChangeCode*/,
	TableIndexT		/*inStartRow*/,
	SInt32			/*inRowCount*/)
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CMessageWindow::PaneChanged(
	MSG_Pane*						/*inPane*/,
	MSG_PANE_CHANGED_NOTIFY_CODE	inNotifyCode,
	int32							value)
//----------------------------------------------------------------------------------------
{
	switch (inNotifyCode)
	{
		case MSG_PaneNotifyMessageLoaded:
			AdaptToolbarToMessage();
			if (!GetMessageView()->IsDueToCloseLater())
				Show();
			break;
		case MSG_PaneNotifyLastMessageDeleted:
			// AttemptClose() here leads to nasty re-entrant interrrupt-context.
			// What to do?  Should close the window, but how?
			break;
		case MSG_PaneNotifyFolderDeleted:
			if ((MSG_FolderInfo*)value != GetMessageView()->GetFolderInfo())
				break;
			// ELSE FALL THROUGH...
		case MSG_PaneNotifyMessageDeleted:
			AttemptClose(); // Causes reentrant InterruptContext call.
			break;
	}
} // CMessageWindow::PaneChanged

//----------------------------------------------------------------------------------------
CMessageWindow* CMessageWindow::FindAndShow(MessageKey inMessageKey)
//----------------------------------------------------------------------------------------
{
	CWindowIterator iter(WindowType_Message, false);
	CMessageWindow* window;
	for (iter.Next(window); window; iter.Next(window))
	{
		window = dynamic_cast<CMessageWindow*>(window);
		ThrowIfNULL_(window);
		CMessageView* messageView = window->GetMessageView();
		// inMessageKey zero means return the first message window of any kind
		if (!inMessageKey || (messageView && messageView->GetCurMessageKey() == inMessageKey))
			return window;
	}
	return nil;
} // CMessageWindow::FindAndShow

//----------------------------------------------------------------------------------------
void CMessageWindow::OpenFromURL(const char* url)
//----------------------------------------------------------------------------------------
{
	MSG_MessageLine msgLine;
	MSG_GetMessageLineForURL(CMailNewsContext::GetMailMaster(), url, &msgLine);
	CMessageWindow* messageWindow = CMessageWindow::FindAndShow(msgLine.messageKey);
	if (messageWindow)
	{
		// Found it.  Bring it to the front.
		messageWindow->Select();
		return;
	}
	XP_Bool prefReuseWindow = 0; // recycle any message window
	PREF_GetBoolPref("mailnews.reuse_message_window", &prefReuseWindow);
	if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey) ^ prefReuseWindow)
	{
		messageWindow = CMessageWindow::FindAndShow(0);
	}
	// If we couldn't (or shouldn't) recycle one, make a new one.
	if (!messageWindow)
	{
		try
		{
			messageWindow =
					(CMessageWindow*)URobustCreateWindow::CreateWindow(
						CMessageWindow::res_ID,
						LCommander::GetTopCommander());
			CBrowserContext* theContext = new CBrowserContext(MWContextMailMsg);
			StSharer theShareLock(theContext); // if we throw, theContext will have no users & die
			messageWindow->SetWindowContext(theContext);
		}
		catch(...)
		{
			delete messageWindow;
			messageWindow = NULL;
			throw;
		}
	}
	// Whether it's a new one or an old one, load the message now.
	if (messageWindow)
	{
		try
		{
			CMessageView* messageView = messageWindow->GetMessageView();
			ThrowIfNULL_(messageView);
			messageView->ShowURLMessage(url, false);
			messageWindow->Select();
			messageWindow->Show();
		}
		catch(...)
		{
			delete messageWindow;
			messageWindow = NULL;
			throw;			
		}
	}
} // CMessageWindow::OpenFromURL

//----------------------------------------------------------------------------------------
/* static */ void CMessageWindow::CloseAll(MessageKey inMessageKey)
//----------------------------------------------------------------------------------------
{
	CMessageWindow* win;
	do {
		win = CMessageWindow::FindAndShow(inMessageKey);
		if (win )
			win->AttemptClose();
	} while (win);
} // CMessageWindow::CloseAll

//----------------------------------------------------------------------------------------
/* static */ void CMessageWindow::NoteSelectionFiledOrDeleted(const CMailSelection& inSelection)
//	Tell every relevant message window that its message has been moved, so that it
//	can, according to the preferences, close itself.
//----------------------------------------------------------------------------------------
{
	const MSG_ViewIndex* index = inSelection.GetSelectionList();
	MSG_PaneType paneType = ::MSG_GetPaneType(inSelection.xpPane);
	for (int count = 0; count < inSelection.selectionSize; count++, index++)
	{
		MessageKey key;
		switch (paneType)
		{
			case MSG_THREADPANE:
				// These are the list panes:
				key = ::MSG_GetMessageKey(inSelection.xpPane, *index);
				break;
			case MSG_SEARCHPANE:
				// These are the list panes:
				MSG_ResultElement* ignoredElement;
				MSG_FolderInfo* ignoredFolder;
				if (!CSearchTableView::GetMessageResultInfo(
					inSelection.xpPane,
					*index,
					ignoredElement,
					ignoredFolder,
					key))
					continue;
				break;
			case MSG_MESSAGEPANE:
				// The message pane itself is running the copy.  It will be notified
				// on completion and close itself.  Fall through and return.
			default:
				return; // no messages involved!  Zero loop iterations.
		}
		CWindowIterator iter(WindowType_Message, false);
		CMessageWindow* window;
		for (iter.Next(window); window; iter.Next(window))
		{
			window = dynamic_cast<CMessageWindow*>(window);
			Assert_(window);
			CMessageView* messageView = window->GetMessageView();
			if (messageView->GetCurMessageKey() == key)
				messageView->MaybeCloseLater(cmd_MoveMailMessages);
		} // for iter
	} // for count
} // CMessageWindow::CloseAll

//----------------------------------------------------------------------------------------
void CMessageWindow::ActivateSelf()
//----------------------------------------------------------------------------------------
{	
	CMailNewsWindow::ActivateSelf();
}

//----------------------------------------------------------------------------------------
void CMessageWindow::AdaptToolbarToMessage()
//----------------------------------------------------------------------------------------
{

	const PaneIDT paneID_MessageFileButton		= 'Bfil';
	const PaneIDT paneID_MessageReplyButton		= 'Brep';

	const PaneIDT paneID_MessageGetMailButton	= 'Bget';	// mail
	const PaneIDT paneID_MessageGetNewsButton	= 'Bmor';	// news

	const PaneIDT paneID_MessageComposeButton	= 'Bcmp';	// mail
	const PaneIDT paneID_MessagePostNewButton	= 'Bpst';	// news

	const PaneIDT paneID_MessagePrintButton		= 'Bprn';	// mail
	const PaneIDT paneID_MessageBackButton		= 'Bbck';	// news

	const PaneIDT paneID_MessageDeleteButton		= 'Bdel';	// mail
	const PaneIDT paneID_MessageMarkButton		= 'Bmrk';	// news

	CMessageView* messageView = GetMessageView();
	Assert_(messageView != NULL);

	uint32 folderFlags = messageView->GetFolderFlags();
	
	// Set the window title to the subject of the message.
	MessageKey id = messageView->GetCurMessageKey();
	if (id != MSG_MESSAGEKEYNONE)
	{
		MSG_MessageLine messageLine;
		::MSG_GetThreadLineById(messageView->GetMessagePane(), id, &messageLine);
		char buffer[256];
		const char* raw = CMessage::GetSubject(&messageLine, buffer, sizeof(buffer)-1);
		char* conv = IntlDecodeMimePartIIStr(raw, GetWindowContext()->GetWinCSID(), FALSE);
		SetDescriptor(CStr255((conv != NULL) ? conv : raw));
		if (conv)
			XP_FREE(conv);
	}
	
	// show/hide buttons depending on the window type (News vs Mail)
	LControl *			aControl;
	Boolean				isNewsWindow	= ((folderFlags & MSG_FOLDER_FLAG_NEWSGROUP) != 0);
	const short	kBtnCount		= 3;

	PaneIDT		mailBtn[kBtnCount]	=
					{
						paneID_MessageGetMailButton, 
						paneID_MessageComposeButton, 
						paneID_MessageDeleteButton 	// update kBtnCount if you add a btn
					};

	PaneIDT		newsBtn[kBtnCount]	=
					{
						paneID_MessageGetNewsButton, 
						paneID_MessagePostNewButton, 
						paneID_MessageMarkButton 	// update kBtnCount if you add a btn
					};

	for (short btnIndex = 0; btnIndex < kBtnCount; btnIndex ++)
	{
		if (isNewsWindow)
		{
			aControl = dynamic_cast<LControl *>(FindPaneByID(mailBtn[btnIndex]));
			if (aControl != nil) aControl->Hide();
			aControl = dynamic_cast<LControl *>(FindPaneByID(newsBtn[btnIndex]));
			if (aControl != nil) aControl->Show();
		}
		else
		{
			aControl = dynamic_cast<LControl *>(FindPaneByID(newsBtn[btnIndex]));
			if (aControl != nil) aControl->Hide();
			aControl = dynamic_cast<LControl *>(FindPaneByID(mailBtn[btnIndex]));
			if (aControl != nil) aControl->Show();
		}
	}

	// other changes depending on the window type
	
	if (isNewsWindow)
	{
		aControl = dynamic_cast<LControl *>(FindPaneByID(paneID_MessageReplyButton));
		if (aControl != nil) aControl->SetValueMessage(cmd_PostReply);		// quick-click default
	}
	else
	{
		aControl = dynamic_cast<LControl *>(FindPaneByID(paneID_MessageReplyButton));
		if (aControl != nil) aControl->SetValueMessage(cmd_ReplyToSender);	// quick-click default
	}

	UInt32 messageFlags = messageView->GetCurMessageFlags();
	ResIDT iconID = (id == MSG_MESSAGEKEYNONE)
					?	CProxyPane::kProxyIconNormalID
					:	CMessage::GetIconID(folderFlags, messageFlags);
	LIconPane* proxyIcon = dynamic_cast<LIconPane*>(FindPaneByID('poxy'));
	if (proxyIcon)
	{
		proxyIcon->SetIconID(iconID);
	}
	CProxyPane* newProxy = dynamic_cast<CProxyPane*>(FindPaneByID(CProxyPane::class_ID));
	if (newProxy)
	{
		newProxy->SetIconIDs(iconID, iconID);
	}
} // CMessageWindow::AdaptToolbarToMessage

//----------------------------------------------------------------------------------------
CExtraFlavorAdder* CMessageWindow::CreateExtraFlavorAdder() const
//----------------------------------------------------------------------------------------
{
	class MessageWindowFlavorAdder : public CExtraFlavorAdder
	{
		public:
			MessageWindowFlavorAdder(CMessageView* inMessageView)
			: mMessageView(inMessageView)
			{
			}
			virtual void AddExtraFlavorData(DragReference inDragRef, ItemReference inItemRef)
			{
			#if 1
				// Pass a selection using the message view. Not sure if this will work with the BE
				mSelection.xpPane = mMessageView->GetMessagePane();
				if (!mSelection.xpPane)
					return;
				MSG_ViewIndex viewIndex = mMessageView->GetCurMessageViewIndex();
			#else
				// Pass a selection, as if this is done from a thread view.
				mSelection.xpPane = ::MSG_FindPaneOfType(
					CMailNewsContext::GetMailMaster(),
					mMessageView->GetFolderInfo(), 
					MSG_THREADPANE);
				if (!mSelection.xpPane)
					return; // drat.  This is a real drag.
				MSG_ViewIndex viewIndex = mMessageView->GetCurMessageKey();
				MSG_ViewIndex viewIndex = MSG_GetIndexForKey(mSelection.xpPane, key, true);
			#endif
				if (viewIndex == MSG_VIEWINDEXNONE)
					return;
				mSelection.SetSingleSelection(viewIndex);
				::AddDragItemFlavor(
					inDragRef,
					inItemRef,
					kMailNewsSelectionDragFlavor,
					&mSelection,
					sizeof(mSelection),
					0);
			}
		private:
			CMessageView*		mMessageView;
			CMailSelection		mSelection;
	};
	return new MessageWindowFlavorAdder(const_cast<CMessageWindow*>(this)->GetMessageView());

} // CMessageWindow::CreateExtraFlavorAdder
