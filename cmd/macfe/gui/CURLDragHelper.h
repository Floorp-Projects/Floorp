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
// Mike Pinkerton
// Netscape Communications
//
// A collection of classes that help out when implementing drag and drop. CURLDragHelper
// is just a bunch of static utility routines. CURLDragMixin (and its subclasses) are
// the real meat of this file, implementing most of the work of pulling information
// out of the drag manager for the 4 or so most common drag flavors encountered when
// dragging an icon around in Navigator.
//
// To use the CURLDragMixin classes, just mix them into the class that deals with d&d,
// overload PP's ReceiveDragItem() to call the one in CURLDragMixin and override the
// functions that actually do the work for your class:
//		virtual void HandleDropOfPageProxy ( const char* inURL, const char* inTitle ) = 0;
//		virtual void HandleDropOfLocalFile ( const char* inFileURL, const char* fileName ) = 0;
//		virtual void HandleDropOfText ( const char* inTextData ) = 0;
//
// CHTAwareURLDragMixin adds the ability to understand drags from HT (Aurora) and adds
// one more method to override
//		virtual void HandleDropOfHTResource ( HT_Resource node ) = 0;
//

#pragma once

#include <vector.h>
#include <string>
#include "htrdf.h"


//
// class CURLDragHelper
//
// Interface to a class that contains code common to classes that have to do drag and
// drop for items w/ urls (bookmarks, etc). Nothing really out of the ordinary here.
//

class CURLDragHelper
{
public:

	// call after a drop. Handles sending the data or creating files from flavors
	// registered for url-ish items
	static void DoDragSendData ( const char* inURL, char* inTitle, FlavorType inFlavor,
									ItemReference inItemRef, DragReference inDragRef) ;

	// Make sure the caption for an icon isn't too long so that it leaves turds.
	// handles middle truncation, etc if title is too long
	static string MakeIconTextValid ( const char* inTitle );

}; // CURLDragHelper



//
// class CURLDragMixin
//
// A mixin class for implementing drag and drop for the common drag flavors encountered
// when the user drags around something with a URL.
//

class CURLDragMixin
{
public:

	CURLDragMixin ( ) ;
	virtual ~CURLDragMixin ( ) { } ;
	
protected:

	virtual void ReceiveDragItem ( DragReference inDragRef, DragAttributes /*inDragAttrs*/,
											ItemReference inItemRef, Rect & /*inItemBounds*/ ) ;
	virtual bool FindBestFlavor ( DragReference inDragRef, ItemReference inItemRef,
											FlavorType & oFlavor ) ;

		// must override to do the right thing
	virtual void HandleDropOfPageProxy ( const char* inURL, const char* inTitle ) = 0;
	virtual void HandleDropOfLocalFile ( const char* inFileURL, const char* fileName,
											const HFSFlavor & inFileData ) = 0;
	virtual void HandleDropOfText ( const char* inTextData ) = 0;

	typedef vector<FlavorType> 					FlavorList;
	typedef vector<FlavorType>::iterator		FlavorListIterator;
	typedef vector<FlavorType>::const_iterator	FlavorListConst_Iterator;
	
		// access to the flavor list
	FlavorList & AcceptedFlavors() { return mAcceptableFlavors; }
	const FlavorList & AcceptedFlavors() const { return mAcceptableFlavors; }
	
private:
	
	FlavorList mAcceptableFlavors;
	
}; // CURLDragMixin


//
// class CHTAwareURLDragMixin
//
// Adds to CURLDragMixin the ability to understand things coming from HT (Aurora).
//

class CHTAwareURLDragMixin : public CURLDragMixin
{
public:

	CHTAwareURLDragMixin ( );
	virtual ~CHTAwareURLDragMixin ( ) { } ;
	
protected:

		// overridden to handle HT_Resource drops
	virtual void ReceiveDragItem ( DragReference inDragRef, DragAttributes /*inDragAttrs*/,
											ItemReference inItemRef, Rect & /*inItemBounds*/ ) ;

		// must override to do the right thing
	virtual void HandleDropOfHTResource ( HT_Resource node ) = 0;

}; // CHTAwareURLDragMixin