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
 *   Scooter Morris <scootermorris@comcast.net> (original author)
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

#include <math.h>

#include "nsCOMPtr.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMSVGTransformList.h"
#include "nsISVGPathGeometrySource.h"
#include "nsISVGLibartRegion.h"
#include "nsSVGLibartGradient.h"
#include "prdtoa.h"
#include "nsString.h"

#define EPSILON 1e-6

static void SVGToMatrix(nsIDOMSVGMatrix *ctm, double matrix[])
{
  float val;
  ctm->GetA(&val);
  matrix[0] = val;
    
  ctm->GetB(&val);
  matrix[1] = val;
  
  ctm->GetC(&val);  
  matrix[2] = val;  
  
  ctm->GetD(&val);  
  matrix[3] = val;  
  
  ctm->GetE(&val);
  matrix[4] = val;

  ctm->GetF(&val);
  matrix[5] = val;
}

// Calculate the bounding box for this gradient
static void GetBounds(nsISVGLibartRegion *aRegion, ArtIRect *rect)
{
  ArtUta *aUta = aRegion->GetUta();
  int nRects = 0;
  ArtIRect *rectList = art_rect_list_from_uta(aUta, 200, 200, &nRects);
  rect->x0 = rectList[0].x0;
  rect->x1 = rectList[0].x1;
  rect->y0 = rectList[0].y0;
  rect->y1 = rectList[0].y1;
  for (int i = 1; i < nRects; i++) 
  {
    if (rectList[i].x0 < rect->x0)
      rect->x0 = rectList[i].x0;
    if (rectList[i].y0 < rect->y0)
      rect->y0 = rectList[i].y0;
    if (rectList[i].x1 > rect->x1)
      rect->x1 = rectList[i].x1;
    if (rectList[i].y1 > rect->y1)
      rect->y1 = rectList[i].y1;
  }

  art_free(rectList);
}

static ArtGradientStop *
GetStops(nsISVGGradient *aGrad) {
  PRUint32 nStops;
  aGrad->GetStopCount(&nStops);
  ArtGradientStop *stops = art_new(ArtGradientStop, nStops);
  for (PRUint32 i = 0; i < nStops; i++)
  {
    // Get the stops into the stop array
    nscolor rgba;
    float offset;

    // ##stops[i].offset = rstops->stop[i].offset;
    aGrad->GetStopOffset(i, &offset);
    stops[i].offset = offset;

    aGrad->GetStopColor(i, &rgba);

    stops[i].color[0] = ART_PIX_MAX_FROM_8(NS_GET_R(rgba));
    stops[i].color[1] = ART_PIX_MAX_FROM_8(NS_GET_G(rgba));
    stops[i].color[2] = ART_PIX_MAX_FROM_8(NS_GET_B(rgba));

    // now get the opacity
    float opacity;
    aGrad->GetStopOpacity(i, &opacity);
    stops[i].color[3] = ART_PIX_MAX_FROM_8((int)(opacity*NS_GET_A(rgba)));
  }
  return stops;
}

static void
LibartLinearGradient(ArtRender *render, nsISVGGradient *aGrad, double *affine)
{
  ArtGradientLinear *agl = nsnull;
  double x1, y1, x2, y2;
  double dx, dy, scale;
  float fX1, fY1, fX2, fY2;
  PRUint32 nStops = 0;

  agl = art_new (ArtGradientLinear, 1);
  aGrad->GetStopCount(&nStops);
  NS_ASSERTION(nStops > 0, "no stops for gradient");
  if (nStops == 0)
    return;
  agl->n_stops = nStops;
  agl->stops = GetStops(aGrad);

#ifdef DEBUG_scooter
  printf("In LibartLinearGradient\n");
  printf("Stop count: %d\n", agl->n_stops);
#endif

  // Get the Linear Gradient interface
  nsCOMPtr<nsISVGLinearGradient>aLgrad = do_QueryInterface(aGrad);
  NS_ASSERTION(aLgrad, "error gradient did not provide a Linear Gradient interface");

  // Get the gradient vector
  aLgrad->GetX1(&fX1);
  aLgrad->GetX2(&fX2);
  aLgrad->GetY1(&fY1);
  aLgrad->GetY2(&fY2);

  // Convert to double
  x1 = fX1; y1 = fY1; x2 = fX2; y2 = fY2;

  // Convert to pixel space 
  float x = x1 * affine[0] + y1 * affine[2] + affine[4];
  float y = x1 * affine[1] + y1 * affine[3] + affine[5];
  x1 = x; y1 = y;
  x = x2 * affine[0] + y2 * affine[2] + affine[4];
  y = x2 * affine[1] + y2 * affine[3] + affine[5];
  x2 = x; y2 = y;

  // solve a, b, c so ax1 + by1 + c = 0 and ax2 + by2 + c = 1, maximum
  // gradient is in x1,y1 to x2,y2 dir
  dx = x2 - x1;
  dy = y2 - y1;

  // Protect against devide by 0
  if (fabs(dx) + fabs(dy) <= EPSILON )
    scale = 0.;
  else
    scale = 1.0 / (dx * dx + dy * dy);

  agl->a = dx * scale;
  agl->b = dy * scale;
  agl->c = -(x1 * agl->a + y1 * agl->b);

  // Get the spread method
  PRUint16 aSpread;
  aGrad->GetSpreadMethod(&aSpread);
  if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD)
    agl->spread = ART_GRADIENT_PAD;
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT)
    agl->spread = ART_GRADIENT_REFLECT;
  else if (aSpread == nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT)
    agl->spread = ART_GRADIENT_REPEAT;

  art_render_gradient_linear (render, agl, ART_FILTER_NEAREST);
  return;
}

static void
LibartRadialGradient(ArtRender *render, nsISVGGradient *aGrad, double *affine)
{
  ArtGradientRadial *agr = nsnull;
  double cx, cy, r, fx, fy;
  double aff1[6], aff2[6];
  float fCx, fCy, fR, fFx, fFy;
  PRUint32 nStops = 0;

#ifdef DEBUG_scooter
  printf("In LibartRadialGradient\n");
#endif

  agr = art_new (ArtGradientRadial, 1);
  aGrad->GetStopCount(&nStops);
  NS_ASSERTION(nStops > 0, "no stops for gradient");
  if (nStops == 0)
    return;
  agr->n_stops = nStops;
  agr->stops = GetStops(aGrad);

  // Get the Radial Gradient interface
  nsCOMPtr<nsISVGRadialGradient>aRgrad = do_QueryInterface(aGrad);
  NS_ASSERTION(aRgrad, "error gradient did not provide a Linear Gradient interface");

  // Get the gradient vector
  aRgrad->GetCx(&fCx);
  aRgrad->GetCy(&fCy);
  aRgrad->GetR(&fR);
  aRgrad->GetFx(&fFx);
  aRgrad->GetFy(&fFy);

  cx = fCx;
  cy = fCy;
  r = fR;
  fx = fFx;
  fy = fFy;

  art_affine_scale (aff1, r, r);
  art_affine_translate (aff2, cx, cy);
  art_affine_multiply (aff1, aff1, aff2);
  art_affine_multiply (aff1, aff1, affine);
  art_affine_invert (agr->affine, aff1);

  // libart doesn't support spreads on radial gradients
  agr->fx = (fx - cx) / r;
  agr->fy = (fy - cy) / r;
  art_render_gradient_radial (render, agr, ART_FILTER_NEAREST);
}

void
LibartGradient(ArtRender *render, nsIDOMSVGMatrix *aMatrix, 
               nsISVGGradient *aGrad, nsISVGLibartRegion *aRegion)
{
  double affine[6];
  NS_ASSERTION(aGrad, "Called LibartGradient without a gradient!");
  if (!aGrad) {
    return;
  }

  // Get the gradientUnits
  PRUint16 bbox;
  aGrad->GetGradientUnits(&bbox);

  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) 
  {
    // BoundingBox
    // We need to calculate this from the Region (Uta?) in
    // the object we're filling
    ArtIRect aBoundsRect;
    GetBounds(aRegion, &aBoundsRect);
#ifdef DEBUG_scooter
    printf("In LibartGradient: bbox (%f, %f, %f, %f)\n", (float)aBoundsRect.x0, 
           (float)aBoundsRect.x1, (float)aBoundsRect.y0, (float)aBoundsRect.y1);
#endif
    affine[0] = aBoundsRect.x1 - aBoundsRect.x0;
    affine[1] = 0.;
    affine[2] = 0.;
    affine[3] = aBoundsRect.y1 - aBoundsRect.y0;
    affine[4] = aBoundsRect.x0;
    affine[5] = aBoundsRect.y0;
  } else {
    // userSpaceOnUse
    // Get the current transformation matrix
    SVGToMatrix(aMatrix, affine);
  }

  // Get the transform list (if there is one)
  nsCOMPtr<nsIDOMSVGMatrix> svgMatrix;
  aGrad->GetGradientTransform(getter_AddRefs(svgMatrix));
  NS_ASSERTION(svgMatrix, "LibartLinearGradient: GetGradientTransform returns null");

  double aTransMatrix[6];
  SVGToMatrix(svgMatrix, aTransMatrix);

  art_affine_multiply(affine, aTransMatrix, affine);

  // Linear or Radial?
  PRUint32 type;
  aGrad->GetGradientType(&type);
  if (type == nsISVGGradient::SVG_LINEAR_GRADIENT)
    LibartLinearGradient(render, aGrad, affine);
  else if (type == nsISVGGradient::SVG_RADIAL_GRADIENT)
    LibartRadialGradient(render, aGrad, affine);

  return;
}


