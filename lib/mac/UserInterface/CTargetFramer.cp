/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
