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
#include "nsTableCell.h"
#include "nsTableColGroup.h"
#include "nsTableRow.h"
#include "nsTablePart.h"
#include "nsTableCellFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

#ifdef NS_DEBUG
static PRBool gsDebug = PR_TRUE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif

// nsTableContent checks aTag
nsTableCell::nsTableCell(nsIAtom* aTag)
  : nsTableContent(aTag),
  mRow(0),
  mRowSpan(1),
  mColSpan(1),
  mColIndex(0)
{
  mImplicit = PR_FALSE;
}

nsTableCell::nsTableCell(PRBool aImplicit)
  : nsTableContent(NS_NewAtom(nsTablePart::kDataCellTagString)),
  mRow(0),
  mRowSpan(1),
  mColSpan(1),
  mColIndex(0)
{
  mImplicit = aImplicit;
}

// nsTableContent checks aTag
nsTableCell::nsTableCell (nsIAtom* aTag, int aRowSpan, int aColSpan)
  : nsTableContent(aTag),
  mRow(0),
  mRowSpan(aRowSpan),
  mColSpan(aColSpan),
  mColIndex(0)
{
  NS_ASSERTION(0<aRowSpan, "bad row span");
  NS_ASSERTION(0<aColSpan, "bad col span");
  mImplicit = PR_FALSE;
}

nsTableCell::~nsTableCell()
{
  NS_IF_RELEASE(mRow);
}

// for debugging only
nsrefcnt nsTableCell::AddRef(void)
{
  if (gsNoisyRefs) printf("Add Ref: %x, nsTableCell cnt = %d \n",this, mRefCnt+1);
  return ++mRefCnt;
}

// for debugging only
nsrefcnt nsTableCell::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Release: %x, nsTableCell cnt = %d \n",this, mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE) printf("Delete: %x, nsTableCell \n",this);
    delete this;
    return 0;
  }
  return mRefCnt;
}

int nsTableCell::GetType()
{
  return nsITableContent::kTableCellType;
}

int nsTableCell::GetRowSpan ()
{
  NS_ASSERTION(0<mRowSpan, "bad row span");
  return mRowSpan;
}

int nsTableCell::GetColSpan ()
{
  NS_ASSERTION(0<mColSpan, "bad col span");
  return mColSpan;
}

nsTableRow * nsTableCell::GetRow ()
{
  NS_IF_ADDREF(mRow);
  return mRow;
}

void nsTableCell::SetRow (nsTableRow * aRow)
{
  NS_IF_RELEASE(mRow);
  mRow = aRow;
  NS_IF_ADDREF(aRow);
}

int nsTableCell::GetColIndex ()
{
  NS_ASSERTION(0<=mColIndex, "bad column index");
  return mColIndex;
}

void nsTableCell::SetColIndex (int aColIndex)
{
  NS_ASSERTION(0<=aColIndex, "illegal negative column index.");
  mColIndex = aColIndex;
}

nsresult
nsTableCell::CreateFrame(nsIPresContext* aPresContext,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableCellFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return rv;
}

void nsTableCell::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  NS_PRECONDITION(nsnull!=aAttribute, "bad attribute arg");
  nsHTMLValue val;
  if ((aAttribute == nsHTMLAtoms::align) &&
      ParseDivAlignParam(aValue, val)) {
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  if (aAttribute == nsHTMLAtoms::background) {
    nsAutoString href(aValue);
    href.StripWhitespace();
    nsHTMLTagContent::SetAttribute(aAttribute, href);
    return;
  }
  if (aAttribute == nsHTMLAtoms::bgcolor) {
    ParseColor(aValue, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  if (aAttribute == nsHTMLAtoms::rowspan) {
    ParseValue(aValue, 1, 10000, val, eHTMLUnit_Integer);
    SetRowSpan(val.GetIntValue());
    return;
  }
  if (aAttribute == nsHTMLAtoms::colspan) {
    ParseValue(aValue, 1, 1000, val, eHTMLUnit_Integer);
    SetColSpan(val.GetIntValue());
    return;
  }

  // Use default attribute catching code
  nsTableContent::SetAttribute(aAttribute, aValue);
}



void nsTableCell::MapAttributesInto(nsIStyleContext* aContext,
                                    nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  nsHTMLValue value;

  // align: enum
  GetAttribute(nsHTMLAtoms::align, value);
  if (value.GetUnit() == eHTMLUnit_Enumerated) 
  {
    nsStyleText* text = (nsStyleText*)aContext->GetData(eStyleStruct_Text);
    text->mTextAlign = value.GetIntValue();
  }
  MapBackgroundAttributesInto(aContext, aPresContext);

}

void nsTableCell::SetRowSpan(int aRowSpan)
{
  NS_ASSERTION(0<aRowSpan, "bad row span");
  int oldSpan = mRowSpan;
  mRowSpan = aRowSpan;
  if (mRowSpan != oldSpan)
    ResetCellMap ();
}

void nsTableCell::SetColSpan (int aColSpan)
{
  NS_ASSERTION(0<aColSpan, "bad col span");
  int oldSpan = mColSpan;
  mColSpan = aColSpan;
  if (mColSpan != oldSpan)
    ResetCellMap ();
}

void nsTableCell::ResetCellMap ()
{
  if (nsnull != mRow)
    mRow->ResetCellMap ();
}

nsresult
NS_NewTableCellPart(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new nsTableCell(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
