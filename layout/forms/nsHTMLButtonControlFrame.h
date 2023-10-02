/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLButtonControlFrame_h___
#define nsHTMLButtonControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsButtonFrameRenderer.h"

class gfxContext;
class nsPresContext;

class nsHTMLButtonControlFrame : public nsContainerFrame,
                                 public nsIFormControlFrame {
 public:
  explicit nsHTMLButtonControlFrame(ComputedStyle* aStyle,
                                    nsPresContext* aPresContext)
      : nsHTMLButtonControlFrame(aStyle, aPresContext, kClassID) {}

  ~nsHTMLButtonControlFrame();

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsHTMLButtonControlFrame)

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  nscoord GetMinISize(gfxContext* aRenderingContext) override;

  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  Maybe<nscoord> GetNaturalBaselineBOffset(
      mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup,
      BaselineExportContext aExportContext) const override;

  nsresult HandleEvent(nsPresContext* aPresContext,
                       mozilla::WidgetGUIEvent* aEvent,
                       nsEventStatus* aEventStatus) override;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  ComputedStyle* GetAdditionalComputedStyle(int32_t aIndex) const override;
  void SetAdditionalComputedStyle(int32_t aIndex,
                                  ComputedStyle* aComputedStyle) override;

#ifdef DEBUG
  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override;
  void RemoveFrame(DestroyContext&, ChildListID, nsIFrame*) override;
#endif

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"HTMLButtonControl"_ns, aResult);
  }
#endif

  // nsIFormControlFrame
  void SetFocus(bool aOn, bool aRepaint) override;
  nsresult SetFormProperty(nsAtom* aName, const nsAString& aValue) override;

  // Inserted child content gets its frames parented by our child block
  nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  // Return the ::-moz-button-content anonymous box.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

 protected:
  nsHTMLButtonControlFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                           nsIFrame::ClassID aID);

  virtual bool IsInput() { return false; }

  // Indicates whether we should clip our children's painting to our
  // border-box (either because of "overflow" or because of legacy reasons
  // about how <input>-flavored buttons work).
  bool ShouldClipPaintingToBorderBox();

  // Reflows the button's sole child frame, and computes the desired size
  // of the button itself from the results.
  void ReflowButtonContents(nsPresContext* aPresContext,
                            ReflowOutput& aButtonDesiredSize,
                            const ReflowInput& aButtonReflowInput,
                            nsIFrame* aFirstKid);

  BaselineSharingGroup GetDefaultBaselineSharingGroup() const override;
  nscoord SynthesizeFallbackBaseline(
      mozilla::WritingMode aWM,
      BaselineSharingGroup aBaselineGroup) const override;

  nsButtonFrameRenderer mRenderer;
};

#endif
