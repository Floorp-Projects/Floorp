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

#ifndef __NS_SVGPATHDATAPARSER_H__
#define __NS_SVGPATHDATAPARSER_H__

#include "nsSVGDataParser.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsTArray.h"
#include "gfxPoint.h"

class nsSVGPathList;

namespace mozilla {
class SVGPathData;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathDataParser: a simple recursive descent parser that builds
// nsIDOMSVGPathSegs from path data strings. The grammar for path data
// can be found in SVG CR 20001102, chapter 8.

class nsSVGPathDataParser : public nsSVGDataParser
{
protected:
  // Path data storage
  virtual nsresult StoreMoveTo(bool absCoords, float x, float y) = 0;
  virtual nsresult StoreClosePath() = 0;
  virtual nsresult StoreLineTo(bool absCoords, float x, float y) = 0;
  virtual nsresult StoreHLineTo(bool absCoords, float x) = 0;
  virtual nsresult StoreVLineTo(bool absCoords, float y) = 0;
  virtual nsresult StoreCurveTo(bool absCoords, float x, float y,
                                float x1, float y1, float x2, float y2) = 0;
  virtual nsresult StoreSmoothCurveTo(bool absCoords, float x, float y,
                                      float x2, float y2) = 0;
  virtual nsresult StoreQuadCurveTo(bool absCoords, float x, float y,
                                    float x1, float y1) = 0;
  virtual nsresult StoreSmoothQuadCurveTo(bool absCoords,
                                          float x, float y) = 0;
  virtual nsresult StoreEllipticalArc(bool absCoords, float x, float y,
                                      float r1, float r2, float angle,
                                      bool largeArcFlag, bool sweepFlag) = 0;
  virtual nsresult Match();
 
  nsresult MatchCoordPair(float* aX, float* aY);
  bool IsTokenCoordPairStarter();

  nsresult MatchCoord(float* aX);
  bool IsTokenCoordStarter();

  nsresult MatchFlag(bool* f);

  nsresult MatchSvgPath();
  
  nsresult MatchSubPaths();
  bool IsTokenSubPathsStarter();
  
  nsresult MatchSubPath();
  bool IsTokenSubPathStarter();
  
  nsresult MatchSubPathElements();
  bool IsTokenSubPathElementsStarter();

  nsresult MatchSubPathElement();
  bool IsTokenSubPathElementStarter();

  nsresult MatchMoveto();
  nsresult MatchMovetoArgSeq(bool absCoords);
  
  nsresult MatchClosePath();
  
  nsresult MatchLineto();
  
  nsresult MatchLinetoArgSeq(bool absCoords);
  bool IsTokenLinetoArgSeqStarter();
  
  nsresult MatchHorizontalLineto();
  nsresult MatchHorizontalLinetoArgSeq(bool absCoords);
  
  nsresult MatchVerticalLineto();
  nsresult MatchVerticalLinetoArgSeq(bool absCoords);
  
  nsresult MatchCurveto();
  nsresult MatchCurvetoArgSeq(bool absCoords);
  nsresult MatchCurvetoArg(float* x, float* y, float* x1,
                           float* y1, float* x2, float* y2);
  bool IsTokenCurvetoArgStarter();
  
  nsresult MatchSmoothCurveto();
  nsresult MatchSmoothCurvetoArgSeq(bool absCoords);
  nsresult MatchSmoothCurvetoArg(float* x, float* y, float* x2, float* y2);
  bool IsTokenSmoothCurvetoArgStarter();
  
  nsresult MatchQuadBezierCurveto();
  nsresult MatchQuadBezierCurvetoArgSeq(bool absCoords);  
  nsresult MatchQuadBezierCurvetoArg(float* x, float* y, float* x1, float* y1);
  bool IsTokenQuadBezierCurvetoArgStarter();
  
  nsresult MatchSmoothQuadBezierCurveto();  
  nsresult MatchSmoothQuadBezierCurvetoArgSeq(bool absCoords);
  
  nsresult MatchEllipticalArc();  
  nsresult MatchEllipticalArcArgSeq(bool absCoords);
  nsresult MatchEllipticalArcArg(float* x, float* y,
                                 float* r1, float* r2, float* angle,
                                 bool* largeArcFlag, bool* sweepFlag);
  bool IsTokenEllipticalArcArgStarter();
  
 };

class nsSVGArcConverter
{
public:
  nsSVGArcConverter(const gfxPoint &from,
                    const gfxPoint &to,
                    const gfxPoint &radii,
                    double angle,
                    bool largeArcFlag,
                    bool sweepFlag);
  bool GetNextSegment(gfxPoint *cp1, gfxPoint *cp2, gfxPoint *to);
protected:
  PRInt32 mNumSegs, mSegIndex;
  double mTheta, mDelta, mT;
  double mSinPhi, mCosPhi;
  double mRx, mRy;
  gfxPoint mFrom, mC;
};

class nsSVGPathDataParserToInternal : public nsSVGPathDataParser
{
public:
  nsSVGPathDataParserToInternal(mozilla::SVGPathData *aList)
    : mPathSegList(aList)
  {}
  nsresult Parse(const nsAString &aValue);

protected:
  virtual nsresult StoreMoveTo(bool absCoords, float x, float y);
  virtual nsresult StoreClosePath();
  virtual nsresult StoreLineTo(bool absCoords, float x, float y);
  virtual nsresult StoreHLineTo(bool absCoords, float x);
  virtual nsresult StoreVLineTo(bool absCoords, float y);
  virtual nsresult StoreCurveTo(bool absCoords, float x, float y,
                                float x1, float y1, float x2, float y2);
  virtual nsresult StoreSmoothCurveTo(bool absCoords, float x, float y,
                                      float x2, float y2);
  virtual nsresult StoreQuadCurveTo(bool absCoords, float x, float y,
                                    float x1, float y1);
  virtual nsresult StoreSmoothQuadCurveTo(bool absCoords,
                                          float x, float y);
  virtual nsresult StoreEllipticalArc(bool absCoords, float x, float y,
                                      float r1, float r2, float angle,
                                      bool largeArcFlag, bool sweepFlag);

private:
  mozilla::SVGPathData *mPathSegList;
};

#endif // __NS_SVGPATHDATAPARSER_H__
