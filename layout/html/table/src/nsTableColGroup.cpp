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
#include "nsTableColGroup.h"
#include "nsTableColGroupFrame.h"
#include "nsTableCol.h"
#include "nsTablePart.h"
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


nsTableColGroup::nsTableColGroup(nsIAtom* aTag, int aSpan)
  : nsTableContent(aTag),
    mSpan(aSpan),
    mStartColIndex(0),
    mColCount(0)
{
}

nsTableColGroup::nsTableColGroup (PRBool aImplicit)
  : nsTableContent(NS_NewAtom(nsTablePart::kColGroupTagString)),
    mSpan(0),
    mStartColIndex(0),
    mColCount(0)
{
  mImplicit = aImplicit;
}


nsTableColGroup::~nsTableColGroup()
{
}

// Added for debuging purposes -- remove from final build
nsrefcnt nsTableColGroup::AddRef(void)
{
  if (gsNoisyRefs==PR_TRUE) 
    printf("Add Ref: %x, nsTableColGroup cnt = %d \n",this,mRefCnt+1);
  return ++mRefCnt;
}

nsrefcnt nsTableColGroup::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) 
    if (gsNoisyRefs==PR_TRUE) printf("Release: %x, nsTableColGroup cnt = %d \n",this,mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE) printf("Delete: %x, nsTableColGroup \n",this);
    delete this;
    return 0;
  }
  return mRefCnt;
}

int nsTableColGroup::GetType()
{
  return nsITableContent::kTableColGroupType;
}

int nsTableColGroup::GetSpan ()
{
  if (0 < mSpan)
    return mSpan;
  return 1;
}
  
int nsTableColGroup::GetStartColumnIndex ()
{
  return mStartColIndex;
}
  
void nsTableColGroup::SetStartColumnIndex (int aIndex)
{
  if (aIndex != mStartColIndex)
    mColCount = 0;  // our index is beign changed, trigger reset of col indicies, don't propogate back to table
  mStartColIndex = aIndex;
}


void nsTableColGroup::SetSpan (int aSpan)
{
  mSpan = aSpan;
  if (0 < ChildCount ())  // span is only relevant if we don't have children
    ResetColumns ();
}

void nsTableColGroup::ResetColumns ()
{
  mColCount = 0;
  if (nsnull != mTable)
    mTable->ResetColumns ();
}

int nsTableColGroup::GetColumnCount ()
{
  if (0 == mColCount)
  {
    int count = ChildCount ();
    if (0 < count)
    {
      for (int index = 0; index < count; index++)
      {
        nsIContent * child = ChildAt (index); // child: REFCNT++
        NS_ASSERTION(nsnull!=child, "bad child");
        // is child a column?
        nsTableContent *tableContent = (nsTableContent *)child;
        if (tableContent->GetType() == nsITableContent::kTableColType)
        {
          nsTableCol * col = (nsTableCol *)tableContent;
          col->SetColumnIndex (mStartColIndex + mColCount);
          mColCount += col->GetRepeat ();
        }
        NS_RELEASE(child);                   // child: REFCNT--
      }
    }
    else
      mColCount = GetSpan ();
  }
  return mColCount;
}

PRBool nsTableColGroup::SetInternalAttribute (nsString *aName, nsString *aValue)
{
// Assert aName
  PRBool result = PR_FALSE;
  return result;
}

nsString * nsTableColGroup::GetInternalAttribute (nsString *aName)
{
// Assert aName
  nsString * result = nsnull;
  return result;
}

PRBool nsTableColGroup::UnsetInternalAttribute (nsString *aName)
{
// Assert aName
  PRBool result = PR_FALSE;
  return result;
}

int nsTableColGroup::GetInternalAttributeState (nsString *aName)
{
// Assert aName
  int result = 0;
  return result;
}

PRBool nsTableColGroup::IsInternalAttribute (nsString *aName)
{
// Assert aName
  PRBool result = PR_FALSE;
  return result;
}

nsString * nsTableColGroup::GetAllInternalAttributeNames ()
{
// Assert aName
  nsString * result = nsnull;
  return result;
}

PRBool nsTableColGroup::AppendChild (nsIContent *aContent)
{
  NS_ASSERTION(nsnull!=aContent, "bad arg");

  // is aContent a TableRow?
  PRBool isCol = IsCol(aContent);

  if (PR_FALSE==isCol)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return PR_FALSE;
  }

  PRBool result = PR_FALSE;
  PRBool contentHandled = PR_FALSE;
  // SEC: TODO verify that aContent is table content
  nsTableContent *tableContent = (nsTableContent *)aContent;
  if (PR_FALSE==tableContent->IsImplicit())
  {
    /* if aContent is not implicit, 
     * and if we already have an implicit column for this actual column, 
     * then replace the implicit col with this actual col.
     */
    PRInt32 childCount = ChildCount();
    for (PRInt32 colIndex=0; colIndex<childCount; colIndex++)
    {
      nsTableContent *col = (nsTableContent*)ChildAt(colIndex);
      NS_ASSERTION(nsnull!=col, "bad child");
      if (PR_TRUE==col->IsImplicit())
      {
        ReplaceChildAt(aContent, colIndex);
        contentHandled = PR_TRUE;
        break;
      }
    }
  }
  if (PR_FALSE==contentHandled)
    result = nsTableContent::AppendChild (aContent);
  if (result)
  {
    /* Set the table pointer */
    ((nsTableContent*)aContent)->SetTable(mTable);
    
    ((nsTableCol *)aContent)->SetColGroup (this);
    ResetColumns ();
  }
  return result;
}

PRBool nsTableColGroup::InsertChildAt (nsIContent *aContent, int aIndex)
{
  NS_ASSERTION(nsnull!=aContent, "bad arg");

  // is aContent a TableCol?
  PRBool isCol = IsCol(aContent);

  // if not, ignore the request to add aContent
  if (PR_FALSE==isCol)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return PR_FALSE;
  }

  // if so, add the row to this group
  PRBool result = nsTableContent::InsertChildAt (aContent, aIndex);
  if (result)
  {
    ((nsTableCol *)aContent)->SetColGroup (this);
    ResetColumns ();
  }
  return result;
}


PRBool nsTableColGroup::ReplaceChildAt (nsIContent * aContent, int aIndex)
{
  NS_ASSERTION(nsnull!=aContent, "bad arg");
  NS_ASSERTION((0<=aIndex && ChildCount()>aIndex), "bad arg");
  if ((nsnull==aContent) || !(0<=aIndex && ChildCount()>aIndex))
    return PR_FALSE;

  // is aContent a TableRow?
  PRBool isCol = IsCol(aContent);

  // if not, ignore the request to replace the child at aIndex
  if (PR_FALSE==isCol)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return PR_FALSE;
  }

  nsIContent * lastChild = ChildAt (aIndex);  // lastChild : REFCNT++
  NS_ASSERTION(nsnull!=lastChild, "bad child");
  PRBool result = nsTableContent::ReplaceChildAt (aContent, aIndex);
  if (result)
  {
    ((nsTableCol *)aContent)->SetColGroup (this);
    if (nsnull != lastChild)
      ((nsTableCol *)lastChild)->SetColGroup (nsnull);
    ResetColumns ();
  }
  NS_RELEASE(lastChild);                      // lastChild : REFCNT--
  return result;
}

/**
 * Remove a child at the given position. The method is ignored if
 * the index is invalid (too small or too large).
 */
PRBool nsTableColGroup::RemoveChildAt (int aIndex)
{
  NS_ASSERTION((0<=aIndex && ChildCount()>aIndex), "bad arg");
  nsIContent * lastChild = ChildAt (aIndex);  // lastChild: REFCNT++
  NS_ASSERTION(nsnull!=lastChild, "bad child");
  PRBool result = nsTableContent::RemoveChildAt (aIndex);
  if (result)
  {
    if (nsnull != lastChild)
      ((nsTableCol *)lastChild)->SetColGroup (nsnull);
    ResetColumns ();
  }
  NS_IF_RELEASE(lastChild);                   // lastChild REFCNT--
  return result;
}

/** support method to determine if the param aContent is a TableRow object */
PRBool nsTableColGroup::IsCol(nsIContent * aContent) const
{
  NS_ASSERTION(nsnull!=aContent, "bad arg");
  PRBool result = PR_FALSE;
  if (nsnull!=aContent)
  {
    // is aContent a col?
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType == nsITableContent::kTableColType)
      result = PR_TRUE;
  }
  return result;
}

nsIFrame* nsTableColGroup::CreateFrame(nsIPresContext* aPresContext,
                                       PRInt32 aIndexInParent,
                                       nsIFrame* aParentFrame)
{
  nsIFrame* rv;
  nsresult status = nsTableColGroupFrame::NewFrame(&rv, this, aIndexInParent,
                                                   aParentFrame);
  NS_ASSERTION(nsnull!=rv, "can't allocate a new frame");
  return rv;
}

/* ---------- Global Functions ---------- */

NS_HTML nsresult
NS_NewTableColGroupPart(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new nsTableColGroup(aTag, 0);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
