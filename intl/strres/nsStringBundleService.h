/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStringBundleService_h__
#define nsStringBundleService_h__

#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
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

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLESERVICE
  NS_DECL_NSIOBSERVER

private:
  virtual ~nsStringBundleService();

  nsresult getStringBundle(const char *aUrl, nsIStringBundle** aResult);
  nsresult FormatWithBundle(nsIStringBundle* bundle, nsresult aStatus,
                            uint32_t argCount, char16_t** argArray,
                            char16_t* *result);

  void flushBundleCache();

  bundleCacheEntry_t *insertIntoCache(already_AddRefed<nsIStringBundle> aBundle,
                                      nsCString &aHashKey);

  nsDataHashtable<nsCStringHashKey, bundleCacheEntry_t*> mBundleMap;
  mozilla::LinkedList<bundleCacheEntry_t> mBundleCache;

  nsCOMPtr<nsIErrorService> mErrorService;
  nsCOMPtr<nsIStringBundleOverride> mOverrideStrings;
};

#endif
