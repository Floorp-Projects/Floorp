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

#include "nsIFontCache.h"
#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"

static NS_DEFINE_IID(kIFontCacheIID, NS_IFONT_CACHE_IID);

class FontCacheImpl : public nsIFontCache
{
public:
  FontCacheImpl();
  ~FontCacheImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsIDeviceContext* aContext);
  NS_IMETHOD  GetDeviceContext(nsIDeviceContext *&aContext) const;
  NS_IMETHOD  GetMetricsFor(const nsFont& aFont, nsIFontMetrics *&aMetrics);
  NS_IMETHOD  Flush();

protected:
  nsVoidArray       mFontMetrics;
  nsIDeviceContext  *mContext;
};

FontCacheImpl :: FontCacheImpl()
{
  NS_INIT_REFCNT();

  mContext = nsnull;
}

FontCacheImpl :: ~FontCacheImpl()
{
  Flush();
}

NS_IMPL_ISUPPORTS(FontCacheImpl, kIFontCacheIID)

NS_IMETHODIMP FontCacheImpl :: Init(nsIDeviceContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");
  // Note: we don't hold a reference to the device context, because it
  // holds a reference to us and we don't want circular references
  mContext = aContext;
  return NS_OK;
}

NS_IMETHODIMP FontCacheImpl::GetDeviceContext(nsIDeviceContext *&aContext) const
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP FontCacheImpl :: GetMetricsFor(const nsFont& aFont, nsIFontMetrics *&aMetrics)
{
  // First check our cache
  PRInt32 n = mFontMetrics.Count();

  for (PRInt32 cnt = 0; cnt < n; cnt++)
  {
    aMetrics = (nsIFontMetrics*) mFontMetrics.ElementAt(cnt);

    const nsFont* font;
    aMetrics->GetFont(font);
    if (aFont.Equals(*font))
    {
      NS_ADDREF(aMetrics);
      return NS_OK;
    }
  }

  // It's not in the cache. Get font metrics and then cache them.

  static NS_DEFINE_IID(kFontMetricsCID, NS_FONT_METRICS_CID);
  static NS_DEFINE_IID(kFontMetricsIID, NS_IFONT_METRICS_IID);

  nsIFontMetrics* fm;
  nsresult        rv = nsRepository::CreateInstance(kFontMetricsCID, nsnull,
                                                    kFontMetricsIID, (void **)&fm);
  if (NS_OK != rv) {
    aMetrics = nsnull;
    return rv;
  }

  rv = fm->Init(aFont, mContext);

  if (NS_OK != rv) {
    aMetrics = nsnull;
    return rv;
  }

  mFontMetrics.AppendElement(fm);

  NS_ADDREF(fm);
  aMetrics = fm;
  return NS_OK;
}

NS_IMETHODIMP FontCacheImpl::Flush()
{
  PRInt32 i, n = mFontMetrics.Count();
  for (i = 0; i < n; i++) {
    nsIFontMetrics* fm = (nsIFontMetrics*) mFontMetrics.ElementAt(i);
    fm->Destroy();
    NS_RELEASE(fm);
  }
  mFontMetrics.Clear();
  return NS_OK;
}

extern "C" NS_GFX_(nsresult)
NS_NewFontCache(nsIFontCache **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  nsIFontCache *cache = new FontCacheImpl();

  if (nsnull == cache)
    return NS_ERROR_OUT_OF_MEMORY;

  return cache->QueryInterface(kIFontCacheIID, (void **) aInstancePtrResult);
}
