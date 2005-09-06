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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIDOMSVGTransformable.h"
#include "nsSVGDefsFrame.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsSVGLength.h"
#include "nsSVGGFrame.h"
#include "nsIDOMSVGUseElement.h"
#include "nsISVGValue.h"
#include "nsIAnonymousContentCreator.h"
#include "nsSVGMatrix.h"
#include "nsLayoutAtoms.h"

typedef nsSVGGFrame nsSVGUseFrameBase;

class nsSVGUseFrame : public nsSVGUseFrameBase,
                      public nsIAnonymousContentCreator
{
  friend nsresult
  NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);

protected:

  NS_IMETHOD InitSVG();

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  

public:

  // nsISVGContainerFrame interface:
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgUseFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGUse"), aResult);
  }
#endif

  // nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsPresContext* aPresContext,
                                    nsISupportsArray& aAnonymousItems);
  NS_IMETHOD CreateFrameFor(nsPresContext *aPresContext,
                            nsIContent *aContent,
                            nsIFrame **aFrame);

protected:

  nsCOMPtr<nsIDOMSVGLength> mX;
  nsCOMPtr<nsIDOMSVGLength> mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(aContent);
  if (!transformable) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGUseFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsSVGUseFrame* it = new (aPresShell) nsSVGUseFrame;
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

NS_IMETHODIMP
nsSVGUseFrame::InitSVG()
{
  nsresult rv = nsSVGUseFrameBase::InitSVG();
  if (NS_FAILED(rv)) return rv;
 
  nsCOMPtr<nsIDOMSVGUseElement> UseElement = do_QueryInterface(mContent);

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    UseElement->GetX(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no x");
    if (!mX) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mX);
    if (value)
      value->AddObserver(this);  // nsISVGValueObserver
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    UseElement->GetY(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no y");
    if (!mY) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mY);
    if (value)
      value->AddObserver(this);
  }

  return NS_OK;
}

nsIAtom *
nsSVGUseFrame::GetType() const
{
  return nsLayoutAtoms::svgUseFrame;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGUseFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsSVGUseFrameBase)

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGUseFrame::GetCanvasTM()
{
  if (!mPropagateTransform) {
    nsIDOMSVGMatrix *retval;
    if (mOverrideCTM) {
      retval = mOverrideCTM;
      NS_ADDREF(retval);
    } else {
      NS_NewSVGMatrix(&retval);
    }
    return retval;
  }

  nsCOMPtr<nsIDOMSVGMatrix> currentTM = nsSVGUseFrameBase::GetCanvasTM();

  // x and y:
  float x, y;
  mX->GetValue(&x);
  mY->GetValue(&y);
  nsCOMPtr<nsIDOMSVGMatrix> fini;
  currentTM->Translate(x, y, getter_AddRefs(fini));

  nsIDOMSVGMatrix *retval = fini.get();
  NS_IF_ADDREF(retval);
  return retval;
}


//----------------------------------------------------------------------
// nsIAnonymousContentCreator methods:

NS_IMETHODIMP
nsSVGUseFrame::CreateAnonymousContent(nsPresContext* aPresContext,
                                      nsISupportsArray& aAnonymousItems)
{
  nsCOMPtr<nsIAnonymousContentCreator> use = do_QueryInterface(mContent);

  if (use)
    return use->CreateAnonymousContent(aPresContext, aAnonymousItems);
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGUseFrame::CreateFrameFor(nsPresContext *aPresContext,
                              nsIContent *aContent,
                              nsIFrame **aFrame)
{
  *aFrame = nsnull;
  return NS_ERROR_FAILURE;
}
