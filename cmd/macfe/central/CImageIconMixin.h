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

#pragma once

#include <string>

#include "CIconCache.h"
#include "mimages.h"


//
// class CImageIconMixin
//
// Draw an image, most likely as an icon.
//
class CImageIconMixin : public LListener
{
public:
	
	CImageIconMixin ( ) { } ;
	CImageIconMixin ( const string & inURL ) ;
	virtual ~CImageIconMixin ( ) ;		
	
		// Change the url telling us which image to load. Safe to call even when an image
		// load is pending, but will NOT re-request the new image.
	virtual void SetImageURL ( const string & inNewURL ) ;
	
		// Draw the image. Will begin loading it if the data has not yet arrived and will
		// draw the standby in its place. Pass zero's for width and height to use image defaults
		// or the image will be scaled to what you put in.
	void DrawImage ( const Point & inTopLeft, IconTransformType inTransform,
						Uint32 inWidth, Uint32 inHeight ) const;
	
		// If the image has been loaded, this returns true and the dimensions of the
		// image in |outDimensions|
	bool GetImageDimensions ( SDimension16 & outDimensions ) ;
	
protected:

		// catch the message that the image is ready to draw
	virtual void ListenToMessage ( MessageT inMessage, void* ioPtr ) ;
	
		// called when the image is ready to draw. This should do something like
		// refresh the view.
	virtual void ImageIsReady ( ) = 0;
	
		// Draw a scaled image as an icon. Override to draw differently
	virtual void DoDrawing ( DrawingState & inState, const Point & inTopLeft,
								IconTransformType inTransform, Uint32 inWidth,
								Uint32 inHeight ) const ;
	
		// Draw something when the real image is not yet here.
	virtual void DrawStandby ( const Point & inTopLeft, IconTransformType inTransform ) const = 0;

private:

	string mURL;
	
}; // class CImageIconMixin


//
// class CTiledImageMixin
//
// Tile an image, for use as backgrounds in chrome or tables
//
class CTiledImageMixin : public CImageIconMixin
{
public:

	CTiledImageMixin ( ) { };
	CTiledImageMixin ( const string & inURL ) ;
	virtual ~CTiledImageMixin ( ) ;

protected:

		// Draw a image as a tiled image for drawing backgrounds. Width and Height are
		// reinterpreted to mean the width and height of the area the image is being tiled
		// in.
	virtual void DoDrawing ( DrawingState & inState, const Point & inTopLeft,
								IconTransformType inTransform, Uint32 inWidth,
								Uint32 inHeight ) const ;

}; // class CTiledImageMixin
