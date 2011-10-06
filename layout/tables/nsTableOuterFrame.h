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
#include "nsTableFrame.h"

class nsTableCaptionFrame : public nsBlockFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsISupports
  virtual nsIAtom* GetType() const;
  friend nsIFrame* NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsStyleContext*  aContext);

  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, PRBool aShrinkWrap);

  virtual nsIFrame* GetParentStyleContextFrame();

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

protected:
  nsTableCaptionFrame(nsStyleContext*  aContext);
  virtual ~nsTableCaptionFrame();
};


/* TODO
1. decide if we'll allow subclassing.  If so, decide which methods really need to be virtual.
*/

/**
 * main frame for an nsTable content object, 
 * the nsTableOuterFrame contains 0 or one caption frame, and a nsTableFrame
 * pseudo-frame (referred to as the "inner frame').
 */
class nsTableOuterFrame : public nsHTMLContainerFrame, public nsITableLayout
{
public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsIFrame* NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
  
  // nsIFrame overrides - see there for a description

  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  
  virtual PRBool IsContainingBlock() const;

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);
 
  virtual nsFrameList GetChildList(ChildListID aListID) const;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const;

  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList);

  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);

  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame);

  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  nsresult BuildDisplayListForInnerTable(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);

  virtual nscoord GetBaseline() const;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, PRBool aShrinkWrap);

  /** process a reflow command for the table.
    * This involves reflowing the caption and the inner table.
    * @see nsIFrame::Reflow */
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableOuterFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  /** SetSelected needs to be overridden to talk to inner tableframe
   */
  void SetSelected(PRBool aSelected,
                   SelectionType aType);

  virtual nsIFrame* GetParentStyleContextFrame();

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

  NS_IMETHOD GetIndexByRowAndColumn(PRInt32 aRow, PRInt32 aColumn, PRInt32 *aIndex);
  NS_IMETHOD GetRowAndColumnByIndex(PRInt32 aIndex, PRInt32 *aRow, PRInt32 *aColumn);

protected:


  nsTableOuterFrame(nsStyleContext* aContext);
  virtual ~nsTableOuterFrame();

  void InitChildReflowState(nsPresContext&    aPresContext,                     
                            nsHTMLReflowState& aReflowState);

  /** Always returns 0, since the outer table frame has no border of its own
    * The inner table frame can answer this question in a meaningful way.
    * @see nsHTMLContainerFrame::GetSkipSides */
  virtual PRIntn GetSkipSides() const;

  PRUint8 GetCaptionSide(); // NS_STYLE_CAPTION_SIDE_* or NO_SIDE

  PRBool HasSideCaption() {
    PRUint8 captionSide = GetCaptionSide();
    return captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
           captionSide == NS_STYLE_CAPTION_SIDE_RIGHT;
  }
  
  PRUint8 GetCaptionVerticalAlign();

  void SetDesiredSize(PRUint8         aCaptionSide,
                      const nsMargin& aInnerMargin,
                      const nsMargin& aCaptionMargin,
                      nscoord&        aWidth,
                      nscoord&        aHeight);

  nsresult   GetCaptionOrigin(PRUint32         aCaptionSide,
                              const nsSize&    aContainBlockSize,
                              const nsSize&    aInnerSize, 
                              const nsMargin&  aInnerMargin,
                              const nsSize&    aCaptionSize,
                              nsMargin&        aCaptionMargin,
                              nsPoint&         aOrigin);

  nsresult   GetInnerOrigin(PRUint32         aCaptionSide,
                            const nsSize&    aContainBlockSize,
                            const nsSize&    aCaptionSize, 
                            const nsMargin&  aCaptionMargin,
                            const nsSize&    aInnerSize,
                            nsMargin&        aInnerMargin,
                            nsPoint&         aOrigin);
  
  // reflow the child (caption or innertable frame)
  void OuterBeginReflowChild(nsPresContext*           aPresContext,
                             nsIFrame*                aChildFrame,
                             const nsHTMLReflowState& aOuterRS,
                             void*                    aChildRSSpace,
                             nscoord                  aAvailWidth);

  nsresult OuterDoReflowChild(nsPresContext*           aPresContext,
                              nsIFrame*                aChildFrame,
                              const nsHTMLReflowState& aChildRS,
                              nsHTMLReflowMetrics&     aMetrics,
                              nsReflowStatus&          aStatus);

  // Set the reflow metrics
  void UpdateReflowMetrics(PRUint8              aCaptionSide,
                           nsHTMLReflowMetrics& aMet,
                           const nsMargin&      aInnerMargin,
                           const nsMargin&      aCaptionMargin);

  // Get the margin.  aMarginNoAuto is aMargin, but with auto 
  // margins set to 0
  void GetChildMargin(nsPresContext*           aPresContext,
                      const nsHTMLReflowState& aOuterRS,
                      nsIFrame*                aChildFrame,
                      nscoord                  aAvailableWidth,
                      nsMargin&                aMargin);

  nsTableFrame* InnerTableFrame() {
    return static_cast<nsTableFrame*>(mFrames.FirstChild());
  }
  
private:
  nsFrameList   mCaptionFrames;
};

inline PRIntn nsTableOuterFrame::GetSkipSides() const
{ return 0; }

#endif
