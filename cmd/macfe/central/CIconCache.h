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
// CIconCache.h
// Mike Pinkerton, Netscape Communications
//
// Contains the implementation of the CImageCache class which is a small cache of
// images which the FE can pull from with a low overhead. It's good for things
// such as using images for icons in the chrome. Note that this is very
// different from the XP image cache.
//
// The filename is old, owing back to the original name for this class, CIconCache
//

#pragma once

#include <map>
#include <string>
#include "CIconContext.h"
#include "il_types.h"
#include "mimages.h"


//
// class ImageCacheData
//
// Contains all the relevant data for an image loaded from a url (not an FE icon). The
// context controls the loading and notification of when the image has been loaded
// and keeps copies of the FE pixmap data so when imageLib cleans house, we're still ok.
//
class ImageCacheData : public LBroadcaster {
public:
	ImageCacheData ( const string & inURL ) ;
	~ImageCacheData ( ) ;

		// Hold on tight to whatever we need to draw, making copies if necessary.
	void CacheThese ( IL_Pixmap* inImage, IL_Pixmap* inMask )  ;
	
	CIconContext* IconContext ( ) const { return mContext; } ;
	void IconContextHasGoneAway ( ) { mContext = NULL; } ;

	bool ImageAvailable ( ) { return mImage != NULL; } ;
	
	NS_PixMap* Image ( ) const { return mImage; }
	NS_PixMap* Mask ( ) const { return mMask; }
	
private:

		// declare these as private so no one can do them.
	ImageCacheData ( const ImageCacheData & ) ;
	ImageCacheData & operator= ( const ImageCacheData & ) ;
	
	NS_PixMap*	mImage;
	NS_PixMap*	mMask;

	CIconContext* mContext;

}; // struct ImageCacheData


//
// class CImageCache
//
// There should only be one of these in the entire application. Users make a request
// by asking the cache for a particular URL. If the URL is not loaded, a new icon context
// will be created and the caller will be placed on a list of listeners for when that
// image is ready to draw. If the image has already been loaded, the caller will either
// be placed on that listener list (for animating images) or be given the data directly
// (for static images).
//
class CImageCache : public LListener
{
public:
	enum ELoadResult { kDataPresent, kPutOnWaitingList, kEmptyURL } ;
	
	CImageCache ( ) ;
	virtual ~CImageCache ( ) ;
	
		// Make a request for an icon. Will start loading if not present. If an
		// empty string is passed in, this will return |kEmptyURL| and nothing
		// will be loaded.
	ELoadResult RequestIcon ( const string & inURL, const LListener* inClient ) ;
	
		// For images that are already loaded (RequestIcon() returned |kDataPresent|,
		// get the data.
	void FetchImageData ( const string & inURL, NS_PixMap** outImage, NS_PixMap** outMask ) ;
	
		// Housecleaning. When a static image is finished, the context should go away
		// but the data should remain.
	void ContextFinished ( const MWContext* inContext ) ;
	
		// a view might be going away before an image is finished loading. If that is
		// the case, it needs to be removed from the context's listener list so we don't
		// go telling dead objects their image has arrived.
	void ListenerGoingAway ( const string & inURL, LListener* inObjectGoingAway ) ;
	
private:

		// declare these private so no one can make a copy of the cache
	CImageCache ( const CImageCache & ) ;
	CImageCache & operator= ( const CImageCache & ) ;
	
	void ListenToMessage ( MessageT inMessage, void* inData ) ;
	
		// clean out the cache when it gets too big.
	void Flush() ;
	
	map<string, ImageCacheData*>	mCache;
	
}; // class CImageCache


// global declaration of our icon cache
CImageCache& gImageCache ( ) ;
