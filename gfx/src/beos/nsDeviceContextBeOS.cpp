/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <math.h>
#include <Menu.h>
#include "nspr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"

#include "nsDeviceContextBeOS.h"
#include "nsFontMetricsBeOS.h"
#include "nsGfxCIID.h"

#ifdef USE_POSTSCRIPT
#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"
#endif /* USE_POSTSCRIPT */

#include <ScrollBar.h>
#include <Screen.h>

#include "nsIScreenManager.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID); 
 
nscoord nsDeviceContextBeOS::mDpi = 96; 
 
nsDeviceContextBeOS::nsDeviceContextBeOS()
  : DeviceContextImpl()
{
  mTwipsToPixels = 1.0;
  mPixelsToTwips = 1.0;
  mDepth = 0 ;
  mNumCells = 0;
  
  mWidthFloat = 0.0f; 
  mHeightFloat = 0.0f; 
  mWidth = -1; 
  mHeight = -1; 
}

nsDeviceContextBeOS::~nsDeviceContextBeOS()
{
  nsresult rv; 
  nsCOMPtr<nsIPref> prefs = do_GetService(kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv)) { 
    prefs->UnregisterCallback("browser.display.screen_resolution", 
                              prefChanged, (void *)this); 
  } 
}

NS_IMETHODIMP nsDeviceContextBeOS::Init(nsNativeWidget aNativeWidget)
{
  // get the screen object and its width/height 
  // XXXRight now this will only get the primary monitor. 

  nsresult ignore; 
  nsCOMPtr<nsIScreenManager> sm ( do_GetService("@mozilla.org/gfx/screenmanager;1", &ignore) ); 
  if ( sm ) { 
    nsCOMPtr<nsIScreen> screen; 
    sm->GetPrimaryScreen ( getter_AddRefs(screen) ); 
    if ( screen ) { 
      PRInt32 x, y, width, height, depth; 
      screen->GetAvailRect ( &x, &y, &width, &height ); 
      screen->GetPixelDepth ( &depth ); 
      mWidthFloat = float(width); 
      mHeightFloat = float(height); 
      mDepth = NS_STATIC_CAST ( PRUint32, depth ); 
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
      res = prefs->GetIntPref("browser.display.screen_resolution", &prefVal); 
      if (! NS_SUCCEEDED(res)) { 
        prefVal = -1; 
      } 
      prefs->RegisterCallback("browser.display.screen_resolution", prefChanged, 
                              (void *)this); 
    } 
 
    // Set OSVal to what the operating system thinks the logical resolution is. 
    PRInt32 OSVal = 72; 
    
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
 
  SetDPI(mDpi); 

  mScrollbarHeight = PRInt16(B_H_SCROLL_BAR_HEIGHT); 
  mScrollbarWidth = PRInt16(B_V_SCROLL_BAR_WIDTH); 

  menu_info info;
  get_menu_info(&info);
  mMenuFont.SetFamilyAndStyle(info.f_family,info.f_style);
  
#ifdef DEBUG 
  static PRBool once = PR_TRUE; 
  if (once) { 
    printf("GFX: dpi=%d t2p=%g p2t=%g depth=%d\n", mDpi, mTwipsToPixels, mPixelsToTwips,mDepth); 
    once = PR_FALSE; 
  } 
#endif 

  DeviceContextImpl::CommonInit();
  
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::CreateRenderingContext(nsIRenderingContext *&aContext) 
{ 
  nsIRenderingContext *pContext; 
  nsresult             rv; 
  nsDrawingSurfaceBeOS  *surf; 
  BView *w; 

  w = (BView*)mWidget;

  // to call init for this, we need to have a valid nsDrawingSurfaceBeOS created 
  pContext = new nsRenderingContextBeOS(); 
 
  if (nsnull != pContext) 
  { 
    NS_ADDREF(pContext); 
 
    // create the nsDrawingSurfaceBeOS 
    surf = new nsDrawingSurfaceBeOS(); 
 
    if (surf && w) 
      { 
 
        // init the nsDrawingSurfaceBeOS 
        rv = surf->Init(w); 
 
        if (NS_OK == rv) 
          // Init the nsRenderingContextBeOS 
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
 
NS_IMETHODIMP nsDeviceContextBeOS::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  //XXX it is very critical that this not lie!! MMP
  // read the comments in the mac code for this
  aSupportsWidgets = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  float scale;
  GetCanonicalPixelScale(scale);
  aWidth = mScrollbarWidth * mPixelsToTwips * scale;
  aHeight = mScrollbarHeight * mPixelsToTwips * scale;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
  nsresult status = NS_OK;

  switch (aID) 
  {
    case eSystemFont_PullDownMenu: 
    case eSystemFont_Menu:
      status = GetSystemFontInfo(&mMenuFont, aID, aFont); 
      break;
    case eSystemFont_Caption:             // css2 bold  
      status = GetSystemFontInfo(be_bold_font, aID, aFont); 
      break;
    case eSystemFont_List:   
    case eSystemFont_Field:
    case eSystemFont_Icon : 
    case eSystemFont_MessageBox : 
    case eSystemFont_SmallCaption : 
    case eSystemFont_StatusBar : 
    case eSystemFont_Window:                      // css3 
    case eSystemFont_Document: 
    case eSystemFont_Workspace: 
    case eSystemFont_Desktop: 
    case eSystemFont_Info: 
    case eSystemFont_Dialog: 
    case eSystemFont_Button: 
    case eSystemFont_Tooltips:            // moz 
    case eSystemFont_Widget: 
    default:
      status = GetSystemFontInfo(be_plain_font, aID, aFont);
  }

  return status;
}

NS_IMETHODIMP nsDeviceContextBeOS::CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsBeOS::FamilyExists(aFontName); 
} 

/* 
NS_IMETHODIMP nsDeviceContextBeOS::CheckFontExistence(const nsString& aFontName) 
{
  PRBool  isthere = PR_FALSE;

  char* cStr = ToNewCString(aFontName);

	int32 numFamilies = count_font_families();
	for(int32 i = 0; i < numFamilies; i++)
	{
		font_family family; 
		uint32 flags; 
		if(get_font_family(i, &family, &flags) == B_OK)
		{
			if(strcmp(family, cStr) == 0)
			{
				isthere = PR_TRUE;
				break;
			}
		} 
	}

	//printf("%s there? %s\n", cStr, isthere?"Yes":"No" );
	
  delete[] cStr;

  if (PR_TRUE == isthere)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}
*/

NS_IMETHODIMP nsDeviceContextBeOS::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  if (mWidth == -1) 
    mWidth = NSToIntRound(mWidthFloat * mDevUnitsToAppUnits);

  if (mHeight == -1) 
    mHeight = NSToIntRound(mHeightFloat * mDevUnitsToAppUnits); 
 
  aWidth = mWidth; 
  aHeight = mHeight; 
 
  return NS_OK; 
}

NS_IMETHODIMP nsDeviceContextBeOS::GetRect(nsRect &aRect)
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
 
NS_IMETHODIMP nsDeviceContextBeOS::GetClientRect(nsRect &aRect) 
{ 
//XXX do we know if the client rect should ever differ from the screen rect? 
  return GetRect ( aRect );
}

NS_IMETHODIMP nsDeviceContextBeOS::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                      nsIDeviceContext *&aContext)
{
#ifdef USE_POSTSCRIPT
  static NS_DEFINE_CID(kCDeviceContextPS, NS_DEVICECONTEXTPS_CID);
  
  // Create a Postscript device context 
  nsresult rv;
  nsIDeviceContextPS *dcps;
  
  rv = CallCreateInstance(kCDeviceContextPS, &dcps);

  NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create PS Device context");
  
  dcps->SetSpec(aDevice);
  dcps->InitDeviceContextPS((nsIDeviceContext*)aContext,
                            (nsIDeviceContext*)this);

  rv = dcps->QueryInterface(NS_GET_IID(nsIDeviceContext),
                            (void **)&aContext);

  NS_RELEASE(dcps);
  
  return rv;
  #else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif /* USE_POSTSCRIPT */
}

NS_IMETHODIMP nsDeviceContextBeOS::BeginDocument(PRUnichar * aTitle, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::EndDocument(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::BeginPage(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::EndPage(void)
{
  return NS_OK;
} 
 
NS_IMETHODIMP nsDeviceContextBeOS::GetDepth(PRUint32& aDepth) 
{ 
  aDepth = mDepth; 
  return NS_OK; 
} 
 
nsresult 
nsDeviceContextBeOS::SetDPI(PRInt32 aDpi) 
{ 
  mDpi = aDpi; 
  
  int pt2t = 72; 
 
  // make p2t a nice round number - this prevents rounding problems 
  mPixelsToTwips = float(NSToIntRound(float(NSIntPointsToTwips(pt2t)) / float(aDpi))); 
  mTwipsToPixels = 1.0f / mPixelsToTwips; 
 
  // XXX need to reflow all documents 
  return NS_OK; 
} 
 
int nsDeviceContextBeOS::prefChanged(const char *aPref, void *aClosure) 
{ 
  nsDeviceContextBeOS *context = (nsDeviceContextBeOS*)aClosure; 
  nsresult rv; 
  
  if (nsCRT::strcmp(aPref, "browser.display.screen_resolution")==0) { 
    PRInt32 dpi; 
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
    rv = prefs->GetIntPref(aPref, &dpi); 
    if (NS_SUCCEEDED(rv)) 
      context->SetDPI(dpi); 
  } 
  
  return 0; 
} 
 
nsresult 
nsDeviceContextBeOS::GetSystemFontInfo(const BFont *theFont, nsSystemFontID anID, nsFont* aFont) const 
{ 
  nsresult status = NS_OK; 
 
  aFont->style       = NS_FONT_STYLE_NORMAL; 
  aFont->weight      = NS_FONT_WEIGHT_NORMAL; 
  aFont->decorations = NS_FONT_DECORATION_NONE; 
  
  // do we have the default_font defined by BeOS, if not then 
  // we error out. 
  if( !theFont )
  	switch (anID) 
  	{
      case eSystemFont_Menu:
        status = GetSystemFontInfo(&mMenuFont, anID, aFont); 
        break;
      case eSystemFont_List:
      case eSystemFont_Field:
        theFont = be_plain_font;
        break;
      case eSystemFont_Caption:
        theFont = be_bold_font;
        break;
      default:
        theFont = be_plain_font; // BeOS default font 
    }
   
  if( !theFont ) 
  { 
    status = NS_ERROR_FAILURE; 
  } 
  else 
  { 
    font_family family; 
    font_style style; 
    font_height height; 
 
    theFont->GetFamilyAndStyle(&family, &style);   
    aFont->name.AssignWithConversion( family ); 
 
       // No weight 
           
    theFont->GetHeight(&height); 
    aFont->size = NSIntPixelsToTwips(uint32(height.ascent+height.descent+height.leading), mPixelsToTwips); 
 
       // no style 
    
       // no decoration 

    aFont->systemFont = PR_TRUE;

    status = NS_OK; 
  } 
  return (status); 
}
