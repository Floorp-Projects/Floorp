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
#include "nsHTMLAtoms.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif

// hack, remove when hack in nsTableCol constructor is removed
static PRInt32 HACKcounter=0;
static nsIAtom *HACKattribute=nsnull;
#include "prprf.h"  // remove when nsTableCol constructor hack is removed
// end hack code


nsTableColGroup::nsTableColGroup(nsIAtom* aTag, int aSpan)
  : nsTableContent(aTag),
    mSpan(aSpan),
    mStartColIndex(0),
    mColCount(0)
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

nsTableColGroup::nsTableColGroup (PRBool aImplicit)
  : nsTableContent(NS_NewAtom(nsTablePart::kColGroupTagString)),
    mSpan(0),
    mStartColIndex(0),
    mColCount(0)
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
    mColCount = 0;  // our index is being changed, trigger reset of col indicies, don't propogate back to table
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

/** returns the number of columns represented by this group.
  * if there are col children, count them (taking into account the span of each)
  * else, check my own span attribute.
  */
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

NS_IMETHODIMP
nsTableColGroup::AppendChild (nsIContent *aContent, PRBool aNotify)
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
        ReplaceChildAt(aContent, colIndex, aNotify);
        contentHandled = PR_TRUE;
        break;
      }
    }
  }
  if (PR_FALSE==contentHandled)
    result = nsTableContent::AppendChild (aContent, aNotify);
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
    return NS_OK;
  }

  nsIContent * lastChild = ChildAt (aIndex);  // lastChild : REFCNT++
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
  NS_ASSERTION((0<=aIndex && ChildCount()>aIndex), "bad arg");
  nsIContent * lastChild = ChildAt (aIndex);  // lastChild: REFCNT++
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

void nsTableColGroup::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  nsHTMLValue val;

  if (aAttribute == nsHTMLAtoms::width) 
  {
    ParseValueOrPercentOrProportional(aValue, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if ( aAttribute == nsHTMLAtoms::span)
  {
    ParseValue(aValue, 0, val, eHTMLUnit_Integer);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    nsHTMLValue val;
    if (ParseTableAlignParam(aValue, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
    return;
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    nsHTMLValue val;
    if (ParseTableAlignParam(aValue, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
    return;
  }
}

void nsTableColGroup::MapAttributesInto(nsIStyleContext* aContext,
                                        nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  float p2t;
  nsHTMLValue value;

  // width
  GetAttribute(nsHTMLAtoms::width, value);
  if (value.GetUnit() != eHTMLUnit_Null) {
    nsStylePosition* position = (nsStylePosition*)
      aContext->GetData(eStyleStruct_Position);
    switch (value.GetUnit()) {
    case eHTMLUnit_Percent:
      position->mWidth.SetPercentValue(value.GetPercentValue());
      break;

    case eHTMLUnit_Pixel:
      p2t = aPresContext->GetPixelsToTwips();
      position->mWidth.SetCoordValue(nscoord(p2t * (float)value.GetPixelValue()));
      break;
    }
  }

  // align
  GetAttribute(nsHTMLAtoms::align, value);
  if (value.GetUnit() != eHTMLUnit_Null) {
    NS_ASSERTION(value.GetUnit() == eHTMLUnit_Enumerated, "unexpected unit");
    nsStyleDisplay* display = (nsStyleDisplay*)aContext->GetData(eStyleStruct_Display);

    switch (value.GetIntValue()) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      display->mFloats = NS_STYLE_FLOAT_LEFT;
      break;

    case NS_STYLE_TEXT_ALIGN_RIGHT:
      display->mFloats = NS_STYLE_FLOAT_RIGHT;
      break;
    }
  }
}

nsresult
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
