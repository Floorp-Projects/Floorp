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
// I apologize profusely for the poor design and amount of copied code
// from CButton. We had to do this is about a week from start to finish
// about a month _after_ the feature-complete deadline. If you don't like
// the code, deal (pinkerton).
//

#include "CRDFToolbarItem.h"
#include "CToolbarModeManager.h"
#include "UGraphicGizmos.h"
#include "UGAAppearance.h"
#include "URDFUtilities.h"


extern RDF_NCVocab gNavCenter;			// RDF vocab struct for NavCenter


CRDFToolbarItem :: CRDFToolbarItem ( HT_Resource inNode )
	: mNode(inNode)
{
	Assert_(mNode != NULL);

}


CRDFToolbarItem :: ~CRDFToolbarItem ( )
{

}



#pragma mark -


CRDFPushButton :: CRDFPushButton ( HT_Resource inNode )
	: CRDFToolbarItem(inNode), mTitleTraitsID(130), mCurrentMode(eTOOLBAR_TEXT_AND_ICONS),
		mMouseInFrame(false),  mTrackInside(false), mGraphicPadPixels(5), mTitlePadPixels(5),
		mOvalWidth(8), mOvalHeight(8)
{
	// figure out icon and text placement depending on the toolbarBitmapPosition property in HT. The
	// first param is the alignment when the icon is one top, the 2nd when the icon is on the side.
	mTitleAlignment = CalcAlignment(kAlignCenterBottom, kAlignCenterRight);
	mGraphicAlignment = CalcAlignment(kAlignCenterTop, kAlignCenterLeft);
}


CRDFPushButton :: ~CRDFPushButton ( )
{

}

//
// CalcAlignment
//
// Compute the FE alignment of a specific HT property. If HT specifies the icon is on top, use |inTopAlignment|,
// otherwise use |inSideAlignment|. This property is a view-level property so we need to pass in the top node
// of the view, not the node associated with this button. As a result, all buttons on a given toolbar MUST
// have the same alignment. You cannot have one button with the icon on top and another with the icons on the side
// in the same toolbar.
//
UInt32
CRDFPushButton :: CalcAlignment ( UInt32 inTopAlignment, Uint32 inSideAlignment ) const
{
	Uint32 retVal = inSideAlignment;
	char* value = NULL;
	PRBool success = HT_GetTemplateData ( HT_TopNode(HT_GetView(HTNode())), gNavCenter->toolbarBitmapPosition, HT_COLUMN_STRING, &value );
	if ( success && value ) {
		if ( strcmp(value, "top") == 0 )
			retVal = inTopAlignment;
	}
	
	return retVal;
} // CalcAlignment


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
// Precompute where the graphic should go.
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
	StColorPenState savedState;
	StColorPenState::Normalize();
	
	if (IsTrackInside() || GetValue() == Button_On)
		::OffsetRect(&mCachedTitleFrame, 1, 1);

	if ( IsMouseInFrame() )
		URDFUtilities::SetupForegroundTextColor ( HTNode(), gNavCenter->viewRolloverColor, kThemeIconLabelTextColor ) ;
	else
		URDFUtilities::SetupForegroundTextColor ( HTNode(), gNavCenter->viewFGColor, kThemeIconLabelTextColor ) ;

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
	PRBool success = HT_GetTemplateData ( HTNode(), gNavCenter->toolbarEnabledIcon, HT_COLUMN_STRING, &url );
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

	// Now draw GA button bevel, but only if HT hasn't specified a rollover color
	StColorPenState thePenSaver;
	thePenSaver.Normalize();
	if ( URDFUtilities::SetupForegroundColor(HTNode(), gNavCenter->viewRolloverColor, kThemeIconLabelTextColor) ) {
		::PenSize(2,2);
		::FrameRoundRect(&mCachedButtonFrame, mOvalWidth, mOvalHeight);
	}
	else
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

	// Draw face of button first
	while ( theLoop.NextDepth ( depth )) 
		if ( depth >= 4 )		// don't do anything for black and white
		{
			Rect frame = mCachedButtonFrame;
			::InsetRect(&frame, 1, 1);

			// Do we need to do this very slight darkening?
//			UGraphicGizmos::LowerRoundRectColorVolume(frame, 4, 4, UGAAppearance::sGASevenGrayLevels);
		}

	// Now draw GA pressed button bevel
	if ( URDFUtilities::SetupBackgroundColor(HTNode(), gNavCenter->viewPressedColor, kThemeIconLabelBackgroundBrush) ) {
		Rect frame = mCachedButtonFrame;
		::InsetRect(&frame, 1, 1);
		::EraseRect(&frame);
	}
	else
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
//		StClipRgnState savedClip(buttonRgnPort);	///еее grrr, this doesn't work
		
		GetSuperView()->Draw(NULL);
		Draw(NULL);
	}
}


//
// HotSpotAction
//
// Called when the mouse is clicked w/in this control
//
void
CRDFPushButton :: HotSpotAction(short /* inHotSpot */, Boolean inCurrInside, Boolean inPrevInside)
{
	if (inCurrInside != inPrevInside) {
		SetTrackInside(inCurrInside);
		Draw(NULL);
	}

} // HotSpotAction


#pragma mark -


CRDFSeparator :: CRDFSeparator ( HT_Resource inNode )
	: CRDFToolbarItem(inNode)
{


}


CRDFSeparator :: ~CRDFSeparator ( )
{



}


// a strawman drawing routine for testing purposes only
void
CRDFSeparator :: DrawSelf ( )
{
	Rect localRect;
	CalcLocalFrameRect ( localRect );

	::FrameRect ( &localRect );
}

#pragma mark -


CRDFURLBar :: CRDFURLBar ( HT_Resource inNode )
	: CRDFToolbarItem(inNode)
{


}


CRDFURLBar :: ~CRDFURLBar ( )
{



}

// a strawman drawing routine for testing purposes only
void
CRDFURLBar :: DrawSelf ( )
{
	Rect localRect;
	CalcLocalFrameRect ( localRect );

	::FrameRect ( &localRect );
}

