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

#include "nsInput.h"
#include "nsInputFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLTagContent.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsIButton.h"
#include "nsIViewManager.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIButton.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontCache.h"
#include "nsIFontMetrics.h"
#include "nsIFormManager.h"

static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);

enum nsInputButtonType {
  kInputButtonButton,
  kInputButtonReset,
  kInputButtonSubmit
};
	
class nsInputButton : public nsInput {
public:
  nsInputButton (nsIAtom* aTag, nsIFormManager* aManager,
                 nsInputButtonType aType);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;

  virtual void GetDefaultLabel(nsString& aLabel);

  nsInputButtonType GetButtonType() { return mType; }

  virtual PRInt32 GetMaxNumValues(); 

  virtual PRBool GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, nsString* aValues);

  // Attributes
  nsString* mValue;
  nscoord mWidth;
  nscoord mHeight;

 protected:
  virtual ~nsInputButton();

  virtual void GetType(nsString& aResult) const;

  nsInputButtonType mType;
};

class nsInputButtonFrame : public nsInputFrame {
public:
  nsInputButtonFrame(nsIContent* aContent,
                     PRInt32 aIndexInParent,
                     nsIFrame* aParentFrame);

  virtual void PreInitializeWidget(nsIPresContext* aPresContext, 
	                               nsSize& aBounds);

  virtual void InitializeWidget(nsIView* aView);

  virtual void MouseClicked();

  virtual const nsIID GetCID();

  virtual const nsIID GetIID();


protected:
  virtual      ~nsInputButtonFrame();
  nsString     mCacheLabel;     
  nsFont*      mCacheFont;  // XXX this is bad, the lifetime of the font is not known or controlled
};


// nsInputButton Implementation

nsInputButton::nsInputButton(nsIAtom* aTag, nsIFormManager* aManager,
                             nsInputButtonType aType)
  : nsInput(aTag, aManager), mType(aType) 
{
  mWidth = -1;
  mHeight = -1;
}

nsInputButton::~nsInputButton()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}


void nsInputButton::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull == mValue) {
      mValue = new nsString(aValue);
    } else {
      mValue->SetLength(0);
      mValue->Append(aValue);
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    nsHTMLValue value;
    ParseValue(aValue, 0, 1000, value);
    mWidth = value.GetIntValue();
  }
  else if (aAttribute == nsHTMLAtoms::height) {
    nsHTMLValue value;
    ParseValue(aValue, 0, 1000, value);
    mHeight = value.GetIntValue();
  }
  else {
    nsInput::SetAttribute(aAttribute, aValue);
  }
}

nsContentAttr nsInputButton::GetAttribute(nsIAtom* aAttribute,
                                          nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::value) {
    if (nsnull != mValue) {
      aResult.Set(*mValue);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    if (0 <= mWidth) {
      aResult.Set(mWidth, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::height) {
    if (0 <= mHeight) {
      aResult.Set(mHeight, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsInput::GetAttribute(aAttribute, aResult);
  }
  return ca;
}

void nsInputButton::GetType(nsString& aResult) const
{
  aResult.SetLength(0);
  switch (mType) {
  case kInputButtonButton:
    aResult.Append("button");
    break;
  case kInputButtonReset:
    aResult.Append("reset");
    break;
  default:
  case kInputButtonSubmit:
    aResult.Append("submit");
    break;
  }
}

void
nsInputButton::GetDefaultLabel(nsString& aString) 
{
  if (kInputButtonReset == mType) {
    aString = "Reset";
  } else if (kInputButtonSubmit == mType) {
    aString = "Submit";
  } else {
    aString = "noname";
  }
}

nsIFrame* 
nsInputButton::CreateFrame(nsIPresContext* aPresContext,
                           PRInt32 aIndexInParent,
                           nsIFrame* aParentFrame)
{
  nsIFrame* rv = new nsInputButtonFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

PRInt32
nsInputButton::GetMaxNumValues() 
{
  if (kInputButtonSubmit == mType) {
    return 1;
  } else {
	return 0;
  }
}


PRBool
nsInputButton::GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                         nsString* aValues)
{
  if (aMaxNumValues <= 0) {
    return PR_FALSE;
  }

  if (kInputButtonSubmit != mType) {
    aNumValues = 0;
    return PR_FALSE;
  }

  if (nsnull != mValue) {
    aValues[0].SetLength(0);
    aValues[0].Append(*mValue);
    aNumValues = 1;
    return PR_TRUE;
  } else {
    aNumValues = 0;
    return PR_FALSE;
  }
}

//----------------------------------------------------------------------
// nsInputButtonFrame Implementation

nsInputButtonFrame::nsInputButtonFrame(nsIContent* aContent,
                           PRInt32 aIndexInParent,
                           nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aIndexInParent, aParentFrame)
{
  mCacheLabel = "";
  mCacheFont  = nsnull;
}

nsInputButtonFrame::~nsInputButtonFrame()
{
  mCacheLabel = "";       
  mCacheFont  = nsnull;  // should this be released? NO
}

void
nsInputButtonFrame::MouseClicked() 
{
  nsInputButton* button = (nsInputButton *)mContent;
  nsIFormManager* formMan = button->GetFormManager();
  if (nsnull != formMan) {
    if (kInputButtonReset == button->GetButtonType()) {
      formMan->OnReset();
    } else if (kInputButtonSubmit == button->GetButtonType()) {
      formMan->OnSubmit();
    }
    NS_RELEASE(formMan);
  }
}


void 
nsInputButtonFrame::PreInitializeWidget(nsIPresContext* aPresContext, nsSize& aBounds)
{
  float p2t = aPresContext->GetPixelsToTwips();

  nsInputButton* content = (nsInputButton *)mContent; // this must be an nsInputButton 
  // should this be the parent
  nsStyleFont* styleFont = (nsStyleFont*)mStyleContext->GetData(kStyleFontSID);
  nsIDeviceContext* deviceContext = aPresContext->GetDeviceContext();
  nsIFontCache* fontCache = deviceContext->GetFontCache();

  // get the label for the button
  nsAutoString label;
  if (nsnull == content->mValue) {
    content->GetDefaultLabel(label);
  } else {
    label.Append(*content->mValue);
  }
  mCacheLabel = label;  // cache the label

  // get the width and height based on the label, font, padding
  nsIFontMetrics* fontMet = fontCache->GetMetricsFor(styleFont->mFont);
  nscoord horPadding = (int) (5.0 * p2t);  // need to get this from the widget
  nscoord verPadding = (int) (5.0 * p2t);  // need to get this from the widget
  aBounds.width  = fontMet->GetWidth(label) + (2 * horPadding);
  aBounds.height = fontMet->GetHeight() + (2 * verPadding);
  
  mCacheFont = &(styleFont->mFont); // cache the font  XXX this is bad, the lifetime of the font is not known or controlled

  NS_RELEASE(fontMet);
  NS_RELEASE(fontCache);
  NS_RELEASE(deviceContext);
}


void 
nsInputButtonFrame::InitializeWidget(nsIView *aView)
{
  nsIButton* button;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&button)) {
	if (nsnull != mCacheFont) {
	  button->SetFont(*mCacheFont);
	}
	button->SetLabel(mCacheLabel);
    NS_RELEASE(button);
  }
}

const nsIID
nsInputButtonFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID
nsInputButtonFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

nsresult
CreateButton(nsIHTMLContent** aInstancePtrResult,
             nsIAtom* aTag, nsIFormManager* aManager,
             nsInputButtonType aType)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputButton(aTag, aManager, aType);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsresult
NS_NewHTMLInputButton(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kInputButtonButton);
}

nsresult
NS_NewHTMLInputSubmit(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kInputButtonSubmit);
}

nsresult
NS_NewHTMLInputReset(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kInputButtonReset);
}
