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
#include "nsStyleUtil.h"
#include "nsCRT.h"
#include "nsStyleConsts.h"
#include "nsUnitConversion.h"

#include "nsGkAtoms.h"
#include "nsILinkHandler.h"
#include "nsILink.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsContentUtils.h"
#include "nsTextFormatter.h"

// XXX This is here because nsCachedStyleData is accessed outside of
// the content module; e.g., by nsCSSFrameConstructor.
#include "nsRuleNode.h"

nsCachedStyleData::StyleStructInfo
nsCachedStyleData::gInfo[] = {

#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
  { offsetof(nsCachedStyleData, mInheritedData), \
    offsetof(nsInheritedStyleData, m##name##Data), \
    PR_FALSE },
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
  { offsetof(nsCachedStyleData, mResetData), \
    offsetof(nsResetStyleData, m##name##Data), \
    PR_TRUE },

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

  { 0, 0, 0 }
};

#define POSITIVE_SCALE_FACTOR 1.10 /* 10% */
#define NEGATIVE_SCALE_FACTOR .90  /* 10% */


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

/*
 * Return the scaling percentage given a font scaler
 * Lifted from layutil.c
 */
float nsStyleUtil::GetScalingFactor(PRInt32 aScaler)
{
  double scale = 1.0;
  double mult;
  PRInt32 count;

  if(aScaler < 0)   {
    count = -aScaler;
    mult = NEGATIVE_SCALE_FACTOR;
  }
  else {
    count = aScaler;
    mult = POSITIVE_SCALE_FACTOR;
  }

  /* use the percentage scaling factor to the power of the pref */
  while(count--) {
    scale *= mult;
  }

  return (float)scale;
}


//------------------------------------------------------------------------------
// Font Algorithm Code
//------------------------------------------------------------------------------

nscoord
nsStyleUtil::CalcFontPointSize(PRInt32 aHTMLSize, PRInt32 aBasePointSize,
                               float aScalingFactor, nsPresContext* aPresContext,
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

  dFontSize *= aScalingFactor;

  if (1.0 < dFontSize) {
    return (nscoord)dFontSize;
  }
  return (nscoord)1;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

nscoord nsStyleUtil::FindNextSmallerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                             float aScalingFactor, nsPresContext* aPresContext,
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
  
  smallestIndexFontSize = CalcFontPointSize(indexMin, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
  largestIndexFontSize = CalcFontPointSize(indexMax, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType); 
  if (aFontSize > smallestIndexFontSize) {
    if (aFontSize < NSToCoordRound(float(largestIndexFontSize) * 1.5)) { // smaller will be in HTML table
      // find largest index smaller than current
      for (index = indexMax; index >= indexMin; index--) {
        indexFontSize = CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
        if (indexFontSize < aFontSize)
          break;
      } 
      // set up points beyond table for interpolation purposes
      if (indexFontSize == smallestIndexFontSize) {
        smallerIndexFontSize = indexFontSize - onePx;
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
      } else if (indexFontSize == largestIndexFontSize) {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
        largerIndexFontSize = NSToCoordRound(float(largestIndexFontSize) * 1.5);
      } else {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
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
    smallerSize = PR_MAX(aFontSize - onePx, onePx);
  }
  return smallerSize;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

nscoord nsStyleUtil::FindNextLargerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                            float aScalingFactor, nsPresContext* aPresContext,
                                            nsFontSizeType aFontSizeType)
{
  PRInt32 index;
  PRInt32 indexMin;
  PRInt32 indexMax;
  float relativePosition;
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
  
  smallestIndexFontSize = CalcFontPointSize(indexMin, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
  largestIndexFontSize = CalcFontPointSize(indexMax, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType); 
  if (aFontSize > (smallestIndexFontSize - onePx)) {
    if (aFontSize < largestIndexFontSize) { // larger will be in HTML table
      // find smallest index larger than current
      for (index = indexMin; index <= indexMax; index++) { 
        indexFontSize = CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
        if (indexFontSize > aFontSize)
          break;
      }
      // set up points beyond table for interpolation purposes
      if (indexFontSize == smallestIndexFontSize) {
        smallerIndexFontSize = indexFontSize - onePx;
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
      } else if (indexFontSize == largestIndexFontSize) {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
        largerIndexFontSize = NSToCoordRound(float(largestIndexFontSize) * 1.5);
      } else {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
      }
      // compute the relative position of the parent size between the two closest indexed sizes
      relativePosition = float(aFontSize - smallerIndexFontSize) / float(indexFontSize - smallerIndexFontSize);
      // set the new size to have the same relative position between the next largest two indexed sizes
      largerSize = indexFontSize + NSToCoordRound(relativePosition * (largerIndexFontSize - indexFontSize));      
    }
    else {  // larger than HTML table, increase by 50%
      largerSize = NSToCoordRound(float(aFontSize) * 1.5);
    }
  }
  else { // smaller than HTML table, increase by 1px
    largerSize = aFontSize + onePx; 
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
  PRBool  negativeStep = PRBool(50 < step);
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


/*static*/
PRBool nsStyleUtil::IsHTMLLink(nsIContent *aContent, nsIAtom *aTag, nsPresContext *aPresContext, nsLinkState *aState)
{
  NS_ASSERTION(aContent && aState, "null arg in IsHTMLLink");

  // check for:
  //  - HTML ANCHOR with valid HREF
  //  - HTML LINK with valid HREF
  //  - HTML AREA with valid HREF

  PRBool result = PR_FALSE;

  if ((aTag == nsGkAtoms::a) ||
      (aTag == nsGkAtoms::link) ||
      (aTag == nsGkAtoms::area)) {

    nsCOMPtr<nsILink> link( do_QueryInterface(aContent) );
    // In XML documents, this can be null.
    if (link) {
      nsLinkState linkState;
      link->GetLinkState(linkState);
      if (linkState == eLinkState_Unknown) {
        // if it is an anchor, area or link then check the href attribute
        // make sure this anchor has a link even if we are not testing state
        // if there is no link, then this anchor is not really a linkpseudo.
        // bug=23209

        nsCOMPtr<nsIURI> hrefURI;
        link->GetHrefURI(getter_AddRefs(hrefURI));

        if (hrefURI) {
          nsILinkHandler *linkHandler = aPresContext->GetLinkHandler();
          if (linkHandler) {
            linkHandler->GetLinkState(hrefURI, linkState);
          }
          else {
            // no link handler?  then all links are unvisited
            linkState = eLinkState_Unvisited;
          }
        } else {
          linkState = eLinkState_NotLink;
        }
        if (linkState != eLinkState_NotLink) {
          aPresContext->Document()->AddStyleRelevantLink(aContent, hrefURI);
        }
        link->SetLinkState(linkState);
      }
      if (linkState != eLinkState_NotLink) {
        *aState = linkState;
        result = PR_TRUE;
      }
    }
  }

  return result;
}

/*static*/
PRBool nsStyleUtil::IsLink(nsIContent    *aContent,
                           nsPresContext *aPresContext,
                           nsLinkState   *aState)
{
  // XXX PERF This function will cause serious performance problems on
  // pages with lots of XLinks.  We should be caching the visited
  // state of the XLinks.  Where???

  NS_ASSERTION(aContent && aState, "invalid call to IsLink with null content");

  PRBool rv = PR_FALSE;

  if (aContent && aState) {
    nsCOMPtr<nsIURI> absURI;
    if (aContent->IsLink(getter_AddRefs(absURI))) {
      nsILinkHandler *linkHandler = aPresContext->GetLinkHandler();
      if (linkHandler) {
        linkHandler->GetLinkState(absURI, *aState);
      }
      else {
        // no link handler?  then all links are unvisited
        *aState = eLinkState_Unvisited;
      }
      aPresContext->Document()->AddStyleRelevantLink(aContent, absURI);

      rv = PR_TRUE;
    }
  }
  return rv;
}

// Compare two language strings
PRBool nsStyleUtil::DashMatchCompare(const nsAString& aAttributeValue,
                                     const nsAString& aSelectorValue,
                                     const nsStringComparator& aComparator)
{
  PRBool result;
  PRUint32 selectorLen = aSelectorValue.Length();
  PRUint32 attributeLen = aAttributeValue.Length();
  if (selectorLen > attributeLen) {
    result = PR_FALSE;
  }
  else {
    nsAString::const_iterator iter;
    if (selectorLen != attributeLen &&
        *aAttributeValue.BeginReading(iter).advance(selectorLen) !=
            PRUnichar('-')) {
      // to match, the aAttributeValue must have a dash after the end of
      // the aSelectorValue's text (unless the aSelectorValue and the
      // aAttributeValue have the same text)
      result = PR_FALSE;
    }
    else {
      result = StringBeginsWith(aAttributeValue, aSelectorValue, aComparator);
    }
  }
  return result;
}

void nsStyleUtil::EscapeCSSString(const nsString& aString, nsAString& aReturn)
{
  aReturn.Truncate();

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
     nsTextFormatter::snprintf(buf, NS_ARRAY_LENGTH(buf), NS_LITERAL_STRING("\\%hX ").get(), *in);
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
