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

#ifndef nsDeviceContext_h___
#define nsDeviceContext_h___

#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "libimg.h"

class nsIImageRequest;
class nsHashtable;
class nsFontCache;

class DeviceContextImpl : public nsIDeviceContext
{
public:
  DeviceContextImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsNativeWidget aWidget);

  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext);
  NS_IMETHOD  InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWindow);

  NS_IMETHOD  GetDevUnitsToTwips(float &aDevUnitsToTwips) const;
  NS_IMETHOD  GetTwipsToDevUnits(float &aTwipsToDevUnits) const;

  NS_IMETHOD  SetAppUnitsToDevUnits(float aAppUnits);
  NS_IMETHOD  SetDevUnitsToAppUnits(float aDevUnits);

  NS_IMETHOD  GetAppUnitsToDevUnits(float &aAppUnits) const;
  NS_IMETHOD  GetDevUnitsToAppUnits(float &aDevUnits) const;

  NS_IMETHOD  GetCanonicalPixelScale(float &aScale) const;

  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics);

  NS_IMETHOD  SetZoom(float aZoom);
  NS_IMETHOD  GetZoom(float &aZoom) const;

  NS_IMETHOD  GetGamma(float &aGamma);
  NS_IMETHOD  SetGamma(float aGamma);

  NS_IMETHOD  GetGammaTable(PRUint8 *&aGammaTable);

  NS_IMETHOD LoadIconImage(PRInt32 aId, nsIImage*& aImage);

  NS_IMETHOD FirstExistingFont(const nsFont& aFont, nsString& aFaceName);

  NS_IMETHOD GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                              PRBool& aAliased);

  NS_IMETHOD FlushFontCache(void);

  NS_IMETHOD GetDepth(PRUint32& aDepth);

  NS_IMETHOD GetILColorSpace(IL_ColorSpace*& aColorSpace);

  NS_IMETHOD GetPaletteInfo(nsPaletteInfo&);

protected:
  virtual ~DeviceContextImpl();

  void CommonInit(void);
  nsresult CreateFontCache();
  void SetGammaTable(PRUint8 * aTable, float aCurrentGamma, float aNewGamma);
  nsresult CreateIconILGroupContext();
  virtual nsresult CreateFontAliasTable();
  nsresult AliasFont(const nsString& aFont, 
                     const nsString& aAlias, const nsString& aAltAlias,
                     PRBool aForceAlias);

  float             mTwipsToPixels;
  float             mPixelsToTwips;
  float             mAppUnitsToDevUnits;
  float             mDevUnitsToAppUnits;
  nsFontCache       *mFontCache;
  float             mZoom;
  float             mGammaValue;
  PRUint8           *mGammaTable;
  IL_GroupContext*  mIconImageGroup;
  nsIImageRequest*  mIcons[NS_NUMBER_OF_ICONS];
  nsHashtable*      mFontAliasTable;
  IL_ColorSpace*    mColorSpace;

public:
  nsNativeWidget    mWidget;
};

#endif /* nsDeviceContext_h___ */
