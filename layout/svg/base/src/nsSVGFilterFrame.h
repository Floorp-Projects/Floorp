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

  // nsIFrame methods:
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) {
    return NS_OK;
  }

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  /**
   * Paint the given filtered frame.
   * @param aDirtyArea The area than needs to be painted, in aFilteredFrame's
   *   frame space (i.e. relative to its origin, the top-left corner of its
   *   border box).
   */
  nsresult PaintFilteredFrame(nsRenderingContext *aContext,
                              nsIFrame *aFilteredFrame,
                              nsSVGFilterPaintCallback *aPaintCallback,
                              const nsRect* aDirtyArea);

  /**
   * Returns the post-filter area that could be dirtied when the given
   * pre-filter area of aFilteredFrame changes.
   * @param aPreFilterDirtyRect The pre-filter area of aFilteredFrame that has
   *   changed, relative to aFilteredFrame, in app units.
   */
  nsRect GetPostFilterDirtyArea(nsIFrame *aFilteredFrame,
                                const nsRect& aPreFilterDirtyRect);

  /**
   * Returns the pre-filter area that is needed from aFilteredFrame when the
   * given post-filter area needs to be repainted.
   * @param aPostFilterDirtyRect The post-filter area that is dirty, relative
   *   to aFilteredFrame, in app units.
   */
  nsRect GetPreFilterNeededArea(nsIFrame *aFilteredFrame,
                                const nsRect& aPostFilterDirtyRect);

  /**
   * Returns the post-filter visual overflow rect (paint bounds) of
   * aFilteredFrame.
   * @param aOverrideBBox A user space rect, in user units, that should be used
   *   as aFilteredFrame's bbox ('bbox' is a specific SVG term), if non-null.
   * @param aPreFilterBounds The pre-filter visual overflow rect of
   *   aFilteredFrame, if non-null.
   */
  nsRect GetPostFilterBounds(nsIFrame *aFilteredFrame,
                             const gfxRect *aOverrideBBox = nsnull,
                             const nsRect *aPreFilterBounds = nsnull);

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
