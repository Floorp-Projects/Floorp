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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CMenuTable.h"
#include <Menus.h>
#include <UDrawingUtils.h>
#include <UTextTraits.h>
#include <LTableMonoGeometry.h>
#include <LTableSingleSelector.h>
#include <LTableArrayStorage.h>
#include <LDropFlag.h>
//#include <QAP_Assist.h>

#include "CPrefsMediator.h"

#pragma options align= packed

struct CellData
{
	MessageT	message;
	Str255		label;
};

#pragma options align=reset

//----------------------------------------------------------------------------------------
CMenuTable::CMenuTable(LStream *inStream)
: 		LTextHierTable(inStream)
//,		CQAPartnerTableMixin(this)
//----------------------------------------------------------------------------------------
{
	*inStream >> mLeafTextTraits;
	*inStream >> mParentTextTraits;
	*inStream >> mFirstIndent;
	*inStream >> mLevelIndent;
	*inStream >> mMenuID;
}


//----------------------------------------------------------------------------------------
CMenuTable::~CMenuTable()
//----------------------------------------------------------------------------------------
{
}


//-----------------------------------
void CMenuTable::AddTableLabels(
	TableIndexT	&currentRow,
	ResIDT		menuID,
	Boolean		firstLevel)
// Recursive.  Adds table entries by using a hierarchical menu structure.
//-----------------------------------
{
	MenuHandle hMenu = nil;
	Int16**	theMcmdH = nil;
	try
	{
		hMenu = ::GetMenu(menuID);
		ThrowIfResFail_(hMenu);
		short iItemCnt = ::CountMItems(hMenu);
		theMcmdH = (Int16**)::GetResource('Mcmd', menuID);
		ThrowIfResFail_(theMcmdH);
		if (**theMcmdH != iItemCnt)
		{
			ReleaseResource((Handle)theMcmdH);
			theMcmdH = nil;
			throw((OSErr)resNotFound);
		}
		StHandleLocker hl((Handle)theMcmdH);
		MessageT* messages = (MessageT *)(*theMcmdH + 1);
		// There does not seem to be a way to add a row that is at a higher
		// level than the one immediately preceding it. For this reason we
		// must make two pass through the labels. The first pass creates all of
		// the rows for a given level and the next pass adds the children.
		int menuItem;
		TableIndexT	currentRowSaved = currentRow;
		for (menuItem = 1; menuItem <= iItemCnt; ++menuItem)
		{
			CellData theData;
			::GetMenuItemText(hMenu, menuItem, theData.label);
			if (messages)
				theData.message = messages[menuItem - 1];
			else
				theData.message = 0;
			short cmdChar;
			::GetItemCmd(hMenu, menuItem, &cmdChar);
			Boolean	hasSubMenus = (hMenuCmd == cmdChar);
			if (!firstLevel && (1 == menuItem))
			{
				InsertChildRows(	1, currentRow++, &theData,
									sizeof(MessageT) + theData.label[0] + 1,
									hasSubMenus, false);
			}
			else
			{
				InsertSiblingRows(	1, currentRow++, &theData,
									sizeof(MessageT) + theData.label[0] + 1,
									hasSubMenus, false);
			}
		}
		currentRow = currentRowSaved;
		for (menuItem = 1; menuItem <= iItemCnt; ++menuItem)
		{
			short cmdChar;
			::GetItemCmd(hMenu, menuItem, &cmdChar);
			Boolean	hasSubMenus = (hMenuCmd == cmdChar);
			++currentRow;
			if (hasSubMenus)
			{
				short	markChar;
				::GetItemMark(hMenu, menuItem, &markChar);
	//			AddTableLabels(currentRow, markChar & 0x0F, false);
				AddTableLabels(currentRow, markChar, false);
			}
		}
	}
	catch(...)
	{
		if (hMenu)
			ReleaseResource((Handle)hMenu);
		if (theMcmdH)
			ReleaseResource((Handle)theMcmdH);
		throw;
	}
	if (hMenu)
		ReleaseResource((Handle)hMenu);
	if (theMcmdH)
		ReleaseResource((Handle)theMcmdH);
}

//----------------------------------------------------------------------------------------
void CMenuTable::FinishCreate()
//----------------------------------------------------------------------------------------
{
	// geometry
	SDimension16 tableSize;
	GetFrameSize(tableSize);

	StTextState	ts;
	UTextTraits::SetPortTextTraits(mParentTextTraits);
	FontInfo	fi;

	::GetFontInfo(&fi);
	Uint16		height = fi.ascent + fi.descent + fi.leading + 1;
	// the height should never be less than 12
	height = height > 12? height: 12;

	LTableMonoGeometry		*theGeometry =
		new LTableMonoGeometry(	this,
								tableSize.width,
								fi.ascent + fi.descent + fi.leading + 1);
	SetTableGeometry(theGeometry);
	// selector
	LTableSingleSelector	*theSelector =
		new LTableSingleSelector(this);
	SetTableSelector(theSelector);
	// storage
	LTableArrayStorage		*theStorage =
		new LTableArrayStorage(this, (unsigned long)0);
	SetTableStorage(theStorage);

	// rows and cols
	InsertCols(1, 0, "", 0, false);

	TableIndexT currentRow = 0;
	AddTableLabels(currentRow, mMenuID);
	UnselectAllCells();
}

//----------------------------------------------------------------------------------------
void CMenuTable::SelectionChanged()
//----------------------------------------------------------------------------------------
{
	BroadcastMessage(msg_SelectionChanged, this);
}

//----------------------------------------------------------------------------------------
MessageT CMenuTable::GetSelectedMessage()
//----------------------------------------------------------------------------------------
{
	MessageT result = 0;
	STableCell		theCell = GetFirstSelectedCell();	// we have only one
	CellData		theData;
	unsigned long	theSize = sizeof(theData);
	if (theCell.row && theCell.col)
	{
		TableIndexT	index;
		CellToIndex(theCell, index);
		index = GetWideOpenIndex(index);
		STableCell	expandedCell;
		IndexToCell(index, expandedCell);
		GetCellData(expandedCell, &theData, theSize);
		if (theSize > sizeof(theData.message))
		{
			result = theData.message;
		}
	}
	return result;
}

//----------------------------------------------------------------------------------------
TableIndexT	CMenuTable::FindMessage( MessageT message )
// Returns Wide Open index
//----------------------------------------------------------------------------------------
{
	TableIndexT index = 0;
	CellData cellData;
	STableCell cell(0,0);
	unsigned long	theSize = sizeof(cellData);
	TableIndexT numberRows, numberCols;
	GetWideOpenTableSize( numberRows, numberCols ) ;
	
	for ( TableIndexT i = 1; i<= numberRows; i++ )
	{
		
		IndexToCell( i, cell );
		GetCellData( cell , &cellData, theSize );
		if ( cellData.message == message )
		{
			index = i;
			break;
		}
	}
	return index;
}

//----------------------------------------------------------------------------------------
void CMenuTable::DrawCell(
	const STableCell	&inCell,
	const Rect			&inLocalRect)
//----------------------------------------------------------------------------------------
{
	TableIndexT	woRow = mCollapsableTree->GetWideOpenIndex(inCell.row);
	
	DrawDropFlag(inCell, woRow);
	
	STableCell	woCell(woRow, inCell.col);
	CellData	cellData;
	Uint32		dataSize = sizeof(cellData);

	GetCellData(woCell, &cellData, dataSize);
	
	ResIDT	textTraitsID = mLeafTextTraits;
	if (mCollapsableTree->IsCollapsable(woRow)) {
		textTraitsID = mParentTextTraits;
	}
	UTextTraits::SetPortTextTraits(textTraitsID);
	
	Uint32	nestingLevel = mCollapsableTree->GetNestingLevel(woRow);
	FontInfo	fi;
	::GetFontInfo(&fi);
	Int16		height = inLocalRect.top +
							(inLocalRect.bottom - inLocalRect.top) / 2 +
							fi.ascent - ((fi.ascent + fi.descent) / 2);

	MoveTo(inLocalRect.left + mFirstIndent + nestingLevel * mLevelIndent,
			height);
	DrawString(cellData.label);
}

//----------------------------------------------------------------------------------------
void CMenuTable::GetHiliteRgn(RgnHandle ioHiliteRgn)
//	Pass back a Region containing the frames of all selected cells which
//	are within the visible rectangle of the Table
//
//	Caller must allocate space for the region
//----------------------------------------------------------------------------------------
{
	::SetEmptyRgn(ioHiliteRgn);			// Assume no visible selection

	Rect	visRect;
	GetRevealedRect(visRect);			// Check if Table is revealed
	if (!::EmptyRect(&visRect)) {
		PortToLocalPoint(topLeft(visRect));
		PortToLocalPoint(botRight(visRect));
		
		STableCell	topLeftCell, botRightCell;
		FetchIntersectingCells(visRect, topLeftCell, botRightCell);
		
		RgnHandle	cellRgn = ::NewRgn();
		
		STableCell	cell;				// Loop thru all cells
		for (cell.row = topLeftCell.row; cell.row <= botRightCell.row; cell.row++) {
			for (cell.col = topLeftCell.col; cell.col <= botRightCell.col; cell.col++) {
				if (CellIsSelected(cell)) {
					Rect	cellRect;
					GetLocalCellRect(cell, cellRect);
					cellRect.left += (mFirstIndent - 1);
					::RectRgn(cellRgn, &cellRect);
					::UnionRgn(ioHiliteRgn, cellRgn, ioHiliteRgn);
				}
			}
		}
		
		::DisposeRgn(cellRgn);
	}
}

//----------------------------------------------------------------------------------------
void CMenuTable::HiliteSelection(
	Boolean	inActively,
	Boolean inHilite)
//	Draw or undraw hiliting for the current selection in either the
//	active or inactive state
//----------------------------------------------------------------------------------------
{
	STableCell	theCell;
	
	while (GetNextSelectedCell(theCell))
	{
		if (inActively) {
			HiliteCellActively(theCell, inHilite);
		} else {
			HiliteCellInactively(theCell, inHilite);
		}
	}
}

//----------------------------------------------------------------------------------------
void CMenuTable::HiliteCellActively(const STableCell	&inCell, Boolean /* inHilite */)
//----------------------------------------------------------------------------------------
{
	Rect	cellFrame;
	if (GetLocalCellRect(inCell, cellFrame) && FocusExposed())
	{
        StColorPenState saveColorPen;   // Preserve color & pen state
        StColorPenState::Normalize();

		UDrawingUtils::SetHiliteModeOn();
		cellFrame.left += (mFirstIndent - 1);
		::InvertRect(&cellFrame);
	}
}

//----------------------------------------------------------------------------------------
void CMenuTable::HiliteCellInactively(const STableCell &inCell, Boolean /* inHilite */)
//----------------------------------------------------------------------------------------
{
	Rect	cellFrame;
	if (GetLocalCellRect(inCell, cellFrame) && FocusExposed())
	{
        StColorPenState saveColorPen;   // Preserve color & pen state
        StColorPenState::Normalize();

		cellFrame.left += (mFirstIndent - 1);
		UDrawingUtils::SetHiliteModeOn();
		::PenMode(srcXor);
		::FrameRect(&cellFrame);
	}
}

//----------------------------------------------------------------------------------------
void CMenuTable::ClickSelf(const SMouseDownEvent &inMouseDown)
//----------------------------------------------------------------------------------------
{
	STableCell	hitCell;
	SPoint32	imagePt;
	
	LocalToImagePoint(inMouseDown.whereLocal, imagePt);
	
	if (GetCellHitBy(imagePt, hitCell))
	{
		// Before calling CPrefsPaneManager::CanSwitch() we should check to see
		// if the click is on a different cell. (No point in calling CanSwitch()
		// if we aren't going to switch.)
		Boolean	switchAllowed = true;
		if (hitCell != GetFirstSelectedCell())
		{
			switchAllowed = CPrefsMediator::CanSwitch();
		}
		if (switchAllowed)
		{
											// Click is inside hitCell
											// Check if click is inside DropFlag
			TableIndexT	woRow = mCollapsableTree->GetWideOpenIndex(hitCell.row);
			Rect	flagRect;
			CalcCellFlagRect(hitCell, flagRect);
			
			if (mCollapsableTree->IsCollapsable(woRow) &&
				::PtInRect(inMouseDown.whereLocal, &flagRect))
			{
											// Click is inside DropFlag
				FocusDraw();
				Boolean	expanded = mCollapsableTree->IsExpanded(woRow);
				if (LDropFlag::TrackClick(flagRect, inMouseDown.whereLocal, expanded))
				{
											// Mouse released inside DropFlag
											//   so toggle the Row
					if (inMouseDown.macEvent.modifiers & optionKey)
					{
											// OptionKey down means to do
											//   a deep collapse/expand						
						if (expanded)
						{
							DeepCollapseRow(woRow);
						}
						else
						{
							DeepExpandRow(woRow);
						}
					
					}
					else
					{				// Shallow collapse/expand
						if (expanded)
						{
							CollapseRow(woRow);
						}
						else
						{
							ExpandRow(woRow);
						}
					}
				}
			}
			if (ClickSelect(hitCell, inMouseDown))
			{
											// Click outside of the DropFlag
				ClickCell(hitCell, inMouseDown);
			}
		}	// else don't allow the selection to change
	}
	else
	{							// Click is outside of any Cell
		UnselectAllCells();
	}
}

#pragma mark -
#if defined(QAP_BUILD)

//-----------------------------------
void CMenuTable::QapGetListInfo(PQAPLISTINFO pInfo)
//-----------------------------------
{
	TableIndexT	outRows, outCols, lcv;
	
	if (pInfo == nil)
		return;
	
	GetWideOpenTableSize(outRows, outCols);
	
	for (lcv = 0; lcv < outRows; lcv++)
	{
		if (IsCollapsable(lcv) && !IsExpanded(lcv))
		{
			ExpandRow(lcv);
		}
	}

	// fetch vertical scrollbar Macintosh control
	ControlHandle macVScroll = NULL;
	LScroller *myScroller = dynamic_cast<LScroller *>(GetSuperView());
	if (myScroller != NULL)
	{
		if (myScroller->GetVScrollbar() != NULL)
			macVScroll = myScroller->GetVScrollbar()->GetMacControl();
	}

	pInfo->itemCount	= (short)outRows;
	pInfo->topIndex 	= 0;
	pInfo->itemHeight 	= GetRowHeight(0);
	pInfo->visibleCount = outRows;
	pInfo->vScroll 		= macVScroll;
	pInfo->isMultiSel 	= false;
	pInfo->isExtendSel 	= false;
	pInfo->hasText 		= true;
}


//-----------------------------------
Ptr CMenuTable::QapAddCellToBuf(Ptr pBuf, Ptr pLimit, const STableCell& sTblCell)
//-----------------------------------
{
	char 		str[256];
	short		len = 0;
	CellData 	theData;

	UInt32 dataSize = sizeof(theData);
	GetCellData(sTblCell, (void *)&theData, dataSize);

	strncpy(str, (const char *)&theData.label[1], (short)theData.label[0]);
	str[(short)theData.label[0]] = '\0';
	len = strlen(str) + 1;

	if (pBuf + sizeof(short) + len >= pLimit)
		return NULL;

	*(unsigned short *)pBuf = sTblCell.row - 1;
	if (CellIsSelected(sTblCell))
		*(unsigned short *)pBuf |= 0x8000;

	pBuf += sizeof(short);

	strcpy(pBuf, str);
	pBuf += len;

	return pBuf;
}

#endif //QAP_BUILD
