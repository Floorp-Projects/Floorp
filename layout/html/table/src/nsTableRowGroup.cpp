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
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsIRenderingContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"

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

void nsTableRowGroup::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
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

void nsTableRowGroup::MapAttributesInto(nsIStyleContext* aContext,
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
      // make sure the table cell map gets rebuilt
    }
  }
  // otherwise, if it's a cell, create an implicit row for it
  else if (IsTableCell(aContent))
  {
    // find last row, if ! implicit, make one, append there
    nsTableRow *row = nsnull;
    int index = ChildCount ();
    while ((0 < index) && (nsnull==row))
    {
      nsIContent *child = ChildAt (--index);  // child: REFCNT++    
      if (nsnull != child)
      {
        if (IsRow(child))
          row = (nsTableRow *)child;
        NS_RELEASE(child);                    // child: REFCNT--
      }
    }
    PRBool rowIsImplicit = PR_FALSE;
    if (nsnull!=row)
      row->IsSynthetic(rowIsImplicit);
    if ((nsnull == row) || (PR_FALSE==rowIsImplicit))
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
    nsITableContent *tableContentInterface = nsnull;
    nsresult rv = aContent->QueryInterface(kITableContentIID, 
                                           (void **)&tableContentInterface);  // tableContentInterface: REFCNT++

    const int contentType = tableContentInterface->GetType();
    NS_RELEASE(tableContentInterface);
    if (contentType == nsITableContent::kTableRowType)
      result = PR_TRUE;
  }
  return result;
}

/** support method to determine if the param aContent is a TableCell object */
PRBool nsTableRowGroup::IsTableCell(nsIContent * aContent) const
{
  PRBool result = PR_FALSE;
  if (nsnull!=aContent)
  {
    // is aContent a table cell?
    nsITableContent *tableContentInterface = nsnull;
    nsresult rv = aContent->QueryInterface(kITableContentIID, 
                                           (void **)&tableContentInterface);  // tableContentInterface: REFCNT++

    const int contentType = tableContentInterface->GetType();
    NS_RELEASE(tableContentInterface);
    if (contentType == nsITableContent::kTableCellType)
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
  nsIHTMLContent* content = new nsTableRowGroup(aTag);
  if (nsnull == content) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return content->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
