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



// CSubscribeView.cp

#include "CSubscribeView.h"

// MacOS
#include <Events.h>

// PowerPlant
#include <LDropFlag.h>
#include <UCursor.h>

// XP
#include "msgcom.h"

// MacFE
#include "macutil.h"
#include "macgui.h"
#include "resgui.h"
#include "CPaneEnabler.h"

// Mail/News Specific
#include "CMailNewsContext.h"
#include "CSubscribeWindow.h"
#include "MailNewsgroupWindow_Defines.h"
#include "UMailFolderMenus.h"
#include "UMailSelection.h"

/*

Overview:
---------

	CSubscribeView:
		It inherits from CMailFlexTable and displays the list of newsgroups.
		It has 3 states (or MSG_SubscribeMode) which correspond to the 3 panels
		of the CSubscribeWindow: All, Search and New. In each state, you can
		select the newsgroups you want to subscribe to.

	CNewsgroup:
		It corresponds to an entry in the CSubscribeView and allows
		you to access to the properties of the entry. Its purpose
		is more informational than functional. Most of the methods
		are simple accessors. Some of them are not used but left
		here for future generations.


Hints:
------

	Loading of the newsgroup list is done in the idler because it's a synchronous
	operation and we don't want it to be done before the window is displayed. It
	could potentially take a long time.
	
	The methods you need to know about are RefreshList() and the usual
	pair of FindCommandStatus() and ObeyCommand(). The rest is mostly for
	internal use.

	Most of the commands handled by the CSubscribeView are of type cmd_NewsXXX
	(cmd_NewsToggleSubscribe for instance) and they correspond to the different buttons
	of the CSubscribeWindow. The commands which don't fit in that scheme (ie. for
	which a higher and better level of abstraction might have been possible) are:

		* msg_EditField: sent by the editable field of the "All Groups" tab panel,
		it allows to do a type-ahead search in the list of newsgroups. Enabled
		here in FindCommandStatus() and processed by CSubscribeWindow.

		* cmd_OpenNewsHost: sent by the Add Server button. Enabled here in
		FindCommandStatus() and processed by CSubscribeWindow.

		* msg_TabSelect: sent by the LTabGroup. Processed here.

	The communication with the news servers has two states. In the first one,
	we download the list of newsgroup names: it happens the first time we
	connect to the server (the list is then stored in a cache file), or
	everytime we click the Get Groups button. In that state, the user is not
	allowed to do anything except cancel or connect to another server.
	In the second state, we download the number of postings in each newsgroup:
	it happens everytime we switch to another tab panel or open a folder in
	the list. Since it is much more common, it is done asynchronously and
	all (or almost all) the commands must be available to the user. So, we have
	two flags which indicate how busy we are. The first one, mStillLoading,
	is inherited from CMailFlexTable and is set whenever we talk to the host.
	The second flag, mStillLoadingFullList, is defined here and is set when
	we downnload the newsgroup list.

	More info about the window in "CSubscribeWindow.cp".

History:
--------
	November 97:
	  Added the mCommitState for IMAP public folders support. It is set
	  when the user clicks OK: while the BE talks to the IMAP server,
	  the dialog is put in a state where the only possible action is
	  to click the Stop button.
*/


enum
{
	kNormalMessageFolderIconID		=	15238
,	kNewsgroupIconID				=	15231
,	kSubscribedIconID				= 	15237
,	kUnsubscribedIconID				= 	15235
	
};

#define kIconMargin			4
#define kIndentPerLevel		16
#define kIconWidth			16


#pragma mark --- CNewsgroup

//----------------------------------------------------------------------------
//	CNewsgroup
//
//		Constructor
//		Cache functions
//----------------------------------------------------------------------------

MSG_GroupNameLine	CNewsgroup::sNewsgroupData;
MSG_ViewIndex		CNewsgroup::sCachedIndex;


CNewsgroup::CNewsgroup(TableIndexT inRow, MSG_Pane* inNewsgroupList) 
	:	mIndex(inRow - 1)
	,	mNewsgroupList(inNewsgroupList)
{
	InvalidateCache();
	UpdateCache();
}

void CNewsgroup::InvalidateCache() const
{
	sCachedIndex = LONG_MAX;
}

void CNewsgroup::UpdateCache() const
{
	if (mIndex != sCachedIndex)
	{
		MSG_GetGroupNameLineByIndex(mNewsgroupList, mIndex, 1, &sNewsgroupData);
		sCachedIndex = mIndex;
	}
}


//----------------------------------------------------------------------------
//	CNewsgroup / Newsgroup info accessor functions
//
//		GetNewsgroupLine
//
//		GetName
//		GetPrettyName
//		CountPostings
//		GetLevel
//		CanContainThreads
//		CountChildren
//		GetIconID
//----------------------------------------------------------------------------

MSG_GroupNameLine* CNewsgroup::GetNewsgroupLine() const
{
	UpdateCache();
	return &sNewsgroupData;
}


char*		CNewsgroup::GetName() const				{ return GetNewsgroupLine()->name;}
char* 		CNewsgroup::GetPrettyName() const		{ return GetNewsgroupLine()->name;}
Int32		CNewsgroup::CountPostings() const		{ return GetNewsgroupLine()->total;}
UInt32		CNewsgroup::GetLevel() const			{ return GetNewsgroupLine()->level;}
Boolean		CNewsgroup::CanContainThreads() const	{ return GetLevel() > kRootLevel;}


UInt32 CNewsgroup::CountChildren() const
{
	if (HasChildren())
		return sNewsgroupData.total;
	return 0;
}


ResIDT CNewsgroup::GetIconID() const
{
	//е TO DO е: deal with the "Open" state, the "read" state, etc.
	if (this->HasChildren())
		return kNormalMessageFolderIconID;
	else
		return kNewsgroupIconID;
}


//----------------------------------------------------------------------------
//	CNewsgroup / Flags accessor functions
//
//		GetNewsgroupFlags
//
//		IsOpen
//		HasChildren
//		IsSubscribed
//		IsNew
//----------------------------------------------------------------------------

UInt32 CNewsgroup::GetNewsgroupFlags() const
{
	UpdateCache();
	return sNewsgroupData.flags;
}


Boolean CNewsgroup::IsOpen() const
{
	return ((GetNewsgroupFlags() & MSG_GROUPNAME_FLAG_ELIDED) == 0);
}


Boolean CNewsgroup::HasChildren() const
{	
	return ((GetNewsgroupFlags() & MSG_GROUPNAME_FLAG_HASCHILDREN) != 0);
}


Boolean CNewsgroup::IsSubscribed() const
{
	return ((GetNewsgroupFlags() & MSG_GROUPNAME_FLAG_SUBSCRIBED) != 0);
}


Boolean CNewsgroup::IsNew() const
{
	return ((GetNewsgroupFlags() & MSG_GROUPNAME_FLAG_NEW_GROUP) != 0);
}


#pragma mark --- CSubscribeView

//----------------------------------------------------------------------------
//	CSubscribeView Constructor/Destructor
//
//----------------------------------------------------------------------------

CSubscribeView::CSubscribeView(LStream *inStream)
:	Inherited(inStream),
	mStillLoadingFullList(false),
	mCommitState(kIdle)
{
}


CSubscribeView::~CSubscribeView()
{
//	Warning: Don't call MSG_DestroyPane() here: 
//	CMailFlexTable calls SetMessagePane(NULL) in its destructor.
}


//----------------------------------------------------------------------------
//	CloseParentWindow
//
//----------------------------------------------------------------------------
void CSubscribeView::CloseParentWindow()
{
	CSubscribeWindow * myWindow = 
		dynamic_cast<CSubscribeWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	if (myWindow)
		myWindow->DoClose();
}


//----------------------------------------------------------------------------
//	SetProgressBarLaziness
//
//----------------------------------------------------------------------------
void CSubscribeView::SetProgressBarLaziness(
			CProgressListener::ProgressBarLaziness 		inLaziness)
{
	CSubscribeWindow * myWindow = 
		dynamic_cast<CSubscribeWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	if (myWindow)
	{
		CProgressListener * myProgress = myWindow->GetProgressListener();
		if (myProgress)
			myProgress->SetLaziness(inLaziness);
	}
}


//----------------------------------------------------------------------------
//	ProcessKeyPress
//
//		Update buttons depending on the selection when scrolling through the list
//----------------------------------------------------------------------------

Boolean CSubscribeView::ProcessKeyPress(const EventRecord &inKeyEvent)
{
	Boolean	keyHandled = Inherited::ProcessKeyPress(inKeyEvent);
	CPaneEnabler::UpdatePanes();
	return keyHandled;
}


//----------------------------------------------------------------------------
//	GetIconID
//
//----------------------------------------------------------------------------

ResIDT CSubscribeView::GetIconID(TableIndexT inRow) const
{
	CNewsgroup newsgroup(inRow, GetMessagePane());
	return newsgroup.GetIconID();
}

//----------------------------------------------------------------------------
//	DrawCell
//
//----------------------------------------------------------------------------

void CSubscribeView::DrawCell(const STableCell& inCell, const Rect& inLocalRect)
{
	// check if we are in the update region
	RgnHandle updateRgn  = GetLocalUpdateRgn();
	if (!updateRgn)
		return;

	Rect updateRect = (**updateRgn).rgnBBox;
	::DisposeRgn(updateRgn);
	
	Rect intersection;
	if (!::SectRect(&updateRect, &inLocalRect, &intersection))
		return;

	// draw the cell
	StClipRgnState savedClip;
	::ClipRect(&intersection);
	PaneIDT	cellType = GetCellDataType(inCell);
	CNewsgroup newsgroup(inCell.row, GetMessagePane());
	switch (cellType)
	{
		case kNewsgroupNameColumn:
			SInt16 newLeft = DrawIcons(inCell, inLocalRect);
			Rect textRect = inLocalRect;
			textRect.left = newLeft;
			char groupName[cMaxMailFolderNameLength+1];
			if (GetHiliteText(inCell.row, groupName, sizeof(groupName), &textRect))
				DrawTextString(groupName, &mTextFontInfo, 0, textRect);
			break;

		case kNewsgroupSubscribedColumn:
			if (CanSubscribeToNewsgroup(newsgroup))
			{
				short iconID = (newsgroup.IsSubscribed() ? 
								kSubscribedIconID : kUnsubscribedIconID);
				DrawIconFamily(iconID, 16, 16, 0, inLocalRect);
			}
			break;

		case kNewsgroupPostingsColumn:
			if (CanSubscribeToNewsgroup(newsgroup))
				DrawCountCell(newsgroup.CountPostings(), inLocalRect);
			break;

	}
}


//----------------------------------------------------------------------------
//	RefreshList
//
//----------------------------------------------------------------------------

void CSubscribeView::RefreshList(MSG_Host* newsHost, MSG_SubscribeMode subscribeMode)
{
	mNewsHost = newsHost;
	mSubscribeMode = subscribeMode;
	StartIdling();
}


//----------------------------------------------------------------------------
//	SpendTime
//
//----------------------------------------------------------------------------

void CSubscribeView::SpendTime(const EventRecord &)
{
	// stop idling first (avoid infinite recursive calls if BE calls FE_Alert)
	StopIdling();

	Assert_(mContext != NULL);
	if (mContext == NULL)
		return;
	
	StSpinningBeachBallCursor	beachBallCursor;

	// create the list
	if (GetMessagePane() == NULL) 
	{
		SetMessagePane(MSG_CreateSubscribePaneForHost(
							*mContext, CMailNewsContext::GetMailMaster(), mNewsHost));
		ThrowIfNULL_(GetMessagePane());
		MSG_SetFEData(GetMessagePane(), CMailCallbackManager::Get());

		// force a refresh of the postings count
		SetProgressBarLaziness(CProgressListener::lazy_VeryButForThisCommandOnly);
		MSG_Command(GetMessagePane(), MSG_UpdateMessageCount, NULL, NULL);
	}
	else
	{
		// display the list (it is read from the cache file)
		SetProgressBarLaziness(CProgressListener::lazy_VeryButForThisCommandOnly);
		MSG_SubscribeSetHost(GetMessagePane(), mNewsHost);
	}
	MSG_SubscribeSetMode(GetMessagePane(), mSubscribeMode);

	switch (mSubscribeMode)
	{
		case MSG_SubscribeAll:
			MSG_GroupNameLine aLine;
			// If the list (ie. the cache file) is empty, download it from the server.
			if (! MSG_GetGroupNameLineByIndex(GetMessagePane(), 0, 1, &aLine))
			{
				SetProgressBarLaziness(CProgressListener::lazy_JustABit);
				MSG_Command(GetMessagePane(), MSG_FetchGroupList, NULL, NULL);
			}
			break;

		case MSG_SubscribeSearch:
			// don't get the list yet: wait until user clicks Search button
			break;

		case MSG_SubscribeNew:
			// Always get the list of new newsgroups from the server
			SetProgressBarLaziness(CProgressListener::lazy_VeryButForThisCommandOnly);
			MSG_Command(GetMessagePane(), MSG_CheckForNew, NULL, NULL);
			break;
	}

	// notify ourselves
	NotifySelfAll();
	
	// Since fetching the group list is a synchronous operation, we may have a lot of
	// unresponded to clicks and key strokes in the event queue. Flush them.
	::FlushEvents(mDownMask | mUpMask | keyDownMask	| keyUpMask | autoKeyMask, 0);
}


//----------------------------------------------------------------------------
//	SearchForString
//
//----------------------------------------------------------------------------
void	CSubscribeView::SearchForString(const StringPtr searchStr)
{
	P2CStr(searchStr);
	switch (MSG_SubscribeGetMode(GetMessagePane()))
	{
		case MSG_SubscribeAll:
			MSG_ViewIndex theRow;
			theRow = MSG_SubscribeFindFirst(GetMessagePane(), (const char *)searchStr);
			STableCell theCell(theRow + 1, 1);
			SetNotifyOnSelectionChange(false);
			UnselectAllCells();
			SetNotifyOnSelectionChange(true);
			SelectCell(theCell);
			ScrollCellIntoFrame(theCell);
			CPaneEnabler::UpdatePanes();
			break;

		case MSG_SubscribeSearch:
			MSG_SubscribeFindAll(GetMessagePane(), (const char *)searchStr);
			// beep when list is empty
			MSG_GroupNameLine aLine;
			if (! MSG_GetGroupNameLineByIndex(GetMessagePane(), 0, 1, &aLine))
				::SysBeep(1);
			break;
	}
}


//----------------------------------------------------------------------------
//	SubscribeCancel
//
//----------------------------------------------------------------------------
void	CSubscribeView::SubscribeCancel()
{
	if (GetMessagePane() != NULL)
		MSG_SubscribeCancel(GetMessagePane());
	CloseParentWindow();
}


//----------------------------------------------------------------------------
//	SubscribeCommit
//
//----------------------------------------------------------------------------
void	CSubscribeView::SubscribeCommit()
{
	if (GetMessagePane() != NULL)
	{
		mContext->AddListener(this);
		if (XP_IsContextBusy(*mContext))
		{
			mCommitState = kInterrupting;
			XP_InterruptContext(*mContext);
		}
		else
		{
			mCommitState =  kCommitting;
			MSG_SubscribeCommit(GetMessagePane());
		}
	}
	else
		CloseParentWindow();
}


//----------------------------------------------------------------------------
//	CellHasDropFlag
//
//----------------------------------------------------------------------------

Boolean CSubscribeView::CellHasDropFlag(
	const STableCell& 		inCell, 
	Boolean& 				outIsExpanded) const
{
	PaneIDT cellType = GetCellDataType(inCell);
	CNewsgroup newsgroup(inCell.row, GetMessagePane());
	if (cellType == kNewsgroupNameColumn && newsgroup.CountChildren() != 0) 
	{
		outIsExpanded = newsgroup.IsOpen();	
		return true;
	}
	return false;
}


//----------------------------------------------------------------------------
//	SetCellExpansion
//
//----------------------------------------------------------------------------

void CSubscribeView::SetCellExpansion(
	const STableCell&		inCell, 
	Boolean					inExpand)
{
	CNewsgroup newsgroup(inCell.row, GetMessagePane());
	if (inExpand == newsgroup.IsOpen())
		return;
	ToggleExpandAction(inCell.row);
}

//------------------------------------------------------------------------------
//	GetNestedLevel
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Return the ident for that row.
//	This method goes together with GetHiliteTextRect().
//
UInt16 CSubscribeView::GetNestedLevel(TableIndexT inRow) const
{
	CNewsgroup newsgroup(inRow, GetMessagePane());
	return newsgroup.GetLevel();
}

//------------------------------------------------------------------------------
//	GetHiliteColumn
//------------------------------------------------------------------------------
//	From CStandardFlexTable. When a row is selected, we can
//	hilite the whole row or just the folder name.
//	This method goes together with GetHiliteTextRect().
//
TableIndexT CSubscribeView::GetHiliteColumn() const
{
	return mTableHeader->ColumnFromID(kNewsgroupNameColumn);
}


//------------------------------------------------------------------------------
//	GetHiliteTextRect
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Pass back the rect to hilite for that row (either
//	the whole row, either just the folder name). Return true if rect is not empty.
//	This method goes together with GetHiliteColumn() and GetNestedLevel().
//
Boolean CSubscribeView::GetHiliteTextRect(
		const TableIndexT	inRow,
		Rect&				outRect) const
{
	STableCell cell(inRow, GetHiliteColumn());
	if (!GetLocalCellRect(cell, outRect))
		return false;

	Rect iconRect;
	GetIconRect(cell, outRect, iconRect);
	outRect.left = iconRect.right;
	char folderName[cMaxMailFolderNameLength + 1];
	GetHiliteText(inRow, folderName, sizeof(folderName), &outRect);
	return true;
}


//------------------------------------------------------------------------------
//	GetMainRowText
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Return the folder name for that row.
//
void CSubscribeView::GetMainRowText(
		TableIndexT		inRow,
		char*			outText,
		UInt16			inMaxBufferLength) const
{
	if (!outText || inMaxBufferLength == 0)
		return;

	if (inMaxBufferLength < cMaxMailFolderNameLength + 1)
	{
		*outText = '\0';
		return;
	}

	CNewsgroup newsgroup(inRow, GetMessagePane());
	// еее FIX ME -> Get the BE to store the newsgroup name already unescaped
	XP_STRCPY(outText, newsgroup.GetPrettyName());
	NET_UnEscape(outText);
}


//----------------------------------------------------------------------------
//	ChangeStarting
//
//----------------------------------------------------------------------------

void CSubscribeView::ChangeStarting(
	MSG_Pane*			inPane,
	MSG_NOTIFY_CODE		inChangeCode,
	TableIndexT			inStartRow,
	SInt32				inRowCount)
{
	switch ( inChangeCode )
	{
		case MSG_NotifyScramble:
		case MSG_NotifyAll:
			mStillLoadingFullList = true;
			break;
	}
	
	Inherited::ChangeStarting(inPane, inChangeCode, inStartRow, inRowCount);
}

//----------------------------------------------------------------------------
//	ChangeFinished
//
//----------------------------------------------------------------------------

void CSubscribeView::ChangeFinished(
	MSG_Pane*			inPane,
	MSG_NOTIFY_CODE		inChangeCode,
	TableIndexT			inStartRow,
	SInt32				inRowCount)
{
	switch ( inChangeCode )
	{
		case MSG_NotifyScramble:
		case MSG_NotifyAll:
			mStillLoadingFullList = false;
			break;
	}

	Inherited::ChangeFinished(inPane, inChangeCode, inStartRow, inRowCount);
	
	if ( mMysticPlane < kMysticUpdateThreshHold )
	{
		switch ( inChangeCode )
		{
			case MSG_NotifyScramble:
			case MSG_NotifyAll:
				UnselectAllCells();
				break;
		}
	}
}

//----------------------------------------------------------------------------
//	OpenRow
//
//----------------------------------------------------------------------------
//	Called on enter/return or double-click.
//	Hack: don't do anything when multiple items are selected.
//
void CSubscribeView::OpenRow(TableIndexT inRow)
{
	if (inRow > 0)
	{
		CMailSelection selection;
		if (selection.selectionSize <= 1)
		{
			STableCell aCell(inRow, 1);
			Boolean outIsExpanded;
			if (CellHasDropFlag(aCell, outIsExpanded))
			{
				MSG_ToggleExpansion (GetMessagePane(), inRow - 1, NULL);
			}
		}
	}
}


//----------------------------------------------------------------------------
//	ListenToMessage
//
//----------------------------------------------------------------------------

void CSubscribeView::ListenToMessage(MessageT inMessage, void* ioParam)
{
	if (! ObeyCommand(inMessage, ioParam))	// check button messages first
	{
		switch (inMessage)
		{
			case msg_NSCAllConnectionsComplete:
				if (mCommitState == kInterrupting)
				{
					mCommitState = kCommitting;
					MSG_SubscribeCommit(GetMessagePane());
				}
				else if (mCommitState == kCommitting)
				{
					mCommitState = kIdle;
					CloseParentWindow();	// the window is closed here...
					return;					// ...so exit now because I'm dead
				}
				/* no break; */

			default:
				Inherited::ListenToMessage(inMessage, ioParam);
				break;
		}
	}
}


//----------------------------------------------------------------------------
//	FindCommandStatus
//
//----------------------------------------------------------------------------

void CSubscribeView::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
{
	// The pane can be null when no server has been configured.
	// In that case, we only enable the Add Server button.
	if (GetMessagePane() == NULL)
	{
		outEnabled = (inCommand == cmd_OpenNewsHost);
		return;
	}

	// When committing, allow only the Stop button...
	if (mCommitState == kCommitting)
	{
		outEnabled = (inCommand == cmd_Stop);
		return;
	}
	else if (mCommitState == kInterrupting)
	{
		outEnabled = false;
		return;
	}

	// ...otherwise the Stop button is enabled whenever we talk to the server
	if (inCommand == cmd_Stop)
	{
		outEnabled = (mStillLoading | mStillLoadingFullList);
		return;
	}

	// Handle basic commands as long as we're not
	// downloading the full newsgroups list
	if (! mStillLoadingFullList)
	{
		if (FindMessageLibraryCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName))
			return;
	}

	switch (inCommand)
	{
		case msg_EditField:		// similar in behaviour to cmd_NewsSearch
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
			FindSubscribeCommandStatus(inCommand, outEnabled);
			break;

		default:
			Inherited::FindCommandStatus(
				inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
}


//----------------------------------------------------------------------------
//	FindSubscribeCommandStatus
//
//----------------------------------------------------------------------------
void CSubscribeView::FindSubscribeCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled)
{
	Assert_(GetMessagePane() != NULL);
	if (GetMessagePane() == NULL)
	{
		outEnabled = false;
		return;
	}

	// When downloading the newsgroups list, can't do
	// anything but connect to a different server.
	if (mStillLoadingFullList)
	{
		outEnabled = (inCommand == cmd_NewsHostChanged);
		return;
	}

	// When connected to an IMAP server, the Search and New tab panels
	// are completely disabled except for connect to a different server.
	if (MSG_IsIMAPHost(mNewsHost))
	{
		switch (MSG_SubscribeGetMode(GetMessagePane()))
		{
			case MSG_SubscribeSearch:
			case MSG_SubscribeNew:
				outEnabled = (inCommand == cmd_NewsHostChanged);
				return;		// <-- note it
		}
	}

	switch (inCommand)
	{
		// Expand All is enabled if the selection
		// contains a non-expanded folder and
		// if the host is not an IMAP host
		case cmd_NewsExpandAll:
			{
				if (! MSG_IsIMAPHost(mNewsHost))
				{
					CMailSelection selection;
					GetSelection(selection);
					if (selection.selectionSize > 0)
					{
						STableCell	aCell;
						Boolean		outIsExpanded;
						for (long index = 0; index < selection.selectionSize; index ++)
						{
							aCell.SetCell(selection.GetSelectionList()[index] + 1, 1);
							if (CellHasDropFlag(aCell, outIsExpanded) && (! outIsExpanded))
							{
								outEnabled = true;
								break;
							}
						}
					}
				}
			}
			break;

		// These commands require a selection. Note: Collapse All
		// does not really require a selection but it is prettier
		// when it is not activated just after the list is displayed.
		case cmd_NewsToggleSubscribe:
		case cmd_NewsExpandGroup:
		case cmd_NewsCollapseGroup:
		case cmd_NewsCollapseAll:
			{
				CMailSelection selection;
				GetSelection(selection);
				outEnabled = (selection.selectionSize > 0);
			}
			break;

		// These commands require a selection _and_ the subscribed/unsubscribed
		// state of the selected items must match the command.
		case cmd_NewsSetSubscribe:
		case cmd_NewsClearSubscribe:
			{
				Boolean lookingForBool = (inCommand == cmd_NewsSetSubscribe ? false : true);

				CMailSelection selection;
				if (GetSelection(selection))
				{
					const MSG_ViewIndex* index = selection.GetSelectionList();
					for (int i = 0; i < selection.selectionSize; i ++, index++)
					{
						CNewsgroup newsgroup((*index + 1), GetMessagePane()); // add one to convert to TableIndexT
						if (CanSubscribeToNewsgroup(newsgroup))
						{
							if (lookingForBool == newsgroup.IsSubscribed())
							{
								outEnabled = true;	// found what I was looking for
								break;
							}
						}
					}
				}
			}
			break;

		// Don't enable Get Groups when I'm downloading the number of postings.
		// This is not required but forcing the user to click Stop before is
		// a way to notify him that something is going on on the network.
		case cmd_NewsGetGroups:
			outEnabled = (! mStillLoading);
			break;

		// This commands are always enabled, except when downloading
		// the newsgroups list but this is checked above
		case msg_EditField:		// similar in behaviour to cmd_NewsSearch
		case cmd_NewsSearch:
		case cmd_NewsGetNew:
		case cmd_NewsClearNew:
		default:
			outEnabled = true;
			break;
	}
}


//----------------------------------------------------------------------------
//	ObeyCommand
//
//----------------------------------------------------------------------------

Boolean CSubscribeView::ObeyCommand(
	CommandT			inCommand,
	void				*ioParam)
{
	Boolean result = false;
	mContext->SetCurrentCommand(inCommand);
	switch(inCommand)
	{
		//-----------------------------------	
		// TabGroup command
		//-----------------------------------
		case msg_TabSelect:
			if (GetMessagePane() != NULL)
			{
				CMailSelection selection;
				GetSelection(selection);
				if (selection.selectionSize == 0)
				{
					// set a selection to activate the button enablers
					STableCell cell(1,1);
					SelectCell(cell);
				}
			}
			result = true;
			break;

		//-----------------------------------	
		// Button commands
		//-----------------------------------

// now handled by the window
//		case cmd_NewsHostChanged:
//			if (mStillLoading || mStillLoadingFullList)
//				ObeyCommand(cmd_Stop, NULL);
//			MSG_Host* newsHost = (MSG_Host*)(ioParam);
//			RefreshList(newsHost, MSG_SubscribeGetMode(GetMessagePane()));
//			result = true;
//			break;

		case cmd_NewsExpandAll:
		case cmd_NewsCollapseAll:
		case cmd_NewsToggleSubscribe:
		case cmd_NewsSetSubscribe:
		case cmd_NewsClearSubscribe:
		case cmd_NewsGetGroups:
		case cmd_NewsGetNew:
		case cmd_NewsClearNew:
			MSG_CommandType theCommand;
			switch (inCommand)
			{
				case cmd_NewsExpandAll: 		theCommand = MSG_ExpandAll;				break;
				case cmd_NewsCollapseAll:		theCommand = MSG_CollapseAll;			break;
				case cmd_NewsToggleSubscribe:	theCommand = MSG_ToggleSubscribed;		break;
				case cmd_NewsSetSubscribe:		theCommand = MSG_SetSubscribed;			break;
				case cmd_NewsClearSubscribe:	theCommand = MSG_ClearSubscribed;		break;
				case cmd_NewsGetGroups:			theCommand = MSG_FetchGroupList;		break;
				case cmd_NewsGetNew:			theCommand = MSG_CheckForNew;			break;
				case cmd_NewsClearNew:			theCommand = MSG_ClearNew;				break;
			}

			if (inCommand == cmd_NewsExpandAll || inCommand == cmd_NewsCollapseAll)
				UCursor::SetWatch();

			CMailSelection selection;		// not all commands require the selection...
			GetSelection(selection);		// ...in these cases, it will just be ignored
			MSG_Command(GetMessagePane(),
						theCommand, 
						(MSG_ViewIndex*)selection.GetSelectionList(),
						selection.selectionSize
						);

			UCursor::SetArrow();
			result = true;
			break;

		//-----------------------------------	
		// MSGLIB commands
		//-----------------------------------		
		default:
			if (inCommand == cmd_Stop)
			{
				if (mCommitState != kIdle)
				{
					mCommitState = kIdle;
					mContext->RemoveListener(this);
				}
			}
			result = (ObeyMessageLibraryCommand(inCommand, ioParam)		
				|| Inherited::ObeyCommand(inCommand, ioParam));
			break;
	}
	return result;
}

//----------------------------------------------------------------------------
//	ClickSelect
//
//		Handle clicks in the Subscribe column.
//----------------------------------------------------------------------------

Boolean	CSubscribeView::ClickSelect(
	const STableCell		&inCell,
	const SMouseDownEvent	&inMouseDown)
{
	PaneIDT	cellType = GetCellDataType(inCell);
	if (cellType == kNewsgroupSubscribedColumn)
		return true;
	else
		return Inherited::ClickSelect(inCell, inMouseDown);
}

//----------------------------------------------------------------------------
//	ClickCell
//
//		Handle clicks in the Subscribe column.
//----------------------------------------------------------------------------

void	CSubscribeView::ClickCell(
	const STableCell	&inCell,
	const SMouseDownEvent &inMouseDown)
							
{
	Boolean		clickInSubscribeColumn = false;

	PaneIDT	cellType = GetCellDataType(inCell);
	if (cellType == kNewsgroupSubscribedColumn)
	{
		CNewsgroup newsgroup(inCell.row, GetMessagePane());
		if (CanSubscribeToNewsgroup(newsgroup))
		{
			clickInSubscribeColumn = true;
			Inherited::ClickCell(inCell, inMouseDown);

			MSG_ViewIndex viewIndex = inCell.row - 1;
			MSG_Command(GetMessagePane(), MSG_ToggleSubscribed, &viewIndex, 1);
		}
	}

	if (! clickInSubscribeColumn)
		Inherited::ClickCell(inCell, inMouseDown);
}


//----------------------------------------------------------------------------
//	GetQapRowText
//
//		Return info for QA Partner
//----------------------------------------------------------------------------

#if defined(QAP_BUILD)		
void CSubscribeView::GetQapRowText(
	TableIndexT			inRow,
	char*				outText,
	UInt16				inMaxBufferLength) const
{
	if (!outText || inMaxBufferLength == 0)
		return;

	cstring rowText("");
	short colCount = mTableHeader->CountVisibleColumns();
	CNewsgroup newsgroup(inRow, GetMessagePane());

	CMailNewsWindow * myWindow = dynamic_cast<CMailNewsWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	if (!myWindow) return;

	for (short col = 1; col <= colCount; col ++)
	{
		STableCell	aCell(inRow, col);
		LTableHeader::SColumnData * colData = mTableHeader->GetColumnData(col);
		if (!colData) break;
		LPane * colPane = myWindow->FindPaneByID(colData->paneID);
		if (!colPane) break;

		// get column name
		CStr255 descriptor;
		colPane->GetDescriptor(descriptor);
		rowText += descriptor;
		rowText += "=\042";

		// add cell text
		switch (PaneIDT dataType = GetCellDataType(aCell))
		{

			case kNewsgroupNameColumn:
				if (newsgroup.CountChildren() != 0)
				{
					if (newsgroup.IsOpen())
						rowText += "-";
					else
						rowText += "+";
				}
				else
					rowText += " ";
				rowText += newsgroup.GetName();
				break;

			case kNewsgroupSubscribedColumn:
				if (CanSubscribeToNewsgroup(newsgroup))
					if (newsgroup.IsSubscribed())
						rowText += "+";
				break;

			case kNewsgroupPostingsColumn:
				int theNum = newsgroup.CountPostings();
				if (theNum >= 0)
				{
					char tempStr[32];
					sprintf(tempStr, "%d", theNum);
					rowText += tempStr;
				}
				else
					rowText += "?";
				break;
		}

		if (col < colCount)
			rowText += "\042 | ";
		else
			rowText += "\042\r";
	}
	strncpy(outText, (char*)rowText, inMaxBufferLength);
	outText[inMaxBufferLength - 1] = '\0';
} // CSubscribeView::GetQapRowText
#endif //QAP_BUILD
