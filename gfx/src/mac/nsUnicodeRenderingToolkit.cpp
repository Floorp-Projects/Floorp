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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#include "nsUnicodeRenderingToolkit.h"
#include "nsUnicodeFontMappingMac.h"
#include "nsUnicodeFallbackCache.h"
#include "nsDrawingSurfaceMac.h"
#include "nsTransform2D.h"
#include "nsFontMetricsMac.h"
#include "nsGraphicState.h"
#include "prprf.h"
#include "nsCarbonHelpers.h"
#include "nsISaveAsCharset.h"
#include "nsIComponentManager.h"
#include "nsUnicharUtils.h"


#include "nsMacUnicodeFontInfo.h"
#include "nsICharRepresentable.h"

#include <FixMath.h>


#define BAD_FONT_NUM -1
#define BAD_SCRIPT 0x7F
#define STACK_TRESHOLD 1000
static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);

//#define DISABLE_TEC_FALLBACK
//#define DISABLE_PRECOMPOSEHANGUL_FALLBACK
//#define DISABLE_LATINL_FALLBACK
//#define DISABLE_ATSUI_FALLBACK
//#define DISABLE_TRANSLITERATE_FALLBACK
//#define DISABLE_UPLUS_FALLBACK

#define IS_FORMAT_CONTROL_CHARS(c) 	((0x2000==((c)&0xFFF0))||(0x2028==((c)&0xFFF8)))
#define IS_CONTEXTUAL_CHARS(c) 		  ((0x0600<=(c))&&((c)<0x1000))
#define IS_COMBINING_CHARS(c) 		  ((0x0300<=(c))&&((c)<0x0370))

#define QUESTION_FALLBACKSIZE 9
#define UPLUS_FALLBACKSIZE 9
#define IN_RANGE(c, l, h) (((l) <= (c)) && ((c) <= (h)))
#define IN_STANDARD_MAC_ROMAN_FONT(c) ( \
  (((c) < 0x180) && (! (\
   (0x0110 == (c)) || \
   (0x012c == (c)) || \
   (0x012d == (c)) || \
   (0x0138 == (c)) || \
   (0x014a == (c)) || \
   (0x014b == (c)) || \
   (0x017f == (c))    \
   )))) 
#define IN_SYMBOL_FONT(c) ( \
  IN_RANGE(c, 0x0391, 0x03a1) || \
  IN_RANGE(c, 0x03a3, 0x03a9) || \
  IN_RANGE(c, 0x03b1, 0x03c1) || \
  IN_RANGE(c, 0x03c3, 0x03c9) || \
  (0x2111 == (c))   || \
  (0x2118 == (c))   || \
  (0x211c == (c))   || \
  (0x2135 == (c))   || \
  IN_RANGE(c, 0x2190, 0x2193) || \
  IN_RANGE(c, 0x21d0, 0x21d3) || \
  IN_RANGE(c, 0x21ed, 0x21ee) || \
  (0x2200 == (c))   || \
  (0x2203 == (c))   || \
  (0x2205 == (c))   || \
  (0x2208 == (c))   || \
  (0x2209 == (c))   || \
  (0x2212 == (c))   || \
  (0x2217 == (c))   || \
  (0x221d == (c))   || \
  (0x2220 == (c))   || \
  IN_RANGE(c, 0x2227, 0x222b) || \
  (0x2234 == (c))   || \
  (0x223c == (c))   || \
  (0x2261 == (c))   || \
  (0x2282 == (c))   || \
  (0x2283 == (c))   || \
  (0x2286 == (c))   || \
  (0x2287 == (c))   || \
  (0x2295 == (c))   || \
  (0x2297 == (c))   || \
  (0x22a5 == (c))   || \
  (0x2320 == (c))   || \
  (0x2321 == (c))   || \
  (0x240d == (c))   || \
  (0x2660 == (c))   || \
  (0x2663 == (c))   || \
  (0x2665 == (c))   || \
  (0x2666 == (c))      \
  )
#define SPECIAL_IN_SYMBOL_FONT(c) ( \
  IN_RANGE(c, 0x2308, 0x230b) || \
  (0x2329 == (c))   || \
  (0x232a == (c))    \
  )
#define BAD_TEXT_ENCODING 0xFFFFFFFF

// all the chracter should not be drawn if there are no glyph
#define IS_ZERO_WIDTH_CHAR(c) ( \
  IN_RANGE(c, 0x200b, 0x200f) || \
  IN_RANGE(c, 0x202a, 0x202e) \
  )
  

#define IN_ARABIC_PRESENTATION_A(a) ((0xfb50 <= (a)) && ((a) <= 0xfdff))
#define IN_ARABIC_PRESENTATION_B(a) ((0xfe70 <= (a)) && ((a) <= 0xfeff))
#define IN_ARABIC_PRESENTATION_A_OR_B(a) (IN_ARABIC_PRESENTATION_A(a) || IN_ARABIC_PRESENTATION_B(a))

// we should not ues TEC fallback for characters in latin, greek and cyrillic script
// because Japanese, Chinese and Korean font have these chracters. If we let them 
// render in the TEC fallback process, then we will use a Japanese/korean/chinese font
// to render it even the current font have a glyph in it
// if we skip the TEC fallback, then the ATSUI fallback will try to use the glyph 
// in the font first (TEC or TEC fallback are using QuickDraw, which can only use 
// the glyphs that in the font script's encodign. but a lot of TrueType font
// have houndred more glyph in additional to the font scripts
#define IS_LATIN(c)  ( IN_RANGE(c, 0x0000, 0x024F) || IN_RANGE(c, 0x1e00, 0x1eff) )
#define IS_GREEK(c)  ( IN_RANGE(c, 0x0370, 0x03FF) || IN_RANGE(c, 0x1f00, 0x1fff) )
#define IS_CYRILLIC(c)  IN_RANGE(c, 0x0400, 0x04ff)
#define IS_SKIP_TEC_FALLBACK(c) (IS_LATIN(c) || IS_GREEK(c) || IS_CYRILLIC(c))

//------------------------------------------------------------------------
#pragma mark -
//------------------------------------------------------------------------

nsUnicodeFallbackCache* nsUnicodeRenderingToolkit :: GetTECFallbackCache()
{
	static nsUnicodeFallbackCache* gTECFallbackCache = nsnull;
	
	if( ! gTECFallbackCache)
		gTECFallbackCache = new nsUnicodeFallbackCache();
	return gTECFallbackCache;
}
//------------------------------------------------------------------------

PRBool 
nsUnicodeRenderingToolkit::TECFallbackGetDimensions(
  const PRUnichar *aCharPt, 
  nsTextDimensions& oDim, 
  short origFontNum, 
  nsUnicodeFontMappingMac& fontMapping)
{
  char buf[20];
  ByteCount processBytes = 0;
  ByteCount outLen = 0;
  ScriptCode fallbackScript;
  nsUnicodeFallbackCache* cache = GetTECFallbackCache();
  short aWidth;
  FontInfo finfo;
  const short *scriptFallbackFonts = fontMapping.GetScriptFallbackFonts();
  
  if ((0xf780 <= *aCharPt) && (*aCharPt <= 0xf7ff))
  {
    // If we are encountering our PUA characters for User-Defined characters, we better
    // just drop the high-byte and return the width for the low-byte.
    *buf = (*aCharPt & 0x00FF);
    ::GetFontInfo(&finfo);
    oDim.ascent = finfo.ascent;
    oDim.descent = finfo.descent;
    
    GetScriptTextWidth (buf,1,aWidth);
    oDim.width = aWidth;

    return PR_TRUE;
  }
  else if (cache->Get(*aCharPt, fallbackScript))
  {
    if (BAD_SCRIPT == fallbackScript)
      return PR_FALSE;
    
    if(fontMapping.ConvertUnicodeToGlyphs(scriptFallbackFonts[fallbackScript], aCharPt, 1,
        buf, STACK_TRESHOLD, outLen, processBytes, kUnicodeLooseMappingsMask))
    {  
        ::TextFont(scriptFallbackFonts[fallbackScript]);
        GetScriptTextWidth(buf, outLen, aWidth);
        ::GetFontInfo(&finfo);
        oDim.ascent = finfo.ascent;
        oDim.descent = finfo.descent;
        oDim.width = aWidth;
        ::TextFont(origFontNum);
    }
    return PR_TRUE;
  }
  
  for(fallbackScript = 0 ; fallbackScript < 32; fallbackScript++)
  {
    if (BAD_FONT_NUM != scriptFallbackFonts[fallbackScript])
    {
        if(fontMapping.ConvertUnicodeToGlyphs(scriptFallbackFonts[fallbackScript], aCharPt, 1,
            buf, STACK_TRESHOLD, outLen, processBytes, kUnicodeLooseMappingsMask))
        {
            NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");
            ::TextFont(scriptFallbackFonts[fallbackScript]);
            GetScriptTextWidth(buf, outLen, aWidth);
            ::GetFontInfo(&finfo);
            oDim.ascent = finfo.ascent;
            oDim.descent = finfo.descent;
            oDim.width = aWidth;
            ::TextFont(origFontNum);        
            break;
        }          
    }
  }
  
  if (0 == outLen)
    fallbackScript = BAD_SCRIPT;
    
  // put into cache
  cache->Set(*aCharPt, fallbackScript);
  
  return (BAD_SCRIPT != fallbackScript);
}
//------------------------------------------------------------------------

#if MOZ_MATHML
PRBool nsUnicodeRenderingToolkit::TECFallbackGetBoundingMetrics(
    const PRUnichar *aCharPt,
    nsBoundingMetrics& oBoundingMetrics,
    short fontNum,
    nsUnicodeFontMappingMac& fontMapping)
{
    char buf[STACK_TRESHOLD];
    ByteCount processBytes = 0;
    ByteCount outLen = 0;
    ScriptCode fallbackScript;
    nsUnicodeFallbackCache* cache = GetTECFallbackCache();
    const short *scriptFallbackFonts = fontMapping.GetScriptFallbackFonts();
    
    if((0xf780 <= *aCharPt) && (*aCharPt <= 0xf7ff))
    {
        // If we are encountering our PUA characters for User-Defined characters, we better
        // just drop the high-byte and return the width for the low-byte.
        *buf = (*aCharPt & 0x00FF);
        GetScriptTextBoundingMetrics(buf, 1, ::FontToScript(fontNum), oBoundingMetrics);
        return PR_TRUE;
    }
    else if(cache->Get(*aCharPt, fallbackScript))
    {
        if(BAD_SCRIPT == fallbackScript)
            return PR_FALSE;
        
        if(fontMapping.ConvertUnicodeToGlyphs(scriptFallbackFonts[fallbackScript], aCharPt, 1,
            buf, STACK_TRESHOLD, outLen, processBytes, kUnicodeLooseMappingsMask))
        {  
            ::TextFont(scriptFallbackFonts[fallbackScript]);
            GetScriptTextBoundingMetrics(buf, outLen, fallbackScript, oBoundingMetrics);
            ::TextFont(fontNum);
        }
        return PR_TRUE;
    }
    
    for(fallbackScript = 0; fallbackScript < 32; fallbackScript++)
    {
        if(BAD_FONT_NUM != scriptFallbackFonts[fallbackScript])
        {
            if(fontMapping.ConvertUnicodeToGlyphs(scriptFallbackFonts[fallbackScript], aCharPt, 1,
                buf, STACK_TRESHOLD, outLen, processBytes, kUnicodeLooseMappingsMask))
            {
                NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");
                ::TextFont(scriptFallbackFonts[fallbackScript]);
                GetScriptTextBoundingMetrics(buf, outLen, fallbackScript, oBoundingMetrics);           
                ::TextFont(fontNum);
                break;
            }   
        }
    }
    
    if(0 == outLen)
        fallbackScript = BAD_SCRIPT;
        
    // put into cache
    cache->Set(*aCharPt, fallbackScript);
    
    return (BAD_SCRIPT != fallbackScript);
}
#endif // MOZ_MATHML
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: TECFallbackDrawChar(
  const PRUnichar *aCharPt, 
  PRInt32 x, 
  PRInt32 y, 
  short& oWidth, 
  short origFontNum, 
  nsUnicodeFontMappingMac& fontMapping)
{
  char buf[20];
  ByteCount processBytes = 0;
  ByteCount outLen = 0;
  ScriptCode fallbackScript;
  nsUnicodeFallbackCache* cache = GetTECFallbackCache();
  const short *scriptFallbackFonts = fontMapping.GetScriptFallbackFonts();
  
  // since we always call TECFallbackGetWidth before TECFallbackDrawChar
  // we could assume that we can always get the script code from cache.
  if ((0xf780 <= *aCharPt) && (*aCharPt <= 0xf7ff))
  {
    // If we are encountering our PUA characters for User-Defined characters, we better
    // just drop the high-byte and draw the text for the low-byte.
    *buf = (*aCharPt & 0x00FF);
    DrawScriptText (buf,1,x,y,oWidth);

    return PR_TRUE;
  }
  else if (cache->Get(*aCharPt, fallbackScript))
  {
    if (BAD_SCRIPT == fallbackScript)
      return PR_FALSE;
    
    if(fontMapping.ConvertUnicodeToGlyphs(scriptFallbackFonts[fallbackScript], aCharPt, 1,
        buf, STACK_TRESHOLD, outLen, processBytes, kUnicodeLooseMappingsMask))
    {
        ::TextFont(scriptFallbackFonts[fallbackScript]);
        DrawScriptText(buf, outLen, x, y, oWidth);
        ::TextFont(origFontNum);
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}
//------------------------------------------------------------------------
static PRUnichar gSymbolReplacement[]={0xf8ee,0xf8f9,0xf8f0,0xf8fb,0x3008,0x3009};

//------------------------------------------------------------------------

static nsresult FormAorBIsolated(PRUnichar aChar, nsMacUnicodeFontInfo& aInfo, PRUnichar* aOutChar)
{
  static const PRUnichar arabicisolated[]=
  {
    // start of form a
    0xFB50, 0x0671,  0xFB52, 0x067B,  0xFB56, 0x067E,  0xFB5A, 0x0680,  0xFB5E, 0x067A,
    0xFB62, 0x067F,  0xFB66, 0x0679,  0xFB6A, 0x06A4,  0xFB6E, 0x06A6,  0xFB72, 0x0684,
    0xFB76, 0x0683,  0xFB7A, 0x0686,  0xFB7E, 0x0687,  0xFB82, 0x068D,  0xFB84, 0x068C,
    0xFB86, 0x068E,  0xFB88, 0x0688,  0xFB8A, 0x0698,  0xFB8C, 0x0691,  0xFB8E, 0x06A9,
    0xFB92, 0x06AF,  0xFB96, 0x06B3,  0xFB9A, 0x06B1,  0xFB9E, 0x06BA,  0xFBA0, 0x06BB,
    0xFBA4, 0x06C0,  0xFBA6, 0x06C1,  0xFBAA, 0x06BE,  0xFBAE, 0x06D2,  0xFBB0, 0x06D3,
    0xFBD3, 0x06AD,  0xFBD7, 0x06C7,  0xFBD9, 0x06C6,  0xFBDB, 0x06C8,  0xFBDD, 0x0677,
    0xFBDE, 0x06CB,  0xFBE0, 0x06C5,  0xFBE2, 0x06C9,  0xFBE4, 0x06D0,  0xFBFC, 0x06CC,

    // start of form b
    0xFE70, 0x064B,  0xFE72, 0x064C,  0xFE74, 0x064D,  0xFE76, 0x064E,  0xFE78, 0x064F,
    0xFE7A, 0x0650,  0xFE7C, 0x0651,  0xFE7E, 0x0652,  0xFE80, 0x0621,  0xFE81, 0x0622,
    0xFE83, 0x0623,  0xFE85, 0x0624,  0xFE87, 0x0625,  0xFE89, 0x0626,  0xFE8D, 0x0627,
    0xFE8F, 0x0628,  0xFE93, 0x0629,  0xFE95, 0x062A,  0xFE99, 0x062B,  0xFE9D, 0x062C,
    0xFEA1, 0x062D,  0xFEA5, 0x062E,  0xFEA9, 0x062F,  0xFEAB, 0x0630,  0xFEAD, 0x0631,
    0xFEAF, 0x0632,  0xFEB1, 0x0633,  0xFEB5, 0x0634,  0xFEB9, 0x0635,  0xFEBD, 0x0636,
    0xFEC1, 0x0637,  0xFEC5, 0x0638,  0xFEC9, 0x0639,  0xFECD, 0x063A,  0xFED1, 0x0641,
    0xFED5, 0x0642,  0xFED9, 0x0643,  0xFEDD, 0x0644,  0xFEE1, 0x0645,  0xFEE5, 0x0646,
    0xFEE9, 0x0647,  0xFEED, 0x0648,  0xFEEF, 0x0649,  0xFEF1, 0x064A,
    0x0000
  };
  const PRUnichar* p;
  for ( p= arabicisolated; *p; p += 2) {
    if (aChar == *p) {
      if (aInfo.HasGlyphFor(*(p+1))) {
        *aOutChar = *(p+1);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}
//------------------------------------------------------------------------

PRBool 
nsUnicodeRenderingToolkit::ATSUIFallbackGetDimensions(
  const PRUnichar *aCharPt, 
  nsTextDimensions& oDim, 
  short origFontNum,
  short aSize, PRBool aBold, PRBool aItalic, nscolor aColor) 
{
  
  nsMacUnicodeFontInfo info;
  if (nsATSUIUtils::IsAvailable()  
      && (IN_STANDARD_MAC_ROMAN_FONT(*aCharPt) 
         ||IN_SYMBOL_FONT(*aCharPt)
         ||SPECIAL_IN_SYMBOL_FONT(*aCharPt)
         ||info.HasGlyphFor(*aCharPt)))
  {
    mATSUIToolkit.PrepareToDraw(mPort, mContext );
    nsresult res;
    if (SPECIAL_IN_SYMBOL_FONT(*aCharPt))
    {
      short rep = 0;
      if ((*aCharPt) > 0x230b)
         rep = (*aCharPt) - 0x2325;
      else 
         rep = (*aCharPt) - 0x2308;
      res = mATSUIToolkit.GetTextDimensions(gSymbolReplacement+rep, 1, oDim, aSize, 
                                            origFontNum, 
                                            aBold, aItalic, aColor);
    } 
    else 
    {
      res = mATSUIToolkit.GetTextDimensions(aCharPt, 1, oDim, aSize, 
                                            origFontNum, 
                                            aBold, aItalic, aColor);
    }
    if (NS_SUCCEEDED(res)) 
    {
      return PR_TRUE;
    }
  }
  if (IN_ARABIC_PRESENTATION_A_OR_B(*aCharPt))
  {      
    PRUnichar isolated;
    if (NS_SUCCEEDED( FormAorBIsolated(*aCharPt, info, &isolated))) 
    {
      if(NS_SUCCEEDED(ATSUIFallbackGetDimensions(&isolated, oDim, origFontNum, 
                                                 aSize, aBold, aItalic, aColor))) 
      {
         return PR_TRUE;
      }
    }                                                 
  }

  // we know some ATSUI font do not have bold, turn it off and try again
  if (aBold) 
  {
	  if (NS_SUCCEEDED(ATSUIFallbackGetDimensions(aCharPt, oDim, origFontNum, 
	                                              aSize, PR_FALSE, aItalic, aColor))) 
	  {
	     return PR_TRUE;
	  }
  }
  // we know some ATSUI font do not have italic, turn it off and try again
  if (aItalic) 
  {
	  if (NS_SUCCEEDED(ATSUIFallbackGetDimensions(aCharPt, oDim, origFontNum, 
	                                              aSize, PR_FALSE, PR_FALSE, aColor))) 
	  {
	     return PR_TRUE;
	  }
  }
  return PR_FALSE;
}
//------------------------------------------------------------------------

#ifdef MOZ_MATHML
PRBool
nsUnicodeRenderingToolkit::ATSUIFallbackGetBoundingMetrics(
  const PRUnichar *aCharPt,
  nsBoundingMetrics& oBoundingMetrics,
  short origFontNum,
  short aSize, PRBool aBold, PRBool aItalic, nscolor aColor)
{

  nsMacUnicodeFontInfo info;
  if (nsATSUIUtils::IsAvailable()  
      && (IN_STANDARD_MAC_ROMAN_FONT(*aCharPt) 
         ||IN_SYMBOL_FONT(*aCharPt)
         ||SPECIAL_IN_SYMBOL_FONT(*aCharPt)
         ||info.HasGlyphFor(*aCharPt)))
  {
    mATSUIToolkit.PrepareToDraw(mPort, mContext );
    nsresult res;
    if (SPECIAL_IN_SYMBOL_FONT(*aCharPt))
    {
      short rep = 0;
      if ((*aCharPt) > 0x230b)
         rep = (*aCharPt) - 0x2325;
      else
         rep = (*aCharPt) - 0x2308;
      res = mATSUIToolkit.GetBoundingMetrics(gSymbolReplacement+rep, 1, oBoundingMetrics, aSize, 
                                             origFontNum, 
                                             aBold, aItalic, aColor);
    }
    else
    {
      res = mATSUIToolkit.GetBoundingMetrics(aCharPt, 1, oBoundingMetrics, aSize, 
                                             origFontNum, 
                                             aBold, aItalic, aColor);
    }
    if (NS_SUCCEEDED(res))
    {
      return PR_TRUE;
    }
  }
  if (IN_ARABIC_PRESENTATION_A_OR_B(*aCharPt))
  {  
    PRUnichar isolated;
    if (NS_SUCCEEDED( FormAorBIsolated(*aCharPt, info, &isolated))) 
    {
      if(NS_SUCCEEDED(ATSUIFallbackGetBoundingMetrics(&isolated, oBoundingMetrics, origFontNum, 
                                                      aSize, aBold, aItalic, aColor))) 
      {
        return PR_TRUE;
      }
    }                                                 
  }

  // we know some ATSUI font do not have bold, turn it off and try again
  if (aBold)
  {
    if (NS_SUCCEEDED(ATSUIFallbackGetBoundingMetrics(aCharPt, oBoundingMetrics, origFontNum, 
                                                     aSize, PR_FALSE, aItalic, aColor))) 
    {
      return PR_TRUE;
    }
  }
  // we know some ATSUI font do not have italic, turn it off and try again
  if (aItalic) 
  {
    if (NS_SUCCEEDED(ATSUIFallbackGetBoundingMetrics(aCharPt, oBoundingMetrics, origFontNum, 
                                                     aSize, PR_FALSE, PR_FALSE, aColor))) 
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}
#endif // MOZ_MATHML

//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: ATSUIFallbackDrawChar(
  const PRUnichar *aCharPt, 
  PRInt32 x,   PRInt32 y, 
  short& oWidth, 
  short origFontNum,
  short aSize, PRBool aBold, PRBool aItalic, nscolor aColor) 
{
  nsMacUnicodeFontInfo info;
  if (nsATSUIUtils::IsAvailable()  
      && (IN_STANDARD_MAC_ROMAN_FONT(*aCharPt) 
         ||IN_SYMBOL_FONT(*aCharPt)
         ||SPECIAL_IN_SYMBOL_FONT(*aCharPt)
         ||info.HasGlyphFor(*aCharPt)))
  {
    mATSUIToolkit.PrepareToDraw(mPort, mContext );
    nsresult res;
    if(SPECIAL_IN_SYMBOL_FONT(*aCharPt)) 
    {
      short rep = 0;
      if ((*aCharPt) > 0x230b)
        rep = (*aCharPt) - 0x2325;
      else 
        rep = (*aCharPt) - 0x2308;
      res = mATSUIToolkit.DrawString(gSymbolReplacement+rep, 1, x, y, oWidth, aSize, 
                                     origFontNum, 
                                     aBold, aItalic, aColor);
    } else {
      res = mATSUIToolkit.DrawString(aCharPt, 1, x, y, oWidth, aSize, 
                                     origFontNum, 
                                     aBold, aItalic, aColor);
    }
    if (NS_SUCCEEDED(res))
      return PR_TRUE;
  }
  if (IN_ARABIC_PRESENTATION_A_OR_B(*aCharPt))
  {      
    PRUnichar isolated;
    if (NS_SUCCEEDED( FormAorBIsolated(*aCharPt, info, &isolated))) {
      if (NS_SUCCEEDED(ATSUIFallbackDrawChar(&isolated, x, y, oWidth, origFontNum, 
                                             aSize, aBold, aItalic, aColor))) 
      {
         return PR_TRUE;
      }
    }                                                 
  }
  // we know some ATSUI font do not have bold, turn it off and try again
  if (aBold)
  {
    if (NS_SUCCEEDED(ATSUIFallbackDrawChar(aCharPt, x, y, oWidth, origFontNum, 
                                          aSize, PR_FALSE, aItalic, aColor))) 
    {
       return PR_TRUE;
    }
  }
  // we know some ATSUI font do not have italic, turn it off and try again
  if (aItalic)
  {
    if (NS_SUCCEEDED(ATSUIFallbackDrawChar(aCharPt, x, y, oWidth, origFontNum, 
                                           aSize, PR_FALSE, PR_FALSE, aColor))) 
    {
       return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool  nsUnicodeRenderingToolkit :: SurrogateGetDimensions(
  const PRUnichar *aSurrogatePt, nsTextDimensions& oDim, short origFontNum,  
  short aSize, PRBool aBold, PRBool aItalic, nscolor aColor)
{
  nsresult res;
  mATSUIToolkit.PrepareToDraw(mPort, mContext );
  res = mATSUIToolkit.GetTextDimensions(aSurrogatePt, 2, oDim, aSize, 
                                        origFontNum, 
                                        aBold, aItalic, aColor);
  return NS_SUCCEEDED(res);
}

PRBool  nsUnicodeRenderingToolkit :: SurrogateDrawChar(
  const PRUnichar *aSurrogatePt, PRInt32 x, PRInt32 y, short& oWidth, short origFontNum, 
  short aSize, PRBool aBold, PRBool aItalic, nscolor aColor)
{
  nsresult res;
  mATSUIToolkit.PrepareToDraw(mPort, mContext );
  res = mATSUIToolkit.DrawString(aSurrogatePt, 2, x, y, oWidth, aSize, 
                                 origFontNum, 
                                 aBold, aItalic, aColor);
  return NS_SUCCEEDED(res);
}

#ifdef MOZ_MATHML
PRBool  nsUnicodeRenderingToolkit :: SurrogateGetBoundingMetrics(
  const PRUnichar *aSurrogatePt, nsBoundingMetrics& oBoundingMetrics, short origFontNum,
  short aSize, PRBool aBold, PRBool aItalic, nscolor aColor)
{
  nsresult res;
  mATSUIToolkit.PrepareToDraw(mPort, mContext );
  res = mATSUIToolkit.GetBoundingMetrics(aSurrogatePt, 2, oBoundingMetrics, aSize, 
                                             origFontNum, 
                                             aBold, aItalic, aColor);

  return NS_SUCCEEDED(res);
}
#endif

static const char question[] = "<?>";

//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: QuestionMarkFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth)
{
  CGrafPtr thePort;
  ::GetPort((GrafPtr*)&thePort);
  short saveSize = ::GetPortTextSize(thePort);		
  ::TextSize(QUESTION_FALLBACKSIZE);
  GetScriptTextWidth(question, 3,oWidth);
  ::TextSize(saveSize);
  return PR_TRUE;
}
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: QuestionMarkFallbackDrawChar(
	const PRUnichar *aCharPt, 
	PRInt32 x, 
	PRInt32 y, 
	short& oWidth)
{
  CGrafPtr thePort;
  ::GetPort((GrafPtr*)&thePort);
  short saveSize = ::GetPortTextSize(thePort);		
  ::TextSize(QUESTION_FALLBACKSIZE);
  DrawScriptText(question, 3, x, y, oWidth);
  ::TextSize(saveSize);
  return PR_TRUE;
}
//------------------------------------------------------------------------
PRBool nsUnicodeRenderingToolkit :: LoadTransliterator()
{
	if(mTrans) 
		return PR_TRUE;
		
	nsresult res;
    mTrans = do_CreateInstance(kSaveAsCharsetCID, &res);
    if ( NS_SUCCEEDED(res) )
    {
       res = mTrans->Init("x-mac-roman",
               nsISaveAsCharset::attr_FallbackQuestionMark +
               nsISaveAsCharset::attr_EntityBeforeCharsetConv +
               nsISaveAsCharset::attr_IgnoreIgnorables,
               nsIEntityConverter::transliterate);
      NS_ASSERTION(NS_SUCCEEDED(res), "cannot init the converter");
      if (NS_FAILED(res)) 
      {
        mTrans = nsnull;
        return PR_FALSE;
      }
    }
    return PR_TRUE;
}
//------------------------------------------------------------------------
PRBool nsUnicodeRenderingToolkit :: TransliterateFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth)
{
  if(LoadTransliterator()) {
    nsAutoString tmp(aCharPt, 1);
    char* conv = nsnull;
    if(NS_SUCCEEDED(mTrans->Convert(tmp.get(), &conv)) && conv) {
      CGrafPtr thePort;
      ::GetPort((GrafPtr*)&thePort);
	    short aSize = ::GetPortTextSize(thePort);		 		
  		PRInt32 l=strlen(conv);
    	if((l>3) && ('^' == conv[0]) && ('(' == conv[1]) && (')' == conv[l-1])) // sup
    	{
    		short small = aSize * 2 / 3;
    		::TextSize(small);
  			GetScriptTextWidth(conv+2, l-3,oWidth);   
    		::TextSize(aSize);
    	} 
    	else if((l>3) && ('v' == conv[0]) && ('(' == conv[1]) && (')' == conv[l-1])) // sub
    	{
    		short small = aSize * 2 / 3;
    		::TextSize(small);
  			GetScriptTextWidth(conv+2, l-3,oWidth);   
     		::TextSize(aSize);
   		} 
   		else if((l>1) && ('0' <= conv[0]) && ( conv[0] <= '9') && ('/' == conv[1])) // fract
    	{
    		short small = aSize * 2 / 3;
    		short tmpw=0;
    		
    		::TextSize(small);
   			GetScriptTextWidth(conv, 1 ,tmpw);   
   			oWidth = tmpw;
    		
     		::TextSize(aSize);
  			GetScriptTextWidth(conv+1, 1,tmpw);   
    		oWidth += tmpw;
     		
    		if(l>2) {
    			::TextSize(small);
  				GetScriptTextWidth(conv+2, l-2,tmpw);   
    			oWidth += tmpw;
 	    		::TextSize(aSize);
    		}
    	} else {
  			GetScriptTextWidth(conv, l,oWidth);   
  		}
  		
  		 
    	nsMemory::Free(conv);
    	return PR_TRUE;
    }
  }
  return PR_FALSE;
}
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: TransliterateFallbackDrawChar(
	const PRUnichar *aCharPt, 
	PRInt32 x, 
	PRInt32 y, 
	short& oWidth)
{
  if(LoadTransliterator()) {
    nsAutoString tmp(aCharPt, 1);
    char* conv = nsnull;
    if(NS_SUCCEEDED(mTrans->Convert(tmp.get(), &conv)) && conv) {
	    CGrafPtr thePort;
	    ::GetPort((GrafPtr*)&thePort);
	    short aSize = ::GetPortTextSize(thePort);		
    	PRInt32 l=strlen(conv);
    	if((l>3) && ('^' == conv[0]) && ('(' == conv[1]) && (')' == conv[l-1])) // sup
    	{
    		short small = aSize * 2 / 3;
    		::TextSize(small);
    		DrawScriptText(conv+2, l-3, x, y-small/2, oWidth);
    		::TextSize(aSize);
    	} 
    	else if((l>3) && ('v' == conv[0]) && ('(' == conv[1]) && (')' == conv[l-1])) // sub
    	{
    		short small = aSize * 2 / 3;
    		::TextSize(small);
    		DrawScriptText(conv+2, l-3, x, y+small/2, oWidth);
     		::TextSize(aSize);
   		} 
   		else if((l>1) && ('0' <= conv[0]) && ( conv[0] <= '9') && ('/' == conv[1])) // fract
    	{
    		short small = aSize * 2 / 3;
    		short tmpw=0;
    		
    		::TextSize(small);
    		DrawScriptText(conv, 1, x, y-small/2, tmpw);
    		oWidth = tmpw;
    		
     		::TextSize(aSize);
    		DrawScriptText(conv+1, 1, x+oWidth, y, tmpw);
    		oWidth += tmpw;
     		
    		if(l>2) {
    			::TextSize(small);
    			DrawScriptText(conv+2, l-2, x+oWidth, y+small/2, tmpw);
    			oWidth += tmpw;
 	    		::TextSize(aSize);
    		}
    	} else {
  			DrawScriptText(conv, l, x, y, oWidth);
  		}
    	nsMemory::Free(conv);
    	return PR_TRUE;
    }
  }
  return PR_FALSE;
}
//------------------------------------------------------------------------
#define CAN_DO_PRECOMPOSE_HANGUL(u, f) ((0xAC00<=(u)) && ((u)<=0xD7FF) && ((f) != BAD_FONT_NUM))
#define SBase 0xAC00
#define LCount 19
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
static void UnicodePrecomposedHangulTo4EUCKR(PRUnichar in, char *out)
{
        static PRUint8 lMap[LCount] = {
          0xa1, 0xa2, 0xa4, 0xa7, 0xa8, 0xa9, 0xb1, 0xb2, 0xb3, 0xb5,
          0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe
        };

        static PRUint8 tMap[TCount] = {
          0xd4, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa9, 0xaa, 
          0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb4, 0xb5, 
          0xb6, 0xb7, 0xb8, 0xba, 0xbb, 0xbc, 0xbd, 0xbe
        };
        PRUint16 SIndex, LIndex, VIndex, TIndex;
        /* the following line are copy from Unicode 2.0 page 3-13 */
        /* item 1 of Hangul Syllabel Decomposition */
        SIndex =  in - SBase;

        /* the following lines are copy from Unicode 2.0 page 3-14 */
        /* item 2 of Hangul Syllabel Decomposition w/ modification */
        LIndex = SIndex / NCount;
        VIndex = (SIndex % NCount) / TCount;
        TIndex = SIndex % TCount;
		// somehow Apple's Korean font show glaph on A4D4 :( so we use '[' + L + V + T + ']' intead of 
		// Filler + L + V + T to display
		// output '['
		*out++ = '['; 
		// output L
		*out++ = 0xA4;
		*out++ = lMap[LIndex] ;
		// output V
		*out++ = 0xA4;
		*out++  = (VIndex + 0xbf);
		// output T
		*out++ = 0xA4;
		*out++ = tMap[TIndex] ;
		// output ']'
		*out++ = ']'; 
}
    
//------------------------------------------------------------------------
PRBool nsUnicodeRenderingToolkit :: PrecomposeHangulFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth,
	short koreanFont,
	short origFont)
{
  if(CAN_DO_PRECOMPOSE_HANGUL(*aCharPt, koreanFont)) {
	  char euckr[8];
	  if(koreanFont != origFont)
	  	::TextFont(koreanFont);		  
	  UnicodePrecomposedHangulTo4EUCKR(*aCharPt, euckr);
	  GetScriptTextWidth(euckr, 8, oWidth); 
	  if(koreanFont != origFont)
	  	::TextFont(origFont);		  
	  return PR_TRUE;
  } else {
	  return PR_FALSE;
  }
}
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: PrecomposeHangulFallbackDrawChar(
	const PRUnichar *aCharPt, 
	PRInt32 x, 
	PRInt32 y, 
	short& oWidth,
	short koreanFont,
	short origFont)
{
  if(CAN_DO_PRECOMPOSE_HANGUL(*aCharPt, koreanFont)) {
	  char euckr[8];
	  if(koreanFont != origFont)
	  	::TextFont(koreanFont);		  
	  UnicodePrecomposedHangulTo4EUCKR(*aCharPt, euckr);
	  DrawScriptText(euckr, 8, x, y, oWidth); 
	  if(koreanFont != origFont)
	  	::TextFont(origFont);		  
	  return PR_TRUE;
  } else {
	  return PR_FALSE;
  }
}
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: UPlusFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth)
{
  CGrafPtr thePort;
  ::GetPort((GrafPtr*)&thePort);
  short saveSize = ::GetPortTextSize(thePort);		
  char buf[16];
  PRUint32 len = PR_snprintf(buf, 16 , "<U+%04X>", *aCharPt);
  ::TextSize(UPLUS_FALLBACKSIZE);
  if(len != -1) 
    GetScriptTextWidth(buf, len, oWidth);
  ::TextSize(saveSize);
  return (-1 != len);
}
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: UPlusFallbackDrawChar(
	const PRUnichar *aCharPt, 
	PRInt32 x, 
	PRInt32 y, 
	short& oWidth)
{
  CGrafPtr thePort;
  ::GetPort((GrafPtr*)&thePort);
  short saveSize = ::GetPortTextSize(thePort);		
  char buf[16];
  PRUint32 len = PR_snprintf(buf, 16 , "<U+%04X>", *aCharPt);
  ::TextSize(UPLUS_FALLBACKSIZE);
  if(len != -1) 
    DrawScriptText(buf, len, x, y, oWidth);
  ::TextSize(saveSize);
  return (-1 != len);
}
//------------------------------------------------------------------------
/*
# capital mean above
# small mean below
# r - ring below
# R - Ring Above
# d - dot below
# D - Dot Above
# l - line below
# L - Line Above
# c - cedilla
# A - Acute
# x - circumflex below
# X - Circumflex Above
# G - Grave above
# T - Tilde above
# t - tilde below
# B - Breve Above
# b - breve below
# U - Diaeresis Above
# u - diaeresis below
# H - Hook Above 
# N - Horn Above
*/
static const char * const g1E00Dec = 
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E00 - U+1E0F
 "Ar ar BD bD Bd bd Bl bl CcAccADD dD Dd dd Dl dl "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E10 - U+1E1F
 "Dc dc Dx dx ELGeLGELAeLAEx ex Et et EcBecBFD fD "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E20 - U+1E2F
 "GL gL HD hD Hd hd HU hU Hc hc Hb hb It it IUAiUA"
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E30 - U+1E3F
 "KA kA Kd kd Kl kl Ld ld LdLldLLl ll Lx lx MA mA "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E40 - U+1E4F
 "MD mD Md md ND nD Nd nd Nl nl Nx nx OTAoTAOTUoTU"
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E50 - U+1E5F
 "OLGoLGOLAoLAPA pA PD pD RD rD Rd rd RdLrdLRl rl "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E60 - U+1E6F
 "SD sD Sd sd SADsADSBDsBDSdDsdDTD tD Td td Tl tl "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E70 - U+1E7F
 "Tx tx Uu uu Ut ut Ux ux UTAuTAULUuLUVT vT Vd vd "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E80 - U+1E8F
 "WG wG WA wA WU wU WD wD Wd wd XD xD XU xU YD Yd "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1E90 - U+1E9F
 "ZX zX Zd zd Zl zl Hl tU wR yR aH                "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1EA0 - U+1EAF
 "Ad ad AH aH AXAaXAAXGaXGAXHaXHAXTaXTAdXadXABAaBA"
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1EB0 - U+1EBF
 "ABGaBGABHaBHABTaBTAdBadBEd ed EH eH ET eT EXAeXA"
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1EC0 - U+1ECF
 "EXGeXGEXHeXHEXTeXTEdXedXIH iH Id id Od od OH oH "
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1ED0 - U+1EDF
 "OXAoXAOXGoXGOXHoXHOXToXTOdXodXOAnoAnOGnoGnOHnoHn"
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1EE0 - U+1EEF
 "OHToHTOdTodTUd ud UH uh UAnuAnUGnuGnUHnuHnUTnuTn"
//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   U+1EF0 - U+1EFF
 "UdnudnYG yG Yd yd YH yH YT yT                   "
;
//------------------------------------------------------------------------
static void CCodeToMacRoman(char aIn, char& aOut,short aWidth, short aHeight, short& oXadj, short& oYadj)
{
	aOut = ' ';
	oXadj = oYadj = 0;
	switch(aIn)
	{
		case 'r' : aOut = '\xfb'; oYadj = aHeight * 5 / 6; break ;// # r - ring below
		case 'R' : aOut = '\xfb';                          break ;// # R - Ring Above
		case 'd' : aOut = '\xfa'; oYadj = aHeight * 5 / 6; break ;// # d - dot below
		case 'D' : aOut = '\xfa';                          break ;// # D - Dot Above
		case 'l' : aOut = '_';                             break ;// # l - line below
		case 'L' : aOut = '\xf8';                          break ;// # L - Line Above
		case 'c' : aOut = '\xfc';                          break ;// # c - cedilla
		case 'A' : aOut = '\xab';                          break ;// # A - Acute
		case 'x' : aOut = '\xf6'; oYadj= aHeight * 5 / 6;  break ;// # x - circumflex below
		case 'X' : aOut = '\xf6';                          break ;// # X - Circumflex Above
		case 'G' : aOut = '`';                             break ;// # G - Grave above
		case 'T' : aOut = '\xf7';                          break ;// # T - Tilde above
		case 't' : aOut = '\xf7'; oYadj= aHeight * 5 / 6;  break ;// # t - tilde below
		case 'B' : aOut = '\xf9';                          break ;// # B - Breve Above
		case 'b' : aOut = '\xf9'; oYadj= aHeight * 5 / 6;  break ;// # b - breve below
		case 'U' : aOut = '\xac';                          break ;// # U - Diaeresis Above
		case 'u' : aOut = '\xac'; oYadj= aHeight * 5 / 6;  break ;// # u - diaeresis below
		case 'H' : aOut = ',';    oYadj= - aHeight * 5 / 6;  break ;// # H - Hook Above 
		case 'n' : aOut = ',';    oYadj= - aHeight * 5 / 6;  oXadj = aWidth /4; break ;// # N - Horn Above
		default: NS_ASSERTION(0, "unknown ccode");
		        break;	
	}
}

PRBool nsUnicodeRenderingToolkit :: LatinFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth)
{
  if(0x1E00 == (0xFF00 & *aCharPt)) 
  {
  	PRInt32 idx = 3 * ( *aCharPt & 0x00FF);
  	if(' ' != g1E00Dec[idx])
  	{
   		GetScriptTextWidth(g1E00Dec+idx, 1, oWidth);
  		return PR_TRUE;
  	}
  }
  return PR_FALSE;
}
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: LatinFallbackDrawChar(
	const PRUnichar *aCharPt, 
	PRInt32 x, 
	PRInt32 y, 
	short& oWidth)
{
  if(0x1E00 == (0xFF00 & *aCharPt)) 
  {
  	PRInt32 idx = 3 * ( *aCharPt & 0x00FF);
  	if(' ' != g1E00Dec[idx])
  	{
      CGrafPtr thePort;
      ::GetPort((GrafPtr*)&thePort);
	    short aSize = ::GetPortTextSize(thePort);	
	    short dummy;
	    short realwidth;
  		char comb[2];
  		short xadj;
  		short yadj;
  		short yadjB=0;
  		
  		PRBool baseTooBig = 	('A'<= g1E00Dec[idx]  ) && (g1E00Dec[idx] <= 'Z'  ) 
  		                     || ('f' == g1E00Dec[idx])|| ('l' == g1E00Dec[idx])|| ('k' == g1E00Dec[idx]);
  		PRBool firstIsAbove    = ('A'<= g1E00Dec[idx+1]) && (g1E00Dec[idx+1] <= 'Z') ;
  		PRBool secondIsAbove   = ('A'<= g1E00Dec[idx+2]) && (g1E00Dec[idx+2] <= 'Z') ;
  		
  		GetScriptTextWidth(g1E00Dec+idx, 1, oWidth);
  		if(baseTooBig && (firstIsAbove ||secondIsAbove ))
    		::TextSize(aSize *3/4);		

  		DrawScriptText(g1E00Dec+idx, 1, x, y, realwidth);

  		if(baseTooBig && (firstIsAbove ||secondIsAbove ))
  			::TextSize(aSize);  			
  		if(' ' != g1E00Dec[idx+1]) {
  			CCodeToMacRoman(g1E00Dec[idx+1],comb[0], realwidth, aSize, xadj, yadj);
  			
  			GetScriptTextWidth(comb, 1, dummy);
  			
  			DrawScriptText( comb, 1, x + xadj + ( realwidth - dummy ) / 2, y + yadj + yadjB, dummy);
  		}
  		if(' ' != g1E00Dec[idx+2]) {
  			if( firstIsAbove && secondIsAbove)
   				yadjB = yadjB - aSize / 6; 		
  		
  			CCodeToMacRoman(g1E00Dec[idx+2],comb[0], realwidth, aSize, xadj, yadj);
  			GetScriptTextWidth(comb, 1, dummy);
  			DrawScriptText( comb, 1, x + xadj+ ( realwidth - dummy ) / 2, y+ yadj + yadjB, dummy);
  		}
  		return PR_TRUE;
  	}
  }
  return PR_FALSE;
}

//------------------------------------------------------------------------
void nsUnicodeRenderingToolkit :: GetScriptTextWidth(
	const char* buf, 
	ByteCount aLen, 
	short& oWidth)
{
 	oWidth = ::TextWidth(buf, 0, aLen);
}

#if MOZ_MATHML
//------------------------------------------------------------------------
void nsUnicodeRenderingToolkit::GetScriptTextBoundingMetrics(
    const char* buf,
    ByteCount aLen,
    ScriptCode aScript,
    nsBoundingMetrics& oBoundingMetrics)
{
    Point scale = { 1, 1 };
    Fixed stackWidths[STACK_TRESHOLD], *widths;
    Fixed stackLefts[STACK_TRESHOLD], *lefts;
    Rect stackRects[STACK_TRESHOLD], *rects;
    OSStatus err;

    NS_PRECONDITION(aLen > 0, "length must be greater than 0");

    if(aLen > STACK_TRESHOLD)
    {
        widths = (Fixed*) nsMemory::Alloc(aLen * sizeof(Fixed));
        lefts = (Fixed*) nsMemory::Alloc(aLen * sizeof(Fixed));
        rects = (Rect*) nsMemory::Alloc(aLen * sizeof(Fixed));
        
        // if any of the allocations failed the 'else' case below will be executed
    }
    else
    {
        widths = stackWidths;
        lefts = stackLefts;
        rects = stackRects;
    }

    if(!GetOutlinePreferred())
        SetOutlinePreferred(PR_TRUE);

    if(widths && lefts && rects &&
        (err = ::OutlineMetrics(aLen, buf, scale, scale, NULL, NULL, widths, lefts, rects)) == noErr)
    {
        ByteCount byteIndex = 0, glyphIndex = 0;

        while(byteIndex < aLen)
        {
            nsBoundingMetrics bounds;
            bounds.leftBearing = rects[glyphIndex].left + FixRound(lefts[glyphIndex]);
            bounds.rightBearing = rects[glyphIndex].right + FixRound(lefts[glyphIndex]);
            bounds.ascent = rects[glyphIndex].bottom;
            bounds.descent = -rects[glyphIndex].top;
            bounds.width = FixRound(widths[glyphIndex]);

            if(glyphIndex == 0)
                oBoundingMetrics = bounds;
            else
                oBoundingMetrics += bounds;

            // for two byte characters byteIndex will increase by 2
            //   while glyph index will only increase by 1
            if(CharacterByteType((Ptr) buf, byteIndex, aScript) == smFirstByte)
                byteIndex += 2;
            else
                byteIndex++;
            glyphIndex++;
        }
    }
    else
    {
        NS_WARNING("OulineMetrics failed");

        FontInfo fInfo;
        ::GetFontInfo(&fInfo);

        oBoundingMetrics.leftBearing = 0;
        oBoundingMetrics.rightBearing = ::TextWidth(buf, 0, aLen);
        oBoundingMetrics.ascent = fInfo.ascent;
        oBoundingMetrics.descent = fInfo.descent;
        oBoundingMetrics.width = oBoundingMetrics.rightBearing;
    }

    if(aLen > STACK_TRESHOLD)
    {
        if(widths)
            nsMemory::Free(widths);
        if(lefts)
            nsMemory::Free(lefts);
        if(rects)
            nsMemory::Free(rects);
    }
}
#endif // MOZ_MATHML

//------------------------------------------------------------------------
void nsUnicodeRenderingToolkit :: DrawScriptText(
	const char* buf, 
	ByteCount aLen, 
	PRInt32 x, 
	PRInt32 y, 
	short& oWidth)
{
	::MoveTo(x, y);
	::DrawText(buf,0,aLen);
	
	Point   penLoc;
	::GetPen(&penLoc);
	oWidth = penLoc.h - x;
}
//------------------------------------------------------------------------

nsresult 
nsUnicodeRenderingToolkit::GetTextSegmentWidth(
			const PRUnichar *aString, PRUint32 aLength, 
			short fontNum, nsUnicodeFontMappingMac& fontMapping, 
			PRUint32& oWidth)
{
  nsTextDimensions dim;
  nsresult res = GetTextSegmentDimensions(aString, aLength, fontNum, fontMapping, dim);
  oWidth = dim.width;
  return res;
}
//------------------------------------------------------------------------


nsresult 
nsUnicodeRenderingToolkit::GetTextSegmentDimensions(
      const PRUnichar *aString, PRUint32 aLength, 
      short fontNum, nsUnicodeFontMappingMac& fontMapping, 
      nsTextDimensions& oDim)
{
  oDim.Clear();
  if(aLength == 0) 
    return NS_OK;
  NS_PRECONDITION(BAD_FONT_NUM != fontNum, "illegal font num");
  short textWidth = 0;
  PRUint32 processLen = 0;
  char *heapBuf = nsnull;
  PRUint32 heapBufSize = 0;
  short thisWidth = 0;
  char stackBuf[STACK_TRESHOLD];
  char *buf ;
  ByteCount processBytes;
  ByteCount outLen;
  const short *scriptFallbackFonts = fontMapping.GetScriptFallbackFonts();
  
  ::TextFont(fontNum);
  
  FontInfo fInfo;
  ::GetFontInfo(&fInfo);
  nsTextDimensions segDim;
  segDim.ascent = fInfo.ascent;
  segDim.descent = fInfo.descent;
  oDim.Combine(segDim);
  
  // find buf from stack or heap. We only need to do this once in this function.
  // put this out of the loop for performance...
  ByteCount bufLen = aLength * 2 + 10;
  if (bufLen > STACK_TRESHOLD)
  {
    if (bufLen > heapBufSize)
    {
      if (heapBuf)
        nsMemory::Free(heapBuf);
      heapBuf = (char*) nsMemory::Alloc(bufLen);
      heapBufSize = bufLen;
      if (nsnull == heapBuf) 
        return NS_ERROR_OUT_OF_MEMORY;
    } 
    buf = heapBuf;
  } 
  else 
  {
    bufLen = STACK_TRESHOLD;
    buf = stackBuf;
  }
  do {
    outLen = 0;
    processBytes = 0;

    if(fontMapping.ConvertUnicodeToGlyphs(fontNum, aString, aLength - processLen,
            buf, bufLen, outLen, processBytes, 0))
    {
        GetScriptTextWidth(buf, outLen, thisWidth);
      
        segDim.Clear();
        segDim.width = thisWidth;
        oDim.Combine(segDim);
    
        NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");
    
        PRInt32 processUnicode = processBytes / 2;
        processLen += processUnicode;
        aString += processUnicode;
    }
         
    // Cannot precess by TEC, process one char a time by fallback mechanism
    if (processLen < aLength)
    {
      PRBool fallbackDone = PR_FALSE;
      segDim.Clear();
      
      if (IS_HIGH_SURROGATE(*aString) && 
          ((processLen+1) < aLength) &&
          IS_LOW_SURROGATE(*(aString+1)))
      {
         const nsFont *font;
         mGS->mFontMetrics->GetFont(font);
         fallbackDone = SurrogateGetDimensions(aString, segDim, fontNum, 
                                               font->size, 
                                               (font->weight > NS_FONT_WEIGHT_NORMAL), 
                                               ((NS_FONT_STYLE_ITALIC ==  font->style) || 
                                               (NS_FONT_STYLE_OBLIQUE ==  font->style)),
                                               mGS->mColor );     
         if (fallbackDone)
         {   
           oDim.Combine(segDim);    
           // for fallback measure/drawing, we always do one char a time.
           aString += 2;
           processLen += 2;
           continue;
         }
      }
#ifndef DISABLE_TEC_FALLBACK
      // Fallback by try different Script code
      if (! IS_SKIP_TEC_FALLBACK(*aString))
        fallbackDone = TECFallbackGetDimensions(aString, segDim, fontNum, fontMapping);
#endif

      //
      // We really don't care too much of performance after this
      // This will only be called when we cannot display this character in ANY mac script avaliable
      // 
#ifndef DISABLE_ATSUI_FALLBACK  
      // Fallback by using ATSUI
      if (!fallbackDone)  
      {
        const nsFont *font;
        mGS->mFontMetrics->GetFont(font);
        fallbackDone = ATSUIFallbackGetDimensions(aString, segDim, fontNum, 
                                                  font->size, 
                                                  (font->weight > NS_FONT_WEIGHT_NORMAL), 
                                                  ((NS_FONT_STYLE_ITALIC ==  font->style) || 
                                                   (NS_FONT_STYLE_OBLIQUE ==  font->style)),
                                                  mGS->mColor );
      }

#endif
		  if(! fallbackDone) {
		     if(IS_ZERO_WIDTH_CHAR(*aString))
		     {
		        fallbackDone = PR_TRUE;
		     }
		  }
#ifndef DISABLE_LATIN_FALLBACK
      if (!fallbackDone) 
      {
        fallbackDone = LatinFallbackGetWidth(aString, thisWidth);
        if (fallbackDone)
          segDim.width = thisWidth;
      }
#endif
#ifndef DISABLE_PRECOMPOSEHANGUL_FALLBACK
      if (!fallbackDone)
      {
        fallbackDone = PrecomposeHangulFallbackGetWidth(aString, thisWidth,
                                                        scriptFallbackFonts[smKorean], fontNum);
        if (fallbackDone)
          segDim.width = thisWidth;
      }
#endif
#ifndef DISABLE_TRANSLITERATE_FALLBACK  
      // Fallback to Transliteration
      if (!fallbackDone) 
      {
        fallbackDone = TransliterateFallbackGetWidth(aString, thisWidth);
        if (fallbackDone)
          segDim.width = thisWidth;
      }
#endif
#ifndef DISABLE_UPLUS_FALLBACK  
      // Fallback to UPlus
      if (!fallbackDone)
      {
        fallbackDone = UPlusFallbackGetWidth(aString, thisWidth);
        if (fallbackDone)
          segDim.width = thisWidth;
      }
#endif
        
      // Fallback to question mark
      if (!fallbackDone) 
      {
        QuestionMarkFallbackGetWidth(aString, thisWidth);
        if (fallbackDone)
          segDim.width = thisWidth;
      }
      
      oDim.Combine(segDim);    
      // for fallback measure/drawing, we always do one char a time.
      aString++;
      processLen++;
    }
  } while (processLen < aLength);
    
    // release buffer if it is from heap
  if (heapBuf)
    nsMemory::Free(heapBuf);
      
  return NS_OK;
}
//------------------------------------------------------------------------

#ifdef MOZ_MATHML
nsresult
nsUnicodeRenderingToolkit::GetTextSegmentBoundingMetrics(
      const PRUnichar *aString, PRUint32 aLength,
      short fontNum, nsUnicodeFontMappingMac& fontMapping,
      nsBoundingMetrics& oBoundingMetrics)
{
  oBoundingMetrics.Clear();
  if(aLength == 0) 
    return NS_OK;
  NS_PRECONDITION(BAD_FONT_NUM != fontNum, "illegal font num");
  PRBool firstTime = PR_TRUE;
  PRUint32 processLen = 0;
  nsBoundingMetrics segBoundingMetrics;
  char *heapBuf = nsnull;
  PRUint32 heapBufSize = 0;
  char stackBuf[STACK_TRESHOLD];
  char *buf;
  ByteCount processBytes;
  ByteCount outLen;
  
  ::TextFont(fontNum);
  ScriptCode script = ::FontToScript(fontNum);
  
  // find buf from stack or heap. We only need to do this once in this function.
  // put this out of the loop for performance...
  ByteCount bufLen = aLength * 2 + 10;
  if (bufLen > STACK_TRESHOLD)
  {
    if (bufLen > heapBufSize)
    {
      if (heapBuf)
        nsMemory::Free(heapBuf);
      heapBuf = (char*) nsMemory::Alloc(bufLen);
      heapBufSize = bufLen;
      if (nsnull == heapBuf) 
        return NS_ERROR_OUT_OF_MEMORY;
    } 
    buf = heapBuf;
  } 
  else 
  {
    bufLen = STACK_TRESHOLD;
    buf = stackBuf;
  }

  do {
    outLen = 0;
    processBytes = 0;
        
    if(fontMapping.ConvertUnicodeToGlyphs(fontNum, aString, aLength - processLen,
        buf, bufLen, outLen, processBytes, 0))
    {
        segBoundingMetrics.Clear();
        GetScriptTextBoundingMetrics(buf, outLen, script, segBoundingMetrics);
        
        if(firstTime) {
            firstTime = PR_FALSE;
            oBoundingMetrics = segBoundingMetrics;
        }
        else
            oBoundingMetrics += segBoundingMetrics;
        
        NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");
        
        PRInt32 processUnicode = processBytes / 2;
        processLen += processUnicode;
        aString += processUnicode;
    }
    
    // Cannot process by TEC, process one char a time by fallback mechanism
    if (processLen < aLength)
    {
      PRBool fallbackDone = PR_FALSE;
      segBoundingMetrics.Clear();

      if (IS_HIGH_SURROGATE(*aString) && 
          ((processLen+1) < aLength) &&
          IS_LOW_SURROGATE(*(aString+1)) )
      {
         const nsFont *font;
         mGS->mFontMetrics->GetFont(font);
         fallbackDone = SurrogateGetBoundingMetrics(aString, segBoundingMetrics, fontNum, 
                                                    font->size, 
                                                    (font->weight > NS_FONT_WEIGHT_NORMAL), 
                                                    ((NS_FONT_STYLE_ITALIC ==  font->style) || 
                                                     (NS_FONT_STYLE_OBLIQUE ==  font->style)),
                                                     mGS->mColor );
         if (fallbackDone)
         {      
           if (firstTime) {
             firstTime = PR_FALSE;
             oBoundingMetrics = segBoundingMetrics;
           }
           else
             oBoundingMetrics += segBoundingMetrics;
           aString += 2;
           processLen += 2;
           continue;
         }
      }
#ifndef DISABLE_TEC_FALLBACK
      if (! IS_SKIP_TEC_FALLBACK(*aString))
        fallbackDone = TECFallbackGetBoundingMetrics(aString, segBoundingMetrics, fontNum, fontMapping);
#endif

#ifndef DISABLE_ATSUI_FALLBACK  
      // Fallback by using ATSUI
      if (!fallbackDone)  
      {
        const nsFont *font;
        mGS->mFontMetrics->GetFont(font);
        fallbackDone = ATSUIFallbackGetBoundingMetrics(aString, segBoundingMetrics, fontNum, 
                                                  font->size, 
                                                  (font->weight > NS_FONT_WEIGHT_NORMAL), 
                                                  ((NS_FONT_STYLE_ITALIC ==  font->style) || 
                                                   (NS_FONT_STYLE_OBLIQUE ==  font->style)),
                                                  mGS->mColor );
      }

#endif
      if(! fallbackDone) {
         if(IS_ZERO_WIDTH_CHAR(*aString))
         {
           fallbackDone = PR_TRUE;
         }
      }

      if (firstTime) {
        firstTime = PR_FALSE;
        oBoundingMetrics = segBoundingMetrics;
      }
      else
        oBoundingMetrics += segBoundingMetrics;
      // for fallback measure/drawing, we always do one char a time.
      aString++;
      processLen++;
    }
  } while (processLen < aLength);
  
  // release buffer if it is from heap
  if (heapBuf)
    nsMemory::Free(heapBuf);
  
  return NS_OK;
}
#endif // MOZ_MATHML
//------------------------------------------------------------------------


nsresult nsUnicodeRenderingToolkit :: DrawTextSegment(
			const PRUnichar *aString, PRUint32 aLength, 
			short fontNum, nsUnicodeFontMappingMac& fontMapping, 
			PRInt32 x, PRInt32 y, PRUint32& oWidth)
{
	if(aLength == 0) {
		oWidth = 0;
		return NS_OK;
	}	
 	NS_PRECONDITION(BAD_FONT_NUM != fontNum, "illegal font num");
    short textWidth = 0;
    PRUint32 processLen = 0;
    char *heapBuf = nsnull;
    PRUint32 heapBufSize = 0;
    short thisWidth = 0;
    char stackBuf[STACK_TRESHOLD];
    char *buf ;
    ByteCount processBytes;
    ByteCount outLen;
  	const short *scriptFallbackFonts = fontMapping.GetScriptFallbackFonts();

    ::TextFont(fontNum);
  	
  	// find buf from stack or heap. We only need to do this once in this function.
  	// put this out of the loop for performance...
  	ByteCount bufLen = aLength * 2 + 10;
  	if( bufLen > STACK_TRESHOLD)
  	{
  	 	if(bufLen > heapBufSize )
  	 	{
  	 		if(heapBuf)
  	 			delete[] heapBuf;
  	 		heapBuf = new char [bufLen];
  	 		heapBufSize = bufLen;
  	 		if(nsnull == heapBuf) {
  	 			return NS_ERROR_OUT_OF_MEMORY;
  	 		}  	  	 		
  	 	} 
  	 	buf = heapBuf;
  	 } else {
  	 	bufLen = STACK_TRESHOLD;
  	 	buf = stackBuf;
  	}

    do {
      outLen = 0;
      processBytes = 0;
        
      if(fontMapping.ConvertUnicodeToGlyphs(fontNum, aString, aLength - processLen,
                buf, bufLen, outLen, processBytes, 0))
      {
        DrawScriptText(buf, outLen, x, y, thisWidth);
        
        textWidth += thisWidth;
        x += thisWidth;			
        
        NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");
        
        PRInt32 processUnicode = processBytes / 2;
        processLen += processUnicode;
        aString += processUnicode;
      }
  	  	 
  	  // Cannot precess by TEC, process one char a time by fallback mechanism
  	  if( processLen < aLength)
  	  {
		  PRBool fallbackDone = PR_FALSE;

      if (IS_HIGH_SURROGATE(*aString) && 
          ((processLen+1) < aLength) &&
          IS_LOW_SURROGATE(*(aString+1)) )
      {
         const nsFont *font;
         mGS->mFontMetrics->GetFont(font);
         fallbackDone = SurrogateDrawChar(aString, x, y, thisWidth, fontNum, 
                                          font->size, 
                                          (font->weight > NS_FONT_WEIGHT_NORMAL), 
                                          ((NS_FONT_STYLE_ITALIC ==  font->style) || (NS_FONT_STYLE_OBLIQUE ==  font->style)),
                                          mGS->mColor ); 
         if (fallbackDone)
         {      
           textWidth += thisWidth;
           x += thisWidth;         
           aString += 2;
           processLen += 2;
           continue;
         }
      }
#ifndef DISABLE_TEC_FALLBACK
		  // Fallback by try different Script code
		  if (! IS_SKIP_TEC_FALLBACK(*aString))
  		  fallbackDone = TECFallbackDrawChar(aString, x, y, thisWidth, fontNum, fontMapping);
#endif
		  //
		  // We really don't care too much of performance after this
		  // This will only be called when we cannot display this character in ANY mac script avaliable
		  // 

#ifndef DISABLE_ATSUI_FALLBACK  
		  // Fallback by using ATSUI
		  if(! fallbackDone)  {
		  	const nsFont *font;
			  mGS->mFontMetrics->GetFont(font);
		  	fallbackDone = ATSUIFallbackDrawChar(aString, x, y, thisWidth, fontNum, 
									  		font->size, 
									  		(font->weight > NS_FONT_WEIGHT_NORMAL), 
									  		((NS_FONT_STYLE_ITALIC ==  font->style) || (NS_FONT_STYLE_OBLIQUE ==  font->style)),
									  		mGS->mColor );
		  }
#endif
		  if(! fallbackDone) {
		     if(IS_ZERO_WIDTH_CHAR(*aString))
		     {
		        thisWidth = 0;
		        fallbackDone = PR_TRUE;
		     }
		  }
      
#ifndef DISABLE_LATIN_FALLBACK
		  if(! fallbackDone)
		  	fallbackDone = LatinFallbackDrawChar(aString, x, y, thisWidth);
#endif
#ifndef DISABLE_PRECOMPOSEHANGUL_FALLBACK
		  if(! fallbackDone)
		  	fallbackDone = PrecomposeHangulFallbackDrawChar(aString, x, y, thisWidth,scriptFallbackFonts[smKorean], fontNum);
#endif
#ifndef DISABLE_TRANSLITERATE_FALLBACK  
		  // Fallback to Transliteration
		  if(! fallbackDone) {
		  	fallbackDone = TransliterateFallbackDrawChar(aString, x, y, thisWidth);
		  }
#endif
#ifndef DISABLE_UPLUS_FALLBACK  
		  // Fallback to U+xxxx
		  if(! fallbackDone)
		  	fallbackDone = UPlusFallbackDrawChar(aString, x, y, thisWidth);
#endif
		  	
		  // Fallback to question mark
		  if(! fallbackDone)
		  	QuestionMarkFallbackDrawChar(aString, x, y, thisWidth);
		  	
		  textWidth += thisWidth;
   	  	  x += thisWidth;
		  
		  // for fallback measure/drawing, we always do one char a time.
	  	  aString++;
	  	  processLen++;
	  }
    } while (processLen < aLength);
    
    // release buffer if it is from heap
    if(heapBuf)
    	delete[] heapBuf;
    	
    oWidth = textWidth;
    return NS_OK;
}
//------------------------------------------------------------------------

nsresult 
nsUnicodeRenderingToolkit::GetWidth(const PRUnichar *aString, PRUint32 aLength, 
                                    nscoord &aWidth, PRInt32 *aFontID)
{
  nsTextDimensions dim;

  nsresult res = GetTextDimensions(aString, aLength, dim, aFontID);
  aWidth = dim.width;
  return res;
}

nsresult 
nsUnicodeRenderingToolkit::GetTextDimensions(const PRUnichar *aString, PRUint32 aLength, 
                                              nsTextDimensions &aDim, PRInt32 *aFontID)
{
  nsresult res = NS_OK;
  nsFontMetricsMac *metrics = (nsFontMetricsMac*) mGS->mFontMetrics;
  nsUnicodeFontMappingMac* fontmap = metrics->GetUnicodeFontMapping();

  PRUint32 i;
  short fontNum[2];
  fontNum[0] = fontNum[1] = BAD_FONT_NUM;
  PRUint32 start;
    
  nsTextDimensions textDim;
  nsTextDimensions thisDim;
  
  for(i =0, start = 0; i < aLength; i++)
  {
    fontNum[ i % 2] = fontmap->GetFontID(aString[i]);
    if ((fontNum[0] != fontNum[1]) && (0 != i))
    {  // start new font run...
      thisDim.Clear();
      res =  GetTextSegmentDimensions(aString+start, i - start, fontNum[(i + 1) % 2], *fontmap, thisDim);
      if (NS_FAILED (res)) 
        return res;    
      textDim.Combine(thisDim);
      start = i;
    }
  }
  res = GetTextSegmentDimensions(aString+start, aLength - start, fontNum[(i + 1) % 2], *fontmap, thisDim);
  if (NS_FAILED (res)) 
    return res;    
  textDim.Combine(thisDim);

  aDim.width = NSToCoordRound(float(textDim.width) * mP2T);
  aDim.ascent = NSToCoordRound(float(textDim.ascent) * mP2T);
  aDim.descent = NSToCoordRound(float(textDim.descent) * mP2T);
  return res;  
}
//------------------------------------------------------------------------

#ifdef MOZ_MATHML
nsresult
nsUnicodeRenderingToolkit::GetTextBoundingMetrics(const PRUnichar *aString, PRUint32 aLength,
                                                  nsBoundingMetrics &aBoundingMetrics, PRInt32 *aFontID)
{
  nsresult res = NS_OK;
  nsFontMetricsMac *metrics = (nsFontMetricsMac*) mGS->mFontMetrics;
  nsUnicodeFontMappingMac* fontmap = metrics->GetUnicodeFontMapping();

  PRUint32 i;
  short fontNum[2];
  fontNum[0] = fontNum[1] = BAD_FONT_NUM;
  PRUint32 start;
  PRBool firstTime = PR_TRUE;
  nsBoundingMetrics thisBoundingMetrics;

  for(i =0, start = 0; i < aLength; i++)
  {
    fontNum[ i % 2] = fontmap->GetFontID(aString[i]);
    if ((fontNum[0] != fontNum[1]) && (0 != i))
    {  // start new font run...
      res = GetTextSegmentBoundingMetrics(aString+start, i - start, fontNum[(i + 1) % 2], *fontmap, thisBoundingMetrics);
      if (NS_FAILED (res))
        return res;
      if (firstTime) {
        firstTime = PR_FALSE;
        aBoundingMetrics = thisBoundingMetrics;
      }
      else
        aBoundingMetrics += thisBoundingMetrics;
      start = i;
    }
  }
  res = GetTextSegmentBoundingMetrics(aString+start, aLength - start, fontNum[(i + 1) % 2], *fontmap, thisBoundingMetrics);
  if (NS_FAILED (res))
    return res;
  if (firstTime)
    aBoundingMetrics = thisBoundingMetrics;
  else
    aBoundingMetrics += thisBoundingMetrics;

  aBoundingMetrics.leftBearing = NSToCoordRound(float(aBoundingMetrics.leftBearing) * mP2T);
  aBoundingMetrics.rightBearing = NSToCoordRound(float(aBoundingMetrics.rightBearing) * mP2T);
  aBoundingMetrics.ascent = NSToCoordRound(float(aBoundingMetrics.ascent) * mP2T);
  aBoundingMetrics.descent = NSToCoordRound(float(aBoundingMetrics.descent) * mP2T);
  aBoundingMetrics.width = NSToCoordRound(float(aBoundingMetrics.width) * mP2T);

  return res;
}
#endif // MOZ_MATHML

//------------------------------------------------------------------------

nsresult
nsUnicodeRenderingToolkit::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
  nsresult res = NS_OK;
  nsFontMetricsMac *metrics = (nsFontMetricsMac*) mGS->mFontMetrics;
  nsUnicodeFontMappingMac* fontmap = metrics->GetUnicodeFontMapping();

  PRInt32 x = aX;
  PRInt32 transformedY = aY;
  mGS->mTMatrix.TransformCoord(&x, &transformedY);

  PRUint32 i;
  PRInt32 currentX = aX;
  PRUint32 thisWidth = 0;
	
  if (aSpacing)
  {
    // fix me ftang -  handle (mRightToLeftText) here
    for (i = 0; i < aLength; )
    {
      PRUint32 drawLen;
      short curFontNum = fontmap->GetFontID(aString[i]);

      for (drawLen = 1; (i + drawLen) < aLength; drawLen++)
      {
        PRUnichar uc = aString[i+drawLen];
      	if(! (IS_CONTEXTUAL_CHARS(uc) || 
      		    IS_FORMAT_CONTROL_CHARS(uc) ||
      	      IS_COMBINING_CHARS(uc)) ) {
      	  break;
      	}
      }

      PRInt32 transformedX = currentX, ignoreY = 0;
      mGS->mTMatrix.TransformCoord(&transformedX, &ignoreY);
      res = DrawTextSegment(aString+i, drawLen, curFontNum, *fontmap, transformedX, transformedY, thisWidth);
	    if (NS_FAILED(res))
	 		  return res;
      
      for (PRUint32 j = 0; j < drawLen; j++)
	   	  currentX += aSpacing[i + j];
	   	  
	   	i += drawLen;
    }
  }
  else    // no spacing array
  {
    short thisFont, nextFont;
    
    PRUint32 start;

      // normal left to right
        thisFont = fontmap->GetFontID(aString[0]);
	    for (i = 1, start = 0; i < aLength; i++)
	    {
	    	PRUnichar uch = aString[i];
	    	if(! IS_FORMAT_CONTROL_CHARS(uch))
	    	{
	    		nextFont = fontmap->GetFontID(uch);
	    		if (thisFont != nextFont) 
	        {
	          // start new font run...
	          PRInt32 transformedX = currentX, ignoreY = 0;
	          mGS->mTMatrix.TransformCoord(&transformedX, &ignoreY);
	          
            res = DrawTextSegment(aString + start, i - start, thisFont, *fontmap, transformedX, transformedY, thisWidth);
	    	  	if (NS_FAILED(res))
	    	 		  return res;
	    	 		
	    		  currentX += NSToCoordRound(float(thisWidth) * mP2T);
	    		  start = i;
	    		  thisFont = nextFont;
	    		}
	    	}
	    }

	    PRInt32 transformedX = currentX, ignoreY = 0;
	    mGS->mTMatrix.TransformCoord(&transformedX, &ignoreY);
      res = DrawTextSegment(aString+start, aLength-start, thisFont, *fontmap, transformedX, transformedY, thisWidth);
    if (NS_FAILED(res))
      return res;
  }
  
	return NS_OK;
}


//------------------------------------------------------------------------
nsresult
nsUnicodeRenderingToolkit::PrepareToDraw(float aP2T, nsIDeviceContext* aContext,
                                         nsGraphicState* aGS, 
                                         CGrafPtr aPort, PRBool aRightToLeftText )
{
	mP2T = aP2T;
	mContext = aContext;
	mGS = aGS;
	mPort = aPort;
	mRightToLeftText = aRightToLeftText;
	return NS_OK;
}
