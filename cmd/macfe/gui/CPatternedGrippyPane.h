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
// CPatternedGrippyPane.h
//
// Interface for class that draws a "grippy" pattern in the pane rectangle so users know
// they can drag/click in this area. Also hilights when mouse enters (roll-over feedback).
//
// It is fully Apperance Manager savvy.
//

#pragma once 

class LPane;
class LStream;
class CSharedPatternWorld;


class CPatternedGrippyPane : public LPane
{
	public:
		
		enum { class_ID = 'PgPn' };
		
								CPatternedGrippyPane(LStream* inStream);
		virtual					~CPatternedGrippyPane();
		
		// for hilighting on mouse entry/exit
		virtual void MouseEnter ( Point inPoint, const EventRecord & inEvent ) ;
		virtual void MouseLeave ( ) ;
		
	protected:
		
		virtual	void			DrawSelf(void);

		CIconHandle				mTriangle;
		CSharedPatternWorld*	mBackPatternHilite;		// back pattern when mouse inside
		Boolean					mMouseInside;
		
		CSharedPatternWorld*	mGrippy;				// grippy pattern (gray bg)
		CSharedPatternWorld*	mGrippyHilite;			// grippy pattern hilite (blue bg)
		
}; // CPatternedGrippyPane
