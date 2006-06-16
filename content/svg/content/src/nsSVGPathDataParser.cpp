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
#include "nsSVGPathSeg.h"
#include "nsSVGPathElement.h"
#include "prdtoa.h"
#include "nsSVGUtils.h"
#include <stdlib.h>
#include <math.h>

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


// ---------------------------------------------------------------
// nsSVGPathDataParserToInternal

nsresult
nsSVGPathDataParserToInternal::Parse(const nsAString &aValue)
{
  mPathData->Clear();
  mPx = mPy = mCx = mCy = mStartX = mStartY = 0;
  mNumCommands = mNumArguments = 0;
  mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;

  nsresult rv = nsSVGPathDataParser::Parse(aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = PathFini();
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsSVGPathDataParserToInternal::StoreMoveTo(PRBool absCoords, float x, float y)
{
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS;
  } else {
    x += mPx;
    y += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL;
  }
  return PathMoveTo(x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreClosePath()
{
  mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH;

  return PathClose();
}

nsresult
nsSVGPathDataParserToInternal::StoreLineTo(PRBool absCoords, float x, float y)
{
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS;
  } else {
    x += mPx;
    y += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_LINETO_REL;
  }
  return PathLineTo(x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreHLineTo(PRBool absCoords, float x)
{
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS;
  } else {
    x += mPx;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL;
  }
  return PathLineTo(x, mPy);
}

nsresult
nsSVGPathDataParserToInternal::StoreVLineTo(PRBool absCoords, float y)
{
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS;
  } else {
    y += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL;
  }
  return PathLineTo(mPx, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreCurveTo(PRBool absCoords,
                                            float x, float y,
                                            float x1, float y1,
                                            float x2, float y2)
{
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS;
  } else {
    x += mPx;  x1 += mPx;  x2 += mPx;
    y += mPy;  y1 += mPy;  y2 += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL;
  }
  mCx = x2;
  mCy = y2;
  return PathCurveTo(x1, y1, x2, y2, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreSmoothCurveTo(PRBool absCoords,
                                                  float x, float y,
                                                  float x2, float y2)
{
  float x1, y1;

  // first controlpoint = reflection last one about current point
  if (mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL        ||
      mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS        ||
      mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL ||
      mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS ) {
    x1 = 2 * mPx - mCx;
    y1 = 2 * mPy - mCy;
  } else {
    x1 = mPx;
    y1 = mPy;
  }
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS;
  } else {
    x += mPx;
    x2 += mPx;
    y += mPy;
    y2 += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL;
  }
  mCx = x2;
  mCy = y2;
  return PathCurveTo(x1, y1, x2, y2, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreQuadCurveTo(PRBool absCoords,
                                                float x, float y,
                                                float x1, float y1)
{
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS;
  } else {
    x += mPx;
    x1 += mPx;
    y += mPy;
    y1 += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL;
  }

  float x31, y31, x32, y32;
  // conversion of quadratic bezier curve to cubic bezier curve:
  x31 = mPx + (x1 - mPx) * 2 / 3;
  y31 = mPy + (y1 - mPy) * 2 / 3;
  x32 = x1 + (x - x1) / 3;
  y32 = y1 + (y - y1) / 3;

  mCx = x1;
  mCy = y1;
  return PathCurveTo(x31, y31, x32, y32, x, y);
}

nsresult
nsSVGPathDataParserToInternal::StoreSmoothQuadCurveTo(PRBool absCoords,
                                                      float x, float y)
{
  float x1, y1;

  // first controlpoint = reflection last one about current point
  if (mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL        ||
      mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS        ||
      mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL ||
      mPrevSeg == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS ) {
    x1 = 2 * mPx - mCx;
    y1 = 2 * mPy - mCy;
  } else {
    x1 = mPx;
    y1 = mPy;
  }
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS;
  } else {
    x += mPx;
    y += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL;
  }

  float x31, y31, x32, y32;
  // conversion of quadratic bezier curve to cubic bezier curve:
  x31 = mPx + (x1 - mPx) * 2 / 3;
  y31 = mPy + (y1 - mPy) * 2 / 3;
  x32 = x1 + (x - x1) / 3;
  y32 = y1 + (y - y1) / 3;

  mCx = x1;
  mCy = y1;
  return PathCurveTo(x31, y31, x32, y32, x, y);
}

static double
CalcVectorAngle(double ux, double uy, double vx, double vy)
{
  double ta = atan2(uy, ux);
  double tb = atan2(vy, vx);
  if (tb >= ta)
    return tb-ta;
  return 2 * M_PI - (ta-tb);
}

nsresult
nsSVGPathDataParserToInternal::ConvertArcToCurves(float x2, float y2,
                                                  float rx, float ry,
                                                  float angle,
                                                  PRBool largeArcFlag,
                                                  PRBool sweepFlag)
{
  const double radPerDeg = M_PI/180.0;

  double x1=mPx, y1=mPy;

  // 1. Treat out-of-range parameters as described in
  // http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
  
  // If the endpoints (x1, y1) and (x2, y2) are identical, then this
  // is equivalent to omitting the elliptical arc segment entirely
  if (x1 == x2 && y1 == y2)
    return NS_OK;

  // If rX = 0 or rY = 0 then this arc is treated as a straight line
  // segment (a "lineto") joining the endpoints.
  if (rx == 0.0f || ry == 0.0f) {
    return PathLineTo(x2, y2);
  }

  // If rX or rY have negative signs, these are dropped; the absolute
  // value is used instead.
  if (rx<0.0) rx = -rx;
  if (ry<0.0) ry = -ry;
  
  // 2. convert to center parameterization as shown in
  // http://www.w3.org/TR/SVG/implnote.html
  double sinPhi = sin(angle*radPerDeg);
  double cosPhi = cos(angle*radPerDeg);
  
  double x1dash =  cosPhi * (x1-x2)/2.0 + sinPhi * (y1-y2)/2.0;
  double y1dash = -sinPhi * (x1-x2)/2.0 + cosPhi * (y1-y2)/2.0;

  double root;
  double numerator = rx*rx*ry*ry - rx*rx*y1dash*y1dash - ry*ry*x1dash*x1dash;

  if (numerator < 0.0) { 
    //  If rX , rY and are such that there is no solution (basically,
    //  the ellipse is not big enough to reach from (x1, y1) to (x2,
    //  y2)) then the ellipse is scaled up uniformly until there is
    //  exactly one solution (until the ellipse is just big enough).

    // -> find factor s, such that numerator' with rx'=s*rx and
    //    ry'=s*ry becomes 0 :
    float s = (float)sqrt(1.0 - numerator/(rx*rx*ry*ry));

    rx *= s;
    ry *= s;
    root = 0.0;

  }
  else {
    root = (largeArcFlag == sweepFlag ? -1.0 : 1.0) *
      sqrt( numerator/(rx*rx*y1dash*y1dash + ry*ry*x1dash*x1dash) );
  }
  
  double cxdash = root*rx*y1dash/ry;
  double cydash = -root*ry*x1dash/rx;
  
  double cx = cosPhi * cxdash - sinPhi * cydash + (x1+x2)/2.0;
  double cy = sinPhi * cxdash + cosPhi * cydash + (y1+y2)/2.0;
  double theta1 = CalcVectorAngle(1.0, 0.0, (x1dash-cxdash)/rx, (y1dash-cydash)/ry);
  double dtheta = CalcVectorAngle((x1dash-cxdash)/rx, (y1dash-cydash)/ry,
                                  (-x1dash-cxdash)/rx, (-y1dash-cydash)/ry);
  if (!sweepFlag && dtheta>0)
    dtheta -= 2.0*M_PI;
  else if (sweepFlag && dtheta<0)
    dtheta += 2.0*M_PI;
  
  // 3. convert into cubic bezier segments <= 90deg
  int segments = (int)ceil(fabs(dtheta/(M_PI/2.0)));
  double delta = dtheta/segments;
  double t = 8.0/3.0 * sin(delta/4.0) * sin(delta/4.0) / sin(delta/2.0);
  
  for (int i = 0; i < segments; ++i) {
    double cosTheta1 = cos(theta1);
    double sinTheta1 = sin(theta1);
    double theta2 = theta1 + delta;
    double cosTheta2 = cos(theta2);
    double sinTheta2 = sin(theta2);
    
    // a) calculate endpoint of the segment:
    double xe = cosPhi * rx*cosTheta2 - sinPhi * ry*sinTheta2 + cx;
    double ye = sinPhi * rx*cosTheta2 + cosPhi * ry*sinTheta2 + cy;

    // b) calculate gradients at start/end points of segment:
    double dx1 = t * ( - cosPhi * rx*sinTheta1 - sinPhi * ry*cosTheta1);
    double dy1 = t * ( - sinPhi * rx*sinTheta1 + cosPhi * ry*cosTheta1);
    
    double dxe = t * ( cosPhi * rx*sinTheta2 + sinPhi * ry*cosTheta2);
    double dye = t * ( sinPhi * rx*sinTheta2 - cosPhi * ry*cosTheta2);

    // c) draw the cubic bezier:
    nsresult rv = PathCurveTo(x1+dx1, y1+dy1, xe+dxe, ye+dye, xe, ye);
    NS_ENSURE_SUCCESS(rv, rv);

    // do next segment
    theta1 = theta2;
    x1 = (float)xe;
    y1 = (float)ye;
  }

  return NS_OK;
}

nsresult
nsSVGPathDataParserToInternal::StoreEllipticalArc(PRBool absCoords,
                                                  float x, float y,
                                                  float r1, float r2,
                                                  float angle,
                                                  PRBool largeArcFlag,
                                                  PRBool sweepFlag)
{
  if (absCoords) {
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_ARC_ABS;
  } else {
    x += mPx;
    y += mPy;
    mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_ARC_REL;
  }
  return ConvertArcToCurves(x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

nsresult
nsSVGPathDataParserToInternal::PathEnsureSpace(PRUint32 aNumArgs)
{
  if (!(mNumCommands % 4) &&
      !mCommands.AppendElement())
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mArguments.SetLength(mArguments.Length()+aNumArgs))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

void
nsSVGPathDataParserToInternal::PathAddCommandCode(PRUint8 aCommand)
{
  PRUint32 offset = mNumCommands / 4;
  PRUint32 shift = 2 * (mNumCommands % 4);
  if (shift == 0) {
    // make sure we set the byte, to avoid false UMR reports
    mCommands[offset] = aCommand;
  } else {
    mCommands[offset] |= aCommand << shift;
  }
  mNumCommands++;
}

nsresult
nsSVGPathDataParserToInternal::PathMoveTo(float x, float y)
{
  nsresult rv = PathEnsureSpace(2);
  NS_ENSURE_SUCCESS(rv, rv);

  PathAddCommandCode(nsSVGPathList::MOVETO);
  mArguments[mNumArguments++] = x;
  mArguments[mNumArguments++] = y;

  mPx = mStartX = x;
  mPy = mStartY = y;

  return NS_OK;
}

nsresult
nsSVGPathDataParserToInternal::PathLineTo(float x, float y)
{
  nsresult rv = PathEnsureSpace(2);
  NS_ENSURE_SUCCESS(rv, rv);

  PathAddCommandCode(nsSVGPathList::LINETO);
  mArguments[mNumArguments++] = x;
  mArguments[mNumArguments++] = y;

  mPx = x;
  mPy = y;

  return NS_OK;
}

nsresult
nsSVGPathDataParserToInternal::PathCurveTo(float x1, float y1,
                                           float x2, float y2,
                                           float x3, float y3)
{
  nsresult rv = PathEnsureSpace(6);
  NS_ENSURE_SUCCESS(rv, rv);

  PathAddCommandCode(nsSVGPathList::CURVETO);
  mArguments[mNumArguments++] = x1;
  mArguments[mNumArguments++] = y1;
  mArguments[mNumArguments++] = x2;
  mArguments[mNumArguments++] = y2;
  mArguments[mNumArguments++] = x3;
  mArguments[mNumArguments++] = y3;

  mPx = x3;
  mPy = y3;

  return NS_OK;
}

nsresult
nsSVGPathDataParserToInternal::PathClose()
{
  nsresult rv = PathEnsureSpace(0);
  NS_ENSURE_SUCCESS(rv, rv);

  PathAddCommandCode(nsSVGPathList::CLOSEPATH);

  mPx = mStartX;
  mPy = mStartY;

  return NS_OK;
}

nsresult
nsSVGPathDataParserToInternal::PathFini()
{
  // We're done adding data to the arrays - copy to a straight array
  // in mPathData, which allows us to remove the 8-byte overhead per
  // nsTArray.  For a bonus savings we allocate a single array instead
  // of two.
  PRUint32 argArraySize;

  argArraySize = mArguments.Length() * sizeof(float);
  mPathData->mArguments = (float *)malloc(argArraySize + mCommands.Length());
  if (!mPathData->mArguments)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy(mPathData->mArguments, mArguments.Elements(), argArraySize);
  memcpy(mPathData->mArguments + mNumArguments,
         mCommands.Elements(),
         mCommands.Length());
  mPathData->mNumArguments = mNumArguments;
  mPathData->mNumCommands = mNumCommands;

  return NS_OK;
}

// ---------------------------------------------------------------
// nsSVGPathDataParserToDOM

nsresult
nsSVGPathDataParserToDOM::AppendSegment(nsIDOMSVGPathSeg* seg)
{
  NS_ADDREF(seg);
  mData->AppendElement((void*)seg);
  return NS_OK;
}


nsresult
nsSVGPathDataParserToDOM::StoreMoveTo(PRBool absCoords, float x, float y)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegMovetoAbs> segAbs;
    rv = NS_NewSVGPathSegMovetoAbs(getter_AddRefs(segAbs), x, y);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegMovetoRel> segRel;
    rv = NS_NewSVGPathSegMovetoRel(getter_AddRefs(segRel), x, y);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreClosePath()
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSegClosePath> seg;
  rv = NS_NewSVGPathSegClosePath(getter_AddRefs(seg));
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreLineTo(PRBool absCoords, float x, float y)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegLinetoAbs> segAbs;
    rv = NS_NewSVGPathSegLinetoAbs(getter_AddRefs(segAbs), x, y);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegLinetoRel> segRel;
    rv = NS_NewSVGPathSegLinetoRel(getter_AddRefs(segRel), x, y);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreHLineTo(PRBool absCoords, float x)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegLinetoHorizontalAbs> segAbs;
    rv = NS_NewSVGPathSegLinetoHorizontalAbs(getter_AddRefs(segAbs), x);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegLinetoHorizontalRel> segRel;
    rv = NS_NewSVGPathSegLinetoHorizontalRel(getter_AddRefs(segRel), x);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreVLineTo(PRBool absCoords, float y)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegLinetoVerticalAbs> segAbs;
    rv = NS_NewSVGPathSegLinetoVerticalAbs(getter_AddRefs(segAbs), y);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegLinetoVerticalRel> segRel;
    rv = NS_NewSVGPathSegLinetoVerticalRel(getter_AddRefs(segRel), y);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreCurveTo(PRBool absCoords,
                                       float x, float y,
                                       float x1, float y1,
                                       float x2, float y2)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicAbs> segAbs;
    rv = NS_NewSVGPathSegCurvetoCubicAbs(getter_AddRefs(segAbs), x, y, x1, y1, x2, y2);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicRel> segRel;
    rv = NS_NewSVGPathSegCurvetoCubicRel(getter_AddRefs(segRel), x, y, x1, y1, x2, y2);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreSmoothCurveTo(PRBool absCoords,
                                             float x, float y,
                                             float x2, float y2)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicSmoothAbs> segAbs;
    rv = NS_NewSVGPathSegCurvetoCubicSmoothAbs(getter_AddRefs(segAbs), x, y, x2, y2);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicSmoothRel> segRel;
    rv = NS_NewSVGPathSegCurvetoCubicSmoothRel(getter_AddRefs(segRel), x, y, x2, y2);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreQuadCurveTo(PRBool absCoords,
                                           float x, float y,
                                           float x1, float y1)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticAbs> segAbs;
    rv = NS_NewSVGPathSegCurvetoQuadraticAbs(getter_AddRefs(segAbs), x, y, x1, y1);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticRel> segRel;
    rv = NS_NewSVGPathSegCurvetoQuadraticRel(getter_AddRefs(segRel), x, y, x1, y1);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreSmoothQuadCurveTo(PRBool absCoords,
                                                 float x, float y)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs> segAbs;
    rv = NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(getter_AddRefs(segAbs), x, y);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticSmoothRel> segRel;
    rv = NS_NewSVGPathSegCurvetoQuadraticSmoothRel(getter_AddRefs(segRel), x, y);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

nsresult
nsSVGPathDataParserToDOM::StoreEllipticalArc(PRBool absCoords,
                                             float x, float y,
                                             float r1, float r2,
                                             float angle,
                                             PRBool largeArcFlag,
                                             PRBool sweepFlag)
{
  nsresult rv;
  nsCOMPtr<nsIDOMSVGPathSeg> seg;
  if (absCoords) {
    nsCOMPtr<nsIDOMSVGPathSegArcAbs> segAbs;
    rv = NS_NewSVGPathSegArcAbs(getter_AddRefs(segAbs), x, y, r1, r2, angle,
                                largeArcFlag, sweepFlag);
    seg = segAbs;
  }
  else {
    nsCOMPtr<nsIDOMSVGPathSegArcRel> segRel;
    rv = NS_NewSVGPathSegArcRel(getter_AddRefs(segRel), x, y, r1, r2, angle,
                                largeArcFlag, sweepFlag);
    seg = segRel;
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return AppendSegment(seg);
}

