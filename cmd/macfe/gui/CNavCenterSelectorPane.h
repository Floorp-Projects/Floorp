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
// Mike Pinkerton, Netscape Communications
//
// Contains:
//
// ¥ SelectorImage
//		an abstract class for drawing the workspace icon
//
// ¥ TitleImage
//		implements above
//
// ¥ CNavCenterSelectorPane
//		handles drawing/clicking/tooltips/etc of workspaces and all powerplant interactions
//

#pragma once

#include <LPane.h>
#include <LBroadcaster.h>

#include "LString.h"
#include "htrdf.h"
#include "CDynamicTooltips.h"
#include "CURLDragHelper.h"


#pragma mark -- class SelectorImage and SelectorData --


//
// SelectorImage
//
// An abstract base class that knows how to draw itself in the appropriate mode (text/icon/text&icon)
// as well as selected or not-selected.
//
class SelectorImage
{
	public:
		virtual ~SelectorImage ( ) { } ;
		virtual void DrawInCurrentView( const Rect& bounds, unsigned long mode ) const = 0;
};


//
// SelectorData
//
// an element within the Nav Center workspace list.
//
struct SelectorData
{
	SelectorData ( HT_View inView ) ;
	~SelectorData();
	
	SelectorImage*		workspaceImage;		// ptr to object that knows how to draw workspace icon
};


#pragma mark -- class CNavCenterSelectorPane --

//
// CNavCenterSelectorPane
//
// Draws the workspace icons in a vertical table. Handles setting the currently selected
// workspace when an icon is clicked on and telling the RDFCoordinator to change the open/close
// state of the shelf if need be.
//

class CNavCenterSelectorPane
		: public LView, public LDragAndDrop, public LBroadcaster, public LCommander,
			public CDynamicTooltipMixin, public CHTAwareURLDragMixin
{
public:
	friend class CNavCenterSelectorContextMenuAttachment;
	
		// modes for drawing icons
	enum {
		kSelected = 1, kIcon = 2, kText = 4
	};

		// powerplant stuff (panes and messages)
	enum {
		class_ID					= 'NvSl',
		msg_ShelfStateShouldChange	= 'shlf',		// broadcast when shelf should open/close
		msg_ActiveSelectorChanged	= 'selc'		// broadcast when selector changes
	};

	CNavCenterSelectorPane( LStream* );
	virtual ~CNavCenterSelectorPane();

		// Tell view to select the next workspace, wrapping around if we hit the ends	
	virtual void CycleCurrentWorkspace ( ) ;

		// Tell view if it is in a browser window 
	void SetEmbedded ( bool isEmbedded ) { mIsEmbedded = isEmbedded; }
	
		// Get/Set info about the HT_Pane that contains all the workspaces to display 
	void SetHTPane ( HT_Pane inPane ) { mHTPane = inPane; }
	HT_Pane GetHTPane ( ) const { return mHTPane; }

		// access and change the current workspace.
	void SetActiveWorkspace ( HT_View inNewWorkspace ) ;
	HT_View GetActiveWorkspace ( ) const { return mCachedWorkspace; }
	
protected:
	enum EDropLocation { kDropBefore, kDropOn, kDropAfter  } ;
	
		// PowerPlant overrides
	virtual void DrawSelf();
	virtual void ClickSelf( const SMouseDownEvent& );
	virtual void AdjustCursorSelf( Point, const EventRecord& );
	virtual void FindCommandStatus ( CommandT inCommand, Boolean &outEnabled,
										Boolean &outUsesMark, Char16 &outMark, Str255 outName) ;

		// redraw when things change and broadcast that they have.
	virtual void NoticeActiveWorkspaceChanged ( HT_View iter ) ;
	
		// click handling
	size_t FindIndexForPoint ( const Point & inMouseLoc ) const;
	size_t FindIndexForPoint ( const Point & inMouseLoc, EDropLocation & outWhere ) const ;
	HT_View FindSelectorForPoint ( const Point & inMouseLoc ) const;
	HT_View FindSelectorForPoint ( const Point & inMouseLoc, EDropLocation & outWhere,
										TableIndexT & outRowBefore ) const;

		// stuff for dynamic tooltips
	virtual void FindTooltipForMouseLocation ( const EventRecord& inMacEvent, StringPtr outTip ) ;
	virtual void MouseWithin ( Point inPortPt, const EventRecord& ) ;
	virtual void MouseLeave ( ) ;

		// stuff for drag and drop
	virtual Boolean	ItemIsAcceptable ( DragReference inDragRef, ItemReference inItemRef ) ;
	virtual void	InsideDropArea ( DragReference inDragRef ) ;
	virtual void	EnterDropArea ( DragReference inDragRef, Boolean inDragHasLeftSender ) ;
	virtual void	LeaveDropArea ( DragReference inDragRef ) ;
	virtual void	DrawDividingLine( TableIndexT inRow, EDropLocation inPrep ) ;
	virtual void	HandleDropOfHTResource ( HT_Resource node ) ;
	virtual void	HandleDropOfPageProxy ( const char* inURL, const char* inTitle ) ;
	virtual void	HandleDropOfLocalFile ( const char* inFileURL, const char* fileName,
												const HFSFlavor & /*inFileData*/ ) ;
	virtual void	HandleDropOfText ( const char* inTextData ) ;
	virtual void	ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttrs,
											ItemReference inItemRef, Rect & inItemBounds ) ;
	
	bool				mIsEmbedded;			// is this embedded in chrome or standalone window?
	HT_Pane				mHTPane;
	HT_View				mCachedWorkspace;		// which view is currently active?
	short				mTooltipIndex;			// which item is mouse hovering over
	bool				mMouseIsInsidePane;		// is the mouse in the pane?

	Uint32				mDragAndDropTickCount;	// used only during d&d as a timer
	TableIndexT			mDropRow;				// row mouse is over during a drop.
												//	holds |LArray::index_Last| when invalid
	EDropLocation		mDropPreposition;		// where does drop go, in relation to mDropRow?
	
	unsigned long		mImageMode;
	size_t				mCellHeight;

}; // class CNavCenterSelectorPane


#pragma mark -- class TitleImage --

//
// TitleImage
//
// A simple little hack to display the title of the workspace
//

class TitleImage : public SelectorImage
{
public:
	TitleImage ( const LStr255 & inTitle, ResIDT inIconID ) ;

	virtual void DrawInCurrentView( const Rect& bounds, unsigned long mode ) const ;
	
	const LStr255 & Title ( ) const { return mTitle; } ;
	ResIDT IconID ( ) const { return mIconID; } ;
	Uint16 TextWidth ( ) const { return mTextWidth; } ;
		
private:

#if DRAW_SELECTED_AS_TAB
	virtual	void			DrawOneTab(RgnHandle inBounds) const;
	virtual	void			DrawTopBevel(RgnHandle inTab) const;
	virtual	void			DrawBottomBevel(RgnHandle inTab, Boolean inCurrentTab) const;
	virtual void			DrawOneTabBackground(
								RgnHandle inRegion,
								Boolean inCurrentTab) const;
	virtual void			DrawOneTabFrame(RgnHandle inRegion, Boolean inCurrentTab) const;
	virtual void			DrawCurrentTabSideClip(RgnHandle inRegion) const;
#endif

	LStr255 mTitle;
	ResIDT mIconID;
	Uint16 mTextWidth;
	
}; // classTitleImage
