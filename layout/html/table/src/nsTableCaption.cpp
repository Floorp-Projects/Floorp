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
#include "nsTableCaption.h"
#include "nsTableCaptionFrame.h"
#include "nsBodyFrame.h"
#include "nsTablePart.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsContainerFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"


#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif


nsTableCaption::nsTableCaption(nsIAtom* aTag)
  : nsTableContent(aTag)
{
  mImplicit = PR_FALSE;
}

nsTableCaption::nsTableCaption(PRBool aImplicit)
  : nsTableContent(NS_NewAtom(nsTablePart::kCaptionTagString))
{
  mImplicit = aImplicit;
}

nsTableCaption::~nsTableCaption()
{
}

nsrefcnt nsTableCaption::AddRef(void)
{
  if (gsNoisyRefs) printf("Add Ref: %x, nsTableCaption cnt = %d \n",this, mRefCnt+1);
  return ++mRefCnt;
}

nsrefcnt nsTableCaption::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Release: %x, nsTableCaption cnt = %d \n",this, mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE) printf("Delete: %x, nsTableCaption \n",this);
    delete this;
    return 0;
  }
  return mRefCnt;
}

int nsTableCaption::GetType()
{
  return nsITableContent::kTableCaptionType;
}

void nsTableCaption::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  nsHTMLValue val;

  if (aAttribute == nsHTMLAtoms::align) {
    if (ParseTableCaptionAlignParam(aValue, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
    return;
  }
}

void nsTableCaption::MapAttributesInto(nsIStyleContext* aContext,
                                    nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  nsHTMLValue value;

  // align
  GetAttribute(nsHTMLAtoms::align, value);
  if (value.GetUnit() != eHTMLUnit_Null) {
    NS_ASSERTION(value.GetUnit() == eHTMLUnit_Enumerated, "unexpected unit");

    PRUint8 alignValue = value.GetIntValue();
    // this code relies on top, bottom, left, and right all being unique values
    if (NS_STYLE_VERTICAL_ALIGN_BOTTOM==alignValue || 
             NS_STYLE_VERTICAL_ALIGN_TOP==alignValue)
    {
      nsStyleText* textStyle =
        (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mVerticalAlign.SetIntValue(alignValue, eStyleUnit_Enumerated);
    }
    // XXX: this isn't the right place to put this attribute!  probably on the float guy
    // talk to Peter and Troy
    else if (NS_STYLE_TEXT_ALIGN_LEFT==alignValue || 
             NS_STYLE_TEXT_ALIGN_RIGHT==alignValue)
    {
      nsStyleText* textStyle =
        (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mTextAlign = alignValue;
    }
  }
}


nsresult
nsTableCaption::CreateFrame(nsIPresContext* aPresContext,
                            nsIFrame* aParentFrame,
                            nsIStyleContext* aStyleContext,
                            nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableCaptionFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return rv;
}


nsresult
NS_NewTableCaptionPart(nsIHTMLContent** aInstancePtrResult,
                       nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new nsTableCaption(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
