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

// YY need to pass isMultiple before create called

//#include "nsFormControlFrame.h"
#include "nsLegendFrame.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsCSSRendering.h"
//#include "nsIDOMHTMLCollection.h"
#include "nsIContent.h"
//#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
//#include "nsIRadioButton.h"
//#include "nsWidgetsCID.h"
//#include "nsSize.h"
#include "nsHTMLAtoms.h"
//#include "nsIView.h"
//#include "nsIListWidget.h"
//#include "nsIComboBox.h"
//#include "nsIListBox.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsFont.h"

static NS_DEFINE_IID(kLegendFrameCID, NS_LEGEND_FRAME_CID);
static NS_DEFINE_IID(kIDOMHTMLLegendElementIID, NS_IDOMHTMLLEGENDELEMENT_IID);
 
nsresult
NS_NewLegendFrame(nsIFrame*& aResult)
{
  aResult = new nsLegendFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsLegendFrame::nsLegendFrame()
  : nsHTMLContainerFrame()
{
  mInline = PR_FALSE;
}

NS_IMETHODIMP
nsLegendFrame::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kLegendFrameCID)) {
    *aInstancePtrResult = (void*) ((nsLegendFrame*)this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtrResult);
}

NS_IMETHODIMP
nsLegendFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  PRUint8 flags = (mInline) ? NS_BLOCK_SHRINK_WRAP : 0;
  NS_NewAreaFrame(mFirstChild, flags);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext =
    aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::legendContentPseudo, mStyleContext);
  mFirstChild->Init(aPresContext, mContent, this, this, styleContext);
  NS_RELEASE(styleContext);                                           

  // Set the geometric and content parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetGeometricParent(mFirstChild);
    frame->SetContentParent(mFirstChild);
  }

  // Queue up the frames for the inline frame
  return mFirstChild->SetInitialChildList(aPresContext, nsnull, aChildList);
}

NS_IMETHODIMP
nsLegendFrame::Paint(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect,
                     nsFramePaintLayer aWhichLayer)
{
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

NS_IMETHODIMP 
nsLegendFrame::Reflow(nsIPresContext& aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus)
{
  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);

  // reflow the child
  nsHTMLReflowState reflowState(aPresContext, mFirstChild, aReflowState,
                                availSize);
  ReflowChild(mFirstChild, aPresContext, aDesiredSize, reflowState, aStatus);

  // get border and padding
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  // Place the child
  nsRect rect = nsRect(borderPadding.left, borderPadding.top, aDesiredSize.width, aDesiredSize.height);
  mFirstChild->SetRect(rect);

  // add in our border and padding to the size of the child
  aDesiredSize.width  += borderPadding.left + borderPadding.right;
  if (aReflowState.HaveFixedContentWidth() && (aReflowState.computedWidth > aDesiredSize.width)) {
    aDesiredSize.width = aReflowState.computedWidth;
  }

  aDesiredSize.height += borderPadding.top + borderPadding.bottom;
  if (aReflowState.HaveFixedContentHeight() && (aReflowState.computedHeight > aDesiredSize.height)) {
    aDesiredSize.height = aReflowState.computedHeight;
  }

  // adjust our max element size, if necessary
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
  }
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRInt32 nsLegendFrame::GetAlign()
{
  PRInt32 intValue = NS_STYLE_TEXT_ALIGN_CENTER;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::align, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        intValue = value.GetIntValue();
      }
    }
    NS_RELEASE(content);
  }
  return intValue;
}

PRIntn
nsLegendFrame::GetSkipSides() const
{
  return 0;
}

PRBool
nsLegendFrame::IsInline()
{
  return mInline;
}

NS_IMETHODIMP
nsLegendFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Legend", aResult);
}
