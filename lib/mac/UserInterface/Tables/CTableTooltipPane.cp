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


#include "CTableToolTipPane.h"

#include "CStandardFlexTable.h"
#include "UGraphicGizmos.h"


//----------------------------------------------------------------------------------------
CTableToolTipAttachment::CTableToolTipAttachment(LStream* inStream)
//----------------------------------------------------------------------------------------
	: Inherited(inStream)
{
	mPaneMustBeActive = false;
}

#pragma mark -

//----------------------------------------------------------------------------------------
CTableToolTipPane::CTableToolTipPane(LStream* inStream)
	:	CToolTipPane(inStream)
//----------------------------------------------------------------------------------------
{
} // CTableToolTipPane::CTableToolTipPane

//----------------------------------------------------------------------------------------
void CTableToolTipPane::DrawSelf()
//----------------------------------------------------------------------------------------
{
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	::EraseRect(&theFrame);
	::FrameRect(&theFrame);
	UTextTraits::SetPortTextTraits(mTextDrawingStuff.mTextTraitsID);
	::TextFace(mTextDrawingStuff.style);
	::TextMode(mTextDrawingStuff.mode);
	::RGBForeColor(&mTextDrawingStuff.color);
	theFrame.left += 2;
	if (mTextDrawingStuff.encoding == CStandardFlexTable::TextDrawingStuff::eDefault)
		UGraphicGizmos::PlaceStringInRect(mTip, theFrame, teFlushDefault);
	else
		UGraphicGizmos::DrawUTF8TextString(mTip, nil, 0, theFrame, teFlushDefault);
} // CTableToolTipPane::DrawSelf
	
//----------------------------------------------------------------------------------------
Boolean CTableToolTipPane::WantsToCancel(Point inMousePort)
// Cancel if we detect that we're now in a new cell, because the tooltip has to be
// recalculated.
//----------------------------------------------------------------------------------------
{
	STableCell currentCell;
	if (GetTableAndCell(inMousePort, currentCell))
	{
		static STableCell sLastCell(0, 0);
		if (currentCell == sLastCell)
			return false;
		sLastCell = currentCell;
	}
	return true;
} //  CTableToolTipPane::WantsToCancel

//----------------------------------------------------------------------------------------
void CTableToolTipPane::CalcTipText(
	LWindow*			inOwningWindow,
	const EventRecord&	inMacEvent,
	StringPtr			outTipText)
//----------------------------------------------------------------------------------------
{
	STableCell cell;
	Point where = inMacEvent.where;
	inOwningWindow->GlobalToPortPoint(where);
	outTipText[0] = 0;
	CStandardFlexTable* table = GetTableAndCell(where, cell);
	if (!table)
		return;
	table->CalcToolTipText(cell, outTipText, mTextDrawingStuff, mTruncationOnly);
} // CTableToolTipPane::CalcTipText

//----------------------------------------------------------------------------------------
void CTableToolTipPane::CalcFrameWithRespectTo(
	LWindow*				inOwningWindow,
	const EventRecord&		inMacEvent,
	Rect& 					outPortFrame)
//----------------------------------------------------------------------------------------
{
	StTextState theTextSaver;
	UTextTraits::SetPortTextTraits(mTextDrawingStuff.mTextTraitsID);
	::TextFace(mTextDrawingStuff.style);
	::TextMode(mTextDrawingStuff.mode);
	::RGBForeColor(&mTextDrawingStuff.color);
	
	Int16 theTextHeight
		= mTextDrawingStuff.mTextFontInfo.ascent
		+ mTextDrawingStuff.mTextFontInfo.descent
		+ mTextDrawingStuff.mTextFontInfo.leading
		+ (2 * 2);
	// Int16 theTextWidth = ::StringWidth(mTip) + 4; //(2 * ::CharWidth(char_Space));
	Int16 theTextWidth ;

	if(mTextDrawingStuff.encoding == CStandardFlexTable::TextDrawingStuff::eDefault)
	{
		theTextWidth = ::StringWidth(mTip) + 4;
	}
	else
	{
		theTextWidth = UGraphicGizmos::GetUTF8TextWidth(mTip, mTip.Length()) + 4;
	}

	inOwningWindow->FocusDraw();	

	Rect theOwningPortFrame;
	mOwningPane->CalcPortFrameRect(theOwningPortFrame);

	// Find which cell the mouse is over
	STableCell cell;
	Point where = inMacEvent.where;
	inOwningWindow->GlobalToPortPoint(where);
	CStandardFlexTable* table = GetTableAndCell(where, cell);
	if (!table)
		return;
	// Get a rectangle for the text.  Normally, it's the cell rect.  But if it's the hilite cell,
	// make sure we don't cover over the icon (unless we're forced to to make it fit)
	table->FocusDraw();
	table->GetLocalCellRect(cell, outPortFrame);
	outPortFrame.right--; // that's what CStandardFlexTable::DrawCell does
	if (cell.col == table->GetHiliteColumn())
	{
		Rect hiliteRect;
		table->GetHiliteTextRect(cell.row, false, hiliteRect);
		outPortFrame.left = hiliteRect.left;
		outPortFrame.left -= 2; // looks better.
	}
		
	table->LocalToPortPoint(topLeft(outPortFrame));
	table->LocalToPortPoint(botRight(outPortFrame));
	
	Int16 textRight = outPortFrame.left + theTextWidth;
	if (mTruncationOnly && textRight <= outPortFrame.right)
	{
		outPortFrame.right = outPortFrame.left; // No tooltip
		return;
	}
	else
		outPortFrame.right = outPortFrame.left + theTextWidth;

	inOwningWindow->FocusDraw();	
	ForceInPortFrame(inOwningWindow, outPortFrame);	
} //  CTableToolTipPane::CalcFrameWithRespectTo

//----------------------------------------------------------------------------------------
CStandardFlexTable* CTableToolTipPane::GetTableAndCell(
	Point				inMousePort,
	STableCell&			outCell)
//----------------------------------------------------------------------------------------
{
	CStandardFlexTable* table = dynamic_cast<CStandardFlexTable*>(mOwningPane);
	Assert_(table);
	if (table)
	{
		table->FocusDraw();
		Point mouseLocal = inMousePort;
		table->PortToLocalPoint(mouseLocal);
		SPoint32 whereImage;
		table->LocalToImagePoint(mouseLocal, whereImage);
		table->GetCellHitBy(whereImage, outCell);
	}
	return table;
} // CTableToolTipPane::GetTableAndCell

