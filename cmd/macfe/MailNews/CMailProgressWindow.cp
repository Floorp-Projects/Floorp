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



// CMailProgressWindow.cp

#include "CMailProgressWindow.h"

#include "MailNewsCallbacks.h"
#include "URobustCreateWindow.h"
#include "CMailNewsContext.h"
#include "PascalString.h"
#include "CProgressBar.h"
#include "COffscreenCaption.h"
#include "uapp.h" // to check if we're quitting.
#include "CWindowMenu.h"
#include "prefapi.h"
#include "CNetscapeWindow.h"
#include "UOffline.h"
#include "StSetBroadcasting.h"

const ResIDT cOfflineListID = 16010;
const Int16 cOfflineStrIndex = 3;


//======================================
class CDownloadListener : public CMailCallbackListener
// All Connections Complete is not always sent by operations within msglib.  Therefore,
// we must listen for the MSG_PaneProgressDone message via the pane-changed mechanism, too.
// So the progress window "has a" CDownloadListener, which has authority to force the
// progress window to close itself.
//======================================
{
public:
								CDownloadListener(
									CMailProgressWindow* inWindow, MSG_Pane* inPane);
	virtual void 				PaneChanged(
									MSG_Pane* inPane,
									MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
									int32 value); // must always be supported.
	CMailProgressWindow*		mProgressWindow;
}; // class CDownloadListener

//-----------------------------------
CDownloadListener::CDownloadListener(CMailProgressWindow* inWindow, MSG_Pane* inPane)
//-----------------------------------
:	mProgressWindow(inWindow)
{
 	SetPane(inPane);
}

//-----------------------------------
void CDownloadListener::PaneChanged(
	MSG_Pane* inPane,
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 value)
//-----------------------------------
{
#pragma unused (inPane)
#pragma unused (value)
	if (inNotifyCode == MSG_PaneProgressDone
		|| inNotifyCode == MSG_PaneNotifySelectNewFolder	// Bug #106165
		|| inNotifyCode == MSG_PaneNotifyNewFolderFailed	// ''
		|| inNotifyCode == MSG_PaneNotifyCopyFinished)		// Bug #113142 and #113860
	{
		mProgressWindow->ListenToMessage(msg_NSCAllConnectionsComplete, nil);
	}
}

//======================================
// class CMailProgressWindow
//======================================

CMailProgressWindow* CMailProgressWindow::sModalMailProgressWindow = nil;
// if you initiate a get mail command, you can't start getting mail again before the
// first CMailProgressWindow closes
Boolean	CMailProgressWindow::sGettingMail = false;

//-----------------------------------
/*static*/ CMailProgressWindow* CMailProgressWindow::CreateWindow(
	ResIDT inResID,
	MSG_Pane* inPane,
	const CStr255& inCommandName)
//-----------------------------------
{
	CMailProgressWindow* progressWindow = nil;	
	try
	{
		MSG_SetFEData(inPane, CMailCallbackManager::Get());
		progressWindow = dynamic_cast<CMailProgressWindow*>(
			URobustCreateWindow::CreateWindow(inResID, LCommander::GetTopCommander()));
		ThrowIfNULL_(progressWindow);
		progressWindow->SetWindowContext(ExtractNSContext(MSG_GetContext(inPane)));
		// the window will be shown after a decent interval.
		if (inCommandName.Length() > 0)
			progressWindow->SetDescriptor(inCommandName);
		progressWindow->ListenToPane(inPane);
	}
	catch (...)
	{
		delete progressWindow;		
		throw;
	}
	return progressWindow;
} // CMailProgressWindow::CreateWindow

//-----------------------------------
/*static*/ CMailProgressWindow* CMailProgressWindow::CreateIndependentWindow(
	ResIDT inResID,
	MSG_Pane* inPane,
	const CStr255& inCommandName,
	UInt16 inDelay)
// This is private to the class.  The pane passed in must have been made by this class.
//-----------------------------------
{
	CMailProgressWindow* progressWindow = CreateWindow(inResID, inPane, inCommandName);
	progressWindow->mPane = inPane; // This is the sign that we made it.
	progressWindow->SetDelay(inDelay);
	if (inDelay == 0)
		progressWindow->Show();
	return progressWindow;
} // CMailProgressWindow::CreateModelessWindow

//-----------------------------------
/*static*/ CMailProgressWindow* CMailProgressWindow::CreateModalParasite(
	ResIDT inResID,
	MSG_Pane* inPane,
	const CStr255& inCommandName)
//-----------------------------------
{
	sModalMailProgressWindow = CreateWindow(inResID, inPane, inCommandName);
	return sModalMailProgressWindow;
} // CMailProgressWindow::CreateModalParasite

//-----------------------------------
/*static*/ CMailProgressWindow* CMailProgressWindow::ObeyMessageLibraryCommand(
	ResIDT inResID,
	MSG_Pane* inPane,
	MSG_CommandType inCommand,
	const CStr255& inCommandName,
	MSG_ViewIndex* inSelectedIndices,
	int32 inNumIndices)
//-----------------------------------
{
	CMailProgressWindow* progressWindow = nil;	
	try
	{
		CMailNewsContext* context = new CMailNewsContext(MWContextMailNewsProgress);
		MSG_Pane* pane = MSG_CreateProgressPane(*context, CMailNewsContext::GetMailMaster(), inPane);
		ThrowIfNULL_(pane);
		progressWindow = CreateIndependentWindow(inResID, pane, inCommandName);
		progressWindow->mParentPane = inPane;

		// remember if we're getting mail or news...
		progressWindow->mCommandBeingServiced = inCommand;
		// only allowed to get mail one dialog at a time... (in other words, you can't
		// have two of these dialogs open at the same time both of them getting mail)
		if (inCommand == MSG_GetNewMail)
		{
			sGettingMail = true;
			// Note: for MSG_GetNewMail, we get the pane notification CopyFinished when
			// messages are filtered, and later we get allconnectionscomplete from the
			// context.  So avoid closing the window too early by turning off pane
			// notifications.
			progressWindow->ListenToPane(nil);

			// This is getting messy.  I think CDownloadListener should have a new built-in
			// data member, which stores the notify code which it will use to destroy the window.
			// eg, for a file copy, set this to MSG_PaneNotifyCopyFinished, etc etc.  The problem
			// now is that some operations, like GetNewMail, send one of these AS WELL AS all
			// connections complete.  Whereas some operations send only one or the other of these.
		}

		::MSG_Command(pane, inCommand, inSelectedIndices, inNumIndices);
	}
	catch (...)
	{
		delete progressWindow;		
		throw;
	}
	
	return progressWindow;
} // CMailProgressWindow::ObeyMessageLibraryCommand

//-----------------------------------
/*static*/ CMailProgressWindow* CMailProgressWindow::CleanUpFolders()
//-----------------------------------
{
	CMailProgressWindow* progressWindow = nil;	
	try
	{
		CMailNewsContext* context = new CMailNewsContext(MWContextMailNewsProgress);
		MSG_Pane* pane = MSG_CreateFolderPane(*context, CMailNewsContext::GetMailMaster());
		ThrowIfNULL_(pane);
		
		progressWindow = CreateIndependentWindow(res_ID_modeless, pane, "");
		progressWindow->mCommandBeingServiced = (MSG_CommandType)'ClnF';		
		MSG_CleanupFolders(pane);
			// Of course, that should be "CleanUpFolders"
		// Sigh.  It's asynchronous.  It works in the background.  But we may be trying to quit.
		Boolean quitting = (CFrontApp::GetApplication()->GetState() == programState_Quitting);
		if (quitting)
		{	
			do
			{
				// progress call will hide wind when done.
				// Or user will cancel (and close and delete).  This will set mPane to nil.
				CFrontApp::GetApplication()->ProcessNextEvent();
			} while (progressWindow->IsVisible() && progressWindow->mPane == pane);
			if (progressWindow->mPane == pane)
				progressWindow->DoClose(); // close it if it's not closed already
		}
		
	}
	catch (...)
	{
		delete progressWindow;		
		throw;
	}
	return progressWindow;
} // CMailProgressWindow::CleanUpFolders

//-----------------------------------
/*static*/ CMailProgressWindow* CMailProgressWindow::SynchronizeForOffline(
	const CStr255& inCommandName,
	ResIDT inResID)
//-----------------------------------
{
	CMailProgressWindow* progressWindow = nil;	
	try
	{
		// Read the prefs
		XP_Bool getNews, getMail, sendMail, getDirectories, goOffline;
		PREF_GetBoolPref("offline.download_discussions", &getNews);
		PREF_GetBoolPref("offline.download_mail", &getMail);
		PREF_GetBoolPref("offline.download_messages", &sendMail);
		PREF_GetBoolPref("offline.download_directories", &getDirectories);
		PREF_GetBoolPref("offline.offline_after_sync", &goOffline);

		// Synchronize
		CMailNewsContext* context = new CMailNewsContext(MWContextMailNewsProgress);
		MSG_Pane* pane = MSG_CreateFolderPane(*context, CMailNewsContext::GetMailMaster());
		ThrowIfNULL_(pane);
		progressWindow = CreateIndependentWindow(inResID, pane, inCommandName);
		::MSG_SynchronizeOffline(CMailNewsContext::GetMailMaster(), pane,
									getNews, getMail, sendMail, getDirectories, goOffline);
	}
	catch (...)
	{
		delete progressWindow;		
		throw;
	}
	return progressWindow;
} // CMailProgressWindow::SynchronizeForOffline

//-----------------------------------
/*static*/ CMailProgressWindow* CMailProgressWindow::ToggleOffline(
	ResIDT inResID)
//-----------------------------------
{
	CMailProgressWindow* progressWindow = nil;	
	try
	{
		CMailNewsContext* context = new CMailNewsContext(MWContextMailNewsProgress);
		MSG_Pane* pane = MSG_CreateFolderPane(*context, CMailNewsContext::GetMailMaster());
		ThrowIfNULL_(pane);
		progressWindow = CreateIndependentWindow(inResID, pane, "\p", 60);
		::MSG_GoOffline(CMailNewsContext::GetMailMaster(), pane, false, false, false, false);

		// force a menu update to toggle the menu command name
		LCommander::SetUpdateCommandStatus(true);

		// display a status msg in all windows
		LStr255 offlineString("");
		if (!UOffline::AreCurrentlyOnline())
			offlineString = LStr255(cOfflineListID, cOfflineStrIndex);
		CNetscapeWindow::DisplayStatusMessageInAllWindows(offlineString);
	}
	catch (...)
	{
		delete progressWindow;		
		throw;
	}
	return progressWindow;
} // CMailProgressWindow::ToggleOffline

//-----------------------------------
/*static*/ MSG_Pane* CMailProgressWindow::JustGiveMeAPane(
	const CStr255& inCommandName,
	MSG_Pane* inParentPane)
//-----------------------------------
{
	CMailProgressWindow* progressWindow = nil;	
	MSG_Pane* pane = nil;
	try
	{
		CMailNewsContext* context = new CMailNewsContext(MWContextMailNewsProgress);
		pane = ::MSG_CreateProgressPane(
			*context, CMailNewsContext::GetMailMaster(), inParentPane);
		ThrowIfNULL_(pane);
		progressWindow = CreateIndependentWindow(res_ID_modeless, pane, inCommandName);
	}
	catch (...)
	{
		delete progressWindow;		
		throw;
	}
	return pane;
} // CMailProgressWindow::GoOffline

//-----------------------------------
CMailProgressWindow::CMailProgressWindow(LStream* inStream)
//-----------------------------------
:	CDownloadProgressWindow(inStream)
,	mStartTime(::TickCount())
,	mCallbackListener(nil)
,	mLastCall(0)
,	mPane(nil)
,	mParentPane(nil)
,	mCancelCallback(nil)
{
	*inStream >> mDelay;

	// Don't list these windows in the window menu (unfortunately, the constructor
	// of CMediatedWindow has already added us at this point).  So tell Mr. WindowMenu
	// that we're dead, dead, dead.  Do this by calling Hide(), which has the side-effect
	// of causing a broadcast that we've disappeared.  The window is currently invisible
	// anyway.
	Hide();
	//CWindowMenu::sWindowMenu->ListenToMessage(msg_WindowDisposed, this);

	// Watch out for commands that never call us back: we won't have cleaned up
	// the previous window until now...
	delete sModalMailProgressWindow;
		// this static is reassigned in the routine that makes modal ones.
} // CMailProgressWindow::CMailProgressWindow

//-----------------------------------
CMailProgressWindow::~CMailProgressWindow()
//-----------------------------------
{
	if (sModalMailProgressWindow == this)
		sModalMailProgressWindow = nil;
		
	// if we're the window servicing the get mail request then after we're destroyed,
	// someone else can get issue a get mail command too
	if ((mCommandBeingServiced == MSG_GetNewMail) && sGettingMail)
		sGettingMail = false;
		
	delete mCallbackListener;
	mCallbackListener = nil;
	if (mPane) // ie, I made it...
	{
		// clear mPane so e won't come into this block again.
		MSG_Pane* temp = mPane;
		mPane = nil; 
		
		// Now let the back end run its course
		CNSContext* context = GetWindowContext();
		if (context)
			XP_InterruptContext((MWContext*)*context);
		::MSG_DestroyPane(temp);
		
		// When we delete a progress pane it can affect CommandStatus for commands like GetMail
		// since the window has already been activated, FindCommandStatus needs to be called
		SetUpdateCommandStatus( true );
	}
} // CMailProgressWindow::~CMailProgressWindow

//-----------------------------------
ResIDT CMailProgressWindow::GetStatusResID() const
//-----------------------------------
{
	return res_ID_modal;
} // client must provide!

//-----------------------------------
UInt16 CMailProgressWindow::GetValidStatusVersion() const
//-----------------------------------
{
	return 0x0001;
} // CDownloadProgressWindow::GetValidStatusVersion

//-----------------------------------
void CMailProgressWindow::FinishCreateSelf()
//-----------------------------------
{
	Inherited::FinishCreateSelf();
	if (HasAttribute(windAttr_Modal))
	{
		Assert_(!sModalMailProgressWindow);
		sModalMailProgressWindow = this;
	}
	StartIdling();
} // CMailProgressWindow::FinishCreateSelf

//-----------------------------------
void CMailProgressWindow::ListenToPane(MSG_Pane* inPane)
//-----------------------------------
{
	if (!mCallbackListener)
		mCallbackListener = new CDownloadListener(this, inPane);
	else 
		mCallbackListener->SetPane(inPane);
}

//-----------------------------------
void CMailProgressWindow::SpendTime(const EventRecord&)
//-----------------------------------
{
	UInt32 time = ::TickCount();
	// First time, set the time called.
	if (mLastCall == 0)
	{
		mLastCall = time;
		return;
	}
	// If this is not the first call, but we're not getting tickled, close.
	if (time > mStartTime + mDelay * 2 && !IsVisible())
	{
		// prevent re-entrancy (DoClose can take time, because of callbacks
		// from interrupt context.)	
		StopRepeating();
		DoClose();
		return;
	}
	// Normal case.  Make sure the barber-pole spins.
	if (mBar->GetValue() == CProgressBar::eIndefinite)
		mBar->Refresh();
} // CMailProgressWindow::SpendTime

//-----------------------------------
void CMailProgressWindow::Show()
//-----------------------------------
{
	// Keep it out of the window menu!
	StSetBroadcasting saver(CWindowMediator::GetWindowMediator(), false);
	Inherited::Show();
}

//-----------------------------------
void CMailProgressWindow::ListenToMessage(
	MessageT			inMessage,
	void*				ioParam)
//-----------------------------------
{
	const UInt32 kMinTimeBetweenUpdates = 20;
	UInt32 time = ::TickCount();
	if (!IsVisible() // ooh, it's been comfy here, invisible, but maybe there's work...
		&& inMessage != msg_NSCAllConnectionsComplete // heck, I'm not showing up for this...
		&& time > mStartTime + mDelay) // I only show up for LONG processes.
		Show();
	mLastCall = time;
	switch (inMessage)
	{
		case msg_NSCProgressUpdate:
			// Ignore that message in mail/news windows (it's ignored on
			// Windows too). Otherwise, the progress bar alternates between
			// a percent value and a barber pole.
			return;

		case msg_NSCAllConnectionsComplete:
			if (mParentPane)
			{
				// send a fake progress-done call to parent pane.  This helps with "get new mail"
				// because the inbox has to scroll to display it.
				FE_PaneChanged(
					mParentPane,
					PR_TRUE, // async
					MSG_PaneProgressDone,
					msg_NSCAllConnectionsComplete);
			}
			Hide(); // So that on idle we'll close.
					// This avoids the base class behavior (calling DoClose()) immediately,
					// and instead waits till idle time to close.
			mDelay = 0 ; // Don't want to wait a long time to destroy the window
			StopListening(); // Don't show ourselves on subsequent messages.
			ListenToPane(nil);
			return;
		case msg_Cancel:
			if (mCancelCallback && !mCancelCallback()) // result true means "execute default"
				return;
			break;
	}
	Inherited::ListenToMessage(inMessage, ioParam);
} // CMailProgressWindow::ListenToMessage

//-----------------------------------
void CMailProgressWindow::NoteProgressBegin(const CContextProgress& inProgress)
//-----------------------------------
{
	// Base class will clobber the window title with a null string.  Don't do that!
	if (inProgress.mAction.length())
		SetDescriptor(CStr255(inProgress.mAction));
	mMessage->SetDescriptor(inProgress.mMessage);
	mComment->SetDescriptor(inProgress.mComment);
} // CMailProgressWindow::NoteProgressBegin
