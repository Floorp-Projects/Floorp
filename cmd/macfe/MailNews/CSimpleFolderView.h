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



// CSimpleFolderView.h

#pragma once

#include "CMailFlexTable.h"

class CThreadView;
class CThreadWindow;
class CStr255;
class CMessageFolder;



//------------------------------------------------------------------------------
//		¥ CSimpleFolderView
//------------------------------------------------------------------------------
//
class CSimpleFolderView : public CMailFlexTable
{
private:
	typedef CMailFlexTable Inherited;

public:
	enum
	{	class_ID = 'SFVw'
	};

						CSimpleFolderView(LStream *inStream);
	virtual				~CSimpleFolderView();

protected:

	//-----------------------------------
	// Public folder fun
	//-----------------------------------
public:
	void				LoadFolderList(CNSContext* inContext);

	//-----------------------------------
	// Command implementation
	//-----------------------------------
	MSG_FolderInfo*		GetSelectedFolder() const;
	void				SelectFirstFolderWithFlags(uint32 inFlags);
	void				SelectFolder(const MSG_FolderInfo* inFolderInfo);
	void				SelectHilitesWholeRow(const Boolean inWholeRow)
									{ mSelectHilitesWholeRow = inWholeRow;};
	virtual void		DeleteSelection();
	
	//-----------------------------------
	// Drawing (specials from CStandardFlexTable)
	//-----------------------------------
protected:
	virtual ResIDT		GetIconID(TableIndexT inRow) const;
	virtual UInt16		GetNestedLevel(TableIndexT inRow) const;

	virtual TableIndexT	GetHiliteColumn() const;
	virtual Boolean		GetHiliteTextRect(
									const TableIndexT	inRow,
									Rect&				outRect) const;

	virtual void		ApplyTextStyle(TableIndexT inRow) const;
	virtual void		DrawCellContents(
									const STableCell	&inCell,
									const Rect			&inLocalRect);
	
	//-----------------------------------
	// Hierarchy	
	//-----------------------------------
	virtual Boolean		CellHasDropFlag(const STableCell& inCell, Boolean& outIsExpanded) const;
	virtual void		SetCellExpansion(const STableCell& inCell, Boolean inExpand);
	virtual void		GetMainRowText(
									TableIndexT			inRow,
									char*				outText,
									UInt16				inMaxBufferLength) const;

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

	//-----------------------------------
	// Messaging
	//-----------------------------------
	virtual void		ListenToMessage(MessageT inMessage, void* ioParam);


	//-----------------------------------
	// Data change notification (callbacks from MSGlib)
	//-----------------------------------
	virtual void		ChangeFinished(
									MSG_Pane*		inPane,
									MSG_NOTIFY_CODE	inChangeCode,
									TableIndexT		inStartRow,
									SInt32			inRowCount);
	void				FoldersChanged(
									TableIndexT		inStartRow,
									SInt32			inRowCount) const;
protected:
	void				UpdateMailFolderMenusOnNextUpdate()
									{ mUpdateMailFolderMenusOnNextUpdate = true; }

	//-----------------------------------
	// Data
	//-----------------------------------
	
	Boolean				mSelectHilitesWholeRow;	// select whole row vs. just the name

	Boolean				mUpdateMailFolderMenusWhenComplete;
	Boolean				mUpdateMailFolderMenusOnNextUpdate;
};

