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

#ifndef nsDeviceContextPh_h___
#define nsDeviceContextPh_h___

#include "nsDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsIRenderingContext.h"
#include "nsIScreenManager.h"

#include "nsRenderingContextPh.h"
#include <Pt.h>

class nsDeviceContextPh : public DeviceContextImpl
{
public:
  nsDeviceContextPh();
  virtual ~nsDeviceContextPh();
  
  NS_IMETHOD  Init(nsNativeWidget aWidget);


  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aView,aContext));}
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext) {return (DeviceContextImpl::CreateRenderingContext(aWidget,aContext));}

	inline
  NS_IMETHODIMP SupportsNativeWidgets(PRBool &aSupportsWidgets)
		{
		/* REVISIT: These needs to return FALSE if we are printing! */
	  if( nsnull == mDC ) aSupportsWidgets = PR_TRUE;
	  else aSupportsWidgets = PR_FALSE;   /* while printing! */
	  return NS_OK;
		}

	inline
  NS_IMETHODIMP GetScrollBarDimensions(float &aWidth, float &aHeight) const
		{
		/* Revisit: the scroll bar sizes is a gross guess based on Phab */
		float scale;
		GetCanonicalPixelScale(scale);
		aWidth = mScrollbarWidth * mPixelsToTwips * scale;
		aHeight = mScrollbarHeight * mPixelsToTwips * scale;
		return NS_OK;
		}

  NS_IMETHOD  GetSystemFont(nsSystemFontID anID, nsFont *aFont) const;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.

	inline
  NS_IMETHODIMP GetDrawingSurface(nsIRenderingContext &aContext, nsIDrawingSurface* &aSurface)
		{
		nsRect aRect;
		GetClientRect( aRect );
		aContext.CreateDrawingSurface(aRect, 0, aSurface);
		return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
		}

  NS_IMETHOD  CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD  GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  inline NS_IMETHOD  GetClientRect(nsRect &aRect) { return GetRect ( aRect ); }
  NS_IMETHOD GetRect(nsRect &aRect);
 
  NS_IMETHOD  GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                  nsIDeviceContext *&aContext);

	NS_IMETHOD  BeginDocument(PRUnichar *t, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage);
  NS_IMETHOD  EndDocument(void);
  NS_IMETHOD  AbortDocument(void);

  NS_IMETHOD  BeginPage(void);
  NS_IMETHOD  EndPage(void);

	inline
  NS_IMETHODIMP GetDepth(PRUint32& aDepth)
		{
		aDepth = mDepth; // 24;
		return NS_OK;
		}

  static int prefChanged(const char *aPref, void *aClosure);
  nsresult    SetDPI(PRInt32 dpi);

protected:

  nsresult    Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext);
  nsresult    GetDisplayInfo(PRInt32 &aWidth, PRInt32 &aHeight, PRUint32 &aDepth);
  void        CommonInit(nsNativeDeviceContext aDC);
  void 		GetPrinterRect(int *width, int *height);

  nsIDrawingSurface*      mSurface;
  PRUint32              mDepth;  // bit depth of device
  float                 mPixelScale;
  PRInt16               mScrollbarHeight;
  PRInt16               mScrollbarWidth;
  
  float                 mWidthFloat;
  float                 mHeightFloat;
  PRInt32               mWidth;
  PRInt32               mHeight;

  nsIDeviceContextSpec  *mSpec;
  nsNativeDeviceContext mDC;
	PhGC_t								*mGC;

  static nscoord        mDpi;

  PRBool mIsPrintingStart;

private:
	nsCOMPtr<nsIScreenManager> mScreenManager;
	int ReadSystemFonts( ) const;
	void DefaultSystemFonts( ) const;
};

#define	NS_FONT_STYLE_ANTIALIAS				0xf0

#endif /* nsDeviceContextPh_h___ */
