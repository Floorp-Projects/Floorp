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
// CToolbarBevelButton
// Mike Pinkerton, Netscape Communications
//
// A bevel button that knows how to change modes (text, icon, icon & text)
//

#pragma once

#include "LCmdBevelButton.h"


class CToolbarBevelButton
	:	public LCmdBevelButton
{
	public:
		enum {
			eMode_IconsOnly = 0,
			eMode_TextOnly,
			eMode_IconsAndText
		};

		enum {	defaultMode = eMode_IconsAndText	};

		enum {	class_ID = 'TbBv', imp_class_ID = 'TbBi' };
		
							CToolbarBevelButton(LStream* inStream);
		virtual				~CToolbarBevelButton();

		virtual Boolean		ChangeMode(Int8 newMode, SDimension16& outDimensionDeltas);

	protected:
		virtual void		FinishCreateSelf();

			// used during mode switches that involve the title appearing/disappearing
		virtual void		SaveTitle();
		virtual void		RestoreTitle();
		
		Int8		mCurrentMode;
		Int16		mOriginalWidth;
		Int16		mOriginalHeight;
		
		LStr255*	mOriginalTitle;		// saved if we go into icon only mode
		ResIDT		mResID;				// saved if we go into text only mode
};