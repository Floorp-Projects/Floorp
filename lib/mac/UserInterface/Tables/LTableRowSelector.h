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

/*
	
	Created 3/26/96 - Tim Craycroft

	
	A subclass of LTableSelector that always selects entire rows.
	Supports multiple selection and std selection extension semantics.
	
	Kind of lame implementation.  We store an array (LArray) 
	of bytes, each byte representing a row.  Easier than expanding
	and collapsing an array of bits.
*/


#pragma once

#include <UTableHelpers.h>
#include <LArray.h>

class	LTableRowSelector : public LTableSelector,
							public LArray {
public:
		LTableRowSelector(LTableView *inTableView, Boolean inAllowMultiple=true);
						
	// TSM - Added two methods below for compatibility with PP 1.5
	virtual	STableCell	GetFirstSelectedCell() const;
	virtual	TableIndexT	GetFirstSelectedRow() const;

	virtual Boolean		CellIsSelected(const STableCell	&inCell) const;
	
	virtual void		SelectCell(const STableCell	&inCell);
	virtual void		SelectAllCells();
	virtual void		UnselectCell(const STableCell &inCell);
	virtual void		UnselectAllCells();
	
	virtual void		ClickSelect(const STableCell		&inCell,
									const SMouseDownEvent	&inMouseDown);
	
	virtual Boolean		DragSelect(	const STableCell		&inCell,
									const SMouseDownEvent	&inMouseDown);

	virtual void		InsertRows(	Uint32					inHowMany,
									TableIndexT				inAfterRow);
									
	virtual void		RemoveRows(
								Uint32		inHowMany,
								TableIndexT	inFromRow );
	TableIndexT			GetSelectedRowCount() const { return mSelectedRowCount; }
	virtual void 		DoSelect(TableIndexT inRow, Boolean inSelect, Boolean inHilite, Boolean inNotify = false);	

protected:
	virtual void DoSelectAll(Boolean inSelect, Boolean inNotify);		
	void DoSelectRange(TableIndexT inFrom, TableIndexT inTo, Boolean inSelect, Boolean inNotify);
	
	void ExtendSelection(TableIndexT inRow);

protected:
	
	TableIndexT			mAnchorRow;
	TableIndexT			mExtensionRow;
	Boolean				mAllowMultiple;
	TableIndexT 		mSelectedRowCount;
	RgnHandle			mAddToSelection;
	RgnHandle			mRemoveFromSelection;
	RgnHandle			mInvertSelection;

};

