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
// Implementations for all the things that can go on a toolbar.
//
// CRDFToolbarItem - the base class for things that go on toolbars
// CRDFPushButton - a toolbar button
// CRDFSeparator - a separator bar
// CRDFURLBar - the url bar w/ proxy icon
//
// I apologize profusely for the poor design and amount of copied code
// from CButton. We had to do this is about a week from start to finish
// about a month _after_ the feature-complete deadline. If you don't like
// the code, deal (pinkerton).
//

#include "CRDFToolbarItem.h"
#include "CToolbarModeManager.h"
#include "UGraphicGizmos.h"
#include "UGAAppearance.h"
#include "URDFUtilities.h"
#include "CBrowserWindow.h"
#include "CTargetedUpdateMenuRegistry.h"
#include "uapp.h"
#include "CPaneEnabler.h"


extern RDF_NCVocab gNavCenter;			// RDF vocab struct for NavCenter


CRDFToolbarItem :: CRDFToolbarItem ( HT_Resource inNode )
	: mNode(inNode)
{
	Assert_(mNode != NULL);
}


CRDFToolbarItem :: ~CRDFToolbarItem ( )
{

}


#pragma mark -


CRDFPushButton :: CRDFPushButton ( HT_Resource inNode )
	: CRDFToolbarItem(inNode), mTitleTraitsID(130), mCurrentMode(eTOOLBAR_TEXT_AND_ICONS),
		mMouseInFrame(false),  mTrackInside(false), mGraphicPadPixels(5), mTitlePadPixels(5),
		mOvalWidth(8), mOvalHeight(8)
{
	// figure out icon and text placement depending on the toolbarBitmapPosition property in HT. The
	// first param is the alignment when the icon is one top, the 2nd when the icon is on the side.
	mTitleAlignment = CalcAlignment(kAlignCenterBottom, kAlignCenterRight);
	mGraphicAlignment = CalcAlignment(kAlignCenterTop, kAlignCenterLeft);
	
	mCurrentMode = CalcDisplayMode();
	
	AttachTooltip();
	AttachContextMenu();
	AttachPaneEnabler();
	
	AssignCommand();
	
	// get ready to draw by caching all the graphic and title rects.
	PrepareDrawButton();
}


CRDFPushButton :: ~CRDFPushButton ( )
{

}


//
// HookUpToListeners
//
// Find the top level browser window and register it as a listener for commands, but only
// if this button is a special command. Other kinds of buttons don't need to be registered as
// they just dispatch immediately and don't broadcast commands.
//
void
CRDFPushButton :: HookUpToListeners ( )
{
	const char* url = HT_GetNodeURL(HTNode());
	if ( url && strncmp(url, "command:", 8) == 0 ) {
		LView* top=GetSuperView();
		while ( top && top->GetSuperView() )
			top = top->GetSuperView();
		LListener* topListener = dynamic_cast<LListener*>(top);
		if ( topListener )
			AddListener(topListener);	
	}
		
} // HookUpToListeners


//
// AssignCommand
//
// Set LControl's value message for clicking and command enabling. The default is |cmd_ToolbarButton|
// which is a container or url (a la personal toolbar button). Explicit commands (anything with
// "command:" in their URL) have their own command ID's in the FE.
//
void
CRDFPushButton :: AssignCommand ( )
{
	SetValueMessage ( cmd_ToolbarButton );

	// check for one of our known commands. If it is one of those, change the command id.
	const char* url = HT_GetNodeURL(HTNode());
	if ( url && strncmp(url, "command:", 8) == 0 ) {
		if ( strcmp(url, "command:back") == 0 )
			SetValueMessage ( cmd_GoBack );
		else if ( strcmp(url, "command:forward") == 0 )
			SetValueMessage ( cmd_GoForward );
		else if ( strcmp(url, "command:stop") == 0 )
			SetValueMessage ( cmd_Stop );
		else if ( strcmp(url, "command:reload") == 0 )
			SetValueMessage ( cmd_Reload );		
		else if ( strcmp(url, "command:search") == 0 )
			SetValueMessage ( cmd_NetSearch );
	}
	
} // AssignCommand


//
// CalcDisplayMode
//
// Computes the appropriate FE constant for the toolbar mode given what is in HT. Because this
// property can be specified/overridden, we have to check several places in a cascading
// manner: 1) check the node; if not there, 2) check the view the node is in.
//
UInt32
CRDFPushButton :: CalcDisplayMode ( ) const
{
	Uint32 retVal = eTOOLBAR_TEXT_AND_ICONS;
	char* data = NULL;
	if ( ! HT_GetTemplateData(HTNode(), gNavCenter->toolbarDisplayMode, HT_COLUMN_STRING, &data) )
		if ( ! HT_GetTemplateData(HT_TopNode(HT_GetView(HTNode())), gNavCenter->toolbarDisplayMode, HT_COLUMN_STRING, &data) )
			data = NULL;

	if ( data ) {
		if ( strcmp(data, "text") == 0 )
			retVal = eTOOLBAR_TEXT;
		else if ( strcmp(data, "pictures") == 0 )
			retVal = eTOOLBAR_ICONS;
	}
	
	return retVal;
	
} // CalcDisplayMode


//
// CalcAlignment
//
// Compute the FE alignment of a specific HT property. If HT specifies the icon is on top, use |inTopAlignment|,
// otherwise use |inSideAlignment|. This property is a view-level property so we need to pass in the top node
// of the view, not the node associated with this button. As a result, all buttons on a given toolbar MUST
// have the same alignment. You cannot have one button with the icon on top and another with the icons on the side
// in the same toolbar.
//
UInt32
CRDFPushButton :: CalcAlignment ( UInt32 inTopAlignment, Uint32 inSideAlignment ) const
{
	Uint32 retVal = inSideAlignment;
	char* value = NULL;
	PRBool success = HT_GetTemplateData ( HT_TopNode(HT_GetView(HTNode())), gNavCenter->toolbarBitmapPosition, HT_COLUMN_STRING, &value );
	if ( success && value ) {
		if ( strcmp(value, "top") == 0 )
			retVal = inTopAlignment;
	}
	
	return retVal;
} // CalcAlignment


//
// CalcTitleFrame
//
// This calculates the bounding box of the title (if any).  This is useful
// for both the string placement, as well as position the button graphic
// (again, if any).
//
// Note that this routine sets the text traits for the ensuing draw.  If
// you override this method, make sure that you're doing the same.
//
void 
CRDFPushButton :: CalcTitleFrame ( )
{
	char* title = HT_GetNodeName(HTNode());
	if ( !title || !*title )
		return;

	UTextTraits::SetPortTextTraits(mTitleTraitsID);

	FontInfo theInfo;
	::GetFontInfo(&theInfo);
	mCachedTitleFrame.top = mCachedButtonFrame.top;
	mCachedTitleFrame.left = mCachedButtonFrame.left;		
	mCachedTitleFrame.right = mCachedTitleFrame.left + ::StringWidth(LStr255(title));;
	mCachedTitleFrame.bottom = mCachedTitleFrame.top + theInfo.ascent + theInfo.descent + theInfo.leading;;

	if (mCurrentMode != eTOOLBAR_TEXT)
	{
		UGraphicGizmos::AlignRectOnRect(mCachedTitleFrame, mCachedButtonFrame, mTitleAlignment);
		UGraphicGizmos::PadAlignedRect(mCachedTitleFrame, mTitlePadPixels, mTitleAlignment);
	}
	else
	{
		UGraphicGizmos::CenterRectOnRect(mCachedTitleFrame, mCachedButtonFrame);
	}
}


//
// CalcGraphicFrame
//
// Precompute where the graphic should go.
//
void
CRDFPushButton :: CalcGraphicFrame ( )
{
	// The container for the graphic starts out as a 32x32 area. If HT tells us 
	// the icon is a small icon, use 16x16. If the icon image is loaded, use its
	// width and height.
	Rect theContainerFrame = mCachedButtonFrame;
	Rect theImageFrame = { 0, 0, 32, 32 };
	char* url = HT_GetIconURL ( HTNode(), PR_TRUE, HT_TOOLBAR_ENABLED );
	if ( url ) {
		if ( strncmp(url, "icon/small", 4) == 0 )
			theImageFrame.right = theImageFrame.bottom = 16;
		else {
			SetImageURL(url);
			
			SDimension16 imageSize;
			if ( GetImageDimensions(imageSize) ) {
				theImageFrame.right = imageSize.width;
				theImageFrame.bottom = imageSize.height;
			}
		}
	}
	
	UGraphicGizmos::AlignRectOnRect(theImageFrame, theContainerFrame, mGraphicAlignment);
	UGraphicGizmos::PadAlignedRect(theImageFrame, mGraphicPadPixels, mGraphicAlignment);
	mCachedGraphicFrame = theImageFrame;
}


//
// AttachTooltip
//
// Creates the tooltip that asks HT to fill it in
//
void
CRDFPushButton :: AttachTooltip ( )
{
	CToolTipAttachment* tip = new CToolTipAttachment(80, 11508);
	Assert_(tip != NULL);
	if ( tip )
		AddAttachment(tip);

} // AttachTooltip


//
// AttachContextMenu
//
// Creates a context menu attachment that will be filled in from HT.
//
void
CRDFPushButton :: AttachContextMenu ( )
{
//	CToolbarContextMenuAttachment* menuAtt = new CToolbarContextMenuAttachment ( HTNode() );


} // AttachContextMenu


//
// AttachPaneEnabler
//
// Creates a enabler policy attachment. We can use the default enabling policy (which is based off
// FindCommandStatus) because we are an LControl.
//
void
CRDFPushButton :: AttachPaneEnabler ( )
{
	CPaneEnabler* enabler = new CPaneEnabler;
	Assert_(enabler != NULL);
	if ( enabler )
		AddAttachment(enabler);

} // AttachPaneEnabler


//
// FindTooltipForMouseLocation
//
// Dredge the tooltip text out of HT for this button
//
void
CRDFPushButton :: FindTooltipForMouseLocation ( const EventRecord& inMacEvent, StringPtr outTip )
{
	char* name = HT_GetNodeName(HTNode());
	if ( name ) {
		outTip[0] = strlen(name);
		strcpy ( (char*) &outTip[1], name );
	}
	else
		*outTip = 0;

} // FindTooltipForMouseLocation


//
// NaturalSize
//
SDimension16
CRDFPushButton::NaturalSize( SDimension16 inAvailable ) const
	/*
	  ...returns the natural size of this item.	 The toolbar asks this of
	  each item during layout.	If possible, the toolbar will resize the
	  item to its natural size, as returned by this routine.

	  If you return exactly the value from |inAvailable| for width,	 the
	  toolbar may assume that you wish to consume all available width, e.g.,
	  for a URL entry field, or a status message.  This does not apply to
	  height (mainly because of the problem described below).  Returning a
	  height greater than |inAvailable| may encourage the toolbar to grow
	  vertically.

	  For a particular dimension:
		* an item with a fixed size can ignore |inAvailable| and return its
		  size;
		* an item with a percentage size can calculate it based on
		  |inAvailable|;
		* an item with a space-filling size should return the smallest space
			it can live in. The toolbar will know it is stretchy because
			it can call IsStretchy().

	  Problems:
		this routine is essentially a hack.	 In particular, if |inAvailable|
	  happens to coincide with the natural size in either dimension,
	  hilarity ensues.	Hopefully, |inAvailable| will always reflect a
	  toolbar wider than a normal button :->

	  Assumptions:
		any toolbar item can be resized arbitrarilly small (to an
	  unspecified limit).
	*/
{
	SDimension16 desiredSpace;

	bool iconOnSide = true;
	char* value = NULL;
	PRBool success = HT_GetTemplateData ( HT_TopNode(HT_GetView(HTNode())), gNavCenter->toolbarBitmapPosition, HT_COLUMN_STRING, &value );
	if ( success && value ) {
		if ( strcmp(value, "top") == 0 )
			iconOnSide = false;
	}
	
	if ( iconOnSide ) {
		desiredSpace.width = (mCachedTitleFrame.right - mCachedTitleFrame.left) + 
								(mCachedGraphicFrame.right - mCachedGraphicFrame.left) + 15;
		desiredSpace.height = min ( max(mCachedGraphicFrame.bottom - mCachedGraphicFrame.top, 
										mCachedTitleFrame.bottom - mCachedTitleFrame.top), 24);
	}
	else {
		desiredSpace.width = 50;
		desiredSpace.height = 50;		//еее for now
	}
	
	return desiredSpace;
}


//
// PrepareDrawButton
//
// Sets up frames and masks before we draw
//
void 
CRDFPushButton :: PrepareDrawButton( )
{
	CalcLocalFrameRect(mCachedButtonFrame);

	// Calculate the drawing mask region.
	::OpenRgn();
	::FrameRoundRect(&mCachedButtonFrame, mOvalWidth, mOvalHeight);
	::CloseRgn(mButtonMask);
	
	CalcTitleFrame();
	CalcGraphicFrame();
}


//
// FinalizeDrawButton
//
// Called after we are done drawing.
//
void
CRDFPushButton :: FinalizeDrawButton ( )
{
	// nothing for now.
}


void 
CRDFPushButton::DrawSelf()
{
	PrepareDrawButton();

	DrawButtonContent();

	const char* title = HT_GetNodeName(HTNode());
	if ( title ) {
		if (mCurrentMode != eTOOLBAR_ICONS && *title != 0)
			DrawButtonTitle();
	}
	
	if (mCurrentMode != eTOOLBAR_TEXT)
		DrawButtonGraphic();
			
	if (!IsEnabled() || !IsActive())
		DrawSelfDisabled();
			
	FinalizeDrawButton();
}


//
// DrawButtonContent
//
// Handle drawing things other than the graphic or title. For instance, if the
// mouse is within this frame, draw a border around the button. If the mouse is
// down, draw the button to look depressed.
//
void
CRDFPushButton :: DrawButtonContent ( ) 
{
	if (IsActive() && IsEnabled()) {
		if ( IsTrackInside() )
			DrawButtonHilited();
		else if (IsMouseInFrame())
			DrawButtonOutline();
	}

} // DrawButtonContent


//
// DrawButtonTitle
//
// Draw the title of the button.
//
void
CRDFPushButton :: DrawButtonTitle ( )
{
	StColorPenState savedState;
	StColorPenState::Normalize();
	
	if (IsTrackInside() || GetValue() == Button_On)
		::OffsetRect(&mCachedTitleFrame, 1, 1);

	if ( IsMouseInFrame() )
		URDFUtilities::SetupForegroundTextColor ( HTNode(), gNavCenter->viewRolloverColor, kThemeIconLabelTextColor ) ;
	else
		URDFUtilities::SetupForegroundTextColor ( HTNode(), gNavCenter->viewFGColor, kThemeIconLabelTextColor ) ;

	char* title = HT_GetNodeName(HTNode());
	UGraphicGizmos::PlaceStringInRect(LStr255(title), mCachedTitleFrame, teCenter, teCenter);

} // DrawButtonTitle


//
// DrawButtonGraphic
//
// Draw the image specified by HT.
// еееIf there isn't an image, what should we do
//
void
CRDFPushButton :: DrawButtonGraphic ( )
{
	const ResIDT cFolderIconID = kGenericFolderIconResource;
	const ResIDT cItemIconID = 15313;
	const ResIDT cFileIconID = kGenericDocumentIconResource;
	
	// figure out which graphic to display. We need to check for pressed before rollover because
	// rollover will always be true when tracking a mouseDown, but not vice-versa.
	HT_IconType displayType = HT_TOOLBAR_ENABLED;
	if ( !IsEnabled() )
		displayType = HT_TOOLBAR_DISABLED;
	else if ( IsTrackInside() )
		displayType = HT_TOOLBAR_PRESSED;
	else if ( IsMouseInFrame() )
		displayType = HT_TOOLBAR_ROLLOVER;
		
	char* url = HT_GetIconURL ( HTNode(), PR_TRUE, displayType );
	if ( url ) {
		if ( strncmp(url, "icon", 4) != 0 ) {
			// setup where we should draw
			Point topLeft;
			topLeft.h = mCachedGraphicFrame.left; topLeft.v = mCachedGraphicFrame.top;
			//uint16 width = 0;
			//uint16 height = 0;
			
			// draw
			SetImageURL ( url );
			DrawImage ( topLeft, kTransformNone, 0, 0 );
		}
		else {
			SInt32 iconID = HT_IsContainer(HTNode()) ? cFolderIconID : cItemIconID;
			::PlotIconID(&mCachedGraphicFrame, atNone, kTransformNone, iconID);
		}
	}
	
} // DrawButtonGraphic

void
CRDFPushButton :: DrawSelfDisabled ( )
{
	// ??

} // DrawSelfDisabled


//
// DrawButtonOutline
//
// Draw the frame around the button to show rollover feedback
//
void
CRDFPushButton :: DrawButtonOutline ( )
{
	// е Setup a device loop so that we can handle drawing at the correct bit depth
	StDeviceLoop	theLoop ( mCachedButtonFrame );
	Int16			depth;

	// Draw face of button first			
	while ( theLoop.NextDepth ( depth )) 
		if ( depth >= 4 )		// don't do anything for black and white
			{
			Rect rect = mCachedButtonFrame;
			::InsetRect(&rect, 1, 1);
			UGraphicGizmos::LowerRoundRectColorVolume(rect, 4, 4,
													  UGAAppearance::sGAHiliteContentTint);
			}

	// Now draw GA button bevel, but only if HT hasn't specified a rollover color
	StColorPenState thePenSaver;
	thePenSaver.Normalize();
	if ( URDFUtilities::SetupForegroundColor(HTNode(), gNavCenter->viewRolloverColor, kThemeIconLabelTextColor) ) {
		::PenSize(2,2);
		::FrameRoundRect(&mCachedButtonFrame, mOvalWidth, mOvalHeight);
	}
	else
		UGAAppearance::DrawGAButtonBevelTint(mCachedButtonFrame);

} // DrawButtonOutline


//
// DrawButtonHilited
//
// Draw the button as if it has been clicked on, drawing the insides depressed
//
void
CRDFPushButton :: DrawButtonHilited ( )
{
	// е Setup a device loop so that we can handle drawing at the correct bit depth
	StDeviceLoop	theLoop ( mCachedButtonFrame );
	Int16			depth;

	// Draw face of button first
	while ( theLoop.NextDepth ( depth )) 
		if ( depth >= 4 )		// don't do anything for black and white
		{
			Rect frame = mCachedButtonFrame;
			::InsetRect(&frame, 1, 1);

			// Do we need to do this very slight darkening?
//			UGraphicGizmos::LowerRoundRectColorVolume(frame, 4, 4, UGAAppearance::sGASevenGrayLevels);
		}

	// Now draw GA pressed button bevel
	if ( URDFUtilities::SetupBackgroundColor(HTNode(), gNavCenter->viewPressedColor, kThemeIconLabelBackgroundBrush) ) {
		Rect frame = mCachedButtonFrame;
		::InsetRect(&frame, 1, 1);
		::EraseRect(&frame);
	}
	else
		UGAAppearance::DrawGAButtonPressedBevelTint(mCachedButtonFrame);
}


//
// ImageIsReady
//
// Called when the icon is ready to draw so we can refresh accordingly.
//
void
CRDFPushButton :: ImageIsReady ( )
{
	// image dimensions have probably changed, and now that we know what they are, we
	// should recompute accordingly.
	CalcGraphicFrame();
	
	Refresh();	// for now.

} // ImageIsReady


//
// DrawStandby
//
// Called when the image we want to draw has not finished loading. We get
// called to draw something in its place. Any good ideas?
//
void
CRDFPushButton :: DrawStandby ( const Point & inTopLeft, IconTransformType inTransform ) const
{

} // DrawStandby


void 
CRDFPushButton :: MouseEnter ( Point /*inPortPt*/, const EventRecord & /*inMacEvent*/ )
{
	mMouseInFrame = true;
	if (IsActive() && IsEnabled())
		Refresh();
}


void 
CRDFPushButton :: MouseWithin ( Point /*inPortPt*/, const EventRecord & /*inMacEvent*/ )
{
	// Nothing to do for now
}


void 
CRDFPushButton :: MouseLeave( )
{
	if ( !IsMouseInFrame() )
		return;
	
	mMouseInFrame = false;
	if (IsActive() && IsEnabled())
		Refresh();

} // MouseLeave


//
// HotSpotAction
//
// Called when the mouse is clicked w/in this control
//
void
CRDFPushButton :: HotSpotAction(short /* inHotSpot */, Boolean inCurrInside, Boolean inPrevInside)
{
	if (inCurrInside != inPrevInside) {
		SetTrackInside(inCurrInside);
		Draw(NULL);						// draw immed. because mouse is down, can't wait for update event
	}

} // HotSpotAction


void
CRDFPushButton :: HotSpotResult(Int16 inHotSpot)
{
	const char* url = HT_GetNodeURL(HTNode());
	if ( url && strncmp(url, "command:", 8) == 0 )
	{
		// We're a command, baby.  Look up our FE command and execute it.
		HotSpotAction ( 0, false, true );
		BroadcastValueMessage();
	}
	else if ( !HT_IsContainer(HTNode()) && !HT_IsSeparator(HTNode()) )
	{
		// we're a plain old url (personal toolbar style). Launch it if HT says we're allowed to.
		if ( !URDFUtilities::LaunchNode(HTNode()) )
			CFrontApp::DoGetURL( url );
	}
	else {
	
		// we're a popdown treeview. Open it the way it wants to be opened
		// convert local to port coords for this cell
		Rect portRect;
		CalcPortFrameRect ( portRect );
		
		// find the Browser window and tell it to show a popdown with
		// the give HT_Resource for this cell
		LView* top=GetSuperView();
		while ( top && top->GetSuperView() )
			top = top->GetSuperView();

		// popdown the tree			
		CBrowserWindow* browser = dynamic_cast<CBrowserWindow*>(top);
		Assert_(browser != NULL);
		if ( browser ) {
			switch ( HT_GetTreeStateForButton(HTNode()) ) {
				
				case HT_DOCKED_WINDOW:
					browser->OpenDockedTreeView ( HTNode() );
					break;
					
				case HT_STANDALONE_WINDOW:		
					LCommander::GetTopCommander()->ProcessCommand(cmd_NCOpenNewWindow, HTNode() );
					break;
				
				case HT_POPUP_WINDOW:			
					browser->PopDownTreeView ( portRect.left, portRect.bottom + 5, HTNode() );
					break;
				
				default:
					DebugStr("\pHT_GetTreeStateForButton returned unknown type");
					break;
			} // case of which tree type to create
		} // if browser window found
		
	} // else create a tree
	
} // HotSpotResult


//
// DoneTracking
//
// Reset the toolbar back to its original state.
//
void
CRDFPushButton :: DoneTracking ( SInt16 inHotSpot, Boolean inGoodTrack )
{		
	SetTrackInside(false);

	if ( inGoodTrack )
		Refresh();
	else
		MouseLeave();		// mouse has left the building. Redraw the correct state now, not later
}


//
// EnableSelf
// DisableSelf
//
// Override to redraw immediately when enabled or disabled.
//

void 
CRDFPushButton :: EnableSelf ( )
{
	if (FocusExposed()) {
		FocusDraw();
		Draw(NULL);
	}
}

void 
CRDFPushButton :: DisableSelf ( )
{
	if (FocusExposed()) {
		FocusDraw();
		Draw(NULL);
	}
}


#pragma mark -


CRDFSeparator :: CRDFSeparator ( HT_Resource inNode )
	: CRDFToolbarItem(inNode)
{


}


CRDFSeparator :: ~CRDFSeparator ( )
{



}


// a strawman drawing routine for testing purposes only
void
CRDFSeparator :: DrawSelf ( )
{
	Rect localRect;
	CalcLocalFrameRect ( localRect );

	::PaintRect ( &localRect );
}


//
// NaturalSize
// (See comment on CRDFPushButton::NaturalSize() for tons of info.)
//
// Return the appropriate size for a separator.
//
SDimension16
CRDFSeparator :: NaturalSize( SDimension16 inAvailable ) const
{
	SDimension16 desiredSpace;
	desiredSpace.width = 5; 
	desiredSpace.height = 24;
	
	return desiredSpace;
	 
} // NaturalSize


#pragma mark -


CRDFURLBar :: CRDFURLBar ( HT_Resource inNode )
	: CRDFToolbarItem(inNode)
{

}


CRDFURLBar :: ~CRDFURLBar ( )
{



}


//
// FinishCreate
//
// Called after this item has been placed into the widget hierarchy. Reanimate the
// url bar from a PPob. This needs to be done here (and not in the constructor)
// because some of its components (proxy icon, etc) throw/assert if they are not
// part of a window at creation time.
//
void
CRDFURLBar :: FinishCreate ( )
{
	LWindow* window = LWindow::FetchWindowObject(GetMacPort());
	LView* view = UReanimator::CreateView(1104, this, window);	// create the url bar

	// chances are good that if this button streams in after the window has been
	// created, the url bar will have not been hooked up to the current context. Doing
	// it here ensures it will.
	CBrowserWindow* browser = dynamic_cast<CBrowserWindow*>(window);
	if ( browser )
		browser->HookupContextToToolbars();
		
} // FinishCreate


//
// NaturalSize
// (See comment on CRDFPushButton::NaturalSize() for tons of info.)
//
// Return the smallest size we can live in.
//
SDimension16
CRDFURLBar::NaturalSize( SDimension16 inAvailable ) const
{
	SDimension16 desiredSpace;
	desiredSpace.width = 100; desiredSpace.height = 50;		//еее is this enough?
	
	return desiredSpace;
	 
} // NaturalSize


//
// IsStretchy
//
// 
bool
CRDFURLBar :: IsStretchy ( ) const 
{
	return true;		//еее do this right.
}
