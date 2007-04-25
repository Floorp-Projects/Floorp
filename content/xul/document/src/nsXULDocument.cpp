/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=80: */
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
#include "nsIScrollableView.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIContentViewer.h"
#include "nsGUIEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMXULElement.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIStreamListener.h"
#include "nsITimer.h"
#include "nsIDocShell.h"
#include "nsGkAtoms.h"
#include "nsXMLContentSink.h"
#include "nsXULContentSink.h"
#include "nsXULContentUtils.h"
#include "nsIXULOverlayProvider.h"
#include "nsNetUtil.h"
#include "nsParserUtils.h"
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
#include "nsIScriptRuntime.h"
#include "nsIScriptSecurityManager.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsIParser.h"
#include "nsIParserService.h"
#include "nsICSSStyleSheet.h"
#include "nsIScriptError.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsEventDispatcher.h"
#include "nsContentErrors.h"
#include "nsIObserverService.h"
#include "nsNodeUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIXULWindow.h"

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

const PRUint32 kMaxAttrNameLength = 512;
const PRUint32 kMaxAttributeLength = 4096;

//----------------------------------------------------------------------
//
// Statics
//

PRInt32 nsXULDocument::gRefCnt = 0;

nsIRDFService* nsXULDocument::gRDFService;
nsIRDFResource* nsXULDocument::kNC_persist;
nsIRDFResource* nsXULDocument::kNC_attribute;
nsIRDFResource* nsXULDocument::kNC_value;

PRLogModuleInfo* nsXULDocument::gXULLog;

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
    : nsXMLDocument("application/vnd.mozilla.xul+xml"),
      mResolutionPhase(nsForwardReference::eStart),
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

        // Remove the current document here from the FastLoad table in
        // case the document did not make it past StartLayout in
        // ResumeWalk. The FastLoad table must be clear of entries so
        // that the FastLoad file footer can be properly written.
        if (mDocumentURI)
            nsXULPrototypeCache::GetInstance()->RemoveFromFastLoadSet(mDocumentURI);
    }

    // The destructor of nsDocument will delete references to style
    // sheets, but we don't want that if we're a popup document, so
    // then we'll clear the stylesheets array here to prevent that
    // from happening.
    if (mIsPopup) {
        mStyleSheets.Clear();
        mStyleAttrStyleSheet = nsnull;
        mAttrStyleSheet = nsnull;
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULDocument)

static PRIntn
TraverseElement(const PRUnichar* aID, nsIContent* aElement, void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        NS_STATIC_CAST(nsCycleCollectionTraversalCallback*, aContext);

    cb->NoteXPCOMChild(aElement);

    return HT_ENUMERATE_NEXT;
}

static PLDHashOperator PR_CALLBACK
TraverseTemplateBuilders(nsISupports* aKey, nsIXULTemplateBuilder* aData,
                         void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        NS_STATIC_CAST(nsCycleCollectionTraversalCallback*, aContext);

    cb->NoteXPCOMChild(aKey);
    cb->NoteXPCOMChild(aData);

    return PL_DHASH_NEXT;
}

static PLDHashOperator PR_CALLBACK
TraverseObservers(nsIURI* aKey, nsIObserver* aData, void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        NS_STATIC_CAST(nsCycleCollectionTraversalCallback*, aContext);

    cb->NoteXPCOMChild(aData);

    return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXULDocument, nsXMLDocument)
    // XXX tmp->mForwardReferences?
    // XXX tmp->mContextStack?

    tmp->mElementMap.Enumerate(TraverseElement, &cb);

    // An element will only have a template builder as long as it's in the
    // document, so we'll traverse the table here instead of from the element.
    if (tmp->mTemplateBuilderTable)
        tmp->mTemplateBuilderTable->EnumerateRead(TraverseTemplateBuilders, &cb);
        
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mCurrentPrototype,
                                                     nsIScriptGlobalObjectOwner)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mMasterPrototype,
                                                     nsIScriptGlobalObjectOwner)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mCommandDispatcher,
                                                     nsIDOMXULCommandDispatcher)

    PRUint32 i, count = tmp->mPrototypes.Length();
    for (i = 0; i < count; ++i) {
        cb.NoteXPCOMChild(NS_STATIC_CAST(nsIScriptGlobalObjectOwner*, 
                                         tmp->mPrototypes[i]));
    }
    
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTooltipNode)

    if (tmp->mOverlayLoadObservers.IsInitialized())
        tmp->mOverlayLoadObservers.EnumerateRead(TraverseObservers, &cb);
    if (tmp->mPendingOverlayLoadNotifications.IsInitialized())
        tmp->mPendingOverlayLoadNotifications.EnumerateRead(TraverseObservers, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsXULDocument, nsXMLDocument)
NS_IMPL_RELEASE_INHERITED(nsXULDocument, nsXMLDocument)


// QueryInterface implementation for nsXULDocument
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXULDocument)
    NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
    NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
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
nsXULDocument::ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal)
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

// This is called when the master document begins loading, whether it's
// fastloaded or not.
nsresult
nsXULDocument::StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset, nsIContentSink* aSink)
{
    // NOTE: If this ever starts calling nsDocument::StartDocumentLoad
    // we'll possibly need to reset our content type afterwards.
    mStillWalking = PR_TRUE;
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);

    mDocumentTitle.SetIsVoid(PR_TRUE);

    mChannel = aChannel;

    // Get the URI.  Note that this should match nsDocShell::OnLoadingSite
    // XXXbz this code is repeated from nsDocument::Reset and
    // nsScriptSecurityManager::GetChannelPrincipal; we really need to refactor
    // this part better.
    nsLoadFlags loadFlags = 0;
    nsresult rv = aChannel->GetLoadFlags(&loadFlags);
    if (NS_SUCCEEDED(rv)) {
        if (loadFlags & nsIChannel::LOAD_REPLACE) {
            rv = aChannel->GetURI(getter_AddRefs(mDocumentURI));
        } else {
            rv = aChannel->GetOriginalURI(getter_AddRefs(mDocumentURI));
        }
    }
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = ResetStylesheetsToURI(mDocumentURI);
    if (NS_FAILED(rv)) return rv;

    RetrieveRelevantHeaders(aChannel);

    // Look in the chrome cache: we've got this puppy loaded
    // already.
    nsXULPrototypeDocument* proto = IsChromeURI(mDocumentURI) ?
            nsXULPrototypeCache::GetInstance()->GetPrototype(mDocumentURI) :
            nsnull;

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

        // Set up the right principal on ourselves.
        SetPrincipal(proto->DocumentPrincipal());

        // We need a listener, even if proto is not yet loaded, in which
        // event the listener's OnStopRequest method does nothing, and all
        // the interesting work happens below nsXULDocument::EndLoad, from
        // the call there to mCurrentPrototype->NotifyLoadDone().
        *aDocListener = new CachedChromeStreamListener(this, loaded);
        if (! *aDocListener)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        PRBool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();
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
            nsXULPrototypeCache::GetInstance()->PutPrototype(mCurrentPrototype);
        }
    }

    NS_IF_ADDREF(*aDocListener);
    return NS_OK;
}

// This gets invoked after a prototype for this document or one of
// its overlays is fully built in the content sink.
void
nsXULDocument::EndLoad()
{
    // This can happen if an overlay fails to load
    if (!mCurrentPrototype)
        return;

    nsresult rv;

    // Whack the prototype document into the cache so that the next
    // time somebody asks for it, they don't need to load it by hand.

    nsCOMPtr<nsIURI> uri = mCurrentPrototype->GetURI();
    PRBool isChrome = IsChromeURI(uri);

    // Remember if the XUL cache is on
    PRBool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();

    // If the current prototype is an overlay document (non-master prototype)
    // and we're filling the FastLoad disk cache, tell the cache we're done
    // loading it, and write the prototype. The master prototype is put into
    // the cache earlier in nsXULDocument::StartDocumentLoad.
    if (useXULCache && mIsWritingFastLoad && isChrome &&
        mMasterPrototype != mCurrentPrototype) {
        nsXULPrototypeCache::GetInstance()->WritePrototype(mCurrentPrototype);
    }

    if (isChrome) {
        nsCOMPtr<nsIXULOverlayProvider> reg =
            do_GetService(NS_CHROMEREGISTRY_CONTRACTID);

        if (reg) {
            nsCOMPtr<nsISimpleEnumerator> overlays;
            rv = reg->GetStyleOverlays(uri, getter_AddRefs(overlays));
            if (NS_FAILED(rv)) return;

            PRBool moreSheets;
            nsCOMPtr<nsISupports> next;
            nsCOMPtr<nsIURI> sheetURI;

            while (NS_SUCCEEDED(rv = overlays->HasMoreElements(&moreSheets)) &&
                   moreSheets) {
                overlays->GetNext(getter_AddRefs(next));

                sheetURI = do_QueryInterface(next);
                if (!sheetURI) {
                    NS_ERROR("Chrome registry handed me a non-nsIURI object!");
                    continue;
                }

                if (IsChromeURI(sheetURI)) {
                    mCurrentPrototype->AddStyleSheetReference(sheetURI);
                }
            }
        }

        if (useXULCache) {
            // If it's a chrome prototype document, then notify any
            // documents that raced to load the prototype, and awaited
            // its load completion via proto->AwaitLoadDone().
            rv = mCurrentPrototype->NotifyLoadDone();
            if (NS_FAILED(rv)) return;
        }
    }

    OnPrototypeLoadDone(PR_TRUE);
}

NS_IMETHODIMP
nsXULDocument::OnPrototypeLoadDone(PRBool aResumeWalk)
{
    nsresult rv;

    // Add the style overlays from chrome registry, if any.
    rv = AddPrototypeSheets();
    if (NS_FAILED(rv)) return rv;

    rv = PrepareToWalk();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to prepare for walk");
    if (NS_FAILED(rv)) return rv;

    if (aResumeWalk) {
        rv = ResumeWalk();
    }
    return rv;
}

// called when an error occurs parsing a document
PRBool
nsXULDocument::OnDocumentParserError()
{
  // don't report errors that are from overlays
  if (mCurrentPrototype && mMasterPrototype != mCurrentPrototype) {
    nsCOMPtr<nsIURI> uri = mCurrentPrototype->GetURI();
    if (IsChromeURI(uri)) {
      nsCOMPtr<nsIObserverService> os(
        do_GetService("@mozilla.org/observer-service;1"));
      if (os)
        os->NotifyObservers(uri, "xul-overlay-parsererror",
                            EmptyString().get());
    }

    return PR_FALSE;
  }

  return PR_TRUE;
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
        if ((aAttribute == nsGkAtoms::id) ||
            (aAttribute == nsGkAtoms::ref) ||
            (aAttribute == nsGkAtoms::persist)) {
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

	// We may be copying event handlers etc, so we must also copy
	// the script-type to the listener.
	listener->SetScriptTypeID(broadcaster->GetScriptTypeID());

    if (aAttr.EqualsLiteral("*")) {
        PRUint32 count = broadcaster->GetAttrCount();
        while (count-- > 0) {
            const nsAttrName* attrName = broadcaster->GetAttrNameAt(count);
            PRInt32 nameSpaceID = attrName->NamespaceID();
            nsIAtom* name = attrName->LocalName();

            // _Don't_ push the |id|, |ref|, or |persist| attribute's value!
            if (! CanBroadcast(nameSpaceID, name))
                continue;

            nsAutoString value;
            broadcaster->GetAttr(nameSpaceID, name, value);
            listener->SetAttr(nameSpaceID, name, attrName->GetPrefix(), value,
                              PR_FALSE);

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
        if (broadcaster->GetAttr(kNameSpaceID_None, name, value)) {
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

        if (!child->NodeInfo()->Equals(nsGkAtoms::observes, kNameSpaceID_XUL))
            continue;

        // Is this the element that was listening to us?
        nsAutoString listeningToID;
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::element, listeningToID);

        nsAutoString broadcasterID;
        aBroadcaster->GetAttr(kNameSpaceID_None, nsGkAtoms::id, broadcasterID);

        if (listeningToID != broadcasterID)
            continue;

        // We are observing the broadcaster, but is this the right
        // attribute?
        nsAutoString listeningToAttribute;
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::attribute,
                       listeningToAttribute);

        if (!aAttr->Equals(listeningToAttribute) &&
            !listeningToAttribute.EqualsLiteral("*")) {
            continue;
        }

        // This is the right <observes> element. Execute the
        // |onbroadcast| event handler
        nsEvent event(PR_TRUE, NS_XUL_BROADCAST);

        PRInt32 j = GetNumberOfShells();
        while (--j >= 0) {
            nsCOMPtr<nsIPresShell> shell = GetShellAt(j);

            nsCOMPtr<nsPresContext> aPresContext = shell->GetPresContext();

            // Handle the DOM event
            nsEventStatus status = nsEventStatus_eIgnore;
            nsEventDispatcher::Dispatch(child, aPresContext, &event, nsnull,
                                        &status);
        }
    }

    return NS_OK;
}

void
nsXULDocument::AttributeChanged(nsIDocument* aDocument,
                                nsIContent* aElement, PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute, PRInt32 aModType)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");

    nsresult rv;

    // XXXbz check aNameSpaceID, dammit!
    // First see if we need to update our element map.
    if ((aAttribute == nsGkAtoms::id) || (aAttribute == nsGkAtoms::ref)) {

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
            PRBool attrSet = aElement->GetAttr(kNameSpaceID_None, aAttribute, value);

            for (PRInt32 i = entry->mListeners.Count() - 1; i >= 0; --i) {
                BroadcastListener* bl =
                    NS_STATIC_CAST(BroadcastListener*, entry->mListeners[i]);

                if ((bl->mAttribute == aAttribute) ||
                    (bl->mAttribute == nsGkAtoms::_asterix)) {
                    nsCOMPtr<nsIContent> listener
                        = do_QueryInterface(bl->mListener);

                    if (attrSet) {
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

    // See if there is anything we need to persist in the localstore.
    //
    // XXX Namespace handling broken :-(
    nsAutoString persist;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::persist, persist);
    if (!persist.IsEmpty()) {
        nsAutoString attr;
        rv = aAttribute->ToString(attr);
        if (NS_FAILED(rv)) return;

        // XXXldb This should check that it's a token, not just a substring.
        if (persist.Find(attr) >= 0) {
            rv = Persist(aElement, kNameSpaceID_None, aAttribute);
            if (NS_FAILED(rv)) return;
        }
    }
}

void
nsXULDocument::ContentAppended(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");

    // Update our element map
    PRUint32 count = aContainer->GetChildCount();

    nsresult rv = NS_OK;
    for (PRUint32 i = aNewIndexInContainer; i < count && NS_SUCCEEDED(rv);
         ++i) {
        rv = AddSubtreeToDocument(aContainer->GetChildAt(i));
    }
}

void
nsXULDocument::ContentInserted(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");

    AddSubtreeToDocument(aChild);
}

void
nsXULDocument::ContentRemoved(nsIDocument* aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");

    RemoveSubtreeFromDocument(aChild);
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
                                nsCOMArray<nsIContent>& aElements)
{
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
nsXULDocument::GetScriptGlobalObjectOwner(nsIScriptGlobalObjectOwner** aGlobalOwner)
{
    NS_IF_ADDREF(*aGlobalOwner = mMasterPrototype);
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
    void* attrValue = new nsString(aValue);
    NS_ENSURE_TRUE(attrValue, NS_ERROR_OUT_OF_MEMORY);
    nsContentList *list = new nsContentList(this,
                                            MatchAttribute,
                                            nsContentUtils::DestroyMatchString,
                                            attrValue,
                                            PR_TRUE,
                                            attrAtom,
                                            kNameSpaceID_Unknown);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    
    NS_ADDREF(*aReturn = list);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetElementsByAttributeNS(const nsAString& aNamespaceURI,
                                        const nsAString& aAttribute,
                                        const nsAString& aValue,
                                        nsIDOMNodeList** aReturn)
{
    nsCOMPtr<nsIAtom> attrAtom(do_GetAtom(aAttribute));
    NS_ENSURE_TRUE(attrAtom, NS_ERROR_OUT_OF_MEMORY);
    void* attrValue = new nsString(aValue);
    NS_ENSURE_TRUE(attrValue, NS_ERROR_OUT_OF_MEMORY);

    PRInt32 nameSpaceId = kNameSpaceID_Wildcard;
    if (!aNamespaceURI.EqualsLiteral("*")) {
      nsresult rv =
        nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                              nameSpaceId);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsContentList *list = new nsContentList(this,
                                            MatchAttribute,
                                            nsContentUtils::DestroyMatchString,
                                            attrValue,
                                            PR_TRUE,
                                            attrAtom,
                                            nameSpaceId);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aReturn = list);
    return NS_OK;
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
        // Make sure that this QName is going to be valid.
        nsIParserService *parserService = nsContentUtils::GetParserService();
        NS_ASSERTION(parserService, "Running scripts during shutdown?");

        const PRUnichar *colon;
        rv = parserService->CheckQName(PromiseFlatString(aAttr), PR_TRUE, &colon);
        if (NS_FAILED(rv)) {
            // There was an invalid character or it was malformed.
            return NS_ERROR_INVALID_ARG;
        }

        if (colon) {
            // We don't really handle namespace qualifiers in attribute names.
            return NS_ERROR_NOT_IMPLEMENTED;
        }

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

    // Don't bother with unreasonable attributes. We clamp long values,
    // but truncating attribute names turns it into a different attribute
    // so there's no point in persisting anything at all
    if (!attrstr || strlen(attrstr) > kMaxAttrNameLength) {
        NS_WARNING("Can't persist, Attribute name too long");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsCOMPtr<nsIRDFResource> attr;
    rv = gRDFService->GetResource(nsDependentCString(attrstr),
                                  getter_AddRefs(attr));
    if (NS_FAILED(rv)) return rv;

    // Turn the value into a literal
    nsAutoString valuestr;
    aElement->GetAttr(kNameSpaceID_None, aAttribute, valuestr);

    // prevent over-long attributes that choke the parser (bug 319846)
    // (can't simply Truncate without testing, it's implemented
    // using SetLength and will grow a short string)
    if (valuestr.Length() > kMaxAttributeLength) {
        NS_WARNING("Truncating persisted attribute value");
        valuestr.Truncate(kMaxAttributeLength);
    }

    // See if there was an old value...
    nsCOMPtr<nsIRDFNode> oldvalue;
    rv = mLocalStore->GetTarget(element, attr, PR_TRUE, getter_AddRefs(oldvalue));
    if (NS_FAILED(rv)) return rv;

    if (oldvalue && valuestr.IsEmpty()) {
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

    FlushPendingNotifications(Flush_Layout);

    nsIFrame* frame =
        mRootContent ? aShell->GetPrimaryFrameFor(mRootContent) : nsnull;
    if (frame) {
        nsIView* view = frame->GetView();
        // If we have a view check if it's scrollable. If not,
        // just use the view size itself
        if (view) {
            nsIScrollableView* scrollableView = view->ToScrollableView();

            if (scrollableView) {
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

        *aWidth = nsPresContext::AppUnitsToIntCSSPixels(size.width);
        *aHeight = nsPresContext::AppUnitsToIntCSSPixels(size.height);
    }
    else {
        *aWidth = 0;
        *aHeight = 0;
        result = NS_ERROR_FAILURE;
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
    // Get popup node.
    nsresult rv = TrustedGetPopupNode(aNode); // addref happens here

    if (NS_SUCCEEDED(rv) && *aNode && !nsContentUtils::CanCallerAccess(*aNode)) {
        NS_RELEASE(*aNode);
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    return rv;
}

NS_IMETHODIMP
nsXULDocument::TrustedGetPopupNode(nsIDOMNode** aNode)
{
    // Get the focus controller.
    nsCOMPtr<nsIFocusController> focusController;
    GetFocusController(getter_AddRefs(focusController));
    NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);

    // Get the popup node.
    return focusController->GetPopupNode(aNode); // addref happens here
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
nsXULDocument::GetTrustedPopupEvent(nsIDOMEvent** aEvent)
{
    nsresult rv;

    nsCOMPtr<nsIFocusController> focusController;
    GetFocusController(getter_AddRefs(focusController));
    NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);

    rv = focusController->GetPopupEvent(aEvent);

    return rv;
}

NS_IMETHODIMP
nsXULDocument::SetTrustedPopupEvent(nsIDOMEvent* aEvent)
{
    nsresult rv;

    nsCOMPtr<nsIFocusController> focusController;
    GetFocusController(getter_AddRefs(focusController));
    NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);

    rv = focusController->SetPopupEvent(aEvent);

    return rv;
}

// Returns the rangeOffset element from the popupEvent. This is for chrome
// callers only.
NS_IMETHODIMP
nsXULDocument::GetPopupRangeParent(nsIDOMNode** aRangeParent)
{
    NS_ENSURE_ARG_POINTER(aRangeParent);
    *aRangeParent = nsnull;

    nsCOMPtr<nsIDOMEvent> event;
    nsresult rv = GetTrustedPopupEvent(getter_AddRefs(event));
    NS_ENSURE_SUCCESS(rv, rv);
    if (! event)
        return NS_ERROR_UNEXPECTED; // no event active

    nsCOMPtr<nsIDOMNSUIEvent> uiEvent = do_QueryInterface(event, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = uiEvent->GetRangeParent(aRangeParent); // addrefs

    if (NS_SUCCEEDED(rv) && *aRangeParent &&
            !nsContentUtils::CanCallerAccess(*aRangeParent)) {
        NS_RELEASE(*aRangeParent);
        return NS_ERROR_DOM_SECURITY_ERR;
    }
    return rv;
}

// Returns the rangeOffset element from the popupEvent. We check the rangeParent
// to determine if the caller has rights to access to the data.
NS_IMETHODIMP
nsXULDocument::GetPopupRangeOffset(PRInt32* aRangeOffset)
{
    NS_ENSURE_ARG_POINTER(aRangeOffset);

    nsCOMPtr<nsIDOMEvent> event;
    nsresult rv = GetTrustedPopupEvent(getter_AddRefs(event));
    NS_ENSURE_SUCCESS(rv, rv);
    if (! event)
        return NS_ERROR_UNEXPECTED; // no event active

    nsCOMPtr<nsIDOMNSUIEvent> uiEvent = do_QueryInterface(event, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> parent;
    rv = uiEvent->GetRangeParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);

    if (parent && !nsContentUtils::CanCallerAccess(parent))
        return NS_ERROR_DOM_SECURITY_ERR;

    return uiEvent->GetRangeOffset(aRangeOffset);
}

NS_IMETHODIMP
nsXULDocument::GetTooltipNode(nsIDOMNode** aNode)
{
    if (mTooltipNode && !nsContentUtils::CanCallerAccess(mTooltipNode)) {
        return NS_ERROR_DOM_SECURITY_ERR;
    }
    *aNode = mTooltipNode;
    NS_IF_ADDREF(*aNode);
    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::TrustedGetTooltipNode(nsIDOMNode** aNode)
{
    NS_IF_ADDREF(*aNode = mTooltipNode);
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

    if (!CheckGetElementByIdArg(aId))
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
    if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::commandupdater,
                              nsGkAtoms::_true, eCaseMatters)) {
        rv = nsXULContentUtils::SetCommandUpdater(this, aElement);
        if (NS_FAILED(rv)) return rv;
    }

    // 3. Check for a broadcaster hookup attribute, in which case
    // we'll hook the node up as a listener on a broadcaster.
    PRBool listener, resolved;
    rv = CheckBroadcasterHookup(aElement, &listener, &resolved);
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
    // We need to pay special attention to the keyset tag to set up a listener
    if (aElement->NodeInfo()->Equals(nsGkAtoms::keyset, kNameSpaceID_XUL)) {
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
    // From here on we only care about elements.
    if (!aElement->IsNodeOfType(nsINode::eELEMENT)) {
        return NS_OK;
    }

    // Do pre-order addition magic
    nsresult rv = AddElementToDocumentPre(aElement);
    if (NS_FAILED(rv)) return rv;

    // Recurse to children
    nsXULElement *xulcontent = nsXULElement::FromContent(aElement);

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
    // From here on we only care about elements.
    if (!aElement->IsNodeOfType(nsINode::eELEMENT)) {
        return NS_OK;
    }

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
    if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::commandupdater,
                              nsGkAtoms::_true, eCaseMatters)) {
        nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(aElement);
        NS_ASSERTION(domelement != nsnull, "not a DOM element");
        if (! domelement)
            return NS_ERROR_UNEXPECTED;

        rv = mCommandDispatcher->RemoveCommandUpdater(domelement);
        if (NS_FAILED(rv)) return rv;
    }

    // 4. Remove the element from our broadcaster map, since it is no longer
    // in the document.
    nsCOMPtr<nsIDOMElement> broadcaster, listener;
    nsAutoString attribute, broadcasterID;
    rv = FindBroadcaster(aElement, getter_AddRefs(listener),
                         broadcasterID, attribute, getter_AddRefs(broadcaster));
    if (rv == NS_FINDBROADCASTER_FOUND) {
        RemoveBroadcastListenerFor(broadcaster, listener, attribute);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::SetTemplateBuilderFor(nsIContent* aContent,
                                     nsIXULTemplateBuilder* aBuilder)
{
    if (! mTemplateBuilderTable) {
        mTemplateBuilderTable = new BuilderTable;
        if (! mTemplateBuilderTable || !mTemplateBuilderTable->Init()) {
            mTemplateBuilderTable = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    if (aBuilder) {
        mTemplateBuilderTable->Put(aContent, aBuilder);
    }
    else {
        mTemplateBuilderTable->Remove(aContent);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::GetTemplateBuilderFor(nsIContent* aContent,
                                     nsIXULTemplateBuilder** aResult)
{
    if (mTemplateBuilderTable) {
        mTemplateBuilderTable->Get(aContent, aResult);
    }
    else
        *aResult = nsnull;

    return NS_OK;
}

// Attributes that are used with getElementById() and the
// resource-to-element map.
nsIAtom** nsXULDocument::kIdentityAttrs[] =
{
    &nsGkAtoms::id,
    &nsGkAtoms::ref,
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
        aElement->GetAttr(kNameSpaceID_None, *kIdentityAttrs[i], value);
        if (!value.IsEmpty()) {
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
        aElement->GetAttr(kNameSpaceID_None, *kIdentityAttrs[i], value);
        if (!value.IsEmpty()) {
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
    mCommandDispatcher = new nsXULCommandDispatcher(this);
    NS_ENSURE_TRUE(mCommandDispatcher, NS_ERROR_OUT_OF_MEMORY);

    // this _could_ fail; e.g., if we've tried to grab the local store
    // before profiles have initialized. If so, no big deal; nothing
    // will persist.
    mLocalStore = do_GetService(kLocalStoreCID);

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

        // ensure that the XUL prototype cache is instantiated successfully,
        // so that we can use nsXULPrototypeCache::GetInstance() without
        // null-checks in the rest of the class.
        nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
        if (!cache) {
          NS_ERROR("Could not instantiate nsXULPrototypeCache");
          return NS_ERROR_FAILURE;
        }
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
        nsPresContext *cx = shell->GetPresContext();
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
        nsresult rv = NS_OK;
        nsIViewManager* vm = shell->GetViewManager();
        if (vm) {
            nsCOMPtr<nsIContentViewer> contentViewer;
            rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
            if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
                PRBool enabled;
                contentViewer->GetEnableRendering(&enabled);
                if (enabled) {
                    vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
                }
            }
        }

        rv = shell->InitialReflow(r.width, r.height);
        NS_ENSURE_SUCCESS(rv, rv);

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
                              void* aData)
{
    NS_PRECONDITION(aContent, "Must have content node to work with!");
    nsString* attrValue = NS_STATIC_CAST(nsString*, aData);
    if (aNamespaceID != kNameSpaceID_Unknown &&
        aNamespaceID != kNameSpaceID_Wildcard) {
        return attrValue->EqualsLiteral("*") ?
            aContent->HasAttr(aNamespaceID, aAttrName) :
            aContent->AttrValueIs(aNamespaceID, aAttrName, *attrValue,
                                  eCaseMatters);
    }

    // Qualified name match. This takes more work.

    PRUint32 count = aContent->GetAttrCount();
    for (PRUint32 i = 0; i < count; ++i) {
        const nsAttrName* name = aContent->GetAttrNameAt(i);
        PRBool nameMatch;
        if (name->IsAtom()) {
            nameMatch = name->Atom() == aAttrName;
        } else if (aNamespaceID == kNameSpaceID_Wildcard) {
            nameMatch = name->NodeInfo()->Equals(aAttrName);
        } else {
            nameMatch = name->NodeInfo()->QualifiedNameEquals(aAttrName);
        }

        if (nameMatch) {
            return attrValue->EqualsLiteral("*") ||
                aContent->AttrValueIs(name->NamespaceID(), name->LocalName(),
                                      *attrValue, eCaseMatters);
        }
    }

    return PR_FALSE;
}

nsresult
nsXULDocument::PrepareToLoad(nsISupports* aContainer,
                             const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsIParser** aResult)
{
    // Get the document's principal
    nsCOMPtr<nsIPrincipal> principal;
    nsContentUtils::GetSecurityManager()->
        GetChannelPrincipal(aChannel, getter_AddRefs(principal));
    return PrepareToLoadPrototype(mDocumentURI, aCommand, principal, aResult);
}


nsresult
nsXULDocument::PrepareToLoadPrototype(nsIURI* aURI, const char* aCommand,
                                      nsIPrincipal* aDocumentPrincipal,
                                      nsIParser** aResult)
{
    nsresult rv;

    // Create a new prototype document.
    rv = NS_NewXULPrototypeDocument(getter_AddRefs(mCurrentPrototype));
    if (NS_FAILED(rv)) return rv;

    rv = mCurrentPrototype->InitPrincipal(aURI, aDocumentPrincipal);
    if (NS_FAILED(rv)) {
        mCurrentPrototype = nsnull;
        return rv;
    }    

    // Bootstrap the master document prototype.
    if (! mMasterPrototype) {
        mMasterPrototype = mCurrentPrototype;
        // Set our principal based on the master proto.
        SetPrincipal(aDocumentPrincipal);
    }

    // Create a XUL content sink, a parser, and kick off a load for
    // the overlay.
    nsRefPtr<XULContentSinkImpl> sink = new XULContentSinkImpl();
    if (!sink) return NS_ERROR_OUT_OF_MEMORY;

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

    nsCOMArray<nsIContent> elements;

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
        nsXULContentUtils::MakeElementID(this, nsDependentCString(uri), id);

        if (id.IsEmpty())
            continue;

        // This will clear the array if there are no elements.
        GetElementsForID(id, elements);

        if (!elements.Count())
            continue;

        ApplyPersistentAttributesToElements(resource, elements);
    }

    mApplyingPersistedAttrs = PR_FALSE;

    return NS_OK;
}


nsresult
nsXULDocument::ApplyPersistentAttributesToElements(nsIRDFResource* aResource,
                                                   nsCOMArray<nsIContent>& aElements)
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

        PRUint32 cnt = aElements.Count();

        for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
            nsCOMPtr<nsIContent> element = aElements.SafeObjectAt(i);
            if (!element)
                continue;

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

            if (element->NodeInfo()->Equals(nsGkAtoms::_template,
                                            kNameSpaceID_XUL)) {
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
    mPrototypes.AppendElement(mCurrentPrototype);

    // Get the prototype's root element and initialize the context
    // stack for the prototype walk.
    nsXULPrototypeElement* proto = mCurrentPrototype->GetRootElement();

    if (! proto) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULLog, PR_LOG_ERROR)) {
            nsCOMPtr<nsIURI> url = mCurrentPrototype->GetURI();

            nsCAutoString urlspec;
            rv = url->GetSpec(urlspec);
            if (NS_FAILED(rv)) return rv;

            PR_LOG(gXULLog, PR_LOG_ERROR,
                   ("xul: error parsing '%s'", urlspec.get()));
        }
#endif

        return NS_OK;
    }

    PRUint32 piInsertionPoint = 0;
    if (mState != eState_Master) {
        piInsertionPoint = IndexOf(GetRootContent());
        NS_ASSERTION(piInsertionPoint >= 0,
                     "No root content when preparing to walk overlay!");
    }

    const nsTArray<nsXULPrototypePI*>& processingInstructions =
        mCurrentPrototype->GetProcessingInstructions();

    PRUint32 total = processingInstructions.Length();
    for (PRUint32 i = 0; i < total; ++i) {
        rv = CreateAndInsertPI(processingInstructions[i],
                               this, piInsertionPoint + i);
        if (NS_FAILED(rv)) return rv;
    }

    // Now check the chrome registry for any additional overlays.
    rv = AddChromeOverlays();
    if (NS_FAILED(rv)) return rv;

    // Do one-time initialization if we're preparing to walk the
    // master document's prototype.
    nsCOMPtr<nsIContent> root;

    if (mState == eState_Master) {
        // Add the root element
        rv = CreateElementFromPrototype(proto, getter_AddRefs(root));
        if (NS_FAILED(rv)) return rv;

        rv = AppendChildTo(root, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
        
        // Add the root element to the XUL document's ID-to-element map.
        rv = AddElementToMap(root);
        if (NS_FAILED(rv)) return rv;

        // Block onload until we've finished building the complete
        // document content model.
        BlockOnload();
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
nsXULDocument::CreateAndInsertPI(const nsXULPrototypePI* aProtoPI,
                                 nsINode* aParent, PRUint32 aIndex)
{
    NS_PRECONDITION(aProtoPI, "null ptr");
    NS_PRECONDITION(aParent, "null ptr");

    nsresult rv;
    nsCOMPtr<nsIContent> node;

    rv = NS_NewXMLProcessingInstruction(getter_AddRefs(node),
                                        mNodeInfoManager,
                                        aProtoPI->mTarget,
                                        aProtoPI->mData);
    if (NS_FAILED(rv)) return rv;

    if (aProtoPI->mTarget.EqualsLiteral("xml-stylesheet")) {
        rv = InsertXMLStylesheetPI(aProtoPI, aParent, aIndex, node);
    } else if (aProtoPI->mTarget.EqualsLiteral("xul-overlay")) {
        rv = InsertXULOverlayPI(aProtoPI, aParent, aIndex, node);
    } else {
        // No special processing, just add the PI to the document.
        rv = aParent->InsertChildAt(node, aIndex, PR_FALSE);
    }

    return rv;
}

nsresult
nsXULDocument::InsertXMLStylesheetPI(const nsXULPrototypePI* aProtoPI,
                                     nsINode* aParent,
                                     PRUint32 aIndex,
                                     nsIContent* aPINode)
{
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aPINode));
    NS_ASSERTION(ssle, "passed XML Stylesheet node does not "
                       "implement nsIStyleSheetLinkingElement!");

    nsresult rv;

    ssle->InitStyleLinkElement(PR_FALSE);
    // We want to be notified when the style sheet finishes loading, so
    // disable style sheet loading for now.
    ssle->SetEnableUpdates(PR_FALSE);
    ssle->OverrideBaseURI(mCurrentPrototype->GetURI());

    rv = aParent->InsertChildAt(aPINode, aIndex, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    ssle->SetEnableUpdates(PR_TRUE);

    // load the stylesheet if necessary, passing ourselves as
    // nsICSSObserver
    PRBool willNotify;
    PRBool isAlternate;
    rv = ssle->UpdateStyleSheet(this, &willNotify, &isAlternate);
    if (NS_SUCCEEDED(rv) && willNotify && !isAlternate) {
        ++mPendingSheets;
    }

    // Ignore errors from UpdateStyleSheet; we don't want failure to
    // do that to break the XUL document load.  But do propagate out
    // NS_ERROR_OUT_OF_MEMORY.
    if (rv == NS_ERROR_OUT_OF_MEMORY) {
        return rv;
    }
    
    return NS_OK;
}

nsresult
nsXULDocument::InsertXULOverlayPI(const nsXULPrototypePI* aProtoPI,
                                  nsINode* aParent,
                                  PRUint32 aIndex,
                                  nsIContent* aPINode)
{
    nsresult rv;

    rv = aParent->InsertChildAt(aPINode, aIndex, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // xul-overlay PI is special only in prolog
    if (!nsContentUtils::InProlog(aPINode)) {
        return NS_OK;
    }

    nsAutoString href;
    nsParserUtils::GetQuotedAttributeValue(aProtoPI->mData,
                                           nsGkAtoms::href,
                                           href);

    // If there was no href, we can't do anything with this PI
    if (href.IsEmpty()) {
        return NS_OK;
    }

    // Add the overlay to our list of overlays that need to be processed.
    nsCOMPtr<nsIURI> uri;

    rv = NS_NewURI(getter_AddRefs(uri), href, nsnull,
                   mCurrentPrototype->GetURI());
    if (NS_SUCCEEDED(rv)) {
        // We insert overlays into mUnloadedOverlays at the same index in
        // document order, so they end up in the reverse of the document
        // order in mUnloadedOverlays.
        // This is needed because the code in ResumeWalk loads the overlays
        // by processing the last item of mUnloadedOverlays and removing it
        // from the array.
        rv = mUnloadedOverlays.InsertObjectAt(uri, 0);
    } else if (rv == NS_ERROR_MALFORMED_URI) {
        // The URL is bad, move along. Don't propagate for now.
        // XXX report this to the Error Console (bug 359846)
        rv = NS_OK;
    }

    return rv;
}

nsresult
nsXULDocument::AddChromeOverlays()
{
    nsresult rv;

    nsCOMPtr<nsIURI> docUri = mCurrentPrototype->GetURI();

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
        if (NS_FAILED(rv) || !next) break;

        uri = do_QueryInterface(next);
        if (!uri) {
            NS_ERROR("Chrome registry handed me a non-nsIURI object!");
            continue;
        }

        // Same comment as in nsXULDocument::InsertXULOverlayPI
        rv = mUnloadedOverlays.InsertObjectAt(uri, 0);
        if (NS_FAILED(rv)) break;
    }

    return rv;
}

NS_IMETHODIMP
nsXULDocument::LoadOverlay(const nsAString& aURL, nsIObserver* aObserver)
{
    nsresult rv;

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), aURL, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (aObserver) {
        nsIObserver* obs = nsnull;
        NS_ENSURE_TRUE(mOverlayLoadObservers.IsInitialized() || mOverlayLoadObservers.Init(), 
                       NS_ERROR_OUT_OF_MEMORY);
        
        obs = mOverlayLoadObservers.GetWeak(uri);

        if (obs) {
            // We don't support loading the same overlay twice into the same
            // document - that doesn't make sense anyway.
            return NS_ERROR_FAILURE;
        }
        mOverlayLoadObservers.Put(uri, aObserver);
    }
    PRBool shouldReturn, failureFromContent;
    rv = LoadOverlayInternal(uri, PR_TRUE, &shouldReturn, &failureFromContent);
    if (NS_FAILED(rv) && mOverlayLoadObservers.IsInitialized())
        mOverlayLoadObservers.Remove(uri); // remove the observer if LoadOverlayInternal generated an error
    return rv;
}

nsresult
nsXULDocument::LoadOverlayInternal(nsIURI* aURI, PRBool aIsDynamic,
                                   PRBool* aShouldReturn,
                                   PRBool* aFailureFromContent)
{
    nsresult rv;

    *aShouldReturn = PR_FALSE;
    *aFailureFromContent = PR_FALSE;

    nsCOMPtr<nsIScriptSecurityManager> secMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsCAutoString urlspec;
        aURI->GetSpec(urlspec);

        PR_LOG(gXULLog, PR_LOG_DEBUG,
                ("xul: loading overlay %s", urlspec.get()));
    }
#endif

    if (aIsDynamic)
        mResolutionPhase = nsForwardReference::eStart;

    // Chrome documents are allowed to load overlays from anywhere.
    // Also, any document may load a chrome:// overlay.
    // In all other cases, the overlay is only allowed to load if
    // the master document and prototype document have the same origin.

    PRBool overlayIsChrome = IsChromeURI(aURI);
    if (!IsChromeURI(mDocumentURI) && !overlayIsChrome) {
        // Make sure we're allowed to load this overlay.
        rv = secMan->CheckSameOriginURI(mDocumentURI, aURI);
        if (NS_FAILED(rv)) {
            *aFailureFromContent = PR_TRUE;
            return rv;
        }
    }

    // Look in the prototype cache for the prototype document with
    // the specified overlay URI.
    mCurrentPrototype = overlayIsChrome ?
        nsXULPrototypeCache::GetInstance()->GetPrototype(aURI) : nsnull;

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

    PRBool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();
    if (aIsDynamic)
        mIsWritingFastLoad = useXULCache;

    if (useXULCache && mCurrentPrototype) {
        PRBool loaded;
        rv = mCurrentPrototype->AwaitLoadDone(this, &loaded);
        if (NS_FAILED(rv)) return rv;

        if (! loaded) {
            // Return to the main event loop and eagerly await the
            // prototype overlay load's completion. When the content
            // sink completes, it will trigger an EndLoad(), which'll
            // wind us back up here, in ResumeWalk().
            *aShouldReturn = PR_TRUE;
            return NS_OK;
        }

        PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was cached"));

        // Found the overlay's prototype in the cache, fully loaded. If
        // this is a dynamic overlay, this will call ResumeWalk.
        // Otherwise, we'll return to ResumeWalk, which called us.
        return OnPrototypeLoadDone(aIsDynamic);
    }
    else {
        // Not there. Initiate a load.
        PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: overlay was not cached"));

        // No one ever uses overlay principals for anything, so it's OK to give
        // them the null principal.  Too bad this code insists on the sort of
        // syncloading that can't provide us the right principal from the
        // channel...
        nsCOMPtr<nsIParser> parser;
        rv = PrepareToLoadPrototype(aURI, "view", nsnull, getter_AddRefs(parser));
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
        parser->Parse(aURI, parserObserver);
        NS_RELEASE(parserObserver);

        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
        rv = NS_OpenURI(listener, nsnull, aURI, nsnull, group);
        if (NS_FAILED(rv)) {
            // Abandon this prototype
            mCurrentPrototype = nsnull;

            // The parser won't get an OnStartRequest and
            // OnStopRequest, so it needs a Terminate.
            parser->Terminate();

            // Just move on to the next overlay.  NS_OpenURI could fail
            // just because a channel could not be opened, which can happen
            // if a file or chrome package does not exist.
            ReportMissingOverlay(aURI);
            
            // XXX the error could indicate an internal error as well...
            *aFailureFromContent = PR_TRUE;
            return rv;
        }

        // If it's a 'chrome:' prototype document, then put it into
        // the prototype cache; other XUL documents will be reloaded
        // each time.  We must do this after NS_OpenURI and AsyncOpen,
        // or chrome code will wrongly create a cached chrome channel
        // instead of a real one.
        if (useXULCache && overlayIsChrome) {
            nsXULPrototypeCache::GetInstance()->PutPrototype(mCurrentPrototype);
        }

        // Return to the main event loop and eagerly await the
        // overlay load's completion. When the content sink
        // completes, it will trigger an EndLoad(), which'll wind
        // us back in ResumeWalk().
        if (!aIsDynamic)
            *aShouldReturn = PR_TRUE;
    }
    return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
FirePendingMergeNotification(nsIURI* aKey, nsCOMPtr<nsIObserver>& aObserver, void* aClosure)
{
    aObserver->Observe(aKey, "xul-overlay-merged", EmptyString().get());

    typedef nsInterfaceHashtable<nsURIHashKey,nsIObserver> table;
    table* observers = NS_STATIC_CAST(table*, aClosure);
    observers->Remove(aKey);

    return PL_DHASH_REMOVE;
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

    while (1) {
        // Begin (or resume) walking the current prototype.

        while (mContextStack.Depth() > 0) {
            // Look at the top of the stack to determine what we're
            // currently working on.
            // This will always be a node already constructed and
            // inserted to the actual document.
            nsXULPrototypeElement* proto;
            nsCOMPtr<nsIContent> element;
            PRInt32 indx; // all children of proto before indx (not
                          // inclusive) have already been constructed
            rv = mContextStack.Peek(&proto, getter_AddRefs(element), &indx);
            if (NS_FAILED(rv)) return rv;

            if (indx >= (PRInt32)proto->mNumChildren) {
                if (element) {
                    // We've processed all of the prototype's children. If
                    // we're in the master prototype, do post-order
                    // document-level hookup. (An overlay will get its
                    // document hookup done when it's successfully
                    // resolved.)
                    if (mState == eState_Master) {
                        AddElementToDocumentPost(element);

                        if (element->NodeInfo()->Equals(nsGkAtoms::style,
                                                        kNameSpaceID_XHTML) ||
                            element->NodeInfo()->Equals(nsGkAtoms::style,
                                                        kNameSpaceID_SVG)) {
                            // XXX sucks that we have to do this -
                            // see bug 370111
                            nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
                                do_QueryInterface(element);
                            NS_ASSERTION(ssle, "<html:style> doesn't implement "
                                               "nsIStyleSheetLinkingElement?");
                            PRBool willNotify;
                            PRBool isAlternate;
                            ssle->UpdateStyleSheet(nsnull, &willNotify,
                                                   &isAlternate);
                        }
                    }

#ifdef MOZ_XTF
                    if (element->GetNameSpaceID() > kNameSpaceID_LastBuiltin) {
                        element->DoneAddingChildren(PR_FALSE);
                    }
#endif
                }
                // Now pop the context stack back up to the parent
                // element and continue the prototype walk.
                mContextStack.Pop();
                continue;
            }

            // Grab the next child, and advance the current context stack
            // to the next sibling to our right.
            nsXULPrototypeNode* childproto = proto->mChildren[indx];
            mContextStack.SetTopIndex(++indx);

            // Whether we're in the "first ply" of an overlay:
            // the "hookup" nodes. In the case !processingOverlayHookupNodes,
            // we're in the master document -or- we're in an overlay, and far
            // enough down into the overlay's content that we can simply build
            // the delegates and attach them to the parent node.
            PRBool processingOverlayHookupNodes = (mState == eState_Overlay) && 
                                                  (mContextStack.Depth() == 1);

            NS_ASSERTION(element || processingOverlayHookupNodes,
                         "no element on context stack");

            switch (childproto->mType) {
            case nsXULPrototypeNode::eType_Element: {
                // An 'element', which may contain more content.
                nsXULPrototypeElement* protoele =
                    NS_STATIC_CAST(nsXULPrototypeElement*, childproto);

                nsCOMPtr<nsIContent> child;

                if (!processingOverlayHookupNodes) {
                    rv = CreateElementFromPrototype(protoele,
                                                    getter_AddRefs(child));
                    if (NS_FAILED(rv)) return rv;

                    // ...and append it to the content model.
                    rv = element->AppendChildTo(child, PR_FALSE);
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
                else {
                    if (mState == eState_Master) {
                        // If there are no children, and we're in the
                        // master document, do post-order document hookup
                        // immediately.
                        AddElementToDocumentPost(child);
                    }
#ifdef MOZ_XTF
                    if (child &&
                        child->GetNameSpaceID() > kNameSpaceID_LastBuiltin) {
                        child->DoneAddingChildren(PR_FALSE);
                    }
#endif
                }
            }
            break;

            case nsXULPrototypeNode::eType_Script: {
                // A script reference. Execute the script immediately;
                // this may have side effects in the content model.
                nsXULPrototypeScript* scriptproto =
                    NS_STATIC_CAST(nsXULPrototypeScript*, childproto);

                if (scriptproto->mSrcURI) {
                    // A transcluded script reference; this may
                    // "block" our prototype walk if the script isn't
                    // cached, or the cached copy of the script is
                    // stale and must be reloaded.
                    PRBool blocked;
                    rv = LoadScript(scriptproto, &blocked);
                    // If the script cannot be loaded, just keep going!

                    if (NS_SUCCEEDED(rv) && blocked)
                        return NS_OK;
                }
                else if (scriptproto->mScriptObject.mObject) {
                    // An inline script
                    rv = ExecuteScript(scriptproto);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;

            case nsXULPrototypeNode::eType_Text: {
                // A simple text node.

                if (!processingOverlayHookupNodes) {
                    // This does mean that text nodes that are direct children
                    // of <overlay> get ignored.

                    nsCOMPtr<nsIContent> text;
                    rv = NS_NewTextNode(getter_AddRefs(text),
                                        mNodeInfoManager);
                    NS_ENSURE_SUCCESS(rv, rv);

                    nsXULPrototypeText* textproto =
                        NS_STATIC_CAST(nsXULPrototypeText*, childproto);
                    text->SetText(textproto->mValue, PR_FALSE);

                    rv = element->AppendChildTo(text, PR_FALSE);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }
            break;

            case nsXULPrototypeNode::eType_PI: {
                nsXULPrototypePI* piProto =
                    NS_STATIC_CAST(nsXULPrototypePI*, childproto);

                // <?xul-overlay?> and <?xml-stylesheet?> don't have effect
                // outside the prolog, like they used to. Issue a warning.

                if (piProto->mTarget.EqualsLiteral("xml-stylesheet") ||
                    piProto->mTarget.EqualsLiteral("xul-overlay")) {

                    const PRUnichar* params[] = { piProto->mTarget.get() };

                    nsCOMPtr<nsIURI> overlayURI = mCurrentPrototype->GetURI();

                    nsContentUtils::ReportToConsole(
                                        nsContentUtils::eXUL_PROPERTIES,
                                        "PINotInProlog",
                                        params, NS_ARRAY_LENGTH(params),
                                        overlayURI,
                                        EmptyString(), /* source line */
                                        0, /* line number */
                                        0, /* column number */
                                        nsIScriptError::warningFlag,
                                        "XUL Document");
                }

                nsIContent* parent = processingOverlayHookupNodes ?
                    GetRootContent() : element.get();

                if (parent) {
                    // an inline script could have removed the root element
                    rv = CreateAndInsertPI(piProto, parent,
                                           parent->GetChildCount());
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }
            break;

            default:
                NS_NOTREACHED("Unexpected nsXULPrototypeNode::Type value");
            }
        }

        // Once we get here, the context stack will have been
        // depleted. That means that the entire prototype has been
        // walked and content has been constructed.

        // If we're not already, mark us as now processing overlays.
        mState = eState_Overlay;

        // If there are no overlay URIs, then we're done.
        PRUint32 count = mUnloadedOverlays.Count();
        if (! count)
            break;

        nsCOMPtr<nsIURI> uri = mUnloadedOverlays[count-1];
        mUnloadedOverlays.RemoveObjectAt(count-1);

        PRBool shouldReturn, failureFromContent;
        rv = LoadOverlayInternal(uri, PR_FALSE, &shouldReturn,
                                 &failureFromContent);
        if (failureFromContent)
            // The failure |rv| was the result of a problem in the content
            // rather than an unexpected problem in our implementation, so
            // just continue with the next overlay.
            continue;
        if (NS_FAILED(rv))
            return rv;
        if (shouldReturn)
            return NS_OK;
    }

    // If we get here, there is nothing left for us to walk. The content
    // model is built and ready for layout.
    rv = ResolveForwardReferences();
    if (NS_FAILED(rv)) return rv;

    rv = ApplyPersistentAttributes();
    if (NS_FAILED(rv)) return rv;

    mStillWalking = PR_FALSE;
    if (mPendingSheets == 0) {
        rv = DoneWalking();
    }
    return rv;
}

nsresult
nsXULDocument::DoneWalking()
{
    NS_PRECONDITION(mPendingSheets == 0, "there are sheets to be loaded");
    NS_PRECONDITION(!mStillWalking, "walk not done");

    // XXXldb This is where we should really be setting the chromehidden
    // attribute.

    PRUint32 count = mOverlaySheets.Count();
    for (PRUint32 i = 0; i < count; ++i) {
        AddStyleSheet(mOverlaySheets[i]);
    }
    mOverlaySheets.Clear();

    if (!mDocumentLoaded) {
        // Make sure we don't reenter here from StartLayout().  Note that
        // setting mDocumentLoaded to true here means that if StartLayout()
        // causes ResumeWalk() to be reentered, we'll take the other branch of
        // the |if (!mDocumentLoaded)| check above and since
        // mInitialLayoutComplete will be false will follow the else branch
        // there too.  See the big comment there for how such reentry can
        // happen.
        mDocumentLoaded = PR_TRUE;

        nsAutoString title;
        if (mRootContent) {
            mRootContent->GetAttr(kNameSpaceID_None, nsGkAtoms::title,
                                  title);
        }
        SetTitle(title);

        // Before starting layout, check whether we're a toplevel chrome
        // window.  If we are, set our chrome flags now, so that we don't have
        // to restyle the whole frame tree after StartLayout.
        nsCOMPtr<nsISupports> container = GetContainer();
        nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(container);
        if (item) {
            nsCOMPtr<nsIDocShellTreeOwner> owner;
            item->GetTreeOwner(getter_AddRefs(owner));
            nsCOMPtr<nsIXULWindow> xulWin = do_GetInterface(owner);
            if (xulWin) {
                nsCOMPtr<nsIDocShell> xulWinShell;
                xulWin->GetDocShell(getter_AddRefs(xulWinShell));
                if (SameCOMIdentity(xulWinShell, container)) {
                    // We're the chrome document!  Apply our chrome flags now.
                    xulWin->ApplyChromeFlags();
                }
            }
        }

        StartLayout();

        if (mIsWritingFastLoad && IsChromeURI(mDocumentURI))
            nsXULPrototypeCache::GetInstance()->WritePrototype(mMasterPrototype);

        NS_DOCUMENT_NOTIFY_OBSERVERS(EndLoad, (this));

        DispatchContentLoadedEvents();

        // Undo the onload-blocking we did in PrepareToWalk().  Note that we do
        // the unblocking sync because that's what the old code did.
        UnblockOnload(PR_TRUE);
        
        mInitialLayoutComplete = PR_TRUE;

        // Walk the set of pending load notifications and notify any observers.
        // See below for detail.
        if (mPendingOverlayLoadNotifications.IsInitialized())
            mPendingOverlayLoadNotifications.Enumerate(FirePendingMergeNotification, (void*)&mOverlayLoadObservers);
    }
    else {
        if (mOverlayLoadObservers.IsInitialized()) {
            nsCOMPtr<nsIURI> overlayURI = mCurrentPrototype->GetURI();
            nsCOMPtr<nsIObserver> obs;
            if (mInitialLayoutComplete) {
                // We have completed initial layout, so just send the notification.
                mOverlayLoadObservers.Get(overlayURI, getter_AddRefs(obs));
                if (obs)
                    obs->Observe(overlayURI, "xul-overlay-merged", EmptyString().get());
                mOverlayLoadObservers.Remove(overlayURI);
            }
            else {
                // If we have not yet displayed the document for the first time 
                // (i.e. we came in here as the result of a dynamic overlay load
                // which was spawned by a binding-attached event caused by 
                // StartLayout() on the master prototype - we must remember that
                // this overlay has been merged and tell the listeners after 
                // StartLayout() is completely finished rather than doing so 
                // immediately - otherwise we may be executing code that needs to
                // access XBL Binding implementations on nodes for which frames 
                // have not yet been constructed because their bindings have not
                // yet been attached. This can be a race condition because dynamic
                // overlay loading can take varying amounts of time depending on
                // whether or not the overlay prototype is in the XUL cache. The
                // most likely effect of this bug is odd UI initialization due to
                // methods and properties that do not work.
                // XXXbz really, we shouldn't be firing binding constructors
                // until after StartLayout returns!

                NS_ENSURE_TRUE(mPendingOverlayLoadNotifications.IsInitialized() || mPendingOverlayLoadNotifications.Init(), 
                               NS_ERROR_OUT_OF_MEMORY);
                
                mPendingOverlayLoadNotifications.Get(overlayURI, getter_AddRefs(obs));
                if (!obs) {
                    mOverlayLoadObservers.Get(overlayURI, getter_AddRefs(obs));
                    NS_ASSERTION(obs, "null overlay load observer?");
                    mPendingOverlayLoadNotifications.Put(overlayURI, obs);
                }
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULDocument::StyleSheetLoaded(nsICSSStyleSheet* aSheet,
                                PRBool aWasAlternate,
                                nsresult aStatus)
{
    if (!aWasAlternate) {
        // Don't care about when alternate sheets finish loading

        NS_ASSERTION(mPendingSheets > 0,
            "Unexpected StyleSheetLoaded notification");

        --mPendingSheets;

        if (!mStillWalking && mPendingSheets == 0) {
            return DoneWalking();
        }
    }

    return NS_OK;
}

void
nsXULDocument::ReportMissingOverlay(nsIURI* aURI)
{
    NS_PRECONDITION(aURI, "Must have a URI");
    
    nsCAutoString spec;
    aURI->GetSpec(spec);

    NS_ConvertUTF8toUTF16 utfSpec(spec);
    const PRUnichar* params[] = { utfSpec.get() };

    nsContentUtils::ReportToConsole(nsContentUtils::eXUL_PROPERTIES,
                                    "MissingOverlay",
                                    params, NS_ARRAY_LENGTH(params),
                                    mDocumentURI,
                                    EmptyString(), /* source line */
                                    0, /* line number */
                                    0, /* column number */
                                    nsIScriptError::warningFlag,
                                    "XUL Document");
}

nsresult
nsXULDocument::LoadScript(nsXULPrototypeScript* aScriptProto, PRBool* aBlock)
{
    // Load a transcluded script
    nsresult rv;

    if (aScriptProto->mScriptObject.mObject) {
        rv = ExecuteScript(aScriptProto);

        // Ignore return value from execution, and don't block
        *aBlock = PR_FALSE;
        return NS_OK;
    }

    // Try the XUL script cache, in case two XUL documents source the same
    // .js file (e.g., strres.js from navigator.xul and utilityOverlay.xul).
    // XXXbe the cache relies on aScriptProto's GC root!
    PRBool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();

    if (useXULCache) {
        PRUint32 fetchedLang = nsIProgrammingLanguage::UNKNOWN;
        void *newScriptObject =
            nsXULPrototypeCache::GetInstance()->GetScript(
                                   aScriptProto->mSrcURI,
                                   &fetchedLang);
        if (newScriptObject) {
            // The script language for a proto must remain constant - we
            // can't just change it for this unexpected language.
            if (aScriptProto->mScriptObject.mLangID != fetchedLang) {
                NS_ERROR("XUL cache gave me an incorrect script language");
                return NS_ERROR_UNEXPECTED;
            }
            aScriptProto->mScriptObject.set(newScriptObject);
        }

        if (aScriptProto->mScriptObject.mObject) {
            rv = ExecuteScript(aScriptProto);

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
        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

        // Note: the loader will keep itself alive while it's loading.
        nsCOMPtr<nsIStreamLoader> loader;
        rv = NS_NewStreamLoader(getter_AddRefs(loader), aScriptProto->mSrcURI,
                                this, nsnull, group);
        if (NS_FAILED(rv)) {
            mCurrentScriptProto = nsnull;
            return rv;
        }

        aScriptProto->mSrcLoading = PR_TRUE;
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
    nsCOMPtr<nsIRequest> request;
    aLoader->GetRequest(getter_AddRefs(request));
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);

#ifdef DEBUG
    // print a load error on bad status
    if (NS_FAILED(aStatus)) {
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
        // script->mScriptObject, causing control flow at the top of LoadScript
        // not to reach here.
        nsCOMPtr<nsIURI> uri = scriptProto->mSrcURI;

        // XXX should also check nsIHttpChannel::requestSucceeded

        nsString stringStr;
        rv = nsScriptLoader::ConvertToUTF16(channel, string, stringLen,
                                            EmptyString(), this, stringStr);
        if (NS_SUCCEEDED(rv))
          rv = scriptProto->Compile(stringStr.get(), stringStr.Length(), uri,
                                    1, this, mCurrentPrototype);

        aStatus = rv;
        if (NS_SUCCEEDED(rv)) {
            rv = ExecuteScript(scriptProto);

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
            PRBool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();
  
            if (useXULCache && IsChromeURI(mDocumentURI)) {
                nsXULPrototypeCache::GetInstance()->PutScript(
                                   scriptProto->mSrcURI,
                                   scriptProto->mScriptObject.mLangID,
                                   scriptProto->mScriptObject.mObject);
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
                nsIScriptGlobalObject* global =
                    mCurrentPrototype->GetScriptGlobalObject();

                NS_ASSERTION(global != nsnull, "master prototype w/o global?!");
                if (global) {
                    PRUint32 stid = scriptProto->mScriptObject.mLangID;
                    nsIScriptContext *scriptContext = \
                          global->GetScriptContext(stid);
                    NS_ASSERTION(scriptContext != nsnull,
                                 "Failed to get script context for language");
                    if (scriptContext)
                        scriptProto->SerializeOutOfLine(nsnull, global);
                }
            }
        }
        // ignore any evaluation errors
    }

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
        if (NS_SUCCEEDED(aStatus) && scriptProto->mScriptObject.mObject) {
            doc->ExecuteScript(scriptProto);
        }
        doc->ResumeWalk();
        NS_RELEASE(doc);
    }

    return rv;
}


nsresult
nsXULDocument::ExecuteScript(nsIScriptContext * aContext, void * aScriptObject)
{
    NS_PRECONDITION(aScriptObject != nsnull && aContext != nsnull, "null ptr");
    if (! aScriptObject || ! aContext)
        return NS_ERROR_NULL_POINTER;

    NS_ENSURE_TRUE(mScriptGlobalObject, NS_ERROR_NOT_INITIALIZED);

    // Execute the precompiled script with the given version
    nsresult rv;
    void *global = mScriptGlobalObject->GetScriptGlobal(
                                            aContext->GetScriptTypeID());
    rv = aContext->ExecuteScript(aScriptObject,
                                 global,
                                 nsnull, nsnull);

    return rv;
}

nsresult
nsXULDocument::ExecuteScript(nsXULPrototypeScript *aScript)
{
    NS_PRECONDITION(aScript != nsnull, "null ptr");
    NS_ENSURE_TRUE(aScript, NS_ERROR_NULL_POINTER);
    NS_ENSURE_TRUE(mScriptGlobalObject, NS_ERROR_NOT_INITIALIZED);
    PRUint32 stid = aScript->mScriptObject.mLangID;

    nsresult rv;
    rv = mScriptGlobalObject->EnsureScriptEnvironment(stid);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptContext> context =
      mScriptGlobalObject->GetScriptContext(stid);
    // failure getting a script context is fatal.
    NS_ENSURE_TRUE(context != nsnull, NS_ERROR_UNEXPECTED);

    if (aScript->mScriptObject.mObject)
        rv = ExecuteScript(context, aScript->mScriptObject.mObject);
    else
        rv = NS_ERROR_UNEXPECTED;
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
        // into the element.  Get a nodeinfo from our nodeinfo manager
        // for this node.
        nsCOMPtr<nsINodeInfo> newNodeInfo;
        rv = mNodeInfoManager->GetNodeInfo(aPrototype->mNodeInfo->NameAtom(),
                                           aPrototype->mNodeInfo->GetPrefixAtom(),
                                           aPrototype->mNodeInfo->NamespaceID(),
                                           getter_AddRefs(newNodeInfo));
        if (NS_FAILED(rv)) return rv;
        rv = NS_NewElement(getter_AddRefs(result), newNodeInfo->NamespaceID(),
                           newNodeInfo);
        if (NS_FAILED(rv)) return rv;

#ifdef MOZ_XTF
        if (result && newNodeInfo->NamespaceID() > kNameSpaceID_LastBuiltin) {
            result->BeginAddingChildren();
        }
#endif

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;
    }

    result.swap(*aResult);

    return NS_OK;
}

nsresult
nsXULDocument::CreateOverlayElement(nsXULPrototypeElement* aPrototype,
                                    nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> element;
    rv = CreateElementFromPrototype(aPrototype, getter_AddRefs(element));
    if (NS_FAILED(rv)) return rv;

    OverlayForwardReference* fwdref =
        new OverlayForwardReference(this, element);
    if (! fwdref)
        return NS_ERROR_OUT_OF_MEMORY;

    // transferring ownership to ya...
    rv = AddForwardReference(fwdref);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*aResult = element);
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
                                      nsGkAtoms::datasources);
    return NS_OK;
}

/* static */ nsresult
nsXULDocument::CreateTemplateBuilder(nsIContent* aElement)
{
    // Check if need to construct a tree builder or content builder.
    PRBool isTreeBuilder = PR_FALSE;

    nsIDocument *document = aElement->GetOwnerDoc();
    NS_ASSERTION(document, "no document");
    NS_ENSURE_TRUE(document, NS_ERROR_UNEXPECTED);

    PRInt32 nameSpaceID;
    nsIAtom* baseTag = document->BindingManager()->
      ResolveTag(aElement, &nameSpaceID);

    if ((nameSpaceID == kNameSpaceID_XUL) && (baseTag == nsGkAtoms::tree)) {
        // By default, we build content for a tree and then we attach
        // the tree content view. However, if the `dont-build-content'
        // flag is set, then we we'll attach a tree builder which
        // directly implements the tree view.

        nsAutoString flags;
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::flags, flags);
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
                                          nsGkAtoms::treechildren,
                                          getter_AddRefs(bodyContent));

        if (! bodyContent) {
            nsresult rv = document->CreateElem(nsGkAtoms::treechildren,
                                               nsnull, kNameSpaceID_XUL,
                                               PR_FALSE,
                                               getter_AddRefs(bodyContent));
            NS_ENSURE_SUCCESS(rv, rv);

            aElement->AppendChildTo(bodyContent, PR_FALSE);
        }
    }
    else {
        // Create and initialize a content builder.
        nsCOMPtr<nsIXULTemplateBuilder> builder
            = do_CreateInstance("@mozilla.org/xul/xul-template-builder;1");

        if (! builder)
            return NS_ERROR_FAILURE;

        builder->Init(aElement);

        nsXULElement *xulContent = nsXULElement::FromContent(aElement);
        if (xulContent) {
            // Mark the XUL element as being lazy, so the template builder
            // will run when layout first asks for these nodes.
            xulContent->SetLazyState(nsXULElement::eChildrenMustBeRebuilt);
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
    nsresult rv;

    const nsCOMArray<nsIURI>& sheets = mCurrentPrototype->GetStyleSheetReferences();

    for (PRInt32 i = 0; i < sheets.Count(); i++) {
        nsCOMPtr<nsIURI> uri = sheets[i];

        nsCOMPtr<nsICSSStyleSheet> incompleteSheet;
        rv = CSSLoader()->LoadSheet(uri, this, getter_AddRefs(incompleteSheet));

        // XXXldb We need to prevent bogus sheets from being held in the
        // prototype's list, but until then, don't propagate the failure
        // from LoadSheet (and thus exit the loop).
        if (NS_SUCCEEDED(rv)) {
            ++mPendingSheets;
            if (!mOverlaySheets.AppendObject(incompleteSheet)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
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
    nsCOMPtr<nsIContent> target;

    PRBool notify = PR_FALSE;
    nsIPresShell *shell = mDocument->GetShellAt(0);
    if (shell)
        shell->GetDidInitialReflow(&notify);

    nsAutoString id;
    mOverlay->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
    if (id.IsEmpty()) {
        // mOverlay is a direct child of <overlay> and has no id.
        // Insert it under the root element in the base document.
        if (!mDocument->mRootContent) {
            return eResolve_Error;
        }

        rv = mDocument->InsertElement(mDocument->mRootContent, mOverlay, notify);
        if (NS_FAILED(rv)) return eResolve_Error;

        target = mOverlay;
    }
    else {
        // The hook-up element has an id, try to match it with an element
        // with the same id in the base document.
        nsCOMPtr<nsIDOMElement> domtarget;
        rv = mDocument->GetElementById(id, getter_AddRefs(domtarget));
        if (NS_FAILED(rv)) return eResolve_Error;

        // If we can't find the element in the document, defer the hookup
        // until later.
        target = do_QueryInterface(domtarget);
        NS_ASSERTION(!domtarget || target, "not an nsIContent");
        if (!target)
            return eResolve_Later;

        // While merging, set the default script language of the element to be
        // the language from the overlay - attributes will then be correctly
        // hooked up with the appropriate language (while child nodes ignore
        // the default language - they have it in their proto.
        PRUint32 oldDefLang = target->GetScriptTypeID();
        target->SetScriptTypeID(mOverlay->GetScriptTypeID());
        rv = Merge(target, mOverlay, notify);
        target->SetScriptTypeID(oldDefLang);
        if (NS_FAILED(rv)) return eResolve_Error;
    }

    if (!notify) {
        // Add child and any descendants to the element map
        rv = mDocument->AddSubtreeToDocument(target);
        if (NS_FAILED(rv)) return eResolve_Error;
    }

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
                                              nsIContent* aOverlayNode, 
                                              PRBool aNotify)
{
    // This function is given:
    // aTargetNode:  the node in the document whose 'id' attribute
    //               matches a toplevel node in our overlay.
    // aOverlayNode: the node in the overlay document that matches
    //               a node in the actual document.
    // aNotify:      whether or not content manipulation methods should
    //               use the aNotify parameter. After the initial 
    //               reflow (i.e. in the dynamic overlay merge case),
    //               we want all the content manipulation methods we
    //               call to notify so that frames are constructed 
    //               etc. Otherwise do not, since that's during initial
    //               document construction before StartLayout has been
    //               called which will do everything for us.
    //
    // This function merges the tree from the overlay into the tree in
    // the document, overwriting attributes and appending child content
    // nodes appropriately. (See XUL overlay reference for details)

    nsresult rv;

    // Merge attributes from the overlay content node to that of the
    // actual document.
    PRUint32 i;
    const nsAttrName* name;
    for (i = 0; (name = aOverlayNode->GetAttrNameAt(i)); ++i) {
        // We don't want to swap IDs, they should be the same.
        if (name->Equals(nsGkAtoms::id))
            continue;

        PRInt32 nameSpaceID = name->NamespaceID();
        nsIAtom* attr = name->LocalName();
        nsIAtom* prefix = name->GetPrefix();

        nsAutoString value;
        aOverlayNode->GetAttr(nameSpaceID, attr, value);

        // Element in the overlay has the 'removeelement' attribute set
        // so remove it from the actual document.
        if (attr == nsGkAtoms::removeelement &&
            value.EqualsLiteral("true")) {

            rv = RemoveElement(aTargetNode->GetParent(), aTargetNode);
            if (NS_FAILED(rv)) return rv;

            return NS_OK;
        }

        rv = aTargetNode->SetAttr(nameSpaceID, attr, prefix, value, aNotify);
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
        currContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);

        nsCOMPtr<nsIDOMElement> nodeInDocument;
        if (!id.IsEmpty()) {
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

            nsCOMPtr<nsIDOMNode> nodeParent;
            rv = nodeInDocument->GetParentNode(getter_AddRefs(nodeParent));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIDOMElement> elementParent(do_QueryInterface(nodeParent));

            nsAutoString parentID;
            elementParent->GetAttribute(NS_LITERAL_STRING("id"), parentID);
            if (aTargetNode->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
                                         parentID, eCaseMatters)) {
                // The element matches. "Go Deep!"
                nsCOMPtr<nsIContent> childDocumentContent(do_QueryInterface(nodeInDocument));
                rv = Merge(childDocumentContent, currContent, aNotify);
                if (NS_FAILED(rv)) return rv;
                rv = aOverlayNode->RemoveChildAt(0, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                continue;
            }
        }

        rv = aOverlayNode->RemoveChildAt(0, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        rv = InsertElement(aTargetNode, currContent, aNotify);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}



nsXULDocument::OverlayForwardReference::~OverlayForwardReference()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING) && !mResolved) {
        nsAutoString id;
        mOverlay->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);

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
    rv = mDocument->CheckBroadcasterHookup(mObservesElement, &listener, &mResolved);
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

        if (tag == nsGkAtoms::observes) {
            mObservesElement->GetAttr(kNameSpaceID_None, nsGkAtoms::element, broadcasterID);
            mObservesElement->GetAttr(kNameSpaceID_None, nsGkAtoms::attribute, attribute);
        }
        else {
            mObservesElement->GetAttr(kNameSpaceID_None, nsGkAtoms::observes, broadcasterID);
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
nsXULDocument::FindBroadcaster(nsIContent* aElement,
                               nsIDOMElement** aListener,
                               nsString& aBroadcasterID,
                               nsString& aAttribute,
                               nsIDOMElement** aBroadcaster)
{
    NS_ASSERTION(aElement->IsNodeOfType(nsINode::eELEMENT),
                 "Only pass elements into FindBroadcaster!");

    nsresult rv;
    nsINodeInfo *ni = aElement->NodeInfo();
    *aListener = nsnull;
    *aBroadcaster = nsnull;

    if (ni->Equals(nsGkAtoms::observes, kNameSpaceID_XUL)) {
        // It's an <observes> element, which means that the actual
        // listener is the _parent_ node. This element should have an
        // 'element' attribute that specifies the ID of the
        // broadcaster element, and an 'attribute' element, which
        // specifies the name of the attribute to observe.
        nsIContent* parent = aElement->GetParent();

        // If we're still parented by an 'overlay' tag, then we haven't
        // made it into the real document yet. Defer hookup.
        if (parent->NodeInfo()->Equals(nsGkAtoms::overlay,
                                       kNameSpaceID_XUL)) {
            return NS_FINDBROADCASTER_AWAIT_OVERLAYS;
        }

        if (NS_FAILED(CallQueryInterface(parent, aListener)))
            *aListener = nsnull;

        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::element, aBroadcasterID);
        if (aBroadcasterID.IsEmpty()) {
            return NS_FINDBROADCASTER_NOT_FOUND;
        }
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::attribute, aAttribute);
    }
    else {
        // It's a generic element, which means that we'll use the
        // value of the 'observes' attribute to determine the ID of
        // the broadcaster element, and we'll watch _all_ of its
        // values.
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::observes, aBroadcasterID);

        // Bail if there's no aBroadcasterID
        if (aBroadcasterID.IsEmpty()) {
            // Try the command attribute next.
            aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::command, aBroadcasterID);
            if (!aBroadcasterID.IsEmpty()) {
                // We've got something in the command attribute.  We
                // only treat this as a normal broadcaster if we are
                // not a menuitem or a key.

                if (ni->Equals(nsGkAtoms::menuitem, kNameSpaceID_XUL) ||
                    ni->Equals(nsGkAtoms::key, kNameSpaceID_XUL)) {
                return NS_FINDBROADCASTER_NOT_FOUND;
              }
            }
            else {
              return NS_FINDBROADCASTER_NOT_FOUND;
            }
        }

        if (NS_FAILED(CallQueryInterface(aElement, aListener)))
            *aListener = nsnull;

        aAttribute.AssignLiteral("*");
    }

    // Make sure we got a valid listener.
    NS_ENSURE_TRUE(*aListener, NS_ERROR_UNEXPECTED);

    // Try to find the broadcaster element in the document.
    rv = GetElementById(aBroadcasterID, aBroadcaster);
    if (NS_FAILED(rv)) return rv;

    // If we can't find the broadcaster, then we'll need to defer the
    // hookup. We may need to resolve some of the other overlays
    // first.
    if (! *aBroadcaster) {
        return NS_FINDBROADCASTER_AWAIT_OVERLAYS;
    }

    return NS_FINDBROADCASTER_FOUND;
}

nsresult
nsXULDocument::CheckBroadcasterHookup(nsIContent* aElement,
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
    nsCOMPtr<nsIDOMElement> broadcaster;

    rv = FindBroadcaster(aElement, getter_AddRefs(listener),
                         broadcasterID, attribute, getter_AddRefs(broadcaster));
    switch (rv) {
        case NS_FINDBROADCASTER_NOT_FOUND:
            *aNeedsHookup = PR_FALSE;
            return NS_OK;
        case NS_FINDBROADCASTER_AWAIT_OVERLAYS:
            *aNeedsHookup = PR_TRUE;
            return NS_OK;
        case NS_FINDBROADCASTER_FOUND:
            break;
        default:
            return rv;
    }

    rv = AddBroadcastListenerFor(broadcaster, listener, attribute);
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
nsXULDocument::InsertElement(nsIContent* aParent, nsIContent* aChild, PRBool aNotify)
{
    // Insert aChild appropriately into aParent, accounting for a
    // 'pos' attribute set on aChild.
    nsresult rv;

    nsAutoString posStr;
    PRBool wasInserted = PR_FALSE;

    // insert after an element of a given id
    aChild->GetAttr(kNameSpaceID_None, nsGkAtoms::insertafter, posStr);
    PRBool isInsertAfter = PR_TRUE;

    if (posStr.IsEmpty()) {
        aChild->GetAttr(kNameSpaceID_None, nsGkAtoms::insertbefore, posStr);
        isInsertAfter = PR_FALSE;
    }

    if (!posStr.IsEmpty()) {
        nsCOMPtr<nsIDOMDocument> domDocument(
               do_QueryInterface(aParent->GetDocument()));
        nsCOMPtr<nsIDOMElement> domElement;

        char* str = ToNewCString(posStr);
        char* rest;
        char* token = nsCRT::strtok(str, ", ", &rest);

        while (token) {
            rv = domDocument->GetElementById(NS_ConvertASCIItoUTF16(token),
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
                rv = aParent->InsertChildAt(aChild, pos, aNotify);
                if (NS_FAILED(rv))
                    return rv;

                wasInserted = PR_TRUE;
            }
        }
    }

    if (!wasInserted) {

        aChild->GetAttr(kNameSpaceID_None, nsGkAtoms::position, posStr);
        if (!posStr.IsEmpty()) {
            // Positions are one-indexed.
            PRInt32 pos = posStr.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv));
            // Note: if the insertion index (which is |pos - 1|) would be less
            // than 0 or greater than the number of children aParent has, then
            // don't insert, since the position is bogus.  Just skip on to
            // appending.
            if (NS_SUCCEEDED(rv) && pos > 0 &&
                PRUint32(pos - 1) <= aParent->GetChildCount()) {
                rv = aParent->InsertChildAt(aChild, pos - 1, aNotify);
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
        rv = aParent->AppendChildTo(aChild, aNotify);
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

    return mDocument->OnPrototypeLoadDone(PR_TRUE);
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
        nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
        if (aChannel) {
            nsCOMPtr<nsIURI> uri;
            aChannel->GetOriginalURI(getter_AddRefs(uri));
            if (uri) {
                mDocument->ReportMissingOverlay(uri);
            }
        }

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
