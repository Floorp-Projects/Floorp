/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGPathDataParser.h"

#include "mozilla/gfx/Point.h"
#include "nsSVGDataParser.h"
#include "SVGPathData.h"
#include "SVGPathSegUtils.h"
#include <stdlib.h>
#include <math.h>

using namespace mozilla;
using namespace mozilla::gfx;

nsresult nsSVGPathDataParser::Match()
{
  return MatchSvgPath();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchCoordPair(float* aX, float* aY)
{
  ENSURE_MATCHED(MatchCoord(aX));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchCoord(aY));

  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenCoordPairStarter()
{
  return IsTokenCoordStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchCoord(float* aX)
{
  ENSURE_MATCHED(MatchNumber(aX));

  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenCoordStarter()
{
  return IsTokenNumberStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchFlag(bool* f)
{
  switch (mTokenVal) {
    case '0':
      *f = false;
      break;
    case '1':
      *f = true;
      break;
    default:
      return NS_ERROR_FAILURE;
  }
  GetNextToken();
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSvgPath()
{
  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  if (IsTokenSubPathsStarter()) {
    ENSURE_MATCHED(MatchSubPaths());
  }

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSubPaths()
{
  ENSURE_MATCHED(MatchSubPath());

  while (1) {
    const char* pos = mTokenPos;

    while (IsTokenWspStarter()) {
      ENSURE_MATCHED(MatchWsp());
    }

    if (IsTokenSubPathStarter()) {
      ENSURE_MATCHED(MatchSubPath());
    }
    else {
      if (pos != mTokenPos) RewindTo(pos);
      break;
    }
  }
  
  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenSubPathsStarter()
{
  return IsTokenSubPathStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSubPath()
{
  ENSURE_MATCHED(MatchMoveto());

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  if (IsTokenSubPathElementsStarter())
    ENSURE_MATCHED(MatchSubPathElements());
  
  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenSubPathStarter()
{
  return (tolower(mTokenVal) == 'm');
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSubPathElements()
{
  ENSURE_MATCHED(MatchSubPathElement());

  while (1) {
    const char* pos = mTokenPos;

    while (IsTokenWspStarter()) {
      ENSURE_MATCHED(MatchWsp());
    }


    if (IsTokenSubPathElementStarter()) {
        ENSURE_MATCHED(MatchSubPathElement());
    }
    else {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenSubPathElementsStarter()
{
  return IsTokenSubPathElementStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSubPathElement()
{
  switch (tolower(mTokenVal)) {
    case 'z':
      ENSURE_MATCHED(MatchClosePath());
      break;
    case 'l':
      ENSURE_MATCHED(MatchLineto());
      break;      
    case 'h':
      ENSURE_MATCHED(MatchHorizontalLineto());
      break;
    case 'v':
      ENSURE_MATCHED(MatchVerticalLineto());
      break;
    case 'c':
      ENSURE_MATCHED(MatchCurveto());
      break;      
    case 's':
      ENSURE_MATCHED(MatchSmoothCurveto());
      break;
    case 'q':
      ENSURE_MATCHED(MatchQuadBezierCurveto());
      break;
    case 't':
      ENSURE_MATCHED(MatchSmoothQuadBezierCurveto());
      break;
    case 'a':
      ENSURE_MATCHED(MatchEllipticalArc());
      break;
    default:
      return NS_ERROR_FAILURE;
      break;
  }
  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenSubPathElementStarter()
{
  switch (tolower(mTokenVal)) {
    case 'z': case 'l': case 'h': case 'v': case 'c':
    case 's': case 'q': case 't': case 'a':
      return true;
      break;
    default:
      return false;
      break;
  }
  return false;
}  

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchMoveto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'M':
      absCoords = true;
      break;
    case 'm':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchMovetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::MatchMovetoArgSeq(bool absCoords)
{
  
  float x, y;
  ENSURE_MATCHED(MatchCoordPair(&x, &y));

  nsresult rv = StoreMoveTo(absCoords, x, y);
  NS_ENSURE_SUCCESS(rv, rv);
    
  const char* pos = mTokenPos;

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  if (IsTokenLinetoArgSeqStarter()) {
    ENSURE_MATCHED(MatchLinetoArgSeq(absCoords));
  }
  else {
    if (pos != mTokenPos) RewindTo(pos);
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchClosePath()
{
  switch (mTokenVal) {
    case 'Z':
    case 'z':
      GetNextToken();
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  return StoreClosePath();
}

//----------------------------------------------------------------------
  
nsresult nsSVGPathDataParser::MatchLineto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'L':
      absCoords = true;
      break;
    case 'l':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchLinetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::MatchLinetoArgSeq(bool absCoords)
{
  while(1) {
    float x, y;
    ENSURE_MATCHED(MatchCoordPair(&x, &y));
    
    nsresult rv = StoreLineTo(absCoords, x, y);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenCoordPairStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;  
}

bool nsSVGPathDataParser::IsTokenLinetoArgSeqStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchHorizontalLineto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'H':
      absCoords = true;
      break;
    case 'h':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchHorizontalLinetoArgSeq(absCoords));
  
  return NS_OK;
}
  
nsresult nsSVGPathDataParser::MatchHorizontalLinetoArgSeq(bool absCoords)
{
  while(1) {
    float x;
    ENSURE_MATCHED(MatchCoord(&x));
    
    nsresult rv = StoreHLineTo(absCoords, x);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenCoordStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;    
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchVerticalLineto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'V':
      absCoords = true;
      break;
    case 'v':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchVerticalLinetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::MatchVerticalLinetoArgSeq(bool absCoords)
{
  while(1) {
    float y;
    ENSURE_MATCHED(MatchCoord(&y));
    
    nsresult rv = StoreVLineTo(absCoords, y);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenCoordStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;      
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchCurveto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'C':
      absCoords = true;
      break;
    case 'c':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchCurvetoArgSeq(absCoords));
  
  return NS_OK;
}


nsresult nsSVGPathDataParser::MatchCurvetoArgSeq(bool absCoords)
{
  while(1) {
    float x, y, x1, y1, x2, y2;
    ENSURE_MATCHED(MatchCurvetoArg(&x, &y, &x1, &y1, &x2, &y2));

    nsresult rv = StoreCurveTo(absCoords, x, y, x1, y1, x2, y2);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenCurvetoArgStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;      
}

nsresult
nsSVGPathDataParser::MatchCurvetoArg(float* x, float* y, float* x1,
                                     float* y1, float* x2, float* y2)
{
  ENSURE_MATCHED(MatchCoordPair(x1, y1));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchCoordPair(x2, y2));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchCoordPair(x, y));

  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenCurvetoArgStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSmoothCurveto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'S':
      absCoords = true;
      break;
    case 's':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchSmoothCurvetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::MatchSmoothCurvetoArgSeq(bool absCoords)
{
  while(1) {
    float x, y, x2, y2;
    ENSURE_MATCHED(MatchSmoothCurvetoArg(&x, &y, &x2, &y2));
    
    nsresult rv = StoreSmoothCurveTo(absCoords, x, y, x2, y2);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenSmoothCurvetoArgStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

nsresult nsSVGPathDataParser::MatchSmoothCurvetoArg(float* x, float* y, float* x2, float* y2)
{
  ENSURE_MATCHED(MatchCoordPair(x2, y2));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchCoordPair(x, y));

  return NS_OK;
}

bool nsSVGPathDataParser::IsTokenSmoothCurvetoArgStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchQuadBezierCurveto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'Q':
      absCoords = true;
      break;
    case 'q':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchQuadBezierCurvetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::MatchQuadBezierCurvetoArgSeq(bool absCoords)
{
  while(1) {
    float x, y, x1, y1;
    ENSURE_MATCHED(MatchQuadBezierCurvetoArg(&x, &y, &x1, &y1));

    nsresult rv = StoreQuadCurveTo(absCoords, x, y, x1, y1);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenQuadBezierCurvetoArgStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

nsresult nsSVGPathDataParser::MatchQuadBezierCurvetoArg(float* x, float* y, float* x1, float* y1)
{
  ENSURE_MATCHED(MatchCoordPair(x1, y1));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchCoordPair(x, y));

  return NS_OK;  
}

bool nsSVGPathDataParser::IsTokenQuadBezierCurvetoArgStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSmoothQuadBezierCurveto()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'T':
      absCoords = true;
      break;
    case 't':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchSmoothQuadBezierCurvetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::MatchSmoothQuadBezierCurvetoArgSeq(bool absCoords)
{
  while(1) {
    float x, y;
    ENSURE_MATCHED(MatchCoordPair(&x, &y));
   
    nsresult rv = StoreSmoothQuadCurveTo(absCoords, x, y);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenCoordPairStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchEllipticalArc()
{
  bool absCoords;
  
  switch (mTokenVal) {
    case 'A':
      absCoords = true;
      break;
    case 'a':
      absCoords = false;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  GetNextToken();

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }

  ENSURE_MATCHED(MatchEllipticalArcArgSeq(absCoords));
  
  return NS_OK;
}


nsresult nsSVGPathDataParser::MatchEllipticalArcArgSeq(bool absCoords)
{
  while(1) {
    float x, y, r1, r2, angle;
    bool largeArcFlag, sweepFlag;
    
    ENSURE_MATCHED(MatchEllipticalArcArg(&x, &y, &r1, &r2, &angle, &largeArcFlag, &sweepFlag));

    nsresult rv = StoreEllipticalArc(absCoords, x, y, r1, r2, angle,
                                     largeArcFlag, sweepFlag);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = mTokenPos;

    if (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (!IsTokenEllipticalArcArgStarter()) {
      if (pos != mTokenPos) RewindTo(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

nsresult nsSVGPathDataParser::MatchEllipticalArcArg(float* x, float* y,
                                                    float* r1, float* r2, float* angle,
                                                    bool* largeArcFlag, bool* sweepFlag)
{
  ENSURE_MATCHED(MatchNumber(r1));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchNumber(r2));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchNumber(angle));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }
  
  ENSURE_MATCHED(MatchFlag(largeArcFlag));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchFlag(sweepFlag));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchCoordPair(x, y));
  
  return NS_OK;  
  
}

bool nsSVGPathDataParser::IsTokenEllipticalArcArgStarter()
{
  return IsTokenNumberStarter();
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


// ---------------------------------------------------------------
// nsSVGPathDataParserToInternal

nsresult
nsSVGPathDataParserToInternal::Parse(const nsAString &aValue)
{
  mPathSegList->Clear();
  return nsSVGPathDataParser::Parse(aValue);
}

nsresult
nsSVGPathDataParserToInternal::StoreMoveTo(bool absCoords, float x, float y)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_MOVETO_ABS : PATHSEG_MOVETO_REL, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreClosePath()
{
  return mPathSegList->AppendSeg(PATHSEG_CLOSEPATH);
}

nsresult
nsSVGPathDataParserToInternal::StoreLineTo(bool absCoords, float x, float y)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_LINETO_ABS : PATHSEG_LINETO_REL, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreHLineTo(bool absCoords, float x)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_LINETO_HORIZONTAL_ABS : PATHSEG_LINETO_HORIZONTAL_REL, x);
}

nsresult
nsSVGPathDataParserToInternal::StoreVLineTo(bool absCoords, float y)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_LINETO_VERTICAL_ABS : PATHSEG_LINETO_VERTICAL_REL, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreCurveTo(bool absCoords,
                                            float x, float y,
                                            float x1, float y1,
                                            float x2, float y2)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_CURVETO_CUBIC_ABS : PATHSEG_CURVETO_CUBIC_REL,
                                 x1, y1, x2, y2, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreSmoothCurveTo(bool absCoords,
                                                  float x, float y,
                                                  float x2, float y2)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_CURVETO_CUBIC_SMOOTH_ABS : PATHSEG_CURVETO_CUBIC_SMOOTH_REL,
                                 x2, y2, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreQuadCurveTo(bool absCoords,
                                                float x, float y,
                                                float x1, float y1)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_CURVETO_QUADRATIC_ABS : PATHSEG_CURVETO_QUADRATIC_REL,
                                 x1, y1, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreSmoothQuadCurveTo(bool absCoords,
                                                      float x, float y)
{
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS : PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreEllipticalArc(bool absCoords,
                                                  float x, float y,
                                                  float r1, float r2,
                                                  float angle,
                                                  bool largeArcFlag,
                                                  bool sweepFlag)
{
  // We can only pass floats after 'type', and per the SVG spec for arc,
  // non-zero args are treated at 'true'.
  return mPathSegList->AppendSeg(absCoords ? PATHSEG_ARC_ABS : PATHSEG_ARC_REL,
                                 r1, r2, angle,
                                 largeArcFlag ? 1.0f : 0.0f,
                                 sweepFlag ? 1.0f : 0.0f,
                                 x, y);
}

