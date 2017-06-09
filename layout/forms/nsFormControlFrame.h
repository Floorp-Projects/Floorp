/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFormControlFrame_h___
#define nsFormControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsIFormControlFrame.h"
#include "nsAtomicContainerFrame.h"
#include "nsDisplayList.h"

/**
 * nsFormControlFrame is the base class for radio buttons and
 * checkboxes.  It also has two static methods (RegUnRegAccessKey and
 * GetScreenHeight) that are used by other form controls.
 */
class nsFormControlFrame : public nsAtomicContainerFrame,
                           public nsIFormControlFrame
{
public:
  /**
    * Main constructor
    * @param aContent the content representing this frame
    * @param aParentFrame the parent frame
    */
  nsFormControlFrame(nsStyleContext*, nsIFrame::ClassID);

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsAtomicContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  NS_DECL_QUERYFRAME
  NS_DECL_ABSTRACT_FRAME(nsFormControlFrame)

  // nsIFrame replacements
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override {
    DO_GLOBAL_REFLOW_COUNT_DSP("nsFormControlFrame");
    DisplayBorderBackgroundOutline(aBuilder, aLists);
  }

  /**
   * Both GetMinISize and GetPrefISize will return whatever GetIntrinsicISize
   * returns.
   */
  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext *aRenderingContext) override;

  /**
   * Our auto size is just intrinsic width and intrinsic height.
   */
  virtual mozilla::LogicalSize
  ComputeAutoSize(gfxContext*                 aRenderingContext,
                  mozilla::WritingMode        aWM,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord                     aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  ComputeSizeFlags            aFlags) override;

  /**
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode)
    const override;

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  virtual void Reflow(nsPresContext*      aCX,
                      ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&      aStatus) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  // new behavior

  virtual void SetFocus(bool aOn = true, bool aRepaint = false) override;

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) override;

  // AccessKey Helper function
  static nsresult RegUnRegAccessKey(nsIFrame * aFrame, bool aDoReg);

  /**
   * Returns the usable screen rect in app units, eg the rect where we can
   * draw dropdowns.
   */
  static nsRect GetUsableScreenRect(nsPresContext* aPresContext);

protected:

  virtual ~nsFormControlFrame();

  static nscoord DefaultSize()
  {
    // XXXmats We have traditionally always returned 9px for GetMin/PrefISize
    // but we might want to factor in what the theme says, something like:
    // GetMinimumWidgetSize - GetWidgetPadding - GetWidgetBorder.
    return nsPresContext::CSSPixelsToAppUnits(9);
  }

  /**
   * Get the state of the checked attribute.
   * @param aState set to true if the checked attribute is set,
   * false if the checked attribute has been removed
   */
  void GetCurrentCheckState(bool* aState);
};

#endif

