/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef __NS_SVGPATHDATAPARSER_H__
#define __NS_SVGPATHDATAPARSER_H__

#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIDOMSVGPathSeg.h"

////////////////////////////////////////////////////////////////////////
// nsSVGPathDataParser: a simple recursive descent parser that builds
// nsIDOMPathSegs from path data strings. The grammar for path data
// can be found in SVG CR 20001102, chapter 8.

class nsSVGPathDataParser
{
public:
  nsSVGPathDataParser(nsVoidArray* data);
  nsresult Parse(const char* str);

protected:
  nsVoidArray *mData;
  
  const char* inputpos;
  
  char tokenval;
  const char* tokenpos;
  enum { DIGIT, WSP, COMMA, POINT, SIGN, OTHER, END } tokentype;
  

  // helpers
  nsresult AppendSegment(nsIDOMSVGPathSeg* seg);
  void getNextToken();
  void windBack(const char* pos);
  nsresult match(char tok);


  nsresult matchSvgPath();
  
  nsresult matchSubPaths();
  PRBool isTokenSubPathsStarter();
  
  nsresult matchSubPath();
  PRBool isTokenSubPathStarter();
  
  nsresult matchSubPathElements();
  PRBool isTokenSubPathElementsStarter();

  nsresult matchSubPathElement();
  PRBool isTokenSubPathElementStarter();

  nsresult matchMoveto();
  nsresult matchMovetoArgSeq(PRBool absCoords);
  
  nsresult matchClosePath();
  
  nsresult matchLineto();
  
  nsresult matchLinetoArgSeq(PRBool absCoords);
  PRBool isTokenLinetoArgSeqStarter();
  
  nsresult matchHorizontalLineto();
  nsresult matchHorizontalLinetoArgSeq(PRBool absCoords);
  
  nsresult matchVerticalLineto();
  nsresult matchVerticalLinetoArgSeq(PRBool absCoords);
  
  nsresult matchCurveto();
  nsresult matchCurvetoArgSeq(PRBool absCoords);
  nsresult matchCurvetoArg(float* x, float* y, float* x1,
                           float* y1, float* x2, float* y2);
  PRBool isTokenCurvetoArgStarter();
  
  nsresult matchSmoothCurveto();
  nsresult matchSmoothCurvetoArgSeq(PRBool absCoords);
  nsresult matchSmoothCurvetoArg(float* x, float* y, float* x2, float* y2);
  PRBool isTokenSmoothCurvetoArgStarter();
  
  nsresult matchQuadBezierCurveto();
  nsresult matchQuadBezierCurvetoArgSeq(PRBool absCoords);  
  nsresult matchQuadBezierCurvetoArg(float* x, float* y, float* x1, float* y1);
  PRBool isTokenQuadBezierCurvetoArgStarter();
  
  nsresult matchSmoothQuadBezierCurveto();  
  nsresult matchSmoothQuadBezierCurvetoArgSeq(PRBool absCoords);
  
  nsresult matchEllipticalArc();  
  nsresult matchEllipticalArcArgSeq(PRBool absCoords);
  nsresult matchEllipticalArcArg(float* x, float* y,
                                 float* r1, float* r2, float* angle,
                                 PRBool* largeArcFlag, PRBool* sweepFlag);
  PRBool isTokenEllipticalArcArgStarter();
  
  nsresult matchCoordPair(float* x, float* y);
  PRBool isTokenCoordPairStarter();
  
  nsresult matchCoord(float* x);
  PRBool isTokenCoordStarter();
  
  nsresult matchNonNegativeNumber(float* x);
  PRBool isTokenNonNegativeNumberStarter();
  
  nsresult matchNumber(float* x);
  PRBool isTokenNumberStarter();
  
  nsresult matchFlag(PRBool* f);
  
  nsresult matchCommaWsp();
  PRBool isTokenCommaWspStarter();
  
  nsresult matchIntegerConst();
  
  nsresult matchFloatingPointConst();
  
  nsresult matchFractConst();
  
  nsresult matchExponent();
  PRBool isTokenExponentStarter();
  
  nsresult matchDigitSeq();
  PRBool isTokenDigitSeqStarter();
  
  nsresult matchWsp();
  PRBool isTokenWspStarter();
  
};

#endif // __NS_SVGPATHDATAPARSER_H__
