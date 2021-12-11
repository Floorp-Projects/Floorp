/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
// NS_SUFFICIENT_LUMINOSITY_DIFFERENCE is the a11y standard for text
// on a background. Use 20% of that standard since we have a background
// on top of another background
#define NS_SUFFICIENT_LUMINOSITY_DIFFERENCE_BG \
  (NS_SUFFICIENT_LUMINOSITY_DIFFERENCE / 5)

#define NS_LUMINOSITY_DIFFERENCE(a, b)                    \
  int32_t(mozilla::Abs(NS_GetLuminosity(a | 0xff000000) - \
                       NS_GetLuminosity(b | 0xff000000)))

// Maximum value that NS_GetLuminosity can return.
#define NS_MAX_LUMINOSITY 255000

// To determine 3D colors for groove / ridge borders based on the border color
void NS_GetSpecial3DColors(nscolor aResult[2], nscolor aBorderColor);

// Determins brightness for a specific color
int NS_GetBrightness(uint8_t aRed, uint8_t aGreen, uint8_t aBlue);

// Get Luminosity of a specific color. That is same as Y of YIQ color space.
// The range of return value is 0 to NS_MAX_LUMINOSITY.
int32_t NS_GetLuminosity(nscolor aColor);

// function to convert from RGBA color space to HSVA color space
void NS_RGB2HSV(nscolor aColor, uint16_t& aHue, uint16_t& aSat,
                uint16_t& aValue, uint8_t& aAlpha);

// function to convert from HSVA color space to RGBA color space
void NS_HSV2RGB(nscolor& aColor, uint16_t aHue, uint16_t aSat, uint16_t aValue,
                uint8_t aAlpha);

#endif
