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
#include "nsTextControlFrame.h"
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

#ifdef SingleSignon
#include "nsIDocument.h"
#include "prmem.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#endif

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

nsresult NS_NewTextControlFrame(nsIFrame*& aResult);
nsresult
NS_NewTextControlFrame(nsIFrame*& aResult)
{
  aResult = new nsTextControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nscoord 
nsTextControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(4, aPixToTwip);
}

nscoord 
nsTextControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

// for a text area aInnerHeight is the height of one line
nscoord 
nsTextControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                             nscoord aInnerHeight) const
{

  // XXX NOTE: the enums eMetric_TextShouldUseVerticalInsidePadding and eMetric_TextVerticalInsidePadding
  // are ONLY needed because GTK is not using the "float" padding values and wants to only use an 
  // integer value for the padding instead of calculating like the other platforms.
  //
  // If GTK decides to start calculating the value, PLEASE remove these two enum from nsILookAndFeel and
  // all the platforms nsLookAndFeel impementations so we don't have these extra values remaining in the code.
  // The two enums are:
  //    eMetric_TextVerticalInsidePadding
  //    eMetric_TextShouldUseVerticalInsidePadding
  //
  float   padTextArea;
  float   padTextField;
  PRInt32 vertPad;
  PRInt32 shouldUseVertPad;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_TextAreaVerticalInsidePadding,  padTextArea);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_TextFieldVerticalInsidePadding,  padTextField);
   // These two (below) are really only needed for GTK
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextVerticalInsidePadding,  vertPad);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextShouldUseVerticalInsidePadding,  shouldUseVertPad);
   NS_RELEASE(lookAndFeel);
  }

  if (1 == shouldUseVertPad) {
    return NSIntPixelsToTwips(vertPad, aPixToTwip); // XXX this is probably wrong (for GTK)
  } else {
    PRInt32 type;
    GetType(&type);
    if (NS_FORM_TEXTAREA == type) {
      return (nscoord)NSToIntRound(float(aInnerHeight) * padTextArea);
    } else {
      return (nscoord)NSToIntRound(float(aInnerHeight) * padTextField);
    }
  }
}

nscoord 
nsTextControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  // XXX NOTE: the enum eMetric_TextShouldUseHorizontalInsideMinimumPadding
  // is ONLY needed because GTK is not using the "float" padding values and wants to only use the 
  // "minimum" integer value for the padding instead of calculating and comparing like the other platforms.
  //
  // If GTK decides to start calculating and comparing the values, 
  // PLEASE remove these the enum from nsILookAndFeel and
  // all the platforms nsLookAndFeel impementations so we don't have these extra values remaining in the code.
  // The enum is:
  //    eMetric_TextShouldUseHorizontalInsideMinimumPadding
  //
  float   padTextField;
  float   padTextArea;
  PRInt32 padMinText;
  PRInt32 shouldUsePadMinText;

  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_TextFieldHorizontalInsidePadding,  padTextField);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_TextAreaHorizontalInsidePadding,  padTextArea);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextHorizontalInsideMinimumPadding,  padMinText);
   // This one (below) is really only needed for GTK
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_TextShouldUseHorizontalInsideMinimumPadding,  shouldUsePadMinText);
   NS_RELEASE(lookAndFeel);
  }

  nscoord padding;
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_TEXTAREA == type) {
    padding = (nscoord)(aCharWidth * padTextArea);
  } else {
    padding = (nscoord)(aCharWidth * padTextField);
  }
  nscoord min = NSIntPixelsToTwips(padMinText, aPixToTwip);
  if (padding > min && (1 == shouldUsePadMinText)) {
    return padding;
  } else {
    return min;
  }

}


const nsIID&
nsTextControlFrame::GetIID()
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_TEXTAREA == type) {
    return kITextAreaWidgetIID;
  } else {
    return kITextWidgetIID;
  }
}
  
const nsIID&
nsTextControlFrame::GetCID()
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_TEXTAREA == type) {
    return kTextAreaCID;
  } else {
    return kTextCID;
  }
}

void
nsTextControlFrame::EnterPressed(nsIPresContext& aPresContext) 
{
  if (mFormFrame && mFormFrame->CanSubmit(*this)) {
    nsIContent *formContent = nsnull;

    mFormFrame->GetContent(&formContent);
    if (nsnull != formContent) {
      nsEvent event;
      nsEventStatus status = nsEventStatus_eIgnore;

      event.eventStructType = NS_EVENT;
      event.message = NS_FORM_SUBMIT;
      formContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
      NS_RELEASE(formContent);
    }

    mFormFrame->OnSubmit(&aPresContext, this);
  }
}

#define DEFAULT_PIXEL_WIDTH 20
void 
nsTextControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsHTMLReflowMetrics& aDesiredLayoutSize,
                                   nsSize& aDesiredWidgetSize)
{
  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(&mode);

  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(*aPresContext, aReflowState, styleSize);

  nsSize desiredSize;
  nsSize minSize;
  
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    PRInt32 width;
    if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
      width = DEFAULT_PIXEL_WIDTH;
    }
    //if (eCompatibility_NavQuirks == mode) {
    //  width += 1;
    //}
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, PR_FALSE, nsnull, 1);
    nsFormControlHelper::CalculateSize(aPresContext, aReflowState.rendContext, this, styleSize, 
                                       textSpec, desiredSize, minSize, widthExplicit, heightExplicit, ignore);
  } else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, nsnull, DEFAULT_PIXEL_WIDTH, 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    nsFormControlHelper::CalculateSize(aPresContext, aReflowState.rendContext, this, styleSize, 
                                       areaSpec, desiredSize, minSize, widthExplicit, heightExplicit, ignore);
  }

  if (NS_FORM_TEXTAREA == type) {
    nscoord scrollbarWidth  = 0;
    nscoord scrollbarHeight = 0;
    float   scale;
    float   p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    nsCOMPtr<nsIDeviceContext> dx;
    aPresContext->GetDeviceContext(getter_AddRefs(dx));
    if (dx) { 
      float sbWidth;
      float sbHeight;
      dx->GetCanonicalPixelScale(scale);
      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      scrollbarWidth  = PRInt32(sbWidth * scale);
      scrollbarHeight = PRInt32(sbHeight * scale);
    } else {
      scrollbarWidth  = GetScrollbarWidth(p2t);
      scrollbarHeight = scrollbarWidth;
    }

    if (!heightExplicit) {
      desiredSize.height += scrollbarHeight;
      minSize.height     += scrollbarHeight;
    } 
    if (!widthExplicit) {
      desiredSize.width += scrollbarWidth;
      minSize.width     += scrollbarWidth;
    }
  }

  aDesiredLayoutSize.width  = desiredSize.width;
  aDesiredLayoutSize.height = desiredSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;

  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->width  = minSize.width;
    aDesiredLayoutSize.maxElementSize->height = minSize.height;
  }

  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;

}

nsWidgetInitData*
nsTextControlFrame::GetWidgetInitData(nsIPresContext& aPresContext)
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
nsTextControlFrame::GetText(nsString* aText, PRBool aInitialValue)
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
   result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
/* XXX REMOVE 
   nsIDOMHTMLInputElement* textElem = nsnull;
    result = mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&textElem);
    if ((NS_OK == result) && textElem) {
      if (PR_TRUE == aInitialValue) {
        result = textElem->GetDefaultValue(*aText);
      }
      else {
        result = textElem->GetValue(*aText);
      }

      NS_RELEASE(textElem);
    }
*/

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
nsTextControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
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
      else if (nsHTMLAtoms::size == aAttribute) {
        nsFormFrame::StyleChangeReflow(aPresContext, this);
      }
      // Allow the base class to handle common attributes supported
      // by all form elements... 
      else {
        result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
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
          result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
        }
        NS_RELEASE(textArea);
      }
      else { // We didn't get a Text or TextArea.  Uh oh...
        result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
      }
    }
  }

  return result;
}

void 
nsTextControlFrame::PostCreateWidget(nsIPresContext* aPresContext,
                                     nscoord& aWidth, 
                                     nscoord& aHeight)
{
  if (!mWidget) {
    return;
  }

  PRInt32 type;
  GetType(&type);

  nsFont font(aPresContext->GetDefaultFixedFontDeprecated()); 
  GetFont(aPresContext, font);
  mWidget->SetFont(font);
  SetColors(*aPresContext);

  PRUint32 ignore;
  nsAutoString value;

  nsITextAreaWidget* textArea = nsnull;
  nsITextWidget* text = nsnull;
  if (NS_OK == mWidget->QueryInterface(kITextWidgetIID,(void**)&text)) {

#ifdef SingleSignon
    /* get name of text */
    PRBool failed = PR_TRUE;
    nsAutoString name;
    GetName(&name);

    /* get url name */
    char *URLName = nsnull;
    nsIURL* docURL = nsnull;
    nsIDocument* doc = nsnull;
    mContent->GetDocument(doc);
    if (nsnull != doc) {
      docURL = doc->GetDocumentURL();
      NS_RELEASE(doc);
      if (nsnull != docURL) {
        const char* spec;
        (void)docURL->GetSpec(&spec);
        if (nsnull != spec) {
          URLName = (char*)PR_Malloc(PL_strlen(spec)+1);
          PL_strcpy(URLName, spec);
        }
        NS_RELEASE(docURL);
      }
    }

    if (nsnull != URLName) {
      /* invoke single-signon to get previously-used value of text */
      nsINetService *service;
      nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                            kINetServiceIID,
                                            (nsISupports **)&service);
      if ((NS_OK == res) && (nsnull != service)) {
        char* valueString = NULL;
        char* nameString = name.ToNewCString();
        res = service->SI_RestoreSignonData(URLName, nameString, &valueString);
        delete[] nameString;
        NS_RELEASE(service);
        PR_FREEIF(URLName);
        if (valueString && *valueString) {
          value = valueString;
          failed = PR_FALSE;
        }
      }
    }
    if (failed) {
      GetText(&value, PR_TRUE);
    }
#else
  GetText(&value, PR_TRUE);
#endif

    text->SetText(value, ignore);
    PRInt32 maxLength;
    nsresult result = GetMaxLength(&maxLength);
    if (NS_CONTENT_ATTR_NOT_THERE != result) {
      text->SetMaxTextLength(maxLength);
    }
    NS_RELEASE(text);
  } else if (NS_OK == mWidget->QueryInterface(kITextAreaWidgetIID,(void**)&textArea)) {
    GetText(&value, PR_TRUE);
    textArea->SetText(value, ignore);
    NS_RELEASE(textArea);
  }
  if (nsFormFrame::GetDisabled(this)) {
    mWidget->Enable(PR_FALSE);
  }
}

PRInt32 
nsTextControlFrame::GetMaxNumValues()
{
  return 1;
}
  
PRBool
nsTextControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
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
nsTextControlFrame::Reset() 
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

NS_IMETHODIMP
nsTextControlFrame::GetCursor(nsIPresContext& aPresContext, nsPoint& aPoint, PRInt32& aCursor)
{
  /*const nsStyleColor* styleColor;
  GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)styleColor);
  aCursor = styleColor->mCursor;*/

  //XXX This is wrong, should be through style.
  aCursor = NS_STYLE_CURSOR_TEXT;
  return NS_OK;
}


NS_IMETHODIMP
nsTextControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TextControl", aResult);
}


void
nsTextControlFrame::PaintTextControlBackground(nsIPresContext& aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect,
                                     nsFramePaintLayer aWhichLayer) {
  nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

void
nsTextControlFrame::PaintTextControl(nsIPresContext& aPresContext,
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
    aPresContext.GetScaledPixelsToTwips(&p2t);
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

    nsFont font(aPresContext.GetDefaultFixedFontDeprecated()); 
    GetFont(&aPresContext, font);

    aRenderingContext.SetFont(font);

    nscoord textWidth;
    nscoord textHeight;
 
    aRenderingContext.GetWidth(aText, textWidth);

    nsIFontMetrics* metrics;
    context->GetMetricsFor(font, metrics);
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
      PRInt32 pos    = aText.Find('\n', 0);
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
        pos = aText.Find('\n', pos);
      }

      aRenderingContext.PopState(clipEmpty);

      // Scrollbars
      nsIAtom * sbAtom = NS_NewAtom(":scrollbar-look");
      nsIStyleContext* scrollbarStyle;
      aPresContext.ResolvePseudoStyleContextFor(mContent, sbAtom, aStyleContext, PR_FALSE, &scrollbarStyle);
      NS_RELEASE(sbAtom);

      sbAtom = NS_NewAtom(":scrollbar-arrow-look");
      nsIStyleContext* arrowStyle;
      aPresContext.ResolvePseudoStyleContextFor(mContent, sbAtom, aStyleContext, PR_FALSE, &arrowStyle);
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
nsTextControlFrame::Paint(nsIPresContext& aPresContext,
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

void nsTextControlFrame::GetTextControlFrameState(nsString& aValue)
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

void nsTextControlFrame::SetTextControlFrameState(const nsString& aValue)
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
  }    
}

NS_IMETHODIMP nsTextControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::value == aName) {
    SetTextControlFrameState(aValue);
  }
  else {
    return nsFormControlFrame::SetProperty(aName, aValue);
  }
  return NS_OK;
}      

NS_IMETHODIMP nsTextControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    GetTextControlFrameState(aValue);
  }
  else {
    return nsFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;
}  
