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
#include "nsStyleUtil.h"


static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);


// Prototypes
nsresult
NS_NewHTMLInputCheckbox(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag, nsIFormManager* aManager);


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

nsresult
NS_NewInputCheckboxFrame(nsIContent* aContent,
                         nsIFrame*   aParent,
                         nsIFrame*&  aResult)
{
  aResult = new nsInputCheckboxFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

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
#ifdef XP_PC
  aDesiredWidgetSize.width  = NSIntPixelsToTwips(12, p2t);
  aDesiredWidgetSize.height = NSIntPixelsToTwips(12, p2t);
#endif
#ifdef XP_PC
  aDesiredWidgetSize.width  = NSIntPixelsToTwips(20, p2t);
  aDesiredWidgetSize.height = NSIntPixelsToTwips(20, p2t);
#endif
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
  nsresult result = content->GetAttribute(nsHTMLAtoms::checked, value); 
  PRBool checked = (result != NS_CONTENT_ATTR_NOT_THERE) ? PR_TRUE : PR_FALSE;

  // set the widget to the initial state
  nsIWidget*			widget = nsnull;
  nsICheckButton* checkbox = nsnull;
  
  if (NS_OK == GetWidget(aView, &widget)) {
  	widget->QueryInterface(GetIID(),(void**)&checkbox);
	  checkbox->SetState(checked);

    const nsStyleColor* color = 
      nsStyleUtil::FindNonTransparentBackground(mStyleContext);

    if (nsnull != color) {
      widget->SetBackgroundColor(color->mBackgroundColor);
    }
    else {
      widget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
    }

    NS_IF_RELEASE(checkbox);
    NS_IF_RELEASE(widget);
  }
}

void 
nsInputCheckboxFrame::MouseClicked(nsIPresContext* aPresContext) 
{
	nsIWidget*			widget = nsnull;
  nsICheckButton* checkbox = nsnull;
  nsIView* view;
  GetView(view);
  if (NS_OK == GetWidget(view, &widget)) {
  	widget->QueryInterface(GetIID(),(void**)&checkbox);
  	PRBool oldState;
  	checkbox->GetState(oldState);
		PRBool newState = oldState ? PR_FALSE : PR_TRUE;
		checkbox->SetState(newState);
    NS_IF_RELEASE(checkbox);
    NS_IF_RELEASE(widget);
  }
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
nsInputCheckbox::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
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

  nsIWidget* widget = GetWidget();
  nsICheckButton* checkBox = nsnull;
  if (widget != nsnull && NS_OK == widget->QueryInterface(kICheckButtonIID,(void**)&checkBox))
  {
    PRBool state = PR_FALSE;
    checkBox->GetState(state);
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
    NS_RELEASE(checkBox);
    return PR_TRUE;
  }
  return PR_FALSE;
}

void 
nsInputCheckbox::Reset() 
{ 
  nsIWidget*       widget = GetWidget();
  nsICheckButton*  checkBox = nsnull;
  if (widget != nsnull && NS_OK == widget->QueryInterface(kICheckButtonIID,(void**)&checkBox))
  {
    checkBox->SetState(mChecked);
    NS_RELEASE(checkBox);
  }
}  

void nsInputCheckbox::GetType(nsString& aResult) const
{
  aResult = "checkbox";
}

NS_IMETHODIMP
nsInputCheckbox::SetAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::checked) {
    mChecked = PR_TRUE;
  }
  return nsInputCheckboxSuper::SetAttribute(aAttribute, aValue, aNotify);
}

NS_IMETHODIMP
nsInputCheckbox::GetAttribute(nsIAtom* aAttribute,
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

