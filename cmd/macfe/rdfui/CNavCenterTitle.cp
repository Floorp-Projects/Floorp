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
// Class that draws the header area for the nav center which shows the title
// of the currently selected view as well as harboring the closebox for
// an easy way to close up the navCenter shelf.
//

#include "CNavCenterTitle.h"
#include "URDFUtilities.h"
#include "CContextMenuAttachment.h"
#include "CRDFCoordinator.h"


CNavCenterStrip :: CNavCenterStrip ( LStream *inStream )
	: CGrayBevelView (inStream),
		mView(NULL)
{

}


CNavCenterStrip :: ~CNavCenterStrip()
{
	// nothing to do 
}


//
// FindCommandStatus
//
// Respond to commands from the context menu for Aurora popup-location.
//
void
CNavCenterStrip :: FindCommandStatus ( CommandT inCommand, Boolean &outEnabled,
										Boolean	&outUsesMark, Char16 &outMark, Str255 outName)
{
	outEnabled = true;
	outUsesMark = false;	//еее expand later to mark the current state

} // FindCommandStatus


//
// ClickSelf
//
// Handle context-clicks by passing them off to the attachement (if present)
//
void
CNavCenterStrip :: ClickSelf ( const SMouseDownEvent & inMouseDown )
{
	CContextMenuAttachment::SExecuteParams params;
	params.inMouseDown = &inMouseDown;
	
	ExecuteAttachments ( CContextMenuAttachment::msg_ContextMenu, (void*)&params );

} // ClickSelf


//
// ListenToMessage
//
// We want to know when the selected workspace changes so that we can update the
// title string, etc. The RDFCoordinator sets us up as a listener which will broadcast,
// when things change.
//
void
CNavCenterStrip :: ListenToMessage ( MessageT inMessage, void* ioParam ) 
{
	switch ( inMessage ) {
	
		case CRDFCoordinator::msg_ActiveSelectorChanged:
		{
			mView = reinterpret_cast<HT_View>(ioParam);
			if ( mView )
				SetView(mView);

			// if we're in the middle of a drag and drop, draw NOW, not
			// when we get a refresh event.
			if ( ::StillDown() ) {
				FocusDraw();
				Draw(nil);
			}
		}
		
		default:
			CTiledImageMixin::ListenToMessage ( inMessage, ioParam );
	
	} // case of which message

} // ListenToMessage


//
// AdjustCursorSelf
//
// Handle changing cursor to contextual menu cursor if cmd key is down
//
void
CNavCenterStrip :: AdjustCursorSelf( Point /*inPoint*/, const EventRecord& inEvent )
{
	ExecuteAttachments(CContextMenuAttachment::msg_ContextMenuCursor, 
								static_cast<void*>(const_cast<EventRecord*>(&inEvent)));

} // AdjustCursorSelf


//
// DrawBeveledFill
//
// If the HT_View currently has a bg image specified, draw it (or at least kick off the load)
// otherwise try to erase with the specified bg color.
//
void
CNavCenterStrip :: DrawBeveledFill ( )
{
	StColorState saved;
	
	if ( mView ) {
		HT_Resource topNode = HT_TopNode(GetView());
		if ( topNode ) {
			char* url = NULL;
			PRBool success = HT_GetTemplateData ( topNode, BackgroundURLProperty(), HT_COLUMN_STRING, &url );
			if ( success && url ) {
				// draw the background image tiled to fill the whole pane
				Point topLeft = { 0, 0 };
				SetImageURL ( url );
				DrawImage ( topLeft, kTransformNone, mFrameSize.width, mFrameSize.height );
				FocusDraw();
			}
			else 
				EraseBackground(topNode);
		}
		else
			EraseBackground(NULL);
	}
	else
		EraseBackground(NULL);

} // DrawBeveledFill


//
// ImageIsReady
//
// Called when the bg image is done loading and is ready to draw. Force a redraw and
// we'll pick it up
//
void
CNavCenterStrip :: ImageIsReady ( )
{
	Refresh();

} // ImageIsReady


//
// DrawStandby
//
// Draw correctly when the image has yet to load.
//
void
CNavCenterStrip :: DrawStandby ( const Point & /*inTopLeft*/, 
									IconTransformType /*inTransform*/ ) const
{
	// we're just waiting for the image to come in, who cares if we don't use
	// HT's color?
	EraseBackground(NULL);

} // DrawStandby


//
// EraseBackground
//
// Fill in the bg with either the correct HT color (from a property on |inTopNode|, the
// correct AM color, or the default GA implementation (if we are before AM 1.1).
//
void
CNavCenterStrip :: EraseBackground ( HT_Resource inTopNode ) const
{
	// when we can get the right AM bg color (in AM 1.1), use that but for now just ignore it
	if ( !inTopNode || ! URDFUtilities::SetupBackgroundColor(inTopNode, BackgroundColorProperty(),
											kThemeListViewSortColumnBackgroundBrush) ) {
		CNavCenterStrip* self = const_cast<CNavCenterStrip*>(this);		// hack
		self->CGrayBevelView::DrawBeveledFill();
	}
	else {
		// use HT's color
		Rect bounds;
		CalcLocalFrameRect(bounds);
		::EraseRect(&bounds);
	}
} // EraseBackground


#pragma mark -


CNavCenterTitle :: CNavCenterTitle ( LStream *inStream )
	: CNavCenterStrip (inStream),
		mTitle(NULL)
{

}


CNavCenterTitle :: ~CNavCenterTitle()
{
	// nothing to do 
}


//
// FinishCreateSelf
//
// Last minute setup stuff....
//
void
CNavCenterTitle :: FinishCreateSelf ( )
{
	mTitle = dynamic_cast<CChameleonCaption*>(FindPaneByID(kTitlePaneID));
	Assert_(mTitle != NULL);
	
} // FinishCreateSelf



//
// SetView
//
// Set the caption to the title of the current view
//
void
CNavCenterTitle :: SetView ( HT_View inView )
{
	// do not delete |buffer|
	const char* buffer = HT_GetViewName ( GetView() );
	TitleCaption().SetDescriptor(LStr255(buffer));
	
	RGBColor textColor;
	if ( URDFUtilities::GetColor(HT_TopNode(GetView()), ForegroundColorProperty(), &textColor) )
		TitleCaption().SetColor ( textColor );
		
} // SetView


#pragma mark -


CNavCenterCommandStrip :: CNavCenterCommandStrip ( LStream *inStream )
	: CNavCenterStrip (inStream),
		mAddPage(NULL), mManage(NULL), mClose(NULL)
{

}


CNavCenterCommandStrip :: ~CNavCenterCommandStrip()
{
	// nothing to do 
}


//
// FinishCreateSelf
//
// Last minute setup stuff....
//
void
CNavCenterCommandStrip :: FinishCreateSelf ( )
{
	// none of these are always present
	mClose = dynamic_cast<CChameleonCaption*>(FindPaneByID(kClosePaneID));
	mAddPage = dynamic_cast<CChameleonCaption*>(FindPaneByID(kAddPagePaneID));
	mManage = dynamic_cast<CChameleonCaption*>(FindPaneByID(kManagePaneID));
	
} // FinishCreateSelf


//
// SetView
//
// Update the text colors to those of the current view
//
void
CNavCenterCommandStrip :: SetView ( HT_View inView )
{
	RGBColor textColor;
	if ( URDFUtilities::GetColor(HT_TopNode(GetView()), ForegroundColorProperty(), &textColor) ) {
		if ( mClose ) mClose->SetColor ( textColor );
		if ( mAddPage ) mAddPage->SetColor ( textColor );
		if ( mManage ) mManage->SetColor ( textColor );
	}

} // SetView