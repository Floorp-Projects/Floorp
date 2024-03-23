/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMeterFrame_h___
#define nsMeterFrame_h___

#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

class nsMeterFrame final : public nsContainerFrame,
                           public nsIAnonymousContentCreator

{
  using Element = mozilla::dom::Element;

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMeterFrame)

  explicit nsMeterFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
  virtual ~nsMeterFrame();

  void Destroy(DestroyContext&) override;
  void Reflow(nsPresContext*, ReflowOutput&, const ReflowInput&,
              nsReflowStatus&) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"Meter"_ns, aResult);
  }
#endif

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  /**
   * Returns whether the frame and its child should use the native style.
   */
  bool ShouldUseNativeStyle() const;

 protected:
  // Helper function which reflow the anonymous div frame.
  void ReflowBarFrame(nsIFrame* aBarFrame, nsPresContext* aPresContext,
                      const ReflowInput& aReflowInput,
                      const mozilla::LogicalSize& aParentContentBoxSize,
                      nsReflowStatus& aStatus);
  /**
   * The div used to show the meter bar.
   * @see nsMeterFrame::CreateAnonymousContent
   */
  nsCOMPtr<Element> mBarDiv;
};

#endif
