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
#ifndef nsCSSInlineFrame_h___
#define nsCSSInlineFrame_h___

#include "nsCSSContainerFrame.h"
#include "nsCSSInlineLayout.h"
#include "nsCSSLineLayout.h"

class nsCSSInlineFrame;

/**
 * Reflow state object for managing css inline layout. Most of the state
 * is managed by the nsCSSInlineLayout object.
 */
struct nsCSSInlineReflowState : public nsReflowState {
  nsCSSInlineReflowState(nsCSSLineLayout& aLineLayout,
                         nsCSSInlineFrame* aInlineFrame,
                         nsIStyleContext* aInlineSC,
                         const nsReflowState& aReflowState,
                         nsSize* aMaxElementSize);
  ~nsCSSInlineReflowState();

  nsIPresContext* mPresContext;
  nsCSSInlineLayout mInlineLayout;

  nsIFrame* mLastChild;         // last child we have reflowed (so far)

  PRInt32 mKidContentIndex;     // content index of last child reflowed

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
class nsCSSInlineFrame : public nsCSSContainerFrame,
                         public nsIInlineReflow
{
public:
  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aCX,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
#if XXX_not_yet
  NS_IMETHOD  Reflow(nsIPresContext*      aPresContext,
                     nsReflowMetrics&     aDesiredSize,
                     const nsReflowState& aReflowState,
                     nsReflowStatus&      aStatus);
#endif

  // nsIInlineReflow
  NS_IMETHOD FindTextRuns(nsCSSLineLayout& aLineLayout);
  NS_IMETHOD InlineReflow(nsCSSLineLayout&     aLineLayout,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState);

protected:
  nsCSSInlineFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsCSSInlineFrame();

  virtual PRIntn GetSkipSides() const;

  nsresult FrameAppendedReflow(nsCSSInlineReflowState& aState);

  nsresult ChildIncrementalReflow(nsCSSInlineReflowState& aState);

  nsresult ResizeReflow(nsCSSInlineReflowState& aState);

  void ComputeFinalSize(nsCSSInlineReflowState& aState,
                        nsReflowMetrics& aMetrics);

  nsInlineReflowStatus ReflowMapped(nsCSSInlineReflowState& aState);

  nsInlineReflowStatus ReflowUnmapped(nsCSSInlineReflowState& aState);

  nsInlineReflowStatus PullUpChildren(nsCSSInlineReflowState& aState);

  nsresult MaybeCreateNextInFlow(nsCSSInlineReflowState& aState,
                                 nsIFrame*               aFrame);

  void PushKids(nsCSSInlineReflowState& aState,
                nsIFrame* aPrevChild, nsIFrame* aPushedChild);

  friend nsresult NS_NewCSSInlineFrame(nsIFrame**  aInstancePtrResult,
                                       nsIContent* aContent,
                                       nsIFrame*   aParent);
};

extern nsresult NS_NewCSSInlineFrame(nsIFrame**  aInstancePtrResult,
                                     nsIContent* aContent,
                                     nsIFrame*   aParent);

#endif /* nsCSSInlineFrame_h___ */
