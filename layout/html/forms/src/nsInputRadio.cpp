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
#include "nsStyleUtil.h"


// prototypes
nsresult
NS_NewHTMLInputRadio(nsIHTMLContent** aInstancePtrResult,
                     nsIAtom* aTag, nsIFormManager* aManager);


static NS_DEFINE_IID(kRadioIID, NS_IRADIOBUTTON_IID);


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

nsresult
NS_NewInputRadioFrame(nsIContent* aContent,
                      nsIFrame*   aParent,
                      nsIFrame*&  aResult)
{
  aResult = new nsInputRadioFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

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
  aDesiredWidgetSize.width  = NSIntPixelsToTwips(12, p2t);
  aDesiredWidgetSize.height = NSIntPixelsToTwips(12, p2t);
  aDesiredLayoutSize.width  = aDesiredWidgetSize.width;
  aDesiredLayoutSize.height = aDesiredWidgetSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
}

void 
nsInputRadioFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsInputRadio* content = (nsInputRadio *)mContent; 
  //PRInt32 checkedAttr; 
  //nsContentAttr result = ((nsInput *)content)->GetAttribute(nsHTMLAtoms::checked, checkedAttr); 
  //if ((result == eContentAttr_HasValue) && (PR_FALSE != checkedAttr)) {
  nsIWidget* widget = nsnull;
  nsIRadioButton* radio = nsnull;
  nsresult result = GetWidget(aView, &widget);
  if (NS_OK == result) {
  	result = widget->QueryInterface(GetIID(),(void**)&radio);
  	if (result == NS_OK)
  	{
		  radio->SetState(content->mForcedChecked);

	    const nsStyleColor* color = 
	      nsStyleUtil::FindNonTransparentBackground(mStyleContext);

	    if (nsnull != color) {
	      widget->SetBackgroundColor(color->mBackgroundColor);
	    }
	    else {
	      widget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
	    }
	    NS_RELEASE(radio);
	   }
	   NS_RELEASE(widget);
  }
}

// nsInputRadio

const nsString* nsInputRadio::kTYPE = new nsString("radio");

nsInputRadio::nsInputRadio(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager) 
{
  mInitialChecked = PR_FALSE;
  mForcedChecked = PR_FALSE;
}

nsInputRadio::~nsInputRadio()
{
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  float p2t = aPresContext->GetPixelsToTwips();
  nscoord pad = NSIntPixelsToTwips(3, p2t);

  // add left and right padding around the radio button via css
  nsStyleSpacing* spacing = (nsStyleSpacing*) aContext->GetMutableStyleData(eStyleStruct_Spacing);
  if (eStyleUnit_Null == spacing->mMargin.GetLeftUnit()) {
    nsStyleCoord left(pad);
    spacing->mMargin.SetLeft(left);
  }
  if (eStyleUnit_Null == spacing->mMargin.GetRightUnit()) {
    nsStyleCoord right(NSIntPixelsToTwips(5, p2t));
    spacing->mMargin.SetRight(right);
  }
  // add bottom padding if backward mode
  // XXX why isn't this working?
  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(mode);
  if (eCompatibility_NavQuirks == mode) {
    if (eStyleUnit_Null == spacing->mMargin.GetBottomUnit()) {
      nsStyleCoord bottom(pad);
      spacing->mMargin.SetBottom(bottom);
    }
  }
  nsInput::MapAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsInputRadio::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
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
      nsIFormManager* formMan = radio->GetFormManager();
      if (formMan) {
        formMan->OnRadioChecked(*control);
        NS_RELEASE(formMan);
      }
      NS_RELEASE(radio);
    } 
  }
}

PRBool
nsInputRadio::GetChecked(PRBool aGetInitialValue) const
{
  if (aGetInitialValue) {
    return mInitialChecked;
  }
  else {
    if (mWidget) {
      PRBool state = PR_FALSE;
    	nsIRadioButton* radio = nsnull;
    	nsresult result = mWidget->QueryInterface(kRadioIID,(void**)&radio);
      if (result == NS_OK)
      {     	
      	radio->GetState(state);
      	NS_IF_RELEASE(radio);
      }
      return state;
    }
    else {
      return mForcedChecked;
    }
  }
}

void
nsInputRadio::SetChecked(PRBool aValue, PRBool aSetInitialValue)
{
  mForcedChecked = aValue;
  if (aSetInitialValue) {
    mInitialChecked = aValue;
  }
  //NS_ASSERTION(mWidget, "no widget for this nsInputRadio");
  if (mWidget) {
    ((nsIRadioButton *)mWidget)->SetState(aValue);
  }
}

void nsInputRadio::GetType(nsString& aResult) const
{
  aResult = "radio";
}

NS_IMETHODIMP
nsInputRadio::SetAttribute(nsIAtom* aAttribute,
                           const nsString& aValue,
                           PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::checked) {
    mInitialChecked = PR_TRUE;
    mForcedChecked = PR_TRUE;
  }
  return nsInputRadioSuper::SetAttribute(aAttribute, aValue, aNotify);
}

NS_IMETHODIMP
nsInputRadio::GetAttribute(nsIAtom* aAttribute,
                           nsHTMLValue& aResult) const
{
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::checked) {
    return GetCacheAttribute(mInitialChecked, aResult, eHTMLUnit_Empty);
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
  
  nsIWidget*		  widget = GetWidget();
  nsIRadioButton* radio = nsnull;
  PRBool state = PR_FALSE;
 	
 	if (widget != nsnull)
 	{
 		widget->QueryInterface(kRadioIID,(void**)&radio);
  	radio->GetState(state);
	  NS_IF_RELEASE(radio);
  }
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
  nsIWidget* widget = GetWidget();
  nsIRadioButton* radio = nsnull;
  if (widget != nsnull && NS_OK == widget->QueryInterface(kRadioIID,(void**)&radio))
  {
    radio->SetState(mInitialChecked);
    NS_RELEASE(radio);
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
