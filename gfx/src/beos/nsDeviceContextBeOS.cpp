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

#include "nsDeviceContextBeOS.h"
#include "nsGfxCIID.h"

#include "nsGfxPSCID.h"
#include "nsIDeviceContextPS.h"

#include <ScrollBar.h>
#include <Screen.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

nsDeviceContextBeOS::nsDeviceContextBeOS()
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

nsDeviceContextBeOS::~nsDeviceContextBeOS()
{
}

NS_IMPL_QUERY_INTERFACE(nsDeviceContextBeOS, kDeviceContextIID)
NS_IMPL_ADDREF(nsDeviceContextBeOS)
NS_IMPL_RELEASE(nsDeviceContextBeOS)

NS_IMETHODIMP nsDeviceContextBeOS::Init(nsNativeWidget aNativeWidget)
{
  mWidget = aNativeWidget;

// this is used for something odd.  who knows
  mTwipsToPixels = (72) /		// FIXME: hardcoded value
		     (float)NSIntPointsToTwips(72);

  mPixelsToTwips = 1.0f / mTwipsToPixels;

  BScreen scr;

  mDepth = 32;	// FIXME: hardcoded value

  DeviceContextImpl::Init(aNativeWidget);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::CreateRenderingContext(nsIRenderingContext *&aContext)
{
printf("nsDeviceContextBeOS::CreateRenderingContext - FIXME: not implemented\n");
  return NS_ERROR_FAILURE;
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
  aWidth = (B_V_SCROLL_BAR_WIDTH + 1) * mPixelsToTwips;
  aHeight = (B_H_SCROLL_BAR_HEIGHT + 1) * mPixelsToTwips;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = NS_RGB(0x80,0x80,0x80);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = NS_RGB(0xff,0xff,0xff);
        break;
    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight:
        aInfo->mSize = B_H_SCROLL_BAR_HEIGHT;
        break;
    case eSystemAttr_Size_ScrollbarWidth : 
        aInfo->mSize = B_V_SCROLL_BAR_WIDTH;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = 5;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = 5;
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

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextBeOS::GetDrawingSurface(nsIRenderingContext &aContext, 
                                                    nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;  
}

NS_IMETHODIMP nsDeviceContextBeOS::ConvertPixel(nscolor aColor, 
                                               PRUint32 & aPixel)
{
  aPixel = aColor;	// FIXME: what is this supposed to do?

  return NS_OK;
}


NS_IMETHODIMP nsDeviceContextBeOS::CheckFontExistence(const nsString& aFontName)
{
  PRBool  isthere = PR_FALSE;

  char* cStr = aFontName.ToNewCString();

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

NS_IMETHODIMP nsDeviceContextBeOS::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  aWidth = 1;
  aHeight = 1;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextBeOS::GetClientRect(nsRect &aRect)
{
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = 0;
  aRect.height = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextBeOS::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                                      nsIDeviceContext *&aContext)
{
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

NS_IMETHODIMP nsDeviceContextBeOS::BeginDocument(void)
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
