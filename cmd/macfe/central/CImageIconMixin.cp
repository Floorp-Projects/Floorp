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
// CImageIconMixin
// Mike Pinkerton, Netscape Communications
//
// When you have an object that wants to draw itself using an image (gif, jpeg, etc),
// just mix this class in and call the appropriate routines when you want to draw.
// Most importantly, this handles cleaning up when the object goes away if it has
// made a request for an image that has yet to arrive.
//


#include "CImageIconMixin.h"


CImageIconMixin :: CImageIconMixin ( const string & inURL )
	: mURL(inURL)
{

}


//
// destructor
//
// If we made a request to load an image and we have yet to display it, we get a
// message when the image is ready to draw. However, if we're going away, it would
// be bad for us to get that message at some point in the future. Make sure we
// are off the listener list for the image cache.
// 
CImageIconMixin :: ~CImageIconMixin ( )
{
	gImageCache().ListenerGoingAway ( mURL, this );
	
}


//
// ListenToMessage
//
// Listen for the update of image and then 
//
void
CImageIconMixin :: ListenToMessage ( const MessageT inMessage, void* ioData )
{
	if ( inMessage == CIconContext::msg_ImageReadyToDraw )
		ImageIsReady();
		
} // ListenToMessage


//
// DrawIcon
//	
// Draw the image. Will begin loading it if the data has not yet arrived and will
// draw the standby in its place
//
void 
CImageIconMixin :: DrawImage ( const Point & inTopLeft, IconTransformType inTransform, 
								Uint32 inWidth, Uint32 inHeight ) const
{
	try { 
		if ( gImageCache().RequestIcon(mURL, this) == CImageCache::kDataPresent ) {
			StColorState::Normalize();

			DrawingState state;
			state.copyMode = srcCopy;
			gImageCache().FetchImageData ( mURL, &state.pixmap, &state.mask );

			LockFEPixmapBuffer ( state.pixmap ) ;
			LockFEPixmapBuffer ( state.mask );
//ее do appropriate things for transform
			DoDrawing ( state, inTopLeft, inTransform, inWidth, inHeight ) ;
			UnlockFEPixmapBuffer ( state.mask );
			UnlockFEPixmapBuffer ( state.pixmap ) ;
		}
		else {
			// icon not ready yet, just draw this thing....
			DrawStandby ( inTopLeft, inTransform ) ;
		}
	}
	catch ( invalid_argument & ia ) {
		DebugStr("\pData requested for something not in cache");
		DrawStandby ( inTopLeft, inTransform ) ;
	}
	catch ( bad_alloc & ba ) {
		DebugStr("\pCould not allocate memory drawing icon");
		DrawStandby ( inTopLeft, inTransform ) ;
	}

} // DrawIcon


void
CImageIconMixin :: DoDrawing ( DrawingState & inState, const Point & inTopLeft,
								IconTransformType inTransform, Uint32 inWidth, Uint32 inHeight ) const
{
	if ( inWidth == 0 && inHeight == 0 ) {
		inWidth = inState.pixmap->pixmap.bounds.right;
		inHeight = inState.pixmap->pixmap.bounds.bottom;
	}
	DrawScaledImage ( &inState, inTopLeft, 0, 0, inWidth, inHeight );

} // DoDrawing


//
// SetImageURL
//
// Change the url of the image to draw. We might be in the middle of loading another
// image when we get this call, so unregister ourselves. This does NOT re-request
// the new icon.
//
void
CImageIconMixin :: SetImageURL ( const string & inNewURL )
{
	gImageCache().ListenerGoingAway ( mURL, this );
	mURL = inNewURL;
	
} // SetImageURL


//
// GetImageDimensions
//
// If the image has been loaded, this returns true and the dimensions of the
// image in |outDimensions|
//
bool
CImageIconMixin :: GetImageDimensions ( SDimension16 & outDimensions )
{
	bool imagePresent = false;
	
	try { 
		if ( gImageCache().RequestIcon(mURL, this) == CImageCache::kDataPresent ) {

			DrawingState state;
			state.copyMode = srcCopy;
			gImageCache().FetchImageData ( mURL, &state.pixmap, &state.mask );
			
			outDimensions.width = state.pixmap->pixmap.bounds.right;
			outDimensions.height = state.pixmap->pixmap.bounds.bottom;
			
			imagePresent = true;
		}
	}
	catch ( invalid_argument & ia ) {
		DebugStr("\pData requested for something not in cache");
	}
	
	return imagePresent;
	
} // GetImageDimensions


#pragma mark -


CTiledImageMixin :: CTiledImageMixin ( const string & inURL )
	: CImageIconMixin ( inURL )
{
}


CTiledImageMixin :: ~CTiledImageMixin ( ) 
{
}


//
// DoDrawing
//
// Overloaded to draw a image as a tiled image for drawing backgrounds
//
void
CTiledImageMixin :: DoDrawing ( DrawingState & inState, const Point & inTopLeft,
								IconTransformType inTransform, Uint32 inWidth, Uint32 inHeight ) const
{
	DrawTiledImage ( &inState, inTopLeft, 0, 0, inWidth, inHeight );

} // DoDrawing

