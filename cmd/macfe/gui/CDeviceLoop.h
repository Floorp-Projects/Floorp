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

#ifndef __H_CDeviceLoop
#define __H_CDeviceLoop
#pragma once

/*======================================================================================

	DESCRIPTION:	Implements an optimized stack-based device loop object.
					
					Example:

						Int16 depth;
						TDeviceLoop theLoop(frame, depth);
					
						if ( depth ) do {
							switch ( depth ) {
			
								case 1:		// Black & white
									break;
			
								case 4:		// 16 colors
									break;
			
								case 8:		// 256 colors
									break;
			
								case 16:	// Thousands of colors
									break;
				
								case 32:	// Millions of colors
									break;
							}
						} while ( theLoop.NextDepth(depth) );
					
	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -

class CDeviceLoop {

public:

					CDeviceLoop(const Rect &inLocalRect, Int16 &outFirstDepth);
					~CDeviceLoop(void);

	Boolean			NextDepth(Int16 &outNextDepth);

protected:

	void			ClipToIntersection(const Rect &inLocalRect);
	
	// Instance variables

	Rect			mGlobalRect;
	GDHandle		mCurrentDevice;
	RgnHandle		mSaveClip;

};

#endif // __H_CDeviceLoop

