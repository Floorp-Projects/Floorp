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