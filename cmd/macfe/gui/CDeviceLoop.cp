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


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CDeviceLoop.h"


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -

/*======================================================================================
	Constructor.
======================================================================================*/

CDeviceLoop::CDeviceLoop(const Rect &inLocalRect, Int16 &outFirstDepth) {

	mSaveClip = nil;
	mGlobalRect = inLocalRect;			// Convert to Global coords
	::LocalToGlobal(&topLeft(mGlobalRect));
	::LocalToGlobal(&botRight(mGlobalRect));
	mCurrentDevice = nil;
	
	outFirstDepth = 0;
	NextDepth(outFirstDepth);
}
	

/*======================================================================================
	Destructor.
======================================================================================*/

CDeviceLoop::~CDeviceLoop(void) {

	if ( mSaveClip != nil ) {				// Restore clipping region
		::SetClip(mSaveClip);
		::DisposeRgn(mSaveClip);
		mSaveClip = nil;
	}
}


/*======================================================================================
	Get the next drawing depth, return false if none.
======================================================================================*/

static Boolean RectEnclosesRect(const Rect *inEnclosingRect, const Rect *inTestRect) {

	return ((inEnclosingRect->top <= inTestRect->top) && 
			(inEnclosingRect->left <= inTestRect->left) &&
			(inEnclosingRect->right >= inTestRect->right) &&
			(inEnclosingRect->bottom >= inTestRect->bottom));
}

Boolean CDeviceLoop::NextDepth(Int16 &outNextDepth) {

	if ( mCurrentDevice ) {
		if ( !mSaveClip ) {
			// The first device we found that contained any portion of the specified
			// rectangle actually contained ALL of the rectangle, so exit here.
			return false;
		}
		mCurrentDevice = GetNextDevice(mCurrentDevice);
	} else {
		mCurrentDevice = GetDeviceList();
	}

	// Locate the first device for processing
	
	while ( mCurrentDevice ) {
	
		if ( ::TestDeviceAttribute(mCurrentDevice, screenDevice) &&
			 ::TestDeviceAttribute(mCurrentDevice, screenActive) ) {
			
			Rect deviceRect = (**mCurrentDevice).gdRect, intersection;
			
			if ( ::SectRect(&mGlobalRect, &deviceRect, &intersection) ) {
			
				// Some portion of the device rect encloses the specified rect
			
				outNextDepth = (**((**mCurrentDevice).gdPMap)).pixelSize;

				if ( !mSaveClip ) {
					if ( RectEnclosesRect(&deviceRect, &mGlobalRect) ) {
						// Specified rectangle is completely enclosed within this device
						// DON'T create a clipping region and exit here
						break;
					} else {
						mSaveClip = ::NewRgn();				// Save clipping region
						if ( mSaveClip ) {
							::GetClip(mSaveClip);
						}
					}
				}
				
				// Set clipping region to the intersection of the target
				// rectangle, the screen rectangle, and the original
				// clipping region

				::GlobalToLocal(&topLeft(intersection));
				::GlobalToLocal(&botRight(intersection));
				ClipToIntersection(intersection);
				break;				// Exit device loop
			}
		}
		
		mCurrentDevice = ::GetNextDevice(mCurrentDevice);
	}
	
	return (mCurrentDevice != nil);
}
	

/*======================================================================================
	Draw the control.
======================================================================================*/

void CDeviceLoop::ClipToIntersection(const Rect &inLocalRect) {

	if ( mSaveClip ) {
		RgnHandle overlap = ::NewRgn();
		::RectRgn(overlap, &inLocalRect);
		::SectRgn(mSaveClip, overlap, overlap);
		::SetClip(overlap);
		::DisposeRgn(overlap);
	}
}

