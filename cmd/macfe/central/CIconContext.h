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

#pragma once

#include <string>
#include "CNSContext.h"
#include "il_types.h"


//
// IconImageData
//
// Used to pass around the image and the mask from the context to
// anyone who happens to be listening.
//
struct IconImageData {
	IconImageData ( IL_Pixmap* inImage, IL_Pixmap* inMask, const string & inStr )
		: image(inImage), mask(inMask), url(inStr) { } ;
	
	const string & url;
	IL_Pixmap* image;
	IL_Pixmap* mask;
};


//
// CIconContext
//
// A context that knows about loading and displaying images for the purposes
// of replacing icons in the FE.
//
class CIconContext : public CNSContext
{
public:
	enum { msg_ImageReadyToDraw = 'ImgD' } ;	// data is IconImageData*
	
	CIconContext ( const string & inURL ) ;
	virtual ~CIconContext ( ) ;
	
	virtual void DrawImage ( IL_Pixmap* image, IL_Pixmap* mask ) ;
	
		// access to the url this context is loading
	const string & GetURL ( ) const { return mURL; }
	string & GetURL ( ) { return mURL; }
	
	virtual void RemoveAllListeners ( ) ;
	
private:

	IL_GroupContext *mImageGroupContext;
	string mURL;
	
}; // class CIconContext