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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include <math.h>

#include "nspr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"

#include "nsDeviceContextPh.h"
#include "nsRenderingContextPh.h"
#include "nsDeviceContextSpecPh.h"
#include "nsHashtable.h"

#include "nsPhGfxLog.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

nscoord nsDeviceContextPh::mDpi = 96;

static nsHashtable* mFontLoadCache = nsnull;

nsDeviceContextPh :: nsDeviceContextPh( ) {
  NS_INIT_REFCNT();
  
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mDepth = 0 ;
  mSurface = NULL;
  mPixelScale = 1.0f;
  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;

  /* These *MUST* be -1 */
  mWidth = -1;
  mHeight = -1;

  mSpec = nsnull;
  mDC = nsnull;

  mIsPrinting = 0;
	}

nsDeviceContextPh :: ~nsDeviceContextPh( ) {
  nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *)mSurface;

  NS_IF_RELEASE(surf);    //this clears the surf pointer...
  mSurface = nsnull;

	if( mFontLoadCache ) { 
		delete mFontLoadCache;
		mFontLoadCache = nsnull;
		}
	NS_IF_RELEASE( mSpec );
	}

NS_IMPL_ISUPPORTS1(nsDeviceContextPh, nsIDeviceContext)

NS_IMETHODIMP nsDeviceContextPh :: Init( nsNativeWidget aWidget ) {
  float newscale, origscale;
  float a2d,t2d;
    
  CommonInit(NULL);
 
  GetTwipsToDevUnits(newscale);
  GetAppUnitsToDevUnits(origscale);

  GetTwipsToDevUnits(t2d);
  GetAppUnitsToDevUnits(a2d);

  // Call my base class
  return DeviceContextImpl::Init( aWidget );
	}


/* Called for Printing */
nsresult nsDeviceContextPh :: Init( nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext ) {
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
	const PhDim_t *psize;
	PhDim_t dim;
	PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();

	PpPrintGetPC(pc, Pp_PC_PAPER_SIZE, (const void **)&psize );
	mWidthFloat = (float)(psize->w / 10);
	mHeightFloat = (float)(psize->h / 10);
	dim.w = psize->w / 10;
	dim.h = psize->h / 10;
	PpPrintSetPC(pc, INITIAL_PC, 0 , Pp_PC_SOURCE_SIZE, &dim );
	return NS_OK;
	}

void nsDeviceContextPh :: GetPrinterRect( int *width, int *height ) {
	PhDim_t dim;
	const PhDim_t *psize;
	const PhRect_t 	*non_print;
	PhRect_t	rect, margins;
	const char *orientation = 0;
	int			tmp;
	PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();
	
	memset( &rect, 0, sizeof(rect));
	memset( &margins, 0, sizeof(margins));

	PpPrintGetPC(pc, Pp_PC_PAPER_SIZE, (const void **)&psize );
	PpPrintGetPC(pc, Pp_PC_NONPRINT_MARGINS, (const void **)&non_print );
	dim.w = (psize->w - ( non_print->ul.x + non_print->lr.x )) * 100 / 1000;
	dim.h = (psize->h - ( non_print->ul.x + non_print->lr.x )) * 100 / 1000;

	PpPrintGetPC(pc, Pp_PC_ORIENTATION, (const void **)&orientation );

	if( *orientation ) {
		tmp = dim.w;
		dim.w = dim.h;
		dim.h = tmp;
		}

	/* set these to 0 since we do the margins */
	PpPrintSetPC(pc, INITIAL_PC, 0 , Pp_PC_MARGINS, &rect ); 
	PpPrintSetPC(pc, INITIAL_PC, 0 , Pp_PC_SOURCE_SIZE, &dim );

	*width = dim.w;
	*height = dim.h;
	}


void nsDeviceContextPh :: CommonInit( nsNativeDeviceContext aDC ) {
  PRInt32           aWidth, aHeight;
  static int        initialized = 0;


	if( !mScreenManager ) mScreenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");

  if( !initialized ) {
    initialized = 1;
    // Set prefVal the value of the preference "browser.display.screen_resolution"
    // or -1 if we can't get it.
    // If it's negative, we pretend it's not set.
    // If it's 0, it means force use of the operating system's logical resolution.
    // If it's positive, we use it as the logical resolution
    PRInt32 prefVal = -1;
    nsresult res;

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
    if( NS_SUCCEEDED( res ) && prefs ) {
      res = prefs->GetIntPref("browser.display.screen_resolution", &prefVal);
      if( !NS_SUCCEEDED( res ) ) {
        prefVal = 96;
      	}

      prefs->RegisterCallback( "browser.display.screen_resolution", prefChanged, (void *)this );
      if( prefVal >0 ) mDpi = prefVal;
    	}
  	}

  SetDPI( mDpi ); 

	GetDisplayInfo(aWidth, aHeight, mDepth);

	/* Turn off virtual console support... */
	mWidthFloat  = (float) aWidth;
	mHeightFloat = (float) aHeight;
    
  /* Revisit: the scroll bar sizes is a gross guess based on Phab */
  mScrollbarHeight = 17;
  mScrollbarWidth  = 17;
  
  /* Call Base Class */
  DeviceContextImpl::CommonInit( );
	}

NS_IMETHODIMP nsDeviceContextPh :: CreateRenderingContext( nsIRenderingContext *&aContext ) {
  /* I stole this code from windows but its not working yet! */
  nsIRenderingContext *pContext;
  nsresult             rv;
  nsDrawingSurfacePh  *surf;
   
	pContext = new nsRenderingContextPh();
	
	if( nsnull != pContext ) {
	  NS_ADDREF(pContext);

	  surf = new nsDrawingSurfacePh();
	  if( nsnull != surf ) {
			/* I think this is a good idea... not sure if mDC is the right one tho... */
			PhGC_t * aGC = (PhGC_t *) mDC;
		
			rv = surf->Init(aGC);
			if( NS_OK == rv ) rv = pContext->Init(this, surf);
			else rv = NS_ERROR_OUT_OF_MEMORY;
			}
		}
	else rv = NS_ERROR_OUT_OF_MEMORY;

	if( NS_OK != rv ) NS_IF_RELEASE( pContext );

	aContext = pContext;
	NS_ADDREF( pContext ); // otherwise it's crashing after printing
	return rv;
	}

NS_IMETHODIMP nsDeviceContextPh :: SupportsNativeWidgets( PRBool &aSupportsWidgets ) {
/* REVISIT: These needs to return FALSE if we are printing! */
  if( nsnull == mDC ) aSupportsWidgets = PR_TRUE;
	else aSupportsWidgets = PR_FALSE;		/* while printing! */
  return NS_OK;
	}

NS_IMETHODIMP nsDeviceContextPh :: GetScrollBarDimensions( float &aWidth, float &aHeight ) const {
  /* Revisit: the scroll bar sizes is a gross guess based on Phab */
  aWidth = mScrollbarWidth * mPixelsToTwips;
  aHeight = mScrollbarHeight * mPixelsToTwips;
  return NS_OK;
	}

NS_IMETHODIMP nsDeviceContextPh :: GetSystemAttribute( nsSystemAttrID anID, SystemAttrStruct * aInfo ) const {
  nsresult status = NS_OK;

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
    case eSystemAttr_Font_Caption:      // css2
	case eSystemAttr_Font_Icon:
	case eSystemAttr_Font_Menu:
	case eSystemAttr_Font_MessageBox:
	case eSystemAttr_Font_SmallCaption:
	case eSystemAttr_Font_StatusBar:
	case eSystemAttr_Font_Window:       // css3
	case eSystemAttr_Font_Document:
	case eSystemAttr_Font_Workspace:
	case eSystemAttr_Font_Desktop:
	case eSystemAttr_Font_Info:
	case eSystemAttr_Font_Dialog:
	case eSystemAttr_Font_Button:
	case eSystemAttr_Font_PullDownMenu:
	case eSystemAttr_Font_List:
	case eSystemAttr_Font_Field:
	case eSystemAttr_Font_Tooltips:     // moz
	case eSystemAttr_Font_Widget:
	  aInfo->mFont->style       = NS_FONT_STYLE_NORMAL;
	  aInfo->mFont->weight      = NS_FONT_WEIGHT_NORMAL;
	  aInfo->mFont->decorations = NS_FONT_DECORATION_NONE;
	  aInfo->mFont->name.AssignWithConversion("TextFont");
	  switch(anID) {
		  case eSystemAttr_Font_MessageBox:
		     aInfo->mFont->name.AssignWithConversion("MessageFont");
			 break;
		  case eSystemAttr_Font_Tooltips:
		     aInfo->mFont->name.AssignWithConversion("BalloonFont");
		     break;
		  case eSystemAttr_Font_Menu:
		     aInfo->mFont->name.AssignWithConversion("MenuFont");
		     break;
	  }
	  break;
  } // switch

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPh :: GetDrawingSurface( nsIRenderingContext &aContext, nsDrawingSurface &aSurface ) {
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
	}

NS_IMETHODIMP nsDeviceContextPh :: GetClientRect( nsRect &aRect ) {
	nsresult rv = NS_OK;
	if( mIsPrinting ) { //( mSpec )
	  // we have a printer device
	  aRect.x = 0;
	  aRect.y = 0;
	  aRect.width = NSToIntRound(mWidth * mDevUnitsToAppUnits);
	  aRect.height = NSToIntRound(mHeight * mDevUnitsToAppUnits);
		}
	else rv = GetRect ( aRect );
	return rv;
	}

/* I need to know the requested font size to finish this function */
NS_IMETHODIMP nsDeviceContextPh :: CheckFontExistence( const nsString& aFontName ) {
  nsresult    ret_code = NS_ERROR_FAILURE;
  char        *fontName = ToNewCString(aFontName);

  if( fontName ) {
		FontID *id = NULL;

		if( ( id = PfFindFont( (uchar_t *)fontName, 0, 0 ) ) ) {
			if( !mFontLoadCache ) mFontLoadCache = new nsHashtable();

			nsCStringKey key((char *)(id->pucStem));
			if( !mFontLoadCache->Exists( &key ) ) {
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

NS_IMETHODIMP nsDeviceContextPh::GetDepth( PRUint32& aDepth ) {
  aDepth = mDepth; // 24;
  return NS_OK;
	}


NS_IMETHODIMP nsDeviceContextPh :: ConvertPixel( nscolor aColor, PRUint32 & aPixel ) {
  aPixel = NS_TO_PH_RGB(aColor);
  return NS_OK;
	}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceSurfaceDimensions( PRInt32 &aWidth, PRInt32 &aHeight ) {
	if( mIsPrinting ) { //(mSpec)
		aWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);
		aHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);
		return NS_OK;
		}

  if( mWidth == -1 ) mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);
  if( mHeight == -1 ) mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

  aWidth = mWidth;
  aHeight = mHeight;

  return NS_OK;
	}

NS_IMETHODIMP nsDeviceContextPh::GetRect( nsRect &aRect ) {
	if( mScreenManager ) {
    nsCOMPtr<nsIScreen> screen;
    mScreenManager->GetPrimaryScreen( getter_AddRefs( screen ) );
    screen->GetRect(&aRect.x, &aRect.y, &aRect.width, &aRect.height);
    aRect.x = NSToIntRound(mDevUnitsToAppUnits * aRect.x);
    aRect.y = NSToIntRound(mDevUnitsToAppUnits * aRect.y);
    aRect.width = NSToIntRound(mDevUnitsToAppUnits * aRect.width);
    aRect.height = NSToIntRound(mDevUnitsToAppUnits * aRect.height);
		}
	else {
  	PRInt32 width, height;
  	GetDeviceSurfaceDimensions( width, height );
  	aRect.x = 0;
  	aRect.y = 0;
  	aRect.width = width;
  	aRect.height = height;
		}
  return NS_OK;
	}

NS_IMETHODIMP nsDeviceContextPh :: GetDeviceContextFor( nsIDeviceContextSpec *aDevice, nsIDeviceContext *&aContext ) {
	//XXX this API should take an CID, use the repository and
	//then QI for the real object rather than casting... MMP

	aContext = new nsDeviceContextPh();
	if(nsnull != aContext){
		NS_ADDREF(aContext);
	} else {
		return NS_ERROR_OUT_OF_MEMORY;
	}
	((nsDeviceContextPh *)aContext)->mSpec = aDevice;
	NS_ADDREF(aDevice);
	return ((nsDeviceContextPh *)aContext)->Init(NULL, this);
	}

nsresult nsDeviceContextPh::SetDPI( PRInt32 aDpi ) {
  const int pt2t = 72;

  mDpi = aDpi;
    
  // make p2t a nice round number - this prevents rounding problems
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(aDpi)));
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  // XXX need to reflow all documents
  return NS_OK;
	}

int nsDeviceContextPh::prefChanged( const char *aPref, void *aClosure ) {
  nsDeviceContextPh *context = (nsDeviceContextPh*)aClosure;
  nsresult rv;

  if( nsCRT::strcmp(aPref, "browser.display.screen_resolution")==0 )  {
    PRInt32 dpi;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    rv = prefs->GetIntPref(aPref, &dpi);
    if( NS_SUCCEEDED( rv ) ) context->SetDPI( dpi ); 
		}
  return 0;
	}

NS_IMETHODIMP nsDeviceContextPh :: BeginDocument( PRUnichar *t ) {
  PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();
  PpStartJob(pc);
    mIsPrinting = 1;
    mIsPrintingStart = 1;
  return NS_OK;
    }

NS_IMETHODIMP nsDeviceContextPh :: EndDocument( void ) {
  PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();
  PpEndJob(pc);
	mIsPrinting = 0;
  return NS_OK;
	}

NS_IMETHODIMP nsDeviceContextPh :: BeginPage( void ) {
	PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();
	if( !mIsPrintingStart ) PpPrintNewPage( pc );
	PpContinueJob( pc );
	mIsPrintingStart = 0;
	return NS_OK;
    }

NS_IMETHODIMP nsDeviceContextPh :: EndPage( void ) {
	PpPrintContext_t *pc = ((nsDeviceContextSpecPh *)mSpec)->GetPrintContext();
	PpSuspendJob(pc);
	return NS_OK;
    }

int nsDeviceContextPh :: IsPrinting( void ) { return mIsPrinting ? 1 : 0; }

/*
 Get the size and color depth of the display
 */
nsresult nsDeviceContextPh :: GetDisplayInfo( PRInt32 &aWidth, PRInt32 &aHeight, PRUint32 &aDepth ) {
  nsresult    			res = NS_ERROR_FAILURE;
  PhSysInfo_t       SysInfo;
  PhRect_t          rect;
  char              *p = NULL;
  int               inp_grp;
  PhRid_t           rid;

  /* Initialize variables */
  aWidth  = 0;
  aHeight = 0;
  aDepth  = 0;
  
	/* Get the Screen Size and Depth*/
	p = getenv("PHIG");
	if( p ) inp_grp = atoi( p );
	else inp_grp = 1;

	PhQueryRids( 0, 0, inp_grp, Ph_GRAFX_REGION, 0, 0, 0, &rid, 1 );
	PhWindowQueryVisible( Ph_QUERY_INPUT_GROUP | Ph_QUERY_EXACT, 0, inp_grp, &rect );
	aWidth  = rect.lr.x - rect.ul.x + 1;
	aHeight = rect.lr.y - rect.ul.y + 1;  

	/* Get the System Info for the RID */
	if( PhQuerySystemInfo( rid, NULL, &SysInfo ) ) {
		/* Make sure the "color_bits" field is valid */
		if( SysInfo.gfx.valid_fields & Ph_GFX_COLOR_BITS ) {
			aDepth = SysInfo.gfx.color_bits;
			res = NS_OK;
			}
		}
  return res;
	}
