/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERFRAME_H__
#define __NS_SVGFILTERFRAME_H__

#include "nsFrame.h"
#include "nsQueryFrame.h"
#include "nsRect.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class nsIAtom;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsRenderingContext;
class nsStyleContext;
class nsSVGFilterPaintCallback;
class nsSVGFilterElement;
class nsSVGIntegerPair;
class nsSVGLength2;

typedef nsSVGContainerFrame nsSVGFilterFrameBase;

class nsSVGFilterFrame : public nsSVGFilterFrameBase
{
  friend nsIFrame*
  NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGFilterFrame(nsStyleContext* aContext)
    : nsSVGFilterFrameBase(aContext),
      mLoopFlag(false),
      mNoHRefURI(false)
  {
    AddStateBits(NS_STATE_SVG_NONDISPLAY_CHILD);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  nsresult FilterPaint(nsRenderingContext *aContext,
                       nsIFrame *aTarget, nsSVGFilterPaintCallback *aPaintCallback,
                       const nsIntRect* aDirtyRect);

  /**
   * Returns the area that could change when the given rect of the source changes.
   * The rectangles are relative to the origin of the outer svg, if aTarget is SVG,
   * relative to aTarget itself otherwise, in device pixels.
   */
  nsIntRect GetInvalidationBBox(nsIFrame *aTarget, const nsIntRect& aRect);

  /**
   * Returns the area in device pixels that is needed from the source when
   * the given area needs to be repainted.
   * The rectangles are relative to the origin of the outer svg, if aTarget is SVG,
   * relative to aTarget itself otherwise, in device pixels.
   */
  nsIntRect GetSourceForInvalidArea(nsIFrame *aTarget, const nsIntRect& aRect);

  /**
   * Returns the bounding box of the post-filter area of aTarget.
   * The rectangles are relative to the origin of the outer svg, if aTarget is SVG,
   * relative to aTarget itself otherwise, in device pixels.
   * @param aOverrideBBox overrides the normal bbox for the source, if non-null
   */
  nsIntRect GetFilterBBox(nsIFrame *aTarget,
                          const nsIntRect *aOverrideBBox = nsnull,
                          const nsIntRect *aPreFilterBounds = nsnull);

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFilterFrame
   */
  virtual nsIAtom* GetType() const;

private:
  // Parse our xlink:href and set up our nsSVGPaintingProperty if we
  // reference another filter and we don't have a property. Return
  // the referenced filter's frame if available, null otherwise.
  class AutoFilterReferencer;
  friend class nsAutoFilterInstance;
  nsSVGFilterFrame* GetReferencedFilter();
  nsSVGFilterFrame* GetReferencedFilterIfNotInUse();

  // Accessors to lookup filter attributes
  PRUint16 GetEnumValue(PRUint32 aIndex, nsIContent *aDefault);
  PRUint16 GetEnumValue(PRUint32 aIndex)
  {
    return GetEnumValue(aIndex, mContent);
  }
  const nsSVGIntegerPair *GetIntegerPairValue(PRUint32 aIndex, nsIContent *aDefault);
  const nsSVGIntegerPair *GetIntegerPairValue(PRUint32 aIndex)
  {
    return GetIntegerPairValue(aIndex, mContent);
  }
  const nsSVGLength2 *GetLengthValue(PRUint32 aIndex, nsIContent *aDefault);
  const nsSVGLength2 *GetLengthValue(PRUint32 aIndex)
  {
    return GetLengthValue(aIndex, mContent);
  }
  const nsSVGFilterElement *GetFilterContent(nsIContent *aDefault);
  const nsSVGFilterElement *GetFilterContent()
  {
    return GetFilterContent(mContent);
  }

  // This flag is used to detect loops in xlink:href processing
  bool                              mLoopFlag;
  bool                              mNoHRefURI;
};

#endif
