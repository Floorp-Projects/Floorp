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

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);


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
                    const nsRect& aDirtyRect)
{
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    // Make sure the widget is visible if it isn't currently visible
//    if (PR_FALSE == mDidInit) {
//      PostCreateWidget(&aPresContext);
//      mDidInit = PR_TRUE;
//    }
    // Point borders/padding if any
    return nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
  }
  return NS_OK;
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
  if (nsnull != dx) { 
	PRBool native = PR_FALSE;
    dx->SupportsNativeWidgets(supportsWidgets);
    NS_RELEASE(dx);
  }

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
    result = content->GetAttribute(nsHTMLAtoms::maxlength, value);
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
    result = content->GetAttribute(nsHTMLAtoms::size, value);
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
      result = formControl->GetAttribute(nsHTMLAtoms::name, value);
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
      result = formControl->GetAttribute(nsHTMLAtoms::value, value);
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
    aSize.width = aReflowState.minWidth;
  }
  else {
    aSize.width = CSS_NOTSET;
  }
  if (aReflowState.HaveFixedContentHeight()) {
    aSize.height = aReflowState.minHeight;
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
    valStatus = hContent->GetAttribute(aSpec.mColValueAttr, valAttr);
  }
  nsHTMLValue colAttr;
  nsresult colStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColSizeAttr) {
    colStatus = hContent->GetAttribute(aSpec.mColSizeAttr, colAttr);
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
    rowStatus = hContent->GetAttribute(aSpec.mRowSizeAttr, rowAttr);
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


void 
nsFormControlFrame::DrawLine(nsIRenderingContext& aRenderingContext, 
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


