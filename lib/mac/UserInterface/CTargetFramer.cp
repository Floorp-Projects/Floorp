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

// Author: John R. McMullen


#include "CTargetFramer.h"

#include "UGraphicGizmos.h"

#include <LCommander.h>
#include <LPane.h>

static const kSideInsetPixels = 1;
static const kTopAndBottomInsetPixels = 1;

//-----------------------------------
CTargetFramer::CTargetFramer()
//-----------------------------------
:	LAttachment(msg_AnyMessage, /* inExecuteHost = */ true)
,	mPane(nil)
,	mCommander(nil)
,	mDrawWithHilite(false)
{

} // CTargetFramer::CTargetFramer

//-----------------------------------
CTargetFramer::CTargetFramer(LStream* inStream)
//-----------------------------------
:	LAttachment(inStream)
,	mPane(nil)
,	mCommander(nil)
{
	SetMessage(msg_AnyMessage);
	SetExecuteHost(true);
	SetOwnerHost(LAttachable::GetDefaultAttachable());
} // CTargetFramer::CTargetFramer

//-----------------------------------
CTargetFramer::~CTargetFramer()
//-----------------------------------
{
} // CTargetFramer::~CTargetFramer

//-----------------------------------
void CTargetFramer::SetOwnerHost(LAttachable* inAttachable)
//-----------------------------------
{
	mCommander = dynamic_cast<LCommander*>(inAttachable);
	Assert_(mCommander);
	mPane = dynamic_cast<LPane*>(inAttachable);
	Assert_(mPane);
	mDrawWithHilite = mCommander->IsTarget();
	Inherited::SetOwnerHost(inAttachable);
} // CTargetFramer::SetOwnerHost

//-----------------------------------
void CTargetFramer::CalcBorderRegion(RgnHandle ioBorderRgn)
//-----------------------------------
{
	// compute the border region
	StRegion frameRgn;
	Rect frameRect;
	mPane->CalcLocalFrameRect(frameRect);
	::RectRgn(frameRgn, &frameRect);
	::InsetRect(&frameRect, kSideInsetPixels, kTopAndBottomInsetPixels);
	::RectRgn(ioBorderRgn, &frameRect);
	::DiffRgn(frameRgn, ioBorderRgn, ioBorderRgn);
} //  CTargetFramer::CalcBorderRegion

//-----------------------------------
void CTargetFramer::InvertBorder()
//-----------------------------------
{
	StRegion borderRgn;
	StColorPenState penState;
	penState.Normalize();

	CalcBorderRegion(borderRgn);

	// Fiddle with the hilite color so that our inversion does not
	// interfere with the inversion already done by the pane
	RGBColor savedHiliteColor;
	::LMGetHiliteRGB(&savedHiliteColor);
	RGBColor darkTingeColor, ignoredLightColor;
	UGraphicGizmos::CalcWindowTingeColors(
		mPane->GetMacPort(), ignoredLightColor, darkTingeColor);
	::HiliteColor(&darkTingeColor);
	UDrawingUtils::SetHiliteModeOn();
	::InvertRgn(borderRgn);
	::HiliteColor(&savedHiliteColor);
} //  CTargetFramer::InvertBorder

//-----------------------------------
void CTargetFramer::ExecuteSelf(MessageT inMessage, void* ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
		case msg_BecomingTarget:
		case msg_ResigningTarget:
			mPane->FocusDraw();
			// FALL THROUGH!
		case msg_DrawOrPrint:
		case msg_DrawSelfDone:
		{
			switch (inMessage)
			{
				case msg_BecomingTarget:
				case msg_ResigningTarget:
				{
					mDrawWithHilite = !mDrawWithHilite;
					InvertBorder();
					break;
				}
				case msg_DrawOrPrint:
				{
					// Called before the pane does DrawSelf
					StRegion borderRgn;
					CalcBorderRegion(borderRgn);
					::EraseRgn(borderRgn);
					break;
				}
				case msg_DrawSelfDone:
				{
					// Called after the pane does DrawSelf
					if (mDrawWithHilite)
						InvertBorder();
					break;
				}
			} // switch
			break;
		}
		default:
			return;
	}
} // CTargetFramer::ExecuteSelf
