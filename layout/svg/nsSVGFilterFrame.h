/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERFRAME_H__
#define __NS_SVGFILTERFRAME_H__

#include "mozilla/Attributes.h"
#include "nsFrame.h"
#include "nsQueryFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class nsIAtom;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsStyleContext;
class nsSVGLength2;

struct nsRect;

namespace mozilla {
namespace dom {
class SVGFilterElement;
} // namespace dom
} // namespace mozilla

class nsSVGFilterFrame : public nsSVGContainerFrame
{
  friend nsIFrame*
  NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit nsSVGFilterFrame(nsStyleContext* aContext)
    : nsSVGContainerFrame(aContext)
    , mLoopFlag(false)
    , mNoHRefURI(false)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame methods:
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {}

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFilterFrame
   */
  virtual nsIAtom* GetType() const override;

private:
  // Parse our xlink:href and set up our nsSVGPaintingProperty if we
  // reference another filter and we don't have a property. Return
  // the referenced filter's frame if available, null otherwise.
  class AutoFilterReferencer;
  friend class nsSVGFilterInstance;
  nsSVGFilterFrame* GetReferencedFilter();
  nsSVGFilterFrame* GetReferencedFilterIfNotInUse();

  // Accessors to lookup filter attributes
  uint16_t GetEnumValue(uint32_t aIndex, nsIContent *aDefault);
  uint16_t GetEnumValue(uint32_t aIndex)
  {
    return GetEnumValue(aIndex, mContent);
  }
  const nsSVGLength2 *GetLengthValue(uint32_t aIndex, nsIContent *aDefault);
  const nsSVGLength2 *GetLengthValue(uint32_t aIndex)
  {
    return GetLengthValue(aIndex, mContent);
  }
  const mozilla::dom::SVGFilterElement *GetFilterContent(nsIContent *aDefault);
  const mozilla::dom::SVGFilterElement *GetFilterContent()
  {
    return GetFilterContent(mContent);
  }

  // This flag is used to detect loops in xlink:href processing
  bool                              mLoopFlag;
  bool                              mNoHRefURI;
};

#endif
