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
#include "nsHTMLParts.h"
#include "nsHTMLTagContent.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"

// this file is obsolete and will be removed

static NS_DEFINE_IID(kStyleBorderSID, NS_STYLEBORDER_SID);

enum FormElementType {
  eFormElementType_Submit,
  eFormElementType_Reset,
  eFormElementType_Text,
  eFormElementType_Password,
  eFormElementType_TextArea,
};

class FormElementFrame : public nsLeafFrame {
public:
  FormElementFrame(nsIContent* aContent,
             PRInt32 aIndexInParent,
             nsIFrame* aParentFrame);

  virtual void Paint(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

protected:
  virtual ~FormElementFrame();
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize& aMaxSize);
};

class FormElementContent : public nsHTMLTagContent {
public:
  FormElementContent(nsIAtom* aTag, FormElementType aType);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

protected:
  virtual ~FormElementContent();

  FormElementType mType;
};

//----------------------------------------------------------------------

FormElementFrame::FormElementFrame(nsIContent* aContent,
                                   PRInt32 aIndexInParent,
                                   nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aIndexInParent, aParentFrame)
{
}

FormElementFrame::~FormElementFrame()
{
}

// XXX it would be cool if form element used our rendering sw, then
// they could be blended, and bordered, and so on...
void FormElementFrame::Paint(nsIPresContext& aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect)
{
  nsStyleBorder* myBorder =
    (nsStyleBorder*)mStyleContext->GetData(kStyleBorderSID);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, mRect, *myBorder, 0);
}

void FormElementFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                      nsReflowMetrics& aDesiredSize,
                                      const nsSize& aMaxSize)
{
  float p2t = aPresContext->GetPixelsToTwips();
  aDesiredSize.width = nscoord(75 * p2t);
  aDesiredSize.height = nscoord(37 * p2t);
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

//----------------------------------------------------------------------

FormElementContent::FormElementContent(nsIAtom* aTag, FormElementType aType)
  : nsHTMLTagContent(aTag),
    mType(aType)
{
}

FormElementContent::~FormElementContent()
{
}

nsIFrame* FormElementContent::CreateFrame(nsIPresContext* aPresContext,
                                          PRInt32 aIndexInParent,
                                          nsIFrame* aParentFrame)
{
  nsIFrame* rv = new FormElementFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

NS_HTML nsresult
NS_NewHTMLSubmitButton(nsIHTMLContent** aInstancePtrResult,
                       nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new FormElementContent(aTag, eFormElementType_Submit);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

NS_HTML nsresult
NS_NewHTMLResetButton(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new FormElementContent(aTag, eFormElementType_Reset);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}
