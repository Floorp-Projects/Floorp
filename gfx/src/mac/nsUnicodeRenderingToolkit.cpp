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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#define BAD_FONT_NUM -1
#define BAD_SCRIPT 0x7F
#define STACK_TREASHOLD 1000
static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);

//#define DISABLE_TEC_FALLBACK
//#define DISABLE_PRECOMPOSEHANGUL_FALLBACK
//#define DISABLE_LATINL_FALLBACK
//#define DISABLE_ATSUI_FALLBACK
//#define DISABLE_TRANSLITERATE_FALLBACK
//#define DISABLE_UPLUS_FALLBACK


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

//------------------------------------------------------------------------
static UnicodeToTextInfo gConverters[32] = { 
	nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull,
	nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull,
	nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull,
	nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull
};
//------------------------------------------------------------------------
UnicodeToTextInfo nsUnicodeRenderingToolkit :: GetConverterByScript(ScriptCode sc)
{
	NS_PRECONDITION(sc < 32, "illegal script id");
    if(sc >= 32)
      return nsnull;
    if(gConverters[sc] != nsnull) {
      return gConverters[sc];
    }
    OSStatus err = noErr;
    
    //
    TextEncoding scriptEncoding;
    err = ::UpgradeScriptInfoToTextEncoding(sc, kTextLanguageDontCare, kTextRegionDontCare, nsnull, &scriptEncoding);
    if( noErr == err ) 
  		err = ::CreateUnicodeToTextInfoByEncoding(scriptEncoding, &gConverters[sc] );

    if(noErr != err) 
    	gConverters[sc] = nsnull;
   	return gConverters[sc];
}
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
PRBool nsUnicodeRenderingToolkit :: TECFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth, 
	short origFontNum, 
	const short* scriptFallbackFonts)
{
	OSStatus err = noErr;
	char buf[20];
	ByteCount processBytes = 0;
	ByteCount outLen = 0;
	ScriptCode fallbackScript;
	nsUnicodeFallbackCache* cache = GetTECFallbackCache();
	
	if ((0xf780 <= *aCharPt) && (*aCharPt <= 0xf7ff))
	{
		// If we are encountering our PUA characters for User-Defined characters, we better
		// just drop the high-byte and return the width for the low-byte.
		*buf = (*aCharPt & 0x00FF);
		GetScriptTextWidth (buf,1,oWidth);

		return PR_TRUE;
	}
	else if( cache->Get(*aCharPt, fallbackScript))
	{
		if(BAD_SCRIPT == fallbackScript)
			return PR_FALSE;
			
		UnicodeToTextInfo fallbackConverter = GetConverterByScript(fallbackScript);
		if( fallbackConverter ) {
	  	  	 err = ::ConvertFromUnicodeToText(fallbackConverter, (ByteCount)(2), (ConstUniCharArrayPtr) aCharPt, 
	 						kUnicodeLooseMappingsMask , 0, NULL, 0, NULL,
	 						STACK_TREASHOLD, &processBytes, &outLen, 
	                        (LogicalAddress)buf);
	        if( outLen > 0) {
		  	  	::TextFont(scriptFallbackFonts[fallbackScript]);
		  	  	GetScriptTextWidth(buf, outLen, oWidth);
				::TextFont(origFontNum);
			}
		}
		return PR_TRUE;
	}
	
	for( fallbackScript = 0 ; fallbackScript < 32; fallbackScript++)
	{
		if(BAD_FONT_NUM != scriptFallbackFonts[fallbackScript])
		{
			UnicodeToTextInfo fallbackConverter = GetConverterByScript(fallbackScript);
			if( fallbackConverter ) {
		  	  	 err = ::ConvertFromUnicodeToText(fallbackConverter, (ByteCount)(2), (ConstUniCharArrayPtr) aCharPt, 
		 						kUnicodeLooseMappingsMask , 0, NULL, 0, NULL,
		 						STACK_TREASHOLD, &processBytes, &outLen, 
		                        (LogicalAddress)buf);
		  	  	 if(outLen > 0)
		  	  	 {
					NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");
		  	  	 	::TextFont(scriptFallbackFonts[fallbackScript]);
		  	  	 	GetScriptTextWidth(buf, outLen, oWidth);
					::TextFont(origFontNum);				
					break;
		   	  	 }			  	  	                        
			}  	  		
		}
	}
	
	if( 0 == outLen )
		fallbackScript = BAD_SCRIPT;
		
	// put into cache
	cache->Set(*aCharPt, fallbackScript);
	
	return (BAD_SCRIPT != fallbackScript);
}
//------------------------------------------------------------------------
PRBool nsUnicodeRenderingToolkit :: TECFallbackDrawChar(
	const PRUnichar *aCharPt, 
	PRInt32 x, 
	PRInt32 y, 
	short& oWidth, 
	short origFontNum, 
	const short* scriptFallbackFonts)
{
	OSStatus err = noErr;
	char buf[20];
	ByteCount processBytes = 0;
	ByteCount outLen = 0;
	ScriptCode fallbackScript;
	nsUnicodeFallbackCache* cache = GetTECFallbackCache();
	
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
	else if( cache->Get(*aCharPt, fallbackScript))
	{
		if(BAD_SCRIPT == fallbackScript)
			return PR_FALSE;
			
		UnicodeToTextInfo fallbackConverter = GetConverterByScript(fallbackScript);
		if( fallbackConverter ) {
	  	  	 err = ::ConvertFromUnicodeToText(fallbackConverter, (ByteCount)(2), (ConstUniCharArrayPtr) aCharPt, 
	 						kUnicodeLooseMappingsMask , 0, NULL, 0, NULL,
	 						STACK_TREASHOLD, &processBytes, &outLen, 
	                        (LogicalAddress)buf);
	        if( outLen > 0) {
		  	  	::TextFont(scriptFallbackFonts[fallbackScript]);
		  	  	DrawScriptText(buf, outLen, x, y, oWidth);
				::TextFont(origFontNum);
			}
		}
		return PR_TRUE;
	}
	return PR_FALSE;
}
//------------------------------------------------------------------------
static PRUnichar gSymbolReplacement[]={0xf8ee,0xf8f9,0xf8f0,0xf8fb,0x3008,0x3009};

PRBool nsUnicodeRenderingToolkit :: ATSUIFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth, 
	short origFontNum,
	short aSize, PRBool aBold, PRBool aItalic, nscolor aColor) 
{
	if (nsATSUIUtils::IsAvailable()  
	    &&  (IN_STANDARD_MAC_ROMAN_FONT(*aCharPt) ||IN_SYMBOL_FONT(*aCharPt)||SPECIAL_IN_SYMBOL_FONT(*aCharPt)))
	{
		mATSUIToolkit.PrepareToDraw(mPort, mContext );
		nsresult res;
		if(SPECIAL_IN_SYMBOL_FONT(*aCharPt)) {
			 short rep = 0;
			 if((*aCharPt) > 0x230b)
			 	rep = (*aCharPt) - 0x2325;
			 else 
			 	rep = (*aCharPt) - 0x2308;
			 res = mATSUIToolkit.GetWidth(gSymbolReplacement+rep, oWidth, aSize, 
													origFontNum, 
													aBold, aItalic, aColor);
		} else {
			 res = mATSUIToolkit.GetWidth(aCharPt, oWidth, aSize, 
													origFontNum, 
													aBold, aItalic, aColor);
		}
		if(NS_SUCCEEDED(res))
			return PR_TRUE;
	}
	return PR_FALSE;
}
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: ATSUIFallbackDrawChar(
	const PRUnichar *aCharPt, 
	PRInt32 x, 	PRInt32 y, 
	short& oWidth, 
	short origFontNum,
	short aSize, PRBool aBold, PRBool aItalic, nscolor aColor) 
{
	if (nsATSUIUtils::IsAvailable()
	   &&  (IN_STANDARD_MAC_ROMAN_FONT(*aCharPt) ||IN_SYMBOL_FONT(*aCharPt)||SPECIAL_IN_SYMBOL_FONT(*aCharPt)))
	{
		mATSUIToolkit.PrepareToDraw(mPort, mContext );
		nsresult res;
		if(SPECIAL_IN_SYMBOL_FONT(*aCharPt)) {
			 short rep = 0;
			 if((*aCharPt) > 0x230b)
			 	rep = (*aCharPt) - 0x2325;
			 else 
			 	rep = (*aCharPt) - 0x2308;
			res = mATSUIToolkit.DrawString(gSymbolReplacement+rep, x, y, oWidth, aSize, 
													origFontNum, 
													aBold, aItalic, aColor);
		} else {
			res = mATSUIToolkit.DrawString(aCharPt, x, y, oWidth, aSize, 
													origFontNum, 
													aBold, aItalic, aColor);
		}
		if(NS_SUCCEEDED(res))
			return PR_TRUE;
	}
	return PR_FALSE;
}
static char *question = "<?>";
//------------------------------------------------------------------------

PRBool nsUnicodeRenderingToolkit :: QuestionMarkFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth)
{
  GrafPtr thePort;
  ::GetPort(&thePort);
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
  GrafPtr thePort;
  ::GetPort(&thePort);
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
    if ( NS_SUCCEEDED(nsComponentManager::CreateInstance(
    	kSaveAsCharsetCID, 0, NS_GET_IID(nsISaveAsCharset), 
        getter_AddRefs(mTrans) ) ) )
    {
       res = mTrans->Init("x-mac-roman",
               nsISaveAsCharset::attr_FallbackQuestionMark +
               nsISaveAsCharset::attr_EntityBeforeCharsetConv,
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
	    GrafPtr thePort;
	    ::GetPort(&thePort);
	    short aSize = ::GetPortTextSize(thePort);		 		
  		PRInt32 l=nsCRT::strlen(conv);
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
	    GrafPtr thePort;
	    ::GetPort(&thePort);
	    short aSize = ::GetPortTextSize(thePort);		
    	PRInt32 l=nsCRT::strlen(conv);
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
  GrafPtr thePort;
  ::GetPort(&thePort);
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
  GrafPtr thePort;
  ::GetPort(&thePort);
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
static char *g1E00Dec = 
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
	    GrafPtr thePort;
	    ::GetPort(&thePort);
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
	
	Point		penLoc;
	::GetPen(&penLoc);
	oWidth = penLoc.h - x;
}
//------------------------------------------------------------------------

nsresult nsUnicodeRenderingToolkit :: GetTextSegmentWidth(
			const PRUnichar *aString, PRUint32 aLength, 
			short fontNum, const short *scriptFallbackFonts, 
			PRUint32& oWidth)
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
    char stackBuf[STACK_TREASHOLD];
    char *buf ;
    ByteCount processBytes;
    ByteCount outLen;
  	OSStatus err = noErr;

    ::TextFont(fontNum);
    ScriptCode script = ::FontToScript(fontNum);
    UnicodeToTextInfo converter = GetConverterByScript(script);
  	
  	// find buf from stack or heap. We only need to do this once in this function.
  	// put this out of the loop for performance...
  	ByteCount bufLen = aLength * 2 + 10;
  	if( bufLen > STACK_TREASHOLD)
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
  	 	bufLen = STACK_TREASHOLD;
  	 	buf = stackBuf;
  	}

    do {
  	  if(converter)
  	  {
	  	outLen = 0;
 	  	err = noErr;
  	  	processBytes = 0;
  	  	err = ::ConvertFromUnicodeToText(converter, (ByteCount)(2*(aLength - processLen)), (ConstUniCharArrayPtr) aString, 
  	  	 						0 , 0, NULL, 0, NULL,
  	  	 						bufLen, &processBytes, &outLen, 
  	  	                        (LogicalAddress)buf);
  	  	   	  	 
  	  	 // no mater if failed or not, as long as it convert some text, we process it.
  	  	 if(outLen > 0)
  	  	 {
			GetScriptTextWidth(buf, outLen, thisWidth);
			
			textWidth += thisWidth;

 	  	 	NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");

  	  	 	PRInt32 processUnicode = processBytes / 2;
  	  	 	processLen += processUnicode;
  	  	 	aString += processUnicode;
  	  	 }
  	  	 
  	  }
  	  // Cannot precess by TEC, process one char a time by fallback mechanism
  	  if( processLen < aLength)
  	  {
		  PRBool fallbackDone = PR_FALSE;
#ifndef DISABLE_TEC_FALLBACK
		  // Fallback by try different Script code
		 fallbackDone = TECFallbackGetWidth(aString, thisWidth, fontNum, scriptFallbackFonts);
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
		  	fallbackDone = ATSUIFallbackGetWidth(aString, thisWidth, fontNum, 
									  		font->size, 
									  		(font->weight > NS_FONT_WEIGHT_NORMAL), 
									  		((NS_FONT_STYLE_ITALIC ==  font->style) || (NS_FONT_STYLE_OBLIQUE ==  font->style)),
									  		mGS->mColor );
		  }
#endif
#ifndef DISABLE_LATIN_FALLBACK
		  if(! fallbackDone)
		  	fallbackDone = LatinFallbackGetWidth(aString, thisWidth);
#endif
#ifndef DISABLE_PRECOMPOSEHANGUL_FALLBACK
		  if(! fallbackDone)
		  	fallbackDone = PrecomposeHangulFallbackGetWidth(aString, thisWidth,scriptFallbackFonts[smKorean], fontNum);
#endif
#ifndef DISABLE_TRANSLITERATE_FALLBACK  
		  // Fallback to Transliteration
		  if(! fallbackDone) {
		  	fallbackDone = TransliterateFallbackGetWidth(aString, thisWidth);
		  }
#endif
#ifndef DISABLE_UPLUS_FALLBACK  
		  // Fallback to UPlus
		  if(! fallbackDone)
		  	fallbackDone = UPlusFallbackGetWidth(aString, thisWidth);
#endif
		  	
		  // Fallback to question mark
		  if(! fallbackDone)
		  	QuestionMarkFallbackGetWidth(aString, thisWidth);
		  	
		  textWidth += thisWidth;
		  
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

nsresult nsUnicodeRenderingToolkit :: DrawTextSegment(
			const PRUnichar *aString, PRUint32 aLength, 
			short fontNum, const short *scriptFallbackFonts, 
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
    char stackBuf[STACK_TREASHOLD];
    char *buf ;
    ByteCount processBytes;
    ByteCount outLen;
  	OSStatus err = noErr;

    ::TextFont(fontNum);
    ScriptCode script = ::FontToScript(fontNum);
    UnicodeToTextInfo converter = GetConverterByScript(script);
  	
  	// find buf from stack or heap. We only need to do this once in this function.
  	// put this out of the loop for performance...
  	ByteCount bufLen = aLength * 2 + 10;
  	if( bufLen > STACK_TREASHOLD)
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
  	 	bufLen = STACK_TREASHOLD;
  	 	buf = stackBuf;
  	}

    do {
  	  if(converter)
  	  {
	  	outLen = 0;
 	  	err = noErr;
  	  	processBytes = 0;
  	  	err = ::ConvertFromUnicodeToText(converter, (ByteCount)(2*(aLength - processLen)), (ConstUniCharArrayPtr) aString, 
  	  	 						0 , 0, NULL, 0, NULL,
  	  	 						bufLen, &processBytes, &outLen, 
  	  	                        (LogicalAddress)buf);
  	  	   	  	 
  	  	 // no matter if failed or not, as long as it convert some text, we process it.
  	  	 if(outLen > 0)
  	  	 {
			DrawScriptText(buf, outLen, x, y, thisWidth);
			
			textWidth += thisWidth;
  		    x += thisWidth;			

 	  	 	NS_PRECONDITION(0 == (processBytes % 2), "strange conversion result");

  	  	 	PRInt32 processUnicode = processBytes / 2;
  	  	 	processLen += processUnicode;
  	  	 	aString += processUnicode;
  	  	 }
  	  	 
  	  }
  	  // Cannot precess by TEC, process one char a time by fallback mechanism
  	  if( processLen < aLength)
  	  {
		  PRBool fallbackDone = PR_FALSE;

#ifndef DISABLE_TEC_FALLBACK
		  // Fallback by try different Script code
		  fallbackDone = TECFallbackDrawChar(aString, x, y, thisWidth, fontNum, scriptFallbackFonts);
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

NS_IMETHODIMP nsUnicodeRenderingToolkit :: GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID)
{
 	nsresult res = NS_OK;
    nsFontMetricsMac *metrics = (nsFontMetricsMac*) mGS->mFontMetrics;
    nsUnicodeFontMappingMac* fontmap = metrics->GetUnicodeFontMapping();

    PRUint32 i;
    short fontNum[2];
    fontNum[0] = fontNum[1] = BAD_FONT_NUM;
    PRUint32 start;
    
    PRUint32 textWidth = 0;
    PRUint32 thisWidth = 0;
    const short *scriptFallbackFonts = fontmap->GetScriptFallbackFonts();
    for(i =0, start = 0; i < aLength; i++)
    {
       fontNum[ i % 2] = fontmap->GetFontID(aString[i]);
       if((fontNum[0] != fontNum[1]) && (0 != i))
       {  // start new font run...
       	  PRUint32 thisWidth = 0;
       	  res =  GetTextSegmentWidth(aString+start, i-start, fontNum[ (i+1) % 2], scriptFallbackFonts, thisWidth);
       	  if(NS_FAILED (res)) 
 				return res;		
          textWidth += thisWidth;
          start = i;
       }
    }
	res = GetTextSegmentWidth(aString+start, aLength-start, fontNum[ (i+1) % 2], scriptFallbackFonts, thisWidth);
    if(NS_FAILED (res)) 
		return res;		
    textWidth += thisWidth;	

    aWidth = NSToCoordRound(float(textWidth) * mP2T);
	return res;  
}

#define IS_FORMAT_CONTROL_CHARS(c) 	((0x2000==((c)&0xFFF0))||(0x2028==((c)&0xFFF8)))
#define IS_CONTEXTUAL_CHARS(c) 		  ((0x0600<=(c))&&((c)<0x1000))
#define IS_COMBINING_CHARS(c) 		  ((0x0300<=(c))&&((c)<0x0370))
//------------------------------------------------------------------------
NS_IMETHODIMP nsUnicodeRenderingToolkit :: DrawString(const PRUnichar *aString, PRUint32 aLength,
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
  const short *scriptFallbackFonts = fontmap->GetScriptFallbackFonts();
	
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
      res = DrawTextSegment(aString+i, drawLen, curFontNum, scriptFallbackFonts, transformedX, transformedY, thisWidth);
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
    thisFont = fontmap->GetFontID(aString[0]);
    PRUint32 start;

    if (mRightToLeftText) {
      // right to left
	    for (i = 1, start = 0; i < aLength; i++)
	    {
	      PRUnichar  uch = aString[aLength - i - 1];
	    	if(! IS_FORMAT_CONTROL_CHARS(uch))
	    	{
	    		nextFont = fontmap->GetFontID(uch);
	    		if (thisFont != nextFont) 
	        {
	          // start new font run...
	          PRInt32 transformedX = currentX, ignoreY = 0;
	          mGS->mTMatrix.TransformCoord(&transformedX, &ignoreY);
	          
            res = DrawTextSegment(aString + aLength - i, i-start, thisFont, scriptFallbackFonts, transformedX, transformedY, thisWidth);
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
      res = DrawTextSegment(aString , aLength-start, thisFont, scriptFallbackFonts, transformedX, transformedY, thisWidth);
    } else { 
      // normal left to right
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
	          
            res = DrawTextSegment(aString + start, i - start, thisFont, scriptFallbackFonts, transformedX, transformedY, thisWidth);
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
      res = DrawTextSegment(aString+start, aLength-start, thisFont, scriptFallbackFonts, transformedX, transformedY, thisWidth);
    }
    if (NS_FAILED(res))
      return res;
  }
  
	return NS_OK;
}


//------------------------------------------------------------------------
NS_IMETHODIMP nsUnicodeRenderingToolkit :: PrepareToDraw(float aP2T, nsIDeviceContext* aContext, nsGraphicState* aGS, 
#ifdef IBMBIDI
GrafPtr aPort, PRBool aRightToLeftText
#else
GrafPtr aPort
#endif
)
{
	mP2T = aP2T;
	mContext = aContext;
	mGS = aGS;
	mPort = aPort;
#ifdef IBMBIDI
	mRightToLeftText = aRightToLeftText;
#endif
	return NS_OK;
}
