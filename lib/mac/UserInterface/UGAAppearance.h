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

// This class is used to draw standard GA appearance bevels (Gray Appearance Appearance).
// The main reason for this class is to draw the GA appearance using arithmetic
// mode tints to support our pattern background feature.

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Class UGAAppearance
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class UGAAppearance
{
	public:
		static void		DrawGAButtonBevelTint(
								const Rect		&inRect);
		static void		DrawGAButtonPressedBevelTint(
								const Rect		&inRect);
		static void		DrawGAButtonDimmedBevelTint(
								const Rect		&inRect);
		
		static void		DrawGAPopupBevelTint(
								const Rect		&inRect);
		static void		DrawGAPopupPressedBevelTint(
								const Rect		&inRect);
		static void		DrawGAPopupDimmedBevelTint(
								const Rect		&inRect);
		
	// GA Bevel tints.
	// Use these tint values to go between gray levels in GA appearance
	
		static Uint16	sGAOneGrayLevel;
		static Uint16	sGATwoGrayLevels;
		static Uint16	sGAThreeGrayLevels;
		static Uint16	sGAFourGrayLevels;
		static Uint16	sGAFiveGrayLevels;
		static Uint16	sGASixGrayLevels;
		static Uint16	sGASevenGrayLevels;

		static Uint16	sGAHiliteContentTint;
};