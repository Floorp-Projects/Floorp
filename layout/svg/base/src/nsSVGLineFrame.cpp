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
 *   William Cook <william.cook@crocodile-clips.com> (original author)
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
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
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGLineElement.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsISVGRendererPathBuilder.h"

class nsSVGLineFrame : public nsSVGPathGeometryFrame
{
protected:
  friend nsresult
  NS_NewSVGLineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);

  virtual ~nsSVGLineFrame();
  virtual nsresult Init();

public:
  // nsISVGValueObserver interface:
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(nsISVGRendererPathBuilder *pathBuilder);
  
private:
  nsCOMPtr<nsIDOMSVGLength> mX1;
  nsCOMPtr<nsIDOMSVGLength> mY1;
  nsCOMPtr<nsIDOMSVGLength> mX2;
  nsCOMPtr<nsIDOMSVGLength> mY2;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGLineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  nsCOMPtr<nsIDOMSVGLineElement> line = do_QueryInterface(aContent);
  if (!line) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGLineFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }

  nsSVGLineFrame* it = new (aPresShell) nsSVGLineFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
}

nsSVGLineFrame::~nsSVGLineFrame()
{
  nsCOMPtr<nsISVGValue> value;
  if (mX1 && (value = do_QueryInterface(mX1)))
      value->RemoveObserver(this);
  if (mY1 && (value = do_QueryInterface(mY1)))
      value->RemoveObserver(this);
  if (mX2 && (value = do_QueryInterface(mX2)))
      value->RemoveObserver(this);
  if (mY2 && (value = do_QueryInterface(mY2)))
      value->RemoveObserver(this);
}

nsresult nsSVGLineFrame::Init()
{
  nsresult rv = nsSVGPathGeometryFrame::Init();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMSVGLineElement> line = do_QueryInterface(mContent);
  NS_ASSERTION(line,"wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    line->GetX1(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mX1));
    NS_ASSERTION(mX1, "no x1");
    if (!mX1) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mX1);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    line->GetY1(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mY1));
    NS_ASSERTION(mY1, "no y1");
    if (!mY1) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mY1);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    line->GetX2(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mX2));
    NS_ASSERTION(mX2, "no x2");
    if (!mX2) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mX2);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    line->GetY2(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mY2));
    NS_ASSERTION(mY2, "no y2");
    if (!mY2) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mY2);
    if (value)
      value->AddObserver(this);
  }

  return NS_OK; 
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGLineFrame::DidModifySVGObservable(nsISVGValue* observable)
{
  nsCOMPtr<nsIDOMSVGLength> l = do_QueryInterface(observable);
  if (l && (mX1==l || mY1==l || mX2==l || mY2==l)) {
    UpdateGraphic(nsISVGPathGeometrySource::UPDATEMASK_PATH);
    return NS_OK;
  }
  // else
  return nsSVGPathGeometryFrame::DidModifySVGObservable(observable);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

/* void constructPath (in nsISVGRendererPathBuilder pathBuilder); */
NS_IMETHODIMP nsSVGLineFrame::ConstructPath(nsISVGRendererPathBuilder* pathBuilder)
{
  float x1,y1,x2,y2;

  mX1->GetValue(&x1);
  mY1->GetValue(&y1);
  mX2->GetValue(&x2);
  mY2->GetValue(&y2);

  // move to start coordinates then draw line to end coordinates
  pathBuilder->Moveto(x1, y1);
  pathBuilder->Lineto(x2, y2);

  return NS_OK;
}

// const nsStyleSVG* nsSVGLineFrame::GetStyle()
// {
//   // XXX TODO: strip out any fill color as per svg specs
//   return nsSVGGraphicFrame::GetStyle();
// }
