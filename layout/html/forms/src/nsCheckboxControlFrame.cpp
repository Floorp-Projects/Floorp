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

#include "nsICheckButton.h"
#include "nsFormControlFrame.h"
#include "nsFormFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsWidgetsCID.h"
#include "nsIView.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsStyleUtil.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLInputElement.h"


#ifdef XP_PC
#define NS_PC_DESIRED_CHECKBOX_SIZE 20
#define NS_PC_ABSOLUTE_CHECKBOX_SIZE 12
#endif


static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

class nsCheckboxControlFrame : public nsFormControlFrame {
public:
  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("CheckboxControl", aResult);
  }

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  virtual void Reset();

  NS_IMETHOD GetChecked(PRBool* aResult);

  //
  // XXX: The following methods are TEMPORARY. They are being used to get printing working
  // under windows. Later it may be used to GFX-render the controls to the display. 
  // Expect this code to repackaged and moved to a new location in the future.

  virtual void GetCurrentCheckState(PRBool* aState);

  virtual void PaintFixedSizeCheckMark(nsIRenderingContext& aRenderingContext, 
                                     float aPixelsToTwips);

  virtual void PaintCheckBox(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect);

  virtual void PaintFixedSizeCheckMarkBorder(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect);
  //XXX: End of the temporary methods
  
protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

nsresult
NS_NewCheckboxControlFrame(nsIFrame*& aResult)
{
  aResult = new nsCheckboxControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

const nsIID&
nsCheckboxControlFrame::GetIID()
{
  static NS_DEFINE_IID(kCheckboxIID, NS_ICHECKBUTTON_IID);
  return kCheckboxIID;
}
  
const nsIID&
nsCheckboxControlFrame::GetCID()
{
  static NS_DEFINE_IID(kCheckboxCID, NS_CHECKBUTTON_CID);
  return kCheckboxCID;
}

void
nsCheckboxControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                            const nsHTMLReflowState& aReflowState,
                                            nsHTMLReflowMetrics& aDesiredLayoutSize,
                                            nsSize& aDesiredWidgetSize)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(p2t);
#ifdef XP_PC
   aDesiredWidgetSize.width  = NSIntPixelsToTwips(NS_PC_DESIRED_CHECKBOX_SIZE, p2t);
   aDesiredWidgetSize.height = NSIntPixelsToTwips(NS_PC_DESIRED_CHECKBOX_SIZE, p2t);
#endif
  aDesiredLayoutSize.width  = aDesiredWidgetSize.width;
  aDesiredLayoutSize.height = aDesiredWidgetSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
}

NS_IMETHODIMP
nsCheckboxControlFrame::GetChecked(PRBool* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  *aResult = PR_FALSE;
  if (mContent) {
    nsIHTMLContent* iContent = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&iContent);
    if ((NS_OK == result) && iContent) {
      nsHTMLValue value;
      result = iContent->GetAttribute(nsHTMLAtoms::checked, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        *aResult = PR_TRUE;
      }
      NS_RELEASE(iContent);
    }
  }
  return result;
}

void 
nsCheckboxControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  if (!mWidget) {
    return;
  }

  SetColors(*aPresContext);
  mWidget->Enable(!nsFormFrame::GetDisabled(this));

  // set the widget to the initial state
  nsICheckButton* checkbox = nsnull;
  if (NS_OK == mWidget->QueryInterface(GetIID(),(void**)&checkbox)) {
    PRBool checked;
    nsresult result = GetChecked(&checked);
    if (NS_CONTENT_ATTR_HAS_VALUE == result) {
      checkbox->SetState(checked);
    }
    NS_RELEASE(checkbox);
  }
}

NS_IMETHODIMP
nsCheckboxControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                         nsIContent*     aChild,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aHint)
{
  nsresult result = NS_OK;
  if (mWidget) {
    if (nsHTMLAtoms::checked == aAttribute) {
      nsICheckButton* button = nsnull;
      result = mWidget->QueryInterface(GetIID(), (void**)&button);
      if ((NS_SUCCEEDED(result)) && (nsnull != button)) {
        PRBool checkedAttr;
        GetChecked(&checkedAttr);
        PRBool checkedPrevState;
        button->GetState(checkedPrevState);
        if (checkedAttr != checkedPrevState) {
          button->SetState(checkedAttr);
        }
        NS_RELEASE(button);
      }
    }   
  }
  return result;
}

void nsCheckboxControlFrame::GetCurrentCheckState(PRBool *aState)
{
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    inputElement->GetChecked(aState);
    NS_RELEASE(inputElement);
  }
}

//XXX: This may be needed later when we actually need the correct state for the checkbox
//void nsCheckboxControlFrame::SetCurrentCheckState(PRBool aState)
//{
//   nsIDOMHTMLInputElement* inputElement;
//  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
//    inputElement->SetDefaultChecked(aState); //XXX: TEMPORARY this should be GetChecked but it get's it's state from the widget instead
//    NS_RELEASE(inputElement);
//  }
//}

void 
nsCheckboxControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  nsICheckButton* checkbox = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(GetIID(),(void**)&checkbox))) {
    PRBool oldState;
    checkbox->GetState(oldState);
    PRBool newState = oldState ? PR_FALSE : PR_TRUE;
    checkbox->SetState(newState);
 //   SetCurrentCheckState(newState); // Let content model know about the change
    NS_IF_RELEASE(checkbox);
  }
}

PRInt32 
nsCheckboxControlFrame::GetMaxNumValues()
{
  return 1;
}
  

PRBool
nsCheckboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                       nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult nameResult = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != nameResult)) {
    return PR_FALSE;
  }

  PRBool result = PR_TRUE;

  nsAutoString value;
  nsresult valueResult = GetValue(&value);

  nsICheckButton* checkBox = nsnull;
  if ((nsnull != mWidget) && 
      (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox))) {
    PRBool state = PR_FALSE;
    checkBox->GetState(state);
    if (PR_TRUE != state) {
      result = PR_FALSE;
    } else {
      if (NS_CONTENT_ATTR_HAS_VALUE == valueResult) {
        aValues[0] = "on";
      } else {
        aValues[0] = value;
      }
      aNames[0] = name;
      aNumValues = 1;
    }
    NS_RELEASE(checkBox);
  }

  return result;
}

void 
nsCheckboxControlFrame::Reset() 
{ 
  nsICheckButton*  checkBox = nsnull;
  if ((mWidget != nsnull) && 
      (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox))) {
    PRBool checked;
    GetChecked(&checked);
    checkBox->SetState(checked);
    NS_RELEASE(checkBox);
  }
}  


//
// XXX: The following paint code is TEMPORARY. It is being used to get printing working
// under windows. Later it may be used to GFX-render the controls to the display. 
// Expect this code to repackaged and moved to a new location in the future.
//

void
nsCheckboxControlFrame::PaintFixedSizeCheckMark(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips)
{
   //XXX: Check mark is always draw in black. If the this code is used to Render the widget 
   //to the screen the color should be set using the CSS style color instead.
  aRenderingContext.SetColor(NS_RGB(0, 0, 0));
    // Offsets to x,y location, These offsets are used to place the checkmark in the middle
    // of it's 12X12 pixel box.
  const PRUint32 ox = 3;
  const PRUint32 oy = 3;

  nscoord onePixel = NSIntPixelsToTwips(1, aPixelsToTwips);

    // Draw checkmark using a series of rectangles. This builds an replica of the
    // way the checkmark looks under Windows. Using a polygon does not correctly 
    // represent a checkmark under Windows. This is due to round-off error in the
    // Twips to Pixel conversions.
  DrawLine(aRenderingContext, 0 + ox, 2 + oy, 0 + ox, 4 + oy, PR_FALSE, 1, onePixel);
  DrawLine(aRenderingContext, 1 + ox, 3 + oy, 1 + ox, 5 + oy, PR_FALSE, 1, onePixel);
  DrawLine(aRenderingContext, 2 + ox, 4 + oy, 2 + ox, 6 + oy, PR_FALSE, 1, onePixel);
  DrawLine(aRenderingContext, 3 + ox, 3 + oy, 3 + ox, 5 + oy, PR_FALSE, 1, onePixel);
  DrawLine(aRenderingContext, 4 + ox, 2 + oy, 4 + ox, 4 + oy, PR_FALSE, 1, onePixel);
  DrawLine(aRenderingContext, 5 + ox, 1 + oy, 5 + ox, 3 + oy, PR_FALSE, 1, onePixel);
  DrawLine(aRenderingContext, 6 + ox, 0 + oy, 6 + ox, 2 + oy, PR_FALSE, 1, onePixel);
}

void
nsCheckboxControlFrame::PaintFixedSizeCheckMarkBorder(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips)
{
 //XXX: Future, use CSS to paint border instead of painting it ourselves here.   
// nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

    // Offsets to x,y location
  PRUint32 ox = 0;
  PRUint32 oy = 0;

  nscoord onePixel = NSIntPixelsToTwips(1, aPixelsToTwips);
  nscoord twoPixels = NSIntPixelsToTwips(2, aPixelsToTwips);
  nscoord ninePixels = NSIntPixelsToTwips(9, aPixelsToTwips);
  nscoord twelvePixels = NSIntPixelsToTwips(12, aPixelsToTwips);

    // Draw Background
  nsRect rect(0, 0, twelvePixels, twelvePixels);
  aRenderingContext.SetColor(NS_RGB(255, 255, 255));
  aRenderingContext.FillRect(rect);

    // Draw Border
  aRenderingContext.SetColor(NS_RGB(128, 128, 128));
  DrawLine(aRenderingContext, 0 + ox, 0 + oy, 11 + ox, 0 + oy, PR_TRUE, 1, onePixel);
  DrawLine(aRenderingContext, 0 + ox, 0 + oy, 0 + ox, 11 + oy, PR_FALSE, 1, onePixel);

  aRenderingContext.SetColor(NS_RGB(192, 192, 192));
  DrawLine(aRenderingContext, 1 + ox, 11 + oy, 11 + ox, 11 + oy, PR_TRUE, 1, onePixel);
  DrawLine(aRenderingContext, 11 + ox, 1 + oy, 11 + ox, 11 + oy, PR_FALSE, 1, onePixel);

  aRenderingContext.SetColor(NS_RGB(0, 0, 0));
  DrawLine(aRenderingContext, 1 + ox, 1 + oy, 10 + ox, 1 + oy, PR_TRUE, 1, onePixel);
  DrawLine(aRenderingContext, 1 + ox, 1 + oy, 1 + ox, 10 + oy, PR_FALSE, 1, onePixel);

}

#if 0
//XXX: Future, use the following code to draw a checkmark in any size.
void
nsCheckboxControlFrame::PaintScaledCheckMark(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips, PRUint32 aWidth, PRUint32 aHeight)
{
#define NS_CHECKED_POINTS 7

    // Points come from the coordinates on a 11X11 pixels rectangle with 0,0 at the lower
    // left. 
  nscoord checkedPolygonDef[] = {0,2, 2,4, 6,0 , 6,2, 2,6, 0,4, 0,2 };
  nsPoint checkedPolygon[NS_CHECKED_POINTS];
  PRUint32 defIndex = 0;
  PRUint32 polyIndex = 0;
  PRBool scalable = PR_FALSE;
  aRenderingContext.SetColor(color->mColor);
  PRUint32 height = aHeight; 

  for (defIndex = 0; defIndex < (NS_CHECKED_POINTS * 2); defIndex++) {
    checkedPolygon[polyIndex].x = nscoord(((checkedPolygonDef[defIndex])) * (height / 11.0) + (height / 11.0));
    defIndex++;
    checkedPolygon[polyIndex].y = nscoord((((checkedPolygonDef[defIndex]))) * (height / 11.0) + (height / 11.0));
    polyIndex++;
  }
  
  aRenderingContext.FillPolygon(checkedPolygon, NS_CHECKED_POINTS);

}
#endif

void
nsCheckboxControlFrame::PaintCheckBox(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect)
{
#ifdef XP_PC
  aRenderingContext.PushState();

  float p2t;
  aPresContext.GetScaledPixelsToTwips(p2t);
 
    //Offset fixed size checkbox in to the middle of the area reserved for the checkbox
    const int printOffsetX = (NS_PC_DESIRED_CHECKBOX_SIZE - NS_PC_ABSOLUTE_CHECKBOX_SIZE);
    const int printOffsetY = (NS_PC_DESIRED_CHECKBOX_SIZE - NS_PC_ABSOLUTE_CHECKBOX_SIZE);
    aRenderingContext.Translate(NSIntPixelsToTwips(printOffsetX, p2t), 
                                NSIntPixelsToTwips(printOffsetY, p2t));

    // Draw's background + border
  PaintFixedSizeCheckMarkBorder(aRenderingContext, p2t);
 
  PRBool checked = PR_TRUE;
  GetCurrentCheckState(&checked); // Get check state from the content model

  if (PR_TRUE == checked) {
    PaintFixedSizeCheckMark(aRenderingContext, p2t);
  }
  PRBool clip;
  aRenderingContext.PopState(clip);
#endif
}

NS_METHOD 
nsCheckboxControlFrame::Paint(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect)
{
#ifdef XP_PC
  //XXX: This call and the Paint Methods could be moved to the Widget library instead of
  // being done here. In the future, If we decide to have the Widgets GFX rendered and use
  // CSS style to control all aspects of the widgets appearance, it may make sense to keep
  // The rendering code for the widget here in the Frame.
  PaintCheckBox(aPresContext, aRenderingContext, aDirtyRect);
#endif
  return NS_OK;
}


