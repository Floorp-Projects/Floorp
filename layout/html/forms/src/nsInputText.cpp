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

#include "nsInputText.h"
#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsString.h"
#include "nsHTMLAtoms.h"

class nsInputTextFrame : public nsInputFrame {
public:
  nsInputTextFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
                   nsIFrame* aParentFrame);

  virtual void PreInitializeWidget(nsIPresContext* aPresContext, 
	                               nsSize& aBounds);

  virtual void InitializeWidget(nsIView *aView);

  virtual const nsIID GetCID();

  virtual const nsIID GetIID();

protected:
  virtual ~nsInputTextFrame();
  nsString mCacheValue;
};

nsInputTextFrame::nsInputTextFrame(nsIContent* aContent,
                                   PRInt32 aIndexInParent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsInputTextFrame::~nsInputTextFrame()
{
}

const nsIID
nsInputTextFrame::GetIID()
{
  static NS_DEFINE_IID(kTextIID, NS_ITEXTWIDGET_IID);
  return kTextIID;
}
  
const nsIID
nsInputTextFrame::GetCID()
{
  static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
  return kTextCID;
}

void 
nsInputTextFrame::PreInitializeWidget(nsIPresContext* aPresContext, 
                                      nsSize& aBounds)
{
  nsInputText* content = (nsInputText *)mContent; // this must be an nsInputButton 

  // get the value of the text
  if (nsnull != content->mValue) {
    mCacheValue = *content->mValue;
  } else {
    mCacheValue = "";
  }

  float p2t = aPresContext->GetPixelsToTwips();
  aBounds.width  = (int)(120 * p2t);
  aBounds.height = (int)(20 * p2t);
}

void 
nsInputTextFrame::InitializeWidget(nsIView *aView)
{
  nsITextWidget* text;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&text)) {
	text->SetText(mCacheValue);
    NS_RELEASE(text);
  }
}

//----------------------------------------------------------------------
// nsInputText

nsInputText::nsInputText(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager)
{
}

nsInputText::~nsInputText()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}

nsIFrame* 
nsInputText::CreateFrame(nsIPresContext* aPresContext,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
{
  nsIFrame* rv = new nsInputTextFrame(this, aIndexInParent, aParentFrame);
  printf("** nsInputText::CreateFrame frame=%d", rv);
  return rv;
}

PRInt32 
nsInputText::GetMaxNumValues()
{
  return 1;
}
  
PRBool
nsInputText::GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                       nsString* aValues)
{
  if (aMaxNumValues <= 0) {
    return PR_FALSE;
  }
  nsITextWidget* text = (nsITextWidget *)GetWidget();
  nsString value;
  text->GetText(aValues[0], 0);  // the last parm is not used 
  aNumValues = 1;

  return PR_TRUE;
}


void 
nsInputText::Reset() 
{
  nsITextWidget* text = (nsITextWidget *)GetWidget();
  if (nsnull == mValue) {
    text->SetText("");
  } else {
    text->SetText(*mValue);
  }
}  

void nsInputText::GetType(nsString& aResult) const
{
  aResult = "text";
}

void nsInputText::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::size) {
    nsHTMLValue value;
    ParseValue(aValue, 1, value);
    mSize = value.GetIntValue();
  }
  else if (aAttribute == nsHTMLAtoms::maxlength) {
    nsHTMLValue value;
    ParseValue(aValue, 1, value);/* XXX nav doesn't clamp; what does it do with illegal values? */
    mMaxLength = value.GetIntValue();
  }
  else if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull == mValue) {
      mValue = new nsString(aValue);
    } else {
      *mValue = aValue;
    }
  }
  else {
    nsInput::SetAttribute(aAttribute, aValue);
  }
}

nsContentAttr nsInputText::GetAttribute(nsIAtom* aAttribute,
                                        nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::size) {
    if (0 < mSize) {
      aResult.Set(mSize, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::maxlength) {
    if (0 < mMaxLength) {
      aResult.Set(mMaxLength, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull != mValue) {
      aResult.Set(*mValue);
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsInput::GetAttribute(aAttribute, aResult);
  }
  return ca;
}

NS_HTML nsresult
NS_NewHTMLInputText(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputText(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

