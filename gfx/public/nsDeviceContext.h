/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <Roland.Mainz@informatik.med.uni-giessen.de>
 */

#ifndef nsDeviceContext_h___
#define nsDeviceContext_h___

#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "libimg.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIStringBundle.h"
#include "nsVoidArray.h"

class nsIImageRequest;
class nsHashtable;

class nsFontCache
{
public:
  nsFontCache();
  ~nsFontCache();

  NS_IMETHOD Init(nsIDeviceContext* aContext);
  NS_IMETHOD GetDeviceContext(nsIDeviceContext *&aContext) const;
  NS_IMETHOD GetMetricsFor(const nsFont& aFont, nsIAtom* aLangGroup,
                           nsIFontMetrics *&aMetrics);
  nsresult   Flush();
  /* printer device context classes may create their own
   * subclasses of nsFontCache (and override this method) and override 
   * DeviceContextImpl::CreateFontCache (see bug 81311).
   */           
  NS_IMETHOD CreateFontMetricsInstance(nsIFontMetrics** fm);
  
protected:
  nsVoidArray      mFontMetrics;
  nsIDeviceContext *mContext; // we do not addref this since
                              // ownership is implied. MMP.
};

class NS_GFX DeviceContextImpl : public nsIDeviceContext
{
public:
  DeviceContextImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsNativeWidget aWidget);

  NS_IMETHOD  CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext);
  NS_IMETHOD  CreateRenderingContext(nsIRenderingContext *&aContext){return NS_ERROR_NOT_IMPLEMENTED;}

  NS_IMETHOD  InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWindow);

  NS_IMETHOD  GetDevUnitsToTwips(float &aDevUnitsToTwips) const;
  NS_IMETHOD  GetTwipsToDevUnits(float &aTwipsToDevUnits) const;

  NS_IMETHOD  SetAppUnitsToDevUnits(float aAppUnits);
  NS_IMETHOD  SetDevUnitsToAppUnits(float aDevUnits);

  NS_IMETHOD  GetAppUnitsToDevUnits(float &aAppUnits) const;
  NS_IMETHOD  GetDevUnitsToAppUnits(float &aDevUnits) const;

  NS_IMETHOD  GetCanonicalPixelScale(float &aScale) const;

  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIAtom* aLangGroup,
                            nsIFontMetrics*& aMetrics);
  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics);

  NS_IMETHOD  SetZoom(float aZoom);
  NS_IMETHOD  GetZoom(float &aZoom) const;

  NS_IMETHOD  SetTextZoom(float aTextZoom);
  NS_IMETHOD  GetTextZoom(float &aTextZoom) const;

  NS_IMETHOD  GetGamma(float &aGamma);
  NS_IMETHOD  SetGamma(float aGamma);

  NS_IMETHOD  GetGammaTable(PRUint8 *&aGammaTable);

  NS_IMETHOD LoadIconImage(PRInt32 aId, nsIImage*& aImage);

  NS_IMETHOD FirstExistingFont(const nsFont& aFont, nsString& aFaceName);

  NS_IMETHOD GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                              PRBool& aAliased);

  NS_IMETHOD CreateFontCache();

  NS_IMETHOD FlushFontCache(void);

  NS_IMETHOD GetDepth(PRUint32& aDepth);

  NS_IMETHOD GetILColorSpace(IL_ColorSpace*& aColorSpace);

  NS_IMETHOD GetPaletteInfo(nsPaletteInfo&);

  static nsresult GetLocalizedString(nsIStringBundle* aStrBundle, const char* aKey, nsString& oVal);
  static nsresult GetLocalizedBundle(const char * aPropFileName, nsIStringBundle** aStrBundle);

protected:
  virtual ~DeviceContextImpl();

  void CommonInit(void);
  void SetGammaTable(PRUint8 * aTable, float aCurrentGamma, float aNewGamma);
  nsresult CreateIconILGroupContext();
  virtual nsresult CreateFontAliasTable();
  nsresult AliasFont(const nsString& aFont, 
                     const nsString& aAlias, const nsString& aAltAlias,
                     PRBool aForceAlias);
  void GetLocaleLangGroup(void);

  float             mTwipsToPixels;
  float             mPixelsToTwips;
  float             mAppUnitsToDevUnits;
  float             mDevUnitsToAppUnits;
  nsFontCache       *mFontCache;
  nsCOMPtr<nsIAtom> mLocaleLangGroup; // XXX temp fix for performance bug - erik
  float             mZoom;
  float             mTextZoom;
  float             mGammaValue;
  PRUint8           *mGammaTable;
  IL_GroupContext*  mIconImageGroup;
  nsIImageRequest*  mIcons[NS_NUMBER_OF_ICONS];
  nsHashtable*      mFontAliasTable;
  IL_ColorSpace*    mColorSpace;
  float             mCPixelScale;

public:
  nsNativeWidget    mWidget;
};

#endif /* nsDeviceContext_h___ */
