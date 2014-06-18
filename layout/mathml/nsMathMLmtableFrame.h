/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmtableFrame_h___
#define nsMathMLmtableFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsTableOuterFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableCellFrame.h"

//
// <mtable> -- table or matrix
//

class nsMathMLmtableOuterFrame : public nsTableOuterFrame,
                                 public nsMathMLFrame
{
public:
  friend nsContainerFrame* NS_NewMathMLmtableOuterFrame(nsIPresShell* aPresShell,
                                                        nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // overloaded nsTableOuterFrame methods

  virtual void
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual nsresult
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsTableOuterFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

protected:
  nsMathMLmtableOuterFrame(nsStyleContext* aContext) : nsTableOuterFrame(aContext) {}
  virtual ~nsMathMLmtableOuterFrame();

  // helper to find the row frame at a given index, positive or negative, e.g.,
  // 1..n means the first row down to the last row, -1..-n means the last row
  // up to the first row. Used for alignments that are relative to a given row
  nsIFrame*
  GetRowFrameAt(nsPresContext* aPresContext,
                int32_t         aRowIndex);
}; // class nsMathMLmtableOuterFrame

// --------------

class nsMathMLmtableFrame : public nsTableFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsMathMLmtableFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame* NS_NewMathMLmtableFrame(nsIPresShell* aPresShell,
                                                   nsStyleContext* aContext);

  // Overloaded nsTableFrame methods

  virtual void
  SetInitialChildList(ChildListID  aListID,
                      nsFrameList& aChildList) MOZ_OVERRIDE;

  virtual void
  AppendFrames(ChildListID  aListID,
               nsFrameList& aFrameList) MOZ_OVERRIDE
  {
    nsTableFrame::AppendFrames(aListID, aFrameList);
    RestyleTable();
  }

  virtual void
  InsertFrames(ChildListID aListID,
               nsIFrame* aPrevFrame,
               nsFrameList& aFrameList) MOZ_OVERRIDE
  {
    nsTableFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
    RestyleTable();
  }

  virtual void
  RemoveFrame(ChildListID aListID,
              nsIFrame* aOldFrame) MOZ_OVERRIDE
  {
    nsTableFrame::RemoveFrame(aListID, aOldFrame);
    RestyleTable();
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsTableFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  // helper to restyle and reflow the table when a row is changed -- since MathML
  // attributes are inter-dependent and row/colspan can affect the table, it is
  // safer (albeit grossly suboptimal) to just relayout the whole thing.
  void RestyleTable();

  /** helper to get the cell spacing X style value */
  nscoord GetCellSpacingX(int32_t aColIndex) MOZ_OVERRIDE;

  /** Sums the combined cell spacing between the columns aStartColIndex to
   *  aEndColIndex.
   */
  nscoord GetCellSpacingX(int32_t aStartColIndex,
                          int32_t aEndColIndex) MOZ_OVERRIDE;

  /** helper to get the cell spacing Y style value */
  nscoord GetCellSpacingY(int32_t aRowIndex) MOZ_OVERRIDE;

  /** Sums the combined cell spacing between the rows aStartRowIndex to
   *  aEndRowIndex.
   */
  nscoord GetCellSpacingY(int32_t aStartRowIndex,
                          int32_t aEndRowIndex) MOZ_OVERRIDE;

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

  bool GetUseCSSSpacing()
  {
    return mUseCSSSpacing;
  }

protected:
  nsMathMLmtableFrame(nsStyleContext* aContext) : nsTableFrame(aContext) {}
  virtual ~nsMathMLmtableFrame();

private:
  nsTArray<nscoord> mColSpacing;
  nsTArray<nscoord> mRowSpacing;
  nscoord mFrameSpacingX;
  nscoord mFrameSpacingY;
  bool mUseCSSSpacing;
}; // class nsMathMLmtableFrame

// --------------

class nsMathMLmtrFrame : public nsTableRowFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame* NS_NewMathMLmtrFrame(nsIPresShell* aPresShell,
                                                nsStyleContext* aContext);

  // overloaded nsTableRowFrame methods

  virtual nsresult
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) MOZ_OVERRIDE;

  virtual void
  AppendFrames(ChildListID  aListID,
               nsFrameList& aFrameList) MOZ_OVERRIDE
  {
    nsTableRowFrame::AppendFrames(aListID, aFrameList);
    RestyleTable();
  }

  virtual void
  InsertFrames(ChildListID aListID,
               nsIFrame* aPrevFrame,
               nsFrameList& aFrameList) MOZ_OVERRIDE
  {
    nsTableRowFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
    RestyleTable();
  }

  virtual void
  RemoveFrame(ChildListID aListID,
              nsIFrame* aOldFrame) MOZ_OVERRIDE
  {
    nsTableRowFrame::RemoveFrame(aListID, aOldFrame);
    RestyleTable();
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsTableRowFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  // helper to restyle and reflow the table -- @see nsMathMLmtableFrame.
  void RestyleTable()
  {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    if (tableFrame && tableFrame->IsFrameOfType(nsIFrame::eMathML)) {
      // relayout the table
      ((nsMathMLmtableFrame*)tableFrame)->RestyleTable();
    }
  }

protected:
  nsMathMLmtrFrame(nsStyleContext* aContext) : nsTableRowFrame(aContext) {}
  virtual ~nsMathMLmtrFrame();
}; // class nsMathMLmtrFrame

// --------------

class nsMathMLmtdFrame : public nsTableCellFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame* NS_NewMathMLmtdFrame(nsIPresShell* aPresShell,
                                                nsStyleContext* aContext);

  // overloaded nsTableCellFrame methods

  virtual nsresult
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) MOZ_OVERRIDE;

  virtual uint8_t GetVerticalAlign() const MOZ_OVERRIDE;
  virtual nsresult ProcessBorders(nsTableFrame* aFrame,
                                  nsDisplayListBuilder* aBuilder,
                                  const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual int32_t GetRowSpan() MOZ_OVERRIDE;
  virtual int32_t GetColSpan() MOZ_OVERRIDE;
  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsTableCellFrame::IsFrameOfType(aFlags & ~(nsIFrame::eMathML));
  }

  virtual nsMargin* GetBorderWidth(nsMargin& aBorder) const MOZ_OVERRIDE;

  virtual nsMargin GetBorderOverflow() MOZ_OVERRIDE;

protected:
  nsMathMLmtdFrame(nsStyleContext* aContext) : nsTableCellFrame(aContext) {}
  virtual ~nsMathMLmtdFrame();
}; // class nsMathMLmtdFrame

// --------------

class nsMathMLmtdInnerFrame : public nsBlockFrame,
                              public nsMathMLFrame {
public:
  friend nsContainerFrame* NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // Overloaded nsIMathMLFrame methods

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(int32_t         aFirstIndex,
                                    int32_t         aLastIndex,
                                    uint32_t        aFlagsValues,
                                    uint32_t        aFlagsToUpdate) MOZ_OVERRIDE
  {
    nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(this,
      aFirstIndex, aLastIndex, aFlagsValues, aFlagsToUpdate);
    return NS_OK;
  }

  virtual void
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsBlockFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eMathML | nsIFrame::eExcludesIgnorableWhitespace));
  }

  virtual const nsStyleText* StyleTextForLineLayout() MOZ_OVERRIDE;
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;

  bool
  IsMrowLike() MOZ_OVERRIDE {
    return mFrames.FirstChild() != mFrames.LastChild() ||
           !mFrames.FirstChild();
  }

protected:
  nsMathMLmtdInnerFrame(nsStyleContext* aContext);
  virtual ~nsMathMLmtdInnerFrame();

  nsStyleText* mUniqueStyleText;

};  // class nsMathMLmtdInnerFrame

#endif /* nsMathMLmtableFrame_h___ */
