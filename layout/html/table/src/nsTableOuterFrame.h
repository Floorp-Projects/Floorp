/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsTableOuterFrame_h__
#define nsTableOuterFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsITableLayout.h"

#ifdef DEBUG_TABLE_REFLOW_TIMING
class nsReflowTimer;
#endif

struct nsStyleTable;
class nsTableFrame;

class nsTableCaptionFrame : public nsBlockFrame
{
public:
  // nsISupports
  virtual nsIAtom* GetType() const;
  friend nsresult NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

protected:
  nsTableCaptionFrame();
  virtual ~nsTableCaptionFrame();
};


/* TODO
1. decide if we'll allow subclassing.  If so, decide which methods really need to be virtual.
*/

/**
 * main frame for an nsTable content object, 
 * the nsTableOuterFrame contains 0 or one caption frame, and a nsTableFrame
 * psuedo-frame (referred to as the "inner frame').
 */
class nsTableOuterFrame : public nsHTMLContainerFrame, public nsITableLayout
{
public:

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  /** instantiate a new instance of nsTableOuterFrame.
    * @param aPresShell the presentation shell
    * @param aResult    the new object is returned in this out-param
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
  
  // nsIFrame overrides - see there for a description

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  
  virtual PRBool IsContainingBlock() const;

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
 
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const;

  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;

  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);

  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);

  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstChild(nsnull)->GetContentInsertionFrame();
  }

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  // the outer table does not paint its entire background if it has margins and/or captions
  virtual PRBool CanPaintBackground() { return PR_FALSE; }

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  /** process a reflow command for the table.
    * This involves reflowing the caption and the inner table.
    * @see nsIFrame::Reflow */
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableOuterFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  /** SetSelected needs to be overridden to talk to inner tableframe
   */
  NS_IMETHOD SetSelected(nsIPresContext* aPresContext,
                         nsIDOMRange *aRange,
                         PRBool aSelected,
                         nsSpread aSpread);

  /** return the min width of the caption.  Return 0 if there is no caption. 
    * The return value is only meaningful after the caption has had a pass1 reflow.
    */
  nscoord GetMinCaptionWidth();

  NS_IMETHOD GetParentStyleContextFrame(nsIPresContext* aPresContext,
                                        nsIFrame**      aProviderFrame,
                                        PRBool*         aIsChild);

  /*---------------- nsITableLayout methods ------------------------*/

  /** @see nsITableFrame::GetCellDataAt */
  NS_IMETHOD GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex, 
                           nsIDOMElement* &aCell,   //out params
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                           PRInt32& aRowSpan, PRInt32& aColSpan,
                           PRInt32& aActualRowSpan, PRInt32& aActualColSpan,
                           PRBool& aIsSelected);

  /** @see nsITableFrame::GetTableSize */
  NS_IMETHOD GetTableSize(PRInt32& aRowCount, PRInt32& aColCount);

  static void ZeroAutoMargin(nsHTMLReflowState& aReflowState,
                             nsMargin&          aMargin);

  PRBool IsNested(const nsHTMLReflowState& aReflowState) const;

  static PRBool IsAutoWidth(nsIFrame& aTableOrCaption,
                            PRBool*   aIsPctWidth = nsnull);

protected:


  nsTableOuterFrame();
  virtual ~nsTableOuterFrame();

  void InitChildReflowState(nsIPresContext&    aPresContext,                     
                            nsHTMLReflowState& aReflowState);

  /** Always returns 0, since the outer table frame has no border of its own
    * The inner table frame can answer this question in a meaningful way.
    * @see nsHTMLContainerFrame::GetSkipSides */
  virtual PRIntn GetSkipSides() const;

  /** overridden here to handle special caption-table relationship
    * @see nsContainerFrame::VerifyTree
    */
  NS_IMETHOD VerifyTree() const;

  /**
   * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
   * pointers.
   *
   * Updates the child count and content offsets of all containers that are
   * affected
   *
   * Overloaded here because nsContainerFrame makes assumptions about pseudo-frames
   * that are not true for tables.
   *
   * @param   aChild child this child's next-in-flow
   * @return  PR_TRUE if successful and PR_FALSE otherwise
   */
  virtual void DeleteChildsNextInFlow(nsIPresContext* aPresContext, nsIFrame* aChild);

// begin Incremental Reflow methods

  /** process an incremental reflow command */
  NS_IMETHOD IncrementalReflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus);

  /** process an incremental reflow command targeted at a child of this frame. */
  NS_IMETHOD IR_TargetIsChild(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus,
                              nsIFrame *               aNextFrame);

  /** process an incremental reflow command targeted at the table inner frame. */
  NS_IMETHOD IR_TargetIsInnerTableFrame(nsIPresContext*          aPresContext,
                                        nsHTMLReflowMetrics&     aDesiredSize,
                                        const nsHTMLReflowState& aReflowState,
                                        nsReflowStatus&          aStatus);

  /** process an incremental reflow command targeted at the caption. */
  NS_IMETHOD IR_TargetIsCaptionFrame(nsIPresContext*          aPresContext,
                                     nsHTMLReflowMetrics&     aDesiredSize,
                                     const nsHTMLReflowState& aReflowState,
                                     nsReflowStatus&          aStatus);

  /** process an incremental reflow command targeted at this frame.
    * many incremental reflows that are targeted at this outer frame
    * are actually handled by the inner frame.  The logic to decide this
    * is here.
    */
  NS_IMETHOD IR_TargetIsMe(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus);

  /** pass along the incremental reflow command to the inner table. */
  NS_IMETHOD IR_InnerTableReflow(nsIPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus);

  /** handle incremental reflow notification that a caption was inserted. */
  NS_IMETHOD IR_CaptionInserted(nsIPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus);

  /** handle incremental reflow notification that we have dirty child frames */
  NS_IMETHOD IR_ReflowDirty(nsIPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus);

  /** handle incremental reflow notification that the caption style was changed
    * such that it is now left|right instead of top|bottom, or vice versa.
    */
  PRBool IR_CaptionChangedAxis(const nsStyleTable* aOldStyle, 
                               const nsStyleTable* aNewStyle) const;

  // end Incremental Reflow methods

  nscoord GetMaxElementWidth(PRUint8         aCaptionSide,
                             const nsMargin& aInnerMargin,
                             const nsMargin& aInnerPadding,
                             const nsMargin& aCaptionMargin);

  nscoord GetMaxWidth(PRUint8         aCaptionSide,
                      const nsMargin& aInnerMargin,
                      const nsMargin& aCaptionMargin);

  PRUint8 GetCaptionSide();
  
  PRUint8 GetCaptionVerticalAlign();

  void SetDesiredSize(nsIPresContext* aPresContext,
                      PRUint8         aCaptionSide,
                      const nsMargin& aInnerMargin,
                      const nsMargin& aCaptionMargin,
                      nscoord         aAvailWidth,
                      nscoord&        aWidth,
                      nscoord&        aHeight);

  void PctAdjustMinCaptionWidth(nsIPresContext*           aPresContext,
                                const nsHTMLReflowState&  aOuterRS,
                                PRUint8                   aCaptionSide,
                                nscoord&                  capMin);

  void BalanceLeftRightCaption(nsIPresContext* aPresContext,
                               PRUint8         aCaptionSide,
                               const nsMargin& aInnerMargin, 
                               const nsMargin& aCaptionMargin,
                               nscoord&        aInnerWidth,
                               nscoord&        aCaptionWidth);

  NS_IMETHOD GetCaptionOrigin(nsIPresContext*  aPresContext,
                              PRUint32         aCaptionSide,
                              const nsSize&    aContainBlockSize,
                              const nsSize&    aInnerSize, 
                              const nsMargin&  aInnerMargin,
                              const nsSize&    aCaptionSize,
                              nsMargin&        aCaptionMargin,
                              nsPoint&         aOrigin);

  NS_IMETHOD GetInnerOrigin(nsIPresContext*  aPresContext,
                            PRUint32         aCaptionSide,
                            const nsSize&    aContainBlockSize,
                            const nsSize&    aCaptionSize, 
                            const nsMargin&  aCaptionMargin,
                            const nsSize&    aInnerSize,
                            nsMargin&        aInnerMargin,
                            nsPoint&         aOrigin);
  // Get the available width for the caption, aInnerMarginNoAuto is aInnerMargin, but with 
  // auto margins set to 0
  nscoord GetCaptionAvailWidth(nsIPresContext*          aPresContext,
                               nsIFrame*                aCaptionFrame,
                               const nsHTMLReflowState& aReflowState,
                               nsMargin&                aCaptionMargin,
                               nsMargin&                aCaptionPad,
                               nscoord*                 aInnerWidth        = nsnull,
                               const nsMargin*          aInnerMarginNoAuto = nsnull,
                               const nsMargin*          aInnerMargin       = nsnull);

  nscoord GetInnerTableAvailWidth(nsIPresContext*          aPresContext,
                                  nsIFrame*                aInnerTable,
                                  const nsHTMLReflowState& aOuterRS,
                                  nscoord*                 aCaptionWidth,
                                  nsMargin&                aInnerMargin,
                                  nsMargin&                aInnerPadding);
  
  // reflow the child (caption or innertable frame),aMarginNoAuto is aMargin, 
  // but with auto margins set to 0 
  NS_IMETHOD OuterReflowChild(nsIPresContext*           aPresContext,
                              nsIFrame*                 aChildFrame,
                              const nsHTMLReflowState&  aOuterRS,
                              nsHTMLReflowMetrics&      aMetrics,
                              nscoord                   aAvailWidth,
                              nsSize&                   aDesiredSize,
                              nsMargin&                 aMargin,
                              nsMargin&                 aMarginNoAuto,
                              nsMargin&                 aPadding,
                              nsReflowReason            aReflowReason,
                              nsReflowStatus&           aStatus,
                              PRBool*                   aNeedToReflowCaption = nsnull);

  // Set the reflow metrics,  aInnerMarginNoAuto is  aInnerMargin, but with 
  // auto margins set to 0
  // aCaptionMargionNoAuto is aCaptionMargin, but with auto margins set to 0
  void UpdateReflowMetrics(nsIPresContext*      aPresContext,
                           PRUint8              aCaptionSide,
                           nsHTMLReflowMetrics& aMet,
                           const nsMargin&      aInnerMargin,
                           const nsMargin&      aInnerMarginNoAuto,
                           const nsMargin&      aInnerPadding,
                           const nsMargin&      aCaptionMargin,
                           const nsMargin&      aCaptionMargionNoAuto,
                           const nscoord        aAvailWidth);

  void InvalidateDamage(nsIPresContext* aPresContext,
                        PRUint8         aCaptionSide,
                        const nsSize&   aOuterSize,
                        PRBool          aInnerChanged,
                        PRBool          aCaptionChanged,
                        nsRect*         aOldOverflowArea);
  
  // Get the margin and padding, aMarginNoAuto is aMargin, but with auto 
  // margins set to 0
  void GetMarginPadding(nsIPresContext*          aPresContext,                     
                        const nsHTMLReflowState& aOuterRS,
                        nsIFrame*                aChildFrame,
                        nscoord                  aAvailableWidth,
                        nsMargin&                aMargin,
                        nsMargin&                aMarginNoAuto,
                        nsMargin&                aPadding);

private:
  // used to keep track of this frame's children. They are redundant with mFrames, but more convient
  nsTableFrame* mInnerTableFrame; 
  nsIFrame*     mCaptionFrame;

  // used to track caption max element size 
  PRInt32   mMinCaptionWidth;
  nscoord   mPriorAvailWidth;

#ifdef DEBUG_TABLE_REFLOW_TIMING
public:
  nsReflowTimer* mTimer;
#endif
};

inline nscoord nsTableOuterFrame::GetMinCaptionWidth()
{ return mMinCaptionWidth; }

inline PRIntn nsTableOuterFrame::GetSkipSides() const
{ return 0; }

#endif



