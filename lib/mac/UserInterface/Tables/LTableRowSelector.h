/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

	virtual void		MakeSelectionRegion(RgnHandle ioRgnHandle, TableIndexT hiliteColumn);
	virtual void		SetSelectionFromRegion(RgnHandle inRgnHandle);
	
protected:
	virtual void DoSelectAll(Boolean inSelect, Boolean inNotify);		
	void DoSelectRange(TableIndexT inFrom, TableIndexT inTo, Boolean inSelect, Boolean inNotify);
	
	void ExtendSelection(TableIndexT inRow);

protected:
	
	TableIndexT			mAnchorRow;
	TableIndexT			mExtensionRow;
	Boolean				mAllowMultiple;
	TableIndexT 		mSelectedRowCount;
};

