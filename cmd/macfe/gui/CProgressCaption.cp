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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CProgressCaption.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CProgressCaption.h"

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <URegions.h>
#include "UGraphicGizmos.h"

#pragma mark public

enum {eNoColorSet = -1};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CProgressCaption::CProgressCaption(LStream* inStream)
	:	LView(inStream),
		mBar(NULL), mStatusText(NULL)
{
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CProgressCaption::~CProgressCaption()
{
}


void
CProgressCaption :: FinishCreateSelf ( )
{
	mBar = dynamic_cast<LProgressBar*>(FindPaneByID(kProgressBar));
	mStatusText = dynamic_cast<LCaption*>(FindPaneByID(kStatusText));
	
	Assert_(mBar != NULL);
	Assert_(mStatusText != NULL);

} // FinishCreateSelf


//
// GetDescriptor
// SetDescriptor
//
// Pass-through's to the caption object.
//

StringPtr
CProgressCaption::GetDescriptor(
	Str255	outDescriptor) const
{
	return mStatusText->GetDescriptor ( outDescriptor );
}

void
CProgressCaption::SetDescriptor(ConstStringPtr	inDescriptor)
{
	mStatusText->SetDescriptor ( "\p" );	
	if (FocusExposed())
		Draw(NULL);

	mStatusText->SetDescriptor ( inDescriptor );	
	if (FocusExposed())
		Draw(NULL);
}

void
CProgressCaption::SetDescriptor(const char* inCDescriptor)
{
	mStatusText->SetDescriptor ( "\p" );	
	if (FocusExposed())
		Draw(NULL);

	mStatusText->SetDescriptor ( LStr255(inCDescriptor) );
	if (FocusExposed())
		Draw(NULL);
}


//
// GetValue
// SetValue
//
// Pass-throughs to the progress bar object. The value (-1) from layout means set the
// bar to an indefinite state (barber pole).
//

void
CProgressCaption :: SetValue ( Int32 inValue )
{
	if ( inValue == eIndefinite ) {
		EventRecord ignored;				// SpendTime() really doesn't use this. Only needed
											// because there is no API to spin barber pole (idle).
		mBar->SetIndeterminateFlag ( true );
		mBar->SpendTime(ignored);
	}
	else {
		mBar->SetIndeterminateFlag ( false );	
		mBar->SetValue ( inValue );
	}
}

Int32
CProgressCaption :: GetValue ( ) const
{
	return mBar->GetValue ( );
}


#if 0
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressCaption::Draw(RgnHandle inSuperDrawRgnH)
{
	Rect theFrame;
	if ((mVisible == triState_On) && CalcPortFrameRect(theFrame) &&
			((inSuperDrawRgnH == nil) || RectInRgn(&theFrame, inSuperDrawRgnH)) && FocusDraw())
		{
		PortToLocalPoint(topLeft(theFrame));	// Get Frame in Local coords
		PortToLocalPoint(botRight(theFrame));

		if (ExecuteAttachments(msg_DrawOrPrint, &theFrame))
			{
			Boolean bDidDraw = false;

			StColorPenState thePenSaver;
			StColorPenState::Normalize();
			
			// Fail safe offscreen drawing
			StValueChanger<EDebugAction> okayToFail(gDebugThrow, debugAction_Nothing);
			try
				{			
				LGWorld theOffWorld(theFrame, 0, useTempMem);

				if (!theOffWorld.BeginDrawing())
					throw memFullErr;
					
				DrawSelf();
					
				theOffWorld.EndDrawing();
				theOffWorld.CopyImage(GetMacPort(), theFrame, srcCopy);
				bDidDraw = true;
				}
			catch (...)
				{
				// 	& draw onscreen
				}
				
			if (!bDidDraw)
				DrawSelf();
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressCaption::GetActiveColors(RGBColor& bodyColor, RGBColor& barColor, RGBColor& frameColor) const
{
	CProgressBar::GetActiveColors(bodyColor, barColor, frameColor);
	barColor.red = 0x8888;
	barColor.green = 0x8888;
	barColor.blue = 0xFFFF;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CProgressCaption::DrawSelf(void)
{
	Rect theFrame;
	CalcLocalFrameRect(theFrame);

	ResIDT theTraitsID;
	switch (GetValue())
	{
		case eIndefinite:
			theTraitsID = mIndefiniteTraitsID;
			break;
		case eBarHidden:
			theTraitsID = mHiddenTraitsID;
			break;
		default:
			theTraitsID = mBoundedTraitsID;
			break;
	}

	Int16 just = UTextTraits::SetPortTextTraits(theTraitsID);

	if (GetValue() == eBarHidden)
	{
		RGBColor theTextColor;
		::GetForeColor(&theTextColor);
		
		if (mEraseColor != eNoColorSet)
			::PmBackColor(mEraseColor);
		else
			ApplyForeAndBackColors();

		::EraseRect(&theFrame);
		::RGBForeColor(&theTextColor);
	}
	else
	{
		CProgressBar::DrawSelf();
	}
	theFrame.right -= 3;
	theFrame.left += 3;
	theFrame.top -= 1;
	UGraphicGizmos::PlaceStringInRect(mText, theFrame, just);
}

#endif