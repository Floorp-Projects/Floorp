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

#ifndef MOZILLA_GFX_HELPERSCAIRO_H_
#define MOZILLA_GFX_HELPERSCAIRO_H_

#include "2D.h"
#include "cairo.h"

namespace mozilla {
namespace gfx {

static inline cairo_operator_t
GfxOpToCairoOp(CompositionOp op)
{
  switch (op)
  {
    case OP_OVER:
      return CAIRO_OPERATOR_OVER;
    case OP_ADD:
      return CAIRO_OPERATOR_ADD;
    case OP_ATOP:
      return CAIRO_OPERATOR_ATOP;
    case OP_OUT:
      return CAIRO_OPERATOR_OUT;
    case OP_IN:
      return CAIRO_OPERATOR_IN;
    case OP_SOURCE:
      return CAIRO_OPERATOR_SOURCE;
    case OP_DEST_IN:
      return CAIRO_OPERATOR_DEST_IN;
    case OP_DEST_OUT:
      return CAIRO_OPERATOR_DEST_OUT;
    case OP_DEST_OVER:
      return CAIRO_OPERATOR_DEST_OVER;
    case OP_DEST_ATOP:
      return CAIRO_OPERATOR_DEST_ATOP;
    case OP_XOR:
      return CAIRO_OPERATOR_XOR;
    case OP_COUNT:
      break;
  }

  return CAIRO_OPERATOR_OVER;
}

static inline cairo_filter_t
GfxFilterToCairoFilter(Filter filter)
{
  switch (filter)
  {
    case FILTER_LINEAR:
      return CAIRO_FILTER_BILINEAR;
    case FILTER_POINT:
      return CAIRO_FILTER_NEAREST;
  }

  return CAIRO_FILTER_BILINEAR;
}

static inline cairo_extend_t
GfxExtendToCairoExtend(ExtendMode extend)
{
  switch (extend)
  {
    case EXTEND_CLAMP:
      return CAIRO_EXTEND_PAD;
    case EXTEND_REPEAT:
      return CAIRO_EXTEND_REPEAT;
    case EXTEND_REFLECT:
      return CAIRO_EXTEND_REFLECT;
  }

  return CAIRO_EXTEND_PAD;
}

static inline cairo_format_t
GfxFormatToCairoFormat(SurfaceFormat format)
{
  switch (format)
  {
    case FORMAT_B8G8R8A8:
      return CAIRO_FORMAT_ARGB32;
    case FORMAT_B8G8R8X8:
      return CAIRO_FORMAT_RGB24;
    case FORMAT_A8:
      return CAIRO_FORMAT_A8;
  }

  return CAIRO_FORMAT_ARGB32;
}

static inline cairo_content_t
GfxFormatToCairoContent(SurfaceFormat format)
{
  switch (format)
  {
    case FORMAT_B8G8R8A8:
      return CAIRO_CONTENT_COLOR_ALPHA;
    case FORMAT_B8G8R8X8:
      return CAIRO_CONTENT_COLOR;
    case FORMAT_A8:
      return CAIRO_CONTENT_ALPHA;
  }

  return CAIRO_CONTENT_COLOR_ALPHA;
}

static inline cairo_line_join_t
GfxLineJoinToCairoLineJoin(JoinStyle style)
{
  switch (style)
  {
    case JOIN_BEVEL:
      return CAIRO_LINE_JOIN_BEVEL;
    case JOIN_ROUND:
      return CAIRO_LINE_JOIN_ROUND;
    case JOIN_MITER:
      return CAIRO_LINE_JOIN_MITER;
    case JOIN_MITER_OR_BEVEL:
      return CAIRO_LINE_JOIN_MITER;
  }

  return CAIRO_LINE_JOIN_MITER;
}

static inline cairo_line_cap_t
GfxLineCapToCairoLineCap(CapStyle style)
{
  switch (style)
  {
    case CAP_BUTT:
      return CAIRO_LINE_CAP_BUTT;
    case CAP_ROUND:
      return CAIRO_LINE_CAP_ROUND;
    case CAP_SQUARE:
      return CAIRO_LINE_CAP_SQUARE;
  }

  return CAIRO_LINE_CAP_BUTT;
}

static inline SurfaceFormat
CairoContentToGfxFormat(cairo_content_t content)
{
  switch (content)
  {
    case CAIRO_CONTENT_COLOR_ALPHA:
      return FORMAT_B8G8R8A8;
    case CAIRO_CONTENT_COLOR:
      return FORMAT_B8G8R8X8;
    case CAIRO_CONTENT_ALPHA:
      return FORMAT_A8;
  }

  return FORMAT_B8G8R8A8;
}

static inline void
GfxMatrixToCairoMatrix(const Matrix& mat, cairo_matrix_t& retval)
{
  cairo_matrix_init(&retval, mat._11, mat._12, mat._21, mat._22, mat._31, mat._32);
}

static inline void
SetCairoStrokeOptions(cairo_t* aCtx, const StrokeOptions& aStrokeOptions)
{
  cairo_set_line_width(aCtx, aStrokeOptions.mLineWidth);

  cairo_set_miter_limit(aCtx, aStrokeOptions.mMiterLimit);

  if (aStrokeOptions.mDashPattern) {
    // Convert array of floats to array of doubles
    std::vector<double> dashes(aStrokeOptions.mDashLength);
    for (size_t i = 0; i < aStrokeOptions.mDashLength; ++i) {
      dashes[i] = aStrokeOptions.mDashPattern[i];
    }
    cairo_set_dash(aCtx, &dashes[0], aStrokeOptions.mDashLength,
                   aStrokeOptions.mDashOffset);
  }

  cairo_set_line_join(aCtx, GfxLineJoinToCairoLineJoin(aStrokeOptions.mLineJoin));

  cairo_set_line_cap(aCtx, GfxLineCapToCairoLineCap(aStrokeOptions.mLineCap));
}

static inline cairo_fill_rule_t
GfxFillRuleToCairoFillRule(FillRule rule)
{
  switch (rule)
  {
    case FILL_WINDING:
      return CAIRO_FILL_RULE_WINDING;
    case FILL_EVEN_ODD:
      return CAIRO_FILL_RULE_EVEN_ODD;
  }

  return CAIRO_FILL_RULE_WINDING;
}

}
}

#endif /* MOZILLA_GFX_HELPERSCAIRO_H_ */
