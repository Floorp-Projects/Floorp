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


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This class provides two very important functions.  First provides a
//	mechanism for which multiple panes can share the same pattern without
//	duplicating the pattern data for each instance.
//
//	Second, this class provides a method of drawing that pattern relative
//	to any point.
//
//	The internals of this class distinguish between patterns that are
//	8-bits or less in depth from those that are 16 or 32 bits deep.  This
//	is necessary because QuickDraw draws the patterns differently depending
//	on that criteria.
//
//	In order to draw an "shallow" (those <= 8 bits deep) you simply need to
//	offset the target port origin the appropriate ammount and then fill.
//
//	"Deep" patterns (those > 8 bits deep) can not be predictably drawn
//	relative to a specific point in the destination port.  No matter what
//	you do to the port origin, the pattern will continue to be drawn from
//	some global relative point.  As I recall there's some low memory global
//	that denotes the offset.  Even if you get the low memory global and try
//	to compute a global-relative transformation, your pattern will not
//	necessarily blit in the right orientation.  Video card hardware can
//	accelerate the pattern drawing, and may not use the same relative
//	point.  Sounds like fun, huh?
//
//	I just thought I'd document this so when you look at the code you don't
//	think: "gee, why didn't he just use FillCRect() for all the patterns?"
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LSharable.h>
#include <LArray.h>
#include "CGWorld.h"

const ResIDT cUtilityPatternResID = 42;

class CGWorld;

class CSharedPatternWorld : public LSharable
{
	public:

		enum EPatternOrientation {
			eOrientation_Self = 0,
			eOrientation_Superview,
			eOrientation_Port
		};

		static CSharedPatternWorld* CreateSharedPatternWorld(ResIDT inPatternID);
		
		static void 		CalcRelativePoint(
								LPane*					inForPane,
								EPatternOrientation		inOrientation,
								Point& 					outPoint);

		void				Fill(
								CGrafPtr				inPort,
								const Rect& 			inBounds,
								Point					inAlignTo);

		void				Fill(
								CGrafPtr				inPort,
								RgnHandle 				inRegion,
								Point					inAlignTo);
		
		static Boolean		sUseUtilityPattern;

	protected:
	
							CSharedPatternWorld(ResIDT inPatternID);
		virtual				~CSharedPatternWorld();

		Boolean				IsShallow(void) const;
		
		void				FillShallow(
								CGrafPtr				inPort,
								const Rect& 			inBounds,
								Point					inAlignTo);

		void				FillDeep(
								CGrafPtr				inPort,
								const Rect& 			inBounds,
								Point					inAlignTo);
		

		Boolean				mIsShallow;				// true if depth <= 8 bits.
		CGWorld*			mPatternWorld;			// valid if NOT shallow
		PixPatHandle		mPatternHandle;			// valid if shallow
		ResIDT				mPatternID;

		static LArray*		sPatterns;

};

//inline CSharedPatternWorld::operator GWorldPtr()
//	{	return mPatternWorld->GetMacGWorld(); 	}


inline Boolean CSharedPatternWorld::IsShallow(void) const
	{	return mIsShallow;						}

