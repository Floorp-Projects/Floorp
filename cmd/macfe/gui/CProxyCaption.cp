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

// ===========================================================================
//	CProxyCaption.cp
//
// ===========================================================================

#include "CProxyCaption.h"

#include "mfinder.h" // needed for workaround function to bug in Appe's
					 // ::TruncText and ::TruncString - andrewb 6/20/97
					 
#include <UGAColorRamp.h>
#include <UDrawingUtils.h>
#include <UNewTextDrawing.h>

// ---------------------------------------------------------------------------
//		¥ CProxyPane
// ---------------------------------------------------------------------------

CProxyCaption::CProxyCaption(
	LStream*		inStream)
	
	:	super(inStream)
{
}

// ---------------------------------------------------------------------------
//		¥ FinishCreateSelf
// ---------------------------------------------------------------------------
	
void
CProxyCaption::FinishCreateSelf()
{
	super::FinishCreateSelf();
	
	CalcLocalFrameRect(mOriginalFrame);
}

// ---------------------------------------------------------------------------
//		¥ SetDescriptor
// ---------------------------------------------------------------------------
	
void
CProxyCaption::SetDescriptor(
	ConstStringPtr	inDescriptor)
{
	super::SetDescriptor(inDescriptor);
	
	FocusDraw();
	
	// Adjust frame to fit new descriptor
	
	Rect	frame = mOriginalFrame;
	Rect	newFrame;
	
	Int16	just = UTextTraits::SetPortTextTraits(mTxtrID);

	StringPtr theDescriptor = const_cast<StringPtr>(inDescriptor);
	
	frame.left += 1; // A little extra padding looks better

	UNewTextDrawing::MeasureWithJustification(
									reinterpret_cast<char*>(&theDescriptor[1]),
									inDescriptor[0],
									frame,
									just,
									newFrame,
									true);
							
	if ((newFrame.right - newFrame.left) > (frame.right - frame.left))
	{
		newFrame = frame;
	}
	
	
		 // The + 2 factor seems to be necessary to not lose the
		 // last word of the title
	ResizeFrameTo((newFrame.right - newFrame.left) + 2, newFrame.bottom - newFrame.top, true);
}	

// ---------------------------------------------------------------------------
//		¥ DrawSelf
// ---------------------------------------------------------------------------

void
CProxyCaption::DrawSelf()
{
	Rect	frame;
	CalcLocalFrameRect(frame);
	
	Int16	justification = UTextTraits::SetPortTextTraits(mTxtrID);

	::RGBBackColor(&UGAColorRamp::GetBlackColor());
	::RGBForeColor(&UGAColorRamp::GetWhiteColor());
	
	::EraseRect(&frame);
	
	frame.left += 1; // A little extra padding looks better
	
	DrawText((Ptr)&mText[1], mText[0], frame);
}

// ---------------------------------------------------------------------------
//		¥ DrawText
// ---------------------------------------------------------------------------
//	NOTE: This version is differs from UDrawingUtils::DrawWithJustification
//	in that we don't wrap lines, we just draw one line truncated. However,
//	the measurements are identical to UDrawingUtils::DrawWithJustification
//	so that UNewTextDrawing::MeasureWithJustification will correspond to what
//	is drawn.

void
CProxyCaption::DrawText(
	Ptr				inText,
	Int32			inLength,
	const Rect&		inRect)
{
	FontInfo fontInfo;
	
	::GetFontInfo(&fontInfo);
	
	Int16 lineBase = inRect.top + fontInfo.ascent + fontInfo.leading;
	
	StClipRgnState saveClip;				// Draw within input rectangle
	saveClip.ClipToIntersection(inRect);
	
	::MoveTo(inRect.left, lineBase);
	
	short length = inLength;
	
	// DANGER! If you were to use Apple's ::TruncText with TruncMiddle as the
	// last parameter you run the risk of crashing--in a very bad way. andrewb
	MiddleTruncationThatWorks((char *)inText, length, inRect.right - inRect.left);
	::DrawText(inText, 0, length);
}

