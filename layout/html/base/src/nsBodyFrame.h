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
  friend nsresult NS_NewBodyFrame(nsIContent* aContent, nsIFrame* aParent,
                                  nsIFrame*& aResult, PRUint32 aFlags);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext,
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);

  NS_IMETHOD GetCursorAndContentAt(nsIPresContext& aPresContext,
                         const nsPoint&  aPoint,
                         nsIFrame**      aFrame,
                         nsIContent**    aContent,
                         PRInt32&        aCursor);

  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);

  // nsIAnchoredItems
  virtual void AddAnchoredItem(nsIFrame*         aAnchoredItem,
                               AnchoringPosition aPosition,
                               nsIFrame*         aContainer);
  virtual void RemoveAnchoredItem(nsIFrame* aAnchoredItem);

  // nsIAbsoluteItems
  NS_IMETHOD  AddAbsoluteItem(nsAbsoluteFrame* aAnchorFrame);
  NS_IMETHOD  RemoveAbsoluteItem(nsAbsoluteFrame* aAnchorFrame);

  NS_IMETHOD VerifyTree() const;

protected:
  PRUint32  mFlags;

  void SetFlags(PRUint32 aFlags) {
    mFlags = aFlags;
  }

  nsBodyFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  ~nsBodyFrame();

  void ComputeDesiredSize(nsIPresContext& aPresContext,
                          const nsHTMLReflowState& aReflowState,
                          const nsRect& aDesiredRect,
                          const nsSize& aMaxSize,
                          const nsMargin& aBorderPadding,
                          nsHTMLReflowMetrics& aDesiredSize);

  virtual PRIntn GetSkipSides() const;

  void ReflowAbsoluteItems(nsIPresContext& aPresContext,
                           const nsHTMLReflowState& aReflowState);

  nsIView* CreateAbsoluteView(nsIStyleContext* aStyleContext) const;

  void TranslatePoint(nsIFrame* aFrameFrom, nsPoint& aPoint) const;

  void ComputeAbsoluteFrameBounds(nsIFrame*                aAnchorFrame,
                                  const nsHTMLReflowState& aState,
                                  const nsStylePosition*   aPosition,
                                  nsRect&                  aRect) const;

  void AddFrame(nsIFrame* aFrame);

private:
  nsSpaceManager* mSpaceManager;
  nsVoidArray     mAbsoluteItems;
  PRInt32         mChildCount;

  nsSize GetColumnAvailSpace(nsIPresContext& aPresContext,
                             const nsMargin& aBorderPadding,
                             const nsSize&   aMaxSize);
};

#endif /* nsBodyFrame_h___ */
