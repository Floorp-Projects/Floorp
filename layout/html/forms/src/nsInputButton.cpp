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
#include "nsHTMLContainer.h"
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

enum nsInputButtonType {
  kButton_InputButton,
  kButton_Button,
  kButton_InputReset,
  kButton_InputSubmit,
  kButton_InputHidden
};
	
class nsInputButton : public nsInput {
public:
  nsInputButton (nsIAtom* aTag, nsIFormManager* aManager,
                 nsInputButtonType aType);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual void GetDefaultLabel(nsString& aLabel);

  nsInputButtonType GetButtonType() { return mType; }

  virtual PRInt32 GetMaxNumValues(); 

  virtual PRBool GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, nsString* aValues);

  virtual PRBool IsHidden();

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

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView* aView);

  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual const nsIID GetCID();

  virtual const nsIID GetIID();

  nsInputButtonType GetButtonType() const;


protected:
  virtual  ~nsInputButtonFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsSize& aMaxSize,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};


// nsInputButton Implementation

nsInputButton::nsInputButton(nsIAtom* aTag, nsIFormManager* aManager,
                             nsInputButtonType aType)
  : nsInput(aTag, aManager), mType(aType) 
{
}

nsInputButton::~nsInputButton()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}

PRBool 
nsInputButton::IsHidden()
{
  if (kButton_InputHidden == mType) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

void nsInputButton::GetType(nsString& aResult) const
{
  aResult.SetLength(0);
  switch (mType) {
  case kButton_InputButton:
  case kButton_Button:
    aResult.Append("button");
    break;
  case kButton_InputReset:
    aResult.Append("reset");
    break;
  default:
  case kButton_InputSubmit:
    aResult.Append("submit");
    break;
  }
}

void
nsInputButton::GetDefaultLabel(nsString& aString) 
{
  if (kButton_InputReset == mType) {
    aString = "Reset";
  } else if (kButton_InputSubmit == mType) {
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
  if (kButton_InputHidden == mType) {
    nsIFrame* frame;
    nsFrame::NewFrame(&frame, this, aIndexInParent, aParentFrame);
    return frame;
  } 
  else {
   return new nsInputButtonFrame(this, aIndexInParent, aParentFrame);
  }
 
}

PRInt32
nsInputButton::GetMaxNumValues() 
{
  if ((kButton_InputSubmit == mType) || (kButton_InputHidden)) {
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

  if ((kButton_InputSubmit != mType) && (kButton_InputHidden != mType)) {
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
}

nsInputButtonFrame::~nsInputButtonFrame()
{
}

nsInputButtonType 
nsInputButtonFrame::GetButtonType() const
{
  nsInputButton* button = (nsInputButton *)mContent;
  return button->GetButtonType();
}

void
nsInputButtonFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  nsInputButton* button = (nsInputButton *)mContent;
  nsIFormManager* formMan = button->GetFormManager();
  if (nsnull != formMan) {
    if (kButton_InputReset == button->GetButtonType()) {
      formMan->OnReset();
    } else if (kButton_InputSubmit == button->GetButtonType()) {
      //NS_ADDREF(this);
      formMan->OnSubmit(aPresContext, this);
      //NS_RELEASE(this);
    }
    NS_RELEASE(formMan);
  }
}

void 
nsInputButtonFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                   const nsSize& aMaxSize,
                                   nsReflowMetrics& aDesiredLayoutSize,
                                   nsSize& aDesiredWidgetSize)
{
  if (kButton_InputHidden == GetButtonType()) {
    aDesiredLayoutSize.width  = 0;
    aDesiredLayoutSize.height = 0;
    aDesiredWidgetSize.width  = 0;
    aDesiredWidgetSize.height = 0;
    return;
  }
  nsSize styleSize;
  GetStyleSize(*aPresContext, aMaxSize, styleSize);

  nsSize size;
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  nsInputDimensionSpec spec(nsHTMLAtoms::size, PR_TRUE, nsHTMLAtoms::value, 1,
                            PR_FALSE, nsnull, 1);
  CalculateSize(aPresContext, this, styleSize, spec, size, 
                widthExplicit, heightExplicit, ignore);

  if (!widthExplicit) {
    size.width += 100;
  } 
  if (!heightExplicit) {
    size.height += 100;
  } 

  aDesiredLayoutSize.width = size.width;
  aDesiredLayoutSize.height= size.height;
  aDesiredWidgetSize.width = size.width;
  aDesiredWidgetSize.height= size.height;
}


void 
nsInputButtonFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsIButton* button;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&button)) {
	  button->SetFont(GetFont(aPresContext));
	} 
  else {
    NS_ASSERTION(0, "no widget in button control");
  }

  nsInputButton* content;
  GetContent((nsIContent*&) content);
  nsString value;
  nsContentAttr status = content->GetAttribute(nsHTMLAtoms::value, value);
  if (eContentAttr_HasValue == status) {  
	  button->SetLabel(value);
  } 
  else {
    button->SetLabel(" ");
  }

  NS_RELEASE(content);
  NS_RELEASE(button);
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
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_InputButton);
}

nsresult
NS_NewHTMLButton(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_Button);
}

nsresult
NS_NewHTMLInputSubmit(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_InputSubmit);
}

nsresult
NS_NewHTMLInputReset(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_InputReset);
}

nsresult
NS_NewHTMLInputHidden(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_InputHidden);
}
