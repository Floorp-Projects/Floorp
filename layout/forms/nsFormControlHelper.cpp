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
#include "nsFormControlHelper.h"
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

// Needed for Localization
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsIStringBundle.h"
#include "nsITextContent.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kIOServiceCID,            NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
// done I10N


static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

// For figuring out the "WRAP" property
#define kTextControl_Wrap_Soft "SOFT"
#define kTextControl_Wrap_Virtual "VIRTUAL"   // "virtual" is a synonym for "soft"
#define kTextControl_Wrap_Hard "HARD"
#define kTextControl_Wrap_Physical "PHYSICAL" // "physical" is a synonym for "hard"
#define kTextControl_Wrap_Off  "OFF"



MOZ_DECL_CTOR_COUNTER(nsFormControlHelper);

nsFormControlHelper::nsFormControlHelper()
{
  MOZ_COUNT_CTOR(nsFormControlHelper);
}

nsFormControlHelper::~nsFormControlHelper()
{
  MOZ_COUNT_DTOR(nsFormControlHelper);
}

void nsFormControlHelper::ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame)
{

  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(aPresContext, pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view != nsnull) {
    nsIViewManager * viewMgr;
    view->GetViewManager(viewMgr);
    if (viewMgr != nsnull) {
      viewMgr->UpdateView(view, rect, NS_VMREFRESH_NO_SYNC);
      NS_RELEASE(viewMgr);
    }
  }
  
}

void nsFormControlHelper::PlatformToDOMLineBreaks(nsString &aString)
{
  // Windows linebreaks: Map CRLF to LF:
  aString.ReplaceSubstring(NS_ConvertASCIItoUCS2("\r\n"), NS_ConvertASCIItoUCS2("\n"));

  // Mac linebreaks: Map any remaining CR to LF:
  aString.ReplaceSubstring(NS_ConvertASCIItoUCS2("\r"), NS_ConvertASCIItoUCS2("\n"));
}

PRBool nsFormControlHelper::GetBool(const nsAReadableString& aValue)
{
  return aValue.Equals(NS_STRING_TRUE);
}

void nsFormControlHelper::GetBoolString(const PRBool aValue,
                                        nsAWritableString& aResult)
{
  aResult.Assign( aValue ? NS_STRING_TRUE :
                  NS_STRING_FALSE );
}


nsCompatibility
nsFormControlHelper::GetRepChars(nsIPresContext* aPresContext, char& char1, char& char2) 
{
  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(&mode);
  if (eCompatibility_Standard == mode) {
    char1 = 'W';
    char2 = 'w';
    return eCompatibility_Standard;
  } else {
    char1 = 'W';
    char2 = 'w';
    return eCompatibility_NavQuirks;
  }
}

nsresult nsFormControlHelper::GetFrameFontFM(nsIPresContext* aPresContext, 
                                         nsIFormControlFrame * aFrame,
                                         nsIFontMetrics** aFontMet)
{
  // Initialize with default font
  const nsFont * font = nsnull;
  // Get frame font
  if (NS_SUCCEEDED(aFrame->GetFont(aPresContext, font))) {
    nsCOMPtr<nsIDeviceContext> deviceContext;
    aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));
    NS_ASSERTION(deviceContext, "Couldn't get the device context"); 
    if (font != nsnull) { // Get font metrics
      return deviceContext->GetMetricsFor(*font, *aFontMet);
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsFormControlHelper::GetWrapProperty(nsIContent * aContent, nsString &aOutValue)
{
  aOutValue.SetLength(0);
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  aContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::wrap, value);
    if (eHTMLUnit_String == value.GetUnit()) { 
      value.GetStringValue(aOutValue);
    }
    NS_RELEASE(content);
  }
  return result;
}


nsresult 
nsFormControlHelper::GetWrapPropertyEnum(nsIContent * aContent, nsHTMLTextWrap& aWrapProp)
{
  nsString wrap;
  aWrapProp = eHTMLTextWrap_Off; // the default
  
  nsresult result = GetWrapProperty(aContent, wrap);

  if (NS_CONTENT_ATTR_NOT_THERE != result) {

    nsAutoString wrapHard; wrapHard.AssignWithConversion(kTextControl_Wrap_Hard);
    nsAutoString wrapPhysical; wrapPhysical.AssignWithConversion(kTextControl_Wrap_Physical);
    if (wrap.EqualsIgnoreCase(wrapHard) ||
        wrap.EqualsIgnoreCase(wrapPhysical)) {
      aWrapProp = eHTMLTextWrap_Hard;
      return result;
    }

    nsAutoString wrapSoft; wrapSoft.AssignWithConversion(kTextControl_Wrap_Soft);
    nsAutoString wrapVirtual; wrapVirtual.AssignWithConversion(kTextControl_Wrap_Virtual);
    if (wrap.EqualsIgnoreCase(wrapSoft) ||
        wrap.EqualsIgnoreCase(wrapVirtual)) {
      aWrapProp = eHTMLTextWrap_Soft;
      return result;
    }
  }
  return result;
}

nscoord 
nsFormControlHelper::CalcNavQuirkSizing(nsIPresContext*      aPresContext, 
                                        nsIRenderingContext* aRendContext,
                                        nsIFontMetrics*      aFontMet, 
                                        nsIFormControlFrame* aFrame,
                                        nsInputDimensionSpec& aSpec,
                                        nsSize&              aSize)
{
  float p2t;
  float t2p;
  aPresContext->GetPixelsToTwips(&p2t);
  aPresContext->GetTwipsToPixels(&t2p);

  nscoord ascent;
  nscoord descent;
  nscoord maxCharWidth;
  aFontMet->GetMaxAscent(ascent);
  aFontMet->GetMaxDescent(descent);
  aFontMet->GetMaxAdvance(maxCharWidth);

  ascent       = NSToCoordRound(ascent * t2p);
  descent      = NSToCoordRound(descent * t2p);
  maxCharWidth = NSToCoordRound(maxCharWidth * t2p);

  char char1, char2;
  GetRepChars(aPresContext, char1, char2);

  nscoord char1Width, char2Width;
  aRendContext->GetWidth(char1, char1Width);
  aRendContext->GetWidth(char2, char2Width);
  char1Width = NSToCoordRound(char1Width * t2p);
  char2Width = NSToCoordRound(char2Width * t2p);

  // Nav Quirk Calculation for TextField
  PRInt32 type;
  aFrame->GetType(&type);
  nscoord width;
  nscoord hgt;
  nscoord height;
  nscoord average = 0;
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    average = (char1Width + char2Width) / 2;
    width   = maxCharWidth;
    hgt     = ascent + descent;
    height  = hgt + (hgt / 2);
    width  += aSpec.mColDefaultSize * average;
  } else if (NS_FORM_TEXTAREA == type) {
    nscoord lines = 1;
    nscoord scrollbarWidth  = 0;
    nscoord scrollbarHeight = 0;
    float   scale;
    nsCOMPtr<nsIDeviceContext> dx;
    aPresContext->GetDeviceContext(getter_AddRefs(dx));
    if (dx) { 
      float sbWidth;
      float sbHeight;
      dx->GetCanonicalPixelScale(scale);
      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      scrollbarWidth  = PRInt32(sbWidth * scale);
      scrollbarHeight = PRInt32(sbHeight * scale);
      scrollbarWidth  = NSToCoordRound(scrollbarWidth * t2p);
      scrollbarHeight  = NSToCoordRound(scrollbarHeight * t2p);
    } else {
      NS_ASSERTION(0, "Couldn't get the device context"); 
      scrollbarWidth  = 16;
      scrollbarHeight = 16;
    }
    nsIContent * content;
    aFrame->GetFormContent(content);
    nsCOMPtr<nsIHTMLContent> hContent(do_QueryInterface(content));

    // determine the height
    nsHTMLValue rowAttr;
    nsresult rowStatus = NS_CONTENT_ATTR_NOT_THERE;
    if (nsnull != aSpec.mRowSizeAttr) {
      rowStatus = hContent->GetHTMLAttribute(aSpec.mRowSizeAttr, rowAttr);
    }
    if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
      PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) 
                              ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
      lines = (rowAttrInt > 0) ? rowAttrInt : 1;
    } else {
      lines = aSpec.mRowDefaultSize;
    }

    average = (char1Width + char2Width) / 2;
    width   = ((aSpec.mColDefaultSize + 1) * average) + scrollbarWidth;
    hgt     = ascent + descent;
    height  = (lines + 1) * hgt;

    // then if not word wrapping
    nsHTMLTextWrap wrapProp;
    nsFormControlHelper::GetWrapPropertyEnum(content, wrapProp);
    if (wrapProp == eHTMLTextWrap_Off) {
      height += scrollbarHeight;
    }
    NS_RELEASE(content);
  } else if (NS_FORM_INPUT_BUTTON == type ||
             NS_FORM_INPUT_SUBMIT == type ||
             NS_FORM_INPUT_RESET  == type) {
    GetTextSize(aPresContext, aFrame, *aSpec.mColDefaultValue, aSize, aRendContext);
    aSize.width  = NSToCoordRound(aSize.width * t2p);
    aSize.height = NSToCoordRound(aSize.height * t2p);
    width  = 3 * aSize.width / 2;
    height = 3 * aSize.height / 2;
  } else if (NS_FORM_INPUT_HIDDEN == type) {
    width  = 0;
    height = 0;
  } else {
    width  = 0;
    height = 0;
  }

#ifdef DEBUG_rodsXXXX
  printf("********* Nav Quirks: %d,%d  max:%d average:%d ascent:%d descent:%d\n", 
          width, height, maxCharWidth, average, ascent, descent);
#endif

  aSize.width  = NSIntPixelsToTwips(width, p2t);
  aSize.height = NSIntPixelsToTwips(height, p2t);
  average      = NSIntPixelsToTwips(average, p2t);

  return average;

}

nscoord 
nsFormControlHelper::GetTextSize(nsIPresContext* aPresContext, nsIFormControlFrame* aFrame,
                                const nsString& aString, nsSize& aSize,
                                nsIRenderingContext *aRendContext)
{
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult res = GetFrameFontFM(aPresContext, aFrame, getter_AddRefs(fontMet));
  if (NS_SUCCEEDED(res) && fontMet) {
    aRendContext->SetFont(fontMet);

    // measure string
    aRendContext->GetWidth(aString, aSize.width);
    fontMet->GetHeight(aSize.height);
  } else {
    NS_ASSERTION(PR_FALSE, "Couldn't get Font Metrics"); 
    aSize.width = 0;
  }

  char char1, char2;
  nsCompatibility mode = GetRepChars(aPresContext, char1, char2);
  nscoord char1Width, char2Width;
  aRendContext->GetWidth(char1, char1Width);
  aRendContext->GetWidth(char2, char2Width);

  if (eCompatibility_Standard == mode) {
    return ((char1Width + char2Width) / 2) + 1;
  } else {
    return (char1Width + char2Width) / 2;
  }
}  
  
nscoord
nsFormControlHelper::GetTextSize(nsIPresContext* aPresContext, nsIFormControlFrame* aFrame,
                                PRInt32 aNumChars, nsSize& aSize,
                                nsIRenderingContext *aRendContext)
{
  nsAutoString val;
  char char1, char2;
  GetRepChars(aPresContext, char1, char2);
  int i;
  for (i = 0; i < aNumChars; i+=2) {
    val.AppendWithConversion(char1);  
  }
  for (i = 1; i < aNumChars; i+=2) {
    val.AppendWithConversion(char2);  
  }
  return GetTextSize(aPresContext, aFrame, val, aSize, aRendContext);
}
  
PRInt32
nsFormControlHelper::CalculateSize (nsIPresContext*       aPresContext, 
                                    nsIRenderingContext*  aRendContext,
                                    nsIFormControlFrame*  aFrame,
                                    const nsSize&         aCSSSize, 
                                    nsInputDimensionSpec& aSpec, 
                                    nsSize&               aDesiredSize, 
                                    nsSize&               aMinSize, 
                                    PRBool&               aWidthExplicit, 
                                    PRBool&               aHeightExplicit, 
                                    nscoord&              aRowHeight) 
{
  nscoord charWidth   = 0; 
  PRInt32 numRows     = ATTR_NOTSET;
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  nsIContent* iContent = nsnull;
  aFrame->GetFormContent((nsIContent*&) iContent);
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
  aPresContext->GetScaledPixelsToTwips(&p2t);

#if 0
  // determine if it is percentage based width, height
  PRBool percentageWidth  = PR_FALSE;
  PRBool percentageHeight = PR_FALSE;

  const nsStylePosition* pos;
  nsIFrame* iFrame = nsnull;
  nsresult rv = aFrame->QueryInterface(kIFrameIID, (void**)&iFrame);
  if ((NS_OK == rv) && (nsnull != iFrame)) { 
    iFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)pos);
    if (eStyleUnit_Percent == pos->mWidth.GetUnit()) {
      percentageWidth = PR_TRUE;
    }
    if (eStyleUnit_Percent == pos->mWidth.GetUnit()) {
      percentageHeight = PR_TRUE;
    }
  }
#endif

  nsCompatibility qMode;
  aPresContext->GetCompatibilityMode(&qMode);

  // determine the width, char height, row height
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
    if (aSpec.mColSizeAttrInPixels) {
      // need to set charWidth and aDesiredSize.height
      charWidth = GetTextSize(aPresContext, aFrame, 1, aDesiredSize, aRendContext);
      col = (col <= 0) ? 15 : col; // XXX why a default of 15 pixels, why hide it
                                   // XXX this conflicts with a default of 20 found in nsGfxTextControlFrame.
      aDesiredSize.width = NSIntPixelsToTwips(col, p2t);
    } else {
      col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
      if (eCompatibility_NavQuirks == qMode) {
        nsCOMPtr<nsIFontMetrics> fontMet;
        nsresult res = GetFrameFontFM(aPresContext, aFrame, getter_AddRefs(fontMet));
        if (NS_SUCCEEDED(res) && fontMet) {
          aRendContext->SetFont(fontMet);
          aSpec.mColDefaultSize = col;
          charWidth = CalcNavQuirkSizing(aPresContext, aRendContext, fontMet, 
                                         aFrame, aSpec, aDesiredSize);
        } else {
          charWidth = GetTextSize(aPresContext, aFrame, col, aDesiredSize, aRendContext);
        }
      }
    }
    if (eCompatibility_Standard == qMode) {
      if (aSpec.mColSizeAttrInPixels) {
        aWidthExplicit = PR_TRUE;
      }
    }
    aMinSize.width = aDesiredSize.width;
  } else {
    // set aDesiredSize for height calculation below. CSS may override width
    if (NS_CONTENT_ATTR_HAS_VALUE == valStatus) { // use width of initial value 
      charWidth = GetTextSize(aPresContext, aFrame, valAttr, aDesiredSize, aRendContext);
    } else if (aSpec.mColDefaultValue) {          // use default value
      charWidth = GetTextSize(aPresContext, aFrame, *aSpec.mColDefaultValue, aDesiredSize, aRendContext);
    } else if (aSpec.mColDefaultSizeInPixels) {   // use default width in pixels
      charWidth = GetTextSize(aPresContext, aFrame, 1, aDesiredSize, aRendContext);
      aDesiredSize.width = aSpec.mColDefaultSize;
    } else  {                                     // use default width in num characters
      if (eCompatibility_NavQuirks == qMode) {
        nsCOMPtr<nsIFontMetrics> fontMet;
        nsresult res = GetFrameFontFM(aPresContext, aFrame, getter_AddRefs(fontMet));
        if (NS_SUCCEEDED(res) && fontMet) {
          aRendContext->SetFont(fontMet);
          // this passes in a 
          charWidth = CalcNavQuirkSizing(aPresContext, aRendContext, fontMet, 
                                         aFrame, aSpec, aDesiredSize);
        } else {
          NS_ASSERTION(fontMet, "Couldn't get Font Metrics"); 
          aDesiredSize.width = 300;  // arbitrary values
          aDesiredSize.width = 1500;
        }
      } else {
        charWidth = GetTextSize(aPresContext, aFrame, aSpec.mColDefaultSize, aDesiredSize, aRendContext); 
      }
    }
    aMinSize.width = aDesiredSize.width;
    if (CSS_NOTSET != aCSSSize.width) {  // css provides width
      NS_ASSERTION(aCSSSize.width >= 0, "form control's computed width is < 0"); 
      if (NS_INTRINSICSIZE != aCSSSize.width) {
        aDesiredSize.width = PR_MAX(aDesiredSize.width,aCSSSize.width);
        aWidthExplicit = PR_TRUE;
      }
    }
  }
  aRowHeight      = aDesiredSize.height;
  aMinSize.height = aDesiredSize.height;

  // determine the height
  if (eCompatibility_Standard == qMode) {
    nsHTMLValue rowAttr;
    nsresult rowStatus = NS_CONTENT_ATTR_NOT_THERE;
    if (nsnull != aSpec.mRowSizeAttr) {
      rowStatus = hContent->GetHTMLAttribute(aSpec.mRowSizeAttr, rowAttr);
    }
    if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
      PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) 
                              ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
      numRows = (rowAttrInt > 0) ? rowAttrInt : 1;
      aDesiredSize.height = aDesiredSize.height * numRows;
    } else {
      aDesiredSize.height = aDesiredSize.height * aSpec.mRowDefaultSize;
      if (CSS_NOTSET != aCSSSize.height) {  // css provides height
        NS_ASSERTION(aCSSSize.height > 0, "form control's computed height is <= 0"); 
        if (NS_INTRINSICSIZE != aCSSSize.height) {
          aDesiredSize.height = PR_MAX(aDesiredSize.height,aCSSSize.height);
          aHeightExplicit = PR_TRUE;
        }
      }
    }
  }

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);

  // only add in padding if we are not Gfx, or if we're a Gfx text widget
  PRBool requiresWidget = PR_FALSE;
  aFrame->RequiresWidget(requiresWidget);
  PRInt32 type;
  aFrame->GetType(&type);

  if (PR_TRUE == requiresWidget || eWidgetRendering_Native == mode ||
      type==NS_FORM_INPUT_TEXT || type==NS_FORM_TEXTAREA || type==NS_FORM_INPUT_PASSWORD) 
  {
    if (!aWidthExplicit && mode == eWidgetRendering_Native) {
	  //if (!aWidthExplicit) {
      PRInt32 hPadding = (2 * aFrame->GetHorizontalInsidePadding(aPresContext, p2t, aDesiredSize.width, charWidth));
      aDesiredSize.width += hPadding;
      aMinSize.width += hPadding;
    }
    if (!aHeightExplicit && mode == eWidgetRendering_Native) {
	  //if (!aHeightExplicit) { 
      PRInt32 vPadding = (2 * aFrame->GetVerticalInsidePadding(aPresContext, p2t, aRowHeight));
      aDesiredSize.height += vPadding;
      aMinSize.height += vPadding;
    }
  }

  NS_RELEASE(hContent);
  if (ATTR_NOTSET == numRows) {
    numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;
  }

  NS_RELEASE(iContent);

  return numRows;
}


// this handles all of the input types rather than having them do it.
nsresult  
nsFormControlHelper::GetFont(nsIFormControlFrame * aFormFrame,
                             nsIPresContext*       aPresContext, 
                             nsIStyleContext *     aStyleContext, 
                             const nsFont*&        aFont)
{
  const nsStyleFont* styleFont = (const nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font);

  nsWidgetRendering m;
  aPresContext->GetWidgetRenderingMode(&m);

  // only add in padding if we are not Gfx, excluding the text widgets
  PRBool requiresWidget = PR_FALSE;
  aFormFrame->RequiresWidget(requiresWidget);

  PRInt32 type;
  aFormFrame->GetType(&type);

  if (type!=NS_FORM_INPUT_TEXT &&
      type!=NS_FORM_TEXTAREA   &&
      type!=NS_FORM_INPUT_PASSWORD) {
    if (PR_TRUE != requiresWidget && eWidgetRendering_Gfx == m) {
      aFont = &styleFont->mFont;
      return NS_OK;
    }
  }

  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(&mode);

  if (eCompatibility_Standard == mode) {
    aFont = &styleFont->mFont;
    return NS_OK;
  }

  switch (type) {
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_TEXTAREA:
    case NS_FORM_INPUT_PASSWORD:
      aFont = &styleFont->mFont;
      break;
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_SELECT:
      NS_ASSERTION(0, "getting the font here has been predicated");
#if 0
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
        const nsFont& normal = aPresContext->GetDefaultFontDeprecated();
        PRInt32 scaler;
        aPresContext->GetFontScaler(&scaler);
        float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
        PRInt32 fontIndex = nsStyleUtil::FindNextSmallerFontSize(aFont.size, (PRInt32)normal.size, scaleFactor, aPresContext);
        aFont.size = nsStyleUtil::CalcFontPointSize(fontIndex, (PRInt32)normal.size, scaleFactor, aPresContext);
      }
#endif
      break;
  }
  return NS_OK;
}


//
//-------------------------------------------------------------------------------------
// Utility methods for rendering Form Elements using GFX
//-------------------------------------------------------------------------------------
//

void 
nsFormControlHelper::PaintLine(nsIRenderingContext& aRenderingContext, 
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
static const nscoord nsArrowOffsetX = 3;
static const nscoord nsArrowOffsetY = 3;
static nscoord nsArrowRightPoints[]  = {0, 0, 0, 6, 6, 3 };
static nscoord nsArrowLeftPoints[]   = {0, 3, 6, 0, 6, 6 };
static nscoord nsArrowUpPoints[]     = {3, 0, 6, 6, 0, 6 }; 
static nscoord nsArrowDownPoints[]   = {0, 0, 3, 6, 6, 0 };

//---------------------------------------------------------------------------
void 
nsFormControlHelper::SetupPoints(PRUint32 aNumberOfPoints, nscoord* aPoints, nsPoint* aPolygon, nscoord aScaleFactor, nscoord aX, nscoord aY,
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
nsFormControlHelper::PaintArrowGlyph(nsIRenderingContext& aRenderingContext, 
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
nsFormControlHelper::PaintArrow(nsArrowDirection aArrowDirection,
					                    nsIRenderingContext& aRenderingContext,
															nsIPresContext* aPresContext, 
															const nsRect& aDirtyRect,
                                                            nsRect& aRect, 
															nscoord aOnePixel, 
															nsIStyleContext* aArrowStyle,
															const nsStyleSpacing& aSpacing,
															nsIFrame* aForFrame,
                                                            nsRect& aFrameRect)
{
  // Draw border using CSS
  const nsStyleColor* color = (const nsStyleColor*)
      aArrowStyle->GetStyleData(eStyleStruct_Color);
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, aRect,  *color, aSpacing, 0, 0);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                               aDirtyRect, aRect, aSpacing, aArrowStyle, 0);

  // Draw the glyph in black
  aRenderingContext.SetColor(NS_RGB(0, 0, 0));
   // Draw arrow centered in the rectangle
  PaintArrowGlyph(aRenderingContext, aRect.x + (aRect.width / 2), aRect.y + (aRect.height / 2), aArrowDirection, aOnePixel);

}

//---------------------------------------------------------------------------
void 
nsFormControlHelper::PaintScrollbar(nsIRenderingContext& aRenderingContext,
																	nsIPresContext* aPresContext, 
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
  const nsStyleColor* scrollbarColor =
    (const nsStyleColor*)aScrollbarStyleContext->GetStyleData(eStyleStruct_Color);

  // Get the Scrollbar's Arrow's Style structs
  const nsStyleSpacing* arrowSpacing =
    (const nsStyleSpacing*)aScrollbarArrowStyleContext->GetStyleData(eStyleStruct_Spacing);
  const nsStyleColor* arrowColor =
    (const nsStyleColor*)aScrollbarArrowStyleContext->GetStyleData(eStyleStruct_Color);

  // Paint background for scrollbar
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, aRect, *scrollbarColor, *arrowSpacing, 0, 0);

  if (PR_TRUE == aHorizontal) {
    // Draw horizontal Arrow
		nscoord arrowWidth = aRect.height;
		nsRect arrowLeftRect(aRect.x, aRect.y, arrowWidth, arrowWidth);
    PaintArrow(eArrowDirection_Left,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowLeftRect,aOnePixel, aScrollbarArrowStyleContext,
					*arrowSpacing, aForFrame, aFrameRect);

		nsRect thumbRect(aRect.x+arrowWidth, aRect.y, arrowWidth, arrowWidth);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, thumbRect, *arrowColor, *arrowSpacing, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                                aDirtyRect, thumbRect, *arrowSpacing,aScrollbarArrowStyleContext, 0);

    nsRect arrowRightRect(aRect.x + (aRect.width - arrowWidth), aRect.y, arrowWidth, arrowWidth);
	  PaintArrow(eArrowDirection_Right,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowRightRect,aOnePixel, aScrollbarArrowStyleContext,
					*arrowSpacing, aForFrame, aFrameRect);

	}
  else {
		// Paint vertical arrow
		nscoord arrowHeight = aRect.width;
		nsRect arrowUpRect(aRect.x, aRect.y, arrowHeight, arrowHeight);
	  PaintArrow(eArrowDirection_Up,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowUpRect,aOnePixel, aScrollbarArrowStyleContext,
					*arrowSpacing, aForFrame, aFrameRect);

		nsRect thumbRect(aRect.x, aRect.y+arrowHeight, arrowHeight, arrowHeight);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, thumbRect, *arrowColor, *arrowSpacing, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                                aDirtyRect, thumbRect, *arrowSpacing,aScrollbarArrowStyleContext, 0);

		nsRect arrowDownRect(aRect.x, aRect.y + (aRect.height - arrowHeight), arrowHeight, arrowHeight);
	  PaintArrow(eArrowDirection_Down,aRenderingContext,aPresContext, 
			        aDirtyRect, arrowDownRect,aOnePixel, aScrollbarArrowStyleContext,
					*arrowSpacing, aForFrame, aFrameRect);
  }

}



void
nsFormControlHelper::PaintFixedSizeCheckMark(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips)
{

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
nsFormControlHelper::PaintFixedSizeCheckMarkBorder(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips, const nsStyleColor& aBackgroundColor)
{
    // Offsets to x,y location
  PRUint32 ox = 0;
  PRUint32 oy = 0;

  nscoord onePixel = NSIntPixelsToTwips(1, aPixelsToTwips);
  nscoord twelvePixels = NSIntPixelsToTwips(12, aPixelsToTwips);

    // Draw Background
  aRenderingContext.SetColor(aBackgroundColor.mColor);
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

//Draw a checkmark in any size.
void
nsFormControlHelper::PaintCheckMark(nsIRenderingContext& aRenderingContext,
                                    float aPixelsToTwips, const nsRect & aRect)
{
 // Width and height of the fixed size checkmark in TWIPS.
  const PRInt32 fixedSizeCheckmarkWidth = 165;
  const PRInt32 fixedSizeCheckmarkHeight = 165;

  if ((fixedSizeCheckmarkWidth == aRect.width)  &&
      (fixedSizeCheckmarkHeight == aRect.height)) {
      // Standard size, so draw a fixed size check mark instead of a scaled check mark.
      PaintFixedSizeCheckMark(aRenderingContext, aPixelsToTwips);
    return;
  }

  const PRUint32 checkpoints = 7;
  const PRUint32 checksize   = 9; //This is value is determined by added 2 units to the end
                                //of the 7X& pixel rectangle below to provide some white space
                                //around the checkmark when it is rendered.

    // Points come from the coordinates on a 7X7 pixels 
    // box with 0,0 at the lower left. 
  nscoord checkedPolygonDef[] = {0,2, 2,4, 6,0 , 6,2, 2,6, 0,4, 0,2 };
    // Location of the center point of the checkmark
  const PRUint32 centerx = 3;
  const PRUint32 centery = 3;
  
  nsPoint checkedPolygon[checkpoints];
  PRUint32 defIndex = 0;
  PRUint32 polyIndex = 0;

     // Scale the checkmark based on the smallest dimension
  PRUint32 size = aRect.width / checksize;
  if (aRect.height < aRect.width) {
   size = aRect.height / checksize;
  }
  
    // Center and offset each point in the polygon definition.
  for (defIndex = 0; defIndex < (checkpoints * 2); defIndex++) {
    checkedPolygon[polyIndex].x = nscoord((((checkedPolygonDef[defIndex]) - centerx) * (size)) + (aRect.width / 2) + aRect.x);
    defIndex++;
    checkedPolygon[polyIndex].y = nscoord((((checkedPolygonDef[defIndex]) - centery) * (size)) + (aRect.height / 2) + aRect.y);
    polyIndex++;
  }
  
  aRenderingContext.FillPolygon(checkedPolygon, checkpoints);
}

void
nsFormControlHelper::PaintFocus(nsIRenderingContext& aRenderingContext,
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
    nsCSSRendering::DrawDashedSides(0, aRenderingContext,aDirtyRect, borderStyles, borderColors, aOutside,
                                    aInside, PR_FALSE, nsnull);
}


void 
nsFormControlHelper::PaintRectangularButton(nsIPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect, const nsRect& aRect, 
                            PRBool aPressed, 
                            PRBool aShowFocus, 
                            PRBool aDisabled,
                            PRBool aDrawOutline,
                            nsIStyleContext* outlineStyle,
                            nsIStyleContext* focusStyle,
                            nsIStyleContext* aStyleContext, 
                            nsString& aLabel, 
                            nsIFrame* aForFrame)
                          
{
    nsIFormControlFrame * formFrame;
    nsresult result = aForFrame->QueryInterface(kIFormControlFrameIID, (void**)&formFrame);
    if (NS_OK != result) {
      NS_RELEASE(formFrame);
    }

    aRenderingContext.PushState();
     // Draw border using CSS
     // Get the Scrollbar's Arrow's Style structs
    const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
    const nsStyleColor* color =
    (const nsStyleColor*)aStyleContext->GetStyleData(eStyleStruct_Color);

	
    nsRect rect(aRect.x, aRect.y, aRect.width, aRect.height);

	const nsStyleSpacing* outline = (const nsStyleSpacing*)outlineStyle->GetStyleData(eStyleStruct_Spacing);
  	const nsStyleSpacing* focus = (const nsStyleSpacing*)focusStyle->GetStyleData(eStyleStruct_Spacing);
 
	nsMargin outlineBorder;
    outline->CalcBorderFor(aForFrame, outlineBorder);

 	nsMargin focusBorder;
    focus->CalcBorderFor(aForFrame, focusBorder);

	int left = (focusBorder.left > outlineBorder.left) ? focusBorder.left : outlineBorder.left;
	int right = (focusBorder.right > outlineBorder.right) ? focusBorder.right : outlineBorder.right;
	int top = (focusBorder.top > outlineBorder.top) ? focusBorder.top : outlineBorder.top;
	int bottom = (focusBorder.bottom > outlineBorder.bottom) ? focusBorder.bottom : outlineBorder.bottom;

	if (aShowFocus) {
		int l = left - focusBorder.left;
		int t = top - focusBorder.top;
		int b = bottom - focusBorder.bottom;
		int r = right - focusBorder.right;

		int w = aRect.width - (l + r);
		int h = aRect.height - (t + b);

		nsRect focusRect(l, t, w, h);

		nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
								   aDirtyRect, focusRect, *focus, aStyleContext, 0);
	} 
	
	if (aDrawOutline) {
		int l = left - outlineBorder.left;
		int t = top - outlineBorder.top;
		int b = bottom - outlineBorder.bottom;
		int r = right - outlineBorder.right;

		int w = aRect.width - (l + r);
		int h = aRect.height - (t + b);

		nsRect outlineRect(l, t, w, h);

		nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
								   aDirtyRect, outlineRect, *outline, aStyleContext, 0);
	}

	rect.x += left;
	rect.y += right;
	rect.width -= (left + right);
	rect.height -= (top + bottom);

	nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, rect,  *color, *spacing, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aForFrame,
                               aDirtyRect, rect, *spacing,aStyleContext, 0);

    nsMargin border;
    spacing->CalcBorderFor(aForFrame, border);


    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    nsRect outside(aRect.x, aRect.y, aRect.width, aRect.height);
    outside.Deflate(border);
    outside.Deflate(onePixel, onePixel);

    nsRect inside(outside);
    inside.Deflate(onePixel, onePixel);

/*
    if (aShowFocus) { 
      PaintFocus(aRenderingContext,
                 aDirtyRect, inside, outside);
    }
	*/

	aRenderingContext.SetColor(NS_RGB(255,255,255));

    float appUnits;
    float devUnits;
    float scale;
    nsIDeviceContext * context;
    aRenderingContext.GetDeviceContext(context);

    context->GetCanonicalPixelScale(scale);
    context->GetAppUnitsToDevUnits(devUnits);
    context->GetDevUnitsToAppUnits(appUnits);

    const nsFont * font = nsnull;
    nsresult res = formFrame->GetFont(aPresContext, font);
    if (NS_SUCCEEDED(res) && font != nsnull) {
      aRenderingContext.SetFont(*font);
    }

    nscoord ascent;
    nscoord descent;
  
    nscoord textWidth;
    nscoord textHeight;
    aRenderingContext.GetWidth(aLabel, textWidth);

    nsIFontMetrics* metrics;
    context->GetMetricsFor(*font, metrics);
    metrics->GetMaxAscent(ascent);
    metrics->GetMaxDescent(descent);
    textHeight = ascent + descent;

    nscoord x = ((inside.width  - textWidth) / 2)  + inside.x;
    nscoord y = ((inside.height - textHeight) / 2) + inside.y;
    if (PR_TRUE == aPressed) {
      x += onePixel;
      y += onePixel;
    }

	if (aDisabled) {
	   aRenderingContext.SetColor(NS_RGB(255,255,255));
       aRenderingContext.DrawString(aLabel, x+onePixel, y+onePixel);
	}

	aRenderingContext.SetColor(color->mColor);
    aRenderingContext.DrawString(aLabel, x, y);

    NS_RELEASE(context);
    PRBool clipEmpty;
    aRenderingContext.PopState(clipEmpty);

    NS_RELEASE(formFrame);
}


void 
nsFormControlHelper::GetCircularRect(PRUint32 aWidth, PRUint32 aHeight, nsRect& aRect)
{
 if (aWidth > aHeight)
    aRect.SetRect((aWidth/2) - (aHeight/2), 0, aHeight, aHeight);
  else
    aRect.SetRect(0, (aHeight/2) - (aWidth/2), aWidth, aWidth);
}


void
nsFormControlHelper::PaintCircularBackground(nsIPresContext* aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect, nsIStyleContext* aStyleContext, PRBool aInset,
                         nsIFrame* aForFrame, PRUint32 aWidth, PRUint32 aHeight)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel     = NSIntPixelsToTwips(1, p2t);

  nsRect outside;
  nsFormControlHelper::GetCircularRect(aWidth, aHeight, outside);
  
  outside.Deflate(onePixel, onePixel);
  outside.Deflate(onePixel, onePixel);
  const nsStyleColor* color = (const nsStyleColor*)
     aStyleContext->GetStyleData(eStyleStruct_Color);
  aRenderingContext.SetColor(color->mBackgroundColor);
  aRenderingContext.FillArc(outside, 0, 180);
  aRenderingContext.FillArc(outside, 180, 360);
}

//XXX: TODO Remove this. It is no longer needed since circular borders
//are painted by the standard border code by setting a border radius
//to 1/2 the width of the border
void
nsFormControlHelper::PaintCircularBorder(nsIPresContext* aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect, nsIStyleContext* aStyleContext, PRBool aInset,
                         nsIFrame* aForFrame, PRUint32 aWidth, PRUint32 aHeight)
{
  const PRUint32 standardWidth = 180;
  const PRUint32 standardHeight = 180;

  aRenderingContext.PushState();

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
 
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  spacing->CalcBorderFor(aForFrame, border);

  nscoord onePixel     = NSIntPixelsToTwips(1, p2t);
  nscoord twelvePixels = NSIntPixelsToTwips(12, p2t);

  nsRect outside(0, 0, aWidth, aHeight);

  PRBool standardSize = ((aWidth == standardWidth) && (aHeight == standardHeight));
  if (PR_TRUE == standardSize) {
    outside.SetRect(0, 0, twelvePixels, twelvePixels);

    if (PR_TRUE == aInset) {
      outside.Deflate(onePixel, onePixel);
      outside.Deflate(onePixel, onePixel);
      aRenderingContext.SetColor(NS_RGB(192,192,192));
      aRenderingContext.FillArc(outside, 0, 180);
      aRenderingContext.FillArc(outside, 180, 360);
      outside.SetRect(0, 0, twelvePixels, twelvePixels);
    }

    // DrakGray    
//    PRUint8 borderStyle = aBorderStyle.GetBorderStyle(NS_SIDE_TOP),
//    nscolor borderColor = aBorderStyle.GetBorderColor(NS_SIDE_TOP),
		
    aRenderingContext.SetColor(NS_RGB(128,128,128));
//    aRenderingContext.SetColor(borderColor);
    PaintLine(aRenderingContext, 4, 0, 7, 0, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 2, 1, 3, 1, PR_TRUE, 1, onePixel);
    PaintLine(aRenderingContext, 8, 1, 9, 1, PR_TRUE, 1, onePixel);

    PaintLine(aRenderingContext, 1, 2, 1, 3, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 0, 4, 0, 7, PR_FALSE, 1, onePixel);
    PaintLine(aRenderingContext, 1, 8, 1, 9, PR_FALSE, 1, onePixel);

    // Black
//    nscolor borderColor = aBorderStyle.GetBorderColor(NS_SIDE_BOTTOM);
//    aRenderingContext.SetColor(borderColor);
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
    nsFormControlHelper::GetCircularRect(aWidth, aHeight, outside);
    
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
  }

  PRBool clip;
  aRenderingContext.PopState(clip);
}


nsresult
nsFormControlHelper::GetName(nsIContent* aContent,nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (nsnull != aContent) {
    nsIHTMLContent* formControl = nsnull;
    result = aContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
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


nsresult
nsFormControlHelper::GetValue(nsIContent* aContent, nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (nsnull != aContent) {
    nsIHTMLContent* formControl = nsnull;
    result = aContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
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

nsresult
nsFormControlHelper::GetInputElementValue(nsIContent* aContent, nsString* aText, PRBool aInitialValue)
{
  nsresult result = NS_OK;
  nsIDOMHTMLInputElement* inputElem = nsnull;
  result = aContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElem);
  if ((NS_OK == result) && inputElem) {
    if (PR_TRUE == aInitialValue) {
      result = inputElem->GetDefaultValue(*aText);
    }
    else {
      result = inputElem->GetValue(*aText);
    }

    NS_RELEASE(inputElem);
  }
  return result;
}

//----------------------------------------------------------------------------------
#define form_properties "chrome://communicator/locale/layout/HtmlForm.properties"

// Return localised string for resource string (e.g. "Submit" -> "Submit Query")
// This code is derived from nsBookmarksService::Init() and cookie_Localize()
nsresult
nsFormControlHelper::GetLocalizedString(char* aKey, nsString& oVal)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundle> bundle;
  
  // Create a URL for the string resource file
  // Create a bundle for the localization
  NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && pNetService) {
    nsCOMPtr<nsIURI> uri;
    rv = pNetService->NewURI(form_properties, nsnull, getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv) && uri) {

      // Create bundle
      NS_WITH_SERVICE(nsIStringBundleService, stringService, kStringBundleServiceCID, &rv);
      if (NS_SUCCEEDED(rv) && stringService) {
        nsXPIDLCString spec;
        rv = uri->GetSpec(getter_Copies(spec));
        if (NS_SUCCEEDED(rv) && spec) {
          nsCOMPtr<nsILocale> locale = nsnull;
          rv = stringService->CreateBundle(spec, locale, getter_AddRefs(bundle));
        }
      }
    }
  }

  // Determine default label from string bundle
  if (NS_SUCCEEDED(rv) && bundle && aKey) {
    nsXPIDLString valUni;
    nsAutoString key; key.AssignWithConversion(aKey);
    rv = bundle->GetStringFromName(key.GetUnicode(), getter_Copies(valUni));
    if (NS_SUCCEEDED(rv) && valUni) {
      oVal.Assign(valUni);
    } else {
      oVal.Truncate();
    }
  }
  return rv;
}

// Return value of disabled attribute or PR_FALSE if none set
nsresult
nsFormControlHelper::GetDisabled(nsIContent* aContent, PRBool* oIsDisabled)
{
  nsCOMPtr<nsIHTMLContent> formControl = do_QueryInterface(aContent);
  nsHTMLValue value;
  nsresult result = formControl->GetHTMLAttribute(nsHTMLAtoms::disabled, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    if (eHTMLUnit_Empty == value.GetUnit()) {
      *oIsDisabled = PR_TRUE;
    } else {
      *oIsDisabled = PR_FALSE;
    }
  } else {
    *oIsDisabled = PR_FALSE;
  }
  return NS_OK;
}
