/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* functions that manipulate colors */

#ifndef __nsCSSColorUtils_h
#define __nsCSSColorUtils_h

#include "nsColor.h"

// "Sufficient contrast" is determined by
// "Techniques For Accessibility Evalution And Repair Tools".
// See http://www.w3.org/TR/AERT#color-contrast
#define NS_SUFFICIENT_LUMINOSITY_DIFFERENCE 125000
#define NS_LUMINOSITY_DIFFERENCE(a, b) \
          NS_ABS(NS_GetLuminosity(a) - NS_GetLuminosity(b))

// To determine colors based on the background brightness and border color
void NS_GetSpecial3DColors(nscolor aResult[2],
                           nscolor aBackgroundColor,
                           nscolor aBorderColor);

// Determins brightness for a specific color
int NS_GetBrightness(PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue);

// Get Luminosity of a specific color. That is same as Y of YIQ color space.
// The range of return value is 0 to 255000.
PRInt32 NS_GetLuminosity(nscolor aColor);

// function to convert from RGBA color space to HSVA color space 
void NS_RGB2HSV(nscolor aColor, PRUint16 &aHue, PRUint16 &aSat,
                PRUint16 &aValue, PRUint8 &aAlpha);

// function to convert from HSVA color space to RGBA color space 
void NS_HSV2RGB(nscolor &aColor, PRUint16 aHue, PRUint16 aSat, PRUint16 aValue,
                PRUint8 aAlpha);

#endif
