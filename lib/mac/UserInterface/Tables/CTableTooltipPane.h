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