/* ***** BEGIN LICENSE BLOCK *****
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
               

// This class is a minimal override of PowerPlant's LTextColumn
// to allow instrumentation through QA-Partner.
//
// It should be used everywhere in place of LTextColumn.

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CTextColumn.h"


// ---------------------------------------------------------------------------
//		¥ CTextColumn(LStream*)
// ---------------------------------------------------------------------------
//	Construct from the data in a Stream

CTextColumn::CTextColumn(
	LStream*	inStream)
	
	:	super(inStream)
	,	CQAPartnerTableMixin(this)
{
}

// ---------------------------------------------------------------------------
//		¥ ~CTextColumn
// ---------------------------------------------------------------------------
//	Destructor

CTextColumn::~CTextColumn()
{
}


#pragma mark -
#if defined(QAP_BUILD)

#include <LScrollerView.h>

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
	LScrollerView *myScroller = dynamic_cast<LScrollerView *>(GetSuperView());
	if (myScroller != NULL)
	{
#if 0
// LScrollerView does not provide public access to its scrollbars (pinkerton)
		if (myScroller->GetVScrollbar() != NULL)
			macVScroll = myScroller->GetVScrollbar()->GetMacControl();
#endif
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
