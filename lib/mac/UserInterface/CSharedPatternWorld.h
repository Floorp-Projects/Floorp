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

