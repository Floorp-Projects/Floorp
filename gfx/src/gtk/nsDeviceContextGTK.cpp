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

#include <math.h>

#include "nspr.h"
#include "il_util.h"

#include "nsDeviceContextGTK.h"
#include "nsGfxCIID.h"

#include "../ps/nsDeviceContextPS.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor) NS_RGB(c.red, c.green, c.blue))

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextGTK::nsDeviceContextGTK()
{
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

nsDeviceContextGTK::~nsDeviceContextGTK()
{
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextGTK, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextGTK)
NS_IMPL_RELEASE(nsDeviceContextGTK)

NS_IMETHODIMP nsDeviceContextGTK::Init(nsNativeWidget aNativeWidget)
{
  GdkVisual *vis;
  GtkRequisition req;
  GtkWidget *sb;

  mWidget = aNativeWidget;

  // Compute dpi of display
  float screenWidth = float(::gdk_screen_width());
  float screenWidthIn = float(::gdk_screen_width_mm()) / 25.4f;
  nscoord dpi = nscoord(screenWidth / screenWidthIn);

  // Now for some wacky heuristics. 
  if (dpi < 84) dpi = 72;
  else if (dpi < 108) dpi = 96;
  else if (dpi < 132) dpi = 120;

  mTwipsToPixels = float(dpi) / float(NSIntPointsToTwips(72));
  mPixelsToTwips = 1.0f / mTwipsToPixels;

#ifdef DEBUG
  static PRBool once = PR_TRUE;
  if (once) {
    printf("GFX: dpi=%d t2p=%g p2t=%g\n", dpi, mTwipsToPixels, mPixelsToTwips);
    once = PR_FALSE;
  }
#endif

#if 0
  mTwipsToPixels = ( ((float)::gdk_screen_width()) /
                     ((float)::gdk_screen_width_mm()) * 25.4) /
		     (float)NSIntPointsToTwips(72);

  mPixelsToTwips = 1.0f / mTwipsToPixels;
#endif

  vis = gdk_rgb_get_visual();
  mDepth = vis->depth;

  sb = gtk_vscrollbar_new(NULL);
  gtk_widget_ref(sb);
  gtk_object_sink(GTK_OBJECT(sb));
  gtk_widget_size_request(sb,&req);
  mScrollbarWidth = req.width;
  gtk_widget_destroy(sb);
  gtk_widget_unref(sb);
  
  sb = gtk_hscrollbar_new(NULL);
  gtk_widget_ref(sb);
  gtk_object_sink(GTK_OBJECT(sb));
  gtk_widget_size_request(sb,&req);
  mScrollbarHeight = req.height;
  gtk_widget_destroy(sb);
  gtk_widget_unref(sb);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextGTK::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  //XXX it is very critical that this not lie!! MMP
  // read the comments in the mac code for this
  aSupportsWidgets = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  aWidth = mScrollbarWidth * mPixelsToTwips;
  aHeight = mScrollbarHeight * mPixelsToTwips;
 
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;
  GtkStyle *style = gtk_style_new();  // get the default styles

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_SELECTED]);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_SELECTED]);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_SELECTED]);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = GDK_COLOR_TO_NS_RGB(style->text[GTK_STATE_SELECTED]);
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight:
        aInfo->mSize = mScrollbarHeight;
        break;
    case eSystemAttr_Size_ScrollbarWidth : 
        aInfo->mSize = mScrollbarWidth;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = style->klass->xthickness;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = style->klass->ythickness;
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
  gtk_style_unref(style);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::GetDrawingSurface(nsIRenderingContext &aContext, 
                                                    nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;  
}

NS_IMETHODIMP nsDeviceContextGTK::ConvertPixel(nscolor aColor, 
                                               PRUint32 & aPixel)
{
  aPixel = ::gdk_rgb_xpixel_from_rgb ((aColor & 0xff) << 16 |
                                      (aColor & 0xff00) |
                                      ((aColor >> 16) & 0xff));

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextGTK::CheckFontExistence(const nsString& aFontName)
{
  char        **fnames = nsnull;
  PRInt32     namelen = aFontName.Length() + 1;
  char        *wildstring = (char *)PR_Malloc(namelen + 200);
  float       t2d;
  GetTwipsToDevUnits(t2d);
  PRInt32     dpi = NSToIntRound(t2d * 1440);
  int         numnames = 0;
  XFontStruct *fonts;
  nsresult    rv = NS_ERROR_FAILURE;
  
  if (nsnull == wildstring)
    return NS_ERROR_UNEXPECTED;
  
  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;
  
  char* fontName = aFontName.ToNewCString();
  PR_snprintf(wildstring, namelen + 200,
             "*-%s-*-*-normal--*-*-%d-%d-*-*-*",
             fontName, dpi, dpi);
  delete [] fontName;
  
  fnames = ::XListFontsWithInfo(GDK_DISPLAY(), wildstring, 1, &numnames, &fonts);
  
  if (numnames > 0)
  {
    ::XFreeFontInfo(fnames, fonts, numnames);
    rv = NS_OK;
  }
  
  PR_Free(wildstring);
  
  return rv;
}

NS_IMETHODIMP nsDeviceContextGTK::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  aWidth = 1;
  aHeight = 1;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextGTK::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                      nsIDeviceContext *&aContext)
{
  // Create a Postscript device context 
  aContext = new nsDeviceContextPS();
  ((nsDeviceContextPS *)aContext)->SetSpec(aDevice);
  NS_ADDREF(aDevice);
  return((nsDeviceContextPS *) aContext)->Init((nsIDeviceContext*)aContext, (nsIDeviceContext*)this);
}

NS_IMETHODIMP nsDeviceContextGTK::BeginDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::EndDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::BeginPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextGTK::EndPage(void)
{
  return NS_OK;
}
