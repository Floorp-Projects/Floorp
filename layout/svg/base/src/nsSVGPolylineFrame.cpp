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

#include "nsSVGPathGeometryFrame.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsIDOMSVGPointList.h"
#include "nsIDOMSVGPoint.h"
//#include "nsASVGPathBuilder.h"
#include "nsISVGRendererPathBuilder.h"

class nsSVGPolylineFrame : public nsSVGPathGeometryFrame
{
protected:
  friend nsresult
  NS_NewSVGPolylineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);

  virtual ~nsSVGPolylineFrame();
  virtual nsresult Init();

public:
  // nsISVGValueObserver interface:
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(nsISVGRendererPathBuilder *pathBuilder);

//  virtual void ConstructPath(nsASVGPathBuilder* pathBuilder);

  nsCOMPtr<nsIDOMSVGPointList> mPoints;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPolylineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  nsCOMPtr<nsIDOMSVGAnimatedPoints> anim_points = do_QueryInterface(aContent);
  if (!anim_points) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGPolylineFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsSVGPolylineFrame* it = new (aPresShell) nsSVGPolylineFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
}

nsSVGPolylineFrame::~nsSVGPolylineFrame()
{
  nsCOMPtr<nsISVGValue> value;
  if (mPoints && (value = do_QueryInterface(mPoints)))
      value->RemoveObserver(this);
}

nsresult nsSVGPolylineFrame::Init()
{
  nsresult rv = nsSVGPathGeometryFrame::Init();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMSVGAnimatedPoints> anim_points = do_QueryInterface(mContent);
  NS_ASSERTION(anim_points,"wrong content element");
  anim_points->GetPoints(getter_AddRefs(mPoints));
  NS_ASSERTION(mPoints, "no points");
  if (!mPoints) return NS_ERROR_FAILURE;
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mPoints);
  if (value)
    value->AddObserver(this);
  return NS_OK; 
}  

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGPolylineFrame::DidModifySVGObservable(nsISVGValue* observable)
{
  nsCOMPtr<nsIDOMSVGPointList> l = do_QueryInterface(observable);
  if (l && mPoints==l) {
    UpdateGraphic(nsISVGPathGeometrySource::UPDATEMASK_PATH);
    return NS_OK;
  }
  // else
  return nsSVGPathGeometryFrame::DidModifySVGObservable(observable);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

/* void constructPath (in nsISVGRendererPathBuilder pathBuilder); */
NS_IMETHODIMP nsSVGPolylineFrame::ConstructPath(nsISVGRendererPathBuilder* pathBuilder)
{
  if (!mPoints) return NS_OK;

  PRUint32 count;
  mPoints->GetNumberOfItems(&count);
  if (count == 0) return NS_OK;
  
  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMSVGPoint> point;
    mPoints->GetItem(i, getter_AddRefs(point));

    float x, y;
    point->GetX(&x);
    point->GetY(&y);
    if (i == 0)
      pathBuilder->Moveto(x, y);
    else
      pathBuilder->Lineto(x, y);
  }

  return NS_OK;
}
