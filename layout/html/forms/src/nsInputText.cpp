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
#include "nsIStyleContext.h"
#include "nsFont.h"
#include "nsDOMEvent.h"

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);

class nsInputTextFrame : public nsInputFrame {
public:
  nsInputTextFrame(nsIContent* aContent,
                   nsIFrame* aParentFrame);

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual void EnterPressed(nsIPresContext& aPresContext) ;

  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
protected:

  virtual ~nsInputTextFrame();
  
  nsInputTextType GetTextType();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

nsresult
NS_NewInputTextFrame(nsIContent* aContent,
                     nsIFrame*   aParent,
                     nsIFrame*&  aResult)
{
  aResult = new nsInputTextFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsInputTextFrame::nsInputTextFrame(nsIContent* aContent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsInputTextFrame::~nsInputTextFrame()
{
}

nscoord nsInputTextFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(4, aPixToTwip);
}

nscoord nsInputTextFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

// for a text area aInnerHeight is the height of one line
nscoord nsInputTextFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                                   nscoord aInnerHeight) const
{
#ifdef XP_PC
  nsAutoString type;
  ((nsInput*)mContent)->GetType(type);
  if (type.EqualsIgnoreCase("textarea")) {
    return (nscoord)NSToIntRound(float(aInnerHeight) * 0.40f);
  } else {
    return (nscoord)NSToIntRound(float(aInnerHeight) * 0.25f);
  }
#endif
#ifdef XP_UNIX
  return NSIntPixelsToTwips(10, aPixToTwip); // XXX this is probably wrong
#endif
}

nscoord nsInputTextFrame::GetHorizontalInsidePadding(float aPixToTwip, 
                                                     nscoord aInnerWidth,
                                                     nscoord aCharWidth) const
{
#ifdef XP_PC
  nscoord padding;
  nsAutoString type;
  ((nsInput*)mContent)->GetType(type);
  if (type.EqualsIgnoreCase("textarea")) {
    padding = (nscoord)(40 * aCharWidth / 100);
  } else {
    padding = (nscoord)(55 * aCharWidth / 100);
  }
  nscoord min = NSIntPixelsToTwips(3, aPixToTwip);
  if (padding > min) {
    return padding;
  } else {
    return min;
  }
#endif
#ifdef XP_UNIX
  return NSIntPixelsToTwips(6, aPixToTwip);  // XXX this is probably wrong
#endif
}

static NS_DEFINE_IID(kTextIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kTextAreaIID, NS_ITEXTAREAWIDGET_IID);


const nsIID&
nsInputTextFrame::GetIID()
{

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
nsInputTextFrame::EnterPressed(nsIPresContext& aPresContext) 
{
  nsInputText* text = (nsInputText *)mContent;
  nsIFormManager* formMan = text->GetFormManager();
  if (nsnull != formMan) {
    nsInputTextType type = text->GetTextType();
    // a form with one text area causes a submit when the enter key is pressed
    // XXX this logic should be in the form manager, but then it needs to be passed
    //     enough to trigger the dom event.
    if ((kInputText_Text == type) && text->GetCanSubmit()) {
      nsEventStatus mStatus = nsEventStatus_eIgnore;
      nsEvent mEvent;
      mEvent.eventStructType = NS_EVENT;
      mEvent.message = NS_FORM_SUBMIT;
      mContent->HandleDOMEvent(aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus); 

      nsIFormControl* control;
      mContent->QueryInterface(kIFormControlIID, (void**)&control);
      formMan->OnSubmit(&aPresContext, this, control);
      NS_IF_RELEASE(control);
    }
    NS_RELEASE(formMan);
  }
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
                (kInputText_FileText == textType)) {
    nsHTMLValue sizeAttr;
    PRInt32 width = 21;
    if (NS_CONTENT_ATTR_HAS_VALUE == ((nsInput*)mContent)->GetAttribute(nsHTMLAtoms::size, sizeAttr)) {
      width = (sizeAttr.GetUnit() == eHTMLUnit_Pixel) ? sizeAttr.GetPixelValue() : sizeAttr.GetIntValue();
      if (kBackwardMode == GetMode()) {
        width += 1;
      }
    }
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, PR_FALSE, nsnull, 1);
  //  nsInputDimensionSpec textSpec(nsHTMLAtoms::size, PR_FALSE, nsnull,
  //                                nsnull, 21, PR_FALSE, nsnull, 1);
    CalculateSize(aPresContext, this, styleSize, textSpec, size, 
                  widthExplicit, heightExplicit, ignore);
  } 
  else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, nsnull, 20, 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSize(aPresContext, this, styleSize, areaSpec, size, 
                  widthExplicit, heightExplicit, ignore);
  }

  if (kInputText_Area == textType) {
    float p2t = aPresContext->GetPixelsToTwips();
    nscoord scrollbarWidth = GetScrollbarWidth(p2t);

    if (!heightExplicit) {
      size.height += scrollbarWidth;
    } 
    if (!widthExplicit) {
      size.width += scrollbarWidth;
    }
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
  nsTextWidgetInitData* data = nsnull;
  nsInputText* content;
  GetContent((nsIContent *&) content);

  if (kInputText_Password == content->GetTextType()) {
    data = new nsTextWidgetInitData();
    data->clipChildren = PR_TRUE;
    data->mIsPassword = PR_TRUE;
  }
  NS_RELEASE(content);

  return data;
}

void 
nsInputTextFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsIWidget* widget = nsnull;
  if (NS_OK == GetWidget(aView, &widget)) {
    nsITextWidget* text = nsnull;
    nsITextAreaWidget* textArea = nsnull;

    // This is replicated code for both text interfaces -- this should be factored
    if (widget != nsnull && NS_OK == widget->QueryInterface(kTextIID,(void**)&text))  
    {
      const nsStyleFont* fontStyle = (const nsStyleFont*)(mStyleContext->GetStyleData(eStyleStruct_Font));
	    widget->SetFont(fontStyle->mFixedFont);
      nsInputText* content;
      GetContent((nsIContent *&) content);
      nsAutoString valAttr;
      nsresult valStatus = ((nsHTMLTagContent *)content)->GetAttribute(nsHTMLAtoms::value, valAttr);
      PRUint32 size;
      text->SetText(valAttr,size);
      PRInt32 maxLength = content->GetMaxLength();
      if (ATTR_NOTSET != maxLength) {
        text->SetMaxTextLength(maxLength);
      }
      widget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
      NS_RELEASE(content);
      NS_RELEASE(text);
    }
    else if (widget != nsnull && NS_OK == widget->QueryInterface(kTextAreaIID,(void**)&textArea))
    {
      const nsStyleFont* fontStyle = (const nsStyleFont*)(mStyleContext->GetStyleData(eStyleStruct_Font));
	    widget->SetFont(fontStyle->mFixedFont);
      nsInputText* content;
      GetContent((nsIContent *&) content);
      nsAutoString valAttr;
      nsresult valStatus = ((nsHTMLTagContent *)content)->GetAttribute(nsHTMLAtoms::value, valAttr);
      PRUint32 size;
      textArea->SetText(valAttr,size);
      PRInt32 maxLength = content->GetMaxLength();
      if (ATTR_NOTSET != maxLength) {
        textArea->SetMaxTextLength(maxLength);
      }
      widget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
      NS_RELEASE(content);
      NS_RELEASE(textArea);
    }
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
}

nsInputTextType
nsInputText::GetTextType() const
{
  return mType;
}

PRInt32 
nsInputText::GetMaxNumValues()
{
  return 1;
}
  
static NS_DEFINE_IID(kITextWidgetIID,     NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);

PRBool
nsInputText::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                            nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
    return PR_FALSE;
  }
  nsIWidget* widget = GetWidget();
  nsITextWidget* text = nsnull;
  nsITextAreaWidget* textArea = nsnull;

  if (widget != nsnull && NS_OK == widget->QueryInterface(kITextWidgetIID,(void**)&text))
  {
    nsString value;
    PRUint32 size;
    text->GetText(aValues[0],0,size);  // the last parm is not used
    aNames[0] = *mName;  
    aNumValues = 1;

    NS_RELEASE(text);
    return PR_TRUE;
  }
  else if (widget != nsnull && NS_OK == widget->QueryInterface(kITextAreaWidgetIID,(void**)&textArea))
  {
    nsString value;
    PRUint32 size;
    textArea->GetText(aValues[0],0,size);  // the last parm is not used
    aNames[0] = *mName;  
    aNumValues = 1;

    NS_RELEASE(textArea);
    return PR_TRUE;
  }
  return PR_FALSE;
}


void 
nsInputText::Reset() 
{
  nsIWidget* widget = GetWidget();
  nsITextWidget* text = nsnull;
  nsITextAreaWidget* textArea = nsnull;
  if (widget != nsnull && NS_OK == widget->QueryInterface(kTextIID,(void**)&text))
  {
    PRUint32 size;
    if (nsnull == mValue) {
      text->SetText("",size);
    } else {
      text->SetText(*mValue,size);
    }
    NS_RELEASE(text);
  }
  else if (widget != nsnull && NS_OK == widget->QueryInterface(kTextAreaIID,(void**)&textArea))
  {
    PRUint32 size;
    if (nsnull == mValue) {
      textArea->SetText("",size);
    } else {
      textArea->SetText(*mValue,size);
    }
    NS_RELEASE(textArea);
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
    aResult = "textarea";
  }
  else if (kInputText_FileText == mType) {   
    aResult = "filetext";
  }
}

NS_IMETHODIMP
nsInputText::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify)
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
  return nsInputTextSuper::SetAttribute(aAttribute, aValue, aNotify);
}

NS_IMETHODIMP
nsInputText::GetAttribute(nsIAtom* aAttribute,
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
