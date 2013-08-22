/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

*/

#include "mozilla/Util.h"

// Note the ALPHABETICAL ORDERING
#include "XULDocument.h"

#include "nsError.h"
#include "nsIBoxObject.h"
#include "nsIChromeRegistry.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIContentViewer.h"
#include "nsGUIEvent.h"
#include "nsIDOMXULElement.h"
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
#include "nsParserCIID.h"
#include "nsPIBoxObject.h"
#include "nsRDFCID.h"
#include "nsILocalStore.h"
#include "nsXPIDLString.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsXULCommandDispatcher.h"
#include "nsXULElement.h"
#include "prlog.h"
#include "rdf.h"
#include "nsIFrame.h"
#include "nsXBLService.h"
#include "nsCExternalHandlerService.h"
#include "nsMimeTypes.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsContentList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsIParser.h"
#include "nsCharsetSource.h"
#include "nsIParserService.h"
#include "nsCSSStyleSheet.h"
#include "mozilla/css/Loader.h"
#include "nsIScriptError.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsEventDispatcher.h"
#include "nsIObserverService.h"
#include "nsNodeUtils.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIXULWindow.h"
#include "nsXULPopupManager.h"
#include "nsCCUncollectableMarker.h"
#include "nsURILoader.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/XULDocumentBinding.h"
#include "mozilla/Preferences.h"
#include "nsTextNode.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
//
// CIDs
//

static NS_DEFINE_CID(kParserCID,                 NS_PARSER_CID);

static bool IsChromeURI(nsIURI* aURI)
{
    bool isChrome = false;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
        return true;
    return false;
}

static bool IsOverlayAllowed(nsIURI* aURI)
{
    bool canOverlay = false;
    if (NS_SUCCEEDED(aURI->SchemeIs("about", &canOverlay)) && canOverlay)
        return true;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &canOverlay)) && canOverlay)
        return true;
    return false;
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

const uint32_t kMaxAttrNameLength = 512;
const uint32_t kMaxAttributeLength = 4096;

//----------------------------------------------------------------------
//
// Statics
//

int32_t XULDocument::gRefCnt = 0;

nsIRDFService* XULDocument::gRDFService;
nsIRDFResource* XULDocument::kNC_persist;
nsIRDFResource* XULDocument::kNC_attribute;
nsIRDFResource* XULDocument::kNC_value;

PRLogModuleInfo* XULDocument::gXULLog;

//----------------------------------------------------------------------

struct BroadcasterMapEntry : public PLDHashEntryHdr {
    Element*         mBroadcaster; // [WEAK]
    nsSmallVoidArray mListeners;   // [OWNING] of BroadcastListener objects
};

struct BroadcastListener {
    nsWeakPtr mListener;
    nsCOMPtr<nsIAtom> mAttribute;
};

Element*
nsRefMapEntry::GetFirstElement()
{
    return static_cast<Element*>(mRefContentList.SafeElementAt(0));
}

void
nsRefMapEntry::AppendAll(nsCOMArray<nsIContent>* aElements)
{
    for (int32_t i = 0; i < mRefContentList.Count(); ++i) {
        aElements->AppendObject(static_cast<nsIContent*>(mRefContentList[i]));
    }
}

bool
nsRefMapEntry::AddElement(Element* aElement)
{
    if (mRefContentList.IndexOf(aElement) >= 0)
        return true;
    return mRefContentList.AppendElement(aElement);
}

bool
nsRefMapEntry::RemoveElement(Element* aElement)
{
    mRefContentList.RemoveElement(aElement);
    return mRefContentList.Count() == 0;
}

//----------------------------------------------------------------------
//
// ctors & dtors
//

namespace mozilla {
namespace dom {

XULDocument::XULDocument(void)
    : XMLDocument("application/vnd.mozilla.xul+xml"),
      mDocLWTheme(Doc_Theme_Uninitialized),
      mState(eState_Master),
      mResolutionPhase(nsForwardReference::eStart)
{
    // NOTE! nsDocument::operator new() zeroes out all members, so don't
    // bother initializing members to 0.

    // Override the default in nsDocument
    mCharacterSet.AssignLiteral("UTF-8");

    mDefaultElementType = kNameSpaceID_XUL;
    mIsXUL = true;

    mDelayFrameLoaderInitialization = true;

    mAllowXULXBL = eTriTrue;
}

XULDocument::~XULDocument()
{
    NS_ASSERTION(mNextSrcLoadWaiter == nullptr,
        "unreferenced document still waiting for script source to load?");

    // In case we failed somewhere early on and the forward observer
    // decls never got resolved.
    mForwardReferences.Clear();

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

    Preferences::UnregisterCallback(XULDocument::DirectionChanged,
                                    "intl.uidirection.", this);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);

        NS_IF_RELEASE(kNC_persist);
        NS_IF_RELEASE(kNC_attribute);
        NS_IF_RELEASE(kNC_value);
    }
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewXULDocument(nsIXULDocument** result)
{
    NS_PRECONDITION(result != nullptr, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    XULDocument* doc = new XULDocument();
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


namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
//
// nsISupports interface
//

static PLDHashOperator
TraverseTemplateBuilders(nsISupports* aKey, nsIXULTemplateBuilder* aData,
                         void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        static_cast<nsCycleCollectionTraversalCallback*>(aContext);

    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mTemplateBuilderTable key");
    cb->NoteXPCOMChild(aKey);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mTemplateBuilderTable value");
    cb->NoteXPCOMChild(aData);

    return PL_DHASH_NEXT;
}

static PLDHashOperator
TraverseObservers(nsIURI* aKey, nsIObserver* aData, void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        static_cast<nsCycleCollectionTraversalCallback*>(aContext);

    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mOverlayLoadObservers/mPendingOverlayLoadNotifications value");
    cb->NoteXPCOMChild(aData);

    return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(XULDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XULDocument, XMLDocument)
    NS_ASSERTION(!nsCCUncollectableMarker::InGeneration(cb, tmp->GetMarkedCCGeneration()),
                 "Shouldn't traverse XULDocument!");
    // XXX tmp->mForwardReferences?
    // XXX tmp->mContextStack?

    // An element will only have a template builder as long as it's in the
    // document, so we'll traverse the table here instead of from the element.
    if (tmp->mTemplateBuilderTable)
        tmp->mTemplateBuilderTable->EnumerateRead(TraverseTemplateBuilders, &cb);

    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentPrototype)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMasterPrototype)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCommandDispatcher)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrototypes);
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalStore)

    if (tmp->mOverlayLoadObservers.IsInitialized())
        tmp->mOverlayLoadObservers.EnumerateRead(TraverseObservers, &cb);
    if (tmp->mPendingOverlayLoadNotifications.IsInitialized())
        tmp->mPendingOverlayLoadNotifications.EnumerateRead(TraverseObservers, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XULDocument, XMLDocument)
    delete tmp->mTemplateBuilderTable;
    tmp->mTemplateBuilderTable = nullptr;

    NS_IMPL_CYCLE_COLLECTION_UNLINK(mCommandDispatcher)
    //XXX We should probably unlink all the objects we traverse.
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(XULDocument, XMLDocument)
NS_IMPL_RELEASE_INHERITED(XULDocument, XMLDocument)


// QueryInterface implementation for XULDocument
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(XULDocument)
    NS_INTERFACE_TABLE_INHERITED5(XULDocument, nsIXULDocument,
                                  nsIDOMXULDocument, nsIStreamLoaderObserver,
                                  nsICSSLoaderObserver, nsIOffThreadScriptReceiver)
NS_INTERFACE_TABLE_TAIL_INHERITING(XMLDocument)


//----------------------------------------------------------------------
//
// nsIDocument interface
//

void
XULDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
    NS_NOTREACHED("Reset");
}

void
XULDocument::ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                        nsIPrincipal* aPrincipal)
{
    NS_NOTREACHED("ResetToURI");
}

void
XULDocument::SetContentType(const nsAString& aContentType)
{
    NS_ASSERTION(aContentType.EqualsLiteral("application/vnd.mozilla.xul+xml"),
                 "xul-documents always has content-type application/vnd.mozilla.xul+xml");
    // Don't do anything, xul always has the mimetype
    // application/vnd.mozilla.xul+xml
}

// This is called when the master document begins loading, whether it's
// being cached or not.
nsresult
XULDocument::StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               bool aReset, nsIContentSink* aSink)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING)) {

        nsCOMPtr<nsIURI> uri;
        nsresult rv = aChannel->GetOriginalURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
            nsAutoCString urlspec;
            rv = uri->GetSpec(urlspec);
            if (NS_SUCCEEDED(rv)) {
                PR_LOG(gXULLog, PR_LOG_WARNING,
                       ("xul: load document '%s'", urlspec.get()));
            }
        }
    }
#endif
    // NOTE: If this ever starts calling nsDocument::StartDocumentLoad
    // we'll possibly need to reset our content type afterwards.
    mStillWalking = true;
    mMayStartLayout = false;
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);

    mChannel = aChannel;

    mHaveInputEncoding = true;

    // Get the URI.  Note that this should match nsDocShell::OnLoadingSite
    nsresult rv =
        NS_GetFinalChannelURI(aChannel, getter_AddRefs(mDocumentURI));
    NS_ENSURE_SUCCESS(rv, rv);
    
    ResetStylesheetsToURI(mDocumentURI);

    RetrieveRelevantHeaders(aChannel);

    // Look in the chrome cache: we've got this puppy loaded
    // already.
    nsXULPrototypeDocument* proto = IsChromeURI(mDocumentURI) ?
            nsXULPrototypeCache::GetInstance()->GetPrototype(mDocumentURI) :
            nullptr;

    // Same comment as nsChromeProtocolHandler::NewChannel and
    // XULDocument::ResumeWalk
    // - Ben Goodger
    //
    // We don't abort on failure here because there are too many valid
    // cases that can return failure, and the null-ness of |proto| is enough
    // to trigger the fail-safe parse-from-disk solution. Example failure cases
    // (for reference) include:
    //
    // NS_ERROR_NOT_AVAILABLE: the URI cannot be found in the startup cache,
    //                         parse from disk
    // other: the startup cache file could not be found, probably
    //        due to being accessed before a profile has been selected (e.g.
    //        loading chrome for the profile manager itself). This must be
    //        parsed from disk.

    if (proto) {
        // If we're racing with another document to load proto, wait till the
        // load has finished loading before trying to add cloned style sheets.
        // XULDocument::EndLoad will call proto->NotifyLoadDone, which will
        // find all racing documents and notify them via OnPrototypeLoadDone,
        // which will add style sheet clones to each document.
        bool loaded;
        rv = proto->AwaitLoadDone(this, &loaded);
        if (NS_FAILED(rv)) return rv;

        mMasterPrototype = mCurrentPrototype = proto;

        // Set up the right principal on ourselves.
        SetPrincipal(proto->DocumentPrincipal());

        // We need a listener, even if proto is not yet loaded, in which
        // event the listener's OnStopRequest method does nothing, and all
        // the interesting work happens below XULDocument::EndLoad, from
        // the call there to mCurrentPrototype->NotifyLoadDone().
        *aDocListener = new CachedChromeStreamListener(this, loaded);
        if (! *aDocListener)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();
        bool fillXULCache = (useXULCache && IsChromeURI(mDocumentURI));


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
XULDocument::EndLoad()
{
    // This can happen if an overlay fails to load
    if (!mCurrentPrototype)
        return;

    nsresult rv;

    // Whack the prototype document into the cache so that the next
    // time somebody asks for it, they don't need to load it by hand.

    nsCOMPtr<nsIURI> uri = mCurrentPrototype->GetURI();
    bool isChrome = IsChromeURI(uri);

    // Remember if the XUL cache is on
    bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();

    // If the current prototype is an overlay document (non-master prototype)
    // and we're filling the FastLoad disk cache, tell the cache we're done
    // loading it, and write the prototype. The master prototype is put into
    // the cache earlier in XULDocument::StartDocumentLoad.
    if (useXULCache && mIsWritingFastLoad && isChrome &&
        mMasterPrototype != mCurrentPrototype) {
        nsXULPrototypeCache::GetInstance()->WritePrototype(mCurrentPrototype);
    }

    if (IsOverlayAllowed(uri)) {
        nsCOMPtr<nsIXULOverlayProvider> reg =
            mozilla::services::GetXULOverlayProviderService();

        if (reg) {
            nsCOMPtr<nsISimpleEnumerator> overlays;
            rv = reg->GetStyleOverlays(uri, getter_AddRefs(overlays));
            if (NS_FAILED(rv)) return;

            bool moreSheets;
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

        if (isChrome && useXULCache) {
            // If it's a chrome prototype document, then notify any
            // documents that raced to load the prototype, and awaited
            // its load completion via proto->AwaitLoadDone().
            rv = mCurrentPrototype->NotifyLoadDone();
            if (NS_FAILED(rv)) return;
        }
    }

    OnPrototypeLoadDone(true);
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING)) {
        nsAutoCString urlspec;
        rv = uri->GetSpec(urlspec);
        if (NS_SUCCEEDED(rv)) {
            PR_LOG(gXULLog, PR_LOG_WARNING,
                   ("xul: Finished loading document '%s'", urlspec.get()));
        }
    }
#endif
}

NS_IMETHODIMP
XULDocument::OnPrototypeLoadDone(bool aResumeWalk)
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
bool
XULDocument::OnDocumentParserError()
{
  // don't report errors that are from overlays
  if (mCurrentPrototype && mMasterPrototype != mCurrentPrototype) {
    nsCOMPtr<nsIURI> uri = mCurrentPrototype->GetURI();
    if (IsChromeURI(uri)) {
      nsCOMPtr<nsIObserverService> os =
        mozilla::services::GetObserverService();
      if (os)
        os->NotifyObservers(uri, "xul-overlay-parsererror",
                            EmptyString().get());
    }

    return false;
  }

  return true;
}

static void
ClearBroadcasterMapEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
    BroadcasterMapEntry* entry =
        static_cast<BroadcasterMapEntry*>(aEntry);
    for (int32_t i = entry->mListeners.Count() - 1; i >= 0; --i) {
        delete (BroadcastListener*)entry->mListeners[i];
    }

    // N.B. that we need to manually run the dtor because we
    // constructed the nsSmallVoidArray object in-place.
    entry->mListeners.~nsSmallVoidArray();
}

static bool
CanBroadcast(int32_t aNameSpaceID, nsIAtom* aAttribute)
{
    // Don't push changes to the |id|, |ref|, |persist|, |command| or
    // |observes| attribute.
    if (aNameSpaceID == kNameSpaceID_None) {
        if ((aAttribute == nsGkAtoms::id) ||
            (aAttribute == nsGkAtoms::ref) ||
            (aAttribute == nsGkAtoms::persist) ||
            (aAttribute == nsGkAtoms::command) ||
            (aAttribute == nsGkAtoms::observes)) {
            return false;
        }
    }
    return true;
}

struct nsAttrNameInfo
{
  nsAttrNameInfo(int32_t aNamespaceID, nsIAtom* aName, nsIAtom* aPrefix) :
    mNamespaceID(aNamespaceID), mName(aName), mPrefix(aPrefix) {}
  nsAttrNameInfo(const nsAttrNameInfo& aOther) :
    mNamespaceID(aOther.mNamespaceID), mName(aOther.mName),
    mPrefix(aOther.mPrefix) {}
  int32_t           mNamespaceID;
  nsCOMPtr<nsIAtom> mName;
  nsCOMPtr<nsIAtom> mPrefix;
};

void
XULDocument::SynchronizeBroadcastListener(Element *aBroadcaster,
                                          Element *aListener,
                                          const nsAString &aAttr)
{
    if (!nsContentUtils::IsSafeToRunScript()) {
        nsDelayedBroadcastUpdate delayedUpdate(aBroadcaster, aListener,
                                               aAttr);
        mDelayedBroadcasters.AppendElement(delayedUpdate);
        MaybeBroadcast();
        return;
    }
    bool notify = mDocumentLoaded || mHandlingDelayedBroadcasters;

    if (aAttr.EqualsLiteral("*")) {
        uint32_t count = aBroadcaster->GetAttrCount();
        nsTArray<nsAttrNameInfo> attributes(count);
        for (uint32_t i = 0; i < count; ++i) {
            const nsAttrName* attrName = aBroadcaster->GetAttrNameAt(i);
            int32_t nameSpaceID = attrName->NamespaceID();
            nsIAtom* name = attrName->LocalName();

            // _Don't_ push the |id|, |ref|, or |persist| attribute's value!
            if (! CanBroadcast(nameSpaceID, name))
                continue;

            attributes.AppendElement(nsAttrNameInfo(nameSpaceID, name,
                                                    attrName->GetPrefix()));
        }

        count = attributes.Length();
        while (count-- > 0) {
            int32_t nameSpaceID = attributes[count].mNamespaceID;
            nsIAtom* name = attributes[count].mName;
            nsAutoString value;
            if (aBroadcaster->GetAttr(nameSpaceID, name, value)) {
              aListener->SetAttr(nameSpaceID, name, attributes[count].mPrefix,
                                 value, notify);
            }

#if 0
            // XXX we don't fire the |onbroadcast| handler during
            // initial hookup: doing so would potentially run the
            // |onbroadcast| handler before the |onload| handler,
            // which could define JS properties that mask XBL
            // properties, etc.
            ExecuteOnBroadcastHandlerFor(aBroadcaster, aListener, name);
#endif
        }
    }
    else {
        // Find out if the attribute is even present at all.
        nsCOMPtr<nsIAtom> name = do_GetAtom(aAttr);

        nsAutoString value;
        if (aBroadcaster->GetAttr(kNameSpaceID_None, name, value)) {
            aListener->SetAttr(kNameSpaceID_None, name, value, notify);
        } else {
            aListener->UnsetAttr(kNameSpaceID_None, name, notify);
        }

#if 0
        // XXX we don't fire the |onbroadcast| handler during initial
        // hookup: doing so would potentially run the |onbroadcast|
        // handler before the |onload| handler, which could define JS
        // properties that mask XBL properties, etc.
        ExecuteOnBroadcastHandlerFor(aBroadcaster, aListener, name);
#endif
    }
}

NS_IMETHODIMP
XULDocument::AddBroadcastListenerFor(nsIDOMElement* aBroadcaster,
                                     nsIDOMElement* aListener,
                                     const nsAString& aAttr)
{
    ErrorResult rv;
    nsCOMPtr<Element> broadcaster = do_QueryInterface(aBroadcaster);
    nsCOMPtr<Element> listener = do_QueryInterface(aListener);
    NS_ENSURE_ARG(broadcaster && listener);
    AddBroadcastListenerFor(*broadcaster, *listener, aAttr, rv);
    return rv.ErrorCode();
}

void
XULDocument::AddBroadcastListenerFor(Element& aBroadcaster, Element& aListener,
                                     const nsAString& aAttr, ErrorResult& aRv)
{
    nsresult rv =
        nsContentUtils::CheckSameOrigin(this, &aBroadcaster);

    if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return;
    }

    rv = nsContentUtils::CheckSameOrigin(this, &aListener);

    if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return;
    }

    static PLDHashTableOps gOps = {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        PL_DHashVoidPtrKeyStub,
        PL_DHashMatchEntryStub,
        PL_DHashMoveEntryStub,
        ClearBroadcasterMapEntry,
        PL_DHashFinalizeStub,
        nullptr
    };

    if (! mBroadcasterMap) {
        mBroadcasterMap =
            PL_NewDHashTable(&gOps, nullptr, sizeof(BroadcasterMapEntry),
                             PL_DHASH_MIN_SIZE);

        if (! mBroadcasterMap) {
            aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
        }
    }

    BroadcasterMapEntry* entry =
        static_cast<BroadcasterMapEntry*>
                   (PL_DHashTableOperate(mBroadcasterMap, &aBroadcaster,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(entry)) {
        entry =
            static_cast<BroadcasterMapEntry*>
                       (PL_DHashTableOperate(mBroadcasterMap, &aBroadcaster,
                                                PL_DHASH_ADD));

        if (! entry) {
            aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
        }

        entry->mBroadcaster = &aBroadcaster;

        // N.B. placement new to construct the nsSmallVoidArray object
        // in-place
        new (&entry->mListeners) nsSmallVoidArray();
    }

    // Only add the listener if it's not there already!
    nsCOMPtr<nsIAtom> attr = do_GetAtom(aAttr);

    BroadcastListener* bl;
    for (int32_t i = entry->mListeners.Count() - 1; i >= 0; --i) {
        bl = static_cast<BroadcastListener*>(entry->mListeners[i]);

        nsCOMPtr<Element> blListener = do_QueryReferent(bl->mListener);

        if (blListener == &aListener && bl->mAttribute == attr)
            return;
    }

    bl = new BroadcastListener;

    bl->mListener  = do_GetWeakReference(&aListener);
    bl->mAttribute = attr;

    entry->mListeners.AppendElement(bl);

    SynchronizeBroadcastListener(&aBroadcaster, &aListener, aAttr);
}

NS_IMETHODIMP
XULDocument::RemoveBroadcastListenerFor(nsIDOMElement* aBroadcaster,
                                        nsIDOMElement* aListener,
                                        const nsAString& aAttr)
{
    nsCOMPtr<Element> broadcaster = do_QueryInterface(aBroadcaster);
    nsCOMPtr<Element> listener = do_QueryInterface(aListener);
    NS_ENSURE_ARG(broadcaster && listener);
    RemoveBroadcastListenerFor(*broadcaster, *listener, aAttr);
    return NS_OK;
}

void
XULDocument::RemoveBroadcastListenerFor(Element& aBroadcaster,
                                        Element& aListener,
                                        const nsAString& aAttr)
{
    // If we haven't added any broadcast listeners, then there sure
    // aren't any to remove.
    if (! mBroadcasterMap)
        return;

    BroadcasterMapEntry* entry =
        static_cast<BroadcasterMapEntry*>
                   (PL_DHashTableOperate(mBroadcasterMap, &aBroadcaster,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
        nsCOMPtr<nsIAtom> attr = do_GetAtom(aAttr);
        for (int32_t i = entry->mListeners.Count() - 1; i >= 0; --i) {
            BroadcastListener* bl =
                static_cast<BroadcastListener*>(entry->mListeners[i]);

            nsCOMPtr<Element> blListener = do_QueryReferent(bl->mListener);

            if (blListener == &aListener && bl->mAttribute == attr) {
                entry->mListeners.RemoveElementAt(i);
                delete bl;

                if (entry->mListeners.Count() == 0)
                    PL_DHashTableOperate(mBroadcasterMap, &aBroadcaster,
                                         PL_DHASH_REMOVE);

                break;
            }
        }
    }
}

nsresult
XULDocument::ExecuteOnBroadcastHandlerFor(Element* aBroadcaster,
                                          Element* aListener,
                                          nsIAtom* aAttr)
{
    // Now we execute the onchange handler in the context of the
    // observer. We need to find the observer in order to
    // execute the handler.

    for (nsIContent* child = aListener->GetFirstChild();
         child;
         child = child->GetNextSibling()) {

        // Look for an <observes> element beneath the listener. This
        // ought to have an |element| attribute that refers to
        // aBroadcaster, and an |attribute| element that tells us what
        // attriubtes we're listening for.
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
        nsEvent event(true, NS_XUL_BROADCAST);

        nsCOMPtr<nsIPresShell> shell = GetShell();
        if (shell) {
            nsRefPtr<nsPresContext> aPresContext = shell->GetPresContext();

            // Handle the DOM event
            nsEventStatus status = nsEventStatus_eIgnore;
            nsEventDispatcher::Dispatch(child, aPresContext, &event, nullptr,
                                        &status);
        }
    }

    return NS_OK;
}

void
XULDocument::AttributeWillChange(nsIDocument* aDocument,
                                 Element* aElement, int32_t aNameSpaceID,
                                 nsIAtom* aAttribute, int32_t aModType)
{
    NS_ABORT_IF_FALSE(aElement, "Null content!");
    NS_PRECONDITION(aAttribute, "Must have an attribute that's changing!");

    // XXXbz check aNameSpaceID, dammit!
    // See if we need to update our ref map.
    if (aAttribute == nsGkAtoms::ref ||
        (aAttribute == nsGkAtoms::id && !aElement->GetIDAttributeName())) {
        // Might not need this, but be safe for now.
        nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
        RemoveElementFromRefMap(aElement);
    }
}

void
XULDocument::AttributeChanged(nsIDocument* aDocument,
                              Element* aElement, int32_t aNameSpaceID,
                              nsIAtom* aAttribute, int32_t aModType)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");

    // Might not need this, but be safe for now.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    // XXXbz check aNameSpaceID, dammit!
    // See if we need to update our ref map.
    if (aAttribute == nsGkAtoms::ref ||
        (aAttribute == nsGkAtoms::id && !aElement->GetIDAttributeName())) {
        AddElementToRefMap(aElement);
    }
    
    nsresult rv;

    // Synchronize broadcast listeners
    if (mBroadcasterMap &&
        CanBroadcast(aNameSpaceID, aAttribute)) {
        BroadcasterMapEntry* entry =
            static_cast<BroadcasterMapEntry*>
                       (PL_DHashTableOperate(mBroadcasterMap, aElement,
                                                PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
            // We've got listeners: push the value.
            nsAutoString value;
            bool attrSet = aElement->GetAttr(kNameSpaceID_None, aAttribute, value);

            int32_t i;
            for (i = entry->mListeners.Count() - 1; i >= 0; --i) {
                BroadcastListener* bl =
                    static_cast<BroadcastListener*>(entry->mListeners[i]);

                if ((bl->mAttribute == aAttribute) ||
                    (bl->mAttribute == nsGkAtoms::_asterix)) {
                    nsCOMPtr<Element> listenerEl
                        = do_QueryReferent(bl->mListener);
                    if (listenerEl) {
                        nsAutoString currentValue;
                        bool hasAttr = listenerEl->GetAttr(kNameSpaceID_None,
                                                           aAttribute,
                                                           currentValue);
                        // We need to update listener only if we're
                        // (1) removing an existing attribute,
                        // (2) adding a new attribute or
                        // (3) changing the value of an attribute.
                        bool needsAttrChange =
                            attrSet != hasAttr || !value.Equals(currentValue);
                        nsDelayedBroadcastUpdate delayedUpdate(aElement,
                                                               listenerEl,
                                                               aAttribute,
                                                               value,
                                                               attrSet,
                                                               needsAttrChange);

                        uint32_t index =
                            mDelayedAttrChangeBroadcasts.IndexOf(delayedUpdate,
                                0, nsDelayedBroadcastUpdate::Comparator());
                        if (index != mDelayedAttrChangeBroadcasts.NoIndex) {
                            if (mHandlingDelayedAttrChange) {
                                NS_WARNING("Broadcasting loop!");
                                continue;
                            }
                            mDelayedAttrChangeBroadcasts.RemoveElementAt(index);
                        }

                        mDelayedAttrChangeBroadcasts.AppendElement(delayedUpdate);
                    }
                }
            }
        }
    }

    // checks for modifications in broadcasters
    bool listener, resolved;
    CheckBroadcasterHookup(aElement, &listener, &resolved);

    // See if there is anything we need to persist in the localstore.
    //
    // XXX Namespace handling broken :-(
    nsAutoString persist;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::persist, persist);
    if (!persist.IsEmpty()) {
        // XXXldb This should check that it's a token, not just a substring.
        if (persist.Find(nsDependentAtomString(aAttribute)) >= 0) {
            rv = Persist(aElement, kNameSpaceID_None, aAttribute);
            if (NS_FAILED(rv)) return;
        }
    }
}

void
XULDocument::ContentAppended(nsIDocument* aDocument,
                             nsIContent* aContainer,
                             nsIContent* aFirstNewContent,
                             int32_t aNewIndexInContainer)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");
    
    // Might not need this, but be safe for now.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    // Update our element map
    nsresult rv = NS_OK;
    for (nsIContent* cur = aFirstNewContent; cur && NS_SUCCEEDED(rv);
         cur = cur->GetNextSibling()) {
        rv = AddSubtreeToDocument(cur);
    }
}

void
XULDocument::ContentInserted(nsIDocument* aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             int32_t aIndexInContainer)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");

    // Might not need this, but be safe for now.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    AddSubtreeToDocument(aChild);
}

void
XULDocument::ContentRemoved(nsIDocument* aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            int32_t aIndexInContainer,
                            nsIContent* aPreviousSibling)
{
    NS_ASSERTION(aDocument == this, "unexpected doc");

    // Might not need this, but be safe for now.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    RemoveSubtreeFromDocument(aChild);
}

//----------------------------------------------------------------------
//
// nsIXULDocument interface
//

void
XULDocument::GetElementsForID(const nsAString& aID,
                              nsCOMArray<nsIContent>& aElements)
{
    aElements.Clear();

    nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(aID);
    if (entry) {
        entry->AppendAllIdContent(&aElements);
    }
    nsRefMapEntry *refEntry = mRefMap.GetEntry(aID);
    if (refEntry) {
        refEntry->AppendAll(&aElements);
    }
}

nsresult
XULDocument::AddForwardReference(nsForwardReference* aRef)
{
    if (mResolutionPhase < aRef->GetPhase()) {
        if (!mForwardReferences.AppendElement(aRef)) {
            delete aRef;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    else {
        NS_ERROR("forward references have already been resolved");
        delete aRef;
    }

    return NS_OK;
}

nsresult
XULDocument::ResolveForwardReferences()
{
    if (mResolutionPhase == nsForwardReference::eDone)
        return NS_OK;

    NS_ASSERTION(mResolutionPhase == nsForwardReference::eStart,
                 "nested ResolveForwardReferences()");
        
    // Resolve each outstanding 'forward' reference. We iterate
    // through the list of forward references until no more forward
    // references can be resolved. This annealing process is
    // guaranteed to converge because we've "closed the gate" to new
    // forward references.

    const nsForwardReference::Phase* pass = nsForwardReference::kPasses;
    while ((mResolutionPhase = *pass) != nsForwardReference::eDone) {
        uint32_t previous = 0;
        while (mForwardReferences.Length() &&
               mForwardReferences.Length() != previous) {
            previous = mForwardReferences.Length();

            for (uint32_t i = 0; i < mForwardReferences.Length(); ++i) {
                nsForwardReference* fwdref = mForwardReferences[i];

                if (fwdref->GetPhase() == *pass) {
                    nsForwardReference::Result result = fwdref->Resolve();

                    switch (result) {
                    case nsForwardReference::eResolve_Succeeded:
                    case nsForwardReference::eResolve_Error:
                        mForwardReferences.RemoveElementAt(i);

                        // fixup because we removed from list
                        --i;
                        break;

                    case nsForwardReference::eResolve_Later:
                        // do nothing. we'll try again later
                        ;
                    }

                    if (mResolutionPhase == nsForwardReference::eStart) {
                        // Resolve() loaded a dynamic overlay,
                        // (see XULDocument::LoadOverlayInternal()).
                        // Return for now, we will be called again.
                        return NS_OK;
                    }
                }
            }
        }

        ++pass;
    }

    mForwardReferences.Clear();
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIDOMDocument interface
//

NS_IMETHODIMP
XULDocument::GetElementsByAttribute(const nsAString& aAttribute,
                                    const nsAString& aValue,
                                    nsIDOMNodeList** aReturn)
{
    *aReturn = GetElementsByAttribute(aAttribute, aValue).get();
    return NS_OK;
}

already_AddRefed<nsINodeList>
XULDocument::GetElementsByAttribute(const nsAString& aAttribute,
                                    const nsAString& aValue)
{
    nsCOMPtr<nsIAtom> attrAtom(do_GetAtom(aAttribute));
    void* attrValue = new nsString(aValue);
    nsRefPtr<nsContentList> list = new nsContentList(this,
                                            MatchAttribute,
                                            nsContentUtils::DestroyMatchString,
                                            attrValue,
                                            true,
                                            attrAtom,
                                            kNameSpaceID_Unknown);
    
    return list.forget();
}

NS_IMETHODIMP
XULDocument::GetElementsByAttributeNS(const nsAString& aNamespaceURI,
                                      const nsAString& aAttribute,
                                      const nsAString& aValue,
                                      nsIDOMNodeList** aReturn)
{
    ErrorResult rv;
    *aReturn = GetElementsByAttributeNS(aNamespaceURI, aAttribute,
                                        aValue, rv).get();
    return rv.ErrorCode();
}

already_AddRefed<nsINodeList>
XULDocument::GetElementsByAttributeNS(const nsAString& aNamespaceURI,
                                      const nsAString& aAttribute,
                                      const nsAString& aValue,
                                      ErrorResult& aRv)
{
    nsCOMPtr<nsIAtom> attrAtom(do_GetAtom(aAttribute));
    void* attrValue = new nsString(aValue);

    int32_t nameSpaceId = kNameSpaceID_Wildcard;
    if (!aNamespaceURI.EqualsLiteral("*")) {
      nsresult rv =
        nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                              nameSpaceId);
      if (NS_FAILED(rv)) {
          aRv.Throw(rv);
          return nullptr;
      }
    }

    nsRefPtr<nsContentList> list = new nsContentList(this,
                                            MatchAttribute,
                                            nsContentUtils::DestroyMatchString,
                                            attrValue,
                                            true,
                                            attrAtom,
                                            nameSpaceId);
    return list.forget();
}

NS_IMETHODIMP
XULDocument::Persist(const nsAString& aID,
                     const nsAString& aAttr)
{
    // If we're currently reading persisted attributes out of the
    // localstore, _don't_ re-enter and try to set them again!
    if (mApplyingPersistedAttrs)
        return NS_OK;

    Element* element = nsDocument::GetElementById(aID);
    if (!element)
        return NS_OK;

    nsCOMPtr<nsIAtom> tag;
    int32_t nameSpaceID;

    nsCOMPtr<nsINodeInfo> ni = element->GetExistingAttrNameFromQName(aAttr);
    nsresult rv;
    if (ni) {
        tag = ni->NameAtom();
        nameSpaceID = ni->NamespaceID();
    }
    else {
        // Make sure that this QName is going to be valid.
        const PRUnichar *colon;
        rv = nsContentUtils::CheckQName(PromiseFlatString(aAttr), true, &colon);

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
XULDocument::Persist(nsIContent* aElement, int32_t aNameSpaceID,
                     nsIAtom* aAttribute)
{
    // For non-chrome documents, persistance is simply broken
    if (!nsContentUtils::IsSystemPrincipal(NodePrincipal()))
        return NS_ERROR_NOT_AVAILABLE;

    // First make sure we _have_ a local store to stuff the persisted
    // information into. (We might not have one if profile information
    // hasn't been loaded yet...)
    if (!mLocalStore)
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
    // Don't bother with unreasonable attributes. We clamp long values,
    // but truncating attribute names turns it into a different attribute
    // so there's no point in persisting anything at all
    nsAtomCString attrstr(aAttribute);
    if (attrstr.Length() > kMaxAttrNameLength) {
        NS_WARNING("Can't persist, Attribute name too long");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsCOMPtr<nsIRDFResource> attr;
    rv = gRDFService->GetResource(attrstr,
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
    rv = mLocalStore->GetTarget(element, attr, true, getter_AddRefs(oldvalue));
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
            rv = mLocalStore->Assert(element, attr, newvalue, true);
        }
    }

    if (NS_FAILED(rv)) return rv;

    // Add it to the persisted set for this document (if it's not
    // there already).
    {
        nsAutoCString docurl;
        rv = mDocumentURI->GetSpec(docurl);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> doc;
        rv = gRDFService->GetResource(docurl, getter_AddRefs(doc));
        if (NS_FAILED(rv)) return rv;

        bool hasAssertion;
        rv = mLocalStore->HasAssertion(doc, kNC_persist, element, true, &hasAssertion);
        if (NS_FAILED(rv)) return rv;

        if (! hasAssertion) {
            rv = mLocalStore->Assert(doc, kNC_persist, element, true);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


nsresult
XULDocument::GetViewportSize(int32_t* aWidth,
                             int32_t* aHeight)
{
    *aWidth = *aHeight = 0;

    FlushPendingNotifications(Flush_Layout);

    nsIPresShell *shell = GetShell();
    NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

    nsIFrame* frame = shell->GetRootFrame();
    NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

    nsSize size = frame->GetSize();

    *aWidth = nsPresContext::AppUnitsToIntCSSPixels(size.width);
    *aHeight = nsPresContext::AppUnitsToIntCSSPixels(size.height);

    return NS_OK;
}

NS_IMETHODIMP
XULDocument::GetWidth(int32_t* aWidth)
{
    NS_ENSURE_ARG_POINTER(aWidth);

    int32_t height;
    return GetViewportSize(aWidth, &height);
}

int32_t
XULDocument::GetWidth(ErrorResult& aRv)
{
    int32_t width;
    aRv = GetWidth(&width);
    return width;
}

NS_IMETHODIMP
XULDocument::GetHeight(int32_t* aHeight)
{
    NS_ENSURE_ARG_POINTER(aHeight);

    int32_t width;
    return GetViewportSize(&width, aHeight);
}

int32_t
XULDocument::GetHeight(ErrorResult& aRv)
{
    int32_t height;
    aRv = GetHeight(&height);
    return height;
}

JSObject*
GetScopeObjectOfNode(nsIDOMNode* node)
{
    MOZ_ASSERT(node, "Must not be called with null.");

    // Window root occasionally keeps alive a node of a document whose
    // window is already dead. If in this brief period someone calls
    // GetPopupNode and we return that node, nsNodeSH::PreCreate will throw,
    // because it will not know which scope this node belongs to. Returning
    // an orphan node like that to JS would be a bug anyway, so to avoid
    // this, let's do the same check as nsNodeSH::PreCreate does to
    // determine the scope and if it fails let's just return null in
    // XULDocument::GetPopupNode.
    nsCOMPtr<nsINode> inode = do_QueryInterface(node);
    MOZ_ASSERT(inode, "How can this happen?");

    nsIDocument* doc = inode->OwnerDoc();
    MOZ_ASSERT(inode, "This should never happen.");

    nsIGlobalObject* global = doc->GetScopeObject();
    return global ? global->GetGlobalJSObject() : nullptr;
}

//----------------------------------------------------------------------
//
// nsIDOMXULDocument interface
//

NS_IMETHODIMP
XULDocument::GetPopupNode(nsIDOMNode** aNode)
{
    *aNode = nullptr;

    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsPIWindowRoot> rootWin = GetWindowRoot();
    if (rootWin)
        node = rootWin->GetPopupNode(); // addref happens here

    if (!node) {
        nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
        if (pm) {
            node = pm->GetLastTriggerPopupNode(this);
        }
    }

    if (node && nsContentUtils::CanCallerAccess(node)
        && GetScopeObjectOfNode(node)) {
        node.swap(*aNode);
    }

    return NS_OK;
}

already_AddRefed<nsINode>
XULDocument::GetPopupNode()
{
    nsCOMPtr<nsIDOMNode> node;
    DebugOnly<nsresult> rv = GetPopupNode(getter_AddRefs(node));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    nsCOMPtr<nsINode> retval(do_QueryInterface(node));
    return retval.forget();
}

NS_IMETHODIMP
XULDocument::SetPopupNode(nsIDOMNode* aNode)
{
    if (aNode) {
        // only allow real node objects
        nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
        NS_ENSURE_ARG(node);
    }

    nsCOMPtr<nsPIWindowRoot> rootWin = GetWindowRoot();
    if (rootWin)
        rootWin->SetPopupNode(aNode); // addref happens here

    return NS_OK;
}

void
XULDocument::SetPopupNode(nsINode* aNode)
{
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aNode));
    DebugOnly<nsresult> rv = SetPopupNode(node);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
}

// Returns the rangeOffset element from the XUL Popup Manager. This is for
// chrome callers only.
NS_IMETHODIMP
XULDocument::GetPopupRangeParent(nsIDOMNode** aRangeParent)
{
    NS_ENSURE_ARG_POINTER(aRangeParent);
    *aRangeParent = nullptr;

    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (!pm)
        return NS_ERROR_FAILURE;

    int32_t offset;
    pm->GetMouseLocation(aRangeParent, &offset);

    if (*aRangeParent && !nsContentUtils::CanCallerAccess(*aRangeParent)) {
        NS_RELEASE(*aRangeParent);
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    return NS_OK;
}

already_AddRefed<nsINode>
XULDocument::GetPopupRangeParent(ErrorResult& aRv)
{
    nsCOMPtr<nsIDOMNode> node;
    aRv = GetPopupRangeParent(getter_AddRefs(node));
    nsCOMPtr<nsINode> retval(do_QueryInterface(node));
    return retval.forget();
}


// Returns the rangeOffset element from the XUL Popup Manager. We check the
// rangeParent to determine if the caller has rights to access to the data.
NS_IMETHODIMP
XULDocument::GetPopupRangeOffset(int32_t* aRangeOffset)
{
    ErrorResult rv;
    *aRangeOffset = GetPopupRangeOffset(rv);
    return rv.ErrorCode();
}

int32_t
XULDocument::GetPopupRangeOffset(ErrorResult& aRv)
{
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (!pm) {
        aRv.Throw(NS_ERROR_FAILURE);
        return 0;
    }

    int32_t offset;
    nsCOMPtr<nsIDOMNode> parent;
    pm->GetMouseLocation(getter_AddRefs(parent), &offset);

    if (parent && !nsContentUtils::CanCallerAccess(parent)) {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return 0;
    }
    return offset;
}

NS_IMETHODIMP
XULDocument::GetTooltipNode(nsIDOMNode** aNode)
{
    *aNode = nullptr;

    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
        nsCOMPtr<nsIDOMNode> node = pm->GetLastTriggerTooltipNode(this);
        if (node && nsContentUtils::CanCallerAccess(node))
            node.swap(*aNode);
    }

    return NS_OK;
}

already_AddRefed<nsINode>
XULDocument::GetTooltipNode()
{
    nsCOMPtr<nsIDOMNode> node;
    DebugOnly<nsresult> rv = GetTooltipNode(getter_AddRefs(node));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    nsCOMPtr<nsINode> retval(do_QueryInterface(node));
    return retval.forget();
}

NS_IMETHODIMP
XULDocument::SetTooltipNode(nsIDOMNode* aNode)
{
    // do nothing
    return NS_OK;
}


NS_IMETHODIMP
XULDocument::GetCommandDispatcher(nsIDOMXULCommandDispatcher** aTracker)
{
    *aTracker = mCommandDispatcher;
    NS_IF_ADDREF(*aTracker);
    return NS_OK;
}

Element*
XULDocument::GetElementById(const nsAString& aId)
{
    if (!CheckGetElementByIdArg(aId))
        return nullptr;

    nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(aId);
    if (entry) {
        Element* element = entry->GetIdElement();
        if (element)
            return element;
    }

    nsRefMapEntry* refEntry = mRefMap.GetEntry(aId);
    if (refEntry) {
        NS_ASSERTION(refEntry->GetFirstElement(),
                     "nsRefMapEntries should have nonempty content lists");
        return refEntry->GetFirstElement();
    }
    return nullptr;
}

nsresult
XULDocument::AddElementToDocumentPre(Element* aElement)
{
    // Do a bunch of work that's necessary when an element gets added
    // to the XUL Document.
    nsresult rv;

    // 1. Add the element to the resource-to-element map. Also add it to
    // the id map, since it seems this can be called when creating
    // elements from prototypes.
    nsIAtom* id = aElement->GetID();
    if (id) {
        nsAutoScriptBlocker scriptBlocker;
        AddToIdTable(aElement, id);
    }
    rv = AddElementToRefMap(aElement);
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
    bool listener, resolved;
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
XULDocument::AddElementToDocumentPost(Element* aElement)
{
    // We need to pay special attention to the keyset tag to set up a listener
    if (aElement->NodeInfo()->Equals(nsGkAtoms::keyset, kNameSpaceID_XUL)) {
        // Create our XUL key listener and hook it up.
        nsXBLService::AttachGlobalKeyHandler(aElement);
    }

    // See if we need to attach a XUL template to this node
    bool needsHookup;
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
XULDocument::AddSubtreeToDocument(nsIContent* aContent)
{
    NS_ASSERTION(aContent->GetCurrentDoc() == this, "Element not in doc!");
    // From here on we only care about elements.
    if (!aContent->IsElement()) {
        return NS_OK;
    }

    Element* aElement = aContent->AsElement();

    // Do pre-order addition magic
    nsresult rv = AddElementToDocumentPre(aElement);
    if (NS_FAILED(rv)) return rv;

    // Recurse to children
    for (nsIContent* child = aElement->GetLastChild();
         child;
         child = child->GetPreviousSibling()) {

        rv = AddSubtreeToDocument(child);
        if (NS_FAILED(rv))
            return rv;
    }

    // Do post-order addition magic
    return AddElementToDocumentPost(aElement);
}

NS_IMETHODIMP
XULDocument::RemoveSubtreeFromDocument(nsIContent* aContent)
{
    // From here on we only care about elements.
    if (!aContent->IsElement()) {
        return NS_OK;
    }

    Element* aElement = aContent->AsElement();

    // Do a bunch of cleanup to remove an element from the XUL
    // document.
    nsresult rv;

    if (aElement->NodeInfo()->Equals(nsGkAtoms::keyset, kNameSpaceID_XUL)) {
        nsXBLService::DetachGlobalKeyHandler(aElement);
    }

    // 1. Remove any children from the document.
    for (nsIContent* child = aElement->GetLastChild();
         child;
         child = child->GetPreviousSibling()) {

        rv = RemoveSubtreeFromDocument(child);
        if (NS_FAILED(rv))
            return rv;
    }

    // 2. Remove the element from the resource-to-element map.
    // Also remove it from the id map, since we added it in
    // AddElementToDocumentPre().
    RemoveElementFromRefMap(aElement);
    nsIAtom* id = aElement->GetID();
    if (id) {
        nsAutoScriptBlocker scriptBlocker;
        RemoveFromIdTable(aElement, id);
    }

    // 3. If the element is a 'command updater', then remove the
    // element from the document's command dispatcher.
    if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::commandupdater,
                              nsGkAtoms::_true, eCaseMatters)) {
        nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(aElement);
        NS_ASSERTION(domelement != nullptr, "not a DOM element");
        if (! domelement)
            return NS_ERROR_UNEXPECTED;

        rv = mCommandDispatcher->RemoveCommandUpdater(domelement);
        if (NS_FAILED(rv)) return rv;
    }

    // 4. Remove the element from our broadcaster map, since it is no longer
    // in the document.
    nsCOMPtr<Element> broadcaster, listener;
    nsAutoString attribute, broadcasterID;
    rv = FindBroadcaster(aElement, getter_AddRefs(listener),
                         broadcasterID, attribute, getter_AddRefs(broadcaster));
    if (rv == NS_FINDBROADCASTER_FOUND) {
        RemoveBroadcastListenerFor(*broadcaster, *listener, attribute);
    }

    return NS_OK;
}

NS_IMETHODIMP
XULDocument::SetTemplateBuilderFor(nsIContent* aContent,
                                   nsIXULTemplateBuilder* aBuilder)
{
    if (! mTemplateBuilderTable) {
        if (!aBuilder) {
            return NS_OK;
        }
        mTemplateBuilderTable = new BuilderTable;
        mTemplateBuilderTable->Init();
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
XULDocument::GetTemplateBuilderFor(nsIContent* aContent,
                                   nsIXULTemplateBuilder** aResult)
{
    if (mTemplateBuilderTable) {
        mTemplateBuilderTable->Get(aContent, aResult);
    }
    else
        *aResult = nullptr;

    return NS_OK;
}

static void
GetRefMapAttribute(Element* aElement, nsAutoString* aValue)
{
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::ref, *aValue);
    if (aValue->IsEmpty() && !aElement->GetIDAttributeName()) {
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::id, *aValue);
    }
}

nsresult
XULDocument::AddElementToRefMap(Element* aElement)
{
    // Look at the element's 'ref' attribute, and if set,
    // add an entry in the resource-to-element map to the element.
    nsAutoString value;
    GetRefMapAttribute(aElement, &value);
    if (!value.IsEmpty()) {
        nsRefMapEntry *entry = mRefMap.PutEntry(value);
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        if (!entry->AddElement(aElement))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

void
XULDocument::RemoveElementFromRefMap(Element* aElement)
{
    // Remove the element from the resource-to-element map.
    nsAutoString value;
    GetRefMapAttribute(aElement, &value);
    if (!value.IsEmpty()) {
        nsRefMapEntry *entry = mRefMap.GetEntry(value);
        if (!entry)
            return;
        if (entry->RemoveElement(aElement)) {
            mRefMap.RawRemoveEntry(entry);
        }
    }
}

//----------------------------------------------------------------------
//
// nsIDOMNode interface
//

nsresult
XULDocument::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
    // We don't allow cloning of a XUL document
    *aResult = nullptr;
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
XULDocument::Init()
{
    mRefMap.Init();

    nsresult rv = XMLDocument::Init();
    NS_ENSURE_SUCCESS(rv, rv);

    // Create our command dispatcher and hook it up.
    mCommandDispatcher = new nsXULCommandDispatcher(this);
    NS_ENSURE_TRUE(mCommandDispatcher, NS_ERROR_OUT_OF_MEMORY);

    // this _could_ fail; e.g., if we've tried to grab the local store
    // before profiles have initialized. If so, no big deal; nothing
    // will persist.
    mLocalStore = do_GetService(NS_LOCALSTORE_CONTRACTID);

    if (gRefCnt++ == 0) {
        // Keep the RDF service cached in a member variable to make using
        // it a bit less painful
        rv = CallGetService("@mozilla.org/rdf/rdf-service;1", &gRDFService);
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

    Preferences::RegisterCallback(XULDocument::DirectionChanged,
                                  "intl.uidirection.", this);

#ifdef PR_LOGGING
    if (! gXULLog)
        gXULLog = PR_NewLogModule("XULDocument");
#endif

    return NS_OK;
}


nsresult
XULDocument::StartLayout(void)
{
    mMayStartLayout = true;
    nsCOMPtr<nsIPresShell> shell = GetShell();
    if (shell) {
        // Resize-reflow this time
        nsPresContext *cx = shell->GetPresContext();
        NS_ASSERTION(cx != nullptr, "no pres context");
        if (! cx)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsISupports> container = cx->GetContainer();
        NS_ASSERTION(container != nullptr, "pres context has no container");
        if (! container)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
        NS_ASSERTION(docShell != nullptr, "container is not a docshell");
        if (! docShell)
            return NS_ERROR_UNEXPECTED;

        nsresult rv = NS_OK;
        nsRect r = cx->GetVisibleArea();
        rv = shell->Initialize(r.width, r.height);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

/* static */
bool
XULDocument::MatchAttribute(nsIContent* aContent,
                            int32_t aNamespaceID,
                            nsIAtom* aAttrName,
                            void* aData)
{
    NS_PRECONDITION(aContent, "Must have content node to work with!");
    nsString* attrValue = static_cast<nsString*>(aData);
    if (aNamespaceID != kNameSpaceID_Unknown &&
        aNamespaceID != kNameSpaceID_Wildcard) {
        return attrValue->EqualsLiteral("*") ?
            aContent->HasAttr(aNamespaceID, aAttrName) :
            aContent->AttrValueIs(aNamespaceID, aAttrName, *attrValue,
                                  eCaseMatters);
    }

    // Qualified name match. This takes more work.

    uint32_t count = aContent->GetAttrCount();
    for (uint32_t i = 0; i < count; ++i) {
        const nsAttrName* name = aContent->GetAttrNameAt(i);
        bool nameMatch;
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

    return false;
}

nsresult
XULDocument::PrepareToLoad(nsISupports* aContainer,
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
XULDocument::PrepareToLoadPrototype(nsIURI* aURI, const char* aCommand,
                                    nsIPrincipal* aDocumentPrincipal,
                                    nsIParser** aResult)
{
    nsresult rv;

    // Create a new prototype document.
    rv = NS_NewXULPrototypeDocument(getter_AddRefs(mCurrentPrototype));
    if (NS_FAILED(rv)) return rv;

    rv = mCurrentPrototype->InitPrincipal(aURI, aDocumentPrincipal);
    if (NS_FAILED(rv)) {
        mCurrentPrototype = nullptr;
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
XULDocument::ApplyPersistentAttributes()
{
    // For non-chrome documents, persistance is simply broken
    if (!nsContentUtils::IsSystemPrincipal(NodePrincipal()))
        return NS_ERROR_NOT_AVAILABLE;

    // Add all of the 'persisted' attributes into the content
    // model.
    if (!mLocalStore)
        return NS_OK;

    mApplyingPersistedAttrs = true;
    ApplyPersistentAttributesInternal();
    mApplyingPersistedAttrs = false;

    return NS_OK;
}


nsresult 
XULDocument::ApplyPersistentAttributesInternal()
{
    nsCOMArray<nsIContent> elements;

    nsAutoCString docurl;
    mDocumentURI->GetSpec(docurl);

    nsCOMPtr<nsIRDFResource> doc;
    gRDFService->GetResource(docurl, getter_AddRefs(doc));

    nsCOMPtr<nsISimpleEnumerator> persisted;
    mLocalStore->GetTargets(doc, kNC_persist, true, getter_AddRefs(persisted));

    while (1) {
        bool hasmore = false;
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

    return NS_OK;
}


nsresult
XULDocument::ApplyPersistentAttributesToElements(nsIRDFResource* aResource,
                                                 nsCOMArray<nsIContent>& aElements)
{
    nsresult rv;

    nsCOMPtr<nsISimpleEnumerator> attrs;
    rv = mLocalStore->ArcLabelsOut(aResource, getter_AddRefs(attrs));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        bool hasmore;
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
        rv = mLocalStore->GetTarget(aResource, property, true,
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

        uint32_t cnt = aElements.Count();

        for (int32_t i = int32_t(cnt) - 1; i >= 0; --i) {
            nsCOMPtr<nsIContent> element = aElements.SafeObjectAt(i);
            if (!element)
                continue;

            rv = element->SetAttr(/* XXX */ kNameSpaceID_None,
                                  attr,
                                  wrapper,
                                  true);
        }
    }

    return NS_OK;
}

void
XULDocument::TraceProtos(JSTracer* aTrc, uint32_t aGCNumber)
{
    uint32_t i, count = mPrototypes.Length();
    for (i = 0; i < count; ++i) {
        mPrototypes[i]->TraceProtos(aTrc, aGCNumber);
    }
}

//----------------------------------------------------------------------
//
// XULDocument::ContextStack
//

XULDocument::ContextStack::ContextStack()
    : mTop(nullptr), mDepth(0)
{
}

XULDocument::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        NS_IF_RELEASE(doomed->mElement);
        delete doomed;
    }
}

nsresult
XULDocument::ContextStack::Push(nsXULPrototypeElement* aPrototype,
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
XULDocument::ContextStack::Pop()
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
XULDocument::ContextStack::Peek(nsXULPrototypeElement** aPrototype,
                                nsIContent** aElement,
                                int32_t* aIndex)
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
XULDocument::ContextStack::SetTopIndex(int32_t aIndex)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    mTop->mIndex = aIndex;
    return NS_OK;
}


//----------------------------------------------------------------------
//
// Content model walking routines
//

nsresult
XULDocument::PrepareToWalk()
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

            nsAutoCString urlspec;
            rv = url->GetSpec(urlspec);
            if (NS_FAILED(rv)) return rv;

            PR_LOG(gXULLog, PR_LOG_ERROR,
                   ("xul: error parsing '%s'", urlspec.get()));
        }
#endif

        return NS_OK;
    }

    uint32_t piInsertionPoint = 0;
    if (mState != eState_Master) {
        int32_t indexOfRoot = IndexOf(GetRootElement());
        NS_ASSERTION(indexOfRoot >= 0,
                     "No root content when preparing to walk overlay!");
        piInsertionPoint = indexOfRoot;
    }

    const nsTArray<nsRefPtr<nsXULPrototypePI> >& processingInstructions =
        mCurrentPrototype->GetProcessingInstructions();

    uint32_t total = processingInstructions.Length();
    for (uint32_t i = 0; i < total; ++i) {
        rv = CreateAndInsertPI(processingInstructions[i],
                               this, piInsertionPoint + i);
        if (NS_FAILED(rv)) return rv;
    }

    // Now check the chrome registry for any additional overlays.
    rv = AddChromeOverlays();
    if (NS_FAILED(rv)) return rv;

    // Do one-time initialization if we're preparing to walk the
    // master document's prototype.
    nsRefPtr<Element> root;

    if (mState == eState_Master) {
        // Add the root element
        rv = CreateElementFromPrototype(proto, getter_AddRefs(root), true);
        if (NS_FAILED(rv)) return rv;

        rv = AppendChildTo(root, false);
        if (NS_FAILED(rv)) return rv;
        
        rv = AddElementToRefMap(root);
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
XULDocument::CreateAndInsertPI(const nsXULPrototypePI* aProtoPI,
                               nsINode* aParent, uint32_t aIndex)
{
    NS_PRECONDITION(aProtoPI, "null ptr");
    NS_PRECONDITION(aParent, "null ptr");

    nsRefPtr<ProcessingInstruction> node =
        NS_NewXMLProcessingInstruction(mNodeInfoManager, aProtoPI->mTarget,
                                       aProtoPI->mData);

    nsresult rv;
    if (aProtoPI->mTarget.EqualsLiteral("xml-stylesheet")) {
        rv = InsertXMLStylesheetPI(aProtoPI, aParent, aIndex, node);
    } else if (aProtoPI->mTarget.EqualsLiteral("xul-overlay")) {
        rv = InsertXULOverlayPI(aProtoPI, aParent, aIndex, node);
    } else {
        // No special processing, just add the PI to the document.
        rv = aParent->InsertChildAt(node, aIndex, false);
    }

    return rv;
}

nsresult
XULDocument::InsertXMLStylesheetPI(const nsXULPrototypePI* aProtoPI,
                                   nsINode* aParent,
                                   uint32_t aIndex,
                                   nsIContent* aPINode)
{
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aPINode));
    NS_ASSERTION(ssle, "passed XML Stylesheet node does not "
                       "implement nsIStyleSheetLinkingElement!");

    nsresult rv;

    ssle->InitStyleLinkElement(false);
    // We want to be notified when the style sheet finishes loading, so
    // disable style sheet loading for now.
    ssle->SetEnableUpdates(false);
    ssle->OverrideBaseURI(mCurrentPrototype->GetURI());

    rv = aParent->InsertChildAt(aPINode, aIndex, false);
    if (NS_FAILED(rv)) return rv;

    ssle->SetEnableUpdates(true);

    // load the stylesheet if necessary, passing ourselves as
    // nsICSSObserver
    bool willNotify;
    bool isAlternate;
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
XULDocument::InsertXULOverlayPI(const nsXULPrototypePI* aProtoPI,
                                nsINode* aParent,
                                uint32_t aIndex,
                                nsIContent* aPINode)
{
    nsresult rv;

    rv = aParent->InsertChildAt(aPINode, aIndex, false);
    if (NS_FAILED(rv)) return rv;

    // xul-overlay PI is special only in prolog
    if (!nsContentUtils::InProlog(aPINode)) {
        return NS_OK;
    }

    nsAutoString href;
    nsContentUtils::GetPseudoAttributeValue(aProtoPI->mData,
                                            nsGkAtoms::href,
                                            href);

    // If there was no href, we can't do anything with this PI
    if (href.IsEmpty()) {
        return NS_OK;
    }

    // Add the overlay to our list of overlays that need to be processed.
    nsCOMPtr<nsIURI> uri;

    rv = NS_NewURI(getter_AddRefs(uri), href, nullptr,
                   mCurrentPrototype->GetURI());
    if (NS_SUCCEEDED(rv)) {
        // We insert overlays into mUnloadedOverlays at the same index in
        // document order, so they end up in the reverse of the document
        // order in mUnloadedOverlays.
        // This is needed because the code in ResumeWalk loads the overlays
        // by processing the last item of mUnloadedOverlays and removing it
        // from the array.
        mUnloadedOverlays.InsertElementAt(0, uri);
        rv = NS_OK;
    } else if (rv == NS_ERROR_MALFORMED_URI) {
        // The URL is bad, move along. Don't propagate for now.
        // XXX report this to the Error Console (bug 359846)
        rv = NS_OK;
    }

    return rv;
}

nsresult
XULDocument::AddChromeOverlays()
{
    nsresult rv;

    nsCOMPtr<nsIURI> docUri = mCurrentPrototype->GetURI();

    /* overlays only apply to chrome or about URIs */
    if (!IsOverlayAllowed(docUri)) return NS_OK;

    nsCOMPtr<nsIXULOverlayProvider> chromeReg =
        mozilla::services::GetXULOverlayProviderService();
    // In embedding situations, the chrome registry may not provide overlays,
    // or even exist at all; that's OK.
    NS_ENSURE_TRUE(chromeReg, NS_OK);

    nsCOMPtr<nsISimpleEnumerator> overlays;
    rv = chromeReg->GetXULOverlays(docUri, getter_AddRefs(overlays));
    NS_ENSURE_SUCCESS(rv, rv);

    bool moreOverlays;
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

        // Same comment as in XULDocument::InsertXULOverlayPI
        mUnloadedOverlays.InsertElementAt(0, uri);
    }

    return rv;
}

NS_IMETHODIMP
XULDocument::LoadOverlay(const nsAString& aURL, nsIObserver* aObserver)
{
    nsresult rv;

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr);
    if (NS_FAILED(rv)) return rv;

    if (aObserver) {
        nsIObserver* obs = nullptr;
        if (!mOverlayLoadObservers.IsInitialized()) {
            mOverlayLoadObservers.Init();
        }
        obs = mOverlayLoadObservers.GetWeak(uri);

        if (obs) {
            // We don't support loading the same overlay twice into the same
            // document - that doesn't make sense anyway.
            return NS_ERROR_FAILURE;
        }
        mOverlayLoadObservers.Put(uri, aObserver);
    }
    bool shouldReturn, failureFromContent;
    rv = LoadOverlayInternal(uri, true, &shouldReturn, &failureFromContent);
    if (NS_FAILED(rv) && mOverlayLoadObservers.IsInitialized())
        mOverlayLoadObservers.Remove(uri); // remove the observer if LoadOverlayInternal generated an error
    return rv;
}

nsresult
XULDocument::LoadOverlayInternal(nsIURI* aURI, bool aIsDynamic,
                                 bool* aShouldReturn,
                                 bool* aFailureFromContent)
{
    nsresult rv;

    *aShouldReturn = false;
    *aFailureFromContent = false;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_DEBUG)) {
        nsAutoCString urlspec;
        aURI->GetSpec(urlspec);
        nsAutoCString parentDoc;
        nsCOMPtr<nsIURI> uri;
        nsresult rv = mChannel->GetOriginalURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv))
            rv = uri->GetSpec(parentDoc);
        if (!(parentDoc.get()))
            parentDoc = "";

        PR_LOG(gXULLog, PR_LOG_DEBUG,
                ("xul: %s loading overlay %s", parentDoc.get(), urlspec.get()));
    }
#endif

    if (aIsDynamic)
        mResolutionPhase = nsForwardReference::eStart;

    // Chrome documents are allowed to load overlays from anywhere.
    // In all other cases, the overlay is only allowed to load if
    // the master document and prototype document have the same origin.

    bool documentIsChrome = IsChromeURI(mDocumentURI);
    if (!documentIsChrome) {
        // Make sure we're allowed to load this overlay.
        rv = NodePrincipal()->CheckMayLoad(aURI, true, false);
        if (NS_FAILED(rv)) {
            *aFailureFromContent = true;
            return rv;
        }
    }

    // Look in the prototype cache for the prototype document with
    // the specified overlay URI. Only use the cache if the containing
    // document is chrome otherwise it may not have a system principal and
    // the cached document will, see bug 565610.
    bool overlayIsChrome = IsChromeURI(aURI);
    mCurrentPrototype = overlayIsChrome && documentIsChrome ?
        nsXULPrototypeCache::GetInstance()->GetPrototype(aURI) : nullptr;

    // Same comment as nsChromeProtocolHandler::NewChannel and
    // XULDocument::StartDocumentLoad
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

    bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();
    if (useXULCache && mCurrentPrototype) {
        bool loaded;
        rv = mCurrentPrototype->AwaitLoadDone(this, &loaded);
        if (NS_FAILED(rv)) return rv;

        if (! loaded) {
            // Return to the main event loop and eagerly await the
            // prototype overlay load's completion. When the content
            // sink completes, it will trigger an EndLoad(), which'll
            // wind us back up here, in ResumeWalk().
            *aShouldReturn = true;
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

        if (mIsGoingAway) {
            PR_LOG(gXULLog, PR_LOG_DEBUG, ("xul: ...and document already destroyed"));
            return NS_ERROR_NOT_AVAILABLE;
        }

        // We'll set the right principal on the proto doc when we get
        // OnStartRequest from the parser, so just pass in a null principal for
        // now.
        nsCOMPtr<nsIParser> parser;
        rv = PrepareToLoadPrototype(aURI, "view", nullptr, getter_AddRefs(parser));
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
        ParserObserver* parserObserver =
            new ParserObserver(this, mCurrentPrototype);
        if (! parserObserver)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(parserObserver);
        parser->Parse(aURI, parserObserver);
        NS_RELEASE(parserObserver);

        nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
        nsCOMPtr<nsIChannel> channel;
        rv = NS_NewChannel(getter_AddRefs(channel), aURI, nullptr, group);

        if (NS_SUCCEEDED(rv)) {
            // Set the owner of the channel to be our principal so
            // that the overlay's JSObjects etc end up being created
            // with the right principal and in the correct
            // compartment.
            channel->SetOwner(NodePrincipal());

            rv = channel->AsyncOpen(listener, nullptr);
        }

        if (NS_FAILED(rv)) {
            // Abandon this prototype
            mCurrentPrototype = nullptr;

            // The parser won't get an OnStartRequest and
            // OnStopRequest, so it needs a Terminate.
            parser->Terminate();

            // Just move on to the next overlay.  NS_OpenURI could fail
            // just because a channel could not be opened, which can happen
            // if a file or chrome package does not exist.
            ReportMissingOverlay(aURI);
            
            // XXX the error could indicate an internal error as well...
            *aFailureFromContent = true;
            return rv;
        }

        // If it's a 'chrome:' prototype document, then put it into
        // the prototype cache; other XUL documents will be reloaded
        // each time.  We must do this after NS_OpenURI and AsyncOpen,
        // or chrome code will wrongly create a cached chrome channel
        // instead of a real one. Prototypes are only cached when the
        // document to be overlayed is chrome to avoid caching overlay
        // scripts with incorrect principals, see bug 565610.
        if (useXULCache && overlayIsChrome && documentIsChrome) {
            nsXULPrototypeCache::GetInstance()->PutPrototype(mCurrentPrototype);
        }

        // Return to the main event loop and eagerly await the
        // overlay load's completion. When the content sink
        // completes, it will trigger an EndLoad(), which'll wind
        // us back in ResumeWalk().
        if (!aIsDynamic)
            *aShouldReturn = true;
    }
    return NS_OK;
}

static PLDHashOperator
FirePendingMergeNotification(nsIURI* aKey, nsCOMPtr<nsIObserver>& aObserver, void* aClosure)
{
    aObserver->Observe(aKey, "xul-overlay-merged", EmptyString().get());

    typedef nsInterfaceHashtable<nsURIHashKey,nsIObserver> table;
    table* observers = static_cast<table*>(aClosure);
    observers->Remove(aKey);

    return PL_DHASH_REMOVE;
}

nsresult
XULDocument::ResumeWalk()
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
    nsCOMPtr<nsIURI> overlayURI =
        mCurrentPrototype ? mCurrentPrototype->GetURI() : nullptr;

    while (1) {
        // Begin (or resume) walking the current prototype.

        while (mContextStack.Depth() > 0) {
            // Look at the top of the stack to determine what we're
            // currently working on.
            // This will always be a node already constructed and
            // inserted to the actual document.
            nsXULPrototypeElement* proto;
            nsCOMPtr<nsIContent> element;
            int32_t indx; // all children of proto before indx (not
                          // inclusive) have already been constructed
            rv = mContextStack.Peek(&proto, getter_AddRefs(element), &indx);
            if (NS_FAILED(rv)) return rv;

            if (indx >= (int32_t)proto->mChildren.Length()) {
                if (element) {
                    // We've processed all of the prototype's children. If
                    // we're in the master prototype, do post-order
                    // document-level hookup. (An overlay will get its
                    // document hookup done when it's successfully
                    // resolved.)
                    if (mState == eState_Master) {
                        AddElementToDocumentPost(element->AsElement());

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
                            bool willNotify;
                            bool isAlternate;
                            ssle->UpdateStyleSheet(nullptr, &willNotify,
                                                   &isAlternate);
                        }
                    }
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
            bool processingOverlayHookupNodes = (mState == eState_Overlay) && 
                                                  (mContextStack.Depth() == 1);

            NS_ASSERTION(element || processingOverlayHookupNodes,
                         "no element on context stack");

            switch (childproto->mType) {
            case nsXULPrototypeNode::eType_Element: {
                // An 'element', which may contain more content.
                nsXULPrototypeElement* protoele =
                    static_cast<nsXULPrototypeElement*>(childproto);

                nsRefPtr<Element> child;

                if (!processingOverlayHookupNodes) {
                    rv = CreateElementFromPrototype(protoele,
                                                    getter_AddRefs(child),
                                                    false);
                    if (NS_FAILED(rv)) return rv;

                    // ...and append it to the content model.
                    rv = element->AppendChildTo(child, false);
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
                if (protoele->mChildren.Length() > 0) {
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
                }
            }
            break;

            case nsXULPrototypeNode::eType_Script: {
                // A script reference. Execute the script immediately;
                // this may have side effects in the content model.
                nsXULPrototypeScript* scriptproto =
                    static_cast<nsXULPrototypeScript*>(childproto);

                if (scriptproto->mSrcURI) {
                    // A transcluded script reference; this may
                    // "block" our prototype walk if the script isn't
                    // cached, or the cached copy of the script is
                    // stale and must be reloaded.
                    bool blocked;
                    rv = LoadScript(scriptproto, &blocked);
                    // If the script cannot be loaded, just keep going!

                    if (NS_SUCCEEDED(rv) && blocked)
                        return NS_OK;
                }
                else if (scriptproto->GetScriptObject()) {
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

                    nsRefPtr<nsTextNode> text =
                        new nsTextNode(mNodeInfoManager);

                    nsXULPrototypeText* textproto =
                        static_cast<nsXULPrototypeText*>(childproto);
                    text->SetText(textproto->mValue, false);

                    rv = element->AppendChildTo(text, false);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }
            break;

            case nsXULPrototypeNode::eType_PI: {
                nsXULPrototypePI* piProto =
                    static_cast<nsXULPrototypePI*>(childproto);

                // <?xul-overlay?> and <?xml-stylesheet?> don't have effect
                // outside the prolog, like they used to. Issue a warning.

                if (piProto->mTarget.EqualsLiteral("xml-stylesheet") ||
                    piProto->mTarget.EqualsLiteral("xul-overlay")) {

                    const PRUnichar* params[] = { piProto->mTarget.get() };

                    nsContentUtils::ReportToConsole(
                                        nsIScriptError::warningFlag,
                                        "XUL Document", nullptr,
                                        nsContentUtils::eXUL_PROPERTIES,
                                        "PINotInProlog",
                                        params, ArrayLength(params),
                                        overlayURI);
                }

                nsIContent* parent = processingOverlayHookupNodes ?
                    GetRootElement() : element.get();

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
        uint32_t count = mUnloadedOverlays.Length();
        if (! count)
            break;

        nsCOMPtr<nsIURI> uri = mUnloadedOverlays[count-1];
        mUnloadedOverlays.RemoveElementAt(count - 1);

        bool shouldReturn, failureFromContent;
        rv = LoadOverlayInternal(uri, false, &shouldReturn,
                                 &failureFromContent);
        if (failureFromContent)
            // The failure |rv| was the result of a problem in the content
            // rather than an unexpected problem in our implementation, so
            // just continue with the next overlay.
            continue;
        if (NS_FAILED(rv))
            return rv;
        if (mOverlayLoadObservers.IsInitialized()) {
            nsIObserver *obs = mOverlayLoadObservers.GetWeak(overlayURI);
            if (obs) {
                // This overlay has an unloaded overlay, so it will never
                // notify. The best we can do is to notify for the unloaded
                // overlay instead, assuming nobody is already notifiable
                // for it. Note that this will confuse the observer.
                if (!mOverlayLoadObservers.GetWeak(uri))
                    mOverlayLoadObservers.Put(uri, obs);
                mOverlayLoadObservers.Remove(overlayURI);
            }
        }
        if (shouldReturn)
            return NS_OK;
        overlayURI.swap(uri);
    }

    // If we get here, there is nothing left for us to walk. The content
    // model is built and ready for layout.
    rv = ResolveForwardReferences();
    if (NS_FAILED(rv)) return rv;

    ApplyPersistentAttributes();

    mStillWalking = false;
    if (mPendingSheets == 0) {
        rv = DoneWalking();
    }
    return rv;
}

nsresult
XULDocument::DoneWalking()
{
    NS_PRECONDITION(mPendingSheets == 0, "there are sheets to be loaded");
    NS_PRECONDITION(!mStillWalking, "walk not done");

    // XXXldb This is where we should really be setting the chromehidden
    // attribute.

    uint32_t count = mOverlaySheets.Length();
    for (uint32_t i = 0; i < count; ++i) {
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
        mDocumentLoaded = true;

        NotifyPossibleTitleChange(false);

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

        NS_ASSERTION(mDelayFrameLoaderInitialization,
                     "mDelayFrameLoaderInitialization should be true!");
        mDelayFrameLoaderInitialization = false;
        NS_WARN_IF_FALSE(mUpdateNestLevel == 0,
                         "Constructing XUL document in middle of an update?");
        if (mUpdateNestLevel == 0) {
            MaybeInitializeFinalizeFrameLoaders();
        }

        NS_DOCUMENT_NOTIFY_OBSERVERS(EndLoad, (this));

        // DispatchContentLoadedEvents undoes the onload-blocking we
        // did in PrepareToWalk().
        DispatchContentLoadedEvents();

        mInitialLayoutComplete = true;

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

                if (!mPendingOverlayLoadNotifications.IsInitialized()) {
                    mPendingOverlayLoadNotifications.Init();
                }
                
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
XULDocument::StyleSheetLoaded(nsCSSStyleSheet* aSheet,
                              bool aWasAlternate,
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
XULDocument::MaybeBroadcast()
{
    // Only broadcast when not in an update and when safe to run scripts.
    if (mUpdateNestLevel == 0 &&
        (mDelayedAttrChangeBroadcasts.Length() ||
         mDelayedBroadcasters.Length())) {
        if (!nsContentUtils::IsSafeToRunScript()) {
            if (!mInDestructor) {
                nsContentUtils::AddScriptRunner(
                    NS_NewRunnableMethod(this, &XULDocument::MaybeBroadcast));
            }
            return;
        }
        if (!mHandlingDelayedAttrChange) {
            mHandlingDelayedAttrChange = true;
            for (uint32_t i = 0; i < mDelayedAttrChangeBroadcasts.Length(); ++i) {
                nsIAtom* attrName = mDelayedAttrChangeBroadcasts[i].mAttrName;
                if (mDelayedAttrChangeBroadcasts[i].mNeedsAttrChange) {
                    nsCOMPtr<nsIContent> listener =
                        do_QueryInterface(mDelayedAttrChangeBroadcasts[i].mListener);
                    nsString value = mDelayedAttrChangeBroadcasts[i].mAttr;
                    if (mDelayedAttrChangeBroadcasts[i].mSetAttr) {
                        listener->SetAttr(kNameSpaceID_None, attrName, value,
                                          true);
                    } else {
                        listener->UnsetAttr(kNameSpaceID_None, attrName,
                                            true);
                    }
                }
                ExecuteOnBroadcastHandlerFor(mDelayedAttrChangeBroadcasts[i].mBroadcaster,
                                             mDelayedAttrChangeBroadcasts[i].mListener,
                                             attrName);
            }
            mDelayedAttrChangeBroadcasts.Clear();
            mHandlingDelayedAttrChange = false;
        }

        uint32_t length = mDelayedBroadcasters.Length();
        if (length) {
            bool oldValue = mHandlingDelayedBroadcasters;
            mHandlingDelayedBroadcasters = true;
            nsTArray<nsDelayedBroadcastUpdate> delayedBroadcasters;
            mDelayedBroadcasters.SwapElements(delayedBroadcasters);
            for (uint32_t i = 0; i < length; ++i) {
                SynchronizeBroadcastListener(delayedBroadcasters[i].mBroadcaster,
                                             delayedBroadcasters[i].mListener,
                                             delayedBroadcasters[i].mAttr);
            }
            mHandlingDelayedBroadcasters = oldValue;
        }
    }
}

void
XULDocument::EndUpdate(nsUpdateType aUpdateType)
{
    XMLDocument::EndUpdate(aUpdateType);

    MaybeBroadcast();
}

void
XULDocument::ReportMissingOverlay(nsIURI* aURI)
{
    NS_PRECONDITION(aURI, "Must have a URI");
    
    nsAutoCString spec;
    aURI->GetSpec(spec);

    NS_ConvertUTF8toUTF16 utfSpec(spec);
    const PRUnichar* params[] = { utfSpec.get() };
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    "XUL Document", this,
                                    nsContentUtils::eXUL_PROPERTIES,
                                    "MissingOverlay",
                                    params, ArrayLength(params));
}

nsresult
XULDocument::LoadScript(nsXULPrototypeScript* aScriptProto, bool* aBlock)
{
    // Load a transcluded script
    nsresult rv;

    bool isChromeDoc = IsChromeURI(mDocumentURI);

    if (isChromeDoc && aScriptProto->GetScriptObject()) {
        rv = ExecuteScript(aScriptProto);

        // Ignore return value from execution, and don't block
        *aBlock = false;
        return NS_OK;
    }

    // Try the XUL script cache, in case two XUL documents source the same
    // .js file (e.g., strres.js from navigator.xul and utilityOverlay.xul).
    // XXXbe the cache relies on aScriptProto's GC root!
    bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();

    if (isChromeDoc && useXULCache) {
        JSScript* newScriptObject =
            nsXULPrototypeCache::GetInstance()->GetScript(
                                   aScriptProto->mSrcURI);
        if (newScriptObject) {
            // The script language for a proto must remain constant - we
            // can't just change it for this unexpected language.
            aScriptProto->Set(newScriptObject);
        }

        if (aScriptProto->GetScriptObject()) {
            rv = ExecuteScript(aScriptProto);

            // Ignore return value from execution, and don't block
            *aBlock = false;
            return NS_OK;
        }
    }

    // Allow security manager and content policies to veto the load. Note that
    // at this point we already lost context information of the script.
    rv = nsScriptLoader::ShouldLoadScript(
                            this,
                            static_cast<nsIDocument*>(this),
                            aScriptProto->mSrcURI,
                            NS_LITERAL_STRING("application/x-javascript"));
    if (NS_FAILED(rv)) {
      *aBlock = false;
      return rv;
    }

    // Release script objects from FastLoad since we decided against using them
    aScriptProto->UnlinkJSObjects();

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
                                this, nullptr, group);
        if (NS_FAILED(rv)) {
            mCurrentScriptProto = nullptr;
            return rv;
        }

        aScriptProto->mSrcLoading = true;
    }

    // Block until OnStreamComplete resumes us.
    *aBlock = true;
    return NS_OK;
}

NS_IMETHODIMP
XULDocument::OnStreamComplete(nsIStreamLoader* aLoader,
                              nsISupports* context,
                              nsresult aStatus,
                              uint32_t stringLen,
                              const uint8_t* string)
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
                nsAutoCString uriSpec;
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
    if (!mCurrentScriptProto) {
        // XXX Wallpaper for bug 270042
        return NS_OK;
    }

    if (NS_SUCCEEDED(aStatus)) {
        // If the including XUL document is a FastLoad document, and we're
        // compiling an out-of-line script (one with src=...), then we must
        // be writing a new FastLoad file.  If we were reading this script
        // from the FastLoad file, XULContentSinkImpl::OpenScript (over in
        // nsXULContentSink.cpp) would have already deserialized a non-null
        // script->mScriptObject, causing control flow at the top of LoadScript
        // not to reach here.
        nsCOMPtr<nsIURI> uri = mCurrentScriptProto->mSrcURI;

        // XXX should also check nsIHttpChannel::requestSucceeded

        MOZ_ASSERT(!mOffThreadCompiling && mOffThreadCompileString.Length() == 0,
                   "XULDocument can't load multiple scripts at once");

        rv = nsScriptLoader::ConvertToUTF16(channel, string, stringLen,
                                            EmptyString(), this, mOffThreadCompileString);
        if (NS_SUCCEEDED(rv)) {
            rv = mCurrentScriptProto->Compile(mOffThreadCompileString.get(),
                                              mOffThreadCompileString.Length(),
                                              uri, 1, this,
                                              mCurrentPrototype,
                                              this);
            if (NS_SUCCEEDED(rv) && !mCurrentScriptProto->GetScriptObject()) {
                // We will be notified via OnOffThreadCompileComplete when the
                // compile finishes. Keep the contents of the compiled script
                // alive until the compilation finishes.
                mOffThreadCompiling = true;
                BlockOnload();
                return NS_OK;
            }
            mOffThreadCompileString.Truncate();
        }
    }

    return OnScriptCompileComplete(mCurrentScriptProto->GetScriptObject(), rv);
}

NS_IMETHODIMP
XULDocument::OnScriptCompileComplete(JSScript* aScript, nsresult aStatus)
{
    // Allow load events to be fired once off thread compilation finishes.
    if (mOffThreadCompiling) {
        mOffThreadCompiling = false;
        UnblockOnload(false);
    }

    // After compilation finishes the script's characters are no longer needed.
    mOffThreadCompileString.Truncate();

    // When compiling off thread the script will not have been attached to the
    // script proto yet.
    if (aScript && !mCurrentScriptProto->GetScriptObject())
        mCurrentScriptProto->Set(aScript);

    // Clear mCurrentScriptProto now, but save it first for use below in
    // the execute code, and in the while loop that resumes walks of other
    // documents that raced to load this script.
    nsXULPrototypeScript* scriptProto = mCurrentScriptProto;
    mCurrentScriptProto = nullptr;

    // Clear the prototype's loading flag before executing the script or
    // resuming document walks, in case any of those control flows starts a
    // new script load.
    scriptProto->mSrcLoading = false;

    nsresult rv = aStatus;
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
        // is released when its owning XULDocument is unloaded.
        //
        // (See http://bugzilla.mozilla.org/show_bug.cgi?id=98207 for
        // the true crime story.)
        bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();
  
        if (useXULCache && IsChromeURI(mDocumentURI) && scriptProto->GetScriptObject()) {
            nsXULPrototypeCache::GetInstance()->PutScript(
                               scriptProto->mSrcURI,
                               scriptProto->GetScriptObject());
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
            scriptProto->SerializeOutOfLine(nullptr, mCurrentPrototype);
        }
        // ignore any evaluation errors
    }

    rv = ResumeWalk();

    // Load a pointer to the prototype-script's list of XULDocuments who
    // raced to load the same script
    XULDocument** docp = &scriptProto->mSrcLoadWaiters;

    // Resume walking other documents that waited for this one's load, first
    // executing the script we just compiled, in each doc's script context
    XULDocument* doc;
    while ((doc = *docp) != nullptr) {
        NS_ASSERTION(doc->mCurrentScriptProto == scriptProto,
                     "waiting for wrong script to load?");
        doc->mCurrentScriptProto = nullptr;

        // Unlink doc from scriptProto's list before executing and resuming
        *docp = doc->mNextSrcLoadWaiter;
        doc->mNextSrcLoadWaiter = nullptr;

        // Execute only if we loaded and compiled successfully, then resume
        if (NS_SUCCEEDED(aStatus) && scriptProto->GetScriptObject()) {
            doc->ExecuteScript(scriptProto);
        }
        doc->ResumeWalk();
        NS_RELEASE(doc);
    }

    return rv;
}

nsresult
XULDocument::ExecuteScript(nsIScriptContext * aContext,
                           JS::Handle<JSScript*> aScriptObject)
{
    NS_PRECONDITION(aScriptObject != nullptr && aContext != nullptr, "null ptr");
    if (! aScriptObject || ! aContext)
        return NS_ERROR_NULL_POINTER;

    NS_ENSURE_TRUE(mScriptGlobalObject, NS_ERROR_NOT_INITIALIZED);

    if (!aContext->GetScriptsEnabled())
        return NS_OK;

    // Execute the precompiled script with the given version
    nsAutoMicroTask mt;
    JSContext *cx = aContext->GetNativeContext();
    AutoCxPusher pusher(cx);
    JSObject* global = mScriptGlobalObject->GetGlobalJSObject();
    xpc_UnmarkGrayObject(global);
    xpc_UnmarkGrayScript(aScriptObject);
    JSAutoCompartment ac(cx, global);
    JS::Rooted<JS::Value> unused(cx);
    if (!JS_ExecuteScript(cx, global, aScriptObject, unused.address()))
        nsJSUtils::ReportPendingException(cx);
    return NS_OK;
}

nsresult
XULDocument::ExecuteScript(nsXULPrototypeScript *aScript)
{
    NS_PRECONDITION(aScript != nullptr, "null ptr");
    NS_ENSURE_TRUE(aScript, NS_ERROR_NULL_POINTER);
    NS_ENSURE_TRUE(mScriptGlobalObject, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    rv = mScriptGlobalObject->EnsureScriptEnvironment();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptContext> context =
      mScriptGlobalObject->GetScriptContext();
    // failure getting a script context is fatal.
    NS_ENSURE_TRUE(context != nullptr, NS_ERROR_UNEXPECTED);

    if (aScript->GetScriptObject())
        rv = ExecuteScript(context, aScript->GetScriptObject());
    else
        rv = NS_ERROR_UNEXPECTED;
    return rv;
}


nsresult
XULDocument::CreateElementFromPrototype(nsXULPrototypeElement* aPrototype,
                                        Element** aResult,
                                        bool aIsRoot)
{
    // Create a content model element from a prototype element.
    NS_PRECONDITION(aPrototype != nullptr, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    *aResult = nullptr;
    nsresult rv = NS_OK;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_NOTICE)) {
        PR_LOG(gXULLog, PR_LOG_NOTICE,
               ("xul: creating <%s> from prototype",
                NS_ConvertUTF16toUTF8(aPrototype->mNodeInfo->QualifiedName()).get()));
    }
#endif

    nsRefPtr<Element> result;

    if (aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        // If it's a XUL element, it'll be lightweight until somebody
        // monkeys with it.
        rv = nsXULElement::Create(aPrototype, this, true, aIsRoot, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // If it's not a XUL element, it's gonna be heavyweight no matter
        // what. So we need to copy everything out of the prototype
        // into the element.  Get a nodeinfo from our nodeinfo manager
        // for this node.
        nsCOMPtr<nsINodeInfo> newNodeInfo;
        newNodeInfo = mNodeInfoManager->GetNodeInfo(aPrototype->mNodeInfo->NameAtom(),
                                                    aPrototype->mNodeInfo->GetPrefixAtom(),
                                                    aPrototype->mNodeInfo->NamespaceID(),
                                                    nsIDOMNode::ELEMENT_NODE);
        if (!newNodeInfo) return NS_ERROR_OUT_OF_MEMORY;
        nsCOMPtr<nsIContent> content;
        nsCOMPtr<nsINodeInfo> xtfNi = newNodeInfo;
        rv = NS_NewElement(getter_AddRefs(content), newNodeInfo.forget(),
                           NOT_FROM_PARSER);
        if (NS_FAILED(rv))
            return rv;

        result = content->AsElement();

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;
    }

    result.swap(*aResult);

    return NS_OK;
}

nsresult
XULDocument::CreateOverlayElement(nsXULPrototypeElement* aPrototype,
                                  Element** aResult)
{
    nsresult rv;

    nsRefPtr<Element> element;
    rv = CreateElementFromPrototype(aPrototype, getter_AddRefs(element), false);
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
XULDocument::AddAttributes(nsXULPrototypeElement* aPrototype,
                           nsIContent* aElement)
{
    nsresult rv;

    for (uint32_t i = 0; i < aPrototype->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &(aPrototype->mAttributes[i]);
        nsAutoString  valueStr;
        protoattr->mValue.ToString(valueStr);

        rv = aElement->SetAttr(protoattr->mName.NamespaceID(),
                               protoattr->mName.LocalName(),
                               protoattr->mName.GetPrefix(),
                               valueStr,
                               false);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
XULDocument::CheckTemplateBuilderHookup(nsIContent* aElement,
                                        bool* aNeedsHookup)
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
            *aNeedsHookup = false;
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
XULDocument::CreateTemplateBuilder(nsIContent* aElement)
{
    // Check if need to construct a tree builder or content builder.
    bool isTreeBuilder = false;

    // return successful if the element is not is a document, as an inline
    // script could have removed it
    nsIDocument *document = aElement->GetCurrentDoc();
    NS_ENSURE_TRUE(document, NS_OK);

    int32_t nameSpaceID;
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
            isTreeBuilder = true;
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
            nsresult rv =
                document->CreateElem(nsDependentAtomString(nsGkAtoms::treechildren),
                                     nullptr, kNameSpaceID_XUL,
                                     getter_AddRefs(bodyContent));
            NS_ENSURE_SUCCESS(rv, rv);

            aElement->AppendChildTo(bodyContent, false);
        }
    }
    else {
        // Create and initialize a content builder.
        nsCOMPtr<nsIXULTemplateBuilder> builder
            = do_CreateInstance("@mozilla.org/xul/xul-template-builder;1");

        if (! builder)
            return NS_ERROR_FAILURE;

        builder->Init(aElement);
        builder->CreateContents(aElement, false);
    }

    return NS_OK;
}


nsresult
XULDocument::AddPrototypeSheets()
{
    nsresult rv;

    const nsCOMArray<nsIURI>& sheets = mCurrentPrototype->GetStyleSheetReferences();

    for (int32_t i = 0; i < sheets.Count(); i++) {
        nsCOMPtr<nsIURI> uri = sheets[i];

        nsRefPtr<nsCSSStyleSheet> incompleteSheet;
        rv = CSSLoader()->LoadSheet(uri,
                                    mCurrentPrototype->DocumentPrincipal(),
                                    EmptyCString(), this,
                                    getter_AddRefs(incompleteSheet));

        // XXXldb We need to prevent bogus sheets from being held in the
        // prototype's list, but until then, don't propagate the failure
        // from LoadSheet (and thus exit the loop).
        if (NS_SUCCEEDED(rv)) {
            ++mPendingSheets;
            if (!mOverlaySheets.AppendElement(incompleteSheet)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// XULDocument::OverlayForwardReference
//

nsForwardReference::Result
XULDocument::OverlayForwardReference::Resolve()
{
    // Resolve a forward reference from an overlay element; attempt to
    // hook it up into the main document.
    nsresult rv;
    nsCOMPtr<nsIContent> target;

    nsIPresShell *shell = mDocument->GetShell();
    bool notify = shell && shell->DidInitialize();

    nsAutoString id;
    mOverlay->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
    if (id.IsEmpty()) {
        // mOverlay is a direct child of <overlay> and has no id.
        // Insert it under the root element in the base document.
        Element* root = mDocument->GetRootElement();
        if (!root) {
            return eResolve_Error;
        }

        rv = mDocument->InsertElement(root, mOverlay, notify);
        if (NS_FAILED(rv)) return eResolve_Error;

        target = mOverlay;
    }
    else {
        // The hook-up element has an id, try to match it with an element
        // with the same id in the base document.
        target = mDocument->GetElementById(id);

        // If we can't find the element in the document, defer the hookup
        // until later.
        if (!target)
            return eResolve_Later;

        rv = Merge(target, mOverlay, notify);
        if (NS_FAILED(rv)) return eResolve_Error;
    }

    // Check if 'target' is still in our document --- it might not be!
    if (!notify && target->GetCurrentDoc() == mDocument) {
        // Add child and any descendants to the element map
        // XXX this is bogus, the content in 'target' might already be
        // in the document
        rv = mDocument->AddSubtreeToDocument(target);
        if (NS_FAILED(rv)) return eResolve_Error;
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_NOTICE)) {
        nsAutoCString idC;
        idC.AssignWithConversion(id);
        PR_LOG(gXULLog, PR_LOG_NOTICE,
               ("xul: overlay resolved '%s'",
                idC.get()));
    }
#endif

    mResolved = true;
    return eResolve_Succeeded;
}



nsresult
XULDocument::OverlayForwardReference::Merge(nsIContent* aTargetNode,
                                            nsIContent* aOverlayNode,
                                            bool aNotify)
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
    uint32_t i;
    const nsAttrName* name;
    for (i = 0; (name = aOverlayNode->GetAttrNameAt(i)); ++i) {
        // We don't want to swap IDs, they should be the same.
        if (name->Equals(nsGkAtoms::id))
            continue;

        // In certain cases merging command or observes is unsafe, so don't.
        if (!aNotify) {
            if (aTargetNode->NodeInfo()->Equals(nsGkAtoms::observes,
                                                kNameSpaceID_XUL))
                continue;

            if (name->Equals(nsGkAtoms::observes) &&
                aTargetNode->HasAttr(kNameSpaceID_None, nsGkAtoms::observes))
                continue;

            if (name->Equals(nsGkAtoms::command) &&
                aTargetNode->HasAttr(kNameSpaceID_None, nsGkAtoms::command) &&
                !aTargetNode->NodeInfo()->Equals(nsGkAtoms::key,
                                                 kNameSpaceID_XUL) &&
                !aTargetNode->NodeInfo()->Equals(nsGkAtoms::menuitem,
                                                 kNameSpaceID_XUL))
                continue;
        }

        int32_t nameSpaceID = name->NamespaceID();
        nsIAtom* attr = name->LocalName();
        nsIAtom* prefix = name->GetPrefix();

        nsAutoString value;
        aOverlayNode->GetAttr(nameSpaceID, attr, value);

        // Element in the overlay has the 'removeelement' attribute set
        // so remove it from the actual document.
        if (attr == nsGkAtoms::removeelement &&
            value.EqualsLiteral("true")) {

            nsCOMPtr<nsIContent> parent = aTargetNode->GetParent();
            rv = RemoveElement(parent, aTargetNode);
            if (NS_FAILED(rv)) return rv;

            return NS_OK;
        }

        rv = aTargetNode->SetAttr(nameSpaceID, attr, prefix, value, aNotify);
        if (!NS_FAILED(rv) && !aNotify)
            rv = mDocument->BroadcastAttributeChangeFromOverlay(aTargetNode,
                                                                nameSpaceID,
                                                                attr, prefix,
                                                                value);
        if (NS_FAILED(rv)) return rv;
    }


    // Walk our child nodes, looking for elements that have the 'id'
    // attribute set. If we find any, we must do a parent check in the
    // actual document to ensure that the structure matches that of
    // the actual document. If it does, we can call ourselves and attempt
    // to merge inside that subtree. If not, we just append the tree to
    // the parent like any other.

    uint32_t childCount = aOverlayNode->GetChildCount();

    // This must be a strong reference since it will be the only
    // reference to a content object during part of this loop.
    nsCOMPtr<nsIContent> currContent;

    for (i = 0; i < childCount; ++i) {
        currContent = aOverlayNode->GetFirstChild();

        nsIAtom *idAtom = currContent->GetID();

        nsIContent *elementInDocument = nullptr;
        if (idAtom) {
            nsDependentAtomString id(idAtom);

            if (!id.IsEmpty()) {
                nsIDocument *doc = aTargetNode->GetDocument();
                if (!doc) return NS_ERROR_FAILURE;

                elementInDocument = doc->GetElementById(id);
            }
        }

        // The item has an 'id' attribute set, and we need to check with
        // the actual document to see if an item with this id exists at
        // this locale. If so, we want to merge the subtree under that
        // node. Otherwise, we just do an append as if the element had
        // no id attribute.
        if (elementInDocument) {
            // Given two parents, aTargetNode and aOverlayNode, we want
            // to call merge on currContent if we find an associated
            // node in the document with the same id as currContent that
            // also has aTargetNode as its parent.

            nsIContent *elementParent = elementInDocument->GetParent();

            nsIAtom *parentID = elementParent->GetID();
            if (parentID &&
                aTargetNode->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
                                         nsDependentAtomString(parentID),
                                         eCaseMatters)) {
                // The element matches. "Go Deep!"
                rv = Merge(elementInDocument, currContent, aNotify);
                if (NS_FAILED(rv)) return rv;
                aOverlayNode->RemoveChildAt(0, false);

                continue;
            }
        }

        aOverlayNode->RemoveChildAt(0, false);

        rv = InsertElement(aTargetNode, currContent, aNotify);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}



XULDocument::OverlayForwardReference::~OverlayForwardReference()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING) && !mResolved) {
        nsAutoString id;
        mOverlay->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);

        nsAutoCString idC;
        idC.AssignWithConversion(id);

        nsIURI *protoURI = mDocument->mCurrentPrototype->GetURI();
        nsAutoCString urlspec;
        protoURI->GetSpec(urlspec);

        nsCOMPtr<nsIURI> docURI;
        nsAutoCString parentDoc;
        nsresult rv = mDocument->mChannel->GetOriginalURI(getter_AddRefs(docURI));
        if (NS_SUCCEEDED(rv))
            docURI->GetSpec(parentDoc);
        PR_LOG(gXULLog, PR_LOG_WARNING,
               ("xul: %s overlay failed to resolve '%s' in %s",
                urlspec.get(), idC.get(), parentDoc.get()));
    }
#endif
}


//----------------------------------------------------------------------
//
// XULDocument::BroadcasterHookup
//

nsForwardReference::Result
XULDocument::BroadcasterHookup::Resolve()
{
    nsresult rv;

    bool listener;
    rv = mDocument->CheckBroadcasterHookup(mObservesElement, &listener, &mResolved);
    if (NS_FAILED(rv)) return eResolve_Error;

    return mResolved ? eResolve_Succeeded : eResolve_Later;
}


XULDocument::BroadcasterHookup::~BroadcasterHookup()
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULLog, PR_LOG_WARNING) && !mResolved) {
        // Tell the world we failed
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

        nsAutoCString attributeC,broadcasteridC;
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_WARNING,
               ("xul: broadcaster hookup failed <%s attribute='%s'> to %s",
                nsAtomCString(tag).get(),
                attributeC.get(),
                broadcasteridC.get()));
    }
#endif
}


//----------------------------------------------------------------------
//
// XULDocument::TemplateBuilderHookup
//

nsForwardReference::Result
XULDocument::TemplateBuilderHookup::Resolve()
{
    bool needsHookup;
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
XULDocument::BroadcastAttributeChangeFromOverlay(nsIContent* aNode,
                                                 int32_t aNameSpaceID,
                                                 nsIAtom* aAttribute,
                                                 nsIAtom* aPrefix,
                                                 const nsAString& aValue)
{
    nsresult rv = NS_OK;

    if (!mBroadcasterMap || !CanBroadcast(aNameSpaceID, aAttribute))
        return rv;

    if (!aNode->IsElement())
        return rv;

    BroadcasterMapEntry* entry = static_cast<BroadcasterMapEntry*>
        (PL_DHashTableOperate(mBroadcasterMap, aNode->AsElement(), PL_DHASH_LOOKUP));
    if (!PL_DHASH_ENTRY_IS_BUSY(entry))
        return rv;

    // We've got listeners: push the value.
    int32_t i;
    for (i = entry->mListeners.Count() - 1; i >= 0; --i) {
        BroadcastListener* bl = static_cast<BroadcastListener*>
            (entry->mListeners[i]);

        if ((bl->mAttribute != aAttribute) &&
            (bl->mAttribute != nsGkAtoms::_asterix))
            continue;

        nsCOMPtr<nsIContent> l = do_QueryReferent(bl->mListener);
        if (l) {
            rv = l->SetAttr(aNameSpaceID, aAttribute,
                            aPrefix, aValue, false);
            if (NS_FAILED(rv)) return rv;
        }
    }
    return rv;
}

nsresult
XULDocument::FindBroadcaster(Element* aElement,
                             Element** aListener,
                             nsString& aBroadcasterID,
                             nsString& aAttribute,
                             Element** aBroadcaster)
{
    nsINodeInfo *ni = aElement->NodeInfo();
    *aListener = nullptr;
    *aBroadcaster = nullptr;

    if (ni->Equals(nsGkAtoms::observes, kNameSpaceID_XUL)) {
        // It's an <observes> element, which means that the actual
        // listener is the _parent_ node. This element should have an
        // 'element' attribute that specifies the ID of the
        // broadcaster element, and an 'attribute' element, which
        // specifies the name of the attribute to observe.
        nsIContent* parent = aElement->GetParent();
        if (!parent) {
             // <observes> is the root element
            return NS_FINDBROADCASTER_NOT_FOUND;
        }

        // If we're still parented by an 'overlay' tag, then we haven't
        // made it into the real document yet. Defer hookup.
        if (parent->NodeInfo()->Equals(nsGkAtoms::overlay,
                                       kNameSpaceID_XUL)) {
            return NS_FINDBROADCASTER_AWAIT_OVERLAYS;
        }

        *aListener = parent->IsElement() ? parent->AsElement() : nullptr;
        NS_IF_ADDREF(*aListener);

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

        *aListener = aElement;
        NS_ADDREF(*aListener);

        aAttribute.AssignLiteral("*");
    }

    // Make sure we got a valid listener.
    NS_ENSURE_TRUE(*aListener, NS_ERROR_UNEXPECTED);

    // Try to find the broadcaster element in the document.
    *aBroadcaster = GetElementById(aBroadcasterID);

    // If we can't find the broadcaster, then we'll need to defer the
    // hookup. We may need to resolve some of the other overlays
    // first.
    if (! *aBroadcaster) {
        return NS_FINDBROADCASTER_AWAIT_OVERLAYS;
    }

    NS_ADDREF(*aBroadcaster);

    return NS_FINDBROADCASTER_FOUND;
}

nsresult
XULDocument::CheckBroadcasterHookup(Element* aElement,
                                    bool* aNeedsHookup,
                                    bool* aDidResolve)
{
    // Resolve a broadcaster hookup. Look at the element that we're
    // trying to resolve: it could be an '<observes>' element, or just
    // a vanilla element with an 'observes' attribute on it.
    nsresult rv;

    *aDidResolve = false;

    nsCOMPtr<Element> listener;
    nsAutoString broadcasterID;
    nsAutoString attribute;
    nsCOMPtr<Element> broadcaster;

    rv = FindBroadcaster(aElement, getter_AddRefs(listener),
                         broadcasterID, attribute, getter_AddRefs(broadcaster));
    switch (rv) {
        case NS_FINDBROADCASTER_NOT_FOUND:
            *aNeedsHookup = false;
            return NS_OK;
        case NS_FINDBROADCASTER_AWAIT_OVERLAYS:
            *aNeedsHookup = true;
            return NS_OK;
        case NS_FINDBROADCASTER_FOUND:
            break;
        default:
            return rv;
    }

    NS_ENSURE_ARG(broadcaster && listener);
    ErrorResult domRv;
    AddBroadcastListenerFor(*broadcaster, *listener, attribute, domRv);
    if (domRv.Failed()) {
        return domRv.ErrorCode();
    }

#ifdef PR_LOGGING
    // Tell the world we succeeded
    if (PR_LOG_TEST(gXULLog, PR_LOG_NOTICE)) {
        nsCOMPtr<nsIContent> content =
            do_QueryInterface(listener);

        NS_ASSERTION(content != nullptr, "not an nsIContent");
        if (! content)
            return rv;

        nsAutoCString attributeC,broadcasteridC;
        attributeC.AssignWithConversion(attribute);
        broadcasteridC.AssignWithConversion(broadcasterID);
        PR_LOG(gXULLog, PR_LOG_NOTICE,
               ("xul: broadcaster hookup <%s attribute='%s'> to %s",
                nsAtomCString(content->Tag()).get(),
                attributeC.get(),
                broadcasteridC.get()));
    }
#endif

    *aNeedsHookup = false;
    *aDidResolve = true;
    return NS_OK;
}

nsresult
XULDocument::InsertElement(nsIContent* aParent, nsIContent* aChild,
                           bool aNotify)
{
    // Insert aChild appropriately into aParent, accounting for a
    // 'pos' attribute set on aChild.

    nsAutoString posStr;
    bool wasInserted = false;

    // insert after an element of a given id
    aChild->GetAttr(kNameSpaceID_None, nsGkAtoms::insertafter, posStr);
    bool isInsertAfter = true;

    if (posStr.IsEmpty()) {
        aChild->GetAttr(kNameSpaceID_None, nsGkAtoms::insertbefore, posStr);
        isInsertAfter = false;
    }

    if (!posStr.IsEmpty()) {
        nsIDocument *document = aParent->OwnerDoc();

        nsIContent *content = nullptr;

        char* str = ToNewCString(posStr);
        char* rest;
        char* token = nsCRT::strtok(str, ", ", &rest);

        while (token) {
            content = document->GetElementById(NS_ConvertASCIItoUTF16(token));
            if (content)
                break;

            token = nsCRT::strtok(rest, ", ", &rest);
        }
        nsMemory::Free(str);

        if (content) {
            int32_t pos = aParent->IndexOf(content);

            if (pos != -1) {
                pos = isInsertAfter ? pos + 1 : pos;
                nsresult rv = aParent->InsertChildAt(aChild, pos, aNotify);
                if (NS_FAILED(rv))
                    return rv;

                wasInserted = true;
            }
        }
    }

    if (!wasInserted) {

        aChild->GetAttr(kNameSpaceID_None, nsGkAtoms::position, posStr);
        if (!posStr.IsEmpty()) {
            nsresult rv;
            // Positions are one-indexed.
            int32_t pos = posStr.ToInteger(&rv);
            // Note: if the insertion index (which is |pos - 1|) would be less
            // than 0 or greater than the number of children aParent has, then
            // don't insert, since the position is bogus.  Just skip on to
            // appending.
            if (NS_SUCCEEDED(rv) && pos > 0 &&
                uint32_t(pos - 1) <= aParent->GetChildCount()) {
                rv = aParent->InsertChildAt(aChild, pos - 1, aNotify);
                if (NS_SUCCEEDED(rv))
                    wasInserted = true;
                // If the insertion fails, then we should still
                // attempt an append.  Thus, rather than returning rv
                // immediately, we fall through to the final
                // "catch-all" case that just does an AppendChildTo.
            }
        }
    }

    if (!wasInserted) {
        return aParent->AppendChildTo(aChild, aNotify);
    }
    return NS_OK;
}

nsresult
XULDocument::RemoveElement(nsIContent* aParent, nsIContent* aChild)
{
    int32_t nodeOffset = aParent->IndexOf(aChild);

    aParent->RemoveChildAt(nodeOffset, true);
    return NS_OK;
}

//----------------------------------------------------------------------
//
// CachedChromeStreamListener
//

XULDocument::CachedChromeStreamListener::CachedChromeStreamListener(XULDocument* aDocument, bool aProtoLoaded)
    : mDocument(aDocument),
      mProtoLoaded(aProtoLoaded)
{
    NS_ADDREF(mDocument);
}


XULDocument::CachedChromeStreamListener::~CachedChromeStreamListener()
{
    NS_RELEASE(mDocument);
}


NS_IMPL_ISUPPORTS2(XULDocument::CachedChromeStreamListener,
                   nsIRequestObserver, nsIStreamListener)

NS_IMETHODIMP
XULDocument::CachedChromeStreamListener::OnStartRequest(nsIRequest *request,
                                                        nsISupports* acontext)
{
    return NS_ERROR_PARSED_DATA_CACHED;
}


NS_IMETHODIMP
XULDocument::CachedChromeStreamListener::OnStopRequest(nsIRequest *request,
                                                       nsISupports* aContext,
                                                       nsresult aStatus)
{
    if (! mProtoLoaded)
        return NS_OK;

    return mDocument->OnPrototypeLoadDone(true);
}


NS_IMETHODIMP
XULDocument::CachedChromeStreamListener::OnDataAvailable(nsIRequest *request,
                                                         nsISupports* aContext,
                                                         nsIInputStream* aInStr,
                                                         uint64_t aSourceOffset,
                                                         uint32_t aCount)
{
    NS_NOTREACHED("CachedChromeStream doesn't receive data");
    return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
//
// ParserObserver
//

XULDocument::ParserObserver::ParserObserver(XULDocument* aDocument,
                                            nsXULPrototypeDocument* aPrototype)
    : mDocument(aDocument), mPrototype(aPrototype)
{
}

XULDocument::ParserObserver::~ParserObserver()
{
}

NS_IMPL_ISUPPORTS1(XULDocument::ParserObserver, nsIRequestObserver)

NS_IMETHODIMP
XULDocument::ParserObserver::OnStartRequest(nsIRequest *request,
                                            nsISupports* aContext)
{
    // Guard against buggy channels calling OnStartRequest multiple times.
    if (mPrototype) {
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
        if (channel && secMan) {
            nsCOMPtr<nsIPrincipal> principal;
            secMan->GetChannelPrincipal(channel, getter_AddRefs(principal));

            // Failure there is ok -- it'll just set a (safe) null principal
            mPrototype->SetDocumentPrincipal(principal);
        }

        // Make sure to avoid cycles
        mPrototype = nullptr;
    }
        
    return NS_OK;
}

NS_IMETHODIMP
XULDocument::ParserObserver::OnStopRequest(nsIRequest *request,
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
    mDocument = nullptr;

    return rv;
}

already_AddRefed<nsPIWindowRoot>
XULDocument::GetWindowRoot()
{
    nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryReferent(mDocumentContainer);
    nsCOMPtr<nsIDOMWindow> window(do_GetInterface(ir));
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(window));
    return piWin ? piWin->GetTopWindowRoot() : nullptr;
}

bool
XULDocument::IsDocumentRightToLeft()
{
    // setting the localedir attribute on the root element forces a
    // specific direction for the document.
    Element* element = GetRootElement();
    if (element) {
        static nsIContent::AttrValuesArray strings[] =
            {&nsGkAtoms::ltr, &nsGkAtoms::rtl, nullptr};
        switch (element->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::localedir,
                                         strings, eCaseMatters)) {
            case 0: return false;
            case 1: return true;
            default: break; // otherwise, not a valid value, so fall through
        }
    }

    // otherwise, get the locale from the chrome registry and
    // look up the intl.uidirection.<locale> preference
    nsCOMPtr<nsIXULChromeRegistry> reg =
        mozilla::services::GetXULChromeRegistryService();
    if (!reg)
        return false;

    nsAutoCString package;
    bool isChrome;
    if (NS_SUCCEEDED(mDocumentURI->SchemeIs("chrome", &isChrome)) &&
        isChrome) {
        mDocumentURI->GetHostPort(package);
    }
    else {
        // use the 'global' package for about and resource uris.
        // otherwise, just default to left-to-right.
        bool isAbout, isResource;
        if (NS_SUCCEEDED(mDocumentURI->SchemeIs("about", &isAbout)) &&
            isAbout) {
            package.AssignLiteral("global");
        }
        else if (NS_SUCCEEDED(mDocumentURI->SchemeIs("resource", &isResource)) &&
            isResource) {
            package.AssignLiteral("global");
        }
        else {
            return false;
        }
    }

    bool isRTL = false;
    reg->IsLocaleRTL(package, &isRTL);
    return isRTL;
}

void
XULDocument::ResetDocumentDirection()
{
    DocumentStatesChanged(NS_DOCUMENT_STATE_RTL_LOCALE);
}

int
XULDocument::DirectionChanged(const char* aPrefName, void* aData)
{
  // Reset the direction and restyle the document if necessary.
  XULDocument* doc = (XULDocument *)aData;
  if (doc) {
      doc->ResetDocumentDirection();
  }

  return 0;
}

int
XULDocument::GetDocumentLWTheme()
{
    if (mDocLWTheme == Doc_Theme_Uninitialized) {
        mDocLWTheme = Doc_Theme_None; // No lightweight theme by default

        Element* element = GetRootElement();
        nsAutoString hasLWTheme;
        if (element &&
            element->GetAttr(kNameSpaceID_None, nsGkAtoms::lwtheme, hasLWTheme) &&
            !(hasLWTheme.IsEmpty()) &&
            hasLWTheme.EqualsLiteral("true")) {
            mDocLWTheme = Doc_Theme_Neutral;
            nsAutoString lwTheme;
            element->GetAttr(kNameSpaceID_None, nsGkAtoms::lwthemetextcolor, lwTheme);
            if (!(lwTheme.IsEmpty())) {
                if (lwTheme.EqualsLiteral("dark"))
                    mDocLWTheme = Doc_Theme_Dark;
                else if (lwTheme.EqualsLiteral("bright"))
                    mDocLWTheme = Doc_Theme_Bright;
            }
        }
    }
    return mDocLWTheme;
}

NS_IMETHODIMP
XULDocument::GetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject** aResult)
{
    ErrorResult rv;
    nsCOMPtr<Element> el = do_QueryInterface(aElement);
    *aResult = GetBoxObjectFor(el, rv).get();
    return rv.ErrorCode();
}

JSObject*
XULDocument::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return XULDocumentBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
