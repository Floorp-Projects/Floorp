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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <UGAColorRamp.h>

#include "CPatternButton.h"
#include "UGraphicGizmos.h"
#include "UGAAppearance.h"
#include "CSharedPatternWorld.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CPatternButton::CPatternButton(LStream* inStream)
	:	CToolbarButton(inStream)
{
	mMouseInFrame = false;

	ResIDT thePatternID;
	*inStream >> thePatternID;
	
	*inStream >> mOrientation;
	
	mPatternWorld = CSharedPatternWorld::CreateSharedPatternWorld(thePatternID);
	ThrowIfNULL_(mPatternWorld);
	mPatternWorld->AddUser(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CPatternButton::~CPatternButton()
{
	mPatternWorld->RemoveUser(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawButtonContent
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
	
void CPatternButton::DrawButtonContent(void)
{
	CGrafPtr thePort;
	::GetPort(&(GrafPtr)thePort);
	
	Rect theFrame = mCachedButtonFrame;
	Point theAlignment;
	
	CalcOrientationPoint(theAlignment);
	mPatternWorld->Fill(thePort, theFrame, theAlignment);

	::InsetRect(&theFrame, 2, 2);

	if (IsActive() && IsEnabled())
		{
		if (IsTrackInside() || (!IsBehaviourButton() && (mValue == Button_On)))
			{
			DrawButtonHilited();
			}
		else if (IsMouseInFrame())
			{
			DrawButtonNormal();
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CPatternButton::DrawButtonGraphic(void)
{
	bool useMouseOverIcon = false;
	mIconTransform = kTransformNone;
	
	if (IsEnabled() && IsActive())
	{
		if (!IsBehaviourButton() && (GetValue() == Button_On))
		{
			// do something different if IsTrackInside()
			//theNewID += 2;
		}
		else if (IsTrackInside())
			mIconTransform = kTransformSelected;
		else if (IsMouseInFrame())
			useMouseOverIcon = true;
	}
	else
		mIconTransform = kTransformDisabled;

	ResIDT theIconID;
	if ( useMouseOverIcon ) {
		theIconID = GetGraphicID();
		SetGraphicID(theIconID + 2);
	}
	CToolbarButton::DrawButtonGraphic();
	if ( useMouseOverIcon )
		SetGraphicID(theIconID);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CPatternButton::DrawButtonTitle(void)
{
	StColorPenState::Normalize();
		
	if (IsTrackInside() || GetValue() == Button_On)
	{
		::RGBForeColor(&UGAColorRamp::GetWhiteColor());
		::OffsetRect(&mCachedTitleFrame, 1, 1);
	}
	else if (!IsActive() || !IsEnabled())
	{
//		::RGBForeColor(&UGAColorRamp::GetColor(colorRamp_Gray7));
		::TextMode(grayishTextOr);
	}

	UGraphicGizmos::PlaceStringInRect(mTitle, mCachedTitleFrame, teCenter, teCenter);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CPatternButton::MouseEnter(
	Point				/* inPortPt */,
	const EventRecord&	/* inMacEvent */)
{
	mMouseInFrame = true;
	if (IsActive() && IsEnabled())
		Draw(NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CPatternButton::MouseWithin(
	Point				/* inPortPt */,
	const EventRecord&	/* inMacEvent */)
{
	// Nothing to do for now
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CPatternButton::MouseLeave(void)
{
	mMouseInFrame = false;
	if (IsActive() && IsEnabled())
		Draw(NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CPatternButton::CalcOrientationPoint(Point& outPoint)
{
	// the downward scale from 32 to 16 bit coordinate spaces
	// in converting from SPoint32 to plain old Point
	// is ok because we are guaranteed to be in bounds since we
	// wont be drawing if we cant FocusExposed().

	SPoint32 theFrameLocation;
	switch (mOrientation)
		{
		case CSharedPatternWorld::eOrientation_Self:
			{
			GetFrameLocation(theFrameLocation);
			outPoint.h = theFrameLocation.h;
			outPoint.v = theFrameLocation.v;
			PortToLocalPoint(outPoint);
			}
			break;
			
		case CSharedPatternWorld::eOrientation_Superview:
			{
			mSuperView->GetFrameLocation(theFrameLocation);
			outPoint.h = -theFrameLocation.h;
			outPoint.v = -theFrameLocation.v;
			PortToLocalPoint(outPoint);
			}
			break;
			
		case CSharedPatternWorld::eOrientation_Port:
			{
			mSuperView->GetPortOrigin(outPoint);
			}
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawButtonNormal
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Draw button AGA bevel border

void CPatternButton::DrawButtonNormal()
{
	// ¥ Setup a device loop so that we can handle drawing at the correct bit depth
	StDeviceLoop	theLoop ( mCachedButtonFrame );
	Int16			depth;

	// Draw face of button first			
	while ( theLoop.NextDepth ( depth )) 
		if ( depth >= 4 )		// don't do anything for black and white
			{
			Rect rect = mCachedButtonFrame;
			::InsetRect(&rect, 1, 1);
			UGraphicGizmos::LowerRoundRectColorVolume(rect, 4, 4,
													  UGAAppearance::sGAHiliteContentTint);
			}

	// Now draw GA button bevel
	UGAAppearance::DrawGAButtonBevelTint(mCachedButtonFrame);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawButtonHilited
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Draw button AGA "mouse-down" look

void CPatternButton::DrawButtonHilited()
{
	// ¥ Setup a device loop so that we can handle drawing at the correct bit depth
	StDeviceLoop	theLoop ( mCachedButtonFrame );
	Int16			depth;

	Rect frame = mCachedButtonFrame;

	// Draw face of button first
	while ( theLoop.NextDepth ( depth )) 
		if ( depth >= 4 )		// don't do anything for black and white
			{
			::InsetRect(&frame, 1, 1);
			// Do we need to do this very slight darkening?
			UGraphicGizmos::LowerRoundRectColorVolume(frame, 4, 4, UGAAppearance::sGASevenGrayLevels);
			::InsetRect(&frame, -1, -1);
			}

	// Now draw GA pressed button bevel
	UGAAppearance::DrawGAButtonPressedBevelTint(mCachedButtonFrame);
}