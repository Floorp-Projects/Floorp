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
