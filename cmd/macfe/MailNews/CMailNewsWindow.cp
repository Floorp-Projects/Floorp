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



// CMailNewsWindow.cp

#include "CMailNewsWindow.h"

#include "MailNewsgroupWindow_Defines.h"
#include "CGrayBevelView.h"
#include "CProgressListener.h"
#include "CMailNewsContext.h"
#include "LTableViewHeader.h"
#include "CMailFlexTable.h"
#include "CMessageFolderView.h"
#include "CPrefsDialog.h"
#include "CPatternButton.h"
#include "URobustCreateWindow.h"
#include "CSpinningN.h"
#include "CProxyPane.h"
#include "CDragBarContainer.h"
#include "UDeferredTask.h"
#include "CWindowMenu.h"

#include "resgui.h"
#include "macutil.h"
// helper function used by CThreadWindow and CMessageWindow to set the default action of 
// the folder button.
#include "prefapi.h"



#pragma mark -

//======================================
// class CMailNewsWindow
//======================================

const char* Pref_MailShowToolbar = "mailnews.chrome.show_toolbar";
const char* Pref_MailShowLocationBar = "mailnews.chrome.show_url_bar";

//----------------------------------------------------------------------------------------
CMailNewsWindow::CMailNewsWindow(LStream *inStream, DataIDT inWindowType)
//----------------------------------------------------------------------------------------
:	Inherited(inStream, inWindowType)
,	CSaveWindowStatus(this)
,	mProgressListener(NULL)
,	mMailNewsContext(NULL)
{
	mToolbarShown[LOCATION_TOOLBAR] = false; // set to true in base class
}

//----------------------------------------------------------------------------------------
void CMailNewsWindow::DoDefaultPrefs()
//----------------------------------------------------------------------------------------
{
	CPrefsDialog::EditPrefs(CPrefsDialog::eExpandMailNews);
}


//----------------------------------------------------------------------------------------
CMailNewsWindow::~CMailNewsWindow()
//----------------------------------------------------------------------------------------
{
	if (mMailNewsContext)
		mMailNewsContext->RemoveUser(this);
	delete mProgressListener;
}

//----------------------------------------------------------------------------------------
const char* CMailNewsWindow::GetLocationBarPrefName() const
//----------------------------------------------------------------------------------------
{
	return Pref_MailShowLocationBar;
}

//----------------------------------------------------------------------------------------
void CMailNewsWindow::ReadGlobalDragbarStatus()
	// Unfortunately, the visibility of the drag bars is determined in several ways, all
	// of which must be kept in synch.  These ways include, but are not limited to,
	// the PPob resource (through the visible flag), the saved window status (through
	// Read/WriteWindowStatus and Save/RestorePlace etc) and the preferences, which are
	// used to save the values of mToolbarShown.
//----------------------------------------------------------------------------------------
{
	XP_Bool	value;
	if (PREF_GetBoolPref(Pref_MailShowToolbar, &value) == PREF_NOERROR)
		mToolbarShown[MESSAGE_TOOLBAR] = value;
	if (PREF_GetBoolPref(GetLocationBarPrefName(), &value) == PREF_NOERROR)
		mToolbarShown[LOCATION_TOOLBAR] = value;
} // CMailNewsWindow::ReadGlobalDragbarStatus

//----------------------------------------------------------------------------------------
void CMailNewsWindow::WriteGlobalDragbarStatus()
//----------------------------------------------------------------------------------------
{
	PREF_SetBoolPref(Pref_MailShowToolbar, mToolbarShown[MESSAGE_TOOLBAR]);
	PREF_SetBoolPref(GetLocationBarPrefName(), mToolbarShown[LOCATION_TOOLBAR]);
} // CMailNewsWindow::WriteGlobalDragbarStatus

//----------------------------------------------------------------------------------------
void CMailNewsWindow::FinishCreateSelf()
//----------------------------------------------------------------------------------------
{
	Inherited::FinishCreateSelf();
	SetAttribute(windAttr_DelaySelect);
		// should be in resource, but there's a resource freeze.
	try
	{
		//================================================================================
		// WARNING!  WARNING!  WARNING!
		// CThreadWindow::FinishCreateSelf() does not call CMailNewsWindow::FinishCreateSelf.
		//  So if you add any new stuff here, you have to add it there, too.
		//================================================================================

		ReadGlobalDragbarStatus();

		// Let there be a mail-news context
		mMailNewsContext = CreateContext();
		StSharer theLock(mMailNewsContext);
		// Let there be a progress listener, placed in my firmament,
		// which shall listen to the mail-news context
		if (mProgressListener)
			mMailNewsContext->AddListener(mProgressListener);
		else
			mProgressListener = new CProgressListener(this, mMailNewsContext);
		ThrowIfNULL_(mProgressListener);
		// The progress listener should be "just a bit" lazy during network activity
		// and "not at all" at idle time to display the URLs pointed by the mouse cursor.
		mProgressListener->SetLaziness(CProgressListener::lazy_NotAtAll);
		mMailNewsContext->AddListener(mProgressListener);
		mMailNewsContext->AddUser(this);
		CMailFlexTable* table = GetActiveTable();
		if (table)
		{
			SetLatentSub(table);
			mMailNewsContext->AddListener(table); // listen for all connections complete.		
		}
		CSpinningN* theN = dynamic_cast<CSpinningN*>(FindPaneByID(CSpinningN::class_ID));
		if (theN)
			mMailNewsContext->AddListener(theN);
	}
	catch (...) {
		throw;
	}
	// And behold, he saw that it was good.
	CSaveWindowStatus::FinishCreateWindow();
} // CMailNewsWindow::FinishCreateSelf

//----------------------------------------------------------------------------------------
CNSContext* CMailNewsWindow::CreateContext() const
//----------------------------------------------------------------------------------------
{
	CMailNewsContext* result = new CMailNewsContext();
	FailNIL_(result);
	return result;
} //CMailNewsWindow::CreateContext

//----------------------------------------------------------------------------------------
void CMailNewsWindow::AboutToClose()
//----------------------------------------------------------------------------------------
{
	//================================================================================
	// WARNING!  WARNING!  WARNING!
	// CThreadWindow::AboutToClose() does not call CMailNewsWindow::AboutToClose.
	//  So if you add any new stuff here, you have to add it there, too.
	//================================================================================
	CSaveWindowStatus::AttemptCloseWindow(); // Do this first: uses table
	WriteGlobalDragbarStatus();
	// Bug fix: must delete the pane before killing the context, because
	// the destructor of the pane references the context when it cleans up.
	CMailFlexTable* t = GetActiveTable();
	if (t)
	{
		if (mMailNewsContext)
			mMailNewsContext->RemoveListener(t); // bad time to listen for all connections complete.		
		delete t;
	}
} // CMailNewsWindow::AboutToClose

//----------------------------------------------------------------------------------------
void CMailNewsWindow::AttemptClose()
//----------------------------------------------------------------------------------------
{
	CDeferredCloseTask::DeferredClose(this);
}

//----------------------------------------------------------------------------------------
void CMailNewsWindow::DoClose()
//----------------------------------------------------------------------------------------
{
	AboutToClose();
	Inherited::DoClose();
}

//----------------------------------------------------------------------------------------
Boolean CMailNewsWindow::AttemptQuitSelf(Int32 /* inSaveOption */)
// Derived classes should be careful to call DeferredClose if they override this fn.
//----------------------------------------------------------------------------------------
{
	CDeferredCloseTask::DeferredClose(this);
	return true;
}

//----------------------------------------------------------------------------------------
void CMailNewsWindow::ReadWindowStatus(LStream *inStatusData)
//----------------------------------------------------------------------------------------
{
	CSaveWindowStatus::ReadWindowStatus(inStatusData);
	CDragBarContainer* dragContainer = (CDragBarContainer*)FindPaneByID('DbCt');
	if (dragContainer && inStatusData)
		dragContainer->RestorePlace(inStatusData);
	
	// CThreadWindow does this now
	//CMailFlexTable* table = GetActiveTable();
	//if (table && inStatusData)
	//	table->GetTableHeader()->ReadColumnState(inStatusData);
}

//----------------------------------------------------------------------------------------
void CMailNewsWindow::WriteWindowStatus(LStream *outStatusData)
//----------------------------------------------------------------------------------------
{
	CSaveWindowStatus::WriteWindowStatus(outStatusData);
	CDragBarContainer* dragContainer = (CDragBarContainer*)FindPaneByID('DbCt');
	if (dragContainer && outStatusData)
		dragContainer->SavePlace(outStatusData);
	
	// CThreadWindow does this now
	//CMailFlexTable* table = GetActiveTable();
	//if (table && outStatusData)
	//	table->GetTableHeader()->WriteColumnState(outStatusData);
}

//----------------------------------------------------------------------------------------
void CMailNewsWindow::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
//----------------------------------------------------------------------------------------
{
	outUsesMark = false;
	
	switch (inCommand) {

		case cmd_ToggleToolbar:
			outEnabled = true;
			if (mToolbarShown[MESSAGE_TOOLBAR])
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, HIDE_MESSAGE_TOOLBAR_STRING);
			else
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, SHOW_MESSAGE_TOOLBAR_STRING);
			break;
		
		case cmd_ToggleLocationBar:
			outEnabled = true;
			if (mToolbarShown[LOCATION_TOOLBAR])
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, HIDE_LOCATION_TOOLBAR_STRING);
			else
				::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, SHOW_LOCATION_TOOLBAR_STRING);
			break;
		case cmd_ShowLocationBar:
			outEnabled = !mToolbarShown[LOCATION_TOOLBAR];
			break;
		case cmd_HideLocationBar:
			outEnabled = mToolbarShown[LOCATION_TOOLBAR];
			break;
		default:
			CNetscapeWindow::FindCommandStatus(
				inCommand, outEnabled, outUsesMark, outMark, outName);
	}
} // CMailNewsWindow::FindCommandStatus

//----------------------------------------------------------------------------------------
Boolean	CMailNewsWindow::ObeyCommand(
	CommandT			inCommand,
	void				*ioParam)
//----------------------------------------------------------------------------------------
{
	Boolean cmdHandled = false;
	switch (inCommand) {
	
		case cmd_ToggleToolbar:
			ToggleDragBar(cMessageToolbar, MESSAGE_TOOLBAR, Pref_MailShowToolbar);
			cmdHandled = true;
			break;
			
		case cmd_ShowLocationBar:
		case cmd_HideLocationBar:
			if (mToolbarShown[LOCATION_TOOLBAR] != (inCommand == cmd_ShowLocationBar))
				ToggleDragBar(cMailNewsLocationToolbar, LOCATION_TOOLBAR, GetLocationBarPrefName());
			cmdHandled = true;
			break;

		case cmd_ToggleLocationBar:
			ToggleDragBar(cMailNewsLocationToolbar, LOCATION_TOOLBAR, GetLocationBarPrefName());
			cmdHandled = true;
			break;
			
		default:
			cmdHandled = CNetscapeWindow::ObeyCommand(inCommand, ioParam);
		
	}
	
	return cmdHandled;
} // CMailNewsWindow::ObeyCommand

#pragma mark -

//======================================
// class CMailNewsFolderWindow
//======================================

//----------------------------------------------------------------------------------------
/* static */ CMailNewsFolderWindow* CMailNewsFolderWindow::FindAndShow(
	Boolean inMakeNew,
	CommandT inCommand)
// Handle the menu command that creates/shows/selects the MailNews window.
// Currently there can only be one of these.
//----------------------------------------------------------------------------------------
{
	CMailNewsFolderWindow* result = NULL;
	try
	{
		CMailNewsContext::ThrowUnlessPrefsSet(MWContextMail);
		if (inCommand == cmd_NewsGroups)
			CMailNewsContext::ThrowUnlessPrefsSet(MWContextNews);
		CWindowIterator iter(WindowType_MailNews);
		iter.Next(result);
		if (!result && inMakeNew)
		{
			result = dynamic_cast<CMailNewsFolderWindow*>(
				URobustCreateWindow::CreateWindow(res_ID, LCommander::GetTopCommander())
				);
			ThrowIfNULL_(result);
		}
		if (result)
		{
			result->Show();
			result->Select();
		}
		if (inCommand)
		{
			CMessageFolderView* folderView = (CMessageFolderView*)result->GetActiveTable();
			if (folderView)
			{
				switch (inCommand)
				{
					case cmd_NewsGroups: 			// Select first news host
					case cmd_MailNewsFolderWindow:	// Select first mail host
						folderView->SelectFirstFolderWithFlags(
							inCommand == cmd_NewsGroups
								? MSG_FOLDER_FLAG_NEWS_HOST
								: MSG_FOLDER_FLAG_MAIL);
						break;
					default:
						folderView->ObeyCommand(inCommand, nil);
				}
			}
		}
	}
	catch( ... )
	{
	}
	return result;
}

//----------------------------------------------------------------------------------------
CMailNewsFolderWindow::CMailNewsFolderWindow(LStream *inStream)
//----------------------------------------------------------------------------------------
:	CMailNewsWindow(inStream, WindowType_MailNews)
{
}

//----------------------------------------------------------------------------------------
CMailNewsFolderWindow::~CMailNewsFolderWindow()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CMailNewsFolderWindow::FinishCreateSelf()
//----------------------------------------------------------------------------------------
{
	Inherited::FinishCreateSelf();
	CMessageFolderView* list = (CMessageFolderView*)GetActiveTable();
	Assert_(list);
	LCaption* locationCaption = (LCaption*)FindPaneByID('LCap');
	if (locationCaption && list)
	{
		CStr255 tempString;
		list->GetLongWindowDescription(tempString);
		locationCaption->SetDescriptor(tempString);
		CProxyPane* proxy = dynamic_cast<CProxyPane*>(FindPaneByID(CProxyPane::class_ID));
		if (proxy)
		{
			proxy->ListenToMessage(msg_NSCDocTitleChanged, (char*)tempString);
			proxy->SetIconIDs(15393, 15393);
		}
	}
	list->LoadFolderList(mMailNewsContext);
	UReanimator::LinkListenerToControls((CMailFlexTable*)list, this, res_ID);
} // CMailNewsWindow::FinishCreateSelf

//----------------------------------------------------------------------------------------
void CMailNewsFolderWindow::CalcStandardBoundsForScreen(
		const Rect &inScreenBounds,
		Rect &outStdBounds) const
//----------------------------------------------------------------------------------------
{
	LWindow::CalcStandardBoundsForScreen(inScreenBounds, outStdBounds);
	Rect	contRect = UWindows::GetWindowContentRect(mMacWindowP);

	outStdBounds.left = contRect.left;
	outStdBounds.right = contRect.right;
}

//----------------------------------------------------------------------------------------
UInt16 CMailNewsFolderWindow::GetValidStatusVersion(void) const
//----------------------------------------------------------------------------------------
{
	return 0x0113;
}

//----------------------------------------------------------------------------------------
CMailFlexTable* CMailNewsFolderWindow::GetActiveTable()
// Get the currently active table in the window. The active table is the table in
// the window that the user considers to be receiving input.
//----------------------------------------------------------------------------------------
{
	return dynamic_cast<CMessageFolderView*>(FindPaneByID('Flst'));
}
