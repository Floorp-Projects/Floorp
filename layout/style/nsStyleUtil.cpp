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

#include <math.h>

#include "mozilla/Util.h"

#include "nsStyleUtil.h"
#include "nsCRT.h"
#include "nsStyleConsts.h"

#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsContentUtils.h"
#include "nsTextFormatter.h"
#include "nsCSSProps.h"
#include "nsRuleNode.h"

using namespace mozilla;

//------------------------------------------------------------------------------
// Font Algorithm Code
//------------------------------------------------------------------------------

nscoord
nsStyleUtil::CalcFontPointSize(PRInt32 aHTMLSize, PRInt32 aBasePointSize,
                               nsPresContext* aPresContext,
                               nsFontSizeType aFontSizeType)
{
#define sFontSizeTableMin  9 
#define sFontSizeTableMax 16 

// This table seems to be the one used by MacIE5. We hope its adoption in Mozilla
// and eventually in WinIE5.5 will help to establish a standard rendering across
// platforms and browsers. For now, it is used only in Strict mode. More can be read
// in the document written by Todd Farhner at:
// http://style.verso.com/font_size_intervals/altintervals.html
//
  static PRInt32 sStrictFontSizeTable[sFontSizeTableMax - sFontSizeTableMin + 1][8] =
  {
      { 9,    9,     9,     9,    11,    14,    18,    27},
      { 9,    9,     9,    10,    12,    15,    20,    30},
      { 9,    9,    10,    11,    13,    17,    22,    33},
      { 9,    9,    10,    12,    14,    18,    24,    36},
      { 9,   10,    12,    13,    16,    20,    26,    39},
      { 9,   10,    12,    14,    17,    21,    28,    42},
      { 9,   10,    13,    15,    18,    23,    30,    45},
      { 9,   10,    13,    16,    18,    24,    32,    48}
  };
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref
//
//------------------------------------------------------------
//
// This table gives us compatibility with WinNav4 for the default fonts only.
// In WinNav4, the default fonts were:
//
//     Times/12pt ==   Times/16px at 96ppi
//   Courier/10pt == Courier/13px at 96ppi
//
// The 2 lines below marked "anchored" have the exact pixel sizes used by
// WinNav4 for Times/12pt and Courier/10pt at 96ppi. As you can see, the
// HTML size 3 (user pref) for those 2 anchored lines is 13px and 16px.
//
// All values other than the anchored values were filled in by hand, never
// going below 9px, and maintaining a "diagonal" relationship. See for
// example the 13s -- they follow a diagonal line through the table.
//
  static PRInt32 sQuirksFontSizeTable[sFontSizeTableMax - sFontSizeTableMin + 1][8] =
  {
      { 9,    9,     9,     9,    11,    14,    18,    28 },
      { 9,    9,     9,    10,    12,    15,    20,    31 },
      { 9,    9,     9,    11,    13,    17,    22,    34 },
      { 9,    9,    10,    12,    14,    18,    24,    37 },
      { 9,    9,    10,    13,    16,    20,    26,    40 }, // anchored (13)
      { 9,    9,    11,    14,    17,    21,    28,    42 },
      { 9,   10,    12,    15,    17,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // anchored (16)
  };
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

#if 0
//
// These are the exact pixel values used by WinIE5 at 96ppi.
//
      { ?,    8,    11,    12,    13,    16,    21,    32 }, // smallest
      { ?,    9,    12,    13,    16,    21,    27,    40 }, // smaller
      { ?,   10,    13,    16,    18,    24,    32,    48 }, // medium
      { ?,   13,    16,    19,    21,    27,    37,    ?? }, // larger
      { ?,   16,    19,    21,    24,    32,    43,    ?? }  // largest
//
// HTML       1      2      3      4      5      6      7
// CSS  ?     ?      ?      ?      ?      ?      ?      ?
//
// (CSS not tested yet.)
//
#endif

  static PRInt32 sFontSizeFactors[8] = { 60,75,89,100,120,150,200,300 };

  static PRInt32 sCSSColumns[7]  = {0, 1, 2, 3, 4, 5, 6}; // xxs...xxl
  static PRInt32 sHTMLColumns[7] = {1, 2, 3, 4, 5, 6, 7}; // 1...7

  double dFontSize;

  if (aFontSizeType == eFontSize_HTML) {
    aHTMLSize--;    // input as 1-7
  }

  if (aHTMLSize < 0)
    aHTMLSize = 0;
  else if (aHTMLSize > 6)
    aHTMLSize = 6;

  PRInt32* column;
  switch (aFontSizeType)
  {
    case eFontSize_HTML: column = sHTMLColumns; break;
    case eFontSize_CSS:  column = sCSSColumns;  break;
  }

  // Make special call specifically for fonts (needed PrintPreview)
  PRInt32 fontSize = nsPresContext::AppUnitsToIntCSSPixels(aBasePointSize);

  if ((fontSize >= sFontSizeTableMin) && (fontSize <= sFontSizeTableMax))
  {
    PRInt32 row = fontSize - sFontSizeTableMin;

    if (aPresContext->CompatibilityMode() == eCompatibility_NavQuirks) {
      dFontSize = nsPresContext::CSSPixelsToAppUnits(sQuirksFontSizeTable[row][column[aHTMLSize]]);
    } else {
      dFontSize = nsPresContext::CSSPixelsToAppUnits(sStrictFontSizeTable[row][column[aHTMLSize]]);
    }
  }
  else
  {
    PRInt32 factor = sFontSizeFactors[column[aHTMLSize]];
    dFontSize = (factor * aBasePointSize) / 100;
  }


  if (1.0 < dFontSize) {
    return (nscoord)dFontSize;
  }
  return (nscoord)1;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

nscoord nsStyleUtil::FindNextSmallerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                             nsPresContext* aPresContext,
                                             nsFontSizeType aFontSizeType)
{
  PRInt32 index;
  PRInt32 indexMin;
  PRInt32 indexMax;
  float relativePosition;
  nscoord smallerSize;
  nscoord indexFontSize = aFontSize; // XXX initialize to quell a spurious gcc3.2 warning
  nscoord smallestIndexFontSize;
  nscoord largestIndexFontSize;
  nscoord smallerIndexFontSize;
  nscoord largerIndexFontSize;

  nscoord onePx = nsPresContext::CSSPixelsToAppUnits(1);

  if (aFontSizeType == eFontSize_HTML) {
    indexMin = 1;
    indexMax = 7;
  } else {
    indexMin = 0;
    indexMax = 6;
  }
  
  smallestIndexFontSize = CalcFontPointSize(indexMin, aBasePointSize, aPresContext, aFontSizeType);
  largestIndexFontSize = CalcFontPointSize(indexMax, aBasePointSize, aPresContext, aFontSizeType); 
  if (aFontSize > smallestIndexFontSize) {
    if (aFontSize < NSToCoordRound(float(largestIndexFontSize) * 1.5)) { // smaller will be in HTML table
      // find largest index smaller than current
      for (index = indexMax; index >= indexMin; index--) {
        indexFontSize = CalcFontPointSize(index, aBasePointSize, aPresContext, aFontSizeType);
        if (indexFontSize < aFontSize)
          break;
      } 
      // set up points beyond table for interpolation purposes
      if (indexFontSize == smallestIndexFontSize) {
        smallerIndexFontSize = indexFontSize - onePx;
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      } else if (indexFontSize == largestIndexFontSize) {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = NSToCoordRound(float(largestIndexFontSize) * 1.5);
      } else {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      }
      // compute the relative position of the parent size between the two closest indexed sizes
      relativePosition = float(aFontSize - indexFontSize) / float(largerIndexFontSize - indexFontSize);            
      // set the new size to have the same relative position between the next smallest two indexed sizes
      smallerSize = smallerIndexFontSize + NSToCoordRound(relativePosition * (indexFontSize - smallerIndexFontSize));      
    }
    else {  // larger than HTML table, drop by 33%
      smallerSize = NSToCoordRound(float(aFontSize) / 1.5);
    }
  }
  else { // smaller than HTML table, drop by 1px
    smallerSize = NS_MAX(aFontSize - onePx, onePx);
  }
  return smallerSize;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

nscoord nsStyleUtil::FindNextLargerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                            nsPresContext* aPresContext,
                                            nsFontSizeType aFontSizeType)
{
  PRInt32 index;
  PRInt32 indexMin;
  PRInt32 indexMax;
  float relativePosition;
  nscoord adjustment;
  nscoord largerSize;
  nscoord indexFontSize = aFontSize; // XXX initialize to quell a spurious gcc3.2 warning
  nscoord smallestIndexFontSize;
  nscoord largestIndexFontSize;
  nscoord smallerIndexFontSize;
  nscoord largerIndexFontSize;

  nscoord onePx = nsPresContext::CSSPixelsToAppUnits(1);

  if (aFontSizeType == eFontSize_HTML) {
    indexMin = 1;
    indexMax = 7;
  } else {
    indexMin = 0;
    indexMax = 6;
  }
  
  smallestIndexFontSize = CalcFontPointSize(indexMin, aBasePointSize, aPresContext, aFontSizeType);
  largestIndexFontSize = CalcFontPointSize(indexMax, aBasePointSize, aPresContext, aFontSizeType); 
  if (aFontSize > (smallestIndexFontSize - onePx)) {
    if (aFontSize < largestIndexFontSize) { // larger will be in HTML table
      // find smallest index larger than current
      for (index = indexMin; index <= indexMax; index++) { 
        indexFontSize = CalcFontPointSize(index, aBasePointSize, aPresContext, aFontSizeType);
        if (indexFontSize > aFontSize)
          break;
      }
      // set up points beyond table for interpolation purposes
      if (indexFontSize == smallestIndexFontSize) {
        smallerIndexFontSize = indexFontSize - onePx;
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      } else if (indexFontSize == largestIndexFontSize) {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = NSCoordSaturatingMultiply(largestIndexFontSize, 1.5);
      } else {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      }
      // compute the relative position of the parent size between the two closest indexed sizes
      relativePosition = float(aFontSize - smallerIndexFontSize) / float(indexFontSize - smallerIndexFontSize);
      // set the new size to have the same relative position between the next largest two indexed sizes
      adjustment = NSCoordSaturatingNonnegativeMultiply(largerIndexFontSize - indexFontSize, relativePosition);
      largerSize = NSCoordSaturatingAdd(indexFontSize, adjustment);
    }
    else {  // larger than HTML table, increase by 50%
      largerSize = NSCoordSaturatingMultiply(aFontSize, 1.5);
    }
  }
  else { // smaller than HTML table, increase by 1px
    largerSize = NSCoordSaturatingAdd(aFontSize, onePx);
  }
  return largerSize;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

PRInt32 
nsStyleUtil::ConstrainFontWeight(PRInt32 aWeight)
{
  aWeight = ((aWeight < 100) ? 100 : ((aWeight > 900) ? 900 : aWeight));
  PRInt32 base = ((aWeight / 100) * 100);
  PRInt32 step = (aWeight % 100);
  bool    negativeStep = bool(50 < step);
  PRInt32 maxStep;
  if (negativeStep) {
    step = 100 - step;
    maxStep = (base / 100);
    base += 100;
  }
  else {
    maxStep = ((900 - base) / 100);
  }
  if (maxStep < step) {
    step = maxStep;
  }
  return (base + ((negativeStep) ? -step : step));
}

// Compare two language strings
bool nsStyleUtil::DashMatchCompare(const nsAString& aAttributeValue,
                                     const nsAString& aSelectorValue,
                                     const nsStringComparator& aComparator)
{
  bool result;
  PRUint32 selectorLen = aSelectorValue.Length();
  PRUint32 attributeLen = aAttributeValue.Length();
  if (selectorLen > attributeLen) {
    result = false;
  }
  else {
    nsAString::const_iterator iter;
    if (selectorLen != attributeLen &&
        *aAttributeValue.BeginReading(iter).advance(selectorLen) !=
            PRUnichar('-')) {
      // to match, the aAttributeValue must have a dash after the end of
      // the aSelectorValue's text (unless the aSelectorValue and the
      // aAttributeValue have the same text)
      result = false;
    }
    else {
      result = StringBeginsWith(aAttributeValue, aSelectorValue, aComparator);
    }
  }
  return result;
}

void nsStyleUtil::AppendEscapedCSSString(const nsString& aString,
                                         nsAString& aReturn)
{
  aReturn.Append(PRUnichar('"'));

  const nsString::char_type* in = aString.get();
  const nsString::char_type* const end = in + aString.Length();
  for (; in != end; in++)
  {
    if (*in < 0x20)
    {
     // Escape all characters below 0x20 numerically.
   
     /*
      This is the buffer into which snprintf should write. As the hex. value is,
      for numbers below 0x20, max. 2 characters long, we don't need more than 5
      characters ("\XX "+NUL).
     */
     PRUnichar buf[5];
     nsTextFormatter::snprintf(buf, ArrayLength(buf), NS_LITERAL_STRING("\\%hX ").get(), *in);
     aReturn.Append(buf);
   
    } else switch (*in) {
      // Special characters which should be escaped: Quotes and backslash
      case '\\':
      case '\"':
      case '\'':
       aReturn.Append(PRUnichar('\\'));
      // And now, after the eventual escaping character, the actual one.
      default:
       aReturn.Append(PRUnichar(*in));
    }
  }

  aReturn.Append(PRUnichar('"'));
}

/* static */ void
nsStyleUtil::AppendEscapedCSSIdent(const nsString& aIdent, nsAString& aReturn)
{
  // The relevant parts of the CSS grammar are:
  //   ident    [-]?{nmstart}{nmchar}*
  //   nmstart  [_a-z]|{nonascii}|{escape}
  //   nmchar   [_a-z0-9-]|{nonascii}|{escape}
  //   nonascii [^\0-\177]
  //   escape   {unicode}|\\[^\n\r\f0-9a-f]
  //   unicode  \\[0-9a-f]{1,6}(\r\n|[ \n\r\t\f])?
  // from http://www.w3.org/TR/CSS21/syndata.html#tokenization

  const nsString::char_type* in = aIdent.get();
  const nsString::char_type* const end = in + aIdent.Length();

  // Deal with the leading dash separately so we don't need to
  // unnecessarily escape digits.
  if (in != end && *in == '-') {
    aReturn.Append(PRUnichar('-'));
    ++in;
  }

  bool first = true;
  for (; in != end; ++in, first = false)
  {
    if (*in < 0x20 || (first && '0' <= *in && *in <= '9'))
    {
      // Escape all characters below 0x20, and digits at the start
      // (including after a dash), numerically.  If we didn't escape
      // digits numerically, they'd get interpreted as a numeric escape
      // for the wrong character.

      /*
       This is the buffer into which snprintf should write. As the hex.
       value is, for numbers below 0x7F, max. 2 characters long, we
       don't need more than 5 characters ("\XX "+NUL).
      */
      PRUnichar buf[5];
      nsTextFormatter::snprintf(buf, ArrayLength(buf),
                                NS_LITERAL_STRING("\\%hX ").get(), *in);
      aReturn.Append(buf);
    } else {
      PRUnichar ch = *in;
      if (!((ch == PRUnichar('_')) ||
            (PRUnichar('A') <= ch && ch <= PRUnichar('Z')) ||
            (PRUnichar('a') <= ch && ch <= PRUnichar('z')) ||
            PRUnichar(0x80) <= ch ||
            (!first && ch == PRUnichar('-')) ||
            (PRUnichar('0') <= ch && ch <= PRUnichar('9')))) {
        // Character needs to be escaped
        aReturn.Append(PRUnichar('\\'));
      }
      aReturn.Append(ch);
    }
  }
}

/* static */ void
nsStyleUtil::AppendBitmaskCSSValue(nsCSSProperty aProperty,
                                   PRInt32 aMaskedValue,
                                   PRInt32 aFirstMask,
                                   PRInt32 aLastMask,
                                   nsAString& aResult)
{
  for (PRInt32 mask = aFirstMask; mask <= aLastMask; mask <<= 1) {
    if (mask & aMaskedValue) {
      AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, mask),
                         aResult);
      aMaskedValue &= ~mask;
      if (aMaskedValue) { // more left
        aResult.Append(PRUnichar(' '));
      }
    }
  }
  NS_ABORT_IF_FALSE(aMaskedValue == 0, "unexpected bit remaining in bitfield");
}

/* static */ void
nsStyleUtil::AppendFontFeatureSettings(const nsTArray<gfxFontFeature>& aFeatures,
                                       nsAString& aResult)
{
  for (PRUint32 i = 0, numFeat = aFeatures.Length(); i < numFeat; i++) {
    const gfxFontFeature& feat = aFeatures[i];

    if (i != 0) {
        aResult.AppendLiteral(", ");
    }

    // output tag
    char tag[7];
    tag[0] = '"';
    tag[1] = (feat.mTag >> 24) & 0xff;
    tag[2] = (feat.mTag >> 16) & 0xff;
    tag[3] = (feat.mTag >> 8) & 0xff;
    tag[4] = feat.mTag & 0xff;
    tag[5] = '"';
    tag[6] = 0;
    aResult.AppendASCII(tag);

    // output value, if necessary
    if (feat.mValue == 0) {
      // 0 ==> off
      aResult.AppendLiteral(" off");
    } else if (feat.mValue > 1) {
      aResult.AppendLiteral(" ");
      aResult.AppendInt(feat.mValue);
    }
    // else, omit value if 1, implied by default
  }
}

/* static */ void
nsStyleUtil::AppendFontFeatureSettings(const nsCSSValue& aSrc,
                                       nsAString& aResult)
{
  nsCSSUnit unit = aSrc.GetUnit();

  if (unit == eCSSUnit_Normal) {
    aResult.AppendLiteral("normal");
    return;
  }

  NS_PRECONDITION(unit == eCSSUnit_PairList || unit == eCSSUnit_PairListDep,
                  "improper value unit for font-feature-settings:");

  nsTArray<gfxFontFeature> featureSettings;
  nsRuleNode::ComputeFontFeatures(aSrc.GetPairListValue(), featureSettings);
  AppendFontFeatureSettings(featureSettings, aResult);
}

/* static */ float
nsStyleUtil::ColorComponentToFloat(PRUint8 aAlpha)
{
  // Alpha values are expressed as decimals, so we should convert
  // back, using as few decimal places as possible for
  // round-tripping.
  // First try two decimal places:
  float rounded = NS_roundf(float(aAlpha) * 100.0f / 255.0f) / 100.0f;
  if (FloatToColorComponent(rounded) != aAlpha) {
    // Use three decimal places.
    rounded = NS_roundf(float(aAlpha) * 1000.0f / 255.0f) / 1000.0f;
  }
  return rounded;
}

/* static */ bool
nsStyleUtil::IsSignificantChild(nsIContent* aChild, bool aTextIsSignificant,
                                bool aWhitespaceIsSignificant)
{
  NS_ASSERTION(!aWhitespaceIsSignificant || aTextIsSignificant,
               "Nonsensical arguments");

  bool isText = aChild->IsNodeOfType(nsINode::eTEXT);

  if (!isText && !aChild->IsNodeOfType(nsINode::eCOMMENT) &&
      !aChild->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
    return true;
  }

  return aTextIsSignificant && isText && aChild->TextLength() != 0 &&
         (aWhitespaceIsSignificant ||
          !aChild->TextIsOnlyWhitespace());
}

