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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


// CNameCompletionPicker.cp

#include "abcom.H"
#ifdef MOZ_NEWADDR
#include "CNameCompletionPicker.h"
#include "UStdDialogs.h"
#include "SearchHelpers.h"
#include "LGAPushButton.h"

LEditField*			CNameCompletionPicker::mLastReceivedEditField	= nil;
MSG_Pane*			CNameCompletionPicker::mLastReceivedPickerPane	= nil;
CMailNewsContext*	CNameCompletionPicker::mLastReceivedContext		= nil;
int					CNameCompletionPicker::mLastReceivedNumResults	= 0;


//----------------------------------------------------------------------------
//		¥ CNameCompletionTable class								[static]
//----------------------------------------------------------------------------
//
CNameCompletionTable::CNameCompletionTable(LStream* inStream )
	:	CMailFlexTable(inStream)
{
}


CNameCompletionTable::~CNameCompletionTable()
{
	SetMessagePane(NULL);
		// Do it here so that our DestroyMessagePane() is called.
		// If we let the inherited CMailFlexTable do it, it will call
		// its own DestroyMessagePane() because ours is already destroyed
		// and the PickerPane will be deleted (which is something we
		// don't want because it belongs to the CMailAddressEditField).
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionTable::DestroyMessagePane
//----------------------------------------------------------------------------
//	Don't delete the MSG_PickerPane: it belongs to the CMailAddressEditField
//
void CNameCompletionTable::DestroyMessagePane(MSG_Pane* /*inPane*/)
{
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionTable::SetColumnHeaders
//----------------------------------------------------------------------------
//
void CNameCompletionTable::SetColumnHeaders()
{
		LTableViewHeader*	tableHeader = GetTableHeader();
		PaneIDT				headerPaneID;

	// Column #1 = 'Type' is the container type (Address Book / LDAP server).
	// Column #2 = 'Col0' is the entry type (User / Mailing list).
	// The other columns have configurable headers.
	for (short col = 2; col <= tableHeader->CountColumns(); col ++)
	{
		headerPaneID = tableHeader->GetColumnPaneID(col);
		Int32 index = headerPaneID - eTableHeaderBase;
		AB_ColumnInfo *info = AB_GetColumnInfoForPane(GetMessagePane(), AB_ColumnID(index));

		LCaption* headerPane = dynamic_cast<LCaption*>(GetSuperView()->FindPaneByID(headerPaneID));
		if (headerPane) headerPane->SetDescriptor(CStr255(info->displayString));

		AB_FreeColumnInfo(info);
	}
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionTable::SelectionChanged
//----------------------------------------------------------------------------
//
void CNameCompletionTable::SelectionChanged()
{
	Inherited::SelectionChanged();
	StartBroadcasting();
		// Long story: the CTableKeyAttachment from CStandardFlexTable
		// calls StopBroadcasting() on keyDown and StartBroadcasting()
		// on keyUp. Problem: we are in a dialog and we never get keyUp's.
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionTable::OpenRow
//----------------------------------------------------------------------------
//
void CNameCompletionTable::OpenRow(TableIndexT inRow)
{
	if (IsValidRow(inRow))
	{
		BroadcastMessage(msg_OK, nil);
	}
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionTable::GetCellDisplayData
//----------------------------------------------------------------------------
//	Entirely copied from CMailingListTableView
//
void CNameCompletionTable::GetCellDisplayData(const STableCell &inCell, ResIDT& ioIcon, CStr255 &ioDisplayString)
{
	ioIcon = 0;
	ioDisplayString = "";

	// Get the attribute corresponding to the current column
	LTableViewHeader* tableHeader = GetTableHeader();
	PaneIDT headerPaneID = tableHeader->GetColumnPaneID(inCell.col);
	Int32 index = headerPaneID - eTableHeaderBase;

	AB_ColumnInfo *info = AB_GetColumnInfoForPane(GetMessagePane(), AB_ColumnID(index));
	AB_AttribID attrib = info->attribID;
	AB_FreeColumnInfo(info);

	// Get the data
	uint16 numItems = 1;
	AB_AttributeValue* value;
	if (AB_GetEntryAttributesForPane(GetMessagePane(), inCell.row-1, &attrib, &value, &numItems) == AB_SUCCESS)
	{ 
		if (attrib == AB_attribEntryType)
		{
			ioIcon = (value->u.entryType == AB_MailingList ? 15263 : 15260);
		}
		else
		{
			ioDisplayString = value->u.string ;
		}
		AB_FreeEntryAttributeValue(value);
	}
} 


//----------------------------------------------------------------------------
//		¥ CNameCompletionTable::DrawCellContents
//----------------------------------------------------------------------------
//	Mostly copied from CMailingListTableView
//
void CNameCompletionTable::DrawCellContents(const STableCell &inCell, const Rect &inLocalRect)
{
	ResIDT iconID = 0;
	PaneIDT cellType = GetCellDataType(inCell);
	switch (cellType)
	{
		case 'Type':
			AB_ContainerType container = AB_GetEntryContainerType(GetMessagePane(), inCell.row-1);
			switch (container)
			{
				case AB_LDAPContainer:		iconID = 15250;		break;	// remote folder
				case AB_MListContainer:		iconID = 15258;		break;	// address book
				case AB_PABContainer:		iconID = 15258;		break;	// address book
				case AB_UnknownContainer:	iconID = 0;			break;
			}
			if (iconID)
				DrawIconFamily(iconID, 16, 16,  0, inLocalRect);
			break;
//		case 'Col0':
//		...
//		case 'Col6':
		default:
			CStr255 displayString;
			GetCellDisplayData(inCell, iconID, displayString);
			if (iconID)
				DrawIconFamily(iconID, 16, 16, kTransformNone, inLocalRect);
			else
				DrawTextString(displayString, &mTextFontInfo, 0, inLocalRect);
			break;
	}
}


#pragma mark -
//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker::DisplayDialog						[static]
//----------------------------------------------------------------------------
//	Show the name completion dialog
//
int CNameCompletionPicker::DisplayDialog(LEditField* inEditField, MSG_Pane* inPane, CMailNewsContext* inContext, int inNumResults)
{
	// put up dialog
	mLastReceivedEditField	= inEditField;
	mLastReceivedPickerPane	= inPane;
	mLastReceivedContext	= inContext;
	mLastReceivedNumResults	= inNumResults;

	RegisterClass_(CNameCompletionPicker);
	RegisterClass_(CNameCompletionTable);

	StStdDialogHandler handler(CNameCompletionPicker::res_ID, NULL);
	CNameCompletionPicker* dlog = (CNameCompletionPicker*)handler.GetDialog();

	// run the dialog
	MessageT message;
	do
	{
		message = handler.DoDialog();
	} while (message != msg_OK && message != msg_Cancel);
	
	// return the result
	STableCell aCell(0,0);
	if (message == msg_OK)
		aCell = dlog->GetActiveTable()->GetFirstSelectedCell();

	// explicitly close the dialog to save its status
	dlog->DoClose();

	return (aCell.row);
}


#pragma mark -
//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker class
//----------------------------------------------------------------------------
//
CNameCompletionPicker::CNameCompletionPicker(LStream *inStream)
	:	CMailNewsWindow(inStream, WindowType_NameCompletion)
{
}

CNameCompletionPicker::~CNameCompletionPicker()
{
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker::FinishCreateSelf
//----------------------------------------------------------------------------
//
void CNameCompletionPicker::FinishCreateSelf()
{
	// transmit parameters from the Compose window to the list
	ReceiveComposeWindowParameters();

	mNameCompletionTable = dynamic_cast<CNameCompletionTable *>(USearchHelper::FindViewSubview(this, paneID_NameCompletionTable));
	FailNILRes_(mNameCompletionTable);
	mNameCompletionTable->ReceiveMessagePane(mPickerPane);

	// finish create stuff
	Inherited::FinishCreateSelf();
	CSaveWindowStatus::FinishCreateWindow();

	// prepare list
	mNameCompletionTable->SetColumnHeaders();
	mNameCompletionTable->SetRowCount();
	STableCell cellToSelect(2, 1);
	mNameCompletionTable->SelectCell(cellToSelect);

	// default button
	LGAPushButton * defaultBtn = dynamic_cast<LGAPushButton*>(FindPaneByID(paneID_OkButton));
	if (defaultBtn) defaultBtn->SetDefaultButton(true, true);
	mNameCompletionTable->AddListener(this);

	// show window
	this->Show();
	this->Select();
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker::DoClose
//----------------------------------------------------------------------------
//
void CNameCompletionPicker::DoClose()
{
	// Save table data and window position
	SaveStatusInfo();

	// Close window
	Inherited::DoClose();
}

//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker::CalcStandardBoundsForScreen
//----------------------------------------------------------------------------
//	Zoom in the vertical direction only. 
//
void	CNameCompletionPicker::CalcStandardBoundsForScreen(
		const Rect &inScreenBounds,
		Rect &outStdBounds) const
{
	LWindow::CalcStandardBoundsForScreen(inScreenBounds, outStdBounds);
	Rect	contRect = UWindows::GetWindowContentRect(mMacWindowP);

	outStdBounds.left = contRect.left;
	outStdBounds.right = contRect.right;
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker::ListenToMessage
//----------------------------------------------------------------------------
//
void CNameCompletionPicker::ListenToMessage(MessageT inMessage, void */*ioParam*/)
{
	if (inMessage == msg_OK)
	{
		LControl* keyButton = (LControl*)FindPaneByID(paneID_OkButton);
		keyButton->SimulateHotSpotClick(kControlButtonPart);
	}
}


//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker::HandleKeyPress
//----------------------------------------------------------------------------
//	Handle Escape and Cmd-Period (copied from LDialogBox)
//
Boolean CNameCompletionPicker::HandleKeyPress(const EventRecord &inKeyEvent)
{
	Boolean		keyHandled	= false;
	PaneIDT		keyButtonID	= 0;
	
	switch (inKeyEvent.message & charCodeMask)
	{
		case char_Enter:
		case char_Return:
			keyButtonID = paneID_OkButton;
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
//		¥ CNameCompletionPicker::ReadWindowStatus
//----------------------------------------------------------------------------
//	Put the window next to the edit field and try to restore its last size.
//
void CNameCompletionPicker::ReadWindowStatus(LStream *inStatusData)
{
	// get last window size
	Rect savedBounds;
	if (inStatusData != nil)
		*inStatusData >> savedBounds;
	else
	{
		GetPaneGlobalBounds(this, &savedBounds);
		savedBounds.right = savedBounds.left + 320;	//¥ TODO: remove these hard-coded values
	}

	// put the window at the right of the caret position in the edit field
	TEHandle teH = mEditField->GetMacTEH();
	short caretPos = (*teH)->selStart;

	Rect actualBounds;
	mEditField->CalcPortFrameRect(actualBounds);
	mEditField->PortToGlobalPoint(topLeft(actualBounds));
	actualBounds.top -= 44;						//¥ TODO: remove these hard-coded values
	actualBounds.left += (caretPos + 3) * 7;	//¥ TODO:
	actualBounds.bottom = actualBounds.top + (savedBounds.bottom - savedBounds.top);
	actualBounds.right = actualBounds.left + (savedBounds.right - savedBounds.left);

	VerifyWindowBounds(this, &actualBounds);
	DoSetBounds(actualBounds);

	// restore table data
	if (inStatusData != nil)
		mNameCompletionTable->ReadSavedTableStatus(inStatusData);
}

//----------------------------------------------------------------------------
//		¥ CNameCompletionPicker::WriteWindowStatus
//----------------------------------------------------------------------------
//	Save window size and table data.
//
void CNameCompletionPicker::WriteWindowStatus(LStream *outStatusData)
{
	CSaveWindowStatus::WriteWindowStatus(outStatusData);
	mNameCompletionTable->WriteSavedTableStatus(outStatusData);
}
#endif //MOZ_NEWADDR
