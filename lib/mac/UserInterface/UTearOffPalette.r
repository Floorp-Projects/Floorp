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

/* The standard window resource for palette windows */

#define SystemSevenOrLater 1

#include <Types.r>

#include <PowerPlant.r>

#define FLOAT_ID -19844
#define SIDE_FLOAT_ID -19845

resource 'WIND' (FLOAT_ID , "", purgeable) {
	{22, 380, 72, 680},
	1989, invisible, goAway,
	0x0,	// refCon
	"Floater",
	noAutoCenter
};

resource 'PPob' (FLOAT_ID, "Floater") { {
	ClassAlias { 'DRFW' },
	ObjectData { Window {
		FLOAT_ID, floating,
		hasCloseBox, hasTitleBar, noResize, noSizeBox, noZoom, noShowNew,
		enabled, noTarget, hasGetSelectClick, hasHideOnSuspend, noDelaySelect,
		hasEraseOnUpdate,
		0, 0,						//	Minimum width, height
		screenSize, screenSize,		//	Maximum width, height
		screenSize, screenSize,		//	Standard width, height
		0							//	UserCon
	} }
} };

resource 'WIND' (SIDE_FLOAT_ID , "", purgeable) {
	{22, 380, 72, 680},
	1997, invisible, goAway,
	0x0,	// refCon
	"Floater",
	noAutoCenter
};

resource 'PPob' (SIDE_FLOAT_ID, "Floater") { {
	ClassAlias { 'DRFW' },
	ObjectData { Window {
		SIDE_FLOAT_ID, floating,
		hasCloseBox, hasTitleBar, noResize, noSizeBox, noZoom, noShowNew,
		enabled, noTarget, hasGetSelectClick, hasHideOnSuspend, noDelaySelect,
		hasEraseOnUpdate,
		0, 0,						//	Minimum width, height
		screenSize, screenSize,		//	Maximum width, height
		screenSize, screenSize,		//	Standard width, height
		0							//	UserCon
	} }
} };
