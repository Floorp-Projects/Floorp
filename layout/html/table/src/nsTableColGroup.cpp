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
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif

static NS_DEFINE_IID(kITableContentIID, NS_ITABLECONTENT_IID);


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

/** returns the number of columns represented by this group.
  * if there are col children, count them (taking into account the span of each)
  * else, check my own span attribute.
  */
int nsTableColGroup::GetColumnCount ()
{
  if (0 == mColCount)
  {
    int count;
    ChildCount (count);
    if (0 < count)
    {
      for (int index = 0; index < count; index++)
      {
        nsIContent * child;
        ChildAt (index, child); // child: REFCNT++
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

void nsTableColGroup::ResetColumns ()
{
  mColCount = 0;
}

NS_IMETHODIMP
nsTableColGroup::AppendChildTo (nsIContent *aContent, PRBool aNotify)
{
  NS_ASSERTION(nsnull!=aContent, "bad arg");

  // is aContent a TableRow?
  PRBool isCol = IsCol(aContent);

  if (PR_FALSE==isCol)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return NS_OK;
  }

  nsresult result = NS_ERROR_FAILURE;
  PRBool contentHandled = PR_FALSE;
  // SEC: TODO verify that aContent is table content
  nsTableContent *tableContent = (nsTableContent *)aContent;
  PRBool isImplicit;
  tableContent->IsSynthetic(isImplicit);
  if (PR_FALSE==isImplicit)
  {
    /* if aContent is not implicit, 
     * and if we already have an implicit column for this actual column, 
     * then replace the implicit col with this actual col.
     */
    PRInt32 childCount;
    ChildCount(childCount);
    for (PRInt32 colIndex=0; colIndex<childCount; colIndex++)
    {
      nsTableContent *col;
      ChildAt(colIndex, (nsIContent*&)col);
      NS_ASSERTION(nsnull!=col, "bad child");
      PRBool colIsImplicit;
      col->IsSynthetic(colIsImplicit);
      if (PR_TRUE==colIsImplicit)
      {
        ReplaceChildAt(aContent, colIndex, aNotify);
        contentHandled = PR_TRUE;
        break;
      }
    }
  }
  if (PR_FALSE==contentHandled)
    result = nsTableContent::AppendChildTo (aContent, aNotify);
  if (NS_OK==result)
  {
    ((nsTableCol *)aContent)->SetColGroup (this);
    ((nsTableCol *)aContent)->SetColumnIndex (mStartColIndex + mColCount);
    ResetColumns ();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableColGroup::InsertChildAt (nsIContent *aContent, PRInt32 aIndex,
                                PRBool aNotify)
{
  NS_ASSERTION(nsnull!=aContent, "bad arg");

  // is aContent a TableCol?
  PRBool isCol = IsCol(aContent);

  // if not, ignore the request to add aContent
  if (PR_FALSE==isCol)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return NS_OK;
  }

  // if so, add the row to this group
  nsresult result = nsTableContent::InsertChildAt (aContent, aIndex, aNotify);
  if (NS_OK==result)
  {
    ((nsTableCol *)aContent)->SetColGroup (this);
    ResetColumns ();
  }
  return NS_OK;
}


NS_IMETHODIMP
nsTableColGroup::ReplaceChildAt (nsIContent * aContent, PRInt32 aIndex,
                                 PRBool aNotify)
{
  PRInt32 numKids;
  ChildCount(numKids);
  NS_ASSERTION(nsnull!=aContent, "bad arg");
  NS_ASSERTION((0<=aIndex && numKids>aIndex), "bad arg");
  if ((nsnull==aContent) || !(0<=aIndex && numKids>aIndex))
    return PR_FALSE;

  // is aContent a TableRow?
  PRBool isCol = IsCol(aContent);

  // if not, ignore the request to replace the child at aIndex
  if (PR_FALSE==isCol)
  {
    // you should go talk to my parent if you want to insert something other than a column
    return NS_OK;
  }

  nsIContent * lastChild;
  ChildAt (aIndex, lastChild);  // lastChild : REFCNT++
  NS_ASSERTION(nsnull!=lastChild, "bad child");
  nsresult result = nsTableContent::ReplaceChildAt (aContent, aIndex, aNotify);
  if (NS_OK==result)
  {
    ((nsTableCol *)aContent)->SetColGroup (this);
    if (nsnull != lastChild)
      ((nsTableCol *)lastChild)->SetColGroup (nsnull);
    ResetColumns ();
  }
  NS_RELEASE(lastChild);                      // lastChild : REFCNT--

  return NS_OK;
}

/**
 * Remove a child at the given position. The method is ignored if
 * the index is invalid (too small or too large).
 */
NS_IMETHODIMP
nsTableColGroup::RemoveChildAt (PRInt32 aIndex, PRBool aNotify)
{
#ifdef NS_DEBUG
  PRInt32 numKids;
  ChildCount(numKids);
  NS_ASSERTION((0<=aIndex && numKids>aIndex), "bad arg");
#endif

  nsIContent * lastChild;
  ChildAt (aIndex, lastChild);  // lastChild: REFCNT++
  NS_ASSERTION(nsnull!=lastChild, "bad child");
  nsresult result = nsTableContent::RemoveChildAt (aIndex, aNotify);
  if (NS_OK==result)
  {
    if (nsnull != lastChild)
      ((nsTableCol *)lastChild)->SetColGroup (nsnull);
    ResetColumns ();
  }
  NS_IF_RELEASE(lastChild);                   // lastChild REFCNT--

  return NS_OK;
}

/** support method to determine if the param aContent is an nsTableCol object */
PRBool nsTableColGroup::IsCol(nsIContent * aContent) const
{
  PRBool result = PR_FALSE;
  if (nsnull!=aContent)
  {
    // is aContent a col?
    nsITableContent *tableContentInterface = nsnull;
    nsresult rv = aContent->QueryInterface(kITableContentIID, 
                                           (void **)&tableContentInterface);  // tableContentInterface: REFCNT++

    if (NS_SUCCEEDED(rv))
    {
      const int contentType = tableContentInterface->GetType();
      NS_RELEASE(tableContentInterface);
      if (contentType == nsITableContent::kTableColType)
        result = PR_TRUE;
    }
  }
  return result;
}

NS_IMETHODIMP
nsTableColGroup::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                              PRBool aNotify)
{
  nsHTMLValue val;

  if (aAttribute == nsHTMLAtoms::width) 
  {
    ParseValueOrPercentOrProportional(aValue, val, eHTMLUnit_Pixel);
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }
  else if ( aAttribute == nsHTMLAtoms::span)
  {
    ParseValue(aValue, 0, val, eHTMLUnit_Integer);
    SetSpan(val.GetIntValue());
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    nsHTMLValue val;
    if (ParseTableAlignParam(aValue, val)) {
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    nsHTMLValue val;
    if (ParseTableAlignParam(aValue, val)) {
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  return nsTableContent::SetAttribute(aAttribute, aValue, aNotify);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");
  if (nsnull != aAttributes) {

    float p2t;
    nsHTMLValue value;
    nsStyleText* textStyle = nsnull;

    // width
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() != eHTMLUnit_Null) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      switch (value.GetUnit()) {
      case eHTMLUnit_Percent:
        position->mWidth.SetPercentValue(value.GetPercentValue());
        break;

      case eHTMLUnit_Pixel:
        p2t = aPresContext->GetPixelsToTwips();
        position->mWidth.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
        break;
      }
    }

    // align: enum
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mTextAlign = value.GetIntValue();
    }
    
    // valign: enum
    aAttributes->GetAttribute(nsHTMLAtoms::valign, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      if (nsnull==textStyle)
        textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mVerticalAlign.SetIntValue(value.GetIntValue(), eStyleUnit_Enumerated);
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsTableColGroup::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}



NS_IMETHODIMP
nsTableColGroup::CreateFrame(nsIPresContext* aPresContext,
                             nsIFrame* aParentFrame,
                             nsIStyleContext* aStyleContext,
                             nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableColGroupFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
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
