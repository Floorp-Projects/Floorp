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
// Mike Pinkerton, Netscape Communications
//
// Implementations for all the things that can go on a toolbar.
//
// CRDFToolbarItem - the base class for things that go on toolbars
// CRDFPushButton - a toolbar button
// CRDFSeparator - a separator bar
// CRDFURLBar - the url bar w/ proxy icon
//

#include "CRDFToolbarItem.h"
#include "CToolbarModeManager.h"
#include "UGraphicGizmos.h"
#include "UGAAppearance.h"


extern RDF_NCVocab gNavCenter;			// RDF vocab struct for NavCenter


CRDFToolbarItem :: CRDFToolbarItem ( HT_Resource inNode )
	: mNode(inNode)
{
	Assert_(mNode != NULL);


}


CRDFToolbarItem :: ~CRDFToolbarItem ( )
{



}


// a strawman drawing routine for testing purposes only
void
CRDFToolbarItem :: DrawSelf ( )
{
	Rect localRect;
	CalcLocalFrameRect ( localRect );

	::FrameRect ( &localRect );
}


#pragma mark -


CRDFPushButton :: CRDFPushButton ( HT_Resource inNode )
	: CRDFToolbarItem(inNode), mTitleTraitsID(130), mCurrentMode(eTOOLBAR_TEXT_AND_ICONS),
		mMouseInFrame(false),  mTrackInside(false), mGraphicPadPixels(5), mTitlePadPixels(5),
		mTitleAlignment(kAlignCenterBottom), mGraphicAlignment(kAlignCenterTop),
		mOvalWidth(8), mOvalHeight(8)
{
	DebugStr("\pCreating a CRDFPushButton");

}


CRDFPushButton :: ~CRDFPushButton ( )
{



}


//
// CalcTitleFrame
//
// This calculates the bounding box of the title (if any).  This is useful
// for both the string placement, as well as position the button graphic
// (again, if any).
//
// Note that this routine sets the text traits for the ensuing draw.  If
// you override this method, make sure that you're doing the same.
//
void 
CRDFPushButton :: CalcTitleFrame ( )
{
	char* title = HT_GetNodeName(HTNode());
	if ( !title || !*title )
		return;

	UTextTraits::SetPortTextTraits(mTitleTraitsID);

	FontInfo theInfo;
	::GetFontInfo(&theInfo);
	mCachedTitleFrame.top = mCachedButtonFrame.top;
	mCachedTitleFrame.left = mCachedButtonFrame.left;		
	mCachedTitleFrame.right = mCachedTitleFrame.left + ::StringWidth(LStr255(title));;
	mCachedTitleFrame.bottom = mCachedTitleFrame.top + theInfo.ascent + theInfo.descent + theInfo.leading;;

	if (mCurrentMode != eTOOLBAR_TEXT)
	{
		UGraphicGizmos::AlignRectOnRect(mCachedTitleFrame, mCachedButtonFrame, mTitleAlignment);
		UGraphicGizmos::PadAlignedRect(mCachedTitleFrame, mTitlePadPixels, mTitleAlignment);
	}
	else
	{
		UGraphicGizmos::CenterRectOnRect(mCachedTitleFrame, mCachedButtonFrame);
	}
}


//
// CalcGraphicFrame
//
//
void
CRDFPushButton :: CalcGraphicFrame ( )
{
	// The container for the graphic starts out as the whole
	// button area.
	Rect theContainerFrame = mCachedButtonFrame;
	Rect theImageFrame = theContainerFrame;
	
	UGraphicGizmos::AlignRectOnRect(theImageFrame, theContainerFrame, mGraphicAlignment);
	UGraphicGizmos::PadAlignedRect(theImageFrame, mGraphicPadPixels, mGraphicAlignment);
	mCachedGraphicFrame = theImageFrame;
}

//
// PrepareDrawButton
//
// Sets up frames and masks before we draw
//
void 
CRDFPushButton :: PrepareDrawButton( )
{
	CalcLocalFrameRect(mCachedButtonFrame);

	// Calculate the drawing mask region.
	::OpenRgn();
	::FrameRoundRect(&mCachedButtonFrame, mOvalWidth, mOvalHeight);
	::CloseRgn(mButtonMask);
	
	CalcTitleFrame();
	CalcGraphicFrame();
}


//
// FinalizeDrawButton
//
// Called after we are done drawing.
//
void
CRDFPushButton :: FinalizeDrawButton ( )
{
	// nothing for now.
}


void 
CRDFPushButton::DrawSelf()
{
	PrepareDrawButton();

	DrawButtonContent();

	const char* title = HT_GetNodeName(HTNode());
	if ( title ) {
		if (mCurrentMode != eTOOLBAR_ICONS && *title != 0)
			DrawButtonTitle();
	}
	
	if (mCurrentMode != eTOOLBAR_TEXT)
		DrawButtonGraphic();
			
	if (!IsEnabled() || !IsActive())
		DrawSelfDisabled();
			
	FinalizeDrawButton();
}


//
// DrawButtonContent
//
// Handle drawing things other than the graphic or title. For instance, if the
// mouse is within this frame, draw a border around the button. If the mouse is
// down, draw the button to look depressed.
//
void
CRDFPushButton :: DrawButtonContent ( ) 
{
	if (IsActive() && IsEnabled()) {
		if ( IsTrackInside() )
			DrawButtonHilited();
		else if (IsMouseInFrame())
			DrawButtonOutline();
	}

} // DrawButtonContent


//
// DrawButtonTitle
//
// Draw the title of the button.
//
void
CRDFPushButton :: DrawButtonTitle ( )
{
	StColorPenState::Normalize();
	
	if (IsTrackInside() || GetValue() == Button_On)
		::OffsetRect(&mCachedTitleFrame, 1, 1);

	char* title = HT_GetNodeName(HTNode());
	UGraphicGizmos::PlaceStringInRect(LStr255(title), mCachedTitleFrame, teCenter, teCenter);

} // DrawButtonTitle


//
// DrawButtonGraphic
//
// Draw the image specified by HT.
// еееIf there isn't an image, what should we do
//
void
CRDFPushButton :: DrawButtonGraphic ( )
{
	char* url = NULL;
	PRBool success = HT_GetTemplateData ( HTNode(), gNavCenter->RDF_largeIcon, HT_COLUMN_STRING, &url );
	if ( success && url ) {

		// setup where we should draw
		Point topLeft;
		topLeft.h = mCachedGraphicFrame.left; topLeft.v = mCachedGraphicFrame.top;
		uint16 width = mCachedGraphicFrame.right - mCachedGraphicFrame.left;
		uint16 height = mCachedGraphicFrame.bottom - mCachedGraphicFrame.top;
		
		// draw
		SetImageURL ( url );
		DrawImage ( topLeft, kTransformNone, width, height );
	}
	
} // DrawButtonGraphic

void
CRDFPushButton :: DrawSelfDisabled ( )
{
	// ??

} // DrawSelfDisabled


//
// DrawButtonOutline
//
// Draw the frame around the button to show rollover feedback
//
void
CRDFPushButton :: DrawButtonOutline ( )
{
	// е Setup a device loop so that we can handle drawing at the correct bit depth
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

} // DrawButtonOutline


//
// DrawButtonHilited
//
// Draw the button as if it has been clicked on, drawing the insides depressed
//
void
CRDFPushButton :: DrawButtonHilited ( )
{
	// е Setup a device loop so that we can handle drawing at the correct bit depth
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


//
// ImageIsReady
//
// Called when the icon is ready to draw so we can refresh accordingly.
//
void
CRDFPushButton :: ImageIsReady ( )
{
	Refresh();	// for now.

} // ImageIsReady


//
// DrawStandby
//
// Called when the image we want to draw has not finished loading. We get
// called to draw something in its place. Any good ideas?
void
CRDFPushButton :: DrawStandby ( const Point & inTopLeft, IconTransformType inTransform ) const
{
	::FrameRect ( &mCachedGraphicFrame );		//еее this is wrong!

} // DrawStandby


void 
CRDFPushButton :: MouseEnter ( Point /*inPortPt*/, const EventRecord & /*inMacEvent*/ )
{
	mMouseInFrame = true;
	if (IsActive() && IsEnabled())
		Draw(NULL);
}


void 
CRDFPushButton :: MouseWithin ( Point /*inPortPt*/, const EventRecord & /*inMacEvent*/ )
{
	// Nothing to do for now
}


void 
CRDFPushButton :: MouseLeave( )
{
	mMouseInFrame = false;
	if (IsActive() && IsEnabled()) {
		// since we can't simply draw the border w/ xor, we need to get the toolbar
		// to redraw its bg and then redraw the normal button over it. To do this
		// we create a rgn (in port coords), make it the clip rgn, and then draw
		// the parent toolbar and this button again.
		Rect portRect;
		CalcPortFrameRect(portRect);
		StRegion buttonRgnPort(portRect);
		StClipRgnState savedClip(buttonRgnPort);
		
		GetSuperView()->Draw(NULL);
		Draw(NULL);
	}
}


#pragma mark -


CRDFSeparator :: CRDFSeparator ( HT_Resource inNode )
	: CRDFToolbarItem(inNode)
{
	DebugStr("\pCreating a CRDFSeparator");


}


CRDFSeparator :: ~CRDFSeparator ( )
{



}

#pragma mark -


CRDFURLBar :: CRDFURLBar ( HT_Resource inNode )
	: CRDFToolbarItem(inNode)
{
	DebugStr("\pCreating a CRDFURLBar");


}


CRDFURLBar :: ~CRDFURLBar ( )
{



}
