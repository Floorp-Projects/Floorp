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



// CThreadWindow.cp
// This is the 3-pane mail window

#include "CThreadWindow.h"

#include "CMessageFolder.h"
#include "CGrayBevelView.h"
#include "CProgressListener.h"
#include "CBrowserContext.h"
#include "CThreadView.h"
#include "CMessageView.h"
#include "CMessageFolderView.h"
#include "CMailFolderButtonPopup.h"
#include "CSpinningN.h"
#include "CApplicationEventAttachment.h"
#include "CMailProgressWindow.h"
#include "CProxyPane.h"
#include "CProxyDragTask.h"
#include "LTableViewHeader.h"
#include "CTargetFramer.h"

#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"

#include "msg_srch.h"
//#include "secnav.h"

#include "uapp.h"
#include "macutil.h"
#include "StSetBroadcasting.h"
#include "earlmgr.h"
#include "prefapi.h"
#include <LIconPane.h>
#include <LGAIconSuiteControl.h>
#include "CMessageAttachmentView.h"
#include "CThreadMessageController.h"
#include "CFolderThreadController.h"
#include "UDeferredTask.h"
#include "UMailSelection.h"

#define kTwistieID 'Twst'
static const PaneIDT paneID_ThreadFileButton 	= 'Bfil';
static const PaneIDT paneID_ThreadReplyButton 	= 'Brep';
static const PaneIDT paneID_ThreadGetMailButton	= 'Bget';	// mail
static const PaneIDT paneID_ThreadGetNewsButton	= 'Bmor';	// news
static const PaneIDT paneID_ThreadComposeButton = 'Bcmp';	// mail
static const PaneIDT paneID_ThreadPostNewButton = 'Bpst';	// news
static const PaneIDT paneID_ThreadPrintButton	= 'Bprn';	// mail
static const PaneIDT paneID_ThreadBackButton	= 'Bbck';	// news
static const PaneIDT paneID_ThreadDeleteButton	= 'Bdel';	// mail
static const PaneIDT paneID_ThreadMarkButton	= 'Bmrk';	// news

const PaneIDT paneID_ThreadRelocateButton 	= 'MfPM';


//-----------------------------------
CThreadWindow::CThreadWindow(LStream *inStream)
//-----------------------------------
:	Inherited(inStream, WindowType_MailThread)
,	mStatusResID(res_ID)
,	mThreadView(nil)
,	mFolderContext(nil)
,	mFolderView(nil)
,	mFolderThreadController(nil)
,	mThreadMessageController(nil)
{
} // CThreadWindow::CThreadWindow

//-----------------------------------
CThreadWindow::~CThreadWindow()
//-----------------------------------
{
	if (mFolderContext)
	{
		mFolderContext->RemoveUser(this);
		mFolderContext = nil;
	}
} // CThreadWindow::~CThreadWindow

#define kButtonBarResID 10513
	// Common included button bar for thread windows.
	// This has the RIdl for the buttons.
	
//-----------------------------------
void CThreadWindow::FinishCreateSelf()
//-----------------------------------
{
	CNetscapeWindow::FinishCreateSelf(); // SKIP CMailNewsWindow, and roll our own!
		// should be in resource, but there's a resource freeze.
	CExpandoDivider* threadMessageExpandoView = dynamic_cast<CExpandoDivider*>(FindPaneByID('Expo'));	
	
	// Initialize our stored view pointers.
	CMessageView* messageView = GetMessageView();
	Assert_(messageView);
	GetThreadView();	
	Assert_(mThreadView); // Else you're deadybones!
	
	CMessageAttachmentView* attachmentView	=
		dynamic_cast<CMessageAttachmentView*>( FindPaneByID('MATv') );
	mFolderView = dynamic_cast<CMessageFolderView*>(FindPaneByID('Flst'));
	ThrowIfNil_(mFolderView);

	//-----------
	// The following code is analogous to
	// CMailNewsWindow::FinishCreateSelf() except it deals with mFolderContext
	// as well as mMailNewsContext
	//-----------
	// Let there be a thread context
	mMailNewsContext = CreateContext();
	mMailNewsContext->AddUser(this);

	// Let there be a progress listener, placed in my firmament,
	// which shall listen to both contexts
	mProgressListener = new CProgressListener(this, mMailNewsContext);

	// Let there be a folder context
	mFolderContext = new CMailNewsContext(MWContextMail);
	StSharer theLock(mFolderContext);
	mFolderContext->AddUser(this);
	// The progress listener must also listen to the folder context
	mFolderContext->AddListener(mProgressListener);

#if !ONECONTEXTPERWINDOW
	// Make a listener list for the message context
	typedef LListener* SListenerPointer;
	const UInt8 kNumberOfListeners = 2;
	LListener** messageContextListeners = new SListenerPointer[kNumberOfListeners + 1];
	// The progress listener must also listen to the message context.
	messageContextListeners[0] = mProgressListener;
#endif

	// The progress listener is "just a bit" lazy during network activity and
	// "not at all" at idle time to display the URLs pointed by the mouse cursor.
	mProgressListener->SetLaziness(CProgressListener::lazy_NotAtAll);
	
	// The spinning N must listen to all three contexts
	CSpinningN* theN = dynamic_cast<CSpinningN*>(FindPaneByID(CSpinningN::class_ID));
	if (theN)
	{
		mFolderContext->AddListener(theN);
		mMailNewsContext->AddListener(theN);
#if !ONECONTEXTPERWINDOW
		messageContextListeners[1] = theN;
#endif
	}

#if !ONECONTEXTPERWINDOW
	// Terminate the listener list
	messageContextListeners[kNumberOfListeners] = 0;
#endif
	
	mThreadMessageController = new CThreadMessageController(
		threadMessageExpandoView
	,	mThreadView
	,	messageView
	,	attachmentView
#if !ONECONTEXTPERWINDOW
	,	messageContextListeners
#endif
	);
	
	mThreadMessageController->InitializeDimensions();

	// ThreadMessageController is a CExpandoListener, so let it do some listening!
	LControl* twistie = dynamic_cast<LControl*>(FindPaneByID(kTwistieID));
	if (twistie) twistie->AddListener((CExpandoListener*)mThreadMessageController);

	//-----------
	// The next part of this code code is analogous to
	// CMailNewsFolderWindow::FinishCreateSelf() except it deals with mFolderContext
	// instead of mMailNewsContext
	//-----------
	mFolderView->LoadFolderList(mFolderContext);
	UReanimator::LinkListenerToControls(mFolderView, this, kButtonBarResID);

	// Make the Folder/Thread Controller
	LDividedView* folderThreadExpandoView
		= dynamic_cast<LDividedView*>(FindPaneByID('ExCt'));	
	mFolderThreadController = new CFolderThreadController(
		folderThreadExpandoView,
		mMailNewsContext,
		mFolderView,
		mThreadView);

	SetLatentSub(mFolderView);
	mMailNewsContext->AddListener(mThreadView); // listen for all connections complete.		
	mFolderContext->AddListener(mFolderView); 	// listen for all connections complete.		
		
	mThreadMessageController->FinishCreateSelf();
	// The folder-thread controller is a tabgroup, tabbing between the folder, thread,
	// and message views.  We have to add the message view explicitly to the folder-
	// thread controller here, because it doesn't know about the message view.  And we have to
	// do that after the folder-thread controller adds the folder and thread subcommanders.
	mFolderThreadController->FinishCreateSelf();
	messageView->SetSuperCommander(mFolderThreadController);
	CTargetFramer* framer = new CTargetFramer();
	messageView->AddAttachment(framer);
	
	// make the message view listen to buttons
	UReanimator::LinkListenerToControls((CMessageView::Inherited *)messageView, this, kButtonBarResID);
	
	// If the folder-thread controller can't handle a command (because the wrong pane
	// is the target), let it pass it up to the thread-message controller, which has a
	// mechanism for delegating to the message view.
	mFolderThreadController->SetSuperCommander(mThreadMessageController);
	mThreadMessageController->SetSuperCommander(this);

	// The 2-pane and 3-pane thread view have the same pane controls, so:
	UReanimator::LinkListenerToControls(mThreadView, this, kButtonBarResID);
	USecurityIconHelpers::AddListenerToSmallButton(
		this /*LWindow**/,
		mThreadView /*LListener**/);

	LGAIconSuiteControl* offlineButton
		= dynamic_cast<LGAIconSuiteControl*>(FindPaneByID(kOfflineButtonPaneID));
	if (offlineButton)
		offlineButton->AddListener(CFrontApp::GetApplication());

	LControl* stopButton
		= dynamic_cast<LControl*>(FindPaneByID('Bstp'));
	if (stopButton)
		stopButton->AddListener((CHTMLView*)messageView);

	// Create the new window just like the frontmost one of the same type, if possible
	CWindowIterator iter(WindowType_MailThread);
	CMediatedWindow* window;
	CThreadWindow* templateWindow = nil;
	for (iter.Next(window); window; iter.Next(window))
	{
		templateWindow = dynamic_cast<CThreadWindow*>(window);
		if (!templateWindow || templateWindow != this)
			break;
	}
	if (templateWindow && templateWindow != this)
	{
		// Use the template window
		CSaveWindowStatus::FinishCreateWindow(templateWindow);
	}
	else
	{
		//Get it from disk
		CSaveWindowStatus::FinishCreateWindow();
	}
	ReadGlobalDragbarStatus();
	AdjustStagger(WindowType_MailThread);

	// And behold, he saw that it was good. 
	SetLatentSub(mThreadView);
	SwitchTarget(mThreadView); // only one of these will work.
} // CThreadWindow::FinishCreateSelf

//-----------------------------------
void CThreadWindow::AboutToClose()
//-----------------------------------
{
	CSaveWindowStatus::AttemptCloseWindow(); // Do this first: uses table and calls the window's WriteWindowStatus() method
	WriteGlobalDragbarStatus();
	mFolderView = nil;
	// This is special:
	mFolderContext->RemoveListener(mProgressListener);
	if (mFolderView)
	{
		if (mFolderContext)
			mFolderContext->RemoveListener(mFolderView); // don't listen for all connections complete.		
		mFolderView = nil;
	}
	mThreadView = nil;
	// This is special:
	mMailNewsContext->RemoveListener(mProgressListener);
	if (mThreadView)
	{
		if (mMailNewsContext)
			mMailNewsContext->RemoveListener(mThreadView); // don't listen for all connections complete.		
		mThreadView = nil;
	}

	if (mThreadMessageController)
	{
		mThreadMessageController->InstallMessagePane(false);
		mThreadMessageController->SetThreadView(nil);
//		delete mThreadMessageController; No, it's a subcommander and will be deleted when we are.
		mThreadMessageController = nil;
	}
	
	// InstallMessagePane(false) can call InterruptContext, which in turn can show the window
	// so we must call Hide() here. Hiding is necessary so that views in the window are not
	// the current target while we are deleting them here.
	if (IsVisible())
		Hide();
	
	// Delete the folder-thread controller now, thus deleting all the msglib panes.  We have
	// to do that first before deleting the context, because the destructor of the pane
	// references the context.  Caution: Commanders delete all their subcommanders
	// when they die.  So detach it before we delete it, so that our destructor doesn't
	// perform a double-deletion.
	
	//SwitchTarget(this);
	
	mFolderThreadController->SetSuperCommander(nil);
	delete mFolderThreadController;
	mFolderThreadController = nil;
	
	CSpinningN* theN = dynamic_cast<CSpinningN*>(FindPaneByID(CSpinningN::class_ID));
	if (theN)
		mFolderContext->RemoveListener(theN);
} // CThreadWindow::AboutToClose

//----------------------------------------------------------------------------------------
CNSContext* CThreadWindow::CreateContext() const
//----------------------------------------------------------------------------------------
{
#if ONECONTEXTPERWINDOW
	CNSContext* result = new CBrowserContext(MWContextMailMsg);
	FailNIL_(result);
	return result;
#else
	return Inherited::CreateContext();
#endif
} //CMailNewsWindow::CreateContext

//----------------------------------------------------------------------------------------
static void InterruptTableContext(CMailFlexTable* inTable)
//----------------------------------------------------------------------------------------
{
	if (inTable)
	{
		CNSContext* context = inTable->GetContext();
		if (context)
			XP_InterruptContext(*context);
	}
}

//----------------------------------------------------------------------------------------
static Boolean IsTableContextBusy(CMailFlexTable* inTable)
//----------------------------------------------------------------------------------------
{
	if (inTable)
	{
		CNSContext* context = inTable->GetContext();
		if (context)
			return XP_IsContextBusy(*context);
	}
	return false;
}

//----------------------------------------------------------------------------------------
void CThreadWindow::StopAllContexts()
//----------------------------------------------------------------------------------------
{
	InterruptTableContext(GetFolderView());
	InterruptTableContext(GetThreadView());
#if !ONECONTEXTPERWINDOW
	CMessageView* mv = GetMessageView();
	if (mv)
	{
		CBrowserContext* context = mv->GetContext();
		if (context)
			XP_InterruptContext(*context);
	}
#endif
}

//----------------------------------------------------------------------------------------
Boolean CThreadWindow::IsAnyContextBusy()
//----------------------------------------------------------------------------------------
{
	Boolean busy = IsTableContextBusy(GetFolderView());
	if (!busy)
		busy = IsTableContextBusy(GetThreadView());
#if !ONECONTEXTPERWINDOW
	if (!busy)
	{
		CMessageView* mv = GetMessageView();
		if (mv)
		{
			CBrowserContext* context = mv->GetContext();
			if (context)
				return XP_IsContextBusy(*context);
		}
	}
#endif
	return busy;
}

//-----------------------------------
void CThreadWindow::UpdateFilePopupCurrentItem()
//-----------------------------------
{
	CThreadView *threadView = GetThreadView();
	MSG_FolderInfo* folderInfo = nil;
	if (threadView != nil)
		folderInfo = threadView->GetOwningFolder();

	CMailFolderPatternTextPopup *popup2 = 
		dynamic_cast<CMailFolderPatternTextPopup *>(FindPaneByID(paneID_ThreadRelocateButton));
	if ( popup2 != nil && threadView->GetOwningFolder())
	{
		StSetBroadcasting setBroadcasting(popup2, false);	// Don't broadcast anything here
		popup2->MSetSelectedFolder(folderInfo);
	}
	
	CMailFolderSubmenu::SetSelectedFolder(folderInfo);
} // CThreadWindow::UpdateFilePopupCurrentItem

//-----------------------------------
void CThreadWindow::ActivateSelf()
//-----------------------------------
{	
	Inherited::ActivateSelf();
#if 0
	// Don't do this for the 3-pane UI
	// make sure the message view isn't target
	CMailFlexTable* threadView = GetThreadView();
	if (threadView)
	{
		SwitchTarget(threadView);
	}
#endif
	UpdateFilePopupCurrentItem(); // Is this really necessary?
} // CThreadWindow::ActivateSelf


//-----------------------------------
void CThreadWindow::AdaptToolbarToFolder()
//-----------------------------------
{
	CThreadView* threadView = GetThreadView();
	if (threadView == nil)
		return;

	// show/hide buttons depending on the window type (News vs Mail)
	LControl *			aControl;
	Boolean				isNewsWindow	= ((threadView->GetFolderFlags() & MSG_FOLDER_FLAG_NEWSGROUP) != 0);
	static const short	kBtnCount		= 3;
	
	PaneIDT		mailBtn[kBtnCount]	=
					{
						paneID_ThreadGetMailButton, 
						paneID_ThreadComposeButton, 
						paneID_ThreadDeleteButton 	// update kBtnCount if you add a btn
					};

	PaneIDT		newsBtn[kBtnCount]	=
					{
						paneID_ThreadGetNewsButton, 
						paneID_ThreadPostNewButton, 
						paneID_ThreadMarkButton 	// update kBtnCount if you add a btn
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
		aControl = dynamic_cast<LControl *>(FindPaneByID(paneID_ThreadReplyButton));
		if (aControl != nil) aControl->SetValueMessage(cmd_PostReply);		// quick-click default
	}
	else
	{
		aControl = dynamic_cast<LControl *>(FindPaneByID(paneID_ThreadReplyButton));
		if (aControl != nil) aControl->SetValueMessage(cmd_ReplyToSender);	// quick-click default
	}
	CMessageFolder owningFolder(threadView->GetOwningFolder());
	ResIDT iconID = owningFolder.GetIconID();

	LIconPane* proxyIcon = dynamic_cast<LIconPane*>(FindPaneByID('poxy'));
	if (proxyIcon)
		proxyIcon->SetIconID(iconID);

	CProxyPane* newProxy = dynamic_cast<CProxyPane*>(FindPaneByID(CProxyPane::class_ID));
	if (newProxy)
	{
		newProxy->SetIconIDs(iconID, iconID);
	}

} // CThreadWindow::AdaptToolbarToFolder

//-----------------------------------
CMailFlexTable* CThreadWindow::GetActiveTable()
// Get the currently active table in the window. The active table is the table in
// the window that the user considers to be receiving input.
//-----------------------------------
{
	CMailFlexTable* result = GetFolderView();
	if (result && result->IsOnDuty())
		return result;
	return GetThreadView();
} // CThreadWindow::GetActiveTable

//-----------------------------------
CMailFlexTable* CThreadWindow::GetSearchTable()
// Get the table which should be used to set the search base node.  The base class
// returns GetActiveTable() for this, but now life is more complicated...
// The fix is to return the folder view, even if it is not currently the target.  Otherwise,
// if we try to return the thread view and it's not currently displaying a folder, bad things
// can happen.
//-----------------------------------
{
	CMailFlexTable* result = GetFolderView();
	if (result)
		return result;
	result = GetThreadView();
	if (::MSG_GetCurFolder(result->GetMessagePane()))
		return result;
	return nil;
} // CThreadWindow::GetActiveTable

//-----------------------------------
CMessageFolderView* CThreadWindow::GetFolderView()
//-----------------------------------
{
	return mFolderView;
} //  CThreadWindow::GetFolderView

//-----------------------------------
CThreadView* CThreadWindow::GetThreadView()
//-----------------------------------
{
	if (!mThreadView)
		mThreadView = dynamic_cast<CThreadView*>(FindPaneByID('Tabl'));
	return mThreadView;
} //  CThreadWindow::GetThreadView


//-----------------------------------
CMessageView* CThreadWindow::GetMessageView()
//-----------------------------------
{
	return dynamic_cast<CMessageView*>(FindPaneByID(CMessageView::class_ID));
}

//-----------------------------------
void CThreadWindow::ShowMessageKey(MessageKey inKey)
//-----------------------------------
{
	CThreadView* threadView = GetThreadView();
	Assert_(threadView);

	threadView->SelectMessageWhenReady(inKey);

} // CThreadWindow::ShowSearchMessage

//-----------------------------------
CThreadWindow* CThreadWindow::FindOrCreate(
	const MSG_FolderInfo* inFolderInfo,
	CommandT inCommand)
//-----------------------------------
{
	// First see if there's an open window open to this folder.
	ThrowIf_(Memory_MemoryIsLow());
	CWindowIterator iter(WindowType_MailThread);
	CMediatedWindow* window;
	CThreadWindow* threadWindow = nil;
	for (iter.Next(window); window; iter.Next(window))
	{
		threadWindow = dynamic_cast<CThreadWindow*>(window);
		ThrowIfNULL_(threadWindow);
		CThreadView* threadView = threadWindow->GetThreadView();
		ThrowIfNULL_(threadView);
		if (threadView->GetOwningFolder() == inFolderInfo)
		{
			if (inCommand != cmd_Nothing)
				threadView->ObeyCommand(inCommand, nil);
			return threadWindow;
		}
	}

	// Window not found: let's create it
	return FindAndShow(inFolderInfo, true, inCommand, true);

} // CThreadWindow::FindOrCreate

//-----------------------------------
CThreadWindow* CThreadWindow::FindAndShow(
	const MSG_FolderInfo* inFolderInfo,
	Boolean inMakeNew,
	CommandT inCommand,
	Boolean forceNewWindow)
//-----------------------------------
{
	// NOTE: all references to 'categories' have been removed from
	// this function in v1.63 on Tuesday 97/06/03.

	// First see if there's an open window open to this folder.
	// If there is, bring it to the front.
	ThrowIf_(Memory_MemoryIsLow());
	CWindowIterator iter(WindowType_MailThread, false); // false, don't want hidden windows.
	CMediatedWindow* window;
	CThreadWindow* existingThreadWindow = nil;
	CThreadWindow* threadWindow = nil;
	for (iter.Next(window); window; iter.Next(window))
	{
		existingThreadWindow = dynamic_cast<CThreadWindow*>(window);
		ThrowIfNULL_(existingThreadWindow);
		CThreadView* tempThreadView = existingThreadWindow->GetThreadView();
		ThrowIfNULL_(tempThreadView);
		if (tempThreadView->GetOwningFolder() == inFolderInfo)
		{
			threadWindow = existingThreadWindow; // Found it!
			break;
		}
	}
	if (!threadWindow && !inMakeNew)
		return nil;

	// Re-use another Thread window, if possible...
	if (!threadWindow && !forceNewWindow)
	{
		XP_Bool prefReuseWindow;
		PREF_GetBoolPref("mailnews.reuse_thread_window", &prefReuseWindow);
		if ( CApplicationEventAttachment::CurrentEventHasModifiers(optionKey) )
			prefReuseWindow = !prefReuseWindow;
		if (prefReuseWindow)
		{
			CWindowIterator iter(WindowType_MailThread);	
			iter.Next(threadWindow);	// use the frontmost open thread window
		}
	}

	// ...Otherwise create a new window
	CMessageFolder folder(inFolderInfo);
	if (!threadWindow)
	{
		try
		{
			::SetCursor(*::GetCursor(watchCursor));
			SetDefaultCommander(LCommander::GetTopCommander());
			SetDefaultAttachable(nil);
			threadWindow = (CThreadWindow*) UReanimator::ReadObjects('PPob', res_ID);
#if 0
			// FIX ME: for CSaveWindowStatus, use one resID for inbox, and 2-pane resid
			// for all other thread panes.  We will not use staggering for inbox.
			threadWindow->SetStatusResID(folder.IsInbox() ? res_ID : res_ID_Alt);
#endif
			threadWindow->SetStatusResID(res_ID);
			threadWindow->FinishCreate();
		}
		catch (...)
		{
			delete threadWindow;
			threadWindow = nil;
			throw;
		}
	}

	CThreadView* messageList = threadWindow->GetThreadView();
	ThrowIfNULL_(messageList);
		
	// Load in the Mail messages or News postings
	if (messageList->GetOwningFolder() != inFolderInfo)
	{
		try
		{
			::SetCursor(*::GetCursor(watchCursor));
			if (threadWindow->mProgressListener)
				threadWindow->mProgressListener->SetLaziness(
					CProgressListener::lazy_VeryButForThisCommandOnly);
			messageList->EnableStopButton(true);
			CMessageFolderView* folderView = threadWindow->GetFolderView();
			if (folderView)
				folderView->SelectFolder(inFolderInfo);
			else
				messageList->LoadMessageFolder(
							(CMailNewsContext*)threadWindow->GetWindowContext(), // thread view's own context
							folder);	// loadNow = false.
		}
		catch(...)
		{
			// Note, threadView has timer that will delete window.
			throw;
		}
	}

	if (inCommand == cmd_GetNewMail)
	{
		// only do a get new mail here if were are executing the command on an existing window
		// (because the user must have really wanted it if the command was passed in)
		// or if we are online and the folder is pop (because IMAP gets new mail automatically
		// if the folder is loaded for the first time).
		if (threadWindow == existingThreadWindow
		|| (!NET_IsOffline() && !(folder.GetFolderFlags() & MSG_FOLDER_FLAG_IMAPBOX)))
		{
			// A thread window has two panes that can handle this command.  If the thread
			// pane is busy, why not give the folder pane something to do?
			Boolean handled = false;
			CNSContext* context = messageList->GetContext();
			if (context && XP_IsContextBusy(*context))
			{
				CMessageFolderView* fv = threadWindow->GetFolderView();
				if (fv)
				{
					context = fv->GetContext();
					if (context && !XP_IsContextBusy(*context))
					{
						CDeferredTaskManager::Post(new CDeferredCommand(fv, inCommand, nil), fv);
						handled = true;
					}
				}
			}
			if (!handled)
				messageList->ObeyCommandWhenReady(inCommand);
		}
	}
	else if (inCommand != cmd_Nothing)
		messageList->ObeyCommandWhenReady(inCommand);

	threadWindow->Show();
	threadWindow->Select();

	// HACK TO SUPPORT OPENING OF MORE THAN ONE FOLDER.
	// Otherwise, the BE doesn't initialize itself completely and the second window
	// fails.
	EventRecord ignoredEvent = {0};
	TheEarlManager.SpendTime(ignoredEvent);
	return threadWindow;
} // CThreadWindow::FindAndShow

//-----------------------------------
CThreadWindow* CThreadWindow::OpenFromURL(const char* url)
//-----------------------------------
{
	CThreadWindow* threadWindow = nil;
	MSG_FolderInfo* folderInfo = MSG_GetFolderInfoFromURL(
		CMailNewsContext::GetMailMaster(),
		url, true);
	if (!folderInfo)
		return nil;
	if (!CThreadWindow::FindAndShow(folderInfo, CThreadWindow::kDontMakeNew))
		threadWindow
			= CThreadWindow::FindAndShow(folderInfo, CThreadWindow::kMakeNew);
	return threadWindow;
}

//-----------------------------------
cstring	CThreadWindow::GetCurrentURL() const
//-----------------------------------
{
	if (mThreadMessageController)
		return mThreadMessageController->GetCurrentURL();
	else
		return cstring("");
}

//-----------------------------------
void CThreadWindow::SetFolderName(const char* inFolderName, Boolean inIsNewsgroup)
//-----------------------------------
{
	UpdateFilePopupCurrentItem();
	AdaptToolbarToFolder();
	
	if (!inFolderName)
		return;
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#else
#error "There'll probably be a bug here."
#endif
	struct WindowTemplate {
		Rect							boundsRect;
		short							procID;
		Boolean							visible;
		Boolean							filler1;
		Boolean							goAwayFlag;
		Boolean							filler2;
		long							refCon;
		Str255							title;
	};
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif
	// Name of window in PPob is "Netscape ^0 Ò^1Ó
	WindowTemplate** wt = (WindowTemplate**)GetResource('WIND', CThreadWindow::res_ID);
	Assert_(wt);
	if (wt)
	{
		StHandleLocker((Handle)wt);
		CStr255 windowTitle((*wt)->title);
		ReleaseResource((Handle)wt);
		CStr255 typeString;
		// Get "folder" or "newsgroup" as appropriate
		GetIndString(typeString, 7099, 1 + inIsNewsgroup);
		::StringParamText(windowTitle, (const char*)typeString, inFolderName);
		SetDescriptor(windowTitle);
		CProxyPane* proxy = dynamic_cast<CProxyPane*>(FindPaneByID(CProxyPane::class_ID));
		if (proxy)
			proxy->ListenToMessage(msg_NSCDocTitleChanged, (char*)windowTitle);
	}
} // CThreadWindow::SetFolderName

//-----------------------------------
void CThreadWindow::CloseAll(const MSG_FolderInfo* inFolderInfo)
//-----------------------------------
{
	CThreadWindow* win;
	do {
		win = CThreadWindow::FindAndShow(inFolderInfo);
		if (win)
			win->AttemptClose();
	} while (win);
} // CThreadWindow::CloseAll

//-----------------------------------
CThreadWindow* CThreadWindow::ShowInbox(CommandT inCommand)
//-----------------------------------
{
	CThreadWindow* result = nil;
	if (!CMailProgressWindow::GettingMail())
	{
		CMailNewsContext::ThrowIfNoLocalInbox(); // prefs have not been set up.
		
		MSG_FolderInfo* inboxFolderInfo = nil;
		int32 numberOfInboxes =  MSG_GetFoldersWithFlag(
			CMailNewsContext::GetMailMaster(),
			MSG_FOLDER_FLAG_INBOX,
			&inboxFolderInfo,
			1);
			
		if (!numberOfInboxes || inboxFolderInfo == nil)
			CMailNewsContext::AlertPrefAndThrow(PREF_PopHost);
			
		result = FindAndShow(inboxFolderInfo, true, inCommand);
	} 
	return result;
} // CThreadWindow::ShowInbox

//-----------------------------------
UInt16 CThreadWindow::GetValidStatusVersion() const
//-----------------------------------
{
	return 0x0132;
	// to be updated whenever the form of the saved window data changes
} // CThreadWindow::GetValidStatusVersion()

//-----------------------------------
void CThreadWindow::ReadWindowStatus(LStream *inStatusData)
//-----------------------------------
{
	//LCommander::SwitchTarget(mFolderView); // make sure it's the active table
	
	Inherited::ReadWindowStatus(inStatusData);
	
	if (inStatusData)
	{
		if (mFolderView)
			mFolderView->ReadSavedTableStatus(inStatusData);
			
		if (mThreadView)
			mThreadView->ReadSavedTableStatus(inStatusData);
			
		mThreadMessageController->ReadStatus(inStatusData);	
		mFolderThreadController->ReadStatus(inStatusData);	

		// We are currently closed, whatever the value of mExpandState may say (after
		// all, we only just read in the value).  So we set mExpandState back to
		// closed_state, and bang the twistie to set everything the right way.
		ExpandStateT state = mThreadMessageController->GetExpandState();
		mThreadMessageController->NoteExpandState(closed_state);
		LControl* twistie = dynamic_cast<LControl*>(FindPaneByID(kTwistieID));
		mThreadMessageController->SetStoreStatusEnabled(false);
		if (twistie)
			twistie->SetValue(state);
		mThreadMessageController->SetStoreStatusEnabled(true);
	}
} // CThreadWindow::ReadWindowStatus

//-----------------------------------
void CThreadWindow::WriteWindowStatus(LStream *outStatusData)
// Overridden to stagger in the default case.
//-----------------------------------
{
	// don't mess with targets when closing windows any more
	//LCommander::SwitchTarget(mFolderView); // make sure it's the active table
	
	Inherited::WriteWindowStatus(outStatusData);		// write the drag bar status and window location
	
	if (outStatusData)
	{
		if (mFolderView)
			mFolderView->WriteSavedTableStatus(outStatusData);
			
		if (mThreadView)
			mThreadView->WriteSavedTableStatus(outStatusData);
			
		mThreadMessageController->WriteStatus(outStatusData);		// write expando status
		mFolderThreadController->WriteStatus(outStatusData);		// write divided view status
	}
} // CThreadWindow::WriteWindowStatus

//-----------------------------------
ResIDT CThreadWindow::GetStatusResID() const
//-----------------------------------
{
	return mStatusResID;
} // CThreadWindow::GetStatusResID()

#if 0
//-----------------------------------
Boolean CThreadWindow::ObeyMotionCommand(MSG_ViewIndex index, MSG_MotionType cmd)
//-----------------------------------
{
	if (GetExpandState() == closed_state) return false;
	Try_
	{
		CMessageView* messageView = GetMessageView();
		if (messageView && messageView->ObeyMotionCommand(cmd))
		{
			CThreadView* threadView = GetThreadView();
			STableCell cell(resultIndex + 1,1);
			threadView->UnselectAllCells();
			threadView->SelectCell(cell);
		}
		else
			Throw_(noErr);
	}
	Catch_ (err)
	{
		SysBeep(1);
	}
	EndCatch_
	return false;
} // CThreadWindow::ObeyMotionCommand
#endif // 0

//-----------------------------------
Boolean CThreadWindow::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
//-----------------------------------
{
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
	return Inherited::ObeyCommand(inCommand, ioParam);
} // CThreadWindow::ObeyCommand

//-----------------------------------
void CThreadWindow::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
//-----------------------------------
{
	switch (inCommand)
	{
		case cmd_SelectSelection:
			outEnabled = true;
			return;
	}
	Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
} // CThreadWindow::FindCommandStatus

//-----------------------------------
Int16 CThreadWindow::DefaultCSIDForNewWindow()
//-----------------------------------
{
	Int16 csid = 0;
	CMessageView* messageView = GetMessageView();
	if (messageView)
		csid =	messageView->DefaultCSIDForNewWindow();	 
	if( 0 == csid)
	{
		CThreadView* threadView = GetThreadView();
		if(threadView)
			csid = threadView->DefaultCSIDForNewWindow();
	}
	return csid;
} // CThreadWindow::DefaultCSIDForNewWindow

//-----------------------------------
void CThreadWindow::SetDefaultCSID(Int16 default_csid)
//-----------------------------------
{
	CMessageView* messageView = GetMessageView();
	if (messageView)
		messageView->SetDefaultCSID(default_csid, true);		// force repagination
} // CThreadWindow::SetDefaultCSID

//-----------------------------------
URL_Struct* CThreadWindow::CreateURLForProxyDrag(char* outTitle)
//-----------------------------------
{
	return mThreadView->CreateURLForProxyDrag(outTitle);
} // CThreadWindow::CreateURLForProxyDrag

//----------------------------------------------------------------------------------------
const char* CThreadWindow::GetLocationBarPrefName() const
//----------------------------------------------------------------------------------------
{
	if (mFolderThreadController)
	{
		const char* result = mFolderThreadController->GetLocationBarPrefName();
		if (result)
			return result;
	}
	return Inherited::GetLocationBarPrefName();
}

//----------------------------------------------------------------------------------------
CExtraFlavorAdder* CThreadWindow::CreateExtraFlavorAdder() const
//----------------------------------------------------------------------------------------
{
	class ThreadWindowFlavorAdder : public CExtraFlavorAdder
	{
		public:
			ThreadWindowFlavorAdder(MSG_Pane* inFolderPane, MSG_FolderInfo* inFolderInfo)
			:	mFolderPane(inFolderPane)
			,	mFolderInfo(inFolderInfo)
			{
			}
			virtual void AddExtraFlavorData(DragReference inDragRef, ItemReference inItemRef)
			{
				// Pass a selection, as if this is done from a folder view.
				mSelection.xpPane = mFolderPane;
				if (!mSelection.xpPane)
					return; // drat.  This is a real drag.
				MSG_ViewIndex viewIndex = ::MSG_GetFolderIndexForInfo(
												mFolderPane, 
												mFolderInfo,
												true /*expand*/);
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
			MSG_Pane*			mFolderPane;
			MSG_FolderInfo*		mFolderInfo;
			CMailSelection		mSelection;
	};
	
	CThreadView* threadView = const_cast<CThreadWindow*>(this)->GetThreadView();
	MSG_FolderInfo* folderInfo = threadView->GetOwningFolder();
	MSG_Pane* folderPane = ::MSG_FindPane(nil, MSG_FOLDERPANE);
	return new ThreadWindowFlavorAdder(folderPane, folderInfo);
} // CMessageWindow::CreateExtraFlavorAdder
