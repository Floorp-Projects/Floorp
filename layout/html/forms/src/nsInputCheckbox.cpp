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

#include "nsInputCheckbox.h"
#include "nsICheckButton.h"
#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsWidgetsCID.h"
#include "nsIView.h"
#include "nsHTMLAtoms.h"

class nsInputCheckboxFrame : public nsInputFrame {
public:
  nsInputCheckboxFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
                   nsIFrame* aParentFrame);

  virtual void PreInitializeWidget(nsIPresContext* aPresContext, 
	                               nsSize& aBounds);

  virtual void InitializeWidget(nsIView *aView);

  virtual const nsIID GetCID();

  virtual const nsIID GetIID();

  virtual void MouseClicked();

protected:
  virtual ~nsInputCheckboxFrame();
  PRBool   mCacheState;
};

nsInputCheckboxFrame::nsInputCheckboxFrame(nsIContent* aContent,
                                   PRInt32 aIndexInParent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsInputCheckboxFrame::~nsInputCheckboxFrame()
{
  mCacheState = PR_FALSE;
}


const nsIID
nsInputCheckboxFrame::GetIID()
{
  static NS_DEFINE_IID(kCheckboxIID, NS_ICHECKBUTTON_IID);
  return kCheckboxIID;
}
  
const nsIID
nsInputCheckboxFrame::GetCID()
{
  static NS_DEFINE_IID(kCheckboxCID, NS_CHECKBUTTON_CID);
  return kCheckboxCID;
}

void 
nsInputCheckboxFrame::PreInitializeWidget(nsIPresContext* aPresContext, 
                                          nsSize& aBounds)
{
  nsInputCheckbox* content = (nsInputCheckbox *)mContent; // this must be an nsCheckbox 

  // get the state
  nsHTMLValue value; 
  nsContentAttr result = content->GetAttribute(nsHTMLAtoms::checked, value); 
  if (result != eContentAttr_NotThere) {
    mCacheState = PR_TRUE;/* XXX why cache state? */
  }

  float p2t = aPresContext->GetPixelsToTwips();
  aBounds.width  = (int)(13 * p2t);
  aBounds.height = (int)(13 * p2t);
}

void 
nsInputCheckboxFrame::InitializeWidget(nsIView *aView)
{
  nsICheckButton* checkbox;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&checkbox)) {
	checkbox->SetState(mCacheState);
    NS_RELEASE(checkbox);
  }
}

void 
nsInputCheckboxFrame::MouseClicked() 
{
  nsICheckButton* checkbox;
  nsIView* view;
  GetView(view);
  if (NS_OK == GetWidget(view, (nsIWidget **)&checkbox)) {
	PRBool newState = (checkbox->GetState()) ? PR_FALSE : PR_TRUE;
	checkbox->SetState(newState);
    NS_RELEASE(checkbox);
  }
  NS_RELEASE(view);
}



// nsInputCheckbox

nsInputCheckbox::nsInputCheckbox(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager)
{
}

nsInputCheckbox::~nsInputCheckbox()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}


PRInt32 
nsInputCheckbox::GetMaxNumValues()
{
  return 1;
}
  

PRBool
nsInputCheckbox::GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                           nsString* aValues)
{
  if (aMaxNumValues <= 0) {
    return PR_FALSE;
  }
  nsICheckButton* checkBox = (nsICheckButton *)GetWidget();
  PRBool state = checkBox->GetState();
  if(PR_TRUE != state) {
    return PR_FALSE;
  }

  if (nsnull == mValue) {
    aValues[0] = "on";
  } else {
    aValues[0] = *mValue;
  }
  aNumValues = 1;

  return PR_TRUE;
}

void 
nsInputCheckbox::Reset() 
{
  nsICheckButton* checkbox = (nsICheckButton *)GetWidget();
  if (nsnull != checkbox) {
    checkbox->SetState(mInitialChecked);
  }
}  

nsIFrame* 
nsInputCheckbox::CreateFrame(nsIPresContext* aPresContext,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
{
  nsIFrame* rv = new nsInputCheckboxFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

void nsInputCheckbox::GetType(nsString& aResult) const
{
  aResult = "checkbox";
}

void nsInputCheckbox::SetAttribute(nsIAtom* aAttribute,
                                   const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::checked) {
    mInitialChecked = PR_TRUE;
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

nsContentAttr nsInputCheckbox::GetAttribute(nsIAtom* aAttribute,
                                            nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::checked) {
    if (mInitialChecked) {
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
NS_NewHTMLInputCheckbox(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputCheckbox(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

