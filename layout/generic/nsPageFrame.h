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
#ifndef nsPageFrame_h___
#define nsPageFrame_h___

#include "nsHTMLContainerFrame.h"

// Pseudo frame created by the root content frame
class PageFrame : public nsContainerFrame {
public:
  PageFrame(nsIContent* aContent, nsIFrame* aParent);

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

  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aCX,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  NS_IMETHOD  Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect);

  // Debugging
  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  void CreateFirstChild(nsIPresContext* aPresContext);
};

#endif /* nsPageFrame_h___ */

