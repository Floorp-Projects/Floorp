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
 *    Håkan Waara <hwaara@chello.se>
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
#include "nsIDOMSVGRectElement.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"

#include "nsISVGRendererPathBuilder.h"

class nsSVGRectFrame : public nsSVGPathGeometryFrame
{
protected:
  friend nsresult
  NS_NewSVGRectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);

  virtual ~nsSVGRectFrame();
  virtual nsresult Init();

public:
  // nsISVGValueObserver interface:
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(nsISVGRendererPathBuilder *pathBuilder);

private:
  nsCOMPtr<nsIDOMSVGLength> mX;
  nsCOMPtr<nsIDOMSVGLength> mY;
  nsCOMPtr<nsIDOMSVGLength> mWidth;
  nsCOMPtr<nsIDOMSVGLength> mHeight;
  nsCOMPtr<nsIDOMSVGLength> mRx;
  nsCOMPtr<nsIDOMSVGLength> mRy;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGRectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  nsCOMPtr<nsIDOMSVGRectElement> Rect = do_QueryInterface(aContent);
  if (!Rect) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGRectFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }

  nsSVGRectFrame* it = new (aPresShell) nsSVGRectFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
}

nsSVGRectFrame::~nsSVGRectFrame()
{
  nsCOMPtr<nsISVGValue> value;
  if (mX && (value = do_QueryInterface(mX)))
      value->RemoveObserver(this);
  if (mY && (value = do_QueryInterface(mY)))
      value->RemoveObserver(this);
  if (mWidth && (value = do_QueryInterface(mWidth)))
      value->RemoveObserver(this);
  if (mHeight && (value = do_QueryInterface(mHeight)))
      value->RemoveObserver(this);
  if (mRx && (value = do_QueryInterface(mRx)))
      value->RemoveObserver(this);
  if (mRy && (value = do_QueryInterface(mRy)))
      value->RemoveObserver(this);
}

nsresult nsSVGRectFrame::Init()
{
  nsresult rv = nsSVGPathGeometryFrame::Init();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMSVGRectElement> Rect = do_QueryInterface(mContent);
  NS_ASSERTION(Rect,"wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    Rect->GetX(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no x");
    if (!mX) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mX);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    Rect->GetY(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no y");
    if (!mY) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mY);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    Rect->GetWidth(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mWidth));
    NS_ASSERTION(mWidth, "no width");
    if (!mWidth) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mWidth);
    if (value)
      value->AddObserver(this);
  }
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    Rect->GetHeight(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mHeight));
    NS_ASSERTION(mHeight, "no height");
    if (!mHeight) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mHeight);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    Rect->GetRx(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mRx));
    NS_ASSERTION(mRx, "no rx");
    if (!mRx) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mRx);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    Rect->GetRy(getter_AddRefs(length));
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
nsSVGRectFrame::DidModifySVGObservable(nsISVGValue* observable)
{
  nsCOMPtr<nsIDOMSVGLength> l = do_QueryInterface(observable);
  if (l && (mX==l || mY==l || mWidth==l || mHeight==l || mRx==l || mRy==l)) {
    UpdateGraphic(nsISVGPathGeometrySource::UPDATEMASK_PATH);
    return NS_OK;
  }
  // else
  return nsSVGPathGeometryFrame::DidModifySVGObservable(observable);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

/* void constructPath (in nsISVGRendererPathBuilder pathBuilder); */
NS_IMETHODIMP nsSVGRectFrame::ConstructPath(nsISVGRendererPathBuilder* pathBuilder)
{
  float x, y, width, height, rx, ry;

  mX->GetValue(&x);
  mY->GetValue(&y);
  mWidth->GetValue(&width);
  mHeight->GetValue(&height);
  mRx->GetValue(&rx);
  mRy->GetValue(&ry);

  /* In a perfect world, this would be handled by the DOM, and 
     return a DOM exception. */
  if (width == 0 || height == 0 || ry < 0 || rx < 0)
    return NS_OK;

  /* If any of the attributes are not set, we need to set it to the corresponding
     attribute's value (e.g. if rx is not set, assign ry's value to rx). */
  if (!rx || !ry)
  {
    if (rx < ry)
      rx = ry;
    else
      ry = rx;
  }

  // Rx/ry must not be values higher than half the rectangle's width/height
  if (rx > (width/2))
    rx = width/2;

  if (ry > (height/2))
    ry = height/2;

  pathBuilder->Moveto(x+rx, y);
  pathBuilder->Lineto(x+width-rx, y);
  pathBuilder->Arcto(x+width, y+ry , rx, ry, 0.0, 0, 1);
  pathBuilder->Lineto(x+width, y+height-ry);
  pathBuilder->Arcto(x+width-rx, y+height , rx, ry, 0.0, 0, 1);
  pathBuilder->Lineto(x+rx,y+height);
  pathBuilder->Arcto(x, y+height-ry , rx, ry, 0.0, 0, 1);
  pathBuilder->Lineto(x,y+ry);
  pathBuilder->Arcto(x+rx, y, rx, ry, 0.0, 0, 1);
  pathBuilder->ClosePath(&x,&y);

  return NS_OK;
}
