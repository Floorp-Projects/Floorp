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
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef __NS_SVGMASKFRAME_H__
#define __NS_SVGMASKFRAME_H__

#include "nsSVGContainerFrame.h"
#include "gfxPattern.h"
#include "gfxMatrix.h"

class gfxContext;

typedef nsSVGContainerFrame nsSVGMaskFrameBase;

class nsSVGMaskFrame : public nsSVGMaskFrameBase
{
  friend nsIFrame*
  NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGMaskFrame(nsStyleContext* aContext) :
    nsSVGMaskFrameBase(aContext),
    mInUse(PR_FALSE) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsSVGMaskFrame method:
  already_AddRefed<gfxPattern> ComputeMaskAlpha(nsSVGRenderState *aContext,
                                                nsIFrame* aParent,
                                                const gfxMatrix &aMatrix,
                                                float aOpacity = 1.0f);

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgMaskFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGMask"), aResult);
  }
#endif

private:
  // A helper class to allow us to paint masks safely. The helper
  // automatically sets and clears the mInUse flag on the mask frame
  // (to prevent nasty reference loops). It's easy to mess this up
  // and break things, so this helper makes the code far more robust.
  class AutoMaskReferencer
  {
  public:
    AutoMaskReferencer(nsSVGMaskFrame *aFrame)
       : mFrame(aFrame) {
      NS_ASSERTION(!mFrame->mInUse, "reference loop!");
      mFrame->mInUse = PR_TRUE;
    }
    ~AutoMaskReferencer() {
      mFrame->mInUse = PR_FALSE;
    }
  private:
    nsSVGMaskFrame *mFrame;
  };

  nsIFrame *mMaskParent;
  nsAutoPtr<gfxMatrix> mMaskParentMatrix;
  // recursion prevention flag
  bool mInUse;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM();
};

#endif
