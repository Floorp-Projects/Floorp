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
#include "nsIStyleContext.h"

class nsInputCheckboxFrame : public nsInputFrame {
public:
  nsInputCheckboxFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual void MouseClicked(nsIPresContext* aPresContext);

protected:
  virtual ~nsInputCheckboxFrame();
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

nsInputCheckboxFrame::nsInputCheckboxFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsInputCheckboxFrame::~nsInputCheckboxFrame()
{
}

const nsIID&
nsInputCheckboxFrame::GetIID()
{
  static NS_DEFINE_IID(kCheckboxIID, NS_ICHECKBUTTON_IID);
  return kCheckboxIID;
}
  
const nsIID&
nsInputCheckboxFrame::GetCID()
{
  static NS_DEFINE_IID(kCheckboxCID, NS_CHECKBUTTON_CID);
  return kCheckboxCID;
}

void
nsInputCheckboxFrame::GetDesiredSize(nsIPresContext* aPresContext,
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
nsInputCheckboxFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  // get the intial state of the checkbox
  nsInputCheckbox* content = (nsInputCheckbox *)mContent; // this must be an nsCheckbox 
  nsHTMLValue value; 
  nsContentAttr result = content->GetAttribute(nsHTMLAtoms::checked, value); 
  PRBool checked = (result != eContentAttr_NotThere) ? PR_TRUE : PR_FALSE;

  // set the widget to the initial state
  nsICheckButton* checkbox;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&checkbox)) {
	  checkbox->SetState(checked);
    NS_RELEASE(checkbox);
  }
}

void 
nsInputCheckboxFrame::MouseClicked(nsIPresContext* aPresContext) 
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
  mChecked = PR_FALSE;
}

nsInputCheckbox::~nsInputCheckbox()
{
}


void nsInputCheckbox::MapAttributesInto(nsIStyleContext* aContext, 
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

PRInt32 
nsInputCheckbox::GetMaxNumValues()
{
  return 1;
}
  

PRBool
nsInputCheckbox::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
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
  aNames[0] = *mName;
  aNumValues = 1;

  return PR_TRUE;
}

void 
nsInputCheckbox::Reset() 
{
  nsICheckButton* checkbox = (nsICheckButton *)GetWidget();
  if (nsnull != checkbox) {
    checkbox->SetState(mChecked);
  }
}  

nsresult
nsInputCheckbox::CreateFrame(nsIPresContext* aPresContext,
                             nsIFrame* aParentFrame,
                             nsIStyleContext* aStyleContext,
                             nsIFrame*& aResult)
{
  nsIFrame* frame = new nsInputCheckboxFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void nsInputCheckbox::GetType(nsString& aResult) const
{
  aResult = "checkbox";
}

void nsInputCheckbox::SetAttribute(nsIAtom* aAttribute,
                                   const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::checked) {
    mChecked = PR_TRUE;
  }
  nsInputCheckboxSuper::SetAttribute(aAttribute, aValue);
}

nsContentAttr nsInputCheckbox::GetAttribute(nsIAtom* aAttribute,
                                            nsHTMLValue& aResult) const
{
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::checked) {
    return GetCacheAttribute(mChecked, aResult, eHTMLUnit_Empty);
  }
  else {
    return nsInputCheckboxSuper::GetAttribute(aAttribute, aResult);
  }
}

nsresult
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

