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
// CToolbarBevelButton
// Mike Pinkerton, Netscape Communications
//
// A bevel button that knows how to change modes (text, icon, icon & text).
//

#include "CToolbarBevelButton.h"


// "Magic" constants
const Int16 cIconOnlyHeight = 32;
const Int16 cIconOnlyWidth = 36;
const Int16 cTextOnlyHeight = 21;


CToolbarBevelButton::CToolbarBevelButton(LStream* inStream)
	:	LCmdBevelButton(inStream),
		mCurrentMode(defaultMode),
		mOriginalWidth(0),
		mOriginalHeight(0),
		mOriginalTitle(NULL),
		mResID(0)
{
}


CToolbarBevelButton::~CToolbarBevelButton()
{
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	FinishCreateSelf
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CToolbarBevelButton::FinishCreateSelf()
{
	LBevelButton::FinishCreateSelf();

	Rect theButtonRect;
	CalcLocalFrameRect(theButtonRect);

	mOriginalWidth = theButtonRect.right - theButtonRect.left;
	mOriginalHeight = theButtonRect.bottom - theButtonRect.top;

	// save the resid in case we have to go to textonly mode and cannot recover
	// it from the control manager
	ControlButtonContentInfo currInfo;
	GetContentInfo ( currInfo );
	mResID = currInfo.u.resID;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ChangeMode
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
Boolean 
CToolbarBevelButton :: ChangeMode(Int8 inNewMode, SDimension16& outDimensionDeltas)
{
	SDimension16 oldDimensions;
	GetFrameSize(oldDimensions);

	outDimensionDeltas.width = 0;
	outDimensionDeltas.height = 0;

	ControlButtonContentInfo newInfo;
	newInfo.contentType = kControlContentIconSuiteRes;
	newInfo.u.resID = mResID;

	// going from icons only to anything with text requires restoring the title
	if ( mCurrentMode == eMode_IconsOnly )
		RestoreTitle();
	
	mCurrentMode = inNewMode;
	
	switch (inNewMode)
	{
		case eMode_IconsOnly:
			// save the title because we can't get it back from the control info
			SaveTitle();
			SetDescriptor("\p");
			outDimensionDeltas.width = cIconOnlyWidth - oldDimensions.width;
			outDimensionDeltas.height = cIconOnlyHeight - oldDimensions.height;
			break;
		
		case eMode_TextOnly:
			newInfo.contentType = kControlContentTextOnly;
			outDimensionDeltas.width = mOriginalWidth - oldDimensions.width;
			outDimensionDeltas.height = cTextOnlyHeight - oldDimensions.height;
			break;
		
		case eMode_IconsAndText:
			outDimensionDeltas.width = mOriginalWidth - oldDimensions.width;
			outDimensionDeltas.height = mOriginalHeight - oldDimensions.height;
			break;
	}

	SetContentInfo ( newInfo );
	ResizeFrameBy(outDimensionDeltas.width, outDimensionDeltas.height, true);
	return true;
}


void
CToolbarBevelButton :: SaveTitle ( )
{
	delete mOriginalTitle;
	mOriginalTitle = new LStr255();
	GetDescriptor(*mOriginalTitle);
}


void
CToolbarBevelButton :: RestoreTitle ( )
{
	if ( mOriginalTitle ) {
		SetDescriptor ( *mOriginalTitle );
		delete mOriginalTitle;
		mOriginalTitle = NULL;
	}
}