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

class nsInlineState;

// Inline container class. Does not support being used as a pseudo frame
class nsInlineFrame : public nsHTMLContainerFrame
{
public:
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  NS_IMETHOD  ResizeReflow(nsIPresContext*  aPresContext,
                           nsReflowMetrics& aDesiredSize,
                           const nsSize&    aMaxSize,
                           nsSize*          aMaxElementSize,
                           ReflowStatus&    aStatus);

  NS_IMETHOD  IncrementalReflow(nsIPresContext*  aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize&    aMaxSize,
                                nsReflowCommand& aReflowCommand,
                                ReflowStatus&    aStatus);

  NS_IMETHOD  GetReflowMetrics(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aMetrics);

protected:
  nsInlineFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsInlineFrame();

  virtual PRIntn GetSkipSides() const;

  void InitializeState(nsIPresContext* aPresContext,
                       nsInlineState& aState);

  void ComputeFinalSize(nsIPresContext* aPresContext,
                        nsInlineState&   aState,
                        nsReflowMetrics& aSize);

  PRBool ReflowMappedChildren(nsIPresContext* aPresContext,
                              nsInlineState&  aState);

  PRBool PullUpChildren(nsIPresContext* aPresContext,
                        nsInlineState&  aState);

  ReflowStatus ReflowUnmappedChildren(nsIPresContext* aPresContext,
                                      nsInlineState&  aState);

  void PlaceChild(nsIFrame*              aChild,
                  PRInt32                aIndex,  // in the child frame list
                  nsInlineState&         aState,
                  const nsReflowMetrics& aChildSize,
                  const nsSize*          aChildMaxElementSize);

  PRIntn RecoverState(nsIPresContext* aCX, nsInlineState& aState,
                      nsIFrame* aSkipChild);

  ReflowStatus AdjustChildren(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              nsInlineState& aState,
                              nsIFrame* aKid,
                              nsReflowMetrics& aKidMetrics,
                              ReflowStatus aKidReflowStatus);
};

#endif /* nsInlineFrame_h___ */
