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
 */

#include <math.h>

#include "nspr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "il_util.h"
#include "nsCRT.h"

#include "nsDeviceContextPh.h"
#include "nsRenderingContextPh.h"
#include "nsDeviceContextSpecPh.h"
#include "nsHashtable.h"

#include "nsPhGfxLog.h"

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

nscoord nsDeviceContextPh::mDpi = 96;

NS_IMPL_ISUPPORTS1(nsDeviceContextPh, nsIDeviceContext)

static nsHashtable* mFontLoadCache = nsnull;
static int globals_initialized = 0;

nsDeviceContextPh :: nsDeviceContextPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::nsDeviceContextPh this=<%p>\n", this));

  NS_INIT_REFCNT();
  
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mDepth = 0 ;
  mSurface = NULL;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mPixelScale = 1.0f;
  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;

  /* These *MUST* be -1 */
  mWidth = -1;
  mHeight = -1;

  mSpec = nsnull;
  mDC = nsnull;
}

nsDeviceContextPh :: ~nsDeviceContextPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::~nsDeviceContextPh destructor called this=<%p> mSurface=<%p>\n", this, mSurface));

  nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *)mSurface;

  NS_IF_RELEASE(surf);    //this clears the surf pointer...
  mSurface = nsnull;

  if (NULL != mPaletteInfo.palette)
    PR_Free(mPaletteInfo.palette);

  if (mFontLoadCache) 
  {
	 delete mFontLoadCache;
	 mFontLoadCache = nsnull;
  }

  NS_IF_RELEASE(mSpec);
}


NS_IMETHODIMP nsDeviceContextPh :: Init(nsNativeWidget aWidget)
{
  float newscale, origscale;
  float a2d,t2d;
    
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with aWidget aWidget=<%p>\n", aWidget));

  CommonInit(NULL);
 
  GetTwipsToDevUnits(newscale);
  GetAppUnitsToDevUnits(origscale);
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init newscale=<%f> origscale=<%f>\n", newscale, origscale));
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init mPixelScale=<%f>\n", mPixelScale));

  GetTwipsToDevUnits(t2d);
  GetAppUnitsToDevUnits(a2d);

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init t2d=<%f> a2d=<%f> mTwipsToPixels=<%f>\n", t2d,a2d,mTwipsToPixels));

  // Call my base class
  return DeviceContextImpl::Init(aWidget);
}


/* Called for Printing */
nsresult nsDeviceContextPh :: Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with nsNativeDeviceContext aContext=<%p> aOrigContext=<%p>\n", aContext, aOrigContext));

  nsDeviceContextSpecPh  * PrintSpec = nsnull;
  PpPrintContext_t       *PrinterContext = nsnull;
  float                  origscale, newscale, t2d, a2d;
    
  mDC = aContext;

  CommonInit(mDC);

  GetTwipsToDevUnits(newscale);
  aOrigContext->GetAppUnitsToDevUnits(origscale);
  
  mPixelScale = newscale / origscale;

  aOrigContext->GetTwipsToDevUnits(t2d);
  aOrigContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

// for printers
	int width, height;
  	//GetPrinterRect(&width, &height);
	const PhDim_t *psize;
	PhDim_t dim;
	PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();

	PpPrintGetPC(pc, Pp_PC_PAPER_SIZE, (const void **)&psize );
  	mWidthFloat = (float)(psize->w / 10);
  	mHeightFloat = (float)(psize->h / 10);
  	dim.w = psize->w / 10;
  	dim.h = psize->h / 10;
	printf("PRINT: %d, %d\n", (int)mWidthFloat, (int)mHeightFloat);
	PpPrintSetPC(pc, INITIAL_PC, 0 , Pp_PC_SOURCE_SIZE, &dim );
  
  return NS_OK;
}

void nsDeviceContextPh :: GetPrinterRect(int *width, int *height)
{
	PhDim_t dim;
	const PhDim_t *psize;
	const PhRect_t 	*mrect, *non_print;
	PhRect_t	rect, margins;
	const char *orientation = 0, *name = NULL;
	char		*ptr;
	int			tmp;
	PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();
	
	memset( &rect, 0, sizeof(rect));
	memset( &margins, 0, sizeof(margins));

	printf("PC: %X\n", pc);
	PpPrintGetPC(pc, Pp_PC_PAPER_SIZE, (const void **)&psize );
	PpPrintGetPC(pc, Pp_PC_NONPRINT_MARGINS, (const void **)&non_print );
	printf("SIZE: %d, %d\n", psize->w, psize->h);
	dim.w = (psize->w - ( non_print->ul.x + non_print->lr.x )) * 100 / 1000;
	dim.h = (psize->h - ( non_print->ul.x + non_print->lr.x )) * 100 / 1000;

#if 0
	/* get margins requested in pc */
	PpPrintGetPC(pc, Pp_PC_MARGINS, &mrect );
	if ( mrect->ul.x > non_print->ul.x )
	    margins.ul.x = max( mrect->ul.x - non_print->ul.x, 0 ) * pMyPrintData->pUnits.x / 1000;
	if ( mrect->ul.y > non_print->ul.y )
	    margins.ul.y = max( mrect->ul.y - non_print->ul.y, 0 ) * pMyPrintData->pUnits.y / 1000;
	if ( mrect->lr.x > non_print->lr.y )
	    margins.lr.x = max( mrect->lr.x - non_print->lr.x, 0 ) * pMyPrintData->pUnits.x / 1000;
	if ( mrect->lr.y > non_print->lr.y )
	    margins.lr.y = max( mrect->lr.y - non_print->lr.y, 0 ) * pMyPrintData->pUnits.y / 1000;

#endif
	PpPrintGetPC(pc, Pp_PC_ORIENTATION, (const void **)&orientation );

	if ( *orientation ) 
	{
		tmp = dim.w;
		dim.w = dim.h;
		dim.h = tmp;
	}

#if 0
	/* compute drawable area of paper after margins are set */
	pMyPrintData->rDrawableArea.left = margins.ul.x;
	pMyPrintData->rDrawableArea.top = margins.ul.y;
	pMyPrintData->rDrawableArea.right = dim.w - margins.lr.x;
	pMyPrintData->rDrawableArea.bottom = dim.h - margins.lr.y;

	if ( pMyPrintData->rDrawableArea.left >= pMyPrintData->rDrawableArea.right ||
	 	 pMyPrintData->rDrawableArea.top  >= pMyPrintData->rDrawableArea.bottom ) {
		pMyPrintData->result = SM_RESULT_PRINT_NOTPRINTABLE;
		return STATE_ABORT;
	}
#endif		

	/* set these to 0 since we do the margins */
	PpPrintSetPC(pc, INITIAL_PC, 0 , Pp_PC_MARGINS, &rect ); 
	PpPrintSetPC(pc, INITIAL_PC, 0 , Pp_PC_SOURCE_SIZE, &dim );

	*width = dim.w;
	*height = dim.h;
}


void nsDeviceContextPh :: CommonInit(nsNativeDeviceContext aDC)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit mSpec=<%p>\n", mSpec));

  PRInt32           aWidth, aHeight;
  nsresult          err;
  static int        initialized = 0;

  if (!initialized)
  {
    initialized = 1;
    // Set prefVal the value of the preference "browser.display.screen_resolution"
    // or -1 if we can't get it.
    // If it's negative, we pretend it's not set.
    // If it's 0, it means force use of the operating system's logical resolution.
    // If it's positive, we use it as the logical resolution
    PRInt32 prefVal = -1;
    nsresult res;

    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
    if (NS_SUCCEEDED(res) && prefs)
    {
      res = prefs->GetIntPref("browser.display.screen_resolution", &prefVal);
      if (! NS_SUCCEEDED(res))
      {
        prefVal = 96;
      }

      prefs->RegisterCallback("browser.display.screen_resolution", prefChanged,
                              (void *)this);
      mDpi = prefVal;
    }
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit mDpi=<%d>\n", mDpi));

  SetDPI(mDpi); 

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit mPixelsToTwips=<%f>\n", mPixelsToTwips));
  
    err = GetDisplayInfo(aWidth, aHeight, mDepth);
    if (err == NS_ERROR_FAILURE)
      abort();

    /* Turn off virtual console support... */
    mWidthFloat  = (float) aWidth;
    mHeightFloat = (float) aHeight;
    
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit with aWidget: Screen Size (%f,%f)\n", mWidthFloat,mHeightFloat));


	if (mDepth > 8)
	{
		mPaletteInfo.isPaletteDevice = PR_FALSE;
		mPaletteInfo.sizePalette = 0;
		mPaletteInfo.numReserved = 0;
		mPaletteInfo.palette = NULL;
	}
	else
	{
		PgColor_t color[_Pg_MAX_PALETTE];

		PgGetPalette(color);
		// palette based
		mPaletteInfo.isPaletteDevice = PR_TRUE;
		mPaletteInfo.sizePalette = _Pg_MAX_PALETTE;
		mPaletteInfo.numReserved = 16;  				/* GUESS */
		mPaletteInfo.palette = PR_Malloc(_Pg_MAX_PALETTE * sizeof(PgColor_t));
		memcpy(mPaletteInfo.palette, color, _Pg_MAX_PALETTE * sizeof(PgColor_t));
	}
  

  /* Revisit: the scroll bar sizes is a gross guess based on Phab */
  mScrollbarHeight = 17.0f;
  mScrollbarWidth  = 17.0f;
  
  /* Call Base Class */
  DeviceContextImpl::CommonInit();
}

NS_IMETHODIMP nsDeviceContextPh :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CreateRenderingContext - Not Implemented\n"));

  /* I stole this code from windows but its not working yet! */
  nsIRenderingContext *pContext;
  nsresult             rv;
  nsDrawingSurfacePh  *surf;
   
	pContext = new nsRenderingContextPh();
	
	if (nsnull != pContext)
	{
	  NS_ADDREF(pContext);

	  surf = new nsDrawingSurfacePh();
	  if (nsnull != surf)
	  {
		/* I think this is a good idea... not sure if mDC is the right one tho... */
		PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CreateRenderingContext - mDC=<%p>\n", mDC));

		PhGC_t * aGC = (PhGC_t *) mDC;
		printf("CreateRenderingContext\n");
		
		rv = surf->Init(aGC);
		if (NS_OK == rv)
		  rv = pContext->Init(this, surf);
	  }
	  else
		 rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else
       rv = NS_ERROR_OUT_OF_MEMORY;

    if (NS_OK != rv)
    {
      NS_IF_RELEASE(pContext);
    }

    aContext = pContext;
    return rv;
}

NS_IMETHODIMP nsDeviceContextPh :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
/* REVISIT: These needs to return FALSE if we are printing! */
  if (nsnull == mDC)
     aSupportsWidgets = PR_TRUE;
  else
     aSupportsWidgets = PR_FALSE;		/* while printing! */

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::SupportsNativeWidgets aSupportsWidgets=<%d>\n", aSupportsWidgets));

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetScrollBarDimensions\n"));

  /* Revisit: the scroll bar sizes is a gross guess based on Phab */
  aWidth = mScrollbarWidth * mPixelsToTwips;
  aHeight = mScrollbarHeight * mPixelsToTwips;
  
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
   PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetSystemAttribute\n"));
  nsresult status = NS_OK;

#if 1
  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = NS_RGB(255,255,255);	/* White */
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = NS_RGB(0,0,0);	        /* Black */
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = NS_RGB(255,255,255); //NS_RGB(128,128,128);	 Gray 
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = NS_RGB(0,0,0);	        /* Black */
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = NS_RGB(200,200,200);	/* Dark Gray */
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = NS_RGB(0,0,0);	        /* Black */
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = NS_RGB(255,255,255);	/* White */
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = NS_RGB(0,0,0);	        /* Black */
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = NS_RGB(0,0,255);	        /* Blue */
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = NS_RGB(255,255,255);	/* White */
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight:
        aInfo->mSize = mScrollbarHeight;
        break;
    case eSystemAttr_Size_ScrollbarWidth: 
        aInfo->mSize = mScrollbarWidth;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = 8; // REVISIT - HACK!
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = 20; // REVISIT - HACK!
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = 4;
        break;
    //---------
    // Fonts
    //---------
    case eSystemAttr_Font_Caption:
    case eSystemAttr_Font_Icon:
    case eSystemAttr_Font_Menu:
    case eSystemAttr_Font_MessageBox:
    case eSystemAttr_Font_SmallCaption:
    case eSystemAttr_Font_StatusBar:
    case eSystemAttr_Font_Tooltips:
    case eSystemAttr_Font_Widget:
      status = NS_ERROR_FAILURE;
      break;
  } // switch

#endif

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDrawingSurface\n"));

  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetClientRect(nsRect &aRect)
{
	nsresult rv = NS_OK;

	if ( mSpec )
	{
	  // we have a printer device
	  aRect.x = 0;
	  aRect.y = 0;
	  aRect.width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
	  aRect.height = NSToIntRound(mHeight * mDevUnitsToAppUnits);
	  printf("GetClientRect: printer: %d, %d - %d, %d\n", mWidth, mHeight, aRect.width, aRect.height);
	}
	else
		rv = GetRect ( aRect );

	return rv;
}

/* I need to know the requested font size to finish this function */
NS_IMETHODIMP nsDeviceContextPh :: CheckFontExistence(const nsString& aFontName)
{
  nsresult    ret_code = NS_ERROR_FAILURE;
  char        *fontName = aFontName.ToNewCString();

  if (fontName) 
  {
	FontID *id = NULL;

	if (id = PfFindFont((uchar_t *)fontName, 0, 0))
	{
		if (!mFontLoadCache)
			mFontLoadCache = new nsHashtable(); 

		//nsStringKey key((char *)(id->pucStem));
		nsCStringKey key((char *)(id->pucStem));
		if (!(mFontLoadCache->Exists(&key)))
		{
			char FullFontName[MAX_FONT_TAG];
			PfGenerateFontName((uchar_t *)fontName, nsnull, 8, (uchar_t *)FullFontName);
			PfLoadFont(FullFontName, PHFONT_LOAD_METRICS, nsnull);
			PfLoadMetrics(FullFontName);
			// add this font to the table
			mFontLoadCache->Put(&key, nsnull);
		}
	
		ret_code = NS_OK;
		PfFreeFont(id);
	}

    delete [] fontName;
  }

  /* Return ok and we will map it to some other font later */  
  return ret_code;
}

NS_IMETHODIMP nsDeviceContextPh::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth; // 24;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDepth aDepth=<%d> - Not Implemented\n", aDepth));
  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextPh :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::ConvertPixel - Not Implemented\n"));
  aPixel = NS_TO_PH_RGB(aColor);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
	if (mSpec)
	{
		printf("PRINT: GetDeviceSurfaceDimensions\n");
		aWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);
		aHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);
		return (NS_OK);
	}

  if (mWidth == -1)
    mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

  if (mHeight == -1)
    mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

  aWidth = mWidth;
  aHeight = mHeight;
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDeviceSurfaceDimensions mDevUnitsToAppUnits=<%f> mWidth=<%d> mHeight=<%d>\n", mDevUnitsToAppUnits, mWidth, mHeight));

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh::GetRect(nsRect &aRect)
{
  PRInt32 width, height;
  nsresult rv;
  rv = GetDeviceSurfaceDimensions(width, height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
	//XXX this API should take an CID, use the repository and
	//then QI for the real object rather than casting... MMP

	aContext = new nsDeviceContextPh();

	((nsDeviceContextPh *)aContext)->mSpec = aDevice;
	NS_ADDREF(aDevice);

	//((nsDeviceContextSpecWin *)aDevice)->GetDeviceName(devicename);
	//((nsDeviceContextSpecWin *)aDevice)->GetDriverName(drivername);
	//((nsDeviceContextSpecWin *)aDevice)->GetDEVMODE(hdevmode);

	//devmode = (DEVMODE *)::GlobalLock(hdevmode);
	//HDC dc = ::CreateDC(drivername, devicename, NULL, devmode);

	//  ::SetAbortProc(dc, (ABORTPROC)abortproc);

	//::GlobalUnlock(hdevmode);

	return ((nsDeviceContextPh *)aContext)->Init(NULL, this);
}

nsresult nsDeviceContextPh::SetDPI(PRInt32 aDpi)
{
  const int pt2t = 72;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::SetDPI old DPI=<%d> new DPI=<%d>\n", mDpi,aDpi));

  mDpi = aDpi;
    
  // make p2t a nice round number - this prevents rounding problems
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(aDpi)));
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  // XXX need to reflow all documents
  return NS_OK;
}

int nsDeviceContextPh::prefChanged(const char *aPref, void *aClosure)
{
  nsDeviceContextPh *context = (nsDeviceContextPh*)aClosure;
  nsresult rv;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::prefChanged aPref=<%s>\n", aPref));
  
  if (nsCRT::strcmp(aPref, "browser.display.screen_resolution")==0) 
  {
    PRInt32 dpi;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    rv = prefs->GetIntPref(aPref, &dpi);
    if (NS_SUCCEEDED(rv))
      context->SetDPI(dpi);
  }
  
  return 0;
}


NS_IMETHODIMP nsDeviceContextPh :: BeginDocument(void)
{
  nsresult    ret_code = NS_ERROR_FAILURE;
  int         err;
  PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();

	PhDrawContext_t *dc = PhDCSetCurrent(NULL);
	printf("Begin: %X, ", dc);
	PhDCSetCurrent(dc);

  PpStartJob(pc);
  PpContinueJob(pc);

	dc = PhDCSetCurrent(NULL);
	printf("%X\n", dc);
	PhDCSetCurrent(dc);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: EndDocument(void)
{
  printf("EndDocument\n");
  PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();
  PpSuspendJob(pc);
  PpEndJob(pc);
  return NS_OK;
#if 0
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndDocument - Not Implemented\n"));
  nsresult    ret_code = NS_ERROR_FAILURE;

  /* convert the mSpec into a nsDeviceContextPh */
  if (mSpec)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndDocument - mSpec=<%p>\n", mSpec));
    nsDeviceContextSpecPh * PrintSpec = nsnull;
    mSpec->QueryInterface(kIDeviceContextSpecIID, (void**) &PrintSpec);
    if (PrintSpec)
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndDocument - PriontSpec=<%p>\n", PrintSpec));

      PpPrintContext_t *PrinterContext = nsnull;
  	  PrintSpec->GetPrintContext( PrinterContext );
      if (PrinterContext)
	  {
        PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndDocument - PrinterContext=<%p>\n", PrinterContext));

		int err;
		err = PpPrintClose(PrinterContext);
		if (err == 0)
		{
          PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndDocument - err=<%d>\n", err));
          ret_code = NS_OK;
		}
      }
    }
    NS_RELEASE(PrintSpec);
  }  

  return ret_code;
#endif
}

NS_IMETHODIMP nsDeviceContextPh :: BeginPage(void)
{
	printf("BeginPage\n");
  	return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: EndPage(void)
{
	printf("EndPage\n");
  PpPrintNewPage(((nsDeviceContextSpecPh *)mSpec)->GetPrintContext());
	return NS_OK;
#if 0
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndPage - Not Implemented\n"));

  nsresult    ret_code = NS_ERROR_FAILURE;

  /* convert the mSpec into a nsDeviceContextPh */
  if (mSpec)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndPage - mSpec=<%p>\n", mSpec));
    nsDeviceContextSpecPh * PrintSpec = nsnull;
    mSpec->QueryInterface(kIDeviceContextSpecIID, (void**) &PrintSpec);
    if (PrintSpec)
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndPage - PriontSpec=<%p>\n", PrintSpec));

      PpPrintContext_t *PrinterContext = nsnull;
  	  PrintSpec->GetPrintContext( PrinterContext );
      if (PrinterContext)
	  {
        PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndPage - PrinterContext=<%p>\n", PrinterContext));

		int err;
		err = PpPrintNewPage(PrinterContext);
		if (err == 0)
		{
          PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::EndPage - err=<%d>\n", err));
          ret_code = NS_OK;
		}
      }
    }
    NS_RELEASE(PrintSpec);
  }  
#endif

  return NS_OK;
}

int nsDeviceContextPh :: IsPrinting(void)
{
	if (mSpec)
		return 1;

	return 0;
}

/*
 Get the size and color depth of the display
 */
nsresult nsDeviceContextPh :: GetDisplayInfo(PRInt32 &aWidth, PRInt32 &aHeight, PRUint32 &aDepth)
{
//PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDisplayInfo\n"));
  nsresult    		res = NS_ERROR_FAILURE;
  PhSysInfo_t       SysInfo;
  PhRect_t          rect;
  char              *p = NULL;
  int               inp_grp = 0;
  PhRid_t           rid;
  PhRegion_t        region;

  /* Initialize variables */
  aWidth  = 0;
  aHeight = 0;
  aDepth  = 0;
  
  /* Get the Screen Size and Depth*/
   p = getenv("PHIG");
   if (p)
   {
     inp_grp = atoi(p);

     PhQueryRids( 0, 0, inp_grp, Ph_GRAFX_REGION, 0, 0, 0, &rid, 1 );
     PhWindowQueryVisible( Ph_QUERY_INPUT_GROUP | Ph_QUERY_EXACT, 0, inp_grp, &rect );
     aWidth  = rect.lr.x - rect.ul.x + 1;
     aHeight = rect.lr.y - rect.ul.y + 1;  

     /* Get the System Info for the RID */
     if (!PhQuerySystemInfo(rid, NULL, &SysInfo))
     {
       PR_LOG(PhGfxLog, PR_LOG_ERROR,("nsDeviceContextPh::GetDisplayInfo with aWidget: Error getting SystemInfo\n"));
	   return NS_ERROR_FAILURE;
     }
     else
     {
       /* Make sure the "color_bits" field is valid */
       if (SysInfo.gfx.valid_fields & Ph_GFX_COLOR_BITS)
       {
        aDepth = SysInfo.gfx.color_bits;
        res = NS_OK;
       }
     }	
  }
  else
  {
    printf("The PHIG environment variable must be set, try setting it to 1\n");  
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDisplayInfo aWidth=<%d> aHeight=<%d> aDepth=<%d>\n", aWidth, aHeight, aDepth));
  
  return res;
}

