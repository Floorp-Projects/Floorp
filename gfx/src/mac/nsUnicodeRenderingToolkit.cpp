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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
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
	
	if( cache->Get(*aCharPt, fallbackScript))
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
	if( cache->Get(*aCharPt, fallbackScript))
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

PRBool nsUnicodeRenderingToolkit :: ATSUIFallbackGetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth, 
	short origFontNum,
	short aSize, PRBool aBold, PRBool aItalic, nscolor aColor) 
{
	if (nsATSUIUtils::IsAvailable()  
	    &&  (IN_STANDARD_MAC_ROMAN_FONT(*aCharPt) ||IN_SYMBOL_FONT(*aCharPt)))
	{
		mATSUIToolkit.PrepareToDraw(mPort, mContext );
		nsresult res = mATSUIToolkit.GetWidth(aCharPt, oWidth, aSize, 
													origFontNum, 
													aBold, aItalic, aColor);
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
	   &&  (IN_STANDARD_MAC_ROMAN_FONT(*aCharPt) ||IN_SYMBOL_FONT(*aCharPt)))
	{
		mATSUIToolkit.PrepareToDraw(mPort, mContext );
		nsresult res = mATSUIToolkit.DrawString(aCharPt, x, y, oWidth, aSize, 
													origFontNum, 
													aBold, aItalic, aColor);
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
    if(NS_SUCCEEDED(mTrans->Convert(tmp.GetUnicode(), &conv)) && conv) {
  		GetScriptTextWidth(conv, nsCRT::strlen(conv),oWidth);    
    	nsAllocator::Free(conv);
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
    if(NS_SUCCEEDED(mTrans->Convert(tmp.GetUnicode(), &conv)) && conv) {
  		DrawScriptText(conv, nsCRT::strlen(conv), x, y, oWidth);
    	nsAllocator::Free(conv);
    	return PR_TRUE;
    }
  }
  return PR_FALSE;
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
 	oWidth = ::TextWidth(buf, 0, aLen);
    ::MoveTo(x, y);
	::DrawText(buf,0,aLen);
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
#ifndef DISABLE_TRANSLITERATE_FALLBACK  
		  // Fallback to Transliteration
		  if(! fallbackDone)
		  	fallbackDone = TransliterateFallbackGetWidth(aString, thisWidth);
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
  	  	   	  	 
  	  	 // no mater if failed or not, as long as it convert some text, we process it.
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
#ifndef DISABLE_TRANSLITERATE_FALLBACK  
		  // Fallback to Transliteration
		  if(! fallbackDone)
		  	fallbackDone = TransliterateFallbackDrawChar(aString, x, y, thisWidth);
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
#define IS_CONTEXTUAL_CHARS(c) 		((0x0600<=(c))&&((c)<0x1000))
#define IS_COMBINING_CHARS(c) 		((0x0300<=(c))&&((c)<0x0370))
//------------------------------------------------------------------------
NS_IMETHODIMP nsUnicodeRenderingToolkit :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
 	nsresult res = NS_OK;
    nsFontMetricsMac *metrics = (nsFontMetricsMac*) mGS->mFontMetrics;
    nsUnicodeFontMappingMac* fontmap = metrics->GetUnicodeFontMapping();
    	
	PRInt32 x = aX;
	PRInt32 y = aY;	
	
	nscoord ascent = 0;
	mGS->mFontMetrics->GetMaxAscent(ascent);
	y += ascent;

    mGS->mTMatrix.TransformCoord(&x,&y);

    PRUint32 i;
	PRInt32 currentX = x;
	PRUint32 thisWidth = 0;
    const short *scriptFallbackFonts = fontmap->GetScriptFallbackFonts();
	
    if(aSpacing) {
		int buffer[STACK_TREASHOLD];
		int* spacing = (aLength <= STACK_TREASHOLD ? buffer : new int[aLength]);
		if (spacing)
		{
			mGS->mTMatrix.ScaleXCoords(aSpacing, aLength, spacing);
		    for(i =0; i < aLength; )
		    {
		       PRUint32 j,drawLen;
		       short curFontNum = fontmap->GetFontID(aString[i]);
		       for(drawLen = 1; ((i+drawLen) < aLength);drawLen++) {
		       	    PRUnichar uc = aString[i+drawLen];
		       		if(! ( IS_CONTEXTUAL_CHARS(uc) || 
		       			   IS_FORMAT_CONTROL_CHARS(uc) ||
		       		       IS_COMBINING_CHARS(uc)) ) {
		       		       break;
		       		}
		       }
		       res = DrawTextSegment(aString+i, drawLen, curFontNum, scriptFallbackFonts, currentX, y, thisWidth);
			   if(NS_FAILED(res)) {
			    	if (spacing != buffer)
						delete[] spacing;
			 		goto end_of_func;
		       }
		       for(j=0;j<drawLen;j++)
			   		currentX += spacing[i++];
		    }
	    	if (spacing != buffer)
				delete[] spacing;
		}
		else {
			res =  NS_ERROR_OUT_OF_MEMORY;	
 			goto end_of_func;
		}
    } else {
    	short thisFont, nextFont;
    	thisFont=fontmap->GetFontID(aString[0]);;	
    	PRUint32 start;
    	for(i =1, start=0; i < aLength; i++)
    	{
    		PRUnichar uch = aString[i];
    		if(! IS_FORMAT_CONTROL_CHARS(uch))
    		{
    			nextFont = fontmap->GetFontID(uch);
    			if(thisFont != nextFont) 
		        {  // start new font run...
		       
		       		res = DrawTextSegment(aString+start, i-start, thisFont, scriptFallbackFonts, 
		          						currentX, y, thisWidth);
				  	if(NS_FAILED(res))
				 		goto end_of_func;
					currentX += thisWidth;
					start = i;
					thisFont = nextFont;
    			}
    		}
	    }

    	res = DrawTextSegment(aString+start, aLength-start, thisFont, 
    							scriptFallbackFonts, currentX, y, thisWidth);
		if(NS_FAILED(res))
			goto end_of_func;
    }
    
end_of_func:

	return res;        
}
//------------------------------------------------------------------------
NS_IMETHODIMP nsUnicodeRenderingToolkit :: PrepareToDraw(float aP2T, nsIDeviceContext* aContext, nsGraphicState* aGS, GrafPtr aPort)
{
	mP2T = aP2T;
	mContext = aContext;
	mGS = aGS;
	mPort = aPort;
	return NS_OK;
}
