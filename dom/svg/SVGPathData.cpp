/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGPathData.h"

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/RefPtr.h"
#include "nsError.h"
#include "nsString.h"
#include "SVGPathDataParser.h"
#include <stdarg.h>
#include "nsStyleConsts.h"
#include "SVGContentUtils.h"
#include "SVGGeometryElement.h"
#include "SVGPathSegUtils.h"
#include <algorithm>

using namespace mozilla::dom::SVGPathSeg_Binding;
using namespace mozilla::gfx;

namespace mozilla {

static inline bool IsMoveto(uint16_t aSegType) {
  return aSegType == PATHSEG_MOVETO_ABS || aSegType == PATHSEG_MOVETO_REL;
}

static inline bool IsMoveto(StylePathCommand::Tag aSegType) {
  return aSegType == StylePathCommand::Tag::MoveTo;
}

static inline bool IsValidType(uint16_t aSegType) {
  return SVGPathSegUtils::IsValidType(aSegType);
}

static inline bool IsValidType(StylePathCommand::Tag aSegType) {
  return aSegType != StylePathCommand::Tag::Unknown;
}

static inline bool IsClosePath(uint16_t aSegType) {
  return aSegType == PATHSEG_CLOSEPATH;
}

static inline bool IsClosePath(StylePathCommand::Tag aSegType) {
  return aSegType == StylePathCommand::Tag::ClosePath;
}

nsresult SVGPathData::CopyFrom(const SVGPathData& rhs) {
  if (!mData.Assign(rhs.mData, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void SVGPathData::GetValueAsString(nsAString& aValue) const {
  // we need this function in DidChangePathSegList
  aValue.Truncate();
  if (!Length()) {
    return;
  }
  uint32_t i = 0;
  for (;;) {
    nsAutoString segAsString;
    SVGPathSegUtils::GetValueAsString(&mData[i], segAsString);
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(segAsString);
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
    if (i >= mData.Length()) {
      MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt");
      return;
    }
    aValue.Append(' ');
  }
}

nsresult SVGPathData::SetValueFromString(const nsAString& aValue) {
  // We don't use a temp variable since the spec says to parse everything up to
  // the first error. We still return any error though so that callers know if
  // there's a problem.

  SVGPathDataParser pathParser(aValue, this);
  return pathParser.Parse() ? NS_OK : NS_ERROR_DOM_SYNTAX_ERR;
}

nsresult SVGPathData::AppendSeg(uint32_t aType, ...) {
  uint32_t oldLength = mData.Length();
  uint32_t newLength = oldLength + 1 + SVGPathSegUtils::ArgCountForType(aType);
  if (!mData.SetLength(newLength, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mData[oldLength] = SVGPathSegUtils::EncodeType(aType);
  va_list args;
  va_start(args, aType);
  for (uint32_t i = oldLength + 1; i < newLength; ++i) {
    // NOTE! 'float' is promoted to 'double' when passed through '...'!
    mData[i] = float(va_arg(args, double));
  }
  va_end(args);
  return NS_OK;
}

float SVGPathData::GetPathLength() const {
  SVGPathTraversalState state;

  uint32_t i = 0;
  while (i < mData.Length()) {
    SVGPathSegUtils::TraversePathSegment(&mData[i], state);
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
  }

  MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt");

  return state.length;
}

#ifdef DEBUG
uint32_t SVGPathData::CountItems() const {
  uint32_t i = 0, count = 0;

  while (i < mData.Length()) {
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
    count++;
  }

  MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt");

  return count;
}
#endif

bool SVGPathData::GetSegmentLengths(nsTArray<double>* aLengths) const {
  aLengths->Clear();
  SVGPathTraversalState state;

  uint32_t i = 0;
  while (i < mData.Length()) {
    state.length = 0.0;
    SVGPathSegUtils::TraversePathSegment(&mData[i], state);
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    aLengths->AppendElement(state.length);
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
  }

  MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt");

  return true;
}

bool SVGPathData::GetDistancesFromOriginToEndsOfVisibleSegments(
    FallibleTArray<double>* aOutput) const {
  SVGPathTraversalState state;

  aOutput->Clear();

  uint32_t i = 0;
  while (i < mData.Length()) {
    uint32_t segType = SVGPathSegUtils::DecodeType(mData[i]);
    SVGPathSegUtils::TraversePathSegment(&mData[i], state);

    // With degenerately large point coordinates, TraversePathSegment can fail
    // and end up producing NaNs.
    if (!std::isfinite(state.length)) {
      return false;
    }

    // We skip all moveto commands except an initial moveto. See the text 'A
    // "move to" command does not count as an additional point when dividing up
    // the duration...':
    //
    // http://www.w3.org/TR/SVG11/animate.html#AnimateMotionElement
    //
    // This is important in the non-default case of calcMode="linear". In
    // this case an equal amount of time is spent on each path segment,
    // except on moveto segments which are jumped over immediately.

    if (i == 0 ||
        (segType != PATHSEG_MOVETO_ABS && segType != PATHSEG_MOVETO_REL)) {
      if (!aOutput->AppendElement(state.length, fallible)) {
        return false;
      }
    }
    i += 1 + SVGPathSegUtils::ArgCountForType(segType);
  }

  MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt?");

  return true;
}

uint32_t SVGPathData::GetPathSegAtLength(float aDistance) const {
  // TODO [SVGWG issue] get specified what happen if 'aDistance' < 0, or
  // 'aDistance' > the length of the path, or the seg list is empty.
  // Return -1? Throwing would better help authors avoid tricky bugs (DOM
  // could do that if we return -1).

  uint32_t i = 0, segIndex = 0;
  SVGPathTraversalState state;

  while (i < mData.Length()) {
    SVGPathSegUtils::TraversePathSegment(&mData[i], state);
    if (state.length >= aDistance) {
      return segIndex;
    }
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
    segIndex++;
  }

  MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt");

  return std::max(1U, segIndex) -
         1;  // -1 because while loop takes us 1 too far
}

/**
 * The SVG spec says we have to paint stroke caps for zero length subpaths:
 *
 *   http://www.w3.org/TR/SVG11/implnote.html#PathElementImplementationNotes
 *
 * Cairo only does this for |stroke-linecap: round| and not for
 * |stroke-linecap: square| (since that's what Adobe Acrobat has always done).
 * Most likely the other backends that DrawTarget uses have the same behavior.
 *
 * To help us conform to the SVG spec we have this helper function to draw an
 * approximation of square caps for zero length subpaths. It does this by
 * inserting a subpath containing a single user space axis aligned straight
 * line that is as small as it can be while minimizing the risk of it being
 * thrown away by the DrawTarget's backend for being too small to affect
 * rendering. The idea is that we'll then get stroke caps drawn for this axis
 * aligned line, creating an axis aligned rectangle that approximates the
 * square that would ideally be drawn.
 *
 * Since we don't have any information about transforms from user space to
 * device space, we choose the length of the small line that we insert by
 * making it a small percentage of the stroke width of the path. This should
 * hopefully allow us to make the line as long as possible (to avoid rounding
 * issues in the backend resulting in the backend seeing it as having zero
 * length) while still avoiding the small rectangle being noticeably different
 * from a square.
 *
 * Note that this function inserts a subpath into the current gfx path that
 * will be present during both fill and stroke operations.
 */
static void ApproximateZeroLengthSubpathSquareCaps(PathBuilder* aPB,
                                                   const Point& aPoint,
                                                   Float aStrokeWidth) {
  // Note that caps are proportional to stroke width, so if stroke width is
  // zero it's actually fine for |tinyLength| below to end up being zero.
  // However, it would be a waste to inserting a LineTo in that case, so better
  // not to.
  MOZ_ASSERT(aStrokeWidth > 0.0f,
             "Make the caller check for this, or check it here");

  // The fraction of the stroke width that we choose for the length of the
  // line is rather arbitrary, other than being chosen to meet the requirements
  // described in the comment above.

  Float tinyLength = aStrokeWidth / SVG_ZERO_LENGTH_PATH_FIX_FACTOR;

  aPB->LineTo(aPoint + Point(tinyLength, 0));
  aPB->MoveTo(aPoint);
}

#define MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT  \
  do {                                                           \
    if (!subpathHasLength && hasLineCaps && aStrokeWidth > 0 &&  \
        subpathContainsNonMoveTo && IsValidType(prevSegType) &&  \
        (!IsMoveto(prevSegType) || IsClosePath(segType))) {      \
      ApproximateZeroLengthSubpathSquareCaps(aBuilder, segStart, \
                                             aStrokeWidth);      \
    }                                                            \
  } while (0)

already_AddRefed<Path> SVGPathData::BuildPath(PathBuilder* aBuilder,
                                              StyleStrokeLinecap aStrokeLineCap,
                                              Float aStrokeWidth) const {
  if (mData.IsEmpty() || !IsMoveto(SVGPathSegUtils::DecodeType(mData[0]))) {
    return nullptr;  // paths without an initial moveto are invalid
  }

  bool hasLineCaps = aStrokeLineCap != StyleStrokeLinecap::Butt;
  bool subpathHasLength = false;  // visual length
  bool subpathContainsNonMoveTo = false;

  uint32_t segType = PATHSEG_UNKNOWN;
  uint32_t prevSegType = PATHSEG_UNKNOWN;
  Point pathStart(0.0, 0.0);  // start point of [sub]path
  Point segStart(0.0, 0.0);
  Point segEnd;
  Point cp1, cp2;    // previous bezier's control points
  Point tcp1, tcp2;  // temporaries

  // Regarding cp1 and cp2: If the previous segment was a cubic bezier curve,
  // then cp2 is its second control point. If the previous segment was a
  // quadratic curve, then cp1 is its (only) control point.

  uint32_t i = 0;
  while (i < mData.Length()) {
    segType = SVGPathSegUtils::DecodeType(mData[i++]);
    uint32_t argCount = SVGPathSegUtils::ArgCountForType(segType);

    switch (segType) {
      case PATHSEG_CLOSEPATH:
        // set this early to allow drawing of square caps for "M{x},{y} Z":
        subpathContainsNonMoveTo = true;
        MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT;
        segEnd = pathStart;
        aBuilder->Close();
        break;

      case PATHSEG_MOVETO_ABS:
        MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT;
        pathStart = segEnd = Point(mData[i], mData[i + 1]);
        aBuilder->MoveTo(segEnd);
        subpathHasLength = false;
        break;

      case PATHSEG_MOVETO_REL:
        MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT;
        pathStart = segEnd = segStart + Point(mData[i], mData[i + 1]);
        aBuilder->MoveTo(segEnd);
        subpathHasLength = false;
        break;

      case PATHSEG_LINETO_ABS:
        segEnd = Point(mData[i], mData[i + 1]);
        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(segEnd);
        }
        break;

      case PATHSEG_LINETO_REL:
        segEnd = segStart + Point(mData[i], mData[i + 1]);
        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(segEnd);
        }
        break;

      case PATHSEG_CURVETO_CUBIC_ABS:
        cp1 = Point(mData[i], mData[i + 1]);
        cp2 = Point(mData[i + 2], mData[i + 3]);
        segEnd = Point(mData[i + 4], mData[i + 5]);
        if (segEnd != segStart || segEnd != cp1 || segEnd != cp2) {
          subpathHasLength = true;
          aBuilder->BezierTo(cp1, cp2, segEnd);
        }
        break;

      case PATHSEG_CURVETO_CUBIC_REL:
        cp1 = segStart + Point(mData[i], mData[i + 1]);
        cp2 = segStart + Point(mData[i + 2], mData[i + 3]);
        segEnd = segStart + Point(mData[i + 4], mData[i + 5]);
        if (segEnd != segStart || segEnd != cp1 || segEnd != cp2) {
          subpathHasLength = true;
          aBuilder->BezierTo(cp1, cp2, segEnd);
        }
        break;

      case PATHSEG_CURVETO_QUADRATIC_ABS:
        cp1 = Point(mData[i], mData[i + 1]);
        // Convert quadratic curve to cubic curve:
        tcp1 = segStart + (cp1 - segStart) * 2 / 3;
        segEnd = Point(mData[i + 2], mData[i + 3]);  // set before setting tcp2!
        tcp2 = cp1 + (segEnd - cp1) / 3;
        if (segEnd != segStart || segEnd != cp1) {
          subpathHasLength = true;
          aBuilder->BezierTo(tcp1, tcp2, segEnd);
        }
        break;

      case PATHSEG_CURVETO_QUADRATIC_REL:
        cp1 = segStart + Point(mData[i], mData[i + 1]);
        // Convert quadratic curve to cubic curve:
        tcp1 = segStart + (cp1 - segStart) * 2 / 3;
        segEnd = segStart +
                 Point(mData[i + 2], mData[i + 3]);  // set before setting tcp2!
        tcp2 = cp1 + (segEnd - cp1) / 3;
        if (segEnd != segStart || segEnd != cp1) {
          subpathHasLength = true;
          aBuilder->BezierTo(tcp1, tcp2, segEnd);
        }
        break;

      case PATHSEG_ARC_ABS:
      case PATHSEG_ARC_REL: {
        Point radii(mData[i], mData[i + 1]);
        segEnd = Point(mData[i + 5], mData[i + 6]);
        if (segType == PATHSEG_ARC_REL) {
          segEnd += segStart;
        }
        if (segEnd != segStart) {
          subpathHasLength = true;
          if (radii.x == 0.0f || radii.y == 0.0f) {
            aBuilder->LineTo(segEnd);
          } else {
            SVGArcConverter converter(segStart, segEnd, radii, mData[i + 2],
                                      mData[i + 3] != 0, mData[i + 4] != 0);
            while (converter.GetNextSegment(&cp1, &cp2, &segEnd)) {
              aBuilder->BezierTo(cp1, cp2, segEnd);
            }
          }
        }
        break;
      }

      case PATHSEG_LINETO_HORIZONTAL_ABS:
        segEnd = Point(mData[i], segStart.y);
        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(segEnd);
        }
        break;

      case PATHSEG_LINETO_HORIZONTAL_REL:
        segEnd = segStart + Point(mData[i], 0.0f);
        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(segEnd);
        }
        break;

      case PATHSEG_LINETO_VERTICAL_ABS:
        segEnd = Point(segStart.x, mData[i]);
        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(segEnd);
        }
        break;

      case PATHSEG_LINETO_VERTICAL_REL:
        segEnd = segStart + Point(0.0f, mData[i]);
        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(segEnd);
        }
        break;

      case PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
        cp1 = SVGPathSegUtils::IsCubicType(prevSegType) ? segStart * 2 - cp2
                                                        : segStart;
        cp2 = Point(mData[i], mData[i + 1]);
        segEnd = Point(mData[i + 2], mData[i + 3]);
        if (segEnd != segStart || segEnd != cp1 || segEnd != cp2) {
          subpathHasLength = true;
          aBuilder->BezierTo(cp1, cp2, segEnd);
        }
        break;

      case PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
        cp1 = SVGPathSegUtils::IsCubicType(prevSegType) ? segStart * 2 - cp2
                                                        : segStart;
        cp2 = segStart + Point(mData[i], mData[i + 1]);
        segEnd = segStart + Point(mData[i + 2], mData[i + 3]);
        if (segEnd != segStart || segEnd != cp1 || segEnd != cp2) {
          subpathHasLength = true;
          aBuilder->BezierTo(cp1, cp2, segEnd);
        }
        break;

      case PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
        cp1 = SVGPathSegUtils::IsQuadraticType(prevSegType) ? segStart * 2 - cp1
                                                            : segStart;
        // Convert quadratic curve to cubic curve:
        tcp1 = segStart + (cp1 - segStart) * 2 / 3;
        segEnd = Point(mData[i], mData[i + 1]);  // set before setting tcp2!
        tcp2 = cp1 + (segEnd - cp1) / 3;
        if (segEnd != segStart || segEnd != cp1) {
          subpathHasLength = true;
          aBuilder->BezierTo(tcp1, tcp2, segEnd);
        }
        break;

      case PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
        cp1 = SVGPathSegUtils::IsQuadraticType(prevSegType) ? segStart * 2 - cp1
                                                            : segStart;
        // Convert quadratic curve to cubic curve:
        tcp1 = segStart + (cp1 - segStart) * 2 / 3;
        segEnd = segStart +
                 Point(mData[i], mData[i + 1]);  // changed before setting tcp2!
        tcp2 = cp1 + (segEnd - cp1) / 3;
        if (segEnd != segStart || segEnd != cp1) {
          subpathHasLength = true;
          aBuilder->BezierTo(tcp1, tcp2, segEnd);
        }
        break;

      default:
        MOZ_ASSERT_UNREACHABLE("Bad path segment type");
        return nullptr;  // according to spec we'd use everything up to the bad
                         // seg anyway
    }

    subpathContainsNonMoveTo =
        segType != PATHSEG_MOVETO_ABS && segType != PATHSEG_MOVETO_REL;
    i += argCount;
    prevSegType = segType;
    segStart = segEnd;
  }

  MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt");
  MOZ_ASSERT(prevSegType == segType,
             "prevSegType should be left at the final segType");

  MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT;

  return aBuilder->Finish();
}

already_AddRefed<Path> SVGPathData::BuildPathForMeasuring() const {
  // Since the path that we return will not be used for painting it doesn't
  // matter what we pass to CreatePathBuilder as aFillRule. Hawever, we do want
  // to pass something other than NS_STYLE_STROKE_LINECAP_SQUARE as
  // aStrokeLineCap to avoid the insertion of extra little lines (by
  // ApproximateZeroLengthSubpathSquareCaps), in which case the value that we
  // pass as aStrokeWidth doesn't matter (since it's only used to determine the
  // length of those extra little lines).

  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<PathBuilder> builder =
      drawTarget->CreatePathBuilder(FillRule::FILL_WINDING);
  return BuildPath(builder, StyleStrokeLinecap::Butt, 0);
}

// We could simplify this function because this is only used by CSS motion path
// and clip-path, which don't render the SVG Path. i.e. The returned path is
// used as a reference.
/* static */
already_AddRefed<Path> SVGPathData::BuildPath(
    Span<const StylePathCommand> aPath, PathBuilder* aBuilder,
    StyleStrokeLinecap aStrokeLineCap, Float aStrokeWidth, float aZoomFactor) {
  if (aPath.IsEmpty() || !aPath[0].IsMoveTo()) {
    return nullptr;  // paths without an initial moveto are invalid
  }

  auto toGfxPoint = [](const StyleCoordPair& aPair) {
    return Point(aPair._0, aPair._1);
  };

  auto isCubicType = [](StylePathCommand::Tag aType) {
    return aType == StylePathCommand::Tag::CurveTo ||
           aType == StylePathCommand::Tag::SmoothCurveTo;
  };

  auto isQuadraticType = [](StylePathCommand::Tag aType) {
    return aType == StylePathCommand::Tag::QuadBezierCurveTo ||
           aType == StylePathCommand::Tag::SmoothQuadBezierCurveTo;
  };

  bool hasLineCaps = aStrokeLineCap != StyleStrokeLinecap::Butt;
  bool subpathHasLength = false;  // visual length
  bool subpathContainsNonMoveTo = false;

  StylePathCommand::Tag segType = StylePathCommand::Tag::Unknown;
  StylePathCommand::Tag prevSegType = StylePathCommand::Tag::Unknown;
  Point pathStart(0.0, 0.0);  // start point of [sub]path
  Point segStart(0.0, 0.0);
  Point segEnd;
  Point cp1, cp2;    // previous bezier's control points
  Point tcp1, tcp2;  // temporaries

  auto scale = [aZoomFactor](const Point& p) {
    return Point(p.x * aZoomFactor, p.y * aZoomFactor);
  };

  // Regarding cp1 and cp2: If the previous segment was a cubic bezier curve,
  // then cp2 is its second control point. If the previous segment was a
  // quadratic curve, then cp1 is its (only) control point.

  for (const StylePathCommand& cmd : aPath) {
    segType = cmd.tag;
    switch (segType) {
      case StylePathCommand::Tag::ClosePath:
        // set this early to allow drawing of square caps for "M{x},{y} Z":
        subpathContainsNonMoveTo = true;
        MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT;
        segEnd = pathStart;
        aBuilder->Close();
        break;
      case StylePathCommand::Tag::MoveTo: {
        MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT;
        const Point& p = toGfxPoint(cmd.move_to.point);
        pathStart = segEnd =
            cmd.move_to.absolute == StyleIsAbsolute::Yes ? p : segStart + p;
        aBuilder->MoveTo(scale(segEnd));
        subpathHasLength = false;
        break;
      }
      case StylePathCommand::Tag::LineTo: {
        const Point& p = toGfxPoint(cmd.line_to.point);
        segEnd =
            cmd.line_to.absolute == StyleIsAbsolute::Yes ? p : segStart + p;
        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(scale(segEnd));
        }
        break;
      }
      case StylePathCommand::Tag::CurveTo:
        cp1 = toGfxPoint(cmd.curve_to.control1);
        cp2 = toGfxPoint(cmd.curve_to.control2);
        segEnd = toGfxPoint(cmd.curve_to.point);

        if (cmd.curve_to.absolute == StyleIsAbsolute::No) {
          cp1 += segStart;
          cp2 += segStart;
          segEnd += segStart;
        }

        if (segEnd != segStart || segEnd != cp1 || segEnd != cp2) {
          subpathHasLength = true;
          aBuilder->BezierTo(scale(cp1), scale(cp2), scale(segEnd));
        }
        break;

      case StylePathCommand::Tag::QuadBezierCurveTo:
        cp1 = toGfxPoint(cmd.quad_bezier_curve_to.control1);
        segEnd = toGfxPoint(cmd.quad_bezier_curve_to.point);

        if (cmd.quad_bezier_curve_to.absolute == StyleIsAbsolute::No) {
          cp1 += segStart;
          segEnd += segStart;  // set before setting tcp2!
        }

        // Convert quadratic curve to cubic curve:
        tcp1 = segStart + (cp1 - segStart) * 2 / 3;
        tcp2 = cp1 + (segEnd - cp1) / 3;

        if (segEnd != segStart || segEnd != cp1) {
          subpathHasLength = true;
          aBuilder->BezierTo(scale(tcp1), scale(tcp2), scale(segEnd));
        }
        break;

      case StylePathCommand::Tag::EllipticalArc: {
        const auto& arc = cmd.elliptical_arc;
        Point radii(arc.rx, arc.ry);
        segEnd = toGfxPoint(arc.point);
        if (arc.absolute == StyleIsAbsolute::No) {
          segEnd += segStart;
        }
        if (segEnd != segStart) {
          subpathHasLength = true;
          if (radii.x == 0.0f || radii.y == 0.0f) {
            aBuilder->LineTo(scale(segEnd));
          } else {
            SVGArcConverter converter(segStart, segEnd, radii, arc.angle,
                                      arc.large_arc_flag._0, arc.sweep_flag._0);
            while (converter.GetNextSegment(&cp1, &cp2, &segEnd)) {
              aBuilder->BezierTo(scale(cp1), scale(cp2), scale(segEnd));
            }
          }
        }
        break;
      }
      case StylePathCommand::Tag::HorizontalLineTo:
        if (cmd.horizontal_line_to.absolute == StyleIsAbsolute::Yes) {
          segEnd = Point(cmd.horizontal_line_to.x, segStart.y);
        } else {
          segEnd = segStart + Point(cmd.horizontal_line_to.x, 0.0f);
        }

        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(scale(segEnd));
        }
        break;

      case StylePathCommand::Tag::VerticalLineTo:
        if (cmd.vertical_line_to.absolute == StyleIsAbsolute::Yes) {
          segEnd = Point(segStart.x, cmd.vertical_line_to.y);
        } else {
          segEnd = segStart + Point(0.0f, cmd.vertical_line_to.y);
        }

        if (segEnd != segStart) {
          subpathHasLength = true;
          aBuilder->LineTo(scale(segEnd));
        }
        break;

      case StylePathCommand::Tag::SmoothCurveTo:
        cp1 = isCubicType(prevSegType) ? segStart * 2 - cp2 : segStart;
        cp2 = toGfxPoint(cmd.smooth_curve_to.control2);
        segEnd = toGfxPoint(cmd.smooth_curve_to.point);

        if (cmd.smooth_curve_to.absolute == StyleIsAbsolute::No) {
          cp2 += segStart;
          segEnd += segStart;
        }

        if (segEnd != segStart || segEnd != cp1 || segEnd != cp2) {
          subpathHasLength = true;
          aBuilder->BezierTo(scale(cp1), scale(cp2), scale(segEnd));
        }
        break;

      case StylePathCommand::Tag::SmoothQuadBezierCurveTo: {
        cp1 = isQuadraticType(prevSegType) ? segStart * 2 - cp1 : segStart;
        // Convert quadratic curve to cubic curve:
        tcp1 = segStart + (cp1 - segStart) * 2 / 3;

        const Point& p = toGfxPoint(cmd.smooth_quad_bezier_curve_to.point);
        // set before setting tcp2!
        segEnd =
            cmd.smooth_quad_bezier_curve_to.absolute == StyleIsAbsolute::Yes
                ? p
                : segStart + p;
        tcp2 = cp1 + (segEnd - cp1) / 3;

        if (segEnd != segStart || segEnd != cp1) {
          subpathHasLength = true;
          aBuilder->BezierTo(scale(tcp1), scale(tcp2), scale(segEnd));
        }
        break;
      }
      case StylePathCommand::Tag::Unknown:
        MOZ_ASSERT_UNREACHABLE("Unacceptable path segment type");
        return nullptr;
    }

    subpathContainsNonMoveTo = !IsMoveto(segType);
    prevSegType = segType;
    segStart = segEnd;
  }

  MOZ_ASSERT(prevSegType == segType,
             "prevSegType should be left at the final segType");

  MAYBE_APPROXIMATE_ZERO_LENGTH_SUBPATH_SQUARE_CAPS_TO_DT;

  return aBuilder->Finish();
}

static double AngleOfVector(const Point& aVector) {
  // C99 says about atan2 "A domain error may occur if both arguments are
  // zero" and "On a domain error, the function returns an implementation-
  // defined value". In the case of atan2 the implementation-defined value
  // seems to commonly be zero, but it could just as easily be a NaN value.
  // We specifically want zero in this case, hence the check:

  return (aVector != Point(0.0, 0.0)) ? atan2(aVector.y, aVector.x) : 0.0;
}

static float AngleOfVector(const Point& cp1, const Point& cp2) {
  return static_cast<float>(AngleOfVector(cp1 - cp2));
}

void SVGPathData::GetMarkerPositioningData(nsTArray<SVGMark>* aMarks) const {
  // This code should assume that ANY type of segment can appear at ANY index.
  // It should also assume that segments such as M and Z can appear in weird
  // places, and repeat multiple times consecutively.

  // info on current [sub]path (reset every M command):
  Point pathStart(0.0, 0.0);
  float pathStartAngle = 0.0f;
  uint32_t pathStartIndex = 0;

  // info on previous segment:
  uint16_t prevSegType = PATHSEG_UNKNOWN;
  Point prevSegEnd(0.0, 0.0);
  float prevSegEndAngle = 0.0f;
  Point prevCP;  // if prev seg was a bezier, this was its last control point

  uint32_t i = 0;
  while (i < mData.Length()) {
    // info on current segment:
    uint16_t segType =
        SVGPathSegUtils::DecodeType(mData[i++]);  // advances i to args
    Point& segStart = prevSegEnd;
    Point segEnd;
    float segStartAngle, segEndAngle;

    switch (segType)  // to find segStartAngle, segEnd and segEndAngle
    {
      case PATHSEG_CLOSEPATH:
        segEnd = pathStart;
        segStartAngle = segEndAngle = AngleOfVector(segEnd, segStart);
        break;

      case PATHSEG_MOVETO_ABS:
      case PATHSEG_MOVETO_REL:
        if (segType == PATHSEG_MOVETO_ABS) {
          segEnd = Point(mData[i], mData[i + 1]);
        } else {
          segEnd = segStart + Point(mData[i], mData[i + 1]);
        }
        pathStart = segEnd;
        pathStartIndex = aMarks->Length();
        // If authors are going to specify multiple consecutive moveto commands
        // with markers, me might as well make the angle do something useful:
        segStartAngle = segEndAngle = AngleOfVector(segEnd, segStart);
        i += 2;
        break;

      case PATHSEG_LINETO_ABS:
      case PATHSEG_LINETO_REL:
        if (segType == PATHSEG_LINETO_ABS) {
          segEnd = Point(mData[i], mData[i + 1]);
        } else {
          segEnd = segStart + Point(mData[i], mData[i + 1]);
        }
        segStartAngle = segEndAngle = AngleOfVector(segEnd, segStart);
        i += 2;
        break;

      case PATHSEG_CURVETO_CUBIC_ABS:
      case PATHSEG_CURVETO_CUBIC_REL: {
        Point cp1, cp2;  // control points
        if (segType == PATHSEG_CURVETO_CUBIC_ABS) {
          cp1 = Point(mData[i], mData[i + 1]);
          cp2 = Point(mData[i + 2], mData[i + 3]);
          segEnd = Point(mData[i + 4], mData[i + 5]);
        } else {
          cp1 = segStart + Point(mData[i], mData[i + 1]);
          cp2 = segStart + Point(mData[i + 2], mData[i + 3]);
          segEnd = segStart + Point(mData[i + 4], mData[i + 5]);
        }
        prevCP = cp2;
        segStartAngle = AngleOfVector(
            cp1 == segStart ? (cp1 == cp2 ? segEnd : cp2) : cp1, segStart);
        segEndAngle = AngleOfVector(
            segEnd, cp2 == segEnd ? (cp1 == cp2 ? segStart : cp1) : cp2);
        i += 6;
        break;
      }

      case PATHSEG_CURVETO_QUADRATIC_ABS:
      case PATHSEG_CURVETO_QUADRATIC_REL: {
        Point cp1;  // control point
        if (segType == PATHSEG_CURVETO_QUADRATIC_ABS) {
          cp1 = Point(mData[i], mData[i + 1]);
          segEnd = Point(mData[i + 2], mData[i + 3]);
        } else {
          cp1 = segStart + Point(mData[i], mData[i + 1]);
          segEnd = segStart + Point(mData[i + 2], mData[i + 3]);
        }
        prevCP = cp1;
        segStartAngle = AngleOfVector(cp1 == segStart ? segEnd : cp1, segStart);
        segEndAngle = AngleOfVector(segEnd, cp1 == segEnd ? segStart : cp1);
        i += 4;
        break;
      }

      case PATHSEG_ARC_ABS:
      case PATHSEG_ARC_REL: {
        double rx = mData[i];
        double ry = mData[i + 1];
        double angle = mData[i + 2];
        bool largeArcFlag = mData[i + 3] != 0.0f;
        bool sweepFlag = mData[i + 4] != 0.0f;
        if (segType == PATHSEG_ARC_ABS) {
          segEnd = Point(mData[i + 5], mData[i + 6]);
        } else {
          segEnd = segStart + Point(mData[i + 5], mData[i + 6]);
        }

        // See section F.6 of SVG 1.1 for details on what we're doing here:
        // http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes

        if (segStart == segEnd) {
          // F.6.2 says "If the endpoints (x1, y1) and (x2, y2) are identical,
          // then this is equivalent to omitting the elliptical arc segment
          // entirely." We take that very literally here, not adding a mark, and
          // not even setting any of the 'prev' variables so that it's as if
          // this arc had never existed; note the difference this will make e.g.
          // if the arc is proceeded by a bezier curve and followed by a
          // "smooth" bezier curve of the same degree!
          i += 7;
          continue;
        }

        // Below we have funny interleaving of F.6.6 (Correction of out-of-range
        // radii) and F.6.5 (Conversion from endpoint to center
        // parameterization) which is designed to avoid some unnecessary
        // calculations.

        if (rx == 0.0 || ry == 0.0) {
          // F.6.6 step 1 - straight line or coincidental points
          segStartAngle = segEndAngle = AngleOfVector(segEnd, segStart);
          i += 7;
          break;
        }
        rx = fabs(rx);  // F.6.6.1
        ry = fabs(ry);

        // F.6.5.1:
        angle = angle * M_PI / 180.0;
        double x1p = cos(angle) * (segStart.x - segEnd.x) / 2.0 +
                     sin(angle) * (segStart.y - segEnd.y) / 2.0;
        double y1p = -sin(angle) * (segStart.x - segEnd.x) / 2.0 +
                     cos(angle) * (segStart.y - segEnd.y) / 2.0;

        // This is the root in F.6.5.2 and the numerator under that root:
        double root;
        double numerator =
            rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;

        if (numerator >= 0.0) {
          root = sqrt(numerator / (rx * rx * y1p * y1p + ry * ry * x1p * x1p));
          if (largeArcFlag == sweepFlag) root = -root;
        } else {
          // F.6.6 step 3 - |numerator < 0.0|. This is equivalent to the result
          // of F.6.6.2 (lamedh) being greater than one. What we have here is
          // ellipse radii that are too small for the ellipse to reach between
          // segStart and segEnd. We scale the radii up uniformly so that the
          // ellipse is just big enough to fit (i.e. to the point where there is
          // exactly one solution).

          double lamedh =
              1.0 - numerator / (rx * rx * ry * ry);  // equiv to eqn F.6.6.2
          double s = sqrt(lamedh);
          rx *= s;  // F.6.6.3
          ry *= s;
          root = 0.0;
        }

        double cxp = root * rx * y1p / ry;  // F.6.5.2
        double cyp = -root * ry * x1p / rx;

        double theta, delta;
        theta = AngleOfVector(
            Point((x1p - cxp) / rx, (y1p - cyp) / ry));  // F.6.5.5
        delta = AngleOfVector(
                    Point((-x1p - cxp) / rx, (-y1p - cyp) / ry)) -  // F.6.5.6
                theta;
        if (!sweepFlag && delta > 0)
          delta -= 2.0 * M_PI;
        else if (sweepFlag && delta < 0)
          delta += 2.0 * M_PI;

        double tx1, ty1, tx2, ty2;
        tx1 = -cos(angle) * rx * sin(theta) - sin(angle) * ry * cos(theta);
        ty1 = -sin(angle) * rx * sin(theta) + cos(angle) * ry * cos(theta);
        tx2 = -cos(angle) * rx * sin(theta + delta) -
              sin(angle) * ry * cos(theta + delta);
        ty2 = -sin(angle) * rx * sin(theta + delta) +
              cos(angle) * ry * cos(theta + delta);

        if (delta < 0.0f) {
          tx1 = -tx1;
          ty1 = -ty1;
          tx2 = -tx2;
          ty2 = -ty2;
        }

        segStartAngle = static_cast<float>(atan2(ty1, tx1));
        segEndAngle = static_cast<float>(atan2(ty2, tx2));
        i += 7;
        break;
      }

      case PATHSEG_LINETO_HORIZONTAL_ABS:
      case PATHSEG_LINETO_HORIZONTAL_REL:
        if (segType == PATHSEG_LINETO_HORIZONTAL_ABS) {
          segEnd = Point(mData[i++], segStart.y);
        } else {
          segEnd = segStart + Point(mData[i++], 0.0f);
        }
        segStartAngle = segEndAngle = AngleOfVector(segEnd, segStart);
        break;

      case PATHSEG_LINETO_VERTICAL_ABS:
      case PATHSEG_LINETO_VERTICAL_REL:
        if (segType == PATHSEG_LINETO_VERTICAL_ABS) {
          segEnd = Point(segStart.x, mData[i++]);
        } else {
          segEnd = segStart + Point(0.0f, mData[i++]);
        }
        segStartAngle = segEndAngle = AngleOfVector(segEnd, segStart);
        break;

      case PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
      case PATHSEG_CURVETO_CUBIC_SMOOTH_REL: {
        Point cp1 = SVGPathSegUtils::IsCubicType(prevSegType)
                        ? segStart * 2 - prevCP
                        : segStart;
        Point cp2;
        if (segType == PATHSEG_CURVETO_CUBIC_SMOOTH_ABS) {
          cp2 = Point(mData[i], mData[i + 1]);
          segEnd = Point(mData[i + 2], mData[i + 3]);
        } else {
          cp2 = segStart + Point(mData[i], mData[i + 1]);
          segEnd = segStart + Point(mData[i + 2], mData[i + 3]);
        }
        prevCP = cp2;
        segStartAngle = AngleOfVector(
            cp1 == segStart ? (cp1 == cp2 ? segEnd : cp2) : cp1, segStart);
        segEndAngle = AngleOfVector(
            segEnd, cp2 == segEnd ? (cp1 == cp2 ? segStart : cp1) : cp2);
        i += 4;
        break;
      }

      case PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
      case PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL: {
        Point cp1 = SVGPathSegUtils::IsQuadraticType(prevSegType)
                        ? segStart * 2 - prevCP
                        : segStart;
        if (segType == PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS) {
          segEnd = Point(mData[i], mData[i + 1]);
        } else {
          segEnd = segStart + Point(mData[i], mData[i + 1]);
        }
        prevCP = cp1;
        segStartAngle = AngleOfVector(cp1 == segStart ? segEnd : cp1, segStart);
        segEndAngle = AngleOfVector(segEnd, cp1 == segEnd ? segStart : cp1);
        i += 2;
        break;
      }

      default:
        // Leave any existing marks in aMarks so we have a visual indication of
        // when things went wrong.
        MOZ_ASSERT(false, "Unknown segment type - path corruption?");
        return;
    }

    // Set the angle of the mark at the start of this segment:
    if (aMarks->Length()) {
      SVGMark& mark = aMarks->LastElement();
      if (!IsMoveto(segType) && IsMoveto(prevSegType)) {
        // start of new subpath
        pathStartAngle = mark.angle = segStartAngle;
      } else if (IsMoveto(segType) && !IsMoveto(prevSegType)) {
        // end of a subpath
        if (prevSegType != PATHSEG_CLOSEPATH) mark.angle = prevSegEndAngle;
      } else {
        if (!(segType == PATHSEG_CLOSEPATH && prevSegType == PATHSEG_CLOSEPATH))
          mark.angle =
              SVGContentUtils::AngleBisect(prevSegEndAngle, segStartAngle);
      }
    }

    // Add the mark at the end of this segment, and set its position:
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    aMarks->AppendElement(SVGMark(static_cast<float>(segEnd.x),
                                  static_cast<float>(segEnd.y), 0.0f,
                                  SVGMark::eMid));

    if (segType == PATHSEG_CLOSEPATH && prevSegType != PATHSEG_CLOSEPATH) {
      aMarks->LastElement().angle = aMarks->ElementAt(pathStartIndex).angle =
          SVGContentUtils::AngleBisect(segEndAngle, pathStartAngle);
    }

    prevSegType = segType;
    prevSegEnd = segEnd;
    prevSegEndAngle = segEndAngle;
  }

  MOZ_ASSERT(i == mData.Length(), "Very, very bad - mData corrupt");

  if (aMarks->Length()) {
    if (prevSegType != PATHSEG_CLOSEPATH) {
      aMarks->LastElement().angle = prevSegEndAngle;
    }
    aMarks->LastElement().type = SVGMark::eEnd;
    aMarks->ElementAt(0).type = SVGMark::eStart;
  }
}

size_t SVGPathData::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  return mData.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

size_t SVGPathData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

}  // namespace mozilla
