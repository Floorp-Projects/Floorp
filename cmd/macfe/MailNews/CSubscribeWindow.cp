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



// CSubscribeWindow.cp

#include "CSubscribeWindow.h"
#include "CSubscribeView.h"
#include "CMailNewsContext.h"
#include "CNewsSubscriber.h"
#include "CProgressListener.h"
#include "CPaneEnabler.h"
#include "CTabControl.h"
#include "URobustCreateWindow.h"
#include "UMenuUtils.h"
#include "UMailFolderMenus.h"

#include <LGAPushButton.h>
#include <LGAPopup.h>
#include <UModalDialogs.h>
#include "CLargeEditField.h"
#include "macgui.h"
//#include "newshost.h"
#include "miconutils.h"
#include "MercutioAPI.h"

extern "C" {
	#include "asyncCursors.h"
}

// Command Numbers
#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"


/*

Overview:
---------

	CSubscribeWindow:
		This window displays the list of newsgroups available on the
		different news servers and lets you select the newsgroups you
		want to subscribe to. Technically speaking, the CSubscribeWindow
		handles the interactions between a CSubscribeView and a small bunch
		of buttons and edit fields, plus sother elements listed below.

	CSubscribeHostPopup:
		Popup menu to select the news server you want to connect to.

	CNewsHostIterator:
		Simple iterator to access the list of news servers.
		Used by the CSubscribeHostPopup.


Hints:
------
	The newsgroup list (a CSubscribeView) should not be disposed when switching
	panels in order to keep its state (ie. its Context) and not reload it
	everytime from the cache file or from the server. The list is originally
	attached to the window and when a panel is activated, a CPlaceHolderView
	puts it inside the panel. The list is linked where it belongs in the commander
	chain after the CTabControl broadcasts the activation of a tab panel.

	We have a similar problem with the CSubscribeHostPopup: it must keep its
	state when switching panels. This is done by storing the last selected
	host in a static variable of the CNewsSubscriber (which has been delegated
	this role because it only contains static stuff). The same static variable 
	is used when bringing up the CSubscribeWindow in order to display the list
	corresponding to the host currently selected in the Message Center.

	More info about the list in "CSubscribeView.cp".

History:
--------
	November 97:
	  Converted BE calls to the new API in order to support IMAP public folders.
	  Added Mercutio callback to display imap/news server icons in the popup menu.

	March 98:
	  Disabled "Search" and "New" panels when an IMAP host is selected. It required to
	  put the host popup menu in a CPlaceHolderView too in order to avoid deleting
	  and recreating it when a switch from a News host to an IMAP host causes
	  the tab panel to automatically display "All Groups" (it's bad to delete a
	  control when it's broadcasting).
*/


static const ResIDT resID_SubscribeTab1			= 14002;	// PPob resources
static const ResIDT resID_SubscribeTab2			= 14003;
static const ResIDT resID_SubscribeTab3			= 14004;
static const ResIDT resID_SubscribeWindow 		= 14006;

static const PaneIDT paneID_OkButton			= 'BtOk';	// generic buttons
static const PaneIDT paneID_CancelButton		= 'Canc';
static const PaneIDT paneID_EditField			= 'NuEd';
static const PaneIDT paneID_FolderList			= 'Slst';
static const PaneIDT paneID_FolderView			= 'SbVw';
static const PaneIDT paneID_TabSwitcher			= 'SbTb';
static const PaneIDT paneID_TabControl			= 'TabC';
static const PaneIDT paneID_SubscribeButton		= 'Subs';
static const PaneIDT paneID_UnsubscribeButton	= 'UnSb';
static const PaneIDT paneID_ServerPopup			= 'SPop';
static const PaneIDT paneID_StopButton			= 'Stop';

static const PaneIDT paneID_AllTabPanel			= 'Sub1';	// tab #1 buttons
static const PaneIDT paneID_ExpandButton		= 'Expn';
static const PaneIDT paneID_CollapseButton		= 'Coll';
static const PaneIDT paneID_GetGroupsButton		= 'GetG';
static const PaneIDT paneID_AddServerButton		= 'AddS';

static const PaneIDT paneID_SearchTabPanel		= 'Sub2';	// tab #2 buttons
static const PaneIDT paneID_SearchButton		= 'Srch';

static const PaneIDT paneID_NewTabPanel			= 'Sub3';	// tab #3 buttons
static const PaneIDT paneID_GetNewButton		= 'GetN';
static const PaneIDT paneID_ClearNewButton		= 'ClrN';


//----------------------------------------------------------------------------
//	USubscribeUtilities
//
//----------------------------------------------------------------------------

//#define REGISTER_(letter,root) \
//	RegisterClass_(letter##root::class_ID, \
//		(ClassCreatorFunc)letter##root::Create##root##Stream);
	
#define REGISTER_(letter,root) \
	RegisterClass_(letter##root);
	
#define REGISTERC(root) REGISTER_(C,root)
#define REGISTERL(root) REGISTER_(L,root)


void USubscribeUtilities::RegisterSubscribeClasses()
{
	REGISTERC(SubscribeWindow)
	REGISTERC(SubscribeHostPopup)
	REGISTERC(TabContainer)		// reusing CTabContainer from CMailComposeWindow.cp
}


#pragma mark -

//----------------------------------------------------------------------------
//	CNewsHostIterator
//
//----------------------------------------------------------------------------
MSG_Host*	CNewsHostIterator::mHostList[CNewsHostIterator::kMaxNewsHosts];
Int32		CNewsHostIterator::mHostCount = 0;

CNewsHostIterator::CNewsHostIterator(MSG_Master* master)
{
	mHostCount = MSG_GetSubscribingHosts(master,
					&mHostList[0],
						CNewsHostIterator::kMaxNewsHosts);
	mIndex = 0;
}

	
Boolean CNewsHostIterator::Current(MSG_Host*& outNewsHost)
{
	outNewsHost = nil;
	if (mIndex < mHostCount)
		outNewsHost = mHostList[mIndex];
	return (outNewsHost != nil);	
}

	
Boolean CNewsHostIterator::Next(MSG_Host*& outNewsHost)
{
	Boolean	valid = Current(outNewsHost);
	ResetTo(GetCurrentIndex() + 1);
	return valid;	
}

	
Boolean CNewsHostIterator::FindIndex(const MSG_Host* inNewsHost, Int32& outIndex)
{
	Int32 index;
	for (index = 0; index < mHostCount; index ++)
	{
		if (inNewsHost == mHostList[index])
		{
			outIndex = index;
			return true;
		}
	}
	outIndex = 0;
	return false;
}


#pragma mark -


//----------------------------------------------------------------------------
//	FindAndShow			[static]
//		Handle the menu command that creates/shows/selects the MailNews window.
//		Currently there can only be one of these.
//----------------------------------------------------------------------------

CSubscribeWindow* CSubscribeWindow::FindAndShow(Boolean makeNew)
{
	CSubscribeWindow* result = NULL;
	try
	{
		CWindowIterator iter(WindowType_SubscribeNews);
		iter.Next(result);
		if (!result && makeNew)
		{
			result = dynamic_cast<CSubscribeWindow*>
						(URobustCreateWindow::CreateWindow(
							res_ID, LCommander::GetTopCommander()));
			ThrowIfNULL_(result);
		}
		if (result)
		{
			result->Show();
			result->Select();
		}
	}
	catch( ... )
	{
	}
	return result;
}


//------------------------------------------------------------------------------
//	DisplayDialog		[static]
//------------------------------------------------------------------------------

Boolean CSubscribeWindow::DisplayDialog()
{
	StDialogHandler handler(res_ID, NULL);

	MessageT message;
	do
	{
		message = handler.DoDialog();
	} while (message != paneID_OkButton && message != paneID_CancelButton);
	
	return (message == paneID_OkButton);
}


//----------------------------------------------------------------------------
//	CSubscribeWindow Constructor / Destructor
//
//----------------------------------------------------------------------------

CSubscribeWindow::CSubscribeWindow(LStream* inStream) :
	CMailNewsWindow(inStream, WindowType_SubscribeNews),
	mList(nil)
{
}

CSubscribeWindow::~CSubscribeWindow()
{
	stopAsyncCursors();
}


//----------------------------------------------------------------------------
//	FinishCreateSelf
//
//----------------------------------------------------------------------------

void CSubscribeWindow::FinishCreateSelf()
{
	// Finish create context and other things
	Inherited::FinishCreateSelf();

	mProgressListener->SetLaziness(CProgressListener::lazy_JustABit);

	mList = dynamic_cast<CSubscribeView*>(GetActiveTable());
	if (mList) mList->SetContext((CMailNewsContext*)this->GetWindowContext());

	// Hook up listeners
	UReanimator::LinkListenerToControls(this, this, GetPaneID());

	CTabControl* tabControl = dynamic_cast<CTabControl*>(FindPaneByID(paneID_TabControl));
	if (tabControl) tabControl->AddListener(this);

	// Initialize dialog to display first tab (hack because the CTabControl has
	// already broadcasted its value message but nobody was listening at that time)
	long msg = resID_SubscribeTab1;
	ListenToMessage(msg_TabSwitched, &msg);

	// "Search" and "New" are not supported for IMAP
	if (MSG_IsIMAPHost(CNewsSubscriber::GetHost()))
	{
		CTabControl* tabControl = dynamic_cast<CTabControl*>(FindPaneByID(paneID_TabControl));
		Assert_(tabControl);
		tabControl->SetTabEnable(resID_SubscribeTab2, false);
		tabControl->SetTabEnable(resID_SubscribeTab3, false);
	}
}


//----------------------------------------------------------------------------
//	DoClose
//
//----------------------------------------------------------------------------

void CSubscribeWindow::DoClose()
{
	// Update the Mail Folder popup menu
	CMailFolderMixin::UpdateMailFolderMixins();

	// Update the Message Center
	CMailNewsFolderWindow* messageCenter = NULL;
	CWindowIterator iter(WindowType_MailNews);
	iter.Next(messageCenter);
	if (messageCenter)
	{
		messageCenter->Refresh();
	}

	// Close window
	Inherited::DoClose();
}


//----------------------------------------------------------------------------
//	GetActiveTable
//
//		Get the currently active table in the window. The active table is 
//		the table in the window that the user considers to be receiving input.
//----------------------------------------------------------------------------

CMailFlexTable* CSubscribeWindow::GetActiveTable()
{
	CMailFlexTable* list = dynamic_cast<CMailFlexTable*>
								(FindPaneByID(paneID_FolderList));
	Assert_(list);
	return list;
}


//----------------------------------------------------------------------------
//	CalcStandardBoundsForScreen
//
//		Zoom in the vertical direction only. 
//----------------------------------------------------------------------------

void	CSubscribeWindow::CalcStandardBoundsForScreen(
		const Rect &inScreenBounds,
		Rect &outStdBounds) const
{
	LWindow::CalcStandardBoundsForScreen(inScreenBounds, outStdBounds);
	Rect	contRect = UWindows::GetWindowContentRect(mMacWindowP);

	outStdBounds.left = contRect.left;
	outStdBounds.right = contRect.right;
}


//----------------------------------------------------------------------------
//	CommandDelegatesToSubscribeList
//
//----------------------------------------------------------------------------

Boolean CSubscribeWindow::CommandDelegatesToSubscribeList(CommandT inCommand)
{
	switch(inCommand)
	{
		case cmd_Stop:

		case cmd_OpenNewsHost:
		case cmd_NewsHostChanged:

		case cmd_NewsToggleSubscribe:
		case cmd_NewsSetSubscribe:
		case cmd_NewsClearSubscribe:
		case cmd_NewsExpandGroup:
		case cmd_NewsExpandAll:
		case cmd_NewsCollapseGroup:
		case cmd_NewsCollapseAll:
		case cmd_NewsGetGroups:
		case cmd_NewsSearch:
		case cmd_NewsGetNew:
		case cmd_NewsClearNew:
			return true;
	}
	return false;
}


//----------------------------------------------------------------------------
//	FindCommandStatus
//
//		Pass down commands to the list (ie. the CSubscribeView) when another
//		object (ie. the edit field) is the target
//----------------------------------------------------------------------------

void CSubscribeWindow::FindCommandStatus(
			CommandT		inCommand,
			Boolean			&outEnabled,
			Boolean			&outUsesMark,
			Char16			&outMark,
			Str255			outName)
{
	if (mList == nil)
	{
		outEnabled = (inCommand == paneID_CancelButton);
	}
	else
	{
		if (CommandDelegatesToSubscribeList(inCommand))
		{
			mList->FindCommandStatus(inCommand, outEnabled, outUsesMark,
													outMark,	outName);
		}
		else
		{
			CMediatedWindow::FindCommandStatus(inCommand, outEnabled, outUsesMark,
															outMark,	outName);
		}
	}
}


//----------------------------------------------------------------------------
//	ObeyCommand
//
//----------------------------------------------------------------------------

Boolean CSubscribeWindow::ObeyCommand(
	CommandT			inCommand,
	void				*ioParam)
{
#pragma unused (ioParam)
	if (inCommand == cmd_Stop)
	{
		if (mList)
		{
			// Re-enable main controls in case we were committing
			mList->Enable();
			LButton * btn;
			btn = (LButton *)FindPaneByID(paneID_CancelButton);
			if (btn) btn->Enable();
			btn = (LButton *)FindPaneByID(paneID_OkButton);
			if (btn) btn->Enable();
		}
	}
	return false;
}


//----------------------------------------------------------------------------
//	ListenToMessage
//
//----------------------------------------------------------------------------

void CSubscribeWindow::ListenToMessage(MessageT inMessage, void* ioParam)
{
#pragma unused (ioParam)
	switch (inMessage)
	{
		// Selecting another host
		case cmd_NewsHostChanged:
			if (mList)
			{
				mList->ObeyCommand(cmd_Stop, NULL);

				MSG_Host* newsHost = (MSG_Host*)(ioParam);
				MSG_SubscribeMode subscribeMode = MSG_SubscribeGetMode(mList->GetMessagePane());
				CTabControl* tabControl = dynamic_cast<CTabControl*>(FindPaneByID(paneID_TabControl));
				Assert_(tabControl);

				if (MSG_IsIMAPHost(newsHost))
				{
					// "Search" and "New" are not supported for IMAP...
					tabControl->SetTabEnable(resID_SubscribeTab2, false);
					tabControl->SetTabEnable(resID_SubscribeTab3, false);
					// ... so switch back to "All"
					if (tabControl->GetValue() != 1)
						tabControl->SetValue(1);		// will refresh the list too
					else
						mList->RefreshList(newsHost, subscribeMode);
				}
				else
				{
					tabControl->SetTabEnable(resID_SubscribeTab2, true);
					tabControl->SetTabEnable(resID_SubscribeTab3, true);
					mList->RefreshList(newsHost, subscribeMode);
				}
			}
			break;

		// Switching tabs
		case msg_TabSwitched:
			long paneID = *(long*)ioParam;
			switch (paneID)
			{
				case resID_SubscribeTab1: HandleAllGroupsTabActivate();		break;
				case resID_SubscribeTab2: HandleSearchGroupsTabActivate();	break;
				case resID_SubscribeTab3: HandleNewGroupsTabActivate();		break;
			}
			UReanimator::LinkListenerToControls(mList, this, paneID);
			break;

		// Search: paneID_EditField or paneID_SearchButton
		case msg_EditField2:
		case cmd_NewsSearch:
			HandleSearchInList();
			break;

		// Add Server button: paneID_AddServerButton
		case cmd_OpenNewsHost:
			if (CNewsSubscriber::DoAddNewsHost())
			{
				CSubscribeHostPopup* popup = dynamic_cast<CSubscribeHostPopup*>
												(FindPaneByID(paneID_ServerPopup));
				Assert_(popup);
				if (popup) popup->ReloadMenu();
			}
			break;

		// OK/Cancel buttons
		case paneID_CancelButton:
			if (mList)
				mList->SubscribeCancel();
			else
				DoClose();
			break;

		case paneID_OkButton:
			if (mList)
			{
				mList->Disable();
				LButton * btn;
				btn = (LButton *)FindPaneByID(paneID_CancelButton);
				if (btn) btn->Disable();
				btn = (LButton *)FindPaneByID(paneID_OkButton);
				if (btn) btn->Disable();

				startAsyncCursors();
				mList->SubscribeCommit();
			}
			else
				DoClose();
			break;
	}
}


//----------------------------------------------------------------------------
//	HandleKeyPress
//
//		Handle Escape and Cmd-Period (copied from LDialogBox)
//		Handle Enter and Return in the edit fields
//----------------------------------------------------------------------------

Boolean CSubscribeWindow::HandleKeyPress(
	const EventRecord	&inKeyEvent)
{
	Boolean		keyHandled	= false;
	PaneIDT		keyButtonID	= 0;
	
	switch (inKeyEvent.message & charCodeMask)
	{
		case char_Enter:
		case char_Return:
			if (mList == nil)
				return false;
			switch (MSG_SubscribeGetMode(mList->GetMessagePane()))
			{
				case MSG_SubscribeAll:
					mList->OpenSelection();
					keyHandled = true; 
					break;

				case MSG_SubscribeSearch:
					keyButtonID = paneID_SearchButton;
					break;
			}
			break;

		case char_Escape:
			if ((inKeyEvent.message & keyCodeMask) == vkey_Escape)
				keyButtonID = paneID_CancelButton;
			break;

		default:
			if (UKeyFilters::IsCmdPeriod(inKeyEvent))
				keyButtonID = paneID_CancelButton;
			else
				keyHandled = LWindow::HandleKeyPress(inKeyEvent);
			break;
	}
			
	if (keyButtonID != 0)
	{
		LControl* keyButton = (LControl*)FindPaneByID(keyButtonID);
		keyButton->SimulateHotSpotClick(kControlButtonPart);
		keyHandled = true;
	}
	
	return keyHandled;
}


//----------------------------------------------------------------------------
//	HandleAllGroupsTabActivate
//
//----------------------------------------------------------------------------

void CSubscribeWindow::HandleAllGroupsTabActivate()
{
	if (! mList)
		return;

	LControl* addServerBtn = dynamic_cast<LControl*>(FindPaneByID(paneID_AddServerButton));
	if (addServerBtn) addServerBtn->AddListener(this);

	CSubscribeHostPopup* popup = dynamic_cast<CSubscribeHostPopup*>(FindPaneByID(paneID_ServerPopup));
	if (popup) popup->AddListener(this);

	CLargeEditFieldBroadcast* groupEdit = dynamic_cast<CLargeEditFieldBroadcast*>
									(FindPaneByID(paneID_EditField));
	if (groupEdit)
	{
		groupEdit->AddListener(this);

		// put the list in my LTabGroup...
		mList->SetSuperCommander(groupEdit->GetSuperCommander());

		// ... but make me the target
		SwitchTarget(groupEdit);
		LCommander::SetUpdateCommandStatus(true);
	}

	// refresh the list
	mList->RefreshList(CNewsSubscriber::GetHost(), MSG_SubscribeAll);
}


//----------------------------------------------------------------------------
//	HandleSearchGroupsTabActivate
//
//----------------------------------------------------------------------------

void CSubscribeWindow::HandleSearchGroupsTabActivate()
{
	if (! mList)
		return;

	LControl* searchBtn = dynamic_cast<LControl*>(FindPaneByID(paneID_SearchButton));
	if (searchBtn) searchBtn->AddListener(this);

	CSubscribeHostPopup* popup = dynamic_cast<CSubscribeHostPopup*>(FindPaneByID(paneID_ServerPopup));
	if (popup) popup->AddListener(this);

	LEditField* searchEdit = dynamic_cast<LEditField*>(FindPaneByID(paneID_EditField));
	if (searchEdit)
	{
		// put the list in my LTabGroup...
		mList->SetSuperCommander(searchEdit->GetSuperCommander());

		// ... but make me the target
		SwitchTarget(searchEdit);
		LCommander::SetUpdateCommandStatus(true);
	}

	LGAPushButton * defaultBtn = dynamic_cast<LGAPushButton*>(FindPaneByID(paneID_SearchButton));
	if (defaultBtn) defaultBtn->SetDefaultButton(true, true);

	// refresh the list
	mList->RefreshList(CNewsSubscriber::GetHost(), MSG_SubscribeSearch);
}


//----------------------------------------------------------------------------
//	HandleNewGroupsTabActivate
//
//----------------------------------------------------------------------------

void CSubscribeWindow::HandleNewGroupsTabActivate()
{
	if (! mList)
		return;

	CSubscribeHostPopup* popup = dynamic_cast<CSubscribeHostPopup*>(FindPaneByID(paneID_ServerPopup));
	if (popup) popup->AddListener(this);

	SwitchTarget(mList);
	LCommander::SetUpdateCommandStatus(true);

	// refresh the list
	mList->RefreshList(CNewsSubscriber::GetHost(), MSG_SubscribeNew);
}


//----------------------------------------------------------------------------
//	HandleSearchInList
//
//----------------------------------------------------------------------------

void CSubscribeWindow::HandleSearchInList()
{
	if ((! mList) || (! mList->GetMessagePane()))
		return;

	Str255		editStr;
	LEditField* editField = dynamic_cast<LEditField*>(FindPaneByID(paneID_EditField));
	Assert_(editField);
	if (editField)
	{
		editField->GetDescriptor(editStr);
		mList->SearchForString(editStr);
	}
}


#pragma mark -


//----------------------------------------------------------------------------
//	CSubscribeHostPopup
//
//----------------------------------------------------------------------------

void*		CSubscribeHostPopup::sMercutioCallback = nil;

static Int16		cIMAPHostIconID = 15226;
static Int16		cNewsHostIconID = 15227;

//-----------------------------------
static pascal void SubscribeMercutioCallback(
	Int16					menuID,
	Int16					previousModifiers,
	RichItemDataYadaYada&	inItemData)
//-----------------------------------
{
#pragma unused (menuID)
#pragma unused (previousModifiers)
	RichItemData& itemData = (RichItemData&)inItemData;
	switch (itemData.cbMsg)
	{
		case cbBasicDataOnlyMsg:
			itemData.flags |=
				(ksameAlternateAsLastTime|kIconIsSmall|kHasIcon/*|kChangedByCallback*/|kdontDisposeIcon);
			itemData.flags &= ~(kIsDynamic);
			return;

		case cbIconOnlyMsg:
			break; // look out below!

		default:
			return;
	}

	MSG_Host * host = CNewsHostIterator::GetHostList()[itemData.itemID - 1];
	Int16 iconID = 0;
	if (MSG_IsNewsHost(host))
		iconID = cNewsHostIconID;
	else if (MSG_IsIMAPHost(host))
		iconID = cIMAPHostIconID;

	itemData.hIcon = CIconList::GetIconSuite(iconID);
	itemData.iconType = 'suit';
}

//--------------------------------------
CSubscribeHostPopup::CSubscribeHostPopup(LStream * inStream)
	: LGAPopup(inStream),
	mNewsHostIterator(nil)
{
}

//-----------------------------------------
CSubscribeHostPopup::~CSubscribeHostPopup()
{
	delete mNewsHostIterator;
}

//------------------------------------------
void CSubscribeHostPopup::FinishCreateSelf()
{
	LGAPopup::FinishCreateSelf();		// line order...
	MenuHandle menuH = GetMacMenuH();	// ...does matter
	if (MDEF_IsCustomMenu(menuH))
	{
		if (!sMercutioCallback)
			sMercutioCallback = NewMercutioCallback(SubscribeMercutioCallback);
		FailNIL_(sMercutioCallback);
		MDEF_SetCallbackProc(menuH, (MercutioCallbackUPP)sMercutioCallback);

		// Items with 'condense' style will be drawn by the Mercutio callback
		MenuPrefsRec myPrefs;
		::memset(&myPrefs, 0, sizeof(myPrefs));
		myPrefs.useCallbackFlag.s = condense;
		MDEF_SetMenuPrefs(menuH, &myPrefs);
	}
	ReloadMenu();
}

//------------------------------------
void CSubscribeHostPopup::ReloadMenu()
{
	// empty menu
	for (Int32 index = 0; index < GetMaxValue(); index ++) {
		::DeleteMenuItem(GetMacMenuH(), 1);
	}

	// reload menu
	delete mNewsHostIterator;
	mNewsHostIterator = new CNewsHostIterator(CMailNewsContext::GetMailMaster());
	MSG_Host*	outNewsHost;
	MenuHandle	menuH = GetMacMenuH();
	while (mNewsHostIterator->Next(outNewsHost))
	{
		// Add the menu item
		LStr255 pstr(MSG_GetHostUIName(outNewsHost));
		Int16 itemIndex = UMenuUtils::AppendMenuItem(menuH, pstr, true);

		// Ask for a callback so we can set the icon
		if (MDEF_IsCustomMenu(menuH))
			::SetItemStyle(menuH, itemIndex, condense);
	}

	// select the default host
	Int32 hostIndex;
	Boolean found = mNewsHostIterator->FindIndex(CNewsSubscriber::GetHost(), hostIndex);
	if (! found)
	{
		mNewsHostIterator->ResetTo(hostIndex = 0);

		MSG_Host*	outNewsHost;
		mNewsHostIterator->Current(outNewsHost);
		CNewsSubscriber::SetHost(outNewsHost);
	}

	SetMaxValue(mNewsHostIterator->GetCount());
	mValue = -1;				// force BroadcastValueMessage();
	SetValue(hostIndex + 1);	// iterator is 0-based, menu is 1-based
}


void CSubscribeHostPopup::BroadcastValueMessage()
{
	MSG_Host*		outNewsHost;
	mNewsHostIterator->ResetTo(mValue - 1);
	mNewsHostIterator->Current(outNewsHost);
	CNewsSubscriber::SetHost(outNewsHost);

	if (mValueMessage != msg_Nothing) {
		BroadcastMessage(mValueMessage, (void*)outNewsHost);
	}
}

