/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

  // nsIFrame replacements
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);
  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  virtual ~nsLeafFrame();

  /**
   * Return the desired size of the frame's content area. Note that this
   * method doesn't need to deal with padding or borders (the caller will
   * deal with it). In addition, the ascent will be set to the height
   * and the descent will be set to zero.
   */
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize) = 0;

  /**
   * Subroutine to add in borders and padding
   */
  void AddBordersAndPadding(nsIPresContext* aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aDesiredSize,
                            nsMargin& aBorderPadding);
};

#endif /* nsLeafFrame_h___ */
