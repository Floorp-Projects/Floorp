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
                   nsIFrame* aParentFrame);

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual PRInt32 GetVerticalBorderWidth(float aPixToTwip) const;
  virtual PRInt32 GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual PRInt32 GetVerticalInsidePadding(float aPixToTwip,
                                           PRInt32 aInnerHeight) const;
  virtual PRInt32 GetHorizontalInsidePadding(float aPixToTwip, 
                                             PRInt32 aInnerWidth) const;
protected:

  virtual ~nsInputTextFrame();
  
  nsInputTextType GetTextType();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

nsInputTextFrame::nsInputTextFrame(nsIContent* aContent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsInputTextFrame::~nsInputTextFrame()
{
}

PRInt32 nsInputTextFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return (int)(4 * aPixToTwip + 0.5);
}

PRInt32 nsInputTextFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

PRInt32 nsInputTextFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                                     PRInt32 aInnerHeight) const
{
  return (int)(5 * aPixToTwip + 0.5);
}

PRInt32 nsInputTextFrame::GetHorizontalInsidePadding(float aPixToTwip, 
                                                       PRInt32 aInnerWidth) const
{
  return (int)(6 * aPixToTwip + 0.5);
}

const nsIID&
nsInputTextFrame::GetIID()
{
  static NS_DEFINE_IID(kTextIID, NS_ITEXTWIDGET_IID);
  static NS_DEFINE_IID(kTextAreaIID, NS_ITEXTAREAWIDGET_IID);

  if (kInputText_Area == GetTextType()) {
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

  if (kInputText_Area == GetTextType()) {
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
                                 const nsReflowState& aReflowState,
                                 nsReflowMetrics& aDesiredLayoutSize,
                                 nsSize& aDesiredWidgetSize)
{
  nsInputTextType textType = GetTextType();
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(*aPresContext, aReflowState, styleSize);

  nsSize size;
  
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  if ((kInputText_Text == textType) || (kInputText_Password == textType) ||
      (kInputText_FileText == textType)) 
  {
    nsInputDimensionSpec textSpec(nsHTMLAtoms::size, PR_FALSE, nsnull,
                                  nsnull, 21, PR_FALSE, nsnull, 1);
    CalculateSize(aPresContext, this, styleSize, textSpec, size, 
                  widthExplicit, heightExplicit, ignore);
  } 
  else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, nsnull, 20, 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSize(aPresContext, this, styleSize, areaSpec, size, 
                  widthExplicit, heightExplicit, ignore);
  }

  float p2t = aPresContext->GetPixelsToTwips();
  PRInt32 scrollbarWidth = GetScrollbarWidth(p2t);
  if (!heightExplicit) {
    if (kInputText_Area == textType) {
      size.height += scrollbarWidth;
      if (kBackwardMode == GetMode()) {
        size.height += (int)(7 * p2t + 0.5);
      }
    } 
  }

  if (!widthExplicit && (kInputText_Area == textType)) {
    size.width += scrollbarWidth;
  }

  aDesiredLayoutSize.width  = size.width;
  aDesiredLayoutSize.height = size.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
}

nsWidgetInitData*
nsInputTextFrame::GetWidgetInitData(nsIPresContext& aPresContext)
{
  static NS_DEFINE_IID(kTextIID, NS_ITEXTWIDGET_IID);

  nsTextWidgetInitData* data = nsnull;
  nsInputText* content;
  GetContent((nsIContent *&) content);

  if (kInputText_Password == content->GetTextType()) {
    data = new nsTextWidgetInitData();
    data->mIsPassword = PR_TRUE;
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

nsresult
nsInputText::CreateFrame(nsIPresContext* aPresContext,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult)
{
  nsIFrame* frame = new nsInputTextFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

PRInt32 
nsInputText::GetMaxNumValues()
{
  return 1;
}
  
PRBool
nsInputText::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                            nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
    return PR_FALSE;
  }
  nsITextWidget* text = (nsITextWidget *)GetWidget();
  nsString value;
  text->GetText(aValues[0], 0);  // the last parm is not used
  aNames[0] = *mName;
  
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
  if (kInputText_Text == mType) {
    aResult = "text";
  } 
  else if (kInputText_Password == mType) {
    aResult = "password";
  }
  else if (kInputText_Area == mType) {   
    aResult = "";
  }
  else if (kInputText_FileText == mType) {   
    aResult = "filetext";
  }
}

void nsInputText::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::maxlength) {
    CacheAttribute(aValue, ATTR_NOTSET, mMaxLength);
  }
  else if ((aAttribute == nsHTMLAtoms::rows) && (kInputText_Area == mType)) {
    CacheAttribute(aValue, ATTR_NOTSET, mNumRows);
  }
  else if ((aAttribute == nsHTMLAtoms::cols) && (kInputText_Area == mType)) {
    CacheAttribute(aValue, ATTR_NOTSET, mNumCols);
  }
  nsInputTextSuper::SetAttribute(aAttribute, aValue);
}

nsContentAttr nsInputText::GetAttribute(nsIAtom* aAttribute,
                                        nsHTMLValue& aResult) const
{
  if (aAttribute == nsHTMLAtoms::maxlength) {
    return GetCacheAttribute(mMaxLength, aResult, eHTMLUnit_Integer);
  } 
  else if ((aAttribute == nsHTMLAtoms::rows) && (kInputText_Area == mType)) {
    return GetCacheAttribute(mNumRows, aResult, eHTMLUnit_Integer);
  }
  else if ((aAttribute == nsHTMLAtoms::cols) && (kInputText_Area == mType)) {
    return GetCacheAttribute(mNumCols, aResult, eHTMLUnit_Integer);
  }
  else {
    return nsInputTextSuper::GetAttribute(aAttribute, aResult);
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

  nsIHTMLContent* it = new nsInputText(aTag, aManager, kInputText_Text);

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

  nsIHTMLContent* it = new nsInputText(aTag, aManager, kInputText_Password);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsresult
NS_NewHTMLInputFileText(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputText(aTag, aManager, kInputText_FileText);

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

  nsIHTMLContent* it = new nsInputText(aTag, aManager, kInputText_Area);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}
