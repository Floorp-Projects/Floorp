/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef CarbonHelpers_h__
#define CarbonHelpers_h__

#include <ConditionalMacros.h>

#if (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
#include <ControlDefinitions.h>
#else
#include <Controls.h>
#endif
#include <Menus.h>
#include <MacWindows.h>
#include <LowMem.h>

#if UNIVERSAL_INTERFACES_VERSION < 0x0341
enum {
  inToolbarButton               = 13    	/* Mac OS X forward*/
};
enum {
  kWindowToolbarButtonAttribute = (1L << 6)	/* this window has a toolbar button widget in the titlebar (Mac OS X only)*/
};
#endif

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

inline void SetControlPopupMenuStuff ( ControlHandle control, MenuHandle menu, short aID )
{
	#if TARGET_CARBON
		::SetControlPopupMenuHandle ( control, menu );
		::SetControlPopupMenuID ( control, aID );
	#else
		PopupPrivateData* popupData = (PopupPrivateData*)*((*control)->contrlData);
		if (popupData) {
			popupData->mHandle = menu;
			popupData->mID = aID;
		}
	#endif
}


inline WindowRef GetTheWindowList(void)
{
#if TARGET_CARBON
  return GetWindowList();
#else
  return LMGetWindowList();
#endif
}


#if !TARGET_CARBON

inline void GetPortHiliteColor(CGrafPtr port, RGBColor* color)
{
	// is this really a color grafport?
	if (port->portVersion & 0xC000)
	{
		GrafVars** grafVars = (GrafVars**)port->grafVars;
		*color = (*grafVars)->rgbHiliteColor;
	}
	else
	{
		RGBColor fakeColor = { 0x0000, 0x0000, 0x0000};
		*color = fakeColor;
	}
}

inline Boolean IsPortOffscreen(CGrafPtr port)
{
  return ((UInt16)port->portVersion == 0xC001);
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

inline void GetPortVisibleRegion(CGrafPtr port, RgnHandle visRgn)
{
	::CopyRgn(port->visRgn, visRgn);
}

inline void GetPortClipRegion(CGrafPtr port, RgnHandle clipRgn)
{
	::CopyRgn(port->clipRgn, clipRgn);
}

inline short GetPortTextFace(CGrafPtr port)
{
	return port->txFace;
}

inline short GetPortTextFont(CGrafPtr port)
{
	return port->txFont;
}

inline short GetPortTextSize(CGrafPtr port)
{
	return port->txSize;
}

inline Rect* GetPortBounds(CGrafPtr port, Rect* portRect)
{
	*portRect = port->portRect;
	return portRect;
}

inline PixMapHandle GetPortPixMap(CGrafPtr port)
{
	return port->portPixMap;
}

inline Boolean IsRegionRectangular(RgnHandle rgn)
{
	return (**rgn).rgnSize == 10;
}

inline GrafPtr GetQDGlobalsThePort()
{
  return qd.thePort;
}

inline const BitMap * GetPortBitMapForCopyBits(CGrafPtr port)
{
    return &((GrafPtr)port)->portBits;
}

inline OSErr AEGetDescData (const AEDesc * theAEDesc, void * dataPtr, Size maximumSize)
{
    ::BlockMoveData(*(theAEDesc->dataHandle), dataPtr, maximumSize);
    return noErr;
}


#endif /* !TARGET_CARBON */

#endif /* CarbonHelpers_h__ */
