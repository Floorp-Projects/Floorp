/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsCoord.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIComponentManager.h"
#include "nsGUIEvent.h"
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
#include "nsIContent.h"
#include "nsStyleUtil.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsNetCID.h"

// Needed for Localization
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsITextContent.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
// done I10N


// For figuring out the "WRAP" property
#define kTextControl_Wrap_Soft "SOFT"
#define kTextControl_Wrap_Virtual "VIRTUAL"   // "virtual" is a synonym for "soft"
#define kTextControl_Wrap_Hard "HARD"
#define kTextControl_Wrap_Physical "PHYSICAL" // "physical" should be a synonym
                                              // for "hard" but NS 4.x and IE make
                                              // it a synonym for "soft" 
#define kTextControl_Wrap_Off  "OFF"


MOZ_DECL_CTOR_COUNTER(nsFormControlHelper)

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
  aString.ReplaceSubstring(NS_LITERAL_STRING("\r\n").get(), NS_LITERAL_STRING("\n").get());

  // Mac linebreaks: Map any remaining CR to LF:
  aString.ReplaceSubstring(NS_LITERAL_STRING("\r").get(), NS_LITERAL_STRING("\n").get());
}

PRBool nsFormControlHelper::GetBool(const nsAString& aValue)
{
  return aValue.Equals(NS_STRING_TRUE);
}

void nsFormControlHelper::GetBoolString(const PRBool aValue,
                                        nsAString& aResult)
{
  if (aValue)
    aResult.Assign(NS_STRING_TRUE); 
  else
    aResult.Assign(NS_STRING_FALSE);
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
  nsCOMPtr<nsIHTMLContent> content(do_QueryInterface(aContent));

  if (content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::wrap, value);
    if (eHTMLUnit_String == value.GetUnit()) { 
      value.GetStringValue(aOutValue);
    }
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

    if (wrap.EqualsIgnoreCase(kTextControl_Wrap_Hard)) {
      aWrapProp = eHTMLTextWrap_Hard;
      return result;
    }

    if (wrap.EqualsIgnoreCase(kTextControl_Wrap_Soft) ||
        wrap.EqualsIgnoreCase(kTextControl_Wrap_Virtual) ||
        wrap.EqualsIgnoreCase(kTextControl_Wrap_Physical)) {
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
  GetRepChars(char1, char2);

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
  GetRepChars(char1, char2);
  nscoord char1Width, char2Width;
  aRendContext->GetWidth(char1, char1Width);
  aRendContext->GetWidth(char2, char2Width);

  return ((char1Width + char2Width) / 2) + 1;
}  
  
nscoord
nsFormControlHelper::GetTextSize(nsIPresContext* aPresContext, nsIFormControlFrame* aFrame,
                                PRInt32 aNumChars, nsSize& aSize,
                                nsIRenderingContext *aRendContext)
{
  nsAutoString val;
  char char1, char2;
  GetRepChars(char1, char2);
  int i;
  for (i = 0; i < aNumChars; i+=2) {
    val.Append(PRUnichar(char1));
  }
  for (i = 1; i < aNumChars; i+=2) {
    val.Append(PRUnichar(char2));
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

  nsCOMPtr<nsIContent> iContent;
  aFrame->GetFormContent(*getter_AddRefs(iContent));

  nsCOMPtr<nsIHTMLContent> hContent(do_QueryInterface(iContent));

  if (!hContent) {
    return 0;
  }

  nsAutoString valAttr;
  nsresult valStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aSpec.mColValueAttr) {
    valStatus = hContent->GetAttr(kNameSpaceID_None, aSpec.mColValueAttr,
                                  valAttr);
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
  nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIFrame), (void**)&iFrame);
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

  // determine the width, char height, row height
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
    if (aSpec.mColSizeAttrInPixels) {
      // need to set charWidth and aDesiredSize.height
      charWidth = GetTextSize(aPresContext, aFrame, 1, aDesiredSize, aRendContext);
      col = (col <= 0) ? 15 : col; // XXX why a default of 15 pixels, why hide it
                                   // XXX this conflicts with a default of 20 found in nsTextControlFrame.
      aDesiredSize.width = NSIntPixelsToTwips(col, p2t);
    } else {
      col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
    }
    if (aSpec.mColSizeAttrInPixels) {
      aWidthExplicit = PR_TRUE;
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
      charWidth = GetTextSize(aPresContext, aFrame, aSpec.mColDefaultSize, aDesiredSize, aRendContext); 
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

  if (ATTR_NOTSET == numRows) {
    numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;
  }

  return numRows;
}


nsresult  
nsFormControlHelper::GetFont(nsIFormControlFrame * aFormFrame,
                             nsIPresContext*       aPresContext, 
                             nsIStyleContext *     aStyleContext, 
                             const nsFont*&        aFont)
{
  const nsStyleFont* styleFont = (const nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font);

  aFont = &styleFont->mFont;
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

nsresult
nsFormControlHelper::GetName(nsIContent* aContent, nsAString* aResult)
{
  NS_PRECONDITION(aResult, "Null pointer bad!");
  nsCOMPtr<nsIHTMLContent> formControl(do_QueryInterface(aContent));
  if (!formControl)
    return NS_ERROR_FAILURE;

  nsHTMLValue value;
  nsresult rv = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == rv && eHTMLUnit_String == value.GetUnit()) {
    value.GetStringValue(*aResult);
  }
  return rv;
}

nsresult
nsFormControlHelper::GetType(nsIContent* aContent, PRInt32* aType)
{
  NS_PRECONDITION(aType, "Null pointer bad!");
  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(aContent));
  if (!formControl)
    return NS_ERROR_FAILURE;

  return formControl->GetType(aType);
}

nsresult
nsFormControlHelper::GetValueAttr(nsIContent* aContent, nsAString* aResult)
{
  NS_PRECONDITION(aResult, "Null pointer bad!");
  nsCOMPtr<nsIHTMLContent> formControl(do_QueryInterface(aContent));
  if (!formControl)
    return NS_ERROR_FAILURE;

  nsHTMLValue value;
  nsresult rv = formControl->GetHTMLAttribute(nsHTMLAtoms::value, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == rv && eHTMLUnit_String == value.GetUnit()) {
    value.GetStringValue(*aResult);
  }
  return rv;
}

//----------------------------------------------------------------------------------
// Return localised string for resource string (e.g. "Submit" -> "Submit Query")
// This code is derived from nsBookmarksService::Init() and cookie_Localize()
nsresult
nsFormControlHelper::GetLocalizedString(const char * aPropFileName, const PRUnichar* aKey, nsString& oVal)
{
  NS_ENSURE_ARG_POINTER(aKey);
  
  nsresult rv;
  
  nsCOMPtr<nsIStringBundle> bundle;
  
  // Create a bundle for the localization
  nsCOMPtr<nsIStringBundleService> stringService = 
    do_GetService(kStringBundleServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && stringService)
    rv = stringService->CreateBundle(aPropFileName, getter_AddRefs(bundle));

  // Determine default label from string bundle
  if (NS_SUCCEEDED(rv) && bundle) {
    nsXPIDLString valUni;
    rv = bundle->GetStringFromName(aKey, getter_Copies(valUni));
    if (NS_SUCCEEDED(rv) && valUni) {
      oVal.Assign(valUni);
    } else {
      oVal.Truncate();
    }
  }
  return rv;
}

nsresult
nsFormControlHelper::Reset(nsIFrame* aFrame, nsIPresContext* aPresContext)
{
  nsCOMPtr<nsIContent> controlContent;
  aFrame->GetContent(getter_AddRefs(controlContent));

  nsCOMPtr<nsIFormControl> control = do_QueryInterface(controlContent);
  if (control) {
    control->Reset();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

void
nsFormControlHelper::StyleChangeReflow(nsIPresContext* aPresContext,
                                       nsIFrame* aFrame)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  nsHTMLReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        eReflowType_StyleChanged);
  if (NS_SUCCEEDED(rv)) {
    shell->AppendReflowCommand(reflowCmd);
  }
}


