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



// COfflinePicker.cp

#include "COfflinePicker.h"

#include "CMessageFolder.h"
#include "MailNewsAddressBook.h"
#include "URobustCreateWindow.h"
#include "UModalDialogs.h"

#include "cstring.h"
#include "macutil.h"
#include "MailNewsgroupWindow_Defines.h"
#include "LGAPushButton.h"
#include "dirprefs.h"

enum
{
	kCheckedIconID		= 15237
,	kUncheckedIconID	= 15235
,	kLDAPHdrIconID		= 15258 //15226
,	kLDAPIconID			= 15258
};


//------------------------------------------------------------------------------
//		¥ COfflineItem
//------------------------------------------------------------------------------
//
class COfflineItem
{
public:
						COfflineItem(const TableIndexT inRow, const COfflinePickerView* inPickerView);	
						~COfflineItem();

	UInt32				GetLevel() const;
	ResIDT				GetIconID() const;
	const char*			GetName() const;

	UInt32				GetFolderPrefFlags() const;
	void				SetFolderPrefFlags(const UInt32) const;

	Boolean				IsOpen() const;
	Boolean				IsNewsgroup() const;
	Boolean				IsLocalMailFolder() const;
	Boolean				IsIMAPMailFolder() const;
	Boolean				IsLDAPDirectory() const;

	Boolean				HasNewMessages() const;
	UInt32				CountSubFolders() const;


	TableIndexT						GetRow() const {return mRow;};
	COfflinePickerView::RowType		GetRowType() const {return mRowType;};
	void *							GetInfo() const {return (mMessageFolder ? (void*)mMessageFolder->GetFolderInfo() : (void*)mDirServer);};

protected:
	const COfflinePickerView *		mPickerView;
	const TableIndexT				mRow;
	COfflinePickerView::RowType		mRowType;
	CMessageFolder *				mMessageFolder;
	DIR_Server *					mDirServer;
};

//	¥ COfflineItem
COfflineItem::COfflineItem(const TableIndexT inRow, const COfflinePickerView* inPickerView)
	: mPickerView(inPickerView)
	, mRow(inRow)
	, mMessageFolder(nil)
	, mDirServer(nil)
{
	mRowType = mPickerView->GetRowType(inRow);
	if (mRowType == COfflinePickerView::kRowMailNews)
		mMessageFolder = new CMessageFolder(inRow, inPickerView->GetMessagePane());
	else
		if (mRowType == COfflinePickerView::kRowLDAP)
		{
			TableIndexT outRows, outCols;
			mPickerView->GetTableSize(outRows, outCols);
			mDirServer = (DIR_Server *)XP_ListGetObjectNum(mPickerView->mLDAPList, outRows - mRow + 1);
		}
}

//	¥ ~COfflineItem
COfflineItem::~COfflineItem()
{
	delete mMessageFolder;
}

//	¥ GetLevel
UInt32	COfflineItem::GetLevel() const
{
	switch (mRowType)
	{
		case COfflinePickerView::kRowMailNews:	return mMessageFolder->GetLevel();
		case COfflinePickerView::kRowLDAPHdr:	return kRootLevel;
		case COfflinePickerView::kRowLDAP:		return kRootLevel + 1;
	}
	return kRootLevel;
}

//	¥ GetIconID
ResIDT COfflineItem::GetIconID() const
{
	switch (mRowType)
	{
		case COfflinePickerView::kRowMailNews:	return mMessageFolder->GetIconID();
		case COfflinePickerView::kRowLDAPHdr:	return kLDAPHdrIconID;
		case COfflinePickerView::kRowLDAP:		return kLDAPIconID;
	}
	return 0;
}

//	¥ GetName
const char* COfflineItem::GetName() const
{
	switch (mRowType)
	{
		case COfflinePickerView::kRowMailNews:	return mMessageFolder->GetName();
		case COfflinePickerView::kRowLDAPHdr:	return (char*)&mPickerView->mLDAPHdrStr[1];
		case COfflinePickerView::kRowLDAP:		return mDirServer->description;
	}
	return nil;
}

//	¥ GetFolderPrefFlags
UInt32 COfflineItem::GetFolderPrefFlags() const
{
	switch (mRowType)
	{
		case COfflinePickerView::kRowMailNews:	return mMessageFolder->GetFolderPrefFlags();
		case COfflinePickerView::kRowLDAPHdr:	return 0;
		case COfflinePickerView::kRowLDAP:		return (DIR_TestFlag(mDirServer, DIR_REPLICATION_ENABLED) ? MSG_FOLDER_PREF_OFFLINE : 0);
	}
	return 0;
}

//	¥ SetFolderPrefFlags
void COfflineItem::SetFolderPrefFlags(const UInt32 inPrefFlags) const
{
	switch (mRowType)
	{
		case COfflinePickerView::kRowMailNews:
			::MSG_SetFolderPrefFlags(mMessageFolder->GetFolderInfo(), inPrefFlags);
			break;
		case COfflinePickerView::kRowLDAPHdr:
			// nothing
			break;
		case COfflinePickerView::kRowLDAP:
			DIR_ForceFlag(mDirServer, DIR_REPLICATION_ENABLED, ((inPrefFlags & MSG_FOLDER_PREF_OFFLINE) != 0));
			break;
	}
}

//	¥ IsOpen
Boolean COfflineItem::IsOpen() const
{
	switch (mRowType)
	{
		case COfflinePickerView::kRowMailNews:	return mMessageFolder->IsOpen();
		case COfflinePickerView::kRowLDAPHdr:	return mPickerView->IsLDAPExpanded();
		case COfflinePickerView::kRowLDAP:		return false;
	}
	return false;
}

//	¥ IsNewsgroup
Boolean COfflineItem::IsNewsgroup() const
{
	return (mMessageFolder ? mMessageFolder->IsNewsgroup() : false);
}

//	¥ IsLocalMailFolder
Boolean COfflineItem::IsLocalMailFolder() const
{
	if (mRowType == COfflinePickerView::kRowMailNews)
		return mMessageFolder->IsLocalMailFolder();
	return false;
}

//	¥ IsIMAPMailFolder
Boolean COfflineItem::IsIMAPMailFolder() const
{
	return (mMessageFolder ? mMessageFolder->IsIMAPMailFolder() : false);
}

//	¥ IsLDAPDirectory
Boolean COfflineItem::IsLDAPDirectory() const
{
	return (mRowType == COfflinePickerView::kRowLDAP);
}

//	¥ HasNewMessages
Boolean COfflineItem::HasNewMessages() const
{
	return (mMessageFolder ? mMessageFolder->HasNewMessages() : false);
}

//	¥ CountSubFolders
UInt32 COfflineItem::CountSubFolders() const
{
	switch (mRowType)
	{
		case COfflinePickerView::kRowMailNews:	return mMessageFolder->CountSubFolders();
		case COfflinePickerView::kRowLDAPHdr:	return mPickerView->CountLDAPItems();
		case COfflinePickerView::kRowLDAP:		return 0;
	}
	return 0;

}


#pragma mark -
//------------------------------------------------------------------------------
//		¥ COfflinePickerView
//------------------------------------------------------------------------------
//
COfflinePickerView::COfflinePickerView(LStream *inStream)
		: Inherited(inStream)
		, mWantLDAP(true)
		, mLDAPList(nil)
		, mLDAPCount(0)
		, mLDAPExpanded(false)
		, mSaveItemArray(sizeof(SSaveItemRec))
{
}


//------------------------------------------------------------------------------
//		¥ ~COfflinePickerView
//------------------------------------------------------------------------------
//
COfflinePickerView::~COfflinePickerView()
{
	if (mLDAPList)
		XP_ListDestroy(mLDAPList);
}


//------------------------------------------------------------------------------
//		¥ View / GetRowType
//------------------------------------------------------------------------------
//
COfflinePickerView::RowType
		COfflinePickerView::GetRowType(TableIndexT inRow) const
{
	if (! mWantLDAP)
		return kRowMailNews;

	TableIndexT outRows, outCols;
	GetTableSize(outRows, outCols);

	TableIndexT ldapHdrRow;
	if (mLDAPExpanded)
		ldapHdrRow = outRows - mLDAPCount;
	else
		ldapHdrRow = outRows;

	if (inRow < ldapHdrRow)
		return kRowMailNews;
	else if (inRow == ldapHdrRow)
		return kRowLDAPHdr;
	else
		return kRowLDAP;
}


//------------------------------------------------------------------------------
//		¥ View / AppendLDAPList
//------------------------------------------------------------------------------
//
void COfflinePickerView::AppendLDAPList()
{
	if (!mWantLDAP)
		return;

	// get list (which also contains Personal ABook + HTML directories)
	XP_List *serverList = CAddressBookManager::GetDirServerList();
	if (!serverList)
	{
		mWantLDAP = false;
		return;
	}
	mLDAPList = XP_ListNew();
	ThrowIfNULL_(mLDAPList);

	// extract LDAP items
	int totalCount = XP_ListCount(serverList);
	for (Int32 i = 1; i <= totalCount ; i++)
	{
		DIR_Server *server = (DIR_Server *) XP_ListGetObjectNum(serverList, i);
		if (server->dirType == LDAPDirectory)
			XP_ListAddObject(mLDAPList, server);
	}
	mLDAPCount = XP_ListCount(mLDAPList);
	if (mLDAPCount == 0)
	{
		XP_ListDestroy(mLDAPList);
		mLDAPList = nil;
		mWantLDAP = false;
		return;
	}

	// get the LDAP header string and insert a row at the end of the list
	::GetIndString(mLDAPHdrStr, 7099, 26);
	mLDAPHdrStr[mLDAPHdrStr[0]+1] = '\0';

	TableIndexT outRows, outCols;
	GetTableSize(outRows, outCols);
	InsertRows(1, outRows, nil, 0, true);
}


//----------------------------------------------------------------------------
//		¥ View / SaveItemPrefFlags
//----------------------------------------------------------------------------
//
void	COfflinePickerView::SaveItemPrefFlags(const COfflineItem * inOfflineItem, UInt32 inPrefsFlags)
{
	// prepare new item to save
	SSaveItemRec	newItemRec;
	newItemRec.itemType = inOfflineItem->GetRowType();
	newItemRec.itemInfo = inOfflineItem->GetInfo();
	newItemRec.originalPrefsFlags = inPrefsFlags;

	// check if item has already been saved
	LArrayIterator iterator(mSaveItemArray, LArrayIterator::from_Start);
	SSaveItemRec itemRec;
	while (iterator.Next(&itemRec))
	{
		if ((itemRec.itemType == newItemRec.itemType)
			&& (itemRec.itemInfo == newItemRec.itemInfo))
			return;		// item already saved
	}

	// save new item
	mSaveItemArray.InsertItemsAt(1, LArray::index_Last, (void*)&newItemRec, sizeof(newItemRec));
}


//----------------------------------------------------------------------------
//		¥ View / CancelSelection
//----------------------------------------------------------------------------
//
void	COfflinePickerView::CancelSelection()
{
	LArrayIterator iterator(mSaveItemArray, LArrayIterator::from_Start);
	SSaveItemRec itemRec;
	while (iterator.Next(&itemRec))
	{
		switch (itemRec.itemType)
		{
			case COfflinePickerView::kRowMailNews:
				::MSG_SetFolderPrefFlags((MSG_FolderInfo *)itemRec.itemInfo, itemRec.originalPrefsFlags);
				break;

			case COfflinePickerView::kRowLDAP:
				DIR_ForceFlag((DIR_Server *)itemRec.itemInfo, DIR_REPLICATION_ENABLED, ((itemRec.originalPrefsFlags & MSG_FOLDER_PREF_OFFLINE) != 0));
				DIR_SaveServerPreferences(CAddressBookManager::GetDirServerList());
				break;
		}
	}
	mSaveItemArray.RemoveItemsAt(1, mSaveItemArray.GetCount());
}


//----------------------------------------------------------------------------
//		¥ View / CommitSelection
//----------------------------------------------------------------------------
//
void	COfflinePickerView::CommitSelection()
{
	if (mLDAPList)
		DIR_SaveServerPreferences(CAddressBookManager::GetDirServerList());
}


//----------------------------------------------------------------------------
//		¥ View / DrawCell
//----------------------------------------------------------------------------
//
void COfflinePickerView::DrawCell(const STableCell& inCell, const Rect& inLocalRect)
{
	PaneIDT	cellType = GetCellDataType(inCell);
	switch (cellType)
	{
		case kFolderNameColumn:
			Inherited::DrawCell(inCell, inLocalRect);
			break;

		case kSelectFolderColumn:
			COfflineItem item(inCell.row, this);
			if (item.GetLevel() > kRootLevel)
			{
				short iconID, transformType;
				if (item.IsLocalMailFolder())
				{
					iconID = kCheckedIconID;
					transformType = ttDisabled;
				}
				else
				{
					UInt32 folderPrefFlags = item.GetFolderPrefFlags();
					if (folderPrefFlags & MSG_FOLDER_PREF_OFFLINE)
						iconID = kCheckedIconID;
					else
						iconID = kUncheckedIconID;
					transformType = ttNone;
				}
				DrawIconFamily(iconID, 16, 16, transformType, inLocalRect);
			}
			break;
	}
}


//----------------------------------------------------------------------------
//		¥ View / ClickSelect
//----------------------------------------------------------------------------
//	We don't want any fancy behavior on mouse clicks: no cell selection,
//	no Finder selection, nothing. Just toggle the check-box.
//
 Boolean	COfflinePickerView::ClickSelect(
				const STableCell		&inCell,
				const SMouseDownEvent	&inMouseDown)
{
#pragma unused (inCell)
#pragma unused (inMouseDown)
	return true;
}


//----------------------------------------------------------------------------
//		¥ View / ClickCell
//----------------------------------------------------------------------------
//	We don't want any fancy behavior on mouse clicks: no cell selection,
//	no Finder selection, nothing. Just toggle the check-box.
//
void	COfflinePickerView::ClickCell(
			const STableCell	&inCell,
			const SMouseDownEvent &inMouseDown)
{
	SPoint32	currentPoint;
	STableCell	hitCell = inCell;

	currentPoint.h = inMouseDown.whereLocal.h;
	currentPoint.v = inMouseDown.whereLocal.v;

	COfflineItem item(hitCell.row, this);
	if (item.GetLevel() > kRootLevel)
	{
		if (item.IsNewsgroup() || item.IsIMAPMailFolder() || item.IsLDAPDirectory())
		{
			UInt32 folderPrefFlags = item.GetFolderPrefFlags();
			SaveItemPrefFlags(&item, folderPrefFlags);

			folderPrefFlags ^= MSG_FOLDER_PREF_OFFLINE;
			item.SetFolderPrefFlags(folderPrefFlags);

			for (int i = 1; i <= mTableHeader->CountVisibleColumns(); i++)
			{
				hitCell.col = i;
				RefreshCell(hitCell);
			}
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ View / HandleKeyPress
// ---------------------------------------------------------------------------
//	Overide CStandardFlexTable: Return and Enter don't open the selection.
//

Boolean
COfflinePickerView::HandleKeyPress(
	const EventRecord	&inKeyEvent)
{
	Boolean		keyHandled = false;
	LControl	*keyButton = nil;
	
	switch (inKeyEvent.message & charCodeMask)
	{
		case char_Enter:
		case char_Return:
			LCommander::HandleKeyPress(inKeyEvent);
			return true;
	}
			
	return Inherited::HandleKeyPress(inKeyEvent);
}


//----------------------------------------------------------------------------
//		¥ View / GetQapRowText
//----------------------------------------------------------------------------
//	Return info for QA Partner
//
#if defined(QAP_BUILD)		
void COfflinePickerView::GetQapRowText(
	TableIndexT			inRow,
	char*				outText,
	UInt16				inMaxBufferLength) const
{
	if (!outText || inMaxBufferLength == 0)
		return;

	cstring rowText("");
	short colCount = mTableHeader->CountVisibleColumns();
	COfflineItem item(inRow, this);

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
			case kFolderNameColumn:
				Boolean isExpanded;
				if (CellHasDropFlag(aCell, isExpanded))
				{
					if (isExpanded)
						rowText += "-";
					else
						rowText += "+";
				}
				else
					rowText += " ";
				rowText += item.GetName();
				break;

			case kSelectFolderColumn:
				if (item.GetLevel() > kRootLevel)
				{
					UInt32 folderPrefFlags = item.GetFolderPrefFlags();
					if (folderPrefFlags & MSG_FOLDER_PREF_OFFLINE)
						rowText += "+";
				}
				break;
		}

		if (col < colCount)
			rowText += "\042 | ";
		else
			rowText += "\042\r";
	}
	strncpy(outText, (char*)rowText, inMaxBufferLength);
	outText[inMaxBufferLength - 1] = '\0';
}
#endif //QAP_BUILD


#pragma mark --
//------------------------------------------------------------------------------
//		¥ View / GetIconID
//------------------------------------------------------------------------------
//
ResIDT COfflinePickerView::GetIconID(TableIndexT inRow) const
{
	COfflineItem item(inRow, this);
	return item.GetIconID();
}


//------------------------------------------------------------------------------
//		¥ View / GetNestedLevel
//------------------------------------------------------------------------------
//
UInt16 COfflinePickerView::GetNestedLevel(TableIndexT inRow) const
{
	COfflineItem item(inRow, this);
	return item.GetLevel() - 1;
}


//------------------------------------------------------------------------------
//		¥ View / ApplyTextStyle
//------------------------------------------------------------------------------
//
void COfflinePickerView::ApplyTextStyle(TableIndexT inRow) const
{
	COfflineItem item(inRow, this);
	::TextFace(item.HasNewMessages() ? bold : normal);
}


//------------------------------------------------------------------------------
//		¥ View / CellHasDropFlag
//------------------------------------------------------------------------------
//	Check if a cell has a twistee icon and if the twistee is open.
//
Boolean COfflinePickerView::CellHasDropFlag(
		const STableCell& 	inCell, 
		Boolean& 			outIsExpanded) const
{
	COfflineItem item(inCell.row, this);
	if (GetCellDataType(inCell) == kFolderNameColumn && item.CountSubFolders() != 0) 
	{
		outIsExpanded = item.IsOpen();	
		return true;
	}
	return false;
}


//------------------------------------------------------------------------------
//		¥ View / SetCellExpansion
//------------------------------------------------------------------------------
//
void COfflinePickerView::SetCellExpansion(
		const STableCell&	inCell, 
		Boolean				inExpand)
{
	switch (GetRowType(inCell.row))
	{
		case kRowMailNews:
			Inherited::SetCellExpansion(inCell, inExpand);
			break;

		case kRowLDAPHdr:
			mLDAPExpanded = inExpand;
			TableIndexT outRows, outCols;
			GetTableSize(outRows, outCols);
			if (inExpand)
				InsertRows(mLDAPCount, outRows, nil, 0, true);
			else
				RemoveRows(mLDAPCount, outRows - mLDAPCount, true);
			break;

		case kRowLDAP:
			break;
	}
}


//------------------------------------------------------------------------------
//		¥ View / GetMainRowText
//------------------------------------------------------------------------------
//
void COfflinePickerView::GetMainRowText(
		TableIndexT		inRow,
		char*			outText,
		UInt16			inMaxBufferLength) const
{
	switch (GetRowType(inRow))
	{
		case kRowMailNews:
			Inherited::GetMainRowText(inRow, outText, inMaxBufferLength);
			break;

		case kRowLDAPHdr:
		case kRowLDAP:
			if (outText)
			{
				COfflineItem item(inRow, this);
				::strncpy(outText, item.GetName(), inMaxBufferLength);
			}
			break;
	}
}


//------------------------------------------------------------------------------
//		¥ View / ChangeFinished
//------------------------------------------------------------------------------
//

void COfflinePickerView::ChangeFinished(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount)
{
	// call the inherited method, restoring the original BE row count
	// to work around a check in CMailFlexTable::ChangeFinished()
	// on MSG_NotifyInsertOrDelete
	if ((mLDAPCount > 0) && (inChangeCode == MSG_NotifyInsertOrDelete))
	{
		mRows -= (mLDAPExpanded ? mLDAPCount + 1 : 1);
		Inherited::ChangeFinished(inPane, inChangeCode, inStartRow, inRowCount);
		mRows += (mLDAPExpanded ? mLDAPCount + 1 : 1);
	}
	else
		Inherited::ChangeFinished(inPane, inChangeCode, inStartRow, inRowCount);

	switch (inChangeCode)
	{
		case MSG_NotifyScramble:
		case MSG_NotifyAll:
			if (mWantLDAP)
			{
				COfflineItem item(1, this);
				if (item.IsLocalMailFolder() && item.IsOpen())
				{
					STableCell aCell(1, 1);
					SetCellExpansion(aCell, false);
				}
				AppendLDAPList();
			}
			break;
	}
}


#pragma mark -

//------------------------------------------------------------------------------
//		¥ COfflinePickerWindow
//------------------------------------------------------------------------------
//
COfflinePickerWindow::COfflinePickerWindow(LStream *inStream)
		:	CMailNewsWindow(inStream, WindowType_OfflinePicker)
		,	mList(nil)
{
}


//------------------------------------------------------------------------------
//		¥ ~COfflinePickerWindow
//------------------------------------------------------------------------------
//
COfflinePickerWindow::~COfflinePickerWindow()
{
}


//------------------------------------------------------------------------------
//		¥ Window / FinishCreateSelf
//------------------------------------------------------------------------------
//
void COfflinePickerWindow::FinishCreateSelf()
{
	Inherited::FinishCreateSelf();
	mList = (COfflinePickerView*)GetActiveTable();
	Assert_(mList);
	mList->LoadFolderList(mMailNewsContext);

	UReanimator::LinkListenerToControls(this, this, GetPaneID());

	LGAPushButton * okBtn = dynamic_cast<LGAPushButton*>(FindPaneByID(paneID_OkButton));
	if (okBtn)	okBtn->SetDefaultButton(true, true);

	Show();
	Select();
}


//------------------------------------------------------------------------------
//		¥ Window / CalcStandardBoundsForScreen
//------------------------------------------------------------------------------
//	Zoom in the vertical direction only. 
//
void COfflinePickerWindow::CalcStandardBoundsForScreen(
		const Rect 		&inScreenBounds,
		Rect 			&outStdBounds) const
{
	LWindow::CalcStandardBoundsForScreen(inScreenBounds, outStdBounds);
	Rect	contRect = UWindows::GetWindowContentRect(mMacWindowP);

	outStdBounds.left = contRect.left;
	outStdBounds.right = contRect.right;
}


//----------------------------------------------------------------------------
//		¥ Window / ListenToMessage
//----------------------------------------------------------------------------

void COfflinePickerWindow::ListenToMessage(MessageT inMessage, void* ioParam)
{
#pragma unused (ioParam)
	switch (inMessage)
	{
		case msg_Cancel:
			if (mList)
				mList->CancelSelection();
			Inherited::DoClose();
			break;

		case msg_OK:
			if (mList)
				mList->CommitSelection();
			Inherited::DoClose();
			break;
	}
}


// ---------------------------------------------------------------------------
//		¥ Window / HandleKeyPress
// ---------------------------------------------------------------------------
//	As usual, copied and adapted from LDialogBox.
//	Can't we do an attachment with that?
//

Boolean
COfflinePickerWindow::HandleKeyPress(
	const EventRecord	&inKeyEvent)
{
	Boolean		keyHandled = false;
	LControl	*keyButton = nil;
	
	switch (inKeyEvent.message & charCodeMask) {
	
		case char_Enter:
		case char_Return:
			keyButton = (LControl*) FindPaneByID(paneID_OkButton);
			break;
			
		case char_Escape:
			if ((inKeyEvent.message & keyCodeMask) == vkey_Escape) {
				keyButton =  (LControl*) FindPaneByID(paneID_CancelButton);
			}
			break;

		default:
			if (UKeyFilters::IsCmdPeriod(inKeyEvent)) {
				keyButton =  (LControl*) FindPaneByID(paneID_CancelButton);
			} else {
				keyHandled = LWindow::HandleKeyPress(inKeyEvent);
			}
			break;
	}
			
	if (keyButton != nil) {
		keyButton->SimulateHotSpotClick(kControlButtonPart);
		keyHandled = true;
	}
	
	return keyHandled;
}


//------------------------------------------------------------------------------
//		¥ Window / GetActiveTable
//------------------------------------------------------------------------------
//	From CMailNewsWindow. Get the currently active table in the window.
//	The active table is the one that the user considers to be receiving input.
//
CMailFlexTable* COfflinePickerWindow::GetActiveTable()
{
	return dynamic_cast<COfflinePickerView*>(FindPaneByID('Flst'));
}


//------------------------------------------------------------------------------
//		¥ Window / FindAndShow								[static]
//------------------------------------------------------------------------------
//	Creates/shows/selects the Offline Picker window. There can only be one of these.
//	Use COfflinePickerWindow::DisplayDialog() for a modal dialog.
//
COfflinePickerWindow* COfflinePickerWindow::FindAndShow(Boolean inMakeNew)
{
	COfflinePickerWindow* result = NULL;
	try
	{
		CWindowIterator iter(WindowType_OfflinePicker);
		iter.Next(result);
		if (!result && inMakeNew)
		{
			result = dynamic_cast<COfflinePickerWindow*>(
						URobustCreateWindow::CreateWindow(
							res_ID, LCommander::GetTopCommander()));
			ThrowIfNULL_(result);
		}
	}
	catch (...)
	{
	}
	return result;
}


//------------------------------------------------------------------------------
//		¥ Window / DisplayDialog							[static]
//------------------------------------------------------------------------------
//	Creates/shows/selects the Offline Picker window. There can only be one of these.
//	Use COfflinePickerWindow::FindAndShow() for a non-modal window.
//
Boolean COfflinePickerWindow::DisplayDialog()
{
	StDialogHandler handler(res_ID, NULL);

	MessageT message;
	do
	{
		message = handler.DoDialog();
	} while (message != msg_OK && message != msg_Cancel);
	
	return (message == msg_OK);
}
