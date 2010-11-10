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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGPathDataParser.h"
#include "nsSVGDataParser.h"
#include "nsSVGPathElement.h"
#include "prdtoa.h"
#include "nsSVGUtils.h"
#include <stdlib.h>
#include <math.h>

using namespace mozilla;

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

PRBool nsSVGPathDataParser::IsTokenCoordPairStarter()
{
  return IsTokenCoordStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchCoord(float* aX)
{
  ENSURE_MATCHED(MatchNumber(aX));

  return NS_OK;
}

PRBool nsSVGPathDataParser::IsTokenCoordStarter()
{
  return IsTokenNumberStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchFlag(PRBool* f)
{
  switch (mTokenVal) {
    case '0':
      *f = PR_FALSE;
      break;
    case '1':
      *f = PR_TRUE;
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

PRBool nsSVGPathDataParser::IsTokenSubPathsStarter()
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

PRBool nsSVGPathDataParser::IsTokenSubPathStarter()
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

PRBool nsSVGPathDataParser::IsTokenSubPathElementsStarter()
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

PRBool nsSVGPathDataParser::IsTokenSubPathElementStarter()
{
  switch (tolower(mTokenVal)) {
    case 'z': case 'l': case 'h': case 'v': case 'c':
    case 's': case 'q': case 't': case 'a':
      return PR_TRUE;
      break;
    default:
      return PR_FALSE;
      break;
  }
  return PR_FALSE;
}  

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchMoveto()
{
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'M':
      absCoords = PR_TRUE;
      break;
    case 'm':
      absCoords = PR_FALSE;
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

//  typedef nsresult MovetoSegCreationFunc(nsIDOMSVGPathSeg** res, float x, float y);
//  MovetoSegCreationFunc *creationFunc;


nsresult nsSVGPathDataParser::MatchMovetoArgSeq(PRBool absCoords)
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
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'L':
      absCoords = PR_TRUE;
      break;
    case 'l':
      absCoords = PR_FALSE;
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

nsresult nsSVGPathDataParser::MatchLinetoArgSeq(PRBool absCoords)
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

PRBool nsSVGPathDataParser::IsTokenLinetoArgSeqStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchHorizontalLineto()
{
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'H':
      absCoords = PR_TRUE;
      break;
    case 'h':
      absCoords = PR_FALSE;
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
  
nsresult nsSVGPathDataParser::MatchHorizontalLinetoArgSeq(PRBool absCoords)
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
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'V':
      absCoords = PR_TRUE;
      break;
    case 'v':
      absCoords = PR_FALSE;
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

nsresult nsSVGPathDataParser::MatchVerticalLinetoArgSeq(PRBool absCoords)
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
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'C':
      absCoords = PR_TRUE;
      break;
    case 'c':
      absCoords = PR_FALSE;
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


nsresult nsSVGPathDataParser::MatchCurvetoArgSeq(PRBool absCoords)
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

PRBool nsSVGPathDataParser::IsTokenCurvetoArgStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSmoothCurveto()
{
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'S':
      absCoords = PR_TRUE;
      break;
    case 's':
      absCoords = PR_FALSE;
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

nsresult nsSVGPathDataParser::MatchSmoothCurvetoArgSeq(PRBool absCoords)
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

PRBool nsSVGPathDataParser::IsTokenSmoothCurvetoArgStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchQuadBezierCurveto()
{
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'Q':
      absCoords = PR_TRUE;
      break;
    case 'q':
      absCoords = PR_FALSE;
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

nsresult nsSVGPathDataParser::MatchQuadBezierCurvetoArgSeq(PRBool absCoords)
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

PRBool nsSVGPathDataParser::IsTokenQuadBezierCurvetoArgStarter()
{
  return IsTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::MatchSmoothQuadBezierCurveto()
{
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'T':
      absCoords = PR_TRUE;
      break;
    case 't':
      absCoords = PR_FALSE;
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

nsresult nsSVGPathDataParser::MatchSmoothQuadBezierCurvetoArgSeq(PRBool absCoords)
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
  PRBool absCoords;
  
  switch (mTokenVal) {
    case 'A':
      absCoords = PR_TRUE;
      break;
    case 'a':
      absCoords = PR_FALSE;
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


nsresult nsSVGPathDataParser::MatchEllipticalArcArgSeq(PRBool absCoords)
{
  while(1) {
    float x, y, r1, r2, angle;
    PRBool largeArcFlag, sweepFlag;
    
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
                                                    PRBool* largeArcFlag, PRBool* sweepFlag)
{
  ENSURE_MATCHED(MatchNonNegativeNumber(r1));

  if (IsTokenCommaWspStarter()) {
    ENSURE_MATCHED(MatchCommaWsp());
  }

  ENSURE_MATCHED(MatchNonNegativeNumber(r2));

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

PRBool nsSVGPathDataParser::IsTokenEllipticalArcArgStarter()
{
  return IsTokenNonNegativeNumberStarter();
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


nsSVGArcConverter::nsSVGArcConverter(const gfxPoint &from,
                                     const gfxPoint &to,
                                     const gfxPoint &radii,
                                     double angle,
                                     PRBool largeArcFlag,
                                     PRBool sweepFlag)
{
  const double radPerDeg = M_PI/180.0;

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
  mSegIndex = 0;
}

PRBool
nsSVGArcConverter::GetNextSegment(gfxPoint *cp1, gfxPoint *cp2, gfxPoint *to)
{
  if (mSegIndex == mNumSegs) {
    return PR_FALSE;
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

  return PR_TRUE;
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
nsSVGPathDataParserToInternal::StoreMoveTo(PRBool absCoords, float x, float y)
{
  // Because our IDL compiler doesn't know any better, each seg type constant
  // in nsIDOMSVGPathSeg is in a separate enum. This results in "warning:
  // enumeral mismatch in conditional expression" under GCC if two bare
  // nsIDOMSVGPathSeg constants are used as operands of the ?: operator below.
  // In newer versions of GCC we would be able to turn off this warning using:
  //
  //#pragma GCC diagnostic push
  //#pragma GCC diagnostic ignored "-Wenum-compare"
  //...
  //#pragma GCC diagnostic pop
  //
  // Unfortunately we need to support older versions of GCC. Instead, to
  // eliminate this warning noise being sent to the console, we wrap the
  // operands with PRUint32(...).

  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL);

  return mPathSegList->AppendSeg(type, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreClosePath()
{
  return mPathSegList->AppendSeg(nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH);
}

nsresult
nsSVGPathDataParserToInternal::StoreLineTo(PRBool absCoords, float x, float y)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_LINETO_REL);

  return mPathSegList->AppendSeg(type, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreHLineTo(PRBool absCoords, float x)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL);

  return mPathSegList->AppendSeg(type, x);
}

nsresult
nsSVGPathDataParserToInternal::StoreVLineTo(PRBool absCoords, float y)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL);

  return mPathSegList->AppendSeg(type, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreCurveTo(PRBool absCoords,
                                            float x, float y,
                                            float x1, float y1,
                                            float x2, float y2)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL);

  return mPathSegList->AppendSeg(type, x1, y1, x2, y2, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreSmoothCurveTo(PRBool absCoords,
                                                  float x, float y,
                                                  float x2, float y2)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL);

  return mPathSegList->AppendSeg(type, x2, y2, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreQuadCurveTo(PRBool absCoords,
                                                float x, float y,
                                                float x1, float y1)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL);

  return mPathSegList->AppendSeg(type, x1, y1, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreSmoothQuadCurveTo(PRBool absCoords,
                                                      float x, float y)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL);

  return mPathSegList->AppendSeg(type, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreEllipticalArc(PRBool absCoords,
                                                  float x, float y,
                                                  float r1, float r2,
                                                  float angle,
                                                  PRBool largeArcFlag,
                                                  PRBool sweepFlag)
{
  PRUint32 type = absCoords ?
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_ARC_ABS) :
    PRUint32(nsIDOMSVGPathSeg::PATHSEG_ARC_REL);

  // We can only pass floats after 'type', and per the SVG spec for arc,
  // non-zero args are treated at 'true'.
  return mPathSegList->AppendSeg(type, r1, r2, angle,
                                 largeArcFlag ? 1.0f : 0.0f,
                                 sweepFlag ? 1.0f : 0.0f,
                                 x, y);
}

