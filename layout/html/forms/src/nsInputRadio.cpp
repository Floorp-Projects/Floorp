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

#include "nsInputRadio.h"
#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsIRadioButton.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsHTMLAtoms.h"
#include "nsIFormManager.h"
#include "nsIView.h"
#include "nsIStyleContext.h"

class nsInputRadioFrame : public nsInputFrame {
public:
  nsInputRadioFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual void MouseClicked(nsIPresContext* aPresContext);

protected:

  virtual ~nsInputRadioFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

nsInputRadioFrame::nsInputRadioFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsInputRadioFrame::~nsInputRadioFrame()
{
}

const nsIID&
nsInputRadioFrame::GetIID()
{
  static NS_DEFINE_IID(kRadioIID, NS_IRADIOBUTTON_IID);
  return kRadioIID;
}
  
const nsIID&
nsInputRadioFrame::GetCID()
{
  static NS_DEFINE_IID(kRadioCID, NS_RADIOBUTTON_CID);
  return kRadioCID;
}

void 
nsInputRadioFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsReflowState& aReflowState,
                                  nsReflowMetrics& aDesiredLayoutSize,
                                  nsSize& aDesiredWidgetSize)
{
  float p2t = aPresContext->GetPixelsToTwips();
  aDesiredWidgetSize.width  = (int)(12 * p2t);
  aDesiredWidgetSize.height = (int)(12 * p2t);
  aDesiredLayoutSize.width  = aDesiredWidgetSize.width;
  aDesiredLayoutSize.height = aDesiredWidgetSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
}

void 
nsInputRadioFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsInputRadio* content = (nsInputRadio *)mContent; 
  PRInt32 checkedAttr; 
  nsContentAttr result = ((nsInput *)content)->GetAttribute(nsHTMLAtoms::checked, checkedAttr); 
  if ((result == eContentAttr_HasValue) && (PR_FALSE != checkedAttr)) {
    nsIRadioButton* radio;
    if (NS_OK == GetWidget(aView, (nsIWidget **)&radio)) {
	    radio->SetState(PR_TRUE);
      NS_RELEASE(radio);
    }
  }
}

// nsInputRadio

const nsString* nsInputRadio::kTYPE = new nsString("radio");

nsInputRadio::nsInputRadio(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager) 
{
  mChecked = PR_FALSE;
}

nsInputRadio::~nsInputRadio()
{
}

void nsInputRadio::MapAttributesInto(nsIStyleContext* aContext, 
                                     nsIPresContext* aPresContext)
{
  float p2t = aPresContext->GetPixelsToTwips();
  nscoord pad = (int)(3 * p2t + 0.5);

  // add left and right padding around the radio button via css
  nsStyleSpacing* spacing = (nsStyleSpacing*) aContext->GetData(eStyleStruct_Spacing);
  if (eStyleUnit_Null == spacing->mMargin.GetLeftUnit()) {
    nsStyleCoord left(pad);
    spacing->mMargin.SetLeft(left);
  }
  if (eStyleUnit_Null == spacing->mMargin.GetRightUnit()) {
    nsStyleCoord right((int)(5 * p2t + 0.5));
    spacing->mMargin.SetRight(right);
  }
  // add bottom padding if backward mode
  // XXX why isn't this working?
  nsIFormManager* formMan = GetFormManager();
  if (formMan && (kBackwardMode == formMan->GetMode())) {
    if (eStyleUnit_Null == spacing->mMargin.GetBottomUnit()) {
      nsStyleCoord bottom(pad);
      spacing->mMargin.SetBottom(bottom);
    }
    nsInput::MapAttributesInto(aContext, aPresContext);
  }
}

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);

void 
nsInputRadioFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  nsIRadioButton* radioWidget;
  nsIView* view;
  GetView(view);
  if (NS_OK == GetWidget(view, (nsIWidget **)&radioWidget)) {
	  radioWidget->SetState(PR_TRUE);
    NS_RELEASE(radioWidget);
    nsInputRadio* radio;
    GetContent((nsIContent *&) radio);
    nsIFormControl* control;
    nsresult status = radio->QueryInterface(kIFormControlIID, (void **)&control); 
    NS_ASSERTION(NS_OK == status, "nsInputRadio has no nsIFormControl interface");
    if (NS_OK == status) {
      radio->GetFormManager()->OnRadioChecked(*control);
      NS_RELEASE(radio);
    } 
  }
  NS_RELEASE(view);
}

PRBool
nsInputRadio::GetChecked(PRBool aGetInitialValue) const
{
  if (aGetInitialValue) {
    return mChecked;
  }
  else {
    NS_ASSERTION(mWidget, "no widget for this nsInputRadio");
    return ((nsIRadioButton *)mWidget)->GetState();
  }
}

void
nsInputRadio::SetChecked(PRBool aValue, PRBool aSetInitialValue)
{
  if (aSetInitialValue) {
    mChecked = aValue;
  }
  NS_ASSERTION(mWidget, "no widget for this nsInputRadio");
  ((nsIRadioButton *)mWidget)->SetState(aValue);
}

nsresult
nsInputRadio::CreateFrame(nsIPresContext* aPresContext,
                          nsIFrame* aParentFrame,
                          nsIStyleContext* aStyleContext,
                          nsIFrame*& aResult)
{
  nsIFrame* frame = new nsInputRadioFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void nsInputRadio::GetType(nsString& aResult) const
{
  aResult = "radio";
}

void nsInputRadio::SetAttribute(nsIAtom* aAttribute,
                                const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::checked) {
    mChecked = PR_TRUE;
  }
  nsInputRadioSuper::SetAttribute(aAttribute, aValue);
}

nsContentAttr nsInputRadio::GetAttribute(nsIAtom* aAttribute,
                                         nsHTMLValue& aResult) const
{
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::checked) {
    return GetCacheAttribute(mChecked, aResult, eHTMLUnit_Empty);
  }
  else {
    return nsInputRadioSuper::GetAttribute(aAttribute, aResult);
  }
}

PRBool
nsInputRadio::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                             nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
    return PR_FALSE;
  }
  nsIRadioButton* radio = (nsIRadioButton *)GetWidget();
  PRBool state = radio->GetState();
  if(PR_TRUE != state) {
    return PR_FALSE;
  }

  if (nsnull == mValue) {
    aValues[0] = "on";
  } else {
    aValues[0] = *mValue;
  }
  aNames[0] = *mName;
  aNumValues = 1;

  return PR_TRUE;
}

void 
nsInputRadio::Reset() 
{
  nsIRadioButton* radio = (nsIRadioButton *)GetWidget();
  if (nsnull != radio) {
    radio->SetState(mChecked);
  }
}  

nsresult
NS_NewHTMLInputRadio(nsIHTMLContent** aInstancePtrResult,
                     nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputRadio(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

// CLASS nsInputRadioGroup

nsInputRadioGroup::nsInputRadioGroup(nsString& aName)
:mName(aName), mCheckedRadio(nsnull)
{
}

nsInputRadioGroup::~nsInputRadioGroup()
{
  mCheckedRadio = nsnull;
}
  
PRInt32 
nsInputRadioGroup::GetRadioCount() const 
{ 
  return mRadios.Count(); 
}

nsIFormControl* 
nsInputRadioGroup::GetRadioAt(PRInt32 aIndex) const 
{ 
  return (nsIFormControl*) mRadios.ElementAt(aIndex);
}

PRBool 
nsInputRadioGroup::AddRadio(nsIFormControl* aRadio) 
{ 
  return mRadios.AppendElement(aRadio);
}

PRBool 
nsInputRadioGroup::RemoveRadio(nsIFormControl* aRadio) 
{ 
  return mRadios.RemoveElement(aRadio);
}

nsIFormControl* 
nsInputRadioGroup::GetCheckedRadio()
{
  return mCheckedRadio;
}

void    
nsInputRadioGroup::SetCheckedRadio(nsIFormControl* aRadio)
{
  mCheckedRadio = aRadio;
}

void
nsInputRadioGroup::GetName(nsString& aNameResult) const
{
  aNameResult = mName;
}
