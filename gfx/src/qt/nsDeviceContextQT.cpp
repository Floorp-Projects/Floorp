/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   John C. Griggs <johng@corel.com>
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
#include "nsDeviceContextQT.h"
#include "nsFontMetricsQT.h"
#include "nsFont.h"
#include "nsGfxCIID.h"
#include "nsRenderingContextQT.h" 

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"


#include <qpaintdevicemetrics.h>
#include <qscrollbar.h>
#include <qpalette.h>
#include <qapplication.h>
#include <qstyle.h>

#include "nsIScreenManager.h"

#define QCOLOR_TO_NS_RGB(c) \
    ((nscolor)NS_RGB(c.red(),c.green(),c.blue()))

static NS_DEFINE_CID(kPrefCID,NS_PREF_CID);

nscoord nsDeviceContextQT::mDpi = 96;

NS_IMPL_ISUPPORTS1(nsDeviceContextQT,nsIDeviceContext)

nsDeviceContextQT::nsDeviceContextQT()
{
  NS_INIT_REFCNT();
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mDepth = 0 ;
  mNumCells = 0;
  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
}

nsDeviceContextQT::~nsDeviceContextQT()
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(kPrefCID,&rv);
  if (NS_SUCCEEDED(rv)) {
    prefs->UnregisterCallback("browser.display.screen_resolution",
                              prefChanged,(void*)this);
  }
}

NS_IMETHODIMP nsDeviceContextQT::Init(nsNativeWidget aNativeWidget)
{
  PRBool  bCleanUp = PR_FALSE;
  QWidget *pWidget  = nsnull;

  mWidget = (QWidget*)aNativeWidget;

  if (mWidget != nsnull)
    pWidget = mWidget;
  else {
    pWidget = new QWidget();
    bCleanUp = PR_TRUE;
  }
  QPaintDeviceMetrics qPaintMetrics(pWidget);

  nsresult ignore;
  nsCOMPtr<nsIScreenManager> sm(do_GetService("@mozilla.org/gfx/screenmanager;1",
                                              &ignore));
  if (sm) {
    nsCOMPtr<nsIScreen> screen;
    sm->GetPrimaryScreen(getter_AddRefs(screen));
    if (screen) {
      PRInt32 x,y,width,height,depth;

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

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &res));
    if (NS_SUCCEEDED(res) && prefs) {
      res = prefs->GetIntPref("browser.display.screen_resolution",&prefVal);
      if (! NS_SUCCEEDED(res)) {
        prefVal = -1;
      }
      prefs->RegisterCallback("browser.display.screen_resolution",prefChanged,
                              (void*)this);
    }
    // Set OSVal to what the operating system thinks the logical resolution is.
    PRInt32 OSVal = qPaintMetrics.logicalDpiX();

    if (prefVal > 0) {
      // If there's a valid pref value for the logical resolution,
      // use it.
      mDpi = prefVal;
    }
    else if (prefVal == 0 || OSVal > 96) {
      // Either if the pref is 0 (force use of OS value) or the OS
      // value is bigger than 96, use the OS value.
      mDpi = OSVal;
    }
    else {
      // if we couldn't get the pref or it's negative, and the OS
      // value is under 96ppi, then use 96.
      mDpi = 96;
    }
  }
  SetDPI(mDpi);

#ifdef DEBUG
  static PRBool once = PR_TRUE;
  if (once) {
    printf("GFX: dpi=%d t2p=%g p2t=%g depth=%d\n",
           mDpi,mTwipsToPixels,mPixelsToTwips,mDepth);
    once = PR_FALSE;
  }
#endif

  QScrollBar * sb = new QScrollBar(nsnull,nsnull);
  if (sb) {
    sb->setOrientation(QScrollBar::Vertical);
    QSize size = sb->sizeHint();

    mScrollbarWidth = size.width();

    sb->setOrientation(QScrollBar::Horizontal);
    size = sb->sizeHint();

    mScrollbarHeight = size.height();
  }
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
  nsIRenderingContext *pContext;
  nsresult rv;
  nsDrawingSurfaceQT *surf;
  QPaintDevice *pDev = nsnull;

  if (mWidget)
    pDev = (QPaintDevice*)mWidget;
 
  // to call init for this, we need to have a valid nsDrawingSurfaceQT created
  pContext = new nsRenderingContextQT();
 
  if (nsnull != pContext) {
    NS_ADDREF(pContext);
 
    // create the nsDrawingSurfaceQT
    surf = new nsDrawingSurfaceQT();
 
    if (surf) {
      QPainter *gc = new QPainter();
 
      // init the nsDrawingSurfaceQT
      if (pDev)
        rv = surf->Init(pDev,gc);
      else
        rv = surf->Init(gc,10,10,0);
 
      if (NS_OK == rv)
        // Init the nsRenderingContextQT
        rv = pContext->Init(this,surf);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;
 
  if (NS_OK != rv) {
    NS_IF_RELEASE(pContext);
  }
  aContext = pContext;
  return rv;
}

NS_IMETHODIMP 
nsDeviceContextQT::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
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
  return NS_OK;
}

NS_IMETHODIMP 
nsDeviceContextQT::GetSystemAttribute(nsSystemAttrID anID, 
                                      SystemAttrStruct *aInfo) const
{
  nsresult    status      = NS_OK;
  QPalette    palette     = qApp->palette();
  QColorGroup normalGroup = palette.inactive();
  QColorGroup activeGroup = palette.active();

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eSystemAttr_Color_WindowForeground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(normalGroup.foreground());
      break;

    case eSystemAttr_Color_WidgetBackground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eSystemAttr_Color_WidgetForeground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(normalGroup.foreground());
      break;

    case eSystemAttr_Color_WidgetSelectBackground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(activeGroup.background());
      break;

    case eSystemAttr_Color_WidgetSelectForeground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(activeGroup.foreground());
      break;

    case eSystemAttr_Color_Widget3DHighlight:
      *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
      break;

    case eSystemAttr_Color_Widget3DShadow:
      *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
      break;

    case eSystemAttr_Color_TextBackground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eSystemAttr_Color_TextForeground: 
      *aInfo->mColor = QCOLOR_TO_NS_RGB(normalGroup.text());
      break;

    case eSystemAttr_Color_TextSelectBackground:
      *aInfo->mColor = QCOLOR_TO_NS_RGB(activeGroup.highlight());
      break;

    case eSystemAttr_Color_TextSelectForeground:
      *aInfo->mColor
       = QCOLOR_TO_NS_RGB(activeGroup.highlightedText());
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
      status = GetSystemFontInfo(aInfo->mFont);
      break;
  }
  return status;
}

NS_IMETHODIMP 
nsDeviceContextQT::GetDrawingSurface(nsIRenderingContext &aContext, 
                                     nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull,0,aSurface);
  return (nsnull == aSurface) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;  
}

NS_IMETHODIMP nsDeviceContextQT::ConvertPixel(nscolor aColor, 
                                              PRUint32 &aPixel)
{
  QColor color(NS_GET_R(aColor),NS_GET_G(aColor),NS_GET_B(aColor));

  aPixel = color.pixel();
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsQT::FamilyExists(aFontName);
}

NS_IMETHODIMP nsDeviceContextQT::GetDeviceSurfaceDimensions(PRInt32 &aWidth, 
                                                            PRInt32 &aHeight)
{
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
  PRInt32 width,height;
  nsresult rv;
 
  rv = GetDeviceSurfaceDimensions(width,height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}

NS_IMETHODIMP nsDeviceContextQT::GetClientRect(nsRect &aRect)
{
  return GetRect(aRect);
}

NS_IMETHODIMP 
nsDeviceContextQT::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                       nsIDeviceContext *&aContext)
{

  static NS_DEFINE_CID(kCDeviceContextPS,NS_DEVICECONTEXTPS_CID);
  
  // Create a Postscript device context 
  nsresult rv;
  nsIDeviceContextPS *dcps;
  
  rv = nsComponentManager::CreateInstance(kCDeviceContextPS,nsnull,
                                          NS_GET_IID(nsIDeviceContextPS),
                                          (void**)&dcps);
  NS_ASSERTION(NS_SUCCEEDED(rv),"Couldn't create PS Device context");
  
  dcps->SetSpec(aDevice);
  dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                            (nsIDeviceContext*)this);

  rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                            (void**)&aContext);
  NS_RELEASE(dcps);
  return rv;
}

NS_IMETHODIMP nsDeviceContextQT::BeginDocument(PRUnichar *aTitle)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::EndDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::BeginPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::EndPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQT::GetDepth(PRUint32& aDepth)
{
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

int nsDeviceContextQT::prefChanged(const char *aPref,void *aClosure)
{
  nsDeviceContextQT *context = (nsDeviceContextQT*)aClosure;
  nsresult rv;
 
  if (nsCRT::strcmp(aPref,"browser.display.screen_resolution") == 0) {
    PRInt32 dpi;

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    rv = prefs->GetIntPref(aPref, &dpi);
    if (NS_SUCCEEDED(rv))
      context->SetDPI(dpi);
  }
  return 0;
}

nsresult
nsDeviceContextQT::GetSystemFontInfo(nsFont* aFont) const
{
  nsresult status = NS_OK;
  int rawWeight;
  QFont theFont = QApplication::font();
  QFontInfo theFontInfo(theFont);
 
  aFont->style       = NS_FONT_STYLE_NORMAL;
  aFont->weight      = NS_FONT_WEIGHT_NORMAL;
  aFont->decorations = NS_FONT_DECORATION_NONE;
  aFont->name.AssignWithConversion(theFontInfo.family().latin1());
  if (theFontInfo.bold()) {
    aFont->weight = NS_FONT_WEIGHT_BOLD;
  }
  rawWeight = theFontInfo.pointSize();
  aFont->size = NSIntPixelsToTwips(rawWeight,mPixelsToTwips);
  if (theFontInfo.italic()) {
    aFont->style = NS_FONT_STYLE_ITALIC;
  }
  if (theFontInfo.underline()) {
    aFont->decorations = NS_FONT_DECORATION_UNDERLINE;
  }
  return (status);
}
