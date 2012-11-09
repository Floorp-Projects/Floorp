/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringBundleService_h__
#define nsStringBundleService_h__

#include "plarena.h"

#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIPersistentProperties2.h"
#include "nsIStringBundle.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIErrorService.h"
#include "nsIStringBundleOverride.h"

#include "mozilla/LinkedList.h"

struct bundleCacheEntry_t;

class nsStringBundleService : public nsIStringBundleService,
                              public nsIObserver,
                              public nsSupportsWeakReference
{
public:
  nsStringBundleService();
  virtual ~nsStringBundleService();

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLESERVICE
  NS_DECL_NSIOBSERVER
    
private:
  nsresult getStringBundle(const char *aUrl, nsIStringBundle** aResult);
  nsresult FormatWithBundle(nsIStringBundle* bundle, nsresult aStatus, 
                            uint32_t argCount, PRUnichar** argArray,
                            PRUnichar* *result);

  void flushBundleCache();
  
  bundleCacheEntry_t *insertIntoCache(nsIStringBundle *aBundle,
                                      nsCStringKey *aHashKey);

  static void recycleEntry(bundleCacheEntry_t*);
  
  nsHashtable mBundleMap;
  mozilla::LinkedList<bundleCacheEntry_t> mBundleCache;
  PLArenaPool mCacheEntryPool;

  nsCOMPtr<nsIErrorService>     mErrorService;
  nsCOMPtr<nsIStringBundleOverride> mOverrideStrings;
};

#endif
