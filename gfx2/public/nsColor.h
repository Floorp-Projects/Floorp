/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

#ifndef nsColor_h___
#define nsColor_h___

#include "gfxtypes.h"

class nsString;

/**
 * Color utilities and conversion functions
 * @file nsColor.h
 */

/**
 * Make a color out of r,g,b values. This assumes that the r,g,b values are
 * properly constrained to 0-255. This also assumes that a is 255.
 */
#define NS_RGB(_r,_g,_b) \
  ((gfx_color) ((255 << 24) | ((_b)<<16) | ((_g)<<8) | (_r)))

/**
 * Make a color out of r,g,b,a values. This assumes that the r,g,b,a
 * values are properly constrained to 0-255.
 */
#define NS_RGBA(_r,_g,_b,_a) \
  ((gfx_color) (((_a) << 24) | ((_b)<<16) | ((_g)<<8) | (_r)))

/**
 * Extract color components from gfx_color
 */
//@{
#define NS_GET_R(_rgba) ((PRUint8) ((_rgba) & 0xff))
#define NS_GET_G(_rgba) ((PRUint8) (((_rgba) >> 8) & 0xff))
#define NS_GET_B(_rgba) ((PRUint8) (((_rgba) >> 16) & 0xff))
#define NS_GET_A(_rgba) ((PRUint8) (((_rgba) >> 24) & 0xff))
//@}

/**
 * Translate a hex string to a color. Return true if it parses ok,
 * otherwise return false.
 * This accepts only 3, 6 or 9 digits
 */
extern "C" NS_GFX_(PRBool) NS_HexToRGB(const nsString& aBuf,
                                       gfx_color* aResult);

/**
 * Translate a hex string to a color. Return true if it parses ok,
 * otherwise return false.
 * This version accepts 1 to 9 digits (missing digits are 0)
 */
extern "C" NS_GFX_(PRBool) NS_LooseHexToRGB(const nsString& aBuf,
                                            gfx_color* aResult);

/**
 * Translate a color name to a color. Return true if it parses ok,
 * otherwise return false.
 */
extern "C" NS_GFX_(PRBool) NS_ColorNameToRGB(const nsString& aBuf,
                                             gfx_color* aResult);

/**
 * Weird color computing code stolen from winfe which was stolen
 * from the xfe which was written originally by Eric Bina. So there.
 */
extern "C" NS_GFX_(void) NS_Get3DColors(gfx_color aResult[2],
                                        gfx_color aColor);

/**
 * To determin colors based on the background brightness
 */
extern "C" NS_GFX_(void) NS_GetSpecial3DColors(gfx_color aResult[2],
                                               gfx_color aBackgroundColor,
                                               gfx_color aBorderColor);

/**
 * Special method to brighten a Color and have it shift to white when
 * fully saturated.
 */
extern "C" NS_GFX_(gfx_color) NS_BrightenColor(gfx_color inColor);

/**
 * Special method to darken a Color and have it shift to black when
 * darkest component underflows
 */
extern "C" NS_GFX_(gfx_color) NS_DarkenColor(gfx_color inColor);

/**
 * Determins brightness for a specific color
 */
extern "C" NS_GFX_(int) NS_GetBrightness(PRUint8 aRed,
                                         PRUint8 aGreen,
                                         PRUint8 aBlue);

#endif /* nsColor_h___ */
