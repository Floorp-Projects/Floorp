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

#include "SVGPathSegUtils.h"
#include "nsSVGElement.h"
#include "nsSVGSVGElement.h"
#include "nsSVGPathDataParser.h"
#include "nsString.h"
#include "nsSVGUtils.h"
#include "nsContentUtils.h"
#include "nsTextFormatter.h"
#include "prdtoa.h"
#include <limits>
#include "nsMathUtils.h"
#include "prtypes.h"

using namespace mozilla;

static const float PATH_SEG_LENGTH_TOLERANCE = 0.0000001f;
static const PRUint32 MAX_RECURSION = 10;


/* static */ void
SVGPathSegUtils::GetValueAsString(const float *aSeg, nsAString& aValue)
{
  // Adding new seg type? Is the formatting below acceptable for the new types?
  PR_STATIC_ASSERT(NS_SVG_PATH_SEG_MAX_ARGS == 7);

  PRUint32 type = DecodeType(aSeg[0]);
  PRUnichar typeAsChar = GetPathSegTypeAsLetter(type);

  // Special case arcs:
  if (type == nsIDOMSVGPathSeg::PATHSEG_ARC_ABS ||
      type == nsIDOMSVGPathSeg::PATHSEG_ARC_REL) {
    PRBool largeArcFlag = aSeg[4] != 0.0f;
    PRBool sweepFlag = aSeg[5] != 0.0f;
    nsTextFormatter::ssprintf(aValue,
                              NS_LITERAL_STRING("%c%g,%g %g %d,%d %g,%g").get(),
                              typeAsChar, aSeg[1], aSeg[2], aSeg[3],
                              largeArcFlag, sweepFlag, aSeg[6], aSeg[7]);
  } else {

    switch (ArgCountForType(type)) {
    case 0:
      aValue = typeAsChar;
      break;

    case 1:
      nsTextFormatter::ssprintf(aValue, NS_LITERAL_STRING("%c%g").get(),
                                typeAsChar, aSeg[1]);
      break;

    case 2:
      nsTextFormatter::ssprintf(aValue, NS_LITERAL_STRING("%c%g,%g").get(),
                                typeAsChar, aSeg[1], aSeg[2]);
      break;

    case 4:
      nsTextFormatter::ssprintf(aValue, NS_LITERAL_STRING("%c%g,%g %g,%g").get(),
                                typeAsChar, aSeg[1], aSeg[2], aSeg[3], aSeg[4]);
      break;

    case 6:
      nsTextFormatter::ssprintf(aValue,
                                NS_LITERAL_STRING("%c%g,%g %g,%g %g,%g").get(),
                                typeAsChar, aSeg[1], aSeg[2], aSeg[3], aSeg[4],
                                aSeg[5], aSeg[6]);
      break;

    default:
      NS_ABORT_IF_FALSE(PR_FALSE, "Unknown segment type");
      aValue = NS_LITERAL_STRING("<unknown-segment-type>").get();
      return;
    }
  }
  
  // nsTextFormatter::ssprintf is one of the nsTextFormatter methods that
  // randomly appends '\0' to its output string, which means that the length
  // of the output string is one too long. We need to manually remove that '\0'
  // until nsTextFormatter is fixed.
  //
  if (aValue[aValue.Length() - 1] == PRUnichar('\0')) {
    aValue.SetLength(aValue.Length() - 1);
  }
}


static float
CalcDistanceBetweenPoints(const gfxPoint &p1, const gfxPoint &p2)
{
  return NS_hypot(p2.x - p1.x, p2.y - p1.y);
}


static void SplitQuadraticBezier(const gfxPoint *curve,
                                 gfxPoint *left,
                                 gfxPoint *right)
{
  left[0].x = curve[0].x;
  left[0].y = curve[0].y;
  right[2].x = curve[2].x;
  right[2].y = curve[2].y;
  left[1].x = (curve[0].x + curve[1].x) / 2;
  left[1].y = (curve[0].y + curve[1].y) / 2;
  right[1].x = (curve[1].x + curve[2].x) / 2;
  right[1].y = (curve[1].y + curve[2].y) / 2;
  left[2].x = right[0].x = (left[1].x + right[1].x) / 2;
  left[2].y = right[0].y = (left[1].y + right[1].y) / 2;
}

static void SplitCubicBezier(const gfxPoint *curve,
                             gfxPoint *left,
                             gfxPoint *right)
{
  gfxPoint tmp;
  tmp.x = (curve[1].x + curve[2].x) / 4;
  tmp.y = (curve[1].y + curve[2].y) / 4;
  left[0].x = curve[0].x;
  left[0].y = curve[0].y;
  right[3].x = curve[3].x;
  right[3].y = curve[3].y;
  left[1].x = (curve[0].x + curve[1].x) / 2;
  left[1].y = (curve[0].y + curve[1].y) / 2;
  right[2].x = (curve[2].x + curve[3].x) / 2;
  right[2].y = (curve[2].y + curve[3].y) / 2;
  left[2].x = left[1].x / 2 + tmp.x;
  left[2].y = left[1].y / 2 + tmp.y;
  right[1].x = right[2].x / 2 + tmp.x;
  right[1].y = right[2].y / 2 + tmp.y;
  left[3].x = right[0].x = (left[2].x + right[1].x) / 2;
  left[3].y = right[0].y = (left[2].y + right[1].y) / 2;
}

static gfxFloat CalcBezLengthHelper(gfxPoint *curve, PRUint32 numPts,
                                    PRUint32 recursion_count,
                                    void (*split)(const gfxPoint*, gfxPoint*, gfxPoint*))
{
  gfxPoint left[4];
  gfxPoint right[4];
  gfxFloat length = 0, dist;
  for (PRUint32 i = 0; i < numPts - 1; i++) {
    length += CalcDistanceBetweenPoints(curve[i], curve[i+1]);
  }
  dist = CalcDistanceBetweenPoints(curve[0], curve[numPts - 1]);
  if (length - dist > PATH_SEG_LENGTH_TOLERANCE && recursion_count < MAX_RECURSION) {
    split(curve, left, right);
    ++recursion_count;
    return CalcBezLengthHelper(left, numPts, recursion_count, split) +
           CalcBezLengthHelper(right, numPts, recursion_count, split);
  }
  return length;
}

static inline gfxFloat
CalcLengthOfCubicBezier(const gfxPoint &pos, const gfxPoint &cp1,
                        const gfxPoint &cp2, const gfxPoint &to)
{
  gfxPoint curve[4] = { pos, cp1, cp2, to };
  return CalcBezLengthHelper(curve, 4, 0, SplitCubicBezier);
}

static inline gfxFloat
CalcLengthOfQuadraticBezier(const gfxPoint &pos, const gfxPoint &cp,
                            const gfxPoint &to)
{
  gfxPoint curve[3] = { pos, cp, to };
  return CalcBezLengthHelper(curve, 3, 0, SplitQuadraticBezier);
}


static float GetLengthOfClosePath(const float *aArgs, SVGPathTraversalState &aState)
{
  float dist = CalcDistanceBetweenPoints(aState.pos, aState.start);
  aState.pos = aState.cp1 = aState.cp2 = aState.start;
  return dist;
}

static float GetLengthOfMovetoAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  aState.start = aState.pos = aState.cp1 = aState.cp2 = gfxPoint(aArgs[0], aArgs[1]);
  return 0.0;
}

static float GetLengthOfMovetoRel(const float *aArgs, SVGPathTraversalState &aState)
{
  // aState.pos must be second from right due to +=
  aState.start = aState.cp1 = aState.cp2 = aState.pos += gfxPoint(aArgs[0], aArgs[1]);
  return 0.0;
}

static float GetLengthOfLinetoAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint to(aArgs[0], aArgs[1]);
  float dist = CalcDistanceBetweenPoints(aState.pos, to);
  aState.pos = aState.cp1 = aState.cp2 = to;
  return dist;
}

static float GetLengthOfLinetoRel(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint to = aState.pos + gfxPoint(aArgs[0], aArgs[1]);
  float dist = CalcDistanceBetweenPoints(aState.pos, to);
  aState.pos = aState.cp1 = aState.cp2 = to;
  return dist;
}

static float
GetLengthOfLinetoHorizontalAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint to(aArgs[0], aState.pos.y);
  float dist = fabs(to.x - aState.pos.x);
  aState.pos = aState.cp1 = aState.cp2 = to;
  return dist;
}

static float
GetLengthOfLinetoHorizontalRel(const float *aArgs, SVGPathTraversalState &aState)
{
  aState.cp1 = aState.cp2 = aState.pos += gfxPoint(aArgs[0], 0.0);
  return fabs(aArgs[0]);
}

static float
GetLengthOfLinetoVerticalAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint to(aState.pos.x, aArgs[0]);
  float dist = fabs(to.y - aState.pos.y);
  aState.pos = aState.cp1 = aState.cp2 = to;
  return dist;
}

static float
GetLengthOfLinetoVerticalRel(const float *aArgs, SVGPathTraversalState &aState)
{
  aState.cp1 = aState.cp2 = aState.pos += gfxPoint(0.0, aArgs[0]);
  return fabs(aArgs[0]);
}

static float GetLengthOfCurvetoCubicAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp1(aArgs[0], aArgs[1]);
  gfxPoint cp2(aArgs[2], aArgs[3]);
  gfxPoint to(aArgs[4], aArgs[5]);

  float dist = (float)CalcLengthOfCubicBezier(aState.pos, cp1, cp2, to);

  aState.cp2 = cp2;
  aState.pos = aState.cp1 = to;

  return dist;
}

static float
GetLengthOfCurvetoCubicSmoothAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp1 = aState.pos - (aState.cp2 - aState.pos);
  gfxPoint cp2(aArgs[0], aArgs[1]);
  gfxPoint to(aArgs[2], aArgs[3]);

  float dist = (float)CalcLengthOfCubicBezier(aState.pos, cp1, cp2, to);

  aState.cp2 = cp2;
  aState.pos = aState.cp1 = to;

  return dist;
}

static float
GetLengthOfCurvetoCubicRel(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp1 = aState.pos + gfxPoint(aArgs[0], aArgs[1]);
  gfxPoint cp2 = aState.pos + gfxPoint(aArgs[2], aArgs[3]);
  gfxPoint to  = aState.pos + gfxPoint(aArgs[4], aArgs[5]);

  float dist = (float)CalcLengthOfCubicBezier(aState.pos, cp1, cp2, to);

  aState.cp2 = cp2;
  aState.pos = aState.cp1 = to;

  return dist;
}

static float
GetLengthOfCurvetoCubicSmoothRel(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp1 = aState.pos - (aState.cp2 - aState.pos);
  gfxPoint cp2 = aState.pos + gfxPoint(aArgs[0], aArgs[1]);
  gfxPoint to  = aState.pos + gfxPoint(aArgs[2], aArgs[3]);

  float dist = (float)CalcLengthOfCubicBezier(aState.pos, cp1, cp2, to);

  aState.cp2 = cp2;
  aState.pos = aState.cp1 = to;

  return dist;
}

static float
GetLengthOfCurvetoQuadraticAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp(aArgs[0], aArgs[1]);
  gfxPoint to(aArgs[2], aArgs[3]);

  float dist = (float)CalcLengthOfQuadraticBezier(aState.pos, cp, to);

  aState.cp1 = cp;
  aState.pos = aState.cp2 = to;

  return dist;
}

static float
GetLengthOfCurvetoQuadraticSmoothAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp = aState.pos - (aState.cp1 - aState.pos);
  gfxPoint to(aArgs[0], aArgs[1]);

  float dist = (float)CalcLengthOfQuadraticBezier(aState.pos, cp, to);

  aState.cp1 = cp;
  aState.pos = aState.cp2 = to;

  return dist;
}

static float
GetLengthOfCurvetoQuadraticRel(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp = aState.pos + gfxPoint(aArgs[0], aArgs[1]);
  gfxPoint to = aState.pos + gfxPoint(aArgs[2], aArgs[3]);

  float dist = (float)CalcLengthOfQuadraticBezier(aState.pos, cp, to);

  aState.cp1 = cp;
  aState.pos = aState.cp2 = to;

  return dist;
}

static float
GetLengthOfCurvetoQuadraticSmoothRel(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint cp = aState.pos - (aState.cp1 - aState.pos);
  gfxPoint to = aState.pos + gfxPoint(aArgs[0], aArgs[1]);

  float dist = (float)CalcLengthOfQuadraticBezier(aState.pos, cp, to);

  aState.cp1 = cp;
  aState.pos = aState.cp2 = to;

  return dist;
}

static float
GetLengthOfArcAbs(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint radii(aArgs[0], aArgs[1]);
  gfxPoint to(aArgs[5], aArgs[6]);
  gfxPoint bez[4] = { aState.pos, gfxPoint(0,0), gfxPoint(0,0), gfxPoint(0,0) };
  nsSVGArcConverter converter(aState.pos, to, radii, aArgs[2],
                              aArgs[3] != 0, aArgs[4] != 0);
  float dist = 0;
  while (converter.GetNextSegment(&bez[1], &bez[2], &bez[3]))
  {
    dist += CalcBezLengthHelper(bez, 4, 0, SplitCubicBezier);
    bez[0] = bez[3];
  }
  aState.pos = aState.cp1 = aState.cp2 = to;
  return dist;
}

static float
GetLengthOfArcRel(const float *aArgs, SVGPathTraversalState &aState)
{
  gfxPoint radii(aArgs[0], aArgs[1]);
  gfxPoint to = aState.pos + gfxPoint(aArgs[5], aArgs[6]);
  gfxPoint bez[4] = { aState.pos, gfxPoint(0,0), gfxPoint(0,0), gfxPoint(0,0) };
  nsSVGArcConverter converter(aState.pos, to, radii, aArgs[2],
                              aArgs[3] != 0, aArgs[4] != 0);
  float dist = 0;
  while (converter.GetNextSegment(&bez[1], &bez[2], &bez[3]))
  {
    dist += CalcBezLengthHelper(bez, 4, 0, SplitCubicBezier);
    bez[0] = bez[3];
  }
  aState.pos = aState.cp1 = aState.cp2 = to;
  return dist;
}


typedef float (*getLengthFunc)(const float*, SVGPathTraversalState&);

/* static */ float
SVGPathSegUtils::GetLength(const float *seg, SVGPathTraversalState &aState)
{
  PRUint32 type = DecodeType(seg[0]);

  static getLengthFunc lengthFuncTable[20] = {
    nsnull, //  0 == PATHSEG_UNKNOWN
    GetLengthOfClosePath,
    GetLengthOfMovetoAbs,
    GetLengthOfMovetoRel,
    GetLengthOfLinetoAbs,
    GetLengthOfLinetoRel,
    GetLengthOfCurvetoCubicAbs,
    GetLengthOfCurvetoCubicRel,
    GetLengthOfCurvetoQuadraticAbs,
    GetLengthOfCurvetoQuadraticRel,
    GetLengthOfArcAbs,
    GetLengthOfArcRel,
    GetLengthOfLinetoHorizontalAbs,
    GetLengthOfLinetoHorizontalRel,
    GetLengthOfLinetoVerticalAbs,
    GetLengthOfLinetoVerticalRel,
    GetLengthOfCurvetoCubicSmoothAbs,
    GetLengthOfCurvetoCubicSmoothRel,
    GetLengthOfCurvetoQuadraticSmoothAbs,
    GetLengthOfCurvetoQuadraticSmoothRel
  };

  NS_ABORT_IF_FALSE(IsValidType(type), "Seg type not recognized");

  NS_ABORT_IF_FALSE(type > 0 && type < NS_ARRAY_LENGTH(lengthFuncTable),
                    "Seg type not recognized");

  return lengthFuncTable[type](seg + 1, aState);
}

