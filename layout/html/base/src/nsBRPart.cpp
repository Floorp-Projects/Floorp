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
#include "nsHTMLParts.h"
#include "nsHTMLTagContent.h"
#include "nsIAtom.h"
#include "nsFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsBlockFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsIStyleContext.h"
#include "nsIFontMetrics.h"

static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);

class BRFrame : public nsFrame
{
public:
  BRFrame(nsIContent* aContent,
          PRInt32 aIndexInParent,
          nsIFrame* aParentFrame);
  NS_IMETHOD ResizeReflow(nsIPresContext* aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize& aMaxSize,
                          nsSize* aMaxElementSize,
                          ReflowStatus& aStatus);
  NS_IMETHOD GetReflowMetrics(nsIPresContext*  aPresContext,
                              nsReflowMetrics& aMetrics);

protected:
  virtual ~BRFrame();
};

BRFrame::BRFrame(nsIContent* aContent,
                       PRInt32 aIndexInParent,
                       nsIFrame* aParentFrame)
  : nsFrame(aContent, aIndexInParent, aParentFrame)
{
}

BRFrame::~BRFrame()
{
}

NS_METHOD BRFrame::GetReflowMetrics(nsIPresContext* aPresContext, nsReflowMetrics& aMetrics)
{
  // We have no width, but we're the height of the default font
  nsStyleFont* font =
    (nsStyleFont*)mStyleContext->GetData(kStyleFontSID);
  nsIFontMetrics* fm = aPresContext->GetMetricsFor(font->mFont);
  aMetrics.width = 0;
  aMetrics.height = fm->GetHeight();
  aMetrics.ascent = fm->GetMaxAscent();
  aMetrics.descent = fm->GetMaxDescent();
  NS_RELEASE(fm);
  return NS_OK;
}

NS_METHOD BRFrame::ResizeReflow(nsIPresContext* aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize& aMaxSize,
                                nsSize* aMaxElementSize,
                                ReflowStatus& aStatus)
{
  // Get cached state for containing block frame
  nsBlockReflowState* state = nsnull;
  nsIFrame* parent = mGeometricParent;
  while (nsnull != parent) {
    nsIHTMLFrameType* ft;
    nsresult status = parent->QueryInterface(kIHTMLFrameTypeIID, (void**) &ft);
    if (NS_OK == status) {
      nsHTMLFrameType type = ft->GetFrameType();
      if (eHTMLFrame_Block == type) {
        break;
      }
    }
    parent->GetGeometricParent(parent);
  }
  if (nsnull != parent) {
    nsIPresShell* shell = aPresContext->GetShell();
    state = (nsBlockReflowState*) shell->GetCachedData(parent);
    NS_RELEASE(shell);
  }
  if (nsnull != state) {
    // XXX <BR clear=...>
    state->breakAfterChild = PR_TRUE;
  }

  GetReflowMetrics(aPresContext, aDesiredSize);
  aStatus = frComplete;
  return NS_OK;
}

//----------------------------------------------------------------------

class BRPart : public nsHTMLTagContent {
public:
  BRPart(nsIAtom* aTag);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;

  virtual void UnsetAttribute(nsIAtom* aAttribute);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  PRInt32 GetClear() {
    return mClear;
  }

  void SetClear(PRInt32 aValue) {
    mClear = aValue;
  }

protected:
  virtual ~BRPart();
  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;

  PRInt32 mClear;
};

BRPart::BRPart(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
}

BRPart::~BRPart()
{
}

nsIFrame* BRPart::CreateFrame(nsIPresContext* aPresContext,
                              PRInt32 aIndexInParent,
                              nsIFrame* aParentFrame)
{
  nsIFrame* rv = new BRFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

//----------------------------------------------------------------------
// Attributes

static nsHTMLTagContent::EnumTable kClearTable[] = {
  { "left", NS_STYLE_CLEAR_LEFT },
  { "right", NS_STYLE_CLEAR_RIGHT },
  { "all", NS_STYLE_CLEAR_BOTH },
  { "both", NS_STYLE_CLEAR_BOTH },
  { 0 }
};

void BRPart::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  if (aAttribute == nsHTMLAtoms::clear) {
    nsHTMLValue value;
    if (ParseEnumValue(aString, kClearTable, value)) {
      mClear = value.GetIntValue();
    }
    else {
      mClear = NS_STYLE_CLEAR_NONE;
    }
    return;
  }
  nsHTMLTagContent::SetAttribute(aAttribute, aString);
}

nsContentAttr BRPart::GetAttribute(nsIAtom* aAttribute,
                                   nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::clear) {
    aResult.Reset();
    if (NS_STYLE_CLEAR_NONE != mClear) {
      aResult.Set(mClear, eHTMLUnit_Enumerated);
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsHTMLTagContent::GetAttribute(aAttribute, aResult);
  }
  return ca;
}

void BRPart::UnsetAttribute(nsIAtom* aAttribute)
{
  if (aAttribute == nsHTMLAtoms::clear) {
    mClear = NS_STYLE_CLEAR_NONE;
  }
}

nsContentAttr BRPart::AttributeToString(nsIAtom* aAttribute,
                                        nsHTMLValue& aValue,
                                        nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::clear) {
    if ((eHTMLUnit_Enumerated == aValue.GetUnit()) &&
        (NS_STYLE_CLEAR_NONE != aValue.GetIntValue())) {
      EnumValueToString(aValue, kClearTable, aResult);
      return eContentAttr_HasValue;
    }
  }
  return eContentAttr_NotThere;
}

//----------------------------------------------------------------------

nsresult
NS_NewHTMLBreak(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new BRPart(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}
