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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
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

#include "nsINameSpaceManager.h"
#include "nsMathMLFrame.h"
#include "nsMathMLChar.h"
#include "nsCSSAnonBoxes.h"

// used to map attributes into CSS rules
#include "nsIDocument.h"
#include "nsStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsICSSStyleSheet.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsICSSRule.h"
#include "nsICSSStyleRule.h"
#include "nsStyleChangeList.h"
#include "nsFrameManager.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsContentCID.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
static NS_DEFINE_CID(kCSSStyleSheetCID, NS_CSS_STYLESHEET_CID);


NS_IMPL_QUERY_INTERFACE1(nsMathMLFrame, nsIMathMLFrame)

NS_IMETHODIMP
nsMathMLFrame::InheritAutomaticData(nsIPresContext* aPresContext,
                                    nsIFrame*       aParent) 
{
  mEmbellishData.flags = 0;
  mEmbellishData.nextFrame = nsnull;
  mEmbellishData.coreFrame = nsnull;
  mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  mEmbellishData.leftSpace = 0;
  mEmbellishData.rightSpace = 0;

  mPresentationData.flags = 0;
  mPresentationData.mstyle = nsnull;
  mPresentationData.scriptLevel = 0;

  // by default, just inherit the display & scriptlevel of our parent
  nsPresentationData parentData;
  GetPresentationDataFrom(aParent, parentData);
  mPresentationData.mstyle = parentData.mstyle;
  mPresentationData.scriptLevel = parentData.scriptLevel;
  if (NS_MATHML_IS_DISPLAYSTYLE(parentData.flags)) {
    mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLFrame::UpdatePresentationData(nsIPresContext* aPresContext,
                                      PRInt32         aScriptLevelIncrement,
                                      PRUint32        aFlagsValues,
                                      PRUint32        aFlagsToUpdate)
{
  mPresentationData.scriptLevel += aScriptLevelIncrement;
  // update flags that are relevant to this call
  if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
    // updating the displaystyle flag is allowed
    if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsValues)) {
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
    else {
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }
  if (NS_MATHML_IS_COMPRESSED(aFlagsToUpdate)) {
    // updating the compression flag is allowed
    if (NS_MATHML_IS_COMPRESSED(aFlagsValues)) {
      // 'compressed' means 'prime' style in App. G, TeXbook
      mPresentationData.flags |= NS_MATHML_COMPRESSED;
    }
    // no else. the flag is sticky. it retains its value once it is set
  }
  return NS_OK;
}

// Helper to give a style context suitable for doing the stretching of
// a MathMLChar. Frame classes that use this should ensure that the 
// extra leaf style contexts given to the MathMLChars are acessible to
// the Style System via the Get/Set AdditionalStyleContext() APIs.
/* static */ void
nsMathMLFrame::ResolveMathMLCharStyle(nsIPresContext*  aPresContext,
                                      nsIContent*      aContent,
                                      nsStyleContext*  aParentStyleContext,
                                      nsMathMLChar*    aMathMLChar,
                                      PRBool           aIsMutableChar)
{
  nsIAtom* pseudoStyle = (aIsMutableChar) ?
    nsCSSAnonBoxes::mozMathStretchy :
    nsCSSAnonBoxes::mozMathAnonymous; // savings
  nsRefPtr<nsStyleContext> newStyleContext;
  newStyleContext = aPresContext->StyleSet()->
    ResolvePseudoStyleFor(aContent, pseudoStyle, aParentStyleContext);

  if (newStyleContext)
    aMathMLChar->SetStyleContext(newStyleContext);
}

/* static */ void
nsMathMLFrame::GetEmbellishDataFrom(nsIFrame*        aFrame,
                                    nsEmbellishData& aEmbellishData)
{
  // initialize OUT params
  aEmbellishData.flags = 0;
  aEmbellishData.nextFrame = nsnull;
  aEmbellishData.coreFrame = nsnull;
  aEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  aEmbellishData.leftSpace = 0;
  aEmbellishData.rightSpace = 0;

  if (aFrame) {
    nsIMathMLFrame* mathMLFrame;
    aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (mathMLFrame) {
      mathMLFrame->GetEmbellishData(aEmbellishData);
    }
  }
}

// helper to get the presentation data of a frame, by possibly walking up
// the frame hierarchy if we happen to be surrounded by non-MathML frames.
/* static */ void
nsMathMLFrame::GetPresentationDataFrom(nsIFrame*           aFrame,
                                       nsPresentationData& aPresentationData,
                                       PRBool              aClimbTree)
{
  // initialize OUT params
  aPresentationData.flags = 0;
  aPresentationData.mstyle = nsnull;
  aPresentationData.scriptLevel = 0;

  nsIFrame* frame = aFrame;
  while (frame) {
    nsIMathMLFrame* mathMLFrame;
    frame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (mathMLFrame) {
      mathMLFrame->GetPresentationData(aPresentationData);
      break;
    }
    // stop if the caller doesn't want to lookup beyond the frame
    if (!aClimbTree) {
      break;
    }
    // stop if we reach the root <math> tag
    nsIContent* content = frame->GetContent();
    NS_ASSERTION(content, "dangling frame without a content node");
    if (!content)
      break;

    if (content->Tag() == nsMathMLAtoms::math) {
      const nsStyleDisplay* display = frame->GetStyleDisplay();
      if (display->mDisplay == NS_STYLE_DISPLAY_BLOCK) {
        aPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
      }
      break;
    }
    frame = frame->GetParent();
  }
  NS_ASSERTION(frame, "bad MathML markup - could not find the top <math> element");
}

// helper to get an attribute from the content or the surrounding <mstyle> hierarchy
/* static */ nsresult
nsMathMLFrame::GetAttribute(nsIContent* aContent,
                            nsIFrame*   aMathMLmstyleFrame,
                            nsIAtom*    aAttributeAtom,
                            nsString&   aValue)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

  // see if we can get the attribute from the content
  if (aContent) {
    rv = aContent->GetAttr(kNameSpaceID_None, aAttributeAtom, aValue);
  }

  if (NS_CONTENT_ATTR_NOT_THERE == rv) {
    // see if we can get the attribute from the mstyle frame
    if (aMathMLmstyleFrame) {
      nsIFrame* mstyleParent = aMathMLmstyleFrame->GetParent();

      nsPresentationData mstyleParentData;
      mstyleParentData.mstyle = nsnull;

      if (mstyleParent) {
        nsIMathMLFrame* mathMLFrame;
        mstyleParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
        if (mathMLFrame) {
          mathMLFrame->GetPresentationData(mstyleParentData);
        }
      }

      // recurse all the way up into the <mstyle> hierarchy
      rv = GetAttribute(aMathMLmstyleFrame->GetContent(),
			mstyleParentData.mstyle, aAttributeAtom, aValue);
    }
  }
  return rv;
}

/* static */ void
nsMathMLFrame::GetRuleThickness(nsIRenderingContext& aRenderingContext,
                                nsIFontMetrics*      aFontMetrics,
                                nscoord&             aRuleThickness)
{
  // get the bounding metrics of the overbar char, the rendering context
  // is assumed to have been set with the font of the current style context
#ifdef NS_DEBUG
  const nsFont* myFont;
  aFontMetrics->GetFont(myFont);
  nsCOMPtr<nsIFontMetrics> currFontMetrics;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(currFontMetrics));
  const nsFont* currFont;
  currFontMetrics->GetFont(currFont);
  NS_ASSERTION(currFont->Equals(*myFont), "unexpected state");
#endif
  nscoord xHeight;
  aFontMetrics->GetXHeight(xHeight);
  PRUnichar overBar = 0x00AF;
  nsBoundingMetrics bm;
  nsresult rv = aRenderingContext.GetBoundingMetrics(&overBar, PRUint32(1), bm);
  if (NS_SUCCEEDED(rv)) {
    aRuleThickness = bm.ascent + bm.descent;
  }
  if (NS_FAILED(rv) || aRuleThickness <= 0 || aRuleThickness >= xHeight) {
    // fall-back to the other version
    GetRuleThickness(aFontMetrics, aRuleThickness);
  }

#if 0
  nscoord oldRuleThickness;
  GetRuleThickness(aFontMetrics, oldRuleThickness);

  PRUnichar sqrt = 0xE063; // a sqrt glyph from TeX's CMEX font
  rv = aRenderingContext.GetBoundingMetrics(&sqrt, PRUint32(1), bm);
  nscoord sqrtrule = bm.ascent; // according to TeX, the ascent should be the rule

  printf("xheight:%4d rule:%4d oldrule:%4d  sqrtrule:%4d\n",
          xHeight, aRuleThickness, oldRuleThickness, sqrtrule);
#endif
}

/* static */ void
nsMathMLFrame::GetAxisHeight(nsIRenderingContext& aRenderingContext,
                             nsIFontMetrics*      aFontMetrics,
                             nscoord&             aAxisHeight)
{
  // get the bounding metrics of the minus sign, the rendering context
  // is assumed to have been set with the font of the current style context
#ifdef NS_DEBUG
  const nsFont* myFont;
  aFontMetrics->GetFont(myFont);
  nsCOMPtr<nsIFontMetrics> currFontMetrics;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(currFontMetrics));
  const nsFont* currFont;
  currFontMetrics->GetFont(currFont);
  NS_ASSERTION(currFont->Equals(*myFont), "unexpected state");
#endif
  nscoord xHeight;
  aFontMetrics->GetXHeight(xHeight);
  PRUnichar minus = 0x2212; // not '-', but official Unicode minus sign
  nsBoundingMetrics bm;
  nsresult rv = aRenderingContext.GetBoundingMetrics(&minus, PRUint32(1), bm);
  if (NS_SUCCEEDED(rv)) {
    aAxisHeight = bm.ascent - (bm.ascent + bm.descent)/2;
  }
  if (NS_FAILED(rv) || aAxisHeight <= 0 || aAxisHeight >= xHeight) {
    // fall-back to the other version
    GetAxisHeight(aFontMetrics, aAxisHeight);
  }
}

// ================
// Utilities for parsing and retrieving numeric values
// All returned values are in twips.

/*
The REC says:
  An explicit plus sign ('+') is not allowed as part of a numeric value
  except when it is specifically listed in the syntax (as a quoted '+'  or "+"),

  Units allowed
  ID  Description
  em  ems (font-relative unit traditionally used for horizontal lengths)
  ex  exs (font-relative unit traditionally used for vertical lengths)
  px  pixels, or pixel size of a "typical computer display"
  in  inches (1 inch = 2.54 centimeters)
  cm  centimeters
  mm  millimeters
  pt  points (1 point = 1/72 inch)
  pc  picas (1 pica = 12 points)
  %   percentage of default value

Implementation here:
  The numeric value is valid only if it is of the form nnn.nnn [h/v-unit]
*/

/* static */ PRBool
nsMathMLFrame::ParseNumericValue(nsString&   aString,
                                 nsCSSValue& aCSSValue)
{
  aCSSValue.Reset();
  aString.CompressWhitespace(); //  aString is not a const in this code...

  PRInt32 stringLength = aString.Length();
  if (!stringLength)
    return PR_FALSE;

  nsAutoString number, unit;

  // Gather up characters that make up the number
  PRBool gotDot = PR_FALSE;
  PRUnichar c;
  for (PRInt32 i = 0; i < stringLength; i++) {
    c = aString[i];
    if (gotDot && c == '.')
      return PR_FALSE;  // two dots encountered
    else if (c == '.')
      gotDot = PR_TRUE;
    else if (!nsCRT::IsAsciiDigit(c)) {
      aString.Right(unit, stringLength - i);
      unit.CompressWhitespace(); // some authors leave blanks before the unit
      break;
    }
    number.Append(c);
  }

  // on exit, also return a nicer string version of the value in case
  // the caller wants it (e.g., this removes whitespace before units)
  aString.Assign(number);
  aString.Append(unit);

  // Convert number to floating point
  PRInt32 errorCode;
  float floatValue = number.ToFloat(&errorCode);
  if (errorCode)
    return PR_FALSE;

  nsCSSUnit cssUnit;
  if (unit.IsEmpty()) {
    cssUnit = eCSSUnit_Number; // no explicit unit, this is a number that will act as a multiplier
  }
  else if (unit.EqualsLiteral("%")) {
    aCSSValue.SetPercentValue(floatValue / 100.0f);
    return PR_TRUE;
  }
  else if (unit.EqualsLiteral("em")) cssUnit = eCSSUnit_EM;
  else if (unit.EqualsLiteral("ex")) cssUnit = eCSSUnit_XHeight;
  else if (unit.EqualsLiteral("px")) cssUnit = eCSSUnit_Pixel;
  else if (unit.EqualsLiteral("in")) cssUnit = eCSSUnit_Inch;
  else if (unit.EqualsLiteral("cm")) cssUnit = eCSSUnit_Centimeter;
  else if (unit.EqualsLiteral("mm")) cssUnit = eCSSUnit_Millimeter;
  else if (unit.EqualsLiteral("pt")) cssUnit = eCSSUnit_Point;
  else if (unit.EqualsLiteral("pc")) cssUnit = eCSSUnit_Pica;
  else // unexpected unit
    return PR_FALSE;

  aCSSValue.SetFloatValue(floatValue, cssUnit);
  return PR_TRUE;
}

/* static */ nscoord
nsMathMLFrame::CalcLength(nsIPresContext*   aPresContext,
                          nsStyleContext*   aStyleContext,
                          const nsCSSValue& aCSSValue)
{
  NS_ASSERTION(aCSSValue.IsLengthUnit(), "not a length unit");

  if (aCSSValue.IsFixedLengthUnit()) {
    return aCSSValue.GetLengthTwips();
  }

  nsCSSUnit unit = aCSSValue.GetUnit();

  if (eCSSUnit_Pixel == unit) {
    return NSFloatPixelsToTwips(aCSSValue.GetFloatValue(),
                                aPresContext->ScaledPixelsToTwips());
  }
  else if (eCSSUnit_EM == unit) {
    const nsStyleFont* font = aStyleContext->GetStyleFont();
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)font->mFont.size);
  }
  else if (eCSSUnit_XHeight == unit) {
    nscoord xHeight;
    const nsStyleFont* font = aStyleContext->GetStyleFont();
    nsCOMPtr<nsIFontMetrics> fm = aPresContext->GetMetricsFor(font->mFont);
    fm->GetXHeight(xHeight);
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)xHeight);
  }

  return 0;
}

/* static */ PRBool
nsMathMLFrame::ParseNamedSpaceValue(nsIFrame*   aMathMLmstyleFrame,
                                    nsString&   aString,
                                    nsCSSValue& aCSSValue)
{
  aCSSValue.Reset();
  aString.CompressWhitespace(); //  aString is not a const in this code...
  if (!aString.Length()) return PR_FALSE;

  // See if it is one of the 'namedspace' (ranging 1/18em...7/18em)
  PRInt32 i = 0;
  nsIAtom* namedspaceAtom = nsnull;
  if (aString.EqualsLiteral("veryverythinmathspace")) {
    i = 1;
    namedspaceAtom = nsMathMLAtoms::veryverythinmathspace_;
  }
  else if (aString.EqualsLiteral("verythinmathspace")) {
    i = 2;
    namedspaceAtom = nsMathMLAtoms::verythinmathspace_;
  }
  else if (aString.EqualsLiteral("thinmathspace")) {
    i = 3;
    namedspaceAtom = nsMathMLAtoms::thinmathspace_;
  }
  else if (aString.EqualsLiteral("mediummathspace")) {
    i = 4;
    namedspaceAtom = nsMathMLAtoms::mediummathspace_;
  }
  else if (aString.EqualsLiteral("thickmathspace")) {
    i = 5;
    namedspaceAtom = nsMathMLAtoms::thickmathspace_;
  }
  else if (aString.EqualsLiteral("verythickmathspace")) {
    i = 6;
    namedspaceAtom = nsMathMLAtoms::verythickmathspace_;
  }
  else if (aString.EqualsLiteral("veryverythickmathspace")) {
    i = 7;
    namedspaceAtom = nsMathMLAtoms::veryverythickmathspace_;
  }

  if (0 != i) {
    if (aMathMLmstyleFrame) {
      // see if there is a <mstyle> that has overriden the default value
      // GetAttribute() will recurse all the way up into the <mstyle> hierarchy
      nsAutoString value;
      if (NS_CONTENT_ATTR_HAS_VALUE ==
          GetAttribute(nsnull, aMathMLmstyleFrame, namedspaceAtom, value)) {
        if (ParseNumericValue(value, aCSSValue) &&
            aCSSValue.IsLengthUnit()) {
          return PR_TRUE;
        }
      }
    }

    // fall back to the default value
    aCSSValue.SetFloatValue(float(i)/float(18), eCSSUnit_EM);
    return PR_TRUE;
  }

  return PR_FALSE;
}

// ================
// Utils to map attributes into CSS rules (work-around to bug 69409 which
// is not scheduled to be fixed anytime soon)
//

static const PRInt32 kMathMLversion1 = 1;
static const PRInt32 kMathMLversion2 = 2;

struct
nsCSSMapping {
  PRInt32        compatibility;
  const nsIAtom* attrAtom;
  const char*    cssProperty;
};

static void
GetMathMLAttributeStyleSheet(nsIPresContext* aPresContext,
                             nsIStyleSheet** aSheet)
{
  static const char kTitle[] = "Internal MathML/CSS Attribute Style Sheet";
  *aSheet = nsnull;

  // first, look if the attribute stylesheet is already there
  nsStyleSet *styleSet = aPresContext->StyleSet();
  NS_ASSERTION(styleSet, "no style set");

  nsAutoString title;
  for (PRInt32 i = styleSet->SheetCount(nsStyleSet::eAgentSheet) - 1;
       i >= 0; --i) {
    nsIStyleSheet *sheet = styleSet->StyleSheetAt(nsStyleSet::eAgentSheet, i);
    nsCOMPtr<nsICSSStyleSheet> cssSheet(do_QueryInterface(sheet));
    if (cssSheet) {
      cssSheet->GetTitle(title);
      if (title.Equals(NS_ConvertASCIItoUCS2(kTitle))) {
        *aSheet = sheet;
        NS_IF_ADDREF(*aSheet);
        return;
      }
    }
  }

  // then, create a new one if it isn't yet there
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:internal-mathml-attribute-stylesheet");
  if (!uri)
    return;
  nsCOMPtr<nsICSSStyleSheet> cssSheet(do_CreateInstance(kCSSStyleSheetCID));
  if (!cssSheet)
    return;
  cssSheet->SetURL(uri);
  nsCOMPtr<nsIDOMCSSStyleSheet> domSheet(do_QueryInterface(cssSheet));
  if (domSheet) {
    PRUint32 index;
    domSheet->InsertRule(NS_LITERAL_STRING("@namespace url(http://www.w3.org/1998/Math/MathML);"),
                                           0, &index);
  }
  cssSheet->SetTitle(NS_ConvertASCIItoUCS2(kTitle));

  // all done, no further activity from the net involved, so we better do this
  cssSheet->SetComplete();

  // insert the stylesheet into the styleset without notifying observers
  // XXX Should this be at a different level?
  styleSet->PrependStyleSheet(nsStyleSet::eAgentSheet, cssSheet);
  *aSheet = cssSheet;
  NS_ADDREF(*aSheet);
}

/* static */ PRInt32
nsMathMLFrame::MapAttributesIntoCSS(nsIPresContext* aPresContext,
                                    nsIContent*     aContent)
{
  // normal case, quick return if there are no attributes
  NS_ASSERTION(aContent, "null arg");
  PRUint32 attrCount = 0;
  if (aContent)
    attrCount = aContent->GetAttrCount();
  if (!attrCount)
    return 0;

  // need to initialize here -- i.e., after registering nsMathMLAtoms
  static const nsCSSMapping
  kCSSMappingTable[] = {
    {kMathMLversion2, nsMathMLAtoms::mathcolor_,      "color:"},
    {kMathMLversion1, nsMathMLAtoms::color_,          "color:"},
    {kMathMLversion2, nsMathMLAtoms::mathsize_,       "font-size:"},
    {kMathMLversion1, nsMathMLAtoms::fontsize_,       "font-size:"},
    {kMathMLversion1, nsMathMLAtoms::fontfamily_,     "font-family:"},
    {kMathMLversion2, nsMathMLAtoms::mathbackground_, "background-color:"},
    {kMathMLversion1, nsMathMLAtoms::background_,     "background-color:"},
    {0, nsnull, nsnull}
  };

  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIStyleSheet> sheet;
  nsCOMPtr<nsICSSStyleSheet> cssSheet;
  nsCOMPtr<nsIDOMCSSStyleSheet> domSheet;

  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> prefix;
  nsCOMPtr<nsIAtom> attrAtom;
  PRInt32 ruleCount = 0;
  for (PRUint32 i = 0; i < attrCount; ++i) {
    aContent->GetAttrNameAt(i, &nameSpaceID,
                            getter_AddRefs(attrAtom),
                            getter_AddRefs(prefix));

    // lookup the equivalent CSS property
    const nsCSSMapping* map = kCSSMappingTable;
    while (map->attrAtom && map->attrAtom != attrAtom)
      ++map;
    if (!map->attrAtom)
      continue;
    nsAutoString cssProperty(NS_ConvertASCIItoUCS2(map->cssProperty));

    nsAutoString attrValue;
    aContent->GetAttr(nameSpaceID, attrAtom, attrValue);
    if (attrValue.IsEmpty())
      continue;

    // don't add rules that are already in mathml.css
    // (this will also clean up whitespace before units - see bug 125303)
    if (attrAtom == nsMathMLAtoms::fontsize_ || attrAtom == nsMathMLAtoms::mathsize_) {
      nsCSSValue cssValue;
      nsAutoString numericValue(attrValue);
      if (!ParseNumericValue(numericValue, cssValue))
        continue;
      // on exit, ParseNumericValue also returns a nicer string
      // in which the whitespace before the unit is cleaned up 
      cssProperty.Append(numericValue);
    }
    else
      cssProperty.Append(attrValue);

    nsAutoString attrName;
    attrAtom->ToString(attrName);

    // make a style rule that maps to the equivalent CSS property
    nsAutoString cssRule;
    cssRule.Assign(NS_LITERAL_STRING("[")  + attrName +
                   NS_LITERAL_STRING("='") + attrValue +
                   NS_LITERAL_STRING("']{") + cssProperty + NS_LITERAL_STRING("}"));

    if (!sheet) {
      // first time... we do this to defer the lookup up to the
      // point where we encounter attributes that actually matter
      doc = aContent->GetDocument();
      if (!doc) 
        return 0;
      GetMathMLAttributeStyleSheet(aPresContext, getter_AddRefs(sheet));
      if (!sheet)
        return 0;
      // by construction, these cannot be null at this point
      cssSheet = do_QueryInterface(sheet);
      domSheet = do_QueryInterface(sheet);
      NS_ASSERTION(cssSheet && domSheet, "unexpected null pointers");
      // we will keep the sheet orphan as we populate it. This way,
      // observers of the document won't be notified and we avoid any troubles
      // that may come from reconstructing the frame tree. Our rules only need
      // a re-resolve of style data and a reflow, not a reconstruct-all...
      sheet->SetOwningDocument(nsnull);
    }

    // check for duplicate, if a similar rule is already there, don't bother to add another one
    // XXX bug 142648 - GetSourceSelectorText is in the format *[color=blue] (i.e., no quotes...) 
    // XXXrbs need to keep this in sync with the fix for bug 142648
    nsAutoString selector;
    selector.Assign(NS_LITERAL_STRING("*[") + attrName +
                    NS_LITERAL_STRING("=") + attrValue +
                    NS_LITERAL_STRING("]"));
    PRInt32 k, count;
    cssSheet->StyleRuleCount(count);
    for (k = 0; k < count; ++k) {
      nsAutoString tmpSelector;
      nsCOMPtr<nsICSSRule> tmpRule;
      cssSheet->GetStyleRuleAt(k, *getter_AddRefs(tmpRule));
      nsCOMPtr<nsICSSStyleRule> tmpStyleRule = do_QueryInterface(tmpRule);
      tmpStyleRule->GetSelectorText(tmpSelector);
      if (tmpSelector.Equals(selector)) {
        k = -1;
        break;
      }
    }
    if (k >= 0) {
      // insert the rule (note: when the sheet already has @namespace and
      // friends, insert after them, e.g., at the end, otherwise it won't work)
      // For MathML 2, insert at the end to give it precedence
      PRInt32 pos = (map->compatibility == kMathMLversion2) ? count : 1;
      PRUint32 index;
      domSheet->InsertRule(cssRule, pos, &index);
      ++ruleCount;
    }
  }
  // restore the sheet to its owner
  if (sheet) {
    sheet->SetOwningDocument(doc);
  }

  return ruleCount;
}

/* static */ PRInt32
nsMathMLFrame::MapAttributesIntoCSS(nsIPresContext* aPresContext,
                                    nsIFrame*       aFrame)
{
  PRInt32 ruleCount = MapAttributesIntoCSS(aPresContext, aFrame->GetContent());
  if (!ruleCount)
    return 0;

  // now, re-resolve the style contexts in our subtree
  nsFrameManager *fm = aPresContext->FrameManager();
  nsStyleChangeList changeList;
  fm->ComputeStyleChangeFor(aFrame, &changeList, NS_STYLE_HINT_NONE);
#ifdef DEBUG
  // Use the parent frame to make sure we catch in-flows and such
  nsIFrame* parentFrame = aFrame->GetParent();
  fm->DebugVerifyStyleTree(parentFrame ? parentFrame : aFrame);
#endif

  return ruleCount;
}
