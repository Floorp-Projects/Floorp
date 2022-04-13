/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULPrototypeCache.h"

#include "plstr.h"
#include "nsXULPrototypeDocument.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIObserverService.h"
#include "nsIStorageStream.h"

#include "nsAppDirectoryServiceDefs.h"

#include "js/experimental/JSStencil.h"
#include "js/TracingAPI.h"

#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_nglayout.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/RefPtr.h"
#include "mozilla/intl/LocaleService.h"

using namespace mozilla;
using namespace mozilla::scache;
using mozilla::intl::LocaleService;

static const char kXULCacheInfoKey[] = "nsXULPrototypeCache.startupCache";
#define CACHE_PREFIX(aCompilationTarget) "xulcache/" aCompilationTarget

static void DisableXULCacheChangedCallback(const char* aPref, void* aClosure) {
  if (nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance()) {
    if (!cache->IsEnabled()) {
      // AbortCaching() calls Flush() for us.
      cache->AbortCaching();
    }
  }
}

//----------------------------------------------------------------------

nsXULPrototypeCache* nsXULPrototypeCache::sInstance = nullptr;

nsXULPrototypeCache::nsXULPrototypeCache() = default;

NS_IMPL_ISUPPORTS(nsXULPrototypeCache, nsIObserver)

/* static */
nsXULPrototypeCache* nsXULPrototypeCache::GetInstance() {
  if (!sInstance) {
    NS_ADDREF(sInstance = new nsXULPrototypeCache());

    Preferences::RegisterCallback(
        DisableXULCacheChangedCallback,
        nsDependentCString(
            StaticPrefs::GetPrefName_nglayout_debug_disable_xul_cache()));

    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (obsSvc) {
      nsXULPrototypeCache* p = sInstance;
      obsSvc->AddObserver(p, "chrome-flush-caches", false);
      obsSvc->AddObserver(p, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
      obsSvc->AddObserver(p, "startupcache-invalidate", false);
    }
  }
  return sInstance;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsXULPrototypeCache::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!strcmp(aTopic, "chrome-flush-caches") ||
      !strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Flush();
  } else if (!strcmp(aTopic, "startupcache-invalidate")) {
    AbortCaching();
  } else {
    NS_WARNING("Unexpected observer topic.");
  }
  return NS_OK;
}

nsXULPrototypeDocument* nsXULPrototypeCache::GetPrototype(nsIURI* aURI) {
  if (!aURI) return nullptr;

  nsCOMPtr<nsIURI> uriWithoutRef;
  NS_GetURIWithoutRef(aURI, getter_AddRefs(uriWithoutRef));

  nsXULPrototypeDocument* protoDoc = mPrototypeTable.GetWeak(uriWithoutRef);
  if (protoDoc) {
    return protoDoc;
  }

  nsresult rv = BeginCaching(aURI);
  if (NS_FAILED(rv)) return nullptr;

  // No prototype in XUL memory cache. Spin up the cache Service.
  nsCOMPtr<nsIObjectInputStream> ois;
  rv = GetPrototypeInputStream(aURI, getter_AddRefs(ois));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  RefPtr<nsXULPrototypeDocument> newProto;
  rv = NS_NewXULPrototypeDocument(getter_AddRefs(newProto));
  if (NS_FAILED(rv)) return nullptr;

  rv = newProto->Read(ois);
  if (NS_SUCCEEDED(rv)) {
    rv = PutPrototype(newProto);
  } else {
    newProto = nullptr;
  }

  mInputStreamTable.Remove(aURI);
  return newProto;
}

nsresult nsXULPrototypeCache::PutPrototype(nsXULPrototypeDocument* aDocument) {
  if (!aDocument->GetURI()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> uri;
  NS_GetURIWithoutRef(aDocument->GetURI(), getter_AddRefs(uri));

  // Put() releases any old value
  mPrototypeTable.InsertOrUpdate(uri, RefPtr{aDocument});

  return NS_OK;
}

JS::Stencil* nsXULPrototypeCache::GetStencil(nsIURI* aURI) {
  if (auto* entry = mStencilTable.GetEntry(aURI)) {
    return entry->mStencil;
  }
  return nullptr;
}

nsresult nsXULPrototypeCache::PutStencil(nsIURI* aURI, JS::Stencil* aStencil) {
  MOZ_ASSERT(aStencil, "Need a non-NULL stencil");

#ifdef DEBUG_BUG_392650
  if (mStencilTable.Get(aURI)) {
    nsAutoCString scriptName;
    aURI->GetSpec(scriptName);
    nsAutoCString message("Loaded script ");
    message += scriptName;
    message += " twice (bug 392650)";
    NS_WARNING(message.get());
  }
#endif

  mStencilTable.PutEntry(aURI)->mStencil = aStencil;

  return NS_OK;
}

void nsXULPrototypeCache::Flush() {
  mPrototypeTable.Clear();
  mStencilTable.Clear();
}

bool nsXULPrototypeCache::IsEnabled() {
  return !StaticPrefs::nglayout_debug_disable_xul_cache();
}

void nsXULPrototypeCache::AbortCaching() {
  // Flush the XUL cache for good measure, in case we cached a bogus/downrev
  // script, somehow.
  Flush();

  // Clear the cache set
  mStartupCacheURITable.Clear();
}

nsresult nsXULPrototypeCache::WritePrototype(
    nsXULPrototypeDocument* aPrototypeDocument) {
  nsresult rv = NS_OK, rv2 = NS_OK;

  if (!StartupCache::GetSingleton()) return NS_OK;

  nsCOMPtr<nsIURI> protoURI = aPrototypeDocument->GetURI();

  nsCOMPtr<nsIObjectOutputStream> oos;
  rv = GetPrototypeOutputStream(protoURI, getter_AddRefs(oos));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aPrototypeDocument->Write(oos);
  NS_ENSURE_SUCCESS(rv, rv);
  FinishPrototypeOutputStream(protoURI);
  return NS_FAILED(rv) ? rv : rv2;
}

static nsresult PathifyURIForType(nsXULPrototypeCache::CacheType cacheType,
                                  nsIURI* in, nsACString& out) {
  switch (cacheType) {
    case nsXULPrototypeCache::CacheType::Prototype:
      return PathifyURI(CACHE_PREFIX("proto"), in, out);
    case nsXULPrototypeCache::CacheType::Script:
      return PathifyURI(CACHE_PREFIX("script"), in, out);
  }
  MOZ_ASSERT_UNREACHABLE("unknown cache type?");
  return NS_ERROR_UNEXPECTED;
}

nsresult nsXULPrototypeCache::GetInputStream(CacheType cacheType, nsIURI* uri,
                                             nsIObjectInputStream** stream) {
  nsAutoCString spec;
  nsresult rv = PathifyURIForType(cacheType, uri, spec);
  if (NS_FAILED(rv)) return NS_ERROR_NOT_AVAILABLE;

  const char* buf;
  uint32_t len;
  nsCOMPtr<nsIObjectInputStream> ois;
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) return NS_ERROR_NOT_AVAILABLE;

  rv = sc->GetBuffer(spec.get(), &buf, &len);
  if (NS_FAILED(rv)) return NS_ERROR_NOT_AVAILABLE;

  rv = NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(ois));
  NS_ENSURE_SUCCESS(rv, rv);

  mInputStreamTable.InsertOrUpdate(uri, ois);

  ois.forget(stream);
  return NS_OK;
}

nsresult nsXULPrototypeCache::FinishInputStream(nsIURI* uri) {
  mInputStreamTable.Remove(uri);
  return NS_OK;
}

nsresult nsXULPrototypeCache::GetOutputStream(nsIURI* uri,
                                              nsIObjectOutputStream** stream) {
  nsresult rv;
  nsCOMPtr<nsIObjectOutputStream> objectOutput;
  nsCOMPtr<nsIStorageStream> storageStream;
  bool found = mOutputStreamTable.Get(uri, getter_AddRefs(storageStream));
  if (found) {
    // Setting an output stream here causes crashes on Windows. The previous
    // version of this code always returned NS_ERROR_OUT_OF_MEMORY here,
    // because it used a mistyped contract ID to create its object stream.
    return NS_ERROR_NOT_IMPLEMENTED;
#if 0
        nsCOMPtr<nsIOutputStream> outputStream
            = do_QueryInterface(storageStream);
        objectOutput = NS_NewObjectOutputStream(outputStream);
#endif
  } else {
    rv = NewObjectOutputWrappedStorageStream(
        getter_AddRefs(objectOutput), getter_AddRefs(storageStream), false);
    NS_ENSURE_SUCCESS(rv, rv);
    mOutputStreamTable.InsertOrUpdate(uri, storageStream);
  }
  objectOutput.forget(stream);
  return NS_OK;
}

nsresult nsXULPrototypeCache::FinishOutputStream(CacheType cacheType,
                                                 nsIURI* uri) {
  nsresult rv;
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIStorageStream> storageStream;
  bool found = mOutputStreamTable.Get(uri, getter_AddRefs(storageStream));
  if (!found) return NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIOutputStream> outputStream = do_QueryInterface(storageStream);
  outputStream->Close();

  UniquePtr<char[]> buf;
  uint32_t len;
  rv = NewBufferFromStorageStream(storageStream, &buf, &len);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mStartupCacheURITable.GetEntry(uri)) {
    nsAutoCString spec;
    rv = PathifyURIForType(cacheType, uri, spec);
    if (NS_FAILED(rv)) return NS_ERROR_NOT_AVAILABLE;
    rv = sc->PutBuffer(spec.get(), std::move(buf), len);
    if (NS_SUCCEEDED(rv)) {
      mOutputStreamTable.Remove(uri);
      mStartupCacheURITable.PutEntry(uri);
    }
  }

  return rv;
}

// We have data if we're in the middle of writing it or we already
// have it in the cache.
nsresult nsXULPrototypeCache::HasData(CacheType cacheType, nsIURI* uri,
                                      bool* exists) {
  if (mOutputStreamTable.Get(uri, nullptr)) {
    *exists = true;
    return NS_OK;
  }
  nsAutoCString spec;
  nsresult rv = PathifyURIForType(cacheType, uri, spec);
  if (NS_FAILED(rv)) {
    *exists = false;
    return NS_OK;
  }
  UniquePtr<char[]> buf;
  StartupCache* sc = StartupCache::GetSingleton();
  if (sc) {
    *exists = sc->HasEntry(spec.get());
  } else {
    *exists = false;
  }
  return NS_OK;
}

nsresult nsXULPrototypeCache::BeginCaching(nsIURI* aURI) {
  nsresult rv, tmp;

  nsAutoCString path;
  aURI->GetPathQueryRef(path);
  if (!(StringEndsWith(path, ".xul"_ns) || StringEndsWith(path, ".xhtml"_ns))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  StartupCache* startupCache = StartupCache::GetSingleton();
  if (!startupCache) return NS_ERROR_FAILURE;

  if (StaticPrefs::nglayout_debug_disable_xul_cache()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the chrome directory to validate against the one stored in the
  // cache file, or to store there if we're generating a new file.
  nsCOMPtr<nsIFile> chromeDir;
  rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR, getter_AddRefs(chromeDir));
  if (NS_FAILED(rv)) return rv;
  nsAutoCString chromePath;
  rv = chromeDir->GetPersistentDescriptor(chromePath);
  if (NS_FAILED(rv)) return rv;

  // XXXbe we assume the first package's locale is the same as the locale of
  // all subsequent packages of cached chrome URIs....
  nsAutoCString package;
  rv = aURI->GetHost(package);
  if (NS_FAILED(rv)) return rv;
  nsAutoCString locale;
  LocaleService::GetInstance()->GetAppLocaleAsBCP47(locale);

  nsAutoCString fileChromePath, fileLocale;

  const char* buf = nullptr;
  uint32_t len, amtRead;
  nsCOMPtr<nsIObjectInputStream> objectInput;

  rv = startupCache->GetBuffer(kXULCacheInfoKey, &buf, &len);
  if (NS_SUCCEEDED(rv))
    rv = NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(objectInput));

  if (NS_SUCCEEDED(rv)) {
    rv = objectInput->ReadCString(fileLocale);
    tmp = objectInput->ReadCString(fileChromePath);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    if (NS_FAILED(rv) ||
        (!fileChromePath.Equals(chromePath) || !fileLocale.Equals(locale))) {
      // Our cache won't be valid in this case, we'll need to rewrite.
      // XXX This blows away work that other consumers (like
      // mozJSComponentLoader) have done, need more fine-grained control.
      startupCache->InvalidateCache();
      mStartupCacheURITable.Clear();
      rv = NS_ERROR_UNEXPECTED;
    }
  } else if (rv != NS_ERROR_NOT_AVAILABLE)
    // NS_ERROR_NOT_AVAILABLE is normal, usually if there's no cachefile.
    return rv;

  if (NS_FAILED(rv)) {
    // Either the cache entry was invalid or it didn't exist, so write it now.
    nsCOMPtr<nsIObjectOutputStream> objectOutput;
    nsCOMPtr<nsIInputStream> inputStream;
    nsCOMPtr<nsIStorageStream> storageStream;
    rv = NewObjectOutputWrappedStorageStream(
        getter_AddRefs(objectOutput), getter_AddRefs(storageStream), false);
    if (NS_SUCCEEDED(rv)) {
      rv = objectOutput->WriteStringZ(locale.get());
      tmp = objectOutput->WriteStringZ(chromePath.get());
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
      tmp = objectOutput->Close();
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
      tmp = storageStream->NewInputStream(0, getter_AddRefs(inputStream));
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
    }

    if (NS_SUCCEEDED(rv)) {
      uint64_t len64;
      rv = inputStream->Available(&len64);
      if (NS_SUCCEEDED(rv)) {
        if (len64 <= UINT32_MAX)
          len = (uint32_t)len64;
        else
          rv = NS_ERROR_FILE_TOO_BIG;
      }
    }

    if (NS_SUCCEEDED(rv)) {
      auto putBuf = MakeUnique<char[]>(len);
      rv = inputStream->Read(putBuf.get(), len, &amtRead);
      if (NS_SUCCEEDED(rv) && len == amtRead)
        rv = startupCache->PutBuffer(kXULCacheInfoKey, std::move(putBuf), len);
      else {
        rv = NS_ERROR_UNEXPECTED;
      }
    }

    // Failed again, just bail.
    if (NS_FAILED(rv)) {
      startupCache->InvalidateCache();
      mStartupCacheURITable.Clear();
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

void nsXULPrototypeCache::MarkInCCGeneration(uint32_t aGeneration) {
  for (const auto& prototype : mPrototypeTable.Values()) {
    prototype->MarkInCCGeneration(aGeneration);
  }
}

MOZ_DEFINE_MALLOC_SIZE_OF(CacheMallocSizeOf)

static void ReportSize(const nsCString& aPath, size_t aAmount,
                       const nsCString& aDescription,
                       nsIHandleReportCallback* aHandleReport,
                       nsISupports* aData) {
  nsAutoCString path("explicit/xul-prototype-cache/");
  path += aPath;
  aHandleReport->Callback(""_ns, path, nsIMemoryReporter::KIND_HEAP,
                          nsIMemoryReporter::UNITS_BYTES, aAmount, aDescription,
                          aData);
}

/* static */
void nsXULPrototypeCache::CollectMemoryReports(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData) {
  if (!sInstance) {
    return;
  }

  MallocSizeOf mallocSizeOf = CacheMallocSizeOf;
  size_t other = mallocSizeOf(sInstance);

#define REPORT_SIZE(_path, _amount, _desc) \
  ReportSize(_path, _amount, nsLiteralCString(_desc), aHandleReport, aData)

  other += sInstance->mPrototypeTable.ShallowSizeOfExcludingThis(mallocSizeOf);
  // TODO Report content in mPrototypeTable?

  other += sInstance->mStencilTable.ShallowSizeOfExcludingThis(mallocSizeOf);
  // TODO Report content inside mStencilTable?

  other +=
      sInstance->mStartupCacheURITable.ShallowSizeOfExcludingThis(mallocSizeOf);

  other +=
      sInstance->mOutputStreamTable.ShallowSizeOfExcludingThis(mallocSizeOf);
  other +=
      sInstance->mInputStreamTable.ShallowSizeOfExcludingThis(mallocSizeOf);

  REPORT_SIZE("other"_ns, other,
              "Memory used by "
              "the instance and tables of the XUL prototype cache.");

#undef REPORT_SIZE
}
