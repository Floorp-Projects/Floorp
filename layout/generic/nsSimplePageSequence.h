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
#ifndef nsSimplePageSequence_h___
#define nsSimplePageSequence_h___

#include "nsHTMLContainerFrame.h"
#include "nsIPageSequenceFrame.h"

// Simple page sequence frame class. Used when we're in paginated mode
class nsSimplePageSequenceFrame : public nsContainerFrame,
                                  public nsIPageSequenceFrame {
public:
  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrameReflow
  NS_IMETHOD  Reflow(nsIPresContext&      aPresContext,
                     nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aMaxSize,
                     nsReflowStatus&      aStatus);

  // nsIFrame
  NS_IMETHOD  Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer);

  // nsIPageSequenceFrame
  NS_IMETHOD  Print(nsIPresContext&         aPresContext,
                    const nsPrintOptions&   aPrintOptions,
                    nsIPrintStatusCallback* aStatusCallback);

  // Debugging
  NS_IMETHOD  GetFrameName(nsString& aResult) const;

protected:
  virtual void PaintChild(nsIPresContext&      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsIFrame*            aFrame,
                          nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD_(nsrefcnt) AddRef(void) {return nsContainerFrame::AddRef();}
  NS_IMETHOD_(nsrefcnt) Release(void) {return nsContainerFrame::Release();}
};

/* prototypes */
nsresult
NS_NewSimplePageSequenceFrame(nsIFrame*& aResult);

#endif /* nsSimplePageSequence_h___ */

