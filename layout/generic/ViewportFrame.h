/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object that is the root of the frame tree, which contains
 * the document's scrollbars and contains fixed-positioned elements
 */

#ifndef mozilla_ViewportFrame_h
#define mozilla_ViewportFrame_h

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsPresContext;

namespace mozilla {

class nsDisplayWrapList;
class ServoRestyleState;

/**
 * ViewportFrame is the parent of a single child - the doc root frame or a
 * scroll frame containing the doc root frame. ViewportFrame stores this child
 * in its primary child list.
 */
class ViewportFrame : public nsContainerFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(ViewportFrame)

  explicit ViewportFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : ViewportFrame(aStyle, aPresContext, kClassID) {}

  virtual ~ViewportFrame() = default;  // useful for debugging

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

#ifdef DEBUG
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            const nsLineList::iterator* aPrevFrameLine,
                            nsFrameList& aFrameList) override;
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  nsDisplayWrapList* BuildDisplayListForTopLayer(nsDisplayListBuilder* aBuilder,
                                                 bool* aIsOpaque = nullptr);

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  bool ComputeCustomOverflow(mozilla::OverflowAreas&) override { return false; }

  /**
   * Adjust aReflowInput to account for scrollbars and pres shell
   * GetVisualViewportSizeSet and
   * GetContentDocumentFixedPositionMargins adjustments.
   * @return the rect to use as containing block rect
   */
  nsRect AdjustReflowInputAsContainingBlock(ReflowInput* aReflowInput) const;

  /**
   * Update our style (and recursively the styles of any anonymous boxes we
   * might own)
   */
  void UpdateStyle(ServoRestyleState& aStyleSet);

  /**
   * Return our single anonymous box child.
   */
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

  // Returns adjusted viewport size to reflect the positions that position:fixed
  // elements are attached.
  nsSize AdjustViewportSizeForFixedPosition(const nsRect& aViewportRect) const;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

 protected:
  ViewportFrame(ComputedStyle* aStyle, nsPresContext* aPresContext, ClassID aID)
      : nsContainerFrame(aStyle, aPresContext, aID), mView(nullptr) {}

  /**
   * Calculate how much room is available for fixed frames. That means
   * determining if the viewport is scrollable and whether the vertical and/or
   * horizontal scrollbars are visible.  Adjust the computed width/height and
   * available width for aReflowInput accordingly.
   * @return the current scroll position, or 0,0 if not scrollable
   */
  nsPoint AdjustReflowInputForScrollbars(ReflowInput* aReflowInput) const;

  nsView* GetViewInternal() const override { return mView; }
  void SetViewInternal(nsView* aView) override { mView = aView; }

 private:
  virtual mozilla::layout::FrameChildListID GetAbsoluteListID() const override {
    return kFixedList;
  }

  nsView* mView;
};

}  // namespace mozilla

#endif  // mozilla_ViewportFrame_h
