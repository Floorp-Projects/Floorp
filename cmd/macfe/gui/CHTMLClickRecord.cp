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

// CHTMLClickRecord.cp

#include "CHTMLClickRecord.h"
#include "CNSContext.h"
#include "CContextMenuAttachment.h"
#include "CApplicationEventAttachment.h"

#include "layers.h"
#include "mimages.h"
#include "proto.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CHTMLClickRecord::CHTMLClickRecord(
	Point 				inLocalWhere,
	const SPoint32&		inImageWhere,
	CNSContext* 		inContext,
	LO_Element* 		inElementelement,
	CL_Layer* 			inLayer)
{
	mLocalWhere = inLocalWhere;
	mImageWhere = inImageWhere;
	mElement = inElementelement;
	mAnchorData= NULL;
	mContext = inContext;
	mLayer = inLayer;
	mClickKind = eWaiting;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// Figures out everything (anchors, click kind) about the current cursor position
// called only once -- this is completely horrible

void CHTMLClickRecord::Recalc(void)
{
	if (mElement == NULL)
		{
		mClickKind = eNone;
		return;
		}

	switch (mElement->type)
		{
		case LO_IMAGE:
			mClickKind = CalcImageClick();
			break;
			
		case LO_TEXT:
			mClickKind = CalcTextClick();
			break;
			
		case LO_EDGE:
			mClickKind = CalcEdgeClick();
			break;
			
		default:
			mClickKind = eNone;
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

EClickKind CHTMLClickRecord::CalcImageClick(void)
{
	EClickKind	theKind;
//	SPoint32	localWhere;
	
	// Get the image URL
	PA_LOCK(mImageURL, char *, (char*)mElement->lo_image.image_url);
	PA_UNLOCK(loImage->image_url);
	
	// Where was the click?
/*
	localWhere.h = mLocalWhere.h;
	localWhere.v = mLocalWhere.v;
*/
	// find with layer coordinates (mImageWhere) not local coordinates
	theKind = FindImagePart ( *mContext, &mElement->lo_image, &mImageWhere,
		&mClickURL, &mWindowTarget, mAnchorData );
			
	return theKind;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

EClickKind CHTMLClickRecord::CalcTextClick(void)
{
	EClickKind theKind = eNone;
	if (mElement->lo_text.anchor_href)
		{
		theKind = eTextAnchor;
		PA_LOCK(mClickURL, char*, (char*)mElement->lo_text.anchor_href->anchor);
		PA_UNLOCK(mElement->lo_text.anchor_href->anchor);

		PA_LOCK(mWindowTarget, char *,(char*)mElement->lo_text.anchor_href->target);
		PA_UNLOCK(mElement->lo_text.anchor_href->target);
		}

	return theKind;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

EClickKind CHTMLClickRecord::CalcEdgeClick(void)
{
	EClickKind theKind = eNone;
	if (mElement->lo_edge.movable)
		{
		theKind = eEdge;
		mIsVerticalEdge = mElement->lo_edge.is_vertical;
		mEdgeLowerBound = mElement->lo_edge.left_top_bound;
		mEdgeUpperBound = mElement->lo_edge.right_bottom_bound;
		}

	return theKind;
}

Boolean CHTMLClickRecord::IsClickOnAnchor(void) const
{
	return ((mClickKind != eNone) && (mClickKind != eEdge));
}

Boolean	CHTMLClickRecord::IsClickOnEdge(void) const
{
	return (mClickKind == eEdge);
}

#if 0

void CClickRecord::ResetNamedWindow()
{
	fWindowTarget = "";
}

#endif



//	Pixels hang below their points, and thus the pixels equal to
//	the top/left points are inside, but equal to the bottom/right
//	of the bounding box are outside.

Boolean CHTMLClickRecord::PixelReallyInElement(
	const SPoint32& 		inImagePoint,
	LO_Element* 			inElement)
{
	// account for image border
	Int32 borderWidth = 0;
	
	if (inElement->type == LO_IMAGE)
		borderWidth = inElement->lo_image.border_width;
	
	Int32 realX = inElement->lo_any.x + inElement->lo_any.x_offset;
	Int32 realY = inElement->lo_any.y + inElement->lo_any.y_offset;
	return 
		realY <= inImagePoint.v
		&& inImagePoint.v < (realY + inElement->lo_any.height + (2 * borderWidth))
		&& realX <= inImagePoint.h
		&& inImagePoint.h < (realX + inElement->lo_any.width + (2 * borderWidth));
}

//-----------------------------------
CHTMLClickRecord::EClickState CHTMLClickRecord::WaitForMouseAction(
	const SMouseDownEvent&		inMouseDown,
	LAttachable*				inAttachable,
	Int32						inDelay,
	Boolean						inExecuteContextMenuAttachment)
//-----------------------------------
{
	// ¥ The new way. Give a context menu attachment a chance to
	// execute.  If this returns true ("execute host"), there is no attachment,
	// so do things the old way.  If it returns false, assume the popup has been
	// handled. This can go away if the browser folkies decide to use
	// the attachment mechanism.
	
	CContextMenuAttachment::SExecuteParams params;
	params.inMouseDown = &inMouseDown;
	params.outResult = (CContextMenuAttachment::EClickState)eUndefined;
	if (inExecuteContextMenuAttachment)
	{
	 	inAttachable->ExecuteAttachments(
			CContextMenuAttachment::msg_ContextMenu,
			(void*)&params);
	}
	if (params.outResult != eUndefined)
		return (EClickState)params.outResult;

	// ¥ The old way.
	 return WaitForMouseAction(
	 			inMouseDown.whereLocal,
	 			inMouseDown.macEvent.when,
	 			inDelay);
} // CHTMLClickRecord::WaitForMouseAction

//-----------------------------------
CHTMLClickRecord::EClickState CHTMLClickRecord::WaitForMouseAction(
	const Point& 	inInitialPoint,
	Int32			inWhen,
	Int32			inDelay)
//-----------------------------------
{
	if (CApplicationEventAttachment::CurrentEventHasModifiers(controlKey))
	{
		return eMouseTimeout;
	}
	
	//	Spin on mouse down and wait for either the user to start
	//	dragging or for the popup delay to expire or for mouse up
	//	(in which case we just fall through)
	while (::StillDown())
		{
		Point theCurrentPoint;
		::GetMouse(&theCurrentPoint);
		
		if ((abs(theCurrentPoint.h - inInitialPoint.h) >= eMouseHystersis) ||
			(abs(theCurrentPoint.v - inInitialPoint.v) >= eMouseHystersis))
			return eMouseDragging;

		Int32 now = ::TickCount();

		if (abs( now - inWhen ) > inDelay)
			return eMouseTimeout;
		}
	
	return eMouseUpEarly;
} // CHTMLClickRecord::WaitForMouseAction

//-----------------------------------
void CHTMLClickRecord::CalculatePosition()
// Snarfed from mclick.cp
// Figures out everything (anchors, click kind) about the current cursor position
// called only once -- this is completely horrible
//-----------------------------------
{
	if ( mClickKind != eWaiting )
		return;
	
	if ( !mElement )
	{
		mClickKind = eNone;
		return;
	}

	if ( mElement->type == LO_IMAGE )
	{
		// ¥ click in the image. Complicated case:
		mClickKind = FindImagePart(*mContext, &mElement->lo_image, &mImageWhere, &mImageURL, &mWindowTarget, mAnchorData);
	}
	else if ( mElement->type == LO_TEXT && mElement->lo_text.anchor_href )
	{
		if ( mElement->lo_text.anchor_href )
		{
			mClickKind = eTextAnchor;
			PA_LOCK( mClickURL, char*, (char*)mElement->lo_text.anchor_href->anchor );
			PA_UNLOCK( mElement->lo_text.anchor_href->anchor );
			PA_LOCK( mWindowTarget, char *,(char*)mElement->lo_text.anchor_href->target );
			PA_UNLOCK( mElement->lo_text.anchor_href->target );
		}
		else
			mClickKind = eNone;
	}
	else if ( mElement->type == LO_EDGE && mElement->lo_edge.movable )
	{
		mClickKind = eEdge;
		mIsVerticalEdge = mElement->lo_edge.is_vertical;
		mEdgeLowerBound = mElement->lo_edge.left_top_bound;
		mEdgeUpperBound = mElement->lo_edge.right_bottom_bound;
	}
	else
		mClickKind = eNone;
}


Boolean CHTMLClickRecord::IsAnchor()
{
	CalculatePosition();
	return ((mClickKind != eNone) && (mClickKind != eEdge));
}

Boolean CHTMLClickRecord::IsEdge()
{
	CalculatePosition();
	return (mClickKind == eEdge);
}


