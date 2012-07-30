/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULPrototypeCache_h__
#define nsXULPrototypeCache_h__

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsXBLDocumentInfo.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsURIHashKey.h"
#include "nsXULPrototypeDocument.h"
#include "nsIInputStream.h"
#include "nsIStorageStream.h"

#include "jspubtd.h"

#include "mozilla/scache/StartupCache.h"

using namespace mozilla::scache;

class nsCSSStyleSheet;

struct CacheScriptEntry
{
    JSScript*   mScriptObject; // the script object.
};

/**
 * The XUL prototype cache can be used to store and retrieve shared data for
 * XUL documents, style sheets, XBL, and scripts.
 *
 * The cache has two levels:
 *  1. In-memory hashtables
 *  2. The on-disk cache file.
 */
class nsXULPrototypeCache : public nsIObserver
{
public:
    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    bool IsCached(nsIURI* aURI) {
        return GetPrototype(aURI) != nullptr;
    }
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

    JSScript* GetScript(nsIURI* aURI);
    nsresult PutScript(nsIURI* aURI, JSScript* aScriptObject);

    nsXBLDocumentInfo* GetXBLDocumentInfo(nsIURI* aURL) {
        return mXBLDocTable.GetWeak(aURL);
    }
    nsresult PutXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo);

    /**
     * Get a style sheet by URI. If the style sheet is not in the cache,
     * returns nullptr.
     */
    nsCSSStyleSheet* GetStyleSheet(nsIURI* aURI) {
        return mStyleSheetTable.GetWeak(aURI);
    }

    /**
     * Store a style sheet in the cache. The key, style sheet's URI is obtained
     * from the style sheet itself.
     */
    nsresult PutStyleSheet(nsCSSStyleSheet* aStyleSheet);

    /**
     * Remove a XUL document from the set of loading documents.
     */
    void RemoveFromCacheSet(nsIURI* aDocumentURI);

    /**
     * Write the XUL prototype document to a cache file. The proto must be
     * fully loaded.
     */
    nsresult WritePrototype(nsXULPrototypeDocument* aPrototypeDocument);

    /**
     * This interface allows partial reads and writes from the buffers in the
     * startupCache.
     */
    nsresult GetInputStream(nsIURI* aURI, nsIObjectInputStream** objectInput);
    nsresult FinishInputStream(nsIURI* aURI);
    nsresult GetOutputStream(nsIURI* aURI, nsIObjectOutputStream** objectOutput);
    nsresult FinishOutputStream(nsIURI* aURI);
    nsresult HasData(nsIURI* aURI, bool* exists);

    static StartupCache* GetStartupCache();

    static nsXULPrototypeCache* GetInstance();

    static void ReleaseGlobals()
    {
        NS_IF_RELEASE(sInstance);
    }

    void MarkInCCGeneration(PRUint32 aGeneration);
protected:
    friend nsresult
    NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULPrototypeCache();
    virtual ~nsXULPrototypeCache();

    static nsXULPrototypeCache* sInstance;

    void FlushScripts();
    void FlushSkinFiles();

    nsRefPtrHashtable<nsURIHashKey,nsXULPrototypeDocument>  mPrototypeTable; // owns the prototypes
    nsRefPtrHashtable<nsURIHashKey,nsCSSStyleSheet>        mStyleSheetTable;
    nsDataHashtable<nsURIHashKey,CacheScriptEntry>         mScriptTable;
    nsRefPtrHashtable<nsURIHashKey,nsXBLDocumentInfo>  mXBLDocTable;

    ///////////////////////////////////////////////////////////////////////////
    // StartupCache
    // this is really a hash set, with a dummy data parameter
    nsDataHashtable<nsURIHashKey,PRUint32> mCacheURITable;

    static StartupCache* gStartupCache;
    nsInterfaceHashtable<nsURIHashKey, nsIStorageStream> mOutputStreamTable;
    nsInterfaceHashtable<nsURIHashKey, nsIObjectInputStream> mInputStreamTable;
 
    // Bootstrap caching service
    nsresult BeginCaching(nsIURI* aDocumentURI);
};

#endif // nsXULPrototypeCache_h__
