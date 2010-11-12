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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "SVGPathData.h"
#include "SVGAnimatedPathSegList.h"
#include "SVGPathSegUtils.h"
#include "nsSVGElement.h"
#include "nsISVGValueUtils.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsSVGUtils.h"
#include "string.h"
#include "nsSVGPathDataParser.h"
#include "nsSVGPathGeometryElement.h" // for nsSVGMark
#include "gfxPlatform.h"
#include <stdarg.h>

using namespace mozilla;

nsresult
SVGPathData::CopyFrom(const SVGPathData& rhs)
{
  if (!mData.SetCapacity(rhs.mData.Length())) {
    // Yes, we do want fallible alloc here
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mData = rhs.mData;
  return NS_OK;
}

void
SVGPathData::GetValueAsString(nsAString& aValue) const
{
  // we need this function in DidChangePathSegList
  aValue.Truncate();
  if (!Length()) {
    return;
  }
  PRUint32 i = 0;
  for (;;) {
    nsAutoString segAsString;
    SVGPathSegUtils::GetValueAsString(&mData[i], segAsString);
    // We ignore OOM, since it's not useful for us to return an error.
    aValue.Append(segAsString);
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
    if (i >= mData.Length()) {
      NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt");
      return;
    }
    aValue.Append(' ');
  }
}

nsresult
SVGPathData::SetValueFromString(const nsAString& aValue)
{
  // We don't use a temp variable since the spec says to parse everything up to
  // the first error. We still return any error though so that callers know if
  // there's a problem.

  nsSVGPathDataParserToInternal pathParser(this);
  return pathParser.Parse(aValue);
}

nsresult
SVGPathData::AppendSeg(PRUint32 aType, ...)
{
  PRUint32 oldLength = mData.Length();
  PRUint32 newLength = oldLength + 1 + SVGPathSegUtils::ArgCountForType(aType);
  if (!mData.SetLength(newLength)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mData[oldLength] = SVGPathSegUtils::EncodeType(aType);
  va_list args;
  va_start(args, aType);
  for (PRUint32 i = oldLength + 1; i < newLength; ++i) {
    // NOTE! 'float' is promoted to 'double' when passed through '...'!
    mData[i] = float(va_arg(args, double));
  }
  va_end(args);
  return NS_OK;
}

float
SVGPathData::GetPathLength() const
{
  float length = 0.0;
  SVGPathTraversalState state;

  PRUint32 i = 0;
  while (i < mData.Length()) {
    length += SVGPathSegUtils::GetLength(&mData[i], state);
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
  }

  NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt");

  return length;
}

#ifdef DEBUG
PRUint32
SVGPathData::CountItems() const
{
  PRUint32 i = 0, count = 0;

  while (i < mData.Length()) {
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
    count++;
  }

  NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt");

  return count;
}
#endif

PRBool
SVGPathData::GetSegmentLengths(nsTArray<double> *aLengths) const
{
  aLengths->Clear();
  SVGPathTraversalState state;

  PRUint32 i = 0;
  while (i < mData.Length()) {
    if (!aLengths->AppendElement(SVGPathSegUtils::GetLength(&mData[i], state))) {
      aLengths->Clear();
      return PR_FALSE;
    }
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
  }

  NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt");

  return PR_TRUE;
}

PRBool
SVGPathData::GetDistancesFromOriginToEndsOfVisibleSegments(nsTArray<double> *aOutput) const
{
  double distRunningTotal = 0.0;
  SVGPathTraversalState state;

  aOutput->Clear();

  PRUint32 i = 0;
  while (i < mData.Length()) {
    PRUint32 segType = SVGPathSegUtils::DecodeType(mData[i]);

    // We skip all moveto commands except an initial moveto. See the text 'A
    // "move to" command does not count as an additional point when dividing up
    // the duration...':
    //
    // http://www.w3.org/TR/SVG11/animate.html#AnimateMotionElement
    //
    // This is important in the non-default case of calcMode="linear". In
    // this case an equal amount of time is spent on each path segment,
    // except on moveto segments which are jumped over immediately.

    if (i == 0 || (segType != nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS &&
                   segType != nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL)) {
      distRunningTotal += SVGPathSegUtils::GetLength(&mData[i], state);
      if (!aOutput->AppendElement(distRunningTotal)) {
        return PR_FALSE;
      }
    }
    i += 1 + SVGPathSegUtils::ArgCountForType(segType);
  }

  NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt?");

  return PR_TRUE;
}

PRUint32
SVGPathData::GetPathSegAtLength(float aDistance) const
{
  // TODO [SVGWG issue] get specified what happen if 'aDistance' < 0, or
  // 'aDistance' > the length of the path, or the seg list is empty.
  // Return -1? Throwing would better help authors avoid tricky bugs (DOM
  // could do that if we return -1).

  double distRunningTotal = 0.0;
  PRUint32 i = 0, segIndex = 0;
  SVGPathTraversalState state;

  while (i < mData.Length()) {
    distRunningTotal += SVGPathSegUtils::GetLength(&mData[i], state);
    if (distRunningTotal >= aDistance) {
      return segIndex;
    }
    i += 1 + SVGPathSegUtils::ArgCountForType(mData[i]);
    segIndex++;
  }

  NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt");

  return NS_MAX(0U, segIndex - 1); // -1 because while loop takes us 1 too far
}

void
SVGPathData::ConstructPath(gfxContext *aCtx) const
{
  PRUint32 segType, prevSegType = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;
  gfxPoint pathStart(0.0, 0.0); // start point of [sub]path
  gfxPoint segEnd(0.0, 0.0);    // end point of previous/current segment
  gfxPoint cp1, cp2;            // previous bezier's control points
  gfxPoint tcp1, tcp2;          // temporaries

  // Regarding cp1 and cp2: If the previous segment was a cubic bezier curve,
  // then cp2 is its second control point. If the previous segment was a
  // quadratic curve, then cp1 is its (only) control point.

  PRUint32 i = 0;
  while (i < mData.Length()) {
    segType = SVGPathSegUtils::DecodeType(mData[i++]);

    switch (segType)
    {
    case nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH:
      segEnd = pathStart;
      aCtx->ClosePath();
      break;

    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS:
      pathStart = segEnd = gfxPoint(mData[i], mData[i+1]);
      aCtx->MoveTo(segEnd);
      i += 2;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL:
      pathStart = segEnd += gfxPoint(mData[i], mData[i+1]);
      aCtx->MoveTo(segEnd);
      i += 2;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS:
      segEnd = gfxPoint(mData[i], mData[i+1]);
      aCtx->LineTo(segEnd);
      i += 2;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_REL:
      segEnd += gfxPoint(mData[i], mData[i+1]);
      aCtx->LineTo(segEnd);
      i += 2;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
      cp1 = gfxPoint(mData[i], mData[i+1]);
      cp2 = gfxPoint(mData[i+2], mData[i+3]);
      segEnd = gfxPoint(mData[i+4], mData[i+5]);
      aCtx->CurveTo(cp1, cp2, segEnd);
      i += 6;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
      cp1 = segEnd + gfxPoint(mData[i], mData[i+1]);
      cp2 = segEnd + gfxPoint(mData[i+2], mData[i+3]);
      segEnd += gfxPoint(mData[i+4], mData[i+5]);
      aCtx->CurveTo(cp1, cp2, segEnd);
      i += 6;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
      cp1 = gfxPoint(mData[i], mData[i+1]);
      // Convert quadratic curve to cubic curve:
      tcp1 = segEnd + (cp1 - segEnd) * 2 / 3;
      segEnd = gfxPoint(mData[i+2], mData[i+3]); // changed before setting tcp2!
      tcp2 = cp1 + (segEnd - cp1) / 3;
      aCtx->CurveTo(tcp1, tcp2, segEnd);
      i += 4;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
      cp1 = segEnd + gfxPoint(mData[i], mData[i+1]);
      // Convert quadratic curve to cubic curve:
      tcp1 = segEnd + (cp1 - segEnd) * 2 / 3;
      segEnd += gfxPoint(mData[i+2], mData[i+3]); // changed before setting tcp2!
      tcp2 = cp1 + (segEnd - cp1) / 3;
      aCtx->CurveTo(tcp1, tcp2, segEnd);
      i += 4;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_ARC_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_ARC_REL:
    {
      gfxPoint radii(mData[i], mData[i+1]);
      gfxPoint start = segEnd;
      gfxPoint end = gfxPoint(mData[i+5], mData[i+6]);
      if (segType == nsIDOMSVGPathSeg::PATHSEG_ARC_REL) {
        end += start;
      }
      segEnd = end;
      if (start != end) {
        if (radii.x == 0.0f || radii.y == 0.0f) {
          aCtx->LineTo(end);
          i += 7;
          break;
        }
        nsSVGArcConverter converter(start, end, radii, mData[i+2],
                                    mData[i+3] != 0, mData[i+4] != 0);
        while (converter.GetNextSegment(&cp1, &cp2, &end)) {
          aCtx->CurveTo(cp1, cp2, end);
        }
      }
      i += 7;
      break;
    }

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
      segEnd = gfxPoint(mData[i++], segEnd.y);
      aCtx->LineTo(segEnd);
      break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
      segEnd += gfxPoint(mData[i++], 0.0f);
      aCtx->LineTo(segEnd);
      break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
      segEnd = gfxPoint(segEnd.x, mData[i++]);
      aCtx->LineTo(segEnd);
      break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
      segEnd += gfxPoint(0.0f, mData[i++]);
      aCtx->LineTo(segEnd);
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
      cp1 = SVGPathSegUtils::IsCubicType(prevSegType) ? segEnd * 2 - cp2 : segEnd;
      cp2 = gfxPoint(mData[i],   mData[i+1]);
      segEnd = gfxPoint(mData[i+2], mData[i+3]);
      aCtx->CurveTo(cp1, cp2, segEnd);
      i += 4;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
      cp1 = SVGPathSegUtils::IsCubicType(prevSegType) ? segEnd * 2 - cp2 : segEnd;
      cp2 = segEnd + gfxPoint(mData[i], mData[i+1]);
      segEnd += gfxPoint(mData[i+2], mData[i+3]);
      aCtx->CurveTo(cp1, cp2, segEnd);
      i += 4;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
      cp1 = SVGPathSegUtils::IsQuadraticType(prevSegType) ? segEnd * 2 - cp1 : segEnd;
      // Convert quadratic curve to cubic curve:
      tcp1 = segEnd + (cp1 - segEnd) * 2 / 3;
      segEnd = gfxPoint(mData[i], mData[i+1]); // changed before setting tcp2!
      tcp2 = cp1 + (segEnd - cp1) / 3;
      aCtx->CurveTo(tcp1, tcp2, segEnd);
      i += 2;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
      cp1 = SVGPathSegUtils::IsQuadraticType(prevSegType) ? segEnd * 2 - cp1 : segEnd;
      // Convert quadratic curve to cubic curve:
      tcp1 = segEnd + (cp1 - segEnd) * 2 / 3;
      segEnd = segEnd + gfxPoint(mData[i], mData[i+1]); // changed before setting tcp2!
      tcp2 = cp1 + (segEnd - cp1) / 3;
      aCtx->CurveTo(tcp1, tcp2, segEnd);
      i += 2;
      break;

    default:
      NS_NOTREACHED("Bad path segment type");
      return; // according to spec we'd use everything up to the bad seg anyway
    }
    prevSegType = segType;
  }
  NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt");
}

already_AddRefed<gfxFlattenedPath>
SVGPathData::ToFlattenedPath(const gfxMatrix& aMatrix) const
{
  nsRefPtr<gfxContext> ctx =
    new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());

  ctx->SetMatrix(aMatrix);
  ConstructPath(ctx);
  ctx->IdentityMatrix();

  return ctx->GetFlattenedPath();
}

static PRBool IsMoveto(PRUint16 aSegType)
{
  return aSegType == nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS ||
         aSegType == nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL;
}

static float AngleOfVector(gfxPoint v)
{
  // C99 says about atan2 "A domain error may occur if both arguments are
  // zero" and "On a domain error, the function returns an implementation-
  // defined value". In the case of atan2 the implementation-defined value
  // seems to commonly be zero, but it could just as easily be a NaN value.
  // We specifically want zero in this case, hence the check:

  return (v != gfxPoint(0.0f, 0.0f)) ? atan2(v.y, v.x) : 0.0f;
}

// TODO replace callers with calls to AngleOfVector
static double
CalcVectorAngle(double ux, double uy, double vx, double vy)
{
  double ta = atan2(uy, ux);
  double tb = atan2(vy, vx);
  if (tb >= ta)
    return tb-ta;
  return 2 * M_PI - (ta-tb);
}

void
SVGPathData::GetMarkerPositioningData(nsTArray<nsSVGMark> *aMarks) const
{
  // This code should assume that ANY type of segment can appear at ANY index.
  // It should also assume that segments such as M and Z can appear in weird
  // places, and repeat multiple times consecutively.

  gfxPoint pathStart, segStart, segEnd;
  gfxPoint cp1, cp2; // control points for current bezier curve
  gfxPoint prevCP; // last control point of previous bezier curve

  PRUint16 segType, prevSegType = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;

  // info on the current [sub]path (reset every M command):
  gfxPoint pathStartPoint(0, 0);
  float pathStartAngle = 0;

  float prevSegEndAngle = 0, segStartAngle = 0, segEndAngle = 0;

  PRUint32 i = 0;
  while (i < mData.Length()) {
    segType = SVGPathSegUtils::DecodeType(mData[i++]); // advances i to args

    switch (segType) // to find segStartAngle, segEnd and segEndAngle
    {
    case nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH:
      segEnd = pathStart;
      segStartAngle = segEndAngle = AngleOfVector(segEnd - segStart);
      break;

    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL:
      if (segType == nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS) {
        segEnd = gfxPoint(mData[i], mData[i+1]);
      } else {
        segEnd = segStart + gfxPoint(mData[i], mData[i+1]);
      }
      pathStart = segEnd;
      // If authors are going to specify multiple consecutive moveto commands
      // with markers, me might as well make the angle do something useful:
      segStartAngle = segEndAngle = AngleOfVector(segEnd - segStart);
      i += 2;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_REL:
      if (segType == nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS) {
        segEnd = gfxPoint(mData[i], mData[i+1]);
      } else {
        segEnd = segStart + gfxPoint(mData[i], mData[i+1]);
      }
      segStartAngle = segEndAngle = AngleOfVector(segEnd - segStart);
      i += 2;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
      if (segType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS) {
        cp1 = gfxPoint(mData[i],   mData[i+1]);
        cp2 = gfxPoint(mData[i+2], mData[i+3]);
        segEnd = gfxPoint(mData[i+4], mData[i+5]);
      } else {
        cp1 = segStart + gfxPoint(mData[i],   mData[i+1]);
        cp2 = segStart + gfxPoint(mData[i+2], mData[i+3]);
        segEnd = segStart + gfxPoint(mData[i+4], mData[i+5]);
      }
      prevCP = cp2;
      if (cp1 == segStart) {
        cp1 = cp2;
      }
      if (cp2 == segEnd) {
        cp2 = cp1;
      }
      segStartAngle = AngleOfVector(cp1 - segStart);
      segEndAngle = AngleOfVector(segEnd - cp2);
      i += 6;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
      if (segType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS) {
        cp1 = gfxPoint(mData[i],   mData[i+1]);
        segEnd = gfxPoint(mData[i+2], mData[i+3]);
      } else {
        cp1 = segStart + gfxPoint(mData[i],   mData[i+1]);
        segEnd = segStart + gfxPoint(mData[i+2], mData[i+3]);
      }
      prevCP = cp1;
      segStartAngle = AngleOfVector(cp1 - segStart);
      segEndAngle = AngleOfVector(segEnd - cp1);
      i += 4;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_ARC_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_ARC_REL:
    {
      float rx = mData[i];
      float ry = mData[i+1];
      float angle = mData[i+2];
      PRBool largeArcFlag = mData[i+3] != 0.0f;
      PRBool sweepFlag = mData[i+4] != 0.0f;
      if (segType == nsIDOMSVGPathSeg::PATHSEG_ARC_ABS) {
        segEnd = gfxPoint(mData[i+5], mData[i+6]);
      } else {
        segEnd = segStart + gfxPoint(mData[i+5], mData[i+6]);
      }

      // See section F.6 of SVG 1.1 for details on what we're doing here:
      // http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes

      if (segStart == segEnd) {
        // F.6.2 says "If the endpoints (x1, y1) and (x2, y2) are identical,
        // then this is equivalent to omitting the elliptical arc segment
        // entirely." We take that very literally here, not adding a mark, and
        // not even setting any of the 'prev' variables so that it's as if this
        // arc had never existed; note the difference this will make e.g. if
        // the arc is proceeded by a bezier curve and followed by a "smooth"
        // bezier curve of the same degree!
        i += 7;
        continue;
      }

      // Below we have funny interleaving of F.6.6 (Correction of out-of-range
      // radii) and F.6.5 (Conversion from endpoint to center parameterization)
      // which is designed to avoid some unnecessary calculations.

      if (rx == 0.0 || ry == 0.0) {
        // F.6.6 step 1 - straight line or coincidental points
        segStartAngle = segEndAngle = AngleOfVector(segEnd - segStart);
        i += 7;
        break;
      }
      rx = fabs(rx); // F.6.6.1
      ry = fabs(ry);

      // F.6.5.1:
      angle = angle * M_PI/180.0;
      float x1p = cos(angle) * (segStart.x - segEnd.x) / 2.0
                + sin(angle) * (segStart.y - segEnd.y) / 2.0;
      float y1p = -sin(angle) * (segStart.x - segEnd.x) / 2.0
                 + cos(angle)  *(segStart.y - segEnd.y) / 2.0;

      // This is the root in F.6.5.2 and the numerator under that root:
      float root;
      float numerator = rx*rx*ry*ry - rx*rx*y1p*y1p - ry*ry*x1p*x1p;

      if (numerator < 0.0) {
        // F.6.6 step 3 - |numerator < 0.0| is equivalent to the result of
        // F.6.6.2 (lamedh) being greater than one. What we have here is radii
        // that do not reach between segStart and segEnd, so we need to correct
        // them.
        float lamedh = 1.0 - numerator/(rx*rx*ry*ry); // equiv to eqn F.6.6.2
        float s = sqrt(lamedh);
        rx *= s;  // F.6.6.3
        ry *= s;
        // rx and ry changed, so we have to recompute numerator
        numerator = rx*rx*ry*ry - rx*rx*y1p*y1p - ry*ry*x1p*x1p;
        NS_ABORT_IF_FALSE(numerator >= 0,
                          "F.6.6.3 should prevent this. Will sqrt(-num)!");
      }
      root = sqrt(numerator/(rx*rx*y1p*y1p + ry*ry*x1p*x1p));
      if (largeArcFlag == sweepFlag)
        root = -root;

      float cxp =  root * rx * y1p / ry;  // F.6.5.2
      float cyp = -root * ry * x1p / rx;

      float theta, delta;
      theta = CalcVectorAngle(1.0, 0.0,  (x1p-cxp)/rx, (y1p-cyp)/ry); // F.6.5.5
      delta  = CalcVectorAngle((x1p-cxp)/rx, (y1p-cyp)/ry,
                               (-x1p-cxp)/rx, (-y1p-cyp)/ry);         // F.6.5.6
      if (!sweepFlag && delta > 0)
        delta -= 2.0 * M_PI;
      else if (sweepFlag && delta < 0)
        delta += 2.0 * M_PI;

      float tx1, ty1, tx2, ty2;
      tx1 = -cos(angle)*rx*sin(theta) - sin(angle)*ry*cos(theta);
      ty1 = -sin(angle)*rx*sin(theta) + cos(angle)*ry*cos(theta);
      tx2 = -cos(angle)*rx*sin(theta+delta) - sin(angle)*ry*cos(theta+delta);
      ty2 = -sin(angle)*rx*sin(theta+delta) + cos(angle)*ry*cos(theta+delta);

      if (delta < 0.0f) {
        tx1 = -tx1;
        ty1 = -ty1;
        tx2 = -tx2;
        ty2 = -ty2;
      }

      segStartAngle = atan2(ty1, tx1);
      segEndAngle = atan2(ty2, tx2);
      i += 7;
      break;
    }

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
      if (segType == nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS) {
        segEnd = gfxPoint(mData[i++], segStart.y);
      } else {
        segEnd = segStart + gfxPoint(mData[i++], 0.0f);
      }
      segStartAngle = segEndAngle = AngleOfVector(segEnd - segStart);
      break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
      if (segType == nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS) {
        segEnd = gfxPoint(segStart.x, mData[i++]);
      } else {
        segEnd = segStart + gfxPoint(0.0f, mData[i++]);
      }
      segStartAngle = segEndAngle = AngleOfVector(segEnd - segStart);
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
      cp1 = SVGPathSegUtils::IsCubicType(prevSegType) ? segStart * 2 - prevCP : segStart;
      if (segType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS) {
        cp2 = gfxPoint(mData[i], mData[i+1]);
        segEnd = gfxPoint(mData[i+2], mData[i+3]);
      } else {
        cp2 = segStart + gfxPoint(mData[i], mData[i+1]);
        segEnd = segStart + gfxPoint(mData[i+2], mData[i+3]);
      }
      prevCP = cp2;
      if (cp1 == segStart) {
        cp1 = cp2;
      }
      if (cp2 == segEnd) {
        cp2 = cp1;
      }
      segStartAngle = AngleOfVector(cp1 - segStart);
      segEndAngle = AngleOfVector(segEnd - cp2);
      i += 4;
      break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
      cp1 = SVGPathSegUtils::IsQuadraticType(prevSegType) ? segStart * 2 - prevCP : segStart;
      if (segType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS) {
        segEnd = gfxPoint(mData[i], mData[i+1]);
      } else {
        segEnd = segStart + gfxPoint(mData[i], mData[i+1]);
      }
      prevCP = cp1;
      segStartAngle = AngleOfVector(cp1 - segStart);
      segEndAngle = AngleOfVector(segEnd - cp1);
      i += 2;
      break;

    default:
      // Leave any existing marks in aMarks so we have a visual indication of
      // when things went wrong.
      NS_ABORT_IF_FALSE(PR_FALSE, "Unknown segment type - path corruption?");
      return;
    }

    // Set the angle of the mark at the start of this segment:
    if (aMarks->Length()) {
      nsSVGMark &mark = aMarks->ElementAt(aMarks->Length() - 1);
      if (!IsMoveto(segType) && IsMoveto(prevSegType)) {
        // start of new subpath
        pathStartAngle = mark.angle = segStartAngle;
      } else if (IsMoveto(segType) && !IsMoveto(prevSegType)) {
        // end of a subpath
        if (prevSegType != nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH)
          mark.angle = prevSegEndAngle;
      } else {
        if (!(segType == nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH &&
              prevSegType == nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH))
          mark.angle = nsSVGUtils::AngleBisect(prevSegEndAngle, segStartAngle);
      }
    }

    // Add the mark at the end of this segment, and set its position:
    if (!aMarks->AppendElement(nsSVGMark(segEnd.x, segEnd.y, 0))) {
      aMarks->Clear(); // OOM, so try to free some
      return;
    }

    if (segType == nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH &&
        prevSegType != nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH) {
      aMarks->ElementAt(aMarks->Length() - 1).angle =
        //aMarks->ElementAt(pathStartIndex).angle =
        nsSVGUtils::AngleBisect(segEndAngle, pathStartAngle);
    }

    prevSegType = segType;
    prevSegEndAngle = segEndAngle;
    segStart = segEnd;
  }

  NS_ABORT_IF_FALSE(i == mData.Length(), "Very, very bad - mData corrupt");

  if (aMarks->Length() &&
      prevSegType != nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH)
    aMarks->ElementAt(aMarks->Length() - 1).angle = prevSegEndAngle;
}

