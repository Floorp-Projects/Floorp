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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsDeviceContextMac.h"
#include "nsRenderingContextMac.h"
#include "nsDeviceContextSpecMac.h"
#include "nsString.h"
#include "nsHashtable.h"
#include <TextEncodingConverter.h>
#include <TextCommon.h>
#include <StringCompare.h>
#include <Fonts.h>
#include <Resources.h>
#include <MacWindows.h>
#include "il_util.h"
#include <FixMath.h>
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsQuickSort.h"
#include "nsUnicodeMappingUtil.h"
#include "nsCarbonHelpers.h"


PRUint32 nsDeviceContextMac::mPixelsPerInch = 96;
PRBool nsDeviceContextMac::mDisplayVerySmallFonts = true;


static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
nsDeviceContextMac :: nsDeviceContextMac()
{

  NS_INIT_REFCNT();
  mSpec = nsnull;
  
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
nsDeviceContextMac :: ~nsDeviceContextMac()
{
}

//------------------------------------------------------------------------

NS_IMPL_QUERY_INTERFACE(nsDeviceContextMac, kDeviceContextIID);
NS_IMPL_ADDREF(nsDeviceContextMac);
NS_IMPL_RELEASE(nsDeviceContextMac);

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: Init(nsNativeWidget aNativeWidget)
{
GDHandle			thegd;
PixMapHandle	thepix;
double				pix_inch;

	// this is a windowptr, or grafptr, native to macintosh only
	mSurface = aNativeWidget;

	// get depth and resolution
  thegd = ::GetMainDevice();
	thepix = (**thegd).gdPMap;					// dereferenced handle: don't move memory below!
	mDepth = (**thepix).pixelSize;
  pix_inch = GetScreenResolution();		//Fix2X((**thepix).hRes);
	mTwipsToPixels = pix_inch/(float)NSIntPointsToTwips(72);
	mPixelsToTwips = 1.0f/mTwipsToPixels;
	
  return DeviceContextImpl::Init(aNativeWidget);
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
nsRenderingContextMac *pContext;
nsresult              rv;
GrafPtr								thePort;

	pContext = new nsRenderingContextMac();
  if (nsnull != pContext){
    NS_ADDREF(pContext);

   	::GetPort(&thePort);

    if (nsnull != thePort){
      rv = pContext->Init(this,thePort);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  if (NS_OK != rv){
    NS_IF_RELEASE(pContext);
  }
  aContext = pContext;
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  //XXX it is very critical that this not lie!! MMP
  
  // ¥¥¥ VERY IMPORTANT (pinkerton)
  // This routine should return true if the widgets behave like Win32
  // "windows", that is they paint themselves and the app never knows about
  // them or has to send them update events. We were returning false which
  // meant that raptor needed to make sure to redraw them. However, if we
  // return false, the widgets never get created because the CreateWidget()
  // call in nsView never creates the widget. If we return true (which makes
  // things work), we are lying because the controls really need those
  // precious update/repaint events.
  // 
  // The situation we need is the following:
  // - return false from SupportsNativeWidgets()
  // - Create() is called on widgets when the above case is true.
  // 
  // Raptor currently doesn't work this way and needs to be fixed.
  // (please remove this comment when this situation is rectified)
  
  //if (nsnull == mSurface)
    aSupportsWidgets = PR_TRUE;
  //else
   //aSupportsWidgets = PR_FALSE;

  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  // XXX Should we push this to widget library
  aWidth = 16 * mDevUnitsToAppUnits;
  aHeight = 16 * mDevUnitsToAppUnits;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = NS_RGB(0x80,0x80,0x80);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = NS_RGB(0xff,0xff,0xff);
        break;

    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight : 
        aInfo->mSize = 16;	//21;
        break;
    case eSystemAttr_Size_ScrollbarWidth : 
        aInfo->mSize = 16;	//21;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = 4;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = 4;
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = 4;
        break;

    //---------
    // Fonts
    //---------
    case eSystemAttr_Font_Caption : 
    case eSystemAttr_Font_Icon : 
    case eSystemAttr_Font_Menu : 
    case eSystemAttr_Font_MessageBox : 
    case eSystemAttr_Font_SmallCaption : 
    case eSystemAttr_Font_StatusBar : 
    case eSystemAttr_Font_Tooltips : 
      status = NS_ERROR_FAILURE;
      break;

  } // switch 

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  aPixel = aColor;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::CreateILColorSpace(IL_ColorSpace*& aColorSpace)
{
  nsresult result = NS_OK;


  return result;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{

  if (nsnull == mColorSpace) {
    IL_RGBBits colorRGBBits;
  
    // Default is to create a 32-bit color space
    colorRGBBits.red_shift = 16;  
    colorRGBBits.red_bits = 8;
    colorRGBBits.green_shift = 8;
    colorRGBBits.green_bits = 8; 
    colorRGBBits.blue_shift = 0; 
    colorRGBBits.blue_bits = 8;  
  
    mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 32);
    if (nsnull == mColorSpace) {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_POSTCONDITION(nsnull != mColorSpace, "null color space");
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: CheckFontExistence(const nsString& aFontName)
{
  	short fontNum;
	if (GetMacFontNumber(aFontName, fontNum))
		return NS_OK;
	else
		return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
	// FIXME:  could just union all of the GDevice rectangles together.
	Rect bounds;
	::GetRegionBounds ( ::GetGrayRgn(), &bounds );
	//aWidth = bounds.right - bounds.left;
	//aHeight = bounds.bottom - bounds.top;
	
	
	if(mSpec) {
		aWidth = (mPageRect.right-mPageRect.left)*mDevUnitsToAppUnits;
		aHeight = (mPageRect.bottom-mPageRect.top)*mDevUnitsToAppUnits;
	}else {
		aHeight = NSToIntRound((bounds.bottom - bounds.top)*mDevUnitsToAppUnits);
		aWidth = NSToIntRound((bounds.right - bounds.left) * mDevUnitsToAppUnits);
	}
	
	return NS_OK;	
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::GetClientRect(nsRect &aRect)
{
	// FIXME: equally as broken as GetDeviceSurfaceDimensions,
	// this doesn't do what you want with multiple screens.
	Rect bounds;
	::GetRegionBounds ( ::GetGrayRgn(), &bounds );

	aRect.x = NSToIntRound(bounds.left * mDevUnitsToAppUnits);
	aRect.y = NSToIntRound(bounds.top * mDevUnitsToAppUnits);
	aRect.width = NSToIntRound((bounds.right - bounds.left) * mDevUnitsToAppUnits);
	aRect.height = NSToIntRound((bounds.bottom - bounds.top) * mDevUnitsToAppUnits);
	
	return NS_OK;	
}

#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext)
{
GrafPtr							curPort;	
THPrint							thePrintRecord;			// handle to print record
double							pix_Inch;
nsDeviceContextMac	*macDC;

	aContext = new nsDeviceContextMac();
	macDC = (nsDeviceContextMac*)aContext;
	macDC->mSpec = aDevice;
	NS_ADDREF(aDevice);
	
	::GetPort(&curPort);
	
	thePrintRecord = ((nsDeviceContextSpecMac*)aDevice)->mPrtRec;
	pix_Inch = (**thePrintRecord).prInfo.iHRes;
	
	((nsDeviceContextMac*)aContext)->Init(curPort);

	macDC->mPageRect = (**thePrintRecord).prInfo.rPage;	
	macDC->mTwipsToPixels = pix_Inch/(float)NSIntPointsToTwips(72);
	macDC->mPixelsToTwips = 1.0f/macDC->mTwipsToPixels;
  macDC->mAppUnitsToDevUnits = macDC->mTwipsToPixels;
  macDC->mDevUnitsToAppUnits = 1.0f / macDC->mAppUnitsToDevUnits;
  
  macDC->mCPixelScale = macDC->mTwipsToPixels / mTwipsToPixels;

	//((nsDeviceContextMac*)aContext)->Init(this);
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::BeginDocument(void)
{
#if !TARGET_CARBON
GrafPtr	thePort;
 
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen) {
 		::GetPort(&mOldPort);
 		thePort = (GrafPtr)::PrOpenDoc(((nsDeviceContextSpecMac*)(this->mSpec))->mPrtRec,nsnull,nsnull);
  	((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort = (TPrPort*)thePort;
  	SetDrawingSurface(((nsDeviceContextSpecMac*)(this->mSpec))->mPrtRec);
  	SetPort(thePort);
  }
#endif
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::EndDocument(void)
{
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen){
 		::SetPort(mOldPort);
#if !TARGET_CARBON
		::PrCloseDoc(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort);
#endif
	}
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::BeginPage(void)
{
#if !TARGET_CARBON
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen) 
		::PrOpenPage(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort,nsnull);
#endif
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac::EndPage(void)
{
#if !TARGET_CARBON
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen) {
 		::SetPort((GrafPtr)(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort));
		::PrClosePage(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort);
	}
#endif
  return NS_OK;
}


#pragma mark -

//------------------------------------------------------------------------

nsHashtable* nsDeviceContextMac :: gFontInfoList = nsnull;

class FontNameKey : public nsHashKey
{
public:
  FontNameKey(const nsString& aString);

  virtual PRUint32 HashValue(void) const;
  virtual PRBool Equals(const nsHashKey *aKey) const;
  virtual nsHashKey *Clone(void) const;

  nsAutoString  mString;
};

FontNameKey::FontNameKey(const nsString& aString)
{
	mString = aString;
}

PRUint32 FontNameKey::HashValue(void) const
{
  nsString str;
  mString.ToLowerCase(str);
	return nsCRT::HashValue(str.GetUnicode());
}

PRBool FontNameKey::Equals(const nsHashKey *aKey) const
{
  return mString.EqualsIgnoreCase(((FontNameKey*)aKey)->mString);
}

nsHashKey* FontNameKey::Clone(void) const
{
  return new FontNameKey(mString);
}

#pragma mark -

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
void nsDeviceContextMac :: InitFontInfoList()
{

	OSStatus err;
	if (!gFontInfoList) 
	{
		gFontInfoList = new nsHashtable();
		if (!gFontInfoList)
			return;

		short numFONDs = ::CountResources('FOND');
#if !TARGET_CARBON
		TextEncoding unicodeEncoding = ::CreateTextEncoding(kTextEncodingUnicodeDefault, 
															kTextEncodingDefaultVariant,
													 		kTextEncodingDefaultFormat);
#endif
		TECObjectRef converter = nil;
		ScriptCode lastscript = smUninterp;
		for (short i = 1; i <= numFONDs ; i++)
		{
			Handle fond = ::GetIndResource('FOND', i);
			if (fond)
			{
				short	fondID;
				OSType	resType;
				Str255	fontName;
				::GetResInfo(fond, &fondID, &resType, fontName); 

#if !TARGET_CARBON
				ScriptCode script = ::FontToScript(fondID);
				if (script != lastscript)
				{
					lastscript = script;

					TextEncoding sourceEncoding;
					err = ::UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare, 
								kTextRegionDontCare, NULL, &sourceEncoding);
							
					if (converter)
						err = ::TECDisposeConverter(converter);

					err = ::TECCreateConverter(&converter, sourceEncoding, unicodeEncoding);
					if (err != noErr)
						converter = nil;
				}

				if (converter)
				{
					PRUnichar unicodeFontName[sizeof(fontName)];
					ByteCount actualInputLength, actualOutputLength;
					err = ::TECConvertText(converter, &fontName[1], fontName[0], &actualInputLength, 
												(TextPtr)unicodeFontName , sizeof(unicodeFontName), &actualOutputLength);	
					unicodeFontName[actualOutputLength / sizeof(PRUnichar)] = '\0';

	        		FontNameKey key(unicodeFontName);
					gFontInfoList->Put(&key, (void*)fondID);
				}
#else
				// pinkerton - CreateTextEncoding() makes a carbon app exit. this is a smarmy hack
				char buffer[500];
				::BlockMoveData ( &fontName[1], buffer, *fontName );
				buffer[*fontName] = NULL;
printf("font buffer is %s\n", buffer);
	        	FontNameKey key(buffer);
				gFontInfoList->Put(&key, (void*)fondID);				
#endif
				::ReleaseResource(fond);
			}
		}
		if (converter)
			err = ::TECDisposeConverter(converter);				
	}
}



/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
bool nsDeviceContextMac :: GetMacFontNumber(const nsString& aFontName, short &aFontNum)
{
	//¥TODO?: Maybe we shouldn't call that function so often. If nsFont could store the
	//				fontNum, nsFontMetricsMac::SetFont() wouldn't need to call this at all.
	InitFontInfoList();
#if TARGET_CARBON
	char* fontNameC = aFontName.ToNewCString();
	Str255 fontNamePascal;
	fontNamePascal[0] = strlen(fontNameC);
	::BlockMoveData ( fontNameC, &fontNamePascal[1], fontNamePascal[0] );
	::GetFNum ( fontNamePascal, &aFontNum );
	delete[] fontNameC;
#else
    FontNameKey key(aFontName);
	aFontNum = (short)gFontInfoList->Get(&key);
#endif
	return (aFontNum != 0);
}


//------------------------------------------------------------------------
// Override to tweak font settings
nsresult nsDeviceContextMac::CreateFontAliasTable()
{
  nsresult result = NS_OK;

  if (nsnull == mFontAliasTable) {
    mFontAliasTable = new nsHashtable();
    if (nsnull != mFontAliasTable)
    {
			nsAutoString  fontTimes("Times");
			nsAutoString  fontTimesNewRoman("Times New Roman");
			nsAutoString  fontTimesRoman("Times Roman");
			nsAutoString  fontArial("Arial");
			nsAutoString  fontHelvetica("Helvetica");
			nsAutoString  fontCourier("Courier");
			nsAutoString  fontCourierNew("Courier New");
			nsAutoString  fontUnicode("Unicode");
			nsAutoString  fontBitstreamCyberbit("Bitstream Cyberbit");
			nsAutoString  fontNullStr;

      AliasFont(fontTimes, fontTimesNewRoman, fontTimesRoman, PR_FALSE);
      AliasFont(fontTimesRoman, fontTimesNewRoman, fontTimes, PR_FALSE);
      AliasFont(fontTimesNewRoman, fontTimesRoman, fontTimes, PR_FALSE);
      AliasFont(fontArial, fontHelvetica, fontNullStr, PR_FALSE);
      AliasFont(fontHelvetica, fontArial, fontNullStr, PR_FALSE);
      AliasFont(fontCourier, fontCourierNew, fontNullStr, PR_FALSE);		// changed from DeviceContextImpl
      AliasFont(fontCourierNew, fontCourier, fontNullStr, PR_FALSE);
      AliasFont(fontUnicode, fontBitstreamCyberbit, fontNullStr, PR_FALSE); // XXX ????
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return result;
}

#pragma mark -

//------------------------------------------------------------------------
//
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
PRUint32 nsDeviceContextMac::GetScreenResolution()
{
	static PRBool initialized = PR_FALSE;
	if (initialized)
		return mPixelsPerInch;
	initialized = PR_TRUE;

  nsIPref* prefs;
  nsresult rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
  if (NS_SUCCEEDED(rv) && prefs) {
		PRInt32 intVal;
		if (NS_SUCCEEDED(prefs->GetIntPref("browser.screen_resolution", &intVal))) {
			mPixelsPerInch = intVal;
		}
#if 0
		else {
			short hppi, vppi;
			::ScreenRes(&hppi, &vppi);
			mPixelsPerInch = hppi * 1.17f;
		}
#endif
		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}

	return mPixelsPerInch;
}

PRBool nsDeviceContextMac::DisplayVerySmallFonts()
{
	static PRBool initialized = PR_FALSE;
	if (initialized)
		return mDisplayVerySmallFonts;
	initialized = PR_TRUE;

  nsIPref* prefs;
  nsresult rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
  if (NS_SUCCEEDED(rv) && prefs) {
		PRBool boolVal;
		if (NS_SUCCEEDED(prefs->GetBoolPref("browser.display_very_small_fonts", &boolVal))) {
			mDisplayVerySmallFonts = boolVal;
		}
		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}

	return mDisplayVerySmallFonts;
}


#pragma mark -
//------------------------------------------------------------------------
nsFontEnumeratorMac::nsFontEnumeratorMac()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(nsFontEnumeratorMac,
                  NS_GET_IID(nsIFontEnumerator));
typedef struct EnumerateFamilyInfo
{
  PRUnichar** mArray;
  int         mIndex;
} EnumerateFamilyInfo;

typedef struct EnumerateFontInfo
{
  PRUnichar** mArray;
  int         mIndex;
  int         mCount;
  ScriptCode	mScript;
  nsGenericFontNameType mType;
} EnumerateFontInfo;



static int
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const PRUnichar* str1 = *((const PRUnichar**) aArg1);
  const PRUnichar* str2 = *((const PRUnichar**) aArg2);

  // XXX add nsICollation stuff

  return nsCRT::strcmp(str1, str2);
}
static PRBool
EnumerateFamily(nsHashKey *aKey, void *aData, void* closure)

{
  EnumerateFamilyInfo* info = (EnumerateFamilyInfo*) closure;
  PRUnichar** array = info->mArray;
  int j = info->mIndex;
  
  
  PRUnichar* str = (((FontNameKey*)aKey)->mString).ToNewUnicode();
  if (!str) {
    for (j = j - 1; j >= 0; j--) {
      nsAllocator::Free(array[j]);
    }
    info->mIndex = 0;
    return PR_FALSE;
  }
  array[j] = str;
  info->mIndex++;

  return PR_TRUE;
}

NS_IMETHODIMP
nsFontEnumeratorMac::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
  if (aCount) {
    *aCount = 0;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }
  if (aResult) {
    *aResult = nsnull;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }

	nsDeviceContextMac::InitFontInfoList();
	nsHashtable* list = nsDeviceContextMac::gFontInfoList;
	if(!list) {
		return NS_ERROR_FAILURE;
	}
	PRInt32 items = list->Count();
  PRUnichar** array = (PRUnichar**)
    nsAllocator::Alloc(items * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  EnumerateFamilyInfo info = { array, 0 };
  list->Enumerate ( EnumerateFamily, &info);
  NS_ASSERTION( items == info.mIndex, "didn't get all the fonts");
  if (!info.mIndex) {
    nsAllocator::Free(array);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_QuickSort(array, info.mIndex, sizeof(PRUnichar*),
    CompareFontNames, nsnull);

  *aCount = info.mIndex;
  *aResult = array;

  return NS_OK;
}

static PRBool
EnumerateFont(nsHashKey *aKey, void *aData, void* closure)

{
  EnumerateFontInfo* info = (EnumerateFontInfo*) closure;
  PRUnichar** array = info->mArray;
  int j = info->mCount;
  
  short	fondID = (short) aData;
  ScriptCode script = ::FontToScript(fondID);
	if(script == info->mScript) {
	  PRUnichar* str = (((FontNameKey*)aKey)->mString).ToNewUnicode();
	  if (!str) {
	    for (j = j - 1; j >= 0; j--) {
	      nsAllocator::Free(array[j]);
	    }
	    info->mIndex = 0;
	    return PR_FALSE;
	  }
	  array[j] = str;
	  info->mCount++;
	}
	info->mIndex++;
  return PR_TRUE;
}
NS_IMETHODIMP
nsFontEnumeratorMac::EnumerateFonts(const char* aLangGroup,
  const char* aGeneric, PRUint32* aCount, PRUnichar*** aResult)
{
  if ((! aLangGroup) ||( !aGeneric ))
  	return NS_ERROR_NULL_POINTER;
  	
  if (aCount) {
    *aCount = 0;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }
  if (aResult) {
    *aResult = nsnull;
  }
  else {
    return NS_ERROR_NULL_POINTER;
  }

  if ((!strcmp(aLangGroup, "x-unicode")) ||
      (!strcmp(aLangGroup, "x-user-def"))) {
    return EnumerateAllFonts(aCount, aResult);
  }

	nsDeviceContextMac::InitFontInfoList();
	nsHashtable* list = nsDeviceContextMac::gFontInfoList;
	if(!list) {
		return NS_ERROR_FAILURE;
	}
	PRInt32 items = list->Count();
  PRUnichar** array = (PRUnichar**)
    nsAllocator::Alloc(items * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsUnicodeMappingUtil* gUtil = nsUnicodeMappingUtil::GetSingleton();
	if(!gUtil) {
		return NS_ERROR_FAILURE;
	}
  
  nsAutoString GenName(aGeneric);
  EnumerateFontInfo info = { array, 0 , 0, gUtil->MapLangGroupToScriptCode(aLangGroup) ,gUtil->MapGenericFontNameType(GenName) };
  list->Enumerate ( EnumerateFont, &info);
  if (!info.mIndex) {
    nsAllocator::Free(array);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_QuickSort(array, info.mCount, sizeof(PRUnichar*),
    CompareFontNames, nsnull);

  *aCount = info.mCount;
  *aResult = array;

  return NS_OK;
}

