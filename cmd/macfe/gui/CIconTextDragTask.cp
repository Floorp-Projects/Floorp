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
// Implementation for class that handles dragging an outline of an icon and descriptive text.
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


#include "CIconTextDragTask.h"
#include "CEnvironment.h"
#include "macutil.h"
#include "CGWorld.h"
#include "StRegionHandle.h"
#include "StCaptureView.h"
#include "LDragTask.h"
#include "LArray.h"
#include "resgui.h"


//
// Constructor
//
// Takes a pre-built list of CIconTextSuite's
//
CIconTextDragTask :: CIconTextDragTask( const EventRecord& inEventRecord, 
										const LArray & inDragItems, 
										const Rect& inLocalFrame )
	:	mFrame(inLocalFrame), mDragItems(inDragItems),
		super(inEventRecord)
{
	// do nothing
}


//
// Constructor
//
// Takes a single CIconTextSuite and adds it to our internal list automatically
//
CIconTextDragTask :: CIconTextDragTask ( const EventRecord& inEventRecord, 
											const CIconTextSuite * inItem, 
											const Rect& inLocalFrame )
	: mFrame(inLocalFrame), super(inEventRecord)
{
	AddDragItem ( inItem );
}


//
// Constructor
//
// Provided so user doesn't have to supply a list when the drag task is created, but
// can use AddDragItem() later
//
CIconTextDragTask :: CIconTextDragTask ( const EventRecord& inEventRecord, 
											const Rect& inLocalFrame )
	: mFrame(inLocalFrame), super(inEventRecord)
{


}


//
// Destructor
//
CIconTextDragTask :: ~CIconTextDragTask ( )
{
	// do nothing
}


//
// AddDragItem
//
// Add the item to our internal list
//
void
CIconTextDragTask :: AddDragItem ( const CIconTextSuite * inItem )
{
	mDragItems.InsertItemsAt ( 1, LArray::index_Last, &inItem );

} // AddDragItem


//
// MakeDragRegion
//
// Compute the outline of the region the user sees when they drag. We use the 
// CIconTextSuite to do all the work, since it knows how (and where) it wants to draw.
// Multiple drag items are handled by iterating over the item list and adding each
// item's rectangles to the overall region.
//
void
CIconTextDragTask :: MakeDragRegion( DragReference /*inDragRef*/, RgnHandle inDragRegion)
{
	CIconTextSuite* curr = NULL;
	LArrayIterator it ( mDragItems );
	while ( it.Next(&curr) ) {

		Rect iconRect = curr->IconRectGlobal();
		AddRectDragItem(static_cast<ItemReference>(1), iconRect);
		RgnHandle iconRectRgn = ::NewRgn();
		::RectRgn ( iconRectRgn, &iconRect );
		::UnionRgn ( inDragRegion, iconRectRgn, inDragRegion );
		DisposeRgn ( iconRectRgn );
		
		Rect textRect = curr->TextRectGlobal();
		AddRectDragItem(static_cast<ItemReference>(1), textRect);
		RgnHandle textRectRgn = ::NewRgn();
		::RectRgn ( textRectRgn, &textRect );
		::UnionRgn ( inDragRegion, textRectRgn, inDragRegion );
		DisposeRgn ( textRectRgn );

	} // for each item to be dragged

} // MakeDragRegion


//
// AddFlavors
//
// Overloaded to allow for multiple items being dragged and to assign them each
// a different item reference number. 
//
void 
CIconTextDragTask :: AddFlavors ( DragReference inDragRef )
{
	Uint32 itemRef = 1;
	CIconTextSuite* curr = NULL;
	LArrayIterator it ( mDragItems );
	while ( it.Next(&curr) ) {
		AddFlavorForItem ( inDragRef, itemRef, curr );		
		itemRef++;
	} // for each item
	
} // AddFlavors


//
// AddFlavorForItem
//
// Register the flavors for the given item. Overload this (as opposed to AddFlavors()) to
// do something out of the ordinary for each item or do register different
// drag flavors from the standard bookmark flavors.
//
void
CIconTextDragTask :: AddFlavorForItem ( DragReference inDragRef, ItemReference inItemRef,
											const CIconTextSuite* inItem )
{
	#pragma unused (inDragRef)
	
	// ...register standard bookmark flavors
	AddFlavorBookmarkFile(inItemRef);
	AddFlavorURL(inItemRef);
	AddFlavorHTNode(inItemRef, inItem->GetHTNodeData());

} // AddFlavorForItem


//
// AddFlavorHTNode
//
// This flavor contains the RDF information for a node in a hypertree. It only has 
// meaning internally to Navigator.
//
void
CIconTextDragTask :: AddFlavorHTNode ( ItemReference inItemRef, const HT_Resource inNode )
{
	OSErr theErr = ::AddDragItemFlavor(
									mDragRef,
									inItemRef,
									emHTNodeDrag,
									&inNode,			
									sizeof(HT_Resource),
									flavorSenderOnly);
	ThrowIfOSErr_(theErr);
	

} // AddFlavorHTNode


//
// DoDrag
//
// Register the normal url flavors then dispatch to handle either translucent or
// normal dragging
//
OSErr
CIconTextDragTask :: DoDrag ( )
{
	MakeDragRegion(mDragRef, mDragRegion);
	AddFlavors(mDragRef);
	
	if (UEnvironment::HasFeature(env_HasDragMgrImageSupport)) {
		try {
			DoTranslucentDrag();
		}
		catch (...) {
			DoNormalDrag();
		}
	}
	else
		DoNormalDrag();
	
	return noErr;
	
} // DoDrag


//
// DoNormalDrag
//
// Just tracks the drag displaying the outline of the region created from MakeDragRegion()
//
void
CIconTextDragTask :: DoNormalDrag ( )
{
	::TrackDrag(mDragRef, &mEventRecord, mDragRegion);
	
} // DoNormalDrag


//
// DoTranslucentDrag
//
// Tracks the drag but also draws the icon and caption translucently.
//
void
CIconTextDragTask :: DoTranslucentDrag ( )
{
	::TrackDrag(mDragRef, &mEventRecord, mDragRegion);
	
#if THIS_WOULD_EVER_WORK
	Rect theFrame;
	StColorPortState theColorPortState(mSuite.Parent()->GetMacPort());

	// Normalize the color state (to make CopyBits happy)
	StColorState::Normalize();
	
	// Build a GWorld
	Rect gworldSize = mSuite.BoundingRect();
	CGWorld theGWorld(gworldSize, 0, useTempMem);
	theGWorld.BeginDrawing();
//	StCaptureView theCaptureView( *mSuite.Parent() );

	try
	{
//		theCaptureView.Capture(theGWorld);

		mSuite.Parent()->FocusDraw();
	
		mSuite.mDisplayText->Show();
//		mSuite.DrawIcon();

	   	Point theOffsetPoint = topLeft(PortToGlobalRect(mSuite.Parent(),
	   								LocalToPortRect(mSuite.Parent(), mFrame)));
		
		// Set the drag image

		StRegionHandle theTrackMask;

//		mProxyPane.CalcLocalFrameRect(theFrame);
//		ThrowIfOSErr_(::IconSuiteToRgn(theTrackMask, &theFrame, kAlignAbsoluteCenter, mIconSuite));
		
		mSuite.mDisplayText->CalcLocalFrameRect(theFrame); // Use frame which bounds the actual text, not the frame bounds
		theTrackMask += theFrame;
		
		theGWorld.EndDrawing();
		
		PixMapHandle theMap = ::GetGWorldPixMap(theGWorld.GetMacGWorld());
		OSErr theErr = ::SetDragImage(mDragRef, theMap, theTrackMask, theOffsetPoint, dragDarkerImage);
		ThrowIfOSErr_(theErr);
		
		// Track the drag
		
		::TrackDrag(mDragRef, &mEventRecord, mDragRegion);
	}
	catch (...)
	{
	}
	
	mSuite.mDisplayText->Hide();
#endif

} // DoTranslucentDrag


#pragma mark -

CIconTextSuite :: CIconTextSuite ( )
	: mIconSuite(nil), mIconText(""), mDisplayText(nil), mParent(nil), mHTNodeData(nil)
{

} // constructor


//
// Constructor
//
// Creates a icon suite that is loaded in from an icon in a resource file. |inBounds| must
// be in local coordinates, and is only used as a guideline for giving a top/left to
// draw the icon and caption relative to. It is NOT used as a clipping rectangle.
//
CIconTextSuite :: CIconTextSuite ( LView* inParent, const Rect & inBounds, ResIDT inIconId, 
									const cstring & inIconText, const HT_Resource inHTNodeData )
	: mIconText(inIconText), mBounds(inBounds), mParent(inParent), mHTNodeData(inHTNodeData)
{
	IconSelectorValue selector = svAllAvailableData;
	::GetIconSuite ( &mIconSuite, inIconId, selector );
	ThrowIfNil_ ( mIconSuite );
	
	// build a caption for the icon's title relative to the bounding rect we're given.
	// The top and left offsets are from the LSmallIconTable's drawing routine.
	SPaneInfo paneInfo;
	paneInfo.width = ::TextWidth(inIconText, 0, inIconText.length());
	paneInfo.height = 16;
	paneInfo.visible = false;
	paneInfo.left = inBounds.left + 22;
	paneInfo.top = inBounds.top + 2;
	paneInfo.superView = inParent;
	mDisplayText = new LCaption ( paneInfo, LStr255(mIconText), 130 );
	ThrowIfNil_(mDisplayText);
	
} // constructor


//
// Copy Constructor
//
CIconTextSuite :: CIconTextSuite ( const CIconTextSuite & other )
	: mHTNodeData(other.mHTNodeData)
{
	mIconSuite = other.mIconSuite;
	mBounds = other.mBounds;
	mIconText = other.mIconText;
	mDisplayText = other.mDisplayText;
	mParent = other.mParent;

} // copy constructor


//
// Destructor
//
// Clean up our mess
//
CIconTextSuite :: ~CIconTextSuite ( ) 
{
	::DisposeIconSuite ( mIconSuite, true );
	delete mDisplayText;
	
} // destructor


//
// IconRectLocal
//
// Returns the rectangle (in local coordinates) of the icon, relative to the |mBounds|
// rectangle. The offsets are from LSmallIconTable's draw routine.
//
Rect
CIconTextSuite :: IconRectLocal ( ) const
{
	Rect iconRect;
	
	// Add the icon region
	iconRect.left = mBounds.left + 3;
	iconRect.right = iconRect.left + 16;
	iconRect.bottom = mBounds.bottom - 2;
	iconRect.top = iconRect.bottom - 16;
	return iconRect;
	
} // IconRectLocal


//
// IconRectGlobal
//
// Takes the local icon bounds and makes them global
//
Rect
CIconTextSuite :: IconRectGlobal ( ) const
{
	return ( PortToGlobalRect(mParent, LocalToPortRect(mParent, IconRectLocal())) );

} // IconRectGlobal


//
// TextRectLocal
//
// Returns the rectangle (in local coordinates) of the text caption, relative to the |mBounds|
// rectangle. Since we've already positioned the caption when we started up, this is
// pretty easy.
//
Rect
CIconTextSuite :: TextRectLocal ( ) const
{
	Rect theFrame;

	mDisplayText->CalcLocalFrameRect(theFrame);
	return theFrame;

} // TextRectLocal


//
// TextRectGlobal
//
// Takes the local text bounds and makes them global
//
Rect
CIconTextSuite :: TextRectGlobal ( ) const
{
	return ( PortToGlobalRect(mParent, LocalToPortRect(mParent, TextRectLocal())) );

} // TextRectGlobal


//
// BoundingRect
//
// Compute the rect that can contain the icon and caption entirely.
//
Rect
CIconTextSuite :: BoundingRect ( ) const
{
	Rect bounds = { 0, 0, 0, 0 };
	
	mParent->FocusDraw();
	Rect textRect = TextRectLocal();
	
	bounds.right = textRect.right;
	bounds.bottom = textRect.bottom;
	return bounds;
	
} // BoundingRect
