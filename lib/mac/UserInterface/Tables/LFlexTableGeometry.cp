/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
	
	Created 3/25/96 - Tim Craycroft

*/

#include "LFlexTableGeometry.h"


/*
	LFlexTableGeometry::LFlexTableGeometry	
*/
LFlexTableGeometry::LFlexTableGeometry(	LTableView		*inTableView,
										LTableHeader	*inTableHeader)
	: LTableGeometry(inTableView)
{
	mTableHeader = inTableHeader;
	mRowCount 	 = 0;
	
	// set to a reasonable default
	//
	// this class has no other inteligence for determining this value
	//
	SetRowHeight(16, 0, 0);
}



/*
	LFlexTableGeometry::GetImageCellBounds	
	
	Get the column position from the header view and compute
	the row position.
*/
void		
LFlexTableGeometry::GetImageCellBounds(	const STableCell	&inCell,
										Int32				&outLeft,
										Int32				&outTop,
										Int32				&outRight,
										Int32				&outBottom) const
{	
	if (inCell.col > 0 && inCell.col <= mTableHeader->CountColumns())
	{
		outLeft  = mTableHeader->GetColumnPosition(inCell.col);
		outRight = outLeft + mTableHeader->GetColumnWidth(inCell.col);
			
		outTop 		= (inCell.row-1) * mRowHeight;
		outBottom	= outTop + mRowHeight;

		SDimension32 imageSize;
		mTableView->GetImageSize(imageSize);
		if ( outLeft < 0 ) outLeft = 0;
		if ( outRight > imageSize.width ) outRight = imageSize.width;
	}
	else {
		outLeft = outTop = outRight = outBottom = 0;
	}
}



/*
	LFlexTableGeometry::GetRowHitBy	
*/
TableIndexT	
LFlexTableGeometry::GetRowHitBy(const SPoint32	&inImagePt) const
{
	return (inImagePt.v / mRowHeight) + 1;
}
								

/*
	LFlexTableGeometry::GetColHitBy	
*/
TableIndexT	
LFlexTableGeometry::GetColHitBy(const SPoint32	&inImagePt) const
{
	UInt16	i, nColumns;
	
	nColumns = mTableHeader->CountVisibleColumns();
	
	for (i=2; i<=nColumns; i++)
	{
		if (inImagePt.h < mTableHeader->GetColumnPosition(i)) {
			return i-1;
		}
	}
	
	return nColumns;

}



/*
	LFlexTableGeometry::SetRowHeight
	
	Changes row height for ALL rows and sets the scroll
	unit of the table view to the row height.	
*/
void
LFlexTableGeometry::SetRowHeight(	Uint16		inHeight,
									TableIndexT	inFromRow,
									TableIndexT	inToRow)
{
	#pragma unused(inFromRow, inToRow)
	
	mRowHeight = inHeight;
	
	SPoint32	scrollUnit;
	scrollUnit.h = 0;
	scrollUnit.v = mRowHeight;
	mTableView->SetScrollUnit(scrollUnit);
}



/*
	LFlexTableGeometry::GetTableDimensions
*/
void
LFlexTableGeometry::GetTableDimensions(	Uint32		&outWidth,
										Uint32		&outHeight) const
{
	outHeight 	= CountRows() * mRowHeight;
	outWidth	= mTableHeader->GetHeaderWidth();
}										
	


/*
	LFlexTableGeometry::GetRowHeight
*/
Uint16
LFlexTableGeometry::GetRowHeight(TableIndexT	inRow) const
{
	#pragma unused(inRow)
	return mRowHeight;
}
								
								
/*
	LFlexTableGeometry::GetColWidth
*/
Uint16		
LFlexTableGeometry::GetColWidth(TableIndexT	inCol) const
{
	return mTableHeader->GetColumnWidth(inCol);
}
							
							

/*
	LFlexTableGeometry::InsertRows
*/
void		
LFlexTableGeometry::InsertRows(	Uint32		inHowMany,
								TableIndexT	inAfterRow )
{
	#pragma unused(inAfterRow)
	
	mRowCount += inHowMany;
}							

	
/*
	LFlexTableGeometry::RemoveRows
*/
void		
LFlexTableGeometry::RemoveRows(	Uint32		inHowMany,
								TableIndexT	inFromRow)
{
	#pragma unused(inFromRow)

	mRowCount -= inHowMany;
}
								
							

/*
	LFlexTableGeometry::CountRows
*/
UInt32				
LFlexTableGeometry::CountRows() const
{
	return mRowCount;
}
	
	
	
	
/*
	LFlexTableGeometry::SetColWidth
	
	Invalid, since column widths are controlled by the mTableHeader
	widget.
*/
void		
LFlexTableGeometry::SetColWidth(Uint16		inWidth,
								TableIndexT inFromCol,
								TableIndexT	inToCol)
{
	#pragma unused(inWidth, inFromCol, inToCol)
	
	// We get our widths from mTableHeader
	SignalPStr_("\pLFlexTableGeometry does not support SetColWidth");
}								




								
							
		