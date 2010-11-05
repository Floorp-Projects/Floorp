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

#include "nsXULPrototypeCache.h"

#include "nsContentUtils.h"
#include "plstr.h"
#include "nsXULPrototypeDocument.h"
#include "nsCSSStyleSheet.h"
#include "nsIScriptRuntime.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

#include "nsIChromeRegistry.h"
#include "nsIFastLoadService.h"
#include "nsIFastLoadFileControl.h"
#include "nsIFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIObserverService.h"

#include "nsNetUtil.h"
#include "nsAppDirectoryServiceDefs.h"

#include "jsxdrapi.h"

static NS_DEFINE_CID(kXULPrototypeCacheCID, NS_XULPROTOTYPECACHE_CID);

static PRBool gDisableXULCache = PR_FALSE; // enabled by default
static const char kDisableXULCachePref[] = "nglayout.debug.disable_xul_cache";

//----------------------------------------------------------------------

static int
DisableXULCacheChangedCallback(const char* aPref, void* aClosure)
{
    gDisableXULCache =
        nsContentUtils::GetBoolPref(kDisableXULCachePref, gDisableXULCache);

    // Flush the cache, regardless
    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
    if (cache)
        cache->Flush();

    return 0;
}

//----------------------------------------------------------------------


nsIFastLoadService*   nsXULPrototypeCache::gFastLoadService = nsnull;
nsIFile*              nsXULPrototypeCache::gFastLoadFile = nsnull;
nsXULPrototypeCache*  nsXULPrototypeCache::sInstance = nsnull;


nsXULPrototypeCache::nsXULPrototypeCache()
{
}


nsXULPrototypeCache::~nsXULPrototypeCache()
{
    FlushScripts();

    NS_IF_RELEASE(gFastLoadService); // don't need ReleaseService nowadays!
    NS_IF_RELEASE(gFastLoadFile);
}


NS_IMPL_THREADSAFE_ISUPPORTS2(nsXULPrototypeCache,
                              nsIXULPrototypeCache,
                              nsIObserver)


nsresult
NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(! aOuter, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsRefPtr<nsXULPrototypeCache> result = new nsXULPrototypeCache();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!(result->mPrototypeTable.Init() &&
          result->mStyleSheetTable.Init() &&
          result->mScriptTable.Init() &&
          result->mXBLDocTable.Init() &&
          result->mFastLoadURITable.Init())) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // XXX Ignore return values.
    gDisableXULCache =
        nsContentUtils::GetBoolPref(kDisableXULCachePref, gDisableXULCache);
    nsContentUtils::RegisterPrefCallback(kDisableXULCachePref,
                                         DisableXULCacheChangedCallback,
                                         nsnull);

    nsresult rv = result->QueryInterface(aIID, aResult);

    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (obsSvc && NS_SUCCEEDED(rv)) {
        nsXULPrototypeCache *p = result;
        obsSvc->AddObserver(p, "chrome-flush-skin-caches", PR_FALSE);
        obsSvc->AddObserver(p, "chrome-flush-caches", PR_FALSE);
        obsSvc->AddObserver(p, "startupcache-invalidate", PR_FALSE);
    }

    return rv;
}

/* static */ nsXULPrototypeCache*
nsXULPrototypeCache::GetInstance()
{
    // Theoretically this can return nsnull and callers should handle that.
    if (!sInstance) {
        nsIXULPrototypeCache* cache;

        CallGetService(kXULPrototypeCacheCID, &cache);

        sInstance = static_cast<nsXULPrototypeCache*>(cache);
    }
    return sInstance;
}

/* static */ nsIFastLoadService*
nsXULPrototypeCache::GetFastLoadService()
{
    return gFastLoadService;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsXULPrototypeCache::Observe(nsISupports* aSubject,
                             const char *aTopic,
                             const PRUnichar *aData)
{
    if (!strcmp(aTopic, "chrome-flush-skin-caches")) {
        FlushSkinFiles();
    }
    else if (!strcmp(aTopic, "chrome-flush-caches")) {
        Flush();
    }
    else if (!strcmp(aTopic, "startupcache-invalidate")) {
        AbortFastLoads();
    }
    else {
        NS_WARNING("Unexpected observer topic.");
    }
    return NS_OK;
}

nsXULPrototypeDocument*
nsXULPrototypeCache::GetPrototype(nsIURI* aURI)
{
    nsXULPrototypeDocument* protoDoc = mPrototypeTable.GetWeak(aURI);

    if (!protoDoc) {
        // No prototype in XUL memory cache. Spin up FastLoad Service and
        // look in FastLoad file.
        nsresult rv = StartFastLoad(aURI);
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIObjectInputStream> objectInput;
            gFastLoadService->GetInputStream(getter_AddRefs(objectInput));

            rv = StartFastLoadingURI(aURI, nsIFastLoadService::NS_FASTLOAD_READ);
            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIURI> oldURI;
                gFastLoadService->SelectMuxedDocument(aURI, getter_AddRefs(oldURI));

                // Create a new prototype document.
                nsRefPtr<nsXULPrototypeDocument> newProto;
                rv = NS_NewXULPrototypeDocument(getter_AddRefs(newProto));
                if (NS_FAILED(rv)) return nsnull;

                rv = newProto->Read(objectInput);
                if (NS_SUCCEEDED(rv)) {
                    rv = PutPrototype(newProto);
                    if (NS_FAILED(rv))
                        newProto = nsnull;

                    gFastLoadService->EndMuxedDocument(aURI);
                } else {
                    newProto = nsnull;
                }

                RemoveFromFastLoadSet(aURI);
                protoDoc = newProto;
            }
        }
    }
    return protoDoc;
}

nsresult
nsXULPrototypeCache::PutPrototype(nsXULPrototypeDocument* aDocument)
{
    nsCOMPtr<nsIURI> uri = aDocument->GetURI();
    // Put() releases any old value and addrefs the new one
    NS_ENSURE_TRUE(mPrototypeTable.Put(uri, aDocument), NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

nsresult
nsXULPrototypeCache::PutStyleSheet(nsCSSStyleSheet* aStyleSheet)
{
    nsIURI* uri = aStyleSheet->GetSheetURI();

    NS_ENSURE_TRUE(mStyleSheetTable.Put(uri, aStyleSheet),
                   NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}


void*
nsXULPrototypeCache::GetScript(nsIURI* aURI, PRUint32 *aLangID)
{
    CacheScriptEntry entry;
    if (!mScriptTable.Get(aURI, &entry)) {
        *aLangID = nsIProgrammingLanguage::UNKNOWN;
        return nsnull;
    }
    *aLangID = entry.mScriptTypeID;
    return entry.mScriptObject;
}


/* static */
static PLDHashOperator
ReleaseScriptObjectCallback(nsIURI* aKey, CacheScriptEntry &aData, void* aClosure)
{
    nsCOMPtr<nsIScriptRuntime> rt;
    if (NS_SUCCEEDED(NS_GetScriptRuntimeByID(aData.mScriptTypeID, getter_AddRefs(rt))))
        rt->DropScriptObject(aData.mScriptObject);
    return PL_DHASH_REMOVE;
}

nsresult
nsXULPrototypeCache::PutScript(nsIURI* aURI, PRUint32 aLangID, void* aScriptObject)
{
    CacheScriptEntry existingEntry;
    if (mScriptTable.Get(aURI, &existingEntry)) {
        NS_WARNING("loaded the same script twice (bug 392650)");

        // Reuse the callback used for enumeration in FlushScripts
        ReleaseScriptObjectCallback(aURI, existingEntry, nsnull);
    }

    CacheScriptEntry entry = {aLangID, aScriptObject};

    NS_ENSURE_TRUE(mScriptTable.Put(aURI, entry), NS_ERROR_OUT_OF_MEMORY);

    // Lock the object from being gc'd until it is removed from the cache
    nsCOMPtr<nsIScriptRuntime> rt;
    nsresult rv = NS_GetScriptRuntimeByID(aLangID, getter_AddRefs(rt));
    if (NS_SUCCEEDED(rv))
        rv = rt->HoldScriptObject(aScriptObject);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to GC lock the object");

    // On failure doing the lock, we should remove the map entry?
    return rv;
}

void
nsXULPrototypeCache::FlushScripts()
{
    // This callback will unlock each object so it can once again be gc'd.
    // XXX - this might be slow - we fetch the runtime each and every object.
    mScriptTable.Enumerate(ReleaseScriptObjectCallback, nsnull);
}


nsresult
nsXULPrototypeCache::PutXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo)
{
    nsIURI* uri = aDocumentInfo->DocumentURI();

    nsRefPtr<nsXBLDocumentInfo> info;
    mXBLDocTable.Get(uri, getter_AddRefs(info));
    if (!info) {
        NS_ENSURE_TRUE(mXBLDocTable.Put(uri, aDocumentInfo),
                       NS_ERROR_OUT_OF_MEMORY);
    }
    return NS_OK;
}

static PLDHashOperator
FlushSkinXBL(nsIURI* aKey, nsRefPtr<nsXBLDocumentInfo>& aDocInfo, void* aClosure)
{
  nsCAutoString str;
  aKey->GetPath(str);

  PLDHashOperator ret = PL_DHASH_NEXT;

  if (!strncmp(str.get(), "/skin", 5)) {
    ret = PL_DHASH_REMOVE;
  }

  return ret;
}

static PLDHashOperator
FlushSkinSheets(nsIURI* aKey, nsRefPtr<nsCSSStyleSheet>& aSheet, void* aClosure)
{
  nsCAutoString str;
  aSheet->GetSheetURI()->GetPath(str);

  PLDHashOperator ret = PL_DHASH_NEXT;

  if (!strncmp(str.get(), "/skin", 5)) {
    // This is a skin binding. Add the key to the list.
    ret = PL_DHASH_REMOVE;
  }
  return ret;
}

static PLDHashOperator
FlushScopedSkinStylesheets(nsIURI* aKey, nsRefPtr<nsXBLDocumentInfo> &aDocInfo, void* aClosure)
{
  aDocInfo->FlushSkinStylesheets();
  return PL_DHASH_NEXT;
}

void
nsXULPrototypeCache::FlushSkinFiles()
{
  // Flush out skin XBL files from the cache.
  mXBLDocTable.Enumerate(FlushSkinXBL, nsnull);

  // Now flush out our skin stylesheets from the cache.
  mStyleSheetTable.Enumerate(FlushSkinSheets, nsnull);

  // Iterate over all the remaining XBL and make sure cached
  // scoped skin stylesheets are flushed and refetched by the
  // prototype bindings.
  mXBLDocTable.Enumerate(FlushScopedSkinStylesheets, nsnull);
}


void
nsXULPrototypeCache::Flush()
{
    mPrototypeTable.Clear();

    // Clear the script cache, as it refers to prototype-owned mJSObjects.
    FlushScripts();

    mStyleSheetTable.Clear();
    mXBLDocTable.Clear();
}


PRBool
nsXULPrototypeCache::IsEnabled()
{
    return !gDisableXULCache;
}

static PRBool gDisableXULFastLoad = PR_FALSE;           // enabled by default
static PRBool gChecksumXULFastLoadFile = PR_TRUE;       // XXXbe too paranoid

void
nsXULPrototypeCache::AbortFastLoads()
{
#ifdef DEBUG_brendan
    NS_BREAK();
#endif

    // Flush the XUL cache for good measure, in case we cached a bogus/downrev
    // script, somehow.
    Flush();

    // Clear the FastLoad set
    mFastLoadURITable.Clear();

    nsCOMPtr<nsIFastLoadService> fastLoadService = gFastLoadService;
    nsCOMPtr<nsIFile> file = gFastLoadFile;

    nsresult rv;

    if (! fastLoadService) {
        fastLoadService = do_GetFastLoadService();
        if (! fastLoadService)
            return;

        rv = fastLoadService->NewFastLoadFile(XUL_FASTLOAD_FILE_BASENAME,
                                              getter_AddRefs(file));
        if (NS_FAILED(rv))
            return;
    }

    // Fetch the current input (if FastLoad file existed) or output (if we're
    // creating the FastLoad file during this app startup) stream.
    nsCOMPtr<nsIObjectInputStream> objectInput;
    nsCOMPtr<nsIObjectOutputStream> objectOutput;
    fastLoadService->GetInputStream(getter_AddRefs(objectInput));
    fastLoadService->GetOutputStream(getter_AddRefs(objectOutput));

    if (objectOutput) {
        fastLoadService->SetOutputStream(nsnull);

        if (NS_SUCCEEDED(objectOutput->Close()) && gChecksumXULFastLoadFile)
            fastLoadService->CacheChecksum(file,
                                           objectOutput);
    }

    if (objectInput) {
        // If this is the last of one or more XUL master documents loaded
        // together at app startup, close the FastLoad service's singleton
        // input stream now.
        fastLoadService->SetInputStream(nsnull);
        objectInput->Close();
    }

    // Now rename or remove the file.
    if (file) {
#ifdef DEBUG
        // Remove any existing Aborted.mfasl files generated in previous runs.
        nsCOMPtr<nsIFile> existingAbortedFile;
        file->Clone(getter_AddRefs(existingAbortedFile));
        if (existingAbortedFile) {
            existingAbortedFile->SetLeafName(NS_LITERAL_STRING("Aborted.mfasl"));
            PRBool fileExists = PR_FALSE;
            existingAbortedFile->Exists(&fileExists);
            if (fileExists)
                existingAbortedFile->Remove(PR_FALSE);
        }
        file->MoveToNative(nsnull, NS_LITERAL_CSTRING("Aborted.mfasl"));
#else
        rv = file->Remove(PR_FALSE);
        if (NS_FAILED(rv))
            NS_WARNING("Failed to remove fastload file, fastload data may be outdated");
#endif
    }

    // If the list is empty now, the FastLoad process is done.
    NS_IF_RELEASE(gFastLoadService);
    NS_IF_RELEASE(gFastLoadFile);
}


void
nsXULPrototypeCache::RemoveFromFastLoadSet(nsIURI* aURI)
{
    mFastLoadURITable.Remove(aURI);
}

static const char kDisableXULFastLoadPref[] = "nglayout.debug.disable_xul_fastload";
static const char kChecksumXULFastLoadFilePref[] = "nglayout.debug.checksum_xul_fastload_file";

nsresult
nsXULPrototypeCache::WritePrototype(nsXULPrototypeDocument* aPrototypeDocument)
{
    nsresult rv = NS_OK, rv2 = NS_OK;

    // We're here before the FastLoad service has been initialized, probably because
    // of the profile manager. Bail quietly, don't worry, we'll be back later.
    if (! gFastLoadService)
        return NS_OK;

    // Fetch the current input (if FastLoad file existed) or output (if we're
    // creating the FastLoad file during this app startup) stream.
    nsCOMPtr<nsIObjectInputStream> objectInput;
    nsCOMPtr<nsIObjectOutputStream> objectOutput;
    gFastLoadService->GetInputStream(getter_AddRefs(objectInput));
    gFastLoadService->GetOutputStream(getter_AddRefs(objectOutput));

    nsCOMPtr<nsIURI> protoURI = aPrototypeDocument->GetURI();

    // Remove this document from the FastLoad table. We use the table's
    // emptiness instead of a counter to decide when the FastLoad process
    // has completed. When complete, we can write footer details to the
    // FastLoad file.
    RemoveFromFastLoadSet(protoURI);

    PRInt32 count = mFastLoadURITable.Count();

    if (objectOutput) {
        rv = StartFastLoadingURI(protoURI, nsIFastLoadService::NS_FASTLOAD_WRITE);
        if (NS_SUCCEEDED(rv)) {
            // Re-select the URL of the current prototype, as out-of-line script loads
            // may have changed
            nsCOMPtr<nsIURI> oldURI;
            gFastLoadService->SelectMuxedDocument(protoURI, getter_AddRefs(oldURI));

            aPrototypeDocument->Write(objectOutput);

            gFastLoadService->EndMuxedDocument(protoURI);
        }

        // If this is the last of one or more XUL master documents loaded
        // together at app startup, close the FastLoad service's singleton
        // output stream now.
        //
        // NB: we must close input after output, in case the output stream
        // implementation needs to read from the input stream, to compute a
        // FastLoad file checksum.  In that case, the implementation used
        // nsIFastLoadFileIO to get the corresponding input stream for this
        // output stream.
        if (count == 0) {
            gFastLoadService->SetOutputStream(nsnull);
            rv = objectOutput->Close();

            if (NS_SUCCEEDED(rv) && gChecksumXULFastLoadFile) {
                rv = gFastLoadService->CacheChecksum(gFastLoadFile,
                                                     objectOutput);
            }
        }
    }

    if (objectInput) {
        // If this is the last of one or more XUL master documents loaded
        // together at app startup, close the FastLoad service's singleton
        // input stream now.
        if (count == 0) {
            gFastLoadService->SetInputStream(nsnull);
            rv2 = objectInput->Close();
        }
    }

    // If the list is empty now, the FastLoad process is done.
    if (count == 0) {
        NS_RELEASE(gFastLoadService);
        NS_RELEASE(gFastLoadFile);
    }

    return NS_FAILED(rv) ? rv : rv2;
}


nsresult
nsXULPrototypeCache::StartFastLoadingURI(nsIURI* aURI, PRInt32 aDirectionFlags)
{
    nsresult rv;

    nsCAutoString urlspec;
    rv = aURI->GetAsciiSpec(urlspec);
    if (NS_FAILED(rv)) return rv;

    // If StartMuxedDocument returns NS_ERROR_NOT_AVAILABLE, then
    // we must be reading the file, and urlspec was not associated
    // with any multiplexed stream in it.  The FastLoad service
    // will therefore arrange to update the file, writing new data
    // at the end while old (available) data continues to be read
    // from the pre-existing part of the file.
    return gFastLoadService->StartMuxedDocument(aURI, urlspec.get(), aDirectionFlags);
}

static int
FastLoadPrefChangedCallback(const char* aPref, void* aClosure)
{
    PRBool wasEnabled = !gDisableXULFastLoad;
    gDisableXULFastLoad =
        nsContentUtils::GetBoolPref(kDisableXULFastLoadPref,
                                    gDisableXULFastLoad);

    if (wasEnabled && gDisableXULFastLoad) {
        static NS_DEFINE_CID(kXULPrototypeCacheCID, NS_XULPROTOTYPECACHE_CID);
        nsCOMPtr<nsIXULPrototypeCache> cache =
            do_GetService(kXULPrototypeCacheCID);

        if (cache)
            cache->AbortFastLoads();
    }

    gChecksumXULFastLoadFile =
        nsContentUtils::GetBoolPref(kChecksumXULFastLoadFilePref,
                                    gChecksumXULFastLoadFile);

    return 0;
}


class nsXULFastLoadFileIO : public nsIFastLoadFileIO
{
  public:
    nsXULFastLoadFileIO(nsIFile* aFile)
      : mFile(aFile), mTruncateOutputFile(true) {
    }

    virtual ~nsXULFastLoadFileIO() {
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIFASTLOADFILEIO

    nsCOMPtr<nsIFile>         mFile;
    nsCOMPtr<nsIInputStream>  mInputStream;
    nsCOMPtr<nsIOutputStream> mOutputStream;
    bool mTruncateOutputFile;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsXULFastLoadFileIO, nsIFastLoadFileIO)


NS_IMETHODIMP
nsXULFastLoadFileIO::GetInputStream(nsIInputStream** aResult)
{
    if (! mInputStream) {
        nsresult rv;
        nsCOMPtr<nsIInputStream> fileInput;
        rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInput), mFile);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewBufferedInputStream(getter_AddRefs(mInputStream),
                                       fileInput,
                                       XUL_DESERIALIZATION_BUFFER_SIZE);
        if (NS_FAILED(rv)) return rv;
    }

    NS_ADDREF(*aResult = mInputStream);
    return NS_OK;
}


NS_IMETHODIMP
nsXULFastLoadFileIO::GetOutputStream(nsIOutputStream** aResult)
{
    if (! mOutputStream) {
        PRInt32 ioFlags = PR_WRONLY;
        if (mTruncateOutputFile)
            ioFlags |= PR_CREATE_FILE | PR_TRUNCATE;

        nsresult rv;
        nsCOMPtr<nsIOutputStream> fileOutput;
        rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutput), mFile,
                                         ioFlags, 0644);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewBufferedOutputStream(getter_AddRefs(mOutputStream),
                                        fileOutput,
                                        XUL_SERIALIZATION_BUFFER_SIZE);
        if (NS_FAILED(rv)) return rv;
    }

    NS_ADDREF(*aResult = mOutputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsXULFastLoadFileIO::DisableTruncate()
{
    mTruncateOutputFile = false;
    return NS_OK;
}

nsresult
nsXULPrototypeCache::StartFastLoad(nsIURI* aURI)
{
    nsresult rv;

    nsCAutoString path;
    aURI->GetPath(path);
    if (!StringEndsWith(path, NS_LITERAL_CSTRING(".xul")))
        return NS_ERROR_NOT_AVAILABLE;

    // Test gFastLoadFile to decide whether this is the first nsXULDocument
    // participating in FastLoad.  If gFastLoadFile is non-null, this document
    // must not be first, but it can join the FastLoad process.  Examples of
    // multiple master documents participating include hiddenWindow.xul and
    // navigator.xul on the Mac, and multiple-app-component (e.g., mailnews
    // and browser) startup due to command-line arguments.
    //
    // XXXbe we should attempt to update the FastLoad file after startup!
    //
    // XXXbe we do not yet use nsFastLoadPtrs, but once we do, we must keep
    // the FastLoad input stream open for the life of the app.
    if (gFastLoadService && gFastLoadFile) {
        mFastLoadURITable.Put(aURI, 1);

        return NS_OK;
    }

    // Use a local to refer to the service till we're sure we succeeded, then
    // commit to gFastLoadService.  Same for gFastLoadFile, which is used to
    // delete the FastLoad file on abort.
    nsCOMPtr<nsIFastLoadService> fastLoadService(do_GetFastLoadService());
    if (! fastLoadService)
        return NS_ERROR_FAILURE;

    gDisableXULFastLoad =
        nsContentUtils::GetBoolPref(kDisableXULFastLoadPref,
                                    gDisableXULFastLoad);
    gChecksumXULFastLoadFile =
        nsContentUtils::GetBoolPref(kChecksumXULFastLoadFilePref,
                                    gChecksumXULFastLoadFile);
    nsContentUtils::RegisterPrefCallback(kDisableXULFastLoadPref,
                                         FastLoadPrefChangedCallback,
                                         nsnull);
    nsContentUtils::RegisterPrefCallback(kChecksumXULFastLoadFilePref,
                                         FastLoadPrefChangedCallback,
                                         nsnull);

    if (gDisableXULFastLoad)
        return NS_ERROR_NOT_AVAILABLE;

    // Get the chrome directory to validate against the one stored in the
    // FastLoad file, or to store there if we're generating a new file.
    nsCOMPtr<nsIFile> chromeDir;
    rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR, getter_AddRefs(chromeDir));
    if (NS_FAILED(rv))
        return rv;
    nsCAutoString chromePath;
    rv = chromeDir->GetNativePath(chromePath);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIFile> file;
    rv = fastLoadService->NewFastLoadFile(XUL_FASTLOAD_FILE_BASENAME,
                                          getter_AddRefs(file));
    if (NS_FAILED(rv))
        return rv;

    // Give the FastLoad service an object by which it can get or create a
    // file output stream given an input stream on the same file.
    nsXULFastLoadFileIO* xio = new nsXULFastLoadFileIO(file);
    nsCOMPtr<nsIFastLoadFileIO> io = static_cast<nsIFastLoadFileIO*>(xio);
    if (! io)
        return NS_ERROR_OUT_OF_MEMORY;
    fastLoadService->SetFileIO(io);

    nsCOMPtr<nsIXULChromeRegistry> chromeReg =
        mozilla::services::GetXULChromeRegistryService();
    if (!chromeReg)
        return NS_ERROR_FAILURE;

    // XXXbe we assume the first package's locale is the same as the locale of
    // all subsequent packages of FastLoaded chrome URIs....
    nsCAutoString package;
    rv = aURI->GetHost(package);
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString locale;
    rv = chromeReg->GetSelectedLocale(package, locale);
    if (NS_FAILED(rv))
        return rv;

    // Try to read an existent FastLoad file.
    PRBool exists = PR_FALSE;
    if (NS_SUCCEEDED(file->Exists(&exists)) && exists) {
        nsCOMPtr<nsIObjectInputStream> objectInput;
        rv = fastLoadService->NewInputStream(file, getter_AddRefs(objectInput));

        if (NS_SUCCEEDED(rv)) {
            if (NS_SUCCEEDED(rv)) {
                // Get the XUL fastload file version number, which should be
                // decremented whenever the XUL-specific file format changes
                // (see public/nsIXULPrototypeCache.h for the #define).
                PRUint32 xulFastLoadVersion, jsByteCodeVersion;
                rv = objectInput->Read32(&xulFastLoadVersion);
                rv |= objectInput->Read32(&jsByteCodeVersion);
                if (NS_SUCCEEDED(rv)) {
                    if (xulFastLoadVersion != XUL_FASTLOAD_FILE_VERSION ||
                        jsByteCodeVersion != JSXDR_BYTECODE_VERSION) {
#ifdef DEBUG
                        printf((xulFastLoadVersion != XUL_FASTLOAD_FILE_VERSION)
                               ? "bad FastLoad file version\n"
                               : "bad JS bytecode version\n");
#endif
                        rv = NS_ERROR_UNEXPECTED;
                    } else {
                        nsCAutoString fileChromePath, fileLocale;

                        rv = objectInput->ReadCString(fileChromePath);
                        rv |= objectInput->ReadCString(fileLocale);
                        if (NS_SUCCEEDED(rv) &&
                            (!fileChromePath.Equals(chromePath) ||
                             !fileLocale.Equals(locale))) {
                            rv = NS_ERROR_UNEXPECTED;
                        }
                    }
                }
            }
        }

        if (NS_SUCCEEDED(rv)) {
            fastLoadService->SetInputStream(objectInput);
        } else {
            // NB: we must close before attempting to remove, for non-Unix OSes
            // that can't do open-unlink.
            if (objectInput)
                objectInput->Close();
            xio->mInputStream = nsnull;

#ifdef DEBUG
            file->MoveToNative(nsnull, NS_LITERAL_CSTRING("Invalid.mfasl"));
#else
            file->Remove(PR_FALSE);
#endif
            exists = PR_FALSE;
        }
    }

    // FastLoad file not found, or invalid: write a new one.
    if (! exists) {
        nsCOMPtr<nsIOutputStream> output;
        rv = io->GetOutputStream(getter_AddRefs(output));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIObjectOutputStream> objectOutput;
        rv = fastLoadService->NewOutputStream(output,
                                              getter_AddRefs(objectOutput));
        if (NS_SUCCEEDED(rv)) {
            rv = objectOutput->Write32(XUL_FASTLOAD_FILE_VERSION);
            rv |= objectOutput->Write32(JSXDR_BYTECODE_VERSION);
            rv |= objectOutput->WriteStringZ(chromePath.get());
            rv |= objectOutput->WriteStringZ(locale.get());
        }

        // Remove here even though some errors above will lead to a FastLoad
        // file invalidation.  Other errors (failure to note the dependency on
        // installed-chrome.txt, e.g.) will not cause invalidation, and we may
        // as well tidy up now.
        if (NS_FAILED(rv)) {
            if (objectOutput)
                objectOutput->Close();
            else
                output->Close();
            xio->mOutputStream = nsnull;

            file->Remove(PR_FALSE);
            return NS_ERROR_FAILURE;
        }

        fastLoadService->SetOutputStream(objectOutput);
    }

    // Success!  Insert this URI into the mFastLoadURITable
    // and commit locals to globals.
    mFastLoadURITable.Put(aURI, 1);

    NS_ADDREF(gFastLoadService = fastLoadService);
    NS_ADDREF(gFastLoadFile = file);
    return NS_OK;
}

