/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGPathDataParser.h"

#include "mozilla/gfx/Point.h"
#include "nsSVGDataParser.h"
#include "SVGContentUtils.h"
#include "SVGPathData.h"
#include "SVGPathSegUtils.h"

using namespace mozilla;
using namespace mozilla::dom::SVGPathSeg_Binding;
using namespace mozilla::gfx;

static inline char16_t ToUpper(char16_t aCh)
{
  return aCh >= 'a' && aCh <= 'z' ? aCh - 'a' + 'A' : aCh;
}

bool
nsSVGPathDataParser::Parse()
{
  mPathSegList->Clear();
  return ParsePath();
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseCoordPair(float& aX, float& aY)
{
  return SVGContentUtils::ParseNumber(mIter, mEnd, aX) &&
         SkipCommaWsp() &&
         SVGContentUtils::ParseNumber(mIter, mEnd, aY);
}

bool
nsSVGPathDataParser::ParseFlag(bool& aFlag)
{
  if (mIter == mEnd || (*mIter != '0' && *mIter != '1')) {
    return false;
  }
  aFlag = (*mIter == '1');

  ++mIter;
  return true;
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParsePath()
{
  while (SkipWsp()) {
    if (!ParseSubPath()) {
      return false;
    }
  }

  return true;
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseSubPath()
{
  return ParseMoveto() && ParseSubPathElements();
}

bool
nsSVGPathDataParser::ParseSubPathElements()
{
  while (SkipWsp() && !IsStartOfSubPath()) {
    char16_t commandType = ToUpper(*mIter);

    // Upper case commands have absolute co-ordinates,
    // lower case commands have relative co-ordinates.
    bool absCoords = commandType == *mIter;

    ++mIter;
    SkipWsp();

    if (!ParseSubPathElement(commandType, absCoords)) {
      return false;
    }
  }
  return true;
}

bool
nsSVGPathDataParser::ParseSubPathElement(char16_t aCommandType,
                                         bool aAbsCoords)
{
  switch (aCommandType) {
    case 'Z':
      return ParseClosePath();
    case 'L':
      return ParseLineto(aAbsCoords);
    case 'H':
      return ParseHorizontalLineto(aAbsCoords);
    case 'V':
      return ParseVerticalLineto(aAbsCoords);
    case 'C':
      return ParseCurveto(aAbsCoords);
    case 'S':
      return ParseSmoothCurveto(aAbsCoords);
    case 'Q':
      return ParseQuadBezierCurveto(aAbsCoords);
    case 'T':
      return ParseSmoothQuadBezierCurveto(aAbsCoords);
    case 'A':
      return ParseEllipticalArc(aAbsCoords);
  }
  return false;
}

bool
nsSVGPathDataParser::IsStartOfSubPath() const
{
  return *mIter == 'm' || *mIter == 'M';
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseMoveto()
{
  if (!IsStartOfSubPath()) {
    return false;
  }

  bool absCoords = (*mIter == 'M');

  ++mIter;
  SkipWsp();

  float x, y;
  if (!ParseCoordPair(x, y)) {
    return false;
  }

  if (NS_FAILED(mPathSegList->AppendSeg(
                  absCoords ? PATHSEG_MOVETO_ABS : PATHSEG_MOVETO_REL,
                  x, y))) {
    return false;
  }

  if (!SkipWsp() || IsAlpha(*mIter)) {
    // End of data, or start of a new command
    return true;
  }

  SkipCommaWsp();

  // Per SVG 1.1 Section 8.3.2
  // If a moveto is followed by multiple pairs of coordinates,
  // the subsequent pairs are treated as implicit lineto commands
  return ParseLineto(absCoords);
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseClosePath()
{
  return NS_SUCCEEDED(mPathSegList->AppendSeg(PATHSEG_CLOSEPATH));
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseLineto(bool aAbsCoords)
{
  while (true) {
    float x, y;
    if (!ParseCoordPair(x, y)) {
      return false;
    }

    if (NS_FAILED(mPathSegList->AppendSeg(
                   aAbsCoords ? PATHSEG_LINETO_ABS : PATHSEG_LINETO_REL,
                   x, y))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // End of data, or start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseHorizontalLineto(bool aAbsCoords)
{
  while (true) {
    float x;
    if (!SVGContentUtils::ParseNumber(mIter, mEnd, x)) {
      return false;
    }

    if (NS_FAILED(mPathSegList->AppendSeg(
                    aAbsCoords ? PATHSEG_LINETO_HORIZONTAL_ABS : PATHSEG_LINETO_HORIZONTAL_REL,
                    x))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // End of data, or start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseVerticalLineto(bool aAbsCoords)
{
  while (true) {
    float y;
    if (!SVGContentUtils::ParseNumber(mIter, mEnd, y)) {
      return false;
    }

    if (NS_FAILED(mPathSegList->AppendSeg(
                    aAbsCoords ? PATHSEG_LINETO_VERTICAL_ABS : PATHSEG_LINETO_VERTICAL_REL,
                    y))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // End of data, or start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseCurveto(bool aAbsCoords)
{
  while (true) {
    float x1, y1, x2, y2, x, y;

    if (!(ParseCoordPair(x1, y1) &&
          SkipCommaWsp() &&
          ParseCoordPair(x2, y2) &&
          SkipCommaWsp() &&
          ParseCoordPair(x, y))) {
      return false;
    }

    if (NS_FAILED(mPathSegList->AppendSeg(
                    aAbsCoords ? PATHSEG_CURVETO_CUBIC_ABS : PATHSEG_CURVETO_CUBIC_REL,
                    x1, y1, x2, y2, x, y))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // End of data, or start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseSmoothCurveto(bool aAbsCoords)
{
  while (true) {
    float x2, y2, x, y;
    if (!(ParseCoordPair(x2, y2) &&
          SkipCommaWsp() &&
          ParseCoordPair(x, y))) {
      return false;
    }

    if (NS_FAILED(mPathSegList->AppendSeg(
                    aAbsCoords ? PATHSEG_CURVETO_CUBIC_SMOOTH_ABS : PATHSEG_CURVETO_CUBIC_SMOOTH_REL,
                    x2, y2, x, y))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // End of data, or start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseQuadBezierCurveto(bool aAbsCoords)
{
  while (true) {
    float x1, y1, x, y;
    if (!(ParseCoordPair(x1, y1) &&
         SkipCommaWsp() &&
         ParseCoordPair(x, y))) {
      return false;
    }

    if (NS_FAILED(mPathSegList->AppendSeg(
                    aAbsCoords ? PATHSEG_CURVETO_QUADRATIC_ABS : PATHSEG_CURVETO_QUADRATIC_REL,
                    x1, y1, x, y))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // Start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseSmoothQuadBezierCurveto(bool aAbsCoords)
{
  while (true) {
    float x, y;
    if (!ParseCoordPair(x, y)) {
      return false;
    }

    if (NS_FAILED(mPathSegList->AppendSeg(
                    aAbsCoords ? PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS : PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL,
                    x, y))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // End of data, or start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//----------------------------------------------------------------------

bool
nsSVGPathDataParser::ParseEllipticalArc(bool aAbsCoords)
{
  while (true) {
    float r1, r2, angle, x, y;
    bool largeArcFlag, sweepFlag;

    if (!(SVGContentUtils::ParseNumber(mIter, mEnd, r1) &&
          SkipCommaWsp() &&
          SVGContentUtils::ParseNumber(mIter, mEnd, r2) &&
          SkipCommaWsp() &&
          SVGContentUtils::ParseNumber(mIter, mEnd, angle)&&
          SkipCommaWsp() &&
          ParseFlag(largeArcFlag) &&
          SkipCommaWsp() &&
          ParseFlag(sweepFlag) &&
          SkipCommaWsp() &&
          ParseCoordPair(x, y))) {
      return false;
    }

    // We can only pass floats after 'type', and per the SVG spec for arc,
    // non-zero args are treated at 'true'.
    if (NS_FAILED(mPathSegList->AppendSeg(
                    aAbsCoords ? PATHSEG_ARC_ABS : PATHSEG_ARC_REL,
                    r1, r2, angle,
                    largeArcFlag ? 1.0f : 0.0f,
                    sweepFlag ? 1.0f : 0.0f,
                    x, y))) {
      return false;
    }

    if (!SkipWsp() || IsAlpha(*mIter)) {
      // End of data, or start of a new command
      return true;
    }
    SkipCommaWsp();
  }
}

//-----------------------------------------------------------------------




static double
CalcVectorAngle(double ux, double uy, double vx, double vy)
{
  double ta = atan2(uy, ux);
  double tb = atan2(vy, vx);
  if (tb >= ta)
    return tb-ta;
  return 2 * M_PI - (ta-tb);
}


nsSVGArcConverter::nsSVGArcConverter(const Point& from,
                                     const Point& to,
                                     const Point& radii,
                                     double angle,
                                     bool largeArcFlag,
                                     bool sweepFlag)
{
  MOZ_ASSERT(radii.x != 0.0f && radii.y != 0.0f, "Bad radii");

  const double radPerDeg = M_PI/180.0;
  mSegIndex = 0;

  if (from == to) {
    mNumSegs = 0;
    return;
  }

  // Convert to center parameterization as shown in
  // http://www.w3.org/TR/SVG/implnote.html
  mRx = fabs(radii.x);
  mRy = fabs(radii.y);

  mSinPhi = sin(angle*radPerDeg);
  mCosPhi = cos(angle*radPerDeg);

  double x1dash =  mCosPhi * (from.x-to.x)/2.0 + mSinPhi * (from.y-to.y)/2.0;
  double y1dash = -mSinPhi * (from.x-to.x)/2.0 + mCosPhi * (from.y-to.y)/2.0;

  double root;
  double numerator = mRx*mRx*mRy*mRy - mRx*mRx*y1dash*y1dash -
                     mRy*mRy*x1dash*x1dash;

  if (numerator < 0.0) {
    //  If mRx , mRy and are such that there is no solution (basically,
    //  the ellipse is not big enough to reach from 'from' to 'to'
    //  then the ellipse is scaled up uniformly until there is
    //  exactly one solution (until the ellipse is just big enough).

    // -> find factor s, such that numerator' with mRx'=s*mRx and
    //    mRy'=s*mRy becomes 0 :
    double s = sqrt(1.0 - numerator/(mRx*mRx*mRy*mRy));

    mRx *= s;
    mRy *= s;
    root = 0.0;

  }
  else {
    root = (largeArcFlag == sweepFlag ? -1.0 : 1.0) *
      sqrt( numerator/(mRx*mRx*y1dash*y1dash + mRy*mRy*x1dash*x1dash) );
  }

  double cxdash = root*mRx*y1dash/mRy;
  double cydash = -root*mRy*x1dash/mRx;

  mC.x = mCosPhi * cxdash - mSinPhi * cydash + (from.x+to.x)/2.0;
  mC.y = mSinPhi * cxdash + mCosPhi * cydash + (from.y+to.y)/2.0;
  mTheta = CalcVectorAngle(1.0, 0.0, (x1dash-cxdash)/mRx, (y1dash-cydash)/mRy);
  double dtheta = CalcVectorAngle((x1dash-cxdash)/mRx, (y1dash-cydash)/mRy,
                                  (-x1dash-cxdash)/mRx, (-y1dash-cydash)/mRy);
  if (!sweepFlag && dtheta>0)
    dtheta -= 2.0*M_PI;
  else if (sweepFlag && dtheta<0)
    dtheta += 2.0*M_PI;

  // Convert into cubic bezier segments <= 90deg
  mNumSegs = static_cast<int>(ceil(fabs(dtheta/(M_PI/2.0))));
  mDelta = dtheta/mNumSegs;
  mT = 8.0/3.0 * sin(mDelta/4.0) * sin(mDelta/4.0) / sin(mDelta/2.0);

  mFrom = from;
}

bool
nsSVGArcConverter::GetNextSegment(Point* cp1, Point* cp2, Point* to)
{
  if (mSegIndex == mNumSegs) {
    return false;
  }

  double cosTheta1 = cos(mTheta);
  double sinTheta1 = sin(mTheta);
  double theta2 = mTheta + mDelta;
  double cosTheta2 = cos(theta2);
  double sinTheta2 = sin(theta2);

  // a) calculate endpoint of the segment:
  to->x = mCosPhi * mRx*cosTheta2 - mSinPhi * mRy*sinTheta2 + mC.x;
  to->y = mSinPhi * mRx*cosTheta2 + mCosPhi * mRy*sinTheta2 + mC.y;

  // b) calculate gradients at start/end points of segment:
  cp1->x = mFrom.x + mT * ( - mCosPhi * mRx*sinTheta1 - mSinPhi * mRy*cosTheta1);
  cp1->y = mFrom.y + mT * ( - mSinPhi * mRx*sinTheta1 + mCosPhi * mRy*cosTheta1);

  cp2->x = to->x + mT * ( mCosPhi * mRx*sinTheta2 + mSinPhi * mRy*cosTheta2);
  cp2->y = to->y + mT * ( mSinPhi * mRx*sinTheta2 - mCosPhi * mRy*cosTheta2);

  // do next segment
  mTheta = theta2;
  mFrom = *to;
  ++mSegIndex;

  return true;
}
