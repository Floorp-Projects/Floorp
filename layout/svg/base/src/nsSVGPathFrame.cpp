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
 *   Daniele Nicolodi <daniele@grinta.net>
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

#include "nsSVGPathGeometryFrame.h"
#include "nsIDOMSVGAnimatedPathData.h"
#include "nsIDOMSVGPathSegList.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsIDOMSVGMatrix.h"
#include "nsISupports.h"
#include "nsLayoutAtoms.h"
#include "nsISVGPathFlatten.h"
#include "nsSVGUtils.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsSVGPathElement.h"
#include "nsSVGMarkerFrame.h"

class nsSVGPathFrame : public nsSVGPathGeometryFrame,
                       public nsISVGPathFlatten
{
protected:
  friend nsIFrame*
  NS_NewSVGPathFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

public:
  nsSVGPathFrame(nsStyleContext* aContext) : nsSVGPathGeometryFrame(aContext) {}

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(cairo_t *aCtx);

  // nsSVGPathGeometry methods
  virtual PRBool IsMarkable() { return PR_TRUE; }
  virtual void GetMarkPoints(nsVoidArray *aMarks);

  // nsISVGPathFlatten interface
  NS_IMETHOD GetFlattenedPath(nsSVGPathData **data, nsIFrame *parent);

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgPathFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPath"), aResult);
  }
#endif

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
};

NS_INTERFACE_MAP_BEGIN(nsSVGPathFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGPathFlatten)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPathGeometryFrame)

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGPathFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGAnimatedPathData> anim_data = do_QueryInterface(aContent);
  if (!anim_data) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGPathFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGPathFrame(aContext);
}

//----------------------------------------------------------------------
// nsISVGFrame methods:

NS_IMETHODIMP
nsSVGPathFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::d) {
    UpdateGraphic();
    return NS_OK;
  }

  return nsSVGPathGeometryFrame::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

static double
CalcVectorAngle(double ux, double uy, double vx, double vy)
{
  double ta = atan2(uy, ux);
  double tb = atan2(vy, vx);
  if (tb >= ta)
    return tb-ta;
  return 2 * M_PI - (ta-tb);
}

NS_IMETHODIMP nsSVGPathFrame::ConstructPath(cairo_t *aCtx)
{
  nsSVGPathElement *element = NS_STATIC_CAST(nsSVGPathElement*, mContent);
  element->mPathData.Playback(aCtx);
  
  return NS_OK;  
}

//----------------------------------------------------------------------
// nsSVGPathGeometry methods:

void
nsSVGPathFrame::GetMarkPoints(nsVoidArray *aMarks) {
  nsCOMPtr<nsIDOMSVGPathSegList> segmentList;

  nsCOMPtr<nsIDOMSVGAnimatedPathData> anim_data = do_QueryInterface(mContent);
  NS_ASSERTION(anim_data,"wrong content element");
  anim_data->GetAnimatedPathSegList(getter_AddRefs(segmentList));
  NS_ASSERTION(segmentList, "no pathseglist");
  if (!segmentList) return;

  PRUint32 count;
  segmentList->GetNumberOfItems(&count);
  nsCOMPtr<nsIDOMSVGPathSeg> segment;
  
  float cx = 0.0f; // current point
  float cy = 0.0f;

  float cx1 = 0.0f; // last controlpoint (for s,S,t,T)
  float cy1 = 0.0f;

  PRUint16 lastSegmentType = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;

  float px, py;    // subpath initial point
  float pathAngle;
  PRUint32 pathIndex;

  float prevAngle = 0, startAngle, endAngle;
  
  PRBool newSegment = PR_FALSE;

  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMSVGPathSeg> segment;
    segmentList->GetItem(i, getter_AddRefs(segment));

    PRUint16 type = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;
    segment->GetPathSegType(&type);

    float x, y;
    PRBool absCoords = PR_FALSE;
    
    switch (type) {
    case nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH:
    {
      x = px;
      y = py;
      startAngle = endAngle = atan2(y - cy, x - cx);
    }
    break;
      
    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL:
    {
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegMovetoRel> moveseg = do_QueryInterface(segment);
        NS_ASSERTION(moveseg, "interface not implemented");
        moveseg->GetX(&x);
        moveseg->GetY(&y);
        x += cx;
        y += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegMovetoAbs> moveseg = do_QueryInterface(segment);
        NS_ASSERTION(moveseg, "interface not implemented");
        moveseg->GetX(&x);
        moveseg->GetY(&y);
      }
      px = x;
      py = y;
      startAngle = endAngle = prevAngle;
      newSegment = PR_TRUE;
    }
    break;
    
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_REL:
    {
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegLinetoRel> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
        lineseg->GetY(&y);
        x += cx;
        y += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegLinetoAbs> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
        lineseg->GetY(&y);
      }
      startAngle = endAngle = atan2(y - cy, x - cx);
    }
    break;        
    
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
    {
      float x1, y1, x2, y2;
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicRel> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
        x  += cx;
        y  += cy;
        x1 += cx;
        y1 += cy;
        x2 += cx;
        y2 += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicAbs> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
      }

      cx1 = x2;
      cy1 = y2;

      if (x1 == cx && y1 == cy) {
        x1 = x2;
        y1 = y2;
      }

      if (x2 == x && y2 == y) {
        x2 = x1;
        y2 = y1;
      }

      startAngle = atan2(y1 - cy, x1 - cx);
      endAngle = atan2(y - y2, x - x2);
    }
    break;
    
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
    {
      float x1, y1;

      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticRel> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
        x  += cx;
        y  += cy;
        x1 += cx;
        y1 += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticAbs> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
      }    

      cx1 = x1;
      cy1 = y1;

      startAngle = atan2(y1 - cy, x1 - cx);
      endAngle = atan2(y - y1, x - x1);
    }
    break;
    
    case nsIDOMSVGPathSeg::PATHSEG_ARC_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_ARC_REL:
    {
      float r1, r2, angle;
      PRBool largeArcFlag, sweepFlag;

      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegArcRel> arcseg = do_QueryInterface(segment);
        NS_ASSERTION(arcseg, "interface not implemented");
        arcseg->GetX(&x);
        arcseg->GetY(&y);
        arcseg->GetR1(&r1);
        arcseg->GetR2(&r2);
        arcseg->GetAngle(&angle);
        arcseg->GetLargeArcFlag(&largeArcFlag);
        arcseg->GetSweepFlag(&sweepFlag);
        
        x  += cx;
        y  += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegArcAbs> arcseg = do_QueryInterface(segment);
        NS_ASSERTION(arcseg, "interface not implemented");
        arcseg->GetX(&x);
        arcseg->GetY(&y);
        arcseg->GetR1(&r1);
        arcseg->GetR2(&r2);
        arcseg->GetAngle(&angle);
        arcseg->GetLargeArcFlag(&largeArcFlag);
        arcseg->GetSweepFlag(&sweepFlag);
      }

      /* check for degenerate ellipse */
      if (r1 == 0.0 || r2 == 0.0) {
        startAngle = endAngle = atan2(y - cy, x - cx);
        break;
      }

      r1 = fabs(r1);  r2 = fabs(r2);

      float xp, yp, cxp, cyp;

      /* slope fun&games ... see SVG spec, section F.6 */
      angle = angle*M_PI/180.0;
      xp = cos(angle)*(cx-x)/2.0 + sin(angle)*(cy-y)/2.0;
      yp = -sin(angle)*(cx-x)/2.0 + cos(angle)*(cy-y)/2.0;

      /* make sure radii are large enough */
      float root, numerator = r1*r1*r2*r2 - r1*r1*yp*yp - r2*r2*xp*xp;
      if (numerator < 0.0) {
        float s = sqrt(1.0 - numerator/(r1*r1*r2*r2));
        r1 *= s;
        r2 *= s;
        root = 0.0;
      } else {
        root = sqrt(numerator/(r1*r1*yp*yp + r2*r2*xp*xp));
        if (largeArcFlag == sweepFlag)
          root = -root;
      }
      cxp = root*r1*yp/r2;
      cyp = -root*r2*xp/r1;

      float theta, delta;
      theta = CalcVectorAngle(1.0, 0.0,  (xp-cxp)/r1, (yp-cyp)/r2);
      delta  = CalcVectorAngle((xp-cxp)/r1, (yp-cyp)/r2,
                               (-xp-cxp)/r1, (-yp-cyp)/r2);
      if (!sweepFlag && delta > 0)
        delta -= 2.0*M_PI;
      else if (sweepFlag && delta < 0)
        delta += 2.0*M_PI;

      float tx1, ty1, tx2, ty2;
      tx1 = -cos(angle)*r1*sin(theta) - sin(angle)*r2*cos(theta);
      ty1 = -sin(angle)*r1*sin(theta) + cos(angle)*r2*cos(theta);
      tx2 = -cos(angle)*r1*sin(theta+delta) - sin(angle)*r2*cos(theta+delta);
      ty2 = -sin(angle)*r1*sin(theta+delta) + cos(angle)*r2*cos(theta+delta);

      if (delta < 0.0f) {
        tx1 = -tx1;
        ty1 = -ty1;
        tx2 = -tx2;
        ty2 = -ty2;
      }

      startAngle = atan2(ty1, tx1);
      endAngle = atan2(ty2, tx2);
    }
    break;
    
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
    {
      y = cy;
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegLinetoHorizontalRel> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
        x += cx;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegLinetoHorizontalAbs> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
      }
      startAngle = endAngle = atan2(0, x - cx);
    }
    break;        
    
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
    {
      x = cx;
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegLinetoVerticalRel> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetY(&y);
        y += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegLinetoVerticalAbs> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetY(&y);
      }
      startAngle = endAngle = atan2(y - cy, 0);
    }
    break;
    
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
    {
      float x1, y1, x2, y2;

      if (lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL        ||
          lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS        ||
          lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL ||
          lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS ) {
        // the first controlpoint is the reflection of the last one about the current point:
        x1 = 2*cx - cx1;
        y1 = 2*cy - cy1;
      }
      else {
        // the first controlpoint is equal to the current point:
        x1 = cx;
        y1 = cy;
      }
      
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicSmoothRel> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
        x  += cx;
        y  += cy;
        x2 += cx;
        y2 += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicSmoothAbs> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
      }

      cx1 = x2;
      cy1 = y2;

      if (x1 == cx && y1 == cy) {
        x1 = x2;
        y1 = y2;
      }

      if (x2 == x && y2 == y) {
        x2 = x1;
        y2 = y1;
      }

      startAngle = atan2(y1 - cy, x1 - cx);
      endAngle = atan2(y - y2, x - x2);
    }
    break;
    
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
      { 
        float x1, y1;
        
        if (lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL        ||
            lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS        ||
            lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL ||
            lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS ) {
          // the first controlpoint is the reflection of the last one about the current point:
          x1 = 2*cx - cx1;
          y1 = 2*cy - cy1;
        }
        else {
          // the first controlpoint is equal to the current point:
          x1 = cx;
          y1 = cy;
        }
        
        if (!absCoords) {
          nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticSmoothRel> curveseg = do_QueryInterface(segment);
          NS_ASSERTION(curveseg, "interface not implemented");
          curveseg->GetX(&x);
          curveseg->GetY(&y);
          x  += cx;
          y  += cy;
        } else {
          nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs> curveseg = do_QueryInterface(segment);
          NS_ASSERTION(curveseg, "interface not implemented");
          curveseg->GetX(&x);
          curveseg->GetY(&y);
        }

        cx1 = x1;
        cy1 = y1;
        
        startAngle = atan2(y1 - cy, x1 - cx);
        endAngle = atan2(y - y1, x - x1);
      }
      break;
      
    default:
      NS_ASSERTION(1==0, "unknown path segment");
      break;
    }
    lastSegmentType = type;

    if (newSegment &&
        type != nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS &&
        type != nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL) {
      pathIndex = aMarks->Count() - 1;
      pathAngle = startAngle;
      ((nsSVGMark *)aMarks->ElementAt(aMarks->Count()-1))->angle = pathAngle;
      newSegment = PR_FALSE;
      prevAngle = endAngle;
    } else if (type == nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS ||
               type == nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL) {
      if (aMarks->Count())
        ((nsSVGMark *)aMarks->ElementAt(aMarks->Count()-1))->angle = prevAngle;
    } else {
      ((nsSVGMark *)aMarks->ElementAt(aMarks->Count()-1))->angle =
        nsSVGUtils::AngleBisect(prevAngle, startAngle);
      prevAngle = endAngle;
    }
    
    nsSVGMark *mark = new nsSVGMark;
    mark->x = x;
    mark->y = y;
    aMarks->AppendElement(mark);
    
    if (type == nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH) {
      prevAngle = nsSVGUtils::AngleBisect(endAngle, pathAngle);
      ((nsSVGMark *)aMarks->ElementAt(pathIndex))->angle = prevAngle;
    }

    cx = x;
    cy = y;
  }

  if (aMarks->Count())
    ((nsSVGMark *)aMarks->ElementAt(aMarks->Count()-1))->angle = prevAngle;
}

nsIAtom *
nsSVGPathFrame::GetType() const
{
  return nsLayoutAtoms::svgPathFrame;
}

//----------------------------------------------------------------------
// nsISVGPathFlatten methods:

NS_IMETHODIMP
nsSVGPathFrame::GetFlattenedPath(nsSVGPathData **data,
                                 nsIFrame *parent)
{
  nsIFrame *oldParent = mParent;

  if (parent)
    mParent = parent;

  GetGeometry()->Flatten(this, data);

  if (parent)
    mParent = oldParent;

  return NS_OK;
}
