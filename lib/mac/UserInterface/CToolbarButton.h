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

// This is a subclass of CButton that can draw itself in three states:
// text and icon, text only, or icon only.

// ******** NOTE! ********
// So that we don't disturb any PPob's, this class assumes that the size of
// the button in the PPob is the size to use for the text and icon mode.
// For the other modes, there are "magic" constants (in CToolbarButton.cp)
// that define the frame dimensions of the button.

#pragma once

#include "CButton.h"

class CToolbarButton
	:	public CButton
{
	public:

		// button types enum is in CToolbarModeManager.h
		
		enum {	class_ID = 'TbBt' };
		
							CToolbarButton(LStream* inStream);
		virtual				~CToolbarButton();

		virtual Boolean		ChangeMode(Int8 newMode, SDimension16& outDimensionDeltas);

	protected:
		virtual	void		DrawSelf();
		virtual void		FinishCreateSelf();

		virtual	void		DrawButtonTitle(void);

		virtual	void		CalcTitleFrame(void);

		Int8		mCurrentMode;
		Int16		mOriginalWidth;
		Int16		mOriginalHeight;
};