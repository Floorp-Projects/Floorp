/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsFormControlHelper.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
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
#include "nsIContent.h"
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
// See GetWrapPropertyEnum for details
#define kTextControl_Wrap_Soft "SOFT"
#define kTextControl_Wrap_Hard "HARD"
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


nsresult nsFormControlHelper::GetFrameFontFM(nsIFrame* aFrame,
                                             nsIFontMetrics** aFontMet)
{
  nsStyleContext* sc = aFrame->GetStyleContext();
  return aFrame->GetPresContext()->DeviceContext()->
    GetMetricsFor(sc->GetStyleFont()->mFont,
                  sc->GetStyleVisibility()->mLangGroup,
                  *aFontMet);
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
  // soft is the default; "physical" defaults to soft as well because all other
  // browsers treat it that way and there is no real reason to maintain physical
  // and virtual as separate entities if no one else does.  Only hard and off
  // do anything different.
  aWrapProp = eHTMLTextWrap_Soft; // the default
  
  nsAutoString wrap;
  nsresult rv = GetWrapProperty(aContent, wrap);

  if (rv != NS_CONTENT_ATTR_NOT_THERE) {

    if (wrap.EqualsIgnoreCase(kTextControl_Wrap_Hard)) {
      aWrapProp = eHTMLTextWrap_Hard;
    } else if (wrap.EqualsIgnoreCase(kTextControl_Wrap_Off)) {
      aWrapProp = eHTMLTextWrap_Off;
    }
  }
  return rv;
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

PRInt32
nsFormControlHelper::GetType(nsIContent* aContent)
{
  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(aContent));
  if (formControl) {
    return formControl->GetType();
  }

  NS_ERROR("Form control not implementing nsIFormControl, assuming TEXT type");
  return NS_FORM_INPUT_TEXT;
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
nsFormControlHelper::Reset(nsIFrame* aFrame, nsPresContext* aPresContext)
{
  nsCOMPtr<nsIFormControl> control = do_QueryInterface(aFrame->GetContent());
  if (control) {
    control->Reset();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

void
nsFormControlHelper::StyleChangeReflow(nsPresContext* aPresContext,
                                       nsIFrame* aFrame)
{
  nsHTMLReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        eReflowType_StyleChanged);
  if (NS_SUCCEEDED(rv)) {
    aPresContext->PresShell()->AppendReflowCommand(reflowCmd);
  }
}


