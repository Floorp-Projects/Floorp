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
 *   Roland Mainz <Roland.Mainz@informatik.med.uni-giessen.de>
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

#ifndef nsDeviceContext_h___
#define nsDeviceContext_h___

#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"

class nsIImageRequest;
class nsHashtable;

class nsFontCache
{
public:
  nsFontCache();
  virtual ~nsFontCache();

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

  NS_IMETHOD FirstExistingFont(const nsFont& aFont, nsString& aFaceName);

  NS_IMETHOD GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                              PRBool& aAliased);

  NS_IMETHOD CreateFontCache();

  NS_IMETHOD FlushFontCache(void);

  NS_IMETHOD GetDepth(PRUint32& aDepth);

  NS_IMETHOD GetPaletteInfo(nsPaletteInfo& aPaletteInfo);

#ifdef NS_PRINT_PREVIEW
  NS_IMETHOD SetAltDevice(nsIDeviceContext* aAltDC);
  NS_IMETHOD GetAltDevice(nsIDeviceContext** aAltDC) { *aAltDC = mAltDC.get(); NS_IF_ADDREF(*aAltDC); return NS_OK;}
  NS_IMETHOD SetUseAltDC(PRUint8 aValue, PRBool aOn);
#else 

#endif

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
  nsHashtable*      mFontAliasTable;
  float             mCPixelScale;

#ifdef NS_PRINT_PREVIEW
  nsCOMPtr<nsIDeviceContext> mAltDC;
  PRUint8           mUseAltDC;
#endif

public:
  nsNativeWidget    mWidget;
};

#endif /* nsDeviceContext_h___ */
