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
#include "nsTableColFrame.h"
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

nsTableColFrame::nsTableColFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsFrame(aContent, aParentFrame)
{
}


nsTableColFrame::~nsTableColFrame()
{
}

NS_METHOD nsTableColFrame::Paint(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect)
{
  if (gsDebug==PR_TRUE)
    printf("nsTableColFrame::Paint\n");
  return NS_OK;
}


NS_METHOD nsTableColFrame::Reflow(nsIPresContext*      aPresContext,
                                  nsReflowMetrics&     aDesiredSize,
                                  const nsReflowState& aReflowState,
                                  nsReflowStatus&      aStatus)
{
  NS_ASSERTION(nsnull!=aPresContext, "bad arg");
  aDesiredSize.width=0;
  aDesiredSize.height=0;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    aDesiredSize.maxElementSize->width=0;
    aDesiredSize.maxElementSize->height=0;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRInt32 nsTableColFrame::GetColumnIndex()
{ 
  if (nsnull!=mContent)
    return ((nsTableCol *)mContent)->GetColumnIndex();
  else
    return 0;
}


/* ----- static methods ------ */

nsresult nsTableColFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                   nsIContent* aContent,
                                   nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableColFrame(aContent, aParent);
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
  Init();
}

nsTableCol::nsTableCol()
: nsTableContent(NS_NewAtom(nsTablePart::kColTagString)),
  mColGroup(0),
  mColIndex(0),
  mRepeat(0)
{
  Init();
}

nsTableCol::nsTableCol (PRBool aImplicit)
  : nsTableContent(NS_NewAtom(nsTablePart::kColTagString)),
  mColGroup(0),
  mColIndex(0),
  mRepeat(0)
{
  mImplicit = aImplicit;
  Init();
}

void nsTableCol::Init()
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

void nsTableCol::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  nsHTMLValue val;

  if (aAttribute == nsHTMLAtoms::width) 
  {
    ParseValueOrPercentOrProportional(aValue, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if ( aAttribute == nsHTMLAtoms::repeat)
  {
    ParseValue(aValue, 0, val, eHTMLUnit_Integer);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    SetRepeat(val.GetIntValue());
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

void nsTableCol::MapAttributesInto(nsIStyleContext* aContext,
                                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");
  if (nsnull != mAttributes) {

    float p2t;
    nsHTMLValue value;
    nsStyleText* textStyle = nsnull;

    // width
    GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() != eHTMLUnit_Null) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
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
nsTableCol::CreateFrame(nsIPresContext* aPresContext,
                        nsIFrame* aParentFrame,
                        nsIStyleContext* aStyleContext,
                        nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableColFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
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
