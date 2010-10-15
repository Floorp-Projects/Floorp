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

#ifndef __NS_SVGOUTERSVGFRAME_H__
#define __NS_SVGOUTERSVGFRAME_H__

#include "nsSVGContainerFrame.h"
#include "nsISVGSVGFrame.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGNumber.h"
#include "gfxMatrix.h"

class nsSVGForeignObjectFrame;

////////////////////////////////////////////////////////////////////////
// nsSVGOuterSVGFrame class

typedef nsSVGDisplayContainerFrame nsSVGOuterSVGFrameBase;

class nsSVGOuterSVGFrame : public nsSVGOuterSVGFrameBase,
                           public nsISVGSVGFrame
{
  friend nsIFrame*
  NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGOuterSVGFrame(nsStyleContext* aContext);

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  ~nsSVGOuterSVGFrame() {
    NS_ASSERTION(mForeignObjectHash.Count() == 0,
                 "foreignObject(s) still registered!");
  }
#endif

  // nsIFrame:
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);

  virtual IntrinsicSize GetIntrinsicSize();
  virtual nsSize  GetIntrinsicRatio();

  virtual nsSize ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  DidReflow(nsPresContext*   aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus aStatus);

  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint& aPoint);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual nsSplittableType GetSplittableType() const;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgOuterSVGFrame
   */
  virtual nsIAtom* GetType() const;

  void Paint(const nsDisplayListBuilder* aBuilder,
             nsIRenderingContext& aRenderingContext,
             const nsRect& aDirtyRect, nsPoint aPt);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGOuterSVG"), aResult);
  }
#endif

  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsSVGOuterSVGFrame methods:

  void InvalidateCoveredRegion(nsIFrame *aFrame);
  // Calls aSVG->UpdateCoveredRegion and returns true if the covered
  // region actually changed. If it changed, invalidates the old and new
  // covered regions, taking filters into account, like
  // InvalidateCoveredRegion.
  PRBool UpdateAndInvalidateCoveredRegion(nsIFrame *aFrame);

  PRBool IsRedrawSuspended();

  // nsISVGSVGFrame interface:
  NS_IMETHOD SuspendRedraw();
  NS_IMETHOD UnsuspendRedraw();
  NS_IMETHOD NotifyViewportChange();

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM();

  /* Methods to allow descendant nsSVGForeignObjectFrame frames to register and
   * unregister themselves with their nearest nsSVGOuterSVGFrame ancestor so
   * they can be reflowed. The methods return PR_TRUE on success or PR_FALSE on
   * failure.
   */
  void RegisterForeignObject(nsSVGForeignObjectFrame* aFrame);
  void UnregisterForeignObject(nsSVGForeignObjectFrame* aFrame);

protected:

  /* Returns true if our content is the document element and our document is
   * embedded in an HTML 'object', 'embed' or 'applet' element. Set
   * aEmbeddingFrame to obtain the nsIFrame for the embedding HTML element.
   */
  PRBool EmbeddedByReference(nsIFrame **aEmbeddingFrame = nsnull);

  /* Returns true if our content is the document element and our document is
   * being used as an image.
   */
  PRBool IsRootOfImage();

  // A hash-set containing our nsSVGForeignObjectFrame descendants. Note we use
  // a hash-set to avoid the O(N^2) behavior we'd get tearing down an SVG frame
  // subtree if we were to use a list (see bug 381285 comment 20).
  nsTHashtable<nsVoidPtrHashKey> mForeignObjectHash;

  PRUint32 mRedrawSuspendCount;
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;

  float mFullZoom;

  PRPackedBool mViewportInitialized;
#ifdef XP_MACOSX
  PRPackedBool mEnableBitmapFallback;
#endif
  PRPackedBool mIsRootContent;
};

#endif
