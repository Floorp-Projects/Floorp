/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsColor_h___
#define nsColor_h___

#include "nscore.h"

class nsString;

// A color is a 32 bit unsigned integer with four components: R, G, B
// and A.
typedef PRUint32 nscolor;

// Make a color out of r,g,b values. This assumes that the r,g,b values are
// properly constrained to 0-255. This also assumes that a is 255.
#define NS_RGB(_r,_g,_b) \
  ((nscolor) ((255 << 24) | ((_b)<<16) | ((_g)<<8) | (_r)))

// Make a color out of r,g,b,a values. This assumes that the r,g,b,a
// values are properly constrained to 0-255.
#define NS_RGBA(_r,_g,_b,_a) \
  ((nscolor) (((_a) << 24) | ((_b)<<16) | ((_g)<<8) | (_r)))

// Extract color components from nscolor
#define NS_GET_R(_rgba) ((PRUint8) ((_rgba) & 0xff))
#define NS_GET_G(_rgba) ((PRUint8) (((_rgba) >> 8) & 0xff))
#define NS_GET_B(_rgba) ((PRUint8) (((_rgba) >> 16) & 0xff))
#define NS_GET_A(_rgba) ((PRUint8) (((_rgba) >> 24) & 0xff))

// Fast approximate division by 255. It has the property that
// for all 0 <= n <= 255*255, FAST_DIVIDE_BY_255(n) == n/255.
// But it only uses two adds and two shifts instead of an 
// integer division (which is expensive on many processors).
//
// equivalent to target=v/255
#define FAST_DIVIDE_BY_255(target,v)               \
  PR_BEGIN_MACRO                                   \
    unsigned tmp_ = v;                             \
    target = ((tmp_ << 8) + tmp_ + 255) >> 16;     \
  PR_END_MACRO

// Blending macro
//
// equivalent to target=(bg*(255-alpha)+fg*alpha)/255
#define MOZ_BLEND(target, bg, fg, alpha) \
        FAST_DIVIDE_BY_255(target, (bg)*(255-(alpha)) + (fg)*(alpha))

// Translate a hex string to a color. Return true if it parses ok,
// otherwise return false.
// This accepts only 3, 6 or 9 digits
extern "C" NS_GFX_(PRBool) NS_HexToRGB(const nsString& aBuf, nscolor* aResult);

// Translate a hex string to a color. Return true if it parses ok,
// otherwise return false.
// This version accepts 1 to 9 digits (missing digits are 0)
extern "C" NS_GFX_(PRBool) NS_LooseHexToRGB(const nsString& aBuf, nscolor* aResult);

// Translate a color name to a color. Return true if it parses ok,
// otherwise return false.
extern "C" NS_GFX_(PRBool) NS_ColorNameToRGB(const nsString& aBuf, nscolor* aResult);

// Weird color computing code stolen from winfe which was stolen
// from the xfe which was written originally by Eric Bina. So there.
extern "C" NS_GFX_(void) NS_Get3DColors(nscolor aResult[2], nscolor aColor);

// To determin colors based on the background brightness
extern "C" NS_GFX_(void) NS_GetSpecial3DColors(nscolor aResult[2],
											   nscolor aBackgroundColor,
											   nscolor aBorderColor);

// Special method to brighten a Color and have it shift to white when
// fully saturated.
extern "C" NS_GFX_(nscolor) NS_BrightenColor(nscolor inColor);

// Special method to darken a Color and have it shift to black when
// darkest component underflows
extern "C" NS_GFX_(nscolor) NS_DarkenColor(nscolor inColor);

// Determins brightness for a specific color
extern "C" NS_GFX_(int) NS_GetBrightness(PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue);

#endif /* nsColor_h___ */
