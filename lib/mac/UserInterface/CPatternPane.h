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

#ifndef CPatternPane_H
#define CPatternPane_H
#pragma once

// Includes

#include <LPane.h>

#include "CSharedPatternWorld.h"

// Class declaration

class CPatternPane : public LPane
{
public:
	enum { class_ID = 'PtPn' };
	
	typedef LPane super;
	
							CPatternPane(LStream* inStream);
	virtual					~CPatternPane();

protected:
	virtual void			DrawSelf();
	virtual void			AdornWithBevel(const Rect& inFrameToBevel);
	
			//	We redraw on Activate/Deactivate/Enable/Disable in order to play well
			//	with the darn CBevelView stuff.
	virtual void			ActivateSelf()		{ Draw(nil); }
	virtual void			DeactivateSelf()	{ Draw(nil); }
	
	CSharedPatternWorld::EPatternOrientation	mOrientation;
	Boolean										mAdornWithBevel;
	Boolean										mDontFill;
	CSharedPatternWorld*						mPatternWorld;
};

#endif