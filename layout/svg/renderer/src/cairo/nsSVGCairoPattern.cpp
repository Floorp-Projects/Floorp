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
 * The Original Code is the Mozilla SVG Cairo Renderer project.
 *
 * The Initial Developer of the Original Code is Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Parts of this file contain code derived from the following files(s)
 * of the Mozilla SVG project (these parts are Copyright (C) by their
 * respective copyright-holders):
 *    layout/svg/renderer/src/cairo/nsSVGCairoGradient.cpp
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
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
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGPatternElement.h"
#include "nsISVGPathGeometrySource.h"
#include "nsISVGCairoCanvas.h"
#include "nsISVGCairoSurface.h"
#include "nsSVGCairoCanvas.h"
#include "nsSVGCairoSurface.h"
#include "nsSVGCairoPattern.h"

#ifdef DEBUG
#include <stdio.h>
#endif

static cairo_matrix_t SVGToMatrix(nsIDOMSVGMatrix *ctm)
{
  float A, B, C, D, E, F;
  ctm->GetA(&A);
  ctm->GetB(&B);
  ctm->GetC(&C);
  ctm->GetD(&D);
  ctm->GetE(&E);
  ctm->GetF(&F);
  cairo_matrix_t matrix = { A, B, C, D, E, F };
  return matrix;
}

#ifdef DEBUG_scooter
void printMatrix(char *msg, cairo_matrix_t *matrix);
void dumpPattern(nsISVGCairoSurface *surf);
#endif

cairo_pattern_t *
CairoPattern(nsISVGRendererCanvas *canvas, nsISVGPattern *aPat,
             nsISVGGeometrySource *aSource)
{
  NS_ASSERTION(aPat, "Called CairoPattern without a pattern!");
  if (!aPat)
    return NULL;

  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(cairoCanvas, "wrong svg render context for geometry!");
  if (!cairoCanvas) return NULL;

  cairo_t *ctx = cairoCanvas->GetContext();

  cairo_identity_matrix(ctx);

  // Paint it!
  nsCOMPtr<nsISVGRendererSurface> aSurface;
  nsCOMPtr<nsIDOMSVGMatrix> pMatrix;
  if (NS_FAILED(aPat->PaintPattern(canvas, getter_AddRefs(aSurface), getter_AddRefs(pMatrix), aSource)))
    return nsnull;

  // Get the cairo surface
  nsCOMPtr<nsISVGCairoSurface> cairoSurface = do_QueryInterface(aSurface);
  if (!cairoSurface)
    return nsnull;

  cairo_surface_t *pattern_surface = cairoSurface->GetSurface();
  if (!pattern_surface)
    return nsnull;


#ifdef DEBUG_scooter
  // dumpPattern(cairoSurface);
#endif

  // Translate the pattern frame
  cairo_matrix_t pmatrix = SVGToMatrix(pMatrix);
  if (cairo_matrix_invert(&pmatrix))
    return nsnull;

  cairo_pattern_t *surface_pattern =
    cairo_pattern_create_for_surface(pattern_surface);
  if (surface_pattern) {
    cairo_pattern_set_matrix (surface_pattern, &pmatrix);
    cairo_pattern_set_extend (surface_pattern, CAIRO_EXTEND_REPEAT);
  }
  return surface_pattern;
}

#ifdef DEBUG_scooter
PRUint32 picNum = 0;
void
dumpPattern(nsISVGCairoSurface *cairoSurface) {
  PRUint8 *data;
  PRUint32 length;
  PRInt32 stride;
  PRUint32 iwidth, iheight;
  cairoSurface->GetWidth(&iwidth);
  cairoSurface->GetHeight(&iheight);
  cairoSurface->Lock();
  cairoSurface->GetData(&data, &length, &stride);
  printf("dumpPattern -- data lenth: %d, stride: %d\n",length, stride);
  char s[64];
  sprintf(s, "test-%d.ppm", picNum++);
  FILE *f = fopen(s, "wb");
  fprintf (f, "P6\n%d %d\n255\n", iwidth, iheight);
  for (PRUint32 yy=0; yy<iheight; yy++)
    for (PRUint32 xx=0;xx<iwidth;xx++) {
      fputc(data[stride*yy + 4*xx + 2], f);
      fputc(data[stride*yy + 4*xx + 1], f);
      fputc(data[stride*yy + 4*xx + 0], f);
    }
  fclose(f);
  cairoSurface->Unlock();
  return;
}

void
printMatrix(char *msg, cairo_matrix_t *matrix)
{
  printf ("%s: {%f,%f,%f,%f,%f,%f}\n",msg, matrix->xx,matrix->yx,matrix->xy,matrix->yy,matrix->x0,matrix->y0);
}
#endif
