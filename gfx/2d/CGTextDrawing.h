/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _MOZILLA_GFX_SKIACGPOPUPDRAWER_H
#define _MOZILLA_GFX_SKIACGPOPUPDRAWER_H

#include <ApplicationServices/ApplicationServices.h>
#include "nsDebug.h"
#include "mozilla/Vector.h"
#include "ScaledFontMac.h"
#include "PathCG.h"
#include <dlfcn.h>

// This is used when we explicitly need CG to draw text to support things such
// as vibrancy and subpixel AA on transparent backgrounds. The current use cases
// are really only to enable Skia to support drawing text in those situations.

namespace mozilla {
namespace gfx {

typedef void (*CGContextSetFontSmoothingBackgroundColorFunc) (CGContextRef cgContext, CGColorRef color);

static CGContextSetFontSmoothingBackgroundColorFunc
GetCGContextSetFontSmoothingBackgroundColorFunc()
{
  static CGContextSetFontSmoothingBackgroundColorFunc func = nullptr;
  static bool lookedUpFunc = false;
  if (!lookedUpFunc) {
    func = (CGContextSetFontSmoothingBackgroundColorFunc)dlsym(
      RTLD_DEFAULT, "CGContextSetFontSmoothingBackgroundColor");
    lookedUpFunc = true;
  }
  return func;
}

static CGColorRef
ColorToCGColor(CGColorSpaceRef aColorSpace, const Color& aColor)
{
  CGFloat components[4] = {aColor.r, aColor.g, aColor.b, aColor.a};
  return CGColorCreate(aColorSpace, components);
}

static bool
SetFontSmoothingBackgroundColor(CGContextRef aCGContext, CGColorSpaceRef aColorSpace,
                                const GlyphRenderingOptions* aRenderingOptions)
{
  if (aRenderingOptions) {
    Color fontSmoothingBackgroundColor =
      static_cast<const GlyphRenderingOptionsCG*>(aRenderingOptions)->FontSmoothingBackgroundColor();
    if (fontSmoothingBackgroundColor.a > 0) {
      CGContextSetFontSmoothingBackgroundColorFunc setFontSmoothingBGColorFunc =
        GetCGContextSetFontSmoothingBackgroundColorFunc();
      if (setFontSmoothingBGColorFunc) {
        CGColorRef color = ColorToCGColor(aColorSpace, fontSmoothingBackgroundColor);
        setFontSmoothingBGColorFunc(aCGContext, color);
        CGColorRelease(color);
        return true;
      }
    }
  }

  return false;
}

// Font rendering with a non-transparent font smoothing background color
// can leave pixels in our buffer where the rgb components exceed the alpha
// component. When this happens we need to clean up the data afterwards.
// The purpose of this is probably the following: Correct compositing of
// subpixel anti-aliased fonts on transparent backgrounds requires
// different alpha values per RGB component. Usually, premultiplied color
// values are derived by multiplying all components with the same per-pixel
// alpha value. However, if you multiply each component with a *different*
// alpha, and set the alpha component of the pixel to, say, the average
// of the alpha values that you used during the premultiplication of the
// RGB components, you can trick OVER compositing into doing a simplified
// form of component alpha compositing. (You just need to make sure to
// clamp the components of the result pixel to [0,255] afterwards.)
static void
EnsureValidPremultipliedData(CGContextRef aContext,
                             CGRect aTextBounds = CGRectInfinite)
{
  if (CGBitmapContextGetBitsPerPixel(aContext) != 32 ||
      CGBitmapContextGetAlphaInfo(aContext) != kCGImageAlphaPremultipliedFirst) {
    return;
  }

  uint8_t* bitmapData = (uint8_t*)CGBitmapContextGetData(aContext);
  CGRect bitmapBounds = CGRectMake(0, 0, CGBitmapContextGetWidth(aContext), CGBitmapContextGetHeight(aContext));
  int stride = CGBitmapContextGetBytesPerRow(aContext);

  CGRect bounds = CGRectIntersection(bitmapBounds, aTextBounds);
  int startX = bounds.origin.x;
  int endX = startX + bounds.size.width;
  MOZ_ASSERT(endX <= bitmapBounds.size.width);


  // CGRect assume that our origin is the bottom left.
  // The data assumes that the origin is the top left.
  // Have to switch the Y axis so that our coordinates are correct
  int startY = bitmapBounds.size.height - (bounds.origin.y + bounds.size.height);
  int endY = startY + bounds.size.height;
  MOZ_ASSERT(endY <= (int)CGBitmapContextGetHeight(aContext));

  for (int y = startY; y < endY; y++) {
    for (int x = startX; x < endX; x++) {
      int i = y * stride + x * 4;
      uint8_t a = bitmapData[i + 3];

      bitmapData[i + 0] = std::min(a, bitmapData[i+0]);
      bitmapData[i + 1] = std::min(a, bitmapData[i+1]);
      bitmapData[i + 2] = std::min(a, bitmapData[i+2]);
    }
  }
}

static CGRect
ComputeGlyphsExtents(CGRect *bboxes, CGPoint *positions, CFIndex count, float scale)
{
  CGFloat x1, x2, y1, y2;
  if (count < 1)
    return CGRectZero;

  x1 = bboxes[0].origin.x + positions[0].x;
  x2 = bboxes[0].origin.x + positions[0].x + scale*bboxes[0].size.width;
  y1 = bboxes[0].origin.y + positions[0].y;
  y2 = bboxes[0].origin.y + positions[0].y + scale*bboxes[0].size.height;

  // accumulate max and minimum coordinates
  for (int i = 1; i < count; i++) {
    x1 = std::min(x1, bboxes[i].origin.x + positions[i].x);
    y1 = std::min(y1, bboxes[i].origin.y + positions[i].y);
    x2 = std::max(x2, bboxes[i].origin.x + positions[i].x + scale*bboxes[i].size.width);
    y2 = std::max(y2, bboxes[i].origin.y + positions[i].y + scale*bboxes[i].size.height);
  }

  CGRect extents = {{x1, y1}, {x2-x1, y2-y1}};
  return extents;
}

} // namespace gfx
} // namespace mozilla

#endif
