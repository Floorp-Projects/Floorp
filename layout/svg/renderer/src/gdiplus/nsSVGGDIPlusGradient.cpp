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
 * The Original Code is the Mozilla SVG GDI+ Renderer project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Parts of this file contain code derived from the following files(s)
 * of the Mozilla SVG project (these parts are Copyright (C) by their
 * respective copyright-holders):
 *    layout/svg/renderer/src/libart/nsSVGCairoGradient.cpp
 *
 * Contributor(s):
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

#include <windows.h>
#include <unknwn.h>
#include <Gdiplus.h>
using namespace Gdiplus;

#include "nsCOMPtr.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGNumber.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMSVGTransformList.h"
#include "nsISVGPathGeometrySource.h"
#include "nsSVGGDIPlusGradient.h"
#include <stdio.h>

static Matrix *
SVGToMatrix(nsIDOMSVGMatrix *ctm)
{
  float A, B, C, D, E, F;
  ctm->GetA(&A);
  ctm->GetB(&B);
  ctm->GetC(&C);
  ctm->GetD(&D);
  ctm->GetE(&E);
  ctm->GetF(&F);
  return new Matrix(A, B, C, D, E, F);
}

static void
GDIPlusGetStops(nsISVGGradient *aGrad, Color **aColors, REAL **aPositions,
                PRUint32 *aStops, PRBool aInvert)
{
  aGrad->GetStopCount(aStops);
  *aColors = new Color[*aStops];
  *aPositions = new REAL[*aStops];
  for (PRUint32 i = 0; i < *aStops; i++) {
    nscolor rgba;
    float offset;
    float opacity;

    aGrad->GetStopOffset(i, &offset);
    aGrad->GetStopColor(i, &rgba);
    aGrad->GetStopOpacity(i, &opacity);

#ifdef DEBUG_tor
    fprintf(stderr, "stop %f %08X opacity %f\n", offset, rgba, opacity);
#endif

    (*aColors)[aInvert ? (*aStops - 1 - i) : i].SetValue(Color::MakeARGB(BYTE(255*opacity),
									 NS_GET_R(rgba),
									 NS_GET_G(rgba),
									 NS_GET_B(rgba)));
    (*aPositions)[i] = (REAL)offset;
  }
}


static void
GDIPlusLinearGradient(nsISVGGradient *aGrad, Matrix *aMatrix, Graphics *aGFX,
                      void(*aCallback)(Graphics *, Brush*, void *), void *aData)
{
  float fX1, fY1, fX2, fY2;
    
  nsCOMPtr<nsISVGLinearGradient>aLgrad = do_QueryInterface(aGrad);
  NS_ASSERTION(aLgrad, "error gradient did not provide a Linear Gradient interface");
  
  aLgrad->GetX1(&fX1);
  aLgrad->GetX2(&fX2);
  aLgrad->GetY1(&fY1);
  aLgrad->GetY2(&fY2);

  LinearGradientBrush gradient(PointF(fX1, fY1), PointF(fX2, fY2),
                               Color(0xff, 0, 0), Color(0, 0xff, 0));

  PRUint16 aSpread;
  aGrad->GetSpreadMethod(&aSpread);
  if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD)
    gradient.SetWrapMode(WrapModeTileFlipX);
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT)
    gradient.SetWrapMode(WrapModeTileFlipX);
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT)
    gradient.SetWrapMode(WrapModeTile);

  PRUint32 nStops;
  Color *stopsCol;
  REAL *stopsPos;
  GDIPlusGetStops(aGrad, &stopsCol, &stopsPos, &nStops, PR_FALSE);
  gradient.SetInterpolationColors(stopsCol, stopsPos, nStops);
  SolidBrush leftBrush(stopsCol[0]);
  SolidBrush rightBrush(stopsCol[nStops-1]);
  delete [] stopsCol;
  delete [] stopsPos;

  gradient.MultiplyTransform(aMatrix, MatrixOrderAppend);

  if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD) {
    float dx = fX2 - fX1;
    float dy = fY2 - fY1;

    // what we really want is a halfspace, but we don't have that...
#define INF 100

    PointF rect[4];
    rect[0].X = fX1 + dy; rect[0].Y = fY1 - dx;
    rect[1].X = fX2 + dy; rect[1].Y = fY2 - dx;
    rect[2].X = fX2 - dy; rect[2].Y = fY2 + dx;
    rect[3].X = fX1 - dy; rect[3].Y = fY1 + dx;
    
    GraphicsPath left;
    left.StartFigure();
    rect[0].X = fX1 + INF * dy;  rect[0].Y = fY1 - INF * dx;
    rect[1].X = fX1 + INF * dx + INF * dy;  rect[1].Y = fY1 + INF * dy - INF * dx;
    rect[2].X = fX1 + INF * dx - INF * dy;  rect[2].Y = fY1 + INF * dy + INF * dx;
    rect[3].X = fX1 - INF * dy;  rect[3].Y = fY1 + INF * dx;
    left.AddPolygon(rect, 4);
    left.Transform(aMatrix);
    
    GraphicsPath center;
    center.StartFigure();
    rect[0].X = fX1 + INF * dy;  rect[0].Y = fY1 - INF * dx;
    rect[1].X = fX2 + INF * dy;  rect[1].Y = fY2 - INF * dx;
    rect[2].X = fX2 - INF * dy;  rect[2].Y = fY2 + INF * dx;
    rect[3].X = fX1 - INF * dy;  rect[3].Y = fY1 + INF * dx;
    center.AddPolygon(rect, 4);
    center.Transform(aMatrix);
    
    GraphicsPath right;
    right.StartFigure();
    rect[0].X = fX2 - INF * dx + INF * dy;  rect[0].Y = fY2 - INF * dy - INF * dx;
    rect[1].X = fX2 + INF * dy;  rect[1].Y = fY2 - INF * dx;
    rect[2].X = fX2 - INF * dy;  rect[2].Y = fY2 + INF * dx;
    rect[3].X = fX2 - INF * dx - INF * dy;  rect[3].Y = fY2 - INF * dy + INF * dx;
    right.AddPolygon(rect, 4);
    right.Transform(aMatrix);
    
    Region leftRegion(&left), centerRegion(&center), rightRegion(&right), oldClip;
    aGFX->GetClip(&oldClip);
    
    aGFX->SetClip(&leftRegion, CombineModeExclude);
    aCallback(aGFX, &leftBrush, aData);
    aGFX->SetClip(&oldClip);
    
    aGFX->SetClip(&centerRegion, CombineModeIntersect);
    aCallback(aGFX, &gradient, aData);
    aGFX->SetClip(&oldClip);
    
    aGFX->SetClip(&rightRegion, CombineModeExclude);
    aCallback(aGFX, &rightBrush, aData);
    aGFX->SetClip(&oldClip);
  } else {
    aCallback(aGFX, &gradient, aData);
  }
}

static void
GDIPlusRadialGradient(nsISVGGradient *aGrad, Matrix *aMatrix, Graphics *aGFX,
                      void(*aCallback)(Graphics *, Brush*, void *), void *aData)
{
  float fCx, fCy, fR, fFx, fFy;

  // Get the Radial Gradient interface
  nsCOMPtr<nsISVGRadialGradient>aRgrad = do_QueryInterface(aGrad);
  NS_ASSERTION(aRgrad, "error gradient did not provide a Linear Gradient interface");

  aRgrad->GetCx(&fCx);
  aRgrad->GetCy(&fCy);
  aRgrad->GetR(&fR);
  aRgrad->GetFx(&fFx);
  aRgrad->GetFy(&fFy);

  GraphicsPath circle;
  circle.StartFigure();
  circle.AddEllipse(fCx - fR, fCy - fR, 2 * fR, 2 * fR);

  PathGradientBrush gradient(&circle);
  gradient.SetCenterPoint(PointF(fFx, fFy));

  PRUint16 aSpread;
  aGrad->GetSpreadMethod(&aSpread);
  if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD) {
      gradient.SetWrapMode(WrapModeClamp);
  }  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT)
      gradient.SetWrapMode(WrapModeTileFlipX);
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT)
      gradient.SetWrapMode(WrapModeTile);

  PRUint32 nStops;
  Color *stopsCol;
  REAL *stopsPos;
  GDIPlusGetStops(aGrad, &stopsCol, &stopsPos, &nStops, PR_TRUE);
  gradient.SetInterpolationColors(stopsCol, stopsPos, nStops);
  SolidBrush rimBrush(stopsCol[0]);
  delete [] stopsCol;
  delete [] stopsPos;

  gradient.MultiplyTransform(aMatrix, MatrixOrderAppend);
  
  if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD) {
    circle.Transform(aMatrix);
    Region exclude(&circle), oldClip;

    aGFX->GetClip(&oldClip);
    aGFX->SetClip(&exclude, CombineModeExclude);
    aCallback(aGFX, &rimBrush, aData);
    aGFX->SetClip(&oldClip);
  }

  aCallback(aGFX, &gradient, aData);
}


void
GDIPlusGradient(nsISVGGDIPlusRegion *aRegion, nsISVGGradient *aGrad,
                nsIDOMSVGMatrix *aCTM,
                Graphics *aGFX,
                void(*aCallback)(Graphics *, Brush*, void *), void *aData)
{
  NS_ASSERTION(aGrad, "Called GDIPlusGradient without a gradient!");
  if (!aGrad)
    return;

  // Get the gradientUnits
  PRUint16 bbox;
  aGrad->GetGradientUnits(&bbox);

  Matrix *patternMatrix;
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    // BoundingBox
    // We need to calculate this from the Region (Uta?) in
    // the object we're filling
    const RectF *rect = aRegion->GetRect();

#ifdef DEBUG_tor
    printf("In GDIPlusGradient: bbox (%f, %f  %f x %f)\n",
           rect->X, rect->Y, rect->Width, rect->Height);
#endif
    patternMatrix = new Matrix(rect->Width, 0, 0, rect->Height, rect->X, rect->Y);
  } else {
      patternMatrix = SVGToMatrix(aCTM);
  }

  // Get the transform list (if there is one)
  nsCOMPtr<nsIDOMSVGMatrix> svgMatrix;
  aGrad->GetGradientTransform(getter_AddRefs(svgMatrix));
  NS_ASSERTION(svgMatrix, "GDIPlusGradient: GetGradientTransform returns null");

  Matrix *aTransMatrix =  SVGToMatrix(svgMatrix);
  patternMatrix->Multiply(aTransMatrix);
  delete aTransMatrix;

  // Linear or Radial?
  PRUint32 type;
  aGrad->GetGradientType(&type);
  if (type == nsISVGGradient::SVG_LINEAR_GRADIENT)
    GDIPlusLinearGradient(aGrad, patternMatrix, aGFX, aCallback, aData);
  else if (type == nsISVGGradient::SVG_RADIAL_GRADIENT)
    GDIPlusRadialGradient(aGrad, patternMatrix, aGFX, aCallback, aData);

  delete patternMatrix;
}
