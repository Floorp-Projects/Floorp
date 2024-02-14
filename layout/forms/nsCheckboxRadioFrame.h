/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCheckboxRadioFrame_h___
#define nsCheckboxRadioFrame_h___

#include "mozilla/Attributes.h"
#include "nsIFormControlFrame.h"
#include "nsAtomicContainerFrame.h"
#include "nsDisplayList.h"

/**
 * nsCheckboxRadioFrame is used for radio buttons and checkboxes.
 */
class nsCheckboxRadioFrame final : public nsAtomicContainerFrame,
                                   public nsIFormControlFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsCheckboxRadioFrame)

  explicit nsCheckboxRadioFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext);

  // nsIFrame replacements
  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  /**
   * Both GetMinISize and GetPrefISize will return whatever GetIntrinsicISize
   * returns.
   */
  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  /**
   * Our auto size is just intrinsic width and intrinsic height.
   */
  mozilla::LogicalSize ComputeAutoSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  /**
   * Respond to a gui event
   * @see nsIFrame::HandleEvent
   */
  nsresult HandleEvent(nsPresContext* aPresContext,
                       mozilla::WidgetGUIEvent* aEvent,
                       nsEventStatus* aEventStatus) override;

  Maybe<nscoord> GetNaturalBaselineBOffset(
      mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup,
      BaselineExportContext) const override;

  /**
   * Respond to the request to resize and/or reflow
   * @see nsIFrame::Reflow
   */
  void Reflow(nsPresContext* aCX, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  // new behavior

  void SetFocus(bool aOn = true, bool aRepaint = false) override;

  // nsIFormControlFrame
  nsresult SetFormProperty(nsAtom* aName, const nsAString& aValue) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"CheckboxRadio"_ns, aResult);
  }
#endif

 protected:
  virtual ~nsCheckboxRadioFrame();

  nscoord DefaultSize();
};

#endif
