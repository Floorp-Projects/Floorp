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
// Interface for class that handles dragging an outline of an icon and descriptive text.
// Will do translucent dragging if available (which it almost always should be). It makes
// use of a helper class (CIconTextSuite) to handle building an icon suite and text caption.
// I broke this out to allow more flexibility in how the suite is created (from a resource
// file, from RDF backend, etc).
//
// Hopefully, this (or a derrivative) should be able to meet the needs of any drag and drop
// behavior in the toolbars or the nav center instead of having us reinvent the wheel over
// and over again.
//
// This rather poor first stab is enough functionality to get its first client (the personal
// toolbar) working. It provides a caption to the right of the icon. Translucent
// dragging is broken and commented out. Future versions will also support the ability to
// drag multiple items.
//

#pragma once


#include "CBrowserDragTask.h"
#include "cstring.h"
#include "htrdf.h"


class LCaption;
class LDragTask;
class LArray;


//
// CIconTextSuite
//
// A standalone class that is helpful in building a suite of icon and text caption. Classes
// that use this don't need to know how the suite is created (resource, RDF backend, etc),
// how the icon and the text go together (text beside icon, text below icon..), or even the
// size of the icon (16x16, 32x32...)
//

class CIconTextSuite {

public:
	CIconTextSuite ( ) ;
	CIconTextSuite ( LView* inParent, const Rect & inBounds, ResIDT inIconId, 
						const cstring & inIconText, const HT_Resource inHTNodeData = nil ) ;
	CIconTextSuite ( const CIconTextSuite & other ) ;
	virtual ~CIconTextSuite ( ) ;
	
	virtual Rect IconRectGlobal ( ) const;
	virtual Rect IconRectLocal ( ) const;
	virtual Rect TextRectGlobal ( ) const;
	virtual Rect TextRectLocal ( ) const;
	virtual Rect BoundingRect ( ) const;
	
//	virtual void DrawIcon ( ) const;
//	virtual void DrawText ( ) const;
	
	// 
	// Accessors/Mutators
	//
#if I_WASNT_IN_SUCH_A_HURRY_TO_CHECK_THIS_IN
	void IconSuite ( Handle inIconSuite ) 
		{  
			::DisposeIconSuite ( mIconSuite, true );
			mIconSuite = inIconSuite;
		};
	Handle IconSuite ( ) const { return mIconSuite; } ;
	void IconText ( const cstring & inIconText )  { mIconText = inIconText; } ;
	cstring & IconText ( ) { return mIconText; } ;
	LView* Parent ( ) const { return mParent; } ;
#endif
	
	const HT_Resource GetHTNodeData ( ) const { return mHTNodeData; }
	
 protected:

	Handle mIconSuite;
	cstring mIconText;
	Rect mBounds;
	LCaption* mDisplayText;
	LView* mParent;
	
	const HT_Resource mHTNodeData;		// pointer to HT_Resource in RDF

}; // CIconTextSuite


//
// CIconTextDragTask
//
// Use this class when you need to drag a bookmark (or something url-ish) in the browser window. It
// handles building the drag region and registering the appropriate drag flavors for all drag
// items provided.
//
// The |inLocalFrame| parameter to the constructor is currently not used....soon it may just
// become a point, but we'll see how the translucent dragging stuff goes after feature complete date...
//

class CIconTextDragTask : public CBrowserDragTask
{
public:
	typedef CBrowserDragTask super;
	
	CIconTextDragTask ( const EventRecord& inEventRecord, const LArray & inDragItems, 
							const Rect& inLocalFrame );
	CIconTextDragTask ( const EventRecord& inEventRecord, const CIconTextSuite* inItem, 
							const Rect& inLocalFrame );
	CIconTextDragTask ( const EventRecord& inEventRecord, const Rect& inLocalFrame );
	virtual ~CIconTextDragTask ( ) ;
	
	// dispatch to do either normal or translucent dragging
	virtual OSErr DoDrag();
	
	// for adding drag items one by one after drag task created
	void AddDragItem ( const CIconTextSuite * inItem ) ;
		// void AddDragItem ( const LArray & inItems ) ; maybe? probably not needed....
	
protected:

	virtual	void DoNormalDrag();
	virtual	void DoTranslucentDrag();
	
	// coordinate with the multiple CIconTextSuites to create a drag region
	virtual void MakeDragRegion ( DragReference inDragRef, RgnHandle inDragRegion ) ;

	// add drag flavor info for multiple items
	virtual void AddFlavors ( DragReference inDragRef ) ;

	virtual void AddFlavorForItem ( DragReference inDragRef, ItemReference inItemRef,
										const CIconTextSuite* inItem ) ;

	virtual void AddFlavorHTNode ( ItemReference inItemRef, const HT_Resource inNode ) ;
	
	Rect mFrame;
	LArray mDragItems; 

}; // CIconTextDragTask
