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


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CMouseDragger.h"


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -

/*======================================================================================
	Track the mouse within the view specified by inView. The beginning mouse tracking
	point is given by inStartMouseImagePt, which is in the image coordinate system
	of inView. The mouse movements are pinned to the rect specified by inMouseImagePinRect,
	which again is in the image coordinate system of inView. If inMouseImagePinRect is
	nil, the mouse is pinned to the current image size of the view. This method handles
	all interaction with the user until the mouse button is let up. If inStartMouseImagePt
	is not within inMouseImagePinRect, it is forced to be within the rect by this method.
	The inMinHMoveStart and inMinVMoveStart values specify the minimum amount of movement
	required in either the horizontal or vertical directions to begin mouse tracking.
	
	If the mouse moved from the original point specified by inStartMouseImagePt (as adjusted
	to be within inMouseImagePinRect, if necessary), the method returns true; otherwise,
	no mouse movement occurred and the method returns false.
	
	The methods BeginTracking(), KeepTracking(), and EndTracking() are meant to be 
	overriden by subclasses to implement the behavior of the mouse movements. See these
	method for descriptions.
======================================================================================*/

Boolean CMouseDragger::DoTrackMouse(LView *inView, const SPoint32 *inStartMouseImagePt,
								    const LImageRect *inMouseImagePinRect,
								    const Int16 inMinHMoveStart, const Int16 inMinVMoveStart) {

	mView = inView;
	
	// Setup tracking variables. In addtion to pinning the current mouse point to the specified
	// image rect, we also pin it to the frame of the view.
	
	LImageRect imagePinRect;
	LImageRect viewImageBounds;
	LImageRect viewFrameImageBounds;
	
	{
		SDimension32 imageSize;
		inView->GetImageSize(imageSize);
		viewImageBounds.Set(0, 0, imageSize.width, imageSize.height);	// Add one since pin-rect subtracts 1
	}

	if ( inMouseImagePinRect ) {
		imagePinRect.Set(inMouseImagePinRect);
		if ( imagePinRect.Left() < viewImageBounds.Left() ) imagePinRect.Left(viewImageBounds.Left());
		if ( imagePinRect.Top() < viewImageBounds.Top() ) imagePinRect.Top(viewImageBounds.Top());
		if ( imagePinRect.Right() > viewImageBounds.Right() ) imagePinRect.Right(viewImageBounds.Right());
		if ( imagePinRect.Bottom() > viewImageBounds.Bottom() ) imagePinRect.Bottom(viewImageBounds.Bottom());
	} else {
		imagePinRect.Set(&viewImageBounds);
	}
	
	Rect viewGlobalFrame, viewPortFrame;
	mView->CalcPortFrameRect(viewPortFrame);
	viewGlobalFrame = viewPortFrame;
	mView->PortToGlobalPoint(topLeft(viewGlobalFrame));
	mView->PortToGlobalPoint(botRight(viewGlobalFrame));

	// Pin start point
	
	mStartImagePt = *inStartMouseImagePt;
	imagePinRect.PinPoint(&mStartImagePt.h, &mStartImagePt.v);
	PortToImageRect(mView, &viewPortFrame, &viewFrameImageBounds);
	viewFrameImageBounds.PinPoint(&mStartImagePt.h, &mStartImagePt.v);
	
	mEndImagePt = mStartImagePt;
	SPoint32 lastPoint = mStartImagePt;
	
	// Let subclasses do something when we start
	
	BeginTracking(&mStartImagePt);
	
	// Do the tracking
	
	Boolean trackingStarted = false, keepTracking = true;
	EventRecord macEvent;
	
	Int32 nextScrollTime = 0;
	
	Try_ {
		while ( keepTracking && ::WaitMouseUp() ) {
		
			// Give background processing time
			::WaitNextEvent(0, &macEvent, 0, nil);
		
			SPoint32 newImagePoint;
			GlobalToImagePt(mView, macEvent.where, &newImagePoint);

			Boolean didntScrollView = true;
		
			// Determine if we need to scroll the image, if so handle
			// user tracking here
			
			if ( CanScrollView(mView, macEvent.where, &viewGlobalFrame) ) {
			
				// Make sure that we don't scroll too quickly
				
				if ( nextScrollTime <= LMGetTicks() ) {
				
					// Need to scroll the view
					Point localPt = macEvent.where;
					mView->GlobalToPortPoint(localPt);
					mView->PortToLocalPoint(localPt);
					AboutToScrollView(&mStartImagePt, &mEndImagePt);
					
					mView->AutoScrollImage(localPt);	// Auto scroll the image
					didntScrollView = false;
					nextScrollTime = LMGetTicks() + 2;
					
					// Pin point

					GlobalToImagePt(mView, macEvent.where, &newImagePoint);
					imagePinRect.PinPoint(&newImagePoint.h, &newImagePoint.v);
					PortToImageRect(mView, &viewPortFrame, &viewFrameImageBounds);
					viewFrameImageBounds.PinPoint(&newImagePoint.h, &newImagePoint.v);
					
					if ( !LImageRect::EqualPoints32(&newImagePoint, &mEndImagePt) ) {
						lastPoint = mEndImagePt;
						mEndImagePt = newImagePoint;
					}
					DoneScrollingView(&mStartImagePt, &mEndImagePt);
				}
			}
			
			// If we didn't scroll the image, handle user tracking here
			
			if ( didntScrollView ) {
				imagePinRect.PinPoint(&newImagePoint.h, &newImagePoint.v);
				viewFrameImageBounds.PinPoint(&newImagePoint.h, &newImagePoint.v);
				if ( !LImageRect::EqualPoints32(&newImagePoint, &mEndImagePt) ) {
					if ( !trackingStarted ) {
						Int32 deltaH = newImagePoint.h - mStartImagePt.h;
						Int32 deltaV = newImagePoint.v - mStartImagePt.v;
						if ( deltaH < 0 ) deltaH = -deltaH;
						if ( deltaV < 0 ) deltaV = -deltaV;
						trackingStarted = ((deltaH >= inMinHMoveStart) || (deltaV >= inMinVMoveStart));
					}
					if ( trackingStarted ) {
						lastPoint = mEndImagePt;
						mEndImagePt = newImagePoint;
						
						keepTracking = KeepTracking(&mStartImagePt, &lastPoint, &mEndImagePt);
					}
				}
			}
		}

		EndTracking(&mStartImagePt, &mEndImagePt);
	}
	Catch_(inErr) {
		// Always call EndTracking()
		EndTracking(&mStartImagePt, &mEndImagePt);

		Throw_(inErr);
	}
	EndCatch_

	return !LImageRect::EqualPoints32(&mStartImagePt, &mEndImagePt);
}


/*======================================================================================
	Return true if the global point inGlobalMousePt will cause a scroll of the view
	inView. inGlobalViewFrame is the frame of the view in global coordinates or nil
	if it has not been calculated yet.
======================================================================================*/

Boolean CMouseDragger::CanScrollView(LView *inView, Point inGlobalMousePt, const Rect *inGlobalViewFrame) {

	Rect viewGlobalFrame;
	
	if ( inGlobalViewFrame == nil ) {
		// We calculate it here for caller!
		inView->CalcPortFrameRect(viewGlobalFrame);
		inView->PortToGlobalPoint(topLeft(viewGlobalFrame));
		inView->PortToGlobalPoint(botRight(viewGlobalFrame));
		inGlobalViewFrame = &viewGlobalFrame;
	}

	Boolean	canScroll = false;
	
	SPoint32 scrollUnits;
	inView->GetScrollUnit(scrollUnits);

	Int32 horizScroll = 0;
	if ( inGlobalMousePt.h < inGlobalViewFrame->left ) {
		horizScroll = -scrollUnits.h;
	} else if ( inGlobalMousePt.h > inGlobalViewFrame->right ) {
		horizScroll = scrollUnits.h;
	}
	
	Int32 vertScroll = 0;
	if ( inGlobalMousePt.v < inGlobalViewFrame->top ) {
		vertScroll = -scrollUnits.v;
	} else if (inGlobalMousePt.v > inGlobalViewFrame->bottom) {
		vertScroll = scrollUnits.v;
	}	
	
	if ( (horizScroll != 0) || (vertScroll != 0) ) {
	
		SPoint32 imageLocation;
		inView->GetImageLocation(imageLocation);
		SPoint32 frameLocation;
		inView->GetFrameLocation(frameLocation);
		SDimension32 imageSize;
		inView->GetImageSize(imageSize);
		SDimension16 frameSize;
		inView->GetFrameSize(frameSize);

		if ( vertScroll != 0 ) {
			Int32 tryDown = imageLocation.v - vertScroll;
			if ( tryDown > frameLocation.v ) {
				if ( imageLocation.v <= frameLocation.v ) {
					vertScroll = imageLocation.v - frameLocation.v;
				} else if ( vertScroll < 0 ) {
					vertScroll = 0;
				}
			} else if ( (tryDown + imageSize.height) < (frameLocation.v + frameSize.height)) {
				if ( (imageLocation.v + imageSize.height) >= (frameLocation.v + frameSize.height) ) {
					vertScroll = imageLocation.v - frameLocation.v + imageSize.height - frameSize.height;
				} else if ( vertScroll > 0 ) {
					vertScroll = 0;
				}
			}
		}
		
		if ( horizScroll != 0 ) {
			Int32 tryLeft = imageLocation.h - horizScroll;
			if ( tryLeft > frameLocation.h ) {
				if ( imageLocation.h <= frameLocation.h ) {
					horizScroll = imageLocation.h - frameLocation.h;
				} else if ( horizScroll < 0 ) {
					horizScroll = 0;
				}
			} else if ( (tryLeft + imageSize.width) < (frameLocation.h + frameSize.width) ) {
				if ( (imageLocation.h + imageSize.width) >= (frameLocation.h + frameSize.width) ) {
					horizScroll = imageLocation.h - frameLocation.h + imageSize.width - frameSize.width;
				} else if (horizScroll > 0) {
					horizScroll = 0;
				}
			}
		}
			
		canScroll = (horizScroll != 0) || (vertScroll != 0);
	}
	
	return canScroll;
}


/*======================================================================================
	Convert the specified global point to the image coordinate system of inView.
======================================================================================*/

void CMouseDragger::GlobalToImagePt(LView *inView, Point inGlobalPt, SPoint32 *outImagePt) {

	inView->GlobalToPortPoint(inGlobalPt);
	inView->PortToLocalPoint(inGlobalPt);
	inView->LocalToImagePoint(inGlobalPt, *outImagePt);
}


/*======================================================================================
	Convert the specified image point to the port coordinate system of inView.
======================================================================================*/

void CMouseDragger::ImageToPortPt(LView *inView, const SPoint32 *inImagePt, Point *outPortPt) {

	inView->ImageToLocalPoint(*inImagePt, *outPortPt);
	inView->LocalToPortPoint(*outPortPt);
}


/*======================================================================================
	Convert the specified port point to the image coordinate system of inView.
======================================================================================*/

void CMouseDragger::PortToImagePt(LView *inView, Point inPortPoint, SPoint32 *outImagePt) {

	inView->PortToLocalPoint(inPortPoint);
	inView->LocalToImagePoint(inPortPoint, *outImagePt);
}


/*======================================================================================
	Convert the specified port rect to the image coordinate system of inView.
======================================================================================*/

void CMouseDragger::PortToImageRect(LView *inView, const Rect *inPortRect, LImageRect *outImageRect) {

	PortToImagePt(inView, topLeft(*inPortRect), outImageRect->TopLeft());
	PortToImagePt(inView, botRight(*inPortRect), outImageRect->BotRight());
}


/*======================================================================================
	Convert the specified port rect to the image coordinate system of inView.
======================================================================================*/

void CMouseDragger::ImageToLocalRect(LView *inView, LImageRect *inImageRect, 
									 Rect *outLocalRect) {

	inView->ImageToLocalPoint(*inImageRect->TopLeft(), topLeft(*outLocalRect));
	inView->ImageToLocalPoint(*inImageRect->BotRight(), botRight(*outLocalRect));
}


/*======================================================================================
	This method is ALWAYS called from within the DoTrackMouse() method just before
	the mouse tracking loop begins. If you want to setup instance varibles before
	tracking begins, or draw an initial outline of a tracking image, this is the
	place to do it. 
	
	The inStartMouseImagePt parameter has been pinned to the bounds passed to 
	DoTrackMouse() in the inMouseImagePinRect parameter and is also pinned to the
	frame of the view.
======================================================================================*/

void CMouseDragger::BeginTracking(const SPoint32 *inStartMouseImagePt) {

	// Draw
	DrawDragRegion(inStartMouseImagePt, inStartMouseImagePt, eDoDraw);
}


/*======================================================================================
	This method is called anytime the mouse moves to a new location within the
	bounds passed to DoTrackMouse() in the inMouseImagePinRect parameter. The points
	inPrevMouseImagePt and inCurrMouseImagePt will always be different. Note that
	this method may NEVER be called if the user never moves the mouse from its 
	initial position. This method should perform visual dragging of a region or
	other user tracking interaction.
	
	If the method returns false, mouse tracking is stopped.
	
	All input points have been pinned to the bounds passed to DoTrackMouse() in the 
	inMouseImagePinRect parameter and are also pinned to the frame of the view.
======================================================================================*/

Boolean CMouseDragger::KeepTracking(const SPoint32 *inStartMouseImagePt,
								   const SPoint32 *inPrevMouseImagePt,
								   const SPoint32 *inCurrMouseImagePt) {

	// Erase
	DrawDragRegion(inStartMouseImagePt, inPrevMouseImagePt, eDoErase);

	// Draw
	DrawDragRegion(inStartMouseImagePt, inCurrMouseImagePt, eDoDraw);

	return true;	// Continue tracking
}


/*======================================================================================
	This method is ALWAYS called at the end of user mouse tracking. If the points
	inStartMouseImagePt and inEndMouseImagePt not are equal, the user actually move 
	the mouse somewhere different from where it started. This method should normally
	erase any visual representation of a dragging region.

	All input points have been pinned to the bounds passed to DoTrackMouse() in the 
	inMouseImagePinRect parameter and are also pinned to the frame of the view.
======================================================================================*/

void CMouseDragger::EndTracking(const SPoint32 *inStartMouseImagePt,
							    const SPoint32 *inEndMouseImagePt) {

	// Erase
	DrawDragRegion(inStartMouseImagePt, inEndMouseImagePt, eDoErase);
}


/*======================================================================================
	The mouse has reached a point that will cause the view to be scrolled. This method is
	called just BEFORE the view is scolled to allow any dragging region to be erased
	from the view before scrolling it. After the view is scrolled, the 
	DoneScrollingView() method will be called. The input points represent the
	last points passed to KeepTracking() if it has been called.

	All input points have been pinned to the bounds passed to DoTrackMouse() in the 
	inMouseImagePinRect parameter and are also pinned to the frame of the view.
======================================================================================*/

void CMouseDragger::AboutToScrollView(const SPoint32 *inStartMouseImagePt,
									  const SPoint32 *inCurrMouseImagePt) {

	// Erase
	DrawDragRegion(inStartMouseImagePt, inCurrMouseImagePt, eDoEraseScroll);
}


/*======================================================================================
	The AboutToScrollView() was called and the view was just scolled. This method should
	redraw any dragging region to the screen. The input points represent the NEW images
	points of the mouse after scrolling.

	All input points have been pinned to the bounds passed to DoTrackMouse() in the 
	inMouseImagePinRect parameter and are also pinned to the frame of the view.
======================================================================================*/

void CMouseDragger::DoneScrollingView(const SPoint32 *inStartMouseImagePt,
									  const SPoint32 *inCurrMouseImagePt) {

	// Draw
	DrawDragRegion(inStartMouseImagePt, inCurrMouseImagePt, eDoDraw);
}


/*======================================================================================
	Draw the draw region taking into the difference in mouse positions specified by
	inStartMouseImagePt and inCurrMouseImagePt. inDrawState specifies whether to draw
	the drag region or erase it.
======================================================================================*/

void CMouseDragger::DrawDragRegion(const SPoint32 * /* inStartMouseImagePt */,
								   const SPoint32 * /* inCurrMouseImagePt */,
								   EDrawRegionState /* inDrawState */) {

}


#pragma mark -

/*======================================================================================
	This method is ALWAYS called from within the DoTrackMouse() method just before
	the mouse tracking loop begins. If you want to setup instance varibles before
	tracking begins, or draw an initial outline of a tracking image, this is the
	place to do it. 
	
	The inStartMouseImagePt parameter has been pinned to the bounds passed to 
	DoTrackMouse() in the inMouseImagePinRect parameter.
======================================================================================*/

void CTableRowDragger::BeginTracking(const SPoint32 * /* inStartMouseImagePt */)
{

	mDidDraw = false;
	Rect frame;
	mView->CalcPortFrameRect(frame);

	mPortInsertionLineV = min_Int16;
	mPortInsertionLineLeft = frame.left;
	mPortInsertionLineRight = frame.right - 1;
	mPortFrameMinV = frame.top;
	mPortFrameMaxV = frame.bottom - 1;
	
	// Calculate rects for the arrows
	mView->GetSuperView()->CalcPortFrameRect(frame);
	
	mPortLeftArrowBox.top = mPortRightArrowBox.top = -cArrowVCenterTopOffset;
	mPortLeftArrowBox.right = frame.left - cArrowHMargin;
	mPortLeftArrowBox.bottom = mPortRightArrowBox.bottom = mPortLeftArrowBox.top + cSmallIconHeight;
	mPortLeftArrowBox.left = mPortLeftArrowBox.right - cSmallIconWidth;
	mPortRightArrowBox.left = frame.right + cArrowHMargin;
	mPortRightArrowBox.right = mPortRightArrowBox.left + cSmallIconWidth;

	LTableView *theTable = dynamic_cast<LTableView *>(mView);
	Assert_(theTable != nil);
	TableIndexT rows;
	theTable->GetTableSize(rows, mNumCols);
}

	
/*======================================================================================
	This method is ALWAYS called at the end of user mouse tracking. If the points
	inStartMouseImagePt and inEndMouseImagePt not are equal, the user actually move 
	the mouse somewhere different from where it started. This method should normally
	erase any visual representation of a dragging region.

	All input points have been pinned to the bounds passed to DoTrackMouse() in the 
	inMouseImagePinRect parameter.
======================================================================================*/

void CTableRowDragger::EndTracking(const SPoint32 *inStartMouseImagePt,
						   		   const SPoint32 *inEndMouseImagePt) {

	DrawDragRegion(inStartMouseImagePt, inEndMouseImagePt, eDoEraseScroll);
}

	
/*======================================================================================
	Draw the local row rectangle taking into account the current and starting mouse
	positions.
======================================================================================*/

void CTableRowDragger::DrawDragRegion(const SPoint32 *inStartMouseImagePt,
							  		  const SPoint32 *inCurrMouseImagePt,
							  		  EDrawRegionState inDrawState) {
	
	if ( !mDidDraw && (inDrawState != eDoDraw) ) return;	// Nothing to erase
	
	LTableView *theTable = dynamic_cast<LTableView *>(mView);
	mDidDraw = true;
	
	// Calculate image rectangle to draw
	
	LImageRect drawImageRect;
	{
		SPoint32 topLeftPt, botRightPt;
	
		theTable->GetImageCellBounds(STableCell(mClickedRow, 1), topLeftPt.h, topLeftPt.v,
								 	 botRightPt.h, botRightPt.v);
		if ( mNumCols > 1 ) {
			SPoint32 tempPt;
			theTable->GetImageCellBounds(STableCell(mClickedRow, mNumCols), tempPt.h, tempPt.v,
								 		 botRightPt.h, botRightPt.v);
		}
		Assert_((topLeftPt.v != 0) || (botRightPt.v != 0));
		--botRightPt.v;
		
		drawImageRect.Set(topLeftPt.h, topLeftPt.v, botRightPt.h, botRightPt.v);
		drawImageRect.Offset(inCurrMouseImagePt->h - inStartMouseImagePt->h,
							 inCurrMouseImagePt->v - inStartMouseImagePt->v);
	}
								 
	// Setup the port

	if ( inDrawState == eDoDraw ) {
		UpdateInsertionPoint(&drawImageRect, theTable, inDrawState);
	}
	
	theTable->FocusDraw();
	
	StColorPenState::Normalize();

	::PenMode(patXor);
	::PenPat(&UQDGlobals::GetQDGlobals()->gray);
	
	Rect rowRect;
	CMouseDragger::ImageToLocalRect(theTable, &drawImageRect, &rowRect);

	::FrameRect(&rowRect);
	
	::PenNormal();
	
	if ( inDrawState != eDoDraw ) {
		UpdateInsertionPoint(&drawImageRect, theTable, inDrawState);
	}
}


/*======================================================================================
	Update display of the insertion line.
======================================================================================*/

void CTableRowDragger::UpdateInsertionPoint(const LImageRect *inImageCellDrawRect,
											LTableView *inTableView,
											EDrawRegionState inDrawState) {
	
	// Calculate the current row we're over
	
	
	STableCell hitCell;
	SPoint32 curCenterPt;
	
	curCenterPt.h = inImageCellDrawRect->Left() + 
						((inImageCellDrawRect->Right() - inImageCellDrawRect->Left()) / 2);
	curCenterPt.v = inImageCellDrawRect->Top() +
						((inImageCellDrawRect->Bottom() - inImageCellDrawRect->Top()) / 2);

	inTableView->GetCellHitBy(curCenterPt, hitCell);
	Assert_(inTableView->IsValidRow(hitCell.row));
	
	Int16 newPortInsertionLineV = min_Int16;
	
	if ( inDrawState == eDoDraw ) {
		
		if ( hitCell.row != mClickedRow ) {
		
			TableIndexT	rows, cols;
			inTableView->GetTableSize(rows, cols);

			// Adjust the cell so that mouse drawing occurs over the insertion line
			SPoint32 topLeftPt, botRightPt;
			
			inTableView->GetImageCellBounds(hitCell, topLeftPt.h, topLeftPt.v,
										 	botRightPt.h, botRightPt.v);
				
			Assert_((topLeftPt.v != 0) || (botRightPt.v != 0));
			Int32 cellMidV = topLeftPt.v + ((botRightPt.v - topLeftPt.v)>>1);
			Boolean ignoreChanges = true;
			
			if ( curCenterPt.v >= cellMidV ) {
				if ( hitCell.row < mClickedRow ) {
					if ( hitCell.row < rows ) {
						++hitCell.row;
						ignoreChanges = false;
					}
				}
			} else {
				if ( hitCell.row > mClickedRow ) {
					if ( (hitCell.row > 1) && ((hitCell.row != rows) || (curCenterPt.v < cellMidV)) ) {
						--hitCell.row;
						ignoreChanges = false;
					}
				}
			}
			if ( !ignoreChanges ) {
				ignoreChanges = (hitCell.row != mClickedRow);
				if ( ignoreChanges ) {
					inTableView->GetImageCellBounds(hitCell, topLeftPt.h, topLeftPt.v,
												 	botRightPt.h, botRightPt.v);
					Assert_((topLeftPt.v != 0) || (botRightPt.v != 0));
				}
			}
			if ( ignoreChanges ) {
				Point portPoint;
				if ( hitCell.row > mClickedRow ) {
					CMouseDragger::ImageToPortPt(inTableView, &botRightPt, &portPoint);
					newPortInsertionLineV = portPoint.v;
				} else {
					CMouseDragger::ImageToPortPt(inTableView, &topLeftPt, &portPoint);
					newPortInsertionLineV = portPoint.v - 1;
				}
#ifdef Debug_Signal
				{
					Rect portRect;
					LImageRect imageRect;
					inTableView->CalcPortFrameRect(portRect);
					CMouseDragger::PortToImageRect(inTableView, &portRect, &imageRect);
					Assert_(portRect.bottom >= newPortInsertionLineV);
					Assert_(portRect.top - 2 <= newPortInsertionLineV);
				}
#endif // Debug_Signal
			}
		}
		
		mOverRow = hitCell.row;
	}
	
	if ( (mPortInsertionLineV != newPortInsertionLineV) && (inDrawState != eDoErase) ) {
	
		LWindow *viewWindow = LWindow::FetchWindowObject(inTableView->GetMacPort());
	
		viewWindow->FocusDraw();
		StColorPenState::Normalize();
	
		Int16 vPos;
		Rect arrowLeft, arrowRight;
		
		// Erase old insertion line
		
		if ( mPortInsertionLineV != min_Int16 ) {
			CalcArrowRects(mPortInsertionLineV, &arrowLeft, &arrowRight, true);
			if ( mPortInsertionLineV < mPortFrameMinV ) {
				vPos = mPortFrameMinV;
			} else if ( mPortInsertionLineV >= mPortFrameMaxV ) {
				vPos = mPortFrameMaxV;
			} else {
				vPos = mPortInsertionLineV;
				::PenSize(1, 2);
			}
			::ForeColor(whiteColor);
			::MoveTo(mPortInsertionLineLeft, vPos);
			::LineTo(mPortInsertionLineRight, vPos);
			
			// Update insertion arrows
			
			viewWindow->InvalPortRect(&arrowLeft);
			viewWindow->InvalPortRect(&arrowRight);
			viewWindow->UpdatePort();
			viewWindow->FocusDraw();

/* Old code that relies on ApplyForeAndBackColors()

			// Update insertion arrows
			
			inTableView->ApplyForeAndBackColors();
			::EraseRect(&arrowLeft);
			::EraseRect(&arrowRight);
			::ForeColor(blackColor);
			::PenSize(1, 1);
*/
		}
		
		mPortInsertionLineV = newPortInsertionLineV;
		
		// Draw new insertion line
		
		if ( mPortInsertionLineV != min_Int16 ) {
			CalcArrowRects(mPortInsertionLineV, &arrowLeft, &arrowRight, false);
			if ( mPortInsertionLineV < mPortFrameMinV ) {
				vPos = mPortFrameMinV;
			} else if ( mPortInsertionLineV >= mPortFrameMaxV ) {
				vPos = mPortFrameMaxV;
			} else {
				vPos = mPortInsertionLineV;
				::PenSize(1, 2);
			}
			::MoveTo(mPortInsertionLineLeft, vPos);
			::LineTo(mPortInsertionLineRight, vPos);
			::PlotIconID(&arrowLeft, atNone, ttNone, icm8_LeftInsertArrow);
			::PlotIconID(&arrowRight, atNone, ttNone, icm8_RightInsertArrow);
		}

		::PenSize(1, 1);
	}
}


/*======================================================================================
	Update display of the insertion arrows.
======================================================================================*/

void CTableRowDragger::CalcArrowRects(Int16 inPortLineV, Rect *outLeftRect, Rect *outRightRect,
							  		  Boolean inErase) {
	
	// Calculate rects for the arrows
		
	*outLeftRect = mPortLeftArrowBox;
	*outRightRect = mPortRightArrowBox;
	::OffsetRect(outLeftRect, 0, inPortLineV);
	::OffsetRect(outRightRect, 0, inPortLineV);
	if ( inErase ) {
		outLeftRect->left = outLeftRect->right - cArrowPolyWidth;
		outRightRect->right = outRightRect->left + cArrowPolyWidth;
	}
}


