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

#include "nsHTMLContainer.h"
#include "nsLeafFrame.h"
#include "nsIWebWidget.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
#include "nsRepository.h"

class nsHTMLIFrameFrame : public nsLeafFrame {

public:

  nsHTMLIFrameFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  /**
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  /**
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext*      aCX,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);


protected:

  virtual ~nsHTMLIFrameFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);

  nsIWebWidget* mWebWidget;
};

class nsHTMLIFrame : public nsHTMLContainer {
public:
 
  virtual nsresult  CreateFrame(nsIPresContext*  aPresContext,
                                nsIFrame*        aParentFrame,
                                nsIStyleContext* aStyleContext,
                                nsIFrame*&       aResult);

#if 0
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue) const;

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
#endif

protected:
  nsHTMLIFrame(nsIAtom* aTag);
  virtual  ~nsHTMLIFrame();
  
  friend nsresult NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                                   nsIAtom* aTag);

};

// nsHTMLIFrameFrame

nsHTMLIFrameFrame::nsHTMLIFrameFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aParentFrame)
{
  mWebWidget = nsnull;
}

nsHTMLIFrameFrame::~nsHTMLIFrameFrame()
{
}

NS_IMETHODIMP
nsHTMLIFrameFrame::Paint(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect)
{
  //mWebWidget->Show();
  return NS_OK;
}


static NS_DEFINE_IID(kCWebWidget, NS_WEBWIDGET_CID);
static NS_DEFINE_IID(kIWebWidget, NS_IWEBWIDGET_IID);

NS_IMETHODIMP
nsHTMLIFrameFrame::Reflow(nsIPresContext*      aPresContext,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState,
                          nsReflowStatus&      aStatus)
{
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);

  if (nsnull == mWebWidget) {
    NSRepository::CreateInstance(kCWebWidget, nsnull, kIWebWidget, (void**)&mWebWidget);
    if (nsnull != mWebWidget) {
      nsRect rect(0, 0, aDesiredSize.width, aDesiredSize.height);
      nsIWidget* parentWin;
      GetWindow(parentWin);
      mWebWidget->Init(parentWin->GetNativeData(NS_NATIVE_WINDOW), rect);
      NS_IF_RELEASE(parentWin);
      //mWebWidget->Show();
    } 
    else {
      NS_ASSERTION(0, "could not instantiate web widget for sub document");
    }
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

void 
nsHTMLIFrameFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsReflowState& aReflowState,
                                  nsReflowMetrics& aDesiredSize)
{
  float p2t = aPresContext->GetPixelsToTwips();
  aDesiredSize.width  = (int) (p2t * 200);
  aDesiredSize.height = (int) (p2t * 200);
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

// nsHTMLIFrame

nsHTMLIFrame::nsHTMLIFrame(nsIAtom* aTag) : nsHTMLContainer(aTag)
{
}

nsHTMLIFrame::~nsHTMLIFrame()
{
}

nsresult
nsHTMLIFrame::CreateFrame(nsIPresContext*  aPresContext,
                          nsIFrame*        aParentFrame,
                          nsIStyleContext* aStyleContext,
                          nsIFrame*&       aResult)
{
  nsIFrame* frame = new nsHTMLIFrameFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

nsresult
NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsHTMLIFrame(aTag);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}
