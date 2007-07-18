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
  float x1=mPx, y1=mPy, x3, y3;
  // Treat out-of-range parameters as described in
  // http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
  
  // If the endpoints (x1, y1) and (x2, y2) are identical, then this
  // is equivalent to omitting the elliptical arc segment entirely
  if (x1 == x2 && y1 == y2) {
    return NS_OK;
  }
  // If rX = 0 or rY = 0 then this arc is treated as a straight line
  // segment (a "lineto") joining the endpoints.
  if (rx == 0.0f || ry == 0.0f) {
    return PathLineTo(x2, y2);
  }
  nsSVGArcConverter converter(x1, y1, x2, y2, rx, ry, angle,
                              largeArcFlag, sweepFlag);
  
  while (converter.GetNextSegment(&x1, &y1, &x2, &y2, &x3, &y3)) {
    // c) draw the cubic bezier:
    nsresult rv = PathCurveTo(x1, y1, x2, y2, x3, y3);
    NS_ENSURE_SUCCESS(rv, rv);
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
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  mData->AppendObject(seg);
  return NS_OK;
}


nsresult
nsSVGPathDataParserToDOM::StoreMoveTo(PRBool absCoords, float x, float y)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegMovetoAbs(x, y)
              : NS_NewSVGPathSegMovetoRel(x, y));
}

nsresult
nsSVGPathDataParserToDOM::StoreClosePath()
{
  return AppendSegment(NS_NewSVGPathSegClosePath());
}

nsresult
nsSVGPathDataParserToDOM::StoreLineTo(PRBool absCoords, float x, float y)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegLinetoAbs(x, y)
              : NS_NewSVGPathSegLinetoRel(x, y));
}

nsresult
nsSVGPathDataParserToDOM::StoreHLineTo(PRBool absCoords, float x)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegLinetoHorizontalAbs(x)
              : NS_NewSVGPathSegLinetoHorizontalRel(x));
}

nsresult
nsSVGPathDataParserToDOM::StoreVLineTo(PRBool absCoords, float y)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegLinetoVerticalAbs(y)
              : NS_NewSVGPathSegLinetoVerticalRel(y));
}

nsresult
nsSVGPathDataParserToDOM::StoreCurveTo(PRBool absCoords,
                                       float x, float y,
                                       float x1, float y1,
                                       float x2, float y2)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2)
              : NS_NewSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2));
}

nsresult
nsSVGPathDataParserToDOM::StoreSmoothCurveTo(PRBool absCoords,
                                             float x, float y,
                                             float x2, float y2)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2)
              : NS_NewSVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2));
}

nsresult
nsSVGPathDataParserToDOM::StoreQuadCurveTo(PRBool absCoords,
                                           float x, float y,
                                           float x1, float y1)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegCurvetoQuadraticAbs(x, y, x1, y1)
              : NS_NewSVGPathSegCurvetoQuadraticRel(x, y, x1, y1));
}

nsresult
nsSVGPathDataParserToDOM::StoreSmoothQuadCurveTo(PRBool absCoords,
                                                 float x, float y)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(x, y)
              : NS_NewSVGPathSegCurvetoQuadraticSmoothRel(x, y));
}

nsresult
nsSVGPathDataParserToDOM::StoreEllipticalArc(PRBool absCoords,
                                             float x, float y,
                                             float r1, float r2,
                                             float angle,
                                             PRBool largeArcFlag,
                                             PRBool sweepFlag)
{
  return AppendSegment(
    absCoords ? NS_NewSVGPathSegArcAbs(x, y, r1, r2, angle, 
                                       largeArcFlag, sweepFlag)
              : NS_NewSVGPathSegArcRel(x, y, r1, r2, angle,
                                       largeArcFlag, sweepFlag));
}

nsSVGArcConverter::nsSVGArcConverter(float x1, float y1,
                                     float x2, float y2,
                                     float rx, float ry,
                                     float angle,
                                     PRBool largeArcFlag,
                                     PRBool sweepFlag)
{
  const double radPerDeg = M_PI/180.0;

  // If rX or rY have negative signs, these are dropped; the absolute
  // value is used instead.
  mRx = fabs(rx);
  mRy = fabs(ry);

  // Convert to center parameterization as shown in
  // http://www.w3.org/TR/SVG/implnote.html
  mSinPhi = sin(angle*radPerDeg);
  mCosPhi = cos(angle*radPerDeg);

  double x1dash =  mCosPhi * (x1-x2)/2.0 + mSinPhi * (y1-y2)/2.0;
  double y1dash = -mSinPhi * (x1-x2)/2.0 + mCosPhi * (y1-y2)/2.0;

  double root;
  double numerator = mRx*mRx*mRy*mRy - mRx*mRx*y1dash*y1dash -
                     mRy*mRy*x1dash*x1dash;

  if (numerator < 0.0) {
    //  If mRx , mRy and are such that there is no solution (basically,
    //  the ellipse is not big enough to reach from (x1, y1) to (x2,
    //  y2)) then the ellipse is scaled up uniformly until there is
    //  exactly one solution (until the ellipse is just big enough).

    // -> find factor s, such that numerator' with mRx'=s*mRx and
    //    mRy'=s*mRy becomes 0 :
    float s = (float)sqrt(1.0 - numerator/(mRx*mRx*mRy*mRy));

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

  mCx = mCosPhi * cxdash - mSinPhi * cydash + (x1+x2)/2.0;
  mCy = mSinPhi * cxdash + mCosPhi * cydash + (y1+y2)/2.0;
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

  mX1 = x1;
  mY1 = y1;
  mSegIndex = 0;
}

PRBool
nsSVGArcConverter::GetNextSegment(float *x1, float *y1,
                                  float *x2, float *y2,
                                  float *x3, float *y3)
{
  if (mSegIndex == mNumSegs) {
     return PR_FALSE;
  }
  
  float cosTheta1 = cos(mTheta);
  float sinTheta1 = sin(mTheta);
  float theta2 = mTheta + mDelta;
  float cosTheta2 = cos(theta2);
  float sinTheta2 = sin(theta2);

  // a) calculate endpoint of the segment:
  *x3 = mCosPhi * mRx*cosTheta2 - mSinPhi * mRy*sinTheta2 + mCx;
  *y3 = mSinPhi * mRx*cosTheta2 + mCosPhi * mRy*sinTheta2 + mCy;

  // b) calculate gradients at start/end points of segment:
  *x1 = mX1 + mT * ( - mCosPhi * mRx*sinTheta1 - mSinPhi * mRy*cosTheta1);
  *y1 = mY1 + mT * ( - mSinPhi * mRx*sinTheta1 + mCosPhi * mRy*cosTheta1);

  *x2 = *x3 + mT * ( mCosPhi * mRx*sinTheta2 + mSinPhi * mRy*cosTheta2);
  *y2 = *y3 + mT * ( mSinPhi * mRx*sinTheta2 - mCosPhi * mRy*cosTheta2);

  // do next segment
  mTheta = theta2;
  mX1 = *x3;
  mY1 = *y3;
  ++mSegIndex;

  return PR_TRUE;
}

