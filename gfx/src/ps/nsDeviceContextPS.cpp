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

#include "nsDeviceContextPS.h"
#include "nsRenderingContextPS.h"
#include "nsString.h"
#include "nsFontMetricsPS.h"
#include "prprf.h"

static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);



#define NS_LETTER_SIZE    0
#define NS_LEGAL_SIZE     1
#define NS_EXECUTIVE_SIZE 2

#define PAGE_WIDTH 612 // Points
#define PAGE_HEIGHT 792 //Points





/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS :: nsDeviceContextPS()
{

  NS_INIT_REFCNT();
  mSpec = nsnull;
  
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS :: ~nsDeviceContextPS()
{
PRInt32 i, n;

  // get rid of the fonts in our mFontMetrics cache
  n= mFontMetrics.Count();
  for (i = 0; i < n; i++){
    nsIFontMetrics* fm = (nsIFontMetrics*) mFontMetrics.ElementAt(i);
    fm->Destroy();
    NS_RELEASE(fm);
  }
  mFontMetrics.Clear();
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextPS, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextPS)
NS_IMPL_RELEASE(nsDeviceContextPS)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextPS.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: Init(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aPrinterContext, HDC aTheDC)
{
float origscale, newscale;
float t2d, a2d;

  mDepth = 1;

  mDC = aTheDC;

  mTwipsToPixels = (float)72.0/(float)NSIntPointsToTwips(72);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  GetTwipsToDevUnits(newscale);
  aPrinterContext->GetTwipsToDevUnits(origscale);
  mPixelScale = newscale / origscale;

  aPrinterContext->GetTwipsToDevUnits(t2d);
  aPrinterContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;
  return  NS_OK;
}

/** ---------------------------------------------------
 *  Create a Postscript RenderingContext.  This will create a RenderingContext
 *  from the deligate RenderingContext and intall that into our Postscript RenderingContext.
 *	@update 12/21/98 dwc
 *  @param aContext -- our newly created Postscript RenderingContextPS
 *  @return -- NS_OK if everything succeeded.
 */
NS_IMETHODIMP nsDeviceContextPS :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
nsresult  rv = NS_ERROR_OUT_OF_MEMORY;

	aContext = new nsRenderingContextPS();
  if (nsnull != aContext){
    NS_ADDREF(aContext);
    rv = ((nsRenderingContextPS*) aContext)->Init(this);
  }else{
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_OK != rv){
    NS_IF_RELEASE(aContext);
  }

  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{

  aSupportsWidgets = PR_FALSE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{

  aWidth = ::GetSystemMetrics(SM_CXVSCROLL) * mDevUnitsToAppUnits;
  aHeight = ::GetSystemMetrics(SM_CXHSCROLL) * mDevUnitsToAppUnits;
  aWidth = 0;
  aHeight = 0;
  return NS_OK;

}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDepth(PRUint32& aDepth)
{
  return(1);    // postscript is 1 bit
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::CreateILColorSpace(IL_ColorSpace*& aColorSpace)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
  aColorSpace = nsnull;
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: CheckFontExistence(const nsString& aFontName)
{

  // XXX this needs to find out if this font is supported for postscript
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{

  aWidth = NSToIntRound((72.0f*8.0f) * mDevUnitsToAppUnits);
  aHeight = NSToIntRound((72.0f*10.0f) * mDevUnitsToAppUnits);

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginDocument(void)
{
PrintInfo* pi = new PrintInfo();
PrintSetup* ps = new PrintSetup();

  mPrintContext = new PSContext();
  memset(mPrintContext, 0, sizeof(struct PSContext_));
  memset(ps, 0, sizeof(struct PrintSetup_));
  memset(pi, 0, sizeof(struct PrintInfo_));
 
  ps->top = 0;                        /* Margins  (PostScript Only) */
  ps->bottom = 0;
  ps->left = 0;
  ps->right = 0;
  ps->width = PAGE_WIDTH;            /* Paper size, # of cols for text xlate */
  ps->height = PAGE_HEIGHT;
  ps->header = "header";
  ps->footer = "footer";
  ps->sizes = NULL;
  ps->reverse = 1;                 /* Output order */
  ps->color = TRUE;                /* Image output */
  ps->deep_color = TRUE;		      /* 24 bit color output */
  ps->landscape = FALSE;           /* Rotated output */
  ps->underline = TRUE;            /* underline links */
  ps->scale_images = TRUE;           /* Scale unsized images which are too big */
  ps->scale_pre = FALSE;		      /* do the pre-scaling thing */
  ps->dpi = 72.0f;                 /* dpi for externally sized items */
  ps->rules = 1.0f;			          /* Scale factor for rulers */
  ps->n_up = 0;                        /* cool page combining */
  ps->bigger = 1;                      /* Used to init sizes if sizesin NULL */
  ps->paper_size = NS_LEGAL_SIZE;     /* Paper Size(letter,legal,exec,a4) */
  ps->prefix = "";                    /* For text xlate, prepended to each line */
  ps->eol = "";			   /* For text translation, line terminator */
  ps->bullet = "+";                    /* What char to use for bullets */

  URL_Struct_* url = new URL_Struct_;
  memset(url, 0, sizeof(URL_Struct_));
  ps->url = url;         /* url of doc being translated */
  char filename[30];
  static char g_nsPostscriptFileCount = 0; //('a');
  char ext[30];
  sprintf(ext,"%d",g_nsPostscriptFileCount);
  sprintf(filename,"file%s.ps", ext); 
  g_nsPostscriptFileCount++;
  ps->out = fopen(filename , "w");                     /* Where to send the output */
  ps->filename = filename;                  /* output file name, if any */
  ps->completion = NULL; /* Called when translation finished */
  ps->carg = NULL;                      /* Data saved for completion routine */
  ps->status = 0;                      /* Status of URL on completion */
		/* "other" font is for encodings other than iso-8859-1 */
  ps->otherFontName[0] = NULL;		   
  				/* name of "other" PostScript font */
  ps->otherFontInfo[0] = NULL;	   
  				/* font info parsed from "other" afm file */
  ps->otherFontCharSetID = 0;	   /* charset ID of "other" font */
  ps->cx = NULL;                   /* original context, if available */

  pi->page_height = PAGE_HEIGHT * 10;	/* Size of printable area on page */
  pi->page_width = PAGE_WIDTH * 10;	/* Size of printable area on page */
  pi->page_break = 0;	/* Current page bottom */
  pi->page_topy = 0;	/* Current page top */
  pi->phase = 0;
	/*
	** CONTINUE SPECIAL
	**	The table print code maintains these
	*/
 
  pi->pages=NULL;		/* Contains extents of each page */

  pi->pt_size = 0;		/* Size of above table */
  pi->n_pages = 0;		/* # of valid entries in above table */
	/*
	** END SPECIAL
	*/

  pi->doc_title="Test Title";	/* best guess at title */
  pi->doc_width = 0;	/* Total document width */
  pi->doc_height = 0;	/* Total document height */

  mPrintContext->prInfo = pi;

  // begin the document
  xl_initialize_translation(mPrintContext, ps);
  xl_begin_document(mPrintContext);	
  mPrintSetup = ps;
  
  // begin the page
  xl_begin_page(mPrintContext, 1); 

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::EndDocument(void)
{
   // end the page
  xl_end_page(mPrintContext, 1);

  // end the document
  xl_end_document(mPrintContext);
  xl_finalize_translation(mPrintContext);

  // Cleanup things allocated along the way
  if (nsnull != mPrintContext){
	 if (nsnull != mPrintContext->prInfo)
		 delete mPrintContext->prInfo;

     if (nsnull != mPrintContext->prSetup)
		 delete mPrintContext->prSetup;

     delete mPrintContext;
  }

  if (nsnull != mPrintSetup)
	  delete mPrintSetup;

  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginPage(void)
{
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::EndPage(void)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  aPixel = aColor;
  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextPS::GetMetricsFor(const nsFont& aFont, nsIFontMetrics  *&aMetrics)
{
PRInt32         n,cnt;
nsresult        rv;

  // First check our cache
  n = mFontMetrics.Count();

  for (cnt = 0; cnt < n; cnt++){
    aMetrics = (nsIFontMetrics*) mFontMetrics.ElementAt(cnt);

    const nsFont  *font;
    aMetrics->GetFont(font);
    if (aFont.Equals(*font)){
      NS_ADDREF(aMetrics);
      return NS_OK;
    }
  }

  // It's not in the cache. Get font metrics and then cache them.
  nsIFontMetrics* fm = new nsFontMetricsPS();
  if (nsnull == fm) {
    aMetrics = nsnull;
    return NS_ERROR_FAILURE;
  }

  rv = fm->Init(aFont, this);

  if (NS_OK != rv) {
    aMetrics = nsnull;
    return rv;
  }

  mFontMetrics.AppendElement(fm);
  NS_ADDREF(fm);     // this is for the cache


  for (cnt = 0; cnt < n; cnt++){
    aMetrics = (nsIFontMetrics*) mFontMetrics.ElementAt(cnt);
    const nsFont  *font;
    aMetrics->GetFont(font);
  }

  NS_ADDREF(fm);      // this is for the routine that needs this font
  aMetrics = fm;
  return NS_OK;
}
