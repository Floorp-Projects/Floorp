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

#include <math.h>
#include "nsStyleUtil.h"
#include "nsCRT.h"
#include "nsStyleConsts.h"
#include "nsUnitConversion.h"

#include "nsHTMLAtoms.h"
#include "nsILinkHandler.h"
#include "nsILink.h"
#include "nsIXMLContent.h"
#include "nsIHTMLContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsReadableUtils.h"

// XXX This is here because nsCachedStyleData is accessed outside of
// the content module; e.g., by nsCSSFrameConstructor.
#include "nsRuleNode.h"

nsCachedStyleData::StyleStructInfo
nsCachedStyleData::gInfo[] = {
  // Note that these must line up _exactly_ with the numeric values of
  // the nsStyleStructID enum.
  { 0, 0, 0 },

  /* eStyleStruct_Font */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mFontData),
    PR_FALSE
  },
  /* eStyleStruct_Color */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mColorData),
    PR_FALSE
  },
  /* eStyleStruct_Background */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mBackgroundData),
    PR_TRUE
  },
  /* eStyleStruct_List */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mListData),
    PR_FALSE
  },
  /* eStyleStruct_Position */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mPositionData),
    PR_TRUE
  },
  /* eStyleStruct_Text */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mTextData),
    PR_FALSE
  },
  /* eStyleStruct_TextReset */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mTextData),
    PR_TRUE
  },
  /* eStyleStruct_Display */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mDisplayData),
    PR_TRUE
  },
  /* eStyleStruct_Visibility */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mVisibilityData),
    PR_FALSE
  },
  /* eStyleStruct_Content */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mContentData),
    PR_TRUE
  },
  /* eStyleStruct_Quotes */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mQuotesData),
    PR_FALSE
  },
  /* eStyleStruct_UserInterface */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mUIData),
    PR_FALSE
  },
  /* eStyleStruct_UIReset */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mUIData),
    PR_TRUE
  },
  /* eStyleStruct_Table */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mTableData),
    PR_TRUE
  },
  /* eStyleStruct_TableBorder */
  { offsetof(nsCachedStyleData, mInheritedData),
    offsetof(nsInheritedStyleData, mTableData),
    PR_FALSE
  },
  /* eStyleStruct_Margin */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mMarginData),
    PR_TRUE
  },
  /* eStyleStruct_Padding */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mPaddingData),
    PR_TRUE
  },
  /* eStyleStruct_Border */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mBorderData),
    PR_TRUE
  },
  /* eStyleStruct_Outline */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mOutlineData),
    PR_TRUE
  },
#ifdef INCLUDE_XUL
  /* eStyleStruct_XUL */
  { offsetof(nsCachedStyleData, mResetData),
    offsetof(nsResetStyleData, mXULData),
    PR_TRUE
  },
#endif

  { 0, 0, 0 }
};

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define POSITIVE_SCALE_FACTOR 1.10 /* 10% */
#define NEGATIVE_SCALE_FACTOR .90  /* 10% */

#if DEBUG
#define DUMP_FONT_SIZES 0
#endif


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
//
//------------------------------------------------------------------------------

static PRBool gNavAlgorithmPref = PR_FALSE;

static int PR_CALLBACK NavAlgorithmPrefChangedCallback(const char * name, void * closure)
{
	nsresult rv;
	nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
	if (NS_SUCCEEDED(rv) && prefs) {
		prefs->GetBoolPref(name, &gNavAlgorithmPref);
	}
	return 0;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

static PRBool UseNewFontAlgorithm()
{
	static PRBool once = PR_TRUE;

	if (once)
	{
		once = PR_FALSE;

		nsresult rv;
		nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
		if (NS_SUCCEEDED(rv) && prefs) {
			prefs->GetBoolPref("font.size.nav4algorithm", &gNavAlgorithmPref);
			prefs->RegisterCallback("font.size.nav4algorithm", NavAlgorithmPrefChangedCallback, NULL);
		}
	}
	return (gNavAlgorithmPref ? PR_FALSE : PR_TRUE);
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

/*
 * Lifted from winfe/cxdc.cpp
 */
static nscoord OldCalcFontPointSize(PRInt32 aHTMLSize, PRInt32 aBasePointSize,
                                       float aScalingFactor)
{ // lifted directly from Nav 5.0 code to replicate rounding errors
  double dFontSize;

	switch(aHTMLSize)	{
	case 1:
		dFontSize = 7 * aBasePointSize / 10;
		break;
	case 2:
		dFontSize = 85 * aBasePointSize / 100;
		break;
	case 3:
		dFontSize = aBasePointSize;
    break;
	case 4:
		dFontSize = 12 * aBasePointSize / 10;
		break;
	case 5:
		dFontSize = 3 * aBasePointSize / 2;
		break;
	case 6:
		dFontSize = 2 * aBasePointSize;
		break;
	case 7:
		dFontSize = 3 * aBasePointSize;
		break;
	default:
    if (aHTMLSize < 1) {
      dFontSize = (7 * aBasePointSize / 10) * pow(1.1, aHTMLSize - 1);
    }
    else {  // aHTMLSize > 7
      dFontSize = (3 * aBasePointSize) * pow(1.2, aHTMLSize - 7);
    }
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

static nscoord NewCalcFontPointSize(PRInt32 aHTMLSize, PRInt32 aBasePointSize,
                                       float aScalingFactor, nsIPresContext* aPresContext,
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

  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  PRInt32 fontSize = NSTwipsToIntPixels(aBasePointSize, t2p);

  if ((fontSize >= sFontSizeTableMin) && (fontSize <= sFontSizeTableMax))
  {
    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);

    PRInt32 row = fontSize - sFontSizeTableMin;

		nsCompatibility mode;
	  aPresContext->GetCompatibilityMode(&mode);
	  if (mode == eCompatibility_NavQuirks) {
	    dFontSize = NSIntPixelsToTwips(sQuirksFontSizeTable[row][column[aHTMLSize]], p2t);
	  } else {
	    dFontSize = NSIntPixelsToTwips(sStrictFontSizeTable[row][column[aHTMLSize]], p2t);
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

nscoord nsStyleUtil::CalcFontPointSize(PRInt32 aHTMLSize, PRInt32 aBasePointSize,
                                       float aScalingFactor, nsIPresContext* aPresContext,
                                       nsFontSizeType aFontSizeType)
{
#if DUMP_FONT_SIZES
	extern void DumpFontSizes(nsIPresContext* aPresContext);
	DumpFontSizes(aPresContext);
#endif
	if (UseNewFontAlgorithm())
		return NewCalcFontPointSize(aHTMLSize, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType);
	else
		return OldCalcFontPointSize(aHTMLSize, aBasePointSize, aScalingFactor);
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

PRInt32 nsStyleUtil::FindNextSmallerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                             float aScalingFactor, nsIPresContext* aPresContext,
                                             nsFontSizeType aFontSizeType)
{
  PRInt32 index;
  PRInt32 indexMin;
  PRInt32 indexMax;
  PRInt32 fontSize = NSTwipsToFloorIntPoints(aFontSize);

	if (aFontSizeType == eFontSize_HTML) {
		indexMin = 1;
		indexMax = 7;
	} else {
		indexMin = 0;
		indexMax = 6;
	}

  if (NSTwipsToFloorIntPoints(CalcFontPointSize(indexMin, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType)) < fontSize) {
    if (fontSize <= NSTwipsToFloorIntPoints(CalcFontPointSize(indexMax, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType))) { // in HTML table
      for (index = indexMax; index > indexMin; index--)
        if (fontSize > NSTwipsToFloorIntPoints(CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType)))
          break;
    }
    else {  // larger than HTML table
      return indexMax;
//    for (index = 8; ; index++)
//      if (fontSize < NSTwipsToFloorIntPoints(CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext))) {
//        index--;
//        break;
//      }
    }
  }
  else { // smaller than HTML table
    return indexMin;
//  for (index = 0; -25<index ; index--) //prevent infinite loop (bug 17045)
//    if (fontSize > NSTwipsToFloorIntPoints(CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext))) {
//      break;
//    }
  }
  return index;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

PRInt32 nsStyleUtil::FindNextLargerFontSize(nscoord aFontSize, PRInt32 aBasePointSize, 
                                            float aScalingFactor, nsIPresContext* aPresContext,
                                            nsFontSizeType aFontSizeType)
{
  PRInt32 index;
  PRInt32 indexMin;
  PRInt32 indexMax;
  PRInt32 fontSize = NSTwipsToFloorIntPoints(aFontSize);

	if (aFontSizeType == eFontSize_HTML) {
		indexMin = 1;
		indexMax = 7;
	} else {
		indexMin = 0;
		indexMax = 6;
	}

  if (NSTwipsToFloorIntPoints(CalcFontPointSize(indexMin, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType)) <= fontSize) {
    if (fontSize < NSTwipsToFloorIntPoints(CalcFontPointSize(indexMax, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType))) { // in HTML table
      for (index = indexMin; index < indexMax; index++)
        if (fontSize < NSTwipsToFloorIntPoints(CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext, aFontSizeType)))
          break;
    }
    else {  // larger than HTML table
			return indexMax;
//    for (index = 8; ; index++)
//      if (fontSize < NSTwipsToFloorIntPoints(CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext)))
//        break;
    }
  }
  else {  // smaller than HTML table
    return indexMin;
//  for (index = 0; -25<index ; index--) //prevent infinite loop (bug 17045)
//    if (fontSize > NSTwipsToFloorIntPoints(CalcFontPointSize(index, aBasePointSize, aScalingFactor, aPresContext))) {
//      index++;
//      break;
//    }
  }
  return index;
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


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

const nsStyleBackground* nsStyleUtil::FindNonTransparentBackground(nsIStyleContext* aContext,
                                                                   PRBool aStartAtParent /*= PR_FALSE*/)
{
  const nsStyleBackground* result = nsnull;
  nsIStyleContext*    context;
  if (aStartAtParent) {
    context = aContext->GetParent();  // balance ending release
  } else {
    context = aContext;
    NS_IF_ADDREF(context);  // balance ending release
  }
  NS_ASSERTION( context != nsnull, "Cannot find NonTransparentBackground in a null context" );
  
  while (nsnull != context) {
    result = (const nsStyleBackground*)context->GetStyleData(eStyleStruct_Background);

    if (0 == (result->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
      break;
    }
    else {
      nsIStyleContext* last = context;
      context = context->GetParent();
      NS_RELEASE(last);
    }
  }
  NS_IF_RELEASE(context);
  return result;
}

/*static*/
PRBool nsStyleUtil::IsHTMLLink(nsIContent *aContent, nsIAtom *aTag, nsIPresContext *aPresContext, nsLinkState *aState)
{
  NS_ASSERTION(aContent && aState, "null arg in IsHTMLLink");

  // check for:
  //  - HTML ANCHOR with valid HREF
  //  - HTML LINK with valid HREF
  //  - HTML AREA with valid HREF

  PRBool result = PR_FALSE;

  if ((aTag == nsHTMLAtoms::a) ||
      (aTag == nsHTMLAtoms::link) ||
      (aTag == nsHTMLAtoms::area)) {

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

        char* href;
        link->GetHrefCString(href);

        if (href) {
          nsILinkHandler *linkHandler = nsnull;
          aPresContext->GetLinkHandler(&linkHandler);
          if (linkHandler) {
            linkHandler->GetLinkState(href, linkState);
            NS_RELEASE(linkHandler);
          }
          else {
            // no link handler?  then all links are unvisited
            linkState = eLinkState_Unvisited;
          }
          nsCRT::free(href);
        } else {
          linkState = eLinkState_NotLink;
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
PRBool nsStyleUtil::IsSimpleXlink(nsIContent *aContent, nsIPresContext *aPresContext, nsLinkState *aState)
{
  // XXX PERF This function will cause serious performance problems on
  // pages with lots of XLinks.  We should be caching the visited
  // state of the XLinks.  Where???

  NS_ASSERTION(aContent && aState, "invalid call to IsXlink with null content");

  PRBool rv = PR_FALSE;

  if (aContent && aState) {
    // first see if we have an XML element
    nsCOMPtr<nsIXMLContent> xml(do_QueryInterface(aContent));
    if (xml) {
      // see if it is type=simple (we don't deal with other types)
      nsAutoString val;
      aContent->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::type, val);
      if (val == NS_LITERAL_STRING("simple")) {
        // see if there is an xlink namespace'd href attribute: 
        // - get it if there is, if not no big deal, it is not required for xlinks
        // is it bad to re-use val here?
        aContent->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::href, val);

        // It's an XLink. Resolve it relative to its document.
        nsCOMPtr<nsIURI> baseURI;
        nsCOMPtr<nsIHTMLContent> htmlContent = do_QueryInterface(aContent);
        if (htmlContent) {
          // XXX why do this? will nsIHTMLContent's
          // GetBaseURL() may return something different
          // than the URL of the document it lives in?
          htmlContent->GetBaseURL(*getter_AddRefs(baseURI));
        }
        else {
          nsCOMPtr<nsIDocument> doc;
          aContent->GetDocument(*getter_AddRefs(doc));
          if (doc) {
            doc->GetBaseURL(*getter_AddRefs(baseURI));
          }
        }

        // convert here, rather than twice in NS_MakeAbsoluteURI and
        // back again
        char * href = ToNewCString(val);
        char * absHREF = nsnull;
        (void) NS_MakeAbsoluteURI(&absHREF, href, baseURI);
        nsCRT::free(href);

        nsILinkHandler *linkHandler = nsnull;
        aPresContext->GetLinkHandler(&linkHandler);
        if (linkHandler) {
          linkHandler->GetLinkState(absHREF, *aState);
          NS_RELEASE(linkHandler);
        }
        else {
          // no link handler?  then all links are unvisited
          *aState = eLinkState_Unvisited;
        }
        nsCRT::free(absHREF);

        rv = PR_TRUE;
      }
    }
  }
  return rv;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
#if DUMP_FONT_SIZES
#include "nsIDeviceContext.h"
PRInt32 RoundSize(nscoord aVal, nsIPresContext* aPresContext, bool aWinRounding)
{

	PRInt32 lfHeight;
	nsIDeviceContext* dc;
	aPresContext->GetDeviceContext(&dc);

  float app2dev, app2twip, scale;
  dc->GetAppUnitsToDevUnits(app2dev);

	if (aWinRounding)
	{
	  dc->GetDevUnitsToTwips(app2twip);
	  dc->GetCanonicalPixelScale(scale);
	  app2twip *= app2dev * scale;

	  // This interesting bit of code rounds the font size off to the floor point
	  // value. This is necessary for proper font scaling under windows.
	  PRInt32 sizePoints = NSTwipsToFloorIntPoints(nscoord(aVal*app2twip));
	  float rounded = ((float)NSIntPointsToTwips(sizePoints)) / app2twip;

	  // round font size off to floor point size to be windows compatible
	  // this is proper (windows) rounding
	//   lfHeight = NSToIntRound(rounded * app2dev);

	  // this floor rounding is to make ours compatible with Nav 4.0
	  lfHeight = long(rounded * app2dev);
		return lfHeight;
	}
	else
		return NSToIntRound(aVal*app2dev);
}
#endif


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
#if DUMP_FONT_SIZES
void DumpFontSizes(nsIPresContext* aPresContext)
{
	static gOnce = true;
	if (gOnce)
	{
		gOnce = false;

			PRInt32 baseSize;
			PRInt32 htmlSize;
			PRInt32 cssSize;
			nscoord val;
			nscoord oldVal;

		nsIDeviceContext* dc;
		aPresContext->GetDeviceContext(&dc);
		float dev2app;
  	dc->GetDevUnitsToAppUnits(dev2app);

		bool doWinRounding = true;
		for (short i=0; i<2; i ++)
		{
			doWinRounding ^= true;
			printf("\n\n\n");
			printf("---------------------------------------------------------------\n");
			printf("                          CSS                                  \n");
			printf("                     Rounding %s\n", (doWinRounding ? "ON" : "OFF"));
			printf("---------------------------------------------------------------\n");
			printf("\n");
			printf("NEW SIZES:\n");
			printf("----------\n");
			printf("        xx-small  x-small   small     medium    large     x-large   xx-large\n");
			for (baseSize = 9; baseSize <= 20; baseSize++) {
				printf("%2d:     ", baseSize);
				for (cssSize = 0; cssSize <= 6; cssSize++) {
					val = NewCalcFontPointSize(cssSize, baseSize*dev2app, 1.0f, aPresContext, eFontSize_CSS);
					printf("%2d        ", RoundSize(val, aPresContext, false));
				}
				printf("\n");
			}

			printf("\n");
			printf("OLD SIZES:\n");
			printf("----------\n");
			printf("        xx-small  x-small   small     medium    large     x-large   xx-large\n");
			for (baseSize = 9; baseSize <= 20; baseSize++) {
				printf("%2d:     ", baseSize);
				for (cssSize = 0; cssSize <= 6; cssSize++) {
					val = OldCalcFontPointSize(cssSize, baseSize*dev2app, 1.0f);
					printf("%2d        ", RoundSize(val, aPresContext, doWinRounding));
				}
				printf("\n");
			}

			printf("\n");
			printf("DIFFS:\n");
			printf("------\n");
			printf("        xx-small  x-small   small     medium    large     x-large   xx-large\n");
			for (baseSize = 9; baseSize <= 20; baseSize++) {
				printf("%2d:     ", baseSize);
				for (cssSize = 0; cssSize <= 6; cssSize++) {
					oldVal = OldCalcFontPointSize(cssSize, baseSize*dev2app, 1.0f);
					val = NewCalcFontPointSize(cssSize, baseSize*dev2app, 1.0f, aPresContext, eFontSize_CSS);
					if (RoundSize(oldVal, aPresContext, doWinRounding) <= 8)
						printf(" .");
					else
					  printf("%2d", (RoundSize(val, aPresContext, false)-RoundSize(oldVal, aPresContext, doWinRounding)));
					printf("        ");
				}
				printf("\n");
			}



			printf("\n\n\n");
			printf("---------------------------------------------------------------\n");
			printf("                          HTML                                 \n");
			printf("                     Rounding %s\n", (doWinRounding ? "ON" : "OFF"));
			printf("---------------------------------------------------------------\n");
			printf("\n");
			printf("NEW SIZES:\n");
			printf("----------\n");
			printf("        #1        #2        #3        #4        #5        #6        #7\n");
			for (baseSize = 9; baseSize <= 20; baseSize++) {
				printf("%2d:     ", baseSize);
				for (htmlSize = 1; htmlSize <= 7; htmlSize++) {
					val = NewCalcFontPointSize(htmlSize, baseSize*dev2app, 1.0f, aPresContext, eFontSize_HTML);
					printf("%2d        ", RoundSize(val, aPresContext, false));
				}
				printf("\n");
			}

			printf("\n");
			printf("OLD SIZES:\n");
			printf("----------\n");
			printf("        #1        #2        #3        #4        #5        #6        #7\n");
			for (baseSize = 9; baseSize <= 20; baseSize++) {
				printf("%2d:     ", baseSize);
				for (htmlSize = 1; htmlSize <= 7; htmlSize++) {
					val = OldCalcFontPointSize(htmlSize, baseSize*dev2app, 1.0f);
					printf("%2d        ", RoundSize(val, aPresContext, doWinRounding));
				}
				printf("\n");
			}

			printf("\n");
			printf("DIFFS:\n");
			printf("------\n");
			printf("        #1        #2        #3        #4        #5        #6        #7\n");
			for (baseSize = 9; baseSize <= 20; baseSize++) {
				printf("%2d:     ", baseSize);
				for (htmlSize = 1; htmlSize <= 7; htmlSize++) {
					oldVal = OldCalcFontPointSize(htmlSize, baseSize*dev2app, 1.0f);
					val = NewCalcFontPointSize(htmlSize, baseSize*dev2app, 1.0f, aPresContext, eFontSize_HTML);
					if (RoundSize(oldVal, aPresContext, doWinRounding) <= 8)
						printf(" .");
					else
					  printf("%2d", (RoundSize(val, aPresContext, false)-RoundSize(oldVal, aPresContext, doWinRounding)));
					printf("        ");
				}
				printf("\n");
			}
			printf("\n\n\n");
		}
	}
}
#endif
