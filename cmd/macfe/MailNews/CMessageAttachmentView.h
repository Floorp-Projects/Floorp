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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMessageAttachmentView.h

#pragma once
#include "LTableView.h"
#include "msgcom.h"
#include <LDragAndDrop.h>


class CMessageView;
class CBrokeredView;
class CAttachmentIcon;


class CMessageAttachmentView: public LTableView, public LCommander, public LDragAndDrop
{
	
private:
		typedef LTableView Inherited;
public:
		enum { class_ID = 'MATv' };
					CMessageAttachmentView(LStream* inStream);
	
	virtual			~CMessageAttachmentView();
	virtual	void 	FinishCreateSelf();
	virtual void	SetUpTableHelpers();
	
	void 			SetMessageAttachmentList( MSG_Pane* pane, int32 numberAttachments );
	void			ClearMessageAttachmentView();
	virtual void	OpenSelection( int32 action );
	virtual void 	HandleURL( URL_Struct* inURL, int32 action );
	
// LPane
	virtual void	ResizeFrameBy(
								Int16				inWidthDelta,
								Int16				inHeightDelta,
								Boolean				inRefresh);
			void 			ToggleVisibility();
	virtual	void 			Hide();
	virtual	void			Show();
	void 					Remove();

	
//	LCommander	
	virtual void 	FindCommandStatus(
						CommandT			inCommand,
						Boolean				&outEnabled,
						Boolean				&outUsesMark,
						Char16				&outMark,
						Str255				outName);
	virtual Boolean ObeyCommand( CommandT inCommand, void *ioParam );

// LTableView
	virtual void	HiliteSelection(
							Boolean					inActively,
							Boolean					inHilite);
	virtual void	HiliteCell(
							const STableCell		&inCell,
							Boolean					inHilite);
	virtual Boolean	ClickSelect(
							const STableCell		&inCell,
							const SMouseDownEvent	&inMouseDown); 			
	
//	LDragandDrop
//	------------------------------------------------------------
	virtual void		DragSelection(const STableCell& /*inCell*/, const SMouseDownEvent &inMouseDown);
	virtual void		AddSelectionToDrag(DragReference inDragRef, RgnHandle inDragRgn);
	virtual void 		DoDragSendData(FlavorType inFlavor,
								ItemReference inItemRef,
								DragReference inDragRef);
protected:
	virtual void 	DrawSelf();		
	virtual void	DrawCell( const STableCell &inCell, const Rect &inLocalRect );
			void	CalculateRowsColumns();
	Int32		 	CalculateAttachmentIndex( STableCell inCell);
	
	CBrokeredView*		mBrokeredView;
	MSG_Pane			*mMSGPane;		
	CMessageView		*mMessageView;
	Int16				mExpandedHeight;
	Int16				mClickCountToOpen;
		
	MSG_AttachmentData	*mAttachmentList;
	Int32				mNumberAttachments;
	CAttachmentIcon		**mAttachmentIconList;
	DragSendDataUPP 	mSendDataUPP;
};
