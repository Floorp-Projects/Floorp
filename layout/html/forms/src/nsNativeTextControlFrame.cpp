/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsNativeTextControlFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsString.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsFont.h"
#include "nsDOMEvent.h"
#include "nsIFormControl.h"
#include "nsFormFrame.h"
#include "nsIContent.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"

#include "nsCSSRendering.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

nsresult
NS_NewNativeTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsNativeTextControlFrame* it = new (aPresShell) nsNativeTextControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsNativeTextControlFrame::nsNativeTextControlFrame()
: mCachedState(nsnull)
{
}

nsNativeTextControlFrame::~nsNativeTextControlFrame()
{
  if (mCachedState) {
    delete mCachedState;
    mCachedState = nsnull;
  }
}

void
nsNativeTextControlFrame::EnterPressed(nsIPresContext* aPresContext) 
{
  if (mFormFrame && mFormFrame->CanSubmit(this)) {
    nsIContent *formContent = nsnull;

    mFormFrame->GetContent(&formContent);
    if (nsnull != formContent) {
      nsEvent event;
      nsEventStatus status = nsEventStatus_eIgnore;

      event.eventStructType = NS_EVENT;
      event.message = NS_FORM_SUBMIT;
      formContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
      NS_RELEASE(formContent);
    }

    mFormFrame->OnSubmit(aPresContext, this);
  }
}

nsWidgetInitData*
nsNativeTextControlFrame::GetWidgetInitData(nsIPresContext* aPresContext)
{
  PRInt32 type;
  GetType(&type);

  nsTextWidgetInitData* data = nsnull;

  PRBool readOnly = nsFormFrame::GetReadonly(this);

  if ((NS_FORM_INPUT_PASSWORD == type) || readOnly) {
    data = new nsTextWidgetInitData();
    data->mIsPassword = PR_FALSE;
    data->mIsReadOnly = PR_FALSE;
    if (NS_FORM_INPUT_PASSWORD == type) {
      data->clipChildren = PR_TRUE;
      data->mIsPassword = PR_TRUE;
    } 
    if (readOnly) {
      data->mIsReadOnly = PR_TRUE;
    }
  }

  return data;
}

NS_IMETHODIMP
nsNativeTextControlFrame::GetText(nsString* aText, PRBool aInitialValue)
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
   result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
  } else {
    nsIDOMHTMLTextAreaElement* textArea = nsnull;
    result = mContent->QueryInterface(kIDOMHTMLTextAreaElementIID, (void**)&textArea);
    if ((NS_OK == result) && textArea) {
      if (PR_TRUE == aInitialValue) {
        result = textArea->GetDefaultValue(*aText);
      }
      else {
        result = textArea->GetValue(*aText);
      }
      NS_RELEASE(textArea);
    }
  }
  return result;
}

NS_IMETHODIMP
nsNativeTextControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                           nsIContent*     aChild,
                                           PRInt32         aNameSpaceID,
                                           nsIAtom*        aAttribute,
                                           PRInt32         aHint)
{
  nsresult result = NS_OK;
  if (mWidget) {
    nsITextWidget* text = nsnull;
    result = mWidget->QueryInterface(kITextWidgetIID, (void**)&text);
    if ((NS_OK == result) && (nsnull != text)) {
      if (nsHTMLAtoms::value == aAttribute) {
        nsString value;
        /*XXXnsresult rv = */GetText(&value, PR_TRUE);
        PRUint32 ignore;
        text->SetText(value, ignore);
        if (aHint != NS_STYLE_HINT_REFLOW) 
          nsFormFrame::StyleChangeReflow(aPresContext, this);
      } else if (nsHTMLAtoms::maxlength == aAttribute) {
        PRInt32 maxLength;
        nsresult rv = GetMaxLength(&maxLength);
        if (NS_CONTENT_ATTR_NOT_THERE != rv) {
          text->SetMaxTextLength(maxLength);
        }
      } else if (nsHTMLAtoms::readonly == aAttribute) {
        PRBool oldReadOnly;
        text->SetReadOnly(nsFormFrame::GetReadonly(this),oldReadOnly);
      }
      else if (nsHTMLAtoms::size == aAttribute &&
               aHint != NS_STYLE_HINT_REFLOW) {
        nsFormFrame::StyleChangeReflow(aPresContext, this);
      }
      // Allow the base class to handle common attributes supported
      // by all form elements... 
      else {
        result = nsNativeFormControlFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
      }
      NS_RELEASE(text);
    }
    // XXX Ick, create an common interface that has the functionality of nsTextHelper
    else { // We didn't get a Text, is it a TextArea?
      nsITextAreaWidget* textArea = nsnull;
      result = mWidget->QueryInterface(kITextAreaWidgetIID, (void**)&textArea);
      if ((NS_OK == result) && (nsnull != textArea)) {
        if (nsHTMLAtoms::value == aAttribute) {
          nsString value;
          /*XXXnsresult rv = */GetText(&value, PR_TRUE);
          PRUint32 ignore;
          textArea->SetText(value, ignore);
          nsFormFrame::StyleChangeReflow(aPresContext, this);
        } else if (nsHTMLAtoms::maxlength == aAttribute) {
          PRInt32 maxLength;
          nsresult rv = GetMaxLength(&maxLength);
          if (NS_CONTENT_ATTR_NOT_THERE != rv) {
            textArea->SetMaxTextLength(maxLength);
          }
        } else if (nsHTMLAtoms::readonly == aAttribute) {
          PRBool oldReadOnly;
          textArea->SetReadOnly(nsFormFrame::GetReadonly(this),oldReadOnly);
        }
        else if (nsHTMLAtoms::size == aAttribute) {
          nsFormFrame::StyleChangeReflow(aPresContext, this);
        }
        // Allow the base class to handle common attributes supported
        // by all form elements... 
        else {
          result = nsNativeFormControlFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
        }
        NS_RELEASE(textArea);
      }
      else { // We didn't get a Text or TextArea.  Uh oh...
        result = nsNativeFormControlFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
      }
    }
  }

  return result;
}

void 
nsNativeTextControlFrame::PostCreateWidget(nsIPresContext* aPresContext,
                                           nscoord& aWidth, 
                                           nscoord& aHeight)
{
  if (!mWidget) {
    return;
  }

  PRInt32 type;
  GetType(&type);

  const nsFont * font = nsnull;
  if (NS_SUCCEEDED(GetFont(aPresContext, font))) {
    mWidget->SetFont(*font);
  }
  SetColors(aPresContext);

  PRUint32 ignore;
  nsAutoString value;

  nsITextAreaWidget* textArea = nsnull;
  nsITextWidget* text = nsnull;
  if (NS_OK == mWidget->QueryInterface(kITextWidgetIID,(void**)&text)) {
    if (mCachedState) {
      value = *mCachedState;
      delete mCachedState;
      mCachedState = nsnull;
    } else
      GetText(&value, PR_TRUE);
    text->SetText(value, ignore);
    PRInt32 maxLength;
    nsresult result = GetMaxLength(&maxLength);
    if (NS_CONTENT_ATTR_NOT_THERE != result) {
      text->SetMaxTextLength(maxLength);
    }
    NS_RELEASE(text);
  } else if (NS_OK == mWidget->QueryInterface(kITextAreaWidgetIID,(void**)&textArea)) {
    if (mCachedState) {
      value = *mCachedState;
      delete mCachedState;
      mCachedState = nsnull;
    } else
      GetText(&value, PR_TRUE);
    textArea->SetText(value, ignore);
    NS_RELEASE(textArea);
  }
  if (nsFormFrame::GetDisabled(this)) {
    mWidget->Enable(PR_FALSE);
  }
}
  
PRBool
nsNativeTextControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                         nsString* aValues, nsString* aNames)
{
  if (!mWidget) {
    return PR_FALSE;
  }

  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_NOT_THERE == result)) {
    return PR_FALSE;
  }

  PRUint32 size;
  nsITextWidget* text = nsnull;
  nsITextAreaWidget* textArea = nsnull;

  aNames[0] = name;  
  aNumValues = 1;

  if (NS_OK == mWidget->QueryInterface(kITextWidgetIID,(void**)&text)) {
    text->GetText(aValues[0],0,size);  // the last parm is not used
    NS_RELEASE(text);
    return PR_TRUE;
  } else if (NS_OK == mWidget->QueryInterface(kITextAreaWidgetIID,(void**)&textArea)) {
    textArea->GetText(aValues[0],0,size);  // the last parm is not used
    NS_RELEASE(textArea);
    return PR_TRUE;
  }
  return PR_FALSE;
}


void 
nsNativeTextControlFrame::Reset(nsIPresContext* aPresContext) 
{
  if (!mWidget) {
    return;
  }

  nsITextWidget* text = nsnull;
  nsITextAreaWidget* textArea = nsnull;

  nsAutoString value;
  nsresult valStatus = GetText(&value, PR_TRUE);

  PRUint32 size;
  if (NS_OK == mWidget->QueryInterface(kITextWidgetIID,(void**)&text)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == valStatus) {
      text->SetText(value,size);
    } else {
      text->SetText("",size);
    }
    NS_RELEASE(text);
  } else if (NS_OK == mWidget->QueryInterface(kITextAreaWidgetIID,(void**)&textArea)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == valStatus) {
      textArea->SetText(value,size);
    } else {
      textArea->SetText("",size);
    }
    NS_RELEASE(textArea);
  }

}  

void
nsNativeTextControlFrame::PaintTextControlBackground(nsIPresContext* aPresContext,
                                                     nsIRenderingContext& aRenderingContext,
                                                     const nsRect& aDirtyRect,
                                                     nsFramePaintLayer aWhichLayer) {
  nsNativeFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

void
nsNativeTextControlFrame::PaintTextControl(nsIPresContext* aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect,
                                     nsString& aText,
                                     nsIStyleContext* aStyleContext, nsRect& aRect)
{
    aRenderingContext.PushState();

    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin border;
    spacing->CalcBorderFor(this, border);

    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    nsRect outside(aRect.x, aRect.y, aRect.width, aRect.height);
    outside.Deflate(border);
    outside.Deflate(onePixel, onePixel);

    nsRect inside(outside);
    inside.Deflate(onePixel, onePixel);

#if 0
    if (mGotFocus) { 
      PaintFocus(aRenderingContext,
                 aDirtyRect, inside, outside);
    }
#endif

    float appUnits;
    float devUnits;
    float scale;
    nsIDeviceContext * context;
    aRenderingContext.GetDeviceContext(context);

    context->GetCanonicalPixelScale(scale);
    context->GetAppUnitsToDevUnits(devUnits);
    context->GetDevUnitsToAppUnits(appUnits);

    aRenderingContext.SetColor(NS_RGB(0,0,0));

    const nsFont * font = nsnull;
    nsresult res = GetFont(aPresContext, font);
    if (NS_SUCCEEDED(res) && font != nsnull) {
      mWidget->SetFont(*font);
      aRenderingContext.SetFont(*font);
    }


    nscoord textWidth;
    nscoord textHeight;
 
    aRenderingContext.GetWidth(aText, textWidth);

    nsIFontMetrics* metrics;
    if (font != nsnull) {
      context->GetMetricsFor(*font, metrics);
    }
    metrics->GetHeight(textHeight);

    PRInt32 type;
    GetType(&type);
    if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type) {
      nscoord x = inside.x + onePixel + onePixel;
      nscoord y;
    
      if (NS_FORM_INPUT_TEXT == type) {
        y = ((inside.height  - textHeight) / 2)  + inside.y;
      } else {
        metrics->GetMaxAscent(textHeight);
        y = ((inside.height  - textHeight) / 2)  + inside.y;
        PRInt32 i;
        PRInt32 len = aText.Length();
        aText.SetLength(0);
        for (i=0;i<len;i++) {
          aText.Append("*");
        }
      }
      aRenderingContext.DrawString(aText, x, y); 
    } else {
      float sbWidth;
      float sbHeight;
      context->GetCanonicalPixelScale(scale);
      context->GetScrollBarDimensions(sbWidth, sbHeight);
      PRInt32 scrollbarScaledWidth  = PRInt32(sbWidth * scale);
      PRInt32 scrollbarScaledHeight = PRInt32(sbWidth * scale);

      inside.width  -= scrollbarScaledWidth;
      inside.height -= scrollbarScaledHeight;
      PRBool clipEmpty;
      aRenderingContext.PushState();
      aRenderingContext.SetClipRect(inside, nsClipCombine_kReplace, clipEmpty);

      nscoord x = inside.x + onePixel;
      nscoord y = inside.y + onePixel;

      // Draw multi-line text
      PRInt32 oldPos = 0;
      PRInt32 pos    = aText.FindChar('\n', PR_FALSE,0);
      while (1) {
        nsString substr;
        if (-1 == pos) {
             // Single line, no carriage return.
          aText.Right(substr, aText.Length()-oldPos);
          aRenderingContext.DrawString(substr, x, y); 
          break;     
        } 
          // Strip off substr up to carriage return
        aText.Mid(substr, oldPos, ((pos - oldPos) - 1));
  
        aRenderingContext.DrawString(substr, x, y);
        y += textHeight;
          // Advance to the next carriage return
        pos++;
        oldPos = pos;
        pos = aText.FindChar('\n', PR_FALSE,pos);
      }

      aRenderingContext.PopState(clipEmpty);

      // Scrollbars
      nsIAtom * sbAtom = NS_NewAtom(":scrollbar-look");
      nsIStyleContext* scrollbarStyle;
      aPresContext->ResolvePseudoStyleContextFor(mContent, sbAtom, aStyleContext, PR_FALSE, &scrollbarStyle);
      NS_RELEASE(sbAtom);

      sbAtom = NS_NewAtom(":scrollbar-arrow-look");
      nsIStyleContext* arrowStyle;
      aPresContext->ResolvePseudoStyleContextFor(mContent, sbAtom, aStyleContext, PR_FALSE, &arrowStyle);
      NS_RELEASE(sbAtom);

      nsRect srect(aRect.width-scrollbarScaledWidth-(2*onePixel), 2*onePixel, scrollbarScaledWidth, aRect.height-(onePixel*4)-scrollbarScaledWidth);

      nsFormControlHelper::PaintScrollbar(aRenderingContext,aPresContext, aDirtyRect, srect, PR_FALSE, onePixel, 
																          scrollbarStyle, arrowStyle, this, aRect);   
      // Horizontal
      srect.SetRect(2*onePixel, aRect.height-scrollbarScaledHeight-(2*onePixel), aRect.width-(onePixel*4)-scrollbarScaledHeight, scrollbarScaledHeight);
      nsFormControlHelper::PaintScrollbar(aRenderingContext,aPresContext, aDirtyRect, srect, PR_TRUE, onePixel, 
																        scrollbarStyle, arrowStyle, this, aRect);   

      // Draw the small rect "gap" in the bottom right that the two scrollbars don't cover
      const nsStyleColor*   sbColor   = (const nsStyleColor*)scrollbarStyle->GetStyleData(eStyleStruct_Color);
      srect.SetRect(aRect.width-scrollbarScaledWidth-(2*onePixel), aRect.height-scrollbarScaledHeight-(onePixel*2), scrollbarScaledWidth, scrollbarScaledHeight);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, srect, *sbColor, *spacing, 0, 0);
    }


    NS_RELEASE(context);

    PRBool status;
    aRenderingContext.PopState(status);
}

NS_METHOD 
nsNativeTextControlFrame::Paint(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
  PaintTextControlBackground(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    nsString text;
    GetText(&text, PR_FALSE);
    nsRect rect(0, 0, mRect.width, mRect.height);
    PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, text, mStyleContext, rect);
  }
  return NS_OK;
}

void nsNativeTextControlFrame::GetTextControlFrameState(nsString& aValue)
{
  if (nsnull != mWidget) {
    nsITextWidget* text = nsnull;
    nsITextAreaWidget* textArea = nsnull;   
    PRUint32 size = 0;
    if (NS_OK == mWidget->QueryInterface(kITextWidgetIID,(void**)&text)) {
      text->GetText(aValue,0,size);
      NS_RELEASE(text);
    }
    else if (NS_OK == mWidget->QueryInterface(kITextAreaWidgetIID,
                                                (void**)&textArea)) {
      textArea->GetText(aValue,0, size);
      NS_RELEASE(textArea);
    }
  }
  else {
   //XXX: this should return the a local field for GFX-rendered widgets             aValue = "";
  }
}     

void nsNativeTextControlFrame::SetTextControlFrameState(const nsString& aValue)
{
  if (nsnull != mWidget) {
    nsITextWidget* text = nsnull;
    nsITextAreaWidget* textArea = nsnull;   
    PRUint32 size = 0;
    if (NS_SUCCEEDED(mWidget->QueryInterface(kITextWidgetIID,(void**)&text))) {
      text->SetText(aValue,size);
      NS_RELEASE(text);
    } else if (NS_OK == mWidget->QueryInterface(kITextAreaWidgetIID,
                                                (void**)&textArea)) {
      textArea->SetText(aValue,size);
      NS_RELEASE(textArea);
    }
  } else {
    if (mCachedState) delete mCachedState;
    mCachedState = new nsString(aValue);
    // XXX if (!mCachedState) rv = NS_ERROR_OUT_OF_MEMORY;
    NS_ASSERTION(mCachedState, "Error: new nsString failed!");
  }
}

NS_IMETHODIMP nsNativeTextControlFrame::SetProperty(nsIPresContext* aPresContext,
                                                    nsIAtom* aName,
                                                    const nsAReadableString& aValue)
{
  nsresult rv = NS_OK;
  if (nsHTMLAtoms::value == aName) {
    SetTextControlFrameState(aValue);
  } else if (nsHTMLAtoms::select == aName) {
    if (nsnull != mWidget) {
      nsITextWidget *textWidget;
      rv = mWidget->QueryInterface(kITextWidgetIID, (void**)&textWidget);
      if (NS_SUCCEEDED(rv)) {
        textWidget->SelectAll();
        NS_RELEASE(textWidget);
      } 

      nsITextAreaWidget *textAreaWidget;
      rv = mWidget->QueryInterface(kITextAreaWidgetIID, (void**)&textAreaWidget);
      if (NS_SUCCEEDED(rv)) {
        textAreaWidget->SelectAll();
        NS_RELEASE(textAreaWidget);
      }
    }
  }
  else {
    return nsNativeFormControlFrame::SetProperty(aPresContext, aName, aValue);
  }
  return rv;
}      

NS_IMETHODIMP nsNativeTextControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    GetTextControlFrameState(aValue);
  }
  else {
    return nsNativeFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;
}  

nsresult nsNativeTextControlFrame::RequiresWidget(PRBool &aRequiresWidget)
{
  aRequiresWidget = PR_TRUE;
  return NS_OK;
}

