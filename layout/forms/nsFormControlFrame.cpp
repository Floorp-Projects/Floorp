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
#include "nsCOMPtr.h"
#include "nsFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsCoord.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIComponentManager.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsIFontMetrics.h"
#include "nsIFormControl.h"
#include "nsIDeviceContext.h"
#include "nsHTMLAtoms.h"
#include "nsIButton.h"  // remove this when GetCID is pure virtual
#include "nsICheckButton.h"  //remove this
#include "nsITextWidget.h"  //remove this
#include "nsISupports.h"
#include "nsStyleConsts.h"
#include "nsUnitConversion.h"
#include "nsStyleUtil.h"
#include "nsFormFrame.h"
#include "nsIContent.h"
#include "nsStyleUtil.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"


static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

nsFormControlFrame::nsFormControlFrame()
  : nsLeafFrame()
{
  mLastMouseState = eMouseNone;
  mDidInit        = PR_FALSE;
  mWidget         = nsnull;
  mFormFrame      = nsnull;
}

nsFormControlFrame::~nsFormControlFrame()
{
  mFormFrame = nsnull;
  NS_IF_RELEASE(mWidget);
}

nsresult
nsFormControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);
}

nscoord 
nsFormControlFrame::GetScrollbarWidth(float aPixToTwip)
{
   return NSIntPixelsToTwips(19, aPixToTwip);  // XXX this is windows
}

nscoord 
nsFormControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}

nscoord 
nsFormControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

nscoord 
nsFormControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}

nscoord 
nsFormControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPixToTwip, aInnerWidth);
}

PRInt32
nsFormControlFrame::GetMaxNumValues()
{
  return 0;
}

PRBool
nsFormControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, 
                                   nsString* aValues, nsString* aNames)
{
  aNumValues = 0;
  return PR_FALSE;
}

void 
nsFormControlFrame::SetClickPoint(nscoord aX, nscoord aY)
{
  mLastClickPoint.x = aX;
  mLastClickPoint.y = aY;
}

// XXX it would be cool if form element used our rendering sw, then
// they could be blended, and bordered, and so on...
NS_METHOD
nsFormControlFrame::Paint(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
  return nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            aWhichLayer);
}

void 
nsFormControlFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                   nsSize&                  aDesiredWidgetSize)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(*aPresContext, aReflowState, styleSize);

  // subclasses should always override this method, but if not and no css, make it small
  aDesiredLayoutSize.width  = (styleSize.width  > CSS_NOTSET) ? styleSize.width  : 144;
  aDesiredLayoutSize.height = (styleSize.height > CSS_NOTSET) ? styleSize.height : 144;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->width  = aDesiredLayoutSize.width;
    aDesiredLayoutSize.maxElementSize->height = aDesiredLayoutSize.height;
  }
  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
}

void 
nsFormControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  nsSize ignore;
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize, ignore);
}

NS_IMETHODIMP
nsFormControlFrame::DidReflow(nsIPresContext& aPresContext,
                        nsDidReflowStatus aStatus)
{
  nsresult rv = nsLeafFrame::DidReflow(aPresContext, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIView* view = nsnull;
    GetView(&view);
    if (nsnull != view) {
      view->SetVisibility(nsViewVisibility_kShow);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsFormControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  return NS_OK;
}

NS_METHOD
nsFormControlFrame::Reflow(nsIPresContext&          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  nsresult result = NS_OK;

  // add ourself as an nsIFormControlFrame
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

  nsCOMPtr<nsIDeviceContext> dx;
  aPresContext.GetDeviceContext(getter_AddRefs(dx));
  PRBool requiresWidget = PR_TRUE;
 
    // Checkto see if the device context supports widgets at all
  if (dx) { 
    dx->SupportsNativeWidgets(requiresWidget);
  }

  nsWidgetRendering mode;
  aPresContext.GetWidgetRenderingMode(&mode);
  if (eWidgetRendering_Gfx == mode) {
      // Check with the frame to see if requires a widget to render
    if (PR_TRUE == requiresWidget) {
      RequiresWidget(requiresWidget);   
    }
  }

  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, mWidgetSize);

  if (!mDidInit) {
    if (PR_TRUE == requiresWidget) {
	    nsCOMPtr<nsIPresShell> presShell;
      aPresContext.GetShell(getter_AddRefs(presShell));
  	  nsCOMPtr<nsIViewManager> viewMan;
      presShell->GetViewManager(getter_AddRefs(viewMan));
      nsRect boundBox(0, 0, aDesiredSize.width, aDesiredSize.height); 

      // absolutely positioned controls already have a view but not a widget
      nsIView* view = nsnull;
      GetView(&view);
      if (nsnull == view) {
        result = nsComponentManager::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&view);
	      if (!NS_SUCCEEDED(result)) {
	        NS_ASSERTION(0, "Could not create view for form control"); 
          aStatus = NS_FRAME_NOT_COMPLETE;
          return result;
	      }

        nsIFrame* parWithView;
	      nsIView *parView;

        GetParentWithView(&parWithView);
	      parWithView->GetView(&parView);

	      // initialize the view as hidden since we don't know the (x,y) until Paint
        result = view->Init(viewMan, boundBox, parView, nsnull, nsViewVisibility_kHide);
        if (NS_OK != result) {
	        NS_ASSERTION(0, "view initialization failed"); 
          aStatus = NS_FRAME_NOT_COMPLETE;
          return NS_OK;
	      }

        viewMan->InsertChild(parView, view, 0);
        SetView(view);
      }

      PRInt32 type;
      GetType(&type);
      const nsIID& id = GetCID();

      if ((NS_FORM_INPUT_HIDDEN != type) && (PR_TRUE == requiresWidget)) {
	      // Do the following only if a widget is created
        nsWidgetInitData* initData = GetWidgetInitData(aPresContext); // needs to be deleted
        view->CreateWidget(id, initData);

        if (nsnull != initData) {
          delete(initData);
        }
	  
 	      // set our widget
	      result = GetWidget(view, &mWidget);
	      if ((NS_OK == result) && mWidget) { // keep the ref on mWidget
          nsIFormControl* formControl = nsnull;
          result = mContent->QueryInterface(kIFormControlIID, (void**)&formControl);
          if ((NS_OK == result) && formControl) {
            // set the content's widget, so it can get content modified by the widget
            formControl->SetWidget(mWidget);
            NS_RELEASE(formControl);
          }
 	      } else {
	        NS_ASSERTION(0, "could not get widget");
	      }
      }
	  
	    PostCreateWidget(&aPresContext, aDesiredSize.width, aDesiredSize.height);
      mDidInit = PR_TRUE;

      if ((aDesiredSize.width != boundBox.width) || (aDesiredSize.height != boundBox.height)) {
        viewMan->ResizeView(view, aDesiredSize.width, aDesiredSize.height);
      }

    } else {
      PostCreateWidget(&aPresContext, aDesiredSize.width, aDesiredSize.height);
      mDidInit = PR_TRUE;
    }
  }
  
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
    //XXX aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
  }
    
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}


NS_IMETHODIMP
nsFormControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aHint)
{
  if (nsnull != mWidget) {
    if (nsHTMLAtoms::disabled == aAttribute) {
      mWidget->Enable(!nsFormFrame::GetDisabled(this));
    }
  }
  return NS_OK;
}

nsWidgetInitData* 
nsFormControlFrame::GetWidgetInitData(nsIPresContext& aPresContext)
{
  return nsnull;
}

void 
nsFormControlFrame::SetColors(nsIPresContext& aPresContext)
{
  if (mWidget) {
    nsCompatibility mode;
    aPresContext.GetCompatibilityMode(&mode);
    const nsStyleColor* color =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    if (nsnull != color) {
      if (!(NS_STYLE_BG_COLOR_TRANSPARENT & color->mBackgroundFlags)) {
        mWidget->SetBackgroundColor(color->mBackgroundColor);
#ifdef bug_1021_closed
      } else if (eCompatibility_NavQuirks == mode) {
#else
      } else {
#endif
        mWidget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
      }
      mWidget->SetForegroundColor(color->mColor);
    }
  }
}

void 
nsFormControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
}

// native widgets don't unset focus explicitly and don't need to repaint
void 
nsFormControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  if (mWidget && aOn) {
    mWidget->SetFocus();
  }
}

nsresult
nsFormControlFrame::GetWidget(nsIWidget** aWidget)
{
  if (mWidget) {
    NS_ADDREF(mWidget);
    *aWidget = mWidget;
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
    return NS_OK;
  } else {
    *aWidget = nsnull;
    return NS_FORM_NOTOK;
  }
}

nsresult
nsFormControlFrame::GetWidget(nsIView* aView, nsIWidget** aWidget)
{
  nsIWidget*  widget;
  aView->GetWidget(widget);
  nsresult    result;

  if (nsnull == widget) {
    *aWidget = nsnull;
    result = NS_ERROR_FAILURE;

  } else {
    result =  widget->QueryInterface(kIWidgetIID, (void**)aWidget); // keep the ref
    if (NS_FAILED(result)) {
      NS_ASSERTION(0, "The widget interface is invalid");  // need to print out more details of the widget
    }
    NS_RELEASE(widget);
  }

  return result;
}

const nsIID&
nsFormControlFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID&
nsFormControlFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

NS_IMETHODIMP
nsFormControlFrame::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::maxlength, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return result;
}

nsresult
nsFormControlFrame::GetSizeFromContent(PRInt32* aSize) const
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::size, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return result;
}

NS_IMETHODIMP
nsFormControlFrame::GetType(PRInt32* aType) const
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIFormControl* formControl = nsnull;
    result = mContent->QueryInterface(kIFormControlIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      result = formControl->GetType(aType);
      NS_RELEASE(formControl);
    }
  }
  return result;
}

NS_IMETHODIMP
nsFormControlFrame::GetName(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      nsHTMLValue value;
      result = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}


NS_IMETHODIMP
nsFormControlFrame::GetValue(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      nsHTMLValue value;
      result = formControl->GetHTMLAttribute(nsHTMLAtoms::value, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}


PRBool
nsFormControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  // Since JS Submit() calls are not linked to an element, aSubmitter is null.
  // Return success to allow the call to go through.
  if (aSubmitter == nsnull) return PR_TRUE;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

NS_METHOD nsFormControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  if (nsnull != mWidget) {
    // make sure that the widget in the event is this input
    // XXX if there is no view, it could be an image button. Unfortunately,
    // every image button will get every event.
    nsIView* view;
    GetView(&view);
    if (view) {
      if (mWidget != aEvent->widget) {
        aEventStatus = nsEventStatus_eIgnore;
        return NS_OK;
      }
    }
  }

  //printf(" %d %d %d %d (%d,%d) \n", this, aEvent->widget, aEvent->widgetSupports, 
  //       aEvent->message, aEvent->point.x, aEvent->point.y);

  PRInt32 type;
  GetType(&type);
  switch (aEvent->message) {
    case NS_MOUSE_ENTER:
      mLastMouseState = eMouseEnter;
      break;
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      if (NS_FORM_INPUT_IMAGE == type) {
        mLastMouseState = eMouseDown;
      } else {
        mLastMouseState = (eMouseEnter == mLastMouseState) ? eMouseDown : eMouseNone;
      }
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
      if (eMouseDown == mLastMouseState) {
        MouseClicked(&aPresContext);
      } 
      mLastMouseState = eMouseEnter;
      break;
    case NS_MOUSE_EXIT:
      mLastMouseState = eMouseNone;
      break;
    case NS_KEY_DOWN:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_RETURN == keyEvent->keyCode) {
          EnterPressed(aPresContext);
        } 
        else if (NS_VK_SPACE == keyEvent->keyCode) {
          MouseClicked(&aPresContext);
        }
      }
      break;
  }
  aEventStatus = nsEventStatus_eConsumeDoDefault;
  return NS_OK;
}

void 
nsFormControlFrame::GetStyleSize(nsIPresContext& aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 nsSize& aSize)
{
  if (aReflowState.HaveFixedContentWidth()) {
    aSize.width = aReflowState.computedWidth;
  }
  else {
    aSize.width = CSS_NOTSET;
  }
  if (aReflowState.HaveFixedContentHeight()) {
    aSize.height = aReflowState.computedHeight;
  }
  else {
    aSize.height = CSS_NOTSET;
  }
}

void
nsFormControlFrame::Reset()
{
}

NS_IMETHODIMP
nsFormControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

NS_IMETHODIMP
nsFormControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}

nsresult nsFormControlFrame::GetDefaultCheckState(PRBool *aState)
{	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    res = inputElement->GetDefaultChecked(aState);
    NS_RELEASE(inputElement);
  }
	return res;
}

nsresult nsFormControlFrame::SetDefaultCheckState(PRBool aState)
{
	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    res = inputElement->SetDefaultChecked(aState);
    NS_RELEASE(inputElement);
  }
	return res;
}

nsresult nsFormControlFrame::GetCurrentCheckState(PRBool *aState)
{
	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    res = inputElement->GetChecked(aState);
    NS_RELEASE(inputElement);
  }
	return res;
}

nsresult nsFormControlFrame::SetCurrentCheckState(PRBool aState)
{
	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    inputElement->SetChecked(aState); 
   NS_RELEASE(inputElement);
  }
	return res;
}

NS_IMETHODIMP nsFormControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsFormControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}

nsresult nsFormControlFrame::RequiresWidget(PRBool & aRequiresWidget)
{
  aRequiresWidget = PR_TRUE;
  return NS_OK;
}


