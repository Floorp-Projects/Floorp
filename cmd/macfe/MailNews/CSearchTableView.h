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



// CSearchTableView.h

#pragma once

#include "SearchHelpers.h"
#include "CSearchManager.h"
#include "CDateView.h"
#include "NetscapeDragFlavors.h"

///////////////////////////////////////////////////////////////////////////////////
// CSearchTableView
class CSearchTableView : public CMailFlexTable
{
private:
	typedef CMailFlexTable	Inherited;
	
public:
						enum { class_ID = 'SrTb' };
						enum { 	
								menu_Navigator = 305,
								menu_View = 502,
								menu_Go = 503 
							  };
						typedef enum {
								kInMessageWindow,
								kInThreadWindow,
								kAddToThreadWindowSelection
							 } SearchWindowOpener;
						
						CSearchTableView(LStream *inStream);
	virtual				~CSearchTableView();
	
	void				SetSearchManager(CSearchManager *inSearchManager);


	void				ForceCurrentSort();
	void				GetSortParams(MSG_SearchAttribute *outSortKey, Boolean *outSortBackwards);
	
	virtual void		ChangeSort(const LTableHeader::SortChange *inSortChange);
	virtual void		OpenRow(TableIndexT inRow);


	virtual void		ShowMessage(MSG_ViewIndex inMsgIndex, SearchWindowOpener inWindow);
	virtual void		FindOrCreateThreadWindowForMessage(MSG_ViewIndex inMsgIndex);
	virtual	void		FindCommandStatus( CommandT	inCommand, Boolean &outEnabled,
							Boolean	&outUsesMark, Char16 &outMark, Str255 outName);

	MessageKey			GetRowMessageKey(TableIndexT inRow);
	MSG_ResultElement* 	GetCurrentResultElement(TableIndexT inRow);
	virtual void	 	SetWinCSID(Int16 wincsid);

	virtual void 		ListenToMessage( MessageT inCommand, void *ioParam);
	virtual	Boolean 	ObeyCommand( CommandT inCommand, void* ioParam );
	void 				StartSearch(MWContext *inContext, MSG_Master *inMaster, 
								MSG_SearchAttribute inSortKey, Boolean inSortBackwards);
	static Boolean		GetMessageResultInfo(
							MSG_Pane* inPane,
							MSG_ViewIndex inMsgIndex, MSG_ResultElement*& outElement,
							MSG_FolderInfo*& outFolder, MessageKey& outKey);
protected:
	Boolean			GetDisplayText(MSG_ViewIndex inIndex, MSG_SearchAttribute inAttribute,
								       CStr255& outString);
	void 			UpdateMsgResult(MSG_ViewIndex inIndex);
	void			NewSearchSession(MWContext *inContext, MSG_Master *inMaster, MSG_SearchAttribute inSortKey,
									  	 Boolean inSortBackwards);
	
	
	
	virtual Boolean		IsValidRow(TableIndexT inRow) const;
	virtual void		SetUpTableHelpers();
	virtual void		DeleteSelection();
	virtual void		DrawCellContents(const STableCell &inCell, const Rect &inLocalRect);
	
	virtual void		MsgPaneChanged();
	virtual Int32		GetBENumRows();
	Boolean				GetMessageResultInfo( MSG_ViewIndex inMsgIndex, MSG_ResultElement*& outElement,
							MSG_FolderInfo*& outFolder, MessageKey& outKey);

// drag support
	virtual Boolean		CellInitiatesDrag(const STableCell&) const { return true; }
				// uncomment the line above to turn on drag from the message search
				// results window.  Problem is, URL not obtainable for message search
				// unless the enclosing folder is open in a thread window.  SO it's
				// turned off for now.
//	virtual void		AddSelectionToDrag( DragReference inDragRef, RgnHandle inDragRgn );
	virtual void		AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef);
	virtual void		SelectionChanged();
	virtual void 		ChangeFinished(MSG_Pane */*inPane*/, MSG_NOTIFY_CODE inChangeCode,
							  TableIndexT inStartRow, SInt32 inRowCount);
	// Instance variables ==========================================================
	enum { eInvalidResultIndex = -1 };

	CSearchManager		*mSearchManager; // Belongs to the window, not us.
	MSG_ResultElement	*mResultElement;
	MSG_ViewIndex		mResultIndex;
}; 

///////////////////////////////////////////////////////////////////////////////////
// inlines

inline
CSearchTableView::CSearchTableView(LStream *inStream)
:	CMailFlexTable(inStream),	mSearchManager(nil), mResultElement( nil ), mResultIndex (eInvalidResultIndex )
{
	mDragFlavor = kMailNewsSelectionDragFlavor;
	SetRefreshAllWhenResized(false);
}


inline void		
CSearchTableView::MsgPaneChanged()
{
	mResultElement = nil;
}
	
inline Int32		
CSearchTableView::GetBENumRows()
{
	return (GetMessagePane() == nil) ? 0 : MSG_GetNumLines(GetMessagePane());
}

