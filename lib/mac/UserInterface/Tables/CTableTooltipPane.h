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

#include "CTooltipAttachment.h"

#include "CStandardFlexTable.h"

//========================================================================================
class CTableToolTipAttachment
// A minimal override of CToolTipAttachment, which currently changes only the
// setting of the mMustBeActive flag to false.
//========================================================================================
:	public CToolTipAttachment
{
	private:
		typedef CToolTipAttachment Inherited;

	public:
		enum { class_ID = 'TTTa' };
		
							CTableToolTipAttachment(LStream* inStream);

}; // class CTableToolTipAttachment

//========================================================================================
class CTableToolTipPane
// This gives a cell-by-cell tooltip that appears aligned with a cell in CStandardFlexTable.
// Use the resid of a table tooltip pane (11509) to get this behavior.  It calls the method
// of CStandardFlexTable::CalcTipText() to get the text of each cell.  If the table
// returns an empty string, then no tooltip appears for the cell.
//========================================================================================
:	public CToolTipPane
{
	private:
		typedef CToolTipPane Inherited;

	public:
		enum { class_ID = 'TTTp' };
		
							CTableToolTipPane(LStream* inStream);

		virtual void		CalcFrameWithRespectTo(
									LWindow*				inOwningWindow,
									const EventRecord&		inMacEvent,
									Rect& 					outTipFrame);
	
		virtual	void 		CalcTipText(
									LWindow*				inOwningWindow,
									const EventRecord&		inMacEvent,
									StringPtr				outTipText);
		virtual CStandardFlexTable*	GetTableAndCell(
									Point					inMousePort,
									STableCell&				outCell);
		virtual Boolean		WantsToCancel(
									Point					inMousePort);
		virtual void		DrawSelf();
	
	protected:
		CStandardFlexTable::TextDrawingStuff
							mTextDrawingStuff;
		Boolean				mTruncationOnly; // tooltip has no extra information,
								// and should only be shown if it doesn't fit
								// in the underlying cell.
}; // class CTableTooltipPane