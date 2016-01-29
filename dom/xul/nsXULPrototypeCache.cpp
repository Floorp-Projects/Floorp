/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULPrototypeCache.h"

#include "plstr.h"
#include "nsXULPrototypeDocument.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

#include "nsIChromeRegistry.h"
#include "nsIFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIObserverService.h"
#include "nsIStringStream.h"
#include "nsIStorageStream.h"

#include "nsAppDirectoryServiceDefs.h"

#include "js/TracingAPI.h"

#include "mozilla/CSSStyleSheet.h"
#include "mozilla/Preferences.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::scache;

static bool gDisableXULCache = false; // enabled by default
static const char kDisableXULCachePref[] = "nglayout.debug.disable_xul_cache";
static const char kXULCacheInfoKey[] = "nsXULPrototypeCache.startupCache";
static const char kXULCachePrefix[] = "xulcache";

//----------------------------------------------------------------------

static void
UpdategDisableXULCache()
{
    // Get the value of "nglayout.debug.disable_xul_cache" preference
    gDisableXULCache =
        Preferences::GetBool(kDisableXULCachePref, gDisableXULCache);

    // Sets the flag if the XUL cache is disabled
    if (gDisableXULCache) {
        Telemetry::Accumulate(Telemetry::XUL_CACHE_DISABLED, true);
    }

}

static void
DisableXULCacheChangedCallback(const char* aPref, void* aClosure)
{
    bool wasEnabled = !gDisableXULCache;
    UpdategDisableXULCache();

    if (wasEnabled && gDisableXULCache) {
        nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
        if (cache) {
            // AbortCaching() calls Flush() for us.
            cache->AbortCaching();
        }
    }
}

//----------------------------------------------------------------------

nsXULPrototypeCache*  nsXULPrototypeCache::sInstance = nullptr;


nsXULPrototypeCache::nsXULPrototypeCache()
{
}


nsXULPrototypeCache::~nsXULPrototypeCache()
{
    FlushScripts();
}


NS_IMPL_ISUPPORTS(nsXULPrototypeCache, nsIObserver)

/* static */ nsXULPrototypeCache*
nsXULPrototypeCache::GetInstance()
{
    if (!sInstance) {
        NS_ADDREF(sInstance = new nsXULPrototypeCache());

        UpdategDisableXULCache();

        Preferences::RegisterCallback(DisableXULCacheChangedCallback,
                                      kDisableXULCachePref);

        nsCOMPtr<nsIObserverService> obsSvc =
            mozilla::services::GetObserverService();
        if (obsSvc) {
            nsXULPrototypeCache *p = sInstance;
            obsSvc->AddObserver(p, "chrome-flush-skin-caches", false);
            obsSvc->AddObserver(p, "chrome-flush-caches", false);
            obsSvc->AddObserver(p, "startupcache-invalidate", false);
        }

    }
    return sInstance;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsXULPrototypeCache::Observe(nsISupports* aSubject,
                             const char *aTopic,
                             const char16_t *aData)
{
    if (!strcmp(aTopic, "chrome-flush-skin-caches")) {
        FlushSkinFiles();
    }
    else if (!strcmp(aTopic, "chrome-flush-caches")) {
        Flush();
    }
    else if (!strcmp(aTopic, "startupcache-invalidate")) {
        AbortCaching();
    }
    else {
        NS_WARNING("Unexpected observer topic.");
    }
    return NS_OK;
}

nsXULPrototypeDocument*
nsXULPrototypeCache::GetPrototype(nsIURI* aURI)
{
    if (!aURI)
        return nullptr;

    nsCOMPtr<nsIURI> uriWithoutRef;
    aURI->CloneIgnoringRef(getter_AddRefs(uriWithoutRef));

    nsXULPrototypeDocument* protoDoc = mPrototypeTable.GetWeak(uriWithoutRef);
    if (protoDoc)
        return protoDoc;

    nsresult rv = BeginCaching(aURI);
    if (NS_FAILED(rv))
        return nullptr;

    // No prototype in XUL memory cache. Spin up the cache Service.
    nsCOMPtr<nsIObjectInputStream> ois;
    rv = GetInputStream(aURI, getter_AddRefs(ois));
    if (NS_FAILED(rv))
        return nullptr;

    RefPtr<nsXULPrototypeDocument> newProto;
    rv = NS_NewXULPrototypeDocument(getter_AddRefs(newProto));
    if (NS_FAILED(rv))
        return nullptr;

    rv = newProto->Read(ois);
    if (NS_SUCCEEDED(rv)) {
        rv = PutPrototype(newProto);
    } else {
        newProto = nullptr;
    }

    mInputStreamTable.Remove(aURI);
    return newProto;
}

nsresult
nsXULPrototypeCache::PutPrototype(nsXULPrototypeDocument* aDocument)
{
    if (!aDocument->GetURI()) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> uri;
    aDocument->GetURI()->CloneIgnoringRef(getter_AddRefs(uri));

    // Put() releases any old value and addrefs the new one
    mPrototypeTable.Put(uri, aDocument);

    return NS_OK;
}

nsresult
nsXULPrototypeCache::PutStyleSheet(CSSStyleSheet* aStyleSheet)
{
    nsIURI* uri = aStyleSheet->GetSheetURI();

    mStyleSheetTable.Put(uri, aStyleSheet);

    return NS_OK;
}


JSScript*
nsXULPrototypeCache::GetScript(nsIURI* aURI)
{
    return mScriptTable.Get(aURI);
}

nsresult
nsXULPrototypeCache::PutScript(nsIURI* aURI,
                               JS::Handle<JSScript*> aScriptObject)
{
    MOZ_ASSERT(aScriptObject, "Need a non-NULL script");

#ifdef DEBUG
    if (mScriptTable.Get(aURI)) {
        nsAutoCString scriptName;
        aURI->GetSpec(scriptName);
        nsAutoCString message("Loaded script ");
        message += scriptName;
        message += " twice (bug 392650)";
        NS_WARNING(message.get());
    }
#endif

    mScriptTable.Put(aURI, aScriptObject);

    return NS_OK;
}

nsresult
nsXULPrototypeCache::PutXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo)
{
    nsIURI* uri = aDocumentInfo->DocumentURI();

    RefPtr<nsXBLDocumentInfo> info;
    mXBLDocTable.Get(uri, getter_AddRefs(info));
    if (!info) {
        mXBLDocTable.Put(uri, aDocumentInfo);
    }
    return NS_OK;
}

void
nsXULPrototypeCache::FlushSkinFiles()
{
  // Flush out skin XBL files from the cache.
  for (auto iter = mXBLDocTable.Iter(); !iter.Done(); iter.Next()) {
    nsAutoCString str;
    iter.Key()->GetPath(str);
    if (strncmp(str.get(), "/skin", 5) == 0) {
      iter.Remove();
    }
  }

  // Now flush out our skin stylesheets from the cache.
  for (auto iter = mStyleSheetTable.Iter(); !iter.Done(); iter.Next()) {
    nsAutoCString str;
    iter.Data()->GetSheetURI()->GetPath(str);
    if (strncmp(str.get(), "/skin", 5) == 0) {
      iter.Remove();
    }
  }

  // Iterate over all the remaining XBL and make sure cached
  // scoped skin stylesheets are flushed and refetched by the
  // prototype bindings.
  for (auto iter = mXBLDocTable.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->FlushSkinStylesheets();
  }
}

void
nsXULPrototypeCache::FlushScripts()
{
    mScriptTable.Clear();
}

void
nsXULPrototypeCache::Flush()
{
    mPrototypeTable.Clear();
    mScriptTable.Clear();
    mStyleSheetTable.Clear();
    mXBLDocTable.Clear();
}


bool
nsXULPrototypeCache::IsEnabled()
{
    return !gDisableXULCache;
}

void
nsXULPrototypeCache::AbortCaching()
{
#ifdef DEBUG_brendan
    NS_BREAK();
#endif

    // Flush the XUL cache for good measure, in case we cached a bogus/downrev
    // script, somehow.
    Flush();

    // Clear the cache set
    mStartupCacheURITable.Clear();
}


nsresult
nsXULPrototypeCache::WritePrototype(nsXULPrototypeDocument* aPrototypeDocument)
{
    nsresult rv = NS_OK, rv2 = NS_OK;

    if (!StartupCache::GetSingleton())
        return NS_OK;

    nsCOMPtr<nsIURI> protoURI = aPrototypeDocument->GetURI();

    nsCOMPtr<nsIObjectOutputStream> oos;
    rv = GetOutputStream(protoURI, getter_AddRefs(oos));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aPrototypeDocument->Write(oos);
    NS_ENSURE_SUCCESS(rv, rv);
    FinishOutputStream(protoURI);
    return NS_FAILED(rv) ? rv : rv2;
}

nsresult
nsXULPrototypeCache::GetInputStream(nsIURI* uri, nsIObjectInputStream** stream)
{
    nsAutoCString spec(kXULCachePrefix);
    nsresult rv = PathifyURI(uri, spec);
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;

    nsAutoArrayPtr<char> buf;
    uint32_t len;
    nsCOMPtr<nsIObjectInputStream> ois;
    StartupCache* sc = StartupCache::GetSingleton();
    if (!sc)
        return NS_ERROR_NOT_AVAILABLE;

    rv = sc->GetBuffer(spec.get(), getter_Transfers(buf), &len);
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;

    rv = NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(ois));
    NS_ENSURE_SUCCESS(rv, rv);
    buf.forget();

    mInputStreamTable.Put(uri, ois);

    ois.forget(stream);
    return NS_OK;
}

nsresult
nsXULPrototypeCache::FinishInputStream(nsIURI* uri) {
    mInputStreamTable.Remove(uri);
    return NS_OK;
}

nsresult
nsXULPrototypeCache::GetOutputStream(nsIURI* uri, nsIObjectOutputStream** stream)
{
    nsresult rv;
    nsCOMPtr<nsIObjectOutputStream> objectOutput;
    nsCOMPtr<nsIStorageStream> storageStream;
    bool found = mOutputStreamTable.Get(uri, getter_AddRefs(storageStream));
    if (found) {
        objectOutput = do_CreateInstance("mozilla.org/binaryoutputstream;1");
        if (!objectOutput) return NS_ERROR_OUT_OF_MEMORY;
        nsCOMPtr<nsIOutputStream> outputStream
            = do_QueryInterface(storageStream);
        objectOutput->SetOutputStream(outputStream);
    } else {
        rv = NewObjectOutputWrappedStorageStream(getter_AddRefs(objectOutput),
                                                 getter_AddRefs(storageStream),
                                                 false);
        NS_ENSURE_SUCCESS(rv, rv);
        mOutputStreamTable.Put(uri, storageStream);
    }
    objectOutput.forget(stream);
    return NS_OK;
}

nsresult
nsXULPrototypeCache::FinishOutputStream(nsIURI* uri)
{
    nsresult rv;
    StartupCache* sc = StartupCache::GetSingleton();
    if (!sc)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsIStorageStream> storageStream;
    bool found = mOutputStreamTable.Get(uri, getter_AddRefs(storageStream));
    if (!found)
        return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIOutputStream> outputStream
        = do_QueryInterface(storageStream);
    outputStream->Close();

    nsAutoArrayPtr<char> buf;
    uint32_t len;
    rv = NewBufferFromStorageStream(storageStream, getter_Transfers(buf),
                                    &len);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mStartupCacheURITable.GetEntry(uri)) {
        nsAutoCString spec(kXULCachePrefix);
        rv = PathifyURI(uri, spec);
        if (NS_FAILED(rv))
            return NS_ERROR_NOT_AVAILABLE;
        rv = sc->PutBuffer(spec.get(), buf, len);
        if (NS_SUCCEEDED(rv)) {
            mOutputStreamTable.Remove(uri);
            mStartupCacheURITable.PutEntry(uri);
        }
    }

    return rv;
}

// We have data if we're in the middle of writing it or we already
// have it in the cache.
nsresult
nsXULPrototypeCache::HasData(nsIURI* uri, bool* exists)
{
    if (mOutputStreamTable.Get(uri, nullptr)) {
        *exists = true;
        return NS_OK;
    }
    nsAutoCString spec(kXULCachePrefix);
    nsresult rv = PathifyURI(uri, spec);
    if (NS_FAILED(rv)) {
        *exists = false;
        return NS_OK;
    }
    nsAutoArrayPtr<char> buf;
    uint32_t len;
    StartupCache* sc = StartupCache::GetSingleton();
    if (sc)
        rv = sc->GetBuffer(spec.get(), getter_Transfers(buf), &len);
    else {
        *exists = false;
        return NS_OK;
    }
    *exists = NS_SUCCEEDED(rv);
    return NS_OK;
}

nsresult
nsXULPrototypeCache::BeginCaching(nsIURI* aURI)
{
    nsresult rv, tmp;

    nsAutoCString path;
    aURI->GetPath(path);
    if (!StringEndsWith(path, NS_LITERAL_CSTRING(".xul")))
        return NS_ERROR_NOT_AVAILABLE;

    StartupCache* startupCache = StartupCache::GetSingleton();
    if (!startupCache)
        return NS_ERROR_FAILURE;

    if (gDisableXULCache)
        return NS_ERROR_NOT_AVAILABLE;

    // Get the chrome directory to validate against the one stored in the
    // cache file, or to store there if we're generating a new file.
    nsCOMPtr<nsIFile> chromeDir;
    rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR, getter_AddRefs(chromeDir));
    if (NS_FAILED(rv))
        return rv;
    nsAutoCString chromePath;
    rv = chromeDir->GetNativePath(chromePath);
    if (NS_FAILED(rv))
        return rv;

    // XXXbe we assume the first package's locale is the same as the locale of
    // all subsequent packages of cached chrome URIs....
    nsAutoCString package;
    rv = aURI->GetHost(package);
    if (NS_FAILED(rv))
        return rv;
    nsCOMPtr<nsIXULChromeRegistry> chromeReg
        = do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
    nsAutoCString locale;
    rv = chromeReg->GetSelectedLocale(package, locale);
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString fileChromePath, fileLocale;

    nsAutoArrayPtr<char> buf;
    uint32_t len, amtRead;
    nsCOMPtr<nsIObjectInputStream> objectInput;

    rv = startupCache->GetBuffer(kXULCacheInfoKey, getter_Transfers(buf),
                                 &len);
    if (NS_SUCCEEDED(rv))
        rv = NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(objectInput));

    if (NS_SUCCEEDED(rv)) {
        buf.forget();
        rv = objectInput->ReadCString(fileLocale);
        tmp = objectInput->ReadCString(fileChromePath);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
        if (NS_FAILED(rv) ||
            (!fileChromePath.Equals(chromePath) ||
             !fileLocale.Equals(locale))) {
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
        rv = NewObjectOutputWrappedStorageStream(getter_AddRefs(objectOutput),
                                                 getter_AddRefs(storageStream),
                                                 false);
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
            buf = new char[len];
            rv = inputStream->Read(buf, len, &amtRead);
            if (NS_SUCCEEDED(rv) && len == amtRead)
                rv = startupCache->PutBuffer(kXULCacheInfoKey, buf, len);
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

void
nsXULPrototypeCache::MarkInCCGeneration(uint32_t aGeneration)
{
    for (auto iter = mXBLDocTable.Iter(); !iter.Done(); iter.Next()) {
        iter.Data()->MarkInCCGeneration(aGeneration);
    }
    for (auto iter = mPrototypeTable.Iter(); !iter.Done(); iter.Next()) {
        iter.Data()->MarkInCCGeneration(aGeneration);
    }
}

void
nsXULPrototypeCache::MarkInGC(JSTracer* aTrc)
{
    for (auto iter = mScriptTable.Iter(); !iter.Done(); iter.Next()) {
        JS::Heap<JSScript*>& script = iter.Data();
        JS::TraceEdge(aTrc, &script, "nsXULPrototypeCache script");
    }
}
