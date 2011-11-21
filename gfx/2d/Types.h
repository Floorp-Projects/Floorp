/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef MOZILLA_GFX_TYPES_H_
#define MOZILLA_GFX_TYPES_H_

#if defined (_SVR4) || defined (SVR4) || defined (__OpenBSD__) || defined (_sgi) || defined (__sun) || defined (sun) || defined (__digital__)
#  include <inttypes.h>
#elif defined (_MSC_VER)
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#elif defined (_AIX)
#  include <sys/inttypes.h>
#else
#  include <stdint.h>
#endif

#include <stddef.h>

namespace mozilla {
namespace gfx {

typedef float Float;

enum SurfaceType
{
  SURFACE_DATA, /* Data surface - bitmap in memory */
  SURFACE_D2D1_BITMAP, /* Surface wrapping a ID2D1Bitmap */
  SURFACE_D2D1_DRAWTARGET, /* Surface made from a D2D draw target */
  SURFACE_CAIRO, /* Surface wrapping a cairo surface */
  SURFACE_COREGRAPHICS_IMAGE, /* Surface wrapping a CoreGraphics Image */
  SURFACE_SKIA /* Surface wrapping a Skia bitmap */
};

enum SurfaceFormat
{
  FORMAT_B8G8R8A8,
  FORMAT_B8G8R8X8,
  FORMAT_R5G6B5,
  FORMAT_A8
};

enum BackendType
{
  BACKEND_DIRECT2D,
  BACKEND_COREGRAPHICS,
  BACKEND_CAIRO,
  BACKEND_SKIA
};

enum FontType
{
  FONT_DWRITE,
  FONT_GDI,
  FONT_MAC,
  FONT_SKIA
};

enum NativeSurfaceType
{
  NATIVE_SURFACE_D3D10_TEXTURE
};

enum NativeFontType
{
  NATIVE_FONT_DWRITE_FONT_FACE,
  NATIVE_FONT_GDI_FONT_FACE,
  NATIVE_FONT_MAC_FONT_FACE,
  NATIVE_FONT_SKIA_FONT_FACE
};

enum CompositionOp { OP_OVER, OP_ADD, OP_ATOP, OP_OUT, OP_IN, OP_SOURCE, OP_DEST_IN, OP_DEST_OUT, OP_DEST_OVER, OP_DEST_ATOP, OP_XOR, OP_COUNT };
enum ExtendMode { EXTEND_CLAMP, EXTEND_WRAP, EXTEND_MIRROR };
enum FillRule { FILL_WINDING, FILL_EVEN_ODD };
enum AntialiasMode { AA_NONE, AA_GRAY, AA_SUBPIXEL };
enum Snapping { SNAP_NONE, SNAP_ALIGNED };
enum Filter { FILTER_LINEAR, FILTER_POINT };
enum PatternType { PATTERN_COLOR, PATTERN_SURFACE, PATTERN_LINEAR_GRADIENT, PATTERN_RADIAL_GRADIENT };
enum JoinStyle { JOIN_BEVEL, JOIN_ROUND, JOIN_MITER, JOIN_MITER_OR_BEVEL };
enum CapStyle { CAP_BUTT, CAP_ROUND, CAP_SQUARE };

/* Color is stored in non-premultiplied form */
struct Color
{
public:
  Color()
    : r(0.0f), g(0.0f), b(0.0f), a(0.0f)
  {}
  Color(Float aR, Float aG, Float aB, Float aA)
    : r(aR), g(aG), b(aB), a(aA)
  {}
  Color(Float aR, Float aG, Float aB)
    : r(aR), g(aG), b(aB), a(1.0f)
  {}

  static Color FromABGR(uint32_t aColor)
  {
    Color newColor(((aColor >> 0) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 8) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 16) & 0xff) * (1.0f / 255.0f),
                   ((aColor >> 24) & 0xff) * (1.0f / 255.0f));

    return newColor;
  }

  Float r, g, b, a;
};

struct GradientStop
{
  bool operator<(const GradientStop& aOther) const {
    return offset < aOther.offset;
  }

  Float offset;
  Color color;
};

}
}

// Side constants for use in various places
namespace mozilla {
  namespace css {
    enum Side {eSideTop, eSideRight, eSideBottom, eSideLeft};
  }
}

// XXX - These don't really belong here. But for now this is where they go.
#define NS_SIDE_TOP     mozilla::css::eSideTop
#define NS_SIDE_RIGHT   mozilla::css::eSideRight
#define NS_SIDE_BOTTOM  mozilla::css::eSideBottom
#define NS_SIDE_LEFT    mozilla::css::eSideLeft

#endif /* MOZILLA_GFX_TYPES_H_ */
