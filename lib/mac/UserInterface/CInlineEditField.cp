/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 17 DEC 96

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CInlineEditField.h"


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/


/*======================================================================================
	Set the descriptor and adjust the frame size to the new text.
======================================================================================*/

void CInlineEditField::SetDescriptor(ConstStr255Param inDescriptor) {

	Inherited::SetDescriptor(inDescriptor);
	
	
	if (mGrowableBorder)
		AdjustFrameWidthToText();
		
	mOriginalName = inDescriptor;
}


/*======================================================================================
	Update the text and edit location for this field. inEditText specifies the text to
	start editing, inImageTopLeft is the top, left corner for the edit field in the
	image coordinates of its superview, ioMouseDown specifies the mouse click location
	if the edit field is activated by a mouse click, or nil to just select all of the
	edit field text. ioMouseDown can be an exact copy of the SMouseDownEvent passed to
	ClickSelf() or ClickSelect().
	
	Pass inEditText == nil to hide the edit field and update the port immediately.
======================================================================================*/

void CInlineEditField::UpdateEdit(ConstStr255Param inEditText, const SPoint32 *inImageTopLeft,
								  SMouseDownEvent *ioMouseDown) {

	Boolean wasVisible = IsVisible();
	
	if ( inEditText ) {
	
		Assert_(inImageTopLeft != nil);
		
		SetDescriptor(inEditText);
		Show();
		if ( wasVisible ) {
			Refresh();
		} else {
			DontRefresh();
		}
		PlaceInSuperImageAt(inImageTopLeft->h, inImageTopLeft->v, false);
		if ( wasVisible ) UpdatePort();
		if ( !ioMouseDown ) {
			FocusDraw();
			//StEmptyVisRgn emptyRgn(GetMacPort());
			::TESetSelect(0, max_Int16, mTextEditH);
		}
		Draw(nil);
		
		sLastPaneClicked = nil;
		(**mTextEditH).clickTime = 0;	// No double clicks in new field
		
		if ( ioMouseDown != nil ) {
			ioMouseDown->whereLocal = ioMouseDown->wherePort;
			Click(*ioMouseDown);
		} else {
			SwitchTarget(this);
		}
	} else {
		if ( wasVisible ) {
			Hide();
			UpdatePort();
		}
	}
}



/*======================================================================================
	Finish creating the edit field.
======================================================================================*/

void CInlineEditField::FinishCreateSelf(void) {
	
	Inherited::FinishCreateSelf();
	
	Hide();	// Should be invisible to start

	// Resize the pane so that it fits the text vertically
	
	Int16 height = (**mTextEditH).lineHeight;

	if ( mHasBox ) height += (cEditBoxMargin<<1);
	
	ResizeFrameTo(mFrameSize.width, height, false);
}


/*======================================================================================
	Refresh the border as well.
======================================================================================*/

void CInlineEditField::ResizeFrameBy(Int16 inWidthDelta, Int16 inHeightDelta, Boolean inRefresh) {

	if ( !inWidthDelta && !inHeightDelta ) return;
	
	Inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	
	if ( inRefresh && mGrowableBorder) {
		Rect portRect, refreshRect;
		CalcPortFrameRect(portRect);
		if ( inWidthDelta != 0 ) {
			refreshRect = portRect;
			if ( inWidthDelta > 0 ) {
				refreshRect.right -= inWidthDelta; 
				refreshRect.left = refreshRect.right - cRefreshMargin;
			} else {
				refreshRect.left = refreshRect.right - cRefreshMargin;
			}
			InvalPortRect(&refreshRect);
		}
		if ( inHeightDelta != 0 ) {
			refreshRect = portRect;
			if ( inHeightDelta > 0 ) {
				refreshRect.bottom -= inWidthDelta; 
				refreshRect.top = refreshRect.bottom - cRefreshMargin;
			} else {
				refreshRect.top = refreshRect.bottom - cRefreshMargin;
			}
			InvalPortRect(&refreshRect);
		}
	}
}


/*======================================================================================
	Handle returns, enters, tabs.
======================================================================================*/

Boolean CInlineEditField::HandleKeyPress(const EventRecord &inKeyEvent) {

	if ( !(inKeyEvent.modifiers & cmdKey) ) {
		Int16 theKey = inKeyEvent.message & charCodeMask;
		
		switch ( theKey ) {
			
			// when the user hits escape, set the name back to the original and fall
			// through to broadcast the "change" and hide the pane.
			case char_Escape:
					SetDescriptor(mOriginalName);
					
			case char_Enter:
			case char_Return: {
					PaneIDT paneID = mPaneID;
					BroadcastMessage(msg_HidingInlineEditField, &paneID);
					StopBroadcasting();			// don't broadcast, else we'll end
												// up sending another message when we hide the pane.
					UpdateEdit(nil, nil, nil);	// Remove editing
					StartBroadcasting();
					return true;				// keypress handled
				}
				break;
		}
	}
	
	return Inherited::HandleKeyPress(inKeyEvent);
}


/*======================================================================================
	We are losing target status.
======================================================================================*/

void CInlineEditField::DontBeTarget(void) {

	StValueChanger<Boolean> change(mGivingUpTarget, true);

	Inherited::DontBeTarget();

	PaneIDT paneID = mPaneID;
	BroadcastMessage(msg_HidingInlineEditField, &paneID);
	
	UpdateEdit(nil, nil, nil);	// Remove editing
}


/*======================================================================================
	Set new target only if we are not giving up the target.
======================================================================================*/

void CInlineEditField::HideSelf(void) {

	if ( IsOnDuty() && !mGivingUpTarget ) {	// Shouldn't be on duty when invisible
		SwitchTarget(GetSuperCommander());
	}
}


/*======================================================================================
	Take us off duty and turn on duty to off.
======================================================================================*/

void CInlineEditField::TakeOffDuty(void) {

	Inherited::TakeOffDuty();
	
	mOnDuty = triState_Off;	// Taking our chain off duty means that we are hidden and
							// should no longer be in the chain of command.
}


/*======================================================================================
	Adjust the frame of the edit field to the width of the text.
======================================================================================*/

void CInlineEditField::UserChangedText(void) {

	if (mGrowableBorder)
	{
		AdjustFrameWidthToText();
	}
	PaneIDT paneID = mPaneID;

	BroadcastMessage(msg_InlineEditFieldChanged, &paneID);
}


/*======================================================================================
	Adjust the frame of the edit field to the bounds of the current text.
======================================================================================*/

void CInlineEditField::AdjustFrameWidthToText(void) {

	Int16 newWidth;
	GetSuperView()->EstablishPort();
	UTextTraits::SetPortTextTraits(mTextTraitsID);
	
	Str255 descriptor;
	GetDescriptor(descriptor);
	
	newWidth = ::StringWidth(descriptor) + 2;
	
	if ( mHasBox ) newWidth += (cEditBoxMargin<<1);
	
	ResizeFrameTo(newWidth, mFrameSize.height, true);
	if ( IsVisible() ) UpdatePort();	// Update immediately so there's no lag in display
}

/*
Why we have Growable Border mode (by kellys):

CInlineEditField is derived (indirectly) from CTSMEditField.  When we have growable
borders, CInlineEditField assumes it can change the border whenever the user types.
However, when the user is using a multi-byte script like Japanese, CTSMEditField
"takes over" event handling.  As a result, the border doesn't resize while they're
typing (Bug #81734).  

The proper thing to do is to change the event handling assumptions.  However, due to lack
of time, I have made a "kludge" that avoids the problem.  Under multi-byte systems,
we make the border a fixed size.  Under roman systems, we make it variable.
*/

/*======================================================================================
	Set whether or not we use a growable frame
======================================================================================*/

void CInlineEditField::SetGrowableBorder(Boolean inGrowable) {

	if (inGrowable != mGrowableBorder) {
		mGrowableBorder = inGrowable;
		// ResizeFrameTo(mFrameSize.width, mFrameSize.height, true);
		UserChangedText();
	}
}


/*======================================================================================
	Get whether or not we use a growable frame
======================================================================================*/

Boolean CInlineEditField::GetGrowableBorder() {

	return mGrowableBorder;

}
