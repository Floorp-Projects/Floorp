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
 *   Ben Goodger <ben@netscape.com>
 *   Pete Collins <petejc@collab.net>
 *   Dan Rosen <dr@netscape.com>
 *   Johnny Stenback <jst@netscape.com>
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

/*

  An implementation for the XUL document. This implementation serves
  as the basis for generating an NGLayout content model.

  Notes
  -----

  1. We do some monkey business in the document observer methods to`
     keep the element map in sync for HTML elements. Why don't we just
     do it for _all_ elements? Well, in the case of XUL elements,
     which may be lazily created during frame construction, the
     document observer methods will never be called because we'll be
     adding the XUL nodes into the content model "quietly".

  2. The "element map" maps an RDF resource to the elements whose 'id'
     or 'ref' attributes refer to that resource. We re-use the element
     map to support the HTML-like 'getElementById()' method.

*/

// Note the ALPHABETICAL ORDERING
#include "nsXULDocument.h"

#include "nsDOMError.h"
#include "nsIBoxObject.h"
#include "nsIChromeRegistry.h"
#include "nsIContentSink.h" // for NS_CONTENT_ID_COUNTER_BASE
#include "nsIScrollableView.h"
#include "nsIContentViewer.h"
#include "nsGUIEvent.h"
#include "nsIDOMXULElement.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIStreamListener.h"
#include "nsITextContent.h"
#include "nsITimer.h"
#include "nsIDocShell.h"
#include "nsXULAtoms.h"
#include "nsIXULContent.h"
#include "nsIXULContentSink.h"
#include "nsXULContentUtils.h"
#include "nsIXULOverlayProvider.h"
#include "nsIXULPrototypeCache.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsPIBoxObject.h"
#include "nsRDFCID.h"
#include "nsILocalStore.h"
#include "nsXPIDLString.h"
#include "nsPIDOMWindow.h"
#include "nsXULCommandDispatcher.h"
#include "nsXULDocument.h"
#include "nsXULElement.h"
#include "prlog.h"
#include "rdf.h"
#include "nsIFrame.h"
#include "nsIXBLService.h"
#include "nsCExternalHandlerService.h"
#include "nsMimeTypes.h"
#include "nsIFastLoadService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIFocusController.h"
#include "nsContentList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsIParser.h"
#include "nsICSSStyleSheet.h"

//----------------------------------------------------------------------
//
// CIDs
//

static NS_DEFINE_CID(kLocalStoreCID,             NS_LOCALSTORE_CID);
static NS_DEFINE_CID(kParserCID,                 NS_PARSER_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);

static PRBool IsChromeURI(nsIURI* aURI)
{
    // why is this check a member function of nsXULDocument? -gagan
    PRBool isChrome = PR_FALSE;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
        return PR_TRUE;
    return PR_FALSE;
}

//----------------------------------------------------------------------
//
// Miscellaneous Constants
//

const nsForwardReference::Phase nsForwardReference::kPasses[] = {
    nsForwardReference::eConstruction,
    nsForwardReference::eHookup,
    nsForwardReference::eDone
};


//----------------------------------------------------------------------
//
// Statics
//

PRInt32 nsXULDocument::gRefCnt = 0;

nsIRDFService* nsXULDocument::gRDFService;
nsIRDFResource* nsXULDocument::kNC_persist;
nsIRDFResource* nsXULDocument::kNC_attribute;
nsIRDFResource* nsXULDocument::kNC_value;

nsIXULPrototypeCache* nsXULDocument::gXULCache;

PRLogModuleInfo* nsXULDocument::gXULLog;

class nsProxyLoadStream : public nsIInputStream
{
private:
    const char* mBuffer;
    PRUint32    mSize;
    PRUint32    mIndex;

public:
    nsProxyLoadStream(void) : mBuffer(nsnull)
    {
    }

    virtual ~nsProxyLoadStream(void)
    {
    }

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIBaseStream
    NS_IMETHOD Close(void)
    {
        return NS_OK;
    }

    // nsIInputStream
    NS_IMETHOD Available(PRUint32 *aLength)
    {
        *aLength = mSize - mIndex;
        return NS_OK;
    }

    NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
    {
        PRUint32 readCount = 0;
        while (mIndex < mSize && aCount > 0) {
            *aBuf = mBuffer[mIndex];
            ++aBuf;
            ++mIndex;
            readCount++;
            --aCount;
        }
        *aReadCount = readCount;
        return NS_OK;
    }

    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure,
                            PRUint32 count, PRUint32 *_retval)
    {
        NS_NOTREACHED("ReadSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD IsNonBlocking(PRBool *aNonBlocking)
    {
        *aNonBlocking = PR_TRUE;
        return NS_OK;
    }

    // Implementation
    void SetBuffer(const char* aBuffer, PRUint32 aSize)
    {
        mBuffer = aBuffer;
        mSize = aSize;
        mIndex = 0;
    }
};

NS_IMPL_ISUPPORTS1(nsProxyLoadStream, nsIInputStream)

//----------------------------------------------------------------------
//
// PlaceholderRequest
//
//   This is a dummy request implementation that we add to the load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't
//   fire before we've finished building the complete document content
//   model.
//

class PlaceHolderRequest : public nsIChannel
{
protected:
    PlaceHolderRequest();
    virtual ~PlaceHolderRequest();

    static PRInt32 gRefCnt;
    static nsIURI* gURI;

    nsCOMPtr<nsILoadGroup> mLoadGroup;

public:
    static nsresult
    Create(nsIRequest** aResult);

    NS_DECL_ISUPPORTS

    // nsIRequest
    NS_IMETHOD GetName(nsACString &result) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
    NS_IMETHOD GetStatus(nsresult *status) { *status = NS_OK; return NS_OK; }
    NS_IMETHOD Cancel(nsresult status)  { return NS_OK; }
    NS_IMETHOD Suspend(void) { return NS_OK; }
    NS_IMETHOD Resume(void)  { return NS_OK; }
    NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup) { *aLoadGroup = mLoadGroup; NS_IF_ADDREF(*aLoadGroup); return NS_OK; }
    NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup) { mLoadGroup = aLoadGroup; return NS_OK; }
    NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) { *aLoadFlags = nsIRequest::LOAD_NORMAL; return NS_OK; }
    NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) { return NS_OK; }

    // nsIChannel
    NS_IMETHOD GetOriginalURI(nsIURI* *aOriginalURI) { *aOriginalURI = gURI; NS_ADDREF(*aOriginalURI); return NS_OK; }
    NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) { gURI = aOriginalURI; NS_ADDREF(gURI); return NS_OK; }
    NS_IMETHOD GetURI(nsIURI* *aURI) { *aURI = gURI; NS_ADDREF(*aURI); return NS_OK; }
    NS_IMETHOD SetURI(nsIURI* aURI) { gURI = aURI; NS_ADDREF(gURI); return NS_OK; }
    NS_IMETHOD Open(nsIInputStream **_retval) { *_retval = nsnull; return NS_OK; }
    NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt) { return NS_OK; }
    NS_IMETHOD GetOwner(nsISupports * *aOwner) { *aOwner = nsnull; return NS_OK; }
    NS_IMETHOD SetOwner(nsISupports * aOwner) { return NS_OK; }
    NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) { *aNotificationCallbacks = nsnull; return NS_OK; }
    NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) { return NS_OK; }
    NS_IMETHOD GetSecurityInfo(nsISupports * *aSecurityInfo) { *aSecurityInfo = nsnull; return NS_OK; }
    NS_IMETHOD GetContentType(nsACString &aContentType) { aContentType.Truncate(); return NS_OK; }
    NS_IMETHOD SetContentType(const nsACString &aContentType) { return NS_OK; }
    NS_IMETHOD GetContentCharset(nsACString &aContentCharset) { aContentCharset.Truncate(); return NS_OK; }
    NS_IMETHOD SetContentCharset(const nsACString &aContentCharset) { return NS_OK; }
    NS_IMETHOD GetContentLength(PRInt32 *aContentLength) { return NS_OK; }
    NS_IMETHOD SetContentLength(PRInt32 aContentLength) { return NS_OK; }
};

PRInt32 PlaceHolderRequest::gRefCnt;
nsIURI* PlaceHolderRequest::gURI;

NS_IMPL_ADDREF(PlaceHolderRequest)
NS_IMPL_RELEASE(PlaceHolderRequest)
NS_IMPL_QUERY_INTERFACE2(PlaceHolderRequest, nsIRequest, nsIChannel)

nsresult
PlaceHolderRequest::Create(nsIRequest** aResult)
{
    PlaceHolderRequest* request = new PlaceHolderRequest();
    if (! request)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = request;
    NS_ADDREF(*aResult);
    return NS_OK;
}


PlaceHolderRequest::PlaceHolderRequest()
{

    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = NS_NewURI(&gURI, NS_LITERAL_CSTRING("about:xul-master-placeholder"), nsnull);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create about:xul-master-placeholder");
    }
}


PlaceHolderRequest::~PlaceHolderRequest()
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gURI);
    }
}

//----------------------------------------------------------------------

struct BroadcasterMapEntry : public PLDHashEntryHdr {
    nsIDOMElement*   mBroadcaster; // [WEAK]
    nsSmallVoidArray mListeners;   // [OWNING] of BroadcastListener objects
};

struct BroadcastListener {
    nsIDOMElement*    mListener; // [WEAK] XXXwaterson crash waiting to happen!
    nsCOMPtr<nsIAtom> mAttribute;
};

//----------------------------------------------------------------------
//
// ctors & dtors
//

    // NOTE! nsDocument::operator new() zeroes out all members, so
    // don't bother initializing members to 0.

nsXULDocument::nsXULDocument(void)
    : mResolutionPhase(nsForwardReference::eStart),
      mState(eState_Master)
{

    // NOTE! nsDocument::operator new() zeroes out all members, so don't
    // bother initializing members to 0.

    // Override the default in nsDocument
    mCharacterSet.AssignLiteral("UTF-8");

    mDefaultElementType = kNameSpaceID_XUL;
}

nsXULDocument::~nsXULDocument()
{
    NS_ASSERTION(mNextSrcLoadWaiter == nsnull,
        "unreferenced document still waiting for script source to load?");

    // Notify our observers here, we can't let the nsDocument
    // destructor do that for us since some of the observers are
    // deleted by the time we get there.

    PRInt32 i;
    for (i = mObservers.Count() - 1; i >= 0; --i) {
        // XXX Should this be a kungfudeathgrip?!!!!
        nsIDocumentObserver* observer =
            NS_STATIC_CAST(nsIDocumentObserver *, mObservers.ElementAt(i));

        observer->DocumentWillBeDestroyed(this);
    }

    mObservers.Clear();

    // In case we failed somewhere early on and the forward observer
    // decls never got resolved.
    DestroyForwardReferences();

    // Destroy our broadcaster map.
    if (mBroadcasterMap) {
        PL_DHashTableDestroy(mBroadcasterMap);
    }

    if (mLocalStore) {
        nsCOMPtr<nsIRDFRemoteDataSource> remote =
            do_QueryInterface(mLocalStore);
        if (remote)
            remote->Flush();
    }

    delete mTemplateBuilderTable;

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);

        NS_IF_RELEASE(kNC_persist);
        NS_IF_RELEASE(kNC_attribute);
        NS_IF_RELEASE(kNC_value);

        if (gXULCache) {
            // Remove the current document here from the FastLoad table in
            // case the document did not make it past StartLayout in
            // ResumeWalk. The FastLoad table must be clear of entries so
            // that the FastLoad file footer can be properly written.
            if (mDocumentURI)
                gXULCache->RemoveFromFastLoadSet(mDocumentURI);

            NS_RELEASE(gXULCache);
        }
    }

    // The destructor of nsDocument will delete references to style
    // sheets, but we don't want that if we're a popup document, so
    // then we'll clear the stylesheets array here to prevent that
    // from happening.
    if (mIsPopup) {
        mStyleSheets.Clear();
    }

    // This is done in nsDocument::~nsDocument() too, but since this
    // call ends up calling back into the document through virtual
    // methods (nsIDocument::GetPrincipal()) we must do it here before
    // we go out of nsXULDocument's destructor.
    if (mNodeInfoManager) {
        mNodeInfoManager->DropDocumentReference();
    }
}

nsresult
NS_NewXULDocument(nsIXULDocument** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsXULDocument* doc = new nsXULDocument();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);

    nsresult rv;
    if (NS_FAILED(rv = doc->Init())) {
        NS_RELEASE(doc);
        return rv;
    }

    *result = doc;
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsISupports interface
//

NS_IMPL_ADDREF_INHERITED(nsXULDocument, nsXMLDocument)
NS_IMPL_RELEASE_INHERITED(nsXULDocument, nsXMLDocument)


// QueryInterface implementation for nsXULDocument
NS_INTERFACE_MAP_BEGIN(nsXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULDocument)
NS_INTERFACE_MAP_END_INHERITING(nsXMLDocument)


//----------------------------------------------------------------------
//
// nsIDocument interface
//

void
nsXULDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
    NS_NOTREACHED("Reset");
}

void
nsXULDocument::ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup)
{
    NS_NOTREACHED("ResetToURI");
}

// Override the nsDocument.cpp method to keep from returning the
// "cached XUL" type which is completely internal and may confuse
// people
NS_IMETHODIMP
nsXULDocument::GetContentType(nsAString& aContentType)
{
    aContentType.AssignLiteral("application/vnd.mozilla.xul+xml");
    return NS_OK;
}

void
nsXULDocument::SetContentType(const nsAString& aContentType)
{
    NS_ASSERTION(aContentType.EqualsLiteral("application/vnd.mozilla.xul+xml"),
                 "xul-documents always has content-type application/vnd.mozilla.xul+xml");
    // Don't do anything, xul always has the mimetype
    // application/vnd.mozilla.xul+xml
}

nsresult
nsXULDocument::StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset, nsIContentSink* aSink)
{
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);

    mDocumentTitle.Truncate();

    nsresult rv = aChannel->GetOriginalURI(getter_AddRefs(mDocumentURI));
    NS_ENSURE_SUCCESS(rv, rv);
    
    // XXXbz this code is repeated from nsDocument::Reset; we
    // really need to refactor this part better.
    PRBool isChrome = PR_FALSE;
    PRBool isRes = PR_FALSE;
    rv = mDocumentURI->SchemeIs("chrome", &isChrome);
    rv |= mDocumentURI->SchemeIs("resource", &isRes);

    if (NS_SUCCEEDED(rv) && !isChrome && !isRes) {
        rv = aChannel->GetURI(getter_AddRefs(mDocumentURI));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = ResetStylesheetsToURI(mDocumentURI);
    if (NS_FAILED(rv)) return rv;

    RetrieveRelevantHeaders(aChannel);

    // Look in the chrome cache: we've got this puppy loaded
    // already.
    nsCOMPtr<nsIXULPrototypeDocument> proto;
    if (IsChromeURI(mDocumentURI))
        gXULCache->GetPrototype(mDocumentURI, getter_AddRefs(proto));

    // Same comment as nsChromeProtocolHandler::NewChannel and
    // nsXULDocument::ResumeWalk
    // - Ben Goodger
    //
    // We don't abort on failure here because there are too many valid
    // cases that can return failure, and the null-ness of |proto| is enough
    // to trigger the fail-safe parse-from-disk solution. Example failure cases
    // (for reference) include:
    //
    // NS_ERROR_NOT_AVAILABLE: the URI cannot be found in the FastLoad cache,
    //                         parse from disk
    // other: the FastLoad cache file, XUL.mfl, could not be found, probably
    //        due to being accessed before a profile has been selected (e.g.
    //        loading chrome for the profile manager itself). This must be
    //        parsed from disk.

    if (proto) {
        // If we're racing with another document to load proto, wait till the
        // load has finished loading before trying to add cloned style sheets.
        // nsXULDocument::EndLoad will call proto->NotifyLoadDone, which will
        // find all racing documents and notify them via OnPrototypeLoadDone,
        // which will add style sheet clones to each document.
        PRBool loaded;
        rv = proto->AwaitLoadDone(this, &loaded);
        if (NS_FAILED(rv)) return rv;

        mMasterPrototype = mCurrentPrototype = proto;

        // Add cloned style sheet references only if the prototype has in
        // fact already loaded.  It may still be loading when we hit the XUL
        // prototype cache.
        if (loaded) {
            rv = AddPrototypeSheets();
            if (NS_FAILED(rv)) return rv;
        }

        // We need a listener, even if proto is not yet loaded, in which
        // event the listener's OnStopRequest method does nothing, and all
        // the interesting work happens below nsXULDocument::EndLoad, from
        // the call there to mCurrentPrototype->NotifyLoadDone().
        *aDocListener = new CachedChromeStreamListener(this, loaded);
        if (! *aDocListener)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        PRBool useXULCache;
        gXULCache->GetEnabled(&useXULCache);
        PRBool fillXULCache = (useXULCache && IsChromeURI(mDocumentURI));


        // It's just a vanilla document load. Create a parser to deal
        // with the stream n' stuff.

        nsCOMPtr<nsIParser> parser;
        rv = PrepareToLoad(aContainer, aCommand, aChannel, aLoadGroup,
                           getter_AddRefs(parser));
        if (NS_FAILED(rv)) return rv;

        // Predicate mIsWritingFastLoad on the XUL cache being enabled,
        // so we don't have to re-check whether the cache is enabled all
        // the time.
        mIsWritingFastLoad = useXULCache;

        nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "parser doesn't support nsIStreamListener");
        if (NS_FAILED(rv)) return rv;

        *aDocListener = listener;

        parser->Parse(mDocumentURI);

        // Put the current prototype, created under PrepareToLoad, into the
        // XUL prototype cache now.  We can't do this under PrepareToLoad or
        // overlay loading will break; search for PutPrototype in ResumeWalk
        // and see the comment there.
        if (fillXULCache) {
            rv = gXULCache->PutPrototype(mCurrentPrototype);
            if (NS_FAILED(rv)) return rv;
        }
    }

    NS_IF_ADDREF(*aDocListener);
    return NS_OK;
}

nsIPrincipal*
nsXULDocument::GetPrincipal()
{
    NS_ASSERTION(mMasterPrototype, "Missing master prototype. See bug 169036");
    NS_ENSURE_TRUE(mMasterPrototype, nsnull);

    return mMasterPrototype->GetDocumentPrincipal();
}

void
nsXULDocument::SetPrincipal(nsIPrincipal *aPrincipal)
{
    NS_NOTREACHED("SetPrincipal");
}


void
nsXULDocument::EndLoad()
{
    nsresult rv;

    // Whack the prototype document into the cache so that the next
    // time somebody asks for it, they don't need to load it by hand.

    nsCOMPtr<nsIURI> uri;
    rv = mCurrentPrototype->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return;

    PRBool isChrome = IsChromeURI(uri);

    // Remember if the XUL cache is on
    PRBool useXULCache;
    gXULCache->GetEnabled(&useXULCache);

    // If the current prototype is an overlay document (non-master prototype)
    // and we're filling the FastLoad disk cache, tell the cache we're done
    // loading it, and write the prototype.
    if (useXULCache && mIsWritingFastLoad &&
        mMasterPrototype != mCurrentPrototype &&
        isChrome)
        gXULCache->WritePrototype(mCurrentPrototype);

    if (isChrome) {
        nsCOMPtr<nsIXULOverlayProvider> reg =
            do_GetService(NS_CHROMEREGISTRY_CONTRACTID);
        nsCOMPtr<nsICSSLoader> cssLoader = GetCSSLoader();
        
        if (reg && cssLoader) {
            nsCOMPtr<nsISimpleEnumerator> overlays;
            reg->GetStyleOverlays(uri, getter_AddRefs(overlays));

            PRBool moreSheets;
            nsCOMPtr<nsISupports> next;
            nsCOMPtr<nsIURI> sheetURI;
            nsCOMPtr<nsICSSStyleSheet> sheet;

            while (NS_SUCCEEDED(rv = overlays->HasMoreElements(&moreSheets)) &&
                   moreSheets) {
                overlays->GetNext(getter_AddRefs(next));

                sheetURI = do_QueryInterface(next);
                if (!uri) {
                    NS_ERROR("Chrome registry handed me a non-nsIURI object!");
                    continue;
                }

                if (useXULCache && IsChromeURI(sheetURI)) {
                    mCurrentPrototype->AddStyleSheetReference(sheetURI);
                }

                cssLoader->LoadAgentSheet(sheetURI, getter_AddRefs(sheet));
                if (!sheet) {
                    NS_WARNING("Couldn't load chrome style overlay.");
                    continue;
                }

                AddStyleSheet(sheet, 0);
            }
        }

        if (useXULCache) {
            // If it's a 'chrome:' prototype document, then notify any
            // documents that raced to load the prototype, and awaited
            // its load completion via proto->AwaitLoadDone().
            rv = mCurrentPrototype->NotifyLoadDone();
            if (NS_FAILED(rv)) return;
        }
    }

    // Now walk the prototype to build content.
    rv = PrepareToWalk();
    if (NS_FAILED(rv)) return;

    ResumeWalk();
}

// Called back from nsXULPrototypeDocument::NotifyLoadDone for each XUL
// document that raced to start the same prototype document load, lost
// the race, but hit the XUL prototype cache because the winner filled
// the cache with the not-yet-loaded prototype object.
NS_IMETHODIMP
nsXULDocument::OnPrototypeLoadDone()
{
    nsresult rv;

    // Need to clone style sheet references now, as we couldn't do that
    // in StartDocumentLoad, because the prototype may not have finished
    // loading at that point.
    rv = AddPrototypeSheets();
    if (NS_FAILED(rv)) return rv;

    // Now we must do for each secondary or later XUL document (those that
    // lost the race to start the prototype document load) what is done by
    // nsCachedChromeStreamListener::OnStopRequest for the primary document
    // (the one that won the race to start the prototype load).
    rv = PrepareToWalk();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to prepare for walk");
    if (NS_FAILED(rv)) return rv;

    return ResumeWalk();
}


PR_STATIC_CALLBACK(PRBool)
ClearPresentationStuff(nsHashKey *aKey, void *aData, void* aClosure)
{
    nsISupports *supp = NS_STATIC_CAST(nsISupports *, aData);
    nsCOMPtr<nsPIBoxObject> boxObject(do_QueryInterface(supp));

    if (boxObject) {
        boxObject->InvalidatePresentationStuff();
    }

    return PR_TRUE;
}

NS_IMETHODIMP
nsXULDocument::OnHide()
{
    if (mBoxObjectTable) {
        mBoxObjectTable->Enumerate(ClearPresentationStuff, nsnull);
    }

    return NS_OK;
}

PR_STATIC_CALLBACK(void)
ClearBroadcasterMapEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
    BroadcasterMapEntry* entry =
        NS_STATIC_CAST(BroadcasterMapEntry*, aEntry);
    for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
        delete (BroadcastListener*)entry->mListeners[i];
    }

    // N.B. that we need to manually run the dtor because we
    // constructed the nsSmallVoidArray object in-place.
    entry->mListeners.~nsSmallVoidArray();
}

static PRBool
CanBroadcast(PRInt32 aNameSpaceID, nsIAtom* aAttribute)
{
    // Don't push changes to the |id|, |ref|, or |persist| attribute.
    if (aNameSpaceID == kNameSpaceID_None) {
        if ((aAttribute == nsXULAtoms::id) ||
            (aAttribute == nsXULAtoms::ref) ||
            (aAttribute == nsXULAtoms::persist)) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

void
nsXULDocument::SynchronizeBroadcastListener(nsIDOMElement   *aBroadcaster,
                                            nsIDOMElement   *aListener,
                                            const nsAString &aAttr)
{
    nsCOMPtr<nsIContent> broadcaster = do_QueryInterface(aBroadcaster);
    nsCOMPtr<nsIContent> listener = do_QueryInterface(aListener);

    if (aAttr.EqualsLiteral("*")) {
        PRUint32 count = broadcaster->GetAttrCount();
        while (count-- > 0) {
            PRInt32 nameSpaceID;
            nsCOMPtr<nsIAtom> name;
            nsCOMPtr<nsIAtom> prefix;
            broadcaster->GetAttrNameAt(count, &nameSpaceID,
                                       getter_AddRefs(name),
                                       getter_AddRefs(prefix));

            // _Don't_ push the |id|, |ref|, or |persist| attribute's value!
            if (! CanBroadcast(nameSpaceID, name))
                continue;

            nsAutoString value;
            broadcaster->GetAttr(nameSpaceID, name, value);
            listener->SetAttr(nameSpaceID, name, prefix, value, PR_FALSE);

#if 0
            // XXX we don't fire the |onbroadcast| handler during
            // initial hookup: doing so would potentially run the
            // |onbroadcast| handler before the |onload| handler,
            // which could define JS properties that mask XBL
            // properties, etc.
            ExecuteOnBroadcastHandlerFor(broadcaster, aListener, name);
#endif
        }
    }
    else {
        // Find out if the attribute is even present at all.
        nsCOMPtr<nsIAtom> name = do_GetAtom(aAttr);

        nsAutoString value;
        nsresult rv = broadcaster->GetAttr(kNameSpaceID_None, name, value);

        if (rv == NS_CONTENT_ATTR_NO_VALUE ||
            rv == NS_CONTENT_ATTR_HAS_VALUE) {
            listener->SetAttr(kNameSpaceID_None, name, value, PR_FALSE);
        }
        else {
            listener->UnsetAttr(kNameSpaceID_None, name, PR_FALSE);
        }

#if 0
        // XXX we don't fire the |onbroadcast| handler during initial
        // hookup: doing so would potentially run the |onbroadcast|
        // handler before the |onload| handler, which could define JS
        // properties that mask XBL properties, etc.
        ExecuteOnBroadcastHandlerFor(broadcaster, aListener, name);
#endif
    }
}

NS_IMETHODIMP
nsXULDocument::AddBroadcastListenerFor(nsIDOMElement* aBroadcaster,
                                       nsIDOMElement* aListener,
                                       const nsAString& aAttr)
{
    NS_ENSURE_ARG(aBroadcaster && aListener);
    
    nsresult rv =
        nsContentUtils::CheckSameOrigin(NS_STATIC_CAST(nsDocument *, this),
                                        aBroadcaster);

    if (NS_FAILED(rv)) {
        return rv;
    }

    rv = nsContentUtils::CheckSameOrigin(NS_STATIC_CAST(nsDocument *, this),
                                         aListener);

    if (NS_FAILED(rv)) {
        return rv;
    }

    static PLDHashTableOps gOps = {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        PL_DHashGetKeyStub,
        PL_DHashVoidPtrKeyStub,
        PL_DHashMatchEntryStub,
        PL_DHashMoveEntryStub,
        ClearBroadcasterMapEntry,
        PL_DHashFinalizeStub,
        nsnull
    };

    if (! mBroadcasterMap) {
        mBroadcasterMap =
            PL_NewDHashTable(&gOps, nsnull, sizeof(BroadcasterMapEntry),
                             PL_DHASH_MIN_SIZE);

        if (! mBroadcasterMap)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    BroadcasterMapEntry* entry =
        NS_STATIC_CAST(BroadcasterMapEntry*,
                       PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(entry)) {
        entry =
            NS_STATIC_CAST(BroadcasterMapEntry*,
                           PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                                PL_DHASH_ADD));

        if (! entry)
            return NS_ERROR_OUT_OF_MEMORY;

        entry->mBroadcaster = aBroadcaster;

        // N.B. placement new to construct the nsSmallVoidArray object
        // in-place
        new (&entry->mListeners) nsSmallVoidArray();
    }

    // Only add the listener if it's not there already!
    nsCOMPtr<nsIAtom> attr = do_GetAtom(aAttr);

    BroadcastListener* bl;
    for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
        bl = NS_STATIC_CAST(BroadcastListener*, entry->mListeners[i]);

        if ((bl->mListener == aListener) && (bl->mAttribute == attr))
            return NS_OK;
    }

    bl = new BroadcastListener;
    if (! bl)
        return NS_ERROR_OUT_OF_MEMORY;

    bl->mListener  = aListener;
    bl->mAttribute = attr;

    entry->mListeners.AppendElement(bl);

    SynchronizeBroadcastListener(aBroadcaster, aListener, aAttr);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::RemoveBroadcastListenerFor(nsIDOMElement* aBroadcaster,
                                          nsIDOMElement* aListener,
                                          const nsAString& aAttr)
{
    // If we haven't added any broadcast listeners, then there sure
    // aren't any to remove.
    if (! mBroadcasterMap)
        return NS_OK;

    BroadcasterMapEntry* entry =
        NS_STATIC_CAST(BroadcasterMapEntry*,
                       PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
        nsCOMPtr<nsIAtom> attr = do_GetAtom(aAttr);
        for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
            BroadcastListener* bl =
                NS_STATIC_CAST(BroadcastListener*, entry->mListeners[i]);

            if ((bl->mListener == aListener) && (bl->mAttribute == attr)) {
                entry->mListeners.RemoveElementAt(i);
                delete bl;

                if (entry->mListeners.Count() == 0)
                    PL_DHashTableOperate(mBroadcasterMap, aBroadcaster,
                                         PL_DHASH_REMOVE);

                SynchronizeBroadcastListener(aBroadcaster, aListener, aAttr);

                break;
            }
        }
    }

    return NS_OK;
}

nsresult
nsXULDocument::ExecuteOnBroadcastHandlerFor(nsIContent* aBroadcaster,
                                            nsIDOMElement* aListener,
                                            nsIAtom* aAttr)
{
    // Now we execute the onchange handler in the context of the
    // observer. We need to find the observer in order to
    // execute the handler.

    nsCOMPtr<nsIContent> listener = do_QueryInterface(aListener);
    PRUint32 count = listener->GetChildCount();
    for (PRUint32 i = 0; i < count; ++i) {
        // Look for an <observes> element beneath the listener. This
        // ought to have an |element| attribute that refers to
        // aBroadcaster, and an |attribute| element that tells us what
        // attriubtes we're listening for.
        nsIContent *child = listener->GetChildAt(i);

        nsINodeInfo *ni = child->GetNodeInfo();
        if (!ni || !ni->Equals(nsXULAtoms::observes, kNameSpaceID_XUL))
            continue;

        // Is this the element that was listening to us?
        nsAutoString listeningToID;
        child->GetAttr(kNameSpaceID_None, nsXULAtoms::element, listeningToID);

        nsAutoString broadcasterID;
        aBroadcaster->GetAttr(kNameSpaceID_None, nsXULAtoms::id, broadcasterID);

        if (listeningToID != broadcasterID)
            continue;

        // We are observing the broadcaster, but is this the right
        // attribute?
        nsAutoString listeningToAttribute;
        child->GetAttr(kNameSpaceID_None, nsXULAtoms::attribute,
                       listeningToAttribute);

        if (!aAttr->Equals(listeningToAttribute) &&
            !listeningToAttribute.EqualsLiteral("*")) {
            continue;
        }

        // This is the right <observes> element. Execute the
        // |onbroadcast| event handler
        nsEvent event(NS_XUL_BROADCAST);

        PRInt32 j = mPresShells.Count();
        while (--j >= 0) {
            nsCOMPtr<nsIPresShell> shell =
                NS_STATIC_CAST(nsIPresShell*, mPresShells[j]);

            nsCOMPtr<nsIPresContext> aPresContext;
            shell->GetPresContext(getter_AddRefs(aPresContext));

            // Handle the DOM event
            nsEventStatus status = nsEventStatus_eIgnore;
            child->HandleDOMEvent(aPresContext, &event, nsnull,
                                  NS_EVENT_FLAG_INIT, &status);
        }
    }

    return NS_OK;
}

void
nsXULDocument::AttributeChanged(nsIContent* aElement, PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute, PRInt32 aModType)
{
    nsresult rv;

    // First see if we need to update our element map.
    if ((aAttribute == nsXULAtoms::id) || (aAttribute == nsXULAtoms::ref)) {

        rv = mElementMap.Enumerate(RemoveElementsFromMapByContent, aElement);
        if (NS_FAILED(rv)) return;

        // That'll have removed _both_ the 'ref' and 'id' entries from
        // the map. So add 'em back now.
        rv = AddElementToMap(aElement);
        if (NS_FAILED(rv)) return;
    }

    // Synchronize broadcast listeners
    if (mBroadcasterMap && CanBroadcast(aNameSpaceID, aAttribute)) {
        nsCOMPtr<nsIDOMElement> domele = do_QueryInterface(aElement);
        BroadcasterMapEntry* entry =
            NS_STATIC_CAST(BroadcasterMapEntry*,
                           PL_DHashTableOperate(mBroadcasterMap, domele.get(),
                                                PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
            // We've got listeners: push the value.
            nsAutoString value;
            rv = aElement->GetAttr(kNameSpaceID_None, aAttribute, value);

            for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
                BroadcastListener* bl =
                    NS_STATIC_CAST(BroadcastListener*, entry->mListeners[i]);

                if ((bl->mAttribute == aAttribute) ||
                    (bl->mAttribute == nsXULAtoms::_star)) {
                    nsCOMPtr<nsIContent> listener
                        = do_QueryInterface(bl->mListener);

                    if (rv == NS_CONTENT_ATTR_NO_VALUE ||
                        rv == NS_CONTENT_ATTR_HAS_VALUE) {
                        listener->SetAttr(kNameSpaceID_None, aAttribute, value,
                                          PR_TRUE);
                    }
                    else {
                        listener->UnsetAttr(kNameSpaceID_None, aAttribute,
                                            PR_TRUE);
                    }

                    ExecuteOnBroadcastHandlerFor(aElement, bl->mListener,
                                                 aAttribute);
                }
            }
        }
    }

    // Now notify external observers
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->AttributeChanged(this, aElement, aNameSpaceID, aAttribute,
                                   aModType);
    }

    // See if there is anything we need to persist in the localstore.
    //
    // XXX Namespace handling broken :-(
    nsAutoString persist;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::persist, persist);
    if (NS_FAILED(rv)) return;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsAutoString attr;
        rv = aAttribute->ToString(attr);
        if (NS_FAILED(rv)) return;

        if (persist.Find(attr) >= 0) {
            rv = Persist(aElement, kNameSpaceID_None, aAttribute);
            if (NS_FAILED(rv)) return;
        }
    }
}

void
nsXULDocument::ContentAppended(nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
    // First update our element map
    PRUint32 count = aContainer->GetChildCount();

    for (PRUint32 i = aNewIndexInContainer; i < count; ++i) {
        nsresult rv = AddSubtreeToDocument(aContainer->GetChildAt(i));
        if (NS_FAILED(rv))
            return;
    }

    nsXMLDocument::ContentAppended(aContainer, aNewIndexInContainer);
}

void
nsXULDocument::ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer)
{
    nsresult rv = AddSubtreeToDocument(aChild);
    if (NS_FAILED(rv))
        return;

    nsXMLDocument::ContentInserted(aContainer, aChild,
                                   aIndexInContainer);
}

void
nsXULDocument::ContentRemoved(nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
    nsresult rv;
    rv = RemoveSubtreeFromDocument(aChild);
    if (NS_FAILED(rv))
        return;

    nsXMLDocument::ContentRemoved(aContainer, aChild,
                                  aIndexInContainer);
}

nsresult
nsXULDocument::HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus)
{
    nsresult ret = NS_OK;
    nsIDOMEvent* domEvent = nsnull;
    PRBool externalDOMEvent = PR_FALSE;

    if (NS_EVENT_FLAG_INIT & aFlags) {
        if (aDOMEvent) {
            if (*aDOMEvent) {
                externalDOMEvent = PR_TRUE;
            }
        }
        else {
            aDOMEvent = &domEvent;
        }
        aEvent->flags |= aFlags;
        aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
        aFlags |= NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE;
    }

    // Capturing stage
    if (NS_EVENT_FLAG_CAPTURE & aFlags && mScriptGlobalObject) {
        mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                            aFlags & NS_EVENT_CAPTURE_MASK,
                                            aEventStatus);
    }

    // Local handling stage
    if (mListenerManager) {
        aEvent->flags |= aFlags;
        mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent,
                                      this, aFlags, aEventStatus);
        aEvent->flags &= ~aFlags;
    }

    // Bubbling stage
    if (NS_EVENT_FLAG_BUBBLE & aFlags && mScriptGlobalObject) {
        mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                            aFlags & NS_EVENT_BUBBLE_MASK,
                                            aEventStatus);
    }

    if (NS_EVENT_FLAG_INIT & aFlags) {
        // We're leaving the DOM event loop so if we created a DOM
        // event, release here.
        if (*aDOMEvent && !externalDOMEvent) {
            nsrefcnt rc;
            NS_RELEASE2(*aDOMEvent, rc);
            if (0 != rc) {
                // Okay, so someone in the DOM loop (a listener, JS
                // object) still has a ref to the DOM Event but the
                // internal data hasn't been malloc'd.  Force a copy
                // of the data here so the DOM Event is still valid.
                nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
                    do_QueryInterface(*aDOMEvent);

                if (privateEvent) {
                    privateEvent->DuplicatePrivateData();
                }
            }
        }
        aDOMEvent = nsnull;
    }

    return ret;
}


//----------------------------------------------------------------------
//
// nsIXULDocument interface
//

NS_IMETHODIMP
nsXULDocument::AddElementForID(const nsAString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Add(aID, aElement);
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::RemoveElementForID(const nsAString& aID, nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Remove(aID, aElement);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetElementsForID(const nsAString& aID,
                                nsISupportsArray* aElements)
{
    NS_PRECONDITION(aElements != nsnull, "null ptr");
    if (! aElements)
        return NS_ERROR_NULL_POINTER;

    mElementMap.Find(aID, aElements);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AddForwardReference(nsForwardReference* aRef)
{
    if (mResolutionPhase < aRef->GetPhase()) {
        mForwardReferences.AppendElement(aRef);
    }
    else {
        NS_ERROR("forward references have already been resolved");
        delete aRef;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::ResolveForwardReferences()
{
    if (mResolutionPhase == nsForwardReference::eDone)
        return NS_OK;

    // Resolve each outstanding 'forward' reference. We iterate
    // through the list of forward references until no more forward
    // references can be resolved. This annealing process is
    // guaranteed to converge because we've "closed the gate" to new
    // forward references.

    const nsForwardReference::Phase* pass = nsForwardReference::kPasses;
    while ((mResolutionPhase = *pass) != nsForwardReference::eDone) {
        PRInt32 previous = 0;
        while (mForwardReferences.Count() && mForwardReferences.Count() != previous) {
            previous = mForwardReferences.Count();

            for (PRInt32 i = 0; i < mForwardReferences.Count(); ++i) {
                nsForwardReference* fwdref = NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);

                if (fwdref->GetPhase() == *pass) {
                    nsForwardReference::Result result = fwdref->Resolve();

                    switch (result) {
                    case nsForwardReference::eResolve_Succeeded:
                    case nsForwardReference::eResolve_Error:
                        mForwardReferences.RemoveElementAt(i);
                        delete fwdref;

                        // fixup because we removed from list
                        --i;
                        break;

                    case nsForwardReference::eResolve_Later:
                        // do nothing. we'll try again later
                        ;
                    }
                }
            }
        }

        ++pass;
    }

    DestroyForwardReferences();
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetMasterPrototype(nsIXULPrototypeDocument* aDocument)
{
    mMasterPrototype = aDocument;
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetMasterPrototype(nsIXULPrototypeDocument** aDocument)
{
    *aDocument = mMasterPrototype;
    NS_IF_ADDREF(*aDocument);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetCurrentPrototype(nsIXULPrototypeDocument* aDocument)
{
    mCurrentPrototype = aDocument;
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIDOMDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetElementsByAttribute(const nsAString& aAttribute,
                                      const nsAString& aValue,
                                      nsIDOMNodeList** aReturn)
{
    nsCOMPtr<nsIAtom> attrAtom(do_GetAtom(aAttribute));
    NS_ENSURE_TRUE(attrAtom, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIContentList> list = new nsContentList(this,
                                                      MatchAttribute,
                                                      aValue,
                                                      nsnull,
                                                      PR_TRUE,
                                                      attrAtom,
                                                      kNameSpaceID_None);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    return CallQueryInterface(list, aReturn);
}


NS_IMETHODIMP
nsXULDocument::Persist(const nsAString& aID,
                       const nsAString& aAttr)
{
    // If we're currently reading persisted attributes out of the
    // localstore, _don't_ re-enter and try to set them again!
    if (mApplyingPersistedAttrs)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIDOMElement> domelement;
    rv = GetElementById(aID, getter_AddRefs(domelement));
    if (NS_FAILED(rv)) return rv;

    if (! domelement)
        return NS_OK;

    nsCOMPtr<nsIContent> element = do_QueryInterface(domelement);
    NS_ASSERTION(element != nsnull, "null ptr");
    if (! element)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIAtom> tag;
    PRInt32 nameSpaceID;

    nsCOMPtr<nsINodeInfo> ni = element->GetExistingAttrNameFromQName(aAttr);
    if (ni) {
        tag = ni->NameAtom();
        nameSpaceID = ni->NamespaceID();
    }
    else {
        tag = do_GetAtom(aAttr);
        NS_ENSURE_TRUE(tag, NS_ERROR_OUT_OF_MEMORY);

        nameSpaceID = kNameSpaceID_None;
    }

    rv = Persist(element, nameSpaceID, tag);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsXULDocument::Persist(nsIContent* aElement, PRInt32 aNameSpaceID,
                       nsIAtom* aAttribute)
{
    // First make sure we _have_ a local store to stuff the persisted
    // information into. (We might not have one if profile information
    // hasn't been loaded yet...)
    if (! mLocalStore)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIRDFResource> element;
    rv = nsXULContentUtils::GetElementResource(aElement, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    // No ID, so nothing to persist.
    if (! element)
        return NS_OK;

    // Ick. Construct a property from the attribute. Punt on
    // namespaces for now.
    const char* attrstr;
    rv = aAttribute->GetUTF8String(&attrstr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> attr;
    rv = gRDFService->GetResource(nsDependentCString(attrstr),
                                  getter_AddRefs(attr));
    if (NS_FAILED(rv)) return rv;

    // Turn the value into a literal
    nsAutoString valuestr;
    rv = aElement->GetAttr(kNameSpaceID_None, aAttribute, valuestr);
    if (NS_FAILED(rv)) return rv;

    PRBool novalue = (rv != NS_CONTENT_ATTR_HAS_VALUE);

    // See if there was an old value...
    nsCOMPtr<nsIRDFNode> oldvalue;
    rv = mLocalStore->GetTarget(element, attr, PR_TRUE, getter_AddRefs(oldvalue));
    if (NS_FAILED(rv)) return rv;

    if (oldvalue && novalue) {
        // ...there was an oldvalue, and they've removed it. XXXThis
        // handling isn't quite right...
        rv = mLocalStore->Unassert(element, attr, oldvalue);
    }
    else {
        // Now either 'change' or 'assert' based on whether there was
        // an old value.
        nsCOMPtr<nsIRDFLiteral> newvalue;
        rv = gRDFService->GetLiteral(valuestr.get(), getter_AddRefs(newvalue));
        if (NS_FAILED(rv)) return rv;

        if (oldvalue) {
            if (oldvalue != newvalue)
                rv = mLocalStore->Change(element, attr, oldvalue, newvalue);
            else
                rv = NS_OK;
        }
        else {
            rv = mLocalStore->Assert(element, attr, newvalue, PR_TRUE);
        }
    }

    if (NS_FAILED(rv)) return rv;

    // Add it to the persisted set for this document (if it's not
    // there already).
    {
        nsCAutoString docurl;
        rv = mDocumentURI->GetSpec(docurl);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> doc;
        rv = gRDFService->GetResource(docurl, getter_AddRefs(doc));
        if (NS_FAILED(rv)) return rv;

        PRBool hasAssertion;
        rv = mLocalStore->HasAssertion(doc, kNC_persist, element, PR_TRUE, &hasAssertion);
        if (NS_FAILED(rv)) return rv;

        if (! hasAssertion) {
            rv = mLocalStore->Assert(doc, kNC_persist, element, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}



nsresult
nsXULDocument::DestroyForwardReferences()
{
    for (PRInt32 i = mForwardReferences.Count() - 1; i >= 0; --i) {
        nsForwardReference* fwdref =
            NS_REINTERPRET_CAST(nsForwardReference*, mForwardReferences[i]);
        delete fwdref;
    }

    mForwardReferences.Clear();
    return NS_OK;
}


nsresult
nsXULDocument::GetPixelDimensions(nsIPresShell* aShell, PRInt32* aWidth,
                                  PRInt32* aHeight)
{
    nsresult result = NS_OK;
    nsSize size;
    nsIFrame* frame;

    FlushPendingNotifications(Flush_Layout);

    result = aShell->GetPrimaryFrameFor(mRootContent, &frame);
    if (NS_SUCCEEDED(result) && frame) {
        nsIView* view = frame->GetView();
        // If we have a view check if it's scrollable. If not,
        // just use the view size itself
        if (view) {
            nsIScrollableView* scrollableView;

            if (NS_SUCCEEDED(CallQueryInterface(view, &scrollableView))) {
                scrollableView->GetScrolledView(view);
            }

            nsRect r = view->GetBounds();
            size.height = r.height;
            size.width = r.width;
        }
        // If we don't have a view, use the frame size
        else {
            size = frame->GetSize();
        }

        // Convert from twips to pixels
        nsCOMPtr<nsIPresContext> context;
        result = aShell->GetPresContext(getter_AddRefs(context));

        if (NS_SUCCEEDED(result)) {
            float scale;
            scale = context->TwipsToPixels();

            *aWidth = NSTwipsToIntPixels(size.width, scale);
            *aHeight = NSTwipsToIntPixels(size.height, scale);
        }
    }
    else {
        *aWidth = 0;
        *aHeight = 0;
    }

    return result;
}

NS_IMETHODIMP
nsXULDocument::GetWidth(PRInt32* aWidth)
{
    NS_ENSURE_ARG_POINTER(aWidth);

    nsresult rv = NS_OK;

    // We make the assumption that the first presentation shell
    // is the one for which we need information.
    nsIPresShell *shell = GetShellAt(0);
    if (shell) {
        PRInt32 width, height;

        rv = GetPixelDimensions(shell, &width, &height);
        *aWidth = width;
    } else
        *aWidth = 0;

    return rv;
}

NS_IMETHODIMP
nsXULDocument::GetHeight(PRInt32* aHeight)
{
    NS_ENSURE_ARG_POINTER(aHeight);

    nsresult rv = NS_OK;

    // We make the assumption that the first presentation shell
    // is the one for which we need information.
    nsIPresShell *shell = GetShellAt(0);
    if (shell) {
        PRInt32 width, height;

        rv = GetPixelDimensions(shell, &width, &height);
        *aHeight = height;
    } else
        *aHeight = 0;

    return rv;
}

//----------------------------------------------------------------------
//
// nsIDOMXULDocument interface
//

NS_IMETHODIMP
nsXULDocument::GetPopupNode(nsIDOMNode** aNode)
{
    nsresult rv;

    // get focus controller
    nsCOMPtr<nsIFocusController> focusController;
    GetFocusController(getter_AddRefs(focusController));
    NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);
    // get popup node
    rv = focusController->GetPopupNode(aNode); // addref happens here

    return rv;
}

NS_IMETHODIMP
nsXULDocument::SetPopupNode(nsIDOMNode* aNode)
{
    nsresult rv;

    // get focus controller
    nsCOMPtr<nsIFocusController> focusController;
    GetFocusController(getter_AddRefs(focusController));
    NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);
    // set popup node
    rv = focusController->SetPopupNode(aNode);

    return rv;
}

NS_IMETHODIMP
nsXULDocument::GetTooltipNode(nsIDOMNode** aNode)
{
    *aNode = mTooltipNode;
    NS_IF_ADDREF(*aNode);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetTooltipNode(nsIDOMNode* aNode)
{
    mTooltipNode = aNode;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::GetCommandDispatcher(nsIDOMXULCommandDispatcher** aTracker)
{
    *aTracker = mCommandDispatcher;
    NS_IF_ADDREF(*aTracker);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetElementById(const nsAString& aId,
                              nsIDOMElement** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    *aReturn = nsnull;

    NS_WARN_IF_FALSE(!aId.IsEmpty(),"getElementById(\"\"), fix caller?");
    if (aId.IsEmpty())
      return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIContent> element;
    rv = mElementMap.FindFirst(aId, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    if (element) {
        rv = CallQueryInterface(element, aReturn);
    }

    return rv;
}

nsresult
nsXULDocument::AddElementToDocumentPre(nsIContent* aElement)
{
    // Do a bunch of work that's necessary when an element gets added
    // to the XUL Document.
    nsresult rv;

    // 1. Add the element to the resource-to-element map
    rv = AddElementToMap(aElement);
    if (NS_FAILED(rv)) return rv;

    // 2. If the element is a 'command updater' (i.e., has a
    // "commandupdater='true'" attribute), then add the element to the
    // document's command dispatcher
    nsAutoString value;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::commandupdater,
                           value);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
        value.EqualsLiteral("true")) {
        rv = nsXULContentUtils::SetCommandUpdater(this, aElement);
        if (NS_FAILED(rv)) return rv;
    }

    // 3. Check for a broadcaster hookup attribute, in which case
    // we'll hook the node up as a listener on a broadcaster.
    PRBool listener, resolved;
    rv = CheckBroadcasterHookup(this, aElement, &listener, &resolved);
    if (NS_FAILED(rv)) return rv;

    // If it's not there yet, we may be able to defer hookup until
    // later.
    if (listener && !resolved && (mResolutionPhase != nsForwardReference::eDone)) {
        BroadcasterHookup* hookup = new BroadcasterHookup(this, aElement);
        if (! hookup)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = AddForwardReference(hookup);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsXULDocument::AddElementToDocumentPost(nsIContent* aElement)
{
    nsINodeInfo *ni = aElement->GetNodeInfo();

    // We need to pay special attention to the keyset tag to set up a listener
    if (ni && ni->Equals(nsXULAtoms::keyset, kNameSpaceID_XUL)) {
        // Create our XUL key listener and hook it up.
        nsCOMPtr<nsIXBLService> xblService(do_GetService("@mozilla.org/xbl;1"));
        if (xblService) {
            nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(aElement));
            xblService->AttachGlobalKeyHandler(rec);
        }
    }

    // See if we need to attach a XUL template to this node
    PRBool needsHookup;
    nsresult rv = CheckTemplateBuilderHookup(aElement, &needsHookup);
    if (NS_FAILED(rv))
        return rv;

    if (needsHookup) {
        if (mResolutionPhase == nsForwardReference::eDone) {
            rv = CreateTemplateBuilder(aElement);
            if (NS_FAILED(rv))
                return rv;
        }
        else {
            TemplateBuilderHookup* hookup = new TemplateBuilderHookup(aElement);
            if (! hookup)
                return NS_ERROR_OUT_OF_MEMORY;

            rv = AddForwardReference(hookup);
            if (NS_FAILED(rv))
                return rv;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::AddSubtreeToDocument(nsIContent* aElement)
{
    nsresult rv;

    // Do pre-order addition magic
    rv = AddElementToDocumentPre(aElement);
    if (NS_FAILED(rv)) return rv;

    // Recurse to children
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);

    PRUint32 count =
        xulcontent ? xulcontent->PeekChildCount() : aElement->GetChildCount();

    while (count-- > 0) {
        rv = AddSubtreeToDocument(aElement->GetChildAt(count));
        if (NS_FAILED(rv))
            return rv;
    }

    // Do post-order addition magic
    return AddElementToDocumentPost(aElement);
}

NS_IMETHODIMP
nsXULDocument::RemoveSubtreeFromDocument(nsIContent* aElement)
{
    // Do a bunch of cleanup to remove an element from the XUL
    // document.
    nsresult rv;

    // 1. Remove any children from the document.
    PRUint32 count = aElement->GetChildCount();

    while (count-- > 0) {
        rv = RemoveSubtreeFromDocument(aElement->GetChildAt(count));
        if (NS_FAILED(rv))
            return rv;
    }

    // 2. Remove the element from the resource-to-element map
    rv = RemoveElementFromMap(aElement);
    if (NS_FAILED(rv)) return rv;

    // 3. If the element is a 'command updater', then remove the
    // element from the document's command dispatcher.
    nsAutoString value;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::commandupdater,
                           value);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
        value.EqualsLiteral("true")) {
        nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(aElement);
        NS_ASSERTION(domelement != nsnull, "not a DOM element");
        if (! domelement)
            return NS_ERROR_UNEXPECTED;

        rv = mCommandDispatcher->RemoveCommandUpdater(domelement);
        if (NS_FAILED(rv)) return rv;
    }

    // 4. Remove the element from our broadcaster map, since it is no longer
    // in the document.
    // Do a getElementById to retrieve the broadcaster
    nsCOMPtr<nsIDOMElement> broadcaster;
    nsAutoString observesVal;

    if (aElement->HasAttr(kNameSpaceID_None, nsXULAtoms::observes)) {
        aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::observes, observesVal);
        if (!observesVal.IsEmpty()) {
            GetElementById(observesVal, getter_AddRefs(broadcaster));
            if (broadcaster) {
                nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aElement));
                RemoveBroadcastListenerFor(broadcaster, elt,
                                           NS_LITERAL_STRING("*"));
            }
        }
    }

    if (aElement->HasAttr(kNameSpaceID_None, nsXULAtoms::command)) {
        aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::command, observesVal);
        if (!observesVal.IsEmpty()) {
            GetElementById(observesVal, getter_AddRefs(broadcaster));
            if (broadcaster) {
                nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aElement));
                RemoveBroadcastListenerFor(broadcaster, elt,
                                           NS_LITERAL_STRING("*"));
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetTemplateBuilderFor(nsIContent* aContent,
                                     nsIXULTemplateBuilder* aBuilder)
{
    if (! mTemplateBuilderTable) {
        mTemplateBuilderTable = new nsSupportsHashtable();
        if (! mTemplateBuilderTable)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    nsISupportsKey key(aContent);

    if (aContent) {
        mTemplateBuilderTable->Put(&key, aBuilder);
    }
    else {
        mTemplateBuilderTable->Remove(&key);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetTemplateBuilderFor(nsIContent* aContent,
                                     nsIXULTemplateBuilder** aResult)
{
    if (mTemplateBuilderTable) {
        nsISupportsKey key(aContent);
        *aResult = NS_STATIC_CAST(nsIXULTemplateBuilder*,
                                  mTemplateBuilderTable->Get(&key));
    }
    else
        *aResult = nsnull;

    return NS_OK;
}

// Attributes that are used with getElementById() and the
// resource-to-element map.
nsIAtom** nsXULDocument::kIdentityAttrs[] =
{
    &nsXULAtoms::id,
    &nsXULAtoms::ref,
    nsnull
};

nsresult
nsXULDocument::AddElementToMap(nsIContent* aElement)
{
    // Look at the element's 'id' and 'ref' attributes, and if set,
    // add pointers in the resource-to-element map to the element.
    nsresult rv;

    for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
        nsAutoString value;
        rv = aElement->GetAttr(kNameSpaceID_None, *kIdentityAttrs[i], value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            rv = mElementMap.Add(value, aElement);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


nsresult
nsXULDocument::RemoveElementFromMap(nsIContent* aElement)
{
    // Remove the element from the resource-to-element map.
    nsresult rv;

    for (PRInt32 i = 0; kIdentityAttrs[i] != nsnull; ++i) {
        nsAutoString value;
        rv = aElement->GetAttr(kNameSpaceID_None, *kIdentityAttrs[i], value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get attribute");
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            rv = mElementMap.Remove(value, aElement);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


PRIntn
nsXULDocument::RemoveElementsFromMapByContent(const PRUnichar* aID,
                                              nsIContent* aElement,
                                              void* aClosure)
{
    nsIContent* content = NS_REINTERPRET_CAST(nsIContent*, aClosure);
    return (aElement == content) ? HT_ENUMERATE_REMOVE : HT_ENUMERATE_NEXT;
}



//----------------------------------------------------------------------
//
// nsIDOMNode interface
//

NS_IMETHODIMP
nsXULDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    // We don't allow cloning of a document
    *aReturn = nsnull;
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
nsXULDocument::Init()
{
    nsresult rv = nsXMLDocument::Init();
    NS_ENSURE_SUCCESS(rv, rv);

    // Create our command dispatcher and hook it up.
    rv = nsXULCommandDispatcher::Create(this,
                                        getter_AddRefs(mCommandDispatcher));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a focus tracker");
    if (NS_FAILED(rv)) return rv;

    // this _could_ fail; e.g., if we've tried to grab the local store
    // before profiles have initialized. If so, no big deal; nothing
    // will persist.
    mLocalStore = do_GetService(kLocalStoreCID);

    // Create a new nsISupportsArray for dealing with overlay references
    rv = NS_NewISupportsArray(getter_AddRefs(mUnloadedOverlays));
    if (NS_FAILED(rv)) return rv;

    if (gRefCnt++ == 0) {
        // Keep the RDF service cached in a member variable to make using
        // it a bit less painful
        rv = CallGetService(kRDFServiceCID, &gRDFService);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF Service");
        if (NS_FAILED(rv)) return rv;

        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "persist"),
                                 &kNC_persist);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "attribute"),
                                 &kNC_attribute);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "value"),
                                 &kNC_value);

        rv = CallGetService(kXULPrototypeCacheCID, &gXULCache);
        if (NS_FAILED(rv)) return rv;
    }

#ifdef PR_LOGGING
    if (! gXULLog)
        gXULLog = PR_NewLogModule("nsXULDocument");
#endif

    return NS_OK;
}


nsresult
nsXULDocument::StartLayout(void)
{
    if (!mRootContent) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING)) {
            nsCAutoString urlspec;
            mDocumentURI->GetSpec(urlspec);

            PR_LOG(gXULLog, PR_LOG_WARNING,
                   ("xul: unable to layout '%s'; no root content", urlspec.get()));
        }
#endif
        return NS_OK;
    }

    PRUint32 count = GetNumberOfShells();
    for (PRUint32 i = 0; i < count; ++i) {
        nsIPresShell *shell = GetShellAt(i);

        // Resize-reflow this time
        nsCOMPtr<nsIPresContext> cx;
        shell->GetPresContext(getter_AddRefs(cx));
        NS_ASSERTION(cx != nsnull, "no pres context");
        if (! cx)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsISupports> container = cx->GetContainer();
        NS_ASSERTION(container != nsnull, "pres context has no container");
        if (! container)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
        NS_ASSERTION(docShell != nsnull, "container is not a docshell");
        if (! docShell)
            return NS_ERROR_UNEXPECTED;

        nsRect r = cx->GetVisibleArea();

        // Trigger a refresh before the call to InitialReflow(),
        // because the view manager's UpdateView() function is
        // dropping dirty rects if refresh is disabled rather than
        // accumulating them until refresh is enabled and then
        // triggering a repaint...
        nsIViewManager* vm = shell->GetViewManager();
        if (vm) {
            nsCOMPtr<nsIContentViewer> contentViewer;
            nsresult rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
            if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
                PRBool enabled;
                contentViewer->GetEnableRendering(&enabled);
                if (enabled) {
                    vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
                }
            }
        }

        shell->InitialReflow(r.width, r.height);

        // This is done because iframes don't load their subdocs until
        // they get reflowed.  If we reflow asynchronously, our onload
        // will fire too early. -- hyatt
        // XXXbz this is still needed for <xul:iframe>.  We need to fix that.
        FlushPendingNotifications(Flush_Layout);

        // Start observing the document _after_ we do the initial
        // reflow. Otherwise, we'll get into an trouble trying to
        // create kids before the root frame is established.
        shell->BeginObservingDocument();
    }

    return NS_OK;
}


/* static */
PRBool
nsXULDocument::MatchAttribute(nsIContent* aContent,
                              PRInt32 aNamespaceID,
                              nsIAtom* aAttrName,
                              const nsAString& aAttrValue)
{
    NS_PRECONDITION(aContent, "Must have content node to work with!");
  
    // Getting attrs is expensive, so use HasAttr() first.
    if (!aContent->HasAttr(aNamespaceID, aAttrName)) {
        return PR_FALSE;
    }

    if (aAttrValue.EqualsLiteral("*")) {
        // Wildcard.  We already know we have this attr, so we match
        return PR_TRUE;
    }

    nsAutoString value;
    nsresult rv = aContent->GetAttr(aNamespaceID, aAttrName, value);

    return NS_SUCCEEDED(rv) && value.Equals(aAttrValue);
}

nsresult
nsXULDocument::PrepareToLoad(nsISupports* aContainer,
                             const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsIParser** aResult)
{
    nsresult rv;

    // Get the document's principal
    nsCOMPtr<nsISupports> owner;
    rv = aChannel->GetOwner(getter_AddRefs(owner));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);

    return PrepareToLoadPrototype(mDocumentURI, aCommand, principal, aResult);
}


nsresult
nsXULDocument::PrepareToLoadPrototype(nsIURI* aURI, const char* aCommand,
                                      nsIPrincipal* aDocumentPrincipal,
                                      nsIParser** aResult)
{
    nsresult rv;

    // Create a new prototype document.
    rv = NS_NewXULPrototypeDocument(nsnull,
                                    NS_GET_IID(nsIXULPrototypeDocument),
                                    getter_AddRefs(mCurrentPrototype));
    if (NS_FAILED(rv)) return rv;

    // Bootstrap the master document prototype.
    if (! mMasterPrototype) {
        mMasterPrototype = mCurrentPrototype;
        mMasterPrototype->SetDocumentPrincipal(aDocumentPrincipal);
    }

    rv = mCurrentPrototype->SetURI(aURI);
    if (NS_FAILED(rv)) return rv;

    // Create a XUL content sink, a parser, and kick off a load for
    // the overlay.
    nsCOMPtr<nsIXULContentSink> sink;
    rv = NS_NewXULContentSink(getter_AddRefs(sink));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create XUL content sink");
    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(this, mCurrentPrototype);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize datasource sink");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
    if (NS_FAILED(rv)) return rv;

    parser->SetCommand(nsCRT::strcmp(aCommand, "view-source") ? eViewNormal :
                       eViewSource);

    parser->SetDocumentCharset(NS_LITERAL_CSTRING("UTF-8"),
                               kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink); // grabs a reference to the parser

    *aResult = parser;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
nsXULDocument::ApplyPersistentAttributes()
{
    // Add all of the 'persisted' attributes into the content
    // model.
    if (! mLocalStore)
        return NS_OK;

    mApplyingPersistedAttrs = PR_TRUE;

    nsresult rv;
    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString docurl;
    mDocumentURI->GetSpec(docurl);

    nsCOMPtr<nsIRDFResource> doc;
    gRDFService->GetResource(docurl, getter_AddRefs(doc));

    nsCOMPtr<nsISimpleEnumerator> persisted;
    mLocalStore->GetTargets(doc, kNC_persist, PR_TRUE, getter_AddRefs(persisted));

    while (1) {
        PRBool hasmore = PR_FALSE;
        persisted->HasMoreElements(&hasmore);
        if (! hasmore)
            break;

        nsCOMPtr<nsISupports> isupports;
        persisted->GetNext(getter_AddRefs(isupports));

        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(isupports);
        if (! resource) {
            NS_WARNING("expected element to be a resource");
            continue;
        }

        const char *uri;
        resource->GetValueConst(&uri);
        if (! uri)
            continue;

        nsAutoString id;
        nsXULContentUtils::MakeElementID(this, NS_ConvertASCIItoUCS2(uri), id);

        // This will clear the array if there are no elements.
        GetElementsForID(id, elements);

        PRUint32 cnt = 0;
        elements->Count(&cnt);
        if (! cnt)
            continue;

        ApplyPersistentAttributesToElements(resource, elements);
    }

    mApplyingPersistedAttrs = PR_FALSE;

    return NS_OK;
}


nsresult
nsXULDocument::ApplyPersistentAttributesToElements(nsIRDFResource* aResource,
                                                   nsISupportsArray* aElements)
{
    nsresult rv;

    nsCOMPtr<nsISimpleEnumerator> attrs;
    rv = mLocalStore->ArcLabelsOut(aResource, getter_AddRefs(attrs));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasmore;
        rv = attrs->HasMoreElements(&hasmore);
        if (NS_FAILED(rv)) return rv;

        if (! hasmore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = attrs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
        if (! property) {
            NS_WARNING("expected a resource");
            continue;
        }

        const char* attrname;
        rv = property->GetValueConst(&attrname);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> attr = do_GetAtom(attrname);
        if (! attr)
            return NS_ERROR_OUT_OF_MEMORY;

        // XXX could hang namespace off here, as well...

        nsCOMPtr<nsIRDFNode> node;
        rv = mLocalStore->GetTarget(aResource, property, PR_TRUE,
                                    getter_AddRefs(node));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(node);
        if (! literal) {
            NS_WARNING("expected a literal");
            continue;
        }

        const PRUnichar* value;
        rv = literal->GetValueConst(&value);
        if (NS_FAILED(rv)) return rv;

        nsDependentString wrapper(value);

        PRUint32 cnt;
        rv = aElements->Count(&cnt);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
            nsISupports* isupports2 = aElements->ElementAt(i);
            if (! isupports2)
                continue;

            nsCOMPtr<nsIContent> element = do_QueryInterface(isupports2);
            NS_RELEASE(isupports2);

            rv = element->SetAttr(/* XXX */ kNameSpaceID_None,
                                  attr,
                                  wrapper,
                                  PR_TRUE);
        }
    }

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsXULDocument::ContextStack
//

nsXULDocument::ContextStack::ContextStack()
    : mTop(nsnull), mDepth(0)
{
}

nsXULDocument::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        NS_IF_RELEASE(doomed->mElement);
        delete doomed;
    }
}

nsresult
nsXULDocument::ContextStack::Push(nsXULPrototypeElement* aPrototype,
                                  nsIContent* aElement)
{
    Entry* entry = new Entry;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mPrototype = aPrototype;
    entry->mElement   = aElement;
    NS_IF_ADDREF(entry->mElement);
    entry->mIndex     = 0;

    entry->mNext = mTop;
    mTop = entry;

    ++mDepth;
    return NS_OK;
}

nsresult
nsXULDocument::ContextStack::Pop()
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    Entry* doomed = mTop;
    mTop = mTop->mNext;
    --mDepth;

    NS_IF_RELEASE(doomed->mElement);
    delete doomed;
    return NS_OK;
}

nsresult
nsXULDocument::ContextStack::Peek(nsXULPrototypeElement** aPrototype,
                                           nsIContent** aElement,
                                           PRInt32* aIndex)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aPrototype = mTop->mPrototype;
    *aElement   = mTop->mElement;
    NS_IF_ADDREF(*aElement);
    *aIndex     = mTop->mIndex;

    return NS_OK;
}


nsresult
nsXULDocument::ContextStack::SetTopIndex(PRInt32 aIndex)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    mTop->mIndex = aIndex;
    return NS_OK;
}


PRBool
nsXULDocument::ContextStack::IsInsideXULTemplate()
{
    if (mDepth) {
        for (nsIContent* element = mTop->mElement; element;
             element = element->GetParent()) {

            nsINodeInfo *ni = element->GetNodeInfo();

            if (ni && ni->Equals(nsXULAtoms::Template, kNameSpaceID_XUL)) {
                return PR_TRUE;
            }
        }
    }
    return PR_FALSE;
}


//----------------------------------------------------------------------
//
// Content model walking routines
//

nsresult
nsXULDocument::PrepareToWalk()
{
    // Prepare to walk the mCurrentPrototype
    nsresult rv;

    // Keep an owning reference to the prototype document so that its
    // elements aren't yanked from beneath us.
    mPrototypes.AppendObject(mCurrentPrototype);

    // Push the overlay references onto our overlay processing
    // stack. GetOverlayReferences() will return an ordered array of
    // overlay references...
    nsCOMPtr<nsISupportsArray> overlays;
    rv = mCurrentPrototype->GetOverlayReferences(getter_AddRefs(overlays));
    if (NS_FAILED(rv)) return rv;

    // ...and we preserve this ordering by appending to our
    // mUnloadedOverlays array in reverse order
    PRUint32 count;
    overlays->Count(&count);
    for (PRInt32 i = count - 1; i >= 0; --i) {
        nsISupports* isupports = overlays->ElementAt(i);
        mUnloadedOverlays->AppendElement(isupports);
        NS_IF_RELEASE(isupports);
    }


    // Now check the chrome registry for any additional overlays.
    rv = AddChromeOverlays();

    // Get the prototype's root element and initialize the context
    // stack for the prototype walk.
    nsXULPrototypeElement* proto;
    rv = mCurrentPrototype->GetRootElement(&proto);
    if (NS_FAILED(rv)) return rv;


    if (! proto) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULLog, PR_LOG_ERROR)) {
            nsCOMPtr<nsIURI> url;
            rv = mCurrentPrototype->GetURI(getter_AddRefs(url));
            if (NS_FAILED(rv)) return rv;

            nsCAutoString urlspec;
            rv = url->GetSpec(urlspec);
            if (NS_FAILED(rv)) return rv;

            PR_LOG(gXULLog, PR_LOG_ERROR,
                   ("xul: error parsing '%s'", urlspec.get()));
        }
#endif

        return NS_OK;
    }

    // Do one-time initialization if we're preparing to walk the
    // master document's prototype.
    nsCOMPtr<nsIContent> root;

    if (mState == eState_Master) {
        rv = CreateElementFromPrototype(proto, getter_AddRefs(root));
        if (NS_FAILED(rv)) return rv;

        SetRootContent(root);

        // Add the root element to the XUL document's ID-to-element map.
        rv = AddElementToMap(root);
        if (NS_FAILED(rv)) return rv;

        // Add a dummy channel to the load group as a placeholder for the document
        // load
        rv = PlaceHolderRequest::Create(getter_AddRefs(mPlaceHolderRequest));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

        if (group) {
            rv = mPlaceHolderRequest->SetLoadGroup(group);
            if (NS_FAILED(rv)) return rv;
            rv = group->AddRequest(mPlaceHolderRequest, nsnull);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // There'd better not be anything on the context stack at this
    // point! This is the basis case for our "induction" in
    // ResumeWalk(), below, which'll assume that there's always a
    // content element on the context stack if either 1) we're in the
    // "master" document, or 2) we're in an overlay, and we've got
    // more than one prototype element (the single, root "overlay"
    // element) on the stack.
    NS_ASSERTION(mContextStack.Depth() == 0, "something's on the context stack already");
    if (mContextStack.Depth() != 0)
        return NS_ERROR_UNEXPECTED;

    rv = mContextStack.Push(proto, root);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsXULDocument::AddChromeOverlays()
{
    nsresult rv;

    nsCOMPtr<nsIURI> docUri;
    rv = mCurrentPrototype->GetURI(getter_AddRefs(docUri));
    NS_ENSURE_SUCCESS(rv, rv);

    /* overlays only apply to chrome, skip all content URIs */
    if (!IsChromeURI(docUri)) return NS_OK;

    nsCOMPtr<nsIXULOverlayProvider> chromeReg(do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
    // In embedding situations, the chrome registry may not provide overlays,
    // or even exist at all; that's OK.
    NS_ENSURE_TRUE(chromeReg, NS_OK);

    nsCOMPtr<nsISimpleEnumerator> overlays;
    rv = chromeReg->GetXULOverlays(docUri, getter_AddRefs(overlays));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool moreOverlays;
    nsCOMPtr<nsISupports> next;
    nsCOMPtr<nsIURI> uri;

    while (NS_SUCCEEDED(rv = overlays->HasMoreElements(&moreOverlays)) &&
           moreOverlays) {
        rv = overlays->GetNext(getter_AddRefs(next));
        if (NS_FAILED(rv) || !next) continue;

        uri = do_QueryInterface(next);
        if (!uri) {
            NS_ERROR("Chrome registry handed me a non-nsIURI object!");
            continue;
        }

        mUnloadedOverlays->AppendElement(uri);
    }

    return NS_OK;
}

nsresult
nsXULDocument::ResumeWalk()
{
    // Walk the prototype and build the delegate content model. The
    // walk is performed in a top-down, left-to-right fashion. That
    // is, a parent is built before any of its children; a node is
    // only built after all of its siblings to the left are fully
    // constructed.
    //
    // It is interruptable so that transcluded documents (e.g.,
    // <html:script src="..." />) can be properly re-loaded if the
    // cached copy of the document becomes stale.
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> secMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    while (1) {
        // Begin (or resume) walking the current prototype.

        while (mContextStack.Depth() > 0) {
            // Look at the top of the stack to determine what we're
            // currently working on.
            nsXULPrototypeElement* proto;
            nsCOMPtr<nsIContent> element;
            PRInt32 indx;
            rv = mContextStack.Peek(&proto, getter_AddRefs(element), &indx);
            if (NS_FAILED(rv)) return rv;

            if (indx >= (PRInt32)proto->mNumChildren) {
                // We've processed all of the prototype's children. If
                // we're in the master prototype, do post-order
                // document-level hookup. (An overlay will get its
                // document hookup done when it's successfully
                // resolved.)
                if (element && (mState == eState_Master))
                    AddElementToDocumentPost(element);

                // Now pop the context stack back up to the parent
                // element and continue the prototype walk.
                mContextStack.Pop();
                continue;
            }

            // Grab the next child, and advance the current context stack
            // to the next sibling to our right.
            nsXULPrototypeNode* childproto = proto->mChildren[indx];
            mContextStack.SetTopIndex(++indx);

            switch (childproto->mType) {
            case nsXULPrototypeNode::eType_Element: {
                // An 'element', which may contain more content.
                nsXULPrototypeElement* protoele =
                    NS_REINTERPRET_CAST(nsXULPrototypeElement*, childproto);

                nsCOMPtr<nsIContent> child;

                if ((mState == eState_Master) || (mContextStack.Depth() > 1)) {
                    // We're in the master document -or -we're in an
                    // overlay, and far enough down into the overlay's
                    // content that we can simply build the delegates
                    // and attach them to the parent node.
                    NS_ASSERTION(element != nsnull, "no element on context stack");

                    rv = CreateElementFromPrototype(protoele,
                                                    getter_AddRefs(child));
                    if (NS_FAILED(rv)) return rv;

                    // ...and append it to the content model.
                    rv = element->AppendChildTo(child, PR_FALSE, PR_FALSE);
                    if (NS_FAILED(rv)) return rv;

                    // do pre-order document-level hookup, but only if
                    // we're in the master document. For an overlay,
                    // this will happen when the overlay is
                    // successfully resolved.
                    if (mState == eState_Master)
                        AddElementToDocumentPre(child);
                }
                else {
                    // We're in the "first ply" of an overlay: the
                    // "hookup" nodes. Create an 'overlay' element so
                    // that we can continue to build content, and
                    // enter a forward reference so we can hook it up
                    // later.
                    rv = CreateOverlayElement(protoele, getter_AddRefs(child));
                    if (NS_FAILED(rv)) return rv;
                }

                // If it has children, push the element onto the context
                // stack and begin to process them.
                if (protoele->mNumChildren > 0) {
                    rv = mContextStack.Push(protoele, child);
                    if (NS_FAILED(rv)) return rv;
                }
                else if (mState == eState_Master) {
                    // If there are no children, and we're in the
                    // master document, do post-order document hookup
                    // immediately.
                    AddElementToDocumentPost(child);
                }
            }
            break;

            case nsXULPrototypeNode::eType_Script: {
                // A script reference. Execute the script immediately;
                // this may have side effects in the content model.
                nsXULPrototypeScript* scriptproto =
                    NS_REINTERPRET_CAST(nsXULPrototypeScript*, childproto);

                if (scriptproto->mSrcURI) {
                    // A transcluded script reference; this may
                    // "block" our prototype walk if the script isn't
                    // cached, or the cached copy of the script is
                    // stale and must be reloaded.
                    PRBool blocked;
                    rv = LoadScript(scriptproto, &blocked);
                    if (NS_FAILED(rv)) return rv;

                    if (blocked)
                        return NS_OK;
                }
                else if (scriptproto->mJSObject) {
                    // An inline script
                    rv = ExecuteScript(scriptproto->mJSObject);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;

            case nsXULPrototypeNode::eType_Text: {
                // A simple text node.

                if ((mState == eState_Master) || (mContextStack.Depth() > 1)) {
                    // We're in the master document -or -we're in an
                    // overlay, and far enough down into the overlay's
                    // content that we can simply build the delegates
                    // and attach them to the parent node.
                    NS_ASSERTION(element, "no element on context stack");

                    nsCOMPtr<nsITextContent> text;
                    rv = NS_NewTextNode(getter_AddRefs(text));
                    NS_ENSURE_SUCCESS(rv, rv);

                    nsXULPrototypeText* textproto =
                        NS_REINTERPRET_CAST(nsXULPrototypeText*, childproto);
                    text->SetText(textproto->mValue.get(),
                                  textproto->mValue.Length(),
                                  PR_FALSE);

                    nsCOMPtr<nsIContent> child = do_QueryInterface(text);
                    if (! child)
                        return NS_ERROR_UNEXPECTED;

                    rv = element->AppendChildTo(child, PR_FALSE, PR_FALSE);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;
            }
        }

        // Once we get here, the context stack will have been
        // depleted. That means that the entire prototype has been
        // walked and content has been constructed.

        // If we're not already, mark us as now processing overlays.
        mState = eState_Overlay;

        PRUint32 count;
        mUnloadedOverlays->Count(&count);

        // If there are no overlay URIs, then we're done.
        if (! count)
            break;

        nsCOMPtr<nsIURI> uri =
            dont_AddRef(NS_REINTERPRET_CAST(nsIURI*, mUnloadedOverlays->ElementAt(count - 1)));

        mUnloadedOverlays->RemoveElementAt(count - 1);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
            nsCAutoString urlspec;
            uri->GetSpec(urlspec);

            PR_LOG(gXULLog, PR_LOG_DEBUG,
                   ("xul: loading overlay %s", urlspec.get()));
        }
#endif

        // Chrome documents are allowed to load overlays from anywhere.
        // Also, any document may load a chrome:// overlay.
        // In all other cases, the overlay is only allowed to load if
        // the master document and prototype document have the same origin.

        PRBool overlayIsChrome = IsChromeURI(uri);
        if (!IsChromeURI(mDocumentURI) && !overlayIsChrome) {
            // Make sure we're allowed to load this overlay.
            rv = secMan->CheckSameOriginURI(mDocumentURI, uri);
            if (NS_FAILED(rv)) {
                // move on to the next overlay
                continue;
            }
        }

        // Look in the prototype cache for the prototype document with
        // the specified overlay URI.
        if (overlayIsChrome)
            gXULCache->GetPrototype(uri, getter_AddRefs(mCurrentPrototype));
        else
            mCurrentPrototype = nsnull;

        // Same comment as nsChromeProtocolHandler::NewChannel and
        // nsXULDocument::StartDocumentLoad
        // - Ben Goodger
        //
        // We don't abort on failure here because there are too many valid
        // cases that can return failure, and the null-ness of |proto| is
        // enough to trigger the fail-safe parse-from-disk solution.
        // Example failure cases (for reference) include:
        //
        // NS_ERROR_NOT_AVAILABLE: the URI was not found in the FastLoad file,
        //                         parse from disk
        // other: the FastLoad file, XUL.mfl, could not be found, probably
        //        due to being accessed before a profile has been selected
        //        (e.g. loading chrome for the profile manager itself).
        //        The .xul file must be parsed from disk.

        PRBool useXULCache;
        gXULCache->GetEnabled(&useXULCache);

        if (useXULCache && mCurrentPrototype) {
            PRBool loaded;
            rv = mCurrentPrototype->AwaitLoadDone(this, &loaded);
            if (NS_FAILED(rv)) return rv;

            if (! loaded) {
                // Return to the main event loop and eagerly await the
                // prototype overlay load's completion. When the content
                // sink completes, it will trigger an EndLoad(), which'll
                // wind us back up here, in ResumeWalk().
                return NS_OK;
            }

            // Found the overlay's prototype in the cache, fully loaded.
            rv = AddPrototypeSheets();
            if (NS_FAILED(rv)) return rv;

            // Now prepare to walk the prototype to create its content
            rv = PrepareToWalk();
            if (NS_FAILED(rv)) return rv;

            PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was cached"));
        }
        else {
            // Not there. Initiate a load.
            PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was not cached"));

            nsCOMPtr<nsIParser> parser;
            rv = PrepareToLoadPrototype(uri, "view", nsnull, getter_AddRefs(parser));
            if (NS_FAILED(rv)) return rv;

            // Predicate mIsWritingFastLoad on the XUL cache being enabled,
            // so we don't have to re-check whether the cache is enabled all
            // the time.
            mIsWritingFastLoad = useXULCache;

            nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser);
            if (! listener)
                return NS_ERROR_UNEXPECTED;

            // Add an observer to the parser; this'll get called when
            // Necko fires its On[Start|Stop]Request() notifications,
            // and will let us recover from a missing overlay.
            ParserObserver* parserObserver = new ParserObserver(this);
            if (! parserObserver)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(parserObserver);
            parser->Parse(uri, parserObserver);
            NS_RELEASE(parserObserver);

            nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
            rv = NS_OpenURI(listener, nsnull, uri, nsnull, group);
            if (NS_FAILED(rv)) return rv;

            // If it's a 'chrome:' prototype document, then put it into
            // the prototype cache; other XUL documents will be reloaded
            // each time.  We must do this after NS_OpenURI and AsyncOpen,
            // or chrome code will wrongly create a cached chrome channel
            // instead of a real one.
            if (useXULCache && overlayIsChrome) {
                rv = gXULCache->PutPrototype(mCurrentPrototype);
                if (NS_FAILED(rv)) return rv;
            }

            // Return to the main event loop and eagerly await the
            // overlay load's completion. When the content sink
            // completes, it will trigger an EndLoad(), which'll wind
            // us back up here, in ResumeWalk().
            return NS_OK;
        }
    }

    // If we get here, there is nothing left for us to walk. The content
    // model is built and ready for layout.
    rv = ResolveForwardReferences();
    if (NS_FAILED(rv)) return rv;

    rv = ApplyPersistentAttributes();
    if (NS_FAILED(rv)) return rv;

    // Everything after this point we only want to do once we're
    // certain that we've been embedded in a presentation shell.

    StartLayout();

    if (mIsWritingFastLoad && IsChromeURI(mDocumentURI))
        gXULCache->WritePrototype(mMasterPrototype);

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
    }
    NS_ASSERTION(mPlaceHolderRequest, "Bug 119310, perhaps overlayinfo referenced a overlay that doesn't exist");
    if (mPlaceHolderRequest) {
        // Remove the placeholder channel; if we're the last channel in the
        // load group, this will fire the OnEndDocumentLoad() method in the
        // docshell, and run the onload handlers, etc.
        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
        if (group) {
            rv = group->RemoveRequest(mPlaceHolderRequest, nsnull, NS_OK);
            if (NS_FAILED(rv)) return rv;

            mPlaceHolderRequest = nsnull;
        }
    }

    return rv;
}

nsresult
nsXULDocument::LoadScript(nsXULPrototypeScript* aScriptProto, PRBool* aBlock)
{
    // Load a transcluded script
    nsresult rv;

    if (aScriptProto->mJSObject) {
        rv = ExecuteScript(aScriptProto->mJSObject);

        // Ignore return value from execution, and don't block
        *aBlock = PR_FALSE;
        return NS_OK;
    }

    // Try the XUL script cache, in case two XUL documents source the same
    // .js file (e.g., strres.js from navigator.xul and utilityOverlay.xul).
    // XXXbe the cache relies on aScriptProto's GC root!
    PRBool useXULCache;
    gXULCache->GetEnabled(&useXULCache);

    if (useXULCache) {
        gXULCache->GetScript(aScriptProto->mSrcURI,
                             NS_REINTERPRET_CAST(void**, &aScriptProto->mJSObject));

        if (aScriptProto->mJSObject) {
            rv = ExecuteScript(aScriptProto->mJSObject);

            // Ignore return value from execution, and don't block
            *aBlock = PR_FALSE;
            return NS_OK;
        }
    }

    // Set the current script prototype so that OnStreamComplete can report
    // the right file if there are errors in the script.
    NS_ASSERTION(!mCurrentScriptProto,
                 "still loading a script when starting another load?");
    mCurrentScriptProto = aScriptProto;

    if (aScriptProto->mSrcLoading) {
        // Another XULDocument load has started, which is still in progress.
        // Remember to ResumeWalk this document when the load completes.
        mNextSrcLoadWaiter = aScriptProto->mSrcLoadWaiters;
        aScriptProto->mSrcLoadWaiters = this;
        NS_ADDREF_THIS();
    }
    else {
        // Set mSrcLoading *before* calling NS_NewStreamLoader, in case the
        // stream completes (probably due to an error) within the activation
        // of NS_NewStreamLoader.
        aScriptProto->mSrcLoading = PR_TRUE;

        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

        // N.B., the loader will be released in OnStreamComplete
        nsIStreamLoader* loader;
        rv = NS_NewStreamLoader(&loader, aScriptProto->mSrcURI, this, nsnull, group);
        if (NS_FAILED(rv)) return rv;
    }

    // Block until OnStreamComplete resumes us.
    *aBlock = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::OnStreamComplete(nsIStreamLoader* aLoader,
                                nsISupports* context,
                                nsresult aStatus,
                                PRUint32 stringLen,
                                const PRUint8* string)
{
#ifdef DEBUG
    // print a load error on bad status
    if (NS_FAILED(aStatus)) {
        nsCOMPtr<nsIRequest> request;
        aLoader->GetRequest(getter_AddRefs(request));
        nsCOMPtr<nsIChannel> channel;
        channel = do_QueryInterface(request);
        if (channel) {
            nsCOMPtr<nsIURI> uri;
            channel->GetURI(getter_AddRefs(uri));
            if (uri) {
                nsCAutoString uriSpec;
                uri->GetSpec(uriSpec);
                printf("Failed to load %s\n", uriSpec.get());
            }
        }
    }
#endif

    // This is the completion routine that will be called when a
    // transcluded script completes. Compile and execute the script
    // if the load was successful, then continue building content
    // from the prototype.
    nsresult rv;

    NS_ASSERTION(mCurrentScriptProto && mCurrentScriptProto->mSrcLoading,
                 "script source not loading on unichar stream complete?");

    // Clear mCurrentScriptProto now, but save it first for use below in
    // the compile/execute code, and in the while loop that resumes walks
    // of other documents that raced to load this script
    nsXULPrototypeScript* scriptProto = mCurrentScriptProto;
    mCurrentScriptProto = nsnull;

    // Clear the prototype's loading flag before executing the script or
    // resuming document walks, in case any of those control flows starts a
    // new script load.
    scriptProto->mSrcLoading = PR_FALSE;

    if (NS_SUCCEEDED(aStatus)) {
        // If the including XUL document is a FastLoad document, and we're
        // compiling an out-of-line script (one with src=...), then we must
        // be writing a new FastLoad file.  If we were reading this script
        // from the FastLoad file, XULContentSinkImpl::OpenScript (over in
        // nsXULContentSink.cpp) would have already deserialized a non-null
        // script->mJSObject, causing control flow at the top of LoadScript
        // not to reach here.
        nsCOMPtr<nsIURI> uri = scriptProto->mSrcURI;

        // XXX this seems broken - what if the script is non-ascii? (bug 241739)
        nsString stringStr; stringStr.AssignWithConversion(NS_REINTERPRET_CAST(const char*, string), stringLen);
        rv = scriptProto->Compile(stringStr.get(), stringLen, uri, 1, this,
                                  mCurrentPrototype);

        aStatus = rv;
        if (NS_SUCCEEDED(rv) && scriptProto->mJSObject) {
            rv = ExecuteScript(scriptProto->mJSObject);

            // If the XUL cache is enabled, save the script object there in
            // case different XUL documents source the same script.
            //
            // But don't save the script in the cache unless the master XUL
            // document URL is a chrome: URL.  It is valid for a URL such as
            // about:config to translate into a master document URL, whose
            // prototype document nodes -- including prototype scripts that
            // hold GC roots protecting their mJSObject pointers -- are not
            // cached in the XUL prototype cache.  See StartDocumentLoad,
            // the fillXULCache logic.
            //
            // A document such as about:config is free to load a script via
            // a URL such as chrome://global/content/config.js, and we must
            // not cache that script object without a prototype cache entry
            // containing a companion nsXULPrototypeScript node that owns a
            // GC root protecting the script object.  Otherwise, the script
            // cache entry will dangle once the uncached prototype document
            // is released when its owning nsXULDocument is unloaded.
            //
            // (See http://bugzilla.mozilla.org/show_bug.cgi?id=98207 for
            // the true crime story.)
            PRBool useXULCache;
            gXULCache->GetEnabled(&useXULCache);

            if (useXULCache && IsChromeURI(mDocumentURI)) {
                gXULCache->PutScript(scriptProto->mSrcURI,
                                     NS_REINTERPRET_CAST(void*, scriptProto->mJSObject));
            }

            if (mIsWritingFastLoad && mCurrentPrototype != mMasterPrototype) {
                // If we are loading an overlay script, try to serialize
                // it to the FastLoad file here.  Master scripts will be
                // serialized when the master prototype document gets
                // written, at the bottom of ResumeWalk.  That way, master
                // out-of-line scripts are serialized in the same order that
                // they'll be read, in the FastLoad file, which reduces the
                // number of seeks that dump the underlying stream's buffer.
                //
                // Ignore the return value, as we don't need to propagate
                // a failure to write to the FastLoad file, because this
                // method aborts that whole process on error.
                nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner
                  = do_QueryInterface(mCurrentPrototype);
                nsCOMPtr<nsIScriptGlobalObject> global;
                globalOwner->GetScriptGlobalObject(getter_AddRefs(global));

                NS_ASSERTION(global != nsnull, "master prototype w/o global?!");
                if (global) {
                    nsIScriptContext *scriptContext = global->GetContext();
                    if (scriptContext)
                        scriptProto->SerializeOutOfLine(nsnull, scriptContext);
                }
            }
        }
        // ignore any evaluation errors
    }

    // balance the addref we added in LoadScript()
    NS_RELEASE(aLoader);

    rv = ResumeWalk();

    // Load a pointer to the prototype-script's list of nsXULDocuments who
    // raced to load the same script
    nsXULDocument** docp = &scriptProto->mSrcLoadWaiters;

    // Resume walking other documents that waited for this one's load, first
    // executing the script we just compiled, in each doc's script context
    nsXULDocument* doc;
    while ((doc = *docp) != nsnull) {
        NS_ASSERTION(doc->mCurrentScriptProto == scriptProto,
                     "waiting for wrong script to load?");
        doc->mCurrentScriptProto = nsnull;

        // Unlink doc from scriptProto's list before executing and resuming
        *docp = doc->mNextSrcLoadWaiter;
        doc->mNextSrcLoadWaiter = nsnull;

        // Execute only if we loaded and compiled successfully, then resume
        if (NS_SUCCEEDED(aStatus) && scriptProto->mJSObject) {
            doc->ExecuteScript(scriptProto->mJSObject);
        }
        doc->ResumeWalk();
        NS_RELEASE(doc);
    }

    return rv;
}


nsresult
nsXULDocument::ExecuteScript(JSObject* aScriptObject)
{
    NS_PRECONDITION(aScriptObject != nsnull, "null ptr");
    if (! aScriptObject)
        return NS_ERROR_NULL_POINTER;

    // Execute the precompiled script with the given version
    nsresult rv = NS_ERROR_UNEXPECTED;

    NS_ASSERTION(mScriptGlobalObject != nsnull, "no script global object");

    nsCOMPtr<nsIScriptContext> context;
    if (mScriptGlobalObject && (context = mScriptGlobalObject->GetContext()))
        rv = context->ExecuteScript(aScriptObject, nsnull, nsnull, nsnull);

    return rv;
}


nsresult
nsXULDocument::CreateElementFromPrototype(nsXULPrototypeElement* aPrototype,
                                          nsIContent** aResult)
{
    // Create a content model element from a prototype element.
    NS_PRECONDITION(aPrototype != nsnull, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    *aResult = nsnull;
    nsresult rv = NS_OK;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_NOTICE)) {
        nsAutoString tagstr;
        aPrototype->mNodeInfo->GetQualifiedName(tagstr);

        nsCAutoString tagstrC;
        tagstrC.AssignWithConversion(tagstr);
        PR_LOG(gXULLog, PR_LOG_NOTICE,
               ("xul: creating <%s> from prototype",
                tagstrC.get()));
    }
#endif

    nsCOMPtr<nsIContent> result;

    if (aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        // If it's a XUL element, it'll be lightweight until somebody
        // monkeys with it.
        rv = nsXULElement::Create(aPrototype, this, PR_TRUE, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // If it's not a XUL element, it's gonna be heavyweight no matter
        // what. So we need to copy everything out of the prototype
        // into the element.
        rv = NS_NewElement(getter_AddRefs(result),
                           aPrototype->mNodeInfo->NamespaceID(),
                           aPrototype->mNodeInfo);
        if (NS_FAILED(rv)) return rv;

        result->SetDocument(this, PR_FALSE, PR_TRUE);

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;
    }

    result->SetContentID(mNextContentID++);

    result.swap(*aResult);

    return NS_OK;
}

nsresult
nsXULDocument::CreateOverlayElement(nsXULPrototypeElement* aPrototype,
                                    nsIContent** aResult)
{
    nsresult rv;

    // This doesn't really do anything except create a placeholder
    // element. I'd use an XML element, but it gets its knickers in a
    // knot with DOM ranges when you try to remove its children.
    nsCOMPtr<nsIContent> element;
    rv = nsXULElement::Create(aPrototype, this, PR_FALSE,
                              getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    OverlayForwardReference* fwdref =
        new OverlayForwardReference(this, element);
    if (! fwdref)
        return NS_ERROR_OUT_OF_MEMORY;

    // transferring ownership to ya...
    rv = AddForwardReference(fwdref);
    if (NS_FAILED(rv)) return rv;

    *aResult = element;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsXULDocument::AddAttributes(nsXULPrototypeElement* aPrototype,
                             nsIContent* aElement)
{
    nsresult rv;

    for (PRUint32 i = 0; i < aPrototype->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &(aPrototype->mAttributes[i]);
        nsAutoString  valueStr;
        protoattr->mValue.ToString(valueStr);

        rv = aElement->SetAttr(protoattr->mName.NamespaceID(),
                               protoattr->mName.LocalName(),
                               protoattr->mName.GetPrefix(),
                               valueStr,
                               PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
nsXULDocument::CheckTemplateBuilderHookup(nsIContent* aElement,
                                          PRBool* aNeedsHookup)
{
    // See if the element already has a `database' attribute. If it
    // does, then the template builder has already been created.
    //
    // XXX This approach will crash and burn (well, maybe not _that_
    // bad) if aElement is not a XUL element.
    //
    // XXXvarga Do we still want to support non XUL content?
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(aElement);
    if (xulElement) {
        nsCOMPtr<nsIRDFCompositeDataSource> ds;
        xulElement->GetDatabase(getter_AddRefs(ds));
        if (ds) {
            *aNeedsHookup = PR_FALSE;
            return NS_OK;
        }
    }

    // Check aElement for a 'datasources' attribute, if it has
    // one a XUL template builder needs to be hooked up.
    *aNeedsHookup = aElement->HasAttr(kNameSpaceID_None,
                                      nsXULAtoms::datasources);
    return NS_OK;
}

nsresult
nsXULDocument::CreateTemplateBuilder(nsIContent* aElement)
{
    // Check if need to construct a tree builder or content builder.
    PRBool isTreeBuilder = PR_FALSE;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> baseTag;

    nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1");
    if (xblService) {
        xblService->ResolveTag(aElement, &nameSpaceID, getter_AddRefs(baseTag));
    }
    else {
        nsINodeInfo *ni = aElement->GetNodeInfo();
        nameSpaceID = ni->NamespaceID();
        baseTag = ni->NameAtom();
    }

    if ((nameSpaceID == kNameSpaceID_XUL) && (baseTag == nsXULAtoms::tree)) {
        // By default, we build content for a tree and then we attach
        // the tree content view. However, if the `dont-build-content'
        // flag is set, then we we'll attach a tree builder which
        // directly implements the tree view.

        nsAutoString flags;
        aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::flags, flags);
        if (flags.Find(NS_LITERAL_STRING("dont-build-content")) >= 0) {
            isTreeBuilder = PR_TRUE;
        }
    }

    if (isTreeBuilder) {
        // Create and initialize a tree builder.
        nsCOMPtr<nsIXULTemplateBuilder> builder =
            do_CreateInstance("@mozilla.org/xul/xul-tree-builder;1");

        if (! builder)
            return NS_ERROR_FAILURE;

        builder->Init(aElement);

        // Create a <treechildren> if one isn't there already.
        // XXXvarga what about attributes?
        nsCOMPtr<nsIContent> bodyContent;
        nsXULContentUtils::FindChildByTag(aElement, kNameSpaceID_XUL,
                                          nsXULAtoms::treechildren,
                                          getter_AddRefs(bodyContent));

        if (! bodyContent) {
            // Get the document.
            nsIDocument *document = aElement->GetDocument();
            NS_ASSERTION(document, "no document");
            if (! document)
                return NS_ERROR_UNEXPECTED;

            nsresult rv = document->CreateElem(nsXULAtoms::treechildren,
                                               nsnull, kNameSpaceID_XUL,
                                               PR_FALSE,
                                               getter_AddRefs(bodyContent));
            NS_ENSURE_SUCCESS(rv, rv);

            aElement->AppendChildTo(bodyContent, PR_FALSE, PR_TRUE);
        }
    }
    else {
        // Create and initialize a content builder.
        nsCOMPtr<nsIXULTemplateBuilder> builder
            = do_CreateInstance("@mozilla.org/xul/xul-template-builder;1");

        if (! builder)
            return NS_ERROR_FAILURE;

        builder->Init(aElement);

        nsCOMPtr<nsIXULContent> xulContent = do_QueryInterface(aElement);
        if (xulContent) {
            // Mark the XUL element as being lazy, so the template builder
            // will run when layout first asks for these nodes.
            xulContent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        }
        else {
            // Force construction of immediate template sub-content _now_.
            builder->CreateContents(aElement);
        }
    }

    return NS_OK;
}


nsresult
nsXULDocument::AddPrototypeSheets()
{
    // Add mCurrentPrototype's style sheets to the document.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> sheets;
    rv = mCurrentPrototype->GetStyleSheetReferences(getter_AddRefs(sheets));
    if (NS_FAILED(rv)) return rv;

    PRUint32 count;
    sheets->Count(&count);
    for (PRUint32 i = 0; i < count; ++i) {
        nsISupports* isupports = sheets->ElementAt(i);
        nsCOMPtr<nsIURI> uri = do_QueryInterface(isupports);
        NS_IF_RELEASE(isupports);

        NS_ASSERTION(uri, "not a URI!!!");
        if (! uri)
            return NS_ERROR_UNEXPECTED;

        nsCAutoString spec;
        uri->GetAsciiSpec(spec);

        if (!IsChromeURI(uri)) {
            // These don't get to be in the prototype cache anyway...
            // and we can't load non-chrome sheets synchronously
            continue;
        }

        nsCOMPtr<nsICSSStyleSheet> sheet;

        // If the sheet is a chrome URL, then we can refetch the sheet
        // synchronously, since we know the sheet is local.  It's not
        // too late! :) If we're lucky, the loader will just pull it
        // from the prototype cache anyway.
        // Otherwise we just bail.  It shouldn't currently
        // be possible to get into this situation for any reason
        // other than a skin switch anyway (since skin switching is the
        // only system that partially invalidates the XUL cache).
        // - dwh
        //XXXbz we hit this code from fastload all the time.  Bug 183505.
        nsICSSLoader* loader = GetCSSLoader();
        NS_ENSURE_TRUE(loader, NS_ERROR_OUT_OF_MEMORY);
        rv = loader->LoadAgentSheet(uri, getter_AddRefs(sheet));
        // XXXldb We need to prevent bogus sheets from being held in the
        // prototype's list, but until then, don't propagate the failure
        // from LoadAgentSheet (and thus exit the loop).
        if (NS_SUCCEEDED(rv)) {
            AddStyleSheet(sheet, 0);
        }
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsXULDocument::OverlayForwardReference
//

nsForwardReference::Result
nsXULDocument::OverlayForwardReference::Resolve()
{
    // Resolve a forward reference from an overlay element; attempt to
    // hook it up into the main document.
    nsresult rv;

    nsAutoString id;
    rv = mOverlay->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);
    if (NS_FAILED(rv)) return eResolve_Error;

    if (id.IsEmpty()) {
        // overlay had no id, use the root element
        mDocument->InsertElement(mDocument->mRootContent, mOverlay);
        mResolved = PR_TRUE;
        return eResolve_Succeeded;
    }

    nsCOMPtr<nsIDOMElement> domtarget;
    rv = mDocument->GetElementById(id, getter_AddRefs(domtarget));
    if (NS_FAILED(rv)) return eResolve_Error;

    // If we can't find the element in the document, defer the hookup
    // until later.
    if (! domtarget)
        return eResolve_Later;

    nsCOMPtr<nsIContent> target = do_QueryInterface(domtarget);
    NS_ASSERTION(target != nsnull, "not an nsIContent");
    if (! target)
        return eResolve_Error;

    rv = Merge(target, mOverlay);
    if (NS_FAILED(rv)) return eResolve_Error;

    // Add child and any descendants to the element map
    rv = mDocument->AddSubtreeToDocument(target);
    if (NS_FAILED(rv)) return eResolve_Error;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_NOTICE)) {
        nsCAutoString idC;
        idC.AssignWithConversion(id);
        PR_LOG(gXULLog, PR_LOG_NOTICE,
               ("xul: overlay resolved '%s'",
                idC.get()));
    }
#endif

    mResolved = PR_TRUE;
    return eResolve_Succeeded;
}



nsresult
nsXULDocument::OverlayForwardReference::Merge(nsIContent* aTargetNode,
                                              nsIContent* aOverlayNode)
{
    // This function is given:
    // aTargetNode:  the node in the document whose 'id' attribute
    //               matches a toplevel node in our overlay.
    // aOverlayNode: the node in the overlay document that matches
    //               a node in the actual document.
    //
    // This function merges the tree from the overlay into the tree in
    // the document, overwriting attributes and appending child content
    // nodes appropriately. (See XUL overlay reference for details)

    nsresult rv;

    // Merge attributes from the overlay content node to that of the
    // actual document.
    PRUint32 i, attrCount = aOverlayNode->GetAttrCount();

    for (i = 0; i < attrCount; ++i) {
        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> attr, prefix;
        rv = aOverlayNode->GetAttrNameAt(i, &nameSpaceID,
                                         getter_AddRefs(attr),
                                         getter_AddRefs(prefix));
        if (NS_FAILED(rv)) return rv;

        // We don't want to swap IDs, they should be the same.
        if (nameSpaceID == kNameSpaceID_None && attr.get() == nsXULAtoms::id)
            continue;

        nsAutoString value;
        rv = aOverlayNode->GetAttr(nameSpaceID, attr, value);
        if (NS_FAILED(rv)) return rv;

        nsAutoString tempID;
        rv = aOverlayNode->GetAttr(kNameSpaceID_None, nsXULAtoms::id, tempID);

        // Element in the overlay has the 'removeelement' attribute set
        // so remove it from the actual document.
        if (attr == nsXULAtoms::removeelement &&
            value.EqualsLiteral("true")) {

            rv = RemoveElement(aTargetNode->GetParent(), aTargetNode);
            if (NS_FAILED(rv)) return rv;

            return NS_OK;
        }

        rv = aTargetNode->SetAttr(nameSpaceID, attr, prefix, value, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }


    // Walk our child nodes, looking for elements that have the 'id'
    // attribute set. If we find any, we must do a parent check in the
    // actual document to ensure that the structure matches that of
    // the actual document. If it does, we can call ourselves and attempt
    // to merge inside that subtree. If not, we just append the tree to
    // the parent like any other.

    PRUint32 childCount = aOverlayNode->GetChildCount();

    // This must be a strong reference since it will be the only
    // reference to a content object during part of this loop.
    nsCOMPtr<nsIContent> currContent;

    for (i = 0; i < childCount; ++i) {
        currContent = aOverlayNode->GetChildAt(0);

        nsAutoString id;
        rv = currContent->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIDOMElement> nodeInDocument;
        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            nsCOMPtr<nsIDOMDocument> domDocument(
                        do_QueryInterface(aTargetNode->GetDocument()));
            if (!domDocument) return NS_ERROR_FAILURE;

            rv = domDocument->GetElementById(id, getter_AddRefs(nodeInDocument));
            if (NS_FAILED(rv)) return rv;
        }

        // The item has an 'id' attribute set, and we need to check with
        // the actual document to see if an item with this id exists at
        // this locale. If so, we want to merge the subtree under that
        // node. Otherwise, we just do an append as if the element had
        // no id attribute.
        if (nodeInDocument) {
            // Given two parents, aTargetNode and aOverlayNode, we want
            // to call merge on currContent if we find an associated
            // node in the document with the same id as currContent that
            // also has aTargetNode as its parent.

            nsAutoString documentParentID;
            rv = aTargetNode->GetAttr(kNameSpaceID_None, nsXULAtoms::id,
                                      documentParentID);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIDOMNode> nodeParent;
            rv = nodeInDocument->GetParentNode(getter_AddRefs(nodeParent));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIDOMElement> elementParent(do_QueryInterface(nodeParent));

            nsAutoString parentID;
            elementParent->GetAttribute(NS_LITERAL_STRING("id"), parentID);
            if (parentID.Equals(documentParentID)) {
                // The element matches. "Go Deep!"
                nsCOMPtr<nsIContent> childDocumentContent(do_QueryInterface(nodeInDocument));
                rv = Merge(childDocumentContent, currContent);
                if (NS_FAILED(rv)) return rv;
                rv = aOverlayNode->RemoveChildAt(0, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                continue;
            }
        }

        rv = aOverlayNode->RemoveChildAt(0, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        rv = InsertElement(aTargetNode, currContent);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}



nsXULDocument::OverlayForwardReference::~OverlayForwardReference()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING) && !mResolved) {
        nsAutoString id;
        mOverlay->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);

        nsCAutoString idC;
        idC.AssignWithConversion(id);
        PR_LOG(gXULLog, PR_LOG_WARNING,
               ("xul: overlay failed to resolve '%s'",
                idC.get()));
    }
#endif
}


//----------------------------------------------------------------------
//
// nsXULDocument::BroadcasterHookup
//

nsForwardReference::Result
nsXULDocument::BroadcasterHookup::Resolve()
{
    nsresult rv;

    PRBool listener;
    rv = CheckBroadcasterHookup(mDocument, mObservesElement, &listener, &mResolved);
    if (NS_FAILED(rv)) return eResolve_Error;

    return mResolved ? eResolve_Succeeded : eResolve_Later;
}


nsXULDocument::BroadcasterHookup::~BroadcasterHookup()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING) && !mResolved) {
        // Tell the world we failed
        nsresult rv;

        nsIAtom *tag = mObservesElement->Tag();

        nsAutoString broadcasterID;
        nsAutoString attribute;

        if (tag == nsXULAtoms::observes) {
            rv = mObservesElement->GetAttr(kNameSpaceID_None, nsXULAtoms::element, broadcasterID);
            if (NS_FAILED(rv)) return;

            rv = mObservesElement->GetAttr(kNameSpaceID_None, nsXULAtoms::attribute, attribute);
            if (NS_FAILED(rv)) return;
        }
        else {
            rv = mObservesElement->GetAttr(kNameSpaceID_None, nsXULAtoms::observes, broadcasterID);
            if (NS_FAILED(rv)) return;

            attribute.AssignLiteral("*");
        }

        nsAutoString tagStr;
        rv = tag->ToString(tagStr);
        if (NS_FAILED(rv)) return;

        nsCAutoString tagstrC, attributeC,broadcasteridC;
        tagstrC.AssignWithConversion(tagStr);
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_WARNING,
               ("xul: broadcaster hookup failed <%s attribute='%s'> to %s",
                tagstrC.get(),
                attributeC.get(),
                broadcasteridC.get()));
    }
#endif
}


//----------------------------------------------------------------------
//
// nsXULDocument::TemplateBuilderHookup
//

nsForwardReference::Result
nsXULDocument::TemplateBuilderHookup::Resolve()
{
    PRBool needsHookup;
    nsresult rv = CheckTemplateBuilderHookup(mElement, &needsHookup);
    if (NS_FAILED(rv))
        return eResolve_Error;

    if (needsHookup) {
        rv = CreateTemplateBuilder(mElement);
        if (NS_FAILED(rv))
            return eResolve_Error;
    }

    return eResolve_Succeeded;
}


//----------------------------------------------------------------------


nsresult
nsXULDocument::CheckBroadcasterHookup(nsXULDocument* aDocument,
                                      nsIContent* aElement,
                                      PRBool* aNeedsHookup,
                                      PRBool* aDidResolve)
{
    // Resolve a broadcaster hookup. Look at the element that we're
    // trying to resolve: it could be an '<observes>' element, or just
    // a vanilla element with an 'observes' attribute on it.
    nsresult rv;

    *aDidResolve = PR_FALSE;

    nsCOMPtr<nsIDOMElement> listener;
    nsAutoString broadcasterID;
    nsAutoString attribute;

    nsINodeInfo *ni = aElement->GetNodeInfo();

    if (ni && ni->Equals(nsXULAtoms::observes, kNameSpaceID_XUL)) {
        // It's an <observes> element, which means that the actual
        // listener is the _parent_ node. This element should have an
        // 'element' attribute that specifies the ID of the
        // broadcaster element, and an 'attribute' element, which
        // specifies the name of the attribute to observe.
        nsIContent* parent = aElement->GetParent();

        // If we're still parented by an 'overlay' tag, then we haven't
        // made it into the real document yet. Defer hookup.
        if (parent->GetNodeInfo()->Equals(nsXULAtoms::overlay, kNameSpaceID_XUL)) {
            *aNeedsHookup = PR_TRUE;
            return NS_OK;
        }

        listener = do_QueryInterface(parent);

        rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::element, broadcasterID);
        if (NS_FAILED(rv)) return rv;

        rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::attribute, attribute);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // It's a generic element, which means that we'll use the
        // value of the 'observes' attribute to determine the ID of
        // the broadcaster element, and we'll watch _all_ of its
        // values.
        rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::observes, broadcasterID);
        if (NS_FAILED(rv)) return rv;

        // Bail if there's no broadcasterID
        if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || (broadcasterID.IsEmpty())) {
            // Try the command attribute next.
            rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::command, broadcasterID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE && !broadcasterID.IsEmpty()) {
                // We've got something in the command attribute.  We
                // only treat this as a normal broadcaster if we are
                // not a menuitem or a key.

                nsINodeInfo *ni = aElement->GetNodeInfo();
                if (ni->Equals(nsXULAtoms::menuitem, kNameSpaceID_XUL) ||
                    ni->Equals(nsXULAtoms::key, kNameSpaceID_XUL)) {
                *aNeedsHookup = PR_FALSE;
                return NS_OK;
              }
            }
            else {
              *aNeedsHookup = PR_FALSE;
              return NS_OK;
            }
        }

        listener = do_QueryInterface(aElement);

        attribute.AssignLiteral("*");
    }

    // Make sure we got a valid listener.
    NS_ASSERTION(listener != nsnull, "no listener");
    if (! listener)
        return NS_ERROR_UNEXPECTED;

    // Try to find the broadcaster element in the document.
    nsCOMPtr<nsIDOMElement> broadcaster;
    rv = aDocument->GetElementById(broadcasterID, getter_AddRefs(broadcaster));
    if (NS_FAILED(rv)) return rv;

    // If we can't find the broadcaster, then we'll need to defer the
    // hookup. We may need to resolve some of the other overlays
    // first.
    if (! broadcaster) {
        *aNeedsHookup = PR_TRUE;
        return NS_OK;
    }

    rv = aDocument->AddBroadcastListenerFor(broadcaster, listener, attribute);
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    // Tell the world we succeeded
    if (PR_LOG_TEST(gXULLog, PR_LOG_NOTICE)) {
        nsCOMPtr<nsIContent> content =
            do_QueryInterface(listener);

        NS_ASSERTION(content != nsnull, "not an nsIContent");
        if (! content)
            return rv;

        nsAutoString tagStr;
        rv = content->Tag()->ToString(tagStr);
        if (NS_FAILED(rv)) return rv;

        nsCAutoString tagstrC, attributeC,broadcasteridC;
        tagstrC.AssignWithConversion(tagStr);
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_NOTICE,
               ("xul: broadcaster hookup <%s attribute='%s'> to %s",
                tagstrC.get(),
                attributeC.get(),
                broadcasteridC.get()));
    }
#endif

    *aNeedsHookup = PR_FALSE;
    *aDidResolve = PR_TRUE;
    return NS_OK;
}

nsresult
nsXULDocument::InsertElement(nsIContent* aParent, nsIContent* aChild)
{
    // Insert aChild appropriately into aParent, accounting for a
    // 'pos' attribute set on aChild.
    nsresult rv;

    nsAutoString posStr;
    PRBool wasInserted = PR_FALSE;

    // insert after an element of a given id
    rv = aChild->GetAttr(kNameSpaceID_None, nsXULAtoms::insertafter, posStr);
    if (NS_FAILED(rv)) return rv;
    PRBool isInsertAfter = PR_TRUE;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        rv = aChild->GetAttr(kNameSpaceID_None, nsXULAtoms::insertbefore,
                             posStr);
        if (NS_FAILED(rv)) return rv;
        isInsertAfter = PR_FALSE;
    }

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsCOMPtr<nsIDOMDocument> domDocument(
               do_QueryInterface(aParent->GetDocument()));
        nsCOMPtr<nsIDOMElement> domElement;

        char* str = ToNewCString(posStr);
        char* rest;
        char* token = nsCRT::strtok(str, ", ", &rest);

        while (token) {
            rv = domDocument->GetElementById(NS_ConvertASCIItoUCS2(token),
                                             getter_AddRefs(domElement));
            if (domElement)
                break;

            token = nsCRT::strtok(rest, ", ", &rest);
        }
        nsMemory::Free(str);
        if (NS_FAILED(rv))
            return rv;

        if (domElement) {
            nsCOMPtr<nsIContent> content(do_QueryInterface(domElement));
            NS_ASSERTION(content != nsnull, "null ptr");
            if (!content)
                return NS_ERROR_UNEXPECTED;

            PRInt32 pos = aParent->IndexOf(content);

            if (pos != -1) {
                pos = isInsertAfter ? pos + 1 : pos;
                rv = aParent->InsertChildAt(aChild, pos, PR_FALSE, PR_TRUE);
                if (NS_FAILED(rv))
                    return rv;

                wasInserted = PR_TRUE;
            }
        }
    }

    if (!wasInserted) {

        rv = aChild->GetAttr(kNameSpaceID_None, nsXULAtoms::position, posStr);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            // Positions are one-indexed.
            PRInt32 pos = posStr.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv));
            if (NS_SUCCEEDED(rv)) {
                rv = aParent->InsertChildAt(aChild, pos - 1, PR_FALSE,
                                            PR_TRUE);
                if (NS_SUCCEEDED(rv))
                    wasInserted = PR_TRUE;
                // If the insertion fails, then we should still
                // attempt an append.  Thus, rather than returning rv
                // immediately, we fall through to the final
                // "catch-all" case that just does an AppendChildTo.
            }
        }
    }

    if (! wasInserted) {
        rv = aParent->AppendChildTo(aChild, PR_FALSE, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

nsresult
nsXULDocument::RemoveElement(nsIContent* aParent, nsIContent* aChild)
{
    PRInt32 nodeOffset = aParent->IndexOf(aChild);

    return aParent->RemoveChildAt(nodeOffset, PR_TRUE);
}

//----------------------------------------------------------------------
//
// CachedChromeStreamListener
//

nsXULDocument::CachedChromeStreamListener::CachedChromeStreamListener(nsXULDocument* aDocument, PRBool aProtoLoaded)
    : mDocument(aDocument),
      mProtoLoaded(aProtoLoaded)
{
    NS_ADDREF(mDocument);
}


nsXULDocument::CachedChromeStreamListener::~CachedChromeStreamListener()
{
    NS_RELEASE(mDocument);
}


NS_IMPL_ISUPPORTS2(nsXULDocument::CachedChromeStreamListener,
                   nsIRequestObserver, nsIStreamListener)

NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnStartRequest(nsIRequest *request,
                                                          nsISupports* acontext)
{
    return NS_OK;
}


NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnStopRequest(nsIRequest *request,
                                                         nsISupports* aContext,
                                                         nsresult aStatus)
{
    if (! mProtoLoaded)
        return NS_OK;

    nsresult rv;
    rv = mDocument->PrepareToWalk();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to prepare for walk");
    if (NS_FAILED(rv)) return rv;

    return mDocument->ResumeWalk();
}


NS_IMETHODIMP
nsXULDocument::CachedChromeStreamListener::OnDataAvailable(nsIRequest *request,
                                                           nsISupports* aContext,
                                                           nsIInputStream* aInStr,
                                                           PRUint32 aSourceOffset,
                                                           PRUint32 aCount)
{
    NS_NOTREACHED("CachedChromeStream doesn't receive data");
    return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
//
// ParserObserver
//

nsXULDocument::ParserObserver::ParserObserver(nsXULDocument* aDocument)
    : mDocument(aDocument)
{
    NS_ADDREF(mDocument);
}

nsXULDocument::ParserObserver::~ParserObserver()
{
    NS_IF_RELEASE(mDocument);
}

NS_IMPL_ISUPPORTS1(nsXULDocument::ParserObserver, nsIRequestObserver)

NS_IMETHODIMP
nsXULDocument::ParserObserver::OnStartRequest(nsIRequest *request,
                                              nsISupports* aContext)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::ParserObserver::OnStopRequest(nsIRequest *request,
                                             nsISupports* aContext,
                                             nsresult aStatus)
{
    nsresult rv = NS_OK;

    if (NS_FAILED(aStatus)) {
        // If an overlay load fails, we need to nudge the prototype
        // walk along.
#define YELL_IF_MISSING_OVERLAY 1
#if defined(DEBUG) || defined(YELL_IF_MISSING_OVERLAY)

        nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
        if (!aChannel) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIURI> uri;
        aChannel->GetOriginalURI(getter_AddRefs(uri));

        nsCAutoString spec;
        uri->GetSpec(spec);

        printf("*** Failed to load overlay %s\n", spec.get());
#endif

        rv = mDocument->ResumeWalk();
    }

    // Drop the reference to the document to break cycle between the
    // document, the parser, the content sink, and the parser
    // observer.
    NS_RELEASE(mDocument);

    return rv;
}

void
nsXULDocument::GetFocusController(nsIFocusController** aFocusController)
{
    nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryReferent(mDocumentContainer);
    nsCOMPtr<nsPIDOMWindow> windowPrivate = do_GetInterface(ir);
    if (windowPrivate) {
        NS_IF_ADDREF(*aFocusController = windowPrivate->GetRootFocusController());
    } else
        *aFocusController = nsnull;
}
