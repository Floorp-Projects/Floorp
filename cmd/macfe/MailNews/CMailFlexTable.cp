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


#include "CMailFlexTable.h"

// PP
#include <UException.h>
#include <UModalDialogs.h>

#include "LFlexTableGeometry.h"
//#include "LTableRowSelector.h"

// Want a command number?  They hide in several places...
#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"
#include "UMessageLibrary.h"


#include "CPrefsDialog.h"
#include "CNSContext.h"
#include "UMailSelection.h"

#include "uapp.h" // for UpdateMenus().  Ugh.
#include "CPaneEnabler.h"
#include "CMailNewsContext.h"
#include "UOffline.h"
#include "CBookmarksAttachment.h"
#include "CMailProgressWindow.h"
#include "macutil.h"
#include "prefapi.h"

// XP
#include "shist.h"

//-----------------------------------
CMailFlexTable::CMailFlexTable(LStream *inStream)
//-----------------------------------
:	Inherited(inStream)
,	mMsgPane(NULL)
,	mMysticPlane(0)
,	mStillLoading(false)
,	mContext(nil)
,	mFirstRowToRefresh(0)
,	mLastRowToRefresh(0)
,	mClosing(false)
{
	*inStream >> mDragFlavor;
} // CMailFlexTable::CMailFlexTable

//-----------------------------------
CMailFlexTable::~CMailFlexTable()
//-----------------------------------
{
	mClosing = true;
	SetMessagePane(NULL);
} // CMailFlexTable::~CMailFlexTable

//-----------------------------------
void CMailFlexTable::DrawSelf()
//-----------------------------------
{
	ApplyForeAndBackColors();

	// This function is similar to what we had when the "Erase On Update"
	// LWindow attribute was set in Constructor. This flag has been removed
	// because it created a lot of flickers when browsing mails.
	// The other objects in the Thread window continued to behave correctly
	// but the CThreadView showed some update problems. Instead of fixing
	// them as we are supposed to (ie. by invalidating and erasing only what
	// needs to be redrawn), I prefered to emulate the way it used to work
	// when "Erase On Update" was set. My apologies for this easy solution
	// but we have something to ship next week.

	// OK, I made it better by only erasing what was below the last cell (if anything).
	// jrm 97/08/18

	const STableCell bottomCell(mRows, 1);
	Int32 cellLeft, cellTop, cellRight, cellBottom;
	mTableGeometry->GetImageCellBounds(bottomCell, cellLeft, cellTop,
							cellRight, cellBottom);
	// Convert from image coordinates to port coordinates
	cellBottom += mImageLocation.v;
	Int32 frameBottom = mFrameLocation.v + mFrameSize.height;
	if (cellBottom < frameBottom) // Note the "=", edge case for deleting the last row.
	{
		// erase everything
		Rect frame;
		CalcLocalFrameRect(frame);
		frame.top = frame.bottom - (frameBottom - cellBottom);
		::EraseRect(&frame);
	}
	
	// redraw everything
	Inherited::DrawSelf();
} // CMailFlexTable::DrawSelf

//-----------------------------------
void CMailFlexTable::DestroyMessagePane(MSG_Pane* inPane )
//-----------------------------------
{
	if (mContext)
		XP_InterruptContext((MWContext*)*mContext);
	CMailCallbackListener::SetPane(nil); // turn off callbacks
	if ( GetMessagePane() != NULL )
		::MSG_DestroyPane( inPane);
}// CMailFlexTable::DestroyMessagePane

//-----------------------------------
void CMailFlexTable::SetMessagePane(MSG_Pane* inPane)
//-----------------------------------
{
	DestroyMessagePane( mMsgPane );
	mMsgPane = inPane;
	CMailCallbackListener::SetPane(inPane);
}

//-----------------------------------
void CMailFlexTable::SetRowCount()
// Queries the back end pane and sets the number of rows.
//-----------------------------------
{
	TableIndexT rows, cols;
	GetTableSize(rows, cols);	
	SInt32 diff = mMsgPane ? (::MSG_GetNumLines(mMsgPane) - rows) : -rows;
	if (diff > 0)
		InsertRows(diff, 1, NULL, 0, false);
	else if (diff < 0)
		RemoveRows(-diff, 1, false);
	
} // CMailFlexTable::SetRowCount()

#if 0
//======================================
class StProgressWindowHandler : public StDialogHandler
//======================================
{
private:
	typedef StDialogHandler Inherited;
public:
};
#endif

static Boolean gCanceled = false;
static Boolean CancelCallback()
{
	gCanceled = true;
	return false;
}
//-----------------------------------
void CMailFlexTable::OpenSelection()
// Overrides the base class, in order to show progress.
//-----------------------------------
{
	TableIndexT total = GetSelectedRowCount();
	if (!total)
		return;
	if (total < 10)
	{
		Inherited::OpenSelection();
		return;
	}

	TableIndexT selectedRow = 0;
	StDialogHandler handler(CMailProgressWindow::res_ID_modal, LCommander::GetTopCommander());
	CMailProgressWindow* pw = dynamic_cast<CMailProgressWindow*>(handler.GetDialog());
	if (!pw)
		throw memFullErr;
	
	CStr255 description;
	::GetIndString(description, 7099, 17);
	pw->SetCancelCallback(CancelCallback);
	CContextProgress progress;
	progress.mAction = description;
	progress.mTotal = total;
	progress.mInitCount = 0;
	progress.mPercent = 0;
	progress.mRead = 0;
	progress.mStartTime = ::TickCount();
	gCanceled = false;
	pw->Show();
	pw->UpdatePort();
	pw->ListenToMessage(msg_NSCProgressBegin, &progress);
	while (GetNextSelectedRow(selectedRow) && !gCanceled)
	{
		// Handle plenty of events - activates, updates coming on strong...
		for (int i = 1; i <= 20; i++)
		{
			handler.DoDialog();
			if (gCanceled)
				break;
		}
		OpenRow(selectedRow);
		::GetIndString(description, 7099, 18);
		::StringParamText(description, ++progress.mRead, progress.mTotal, 0, 0);
		progress.mPercent = ((Int32)(progress.mRead) * 100/ total);
//		progress.mMessage = description;
//		pw->ListenToMessage(msg_NSCProgressUpdate, &progress);
//		Someone turned off support for msg_NSCProgressUpdate, so:
		pw->ListenToMessage(msg_NSCProgressMessageChanged, (char*)description);
		pw->ListenToMessage(msg_NSCProgressPercentChanged, &progress.mPercent);
	}
	// No.  Handler will delete!  pw->ListenToMessage(msg_NSCAllConnectionsComplete, nil);
} // CMailFlexTable::OpenSelection

//-----------------------------------
Boolean CMailFlexTable::GetSelection(CMailSelection& selection)
// CStandardFlexTable has one-based lists which are lists of TableIndexT.
// CMailSelection requires zero-based lists which are lists of MSG_ViewIndex
// This routine clones and converts.
//-----------------------------------
{
	selection.xpPane = mMsgPane;
	// Assert, cuz we're going to cast and to convert an array in place!
	Assert_(sizeof(TableIndexT) == sizeof(MSG_ViewIndex));
	// if we've got a selection list, it's assumed up to date,
	// so DON't convert it to a MSG_Index.  When the selection changes, we just get
	// rid of it (in SelectionChanged()) and set mSelectionList to NULL
	Boolean usingCachedSelection = (mSelectionList != NULL);
	selection.selectionList
		= (MSG_ViewIndex*)Inherited::GetUpdatedSelectionList(
			(TableIndexT&)selection.selectionSize);
	if (!selection.selectionList)
		return false;
	if (usingCachedSelection)
		return true;
	// Have selection, not cached, so convert in place from 1-based to 0-based
	MSG_ViewIndex* index = selection.selectionList;
	for (TableIndexT i = 0; i < selection.selectionSize; i++, index++)
		(*index)--;
	return true;
} // CMailFlexTable::GetSelection

//-----------------------------------
const TableIndexT* CMailFlexTable::GetUpdatedSelectionList(TableIndexT& /*outSelectionSize*/)
// This override is here to stop people calling it.  Mail table requires a different type
// for the index and a zero-based one at that!
//-----------------------------------
{
	Assert_(false); // Use GetSelection() instead.
	return NULL;
} // CMailFlexTable::GetUpdatedSelectionList

//----------------------------------------------------------------------------------------
void CMailFlexTable::AddSelectionToDrag(
	DragReference	inDragRef,
	RgnHandle		inDragRgn)
//	Adds a single drag item, which is an array of the 
//	selected row indices.	
//	Throws drag manager errors.
//----------------------------------------------------------------------------------------
{
	Inherited::AddSelectionToDrag(inDragRef, inDragRgn);
		
	// Our drag data is just a pointer to a list of our selected items
	// Danger: the list changes when the selection changes,
	// so this pointer's lifetime is limited.
	CMailSelection selection;
	if (GetSelection(selection))
	{
		mDragFlavor = kMailNewsSelectionDragFlavor;
		OSErr err = ::AddDragItemFlavor(inDragRef, eMailNewsSelectionDragItemRefNum,
						mDragFlavor, &selection, sizeof(selection), flavorSenderOnly);
		ThrowIfOSErr_(err);
	}
} // CMailFlexTable::AddSelectionToDrag

//----------------------------------------------------------------------------------------
void CMailFlexTable::AddRowToDrag(
	TableIndexT		inRow,
	DragReference	inDragRef,
	RgnHandle		inDragRgn)
//	98/04/03 added to support dragging of an unselected item.
//	Adds a single drag item, which is an array of ONE row index, probably not currently
//	selected!
//	Throws drag manager errors.
//----------------------------------------------------------------------------------------
{
	if (inRow == LArray::index_Bad)
		return;
	Inherited::AddRowToDrag(inRow, inDragRef, inDragRgn);
		
	// Our drag data is just a pointer to a pseudo selection 
	// Danger: the list changes when the selection changes,
	// so this pointer's lifetime is limited.
	CMailSelection selection;
	// Subtract 1 to make a MSG_ViewIndex (0-based) from the TableIndexT (1-based)
	selection.xpPane = mMsgPane;
	selection.SetSingleSelection(inRow - 1);
	mDragFlavor = kMailNewsSelectionDragFlavor;
	OSErr err = ::AddDragItemFlavor(inDragRef, eMailNewsSelectionDragItemRefNum,
							mDragFlavor, &selection, sizeof(selection), flavorSenderOnly);
	ThrowIfOSErr_(err);
} // CMailFlexTable::AddSelectionToDrag

//----------------------------------------------------------------------------------------
Boolean CMailFlexTable::GetSelectionFromDrag(
	DragReference	inDragRef,
	CMailSelection&	outSelection)
//	Get the selection back out from the drag data.
//	NOTE: this is called by the DESTINATION pane of the drop.
//	The only flavor we need is kMailNewsSelectionDragFlavor
//----------------------------------------------------------------------------------------
{
	Size dataSize;
	dataSize = sizeof(CMailSelection);
	if (noErr != ::GetFlavorData(
			inDragRef,
			eMailNewsSelectionDragItemRefNum, 
			kMailNewsSelectionDragFlavor,
			&outSelection,
			&dataSize, 
			0))
		return false;
	Assert_(dataSize == sizeof(CMailSelection));
	outSelection.Normalize();
	Assert_(outSelection.GetSelectionList() != NULL);
	return true;
} // CMailFlexTable::GetSelectionFromDrag

//-----------------------------------
void CMailFlexTable::ToggleExpandAction(TableIndexT row)
//-----------------------------------
{
	// rowDelta tells us how many items are added or removed.  We don't
	// need it, because we call ChangeFinished in the FE_LIstChangeFinished
	// callback.
	SInt32 rowDelta;
	MSG_ToggleExpansion(mMsgPane, row - 1, &rowDelta);
} // CMailFlexTable::ToggleExpansion

//-----------------------------------
void CMailFlexTable::ChangeStarting(
	MSG_Pane*		/* inPane */,
	MSG_NOTIFY_CODE	/* inChangeCode */,
	TableIndexT		/* inStartRow */,
	SInt32			/* inRowCount */)
//-----------------------------------
{
	++mMysticPlane;
} // CMailFlexTable::ChangeStarting

//-----------------------------------
void CMailFlexTable::RowsChanged(TableIndexT inFirst, TableIndexT inCount)
// Accumulate a range of rows to update.  We use this to delay refreshing until
// mMysticPlane has reached zero (outer call).
//-----------------------------------
{
	if (inCount == 0 || inFirst == 0)
		return;
	if (inFirst > mRows)
		return;
	if (mFirstRowToRefresh == 0 || inFirst < mFirstRowToRefresh)
		mFirstRowToRefresh = inFirst;
	TableIndexT maxCount = mRows - inFirst + 1;
	if (inCount > maxCount)		
		mLastRowToRefresh = ULONG_MAX;
	else
	{
		TableIndexT last = inFirst + inCount - 1;
		if (last > mLastRowToRefresh)
			mLastRowToRefresh = last;
	}
} // CMailFlexTable::RowsChanged

//-----------------------------------
void CMailFlexTable::ChangeFinished(
	MSG_Pane*		/* inPane */,
	MSG_NOTIFY_CODE	inChangeCode,
	TableIndexT		inStartRow,
	SInt32			inRowCount)
//-----------------------------------
{
	if (mMysticPlane > 0)
		--mMysticPlane;	
	if (mMsgPane && (mMysticPlane <= (kMysticUpdateThreshHold+1))) switch (inChangeCode)
	{
		case MSG_NotifyInsertOrDelete:
		{
			if (inRowCount > 0)
			{
				if (mRows + inRowCount > ::MSG_GetNumLines(mMsgPane))
				{
					// Undo bug.  Undo inserts extra rows.
					Assert_(FALSE); // congrats! The backend "extra ghost row on undo" bug.
				}
				else
				{
					// InsertRows has an "inAfterRow" parameter, but the meaning of
					// inStartRow as received from libmsg is that it is the index of
					// the first INSERTED row!
					InsertRows(inRowCount, inStartRow - 1, NULL, 0, false);		// line order...
					RowsChanged(inStartRow, ULONG_MAX);							// ...does matter
				}
			}
			else if (inRowCount < 0 && mRows > 0)
			{
				if (inStartRow - inRowCount - 1 <= mRows)
				{
					RowsChanged(inStartRow, ULONG_MAX);							// line order...
					RemoveRows(-inRowCount, inStartRow, false);					// ...does matter
				}
			}
			break;
		}
		case MSG_NotifyChanged:
		{
			RowsChanged(inStartRow, inRowCount);
			break;
		}
		case MSG_NotifyScramble:
		case MSG_NotifyAll:
			SetRowCount();
//			TableIndexT rows, cols;
//			GetTableSize(rows, cols);
			mFirstRowToRefresh =1 ;
			mLastRowToRefresh = ULONG_MAX;			
			break;
		default:
		case MSG_NotifyNone:
			break;
	} // switch
	if (mMysticPlane == 0 && mFirstRowToRefresh != 0)
	{
		const STableCell topLeftCell(mFirstRowToRefresh, 1);
		const STableCell botRightCell(
					mLastRowToRefresh > mRows ? mRows : mLastRowToRefresh, mCols);
		if (mLastRowToRefresh > mRows)
		{
			// (note that we're refreshing all the way to the bottom here).
			// Because of the complication of "reconcile
			// overhang", we really need to refresh all --- but only if part of the
			// range is visible. To do this, we need only check if the top of the top cell
			// is above the bottom of the frame.
			Int32 cellLeft, cellTop, cellRight, cellBottom;
			mTableGeometry->GetImageCellBounds(topLeftCell, cellLeft, cellTop,
									cellRight, cellBottom);
			// Convert from image coordinates to port coordinates
			cellTop += mImageLocation.v;
			Int32 frameBottom = mFrameLocation.v + mFrameSize.height;
			if (cellTop <= frameBottom) // Note the "=", edge case for deleting the last row.
				Refresh();	
		}
		else
			RefreshCellRange(topLeftCell, botRightCell);
		mFirstRowToRefresh = 0;
		mLastRowToRefresh = 0;
	}
} // CMailFlexTable::ChangeFinished

//-----------------------------------
void CMailFlexTable::PaneChanged(
	MSG_Pane*		/*  inPane */,
	MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
	int32 /* value */)
//-----------------------------------
{
	switch (inNotifyCode)
	{
		case MSG_PanePastPasswordCheck:
			//EnableStopButton(true);
			break;
	}
} // CMailFlexTable::PaneChanged

//-----------------------------------
Boolean CMailFlexTable::FindMessageLibraryCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
// returns false if not a msglib command.
//-----------------------------------
{
	CMailSelection selection;
	GetSelection(selection);
	return UMessageLibrary::FindMessageLibraryCommandStatus(
		GetMessagePane(),
		(MSG_ViewIndex*)selection.GetSelectionList(),
		selection.selectionSize,
		inCommand,
		outEnabled,
		outUsesMark,
		outMark,
		outName);
}

//-----------------------------------
void CMailFlexTable::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
//-----------------------------------
{
	if (mClosing)	// don't respond to BE callbacks when being destroyed
		return;

	if (inCommand == cmd_Stop && mStillLoading)
	{
		outEnabled = true; // stop button on, nothing else.
		return;
		// ... otherwise, fall through and pass it up to the window
	}

	if (!mMsgPane)
	{
		LCommander::GetTopCommander()->FindCommandStatus(
					inCommand, outEnabled, outUsesMark, outMark, outName);
		return;
	}
	switch (inCommand)
	{
		case cmd_AddToBookmarks:
		{
			outEnabled = mContext && SHIST_GetCurrent(&((MWContext*)*mContext)->hist);
			return;
		}
	}
	if (!FindMessageLibraryCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName)
	|| !outEnabled)
		Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
} // CMailFlexTable::FindCommandStatus

//-----------------------------------
Boolean CMailFlexTable::ObeyMessageLibraryCommand(
	CommandT	inCommand,
	void		* /* ioParam*/)
// Commands handled here are enabled/disabled in
// UMessageLibrary::FindMessageLibraryCommandStatus.
//-----------------------------------
{
	CStr255 commandName;
	MSG_CommandType cmd;
	switch (inCommand)
	{
		case cmd_CompressAllFolders:
			inCommand = cmd_CompressFolder;
			cmd = MSG_CompressFolder; // see hack below.
			break;
		default:
			// Callers rely on this to check if it really is a msglib command.
			// Do that first.
			cmd = UMessageLibrary::GetMSGCommand(inCommand);
			break;
	}
	if (!UMessageLibrary::IsValidCommand(cmd))
		return false;
	// For msglib commands, we have to be careful to check whether the command
	// can be handled for THIS pane, because (in the case of a thread pane)
	// the message pane might have enabled the menu item.  Failing to check
	// again here leads to a nasty crash.
	Boolean enabled; Boolean usesMark; Char16 mark;
	if (inCommand == cmd_Undo)
	{
		inCommand = mUndoCommand;
		// set it for next time
		mUndoCommand = (inCommand == cmd_Undo) ? cmd_Redo : cmd_Undo;
	}
	else
		mUndoCommand = cmd_Undo;
	if (!FindMessageLibraryCommandStatus(inCommand, enabled, usesMark, mark, commandName)
		|| !enabled)
	{
		if (inCommand == cmd_CompressFolder)
		{
			// Hack.
			// The logic is: if the selection permits MSG_CompressFolder, then do that.
			// Otherwise, try for MSG_CompressAllFolders.  We can't change the resources
			// now (localization freeze).  So the menu may have either of these commands
			// in it.
			
			// I don't think that we will ever get here since FindCommand status does a
			// a conversion between CompressFolder and CompressALLFolders --djm
			inCommand = cmd_CompressAllFolders;
			cmd = MSG_CompressAllFolders;
			if (!FindMessageLibraryCommandStatus(inCommand, enabled, usesMark, mark, commandName)
				|| !enabled)
				return false;
		}
		else
			return false;
	}
	// YAH (Yet another Hack) FindMessageLibraryCommandStatus is going to return true since it internally
	// does a conversion between cmd_CompressAllFolders and cmd_CompressFolder. The command then never
	// gets switched
	if ( inCommand == cmd_CompressFolder )
	{
		CMailSelection selection;
		GetSelection(selection);
		XP_Bool plural;
		XP_Bool enabledCommand = false;
		MSG_COMMAND_CHECK_STATE checkedState;
		const char* display_string = nil;
		MSG_CommandStatus( GetMessagePane(), MSG_CompressFolder,
				(MSG_ViewIndex*)selection.GetSelectionList(), selection.selectionSize,
				&enabledCommand, &checkedState, &display_string, &plural) ;
		// If the Compress Folder isn't enabled then Compress all is enabled
		if ( !enabledCommand  )
		{
			inCommand = cmd_CompressAllFolders;
			cmd = MSG_CompressAllFolders;
		}
	
	}
	
	try
	{
#define ALLOW_MODELESS_PROGRESS	1
#define ALLOW_MODAL_PROGRESS	1

		Boolean		cmdHandled = false;
		switch (cmd)
		{
			case MSG_GetNewMail:
			case MSG_GetNextChunkMessages:
				if (NET_IsOffline())
				{
					// Bug #105393.  This fails unhelpfully if the user is offline.  There
					// used to be a test for this here, but for some reason it was
					// removed.  This being so, the newly agreed-upon fix is that, if
					// the user requests new messages while offline, we should instead
					// present the  "Go Online" dialog.  See also CMessageView.cp.
					//		- 98/02/10 jrm.
					PREF_SetBoolPref("offline.download_discussions", true);
					PREF_SetBoolPref("offline.download_mail", true);
					PREF_SetBoolPref("offline.download_directories", false);
					UOffline::ObeySynchronizeCommand();
					cmdHandled = true;
				}
				else if (ALLOW_MODELESS_PROGRESS)
				{
					// Modeless window with separate context and pane
					CMailProgressWindow::ObeyMessageLibraryCommand(
						CMailProgressWindow::res_ID_modeless,
						GetMessagePane(), cmd, commandName);
					cmdHandled = true;
				}
				break;

			case MSG_MarkAllRead:
					// Can't display a dialog with command(s) which apply to
					// all the messages at once in the list because we don't
					// get the callbacks from the BE which allow to update
					// the progress bar and close the Progress window.
				break;

			case MSG_CompressFolder:
			case MSG_CompressAllFolders:
					// Bug #90378 (BE problem which is much easier to fix in the FE) 
					// Make these commands run inside their own separate context.
				CMailSelection selection;
				GetSelection(selection);
				CMailProgressWindow::ObeyMessageLibraryCommand(
					CMailProgressWindow::res_ID_modeless,
					GetMessagePane(), cmd, commandName,
					(MSG_ViewIndex*)selection.GetSelectionList(),
					selection.selectionSize);
				cmdHandled = true;
				break;
				
			default:
				if (ALLOW_MODAL_PROGRESS && !NET_IsOffline())
				{
					// Modal parasite window with same context and pane
					CMailProgressWindow::CreateModalParasite(
						CMailProgressWindow::res_ID_modal,
							GetMessagePane(), commandName);
				}
				break;
		}
		if (! cmdHandled)
		{
			CMailSelection selection;
			GetSelection(selection);
			MSG_Command(GetMessagePane(), cmd,
				(MSG_ViewIndex*)selection.GetSelectionList(),
					selection.selectionSize);
		}
	}
	catch(...)
	{
	}
	return true;
} // CMailFlexTable::ObeyMessageLibraryCommand

//-----------------------------------
Boolean CMailFlexTable::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
//-----------------------------------
{
	if (mClosing)	// don't respond to BE callbacks when being destroyed
		return false;

	if (mStillLoading && inCommand != cmd_Stop)
		return false;

	if (!mMsgPane)
		return LCommander::GetTopCommander()->ObeyCommand(inCommand, ioParam);

	switch (inCommand)
	{
		case cmd_Stop:
		{
			if (mContext)
				XP_InterruptContext(*mContext);
			EnableStopButton(false);
			return true;
		}
		case cmd_AddToBookmarks:
		{
		// MSG_GetFolderInfoFromURL() does not work for URLs pointing to Mail & News messages.
	//			SHIST_GetCurrent(&((MWContext*)*mContext)->hist)
	//			);
			
			// Nova: BM_Entry *entry = SHIST_CreateHotlistStructFromHistoryEntry(
			//		SHIST_GetCurrent(&((MWContext*)*mContext)->hist) );
			
			History_entry *entry = mContext->GetCurrentHistoryEntry();		// Mozilla
			
			if (entry)
				CBookmarksAttachment::AddToBookmarks(entry->address, entry->title);
			else
				SysBeep(1);
			break;
		}
		case cmd_Preferences:
			CPrefsDialog::EditPrefs(CPrefsDialog::eExpandMailNews);
			return true;
		default:
			return  ObeyMessageLibraryCommand(inCommand, ioParam)
			|| Inherited::ObeyCommand(inCommand, ioParam);
	}
	return false;
} // CMailFlexTable::ObeyCommand

//-----------------------------------
void CMailFlexTable::EnableStopButton(Boolean inBusy)
//-----------------------------------
{
	if (inBusy == mStillLoading)
		return;
	mStillLoading = inBusy;
	(CFrontApp::GetApplication())->UpdateMenus();
	// done in CFrontApp::UpdateMenus() already. CPaneEnabler::UpdatePanes();			
}

//-----------------------------------
void CMailFlexTable::DrawCountCell(
	Int32 inCount,
	const Rect& inLocalRect)
// inCount < 0 indicates unknown value
//-----------------------------------
{
	char sizeString[32];
	if (inCount >= 0)
		sprintf(sizeString, "%d", inCount);
	else
		sprintf(sizeString, "?"); 
	DrawTextString(sizeString, &mTextFontInfo, 2, inLocalRect, true, truncEnd);
} // CMessageFolderView::DrawCountCell

//-----------------------------------
void CMailFlexTable::ListenToMessage(MessageT inMessage, void* ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
		case CMailCallbackManager::msg_PaneChanged:
		case CMailCallbackManager::msg_ChangeStarting:
		case CMailCallbackManager::msg_ChangeFinished:
			if (IsMyPane(ioParam))
				CMailCallbackListener::ListenToMessage(inMessage, ioParam);
			break;
		case msg_NSCStartLoadURL:
		case msg_NSCProgressBegin:
			EnableStopButton(true);
			break;
		case msg_NSCAllConnectionsComplete:
			EnableStopButton(false);
			break;
		default:
			if (!IsOnDuty() || !ObeyCommand(inMessage, ioParam))
				ListenToHeaderMessage(inMessage, ioParam);
	}
} // CMailFlexTable::ListenToMessage

//---------------------------------------------------------------------
char* CMailFlexTable::GetTextFromDrag(
	DragReference inDragRef,
	ItemReference inItemRef)
// 	Check if this drag is a URL and returns the URL	if it is. 
// 	*** It is the responsibility of the client to delete the returned
//	result by calling XP_FREEIF()
//---------------------------------------------------------------------
{
	// get the drag data size
	Size dataSize = 0;
	OSErr err = ::GetFlavorDataSize(inDragRef, inItemRef, 'TEXT', &dataSize);
	if (err)
		return nil; // we can't throw during a drag!  Inconvenient in MWDebug.
	char* result = (char*)XP_ALLOC(1 + dataSize);
	if (!result)
		return nil;
	unsigned long offset = 0;
	// get the data out of the drag and put it into the buffer
	err = ::GetFlavorData(inDragRef, inItemRef, 'TEXT', result, &dataSize, offset);
	if (!err)
	{
		// terminate the string with a null char
		result[dataSize] = '\0';
		return result;
	}
	XP_FREEIF(result);
	return nil;
} // CMailFlexTable::GetTextFromDrag

//---------------------------------------------------------------------
MessageKey CMailFlexTable::MessageKeyFromURLDrag(
	DragReference inDragRef,
	ItemReference inItemRef)
// 	Check if this drag is the URL of a message and returns the message key if it is.
//---------------------------------------------------------------------
{
	MessageKey result = MSG_MESSAGEKEYNONE;
	char* url = GetTextFromDrag(inDragRef, inItemRef);
	if (!url)
		return MSG_MESSAGEKEYNONE;
	MSG_MessageLine messageLine;
	int status = MSG_GetMessageLineForURL( CMailNewsContext::GetMailMaster(), url, &messageLine );
	if (status  >= 0)
		result = messageLine.messageKey;
	XP_FREEIF(url);
	return result;
} // CMailFlexTable::MessageKeyFromURLDrag

//---------------------------------------------------------------------
MSG_FolderInfo* CMailFlexTable::GetFolderInfoFromURLDrag(
	DragReference inDragRef,
	ItemReference inItemRef)
// 	Check if this drag is the URL of a folder and returns the folderInfo if it is. 
//---------------------------------------------------------------------
{
	char* url = GetTextFromDrag(inDragRef, inItemRef);
	if (!url)
		return nil;
	MSG_FolderInfo* result = MSG_GetFolderInfoFromURL(CMailNewsContext::GetMailMaster(), url, false);
	XP_FREEIF(url);
	return result;
} // CMailFlexTable::GetFolderInfoFromURLDrag

