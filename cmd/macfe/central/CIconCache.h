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

#pragma once

#include <map>
#include <string>
#include "CIconContext.h"
#include "il_types.h"
#include "mimages.h"


//
// class IconCacheData
//
// Contains all the relevant data for an icon loaded from a url (not an FE icon). The
// icon context controls the loading and notification of when the image has been loaded
// and keeps copies of the FE pixmap data so when imageLib cleans house, we're still ok.
//
class IconCacheData : public LBroadcaster {
public:
	IconCacheData ( const string & inURL ) ;
	~IconCacheData ( ) ;

		// Hold on tight to whatever we need to draw, making copies if necessary.
	void CacheThese ( IL_Pixmap* inImage, IL_Pixmap* inMask )  ;
	
	CIconContext* IconContext ( ) const { return mContext; } ;
	void IconContextHasGoneAway ( ) { mContext = NULL; } ;

	bool ImageAvailable ( ) { return mImage != NULL; } ;
	
	NS_PixMap* Image ( ) const { return mImage; }
	NS_PixMap* Mask ( ) const { return mMask; }
	
private:

	NS_PixMap*	mImage;
	NS_PixMap*	mMask;

	CIconContext* mContext;

}; // struct IconCacheData


//
// class CIconCache
//
// There should only be one of these in the entire application. Users make a request
// by asking the cache for a particular URL. If the URL is not loaded, a new icon context
// will be created and the caller will be placed on a list of listeners for when that
// image is ready to draw. If the image has already been loaded, the caller will either
// be placed on that listener list (for animating images) or be given the data directly
// (for static images).
//
class CIconCache : public LListener
{
public:
	enum ELoadResult { kDataPresent, kPutOnWaitingList } ;
	
	CIconCache ( ) ;
	virtual ~CIconCache ( ) ;
	
		// Make a request for an icon. Will start loading if not present.
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

	void ListenToMessage ( MessageT inMessage, void* inData ) ;
	
		// clean out the cache when it gets too big.
	void Flush() ;
	
	map<string, IconCacheData*>	mCache;
	
}; // class CIconCache


// global declaration of our icon cache
extern CIconCache gIconCache;