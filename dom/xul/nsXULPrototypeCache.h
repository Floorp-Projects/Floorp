/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULPrototypeCache_h__
#define nsXULPrototypeCache_h__

#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsURIHashKey.h"
#include "nsXULPrototypeDocument.h"
#include "nsIStorageStream.h"

#include "mozilla/scache/StartupCache.h"
#include "js/experimental/JSStencil.h"
#include "mozilla/RefPtr.h"

class nsIHandleReportCallback;
namespace mozilla {
class StyleSheet;
}  // namespace mozilla

/**
 * The XUL prototype cache can be used to store and retrieve shared data for
 * XUL documents, style sheets, XBL, and scripts.
 *
 * The cache has two levels:
 *  1. In-memory hashtables
 *  2. The on-disk cache file.
 */
class nsXULPrototypeCache : public nsIObserver {
 public:
  enum class CacheType { Prototype, Script };

  // nsISupports
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  bool IsCached(nsIURI* aURI) { return GetPrototype(aURI) != nullptr; }
  void AbortCaching();

  /**
   * Whether the prototype cache is enabled.
   */
  bool IsEnabled();

  /**
   * Flush the cache; remove all XUL prototype documents, style
   * sheets, and scripts.
   */
  void Flush();

  // The following methods are used to put and retrive various items into and
  // from the cache.

  nsXULPrototypeDocument* GetPrototype(nsIURI* aURI);
  nsresult PutPrototype(nsXULPrototypeDocument* aDocument);
  void RemovePrototype(nsIURI* aURI) { mPrototypeTable.Remove(aURI); }

  JS::Stencil* GetStencil(nsIURI* aURI);
  nsresult PutStencil(nsIURI* aURI, JS::Stencil* aStencil);

  /**
   * Write the XUL prototype document to a cache file. The proto must be
   * fully loaded.
   */
  nsresult WritePrototype(nsXULPrototypeDocument* aPrototypeDocument);

  /**
   * This interface allows partial reads and writes from the buffers in the
   * startupCache.
   */

  inline nsresult GetPrototypeInputStream(nsIURI* aURI,
                                          nsIObjectInputStream** objectInput) {
    return GetInputStream(CacheType::Prototype, aURI, objectInput);
  }
  inline nsresult GetScriptInputStream(nsIURI* aURI,
                                       nsIObjectInputStream** objectInput) {
    return GetInputStream(CacheType::Script, aURI, objectInput);
  }
  inline nsresult FinishScriptInputStream(nsIURI* aURI) {
    return FinishInputStream(aURI);
  }

  inline nsresult GetPrototypeOutputStream(
      nsIURI* aURI, nsIObjectOutputStream** objectOutput) {
    return GetOutputStream(aURI, objectOutput);
  }
  inline nsresult GetScriptOutputStream(nsIURI* aURI,
                                        nsIObjectOutputStream** objectOutput) {
    return GetOutputStream(aURI, objectOutput);
  }

  inline nsresult FinishPrototypeOutputStream(nsIURI* aURI) {
    return FinishOutputStream(CacheType::Prototype, aURI);
  }
  inline nsresult FinishScriptOutputStream(nsIURI* aURI) {
    return FinishOutputStream(CacheType::Script, aURI);
  }

  inline nsresult HasPrototype(nsIURI* aURI, bool* exists) {
    return HasData(CacheType::Prototype, aURI, exists);
  }
  inline nsresult HasScript(nsIURI* aURI, bool* exists) {
    return HasData(CacheType::Script, aURI, exists);
  }

 private:
  nsresult GetInputStream(CacheType cacheType, nsIURI* uri,
                          nsIObjectInputStream** stream);
  nsresult FinishInputStream(nsIURI* aURI);

  nsresult GetOutputStream(nsIURI* aURI, nsIObjectOutputStream** objectOutput);
  nsresult FinishOutputStream(CacheType cacheType, nsIURI* aURI);
  nsresult HasData(CacheType cacheType, nsIURI* aURI, bool* exists);

 public:
  static nsXULPrototypeCache* GetInstance();
  static nsXULPrototypeCache* MaybeGetInstance() { return sInstance; }

  static void ReleaseGlobals() { NS_IF_RELEASE(sInstance); }

  void MarkInCCGeneration(uint32_t aGeneration);

  static void CollectMemoryReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData);

 protected:
  friend nsresult NS_NewXULPrototypeCache(REFNSIID aIID, void** aResult);

  nsXULPrototypeCache();
  virtual ~nsXULPrototypeCache() = default;

  static nsXULPrototypeCache* sInstance;

  nsRefPtrHashtable<nsURIHashKey, nsXULPrototypeDocument>
      mPrototypeTable;  // owns the prototypes

  class StencilHashKey : public nsURIHashKey {
   public:
    explicit StencilHashKey(const nsIURI* aKey) : nsURIHashKey(aKey) {}
    StencilHashKey(StencilHashKey&&) = default;

    RefPtr<JS::Stencil> mStencil;
  };

  nsTHashtable<StencilHashKey> mStencilTable;

  // URIs already written to the startup cache, to prevent double-caching.
  nsTHashtable<nsURIHashKey> mStartupCacheURITable;

  nsInterfaceHashtable<nsURIHashKey, nsIStorageStream> mOutputStreamTable;
  nsInterfaceHashtable<nsURIHashKey, nsIObjectInputStream> mInputStreamTable;

  // Bootstrap caching service
  nsresult BeginCaching(nsIURI* aDocumentURI);
};

#endif  // nsXULPrototypeCache_h__
