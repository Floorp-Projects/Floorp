/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGPATHDATAPARSER_H_
#define DOM_SVG_SVGPATHDATAPARSER_H_

#include "mozilla/Attributes.h"
#include "mozilla/gfx/Point.h"
#include "SVGDataParser.h"

namespace mozilla {
class SVGPathData;

////////////////////////////////////////////////////////////////////////
// SVGPathDataParser: a simple recursive descent parser that builds
// DOMSVGPathSegs from path data strings. The grammar for path data
// can be found in SVG CR 20001102, chapter 8.

class SVGPathDataParser : public SVGDataParser {
 public:
  SVGPathDataParser(const nsAString& aValue, mozilla::SVGPathData* aList)
      : SVGDataParser(aValue), mPathSegList(aList) {
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
  bool ParseSubPathElement(char16_t aCommandType, bool aAbsCoords);

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

  mozilla::SVGPathData* const mPathSegList;
};

class SVGArcConverter {
  using Point = mozilla::gfx::Point;

 public:
  SVGArcConverter(const Point& from, const Point& to, const Point& radii,
                  double angle, bool largeArcFlag, bool sweepFlag);
  bool GetNextSegment(Point* cp1, Point* cp2, Point* to);

 protected:
  int32_t mNumSegs, mSegIndex;
  double mTheta, mDelta, mT;
  double mSinPhi, mCosPhi;
  double mRx, mRy;
  Point mFrom, mC;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGPATHDATAPARSER_H_
