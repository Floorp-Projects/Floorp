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
#include "nsTableRowGroup.h"
#include "nsTableRowGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTablePart.h"
#include "nsTableRow.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsIContentDelegate.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsIRenderingContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsHTMLIIDs.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITableContentIID, NS_ITABLECONTENT_IID);

// nsTableContent checks aTag
nsTableRowGroup::nsTableRowGroup(nsIAtom* aTag)
  : nsTableContent(aTag)
{
}

// nsTableContent checks aTag
nsTableRowGroup::nsTableRowGroup(nsIAtom* aTag, PRBool aImplicit)
  : nsTableContent(aTag)
{
  mImplicit = aImplicit;
}

nsTableRowGroup::~nsTableRowGroup()
{
}

/** return the number of columns in the widest row in this group */
PRInt32 nsTableRowGroup::GetMaxColumns()
{ // computed every time for now, could be cached
  PRInt32 result = 0;
  PRInt32 numRows = ChildCount();
  for (PRInt32 rowIndex = 0; rowIndex < numRows; rowIndex++)
  {
    nsTableRow *row = (nsTableRow*)ChildAt(rowIndex);
    PRInt32 numCols = row->GetMaxColumns();
    if (result < numCols)
      result = numCols;
  }
  return result;
}

// Added for debuging purposes -- remove from final build
nsrefcnt nsTableRowGroup::AddRef(void)
{
  if (gsNoisyRefs==PR_TRUE) 
    printf("Add Ref: %x, nsTableRowGroup cnt = %d \n",this,mRefCnt+1);
  return ++mRefCnt;
}

nsrefcnt nsTableRowGroup::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) 
    printf("Release: %x, nsTableRowGroup cnt = %d \n",this,mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE)  printf("Delete: %x, nsTableRowGroup \n",this);
    delete this;
    return 0;
  }
  return mRefCnt;
}

nsresult
nsTableRowGroup::CreateFrame(nsIPresContext* aPresContext,
                             nsIFrame* aParentFrame,
                             nsIStyleContext* aStyleContext,
                             nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableRowGroupFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return rv;
}

void nsTableRowGroup::ResetCellMap ()
{
  // GREG:  enable this assertion when the content notification code is checked in
  // NS_ASSERTION(nsnull!=mTable, "illegal table content state");
  if (nsnull != mTable)
    mTable->ResetCellMap ();
}

PRBool nsTableRowGroup::AppendChild (nsIContent *aContent)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg to AppendChild");
  PRBool result = PR_TRUE;

  // is aContent a TableRow?
  PRBool isRow = IsRow(aContent);

  // if so, simply add it
  if (PR_TRUE==isRow)
  {
    if (gsDebug==PR_TRUE) printf ("nsTableRowGroup::AppendChild -- inserting a row into this row group.\n");
    // if it is, we'll add it here
    PRBool result = nsTableContent::AppendChild (aContent);
    if (result)
    {
      ((nsTableRow *)aContent)->SetRowGroup (this);
      ResetCellMap ();
    }
  }
  // otherwise, if it's a cell, create an implicit row for it
  else 
  {
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType == nsITableContent::kTableCellType)
    {
      // find last row, if ! implicit, make one, append there
      nsTableRow *row = nsnull;
      int index = ChildCount ();
      while ((0 < index) && (nsnull==row))
      {
        nsIContent *child = ChildAt (--index);  // child: REFCNT++    
        if (nsnull != child)
        {
          nsTableContent * content = (nsTableContent *)child;
          if (content->GetType()==nsITableContent::kTableRowType)
            row = (nsTableRow *)content;
          NS_RELEASE(child);                    // child: REFCNT--
        }
      }
      if ((nsnull == row) || (! row->IsImplicit ()))
      {
        printf ("nsTableRow::AppendChild -- creating an implicit row.\n");
        nsIAtom * trDefaultTag = NS_NewAtom(nsTablePart::kRowTagString);   // trDefaultTag: REFCNT++
        row = new nsTableRow (trDefaultTag, PR_TRUE);
        NS_RELEASE(trDefaultTag);                               // trDefaultTag: REFCNT--
        result = AppendChild (row);
        // SEC: check result
      }
      // group is guaranteed to be allocated at this point
      result = row->AppendChild(aContent);
    }
    // otherwise, punt and let the table try to insert it.  Or maybe just return a failure?
    else
    {
      // you should go talk to my parent if you want to insert something other than a row
        result = PR_FALSE;
    }
  }
  return result;
}

PRBool nsTableRowGroup::InsertChildAt (nsIContent *aContent, int aIndex)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg to InsertChildAt");

  // is aContent a TableRow?
  PRBool isRow = IsRow(aContent);

  // if not, ignore the request to add aContent
  if (PR_FALSE==isRow)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return PR_FALSE;
  }

  // if so, add the row to this group
  PRBool result = nsTableContent::InsertChildAt (aContent, aIndex);
  if (result)
  {
    ((nsTableRow *)aContent)->SetRowGroup (this);
    ResetCellMap ();
  }
  return result;
}


PRBool nsTableRowGroup::ReplaceChildAt (nsIContent *aContent, int aIndex)
{
  NS_PRECONDITION(nsnull!=aContent, "bad aContent arg to ReplaceChildAt");
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to ReplaceChildAt");
  if ((nsnull==aContent) || !(0<=aIndex && aIndex<ChildCount()))
    return PR_FALSE;

  // is aContent a TableRow?
  PRBool isRow = IsRow(aContent);

  // if not, ignore the request to replace the child at aIndex
  {
    // you should go talk to my parent if you want to insert something other than a column
    return PR_FALSE;
  }

  nsIContent * lastChild = ChildAt (aIndex);  // lastChild: REFCNT++
  PRBool result = nsTableContent::ReplaceChildAt (aContent, aIndex);
  if (result)
  {
    ((nsTableRow *)aContent)->SetRowGroup (this);
    if (nsnull != lastChild)
      ((nsTableRow *)lastChild)->SetRowGroup (nsnull);
    ResetCellMap ();
  }
  NS_IF_RELEASE(lastChild);                   // lastChild: REFCNT--
  return result;
}

/**
 * Remove a child at the given position. The method is ignored if
 * the index is invalid (too small or too large).
 */
PRBool nsTableRowGroup::RemoveChildAt (int aIndex)
{
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to RemoveChildAt");
  if (!(0<=aIndex && aIndex<ChildCount()))
    return PR_FALSE;

  nsIContent * lastChild = ChildAt (aIndex);  // lastChild: REFCNT++   
  PRBool result = nsTableContent::RemoveChildAt (aIndex);
  if (result)
  {
    if (nsnull != lastChild)
      ((nsTableRow *)lastChild)->SetRowGroup (nsnull);
    ResetCellMap ();
  }
  NS_IF_RELEASE(lastChild);                  // lastChild: REFCNT-- 
  return result;
}

/** support method to determine if the param aContent is a TableRow object */
PRBool nsTableRowGroup::IsRow(nsIContent * aContent) const
{
  PRBool result = PR_FALSE;
  if (nsnull!=aContent)
  {
    // is aContent a row?
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType == nsITableContent::kTableRowType)
      result = PR_TRUE;
  }
  return result;
}

/* ----------- Global Functions ---------- */

nsresult
NS_NewTableRowGroupPart(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new nsTableRowGroup(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
