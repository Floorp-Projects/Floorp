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
#include "nsHTMLAtoms.h"

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

NS_IMETHODIMP
nsTableRow::AppendChild (nsIContent *aContent, PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aContent, "null ptr");
  nsresult rv = NS_OK;
  if (nsnull!=aContent)
  {
    // SEC: should verify that it is indeed table content before we do the cast
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType != nsITableContent::kTableCellType)
    {
      if (gsDebug==PR_TRUE) printf ("nsTableRow::AppendChild -- didn't get a cell, giving up to parent.\n");
      if (nsnull != mRowGroup)
        return mRowGroup->AppendChild (aContent, aNotify); // let parent have it
      return PR_FALSE;
    }
    else
    {
      rv = nsTableContent::AppendChild (aContent, aNotify);
      if (NS_OK == rv)
      {
        ((nsTableCell *)aContent)->SetRow (this);
        ResetCellMap ();
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsTableRow::InsertChildAt (nsIContent *aContent, PRInt32 aIndex,
                           PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aContent, "null ptr");
  nsresult rv = NS_OK;
  if (nsnull!=aContent)
  {
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType != nsITableContent::kTableCellType)
    { // I only insert cells for now
      // TODO: wrap whatever I'm given here in a cell and insert it
      NS_ASSERTION(PR_FALSE, "unimplemented attempt to insert a non-cell into a row");
      return NS_ERROR_FAILURE;
    }
    else
    {
      rv = nsTableContent::InsertChildAt (aContent, aIndex, aNotify);
      if (NS_OK == rv)
      {
        ((nsTableCell *)aContent)->SetRow (this);
        ResetCellMap ();
      }
    }
  }
  return rv;
}


NS_IMETHODIMP
nsTableRow::ReplaceChildAt (nsIContent *aContent, PRInt32 aIndex,
                            PRBool aNotify)
{
  nsresult rv = NS_OK;

  NS_PRECONDITION(nsnull!=aContent, "bad aContent arg to ReplaceChildAt");
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to ReplaceChildAt");
  if ((nsnull==aContent) || !(0<=aIndex && aIndex<ChildCount()))
    return NS_ERROR_FAILURE;
  else
  {
    nsTableContent *tableContent = (nsTableContent *)aContent;
    const int contentType = tableContent->GetType();
    if (contentType != nsITableContent::kTableCellType)
    { // I only insert cells for now
      // TODO: wrap whatever I'm given here in a cell and insert it
      NS_ASSERTION(PR_FALSE, "unimplemented attempt to insert a non-cell into a row");
      return NS_ERROR_FAILURE;
    }
    else
    {
      NS_ASSERTION(PR_FALSE, "unimplemented attempt to replace a cell in a row");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
#if XXX
    nsIContent * oldChild = ChildAt (aIndex); // oldChild: REFCNT++
    result = nsTableContent::ReplaceChildAt (aContent, aIndex, aNotify);
    if (result)
    {
      ((nsTableCell *)aContent)->SetRow (this);
      if (nsnull!=oldChild)
        ((nsTableCell *)oldChild)->SetRow (nsnull);
      ResetCellMap ();
    }
    NS_IF_RELEASE(oldChild);                  // oldChild: REFCNT--
#endif
  }
  return rv;
}

/**
 * Remove a child at the given position. The method is ignored if
 * the index is invalid (too small or too large).
 */
NS_IMETHODIMP
nsTableRow::RemoveChildAt (int aIndex, PRBool aNotify)
{
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to ReplaceChildAt");
  if (!(0<=aIndex && aIndex<ChildCount()))
    return NS_ERROR_FAILURE;
  
  nsresult rv = NS_OK;
  nsIContent * oldChild = ChildAt (aIndex);   // oldChild: REFCNT++
  if (nsnull!=oldChild)
  {
    rv = nsTableContent::RemoveChildAt (aIndex, aNotify);
    if (NS_OK == rv)
    {
      if (nsnull != oldChild)
        ((nsTableCell *)oldChild)->SetRow (nsnull);
      ResetCellMap ();
    }
  }
  NS_IF_RELEASE(oldChild);                    // oldChild: REFCNT--
  return rv;
}

void nsTableRow::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  NS_PRECONDITION(nsnull!=aAttribute, "bad attribute arg");
  nsHTMLValue val;
  if ((aAttribute == nsHTMLAtoms::align) &&
      ParseDivAlignParam(aValue, val)) {
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  if ((aAttribute == nsHTMLAtoms::valign) &&
      ParseAlignParam(aValue, val)) {
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
}

void nsTableRow::MapAttributesInto(nsIStyleContext* aContext,
                                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");
  if (nsnull != mAttributes) {
    nsHTMLValue value;
    nsStyleText* textStyle = nsnull;

    // align: enum
    GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mTextAlign = value.GetIntValue();
    }
    
    // valign: enum
    GetAttribute(nsHTMLAtoms::valign, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      if (nsnull==textStyle)
        textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mVerticalAlign.SetIntValue(value.GetIntValue(), eStyleUnit_Enumerated);
    }
  }
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
