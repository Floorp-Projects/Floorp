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

#include "CColorCaption.h"


CChameleonCaption :: CChameleonCaption ( LStream* inStream )
	: LGACaption(inStream)
{
	mTextColor.red = mTextColor.green = mTextColor.blue = 0;		// black
	mBackColor.red = mBackColor.green = mBackColor.blue = 0xFFFF;	// white
}


//
// SetColor
//
// Use given colors instead. First version sets both fg and bg, second version only
// sets the fg color.
//
void
CChameleonCaption :: SetColor ( RGBColor & inTextColor, RGBColor & inBackColor )
{
	mTextColor = inTextColor;
	mBackColor = inBackColor;
	
} // SetColor


void
CChameleonCaption :: SetColor ( RGBColor & inTextColor )
{
	mTextColor = inTextColor;
	
} // SetColor


//
// DrawSelf
//
// Taken from LGACaption and modified to use our colors instead of those in
// the text traits rsrc. Differences from the original are marked.
//
void
CChameleonCaption :: DrawSelf()
{
	Rect	localFrame;
	CalcLocalFrameRect ( localFrame );
	
	// ¥ Setup the text traits and get the justification
	Int16	just = UTextTraits::SetPortTextTraits ( mTxtrID );
	
	// ¥ Get the fore and back colors applied
	ApplyForeAndBackColors ();
	RGBColor textColor = mTextColor;
	
	// NSCP -- Apply our own text colors, not those of the text traits rsrc
	::RGBBackColor(&mBackColor);
	
	// ¥ Setup a device loop so that we can handle drawing at the correct bit depth
	StDeviceLoop	theLoop ( localFrame );
	Int16				depth;
	while ( theLoop.NextDepth ( depth )) 
	{		
		// ¥ If we are drawing to a color screen then we are going to lighten
		// the color of the text when we are disabled
		if ( depth > 4 && !IsActive ())
			textColor = UGraphicsUtilities::Lighten ( &textColor );
	
		// NSCP -- Set the foreground color to our own color
		::RGBForeColor ( &textColor );
		
		// ¥ Now we can finally get the text drawn
		this->DrawText(localFrame, just);
	}	
	
} // DrawSelf
void
CChameleonCaption :: DrawText(Rect frame, Int16 inJust)
{
	UTextDrawing::DrawWithJustification ( (Ptr)&mText[1], mText[0], frame, inJust );	
	
}

#pragma mark -

CChameleonBroadcastCaption :: CChameleonBroadcastCaption ( LStream* inStream )
	: CChameleonCaption(inStream)
{
	*inStream >> mMessage;
	*inStream >> mRolloverTraits;
	
	mSavedTraits = GetTextTraitsID();		// cache original text traits
}


//
// ClickSelf
//
// Broadcast our message when clicked on
//
void
CChameleonBroadcastCaption :: ClickSelf ( const SMouseDownEvent & inEvent )
{
	if ( ! ::WaitMouseMoved(inEvent.macEvent.where) )
		BroadcastMessage ( mMessage );
	
} // ClickSelf


//
// MouseWithin
//
// If text traits are specified that are different from the default, set them
// when the mouse is within this view
//
void
CChameleonBroadcastCaption :: MouseWithin ( Point inWhere, const EventRecord & inEvent )
{
#if 0
	if ( GetTextTraitsID() != mRolloverTraits ) {
		SetTextTraitsID ( mRolloverTraits );	
		Draw(NULL);
	}
#endif

} // MouseWithin


//
// MouseLeave
//
// Restore the text traits to normal when the mouse leaves.
//
void
CChameleonBroadcastCaption :: MouseLeave ( )
{
#if 0
	if ( mSavedTraits != GetTextTraitsID() ) {
		SetTextTraitsID ( mSavedTraits );
		Draw(NULL);
	}
#endif	
} // MouseLeave
