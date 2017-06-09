/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
                                 public nsIFormControlFrame
{
public:
  explicit nsHTMLButtonControlFrame(nsStyleContext* aContext)
    : nsHTMLButtonControlFrame(aContext, kClassID)
  {}

  ~nsHTMLButtonControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsHTMLButtonControlFrame)

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;

  virtual nscoord GetPrefISize(gfxContext *aRenderingContext) override;

  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override;

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext, 
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual nsStyleContext* GetAdditionalStyleContext(int32_t aIndex) const override;
  virtual void SetAdditionalStyleContext(int32_t aIndex, 
                                         nsStyleContext* aStyleContext) override;
 
#ifdef DEBUG
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;
#endif

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("HTMLButtonControl"), aResult);
  }
#endif

  virtual bool HonorPrintBackgroundSettings() override { return false; }

  // nsIFormControlFrame
  void SetFocus(bool aOn, bool aRepaint) override;
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) override;

  // Inserted child content gets its frames parented by our child block
  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  /**
   * Update the style of our ::-moz-button-content anonymous box.
   */
  void DoUpdateStyleOfOwnedAnonBoxes(mozilla::ServoStyleSet& aStyleSet,
                                     nsStyleChangeList& aChangeList,
                                     nsChangeHint aHintForThisFrame) override;
protected:
  nsHTMLButtonControlFrame(nsStyleContext* aContext, nsIFrame::ClassID aID);

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

  nsButtonFrameRenderer mRenderer;
};

#endif




