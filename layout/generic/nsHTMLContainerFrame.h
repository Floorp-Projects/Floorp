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
#ifndef nsHTMLContainerFrame_h___
#define nsHTMLContainerFrame_h___

#include "nsContainerFrame.h"
class nsString;
class nsAbsoluteFrame;
class nsPlaceholderFrame;
struct nsStyleDisplay;
struct nsStylePosition;

// Some macros for container classes to do sanity checking on
// width/height/x/y values computed during reflow.
#ifdef DEBUG
#define CRAZY_W 500000
#define CRAZY_H 500000
#define CRAZY_WIDTH(_x) (((_x) < -CRAZY_W) || ((_x) > CRAZY_W))
#define CRAZY_HEIGHT(_y) (((_y) < -CRAZY_H) || ((_y) > CRAZY_H))
#endif

// Base class for html container frames that provides common
// functionality.
class nsHTMLContainerFrame : public nsContainerFrame {
public:
  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

  /**
   * Helper method to create next-in-flows if necessary. If aFrame
   * already has a next-in-flow then this method does
   * nothing. Otherwise, a new continuation frame is created and
   * linked into the flow. In addition, the new frame becomes the
   * next-sibling of aFrame.
   */
  static nsresult CreateNextInFlow(nsIPresContext& aPresContext,
                                   nsIFrame* aOuterFrame,
                                   nsIFrame* aFrame,
                                   nsIFrame*& aNextInFlowResult);

  // Helper method to wrap views around frames. Used by containers
  // under special circumstances (can be used by leaf frames as well)
  static nsresult CreateViewForFrame(nsIPresContext& aPresContext,
                                     nsIFrame* aFrame,
                                     nsIStyleContext* aStyleContext,
                                     PRBool aForce);

  static void UpdateStyleContexts(nsIPresContext& aPresContext,
                                  nsIFrame* aFrame,
                                  nsIFrame* aOldParent,
                                  nsIFrame* aNewParent);

protected:
  virtual PRIntn GetSkipSides() const = 0;
};

#endif /* nsHTMLContainerFrame_h___ */

