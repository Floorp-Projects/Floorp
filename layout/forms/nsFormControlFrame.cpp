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
#include "nsRepository.h"
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
#include "nsGlobalVariables.h"
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
}

nsFormControlFrame::~nsFormControlFrame()
{
  //printf("nsFormControlFrame::~nsFormControlFrame \n");
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
nsFormControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsHTMLReflowMetrics& aDesiredLayoutSize,
                                   nsSize& aDesiredWidgetSize)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(*aPresContext, aReflowState, styleSize);

  // subclasses should always override this method, but if not and no css, make it small
  aDesiredLayoutSize.width  = (styleSize.width  > CSS_NOTSET) ? styleSize.width  : 144;
  aDesiredLayoutSize.height = (styleSize.height > CSS_NOTSET) ? styleSize.height : 144;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
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
    GetView(view);
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
  // add ourself as an nsIFormControlFrame
  nsFormFrame::AddFormControlFrame(aPresContext, *this);
  return NS_OK;
}

NS_METHOD
nsFormControlFrame::Reflow(nsIPresContext&      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&      aStatus)
{
  nsresult result = NS_OK;

  nsIDeviceContext* dx = nsnull;
  dx = aPresContext.GetDeviceContext();
  PRBool supportsWidgets = PR_TRUE;
  //XXX: Temporary flag for getting GFX rendered widgets on the screen
  PRBool useGfxWidgets = PR_FALSE;

  if (PR_FALSE == useGfxWidgets) {
    supportsWidgets = PR_TRUE;
    if (nsnull != dx) { 
      dx->SupportsNativeWidgets(supportsWidgets);
      NS_RELEASE(dx);
    }
  }
  else
    supportsWidgets = PR_FALSE;

  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, mWidgetSize);

  if (!mDidInit) {
	  nsIPresShell   *presShell = aPresContext.GetShell();     // need to release
  	nsIViewManager *viewMan   = presShell->GetViewManager();  // need to release
	  NS_RELEASE(presShell); 
    nsRect boundBox(0, 0, aDesiredSize.width, aDesiredSize.height); 

    // absolutely positioned controls already have a view but not a widget
    nsIView* view = nsnull;
    GetView(view);
    if (nsnull == view) {
      result = nsRepository::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&view);
	    if (!NS_SUCCEEDED(result)) {
	      NS_ASSERTION(0, "Could not create view for form control"); 
        aStatus = NS_FRAME_NOT_COMPLETE;
        return result;
	    }

      nsIFrame* parWithView;
	    nsIView *parView;

      GetParentWithView(parWithView);
	    parWithView->GetView(parView);

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

 	const nsIID& id = GetCID();
    if (PR_TRUE == supportsWidgets) {
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
  //    PostCreateWidget(&aPresContext, aDesiredSize.width, aDesiredSize.height);
  //    mDidInit = PR_TRUE;
	  } else {
	    NS_ASSERTION(0, "could not get widget");
	  }
	}
	
	PostCreateWidget(&aPresContext, aDesiredSize.width, aDesiredSize.height);
    mDidInit = PR_TRUE;

    if ((aDesiredSize.width != boundBox.width) || (aDesiredSize.height != boundBox.height)) {
      viewMan->ResizeView(view, aDesiredSize.width, aDesiredSize.height);
    }

	  NS_IF_RELEASE(viewMan);    
  } 
  
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
//XXX    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
    
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

#if 0
NS_METHOD
nsFormControlFrame::Reflow(nsIPresContext&      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&      aStatus)
{
  nsIView* view = nsnull;
  GetView(view);
  if (nsnull == view) {
    nsresult result = 
      nsRepository::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&view);
    if (NS_OK != result) {
      NS_ASSERTION(0, "Could not create view for form control"); 
      aStatus = NS_FRAME_NOT_COMPLETE;
      return result;
    }
    nsIPresShell   *presShell = aPresContext.GetShell();     // need to release
    nsIViewManager *viewMan   = presShell->GetViewManager();  // need to release
    NS_RELEASE(presShell); 

    GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, mWidgetSize);

    //nsRect boundBox(0, 0, mWidgetSize.width, mWidgetSize.height); 
    nsRect boundBox(0, 0, aDesiredSize.width, aDesiredSize.height); 

    nsIFrame* parWithView;
    nsIView *parView;

    GetParentWithView(parWithView);
    parWithView->GetView(parView);

    // initialize the view as hidden since we don't know the (x,y) until Paint
    result = view->Init(viewMan, boundBox, parView,
                        nsnull, nsViewVisibility_kHide);
    if (NS_OK != result) {
      NS_ASSERTION(0, "view initialization failed"); 
      aStatus = NS_FRAME_NOT_COMPLETE;
      return NS_OK;
    }

    viewMan->InsertChild(parView, view, 0);

    const nsIID& id = GetCID();
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
      PostCreateWidget(&aPresContext, aDesiredSize.width, aDesiredSize.height);
      mDidInit = PR_TRUE;
    } else {
      NS_ASSERTION(0, "could not get widget");
    }

    SetView(view);

    if ((aDesiredSize.width != boundBox.width) || (aDesiredSize.height != boundBox.height)) {
      viewMan->ResizeView(view, aDesiredSize.width, aDesiredSize.height);
    }

    NS_IF_RELEASE(viewMan);  
  }
  else {
    GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, mWidgetSize);

    // If we are being reflowed and have a view, hide the view until
    // we are told to paint (which is when our location will have
    // stabilized).
  }

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
    
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

#endif

NS_IMETHODIMP
nsFormControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aHint)
{
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
    aPresContext.GetCompatibilityMode(mode);
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

NS_IMETHODIMP
nsFormControlFrame::GetSize(PRInt32* aSize) const
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
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

NS_METHOD nsFormControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  // make sure that the widget in the event is this input
  // XXX if there is no view, it could be an image button. Unfortunately,
  // every image button will get every event.
  nsIView* view;
  GetView(view);
  if (view) {
    if (mWidget != aEvent->widget) {
      aEventStatus = nsEventStatus_eIgnore;
      return NS_OK;
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

nsCompatibility
GetRepChars(nsIPresContext& aPresContext, char& char1, char& char2) 
{
  nsCompatibility mode;
  aPresContext.GetCompatibilityMode(mode);
  if (eCompatibility_Standard == mode) {
    char1 = 'm';
    char2 = 'a';
    return eCompatibility_Standard;
  } else {
    char1 = '%';
    char2 = '%';
    return eCompatibility_NavQuirks;
  }
}

nscoord 
nsFormControlFrame::GetTextSize(nsIPresContext& aPresContext, nsFormControlFrame* aFrame,
                                const nsString& aString, nsSize& aSize,
                                nsIRenderingContext *aRendContext)
{
  nsFont font(aPresContext.GetDefaultFixedFont());
  aFrame->GetFont(&aPresContext, font);
  //printf("\n GetTextSize %s", aString.ToNewCString());
  nsIDeviceContext* deviceContext = aPresContext.GetDeviceContext();

  nsIFontMetrics* fontMet;
  deviceContext->GetMetricsFor(font, fontMet);
  aRendContext->SetFont(fontMet);
  aRendContext->GetWidth(aString, aSize.width);
  fontMet->GetHeight(aSize.height);

  char char1, char2;
  nsCompatibility mode = GetRepChars(aPresContext, char1, char2);
  nscoord char1Width, char2Width;
  aRendContext->GetWidth(char1, char1Width);
  aRendContext->GetWidth(char2, char2Width);

  NS_RELEASE(fontMet);
  NS_RELEASE(deviceContext);

  if (eCompatibility_Standard == mode) {
    return ((char1Width + char2Width) / 2) + 1;
  } else {
    return char1Width;
  }
}  
  
nscoord
nsFormControlFrame::GetTextSize(nsIPresContext& aPresContext, nsFormControlFrame* aFrame,
                                PRInt32 aNumChars, nsSize& aSize,
                                nsIRenderingContext *aRendContext)
{
  nsAutoString val;
  char char1, char2;
  GetRepChars(aPresContext, char1, char2);
  int i;
  for (i = 0; i < aNumChars; i+=2) {
    val += char1;  
  }
  for (i = 1; i < aNumChars; i+=2) {
    val += char2;  
  }
  return GetTextSize(aPresContext, aFrame, val, aSize, aRendContext);
}
  
PRInt32
nsFormControlFrame::CalculateSize (nsIPresContext* aPresContext, nsFormControlFrame* aFrame,
                             const nsSize& aCSSSize, nsInputDimensionSpec& aSpec, 
                             nsSize& aBounds, PRBool& aWidthExplicit, 
                             PRBool& aHeightExplicit, nscoord& aRowHeight,
                             nsIRenderingContext *aRendContext) 
{
  nscoord charWidth   = 0;
  PRInt32 numRows     = ATTR_NOTSET;
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  aBounds.width  = CSS_NOTSET;
  aBounds.height = CSS_NOTSET;
  nsSize textSize(0,0);

  nsIContent* iContent = nsnull;
  aFrame->GetContent((nsIContent*&) iContent);
  if (!iContent) {
    return 0;
  }
  nsIHTMLContent* hContent = nsnull;
  nsresult result = iContent->QueryInterface(kIHTMLContentIID, (void**)&hContent);
  if ((NS_OK != result) || !hContent) {
    NS_RELEASE(iContent);
    return 0;
  }
  nsAutoString valAttr;
  nsresult valStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColValueAttr) {
    valStatus = hContent->GetAttribute(kNameSpaceID_HTML, aSpec.mColValueAttr, valAttr);
  }
  nsHTMLValue colAttr;
  nsresult colStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColSizeAttr) {
    colStatus = hContent->GetHTMLAttribute(aSpec.mColSizeAttr, colAttr);
  }
  float p2t;
  aPresContext->GetScaledPixelsToTwips(p2t);

  // determine the width
  nscoord adjSize;
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
    if (aSpec.mColSizeAttrInPixels) {
      col = (col <= 0) ? 15 : col; 
      aBounds.width = NSIntPixelsToTwips(col, p2t);
    }
    else {
      col = (col <= 0) ? 1 : col;
      charWidth = GetTextSize(*aPresContext, aFrame, col, aBounds, aRendContext);
      aRowHeight = aBounds.height;  // XXX aBounds.height has CSS_NOTSET
    }
    if (aSpec.mColSizeAttrInPixels) {
      aWidthExplicit = PR_TRUE;
    }
  }
  else {
    if (CSS_NOTSET != aCSSSize.width) {  // css provides width
      aBounds.width = (aCSSSize.width > 0) ? aCSSSize.width : 1;
      aWidthExplicit = PR_TRUE;
    } 
    else {                       
      if (NS_CONTENT_ATTR_HAS_VALUE == valStatus) { // use width of initial value 
        charWidth = GetTextSize(*aPresContext, aFrame, valAttr, aBounds, aRendContext);
      }
      else if (aSpec.mColDefaultValue) { // use default value
        charWidth = GetTextSize(*aPresContext, aFrame, *aSpec.mColDefaultValue, aBounds, aRendContext);
      }
      else if (aSpec.mColDefaultSizeInPixels) {    // use default width in pixels
        charWidth = GetTextSize(*aPresContext, aFrame, 1, aBounds, aRendContext);
        aBounds.width = aSpec.mColDefaultSize;
      }
      else  {                                    // use default width in num characters
        charWidth = GetTextSize(*aPresContext, aFrame, aSpec.mColDefaultSize, aBounds, aRendContext); 
      }
      aRowHeight = aBounds.height; // XXX aBounds.height has CSS_NOTSET
    }
  }

  // determine the height
  nsHTMLValue rowAttr;
  nsresult rowStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mRowSizeAttr) {
    rowStatus = hContent->GetHTMLAttribute(aSpec.mRowSizeAttr, rowAttr);
  }
  if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
    PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
    adjSize = (rowAttrInt > 0) ? rowAttrInt : 1;
    if (0 == charWidth) {
      charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize, aRendContext);
      aBounds.height = textSize.height * adjSize;
      aRowHeight = textSize.height;
      numRows = adjSize;
    }
    else {
      aBounds.height = aBounds.height * adjSize;
    }
  }
  else if (CSS_NOTSET != aCSSSize.height) {  // css provides height
    aBounds.height = (aCSSSize.height > 0) ? aCSSSize.height : 1;
    aHeightExplicit = PR_TRUE;
  } 
  else {         // use default height in num lines
    if (0 == charWidth) {  
      charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize, aRendContext);
      aBounds.height = textSize.height * aSpec.mRowDefaultSize;
      aRowHeight = textSize.height;
    } 
    else {
      aBounds.height = aBounds.height * aSpec.mRowDefaultSize;
    }
  }

  if ((0 == charWidth) || (0 == textSize.width)) {
    charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize, aRendContext);
    aRowHeight = textSize.height;
  }

  // add inside padding if necessary
  if (!aWidthExplicit) {
    aBounds.width += (2 * aFrame->GetHorizontalInsidePadding(*aPresContext, p2t, aBounds.width, charWidth));
  }
  if (!aHeightExplicit) {
    aBounds.height += (2 * aFrame->GetVerticalInsidePadding(p2t, textSize.height)); 
  }

  NS_RELEASE(hContent);
  if (ATTR_NOTSET == numRows) {
    numRows = aBounds.height / aRowHeight;
  }

  NS_RELEASE(iContent);
  return numRows;
}


// this handles all of the input types rather than having them do it.
NS_IMETHODIMP 
nsFormControlFrame::GetFont(nsIPresContext* aPresContext, nsFont& aFont)
{
  const nsStyleFont* styleFont = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(mode);

  if (eCompatibility_Standard == mode) {
    aFont = styleFont->mFont;
    return NS_OK;
  }

  PRInt32 type;
  GetType(&type);
  switch (type) {
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_TEXTAREA:
    case NS_FORM_INPUT_PASSWORD:
      aFont = styleFont->mFixedFont;
      break;
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_SELECT:
      if ((styleFont->mFlags & NS_STYLE_FONT_FACE_EXPLICIT) || 
          (styleFont->mFlags & NS_STYLE_FONT_SIZE_EXPLICIT)) {
        aFont = styleFont->mFixedFont;
        aFont.weight = NS_FONT_WEIGHT_NORMAL;  // always normal weight
        aFont.size = styleFont->mFont.size;    // normal font size
        if (0 == (styleFont->mFlags & NS_STYLE_FONT_FACE_EXPLICIT)) {
          aFont.name = "Arial";  // XXX windows specific font
        }
      } else {
        // use arial, scaled down one HTML size
        // italics, decoration & variant(?) get used
        aFont = styleFont->mFont;
        aFont.name = "Arial";  // XXX windows specific font
        aFont.weight = NS_FONT_WEIGHT_NORMAL; 
        const nsFont& normal = aPresContext->GetDefaultFont();
        PRInt32 scaler;
        aPresContext->GetFontScaler(scaler);
        float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
        PRInt32 fontIndex = nsStyleUtil::FindNextSmallerFontSize(aFont.size, (PRInt32)normal.size, scaleFactor);
        aFont.size = nsStyleUtil::CalcFontPointSize(fontIndex, (PRInt32)normal.size, scaleFactor);
      }
      break;
  }
  return NS_OK;
}

void
nsFormControlFrame::Reset()
{
}

nsresult nsFormControlFrame::GetDefaultCheckState(PRBool *aState)
{
	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    res = inputElement->GetDefaultChecked(aState);
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


//
//-------------------------------------------------------------------------------------
// Utility methods for rendering Form Elements using GFX
//-------------------------------------------------------------------------------------
//
// XXX: The following location for the paint code is TEMPORARY. 
// It is being used to get printing working
// under windows. Later it will be used to GFX-render the controls to the display. 
// Expect this code to repackaged and moved to a new location in the future.


void 
nsFormControlFrame::PaintLine(nsIRenderingContext& aRenderingContext, 
                              nscoord aSX, nscoord aSY, nscoord aEX, nscoord aEY, 
                              PRBool aHorz, nscoord aWidth, nscoord aOnePixel)
{
  
  nsPoint p[5];
  if (aHorz) {
    aEX++;
    p[0].x = nscoord(float(aSX)*aOnePixel);
    p[0].y = nscoord(float(aSY)*aOnePixel);
    p[1].x = nscoord(float(aEX)*aOnePixel);
    p[1].y = nscoord(float(aEY)*aOnePixel);
    p[2].x = nscoord(float(aEX)*aOnePixel);
    p[2].y = nscoord(float(aEY+1)*aOnePixel);
    p[3].x = nscoord(float(aSX)*aOnePixel);
    p[3].y = nscoord(float(aSY+1)*aOnePixel);
    p[4].x = nscoord(float(aSX)*aOnePixel);
    p[4].y = nscoord(float(aSY)*aOnePixel);
  } else {
    aEY++;
    p[0].x = nscoord(float(aSX)*aOnePixel);
    p[0].y = nscoord(float(aSY)*aOnePixel);
    p[1].x = nscoord(float(aEX)*aOnePixel);
    p[1].y = nscoord(float(aEY)*aOnePixel);
    p[2].x = nscoord(float(aEX+1)*aOnePixel);
    p[2].y = nscoord(float(aEY)*aOnePixel);
    p[3].x = nscoord(float(aSX+1)*aOnePixel);
    p[3].y = nscoord(float(aSY)*aOnePixel);
    p[4].x = nscoord(float(aSX)*aOnePixel);
    p[4].y = nscoord(float(aSY)*aOnePixel);
  }
  aRenderingContext.FillPolygon(p, 5);

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Offset for arrow centerpoint 
const nscoord nsArrowOffsetX = 3;
const nscoord nsArrowOffsetY = 3;
nscoord nsArrowRightPoints[]  = {0, 0, 0, 6, 6, 3 };
nscoord nsArrowLeftPoints[]   = {0, 3, 6, 0, 6, 6 };
nscoord nsArrowUpPoints[]     = {3, 0, 6, 6, 0, 6 }; 
nscoord nsArrowDownPoints[]   = {0, 0, 3, 6, 6, 0 };

//---------------------------------------------------------------------------
void 
nsFormControlFrame::SetupPoints(PRUint32 aNumberOfPoints, nscoord* aPoints, nsPoint* aPolygon, nscoord aScaleFactor, nscoord aX, nscoord aY,
                                nscoord aCenterX, nscoord aCenterY) 
{
  const nscoord offsetX = aCenterX * aScaleFactor;
  const nscoord offsetY = aCenterY * aScaleFactor;

  PRUint32 i = 0;
  PRUint32 count = 0;
  for (i = 0; i < aNumberOfPoints; i++) {
    aPolygon[i].x = (aPoints[count] * aScaleFactor) + aX - offsetX;
    count++;
    aPolygon[i].y = (aPoints[count] * aScaleFactor) + aY - offsetY;
    count++;
  }
}


//---------------------------------------------------------------------------
void 
nsFormControlFrame::PaintArrowGlyph(nsIRenderingContext& aRenderingContext, 
                 nscoord aSX, nscoord aSY, nsArrowDirection aArrowDirection,
								 nscoord aOnePixel)
{
  nsPoint polygon[3];

  switch(aArrowDirection)
  {
    case eArrowDirection_Left:
			SetupPoints(3, nsArrowLeftPoints, polygon, aOnePixel, aSX, aSY, nsArrowOffsetX, nsArrowOffsetY);
		break;

    case eArrowDirection_Right:
			SetupPoints(3, nsArrowRightPoints, polygon, aOnePixel, aSX, aSY, nsArrowOffsetX, nsArrowOffsetY);
		break;

    case eArrowDirection_Up:
			SetupPoints(3, nsArrowUpPoints, polygon, aOnePixel, aSX, aSY, nsArrowOffsetX, nsArrowOffsetY);
		break;

    case eArrowDirection_Down:
			SetupPoints(3, nsArrowDownPoints, polygon, aOnePixel, aSX, aSY, nsArrowOffsetX, nsArrowOffsetY);
		break;
  }
 
  aRenderingContext.FillPolygon(polygon, 3);
}

//---------------------------------------------------------------------------
void 
nsFormControlFrame::PaintArrow(nsArrowDirection aArrowDirection,
					                    nsIRenderingContext& aRenderingContext,
															nsIPresContext& aPresContext, 
															const nsRect& aDirtyRect,
                              nsRect& aRect, 
															nscoord aOnePixel, 
															const nsStyleColor& aColor,
															const nsStyleSpacing& aSpacing,
															nsIFrame* aForFrame,
                              nsRect& aFrameRect)
{
  // Draw border using CSS
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, aRect,  aColor, 0, 0);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                               aDirtyRect, aRect, aSpacing, 0);

  // Draw the glyph in black
  aRenderingContext.SetColor(NS_RGB(0, 0, 0));
   // Draw arrow centered in the rectangle
  PaintArrowGlyph(aRenderingContext, aRect.x + (aRect.width / 2), aRect.y + (aRect.height / 2), aArrowDirection, aOnePixel);

}

//---------------------------------------------------------------------------
void 
nsFormControlFrame::PaintScrollbar(nsIRenderingContext& aRenderingContext,
																	nsIPresContext& aPresContext, 
																  const nsRect& aDirtyRect,
                                  nsRect& aRect, 
																  PRBool aHorizontal, 
																  nscoord aOnePixel, 
                                  nsIStyleContext* aScrollbarStyleContext,
                                  nsIStyleContext* aScrollbarArrowStyleContext,
																	nsIFrame* aForFrame,
                                  nsRect& aFrameRect)
{
  // Get the Scrollbar's Style structs
  const nsStyleSpacing* scrollbarSpacing =
    (const nsStyleSpacing*)aScrollbarStyleContext->GetStyleData(eStyleStruct_Spacing);
  const nsStyleColor* scrollbarColor =
    (const nsStyleColor*)aScrollbarStyleContext->GetStyleData(eStyleStruct_Color);

  // Get the Scrollbar's Arrow's Style structs
  const nsStyleSpacing* arrowSpacing =
    (const nsStyleSpacing*)aScrollbarArrowStyleContext->GetStyleData(eStyleStruct_Spacing);
  const nsStyleColor* arrowColor =
    (const nsStyleColor*)aScrollbarArrowStyleContext->GetStyleData(eStyleStruct_Color);

  // Paint background for scrollbar
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, aRect, *scrollbarColor, 0, 0);

  if (PR_TRUE == aHorizontal) {
    // Draw horizontal Arrow
		nscoord arrowWidth = aRect.height;
		nsRect arrowLeftRect(aRect.x, aRect.y, arrowWidth, arrowWidth);
    PaintArrow(eArrowDirection_Left,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowLeftRect,aOnePixel, *arrowColor, *arrowSpacing, aForFrame, aFrameRect);

		nsRect thumbRect(aRect.x+arrowWidth, aRect.y, arrowWidth, arrowWidth);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, thumbRect, *arrowColor, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                                aDirtyRect, thumbRect, *arrowSpacing, 0);

    nsRect arrowRightRect(aRect.x + (aRect.width - arrowWidth), aRect.y, arrowWidth, arrowWidth);
	  PaintArrow(eArrowDirection_Right,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowRightRect,aOnePixel, *arrowColor, *arrowSpacing, aForFrame, aFrameRect);

	}
  else {
		// Paint vertical arrow
		nscoord arrowHeight = aRect.width;
		nsRect arrowUpRect(aRect.x, aRect.y, arrowHeight, arrowHeight);
	  PaintArrow(eArrowDirection_Up,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowUpRect,aOnePixel, *arrowColor, *arrowSpacing, aForFrame, aFrameRect);

		nsRect thumbRect(aRect.x, aRect.y+arrowHeight, arrowHeight, arrowHeight);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, thumbRect, *arrowColor, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                                aDirtyRect, thumbRect, *arrowSpacing, 0);

		nsRect arrowDownRect(aRect.x, aRect.y + (aRect.height - arrowHeight), arrowHeight, arrowHeight);
	  PaintArrow(eArrowDirection_Down,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowDownRect,aOnePixel, *arrowColor, *arrowSpacing, aForFrame, aFrameRect);
  }

}


void
nsFormControlFrame::PaintFixedSizeCheckMark(nsIRenderingContext& aRenderingContext,
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
  PaintLine(aRenderingContext, 0 + ox, 2 + oy, 0 + ox, 4 + oy, PR_FALSE, 1, onePixel);
  PaintLine(aRenderingContext, 1 + ox, 3 + oy, 1 + ox, 5 + oy, PR_FALSE, 1, onePixel);
  PaintLine(aRenderingContext, 2 + ox, 4 + oy, 2 + ox, 6 + oy, PR_FALSE, 1, onePixel);
  PaintLine(aRenderingContext, 3 + ox, 3 + oy, 3 + ox, 5 + oy, PR_FALSE, 1, onePixel);
  PaintLine(aRenderingContext, 4 + ox, 2 + oy, 4 + ox, 4 + oy, PR_FALSE, 1, onePixel);
  PaintLine(aRenderingContext, 5 + ox, 1 + oy, 5 + ox, 3 + oy, PR_FALSE, 1, onePixel);
  PaintLine(aRenderingContext, 6 + ox, 0 + oy, 6 + ox, 2 + oy, PR_FALSE, 1, onePixel);
}

void
nsFormControlFrame::PaintFixedSizeCheckMarkBorder(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips, const nsStyleColor& aBackgroundColor)
{
//XXX: This method should use the CSS Border rendering code.

    // Offsets to x,y location
  PRUint32 ox = 0;
  PRUint32 oy = 0;

  nscoord onePixel = NSIntPixelsToTwips(1, aPixelsToTwips);
  nscoord twoPixels = NSIntPixelsToTwips(2, aPixelsToTwips);
  nscoord ninePixels = NSIntPixelsToTwips(9, aPixelsToTwips);
  nscoord twelvePixels = NSIntPixelsToTwips(12, aPixelsToTwips);

    // Draw Background
 
  aRenderingContext.SetColor(aBackgroundColor.mBackgroundColor);
  nsRect rect(0, 0, twelvePixels, twelvePixels);
  aRenderingContext.FillRect(rect);

    // Draw Border
  aRenderingContext.SetColor(NS_RGB(128, 128, 128));
  PaintLine(aRenderingContext, 0 + ox, 0 + oy, 11 + ox, 0 + oy, PR_TRUE, 1, onePixel);
  PaintLine(aRenderingContext, 0 + ox, 0 + oy, 0 + ox, 11 + oy, PR_FALSE, 1, onePixel);

  aRenderingContext.SetColor(NS_RGB(192, 192, 192));
  PaintLine(aRenderingContext, 1 + ox, 11 + oy, 11 + ox, 11 + oy, PR_TRUE, 1, onePixel);
  PaintLine(aRenderingContext, 11 + ox, 1 + oy, 11 + ox, 11 + oy, PR_FALSE, 1, onePixel);

  aRenderingContext.SetColor(NS_RGB(0, 0, 0));
  PaintLine(aRenderingContext, 1 + ox, 1 + oy, 10 + ox, 1 + oy, PR_TRUE, 1, onePixel);
  PaintLine(aRenderingContext, 1 + ox, 1 + oy, 1 + ox, 10 + oy, PR_FALSE, 1, onePixel);

}

#if 0
//XXX: Future, use the following code to draw a checkmark in any size.
void
nsFormControlFrame::PaintScaledCheckMark(nsIRenderingContext& aRenderingContext,
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
nsFormControlFrame::PaintFocus(nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect, nsRect& aInside, nsRect& aOutside)
                      
{
      // draw dashed line to indicate selection, XXX don't calc rect every time
    PRUint8 borderStyles[4];
    nscolor borderColors[4];
    nscolor black = NS_RGB(0,0,0);
    for (PRInt32 i = 0; i < 4; i++) {
      borderStyles[i] = NS_STYLE_BORDER_STYLE_DOTTED;
      borderColors[i] = black;
    }
    nsCSSRendering::DrawDashedSides(0, aRenderingContext, borderStyles, borderColors, aOutside,
                                    aInside, PR_FALSE, nsnull);
}


void 
nsFormControlFrame::PaintRectangularButton(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect, PRUint32 aWidth, 
                            PRUint32 aHeight, PRBool aShift, PRBool aShowFocus,
                            nsIStyleContext* aStyleContext, nsString& aLabel, nsFormControlFrame* aForFrame)
                          
{
    aRenderingContext.PushState();
      // Draw border using CSS
     // Get the Scrollbar's Arrow's Style structs
    const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
    const nsStyleColor* color =
    (const nsStyleColor*)aStyleContext->GetStyleData(eStyleStruct_Color);
    nsRect rect(0, 0, aWidth, aHeight);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, rect,  *color, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                               aDirtyRect, rect, *spacing, 0);

    nsMargin border;
    spacing->CalcBorderFor(aForFrame, border);

    float p2t;
    aPresContext.GetScaledPixelsToTwips(p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    nsRect outside(0, 0, aWidth, aHeight);
    outside.Deflate(border);
    outside.Deflate(onePixel, onePixel);

    nsRect inside(outside);
    inside.Deflate(onePixel, onePixel);


    if (aShowFocus) { 
      PaintFocus(aRenderingContext,
                 aDirtyRect, inside, outside);
    }

    float appUnits;
    float devUnits;
    float scale;
    nsIDeviceContext * context;
    aRenderingContext.GetDeviceContext(context);

    context->GetCanonicalPixelScale(scale);
    context->GetAppUnitsToDevUnits(devUnits);
    context->GetDevUnitsToAppUnits(appUnits);

    aRenderingContext.SetColor(NS_RGB(0,0,0));
 
    nsFont font(aPresContext.GetDefaultFixedFont()); 
    aForFrame->GetFont(&aPresContext, font);

    aRenderingContext.SetFont(font);

    nscoord textWidth;
    nscoord textHeight;
    aRenderingContext.GetWidth(aLabel, textWidth);

    nsIFontMetrics* metrics;
    context->GetMetricsFor(font, metrics);
    metrics->GetMaxAscent(textHeight);

    nscoord x = ((inside.width  - textWidth) / 2)  + inside.x;
    nscoord y = ((inside.height - textHeight) / 2) + inside.y;
    if (PR_TRUE == aShift) {
      x += onePixel;
      y += onePixel;
    }

    aRenderingContext.DrawString(aLabel, x, y, 0);
    NS_RELEASE(context);
    PRBool clipEmpty;
    aRenderingContext.PopState(clipEmpty);
}


void
nsFormControlFrame::PaintCircularBorder(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect, nsIStyleContext* aStyleContext, PRBool aInset,
                         nsIFrame* aForFrame, PRUint32 aWidth, PRUint32 aHeight)
{
  aRenderingContext.PushState();

  float p2t;
  aPresContext.GetScaledPixelsToTwips(p2t);
 
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  spacing->CalcBorderFor(aForFrame, border);

  nscoord onePixel     = NSIntPixelsToTwips(1, p2t);
  nscoord twelvePixels = NSIntPixelsToTwips(12, p2t);

  nsRect outside(0, 0, aWidth, aHeight);

  aRenderingContext.SetColor(NS_RGB(192,192,192));
  aRenderingContext.FillRect(outside);

  PRBool standardSize = PR_FALSE;
  if (standardSize) {
    outside.SetRect(0, 0, twelvePixels, twelvePixels);
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillArc(outside, 0, 180);
    aRenderingContext.FillArc(outside, 180, 360);

    if (PR_TRUE == aInset) {
      outside.Deflate(onePixel, onePixel);
      outside.Deflate(onePixel, onePixel);
      aRenderingContext.SetColor(NS_RGB(192,192,192));
      aRenderingContext.FillArc(outside, 0, 180);
      aRenderingContext.FillArc(outside, 180, 360);
      outside.SetRect(0, 0, twelvePixels, twelvePixels);
    }

    // DrakGray
    aRenderingContext.SetColor(NS_RGB(128,128,128));
    PaintLine(aRenderingContext, 4, 0, 7, 0, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 2, 1, 3, 1, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 8, 1, 9, 1, PR_TRUE, 1, onePixel);

    PaintLine(aRenderingContext, 1, 2, 1, 3, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 0, 4, 0, 7, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 1, 8, 1, 9, PR_FALSE, 1, onePixel);

    // Black
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    PaintLine(aRenderingContext, 4, 1, 7, 1, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 2, 2, 3, 2, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 8, 2, 9, 2, PR_TRUE, 1, onePixel);

    PaintLine(aRenderingContext, 2, 2, 2, 3, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 1, 4, 1, 7, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 2, 8, 2, 8, PR_FALSE, 1, onePixel);

    // Gray
    aRenderingContext.SetColor(NS_RGB(192, 192, 192));
    PaintLine(aRenderingContext, 2, 9, 3, 9, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 8, 9, 9, 9, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 4, 10, 7, 10, PR_TRUE, 1, onePixel);

    PaintLine(aRenderingContext, 9, 3, 9, 3, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 10, 4, 10, 7, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 9, 8, 9, 9, PR_FALSE, 1, onePixel);

    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
  } else {
    outside.SetRect(0, 0, twelvePixels, twelvePixels);

    aRenderingContext.SetColor(NS_RGB(128,128,128));
    aRenderingContext.FillArc(outside, 46, 225);
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillArc(outside, 225, 360);
    aRenderingContext.FillArc(outside, 0, 44);

    outside.Deflate(onePixel, onePixel);
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.FillArc(outside, 46, 225);
    aRenderingContext.SetColor(NS_RGB(192,192,192));
    aRenderingContext.FillArc(outside, 225, 360);
    aRenderingContext.FillArc(outside, 0, 44);

    outside.Deflate(onePixel, onePixel);
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillArc(outside, 0, 180);
    aRenderingContext.FillArc(outside, 180, 360);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
  }

  PRBool clip;
  aRenderingContext.PopState(clip);
}



