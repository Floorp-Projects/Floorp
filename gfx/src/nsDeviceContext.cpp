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
 *   IBM Corp.
 */

#include "nsDeviceContext.h"
#include "nsFont.h"
#include "nsIView.h"
#include "nsGfxCIID.h"
#include "nsVoidArray.h"
#include "nsIFontMetrics.h"
#include "nsHashtable.h"
#include "nsILanguageAtomService.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"


NS_IMPL_ISUPPORTS1(DeviceContextImpl, nsIDeviceContext)

DeviceContextImpl :: DeviceContextImpl()
{
  NS_INIT_REFCNT();
  mFontCache = nsnull;
  mDevUnitsToAppUnits = 1.0f;
  mAppUnitsToDevUnits = 1.0f;
  mGammaValue = 1.0f;
  mCPixelScale = 1.0f;
  mGammaTable = new PRUint8[256];
  mZoom = 1.0f;
  mTextZoom = 1.0f;
  mWidget = nsnull;
  mFontAliasTable = nsnull;
}

static PRBool PR_CALLBACK DeleteValue(nsHashKey* aKey, void* aValue, void* closure)
{
  delete ((nsString*)aValue);
  return PR_TRUE;
}

DeviceContextImpl :: ~DeviceContextImpl()
{
  if (nsnull != mFontCache)
  {
    delete mFontCache;
    mFontCache = nsnull;
  }

  if (nsnull != mGammaTable)
  {
    delete[] mGammaTable;
    mGammaTable = nsnull;
  }

  if (nsnull != mFontAliasTable) {
    mFontAliasTable->Enumerate(DeleteValue);
    delete mFontAliasTable;
  }
}

NS_IMETHODIMP DeviceContextImpl :: Init(nsNativeWidget aWidget)
{
  mWidget = aWidget;

  CommonInit();

  return NS_OK;
}

void DeviceContextImpl :: CommonInit(void)
{
  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    mGammaTable[cnt] = cnt;
}

NS_IMETHODIMP DeviceContextImpl :: GetTwipsToDevUnits(float &aTwipsToDevUnits) const
{
  aTwipsToDevUnits = mTwipsToPixels;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetDevUnitsToTwips(float &aDevUnitsToTwips) const
{
  aDevUnitsToTwips = mPixelsToTwips;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: SetAppUnitsToDevUnits(float aAppUnits)
{
  mAppUnitsToDevUnits = aAppUnits;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: SetDevUnitsToAppUnits(float aDevUnits)
{
  mDevUnitsToAppUnits = aDevUnits;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetAppUnitsToDevUnits(float &aAppUnits) const
{
  aAppUnits = mAppUnitsToDevUnits;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetDevUnitsToAppUnits(float &aDevUnits) const
{
  aDevUnits = mDevUnitsToAppUnits;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetCanonicalPixelScale(float &aScale) const
{
  aScale = mCPixelScale;
  return NS_OK;
}

static NS_DEFINE_CID(kRCCID, NS_RENDERING_CONTEXT_CID);

NS_IMETHODIMP DeviceContextImpl :: CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext)
{
  nsIRenderingContext *pContext;
  nsIWidget           *win;
  aView->GetWidget(win);
  nsresult             rv;

  aContext = nsnull;
  rv = nsComponentManager::CreateInstance(kRCCID, nsnull, NS_GET_IID(nsIRenderingContext), (void **)&pContext);

  if (NS_OK == rv) {
    rv = InitRenderingContext(pContext, win);
    if (NS_OK != rv) {
      NS_RELEASE(pContext);
    }
  }

  NS_IF_RELEASE(win);
  aContext = pContext;
  return rv;
}

NS_IMETHODIMP DeviceContextImpl :: CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext)
{
  nsIRenderingContext *pContext;
  nsresult             rv;

  aContext = nsnull;
  rv = nsComponentManager::CreateInstance(kRCCID, nsnull, NS_GET_IID(nsIRenderingContext), (void **)&pContext);

  if (NS_OK == rv) {
    rv = InitRenderingContext(pContext, aWidget);
    if (NS_OK != rv) {
      NS_RELEASE(pContext);
    }
  }

  aContext = pContext;
  return rv;
}

NS_IMETHODIMP DeviceContextImpl :: InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  return aContext->Init(this, aWin);
}

NS_IMETHODIMP DeviceContextImpl::CreateFontCache()
{
  mFontCache = new nsFontCache();
  if (nsnull == mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mFontCache->Init(this);
  return NS_OK;
}

void
DeviceContextImpl::GetLocaleLangGroup(void)
{
  if (!mLocaleLangGroup) {
    nsCOMPtr<nsILanguageAtomService> langService;
    langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
    if (langService) {
      langService->GetLocaleLanguageGroup(getter_AddRefs(mLocaleLangGroup));
    }
    if (!mLocaleLangGroup) {
      mLocaleLangGroup = getter_AddRefs(NS_NewAtom("x-western"));
    }
  }
}

NS_IMETHODIMP DeviceContextImpl::GetMetricsFor(const nsFont& aFont,
  nsIAtom* aLangGroup, nsIFontMetrics*& aMetrics)
{
  if (nsnull == mFontCache) {
    nsresult  rv = CreateFontCache();
    if (NS_FAILED(rv)) {
      aMetrics = nsnull;
      return rv;
    }
    // XXX temporary fix for performance problem -- erik
    GetLocaleLangGroup();
  }

  // XXX figure out why aLangGroup is NULL sometimes
  if (!aLangGroup) {
    aLangGroup = mLocaleLangGroup;
  }

  return mFontCache->GetMetricsFor(aFont, aLangGroup, aMetrics);
}

NS_IMETHODIMP DeviceContextImpl::GetMetricsFor(const nsFont& aFont, nsIFontMetrics*& aMetrics)
{
  if (nsnull == mFontCache) {
    nsresult  rv = CreateFontCache();
    if (NS_FAILED(rv)) {
      aMetrics = nsnull;
      return rv;
    }
    // XXX temporary fix for performance problem -- erik
    GetLocaleLangGroup();
  }
  return mFontCache->GetMetricsFor(aFont, mLocaleLangGroup, aMetrics);
}

NS_IMETHODIMP DeviceContextImpl :: SetZoom(float aZoom)
{
  if (mZoom != aZoom) {
    mZoom = aZoom;
    FlushFontCache();
  }
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetZoom(float &aZoom) const
{
  aZoom = mZoom;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: SetTextZoom(float aTextZoom)
{
  if (mTextZoom != aTextZoom) {
    mTextZoom = aTextZoom;
    FlushFontCache();
  }
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetTextZoom(float &aTextZoom) const
{
  aTextZoom = mTextZoom;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetGamma(float &aGamma)
{
  aGamma = mGammaValue;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: SetGamma(float aGamma)
{
  if (aGamma != mGammaValue)
  {
    //we don't need to-recorrect existing images for this case
    //so pass in 1.0 for the current gamma regardless of what it
    //really happens to be. existing images will get a one time
    //re-correction when they're rendered the next time. MMP

    SetGammaTable(mGammaTable, 1.0f, aGamma);

    mGammaValue = aGamma;
  }
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl :: GetGammaTable(PRUint8 *&aGammaTable)
{
  //XXX we really need to ref count this somehow. MMP
  aGammaTable = mGammaTable;
  return NS_OK;
}

void DeviceContextImpl :: SetGammaTable(PRUint8 * aTable, float aCurrentGamma, float aNewGamma)
{
  double fgval = (1.0f / aCurrentGamma) * (1.0f / aNewGamma);

  for (PRInt32 cnt = 0; cnt < 256; cnt++)
    aTable[cnt] = (PRUint8)(pow((double)cnt * (1. / 256.), fgval) * 255.99999999);
}

NS_IMETHODIMP DeviceContextImpl::GetDepth(PRUint32& aDepth)
{
  aDepth = 24;
  return NS_OK;
}

struct FontEnumData {
  FontEnumData(nsIDeviceContext* aDC, nsString& aFaceName)
    : mDC(aDC), mFaceName(aFaceName)
  {}
  nsIDeviceContext* mDC;
  nsString&         mFaceName;
};

static PRBool FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  FontEnumData* data = (FontEnumData*)aData;
  // XXX for now, all generic fonts are presumed to exist
  //     we may want to actually check if there's an installed conversion
  if (aGeneric) {
    data->mFaceName = aFamily;
    return PR_FALSE; // found one, stop.
  }
  else {
    nsAutoString  local;
    PRBool        aliased;
    data->mDC->GetLocalFontName(aFamily, local, aliased);
    if (aliased || (NS_OK == data->mDC->CheckFontExistence(local))) {
      data->mFaceName = local;
      return PR_FALSE; // found one, stop.
    }
  }
  return PR_TRUE; // didn't exist, continue looking
}

NS_IMETHODIMP DeviceContextImpl::FirstExistingFont(const nsFont& aFont, nsString& aFaceName)
{
  FontEnumData  data(this, aFaceName);
  if (aFont.EnumerateFamilies(FontEnumCallback, &data)) {
    return NS_ERROR_FAILURE;  // ran out
  }
  return NS_OK;
}

class FontAliasKey: public nsHashKey 
{
public:
  FontAliasKey(const nsString& aString)
  {mString.Assign(aString);}

  virtual PRUint32 HashCode(void) const;
  virtual PRBool Equals(const nsHashKey *aKey) const;
  virtual nsHashKey *Clone(void) const;

  nsString  mString;
};

PRUint32 FontAliasKey::HashCode(void) const
{
  PRUint32 hash = 0;
  const PRUnichar* string = mString.get();
  PRUnichar ch;
  while ((ch = *string++) != 0) {
    // FYI: hash = hash*37 + ch
    ch = ToUpperCase(ch);
    hash = ((hash << 5) + (hash << 2) + hash) + ch;
  }
  return hash;
}

PRBool FontAliasKey::Equals(const nsHashKey *aKey) const
{
  return mString.EqualsIgnoreCase(((FontAliasKey*)aKey)->mString);
}

nsHashKey* FontAliasKey::Clone(void) const
{
  return new FontAliasKey(mString);
}
nsresult DeviceContextImpl::CreateFontAliasTable()
{
  nsresult result = NS_OK;

  if (nsnull == mFontAliasTable) {
    mFontAliasTable = new nsHashtable();
    if (nsnull != mFontAliasTable) {

      nsAutoString  times;              times.AssignWithConversion("Times");
      nsAutoString  timesNewRoman;      timesNewRoman.AssignWithConversion("Times New Roman");
      nsAutoString  timesRoman;         timesRoman.AssignWithConversion("Times Roman");
      nsAutoString  arial;              arial.AssignWithConversion("Arial");
      nsAutoString  helvetica;          helvetica.AssignWithConversion("Helvetica");
      nsAutoString  courier;            courier.AssignWithConversion("Courier");
      nsAutoString  courierNew;         courierNew.AssignWithConversion("Courier New");
      nsAutoString  nullStr;

      AliasFont(times, timesNewRoman, timesRoman, PR_FALSE);
      AliasFont(timesRoman, timesNewRoman, times, PR_FALSE);
      AliasFont(timesNewRoman, timesRoman, times, PR_FALSE);
      AliasFont(arial, helvetica, nullStr, PR_FALSE);
      AliasFont(helvetica, arial, nullStr, PR_FALSE);
      AliasFont(courier, courierNew, nullStr, PR_TRUE);
      AliasFont(courierNew, courier, nullStr, PR_FALSE);
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return result;
}

nsresult DeviceContextImpl::AliasFont(const nsString& aFont, 
                                      const nsString& aAlias, const nsString& aAltAlias,
                                      PRBool aForceAlias)
{
  nsresult result = NS_OK;

  if (nsnull != mFontAliasTable) {
    if (aForceAlias || (NS_OK != CheckFontExistence(aFont))) {
      if (NS_OK == CheckFontExistence(aAlias)) {
        nsString* entry = new nsString(aAlias);
        if (nsnull != entry) {
          FontAliasKey key(aFont);
          mFontAliasTable->Put(&key, entry);
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else if ((0 < aAltAlias.Length()) && (NS_OK == CheckFontExistence(aAltAlias))) {
        nsString* entry = new nsString(aAltAlias);
        if (nsnull != entry) {
          FontAliasKey key(aFont);
          mFontAliasTable->Put(&key, entry);
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }
  else {
    result = NS_ERROR_FAILURE;
  }
  return result;
}

NS_IMETHODIMP DeviceContextImpl::GetLocalFontName(const nsString& aFaceName, nsString& aLocalName,
                                                  PRBool& aAliased)
{
  nsresult result = NS_OK;

  if (nsnull == mFontAliasTable) {
    result = CreateFontAliasTable();
  }

  if (nsnull != mFontAliasTable) {
    FontAliasKey key(aFaceName);
    const nsString* alias = (const nsString*)mFontAliasTable->Get(&key);
    if (nsnull != alias) {
      aLocalName = *alias;
      aAliased = PR_TRUE;
    }
    else {
      aLocalName = aFaceName;
      aAliased = PR_FALSE;
    }
  }
  return result;
}

NS_IMETHODIMP DeviceContextImpl :: FlushFontCache(void)
{
  if (nsnull != mFontCache)
    mFontCache->Flush();

  return NS_OK;
}


/////////////////////////////////////////////////////////////

MOZ_DECL_CTOR_COUNTER(nsFontCache)

nsFontCache :: nsFontCache()
{
  MOZ_COUNT_CTOR(nsFontCache);
  mContext = nsnull;
}

nsFontCache :: ~nsFontCache()
{
  MOZ_COUNT_DTOR(nsFontCache);
  Flush();
}

NS_IMETHODIMP 
nsFontCache :: Init(nsIDeviceContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");
  // Note: we don't hold a reference to the device context, because it
  // holds a reference to us and we don't want circular references
  mContext = aContext;
  return NS_OK;
}

NS_IMETHODIMP 
nsFontCache :: GetDeviceContext(nsIDeviceContext *&aContext) const
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP 
nsFontCache :: GetMetricsFor(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIFontMetrics *&aMetrics)
{
  // First check our cache
  PRInt32 n = mFontMetrics.Count();

  for (PRInt32 cnt = 0; cnt < n; cnt++)
  {
    nsIFontMetrics* metrics = NS_STATIC_CAST(nsIFontMetrics*, mFontMetrics[cnt]);

    const nsFont* font;
    metrics->GetFont(font);
    if (aFont.Equals(*font)) {
      nsCOMPtr<nsIAtom> langGroup;
      metrics->GetLangGroup(getter_AddRefs(langGroup));
      if (aLangGroup == langGroup.get()) {
        if (cnt != 0) {
          // promote it to the front of the cache
          mFontMetrics.MoveElement(0, cnt);
        }
        NS_ADDREF(aMetrics = metrics);
        return NS_OK;
      }
    }
  }

  // It's not in the cache. Get font metrics and then cache them.

  nsIFontMetrics *fm = nsnull;
  nsresult rv = CreateFontMetricsInstance(&fm);

  if (NS_FAILED(rv)) {
    aMetrics = nsnull;
    return rv;
  }

  rv = fm->Init(aFont, aLangGroup, mContext);

  if (NS_FAILED(rv)) {
    aMetrics = nsnull;
    return rv;
  }

  mFontMetrics.AppendElement(fm);

  NS_ADDREF(fm);
  aMetrics = fm;
  return NS_OK;
}

/* PostScript and Xprint module may override this method to create 
 * nsIFontMetrics objects with their own classes 
 */
NS_IMETHODIMP
nsFontCache::CreateFontMetricsInstance(nsIFontMetrics** fm)
{
  static NS_DEFINE_CID(kFontMetricsCID, NS_FONT_METRICS_CID);
  return CallCreateInstance(kFontMetricsCID, fm);
}


nsresult nsFontCache :: Flush()
{
  PRInt32 i, n = mFontMetrics.Count();

  for (i = 0; i < n; i++)
  {
    nsIFontMetrics* fm = (nsIFontMetrics*) mFontMetrics.ElementAt(i);
    fm->Destroy();
    NS_RELEASE(fm);
  }

  mFontMetrics.Clear();

  return NS_OK;
}

