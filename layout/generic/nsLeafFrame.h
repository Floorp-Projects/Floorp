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
#ifndef nsLeafFrame_h___
#define nsLeafFrame_h___

#include "nsFrame.h"

/**
 * Abstract class that provides simple fixed-size layout for leaf objects
 * (e.g. images, form elements, etc.). Deriviations provide the implementation
 * of the GetDesiredSize method. The rendering method knows how to render
 * borders and backgrounds.
 */
class nsLeafFrame : public nsFrame {
public:
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD ContentChanged(nsIPresShell*   aShell,
                            nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);

protected:
  nsLeafFrame(nsIContent* aContent, nsIFrame* aParentFrame);
  virtual ~nsLeafFrame();

  /**
   * Return the desired size of the frame's content area. Note that this
   * method doesn't need to deal with padding or borders (the caller will
   * deal with it). In addition, the ascent will be set to the height
   * and the descent will be set to zero.
   */
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize) = 0;

  /**
   * Get the inner rect of this frame. This takes the outer size (mRect)
   * and subtracts off the borders and padding.
   */
  void GetInnerArea(nsIPresContext* aPresContext, nsRect& aInnerArea) const;

  /**
   * Subroutine to add in borders and padding
   */
  void AddBordersAndPadding(nsIPresContext* aPresContext,
                            nsReflowMetrics& aDesiredSize);
};

#endif /* nsLeafFrame_h___ */
