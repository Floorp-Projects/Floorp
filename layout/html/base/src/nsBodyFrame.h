/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsBodyFrame_h___
#define nsBodyFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIAnchoredItems.h"
#include "nsIAbsoluteItems.h"
#include "nsVoidArray.h"

class nsSpaceManager;

struct nsStyleDisplay;
struct nsStylePosition;

class nsBodyFrame : public nsHTMLContainerFrame,
                    public nsIAnchoredItems,
                    public nsIAbsoluteItems
{
public:
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD ContentAppended(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer);

  NS_IMETHOD ContentInserted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent);

  NS_IMETHOD  ContentDeleted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent);

  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  // nsIAnchoredItems
  virtual void AddAnchoredItem(nsIFrame*         aAnchoredItem,
                               AnchoringPosition aPosition,
                               nsIFrame*         aContainer);
  virtual void RemoveAnchoredItem(nsIFrame* aAnchoredItem);

  // nsIAbsoluteItems
  NS_IMETHOD  AddAbsoluteItem(nsAbsoluteFrame* aAnchorFrame);
  NS_IMETHOD  RemoveAbsoluteItem(nsAbsoluteFrame* aAnchorFrame);

  NS_IMETHOD VerifyTree() const;

  // nsContainerFrame
  virtual void PropagateContentOffsets(nsIFrame* aChild,
                                       PRInt32 aFirstContentOffset,
                                       PRInt32 aLastContentOffset,
                                       PRBool aLastContentIsComplete);

protected:
  PRBool  mIsPseudoFrame;

  nsBodyFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  ~nsBodyFrame();

  void ComputeDesiredSize(const nsRect& aDesiredRect,
                          const nsSize& aMaxSize,
                          const nsMargin& aBorderPadding,
                          nsReflowMetrics& aDesiredSize);

  virtual PRIntn GetSkipSides() const;

  void ReflowAbsoluteItems(nsIPresContext*      aPresContext,
                           const nsReflowState& aReflowState);

  nsIView* CreateAbsoluteView(const nsStylePosition* aPosition,
                              const nsStyleDisplay*  aDisplay) const;

  void TranslatePoint(nsIFrame* aFrameFrom, nsPoint& aPoint) const;

  void ComputeAbsoluteFrameBounds(nsIFrame*              aAnchorFrame,
                                  const nsReflowState&   aState,
                                  const nsStylePosition* aPosition,
                                  nsRect&                aRect) const;

  void AddFrame(nsIFrame* aFrame);

private:
  nsSpaceManager* mSpaceManager;
  nsVoidArray     mAbsoluteItems;

  void CreateColumnFrame(nsIPresContext* aPresContext);
  nsSize GetColumnAvailSpace(nsIPresContext* aPresContext,
                             const nsMargin& aBorderPadding,
                             const nsSize&   aMaxSize);
};

#endif /* nsBodyFrame_h___ */
