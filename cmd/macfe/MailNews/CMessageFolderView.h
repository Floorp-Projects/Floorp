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



// CMessageFolderView.h

#pragma once

#include <LTableView.h>
#include <LDragAndDrop.h>

#include "CSimpleFolderView.h"
#include "msgcom.h"

class CThreadView;
class CThreadWindow;
class CStr255;
class CMessageFolder;
class CMailSelection;

//======================================
class CMessageFolderView : public CSimpleFolderView
//======================================
{
private:
	typedef CSimpleFolderView Inherited;
public:
	enum 
	{	class_ID = 'msFv'
	};
	
						CMessageFolderView(LStream *inStream);
	virtual				~CMessageFolderView();
							
	// ------------------------------------------------------------
	// Command implementation
	// ------------------------------------------------------------
	void				DoAddNewsGroup();
	
	virtual void		DeleteSelection();
	void				DoDeleteSelection(const CMailSelection& inSelection);
							
	void				GetLongWindowDescription(CStr255& outDescription);
	virtual	void		OpenRow(TableIndexT inRow);
	virtual void		SelectionChanged(); // maintain history
	virtual void		GetInfo(); // called from the base class.
	void				DropMessages(
							const CMailSelection& inSelection,
							const CMessageFolder& inDestFolder,
							Boolean doCopy);

protected:
	
	inline  void		OpenFolder(UInt32 inFolderIndex) { OpenRow(inFolderIndex); }
	
	// ------------------------------------------------------------
	// Hierarchy	
	// ------------------------------------------------------------
	virtual Boolean		CellInitiatesDrag(const STableCell& inCell) const;
	virtual Boolean 	GetRowDragRgn(TableIndexT inRow, RgnHandle ioHiliteRgn) const;
	
	// ------------------------------------------------------------
	// LDragAndDrop overrides
	// ------------------------------------------------------------
	virtual void 		AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef);
	virtual Boolean		ItemIsAcceptable(DragReference	inDragRef, ItemReference inItemRef);
	virtual void		ReceiveDragItem(
							DragReference	inDragRef,
							DragAttributes	inDragAttrs,
							ItemReference	inItemRef,
							Rect&			inItemBounds);	

	// Specials from CStandardFlexTable
	virtual Boolean		CanDoInlineEditing();
	virtual void		InlineEditorTextChanged();
	virtual void		InlineEditorDone();
	virtual Boolean		RowCanAcceptDrop(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	virtual Boolean		RowCanAcceptDropBetweenAbove(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	Boolean				GetSelectionAndCopyStatusFromDrag(
							DragReference	inDragRef,
							CMessageFolder&	inDestFolder,
							CMailSelection&	outSelection,
							Boolean&		outCopy);
	
	//-----------------------------------
	// Commands
	//-----------------------------------
	virtual void		FindCommandStatus(
							CommandT	inCommand,
							Boolean		&outEnabled,
							Boolean		&outUsesMark,
							Char16		&outMark,
							Str255		outName);
public:
	virtual Boolean		ObeyCommand(
							CommandT	inCommand,
							void		*ioParam);
protected:
	virtual Boolean		FindMessageLibraryCommandStatus(
							CommandT	inCommand,
							Boolean		&outEnabled,
							Boolean		&outUsesMark,
							Char16		&outMark,
							Str255		outName);

	//-----------------------------------
	// Messaging
	//-----------------------------------
	virtual void		ListenToMessage(MessageT inMessage, void* ioParam);
		// msg

public:
	void				WatchFolderForChildren(MSG_FolderInfo* inFolderInfo);

	// ------------------------------------------------------------
	// QA Partner support
	// ------------------------------------------------------------
#if defined(QAP_BUILD)		
public:
	virtual void		GetQapRowText(TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const;
#endif

	// ------------------------------------------------------------
	// Data
	// ------------------------------------------------------------
	
	Boolean				mUpdateMailFolderMenus;
}; // class CMessageFolderView

