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
#include "nsTableRow.h"
#include "nsTableRowFrame.h"
#include "nsTableRowGroup.h"
#include "nsTableCell.h"
#include "nsTableFrame.h"
#include "nsCellLayoutData.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsTableRowFrame.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContentDelegate.h"
#include "nsHTMLIIDs.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif

// nsTableContent checks aTag
nsTableRow::nsTableRow(nsIAtom* aTag)
  : nsTableContent(aTag),
  mRowGroup(0),
  mRowIndex(0)
{
}

// nsTableContent checks aTag
nsTableRow::nsTableRow(nsIAtom* aTag, PRBool aImplicit)
  : nsTableContent(aTag),
  mRowGroup(0),
  mRowIndex(0)
{
  mImplicit = aImplicit;
}

nsTableRow::~nsTableRow()
{
}

int nsTableRow::GetType() 
{
  return nsITableContent::kTableRowType;
}

nsTableRowGroup * nsTableRow::GetRowGroup ()
{
  NS_IF_ADDREF(mRowGroup);
  return mRowGroup;
}

void nsTableRow::SetRowGroup (nsTableRowGroup * aRowGroup)
{
  if (aRowGroup != mRowGroup)
  {
    mRowGroup = aRowGroup;
  }
}

PRInt32 nsTableRow::GetRowIndex ()
{
  NS_PRECONDITION(0<=mRowIndex, "bad mRowIndex");
  return mRowIndex;
}

void nsTableRow::SetRowIndex (int aRowIndex)
{
  mRowIndex = aRowIndex;
}

// Added for debuging purposes -- remove from final build
nsrefcnt nsTableRow::AddRef(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Add Ref: %x, nsTableRow cnt = %d \n",this, mRefCnt+1);
  return ++mRefCnt;
}

nsrefcnt nsTableRow::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Release: %x, nsTableRow cnt = %d \n",this,mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE) printf("Delete: %x, nsTableRow \n",this);
    delete this;
    return 0;
  }
  return mRefCnt;
}

PRInt32 nsTableRow::GetMaxColumns() 
{
  int sum = 0;
  for (int i = 0, n = ChildCount(); i < n; i++) {
    nsTableCell *cell = (nsTableCell *) ChildAt(i); // cell: REFCNT++
    sum += cell->GetColSpan();
    NS_RELEASE(cell);                               // cell: REFCNT--
  }
  return sum;
}

void nsTableRow::ResetCellMap ()
{
  if (nsnull != mRowGroup)
  {
    mRowGroup->ResetCellMap ();
  }
}

PRBool nsTableRow::AppendChild (nsIContent *aContent)
{
  NS_PRECONDITION(nsnull != aContent, "null ptr");
  PRBool result = PR_FALSE;
  if (nsnull!=aContent)
  {
    // SEC: should verify that it is indeed table content before we do the cast
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType != nsITableContent::kTableCellType)
    {
      if (gsDebug==PR_TRUE) printf ("nsTableRow::AppendChild -- didn't get a cell, giving up to parent.\n");
      if (nsnull != mRowGroup)
        return mRowGroup->AppendChild (aContent); // let parent have it
      return PR_FALSE;
    }
    else
    {
      PRBool result = nsTableContent::AppendChild (aContent);
      if (result)
      {
        ((nsTableCell *)aContent)->SetRow (this);
        ResetCellMap ();
      }
    }
  }
  return result;
}

PRBool nsTableRow::InsertChildAt (nsIContent *aContent, int aIndex)
{
  NS_PRECONDITION(nsnull != aContent, "null ptr");
  PRBool result = PR_FALSE;
  if (nsnull!=aContent)
  {
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType != nsITableContent::kTableCellType)
    {
      return PR_FALSE;
    }
    else
    {
      PRBool result = nsTableContent::InsertChildAt (aContent, aIndex);
      if (result)
      {
        ((nsTableCell *)aContent)->SetRow (this);
        ResetCellMap ();
      }
    }
  }
  return result;
}


PRBool nsTableRow::ReplaceChildAt (nsIContent *aContent, int aIndex)
{
  PRBool result = PR_FALSE;

  NS_PRECONDITION(nsnull!=aContent, "bad aContent arg to ReplaceChildAt");
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to ReplaceChildAt");
  if ((nsnull==aContent) || !(0<=aIndex && aIndex<ChildCount()))
    return PR_FALSE;
  else
  {
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType != nsITableContent::kTableCellType)
    {
      return PR_FALSE;
    }
    else
      return PR_FALSE;
    nsIContent * oldChild = ChildAt (aIndex); // oldChild: REFCNT++
    result = nsTableContent::ReplaceChildAt (aContent, aIndex);
    if (result)
    {
      ((nsTableCell *)aContent)->SetRow (this);
      if (nsnull!=oldChild)
        ((nsTableCell *)oldChild)->SetRow (nsnull);
      ResetCellMap ();
    }
    NS_IF_RELEASE(oldChild);                  // oldChild: REFCNT--
  }
  return result;
}

/**
 * Remove a child at the given position. The method is ignored if
 * the index is invalid (too small or too large).
 */
PRBool nsTableRow::RemoveChildAt (int aIndex)
{
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to ReplaceChildAt");
  if (!(0<=aIndex && aIndex<ChildCount()))
    return PR_FALSE;
  
  PRBool result = PR_FALSE;
  nsIContent * oldChild = ChildAt (aIndex);   // oldChild: REFCNT++
  if (nsnull!=oldChild)
  {
    result = nsTableContent::RemoveChildAt (aIndex);
    if (result)
    {
      if (nsnull != oldChild)
        ((nsTableCell *)oldChild)->SetRow (nsnull);
      ResetCellMap ();
    }
  }
  NS_IF_RELEASE(oldChild);                    // oldChild: REFCNT--
  return result;
}

nsresult
nsTableRow::CreateFrame(nsIPresContext* aPresContext,
                        nsIFrame* aParentFrame,
                        nsIStyleContext* aStyleContext,
                        nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableRowFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return rv;
}

/* ---------- Global Functions ---------- */

nsresult
NS_NewTableRowPart(nsIHTMLContent** aInstancePtrResult,
               nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new nsTableRow(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
