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
#include "nsDeviceContextMac.h"
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


#pragma mark -

//------------------------------------------------------------------------
//	nsATSUIToolkit
//
//------------------------------------------------------------------------
nsATSUIToolkit::nsATSUIToolkit()
{
	nsATSUIUtils::Initialize();
}


//------------------------------------------------------------------------
//	GetTextLayout
//
//------------------------------------------------------------------------

#define FloatToFixed(a)		((Fixed)((float)(a) * fixed1))

//------------------------------------------------------------------------
ATSUTextLayout nsATSUIToolkit::GetTextLayout(short aFontNum, short aSize, PRBool aBold, PRBool aItalic, nscolor aColor)
{ 
	ATSUTextLayout txLayout = nsnull;
	OSStatus err;
	if (nsATSUIUtils::gTxLayoutCache->Get(aFontNum, aSize, aBold, aItalic, aColor, &txLayout))
		return txLayout;
		
	UniChar dmy[1];
	err = ::ATSUCreateTextLayoutWithTextPtr (dmy, 0,0,0,0,NULL, NULL, &txLayout);
	if(noErr != err) {
		NS_WARNING("ATSUCreateTextLayoutWithTextPtr failed");
		goto errorDone;
	}

	ATSUStyle				theStyle;
	err = ::ATSUCreateStyle(&theStyle);
	if(noErr != err) {
		NS_WARNING("ATSUCreateStyle failed");
		goto errorDoneDestroyTextLayout;
	}

	ATSUAttributeTag 		theTag[3];
	ByteCount				theValueSize[3];
	ATSUAttributeValuePtr 	theValue[3];

	//--- Font ID & Face -----		
	ATSUFontID atsuFontID;
	
	err = ::ATSUFONDtoFontID(aFontNum, (StyleField)((aBold ? bold : normal) | (aItalic ? italic : normal)), &atsuFontID);
	if(noErr != err) {
		NS_WARNING("ATSUFONDtoFontID failed");
		goto errorDoneDestroyStyle;
	}
	
	theTag[0] = kATSUFontTag;
	theValueSize[0] = (ByteCount) sizeof(ATSUFontID);
	theValue[0] = (ATSUAttributeValuePtr) &atsuFontID;
	//--- Font ID & Face  -----		
	
	//--- Size -----		
	float  dev2app;
	short fontsize = aSize;

	mContext->GetDevUnitsToAppUnits(dev2app);
	Fixed size = FloatToFixed( roundf(float(fontsize) / dev2app));
	if( FixRound ( size ) < 9  && !nsDeviceContextMac::DisplayVerySmallFonts())
		size = X2Fix(9);

	theTag[1] = kATSUSizeTag;
	theValueSize[1] = (ByteCount) sizeof(Fixed);
	theValue[1] = (ATSUAttributeValuePtr) &size;
	//--- Size -----		
	
	//--- Color -----		
	RGBColor color;

	#define COLOR8TOCOLOR16(color8)	 ((color8 << 8) | color8) 		

	color.red = COLOR8TOCOLOR16(NS_GET_R(aColor));
	color.green = COLOR8TOCOLOR16(NS_GET_G(aColor));
	color.blue = COLOR8TOCOLOR16(NS_GET_B(aColor));				
	theTag[2] = kATSUColorTag;
	theValueSize[2] = (ByteCount) sizeof(RGBColor);
	theValue[2] = (ATSUAttributeValuePtr) &color;
	//--- Color -----		

	err =  ::ATSUSetAttributes(theStyle, 3, theTag, theValueSize, theValue);
	if(noErr != err) {
		NS_WARNING("ATSUSetAttributes failed");
		goto errorDoneDestroyStyle;
	}
	 	
	err = ::ATSUSetRunStyle(txLayout, theStyle, kATSUFromTextBeginning, kATSUToTextEnd);
	if(noErr != err) {
		NS_WARNING("ATSUSetRunStyle failed");
		goto errorDoneDestroyStyle;
	}
	
    err = ::ATSUSetTransientFontMatching(txLayout, true);
	if(noErr != err) {
		NS_WARNING( "ATSUSetTransientFontMatching failed");
		goto errorDoneDestroyStyle;
	}
    	
	nsATSUIUtils::gTxLayoutCache->Set(aFontNum, aSize, aBold, aItalic, aColor,  txLayout);	

okDone:
	return txLayout;
	
	
errorDoneDestroyStyle:
	err = ::ATSUDisposeStyle(theStyle);
errorDoneDestroyTextLayout:
	err = ::ATSUDisposeTextLayout(txLayout);
errorDone:
	return nsnull;
}
//------------------------------------------------------------------------
//	PrepareToDraw
//
//------------------------------------------------------------------------
void nsATSUIToolkit::PrepareToDraw(GrafPtr aPort, nsIDeviceContext* aContext)
{
   mPort = aPort;
   mContext = aContext;
}
//------------------------------------------------------------------------
//	StartDraw
//
//------------------------------------------------------------------------
void nsATSUIToolkit::StartDraw(
	const PRUnichar *aCharPt, 
	short aSize, short aFontNum,
	PRBool aBold, PRBool aItalic, nscolor aColor,
	GrafPtr& oSavePort, ATSUTextLayout& oLayout)
{
  ::GetPort(&oSavePort);
  ::SetPort(mPort);

  OSStatus err = noErr;
  oLayout = GetTextLayout(aFontNum, aSize, aBold, aItalic, aColor);
  if (nsnull == oLayout) {
    NS_WARNING("GetTextLayout return nsnull");
  	return;
  }

  err = ATSUSetTextPointerLocation( oLayout, (ConstUniCharArrayPtr)aCharPt, 0, 1, 1);
  if(noErr != err) {
    NS_WARNING("ATSUSetTextPointerLocation failed");
  	oLayout = nsnull;
  }
  return;
}
//------------------------------------------------------------------------
//	EndDraw
//
//------------------------------------------------------------------------
void nsATSUIToolkit::EndDraw(GrafPtr aSavePort)
{
  ::SetPort(aSavePort);
}
//------------------------------------------------------------------------
//	GetWidth
//
//------------------------------------------------------------------------
NS_IMETHODIMP nsATSUIToolkit::GetWidth(
	const PRUnichar *aCharPt, 
	short& oWidth,
	short aSize, short aFontNum,
	PRBool aBold, PRBool aItalic, nscolor aColor)
{
  oWidth = 0;
  if (!nsATSUIUtils::IsAvailable())
  	return NS_ERROR_NOT_INITIALIZED;
  	
  nsresult res = NS_ERROR_FAILURE;
  GrafPtr savePort;
  ATSUTextLayout aTxtLayout;
  
  StartDraw(aCharPt, aSize, aFontNum, aBold, aItalic, aColor , savePort, aTxtLayout);
  if (nsnull == aTxtLayout) 
  	goto done;
  	
  OSStatus err = noErr;	
  ATSUTextMeasurement iAfter; 
  err = ATSUMeasureText( aTxtLayout, 0, 1, NULL, &iAfter, NULL, NULL );
  if(noErr != err) {
     NS_WARNING("ATSUMeasureText failed");
     goto done1;
  }
  oWidth = FixRound(iAfter);
  res = NS_OK;
done1:
  EndDraw(savePort);
done:
  return res;
}


//------------------------------------------------------------------------
//	DrawString
//
//------------------------------------------------------------------------
NS_IMETHODIMP nsATSUIToolkit::DrawString(
	const PRUnichar *aCharPt, 
	PRInt32 x, PRInt32 y, 
	short &oWidth,
	short aSize, short aFontNum,
	PRBool aBold, PRBool aItalic, nscolor aColor)
{
  oWidth = 0;
  if (!nsATSUIUtils::IsAvailable())
  	return NS_ERROR_NOT_INITIALIZED;
  	
  nsresult res = NS_ERROR_FAILURE;
  GrafPtr savePort;
  ATSUTextLayout aTxtLayout;
  
  StartDraw(aCharPt, aSize, aFontNum, aBold, aItalic, aColor , savePort, aTxtLayout);
  if (nsnull == aTxtLayout)
  	goto done;
  	
  OSStatus err = noErr;	
  ATSUTextMeasurement iAfter; 
  err = ATSUMeasureText( aTxtLayout, 0, 1, NULL, &iAfter, NULL, NULL );
  if(noErr != err) {
     NS_WARNING("ATSUMeasureText failed");
     goto done1;
  } 
  err = ATSUDrawText( aTxtLayout, 0, 1, Long2Fix(x), Long2Fix(y));
  if(noErr != err) {
    NS_WARNING("ATSUDrawText failed");
    goto done1;
  } 
  oWidth = FixRound(iAfter);
  res = NS_OK;

done1:
  EndDraw(savePort);
done:
  return res;
}
