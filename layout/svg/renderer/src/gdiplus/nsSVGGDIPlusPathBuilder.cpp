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

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include <Gdiplus.h>
using namespace Gdiplus;

#include "nsCOMPtr.h"
#include "nsISVGPathGeometrySource.h"
#include "nsISVGRendererPathBuilder.h"
#include <math.h>

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * Helper class used by nsSVGGDIPlusPathBuilder
 *
 * Maintains a stack of points during the path building process.
 */
class PointStack
{
public:
  PointStack();
  ~PointStack();

  struct PointData {
    float x;
    float y;
  };
  
  void PushPoint(float x, float y);
  const PointData *PopPoint();
private:  
  void Grow();
  
  PointData mOrigin;
  PointData *mPoints;
  int mCapacity;
  int mSize;
};

/** @} */

PointStack::PointStack()
    : mPoints(0), mCapacity(0), mSize(0)
{
  mOrigin.x = 0.0f;
  mOrigin.y = 0.0f;
}

PointStack::~PointStack()
{
  if (mPoints) delete[] mPoints;
}

void PointStack::PushPoint(float x, float y)
{
  if (mCapacity-mSize<1)
    Grow();
  NS_ASSERTION(mCapacity-mSize>0, "no space in array");
  mPoints[mSize].x = x;
  mPoints[mSize].y = y;
  ++mSize;
}

const PointStack::PointData *PointStack::PopPoint()
{
  if (mSize == 0) return &mOrigin;

  return &mPoints[--mSize];
}

void PointStack::Grow()
{
  // nested subpaths are not very common, so we'll just grow linearly
  PointData *oldPoints = mPoints;
  mPoints = new PointData[++mCapacity];
  if (oldPoints) {
    memcpy(mPoints, oldPoints, sizeof(PointData) * mSize);
    delete[] oldPoints;
  }
}

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 * GDI+ path builder implementation
 */
class nsSVGGDIPlusPathBuilder : public nsISVGRendererPathBuilder
{
protected:
  friend nsresult NS_NewSVGGDIPlusPathBuilder(nsISVGRendererPathBuilder **result,
                                              nsISVGPathGeometrySource *src,
                                              GraphicsPath* dest);

  nsSVGGDIPlusPathBuilder(nsISVGPathGeometrySource *src,
                          GraphicsPath* dest);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererPathBuilder interface:
  NS_DECL_NSISVGRENDERERPATHBUILDER

private:
  nsCOMPtr<nsISVGPathGeometrySource> mSource;
  GraphicsPath *mPath;
  PointStack mSubPathStack;
  PointF mCurrentPoint;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGGDIPlusPathBuilder::nsSVGGDIPlusPathBuilder(nsISVGPathGeometrySource *src,
                                                 GraphicsPath* dest)
    : mSource(src), mPath(dest)
{
  PRUint16 fillrule;
  mSource->GetFillRule(&fillrule);
  mPath->SetFillMode(fillrule==nsISVGGeometrySource::FILL_RULE_NONZERO ?
                     FillModeWinding : FillModeAlternate);
}

nsresult
NS_NewSVGGDIPlusPathBuilder(nsISVGRendererPathBuilder **result,
                            nsISVGPathGeometrySource *src,
                            GraphicsPath* dest)
{
  *result = new nsSVGGDIPlusPathBuilder(src, dest);
  if (!result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGGDIPlusPathBuilder)
NS_IMPL_RELEASE(nsSVGGDIPlusPathBuilder)

NS_INTERFACE_MAP_BEGIN(nsSVGGDIPlusPathBuilder)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererPathBuilder)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRendererPathBuilder methods:

/** Implements void moveto(in float x, in float y); */
NS_IMETHODIMP
nsSVGGDIPlusPathBuilder::Moveto(float x, float y)
{
//  mPath->CloseFigure();
  mPath->StartFigure();
  mCurrentPoint.X = x;
  mCurrentPoint.Y = y;
  mSubPathStack.PushPoint(x, y);
  return NS_OK;
}

/** Implements void lineto(in float x, in float y); */
NS_IMETHODIMP
nsSVGGDIPlusPathBuilder::Lineto(float x, float y)
{
  mPath->AddLine(mCurrentPoint.X, mCurrentPoint.Y, x, y);
  mCurrentPoint.X = x;
  mCurrentPoint.Y = y;
  return NS_OK;
}

/** Implements void curveto(in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGGDIPlusPathBuilder::Curveto(float x, float y, float x1, float y1, float x2, float y2)
{
  mPath->AddBezier(mCurrentPoint.X, mCurrentPoint.Y, // startpoint
                   x1, y1, x2, y2, // controlpoints
                   x, y); // endpoint
  mCurrentPoint.X = x;
  mCurrentPoint.Y = y;
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
nsSVGGDIPlusPathBuilder::Arcto(float x2, float y2, float rx, float ry, float angle, PRBool largeArcFlag, PRBool sweepFlag)
{
  const double pi = 3.14159265359;
  const double radPerDeg = pi/180.0;

  float x1=mCurrentPoint.X, y1=mCurrentPoint.Y;

  // 1. Treat out-of-range parameters as described in
  // http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
  
  // If the endpoints (x1, y1) and (x2, y2) are identical, then this
  // is equivalent to omitting the elliptical arc segment entirely
  if (x1 == x2 && y1 == y2) return NS_OK;

  // If rX = 0 or rY = 0 then this arc is treated as a straight line
  // segment (a "lineto") joining the endpoints.
  if (rx == 0.0f || ry == 0.0f) {
    return Lineto(x2, y2);
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
nsSVGGDIPlusPathBuilder::ClosePath(float *newX, float *newY)
{
  mPath->CloseFigure();
  const PointStack::PointData *point = mSubPathStack.PopPoint();

  *newX = point->x;
  *newY = point->y;
  mCurrentPoint.X = point->x;
  mCurrentPoint.Y = point->y;
  
  return NS_OK;
}

/** Implements void endPath(); */
NS_IMETHODIMP
nsSVGGDIPlusPathBuilder::EndPath()
{
  return NS_OK;
}
