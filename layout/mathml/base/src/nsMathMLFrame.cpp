/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#include "nsINameSpaceManager.h"
#include "nsMathMLFrame.h"
#include "nsMathMLChar.h"

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
                                      nsIStyleContext* aParentStyleContext,
                                      nsMathMLChar*    aMathMLChar,
                                      PRBool           aIsMutableChar)
{
  nsIAtom* fontAtom = (aIsMutableChar) ?
    nsMathMLAtoms::fontstyle_stretchy :
    nsMathMLAtoms::fontstyle_anonymous; // savings
  nsCOMPtr<nsIStyleContext> newStyleContext;
  nsresult rv = aPresContext->ResolvePseudoStyleContextFor(aContent, fontAtom, 
                                             aParentStyleContext, PR_FALSE,
                                             getter_AddRefs(newStyleContext));
  if (NS_SUCCEEDED(rv) && newStyleContext)
    aMathMLChar->SetStyleContext(newStyleContext);
}

/* static */ PRBool
nsMathMLFrame::IsEmbellishOperator(nsIFrame* aFrame)
{
  nsEmbellishData embellishData;
  GetEmbellishDataFrom(aFrame, embellishData);
  return NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags);
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
    nsCOMPtr<nsIAtom> tag;
    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));
    content->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsMathMLAtoms::math) {
      const nsStyleDisplay* display;
      frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
      if (display->mDisplay == NS_STYLE_DISPLAY_BLOCK) {
        aPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
      }
      break;
    }
    frame->GetParent(&frame);
  }
}

/* static */ PRBool
nsMathMLFrame::HasNextSibling(nsIFrame* aFrame)
{
  if (aFrame) {
    nsIFrame* sibling;
    aFrame->GetNextSibling(&sibling);
    return sibling != nsnull;
  }
  return PR_FALSE;
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
      nsCOMPtr<nsIContent> mstyleContent;
      aMathMLmstyleFrame->GetContent(getter_AddRefs(mstyleContent));

      nsIFrame* mstyleParent;
      aMathMLmstyleFrame->GetParent(&mstyleParent);

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
      rv = GetAttribute(mstyleContent, mstyleParentData.mstyle, aAttributeAtom, aValue);
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
  PRUnichar minus = '-';
  nsBoundingMetrics bm;
  nsresult rv = aRenderingContext.GetBoundingMetrics(&minus, PRUint32(1), bm);
  if (NS_SUCCEEDED(rv)) {
    aAxisHeight = bm.ascent - (bm.ascent + bm.descent)/2;
  }
  if (NS_FAILED(rv) || aAxisHeight <= 0 || aAxisHeight >= xHeight) {
    // fall-back to the other version
    GetAxisHeight(aFontMetrics, aAxisHeight);
  }

#if 0
  nscoord oldAxis;
  GetAxisHeight(aFontMetrics, oldAxis);

  PRUnichar plus = '+';
  rv = aRenderingContext.GetBoundingMetrics(&plus, PRUint32(1), bm);
  nscoord plusAxis = bm.ascent - (bm.ascent + bm.descent)/2;;

  printf("xheight:%4d Axis:%4d oldAxis:%4d  plusAxis:%4d\n",
          xHeight, aAxisHeight, oldAxis, plusAxis);
#endif
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

  if (!stringLength) return PR_FALSE;

  nsAutoString number(aString);
  number.SetLength(0);

  nsAutoString unit(aString);
  unit.SetLength(0);

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

  // Convert number to floating point
  PRInt32 errorCode;
  float floatValue = number.ToFloat(&errorCode);
  if (NS_FAILED(errorCode)) return PR_FALSE;

  nsCSSUnit cssUnit;
  if (0 == unit.Length()) {
    cssUnit = eCSSUnit_Number; // no explicit unit, this is a number that will act as a multiplier
  }
  else if (unit.Equals(NS_LITERAL_STRING("%"))) {
    floatValue = floatValue / 100.0f;
    aCSSValue.SetPercentValue(floatValue);
    return PR_TRUE;
  }
  else if (unit.Equals(NS_LITERAL_STRING("em"))) cssUnit = eCSSUnit_EM;
  else if (unit.Equals(NS_LITERAL_STRING("ex"))) cssUnit = eCSSUnit_XHeight;
  else if (unit.Equals(NS_LITERAL_STRING("px"))) cssUnit = eCSSUnit_Pixel;
  else if (unit.Equals(NS_LITERAL_STRING("in"))) cssUnit = eCSSUnit_Inch;
  else if (unit.Equals(NS_LITERAL_STRING("cm"))) cssUnit = eCSSUnit_Centimeter;
  else if (unit.Equals(NS_LITERAL_STRING("mm"))) cssUnit = eCSSUnit_Millimeter;
  else if (unit.Equals(NS_LITERAL_STRING("pt"))) cssUnit = eCSSUnit_Point;
  else if (unit.Equals(NS_LITERAL_STRING("pc"))) cssUnit = eCSSUnit_Pica;
  else // unexpected unit
    return PR_FALSE;

  aCSSValue.SetFloatValue(floatValue, cssUnit);
  return PR_TRUE;
}

/* static */ nscoord
nsMathMLFrame::CalcLength(nsIPresContext*   aPresContext,
                          nsIStyleContext*  aStyleContext,
                          const nsCSSValue& aCSSValue)
{
  NS_ASSERTION(aCSSValue.IsLengthUnit(), "not a length unit");

  if (aCSSValue.IsFixedLengthUnit()) {
    return aCSSValue.GetLengthTwips();
  }

  nsCSSUnit unit = aCSSValue.GetUnit();

  if (eCSSUnit_Pixel == unit) {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    return NSFloatPixelsToTwips(aCSSValue.GetFloatValue(), p2t);
  }
  else if (eCSSUnit_EM == unit) {
    const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
      aStyleContext->GetStyleData(eStyleStruct_Font));
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)font->mFont.size);
  }
  else if (eCSSUnit_XHeight == unit) {
    nscoord xHeight;
    const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
      aStyleContext->GetStyleData(eStyleStruct_Font));
    nsCOMPtr<nsIFontMetrics> fm;
    aPresContext->GetMetricsFor(font->mFont, getter_AddRefs(fm));
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
  nsIAtom* namedspaceAtom;
  if (aString.Equals(NS_LITERAL_STRING("veryverythinmathspace"))) {
    i = 1;
    namedspaceAtom = nsMathMLAtoms::veryverythinmathspace_;
  }
  else if (aString.Equals(NS_LITERAL_STRING("verythinmathspace"))) {
    i = 2;
    namedspaceAtom = nsMathMLAtoms::verythinmathspace_;
  }
  else if (aString.Equals(NS_LITERAL_STRING("thinmathspace"))) {
    i = 3;
    namedspaceAtom = nsMathMLAtoms::thinmathspace_;
  }
  else if (aString.Equals(NS_LITERAL_STRING("mediummathspace"))) {
    i = 4;
    namedspaceAtom = nsMathMLAtoms::mediummathspace_;
  }
  else if (aString.Equals(NS_LITERAL_STRING("thickmathspace"))) {
    i = 5;
    namedspaceAtom = nsMathMLAtoms::thickmathspace_;
  }
  else if (aString.Equals(NS_LITERAL_STRING("verythickmathspace"))) {
    i = 6;
    namedspaceAtom = nsMathMLAtoms::verythickmathspace_;
  }
  else if (aString.Equals(NS_LITERAL_STRING("veryverythickmathspace"))) {
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
