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

// ===========================================================================
//	CProxyPane.h
//
// ===========================================================================

#ifndef CProxyPane_H
#define CProxyPane_H
#pragma once

// Includes

#include <LGAIconSuite.h>
#include <LListener.h>
#include <LDragAndDrop.h>

#include "CProxyCaption.h"
#include "MPaneEnablerPolicy.h"

// Forward declarations

class LView;
class CNetscapeWindow;

//
// Class declaration
//

class CProxyPane
:	public LGAIconSuite
,	public LListener
,	public LDragAndDrop
,	public MPaneEnablerPolicy
{
public:
	enum {
		class_ID = 'Bpxy',
		
		kProxyViewID			= 'Bpxv',
		kProxyPaneID			= CProxyPane::class_ID,
		kProxyTitleCaptionID = CProxyCaption::class_ID,

		kProxyIconNormalID		= 15313,
		kProxyIconMouseOverID	= 15314
		
	};

	typedef LGAIconSuite super;
							
							CProxyPane(
									LStream*		inStream);
	virtual  				~CProxyPane();

	virtual void			ListenToMessage(
									MessageT		inMessage,
									void*			ioParam);
									
	virtual void			DoDragSendData(
									FlavorType		inFlavor,
									ItemReference	inItemRef,
									DragReference	inDragRef);
										
	virtual void			Click(
									SMouseDownEvent&		inMouseDown);

	virtual void			MouseEnter(
									Point				inPortPt,
									const EventRecord&	inMacEvent);
	virtual void			MouseLeave();
	
	void					SetIconIDs(ResIDT inNormalID, ResIDT inMouseOverID)
								{	mProxyIconNormalID = inNormalID;
									mProxyIconMouseOverID = inMouseOverID;
									SetIconResourceID(inNormalID);
									Refresh(); }
	virtual void			HandleEnablingPolicy(); // required as a MPaneEnablerPolicy

protected:

	virtual void			FinishCreateSelf();

	virtual void			ActivateSelf();
	virtual void			DeactivateSelf();
						
	virtual void			ClickSelf(
									const SMouseDownEvent&	inMouseDown);

	virtual void			ToggleIcon(
									ResIDT			inResID);

	CNetscapeWindow*		mWindow;
	LView*					mProxyView;
	LCaption*				mIconText;
	
	DragSendDataUPP 		mSendDataUPP;
	Boolean					mMouseInFrame;
	ResIDT					mProxyIconNormalID;
	ResIDT					mProxyIconMouseOverID;


	virtual void			ToggleIconSelected(
									Boolean			inIconIsSelected);									
	
	// Stack-based helper class to handle toggling selected state of icon.

	class StToggleIconSelected
	{
	public:
		StToggleIconSelected(CProxyPane& inProxyPane, Boolean inSelected)
			:	mProxyPane(inProxyPane),
				mSelectedState(inSelected)
		{
			mProxyPane.ToggleIconSelected(mSelectedState);
		}
		
		~StToggleIconSelected()
		{
			mProxyPane.ToggleIconSelected(!mSelectedState);
		}
		
	protected:
		CProxyPane&				mProxyPane;
		Boolean					mSelectedState;
	};
	
	friend class CProxyPane::StToggleIconSelected;
};


#endif
