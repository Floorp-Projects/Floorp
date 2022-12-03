/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGFILTERFRAME_H_
#define LAYOUT_SVG_SVGFILTERFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/SVGContainerFrame.h"
#include "nsQueryFrame.h"

class nsAtom;
class nsIContent;
class nsIFrame;

struct nsRect;

namespace mozilla {
class SVGAnimatedLength;
class SVGFilterInstance;
class PresShell;

namespace dom {
class SVGFilterElement;
}  // namespace dom
}  // namespace mozilla

nsIFrame* NS_NewSVGFilterFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGFilterFrame final : public SVGContainerFrame {
  friend nsIFrame* ::NS_NewSVGFilterFrame(mozilla::PresShell* aPresShell,
                                          ComputedStyle* aStyle);

 protected:
  explicit SVGFilterFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGContainerFrame(aStyle, aPresContext, kClassID),
        mLoopFlag(false),
        mNoHRefURI(false) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGFilterFrame)

  // nsIFrame methods:
  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override {}

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

 private:
  friend class SVGFilterInstance;

  /**
   * Parses this frame's href and - if it references another filter - returns
   * it.  It also makes this frame a rendering observer of the specified ID.
   */
  SVGFilterFrame* GetReferencedFilter();

  // Accessors to lookup filter attributes
  uint16_t GetEnumValue(uint32_t aIndex, nsIContent* aDefault);
  uint16_t GetEnumValue(uint32_t aIndex) {
    return GetEnumValue(aIndex, mContent);
  }
  const mozilla::SVGAnimatedLength* GetLengthValue(uint32_t aIndex,
                                                   nsIContent* aDefault);
  const mozilla::SVGAnimatedLength* GetLengthValue(uint32_t aIndex) {
    return GetLengthValue(aIndex, mContent);
  }
  const mozilla::dom::SVGFilterElement* GetFilterContent(nsIContent* aDefault);
  const mozilla::dom::SVGFilterElement* GetFilterContent() {
    return GetFilterContent(mContent);
  }

  // This flag is used to detect loops in xlink:href processing
  bool mLoopFlag;
  bool mNoHRefURI;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGFILTERFRAME_H_
