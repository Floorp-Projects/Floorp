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

#include "nsDeviceContextPh.h"
#include "nsRenderingContextPh.h"
#include "nsDeviceContextSpecPh.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "il_util.h"
#include "nsPhGfxLog.h"

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);


nsDeviceContextPh :: nsDeviceContextPh()
  : DeviceContextImpl()
{
  mSurface = NULL;
  mPaletteInfo.isPaletteDevice = PR_FALSE;
  mPaletteInfo.sizePalette = 0;
  mPaletteInfo.numReserved = 0;
  mPaletteInfo.palette = NULL;
  mPixelScale = 1.0f;
  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
  mWidth = 0;
  mHeight = 0;
  mSpec = nsnull;
  mDC = nsnull;
}

nsDeviceContextPh :: ~nsDeviceContextPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::~nsDeviceContextPh destructor called mSurface=<%p>\n", mSurface));

  nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *)mSurface;

  NS_IF_RELEASE(surf);    //this clears the surf pointer...
  mSurface = nsnull;

  if (NULL != mPaletteInfo.palette)
    PR_Free(mPaletteInfo.palette);

#if 0
  if (NULL != mDC)
  {
    ::DeleteDC(mDC);
    mDC = NULL;
  }
#endif

  NS_IF_RELEASE(mSpec);
}

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);

NS_IMETHODIMP nsDeviceContextPh :: Init(nsNativeWidget aWidget)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with aWidget aWidget=<%p>\n", aWidget));

  CommonInit(NULL);
 
  {
  float newscale, origscale;
  
  GetTwipsToDevUnits(newscale);
  GetAppUnitsToDevUnits(origscale);
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init newscale=<%f> origscale=<%f>\n", newscale, origscale));
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init mPixelScale=<%f>\n", mPixelScale));

  float a2d,t2d;
  GetTwipsToDevUnits(t2d);
  GetAppUnitsToDevUnits(a2d);
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init t2d=<%f> a2d=<%f> mTwipsToPixels=<%f>\n", t2d,a2d,mTwipsToPixels));

  } 
 
 
  // Call my base class
  return DeviceContextImpl::Init(aWidget);
}


/* Called for Printing */
nsresult nsDeviceContextPh :: Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with nsNativeDeviceContext aContext=<%p> aOrigContext=<%p>\n", aContext, aOrigContext));

  nsDeviceContextSpecPh * PrintSpec = nsnull;
  PpPrintContext_t      *PrinterContext = nsnull;
  float origscale, newscale;
  float t2d, a2d;
    
  /* convert the mSpec into a nsDeviceContextPh */
  if (mSpec)
  {
    mSpec->QueryInterface(kIDeviceContextSpecIID, (void**) &PrintSpec);
    if (PrintSpec)
    {
  	  PrintSpec->GetPrintContext( PrinterContext );

      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init PpPrinterContext=<%p>\n", PrinterContext));

    }	    
  }
  
  mDC = aContext;

  CommonInit(mSpec);		/* HACK! */

#if 1
  GetTwipsToDevUnits(newscale);
  aOrigContext->GetAppUnitsToDevUnits(origscale);
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init printing newscale=<%f> origscale=<%f>\n", newscale, origscale));
  
  mPixelScale = newscale / origscale;
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init printing mPixelScale=<%f>\n", mPixelScale));

  aOrigContext->GetTwipsToDevUnits(t2d);
  aOrigContext->GetAppUnitsToDevUnits(a2d);
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init printing t2d=<%f> a2d=<%f> mTwipsToPixels=<%f>\n", t2d,a2d,mTwipsToPixels));

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init mAppUnitsToDevUnits=<%f>\n", mAppUnitsToDevUnits));
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init mDevUnitsToAppUnits=<%f>\n", mDevUnitsToAppUnits));
#endif

  return NS_OK;
}

void nsDeviceContextPh :: CommonInit(nsNativeDeviceContext aDC)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit mSpec=<%p>\n", mSpec));

  PRInt32           aWidth, aHeight;
  nsresult          err;
  static nscoord    dpi = 96;
  static int        initialized = 0;

  if (!initialized)
  {
    initialized = 1;
    nsIPref* prefs = nsnull;
    nsresult res = nsServiceManager::GetService(kPrefCID, kIPrefIID,
      (nsISupports**) &prefs);
    if (NS_SUCCEEDED(res) && prefs)
	{
      PRInt32 intVal = 96;
      res = prefs->GetIntPref("browser.screen_resolution", &intVal);
      if (NS_SUCCEEDED(res))
	  {
        if (intVal)
		{
          dpi = intVal;
        }
//        else {
//          // Compute dpi of display
//          float screenWidth = float(::gdk_screen_width());
//          float screenWidthIn = float(::gdk_screen_width_mm()) / 25.4f;
//          dpi = nscoord(screenWidth / screenWidthIn);
//        }
      }
      nsServiceManager::ReleaseService(kPrefCID, prefs);
    }
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit dpi=<%d>\n", dpi));

  mTwipsToPixels = float(dpi) / float(NSIntPointsToTwips(72));
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit mPixelsToTwips=<%f>\n", mPixelsToTwips));
  
//  if (aDC != NULL)
//  {
//    mWidthFloat  = (float) 480;
//    mHeightFloat = (float) 320;  
//  }
//  else
  {
    err = GetDisplayInfo(aWidth, aHeight, mDepth);
    if (err == NS_ERROR_FAILURE)
      abort();

    mWidthFloat  = (float) aWidth;
    mHeightFloat = (float) aHeight;  
  }
    
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit with aWidget: Screen Size (%f,%f)\n", mWidthFloat,mHeightFloat));

  int res;
  PgColor_t color[_Pg_MAX_PALETTE];  

  res = PgGetPalette(color);
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CommonInit PgGetPalette err=<%d>\n", res));
  if (res == -1)
  {
    mPaletteInfo.isPaletteDevice = PR_FALSE;
    mPaletteInfo.sizePalette = 0;
    mPaletteInfo.numReserved = 0;
    mPaletteInfo.palette = NULL;
  }
  else
  {
    mPaletteInfo.isPaletteDevice = PR_TRUE;
    mPaletteInfo.sizePalette = _Pg_MAX_PALETTE;
    mPaletteInfo.numReserved = 16;  				/* GUESS */
    mPaletteInfo.palette = PR_Malloc(sizeof(color));
    if (NULL != mPaletteInfo.palette)
	{
      /* If the memory was allocated */
	  memcpy(mPaletteInfo.palette, color, sizeof(color));
	}
  }

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
#if 0
  /* This is untested and may be breaking things */
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CreateRenderingContext - mDC=<%p>\n", mDC));

		PhGC_t * aGC = (PhGC_t *) mDC;
        rv = surf->Init(aGC);
        if (NS_OK == rv)
#endif
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

NS_IMETHODIMP nsDeviceContextPh :: GetCanonicalPixelScale(float &aScale) const
{
//  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetCanonicalPixelScale <%f>\n", mPixelScale));

  aScale = mPixelScale;		// mPixelScale should be 1.0f

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetScrollBarDimensions\n"));

  /* Revisit: the scroll bar sizes is a gross guess based on Phab */
  aWidth = 17.0f *  mPixelsToTwips;
  aHeight = 17.0f *  mPixelsToTwips;
  
  return NS_OK;
}

nsresult GetSysFontInfo( PhGC_t &aGC, nsSystemAttrID anID, nsFont * aFont) 
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetSysFontInfo - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetSystemAttribute - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDrawingSurface - Not Implemented\n"));
/*
  if (NULL == mSurface)
  {
     aContext.CreateDrawingSurface(nsnull, 0, mSurface);
  }
		
  aSurface = mSurface;
*/
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetClientRect(nsRect &aRect)
{
  PRInt32 width, height;
  nsresult rv;
	
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetClientRect\n"));

  rv = GetDeviceSurfaceDimensions(width, height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}

/* I need to know the requested font size to finish this function */
NS_IMETHODIMP nsDeviceContextPh :: CheckFontExistence(const nsString& aFontName)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CheckFontExistence\n" ));

  nsresult    ret_code = NS_ERROR_FAILURE;
  char        *fontName = aFontName.ToNewCString();

  if( fontName )
  {
    int         MAX_FONTDETAIL = 30;
    FontDetails fDetails[MAX_FONTDETAIL];
    int         fontcount;
  
    fontcount = PfQueryFonts('a', PHFONT_ALL_FONTS, fDetails, MAX_FONTDETAIL);
    if (fontcount >= MAX_FONTDETAIL)
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CheckFontExistence ERROR - Font Array should be increased!\n"));
    }

    if (fontcount)
    {
      int index;
      for(index=0; index < fontcount; index++)
      {
        //PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CheckFontExistence  comparing <%s> with <%s>\n", fontName, fDetails[index].desc));
        if (strncmp(fontName, fDetails[index].desc, strlen(fontName)) == 0)
        {
          //PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CheckFontExistence  Found the font <%s>\n", fDetails[index].desc));
          ret_code = NS_OK;
          break;	  
        }
      }
    }

    if( ret_code == NS_ERROR_FAILURE )
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CheckFontExistence  Did not find the font <%s>\n", fontName));
    }
  
    delete [] fontName;
  }

  /* Return ok and we will map it to some other font later */  
  return ret_code;
}

NS_IMETHODIMP nsDeviceContextPh::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth; // 24; /* REVISIT: Should be mDepth, kedl, FIXME */
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDepth aDepth=<%d> - Not Implemented\n", aDepth));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetILColorSpace - Calling base class method\n"));

  /* This used to return NS_OK but I chose to call my base class instead */
  return DeviceContextImpl::GetILColorSpace(aColorSpace);
}

NS_IMETHODIMP nsDeviceContextPh::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetPaletteInfo - Not Implemented\n"));

  aPaletteInfo.isPaletteDevice = mPaletteInfo.isPaletteDevice;
  aPaletteInfo.sizePalette = mPaletteInfo.sizePalette;
  aPaletteInfo.numReserved = mPaletteInfo.numReserved;
  aPaletteInfo.palette = NULL;
  if (NULL != mPaletteInfo.palette)
  {
    aPaletteInfo.palette = PR_Malloc( sizeof(PgColor_t) * _Pg_MAX_PALETTE );
    if (NULL != aPaletteInfo.palette)
    {
      /* If the memory was allocated */
	  memcpy(aPaletteInfo.palette, mPaletteInfo.palette, sizeof(PgColor_t)*_Pg_MAX_PALETTE);
	}
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::ConvertPixel - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDeviceSurfaceDimensions mDevUnitsToAppUnits=<%f> mWidth=<%d> mHeight=<%d>\n", mDevUnitsToAppUnits, mWidth, mHeight));

  if (mWidth == -1)
    mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

  if (mHeight == -1)
    mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

  aWidth = mWidth;
  aHeight = mHeight;
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDeviceSurfaceDimensions mDevUnitsToAppUnits=<%f> mWidth=<%d> mHeight=<%d>\n", mDevUnitsToAppUnits, mWidth, mHeight));

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                        nsIDeviceContext *&aContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDeviceContextFor\n"));

  aContext = new nsDeviceContextPh();
  ((nsDeviceContextPh*) aContext)->mSpec = aDevice;
  NS_ADDREF(aDevice);
  
  return ((nsDeviceContextPh *) aContext)->Init((nsIDeviceContext*)aContext, (nsIDeviceContext*)this);
}

NS_IMETHODIMP nsDeviceContextPh :: BeginDocument(void)
{
  nsresult    ret_code = NS_ERROR_FAILURE;
  int         err;
  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::BeginDocument - Not Implemented\n"));

  /* convert the mSpec into a nsDeviceContextPh */
  if (mSpec)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::BeginDocument - mSpec=<%p>\n", mSpec));
    nsDeviceContextSpecPh * PrintSpec = nsnull;
    mSpec->QueryInterface(kIDeviceContextSpecIID, (void**) &PrintSpec);
    if (PrintSpec)
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::BeginDocument - PriontSpec=<%p>\n", PrintSpec));

      PpPrintContext_t *PrinterContext = nsnull;
  	  PrintSpec->GetPrintContext( PrinterContext );
      if (PrinterContext)
	  {
        PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::BeginDocument - PrinterContext=<%p>\n", PrinterContext));
#if 1
		PhRect_t Rect = {1500,1500,1500,1500};
		err=PpPrintSetPC( PrinterContext, INTERACTIVE_PC, 0, Pp_PC_MARGINS, &Rect);

		PhDim_t Dim = {720,960};
		err=PpPrintSetPC( PrinterContext, INITIAL_PC, 0, Pp_PC_SOURCE_SIZE, &Dim);

//		PhPoint_t Point = {100,100};
//		err=PpPrintSetPC( PrinterContext, INITIAL_PC, 0, Pp_PC_SCALE, &Point);

#endif
		PhDrawContext_t *DrawContext = nsnull;
		DrawContext = PpPrintStart(PrinterContext);
		if (DrawContext)
		{
          PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::BeginDocument - DrawContext=<%p>\n", DrawContext));
          ret_code = NS_OK;
		}
      }
    }
    NS_RELEASE(PrintSpec);
  }  

  return ret_code;
}

NS_IMETHODIMP nsDeviceContextPh :: EndDocument(void)
{
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
}

NS_IMETHODIMP nsDeviceContextPh :: BeginPage(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::BeginPage - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: EndPage(void)
{
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

  return ret_code;
}

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

     PhQueryRids( 0, 0, inp_grp, Ph_INPUTGROUP_REGION, 0, 0, 0, &rid, 1 );
     PhRegionQuery( rid, &region, &rect, NULL, 0 );
     inp_grp = region.input_group;
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

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDisplayInfo aWidth=<%d> aHeight=<%d> aDepth=<%d>\n", aWidth, aHeight, aDepth));
  
  return res;
}

