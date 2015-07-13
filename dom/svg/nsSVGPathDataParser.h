/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPATHDATAPARSER_H__
#define __NS_SVGPATHDATAPARSER_H__

#include "mozilla/Attributes.h"
#include "mozilla/gfx/Point.h"
#include "nsSVGDataParser.h"

namespace mozilla {
class SVGPathData;
} // namespace mozilla

////////////////////////////////////////////////////////////////////////
// nsSVGPathDataParser: a simple recursive descent parser that builds
// DOMSVGPathSegs from path data strings. The grammar for path data
// can be found in SVG CR 20001102, chapter 8.

class nsSVGPathDataParser : public nsSVGDataParser
{
public:
  nsSVGPathDataParser(const nsAString& aValue,
                      mozilla::SVGPathData* aList)
    : nsSVGDataParser(aValue),
      mPathSegList(aList)
  {
    MOZ_ASSERT(aList, "null path data");
  }

  bool Parse();

private:

  bool ParseCoordPair(float& aX, float& aY);
  bool ParseFlag(bool& aFlag);

  bool ParsePath();
  bool IsStartOfSubPath() const;
  bool ParseSubPath();
  
  bool ParseSubPathElements();
  bool ParseSubPathElement(char16_t aCommandType,
                           bool aAbsCoords);

  bool ParseMoveto();
  bool ParseClosePath();
  bool ParseLineto(bool aAbsCoords);
  bool ParseHorizontalLineto(bool aAbsCoords);
  bool ParseVerticalLineto(bool aAbsCoords);
  bool ParseCurveto(bool aAbsCoords);
  bool ParseSmoothCurveto(bool aAbsCoords);
  bool ParseQuadBezierCurveto(bool aAbsCoords);
  bool ParseSmoothQuadBezierCurveto(bool aAbsCoords);  
  bool ParseEllipticalArc(bool aAbsCoords);  

  mozilla::SVGPathData * const mPathSegList;
};

class nsSVGArcConverter
{
  typedef mozilla::gfx::Point Point;

public:
  nsSVGArcConverter(const Point& from,
                    const Point& to,
                    const Point& radii,
                    double angle,
                    bool largeArcFlag,
                    bool sweepFlag);
  bool GetNextSegment(Point* cp1, Point* cp2, Point* to);
protected:
  int32_t mNumSegs, mSegIndex;
  double mTheta, mDelta, mT;
  double mSinPhi, mCosPhi;
  double mRx, mRy;
  Point mFrom, mC;
};

#endif // __NS_SVGPATHDATAPARSER_H__
