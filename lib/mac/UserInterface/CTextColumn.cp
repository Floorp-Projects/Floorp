/*
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
               

// This class is a minimal override of PowerPlant's LTextColumn
// to allow instrumentation through QA-Partner.
//
// It should be used everywhere in place of LTextColumn.

#if 0	//еее don't activate QAP in Mozilla yet

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CTextColumn.h"


// ---------------------------------------------------------------------------
//		е CTextColumn(LStream*)
// ---------------------------------------------------------------------------
//	Construct from the data in a Stream

CTextColumn::CTextColumn(
	LStream*	inStream)
	
	:	super(inStream)
	,	CQAPartnerTableMixin(this)
{
}

// ---------------------------------------------------------------------------
//		е ~CTextColumn
// ---------------------------------------------------------------------------
//	Destructor

CTextColumn::~CTextColumn()
{
}


#pragma mark -
#if defined(QAP_BUILD)

//-----------------------------------
void CTextColumn::QapGetListInfo(PQAPLISTINFO pInfo)
//-----------------------------------
{
	TableIndexT	outRows, outCols;
	
	if (pInfo == nil)
		return;
	
	GetTableSize(outRows, outCols);

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
Ptr CTextColumn::QapAddCellToBuf(Ptr pBuf, Ptr pLimit, const STableCell& sTblCell)
//-----------------------------------
{
	Str255	str;
	Uint32	len = sizeof(str) - 1;
	GetCellData(sTblCell, str, len);

	len = str[0];
	str[++ len] = '\0';

	if (pBuf + sizeof(short) + len >= pLimit)
		return NULL;

	*(unsigned short *)pBuf = sTblCell.row - 1;
	if (CellIsSelected(sTblCell))
		*(unsigned short *)pBuf |= 0x8000;

	pBuf += sizeof(short);

//	strcpy(pBuf, &str[1]);		// no stdlib here...
//	pBuf += len;

	Byte* string = str;			// ...let's copy it ourselves
	do
	{
		*pBuf ++ = *(++ string);
	} while (*string);

	return pBuf;
}

#endif //QAP_BUILD

#endif //0	//еее don't activate QAP in Mozilla yet
