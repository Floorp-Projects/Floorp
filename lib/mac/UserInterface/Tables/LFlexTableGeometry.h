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



