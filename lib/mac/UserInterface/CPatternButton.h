/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

