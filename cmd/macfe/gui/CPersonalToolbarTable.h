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

//
// Interface for the table that is the main component of the personal toolbar.
//

#pragma once

#include <LSmallIconTable.h>
#include "ntypes.h"
#include "CDynamicTooltips.h"

class CPersonalToolbarManager;
class CUserButtonInfo;


class CPersonalToolbarTable : public LSmallIconTable, public LListener, public LDragAndDrop,
								public CDynamicTooltipMixin
{
	public:
	
		enum { class_ID = 'PerT', kTextTraitsID = 130 };
		enum { kBOOKMARK_ICON = 15313, kFOLDER_ICON = -3999 };

		CPersonalToolbarTable ( LStream* inStream ) ;
		virtual ~CPersonalToolbarTable ( ) ;

		// for handling mouse tracking
		virtual void MouseLeave ( ) ;
		virtual void MouseWithin ( Point inPortPt, const EventRecord& ) ;
		virtual void Click ( SMouseDownEvent &inMouseDown ) ;

		virtual void FillInToolbar ( ) ;
		
		virtual void ListenToMessage ( MessageT inMessage, void* ioPtr ) ;
		
		// send data when a drop occurs
		void DoDragSendData( FlavorType inFlavor, ItemReference inItemRef, DragReference inDragRef) ;
		void ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttrs,
								ItemReference inItemRef, Rect &inItemBounds ) ;

		// calculate tooltip to display url text for the item mouse is over
		virtual void FindTooltipForMouseLocation ( const EventRecord& inMacEvent,
													StringPtr outTip );
													
		virtual void ResizeFrameBy ( Int16 inWidth, Int16 inHeight, Boolean inRefresh );

	protected:
	
		virtual void ClickCell(const STableCell	&inCell, const SMouseDownEvent &inMouseDown);
		virtual Boolean ClickSelect( const STableCell &inCell, const SMouseDownEvent &inMouseDown);
		virtual void FinishCreateSelf ( ) ;
		virtual void DrawCell ( const STableCell &inCell, const Rect &inLocalRect ) ;
		virtual void RedrawCellWithHilite ( const STableCell inCell, bool inHiliteOn ) ;

		// override to do nothing
		virtual void HiliteSelection ( Boolean /*inActively*/, Boolean /*inHilite*/) { } ;
		
		// fetch cell data given a column #
		CUserButtonInfo* GetButtonInfo ( Uint32 inColumn ) ;
		
		// drag and drop overrides for drop feedback, etc
		void HiliteDropArea ( DragReference inDragRef );
		Boolean ItemIsAcceptable ( DragReference inDragRef, ItemReference inItemRef ) ;
		void InsideDropArea ( DragReference inDragRef ) ;
		void DrawDividingLine ( TableIndexT inCol ) ;
		void LeaveDropArea( DragReference inDragRef ) ;
		bool FindBestFlavor ( DragReference inDragRef, ItemReference inItemRef, 
								FlavorType & oFlavor ) ;

		virtual void ComputeItemDropAreas ( const Rect & inLocalCellRect, Rect & oLeftSide, 
												Rect & oRightSide ) ;
		virtual void ComputeFolderDropAreas ( const Rect & inLocalCellRect, Rect & oLeftSide, 
												Rect & oRightSide ) ;

		virtual Rect ComputeTextRect ( const SIconTableRec & inText, const Rect & inLocalRect ) ;
		virtual void RedrawCellWithTextClipping ( const STableCell & inCell ) ;

			// these are valid only during a D&D and are used for hiliting and determining
			// the drop location
		STableCell	mDraggedCell;
		TableIndexT mDropCol;			// position where a drop will go
		bool		mDropOn;			// are they on a folder?
		Rect		mTextHiliteRect;	// cached rect drawn behind selected folder title
		TableIndexT mHiliteCol;			// which column is mouse hovering over?

		static DragSendDataUPP sSendDataUPP;

}; // CPersonalToolbarTable
