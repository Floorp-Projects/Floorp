/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmtableFrame_h___
#define nsMathMLmtableFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsMathMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsTableWrapperFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableCellFrame.h"

namespace mozilla {
class nsDisplayListBuilder;
class nsDisplayListSet;
class PresShell;
}  // namespace mozilla

//
// <mtable> -- table or matrix
//

class nsMathMLmtableWrapperFrame final : public nsTableWrapperFrame,
                                         public nsMathMLFrame {
 public:
  friend nsContainerFrame* NS_NewMathMLmtableOuterFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmtableWrapperFrame)

  // overloaded nsTableWrapperFrame methods

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsTableWrapperFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

 protected:
  explicit nsMathMLmtableWrapperFrame(ComputedStyle* aStyle,
                                      nsPresContext* aPresContext)
      : nsTableWrapperFrame(aStyle, aPresContext, kClassID) {}

  virtual ~nsMathMLmtableWrapperFrame();

  // helper to find the row frame at a given index, positive or negative, e.g.,
  // 1..n means the first row down to the last row, -1..-n means the last row
  // up to the first row. Used for alignments that are relative to a given row
  nsIFrame* GetRowFrameAt(int32_t aRowIndex);
};  // class nsMathMLmtableWrapperFrame

// --------------

class nsMathMLmtableFrame final : public nsTableFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmtableFrame)

  friend nsContainerFrame* NS_NewMathMLmtableFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  // Overloaded nsTableFrame methods

  void SetInitialChildList(ChildListID aListID,
                           nsFrameList&& aChildList) override;

  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override {
    nsTableFrame::AppendFrames(aListID, std::move(aFrameList));
    RestyleTable();
  }

  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override {
    nsTableFrame::InsertFrames(aListID, aPrevFrame, aPrevFrameLine,
                               std::move(aFrameList));
    RestyleTable();
  }

  void RemoveFrame(DestroyContext& aContext, ChildListID aListID,
                   nsIFrame* aOldFrame) override {
    nsTableFrame::RemoveFrame(aContext, aListID, aOldFrame);
    RestyleTable();
  }

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsTableFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  // helper to restyle and reflow the table when a row is changed -- since
  // MathML attributes are inter-dependent and row/colspan can affect the table,
  // it is safer (albeit grossly suboptimal) to just relayout the whole thing.
  void RestyleTable();

  /** helper to get the column spacing style value */
  nscoord GetColSpacing(int32_t aColIndex) override;

  /** Sums the combined cell spacing between the columns aStartColIndex to
   *  aEndColIndex.
   */
  nscoord GetColSpacing(int32_t aStartColIndex, int32_t aEndColIndex) override;

  /** helper to get the row spacing style value */
  nscoord GetRowSpacing(int32_t aRowIndex) override;

  /** Sums the combined cell spacing between the rows aStartRowIndex to
   *  aEndRowIndex.
   */
  nscoord GetRowSpacing(int32_t aStartRowIndex, int32_t aEndRowIndex) override;

  void SetColSpacingArray(const nsTArray<nscoord>& aColSpacing) {
    mColSpacing = aColSpacing.Clone();
  }

  void SetRowSpacingArray(const nsTArray<nscoord>& aRowSpacing) {
    mRowSpacing = aRowSpacing.Clone();
  }

  void SetFrameSpacing(nscoord aSpacingX, nscoord aSpacingY) {
    mFrameSpacingX = aSpacingX;
    mFrameSpacingY = aSpacingY;
  }

  /** Determines whether the placement of table cells is determined by CSS
   * spacing based on padding and border-spacing, or one based upon the
   * rowspacing, columnspacing and framespacing attributes.  The second
   * approach is used if the user specifies at least one of those attributes.
   */
  void SetUseCSSSpacing();
  bool GetUseCSSSpacing() { return mUseCSSSpacing; }

 protected:
  explicit nsMathMLmtableFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext)
      : nsTableFrame(aStyle, aPresContext, kClassID),
        mFrameSpacingX(0),
        mFrameSpacingY(0),
        mUseCSSSpacing(false) {}

  virtual ~nsMathMLmtableFrame();

 private:
  nsTArray<nscoord> mColSpacing;
  nsTArray<nscoord> mRowSpacing;
  nscoord mFrameSpacingX;
  nscoord mFrameSpacingY;
  bool mUseCSSSpacing;
};  // class nsMathMLmtableFrame

// --------------

class nsMathMLmtrFrame final : public nsTableRowFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmtrFrame)

  friend nsContainerFrame* NS_NewMathMLmtrFrame(mozilla::PresShell* aPresShell,
                                                ComputedStyle* aStyle);

  // overloaded nsTableRowFrame methods

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override {
    nsTableRowFrame::AppendFrames(aListID, std::move(aFrameList));
    RestyleTable();
  }

  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override {
    nsTableRowFrame::InsertFrames(aListID, aPrevFrame, aPrevFrameLine,
                                  std::move(aFrameList));
    RestyleTable();
  }

  void RemoveFrame(DestroyContext& aContext, ChildListID aListID,
                   nsIFrame* aOldFrame) override {
    nsTableRowFrame::RemoveFrame(aContext, aListID, aOldFrame);
    RestyleTable();
  }

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsTableRowFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  // helper to restyle and reflow the table -- @see nsMathMLmtableFrame.
  void RestyleTable() {
    nsTableFrame* tableFrame = GetTableFrame();
    if (tableFrame && tableFrame->IsFrameOfType(nsIFrame::eMathML)) {
      // relayout the table
      ((nsMathMLmtableFrame*)tableFrame)->RestyleTable();
    }
  }

 protected:
  explicit nsMathMLmtrFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsTableRowFrame(aStyle, aPresContext, kClassID) {}

  virtual ~nsMathMLmtrFrame();
};  // class nsMathMLmtrFrame

// --------------

class nsMathMLmtdFrame final : public nsTableCellFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmtdFrame)

  friend nsContainerFrame* NS_NewMathMLmtdFrame(mozilla::PresShell* aPresShell,
                                                ComputedStyle* aStyle,
                                                nsTableFrame* aTableFrame);

  // overloaded nsTableCellFrame methods

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  mozilla::StyleVerticalAlignKeyword GetVerticalAlign() const override;
  void ProcessBorders(nsTableFrame* aFrame,
                      mozilla::nsDisplayListBuilder* aBuilder,
                      const mozilla::nsDisplayListSet& aLists) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsTableCellFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  LogicalMargin GetBorderWidth(WritingMode aWM) const override;

  nsMargin GetBorderOverflow() override;

 protected:
  nsMathMLmtdFrame(ComputedStyle* aStyle, nsTableFrame* aTableFrame)
      : nsTableCellFrame(aStyle, aTableFrame, kClassID) {}

  virtual ~nsMathMLmtdFrame();
};  // class nsMathMLmtdFrame

// --------------

class nsMathMLmtdInnerFrame final : public nsBlockFrame, public nsMathMLFrame {
 public:
  friend nsContainerFrame* NS_NewMathMLmtdInnerFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmtdInnerFrame)

  // Overloaded nsIMathMLFrame methods

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(int32_t aFirstIndex, int32_t aLastIndex,
                                    uint32_t aFlagsValues,
                                    uint32_t aFlagsToUpdate) override {
    nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(
        this, aFirstIndex, aLastIndex, aFlagsValues, aFlagsToUpdate);
    return NS_OK;
  }

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsBlockFrame::IsFrameOfType(aFlags & ~nsIFrame::eMathML);
  }

  const nsStyleText* StyleTextForLineLayout() override;
  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

  bool IsMrowLike() override {
    return mFrames.FirstChild() != mFrames.LastChild() || !mFrames.FirstChild();
  }

 protected:
  explicit nsMathMLmtdInnerFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext);
  virtual ~nsMathMLmtdInnerFrame() = default;

  mozilla::UniquePtr<nsStyleText> mUniqueStyleText;

};  // class nsMathMLmtdInnerFrame

#endif /* nsMathMLmtableFrame_h___ */
