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

nsInputFrame::nsInputFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsInputFrameSuper(aContent, aParentFrame)
{
  mLastMouseState = eMouseNone;
  mDidInit = PR_FALSE;
}

nsInputFrame::~nsInputFrame()
{
}

PRInt32 nsInputFrame::gScrollBarWidth = 360;

NS_METHOD nsInputFrame::SetRect(const nsRect& aRect)
{
  return nsInputFrameSuper::SetRect(aRect);
}

NS_METHOD
nsInputFrame::MoveTo(nscoord aX, nscoord aY)
{
  //if ( ((aX == 0) && (aY == 0)) || (aX != mRect.x) || (aY != mRect.y)) {
  if ((aX != mRect.x) || (aY != mRect.y)) {
    mRect.x = aX;
    mRect.y = aY;

    // Let the view know
    nsIView* view = nsnull;
    GetView(view);
    if (nsnull != view) {
      // Position view relative to it's parent, not relative to our
      // parent frame (our parent frame may not have a view). Also,
      // inset the view by the border+padding if present
      nsIView* parentWithView;
      nsPoint origin;
      GetOffsetFromView(origin, parentWithView);
      view->SetPosition(origin.x, origin.y);
      NS_IF_RELEASE(parentWithView);
      NS_RELEASE(view);
    }
  }

  return NS_OK;
}

NS_METHOD
nsInputFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;

  // Let the view know the correct size
  nsIView* view = nsnull;
  GetView(view);
  if (nsnull != view) {
    // XXX combo boxes need revision, they cannot have their height altered
    view->SetDimensions(aWidth, aHeight);
    NS_RELEASE(view);
  }
  return NS_OK;
}

PRInt32 nsInputFrame::GetBorderSpacing(nsIPresContext& aPresContext)
{
  return (int)(2 * (aPresContext.GetPixelsToTwips() + .5));
}

// XXX it would be cool if form element used our rendering sw, then
// they could be blended, and bordered, and so on...
NS_METHOD
nsInputFrame::Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect)
{
  nsStyleDisplay* disp =
    (nsStyleDisplay*)mStyleContext->GetData(eStyleStruct_Display);

  if (disp->mVisible) {
    // Make sure the widget is visible if it isn't currently visible
    nsIView* view = nsnull;
    GetView(view);
    if (nsnull != view) {
      if (PR_FALSE == mDidInit) {
        ((nsInput*)mContent)->GetFormManager()->Init(PR_FALSE);
        PostCreateWidget(&aPresContext, view);
        mDidInit = PR_TRUE;
      }
//      SetViewVisiblity(&aPresContext, PR_TRUE);
      NS_RELEASE(view);
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
      if (newVisibility != view->GetVisibility()) {
        // this only inits the 1st time
        // XXX kipp says: this is yucky; init on first visibility seems lame
        ((nsInput*)mContent)->GetFormManager()->Init(PR_FALSE);
        view->SetVisibility(newVisibility);
        PostCreateWidget(aPresContext, view);
      }
      NS_IF_RELEASE(widget);
    }
    NS_RELEASE(view);
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
                             const nsSize& aMaxSize,
                             nsReflowMetrics& aDesiredLayoutSize,
                             nsSize& aDesiredWidgetSize)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(*aPresContext, styleSize);

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
                             nsReflowMetrics& aDesiredSize,
                             const nsSize& aMaxSize)
{
  nsSize ignore;
  GetDesiredSize(aPresContext, aMaxSize, aDesiredSize, ignore);
}

NS_METHOD
nsInputFrame::Reflow(nsIPresContext*      aPresContext,
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
      GetStyleContext(aPresContext, mStyleContext);
    }
    nsresult result = 
	    NSRepository::CreateInstance(kViewCID, nsnull, kIViewIID,
                                   (void **)&view);
	  if (NS_OK != result) {
	    NS_ASSERTION(0, "Could not create view for button"); 
      aStatus = NS_FRAME_NOT_COMPLETE;
      return result;
	  }
	  nsIPresShell   *presShell = aPresContext->GetShell();     // need to release
	  nsIViewManager *viewMan   = presShell->GetViewManager();  // need to release

    GetDesiredSize(aPresContext, aReflowState.maxSize, aDesiredSize, mWidgetSize);

    //nsRect boundBox(0, 0, mWidgetSize.width, mWidgetSize.height); 
    nsRect boundBox(0, 0, aDesiredSize.width, aDesiredSize.height); 

    nsIFrame* parWithView;
	  nsIView *parView;

    GetParentWithView(parWithView);
	  parWithView->GetView(parView);

	  const nsIID& id = GetCID();
    nsWidgetInitData* initData = GetWidgetInitData(*aPresContext); // needs to be deleted
	  // initialize the view as hidden since we don't know the (x,y) until Paint
    result = view->Init(viewMan, boundBox, parView, &id, initData,
                        nsnull, 0, nsnull,
                        1.0f, nsViewVisibility_kShow);
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
    NS_RELEASE(view);
	  
    NS_IF_RELEASE(parView);
	  NS_IF_RELEASE(viewMan);  
	  NS_IF_RELEASE(presShell); 
  }
  else {
    GetDesiredSize(aPresContext, aReflowState.maxSize, aDesiredSize, mWidgetSize);

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

nscoord nsInputFrame::GetDefaultPadding() const 
{
  return NS_POINTS_TO_TWIPS_INT(2); // XXX should be pixels, need pres context
} 

nscoord nsInputFrame::GetPadding() const
{
  return 0;
}

void 
nsInputFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
}

nsresult
nsInputFrame::GetWidget(nsIView* aView, nsIWidget** aWidget)
{
  const nsIID id = GetIID();
  if (NS_OK == aView->QueryInterface(id, (void**)aWidget)) {
    return NS_OK;
  } else {
	NS_ASSERTION(0, "The widget interface is invalid");  // need to print out more details of the widget
	return NS_NOINTERFACE;
  }
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
	// make sure that the widget in the event is this
  // XXX if there is no view, it could be an image button. Unfortunately,
  // every image button will get every event.
  nsIView* view;
  GetView(view);
  if (view) {
    NS_RELEASE(view);
    nsInput* content = (nsInput *)mContent;
    if (content->GetWidgetSupports() != aEvent->widgetSupports) {
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
	      /*nsIView* view = GetView();
	      nsIWidget *widget = view->GetWindow();
		    widget->SetFocus();
		    NS_RELEASE(widget);
	      NS_RELEASE(view); */
        float conv = aPresContext.GetTwipsToPixels();
        ((nsInput*)mContent)->SetClickPoint(NS_TO_INT_ROUND(conv * aEvent->point.x),
                                            NS_TO_INT_ROUND(conv * aEvent->point.y));   
 		    MouseClicked(&aPresContext);
		    //return PR_FALSE;
	    } 
	    mLastMouseState = eMouseEnter;
	    break;
    case NS_MOUSE_EXIT:
	    mLastMouseState = eMouseNone;
	    break;
  }
  aEventStatus = nsEventStatus_eConsumeDoDefault;
  return NS_OK;
}

void nsInputFrame::GetStyleSize(nsIPresContext& aPresContext,
                                nsSize& aSize)
{
  PRIntn ss = nsCSSLayout::GetStyleSize(&aPresContext, this, aSize);
  if (0 == (ss & NS_SIZE_HAS_WIDTH)) {
    aSize.width = CSS_NOTSET;
  }
  if (0 == (ss & NS_SIZE_HAS_HEIGHT)) {
    aSize.height = CSS_NOTSET;
  }
}

nscoord 
nsInputFrame::GetTextSize(nsIPresContext& aPresContext, nsIFrame* aFrame,
                          const nsString& aString, nsSize& aSize)
{
  //printf("\n GetTextSize %s", aString.ToNewCString());
  nsIStyleContext* styleContext;
  aFrame->GetStyleContext(&aPresContext, styleContext);
  nsStyleFont* styleFont = (nsStyleFont*)styleContext->GetData(eStyleStruct_Font);
  NS_RELEASE(styleContext);
  nsIDeviceContext* deviceContext = aPresContext.GetDeviceContext();
  nsIFontCache* fontCache = deviceContext->GetFontCache();

  nsIFontMetrics* fontMet = fontCache->GetMetricsFor(styleFont->mFont);
  aSize.width  = fontMet->GetWidth(aString);
  aSize.height = fontMet->GetHeight() + fontMet->GetLeading();

  aSize.height = (int)(((float)aSize.height) * .90); // XXX find out why this is necessary

  nscoord charWidth  = fontMet->GetWidth("W");

  NS_RELEASE(fontMet);
  NS_RELEASE(fontCache);
  NS_RELEASE(deviceContext);

  return charWidth;
}
  
nscoord
nsInputFrame::GetTextSize(nsIPresContext& aPresContext, nsIFrame* aFrame,
                          PRInt32 aNumChars, nsSize& aSize)
{
  nsAutoString val;
  for (int i = 0; i < aNumChars; i++) {
    val += 'e';  // use a typical char, what is the avg width character?
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

  aBounds.width  = CSS_NOTSET;
  aBounds.height = CSS_NOTSET;
  nsSize textSize(0,0);

  nsInput* content;
  aFrame->GetContent((nsIContent*&) content);

  nsAutoString valAttr;
  nsContentAttr valStatus = eContentAttr_NotThere;
  if (nsnull != aSpec.mColValueAttr) {
    valStatus = content->GetAttribute(aSpec.mColValueAttr, valAttr);
  }
  nsHTMLValue colAttr;
  nsContentAttr colStatus = eContentAttr_NotThere;
  if (nsnull != aSpec.mColSizeAttr) {
    colStatus = content->GetAttribute(aSpec.mColSizeAttr, colAttr);
  }

  if (eContentAttr_HasValue == colStatus) {  // col attr will provide width
    if (aSpec.mColSizeAttrInPixels) {
      float p2t = aPresContext->GetPixelsToTwips();
      aBounds.width = (int) (((float)colAttr.GetPixelValue()) * p2t);
    }
    else {
      PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
      charWidth = GetTextSize(*aPresContext, aFrame, col, aBounds);
      aRowHeight = aBounds.height;  // XXX aBounds.height has CSS_NOTSET
    }
	  if (aSpec.mColSizeAttrInPixels) {
	    aWidthExplicit = PR_TRUE;
	  }
  }
  else {
    if (CSS_NOTSET != aCSSSize.width) {  // css provides width
      aBounds.width = aCSSSize.width;
	    aWidthExplicit = PR_TRUE;
    } 
    else {                       
      if (eContentAttr_HasValue == valStatus) { // use width of initial value if specified
        charWidth = GetTextSize(*aPresContext, aFrame, valAttr, aBounds);
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

  nsHTMLValue rowAttr;
  nsContentAttr rowStatus = eContentAttr_NotThere;
  if (nsnull != aSpec.mRowSizeAttr) {
    rowStatus = content->GetAttribute(aSpec.mRowSizeAttr, rowAttr);
  }

  if (eContentAttr_HasValue == rowStatus) { // row attr will provide height
    PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
    if (0 == charWidth) {
      charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize);
      aBounds.height = textSize.height * rowAttrInt;
      aRowHeight = textSize.height;
      numRows = rowAttrInt;
    }
    else {
      aBounds.height = aBounds.height * rowAttrInt;
    }
  }
  else if (CSS_NOTSET != aCSSSize.height) {  // css provides height
    aBounds.height = aCSSSize.height;
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

  if (0 == charWidth) {
    charWidth = GetTextSize(*aPresContext, aFrame, 1, textSize);
    aRowHeight = textSize.height;
  }

  // add padding to width if width wasn't specified either from css or
  // size attr
  if (!aWidthExplicit) {
    aBounds.width += charWidth;
  }

  NS_RELEASE(content);
  if (ATTR_NOTSET == numRows) {
    numRows = aBounds.height / aRowHeight;
  }

  aBounds.height += (2 * aFrame->GetBorderSpacing(*aPresContext)); 

  return numRows;
}


nsFont& 
nsInputFrame::GetFont(nsIPresContext* aPresContext)
{
  nsStyleFont* styleFont = (nsStyleFont*)mStyleContext->GetData(eStyleStruct_Font);

  return styleFont->mFont;
}





