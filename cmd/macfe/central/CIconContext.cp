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

#include "CIconContext.h"
#include "mimages.h"


CIconContext :: CIconContext ( const string & inURL ) 
	: CNSContext ( MWContextIcon ), mURL(inURL)
{
	CreateImageContext( &mContext );
	mImageGroupContext = mContext.img_cx;
	IL_AddGroupObserver(mImageGroupContext, ImageGroupObserver, &mContext);

}


CIconContext :: ~CIconContext ( )
{
	DestroyImageContext ( &mContext );

}


//
// DrawImage
//
// Let anyone who cares about this image know that we're ready for it to draw
//
void
CIconContext :: DrawImage ( IL_Pixmap* image, IL_Pixmap* mask )
{
	IconImageData data ( image, mask, GetURL() );
	BroadcastMessage ( msg_ImageReadyToDraw, &data );
}


//
// RemoveAllListeners
//
// Go through and get rid of everyone listening to us because there's no need for anyone
// to listen.
//
void
CIconContext :: RemoveAllListeners ( )
{
	TArrayIterator<LListener*> iterator(mListeners);
	LListener *theListener;
	while (iterator.Next(theListener))
		RemoveListener(theListener);		

} // RemoveAllListeners
