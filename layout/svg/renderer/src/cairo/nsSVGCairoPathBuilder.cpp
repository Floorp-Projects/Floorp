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
 * The Original Code is the Mozilla SVG Cairo Renderer project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Parts of this file contain code derived from the following files(s)
 * of the Mozilla SVG project (these parts are Copyright (C) by their
 * respective copyright-holders):
 *    layout/svg/renderer/src/libart/nsSVGLibartBPathBuilder.cpp
 *
 * Contributor(s):
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

#include "nsCOMPtr.h"
#include "nsISVGRendererPathBuilder.h"
#include "nsSVGCairoPathBuilder.h"
#include <math.h>
#include <cairo.h>

/**
 * \addtogroup cairo_renderer Cairo Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart path builder implementation
 */
class nsSVGCairoPathBuilder : public nsISVGRendererPathBuilder
{
protected:
  friend nsresult NS_NewSVGCairoPathBuilder(nsISVGRendererPathBuilder **result,
                                            cairo_t *ctx);

  nsSVGCairoPathBuilder(cairo_t *ctx);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererPathBuilder interface:
  NS_DECL_NSISVGRENDERERPATHBUILDER

private:

  cairo_t *mCR;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGCairoPathBuilder::nsSVGCairoPathBuilder(cairo_t *ctx)
  : mCR(ctx)
{
  cairo_new_path(mCR);
}

nsresult
NS_NewSVGCairoPathBuilder(nsISVGRendererPathBuilder **result,
                          cairo_t *ctx)
{
  *result = new nsSVGCairoPathBuilder(ctx);
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
    
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGCairoPathBuilder)
NS_IMPL_RELEASE(nsSVGCairoPathBuilder)

NS_INTERFACE_MAP_BEGIN(nsSVGCairoPathBuilder)
NS_INTERFACE_MAP_ENTRY(nsISVGRendererPathBuilder)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererPathBuilder methods:

/** Implements void moveto(in float x, in float y); */
NS_IMETHODIMP
nsSVGCairoPathBuilder::Moveto(float x, float y)
{
  cairo_move_to(mCR, x, y);
  return NS_OK;
}

/** Implements void lineto(in float x, in float y); */
NS_IMETHODIMP
nsSVGCairoPathBuilder::Lineto(float x, float y)
{
  cairo_line_to(mCR, x, y);
  return NS_OK;
}

/** Implements void curveto(in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGCairoPathBuilder::Curveto(float x, float y, float x1, float y1, float x2, float y2)
{
  cairo_curve_to(mCR, x1, y1, x2, y2, x, y);
  return NS_OK;
}

/* helper for Arcto : */
static inline double CalcVectorAngle(double ux, double uy, double vx, double vy)
{
  double ta = atan2(uy, ux);
  double tb = atan2(vy, vx);
  if (tb >= ta)
    return tb-ta;
  return 6.28318530718 - (ta-tb);
}


/** Implements void arcto(in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
nsSVGCairoPathBuilder::Arcto(float x2, float y2, float rx, float ry, float angle, PRBool largeArcFlag, PRBool sweepFlag)
{
  const double pi = 3.14159265359;
  const double radPerDeg = pi/180.0;

  double x1=0.0, y1=0.0;
  cairo_current_point(mCR, &x1, &y1);

  // 1. Treat out-of-range parameters as described in
  // http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
  
  // If the endpoints (x1, y1) and (x2, y2) are identical, then this
  // is equivalent to omitting the elliptical arc segment entirely
  if (x1 == x2 && y1 == y2) return NS_OK;

  // If rX = 0 or rY = 0 then this arc is treated as a straight line
  // segment (a "lineto") joining the endpoints.
  if (rx == 0.0f || ry == 0.0f) {
    Lineto(x2, y2);
    return NS_OK;
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
      sqrt( numerator/(rx*rx*y1dash*y1dash+ry*ry*x1dash*x1dash) );
  }
  
  double cxdash = root*rx*y1dash/ry;
  double cydash = -root*ry*x1dash/rx;
  
  double cx = cosPhi * cxdash - sinPhi * cydash + (x1+x2)/2.0;
  double cy = sinPhi * cxdash + cosPhi * cydash + (y1+y2)/2.0;
  double theta1 = CalcVectorAngle(1.0, 0.0, (x1dash-cxdash)/rx, (y1dash-cydash)/ry);
  double dtheta = CalcVectorAngle((x1dash-cxdash)/rx, (y1dash-cydash)/ry,
                                  (-x1dash-cxdash)/rx, (-y1dash-cydash)/ry);
  if (!sweepFlag && dtheta>0)
    dtheta -= 2.0*pi;
  else if (sweepFlag && dtheta<0)
    dtheta += 2.0*pi;
  
  // 3. convert into cubic bezier segments <= 90deg
  int segments = (int)ceil(fabs(dtheta/(pi/2.0)));
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
    Curveto((float)xe, (float)ye, (float)(x1+dx1), (float)(y1+dy1),
            (float)(xe+dxe), (float)(ye+dye));

    // do next segment
    theta1 = theta2;
    x1 = (float)xe;
    y1 = (float)ye;
  }

  return NS_OK;
}

/** Implements void closePath(out float newX, out float newY); */
NS_IMETHODIMP
nsSVGCairoPathBuilder::ClosePath(float *newX, float *newY)
{
  cairo_close_path(mCR);
  return NS_OK;
}

/** Implements void endPath(); */
NS_IMETHODIMP
nsSVGCairoPathBuilder::EndPath()
{
  return NS_OK;
}
