/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <math.h>

#include "nspr.h"
#include "il_util.h"

#include "nsDeviceContextQT.h"
#include "nsGfxCIID.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"

#include <qpaintdevicemetrics.h>
#include <qscrollbar.h>
#include <qpalette.h>
#include <qapplication.h>
#include <qstyle.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

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
}

nsDeviceContextQT::~nsDeviceContextQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::~nsDeviceContextQT\n"));
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextQT, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextQT)
NS_IMPL_RELEASE(nsDeviceContextQT)

NS_IMETHODIMP nsDeviceContextQT::Init(nsNativeWidget aNativeWidget)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::Init\n"));
    PRBool    bCleanUp = PR_FALSE;
    QWidget * pWidget  = nsnull;

    mWidget = (QWidget *) aNativeWidget;

    if (mWidget != nsnull)
    {
        pWidget = mWidget;
    }
    else
    {
        pWidget = new QWidget();
        bCleanUp = PR_TRUE;
    }

    QPaintDeviceMetrics pgm(pWidget);

    mDepth = pgm.depth();

    // Compute dpi of display
    float width = float(pgm.width());
    float widthIn = float(pgm.widthMM()) / 25.4f;
    nscoord dpi = nscoord(width / widthIn);

    // Now for some wacky heuristics. 
    if (dpi < 84) dpi = 72;
    else if (dpi < 108) dpi = 96;
    else if (dpi < 132) dpi = 120;

    mTwipsToPixels = float(dpi) / float(NSIntPointsToTwips(72));
    mPixelsToTwips = 1.0f / mTwipsToPixels;

    static PRBool once = PR_TRUE;
    if (once) 
    {
        PR_LOG(QtGfxLM, 
               PR_LOG_DEBUG, 
               ("nsDeviceContextQT::Init dpi=%d, t2p=%d, p2t=%d\n",
                dpi, 
                mTwipsToPixels, 
                mPixelsToTwips));
        once = PR_FALSE;
    }

    QScrollBar * sb = new QScrollBar(nsnull, nsnull);

    if (sb)
    {
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

    delete sb;

    if (bCleanUp)
    {
        delete pWidget;
    }

    return NS_OK;
}

NS_IMETHODIMP 
nsDeviceContextQT::CreateRenderingContext(nsIRenderingContext *&aContext)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::CreateRenderingContext\n"));
    return NS_ERROR_FAILURE;
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
    QColorGroup normalGroup = palette.normal();
    QColorGroup activeGroup = palette.active();

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
        *aInfo->mColor = normalGroup.highlight().rgb();//NS_RGB(0xa0,0xa0,0xa0);
        //*aInfo->mColor = normalGroup.light().rgb();//NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = normalGroup.shadow().rgb();//NS_RGB(0x40,0x40,0x40);
        //*aInfo->mColor = normalGroup.dark().rgb();//NS_RGB(0x40,0x40,0x40);
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
    case eSystemAttr_Font_Tooltips: 
    case eSystemAttr_Font_Widget:
        status = NS_ERROR_FAILURE;
        break;
    }

    return NS_OK;
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
    PRInt32     dpi = NSToIntRound(t2d * 1440);
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
    aWidth = 1;
    aHeight = 1;

    return NS_ERROR_FAILURE;
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
                                            nsIDeviceContextPS::GetIID(),
                                            (void **)&dcps);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context");
  
    dcps->SetSpec(aDevice);
    dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                              (nsIDeviceContext*)this);

    rv = dcps->QueryInterface(nsIDeviceContext::GetIID(),
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
    aDepth = 32;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextQT::GetILColorSpace\n"));
    if (nsnull == mColorSpace) 
    {
        IL_RGBBits colorRGBBits;
  
        // Default is to create a 32-bit color space
        colorRGBBits.red_shift = 24;  
        colorRGBBits.red_bits = 8;
        colorRGBBits.green_shift = 16;
        colorRGBBits.green_bits = 8; 
        colorRGBBits.blue_shift = 8; 
        colorRGBBits.blue_bits = 8;  
  
        mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 32);
        if (nsnull == mColorSpace) 
        {
            aColorSpace = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    NS_POSTCONDITION(nsnull != mColorSpace, "null color space");
    aColorSpace = mColorSpace;
    IL_AddRefToColorSpace(aColorSpace);
    return NS_OK;
}
