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
#include "nsComboboxControlFrame.h"
#include "nsFormFrame.h"
#include "nsButtonControlFrame.h"
#include "nsTextControlFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIFileWidget.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"

// Used for Paint
#include "nsCSSRendering.h"
#include "nsIDeviceContext.h"

#include "nsIPresShell.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"

#include "nsIDOMElement.h"
#include "nsListControlFrame.h"
#include "nsIListControlFrame.h"

#include "nsIDOMHTMLCollection.h" //rods
#include "nsIDOMHTMLSelectElement.h" //rods
#include "nsIDOMHTMLOptionElement.h" //rods

// XXX make this pixels
#define CONTROL_SPACING 40  

static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIComboboxControlFrameIID, NS_ICOMBOBOXCONTROLFRAME_IID);

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID,   NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID,   NS_IDOMHTMLOPTIONELEMENT_IID);
#ifdef PLUGGABLE_EVENTS
static NS_DEFINE_IID(kIPluggableEventListenerIID, NS_IPLUGGABLEEVENTLISTENER_IID);
#endif
static NS_DEFINE_IID(kIListControlFrameIID,       NS_ILISTCONTROLFRAME_IID);
static NS_DEFINE_IID(kITextContentIID,            NS_ITEXT_CONTENT_IID);

extern nsresult NS_NewListControlFrame(nsIFrame*& aNewFrame);

//--------------------------------------------------------------
nsresult
NS_NewComboboxControlFrame(nsIFrame*& aResult)
{
  aResult = new nsComboboxControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

//--------------------------------------------------------------
nsComboboxControlFrame::nsComboboxControlFrame()
  : nsHTMLContainerFrame()
{
  mFormFrame        = nsnull;
  mListFrame        = nsnull;
  mListControlFrame = nsnull;
  mPlaceHolderFrame = nsnull;

  mVisibleStyleContext = nsnull;
  mHiddenStyleContext  = nsnull;
  mCurrentStyleContext = nsnull;
  mBlockTextStyle      = nsnull;
  mBlockTextSelectedStyle      = nsnull;
  mBlockTextSelectedFocusStyle = nsnull;
  mFirstTime                   = PR_TRUE;
  mGotFocus                    = PR_FALSE;
}

//--------------------------------------------------------------
nsComboboxControlFrame::~nsComboboxControlFrame()
{

  NS_IF_RELEASE(mVisibleStyleContext);
  NS_IF_RELEASE(mHiddenStyleContext);
  NS_IF_RELEASE(mBlockTextStyle);
  //NS_IF_RELEASE(mListControlFrame);
  //NS_IF_RELEASE(mListFrame);
  //NS_IF_RELEASE(mPlaceHolderFrame);
  
}

//--------------------------------------------------------------
nsresult
nsComboboxControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIComboboxControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIComboboxControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
#ifdef PLUGGABLE_EVENTS
  if (aIID.Equals(kIPluggableEventListenerIID)) {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsIPluggableEventListener*)this);
    return NS_OK;
  }
#endif
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

//--------------------------------------------------------------
PRBool
nsComboboxControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

//--------------------------------------------------------------
void 
nsComboboxControlFrame::Reset()
{
  SetFocus(PR_TRUE, PR_TRUE);
}

//--------------------------------------------------------------
NS_IMETHODIMP 
nsComboboxControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_SELECT;
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;
  rv = GetContent(&content);
  aContent = content;
  return rv;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
   return 0;
}

//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

//--------------------------------------------------------------
// XXX this should be removed when nsView exposes it
nsIWidget*
nsComboboxControlFrame::GetWindowTemp(nsIView *aView)
{
  nsIWidget *window = nsnull;

  nsIView *ancestor = aView;
  while (nsnull != ancestor) {
    ancestor->GetWidget(window);
	  if (nsnull != window) {
	    return window;
	  }
	  ancestor->GetParent(ancestor);
  }
  return nsnull;
}

//--------------------------------------------------------------
void 
nsComboboxControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  //mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "SELECTED", PR_TRUE);
  mGotFocus = aOn;
  if (aRepaint) {
    nsFormControlHelper::ForceDrawFrame(this);
  }
}


//--------------------------------------------------------------
// this is in response to the MouseClick from the containing browse button
// XXX still need to get filters from accept attribute
void nsComboboxControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
 
  if (nsnull != mListControlFrame) {
    SetFocus(PR_FALSE, PR_TRUE);  
    mCurrentStyleContext = (mCurrentStyleContext == mHiddenStyleContext ? mVisibleStyleContext : mHiddenStyleContext);
    if (mCurrentStyleContext == mVisibleStyleContext) {
      mListControlFrame->AboutToDropDown();
      nsIFormControlFrame* fcFrame = nsnull;
      nsresult result = mListFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
      if ((NS_OK == result) && (nsnull != fcFrame)) {
        fcFrame->SetFocus(PR_TRUE, PR_FALSE);
      }
    } else {
      SetFocus(PR_TRUE, PR_TRUE);    
    }

    mListFrame->ReResolveStyleContext(aPresContext, mCurrentStyleContext, NS_STYLE_HINT_NONE, nsnull, nsnull);
    nsFormControlHelper::ForceDrawFrame(mListFrame);
  }

}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                            nsIAtom*        aListName,
                                            nsIFrame*       aChildList)
{
  nsFormFrame::AddFormControlFrame(aPresContext, *this);
  
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP nsComboboxControlFrame::Reflow(nsIPresContext&          aPresContext, 
                                             nsHTMLReflowMetrics&     aDesiredSize,
                                             const nsHTMLReflowState& aReflowState, 
                                             nsReflowStatus&          aStatus)
{
  if (mFirstTime) {
    ReResolveStyleContext(&aPresContext, mStyleContext, NS_STYLE_HINT_REFLOW, nsnull, nsnull); // XXX This temporary
    mListFrame->ReResolveStyleContext(&aPresContext, mCurrentStyleContext, NS_STYLE_HINT_REFLOW, nsnull, nsnull);
    mFirstTime = PR_FALSE;
  }

  PRInt32 numChildren = mFrames.GetLength();
  
  //nsIFrame* childFrame;
  if (1 == numChildren) {
    nsIAtom * textBlockContentPseudo = NS_NewAtom(":combobox-text");
    aPresContext.ResolvePseudoStyleContextFor(mContent, textBlockContentPseudo,
                                              mStyleContext, PR_FALSE,
                                              &mBlockTextStyle);
    NS_RELEASE(textBlockContentPseudo);

    // XXX This code should move to Init(), someday when the frame construction
    // changes are all done and Init() is always getting called...
    /*PRBool disabled = */nsFormFrame::GetDisabled(this);
  }

//  nsSize maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  {
    nsIDOMHTMLSelectElement* select = nsListControlFrame::GetSelect(mContent);
  if (!select) {
    return NS_OK;
  }
  nsIDOMHTMLCollection* options = nsListControlFrame::GetOptions(mContent, select);
  if (!options) {
    NS_RELEASE(select);
    return NS_OK;
  }

  // get the css size 
  nsSize styleSize;
  nsFormControlFrame::GetStyleSize(aPresContext, aReflowState, styleSize);

  // get the size of the longest option 
  PRInt32  maxWidth = 1;
  PRUint32 numOptions;
  options->GetLength(&numOptions);
  for (PRUint32 i = 0; i < numOptions; i++) {
    nsIDOMHTMLOptionElement* option = nsListControlFrame::GetOption(*options, i);
    if (option) {
      //option->CompressContent();
       nsAutoString text;
      if (NS_CONTENT_ATTR_HAS_VALUE != option->GetText(text)) {
        continue;
      }
      nsSize textSize;
      // use the style for the select rather that the option, since widgets don't support it
      nsFormControlHelper::GetTextSize(aPresContext, this, text, textSize, aReflowState.rendContext); 
      //nsFormControlHelper::GetTextSize(aPresContext, this, 1, textSize, aReflowState.rendContext); 
      if (textSize.width > maxWidth) {
        maxWidth = textSize.width;
      }
      NS_RELEASE(option);
    }
  }

  PRInt32 rowHeight = 0;
  nsSize desiredSize;
  nsSize minSize;
  PRBool widthExplicit, heightExplicit;
  nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull, nsnull,
                                maxWidth, PR_TRUE, nsHTMLAtoms::size, 1);
  // XXX fix CalculateSize to return PRUint32
  PRUint32 numRows = 
    (PRUint32)nsFormControlHelper::CalculateSize(&aPresContext, aReflowState.rendContext, this, 
                                                 styleSize, textSpec, desiredSize, minSize,
                                                 widthExplicit, heightExplicit, rowHeight);

  float sp2t;
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);

  aPresContext.GetScaledPixelsToTwips(&sp2t);
  nscoord onePixel = NSIntPixelsToTwips(1, sp2t);

#if 0
  nscoord scrollbarWidth  = 0;
  nscoord scrollbarHeight = 0;
  nsListControlFrame::GetScrollBarDimensions(aPresContext, scrollbarWidth, scrollbarHeight);
#endif

  nscoord extra = desiredSize.height - (rowHeight * numRows);

  numRows = (numOptions > 20 ? 20 : numOptions);
  desiredSize.height = (numRows * rowHeight) + extra;
  aDesiredSize.descent = 0;

  nsMargin  border;
  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  mySpacing->CalcBorderFor(this, border);

  float scale;
  float sbWidth;
  float sbHeight;
  nsCOMPtr<nsIDeviceContext> context;
  aPresContext.GetDeviceContext(getter_AddRefs(context));
  context->GetCanonicalPixelScale(scale);
  context->GetScrollBarDimensions(sbWidth, sbHeight);
  PRInt32 scrollbarScaledWidth  = PRInt32(sbWidth * scale);
//  PRInt32 scrollbarScaledHeight = PRInt32(sbWidth * scale);

  nsFont font(aPresContext.GetDefaultFixedFontDeprecated());
  SystemAttrStruct sis;
  sis.mFont = &font;
  context->GetSystemAttribute(eSystemAttr_Font_Tooltips, &sis);

  nscoord horKludgeAdjustment = NSIntPixelsToTwips(14, p2t);
  nscoord horAdjustment = scrollbarScaledWidth + border.left + border.right + horKludgeAdjustment;

  aDesiredSize.width  = desiredSize.width + horAdjustment;

  nscoord verKludgeAdjustment = onePixel;
  nscoord verAdjustment = border.top + border.bottom + verKludgeAdjustment;

  aDesiredSize.height = rowHeight + verAdjustment;

  mButtonRect.SetRect(aDesiredSize.width-scrollbarScaledWidth-border.right, border.top, 
                      scrollbarScaledWidth, aDesiredSize.height);

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = minSize.width  + verAdjustment;
	  aDesiredSize.maxElementSize->height = minSize.height + horAdjustment;
  }


  nsRect frameRect;
  GetRect(frameRect);
  nsRect curRect;
  mPlaceHolderFrame->GetRect(curRect);
  curRect.x = frameRect.x;
  curRect.y = frameRect.y + aDesiredSize.height;
  mPlaceHolderFrame->SetRect(curRect);

  mListFrame->GetRect(frameRect); 

  }
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////

  aStatus = NS_FRAME_COMPLETE;

  return NS_OK;
}

//------------------------------------------------------------------------------
void
nsComboboxControlFrame::PaintComboboxControl(nsIPresContext&      aPresContext,
                                             nsIRenderingContext& aRenderingContext,
                                             const nsRect&        aDirtyRect,
                                             nsFramePaintLayer    aWhichLayer)
{
    const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
    if (!disp->mVisible) {
      return;
    }
    aRenderingContext.PushState();


    const nsStyleColor*   myColor   = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    const nsStyleFont*    myFont    = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);


    nsIStyleContext * blkStyle;
    if (mGotFocus) {
      blkStyle = mTextStr.Length() > 0?mBlockTextSelectedFocusStyle:mBlockTextStyle;
    } else {
      blkStyle = mTextStr.Length() > 0?mBlockTextSelectedStyle:mBlockTextStyle;
    }
    const nsStyleColor*   blkColor   = (const nsStyleColor*)blkStyle->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* blkSpacing = (const nsStyleSpacing*)blkStyle->GetStyleData(eStyleStruct_Spacing);
//    const nsStyleFont*    blkFont    = (const nsStyleFont*)blkStyle->GetStyleData(eStyleStruct_Font);

    nsRect rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *myColor, *mySpacing, 0, 0);

    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *mySpacing, mStyleContext, 0);

    nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

    const nsStyleSpacing* spacing =(const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin border;
    spacing->CalcBorderFor(this, border);

    float p2t;
    aPresContext.GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    nsRect outside(0, 0, mRect.width, mRect.height);
    outside.Deflate(border);

    nsRect inside(outside);
    outside.Deflate(onePixel, onePixel);

    aRenderingContext.SetColor(blkColor->mBackgroundColor);
    aRenderingContext.FillRect(inside.x, inside.y, inside.width, inside.height);

    float appUnits;
    float devUnits;
    float scale;
    nsIDeviceContext * context;
    aRenderingContext.GetDeviceContext(context);

    context->GetCanonicalPixelScale(scale);
    context->GetAppUnitsToDevUnits(devUnits);
    context->GetDevUnitsToAppUnits(appUnits);

    float sbWidth;
    float sbHeight;
    context->GetScrollBarDimensions(sbWidth, sbHeight);
    PRInt32 scrollbarScaledWidth  = PRInt32(sbWidth * scale);
    PRInt32 scrollbarScaledHeight = PRInt32(sbWidth * scale);

    nsFont font(aPresContext.GetDefaultFixedFontDeprecated()); 
    GetFont(&aPresContext, font);

    aRenderingContext.SetFont(myFont->mFont);

    inside.width  -= scrollbarScaledWidth;
    PRBool clipEmpty;
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(inside, nsClipCombine_kReplace, clipEmpty);

    nscoord x = inside.x + (onePixel * 4);
    nscoord y = inside.y;

    aRenderingContext.SetColor(blkColor->mColor);

    aRenderingContext.DrawString(mTextStr, x, y);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, inside, *blkSpacing, blkStyle, 0);

    aRenderingContext.PopState(clipEmpty);

////////////////////////////////

      inside.width  -= scrollbarScaledWidth;
      inside.height -= scrollbarScaledHeight;

      // Scrollbars
      //const nsStyleColor* myColor = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
      //nsIAtom * sbAtom = NS_NewAtom(":scrollbar-arrow-look");
      //nsIStyleContext* arrowStyle = aPresContext.ResolvePseudoStyleContextFor(mContent, sbAtom, mStyleContext);
      //NS_RELEASE(sbAtom);

      const nsStyleSpacing* arrowSpacing = (const nsStyleSpacing*)mArrowStyle->GetStyleData(eStyleStruct_Spacing);
//      const nsStyleColor*   arrowColor   = (const nsStyleColor*)mArrowStyle->GetStyleData(eStyleStruct_Color);

      nsRect srect(0,0,0,0);//mRect.width-scrollbarWidth-onePixel, onePixel, scrollbarWidth, mRect.height-(onePixel*2));
      srect = mButtonRect;
      nsFormControlHelper::PaintArrow(nsFormControlHelper::eArrowDirection_Down, aRenderingContext,aPresContext, 
                      aDirtyRect, srect, onePixel, mArrowStyle, *arrowSpacing, this, mRect);
    //}


    NS_RELEASE(context);

    PRBool status;
    aRenderingContext.PopState(status);
}

NS_METHOD 
nsComboboxControlFrame::Paint(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PaintComboboxControl(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  }
  return NS_OK;
}

//--------------------------------------------------------------
PRIntn
nsComboboxControlFrame::GetSkipSides() const
{
  return 0;
}


//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetName(nsString* aResult)
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

//--------------------------------------------------------------
PRInt32 
nsComboboxControlFrame::GetMaxNumValues()
{
  return 1;
}
  
//--------------------------------------------------------------
PRBool
nsComboboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                   nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  // use our name and the text widgets value 
  aNames[0] = name;
  nsresult status = PR_FALSE;
  /*nsIWidget*  widget;
  nsITextWidget* textWidget;
  mTextFrame->GetWidget(&widget);
  if (widget && (NS_OK == widget->QueryInterface(kITextWidgetIID, (void**)&textWidget))) {
    PRUint32 actualSize;
    textWidget->GetText(aValues[0], 0, actualSize);
    aNumValues = 1;
    NS_RELEASE(textWidget);
    status = PR_TRUE;
  }
  NS_IF_RELEASE(widget);*/
  return status;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ComboboxControl", aResult);
}


void
nsComboboxControlFrame::RefreshStyleContext(nsIPresContext* aPresContext,
                                            nsIAtom *         aNewContentPseudo,
                                            nsIStyleContext*& aCurrentStyle,
                                            nsIContent *      aContent,
                                            nsIStyleContext*  aParentStyle) 

{
  nsIStyleContext* newStyleContext;
  aPresContext->ProbePseudoStyleContextFor(aContent,
                                           aNewContentPseudo,
                                           aParentStyle,
                                           PR_FALSE,
                                           &newStyleContext);
  if (newStyleContext != aCurrentStyle) {
    NS_IF_RELEASE(aCurrentStyle);
    aCurrentStyle = newStyleContext;
  } else {
    NS_IF_RELEASE(newStyleContext);
  }
}

NS_IMETHODIMP
nsComboboxControlFrame::ReResolveStyleContext(nsIPresContext* aPresContext,
                                              nsIStyleContext* aParentContext,
                                              PRInt32 aParentChange,
                                              nsStyleChangeList* aChangeList,
                                              PRInt32* aLocalChange)
{
  // NOTE: using nsFrame's ReResolveStyleContext method to avoid
  // useless version in base classes.
  PRInt32 ourChange = aParentChange;
  nsresult rv = nsHTMLContainerFrame::ReResolveStyleContext(aPresContext, aParentContext, 
                                                            ourChange, aChangeList, &ourChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aLocalChange) {
    *aLocalChange = ourChange;
  }

  if (NS_COMFALSE != rv) {
    PRInt32 childChange;
    PRBool currentIsVisible = (mCurrentStyleContext == mVisibleStyleContext?PR_TRUE:PR_FALSE);

    RefreshStyleContext(aPresContext, nsHTMLAtoms::dropDownVisible, mVisibleStyleContext, mContent, mStyleContext);

    RefreshStyleContext(aPresContext, nsHTMLAtoms::dropDownHidden, mHiddenStyleContext, mContent, mStyleContext);
    mCurrentStyleContext = (currentIsVisible?mVisibleStyleContext:mHiddenStyleContext);

    mListFrame->ReResolveStyleContext(aPresContext, 
                                      (nsnull != mCurrentStyleContext? mCurrentStyleContext : mStyleContext),
                                      ourChange, aChangeList, &childChange);

    // Button Style
    RefreshStyleContext(aPresContext, nsHTMLAtoms::dropDownBtnOut, mBtnOutStyleContext, mContent, mStyleContext);
    RefreshStyleContext(aPresContext, nsHTMLAtoms::dropDownBtnPressed, mBtnPressedStyleContext, mContent, mStyleContext);

    nsIAtom * txtBlkContentPseudo = NS_NewAtom(":combobox-text");
    RefreshStyleContext(aPresContext, txtBlkContentPseudo, mBlockTextStyle, mContent, mStyleContext);
    NS_IF_RELEASE(txtBlkContentPseudo);

    nsIAtom * txtBlkSelContentPseudo = NS_NewAtom(":combobox-textselected");
    RefreshStyleContext(aPresContext, txtBlkSelContentPseudo, mBlockTextSelectedStyle, mContent, mStyleContext);
    NS_IF_RELEASE(txtBlkSelContentPseudo);

    nsIAtom * txtBlkSelFocContentPseudo = NS_NewAtom(":combobox-textselectedfocus");
    RefreshStyleContext(aPresContext, txtBlkSelFocContentPseudo, mBlockTextSelectedFocusStyle, mContent, mStyleContext);
    NS_IF_RELEASE(txtBlkSelFocContentPseudo);


  }

  return rv;
}
//----------------------------------------------------------------------
NS_IMETHODIMP nsComboboxControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                                   nsGUIEvent*     aEvent,
                                                   nsEventStatus&  aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  if(nsEventStatus_eConsumeNoDefault != aEventStatus) {
    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    } else if (aEvent->message == NS_MOUSE_MOVE && mDoingSelection ||
               aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      // no-op
    } else {
      return NS_OK;
    }

    aEventStatus = nsEventStatus_eConsumeNoDefault;

    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      mArrowStyle = mBtnOutStyleContext;
      nsFormControlHelper::ForceDrawFrame(this);
      //MouseClicked(&aPresContext);

    } else if (aEvent->message == NS_MOUSE_MOVE) {

    } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
      mArrowStyle = mBtnPressedStyleContext;
      nsFormControlHelper::ForceDrawFrame(this);
      MouseClicked(&aPresContext);
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  //nsresult rv = nsScrollFrame::GetFrameForPoint(aPoint, aFrame);
  nsresult rv = GetFrameForPointUsing(aPoint, nsnull, aFrame);
  if (NS_OK == rv) {
    if (*aFrame != this) {
      //mHitFrame = *aFrame;
      *aFrame = this;
    }
    return NS_OK;
  }
  *aFrame = this;
  return NS_OK;
}

nsresult
nsComboboxControlFrame::GetFrameForPointUsing(const nsPoint& aPoint,
                                        nsIAtom*       aList,
                                        nsIFrame**     aFrame)
{
  *aFrame = this;
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIPluggableEventListener
//----------------------------------------------------------------------
//----------------------------------------------------------------------
NS_IMETHODIMP nsComboboxControlFrame::PluggableEventHandler(nsIPresContext& aPresContext, 
                                                 nsGUIEvent*     aEvent,
                                                 nsEventStatus&  aEventStatus)
{
  HandleEvent(aPresContext, aEvent, aEventStatus);
  //aEventStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsComboboxControlFrame::PluggableGetFrameForPoint(const nsPoint& aPoint, 
                                                     nsIFrame**     aFrame)
{
  return GetFrameForPoint(aPoint, aFrame);
}



//----------------------------------------------------------------------
// nsIComboboxControlFrame
//----------------------------------------------------------------------

//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::SetDropDown(nsIFrame* aPlaceHolderFrame, nsIFrame* aDropDownFrame)
{
  mPlaceHolderFrame = aPlaceHolderFrame;
  mListFrame        = aDropDownFrame;

  if (NS_OK != mListFrame->QueryInterface(kIListControlFrameIID, (void**)&mListControlFrame)) {
    return NS_ERROR_FAILURE;
  }

  // Ok, since we now know we have the ListFrame, and we are assuming at this point it has been initialized
  // Let's get the currently selected item, but we make the call using the Interface
  mListControlFrame->GetSelectedItem(mTextStr);
  
  AppendChildren(mPlaceHolderFrame, PR_FALSE);
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::SetDropDownStyleContexts(nsIStyleContext * aVisible, nsIStyleContext * aHidden)
{
  mVisibleStyleContext = aVisible;
  mHiddenStyleContext  = aHidden;
  mCurrentStyleContext = mHiddenStyleContext;

  NS_ADDREF(mVisibleStyleContext);
  NS_ADDREF(mHiddenStyleContext);
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::SetButtonStyleContexts(nsIStyleContext * aOut, nsIStyleContext * aPressed)
{
  mBtnOutStyleContext     = aOut;
  mBtnPressedStyleContext = aPressed;
  mArrowStyle             = aOut;

  NS_ADDREF(mBtnOutStyleContext);
  NS_ADDREF(mBtnPressedStyleContext);
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::ListWasSelected(nsIPresContext* aPresContext)
{
  mArrowStyle = mBtnOutStyleContext;
  MouseClicked(aPresContext);

  nsString str;
  if (nsnull != mListControlFrame) {
    mListControlFrame->GetSelectedItem(mTextStr);
    nsIFormControlFrame* fcFrame = nsnull;
    nsresult result = mListFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
    if ((NS_OK == result) && (nsnull != fcFrame)) {
      fcFrame->SetFocus(PR_FALSE, PR_FALSE);
    }
    SetFocus(PR_TRUE, PR_TRUE);    
  }

  
  return NS_OK;
}

NS_IMETHODIMP nsComboboxControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsComboboxControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}

nsresult nsComboboxControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


