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
	
	Created 3/25/96 - Tim Craycroft


	This class works with an LTableView and an LTableHeader.
	All rows have the same height. Column positions and widths
	are determined by the header view.
*/


#pragma once

#include <UTableHelpers.h>
#include <LTableView.h>

#include "LTableHeader.h"


class	LFlexTableGeometry : public LTableGeometry 
{
public:
						LFlexTableGeometry(
								LTableView		*inTableView,
								LTableHeader	*inTableHeader);
	
	virtual void		GetImageCellBounds(
								const STableCell	&inCell,
								Int32				&outLeft,
								Int32				&outTop,
								Int32				&outRight,
								Int32				&outBottom) const;
								
	virtual TableIndexT	GetRowHitBy(
								const SPoint32	&inImagePt) const;
								
	virtual TableIndexT	GetColHitBy(
								const SPoint32	&inImagePt) const;
								
	virtual void		GetTableDimensions(
								Uint32		&outWidth,
								Uint32		&outHeight) const;
	
	// All rows have the same height
	virtual Uint16		GetRowHeight(
								TableIndexT	inRow) const;
	
	// Sets row height for ALL rows							
	virtual void		SetRowHeight(
								Uint16		inHeight,
								TableIndexT	inFromRow,
								TableIndexT	inToRow);
								
	virtual Uint16		GetColWidth(
								TableIndexT	inCol) const;
								
	
	// DO NOT CALL.  Column widths are controlled by table headewr
	virtual void		SetColWidth(
								Uint16		inWidth,
								TableIndexT inFromCol,
								TableIndexT	inToCol);
								

	virtual void		InsertRows(
								Uint32		inHowMany,
								TableIndexT	inAfterRow );
	virtual void		RemoveRows(
								Uint32		inHowMany,
								TableIndexT	inFromRow);
								
protected:
	UInt32			CountRows() const;
	
	LTableHeader*	mTableHeader;
	UInt16			mRowHeight;
	UInt32			mRowCount;					
};



