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



// CSimpleFolderView.cp

/*
	This class allows to get a list of containers:
		- Mail servers & folders
		- News servers & groups

	It was originally developed as part of CMessageFolderView
	which implements the Message Center. Having a separate class
	allows to implement folder lists in other places (Offline Picker).

	It inherits from CMailFlexTable and CStandardFlexTable.
	It handles the selection and clicks on the twistee icons but
	it doesn't handle any command, except for the Stop button.

	Mail and News folders are obtained from the Back-End by creating
	a Folder Pane.


	History:
	¥ In version 1.1 of this file, there was a mechanism to save/restore
	the selection in case of MSG_NotifyScramble or MSG_NotifyAll (like
	in CThreadView). This mechanism was removed because the folder view
	does not support sort commands.
*/

#include "CSimpleFolderView.h"

// Netscape Mac Libs
#include "resgui.h"

// Mail/News Specific
#include "CMailNewsWindow.h"
#include "MailNewsgroupWindow_Defines.h"
#include "CProgressListener.h"
#include "CMessageFolder.h"
#include "CMailNewsContext.h"
#include "UMailFolderMenus.h"

#define cmd_ExpandCellKludge	'Expd'	// fake command number used in a
										// kludge to avoid multiple redraws


//------------------------------------------------------------------------------
//		¥ CSimpleFolderView
//------------------------------------------------------------------------------
//
CSimpleFolderView::CSimpleFolderView(LStream *inStream)
			:	Inherited(inStream)
			,	mUpdateMailFolderMenusWhenComplete(false)
			,	mUpdateMailFolderMenusOnNextUpdate(false)
			,	mSelectHilitesWholeRow(false)
{
}


//------------------------------------------------------------------------------
//		¥ ~CSimpleFolderView
//------------------------------------------------------------------------------
//
CSimpleFolderView::~CSimpleFolderView()
{
}


#pragma mark -
//------------------------------------------------------------------------------
//		¥ LoadFolderList
//------------------------------------------------------------------------------
//	Given a context, starts getting the list. Typically called in the
//	FinishCreateSelf() of the parent window which is the one which creates the context.
//
void CSimpleFolderView::LoadFolderList(CNSContext* inContext)
{
	Assert_(inContext != NULL);
	mContext = inContext;
	if (GetMessagePane() == NULL) 
	{
		SetMessagePane(MSG_CreateFolderPane(*inContext, CMailNewsContext::GetMailMaster()));
		ThrowIfNULL_(GetMessagePane());
		MSG_SetFEData(GetMessagePane(), CMailCallbackManager::Get());
	}

	NotifySelfAll();	// notify ourselves that everything has changed
}


//------------------------------------------------------------------------------
//		¥ GetSelectedFolder
//------------------------------------------------------------------------------
//	Public utility: return the ID of the selected folder.
//
MSG_FolderInfo* CSimpleFolderView::GetSelectedFolder() const
{
	TableIndexT row = 0;
	if (GetMessagePane() && GetNextSelectedRow(row))
		return MSG_GetFolderInfo(GetMessagePane(), row - 1);
	return NULL;
}


//------------------------------------------------------------------------------
//		¥ SelectFirstFolderWithFlags
//------------------------------------------------------------------------------
//	Public utility: used to select the Inbox on "Window | Message Center"
//	or the first news server on "Prefs | Launch Collabra Discussions".
//
void CSimpleFolderView::SelectFirstFolderWithFlags(uint32 inFlags)
{
	MSG_FolderInfo*	folderInfo;

	::MSG_GetFoldersWithFlag(CMailNewsContext::GetMailMaster(), inFlags, &folderInfo, 1);
	SelectFolder(folderInfo);
}


//------------------------------------------------------------------------------
//		¥ SelectFolder
//------------------------------------------------------------------------------
//	Public utility: can be used to select the parent folder of a Thread window.
//
void CSimpleFolderView::SelectFolder(const MSG_FolderInfo* inFolderInfo)
{
	if (inFolderInfo)
	{
		MSG_ViewIndex index = ::MSG_GetFolderIndexForInfo(
									GetMessagePane(), (MSG_FolderInfo*)inFolderInfo, true);
		ArrayIndexT lineNumber = 1 + index;
		STableCell cellToSelect(lineNumber, 1);

		SetNotifyOnSelectionChange(false);
		UnselectAllCells();
		SetNotifyOnSelectionChange(true);

		ScrollCellIntoFrame(cellToSelect); // show folder before selecting it
		SelectCell(cellToSelect);
	}
	else
		UnselectAllCells();
}


//------------------------------------------------------------------------------
//		¥ DeleteSelection
//------------------------------------------------------------------------------
//	Required by CStandardFlexTable. We don't want to implement that here:
//	the Message Center will certainly stay the only place where a folder can be
//	deleted. If this were to change, move the code from CMessageFolderView back
//	to here along with the 'mWatchedFolder' hack.
//
void CSimpleFolderView::DeleteSelection()
{
}


#pragma mark -
//------------------------------------------------------------------------------
//		¥ GetIconID
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Return the folder icon for that row.
//
ResIDT CSimpleFolderView::GetIconID(TableIndexT inRow) const
{
	CMessageFolder folder(inRow, GetMessagePane());
	return folder.GetIconID();
}


//------------------------------------------------------------------------------
//		¥ GetNestedLevel
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Return the ident for that row.
//	This method goes together with GetHiliteTextRect().
//
UInt16 CSimpleFolderView::GetNestedLevel(TableIndexT inRow) const
{
	CMessageFolder folder(inRow, GetMessagePane());
	return folder.GetLevel() - 1;
}


//------------------------------------------------------------------------------
//		¥ GetHiliteColumn
//------------------------------------------------------------------------------
//	From CStandardFlexTable. When a row is selected, we can hilite the
//	whole row (default behavior of the base class) or just the folder name.
//	This method goes together with GetHiliteTextRect().
//
TableIndexT CSimpleFolderView::GetHiliteColumn() const
{
	if (mSelectHilitesWholeRow)
		return Inherited::GetHiliteColumn();

	return mTableHeader->ColumnFromID(kFolderNameColumn);
}


//------------------------------------------------------------------------------
//		¥ GetHiliteTextRect
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Pass back the rect to hilite for that row (either
//	the whole row, either just the folder name). Return true if rect is not empty.
//	This method goes together with GetHiliteColumn() and GetNestedLevel().
//
Boolean CSimpleFolderView::GetHiliteTextRect(
		const TableIndexT	inRow,
		Rect&				outRect) const
{
	if (mSelectHilitesWholeRow)
		return Inherited::GetHiliteTextRect(inRow, outRect);

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
//		¥ ApplyTextStyle
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Set the text style for that row.
//
void CSimpleFolderView::ApplyTextStyle(TableIndexT inRow) const
{
	CMessageFolder folder(inRow, GetMessagePane());
	// According to the latest UI spec (4/16/98), bold is used for folders with unread messages
	int32 textStyle = normal;
	if( folder.IsOpen() )
	{
		if ( folder.CountUnseen()> 0 )
		 	textStyle = bold;
	}
	else
	{
		// if the folder is closed we want to bold the folder if any subfolder has unread messages
		if ( folder.CountDeepUnseen()> 0 )
		 	textStyle = bold;
	}	 
	::TextFace( textStyle );
}


//------------------------------------------------------------------------------
//		¥ DrawCellContents
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Pass a cell and a rect: it draws it.
//	It knows how to draw the name column (with a twistee if necessary +
//	the icon corresponding to the item) and the 'Unread' and 'Total'
//	columns. If you have additional columns, overide this method.
//
void CSimpleFolderView::DrawCellContents(
		const STableCell&	inCell,
		const Rect&			inLocalRect)
{
	PaneIDT	cellType = GetCellDataType(inCell);
	switch (cellType)
	{
		case kFolderNameColumn:
		{
			// Draw icons (item icon + twistee)
			SInt16 iconRight = DrawIcons(inCell, inLocalRect);

			if (mRowBeingEdited != inCell.row || !mNameEditor)
			{
				
				// Draw folder name
				Rect textRect = inLocalRect;
				textRect.left = iconRight;
				char folderName[cMaxMailFolderNameLength + 1];
				if (GetHiliteText(inCell.row, folderName, sizeof(folderName), &textRect))
				{
					DrawTextString(folderName, &mTextFontInfo, 0, textRect);
					if (inCell.row == mDropRow)
						::InvertRect(&textRect);				
				}
			}
			break;
		}

		case kFolderNumUnreadColumn:
		{
			CMessageFolder folder(inCell.row, GetMessagePane());
			if (folder.CanContainThreads())
				if (folder.CountMessages() != 0)
					DrawCountCell(folder.CountUnseen(), inLocalRect);
			break;
		}

		case kFolderNumTotalColumn:
		{
			CMessageFolder folder(inCell.row, GetMessagePane());
			if (folder.CanContainThreads())
				if (folder.CountMessages() != 0)
					DrawCountCell(folder.CountMessages(), inLocalRect);
			break;
		}
	}
}


#pragma mark -
//------------------------------------------------------------------------------
//		¥ CellHasDropFlag
//------------------------------------------------------------------------------
//	Check if a cell has a twistee icon and if the twistee is open.
//
Boolean CSimpleFolderView::CellHasDropFlag(
		const STableCell& 	inCell, 
		Boolean& 			outIsExpanded) const
{
	CMessageFolder folder(inCell.row, GetMessagePane());
	if (GetCellDataType(inCell) == kFolderNameColumn && folder.CountSubFolders() != 0) 
	{
		outIsExpanded = folder.IsOpen();	
		return true;
	}
	return false;
}


//------------------------------------------------------------------------------
//		¥ SetCellExpansion
//------------------------------------------------------------------------------
//	Open or close the twistee icon of a folder.
//
void CSimpleFolderView::SetCellExpansion(
		const STableCell&	inCell, 
		Boolean				inExpand)
{
	// check current state
	CMessageFolder folder(inCell.row, GetMessagePane());
	if (inExpand == folder.IsOpen())
		return;

	// kludge: slow down the status bar refresh rate
	// to reduce flickers and improve performance
	if (inExpand)
	{
		if (folder.IsNewsHost() || folder.IsIMAPMailFolder())
		{
			CMailNewsWindow * myWindow = dynamic_cast<CMailNewsWindow*>
							(LWindow::FetchWindowObject(GetMacPort()));

			if (myWindow)
				myWindow->GetProgressListener()->SetLaziness(
							CProgressListener::lazy_VeryButForThisCommandOnly);
		}
	}

	// kludge to avoid multiple redraws (see ChangeFinished())
	mContext->SetCurrentCommand(cmd_ExpandCellKludge);

	// toggle twistee
	ToggleExpandAction(inCell.row);
	folder.FolderInfoChanged();
}


//------------------------------------------------------------------------------
//		¥ GetMainRowText
//------------------------------------------------------------------------------
//	From CStandardFlexTable. Return the folder name for that row.
//	Implemented here because the folder name is likely to stay
//	the main text of the row for the users of the class.
//
void CSimpleFolderView::GetMainRowText(
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

	CMessageFolder folder(inRow, GetMessagePane());
	CMailFolderMixin::GetFolderNameForDisplay(outText, folder);
	NET_UnEscape(outText);
}


#pragma mark -
//------------------------------------------------------------------------------
//		¥ FindCommandStatus
//------------------------------------------------------------------------------
//	Enable Stop button when list is loading.
//
void CSimpleFolderView::FindCommandStatus(
		CommandT			inCommand,
		Boolean				&outEnabled,
		Boolean				&outUsesMark,
		Char16				&outMark,
		Str255				outName)
{
	if (mStillLoading)
	{
		if (inCommand == cmd_Stop)
		{
			outEnabled = true;
			return;
		}
	}
	else
	{
		Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
	}
}


//------------------------------------------------------------------------------
//		¥ ListenToMessage
//------------------------------------------------------------------------------
//	When list is complete, update folder menus across the app.
//
void CSimpleFolderView::ListenToMessage(MessageT inMessage, void* ioParam)
{
	Inherited::ListenToMessage(inMessage, ioParam);

	if (inMessage == msg_NSCAllConnectionsComplete)
	{
		if (mUpdateMailFolderMenusWhenComplete)
		{
			mUpdateMailFolderMenusWhenComplete = false;
			CMailFolderMixin::UpdateMailFolderMixins();
		}
		mContext->SetCurrentCommand(cmd_Nothing);
	}
}


#pragma mark -
//------------------------------------------------------------------------------
//		¥ ChangeFinished
//------------------------------------------------------------------------------
//	A list of hacks to update our list + the folder menus across the app.
//	Also restores the selection after a 'sort' command (see ChangeStarting()).
//
void CSimpleFolderView::ChangeFinished(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount)
{
	//--------
	// When opening a News host by toggling the twistee in the Message Center,
	// we get one notification for each subscribed newsgroup and it generates
	// a lot of flicker.
	//
	// The following is a kludge which just ignores the notifications because
	// a News host info never changes anyway. However we're not guaranteed
	// that it will always be the case in the future. A bug report (#79163)
	// has been opened asking the Back-End folks to fix the problem and
	// reassign the bug to a Mac engineer in order to remove that kludge.
	//
		if (inChangeCode == MSG_NotifyChanged)
		{
			if (inRowCount == 1)
			{
				CMessageFolder folder(inStartRow, GetMessagePane());
				folder.FolderInfoChanged();
				UInt32 folderFlags = folder.GetFolderFlags();
				if (((folderFlags & MSG_FOLDER_FLAG_NEWS_HOST) != 0) && 
					((folderFlags & MSG_FOLDER_FLAG_DIRECTORY) != 0))
					inChangeCode = MSG_NotifyNone;
			}
		}
	//
	//--------

	Inherited::ChangeFinished(inPane, inChangeCode, inStartRow, inRowCount);
	
	if (mMysticPlane < kMysticUpdateThreshHold)
	{
		switch (inChangeCode)
		{
			case MSG_NotifyScramble:
			case MSG_NotifyAll:
				UnselectAllCells();
				CMailFolderMixin::UpdateMailFolderMixins();	// This should really be somewhere else!
				break;
				
			case MSG_NotifyInsertOrDelete:
				CMailFolderMixin::UpdateMailFolderMixins();
				// When rows are inserted or deleted because the user has moved a folder hierarchy,
				// then we have to invalidate the cache for the inserted rows, because
				// the folder levels have possibly changed.  This also applies to cmd_Clear,
				// which (unless the option key is down) is a move in disguise.  When expanding
				// folders, we need to do this too, just to cover the case when a folder was
				// moved into another folder and is now being displayed for the first time.
				CommandT currentCommand = mContext->GetCurrentCommand();
				if ((currentCommand == cmd_MoveMailMessages
				|| currentCommand == cmd_Clear
				|| currentCommand == cmd_ExpandCellKludge)
			 	&& inRowCount > 0)
					FoldersChanged(inStartRow, inRowCount);
				break;

			case MSG_NotifyChanged:
				if (mContext->GetCurrentCommand() == cmd_ExpandCellKludge)
					mUpdateMailFolderMenusWhenComplete = true;
				else
				if (mUpdateMailFolderMenusOnNextUpdate)
				{
					mUpdateMailFolderMenusOnNextUpdate = false;
					CMailFolderMixin::UpdateMailFolderMixins();
				}
				else
				{
					FoldersChanged(inStartRow, inRowCount);
				}
				break;
		}
	}
}


//------------------------------------------------------------------------------
//		¥ FoldersChanged
//------------------------------------------------------------------------------
//	Mark a range of folders as 'changed': invalidate the cached line for each.
//
void CSimpleFolderView::FoldersChanged(
		TableIndexT		inStartRow,
		SInt32			inRowCount) const
{
	if (inRowCount < 0)
		inRowCount = -inRowCount;

	for (SInt32 i = 0; i < inRowCount; i ++)
	{
		CMessageFolder folder(inStartRow + i, GetMessagePane());
		folder.FolderInfoChanged();
	}
}
