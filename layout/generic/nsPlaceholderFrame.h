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
#ifndef nsPlaceholderFrame_h___
#define nsPlaceholderFrame_h___

#include "nsFrame.h"

/**
 * Implementation of a frame that's used as a placeholder for a frame that
 * has been moved out of the flow
 */
class nsPlaceholderFrame : public nsFrame {
public:
  /**
   * Create a new placeholder frame
   */
  friend nsresult NS_NewPlaceholderFrame(nsIFrame**  aInstancePtrResult);

  // Get/Set the associated out of flow frame
  nsIFrame*   GetOutOfFlowFrame() const {return mOutOfFlowFrame;}
  void        SetOutOfFlowFrame(nsIFrame* aFrame) {mOutOfFlowFrame = aFrame;}

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  // nsIFrame overrides
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD  ContentChanged(nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::placeholderFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD  GetFrameName(nsString& aResult) const;

protected:
  nsIFrame* mOutOfFlowFrame;
};

#endif /* nsPlaceholderFrame_h___ */
