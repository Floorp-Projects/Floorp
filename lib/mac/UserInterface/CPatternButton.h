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

#include "CToolbarButton.h"

class CSharedPatternWorld;

class CPatternButton : public CToolbarButton
{
	public:

		enum { class_ID = 'PtBt' };
		
							CPatternButton(LStream* inStream);
		virtual				~CPatternButton();
	
	protected:
	
		virtual void		DrawButtonContent(void);
		virtual	void		DrawButtonGraphic(void);
		virtual void		DrawButtonTitle(void);
	
		virtual void		DrawButtonNormal();
		virtual void		DrawButtonHilited();
		
		virtual void		MouseEnter(
									Point				inPortPt,
									const EventRecord	&inMacEvent);

		virtual void		MouseWithin(
									Point				inPortPt,
									const EventRecord	&inMacEvent);

		virtual void		MouseLeave(void);

		
		virtual void		CalcOrientationPoint(Point& outPoint);

		Boolean				IsMouseInFrame(void) const;
		
		CSharedPatternWorld* mPatternWorld;
		Boolean				mMouseInFrame;
		Int16				mOrientation;
};

inline Boolean CPatternButton::IsMouseInFrame(void) const
	{	return mMouseInFrame;		}

