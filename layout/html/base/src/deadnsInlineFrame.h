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
#ifndef nsInlineFrame_h___
#define nsInlineFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsInlineLayout.h"
#include "nsLineLayout.h"

class nsInlineFrame;
class nsPlaceholderFrame;

/**
 * Reflow state object for managing css inline layout. Most of the state
 * is managed by the nsInlineLayout object.
 */
struct nsInlineReflowState : public nsReflowState {
  nsInlineReflowState(nsLineLayout& aLineLayout,
                      nsInlineFrame* aInlineFrame,
                      nsIStyleContext* aInlineSC,
                      const nsReflowState& aReflowState,
                      PRBool aComputeMaxElementSize);
  ~nsInlineReflowState();

  nsIPresContext* mPresContext;
  nsInlineLayout mInlineLayout;

  nsIFrame* mLastChild;         // last child we have reflowed (so far)

  nsMargin mBorderPadding;
  PRBool mNoWrap;
  PRIntn mStyleSizeFlags;
  nsSize mStyleSize;
};

//----------------------------------------------------------------------

/**
 * CSS "inline" layout class.
 *
 * Note: This class does not support being used as a pseudo frame.
 */
class nsInlineFrame : public nsHTMLContainerFrame,
                      public nsIInlineReflow
{
public:
  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aCX,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
#if XXX_not_yet
  NS_IMETHOD  Reflow(nsIPresContext&      aPresContext,
                     nsReflowMetrics&     aDesiredSize,
                     const nsReflowState& aReflowState,
                     nsReflowStatus&      aStatus);
#endif

  // nsIInlineReflow
  NS_IMETHOD FindTextRuns(nsLineLayout&  aLineLayout,
                          nsIReflowCommand* aReflowCommand);
  NS_IMETHOD InlineReflow(nsLineLayout&     aLineLayout,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState);

  // nsContainerFrame
  virtual PRBool DeleteNextInFlowsFor(nsIPresContext& aPresContext, nsIFrame* aChild);

protected:
  nsInlineFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsInlineFrame();

  virtual PRIntn GetSkipSides() const;

  nsresult InitialReflow(nsInlineReflowState& aState);

  nsresult FrameAppendedReflow(nsInlineReflowState& aState);

  nsresult ChildIncrementalReflow(nsInlineReflowState& aState);

  nsresult ResizeReflow(nsInlineReflowState& aState);

  void ComputeFinalSize(nsInlineReflowState& aState,
                        nsReflowMetrics& aMetrics);

  PRBool ReflowMapped(nsInlineReflowState& aState,
                      nsInlineReflowStatus&   aReflowStatus);

  PRBool PullUpChildren(nsInlineReflowState& aState,
                        nsInlineReflowStatus&   aReflowStatus);

  nsIFrame* PullOneChild(nsInlineFrame* aNextInFlow,
                         nsIFrame*         aLastChild);

  nsresult MaybeCreateNextInFlow(nsInlineReflowState& aState,
                                 nsIFrame*               aFrame);

  void PushKids(nsInlineReflowState& aState,
                nsIFrame* aPrevChild, nsIFrame* aPushedChild);

  void DrainOverflowLists();

  nsresult AppendNewFrames(nsIPresContext* aPresContext, nsIFrame*);

  void InsertNewFrame(nsIFrame* aNewFrame, nsIFrame* aPrevSibling);

  friend nsresult NS_NewInlineFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                                    nsIFrame*& aNewFrame);
};

#endif /* nsInlineFrame_h___ */
