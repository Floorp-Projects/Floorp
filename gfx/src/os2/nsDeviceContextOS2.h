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

#ifndef nsDeviceContextOS2_h___
#define nsDeviceContextOS2_h___

#include "nsGfxDefs.h"

#include "nsDeviceContext.h"
#include "nsIScreenManager.h"
#include "nsDeviceContextSpecOS2.h"

class nsIScreen;

class nsDeviceContextOS2 : public DeviceContextImpl
{
public:
  nsDeviceContextOS2();

  NS_IMETHOD  Init(nsNativeWidget aWidget);

  using DeviceContextImpl::CreateRenderingContext;
  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext);

  NS_IMETHOD  SupportsNativeWidgets(PRBool &aSupportsWidgets);

  NS_IMETHOD  GetScrollBarDimensions(float &aWidth, float &aHeight) const;
  NS_IMETHOD  GetSystemFont(nsSystemFontID anID, nsFont *aFont) const;

  NS_IMETHOD  CheckFontExistence(const nsString& aFontName);

  NS_IMETHOD  GetDepth(PRUint32& aDepth);

  NS_IMETHOD GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD GetRect(nsRect &aRect);
  NS_IMETHOD GetClientRect(nsRect &aRect);

  NS_IMETHOD GetDeviceContextFor(nsIDeviceContextSpec *aDevice,
                                 nsIDeviceContext *&aContext);

  NS_IMETHOD PrepareDocument(PRUnichar * aTitle, 
                             PRUnichar*  aPrintToFileName);
  NS_IMETHOD BeginDocument(PRUnichar * aTitle, PRUnichar* aPrintToFileName, PRInt32 aStartPage, PRInt32 aEndPage);
  NS_IMETHOD EndDocument(void);
  NS_IMETHOD AbortDocument(void);

  NS_IMETHOD BeginPage(void);
  NS_IMETHOD EndPage(void);

  // Static Helper Methods
  static char* GetACPString(const nsString& aStr);
  nsresult   SetDPI(PRInt32 aPrefDPI);
  int        GetDPI() { return mDpi; };

protected:
  virtual ~nsDeviceContextOS2();
  void CommonInit(HDC aDC);
  nsresult Init(nsNativeDeviceContext aContext, nsIDeviceContext *aOrigContext);
  void FindScreen ( nsIScreen** outScreen ) ;
  void ComputeClientRectUsingScreen ( nsRect* outRect ) ;
  void ComputeFullAreaUsingScreen ( nsRect* outRect ) ;

  PRBool mCachedClientRect;
  PRBool mCachedFullRect;
  PRBool mPrintingStarted;

  nsIDrawingSurface*      mSurface;
  PRUint32              mDepth;  // bit depth of device
  PRBool                mIsPaletteDevice;
  PRInt32               mWidth;
  PRInt32               mHeight;
  nsRect                mClientRect;
  nsIDeviceContextSpec  *mSpec;
  PRBool                mSupportsRasterFonts;

  nsCOMPtr<nsIScreenManager> mScreenManager;
  static PRUint32 sNumberOfScreens;
  static nscoord mDpi;

public:
  HDC                   mPrintDC;
  HPS                   mPrintPS;

  enum nsPrintState
  {
     nsPrintState_ePreBeginDoc,
     nsPrintState_eBegunDoc,
     nsPrintState_eBegunFirstPage,
     nsPrintState_eEndedDoc
  } mPrintState;

  BOOL isPrintDC();
  PRBool SupportsRasterFonts();
  PRBool IsPaletteDevice() {return mIsPaletteDevice;};
  nsresult CreateFontAliasTable();
};

int PR_CALLBACK prefChanged(const char *aPref, void *aClosure);

#endif /* nsDeviceContextOS2_h___ */
