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
 *    William Cook <william.cook@crocodile-clips.com> (original author)
 *    Alex Fritze <alex.fritze@crocodile-clips.com>
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
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGEllipseElement.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsISVGRendererPathBuilder.h"

class nsSVGEllipseFrame : public nsSVGPathGeometryFrame
{
  friend nsresult
  NS_NewSVGEllipseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);

  virtual ~nsSVGEllipseFrame();

  virtual nsresult Init();

  // nsISVGValueObserver interface:
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(nsISVGRendererPathBuilder *pathBuilder);

  nsCOMPtr<nsIDOMSVGLength> mCx;
  nsCOMPtr<nsIDOMSVGLength> mCy;
  nsCOMPtr<nsIDOMSVGLength> mRx;
  nsCOMPtr<nsIDOMSVGLength> mRy;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGEllipseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  nsCOMPtr<nsIDOMSVGEllipseElement> ellipse = do_QueryInterface(aContent);
  if (!ellipse) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGEllipseFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }

  nsSVGEllipseFrame* it = new (aPresShell) nsSVGEllipseFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
}

nsSVGEllipseFrame::~nsSVGEllipseFrame()
{
  nsCOMPtr<nsISVGValue> value;
  if (mCx && (value = do_QueryInterface(mCx)))
      value->RemoveObserver(this);
  if (mCy && (value = do_QueryInterface(mCy)))
      value->RemoveObserver(this);
  if (mRx && (value = do_QueryInterface(mRx)))
      value->RemoveObserver(this);
  if (mRy && (value = do_QueryInterface(mRy)))
      value->RemoveObserver(this);
}

nsresult nsSVGEllipseFrame::Init()
{
  nsresult rv = nsSVGPathGeometryFrame::Init();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMSVGEllipseElement> ellipse = do_QueryInterface(mContent);
  NS_ASSERTION(ellipse,"wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    ellipse->GetCx(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mCx));
    NS_ASSERTION(mCx, "no cx");
    if (!mCx) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mCx);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    ellipse->GetCy(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mCy));
    NS_ASSERTION(mCy, "no cy");
    if (!mCy) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mCy);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    ellipse->GetRx(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mRx));
    NS_ASSERTION(mRx, "no rx");
    if (!mRx) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mRx);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    ellipse->GetRy(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mRy));
    NS_ASSERTION(mRy, "no ry");
    if (!mRy) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mRy);
    if (value)
      value->AddObserver(this);
  }

  return NS_OK; 
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGEllipseFrame::DidModifySVGObservable(nsISVGValue* observable)
{
  nsCOMPtr<nsIDOMSVGLength> l = do_QueryInterface(observable);
  if (l && (mCx==l || mCy==l || mRx==l || mRy==l)) {
    UpdateGraphic(nsISVGPathGeometrySource::UPDATEMASK_PATH);
    return NS_OK;
  }
  // else
  return nsSVGPathGeometryFrame::DidModifySVGObservable(observable);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

/* void constructPath (in nsISVGRendererPathBuilder pathBuilder); */
NS_IMETHODIMP nsSVGEllipseFrame::ConstructPath(nsISVGRendererPathBuilder* pathBuilder)
{
  float x,y,rx,ry;

  mCx->GetValue(&x);
  mCy->GetValue(&y);
  mRx->GetValue(&rx);
  mRy->GetValue(&ry);

  // build ellipse from two arcs
  pathBuilder->Moveto(x-rx, y);
  pathBuilder->Arcto(x+rx, y, rx, ry, 0.0, 0, 0);
  pathBuilder->Arcto(x-rx, y, rx, ry, 0.0, 1, 0);
  pathBuilder->ClosePath(&x,&y);

  return NS_OK;
}
