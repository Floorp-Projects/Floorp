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
 * Robert Longson.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Longson <longsonr@gmail.com> (original author)
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

#include "nsSVGTSpanFrame.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGMatrix.h"
#include "nsIDOMSVGAElement.h"
#include "nsSVGUtils.h"

// <a> elements can contain text. nsSVGGlyphFrames expect to have
// a class derived from nsSVGTextContainerFrame as a parent. We
// also need something that implements nsISVGGlyphFragmentNode to get
// the text DOM to work.

typedef nsSVGTSpanFrame nsSVGAFrameBase;

class nsSVGAFrame : public nsSVGAFrameBase
{
  friend nsIFrame*
  NS_NewSVGAFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                  nsStyleContext* aContext);
protected:
  nsSVGAFrame(nsStyleContext* aContext) :
    nsSVGAFrameBase(aContext) {}

public:
  // nsIFrame:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  NS_IMETHOD DidSetStyleContext();

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgAFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGA"), aResult);
  }
#endif
  // nsISVGChildFrame interface:
  NS_IMETHOD NotifyCanvasTMChanged(PRBool suppressInvalidation);
  
  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  
private:
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGAFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGAElement> elem = do_QueryInterface(aContent);
  if (!elem) {
    NS_ERROR("Trying to construct an SVGAFrame for a "
             "content element that doesn't support the right interfaces");
    return nsnull;
  }

  return new (aPresShell) nsSVGAFrame(aContext);
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGAFrame::AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType)
{
  if (aNameSpaceID != kNameSpaceID_None)
    return NS_OK;

  if (aAttribute == nsGkAtoms::transform) {
    // transform has changed

    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
    
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGChildFrame* SVGFrame = nsnull;
      CallQueryInterface(kid, &SVGFrame);
      if (SVGFrame)
        SVGFrame->NotifyCanvasTMChanged(PR_FALSE);
      kid = kid->GetNextSibling();
    }
  }

 return NS_OK;
}

NS_IMETHODIMP
nsSVGAFrame::DidSetStyleContext()
{
  nsSVGUtils::StyleEffects(this);

  return NS_OK;
}

nsIAtom *
nsSVGAFrame::GetType() const
{
  return nsGkAtoms::svgAFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGAFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  // make sure our cached transform matrix gets (lazily) updated
  mCanvasTM = nsnull;

  return nsSVGAFrameBase::NotifyCanvasTMChanged(suppressInvalidation);
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGAFrame::GetCanvasTM()
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

  if (!mCanvasTM) {
    // get our parent's tm and append local transforms (if any):
    NS_ASSERTION(mParent, "null parent");
    nsSVGContainerFrame *containerFrame = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                         mParent);
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // got the parent tm, now check for local tm:
    nsSVGGraphicElement *element =
      NS_STATIC_CAST(nsSVGGraphicElement*, mContent);
    nsCOMPtr<nsIDOMSVGMatrix> localTM = element->GetLocalTransformMatrix();

    if (localTM)
      parentTM->Multiply(localTM, getter_AddRefs(mCanvasTM));
    else
      mCanvasTM = parentTM;
  }

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}
