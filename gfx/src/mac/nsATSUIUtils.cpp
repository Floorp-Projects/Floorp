/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsATSUIUtils.h"
#include "nsIDeviceContext.h"
#include "nsDrawingSurfaceMac.h"
#include "nsTransform2D.h"
#include "plhash.h"
#include <Gestalt.h>
#include <FixMath.h>


//------------------------------------------------------------------------
//	ATSUILayoutCache
//
//------------------------------------------------------------------------


//--------------------------------
// Class definition
//--------------------------------
class ATSUILayoutCache
{
public:
	ATSUILayoutCache();
	~ATSUILayoutCache();

	PRBool	Get(short aFont, short aSize, PRBool aBold, PRBool aItalic, nscolor aColor, ATSUTextLayout *aTxlayout);
	void	Set(short aFont, short aSize, PRBool aBold, PRBool aItalic, nscolor aColor, ATSUTextLayout aTxlayout);

private:
	typedef struct
	{
		short	font;
		short	size;
		nscolor	color;
		short	boldItalic;
	} atsuiLayoutCacheKey;

	PRBool	Get(atsuiLayoutCacheKey *key, ATSUTextLayout *txlayout);
	void	Set(atsuiLayoutCacheKey *key, ATSUTextLayout txlayout);

	static PR_CALLBACK PLHashNumber HashKey(const void *aKey);
	static PR_CALLBACK PRIntn		CompareKeys(const void *v1, const void *v2);
	static PR_CALLBACK PRIntn		CompareValues(const void *v1, const void *v2);
	static PR_CALLBACK PRIntn		FreeHashEntries(PLHashEntry *he, PRIntn italic, void *arg);

	struct PLHashTable*		mTable;
	PRUint32				mCount;
};


ATSUILayoutCache::ATSUILayoutCache()
{
	mTable = PL_NewHashTable(8, (PLHashFunction)HashKey, 
								(PLHashComparator)CompareKeys, 
								(PLHashComparator)CompareValues,
								nsnull, nsnull);
	mCount = 0;
}


//--------------------------------
// Public methods
//--------------------------------

ATSUILayoutCache::~ATSUILayoutCache()
{
	if (mTable)
	{
		PL_HashTableEnumerateEntries(mTable, FreeHashEntries, 0);
		PL_HashTableDestroy(mTable);
		mTable = nsnull;
	}
}


PRBool ATSUILayoutCache::Get(short aFont, short aSize, PRBool aBold, PRBool aItalic, nscolor aColor, ATSUTextLayout *aTxlayout)
{
	atsuiLayoutCacheKey k = {aFont, aSize, aColor, (aBold ? 1 : 0) + (aItalic ? 2 : 0) };
	return Get(&k, aTxlayout);
}


void ATSUILayoutCache::Set(short aFont, short aSize, PRBool aBold, PRBool aItalic, nscolor aColor, ATSUTextLayout aTxlayout)
{
	atsuiLayoutCacheKey k = {aFont, aSize, aColor, (aBold ? 1 : 0) + (aItalic ? 2 : 0) };
	return Set(&k, aTxlayout);
}


//--------------------------------
// Private methods
//--------------------------------

PRBool ATSUILayoutCache::Get(atsuiLayoutCacheKey *key, ATSUTextLayout *txlayout)
{
	PLHashEntry **hep = PL_HashTableRawLookup(mTable, HashKey(key), key);
	PLHashEntry *he = *hep;
	if( he )
	{
		*txlayout = (ATSUTextLayout)he->value;
		return PR_TRUE;
	}
	return PR_FALSE;
}


void ATSUILayoutCache::Set(atsuiLayoutCacheKey *key, ATSUTextLayout txlayout)
{
	atsuiLayoutCacheKey *newKey = new atsuiLayoutCacheKey;
	if (newKey)
	{
		*newKey = *key;
		PL_HashTableAdd(mTable, newKey, txlayout);
		mCount ++;
	}
}


PR_CALLBACK PLHashNumber ATSUILayoutCache::HashKey(const void *aKey)
{
	atsuiLayoutCacheKey* key = (atsuiLayoutCacheKey*)aKey;	
	return 	key->font + (key->size << 7) + (key->boldItalic << 12) + key->color;
}


PR_CALLBACK PRIntn ATSUILayoutCache::CompareKeys(const void *v1, const void *v2)
{
	atsuiLayoutCacheKey *k1 = (atsuiLayoutCacheKey *)v1;
	atsuiLayoutCacheKey *k2 = (atsuiLayoutCacheKey *)v2;
	return (k1->font == k2->font) && (k1->color == k2->color ) && (k1->size == k2->size) && (k1->boldItalic == k2->boldItalic);
}


PR_CALLBACK PRIntn ATSUILayoutCache::CompareValues(const void *v1, const void *v2)
{
	ATSUTextLayout t1 = (ATSUTextLayout)v1;
	ATSUTextLayout t2 = (ATSUTextLayout)v2;
	return (t1 == t2);
}


PR_CALLBACK PRIntn ATSUILayoutCache::FreeHashEntries(PLHashEntry *he, PRIntn italic, void *arg)
{
	delete (atsuiLayoutCacheKey*)he->key;
	ATSUDisposeTextLayout((ATSUTextLayout)he->value);
	return HT_ENUMERATE_REMOVE;
}


#pragma mark -
//------------------------------------------------------------------------
//	nsATSUIUtils
//
//------------------------------------------------------------------------


//--------------------------------
// globals
//--------------------------------

ATSUILayoutCache* nsATSUIUtils::gTxLayoutCache = nsnull;

PRBool	nsATSUIUtils::gIsAvailable = PR_FALSE;
PRBool	nsATSUIUtils::gInitialized = PR_FALSE;


//--------------------------------
// Initialize
//--------------------------------

void nsATSUIUtils::Initialize()
{
	if (!gInitialized)
	{
		gInitialized = PR_TRUE;

		long version;
  		gIsAvailable = (::Gestalt(gestaltATSUVersion, &version) == noErr);

  		gTxLayoutCache = new ATSUILayoutCache();
  		if (!gTxLayoutCache)
  			gIsAvailable = PR_FALSE;
	}
}


//--------------------------------
// IsAvailable
//--------------------------------

PRBool nsATSUIUtils::IsAvailable()
{
	return gIsAvailable;
}


//--------------------------------
// ConvertToMacRoman
//--------------------------------

PRBool nsATSUIUtils::ConvertToMacRoman(const PRUnichar *aString, PRUint32 aLength, char* macroman, PRBool onlyAllowASCII)
{
	static char map[0x80] = {
0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,
0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,
0xCA, 0xC1,  0xA2, 0xA3,  0x00, 0xB4,  0x00, 0xA4,  0xAC, 0xA9,  0xBB, 0xC7,  0xC2, 0xD0,  0xA8, 0xF8,
0xA1, 0xB1,  0x00, 0x00,  0xAB, 0xB5,  0xA6, 0xE1,  0xFC, 0x00,  0xBC, 0xC8,  0x00, 0x00,  0x00, 0xC0,
0xCB, 0xE7,  0xE5, 0xCC,  0x80, 0x81,  0xAE, 0x82,  0xE9, 0x83,  0xE6, 0xE8,  0xED, 0xEA,  0xEB, 0xEC,
0x00, 0x84,  0xF1, 0xEE,  0xEF, 0xCD,  0x85, 0x78,  0xAF, 0xF4,  0xF2, 0xF3,  0x86, 0x00,  0x00, 0xA7,
0x88, 0x87,  0x89, 0x8B,  0x8A, 0x8C,  0xBE, 0x8D,  0x8F, 0x8E,  0x90, 0x91,  0x93, 0x92,  0x94, 0x95,
0x00, 0x96,  0x98, 0x97,  0x99, 0x9B,  0x9A, 0xD6,  0xBF, 0x9D,  0x9C, 0x9E,  0x9F, 0x00,  0x00, 0xD8
	
	};
	static char g2010map[0x40] = {
0x00,0x00,0x00,0xd0,0xd1,0x00,0x00,0x00,0xd4,0xd5,0xe2,0x00,0xd2,0xd3,0xe3,0x00,  
0xa0,0xe0,0xa5,0x00,0x00,0x00,0xc9,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  
0xe4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xdc,0xdd,0x00,0x00,0x00,0x00,0x00,  
0x00,0x00,0x05,0x00,0xda,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  
    };
	const PRUnichar *u = aString;
	char *c = macroman;
	if (onlyAllowASCII)
	{
		for (PRUint32 i = 0 ; i < aLength ; i++)
		{
			if ((*u) & 0xFF80)
				return PR_FALSE;
			*c++ = *u++;
		}
	}
	else
	{
	    // XXX we should clean up these by calling the Unicode Converter after cata add x-mac-roman converters there.
		for (PRUint32 i = 0 ; i < aLength ; i++)
		{
			if ((*u) & 0xFF80)
			{
				char ch;
				switch ((*u) & 0xFF00) 
				{
				  case 0x0000:
				  	ch = map[(*u) & 0x007F];
					break;

				  case 0x2000:
				    if (0x2202 == *u)
				    {
				       ch = 0xDB;
					}
					else
					{
					  if ((*u < 0x2013)  || ( *u > 0x2044))
				        return PR_FALSE;
				      ch = g2010map[*u - 0x2010];
				      if( 0 == ch)
				        return PR_FALSE;
				    }
				    break;

				  case 0x2100:
				    if (0x2122 == *u) ch = 0xAA;
				    else if (0x2126 == *u) ch = 0xBD;
				    else return PR_FALSE;
				    break;

				  case 0x2200:
				  	switch (*u)
				  	{
					    case 0x2202: ch = 0xB6; break;
					    case 0x2206: ch = 0xBD; break;
					    case 0x220F: ch = 0xC6; break;
					    case 0x2211: ch = 0xB8; break;
					    case 0x221A: ch = 0xB7; break;
					    case 0x221E: ch = 0xC3; break;
					    case 0x222B: ch = 0xBA; break;
					    case 0x2248: ch = 0xC5; break;
					    case 0x2260: ch = 0xAD; break;
					    case 0x2264: ch = 0xB2; break;
					    case 0x2265: ch = 0xB3; break;
				  		default: return PR_FALSE;
				  	};
				    break;

				  case 0x2500:
				    if (0x25CA == *u) ch = 0xD7;
				    else return PR_FALSE;
				    break;

				  case 0xF800:
				    if (0xF8FF == *u) ch = 0xF0;
				    else return PR_FALSE;
				    break;

				  case 0xFB00:
				    if (0xFB01 == *u) ch = 0xDE;
				    else if( 0xFB02 == *u) ch = 0xDF;
				    else return PR_FALSE;
				    break;

				  default:
				    return PR_FALSE;
				}
				u++;
				if (ch == 0)
					return PR_FALSE;				
				*c++ = ch;
			}
			else
			{
				*c++ = *u++;
			}
		}
	}
	return PR_TRUE;
}


#pragma mark -

//------------------------------------------------------------------------
//	nsATSUIToolkit
//
//------------------------------------------------------------------------
nsATSUIToolkit::nsATSUIToolkit()
{
	nsATSUIUtils::Initialize();

	mLastTextLayout = nsnull;
	mFontOrColorChanged = PR_TRUE;
	mGS = nsnull;
	mPort = nsnull;
	mContext = nsnull;
}


//------------------------------------------------------------------------
//	PrepareToDraw
//
//------------------------------------------------------------------------

void nsATSUIToolkit::PrepareToDraw(PRBool aFontOrColorChanged, GraphicState* aGS, GrafPtr aPort, nsIDeviceContext* aContext)
{
	mFontOrColorChanged |= aFontOrColorChanged;
	mGS = aGS;
	mPort = aPort;
	mContext = aContext;
}


//------------------------------------------------------------------------
//	GetTextLayout
//
//------------------------------------------------------------------------

#define FloatToFixed(a)		((Fixed)((float)(a) * fixed1))

ATSUTextLayout nsATSUIToolkit::GetTextLayout()
{ 
	if (!mFontOrColorChanged)
		return mLastTextLayout;
	mFontOrColorChanged = false;

	ATSUTextLayout txLayout = nsnull;
	OSStatus err;
	
	nsFont *font;
	nsFontHandle fontNum;
	mGS->mFontMetrics->GetFont(font);
	mGS->mFontMetrics->GetFontHandle(fontNum);
	PRBool aBold = font->weight > NS_FONT_WEIGHT_NORMAL;
	PRBool aItalic = (NS_FONT_STYLE_ITALIC ==  font->style) || (NS_FONT_STYLE_OBLIQUE ==  font->style);		
	
	if (!nsATSUIUtils::gTxLayoutCache->Get((short)fontNum, font->size, aBold, aItalic, mGS->mColor, &txLayout))
	{
		UniChar dmy[1];
		err = ATSUCreateTextLayoutWithTextPtr (dmy, 0,0,0,0,NULL, NULL, &txLayout);

 		NS_ASSERTION(noErr == err, "ATSUCreateTextLayoutWithTextPtr failed");
 	
		ATSUStyle				theStyle;
		err = ATSUCreateStyle(&theStyle);
 		NS_ASSERTION(noErr == err, "ATSUCreateStyle failed");

		ATSUAttributeTag 		theTag[3];
		ByteCount				theValueSize[3];
		ATSUAttributeValuePtr 	theValue[3];

 		//--- Font ID & Face -----		
		ATSUFontID atsuFontID;
		
		err = ATSUFONDtoFontID((short)fontNum, (StyleField)((aBold ? bold : normal) | (aItalic ? italic : normal)), &atsuFontID);
 		NS_ASSERTION(noErr == err, "ATSUFONDtoFontID failed");

		theTag[0] = kATSUFontTag;
		theValueSize[0] = (ByteCount) sizeof(ATSUFontID);
		theValue[0] = (ATSUAttributeValuePtr) &atsuFontID;
 		//--- Font ID & Face  -----		
 		
 		//--- Size -----		
 		float  dev2app;
 		short fontsize = font->size;

		mContext->GetDevUnitsToAppUnits(dev2app);
 		Fixed size = FloatToFixed( roundf(float(fontsize) / dev2app));
#if DONT_USE_FONTS_SMALLER_THAN_9
 		if( FixRound ( size ) < 9 )
 			size = X2Fix(9);
#endif

		theTag[1] = kATSUSizeTag;
		theValueSize[1] = (ByteCount) sizeof(Fixed);
		theValue[1] = (ATSUAttributeValuePtr) &size;
 		//--- Size -----		
 		
 		//--- Color -----		
 		RGBColor color;

		#define COLOR8TOCOLOR16(color8)	 ((color8 << 8) | color8) 		

		color.red = COLOR8TOCOLOR16(NS_GET_R(mGS->mColor));
		color.green = COLOR8TOCOLOR16(NS_GET_G(mGS->mColor));
		color.blue = COLOR8TOCOLOR16(NS_GET_B(mGS->mColor));				
		theTag[2] = kATSUColorTag;
		theValueSize[2] = (ByteCount) sizeof(RGBColor);
		theValue[2] = (ATSUAttributeValuePtr) &color;
 		//--- Color -----		

		err =  ATSUSetAttributes(theStyle, 3, theTag, theValueSize, theValue);
 		NS_ASSERTION(noErr == err, "ATSUSetAttributes failed");
		 	
 		err = ATSUSetRunStyle(txLayout, theStyle, kATSUFromTextBeginning, kATSUToTextEnd);
 		NS_ASSERTION(noErr == err, "ATSUSetRunStyle failed");
	
	
		err = ATSUSetTransientFontMatching(txLayout, true);
 		NS_ASSERTION(noErr == err, "ATSUSetTransientFontMatching failed");
 		
 		nsATSUIUtils::gTxLayoutCache->Set((short)fontNum, font->size, aBold, aItalic, mGS->mColor,  txLayout);	
	} 		
	mLastTextLayout = txLayout;
	return mLastTextLayout;
}


//------------------------------------------------------------------------
//	GetWidth
//
//------------------------------------------------------------------------

NS_IMETHODIMP nsATSUIToolkit::GetWidth(const PRUnichar *aString, PRUint32 aLength,
									nscoord &aWidth, PRInt32 *aFontID)
{
  if (!nsATSUIUtils::IsAvailable())
  	return NS_ERROR_NOT_INITIALIZED;

  nsresult res = NS_OK;
  GrafPtr savePort;
  ::GetPort(&savePort);
  ::SetPort(mPort);

  OSStatus err = noErr;
  ATSUTextLayout aTxtLayout = GetTextLayout();
  err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString, 0, aLength, aLength);
  NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");

  ATSUTextMeasurement iAfter; 
  err = ATSUMeasureText( aTxtLayout, 0, aLength, NULL, &iAfter, NULL, NULL );
  NS_ASSERTION(noErr == err, "ATSUMeasureText failed");
  
  float  dev2app;
  mContext->GetDevUnitsToAppUnits(dev2app);
  aWidth = NSToCoordRound(Fix2Long(FixMul(iAfter , X2Fix(dev2app))));      

  ::SetPort(savePort);
  return res;
}



//------------------------------------------------------------------------
//	DrawString
//
//------------------------------------------------------------------------

NS_IMETHODIMP nsATSUIToolkit::DrawString(const PRUnichar *aString, PRUint32 aLength,
									nscoord aX, nscoord aY, PRInt32 aFontID,
									const nscoord* aSpacing)
{
  if (!nsATSUIUtils::IsAvailable())
  	return NS_ERROR_NOT_INITIALIZED;

  nsresult res = NS_OK;
  GrafPtr savePort;
  ::GetPort(&savePort);
  ::SetPort(mPort);

  OSStatus err = noErr;
  
  PRInt32 x = aX;
  PRInt32 y = aY;
  if (mGS->mFontMetrics)
  {
	// set native font and attributes
	//SetPortTextState();

	// substract ascent since drawing specifies baseline
	nscoord ascent = 0;
	mGS->mFontMetrics->GetMaxAscent(ascent);
	y += ascent;
  }
  mGS->mTMatrix->TransformCoord(&x,&y);

  ATSUTextLayout aTxtLayout = GetTextLayout();
  if (NULL == aSpacing) 
  {
    err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString, 0, aLength, aLength);
    NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");
  	err = ATSUDrawText( aTxtLayout, 0, aLength, Long2Fix(x), Long2Fix(y));
    NS_ASSERTION(noErr == err, "ATSUMeasureText failed");
  }
  else
  {
    if (aLength < 500)
    {
     	int spacing[500];
	  	mGS->mTMatrix->ScaleXCoords(aSpacing, aLength, spacing);  	
	    for (PRInt32 i = 0; i < aLength; i++)
	    {
          err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString+i, 0, 1, 1);
          NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");
	      err = ATSUDrawText( aTxtLayout, 0, 1, Long2Fix(x), Long2Fix(y));
	      NS_ASSERTION(noErr == err, "ATSUMeasureText failed");  	
	      x += spacing[i];
	    }
	}
    else
    {
      	int *spacing = new int[aLength];
	    NS_ASSERTION(NULL != spacing, "memalloc failed");  	
		if (spacing)
		{
		  	mGS->mTMatrix->ScaleXCoords(aSpacing, aLength, spacing);  	
		    for (PRInt32 i = 0; i < aLength; i++)
		    {
	          err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString+i, 0, 1, 1);
	          NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");
		      err = ATSUDrawText( aTxtLayout, 0, 1, Long2Fix(x), Long2Fix(y));
		      NS_ASSERTION(noErr == err, "ATSUMeasureText failed");  	
		      x += spacing[i];
		    }
	      	delete[] spacing;
		}
		else
			res = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  ::SetPort(savePort);
  return res;
}
