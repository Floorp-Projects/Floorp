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

#ifndef __H_CMouseDragger
#define __H_CMouseDragger
#pragma once

/*======================================================================================
	AUTHOR:			Ted Morris - 25 SEPT 96
					©1996 Netscape Communications Corp. All rights reserved worldwide.

	DESCRIPTION:	Implements a class used for tracking a mouse action within an LView
					class and making it undoable.
					
					To use this class, do the following:
					
	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LView.h>
#include <LTableView.h>


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -

class LImageRect {

public:

						LImageRect(void) {
							Set(0, 0, 0, 0);
						}
						LImageRect(Int32 inLeft, Int32 inTop, Int32 inRight, Int32 inBottom) {
							Set(inLeft, inTop, inRight, inBottom);
						}
						LImageRect(const LImageRect *inImageRect) {
							Set(inImageRect->Left(), inImageRect->Top(), 
								inImageRect->Right(), inImageRect->Bottom());
						}
						
	Int32				Left(void) const {
							return mTopLeft.h;
						}
	Int32				Top(void) const {
							return mTopLeft.v;
						}
	Int32				Right(void) const {
							return mBotRight.h;
						}
	Int32				Bottom(void) const {
							return mBotRight.v;
						}
	void				Left(Int32 inValue) {
							mTopLeft.h = inValue;
						}
	void				Top(Int32 inValue) {
							mTopLeft.v = inValue;
						}
	void				Right(Int32 inValue) {
							mBotRight.h = inValue;
						}
	void				Bottom(Int32 inValue) {
							mBotRight.v = inValue;
						}
	void				Set(const LImageRect *inImageRect) {
							mTopLeft.h = inImageRect->Left();
							mTopLeft.v = inImageRect->Top();
							mBotRight.h = inImageRect->Right();
							mBotRight.v = inImageRect->Bottom();
						}
	void				Set(Int32 inLeft, Int32 inTop, Int32 inRight, Int32 inBottom) {
							mTopLeft.h = inLeft;
							mTopLeft.v = inTop;
							mBotRight.h = inRight;
							mBotRight.v = inBottom;
						}
	void				Offset(Int32 inDeltaH, Int32 inDeltaV) {
							mTopLeft.h += inDeltaH;
							mTopLeft.v += inDeltaV;
							mBotRight.h += inDeltaH;
							mBotRight.v += inDeltaV;
						}
	SPoint32			*TopLeft(void) {
							return &mTopLeft;
						}
	SPoint32			*BotRight(void) {
							return &mBotRight;
						}
	Boolean				IsEmpty(void) const {
							return ((mTopLeft.h >= mBotRight.h) || (mTopLeft.v >= mBotRight.v));
						}
	Boolean				PointIn(Int32 inHorz, Int32 inVert) const {
							return (((inHorz >= mTopLeft.h) && (inHorz < mBotRight.h)) &&
									((inVert >= mTopLeft.v) && (inVert < mBotRight.v)));
						}
	Boolean				PinPoint(Int32 *ioHorz, Int32 *ioVert) const {
							Boolean rtnVal = false;
							if ( *ioHorz < mTopLeft.h ) {
								*ioHorz = mTopLeft.h; rtnVal = true;
							} else if ( *ioHorz >= mBotRight.h ) {
								*ioHorz = mBotRight.h; rtnVal = true;
							}
							if ( *ioVert < mTopLeft.v ) {
								*ioVert = mTopLeft.v; rtnVal = true;
							} else if ( *ioVert >= mBotRight.v ) {
								*ioVert = mBotRight.v; rtnVal = true;
							}
							return rtnVal;
						}
						
	static Boolean		EqualPoints32(const SPoint32 *inPointA, const SPoint32 *inPointB) {
							return ((inPointA->h == inPointB->h) && (inPointA->v == inPointB->v));
						}

protected:

	// Instance variables ==========================================================
	
	SPoint32			mTopLeft;
	SPoint32			mBotRight;
};

#pragma mark -

class CMouseDragger {
				  
public:

						CMouseDragger(void) {
						}
	virtual				~CMouseDragger(void) {
						}

	// Start public interface ----------------------------------------------------------

	Boolean				DoTrackMouse(LView *inView, const SPoint32 *inStartMouseImagePt,
								     const LImageRect *inMouseImagePinRect = nil,
								     const Int16 inMinHMoveStart = 1, const Int16 inMinVMoveStart = 1);

	// End public interface ------------------------------------------------------------

	static Boolean 		CanScrollView(LView *inView, Point inGlobalMousePt, 
									  const Rect *inGlobalViewFrame = nil);

	static void 		GlobalToImagePt(LView *inView, Point inGlobalPt, 
										SPoint32 *outImagePt);
	static void  		ImageToPortPt(LView *inView, const SPoint32 *inImagePt, 
									  Point *outPortPt);
	static void  		PortToImagePt(LView *inView, Point inPortPoint, 
									  SPoint32 *outImagePt);
	static void  		PortToImageRect(LView *inView, const Rect *inPortRect, 
									  	LImageRect *outImageRect);
	static void  		ImageToLocalRect(LView *inView, LImageRect *inImageRect, 
									  	 Rect *outLocalRect);

protected:

	// Override methods

	virtual void		BeginTracking(const SPoint32 *inStartMouseImagePt);

	virtual Boolean		KeepTracking(const SPoint32 *inStartMouseImagePt,
									 const SPoint32 *inPrevMouseImagePt,
									 const SPoint32 *inCurrMouseImagePt);

	virtual void		EndTracking(const SPoint32 *inStartMouseImagePt,
									const SPoint32 *inEndMouseImagePt);

	virtual void		AboutToScrollView(const SPoint32 *inStartMouseImagePt,
									 	  const SPoint32 *inCurrMouseImagePt);
	virtual void		DoneScrollingView(const SPoint32 *inStartMouseImagePt,
									 	  const SPoint32 *inCurrMouseImagePt);

						enum EDrawRegionState { 
							eDoErase = 0			// Normal erase
						,	eDoEraseScroll = 1		// Erase just before a scoll
						,	eDoDraw = 2 			// Draw
						};
						
	virtual void		DrawDragRegion(const SPoint32 *inStartMouseImagePt,
									   const SPoint32 *inCurrMouseImagePt, 
									   EDrawRegionState inDrawState);

	// Instance variables ==========================================================
	
	LView				*mView;
	SPoint32			mStartImagePt;
	SPoint32			mEndImagePt;
};


// Class specific for dragging rows in an LTableView. Be careful, it hasn't been
// tested on a wide range of LTableView descendents! Also, assumes that the
// table only has a single column! Note: This class also assumes the presence
// of icm8 resources for the arrow icons.

/*======================================================================================
	The ClickCell() method for your table will look something like this:
	
	void CMyTableView::ClickCell(const STableCell &inCell, const SMouseDownEvent &inMouseDown) {

		if ( LPane::GetClickCount() == 2 ) {
		
			// Do double click stuff
			
		} else if ( mRows > 1 ) {

			CTableRowDragger dragger(inCell.row);
			
			SPoint32 startMouseImagePt;
			LocalToImagePoint(inMouseDown.whereLocal, startMouseImagePt);
			
			// Restrict dragging to a thin vertical column the height of the image
			
			LImageRect dragPinRect;
			{
				Int32 left, top, right, bottom;
				GetImageCellBounds(inCell, left, top, right, bottom);
				dragPinRect.Set(startMouseImagePt.h, startMouseImagePt.v - top, startMouseImagePt.h, 
								mImageSize.height - (bottom - startMouseImagePt.v) + 1);
			}
			
			TrySetCursor(cHandClosedCursorID);
			dragger.DoTrackMouse(this, &startMouseImagePt, &dragPinRect, 2, 2);
				
			TableIndexT newRow = dragger.GetDraggedRow();
				
			if ( newRow ) {
				CMyMoveRowAction *action = new CMyMoveRowAction(this, inCell.row, newRow);
				FailNIL_(action);
				PostAction(action);
			}
		}
	}
======================================================================================*/

// CTableRowDragger

class CTableRowDragger : public CMouseDragger {

public:

						CTableRowDragger(TableIndexT inClickedRow) :
								 		 CMouseDragger(),
								 		 mClickedRow(inClickedRow),
								 		 mOverRow(inClickedRow),
								 		 mNumCols(1) {
								 		 
						}
						
	TableIndexT			GetDraggedRow(void) {
							return ((mClickedRow == mOverRow) ? 0 : mOverRow);
						}
						
protected:

						enum {	// Constants
							icm8_LeftInsertArrow = 29203
						,	icm8_RightInsertArrow = 29204
						,	cArrowVCenterTopOffset = 3
						,	cArrowPolyWidth = 4
						,	cArrowHMargin = 1
						,	cSmallIconWidth = 16
						,	cSmallIconHeight = 12

						};

	virtual void		BeginTracking(const SPoint32 *inStartMouseImagePt);
	virtual void		EndTracking(const SPoint32 *inStartMouseImagePt,
							    	const SPoint32 *inEndMouseImagePt);
						
	virtual void		DrawDragRegion(const SPoint32 *inStartMouseImagePt,
									   const SPoint32 *inCurrMouseImagePt,
									   EDrawRegionState inDrawState);
	void				UpdateInsertionPoint(const LImageRect *inImageCellDrawRect,
											 LTableView *inTableView,
											 EDrawRegionState inDrawState);
	void				CalcArrowRects(Int16 inPortLineV, Rect *outLeftRect, Rect *outRightRect,
									   Boolean inErase);

	// Instance variables ==========================================================
	
	TableIndexT			mClickedRow;		// The row that was clicked
	TableIndexT			mOverRow;			// The current row that the mouse is over
	TableIndexT			mNumCols;			// The current row that the mouse is over

	Boolean				mDidDraw;
	
	Int16				mPortInsertionLineV;	// Port vertical coordinate that the last
												// insertion line and arrows was draw at,
												// min_Int16 if no line was last drawn 
	Int16				mPortInsertionLineLeft;	// Port left coordinate for drawing the insertion line
	Int16				mPortInsertionLineRight;// Port right coordinate for drawing the insertion line
	
	Rect				mPortLeftArrowBox;
	Rect				mPortRightArrowBox;

	Int16				mPortFrameMinV;
	Int16				mPortFrameMaxV;
};


#endif // __H_CMouseDragger
