/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmtableFrame_h___
#define nsMathMLmtableFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsTableWrapperFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableCellFrame.h"

//
// <mtable> -- table or matrix
//

class nsMathMLmtableWrapperFrame final
  : public nsTableWrapperFrame
  , public nsMathMLFrame
{
public:
  friend nsContainerFrame*
  NS_NewMathMLmtableOuterFrame(nsIPresShell*   aPresShell,
                               nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // overloaded nsTableWrapperFrame methods

  virtual void
  Reflow(nsPresContext*           aPresContext,
         ReflowOutput&     aDesiredSize,
         const ReflowInput& aReflowInput,
         nsReflowStatus&          aStatus) override;

  virtual nsresult
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsTableWrapperFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

protected:
  explicit nsMathMLmtableWrapperFrame(nsStyleContext* aContext)
    : nsTableWrapperFrame(aContext)
  {}

  virtual ~nsMathMLmtableWrapperFrame();

  // helper to find the row frame at a given index, positive or negative, e.g.,
  // 1..n means the first row down to the last row, -1..-n means the last row
  // up to the first row. Used for alignments that are relative to a given row
  nsIFrame*
  GetRowFrameAt(int32_t aRowIndex);
}; // class nsMathMLmtableWrapperFrame

// --------------

class nsMathMLmtableFrame final : public nsTableFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsMathMLmtableFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame*
  NS_NewMathMLmtableFrame(nsIPresShell*   aPresShell,
                          nsStyleContext* aContext);

  // Overloaded nsTableFrame methods

  virtual void
  SetInitialChildList(ChildListID  aListID,
                      nsFrameList& aChildList) override;

  virtual void
  AppendFrames(ChildListID  aListID,
               nsFrameList& aFrameList) override
  {
    nsTableFrame::AppendFrames(aListID, aFrameList);
    RestyleTable();
  }

  virtual void
  InsertFrames(ChildListID aListID,
               nsIFrame* aPrevFrame,
               nsFrameList& aFrameList) override
  {
    nsTableFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
    RestyleTable();
  }

  virtual void
  RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override
  {
    nsTableFrame::RemoveFrame(aListID, aOldFrame);
    RestyleTable();
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsTableFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  // helper to restyle and reflow the table when a row is changed -- since MathML
  // attributes are inter-dependent and row/colspan can affect the table, it is
  // safer (albeit grossly suboptimal) to just relayout the whole thing.
  void RestyleTable();

  /** helper to get the column spacing style value */
  nscoord GetColSpacing(int32_t aColIndex) override;

  /** Sums the combined cell spacing between the columns aStartColIndex to
   *  aEndColIndex.
   */
  nscoord GetColSpacing(int32_t aStartColIndex,
                        int32_t aEndColIndex) override;

  /** helper to get the row spacing style value */
  nscoord GetRowSpacing(int32_t aRowIndex) override;

  /** Sums the combined cell spacing between the rows aStartRowIndex to
   *  aEndRowIndex.
   */
  nscoord GetRowSpacing(int32_t aStartRowIndex,
                        int32_t aEndRowIndex) override;

  void SetColSpacingArray(const nsTArray<nscoord>& aColSpacing)
  {
    mColSpacing = aColSpacing;
  }

  void SetRowSpacingArray(const nsTArray<nscoord>& aRowSpacing)
  {
    mRowSpacing = aRowSpacing;
  }

  void SetFrameSpacing(nscoord aSpacingX, nscoord aSpacingY)
  {
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
  explicit nsMathMLmtableFrame(nsStyleContext* aContext)
    : nsTableFrame(aContext)
    , mFrameSpacingX(0)
    , mFrameSpacingY(0)
    , mUseCSSSpacing(false)
  {}

  virtual ~nsMathMLmtableFrame();

private:
  nsTArray<nscoord> mColSpacing;
  nsTArray<nscoord> mRowSpacing;
  nscoord mFrameSpacingX;
  nscoord mFrameSpacingY;
  bool mUseCSSSpacing;
}; // class nsMathMLmtableFrame

// --------------

class nsMathMLmtrFrame final : public nsTableRowFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame*
  NS_NewMathMLmtrFrame(nsIPresShell*   aPresShell,
                       nsStyleContext* aContext);

  // overloaded nsTableRowFrame methods

  virtual nsresult
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) override;

  virtual void
  AppendFrames(ChildListID  aListID,
               nsFrameList& aFrameList) override
  {
    nsTableRowFrame::AppendFrames(aListID, aFrameList);
    RestyleTable();
  }

  virtual void
  InsertFrames(ChildListID  aListID,
               nsIFrame*    aPrevFrame,
               nsFrameList& aFrameList) override
  {
    nsTableRowFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
    RestyleTable();
  }

  virtual void
  RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override
  {
    nsTableRowFrame::RemoveFrame(aListID, aOldFrame);
    RestyleTable();
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsTableRowFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  // helper to restyle and reflow the table -- @see nsMathMLmtableFrame.
  void RestyleTable()
  {
    nsTableFrame* tableFrame = GetTableFrame();
    if (tableFrame && tableFrame->IsFrameOfType(nsIFrame::eMathML)) {
      // relayout the table
      ((nsMathMLmtableFrame*)tableFrame)->RestyleTable();
    }
  }

protected:
  explicit nsMathMLmtrFrame(nsStyleContext* aContext)
    : nsTableRowFrame(aContext)
  {}

  virtual ~nsMathMLmtrFrame();
}; // class nsMathMLmtrFrame

// --------------

class nsMathMLmtdFrame : public nsTableCellFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame*
  NS_NewMathMLmtdFrame(nsIPresShell*   aPresShell,
                       nsStyleContext* aContext,
                       nsTableFrame*   aTableFrame);

  // overloaded nsTableCellFrame methods

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual nsresult
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) override;

  virtual uint8_t GetVerticalAlign() const override;
  virtual nsresult ProcessBorders(nsTableFrame*           aFrame,
                                  nsDisplayListBuilder*   aBuilder,
                                  const nsDisplayListSet& aLists) override;

  virtual int32_t GetRowSpan() override;
  virtual int32_t GetColSpan() override;
  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsTableCellFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  virtual LogicalMargin GetBorderWidth(WritingMode aWM) const override;

  virtual nsMargin GetBorderOverflow() override;

protected:
  nsMathMLmtdFrame(nsStyleContext* aContext, nsTableFrame* aTableFrame)
    : nsTableCellFrame(aContext, aTableFrame)
  {
  }

  virtual ~nsMathMLmtdFrame();
}; // class nsMathMLmtdFrame

// --------------

class nsMathMLmtdInnerFrame final
  : public nsBlockFrame
  , public nsMathMLFrame
{
public:
  friend nsContainerFrame*
  NS_NewMathMLmtdInnerFrame(nsIPresShell*   aPresShell,
                            nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // Overloaded nsIMathMLFrame methods

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(int32_t  aFirstIndex,
                                    int32_t  aLastIndex,
                                    uint32_t aFlagsValues,
                                    uint32_t aFlagsToUpdate) override
  {
    nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(this,
      aFirstIndex, aLastIndex, aFlagsValues, aFlagsToUpdate);
    return NS_OK;
  }

  virtual void
  Reflow(nsPresContext*           aPresContext,
         ReflowOutput&     aDesiredSize,
         const ReflowInput& aReflowInput,
         nsReflowStatus&          aStatus) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsBlockFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eMathML | nsIFrame::eExcludesIgnorableWhitespace));
  }

  virtual const nsStyleText* StyleTextForLineLayout() override;
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  bool
  IsMrowLike() override
  {
    return mFrames.FirstChild() !=
           mFrames.LastChild() ||
           !mFrames.FirstChild();
  }

protected:
  explicit nsMathMLmtdInnerFrame(nsStyleContext* aContext);
  virtual ~nsMathMLmtdInnerFrame();

  nsStyleText* mUniqueStyleText;

};  // class nsMathMLmtdInnerFrame

#endif /* nsMathMLmtableFrame_h___ */
