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
#include "libimg.h"

class nsIImageRequest;
class nsHashtable;

class DeviceContextImpl : public nsIDeviceContext
{
public:
  DeviceContextImpl();

  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsNativeWidget aWidget);

  virtual nsIRenderingContext * CreateRenderingContext(nsIView *aView);
  virtual nsresult InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWindow);

  virtual float GetDevUnitsToTwips() const;
  virtual float GetTwipsToDevUnits() const;

  virtual void SetAppUnitsToDevUnits(float aAppUnits);
  virtual void SetDevUnitsToAppUnits(float aDevUnits);

  virtual float GetAppUnitsToDevUnits() const;
  virtual float GetDevUnitsToAppUnits() const;

  virtual nsIFontCache * GetFontCache();
  virtual void FlushFontCache();

  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont);

  virtual void SetZoom(float aZoom);
  virtual float GetZoom() const;

  virtual float GetGamma(void);
  virtual void SetGamma(float aGamma);

  virtual PRUint8 * GetGammaTable(void);

  virtual nsNativeWidget GetNativeWidget(void);

  NS_IMETHOD LoadIconImage(PRInt32 aId, nsIImage*& aImage);

  NS_IMETHOD FirstExistingFont(const nsFont& aFont, nsString& aFaceName);

  NS_IMETHOD GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                              PRBool& aAliased);

  NS_IMETHOD GetDepth(PRUint32& aDepth);

  NS_IMETHOD GetILColorSpace(IL_ColorSpace*& aColorSpace);

protected:
  virtual ~DeviceContextImpl();

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
  nsIFontCache      *mFontCache;
  float             mZoom;
  float             mGammaValue;
  PRUint8           *mGammaTable;
  nsNativeWidget    mWidget;
  IL_GroupContext*  mIconImageGroup;
  nsIImageRequest*  mIcons[NS_NUMBER_OF_ICONS];
  nsHashtable*      mFontAliasTable;
  IL_ColorSpace*    mColorSpace;
};

#endif /* nsDeviceContext_h___ */
