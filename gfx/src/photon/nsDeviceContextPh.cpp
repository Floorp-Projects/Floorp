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
#include "../ps/nsDeviceContextPS.h"
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
//  mDC = NULL;
  mPixelScale = 1.0f;
  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
  mWidth = 0;
  mHeight = 0;
  mStupid = 1;
  mSpec = nsnull;
  mDC = nsnull;
}

nsDeviceContextPh :: ~nsDeviceContextPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::~nsDeviceContextPh destructor called mSurface=<%p>\n", mSurface));

  nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *)mSurface;

  NS_IF_RELEASE(surf);    //this clears the surf pointer...
  mSurface = nsnull;

#if 0
  if (NULL != mPaletteInfo.palette)
    ::DeleteObject((HPALETTE)mPaletteInfo.palette);

  if (NULL != mDC)
  {
    ::DeleteDC(mDC);
    mDC = NULL;
  }
#endif

  NS_IF_RELEASE(mSpec);
}

NS_IMETHODIMP nsDeviceContextPh :: Init(nsNativeWidget aWidget)
{
  PhSysInfo_t SysInfo;
  PhRect_t                        rect;
  PtArg_t                         arg[15];
  char                            c, *p;
  int                             inp_grp = 0;
  PhRid_t                         rid;
  PhRegion_t                      region;
														
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with aWidget aWidget=<%p>\n", aWidget));
  
  // this is a temporary hack!
  mPixelsToTwips = 15.0f;		// from debug under windows
  mTwipsToPixels = 1 / mPixelsToTwips;  

 /* Get the Screen Size */
 p = getenv("PHIG");
 if (p)
   inp_grp = atoi(p);
 else
   abort();

 PhQueryRids( 0, 0, inp_grp, Ph_INPUTGROUP_REGION, 0, 0, 0, &rid, 1 );
 PhRegionQuery( rid, &region, &rect, NULL, 0 );
 inp_grp = region.input_group;
 PhWindowQueryVisible( Ph_QUERY_INPUT_GROUP | Ph_QUERY_EXACT, 0, inp_grp, &rect );
 mWidthFloat  = (float) rect.lr.x - rect.ul.x+1;
 mHeightFloat = (float) rect.lr.y - rect.ul.y+1;  

 PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with aWidget: Screen Size (%f,%f)\n", mWidthFloat,mHeightFloat));


 /* Get the System Info for the RID */
 if (!PhQuerySystemInfo(rid, NULL, &SysInfo))
 {
    PR_LOG(PhGfxLog, PR_LOG_ERROR,("nsDeviceContextPh::Init with aWidget: Error getting SystemInfo\n"));
 }
 else
 {
   /* Make sure the "color_bits" field is valid */
   if (SysInfo.gfx.valid_fields & Ph_GFX_COLOR_BITS)
   {
     mDepth = SysInfo.gfx.color_bits;
     PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with aWidget: Pixel Depth = %d\n", mDepth));
   }
 }	 

  /* Palette Information: just a guess!  REVISIT */
  mPaletteInfo.isPaletteDevice = 1;
  mPaletteInfo.sizePalette = 255;
  mPaletteInfo.numReserved = 16;

  // Call my base class
  return DeviceContextImpl::Init(aWidget);
}

//local method...

nsresult nsDeviceContextPh :: Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::Init with nsNativeDeviceContext aContext=<%p> aOrigContext=<%p>\n", aContext, aOrigContext));

  /* REVISIT: This is not right, but it gets the job done for right now */
  mPixelsToTwips =  1.0f;
  mTwipsToPixels = 1 / mPixelsToTwips;  
  mDC = aContext;

  return NS_OK;
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
//        rv = surf->Init(mDC);
//        if (NS_OK == rv)
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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetCanonicalPixelScale <%f>\n", mPixelScale));

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

/* I need to know the requested font size to finish this function */
NS_IMETHODIMP nsDeviceContextPh :: CheckFontExistence(const nsString& aFontName)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::CheckFontExistence\n" ));

  nsresult    ret_code = NS_ERROR_FAILURE;
  static char *fontName = aFontName.ToNewCString();

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
  aDepth = 24;	//kedl, FIXME!
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDepth - Not Implemented\n"));
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
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::ConvertPixel - Not Implemented\n"));
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  if (mStupid)
  {
	mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);
	mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);
	mStupid=0;
  }
/*
  if (mWidth == -1)
	mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

  if (mHeight == -1)
    mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);
*/

  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsDeviceContextPh::GetDeviceSurfaceDimensions mDevUnitsToAppUnits=<%f> mWidth=<%d> mHeight=<%f>\n", mDevUnitsToAppUnits, mWidth, mHeight));

  aWidth = mWidth;
  aHeight = mHeight;

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
