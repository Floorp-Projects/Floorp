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

#ifndef nsDeviceContextOS2_h___
#define nsDeviceContextOS2_h___

#include "nsGfxDefs.h"

#include "nsDeviceContext.h"
#include "nsIScreenManager.h"
#include "libprint.h"

class nsIScreen;

class nsDeviceContextOS2 : public DeviceContextImpl
{
public:
  nsDeviceContextOS2();

  NS_IMETHOD  Init(nsNativeWidget aWidget);

  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);

  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetCanonicalPixelScale(float &aScale) const;

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;
  NS_IMETHOD  GetSystemFont(nsSystemFontID anID, nsFont *aFont) const;

  //get a low level drawing surface for rendering. the rendering context
  //that is passed in is used to create the drawing surface if there isn't
  //already one in the device context. the drawing surface is then cached
  //in the device context for re-use.
  NS_IMETHOD  GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface);

  NS_IMETHOD  CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD  GetDepth(PRUint32& aDepth);

#ifdef COLOR_256
  NS_IMETHOD  GetILColorSpace(IL_ColorSpace*& aColorSpace);

  NS_IMETHOD  GetPaletteInfo(nsPaletteInfo&);
#endif

  NS_IMETHOD ConvertPixel(nscolor aColor, PRUint32 & aPixel);

  NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD GetRect(nsRect &aRect);
  NS_IMETHOD GetClientRect(nsRect &aRect);

  NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                 nsIDeviceContext *&aContext);

  NS_IMETHOD BeginDocument(PRUnichar * aTitle);
  NS_IMETHOD EndDocument(void);

  NS_IMETHOD BeginPage(void);
  NS_IMETHOD EndPage(void);

  // Static Helper Methods
  static char* GetACPString(const nsString& aStr);

protected:
  virtual ~nsDeviceContextOS2();
  void CommonInit(HDC aDC);
  nsresult Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext);
  void FindScreen ( nsIScreen** outScreen ) ;
  void ComputeClientRectUsingScreen ( nsRect* outRect ) ;
  void ComputeFullAreaUsingScreen ( nsRect* outRect ) ;

  PRBool mCachedClientRect;
  PRBool mCachedFullRect;

  nsDrawingSurface      mSurface;
  PRUint32              mDepth;  // bit depth of device
#ifdef COLOR_256
  nsPaletteInfo         mPaletteInfo;
#endif
  float                 mPixelScale;
  PRInt32               mWidth;
  PRInt32               mHeight;
  nsRect                mClientRect;
  nsIDeviceContextSpec  *mSpec;
  PRBool                mSupportsRasterFonts;
  PRUint32              mPelsPerMeter;

  nsCOMPtr<nsIScreenManager> mScreenManager;
  static PRUint32 sNumberOfScreens;

public:
  HDC                   mPrintDC;
  HPS                   mPrintPS;

  static PRBool gRound;
  static int    PrefChanged(const char* aPref, void* aClosure);

  enum nsPrintState
  {
     nsPrintState_ePreBeginDoc,
     nsPrintState_eBegunDoc,
     nsPrintState_eBegunFirstPage,
     nsPrintState_eEndedDoc
  } mPrintState;

  BOOL isPrintDC();
  PRBool SupportsRasterFonts();
  nsresult nsDeviceContextOS2::CreateFontAliasTable();
};

#endif /* nsDeviceContextOS2_h___ */
