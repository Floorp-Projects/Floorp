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
#include "nsFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"

class BRFrame : public nsFrame {
public:
  // nsIFrame
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

protected:
  virtual ~BRFrame();
};

nsresult
NS_NewBRFrame(nsIFrame*& aResult)
{
  nsIFrame* frame = new BRFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

BRFrame::~BRFrame()
{
}

NS_METHOD
BRFrame::Paint(nsIPresContext& aPresContext,
               nsIRenderingContext& aRenderingContext,
               const nsRect& aDirtyRect,
               nsFramePaintLayer aWhichLayer)
{
  if ((eFramePaintLayer_Overlay == aWhichLayer) &&
      nsIFrame::GetShowFrameBorders()) {
    float p2t = aPresContext.GetPixelsToTwips();
    nscoord five = NSIntPixelsToTwips(5, p2t);
    aRenderingContext.SetColor(NS_RGB(0, 255, 255));
    aRenderingContext.FillRect(0, 0, five, five*2);
  }
  return NS_OK;
}

NS_IMETHODIMP
BRFrame::Reflow(nsIPresContext& aPresContext,
                nsHTMLReflowMetrics& aMetrics,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus& aStatus)
{
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = 0;
    aMetrics.maxElementSize->height = 0;
  }
  aMetrics.height = 0;
  aMetrics.width = 0;
  aMetrics.ascent = 0;
  aMetrics.descent = 0;
  NS_ASSERTION(nsnull != aReflowState.lineLayout, "no line layout");
  aReflowState.lineLayout->SetBRFrame(this);

  // Return our reflow status
  const nsStyleDisplay* display = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  PRUint32 breakType = display->mBreakType;
  if (NS_STYLE_CLEAR_NONE == breakType) {
    breakType = NS_STYLE_CLEAR_LINE;
  }

  aStatus = NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
    NS_INLINE_MAKE_BREAK_TYPE(breakType);
  return NS_OK;
}
