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
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  void          Paint(nsIPresContext& aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect& aDirtyRect);

  ReflowStatus  ResizeReflow(nsIPresContext* aPresContext,
                             nsReflowMetrics& aDesiredSize,
                             const nsSize&   aMaxSize,
                             nsSize*         aMaxElementSize);

  ReflowStatus  IncrementalReflow(nsIPresContext*  aPresContext,
                                  nsReflowMetrics& aDesiredSize,
                                  const nsSize&    aMaxSize,
                                  nsReflowCommand& aReflowCommand);

protected:

  nsTableColFrame(nsIContent* aContent,
                  PRInt32 aIndexInParent,
					        nsIFrame* aParentFrame);

  ~nsTableColFrame();

};



nsTableColFrame::nsTableColFrame(nsIContent* aContent,
                     PRInt32     aIndexInParent,
                     nsIFrame*   aParentFrame)
  : nsFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsTableColFrame::~nsTableColFrame()
{
}

void nsTableColFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  if (gsDebug==PR_TRUE)
    printf("nsTableColFrame::Paint\n");
}


nsIFrame::ReflowStatus
nsTableColFrame::ResizeReflow(nsIPresContext* aPresContext,
                        nsReflowMetrics& aDesiredSize,
                        const nsSize&   aMaxSize,
                        nsSize*         aMaxElementSize)
{
  NS_ASSERTION(nsnull!=aPresContext, "bad arg");
  if (gsDebug==PR_TRUE) printf("nsTableoupFrame::ResizeReflow\n");
  aDesiredSize.width=0;
  aDesiredSize.height=0;
  if (nsnull!=aMaxElementSize)
  {
    aMaxElementSize->width=0;
    aMaxElementSize->height=0;
  }
  return nsIFrame::frComplete;
}

nsIFrame::ReflowStatus
nsTableColFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                   nsReflowMetrics& aDesiredSize,
                                   const nsSize&    aMaxSize,
                                   nsReflowCommand& aReflowCommand)
{
  NS_ASSERTION(nsnull!=aPresContext, "bad arg");
  if (gsDebug==PR_TRUE) printf("nsTableColFrame::IncrementalReflow\n");
  aDesiredSize.width=0;
  aDesiredSize.height=0;
  return nsIFrame::frComplete;
}

nsresult nsTableColFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                   nsIContent* aContent,
                                   PRInt32     aIndexInParent,
                                   nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableColFrame(aContent, aIndexInParent, aParent);
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

static const nsString kRepeatAttributeName("REPEAT");

PRBool nsTableCol::SetInternalAttribute (nsString * aName, nsString * aValue)
{
  /*
  // Assert aName and aValue
  if (aName->EqualsIgnoreCase (kRepeatAttributeName))
  {
    PRInt32 value = atoi(aValue->ToCString());
    SetRepeat (aValue);
    return true;
  }
  return super.setInternalAttribute (aName, aValue);
  */
  return PR_FALSE;
}

/** I allocated it for you, you delete it when you're done */
// ack!!!  SEC this code and HTMLContainer need to get together and decide
nsString * nsTableCol::GetInternalAttribute (nsString * aName)
{
  /*
  // Assert aName
  if (aName->EqualsIgnoreCase (kRepeatAttributeName))
  {
    char repeatAsCharArray[10];
    sprintf(repeatAsCharArray, "%d", GetRepeat());
    nsString * result = new nsString(repeatAsCharArray);
  }
  return nsTableContent::GetInternalAttribute (aName);
  */
  return nsnull;
}

PRBool nsTableCol::UnsetInternalAttribute (nsString * aName)
{
  /*
  if (aName->EqualsIgnoreCase (kRepeatAttributeName))
  {
    fRepeat = 0;
    return true;
  }
  return nsTableContent::UnsetInternalAttribute (aName);
  */
  return PR_FALSE;
}

int nsTableCol::GetInternalAttributeState (nsString * aName)
{
  /*
  if (aName->EqualsIgnoreCase (kRepeatAttributeName))
  {
    if (0 < fRepeat)
      return kAttributeSet;
  }
  return super.getInternalAttributeState (aName);
  */
  return 0;
}

PRBool nsTableCol::IsInternalAttribute (nsString * aName)
{
  /*
  if (aName->EqualsIgnoreCase (kRepeatAttributeName))
    return true;
  return super.isInternalAttribute (aName);
  */
  return PR_FALSE;
}


static nsString * kInternalAttributeNames = nsnull;

nsString * nsTableCol::GetAllInternalAttributeNames ()
{
  /*
  if (null == kInternalAttributeNames)
  {
    nsString * [] superInternal = super.getAllInternalAttributeNames ();

    int superLen = superInternal.length;
    kInternalAttributeNames = new nsString * [superLen + 1];
    System.arraycopy (superInternal, 0, kInternalAttributeNames, 0, superLen);
    kInternalAttributeNames[superLen] = kRepeatAttributeName;
  }
  return kInternalAttributeNames;
  */
  return nsnull;
}

nsIFrame* nsTableCol::CreateFrame(nsIPresContext* aPresContext,
                                  PRInt32 aIndexInParent,
                                  nsIFrame* aParentFrame)
{
  nsIFrame* rv;
  nsresult status = nsTableColFrame::NewFrame(&rv, this, aIndexInParent,
                                              aParentFrame);
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
