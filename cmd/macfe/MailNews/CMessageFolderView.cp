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




#include "CMessageFolderView.h"

// PowerPlant
#include <LTableArrayStorage.h>
#include <LDropFlag.h>

#include "prefapi.h"
#include "shist.h"
#include "uprefd.h"
#include "Secnav.h"

// MacFE
#include "macutil.h"
#include "ufilemgr.h"

// Netscape Mac Libs
#include "UGraphicGizmos.h"
#include "resgui.h"

// Mail/News Specific
#include "UOffline.h"
#include "MailNewsgroupWindow_Defines.h"
#include "CThreadWindow.h"
#include "CMailProgressWindow.h"
#include "LTableViewHeader.h"
#include "CProgressListener.h"
#include "CMessageFolder.h"
#include "CMailNewsContext.h"
#include "CNewsSubscriber.h"
#include "UMailFolderMenus.h"
#include "StGetInfoHandler.h"
#include "UNewFolderDialog.h"
#include "CMessageWindow.h"
#include "UMailSelection.h"
#include "UMessageLibrary.h"

#include "CPrefsDialog.h"
#include "CProxyDragTask.h"
#include "UDeferredTask.h"
#include "CInlineEditField.h"

//========================================================================================
class CFolderWatchingTask
//========================================================================================
:	public CDeferredTask
{
	public:
							CFolderWatchingTask(
								CMessageFolderView*		inFolderView,
								MSG_FolderInfo*			inFolderInfo)
							:	mFolderView(inFolderView)
							,	mWatchedFolder(inFolderInfo, inFolderView->GetMessagePane())
							,	mWatchedFolderChildCount( mWatchedFolder.CountSubFolders())
							,	mAnotherExecuteAttemptCount(0)
							{
							}
		virtual ExecuteResult	ExecuteSelf();
	protected:
		CMessageFolderView*	mFolderView;
		CMessageFolder		mWatchedFolder;
		UInt32				mWatchedFolderChildCount;
		UInt32				mAnotherExecuteAttemptCount;
};

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CFolderWatchingTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	// This is here for the case when folders were dragged into (or deleted into)
	// a folder which was closed in the message center.  We need to refresh the
	// cache, because the newly added children have changed their level in the
	// hierarchy.  We don't get notified unless the new parent is expanded.
	// Currently, this only goes one folder deep, which covers the most common case
	// of folder deletion (move to trash).
	if (++mAnotherExecuteAttemptCount >= 500)
		return eDoneDelete; // give up and die
	CNSContext* context = mFolderView->GetContext();
	if (context->GetCurrentCommand() == cmd_MoveMailMessages )
		context->SetCurrentCommand(cmd_Nothing);
	mWatchedFolder.FolderInfoChanged(mFolderView->GetMessagePane()); // get new backend data
	if (!mWatchedFolder.GetFolderInfo())
		return eDoneDelete;
	UInt32 childCount = mWatchedFolder.CountSubFolders();
	if (childCount <= mWatchedFolderChildCount)
		return eWaitStepBack; // no change yet, but ok to run another task.
	// OK, it's stale.  Fix it.
	mWatchedFolder.FolderLevelChanged(mFolderView->GetMessagePane());
	MSG_ViewIndex index = ::MSG_GetFolderIndex(mFolderView->GetMessagePane(), mWatchedFolder);
	if (index != MSG_VIEWINDEXNONE)
		mFolderView->RefreshRowRange(index + 1, index + 1);
	return eDoneDelete;
} // CFolderWatchingTask::ExecuteSelf

#pragma mark -- CDeleteSelectionTask

//========================================================================================
class CDeleteSelectionTask
//========================================================================================
:	public CDeferredTask
{
	public:
									CDeleteSelectionTask(
										CMessageFolderView*		inFolderView,
										const CMailSelection&	inSelection)
									:	mSelection(inSelection)
									,	mFolderView(inFolderView)
									{}
		virtual ExecuteResult		ExecuteSelf();
	protected:
		CPersistentFolderSelection	mSelection;
		CMessageFolderView*			mFolderView;					
}; // class CDeleteSelectionTask

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeleteSelectionTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	CNetscapeWindow* win = dynamic_cast<CNetscapeWindow*>
		(LWindow::FetchWindowObject(mFolderView->GetMacPort()));
	// Wait till any pending URLs are finished
	if (win->IsAnyContextBusy())
		return eWaitStayFront;
	const MSG_ViewIndex* indices = mSelection.GetSelectionList();
	MSG_ViewIndex numIndices = mSelection.selectionSize;
	mFolderView->DoDeleteSelection(mSelection);
	return eDoneDelete;
} // CDeleteSelectionTask::ExecuteSelf

#pragma mark -- CDeferredDragTask

//========================================================================================
class CDeferredDragTask
//========================================================================================
:	public CDeferredTask
{
	public:
									CDeferredDragTask(
										CMessageFolderView*		inFolderView,
										const CMessageFolder&	inDestFolder,
										Boolean					inDoCopy)
									:	mFolderView(inFolderView)
									,	mDestFolder(inDestFolder)
									,	mDoCopy(inDoCopy)
									{}
	protected:
		CMessageFolderView*			mFolderView;					
		CMessageFolder				mDestFolder;
		Boolean						mDoCopy; // rather than move
};

//========================================================================================
class CDeferredDragFolderTask
//========================================================================================
:	public CDeferredDragTask
{
	public:
									CDeferredDragFolderTask(
										CMessageFolderView*		inFolderView,
										const CMailSelection&	inSelection,
										const CMessageFolder&	inDestFolder,
										Boolean					inDoCopy)
									:	CDeferredDragTask(
											inFolderView
										,	inDestFolder
										,	inDoCopy)
									,	mSelection(inSelection)
									{}
		virtual ExecuteResult		ExecuteSelf();
	protected:
		CPersistentFolderSelection	mSelection;
}; // class CDeferredDragFolderTask

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredDragFolderTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	// display modal dialog
//	if (!NET_IsOffline()) 	// ? 97/08/13 someone removed this line.
							//   98/01/26 it's because moving messages can take some time - even offline
	{
		short stringID = 15; // Copying / Moving Messages
		CStr255 commandString;
		::GetIndString(commandString, 7099, stringID);
		CMailProgressWindow* progressWindow = CMailProgressWindow::CreateModalParasite(
			CMailProgressWindow::res_ID_modal,
			mSelection.xpPane,
			commandString);
		if (progressWindow)
			progressWindow->SetDelay(0);
	}

	// copy / move messages
	if (mDestFolder.IsTrash())
	{
		// Special case.  Back-end doesn't translate this into a delete, and
		// presents an alert unless you're dragging mail folders.  But it should
		// work for news servers etc.  So:
		XP_Bool confirmPref; // Don't ever confirm a drag.
		PREF_GetBoolPref("mailnews.confirm.moveFoldersToTrash", &confirmPref);
		if (confirmPref)
			PREF_SetBoolPref("mailnews.confirm.moveFoldersToTrash", FALSE);
		mFolderView->DoDeleteSelection(mSelection);
		if (confirmPref)
			PREF_SetBoolPref("mailnews.confirm.moveFoldersToTrash", confirmPref);
	}
	else
	{
		mFolderView->GetContext()->SetCurrentCommand(cmd_MoveMailMessages);
		mFolderView->WatchFolderForChildren(mDestFolder);
		::MSG_MoveFoldersInto(
			mSelection.xpPane, 
			mSelection.GetSelectionList(),
			mSelection.selectionSize,
			mDestFolder
			);
	}
	return eDoneDelete;
} //  CDeferredDragFolderTask::ExecuteSelf

//========================================================================================
class CDeferredDragMessageTask
//========================================================================================
:	public CDeferredDragTask
{
	public:
									CDeferredDragMessageTask(
										CMessageFolderView*		inFolderView,
										const CMailSelection&	inSelection,
										const CMessageFolder&	inDestFolder,
										Boolean					inDoCopy)
									:	CDeferredDragTask(
											inFolderView
										,	inDestFolder
										,	inDoCopy)
									,	mSelection(inSelection)
									{}
		virtual ExecuteResult		ExecuteSelf();
	protected:
		CPersistentMessageSelection	mSelection;
}; // class CDeferredDragMessageTask

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredDragMessageTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	mFolderView->DropMessages(mSelection, mDestFolder, mDoCopy);
	return eDoneDelete;
} //  CDeferredDragMessageTask::ExecuteSelf

#pragma mark -- CMessageFolderView

//----------------------------------------------------------------------------------------
CMessageFolderView::CMessageFolderView(LStream *inStream)
//----------------------------------------------------------------------------------------
:	Inherited(inStream)
,	mUpdateMailFolderMenus(false)
{
	mAllowDropAfterLastRow = false;
} // CMessageFolderView::CMessageFolderView


//----------------------------------------------------------------------------------------
CMessageFolderView::~CMessageFolderView()
//----------------------------------------------------------------------------------------
{
} // CMessageFolderView::~CMessageFolderView

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::ItemIsAcceptable(
	DragReference	inDragRef, 
	ItemReference 	inItemRef	)
//----------------------------------------------------------------------------------------
{
	FlavorFlags flavorFlags;
	if (::GetFlavorFlags(inDragRef, inItemRef, kMailNewsSelectionDragFlavor, &flavorFlags) == noErr)
	{
		CMailSelection selection;
		if (!mIsInternalDrop && GetSelectionFromDrag(inDragRef, selection))
			mIsInternalDrop = (selection.xpPane == GetMessagePane());
		return true;
	}
	return false;			
} // CMessageFolderView::ItemIsAcceptable

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::GetSelectionAndCopyStatusFromDrag(
	DragReference	inDragRef,
	CMessageFolder&	inDestFolder,
	CMailSelection&	outSelection,
	Boolean&		outCopy)
// Return true if this is an OK drop operation.
//----------------------------------------------------------------------------------------
{
	if (!GetSelectionFromDrag(inDragRef, outSelection))
		return false;
	Boolean isFolderDrop = (::MSG_GetPaneType(outSelection.xpPane) == MSG_FOLDERPANE);
	// Don't do anything if the user aborted by dragging back into the same first row.
	// There are other cases, but the backend will alert anyway.  In this simple case,
	// the back-end alert is annoying.

	SInt16 modifiers;
	::GetDragModifiers(inDragRef, NULL, &modifiers, NULL);
	outCopy = ((modifiers & optionKey) != 0);
	MSG_DragEffect effect = outCopy ? MSG_Require_Copy : MSG_Default_Drag; 

	// Ask the back end for guidance on what is possible:
	if (isFolderDrop)
	{
		effect = ::MSG_DragFoldersIntoStatus(
			outSelection.xpPane,
			outSelection.GetSelectionList(),
			outSelection.selectionSize,
			inDestFolder,
			effect);
		if (effect == MSG_Drag_Not_Allowed && outCopy)
		{
			// Well, maybe a move is ok
			effect = ::MSG_DragFoldersIntoStatus(
				outSelection.xpPane,
				outSelection.GetSelectionList(),
				outSelection.selectionSize,
				inDestFolder,
				MSG_Default_Drag);
		}
	}
	else
	{
		effect = ::MSG_DragMessagesIntoFolderStatus(
			outSelection.xpPane,
			outSelection.GetSelectionList(),
			outSelection.selectionSize,
			inDestFolder,
			effect);
		if (effect == MSG_Drag_Not_Allowed && outCopy)
		{
			// Well, maybe a move is ok
			effect = ::MSG_DragMessagesIntoFolderStatus(
				outSelection.xpPane,
				outSelection.GetSelectionList(),
				outSelection.selectionSize,
				inDestFolder,
				MSG_Default_Drag);
		}
	}
	outCopy = (effect & MSG_Require_Copy) != 0;
	return (effect != MSG_Drag_Not_Allowed); // this looks ok as a drop operation.
} // CMessageFolderView::GetSelectionAndCopyStatusFromDrag

//----------------------------------------------------------------------------------------
void CMessageFolderView::DropMessages(
	const CMailSelection& inSelection,
	const CMessageFolder& inDestFolder,
	Boolean inDoCopy)
//----------------------------------------------------------------------------------------
{
	// display modal dialog
//	if (!NET_IsOffline()) 	// ? 97/08/13 someone removed this line.
							//   98/01/26 it's because moving messages can take some time - even offline
	{
		short stringID = (inDoCopy ? 19 : 15 ); // Copying / Moving Messages
		CStr255 commandString;
		::GetIndString(commandString, 7099, stringID);
		CMailProgressWindow* progressWindow = CMailProgressWindow::CreateModalParasite(
			CMailProgressWindow::res_ID_modal,
			inSelection.xpPane,
			commandString);
		if (progressWindow)
			progressWindow->SetDelay(0);
	}

	// Close any windows associated with these messages, if the preferences
	// say we should.
	CMessageWindow::NoteSelectionFiledOrDeleted(inSelection);
	if (!inDoCopy)
		::MSG_MoveMessagesIntoFolder(
			inSelection.xpPane,
			inSelection.GetSelectionList(), 
			inSelection.selectionSize,
			inDestFolder
			);
	else
		::MSG_CopyMessagesIntoFolder(
			inSelection.xpPane, 
			inSelection.GetSelectionList(), 
			inSelection.selectionSize,
			inDestFolder
			);
		
} //  CMessageFolderView::DropMessages

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::CanDoInlineEditing()
// (override from CStandardFlexTable)
//----------------------------------------------------------------------------------------
{
	CMessageFolder folder(mRowBeingEdited, GetMessagePane());
	Boolean outEnabled;
	Boolean outUsesMark;
	Char16 outMark;
	Str255 outName;
	FindCommandStatus(cmd_RenameFolder, outEnabled, outUsesMark, outMark, outName);
	return outEnabled;
}

//----------------------------------------------------------------------------------------
void CMessageFolderView::InlineEditorTextChanged()
// (override from CStandardFlexTable)
//----------------------------------------------------------------------------------------
{
}
//----------------------------------------------------------------------------------------
void CMessageFolderView::InlineEditorDone()
// (override from CStandardFlexTable)
//----------------------------------------------------------------------------------------
{
	if (mRowBeingEdited == LArray::index_Bad || !mNameEditor)
		return;
	CMessageFolder folder(mRowBeingEdited, GetMessagePane());
	char oldName[256];
	CMailFolderMixin::GetFolderNameForDisplay(oldName, folder);
	CStr255 newName;
	mNameEditor->GetDescriptor(newName);
	if (newName != oldName)
	{
		char* newNameC = XP_STRDUP(newName);
		if (newNameC)
		{
			::MSG_RenameMailFolder(GetMessagePane(), folder, newNameC);
			folder.FolderInfoChanged(GetMessagePane());
			XP_FREE(newNameC);
		}
	}
} // CMessageFolderView::InlineEditorDone

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::RowCanAcceptDrop(
	DragReference	inDragRef,
	TableIndexT		inDropRow)
// (override from CStandardFlexTable)
//----------------------------------------------------------------------------------------
{
	Boolean dropOK = false;

	SInt16 modifiers;
	::GetDragModifiers(inDragRef, NULL, &modifiers, NULL);
	Boolean doCopy = ((modifiers & optionKey) != 0);
	
	if (inDropRow >= 1 && inDropRow <= mRows)
	{
		CMessageFolder destFolder(inDropRow, GetMessagePane());
		CMailSelection selection;
		dropOK = (GetSelectionAndCopyStatusFromDrag(
			inDragRef, destFolder,
			selection, doCopy));
	}
	if (dropOK && doCopy)
	{
		CursHandle copyDragCursor = ::GetCursor(curs_CopyDrag); // finder's copy-drag cursor
		if (copyDragCursor != nil)
			::SetCursor(*copyDragCursor);
	}
	else
		::SetCursor(&qd.arrow);
	return dropOK;
} // CMessageFolderView::RowCanAcceptDrop

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::RowCanAcceptDropBetweenAbove(
	DragReference	inDragRef,
	TableIndexT		inDropRow)
// (override from CStandardFlexTable)
//----------------------------------------------------------------------------------------
{
	if (inDropRow >= 1 && inDropRow <= mRows)
	{
		CMailSelection selection;
		if (GetSelectionFromDrag(inDragRef, selection))
		{
			// Dropping between rows signifies reordering.  This is only allowed when the
			// source pane of the drag is our own pane.
			if (selection.xpPane != GetMessagePane())
				return false;
			CMessageFolder destFolder(inDropRow, GetMessagePane());
			CMailSelection selection;
			Boolean doCopy;
			return GetSelectionAndCopyStatusFromDrag(
				inDragRef, destFolder,
				selection, doCopy);
		}
	}
	return false;
} // CMessageFolderView::RowCanAcceptDropBetweenAbove

//----------------------------------------------------------------------------------------
void CMessageFolderView::ReceiveDragItem(
	DragReference		inDragRef,
	DragAttributes		/*inDragAttrs*/,
	ItemReference		/*inItemRef*/,
	Rect&				/*inItemBounds*/)
//----------------------------------------------------------------------------------------
{	
	if (mDropRow == 0) return; // FIXME:  when move to top level is implemented, do that!
		
	// What is the destination folder?
	CMessageFolder destFolder(mDropRow, GetMessagePane());

	// What are the items being dragged?
	CMailSelection selection;
	Boolean	doCopy = false;
	if (!GetSelectionAndCopyStatusFromDrag(inDragRef, destFolder, selection, doCopy))
		return;
	
	// We have to delay the drag in order to display any confirmation dialog
	// from the BE (when moving a folder to the trash, for instance).
	// Displaying a dialog in here crashes the Mac.
	CDeferredDragTask* task = nil;
	MSG_PaneType paneType = ::MSG_GetPaneType(selection.xpPane);
	if (paneType == MSG_FOLDERPANE)
		CDeferredTaskManager::Post(
			new CDeferredDragFolderTask(this, selection, destFolder, doCopy), this);
	else if (paneType == MSG_MESSAGEPANE)
		CDeferredTaskManager::Post(
			new CDeferredDragMessageTask(this, selection, destFolder, doCopy), this);
	else // search pane?
		DropMessages(selection, destFolder, doCopy); // devil take the hindmost.
} // CMessageFolderView::ReceiveDragItem								

//----------------------------------------------------------------------------------------
void CMessageFolderView::WatchFolderForChildren(MSG_FolderInfo* inFolderInfo)
//----------------------------------------------------------------------------------------
{
	CFolderWatchingTask* task = new CFolderWatchingTask(this, inFolderInfo);
	CDeferredTaskManager::Post(task, this);
} // CMessageFolderView::WatchFolderForChildren

//----------------------------------------------------------------------------------------
void CMessageFolderView::DoAddNewsGroup()
//----------------------------------------------------------------------------------------
{
	try
	{
		MSG_ViewIndex* indexList = NULL;
		UInt32 numIndices = 0;
		CMailSelection selection;
		if (GetSelection(selection))
		{
			//...unless there's a single item selected, and it's a newsgroup
			if (selection.selectionSize == 1)
			{
				MSG_ViewIndex index = *(MSG_ViewIndex*)selection.GetSelectionList();
				MSG_FolderInfo* info = ::MSG_GetFolderInfo(GetMessagePane(), index);
				CMessageFolder folder(info, GetMessagePane());
				if (folder.IsNewsHost())
				{
					indexList = &index;
					numIndices = 1;
				}
			}
		}
		MSG_Command(
			GetMessagePane(),
			MSG_AddNewsGroup,
			indexList,
			numIndices);
		Refresh(); // works around a bug - we don't get notified.
	}
	catch(...)
	{
	}
} // CMessageFolderView::DoOpenNewsHost()	
				
//----------------------------------------------------------------------------------------
void CMessageFolderView::DeleteSelection()
// Delete all selected folders
//----------------------------------------------------------------------------------------
{
	CMailSelection selection;
	if (GetSelection(selection))
	{
		// Deletion cannot occur if we are viewing the contents in a thread pane.
		// So make a task (which clones the selection) then unselect the selected
		// item (triggering an unload of the folder pane).  Then post a delete
		// task.
		CDeleteSelectionTask* task = new CDeleteSelectionTask(this, selection);
		UnselectAllCells();
		CDeferredTaskManager::Post(task, this);
	}
} // CMessageFolderView::DeleteSelection

//----------------------------------------------------------------------------------------
void CMessageFolderView::DoDeleteSelection(const CMailSelection& inSelection)
//----------------------------------------------------------------------------------------
{
	try
	{
		mContext->SetCurrentCommand(cmd_Clear); // in case it was a keystroke
		// If there's one mail folder in the selection being deleted, then
		// watch the destination folder until the folder count changes.  Otherwise
		// (eg dragging newsgroups) we will wait forever, in vain...
		MSG_ViewIndex* cur = (MSG_ViewIndex*)inSelection.GetSelectionList();
		MSG_FolderInfo* folderToWatch = nil;
		for (int i = 0; i < inSelection.selectionSize; i++, cur++)
		{
			MSG_FolderInfo* info = ::MSG_GetFolderInfo(GetMessagePane(), *cur);
			CMessageFolder folder(info, GetMessagePane());
			if (folder.IsMailFolder())
			{
				::MSG_GetFoldersWithFlag(
							CMailNewsContext::GetMailMaster(),
							MSG_FOLDER_FLAG_TRASH,
							&folderToWatch,
							1);
				break;
			}
		}
		MSG_CommandType cmd = UMessageLibrary::GetMSGCommand(cmd_Clear);
#if 1 // (MSG_Delete != MSG_DeleteFolder)
		if (cmd == MSG_Delete)
			cmd = MSG_DeleteFolder; // This can go away when winfe becomes compliant.
#endif
		::MSG_Command(
				GetMessagePane(),
				cmd,
				const_cast<MSG_ViewIndex*>(inSelection.GetSelectionList()),
				inSelection.selectionSize);
		if (folderToWatch)
			WatchFolderForChildren(folderToWatch);
	}
	catch(...)
	{
	}
} // CMessageFolderView::DoDeleteSelection()	

//----------------------------------------------------------------------------------------
void CMessageFolderView::AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef)
//----------------------------------------------------------------------------------------
{
	if (inRow == LArray::index_Bad || inRow > mRows)
		return;
	
	Inherited::AddRowDataToDrag(inRow, inDragRef);		// does nothing right now
	
	CMessageFolder folder(inRow, GetMessagePane());

	// allow newsgroup text drags into compose window
	if (folder.IsNewsgroup())
	{
		MSG_FolderInfo		*folderInfo = folder.GetFolderInfo();

		URL_Struct	*theURLStruct = ::MSG_ConstructUrlForFolder(GetMessagePane(), folderInfo);
		
		if (theURLStruct != nil)
		{
		
			OSErr err = ::AddDragItemFlavor( inDragRef, inRow, 'TEXT',
							theURLStruct->address, XP_STRLEN(theURLStruct->address), flavorSenderOnly);
		
			NET_FreeURLStruct(theURLStruct);
		}
	}
	
} // CMessageFolderView::AddRowToDrag 

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::CellInitiatesDrag(const STableCell& inCell) const
//----------------------------------------------------------------------------------------
{
	return (GetCellDataType(inCell) == kFolderNameColumn);
} // CMessageFolderView::CellInitiatesDrag

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::GetRowDragRgn(TableIndexT inRow, RgnHandle ioHiliteRgn) const
// The drag region is the hilite region plus the icon
// Note that the row we are dragging may or may not be selected, so don't rely
// on the selection for anything.
//----------------------------------------------------------------------------------------
{
	::SetEmptyRgn(ioHiliteRgn);
	Rect cellRect;
	
	TableIndexT col = GetHiliteColumn();
	if ( !col )
		col = 1;
	STableCell cell(inRow, col);
	if (!GetLocalCellRect(cell, cellRect))
		return false;
	ResIDT iconID = GetIconID(inRow);
	if (iconID)
	{
		GetIconRect(cell, cellRect, cellRect);
		::IconIDToRgn(ioHiliteRgn, &cellRect, kAlignNone, iconID);
	}
	StRegion cellRgn;
	GetRowHiliteRgn(inRow, cellRgn);
	::UnionRgn(ioHiliteRgn, cellRgn, ioHiliteRgn);
	
	CMessageFolder folder(inRow, GetMessagePane());
	
	// If this row is an expanded folder with children, then we need to
	// add each child row to the drag also.
	// If one of these child rows is selected, and we are dragging the selection,
	// then it will get added twice, but this should not be a problem.
	if (folder.CountSubFolders() > 0 && folder.IsOpen())
	{
		UInt32			folderLevel = folder.GetLevel();
		STableCell		nextCell(cell.row + 1, 1);
		StRegion 		tempRgn;
		
		while (GetNextCell(nextCell))
		{
			CMessageFolder	subFolder(nextCell.row, GetMessagePane());
			UInt32			subFolderLevel = subFolder.GetLevel();
		
			if (subFolderLevel <= folderLevel)
				break;
			
			Inherited::GetRowDragRgn(nextCell.row, tempRgn);
			::UnionRgn(tempRgn, ioHiliteRgn, ioHiliteRgn);
		}
	}
	
	return true;
} // CStandardFlexTable::GetRowHiliteRgn

//----------------------------------------------------------------------------------------
void CMessageFolderView::GetInfo()
//----------------------------------------------------------------------------------------
{
	CMessageFolder folder(GetSelectedFolder());
	if (folder.GetFolderInfo())
		UGetInfo::ConductFolderInfoDialog(folder, this);
}

//----------------------------------------------------------------------------------------
void CMessageFolderView::OpenRow(TableIndexT inRow)
// See also CFolderThreadController::ListenToMessage
//----------------------------------------------------------------------------------------
{
	CMessageFolder folder(inRow, GetMessagePane());
	if (folder.IsMailServer())
	{
		UGetInfo::ConductFolderInfoDialog(folder, this);
		return;
	}
	if (folder.IsNewsHost())
	{
		MSG_Host* selectedHost = MSG_GetHostForFolder(folder);
		CNewsSubscriber::DoSubscribeNewsGroup(selectedHost);
		return;
	}
	if (!folder.CanContainThreads())
		return;

	Boolean forceNewWindow = false;
	if (GetSelectedRowCount() > 1 && inRow != GetTableSelector()->GetFirstSelectedRow())
	{
		STableCell cell(inRow, 1);
		if (CellIsSelected(cell))
			forceNewWindow = true;
	}

	CThreadWindow* threadWindow
		= CThreadWindow::FindAndShow(
			folder.GetFolderInfo(),
			CThreadWindow::kMakeNew,
			cmd_Nothing,
			forceNewWindow);	// Force new window for multiple selection.
} // CMessageFolderView::OpenRow

//----------------------------------------------------------------------------------------
void CMessageFolderView::GetLongWindowDescription(CStr255& outDescription)
//----------------------------------------------------------------------------------------
{
	::GetIndString(outDescription, 7099, 11); // "Message Center For ^0"
	char buffer[256]; int bufferLength = sizeof(buffer);
	::PREF_GetCharPref("mail.identity.username", buffer, &bufferLength);
	::StringParamText(outDescription, buffer);
}

//----------------------------------------------------------------------------------------
void CMessageFolderView::SelectionChanged()
//----------------------------------------------------------------------------------------
{
	Inherited::SelectionChanged();
	TableIndexT rowCount = GetSelectedRowCount();
	URL_Struct* url = nil;
	char entryName[128];
	entryName[0] = '\0';
	if (rowCount == 1)
	{
		CMailSelection selection;
		GetSelection( selection );
		MSG_ViewIndex index = *(MSG_ViewIndex*)selection.GetSelectionList();
		MSG_FolderInfo* info = MSG_GetFolderInfo(GetMessagePane(), index);
		MSG_FolderLine folderLine;
		XP_Bool gotIt = ::MSG_GetFolderLineById(
			CMailNewsContext::GetMailMaster(),
			info,
			&folderLine);
		if (gotIt)
		{
			url = ::MSG_ConstructUrlForFolder(GetMessagePane(), info);
			strcpy(entryName, folderLine.name);
		}
	}
	else
	{
		// Use the name in the location bar for any potential bookmarks...
		url = ::MSG_ConstructUrlForPane(GetMessagePane());
		CStr255 tempString;
		GetLongWindowDescription(tempString);
		strcpy(entryName, tempString);
	}
	if (url && *entryName)
	{
		History_entry* theNewEntry = SHIST_CreateHistoryEntry(
			url,
			entryName);
		SHIST_AddDocument(*mContext, theNewEntry);
	}
	XP_FREEIF(url);
}

//----------------------------------------------------------------------------------------
void CMessageFolderView::ListenToMessage(MessageT inMessage, void* ioParam)
//----------------------------------------------------------------------------------------
{
	// button messages.
	if (!IsOnDuty() || !ObeyCommand(inMessage, ioParam))
	{
		Inherited::ListenToMessage(inMessage, ioParam);
	}
} // CMessageFolderView::ListenToMessage

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::FindMessageLibraryCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
// returns false if not a msglib command.
//----------------------------------------------------------------------------------------
{
	// Interpret MarkSelectedRead as "mark all read" (ie, in this folder)
	if (inCommand == cmd_MarkSelectedRead)
		inCommand = cmd_MarkAllRead;
	return Inherited::FindMessageLibraryCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
}

//----------------------------------------------------------------------------------------
void CMessageFolderView::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
//----------------------------------------------------------------------------------------
{
	// Preprocessing before calling UMessageLibrary
	switch (inCommand)
	{
		case cmd_NewFolder:
			outEnabled = true;
			return;
		case cmd_NewNewsgroup:
			MSG_FolderInfo*	folderInfo = GetSelectedFolder();
			inCommand = cmd_NewFolder;
			if (folderInfo != nil)
			{
				CMessageFolder folder(folderInfo, GetMessagePane());
				if (folder.IsNewsgroup() || folder.IsNewsHost())
					inCommand = cmd_NewNewsgroup;
			}
			break;

		case cmd_OpenSelection:	// enabled in base class
		{
			MSG_FolderInfo*	folderInfo = GetSelectedFolder();
			if (folderInfo != nil)
			{
				CMessageFolder folder(folderInfo, GetMessagePane());
				short	strID = 0;
				if (folder.IsNewsgroup())
					strID = kOpenDiscussionStrID;
				else if (folder.IsMailServer())
					strID = kOpenMailServerStrID;
				else if (folder.IsNewsHost())
					strID = kOpenNewsServerStrID;
				else if (folder.IsMailFolder())
					strID = kOpenFolderStrID;
				if (strID != 0)
					::GetIndString(outName, kMailNewsMenuStrings, strID);
			}
			break;
		}	
		case cmd_OpenSelectionNewWindow:
		{
			MSG_FolderInfo*	folderInfo = GetSelectedFolder();
			if (folderInfo != nil)
			{
				CMessageFolder folder(folderInfo, GetMessagePane());
				if (folder.IsMailServer() || folder.IsNewsHost())
				{
					outEnabled = false;
					return;
				}
			}
			break;
		}			
		case cmd_GetMoreMessages:
			CStr255 cmdStr;
			CStr255 numStr;
			::GetIndString(cmdStr, kMailNewsMenuStrings, kNextChunkMessagesStrID);
			::NumToString(CPrefs::GetLong(CPrefs::NewsArticlesMax), numStr);
			cmdStr += numStr;
			memcpy(outName, (StringPtr)cmdStr, cmdStr.Length() + 1);
			break;

		case cmd_Stop:
			if (mStillLoading)
			{
				outEnabled = true; // stop button on, nothing else.
				return;
			}
			break;	
 #if 0
 // 97/10/20.  Fall through to the inherited version.
 //	We don't handle it as a menu command - only internally.
 		case cmd_NewsGroups: // Expand news host
		case cmd_MailNewsFolderWindow: // Expand mail host
			outEnabled = true;
			return;
#endif // 0
		case cmd_GetInfo:
			if (GetSelectedRowCount() != 1)
			{
				outEnabled = false;
				return;
			}
			outEnabled = true;
			return;
			
		case cmd_Clear:
			folderInfo = GetSelectedFolder();
			if (folderInfo != nil) {
				CMessageFolder folder(folderInfo, GetMessagePane());
				if (!folder.IsMailServer())
					outEnabled = true;
			}
			return;

		case cmd_AddToBookmarks:
		{
			MSG_FolderInfo*	folderInfo = GetSelectedFolder();
			if (folderInfo != nil)
			{
				CMessageFolder folder(folderInfo, GetMessagePane());
				if (folder.IsIMAPMailFolder() || folder.IsNewsHost())
				{
					outEnabled = false;
					return;
				}
			}
			break; // call inherited::
		}
	} // switch

	if (!mStillLoading && FindMessageLibraryCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName))
		return;

	switch (inCommand)
	{
		case cmd_OpenNewsHost:
		case cmd_AddNewsgroup:
		case cmd_SubscribeNewsgroups:
			outEnabled = !mStillLoading;
			break;
		default:
			Inherited::FindCommandStatus(
				inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
} // CMessageFolderView::FindCommandStatus

//----------------------------------------------------------------------------------------
Boolean CMessageFolderView::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
//----------------------------------------------------------------------------------------
{
	if (!mContext)
		return false;

	Boolean	cmdHandled = false;
	CNSContext* originalContext = mContext; // in case we close the window & delete it!
	CommandT saveCommand = mContext->GetCurrentCommand();
	mContext->SetCurrentCommand(inCommand);
	switch (inCommand)
	{
		case msg_TabSelect:
			return (IsVisible()); // Allow selection only if pane is visible
#if 0
	// This now HAS to be handled by the top commander (application), because the behavior
	// has to be different depending on whether we are in a message center window or a 3-pane
	// thread window.
		case cmd_NewsGroups: 			// Select first news host
		case cmd_MailNewsFolderWindow:	// Select first mail host
			SelectFirstFolderWithFlags(
				inCommand == cmd_NewsGroups
					? MSG_FOLDER_FLAG_NEWS_HOST
					: MSG_FOLDER_FLAG_MAIL);
			cmdHandled = true;
			break;
#endif // 0
		case cmd_SecurityInfo:
			MWContext * context = *mContext;
			SECNAV_SecurityAdvisor( context, NULL );
			cmdHandled =true;
			break;
		//-----------------------------------	
		// News
		//----------------------------------------------------------------------------------------
		case cmd_OpenNewsHost:
			CNewsSubscriber::DoAddNewsHost();
			cmdHandled = true;
			break;
		case cmd_SubscribeNewsgroups:
			MSG_Host* selectedHost = MSG_GetHostForFolder(GetSelectedFolder());
			CNewsSubscriber::DoSubscribeNewsGroup(selectedHost);
			cmdHandled = true;
			break;
		case cmd_AddNewsgroup:
			DoAddNewsGroup();
			cmdHandled = true;
			break;
		//-----------------------------------	
		// Special cases
		//----------------------------------------------------------------------------------------
#if 0
		// Not needed - have inline editing now (and the menu item is gone).
		case cmd_RenameFolder:
			UpdateMailFolderMenusOnNextUpdate();
			break;
#endif
		case cmd_NewFolder:
		case cmd_NewNewsgroup:
		{
			MSG_FolderInfo*	folderInfo = GetSelectedFolder();
			CMessageFolder folder(folderInfo, GetMessagePane());
			if (folderInfo && (folder.IsNewsgroup() || folder.IsNewsHost()))
				inCommand = cmd_NewNewsgroup;	// no 'cmdHandled = true' here
			else
			{
				UFolderDialogs::ConductNewFolderDialog(folder);
				cmdHandled = true;
			}
			break;
		}
	}
	//-----------------------------------	
	// MSGLIB commands
	//-----------------------------------		
	if (!cmdHandled)
		cmdHandled = ObeyMessageLibraryCommand(inCommand, ioParam)		
			|| Inherited::ObeyCommand(inCommand, ioParam);
	//-----------------------------------	
	// Cleanup
	//-----------------------------------		
	// The following test against originalContext protects against the cases (quit, close)
	// when the object has been deleted.  The test against cmdHandled protects against
	// re-entrant calls to ListenToMessage.
	
	if (cmdHandled && mContext == originalContext)
		mContext->SetCurrentCommand(cmd_Nothing);

	if (! cmdHandled)
		mContext->SetCurrentCommand(saveCommand);
	return cmdHandled;
} // CMessageFolderView::ObeyCommand

#if defined(QAP_BUILD)		
//----------------------------------------------------------------------------------------
void CMessageFolderView::GetQapRowText(
	TableIndexT			inRow,
	char*				outText,
	UInt16				inMaxBufferLength) const
// Calculate the text and (if ioRect is not passed in as null) a rectangle that fits it.
// Return result indicates if any of the text is visible in the cell
//----------------------------------------------------------------------------------------
{
	if (!outText || inMaxBufferLength == 0)
		return;

	cstring rowText("");
	char tempStr[cMaxMailFolderNameLength];
	CMessageFolder folder(inRow, GetMessagePane());
	short colCount = mTableHeader->CountVisibleColumns();

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
		switch (GetCellDataType(aCell))
		{
			case kFolderNameColumn:
				Boolean expanded;
				if (CellHasDropFlag(aCell, expanded))
				{
					if (expanded)
						rowText += "-";
					else
						rowText += "+";
				}
				else
					rowText += " ";

				CMailFolderMixin::GetFolderNameForDisplay(tempStr, folder);
				NET_UnEscape(tempStr);
				rowText += tempStr;
				break;

			case kFolderNumUnreadColumn:
				if (folder.CanContainThreads())
					if (folder.CountMessages() != 0)
					{
						sprintf(tempStr, "%d", folder.CountUnseen());
						rowText += tempStr;
					}
				break;

			case kFolderNumTotalColumn:
				if (folder.CanContainThreads())
					if (folder.CountMessages() != 0)
					{
						sprintf(tempStr, "%d", folder.CountMessages());
						rowText += tempStr;
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
} // CMessageFolderView::GetQapRowText
#endif //QAP_BUILD
