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

// hack, remove when hack in nsTableCol constructor is removed
static PRInt32 HACKcounter=0;
static nsIAtom *HACKattribute=nsnull;
#include "prprf.h"  // remove when nsTableCol constructor hack is removed
// end hack code

// nsTableContent checks aTag
nsTableRowGroup::nsTableRowGroup(nsIAtom* aTag)
  : nsTableContent(aTag)
{
  /* begin hack */
  // temporary hack to get around style sheet optimization that folds all
  // col style context into one, unless there is a unique HTML attribute set
  char out[40];
  PR_snprintf(out, 40, "%d", HACKcounter);
  const nsString value(out);
  if (nsnull==HACKattribute)
    HACKattribute = NS_NewAtom("Steve's unbelievable hack attribute");
  SetAttribute(HACKattribute, value);
  HACKcounter++;
  /* end hack */
}

// nsTableContent checks aTag
nsTableRowGroup::nsTableRowGroup(nsIAtom* aTag, PRBool aImplicit)
  : nsTableContent(aTag)
{
  mImplicit = aImplicit;
  /* begin hack */
  // temporary hack to get around style sheet optimization that folds all
  // col style context into one, unless there is a unique HTML attribute set
  char out[40];
  PR_snprintf(out, 40, "%d", HACKcounter);
  const nsString value(out);
  if (nsnull==HACKattribute)
    HACKattribute = NS_NewAtom("Steve's unbelievable hack attribute");
  SetAttribute(HACKattribute, value);
  HACKcounter++;
  /* end hack */
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

NS_IMETHODIMP
nsTableRowGroup::AppendChild (nsIContent *aContent, PRBool aNotify)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg to AppendChild");
  nsresult result = NS_OK;

  // is aContent a TableRow?
  PRBool isRow = IsRow(aContent);

  // if so, simply add it
  if (PR_TRUE==isRow)
  {
    if (gsDebug==PR_TRUE) printf ("nsTableRowGroup::AppendChild -- inserting a row into this row group.\n");
    // if it is, we'll add it here
    result = nsTableContent::AppendChild (aContent, PR_FALSE);
    if (NS_OK==result)
    {
      ((nsTableRow *)aContent)->SetRowGroup (this);
      // after each row insertion, make sure we have corresponding column content objects
      if (nsnull!=mTable)
        mTable->EnsureColumns();
      // also make sure the table cell map gets rebuilt
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
        result = AppendChild (row, PR_FALSE);
        // SEC: check result
      }
      // group is guaranteed to be allocated at this point
      result = row->AppendChild(aContent, PR_FALSE);
    }
    // otherwise, punt and let the table try to insert it.  Or maybe just return a failure?
    else
    {
      // you should go talk to my parent if you want to insert something other than a row
      result = NS_ERROR_FAILURE;
    }
  }
  return result;
}

NS_IMETHODIMP
nsTableRowGroup::InsertChildAt (nsIContent *aContent, PRInt32 aIndex,
                                PRBool aNotify)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg to InsertChildAt");

  // is aContent a TableRow?
  PRBool isRow = IsRow(aContent);

  // if not, ignore the request to add aContent
  if (PR_FALSE==isRow)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return NS_ERROR_FAILURE;
  }

  // if so, add the row to this group
  nsresult result = nsTableContent::InsertChildAt (aContent, aIndex, PR_FALSE);
  if (NS_OK==result)
  {
    ((nsTableRow *)aContent)->SetRowGroup (this);
    ResetCellMap ();
  }

  return result;
}


NS_IMETHODIMP
nsTableRowGroup::ReplaceChildAt (nsIContent *aContent, PRInt32 aIndex,
                                 PRBool aNotify)
{
  NS_PRECONDITION(nsnull!=aContent, "bad aContent arg to ReplaceChildAt");
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to ReplaceChildAt");
  if ((nsnull==aContent) || !(0<=aIndex && aIndex<ChildCount()))
    return NS_ERROR_FAILURE;

  // is aContent a TableRow?
  PRBool isRow = IsRow(aContent);

  // if not, ignore the request to replace the child at aIndex
  if (PR_FALSE==isRow)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return NS_ERROR_FAILURE;
  }

  nsIContent * lastChild = ChildAt (aIndex);  // lastChild: REFCNT++
  nsresult result = nsTableContent::ReplaceChildAt (aContent, aIndex, PR_FALSE);
  if (NS_OK==result)
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
NS_IMETHODIMP
nsTableRowGroup::RemoveChildAt (PRInt32 aIndex, PRBool aNotify)
{
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to RemoveChildAt");
  if (!(0<=aIndex && aIndex<ChildCount()))
    return NS_ERROR_FAILURE;

  nsIContent * lastChild = ChildAt (aIndex);  // lastChild: REFCNT++   
  nsresult result = nsTableContent::RemoveChildAt (aIndex, PR_FALSE);
  if (NS_OK==result)
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
