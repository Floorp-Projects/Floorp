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
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"


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

nsIFrame* nsTableCaption::CreateFrame(nsIPresContext* aPresContext,
                                      PRInt32 aIndexInParent,
                                      nsIFrame* aParentFrame)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");
  nsIFrame* rv;
  nsresult status = nsTableCaptionFrame::NewFrame(&rv, this, aIndexInParent,
                                                  aParentFrame);
  NS_ASSERTION(nsnull!=rv, "bad arg");
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
