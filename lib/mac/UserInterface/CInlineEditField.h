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

#ifndef __H_CInlineEditField
#define __H_CInlineEditField
#pragma once

/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 19 SEP 96

	DESCRIPTION:	Defines a class for performing inline editing of text fields in a 
					view. Place an instance of this class into the view in Constructor.

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LBroadcasterEditField.h>


#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

class CInlineEditField : public LBroadcasterEditField {


	typedef LBroadcasterEditField Inherited;


public:

						enum { class_ID = 'InEd' };

						CInlineEditField(LStream *inStream) :
										 LBroadcasterEditField(inStream),
										 mGivingUpTarget(false),
										 mGrowableBorder(false) {
							
							SetRefreshAllWhenResized(false);
						}
						
						// Broadcast messages
						
						enum {
							msg_HidingInlineEditField = 'hIeF'		// PaneIDT *, id of the inline edit field
																	// The edit field in losing the target and
																	// being hidden, update the real text field
																	// to the text contained in this edit field.
							
						,	msg_InlineEditFieldChanged = 'iEfC'		// PaneIDT *, The text of the inline edit field 
																	// has been modified by the user.
						};
						
						// Constants
						
						enum {
							cRefreshMargin = 3
						,	cEditBoxMargin = 2
						};
			
	virtual void		SetDescriptor(ConstStr255Param inDescriptor);
	
	virtual void		UpdateEdit(ConstStr255Param inEditText, const SPoint32 *inImageTopLeft,
								   SMouseDownEvent *ioMouseDown = nil);
								 
			void 		SetGrowableBorder(Boolean inGrowable);
			Boolean 	GetGrowableBorder();
	
protected:

	virtual void		FinishCreateSelf(void);
	virtual void		ResizeFrameBy(Int16 inWidthDelta, Int16 inHeightDelta, Boolean inRefresh);
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);
	virtual void		DontBeTarget(void);
	virtual void		HideSelf(void);
	virtual void		TakeOffDuty(void);
	virtual void		UserChangedText(void);
	
	void				AdjustFrameWidthToText(void);
	
	// Instance variables
	
	Boolean				mGivingUpTarget;
	Boolean				mGrowableBorder;
	LStr255				mOriginalName;
};




#endif // __H_CInlineEditField
