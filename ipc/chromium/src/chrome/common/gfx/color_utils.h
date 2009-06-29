// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_COLOR_UTILS_H__
#define CHROME_COMMON_GFX_COLOR_UTILS_H__

#include "SkColor.h"

class SkBitmap;

namespace color_utils {

// Represents set of CIE XYZ tristimulus values.
struct CIE_XYZ {
  double X;
  double Y; // luminance
  double Z;
};

// Represents a L*a*b* color value
struct LabColor {
  int L;
  int a;
  int b;
};

// Note: these transformations assume sRGB as the source color space

// Convert between different color spaces
void SkColorToCIEXYZ(SkColor c, CIE_XYZ* xyz);
void CIEXYZToLabColor(const CIE_XYZ& xyz, LabColor* lab);

SkColor CIEXYZToSkColor(SkAlpha alpha, const CIE_XYZ& xyz);
void LabColorToCIEXYZ(const LabColor& lab, CIE_XYZ* xyz);

void SkColorToLabColor(SkColor c, LabColor* lab);
SkColor LabColorToSkColor(const LabColor& lab, SkAlpha alpha);

// Determine if a given alpha value is nearly completely transparent.
bool IsColorCloseToTransparent(SkAlpha alpha);

// Determine if a color is near grey.
bool IsColorCloseToGrey(int r, int g, int b);

// Gets a color representing a bitmap. The definition of "representing" is the
// average color in the bitmap. The color returned is modified to have the
// specified alpha.
SkColor GetAverageColorOfFavicon(SkBitmap* bitmap, SkAlpha alpha);

// Builds a histogram based on the Y' of the Y'UV representation of
// this image.
void BuildLumaHistogram(SkBitmap* bitmap, int histogram[256]);

// Create a color from a base color and a specific alpha value.
SkColor SetColorAlpha(SkColor c, SkAlpha alpha);

// Gets a Windows system color as a SkColor
SkColor GetSysSkColor(int which);

} // namespace color_utils

#endif  // #ifndef CHROME_COMMON_GFX_COLOR_UTILS_H__
