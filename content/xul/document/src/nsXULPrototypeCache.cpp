/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*



 */

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsICSSStyleSheet.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIURI.h"
#include "nsHashtable.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "nsIDocument.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsXULDocument.h"

class nsXULPrototypeCache : public nsIXULPrototypeCache
{
public:
    // nsISupports
    NS_DECL_ISUPPORTS

    NS_IMETHOD GetPrototype(nsIURI* aURI, nsIXULPrototypeDocument** _result);
    NS_IMETHOD PutPrototype(nsIXULPrototypeDocument* aDocument);
    NS_IMETHOD FlushPrototypes();

    NS_IMETHOD GetStyleSheet(nsIURI* aURI, nsICSSStyleSheet** _result);
    NS_IMETHOD PutStyleSheet(nsICSSStyleSheet* aStyleSheet);
    NS_IMETHOD FlushStyleSheets();

    NS_IMETHOD GetScript(nsIURI* aURI, void** aScriptObject);
    NS_IMETHOD PutScript(nsIURI* aURI, void* aScriptObject);
    NS_IMETHOD FlushScripts();

    NS_IMETHOD GetXBLDocumentInfo(const nsCString& aString, nsIXBLDocumentInfo** _result);
    NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo);
    NS_IMETHOD FlushXBLInformation();

    NS_IMETHOD Flush();

    NS_IMETHOD GetEnabled(PRBool* aIsEnabled);

    NS_IMETHOD AbortFastLoads();

protected:
    friend NS_IMETHODIMP
    NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULPrototypeCache();
    virtual ~nsXULPrototypeCache();

    nsSupportsHashtable mPrototypeTable;
    nsSupportsHashtable mStyleSheetTable;
    nsHashtable         mScriptTable;
    nsSupportsHashtable mXBLDocTable;
    
    class nsIURIKey : public nsHashKey {
    protected:
        nsCOMPtr<nsIURI> mKey;
  
    public:
        nsIURIKey(nsIURI* key) : mKey(key) {}
        ~nsIURIKey(void) {}
  
        PRUint32 HashCode(void) const {
            nsXPIDLCString spec;
            mKey->GetSpec(getter_Copies(spec));
            return (PRUint32) PL_HashString(spec);
        }

        PRBool Equals(const nsHashKey *aKey) const {
            PRBool eq;
            mKey->Equals( ((nsIURIKey*) aKey)->mKey, &eq );
            return eq;
        }

        nsHashKey *Clone(void) const {
            return new nsIURIKey(mKey);
        }
    };
};

static PRBool gDisableXULCache = PR_FALSE; // enabled by default
static const char kDisableXULCachePref[] = "nglayout.debug.disable_xul_cache";

//----------------------------------------------------------------------

PR_STATIC_CALLBACK(int)
DisableXULCacheChangedCallback(const char* aPref, void* aClosure)
{
    nsresult rv;

    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (prefs)
        prefs->GetBoolPref(kDisableXULCachePref, &gDisableXULCache);

    // Flush the cache, regardless
    static NS_DEFINE_CID(kXULPrototypeCacheCID, NS_XULPROTOTYPECACHE_CID);
    nsCOMPtr<nsIXULPrototypeCache> cache =
        do_GetService(kXULPrototypeCacheCID, &rv);

    if (cache)
        cache->Flush();

    return 0;
}

//----------------------------------------------------------------------


nsXULPrototypeCache::nsXULPrototypeCache()
{
    NS_INIT_REFCNT();
}


nsXULPrototypeCache::~nsXULPrototypeCache()
{
    FlushScripts();
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsXULPrototypeCache, nsIXULPrototypeCache);


NS_IMETHODIMP
NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(! aOuter, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsXULPrototypeCache* result = new nsXULPrototypeCache();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
        // XXX Ignore return values.
        prefs->GetBoolPref(kDisableXULCachePref, &gDisableXULCache);
        prefs->RegisterCallback(kDisableXULCachePref, DisableXULCacheChangedCallback, nsnull);
    }

    NS_ADDREF(result);
    rv = result->QueryInterface(aIID, aResult);
    NS_RELEASE(result);

    return rv;
}


//----------------------------------------------------------------------


NS_IMETHODIMP
nsXULPrototypeCache::GetPrototype(nsIURI* aURI, nsIXULPrototypeDocument** _result)
{
    nsIURIKey key(aURI);
    *_result = NS_STATIC_CAST(nsIXULPrototypeDocument*, mPrototypeTable.Get(&key));
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::PutPrototype(nsIXULPrototypeDocument* aDocument)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = aDocument->GetURI(getter_AddRefs(uri));

    nsIURIKey key(uri);

    // Put() w/o  a third parameter with a destination for the
    // replaced value releases it 
    mPrototypeTable.Put(&key, aDocument);

    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::FlushPrototypes()
{
    mPrototypeTable.Reset();

    // Clear the script cache, as it refers to prototype-owned mJSObjects.
    mScriptTable.Reset();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::GetStyleSheet(nsIURI* aURI, nsICSSStyleSheet** _result)
{
    nsIURIKey key(aURI);
    *_result = NS_STATIC_CAST(nsICSSStyleSheet*, mStyleSheetTable.Get(&key));
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::PutStyleSheet(nsICSSStyleSheet* aStyleSheet)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = aStyleSheet->GetURL(*getter_AddRefs(uri));

    nsIURIKey key(uri);
    mStyleSheetTable.Put(&key, aStyleSheet);
    
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::FlushStyleSheets()
{
    mStyleSheetTable.Reset();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::GetScript(nsIURI* aURI, void** aScriptObject)
{
    nsIURIKey key(aURI);
    *aScriptObject = mScriptTable.Get(&key);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::PutScript(nsIURI* aURI, void* aScriptObject)
{
    nsIURIKey key(aURI);
    mScriptTable.Put(&key, aScriptObject);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::FlushScripts()
{
    mScriptTable.Reset();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::GetXBLDocumentInfo(const nsCString& aURL, nsIXBLDocumentInfo** aResult)
{
    nsCStringKey key(aURL);
    *aResult = NS_STATIC_CAST(nsIXBLDocumentInfo*, mXBLDocTable.Get(&key)); // Addref happens here.
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)
{
    nsCOMPtr<nsIDocument> doc;
    aDocumentInfo->GetDocument(getter_AddRefs(doc));

    nsCOMPtr<nsIURI> uri;
    doc->GetDocumentURL(getter_AddRefs(uri));

    nsXPIDLCString str;
    uri->GetSpec(getter_Copies(str));
    
    nsCStringKey key((const char*)str);
    nsCOMPtr<nsIXBLDocumentInfo> info = getter_AddRefs(NS_STATIC_CAST(nsIXBLDocumentInfo*, mXBLDocTable.Get(&key)));
    if (!info)
        mXBLDocTable.Put(&key, aDocumentInfo);

    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::FlushXBLInformation()
{
    mXBLDocTable.Reset();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::Flush()
{
    FlushPrototypes();
    FlushStyleSheets();
    FlushXBLInformation();
    FlushScripts();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::GetEnabled(PRBool* aIsEnabled)
{
    *aIsEnabled = !gDisableXULCache;
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::AbortFastLoads()
{
    return nsXULDocument::AbortFastLoads();
}
