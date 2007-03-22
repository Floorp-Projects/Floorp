/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Roland Mainz <Roland.Mainz@informatik.med.uni-giessen.de>
 *   IBM Corp.
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
#include "nsCRT.h"
#include "nsIRenderingContext.h"

NS_IMPL_ISUPPORTS3(DeviceContextImpl, nsIDeviceContext, nsIObserver, nsISupportsWeakReference)

DeviceContextImpl::DeviceContextImpl()
{
  mAppUnitsPerDevPixel = -1;
  mAppUnitsPerInch = -1;
  mFontCache = nsnull;
  mWidget = nsnull;
  mFontAliasTable = nsnull;

#ifdef NS_DEBUG
  mInitialized = PR_FALSE;
#endif
}

static PRBool PR_CALLBACK DeleteValue(nsHashKey* aKey, void* aValue, void* closure)
{
  delete ((nsString*)aValue);
  return PR_TRUE;
}

DeviceContextImpl::~DeviceContextImpl()
{
  nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1"));
  if (obs)
    obs->RemoveObserver(this, "memory-pressure");

  if (nsnull != mFontCache)
  {
    delete mFontCache;
    mFontCache = nsnull;
  }

  if (nsnull != mFontAliasTable) {
    mFontAliasTable->Enumerate(DeleteValue);
    delete mFontAliasTable;
  }

}

NS_IMETHODIMP
DeviceContextImpl::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aSomeData)
{
  if (mFontCache && !nsCRT::strcmp(aTopic, "memory-pressure")) {
    mFontCache->Compact();
  }
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl::Init(nsNativeWidget aWidget)
{
  mWidget = aWidget;

  CommonInit();

  return NS_OK;
}

void DeviceContextImpl::CommonInit(void)
{
#ifdef NS_DEBUG
  NS_ASSERTION(!mInitialized, "device context is initialized twice!");
  mInitialized = PR_TRUE;
#endif

  // register as a memory-pressure observer to free font resources
  // in low-memory situations.
  nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1"));
  if (obs)
    obs->AddObserver(this, "memory-pressure", PR_TRUE);
}

NS_IMETHODIMP DeviceContextImpl::CreateRenderingContext(nsIView *aView, nsIRenderingContext *&aContext)
{
  nsresult rv;

  aContext = nsnull;
  nsCOMPtr<nsIRenderingContext> pContext;
  rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
  if (NS_SUCCEEDED(rv)) {
    rv = InitRenderingContext(pContext, aView->GetWidget());
    if (NS_SUCCEEDED(rv)) {
      aContext = pContext;
      NS_ADDREF(aContext);
    }
  }
  
  return rv;
}

NS_IMETHODIMP DeviceContextImpl::CreateRenderingContext(nsIDrawingSurface* aSurface, nsIRenderingContext *&aContext)
{
  nsresult rv;

  aContext = nsnull;
  nsCOMPtr<nsIRenderingContext> pContext;
  rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
  if (NS_SUCCEEDED(rv)) {
    rv = InitRenderingContext(pContext, aSurface);
    if (NS_SUCCEEDED(rv)) {
      aContext = pContext;
      NS_ADDREF(aContext);
    }
  }
  
  return rv;
}

NS_IMETHODIMP DeviceContextImpl::CreateRenderingContext(nsIWidget *aWidget, nsIRenderingContext *&aContext)
{
  nsresult rv;

  aContext = nsnull;
  nsCOMPtr<nsIRenderingContext> pContext;
  rv = CreateRenderingContextInstance(*getter_AddRefs(pContext));
  if (NS_SUCCEEDED(rv)) {
    rv = InitRenderingContext(pContext, aWidget);
    if (NS_SUCCEEDED(rv)) {
      aContext = pContext;
      NS_ADDREF(aContext);
    }
  }    
  
  return rv;
}

NS_IMETHODIMP DeviceContextImpl::CreateRenderingContextInstance(nsIRenderingContext *&aContext)
{
  static NS_DEFINE_CID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);

  nsresult rv;
  nsCOMPtr<nsIRenderingContext> pContext = do_CreateInstance(kRenderingContextCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    aContext = pContext;
    NS_ADDREF(aContext);
  }
  return rv;
}

nsresult DeviceContextImpl::InitRenderingContext(nsIRenderingContext *aContext, nsIWidget *aWin)
{
  return aContext->Init(this, aWin);
}

nsresult DeviceContextImpl::InitRenderingContext(nsIRenderingContext *aContext, nsIDrawingSurface* aSurface)
{
  return aContext->Init(this, aSurface);
}

NS_IMETHODIMP DeviceContextImpl::CreateFontCache()
{
  mFontCache = new nsFontCache();
  if (!mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return mFontCache->Init(this);
}

NS_IMETHODIMP DeviceContextImpl::FontMetricsDeleted(const nsIFontMetrics* aFontMetrics)
{
  if (mFontCache) {
    mFontCache->FontMetricsDeleted(aFontMetrics);
  }
  return NS_OK;
}

void
DeviceContextImpl::GetLocaleLangGroup(void)
{
  if (!mLocaleLangGroup) {
    nsCOMPtr<nsILanguageAtomService> langService;
    langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
    if (langService) {
      mLocaleLangGroup = langService->GetLocaleLanguageGroup();
    }
    if (!mLocaleLangGroup) {
      mLocaleLangGroup = do_GetAtom("x-western");
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

NS_IMETHODIMP DeviceContextImpl::GetDepth(PRUint32& aDepth)
{
  aDepth = 24;
  return NS_OK;
}

NS_IMETHODIMP DeviceContextImpl::GetPaletteInfo(nsPaletteInfo& aPaletteInfo)
{
  aPaletteInfo.isPaletteDevice = PR_FALSE;
  aPaletteInfo.sizePalette = 0;
  aPaletteInfo.numReserved = 0;
  aPaletteInfo.palette = nsnull;
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
    if (aliased || (NS_SUCCEEDED(data->mDC->CheckFontExistence(local)))) {
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
  return mString.Equals(((FontAliasKey*)aKey)->mString, nsCaseInsensitiveStringComparator());
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

      nsAutoString  times;              times.AssignLiteral("Times");
      nsAutoString  timesNewRoman;      timesNewRoman.AssignLiteral("Times New Roman");
      nsAutoString  timesRoman;         timesRoman.AssignLiteral("Times Roman");
      nsAutoString  arial;              arial.AssignLiteral("Arial");
      nsAutoString  helvetica;          helvetica.AssignLiteral("Helvetica");
      nsAutoString  courier;            courier.AssignLiteral("Courier");
      nsAutoString  courierNew;         courierNew.AssignLiteral("Courier New");
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
    if (aForceAlias || NS_FAILED(CheckFontExistence(aFont))) {
      if (NS_SUCCEEDED(CheckFontExistence(aAlias))) {
        nsString* entry = new nsString(aAlias);
        if (nsnull != entry) {
          FontAliasKey key(aFont);
          mFontAliasTable->Put(&key, entry);
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else if (!aAltAlias.IsEmpty() && NS_SUCCEEDED(CheckFontExistence(aAltAlias))) {
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

NS_IMETHODIMP DeviceContextImpl::FlushFontCache(void)
{
  if (nsnull != mFontCache)
    mFontCache->Flush();

  return NS_OK;
}

/////////////////////////////////////////////////////////////

nsFontCache::nsFontCache()
{
  MOZ_COUNT_CTOR(nsFontCache);
  mContext = nsnull;
}

nsFontCache::~nsFontCache()
{
  MOZ_COUNT_DTOR(nsFontCache);
  Flush();
}

nsresult
nsFontCache::Init(nsIDeviceContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");
  // Note: we don't hold a reference to the device context, because it
  // holds a reference to us and we don't want circular references
  mContext = aContext;
  return NS_OK;
}

nsresult
nsFontCache::GetDeviceContext(nsIDeviceContext *&aContext) const
{
  aContext = mContext;
  NS_IF_ADDREF(aContext);
  return NS_OK;
}

nsresult
nsFontCache::GetMetricsFor(const nsFont& aFont, nsIAtom* aLangGroup,
  nsIFontMetrics *&aMetrics)
{
  // First check our cache
  // start from the end, which is where we put the most-recent-used element

  nsIFontMetrics* fm;
  PRInt32 n = mFontMetrics.Count() - 1;
  for (PRInt32 i = n; i >= 0; --i) {
    fm = NS_STATIC_CAST(nsIFontMetrics*, mFontMetrics[i]);
    if (fm->Font().Equals(aFont)) {
      nsCOMPtr<nsIAtom> langGroup;
      fm->GetLangGroup(getter_AddRefs(langGroup));
      if (aLangGroup == langGroup.get()) {
        if (i != n) {
          // promote it to the end of the cache
          mFontMetrics.MoveElement(i, n);
        }
        NS_ADDREF(aMetrics = fm);
        return NS_OK;
      }
    }
  }

  // It's not in the cache. Get font metrics and then cache them.

  aMetrics = nsnull;
  nsresult rv = CreateFontMetricsInstance(&fm);
  if (NS_FAILED(rv)) return rv;
  rv = fm->Init(aFont, aLangGroup, mContext);
  if (NS_SUCCEEDED(rv)) {
    // the mFontMetrics list has the "head" at the end, because append is
    // cheaper than insert
    mFontMetrics.AppendElement(fm);
    aMetrics = fm;
    NS_ADDREF(aMetrics);
    return NS_OK;
  }
  fm->Destroy();
  NS_RELEASE(fm);

  // One reason why Init() fails is because the system is running out of resources. 
  // e.g., on Win95/98 only a very limited number of GDI objects are available.
  // Compact the cache and try again.

  Compact();
  rv = CreateFontMetricsInstance(&fm);
  if (NS_FAILED(rv)) return rv;
  rv = fm->Init(aFont, aLangGroup, mContext);
  if (NS_SUCCEEDED(rv)) {
    mFontMetrics.AppendElement(fm);
    aMetrics = fm;
    NS_ADDREF(aMetrics);
    return NS_OK;
  }
  fm->Destroy();
  NS_RELEASE(fm);

  // could not setup a new one, send an old one (XXX search a "best match"?)

  n = mFontMetrics.Count() - 1; // could have changed in Compact()
  if (n >= 0) {
    aMetrics = NS_STATIC_CAST(nsIFontMetrics*, mFontMetrics[n]);
    NS_ADDREF(aMetrics);
    return NS_OK;
  }

  NS_POSTCONDITION(NS_SUCCEEDED(rv), "font metrics should not be null - bug 136248");
  return rv;
}

/* PostScript and Xprint module may override this method to create 
 * nsIFontMetrics objects with their own classes 
 */
nsresult
nsFontCache::CreateFontMetricsInstance(nsIFontMetrics** fm)
{
  static NS_DEFINE_CID(kFontMetricsCID, NS_FONT_METRICS_CID);
  return CallCreateInstance(kFontMetricsCID, fm);
}

nsresult nsFontCache::FontMetricsDeleted(const nsIFontMetrics* aFontMetrics)
{
  mFontMetrics.RemoveElement((void*)aFontMetrics);
  return NS_OK;
}

nsresult nsFontCache::Compact()
{
  // Need to loop backward because the running element can be removed on the way
  for (PRInt32 i = mFontMetrics.Count()-1; i >= 0; --i) {
    nsIFontMetrics* fm = NS_STATIC_CAST(nsIFontMetrics*, mFontMetrics[i]);
    nsIFontMetrics* oldfm = fm;
    // Destroy() isn't here because we want our device context to be notified
    NS_RELEASE(fm); // this will reset fm to nsnull
    // if the font is really gone, it would have called back in
    // FontMetricsDeleted() and would have removed itself
    if (mFontMetrics.IndexOf(oldfm) >= 0) {
      // nope, the font is still there, so let's hold onto it too
      NS_ADDREF(oldfm);
    }
  }
  return NS_OK;
}

nsresult nsFontCache::Flush()
{
  for (PRInt32 i = mFontMetrics.Count()-1; i >= 0; --i) {
    nsIFontMetrics* fm = NS_STATIC_CAST(nsIFontMetrics*, mFontMetrics[i]);
    // Destroy() will unhook our device context from the fm so that we won't
    // waste time in triggering the notification of FontMetricsDeleted()
    // in the subsequent release
    fm->Destroy();
    NS_RELEASE(fm);
  }

  mFontMetrics.Clear();

  return NS_OK;
}

NS_IMETHODIMP
DeviceContextImpl::PrepareNativeWidget(nsIWidget *aWidget, void **aOut)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DeviceContextImpl::ClearCachedSystemFonts()
{
  return NS_OK;
}
