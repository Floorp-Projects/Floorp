/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsCoord.h"

// this class is used to gather caret properties from the OS. It
// must be implemented by each platform that wants more than
// the generic caret properties.

class nsCaretProperties
{
	
	public:
	
												nsCaretProperties();
		virtual							~nsCaretProperties() {}
	
		virtual nscoord			GetCaretWidth() 		{ return mCaretWidth; }
		virtual PRUint32		GetCaretBlinkRate()	{ return mBlinkRate; }
		
		
	protected:

	// have value for no blinking
	
		enum {
			eDefaulBlinkRate					= 500,		// twice a second
			eDetaultCaretWidthTwips		= 20			// one pixel wide
		};


		nscoord					mCaretWidth;			// caret width in twips
		PRUint32				mBlinkRate;				// blink rate in milliseconds
		
		
		// members for vertical placement & size?

};

NS_BASE nsCaretProperties* NewCaretProperties();
