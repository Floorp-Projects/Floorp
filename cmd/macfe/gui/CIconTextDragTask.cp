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
#include "miconutils.h"
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
	while ( it.Next(&curr) )
	{
		{
			Rect iconRect = curr->IconRectGlobal();
			AddRectDragItem(static_cast<ItemReference>(1), iconRect);
			StRegionHandle iconRectRgn ( iconRect );
			StRegionHandle inset ( iconRectRgn );			// sub out all but the outline
			::InsetRgn ( inset, 1, 1 );
			::DiffRgn ( iconRectRgn, inset, iconRectRgn );
			
			::UnionRgn ( inDragRegion, iconRectRgn, inDragRegion );
		}
		{		
			Rect textRect = curr->TextRectGlobal();
			AddRectDragItem(static_cast<ItemReference>(1), textRect);
			StRegionHandle textRectRgn ( textRect );
			StRegionHandle inset ( textRectRgn );			// sub out all but the outline
			::InsetRgn ( inset, 1, 1 );
			::DiffRgn ( textRectRgn, inset, textRectRgn );
			
			::UnionRgn ( inDragRegion, textRectRgn, inDragRegion );
		}
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

	string urlAndTitle = HT_GetNodeURL(inItem->GetHTNodeData());
	if ( inItem->IconText().length() ) {
		urlAndTitle += "\r";
		urlAndTitle += inItem->IconText().c_str();
	}
	AddFlavorBookmark ( inItemRef, urlAndTitle.c_str() );
	
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

#if THIS_WOULD_ACTUALLY_WORK
	
	CIconTextSuite* mSuite;
	mDragItems.FetchItemAt(1, &mSuite);

	// Normalize the color state (to make CopyBits happy)
	StColorPortState theColorPortState(mSuite->Parent()->GetMacPort());
	StColorState::Normalize();
	
	// Build a GWorld. The rectangle passed to CGWorld should be in local coordinates.
	CGWorld theGWorld(mSuite->BoundingRect(), 0, useTempMem);
	theGWorld.BeginDrawing();
	StCaptureView theCaptureView( *mSuite->Parent() );

	try
	{
		theCaptureView.Capture(theGWorld);

		// draw into the GWorld
		mSuite->Parent()->FocusDraw();	
		mSuite->DrawText();
		mSuite->DrawIcon();
		theGWorld.EndDrawing();

	   	Point theOffsetPoint = topLeft(mFrame);
		mSuite->Parent()->LocalToPortPoint(theOffsetPoint);
		mSuite->Parent()->PortToGlobalPoint (theOffsetPoint);
		
		// create the mask.
		StRegionHandle theTrackMask;
		MakeDragRegion(mDragRef, theTrackMask);
//		ThrowIfOSErr_(::IconSuiteToRgn(theTrackMask, &mFrame, kAlignAbsoluteCenter, mIconSuite));
//		Rect textArea = mSuite.TextRectLocal(); // Use frame which bounds the actual text, not the frame bounds
//		theTrackMask += textArea;
				
		// Set the drag image
		PixMapHandle theMap = ::GetGWorldPixMap(theGWorld.GetMacGWorld());
		OSErr theErr = ::SetDragImage(mDragRef, theMap, theTrackMask, theOffsetPoint, kDragDarkerTranslucency);
		ThrowIfOSErr_(theErr);
		
		// Track the drag	
		::TrackDrag(mDragRef, &mEventRecord, mDragRegion);
	}
	catch (...)
	{
		DebugStr("\pError in Translucent Drag; g");
	}
	
//	mSuite.mDisplayText->Hide();

#endif // THIS_WOULD_ACTUALLY_WORK

} // DoTranslucentDrag


#pragma mark -

CIconTextSuite :: CIconTextSuite ( )
	: mIconID(0), mIconText(""), mDisplayText(nil), mParent(nil), mHTNodeData(nil)
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
									const string & inIconText, const HT_Resource inHTNodeData )
	: mIconText(inIconText), mBounds(inBounds), mParent(inParent), mHTNodeData(inHTNodeData),
		mIconID(inIconId)
{
	
	// build a caption for the icon's title relative to the bounding rect we're given.
	// The top and left offsets are from the LSmallIconTable's drawing routine.
	SPaneInfo paneInfo;
	paneInfo.width = ::TextWidth(inIconText.c_str(), 0, inIconText.length());
	paneInfo.height = 16;
	paneInfo.visible = false;
	paneInfo.left = inBounds.left + 22;
	paneInfo.top = inBounds.top + 2;
	paneInfo.superView = inParent;
	mDisplayText = new LCaption ( paneInfo, LStr255(mIconText.c_str()), 130 );
	ThrowIfNil_(mDisplayText);
	
} // constructor


//
// Copy Constructor
//
CIconTextSuite :: CIconTextSuite ( const CIconTextSuite & other )
	: mHTNodeData(other.mHTNodeData)
{
	mIconID = other.mIconID;
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
// Compute the rect that can contain the icon and caption entirely. This rect will have
// its top/left at (0,0)
//
Rect
CIconTextSuite :: BoundingRect ( ) const
{
	Rect bounds = mBounds;
	
	mParent->FocusDraw();
	Rect textRect = TextRectLocal();
	
	if ( textRect.right > bounds.right )
		bounds.right = textRect.right;
	
	::OffsetRect(&bounds, -bounds.left, -bounds.top);	// move to (0,0)
	return bounds;
	
} // BoundingRect


void
CIconTextSuite :: DrawIcon ( ) const
{
	Rect iconRect = IconRectLocal();
	if ( ::PlotIconID(&iconRect, atNone, kTransformSelected, mIconID) != noErr )
		throw;
		
} // DrawIcon


void
CIconTextSuite :: DrawText ( ) const
{
	::EraseRect ( &mBounds );
//	mDisplayText->Show();
//	mDisplayText->Draw(nil);
//	mDisplayText->Hide();
}
