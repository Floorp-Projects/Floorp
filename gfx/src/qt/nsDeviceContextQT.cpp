/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <math.h>

#include "nspr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "il_util.h"
#include "nsCRT.h"
#include "nsDeviceContextQT.h"
#include "nsFontMetricsQT.h"
#include "nsGfxCIID.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"

#include <qpaintdevicemetrics.h>
#include <qscrollbar.h>
#include <qpalette.h>
#include <qapplication.h>
#include <qstyle.h>

#include "nsIScreenManager.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nscoord nsDeviceContextQT::mDpi = 96;

NS_IMPL_ISUPPORTS1(nsDeviceContextQT, nsIDeviceContext)

nsDeviceContextQT::nsDeviceContextQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::nsDeviceContextQT\n"));
    NS_INIT_REFCNT();
    mTwipsToPixels = 1.0;
    mPixelsToTwips = 1.0;
    mDepth = 0 ;
    mPaletteInfo.isPaletteDevice = PR_FALSE;
    mPaletteInfo.sizePalette = 0;
    mPaletteInfo.numReserved = 0;
    mPaletteInfo.palette = NULL;
    mNumCells = 0;
    mWidthFloat = 0.0f;
    mHeightFloat = 0.0f;
    mWidth = -1;
    mHeight = -1;
}

nsDeviceContextQT::~nsDeviceContextQT()
{
  PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::~nsDeviceContextQT\n"));
  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(kPrefCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    prefs->UnregisterCallback("browser.display.screen_resolution",
                              prefChanged, (void *)this);
  }
}

NS_IMETHODIMP nsDeviceContextQT::Init(nsNativeWidget aNativeWidget)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::Init\n"));
    PRBool    bCleanUp = PR_FALSE;
    QWidget * pWidget  = nsnull;

    mWidget = (QWidget *) aNativeWidget;

    if (mWidget != nsnull)
        pWidget = mWidget;
    else {
        pWidget = new QWidget();
        bCleanUp = PR_TRUE;
    }
    QPaintDeviceMetrics qPaintMetrics(pWidget);

    nsresult ignore;
    nsCOMPtr<nsIScreenManager> sm(do_GetService("@mozilla.org/gfx/screenmanager;1",&ignore));
    if (sm) {
      nsCOMPtr<nsIScreen> screen;
      sm->GetPrimaryScreen(getter_AddRefs(screen));
      if (screen) {
        PRInt32 x, y, width, height, depth;
        screen->GetAvailRect(&x,&y,&width,&height);
        screen->GetPixelDepth(&depth);
        mWidthFloat = float(width);
        mHeightFloat = float(height);
        mDepth = NS_STATIC_CAST(PRUint32,depth);
      }
    }
    static int initialized = 0;
    if (!initialized) {
      initialized = 1;

      // Set prefVal the value of the preference "browser.display.screen_resolution"
      // or -1 if we can't get it.
      // If it's negative, we pretend it's not set.
      // If it's 0, it means force use of the operating system's logical resolution.
      // If it's positive, we use it as the logical resolution
      PRInt32 prefVal = -1;
      nsresult res;

      NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
      if (NS_SUCCEEDED(res) && prefs) {
        res = prefs->GetIntPref("browser.display.screen_resolution", &prefVal);
        if (! NS_SUCCEEDED(res)) {
          prefVal = -1;
        }
        prefs->RegisterCallback("browser.display.screen_resolution", prefChanged,
                                (void *)this);
      }

      // Set OSVal to what the operating system thinks the logical resolution is.
      PRInt32 OSVal = qPaintMetrics.logicalDpiX();

      if (prefVal > 0) {
        // If there's a valid pref value for the logical resolution,
        // use it.
        mDpi = prefVal;
      } else if ((prefVal == 0) || (OSVal > 96)) {
        // Either if the pref is 0 (force use of OS value) or the OS
        // value is bigger than 96, use the OS value.
        mDpi = OSVal;
      } else {
        // if we couldn't get the pref or it's negative, and the OS
        // value is under 96ppi, then use 96.
        mDpi = 96;
      }
    }
    // Now for some wacky heuristics. 

    SetDPI(mDpi);

#ifdef DEBUG
    static PRBool once = PR_TRUE;
    if (once) {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDeviceContextQT::Init dpi=%d, t2p=%d, p2t=%d\n",
                mDpi, 
                mTwipsToPixels, 
                mPixelsToTwips));
        printf("GFX: dpi=%d t2p=%g p2t=%g depth=%d\n",
               mDpi,mTwipsToPixels,mPixelsToTwips,mDepth);
        once = PR_FALSE;
    }
#endif

    QScrollBar * sb = new QScrollBar(nsnull, nsnull);
    if (sb) {
        sb->setOrientation(QScrollBar::Vertical);
        QSize size = sb->sizeHint();

        mScrollbarWidth = size.width();

        sb->setOrientation(QScrollBar::Horizontal);
        size = sb->sizeHint();

        mScrollbarHeight = size.height();
    }
    //QSize size = pWidget->frameSize();
    QRect rect = pWidget->frameGeometry();

    mWindowBorderWidth = rect.width();
    mWindowBorderHeight = rect.height();

    DeviceContextImpl::CommonInit();

    delete sb;

    if (bCleanUp)
        delete pWidget;
    return NS_OK;
}

NS_IMETHODIMP 
nsDeviceContextQT::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::CreateRenderingContext\n"));
  nsIRenderingContext *pContext;
  nsresult             rv;
  nsDrawingSurfaceQT  *surf;
  QPaintDevice *pDev = nsnull;

  if (mWidget)
    pDev = (QPaintDevice*)mWidget;
 
  // to call init for this, we need to have a valid nsDrawingSurfaceQT created
  pContext = new nsRenderingContextQT();
 
  if (nsnull != pContext)
  {
    NS_ADDREF(pContext);
 
    // create the nsDrawingSurfaceQT
    surf = new nsDrawingSurfaceQT();
 
    if (surf)
    {
        QPainter *gc = new QPainter();
 
        // init the nsDrawingSurfaceQT
        if (pDev)
          rv = surf->Init(pDev,gc);
        else
          rv = surf->Init(gc,10,10,0);
 
        if (NS_OK == rv)
          // Init the nsRenderingContextQT
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

NS_IMETHODIMP 
nsDeviceContextQT::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::SupportsNativeWidgets\n"));
    //XXX it is very critical that this not lie!! MMP
    // read the comments in the mac code for this
    aSupportsWidgets = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::GetScrollBarDimensions(float &aWidth, 
                                                        float &aHeight) const
{
    aWidth = mScrollbarWidth * mPixelsToTwips;
    aHeight = mScrollbarHeight * mPixelsToTwips;
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsDeviceContextQT::GetScrollBarDimensions: %dx%d\n",
            aWidth,
            aHeight));
 
    return NS_OK;
}

NS_IMETHODIMP 
nsDeviceContextQT::GetSystemAttribute(nsSystemAttrID anID, 
                                      SystemAttrStruct * aInfo) const
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetSystemAttribute\n"));
    nsresult    status      = NS_OK;
    QPalette    palette     = qApp->palette();
    QColorGroup normalGroup = palette.inactive();
    QColorGroup activeGroup = palette.active();
    QColorGroup disabledGroup = palette.disabled();

    switch (anID) 
    {
        //---------
        // Colors
        //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = normalGroup.background().rgb();
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = normalGroup.foreground().rgb();
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = normalGroup.background().rgb();
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = normalGroup.foreground().rgb();
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = activeGroup.background().rgb();
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = activeGroup.foreground().rgb();
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = normalGroup.highlight().rgb();
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = normalGroup.shadow().rgb();
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = normalGroup.background().rgb();
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = normalGroup.text().rgb();
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = activeGroup.background().rgb();
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = activeGroup.foreground().rgb();
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
        aInfo->mSize = mWindowBorderWidth;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = mWindowBorderHeight;
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
    case eSystemAttr_Font_Window:                   // css3
    case eSystemAttr_Font_Document:
    case eSystemAttr_Font_Workspace:
    case eSystemAttr_Font_Desktop:
    case eSystemAttr_Font_Info:
    case eSystemAttr_Font_Dialog:
    case eSystemAttr_Font_Button:
    case eSystemAttr_Font_PullDownMenu:
    case eSystemAttr_Font_List:
    case eSystemAttr_Font_Field:
    case eSystemAttr_Font_Tooltips: 
    case eSystemAttr_Font_Widget:
        status = NS_ERROR_FAILURE;
        break;
    }

    return status;
}

NS_IMETHODIMP 
nsDeviceContextQT::GetDrawingSurface(nsIRenderingContext &aContext, 
                                     nsDrawingSurface &aSurface)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetDrawingSurface\n"));
    aContext.CreateDrawingSurface(nsnull, 0, aSurface);
    return (nsnull == aSurface) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;  
}

NS_IMETHODIMP nsDeviceContextQT::ConvertPixel(nscolor aColor, 
                                              PRUint32 & aPixel)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::ConvertPixel\n"));
    QColor color(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
    aPixel = color.rgb();

    return NS_OK;
}


NS_IMETHODIMP nsDeviceContextQT::CheckFontExistence(const nsString& aFontName)
{
    float       t2d;
    GetTwipsToDevUnits(t2d);
    nsresult    rv = NS_ERROR_FAILURE;
  
    char* fontName = aFontName.ToNewCString();

    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsDeviceContextQT::CheckFontExistence: looking for %s\n",
            fontName));
  
    QFont font(fontName);

    QFontInfo fontInfo(font);

    if (fontInfo.exactMatch() ||
        fontInfo.family() == font.family())
    {
        rv = NS_OK;
    }

    delete [] fontName;

    if (rv)
    {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDeviceContextQT::CheckFontExistence: didn't find it \n"));
    }
    else
    {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDeviceContextQT::CheckFontExistence: found it\n"));
    }
  
    return rv;
}

NS_IMETHODIMP nsDeviceContextQT::GetDeviceSurfaceDimensions(PRInt32 &aWidth, 
                                                            PRInt32 &aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetDeviceSurfaceDimensions\n"));
    if (-1 == mWidth)
      mWidth =  NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

    if (-1 == mHeight)
      mHeight =  NSToIntRound(mHeightFloat * mDevUnitsToAppUnits);

    aWidth = mWidth;
    aHeight = mHeight;

    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::GetRect(nsRect &aRect)
{
    PRInt32 width, height;
    nsresult rv;
 
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetRect\n"));
    rv = GetDeviceSurfaceDimensions(width, height);
    aRect.x = 0;
    aRect.y = 0;
    aRect.width = width;
    aRect.height = height;
    return rv;
}

NS_IMETHODIMP nsDeviceContextQT::GetClientRect(nsRect &aRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetClientRect\n"));
    return GetRect(aRect);
}

NS_IMETHODIMP 
nsDeviceContextQT::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                       nsIDeviceContext *&aContext)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetDeviceContextFor\n"));

    static NS_DEFINE_CID(kCDeviceContextPS, NS_DEVICECONTEXTPS_CID);
  
    // Create a Postscript device context 
    nsresult rv;
    nsIDeviceContextPS *dcps;
  
    rv = nsComponentManager::CreateInstance(kCDeviceContextPS,
                                            nsnull,
                                            NS_GET_IID(nsIDeviceContextPS),
                                            (void **)&dcps);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context");
  
    dcps->SetSpec(aDevice);
    dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                              (nsIDeviceContext*)this);

    rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);

    NS_RELEASE(dcps);
  
    return rv;
}

NS_IMETHODIMP nsDeviceContextQT::BeginDocument(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::BeginDocument\n"));
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::EndDocument(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::EndDocument\n"));
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::BeginPage(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::BeginPage\n"));
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::EndPage(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::EndPage\n"));
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::GetDepth(PRUint32& aDepth)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetDepth\n"));
    aDepth = mDepth;
    return NS_OK;
}

nsresult nsDeviceContextQT::SetDPI(PRInt32 aDpi)
{
  mDpi = aDpi;

  int pt2t = 72;

  // make p2t a nice round number - this prevents rounding problems
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(aDpi)));
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  // XXX need to reflow all documents
  return NS_OK;
}

/* For some reason, Qt seems to want to use its own strcmp */
#define qstrcmp strcmp
 
int nsDeviceContextQT::prefChanged(const char *aPref, void *aClosure)
{
  nsDeviceContextQT *context = (nsDeviceContextQT*)aClosure;
  nsresult rv;
 
  if (nsCRT::strcmp(aPref, "browser.display.screen_resolution") == 0) {
    PRInt32 dpi;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    rv = prefs->GetIntPref(aPref, &dpi);
    if (NS_SUCCEEDED(rv))
      context->SetDPI(dpi);
  }
  return 0;
}

/* Just in case, put things back to normal */
#undef qstrcmp
 
