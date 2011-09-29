/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 *   Ben Goodger <ben@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *   Mark Hammond <mhammond@skippinet.com.au>
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
#include "mozilla/scache/StartupCache.h"

using namespace mozilla::scache;

class nsCSSStyleSheet;

struct CacheScriptEntry
{
    PRUint32    mScriptTypeID; // the script language ID.
    void*       mScriptObject; // the script object.
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
        return GetPrototype(aURI) != nsnull;
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

    void* GetScript(nsIURI* aURI, PRUint32* langID);
    nsresult PutScript(nsIURI* aURI, PRUint32 langID, void* aScriptObject);

    nsXBLDocumentInfo* GetXBLDocumentInfo(nsIURI* aURL) {
        return mXBLDocTable.GetWeak(aURL);
    }
    nsresult PutXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo);

    /**
     * Get a style sheet by URI. If the style sheet is not in the cache,
     * returns nsnull.
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
