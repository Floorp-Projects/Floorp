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
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   John C. Griggs <johng@corel.com>
 *   Esben Mose Hansen <esben@despammed.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "nsDeviceContextQt.h"
#include "nsFontMetricsQt.h"
#include "nsFont.h"
#include "nsGfxCIID.h"
#include "nsRenderingContextQt.h"
#include "nsDeviceContextSpecQt.h"

#ifdef USE_POSTSCRIPT
#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"
#endif /* USE_POSTSCRIPT */
#ifdef USE_XPRINT
#include "nsGfxXPrintCID.h"
#include "nsIDeviceContextXPrint.h"
#endif /* USE_XPRINT */

#include <qpaintdevicemetrics.h>
#include <qscrollbar.h>
#include <qpalette.h>
#include <qapplication.h>
#include <qstyle.h>
#include <qfontdatabase.h>
#include <qfontmetrics.h>
#include <qwidgetlist.h>

#include "nsIScreenManager.h"

#include "qtlog.h"

#define QCOLOR_TO_NS_RGB(c) \
    ((nscolor)NS_RGB(c.red(),c.green(),c.blue()))

nscoord nsDeviceContextQt::mDpi = 0;

nsDeviceContextQt::nsDeviceContextQt()
  : DeviceContextImpl()
{
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mDepth = 0 ;
  mWidthFloat = 0.0f;
  mHeightFloat = 0.0f;
  mWidth = -1;
  mHeight = -1;
}

nsDeviceContextQt::~nsDeviceContextQt()
{
  nsCOMPtr<nsIPrefBranchInternal> pbi = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (pbi) {
    pbi->RemoveObserver("browser.display.screen_resolution", this);
  }
}

NS_IMETHODIMP nsDeviceContextQt::Init(nsNativeWidget aNativeWidget)
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

  if (!mDpi) {
    // Set prefVal the value of the preference "browser.display.screen_resolution"
    // or -1 if we can't get it.
    // If it's negative, we pretend it's not set.
    // If it's 0, it means force use of the operating system's logical resolution.
    // If it's positive, we use it as the logical resolution
    PRInt32 prefVal = -1;
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefBranch) {
      nsresult res = prefBranch->GetIntPref("browser.display.screen_resolution",
                                            &prefVal);
      if (NS_FAILED(res)) {
        prefVal = -1;
      }
      nsCOMPtr<nsIPrefBranchInternal> pbi(do_QueryInterface(prefBranch));
      pbi->AddObserver("browser.display.screen_resolution", this, PR_FALSE);
    }

    SetDPI(prefVal);
  } else {
    SetDPI(mDpi);
  }

#ifdef MOZ_LOGGING
  static PRBool once = PR_TRUE;
  if (once) {
    PR_LOG(gQtLogModule, QT_BASIC, ("GFX: dpi=%d t2p=%g p2t=%g depth=%d\n",
           mDpi,mTwipsToPixels,mPixelsToTwips,mDepth));
    once = PR_FALSE;
  }
#endif

  QStyle &style = pWidget->style();
  mScrollbarWidth = mScrollbarHeight = style.pixelMetric(QStyle::PM_ScrollBarExtent);

  DeviceContextImpl::CommonInit();

  if (bCleanUp)
    delete pWidget;
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextQt::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  nsresult rv;
  nsDrawingSurfaceQt *surf;
  QPaintDevice *pDev = nsnull;

  if (mWidget)
    pDev = (QPaintDevice*)mWidget;

  // to call init for this, we need to have a valid nsDrawingSurfaceQt created
  nsCOMPtr<nsRenderingContextQt> pContext( new nsRenderingContextQt() );

  // create the nsDrawingSurfaceQt
  surf = new nsDrawingSurfaceQt();

  if (surf) {
    //Handled by the nsDrawingSurfaceQt
    //FIXME: instead of passing it around
    //       create it in the nsDrawingSurface init method
    QPainter *gc = new QPainter();

    // init the nsDrawingSurfaceQt
    if (pDev)
      rv = surf->Init(pDev,gc);
    else
      rv = surf->Init(gc,10,10,0);

    if (NS_SUCCEEDED(rv))
      // Init the nsRenderingContextQt
      rv = pContext->Init(this,surf);
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  if (NS_SUCCEEDED(rv)) {
    aContext = pContext;
    NS_ADDREF(aContext);
  }
  return rv;
}


NS_IMETHODIMP nsDeviceContextQt::CreateRenderingContextInstance(nsIRenderingContext *&aContext)
{
  return CreateRenderingContext(aContext);
}

NS_IMETHODIMP
nsDeviceContextQt::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  //XXX it is very critical that this not lie!! MMP
  // read the comments in the mac code for this
  // ##############
  aSupportsWidgets = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQt::GetScrollBarDimensions(float &aWidth,
                                                        float &aHeight) const
{
  float scale;
  GetCanonicalPixelScale(scale);
  aWidth = mScrollbarWidth * mPixelsToTwips * scale;
  aHeight = mScrollbarHeight * mPixelsToTwips * scale;

  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextQt::GetSystemFont(nsSystemFontID anID, nsFont *aFont) const
{
  nsresult    status      = NS_OK;

  switch (anID) {
    case eSystemFont_Caption:
    case eSystemFont_Icon:
    case eSystemFont_Menu:
    case eSystemFont_MessageBox:
    case eSystemFont_SmallCaption:
    case eSystemFont_StatusBar:
    case eSystemFont_Window:                   // css3
    case eSystemFont_Document:
    case eSystemFont_Workspace:
    case eSystemFont_Desktop:
    case eSystemFont_Info:
    case eSystemFont_Dialog:
    case eSystemFont_Button:
    case eSystemFont_PullDownMenu:
    case eSystemFont_List:
    case eSystemFont_Field:
    case eSystemFont_Tooltips:
    case eSystemFont_Widget:
      status = GetSystemFontInfo(aFont);
      break;
  }
  return status;
}

NS_IMETHODIMP nsDeviceContextQt::CheckFontExistence(const nsString& aFontName)
{
  QString family = QString::fromUcs2(aFontName.get());
  QStringList families = QFontDatabase().families();
  return families.find(family) != families.end();
}

NS_IMETHODIMP nsDeviceContextQt::GetDeviceSurfaceDimensions(PRInt32 &aWidth,
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

NS_IMETHODIMP nsDeviceContextQt::GetRect(nsRect &aRect)
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

NS_IMETHODIMP nsDeviceContextQt::GetClientRect(nsRect &aRect)
{
  return GetRect(aRect);
}

NS_IMETHODIMP nsDeviceContextQt::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                     nsIDeviceContext *&aContext)
{
  nsresult                 rv;
  PrintMethod              method;
  nsDeviceContextSpecQt   *spec = NS_STATIC_CAST(nsDeviceContextSpecQt *, aDevice);

  rv = spec->GetPrintMethod(method);
  if (NS_FAILED(rv))
    return rv;

#ifdef USE_XPRINT
  if (method == pmXprint) { // XPRINT
    static NS_DEFINE_CID(kCDeviceContextXp, NS_DEVICECONTEXTXP_CID);
    nsCOMPtr<nsIDeviceContextXp> dcxp(do_CreateInstance(kCDeviceContextXp, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create Xp Device context.");
    if (NS_FAILED(rv))
      return rv;

    rv = dcxp->SetSpec(aDevice);
    if (NS_FAILED(rv))
      return rv;

    rv = dcxp->InitDeviceContextXP((nsIDeviceContext*)aContext,
                                   (nsIDeviceContext*)this);
    if (NS_FAILED(rv))
      return rv;

    rv = dcxp->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);
    return rv;
  }
  else
#endif /* USE_XPRINT */
#ifdef USE_POSTSCRIPT
  if (method == pmPostScript) { // PostScript
    // default/PS
    static NS_DEFINE_CID(kCDeviceContextPS, NS_DEVICECONTEXTPS_CID);

    // Create a Postscript device context
    nsCOMPtr<nsIDeviceContextPS> dcps(do_CreateInstance(kCDeviceContextPS, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context.");
    if (NS_FAILED(rv))
      return rv;

    rv = dcps->SetSpec(aDevice);
    if (NS_FAILED(rv))
      return rv;

    rv = dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                                   (nsIDeviceContext*)this);
    if (NS_FAILED(rv))
      return rv;

    rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                              (void **)&aContext);
    return rv;
  }
#endif /* USE_POSTSCRIPT */

  NS_WARNING("no print module created.");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsDeviceContextQt::BeginDocument(PRUnichar * /*aTitle*/,
                                               PRUnichar* /*aPrintToFileName*/,
                                               PRInt32 /*aStartPage*/,
                                               PRInt32 /*aEndPage*/)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQt::EndDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQt::BeginPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQt::EndPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextQt::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth;
  return NS_OK;
}

nsresult nsDeviceContextQt::SetDPI(PRInt32 aDpi)
{
  // Set OSVal to what the operating system thinks the logical resolution is.
  PRInt32 OSVal = 0;

  QWidget *pDev = mWidget;
  if (!pDev) {
    QWidgetList *wlist = QApplication::allWidgets();
    pDev = wlist->first();
    qDebug("number of widgets is %d", wlist->count() );
    delete wlist;
  }

  QPaintDeviceMetrics qPaintMetrics(pDev);
  OSVal = qPaintMetrics.logicalDpiX();


#ifdef DEBUG
  if (!pDev)
    qDebug("nsDeviceContextQt::SetDPI called without widget (find cleaner solution)");
#endif

  if (aDpi > 0) {
    // If there's a valid pref value for the logical resolution,
    // use it.
    mDpi = aDpi;
  }
  else if (aDpi == 0 || OSVal > 96) {
    // Either if the pref is 0 (force use of OS value) or the OS
    // value is bigger than 96, use the OS value.
    mDpi = OSVal;
  }
  else {
    // if we couldn't get the pref or it's negative, and the OS
    // value is under 96ppi, then use 96.
    mDpi = 96;
  }

  int pt2t = 72;

  // make p2t a nice round number - this prevents rounding problems
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(aDpi)));
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  // XXX need to reflow all documents

  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextQt::Observe(nsISupports* aSubject, const char* aTopic,
                           const PRUnichar* aData)
{
  if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) != 0) {
    // Our local observer only handles pref changes.
    // Forward everything else to our super class.
    return DeviceContextImpl::Observe(aSubject, aTopic, aData);
  }

  nsCOMPtr<nsIPrefBranch> prefBranch(do_QueryInterface(aSubject));
  NS_ASSERTION(prefBranch,
               "All pref change observer subjects implement nsIPrefBranch");
  nsCAutoString prefName(NS_LossyConvertUCS2toASCII(aData).get());

  if (prefName.Equals(NS_LITERAL_CSTRING("browser.display.screen_resolution"))) {
    PRInt32 dpi;
    nsresult rv = prefBranch->GetIntPref(prefName.get(), &dpi);
    if (NS_SUCCEEDED(rv))
      SetDPI(dpi);
    return NS_OK;
  } else
    return DeviceContextImpl::Observe(aSubject, aTopic, aData);
}

nsresult
nsDeviceContextQt::GetSystemFontInfo(nsFont* aFont) const
{
  nsresult status = NS_OK;
  int rawWeight;
  QFont theFont = QApplication::font();
  QFontInfo theFontInfo(theFont);

  aFont->style       = NS_FONT_STYLE_NORMAL;
  aFont->weight      = NS_FONT_WEIGHT_NORMAL;
  aFont->decorations = NS_FONT_DECORATION_NONE;
  aFont->name.Assign(theFontInfo.family().ucs2());
  if (theFontInfo.bold()) {
    aFont->weight = NS_FONT_WEIGHT_BOLD;
  }
  rawWeight = theFontInfo.pixelSize();
  aFont->size = NSIntPixelsToTwips(rawWeight,mPixelsToTwips);
  if (theFontInfo.italic()) {
    aFont->style = NS_FONT_STYLE_ITALIC;
  }
  if (theFontInfo.underline()) {
    aFont->decorations = NS_FONT_DECORATION_UNDERLINE;
  }

  return (status);
}
