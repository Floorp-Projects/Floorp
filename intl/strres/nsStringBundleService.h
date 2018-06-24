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
#include "nsIMemoryReporter.h"

#include "mozilla/LinkedList.h"

struct bundleCacheEntry_t;

class nsStringBundleService : public nsIStringBundleService,
                              public nsIObserver,
                              public nsSupportsWeakReference,
                              public nsIMemoryReporter
{
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

public:
  nsStringBundleService();

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLESERVICE
  NS_DECL_NSIOBSERVER

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool anonymize) override
  {
    size_t amt = SizeOfIncludingThis(MallocSizeOf);

    MOZ_COLLECT_REPORT(
      "explicit/string-bundle-service", KIND_HEAP, UNITS_BYTES,
      amt,
      "Memory used for StringBundleService bundles");
    return NS_OK;
  };

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  void SendContentBundles(mozilla::dom::ContentParent* aContentParent) const override;

  void RegisterContentBundle(const nsCString& aBundleURL,
                             const mozilla::ipc::FileDescriptor& aMapFile,
                             size_t aMapSize) override;

private:
  virtual ~nsStringBundleService();

  void getStringBundle(const char *aUrl, nsIStringBundle** aResult);
  nsresult FormatWithBundle(nsIStringBundle* bundle, nsresult aStatus,
                            uint32_t argCount, char16_t** argArray,
                            nsAString& result);

  void flushBundleCache();

  bundleCacheEntry_t* insertIntoCache(already_AddRefed<nsIStringBundle> aBundle,
                                      const nsACString &aHashKey);

  nsDataHashtable<nsCStringHashKey, bundleCacheEntry_t*> mBundleMap;
  mozilla::LinkedList<bundleCacheEntry_t> mBundleCache;
  mozilla::AutoCleanLinkedList<bundleCacheEntry_t> mSharedBundles;

  nsCOMPtr<nsIErrorService> mErrorService;
};

#endif
