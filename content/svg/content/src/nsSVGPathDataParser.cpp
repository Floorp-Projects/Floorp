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
#include "nsSVGPathSeg.h"
#include "nsSVGPathElement.h"
#include "prdtoa.h"
#include "nsSVGUtils.h"
#include <stdlib.h>
#include <math.h>

//----------------------------------------------------------------------
// helper macros
#define ENSURE_MATCHED(exp) { nsresult rv = exp; if (NS_FAILED(rv)) return rv; }


//----------------------------------------------------------------------
// public interface

nsresult
nsSVGPathDataParser::Parse(const nsAString &aValue)
{
  nsresult rv = NS_OK;

  char *str = ToNewCString(aValue);
  if (!str)
    return NS_ERROR_OUT_OF_MEMORY;

  inputpos = str;
  getNextToken();
  rv = matchSvgPath();
  if (tokentype != END)
    rv = NS_ERROR_FAILURE; // not all tokens were consumed

  nsMemory::Free(str);

  return rv;
}

//----------------------------------------------------------------------
// helpers

void nsSVGPathDataParser::getNextToken()
{
  tokenpos  = inputpos;
  tokenval  = *inputpos;
  
  switch (tokenval) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      tokentype = DIGIT;
      break;
    case '\x20': case '\x9': case '\xd': case '\xa':
      tokentype = WSP;
      break;
    case ',':
      tokentype = COMMA;
      break;
    case '+': case '-':
      tokentype = SIGN;
      break;
    case '.':
      tokentype = POINT;
      break;
    case '\0':
      tokentype = END;
      break;
    default:
      tokentype = OTHER;
  }
  
  if (*inputpos != '\0') {
    ++inputpos;
  }
}

void nsSVGPathDataParser::windBack(const char* pos)
{
  inputpos = pos;
  getNextToken();
}

nsresult nsSVGPathDataParser::match(char tok)
{
  if (tokenval != tok) return NS_ERROR_FAILURE;
  getNextToken();
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchSvgPath()
{
  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  if (isTokenSubPathsStarter()) {
    ENSURE_MATCHED(matchSubPaths());
  }

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchSubPaths()
{
  ENSURE_MATCHED(matchSubPath());

  while (1) {
    const char* pos = tokenpos;

    while (isTokenWspStarter()) {
      ENSURE_MATCHED(matchWsp());
    }

    if (isTokenSubPathStarter()) {
      ENSURE_MATCHED(matchSubPath());
    }
    else {
      if (pos != tokenpos) windBack(pos);
      break;
    }
  }
  
  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenSubPathsStarter()
{
  return isTokenSubPathStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchSubPath()
{
  ENSURE_MATCHED(matchMoveto());

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  if (isTokenSubPathElementsStarter())
    ENSURE_MATCHED(matchSubPathElements());
  
  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenSubPathStarter()
{
  return (tolower(tokenval) == 'm');
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchSubPathElements()
{
  ENSURE_MATCHED(matchSubPathElement());

  while (1) {
    const char* pos = tokenpos;

    while (isTokenWspStarter()) {
      ENSURE_MATCHED(matchWsp());
    }


    if (isTokenSubPathElementStarter()) {
        ENSURE_MATCHED(matchSubPathElement());
    }
    else {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenSubPathElementsStarter()
{
  return isTokenSubPathElementStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchSubPathElement()
{
  switch (tolower(tokenval)) {
    case 'z':
      ENSURE_MATCHED(matchClosePath());
      break;
    case 'l':
      ENSURE_MATCHED(matchLineto());
      break;      
    case 'h':
      ENSURE_MATCHED(matchHorizontalLineto());
      break;
    case 'v':
      ENSURE_MATCHED(matchVerticalLineto());
      break;
    case 'c':
      ENSURE_MATCHED(matchCurveto());
      break;      
    case 's':
      ENSURE_MATCHED(matchSmoothCurveto());
      break;
    case 'q':
      ENSURE_MATCHED(matchQuadBezierCurveto());
      break;
    case 't':
      ENSURE_MATCHED(matchSmoothQuadBezierCurveto());
      break;
    case 'a':
      ENSURE_MATCHED(matchEllipticalArc());
      break;
    default:
      return NS_ERROR_FAILURE;
      break;
  }
  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenSubPathElementStarter()
{
  switch (tolower(tokenval)) {
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

nsresult nsSVGPathDataParser::matchMoveto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'M':
      absCoords = PR_TRUE;
      break;
    case 'm':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchMovetoArgSeq(absCoords));
  
  return NS_OK;
}

//  typedef nsresult MovetoSegCreationFunc(nsIDOMSVGPathSeg** res, float x, float y);
//  MovetoSegCreationFunc *creationFunc;


nsresult nsSVGPathDataParser::matchMovetoArgSeq(PRBool absCoords)
{
  
  float x, y;
  ENSURE_MATCHED(matchCoordPair(&x, &y));

  nsresult rv = StoreMoveTo(absCoords, x, y);
  NS_ENSURE_SUCCESS(rv, rv);
    
  const char* pos = tokenpos;

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  if (isTokenLinetoArgSeqStarter()) {
    ENSURE_MATCHED(matchLinetoArgSeq(absCoords));
  }
  else {
    if (pos != tokenpos) windBack(pos);
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchClosePath()
{
  switch (tokenval) {
    case 'Z':
    case 'z':
      getNextToken();
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  return StoreClosePath();
}

//----------------------------------------------------------------------
  
nsresult nsSVGPathDataParser::matchLineto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'L':
      absCoords = PR_TRUE;
      break;
    case 'l':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchLinetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::matchLinetoArgSeq(PRBool absCoords)
{
  while(1) {
    float x, y;
    ENSURE_MATCHED(matchCoordPair(&x, &y));
    
    nsresult rv = StoreLineTo(absCoords, x, y);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenCoordPairStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;  
}

PRBool nsSVGPathDataParser::isTokenLinetoArgSeqStarter()
{
  return isTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchHorizontalLineto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'H':
      absCoords = PR_TRUE;
      break;
    case 'h':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchHorizontalLinetoArgSeq(absCoords));
  
  return NS_OK;
}
  
nsresult nsSVGPathDataParser::matchHorizontalLinetoArgSeq(PRBool absCoords)
{
  while(1) {
    float x;
    ENSURE_MATCHED(matchCoord(&x));
    
    nsresult rv = StoreHLineTo(absCoords, x);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenCoordStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;    
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchVerticalLineto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'V':
      absCoords = PR_TRUE;
      break;
    case 'v':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchVerticalLinetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::matchVerticalLinetoArgSeq(PRBool absCoords)
{
  while(1) {
    float y;
    ENSURE_MATCHED(matchCoord(&y));
    
    nsresult rv = StoreVLineTo(absCoords, y);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenCoordStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;      
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchCurveto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'C':
      absCoords = PR_TRUE;
      break;
    case 'c':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchCurvetoArgSeq(absCoords));
  
  return NS_OK;
}


nsresult nsSVGPathDataParser::matchCurvetoArgSeq(PRBool absCoords)
{
  while(1) {
    float x, y, x1, y1, x2, y2;
    ENSURE_MATCHED(matchCurvetoArg(&x, &y, &x1, &y1, &x2, &y2));

    nsresult rv = StoreCurveTo(absCoords, x, y, x1, y1, x2, y2);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenCurvetoArgStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;      
}

nsresult
nsSVGPathDataParser::matchCurvetoArg(float* x, float* y, float* x1,
                                     float* y1, float* x2, float* y2)
{
  ENSURE_MATCHED(matchCoordPair(x1, y1));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchCoordPair(x2, y2));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchCoordPair(x, y));

  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenCurvetoArgStarter()
{
  return isTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchSmoothCurveto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'S':
      absCoords = PR_TRUE;
      break;
    case 's':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchSmoothCurvetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::matchSmoothCurvetoArgSeq(PRBool absCoords)
{
  while(1) {
    float x, y, x2, y2;
    ENSURE_MATCHED(matchSmoothCurvetoArg(&x, &y, &x2, &y2));
    
    nsresult rv = StoreSmoothCurveTo(absCoords, x, y, x2, y2);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenSmoothCurvetoArgStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

nsresult nsSVGPathDataParser::matchSmoothCurvetoArg(float* x, float* y, float* x2, float* y2)
{
  ENSURE_MATCHED(matchCoordPair(x2, y2));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchCoordPair(x, y));

  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenSmoothCurvetoArgStarter()
{
  return isTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchQuadBezierCurveto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'Q':
      absCoords = PR_TRUE;
      break;
    case 'q':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchQuadBezierCurvetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::matchQuadBezierCurvetoArgSeq(PRBool absCoords)
{
  while(1) {
    float x, y, x1, y1;
    ENSURE_MATCHED(matchQuadBezierCurvetoArg(&x, &y, &x1, &y1));

    nsresult rv = StoreQuadCurveTo(absCoords, x, y, x1, y1);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenQuadBezierCurvetoArgStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

nsresult nsSVGPathDataParser::matchQuadBezierCurvetoArg(float* x, float* y, float* x1, float* y1)
{
  ENSURE_MATCHED(matchCoordPair(x1, y1));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchCoordPair(x, y));

  return NS_OK;  
}

PRBool nsSVGPathDataParser::isTokenQuadBezierCurvetoArgStarter()
{
  return isTokenCoordPairStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchSmoothQuadBezierCurveto()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'T':
      absCoords = PR_TRUE;
      break;
    case 't':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchSmoothQuadBezierCurvetoArgSeq(absCoords));
  
  return NS_OK;
}

nsresult nsSVGPathDataParser::matchSmoothQuadBezierCurvetoArgSeq(PRBool absCoords)
{
  while(1) {
    float x, y;
    ENSURE_MATCHED(matchCoordPair(&x, &y));
   
    nsresult rv = StoreSmoothQuadCurveTo(absCoords, x, y);
    NS_ENSURE_SUCCESS(rv, rv);

    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenCoordPairStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchEllipticalArc()
{
  PRBool absCoords;
  
  switch (tokenval) {
    case 'A':
      absCoords = PR_TRUE;
      break;
    case 'a':
      absCoords = PR_FALSE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  getNextToken();

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }

  ENSURE_MATCHED(matchEllipticalArcArgSeq(absCoords));
  
  return NS_OK;
}


nsresult nsSVGPathDataParser::matchEllipticalArcArgSeq(PRBool absCoords)
{
  while(1) {
    float x, y, r1, r2, angle;
    PRBool largeArcFlag, sweepFlag;
    
    ENSURE_MATCHED(matchEllipticalArcArg(&x, &y, &r1, &r2, &angle, &largeArcFlag, &sweepFlag));

    nsresult rv = StoreEllipticalArc(absCoords, x, y, r1, r2, angle,
                                     largeArcFlag, sweepFlag);
    NS_ENSURE_SUCCESS(rv, rv);
    
    const char* pos = tokenpos;

    if (isTokenCommaWspStarter()) {
      ENSURE_MATCHED(matchCommaWsp());
    }

    if (!isTokenEllipticalArcArgStarter()) {
      if (pos != tokenpos) windBack(pos);
      return NS_OK;
    }
  }
  
  return NS_OK;        
}

nsresult nsSVGPathDataParser::matchEllipticalArcArg(float* x, float* y,
                                                    float* r1, float* r2, float* angle,
                                                    PRBool* largeArcFlag, PRBool* sweepFlag)
{
  ENSURE_MATCHED(matchNonNegativeNumber(r1));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchNonNegativeNumber(r2));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchNumber(angle));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }
  
  ENSURE_MATCHED(matchFlag(largeArcFlag));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchFlag(sweepFlag));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchCoordPair(x, y));
  
  return NS_OK;  
  
}

PRBool nsSVGPathDataParser::isTokenEllipticalArcArgStarter()
{
  return isTokenNonNegativeNumberStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchCoordPair(float* x, float* y)
{
  ENSURE_MATCHED(matchCoord(x));

  if (isTokenCommaWspStarter()) {
    ENSURE_MATCHED(matchCommaWsp());
  }

  ENSURE_MATCHED(matchCoord(y));

  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenCoordPairStarter()
{
  return isTokenCoordStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchCoord(float* x)
{
  ENSURE_MATCHED(matchNumber(x));

  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenCoordStarter()
{
  return isTokenNumberStarter();
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchNonNegativeNumber(float* x)
{
  // XXX inefficient implementation. We probably hit the windback case
  // often.
  
  const char* pos = tokenpos;

  nsresult rv = matchFloatingPointConst();

  if (NS_FAILED(rv)) {
    windBack(pos);
    ENSURE_MATCHED(matchIntegerConst());
  }

  char* end;
  *x = (float) PR_strtod(pos, &end);
  NS_ASSERTION(end == tokenpos, "number parse error");
               
  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenNonNegativeNumberStarter()
{
  return (tokentype == DIGIT || tokentype == POINT);
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchNumber(float* x)
{
  const char* pos = tokenpos;
  
  if (tokentype == SIGN)
    getNextToken();

  const char* pos2 = tokenpos;

  nsresult rv = matchFloatingPointConst();

  if (NS_FAILED(rv)) {
    windBack(pos);
    ENSURE_MATCHED(matchIntegerConst());
  }

  char* end;
  *x = (float) PR_strtod(pos, &end);
  NS_ASSERTION(end == tokenpos, "number parse error");
               
  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenNumberStarter()
{
  return (tokentype == DIGIT || tokentype == POINT || tokentype == SIGN);
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchFlag(PRBool* f)
{
  switch (tokenval) {
    case '0':
      *f = PR_FALSE;
      break;
    case '1':
      *f = PR_TRUE;
      break;
    default:
      return NS_ERROR_FAILURE;
  }
  getNextToken();
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchCommaWsp()
{
  switch (tokentype) {
    case WSP:
      ENSURE_MATCHED(matchWsp());
      if (tokentype == COMMA)
        getNextToken();
      break;
    case COMMA:
      getNextToken();
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  while (isTokenWspStarter()) {
    ENSURE_MATCHED(matchWsp());
  }
  return NS_OK;
}
  
PRBool nsSVGPathDataParser::isTokenCommaWspStarter()
{
  return (isTokenWspStarter() || tokentype == COMMA);
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchIntegerConst()
{
  ENSURE_MATCHED(matchDigitSeq());
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchFloatingPointConst()
{
  // XXX inefficient implementation. It would be nice if we could make
  // this predictive and wouldn't have to backtrack...
  
  const char* pos = tokenpos;

  nsresult rv = matchFractConst();
  if (NS_SUCCEEDED(rv)) {
    if (isTokenExponentStarter())
      ENSURE_MATCHED(matchExponent());
  }
  else {
    windBack(pos);
    ENSURE_MATCHED(matchDigitSeq());
    ENSURE_MATCHED(matchExponent());    
  }

  return NS_OK;  
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchFractConst()
{
  if (tokentype == POINT) {
    getNextToken();
    ENSURE_MATCHED(matchDigitSeq());
  }
  else {
    ENSURE_MATCHED(matchDigitSeq());
    if (tokentype == POINT) {
      getNextToken();
      if (isTokenDigitSeqStarter()) {
        ENSURE_MATCHED(matchDigitSeq());
      }
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchExponent()
{
  if (!(tolower(tokenval) == 'e')) return NS_ERROR_FAILURE;

  getNextToken();

  if (tokentype == SIGN)
    getNextToken();

  ENSURE_MATCHED(matchDigitSeq());

  return NS_OK;  
}

PRBool nsSVGPathDataParser::isTokenExponentStarter()
{
  return (tolower(tokenval) == 'e');
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchDigitSeq()
{
  if (!(tokentype == DIGIT)) return NS_ERROR_FAILURE;

  do {
    getNextToken();
  } while (tokentype == DIGIT);

  return NS_OK;
}

PRBool nsSVGPathDataParser::isTokenDigitSeqStarter()
{
  return (tokentype == DIGIT);
}

//----------------------------------------------------------------------

nsresult nsSVGPathDataParser::matchWsp()
{
  if (!(tokentype == WSP)) return NS_ERROR_FAILURE;

  do {
    getNextToken();
  } while (tokentype == WSP);

  return NS_OK;  
}

PRBool nsSVGPathDataParser::isTokenWspStarter()
{
  return (tokentype == WSP);
}  


//-----------------------------------------------------------------------


// ---------------------------------------------------------------
// nsSVGPathDataParserToInternal

nsresult
nsSVGPathDataParserToInternal::Parse(const nsAString &aValue)
{
  mPathData->Clear();
  mPx = mPy = mCx = mCy = mStartX = mStartY = 0;
  mNumArguments = mSizeCommandArray = mSizeArgumentArray = 0;
  mPrevSeg = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;

  nsresult rv = nsSVGPathDataParser::Parse(aValue);
  if (NS_FAILED(rv))
    mPathData->Clear();
  
  PathFini();

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
  const PRUint32 expansionRatio = 2;

  if (mPathData->mNumCommands + 1 > mSizeCommandArray) {
    PRUint32 newSize = (mSizeCommandArray + 1) * expansionRatio;
    PRUint8 *tmp;
    tmp = (PRUint8*)realloc(mPathData->mCommands, (newSize + 3) / 4);
    if (!tmp)
      return NS_ERROR_OUT_OF_MEMORY;
    mSizeCommandArray = newSize;
    mPathData->mCommands = tmp;
  }

  if (mNumArguments + aNumArgs > mSizeArgumentArray) {
    PRUint32 newSize = (mSizeArgumentArray + aNumArgs) * expansionRatio;
    float *tmp;
    tmp = (float*)realloc(mPathData->mArguments,
                                    newSize * sizeof(float));
    if (!tmp)
      return NS_ERROR_OUT_OF_MEMORY;
    mSizeArgumentArray = newSize;
    mPathData->mArguments = tmp;
  }

  return NS_OK;
}

void
nsSVGPathDataParserToInternal::PathAddCommandCode(PRUint8 aCommand)
{
  PRUint32 offset = mPathData->mNumCommands / 4;
  PRUint32 shift = 2 * (mPathData->mNumCommands % 4);
  if (shift == 0) {
    // make sure we set the byte, to avoid false UMR reports
    mPathData->mCommands[offset] = aCommand;
  } else {
    mPathData->mCommands[offset] |= aCommand << shift;
  }
  mPathData->mNumCommands++;
}

nsresult
nsSVGPathDataParserToInternal::PathMoveTo(float x, float y)
{
  nsresult rv = PathEnsureSpace(2);
  NS_ENSURE_SUCCESS(rv, rv);

  PathAddCommandCode(nsSVGPathList::MOVETO);
  mPathData->mArguments[mNumArguments++] = x;
  mPathData->mArguments[mNumArguments++] = y;

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
  mPathData->mArguments[mNumArguments++] = x;
  mPathData->mArguments[mNumArguments++] = y;

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
  mPathData->mArguments[mNumArguments++] = x1;
  mPathData->mArguments[mNumArguments++] = y1;
  mPathData->mArguments[mNumArguments++] = x2;
  mPathData->mArguments[mNumArguments++] = y2;
  mPathData->mArguments[mNumArguments++] = x3;
  mPathData->mArguments[mNumArguments++] = y3;

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

void
nsSVGPathDataParserToInternal::PathFini()
{
  if ((mPathData->mNumCommands + 3) / 4 != mSizeCommandArray / 4) {
    PRUint8 *tmp8;
    tmp8 = (PRUint8*)realloc(mPathData->mCommands,
                                       (mPathData->mNumCommands + 3) / 4);
    if (tmp8)
      mPathData->mCommands = tmp8;
  }

  if (mNumArguments != mSizeCommandArray) {
    float *tmp32;
    tmp32 = (float*)realloc(mPathData->mArguments,
                                      mNumArguments * sizeof(float));
    if (tmp32)
      mPathData->mArguments = tmp32;
  }
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
