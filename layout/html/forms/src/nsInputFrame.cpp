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

#include "nsInput.h"
#include "nsInputFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
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
#include "nsIFontCache.h"
#include "nsIFontMetrics.h"
#include "nsIFormManager.h"
#include "nsIDeviceContext.h"
#include "nsHTMLAtoms.h"
#include "nsIButton.h"  // remove this when GetCID is pure virtual
#include "nsICheckButton.h"  //remove this
#include "nsITextWidget.h"  //remove this
#include "nsISupports.h"
#include "nsHTMLForms.h"
#include "nsStyleConsts.h"
#include "nsUnitConversion.h"
#include "nsCSSLayout.h"
#include "nsStyleUtil.h"


static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);


nsInputFrame::nsInputFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsInputFrameSuper(aContent, aParentFrame)
{
  mLastMouseState = eMouseNone;
  mDidInit = PR_FALSE;
}

nsInputFrame::~nsInputFrame()
{
  //printf("nsInputFrame::~nsInputFrame \n");
}

NS_METHOD nsInputFrame::SetRect(const nsRect& aRect)
{
  return nsInputFrameSuper::SetRect(aRect);
}

nsFormRenderingMode nsInputFrame::GetMode() const
{
  nsInput* content = (nsInput *)mContent;
  nsIFormManager* formMan = content->GetFormManager();
  if (formMan) {
    nsFormRenderingMode mode = formMan->GetMode();
    NS_RELEASE(formMan);
    return mode;
  }
  else {
    return kBackwardMode;
  }
}

nscoord nsInputFrame::GetScrollbarWidth(float aPixToTwip)
{
   return NSIntPixelsToTwips(19, aPixToTwip);  // XXX this is windows
}

nscoord nsInputFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}

nscoord nsInputFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

nscoord nsInputFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}

nscoord nsInputFrame::GetHorizontalInsidePadding(float aPixToTwip, 
                                                 nscoord aInnerWidth,
                                                 nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPixToTwip, aInnerWidth);
}

// XXX it would be cool if form element used our rendering sw, then
// they could be blended, and bordered, and so on...
NS_METHOD
nsInputFrame::Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect)
{
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    // Make sure the widget is visible if it isn't currently visible
    nsIView* view = nsnull;
    GetView(view);
    if (nsnull != view) {
      if (PR_FALSE == mDidInit) {
        nsIFormManager* formMan = ((nsInput*)mContent)->GetFormManager();
        if (formMan) {
          formMan->Init(PR_FALSE);
          NS_RELEASE(formMan);
        }
        PostCreateWidget(&aPresContext, view);
        mDidInit = PR_TRUE;
      }
//      SetViewVisiblity(&aPresContext, PR_TRUE);
    }
    // Point borders/padding if any
    return nsInputFrameSuper::Paint(aPresContext, aRenderingContext, aDirtyRect);
  }
  return NS_OK;
}

//don't call this. MMP
void
nsInputFrame::SetViewVisiblity(nsIPresContext* aPresContext, PRBool aShow)
{
  nsIView* view = nsnull;
  GetView(view);
  if (nsnull != view) {
    nsIWidget* widget;
    nsresult result = GetWidget(view, &widget);
    if (NS_OK == result) {
      // initially the widget was created as hidden
      nsViewVisibility newVisibility =
        aShow ? nsViewVisibility_kShow : nsViewVisibility_kHide;
      nsViewVisibility  currentVisibility;
      view->GetVisibility(currentVisibility);
      if (newVisibility != currentVisibility) {
        // this only inits the 1st time
        // XXX kipp says: this is yucky; init on first visibility seems lame
        // XXX is this even called
        nsIFormManager* formMan = ((nsInput*)mContent)->GetFormManager();
        if (formMan) {
          formMan->Init(PR_FALSE);
          NS_RELEASE(formMan);
        }
        view->SetVisibility(newVisibility);
        PostCreateWidget(aPresContext, view);
      }
      NS_IF_RELEASE(widget);
    }
  }
}

PRBool 
nsInputFrame::BoundsAreSet()
{
  if ((0 != mRect.width) || (0 != mRect.height)) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

PRBool
nsInputFrame::IsHidden()
{
  nsInput* content = (nsInput *)mContent; 
  return content->IsHidden();
}

void 
nsInputFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsReflowState& aReflowState,
                             nsReflowMetrics& aDesiredLayoutSize,
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
nsInputFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsReflowState& aReflowState,
                             nsReflowMetrics& aDesiredSize)
{
  nsSize ignore;
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize, ignore);
}

NS_IMETHODIMP
nsInputFrame::DidReflow(nsIPresContext& aPresContext,
                        nsDidReflowStatus aStatus)
{
  nsresult rv = nsInputFrameSuper::DidReflow(aPresContext, aStatus);

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

NS_METHOD
nsInputFrame::Reflow(nsIPresContext&      aPresContext,
                     nsReflowMetrics&     aDesiredSize,
                     const nsReflowState& aReflowState,
                     nsReflowStatus&      aStatus)
{
  nsIView* view = nsnull;
  GetView(view);
  if (nsnull == view) {
    static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
    static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

    // make sure the style context is set
    if (nsnull == mStyleContext) {
      GetStyleContext(&aPresContext, mStyleContext);
    }
    nsresult result = 
	    nsRepository::CreateInstance(kViewCID, nsnull, kIViewIID,
                                   (void **)&view);
	  if (NS_OK != result) {
	    NS_ASSERTION(0, "Could not create view for button"); 
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

	  const nsIID& id = GetCID();
    nsWidgetInitData* initData = GetWidgetInitData(aPresContext); // needs to be deleted
	  // initialize the view as hidden since we don't know the (x,y) until Paint
    result = view->Init(viewMan, boundBox, parView, &id, initData,
                        nsnull, 0, nsnull,
                        1.0f, nsViewVisibility_kHide);
    if (nsnull != initData) {
      delete(initData);
    }
    if (NS_OK != result) {
	    NS_ASSERTION(0, "widget initialization failed"); 
      aStatus = NS_FRAME_NOT_COMPLETE;
      return NS_OK;
	  }

	  // set the content's widget, so it can get content modified by the widget
	  nsIWidget *widget;
	  result = GetWidget(view, &widget);
	  if (NS_OK == result) {
      nsInput* content = (nsInput *)mContent; // change this cast to QueryInterface 
      content->SetWidget(widget);
	    NS_IF_RELEASE(widget);
	  } else {
	    NS_ASSERTION(0, "could not get widget");
	  }

    viewMan->InsertChild(parView, view, 0);

    SetView(view);
	  NS_IF_RELEASE(viewMan);  
  }
  else {
    GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, mWidgetSize);

    // If we are being reflowed and have a view, hide the view until
    // we are told to paint (which is when our location will have
    // stabilized).
//    SetViewVisiblity(aPresContext, PR_FALSE);
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

nsWidgetInitData* 
nsInputFrame::GetWidgetInitData(nsIPresContext& aPresContext)
{
  return nsnull;
}

void 
nsInputFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
}

nsresult
nsInputFrame::GetWidget(nsIView* aView, nsIWidget** aWidget)
{
  nsIWidget*  widget;
  aView->GetWidget(widget);
  nsresult    result;

  if (nsnull == widget) {
    aWidget = nsnull;
    result = NS_ERROR_FAILURE;

  } else {
    const nsIID id = GetIID();

    result =  widget->QueryInterface(kIWidgetIID, (void**)aWidget);
    if (NS_FAILED(result)) {
      NS_ASSERTION(0, "The widget interface is invalid");  // need to print out more details of the widget
    }
    NS_RELEASE(widget);
  }

  return result;
}

const nsIID&
nsInputFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID&
nsInputFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

NS_METHOD nsInputFrame::HandleEvent(nsIPresContext& aPresContext, 
                                    nsGUIEvent* aEvent,
                                    nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  // make sure that the widget in the event is this
  // XXX if there is no view, it could be an image button. Unfortunately,
  // every image button will get every event.
  nsIView* view;
  GetView(view);
  if (view) {
    nsInput* content = (nsInput *)mContent;
    if (content->GetWidget() != aEvent->widget) {
      aEventStatus = nsEventStatus_eIgnore;
      return NS_OK;
    }
  }

  //printf(" %d %d %d %d (%d,%d) \n", this, aEvent->widget, aEvent->widgetSupports, 
  //       aEvent->message, aEvent->point.x, aEvent->point.y);

  switch (aEvent->message) {
    case NS_MOUSE_ENTER:
	    mLastMouseState = eMouseEnter;
	    break;
    case NS_MOUSE_LEFT_BUTTON_DOWN:
	    mLastMouseState = 
	      (eMouseEnter == mLastMouseState) ? eMouseDown : eMouseNone;
	    break;
    case NS_MOUSE_LEFT_BUTTON_UP:
	    if (eMouseDown == mLastMouseState) {
        float t2p = aPresContext.GetTwipsToPixels();
        ((nsInput*)mContent)->SetClickPoint(NSTwipsToIntPixels(aEvent->point.x, t2p),
                                            NSTwipsToIntPixels(aEvent->point.y, t2p));   

        nsEventStatus mStatus = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_MOUSE_EVENT;
        event.message = NS_MOUSE_LEFT_CLICK;
        mContent->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, mStatus);
        
        if (nsEventStatus_eConsumeNoDefault != mStatus) {
          MouseClicked(&aPresContext);
        }
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

void nsInputFrame::GetStyleSize(nsIPresContext& aPresContext,
                                const nsReflowState& aReflowState,
                                nsSize& aSize)
{
  PRIntn ss = nsCSSLayout::GetStyleSize(&aPresContext, aReflowState, aSize);
  if (0 == (ss & NS_SIZE_HAS_WIDTH)) {
    aSize.width = CSS_NOTSET;
  }
  if (0 == (ss & NS_SIZE_HAS_HEIGHT)) {
    aSize.height = CSS_NOTSET;
  }
}

nscoord 
nsInputFrame::GetTextSize(nsIPresContext& aPresContext, nsInputFrame* aFrame,
                          const nsString& aString, nsSize& aSize)
{
  nsFont font = aPresContext.GetDefaultFixedFont();
  aFrame->GetFont(&aPresContext, font);
  //printf("\n GetTextSize %s", aString.ToNewCString());
  nsIDeviceContext* deviceContext = aPresContext.GetDeviceContext();
  nsIFontCache* fontCache;
  deviceContext->GetFontCache(fontCache);

  nsIFontMetrics* fontMet;
  fontCache->GetMetricsFor(font, fontMet);
  fontMet->GetWidth(aString, aSize.width);
  fontMet->GetHeight(aSize.height);

  nscoord charWidth;
  fontMet->GetWidth("W", charWidth);

  NS_RELEASE(fontMet);
  NS_RELEASE(fontCache);
  NS_RELEASE(deviceContext);

  return charWidth;
}
  
nscoord
nsInputFrame::GetTextSize(nsIPresContext& aPresContext, nsInputFrame* aFrame,
                          PRInt32 aNumChars, nsSize& aSize)
{
  nsAutoString val;
  char repChar = (kBackwardMode == aFrame->GetMode()) ? '%' : 'e';
  for (int i = 0; i < aNumChars; i++) {
    val += repChar;  
  }
  return GetTextSize(aPresContext, aFrame, val, aSize);
}
  
PRInt32
nsInputFrame::CalculateSize (nsIPresContext* aPresContext, nsInputFrame* aFrame,
                             const nsSize& aCSSSize, nsInputDimensionSpec& aSpec, 
                             nsSize& aBounds, PRBool& aWidthExplicit, 
                             PRBool& aHeightExplicit, nscoord& aRowHeight) 
{
  nscoord charWidth   = 0;
  PRInt32 numRows     = ATTR_NOTSET;
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  /* XXX For some reason on Win95 Paint doesn't initially get called */
  /* for elements that are out of the view, so we weren't calling    */
  /* PostCreateWidget. I'm calling it here as a work-around          */
  nsIView* view = nsnull;
  aFrame->GetView(view);
  if (nsnull != view) {
    if (PR_FALSE == aFrame->mDidInit) {
      nsIFormManager* formMan = ((nsInput*)aFrame->mContent)->GetFormManager();
      if (formMan) {
        formMan->Init(PR_FALSE);
        aFrame->PostCreateWidget(aPresContext, view);
        aFrame->mDidInit = PR_TRUE;
        NS_RELEASE(formMan);
      }
    }
  }

  aBounds.width  = CSS_NOTSET;
  aBounds.height = CSS_NOTSET;
  nsSize textSize(0,0);

  nsInput* content;
  aFrame->GetContent((nsIContent*&) content);

  nsAutoString valAttr;
  nsresult valStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColValueAttr) {
    valStatus = ((nsHTMLTagContent*)content)->GetAttribute(aSpec.mColValueAttr, valAttr);
  }
  nsHTMLValue colAttr;
  nsresult colStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColSizeAttr) {
    colStatus = content->GetAttribute(aSpec.mColSizeAttr, colAttr);
  }
  float p2t = aPresContext->GetPixelsToTwips();

  // determine the width
  nscoord adjSize;
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    if (aSpec.mColSizeAttrInPixels) {
      adjSize = (colAttr.GetPixelValue() > 0) ? colAttr.GetPixelValue() : 15;
      aBounds.width = NSIntPixelsToTwips(adjSize, p2t);
    }
    else {
      PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
      if (col <= 0) {
        col = 1;
      }
      charWidth = GetTextSize(*aPresContext, aFrame, col, aBounds);
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
        charWidth = GetTextSize(*aPresContext, aFrame, valAttr, aBounds);
      }
      else if (aSpec.mColDefaultValue) { // use default value
        charWidth = GetTextSize(*aPresContext, aFrame, *aSpec.mColDefaultValue, aBounds);
      }
      else if (aSpec.mColDefaultSizeInPixels) {    // use default width in pixels
        charWidth = GetTextSize(*aPresContext, aFrame, 1, aBounds);
        aBounds.width = aSpec.mColDefaultSize;
      }
      else  {                                    // use default width in num characters
        charWidth = GetTextSize(*aPresContext, aFrame, aSpec.mColDefaultSize, aBounds); 
      }
      aRowHeight = aBounds.height; // XXX aBounds.height has CSS_NOTSET
    }
  }

  // determine the height
  nsHTMLValue rowAttr;
  nsresult rowStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mRowSizeAttr) {
    rowStatus = content->GetAttribute(aSpec.mRowSizeAttr, rowAttr);
  }
  if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
    PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
    adjSize = (rowAttrInt > 0) ? rowAttrInt : 1;
    if (0 == charWidth) {
      charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize);
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
      charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize);
      aBounds.height = textSize.height * aSpec.mRowDefaultSize;
      aRowHeight = textSize.height;
    } 
    else {
      aBounds.height = aBounds.height * aSpec.mRowDefaultSize;
    }
  }

  if ((0 == charWidth) || (0 == textSize.width)) {
    charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize);
    aRowHeight = textSize.height;
  }

  // add inside padding if necessary
  if (!aWidthExplicit) {
    aBounds.width += (2 * aFrame->GetHorizontalInsidePadding(p2t, aBounds.width, charWidth));
  }
  if (!aHeightExplicit) {
    aBounds.height += (2 * aFrame->GetVerticalInsidePadding(p2t, textSize.height)); 
  }

  NS_RELEASE(content);
  if (ATTR_NOTSET == numRows) {
    numRows = aBounds.height / aRowHeight;
  }

  return numRows;
}


// this handles all of the input types rather than having them do it.
const void 
nsInputFrame::GetFont(nsIPresContext* aPresContext, nsFont& aFont)
{
  const nsStyleFont* styleFont = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  nsString type;
  ((nsInput*)mContent)->GetType(type);

  // XXX shouldn't this be atom compares instead?
  if (  type.EqualsIgnoreCase("text")     || 
        type.EqualsIgnoreCase("textarea") ||
        type.EqualsIgnoreCase("password"))   {
    aFont = styleFont->mFixedFont;
  } else if (type.EqualsIgnoreCase("button") || 
             type.EqualsIgnoreCase("submit") ||
             type.EqualsIgnoreCase("reset")  || 
             type.EqualsIgnoreCase("select")) { 
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
      PRInt32 scaler = aPresContext->GetFontScaler();
      float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
      PRInt32 fontIndex = nsStyleUtil::FindNextSmallerFontSize(aFont.size, (PRInt32)normal.size, scaleFactor);
      aFont.size = nsStyleUtil::CalcFontPointSize(fontIndex, (PRInt32)normal.size, scaleFactor);
    }
  }
}



