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

  1. We do some monkey business in the document observer methods to
     keep the element map in sync for HTML elements. Why don't we just
     do it for _all_ elements? Well, in the case of XUL elements,
     which may be lazily created during frame construction, the
     document observer methods will never be called because we'll be
     adding the XUL nodes into the content model "quietly".

*/

#include "mozilla/ArrayUtils.h"

#include "XULDocument.h"

#include "nsError.h"
#include "nsIBoxObject.h"
#include "nsIChromeRegistry.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIContentViewer.h"
#include "nsIStreamListener.h"
#include "nsITimer.h"
#include "nsDocShell.h"
#include "nsGkAtoms.h"
#include "nsXMLContentSink.h"
#include "nsXULContentSink.h"
#include "nsXULContentUtils.h"
#include "nsIStringEnumerator.h"
#include "nsDocElementCreatedNotificationRunner.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsPIBoxObject.h"
#include "mozilla/dom/BoxObject.h"
#include "nsString.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsXULElement.h"
#include "nsXULPrototypeCache.h"
#include "mozilla/Logging.h"
#include "nsIFrame.h"
#include "nsXBLService.h"
#include "nsCExternalHandlerService.h"
#include "nsMimeTypes.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsContentList.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsIParser.h"
#include "nsCharsetSource.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "nsIScriptError.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIObserverService.h"
#include "nsNodeUtils.h"
#include "nsIXULWindow.h"
#include "nsXULPopupManager.h"
#include "nsCCUncollectableMarker.h"
#include "nsURILoader.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/dom/DocumentL10n.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/NodeInfoInlines.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/XULDocumentBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "nsTextNode.h"
#include "nsJSUtils.h"
#include "js/CompilationAndEvaluation.h"
#include "js/SourceBufferHolder.h"
#include "mozilla/dom/URL.h"
#include "nsIContentPolicy.h"
#include "mozAutoDocUpdate.h"
#include "xpcpublic.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "nsIConsoleService.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
//
// CIDs
//

static NS_DEFINE_CID(kParserCID,                 NS_PARSER_CID);

//----------------------------------------------------------------------
//
// Statics
//

int32_t XULDocument::gRefCnt = 0;

LazyLogModule XULDocument::gXULLog("XULDocument");

//----------------------------------------------------------------------
//
// ctors & dtors
//

namespace mozilla {
namespace dom {

XULDocument::XULDocument(void)
    : XMLDocument("application/vnd.mozilla.xul+xml"),
      mNextSrcLoadWaiter(nullptr),
      mApplyingPersistedAttrs(false),
      mIsWritingFastLoad(false),
      mDocumentLoaded(false),
      mStillWalking(false),
      mPendingSheets(0),
      mCurrentScriptProto(nullptr),
      mOffThreadCompiling(false),
      mOffThreadCompileStringBuf(nullptr),
      mOffThreadCompileStringLength(0),
      mInitialLayoutComplete(false)
{
    // Override the default in nsDocument
    mCharacterSet = UTF_8_ENCODING;

    mDefaultElementType = kNameSpaceID_XUL;
    mType = eXUL;

    mDelayFrameLoaderInitialization = true;

    mAllowXULXBL = eTriTrue;
}

XULDocument::~XULDocument()
{
    NS_ASSERTION(mNextSrcLoadWaiter == nullptr,
        "unreferenced document still waiting for script source to load?");

    Preferences::UnregisterCallback(XULDocument::DirectionChanged,
                                    "intl.uidirection", this);

    if (mOffThreadCompileStringBuf) {
      js_free(mOffThreadCompileStringBuf);
    }
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewXULDocument(nsIDocument** result)
{
    MOZ_ASSERT(result != nullptr, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RefPtr<XULDocument> doc = new XULDocument();

    nsresult rv;
    if (NS_FAILED(rv = doc->Init())) {
        return rv;
    }

    doc.forget(result);
    return NS_OK;
}


namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
//
// nsISupports interface
//

NS_IMPL_CYCLE_COLLECTION_CLASS(XULDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XULDocument, XMLDocument)
    NS_ASSERTION(!nsCCUncollectableMarker::InGeneration(cb, tmp->GetMarkedCCGeneration()),
                 "Shouldn't traverse XULDocument!");
    // XXX tmp->mContextStack?

    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentPrototype)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrototypes)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalStore)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XULDocument, XMLDocument)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalStore)
    //XXX We should probably unlink all the objects we traverse.
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(XULDocument,
                                             XMLDocument,
                                             nsIStreamLoaderObserver,
                                             nsICSSLoaderObserver,
                                             nsIOffThreadScriptReceiver)


//----------------------------------------------------------------------
//
// nsIDocument interface
//

void
XULDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
    MOZ_ASSERT_UNREACHABLE("Reset");
}

void
XULDocument::ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                        nsIPrincipal* aPrincipal)
{
    MOZ_ASSERT_UNREACHABLE("ResetToURI");
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
    if (MOZ_LOG_TEST(gXULLog, LogLevel::Warning)) {

        nsCOMPtr<nsIURI> uri;
        nsresult rv = aChannel->GetOriginalURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
            nsAutoCString urlspec;
            rv = uri->GetSpec(urlspec);
            if (NS_SUCCEEDED(rv)) {
                MOZ_LOG(gXULLog, LogLevel::Warning,
                       ("xul: load document '%s'", urlspec.get()));
            }
        }
    }
    // NOTE: If this ever starts calling nsDocument::StartDocumentLoad
    // we'll possibly need to reset our content type afterwards.
    mStillWalking = true;
    mMayStartLayout = false;
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);

    mChannel = aChannel;

    // Get the URI.  Note that this should match nsDocShell::OnLoadingSite
    nsresult rv =
        NS_GetFinalChannelURI(aChannel, getter_AddRefs(mDocumentURI));
    NS_ENSURE_SUCCESS(rv, rv);

    mOriginalURI = mDocumentURI;

    // Get the document's principal
    nsCOMPtr<nsIPrincipal> principal;
    nsContentUtils::GetSecurityManager()->
        GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
    principal = MaybeDowngradePrincipal(principal);

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

        mCurrentPrototype = proto;

        // Set up the right principal on ourselves.
        SetPrincipal(proto->DocumentPrincipal());

        // We need a listener, even if proto is not yet loaded, in which
        // event the listener's OnStopRequest method does nothing, and all
        // the interesting work happens below XULDocument::EndLoad, from
        // the call there to mCurrentPrototype->NotifyLoadDone().
        *aDocListener = new CachedChromeStreamListener(this, loaded);
    }
    else {
        bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();
        bool fillXULCache = (useXULCache && IsChromeURI(mDocumentURI));


        // It's just a vanilla document load. Create a parser to deal
        // with the stream n' stuff.

        nsCOMPtr<nsIParser> parser;
        rv = PrepareToLoadPrototype(mDocumentURI, aCommand, principal,
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

// This gets invoked after the prototype for this document is fully built in the
// content sink.
void
XULDocument::EndLoad()
{
    nsresult rv;

    // Whack the prototype document into the cache so that the next
    // time somebody asks for it, they don't need to load it by hand.

    nsCOMPtr<nsIURI> uri = mCurrentPrototype->GetURI();
    bool isChrome = IsChromeURI(uri);

    // Remember if the XUL cache is on
    bool useXULCache = nsXULPrototypeCache::GetInstance()->IsEnabled();

    if (isChrome && useXULCache) {
        // If it's a chrome prototype document, then notify any
        // documents that raced to load the prototype, and awaited
        // its load completion via proto->AwaitLoadDone().
        rv = mCurrentPrototype->NotifyLoadDone();
        if (NS_FAILED(rv)) return;
    }

    OnPrototypeLoadDone(true);
    if (MOZ_LOG_TEST(gXULLog, LogLevel::Warning)) {
        nsAutoCString urlspec;
        rv = uri->GetSpec(urlspec);
        if (NS_SUCCEEDED(rv)) {
            MOZ_LOG(gXULLog, LogLevel::Warning,
                   ("xul: Finished loading document '%s'", urlspec.get()));
        }
    }
}

nsresult
XULDocument::OnPrototypeLoadDone(bool aResumeWalk)
{
    nsresult rv;

    rv = PrepareToWalk();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to prepare for walk");
    if (NS_FAILED(rv)) return rv;

    if (aResumeWalk) {
        rv = ResumeWalk();
    }
    return rv;
}

static bool
ShouldPersistAttribute(Element* aElement, nsAtom* aAttribute)
{
    if (aElement->IsXULElement(nsGkAtoms::window)) {
        // This is not an element of the top document, its owner is
        // not an nsXULWindow. Persist it.
        if (aElement->OwnerDoc()->GetParentDocument()) {
            return true;
        }
        // The following attributes of xul:window should be handled in
        // nsXULWindow::SavePersistentAttributes instead of here.
        if (aAttribute == nsGkAtoms::screenX ||
            aAttribute == nsGkAtoms::screenY ||
            aAttribute == nsGkAtoms::width ||
            aAttribute == nsGkAtoms::height ||
            aAttribute == nsGkAtoms::sizemode) {
            return false;
        }
    }
    return true;
}

void
XULDocument::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                              nsAtom* aAttribute, int32_t aModType,
                              const nsAttrValue* aOldValue)
{
    NS_ASSERTION(aElement->OwnerDoc() == this, "unexpected doc");

    // Might not need this, but be safe for now.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    // See if there is anything we need to persist in the localstore.
    //
    // XXX Namespace handling broken :-(
    nsAutoString persist;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::persist, persist);
    // Persistence of attributes of xul:window is handled in nsXULWindow.
    if (ShouldPersistAttribute(aElement, aAttribute) && !persist.IsEmpty() &&
        // XXXldb This should check that it's a token, not just a substring.
        persist.Find(nsDependentAtomString(aAttribute)) >= 0) {
      nsContentUtils::AddScriptRunner(
        NewRunnableMethod<Element*, int32_t, nsAtom*>(
          "dom::XULDocument::Persist",
          this,
          &XULDocument::Persist,
          aElement,
          kNameSpaceID_None,
          aAttribute));
    }
}

void
XULDocument::ContentAppended(nsIContent* aFirstNewContent)
{
    NS_ASSERTION(aFirstNewContent->OwnerDoc() == this, "unexpected doc");

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
XULDocument::ContentInserted(nsIContent* aChild)
{
    NS_ASSERTION(aChild->OwnerDoc() == this, "unexpected doc");

    // Might not need this, but be safe for now.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    AddSubtreeToDocument(aChild);
}

void
XULDocument::ContentRemoved(nsIContent* aChild, nsIContent* aPreviousSibling)
{
    NS_ASSERTION(aChild->OwnerDoc() == this, "unexpected doc");

    // Might not need this, but be safe for now.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    RemoveSubtreeFromDocument(aChild);
}

//----------------------------------------------------------------------
//
// nsIDocument interface
//


void
XULDocument::Persist(Element* aElement, int32_t aNameSpaceID,
                     nsAtom* aAttribute)
{
    // For non-chrome documents, persistance is simply broken
    if (!nsContentUtils::IsSystemPrincipal(NodePrincipal()))
        return;

    if (!mLocalStore) {
        mLocalStore = do_GetService("@mozilla.org/xul/xulstore;1");
        if (NS_WARN_IF(!mLocalStore)) {
            return;
        }
    }

    nsAutoString id;

    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
    nsAtomString attrstr(aAttribute);

    nsAutoString valuestr;
    aElement->GetAttr(kNameSpaceID_None, aAttribute, valuestr);

    nsAutoCString utf8uri;
    nsresult rv = mDocumentURI->GetSpec(utf8uri);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
    }
    NS_ConvertUTF8toUTF16 uri(utf8uri);

    bool hasAttr;
    rv = mLocalStore->HasValue(uri, id, attrstr, &hasAttr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
    }

    if (hasAttr && valuestr.IsEmpty()) {
        mLocalStore->RemoveValue(uri, id, attrstr);
        return;
    }

    // Persisting attributes to top level windows is handled by nsXULWindow.
    if (aElement->IsXULElement(nsGkAtoms::window)) {
        if (nsCOMPtr<nsIXULWindow> win = GetXULWindowIfToplevelChrome()) {
           return;
        }
    }

    mLocalStore->SetValue(uri, id, attrstr, valuestr);
}

nsresult
XULDocument::AddElementToDocumentPre(Element* aElement)
{
    // Do a bunch of work that's necessary when an element gets added
    // to the XUL Document.

    // 1. Add the element to the id map, since it seems this can be
    // called when creating elements from prototypes.
    nsAtom* id = aElement->GetID();
    if (id) {
        // FIXME: Shouldn't BindToTree take care of this?
        nsAutoScriptBlocker scriptBlocker;
        AddToIdTable(aElement, id);
    }

    return NS_OK;
}

nsresult
XULDocument::AddElementToDocumentPost(Element* aElement)
{
    if (aElement == GetRootElement()) {
        ResetDocumentDirection();
    }

    if (aElement->IsXULElement(nsGkAtoms::link)) {
        LocalizationLinkAdded(aElement);
    } else if (aElement->IsXULElement(nsGkAtoms::linkset)) {
        OnL10nResourceContainerParsed();
    }

    return NS_OK;
}

nsresult
XULDocument::AddSubtreeToDocument(nsIContent* aContent)
{
    NS_ASSERTION(aContent->GetUncomposedDoc() == this, "Element not in doc!");
    // From here on we only care about elements.
    Element* aElement = Element::FromNode(aContent);
    if (!aElement) {
        return NS_OK;
    }

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

nsresult
XULDocument::RemoveSubtreeFromDocument(nsIContent* aContent)
{
    // From here on we only care about elements.
    Element* aElement = Element::FromNode(aContent);
    if (!aElement) {
        return NS_OK;
    }

    // Do a bunch of cleanup to remove an element from the XUL
    // document.
    nsresult rv;

    // Remove any children from the document.
    for (nsIContent* child = aElement->GetLastChild();
         child;
         child = child->GetPreviousSibling()) {

        rv = RemoveSubtreeFromDocument(child);
        if (NS_FAILED(rv))
            return rv;
    }

    // Remove the element from the id map, since we added it in
    // AddElementToDocumentPre().
    nsAtom* id = aElement->GetID();
    if (id) {
        // FIXME: Shouldn't UnbindFromTree take care of this?
        nsAutoScriptBlocker scriptBlocker;
        RemoveFromIdTable(aElement, id);
    }

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsINode interface
//

nsresult
XULDocument::Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const
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
    nsresult rv = XMLDocument::Init();
    NS_ENSURE_SUCCESS(rv, rv);

    if (gRefCnt++ == 0) {
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
                                  "intl.uidirection", this);

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

        nsCOMPtr<nsIDocShell> docShell = cx->GetDocShell();
        NS_ASSERTION(docShell != nullptr, "container is not a docshell");
        if (! docShell)
            return NS_ERROR_UNEXPECTED;

        nsresult rv = shell->Initialize();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
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

    SetPrincipal(aDocumentPrincipal);

    // Create a XUL content sink, a parser, and kick off a load for
    // the document.
    RefPtr<XULContentSinkImpl> sink = new XULContentSinkImpl();

    rv = sink->Init(this, mCurrentPrototype);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize datasource sink");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
    if (NS_FAILED(rv)) return rv;

    parser->SetCommand(nsCRT::strcmp(aCommand, "view-source") ? eViewNormal :
                       eViewSource);

    parser->SetDocumentCharset(UTF_8_ENCODING,
                               kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink); // grabs a reference to the parser

    parser.forget(aResult);
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
    if (!mLocalStore) {
        mLocalStore = do_GetService("@mozilla.org/xul/xulstore;1");
        if (NS_WARN_IF(!mLocalStore)) {
            return NS_ERROR_NOT_INITIALIZED;
        }
    }

    mApplyingPersistedAttrs = true;
    ApplyPersistentAttributesInternal();
    mApplyingPersistedAttrs = false;

    return NS_OK;
}


nsresult
XULDocument::ApplyPersistentAttributesInternal()
{
    nsCOMArray<Element> elements;

    nsAutoCString utf8uri;
    nsresult rv = mDocumentURI->GetSpec(utf8uri);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
    }
    NS_ConvertUTF8toUTF16 uri(utf8uri);

    // Get a list of element IDs for which persisted values are available
    nsCOMPtr<nsIStringEnumerator> ids;
    rv = mLocalStore->GetIDsEnumerator(uri, getter_AddRefs(ids));
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
    }

    while (1) {
        bool hasmore = false;
        ids->HasMore(&hasmore);
        if (!hasmore) {
            break;
        }

        nsAutoString id;
        ids->GetNext(id);

        nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(id);
        if (!entry) {
            continue;
        }

        // We want to hold strong refs to the elements while applying
        // persistent attributes, just in case.
        elements.Clear();
        elements.SetCapacity(entry->GetIdElements().Length());
        for (Element* element : entry->GetIdElements()) {
            elements.AppendObject(element);
        }
        if (elements.IsEmpty()) {
            continue;
        }

        rv = ApplyPersistentAttributesToElements(id, elements);
        if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
        }
    }

    return NS_OK;
}

nsresult
XULDocument::ApplyPersistentAttributesToElements(const nsAString &aID,
                                                 nsCOMArray<Element>& aElements)
{
    nsAutoCString utf8uri;
    nsresult rv = mDocumentURI->GetSpec(utf8uri);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
    }
    NS_ConvertUTF8toUTF16 uri(utf8uri);

    // Get a list of attributes for which persisted values are available
    nsCOMPtr<nsIStringEnumerator> attrs;
    rv = mLocalStore->GetAttributeEnumerator(uri, aID, getter_AddRefs(attrs));
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
    }

    while (1) {
        bool hasmore = PR_FALSE;
        attrs->HasMore(&hasmore);
        if (!hasmore) {
            break;
        }

        nsAutoString attrstr;
        attrs->GetNext(attrstr);

        nsAutoString value;
        rv = mLocalStore->GetValue(uri, aID, attrstr, value);
        if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
        }

        RefPtr<nsAtom> attr = NS_Atomize(attrstr);
        if (NS_WARN_IF(!attr)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        uint32_t cnt = aElements.Count();
        for (int32_t i = int32_t(cnt) - 1; i >= 0; --i) {
            RefPtr<Element> element = aElements.SafeObjectAt(i);
            if (!element) {
                 continue;
            }

            // Applying persistent attributes to top level windows is handled
            // by nsXULWindow.
            if (element->IsXULElement(nsGkAtoms::window)) {
                if (nsCOMPtr<nsIXULWindow> win = GetXULWindowIfToplevelChrome()) {
                    continue;
                }
            }

            Unused << element->SetAttr(kNameSpaceID_None, attr, value, true);
        }
    }

    return NS_OK;
}

void
XULDocument::TraceProtos(JSTracer* aTrc)
{
    uint32_t i, count = mPrototypes.Length();
    for (i = 0; i < count; ++i) {
        mPrototypes[i]->TraceProtos(aTrc);
    }

    if (mCurrentPrototype) {
        mCurrentPrototype->TraceProtos(aTrc);
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
        if (MOZ_LOG_TEST(gXULLog, LogLevel::Error)) {
            nsCOMPtr<nsIURI> url = mCurrentPrototype->GetURI();

            nsAutoCString urlspec;
            rv = url->GetSpec(urlspec);
            if (NS_FAILED(rv)) return rv;

            MOZ_LOG(gXULLog, LogLevel::Error,
                   ("xul: error parsing '%s'", urlspec.get()));
        }

        return NS_OK;
    }

    nsINode* nodeToInsertBefore = nsINode::GetFirstChild();

    const nsTArray<RefPtr<nsXULPrototypePI> >& processingInstructions =
        mCurrentPrototype->GetProcessingInstructions();

    uint32_t total = processingInstructions.Length();
    for (uint32_t i = 0; i < total; ++i) {
        rv = CreateAndInsertPI(processingInstructions[i],
                               this, nodeToInsertBefore);
        if (NS_FAILED(rv)) return rv;
    }

    // Do one-time initialization.
    RefPtr<Element> root;

    // Add the root element
    rv = CreateElementFromPrototype(proto, getter_AddRefs(root), true);
    if (NS_FAILED(rv)) return rv;

    rv = AppendChildTo(root, false);
    if (NS_FAILED(rv)) return rv;

    // Block onload until we've finished building the complete
    // document content model.
    BlockOnload();

    nsContentUtils::AddScriptRunner(
        new nsDocElementCreatedNotificationRunner(this));

    // There'd better not be anything on the context stack at this
    // point! This is the basis case for our "induction" in
    // ResumeWalk(), below, which'll assume that there's always a
    // content element on the context stack if we're in the document.
    NS_ASSERTION(mContextStack.Depth() == 0, "something's on the context stack already");
    if (mContextStack.Depth() != 0)
        return NS_ERROR_UNEXPECTED;

    rv = mContextStack.Push(proto, root);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
XULDocument::CreateAndInsertPI(const nsXULPrototypePI* aProtoPI,
                               nsINode* aParent, nsINode* aBeforeThis)
{
    MOZ_ASSERT(aProtoPI, "null ptr");
    MOZ_ASSERT(aParent, "null ptr");

    RefPtr<ProcessingInstruction> node =
        NS_NewXMLProcessingInstruction(mNodeInfoManager, aProtoPI->mTarget,
                                       aProtoPI->mData);

    nsresult rv;
    if (aProtoPI->mTarget.EqualsLiteral("xml-stylesheet")) {
        rv = InsertXMLStylesheetPI(aProtoPI, aParent, aBeforeThis, node);
    } else {
        // No special processing, just add the PI to the document.
        rv = aParent->InsertChildBefore(node->AsContent(),
                                        aBeforeThis
                                          ? aBeforeThis->AsContent() : nullptr,
                                        false);
    }

    return rv;
}

nsresult
XULDocument::InsertXMLStylesheetPI(const nsXULPrototypePI* aProtoPI,
                                   nsINode* aParent,
                                   nsINode* aBeforeThis,
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

    rv = aParent->InsertChildBefore(aPINode->AsContent(),
                                    aBeforeThis
                                      ? aBeforeThis->AsContent() : nullptr,
                                    false);
    if (NS_FAILED(rv)) return rv;

    ssle->SetEnableUpdates(true);

    // load the stylesheet if necessary, passing ourselves as
    // nsICSSObserver
    auto result = ssle->UpdateStyleSheet(this);
    if (result.isErr()) {
        // Ignore errors from UpdateStyleSheet; we don't want failure to
        // do that to break the XUL document load.  But do propagate out
        // NS_ERROR_OUT_OF_MEMORY.
        if (result.unwrapErr() == NS_ERROR_OUT_OF_MEMORY) {
            return result.unwrapErr();
        }
        return NS_OK;
    }

    auto update = result.unwrap();
    if (update.ShouldBlock()) {
        ++mPendingSheets;
    }

    return NS_OK;
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
    nsCOMPtr<nsIURI> docURI =
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
                    // document-level hookup.
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
                        Unused << ssle->UpdateStyleSheet(nullptr);
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

            NS_ASSERTION(element, "no element on context stack");

            switch (childproto->mType) {
            case nsXULPrototypeNode::eType_Element: {
                // An 'element', which may contain more content.
                nsXULPrototypeElement* protoele =
                    static_cast<nsXULPrototypeElement*>(childproto);

                RefPtr<Element> child;


                rv = CreateElementFromPrototype(protoele,
                                                getter_AddRefs(child),
                                                false);
                if (NS_FAILED(rv)) return rv;

                // ...and append it to the content model.
                rv = element->AppendChildTo(child, false);
                if (NS_FAILED(rv)) return rv;

                // do pre-order document-level hookup.
                AddElementToDocumentPre(child);

                // If it has children, push the element onto the context
                // stack and begin to process them.
                if (protoele->mChildren.Length() > 0) {
                    rv = mContextStack.Push(protoele, child);
                    if (NS_FAILED(rv)) return rv;
                }
                else {
                    // If there are no children, do post-order document hookup
                    // immediately.
                    AddElementToDocumentPost(child);
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
                else if (scriptproto->HasScriptObject()) {
                    // An inline script
                    rv = ExecuteScript(scriptproto);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            break;

            case nsXULPrototypeNode::eType_Text: {
                // A simple text node.
                RefPtr<nsTextNode> text =
                    new nsTextNode(mNodeInfoManager);

                nsXULPrototypeText* textproto =
                    static_cast<nsXULPrototypeText*>(childproto);
                text->SetText(textproto->mValue, false);

                rv = element->AppendChildTo(text, false);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            break;

            case nsXULPrototypeNode::eType_PI: {
                nsXULPrototypePI* piProto =
                    static_cast<nsXULPrototypePI*>(childproto);

                // <?xml-stylesheet?> doesn't have an effect
                // outside the prolog, like it used to. Issue a warning.

                if (piProto->mTarget.EqualsLiteral("xml-stylesheet")) {

                    const char16_t* params[] = { piProto->mTarget.get() };

                    nsContentUtils::ReportToConsole(
                                        nsIScriptError::warningFlag,
                                        NS_LITERAL_CSTRING("XUL Document"), nullptr,
                                        nsContentUtils::eXUL_PROPERTIES,
                                        "PINotInProlog",
                                        params, ArrayLength(params),
                                        docURI);
                }

                nsIContent* parent = element.get();

                if (parent) {
                    // an inline script could have removed the root element
                    rv = CreateAndInsertPI(piProto, parent, nullptr);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }
            break;

            default:
                MOZ_ASSERT_UNREACHABLE("Unexpected nsXULPrototypeNode::Type");
            }
        }

        // Once we get here, the context stack will have been
        // depleted. That means that the entire prototype has been
        // walked and content has been constructed.
        break;
    }

    // If we get here, there is nothing left for us to walk. The content
    // model is built and ready for layout.

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
    MOZ_ASSERT(mPendingSheets == 0, "there are sheets to be loaded");
    MOZ_ASSERT(!mStillWalking, "walk not done");

    // XXXldb This is where we should really be setting the chromehidden
    // attribute.

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

        // For performance reasons, we want to trigger the DocumentL10n's `TriggerInitialDocumentTranslation` within the same
        // microtask that will be created for a `MozBeforeInitialXULLayout`
        // event listener.
        AddEventListener(NS_LITERAL_STRING("MozBeforeInitialXULLayout"), mDocumentL10n, true, false);

        nsContentUtils::DispatchTrustedEvent(
            this,
            static_cast<nsIDocument*>(this),
            NS_LITERAL_STRING("MozBeforeInitialXULLayout"),
            CanBubble::eYes,
            Cancelable::eNo);

        RemoveEventListener(NS_LITERAL_STRING("MozBeforeInitialXULLayout"), mDocumentL10n, true);

        // Before starting layout, check whether we're a toplevel chrome
        // window.  If we are, setup some state so that we don't have to restyle
        // the whole tree after StartLayout.
        if (nsCOMPtr<nsIXULWindow> win = GetXULWindowIfToplevelChrome()) {
            // We're the chrome document!
            win->BeforeStartLayout();
        }

        StartLayout();

        if (mIsWritingFastLoad && IsChromeURI(mDocumentURI))
            nsXULPrototypeCache::GetInstance()->WritePrototype(mCurrentPrototype);

        NS_ASSERTION(mDelayFrameLoaderInitialization,
                     "mDelayFrameLoaderInitialization should be true!");
        mDelayFrameLoaderInitialization = false;
        NS_WARNING_ASSERTION(
          mUpdateNestLevel == 0,
          "Constructing XUL document in middle of an update?");
        if (mUpdateNestLevel == 0) {
            MaybeInitializeFinalizeFrameLoaders();
        }

        NS_DOCUMENT_NOTIFY_OBSERVERS(EndLoad, (this));

        // DispatchContentLoadedEvents undoes the onload-blocking we
        // did in PrepareToWalk().
        DispatchContentLoadedEvents();

        mInitialLayoutComplete = true;
    }

    return NS_OK;
}

NS_IMETHODIMP
XULDocument::StyleSheetLoaded(StyleSheet* aSheet,
                              bool aWasDeferred,
                              nsresult aStatus)
{
    if (!aWasDeferred) {
        // Don't care about when alternate sheets finish loading
        MOZ_ASSERT(mPendingSheets > 0,
                   "Unexpected StyleSheetLoaded notification");

        --mPendingSheets;

        if (!mStillWalking && mPendingSheets == 0) {
            return DoneWalking();
        }
    }

    return NS_OK;
}

void
XULDocument::EndUpdate()
{
    XMLDocument::EndUpdate();
}

nsresult
XULDocument::LoadScript(nsXULPrototypeScript* aScriptProto, bool* aBlock)
{
    // Load a transcluded script
    nsresult rv;

    bool isChromeDoc = IsChromeURI(mDocumentURI);

    if (isChromeDoc && aScriptProto->HasScriptObject()) {
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

        if (aScriptProto->HasScriptObject()) {
            rv = ExecuteScript(aScriptProto);

            // Ignore return value from execution, and don't block
            *aBlock = false;
            return NS_OK;
        }
    }

    // Release script objects from FastLoad since we decided against using them
    aScriptProto->UnlinkJSObjects();

    // Set the current script prototype so that OnStreamComplete can report
    // the right file if there are errors in the script.
    NS_ASSERTION(!mCurrentScriptProto,
                 "still loading a script when starting another load?");
    mCurrentScriptProto = aScriptProto;

    if (isChromeDoc && aScriptProto->mSrcLoading) {
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
        rv = NS_NewStreamLoader(getter_AddRefs(loader),
                                aScriptProto->mSrcURI,
                                this, // aObserver
                                this, // aRequestingContext
                                nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
                                nsIContentPolicy::TYPE_INTERNAL_SCRIPT,
                                group);

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
                printf("Failed to load %s\n", uri->GetSpecOrDefault().get());
            }
        }
    }
#endif

    // This is the completion routine that will be called when a
    // transcluded script completes. Compile and execute the script
    // if the load was successful, then continue building content
    // from the prototype.
    nsresult rv = aStatus;

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

        MOZ_ASSERT(!mOffThreadCompiling && (mOffThreadCompileStringLength == 0 &&
                                            !mOffThreadCompileStringBuf),
                   "XULDocument can't load multiple scripts at once");

        rv = ScriptLoader::ConvertToUTF16(channel, string, stringLen,
                                          EmptyString(), this,
                                          mOffThreadCompileStringBuf,
                                          mOffThreadCompileStringLength);
        if (NS_SUCCEEDED(rv)) {
            // Attempt to give ownership of the buffer to the JS engine.  If
            // we hit offthread compilation, however, we will have to take it
            // back below in order to keep the memory alive until compilation
            // completes.
            JS::SourceBufferHolder srcBuf(mOffThreadCompileStringBuf,
                                          mOffThreadCompileStringLength,
                                          JS::SourceBufferHolder::GiveOwnership);
            mOffThreadCompileStringBuf = nullptr;
            mOffThreadCompileStringLength = 0;

            rv = mCurrentScriptProto->Compile(srcBuf, uri, 1, this, this);
            if (NS_SUCCEEDED(rv) && !mCurrentScriptProto->HasScriptObject()) {
                // We will be notified via OnOffThreadCompileComplete when the
                // compile finishes. The JS engine has taken ownership of the
                // source buffer.
                MOZ_RELEASE_ASSERT(!srcBuf.ownsChars());
                mOffThreadCompiling = true;
                BlockOnload();
                return NS_OK;
            }
        }
    }

    return OnScriptCompileComplete(mCurrentScriptProto->GetScriptObject(), rv);
}

NS_IMETHODIMP
XULDocument::OnScriptCompileComplete(JSScript* aScript, nsresult aStatus)
{
    // When compiling off thread the script will not have been attached to the
    // script proto yet.
    if (aScript && !mCurrentScriptProto->HasScriptObject())
        mCurrentScriptProto->Set(aScript);

    // Allow load events to be fired once off thread compilation finishes.
    if (mOffThreadCompiling) {
        mOffThreadCompiling = false;
        UnblockOnload(false);
    }

    // After compilation finishes the script's characters are no longer needed.
    if (mOffThreadCompileStringBuf) {
      js_free(mOffThreadCompileStringBuf);
      mOffThreadCompileStringBuf = nullptr;
      mOffThreadCompileStringLength = 0;
    }

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

        if (useXULCache && IsChromeURI(mDocumentURI) && scriptProto->HasScriptObject()) {
            JS::Rooted<JSScript*> script(RootingCx(), scriptProto->GetScriptObject());
            nsXULPrototypeCache::GetInstance()->PutScript(
                               scriptProto->mSrcURI, script);
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

        if (aStatus == NS_BINDING_ABORTED && !scriptProto->HasScriptObject()) {
            // If the previous doc load was aborted, we want to try loading
            // again for the next doc. Otherwise, one abort would lead to all
            // subsequent waiting docs to abort as well.
            bool block = false;
            doc->LoadScript(scriptProto, &block);
            NS_RELEASE(doc);
            return rv;
        }

        // Execute only if we loaded and compiled successfully, then resume
        if (NS_SUCCEEDED(aStatus) && scriptProto->HasScriptObject()) {
            doc->ExecuteScript(scriptProto);
        }
        doc->ResumeWalk();
        NS_RELEASE(doc);
    }

    return rv;
}

nsresult
XULDocument::ExecuteScript(nsXULPrototypeScript *aScript)
{
    MOZ_ASSERT(aScript != nullptr, "null ptr");
    NS_ENSURE_TRUE(aScript, NS_ERROR_NULL_POINTER);
    NS_ENSURE_TRUE(mScriptGlobalObject, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    rv = mScriptGlobalObject->EnsureScriptEnvironment();
    NS_ENSURE_SUCCESS(rv, rv);

    // Execute the precompiled script with the given version
    nsAutoMicroTask mt;

    // We're about to run script via JS::CloneAndExecuteScript, so we need an
    // AutoEntryScript. This is Gecko specific and not in any spec.
    AutoEntryScript aes(mScriptGlobalObject, "precompiled XUL <script> element");
    JSContext* cx = aes.cx();

    JS::Rooted<JSScript*> scriptObject(cx, aScript->GetScriptObject());
    NS_ENSURE_TRUE(scriptObject, NS_ERROR_UNEXPECTED);

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    NS_ENSURE_TRUE(xpc::Scriptability::Get(global).Allowed(), NS_OK);

    JS::ExposeObjectToActiveJS(global);
    JSAutoRealm ar(cx, global);

    // The script is in the compilation scope. Clone it into the target scope
    // and execute it. On failure, ~AutoScriptEntry will handle exceptions, so
    // there is no need to manually check the return value.
    JS::RootedValue rval(cx);
    JS::CloneAndExecuteScript(cx, scriptObject, &rval);

    return NS_OK;
}


nsresult
XULDocument::CreateElementFromPrototype(nsXULPrototypeElement* aPrototype,
                                        Element** aResult,
                                        bool aIsRoot)
{
    // Create a content model element from a prototype element.
    MOZ_ASSERT(aPrototype != nullptr, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    *aResult = nullptr;
    nsresult rv = NS_OK;

    if (MOZ_LOG_TEST(gXULLog, LogLevel::Debug)) {
        MOZ_LOG(gXULLog, LogLevel::Debug,
               ("xul: creating <%s> from prototype",
                NS_ConvertUTF16toUTF8(aPrototype->mNodeInfo->QualifiedName()).get()));
    }

    RefPtr<Element> result;

    if (aPrototype->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        // If it's a XUL element, it'll be lightweight until somebody
        // monkeys with it.
        rv = nsXULElement::CreateFromPrototype(aPrototype, this, true, aIsRoot, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // If it's not a XUL element, it's gonna be heavyweight no matter
        // what. So we need to copy everything out of the prototype
        // into the element.  Get a nodeinfo from our nodeinfo manager
        // for this node.
        RefPtr<mozilla::dom::NodeInfo> newNodeInfo;
        newNodeInfo = mNodeInfoManager->GetNodeInfo(aPrototype->mNodeInfo->NameAtom(),
                                                    aPrototype->mNodeInfo->GetPrefixAtom(),
                                                    aPrototype->mNodeInfo->NamespaceID(),
                                                    ELEMENT_NODE);
        if (!newNodeInfo) return NS_ERROR_OUT_OF_MEMORY;
        RefPtr<mozilla::dom::NodeInfo> xtfNi = newNodeInfo;
        rv = NS_NewElement(getter_AddRefs(result), newNodeInfo.forget(),
                           NOT_FROM_PARSER);
        if (NS_FAILED(rv))
            return rv;

        rv = AddAttributes(aPrototype, result);
        if (NS_FAILED(rv)) return rv;
    }

    result.forget(aResult);

    return NS_OK;
}

nsresult
XULDocument::AddAttributes(nsXULPrototypeElement* aPrototype,
                           Element* aElement)
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

//----------------------------------------------------------------------
//
// CachedChromeStreamListener
//

XULDocument::CachedChromeStreamListener::CachedChromeStreamListener(XULDocument* aDocument, bool aProtoLoaded)
    : mDocument(aDocument),
      mProtoLoaded(aProtoLoaded)
{
}


XULDocument::CachedChromeStreamListener::~CachedChromeStreamListener()
{
}


NS_IMPL_ISUPPORTS(XULDocument::CachedChromeStreamListener,
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
    MOZ_ASSERT_UNREACHABLE("CachedChromeStream doesn't receive data");
    return NS_ERROR_UNEXPECTED;
}

bool
XULDocument::IsDocumentRightToLeft()
{
    // setting the localedir attribute on the root element forces a
    // specific direction for the document.
    Element* element = GetRootElement();
    if (element) {
        static Element::AttrValuesArray strings[] =
            {nsGkAtoms::ltr, nsGkAtoms::rtl, nullptr};
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

void
XULDocument::DirectionChanged(const char* aPrefName, XULDocument* aDoc)
{
  // Reset the direction and restyle the document if necessary.
  if (aDoc) {
      aDoc->ResetDocumentDirection();
  }
}

JSObject*
XULDocument::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XULDocument_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
