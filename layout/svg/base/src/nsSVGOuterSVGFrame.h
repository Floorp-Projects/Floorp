/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGOUTERSVGFRAME_H__
#define __NS_SVGOUTERSVGFRAME_H__

#include "gfxMatrix.h"
#include "nsISVGSVGFrame.h"
#include "nsSVGContainerFrame.h"

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

  // nsIFrame:
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);

  virtual IntrinsicSize GetIntrinsicSize();
  virtual nsSize  GetIntrinsicRatio();

  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRUint32 aFlags) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  DidReflow(nsPresContext*   aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus aStatus);

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
             nsRenderingContext* aContext,
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

  virtual bool IsSVGTransformed(gfxMatrix *aOwnTransform,
                                gfxMatrix *aFromParentTransform) const {
    // Outer-<svg> can transform its children with viewBox, currentScale and
    // currentTranslate, but it itself is not transformed by SVG transforms.
    return false;
  }

  // nsISVGSVGFrame interface:
  virtual void NotifyViewportOrTransformChanged(PRUint32 aFlags);

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM();

  virtual bool HasChildrenOnlyTransform(gfxMatrix *aTransform) const;

#ifdef XP_MACOSX
  bool BitmapFallbackEnabled() const {
    return mEnableBitmapFallback;
  }
  void SetBitmapFallbackEnabled(bool aVal) {
    NS_NOTREACHED("don't think me need this any more"); // comment in bug 732429 if we do
    mEnableBitmapFallback = aVal;
  }
#endif

  /**
   * Return true only if the height is unspecified (defaulting to 100%) or else
   * the height is explicitly set to a percentage value no greater than 100%.
   */
  bool VerticalScrollbarNotNeeded() const;

#ifdef DEBUG
  bool IsCallingUpdateBounds() const {
    return mCallingUpdateBounds;
  }
#endif

protected:

#ifdef DEBUG
  bool mCallingUpdateBounds;
#endif

  /* Returns true if our content is the document element and our document is
   * embedded in an HTML 'object', 'embed' or 'applet' element. Set
   * aEmbeddingFrame to obtain the nsIFrame for the embedding HTML element.
   */
  bool IsRootOfReplacedElementSubDoc(nsIFrame **aEmbeddingFrame = nsnull);

  /* Returns true if our content is the document element and our document is
   * being used as an image.
   */
  bool IsRootOfImage();

  nsAutoPtr<gfxMatrix> mCanvasTM;

  float mFullZoom;

  bool mViewportInitialized;
#ifdef XP_MACOSX
  bool mEnableBitmapFallback;
#endif
  bool mIsRootContent;
};

#endif
