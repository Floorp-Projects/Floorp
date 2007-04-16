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
#include "nsVoidArray.h"
#include "nsCOMArray.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsTArray.h"

class nsSVGPathList;

////////////////////////////////////////////////////////////////////////
// nsSVGPathDataParser: a simple recursive descent parser that builds
// nsIDOMPathSegs from path data strings. The grammar for path data
// can be found in SVG CR 20001102, chapter 8.

class nsSVGPathDataParser : public nsSVGDataParser
{
protected:
  // Path data storage
  virtual nsresult StoreMoveTo(PRBool absCoords, float x, float y) = 0;
  virtual nsresult StoreClosePath() = 0;
  virtual nsresult StoreLineTo(PRBool absCoords, float x, float y) = 0;
  virtual nsresult StoreHLineTo(PRBool absCoords, float x) = 0;
  virtual nsresult StoreVLineTo(PRBool absCoords, float y) = 0;
  virtual nsresult StoreCurveTo(PRBool absCoords, float x, float y,
                                float x1, float y1, float x2, float y2) = 0;
  virtual nsresult StoreSmoothCurveTo(PRBool absCoords, float x, float y,
                                      float x2, float y2) = 0;
  virtual nsresult StoreQuadCurveTo(PRBool absCoords, float x, float y,
                                    float x1, float y1) = 0;
  virtual nsresult StoreSmoothQuadCurveTo(PRBool absCoords,
                                          float x, float y) = 0;
  virtual nsresult StoreEllipticalArc(PRBool absCoords, float x, float y,
                                      float r1, float r2, float angle,
                                      PRBool largeArcFlag, PRBool sweepFlag) = 0;
  nsresult  Match();
 
  nsresult MatchCoordPair(float* aX, float* aY);
  PRBool IsTokenCoordPairStarter();

  nsresult MatchCoord(float* aX);
  PRBool IsTokenCoordStarter();

  nsresult MatchFlag(PRBool* f);

  nsresult MatchSvgPath();
  
  nsresult MatchSubPaths();
  PRBool IsTokenSubPathsStarter();
  
  nsresult MatchSubPath();
  PRBool IsTokenSubPathStarter();
  
  nsresult MatchSubPathElements();
  PRBool IsTokenSubPathElementsStarter();

  nsresult MatchSubPathElement();
  PRBool IsTokenSubPathElementStarter();

  nsresult MatchMoveto();
  nsresult MatchMovetoArgSeq(PRBool absCoords);
  
  nsresult MatchClosePath();
  
  nsresult MatchLineto();
  
  nsresult MatchLinetoArgSeq(PRBool absCoords);
  PRBool IsTokenLinetoArgSeqStarter();
  
  nsresult MatchHorizontalLineto();
  nsresult MatchHorizontalLinetoArgSeq(PRBool absCoords);
  
  nsresult MatchVerticalLineto();
  nsresult MatchVerticalLinetoArgSeq(PRBool absCoords);
  
  nsresult MatchCurveto();
  nsresult MatchCurvetoArgSeq(PRBool absCoords);
  nsresult MatchCurvetoArg(float* x, float* y, float* x1,
                           float* y1, float* x2, float* y2);
  PRBool IsTokenCurvetoArgStarter();
  
  nsresult MatchSmoothCurveto();
  nsresult MatchSmoothCurvetoArgSeq(PRBool absCoords);
  nsresult MatchSmoothCurvetoArg(float* x, float* y, float* x2, float* y2);
  PRBool IsTokenSmoothCurvetoArgStarter();
  
  nsresult MatchQuadBezierCurveto();
  nsresult MatchQuadBezierCurvetoArgSeq(PRBool absCoords);  
  nsresult MatchQuadBezierCurvetoArg(float* x, float* y, float* x1, float* y1);
  PRBool IsTokenQuadBezierCurvetoArgStarter();
  
  nsresult MatchSmoothQuadBezierCurveto();  
  nsresult MatchSmoothQuadBezierCurvetoArgSeq(PRBool absCoords);
  
  nsresult MatchEllipticalArc();  
  nsresult MatchEllipticalArcArgSeq(PRBool absCoords);
  nsresult MatchEllipticalArcArg(float* x, float* y,
                                 float* r1, float* r2, float* angle,
                                 PRBool* largeArcFlag, PRBool* sweepFlag);
  PRBool IsTokenEllipticalArcArgStarter();
  
 };

class nsSVGPathDataParserToInternal : public nsSVGPathDataParser
{
public:
  nsSVGPathDataParserToInternal(nsSVGPathList *data) : mPathData(data) {}
  virtual nsresult Parse(const nsAString &aValue);

protected:
  virtual nsresult StoreMoveTo(PRBool absCoords, float x, float y);
  virtual nsresult StoreClosePath();
  virtual nsresult StoreLineTo(PRBool absCoords, float x, float y);
  virtual nsresult StoreHLineTo(PRBool absCoords, float x);
  virtual nsresult StoreVLineTo(PRBool absCoords, float y);
  virtual nsresult StoreCurveTo(PRBool absCoords, float x, float y,
                                float x1, float y1, float x2, float y2);
  virtual nsresult StoreSmoothCurveTo(PRBool absCoords, float x, float y,
                                      float x2, float y2);
  virtual nsresult StoreQuadCurveTo(PRBool absCoords, float x, float y,
                                    float x1, float y1);
  virtual nsresult StoreSmoothQuadCurveTo(PRBool absCoords,
                                          float x, float y);
  virtual nsresult StoreEllipticalArc(PRBool absCoords, float x, float y,
                                      float r1, float r2, float angle,
                                      PRBool largeArcFlag, PRBool sweepFlag);

private:
  nsSVGPathList *mPathData;
  PRUint16 mPrevSeg;       // previous segment type for "smooth" segments"
  float mPx, mPy;          // current point
  float mCx, mCy;          // last control point for "smooth" segments
  float mStartX, mStartY;  // start of current subpath, for closepath

  // information used to construct PathList 
  nsTArray<PRUint8> mCommands;
  nsTArray<float>   mArguments;
  PRUint32          mNumArguments;
  PRUint32          mNumCommands;

  // Pathdata helpers
  nsresult ConvertArcToCurves(float x2, float y2, float rx, float ry,
                              float angle, PRBool largeArcFlag, PRBool sweepFlag);

  nsresult PathEnsureSpace(PRUint32 aNumArgs);
  void PathAddCommandCode(PRUint8 aCommand);
  nsresult PathMoveTo(float x, float y);
  nsresult PathLineTo(float x, float y);
  nsresult PathCurveTo(float x1, float y1, float x2, float y2, float x3, float y3);
  nsresult PathClose();
  nsresult PathFini();
};

class nsSVGPathDataParserToDOM : public nsSVGPathDataParser
{
public:
  nsSVGPathDataParserToDOM(nsCOMArray<nsIDOMSVGPathSeg>* data) : mData(data) {}

protected:
  virtual nsresult StoreMoveTo(PRBool absCoords, float x, float y);
  virtual nsresult StoreClosePath();
  virtual nsresult StoreLineTo(PRBool absCoords, float x, float y);
  virtual nsresult StoreHLineTo(PRBool absCoords, float x);
  virtual nsresult StoreVLineTo(PRBool absCoords, float y);
  virtual nsresult StoreCurveTo(PRBool absCoords, float x, float y,
                                float x1, float y1, float x2, float y2);
  virtual nsresult StoreSmoothCurveTo(PRBool absCoords, float x, float y,
                                      float x2, float y2);
  virtual nsresult StoreQuadCurveTo(PRBool absCoords, float x, float y,
                                    float x1, float y1);
  virtual nsresult StoreSmoothQuadCurveTo(PRBool absCoords,
                                          float x, float y);
  virtual nsresult StoreEllipticalArc(PRBool absCoords, float x, float y,
                                      float r1, float r2, float angle,
                                      PRBool largeArcFlag, PRBool sweepFlag);

private:
  nsresult AppendSegment(nsIDOMSVGPathSeg* seg);

  nsCOMArray<nsIDOMSVGPathSeg>* mData;
};

class nsSVGArcConverter
{
public:
  nsSVGArcConverter(float x1, float y1,
                    float x2, float y2,
                    float rx, float ry,
                    float angle,
                    PRBool largeArcFlag,
                    PRBool sweepFlag);
  PRBool GetNextSegment(float *x1, float *y1,
                        float *x2, float *y2,
                        float *x3, float *y3);
protected:
  PRInt32 mNumSegs, mSegIndex;
  float mTheta, mDelta, mT;
  float mSinPhi, mCosPhi;
  float mX1, mY1, mRx, mRy, mCx, mCy;

};

#endif // __NS_SVGPATHDATAPARSER_H__
