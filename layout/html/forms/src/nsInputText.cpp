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
#include "nsITextAreaWidget.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsString.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLForms.h"

class nsInputTextFrame : public nsInputFrame {
public:
  nsInputTextFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
                   nsIFrame* aParentFrame);

  virtual nsInputWidgetData* GetWidgetInitData();

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

protected:

  virtual ~nsInputTextFrame();
  
  nsInputTextType GetTextType();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsSize& aMaxSize,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
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

const nsIID&
nsInputTextFrame::GetIID()
{
  static NS_DEFINE_IID(kTextIID, NS_ITEXTWIDGET_IID);
  static NS_DEFINE_IID(kTextAreaIID, NS_ITEXTAREAWIDGET_IID);

  if (kInputTextArea == GetTextType()) {
    return kTextAreaIID;
  }
  else {
    return kTextIID;
  }
}
  
const nsIID&
nsInputTextFrame::GetCID()
{
  static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
  static NS_DEFINE_IID(kTextAreaCID, NS_TEXTAREA_CID);

  if (kInputTextArea == GetTextType()) {
    return kTextAreaCID;
  }
  else {
    return kTextCID;
  }
}

nsInputTextType
nsInputTextFrame::GetTextType()
{
  nsInputText* content;
  GetContent((nsIContent *&) content);
  nsInputTextType type = content->GetTextType();
  NS_RELEASE(content);
  return type;
}

void 
nsInputTextFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                 const nsSize& aMaxSize,
                                 nsReflowMetrics& aDesiredLayoutSize,
                                 nsSize& aDesiredWidgetSize)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(*aPresContext, aMaxSize, styleSize);

  nsSize size;
  
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  if ((kInputTextText == GetTextType()) || (kInputTextPassword == GetTextType())) {
    nsInputDimensionSpec textSpec(nsHTMLAtoms::size, PR_FALSE, nsHTMLAtoms::value,
                                  20, PR_FALSE, nsnull, 1);
    CalculateSize(aPresContext, this, styleSize, textSpec, size, 
                  widthExplicit, heightExplicit, ignore);
  } 
  else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 10, 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSize(aPresContext, this, styleSize, areaSpec, size, 
                  widthExplicit, heightExplicit, ignore);
  }
  if (!heightExplicit && (kInputTextArea != GetTextType())) {
    size.height += 100;
  }

  aDesiredLayoutSize.width  = size.width;
  aDesiredLayoutSize.height = size.height;
  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
}

nsInputWidgetData*
nsInputTextFrame::GetWidgetInitData()
{
  static NS_DEFINE_IID(kTextIID, NS_ITEXTWIDGET_IID);

  nsInputWidgetData* data = nsnull;
  nsInputText* content;
  GetContent((nsIContent *&) content);
  if (kInputTextPassword == content->GetTextType()) {
    data = new nsInputWidgetData();
    data->arg1 = 1;
  }
  NS_RELEASE(content);

  return data;
}

void 
nsInputTextFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsITextWidget* text;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&text)) {
	  text->SetFont(GetFont(aPresContext));
    nsInputText* content;
    GetContent((nsIContent *&) content);
    nsAutoString valAttr;
    nsContentAttr valStatus = ((nsInput *)content)->GetAttribute(nsHTMLAtoms::value, valAttr);
    text->SetText(valAttr);
    NS_RELEASE(text);
    NS_RELEASE(content);
  }
}

//----------------------------------------------------------------------
// nsInputText

nsInputText::nsInputText(nsIAtom* aTag, nsIFormManager* aManager, nsInputTextType aType)
  : nsInput(aTag, aManager), mType(aType)
{
  mMaxLength = ATTR_NOTSET;
  mNumRows   = ATTR_NOTSET;
  mNumCols   = ATTR_NOTSET;
}

nsInputText::~nsInputText()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}

nsInputTextType
nsInputText::GetTextType() const
{
  return mType;
}

nsIFrame* 
nsInputText::CreateFrame(nsIPresContext* aPresContext,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
{
  return new nsInputTextFrame(this, aIndexInParent, aParentFrame);
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
  if (kInputTextText == mType) {
    aResult = "text";
  } 
  else if (kInputTextPassword == mType) {
    aResult = "password";
  }
  else if (kInputTextArea == mType) {   
    aResult = "";
  }
}

void nsInputText::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::maxlength) {
    CacheAttribute(aValue, ATTR_NOTSET, mMaxLength);
  }
  else if ((aAttribute == nsHTMLAtoms::rows) && (kInputTextArea == mType)) {
    CacheAttribute(aValue, ATTR_NOTSET, mNumRows);
  }
  else if ((aAttribute == nsHTMLAtoms::cols) && (kInputTextArea == mType)) {
    CacheAttribute(aValue, ATTR_NOTSET, mNumCols);
  }
  else {
    super::SetAttribute(aAttribute, aValue);
  }
}

nsContentAttr nsInputText::GetAttribute(nsIAtom* aAttribute,
                                        nsHTMLValue& aResult) const
{
  if (aAttribute == nsHTMLAtoms::maxlength) {
    return GetCacheAttribute(mMaxLength, aResult, eHTMLUnit_Integer);
  } 
  else if ((aAttribute == nsHTMLAtoms::rows) && (kInputTextArea == mType)) {
    return GetCacheAttribute(mNumRows, aResult, eHTMLUnit_Integer);
  }
  else if ((aAttribute == nsHTMLAtoms::cols) && (kInputTextArea == mType)) {
    return GetCacheAttribute(mNumCols, aResult, eHTMLUnit_Integer);
  }
  else {
    return super::GetAttribute(aAttribute, aResult);
  }
}

nsresult
NS_NewHTMLInputText(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputText(aTag, aManager, kInputTextText);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsresult
NS_NewHTMLInputPassword(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputText(aTag, aManager, kInputTextPassword);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsresult
NS_NewHTMLTextArea(nsIHTMLContent** aInstancePtrResult,
                   nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputText(aTag, aManager, kInputTextArea);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}
