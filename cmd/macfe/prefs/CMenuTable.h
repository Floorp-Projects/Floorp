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

#pragma once

#include <LTextHierTable.h>
//#include <QAP_Assist.h>

//========================================================================================
class CMenuTable
// This descendant of LHierTextTable initializes itself from menu resources.  The hierarchical
// menus give rise to a hierarchical table.  This trick avoids the difficult task of defining a
// new dynamic hierarchical resource template for such tables.  Currently this is only used
// for the left pane of the preferences dialog.
//========================================================================================
: 	public LTextHierTable
,	public LBroadcaster
//,	public CQAPartnerTableMixin
{
	public:
		enum
		{
			class_ID = 'Itxh',
			msg_SelectionChanged = 'selc'
		};
							CMenuTable(LStream *inStream);
		virtual				~CMenuTable();
		
		virtual void		FinishCreate();
		virtual void		SelectionChanged();
		virtual MessageT	GetSelectedMessage();
		virtual void		GetHiliteRgn(RgnHandle ioHiliteRgn);
		virtual void		HiliteSelection(Boolean	inActively,	Boolean	inHilite);
		virtual void		HiliteCellActively(const STableCell	&inCell, Boolean inHilite);
		virtual void		HiliteCellInactively(const STableCell	&inCell, Boolean inHilite);
		virtual void		ClickSelf(const SMouseDownEvent &inMouseDown);
				TableIndexT	FindMessage( MessageT message );
//#if defined(QAP_BUILD)		
//		virtual void		QapGetListInfo (PQAPLISTINFO pInfo);
//		virtual Ptr			QapAddCellToBuf(Ptr pBuf, Ptr pLimit, const STableCell& sTblCell);
//#endif
	protected:
		ResIDT				mMenuID;

		virtual void		DrawCell(
									const STableCell	&inCell,
									const Rect			&inLocalRect);									
		void				AddTableLabels(
									TableIndexT	&currentRow,
									ResIDT		mMenuID,
									Boolean		firstLevel = true);
}; // class CMenuTable
