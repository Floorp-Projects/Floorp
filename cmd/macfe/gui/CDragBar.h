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

#pragma once

#include <LView.h>
#include <LBroadcaster.h>
#include <URegions.h>

#include "CGWorld.h"

class CSharedPatternWorld;

const FlavorType	Flavor_DragBar			= 'dgdb';
const MessageT 		msg_DragBarCollapse 	= 'dgbc';	// CDragBar*
const MessageT		msg_DragBarExpand		= 'dbex';	// CDragBar*

class CDragBar :
			public LView,
			public LBroadcaster
{
		friend class CDragBarDockControl;
	public:

		enum { class_ID = 'DgBr' };
		
								CDragBar(LStream* inStream);
		virtual					~CDragBar();
		
		virtual	void			Dock(void);
		virtual	void			Undock(void);
		virtual	Boolean			IsDocked(void) const;

		virtual	void			StartTracking(void);
		virtual	void			StopTracking(void);

		virtual StringPtr		GetDescriptor(Str255 outDescriptor) const;
		virtual void			SetDescriptor(ConstStringPtr inDescriptor);
		
		virtual void			Draw(RgnHandle inSuperDrawRgnH);
		virtual void			Click(SMouseDownEvent& inMouseDown);
	
		virtual void			SetAvailable(Boolean inAvailable); // for javascript
		
		virtual Boolean			IsAvailable(); // for javascript
	protected:
		
		virtual	void			DrawSelf(void);

		virtual	void			ClickDragSelf(const SMouseDownEvent& inMouseDown);

		TString<Str255>			mTitle;
		CSharedPatternWorld* 	mPatternWorld;
		StRegion				mDockedMask;
		Boolean					mIsDocked;
		Boolean					mIsAvailable; // for javascript
		Boolean					mIsTracking;
};
	

