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
#include "nsTableCol.h"
#include "nsTableColGroup.h"
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

class nsTableColFrame : public nsFrame {
public:
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

protected:

  nsTableColFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  ~nsTableColFrame();

};



nsTableColFrame::nsTableColFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsFrame(aContent, aParentFrame)
{
}


nsTableColFrame::~nsTableColFrame()
{
}

NS_METHOD nsTableColFrame::Paint(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect)
{
  if (gsDebug==PR_TRUE)
    printf("nsTableColFrame::Paint\n");
  return NS_OK;
}


NS_METHOD nsTableColFrame::Reflow(nsIPresContext*      aPresContext,
                                  nsReflowMetrics&     aDesiredSize,
                                  const nsReflowState& aReflowState,
                                  nsReflowStatus&      aStatus)
{
  NS_ASSERTION(nsnull!=aPresContext, "bad arg");
  aDesiredSize.width=0;
  aDesiredSize.height=0;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    aDesiredSize.maxElementSize->width=0;
    aDesiredSize.maxElementSize->height=0;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

nsresult nsTableColFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                   nsIContent* aContent,
                                   nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableColFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

//---------------------- nsTableCol implementation -----------------------------------

nsTableCol::nsTableCol(nsIAtom* aTag)
  : nsTableContent(aTag),
  mColGroup(0),
  mColIndex(0),
  mRepeat(0)
{
}

nsTableCol::nsTableCol()
: nsTableContent(NS_NewAtom(nsTablePart::kColTagString)),
  mColGroup(0),
  mColIndex(0),
  mRepeat(0)
{
}

nsTableCol::nsTableCol (PRBool aImplicit)
  : nsTableContent(NS_NewAtom(nsTablePart::kColTagString)),
  mColGroup(0),
  mColIndex(0),
  mRepeat(0)
{
  mImplicit = aImplicit;
}

nsTableCol::~nsTableCol()
{
}

// Added for debuging purposes -- remove from final build
nsrefcnt nsTableCol::AddRef(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Add Ref: %x, nsTableCol cnt = %d \n",this, mRefCnt+1);
  return ++mRefCnt;
}

nsrefcnt nsTableCol::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Release: %x, nsTableCol cnt = %d \n",this,mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE) printf("Delete: %x, nsTableCol \n",this);
    delete this;
    return 0;
  }
  return mRefCnt;
}

// TODO: what about proportional width values (0*, 1*, etc.) down in COL tag
// TODO: need a ::SetAttribute hook for width

int nsTableCol::GetType()
{
  return nsITableContent::kTableColType;
}

int nsTableCol::GetRepeat ()
{
  if (0 < mRepeat)
    return mRepeat;
  return 1;
}

nsTableColGroup * nsTableCol::GetColGroup ()
{
  NS_IF_ADDREF(mColGroup);
  return mColGroup;
}

void nsTableCol::SetColGroup (nsTableColGroup * aColGroup)
{
  mColGroup = aColGroup;
}

int nsTableCol::GetColumnIndex ()
{
  return mColIndex;
}
  
void nsTableCol::SetColumnIndex (int aColIndex)
{
  mColIndex = aColIndex;
}

void nsTableCol::SetRepeat (int aRepeat)
{
  mRepeat = aRepeat;
  ResetColumns ();
}

void nsTableCol::ResetColumns ()
{
  if (nsnull != mColGroup)
    mColGroup->ResetColumns ();
}


nsresult
nsTableCol::CreateFrame(nsIPresContext* aPresContext,
                        nsIFrame* aParentFrame,
                        nsIStyleContext* aStyleContext,
                        nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableColFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return rv;
}

/* ---------- Global Functions ---------- */

nsresult
NS_NewTableColPart(nsIHTMLContent** aInstancePtrResult,
                   nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new nsTableCol(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
