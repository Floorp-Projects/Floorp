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

#include "nsRadioControlFrame.h"
#include "nsFormControlFrame.h"
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
#include "nsIFormControl.h"
#include "nsIFormManager.h"
#include "nsIView.h"
#include "nsIStyleContext.h"
#include "nsStyleUtil.h"
#include "nsFormFrame.h"

static NS_DEFINE_IID(kIRadioIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);



nsresult
NS_NewRadioControlFrame(nsIContent* aContent,
                        nsIFrame*   aParent,
                        nsIFrame*&  aResult)
{
  aResult = new nsRadioControlFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsRadioControlFrame::nsRadioControlFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsFormControlFrame(aContent, aParentFrame)
{
  mInitialChecked = nsnull;
  mForcedChecked = PR_FALSE;
}

nsRadioControlFrame::~nsRadioControlFrame()
{
  if (mInitialChecked) {
    delete mInitialChecked;
  }
}

const nsIID&
nsRadioControlFrame::GetIID()
{
  return kIRadioIID;
}
  
const nsIID&
nsRadioControlFrame::GetCID()
{
  static NS_DEFINE_IID(kRadioCID, NS_RADIOBUTTON_CID);
  return kRadioCID;
}

void 
nsRadioControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aDesiredLayoutSize,
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
nsRadioControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(GetIID(),(void**)&radio))) {
		radio->SetState(mForcedChecked);

	  const nsStyleColor* color = 
	    nsStyleUtil::FindNonTransparentBackground(mStyleContext);

	  if (nsnull != color) {
	    mWidget->SetBackgroundColor(color->mBackgroundColor);
	  } else {
	    mWidget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
	  }
	  NS_RELEASE(radio);
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
  }
}


void 
nsRadioControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  nsIRadioButton* radioWidget;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID, (void**)&radioWidget))) {
	  radioWidget->SetState(PR_TRUE);
    NS_RELEASE(radioWidget);
    if (mFormFrame) {
      mFormFrame->OnRadioChecked(*this);
    } 
  }
}

PRBool
nsRadioControlFrame::GetChecked(PRBool aGetInitialValue) 
{
  if (aGetInitialValue) {
    if (!mInitialChecked && mContent) {
      mInitialChecked = new PRBool(PR_FALSE);
      nsIHTMLContent* iContent = nsnull;
      nsresult result = mContent->QueryInterface(kIHTMLContentIID, (void**)&iContent);
      if ((NS_OK == result) && iContent) {
        nsHTMLValue value;
        result = iContent->GetAttribute(nsHTMLAtoms::checked, value);
        if (NS_CONTENT_ATTR_HAS_VALUE == result) {
          *mInitialChecked = PR_TRUE;
          mForcedChecked  = PR_TRUE;
        }
        NS_RELEASE(iContent);
      }
    }
    return *mInitialChecked;
  }

  PRBool result = PR_FALSE;
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
    radio->GetState(result);
    NS_RELEASE(radio);
  } else {
    result = mForcedChecked;
  }
  return result;
}

void
nsRadioControlFrame::SetChecked(PRBool aValue, PRBool aSetInitialValue)
{
  mForcedChecked = aValue;
  if (aSetInitialValue) {
    if (!mInitialChecked) {
      mInitialChecked = new PRBool(aValue);
    }
    *mInitialChecked = aValue;
  }
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK ==  mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
    radio->SetState(aValue);
    NS_RELEASE(radio);
  }
}


PRBool
nsRadioControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                    nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  PRBool state = PR_FALSE;

  nsIRadioButton* radio = nsnull;
 	if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
  	radio->GetState(state);
	  NS_RELEASE(radio);
  }
  if(PR_TRUE != state) {
    return PR_FALSE;
  }

  nsAutoString value;
  result = GetValue(&value);
  
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    aValues[0] = value;
  } else {
    aValues[0] = "on";
  }
  aNames[0]  = name;
  aNumValues = 1;

  return PR_TRUE;
}

void 
nsRadioControlFrame::Reset() 
{
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
    PRBool state = (nsnull == mInitialChecked) ? PR_FALSE : *mInitialChecked;
    radio->SetState(state);
    NS_RELEASE(radio);
  }
}  


// CLASS nsRadioControlGroup

nsRadioControlGroup::nsRadioControlGroup(nsString& aName)
:mName(aName), mCheckedRadio(nsnull)
{
}

nsRadioControlGroup::~nsRadioControlGroup()
{
  mCheckedRadio = nsnull;
}
  
PRInt32 
nsRadioControlGroup::GetRadioCount() const 
{ 
  return mRadios.Count(); 
}

nsRadioControlFrame*
nsRadioControlGroup::GetRadioAt(PRInt32 aIndex) const 
{ 
  return (nsRadioControlFrame*) mRadios.ElementAt(aIndex);
}

PRBool 
nsRadioControlGroup::AddRadio(nsRadioControlFrame* aRadio) 
{ 
  return mRadios.AppendElement(aRadio);
}

PRBool 
nsRadioControlGroup::RemoveRadio(nsRadioControlFrame* aRadio) 
{ 
  return mRadios.RemoveElement(aRadio);
}

nsRadioControlFrame*
nsRadioControlGroup::GetCheckedRadio()
{
  return mCheckedRadio;
}

void    
nsRadioControlGroup::SetCheckedRadio(nsRadioControlFrame* aRadio)
{
  mCheckedRadio = aRadio;
}

void
nsRadioControlGroup::GetName(nsString& aNameResult) const
{
  aNameResult = mName;
}
