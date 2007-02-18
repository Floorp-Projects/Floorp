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

#include "nsSVGGFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsSVGMatrix.h"
#include "nsIDOMSVGUseElement.h"
#include "nsIDOMSVGTransformable.h"
#include "nsSVGElement.h"
#include "nsSVGUseElement.h"

typedef nsSVGGFrame nsSVGUseFrameBase;

class nsSVGUseFrame : public nsSVGUseFrameBase,
                      public nsIAnonymousContentCreator
{
  friend nsIFrame*
  NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

protected:
  nsSVGUseFrame(nsStyleContext* aContext) : nsSVGUseFrameBase(aContext) {}

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  

public:
  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  virtual void Destroy();

  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgUseFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGUse"), aResult);
  }
#endif

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements);
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(aContent);
  if (!transformable) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGUseFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGUseFrame(aContext);
}

nsIAtom *
nsSVGUseFrame::GetType() const
{
  return nsGkAtoms::svgUseFrame;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGUseFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsSVGUseFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGUseFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                nsIAtom*        aAttribute,
                                PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y)) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
    
    for (nsIFrame* kid = mFrames.FirstChild(); kid;
         kid = kid->GetNextSibling()) {
      nsISVGChildFrame* SVGFrame = nsnull;
      CallQueryInterface(kid, &SVGFrame);
      if (SVGFrame)
        SVGFrame->NotifyCanvasTMChanged(PR_FALSE);
    }
    return NS_OK;
  }

  return nsSVGUseFrameBase::AttributeChanged(aNameSpaceID,
                                             aAttribute, aModType);
}

void
nsSVGUseFrame::Destroy()
{
  nsSVGUseElement *use = NS_STATIC_CAST(nsSVGUseElement*, mContent);
  use->DestroyAnonymousContent();
  nsSVGUseFrameBase::Destroy();
}


//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

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
  nsSVGElement *element = NS_STATIC_CAST(nsSVGElement*, mContent);
  element->GetAnimatedLengthValues(&x, &y, nsnull);

  nsCOMPtr<nsIDOMSVGMatrix> fini;
  currentTM->Translate(x, y, getter_AddRefs(fini));

  nsIDOMSVGMatrix *retval = fini.get();
  NS_IF_ADDREF(retval);
  return retval;
}


//----------------------------------------------------------------------
// nsIAnonymousContentCreator methods:

nsresult
nsSVGUseFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  nsSVGUseElement *use = NS_STATIC_CAST(nsSVGUseElement*, mContent);

  nsIContent* clone = use->CreateAnonymousContent();
  if (!clone)
    return NS_ERROR_FAILURE;
  if (!aElements.AppendElement(clone))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}
