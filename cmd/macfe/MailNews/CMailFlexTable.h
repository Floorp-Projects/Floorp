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


#pragma once

#include "msgcom.h"
#include "CStandardFlexTable.h"
#include "MailNewsCallbacks.h"

const kMysticUpdateThreshHold = 10;


class CMailNewsContext;
class CMailProgressWindow;
class CMailSelection;

//========================================================================================
class CMailFlexTable
	:	public CStandardFlexTable
	,	public CMailCallbackListener
// Like CStandardFlexTable, an abstract class.
//========================================================================================
{
private:
	typedef CStandardFlexTable Inherited;
public:
	enum 
	{	class_ID = 'mfTb'
	};
	
	enum
	{
		eMailNewsSelectionDragItemRefNum = 0
	};	

										CMailFlexTable(LStream *inStream);
	virtual								~CMailFlexTable();
	virtual void						SetRowCount();
	Boolean								IsStillLoading() const { return mStillLoading; }
	
	//-----------------------------------
	// Drawing
	//-----------------------------------
	virtual void		DrawSelf();

	//-----------------------------------
	// Selection
	//-----------------------------------
	virtual Boolean						GetSelection(CMailSelection&);
		// CStandardFlexTable has one-based lists which are lists of TableIndexT.
		// CMailSelection requires zero-based lists which are lists of MSG_ViewIndex
		// This routine clones and converts.
	virtual void						OpenSelection();
	
	//-----------------------------------
	// Drag/drop
	//-----------------------------------
	FlavorType							GetMessageFlavor(TableIndexT /*inMessageRow*/) const
											{ return mDragFlavor; }

	virtual void						AddSelectionToDrag(
											DragReference inDragRef, RgnHandle inDragRgn);
	virtual void						AddRowToDrag(
											TableIndexT		inRow,
											DragReference	inDragRef,
											RgnHandle		inDragRgn);

	Boolean								GetSelectionFromDrag(
											DragReference	inDragRef,
											CMailSelection&	outSelection);
	
	CNSContext*							GetContext() const { return mContext; }

	//-----------------------------------
	// CMailCallbackListener overrides
	//-----------------------------------
	virtual void ChangeStarting(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount);
	virtual void ChangeFinished(
		MSG_Pane*		inPane,
		MSG_NOTIFY_CODE	inChangeCode,
		TableIndexT		inStartRow,
		SInt32			inRowCount);
	virtual void 		PaneChanged(
		MSG_Pane*		inPane,
		MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
		int32 value);
	MSG_Pane*			GetMessagePane(void) const
		{ return mMsgPane; }
	virtual	void		DestroyMessagePane( MSG_Pane* inPane );
	void				ListenToMessage(MessageT inMessage, void* ioParam);

protected:

	void				SetMessagePane(MSG_Pane* inPane);

	static char* 		GetTextFromDrag(
							DragReference inDragRef,
							ItemReference inItemRef);
							// Caller must XP_FREE the result.
	
	static MSG_FolderInfo*	GetFolderInfoFromURLDrag(
							DragReference inDragRef,
							ItemReference inItemRef);
							// 	Check if this drag is the URL of a folder
							//	and returns the folderInfo if it is. 

	static MessageKey	MessageKeyFromURLDrag(
							DragReference inDragRef,
							ItemReference inItemRef);
							// 	Check if this drag is the URL of a message
							//	and returns the message key if it is.
	
	//-----------------------------------
	// Commands
	//-----------------------------------
protected:
	virtual void		FindCommandStatus(
							CommandT	inCommand,
							Boolean		&outEnabled,
							Boolean		&outUsesMark,
							Char16		&outMark,
							Str255		outName);
	virtual Boolean		ObeyCommand(
							CommandT	inCommand,
							void		*ioParam);
	Boolean				FindMessageLibraryCommandStatus(
							CommandT			inCommand,
							Boolean				&outEnabled,
							Boolean				&outUsesMark,
							Char16				&outMark,
							Str255				outName);
		// returns false if not a msglib command.
	Boolean				ObeyMessageLibraryCommand(
							CommandT	inCommand,
							void		*ioParam);
	void				NotifySelfAll() {	// Manual notification for BE bugs!
							ChangeStarting(mMsgPane, MSG_NotifyAll, 0, 0);
							ChangeFinished(mMsgPane, MSG_NotifyAll, 0, 0);
						}
public:
	void				EnableStopButton(Boolean inBusy);
	
	//-----------------------------------
	// Internal helpers
	//-----------------------------------
protected:
	void				DrawCountCell(
							Int32 inCount,
							const Rect& inLocalRect);
	void				RowsChanged(TableIndexT inFirst, TableIndexT inCount);
			
	//-----------------------------------
	// Window Saving
	//-----------------------------------
private:
	virtual const TableIndexT* GetUpdatedSelectionList(TableIndexT& outSelectionSize);
		// This does an assert: the idea is to use only GetSelection, above.
protected:
	//-----------------------------------
	// Discloser support
	//-----------------------------------
	void				ToggleExpandAction(TableIndexT row);
protected:
	CNSContext*				mContext; // Belongs to the window, not us.
	SInt32					mMysticPlane; // for keeping track of callback level
	FlavorType				mDragFlavor;
	Boolean					mStillLoading;
	TableIndexT				mFirstRowToRefresh;
	TableIndexT				mLastRowToRefresh;
	Boolean					mClosing;
	CommandT				mUndoCommand; // cmd_Undo or cmd_Redo
	
private:
	MSG_Pane* mMsgPane; // MUST be private.  Must call Get/Set.
}; // class CMailFlexTable
