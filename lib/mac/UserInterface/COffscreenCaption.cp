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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	TBD:
//		-	If this caption is going to resize, we need code to recalc the
//			offscreen world accordingly.
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <PP_Messages.h>
#include <UDrawingState.h>
#include <UDrawingUtils.h>
#include <UGWorld.h>
#include <UMemoryMgr.h>
#include <UTextTraits.h>

#include "COffscreenCaption.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

COffscreenCaption::COffscreenCaption(LStream* inStream)
	:	LCaption(inStream)
{
	// The erase color is initialized to -1 because it is an invalid palette
	// index.
	mEraseColor = -1;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

COffscreenCaption::COffscreenCaption(
	const SPaneInfo&	inPaneInfo,
	ConstStringPtr		inString,
	ResIDT				inTextTraitsID)
		:	LCaption(inPaneInfo, inString, inTextTraitsID)
{
	// The erase color is initialized to -1 because it is an invalid palette
	// index.
	mEraseColor = -1;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void COffscreenCaption::SetDescriptor(ConstStringPtr inDescriptor)
{
	if (inDescriptor == NULL)
		mText = "\p";
	else
		mText = inDescriptor;
		
	Draw(NULL);
}

void COffscreenCaption::SetDescriptor(const char* inCDescriptor)
{
	if (inCDescriptor == NULL)
		mText = "\p";
	else
		mText = inCDescriptor;
		
	Draw(NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void COffscreenCaption::Draw(RgnHandle inSuperDrawRgnH)
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
			StValueChanger<EDebugAction> okayToFail(UDebugging::gDebugThrow, debugAction_Nothing);
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
//	¥	DrawSelf
//
//	If the erase color has been set, we'll erase the background in that
//	color.  Otherwise we should use the window's colors.	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void COffscreenCaption::DrawSelf(void)
{
	Rect	theFrame;
	CalcLocalFrameRect(theFrame);
	
	Int16	just = UTextTraits::SetPortTextTraits(mTxtrID);
	
	RGBColor theTextColor;
	::GetForeColor(&theTextColor);
	
	if (mEraseColor != -1)
		::PmBackColor(mEraseColor);
	else
		ApplyForeAndBackColors();

	::EraseRect(&theFrame);
	::RGBForeColor(&theTextColor);
	UTextDrawing::DrawWithJustification((Ptr)&mText[1], mText[0], theFrame, just);
}	
