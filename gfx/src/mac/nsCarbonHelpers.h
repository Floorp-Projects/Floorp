/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   - Mike Pinkerton, Netscape Communications
 */

#ifndef CarbonHelpers_h__
#define CarbonHelpers_h__

#include <Controls.h>
#include <Menus.h>
#include <Windows.h>

//
// for non-carbon builds, provide various accessors to keep the code below free of ifdefs.
//

inline void GetWindowUpdateRegion ( WindowPtr window, RgnHandle outUpdateRgn )
{
	#if TARGET_CARBON
		::GetWindowRegion(window, kWindowUpdateRgn, outUpdateRgn);
	#else
		::CopyRgn(((WindowRecord*)window)->updateRgn, outUpdateRgn);
	#endif
}

inline void SetControlPopupMenuStuff ( ControlHandle control, MenuHandle menu, short id )
{
	#if TARGET_CARBON
		::SetControlPopupMenuHandle ( control, menu );
		::SetControlPopupMenuID ( control, id );
	#else
		PopupPrivateData* popupData = (PopupPrivateData*)*((*control)->contrlData);
		if (popupData) {
			popupData->mHandle = menu;
			popupData->mID = id;
		}
	#endif
}


#if !TARGET_CARBON

inline void GetPortHiliteColor ( GrafPtr port, RGBColor* color )
{
	GrafVars** grafVars = (GrafVars**)((CGrafPtr)port)->grafVars;
	*color = (*grafVars)->rgbHiliteColor;
}

inline void DisposeAEEventHandlerUPP ( RoutineDescriptor* proc )
{
	::DisposeRoutineDescriptor(proc);
}

inline void DisposeNavEventUPP ( RoutineDescriptor* proc )
{
	::DisposeRoutineDescriptor(proc);
}

inline Rect* GetRegionBounds(RgnHandle region, Rect* rect)
{
	*rect = (**region).rgnBBox;
	return rect;
}

inline Boolean IsRegionComplex(RgnHandle region)
{
	return (**region).rgnSize != sizeof(MacRegion);
}

inline Rect* GetWindowPortBounds(WindowRef window, Rect* rect)
{
	*rect = ((GrafPtr)window)->portRect;
	return rect;
}

inline void GetPortVisibleRegion(GrafPtr port, RgnHandle visRgn)
{
	::CopyRgn(port->visRgn, visRgn);
}


#endif /* !TARGET_CARBON */


#endif /* CarbonHelpers_h__ */