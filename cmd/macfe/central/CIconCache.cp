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
// CIconCache.cp
// Mike Pinkerton, Netscape Communications
//
// To Do:
//	е Flush cache when it gets too full?
//


#include "CIconCache.h"
#include "net.h"
#include "mimages.h"
#include "proto.h"
#include "libevent.h"


CIconCache gIconCache;		// global storage definition


IconCacheData :: IconCacheData ( const string & inURL )
	: mImage(NULL), mMask(NULL)
{
	mContext = new CIconContext ( inURL );
	if ( !mContext )
		throw bad_alloc();
}

IconCacheData :: ~IconCacheData ( ) 
{
	DestroyFEPixmap ( mImage );
	DestroyFEPixmap ( mMask );
	delete mContext;
}


//
// CacheThese
//
// Copy whatever FE data (pixmaps, color tables, etc) we need to ensure we can still draw when
// imageLib cleans out the memory cache. Before that, however, be sure to tear down anything
// that already exists.
//
void 
IconCacheData :: CacheThese ( IL_Pixmap* inImage, IL_Pixmap* inMask ) 
{
	// clean up what we allocate beforehand.
	DestroyFEPixmap ( mImage );
	DestroyFEPixmap ( mMask );
	
	DrawingState state;
	StColorPenState::Normalize();
	PreparePixmapForDrawing ( inImage, inMask, true, &state );

	// we use some trickery here to create the copy FE data structures for the image/mask pixmaps.
	// Save the ones that are currently in use then call the callback which initially created them
	// to go ahead and create new ones. Store the |client_data| fields internally (they are our
	// new copies), and then put the old ones back leaving the image/mask untouched.
	NS_PixMap* oldPixmap = mImage = NULL;
	NS_PixMap* oldMask = mMask = NULL;
	size_t imageWidth = 0, imageHeight = 0;
	if ( inImage ) {
		oldPixmap = static_cast<NS_PixMap*>(inImage->client_data);
		imageWidth = inImage->header.width;
		imageHeight =  inImage->header.height;
	}
	if ( inMask )
		oldMask = static_cast<NS_PixMap*>(inMask->client_data);
	_IMGCB_NewPixmap ( NULL, 0, *IconContext(), imageWidth, imageHeight, inImage, inMask );
	if ( inImage ) {
		mImage = static_cast<NS_PixMap*>(inImage->client_data);
		size_t imageSize = inImage->header.widthBytes * inImage->header.height;
		::BlockMoveData ( oldPixmap->image_buffer, mImage->image_buffer, imageSize );
		inImage->client_data = oldPixmap;
	}
	if ( inMask ) {
		mMask = static_cast<NS_PixMap*>(inMask->client_data);
		size_t maskSize = inMask->header.widthBytes * inMask->header.height;
		::BlockMoveData ( oldMask->image_buffer, mMask->image_buffer, maskSize );
		inMask->client_data = oldMask;
	}
	
	DoneDrawingPixmap ( inImage, inMask, &state );

} // CacheThese


#pragma mark -


CIconCache :: CIconCache ( ) 
{

}


CIconCache :: ~CIconCache ( )
{
	//е need to dispose of all the images....but since this is only called at shutdown....
}


//
// RequestIcon
// THROWS bad_alloc
//
// Make a request for an icon. Will start loading if not present, and the return result 
// will be |kPutOnWaitingList|. If the data is already there in the cache, |kDataPresent|
// is returned.
//
CIconCache::ELoadResult
CIconCache :: RequestIcon ( const string & inURL, const LListener* inClient )
{
	ELoadResult result;
	
	IconCacheData* data = mCache[inURL];
	if ( data ) {
		if ( !data->ImageAvailable() && data->IconContext() ) {
			// We're still loading. The cache is already on the notification list, so just
			// add the caller.
			data->AddListener ( const_cast<LListener*>(inClient) );
			result = kPutOnWaitingList;
		}
		else
			result = kDataPresent;
	}
	else {
		// not in cache yet, need to start loading this icon. Add the caller as a 
		// listener to the data class and add us a listener to the context. 
		data = new IconCacheData(inURL);
		if ( !data )
			throw bad_alloc();
		URL_Struct* urlStruct = NET_CreateURLStruct (inURL.c_str(), NET_DONT_RELOAD);
		if ( !urlStruct )
			throw bad_alloc();
		
		mCache[inURL] = data;

		data->IconContext()->AddListener ( this );
		data->AddListener ( const_cast<LListener*>(inClient) );
		data->IconContext()->SwitchLoadURL ( urlStruct, FO_PRESENT );
		
		result = kPutOnWaitingList;
	}
	
	return result;
	
} // RequestIcon


// 
// FetchImageData
// THROWS invalid_argument
//
// For images that are already loaded (RequestIcon() returned |kDataPresent|, get the data.
//
void
CIconCache :: FetchImageData ( const string & inURL, NS_PixMap** outImage, NS_PixMap** outMask )
{
	IconCacheData* data = mCache[inURL];
	if ( data ) {
		*outImage = data->Image();
		*outMask = data->Mask();
	}
	else
		throw invalid_argument(inURL);

} // FetchImageData


//
// DeferredDelete
//
// A mocha callback, called when mocha is done with the given context. We use this below to
// defer the deletion of an icon context. When we get here, we know imageLib is done with it
// so it is safe to delete it.
//
static void DeferredDelete ( CNSContext * context )
{
	context->NoMoreUsers();

} // DeferredDelete


//
// ContextFinished
//
// Housecleaning. When a static image is finished, the context should go away
// but the data should remain.
//
void
CIconCache :: ContextFinished ( const MWContext* inContext )
{
	// find the context in the cache (we don't have the url). Remember the cache
	// contains CIconContext's and we are given an MWContext. Easy enough, though,
	// because the MWContext contains a pointer back to the FE class in |fe.newContext|
	typedef map<string, IconCacheData*>::const_iterator CI;
	CI it = mCache.begin();
	for ( ; it != mCache.end(); ++it ) {
		CIconContext* currContext = (*it).second->IconContext();
		if ( currContext == inContext->fe.newContext )
			break;
	}
//	Assert_(it != mCache.end());
	
	// null out the icon context stored w/in it. Don't delete it because it is
	// still being used by imageLib???
	if ( it != mCache.end() ) {
		(*it).second->IconContext()->RemoveAllListeners();
		
		// We cannot delete the context now, because imageLib is still in the middle of
		// using it. To get around this, we tell mocha to call us back when it is done with the
		// context, which should be about the time of the next event loop. By then, all is well
		// and the DeferredDelete() routine above will delete the context correctly.
		XP_RemoveContextFromList ( *(*it).second->IconContext() );
		ET_RemoveWindowContext( *(*it).second->IconContext(), (ETVoidPtrFunc) DeferredDelete, (*it).second->IconContext() );

		(*it).second->IconContextHasGoneAway();
	}
}



//
// ListenerGoingAway
//
// a view might be going away before an image is finished loading. If that is
// the case, it needs to be removed from the context's listener list so we don't
// go telling dead objects their image has arrived.
//
void
CIconCache :: ListenerGoingAway ( const string & inURL, LListener* inObjectGoingAway )
{
	IconCacheData* data = mCache[inURL];
	if ( data && data->IconContext() )
		data->IconContext()->RemoveListener ( inObjectGoingAway );
	
} // ListenerGoingAway


//
// Flush
//
// clean out the cache when it gets too big.
//
void
CIconCache :: Flush()
{

}


//
// ListenToMessage
//
// Listen to messages from the icon context so we can display the image when imageLib gives us
// something to display. We have to make sure that we deep copy the FE portions of the image
// and mask so that when imagelib cleans out the memory cache, we still have the data we need
// to draw things.
//
void
CIconCache :: ListenToMessage ( MessageT inMessage, void* inData )
{
	switch ( inMessage ) {
		case CIconContext::msg_ImageReadyToDraw:
			// NOTE: don't dispose |data| because it is on stack
			IconImageData* data = static_cast<IconImageData*>(inData);	
			Assert_(data != NULL);
			if ( data ) {
				try {
					IconCacheData* iconData = mCache[data->url];
					if ( iconData ) {
						// remember whatever we need and tell clients to draw
						iconData->CacheThese ( data->image, data->mask );
						iconData->BroadcastMessage ( CIconContext::msg_ImageReadyToDraw, NULL );
					}
				}
				catch ( bad_alloc & ) {
					//еее couldn't allocate memory for item in cache, so don't cache it. Do we
					//еее need to tell anyone about this (unregister listeners, perhaps?)
				}
			}			
			break;		
	}
	
} // ListenToMessage

