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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsCOMPtr.h"
#include "nsISVGRendererPathBuilder.h"
#include "nsSVGLibartBPathBuilder.h"
#include <math.h>


/**
 * \addtogroup libart_renderer Libart Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Libart path builder implementation
 */
class nsSVGLibartBPathBuilder : public nsISVGRendererPathBuilder
{
protected:
  friend nsresult NS_NewSVGLibartBPathBuilder(nsISVGRendererPathBuilder **result,
                                              ArtBpath** dest);

  nsSVGLibartBPathBuilder(ArtBpath** dest);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererPathBuilder interface:
  NS_DECL_NSISVGRENDERERPATHBUILDER

private:
  // helpers
  void EnsureBPathSpace(PRUint32 space=1);
  void EnsureBPathTerminated();
  PRInt32 GetLastOpenBPath();

  ArtBpath** mBPath;
  PRUint32  mBPathSize;
  PRUint32  mBPathEnd; // one-past-the-end
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGLibartBPathBuilder::nsSVGLibartBPathBuilder(ArtBpath** dest)
    : mBPath(dest),
      mBPathSize(0),
      mBPathEnd(0)
{
}

nsresult
NS_NewSVGLibartBPathBuilder(nsISVGRendererPathBuilder **result,
                            ArtBpath** dest)
{
  *result = new nsSVGLibartBPathBuilder(dest);
  if (!result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLibartBPathBuilder)
NS_IMPL_RELEASE(nsSVGLibartBPathBuilder)

NS_INTERFACE_MAP_BEGIN(nsSVGLibartBPathBuilder)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererPathBuilder)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererPathBuilder methods:

/** Implements void moveto(in float x, in float y); */
NS_IMETHODIMP
nsSVGLibartBPathBuilder::Moveto(float x, float y)
{
  EnsureBPathSpace();

  (*mBPath)[mBPathEnd].code = ART_MOVETO_OPEN;
  (*mBPath)[mBPathEnd].x3 = x;
  (*mBPath)[mBPathEnd].y3 = y;

  ++mBPathEnd;
  
  return NS_OK;
}

/** Implements void lineto(in float x, in float y); */
NS_IMETHODIMP
nsSVGLibartBPathBuilder::Lineto(float x, float y)
{
  EnsureBPathSpace();

  (*mBPath)[mBPathEnd].code = ART_LINETO;
  (*mBPath)[mBPathEnd].x3 = x;
  (*mBPath)[mBPathEnd].y3 = y;

  ++mBPathEnd;

  return NS_OK;
}

/** Implements void curveto(in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGLibartBPathBuilder::Curveto(float x, float y, float x1, float y1, float x2, float y2)
{
  EnsureBPathSpace();

  (*mBPath)[mBPathEnd].code = ART_CURVETO;
  (*mBPath)[mBPathEnd].x1 = x1;
  (*mBPath)[mBPathEnd].y1 = y1;
  (*mBPath)[mBPathEnd].x2 = x2;
  (*mBPath)[mBPathEnd].y2 = y2;
  (*mBPath)[mBPathEnd].x3 = x;
  (*mBPath)[mBPathEnd].y3 = y;

  ++mBPathEnd;

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
nsSVGLibartBPathBuilder::Arcto(float x2, float y2, float rx, float ry, float angle, PRBool largeArcFlag, PRBool sweepFlag)
{
  const double pi = 3.14159265359;
  const double radPerDeg = pi/180.0;

  float x1=0.0f, y1=0.0f;
  NS_ASSERTION(mBPathEnd > 0, "Arcto needs a start position");
  if (mBPathEnd > 0) {
    x1 = (float)((*mBPath)[mBPathEnd-1].x3);
    y1 = (float)((*mBPath)[mBPathEnd-1].y3);
  }

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
nsSVGLibartBPathBuilder::ClosePath(float *newX, float *newY)
{
  PRInt32 subpath = GetLastOpenBPath();
  NS_ASSERTION(subpath>=0, "no open subpath");
  if (subpath<0) return NS_OK;

  // insert closing line if needed:
  if ((*mBPath)[subpath].x3 != (*mBPath)[mBPathEnd-1].x3 ||
      (*mBPath)[subpath].y3 != (*mBPath)[mBPathEnd-1].y3) {
    Lineto((float)(*mBPath)[subpath].x3, (float)(*mBPath)[subpath].y3);
  }

  (*mBPath)[subpath].code = ART_MOVETO;

  *newX = (float)((*mBPath)[subpath].x3);
  *newY = (float)((*mBPath)[subpath].y3);
  
  return NS_OK;
}

/** Implements void endPath(); */
NS_IMETHODIMP
nsSVGLibartBPathBuilder::EndPath()
{
  EnsureBPathTerminated();
  return NS_OK;
}

//----------------------------------------------------------------------
// helpers

void
nsSVGLibartBPathBuilder::EnsureBPathSpace(PRUint32 space)
{
  const PRInt32 minGrowSize = 10;

  if (mBPathSize - mBPathEnd >= space)
    return;

  if (space < minGrowSize)
    space = minGrowSize;
  
  mBPathSize += space;
  
  if (!*mBPath) {
    *mBPath = art_new(ArtBpath, mBPathSize);
  }
  else {
    *mBPath = art_renew(*mBPath, ArtBpath, mBPathSize);
  }
}

void
nsSVGLibartBPathBuilder::EnsureBPathTerminated()
{
  NS_ASSERTION (*mBPath, "no bpath");  
  NS_ASSERTION (mBPathEnd>0, "trying to terminate empty bpath");
  
  if (mBPathEnd>0 && (*mBPath)[mBPathEnd-1].code == ART_END) return;

  EnsureBPathSpace(1);
  (*mBPath)[mBPathEnd++].code = ART_END;
}

PRInt32
nsSVGLibartBPathBuilder::GetLastOpenBPath()
{
  if (!*mBPath) return -1;
  
  PRInt32 i = mBPathEnd;
  while (--i >= 0) {
    if ((*mBPath)[i].code == ART_MOVETO_OPEN)
      return i;
  }
  return -1;
}

