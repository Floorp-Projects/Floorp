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
#ifndef nsAbsoluteFrame_h___
#define nsAbsoluteFrame_h___

#include "nsFrame.h"

// Implementation of a frame that's used as a placeholder for an absolutely
// positioned frame
class AbsoluteFrame : public nsFrame
{
public:
  /**
   * Create a new absolutely positioned frame
   */
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  // nsIFrame overrides
  NS_IMETHOD  ChildCount(PRInt32& aChildCount) const;
  NS_IMETHOD  ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const;
  NS_IMETHOD  IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const;
  NS_IMETHOD  FirstChild(nsIFrame*& aFirstChild) const;
  NS_IMETHOD  NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const;
  NS_IMETHOD  PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const;
  NS_IMETHOD  LastChild(nsIFrame*& aLastChild) const;
  NS_IMETHOD  ResizeReflow(nsIPresContext*  aPresContext,
                           nsReflowMetrics& aDesiredSize,
                           const nsSize&    aMaxSize,
                           nsSize*          aMaxElementSize,
                           ReflowStatus&    aStatus);
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0) const;

protected:
  nsIFrame* mFrame;  // the actual absolutely positioned frame

  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  AbsoluteFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~AbsoluteFrame();

  nsIView*  CreateView(nsIView*         aContainingView,
                       const nsRect&    aRect,
                       nsStylePosition* aPosition);
  nsIFrame* GetContainingBlock();
  void      ComputeViewBounds(const nsRect&    aContainingInnerRect,
                              nsStylePosition* aPosition,
                              nsRect&          aRect);
};

#endif /* nsAbsoluteFrame_h___ */
