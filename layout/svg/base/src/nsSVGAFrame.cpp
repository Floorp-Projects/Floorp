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
#include "nsSVGAElement.h"
#include "nsSVGUtils.h"
#include "gfxMatrix.h"
#include "SVGLengthList.h"

// <a> elements can contain text. nsSVGGlyphFrames expect to have
// a class derived from nsSVGTextContainerFrame as a parent. We
// also need something that implements nsISVGGlyphFragmentNode to get
// the text DOM to work.

using namespace mozilla;

typedef nsSVGTSpanFrame nsSVGAFrameBase;

class nsSVGAFrame : public nsSVGAFrameBase
{
  friend nsIFrame*
  NS_NewSVGAFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGAFrame(nsStyleContext* aContext) :
    nsSVGAFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  // nsIFrame:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

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
  virtual void NotifySVGChanged(PRUint32 aFlags);
  
  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM();

  // nsSVGTextContainerFrame methods:
  virtual void GetXY(mozilla::SVGUserUnitList *aX, mozilla::SVGUserUnitList *aY);
  virtual void GetDxDy(mozilla::SVGUserUnitList *aDx, mozilla::SVGUserUnitList *aDy);
  virtual const SVGNumberList* GetRotate() {
    return nsnull;
  }

private:
  nsAutoPtr<gfxMatrix> mCanvasTM;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGAFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGAFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGAFrame)

//----------------------------------------------------------------------
// nsIFrame methods
#ifdef DEBUG
NS_IMETHODIMP
nsSVGAFrame::Init(nsIContent* aContent,
                  nsIFrame* aParent,
                  nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGAElement> elem = do_QueryInterface(aContent);
  NS_ASSERTION(elem,
               "Trying to construct an SVGAFrame for a "
               "content element that doesn't support the right interfaces");

  return nsSVGAFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

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
    
    nsSVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
  }

 return NS_OK;
}

nsIAtom *
nsSVGAFrame::GetType() const
{
  return nsGkAtoms::svgAFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
nsSVGAFrame::NotifySVGChanged(PRUint32 aFlags)
{
  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
  }

  nsSVGAFrameBase::NotifySVGChanged(aFlags);
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGAFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    NS_ASSERTION(mParent, "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
    nsSVGAElement *content = static_cast<nsSVGAElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformTo(parent->GetCanvasTM());

    mCanvasTM = new gfxMatrix(tm);
  }

  return *mCanvasTM;
}

//----------------------------------------------------------------------
// nsSVGTextContainerFrame methods:

void
nsSVGAFrame::GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY)
{
  aX->Clear();
  aY->Clear();
}

void
nsSVGAFrame::GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy)
{
  aDx->Clear();
  aDy->Clear();
}
