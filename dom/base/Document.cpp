/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all our document implementations.
 */

#include "AudioChannelService.h"
#include "mozilla/dom/Document.h"
#include "DocumentInlines.h"
#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/ContentBlocking.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/ContentBlockingUserInteraction.h"
#include "mozilla/CSSEnabledState.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EditorCommands.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EnumSet.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IdentifierMapEntry.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Likely.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/MediaFeatureChange.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/StaticPrefs_full_screen_api.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_page_load.h"
#include "mozilla/StaticPrefs_plugins.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPresData.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/TextControlElement.h"
#include "mozilla/TextEditor.h"
#include "mozilla/URLDecorationStripper.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"
#include <algorithm>

#include "mozilla/Logging.h"
#include "plstr.h"
#include "mozilla/Sprintf.h"

#include "mozilla/Telemetry.h"
#include "nsIInlineSpellChecker.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsITextControlFrame.h"
#include "nsCommandManager.h"
#include "nsCommandParams.h"
#include "nsUnicharUtils.h"
#include "nsContentList.h"
#include "nsCSSPseudoElements.h"
#include "nsIObserver.h"
#include "nsIBaseWindow.h"
#include "nsILayoutHistoryState.h"
#include "mozilla/GlobalStyleSheetCache.h"
#include "mozilla/css/Loader.h"
#include "mozilla/css/ImageLoader.h"
#include "nsDocShell.h"
#include "nsDocShellLoadTypes.h"
#include "nsIDocShellTreeItem.h"
#include "nsCOMArray.h"
#include "nsQueryObject.h"
#include "mozilla/Services.h"
#include "nsScreen.h"
#include "ChildIterator.h"
#include "nsSerializationHelper.h"
#include "nsICertOverrideService.h"
#include "nsIX509Cert.h"
#include "nsIX509CertValidity.h"
#include "nsITransportSecurityInfo.h"
#include "nsINSSErrorsService.h"
#include "nsISocketProvider.h"
#include "nsISiteSecurityService.h"
#include "PermissionDelegateHandler.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/FullscreenChange.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/intl/LocaleService.h"

#include "mozilla/dom/Attr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/CSSImportRule.h"
#include "mozilla/dom/CSPDictionariesBinding.h"
#include "mozilla/dom/DOMIntersectionObserver.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FeaturePolicy.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/FramingChecker.h"
#include "mozilla/dom/HTMLAllCollection.h"
#include "mozilla/dom/HTMLMetaElement.h"
#include "mozilla/dom/HTMLSharedElement.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/TreeOrderedArrayInlines.h"
#include "mozilla/dom/ResizeObserver.h"
#include "mozilla/dom/ResizeObserverController.h"
#include "mozilla/dom/ServiceWorkerContainer.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/ShadowIncludingTreeIterator.h"
#include "mozilla/dom/StyleSheetList.h"
#include "mozilla/dom/SVGUseElement.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/net/CookieJarSettings.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/CDATASection.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/PostMessageEvent.h"
#include "mozilla/ipc/IdleSchedulerChild.h"
#include "nsDOMString.h"
#include "nsLayoutUtils.h"  // for GetFrameForPoint
#include "nsIFrame.h"
#include "nsIBrowserChild.h"
#include "nsImportModule.h"

#include "nsRange.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/NodeIterator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/TreeWalker.h"

#include "mozilla/dom/ServiceWorkerManager.h"
#include "imgLoader.h"

#include "nsAboutProtocolUtils.h"
#include "nsCanvasFrame.h"
#include "nsContentCID.h"
#include "nsContentSecurityUtils.h"
#include "nsError.h"
#include "nsPresContext.h"
#include "nsThreadUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIBrowserUsage.h"
#include "nsIEditingSession.h"
#include "nsIFileChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIRefreshURI.h"
#include "nsIWebNavigation.h"
#include "nsIScriptError.h"
#include "nsISimpleEnumerator.h"
#include "nsIRequestContext.h"
#include "nsStyleSheetService.h"

#include "nsNetUtil.h"  // for NS_NewURI
#include "nsIInputStreamChannel.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"

#include "nsIScriptSecurityManager.h"
#include "nsIPermission.h"
#include "nsPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIPrivateBrowsingChannel.h"
#include "ExpandedPrincipal.h"
#include "mozilla/NullPrincipal.h"

#include "nsPIDOMWindow.h"
#include "nsFocusManager.h"
#include "nsICookieService.h"

#include "nsBidiUtils.h"

#include "nsContentCreatorFunctions.h"

#include "nsIScriptContext.h"
#include "nsHTMLDocument.h"
#include "nsIRequest.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"

#include "nsCharsetSource.h"
#include "nsIParser.h"
#include "nsIContentSink.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/InternalMutationEvent.h"
#include "nsDOMCID.h"

#include "jsapi.h"
#include "xpcpublic.h"
#include "nsCCUncollectableMarker.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocumentLoader.h"
#include "nsIContentViewer.h"
#include "nsIXMLContentSink.h"
#include "nsIPrompt.h"
#include "nsIPropertyBag2.h"
#include "mozilla/dom/PageTransitionEvent.h"
#include "mozilla/dom/StyleSheetApplicableStateChangeEvent.h"
#include "nsJSUtils.h"
#include "nsFrameLoader.h"
#include "nsEscape.h"
#include "nsObjectLoadingContent.h"
#include "nsHtml5TreeOpExecutor.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLLinkElement.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/MediaSource.h"

#include "mozAutoDocUpdate.h"
#include "nsGlobalWindow.h"
#include "mozilla/Encoding.h"
#include "nsDOMNavigationTiming.h"

#include "mozilla/SMILAnimationController.h"
#include "imgIContainer.h"
#include "nsSVGUtils.h"

#include "nsRefreshDriver.h"

// FOR CSP (autogenerated by xpidl)
#include "nsIContentSecurityPolicy.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/nsCSPService.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "nsHTMLStyleSheet.h"
#include "nsHTMLCSSStyleSheet.h"
#include "mozilla/dom/DOMImplementation.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Comment.h"
#include "nsTextNode.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/HTMLCollectionBinding.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "nsXULAppAPI.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/dom/TouchEvent.h"

#include "mozilla/Preferences.h"

#include "imgRequestProxy.h"
#include "nsWrapperCacheInlines.h"
#include "nsSandboxFlags.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/ImageTracker.h"
#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/dom/NodeFilterBinding.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/WebComponentsBinding.h"
#include "mozilla/dom/CustomElementRegistryBinding.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/dom/DocumentL10n.h"
#include "mozilla/ExtensionPolicyService.h"
#include "nsFrame.h"
#include "nsDOMCaretPosition.h"
#include "nsViewportInfo.h"
#include "mozilla/StaticPtr.h"
#include "nsIHttpChannelInternal.h"
#include "nsISecurityConsoleMessage.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/dom/XPathEvaluator.h"
#include "mozilla/dom/XPathNSResolverBinding.h"
#include "mozilla/dom/XPathResult.h"
#include "nsIDocumentEncoder.h"
#include "nsIDocumentActivity.h"
#include "nsIStructuredCloneContainer.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsWindowSizes.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/dom/SVGDocument.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/dom/ChromeObserver.h"
#ifdef MOZ_XUL
#  include "mozilla/dom/XULBroadcastManager.h"
#  include "mozilla/dom/XULPersist.h"
#  include "nsIAppWindow.h"
#  include "nsXULPrototypeDocument.h"
#  include "nsXULCommandDispatcher.h"
#  include "nsXULPopupManager.h"
#  include "nsIDocShellTreeOwner.h"
#endif

#include "mozilla/DocLoadingTimelineMarker.h"

#include "mozilla/dom/WindowGlobalChild.h"

#include "nsISpeculativeConnect.h"

#include "mozilla/MediaManager.h"

#include "AutoplayPolicy.h"
#include "nsIURIMutator.h"
#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/PendingFullscreenEvent.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ResultExtensions.h"
#include "nsHTMLTags.h"
#include "MobileViewportManager.h"
#include "NodeUbiReporting.h"
#include "nsICookieService.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/net/RequestContextService.h"
#include "StorageAccessPermissionRequest.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "ThirdPartyUtil.h"
#include "nsHtml5Module.h"
#include "nsHtml5Parser.h"

#define XML_DECLARATION_BITS_DECLARATION_EXISTS (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES (1 << 3)

#define NS_MAX_DOCUMENT_WRITE_DEPTH 20

mozilla::LazyLogModule gPageCacheLog("PageCache");
mozilla::LazyLogModule gTimeoutDeferralLog("TimeoutDefer");

namespace mozilla {
namespace dom {

typedef nsTArray<Link*> LinkArray;

AutoTArray<Document*, 8>* Document::sLoadingForegroundTopLevelContentDocument =
    nullptr;

static LazyLogModule gDocumentLeakPRLog("DocumentLeak");
static LazyLogModule gCspPRLog("CSP");
LazyLogModule gUserInteractionPRLog("UserInteraction");

static nsresult GetHttpChannelHelper(nsIChannel* aChannel,
                                     nsIHttpChannel** aHttpChannel) {
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    httpChannel.forget(aHttpChannel);
    return NS_OK;
  }

  nsCOMPtr<nsIMultiPartChannel> multipart = do_QueryInterface(aChannel);
  if (!multipart) {
    *aHttpChannel = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> baseChannel;
  nsresult rv = multipart->GetBaseChannel(getter_AddRefs(baseChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  httpChannel = do_QueryInterface(baseChannel);
  httpChannel.forget(aHttpChannel);

  return NS_OK;
}

}  // namespace dom

#define NAME_NOT_VALID ((nsSimpleContentList*)1)

IdentifierMapEntry::IdentifierMapEntry(
    const IdentifierMapEntry::DependentAtomOrString* aKey)
    : mKey(aKey ? *aKey : nullptr) {}

void IdentifierMapEntry::Traverse(
    nsCycleCollectionTraversalCallback* aCallback) {
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                     "mIdentifierMap mNameContentList");
  aCallback->NoteXPCOMChild(static_cast<nsINodeList*>(mNameContentList));

  if (mImageElement) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                       "mIdentifierMap mImageElement element");
    nsIContent* imageElement = mImageElement;
    aCallback->NoteXPCOMChild(imageElement);
  }
}

bool IdentifierMapEntry::IsEmpty() {
  return mIdContentList->IsEmpty() && !mNameContentList && !mChangeCallbacks &&
         !mImageElement;
}

bool IdentifierMapEntry::HasNameElement() const {
  return mNameContentList && mNameContentList->Length() != 0;
}

void IdentifierMapEntry::AddContentChangeCallback(
    Document::IDTargetObserver aCallback, void* aData, bool aForImage) {
  if (!mChangeCallbacks) {
    mChangeCallbacks = new nsTHashtable<ChangeCallbackEntry>;
  }

  ChangeCallback cc = {aCallback, aData, aForImage};
  mChangeCallbacks->PutEntry(cc);
}

void IdentifierMapEntry::RemoveContentChangeCallback(
    Document::IDTargetObserver aCallback, void* aData, bool aForImage) {
  if (!mChangeCallbacks) return;
  ChangeCallback cc = {aCallback, aData, aForImage};
  mChangeCallbacks->RemoveEntry(cc);
  if (mChangeCallbacks->Count() == 0) {
    mChangeCallbacks = nullptr;
  }
}

void IdentifierMapEntry::FireChangeCallbacks(Element* aOldElement,
                                             Element* aNewElement,
                                             bool aImageOnly) {
  if (!mChangeCallbacks) return;

  for (auto iter = mChangeCallbacks->ConstIter(); !iter.Done(); iter.Next()) {
    IdentifierMapEntry::ChangeCallbackEntry* entry = iter.Get();
    // Don't fire image changes for non-image observers, and don't fire element
    // changes for image observers when an image override is active.
    if (entry->mKey.mForImage ? (mImageElement && !aImageOnly) : aImageOnly) {
      continue;
    }

    if (!entry->mKey.mCallback(aOldElement, aNewElement, entry->mKey.mData)) {
      iter.Remove();
    }
  }
}

void IdentifierMapEntry::AddIdElement(Element* aElement) {
  MOZ_ASSERT(aElement, "Must have element");
  MOZ_ASSERT(!mIdContentList->Contains(nullptr), "Why is null in our list?");

  size_t index = mIdContentList.Insert(*aElement);
  if (index == 0) {
    Element* oldElement = mIdContentList->SafeElementAt(1);
    FireChangeCallbacks(oldElement, aElement);
  }
}

void IdentifierMapEntry::RemoveIdElement(Element* aElement) {
  MOZ_ASSERT(aElement, "Missing element");

  // This should only be called while the document is in an update.
  // Assertions near the call to this method guarantee this.

  // This could fire in OOM situations
  // Only assert this in HTML documents for now as XUL does all sorts of weird
  // crap.
  NS_ASSERTION(!aElement->OwnerDoc()->IsHTMLDocument() ||
                   mIdContentList->Contains(aElement),
               "Removing id entry that doesn't exist");

  // XXXbz should this ever Compact() I guess when all the content is gone
  // we'll just get cleaned up in the natural order of things...
  Element* currentElement = mIdContentList->SafeElementAt(0);
  mIdContentList.RemoveElement(*aElement);
  if (currentElement == aElement) {
    FireChangeCallbacks(currentElement, mIdContentList->SafeElementAt(0));
  }
}

void IdentifierMapEntry::SetImageElement(Element* aElement) {
  Element* oldElement = GetImageIdElement();
  mImageElement = aElement;
  Element* newElement = GetImageIdElement();
  if (oldElement != newElement) {
    FireChangeCallbacks(oldElement, newElement, true);
  }
}

void IdentifierMapEntry::ClearAndNotify() {
  Element* currentElement = mIdContentList->SafeElementAt(0);
  mIdContentList.Clear();
  if (currentElement) {
    FireChangeCallbacks(currentElement, nullptr);
  }
  mNameContentList = nullptr;
  if (mImageElement) {
    SetImageElement(nullptr);
  }
  mChangeCallbacks = nullptr;
}

namespace dom {

class SimpleHTMLCollection final : public nsSimpleContentList,
                                   public nsIHTMLCollection {
 public:
  explicit SimpleHTMLCollection(nsINode* aRoot) : nsSimpleContentList(aRoot) {}

  NS_DECL_ISUPPORTS_INHERITED

  virtual nsINode* GetParentObject() override {
    return nsSimpleContentList::GetParentObject();
  }
  virtual uint32_t Length() override { return nsSimpleContentList::Length(); }
  virtual Element* GetElementAt(uint32_t aIndex) override {
    return mElements.SafeElementAt(aIndex)->AsElement();
  }

  virtual Element* GetFirstNamedElement(const nsAString& aName,
                                        bool& aFound) override {
    aFound = false;
    RefPtr<nsAtom> name = NS_Atomize(aName);
    for (uint32_t i = 0; i < mElements.Length(); i++) {
      MOZ_DIAGNOSTIC_ASSERT(mElements[i]);
      Element* element = mElements[i]->AsElement();
      if (element->GetID() == name ||
          (element->HasName() &&
           element->GetParsedAttr(nsGkAtoms::name)->GetAtomValue() == name)) {
        aFound = true;
        return element;
      }
    }
    return nullptr;
  }

  virtual void GetSupportedNames(nsTArray<nsString>& aNames) override {
    AutoTArray<nsAtom*, 8> atoms;
    for (uint32_t i = 0; i < mElements.Length(); i++) {
      MOZ_DIAGNOSTIC_ASSERT(mElements[i]);
      Element* element = mElements[i]->AsElement();

      nsAtom* id = element->GetID();
      MOZ_ASSERT(id != nsGkAtoms::_empty);
      if (id && !atoms.Contains(id)) {
        atoms.AppendElement(id);
      }

      if (element->HasName()) {
        nsAtom* name = element->GetParsedAttr(nsGkAtoms::name)->GetAtomValue();
        MOZ_ASSERT(name && name != nsGkAtoms::_empty);
        if (name && !atoms.Contains(name)) {
          atoms.AppendElement(name);
        }
      }
    }

    nsString* names = aNames.AppendElements(atoms.Length());
    for (uint32_t i = 0; i < atoms.Length(); i++) {
      atoms[i]->ToString(names[i]);
    }
  }

  virtual JSObject* GetWrapperPreserveColorInternal() override {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
  virtual void PreserveWrapperInternal(
      nsISupports* aScriptObjectHolder) override {
    nsWrapperCache::PreserveWrapper(aScriptObjectHolder);
  }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override {
    return HTMLCollection_Binding::Wrap(aCx, this, aGivenProto);
  }

  using nsBaseContentList::Item;

 private:
  virtual ~SimpleHTMLCollection() = default;
};

NS_IMPL_ISUPPORTS_INHERITED(SimpleHTMLCollection, nsSimpleContentList,
                            nsIHTMLCollection)

}  // namespace dom

void IdentifierMapEntry::AddNameElement(nsINode* aNode, Element* aElement) {
  if (!mNameContentList) {
    mNameContentList = new dom::SimpleHTMLCollection(aNode);
  }

  mNameContentList->AppendElement(aElement);
}

void IdentifierMapEntry::RemoveNameElement(Element* aElement) {
  if (mNameContentList) {
    mNameContentList->RemoveElement(aElement);
  }
}

bool IdentifierMapEntry::HasIdElementExposedAsHTMLDocumentProperty() {
  Element* idElement = GetIdElement();
  return idElement &&
         nsGenericHTMLElement::ShouldExposeIdAsHTMLDocumentProperty(idElement);
}

size_t IdentifierMapEntry::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return mKey.mString.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

// Helper structs for the content->subdoc map

class SubDocMapEntry : public PLDHashEntryHdr {
 public:
  // Both of these are strong references
  Element* mKey;  // must be first, to look like PLDHashEntryStub
  dom::Document* mSubDocument;
};

class OnloadBlocker final : public nsIRequest {
 public:
  OnloadBlocker() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST

 private:
  ~OnloadBlocker() = default;
};

NS_IMPL_ISUPPORTS(OnloadBlocker, nsIRequest)

NS_IMETHODIMP
OnloadBlocker::GetName(nsACString& aResult) {
  aResult.AssignLiteral("about:document-onload-blocker");
  return NS_OK;
}

NS_IMETHODIMP
OnloadBlocker::IsPending(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
OnloadBlocker::GetStatus(nsresult* status) {
  *status = NS_OK;
  return NS_OK;
}

NS_IMETHODIMP
OnloadBlocker::Cancel(nsresult status) { return NS_OK; }
NS_IMETHODIMP
OnloadBlocker::Suspend(void) { return NS_OK; }
NS_IMETHODIMP
OnloadBlocker::Resume(void) { return NS_OK; }

NS_IMETHODIMP
OnloadBlocker::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  *aLoadGroup = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
OnloadBlocker::SetLoadGroup(nsILoadGroup* aLoadGroup) { return NS_OK; }

NS_IMETHODIMP
OnloadBlocker::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = nsIRequest::LOAD_NORMAL;
  return NS_OK;
}

NS_IMETHODIMP
OnloadBlocker::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
OnloadBlocker::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
OnloadBlocker::SetLoadFlags(nsLoadFlags aLoadFlags) { return NS_OK; }

// ==================================================================

namespace dom {

ExternalResourceMap::ExternalResourceMap() : mHaveShutDown(false) {}

Document* ExternalResourceMap::RequestResource(
    nsIURI* aURI, nsIReferrerInfo* aReferrerInfo, nsINode* aRequestingNode,
    Document* aDisplayDocument, ExternalResourceLoad** aPendingLoad) {
  // If we ever start allowing non-same-origin loads here, we might need to do
  // something interesting with aRequestingPrincipal even for the hashtable
  // gets.
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aRequestingNode, "Must have a node");
  MOZ_ASSERT(aReferrerInfo, "Must have a referrerInfo");
  *aPendingLoad = nullptr;
  if (mHaveShutDown) {
    return nullptr;
  }

  // First, make sure we strip the ref from aURI.
  nsCOMPtr<nsIURI> clone;
  nsresult rv = NS_GetURIWithoutRef(aURI, getter_AddRefs(clone));
  if (NS_FAILED(rv) || !clone) {
    return nullptr;
  }

  ExternalResource* resource;
  mMap.Get(clone, &resource);
  if (resource) {
    return resource->mDocument;
  }

  RefPtr<PendingLoad>& loadEntry = mPendingLoads.GetOrInsert(clone);
  if (loadEntry) {
    RefPtr<PendingLoad> load(loadEntry);
    load.forget(aPendingLoad);
    return nullptr;
  }

  RefPtr<PendingLoad> load(new PendingLoad(aDisplayDocument));
  loadEntry = load;

  if (NS_FAILED(load->StartLoad(clone, aReferrerInfo, aRequestingNode))) {
    // Make sure we don't thrash things by trying this load again, since
    // chances are it failed for good reasons (security check, etc).
    AddExternalResource(clone, nullptr, nullptr, aDisplayDocument);
  } else {
    load.forget(aPendingLoad);
  }

  return nullptr;
}

void ExternalResourceMap::EnumerateResources(SubDocEnumFunc aCallback) {
  nsTArray<RefPtr<Document>> docs(mMap.Count());
  for (const auto& entry : mMap) {
    if (Document* doc = entry.GetData()->mDocument) {
      docs.AppendElement(doc);
    }
  }

  for (auto& doc : docs) {
    if (aCallback(*doc) == CallState::Stop) {
      return;
    }
  }
}

void ExternalResourceMap::Traverse(
    nsCycleCollectionTraversalCallback* aCallback) const {
  // mPendingLoads will get cleared out as the requests complete, so
  // no need to worry about those here.
  for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
    ExternalResourceMap::ExternalResource* resource = iter.UserData();

    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                       "mExternalResourceMap.mMap entry"
                                       "->mDocument");
    aCallback->NoteXPCOMChild(ToSupports(resource->mDocument));

    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                       "mExternalResourceMap.mMap entry"
                                       "->mViewer");
    aCallback->NoteXPCOMChild(resource->mViewer);

    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                       "mExternalResourceMap.mMap entry"
                                       "->mLoadGroup");
    aCallback->NoteXPCOMChild(resource->mLoadGroup);
  }
}

void ExternalResourceMap::HideViewers() {
  for (auto iter = mMap.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsIContentViewer> viewer = iter.UserData()->mViewer;
    if (viewer) {
      viewer->Hide();
    }
  }
}

void ExternalResourceMap::ShowViewers() {
  for (auto iter = mMap.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsIContentViewer> viewer = iter.UserData()->mViewer;
    if (viewer) {
      viewer->Show();
    }
  }
}

void TransferZoomLevels(Document* aFromDoc, Document* aToDoc) {
  MOZ_ASSERT(aFromDoc && aToDoc, "transferring zoom levels from/to null doc");

  nsPresContext* fromCtxt = aFromDoc->GetPresContext();
  if (!fromCtxt) return;

  nsPresContext* toCtxt = aToDoc->GetPresContext();
  if (!toCtxt) return;

  toCtxt->SetFullZoom(fromCtxt->GetFullZoom());
  toCtxt->SetTextZoom(fromCtxt->TextZoom());
  toCtxt->SetOverrideDPPX(fromCtxt->GetOverrideDPPX());
}

void TransferShowingState(Document* aFromDoc, Document* aToDoc) {
  MOZ_ASSERT(aFromDoc && aToDoc, "transferring showing state from/to null doc");

  if (aFromDoc->IsShowing()) {
    aToDoc->OnPageShow(true, nullptr);
  }
}

nsresult ExternalResourceMap::AddExternalResource(nsIURI* aURI,
                                                  nsIContentViewer* aViewer,
                                                  nsILoadGroup* aLoadGroup,
                                                  Document* aDisplayDocument) {
  MOZ_ASSERT(aURI, "Unexpected call");
  MOZ_ASSERT((aViewer && aLoadGroup) || (!aViewer && !aLoadGroup),
             "Must have both or neither");

  RefPtr<PendingLoad> load;
  mPendingLoads.Remove(aURI, getter_AddRefs(load));

  nsresult rv = NS_OK;

  nsCOMPtr<Document> doc;
  if (aViewer) {
    doc = aViewer->GetDocument();
    NS_ASSERTION(doc, "Must have a document");

    doc->SetDisplayDocument(aDisplayDocument);

    // Make sure that hiding our viewer will tear down its presentation.
    aViewer->SetSticky(false);

    rv = aViewer->Init(nullptr, nsIntRect(0, 0, 0, 0), nullptr);
    if (NS_SUCCEEDED(rv)) {
      rv = aViewer->Open(nullptr, nullptr);
    }

    if (NS_FAILED(rv)) {
      doc = nullptr;
      aViewer = nullptr;
      aLoadGroup = nullptr;
    }
  }

  ExternalResource* newResource = new ExternalResource();
  mMap.Put(aURI, newResource);

  newResource->mDocument = doc;
  newResource->mViewer = aViewer;
  newResource->mLoadGroup = aLoadGroup;
  if (doc) {
    TransferZoomLevels(aDisplayDocument, doc);
    TransferShowingState(aDisplayDocument, doc);
  }

  const nsTArray<nsCOMPtr<nsIObserver>>& obs = load->Observers();
  for (uint32_t i = 0; i < obs.Length(); ++i) {
    obs[i]->Observe(ToSupports(doc), "external-resource-document-created",
                    nullptr);
  }

  return rv;
}

NS_IMPL_ISUPPORTS(ExternalResourceMap::PendingLoad, nsIStreamListener,
                  nsIRequestObserver)

NS_IMETHODIMP
ExternalResourceMap::PendingLoad::OnStartRequest(nsIRequest* aRequest) {
  ExternalResourceMap& map = mDisplayDocument->ExternalResourceMap();
  if (map.HaveShutDown()) {
    return NS_BINDING_ABORTED;
  }

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv =
      SetupViewer(aRequest, getter_AddRefs(viewer), getter_AddRefs(loadGroup));

  // Make sure to do this no matter what
  nsresult rv2 =
      map.AddExternalResource(mURI, viewer, loadGroup, mDisplayDocument);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (NS_FAILED(rv2)) {
    mTargetListener = nullptr;
    return rv2;
  }

  return mTargetListener->OnStartRequest(aRequest);
}

nsresult ExternalResourceMap::PendingLoad::SetupViewer(
    nsIRequest* aRequest, nsIContentViewer** aViewer,
    nsILoadGroup** aLoadGroup) {
  MOZ_ASSERT(!mTargetListener, "Unexpected call to OnStartRequest");
  *aViewer = nullptr;
  *aLoadGroup = nullptr;

  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  NS_ENSURE_TRUE(chan, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
  if (httpChannel) {
    bool requestSucceeded;
    if (NS_FAILED(httpChannel->GetRequestSucceeded(&requestSucceeded)) ||
        !requestSucceeded) {
      // Bail out on this load, since it looks like we have an HTTP error page
      return NS_BINDING_ABORTED;
    }
  }

  nsAutoCString type;
  chan->GetContentType(type);

  nsCOMPtr<nsILoadGroup> loadGroup;
  chan->GetLoadGroup(getter_AddRefs(loadGroup));

  // Give this document its own loadgroup
  nsCOMPtr<nsILoadGroup> newLoadGroup =
      do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  NS_ENSURE_TRUE(newLoadGroup, NS_ERROR_OUT_OF_MEMORY);
  newLoadGroup->SetLoadGroup(loadGroup);

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));

  nsCOMPtr<nsIInterfaceRequestor> newCallbacks =
      new LoadgroupCallbacks(callbacks);
  newLoadGroup->SetNotificationCallbacks(newCallbacks);

  // This is some serious hackery cribbed from docshell
  nsCOMPtr<nsICategoryManager> catMan =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(catMan, NS_ERROR_NOT_AVAILABLE);
  nsCString contractId;
  nsresult rv =
      catMan->GetCategoryEntry("Gecko-Content-Viewers", type, contractId);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
      do_GetService(contractId.get());
  NS_ENSURE_TRUE(docLoaderFactory, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsIStreamListener> listener;
  rv = docLoaderFactory->CreateInstance(
      "external-resource", chan, newLoadGroup, type, nullptr, nullptr,
      getter_AddRefs(listener), getter_AddRefs(viewer));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(viewer, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIParser> parser = do_QueryInterface(listener);
  if (!parser) {
    /// We don't want to deal with the various fake documents yet
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // We can't handle HTML and other weird things here yet.
  nsIContentSink* sink = parser->GetContentSink();
  nsCOMPtr<nsIXMLContentSink> xmlSink = do_QueryInterface(sink);
  if (!xmlSink) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  listener.swap(mTargetListener);
  viewer.forget(aViewer);
  newLoadGroup.forget(aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
ExternalResourceMap::PendingLoad::OnDataAvailable(nsIRequest* aRequest,
                                                  nsIInputStream* aStream,
                                                  uint64_t aOffset,
                                                  uint32_t aCount) {
  // mTargetListener might be null if SetupViewer or AddExternalResource failed.
  NS_ENSURE_TRUE(mTargetListener, NS_ERROR_FAILURE);
  if (mDisplayDocument->ExternalResourceMap().HaveShutDown()) {
    return NS_BINDING_ABORTED;
  }
  return mTargetListener->OnDataAvailable(aRequest, aStream, aOffset, aCount);
}

NS_IMETHODIMP
ExternalResourceMap::PendingLoad::OnStopRequest(nsIRequest* aRequest,
                                                nsresult aStatus) {
  // mTargetListener might be null if SetupViewer or AddExternalResource failed
  if (mTargetListener) {
    nsCOMPtr<nsIStreamListener> listener;
    mTargetListener.swap(listener);
    return listener->OnStopRequest(aRequest, aStatus);
  }

  return NS_OK;
}

nsresult ExternalResourceMap::PendingLoad::StartLoad(
    nsIURI* aURI, nsIReferrerInfo* aReferrerInfo, nsINode* aRequestingNode) {
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aRequestingNode, "Must have a node");
  MOZ_ASSERT(aReferrerInfo, "Must have a referrerInfo");

  nsCOMPtr<nsILoadGroup> loadGroup =
      aRequestingNode->OwnerDoc()->GetDocumentLoadGroup();

  nsresult rv = NS_OK;
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), aURI, aRequestingNode,
                     nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
                     nsIContentPolicy::TYPE_OTHER,
                     nullptr,  // aPerformanceStorage
                     loadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    rv = httpChannel->SetReferrerInfo(aReferrerInfo);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  mURI = aURI;

  return channel->AsyncOpen(this);
}

NS_IMPL_ISUPPORTS(ExternalResourceMap::LoadgroupCallbacks,
                  nsIInterfaceRequestor)

#define IMPL_SHIM(_i) \
  NS_IMPL_ISUPPORTS(ExternalResourceMap::LoadgroupCallbacks::_i##Shim, _i)

IMPL_SHIM(nsILoadContext)
IMPL_SHIM(nsIProgressEventSink)
IMPL_SHIM(nsIChannelEventSink)
IMPL_SHIM(nsIApplicationCacheContainer)

#undef IMPL_SHIM

#define IID_IS(_i) aIID.Equals(NS_GET_IID(_i))

#define TRY_SHIM(_i)                                 \
  PR_BEGIN_MACRO                                     \
  if (IID_IS(_i)) {                                  \
    nsCOMPtr<_i> real = do_GetInterface(mCallbacks); \
    if (!real) {                                     \
      return NS_NOINTERFACE;                         \
    }                                                \
    nsCOMPtr<_i> shim = new _i##Shim(this, real);    \
    shim.forget(aSink);                              \
    return NS_OK;                                    \
  }                                                  \
  PR_END_MACRO

NS_IMETHODIMP
ExternalResourceMap::LoadgroupCallbacks::GetInterface(const nsIID& aIID,
                                                      void** aSink) {
  if (mCallbacks && (IID_IS(nsIPrompt) || IID_IS(nsIAuthPrompt) ||
                     IID_IS(nsIAuthPrompt2) || IID_IS(nsIBrowserChild))) {
    return mCallbacks->GetInterface(aIID, aSink);
  }

  *aSink = nullptr;

  TRY_SHIM(nsILoadContext);
  TRY_SHIM(nsIProgressEventSink);
  TRY_SHIM(nsIChannelEventSink);
  TRY_SHIM(nsIApplicationCacheContainer);

  return NS_NOINTERFACE;
}

#undef TRY_SHIM
#undef IID_IS

ExternalResourceMap::ExternalResource::~ExternalResource() {
  if (mViewer) {
    mViewer->Close(nullptr);
    mViewer->Destroy();
  }
}

// ==================================================================
// =
// ==================================================================

// If we ever have an nsIDocumentObserver notification for stylesheet title
// changes we should update the list from that instead of overriding
// EnsureFresh.
class DOMStyleSheetSetList final : public DOMStringList {
 public:
  explicit DOMStyleSheetSetList(Document* aDocument);

  void Disconnect() { mDocument = nullptr; }

  virtual void EnsureFresh() override;

 protected:
  Document* mDocument;  // Our document; weak ref.  It'll let us know if it
                        // dies.
};

DOMStyleSheetSetList::DOMStyleSheetSetList(Document* aDocument)
    : mDocument(aDocument) {
  NS_ASSERTION(mDocument, "Must have document!");
}

void DOMStyleSheetSetList::EnsureFresh() {
  MOZ_ASSERT(NS_IsMainThread());

  mNames.Clear();

  if (!mDocument) {
    return;  // Spec says "no exceptions", and we have no style sets if we have
             // no document, for sure
  }

  size_t count = mDocument->SheetCount();
  nsAutoString title;
  for (size_t index = 0; index < count; index++) {
    StyleSheet* sheet = mDocument->SheetAt(index);
    NS_ASSERTION(sheet, "Null sheet in sheet list!");
    sheet->GetTitle(title);
    if (!title.IsEmpty() && !mNames.Contains(title) && !Add(title)) {
      return;
    }
  }
}

// ==================================================================
Document::SelectorCache::SelectorCache(nsIEventTarget* aEventTarget)
    : nsExpirationTracker<SelectorCacheKey, 4>(1000, "Document::SelectorCache",
                                               aEventTarget) {}

Document::SelectorCache::~SelectorCache() { AgeAllGenerations(); }

void Document::SelectorCache::NotifyExpired(SelectorCacheKey* aSelector) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSelector);

  // There is no guarantee that this method won't be re-entered when selector
  // matching is ongoing because "memory-pressure" could be notified immediately
  // when OOM happens according to the design of nsExpirationTracker.
  // The perfect solution is to delete the |aSelector| and its
  // RawServoSelectorList in mTable asynchronously.
  // We remove these objects synchronously for now because NotifyExpired() will
  // never be triggered by "memory-pressure" which is not implemented yet in
  // the stage 2 of mozalloc_handle_oom().
  // Once these objects are removed asynchronously, we should update the warning
  // added in mozalloc_handle_oom() as well.
  RemoveObject(aSelector);
  mTable.Remove(aSelector->mKey);
  delete aSelector;
}

Document::FrameRequest::FrameRequest(FrameRequestCallback& aCallback,
                                     int32_t aHandle)
    : mCallback(&aCallback), mHandle(aHandle) {}

// ==================================================================
// =
// ==================================================================

Document::InternalCommandDataHashtable*
    Document::sInternalCommandDataHashtable = nullptr;

// static
void Document::Shutdown() {
  if (sInternalCommandDataHashtable) {
    sInternalCommandDataHashtable->Clear();
    delete sInternalCommandDataHashtable;
    sInternalCommandDataHashtable = nullptr;
  }
}

Document::Document(const char* aContentType)
    : nsINode(nullptr),
      DocumentOrShadowRoot(this),
      mBlockAllMixedContent(false),
      mBlockAllMixedContentPreloads(false),
      mUpgradeInsecureRequests(false),
      mUpgradeInsecurePreloads(false),
      mDontWarnAboutMutationEventsAndAllowSlowDOMMutations(false),
      mCharacterSet(WINDOWS_1252_ENCODING),
      mCharacterSetSource(0),
      mParentDocument(nullptr),
      mCachedRootElement(nullptr),
      mNodeInfoManager(nullptr),
#ifdef DEBUG
      mStyledLinksCleared(false),
#endif
      mBidiEnabled(false),
      mMayNeedFontPrefsUpdate(true),
      mMathMLEnabled(false),
      mIsInitialDocumentInWindow(false),
      mIgnoreDocGroupMismatches(false),
      mLoadedAsData(false),
      mMayStartLayout(true),
      mHaveFiredTitleChange(false),
      mIsShowing(false),
      mVisible(true),
      mRemovedFromDocShell(false),
      // mAllowDNSPrefetch starts true, so that we can always reliably && it
      // with various values that might disable it.  Since we never prefetch
      // unless we get a window, and in that case the docshell value will get
      // &&-ed in, this is safe.
      mAllowDNSPrefetch(true),
      mIsStaticDocument(false),
      mCreatingStaticClone(false),
      mInUnlinkOrDeletion(false),
      mHasHadScriptHandlingObject(false),
      mIsBeingUsedAsImage(false),
      mDocURISchemeIsChrome(false),
      mInChromeDocShell(false),
      mIsDevToolsDocument(false),
      mIsSyntheticDocument(false),
      mHasLinksToUpdateRunnable(false),
      mFlushingPendingLinkUpdates(false),
      mMayHaveDOMMutationObservers(false),
      mMayHaveAnimationObservers(false),
      mHasMixedActiveContentLoaded(false),
      mHasMixedActiveContentBlocked(false),
      mHasMixedDisplayContentLoaded(false),
      mHasMixedDisplayContentBlocked(false),
      mHasCSP(false),
      mHasUnsafeEvalCSP(false),
      mHasUnsafeInlineCSP(false),
      mHasCSPDeliveredThroughHeader(false),
      mBFCacheDisallowed(false),
      mHasHadDefaultView(false),
      mStyleSheetChangeEventsEnabled(false),
      mIsSrcdocDocument(false),
      mHasDisplayDocument(false),
      mFontFaceSetDirty(true),
      mDidFireDOMContentLoaded(true),
      mHasScrollLinkedEffect(false),
      mFrameRequestCallbacksScheduled(false),
      mIsTopLevelContentDocument(false),
      mIsContentDocument(false),
      mDidCallBeginLoad(false),
      mAllowPaymentRequest(false),
      mEncodingMenuDisabled(false),
      mLinksEnabled(true),
      mIsSVGGlyphsDocument(false),
      mInDestructor(false),
      mIsGoingAway(false),
      mInXBLUpdate(false),
      mNeedsReleaseAfterStackRefCntRelease(false),
      mStyleSetFilled(false),
      mQuirkSheetAdded(false),
      mContentEditableSheetAdded(false),
      mDesignModeSheetAdded(false),
      mSSApplicableStateNotificationPending(false),
      mMayHaveTitleElement(false),
      mDOMLoadingSet(false),
      mDOMInteractiveSet(false),
      mDOMCompleteSet(false),
      mAutoFocusFired(false),
      mScrolledToRefAlready(false),
      mChangeScrollPosWhenScrollingToRef(false),
      mDelayFrameLoaderInitialization(false),
      mSynchronousDOMContentLoaded(false),
      mMaybeServiceWorkerControlled(false),
      mAllowZoom(false),
      mValidScaleFloat(false),
      mValidMinScale(false),
      mValidMaxScale(false),
      mWidthStrEmpty(false),
      mParserAborted(false),
      mReportedUseCounters(false),
      mHasReportedShadowDOMUsage(false),
      mDocTreeHadAudibleMedia(false),
      mDocTreeHadPlayRevoked(false),
      mHasDelayedRefreshEvent(false),
      mLoadEventFiring(false),
      mSkipLoadEventAfterClose(false),
      mDisableCookieAccess(false),
      mDisableDocWrite(false),
      mTooDeepWriteRecursion(false),
      mPendingMaybeEditingStateChanged(false),
      mHasBeenEditable(false),
      mHasWarnedAboutZoom(false),
      mIsRunningExecCommand(false),
      mPendingFullscreenRequests(0),
      mXMLDeclarationBits(0),
      mOnloadBlockCount(0),
      mAsyncOnloadBlockCount(0),
      mWriteLevel(0),
      mContentEditableCount(0),
      mEditingState(EditingState::eOff),
      mCompatMode(eCompatibility_FullStandards),
      mReadyState(ReadyState::READYSTATE_UNINITIALIZED),
      mAncestorIsLoading(false),
      mVisibilityState(dom::VisibilityState::Hidden),
      mType(eUnknown),
      mDefaultElementType(0),
      mAllowXULXBL(eTriUnset),
      mSkipDTDSecurityChecks(false),
      mBidiOptions(IBMBIDI_DEFAULT_BIDI_OPTIONS),
      mSandboxFlags(0),
      mPartID(0),
      mMarkedCCGeneration(0),
      mPresShell(nullptr),
      mSubtreeModifiedDepth(0),
      mPreloadPictureDepth(0),
      mEventsSuppressed(0),
      mIgnoreDestructiveWritesCounter(0),
      mFrameRequestCallbackCounter(0),
      mStaticCloneCount(0),
      mWindow(nullptr),
      mBFCacheEntry(nullptr),
      mInSyncOperationCount(0),
      mBlockDOMContentLoaded(0),
      mUseCounters(0),
      mChildDocumentUseCounters(0),
      mUserHasInteracted(false),
      mHasUserInteractionTimerScheduled(false),
      mStackRefCnt(0),
      mUpdateNestLevel(0),
      mViewportType(Unknown),
      mViewportFit(ViewportFitType::Auto),
      mSubDocuments(nullptr),
      mHeaderData(nullptr),
      mFlashClassification(FlashClassification::Unknown),
      mScrollAnchorAdjustmentLength(0),
      mScrollAnchorAdjustmentCount(0),
      mServoRestyleRootDirtyBits(0),
      mThrowOnDynamicMarkupInsertionCounter(0),
      mIgnoreOpensDuringUnloadCounter(0),
      mDocLWTheme(Doc_Theme_Uninitialized),
      mSavedResolution(1.0f),
      mSavedResolutionBeforeMVM(1.0f),
      mPendingInitialTranslation(false),
      mHasStoragePermission(false),
      mGeneration(0),
      mCachedTabSizeGeneration(0),
      mNextFormNumber(0),
      mNextControlNumber(0) {
  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug, ("DOCUMENT %p created", this));

  SetIsInDocument();
  SetIsConnected(true);

  // Create these unconditionally, they will be used to warn about the `zoom`
  // property, even if use counters are disabled.
  mStyleUseCounters = Servo_UseCounters_Create().Consume();

  SetContentTypeInternal(nsDependentCString(aContentType));

  // Start out mLastStyleSheetSet as null, per spec
  SetDOMStringToNull(mLastStyleSheetSet);

  // void state used to differentiate an empty source from an unselected source
  mPreloadPictureFoundSource.SetIsVoid(true);

  RecomputeLanguageFromCharset();

  mPreloadReferrerInfo = new dom::ReferrerInfo(nullptr);
  mReferrerInfo = new dom::ReferrerInfo(nullptr);
}

#ifndef ANDROID
// unused by GeckoView
static bool IsAboutErrorPage(nsGlobalWindowInner* aWin, const char* aSpec) {
  if (NS_WARN_IF(!aWin)) {
    return false;
  }

  nsIURI* uri = aWin->GetDocumentURI();
  if (NS_WARN_IF(!uri)) {
    return false;
  }
  // getSpec is an expensive operation, hence we first check the scheme
  // to see if the caller is actually an about: page.
  if (!uri->SchemeIs("about")) {
    return false;
  }

  nsAutoCString aboutSpec;
  nsresult rv = NS_GetAboutModuleName(uri, aboutSpec);
  NS_ENSURE_SUCCESS(rv, false);

  return aboutSpec.EqualsASCII(aSpec);
}
#endif

bool Document::CallerIsTrustedAboutNetError(JSContext* aCx, JSObject* aObject) {
  nsGlobalWindowInner* win = xpc::WindowOrNull(aObject);
#ifdef ANDROID
  // GeckoView uses data URLs for error pages, so for now just check for any
  // error page
  return win && win->GetDocument() && win->GetDocument()->IsErrorPage();
#else
  return IsAboutErrorPage(win, "neterror");
#endif
}

already_AddRefed<mozilla::dom::Promise> Document::AddCertException(
    bool aIsTemporary) {
  nsIGlobalObject* global = GetScopeObject();
  if (!global) {
    return nullptr;
  }

  ErrorResult er;
  RefPtr<Promise> promise =
      Promise::Create(global, er, Promise::ePropagateUserInteraction);
  if (er.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> info;
  nsCOMPtr<nsITransportSecurityInfo> tsi;
  nsresult rv = NS_OK;
  if (NS_WARN_IF(!mFailedChannel)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  rv = mFailedChannel->GetSecurityInfo(getter_AddRefs(info));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }
  nsCOMPtr<nsIURI> failedChannelURI;
  NS_GetFinalChannelURI(mFailedChannel, getter_AddRefs(failedChannelURI));
  if (!failedChannelURI) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }
  nsAutoCString host;
  failedChannelURI->GetAsciiHost(host);
  int32_t port;
  failedChannelURI->GetPort(&port);

  tsi = do_QueryInterface(info);
  if (NS_WARN_IF(!tsi)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  bool isUntrusted = true;
  rv = tsi->GetIsUntrusted(&isUntrusted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  bool isDomainMismatch = true;
  rv = tsi->GetIsDomainMismatch(&isDomainMismatch);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  bool isNotValidAtThisTime = true;
  rv = tsi->GetIsNotValidAtThisTime(&isNotValidAtThisTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  nsCOMPtr<nsIX509Cert> cert;
  rv = tsi->GetServerCert(getter_AddRefs(cert));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }
  if (NS_WARN_IF(!cert)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  uint32_t flags = 0;
  if (isUntrusted) {
    flags |= nsICertOverrideService::ERROR_UNTRUSTED;
  }
  if (isDomainMismatch) {
    flags |= nsICertOverrideService::ERROR_MISMATCH;
  }
  if (isNotValidAtThisTime) {
    flags |= nsICertOverrideService::ERROR_TIME;
  }

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsISerializable> certSer = do_QueryInterface(cert);
    nsCString certSerialized;
    NS_SerializeToString(certSer, certSerialized);

    ContentChild* cc = ContentChild::GetSingleton();
    MOZ_ASSERT(cc);
    cc->SendAddCertException(certSerialized, flags, host, port, aIsTemporary)
        ->Then(GetCurrentThreadSerialEventTarget(), __func__,
               [promise](const mozilla::MozPromise<
                         nsresult, mozilla::ipc::ResponseRejectReason,
                         true>::ResolveOrRejectValue& aValue) {
                 if (aValue.IsResolve()) {
                   promise->MaybeResolve(aValue.ResolveValue());
                 } else {
                   promise->MaybeRejectWithUndefined();
                 }
               });
    return promise.forget();
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsICertOverrideService> overrideService =
        do_GetService(NS_CERTOVERRIDE_CONTRACTID);
    if (!overrideService) {
      promise->MaybeReject(NS_ERROR_FAILURE);
      return promise.forget();
    }

    rv = overrideService->RememberValidityOverride(host, port, cert, flags,
                                                   aIsTemporary);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      promise->MaybeReject(rv);
      return promise.forget();
    }

    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  promise->MaybeReject(NS_ERROR_FAILURE);
  return promise.forget();
}

void Document::GetNetErrorInfo(NetErrorInfo& aInfo, ErrorResult& aRv) {
  nsCOMPtr<nsISupports> info;
  nsCOMPtr<nsITransportSecurityInfo> tsi;
  nsresult rv = NS_OK;
  if (NS_WARN_IF(!mFailedChannel)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  rv = mFailedChannel->GetSecurityInfo(getter_AddRefs(info));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  tsi = do_QueryInterface(info);
  if (NS_WARN_IF(!tsi)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsAutoString errorCodeString;
  rv = tsi->GetErrorCodeString(errorCodeString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  aInfo.mErrorCodeString.Assign(errorCodeString);
}

bool Document::CallerIsTrustedAboutCertError(JSContext* aCx,
                                             JSObject* aObject) {
  nsGlobalWindowInner* win = xpc::WindowOrNull(aObject);
#ifdef ANDROID
  // GeckoView uses data URLs for error pages, so for now just check for any
  // error page
  return win && win->GetDocument() && win->GetDocument()->IsErrorPage();
#else
  return IsAboutErrorPage(win, "certerror");
#endif
}

bool Document::IsErrorPage() const {
  nsCOMPtr<nsILoadInfo> loadInfo = mChannel ? mChannel->LoadInfo() : nullptr;
  return loadInfo && loadInfo->GetLoadErrorPage();
}

void Document::GetFailedCertSecurityInfo(FailedCertSecurityInfo& aInfo,
                                         ErrorResult& aRv) {
  nsCOMPtr<nsISupports> info;
  nsCOMPtr<nsITransportSecurityInfo> tsi;
  nsresult rv = NS_OK;
  if (NS_WARN_IF(!mFailedChannel)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  rv = mFailedChannel->GetSecurityInfo(getter_AddRefs(info));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  tsi = do_QueryInterface(info);
  if (NS_WARN_IF(!tsi)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsAutoString errorCodeString;
  rv = tsi->GetErrorCodeString(errorCodeString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  aInfo.mErrorCodeString.Assign(errorCodeString);

  rv = tsi->GetIsUntrusted(&aInfo.mIsUntrusted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  rv = tsi->GetIsDomainMismatch(&aInfo.mIsDomainMismatch);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  rv = tsi->GetIsNotValidAtThisTime(&aInfo.mIsNotValidAtThisTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsIX509Cert> cert;
  nsCOMPtr<nsIX509CertValidity> validity;
  rv = tsi->GetServerCert(getter_AddRefs(cert));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  if (NS_WARN_IF(!cert)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  rv = cert->GetValidity(getter_AddRefs(validity));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  if (NS_WARN_IF(!validity)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  PRTime validityResult;
  rv = validity->GetNotBefore(&validityResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  aInfo.mValidNotBefore = DOMTimeStamp(validityResult / PR_USEC_PER_MSEC);

  rv = validity->GetNotAfter(&validityResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  aInfo.mValidNotAfter = DOMTimeStamp(validityResult / PR_USEC_PER_MSEC);

  nsAutoString subjectAltNames;
  rv = cert->GetSubjectAltNames(subjectAltNames);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  aInfo.mSubjectAltNames = subjectAltNames;

  nsAutoString issuerCommonName;
  nsAutoString certChainPEMString;
  Sequence<nsString>& certChainStrings = aInfo.mCertChainStrings.Construct();
  int64_t maxValidity = std::numeric_limits<int64_t>::max();
  int64_t minValidity = 0;
  PRTime notBefore, notAfter;
  nsTArray<RefPtr<nsIX509Cert>> failedCertArray;
  rv = tsi->GetFailedCertChain(failedCertArray);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (NS_WARN_IF(failedCertArray.IsEmpty())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  for (const auto& certificate : failedCertArray) {
    rv = certificate->GetIssuerCommonName(issuerCommonName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }

    rv = certificate->GetValidity(getter_AddRefs(validity));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }
    if (NS_WARN_IF(!validity)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    rv = validity->GetNotBefore(&notBefore);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }

    rv = validity->GetNotAfter(&notAfter);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }

    notBefore = std::max(minValidity, notBefore);
    notAfter = std::min(maxValidity, notAfter);
    nsTArray<uint8_t> certArray;
    rv = certificate->GetRawDER(certArray);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }

    certArray.AppendElement(
        0);  // Append null terminator, required by nsC*String.
    nsDependentCString derString(reinterpret_cast<char*>(certArray.Elements()),
                                 certArray.Length() - 1);
    nsAutoCString der64;
    rv = Base64Encode(derString, der64);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }
    if (!certChainStrings.AppendElement(NS_ConvertUTF8toUTF16(der64),
                                        fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }

  aInfo.mIssuerCommonName.Assign(issuerCommonName);
  aInfo.mCertValidityRangeNotAfter = DOMTimeStamp(notAfter / PR_USEC_PER_MSEC);
  aInfo.mCertValidityRangeNotBefore =
      DOMTimeStamp(notBefore / PR_USEC_PER_MSEC);

  int32_t errorCode;
  rv = tsi->GetErrorCode(&errorCode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsINSSErrorsService> nsserr =
      do_GetService("@mozilla.org/nss_errors_service;1");
  if (NS_WARN_IF(!nsserr)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  nsresult res;
  rv = nsserr->GetXPCOMFromNSSError(errorCode, &res);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
  rv = nsserr->GetErrorMessage(res, aInfo.mErrorMessage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  bool isPrivateBrowsing = nsContentUtils::IsInPrivateBrowsing(this);
  uint32_t flags =
      isPrivateBrowsing ? nsISocketProvider::NO_PERMANENT_STORAGE : 0;
  OriginAttributes attrs;
  attrs = nsContentUtils::GetOriginAttributes(this);
  nsCOMPtr<nsIURI> aURI;
  mFailedChannel->GetURI(getter_AddRefs(aURI));
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    MOZ_ASSERT(cc);
    cc->SendIsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI, flags, attrs,
                        &aInfo.mHasHSTS);
    cc->SendIsSecureURI(nsISiteSecurityService::HEADER_HPKP, aURI, flags, attrs,
                        &aInfo.mHasHPKP);
  } else {
    nsCOMPtr<nsISiteSecurityService> sss =
        do_GetService(NS_SSSERVICE_CONTRACTID);
    if (NS_WARN_IF(!sss)) {
      return;
    }
    Unused << NS_WARN_IF(NS_FAILED(
        sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI, flags,
                         attrs, nullptr, nullptr, &aInfo.mHasHSTS)));
    Unused << NS_WARN_IF(NS_FAILED(
        sss->IsSecureURI(nsISiteSecurityService::HEADER_HPKP, aURI, flags,
                         attrs, nullptr, nullptr, &aInfo.mHasHPKP)));
  }
}

bool Document::IsAboutPage() const {
  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  return principal->SchemeIs("about");
}

void Document::ConstructUbiNode(void* storage) {
  JS::ubi::Concrete<Document>::construct(storage, this);
}

Document::~Document() {
  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug, ("DOCUMENT %p destroyed", this));
  MOZ_ASSERT(!IsTopLevelContentDocument() || !IsResourceDoc(),
             "Can't be top-level and a resource doc at the same time");

  NS_ASSERTION(!mIsShowing, "Destroying a currently-showing document");

  if (IsTopLevelContentDocument()) {
    RemoveToplevelLoadingDocument(this);

    // don't report for about: pages
    if (!IsAboutPage()) {
      // Record the page load
      uint32_t pageLoaded = 1;
      Accumulate(Telemetry::MIXED_CONTENT_UNBLOCK_COUNTER, pageLoaded);
      // Record the mixed content status of the docshell in Telemetry
      enum {
        NO_MIXED_CONTENT = 0,  // There is no Mixed Content on the page
        MIXED_DISPLAY_CONTENT =
            1,  // The page attempted to load Mixed Display Content
        MIXED_ACTIVE_CONTENT =
            2,  // The page attempted to load Mixed Active Content
        MIXED_DISPLAY_AND_ACTIVE_CONTENT =
            3  // The page attempted to load Mixed Display & Mixed Active
               // Content
      };

      bool mixedActiveLoaded = GetHasMixedActiveContentLoaded();
      bool mixedActiveBlocked = GetHasMixedActiveContentBlocked();

      bool mixedDisplayLoaded = GetHasMixedDisplayContentLoaded();
      bool mixedDisplayBlocked = GetHasMixedDisplayContentBlocked();

      bool hasMixedDisplay = (mixedDisplayBlocked || mixedDisplayLoaded);
      bool hasMixedActive = (mixedActiveBlocked || mixedActiveLoaded);

      uint32_t mixedContentLevel = NO_MIXED_CONTENT;
      if (hasMixedDisplay && hasMixedActive) {
        mixedContentLevel = MIXED_DISPLAY_AND_ACTIVE_CONTENT;
      } else if (hasMixedActive) {
        mixedContentLevel = MIXED_ACTIVE_CONTENT;
      } else if (hasMixedDisplay) {
        mixedContentLevel = MIXED_DISPLAY_CONTENT;
      }
      Accumulate(Telemetry::MIXED_CONTENT_PAGE_LOAD, mixedContentLevel);

      // record CSP telemetry on this document
      if (mHasCSP) {
        Accumulate(Telemetry::CSP_DOCUMENTS_COUNT, 1);
      }
      if (mHasUnsafeInlineCSP) {
        Accumulate(Telemetry::CSP_UNSAFE_INLINE_DOCUMENTS_COUNT, 1);
      }
      if (mHasUnsafeEvalCSP) {
        Accumulate(Telemetry::CSP_UNSAFE_EVAL_DOCUMENTS_COUNT, 1);
      }

      if (MOZ_UNLIKELY(mMathMLEnabled)) {
        ScalarAdd(Telemetry::ScalarID::MATHML_DOC_COUNT, 1);
      }

      ScalarAdd(Telemetry::ScalarID::MEDIA_PAGE_COUNT, 1);
      if (mDocTreeHadAudibleMedia) {
        ScalarAdd(Telemetry::ScalarID::MEDIA_PAGE_HAD_MEDIA_COUNT, 1);
      }

      if (IsHTMLDocument()) {
        switch (GetCompatibilityMode()) {
          case eCompatibility_FullStandards:
            Telemetry::AccumulateCategorical(
                Telemetry::LABELS_QUIRKS_MODE::FullStandards);
            break;
          case eCompatibility_AlmostStandards:
            Telemetry::AccumulateCategorical(
                Telemetry::LABELS_QUIRKS_MODE::AlmostStandards);
            break;
          case eCompatibility_NavQuirks:
            Telemetry::AccumulateCategorical(
                Telemetry::LABELS_QUIRKS_MODE::NavQuirks);
            break;
          default:
            MOZ_ASSERT_UNREACHABLE("Unknown quirks mode");
            break;
        }
      }
    }
  }

  mInDestructor = true;
  mInUnlinkOrDeletion = true;

  mozilla::DropJSObjects(this);

  // Clear mObservers to keep it in sync with the mutationobserver list
  mObservers.Clear();

  mIntersectionObservers.Clear();

  if (mStyleSheetSetList) {
    mStyleSheetSetList->Disconnect();
  }

  if (mAnimationController) {
    mAnimationController->Disconnect();
  }

  MOZ_ASSERT(mTimelines.isEmpty());

  mParentDocument = nullptr;

  // Kill the subdocument map, doing this will release its strong
  // references, if any.
  delete mSubDocuments;
  mSubDocuments = nullptr;

  nsAutoScriptBlocker scriptBlocker;

  // Destroy link map now so we don't waste time removing
  // links one by one
  DestroyElementMaps();

  // Invalidate cached array of child nodes
  InvalidateChildNodes();

  // We should not have child nodes when destructor is called,
  // since child nodes keep their owner document alive.
  MOZ_ASSERT(!HasChildren());

  mCachedRootElement = nullptr;

  for (auto& sheets : mAdditionalSheets) {
    for (StyleSheet* sheet : sheets) {
      sheet->ClearAssociatedDocumentOrShadowRoot();
    }
  }

  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nullptr);
  }

  if (mListenerManager) {
    mListenerManager->Disconnect();
    UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  if (mScriptLoader) {
    mScriptLoader->DropDocumentReference();
  }

  if (mCSSLoader) {
    // Could be null here if Init() failed or if we have been unlinked.
    mCSSLoader->DropDocumentReference();
  }

  if (mStyleImageLoader) {
    mStyleImageLoader->DropDocumentReference();
  }

  if (mXULBroadcastManager) {
    mXULBroadcastManager->DropDocumentReference();
  }

  if (mXULPersist) {
    mXULPersist->DropDocumentReference();
  }

  if (mPermissionDelegateHandler) {
    mPermissionDelegateHandler->DropDocumentReference();
  }

  delete mHeaderData;

  mPendingTitleChangeEvent.Revoke();

  mPlugins.Clear();

  MOZ_ASSERT(mDOMMediaQueryLists.isEmpty(),
             "must not have media query lists left");

  if (mNodeInfoManager) {
    mNodeInfoManager->DropDocumentReference();
  }

  if (mDocGroup) {
    mDocGroup->RemoveDocument(this);
  }

  UnlinkOriginalDocumentIfStatic();
}

NS_INTERFACE_TABLE_HEAD(Document)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(Document, nsISupports, nsINode)
    NS_INTERFACE_TABLE_ENTRY(Document, nsINode)
    NS_INTERFACE_TABLE_ENTRY(Document, Document)
    NS_INTERFACE_TABLE_ENTRY(Document, nsIScriptObjectPrincipal)
    NS_INTERFACE_TABLE_ENTRY(Document, EventTarget)
    NS_INTERFACE_TABLE_ENTRY(Document, nsISupportsWeakReference)
    NS_INTERFACE_TABLE_ENTRY(Document, nsIRadioGroupContainer)
    NS_INTERFACE_TABLE_ENTRY(Document, nsIMutationObserver)
    NS_INTERFACE_TABLE_ENTRY(Document, nsIApplicationCacheContainer)
  NS_INTERFACE_TABLE_END
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(Document)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Document)
NS_IMETHODIMP_(MozExternalRefCountType)
Document::Release() {
  MOZ_ASSERT(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(Document);
  nsISupports* base = NS_CYCLE_COLLECTION_CLASSNAME(Document)::Upcast(this);
  bool shouldDelete = false;
  nsrefcnt count = mRefCnt.decr(base, &shouldDelete);
  NS_LOG_RELEASE(this, count, "Document");
  if (count == 0) {
    if (mStackRefCnt && !mNeedsReleaseAfterStackRefCntRelease) {
      mNeedsReleaseAfterStackRefCntRelease = true;
      NS_ADDREF_THIS();
      return mRefCnt.get();
    }
    mRefCnt.incr(base);
    LastRelease();
    mRefCnt.decr(base);
    if (shouldDelete) {
      mRefCnt.stabilizeForDeletion();
      DeleteCycleCollectable();
    }
  }
  return count;
}

NS_IMETHODIMP_(void)
Document::DeleteCycleCollectable() { delete this; }

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(Document)
  if (Element::CanSkip(tmp, aRemovingAllowed)) {
    EventListenerManager* elm = tmp->GetExistingListenerManager();
    if (elm) {
      elm->MarkForCC();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(Document)
  return Element::CanSkipInCC(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(Document)
  return Element::CanSkipThis(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(Document)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    nsAutoCString loadedAsData;
    if (tmp->IsLoadedAsData()) {
      loadedAsData.AssignLiteral("data");
    } else {
      loadedAsData.AssignLiteral("normal");
    }
    uint32_t nsid = tmp->GetDefaultNamespaceID();
    nsAutoCString uri;
    if (tmp->mDocumentURI) uri = tmp->mDocumentURI->GetSpecOrDefault();
    static const char* kNSURIs[] = {"([none])", "(xmlns)", "(xml)",
                                    "(xhtml)",  "(XLink)", "(XSLT)",
                                    "(MathML)", "(RDF)",   "(XUL)"};
    if (nsid < ArrayLength(kNSURIs)) {
      SprintfLiteral(name, "Document %s %s %s", loadedAsData.get(),
                     kNSURIs[nsid], uri.get());
    } else {
      SprintfLiteral(name, "Document %s %s", loadedAsData.get(), uri.get());
    }
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(Document, tmp->mRefCnt.get())
  }

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  tmp->mExternalResourceMap.Traverse(&cb);

  // Traverse all Document pointer members.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSecurityInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDisplayDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFontFaceSet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadyForIdle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentL10n)

  // Traverse all Document nsCOMPtrs.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheetSetList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptLoader)

  DocumentOrShadowRoot::Traverse(tmp, cb);

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOnloadBlocker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLazyLoadImageObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMImplementation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mImageMaps)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOrientationPendingPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOriginalDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedEncoder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStateObjectCached)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingAnimationTracker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTemplateContentsOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildrenCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mImages);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEmbeds);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLinks);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mForms);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScripts);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mApplets);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnchors);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnonymousContents)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCommandDispatcher)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFeaturePolicy)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSuppressedEventListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrototypeDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMidasCommandManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAll)

  // Traverse all our nsCOMArrays.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPreloadingImages)

  for (uint32_t i = 0; i < tmp->mFrameRequestCallbacks.Length(); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mFrameRequestCallbacks[i]");
    cb.NoteXPCOMChild(tmp->mFrameRequestCallbacks[i].mCallback);
  }

  // Traverse animation components
  if (tmp->mAnimationController) {
    tmp->mAnimationController->Traverse(&cb);
  }

  if (tmp->mSubDocuments) {
    for (auto iter = tmp->mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<SubDocMapEntry*>(iter.Get());

      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSubDocuments entry->mKey");
      cb.NoteXPCOMChild(entry->mKey);
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                         "mSubDocuments entry->mSubDocument");
      cb.NoteXPCOMChild(ToSupports(entry->mSubDocument));
    }
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCSSLoader)

  // We own only the items in mDOMMediaQueryLists that have listeners;
  // this reference is managed by their AddListener and RemoveListener
  // methods.
  for (MediaQueryList* mql = tmp->mDOMMediaQueryLists.getFirst(); mql;
       mql = static_cast<LinkedListElement<MediaQueryList>*>(mql)->getNext()) {
    if (mql->HasListeners() &&
        NS_SUCCEEDED(mql->CheckCurrentGlobalCorrectness())) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mDOMMediaQueryLists item");
      cb.NoteXPCOMChild(mql);
    }
  }

  if (tmp->mResizeObserverController) {
    tmp->mResizeObserverController->Traverse(cb);
  }
  for (size_t i = 0; i < tmp->mMetaViewports.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMetaViewports[i].mElement);
  }

  // XXX: This should be not needed once bug 1569185 lands.
  for (auto it = tmp->mL10nProtoElements.ConstIter(); !it.Done(); it.Next()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mL10nProtoElements key");
    cb.NoteXPCOMChild(it.Key());
    CycleCollectionNoteChild(cb, it.UserData(), "mL10nProtoElements value");
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_CLASS(Document)

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Document)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Document)
  tmp->mInUnlinkOrDeletion = true;

  // Clear out our external resources
  tmp->mExternalResourceMap.Shutdown();

  nsAutoScriptBlocker scriptBlocker;

  nsINode::Unlink(tmp);

  while (tmp->HasChildren()) {
    // Hold a strong ref to the node when we remove it, because we may be
    // the last reference to it.
    // If this code changes, change the corresponding code in Document's
    // unlink impl and ContentUnbinder::UnbindSubtree.
    nsCOMPtr<nsIContent> child = tmp->GetLastChild();
    tmp->DisconnectChild(child);
    child->UnbindFromTree();
  }

  tmp->UnlinkOriginalDocumentIfStatic();

  tmp->mCachedRootElement = nullptr;  // Avoid a dangling pointer

  tmp->SetScriptGlobalObject(nullptr);

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSecurityInfo)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDisplayDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLazyLoadImageObserver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFontFaceSet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadyForIdle)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentL10n)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOnloadBlocker)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMImplementation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mImageMaps)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOrientationPendingPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOriginalDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedEncoder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStateObjectCached)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentTimeline)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingAnimationTracker)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTemplateContentsOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildrenCollection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mImages);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEmbeds);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLinks);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mForms);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScripts);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mApplets);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnchors);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnonymousContents)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCommandDispatcher)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFeaturePolicy)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSuppressedEventListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrototypeDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMidasCommandManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAll)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReferrerInfo)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPreloadReferrerInfo)

  if (tmp->IsTopLevelContentDocument()) {
    RemoveToplevelLoadingDocument(tmp);
  }

  tmp->mParentDocument = nullptr;

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPreloadingImages)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIntersectionObservers)

  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    tmp->UnsetFlags(NODE_HAS_LISTENERMANAGER);
    tmp->mListenerManager = nullptr;
  }

  if (tmp->mStyleSheetSetList) {
    tmp->mStyleSheetSetList->Disconnect();
    tmp->mStyleSheetSetList = nullptr;
  }

  delete tmp->mSubDocuments;
  tmp->mSubDocuments = nullptr;

  tmp->mFrameRequestCallbacks.Clear();
  MOZ_RELEASE_ASSERT(!tmp->mFrameRequestCallbacksScheduled,
                     "How did we get here without our presshell going away "
                     "first?");

  DocumentOrShadowRoot::Unlink(tmp);

  // Document has a pretty complex destructor, so we're going to
  // assume that *most* cycles you actually want to break somewhere
  // else, and not unlink an awful lot here.

  tmp->mExpandoAndGeneration.OwnerUnlinked();

  if (tmp->mAnimationController) {
    tmp->mAnimationController->Unlink();
  }

  tmp->mPendingTitleChangeEvent.Revoke();

  if (tmp->mCSSLoader) {
    tmp->mCSSLoader->DropDocumentReference();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mCSSLoader)
  }

  // We own only the items in mDOMMediaQueryLists that have listeners;
  // this reference is managed by their AddListener and RemoveListener
  // methods.
  for (MediaQueryList* mql = tmp->mDOMMediaQueryLists.getFirst(); mql;) {
    MediaQueryList* next =
        static_cast<LinkedListElement<MediaQueryList>*>(mql)->getNext();
    mql->Disconnect();
    mql = next;
  }

  tmp->mInUnlinkOrDeletion = false;

  if (tmp->mResizeObserverController) {
    tmp->mResizeObserverController->Unlink();
  }
  tmp->mMetaViewports.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mL10nProtoElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsresult Document::Init() {
  if (mCSSLoader || mStyleImageLoader || mNodeInfoManager || mScriptLoader) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // Force initialization.
  nsINode::nsSlots* slots = Slots();

  // Prepend self as mutation-observer whether we need it or not (some
  // subclasses currently do, other don't). This is because the code in
  // MutationObservers always notifies the first observer first, expecting the
  // first observer to be the document.
  slots->mMutationObservers.PrependElementUnlessExists(
      static_cast<nsIMutationObserver*>(this));

  mOnloadBlocker = new OnloadBlocker();
  mCSSLoader = new css::Loader(this);
  // Assume we're not quirky, until we know otherwise
  mCSSLoader->SetCompatibilityMode(eCompatibility_FullStandards);

  mStyleImageLoader = new css::ImageLoader(this);

  mNodeInfoManager = new nsNodeInfoManager();
  nsresult rv = mNodeInfoManager->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // mNodeInfo keeps NodeInfoManager alive!
  mNodeInfo = mNodeInfoManager->GetDocumentNodeInfo();
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_OUT_OF_MEMORY);
  MOZ_ASSERT(mNodeInfo->NodeType() == DOCUMENT_NODE,
             "Bad NodeType in aNodeInfo");

  NS_ASSERTION(OwnerDoc() == this, "Our nodeinfo is busted!");

  // If after creation the owner js global is not set for a document
  // we use the default compartment for this document, instead of creating
  // wrapper in some random compartment when the document is exposed to js
  // via some events.
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());
  NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);
  mScopeObject = do_GetWeakReference(global);
  MOZ_ASSERT(mScopeObject);

  mScriptLoader = new dom::ScriptLoader(this);

  // we need to create a policy here so getting the policy within
  // ::Policy() can *always* return a non null policy
  mFeaturePolicy = new dom::FeaturePolicy(this);
  mFeaturePolicy->SetDefaultOrigin(NodePrincipal());

  mStyleSet = MakeUnique<ServoStyleSet>(*this);

  mozilla::HoldJSObjects(this);

  return NS_OK;
}

void Document::RemoveAllProperties() { PropertyTable().RemoveAllProperties(); }

void Document::RemoveAllPropertiesFor(nsINode* aNode) {
  PropertyTable().RemoveAllPropertiesFor(aNode);
}

bool Document::IsVisibleConsideringAncestors() const {
  const Document* parent = this;
  do {
    if (!parent->IsVisible()) {
      return false;
    }
  } while ((parent = parent->GetInProcessParentDocument()));

  return true;
}

void Document::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) {
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIPrincipal> storagePrincipal;
  if (aChannel) {
    // Note: this code is duplicated in PrototypeDocumentContentSink::Init and
    // nsScriptSecurityManager::GetChannelResultPrincipals.
    // Note: this should match nsDocShell::OnLoadingSite
    NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));

    nsIScriptSecurityManager* securityManager =
        nsContentUtils::GetSecurityManager();
    if (securityManager) {
      securityManager->GetChannelResultPrincipals(
          aChannel, getter_AddRefs(principal),
          getter_AddRefs(storagePrincipal));
    }
  }

  bool equal = principal->Equals(storagePrincipal);

  principal = MaybeDowngradePrincipal(principal);
  if (equal) {
    storagePrincipal = principal;
  } else {
    storagePrincipal = MaybeDowngradePrincipal(storagePrincipal);
  }

  ResetToURI(uri, aLoadGroup, principal, storagePrincipal);

  // Note that, since mTiming does not change during a reset, the
  // navigationStart time remains unchanged and therefore any future new
  // timeline will have the same global clock time as the old one.
  mDocumentTimeline = nullptr;

  nsCOMPtr<nsIPropertyBag2> bag = do_QueryInterface(aChannel);
  if (bag) {
    nsCOMPtr<nsIURI> baseURI;
    bag->GetPropertyAsInterface(NS_LITERAL_STRING("baseURI"),
                                NS_GET_IID(nsIURI), getter_AddRefs(baseURI));
    if (baseURI) {
      mDocumentBaseURI = baseURI;
      mChromeXHRDocBaseURI = nullptr;
    }
  }

  mChannel = aChannel;
}

void Document::DisconnectNodeTree() {
  // Delete references to sub-documents and kill the subdocument map,
  // if any. This is not strictly needed, but makes the node tree
  // teardown a bit faster.
  delete mSubDocuments;
  mSubDocuments = nullptr;

  bool oldVal = mInUnlinkOrDeletion;
  mInUnlinkOrDeletion = true;
  {  // Scope for update
    MOZ_AUTO_DOC_UPDATE(this, true);

    // Destroy link map now so we don't waste time removing
    // links one by one
    DestroyElementMaps();

    // Invalidate cached array of child nodes
    InvalidateChildNodes();

    while (HasChildren()) {
      nsMutationGuard::DidMutate();
      nsCOMPtr<nsIContent> content = GetLastChild();
      nsIContent* previousSibling = content->GetPreviousSibling();
      DisconnectChild(content);
      if (content == mCachedRootElement) {
        // Immediately clear mCachedRootElement, now that it's been removed
        // from mChildren, so that GetRootElement() will stop returning this
        // now-stale value.
        mCachedRootElement = nullptr;
      }
      MutationObservers::NotifyContentRemoved(this, content, previousSibling);
      content->UnbindFromTree();
    }
    MOZ_ASSERT(!mCachedRootElement,
               "After removing all children, there should be no root elem");
  }
  mInUnlinkOrDeletion = oldVal;
}

void Document::ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal,
                          nsIPrincipal* aStoragePrincipal) {
  MOZ_ASSERT(aURI, "Null URI passed to ResetToURI");
  MOZ_ASSERT(!!aPrincipal == !!aStoragePrincipal);

  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug,
          ("DOCUMENT %p ResetToURI %s", this, aURI->GetSpecOrDefault().get()));

  mSecurityInfo = nullptr;

  nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
  if (!aLoadGroup || group != aLoadGroup) {
    mDocumentLoadGroup = nullptr;
  }

  DisconnectNodeTree();

  // Reset our stylesheets
  ResetStylesheetsToURI(aURI);

  // Release the listener manager
  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nullptr;
  }

  // Release the stylesheets list.
  mDOMStyleSheets = nullptr;

  // Release our principal after tearing down the document, rather than before.
  // This ensures that, during teardown, the document and the dying window
  // (which already nulled out its document pointer and cached the principal)
  // have matching principals.
  SetPrincipals(nullptr, nullptr);

  // Clear the original URI so SetDocumentURI sets it.
  mOriginalURI = nullptr;

  SetDocumentURI(aURI);
  mChromeXHRDocURI = nullptr;
  // If mDocumentBaseURI is null, Document::GetBaseURI() returns
  // mDocumentURI.
  mDocumentBaseURI = nullptr;
  mChromeXHRDocBaseURI = nullptr;

  // Check if the current document is the top-level DevTools document.
  // For inner DevTools frames, mIsDevToolsDocument will be set when
  // calling SetDocumentParent.
  if (aURI && aURI->SchemeIs("about") &&
      aURI->GetSpecOrDefault().EqualsLiteral("about:devtools-toolbox")) {
    mIsDevToolsDocument = true;
  }

  if (aLoadGroup) {
    mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);
    // there was an assertion here that aLoadGroup was not null.  This
    // is no longer valid: nsDocShell::SetDocument does not create a
    // load group, and it works just fine

    // XXXbz what does "just fine" mean exactly?  And given that there
    // is no nsDocShell::SetDocument, what is this talking about?

    if (IsContentDocument()) {
      // Inform the associated request context about this load start so
      // any of its internal load progress flags gets reset.
      nsCOMPtr<nsIRequestContextService> rcsvc =
          net::RequestContextService::GetOrCreate();
      if (rcsvc) {
        nsCOMPtr<nsIRequestContext> rc;
        rcsvc->GetRequestContextFromLoadGroup(aLoadGroup, getter_AddRefs(rc));
        if (rc) {
          rc->BeginLoad();
        }
      }
    }
  }

  mLastModified.Truncate();
  // XXXbz I guess we're assuming that the caller will either pass in
  // a channel with a useful type or call SetContentType?
  SetContentTypeInternal(EmptyCString());
  mContentLanguage.Truncate();
  mBaseTarget.Truncate();

  mXMLDeclarationBits = 0;

  // Now get our new principal
  if (aPrincipal) {
    SetPrincipals(aPrincipal, aStoragePrincipal);
  } else {
    nsIScriptSecurityManager* securityManager =
        nsContentUtils::GetSecurityManager();
    if (securityManager) {
      nsCOMPtr<nsILoadContext> loadContext(mDocumentContainer);

      if (!loadContext && aLoadGroup) {
        nsCOMPtr<nsIInterfaceRequestor> cbs;
        aLoadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
        loadContext = do_GetInterface(cbs);
      }

      MOZ_ASSERT(loadContext,
                 "must have a load context or pass in an explicit principal");

      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = securityManager->GetLoadContextContentPrincipal(
          mDocumentURI, loadContext, getter_AddRefs(principal));
      if (NS_SUCCEEDED(rv)) {
        SetPrincipals(principal, principal);
      }
    }
  }

  if (mFontFaceSet) {
    mFontFaceSet->RefreshStandardFontLoadPrincipal();
  }

  // Refresh the principal on the realm.
  if (nsPIDOMWindowInner* win = GetInnerWindow()) {
    nsGlobalWindowInner::Cast(win)->RefreshRealmPrincipal();
  }
}

already_AddRefed<nsIPrincipal> Document::MaybeDowngradePrincipal(
    nsIPrincipal* aPrincipal) {
  if (!aPrincipal) {
    return nullptr;
  }

  // We can't load a document with an expanded principal. If we're given one,
  // automatically downgrade it to the last principal it subsumes (which is the
  // extension principal, in the case of extension content scripts).
  auto* basePrin = BasePrincipal::Cast(aPrincipal);
  if (basePrin->Is<ExpandedPrincipal>()) {
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "Should never try to create a document with "
                          "an expanded principal");

    auto* expanded = basePrin->As<ExpandedPrincipal>();
    return do_AddRef(expanded->AllowList().LastElement());
  }

  if (aPrincipal->IsSystemPrincipal()) {
    // We basically want the parent document here, but because this is very
    // early in the load, GetInProcessParentDocument() returns null, so we use
    // the docshell hierarchy to get this information instead.
    if (mDocumentContainer) {
      nsCOMPtr<nsIDocShellTreeItem> parentDocShellItem;
      mDocumentContainer->GetInProcessParent(
          getter_AddRefs(parentDocShellItem));
      nsCOMPtr<nsIDocShell> parentDocShell =
          do_QueryInterface(parentDocShellItem);
      if (parentDocShell) {
        nsCOMPtr<Document> parentDoc;
        parentDoc = parentDocShell->GetDocument();
        if (!parentDoc || !parentDoc->NodePrincipal()->IsSystemPrincipal()) {
          nsCOMPtr<nsIPrincipal> nullPrincipal =
              do_CreateInstance("@mozilla.org/nullprincipal;1");
          return nullPrincipal.forget();
        }
      }
    }
  }
  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  return principal.forget();
}

size_t Document::FindDocStyleSheetInsertionPoint(const StyleSheet& aSheet) {
  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();

  // lowest index first
  int32_t newDocIndex = StyleOrderIndexOfSheet(aSheet);

  size_t count = mStyleSet->SheetCount(StyleOrigin::Author);
  size_t index = 0;
  for (; index < count; index++) {
    auto* sheet = mStyleSet->SheetAt(StyleOrigin::Author, index);
    MOZ_ASSERT(sheet);
    int32_t sheetDocIndex = StyleOrderIndexOfSheet(*sheet);
    if (sheetDocIndex > newDocIndex) {
      break;
    }

    // If the sheet is not owned by the document it can be an author
    // sheet registered at nsStyleSheetService or an additional author
    // sheet on the document, which means the new
    // doc sheet should end up before it.
    if (sheetDocIndex < 0) {
      if (sheetService) {
        auto& authorSheets = *sheetService->AuthorStyleSheets();
        if (authorSheets.IndexOf(sheet) != authorSheets.NoIndex) {
          break;
        }
      }
      if (sheet == GetFirstAdditionalAuthorSheet()) {
        break;
      }
    }
  }

  return index;
}

void Document::ResetStylesheetsToURI(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  ClearAdoptedStyleSheets();

  auto ClearSheetList = [&](nsTArray<RefPtr<StyleSheet>>& aSheetList) {
    for (auto& sheet : Reversed(aSheetList)) {
      sheet->ClearAssociatedDocumentOrShadowRoot();
      if (mStyleSetFilled) {
        mStyleSet->RemoveStyleSheet(*sheet);
      }
    }
    aSheetList.Clear();
  };
  ClearSheetList(mStyleSheets);
  for (auto& sheets : mAdditionalSheets) {
    ClearSheetList(sheets);
  }
  if (mStyleSetFilled) {
    if (auto* ss = nsStyleSheetService::GetInstance()) {
      for (auto& sheet : Reversed(*ss->AuthorStyleSheets())) {
        MOZ_ASSERT(!sheet->GetAssociatedDocumentOrShadowRoot());
        if (sheet->IsApplicable()) {
          mStyleSet->RemoveStyleSheet(*sheet);
        }
      }
    }
  }

  // Now reset our inline style and attribute sheets.
  if (mAttrStyleSheet) {
    mAttrStyleSheet->Reset();
    mAttrStyleSheet->SetOwningDocument(this);
  } else {
    mAttrStyleSheet = new nsHTMLStyleSheet(this);
  }

  if (!mStyleAttrStyleSheet) {
    mStyleAttrStyleSheet = new nsHTMLCSSStyleSheet();
  }

  if (mStyleSetFilled) {
    FillStyleSetDocumentSheets();

    if (mStyleSet->StyleSheetsHaveChanged()) {
      ApplicableStylesChanged();
    }
  }
}

static void AppendSheetsToStyleSet(
    ServoStyleSet* aStyleSet, const nsTArray<RefPtr<StyleSheet>>& aSheets) {
  for (StyleSheet* sheet : Reversed(aSheets)) {
    aStyleSet->AppendStyleSheet(*sheet);
  }
}

void Document::FillStyleSetUserAndUASheets() {
  // Make sure this does the same thing as PresShell::Add{User,Agent}Sheet wrt
  // ordering.

  // The document will fill in the document sheets when we create the presshell
  auto* cache = GlobalStyleSheetCache::Singleton();

  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();
  MOZ_ASSERT(sheetService,
             "should never be creating a StyleSet after the style sheet "
             "service has gone");

  for (StyleSheet* sheet : *sheetService->UserStyleSheets()) {
    mStyleSet->AppendStyleSheet(*sheet);
  }

  StyleSheet* sheet = IsInChromeDocShell() ? cache->GetUserChromeSheet()
                                           : cache->GetUserContentSheet();
  if (sheet) {
    mStyleSet->AppendStyleSheet(*sheet);
  }

  mStyleSet->AppendStyleSheet(*cache->UASheet());

  if (MOZ_LIKELY(NodeInfoManager()->MathMLEnabled())) {
    mStyleSet->AppendStyleSheet(*cache->MathMLSheet());
  }

  if (MOZ_LIKELY(NodeInfoManager()->SVGEnabled())) {
    mStyleSet->AppendStyleSheet(*cache->SVGSheet());
  }

  mStyleSet->AppendStyleSheet(*cache->HTMLSheet());

  if (nsLayoutUtils::ShouldUseNoFramesSheet(this)) {
    mStyleSet->AppendStyleSheet(*cache->NoFramesSheet());
  }

  if (nsLayoutUtils::ShouldUseNoScriptSheet(this)) {
    mStyleSet->AppendStyleSheet(*cache->NoScriptSheet());
  }

  mStyleSet->AppendStyleSheet(*cache->CounterStylesSheet());

  // Load the minimal XUL rules for scrollbars and a few other XUL things
  // that non-XUL (typically HTML) documents commonly use.
  mStyleSet->AppendStyleSheet(*cache->MinimalXULSheet());

  // Only load the full XUL sheet if we'll need it.
  if (LoadsFullXULStyleSheetUpFront()) {
    mStyleSet->AppendStyleSheet(*cache->XULSheet());
  }

  mStyleSet->AppendStyleSheet(*cache->FormsSheet());
  mStyleSet->AppendStyleSheet(*cache->ScrollbarsSheet());
  mStyleSet->AppendStyleSheet(*cache->PluginProblemSheet());

  if (StyleSheet* sheet = cache->GetGeckoViewSheet()) {
    if (!StaticPrefs::widget_disable_native_theme_for_content()) {
      mStyleSet->AppendStyleSheet(*sheet);
    }
  }

  for (StyleSheet* sheet : *sheetService->AgentStyleSheets()) {
    mStyleSet->AppendStyleSheet(*sheet);
  }

  MOZ_ASSERT(!mQuirkSheetAdded);
  if (NeedsQuirksSheet()) {
    mStyleSet->AppendStyleSheet(*cache->QuirkSheet());
    mQuirkSheetAdded = true;
  }
}

void Document::FillStyleSet() {
  MOZ_ASSERT(!mStyleSetFilled);
  FillStyleSetUserAndUASheets();
  FillStyleSetDocumentSheets();
  mStyleSetFilled = true;
}

void Document::RemoveContentEditableStyleSheets() {
  MOZ_ASSERT(IsHTMLOrXHTML());

  auto* cache = GlobalStyleSheetCache::Singleton();
  bool changed = false;
  if (mDesignModeSheetAdded) {
    mStyleSet->RemoveStyleSheet(*cache->DesignModeSheet());
    mDesignModeSheetAdded = false;
    changed = true;
  }
  if (mContentEditableSheetAdded) {
    mStyleSet->RemoveStyleSheet(*cache->ContentEditableSheet());
    mContentEditableSheetAdded = false;
    changed = true;
  }
  if (changed) {
    MOZ_ASSERT(mStyleSetFilled);
    ApplicableStylesChanged();
  }
}

void Document::AddContentEditableStyleSheetsToStyleSet(bool aDesignMode) {
  MOZ_ASSERT(IsHTMLOrXHTML());
  MOZ_DIAGNOSTIC_ASSERT(mStyleSetFilled,
                        "Caller should ensure we're being rendered");

  auto* cache = GlobalStyleSheetCache::Singleton();
  bool changed = false;
  if (!mContentEditableSheetAdded) {
    mStyleSet->AppendStyleSheet(*cache->ContentEditableSheet());
    mContentEditableSheetAdded = true;
    changed = true;
  }
  if (mDesignModeSheetAdded != aDesignMode) {
    if (mDesignModeSheetAdded) {
      mStyleSet->RemoveStyleSheet(*cache->DesignModeSheet());
    } else {
      mStyleSet->AppendStyleSheet(*cache->DesignModeSheet());
    }
    mDesignModeSheetAdded = !mDesignModeSheetAdded;
    changed = true;
  }
  if (changed) {
    ApplicableStylesChanged();
  }
}

void Document::FillStyleSetDocumentSheets() {
  MOZ_ASSERT(mStyleSet->SheetCount(StyleOrigin::Author) == 0,
             "Style set already has document sheets?");

  // Sheets are added in reverse order to avoid worst-case time complexity when
  // looking up the index of a sheet.
  //
  // Note that usually appending is faster (rebuilds less stuff in the
  // styleset), but in this case it doesn't matter since we're filling the
  // styleset from scratch anyway.
  for (StyleSheet* sheet : Reversed(mStyleSheets)) {
    if (sheet->IsApplicable()) {
      mStyleSet->AddDocStyleSheet(*sheet);
    }
  }

  EnumerateUniqueAdoptedStyleSheetsBackToFront([&](StyleSheet& aSheet) {
    if (aSheet.IsApplicable()) {
      mStyleSet->AddDocStyleSheet(aSheet);
    }
  });

  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();
  for (StyleSheet* sheet : *sheetService->AuthorStyleSheets()) {
    mStyleSet->AppendStyleSheet(*sheet);
  }

  AppendSheetsToStyleSet(mStyleSet.get(), mAdditionalSheets[eAgentSheet]);
  AppendSheetsToStyleSet(mStyleSet.get(), mAdditionalSheets[eUserSheet]);
  AppendSheetsToStyleSet(mStyleSet.get(), mAdditionalSheets[eAuthorSheet]);
}

void Document::CompatibilityModeChanged() {
  MOZ_ASSERT(IsHTMLOrXHTML());
  CSSLoader()->SetCompatibilityMode(mCompatMode);
  mStyleSet->CompatibilityModeChanged();
  if (PresShell* presShell = GetPresShell()) {
    // Selectors may have become case-sensitive / case-insensitive, the stylist
    // has already performed the relevant invalidation.
    presShell->EnsureStyleFlush();
  }
  if (!mStyleSetFilled) {
    MOZ_ASSERT(!mQuirkSheetAdded);
    return;
  }
  if (mQuirkSheetAdded == NeedsQuirksSheet()) {
    return;
  }
  auto* cache = GlobalStyleSheetCache::Singleton();
  StyleSheet* sheet = cache->QuirkSheet();
  if (mQuirkSheetAdded) {
    mStyleSet->RemoveStyleSheet(*sheet);
  } else {
    mStyleSet->AppendStyleSheet(*sheet);
  }
  mQuirkSheetAdded = !mQuirkSheetAdded;
  ApplicableStylesChanged();
}

void Document::SetCompatibilityMode(nsCompatibility aMode) {
  NS_ASSERTION(IsHTMLDocument() || aMode == eCompatibility_FullStandards,
               "Bad compat mode for XHTML document!");

  if (mCompatMode == aMode) {
    return;
  }
  mCompatMode = aMode;
  CompatibilityModeChanged();
}

static void WarnIfSandboxIneffective(nsIDocShell* aDocShell,
                                     uint32_t aSandboxFlags,
                                     nsIChannel* aChannel) {
  // If the document is sandboxed (via the HTML5 iframe sandbox
  // attribute) and both the allow-scripts and allow-same-origin
  // keywords are supplied, the sandboxed document can call into its
  // parent document and remove its sandboxing entirely - we print a
  // warning to the web console in this case.
  if (aSandboxFlags & SANDBOXED_NAVIGATION &&
      !(aSandboxFlags & SANDBOXED_SCRIPTS) &&
      !(aSandboxFlags & SANDBOXED_ORIGIN)) {
    RefPtr<BrowsingContext> bc = aDocShell->GetBrowsingContext();
    MOZ_ASSERT(bc->IsInProcess());

    RefPtr<BrowsingContext> parentBC = bc->GetParent();
    if (!parentBC || !parentBC->IsInProcess()) {
      // If parent document is not in process, then by construction it
      // cannot be same origin.
      return;
    }

    // Don't warn if our parent is not the top-level document.
    if (!parentBC->IsTopContent()) {
      return;
    }

    nsCOMPtr<nsIDocShell> parentDocShell = parentBC->GetDocShell();
    MOZ_ASSERT(parentDocShell);

    nsCOMPtr<nsIChannel> parentChannel;
    parentDocShell->GetCurrentDocumentChannel(getter_AddRefs(parentChannel));
    if (!parentChannel) {
      return;
    }
    nsresult rv = nsContentUtils::CheckSameOrigin(aChannel, parentChannel);
    if (NS_FAILED(rv)) {
      return;
    }

    nsCOMPtr<Document> parentDocument = parentDocShell->GetDocument();
    nsCOMPtr<nsIURI> iframeUri;
    parentChannel->GetURI(getter_AddRefs(iframeUri));
    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Iframe Sandbox"),
        parentDocument, nsContentUtils::eSECURITY_PROPERTIES,
        "BothAllowScriptsAndSameOriginPresent", nsTArray<nsString>(),
        iframeUri);
  }
}

bool Document::IsSynthesized() {
  nsCOMPtr<nsILoadInfo> loadInfo = mChannel ? mChannel->LoadInfo() : nullptr;
  return loadInfo && loadInfo->GetServiceWorkerTaintingSynthesized();
}

// static
bool Document::IsCallerChromeOrAddon(JSContext* aCx, JSObject* aObject) {
  nsIPrincipal* principal = nsContentUtils::SubjectPrincipal(aCx);
  return principal && (principal->IsSystemPrincipal() ||
                       principal->GetIsAddonOrExpandedAddonPrincipal());
}

nsresult Document::StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool aReset, nsIContentSink* aSink) {
  if (MOZ_LOG_TEST(gDocumentLeakPRLog, LogLevel::Debug)) {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug,
            ("DOCUMENT %p StartDocumentLoad %s", this,
             uri ? uri->GetSpecOrDefault().get() : ""));
  }

  MOZ_ASSERT(GetReadyStateEnum() == Document::READYSTATE_UNINITIALIZED,
             "Bad readyState");
  SetReadyStateInternal(READYSTATE_LOADING);

  if (nsCRT::strcmp(kLoadAsData, aCommand) == 0) {
    mLoadedAsData = true;
    // We need to disable script & style loading in this case.
    // We leave them disabled even in EndLoad(), and let anyone
    // who puts the document on display to worry about enabling.

    // Do not load/process scripts when loading as data
    ScriptLoader()->SetEnabled(false);

    // styles
    CSSLoader()->SetEnabled(
        false);  // Do not load/process styles when loading as data
  } else if (nsCRT::strcmp("external-resource", aCommand) == 0) {
    // Allow CSS, but not scripts
    ScriptLoader()->SetEnabled(false);
  }

  mMayStartLayout = false;
  MOZ_ASSERT(!mReadyForIdle,
             "We should never hit DOMContentLoaded before this point");

  if (aReset) {
    Reset(aChannel, aLoadGroup);
  }

  nsAutoCString contentType;
  nsCOMPtr<nsIPropertyBag2> bag = do_QueryInterface(aChannel);
  if ((bag && NS_SUCCEEDED(bag->GetPropertyAsACString(
                  NS_LITERAL_STRING("contentType"), contentType))) ||
      NS_SUCCEEDED(aChannel->GetContentType(contentType))) {
    // XXX this is only necessary for viewsource:
    nsACString::const_iterator start, end, semicolon;
    contentType.BeginReading(start);
    contentType.EndReading(end);
    semicolon = start;
    FindCharInReadable(';', semicolon, end);
    SetContentTypeInternal(Substring(start, semicolon));
  }

  RetrieveRelevantHeaders(aChannel);

  mChannel = aChannel;
  nsCOMPtr<nsIInputStreamChannel> inStrmChan = do_QueryInterface(mChannel);
  if (inStrmChan) {
    bool isSrcdocChannel;
    inStrmChan->GetIsSrcdocChannel(&isSrcdocChannel);
    if (isSrcdocChannel) {
      mIsSrcdocDocument = true;
    }
  }

  if (mChannel) {
    nsLoadFlags loadFlags;
    mChannel->GetLoadFlags(&loadFlags);
    bool isDocument = false;
    mChannel->GetIsDocument(&isDocument);
    if (loadFlags & nsIRequest::LOAD_DOCUMENT_NEEDS_COOKIE && isDocument &&
        IsSynthesized() && XRE_IsContentProcess()) {
      ContentChild::UpdateCookieStatus(mChannel);
    }

    // Store the security info for future use.
    mChannel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));
  }

  // If this document is being loaded by a docshell, copy its sandbox flags
  // to the document, and store the fullscreen enabled flag. These are
  // immutable after being set here.
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aContainer);

  // If this is an error page, don't inherit sandbox flags
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (docShell && !loadInfo->GetLoadErrorPage()) {
    mSandboxFlags = loadInfo->GetSandboxFlags();
    WarnIfSandboxIneffective(docShell, mSandboxFlags, GetChannel());
  }

  // The CSP directives upgrade-insecure-requests as well as
  // block-all-mixed-content not only apply to the toplevel document,
  // but also to nested documents. The loadInfo of a subdocument
  // load already holds the correct flag, so let's just set it here
  // on the document. Please note that we set the appropriate preload
  // bits just for the sake of completeness here, because the preloader
  // does not reach into subdocuments.
  mUpgradeInsecureRequests = loadInfo->GetUpgradeInsecureRequests();
  mUpgradeInsecurePreloads = mUpgradeInsecureRequests;
  mBlockAllMixedContent = loadInfo->GetBlockAllMixedContent();
  mBlockAllMixedContentPreloads = mBlockAllMixedContent;

  nsresult rv = InitReferrerInfo(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check CSP navigate-to
  // We need to enforce the CSP of the document that initiated the load,
  // which is the CSP to inherit.
  nsCOMPtr<nsIContentSecurityPolicy> cspToInherit = loadInfo->GetCspToInherit();
  if (cspToInherit) {
    bool allowsNavigateTo = false;
    rv = cspToInherit->GetAllowsNavigateTo(
        mDocumentURI, loadInfo->GetIsFormSubmission(),
        !loadInfo->RedirectChain().IsEmpty(), /* aWasRedirected */
        true,                                 /* aEnforceWhitelist */
        &allowsNavigateTo);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!allowsNavigateTo) {
      aChannel->Cancel(NS_ERROR_CSP_NAVIGATE_TO_VIOLATION);
      return NS_OK;
    }
  }

  rv = InitCSP(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bug 1574372: Download should be fully done in the parent process.
  // Unfortunately we currently can not determine whether a load will
  // result in a download in the parent process. Hence, if running in
  // non-fission-mode then we will have to enforce checks for
  // frame-ancestors and x-frame-options here in the content process
  // but if we run in fission-mode then we do those two security
  // checks within DOMSecurityManager::Observe in the parent.
  bool fissionEnabled =
      docShell && nsDocShell::Cast(docShell)->UseRemoteSubframes();
  if (!fissionEnabled) {
    nsContentPolicyType contentType = loadInfo->GetExternalContentPolicyType();
    // frame-ancestor check only makes sense for subdocument and object loads,
    // if this is not a load of such type, there is nothing to do here.
    if (contentType == nsIContentPolicy::TYPE_SUBDOCUMENT ||
        contentType == nsIContentPolicy::TYPE_OBJECT) {
      // we only enforce frame-ancestors if the load is an actual http
      // channel, otherwise we block dynamic iframes since about:blank
      // inherits the CSP.
      nsCOMPtr<nsIHttpChannel> httpChannel;
      nsContentSecurityUtils::GetHttpChannelFromPotentialMultiPart(
          aChannel, getter_AddRefs(httpChannel));
      if (httpChannel && mCSP) {
        bool safeAncestry = false;
        // PermitsAncestry sends violation reports when necessary
        rv = mCSP->PermitsAncestry(loadInfo, &safeAncestry);
        if (NS_FAILED(rv) || !safeAncestry) {
          // stop!  ERROR page!
          aChannel->Cancel(NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION);
        }
      }
    }

    // the checks for type_subdoc or type_object happen within
    // CheckFrameOptions.
    if (!FramingChecker::CheckFrameOptions(aChannel, mCSP)) {
      // stop!  ERROR page!
      // But before we have to reset the principal of the document
      // because the onload() event fires before the error page
      // is displayed and we do not want the enclosing document
      // to access the contentDocument.
      RefPtr<NullPrincipal> nullPrincipal =
          NullPrincipal::CreateWithInheritedAttributes(NodePrincipal());
      // Before calling SetPrincipals() we should ensure that mFontFaceSet
      // and also GetInnerWindow() is still null at this point, before
      // we can fix Bug 1614735: Evaluate calls to SetPrincipal
      // within Document.cpp
      MOZ_ASSERT(!mFontFaceSet && !GetInnerWindow());
      SetPrincipals(nullPrincipal, nullPrincipal);
      aChannel->Cancel(NS_ERROR_XFO_VIOLATION);
    }
  }

  // Initialize FeaturePolicy
  rv = InitFeaturePolicy(aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = loadInfo->GetCookieJarSettings(getter_AddRefs(mCookieJarSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  mHasStoragePermission = loadInfo->GetHasStoragePermission();

  return NS_OK;
}

nsIContentSecurityPolicy* Document::GetCsp() const { return mCSP; }

void Document::SetCsp(nsIContentSecurityPolicy* aCSP) { mCSP = aCSP; }

nsIContentSecurityPolicy* Document::GetPreloadCsp() const {
  return mPreloadCSP;
}

void Document::SetPreloadCsp(nsIContentSecurityPolicy* aPreloadCSP) {
  mPreloadCSP = aPreloadCSP;
}

void Document::GetCspJSON(nsString& aJSON) {
  aJSON.Truncate();

  if (!mCSP) {
    dom::CSPPolicies jsonPolicies;
    jsonPolicies.ToJSON(aJSON);
    return;
  }
  mCSP->ToJSON(aJSON);
}

void Document::SendToConsole(nsCOMArray<nsISecurityConsoleMessage>& aMessages) {
  for (uint32_t i = 0; i < aMessages.Length(); ++i) {
    nsAutoString messageTag;
    aMessages[i]->GetTag(messageTag);

    nsAutoString category;
    aMessages[i]->GetCategory(category);

    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_ConvertUTF16toUTF8(category), this,
                                    nsContentUtils::eSECURITY_PROPERTIES,
                                    NS_ConvertUTF16toUTF8(messageTag).get());
  }
}

void Document::ApplySettingsFromCSP(bool aSpeculative) {
  nsresult rv = NS_OK;
  if (!aSpeculative) {
    // 1) apply settings from regular CSP
    if (mCSP) {
      // Set up 'block-all-mixed-content' if not already inherited
      // from the parent context or set by any other CSP.
      if (!mBlockAllMixedContent) {
        rv = mCSP->GetBlockAllMixedContent(&mBlockAllMixedContent);
        NS_ENSURE_SUCCESS_VOID(rv);
      }
      if (!mBlockAllMixedContentPreloads) {
        mBlockAllMixedContentPreloads = mBlockAllMixedContent;
      }

      // Set up 'upgrade-insecure-requests' if not already inherited
      // from the parent context or set by any other CSP.
      if (!mUpgradeInsecureRequests) {
        rv = mCSP->GetUpgradeInsecureRequests(&mUpgradeInsecureRequests);
        NS_ENSURE_SUCCESS_VOID(rv);
      }
      if (!mUpgradeInsecurePreloads) {
        mUpgradeInsecurePreloads = mUpgradeInsecureRequests;
      }
    }
    return;
  }

  // 2) apply settings from speculative csp
  if (mPreloadCSP) {
    if (!mBlockAllMixedContentPreloads) {
      rv = mPreloadCSP->GetBlockAllMixedContent(&mBlockAllMixedContentPreloads);
      NS_ENSURE_SUCCESS_VOID(rv);
    }
    if (!mUpgradeInsecurePreloads) {
      rv = mPreloadCSP->GetUpgradeInsecureRequests(&mUpgradeInsecurePreloads);
      NS_ENSURE_SUCCESS_VOID(rv);
    }
  }
}

nsresult Document::InitCSP(nsIChannel* aChannel) {
  MOZ_ASSERT(!mScriptGlobalObject,
             "CSP must be initialized before mScriptGlobalObject is set!");
  if (!StaticPrefs::security_csp_enable()) {
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
            ("CSP is disabled, skipping CSP init for document %p", this));
    return NS_OK;
  }

  // If this is a data document - no need to set CSP.
  if (mLoadedAsData) {
    return NS_OK;
  }

  MOZ_ASSERT(!mCSP, "where did mCSP get set if not here?");

  // If there is a CSP that needs to be inherited from whatever
  // global is considered the client of the document fetch then
  // we query it here from the loadinfo in case the newly created
  // document needs to inherit the CSP. See:
  // https://w3c.github.io/webappsec-csp/#initialize-document-csp
  if (CSP_ShouldResponseInheritCSP(aChannel)) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    mCSP = loadInfo->GetCspToInherit();
  }

  // If there is no CSP to inherit, then we create a new CSP here so
  // that history entries always have the right reference in case a
  // Meta CSP gets dynamically added after the history entry has
  // already been created.
  if (!mCSP) {
    mCSP = new nsCSPContext();
  }

  // Always overwrite the requesting context of the CSP so that any new
  // 'self' keyword added to an inherited CSP translates correctly.
  nsresult rv = mCSP->SetRequestContextWithDocument(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString tCspHeaderValue, tCspROHeaderValue;

  nsCOMPtr<nsIHttpChannel> httpChannel;
  rv = GetHttpChannelHelper(aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (httpChannel) {
    Unused << httpChannel->GetResponseHeader(
        NS_LITERAL_CSTRING("content-security-policy"), tCspHeaderValue);

    Unused << httpChannel->GetResponseHeader(
        NS_LITERAL_CSTRING("content-security-policy-report-only"),
        tCspROHeaderValue);
  }
  NS_ConvertASCIItoUTF16 cspHeaderValue(tCspHeaderValue);
  NS_ConvertASCIItoUTF16 cspROHeaderValue(tCspROHeaderValue);

  // Check if this is a document from a WebExtension.
  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  auto addonPolicy = BasePrincipal::Cast(principal)->AddonPolicy();

  // If there's no CSP to apply, go ahead and return early
  if (!addonPolicy && cspHeaderValue.IsEmpty() && cspROHeaderValue.IsEmpty()) {
    if (MOZ_LOG_TEST(gCspPRLog, LogLevel::Debug)) {
      nsCOMPtr<nsIURI> chanURI;
      aChannel->GetURI(getter_AddRefs(chanURI));
      nsAutoCString aspec;
      chanURI->GetAsciiSpec(aspec);
      MOZ_LOG(gCspPRLog, LogLevel::Debug,
              ("no CSP for document, %s", aspec.get()));
    }

    return NS_OK;
  }

  MOZ_LOG(gCspPRLog, LogLevel::Debug,
          ("Document is an add-on or CSP header specified %p", this));

  // ----- if the doc is an addon, apply its CSP.
  if (addonPolicy) {
    nsAutoString extensionPageCSP;
    Unused << ExtensionPolicyService::GetSingleton().GetBaseCSP(
        extensionPageCSP);
    mCSP->AppendPolicy(extensionPageCSP, false, false);

    mCSP->AppendPolicy(addonPolicy->ExtensionPageCSP(), false, false);
    // Bug 1548468: Move CSP off ExpandedPrincipal
    // Currently the LoadInfo holds the source of truth for every resource load
    // because LoadInfo::GetCSP() queries the CSP from an ExpandedPrincipal
    // (and not from the Client) if the load was triggered by an extension.
    auto* basePrin = BasePrincipal::Cast(principal);
    if (basePrin->Is<ExpandedPrincipal>()) {
      basePrin->As<ExpandedPrincipal>()->SetCsp(mCSP);
    }
  }

  // ----- if there's a full-strength CSP header, apply it.
  if (!cspHeaderValue.IsEmpty()) {
    mHasCSPDeliveredThroughHeader = true;
    rv = CSP_AppendCSPFromHeader(mCSP, cspHeaderValue, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- if there's a report-only CSP header, apply it.
  if (!cspROHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(mCSP, cspROHeaderValue, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- Enforce sandbox policy if supplied in CSP header
  // The document may already have some sandbox flags set (e.g. if the document
  // is an iframe with the sandbox attribute set). If we have a CSP sandbox
  // directive, intersect the CSP sandbox flags with the existing flags. This
  // corresponds to the _least_ permissive policy.
  uint32_t cspSandboxFlags = SANDBOXED_NONE;
  rv = mCSP->GetCSPSandboxFlags(&cspSandboxFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Probably the iframe sandbox attribute already caused the creation of a
  // new NullPrincipal. Only create a new NullPrincipal if CSP requires so
  // and no one has been created yet.
  bool needNewNullPrincipal = (cspSandboxFlags & SANDBOXED_ORIGIN) &&
                              !(mSandboxFlags & SANDBOXED_ORIGIN);

  mSandboxFlags |= cspSandboxFlags;

  if (needNewNullPrincipal) {
    principal = NullPrincipal::CreateWithInheritedAttributes(principal);
    SetPrincipals(principal, principal);
  }

  ApplySettingsFromCSP(false);
  return NS_OK;
}

already_AddRefed<dom::FeaturePolicy> Document::GetParentFeaturePolicy() {
  if (!mDocumentContainer) {
    return nullptr;
  }

  nsPIDOMWindowOuter* containerWindow = mDocumentContainer->GetWindow();
  if (!containerWindow) {
    return nullptr;
  }

  BrowsingContext* context = containerWindow->GetBrowsingContext();
  if (!context) {
    return nullptr;
  }

  RefPtr<dom::FeaturePolicy> parentPolicy;
  if (context->IsContentSubframe() && !context->GetParent()->IsInProcess()) {
    // We are in cross process, so try to get feature policy from
    // container's BrowsingContext
    parentPolicy = context->GetFeaturePolicy();
    return parentPolicy.forget();
  }

  nsCOMPtr<nsINode> node = containerWindow->GetFrameElementInternal();
  HTMLIFrameElement* iframe = HTMLIFrameElement::FromNodeOrNull(node);
  if (!iframe) {
    return nullptr;
  }

  parentPolicy = iframe->FeaturePolicy();
  return parentPolicy.forget();
}

nsresult Document::InitFeaturePolicy(nsIChannel* aChannel) {
  MOZ_ASSERT(mFeaturePolicy, "we should only call init once");

  mFeaturePolicy->ResetDeclaredPolicy();

  if (!StaticPrefs::dom_security_featurePolicy_enabled()) {
    return NS_OK;
  }

  mFeaturePolicy->SetDefaultOrigin(NodePrincipal());

  RefPtr<mozilla::dom::FeaturePolicy> parentPolicy = GetParentFeaturePolicy();
  if (parentPolicy) {
    // Let's inherit the policy from the parent HTMLIFrameElement if it exists.
    mFeaturePolicy->InheritPolicy(parentPolicy);
  }

  // We don't want to parse the http Feature-Policy header if this pref is off.
  if (!StaticPrefs::dom_security_featurePolicy_header_enabled()) {
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel;
  nsresult rv = GetHttpChannelHelper(aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!httpChannel) {
    return NS_OK;
  }

  // query the policy from the header
  nsAutoCString value;
  rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Feature-Policy"),
                                      value);
  if (NS_SUCCEEDED(rv)) {
    mFeaturePolicy->SetDeclaredPolicy(this, NS_ConvertUTF8toUTF16(value),
                                      NodePrincipal(), nullptr);
  }

  return NS_OK;
}

nsresult Document::InitReferrerInfo(nsIChannel* aChannel) {
  MOZ_ASSERT(mReferrerInfo);
  MOZ_ASSERT(mPreloadReferrerInfo);

  if (ReferrerInfo::ShouldResponseInheritReferrerInfo(aChannel)) {
    // The channel is loading `about:srcdoc`. Srcdoc loads should respond with
    // their parent's ReferrerInfo when asked for their ReferrerInfo, unless
    // they have an opaque origin.
    // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
    if (BrowsingContext* bc = GetBrowsingContext()) {
      // At this point the document is not fully created and mParentDocument has
      // not been set yet,
      Document* parentDoc = bc->GetEmbedderElement()
                                ? bc->GetEmbedderElement()->OwnerDoc()
                                : nullptr;
      if (parentDoc) {
        mReferrerInfo = parentDoc->GetReferrerInfo();
        mPreloadReferrerInfo = mReferrerInfo;
        return NS_OK;
      }

      MOZ_ASSERT(bc->IsInProcess() || NodePrincipal()->GetIsNullPrincipal(),
                 "srcdoc without null principal as toplevel!");
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel;
  nsresult rv = GetHttpChannelHelper(aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!httpChannel) {
    return NS_OK;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo = httpChannel->GetReferrerInfo();
  if (referrerInfo) {
    mReferrerInfo = referrerInfo;
  }

  // Override policy if we get one from Referrerr-Policy header
  mozilla::dom::ReferrerPolicy policy =
      nsContentUtils::GetReferrerPolicyFromChannel(aChannel);
  mReferrerInfo = static_cast<dom::ReferrerInfo*>(mReferrerInfo.get())
                      ->CloneWithNewPolicy(policy);

  mPreloadReferrerInfo = mReferrerInfo;
  return NS_OK;
}

void Document::StopDocumentLoad() {
  if (mParser) {
    mParserAborted = true;
    mParser->Terminate();
  }
}

void Document::SetDocumentURI(nsIURI* aURI) {
  nsCOMPtr<nsIURI> oldBase = GetDocBaseURI();
  mDocumentURI = aURI;
  nsIURI* newBase = GetDocBaseURI();

  mDocURISchemeIsChrome = aURI && IsChromeURI(aURI);

  bool equalBases = false;
  // Changing just the ref of a URI does not change how relative URIs would
  // resolve wrt to it, so we can treat the bases as equal as long as they're
  // equal ignoring the ref.
  if (oldBase && newBase) {
    oldBase->EqualsExceptRef(newBase, &equalBases);
  } else {
    equalBases = !oldBase && !newBase;
  }

  // If this is the first time we're setting the document's URI, set the
  // document's original URI.
  if (!mOriginalURI) mOriginalURI = mDocumentURI;

  // If changing the document's URI changed the base URI of the document, we
  // need to refresh the hrefs of all the links on the page.
  if (!equalBases) {
    RefreshLinkHrefs();
  }

  // Recalculate our base domain
  mBaseDomain.Truncate();
  ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();
  if (thirdPartyUtil) {
    Unused << thirdPartyUtil->GetBaseDomain(mDocumentURI, mBaseDomain);
  }

  // Tell our WindowGlobalParent that the document's URI has been changed.
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner && inner->GetWindowGlobalChild()) {
    inner->GetWindowGlobalChild()->SetDocumentURI(mDocumentURI);
  }
}

static void GetFormattedTimeString(PRTime aTime,
                                   nsAString& aFormattedTimeString) {
  PRExplodedTime prtime;
  PR_ExplodeTime(aTime, PR_LocalTimeParameters, &prtime);
  // "MM/DD/YYYY hh:mm:ss"
  char formatedTime[24];
  if (SprintfLiteral(formatedTime, "%02d/%02d/%04d %02d:%02d:%02d",
                     prtime.tm_month + 1, prtime.tm_mday, int(prtime.tm_year),
                     prtime.tm_hour, prtime.tm_min, prtime.tm_sec)) {
    CopyASCIItoUTF16(nsDependentCString(formatedTime), aFormattedTimeString);
  } else {
    // If we for whatever reason failed to find the last modified time
    // (or even the current time), fall back to what NS4.x returned.
    aFormattedTimeString.AssignLiteral(u"01/01/1970 00:00:00");
  }
}

void Document::GetLastModified(nsAString& aLastModified) const {
  if (!mLastModified.IsEmpty()) {
    aLastModified.Assign(mLastModified);
  } else {
    GetFormattedTimeString(PR_Now(), aLastModified);
  }
}

static void IncrementExpandoGeneration(Document& aDoc) {
  ++aDoc.mExpandoAndGeneration.generation;
}

void Document::AddToNameTable(Element* aElement, nsAtom* aName) {
  MOZ_ASSERT(
      nsGenericHTMLElement::ShouldExposeNameAsHTMLDocumentProperty(aElement),
      "Only put elements that need to be exposed as document['name'] in "
      "the named table.");

  IdentifierMapEntry* entry = mIdentifierMap.PutEntry(aName);

  // Null for out-of-memory
  if (entry) {
    if (!entry->HasNameElement() &&
        !entry->HasIdElementExposedAsHTMLDocumentProperty()) {
      IncrementExpandoGeneration(*this);
    }
    entry->AddNameElement(this, aElement);
  }
}

void Document::RemoveFromNameTable(Element* aElement, nsAtom* aName) {
  // Speed up document teardown
  if (mIdentifierMap.Count() == 0) return;

  IdentifierMapEntry* entry = mIdentifierMap.GetEntry(aName);
  if (!entry)  // Could be false if the element was anonymous, hence never added
    return;

  entry->RemoveNameElement(aElement);
  if (!entry->HasNameElement() &&
      !entry->HasIdElementExposedAsHTMLDocumentProperty()) {
    IncrementExpandoGeneration(*this);
  }
}

void Document::AddToIdTable(Element* aElement, nsAtom* aId) {
  IdentifierMapEntry* entry = mIdentifierMap.PutEntry(aId);

  if (entry) { /* True except on OOM */
    if (nsGenericHTMLElement::ShouldExposeIdAsHTMLDocumentProperty(aElement) &&
        !entry->HasNameElement() &&
        !entry->HasIdElementExposedAsHTMLDocumentProperty()) {
      IncrementExpandoGeneration(*this);
    }
    entry->AddIdElement(aElement);
  }
}

void Document::RemoveFromIdTable(Element* aElement, nsAtom* aId) {
  NS_ASSERTION(aId, "huhwhatnow?");

  // Speed up document teardown
  if (mIdentifierMap.Count() == 0) {
    return;
  }

  IdentifierMapEntry* entry = mIdentifierMap.GetEntry(aId);
  if (!entry)  // Can be null for XML elements with changing ids.
    return;

  entry->RemoveIdElement(aElement);
  if (nsGenericHTMLElement::ShouldExposeIdAsHTMLDocumentProperty(aElement) &&
      !entry->HasNameElement() &&
      !entry->HasIdElementExposedAsHTMLDocumentProperty()) {
    IncrementExpandoGeneration(*this);
  }
  if (entry->IsEmpty()) {
    mIdentifierMap.RemoveEntry(entry);
  }
}

void Document::SetPrincipals(nsIPrincipal* aNewPrincipal,
                             nsIPrincipal* aNewStoragePrincipal) {
  MOZ_ASSERT(!!aNewPrincipal == !!aNewStoragePrincipal);
  if (aNewPrincipal && mAllowDNSPrefetch &&
      StaticPrefs::network_dns_disablePrefetchFromHTTPS()) {
    if (aNewPrincipal->SchemeIs("https")) {
      mAllowDNSPrefetch = false;
    }
  }
  mNodeInfoManager->SetDocumentPrincipal(aNewPrincipal);
  mIntrinsicStoragePrincipal = aNewStoragePrincipal;

  ContentBlockingAllowList::ComputePrincipal(
      aNewPrincipal, getter_AddRefs(mContentBlockingAllowListPrincipal));

#ifdef DEBUG
  // Validate that the docgroup is set correctly by calling its getter and
  // triggering its sanity check.
  //
  // If we're setting the principal to null, we don't want to perform the check,
  // as the document is entering an intermediate state where it does not have a
  // principal. It will be given another real principal shortly which we will
  // check. It's not unsafe to have a document which has a null principal in the
  // same docgroup as another document, so this should not be a problem.
  if (aNewPrincipal) {
    GetDocGroup();
  }
#endif
}

#ifdef DEBUG
void Document::AssertDocGroupMatchesKey() const {
  // Sanity check that we have an up-to-date and accurate docgroup
  if (mDocGroup) {
    nsAutoCString docGroupKey;

    // GetKey() can fail, e.g. after the TLD service has shut down.
    nsresult rv = mozilla::dom::DocGroup::GetKey(NodePrincipal(), docGroupKey);
    if (NS_SUCCEEDED(rv)) {
      MOZ_ASSERT(mDocGroup->MatchesKey(docGroupKey));
    }
    // XXX: Check that the TabGroup is correct as well!
  }
}
#endif

nsresult Document::Dispatch(TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) {
  // Note that this method may be called off the main thread.
  if (mDocGroup) {
    return mDocGroup->Dispatch(aCategory, std::move(aRunnable));
  }
  return DispatcherTrait::Dispatch(aCategory, std::move(aRunnable));
}

nsISerialEventTarget* Document::EventTargetFor(TaskCategory aCategory) const {
  if (mDocGroup) {
    return mDocGroup->EventTargetFor(aCategory);
  }
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread* Document::AbstractMainThreadFor(
    mozilla::TaskCategory aCategory) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mDocGroup) {
    return mDocGroup->AbstractMainThreadFor(aCategory);
  }
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
}

void Document::NoteScriptTrackingStatus(const nsACString& aURL,
                                        bool aIsTracking) {
  if (aIsTracking) {
    mTrackingScripts.PutEntry(aURL);
  } else {
    MOZ_ASSERT(!mTrackingScripts.Contains(aURL));
  }
}

bool Document::IsScriptTracking(JSContext* aCx) const {
  JS::AutoFilename filename;
  uint32_t line = 0;
  uint32_t column = 0;
  if (!JS::DescribeScriptedCaller(aCx, &filename, &line, &column)) {
    return false;
  }
  return mTrackingScripts.Contains(nsDependentCString(filename.get()));
}

NS_IMETHODIMP
Document::GetApplicationCache(nsIApplicationCache** aApplicationCache) {
  NS_IF_ADDREF(*aApplicationCache = mApplicationCache);
  return NS_OK;
}

NS_IMETHODIMP
Document::SetApplicationCache(nsIApplicationCache* aApplicationCache) {
  mApplicationCache = aApplicationCache;
  return NS_OK;
}

void Document::GetContentType(nsAString& aContentType) {
  CopyUTF8toUTF16(GetContentTypeInternal(), aContentType);
}

void Document::SetContentType(const nsAString& aContentType) {
  SetContentTypeInternal(NS_ConvertUTF16toUTF8(aContentType));
}

bool Document::GetAllowPlugins() {
  // First, we ask our docshell if it allows plugins.
  auto* browsingContext = GetBrowsingContext();

  if (browsingContext) {
    if (!browsingContext->GetAllowPlugins()) {
      return false;
    }

    // If the docshell allows plugins, we check whether
    // we are sandboxed and plugins should not be allowed.
    if (mSandboxFlags & SANDBOXED_PLUGINS) {
      return false;
    }
  }

  FlashClassification classification = DocumentFlashClassification();
  if (classification == FlashClassification::Denied) {
    return false;
  }

  return true;
}

void Document::InitializeLocalization(nsTArray<nsString>& aResourceIds) {
  MOZ_ASSERT(!mDocumentL10n, "mDocumentL10n should not be initialized yet");

  RefPtr<DocumentL10n> l10n = new DocumentL10n(this);
  ErrorResult rv;
  l10n->Init(aResourceIds, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return;
  }
  mDocumentL10n = l10n;
}

DocumentL10n* Document::GetL10n() { return mDocumentL10n; }

bool Document::DocumentSupportsL10n(JSContext* aCx, JSObject* aObject) {
  nsCOMPtr<nsIPrincipal> callerPrincipal =
      nsContentUtils::SubjectPrincipal(aCx);
  nsGlobalWindowInner* win = xpc::WindowOrNull(aObject);
  bool allowed = false;
  callerPrincipal->IsL10nAllowed(win ? win->GetDocumentURI() : nullptr,
                                 &allowed);
  return allowed;
}

void Document::LocalizationLinkAdded(Element* aLinkElement) {
  if (!AllowsL10n()) {
    return;
  }

  nsAutoString href;
  aLinkElement->GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);
  // If the link is added after the DocumentL10n instance
  // has been initialized, just pass the resource ID to it.
  if (mDocumentL10n) {
    AutoTArray<nsString, 1> resourceIds;
    resourceIds.AppendElement(href);
    mDocumentL10n->AddResourceIds(resourceIds, false);
  } else if (mReadyState >= READYSTATE_INTERACTIVE) {
    // Otherwise, if the document has already been parsed
    // we need to lazily initialize the localization.
    AutoTArray<nsString, 1> resourceIds;
    resourceIds.AppendElement(href);
    InitializeLocalization(resourceIds);
    mDocumentL10n->TriggerInitialDocumentTranslation();
  } else {
    // Otherwise, we're still parsing the document.
    // In that case, add it to the pending list. This list
    // will be resolved once the end of l10n resource
    // container is reached.
    mL10nResources.AppendElement(href);

    if (!mPendingInitialTranslation) {
      // Our initial translation is going to block layout start.  Make sure we
      // don't fire the load event until after that stops happening and layout
      // has a chance to start.
      BlockOnload();
    }

    mPendingInitialTranslation = true;
  }
}

void Document::LocalizationLinkRemoved(Element* aLinkElement) {
  if (!AllowsL10n()) {
    return;
  }

  nsAutoString href;
  aLinkElement->GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);
  if (mDocumentL10n) {
    AutoTArray<nsString, 1> resourceIds;
    resourceIds.AppendElement(href);
    uint32_t remaining = mDocumentL10n->RemoveResourceIds(resourceIds);
    if (remaining == 0) {
      mDocumentL10n = nullptr;
    }
  } else {
    mL10nResources.RemoveElement(href);
  }
}

/**
 * This method should be called once the end of the l10n
 * resource container has been parsed.
 *
 * In XUL this is the end of the first </linkset>,
 * In XHTML/HTML this is the end of </head>.
 *
 * This milestone is used to allow for batch
 * localization context I/O and building done
 * once when all resources in the document have been
 * collected.
 */
void Document::OnL10nResourceContainerParsed() {
  if (!mL10nResources.IsEmpty()) {
    InitializeLocalization(mL10nResources);
    mL10nResources.Clear();
  }
}

void Document::TriggerInitialDocumentTranslation() {
  // Let's call it again, in case the resource
  // container has not been closed, and only
  // now we're closing the document.
  OnL10nResourceContainerParsed();

  if (mDocumentL10n) {
    mDocumentL10n->TriggerInitialDocumentTranslation();
  }
}

void Document::InitialDocumentTranslationCompleted() {
  if (mPendingInitialTranslation) {
    // This means we blocked the load event in LocalizationLinkAdded.  It's
    // important that the load blocker removal here be async, because our caller
    // will notify the content sink after us, and we want the content sync's
    // work to happen before the load event fires.
    UnblockOnload(/* aFireSync = */ false);
  }
  mPendingInitialTranslation = false;

  mL10nProtoElements.Clear();

  nsXULPrototypeDocument* proto = GetPrototype();
  if (proto) {
    proto->SetIsL10nCached();
  }
}

bool Document::AllowsL10n() const {
  bool allowed = false;
  NodePrincipal()->IsL10nAllowed(GetDocumentURI(), &allowed);
  return allowed;
}

bool Document::IsWebAnimationsEnabled(JSContext* aCx, JSObject* /*unused*/) {
  MOZ_ASSERT(NS_IsMainThread());

  return nsContentUtils::IsSystemCaller(aCx) ||
         StaticPrefs::dom_animations_api_core_enabled();
}

bool Document::IsWebAnimationsEnabled(CallerType aCallerType) {
  MOZ_ASSERT(NS_IsMainThread());

  return aCallerType == dom::CallerType::System ||
         StaticPrefs::dom_animations_api_core_enabled();
}

bool Document::IsWebAnimationsGetAnimationsEnabled(JSContext* aCx,
                                                   JSObject* /*unused*/
) {
  MOZ_ASSERT(NS_IsMainThread());

  return nsContentUtils::IsSystemCaller(aCx) ||
         StaticPrefs::dom_animations_api_getAnimations_enabled();
}

bool Document::AreWebAnimationsImplicitKeyframesEnabled(JSContext* aCx,
                                                        JSObject* /*unused*/
) {
  MOZ_ASSERT(NS_IsMainThread());

  return nsContentUtils::IsSystemCaller(aCx) ||
         StaticPrefs::dom_animations_api_implicit_keyframes_enabled();
}

bool Document::AreWebAnimationsTimelinesEnabled(JSContext* aCx,
                                                JSObject* /*unused*/
) {
  MOZ_ASSERT(NS_IsMainThread());

  return nsContentUtils::IsSystemCaller(aCx) ||
         StaticPrefs::dom_animations_api_timelines_enabled();
}

DocumentTimeline* Document::Timeline() {
  if (!mDocumentTimeline) {
    mDocumentTimeline = new DocumentTimeline(this, TimeDuration(0));
  }

  return mDocumentTimeline;
}

SVGSVGElement* Document::GetSVGRootElement() const {
  Element* root = GetRootElement();
  if (!root || !root->IsSVGElement(nsGkAtoms::svg)) {
    return nullptr;
  }
  return static_cast<SVGSVGElement*>(root);
}

/* Return true if the document is in the focused top-level window, and is an
 * ancestor of the focused DOMWindow. */
bool Document::HasFocus(ErrorResult& rv) const {
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    rv.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

  // Is there a focused DOMWindow?
  nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow) {
    return false;
  }

  nsPIDOMWindowOuter* piWindow = nsPIDOMWindowOuter::From(focusedWindow);

  // Are we an ancestor of the focused DOMWindow?
  for (Document* currentDoc = piWindow->GetDoc(); currentDoc;
       currentDoc = currentDoc->GetInProcessParentDocument()) {
    if (currentDoc == this) {
      // Yes, we are an ancestor
      return true;
    }
  }

  return false;
}

void Document::GetDesignMode(nsAString& aDesignMode) {
  if (HasFlag(NODE_IS_EDITABLE)) {
    aDesignMode.AssignLiteral("on");
  } else {
    aDesignMode.AssignLiteral("off");
  }
}

void Document::SetDesignMode(const nsAString& aDesignMode,
                             nsIPrincipal& aSubjectPrincipal, ErrorResult& rv) {
  SetDesignMode(aDesignMode, Some(&aSubjectPrincipal), rv);
}

static void NotifyEditableStateChange(Document& aDoc) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  nsMutationGuard g;
#endif
  for (nsIContent* node = aDoc.GetNextNode(&aDoc); node;
       node = node->GetNextNode(&aDoc)) {
    if (auto* element = Element::FromNode(node)) {
      element->UpdateState(true);
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(!g.Mutated(0));
}

void Document::SetDesignMode(const nsAString& aDesignMode,
                             const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                             ErrorResult& rv) {
  if (aSubjectPrincipal.isSome() &&
      !aSubjectPrincipal.value()->Subsumes(NodePrincipal())) {
    rv.Throw(NS_ERROR_DOM_PROP_ACCESS_DENIED);
    return;
  }
  bool editableMode = HasFlag(NODE_IS_EDITABLE);
  if (aDesignMode.LowerCaseEqualsASCII(editableMode ? "off" : "on")) {
    SetEditableFlag(!editableMode);
    // Changing the NODE_IS_EDITABLE flags on document changes the intrinsic
    // state of all descendant elements of it. Update that now.
    NotifyEditableStateChange(*this);
    rv = EditingStateChanged();
  }
}

nsCommandManager* Document::GetMidasCommandManager() {
  // check if we have it cached
  if (mMidasCommandManager) {
    return mMidasCommandManager;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    return nullptr;
  }

  nsIDocShell* docshell = window->GetDocShell();
  if (!docshell) {
    return nullptr;
  }

  mMidasCommandManager = docshell->GetCommandManager();
  return mMidasCommandManager;
}

// static
void Document::EnsureInitializeInternalCommandDataHashtable() {
  if (sInternalCommandDataHashtable) {
    return;
  }
  sInternalCommandDataHashtable = new InternalCommandDataHashtable();
  // clang-format off
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("bold"),
      InternalCommandData(
          "cmd_bold",
          Command::FormatBold,
          ExecCommandParam::Ignore,
          StyleUpdatingCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("italic"),
      InternalCommandData(
          "cmd_italic",
          Command::FormatItalic,
          ExecCommandParam::Ignore,
          StyleUpdatingCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("underline"),
      InternalCommandData(
          "cmd_underline",
          Command::FormatUnderline,
          ExecCommandParam::Ignore,
          StyleUpdatingCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("strikethrough"),
      InternalCommandData(
          "cmd_strikethrough",
          Command::FormatStrikeThrough,
          ExecCommandParam::Ignore,
          StyleUpdatingCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("subscript"),
      InternalCommandData(
          "cmd_subscript",
          Command::FormatSubscript,
          ExecCommandParam::Ignore,
          StyleUpdatingCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("superscript"),
      InternalCommandData(
          "cmd_superscript",
          Command::FormatSuperscript,
          ExecCommandParam::Ignore,
          StyleUpdatingCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("cut"),
      InternalCommandData(
          "cmd_cut",
          Command::Cut,
          ExecCommandParam::Ignore,
          CutCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("copy"),
      InternalCommandData(
          "cmd_copy",
          Command::Copy,
          ExecCommandParam::Ignore,
          CopyCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("paste"),
      InternalCommandData(
          "cmd_paste",
          Command::Paste,
          ExecCommandParam::Ignore,
          PasteCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("delete"),
      InternalCommandData(
          "cmd_deleteCharBackward",
          Command::DeleteCharBackward,
          ExecCommandParam::Ignore,
          DeleteCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("forwarddelete"),
      InternalCommandData(
          "cmd_deleteCharForward",
          Command::DeleteCharForward,
          ExecCommandParam::Ignore,
          DeleteCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("selectall"),
      InternalCommandData(
          "cmd_selectAll",
          Command::SelectAll,
          ExecCommandParam::Ignore,
          SelectAllCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("undo"),
      InternalCommandData(
          "cmd_undo",
          Command::HistoryUndo,
          ExecCommandParam::Ignore,
          UndoCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("redo"),
      InternalCommandData(
          "cmd_redo",
          Command::HistoryRedo,
          ExecCommandParam::Ignore,
          RedoCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("indent"),
      InternalCommandData("cmd_indent",
          Command::FormatIndent,
          ExecCommandParam::Ignore,
          IndentCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("outdent"),
      InternalCommandData(
          "cmd_outdent",
          Command::FormatOutdent,
          ExecCommandParam::Ignore,
          OutdentCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("backcolor"),
      InternalCommandData(
          "cmd_highlight",
          Command::FormatBackColor,
          ExecCommandParam::String,
          HighlightColorStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("hilitecolor"),
      InternalCommandData(
          "cmd_highlight",
          Command::FormatBackColor,
          ExecCommandParam::String,
          HighlightColorStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("forecolor"),
      InternalCommandData(
          "cmd_fontColor",
          Command::FormatFontColor,
          ExecCommandParam::String,
          FontColorStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("fontname"),
      InternalCommandData(
          "cmd_fontFace",
          Command::FormatFontName,
          ExecCommandParam::String,
          FontFaceStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("fontsize"),
      InternalCommandData(
          "cmd_fontSize",
          Command::FormatFontSize,
          ExecCommandParam::String,
          FontSizeStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("increasefontsize"),
      InternalCommandData(
          "cmd_increaseFont",
          Command::FormatIncreaseFontSize,
          ExecCommandParam::Ignore,
          IncreaseFontSizeCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("decreasefontsize"),
      InternalCommandData(
          "cmd_decreaseFont",
          Command::FormatDecreaseFontSize,
          ExecCommandParam::Ignore,
          DecreaseFontSizeCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("inserthorizontalrule"),
      InternalCommandData(
          "cmd_insertHR",
          Command::InsertHorizontalRule,
          ExecCommandParam::Ignore,
          InsertTagCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("createlink"),
      InternalCommandData(
          "cmd_insertLinkNoUI",
          Command::InsertLink,
          ExecCommandParam::String,
          InsertTagCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("insertimage"),
      InternalCommandData(
          "cmd_insertImageNoUI",
          Command::InsertImage,
          ExecCommandParam::String,
          InsertTagCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("inserthtml"),
      InternalCommandData(
          "cmd_insertHTML",
          Command::InsertHTML,
          ExecCommandParam::String,
          InsertHTMLCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("inserttext"),
      InternalCommandData(
          "cmd_insertText",
          Command::InsertText,
          ExecCommandParam::String,
          InsertPlaintextCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("gethtml"),
      InternalCommandData(
          "cmd_getContents",
          Command::GetHTML,
          ExecCommandParam::Ignore,
          nullptr));  // Not defined in EditorCommands.h
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("justifyleft"),
      InternalCommandData(
          "cmd_align",
          Command::FormatJustifyLeft,
          ExecCommandParam::Ignore,  // Will be set to "left"
          AlignCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("justifyright"),
      InternalCommandData(
          "cmd_align",
          Command::FormatJustifyRight,
          ExecCommandParam::Ignore,  // Will be set to "right"
          AlignCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("justifycenter"),
      InternalCommandData(
          "cmd_align",
          Command::FormatJustifyCenter,
          ExecCommandParam::Ignore,  // Will be set to "center"
          AlignCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("justifyfull"),
      InternalCommandData(
          "cmd_align",
          Command::FormatJustifyFull,
          ExecCommandParam::Ignore,  // Will be set to "justify"
          AlignCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("removeformat"),
      InternalCommandData(
          "cmd_removeStyles",
          Command::FormatRemove,
          ExecCommandParam::Ignore,
          RemoveStylesCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("unlink"),
      InternalCommandData(
          "cmd_removeLinks",
          Command::FormatRemoveLink,
          ExecCommandParam::Ignore,
          StyleUpdatingCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("insertorderedlist"),
      InternalCommandData(
          "cmd_ol",
          Command::InsertOrderedList,
          ExecCommandParam::Ignore,
          ListCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("insertunorderedlist"),
      InternalCommandData(
          "cmd_ul",
          Command::InsertUnorderedList,
          ExecCommandParam::Ignore,
          ListCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("insertparagraph"),
      InternalCommandData(
          "cmd_insertParagraph",
          Command::InsertParagraph,
          ExecCommandParam::Ignore,
          InsertParagraphCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("insertlinebreak"),
      InternalCommandData(
          "cmd_insertLineBreak",
          Command::InsertLineBreak,
          ExecCommandParam::Ignore,
          InsertLineBreakCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("formatblock"),
      InternalCommandData(
          "cmd_paragraphState",
          Command::FormatBlock,
          ExecCommandParam::String,
          ParagraphStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("heading"),
      InternalCommandData(
          "cmd_paragraphState",
          Command::FormatBlock,
          ExecCommandParam::String,
          ParagraphStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("styleWithCSS"),
      InternalCommandData(
          "cmd_setDocumentUseCSS",
          Command::SetDocumentUseCSS,
          ExecCommandParam::Boolean,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("usecss"),  // Legacy command
      InternalCommandData(
          "cmd_setDocumentUseCSS",
          Command::SetDocumentUseCSS,
          ExecCommandParam::InvertedBoolean,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("contentReadOnly"),
      InternalCommandData(
          "cmd_setDocumentReadOnly",
          Command::SetDocumentReadOnly,
          ExecCommandParam::Boolean,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("readonly"),  // Legacy command
      InternalCommandData(
          "cmd_setDocumentReadOnly",
          Command::SetDocumentReadOnly,
          ExecCommandParam::InvertedBoolean,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("insertBrOnReturn"),
      InternalCommandData(
          "cmd_insertBrOnReturn",
          Command::SetDocumentInsertBROnEnterKeyPress,
          ExecCommandParam::Boolean,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("defaultParagraphSeparator"),
      InternalCommandData(
          "cmd_defaultParagraphSeparator",
          Command::SetDocumentDefaultParagraphSeparator,
          ExecCommandParam::String,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("enableObjectResizing"),
      InternalCommandData(
          "cmd_enableObjectResizing",
          Command::ToggleObjectResizers,
          ExecCommandParam::Boolean,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("enableInlineTableEditing"),
      InternalCommandData(
          "cmd_enableInlineTableEditing",
          Command::ToggleInlineTableEditor,
          ExecCommandParam::Boolean,
          SetDocumentStateCommand::GetInstance));
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("enableAbsolutePositionEditing"),
      InternalCommandData(
          "cmd_enableAbsolutePositionEditing",
          Command::ToggleAbsolutePositionEditor,
          ExecCommandParam::Boolean,
          SetDocumentStateCommand::GetInstance));
#if 0
  // with empty string
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("justifynone"),
      InternalCommandData(
          "cmd_align",
          Command::Undefined,
          ExecCommandParam::Ignore,
          nullptr));  // Not implemented yet.
  // REQUIRED SPECIAL REVIEW special review
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("saveas"),
      InternalCommandData(
          "cmd_saveAs",
          Command::Undefined,
          ExecCommandParam::Boolean,
          nullptr));  // Not implemented yet.
  // REQUIRED SPECIAL REVIEW special review
  sInternalCommandDataHashtable->Put(
      NS_LITERAL_STRING("print"),
      InternalCommandData(
          "cmd_print",
          Command::Undefined,
          ExecCommandParam::Boolean,
          nullptr));  // Not implemented yet.
#endif  // #if 0
  // clang-format on
}

Document::InternalCommandData Document::ConvertToInternalCommand(
    const nsAString& aHTMLCommandName,
    const nsAString& aValue /* = EmptyString() */,
    nsAString* aAdjustedValue /* = nullptr */) {
  MOZ_ASSERT(!aAdjustedValue || aAdjustedValue->IsEmpty());
  EnsureInitializeInternalCommandDataHashtable();
  InternalCommandData commandData;
  if (!sInternalCommandDataHashtable->Get(aHTMLCommandName, &commandData)) {
    return InternalCommandData();
  }
  if (!aAdjustedValue) {
    // No further work to do
    return commandData;
  }
  switch (commandData.mExecCommandParam) {
    case ExecCommandParam::Ignore:
      // Just have to copy it, no checking
      switch (commandData.mCommand) {
        case Command::FormatJustifyLeft:
          aAdjustedValue->AssignLiteral("left");
          break;
        case Command::FormatJustifyRight:
          aAdjustedValue->AssignLiteral("right");
          break;
        case Command::FormatJustifyCenter:
          aAdjustedValue->AssignLiteral("center");
          break;
        case Command::FormatJustifyFull:
          aAdjustedValue->AssignLiteral("justify");
          break;
        default:
          MOZ_ASSERT(EditorCommand::GetParamType(commandData.mCommand) ==
                     EditorCommandParamType::None);
          break;
      }
      return commandData;

    case ExecCommandParam::Boolean:
      MOZ_ASSERT(!!(EditorCommand::GetParamType(commandData.mCommand) &
                    EditorCommandParamType::Bool));
      // If this is a boolean value and it's not explicitly false (e.g. no
      // value).  We default to "true" (see bug 301490).
      if (!aValue.LowerCaseEqualsLiteral("false")) {
        aAdjustedValue->AssignLiteral("true");
      } else {
        aAdjustedValue->AssignLiteral("false");
      }
      return commandData;

    case ExecCommandParam::InvertedBoolean:
      MOZ_ASSERT(!!(EditorCommand::GetParamType(commandData.mCommand) &
                    EditorCommandParamType::Bool));
      // For old backwards commands we invert the check.
      if (aValue.LowerCaseEqualsLiteral("false")) {
        aAdjustedValue->AssignLiteral("true");
      } else {
        aAdjustedValue->AssignLiteral("false");
      }
      return commandData;

    case ExecCommandParam::String:
      MOZ_ASSERT(!!(
          EditorCommand::GetParamType(commandData.mCommand) &
          (EditorCommandParamType::String | EditorCommandParamType::CString)));
      switch (commandData.mCommand) {
        case Command::FormatBlock: {
          const char16_t* start = aValue.BeginReading();
          const char16_t* end = aValue.EndReading();
          if (start != end && *start == '<' && *(end - 1) == '>') {
            ++start;
            --end;
          }
          // XXX Should we reorder this array with actual usage?
          static const nsStaticAtom* kFormattableBlockTags[] = {
              // clang-format off
            nsGkAtoms::address,
            nsGkAtoms::blockquote,
            nsGkAtoms::dd,
            nsGkAtoms::div,
            nsGkAtoms::dl,
            nsGkAtoms::dt,
            nsGkAtoms::h1,
            nsGkAtoms::h2,
            nsGkAtoms::h3,
            nsGkAtoms::h4,
            nsGkAtoms::h5,
            nsGkAtoms::h6,
            nsGkAtoms::p,
            nsGkAtoms::pre,
              // clang-format on
          };
          nsAutoString value(nsDependentSubstring(start, end));
          ToLowerCase(value);
          const nsStaticAtom* valueAtom = NS_GetStaticAtom(value);
          for (const nsStaticAtom* kTag : kFormattableBlockTags) {
            if (valueAtom == kTag) {
              kTag->ToString(*aAdjustedValue);
              return commandData;
            }
          }
          return InternalCommandData();
        }
        case Command::FormatFontSize: {
          // Per editing spec as of April 23, 2012, we need to reject the value
          // if it's not a valid floating-point number surrounded by optional
          // whitespace.  Otherwise, we parse it as a legacy font size.  For
          // now, we just parse as a legacy font size regardless (matching
          // WebKit) -- bug 747879.
          int32_t size = nsContentUtils::ParseLegacyFontSize(aValue);
          if (!size) {
            return InternalCommandData();
          }
          MOZ_ASSERT(aAdjustedValue->IsEmpty());
          aAdjustedValue->AppendInt(size);
          return commandData;
        }
        case Command::InsertImage:
        case Command::InsertLink:
          if (aValue.IsEmpty()) {
            // Invalid value, return false
            return InternalCommandData();
          }
          aAdjustedValue->Assign(aValue);
          return commandData;
        case Command::SetDocumentDefaultParagraphSeparator:
          if (!aValue.LowerCaseEqualsLiteral("div") &&
              !aValue.LowerCaseEqualsLiteral("p") &&
              !aValue.LowerCaseEqualsLiteral("br")) {
            // Invalid value
            return InternalCommandData();
          }
          aAdjustedValue->Assign(aValue);
          return commandData;
        default:
          aAdjustedValue->Assign(aValue);
          return commandData;
      }

    default:
      MOZ_ASSERT_UNREACHABLE("New ExecCommandParam value hasn't been handled");
      return InternalCommandData();
  }
}

bool Document::ExecCommand(const nsAString& aHTMLCommandName, bool aShowUI,
                           const nsAString& aValue,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  // Only allow on HTML documents.
  if (!IsHTMLOrXHTML()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_DOCUMENT_EXEC_COMMAND);
    return false;
  }

  // if they are requesting UI from us, let's fail since we have no UI
  if (aShowUI) {
    return false;
  }

  // If we're running an execCommand, we should just return false.
  // https://github.com/w3c/editing/issues/200#issuecomment-575241816
  if (!StaticPrefs::dom_document_exec_command_nested_calls_allowed() &&
      mIsRunningExecCommand) {
    return false;
  }

  //  for optional parameters see dom/src/base/nsHistory.cpp: HistoryImpl::Go()
  //  this might add some ugly JS dependencies?

  nsAutoString adjustedValue;
  InternalCommandData commandData =
      ConvertToInternalCommand(aHTMLCommandName, aValue, &adjustedValue);
  if (commandData.mCommand == Command::DoNothing) {
    return false;
  }

  // if editing is not on, bail
  if (commandData.IsAvailableOnlyWhenEditable() && !IsEditingOnAfterFlush()) {
    return false;
  }

  if (commandData.mCommand == Command::GetHTML) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  // Do security check first.
  if (commandData.IsCutOrCopyCommand()) {
    if (!nsContentUtils::IsCutCopyAllowed(this, aSubjectPrincipal)) {
      // We have rejected the event due to it not being performed in an
      // input-driven context therefore, we report the error to the console.
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("DOM"), this,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "ExecCommandCutCopyDeniedNotInputDriven");
      return false;
    }
  } else if (commandData.IsPasteCommand()) {
    if (!nsContentUtils::PrincipalHasPermission(aSubjectPrincipal,
                                                nsGkAtoms::clipboardRead)) {
      return false;
    }
  }

  // Next, consider context of command handling which is automatically resolved
  // by order of controllers in `nsCommandManager::GetControllerForCommand()`.
  // The order is:
  //   1. HTMLEditor for the document, if there is.
  //   2. TextEditor if there is an active element and it has TextEditor like
  //      <input type="text"> or <textarea>.
  //   3. Retarget to the DocShell or nsCommandManager as what we've done.
  // XXX Chromium handles `execCommand()` in <input type="text"> or
  //     <textarea> when it's in an editing host and has focus.  So, our
  //     traditional behavior is different and does not make sense.
  RefPtr<TextEditor> maybeHTMLEditor;
  if (nsPresContext* presContext = GetPresContext()) {
    if (commandData.IsCutOrCopyCommand()) {
      // Note that we used to use DocShell to handle `cut` and `copy` command
      // for dispatching corresponding events for making possible web apps to
      // implement their own editor without editable elements but supports
      // standard shortcut keys, etc.  In this case, we prefer to use active
      // element's editor to keep same behavior.
      maybeHTMLEditor = nsContentUtils::GetActiveEditor(presContext);
    } else {
      maybeHTMLEditor = nsContentUtils::GetHTMLEditor(presContext);
      if (!maybeHTMLEditor) {
        maybeHTMLEditor = nsContentUtils::GetActiveEditor(presContext);
      }
    }
  }

  // Then, retrieve editor command class instance which should handle it
  // and can handle it now.
  RefPtr<EditorCommand> editorCommand;
  if (!maybeHTMLEditor) {
    // If the command is available without editor, we should redirect the
    // command to focused descendant with DocShell.
    if (commandData.IsAvailableOnlyWhenEditable()) {
      return false;
    }
  } else {
    // Otherwise, we should use EditorCommand instance (which is singleton
    // instance) when it's enabled.
    editorCommand = commandData.mGetEditorCommandFunc();
    if (NS_WARN_IF(!editorCommand)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return false;
    }

    if (!editorCommand->IsCommandEnabled(commandData.mCommand,
                                         maybeHTMLEditor)) {
      // If the EditorCommand instance is disabled, we should do nothing if
      // the command requires an editor.
      if (commandData.IsAvailableOnlyWhenEditable()) {
        // Return false if editor specific commands is disabled (bug 760052).
        return false;
      }
      // Otherwise, we should redirect it to focused descendant with DocShell.
      editorCommand = nullptr;
    }
  }

  AutoRunningExecCommandMarker markRunningExecCommand(*this);

  // If we cannot use EditorCommand instance directly, we need to handle the
  // command with traditional path (i.e., with DocShell or nsCommandManager).
  if (!editorCommand) {
    MOZ_ASSERT(!commandData.IsAvailableOnlyWhenEditable());

    // Special case clipboard write commands like Command::Cut and
    // Command::Copy.  For such commands, we need the behaviour from
    // nsWindowRoot::GetControllers() which is to look at the focused element,
    // and defer to a focused textbox's controller.  The code past taken by
    // other commands in ExecCommand() always uses the window directly, rather
    // than deferring to the textbox, which is desireable for most editor
    // commands, but not these commands (as those should allow copying out of
    // embedded editors). This behaviour is invoked if we call DoCommand()
    // directly on the docShell.
    // XXX This means that we allow web app to pick up selected content in
    //     descendant document and write it into the clipboard when a
    //     descendant document has focus.  However, Chromium does not allow
    //     this and this seems that it's not good behavior from point of view
    //     of security.  We should treat this issue in another bug.
    if (commandData.IsCutOrCopyCommand()) {
      nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
      if (!docShell) {
        return false;
      }
      nsresult rv = docShell->DoCommand(commandData.mXULCommandName);
      if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
        return false;
      }
      return NS_SUCCEEDED(rv);
    }

    // Otherwise (currently, only clipboard read commands like Command::Paste),
    // we don't need to redirect the command to focused subdocument.
    // Therefore, we should handle it with nsCommandManager as used to be.
    // It may dispatch only preceding event of editing on non-editable element
    // to make web apps possible to handle standard shortcut key, etc in
    // their own editor.
    RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
    if (!commandManager) {
      aRv.Throw(NS_ERROR_FAILURE);
      return false;
    }

    nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
    if (!window) {
      aRv.Throw(NS_ERROR_FAILURE);
      return false;
    }

    // Return false for disabled commands (bug 760052)
    if (!commandManager->IsCommandEnabled(
            nsDependentCString(commandData.mXULCommandName), window)) {
      return false;
    }

    MOZ_ASSERT(commandData.IsPasteCommand());
    aRv =
        commandManager->DoCommand(commandData.mXULCommandName, nullptr, window);
    return !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION) && !aRv.Failed();
  }

  // Now, our target is fixed to the editor.  So, we can use EditorCommand
  // with the editor directly.
  MOZ_ASSERT(maybeHTMLEditor);

  EditorCommandParamType paramType =
      EditorCommand::GetParamType(commandData.mCommand);

  // If we don't have meaningful parameter or the EditorCommand does not
  // require additional parameter, we can use `DoCommand()`.
  if (adjustedValue.IsEmpty() || paramType == EditorCommandParamType::None) {
    MOZ_ASSERT(!(paramType & EditorCommandParamType::Bool));
    aRv = editorCommand->DoCommand(commandData.mCommand, *maybeHTMLEditor,
                                   &aSubjectPrincipal);
    return !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION) && !aRv.Failed();
  }

  // If the EditorCommand requires `bool` parameter, `adjustedValue` must be
  // "true" or "false" here.  So, we can use `DoCommandParam()` which takes
  // a `bool` value.
  if (!!(paramType & EditorCommandParamType::Bool)) {
    MOZ_ASSERT(adjustedValue.EqualsLiteral("true") ||
               adjustedValue.EqualsLiteral("false"));
    aRv = editorCommand->DoCommandParam(
        commandData.mCommand, Some(adjustedValue.EqualsLiteral("true")),
        *maybeHTMLEditor, &aSubjectPrincipal);
    return !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION) && !aRv.Failed();
  }

  // Now, the EditorCommand requires `nsAString` or `nsACString` parameter
  // in this case.  However, `paramType` may contain both `String` and
  // `CString` but in such case, we should use `DoCommandParam()` which
  // takes `nsAString`.  So, we should check whether `paramType` contains
  // `String` or not first.
  if (!!(paramType & EditorCommandParamType::String)) {
    MOZ_ASSERT(!adjustedValue.IsVoid());
    aRv = editorCommand->DoCommandParam(commandData.mCommand, adjustedValue,
                                        *maybeHTMLEditor, &aSubjectPrincipal);
    return !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION) && !aRv.Failed();
  }

  // Finally, `paramType` should have `CString`.  We should use
  // `DoCommandParam()` which takes `nsACString`.
  if (!!(paramType & EditorCommandParamType::CString)) {
    NS_ConvertUTF16toUTF8 utf8Value(adjustedValue);
    MOZ_ASSERT(!utf8Value.IsVoid());
    aRv = editorCommand->DoCommandParam(commandData.mCommand, utf8Value,
                                        *maybeHTMLEditor, &aSubjectPrincipal);
    return !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION) && !aRv.Failed();
  }

  MOZ_ASSERT_UNREACHABLE(
      "Not yet implemented to handle new EditorCommandParamType");
  return false;
}

bool Document::QueryCommandEnabled(const nsAString& aHTMLCommandName,
                                   nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aRv) {
  // Only allow on HTML documents.
  if (!IsHTMLOrXHTML()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_DOCUMENT_QUERY_COMMAND_ENABLED);
    return false;
  }

  InternalCommandData commandData = ConvertToInternalCommand(aHTMLCommandName);
  if (commandData.mCommand == Command::DoNothing) {
    return false;
  }

  // cut & copy are always allowed
  if (commandData.IsCutOrCopyCommand()) {
    return nsContentUtils::IsCutCopyAllowed(this, aSubjectPrincipal);
  }

  // Report false for restricted commands
  if (commandData.IsPasteCommand() && !aSubjectPrincipal.IsSystemPrincipal()) {
    return false;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return false;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return commandManager->IsCommandEnabled(
      nsDependentCString(commandData.mXULCommandName), window);
}

bool Document::QueryCommandIndeterm(const nsAString& aHTMLCommandName,
                                    ErrorResult& aRv) {
  // Only allow on HTML documents.
  if (!IsHTMLOrXHTML()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_DOCUMENT_QUERY_COMMAND_INDETERM);
    return false;
  }

  InternalCommandData commandData = ConvertToInternalCommand(aHTMLCommandName);
  if (commandData.mCommand == Command::DoNothing) {
    return false;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return false;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();
  aRv = commandManager->GetCommandState(commandData.mXULCommandName, window,
                                        params);
  if (aRv.Failed()) {
    return false;
  }

  // If command does not have a state_mixed value, this call fails and sets
  // retval to false.  This is fine -- we want to return false in that case
  // anyway (bug 738385), so we just don't throw regardless.
  return params->GetBool("state_mixed");
}

bool Document::QueryCommandState(const nsAString& aHTMLCommandName,
                                 ErrorResult& aRv) {
  // Only allow on HTML documents.
  if (!IsHTMLOrXHTML()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_DOCUMENT_QUERY_COMMAND_STATE);
    return false;
  }

  InternalCommandData commandData = ConvertToInternalCommand(aHTMLCommandName);
  if (commandData.mCommand == Command::DoNothing) {
    return false;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return false;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (aHTMLCommandName.LowerCaseEqualsLiteral("usecss")) {
    // Per spec, state is supported for styleWithCSS but not useCSS, so we just
    // return false always.
    return false;
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();
  aRv = commandManager->GetCommandState(commandData.mXULCommandName, window,
                                        params);
  if (aRv.Failed()) {
    return false;
  }

  // handle alignment as a special case (possibly other commands too?)
  // Alignment is special because the external api is individual
  // commands but internally we use cmd_align with different
  // parameters.  When getting the state of this command, we need to
  // return the boolean for this particular alignment rather than the
  // string of 'which alignment is this?'
  switch (commandData.mCommand) {
    case Command::FormatJustifyLeft: {
      nsAutoCString currentValue;
      aRv = params->GetCString("state_attribute", currentValue);
      if (aRv.Failed()) {
        return false;
      }
      return currentValue.EqualsLiteral("left");
    }
    case Command::FormatJustifyRight: {
      nsAutoCString currentValue;
      aRv = params->GetCString("state_attribute", currentValue);
      if (aRv.Failed()) {
        return false;
      }
      return currentValue.EqualsLiteral("right");
    }
    case Command::FormatJustifyCenter: {
      nsAutoCString currentValue;
      aRv = params->GetCString("state_attribute", currentValue);
      if (aRv.Failed()) {
        return false;
      }
      return currentValue.EqualsLiteral("center");
    }
    case Command::FormatJustifyFull: {
      nsAutoCString currentValue;
      aRv = params->GetCString("state_attribute", currentValue);
      if (aRv.Failed()) {
        return false;
      }
      return currentValue.EqualsLiteral("justify");
    }
    default:
      // If command does not have a state_all value, this call fails and sets
      // retval to false.  This is fine -- we want to return false in that case
      // anyway (bug 738385), so we just succeed and return false regardless.
      return params->GetBool("state_all");
  }

  // If command does not have a state_all value, this call fails and sets
  // retval to false.  This is fine -- we want to return false in that case
  // anyway (bug 738385), so we just succeed and return false regardless.
  return params->GetBool("state_all");
}

bool Document::QueryCommandSupported(const nsAString& aHTMLCommandName,
                                     CallerType aCallerType, ErrorResult& aRv) {
  // Only allow on HTML documents.
  if (!IsHTMLOrXHTML()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_DOCUMENT_QUERY_COMMAND_SUPPORTED);
    return false;
  }

  InternalCommandData commandData = ConvertToInternalCommand(aHTMLCommandName);
  if (commandData.mCommand == Command::DoNothing) {
    return false;
  }

  // Gecko technically supports all the clipboard commands including
  // cut/copy/paste, but non-privileged content will be unable to call
  // paste, and depending on the pref "dom.allow_cut_copy", cut and copy
  // may also be disallowed to be called from non-privileged content.
  // For that reason, we report the support status of corresponding
  // command accordingly.
  if (aCallerType != CallerType::System) {
    if (commandData.IsPasteCommand()) {
      return false;
    }
    if (commandData.IsCutOrCopyCommand() &&
        !StaticPrefs::dom_allow_cut_copy()) {
      // XXXbz should we worry about correctly reporting "true" in the
      // "restricted, but we're an addon with clipboardWrite permissions" case?
      // See also nsContentUtils::IsCutCopyAllowed.
      return false;
    }
  }

  // aHTMLCommandName is supported if it can be converted to a Midas command
  return true;
}

void Document::QueryCommandValue(const nsAString& aHTMLCommandName,
                                 nsAString& aValue, ErrorResult& aRv) {
  aValue.Truncate();

  // Only allow on HTML documents.
  if (!IsHTMLOrXHTML()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_DOCUMENT_QUERY_COMMAND_VALUE);
    return;
  }

  InternalCommandData commandData = ConvertToInternalCommand(aHTMLCommandName);
  if (commandData.mCommand == Command::DoNothing) {
    // Return empty string
    return;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // this is a special command since we are calling DoCommand rather than
  // GetCommandState like the other commands
  RefPtr<nsCommandParams> params = new nsCommandParams();
  if (commandData.mCommand == Command::GetHTML) {
    aRv = params->SetBool("selection_only", true);
    if (aRv.Failed()) {
      return;
    }
    aRv = params->SetCString("format", NS_LITERAL_CSTRING("text/html"));
    if (aRv.Failed()) {
      return;
    }
    aRv =
        commandManager->DoCommand(commandData.mXULCommandName, params, window);
    if (aRv.Failed()) {
      return;
    }
    params->GetString("result", aValue);
    return;
  }

  aRv = params->SetCString("state_attribute", EmptyCString());
  if (aRv.Failed()) {
    return;
  }

  aRv = commandManager->GetCommandState(commandData.mXULCommandName, window,
                                        params);
  if (aRv.Failed()) {
    return;
  }

  // If command does not have a state_attribute value, this call fails, and
  // aValue will wind up being the empty string.  This is fine -- we want to
  // return "" in that case anyway (bug 738385), so we just return NS_OK
  // regardless.
  nsAutoCString result;
  params->GetCString("state_attribute", result);
  CopyUTF8toUTF16(result, aValue);
}

bool Document::IsEditingOnAfterFlush() {
  RefPtr<Document> doc = GetInProcessParentDocument();
  if (doc) {
    // Make sure frames are up to date, since that can affect whether
    // we're editable.
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  return IsEditingOn();
}

void Document::MaybeEditingStateChanged() {
  if (!mPendingMaybeEditingStateChanged && mMayStartLayout &&
      mUpdateNestLevel == 0 && (mContentEditableCount > 0) != IsEditingOn()) {
    if (nsContentUtils::IsSafeToRunScript()) {
      EditingStateChanged();
    } else if (!mInDestructor) {
      nsContentUtils::AddScriptRunner(
          NewRunnableMethod("Document::MaybeEditingStateChanged", this,
                            &Document::MaybeEditingStateChanged));
    }
  }
}

void Document::TearingDownEditor() {
  if (IsEditingOn()) {
    mEditingState = EditingState::eTearingDown;
    if (IsHTMLOrXHTML()) {
      RemoveContentEditableStyleSheets();
    }
  }
}

nsresult Document::TurnEditingOff() {
  NS_ASSERTION(mEditingState != EditingState::eOff, "Editing is already off.");

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsIDocShell* docshell = window->GetDocShell();
  if (!docshell) {
    return NS_ERROR_FAILURE;
  }

  bool isBeingDestroyed = false;
  docshell->IsBeingDestroyed(&isBeingDestroyed);
  if (isBeingDestroyed) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIEditingSession> editSession;
  nsresult rv = docshell->GetEditingSession(getter_AddRefs(editSession));
  NS_ENSURE_SUCCESS(rv, rv);

  // turn editing off
  rv = editSession->TearDownEditorOnWindow(window);
  NS_ENSURE_SUCCESS(rv, rv);

  mEditingState = EditingState::eOff;

  // Editor resets selection since it is being destroyed.  But if focus is
  // still into editable control, we have to initialize selection again.
  if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
    if (RefPtr<TextControlElement> textControlElement =
            TextControlElement::FromNodeOrNull(fm->GetFocusedElement())) {
      RefPtr<TextEditor> textEditor = textControlElement->GetTextEditor();
      if (textEditor) {
        textEditor->ReinitializeSelection(*textControlElement);
      }
    }
  }

  return NS_OK;
}

static bool HasPresShell(nsPIDOMWindowOuter* aWindow) {
  nsIDocShell* docShell = aWindow->GetDocShell();
  if (!docShell) {
    return false;
  }
  return docShell->GetPresShell() != nullptr;
}

nsresult Document::EditingStateChanged() {
  if (mRemovedFromDocShell) {
    return NS_OK;
  }

  if (mEditingState == EditingState::eSettingUp ||
      mEditingState == EditingState::eTearingDown) {
    // XXX We shouldn't recurse
    return NS_OK;
  }

  bool designMode = HasFlag(NODE_IS_EDITABLE);
  EditingState newState =
      designMode ? EditingState::eDesignMode
                 : (mContentEditableCount > 0 ? EditingState::eContentEditable
                                              : EditingState::eOff);
  if (mEditingState == newState) {
    // No changes in editing mode.
    return NS_OK;
  }

  if (newState == EditingState::eOff) {
    // Editing is being turned off.
    nsAutoScriptBlocker scriptBlocker;
    NotifyEditableStateChange(*this);
    return TurnEditingOff();
  }

  // Flush out style changes on our _parent_ document, if any, so that
  // our check for a presshell won't get stale information.
  if (mParentDocument) {
    mParentDocument->FlushPendingNotifications(FlushType::Style);
  }

  // get editing session, make sure this is a strong reference so the
  // window can't get deleted during the rest of this call.
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsIDocShell* docshell = window->GetDocShell();
  if (!docshell) {
    return NS_ERROR_FAILURE;
  }

  // FlushPendingNotifications might destroy our docshell.
  bool isBeingDestroyed = false;
  docshell->IsBeingDestroyed(&isBeingDestroyed);
  if (isBeingDestroyed) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIEditingSession> editSession;
  nsresult rv = docshell->GetEditingSession(getter_AddRefs(editSession));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<HTMLEditor> htmlEditor = editSession->GetHTMLEditorForWindow(window);
  if (htmlEditor) {
    // We might already have an editor if it was set up for mail, let's see
    // if this is actually the case.
    uint32_t flags = 0;
    htmlEditor->GetFlags(&flags);
    if (flags & nsIEditor::eEditorMailMask) {
      // We already have a mail editor, then we should not attempt to create
      // another one.
      return NS_OK;
    }
  }

  if (!HasPresShell(window)) {
    // We should not make the window editable or setup its editor.
    // It's probably style=display:none.
    return NS_OK;
  }

  bool makeWindowEditable = mEditingState == EditingState::eOff;
  bool spellRecheckAll = false;
  bool putOffToRemoveScriptBlockerUntilModifyingEditingState = false;
  htmlEditor = nullptr;

  {
    EditingState oldState = mEditingState;
    nsAutoEditingState push(this, EditingState::eSettingUp);

    RefPtr<PresShell> presShell = GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    // If we're entering the design mode from non-editable state, put the
    // selection at the beginning of the document for compatibility reasons.
    bool collapseSelectionAtBeginningOfDocument =
        designMode && oldState == EditingState::eOff;
    // However, mEditingState may be eOff even if there is some
    // `contenteditable` area and selection has been initialized for it because
    // mEditingState for `contenteditable` may have been scheduled to modify
    // when safe.  In such case, we should not reinitialize selection.
    if (collapseSelectionAtBeginningOfDocument && mContentEditableCount) {
      Selection* selection =
          presShell->GetSelection(nsISelectionController::SELECTION_NORMAL);
      NS_WARNING_ASSERTION(selection, "Why don't we have Selection?");
      if (selection && selection->RangeCount()) {
        // Perhaps, we don't need to check whether the selection is in
        // an editing host or not because all contents will be editable
        // in designMode. (And we don't want to make this code so complicated
        // because of legacy API.)
        collapseSelectionAtBeginningOfDocument = false;
      }
    }

    MOZ_ASSERT(mStyleSetFilled);

    // Before making this window editable, we need to modify UA style sheet
    // because new style may change whether focused element will be focusable
    // or not.
    if (IsHTMLOrXHTML()) {
      AddContentEditableStyleSheetsToStyleSet(designMode);
    }

    if (designMode) {
      // designMode is being turned on (overrides contentEditable).
      spellRecheckAll = oldState == EditingState::eContentEditable;
    }

    // Adjust focused element with new style but blur event shouldn't be fired
    // until mEditingState is modified with newState.
    nsAutoScriptBlocker scriptBlocker;
    if (designMode) {
      nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
      nsIContent* focusedContent = nsFocusManager::GetFocusedDescendant(
          window, nsFocusManager::eOnlyCurrentWindow,
          getter_AddRefs(focusedWindow));
      if (focusedContent) {
        nsIFrame* focusedFrame = focusedContent->GetPrimaryFrame();
        bool clearFocus = focusedFrame ? !focusedFrame->IsFocusable()
                                       : !focusedContent->IsFocusable();
        if (clearFocus) {
          nsFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            fm->ClearFocus(window);
            // If we need to dispatch blur event, we should put off after
            // modifying mEditingState since blur event handler may change
            // designMode state again.
            putOffToRemoveScriptBlockerUntilModifyingEditingState = true;
          }
        }
      }
    }

    if (makeWindowEditable) {
      // Editing is being turned on (through designMode or contentEditable)
      // Turn on editor.
      // XXX This can cause flushing which can change the editing state, so make
      //     sure to avoid recursing.
      rv = editSession->MakeWindowEditable(window, "html", false, false, true);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // XXX Need to call TearDownEditorOnWindow for all failures.
    htmlEditor = docshell->GetHTMLEditor();
    if (!htmlEditor) {
      // Return NS_OK even though we've failed to create an editor here.  This
      // is so that the setter of designMode on non-HTML documents does not
      // fail.
      // This is OK to do because in nsEditingSession::SetupEditorOnWindow() we
      // would detect that we can't support the mimetype if appropriate and
      // would fall onto the eEditorErrorCantEditMimeType path.
      return NS_OK;
    }

    if (collapseSelectionAtBeginningOfDocument) {
      htmlEditor->BeginningOfDocument();
    }

    if (putOffToRemoveScriptBlockerUntilModifyingEditingState) {
      nsContentUtils::AddScriptBlocker();
    }
  }

  mEditingState = newState;
  if (putOffToRemoveScriptBlockerUntilModifyingEditingState) {
    nsContentUtils::RemoveScriptBlocker();
    // If mEditingState is overwritten by another call and already disabled
    // the editing, we shouldn't keep making window editable.
    if (mEditingState == EditingState::eOff) {
      return NS_OK;
    }
  }

  if (makeWindowEditable) {
    // Set the editor to not insert br's on return when in p
    // elements by default.
    // XXX Do we only want to do this for designMode?
    // Note that it doesn't matter what CallerType we pass, because the callee
    // doesn't use it for this command.  Play it safe and pass the more
    // restricted one.
    ErrorResult errorResult;
    nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
    Unused << ExecCommand(NS_LITERAL_STRING("insertBrOnReturn"), false,
                          NS_LITERAL_STRING("false"),
                          // Principal doesn't matter here, because the
                          // insertBrOnReturn command doesn't use it.   Still
                          // it's too bad we can't easily grab a nullprincipal
                          // from somewhere without allocating one..
                          *principal, errorResult);

    if (errorResult.Failed()) {
      // Editor setup failed. Editing is not on after all.
      // XXX Should we reset the editable flag on nodes?
      editSession->TearDownEditorOnWindow(window);
      mEditingState = EditingState::eOff;

      return errorResult.StealNSResult();
    }
  }

  // Resync the editor's spellcheck state.
  if (spellRecheckAll) {
    nsCOMPtr<nsISelectionController> selectionController =
        htmlEditor->GetSelectionController();
    if (NS_WARN_IF(!selectionController)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<Selection> spellCheckSelection = selectionController->GetSelection(
        nsISelectionController::SELECTION_SPELLCHECK);
    if (spellCheckSelection) {
      spellCheckSelection->RemoveAllRanges(IgnoreErrors());
    }
  }
  htmlEditor->SyncRealTimeSpell();

  MaybeDispatchCheckKeyPressEventModelEvent();

  return NS_OK;
}

// Helper class, used below in ChangeContentEditableCount().
class DeferredContentEditableCountChangeEvent : public Runnable {
 public:
  DeferredContentEditableCountChangeEvent(Document* aDoc, Element* aElement)
      : mozilla::Runnable("DeferredContentEditableCountChangeEvent"),
        mDoc(aDoc),
        mElement(aElement) {}

  NS_IMETHOD Run() override {
    if (mElement && mElement->OwnerDoc() == mDoc) {
      mDoc->DeferredContentEditableCountChange(mElement);
    }
    return NS_OK;
  }

 private:
  RefPtr<Document> mDoc;
  RefPtr<Element> mElement;
};

void Document::ChangeContentEditableCount(Element* aElement, int32_t aChange) {
  NS_ASSERTION(int32_t(mContentEditableCount) + aChange >= 0,
               "Trying to decrement too much.");

  mContentEditableCount += aChange;

  nsContentUtils::AddScriptRunner(
      new DeferredContentEditableCountChangeEvent(this, aElement));
}

void Document::DeferredContentEditableCountChange(Element* aElement) {
  if (mParser ||
      (mUpdateNestLevel > 0 && (mContentEditableCount > 0) != IsEditingOn())) {
    return;
  }

  EditingState oldState = mEditingState;

  nsresult rv = EditingStateChanged();
  NS_ENSURE_SUCCESS_VOID(rv);

  if (oldState == mEditingState &&
      mEditingState == EditingState::eContentEditable) {
    // We just changed the contentEditable state of a node, we need to reset
    // the spellchecking state of that node.
    if (aElement) {
      nsPIDOMWindowOuter* window = GetWindow();
      if (!window) {
        return;
      }

      nsIDocShell* docshell = window->GetDocShell();
      if (!docshell) {
        return;
      }

      RefPtr<HTMLEditor> htmlEditor = docshell->GetHTMLEditor();
      if (htmlEditor) {
        RefPtr<nsRange> range = nsRange::Create(aElement);
        IgnoredErrorResult res;
        range->SelectNode(*aElement, res);
        if (res.Failed()) {
          // The node might be detached from the document at this point,
          // which would cause this call to fail.  In this case, we can
          // safely ignore the contenteditable count change.
          return;
        }

        nsCOMPtr<nsIInlineSpellChecker> spellChecker;
        rv = htmlEditor->GetInlineSpellChecker(false,
                                               getter_AddRefs(spellChecker));
        NS_ENSURE_SUCCESS_VOID(rv);

        if (spellChecker) {
          rv = spellChecker->SpellCheckRange(range);
          NS_ENSURE_SUCCESS_VOID(rv);
        }
      }
    }
  }
}

void Document::MaybeDispatchCheckKeyPressEventModelEvent() {
  // Currently, we need to check only when we're becoming editable for
  // contenteditable.
  if (mEditingState != EditingState::eContentEditable) {
    return;
  }

  if (mHasBeenEditable) {
    return;
  }
  mHasBeenEditable = true;

  // Dispatch "CheckKeyPressEventModel" event.  That is handled only by
  // KeyPressEventModelCheckerChild.  Then, it calls SetKeyPressEventModel()
  // with proper keypress event for the active web app.
  WidgetEvent checkEvent(true, eUnidentifiedEvent);
  checkEvent.mSpecifiedEventType = nsGkAtoms::onCheckKeyPressEventModel;
  checkEvent.mFlags.mCancelable = false;
  checkEvent.mFlags.mBubbles = false;
  checkEvent.mFlags.mOnlySystemGroupDispatch = true;
  // Post the event rather than dispatching it synchronously because we need
  // a call of SetKeyPressEventModel() before first key input.  Therefore, we
  // can avoid paying unnecessary runtime cost for most web apps.
  (new AsyncEventDispatcher(this, checkEvent))->PostDOMEvent();
}

void Document::SetKeyPressEventModel(uint16_t aKeyPressEventModel) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return;
  }
  presShell->SetKeyPressEventModel(aKeyPressEventModel);
}

TimeStamp Document::LastFocusTime() const { return mLastFocusTime; }

void Document::SetLastFocusTime(const TimeStamp& aFocusTime) {
  MOZ_DIAGNOSTIC_ASSERT(!aFocusTime.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mLastFocusTime.IsNull() ||
                        aFocusTime >= mLastFocusTime);
  mLastFocusTime = aFocusTime;
}

void Document::GetReferrer(nsAString& aReferrer) const {
  aReferrer.Truncate();
  if (!mReferrerInfo) {
    return;
  }

  nsCOMPtr<nsIURI> referrer = mReferrerInfo->GetComputedReferrer();
  if (!referrer) {
    return;
  }

  nsAutoCString uri;
  nsresult rv = URLDecorationStripper::StripTrackingIdentifiers(referrer, uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  CopyUTF8toUTF16(uri, aReferrer);
}

void Document::GetCookie(nsAString& aCookie, ErrorResult& rv) {
  aCookie.Truncate();  // clear current cookie in case service fails;
                       // no cookie isn't an error condition.

  if (mDisableCookieAccess) {
    return;
  }

  // If the document's sandboxed origin flag is set, access to read cookies
  // is prohibited.
  if (mSandboxFlags & SANDBOXED_ORIGIN) {
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  StorageAccess storageAccess = StorageAllowedForDocument(this);
  if (storageAccess == StorageAccess::eDeny) {
    return;
  }

  if (ShouldPartitionStorage(storageAccess) &&
      !StoragePartitioningEnabled(storageAccess, CookieJarSettings())) {
    return;
  }

  // If the document is a cookie-averse Document... return the empty string.
  if (IsCookieAverse()) {
    return;
  }

  // not having a cookie service isn't an error
  nsCOMPtr<nsICookieService> service =
      do_GetService(NS_COOKIESERVICE_CONTRACTID);
  if (service) {
    // Get a URI from the document principal. We use the original
    // content URI in case the domain was changed by SetDomain
    nsCOMPtr<nsIURI> principalURI;
    NodePrincipal()->GetURI(getter_AddRefs(principalURI));

    if (!principalURI) {
      // Document's principal is not a content or null (may be system), so
      // can't set cookies

      return;
    }

    nsCOMPtr<nsIChannel> channel(mChannel);
    if (!channel) {
      channel = CreateDummyChannelForCookies(principalURI);
      if (!channel) {
        return;
      }
    }

    nsAutoCString cookie;
    service->GetCookieString(principalURI, channel, cookie);
    // CopyUTF8toUTF16 doesn't handle error
    // because it assumes that the input is valid.
    UTF_8_ENCODING->DecodeWithoutBOMHandling(cookie, aCookie);
  }
}

void Document::SetCookie(const nsAString& aCookie, ErrorResult& aRv) {
  if (mDisableCookieAccess) {
    return;
  }

  // If the document's sandboxed origin flag is set, access to write cookies
  // is prohibited.
  if (mSandboxFlags & SANDBOXED_ORIGIN) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  StorageAccess storageAccess = StorageAllowedForDocument(this);
  if (storageAccess == StorageAccess::eDeny) {
    return;
  }

  if (ShouldPartitionStorage(storageAccess) &&
      !StoragePartitioningEnabled(storageAccess, CookieJarSettings())) {
    return;
  }

  // If the document is a cookie-averse Document... do nothing.
  if (IsCookieAverse()) {
    return;
  }

  if (!mDocumentURI) {
    return;
  }

  // The code for getting the URI matches Navigator::CookieEnabled
  nsCOMPtr<nsIURI> principalURI;
  NodePrincipal()->GetURI(getter_AddRefs(principalURI));

  if (!principalURI) {
    // Document's principal is not a content or null (may be system), so
    // can't set cookies

    return;
  }

  nsCOMPtr<nsIChannel> channel(mChannel);
  if (!channel) {
    channel = CreateDummyChannelForCookies(principalURI);
    if (!channel) {
      return;
    }
  }

  // not having a cookie service isn't an error
  nsCOMPtr<nsICookieService> service =
      do_GetService(NS_COOKIESERVICE_CONTRACTID);
  if (!service) {
    return;
  }

  NS_ConvertUTF16toUTF8 cookie(aCookie);
  nsresult rv = service->SetCookieString(principalURI, cookie, channel);

  // No warning messages here.
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(ToSupports(this), "document-set-cookie",
                                     nsString(aCookie).get());
  }
}

already_AddRefed<nsIChannel> Document::CreateDummyChannelForCookies(
    nsIURI* aContentURI) {
  // The cookie service reads the privacy status of the channel we pass to it in
  // order to determine which cookie database to query.  In some cases we don't
  // have a proper channel to hand it to the cookie service though.  This
  // function creates a dummy channel that is not used to load anything, for the
  // sole purpose of handing it to the cookie service.  DO NOT USE THIS CHANNEL
  // FOR ANY OTHER PURPOSE.
  MOZ_ASSERT(!mChannel);

  // The following channel is never openend, so it does not matter what
  // securityFlags we pass; let's follow the principle of least privilege.
  nsCOMPtr<nsIChannel> channel;
  NS_NewChannel(getter_AddRefs(channel), aContentURI, this,
                nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                nsIContentPolicy::TYPE_INVALID);
  nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(channel);
  nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
  if (!pbChannel || !loadContext) {
    return nullptr;
  }
  pbChannel->SetPrivate(loadContext->UsePrivateBrowsing());

  return channel.forget();
}

ReferrerPolicy Document::GetReferrerPolicy() const {
  return mReferrerInfo ? mReferrerInfo->ReferrerPolicy()
                       : ReferrerPolicy::_empty;
}

void Document::GetAlinkColor(nsAString& aAlinkColor) {
  aAlinkColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetALink(aAlinkColor);
  }
}

void Document::SetAlinkColor(const nsAString& aAlinkColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetALink(aAlinkColor);
  }
}

void Document::GetLinkColor(nsAString& aLinkColor) {
  aLinkColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetLink(aLinkColor);
  }
}

void Document::SetLinkColor(const nsAString& aLinkColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetLink(aLinkColor);
  }
}

void Document::GetVlinkColor(nsAString& aVlinkColor) {
  aVlinkColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetVLink(aVlinkColor);
  }
}

void Document::SetVlinkColor(const nsAString& aVlinkColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetVLink(aVlinkColor);
  }
}

void Document::GetBgColor(nsAString& aBgColor) {
  aBgColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetBgColor(aBgColor);
  }
}

void Document::SetBgColor(const nsAString& aBgColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetBgColor(aBgColor);
  }
}

void Document::GetFgColor(nsAString& aFgColor) {
  aFgColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetText(aFgColor);
  }
}

void Document::SetFgColor(const nsAString& aFgColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetText(aFgColor);
  }
}

void Document::CaptureEvents() { WarnOnceAbout(Document::eUseOfCaptureEvents); }

void Document::ReleaseEvents() { WarnOnceAbout(Document::eUseOfReleaseEvents); }

HTMLAllCollection* Document::All() {
  if (!mAll) {
    mAll = new HTMLAllCollection(this);
  }
  return mAll;
}

nsresult Document::GetSrcdocData(nsAString& aSrcdocData) {
  if (mIsSrcdocDocument) {
    nsCOMPtr<nsIInputStreamChannel> inStrmChan = do_QueryInterface(mChannel);
    if (inStrmChan) {
      return inStrmChan->GetSrcdocData(aSrcdocData);
    }
  }
  aSrcdocData = VoidString();
  return NS_OK;
}

Nullable<WindowProxyHolder> Document::GetDefaultView() const {
  nsPIDOMWindowOuter* win = GetWindow();
  if (!win) {
    return nullptr;
  }
  return WindowProxyHolder(win->GetBrowsingContext());
}

nsIContent* Document::GetUnretargetedFocusedContent() const {
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (!window) {
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsIContent* focusedContent = nsFocusManager::GetFocusedDescendant(
      window, nsFocusManager::eOnlyCurrentWindow,
      getter_AddRefs(focusedWindow));
  if (!focusedContent) {
    return nullptr;
  }
  // be safe and make sure the element is from this document
  if (focusedContent->OwnerDoc() != this) {
    return nullptr;
  }

  if (focusedContent->ChromeOnlyAccess()) {
    return focusedContent->FindFirstNonChromeOnlyAccessContent();
  }
  return focusedContent;
}

Element* Document::GetActiveElement() {
  // Get the focused element.
  Element* focusedElement = GetRetargetedFocusedElement();
  if (focusedElement) {
    return focusedElement;
  }

  // No focused element anywhere in this document.  Try to get the BODY.
  if (IsHTMLOrXHTML()) {
    Element* bodyElement = AsHTMLDocument()->GetBody();
    if (bodyElement) {
      return bodyElement;
    }
    // Special case to handle the transition to XHTML from XUL documents
    // where there currently isn't a body element, but we need to match the
    // XUL behavior. This should be removed when bug 1540278 is resolved.
    if (nsContentUtils::IsChromeDoc(this)) {
      Element* docElement = GetDocumentElement();
      if (docElement && docElement->IsXULElement()) {
        return docElement;
      }
    }
    // Because of IE compatibility, return null when html document doesn't have
    // a body.
    return nullptr;
  }

  // If we couldn't get a BODY, return the root element.
  return GetDocumentElement();
}

Element* Document::GetCurrentScript() {
  nsCOMPtr<Element> el(do_QueryInterface(ScriptLoader()->GetCurrentScript()));
  return el;
}

void Document::ReleaseCapture() const {
  // only release the capture if the caller can access it. This prevents a
  // page from stopping a scrollbar grab for example.
  nsCOMPtr<nsINode> node = PresShell::GetCapturingContent();
  if (node && nsContentUtils::CanCallerAccess(node)) {
    PresShell::ReleaseCapturingContent();
  }
}

nsIURI* Document::GetBaseURI(bool aTryUseXHRDocBaseURI) const {
  if (aTryUseXHRDocBaseURI && mChromeXHRDocBaseURI) {
    return mChromeXHRDocBaseURI;
  }

  return GetDocBaseURI();
}

void Document::SetBaseURI(nsIURI* aURI) {
  if (!aURI && !mDocumentBaseURI) {
    return;
  }

  // Don't do anything if the URI wasn't actually changed.
  if (aURI && mDocumentBaseURI) {
    bool equalBases = false;
    mDocumentBaseURI->Equals(aURI, &equalBases);
    if (equalBases) {
      return;
    }
  }

  mDocumentBaseURI = aURI;
  RefreshLinkHrefs();
}

Result<nsCOMPtr<nsIURI>, nsresult> Document::ResolveWithBaseURI(
    const nsAString& aURI) {
  nsCOMPtr<nsIURI> resolvedURI;
  MOZ_TRY(
      NS_NewURI(getter_AddRefs(resolvedURI), aURI, nullptr, GetDocBaseURI()));
  return resolvedURI;
}

URLExtraData* Document::DefaultStyleAttrURLData() {
  MOZ_ASSERT(NS_IsMainThread());
  nsIURI* baseURI = GetDocBaseURI();
  nsIPrincipal* principal = NodePrincipal();
  bool equals;
  if (!mCachedURLData || mCachedURLData->BaseURI() != baseURI ||
      mCachedURLData->Principal() != principal || !mCachedReferrerInfo ||
      NS_FAILED(mCachedURLData->ReferrerInfo()->Equals(mCachedReferrerInfo,
                                                       &equals)) ||
      !equals) {
    mCachedReferrerInfo = ReferrerInfo::CreateForInternalCSSResources(this);
    mCachedURLData = new URLExtraData(baseURI, mCachedReferrerInfo, principal);
  }
  return mCachedURLData;
}

void Document::SetDocumentCharacterSet(NotNull<const Encoding*> aEncoding) {
  if (mCharacterSet != aEncoding) {
    mCharacterSet = aEncoding;
    mEncodingMenuDisabled = aEncoding == UTF_8_ENCODING;
    RecomputeLanguageFromCharset();

    if (nsPresContext* context = GetPresContext()) {
      context->DocumentCharSetChanged(aEncoding);
    }
  }
}

void Document::GetSandboxFlagsAsString(nsAString& aFlags) {
  nsContentUtils::SandboxFlagsToString(mSandboxFlags, aFlags);
}

void Document::GetHeaderData(nsAtom* aHeaderField, nsAString& aData) const {
  aData.Truncate();
  const DocHeaderData* data = mHeaderData;
  while (data) {
    if (data->mField == aHeaderField) {
      aData = data->mData;

      break;
    }
    data = data->mNext;
  }
}

void Document::SetHeaderData(nsAtom* aHeaderField, const nsAString& aData) {
  if (!aHeaderField) {
    NS_ERROR("null headerField");
    return;
  }

  if (!mHeaderData) {
    if (!aData.IsEmpty()) {  // don't bother storing empty string
      mHeaderData = new DocHeaderData(aHeaderField, aData);
    }
  } else {
    DocHeaderData* data = mHeaderData;
    DocHeaderData** lastPtr = &mHeaderData;
    bool found = false;
    do {  // look for existing and replace
      if (data->mField == aHeaderField) {
        if (!aData.IsEmpty()) {
          data->mData.Assign(aData);
        } else {  // don't store empty string
          *lastPtr = data->mNext;
          data->mNext = nullptr;
          delete data;
        }
        found = true;

        break;
      }
      lastPtr = &(data->mNext);
      data = *lastPtr;
    } while (data);

    if (!aData.IsEmpty() && !found) {
      // didn't find, append
      *lastPtr = new DocHeaderData(aHeaderField, aData);
    }
  }

  if (aHeaderField == nsGkAtoms::headerContentLanguage) {
    CopyUTF16toUTF8(aData, mContentLanguage);
    mMayNeedFontPrefsUpdate = true;
    if (auto* presContext = GetPresContext()) {
      presContext->ContentLanguageChanged();
    }
  }

  if (aHeaderField == nsGkAtoms::headerDefaultStyle) {
    SetPreferredStyleSheetSet(aData);
  }

  if (aHeaderField == nsGkAtoms::refresh) {
    // We get into this code before we have a script global yet, so get to
    // our container via mDocumentContainer.
    nsCOMPtr<nsIRefreshURI> refresher(mDocumentContainer);
    if (refresher) {
      // Note: using mDocumentURI instead of mBaseURI here, for consistency
      // (used to just use the current URI of our webnavigation, but that
      // should really be the same thing).  Note that this code can run
      // before the current URI of the webnavigation has been updated, so we
      // can't assert equality here.
      refresher->SetupRefreshURIFromHeader(mDocumentURI, NodePrincipal(),
                                           InnerWindowID(),
                                           NS_ConvertUTF16toUTF8(aData));
    }
  }

  if (aHeaderField == nsGkAtoms::headerDNSPrefetchControl &&
      mAllowDNSPrefetch) {
    // Chromium treats any value other than 'on' (case insensitive) as 'off'.
    mAllowDNSPrefetch = aData.IsEmpty() || aData.LowerCaseEqualsLiteral("on");
  }

  if (aHeaderField == nsGkAtoms::viewport ||
      aHeaderField == nsGkAtoms::handheldFriendly) {
    mViewportType = Unknown;
  }
}

void Document::TryChannelCharset(nsIChannel* aChannel, int32_t& aCharsetSource,
                                 NotNull<const Encoding*>& aEncoding,
                                 nsHtml5TreeOpExecutor* aExecutor) {
  if (aChannel) {
    nsAutoCString charsetVal;
    nsresult rv = aChannel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
      const Encoding* preferred = Encoding::ForLabel(charsetVal);
      if (preferred) {
        aEncoding = WrapNotNull(preferred);
        aCharsetSource = kCharsetFromChannel;
        return;
      } else if (aExecutor && !charsetVal.IsEmpty()) {
        aExecutor->ComplainAboutBogusProtocolCharset(this);
      }
    }
  }
}

static inline void AssertNoStaleServoDataIn(nsINode& aSubtreeRoot) {
#ifdef DEBUG
  for (nsINode* node : ShadowIncludingTreeIterator(aSubtreeRoot)) {
    const Element* element = Element::FromNode(node);
    if (!element) {
      continue;
    }
    MOZ_ASSERT(!element->HasServoData());
  }
#endif
}

already_AddRefed<PresShell> Document::CreatePresShell(
    nsPresContext* aContext, nsViewManager* aViewManager) {
  MOZ_ASSERT(!mPresShell, "We have a presshell already!");

  NS_ENSURE_FALSE(GetBFCacheEntry(), nullptr);

  AssertNoStaleServoDataIn(*this);

  RefPtr<PresShell> presShell = new PresShell(this);
  // Note: we don't hold a ref to the shell (it holds a ref to us)
  mPresShell = presShell;

  bool hadStyleSheets = mStyleSetFilled;
  if (!hadStyleSheets) {
    FillStyleSet();
  }

  presShell->Init(aContext, aViewManager);

  if (hadStyleSheets) {
    // Gaining a shell causes changes in how media queries are evaluated, so
    // invalidate that.
    aContext->MediaFeatureValuesChanged({MediaFeatureChange::kAllChanges});
  } else {
    // Otherwise, we need to at least recompute the initial style now that our
    // resolution and such may have changed. This is done by
    // MediaFeatureValuesChanged above otherwise, see
    // kMediaFeaturesAffectingDefaultStyle.
    mStyleSet->ClearCachedStyleData();
  }

  // Make sure to never paint if we belong to an invisible DocShell.
  nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
  if (docShell && docShell->IsInvisible()) {
    presShell->SetNeverPainting(true);
  }

  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug,
          ("DOCUMENT %p with PressShell %p and DocShell %p", this,
           presShell.get(), docShell.get()));

  mExternalResourceMap.ShowViewers();

  UpdateFrameRequestCallbackSchedulingState();

  if (mDocumentL10n) {
    // In case we already accumulated mutations,
    // we'll trigger the refresh driver now.
    mDocumentL10n->OnCreatePresShell();
  }

  // Now that we have a shell, we might have @font-face rules (the presence of a
  // shell may change which rules apply to us). We don't need to do anything
  // like EnsureStyleFlush or such, there's nothing to update yet and when stuff
  // is ready to update we'll flush the font set.
  MarkUserFontSetDirty();

  return presShell.forget();
}

void Document::UpdateFrameRequestCallbackSchedulingState(
    PresShell* aOldPresShell) {
  // If the condition for shouldBeScheduled changes to depend on some other
  // variable, add UpdateFrameRequestCallbackSchedulingState() calls to the
  // places where that variable can change.
  bool shouldBeScheduled = mPresShell && IsEventHandlingEnabled() &&
                           !mFrameRequestCallbacks.IsEmpty();
  if (shouldBeScheduled == mFrameRequestCallbacksScheduled) {
    // nothing to do
    return;
  }

  PresShell* presShell = aOldPresShell ? aOldPresShell : mPresShell;
  MOZ_RELEASE_ASSERT(presShell);

  nsRefreshDriver* rd = presShell->GetPresContext()->RefreshDriver();
  if (shouldBeScheduled) {
    rd->ScheduleFrameRequestCallbacks(this);
  } else {
    rd->RevokeFrameRequestCallbacks(this);
  }

  mFrameRequestCallbacksScheduled = shouldBeScheduled;
}

void Document::TakeFrameRequestCallbacks(nsTArray<FrameRequest>& aCallbacks) {
  MOZ_ASSERT(aCallbacks.IsEmpty());
  aCallbacks.SwapElements(mFrameRequestCallbacks);
  mCanceledFrameRequestCallbacks.clear();
  // No need to manually remove ourselves from the refresh driver; it will
  // handle that part.  But we do have to update our state.
  mFrameRequestCallbacksScheduled = false;
}

bool Document::ShouldThrottleFrameRequests() {
  if (mStaticCloneCount > 0) {
    // Even if we're not visible, a static clone may be, so run at full speed.
    return false;
  }

  if (Hidden()) {
    // We're not visible (probably in a background tab or the bf cache).
    return true;
  }

  if (!mPresShell) {
    return false;  // Can't do anything smarter.
  }

  nsIFrame* frame = mPresShell->GetRootFrame();
  if (!frame) {
    return false;  // Can't do anything smarter.
  }

  nsIFrame* displayRootFrame = nsLayoutUtils::GetDisplayRootFrame(frame);
  if (!displayRootFrame) {
    return false;  // Can't do anything smarter.
  }

  if (!displayRootFrame->DidPaintPresShell(mPresShell)) {
    // We didn't get painted during the last paint, so we're not visible.
    // Throttle. Note that because we have to paint this document at least
    // once to unthrottle it, we will drop one requestAnimationFrame frame
    // when a document that previously wasn't visible scrolls into view. This
    // is acceptable since it would happen outside the viewport on APZ
    // platforms and is unlikely to be human-perceivable on non-APZ platforms.
    return true;
  }

  // We got painted during the last paint, so run at full speed.
  return false;
}

void Document::DeletePresShell() {
  mExternalResourceMap.HideViewers();
  if (nsPresContext* presContext = mPresShell->GetPresContext()) {
    presContext->RefreshDriver()->CancelPendingFullscreenEvents(this);
  }

  // When our shell goes away, request that all our images be immediately
  // discarded, so we don't carry around decoded image data for a document we
  // no longer intend to paint.
  ImageTracker()->RequestDiscardAll();

  // Now that we no longer have a shell, we need to forget about any FontFace
  // objects for @font-face rules that came from the style set. There's no need
  // to call EnsureStyleFlush either, the shell is going away anyway, so there's
  // no point on it.
  MarkUserFontSetDirty();

  if (mResizeObserverController) {
    mResizeObserverController->ShellDetachedFromDocument();
  }

  if (IsEditingOn()) {
    TurnEditingOff();
  }

  PresShell* oldPresShell = mPresShell;
  mPresShell = nullptr;
  UpdateFrameRequestCallbackSchedulingState(oldPresShell);

  ClearStaleServoData();
  AssertNoStaleServoDataIn(*this);

  mStyleSet->ShellDetachedFromDocument();
  mStyleSetFilled = false;
  mQuirkSheetAdded = false;
  mContentEditableSheetAdded = false;
  mDesignModeSheetAdded = false;
}

void Document::SetBFCacheEntry(nsIBFCacheEntry* aEntry) {
  MOZ_ASSERT(IsBFCachingAllowed() || !aEntry, "You should have checked!");

  if (mPresShell) {
    if (aEntry) {
      mPresShell->StopObservingRefreshDriver();
    } else if (mBFCacheEntry) {
      mPresShell->StartObservingRefreshDriver();
    }
  }
  mBFCacheEntry = aEntry;
}

static void SubDocClearEntry(PLDHashTable* table, PLDHashEntryHdr* entry) {
  SubDocMapEntry* e = static_cast<SubDocMapEntry*>(entry);

  NS_RELEASE(e->mKey);
  if (e->mSubDocument) {
    e->mSubDocument->SetParentDocument(nullptr);
    NS_RELEASE(e->mSubDocument);
  }
}

static void SubDocInitEntry(PLDHashEntryHdr* entry, const void* key) {
  SubDocMapEntry* e =
      const_cast<SubDocMapEntry*>(static_cast<const SubDocMapEntry*>(entry));

  e->mKey = const_cast<Element*>(static_cast<const Element*>(key));
  NS_ADDREF(e->mKey);

  e->mSubDocument = nullptr;
}

nsresult Document::SetSubDocumentFor(Element* aElement, Document* aSubDoc) {
  NS_ENSURE_TRUE(aElement, NS_ERROR_UNEXPECTED);

  if (!aSubDoc) {
    // aSubDoc is nullptr, remove the mapping

    if (mSubDocuments) {
      if (Document* subDoc = GetSubDocumentFor(aElement)) {
        subDoc->SetAllowPaymentRequest(false);
      }
      mSubDocuments->Remove(aElement);
    }
  } else {
    if (!mSubDocuments) {
      // Create a new hashtable

      static const PLDHashTableOps hash_table_ops = {
          PLDHashTable::HashVoidPtrKeyStub, PLDHashTable::MatchEntryStub,
          PLDHashTable::MoveEntryStub, SubDocClearEntry, SubDocInitEntry};

      mSubDocuments = new PLDHashTable(&hash_table_ops, sizeof(SubDocMapEntry));
    }

    // Add a mapping to the hash table
    auto entry =
        static_cast<SubDocMapEntry*>(mSubDocuments->Add(aElement, fallible));

    if (!entry) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (entry->mSubDocument) {
      entry->mSubDocument->SetAllowPaymentRequest(false);
      entry->mSubDocument->SetParentDocument(nullptr);

      // Release the old sub document
      NS_RELEASE(entry->mSubDocument);
    }

    entry->mSubDocument = aSubDoc;
    NS_ADDREF(entry->mSubDocument);

    // set allowpaymentrequest for the binding subdocument
    if (!mAllowPaymentRequest) {
      aSubDoc->SetAllowPaymentRequest(false);
    } else {
      nsresult rv = nsContentUtils::CheckSameOrigin(aElement, aSubDoc);
      if (NS_SUCCEEDED(rv)) {
        aSubDoc->SetAllowPaymentRequest(true);
      } else {
        if (aElement->IsHTMLElement(nsGkAtoms::iframe) &&
            aElement->GetBoolAttr(nsGkAtoms::allowpaymentrequest)) {
          aSubDoc->SetAllowPaymentRequest(true);
        } else {
          aSubDoc->SetAllowPaymentRequest(false);
        }
      }
    }

    aSubDoc->SetParentDocument(this);
  }

  return NS_OK;
}

Document* Document::GetSubDocumentFor(nsIContent* aContent) const {
  if (mSubDocuments && aContent->IsElement()) {
    auto entry = static_cast<SubDocMapEntry*>(
        mSubDocuments->Search(aContent->AsElement()));

    if (entry) {
      return entry->mSubDocument;
    }
  }

  return nullptr;
}

Element* Document::FindContentForSubDocument(Document* aDocument) const {
  NS_ENSURE_TRUE(aDocument, nullptr);

  if (!mSubDocuments) {
    return nullptr;
  }

  for (auto iter = mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<SubDocMapEntry*>(iter.Get());
    if (entry->mSubDocument == aDocument) {
      return entry->mKey;
    }
  }
  return nullptr;
}

bool Document::IsNodeOfType(uint32_t aFlags) const { return false; }

Element* Document::GetRootElement() const {
  return (mCachedRootElement && mCachedRootElement->GetParentNode() == this)
             ? mCachedRootElement
             : GetRootElementInternal();
}

Element* Document::GetUnfocusedKeyEventTarget() { return GetRootElement(); }

Element* Document::GetRootElementInternal() const {
  // We invoke GetRootElement() immediately before the servo traversal, so we
  // should always have a cache hit from Servo.
  MOZ_ASSERT(NS_IsMainThread());

  // Loop backwards because any non-elements, such as doctypes and PIs
  // are likely to appear before the root element.
  for (nsIContent* child = GetLastChild(); child;
       child = child->GetPreviousSibling()) {
    if (Element* element = Element::FromNode(child)) {
      const_cast<Document*>(this)->mCachedRootElement = element;
      return element;
    }
  }

  const_cast<Document*>(this)->mCachedRootElement = nullptr;
  return nullptr;
}

nsresult Document::InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                     bool aNotify) {
  if (aKid->IsElement() && GetRootElement()) {
    NS_WARNING("Inserting root element when we already have one");
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  return nsINode::InsertChildBefore(aKid, aBeforeThis, aNotify);
}

void Document::RemoveChildNode(nsIContent* aKid, bool aNotify) {
  Maybe<mozAutoDocUpdate> updateBatch;
  if (aKid->IsElement()) {
    updateBatch.emplace(this, aNotify);
    // Destroy the link map up front before we mess with the child list.
    DestroyElementMaps();
  }

  // Preemptively clear mCachedRootElement, since we may be about to remove it
  // from our child list, and we don't want to return this maybe-obsolete value
  // from any GetRootElement() calls that happen inside of RemoveChildNode().
  // (NOTE: for this to be useful, RemoveChildNode() must NOT trigger any
  // GetRootElement() calls until after it's removed the child from mChildren.
  // Any call before that point would restore this soon-to-be-obsolete cached
  // answer, and our clearing here would be fruitless.)
  mCachedRootElement = nullptr;
  nsINode::RemoveChildNode(aKid, aNotify);
  MOZ_ASSERT(mCachedRootElement != aKid,
             "Stale pointer in mCachedRootElement, after we tried to clear it "
             "(maybe somebody called GetRootElement() too early?)");
}

void Document::AddStyleSheetToStyleSets(StyleSheet& aSheet) {
  if (mStyleSetFilled) {
    mStyleSet->AddDocStyleSheet(aSheet);
    ApplicableStylesChanged();
  }
}

void Document::RecordShadowStyleChange(ShadowRoot& aShadowRoot) {
  mStyleSet->RecordShadowStyleChange(aShadowRoot);
  ApplicableStylesChanged();
}

void Document::ApplicableStylesChanged() {
  // TODO(emilio): if we decide to resolve style in display: none iframes, then
  // we need to always track style changes and remove the mStyleSetFilled.
  if (!mStyleSetFilled) {
    return;
  }

  MarkUserFontSetDirty();
  PresShell* ps = GetPresShell();
  if (!ps) {
    return;
  }

  ps->EnsureStyleFlush();
  nsPresContext* pc = ps->GetPresContext();
  if (!pc) {
    return;
  }

  pc->MarkCounterStylesDirty();
  pc->MarkFontFeatureValuesDirty();
  pc->RestyleManager()->NextRestyleIsForCSSRuleChanges();
}

void Document::RemoveStyleSheetFromStyleSets(StyleSheet& aSheet) {
  if (mStyleSetFilled) {
    mStyleSet->RemoveStyleSheet(aSheet);
    ApplicableStylesChanged();
  }
}

void Document::RemoveStyleSheet(StyleSheet& aSheet) {
  RefPtr<StyleSheet> sheet = DocumentOrShadowRoot::RemoveSheet(aSheet);

  if (!sheet) {
    NS_ASSERTION(mInUnlinkOrDeletion, "stylesheet not found");
    return;
  }

  if (!mIsGoingAway && sheet->IsApplicable()) {
    RemoveStyleSheetFromStyleSets(*sheet);
  }

  sheet->ClearAssociatedDocumentOrShadowRoot();
}

void Document::InsertSheetAt(size_t aIndex, StyleSheet& aSheet) {
  DocumentOrShadowRoot::InsertSheetAt(aIndex, aSheet);

  if (aSheet.IsApplicable()) {
    AddStyleSheetToStyleSets(aSheet);
  }
}

void Document::StyleSheetApplicableStateChanged(StyleSheet& aSheet) {
  const bool applicable = aSheet.IsApplicable();
  // If we're actually in the document style sheet list
  if (StyleOrderIndexOfSheet(aSheet) >= 0) {
    if (applicable) {
      AddStyleSheetToStyleSets(aSheet);
    } else {
      RemoveStyleSheetFromStyleSets(aSheet);
    }
  }

  if (StyleSheetChangeEventsEnabled()) {
    StyleSheetApplicableStateChangeEventInit init;
    init.mBubbles = true;
    init.mCancelable = true;
    init.mStylesheet = &aSheet;
    init.mApplicable = applicable;

    RefPtr<StyleSheetApplicableStateChangeEvent> event =
        StyleSheetApplicableStateChangeEvent::Constructor(
            this, NS_LITERAL_STRING("StyleSheetApplicableStateChanged"), init);
    event->SetTrusted(true);
    event->SetTarget(this);
    RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(this, event);
    asyncDispatcher->mOnlyChromeDispatch = ChromeOnlyDispatch::eYes;
    asyncDispatcher->PostDOMEvent();
  }

  if (!mSSApplicableStateNotificationPending) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIRunnable> notification = NewRunnableMethod(
        "Document::NotifyStyleSheetApplicableStateChanged", this,
        &Document::NotifyStyleSheetApplicableStateChanged);
    mSSApplicableStateNotificationPending =
        NS_SUCCEEDED(Dispatch(TaskCategory::Other, notification.forget()));
  }
}

void Document::NotifyStyleSheetApplicableStateChanged() {
  mSSApplicableStateNotificationPending = false;
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(
        ToSupports(this), "style-sheet-applicable-state-changed", nullptr);
  }
}

static int32_t FindSheet(const nsTArray<RefPtr<StyleSheet>>& aSheets,
                         nsIURI* aSheetURI) {
  for (int32_t i = aSheets.Length() - 1; i >= 0; i--) {
    bool bEqual;
    nsIURI* uri = aSheets[i]->GetSheetURI();

    if (uri && NS_SUCCEEDED(uri->Equals(aSheetURI, &bEqual)) && bEqual)
      return i;
  }

  return -1;
}

nsresult Document::LoadAdditionalStyleSheet(additionalSheetType aType,
                                            nsIURI* aSheetURI) {
  MOZ_ASSERT(aSheetURI, "null arg");

  // Checking if we have loaded this one already.
  if (FindSheet(mAdditionalSheets[aType], aSheetURI) >= 0)
    return NS_ERROR_INVALID_ARG;

  // Loading the sheet sync.
  RefPtr<css::Loader> loader = new css::Loader(GetDocGroup());

  css::SheetParsingMode parsingMode;
  switch (aType) {
    case Document::eAgentSheet:
      parsingMode = css::eAgentSheetFeatures;
      break;

    case Document::eUserSheet:
      parsingMode = css::eUserSheetFeatures;
      break;

    case Document::eAuthorSheet:
      parsingMode = css::eAuthorSheetFeatures;
      break;

    default:
      MOZ_CRASH("impossible value for aType");
  }

  auto result = loader->LoadSheetSync(aSheetURI, parsingMode,
                                      css::Loader::UseSystemPrincipal::Yes);
  if (result.isErr()) {
    return result.unwrapErr();
  }

  RefPtr<StyleSheet> sheet = result.unwrap();

  sheet->SetAssociatedDocumentOrShadowRoot(
      this, StyleSheet::OwnedByDocumentOrShadowRoot);
  MOZ_ASSERT(sheet->IsApplicable());

  return AddAdditionalStyleSheet(aType, sheet);
}

nsresult Document::AddAdditionalStyleSheet(additionalSheetType aType,
                                           StyleSheet* aSheet) {
  if (mAdditionalSheets[aType].Contains(aSheet)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aSheet->IsApplicable()) {
    return NS_ERROR_INVALID_ARG;
  }

  mAdditionalSheets[aType].AppendElement(aSheet);

  if (mStyleSetFilled) {
    mStyleSet->AppendStyleSheet(*aSheet);
    ApplicableStylesChanged();
  }
  return NS_OK;
}

void Document::RemoveAdditionalStyleSheet(additionalSheetType aType,
                                          nsIURI* aSheetURI) {
  MOZ_ASSERT(aSheetURI);

  nsTArray<RefPtr<StyleSheet>>& sheets = mAdditionalSheets[aType];

  int32_t i = FindSheet(mAdditionalSheets[aType], aSheetURI);
  if (i >= 0) {
    RefPtr<StyleSheet> sheetRef = std::move(sheets[i]);
    sheets.RemoveElementAt(i);

    if (!mIsGoingAway) {
      MOZ_ASSERT(sheetRef->IsApplicable());
      if (mStyleSetFilled) {
        mStyleSet->RemoveStyleSheet(*sheetRef);
        ApplicableStylesChanged();
      }
    }
    sheetRef->ClearAssociatedDocumentOrShadowRoot();
  }
}

nsIGlobalObject* Document::GetScopeObject() const {
  nsCOMPtr<nsIGlobalObject> scope(do_QueryReferent(mScopeObject));
  return scope;
}

DocGroup* Document::GetDocGroupOrCreate() {
  if (!mDocGroup) {
    nsAutoCString docGroupKey;
    nsresult rv = mozilla::dom::DocGroup::GetKey(NodePrincipal(), docGroupKey);
    if (NS_SUCCEEDED(rv)) {
      if (mDocumentContainer) {
        nsPIDOMWindowOuter* window = mDocumentContainer->GetWindow();
        if (window) {
          TabGroup* tabgroup = window->MaybeTabGroup();
          if (tabgroup) {
            mDocGroup = tabgroup->AddDocument(docGroupKey, this);
          }
        }
      }
    }
  }
  return mDocGroup;
}

void Document::SetScopeObject(nsIGlobalObject* aGlobal) {
  mScopeObject = do_GetWeakReference(aGlobal);
  if (aGlobal) {
    mHasHadScriptHandlingObject = true;

    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (window) {
      // We want to get the tabgroup unconditionally, such that we can make
      // certain that it is cached in the inner window early enough.
      mozilla::dom::TabGroup* tabgroup = window->TabGroup();
      // We should already have the principal, and now that we have been added
      // to a window, we should be able to join a DocGroup!
      nsAutoCString docGroupKey;
      nsresult rv =
          mozilla::dom::DocGroup::GetKey(NodePrincipal(), docGroupKey);
      if (mDocGroup) {
        if (NS_SUCCEEDED(rv)) {
          MOZ_RELEASE_ASSERT(mDocGroup->MatchesKey(docGroupKey));
        }
        MOZ_RELEASE_ASSERT(mDocGroup->GetTabGroup() == tabgroup);
      } else {
        mDocGroup = tabgroup->AddDocument(docGroupKey, this);
        MOZ_ASSERT(mDocGroup);
      }

      MOZ_ASSERT_IF(
          mNodeInfoManager->GetArenaAllocator(),
          mNodeInfoManager->GetArenaAllocator() == mDocGroup->ArenaAllocator());
    }
  }
}

bool Document::ContainsEMEContent() {
  bool containsEME = false;

  auto check = [&containsEME](nsISupports* aSupports) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
    if (auto* mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
      if (mediaElem->GetMediaKeys()) {
        containsEME = true;
      }
    }
  };

  EnumerateActivityObservers(check);
  return containsEME;
}

bool Document::ContainsMSEContent() {
  bool containsMSE = false;

  auto check = [&containsMSE](nsISupports* aSupports) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
    if (auto* mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
      RefPtr<MediaSource> ms = mediaElem->GetMozMediaSourceObject();
      if (ms) {
        containsMSE = true;
      }
    }
  };

  EnumerateActivityObservers(check);
  return containsMSE;
}

static void NotifyActivityChanged(nsISupports* aSupports) {
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (auto mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
    mediaElem->NotifyOwnerDocumentActivityChanged();
  }
  nsCOMPtr<nsIObjectLoadingContent> objectLoadingContent(
      do_QueryInterface(aSupports));
  if (objectLoadingContent) {
    nsObjectLoadingContent* olc =
        static_cast<nsObjectLoadingContent*>(objectLoadingContent.get());
    olc->NotifyOwnerDocumentActivityChanged();
  }
  nsCOMPtr<nsIDocumentActivity> objectDocumentActivity(
      do_QueryInterface(aSupports));
  if (objectDocumentActivity) {
    objectDocumentActivity->NotifyOwnerDocumentActivityChanged();
  } else {
    nsCOMPtr<nsIImageLoadingContent> imageLoadingContent(
        do_QueryInterface(aSupports));
    if (imageLoadingContent) {
      auto ilc = static_cast<nsImageLoadingContent*>(imageLoadingContent.get());
      ilc->NotifyOwnerDocumentActivityChanged();
    }
  }
}

bool Document::IsTopLevelWindowInactive() const {
  nsCOMPtr<nsIDocShellTreeItem> treeItem = GetDocShell();
  if (!treeItem) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetInProcessRootTreeItem(getter_AddRefs(rootItem));
  if (!rootItem) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> domWindow = rootItem->GetWindow();
  return domWindow && !domWindow->IsActive();
}

void Document::SetContainer(nsDocShell* aContainer) {
  if (aContainer) {
    mDocumentContainer = aContainer;
  } else {
    mDocumentContainer = WeakPtr<nsDocShell>();
  }

  mInChromeDocShell =
      aContainer && aContainer->GetBrowsingContext()->IsChrome();

  EnumerateActivityObservers(NotifyActivityChanged);

  // IsTopLevelWindowInactive depends on the docshell, so
  // update the cached value now that it's available.
  UpdateDocumentStates(NS_DOCUMENT_STATE_WINDOW_INACTIVE, false);
  if (!aContainer) {
    return;
  }

  BrowsingContext* context = aContainer->GetBrowsingContext();
  if (context && context->IsContent()) {
    if (context->IsTopContent()) {
      SetIsTopLevelContentDocument(true);
    }
    SetIsContentDocument(true);
  }

  mAncestorPrincipals = aContainer->AncestorPrincipals();
  mAncestorOuterWindowIDs = aContainer->AncestorOuterWindowIDs();
}

nsISupports* Document::GetContainer() const {
  return static_cast<nsIDocShell*>(mDocumentContainer);
}

void Document::SetScriptGlobalObject(
    nsIScriptGlobalObject* aScriptGlobalObject) {
  MOZ_ASSERT(aScriptGlobalObject || !mAnimationController ||
                 mAnimationController->IsPausedByType(
                     SMILTimeContainer::PAUSE_PAGEHIDE |
                     SMILTimeContainer::PAUSE_BEGIN),
             "Clearing window pointer while animations are unpaused");

  if (mScriptGlobalObject && !aScriptGlobalObject) {
    // We're detaching from the window.  We need to grab a pointer to
    // our layout history state now.
    mLayoutHistoryState = GetLayoutHistoryState();

    // Also make sure to remove our onload blocker now if we haven't done it yet
    if (mOnloadBlockCount != 0) {
      nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
      if (loadGroup) {
        loadGroup->RemoveRequest(mOnloadBlocker, nullptr, NS_OK);
      }
    }

    ErrorResult error;
    if (GetController().isSome()) {
      imgLoader* loader = nsContentUtils::GetImgLoaderForDocument(this);
      if (loader) {
        loader->ClearCacheForControlledDocument(this);
      }

      // We may become controlled again if this document comes back out
      // of bfcache.  Clear our state to allow that to happen.  Only
      // clear this flag if we are actually controlled, though, so pages
      // that were force reloaded don't become controlled when they
      // come out of bfcache.
      mMaybeServiceWorkerControlled = false;
    }
  }

  // BlockOnload() might be called before mScriptGlobalObject is set.
  // We may need to add the blocker once mScriptGlobalObject is set.
  bool needOnloadBlocker = !mScriptGlobalObject && aScriptGlobalObject;

  mScriptGlobalObject = aScriptGlobalObject;

  if (needOnloadBlocker) {
    EnsureOnloadBlocker();
  }

  UpdateFrameRequestCallbackSchedulingState();

  if (aScriptGlobalObject) {
    // Go back to using the docshell for the layout history state
    mLayoutHistoryState = nullptr;
    SetScopeObject(aScriptGlobalObject);
    mHasHadDefaultView = true;

    if (mAllowDNSPrefetch) {
      nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
      if (docShell) {
#ifdef DEBUG
        nsCOMPtr<nsIWebNavigation> webNav =
            do_GetInterface(aScriptGlobalObject);
        NS_ASSERTION(SameCOMIdentity(webNav, docShell),
                     "Unexpected container or script global?");
#endif
        bool allowDNSPrefetch;
        docShell->GetAllowDNSPrefetch(&allowDNSPrefetch);
        mAllowDNSPrefetch = allowDNSPrefetch;
      }
    }

    // If we are set in a window that is already focused we should remember this
    // as the time the document gained focus.
    IgnoredErrorResult ignored;
    bool focused = HasFocus(ignored);
    if (focused) {
      SetLastFocusTime(TimeStamp::Now());
    }
  }

  // Remember the pointer to our window (or lack there of), to avoid
  // having to QI every time it's asked for.
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mScriptGlobalObject);
  mWindow = window;

  // Now that we know what our window is, we can flush the CSP errors to the
  // Web Console. We are flushing all messages that occured and were stored
  // in the queue prior to this point.
  if (mCSP) {
    static_cast<nsCSPContext*>(mCSP.get())->flushConsoleMessages();
  }

  nsCOMPtr<nsIHttpChannelInternal> internalChannel =
      do_QueryInterface(GetChannel());
  if (internalChannel) {
    nsCOMArray<nsISecurityConsoleMessage> messages;
    DebugOnly<nsresult> rv = internalChannel->TakeAllSecurityMessages(messages);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    SendToConsole(messages);
  }

  // Set our visibility state, but do not fire the event.  This is correct
  // because either we're coming out of bfcache (in which case IsVisible() will
  // still test false at this point and no state change will happen) or we're
  // doing the initial document load and don't want to fire the event for this
  // change.
  dom::VisibilityState oldState = mVisibilityState;
  mVisibilityState = ComputeVisibilityState();
  // When the visibility is changed, notify it to observers.
  // Some observers need the notification, for example HTMLMediaElement uses
  // it to update internal media resource allocation.
  // When video is loaded via VideoDocument, HTMLMediaElement and MediaDecoder
  // creation are already done before Document::SetScriptGlobalObject() call.
  // MediaDecoder decides whether starting decoding is decided based on
  // document's visibility. When the MediaDecoder is created,
  // Document::SetScriptGlobalObject() is not yet called and document is
  // hidden state. Therefore the MediaDecoder decides that decoding is
  // not yet necessary. But soon after Document::SetScriptGlobalObject()
  // call, the document becomes not hidden. At the time, MediaDecoder needs
  // to know it and needs to start updating decoding.
  if (oldState != mVisibilityState) {
    EnumerateActivityObservers(NotifyActivityChanged);
  }

  // The global in the template contents owner document should be the same.
  if (mTemplateContentsOwner && mTemplateContentsOwner != this) {
    mTemplateContentsOwner->SetScriptGlobalObject(aScriptGlobalObject);
  }

  if (!mMaybeServiceWorkerControlled && mDocumentContainer &&
      mScriptGlobalObject && GetChannel()) {
    // If we are shift-reloaded, don't associate with a ServiceWorker.
    if (mDocumentContainer->IsForceReloading()) {
      NS_WARNING("Page was shift reloaded, skipping ServiceWorker control");
      return;
    }

    mMaybeServiceWorkerControlled = true;
  }
}

nsIScriptGlobalObject* Document::GetScriptHandlingObjectInternal() const {
  MOZ_ASSERT(!mScriptGlobalObject,
             "Do not call this when mScriptGlobalObject is set!");
  if (mHasHadDefaultView) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> scriptHandlingObject =
      do_QueryReferent(mScopeObject);
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(scriptHandlingObject);
  if (win) {
    nsPIDOMWindowOuter* outer = win->GetOuterWindow();
    if (!outer || outer->GetCurrentInnerWindow() != win) {
      NS_WARNING("Wrong inner/outer window combination!");
      return nullptr;
    }
  }
  return scriptHandlingObject;
}
void Document::SetScriptHandlingObject(nsIScriptGlobalObject* aScriptObject) {
  NS_ASSERTION(!mScriptGlobalObject || mScriptGlobalObject == aScriptObject,
               "Wrong script object!");
  if (aScriptObject) {
    SetScopeObject(aScriptObject);
    mHasHadDefaultView = false;
  }
}

nsPIDOMWindowOuter* Document::GetWindowInternal() const {
  MOZ_ASSERT(!mWindow, "This should not be called when mWindow is not null!");
  // Let's use mScriptGlobalObject. Even if the document is already removed from
  // the docshell, the outer window might be still obtainable from the it.
  nsCOMPtr<nsPIDOMWindowOuter> win;
  if (mRemovedFromDocShell) {
    // The docshell returns the outer window we are done.
    nsCOMPtr<nsIDocShell> kungFuDeathGrip(mDocumentContainer);
    if (kungFuDeathGrip) {
      win = kungFuDeathGrip->GetWindow();
    }
  } else {
    if (nsCOMPtr<nsPIDOMWindowInner> inner =
            do_QueryInterface(mScriptGlobalObject)) {
      // mScriptGlobalObject is always the inner window, let's get the outer.
      win = inner->GetOuterWindow();
    }
  }

  return win;
}

bool Document::InternalAllowXULXBL() {
  if (nsContentUtils::AllowXULXBLForPrincipal(NodePrincipal())) {
    mAllowXULXBL = eTriTrue;
    return true;
  }

  mAllowXULXBL = eTriFalse;
  return false;
}

// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void Document::AddObserver(nsIDocumentObserver* aObserver) {
  NS_ASSERTION(mObservers.IndexOf(aObserver) == nsTArray<int>::NoIndex,
               "Observer already in the list");
  mObservers.AppendElement(aObserver);
  AddMutationObserver(aObserver);
}

bool Document::RemoveObserver(nsIDocumentObserver* aObserver) {
  // If we're in the process of destroying the document (and we're
  // informing the observers of the destruction), don't remove the
  // observers from the list. This is not a big deal, since we
  // don't hold a live reference to the observers.
  if (!mInDestructor) {
    RemoveMutationObserver(aObserver);
    return mObservers.RemoveElement(aObserver);
  }

  return mObservers.Contains(aObserver);
}

void Document::BeginUpdate() {
  ++mUpdateNestLevel;
  nsContentUtils::AddScriptBlocker();
  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginUpdate, (this));
}

void Document::EndUpdate() {
  const bool reset = !mPendingMaybeEditingStateChanged;
  mPendingMaybeEditingStateChanged = true;

  NS_DOCUMENT_NOTIFY_OBSERVERS(EndUpdate, (this));

  --mUpdateNestLevel;

  nsContentUtils::RemoveScriptBlocker();

  if (mXULBroadcastManager) {
    mXULBroadcastManager->MaybeBroadcast();
  }

  if (reset) {
    mPendingMaybeEditingStateChanged = false;
  }
  MaybeEditingStateChanged();
}

void Document::BeginLoad() {
  if (IsEditingOn()) {
    // Reset() blows away all event listeners in the document, and our
    // editor relies heavily on those. Midas is turned on, to make it
    // work, re-initialize it to give it a chance to add its event
    // listeners again.

    TurnEditingOff();
    EditingStateChanged();
  }

  MOZ_ASSERT(!mDidCallBeginLoad);
  mDidCallBeginLoad = true;

  // Block onload here to prevent having to deal with blocking and
  // unblocking it while we know the document is loading.
  BlockOnload();
  mDidFireDOMContentLoaded = false;
  BlockDOMContentLoaded();

  if (mScriptLoader) {
    mScriptLoader->BeginDeferringScripts();
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginLoad, (this));
}

void Document::MozSetImageElement(const nsAString& aImageElementId,
                                  Element* aElement) {
  if (aImageElementId.IsEmpty()) return;

  // Hold a script blocker while calling SetImageElement since that can call
  // out to id-observers
  nsAutoScriptBlocker scriptBlocker;

  IdentifierMapEntry* entry = mIdentifierMap.PutEntry(aImageElementId);
  if (entry) {
    entry->SetImageElement(aElement);
    if (entry->IsEmpty()) {
      mIdentifierMap.RemoveEntry(entry);
    }
  }
}

void Document::DispatchContentLoadedEvents() {
  // If you add early returns from this method, make sure you're
  // calling UnblockOnload properly.

  // Unpin references to preloaded images
  mPreloadingImages.Clear();

  // DOM manipulation after content loaded should not care if the element
  // came from the preloader.
  mPreloadedPreconnects.Clear();

  if (mTiming) {
    mTiming->NotifyDOMContentLoadedStart(Document::GetDocumentURI());
  }

  // Dispatch observer notification to notify observers document is interactive.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    nsIPrincipal* principal = NodePrincipal();
    os->NotifyObservers(ToSupports(this),
                        principal->IsSystemPrincipal()
                            ? "chrome-document-interactive"
                            : "content-document-interactive",
                        nullptr);
  }

  // Fire a DOM event notifying listeners that this document has been
  // loaded (excluding images and other loads initiated by this
  // document).
  nsContentUtils::DispatchTrustedEvent(this, ToSupports(this),
                                       NS_LITERAL_STRING("DOMContentLoaded"),
                                       CanBubble::eYes, Cancelable::eNo);

  if (auto* const window = GetInnerWindow()) {
    const RefPtr<ServiceWorkerContainer> serviceWorker =
        window->Navigator()->ServiceWorker();

    // This could cause queued messages from a service worker to get
    // dispatched on serviceWorker.
    serviceWorker->StartMessages();
  }

  if (MayStartLayout()) {
    MaybeResolveReadyForIdle();
  }

  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  nsIDocShell* docShell = this->GetDocShell();

  if (timelines && timelines->HasConsumer(docShell)) {
    timelines->AddMarkerForDocShell(
        docShell,
        MakeUnique<DocLoadingTimelineMarker>("document::DOMContentLoaded"));
  }

  if (mTiming) {
    mTiming->NotifyDOMContentLoadedEnd(Document::GetDocumentURI());
  }

  // If this document is a [i]frame, fire a DOMFrameContentLoaded
  // event on all parent documents notifying that the HTML (excluding
  // other external files such as images and stylesheets) in a frame
  // has finished loading.

  // target_frame is the [i]frame element that will be used as the
  // target for the event. It's the [i]frame whose content is done
  // loading.
  nsCOMPtr<EventTarget> target_frame;

  if (mParentDocument) {
    target_frame = mParentDocument->FindContentForSubDocument(this);
  }

  if (target_frame) {
    nsCOMPtr<Document> parent = mParentDocument;
    do {
      RefPtr<Event> event;
      if (parent) {
        IgnoredErrorResult ignored;
        event = parent->CreateEvent(NS_LITERAL_STRING("Events"),
                                    CallerType::System, ignored);
      }

      if (event) {
        event->InitEvent(NS_LITERAL_STRING("DOMFrameContentLoaded"), true,
                         true);

        event->SetTarget(target_frame);
        event->SetTrusted(true);

        // To dispatch this event we must manually call
        // EventDispatcher::Dispatch() on the ancestor document since the
        // target is not in the same document, so the event would never reach
        // the ancestor document if we used the normal event
        // dispatching code.

        WidgetEvent* innerEvent = event->WidgetEventPtr();
        if (innerEvent) {
          nsEventStatus status = nsEventStatus_eIgnore;

          if (RefPtr<nsPresContext> context = parent->GetPresContext()) {
            EventDispatcher::Dispatch(ToSupports(parent), context, innerEvent,
                                      event, &status);
          }
        }
      }

      parent = parent->GetInProcessParentDocument();
    } while (parent);
  }

  // If the document has a manifest attribute, fire a MozApplicationManifest
  // event.
  Element* root = GetRootElement();
  if (root && root->HasAttr(kNameSpaceID_None, nsGkAtoms::manifest)) {
    nsContentUtils::DispatchChromeEvent(
        this, ToSupports(this), NS_LITERAL_STRING("MozApplicationManifest"),
        CanBubble::eYes, Cancelable::eYes);
  }

  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    inner->NoteDOMContentLoaded();
  }

  // TODO
  if (mMaybeServiceWorkerControlled) {
    using mozilla::dom::ServiceWorkerManager;
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      Maybe<ClientInfo> clientInfo = GetClientInfo();
      if (clientInfo.isSome()) {
        swm->MaybeCheckNavigationUpdate(clientInfo.ref());
      }
    }
  }

  UnblockOnload(true);
}

void Document::EndLoad() {
  bool turnOnEditing =
      mParser && (HasFlag(NODE_IS_EDITABLE) || mContentEditableCount > 0);

#if defined(DEBUG)
  // only assert if nothing stopped the load on purpose
  if (!mParserAborted) {
    nsContentSecurityUtils::AssertAboutPageHasCSP(this);
  }
#endif

  // EndLoad may have been called without a matching call to BeginLoad, in the
  // case of a failed parse (for example, due to timeout). In such a case, we
  // still want to execute part of this code to do appropriate cleanup, but we
  // gate part of it because it is intended to match 1-for-1 with calls to
  // BeginLoad. We have an explicit flag bit for this purpose, since it's
  // complicated and error prone to derive this condition from other related
  // flags that can be manipulated outside of a BeginLoad/EndLoad pair.

  // Part 1: Code that always executes to cleanup end of parsing, whether
  // that parsing was successful or not.

  // Drop the ref to our parser, if any, but keep hold of the sink so that we
  // can flush it from FlushPendingNotifications as needed.  We might have to
  // do that to get a StartLayout() to happen.
  if (mParser) {
    mWeakSink = do_GetWeakReference(mParser->GetContentSink());
    mParser = nullptr;
  }

  // Update the attributes on the PerformanceNavigationTiming before notifying
  // the onload observers.
  if (nsPIDOMWindowInner* window = GetInnerWindow()) {
    window->QueuePerformanceNavigationTiming();
  }

  NS_DOCUMENT_NOTIFY_OBSERVERS(EndLoad, (this));

  // Part 2: Code that only executes when this EndLoad matches a BeginLoad.

  if (!mDidCallBeginLoad) {
    return;
  }
  mDidCallBeginLoad = false;

  UnblockDOMContentLoaded();

  if (turnOnEditing) {
    EditingStateChanged();
  }

  if (!GetWindow()) {
    // This is a document that's not in a window.  For example, this could be an
    // XMLHttpRequest responseXML document, or a document created via DOMParser
    // or DOMImplementation.  We don't reach this code normally for such
    // documents (which is not obviously correct), but can reach it via
    // document.open()/document.close().
    //
    // Such documents don't fire load events, but per spec should set their
    // readyState to "complete" when parsing and all loading of subresources is
    // done.  Parsing is done now, and documents not in a window don't load
    // subresources, so just go ahead and mark ourselves as complete.
    SetReadyStateInternal(Document::READYSTATE_COMPLETE,
                          /* updateTimingInformation = */ false);

    // Reset mSkipLoadEventAfterClose just in case.
    mSkipLoadEventAfterClose = false;
  }
}

void Document::UnblockDOMContentLoaded() {
  MOZ_ASSERT(mBlockDOMContentLoaded);
  if (--mBlockDOMContentLoaded != 0 || mDidFireDOMContentLoaded) {
    return;
  }

  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug,
          ("DOCUMENT %p UnblockDOMContentLoaded", this));

  mDidFireDOMContentLoaded = true;
  if (PresShell* presShell = GetPresShell()) {
    presShell->GetRefreshDriver()->NotifyDOMContentLoaded();
  }

  MOZ_ASSERT(mReadyState == READYSTATE_INTERACTIVE);
  if (!mSynchronousDOMContentLoaded) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIRunnable> ev =
        NewRunnableMethod("Document::DispatchContentLoadedEvents", this,
                          &Document::DispatchContentLoadedEvents);
    Dispatch(TaskCategory::Other, ev.forget());
  } else {
    DispatchContentLoadedEvents();
  }
}

void Document::ContentStateChanged(nsIContent* aContent,
                                   EventStates aStateMask) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(),
             "Someone forgot a scriptblocker");
  NS_DOCUMENT_NOTIFY_OBSERVERS(ContentStateChanged,
                               (this, aContent, aStateMask));
}

void Document::RuleChanged(StyleSheet& aSheet, css::Rule* aRule) {
  if (aSheet.IsApplicable()) {
    ApplicableStylesChanged();
  }
}

void Document::RuleAdded(StyleSheet& aSheet, css::Rule& aRule) {
  if (aRule.IsIncompleteImportRule()) {
    return;
  }

  if (aSheet.IsApplicable()) {
    ApplicableStylesChanged();
  }
}

void Document::ImportRuleLoaded(dom::CSSImportRule& aRule, StyleSheet& aSheet) {
  if (aSheet.IsApplicable()) {
    ApplicableStylesChanged();
  }
}

void Document::RuleRemoved(StyleSheet& aSheet, css::Rule& aRule) {
  if (aSheet.IsApplicable()) {
    ApplicableStylesChanged();
  }
}

static Element* GetCustomContentContainer(PresShell* aPresShell) {
  if (!aPresShell || !aPresShell->GetCanvasFrame()) {
    return nullptr;
  }

  return aPresShell->GetCanvasFrame()->GetCustomContentContainer();
}

static void InsertAnonContentIntoCanvas(AnonymousContent& aAnonContent,
                                        PresShell* aPresShell) {
  Element* container = GetCustomContentContainer(aPresShell);
  if (!container) {
    return;
  }

  nsresult rv = container->AppendChildTo(&aAnonContent.ContentNode(), true);
  if (NS_FAILED(rv)) {
    return;
  }

  aPresShell->GetCanvasFrame()->ShowCustomContentContainer();
}

already_AddRefed<AnonymousContent> Document::InsertAnonymousContent(
    Element& aElement, ErrorResult& aRv) {
  nsAutoScriptBlocker scriptBlocker;

  // Clone the node to avoid returning a direct reference.
  nsCOMPtr<nsINode> clone = aElement.CloneNode(true, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  auto anonContent =
      MakeRefPtr<AnonymousContent>(clone.forget().downcast<Element>());
  mAnonymousContents.AppendElement(anonContent);

  InsertAnonContentIntoCanvas(*anonContent, GetPresShell());

  return anonContent.forget();
}

static void RemoveAnonContentFromCanvas(AnonymousContent& aAnonContent,
                                        PresShell* aPresShell) {
  RefPtr<Element> container = GetCustomContentContainer(aPresShell);
  if (!container) {
    return;
  }
  container->RemoveChild(aAnonContent.ContentNode(), IgnoreErrors());
}

void Document::RemoveAnonymousContent(AnonymousContent& aContent,
                                      ErrorResult& aRv) {
  nsAutoScriptBlocker scriptBlocker;

  auto index = mAnonymousContents.IndexOf(&aContent);
  if (index == mAnonymousContents.NoIndex) {
    return;
  }

  mAnonymousContents.RemoveElementAt(index);
  RemoveAnonContentFromCanvas(aContent, GetPresShell());

  if (mAnonymousContents.IsEmpty() &&
      GetCustomContentContainer(GetPresShell())) {
    GetPresShell()->GetCanvasFrame()->HideCustomContentContainer();
  }
}

Element* Document::GetAnonRootIfInAnonymousContentContainer(
    nsINode* aNode) const {
  if (!aNode->IsInNativeAnonymousSubtree()) {
    return nullptr;
  }

  PresShell* presShell = GetPresShell();
  if (!presShell || !presShell->GetCanvasFrame()) {
    return nullptr;
  }

  nsAutoScriptBlocker scriptBlocker;
  nsCOMPtr<Element> customContainer =
      presShell->GetCanvasFrame()->GetCustomContentContainer();
  if (!customContainer) {
    return nullptr;
  }

  // An arbitrary number of elements can be inserted as children of the custom
  // container frame.  We want the one that was added that contains aNode, so
  // we need to keep track of the last child separately using |child| here.
  nsINode* child = aNode;
  nsINode* parent = aNode->GetParentNode();
  while (parent && parent->IsInNativeAnonymousSubtree()) {
    if (parent == customContainer) {
      return Element::FromNode(child);
    }
    child = parent;
    parent = child->GetParentNode();
  }
  return nullptr;
}

Maybe<ClientInfo> Document::GetClientInfo() const {
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    return inner->GetClientInfo();
  }
  return Maybe<ClientInfo>();
}

Maybe<ClientState> Document::GetClientState() const {
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    return inner->GetClientState();
  }
  return Maybe<ClientState>();
}

Maybe<ServiceWorkerDescriptor> Document::GetController() const {
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    return inner->GetController();
  }
  return Maybe<ServiceWorkerDescriptor>();
}

//
// Document interface
//
DocumentType* Document::GetDoctype() const {
  for (nsIContent* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->NodeType() == DOCUMENT_TYPE_NODE) {
      return static_cast<DocumentType*>(child);
    }
  }
  return nullptr;
}

DOMImplementation* Document::GetImplementation(ErrorResult& rv) {
  if (!mDOMImplementation) {
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), "about:blank");
    if (!uri) {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
    bool hasHadScriptObject = true;
    nsIScriptGlobalObject* scriptObject =
        GetScriptHandlingObject(hasHadScriptObject);
    if (!scriptObject && hasHadScriptObject) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mDOMImplementation = new DOMImplementation(
        this, scriptObject ? scriptObject : GetScopeObject(), uri, uri);
  }

  return mDOMImplementation;
}

bool IsLowercaseASCII(const nsAString& aValue) {
  int32_t len = aValue.Length();
  for (int32_t i = 0; i < len; ++i) {
    char16_t c = aValue[i];
    if (!(0x0061 <= (c) && ((c) <= 0x007a))) {
      return false;
    }
  }
  return true;
}

// We only support pseudo-elements with two colons in this function.
static PseudoStyleType GetPseudoElementType(const nsString& aString,
                                            ErrorResult& aRv) {
  MOZ_ASSERT(!aString.IsEmpty(),
             "GetPseudoElementType aString should be non-null");
  if (aString.Length() <= 2 || aString[0] != ':' || aString[1] != ':') {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return PseudoStyleType::NotPseudo;
  }
  RefPtr<nsAtom> pseudo = NS_Atomize(Substring(aString, 1));
  return nsCSSPseudoElements::GetPseudoType(pseudo,
                                            CSSEnabledState::InUASheets);
}

already_AddRefed<Element> Document::CreateElement(
    const nsAString& aTagName, const ElementCreationOptionsOrString& aOptions,
    ErrorResult& rv) {
  rv = nsContentUtils::CheckQName(aTagName, false);
  if (rv.Failed()) {
    return nullptr;
  }

  bool needsLowercase = IsHTMLDocument() && !IsLowercaseASCII(aTagName);
  nsAutoString lcTagName;
  if (needsLowercase) {
    nsContentUtils::ASCIIToLower(aTagName, lcTagName);
  }

  const nsString* is = nullptr;
  PseudoStyleType pseudoType = PseudoStyleType::NotPseudo;
  if (aOptions.IsElementCreationOptions()) {
    const ElementCreationOptions& options =
        aOptions.GetAsElementCreationOptions();

    if (options.mIs.WasPassed()) {
      is = &options.mIs.Value();
    }

    // Check 'pseudo' and throw an exception if it's not one allowed
    // with CSS_PSEUDO_ELEMENT_IS_JS_CREATED_NAC.
    if (options.mPseudo.WasPassed()) {
      pseudoType = GetPseudoElementType(options.mPseudo.Value(), rv);
      if (rv.Failed() || pseudoType == PseudoStyleType::NotPseudo ||
          !nsCSSPseudoElements::PseudoElementIsJSCreatedNAC(pseudoType)) {
        rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }
    }
  }

  RefPtr<Element> elem = CreateElem(needsLowercase ? lcTagName : aTagName,
                                    nullptr, mDefaultElementType, is);

  if (pseudoType != PseudoStyleType::NotPseudo) {
    elem->SetPseudoElementType(pseudoType);
  }

  return elem.forget();
}

already_AddRefed<Element> Document::CreateElementNS(
    const nsAString& aNamespaceURI, const nsAString& aQualifiedName,
    const ElementCreationOptionsOrString& aOptions, ErrorResult& rv) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI, aQualifiedName,
                                            mNodeInfoManager, ELEMENT_NODE,
                                            getter_AddRefs(nodeInfo));
  if (rv.Failed()) {
    return nullptr;
  }

  const nsString* is = nullptr;
  if (aOptions.IsElementCreationOptions()) {
    const ElementCreationOptions& options =
        aOptions.GetAsElementCreationOptions();
    if (options.mIs.WasPassed()) {
      is = &options.mIs.Value();
    }
  }

  nsCOMPtr<Element> element;
  rv = NS_NewElement(getter_AddRefs(element), nodeInfo.forget(),
                     NOT_FROM_PARSER, is);
  if (rv.Failed()) {
    return nullptr;
  }

  return element.forget();
}

already_AddRefed<Element> Document::CreateXULElement(
    const nsAString& aTagName, const ElementCreationOptionsOrString& aOptions,
    ErrorResult& aRv) {
  aRv = nsContentUtils::CheckQName(aTagName, false);
  if (aRv.Failed()) {
    return nullptr;
  }

  const nsString* is = nullptr;
  if (aOptions.IsElementCreationOptions()) {
    const ElementCreationOptions& options =
        aOptions.GetAsElementCreationOptions();
    if (options.mIs.WasPassed()) {
      is = &options.mIs.Value();
    }
  }

  RefPtr<Element> elem = CreateElem(aTagName, nullptr, kNameSpaceID_XUL, is);
  if (!elem) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }
  return elem.forget();
}

already_AddRefed<nsTextNode> Document::CreateEmptyTextNode() const {
  RefPtr<nsTextNode> text = new (mNodeInfoManager) nsTextNode(mNodeInfoManager);
  return text.forget();
}

already_AddRefed<nsTextNode> Document::CreateTextNode(
    const nsAString& aData) const {
  RefPtr<nsTextNode> text = new (mNodeInfoManager) nsTextNode(mNodeInfoManager);
  // Don't notify; this node is still being created.
  text->SetText(aData, false);
  return text.forget();
}

already_AddRefed<DocumentFragment> Document::CreateDocumentFragment() const {
  RefPtr<DocumentFragment> frag =
      new (mNodeInfoManager) DocumentFragment(mNodeInfoManager);
  return frag.forget();
}

// Unfortunately, bareword "Comment" is ambiguous with some Mac system headers.
already_AddRefed<dom::Comment> Document::CreateComment(
    const nsAString& aData) const {
  RefPtr<dom::Comment> comment =
      new (mNodeInfoManager) dom::Comment(mNodeInfoManager);

  // Don't notify; this node is still being created.
  comment->SetText(aData, false);
  return comment.forget();
}

already_AddRefed<CDATASection> Document::CreateCDATASection(
    const nsAString& aData, ErrorResult& rv) {
  if (IsHTMLDocument()) {
    rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (FindInReadable(NS_LITERAL_STRING("]]>"), aData)) {
    rv.Throw(NS_ERROR_DOM_INVALID_CHARACTER_ERR);
    return nullptr;
  }

  RefPtr<CDATASection> cdata =
      new (mNodeInfoManager) CDATASection(mNodeInfoManager);

  // Don't notify; this node is still being created.
  cdata->SetText(aData, false);

  return cdata.forget();
}

already_AddRefed<ProcessingInstruction> Document::CreateProcessingInstruction(
    const nsAString& aTarget, const nsAString& aData, ErrorResult& rv) const {
  nsresult res = nsContentUtils::CheckQName(aTarget, false);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return nullptr;
  }

  if (FindInReadable(NS_LITERAL_STRING("?>"), aData)) {
    rv.Throw(NS_ERROR_DOM_INVALID_CHARACTER_ERR);
    return nullptr;
  }

  RefPtr<ProcessingInstruction> pi =
      NS_NewXMLProcessingInstruction(mNodeInfoManager, aTarget, aData);

  return pi.forget();
}

already_AddRefed<Attr> Document::CreateAttribute(const nsAString& aName,
                                                 ErrorResult& rv) {
  if (!mNodeInfoManager) {
    rv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  nsresult res = nsContentUtils::CheckQName(aName, false);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return nullptr;
  }

  nsAutoString name;
  if (IsHTMLDocument()) {
    nsContentUtils::ASCIIToLower(aName, name);
  } else {
    name = aName;
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  res = mNodeInfoManager->GetNodeInfo(name, nullptr, kNameSpaceID_None,
                                      ATTRIBUTE_NODE, getter_AddRefs(nodeInfo));
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return nullptr;
  }

  RefPtr<Attr> attribute =
      new (mNodeInfoManager) Attr(nullptr, nodeInfo.forget(), EmptyString());
  return attribute.forget();
}

already_AddRefed<Attr> Document::CreateAttributeNS(
    const nsAString& aNamespaceURI, const nsAString& aQualifiedName,
    ErrorResult& rv) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI, aQualifiedName,
                                            mNodeInfoManager, ATTRIBUTE_NODE,
                                            getter_AddRefs(nodeInfo));
  if (rv.Failed()) {
    return nullptr;
  }

  RefPtr<Attr> attribute =
      new (mNodeInfoManager) Attr(nullptr, nodeInfo.forget(), EmptyString());
  return attribute.forget();
}

void Document::ResolveScheduledSVGPresAttrs() {
  for (auto iter = mLazySVGPresElements.Iter(); !iter.Done(); iter.Next()) {
    SVGElement* svg = iter.Get()->GetKey();
    svg->UpdateContentDeclarationBlock();
  }
  mLazySVGPresElements.Clear();
}

already_AddRefed<nsSimpleContentList> Document::BlockedNodesByClassifier()
    const {
  RefPtr<nsSimpleContentList> list = new nsSimpleContentList(nullptr);

  nsTArray<nsWeakPtr> blockedNodes;
  blockedNodes = mBlockedNodesByClassifier;

  for (unsigned long i = 0; i < blockedNodes.Length(); i++) {
    nsWeakPtr weakNode = blockedNodes[i];
    nsCOMPtr<nsIContent> node = do_QueryReferent(weakNode);
    // Consider only nodes to which we have managed to get strong references.
    // Coping with nullptrs since it's expected for nodes to disappear when
    // nobody else is referring to them.
    if (node) {
      list->AppendElement(node);
    }
  }

  return list.forget();
}

void Document::GetSelectedStyleSheetSet(nsAString& aSheetSet) {
  aSheetSet.Truncate();

  // Look through our sheets, find the selected set title
  size_t count = SheetCount();
  nsAutoString title;
  for (size_t index = 0; index < count; index++) {
    StyleSheet* sheet = SheetAt(index);
    NS_ASSERTION(sheet, "Null sheet in sheet list!");

    if (sheet->Disabled()) {
      // Disabled sheets don't affect the currently selected set
      continue;
    }

    sheet->GetTitle(title);

    if (aSheetSet.IsEmpty()) {
      aSheetSet = title;
    } else if (!title.IsEmpty() && !aSheetSet.Equals(title)) {
      // Sheets from multiple sets enabled; return null string, per spec.
      SetDOMStringToNull(aSheetSet);
      return;
    }
  }
}

void Document::SetSelectedStyleSheetSet(const nsAString& aSheetSet) {
  if (DOMStringIsNull(aSheetSet)) {
    return;
  }

  // Must update mLastStyleSheetSet before doing anything else with stylesheets
  // or CSSLoaders.
  mLastStyleSheetSet = aSheetSet;
  EnableStyleSheetsForSetInternal(aSheetSet, true);
}

void Document::SetPreferredStyleSheetSet(const nsAString& aSheetSet) {
  mPreferredStyleSheetSet = aSheetSet;
  // Only mess with our stylesheets if we don't have a lastStyleSheetSet, per
  // spec.
  if (DOMStringIsNull(mLastStyleSheetSet)) {
    // Calling EnableStyleSheetsForSetInternal, not SetSelectedStyleSheetSet,
    // per spec.  The idea here is that we're changing our preferred set and
    // that shouldn't change the value of lastStyleSheetSet.  Also, we're
    // using the Internal version so we can update the CSSLoader and not have
    // to worry about null strings.
    EnableStyleSheetsForSetInternal(aSheetSet, true);
  }
}

DOMStringList* Document::StyleSheetSets() {
  if (!mStyleSheetSetList) {
    mStyleSheetSetList = new DOMStyleSheetSetList(this);
  }
  return mStyleSheetSetList;
}

void Document::EnableStyleSheetsForSet(const nsAString& aSheetSet) {
  // Per spec, passing in null is a no-op.
  if (!DOMStringIsNull(aSheetSet)) {
    // Note: must make sure to not change the CSSLoader's preferred sheet --
    // that value should be equal to either our lastStyleSheetSet (if that's
    // non-null) or to our preferredStyleSheetSet.  And this method doesn't
    // change either of those.
    EnableStyleSheetsForSetInternal(aSheetSet, false);
  }
}

void Document::EnableStyleSheetsForSetInternal(const nsAString& aSheetSet,
                                               bool aUpdateCSSLoader) {
  size_t count = SheetCount();
  nsAutoString title;
  for (size_t index = 0; index < count; index++) {
    StyleSheet* sheet = SheetAt(index);
    NS_ASSERTION(sheet, "Null sheet in sheet list!");

    sheet->GetTitle(title);
    if (!title.IsEmpty()) {
      sheet->SetEnabled(title.Equals(aSheetSet));
    }
  }
  if (aUpdateCSSLoader) {
    CSSLoader()->DocumentStyleSheetSetChanged();
  }
  if (mStyleSet->StyleSheetsHaveChanged()) {
    ApplicableStylesChanged();
  }
}

void Document::GetCharacterSet(nsAString& aCharacterSet) const {
  nsAutoCString charset;
  GetDocumentCharacterSet()->Name(charset);
  CopyASCIItoUTF16(charset, aCharacterSet);
}

already_AddRefed<nsINode> Document::ImportNode(nsINode& aNode, bool aDeep,
                                               ErrorResult& rv) const {
  nsINode* imported = &aNode;

  switch (imported->NodeType()) {
    case DOCUMENT_NODE: {
      break;
    }
    case DOCUMENT_FRAGMENT_NODE:
    case ATTRIBUTE_NODE:
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case COMMENT_NODE:
    case DOCUMENT_TYPE_NODE: {
      return imported->Clone(aDeep, mNodeInfoManager, rv);
    }
    default: {
      NS_WARNING("Don't know how to clone this nodetype for importNode.");
    }
  }

  rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

already_AddRefed<nsRange> Document::CreateRange(ErrorResult& rv) {
  return nsRange::Create(this, 0, this, 0, rv);
}

already_AddRefed<NodeIterator> Document::CreateNodeIterator(
    nsINode& aRoot, uint32_t aWhatToShow, NodeFilter* aFilter,
    ErrorResult& rv) const {
  RefPtr<NodeIterator> iterator =
      new NodeIterator(&aRoot, aWhatToShow, aFilter);
  return iterator.forget();
}

already_AddRefed<TreeWalker> Document::CreateTreeWalker(nsINode& aRoot,
                                                        uint32_t aWhatToShow,
                                                        NodeFilter* aFilter,
                                                        ErrorResult& rv) const {
  RefPtr<TreeWalker> walker = new TreeWalker(&aRoot, aWhatToShow, aFilter);
  return walker.forget();
}

already_AddRefed<Location> Document::GetLocation() const {
  nsCOMPtr<nsPIDOMWindowInner> w = do_QueryInterface(mScriptGlobalObject);

  if (!w) {
    return nullptr;
  }

  return do_AddRef(w->Location());
}

already_AddRefed<nsIURI> Document::GetDomainURI() {
  nsIPrincipal* principal = NodePrincipal();

  nsCOMPtr<nsIURI> uri;
  principal->GetDomain(getter_AddRefs(uri));
  if (uri) {
    return uri.forget();
  }

  principal->GetURI(getter_AddRefs(uri));
  return uri.forget();
}

void Document::GetDomain(nsAString& aDomain) {
  nsCOMPtr<nsIURI> uri = GetDomainURI();

  if (!uri) {
    aDomain.Truncate();
    return;
  }

  nsAutoCString hostName;
  nsresult rv = nsContentUtils::GetHostOrIPv6WithBrackets(uri, hostName);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(hostName, aDomain);
  } else {
    // If we can't get the host from the URI (e.g. about:, javascript:,
    // etc), just return an empty string.
    aDomain.Truncate();
  }
}

void Document::SetDomain(const nsAString& aDomain, ErrorResult& rv) {
  if (!GetBrowsingContext()) {
    // If our browsing context is null; disallow setting domain
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (mSandboxFlags & SANDBOXED_DOMAIN) {
    // We're sandboxed; disallow setting domain
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!FeaturePolicyUtils::IsFeatureAllowed(
          this, NS_LITERAL_STRING("document-domain"))) {
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (aDomain.IsEmpty()) {
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri = GetDomainURI();
  if (!uri) {
    rv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Check new domain - must be a superdomain of the current host
  // For example, a page from foo.bar.com may set domain to bar.com,
  // but not to ar.com, baz.com, or fi.foo.bar.com.

  nsCOMPtr<nsIURI> newURI = RegistrableDomainSuffixOfInternal(aDomain, uri);
  if (!newURI) {
    // Error: illegal domain
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  rv = NodePrincipal()->SetDomain(newURI);
}

already_AddRefed<nsIURI> Document::CreateInheritingURIForHost(
    const nsACString& aHostString) {
  if (aHostString.IsEmpty()) {
    return nullptr;
  }

  // Create new URI
  nsCOMPtr<nsIURI> uri = GetDomainURI();
  if (!uri) {
    return nullptr;
  }

  nsresult rv;
  rv = NS_MutateURI(uri)
           .SetUserPass(EmptyCString())
           .SetPort(-1)  // we want to reset the port number if needed.
           .SetHostPort(aHostString)
           .Finalize(uri);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return uri.forget();
}

already_AddRefed<nsIURI> Document::RegistrableDomainSuffixOfInternal(
    const nsAString& aNewDomain, nsIURI* aOrigHost) {
  if (NS_WARN_IF(!aOrigHost)) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> newURI =
      CreateInheritingURIForHost(NS_ConvertUTF16toUTF8(aNewDomain));
  if (!newURI) {
    // Error: failed to parse input domain
    return nullptr;
  }

  // Check new domain - must be a superdomain of the current host
  // For example, a page from foo.bar.com may set domain to bar.com,
  // but not to ar.com, baz.com, or fi.foo.bar.com.
  nsAutoCString current;
  nsAutoCString domain;
  if (NS_FAILED(aOrigHost->GetAsciiHost(current))) {
    current.Truncate();
  }
  if (NS_FAILED(newURI->GetAsciiHost(domain))) {
    domain.Truncate();
  }

  bool ok = current.Equals(domain);
  if (current.Length() > domain.Length() && StringEndsWith(current, domain) &&
      current.CharAt(current.Length() - domain.Length() - 1) == '.') {
    // We're golden if the new domain is the current page's base domain or a
    // subdomain of it.
    nsCOMPtr<nsIEffectiveTLDService> tldService =
        do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
    if (!tldService) {
      return nullptr;
    }

    nsAutoCString currentBaseDomain;
    ok = NS_SUCCEEDED(
        tldService->GetBaseDomain(aOrigHost, 0, currentBaseDomain));
    NS_ASSERTION(StringEndsWith(domain, currentBaseDomain) ==
                     (domain.Length() >= currentBaseDomain.Length()),
                 "uh-oh!  slight optimization wasn't valid somehow!");
    ok = ok && domain.Length() >= currentBaseDomain.Length();
  }

  if (!ok) {
    // Error: illegal domain
    return nullptr;
  }

  return CreateInheritingURIForHost(domain);
}

Element* Document::GetHtmlElement() const {
  Element* rootElement = GetRootElement();
  if (rootElement && rootElement->IsHTMLElement(nsGkAtoms::html))
    return rootElement;
  return nullptr;
}

Element* Document::GetHtmlChildElement(nsAtom* aTag) {
  Element* html = GetHtmlElement();
  if (!html) return nullptr;

  // Look for the element with aTag inside html. This needs to run
  // forwards to find the first such element.
  for (nsIContent* child = html->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(aTag)) return child->AsElement();
  }
  return nullptr;
}

nsGenericHTMLElement* Document::GetBody() {
  Element* html = GetHtmlElement();
  if (!html) {
    return nullptr;
  }

  for (nsIContent* child = html->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::body) ||
        child->IsHTMLElement(nsGkAtoms::frameset)) {
      return static_cast<nsGenericHTMLElement*>(child);
    }
  }

  return nullptr;
}

void Document::SetBody(nsGenericHTMLElement* newBody, ErrorResult& rv) {
  nsCOMPtr<Element> root = GetRootElement();

  // The body element must be either a body tag or a frameset tag. And we must
  // have a root element to be able to add kids to it.
  if (!newBody ||
      !newBody->IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::frameset) ||
      !root) {
    rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
    return;
  }

  // Use DOM methods so that we pass through the appropriate security checks.
  nsCOMPtr<Element> currentBody = GetBody();
  if (currentBody) {
    root->ReplaceChild(*newBody, *currentBody, rv);
  } else {
    root->AppendChild(*newBody, rv);
  }
}

HTMLSharedElement* Document::GetHead() {
  return static_cast<HTMLSharedElement*>(GetHeadElement());
}

Element* Document::GetTitleElement() {
  // mMayHaveTitleElement will have been set to true if any HTML or SVG
  // <title> element has been bound to this document. So if it's false,
  // we know there is nothing to do here. This avoids us having to search
  // the whole DOM if someone calls document.title on a large document
  // without a title.
  if (!mMayHaveTitleElement) return nullptr;

  Element* root = GetRootElement();
  if (root && root->IsSVGElement(nsGkAtoms::svg)) {
    // In SVG, the document's title must be a child
    for (nsIContent* child = root->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsSVGElement(nsGkAtoms::title)) {
        return child->AsElement();
      }
    }
    return nullptr;
  }

  // We check the HTML namespace even for non-HTML documents, except SVG.  This
  // matches the spec and the behavior of all tested browsers.
  // We avoid creating a live nsContentList since we don't need to watch for DOM
  // tree mutations.
  RefPtr<nsContentList> list = new nsContentList(
      this, kNameSpaceID_XHTML, nsGkAtoms::title, nsGkAtoms::title,
      /* aDeep = */ true,
      /* aLiveList = */ false);

  nsIContent* first = list->Item(0, false);

  return first ? first->AsElement() : nullptr;
}

void Document::GetTitle(nsAString& aTitle) {
  aTitle.Truncate();

  Element* rootElement = GetRootElement();
  if (!rootElement) {
    return;
  }

  nsAutoString tmp;

#ifdef MOZ_XUL
  if (rootElement->IsXULElement()) {
    rootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::title, tmp);
  } else
#endif
  {
    Element* title = GetTitleElement();
    if (!title) {
      return;
    }
    nsContentUtils::GetNodeTextContent(title, false, tmp);
  }

  tmp.CompressWhitespace();
  aTitle = tmp;
}

void Document::SetTitle(const nsAString& aTitle, ErrorResult& aRv) {
  Element* rootElement = GetRootElement();
  if (!rootElement) {
    return;
  }

#ifdef MOZ_XUL
  if (rootElement->IsXULElement()) {
    aRv =
        rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::title, aTitle, true);
    return;
  }
#endif

  Maybe<mozAutoDocUpdate> updateBatch;
  nsCOMPtr<Element> title = GetTitleElement();
  if (rootElement->IsSVGElement(nsGkAtoms::svg)) {
    if (!title) {
      // Batch updates so that mutation events don't change "the title
      // element" under us
      updateBatch.emplace(this, true);
      RefPtr<mozilla::dom::NodeInfo> titleInfo = mNodeInfoManager->GetNodeInfo(
          nsGkAtoms::title, nullptr, kNameSpaceID_SVG, ELEMENT_NODE);
      NS_NewSVGElement(getter_AddRefs(title), titleInfo.forget(),
                       NOT_FROM_PARSER);
      if (!title) {
        return;
      }
      rootElement->InsertChildBefore(title, rootElement->GetFirstChild(), true);
    }
  } else if (rootElement->IsHTMLElement()) {
    if (!title) {
      // Batch updates so that mutation events don't change "the title
      // element" under us
      updateBatch.emplace(this, true);
      Element* head = GetHeadElement();
      if (!head) {
        return;
      }

      RefPtr<mozilla::dom::NodeInfo> titleInfo;
      titleInfo = mNodeInfoManager->GetNodeInfo(
          nsGkAtoms::title, nullptr, kNameSpaceID_XHTML, ELEMENT_NODE);
      title = NS_NewHTMLTitleElement(titleInfo.forget());
      if (!title) {
        return;
      }

      head->AppendChildTo(title, true);
    }
  } else {
    return;
  }

  aRv = nsContentUtils::SetNodeTextContent(title, aTitle, false);
}

void Document::NotifyPossibleTitleChange(bool aBoundTitleElement) {
  NS_ASSERTION(!mInUnlinkOrDeletion || !aBoundTitleElement,
               "Setting a title while unlinking or destroying the element?");
  if (mInUnlinkOrDeletion) {
    return;
  }

  if (aBoundTitleElement) {
    mMayHaveTitleElement = true;
  }
  if (mPendingTitleChangeEvent.IsPending()) return;

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  RefPtr<nsRunnableMethod<Document, void, false>> event =
      NewNonOwningRunnableMethod("Document::DoNotifyPossibleTitleChange", this,
                                 &Document::DoNotifyPossibleTitleChange);
  nsresult rv = Dispatch(TaskCategory::Other, do_AddRef(event));
  if (NS_SUCCEEDED(rv)) {
    mPendingTitleChangeEvent = std::move(event);
  }
}

void Document::DoNotifyPossibleTitleChange() {
  mPendingTitleChangeEvent.Forget();
  mHaveFiredTitleChange = true;

  nsAutoString title;
  GetTitle(title);

  RefPtr<PresShell> presShell = GetPresShell();
  if (presShell) {
    nsCOMPtr<nsISupports> container =
        presShell->GetPresContext()->GetContainerWeak();
    if (container) {
      nsCOMPtr<nsIBaseWindow> docShellWin = do_QueryInterface(container);
      if (docShellWin) {
        docShellWin->SetTitle(title);
      }
    }
  }

  if (nsPIDOMWindowInner* inner = GetInnerWindow()) {
    if (WindowGlobalChild* child = inner->GetWindowGlobalChild()) {
      child->SendUpdateDocumentTitle(title);
    }
  }

  // Fire a DOM event for the title change.
  nsContentUtils::DispatchChromeEvent(this, ToSupports(this),
                                      NS_LITERAL_STRING("DOMTitleChanged"),
                                      CanBubble::eYes, Cancelable::eYes);
}

already_AddRefed<MediaQueryList> Document::MatchMedia(
    const nsAString& aMediaQueryList, CallerType aCallerType) {
  RefPtr<MediaQueryList> result =
      new MediaQueryList(this, aMediaQueryList, aCallerType);

  mDOMMediaQueryLists.insertBack(result);

  return result.forget();
}

void Document::SetMayStartLayout(bool aMayStartLayout) {
  mMayStartLayout = aMayStartLayout;
  if (MayStartLayout()) {
    // Before starting layout, check whether we're a toplevel chrome
    // window.  If we are, setup some state so that we don't have to restyle
    // the whole tree after StartLayout.
    if (nsCOMPtr<nsIAppWindow> win = GetAppWindowIfToplevelChrome()) {
      // We're the chrome document!
      win->BeforeStartLayout();
    }
    ReadyState state = GetReadyStateEnum();
    if (state >= READYSTATE_INTERACTIVE) {
      // DOMContentLoaded has fired already.
      MaybeResolveReadyForIdle();
    }
  }

  MaybeEditingStateChanged();
}

nsresult Document::InitializeFrameLoader(nsFrameLoader* aLoader) {
  mInitializableFrameLoaders.RemoveElement(aLoader);
  // Don't even try to initialize.
  if (mInDestructor) {
    NS_WARNING(
        "Trying to initialize a frame loader while"
        "document is being deleted");
    return NS_ERROR_FAILURE;
  }

  mInitializableFrameLoaders.AppendElement(aLoader);
  if (!mFrameLoaderRunner) {
    mFrameLoaderRunner =
        NewRunnableMethod("Document::MaybeInitializeFinalizeFrameLoaders", this,
                          &Document::MaybeInitializeFinalizeFrameLoaders);
    NS_ENSURE_TRUE(mFrameLoaderRunner, NS_ERROR_OUT_OF_MEMORY);
    nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
  }
  return NS_OK;
}

nsresult Document::FinalizeFrameLoader(nsFrameLoader* aLoader,
                                       nsIRunnable* aFinalizer) {
  mInitializableFrameLoaders.RemoveElement(aLoader);
  if (mInDestructor) {
    return NS_ERROR_FAILURE;
  }

  mFrameLoaderFinalizers.AppendElement(aFinalizer);
  if (!mFrameLoaderRunner) {
    mFrameLoaderRunner =
        NewRunnableMethod("Document::MaybeInitializeFinalizeFrameLoaders", this,
                          &Document::MaybeInitializeFinalizeFrameLoaders);
    NS_ENSURE_TRUE(mFrameLoaderRunner, NS_ERROR_OUT_OF_MEMORY);
    nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
  }
  return NS_OK;
}

void Document::MaybeInitializeFinalizeFrameLoaders() {
  if (mDelayFrameLoaderInitialization) {
    // This method will be recalled when !mDelayFrameLoaderInitialization.
    mFrameLoaderRunner = nullptr;
    return;
  }

  // We're not in an update, but it is not safe to run scripts, so
  // postpone frameloader initialization and finalization.
  if (!nsContentUtils::IsSafeToRunScript()) {
    if (!mInDestructor && !mFrameLoaderRunner &&
        (mInitializableFrameLoaders.Length() ||
         mFrameLoaderFinalizers.Length())) {
      mFrameLoaderRunner = NewRunnableMethod(
          "Document::MaybeInitializeFinalizeFrameLoaders", this,
          &Document::MaybeInitializeFinalizeFrameLoaders);
      nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
    }
    return;
  }
  mFrameLoaderRunner = nullptr;

  // Don't use a temporary array for mInitializableFrameLoaders, because
  // loading a frame may cause some other frameloader to be removed from the
  // array. But be careful to keep the loader alive when starting the load!
  while (mInitializableFrameLoaders.Length()) {
    RefPtr<nsFrameLoader> loader = mInitializableFrameLoaders[0];
    mInitializableFrameLoaders.RemoveElementAt(0);
    NS_ASSERTION(loader, "null frameloader in the array?");
    loader->ReallyStartLoading();
  }

  uint32_t length = mFrameLoaderFinalizers.Length();
  if (length > 0) {
    nsTArray<nsCOMPtr<nsIRunnable>> finalizers;
    mFrameLoaderFinalizers.SwapElements(finalizers);
    for (uint32_t i = 0; i < length; ++i) {
      finalizers[i]->Run();
    }
  }
}

void Document::TryCancelFrameLoaderInitialization(nsIDocShell* aShell) {
  uint32_t length = mInitializableFrameLoaders.Length();
  for (uint32_t i = 0; i < length; ++i) {
    if (mInitializableFrameLoaders[i]->GetExistingDocShell() == aShell) {
      mInitializableFrameLoaders.RemoveElementAt(i);
      return;
    }
  }
}

void Document::SetPrototypeDocument(nsXULPrototypeDocument* aPrototype) {
  mPrototypeDocument = aPrototype;
  mSynchronousDOMContentLoaded = true;
}

nsIPermissionDelegateHandler* Document::PermDelegateHandler() {
  return GetPermissionDelegateHandler();
}

Document* Document::RequestExternalResource(
    nsIURI* aURI, nsIReferrerInfo* aReferrerInfo, nsINode* aRequestingNode,
    ExternalResourceLoad** aPendingLoad) {
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aRequestingNode, "Must have a node");
  MOZ_ASSERT(aReferrerInfo, "Must have a referrerInfo");
  if (mDisplayDocument) {
    return mDisplayDocument->RequestExternalResource(
        aURI, aReferrerInfo, aRequestingNode, aPendingLoad);
  }

  return mExternalResourceMap.RequestResource(
      aURI, aReferrerInfo, aRequestingNode, this, aPendingLoad);
}

void Document::EnumerateExternalResources(SubDocEnumFunc aCallback) {
  mExternalResourceMap.EnumerateResources(aCallback);
}

SMILAnimationController* Document::GetAnimationController() {
  // We create the animation controller lazily because most documents won't want
  // one and only SVG documents and the like will call this
  if (mAnimationController) return mAnimationController;
  // Refuse to create an Animation Controller for data documents.
  if (mLoadedAsData) return nullptr;

  mAnimationController = new SMILAnimationController(this);

  // If there's a presContext then check the animation mode and pause if
  // necessary.
  nsPresContext* context = GetPresContext();
  if (mAnimationController && context &&
      context->ImageAnimationMode() == imgIContainer::kDontAnimMode) {
    mAnimationController->Pause(SMILTimeContainer::PAUSE_USERPREF);
  }

  // If we're hidden (or being hidden), notify the newly-created animation
  // controller. (Skip this check for SVG-as-an-image documents, though,
  // because they don't get OnPageShow / OnPageHide calls).
  if (!mIsShowing && !mIsBeingUsedAsImage) {
    mAnimationController->OnPageHide();
  }

  return mAnimationController;
}

PendingAnimationTracker* Document::GetOrCreatePendingAnimationTracker() {
  if (!mPendingAnimationTracker) {
    mPendingAnimationTracker = new PendingAnimationTracker(this);
  }

  return mPendingAnimationTracker;
}

/**
 * Retrieve the "direction" property of the document.
 *
 * @lina 01/09/2001
 */
void Document::GetDir(nsAString& aDirection) const {
  aDirection.Truncate();
  Element* rootElement = GetHtmlElement();
  if (rootElement) {
    static_cast<nsGenericHTMLElement*>(rootElement)->GetDir(aDirection);
  }
}

/**
 * Set the "direction" property of the document.
 *
 * @lina 01/09/2001
 */
void Document::SetDir(const nsAString& aDirection) {
  Element* rootElement = GetHtmlElement();
  if (rootElement) {
    rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, aDirection, true);
  }
}

nsIHTMLCollection* Document::Images() {
  if (!mImages) {
    mImages = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::img,
                                nsGkAtoms::img);
  }
  return mImages;
}

nsIHTMLCollection* Document::Embeds() {
  if (!mEmbeds) {
    mEmbeds = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::embed,
                                nsGkAtoms::embed);
  }
  return mEmbeds;
}

static bool MatchLinks(Element* aElement, int32_t aNamespaceID, nsAtom* aAtom,
                       void* aData) {
  return aElement->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area) &&
         aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::href);
}

nsIHTMLCollection* Document::Links() {
  if (!mLinks) {
    mLinks = new nsContentList(this, MatchLinks, nullptr, nullptr);
  }
  return mLinks;
}

nsIHTMLCollection* Document::Forms() {
  if (!mForms) {
    // Please keep this in sync with nsHTMLDocument::GetFormsAndFormControls.
    mForms = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::form,
                               nsGkAtoms::form);
  }

  return mForms;
}

nsIHTMLCollection* Document::Scripts() {
  if (!mScripts) {
    mScripts = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::script,
                                 nsGkAtoms::script);
  }
  return mScripts;
}

nsIHTMLCollection* Document::Applets() {
  if (!mApplets) {
    mApplets = new nsEmptyContentList(this);
  }
  return mApplets;
}

static bool MatchAnchors(Element* aElement, int32_t aNamespaceID, nsAtom* aAtom,
                         void* aData) {
  return aElement->IsHTMLElement(nsGkAtoms::a) &&
         aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::name);
}

nsIHTMLCollection* Document::Anchors() {
  if (!mAnchors) {
    mAnchors = new nsContentList(this, MatchAnchors, nullptr, nullptr);
  }
  return mAnchors;
}

mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> Document::Open(
    const nsAString& aURL, const nsAString& aName, const nsAString& aFeatures,
    ErrorResult& rv) {
  MOZ_ASSERT(nsContentUtils::CanCallerAccess(this),
             "XOW should have caught this!");

  nsCOMPtr<nsPIDOMWindowInner> window = GetInnerWindow();
  if (!window) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindowOuter> outer =
      nsPIDOMWindowOuter::GetFromCurrentInner(window);
  if (!outer) {
    rv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }
  RefPtr<nsGlobalWindowOuter> win = nsGlobalWindowOuter::Cast(outer);
  RefPtr<BrowsingContext> newBC;
  rv = win->OpenJS(aURL, aName, aFeatures, getter_AddRefs(newBC));
  if (!newBC) {
    return nullptr;
  }
  return WindowProxyHolder(std::move(newBC));
}

Document* Document::Open(const Optional<nsAString>& /* unused */,
                         const Optional<nsAString>& /* unused */,
                         ErrorResult& aError) {
  // Implements
  // <https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#document-open-steps>

  MOZ_ASSERT(nsContentUtils::CanCallerAccess(this),
             "XOW should have caught this!");

  // Step 1 -- throw if we're an XML document.
  if (!IsHTMLDocument() || mDisableDocWrite) {
    aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Step 2 -- throw if dynamic markup insertion should throw.
  if (ShouldThrowOnDynamicMarkupInsertion()) {
    aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Step 3 -- get the entry document, so we can use it for security checks.
  nsCOMPtr<Document> callerDoc = GetEntryDocument();
  if (!callerDoc) {
    // If we're called from C++ or in some other way without an originating
    // document we can't do a document.open w/o changing the principal of the
    // document to something like about:blank (as that's the only sane thing to
    // do when we don't know the origin of this call), and since we can't
    // change the principals of a document for security reasons we'll have to
    // refuse to go ahead with this call.

    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Step 4 -- make sure we're same-origin (not just same origin-domain) with
  // the entry document.
  if (!callerDoc->NodePrincipal()->Equals(NodePrincipal())) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Step 5 -- if we have an active parser with a nonzero script nesting level,
  // just no-op.
  if ((mParser && mParser->HasNonzeroScriptNestingLevel()) || mParserAborted) {
    return this;
  }

  // Step 6 -- check for open() during unload.  Per spec, this is just a check
  // of the ignore-opens-during-unload counter, but our unload event code
  // doesn't affect that counter yet (unlike pagehide and beforeunload, which
  // do), so we check for unload directly.
  if (ShouldIgnoreOpens()) {
    return this;
  }

  nsCOMPtr<nsIDocShell> shell(mDocumentContainer);
  if (shell) {
    bool inUnload;
    shell->GetIsInUnload(&inUnload);
    if (inUnload) {
      return this;
    }
  }

  // document.open() inherits the CSP from the opening document.
  // Please create an actual copy of the CSP (do not share the same
  // reference) otherwise appending a new policy within the opened
  // document will be incorrectly propagated to the opening doc.
  nsCOMPtr<nsIContentSecurityPolicy> csp = callerDoc->GetCsp();
  if (csp) {
    RefPtr<nsCSPContext> cspToInherit = new nsCSPContext();
    cspToInherit->InitFromOther(static_cast<nsCSPContext*>(csp.get()));
    mCSP = cspToInherit;
  }

  // At this point we know this is a valid-enough document.open() call
  // and not a no-op.  Increment our use counter.
  SetUseCounter(eUseCounter_custom_DocumentOpen);

  // Step 7 -- stop existing navigation of our browsing context (and all other
  // loads it's doing) if we're the active document of our browsing context.
  // Note that we do not want to stop anything if there is no existing
  // navigation.
  if (shell && IsCurrentActiveDocument() &&
      shell->GetIsAttemptingToNavigate()) {
    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(shell));
    webnav->Stop(nsIWebNavigation::STOP_NETWORK);

    // The Stop call may have cancelled the onload blocker request or
    // prevented it from getting added, so we need to make sure it gets added
    // to the document again otherwise the document could have a non-zero
    // onload block count without the onload blocker request being in the
    // loadgroup.
    EnsureOnloadBlocker();
  }

  // Step 8 -- clear event listeners out of our DOM tree
  for (nsINode* node : ShadowIncludingTreeIterator(*this)) {
    if (EventListenerManager* elm = node->GetExistingListenerManager()) {
      elm->RemoveAllListeners();
    }
  }

  // Step 9 -- clear event listeners from our window, if we have one.
  //
  // Note that we explicitly want the inner window, and only if we're its
  // document.  We want to do this (per spec) even when we're not the "active
  // document", so we can't go through GetWindow(), because it might forward to
  // the wrong inner.
  if (nsPIDOMWindowInner* win = GetInnerWindow()) {
    if (win->GetExtantDoc() == this) {
      if (EventListenerManager* elm =
              nsGlobalWindowInner::Cast(win)->GetExistingListenerManager()) {
        elm->RemoveAllListeners();
      }
    }
  }

  // If we have a parser that has a zero script nesting level, we need to
  // properly terminate it.  We do that after we've removed all the event
  // listeners (so termination won't trigger event listeners if it does
  // something to the DOM), but before we remove all elements from the document
  // (so if termination does modify the DOM in some way we will just blow it
  // away immediately.  See the similar code in WriteCommon that handles the
  // !IsInsertionPointDefined() case and should stay in sync with this code.
  if (mParser) {
    MOZ_ASSERT(!mParser->HasNonzeroScriptNestingLevel(),
               "Why didn't we take the early return?");
    // Make sure we don't re-enter.
    IgnoreOpensDuringUnload ignoreOpenGuard(this);
    mParser->Terminate();
    MOZ_RELEASE_ASSERT(!mParser, "mParser should have been null'd out");
  }

  // Step 10 -- remove all our DOM kids without firing any mutation events.
  {
    // We want to ignore any recursive calls to Open() that happen while
    // disconnecting the node tree.  The spec doesn't say to do this, but the
    // spec also doesn't envision unload events on subframes firing while we do
    // this, while all browsers fire them in practice.  See
    // <https://github.com/whatwg/html/issues/4611>.
    IgnoreOpensDuringUnload ignoreOpenGuard(this);
    DisconnectNodeTree();
  }

  // Step 11 -- if we're the current document in our docshell, do the
  // equivalent of pushState() with the new URL we should have.
  if (shell && IsCurrentActiveDocument()) {
    nsCOMPtr<nsIURI> newURI = callerDoc->GetDocumentURI();
    if (callerDoc != this) {
      nsCOMPtr<nsIURI> noFragmentURI;
      nsresult rv = NS_GetURIWithoutRef(newURI, getter_AddRefs(noFragmentURI));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aError.Throw(rv);
        return nullptr;
      }
      newURI = std::move(noFragmentURI);
    }

    // UpdateURLAndHistory might do various member-setting, so make sure we're
    // holding strong refs to all the refcounted args on the stack.  We can
    // assume that our caller is holding on to "this" already.
    nsCOMPtr<nsIURI> currentURI = GetDocumentURI();
    bool equalURIs;
    nsresult rv = currentURI->Equals(newURI, &equalURIs);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aError.Throw(rv);
      return nullptr;
    }
    nsCOMPtr<nsIStructuredCloneContainer> stateContainer(mStateObjectContainer);
    rv = shell->UpdateURLAndHistory(this, newURI, stateContainer, EmptyString(),
                                    /* aReplace = */ true, currentURI,
                                    equalURIs);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aError.Throw(rv);
      return nullptr;
    }

    // And use the security info of the caller document as well, since
    // it's the thing providing our data.
    mSecurityInfo = callerDoc->GetSecurityInfo();

    // This is not mentioned in the spec, but I think that's a spec bug.  See
    // <https://github.com/whatwg/html/issues/4299>.  In any case, since our
    // URL may be changing away from about:blank here, we really want to unset
    // this flag no matter what, since only about:blank can be an initial
    // document.
    SetIsInitialDocument(false);

    // And let our docloader know that it will need to track our load event.
    nsDocShell::Cast(shell)->SetDocumentOpenedButNotLoaded();
  }

  // Per spec nothing happens with our URI in other cases, though note
  // <https://github.com/whatwg/html/issues/4286>.

  // Note that we don't need to do anything here with base URIs per spec.
  // That said, this might be assuming that we implement
  // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#fallback-base-url
  // correctly, which we don't right now for the about:blank case.

  // Step 12, but note <https://github.com/whatwg/html/issues/4292>.
  mSkipLoadEventAfterClose = mLoadEventFiring;

  // Preliminary to steps 13-16.  Set our ready state to uninitialized before
  // we do anything else, so we can then proceed to later ready state levels.
  SetReadyStateInternal(READYSTATE_UNINITIALIZED,
                        /* updateTimingInformation = */ false);

  // Step 13 -- set our compat mode to standards.
  SetCompatibilityMode(eCompatibility_FullStandards);

  // Step 14 -- create a new parser associated with document.  This also does
  // step 16 implicitly.
  mParserAborted = false;
  RefPtr<nsHtml5Parser> parser = nsHtml5Module::NewHtml5Parser();
  mParser = parser;
  parser->Initialize(this, GetDocumentURI(), shell, nullptr);
  nsresult rv = parser->StartExecutor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return nullptr;
  }

  // Clear out our form control state, because the state of controls
  // in the pre-open() document should not affect the state of
  // controls that are now going to be written.
  mLayoutHistoryState = nullptr;

  if (shell) {
    // Prepare the docshell and the document viewer for the impending
    // out-of-band document.write()
    shell->PrepareForNewContentModel();

    nsCOMPtr<nsIContentViewer> cv;
    shell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      cv->LoadStart(this);
    }
  }

  // Step 15.
  SetReadyStateInternal(Document::READYSTATE_LOADING,
                        /* updateTimingInformation = */ false);

  // Step 16 happened with step 14 above.

  // Step 17.
  return this;
}

void Document::Close(ErrorResult& rv) {
  if (!IsHTMLDocument()) {
    // No calling document.close() on XHTML!

    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (ShouldThrowOnDynamicMarkupInsertion()) {
    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!mParser || !mParser->IsScriptCreated()) {
    return;
  }

  ++mWriteLevel;
  rv = (static_cast<nsHtml5Parser*>(mParser.get()))
           ->Parse(EmptyString(), nullptr, true);
  --mWriteLevel;
}

void Document::WriteCommon(const Sequence<nsString>& aText,
                           bool aNewlineTerminate, mozilla::ErrorResult& rv) {
  // Fast path the common case
  if (aText.Length() == 1) {
    WriteCommon(aText[0], aNewlineTerminate, rv);
  } else {
    // XXXbz it would be nice if we could pass all the strings to the parser
    // without having to do all this copying and then ask it to start
    // parsing....
    nsString text;
    for (size_t i = 0; i < aText.Length(); ++i) {
      text.Append(aText[i]);
    }
    WriteCommon(text, aNewlineTerminate, rv);
  }
}

void Document::WriteCommon(const nsAString& aText, bool aNewlineTerminate,
                           ErrorResult& aRv) {
  mTooDeepWriteRecursion =
      (mWriteLevel > NS_MAX_DOCUMENT_WRITE_DEPTH || mTooDeepWriteRecursion);
  if (NS_WARN_IF(mTooDeepWriteRecursion)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!IsHTMLDocument() || mDisableDocWrite) {
    // No calling document.write*() on XHTML!

    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (ShouldThrowOnDynamicMarkupInsertion()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mParserAborted) {
    // Hixie says aborting the parser doesn't undefine the insertion point.
    // However, since we null out mParser in that case, we track the
    // theoretically defined insertion point using mParserAborted.
    return;
  }

  // Implement Step 4.1 of:
  // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#document-write-steps
  if (ShouldIgnoreOpens()) {
    return;
  }

  void* key = GenerateParserKey();
  if (mParser && !mParser->IsInsertionPointDefined()) {
    if (mIgnoreDestructiveWritesCounter) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM Events"), this,
          nsContentUtils::eDOM_PROPERTIES, "DocumentWriteIgnored");
      return;
    }
    // The spec doesn't tell us to ignore opens from here, but we need to
    // ensure opens are ignored here.  See similar code in Open() that handles
    // the case of an existing parser which is not currently running script and
    // should stay in sync with this code.
    IgnoreOpensDuringUnload ignoreOpenGuard(this);
    mParser->Terminate();
    MOZ_RELEASE_ASSERT(!mParser, "mParser should have been null'd out");
  }

  if (!mParser) {
    if (mIgnoreDestructiveWritesCounter) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM Events"), this,
          nsContentUtils::eDOM_PROPERTIES, "DocumentWriteIgnored");
      return;
    }

    Open({}, {}, aRv);

    // If Open() fails, or if it didn't create a parser (as it won't
    // if the user chose to not discard the current document through
    // onbeforeunload), don't write anything.
    if (aRv.Failed() || !mParser) {
      return;
    }
  }

  static NS_NAMED_LITERAL_STRING(new_line, "\n");

  ++mWriteLevel;

  // This could be done with less code, but for performance reasons it
  // makes sense to have the code for two separate Parse() calls here
  // since the concatenation of strings costs more than we like. And
  // why pay that price when we don't need to?
  if (aNewlineTerminate) {
    aRv = (static_cast<nsHtml5Parser*>(mParser.get()))
              ->Parse(aText + new_line, key, false);
  } else {
    aRv =
        (static_cast<nsHtml5Parser*>(mParser.get()))->Parse(aText, key, false);
  }

  --mWriteLevel;

  mTooDeepWriteRecursion = (mWriteLevel != 0 && mTooDeepWriteRecursion);
}

void Document::Write(const Sequence<nsString>& aText, ErrorResult& rv) {
  WriteCommon(aText, false, rv);
}

void Document::Writeln(const Sequence<nsString>& aText, ErrorResult& rv) {
  WriteCommon(aText, true, rv);
}

void* Document::GenerateParserKey(void) {
  if (!mScriptLoader) {
    // If we don't have a script loader, then the parser probably isn't parsing
    // anything anyway, so just return null.
    return nullptr;
  }

  // The script loader provides us with the currently executing script element,
  // which is guaranteed to be unique per script.
  nsIScriptElement* script = mScriptLoader->GetCurrentParserInsertedScript();
  if (script && mParser && mParser->IsScriptCreated()) {
    nsCOMPtr<nsIParser> creatorParser = script->GetCreatorParser();
    if (creatorParser != mParser) {
      // Make scripts that aren't inserted by the active parser of this document
      // participate in the context of the script that document.open()ed
      // this document.
      return nullptr;
    }
  }
  return script;
}

/* static */
bool Document::MatchNameAttribute(Element* aElement, int32_t aNamespaceID,
                                  nsAtom* aAtom, void* aData) {
  MOZ_ASSERT(aElement, "Must have element to work with!");

  if (!aElement->HasName()) {
    return false;
  }

  nsString* elementName = static_cast<nsString*>(aData);
  return aElement->GetNameSpaceID() == kNameSpaceID_XHTML &&
         aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, *elementName,
                               eCaseMatters);
}

/* static */
void* Document::UseExistingNameString(nsINode* aRootNode,
                                      const nsString* aName) {
  return const_cast<nsString*>(aName);
}

nsresult Document::GetDocumentURI(nsString& aDocumentURI) const {
  if (mDocumentURI) {
    nsAutoCString uri;
    nsresult rv = mDocumentURI->GetSpec(uri);
    NS_ENSURE_SUCCESS(rv, rv);

    CopyUTF8toUTF16(uri, aDocumentURI);
  } else {
    aDocumentURI.Truncate();
  }

  return NS_OK;
}

// Alias of above
nsresult Document::GetURL(nsString& aURL) const { return GetDocumentURI(aURL); }

void Document::GetDocumentURIFromJS(nsString& aDocumentURI,
                                    CallerType aCallerType,
                                    ErrorResult& aRv) const {
  if (!mChromeXHRDocURI || aCallerType != CallerType::System) {
    aRv = GetDocumentURI(aDocumentURI);
    return;
  }

  nsAutoCString uri;
  nsresult res = mChromeXHRDocURI->GetSpec(uri);
  if (NS_FAILED(res)) {
    aRv.Throw(res);
    return;
  }
  CopyUTF8toUTF16(uri, aDocumentURI);
}

nsIURI* Document::GetDocumentURIObject() const {
  if (!mChromeXHRDocURI) {
    return GetDocumentURI();
  }

  return mChromeXHRDocURI;
}

void Document::GetCompatMode(nsString& aCompatMode) const {
  NS_ASSERTION(mCompatMode == eCompatibility_NavQuirks ||
                   mCompatMode == eCompatibility_AlmostStandards ||
                   mCompatMode == eCompatibility_FullStandards,
               "mCompatMode is neither quirks nor strict for this document");

  if (mCompatMode == eCompatibility_NavQuirks) {
    aCompatMode.AssignLiteral("BackCompat");
  } else {
    aCompatMode.AssignLiteral("CSS1Compat");
  }
}

}  // namespace dom
}  // namespace mozilla

void nsDOMAttributeMap::BlastSubtreeToPieces(nsINode* aNode) {
  if (Element* element = Element::FromNode(aNode)) {
    if (const nsDOMAttributeMap* map = element->GetAttributeMap()) {
      while (true) {
        RefPtr<Attr> attr;
        {
          // Use an iterator to get an arbitrary attribute from the
          // cache. The iterator must be destroyed before any other
          // operations on mAttributeCache, to avoid hash table
          // assertions.
          auto iter = map->mAttributeCache.ConstIter();
          if (iter.Done()) {
            break;
          }
          attr = iter.UserData();
        }

        BlastSubtreeToPieces(attr);

        mozilla::DebugOnly<nsresult> rv =
            element->UnsetAttr(attr->NodeInfo()->NamespaceID(),
                               attr->NodeInfo()->NameAtom(), false);

        // XXX Should we abort here?
        NS_ASSERTION(NS_SUCCEEDED(rv), "Uh-oh, UnsetAttr shouldn't fail!");
      }
    }

    if (mozilla::dom::ShadowRoot* shadow = element->GetShadowRoot()) {
      BlastSubtreeToPieces(shadow);
      element->UnattachShadow();
    }
  }

  while (aNode->HasChildren()) {
    nsIContent* node = aNode->GetFirstChild();
    BlastSubtreeToPieces(node);
    aNode->RemoveChildNode(node, false);
  }
}

namespace mozilla {
namespace dom {

nsINode* Document::AdoptNode(nsINode& aAdoptedNode, ErrorResult& rv) {
  nsINode* adoptedNode = &aAdoptedNode;

  // Scope firing mutation events so that we don't carry any state that
  // might be stale
  {
    nsINode* parent = adoptedNode->GetParentNode();
    if (parent) {
      nsContentUtils::MaybeFireNodeRemoved(adoptedNode, parent);
    }
  }

  nsAutoScriptBlocker scriptBlocker;

  switch (adoptedNode->NodeType()) {
    case ATTRIBUTE_NODE: {
      // Remove from ownerElement.
      RefPtr<Attr> adoptedAttr = static_cast<Attr*>(adoptedNode);

      nsCOMPtr<Element> ownerElement = adoptedAttr->GetOwnerElement(rv);
      if (rv.Failed()) {
        return nullptr;
      }

      if (ownerElement) {
        RefPtr<Attr> newAttr =
            ownerElement->RemoveAttributeNode(*adoptedAttr, rv);
        if (rv.Failed()) {
          return nullptr;
        }

        newAttr.swap(adoptedAttr);
      }

      break;
    }
    case DOCUMENT_FRAGMENT_NODE: {
      if (adoptedNode->IsShadowRoot()) {
        rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return nullptr;
      }
      [[fallthrough]];
    }
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case COMMENT_NODE:
    case DOCUMENT_TYPE_NODE: {
      // Don't allow adopting a node's anonymous subtree out from under it.
      if (adoptedNode->AsContent()->IsRootOfAnonymousSubtree()) {
        rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }

      // We don't want to adopt an element into its own contentDocument or into
      // a descendant contentDocument, so we check if the frameElement of this
      // document or any of its parents is the adopted node or one of its
      // descendants.
      Document* doc = this;
      do {
        if (nsPIDOMWindowOuter* win = doc->GetWindow()) {
          nsCOMPtr<nsINode> node = win->GetFrameElementInternal();
          if (node && node->IsInclusiveDescendantOf(adoptedNode)) {
            rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
            return nullptr;
          }
        }
      } while ((doc = doc->GetInProcessParentDocument()));

      // Remove from parent.
      nsCOMPtr<nsINode> parent = adoptedNode->GetParentNode();
      if (parent) {
        parent->RemoveChildNode(adoptedNode->AsContent(), true);
      } else {
        MOZ_ASSERT(!adoptedNode->IsInUncomposedDoc());
      }

      break;
    }
    case DOCUMENT_NODE: {
      rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
    default: {
      NS_WARNING("Don't know how to adopt this nodetype for adoptNode.");

      rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  }

  nsCOMPtr<Document> oldDocument = adoptedNode->OwnerDoc();
  bool sameDocument = oldDocument == this;

  AutoJSContext cx;
  JS::Rooted<JSObject*> newScope(cx, nullptr);
  if (!sameDocument) {
    newScope = GetWrapper();
    if (!newScope && GetScopeObject() && GetScopeObject()->HasJSGlobal()) {
      // Make sure cx is in a semi-sane compartment before we call WrapNative.
      // It's kind of irrelevant, given that we're passing aAllowWrapping =
      // false, and documents should always insist on being wrapped in an
      // canonical scope. But we try to pass something sane anyway.
      JSObject* globalObject = GetScopeObject()->GetGlobalJSObject();
      JSAutoRealm ar(cx, globalObject);
      JS::Rooted<JS::Value> v(cx);
      rv = nsContentUtils::WrapNative(cx, ToSupports(this), this, &v,
                                      /* aAllowWrapping = */ false);
      if (rv.Failed()) return nullptr;
      newScope = &v.toObject();
    }
  }

  adoptedNode->Adopt(sameDocument ? nullptr : mNodeInfoManager, newScope, rv);
  if (rv.Failed()) {
    // Disconnect all nodes from their parents, since some have the old document
    // as their ownerDocument and some have this as their ownerDocument.
    nsDOMAttributeMap::BlastSubtreeToPieces(adoptedNode);
    return nullptr;
  }

  MOZ_ASSERT(adoptedNode->OwnerDoc() == this,
             "Should still be in the document we just got adopted into");

  return adoptedNode;
}

bool Document::UseWidthDeviceWidthFallbackViewport() const { return false; }

static Maybe<LayoutDeviceToScreenScale> ParseScaleString(
    const nsString& aScaleString) {
  // https://drafts.csswg.org/css-device-adapt/#min-scale-max-scale
  if (aScaleString.EqualsLiteral("device-width") ||
      aScaleString.EqualsLiteral("device-height")) {
    return Some(LayoutDeviceToScreenScale(10.0f));
  } else if (aScaleString.EqualsLiteral("yes")) {
    return Some(LayoutDeviceToScreenScale(1.0f));
  } else if (aScaleString.EqualsLiteral("no")) {
    return Some(LayoutDeviceToScreenScale(kViewportMinScale));
  } else if (aScaleString.IsEmpty()) {
    return Nothing();
  }

  nsresult scaleErrorCode;
  float scale = aScaleString.ToFloatAllowTrailingChars(&scaleErrorCode);
  if (NS_FAILED(scaleErrorCode)) {
    return Some(LayoutDeviceToScreenScale(kViewportMinScale));
  }

  if (scale < 0) {
    return Nothing();
  }
  return Some(clamped(LayoutDeviceToScreenScale(scale), kViewportMinScale,
                      kViewportMaxScale));
}

bool Document::ParseScalesInViewportMetaData(
    const ViewportMetaData& aViewportMetaData) {
  Maybe<LayoutDeviceToScreenScale> scale;

  scale = ParseScaleString(aViewportMetaData.mInitialScale);
  mScaleFloat = scale.valueOr(LayoutDeviceToScreenScale(0.0f));
  mValidScaleFloat = scale.isSome();

  scale = ParseScaleString(aViewportMetaData.mMaximumScale);
  // Chrome uses '5' for the fallback value of maximum-scale, we might
  // consider matching it in future.
  // https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/html_meta_element.cc?l=452&rcl=65ca4278b42d269ca738fc93ef7ae04a032afeb0
  mScaleMaxFloat = scale.valueOr(kViewportMaxScale);
  mValidMaxScale = scale.isSome();

  scale = ParseScaleString(aViewportMetaData.mMinimumScale);
  mScaleMinFloat = scale.valueOr(kViewportMinScale);
  mValidMinScale = scale.isSome();

  // Resolve min-zoom and max-zoom values.
  // https://drafts.csswg.org/css-device-adapt/#constraining-min-max-zoom
  if (mValidMaxScale && mValidMinScale) {
    mScaleMaxFloat = std::max(mScaleMinFloat, mScaleMaxFloat);
  }
  return mValidScaleFloat || mValidMaxScale || mValidMinScale;
}

bool Document::ParseWidthAndHeightInMetaViewport(const nsAString& aWidthString,
                                                 const nsAString& aHeightString,
                                                 bool aHasValidScale) {
  // The width and height properties
  // https://drafts.csswg.org/css-device-adapt/#width-and-height-properties
  //
  // The width and height viewport <META> properties are translated into width
  // and height descriptors, setting the min-width/min-height value to
  // extend-to-zoom and the max-width/max-height value to the length from the
  // viewport <META> property as follows:
  //
  // 1. Non-negative number values are translated to pixel lengths, clamped to
  //    the range: [1px, 10000px]
  // 2. Negative number values are dropped
  // 3. device-width and device-height translate to 100vw and 100vh respectively
  // 4. Other keywords and unknown values are also dropped
  mMinWidth = nsViewportInfo::Auto;
  mMaxWidth = nsViewportInfo::Auto;
  if (!aWidthString.IsEmpty()) {
    mMinWidth = nsViewportInfo::ExtendToZoom;
    if (aWidthString.EqualsLiteral("device-width")) {
      mMaxWidth = nsViewportInfo::DeviceSize;
    } else {
      nsresult widthErrorCode;
      mMaxWidth = aWidthString.ToInteger(&widthErrorCode);
      if (NS_FAILED(widthErrorCode)) {
        mMaxWidth = nsViewportInfo::Auto;
      } else if (mMaxWidth >= 0.0f) {
        mMaxWidth = clamped(mMaxWidth, CSSCoord(1.0f), CSSCoord(10000.0f));
      } else {
        mMaxWidth = nsViewportInfo::Auto;
      }
    }
  } else if (aHasValidScale) {
    if (aHeightString.IsEmpty()) {
      mMinWidth = nsViewportInfo::ExtendToZoom;
      mMaxWidth = nsViewportInfo::ExtendToZoom;
    }
  } else if (aHeightString.IsEmpty() && UseWidthDeviceWidthFallbackViewport()) {
    mMinWidth = nsViewportInfo::ExtendToZoom;
    mMaxWidth = nsViewportInfo::DeviceSize;
  }

  mMinHeight = nsViewportInfo::Auto;
  mMaxHeight = nsViewportInfo::Auto;
  if (!aHeightString.IsEmpty()) {
    mMinHeight = nsViewportInfo::ExtendToZoom;
    if (aHeightString.EqualsLiteral("device-height")) {
      mMaxHeight = nsViewportInfo::DeviceSize;
    } else {
      nsresult heightErrorCode;
      mMaxHeight = aHeightString.ToInteger(&heightErrorCode);
      if (NS_FAILED(heightErrorCode)) {
        mMaxHeight = nsViewportInfo::Auto;
      } else if (mMaxHeight >= 0.0f) {
        mMaxHeight = clamped(mMaxHeight, CSSCoord(1.0f), CSSCoord(10000.0f));
      } else {
        mMaxHeight = nsViewportInfo::Auto;
      }
    }
  }

  return !aWidthString.IsEmpty() || !aHeightString.IsEmpty();
}

nsViewportInfo Document::GetViewportInfo(const ScreenIntSize& aDisplaySize) {
  MOZ_ASSERT(mPresShell);

  // Compute the CSS-to-LayoutDevice pixel scale as the product of the
  // widget scale and the full zoom.
  nsPresContext* context = mPresShell->GetPresContext();
  // When querying the full zoom, get it from the device context rather than
  // directly from the pres context, because the device context's value can
  // include an adjustment necessay to keep the number of app units per device
  // pixel an integer, and we want the adjusted value.
  float fullZoom = context ? context->DeviceContext()->GetFullZoom() : 1.0;
  fullZoom = (fullZoom == 0.0) ? 1.0 : fullZoom;
  CSSToLayoutDeviceScale layoutDeviceScale =
      context ? context->CSSToDevPixelScale() : CSSToLayoutDeviceScale(1);

  CSSToScreenScale defaultScale =
      layoutDeviceScale * LayoutDeviceToScreenScale(1.0);

  // Special behaviour for desktop mode, provided we are not on an about: page
  nsPIDOMWindowOuter* win = GetWindow();
  if (win && win->IsDesktopModeViewport() && !IsAboutPage()) {
    CSSCoord viewportWidth =
        StaticPrefs::browser_viewport_desktopWidth() / fullZoom;
    CSSToScreenScale scaleToFit(aDisplaySize.width / viewportWidth);
    float aspectRatio = (float)aDisplaySize.height / aDisplaySize.width;
    CSSSize viewportSize(viewportWidth, viewportWidth * aspectRatio);
    ScreenIntSize fakeDesktopSize = RoundedToInt(viewportSize * scaleToFit);
    return nsViewportInfo(fakeDesktopSize, scaleToFit,
                          nsViewportInfo::ZoomFlag::AllowZoom);
  }

  if (!nsLayoutUtils::ShouldHandleMetaViewport(this)) {
    return nsViewportInfo(aDisplaySize, defaultScale,
                          nsLayoutUtils::AllowZoomingForDocument(this)
                              ? nsViewportInfo::ZoomFlag::AllowZoom
                              : nsViewportInfo::ZoomFlag::DisallowZoom);
  }

  // In cases where the width of the CSS viewport is less than or equal to the
  // width of the display (i.e. width <= device-width) then we disable
  // double-tap-to-zoom behaviour. See bug 941995 for details.

  switch (mViewportType) {
    case DisplayWidthHeight:
      return nsViewportInfo(aDisplaySize, defaultScale,
                            nsViewportInfo::ZoomFlag::AllowZoom);
    case Unknown: {
      nsAutoString viewport;
      GetHeaderData(nsGkAtoms::viewport, viewport);
      // We might early exit if the viewport is empty. Even if we don't,
      // at the end of this case we'll note that it was empty. Later, when
      // we're using the cached values, this will trigger alternate code paths.
      if (viewport.IsEmpty()) {
        // If the docType specifies that we are on a site optimized for mobile,
        // then we want to return specially crafted defaults for the viewport
        // info.
        RefPtr<DocumentType> docType = GetDoctype();
        if (docType) {
          nsAutoString docId;
          docType->GetPublicId(docId);
          if ((docId.Find("WAP") != -1) || (docId.Find("Mobile") != -1) ||
              (docId.Find("WML") != -1)) {
            // We're making an assumption that the docType can't change here
            mViewportType = DisplayWidthHeight;
            return nsViewportInfo(aDisplaySize, defaultScale,
                                  nsViewportInfo::ZoomFlag::AllowZoom);
          }
        }

        nsAutoString handheldFriendly;
        GetHeaderData(nsGkAtoms::handheldFriendly, handheldFriendly);
        if (handheldFriendly.EqualsLiteral("true")) {
          mViewportType = DisplayWidthHeight;
          return nsViewportInfo(aDisplaySize, defaultScale,
                                nsViewportInfo::ZoomFlag::AllowZoom);
        }
      }

      ViewportMetaData metaData = GetViewportMetaData();

      // Parse initial-scale, minimum-scale and maximum-scale.
      bool hasValidContents = ParseScalesInViewportMetaData(metaData);

      // Parse width and height properties
      // This function sets m{Min,Max}{Width,Height}.
      if (ParseWidthAndHeightInMetaViewport(metaData.mWidth, metaData.mHeight,
                                            mValidScaleFloat)) {
        hasValidContents = true;
      }

      mAllowZoom = true;
      if ((metaData.mUserScalable.EqualsLiteral("0")) ||
          (metaData.mUserScalable.EqualsLiteral("no")) ||
          (metaData.mUserScalable.EqualsLiteral("false"))) {
        mAllowZoom = false;
      }
      if (!metaData.mUserScalable.IsEmpty()) {
        hasValidContents = true;
      }

      // Resolve viewport-fit value.
      // https://drafts.csswg.org/css-round-display/#viewport-fit-descriptor
      mViewportFit = ViewportFitType::Auto;
      if (!metaData.mViewportFit.IsEmpty()) {
        if (metaData.mViewportFit.EqualsLiteral("contain")) {
          mViewportFit = ViewportFitType::Contain;
          hasValidContents = true;
        } else if (metaData.mViewportFit.EqualsLiteral("cover")) {
          mViewportFit = ViewportFitType::Cover;
          hasValidContents = true;
        } else if (metaData.mViewportFit.EqualsLiteral("auto")) {
          hasValidContents = true;
        }
      }

      mWidthStrEmpty = metaData.mWidth.IsEmpty();

      mViewportType = hasValidContents ? Specified : NoValidContent;
      [[fallthrough]];
    }
    case Specified:
    case NoValidContent:
    default:
      LayoutDeviceToScreenScale effectiveMinScale = mScaleMinFloat;
      LayoutDeviceToScreenScale effectiveMaxScale = mScaleMaxFloat;
      bool effectiveValidMaxScale = mValidMaxScale;

      nsViewportInfo::ZoomFlag effectiveZoomFlag =
          mAllowZoom ? nsViewportInfo::ZoomFlag::AllowZoom
                     : nsViewportInfo::ZoomFlag::DisallowZoom;
      if (StaticPrefs::browser_ui_zoom_force_user_scalable()) {
        // If the pref to force user-scalable is enabled, we ignore the values
        // from the meta-viewport tag for these properties and just assume they
        // allow the page to be scalable. Note in particular that this code is
        // in the "Specified" branch of the enclosing switch statement, so that
        // calls to GetViewportInfo always use the latest value of the
        // browser_ui_zoom_force_user_scalable pref. Other codepaths that
        // return nsViewportInfo instances are all consistent with
        // browser_ui_zoom_force_user_scalable() already.
        effectiveMinScale = kViewportMinScale;
        effectiveMaxScale = kViewportMaxScale;
        effectiveValidMaxScale = true;
        effectiveZoomFlag = nsViewportInfo::ZoomFlag::AllowZoom;
      }

      // Returns extend-zoom value which is MIN(mScaleFloat, mScaleMaxFloat).
      auto ComputeExtendZoom = [&]() -> float {
        if (mValidScaleFloat && effectiveValidMaxScale) {
          return std::min(mScaleFloat.scale, effectiveMaxScale.scale);
        }
        if (mValidScaleFloat) {
          return mScaleFloat.scale;
        }
        if (effectiveValidMaxScale) {
          return effectiveMaxScale.scale;
        }
        return nsViewportInfo::Auto;
      };

      // Resolving 'extend-to-zoom'
      // https://drafts.csswg.org/css-device-adapt/#resolve-extend-to-zoom
      float extendZoom = ComputeExtendZoom();

      CSSCoord minWidth = mMinWidth;
      CSSCoord maxWidth = mMaxWidth;
      CSSCoord minHeight = mMinHeight;
      CSSCoord maxHeight = mMaxHeight;

      // aDisplaySize is in screen pixels; convert them to CSS pixels for the
      // viewport size. We need to use this scaled size for any clamping of
      // width or height.
      CSSSize displaySize = ScreenSize(aDisplaySize) / defaultScale;

      // Resolve device-width and device-height first.
      if (maxWidth == nsViewportInfo::DeviceSize) {
        maxWidth = displaySize.width;
      }
      if (maxHeight == nsViewportInfo::DeviceSize) {
        maxHeight = displaySize.height;
      }
      if (extendZoom == nsViewportInfo::Auto) {
        if (maxWidth == nsViewportInfo::ExtendToZoom) {
          maxWidth = nsViewportInfo::Auto;
        }
        if (maxHeight == nsViewportInfo::ExtendToZoom) {
          maxHeight = nsViewportInfo::Auto;
        }
        if (minWidth == nsViewportInfo::ExtendToZoom) {
          minWidth = maxWidth;
        }
        if (minHeight == nsViewportInfo::ExtendToZoom) {
          minHeight = maxHeight;
        }
      } else {
        CSSSize extendSize = displaySize / extendZoom;
        if (maxWidth == nsViewportInfo::ExtendToZoom) {
          maxWidth = extendSize.width;
        }
        if (maxHeight == nsViewportInfo::ExtendToZoom) {
          maxHeight = extendSize.height;
        }
        if (minWidth == nsViewportInfo::ExtendToZoom) {
          minWidth = nsViewportInfo::Max(extendSize.width, maxWidth);
        }
        if (minHeight == nsViewportInfo::ExtendToZoom) {
          minHeight = nsViewportInfo::Max(extendSize.height, maxHeight);
        }
      }
      // Resolve initial width and height from min/max descriptors
      // https://drafts.csswg.org/css-device-adapt/#resolve-initial-width-height
      CSSCoord width = nsViewportInfo::Auto;
      if (minWidth != nsViewportInfo::Auto ||
          maxWidth != nsViewportInfo::Auto) {
        width = nsViewportInfo::Max(
            minWidth, nsViewportInfo::Min(maxWidth, displaySize.width));
      }
      CSSCoord height = nsViewportInfo::Auto;
      if (minHeight != nsViewportInfo::Auto ||
          maxHeight != nsViewportInfo::Auto) {
        height = nsViewportInfo::Max(
            minHeight, nsViewportInfo::Min(maxHeight, displaySize.height));
      }

      // Resolve width value
      // https://drafts.csswg.org/css-device-adapt/#resolve-width
      if (width == nsViewportInfo::Auto) {
        if (height == nsViewportInfo::Auto || aDisplaySize.height == 0) {
          // What we do in this situation deviates somewhat from the spec. We
          // want to consider the case where no viewport information has been
          // provided, in which case we want to assume that the document is not
          // optimized for aDisplaySize, and we should instead force a useful
          // size.
          if (mViewportType == NoValidContent) {
            // If we don't have any applicable viewport width constraints, this
            // is most likely a desktop page written without mobile devices in
            // mind. We use the desktop mode viewport for those pages by
            // default, because a narrow viewport based on typical mobile device
            // screen sizes (especially in portrait mode) will frequently break
            // the layout of such pages. To keep text readable in that case, we
            // rely on font inflation instead.

            // Divide by fullZoom to stretch CSS pixel size of viewport in order
            // to keep device pixel size unchanged after full zoom applied.
            // See bug 974242.
            width = StaticPrefs::browser_viewport_desktopWidth() / fullZoom;
          } else {
            // Some viewport information was provided; follow the spec.
            width = displaySize.width;
          }
        } else {
          width = height * aDisplaySize.width / aDisplaySize.height;
        }
      }

      // Resolve height value
      // https://drafts.csswg.org/css-device-adapt/#resolve-height
      if (height == nsViewportInfo::Auto) {
        if (aDisplaySize.width == 0) {
          height = displaySize.height;
        } else {
          height = width * aDisplaySize.height / aDisplaySize.width;
        }
      }
      MOZ_ASSERT(width != nsViewportInfo::Auto &&
                 height != nsViewportInfo::Auto);

      CSSSize size(width, height);

      CSSToScreenScale scaleFloat = mScaleFloat * layoutDeviceScale;
      CSSToScreenScale scaleMinFloat = effectiveMinScale * layoutDeviceScale;
      CSSToScreenScale scaleMaxFloat = effectiveMaxScale * layoutDeviceScale;

      nsViewportInfo::AutoSizeFlag sizeFlag =
          nsViewportInfo::AutoSizeFlag::FixedSize;
      if (mMaxWidth == nsViewportInfo::DeviceSize ||
          (mWidthStrEmpty && (mMaxHeight == nsViewportInfo::DeviceSize ||
                              mScaleFloat.scale == 1.0f)) ||
          (!mWidthStrEmpty && mMaxWidth == nsViewportInfo::Auto &&
           mMaxHeight < 0)) {
        sizeFlag = nsViewportInfo::AutoSizeFlag::AutoSize;
      }

      // FIXME: Resolving width and height should be done above 'Resolve width
      // value' and 'Resolve height value'.
      if (sizeFlag == nsViewportInfo::AutoSizeFlag::AutoSize) {
        size = displaySize;
      }

      // The purpose of clamping the viewport width to a minimum size is to
      // prevent page authors from setting it to a ridiculously small value.
      // If the page is actually being rendered in a very small area (as might
      // happen in e.g. Android 8's picture-in-picture mode), we don't want to
      // prevent the viewport from taking on that size.
      CSSSize effectiveMinSize = Min(CSSSize(kViewportMinSize), displaySize);

      size.width = clamped(size.width, effectiveMinSize.width,
                           float(kViewportMaxSize.width));

      // Also recalculate the default zoom, if it wasn't specified in the
      // metadata, and the width is specified.
      if (!mValidScaleFloat && !mWidthStrEmpty) {
        CSSToScreenScale bestFitScale(float(aDisplaySize.width) / size.width);
        scaleFloat = (scaleFloat > bestFitScale) ? scaleFloat : bestFitScale;
      }

      size.height = clamped(size.height, effectiveMinSize.height,
                            float(kViewportMaxSize.height));

      // We need to perform a conversion, but only if the initial or maximum
      // scale were set explicitly by the user.
      if (mValidScaleFloat && scaleFloat >= scaleMinFloat &&
          scaleFloat <= scaleMaxFloat) {
        CSSSize displaySize = ScreenSize(aDisplaySize) / scaleFloat;
        size.width = std::max(size.width, displaySize.width);
        size.height = std::max(size.height, displaySize.height);
      } else if (effectiveValidMaxScale) {
        CSSSize displaySize = ScreenSize(aDisplaySize) / scaleMaxFloat;
        size.width = std::max(size.width, displaySize.width);
        size.height = std::max(size.height, displaySize.height);
      }

      return nsViewportInfo(
          scaleFloat, scaleMinFloat, scaleMaxFloat, size, sizeFlag,
          mValidScaleFloat ? nsViewportInfo::AutoScaleFlag::FixedScale
                           : nsViewportInfo::AutoScaleFlag::AutoScale,
          effectiveZoomFlag, mViewportFit);
  }
}

ViewportMetaData Document::GetViewportMetaData() const {
  // The order of mMetaViewport is first-modified is first. We want the last
  // modified since Chrome uses the last one.
  // See https://webcompat.com/issues/20701#issuecomment-436054739
  return !mMetaViewports.IsEmpty() ? mMetaViewports.LastElement().mData
                                   : ViewportMetaData();
}

void Document::AddMetaViewportElement(HTMLMetaElement* aElement,
                                      ViewportMetaData&& aData) {
  for (size_t i = 0; i < mMetaViewports.Length(); i++) {
    MetaViewportElementAndData& viewport = mMetaViewports[i];
    if (viewport.mElement == aElement) {
      if (viewport.mData == aData) {
        return;
      }
      // Move the existing one to the tail since Chrome uses the last modified
      // viewport meta tag.
      mMetaViewports.RemoveElementAt(i);
      break;
    }
  }

  mMetaViewports.AppendElement(MetaViewportElementAndData{aElement, aData});
  // Trigger recomputation of the nsViewportInfo the next time it's queried.
  mViewportType = Unknown;

  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      this, NS_LITERAL_STRING("DOMMetaViewportFitChanged"), CanBubble::eYes,
      ChromeOnlyDispatch::eYes);
  asyncDispatcher->RunDOMEventWhenSafe();
}

void Document::RemoveMetaViewportElement(HTMLMetaElement* aElement) {
  for (MetaViewportElementAndData& viewport : mMetaViewports) {
    if (viewport.mElement == aElement) {
      mMetaViewports.RemoveElement(viewport);
      // Trigger recomputation of the nsViewportInfo the next time it's queried.
      mViewportType = Unknown;

      RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
          this, NS_LITERAL_STRING("DOMMetaViewportFitChanged"), CanBubble::eYes,
          ChromeOnlyDispatch::eYes);
      asyncDispatcher->RunDOMEventWhenSafe();
      return;
    }
  }
}

void Document::UpdateForScrollAnchorAdjustment(nscoord aLength) {
  mScrollAnchorAdjustmentLength += abs(aLength);
  mScrollAnchorAdjustmentCount += 1;
}

EventListenerManager* Document::GetOrCreateListenerManager() {
  if (!mListenerManager) {
    mListenerManager =
        new EventListenerManager(static_cast<EventTarget*>(this));
    SetFlags(NODE_HAS_LISTENERMANAGER);
  }

  return mListenerManager;
}

EventListenerManager* Document::GetExistingListenerManager() const {
  return mListenerManager;
}

void Document::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  aVisitor.mCanHandle = true;
  // FIXME! This is a hack to make middle mouse paste working also in Editor.
  // Bug 329119
  aVisitor.mForceContentDispatch = true;

  // Load events must not propagate to |window| object, see bug 335251.
  if (aVisitor.mEvent->mMessage != eLoad) {
    nsGlobalWindowOuter* window = nsGlobalWindowOuter::Cast(GetWindow());
    aVisitor.SetParentTarget(
        window ? window->GetTargetForEventTargetChain() : nullptr, false);
  }
}

already_AddRefed<Event> Document::CreateEvent(const nsAString& aEventType,
                                              CallerType aCallerType,
                                              ErrorResult& rv) const {
  nsPresContext* presContext = GetPresContext();

  // Create event even without presContext.
  RefPtr<Event> ev =
      EventDispatcher::CreateEvent(const_cast<Document*>(this), presContext,
                                   nullptr, aEventType, aCallerType);
  if (!ev) {
    rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }
  WidgetEvent* e = ev->WidgetEventPtr();
  e->mFlags.mBubbles = false;
  e->mFlags.mCancelable = false;
  return ev.forget();
}

void Document::FlushPendingNotifications(FlushType aType) {
  mozilla::ChangesToFlush flush(aType, aType >= FlushType::Style);
  FlushPendingNotifications(flush);
}

class nsDocumentOnStack {
 public:
  explicit nsDocumentOnStack(Document* aDoc) : mDoc(aDoc) {
    mDoc->IncreaseStackRefCnt();
  }
  ~nsDocumentOnStack() { mDoc->DecreaseStackRefCnt(); }

 private:
  Document* mDoc;
};

void Document::FlushPendingNotifications(mozilla::ChangesToFlush aFlush) {
  FlushType flushType = aFlush.mFlushType;

  nsDocumentOnStack dos(this);

  // We need to flush the sink for non-HTML documents (because the XML
  // parser still does insertion with deferred notifications).  We
  // also need to flush the sink if this is a layout-related flush, to
  // make sure that layout is started as needed.  But we can skip that
  // part if we have no presshell or if it's already done an initial
  // reflow.
  if ((!IsHTMLDocument() || (flushType > FlushType::ContentAndNotify &&
                             mPresShell && !mPresShell->DidInitialize())) &&
      (mParser || mWeakSink)) {
    nsCOMPtr<nsIContentSink> sink;
    if (mParser) {
      sink = mParser->GetContentSink();
    } else {
      sink = do_QueryReferent(mWeakSink);
      if (!sink) {
        mWeakSink = nullptr;
      }
    }
    // Determine if it is safe to flush the sink notifications
    // by determining if it safe to flush all the presshells.
    if (sink && (flushType == FlushType::Content || IsSafeToFlush())) {
      sink->FlushPendingNotifications(flushType);
    }
  }

  // Should we be flushing pending binding constructors in here?

  if (flushType <= FlushType::ContentAndNotify) {
    // Nothing to do here
    return;
  }

  // If we have a parent we must flush the parent too to ensure that our
  // container is reflowed if its size was changed.
  //
  // We do it only if the subdocument and the parent can observe each other
  // synchronously (that is, if we're not cross-origin), to avoid work that is
  // not observable, and if the parent document has finished loading all its
  // render-blocking stylesheets and may start laying out the document, to avoid
  // unnecessary flashes of unstyled content on the parent document. Note that
  // this last bit means that size-dependent media queries in this document may
  // produce incorrect results temporarily.
  //
  // But if it's not safe to flush ourselves, then don't flush the parent, since
  // that can cause things like resizes of our frame's widget, which we can't
  // handle while flushing is unsafe.
  if (StyleOrLayoutObservablyDependsOnParentDocumentLayout() &&
      mParentDocument->MayStartLayout() && IsSafeToFlush()) {
    ChangesToFlush parentFlush = aFlush;
    if (flushType >= FlushType::Style) {
      // Since media queries mean that a size change of our container can affect
      // style, we need to promote a style flush on ourself to a layout flush on
      // our parent, since we need our container to be the correct size to
      // determine the correct style.
      parentFlush.mFlushType = std::max(FlushType::Layout, flushType);
    }
    mParentDocument->FlushPendingNotifications(parentFlush);
  }

  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(aFlush);
  }
}

void Document::FlushExternalResources(FlushType aType) {
  NS_ASSERTION(
      aType >= FlushType::Style,
      "should only need to flush for style or higher in external resources");
  if (GetDisplayDocument()) {
    return;
  }

  auto flush = [aType](Document& aDoc) {
    aDoc.FlushPendingNotifications(aType);
    return CallState::Continue;
  };

  EnumerateExternalResources(flush);
}

void Document::SetXMLDeclaration(const char16_t* aVersion,
                                 const char16_t* aEncoding,
                                 const int32_t aStandalone) {
  if (!aVersion || *aVersion == '\0') {
    mXMLDeclarationBits = 0;
    return;
  }

  mXMLDeclarationBits = XML_DECLARATION_BITS_DECLARATION_EXISTS;

  if (aEncoding && *aEncoding != '\0') {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_ENCODING_EXISTS;
  }

  if (aStandalone == 1) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS |
                           XML_DECLARATION_BITS_STANDALONE_YES;
  } else if (aStandalone == 0) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS;
  }
}

void Document::GetXMLDeclaration(nsAString& aVersion, nsAString& aEncoding,
                                 nsAString& aStandalone) {
  aVersion.Truncate();
  aEncoding.Truncate();
  aStandalone.Truncate();

  if (!(mXMLDeclarationBits & XML_DECLARATION_BITS_DECLARATION_EXISTS)) {
    return;
  }

  // always until we start supporting 1.1 etc.
  aVersion.AssignLiteral("1.0");

  if (mXMLDeclarationBits & XML_DECLARATION_BITS_ENCODING_EXISTS) {
    // This is what we have stored, not necessarily what was written
    // in the original
    GetCharacterSet(aEncoding);
  }

  if (mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_EXISTS) {
    if (mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_YES) {
      aStandalone.AssignLiteral("yes");
    } else {
      aStandalone.AssignLiteral("no");
    }
  }
}

bool Document::IsScriptEnabled() {
  // If this document is sandboxed without 'allow-scripts'
  // script is not enabled
  if (HasScriptsBlockedBySandbox()) {
    return false;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject =
      do_QueryInterface(GetInnerWindow());
  if (!globalObject || !globalObject->HasJSGlobal()) {
    return false;
  }

  return xpc::Scriptability::Get(globalObject->GetGlobalJSObjectPreserveColor())
      .Allowed();
}

void Document::RetrieveRelevantHeaders(nsIChannel* aChannel) {
  PRTime modDate = 0;
  nsresult rv;

  nsCOMPtr<nsIHttpChannel> httpChannel;
  rv = GetHttpChannelHelper(aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (httpChannel) {
    nsAutoCString tmp;
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("last-modified"),
                                        tmp);

    if (NS_SUCCEEDED(rv)) {
      PRTime time;
      PRStatus st = PR_ParseTimeString(tmp.get(), true, &time);
      if (st == PR_SUCCESS) {
        modDate = time;
      }
    }

    static const char* const headers[] = {
        "default-style", "content-style-type", "content-language",
        "content-disposition", "refresh", "x-dns-prefetch-control",
        "x-frame-options",
        // add more http headers if you need
        // XXXbz don't add content-location support without reading bug
        // 238654 and its dependencies/dups first.
        0};

    nsAutoCString headerVal;
    const char* const* name = headers;
    while (*name) {
      rv = httpChannel->GetResponseHeader(nsDependentCString(*name), headerVal);
      if (NS_SUCCEEDED(rv) && !headerVal.IsEmpty()) {
        RefPtr<nsAtom> key = NS_Atomize(*name);
        SetHeaderData(key, NS_ConvertASCIItoUTF16(headerVal));
      }
      ++name;
    }
  } else {
    nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(aChannel);
    if (fileChannel) {
      nsCOMPtr<nsIFile> file;
      fileChannel->GetFile(getter_AddRefs(file));
      if (file) {
        PRTime msecs;
        rv = file->GetLastModifiedTime(&msecs);

        if (NS_SUCCEEDED(rv)) {
          modDate = msecs * int64_t(PR_USEC_PER_MSEC);
        }
      }
    } else {
      nsAutoCString contentDisp;
      rv = aChannel->GetContentDispositionHeader(contentDisp);
      if (NS_SUCCEEDED(rv)) {
        SetHeaderData(nsGkAtoms::headerContentDisposition,
                      NS_ConvertASCIItoUTF16(contentDisp));
      }
    }
  }

  mLastModified.Truncate();
  if (modDate != 0) {
    GetFormattedTimeString(modDate, mLastModified);
  }
}

already_AddRefed<Element> Document::CreateElem(const nsAString& aName,
                                               nsAtom* aPrefix,
                                               int32_t aNamespaceID,
                                               const nsAString* aIs) {
#ifdef DEBUG
  nsAutoString qName;
  if (aPrefix) {
    aPrefix->ToString(qName);
    qName.Append(':');
  }
  qName.Append(aName);

  // Note: "a:b:c" is a valid name in non-namespaces XML, and
  // Document::CreateElement can call us with such a name and no prefix,
  // which would cause an error if we just used true here.
  bool nsAware = aPrefix != nullptr || aNamespaceID != GetDefaultNamespaceID();
  NS_ASSERTION(NS_SUCCEEDED(nsContentUtils::CheckQName(qName, nsAware)),
               "Don't pass invalid prefixes to Document::CreateElem, "
               "check caller.");
#endif

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  mNodeInfoManager->GetNodeInfo(aName, aPrefix, aNamespaceID, ELEMENT_NODE,
                                getter_AddRefs(nodeInfo));
  NS_ENSURE_TRUE(nodeInfo, nullptr);

  nsCOMPtr<Element> element;
  nsresult rv = NS_NewElement(getter_AddRefs(element), nodeInfo.forget(),
                              NOT_FROM_PARSER, aIs);
  return NS_SUCCEEDED(rv) ? element.forget() : nullptr;
}

bool Document::IsSafeToFlush() const {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return true;
  }
  return presShell->IsSafeToFlush();
}

void Document::Sanitize() {
  // Sanitize the document by resetting all (current and former) password fields
  // and any form fields with autocomplete=off to their default values.  We do
  // this now, instead of when the presentation is restored, to offer some
  // protection in case there is ever an exploit that allows a cached document
  // to be accessed from a different document.

  // First locate all input elements, regardless of whether they are
  // in a form, and reset the password and autocomplete=off elements.

  RefPtr<nsContentList> nodes =
      GetElementsByTagName(NS_LITERAL_STRING("input"));

  nsAutoString value;

  uint32_t length = nodes->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    NS_ASSERTION(nodes->Item(i), "null item in node list!");

    RefPtr<HTMLInputElement> input =
        HTMLInputElement::FromNodeOrNull(nodes->Item(i));
    if (!input) continue;

    input->GetAttr(nsGkAtoms::autocomplete, value);
    if (value.LowerCaseEqualsLiteral("off") || input->HasBeenTypePassword()) {
      input->Reset();
    }
  }

  // Now locate all _form_ elements that have autocomplete=off and reset them
  nodes = GetElementsByTagName(NS_LITERAL_STRING("form"));

  length = nodes->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    NS_ASSERTION(nodes->Item(i), "null item in nodelist");

    HTMLFormElement* form = HTMLFormElement::FromNode(nodes->Item(i));
    if (!form) continue;

    form->GetAttr(kNameSpaceID_None, nsGkAtoms::autocomplete, value);
    if (value.LowerCaseEqualsLiteral("off")) form->Reset();
  }
}

void Document::EnumerateSubDocuments(SubDocEnumFunc aCallback) {
  if (!mSubDocuments) {
    return;
  }

  // PLDHashTable::Iterator can't handle modifications while iterating so we
  // copy all entries to an array first before calling any callbacks.
  AutoTArray<RefPtr<Document>, 8> subdocs;
  for (auto iter = mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<SubDocMapEntry*>(iter.Get());
    if (Document* subdoc = entry->mSubDocument) {
      subdocs.AppendElement(subdoc);
    }
  }
  for (auto& subdoc : subdocs) {
    if (aCallback(*subdoc) == CallState::Stop) {
      break;
    }
  }
}

void Document::CollectDescendantDocuments(
    nsTArray<RefPtr<Document>>& aDescendants, nsDocTestFunc aCallback) const {
  if (!mSubDocuments) {
    return;
  }

  for (auto iter = mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<SubDocMapEntry*>(iter.Get());
    const Document* subdoc = entry->mSubDocument;
    if (subdoc) {
      if (aCallback(subdoc)) {
        aDescendants.AppendElement(entry->mSubDocument);
      }
      subdoc->CollectDescendantDocuments(aDescendants, aCallback);
    }
  }
}

bool Document::CanSavePresentation(nsIRequest* aNewRequest,
                                   uint16_t& aBFCacheCombo) {
  bool ret = true;

  if (!IsBFCachingAllowed()) {
    aBFCacheCombo |= BFCacheStatus::NOT_ALLOWED;
    ret = false;
  }

  nsAutoCString uri;
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gPageCacheLog, LogLevel::Verbose))) {
    if (mDocumentURI) {
      mDocumentURI->GetSpec(uri);
    }
  }

  if (EventHandlingSuppressed()) {
    MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
            ("Save of %s blocked on event handling suppression", uri.get()));
    aBFCacheCombo |= BFCacheStatus::EVENT_HANDLING_SUPPRESSED;
    ret = false;
  }

  // Do not allow suspended windows to be placed in the
  // bfcache.  This method is also used to verify a document
  // coming out of the bfcache is ok to restore, though.  So
  // we only want to block suspend windows that aren't also
  // frozen.
  nsPIDOMWindowInner* win = GetInnerWindow();
  if (win && win->IsSuspended() && !win->IsFrozen()) {
    MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
            ("Save of %s blocked on suspended Window", uri.get()));
    aBFCacheCombo |= BFCacheStatus::SUSPENDED;
    ret = false;
  }

  // Check our event listener manager for unload/beforeunload listeners.
  nsCOMPtr<EventTarget> piTarget = do_QueryInterface(mScriptGlobalObject);
  if (piTarget) {
    EventListenerManager* manager = piTarget->GetExistingListenerManager();
    if (manager && manager->HasUnloadListeners()) {
      MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
              ("Save of %s blocked due to unload handlers", uri.get()));
      aBFCacheCombo |= BFCacheStatus::UNLOAD_LISTENER;
      ret = false;
    }
  }

  // Check if we have pending network requests
  nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
  if (loadGroup) {
    nsCOMPtr<nsISimpleEnumerator> requests;
    loadGroup->GetRequests(getter_AddRefs(requests));

    bool hasMore = false;

    // We want to bail out if we have any requests other than aNewRequest (or
    // in the case when aNewRequest is a part of a multipart response the base
    // channel the multipart response is coming in on).
    nsCOMPtr<nsIChannel> baseChannel;
    nsCOMPtr<nsIMultiPartChannel> part(do_QueryInterface(aNewRequest));
    if (part) {
      part->GetBaseChannel(getter_AddRefs(baseChannel));
    }

    while (NS_SUCCEEDED(requests->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> elem;
      requests->GetNext(getter_AddRefs(elem));

      nsCOMPtr<nsIRequest> request = do_QueryInterface(elem);
      if (request && request != aNewRequest && request != baseChannel) {
        // Favicon loads don't need to block caching.
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        if (channel) {
          nsCOMPtr<nsILoadInfo> li = channel->LoadInfo();
          if (li->InternalContentPolicyType() ==
              nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON) {
            continue;
          }
        }

        if (MOZ_UNLIKELY(MOZ_LOG_TEST(gPageCacheLog, LogLevel::Verbose))) {
          nsAutoCString requestName;
          request->GetName(requestName);
          MOZ_LOG(gPageCacheLog, LogLevel::Verbose,
                  ("Save of %s blocked because document has request %s",
                   uri.get(), requestName.get()));
        }
        aBFCacheCombo |= BFCacheStatus::REQUEST;
        ret = false;
      }
    }
  }

  // Check if we have active GetUserMedia use
  if (MediaManager::Exists() && win &&
      MediaManager::Get()->IsWindowStillActive(win->WindowID())) {
    MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
            ("Save of %s blocked due to GetUserMedia", uri.get()));
    aBFCacheCombo |= BFCacheStatus::ACTIVE_GET_USER_MEDIA;
    ret = false;
  }

#ifdef MOZ_WEBRTC
  // Check if we have active PeerConnections
  if (win && win->HasActivePeerConnections()) {
    MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
            ("Save of %s blocked due to PeerConnection", uri.get()));
    aBFCacheCombo |= BFCacheStatus::ACTIVE_PEER_CONNECTION;
    ret = false;
  }
#endif  // MOZ_WEBRTC

  // Don't save presentations for documents containing EME content, so that
  // CDMs reliably shutdown upon user navigation.
  if (ContainsEMEContent()) {
    aBFCacheCombo |= BFCacheStatus::CONTAINS_EME_CONTENT;
    ret = false;
  }

  // Don't save presentations for documents containing MSE content, to
  // reduce memory usage.
  if (ContainsMSEContent()) {
    MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
            ("Save of %s blocked due to MSE use", uri.get()));
    aBFCacheCombo |= BFCacheStatus::CONTAINS_MSE_CONTENT;
    ret = false;
  }

  if (mSubDocuments) {
    for (auto iter = mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<SubDocMapEntry*>(iter.Get());
      Document* subdoc = entry->mSubDocument;

      uint16_t subDocBFCacheCombo = 0;
      // The aIgnoreRequest we were passed is only for us, so don't pass it on.
      bool canCache =
          subdoc ? subdoc->CanSavePresentation(nullptr, subDocBFCacheCombo)
                 : false;
      if (!canCache) {
        MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
                ("Save of %s blocked due to subdocument blocked", uri.get()));
        aBFCacheCombo |= subDocBFCacheCombo;
        ret = false;
      }
    }
  }

  // BFCache is currently not compatible with remote subframes (bug 1609324)
  if (RefPtr<BrowsingContext> browsingContext = GetBrowsingContext()) {
    for (auto& child : browsingContext->GetChildren()) {
      if (!child->IsInProcess()) {
        aBFCacheCombo |= BFCacheStatus::CONTAINS_REMOTE_SUBFRAMES;
        ret = false;
        break;
      }
    }
  }

  if (win) {
    auto* globalWindow = nsGlobalWindowInner::Cast(win);
#ifdef MOZ_WEBSPEECH
    if (globalWindow->HasActiveSpeechSynthesis()) {
      MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
              ("Save of %s blocked due to Speech use", uri.get()));
      aBFCacheCombo |= BFCacheStatus::HAS_ACTIVE_SPEECH_SYNTHESIS;
      ret = false;
    }
#endif
    if (globalWindow->HasUsedVR()) {
      MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
              ("Save of %s blocked due to having used VR", uri.get()));
      aBFCacheCombo |= BFCacheStatus::HAS_USED_VR;
      ret = false;
    }
  }

  return ret;
}

void Document::Destroy() {
  // The ContentViewer wants to release the document now.  So, tell our content
  // to drop any references to the document so that it can be destroyed.
  if (mIsGoingAway) return;

  ReportUseCounters();

  mIsGoingAway = true;

  ScriptLoader()->Destroy();
  SetScriptGlobalObject(nullptr);
  RemovedFromDocShell();

  bool oldVal = mInUnlinkOrDeletion;
  mInUnlinkOrDeletion = true;

#ifdef DEBUG
  uint32_t oldChildCount = GetChildCount();
#endif

  for (nsIContent* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    child->DestroyContent();
    MOZ_ASSERT(child->GetParentNode() == this);
  }
  MOZ_ASSERT(oldChildCount == GetChildCount());
  MOZ_ASSERT(!mSubDocuments || mSubDocuments->EntryCount() == 0);

  mInUnlinkOrDeletion = oldVal;

  mLayoutHistoryState = nullptr;

  if (mOriginalDocument) {
    mOriginalDocument->mLatestStaticClone = nullptr;
  }

  // Shut down our external resource map.  We might not need this for
  // leak-fixing if we fix nsDocumentViewer to do cycle-collection, but
  // tearing down all those frame trees right now is the right thing to do.
  mExternalResourceMap.Shutdown();

  // Manually break cycles via promise's global object pointer.
  mReadyForIdle = nullptr;
  mOrientationPendingPromise = nullptr;
}

void Document::RemovedFromDocShell() {
  mEditingState = EditingState::eOff;

  if (mRemovedFromDocShell) return;

  mRemovedFromDocShell = true;
  EnumerateActivityObservers(NotifyActivityChanged);

  for (nsIContent* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    child->SaveSubtreeState();
  }

  nsIDocShell* docShell = GetDocShell();
  if (docShell) {
    docShell->SynchronizeLayoutHistoryState();
  }
}

already_AddRefed<nsILayoutHistoryState> Document::GetLayoutHistoryState()
    const {
  nsCOMPtr<nsILayoutHistoryState> state;
  if (!mScriptGlobalObject) {
    state = mLayoutHistoryState;
  } else {
    nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
    if (docShell) {
      docShell->GetLayoutHistoryState(getter_AddRefs(state));
    }
  }

  return state.forget();
}

void Document::EnsureOnloadBlocker() {
  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mOnloadBlockCount != 0 && mScriptGlobalObject) {
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (loadGroup) {
      // Check first to see if mOnloadBlocker is in the loadgroup.
      nsCOMPtr<nsISimpleEnumerator> requests;
      loadGroup->GetRequests(getter_AddRefs(requests));

      bool hasMore = false;
      while (NS_SUCCEEDED(requests->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> elem;
        requests->GetNext(getter_AddRefs(elem));
        nsCOMPtr<nsIRequest> request = do_QueryInterface(elem);
        if (request && request == mOnloadBlocker) {
          return;
        }
      }

      // Not in the loadgroup, so add it.
      loadGroup->AddRequest(mOnloadBlocker, nullptr);
    }
  }
}

void Document::AsyncBlockOnload() {
  while (mAsyncOnloadBlockCount) {
    --mAsyncOnloadBlockCount;
    BlockOnload();
  }
}

void Document::BlockOnload() {
  if (mDisplayDocument) {
    mDisplayDocument->BlockOnload();
    return;
  }

  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mOnloadBlockCount == 0 && mScriptGlobalObject) {
    if (!nsContentUtils::IsSafeToRunScript()) {
      // Because AddRequest may lead to OnStateChange calls in chrome,
      // block onload only when there are no script blockers.
      ++mAsyncOnloadBlockCount;
      if (mAsyncOnloadBlockCount == 1) {
        nsContentUtils::AddScriptRunner(NewRunnableMethod(
            "Document::AsyncBlockOnload", this, &Document::AsyncBlockOnload));
      }
      return;
    }
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (loadGroup) {
      loadGroup->AddRequest(mOnloadBlocker, nullptr);
    }
  }
  ++mOnloadBlockCount;
}

void Document::UnblockOnload(bool aFireSync) {
  if (mDisplayDocument) {
    mDisplayDocument->UnblockOnload(aFireSync);
    return;
  }

  if (mOnloadBlockCount == 0 && mAsyncOnloadBlockCount == 0) {
    MOZ_ASSERT_UNREACHABLE(
        "More UnblockOnload() calls than BlockOnload() "
        "calls; dropping call");
    return;
  }

  --mOnloadBlockCount;

  if (mOnloadBlockCount == 0) {
    if (mScriptGlobalObject) {
      // Only manipulate the loadgroup in this case, because if
      // mScriptGlobalObject is null, it's not ours.
      if (aFireSync && mAsyncOnloadBlockCount == 0) {
        // Increment mOnloadBlockCount, since DoUnblockOnload will decrement it
        ++mOnloadBlockCount;
        DoUnblockOnload();
      } else {
        PostUnblockOnloadEvent();
      }
    } else if (mIsBeingUsedAsImage) {
      // To correctly unblock onload for a document that contains an SVG
      // image, we need to know when all of the SVG document's resources are
      // done loading, in a way comparable to |window.onload|. We fire this
      // event to indicate that the SVG should be considered fully loaded.
      // Because scripting is disabled on SVG-as-image documents, this event
      // is not accessible to content authors. (See bug 837315.)
      RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
          this, NS_LITERAL_STRING("MozSVGAsImageDocumentLoad"), CanBubble::eNo,
          ChromeOnlyDispatch::eNo);
      asyncDispatcher->PostDOMEvent();
    }
  }
}

class nsUnblockOnloadEvent : public Runnable {
 public:
  explicit nsUnblockOnloadEvent(Document* aDoc)
      : mozilla::Runnable("nsUnblockOnloadEvent"), mDoc(aDoc) {}
  NS_IMETHOD Run() override {
    mDoc->DoUnblockOnload();
    return NS_OK;
  }

 private:
  RefPtr<Document> mDoc;
};

void Document::PostUnblockOnloadEvent() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> evt = new nsUnblockOnloadEvent(this);
  nsresult rv = Dispatch(TaskCategory::Other, evt.forget());
  if (NS_SUCCEEDED(rv)) {
    // Stabilize block count so we don't post more events while this one is up
    ++mOnloadBlockCount;
  } else {
    NS_WARNING("failed to dispatch nsUnblockOnloadEvent");
  }
}

void Document::DoUnblockOnload() {
  MOZ_ASSERT(!mDisplayDocument, "Shouldn't get here for resource document");
  MOZ_ASSERT(mOnloadBlockCount != 0,
             "Shouldn't have a count of zero here, since we stabilized in "
             "PostUnblockOnloadEvent");

  --mOnloadBlockCount;

  if (mOnloadBlockCount != 0) {
    // We blocked again after the last unblock.  Nothing to do here.  We'll
    // post a new event when we unblock again.
    return;
  }

  if (mAsyncOnloadBlockCount != 0) {
    // We need to wait until the async onload block has been handled.
    PostUnblockOnloadEvent();
  }

  // If mScriptGlobalObject is null, we shouldn't be messing with the loadgroup
  // -- it's not ours.
  if (mScriptGlobalObject) {
    nsCOMPtr<nsILoadGroup> loadGroup = GetDocumentLoadGroup();
    if (loadGroup) {
      loadGroup->RemoveRequest(mOnloadBlocker, nullptr, NS_OK);
    }
  }
}

nsIContent* Document::GetContentInThisDocument(nsIFrame* aFrame) const {
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    nsIContent* content = f->GetContent();
    if (!content || content->IsInAnonymousSubtree()) continue;

    if (content->OwnerDoc() == this) {
      return content;
    }
    // We must be in a subdocument so jump directly to the root frame.
    // GetParentOrPlaceholderForCrossDoc gets called immediately to jump up to
    // the containing document.
    f = f->PresContext()->GetPresShell()->GetRootFrame();
  }

  return nullptr;
}

void Document::DispatchPageTransition(EventTarget* aDispatchTarget,
                                      const nsAString& aType, bool aInFrameSwap,
                                      bool aPersisted, bool aOnlySystemGroup) {
  if (!aDispatchTarget) {
    return;
  }

  PageTransitionEventInit init;
  init.mBubbles = true;
  init.mCancelable = true;
  init.mPersisted = aPersisted;
  init.mInFrameSwap = aInFrameSwap;

  RefPtr<PageTransitionEvent> event =
      PageTransitionEvent::Constructor(this, aType, init);

  event->SetTrusted(true);
  event->SetTarget(this);
  if (aOnlySystemGroup) {
    event->WidgetEventPtr()->mFlags.mOnlySystemGroupDispatchInContent = true;
  }
  EventDispatcher::DispatchDOMEvent(aDispatchTarget, nullptr, event, nullptr,
                                    nullptr);
}

void Document::OnPageShow(bool aPersisted, EventTarget* aDispatchStartTarget,
                          bool aOnlySystemGroup) {
  const bool inFrameLoaderSwap = !!aDispatchStartTarget;
  MOZ_DIAGNOSTIC_ASSERT(
      inFrameLoaderSwap ==
      (mDocumentContainer && mDocumentContainer->InFrameSwap()));

  Element* root = GetRootElement();
  if (aPersisted && root) {
    // Send out notifications that our <link> elements are attached.
    RefPtr<nsContentList> links =
        NS_GetContentList(root, kNameSpaceID_XHTML, NS_LITERAL_STRING("link"));

    uint32_t linkCount = links->Length(true);
    for (uint32_t i = 0; i < linkCount; ++i) {
      static_cast<HTMLLinkElement*>(links->Item(i, false))->LinkAdded();
    }
  }

  // See Document
  if (!inFrameLoaderSwap) {
    if (aPersisted) {
      ImageTracker()->SetAnimatingState(true);
    }

    // Set mIsShowing before firing events, in case those event handlers
    // move us around.
    mIsShowing = true;
    mVisible = true;

    UpdateVisibilityState();
  }

  EnumerateActivityObservers(NotifyActivityChanged);

  auto notifyExternal = [aPersisted](Document& aExternalResource) {
    aExternalResource.OnPageShow(aPersisted, nullptr);
    return CallState::Continue;
  };
  EnumerateExternalResources(notifyExternal);

  if (mAnimationController) {
    mAnimationController->OnPageShow();
  }

  if (!mIsBeingUsedAsImage) {
    // Dispatch observer notification to notify observers page is shown.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      nsIPrincipal* principal = NodePrincipal();
      os->NotifyObservers(ToSupports(this),
                          principal->IsSystemPrincipal() ? "chrome-page-shown"
                                                         : "content-page-shown",
                          nullptr);
    }

    nsCOMPtr<EventTarget> target = aDispatchStartTarget;
    if (!target) {
      target = do_QueryInterface(GetWindow());
    }
    DispatchPageTransition(target, NS_LITERAL_STRING("pageshow"),
                           inFrameLoaderSwap, aPersisted, aOnlySystemGroup);
  }
}

static void DispatchFullscreenChange(Document& aDocument, nsINode* aTarget) {
  if (nsPresContext* presContext = aDocument.GetPresContext()) {
    auto pendingEvent = MakeUnique<PendingFullscreenEvent>(
        FullscreenEventType::Change, &aDocument, aTarget);
    presContext->RefreshDriver()->ScheduleFullscreenEvent(
        std::move(pendingEvent));
  }
}

static void ClearPendingFullscreenRequests(Document* aDoc);

void Document::OnPageHide(bool aPersisted, EventTarget* aDispatchStartTarget,
                          bool aOnlySystemGroup) {
  const bool inFrameLoaderSwap = !!aDispatchStartTarget;
  MOZ_DIAGNOSTIC_ASSERT(
      inFrameLoaderSwap ==
      (mDocumentContainer && mDocumentContainer->InFrameSwap()));

  // Send out notifications that our <link> elements are detached,
  // but only if this is not a full unload.
  Element* root = GetRootElement();
  if (aPersisted && root) {
    RefPtr<nsContentList> links =
        NS_GetContentList(root, kNameSpaceID_XHTML, NS_LITERAL_STRING("link"));

    uint32_t linkCount = links->Length(true);
    for (uint32_t i = 0; i < linkCount; ++i) {
      static_cast<HTMLLinkElement*>(links->Item(i, false))->LinkRemoved();
    }
  }

  if (mAnimationController) {
    mAnimationController->OnPageHide();
  }

  if (!inFrameLoaderSwap) {
    if (aPersisted) {
      // We do not stop the animations (bug 1024343) when the page is refreshing
      // while being dragged out.
      ImageTracker()->SetAnimatingState(false);
    }

    // Set mIsShowing before firing events, in case those event handlers
    // move us around.
    mIsShowing = false;
    mVisible = false;
  }

  ExitPointerLock();

  if (!mIsBeingUsedAsImage) {
    // Dispatch observer notification to notify observers page is hidden.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      nsIPrincipal* principal = NodePrincipal();
      os->NotifyObservers(ToSupports(this),
                          principal->IsSystemPrincipal()
                              ? "chrome-page-hidden"
                              : "content-page-hidden",
                          nullptr);
    }

    // Now send out a PageHide event.
    nsCOMPtr<EventTarget> target = aDispatchStartTarget;
    if (!target) {
      target = do_QueryInterface(GetWindow());
    }
    {
      PageUnloadingEventTimeStamp timeStamp(this);
      DispatchPageTransition(target, NS_LITERAL_STRING("pagehide"),
                             inFrameLoaderSwap, aPersisted, aOnlySystemGroup);
    }
  }

  if (!inFrameLoaderSwap) {
    UpdateVisibilityState();
  }

  auto notifyExternal = [aPersisted](Document& aExternalResource) {
    aExternalResource.OnPageHide(aPersisted, nullptr);
    return CallState::Continue;
  };
  EnumerateExternalResources(notifyExternal);
  EnumerateActivityObservers(NotifyActivityChanged);

  ClearPendingFullscreenRequests(this);
  if (FullscreenStackTop()) {
    // If this document was fullscreen, we should exit fullscreen in this
    // doctree branch. This ensures that if the user navigates while in
    // fullscreen mode we don't leave its still visible ancestor documents
    // in fullscreen mode. So exit fullscreen in the document's fullscreen
    // root document, as this will exit fullscreen in all the root's
    // descendant documents. Note that documents are removed from the
    // doctree by the time OnPageHide() is called, so we must store a
    // reference to the root (in Document::mFullscreenRoot) since we can't
    // just traverse the doctree to get the root.
    Document::ExitFullscreenInDocTree(this);

    // Since the document is removed from the doctree before OnPageHide() is
    // called, ExitFullscreen() can't traverse from the root down to *this*
    // document, so we must manually call CleanupFullscreenState() below too.
    // Note that CleanupFullscreenState() clears Document::mFullscreenRoot,
    // so we *must* call it after ExitFullscreen(), not before.
    // OnPageHide() is called in every hidden (i.e. descendant) document,
    // so calling CleanupFullscreenState() here will ensure all hidden
    // documents have their fullscreen state reset.
    CleanupFullscreenState();

    // The fullscreenchange event is to be queued in the refresh driver,
    // however a hidden page wouldn't trigger that again, so it makes no
    // sense to dispatch such event here.
  }
}

void Document::WillDispatchMutationEvent(nsINode* aTarget) {
  NS_ASSERTION(
      mSubtreeModifiedDepth != 0 || mSubtreeModifiedTargets.Count() == 0,
      "mSubtreeModifiedTargets not cleared after dispatching?");
  ++mSubtreeModifiedDepth;
  if (aTarget) {
    // MayDispatchMutationEvent is often called just before this method,
    // so it has already appended the node to mSubtreeModifiedTargets.
    int32_t count = mSubtreeModifiedTargets.Count();
    if (!count || mSubtreeModifiedTargets[count - 1] != aTarget) {
      mSubtreeModifiedTargets.AppendObject(aTarget);
    }
  }
}

void Document::MutationEventDispatched(nsINode* aTarget) {
  --mSubtreeModifiedDepth;
  if (mSubtreeModifiedDepth == 0) {
    int32_t count = mSubtreeModifiedTargets.Count();
    if (!count) {
      return;
    }

    nsPIDOMWindowInner* window = GetInnerWindow();
    if (window &&
        !window->HasMutationListeners(NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED)) {
      mSubtreeModifiedTargets.Clear();
      return;
    }

    nsCOMArray<nsINode> realTargets;
    for (int32_t i = 0; i < count; ++i) {
      nsINode* possibleTarget = mSubtreeModifiedTargets[i];
      nsCOMPtr<nsIContent> content = do_QueryInterface(possibleTarget);
      if (content && content->ChromeOnlyAccess()) {
        continue;
      }

      nsINode* commonAncestor = nullptr;
      int32_t realTargetCount = realTargets.Count();
      for (int32_t j = 0; j < realTargetCount; ++j) {
        commonAncestor = nsContentUtils::GetClosestCommonInclusiveAncestor(
            possibleTarget, realTargets[j]);
        if (commonAncestor) {
          realTargets.ReplaceObjectAt(commonAncestor, j);
          break;
        }
      }
      if (!commonAncestor) {
        realTargets.AppendObject(possibleTarget);
      }
    }

    mSubtreeModifiedTargets.Clear();

    int32_t realTargetCount = realTargets.Count();
    for (int32_t k = 0; k < realTargetCount; ++k) {
      InternalMutationEvent mutation(true, eLegacySubtreeModified);
      (new AsyncEventDispatcher(realTargets[k], mutation))
          ->RunDOMEventWhenSafe();
    }
  }
}

void Document::DestroyElementMaps() {
#ifdef DEBUG
  mStyledLinksCleared = true;
#endif
  mStyledLinks.Clear();
  // Notify ID change listeners before clearing the identifier map.
  for (auto iter = mIdentifierMap.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->ClearAndNotify();
  }
  mIdentifierMap.Clear();
  mComposedShadowRoots.Clear();
  mResponsiveContent.Clear();
  IncrementExpandoGeneration(*this);
}

void Document::RefreshLinkHrefs() {
  // Get a list of all links we know about.  We will reset them, which will
  // remove them from the document, so we need a copy of what is in the
  // hashtable.
  LinkArray linksToNotify(mStyledLinks.Count());
  for (auto iter = mStyledLinks.ConstIter(); !iter.Done(); iter.Next()) {
    linksToNotify.AppendElement(iter.Get()->GetKey());
  }

  // Reset all of our styled links.
  nsAutoScriptBlocker scriptBlocker;
  for (LinkArray::size_type i = 0; i < linksToNotify.Length(); i++) {
    linksToNotify[i]->ResetLinkState(true, linksToNotify[i]->ElementHasHref());
  }
}

nsresult Document::CloneDocHelper(Document* clone) const {
  clone->mIsStaticDocument = mCreatingStaticClone;

  // Init document
  nsresult rv = clone->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCreatingStaticClone) {
    nsCOMPtr<nsILoadGroup> loadGroup;

    // |mDocumentContainer| is the container of the document that is being
    // created and not the original container. See CreateStaticClone function().
    nsCOMPtr<nsIDocumentLoader> docLoader(mDocumentContainer);
    if (docLoader) {
      docLoader->GetLoadGroup(getter_AddRefs(loadGroup));
    }
    nsCOMPtr<nsIChannel> channel = GetChannel();
    nsCOMPtr<nsIURI> uri;
    if (channel) {
      NS_GetFinalChannelURI(channel, getter_AddRefs(uri));
    } else {
      uri = Document::GetDocumentURI();
    }
    clone->mChannel = channel;
    if (uri) {
      clone->ResetToURI(uri, loadGroup, NodePrincipal(),
                        EffectiveStoragePrincipal());
    }

    clone->SetContainer(mDocumentContainer);
  }

  // Now ensure that our clone has the same URI, base URI, and principal as us.
  // We do this after the mCreatingStaticClone block above, because that block
  // can set the base URI to an incorrect value in cases when base URI
  // information came from the channel.  So we override explicitly, and do it
  // for all these properties, in case ResetToURI messes with any of the rest of
  // them.
  clone->SetDocumentURI(Document::GetDocumentURI());
  clone->SetChromeXHRDocURI(mChromeXHRDocURI);
  clone->SetPrincipals(NodePrincipal(), EffectiveStoragePrincipal());
  clone->mDocumentBaseURI = mDocumentBaseURI;
  clone->SetChromeXHRDocBaseURI(mChromeXHRDocBaseURI);
  clone->mReferrerInfo =
      static_cast<dom::ReferrerInfo*>(mReferrerInfo.get())->Clone();
  clone->mPreloadReferrerInfo = clone->mReferrerInfo;

  bool hasHadScriptObject = true;
  nsIScriptGlobalObject* scriptObject =
      GetScriptHandlingObject(hasHadScriptObject);
  NS_ENSURE_STATE(scriptObject || !hasHadScriptObject);
  if (mCreatingStaticClone) {
    // If we're doing a static clone (print, print preview), then we're going to
    // be setting a scope object after the clone. It's better to set it only
    // once, so we don't do that here. However, we do want to act as if there is
    // a script handling object. So we set mHasHadScriptHandlingObject.
    clone->mHasHadScriptHandlingObject = true;
  } else if (scriptObject) {
    clone->SetScriptHandlingObject(scriptObject);
  } else {
    clone->SetScopeObject(GetScopeObject());
  }
  // Make the clone a data document
  clone->SetLoadedAsData(true);

  // Misc state

  // State from Document
  clone->mCharacterSet = mCharacterSet;
  clone->mCharacterSetSource = mCharacterSetSource;
  clone->SetCompatibilityMode(mCompatMode);
  clone->mBidiOptions = mBidiOptions;
  clone->mContentLanguage = mContentLanguage;
  clone->SetContentTypeInternal(GetContentTypeInternal());
  clone->mSecurityInfo = mSecurityInfo;

  // State from Document
  clone->mType = mType;
  clone->mXMLDeclarationBits = mXMLDeclarationBits;
  clone->mBaseTarget = mBaseTarget;

  return NS_OK;
}

void Document::NotifyLoading(bool aNewParentIsLoading,
                             const ReadyState& aCurrentState,
                             ReadyState aNewState) {
  // Mirror the top-level loading state down to all subdocuments
  bool was_loading = mAncestorIsLoading ||
                     aCurrentState == READYSTATE_LOADING ||
                     aCurrentState == READYSTATE_INTERACTIVE;
  bool is_loading = aNewParentIsLoading || aNewState == READYSTATE_LOADING ||
                    aNewState == READYSTATE_INTERACTIVE;  // new value for state
  bool set_load_state = was_loading != is_loading;

  MOZ_LOG(
      gTimeoutDeferralLog, mozilla::LogLevel::Debug,
      ("NotifyLoading for doc %p: currentAncestor: %d, newParent: %d, "
       "currentState %d newState: %d, was_loading: %d, is_loading: %d, "
       "set_load_state: %d",
       (void*)this, mAncestorIsLoading, aNewParentIsLoading, (int)aCurrentState,
       (int)aNewState, was_loading, is_loading, set_load_state));

  mAncestorIsLoading = aNewParentIsLoading;
  if (set_load_state && StaticPrefs::dom_timeout_defer_during_load()) {
    // Tell our innerwindow (and thus TimeoutManager)
    nsPIDOMWindowInner* inner = GetInnerWindow();
    if (inner) {
      inner->SetActiveLoadingState(is_loading);
    }
    BrowsingContext* context = GetBrowsingContext();
    if (context) {
      // Don't use PreOrderWalk to mirror this down; go down one level as a
      // time so we can set mAncestorIsLoading and take into account the
      // readystates of the subdocument.  In the child process it will call
      // NotifyLoading() to notify the innerwindow/TimeoutManager, and then
      // iterate it's children
      for (auto& child : context->GetChildren()) {
        MOZ_LOG(gTimeoutDeferralLog, mozilla::LogLevel::Debug,
                ("bc: %p SetAncestorLoading(%d)", (void*)child, is_loading));
        child->SetAncestorLoading(is_loading);
      }
    }
  }
}

void Document::SetReadyStateInternal(ReadyState aReadyState,
                                     bool aUpdateTimingInformation) {
  if (aReadyState == READYSTATE_UNINITIALIZED) {
    // Transition back to uninitialized happens only to keep assertions happy
    // right before readyState transitions to something else. Make this
    // transition undetectable by Web content.
    mReadyState = aReadyState;
    return;
  }

  if (IsTopLevelContentDocument()) {
    if (aReadyState == READYSTATE_LOADING) {
      AddToplevelLoadingDocument(this);
    } else if (aReadyState == READYSTATE_COMPLETE) {
      RemoveToplevelLoadingDocument(this);
    }
  }

  if (aUpdateTimingInformation && READYSTATE_LOADING == aReadyState) {
    mLoadingTimeStamp = TimeStamp::Now();
  }
  NotifyLoading(mAncestorIsLoading, mReadyState, aReadyState);
  mReadyState = aReadyState;
  if (aUpdateTimingInformation && mTiming) {
    switch (aReadyState) {
      case READYSTATE_LOADING:
        mTiming->NotifyDOMLoading(GetDocumentURI());
        break;
      case READYSTATE_INTERACTIVE:
        mTiming->NotifyDOMInteractive(GetDocumentURI());
        break;
      case READYSTATE_COMPLETE:
        mTiming->NotifyDOMComplete(GetDocumentURI());
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected ReadyState value");
        break;
    }
  }
  // At the time of loading start, we don't have timing object, record time.

  if (READYSTATE_INTERACTIVE == aReadyState &&
      NodePrincipal()->IsSystemPrincipal()) {
    if (!mXULPersist) {
      mXULPersist = new XULPersist(this);
      mXULPersist->Init();
    }
    if (!mChromeObserver) {
      mChromeObserver = new ChromeObserver(this);
      mChromeObserver->Init();
    }
  }

  if (aUpdateTimingInformation) {
    RecordNavigationTiming(aReadyState);
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, NS_LITERAL_STRING("readystatechange"),
                               CanBubble::eNo, ChromeOnlyDispatch::eNo);
  asyncDispatcher->RunDOMEventWhenSafe();
}

void Document::GetReadyState(nsAString& aReadyState) const {
  switch (mReadyState) {
    case READYSTATE_LOADING:
      aReadyState.AssignLiteral(u"loading");
      break;
    case READYSTATE_INTERACTIVE:
      aReadyState.AssignLiteral(u"interactive");
      break;
    case READYSTATE_COMPLETE:
      aReadyState.AssignLiteral(u"complete");
      break;
    default:
      aReadyState.AssignLiteral(u"uninitialized");
  }
}

void Document::SuppressEventHandling(uint32_t aIncrease) {
  mEventsSuppressed += aIncrease;
  UpdateFrameRequestCallbackSchedulingState();
  for (uint32_t i = 0; i < aIncrease; ++i) {
    ScriptLoader()->AddExecuteBlocker();
  }

  auto suppressInSubDoc = [aIncrease](Document& aSubDoc) {
    aSubDoc.SuppressEventHandling(aIncrease);
    return CallState::Continue;
  };

  EnumerateSubDocuments(suppressInSubDoc);
}

static void FireOrClearDelayedEvents(nsTArray<nsCOMPtr<Document>>& aDocuments,
                                     bool aFireEvents) {
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) return;

  for (uint32_t i = 0; i < aDocuments.Length(); ++i) {
    // NB: Don't bother trying to fire delayed events on documents that were
    // closed before this event ran.
    if (!aDocuments[i]->EventHandlingSuppressed()) {
      fm->FireDelayedEvents(aDocuments[i]);
      RefPtr<PresShell> presShell = aDocuments[i]->GetPresShell();
      if (presShell) {
        // Only fire events for active documents.
        bool fire = aFireEvents && aDocuments[i]->GetInnerWindow() &&
                    aDocuments[i]->GetInnerWindow()->IsCurrentInnerWindow();
        presShell->FireOrClearDelayedEvents(fire);
      }
      aDocuments[i]->FireOrClearPostMessageEvents(aFireEvents);
    }
  }
}

void Document::PreloadPictureClosed() {
  MOZ_ASSERT(mPreloadPictureDepth > 0);
  mPreloadPictureDepth--;
  if (mPreloadPictureDepth == 0) {
    mPreloadPictureFoundSource.SetIsVoid(true);
  }
}

void Document::PreloadPictureImageSource(const nsAString& aSrcsetAttr,
                                         const nsAString& aSizesAttr,
                                         const nsAString& aTypeAttr,
                                         const nsAString& aMediaAttr) {
  // Nested pictures are not valid syntax, so while we'll eventually load them,
  // it's not worth tracking sources mixed between nesting levels to preload
  // them effectively.
  if (mPreloadPictureDepth == 1 && mPreloadPictureFoundSource.IsVoid()) {
    // <picture> selects the first matching source, so if this returns a URI we
    // needn't consider new sources until a new <picture> is encountered.
    bool found = HTMLImageElement::SelectSourceForTagWithAttrs(
        this, true, VoidString(), aSrcsetAttr, aSizesAttr, aTypeAttr,
        aMediaAttr, mPreloadPictureFoundSource);
    if (found && mPreloadPictureFoundSource.IsVoid()) {
      // Found an empty source, which counts
      mPreloadPictureFoundSource.SetIsVoid(false);
    }
  }
}

already_AddRefed<nsIURI> Document::ResolvePreloadImage(
    nsIURI* aBaseURI, const nsAString& aSrcAttr, const nsAString& aSrcsetAttr,
    const nsAString& aSizesAttr, bool* aIsImgSet) {
  nsString sourceURL;
  bool isImgSet;
  if (mPreloadPictureDepth == 1 && !mPreloadPictureFoundSource.IsVoid()) {
    // We're in a <picture> element and found a URI from a source previous to
    // this image, use it.
    sourceURL = mPreloadPictureFoundSource;
    isImgSet = true;
  } else {
    // Otherwise try to use this <img> as a source
    HTMLImageElement::SelectSourceForTagWithAttrs(
        this, false, aSrcAttr, aSrcsetAttr, aSizesAttr, VoidString(),
        VoidString(), sourceURL);
    isImgSet = !aSrcsetAttr.IsEmpty();
  }

  // Empty sources are not loaded by <img> (i.e. not resolved to the baseURI)
  if (sourceURL.IsEmpty()) {
    return nullptr;
  }

  // Construct into URI using passed baseURI (the parser may know of base URI
  // changes that have not reached us)
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri), sourceURL,
                                                 this, aBaseURI);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  *aIsImgSet = isImgSet;

  // We don't clear mPreloadPictureFoundSource because subsequent <img> tags in
  // this this <picture> share the same <sources> (though this is not valid per
  // spec)
  return uri.forget();
}

void Document::MaybePreLoadImage(nsIURI* uri, const nsAString& aCrossOriginAttr,
                                 enum ReferrerPolicy aReferrerPolicy,
                                 bool aIsImgSet) {
  // Early exit if the img is already present in the img-cache
  // which indicates that the "real" load has already started and
  // that we shouldn't preload it.
  if (nsContentUtils::IsImageInCache(uri, this)) {
    return;
  }

  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL |
                          nsContentUtils::CORSModeToLoadImageFlags(
                              Element::StringToCORSMode(aCrossOriginAttr));

  nsContentPolicyType policyType =
      aIsImgSet ? nsIContentPolicy::TYPE_IMAGESET
                : nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD;

  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      ReferrerInfo::CreateFromDocumentAndPolicyOverride(this, aReferrerPolicy);

  // Image not in cache - trigger preload
  RefPtr<imgRequestProxy> request;
  nsresult rv = nsContentUtils::LoadImage(
      uri, static_cast<nsINode*>(this), this, NodePrincipal(), 0, referrerInfo,
      nullptr,  // no observer
      loadFlags, NS_LITERAL_STRING("img"), getter_AddRefs(request), policyType);

  // Pin image-reference to avoid evicting it from the img-cache before
  // the "real" load occurs. Unpinned in DispatchContentLoadedEvents and
  // unlink
  if (NS_SUCCEEDED(rv)) {
    mPreloadingImages.Put(uri, std::move(request));
  }
}

void Document::MaybePreconnect(nsIURI* aOrigURI, mozilla::CORSMode aCORSMode) {
  NS_MutateURI mutator(aOrigURI);
  if (NS_FAILED(mutator.GetStatus())) {
    return;
  }

  // The URI created here is used in 2 contexts. One is nsISpeculativeConnect
  // which ignores the path and uses only the origin. The other is for the
  // document mPreloadedPreconnects de-duplication hash. Anonymous vs
  // non-Anonymous preconnects create different connections on the wire and
  // therefore should not be considred duplicates of each other and we
  // normalize the path before putting it in the hash to accomplish that.

  if (aCORSMode == CORS_ANONYMOUS) {
    mutator.SetPathQueryRef(NS_LITERAL_CSTRING("/anonymous"));
  } else {
    mutator.SetPathQueryRef(NS_LITERAL_CSTRING("/"));
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = mutator.Finalize(uri);
  if (NS_FAILED(rv)) {
    return;
  }

  auto entry = mPreloadedPreconnects.LookupForAdd(uri);
  if (entry) {
    return;  // we found an existing entry
  }
  entry.OrInsert([]() { return true; });

  nsCOMPtr<nsISpeculativeConnect> speculator(
      do_QueryInterface(nsContentUtils::GetIOService()));
  if (!speculator) {
    return;
  }

  if (aCORSMode == CORS_ANONYMOUS) {
    speculator->SpeculativeAnonymousConnect(uri, NodePrincipal(), nullptr);
  } else {
    speculator->SpeculativeConnect(uri, NodePrincipal(), nullptr);
  }
}

void Document::ForgetImagePreload(nsIURI* aURI) {
  // Checking count is faster than hashing the URI in the common
  // case of empty table.
  if (mPreloadingImages.Count() != 0) {
    nsCOMPtr<imgIRequest> req;
    mPreloadingImages.Remove(aURI, getter_AddRefs(req));
    if (req) {
      // Make sure to cancel the request so imagelib knows it's gone.
      req->CancelAndForgetObserver(NS_BINDING_ABORTED);
    }
  }
}

void Document::UpdateDocumentStates(EventStates aMaybeChangedStates,
                                    bool aNotify) {
  EventStates oldStates = mDocumentState;
  if (aMaybeChangedStates.HasState(NS_DOCUMENT_STATE_RTL_LOCALE)) {
    if (IsDocumentRightToLeft()) {
      mDocumentState |= NS_DOCUMENT_STATE_RTL_LOCALE;
    } else {
      mDocumentState &= ~NS_DOCUMENT_STATE_RTL_LOCALE;
    }
  }

  if (aMaybeChangedStates.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    if (IsTopLevelWindowInactive()) {
      mDocumentState |= NS_DOCUMENT_STATE_WINDOW_INACTIVE;
    } else {
      mDocumentState &= ~NS_DOCUMENT_STATE_WINDOW_INACTIVE;
    }
  }

  EventStates changedStates = oldStates ^ mDocumentState;
  if (aNotify && !changedStates.IsEmpty()) {
    if (PresShell* ps = GetObservingPresShell()) {
      ps->DocumentStatesChanged(changedStates);
    }
  }
}

namespace {

/**
 * Stub for LoadSheet(), since all we want is to get the sheet into
 * the CSSLoader's style cache
 */
class StubCSSLoaderObserver final : public nsICSSLoaderObserver {
  ~StubCSSLoaderObserver() = default;

 public:
  NS_IMETHOD
  StyleSheetLoaded(StyleSheet*, bool, nsresult) override { return NS_OK; }
  NS_DECL_ISUPPORTS
};
NS_IMPL_ISUPPORTS(StubCSSLoaderObserver, nsICSSLoaderObserver)

}  // namespace

void Document::PreloadStyle(nsIURI* uri, const Encoding* aEncoding,
                            const nsAString& aCrossOriginAttr,
                            const enum ReferrerPolicy aReferrerPolicy,
                            const nsAString& aIntegrity, bool aIsLinkPreload) {
  // The CSSLoader will retain this object after we return.
  nsCOMPtr<nsICSSLoaderObserver> obs = new StubCSSLoaderObserver();

  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      ReferrerInfo::CreateFromDocumentAndPolicyOverride(this, aReferrerPolicy);

  auto preloadType = aIsLinkPreload ? css::Loader::IsPreload::FromLink
                                    : css::Loader::IsPreload::FromParser;

  // Charset names are always ASCII.
  Unused << CSSLoader()->LoadSheet(
      uri, preloadType, NodePrincipal(), aEncoding, referrerInfo, obs,
      Element::StringToCORSMode(aCrossOriginAttr), aIntegrity);
}

RefPtr<StyleSheet> Document::LoadChromeSheetSync(nsIURI* uri) {
  return CSSLoader()
      ->LoadSheetSync(uri, css::eAuthorSheetFeatures)
      .unwrapOr(nullptr);
}

void Document::ResetDocumentDirection() {
  if (!nsContentUtils::IsChromeDoc(this)) {
    return;
  }
  UpdateDocumentStates(NS_DOCUMENT_STATE_RTL_LOCALE, true);
}

bool Document::IsDocumentRightToLeft() {
  if (!nsContentUtils::IsChromeDoc(this)) {
    return false;
  }
  // setting the localedir attribute on the root element forces a
  // specific direction for the document.
  Element* element = GetRootElement();
  if (element) {
    static Element::AttrValuesArray strings[] = {nsGkAtoms::ltr, nsGkAtoms::rtl,
                                                 nullptr};
    switch (element->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::localedir,
                                     strings, eCaseMatters)) {
      case 0:
        return false;
      case 1:
        return true;
      default:
        break;  // otherwise, not a valid value, so fall through
    }
  }

  if (!mDocumentURI->SchemeIs("chrome") && !mDocumentURI->SchemeIs("about") &&
      !mDocumentURI->SchemeIs("resource")) {
    return false;
  }

  return intl::LocaleService::GetInstance()->IsAppLocaleRTL();
}

class nsDelayedEventDispatcher : public Runnable {
 public:
  explicit nsDelayedEventDispatcher(nsTArray<nsCOMPtr<Document>>& aDocuments)
      : mozilla::Runnable("nsDelayedEventDispatcher") {
    mDocuments.SwapElements(aDocuments);
  }
  virtual ~nsDelayedEventDispatcher() = default;

  NS_IMETHOD Run() override {
    FireOrClearDelayedEvents(mDocuments, true);
    return NS_OK;
  }

 private:
  nsTArray<nsCOMPtr<Document>> mDocuments;
};

static void GetAndUnsuppressSubDocuments(
    Document& aDocument, nsTArray<nsCOMPtr<Document>>& aDocuments) {
  if (aDocument.EventHandlingSuppressed() > 0) {
    aDocument.DecreaseEventSuppression();
    aDocument.ScriptLoader()->RemoveExecuteBlocker();
  }
  aDocuments.AppendElement(&aDocument);
  auto recurse = [&aDocuments](Document& aSubDoc) {
    GetAndUnsuppressSubDocuments(aSubDoc, aDocuments);
    return CallState::Continue;
  };
  aDocument.EnumerateSubDocuments(recurse);
}

void Document::UnsuppressEventHandlingAndFireEvents(bool aFireEvents) {
  nsTArray<nsCOMPtr<Document>> documents;
  GetAndUnsuppressSubDocuments(*this, documents);

  if (aFireEvents) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIRunnable> ded = new nsDelayedEventDispatcher(documents);
    Dispatch(TaskCategory::Other, ded.forget());
  } else {
    FireOrClearDelayedEvents(documents, false);
  }

  if (!EventHandlingSuppressed()) {
    MOZ_ASSERT(NS_IsMainThread());
    nsTArray<RefPtr<net::ChannelEventQueue>> queues;
    mSuspendedQueues.SwapElements(queues);
    for (net::ChannelEventQueue* queue : queues) {
      queue->Resume();
    }

    // If there have been any events driven by the refresh driver which were
    // delayed due to events being suppressed in this document, make sure there
    // is a refresh scheduled soon so the events will run.
    if (mHasDelayedRefreshEvent) {
      mHasDelayedRefreshEvent = false;

      if (mPresShell) {
        nsRefreshDriver* rd = mPresShell->GetPresContext()->RefreshDriver();
        rd->RunDelayedEventsSoon();
      }
    }
  }
}

void Document::AddSuspendedChannelEventQueue(net::ChannelEventQueue* aQueue) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(EventHandlingSuppressed());
  mSuspendedQueues.AppendElement(aQueue);
}

bool Document::SuspendPostMessageEvent(PostMessageEvent* aEvent) {
  MOZ_ASSERT(NS_IsMainThread());

  if (EventHandlingSuppressed() || !mSuspendedPostMessageEvents.IsEmpty()) {
    mSuspendedPostMessageEvents.AppendElement(aEvent);
    return true;
  }
  return false;
}

void Document::FireOrClearPostMessageEvents(bool aFireEvents) {
  nsTArray<RefPtr<PostMessageEvent>> events;
  events.SwapElements(mSuspendedPostMessageEvents);

  if (aFireEvents) {
    for (PostMessageEvent* event : events) {
      event->Run();
    }
  }
}

void Document::SetSuppressedEventListener(EventListener* aListener) {
  mSuppressedEventListener = aListener;
  auto setOnSubDocs = [&](Document& aDocument) {
    aDocument.SetSuppressedEventListener(aListener);
    return CallState::Continue;
  };
  EnumerateSubDocuments(setOnSubDocs);
}

nsISupports* Document::GetCurrentContentSink() {
  return mParser ? mParser->GetContentSink() : nullptr;
}

Document* Document::GetTemplateContentsOwner() {
  if (!mTemplateContentsOwner) {
    bool hasHadScriptObject = true;
    nsIScriptGlobalObject* scriptObject =
        GetScriptHandlingObject(hasHadScriptObject);

    nsCOMPtr<Document> document;
    nsresult rv = NS_NewDOMDocument(getter_AddRefs(document),
                                    EmptyString(),  // aNamespaceURI
                                    EmptyString(),  // aQualifiedName
                                    nullptr,        // aDoctype
                                    Document::GetDocumentURI(),
                                    Document::GetDocBaseURI(), NodePrincipal(),
                                    true,          // aLoadedAsData
                                    scriptObject,  // aEventObject
                                    DocumentFlavorHTML);
    NS_ENSURE_SUCCESS(rv, nullptr);

    mTemplateContentsOwner = document;
    NS_ENSURE_TRUE(mTemplateContentsOwner, nullptr);

    if (!scriptObject) {
      mTemplateContentsOwner->SetScopeObject(GetScopeObject());
    }

    mTemplateContentsOwner->mHasHadScriptHandlingObject = hasHadScriptObject;

    // Set |mTemplateContentsOwner| as the template contents owner of itself so
    // that it is the template contents owner of nested template elements.
    mTemplateContentsOwner->mTemplateContentsOwner = mTemplateContentsOwner;
  }

  return mTemplateContentsOwner;
}

static already_AddRefed<nsPIDOMWindowOuter> FindTopWindowForElement(
    Element* element) {
  Document* document = element->OwnerDoc();
  if (!document) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
  if (!window) {
    return nullptr;
  }

  // Trying to find the top window (equivalent to window.top).
  if (nsCOMPtr<nsPIDOMWindowOuter> top = window->GetInProcessTop()) {
    window = std::move(top);
  }
  return window.forget();
}

/**
 * nsAutoFocusEvent is used to dispatch a focus event for an
 * nsGenericHTMLFormElement with the autofocus attribute enabled.
 */
class nsAutoFocusEvent : public Runnable {
 public:
  explicit nsAutoFocusEvent(nsCOMPtr<Element>&& aElement,
                            nsCOMPtr<nsPIDOMWindowOuter>&& aTopWindow)
      : mozilla::Runnable("nsAutoFocusEvent"),
        mElement(std::move(aElement)),
        mTopWindow(std::move(aTopWindow)) {}

  NS_IMETHOD Run() override {
    nsCOMPtr<nsPIDOMWindowOuter> currentTopWindow =
        FindTopWindowForElement(mElement);
    if (currentTopWindow != mTopWindow) {
      // The element's top window changed from when the event was queued.
      // Don't take away focus from an unrelated window.
      return NS_OK;
    }

    // Don't steal focus from the user.
    if (mTopWindow->GetFocusedElement()) {
      return NS_OK;
    }

    FocusOptions options;
    ErrorResult rv;
    mElement->Focus(options, CallerType::System, rv);
    return rv.StealNSResult();
  }

 private:
  nsCOMPtr<Element> mElement;
  nsCOMPtr<nsPIDOMWindowOuter> mTopWindow;
};

void Document::SetAutoFocusElement(Element* aAutoFocusElement) {
  if (mAutoFocusFired) {
    // Too late.
    return;
  }

  if (mAutoFocusElement) {
    // The spec disallows multiple autofocus elements, so we consider only the
    // first one to preserve the old behavior.
    return;
  }

  mAutoFocusElement = do_GetWeakReference(aAutoFocusElement);
  TriggerAutoFocus();
}

void Document::TriggerAutoFocus() {
  if (mAutoFocusFired) {
    return;
  }

  if (!mPresShell || !mPresShell->DidInitialize()) {
    // Delay autofocus until frames are constructed so that we don't thrash
    // style and layout calculations.
    return;
  }

  nsCOMPtr<Element> autoFocusElement = do_QueryReferent(mAutoFocusElement);
  if (autoFocusElement && autoFocusElement->OwnerDoc() == this) {
    mAutoFocusFired = true;

    nsCOMPtr<nsPIDOMWindowOuter> topWindow =
        FindTopWindowForElement(autoFocusElement);
    if (!topWindow) {
      return;
    }

    // NOTE: This may be removed in the future since the spec technically
    // allows autofocus after load.
    nsCOMPtr<Document> topDoc = topWindow->GetExtantDoc();
    if (topDoc &&
        topDoc->GetReadyStateEnum() == Document::READYSTATE_COMPLETE) {
      return;
    }

    nsCOMPtr<nsIRunnable> event =
        new nsAutoFocusEvent(std::move(autoFocusElement), topWindow.forget());
    nsresult rv = NS_DispatchToCurrentThread(event.forget());
    NS_ENSURE_SUCCESS_VOID(rv);
  }
}

void Document::SetScrollToRef(nsIURI* aDocumentURI) {
  if (!aDocumentURI) {
    return;
  }

  nsAutoCString ref;

  // Since all URI's that pass through here aren't URL's we can't
  // rely on the nsIURI implementation for providing a way for
  // finding the 'ref' part of the URI, we'll haveto revert to
  // string routines for finding the data past '#'

  nsresult rv = aDocumentURI->GetSpec(ref);
  if (NS_FAILED(rv)) {
    Unused << aDocumentURI->GetRef(mScrollToRef);
    return;
  }

  nsReadingIterator<char> start, end;

  ref.BeginReading(start);
  ref.EndReading(end);

  if (FindCharInReadable('#', start, end)) {
    ++start;  // Skip over the '#'

    mScrollToRef = Substring(start, end);
  }
}

void Document::ScrollToRef() {
  if (mScrolledToRefAlready) {
    RefPtr<PresShell> presShell = GetPresShell();
    if (presShell) {
      presShell->ScrollToAnchor();
    }
    return;
  }

  if (mScrollToRef.IsEmpty()) {
    return;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (presShell) {
    nsresult rv = NS_ERROR_FAILURE;
    // We assume that the bytes are in UTF-8, as it says in the spec:
    // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
    NS_ConvertUTF8toUTF16 ref(mScrollToRef);
    // Check an empty string which might be caused by the UTF-8 conversion
    if (!ref.IsEmpty()) {
      // Note that GoToAnchor will handle flushing layout as needed.
      rv = presShell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
    } else {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_FAILED(rv)) {
      nsAutoCString buff;
      const bool unescaped =
          NS_UnescapeURL(mScrollToRef.BeginReading(), mScrollToRef.Length(),
                         /*aFlags =*/0, buff);

      // This attempt is only necessary if characters were unescaped.
      if (unescaped) {
        NS_ConvertUTF8toUTF16 utf16Str(buff);
        if (!utf16Str.IsEmpty()) {
          rv = presShell->GoToAnchor(utf16Str,
                                     mChangeScrollPosWhenScrollingToRef);
        }
      }

      // If UTF-8 URI failed then try to assume the string as a
      // document's charset.
      if (NS_FAILED(rv)) {
        const Encoding* encoding = GetDocumentCharacterSet();
        rv = encoding->DecodeWithoutBOMHandling(unescaped ? buff : mScrollToRef,
                                                ref);
        if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
          rv = presShell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
        }
      }
    }
    if (NS_SUCCEEDED(rv)) {
      mScrolledToRefAlready = true;
    }
  }
}

void Document::RegisterActivityObserver(nsISupports* aSupports) {
  if (!mActivityObservers) {
    mActivityObservers = new nsTHashtable<nsPtrHashKey<nsISupports>>();
  }
  mActivityObservers->PutEntry(aSupports);
}

bool Document::UnregisterActivityObserver(nsISupports* aSupports) {
  if (!mActivityObservers) {
    return false;
  }
  nsPtrHashKey<nsISupports>* entry = mActivityObservers->GetEntry(aSupports);
  if (!entry) {
    return false;
  }
  mActivityObservers->RemoveEntry(entry);
  return true;
}

void Document::EnumerateActivityObservers(
    ActivityObserverEnumerator aEnumerator) {
  if (!mActivityObservers) {
    return;
  }

  nsTArray<nsCOMPtr<nsISupports>> observers(mActivityObservers->Count());
  for (auto iter = mActivityObservers->ConstIter(); !iter.Done(); iter.Next()) {
    observers.AppendElement(iter.Get()->GetKey());
  }

  for (auto& observer : observers) {
    aEnumerator(observer.get());
  }
}

void Document::RegisterPendingLinkUpdate(Link* aLink) {
  if (aLink->HasPendingLinkUpdate()) {
    return;
  }

  aLink->SetHasPendingLinkUpdate();

  if (!mHasLinksToUpdateRunnable && !mFlushingPendingLinkUpdates) {
    nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod("Document::FlushPendingLinkUpdates", this,
                          &Document::FlushPendingLinkUpdates);
    // Do this work in a second in the worst case.
    nsresult rv = NS_DispatchToCurrentThreadQueue(event.forget(), 1000,
                                                  EventQueuePriority::Idle);
    if (NS_FAILED(rv)) {
      // If during shutdown posting a runnable doesn't succeed, we probably
      // don't need to update link states.
      return;
    }
    mHasLinksToUpdateRunnable = true;
  }

  mLinksToUpdate.InfallibleAppend(aLink);
}

void Document::FlushPendingLinkUpdates() {
  MOZ_DIAGNOSTIC_ASSERT(!mFlushingPendingLinkUpdates);
  MOZ_ASSERT(mHasLinksToUpdateRunnable);
  mHasLinksToUpdateRunnable = false;

  auto restore = MakeScopeExit([&] { mFlushingPendingLinkUpdates = false; });
  mFlushingPendingLinkUpdates = true;

  while (!mLinksToUpdate.IsEmpty()) {
    LinksToUpdateList links(std::move(mLinksToUpdate));
    for (auto iter = links.Iter(); !iter.Done(); iter.Next()) {
      Link* link = iter.Get();
      Element* element = link->GetElement();
      if (element->OwnerDoc() == this) {
        link->ClearHasPendingLinkUpdate();
        if (element->IsInComposedDoc()) {
          element->UpdateLinkState(link->LinkState());
        }
      }
    }
  }
}

already_AddRefed<Document> Document::CreateStaticClone(
    nsIDocShell* aCloneContainer) {
  MOZ_ASSERT(!mCreatingStaticClone);
  MOZ_ASSERT(!GetProperty(nsGkAtoms::adoptedsheetclones));
  mCreatingStaticClone = true;
  SetProperty(nsGkAtoms::adoptedsheetclones, new AdoptedStyleSheetCloneCache(),
              nsINode::DeleteProperty<AdoptedStyleSheetCloneCache>);

  // Make document use different container during cloning.
  RefPtr<nsDocShell> originalShell = mDocumentContainer.get();
  SetContainer(static_cast<nsDocShell*>(aCloneContainer));
  ErrorResult rv;
  nsCOMPtr<nsINode> clonedNode = this->CloneNode(true, rv);
  SetContainer(originalShell);

  nsCOMPtr<Document> clonedDoc;
  if (rv.Failed()) {
    // Don't return yet; we need to reset mCreatingStaticClone
    rv.SuppressException();
  } else {
    clonedDoc = do_QueryInterface(clonedNode);
    if (clonedDoc) {
      if (IsStaticDocument()) {
        clonedDoc->mOriginalDocument = mOriginalDocument;
        mOriginalDocument->mLatestStaticClone = clonedDoc;
      } else {
        clonedDoc->mOriginalDocument = this;
        mLatestStaticClone = clonedDoc;
      }

      clonedDoc->mOriginalDocument->mStaticCloneCount++;

      size_t sheetsCount = SheetCount();
      for (size_t i = 0; i < sheetsCount; ++i) {
        RefPtr<StyleSheet> sheet = SheetAt(i);
        if (sheet) {
          if (sheet->IsApplicable()) {
            RefPtr<StyleSheet> clonedSheet =
                sheet->Clone(nullptr, nullptr, clonedDoc, nullptr);
            NS_WARNING_ASSERTION(clonedSheet,
                                 "Cloning a stylesheet didn't work!");
            if (clonedSheet) {
              clonedDoc->AddStyleSheet(clonedSheet);
            }
          }
        }
      }
      clonedDoc->CloneAdoptedSheetsFrom(*this);

      for (int t = 0; t < AdditionalSheetTypeCount; ++t) {
        auto& sheets = mAdditionalSheets[additionalSheetType(t)];
        for (StyleSheet* sheet : sheets) {
          if (sheet->IsApplicable()) {
            RefPtr<StyleSheet> clonedSheet =
                sheet->Clone(nullptr, nullptr, clonedDoc, nullptr);
            NS_WARNING_ASSERTION(clonedSheet,
                                 "Cloning a stylesheet didn't work!");
            if (clonedSheet) {
              clonedDoc->AddAdditionalStyleSheet(additionalSheetType(t),
                                                 clonedSheet);
            }
          }
        }
      }

      // Font faces created with the JS API will not be reflected in the
      // stylesheets and need to be copied over to the cloned document.
      if (const FontFaceSet* set = GetFonts()) {
        set->CopyNonRuleFacesTo(clonedDoc->Fonts());
      }

      clonedDoc->mReferrerInfo =
          static_cast<dom::ReferrerInfo*>(mReferrerInfo.get())->Clone();
      clonedDoc->mPreloadReferrerInfo = clonedDoc->mReferrerInfo;
    }
  }
  RemoveProperty(nsGkAtoms::adoptedsheetclones);
  mCreatingStaticClone = false;
  return clonedDoc.forget();
}

void Document::UnlinkOriginalDocumentIfStatic() {
  if (IsStaticDocument() && mOriginalDocument) {
    MOZ_ASSERT(mOriginalDocument->mStaticCloneCount > 0);
    mOriginalDocument->mStaticCloneCount--;
    mOriginalDocument = nullptr;
  }
  MOZ_ASSERT(!mOriginalDocument);
}

nsresult Document::ScheduleFrameRequestCallback(FrameRequestCallback& aCallback,
                                                int32_t* aHandle) {
  if (mFrameRequestCallbackCounter == INT32_MAX) {
    // Can't increment without overflowing; bail out
    return NS_ERROR_NOT_AVAILABLE;
  }
  int32_t newHandle = ++mFrameRequestCallbackCounter;

  DebugOnly<FrameRequest*> request =
      mFrameRequestCallbacks.AppendElement(FrameRequest(aCallback, newHandle));
  NS_ASSERTION(request, "This is supposed to be infallible!");
  UpdateFrameRequestCallbackSchedulingState();

  *aHandle = newHandle;
  return NS_OK;
}

void Document::CancelFrameRequestCallback(int32_t aHandle) {
  // mFrameRequestCallbacks is stored sorted by handle
  if (mFrameRequestCallbacks.RemoveElementSorted(aHandle)) {
    UpdateFrameRequestCallbackSchedulingState();
  } else {
    Unused << mCanceledFrameRequestCallbacks.put(aHandle);
  }
}

bool Document::IsCanceledFrameRequestCallback(int32_t aHandle) const {
  return !mCanceledFrameRequestCallbacks.empty() &&
         mCanceledFrameRequestCallbacks.has(aHandle);
}

nsresult Document::GetStateObject(nsIVariant** aState) {
  // Get the document's current state object. This is the object backing both
  // history.state and popStateEvent.state.
  //
  // mStateObjectContainer may be null; this just means that there's no
  // current state object.

  if (!mStateObjectCached && mStateObjectContainer) {
    AutoJSAPI jsapi;
    // Init with null is "OK" in the sense that it will just fail.
    if (!jsapi.Init(GetScopeObject())) {
      return NS_ERROR_UNEXPECTED;
    }
    mStateObjectContainer->DeserializeToVariant(
        jsapi.cx(), getter_AddRefs(mStateObjectCached));
  }

  NS_IF_ADDREF(*aState = mStateObjectCached);
  return NS_OK;
}

void Document::SetNavigationTiming(nsDOMNavigationTiming* aTiming) {
  mTiming = aTiming;
  if (!mLoadingTimeStamp.IsNull() && mTiming) {
    mTiming->SetDOMLoadingTimeStamp(GetDocumentURI(), mLoadingTimeStamp);
  }
}

nsContentList* Document::ImageMapList() {
  if (!mImageMaps) {
    mImageMaps = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::map,
                                   nsGkAtoms::map);
  }

  return mImageMaps;
}

#define DEPRECATED_OPERATION(_op) #_op "Warning",
static const char* kDeprecationWarnings[] = {
#include "nsDeprecatedOperationList.h"
    nullptr};
#undef DEPRECATED_OPERATION

#define DOCUMENT_WARNING(_op) #_op "Warning",
static const char* kDocumentWarnings[] = {
#include "nsDocumentWarningList.h"
    nullptr};
#undef DOCUMENT_WARNING

static UseCounter OperationToUseCounter(
    Document::DeprecatedOperations aOperation) {
  switch (aOperation) {
#define DEPRECATED_OPERATION(_op) \
  case Document::e##_op:          \
    return eUseCounter_##_op;
#include "nsDeprecatedOperationList.h"
#undef DEPRECATED_OPERATION
    default:
      MOZ_CRASH();
  }
}

bool Document::HasWarnedAbout(DeprecatedOperations aOperation) const {
  return mDeprecationWarnedAbout[aOperation];
}

void Document::WarnOnceAbout(DeprecatedOperations aOperation,
                             bool asError /* = false */) const {
  MOZ_ASSERT(NS_IsMainThread());
  if (HasWarnedAbout(aOperation)) {
    return;
  }
  mDeprecationWarnedAbout[aOperation] = true;
  // Don't count deprecated operations for about pages since those pages
  // are almost in our control, and we always need to remove uses there
  // before we remove the operation itself anyway.
  if (!IsAboutPage()) {
    const_cast<Document*>(this)->SetUseCounter(
        OperationToUseCounter(aOperation));
  }
  uint32_t flags =
      asError ? nsIScriptError::errorFlag : nsIScriptError::warningFlag;
  nsContentUtils::ReportToConsole(flags, NS_LITERAL_CSTRING("DOM Core"), this,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  kDeprecationWarnings[aOperation]);
}

bool Document::HasWarnedAbout(DocumentWarnings aWarning) const {
  return mDocWarningWarnedAbout[aWarning];
}

void Document::WarnOnceAbout(
    DocumentWarnings aWarning, bool asError /* = false */,
    const nsTArray<nsString>& aParams /* = empty array */) const {
  MOZ_ASSERT(NS_IsMainThread());
  if (HasWarnedAbout(aWarning)) {
    return;
  }
  mDocWarningWarnedAbout[aWarning] = true;
  uint32_t flags =
      asError ? nsIScriptError::errorFlag : nsIScriptError::warningFlag;
  nsContentUtils::ReportToConsole(flags, NS_LITERAL_CSTRING("DOM Core"), this,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  kDocumentWarnings[aWarning], aParams);
}

mozilla::dom::ImageTracker* Document::ImageTracker() {
  if (!mImageTracker) {
    mImageTracker = new mozilla::dom::ImageTracker;
  }
  return mImageTracker;
}

void Document::GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins) {
  aPlugins.SetCapacity(aPlugins.Length() + mPlugins.Count());
  for (auto iter = mPlugins.ConstIter(); !iter.Done(); iter.Next()) {
    aPlugins.AppendElement(iter.Get()->GetKey());
  }
  auto recurse = [&aPlugins](Document& aSubDoc) {
    aSubDoc.GetPlugins(aPlugins);
    return CallState::Continue;
  };
  EnumerateSubDocuments(recurse);
}

void Document::ScheduleSVGUseElementShadowTreeUpdate(
    SVGUseElement& aUseElement) {
  MOZ_ASSERT(aUseElement.IsInComposedDoc());

  mSVGUseElementsNeedingShadowTreeUpdate.PutEntry(&aUseElement);

  if (PresShell* presShell = GetPresShell()) {
    presShell->EnsureStyleFlush();
  }
}

void Document::DoUpdateSVGUseElementShadowTrees() {
  MOZ_ASSERT(!mSVGUseElementsNeedingShadowTreeUpdate.IsEmpty());
  nsTArray<RefPtr<SVGUseElement>> useElementsToUpdate;

  do {
    useElementsToUpdate.Clear();
    useElementsToUpdate.SetCapacity(
        mSVGUseElementsNeedingShadowTreeUpdate.Count());

    {
      for (auto iter = mSVGUseElementsNeedingShadowTreeUpdate.ConstIter();
           !iter.Done(); iter.Next()) {
        useElementsToUpdate.AppendElement(iter.Get()->GetKey());
      }
      mSVGUseElementsNeedingShadowTreeUpdate.Clear();
    }

    for (auto& useElement : useElementsToUpdate) {
      if (MOZ_UNLIKELY(!useElement->IsInComposedDoc())) {
        // The element was in another <use> shadow tree which we processed
        // already and also needed an update, and is removed from the document
        // now, so nothing to do here.
        MOZ_ASSERT(useElementsToUpdate.Length() > 1);
        continue;
      }
      useElement->UpdateShadowTree();
    }
  } while (!mSVGUseElementsNeedingShadowTreeUpdate.IsEmpty());
}

void Document::NotifyMediaFeatureValuesChanged() {
  for (auto iter = mResponsiveContent.ConstIter(); !iter.Done(); iter.Next()) {
    RefPtr<HTMLImageElement> imageElement = iter.Get()->GetKey();
    imageElement->MediaFeatureValuesChanged();
  }
}

already_AddRefed<Touch> Document::CreateTouch(
    nsGlobalWindowInner* aView, EventTarget* aTarget, int32_t aIdentifier,
    int32_t aPageX, int32_t aPageY, int32_t aScreenX, int32_t aScreenY,
    int32_t aClientX, int32_t aClientY, int32_t aRadiusX, int32_t aRadiusY,
    float aRotationAngle, float aForce) {
  RefPtr<Touch> touch =
      new Touch(aTarget, aIdentifier, aPageX, aPageY, aScreenX, aScreenY,
                aClientX, aClientY, aRadiusX, aRadiusY, aRotationAngle, aForce);
  return touch.forget();
}

already_AddRefed<TouchList> Document::CreateTouchList() {
  RefPtr<TouchList> retval = new TouchList(ToSupports(this));
  return retval.forget();
}

already_AddRefed<TouchList> Document::CreateTouchList(
    Touch& aTouch, const Sequence<OwningNonNull<Touch>>& aTouches) {
  RefPtr<TouchList> retval = new TouchList(ToSupports(this));
  retval->Append(&aTouch);
  for (uint32_t i = 0; i < aTouches.Length(); ++i) {
    retval->Append(aTouches[i].get());
  }
  return retval.forget();
}

already_AddRefed<TouchList> Document::CreateTouchList(
    const Sequence<OwningNonNull<Touch>>& aTouches) {
  RefPtr<TouchList> retval = new TouchList(ToSupports(this));
  for (uint32_t i = 0; i < aTouches.Length(); ++i) {
    retval->Append(aTouches[i].get());
  }
  return retval.forget();
}

already_AddRefed<nsDOMCaretPosition> Document::CaretPositionFromPoint(
    float aX, float aY) {
  using FrameForPointOption = nsLayoutUtils::FrameForPointOption;

  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY);
  nsPoint pt(x, y);

  FlushPendingNotifications(FlushType::Layout);

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return nullptr;
  }

  nsIFrame* rootFrame = presShell->GetRootFrame();

  // XUL docs, unlike HTML, have no frame tree until everything's done loading
  if (!rootFrame) {
    return nullptr;
  }

  nsIFrame* ptFrame = nsLayoutUtils::GetFrameForPoint(
      rootFrame, pt,
      {FrameForPointOption::IgnorePaintSuppression,
       FrameForPointOption::IgnoreCrossDoc});
  if (!ptFrame) {
    return nullptr;
  }

  // We require frame-relative coordinates for GetContentOffsetsFromPoint.
  nsPoint aOffset;
  nsCOMPtr<nsIWidget> widget = nsContentUtils::GetWidget(presShell, &aOffset);
  LayoutDeviceIntPoint refPoint = nsContentUtils::ToWidgetPoint(
      CSSPoint(aX, aY), aOffset, GetPresContext());
  nsPoint adjustedPoint =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(widget, refPoint, ptFrame);

  nsFrame::ContentOffsets offsets =
      ptFrame->GetContentOffsetsFromPoint(adjustedPoint);

  nsCOMPtr<nsIContent> node = offsets.content;
  uint32_t offset = offsets.offset;
  nsCOMPtr<nsIContent> anonNode = node;
  bool nodeIsAnonymous = node && node->IsInNativeAnonymousSubtree();
  if (nodeIsAnonymous) {
    node = ptFrame->GetContent();
    nsIContent* nonanon = node->FindFirstNonChromeOnlyAccessContent();
    HTMLTextAreaElement* textArea = HTMLTextAreaElement::FromNode(nonanon);
    nsITextControlFrame* textFrame = do_QueryFrame(nonanon->GetPrimaryFrame());
    if (textFrame) {
      // If the anonymous content node has a child, then we need to make sure
      // that we get the appropriate child, as otherwise the offset may not be
      // correct when we construct a range for it.
      nsCOMPtr<nsIContent> firstChild = anonNode->GetFirstChild();
      if (firstChild) {
        anonNode = firstChild;
      }

      if (textArea) {
        offset =
            nsContentUtils::GetAdjustedOffsetInTextControl(ptFrame, offset);
      }

      node = nonanon;
    } else {
      node = nullptr;
      offset = 0;
    }
  }

  RefPtr<nsDOMCaretPosition> aCaretPos = new nsDOMCaretPosition(node, offset);
  if (nodeIsAnonymous) {
    aCaretPos->SetAnonymousContentNode(anonNode);
  }
  return aCaretPos.forget();
}

bool Document::IsPotentiallyScrollable(HTMLBodyElement* aBody) {
  // We rely on correct frame information here, so need to flush frames.
  FlushPendingNotifications(FlushType::Frames);

  // An element is potentially scrollable if all of the following conditions are
  // true:

  // The element has an associated CSS layout box.
  nsIFrame* bodyFrame = aBody->GetPrimaryFrame();
  if (!bodyFrame) {
    return false;
  }

  // The element is not the HTML body element, or it is and the root element's
  // used value of the overflow-x or overflow-y properties is not visible.
  MOZ_ASSERT(aBody->GetParent() == aBody->OwnerDoc()->GetRootElement());
  nsIFrame* parentFrame = aBody->GetParent()->GetPrimaryFrame();
  if (parentFrame &&
      parentFrame->StyleDisplay()->mOverflowX == StyleOverflow::Visible &&
      parentFrame->StyleDisplay()->mOverflowY == StyleOverflow::Visible) {
    return false;
  }

  // The element's used value of the overflow-x or overflow-y properties is not
  // visible.
  if (bodyFrame->StyleDisplay()->mOverflowX == StyleOverflow::Visible &&
      bodyFrame->StyleDisplay()->mOverflowY == StyleOverflow::Visible) {
    return false;
  }

  return true;
}

Element* Document::GetScrollingElement() {
  // Keep this in sync with IsScrollingElement.
  if (GetCompatibilityMode() == eCompatibility_NavQuirks) {
    RefPtr<HTMLBodyElement> body = GetBodyElement();
    if (body && !IsPotentiallyScrollable(body)) {
      return body;
    }

    return nullptr;
  }

  return GetRootElement();
}

bool Document::IsScrollingElement(Element* aElement) {
  // Keep this in sync with GetScrollingElement.
  MOZ_ASSERT(aElement);

  if (GetCompatibilityMode() != eCompatibility_NavQuirks) {
    return aElement == GetRootElement();
  }

  // In the common case when aElement != body, avoid refcounting.
  HTMLBodyElement* body = GetBodyElement();
  if (aElement != body) {
    return false;
  }

  // Now we know body is non-null, since aElement is not null.  It's the
  // scrolling element for the document if it itself is not potentially
  // scrollable.
  RefPtr<HTMLBodyElement> strongBody(body);
  return !IsPotentiallyScrollable(strongBody);
}

class UnblockParsingPromiseHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UnblockParsingPromiseHandler)

  explicit UnblockParsingPromiseHandler(Document* aDocument, Promise* aPromise,
                                        const BlockParsingOptions& aOptions)
      : mPromise(aPromise) {
    nsCOMPtr<nsIParser> parser = aDocument->CreatorParserOrNull();
    if (parser &&
        (aOptions.mBlockScriptCreated || !parser->IsScriptCreated())) {
      parser->BlockParser();
      mParser = do_GetWeakReference(parser);
      mDocument = aDocument;
      mDocument->BlockOnload();
      mDocument->BlockDOMContentLoaded();
    }
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    MaybeUnblockParser();

    mPromise->MaybeResolve(aValue);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    MaybeUnblockParser();

    mPromise->MaybeReject(aValue);
  }

 protected:
  virtual ~UnblockParsingPromiseHandler() {
    // If we're being cleaned up by the cycle collector, our mDocument reference
    // may have been unlinked while our mParser weak reference is still alive.
    if (mDocument) {
      MaybeUnblockParser();
    }
  }

 private:
  void MaybeUnblockParser() {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mParser);
    if (parser) {
      MOZ_DIAGNOSTIC_ASSERT(mDocument);
      nsCOMPtr<nsIParser> docParser = mDocument->CreatorParserOrNull();
      if (parser == docParser) {
        parser->UnblockParser();
        parser->ContinueInterruptedParsingAsync();
      }
    }
    if (mDocument) {
      // We blocked DOMContentLoaded and load events on this document.  Unblock
      // them.  Note that we want to do that no matter what's going on with the
      // parser state for this document.  Maybe someone caused it to stop being
      // parsed, so CreatorParserOrNull() is returning null, but we still want
      // to unblock these.
      mDocument->UnblockDOMContentLoaded();
      mDocument->UnblockOnload(false);
    }
    mParser = nullptr;
    mDocument = nullptr;
  }

  nsWeakPtr mParser;
  RefPtr<Promise> mPromise;
  RefPtr<Document> mDocument;
};

NS_IMPL_CYCLE_COLLECTION(UnblockParsingPromiseHandler, mDocument, mPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnblockParsingPromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UnblockParsingPromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnblockParsingPromiseHandler)

already_AddRefed<Promise> Document::BlockParsing(
    Promise& aPromise, const BlockParsingOptions& aOptions, ErrorResult& aRv) {
  RefPtr<Promise> resultPromise =
      Promise::Create(aPromise.GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<PromiseNativeHandler> promiseHandler =
      new UnblockParsingPromiseHandler(this, resultPromise, aOptions);
  aPromise.AppendNativeHandler(promiseHandler);

  return resultPromise.forget();
}

already_AddRefed<nsIURI> Document::GetMozDocumentURIIfNotForErrorPages() {
  if (mFailedChannel) {
    nsCOMPtr<nsIURI> failedURI;
    if (NS_SUCCEEDED(mFailedChannel->GetURI(getter_AddRefs(failedURI)))) {
      return failedURI.forget();
    }
  }

  nsCOMPtr<nsIURI> uri = GetDocumentURIObject();
  if (!uri) {
    return nullptr;
  }

  return uri.forget();
}

Promise* Document::GetDocumentReadyForIdle(ErrorResult& aRv) {
  if (mIsGoingAway) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  if (!mReadyForIdle) {
    nsIGlobalObject* global = GetScopeObject();
    if (!global) {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    mReadyForIdle = Promise::Create(global, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  return mReadyForIdle;
}

void Document::MaybeResolveReadyForIdle() {
  IgnoredErrorResult rv;
  Promise* readyPromise = GetDocumentReadyForIdle(rv);
  if (readyPromise) {
    readyPromise->MaybeResolve(this);
  }
}

mozilla::dom::FeaturePolicy* Document::FeaturePolicy() const {
  // The policy is created when the document is initialized. We _must_ have a
  // policy here even if the featurePolicy pref is off. If this assertion fails,
  // it means that ::FeaturePolicy() is called before ::StartDocumentLoad().
  MOZ_ASSERT(mFeaturePolicy);
  return mFeaturePolicy;
}

nsIDOMXULCommandDispatcher* Document::GetCommandDispatcher() {
  // Only chrome documents are allowed to use command dispatcher.
  if (!nsContentUtils::IsChromeDoc(this)) {
    return nullptr;
  }
  if (!mCommandDispatcher) {
    // Create our command dispatcher and hook it up.
    mCommandDispatcher = new nsXULCommandDispatcher(this);
  }
  return mCommandDispatcher;
}

void Document::InitializeXULBroadcastManager() {
  if (mXULBroadcastManager) {
    return;
  }
  mXULBroadcastManager = new XULBroadcastManager(this);
}

static bool NodeHasScopeObject(nsINode* node) {
  MOZ_ASSERT(node, "Must not be called with null.");

  // Window root occasionally keeps alive a node of a document whose
  // window is already dead. If in this brief period someone calls
  // GetPopupNode and we return that node, we can end up creating a
  // reflector for the node in the wrong global (the current global,
  // not the document global, because we won't know what the document
  // global is).  Returning an orphan node like that to JS would be a
  // bug anyway, so to avoid this, let's do the same check as fetching
  // GetParentObject() on the document does to determine the scope and
  // if there is no usable scope object let's report that and return
  // null from Document::GetPopupNode instead of returning a node that
  // will get a reflector in the wrong scope.
  Document* doc = node->OwnerDoc();
  MOZ_ASSERT(doc, "This should never happen.");

  nsIGlobalObject* global = doc->GetScopeObject();
  return global ? global->HasJSGlobal() : false;
}

already_AddRefed<nsPIWindowRoot> Document::GetWindowRoot() {
  if (!mDocumentContainer) {
    return nullptr;
  }
  // XXX It's unclear why this can't just use GetWindow().
  nsCOMPtr<nsPIDOMWindowOuter> piWin = mDocumentContainer->GetWindow();
  return piWin ? piWin->GetTopWindowRoot() : nullptr;
}

already_AddRefed<nsINode> Document::GetPopupNode() {
  nsCOMPtr<nsINode> node;
  nsCOMPtr<nsPIWindowRoot> rootWin = GetWindowRoot();
  if (rootWin) {
    node = rootWin->GetPopupNode();  // addref happens here
  }

  if (!node) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      node = pm->GetLastTriggerPopupNode(this);
    }
  }

  if (node && NodeHasScopeObject(node)) {
    return node.forget();
  }

  return nullptr;
}

void Document::SetPopupNode(nsINode* aNode) {
  nsCOMPtr<nsPIWindowRoot> rootWin = GetWindowRoot();
  if (rootWin) {
    rootWin->SetPopupNode(aNode);
  }
}

// Returns the rangeOffset element from the XUL Popup Manager. This is for
// chrome callers only.
nsINode* Document::GetPopupRangeParent(ErrorResult& aRv) {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return pm->GetMouseLocationParent();
}

// Returns the rangeOffset element from the XUL Popup Manager.
int32_t Document::GetPopupRangeOffset(ErrorResult& aRv) {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  return pm->MouseLocationOffset();
}

already_AddRefed<nsINode> Document::GetTooltipNode() {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsCOMPtr<nsINode> node = pm->GetLastTriggerTooltipNode(this);
    if (node) {
      return node.forget();
    }
  }

  return nullptr;
}

void Document::MaybeWarnAboutZoom() {
  if (mHasWarnedAboutZoom) {
    return;
  }
  const bool usedZoom =
      mStyleUseCounters && Servo_IsPropertyIdRecordedInUseCounter(
                               mStyleUseCounters.get(), eCSSProperty_zoom);
  if (!usedZoom) {
    return;
  }

  mHasWarnedAboutZoom = true;
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Layout"), this,
      nsContentUtils::eLAYOUT_PROPERTIES, "ZoomPropertyWarning");
}

nsIHTMLCollection* Document::Children() {
  if (!mChildrenCollection) {
    mChildrenCollection =
        new nsContentList(this, kNameSpaceID_Wildcard, nsGkAtoms::_asterisk,
                          nsGkAtoms::_asterisk, false);
  }

  return mChildrenCollection;
}

uint32_t Document::ChildElementCount() { return Children()->Length(); }

// Singleton class to manage the list of fullscreen documents which are the
// root of a branch which contains fullscreen documents. We maintain this list
// so that we can easily exit all windows from fullscreen when the user
// presses the escape key.
class FullscreenRoots {
 public:
  // Adds the root of given document to the manager. Calling this method
  // with a document whose root is already contained has no effect.
  static void Add(Document* aDoc);

  // Iterates over every root in the root list, and calls aFunction, passing
  // each root once to aFunction. It is safe to call Add() and Remove() while
  // iterating over the list (i.e. in aFunction). Documents that are removed
  // from the manager during traversal are not traversed, and documents that
  // are added to the manager during traversal are also not traversed.
  static void ForEach(void (*aFunction)(Document* aDoc));

  // Removes the root of a specific document from the manager.
  static void Remove(Document* aDoc);

  // Returns true if all roots added to the list have been removed.
  static bool IsEmpty();

 private:
  MOZ_COUNTED_DEFAULT_CTOR(FullscreenRoots)
  MOZ_COUNTED_DTOR(FullscreenRoots)

  enum { NotFound = uint32_t(-1) };
  // Looks in mRoots for aRoot. Returns the index if found, otherwise NotFound.
  static uint32_t Find(Document* aRoot);

  // Returns true if aRoot is in the list of fullscreen roots.
  static bool Contains(Document* aRoot);

  // Singleton instance of the FullscreenRoots. This is instantiated when a
  // root is added, and it is deleted when the last root is removed.
  static FullscreenRoots* sInstance;

  // List of weak pointers to roots.
  nsTArray<nsWeakPtr> mRoots;
};

FullscreenRoots* FullscreenRoots::sInstance = nullptr;

/* static */
void FullscreenRoots::ForEach(void (*aFunction)(Document* aDoc)) {
  if (!sInstance) {
    return;
  }
  // Create a copy of the roots array, and iterate over the copy. This is so
  // that if an element is removed from mRoots we don't mess up our iteration.
  nsTArray<nsWeakPtr> roots(sInstance->mRoots);
  // Call aFunction on all entries.
  for (uint32_t i = 0; i < roots.Length(); i++) {
    nsCOMPtr<Document> root = do_QueryReferent(roots[i]);
    // Check that the root isn't in the manager. This is so that new additions
    // while we were running don't get traversed.
    if (root && FullscreenRoots::Contains(root)) {
      aFunction(root);
    }
  }
}

/* static */
bool FullscreenRoots::Contains(Document* aRoot) {
  return FullscreenRoots::Find(aRoot) != NotFound;
}

/* static */
void FullscreenRoots::Add(Document* aDoc) {
  nsCOMPtr<Document> root = nsContentUtils::GetRootDocument(aDoc);
  if (!FullscreenRoots::Contains(root)) {
    if (!sInstance) {
      sInstance = new FullscreenRoots();
    }
    sInstance->mRoots.AppendElement(do_GetWeakReference(root));
  }
}

/* static */
uint32_t FullscreenRoots::Find(Document* aRoot) {
  if (!sInstance) {
    return NotFound;
  }
  nsTArray<nsWeakPtr>& roots = sInstance->mRoots;
  for (uint32_t i = 0; i < roots.Length(); i++) {
    nsCOMPtr<Document> otherRoot(do_QueryReferent(roots[i]));
    if (otherRoot == aRoot) {
      return i;
    }
  }
  return NotFound;
}

/* static */
void FullscreenRoots::Remove(Document* aDoc) {
  nsCOMPtr<Document> root = nsContentUtils::GetRootDocument(aDoc);
  uint32_t index = Find(root);
  NS_ASSERTION(index != NotFound,
               "Should only try to remove roots which are still added!");
  if (index == NotFound || !sInstance) {
    return;
  }
  sInstance->mRoots.RemoveElementAt(index);
  if (sInstance->mRoots.IsEmpty()) {
    delete sInstance;
    sInstance = nullptr;
  }
}

/* static */
bool FullscreenRoots::IsEmpty() { return !sInstance; }

// Any fullscreen change waiting for the widget to finish transition
// is queued here. This is declared static instead of a member of
// Document because in the majority of time, there would be at most
// one document requesting or exiting fullscreen. We shouldn't waste
// the space to hold for it in every document.
class PendingFullscreenChangeList {
 public:
  PendingFullscreenChangeList() = delete;

  template <typename T>
  static void Add(UniquePtr<T> aChange) {
    sList.insertBack(aChange.release());
  }

  static const FullscreenChange* GetLast() { return sList.getLast(); }

  enum IteratorOption {
    // When we are committing fullscreen changes or preparing for
    // that, we generally want to iterate all requests in the same
    // window with eDocumentsWithSameRoot option.
    eDocumentsWithSameRoot,
    // If we are removing a document from the tree, we would only
    // want to remove the requests from the given document and its
    // descendants. For that case, use eInclusiveDescendants.
    eInclusiveDescendants
  };

  template <typename T>
  class Iterator {
   public:
    explicit Iterator(Document* aDoc, IteratorOption aOption)
        : mCurrent(PendingFullscreenChangeList::sList.getFirst()),
          mRootShellForIteration(aDoc->GetDocShell()) {
      if (mCurrent) {
        if (mRootShellForIteration && aOption == eDocumentsWithSameRoot) {
          // Use a temporary to avoid undefined behavior from passing
          // mRootShellForIteration.
          nsCOMPtr<nsIDocShellTreeItem> root;
          mRootShellForIteration->GetInProcessRootTreeItem(
              getter_AddRefs(root));
          mRootShellForIteration = std::move(root);
        }
        SkipToNextMatch();
      }
    }

    UniquePtr<T> TakeAndNext() {
      auto thisChange = TakeAndNextInternal();
      SkipToNextMatch();
      return thisChange;
    }
    bool AtEnd() const { return mCurrent == nullptr; }

   private:
    UniquePtr<T> TakeAndNextInternal() {
      FullscreenChange* thisChange = mCurrent;
      MOZ_ASSERT(thisChange->Type() == T::kType);
      mCurrent = mCurrent->removeAndGetNext();
      return WrapUnique(static_cast<T*>(thisChange));
    }
    void SkipToNextMatch() {
      while (mCurrent) {
        if (mCurrent->Type() == T::kType) {
          nsCOMPtr<nsIDocShellTreeItem> docShell =
              mCurrent->Document()->GetDocShell();
          if (!docShell) {
            // Always automatically drop fullscreen changes which are
            // from a document detached from the doc shell.
            UniquePtr<T> change = TakeAndNextInternal();
            change->MayRejectPromise("Document is not active");
            continue;
          }
          while (docShell && docShell != mRootShellForIteration) {
            nsCOMPtr<nsIDocShellTreeItem> parent;
            docShell->GetInProcessParent(getter_AddRefs(parent));
            docShell = std::move(parent);
          }
          if (docShell) {
            break;
          }
        }
        // The current one either don't have matched type, or isn't
        // inside the given subtree, so skip this item.
        mCurrent = mCurrent->getNext();
      }
    }

    FullscreenChange* mCurrent;
    nsCOMPtr<nsIDocShellTreeItem> mRootShellForIteration;
  };

 private:
  static LinkedList<FullscreenChange> sList;
};

/* static */
LinkedList<FullscreenChange> PendingFullscreenChangeList::sList;

Document* Document::GetFullscreenRoot() {
  nsCOMPtr<Document> root = do_QueryReferent(mFullscreenRoot);
  return root;
}

void Document::SetFullscreenRoot(Document* aRoot) {
  mFullscreenRoot = do_GetWeakReference(aRoot);
}

already_AddRefed<Promise> Document::ExitFullscreen(ErrorResult& aRv) {
  UniquePtr<FullscreenExit> exit = FullscreenExit::Create(this, aRv);
  RefPtr<Promise> promise = exit->GetPromise();
  RestorePreviousFullscreenState(std::move(exit));
  return promise.forget();
}

static void AskWindowToExitFullscreen(Document* aDoc) {
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    nsContentUtils::DispatchEventOnlyToChrome(
        aDoc, ToSupports(aDoc), NS_LITERAL_STRING("MozDOMFullscreen:Exit"),
        CanBubble::eYes, Cancelable::eNo, /* DefaultAction */ nullptr);
  } else {
    if (nsPIDOMWindowOuter* win = aDoc->GetWindow()) {
      win->SetFullscreenInternal(FullscreenReason::ForFullscreenAPI, false);
    }
  }
}

class nsCallExitFullscreen : public Runnable {
 public:
  explicit nsCallExitFullscreen(Document* aDoc)
      : mozilla::Runnable("nsCallExitFullscreen"), mDoc(aDoc) {}

  NS_IMETHOD Run() final {
    if (!mDoc) {
      FullscreenRoots::ForEach(&AskWindowToExitFullscreen);
    } else {
      AskWindowToExitFullscreen(mDoc);
    }
    return NS_OK;
  }

 private:
  nsCOMPtr<Document> mDoc;
};

/* static */
void Document::AsyncExitFullscreen(Document* aDoc) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> exit = new nsCallExitFullscreen(aDoc);
  if (aDoc) {
    aDoc->Dispatch(TaskCategory::Other, exit.forget());
  } else {
    NS_DispatchToCurrentThread(exit.forget());
  }
}

static uint32_t CountFullscreenSubDocuments(Document& aDoc) {
  uint32_t count = 0;
  // FIXME(emilio): Should this be recursive and dig into our nested subdocs?
  auto subDoc = [&count](Document& aSubDoc) {
    if (aSubDoc.FullscreenStackTop()) {
      count++;
    }
    return CallState::Continue;
  };
  aDoc.EnumerateSubDocuments(subDoc);
  return count;
}

bool Document::IsFullscreenLeaf() {
  // A fullscreen leaf document is fullscreen, and has no fullscreen
  // subdocuments.
  if (!FullscreenStackTop()) {
    return false;
  }
  return CountFullscreenSubDocuments(*this) == 0;
}

static Document* GetFullscreenLeaf(Document& aDoc) {
  if (aDoc.IsFullscreenLeaf()) {
    return &aDoc;
  }
  if (!aDoc.FullscreenStackTop()) {
    return nullptr;
  }
  Document* leaf = nullptr;
  auto recurse = [&leaf](Document& aSubDoc) {
    leaf = GetFullscreenLeaf(aSubDoc);
    return leaf ? CallState::Stop : CallState::Continue;
  };
  aDoc.EnumerateSubDocuments(recurse);
  return leaf;
}

static Document* GetFullscreenLeaf(Document* aDoc) {
  if (Document* leaf = GetFullscreenLeaf(*aDoc)) {
    return leaf;
  }
  // Otherwise we could be either in a non-fullscreen doc tree, or we're
  // below the fullscreen doc. Start the search from the root.
  Document* root = nsContentUtils::GetRootDocument(aDoc);
  return GetFullscreenLeaf(*root);
}

static CallState ResetFullscreen(Document& aDocument) {
  if (Element* fsElement = aDocument.FullscreenStackTop()) {
    NS_ASSERTION(CountFullscreenSubDocuments(aDocument) <= 1,
                 "Should have at most 1 fullscreen subdocument.");
    aDocument.CleanupFullscreenState();
    NS_ASSERTION(!aDocument.FullscreenStackTop(), "Should reset fullscreen");
    DispatchFullscreenChange(aDocument, fsElement);
    aDocument.EnumerateSubDocuments(ResetFullscreen);
  }
  return CallState::Continue;
}

// Since Document::ExitFullscreenInDocTree() could be called from
// Element::UnbindFromTree() where it is not safe to synchronously run
// script. This runnable is the script part of that function.
class ExitFullscreenScriptRunnable : public Runnable {
 public:
  explicit ExitFullscreenScriptRunnable(Document* aRoot, Document* aLeaf)
      : mozilla::Runnable("ExitFullscreenScriptRunnable"),
        mRoot(aRoot),
        mLeaf(aLeaf) {}

  NS_IMETHOD Run() override {
    // Dispatch MozDOMFullscreen:Exited to the original fullscreen leaf
    // document since we want this event to follow the same path that
    // MozDOMFullscreen:Entered was dispatched.
    nsContentUtils::DispatchEventOnlyToChrome(
        mLeaf, ToSupports(mLeaf), NS_LITERAL_STRING("MozDOMFullscreen:Exited"),
        CanBubble::eYes, Cancelable::eNo, /* DefaultAction */ nullptr);
    // Ensure the window exits fullscreen.
    if (nsPIDOMWindowOuter* win = mRoot->GetWindow()) {
      win->SetFullscreenInternal(FullscreenReason::ForForceExitFullscreen,
                                 false);
    }
    return NS_OK;
  }

 private:
  nsCOMPtr<Document> mRoot;
  nsCOMPtr<Document> mLeaf;
};

/* static */
void Document::ExitFullscreenInDocTree(Document* aMaybeNotARootDoc) {
  MOZ_ASSERT(aMaybeNotARootDoc);

  // Unlock the pointer
  UnlockPointer();

  // Resolve all promises which waiting for exit fullscreen.
  PendingFullscreenChangeList::Iterator<FullscreenExit> iter(
      aMaybeNotARootDoc, PendingFullscreenChangeList::eDocumentsWithSameRoot);
  while (!iter.AtEnd()) {
    UniquePtr<FullscreenExit> exit = iter.TakeAndNext();
    exit->MayResolvePromise();
  }

  nsCOMPtr<Document> root = aMaybeNotARootDoc->GetFullscreenRoot();
  if (!root || !root->FullscreenStackTop()) {
    // If a document was detached before exiting from fullscreen, it is
    // possible that the root had left fullscreen state. In this case,
    // we would not get anything from the ResetFullscreen() call. Root's
    // not being a fullscreen doc also means the widget should have
    // exited fullscreen state. It means even if we do not return here,
    // we would actually do nothing below except crashing ourselves via
    // dispatching the "MozDOMFullscreen:Exited" event to an nonexistent
    // document.
    return;
  }

  // Record the fullscreen leaf document for MozDOMFullscreen:Exited.
  // See ExitFullscreenScriptRunnable::Run for details. We have to
  // record it here because we don't have such information after we
  // reset the fullscreen state below.
  Document* fullscreenLeaf = GetFullscreenLeaf(root);

  // Walk the tree of fullscreen documents, and reset their fullscreen state.
  ResetFullscreen(*root);

  NS_ASSERTION(!root->FullscreenStackTop(),
               "Fullscreen root should no longer be a fullscreen doc...");

  // Move the top-level window out of fullscreen mode.
  FullscreenRoots::Remove(root);

  nsContentUtils::AddScriptRunner(
      new ExitFullscreenScriptRunnable(root, fullscreenLeaf));
}

static void DispatchFullscreenNewOriginEvent(Document* aDoc) {
  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      aDoc, NS_LITERAL_STRING("MozDOMFullscreen:NewOrigin"), CanBubble::eYes,
      ChromeOnlyDispatch::eYes);
  asyncDispatcher->PostDOMEvent();
}

void Document::RestorePreviousFullscreenState(UniquePtr<FullscreenExit> aExit) {
  NS_ASSERTION(!FullscreenStackTop() || !FullscreenRoots::IsEmpty(),
               "Should have at least 1 fullscreen root when fullscreen!");

  if (!GetWindow()) {
    aExit->MayRejectPromise("No active window");
    return;
  }
  if (!FullscreenStackTop() || FullscreenRoots::IsEmpty()) {
    aExit->MayRejectPromise("Not in fullscreen mode");
    return;
  }

  nsCOMPtr<Document> fullScreenDoc = GetFullscreenLeaf(this);
  AutoTArray<Element*, 8> exitElements;

  Document* doc = fullScreenDoc;
  // Collect all subdocuments.
  for (; doc != this; doc = doc->GetInProcessParentDocument()) {
    Element* fsElement = doc->FullscreenStackTop();
    MOZ_ASSERT(fsElement,
               "Parent document of "
               "a fullscreen document without fullscreen element?");
    exitElements.AppendElement(fsElement);
  }
  MOZ_ASSERT(doc == this, "Must have reached this doc");
  // Collect all ancestor documents which we are going to change.
  for (; doc; doc = doc->GetInProcessParentDocument()) {
    MOZ_ASSERT(!doc->mFullscreenStack.IsEmpty(),
               "Ancestor of fullscreen document must also be in fullscreen");
    Element* fsElement = doc->FullscreenStackTop();
    if (doc != this) {
      if (auto* iframe = HTMLIFrameElement::FromNode(fsElement)) {
        if (iframe->FullscreenFlag()) {
          // If this is an iframe, and it explicitly requested
          // fullscreen, don't rollback it automatically.
          break;
        }
      }
    }
    exitElements.AppendElement(fsElement);
    if (doc->mFullscreenStack.Length() > 1) {
      break;
    }
  }

  Document* lastDoc = exitElements.LastElement()->OwnerDoc();
  if (!lastDoc->GetInProcessParentDocument() &&
      lastDoc->mFullscreenStack.Length() == 1) {
    // If we are fully exiting fullscreen, don't touch anything here,
    // just wait for the window to get out from fullscreen first.
    PendingFullscreenChangeList::Add(std::move(aExit));
    AskWindowToExitFullscreen(this);
    return;
  }

  // If fullscreen mode is updated the pointer should be unlocked
  UnlockPointer();
  // All documents listed in the array except the last one are going to
  // completely exit from the fullscreen state.
  for (auto i : IntegerRange(exitElements.Length() - 1)) {
    exitElements[i]->OwnerDoc()->CleanupFullscreenState();
  }
  // The last document will either rollback one fullscreen element, or
  // completely exit from the fullscreen state as well.
  Document* newFullscreenDoc;
  if (lastDoc->mFullscreenStack.Length() > 1) {
    lastDoc->FullscreenStackPop();
    newFullscreenDoc = lastDoc;
  } else {
    lastDoc->CleanupFullscreenState();
    newFullscreenDoc = lastDoc->GetInProcessParentDocument();
  }
  // Dispatch the fullscreenchange event to all document listed. Note
  // that the loop order is reversed so that events are dispatched in
  // the tree order as indicated in the spec.
  for (Element* e : Reversed(exitElements)) {
    DispatchFullscreenChange(*e->OwnerDoc(), e);
  }
  aExit->MayResolvePromise();

  MOZ_ASSERT(newFullscreenDoc,
             "If we were going to exit from fullscreen on "
             "all documents in this doctree, we should've asked the window to "
             "exit first instead of reaching here.");
  if (fullScreenDoc != newFullscreenDoc &&
      !nsContentUtils::HaveEqualPrincipals(fullScreenDoc, newFullscreenDoc)) {
    // We've popped so enough off the stack that we've rolled back to
    // a fullscreen element in a parent document. If this document is
    // cross origin, dispatch an event to chrome so it knows to show
    // the warning UI.
    DispatchFullscreenNewOriginEvent(newFullscreenDoc);
  }
}

class nsCallRequestFullscreen : public Runnable {
 public:
  explicit nsCallRequestFullscreen(UniquePtr<FullscreenRequest> aRequest)
      : mozilla::Runnable("nsCallRequestFullscreen"),
        mRequest(std::move(aRequest)) {}

  NS_IMETHOD Run() override {
    Document* doc = mRequest->Document();
    doc->RequestFullscreen(std::move(mRequest));
    return NS_OK;
  }

  UniquePtr<FullscreenRequest> mRequest;
};

void Document::AsyncRequestFullscreen(UniquePtr<FullscreenRequest> aRequest) {
  // Request fullscreen asynchronously.
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> event =
      new nsCallRequestFullscreen(std::move(aRequest));
  Dispatch(TaskCategory::Other, event.forget());
}

static void UpdateViewportScrollbarOverrideForFullscreen(Document* aDoc) {
  if (nsPresContext* presContext = aDoc->GetPresContext()) {
    presContext->UpdateViewportScrollStylesOverride();
  }
}

static void ClearFullscreenStateOnElement(Element* aElement) {
  // Remove styles from existing top element.
  EventStateManager::SetFullscreenState(aElement, false);
  // Reset iframe fullscreen flag.
  if (aElement->IsHTMLElement(nsGkAtoms::iframe)) {
    static_cast<HTMLIFrameElement*>(aElement)->SetFullscreenFlag(false);
  }
}

void Document::CleanupFullscreenState() {
  // Iterate the fullscreen stack and clear the fullscreen states.
  // Since we also need to clear the fullscreen-ancestor state, and
  // currently fullscreen elements can only be placed in hierarchy
  // order in the stack, reversely iterating the stack could be more
  // efficient. NOTE that fullscreen-ancestor state would be removed
  // in bug 1199529, and the elements may not in hierarchy order
  // after bug 1195213.
  for (nsWeakPtr& weakPtr : Reversed(mFullscreenStack)) {
    if (nsCOMPtr<Element> element = do_QueryReferent(weakPtr)) {
      ClearFullscreenStateOnElement(element);
    }
  }
  mFullscreenStack.Clear();
  mFullscreenRoot = nullptr;

  // Restore the zoom level that was in place prior to entering fullscreen.
  if (PresShell* presShell = GetPresShell()) {
    if (presShell->GetMobileViewportManager()) {
      presShell->SetResolutionAndScaleTo(
          mSavedResolution, ResolutionChangeOrigin::MainThreadRestore);
    }
  }

  UpdateViewportScrollbarOverrideForFullscreen(this);
}

bool Document::FullscreenStackPush(Element* aElement) {
  NS_ASSERTION(aElement, "Must pass non-null to FullscreenStackPush()");
  Element* top = FullscreenStackTop();
  if (top == aElement || !aElement) {
    return false;
  }
  EventStateManager::SetFullscreenState(aElement, true);
  mFullscreenStack.AppendElement(do_GetWeakReference(aElement));
  NS_ASSERTION(FullscreenStackTop() == aElement, "Should match");
  UpdateViewportScrollbarOverrideForFullscreen(this);
  return true;
}

void Document::FullscreenStackPop() {
  if (mFullscreenStack.IsEmpty()) {
    return;
  }

  ClearFullscreenStateOnElement(FullscreenStackTop());

  // Remove top element. Note the remaining top element in the stack
  // will not have fullscreen style bits set, so we will need to restore
  // them on the new top element before returning.
  uint32_t last = mFullscreenStack.Length() - 1;
  mFullscreenStack.RemoveElementAt(last);

  // Pop from the stack null elements (references to elements which have
  // been GC'd since they were added to the stack) and elements which are
  // no longer in this document.
  while (!mFullscreenStack.IsEmpty()) {
    Element* element = FullscreenStackTop();
    if (!element || !element->IsInUncomposedDoc() ||
        element->OwnerDoc() != this) {
      NS_ASSERTION(!element->State().HasState(NS_EVENT_STATE_FULLSCREEN),
                   "Should have already removed fullscreen styles");
      uint32_t last = mFullscreenStack.Length() - 1;
      mFullscreenStack.RemoveElementAt(last);
    } else {
      // The top element of the stack is now an in-doc element. Return here.
      break;
    }
  }

  UpdateViewportScrollbarOverrideForFullscreen(this);
}

Element* Document::FullscreenStackTop() {
  if (mFullscreenStack.IsEmpty()) {
    return nullptr;
  }
  uint32_t last = mFullscreenStack.Length() - 1;
  nsCOMPtr<Element> element(do_QueryReferent(mFullscreenStack[last]));
  NS_ASSERTION(element, "Should have fullscreen element!");
  NS_ASSERTION(element->IsInComposedDoc(),
               "Fullscreen element should be in doc");
  NS_ASSERTION(element->OwnerDoc() == this,
               "Fullscreen element should be in this doc");
  return element;
}

nsTArray<Element*> Document::GetFullscreenStack() const {
  nsTArray<Element*> elements;
  for (const nsWeakPtr& ptr : mFullscreenStack) {
    if (nsCOMPtr<Element> elem = do_QueryReferent(ptr)) {
      MOZ_ASSERT(elem->State().HasState(NS_EVENT_STATE_FULLSCREEN));
      elements.AppendElement(elem);
    }
  }
  return elements;
}

// Returns true if aDoc is in the focused tab in the active window.
bool IsInActiveTab(Document* aDoc) {
  nsCOMPtr<nsIDocShell> docshell = aDoc->GetDocShell();
  if (!docshell) {
    return false;
  }

  bool isActive = false;
  docshell->GetIsActive(&isActive);
  if (!isActive) {
    return false;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return false;
  }

  if (XRE_IsParentProcess()) {
    // Keep dom/tests/mochitest/chrome/test_MozDomFullscreen_event.xhtml happy
    // by retaining the old code path for the parent process.

    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    docshell->GetInProcessRootTreeItem(getter_AddRefs(rootItem));
    if (!rootItem) {
      return false;
    }
    nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem->GetWindow();
    if (!rootWin) {
      return false;
    }

    nsCOMPtr<nsPIDOMWindowOuter> activeWindow;
    activeWindow = fm->GetActiveWindow();
    if (!activeWindow) {
      return false;
    }

    return activeWindow == rootWin;
  }

  BrowsingContext* bc = aDoc->GetBrowsingContext();
  if (!bc) {
    return false;
  }

  BrowsingContext* activeBrowsingContext = fm->GetActiveBrowsingContext();
  if (!activeBrowsingContext) {
    return false;
  }

  return activeBrowsingContext == bc->Top();
}

void Document::RemoteFrameFullscreenChanged(Element* aFrameElement) {
  // Ensure the frame element is the fullscreen element in this document.
  // If the frame element is already the fullscreen element in this document,
  // this has no effect.
  auto request = FullscreenRequest::CreateForRemote(aFrameElement);
  RequestFullscreen(std::move(request), XRE_IsContentProcess());
}

void Document::RemoteFrameFullscreenReverted() {
  UniquePtr<FullscreenExit> exit = FullscreenExit::CreateForRemote(this);
  RestorePreviousFullscreenState(std::move(exit));
}

static bool HasFullscreenSubDocument(Document& aDoc) {
  uint32_t count = CountFullscreenSubDocuments(aDoc);
  NS_ASSERTION(count <= 1,
               "Fullscreen docs should have at most 1 fullscreen child!");
  return count >= 1;
}

// Returns nullptr if a request for Fullscreen API is currently enabled
// in the given document. Returns a static string indicates the reason
// why it is not enabled otherwise.
static const char* GetFullscreenError(Document* aDoc, CallerType aCallerType) {
  if (!StaticPrefs::full_screen_api_enabled()) {
    return "FullscreenDeniedDisabled";
  }

  if (aCallerType == CallerType::System) {
    // Chrome code can always use the fullscreen API, provided it's not
    // explicitly disabled.
    return nullptr;
  }

  if (!aDoc->IsVisible()) {
    return "FullscreenDeniedHidden";
  }

  // Ensure that all containing elements are <iframe> and have
  // allowfullscreen attribute set.
  nsCOMPtr<nsIDocShell> docShell(aDoc->GetDocShell());
  if (!docShell || !docShell->GetFullscreenAllowed()) {
    return "FullscreenDeniedContainerNotAllowed";
  }
  return nullptr;
}

bool Document::FullscreenElementReadyCheck(FullscreenRequest& aRequest) {
  Element* elem = aRequest.Element();
  // Strictly speaking, this isn't part of the fullscreen element ready
  // check in the spec, but per steps in the spec, when an element which
  // is already the fullscreen element requests fullscreen, nothing
  // should change and no event should be dispatched, but we still need
  // to resolve the returned promise.
  if (elem == FullscreenStackTop()) {
    aRequest.MayResolvePromise();
    return false;
  }
  if (!elem->IsInComposedDoc()) {
    aRequest.Reject("FullscreenDeniedNotInDocument");
    return false;
  }
  if (elem->OwnerDoc() != this) {
    aRequest.Reject("FullscreenDeniedMovedDocument");
    return false;
  }
  if (!GetWindow()) {
    aRequest.Reject("FullscreenDeniedLostWindow");
    return false;
  }
  if (const char* msg = GetFullscreenError(this, aRequest.mCallerType)) {
    aRequest.Reject(msg);
    return false;
  }
  if (HasFullscreenSubDocument(*this)) {
    aRequest.Reject("FullscreenDeniedSubDocFullScreen");
    return false;
  }
  // XXXsmaug Note, we don't follow the latest fullscreen spec here.
  //         This whole check could be probably removed.
  if (FullscreenStackTop() &&
      !nsContentUtils::ContentIsHostIncludingDescendantOf(
          elem, FullscreenStackTop())) {
    // If this document is fullscreen, only grant fullscreen requests from
    // a descendant of the current fullscreen element.
    aRequest.Reject("FullscreenDeniedNotDescendant");
    return false;
  }
  if (!nsContentUtils::IsChromeDoc(this) && !IsInActiveTab(this)) {
    aRequest.Reject("FullscreenDeniedNotFocusedTab");
    return false;
  }
  // Deny requests when a windowed plugin is focused.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    NS_WARNING("Failed to retrieve focus manager in fullscreen request.");
    aRequest.MayRejectPromise("An unexpected error occurred");
    return false;
  }
  if (nsContentUtils::HasPluginWithUncontrolledEventDispatch(
          fm->GetFocusedElement())) {
    aRequest.Reject("FullscreenDeniedFocusedPlugin");
    return false;
  }
  return true;
}

static nsCOMPtr<nsPIDOMWindowOuter> GetRootWindow(Document* aDoc) {
  nsIDocShell* docShell = aDoc->GetDocShell();
  if (!docShell) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  docShell->GetInProcessRootTreeItem(getter_AddRefs(rootItem));
  return rootItem ? rootItem->GetWindow() : nullptr;
}

static bool ShouldApplyFullscreenDirectly(Document* aDoc,
                                          nsPIDOMWindowOuter* aRootWin) {
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // If we are in the content process, we can apply the fullscreen
    // state directly only if we have been in DOM fullscreen, because
    // otherwise we always need to notify the chrome.
    return !!nsContentUtils::GetRootDocument(aDoc)->GetFullscreenElement();
  } else {
    // If we are in the chrome process, and the window has not been in
    // fullscreen, we certainly need to make that fullscreen first.
    if (!aRootWin->GetFullScreen()) {
      return false;
    }
    // The iterator not being at end indicates there is still some
    // pending fullscreen request relates to this document. We have to
    // push the request to the pending queue so requests are handled
    // in the correct order.
    PendingFullscreenChangeList::Iterator<FullscreenRequest> iter(
        aDoc, PendingFullscreenChangeList::eDocumentsWithSameRoot);
    if (!iter.AtEnd()) {
      return false;
    }
    // We have to apply the fullscreen state directly in this case,
    // because nsGlobalWindow::SetFullscreenInternal() will do nothing
    // if it is already in fullscreen. If we do not apply the state but
    // instead add it to the queue and wait for the window as normal,
    // we would get stuck.
    return true;
  }
}

void Document::RequestFullscreen(UniquePtr<FullscreenRequest> aRequest,
                                 bool applyFullScreenDirectly) {
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = GetRootWindow(this);
  if (!rootWin) {
    aRequest->MayRejectPromise("No active window");
    return;
  }

  if (applyFullScreenDirectly || ShouldApplyFullscreenDirectly(this, rootWin)) {
    ApplyFullscreen(std::move(aRequest));
    return;
  }

  // Per spec only HTML, <svg>, and <math> should be allowed, but
  // we also need to allow XUL elements right now.
  Element* elem = aRequest->Element();
  if (!elem->IsHTMLElement() && !elem->IsXULElement() &&
      !elem->IsSVGElement(nsGkAtoms::svg) &&
      !elem->IsMathMLElement(nsGkAtoms::math)) {
    aRequest->Reject("FullscreenDeniedNotHTMLSVGOrMathML");
    return;
  }

  // We don't need to check element ready before this point, because
  // if we called ApplyFullscreen, it would check that for us.
  if (!FullscreenElementReadyCheck(*aRequest)) {
    return;
  }

  PendingFullscreenChangeList::Add(std::move(aRequest));
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // If we are not the top level process, dispatch an event to make
    // our parent process go fullscreen first.
    nsContentUtils::DispatchEventOnlyToChrome(
        this, ToSupports(this), NS_LITERAL_STRING("MozDOMFullscreen:Request"),
        CanBubble::eYes, Cancelable::eNo, /* DefaultAction */ nullptr);
  } else {
    // Make the window fullscreen.
    rootWin->SetFullscreenInternal(FullscreenReason::ForFullscreenAPI, true);
  }
}

/* static */
bool Document::HandlePendingFullscreenRequests(Document* aDoc) {
  bool handled = false;
  PendingFullscreenChangeList::Iterator<FullscreenRequest> iter(
      aDoc, PendingFullscreenChangeList::eDocumentsWithSameRoot);
  while (!iter.AtEnd()) {
    UniquePtr<FullscreenRequest> request = iter.TakeAndNext();
    Document* doc = request->Document();
    if (doc->ApplyFullscreen(std::move(request))) {
      handled = true;
    }
  }
  return handled;
}

static void ClearPendingFullscreenRequests(Document* aDoc) {
  PendingFullscreenChangeList::Iterator<FullscreenRequest> iter(
      aDoc, PendingFullscreenChangeList::eInclusiveDescendants);
  while (!iter.AtEnd()) {
    UniquePtr<FullscreenRequest> request = iter.TakeAndNext();
    request->MayRejectPromise("Fullscreen request aborted");
  }
}

bool Document::ApplyFullscreen(UniquePtr<FullscreenRequest> aRequest) {
  if (!FullscreenElementReadyCheck(*aRequest)) {
    return false;
  }

  // Stash a reference to any existing fullscreen doc, we'll use this later
  // to detect if the origin which is fullscreen has changed.
  nsCOMPtr<Document> previousFullscreenDoc = GetFullscreenLeaf(this);

  // Stores a list of documents which we must dispatch "fullscreenchange"
  // too. We're required by the spec to dispatch the events in root-to-leaf
  // order, but we traverse the doctree in a leaf-to-root order, so we save
  // references to the documents we must dispatch to so that we get the order
  // as specified.
  AutoTArray<Document*, 8> changed;

  // Remember the root document, so that if a fullscreen document is hidden
  // we can reset fullscreen state in the remaining visible fullscreen
  // documents.
  Document* fullScreenRootDoc = nsContentUtils::GetRootDocument(this);

  // If a document is already in fullscreen, then unlock the mouse pointer
  // before setting a new document to fullscreen
  UnlockPointer();

  // Set the fullscreen element. This sets the fullscreen style on the
  // element, and the fullscreen-ancestor styles on ancestors of the element
  // in this document.
  Element* elem = aRequest->Element();
  DebugOnly<bool> x = FullscreenStackPush(elem);
  NS_ASSERTION(x, "Fullscreen state of requesting doc should always change!");
  // Set the iframe fullscreen flag.
  if (auto* iframe = HTMLIFrameElement::FromNode(elem)) {
    iframe->SetFullscreenFlag(true);
  }
  changed.AppendElement(this);

  // Propagate up the document hierarchy, setting the fullscreen element as
  // the element's container in ancestor documents. This also sets the
  // appropriate css styles as well. Note we don't propagate down the
  // document hierarchy, the fullscreen element (or its container) is not
  // visible there. Stop when we reach the root document.
  Document* child = this;
  while (true) {
    child->SetFullscreenRoot(fullScreenRootDoc);

    // When entering fullscreen, reset the RCD's resolution to the intrinsic
    // resolution, otherwise the fullscreen content could be sized larger than
    // the screen (since fullscreen is implemented using position:fixed and
    // fixed elements are sized to the layout viewport).
    // This also ensures that things like video controls aren't zoomed in
    // when in fullscreen mode.
    if (PresShell* presShell = child->GetPresShell()) {
      if (RefPtr<MobileViewportManager> manager =
              presShell->GetMobileViewportManager()) {
        // Save the previous resolution so it can be restored.
        child->mSavedResolution = presShell->GetResolution();
        presShell->SetResolutionAndScaleTo(
            manager->ComputeIntrinsicResolution(),
            ResolutionChangeOrigin::MainThreadRestore);
      }
    }

    NS_ASSERTION(child->GetFullscreenRoot() == fullScreenRootDoc,
                 "Fullscreen root should be set!");
    if (child == fullScreenRootDoc) {
      break;
    }
    Document* parent = child->GetInProcessParentDocument();
    Element* element = parent->FindContentForSubDocument(child);
    if (parent->FullscreenStackPush(element)) {
      changed.AppendElement(parent);
      child = parent;
    } else {
      // We've reached either the root, or a point in the doctree where the
      // new fullscreen element container is the same as the previous
      // fullscreen element's container. No more changes need to be made
      // to the fullscreen stacks of documents further up the tree.
      break;
    }
  }

  FullscreenRoots::Add(this);

  // If it is the first entry of the fullscreen, trigger an event so
  // that the UI can response to this change, e.g. hide chrome, or
  // notifying parent process to enter fullscreen. Note that chrome
  // code may also want to listen to MozDOMFullscreen:NewOrigin event
  // to pop up warning UI.
  if (!previousFullscreenDoc) {
    nsContentUtils::DispatchEventOnlyToChrome(
        this, ToSupports(elem), NS_LITERAL_STRING("MozDOMFullscreen:Entered"),
        CanBubble::eYes, Cancelable::eNo, /* DefaultAction */ nullptr);
  }

  // The origin which is fullscreen gets changed. Trigger an event so
  // that the chrome knows to pop up a warning UI. Note that
  // previousFullscreenDoc == nullptr upon first entry, so we always
  // take this path on the first entry. Also note that, in a multi-
  // process browser, the code in content process is responsible for
  // sending message with the origin to its parent, and the parent
  // shouldn't rely on this event itself.
  if (aRequest->mShouldNotifyNewOrigin &&
      !nsContentUtils::HaveEqualPrincipals(previousFullscreenDoc, this)) {
    DispatchFullscreenNewOriginEvent(this);
  }

  // Dispatch "fullscreenchange" events. Note that the loop order is
  // reversed so that events are dispatched in the tree order as
  // indicated in the spec.
  for (Document* d : Reversed(changed)) {
    DispatchFullscreenChange(*d, d->FullscreenStackTop());
  }
  aRequest->MayResolvePromise();
  return true;
}

bool Document::FullscreenEnabled(CallerType aCallerType) {
  return !GetFullscreenError(this, aCallerType);
}

void Document::ClearOrientationPendingPromise() {
  mOrientationPendingPromise = nullptr;
}

bool Document::SetOrientationPendingPromise(Promise* aPromise) {
  if (mIsGoingAway) {
    return false;
  }

  mOrientationPendingPromise = aPromise;
  return true;
}

static void DispatchPointerLockChange(Document* aTarget) {
  if (!aTarget) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(aTarget, NS_LITERAL_STRING("pointerlockchange"),
                               CanBubble::eYes, ChromeOnlyDispatch::eNo);
  asyncDispatcher->PostDOMEvent();
}

static void DispatchPointerLockError(Document* aTarget, const char* aMessage) {
  if (!aTarget) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(aTarget, NS_LITERAL_STRING("pointerlockerror"),
                               CanBubble::eYes, ChromeOnlyDispatch::eNo);
  asyncDispatcher->PostDOMEvent();
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DOM"), aTarget,
                                  nsContentUtils::eDOM_PROPERTIES, aMessage);
}

class PointerLockRequest final : public Runnable {
 public:
  PointerLockRequest(Element* aElement, bool aUserInputOrChromeCaller)
      : mozilla::Runnable("PointerLockRequest"),
        mElement(do_GetWeakReference(aElement)),
        mDocument(do_GetWeakReference(aElement->OwnerDoc())),
        mUserInputOrChromeCaller(aUserInputOrChromeCaller) {}

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() final;

 private:
  nsWeakPtr mElement;
  nsWeakPtr mDocument;
  bool mUserInputOrChromeCaller;
};

static const char* GetPointerLockError(Element* aElement, Element* aCurrentLock,
                                       bool aNoFocusCheck = false) {
  // Check if pointer lock pref is enabled
  if (!Preferences::GetBool("full-screen-api.pointer-lock.enabled")) {
    return "PointerLockDeniedDisabled";
  }

  nsCOMPtr<Document> ownerDoc = aElement->OwnerDoc();
  if (aCurrentLock && aCurrentLock->OwnerDoc() != ownerDoc) {
    return "PointerLockDeniedInUse";
  }

  if (!aElement->IsInComposedDoc()) {
    return "PointerLockDeniedNotInDocument";
  }

  if (ownerDoc->GetSandboxFlags() & SANDBOXED_POINTER_LOCK) {
    return "PointerLockDeniedSandboxed";
  }

  // Check if the element is in a document with a docshell.
  if (!ownerDoc->GetContainer()) {
    return "PointerLockDeniedHidden";
  }
  nsCOMPtr<nsPIDOMWindowOuter> ownerWindow = ownerDoc->GetWindow();
  if (!ownerWindow) {
    return "PointerLockDeniedHidden";
  }
  nsCOMPtr<nsPIDOMWindowInner> ownerInnerWindow = ownerDoc->GetInnerWindow();
  if (!ownerInnerWindow) {
    return "PointerLockDeniedHidden";
  }
  if (ownerWindow->GetCurrentInnerWindow() != ownerInnerWindow) {
    return "PointerLockDeniedHidden";
  }

  nsCOMPtr<nsPIDOMWindowOuter> top = ownerWindow->GetInProcessScriptableTop();
  if (!top || !top->GetExtantDoc() || top->GetExtantDoc()->Hidden()) {
    return "PointerLockDeniedHidden";
  }

  if (!aNoFocusCheck) {
    mozilla::ErrorResult rv;
    if (!top->GetExtantDoc()->HasFocus(rv)) {
      return "PointerLockDeniedNotFocused";
    }
  }

  return nullptr;
}

static void ChangePointerLockedElement(Element* aElement, Document* aDocument,
                                       Element* aPointerLockedElement) {
  // aDocument here is not really necessary, as it is the uncomposed
  // document of both aElement and aPointerLockedElement as far as one
  // is not nullptr, and they wouldn't both be nullptr in any case.
  // But since the caller of this function should have known what the
  // document is, we just don't try to figure out what it should be.
  MOZ_ASSERT(aDocument);
  MOZ_ASSERT(aElement != aPointerLockedElement);
  if (aPointerLockedElement) {
    MOZ_ASSERT(aPointerLockedElement->GetComposedDoc() == aDocument);
    aPointerLockedElement->ClearPointerLock();
  }
  if (aElement) {
    MOZ_ASSERT(aElement->GetComposedDoc() == aDocument);
    aElement->SetPointerLock();
    EventStateManager::sPointerLockedElement = do_GetWeakReference(aElement);
    EventStateManager::sPointerLockedDoc = do_GetWeakReference(aDocument);
    NS_ASSERTION(EventStateManager::sPointerLockedElement &&
                     EventStateManager::sPointerLockedDoc,
                 "aElement and this should support weak references!");
  } else {
    EventStateManager::sPointerLockedElement = nullptr;
    EventStateManager::sPointerLockedDoc = nullptr;
  }
  // Retarget all events to aElement via capture or
  // stop retargeting if aElement is nullptr.
  PresShell::SetCapturingContent(aElement, CaptureFlags::PointerLock);
  DispatchPointerLockChange(aDocument);
}

NS_IMETHODIMP
PointerLockRequest::Run() {
  nsCOMPtr<Element> e = do_QueryReferent(mElement);
  nsCOMPtr<Document> doc = do_QueryReferent(mDocument);
  const char* error = nullptr;
  if (!e || !doc || !e->GetComposedDoc()) {
    error = "PointerLockDeniedNotInDocument";
  } else if (e->GetComposedDoc() != doc) {
    error = "PointerLockDeniedMovedDocument";
  }
  if (!error) {
    nsCOMPtr<Element> pointerLockedElement =
        do_QueryReferent(EventStateManager::sPointerLockedElement);
    if (e == pointerLockedElement) {
      DispatchPointerLockChange(doc);
      return NS_OK;
    }
    // Note, we must bypass focus change, so pass true as the last parameter!
    error = GetPointerLockError(e, pointerLockedElement, true);
    // Another element in the same document is requesting pointer lock,
    // just grant it without user input check.
    if (!error && pointerLockedElement) {
      ChangePointerLockedElement(e, doc, pointerLockedElement);
      return NS_OK;
    }
  }
  // If it is neither user input initiated, nor requested in fullscreen,
  // it should be rejected.
  if (!error && !mUserInputOrChromeCaller && !doc->GetFullscreenElement()) {
    error = "PointerLockDeniedNotInputDriven";
  }
  if (!error && !doc->SetPointerLock(e, StyleCursorKind::None)) {
    error = "PointerLockDeniedFailedToLock";
  }
  if (error) {
    DispatchPointerLockError(doc, error);
    return NS_OK;
  }

  ChangePointerLockedElement(e, doc, nullptr);
  nsContentUtils::DispatchEventOnlyToChrome(
      doc, ToSupports(e), NS_LITERAL_STRING("MozDOMPointerLock:Entered"),
      CanBubble::eYes, Cancelable::eNo, /* DefaultAction */ nullptr);
  return NS_OK;
}

void Document::RequestPointerLock(Element* aElement, CallerType aCallerType) {
  NS_ASSERTION(aElement,
               "Must pass non-null element to Document::RequestPointerLock");

  nsCOMPtr<Element> pointerLockedElement =
      do_QueryReferent(EventStateManager::sPointerLockedElement);
  if (aElement == pointerLockedElement) {
    DispatchPointerLockChange(this);
    return;
  }

  if (const char* msg = GetPointerLockError(aElement, pointerLockedElement)) {
    DispatchPointerLockError(this, msg);
    return;
  }

  bool userInputOrSystemCaller = UserActivation::IsHandlingUserInput() ||
                                 aCallerType == CallerType::System;
  nsCOMPtr<nsIRunnable> request =
      new PointerLockRequest(aElement, userInputOrSystemCaller);
  Dispatch(TaskCategory::Other, request.forget());
}

bool Document::SetPointerLock(Element* aElement, StyleCursorKind aCursorStyle) {
  MOZ_ASSERT(!aElement || aElement->OwnerDoc() == this,
             "We should be either unlocking pointer (aElement is nullptr), "
             "or locking pointer to an element in this document");
#ifdef DEBUG
  if (!aElement) {
    nsCOMPtr<Document> pointerLockedDoc =
        do_QueryReferent(EventStateManager::sPointerLockedDoc);
    MOZ_ASSERT(pointerLockedDoc == this);
  }
#endif

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    NS_WARNING("SetPointerLock(): No PresShell");
    if (!aElement) {
      // If we are unlocking pointer lock, but for some reason the doc
      // has already detached from the presshell, just ask the event
      // state manager to release the pointer.
      EventStateManager::SetPointerLock(nullptr, nullptr);
      return true;
    }
    return false;
  }
  nsPresContext* presContext = presShell->GetPresContext();
  if (!presContext) {
    NS_WARNING("SetPointerLock(): Unable to get PresContext");
    return false;
  }

  nsCOMPtr<nsIWidget> widget;
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!NS_WARN_IF(!rootFrame)) {
    widget = rootFrame->GetNearestWidget();
    NS_WARNING_ASSERTION(widget,
                         "SetPointerLock(): Unable to find widget in "
                         "presShell->GetRootFrame()->GetNearestWidget();");
    if (aElement && !widget) {
      return false;
    }
  }

  // Hide the cursor and set pointer lock for future mouse events
  RefPtr<EventStateManager> esm = presContext->EventStateManager();
  esm->SetCursor(aCursorStyle, nullptr, Nothing(), widget, true);
  EventStateManager::SetPointerLock(widget, aElement);

  return true;
}

void Document::UnlockPointer(Document* aDoc) {
  if (!EventStateManager::sIsPointerLocked) {
    return;
  }

  nsCOMPtr<Document> pointerLockedDoc =
      do_QueryReferent(EventStateManager::sPointerLockedDoc);
  if (!pointerLockedDoc || (aDoc && aDoc != pointerLockedDoc)) {
    return;
  }
  if (!pointerLockedDoc->SetPointerLock(nullptr, StyleCursorKind::Auto)) {
    return;
  }

  nsCOMPtr<Element> pointerLockedElement =
      do_QueryReferent(EventStateManager::sPointerLockedElement);
  ChangePointerLockedElement(nullptr, pointerLockedDoc, pointerLockedElement);

  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      pointerLockedElement, NS_LITERAL_STRING("MozDOMPointerLock:Exited"),
      CanBubble::eYes, ChromeOnlyDispatch::eYes);
  asyncDispatcher->RunDOMEventWhenSafe();
}

void Document::UpdateVisibilityState() {
  dom::VisibilityState oldState = mVisibilityState;
  mVisibilityState = ComputeVisibilityState();
  if (oldState != mVisibilityState) {
    nsContentUtils::DispatchTrustedEvent(this, ToSupports(this),
                                         NS_LITERAL_STRING("visibilitychange"),
                                         CanBubble::eYes, Cancelable::eNo);
    EnumerateActivityObservers(NotifyActivityChanged);
  }

  if (mVisibilityState == dom::VisibilityState::Visible) {
    MaybeActiveMediaComponents();
  }
}

VisibilityState Document::ComputeVisibilityState() const {
  // We have to check a few pieces of information here:
  // 1)  Are we in bfcache (!IsVisible())?  If so, nothing else matters.
  // 2)  Do we have an outer window?  If not, we're hidden.  Note that we don't
  //     want to use GetWindow here because it does weird groveling for windows
  //     in some cases.
  // 3)  Is our outer window background?  If so, we're hidden.
  // Otherwise, we're visible.
  if (!IsVisible() || !mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->IsBackground()) {
    return dom::VisibilityState::Hidden;
  }

  return dom::VisibilityState::Visible;
}

void Document::PostVisibilityUpdateEvent() {
  nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod("Document::UpdateVisibilityState", this,
                        &Document::UpdateVisibilityState);
  Dispatch(TaskCategory::Other, event.forget());
}

void Document::MaybeActiveMediaComponents() {
  if (!mWindow) {
    return;
  }

  GetWindow()->MaybeActiveMediaComponents();
}

void Document::DocAddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const {
  nsINode::AddSizeOfExcludingThis(aWindowSizes, &aWindowSizes.mDOMOtherSize);

  for (nsIContent* kid = GetFirstChild(); kid; kid = kid->GetNextSibling()) {
    AddSizeOfNodeTree(*kid, aWindowSizes);
  }

  // IMPORTANT: for our ComputedValues measurements, we want to measure
  // ComputedValues accessible from DOM elements before ComputedValues not
  // accessible from DOM elements (i.e. accessible only from the frame tree).
  //
  // Therefore, the measurement of the Document superclass must happen after
  // the measurement of DOM nodes (above), because Document contains the
  // PresShell, which contains the frame tree.
  if (mPresShell) {
    mPresShell->AddSizeOfIncludingThis(aWindowSizes);
  }

  mStyleSet->AddSizeOfIncludingThis(aWindowSizes);

  aWindowSizes.mPropertyTablesSize +=
      mPropertyTable.SizeOfExcludingThis(aWindowSizes.mState.mMallocSizeOf);

  if (EventListenerManager* elm = GetExistingListenerManager()) {
    aWindowSizes.mDOMEventListenersCount += elm->ListenerCount();
  }

  if (mNodeInfoManager) {
    mNodeInfoManager->AddSizeOfIncludingThis(aWindowSizes);
  }

  aWindowSizes.mDOMMediaQueryLists += mDOMMediaQueryLists.sizeOfExcludingThis(
      aWindowSizes.mState.mMallocSizeOf);

  for (const MediaQueryList* mql : mDOMMediaQueryLists) {
    aWindowSizes.mDOMMediaQueryLists +=
        mql->SizeOfExcludingThis(aWindowSizes.mState.mMallocSizeOf);
  }

  DocumentOrShadowRoot::AddSizeOfExcludingThis(aWindowSizes);

  for (auto& sheetArray : mAdditionalSheets) {
    AddSizeOfOwnedSheetArrayExcludingThis(aWindowSizes, sheetArray);
  }
  // Lumping in the loader with the style-sheets size is not ideal,
  // but most of the things in there are in fact stylesheets, so it
  // doesn't seem worthwhile to separate it out.
  // This can be null if we've already been unlinked.
  if (mCSSLoader) {
    aWindowSizes.mLayoutStyleSheetsSize +=
        mCSSLoader->SizeOfIncludingThis(aWindowSizes.mState.mMallocSizeOf);
  }

  if (mResizeObserverController) {
    mResizeObserverController->AddSizeOfIncludingThis(aWindowSizes);
  }

  aWindowSizes.mDOMOtherSize += mAttrStyleSheet
                                    ? mAttrStyleSheet->DOMSizeOfIncludingThis(
                                          aWindowSizes.mState.mMallocSizeOf)
                                    : 0;

  aWindowSizes.mDOMOtherSize += mStyledLinks.ShallowSizeOfExcludingThis(
      aWindowSizes.mState.mMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mMidasCommandManager
  // - many!
}

void Document::DocAddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const {
  aWindowSizes.mDOMOtherSize += aWindowSizes.mState.mMallocSizeOf(this);
  DocAddSizeOfExcludingThis(aWindowSizes);
}

void Document::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                      size_t* aNodeSize) const {
  // This AddSizeOfExcludingThis() overrides the one from nsINode.  But
  // nsDocuments can only appear at the top of the DOM tree, and we use the
  // specialized DocAddSizeOfExcludingThis() in that case.  So this should never
  // be called.
  MOZ_CRASH();
}

/* static */
void Document::AddSizeOfNodeTree(nsINode& aNode, nsWindowSizes& aWindowSizes) {
  size_t nodeSize = 0;
  aNode.AddSizeOfIncludingThis(aWindowSizes, &nodeSize);

  // This is where we transfer the nodeSize obtained from
  // nsINode::AddSizeOfIncludingThis() to a value in nsWindowSizes.
  switch (aNode.NodeType()) {
    case nsINode::ELEMENT_NODE:
      aWindowSizes.mDOMElementNodesSize += nodeSize;
      break;
    case nsINode::TEXT_NODE:
      aWindowSizes.mDOMTextNodesSize += nodeSize;
      break;
    case nsINode::CDATA_SECTION_NODE:
      aWindowSizes.mDOMCDATANodesSize += nodeSize;
      break;
    case nsINode::COMMENT_NODE:
      aWindowSizes.mDOMCommentNodesSize += nodeSize;
      break;
    default:
      aWindowSizes.mDOMOtherSize += nodeSize;
      break;
  }

  if (EventListenerManager* elm = aNode.GetExistingListenerManager()) {
    aWindowSizes.mDOMEventListenersCount += elm->ListenerCount();
  }

  if (aNode.IsContent()) {
    nsTArray<nsIContent*> anonKids;
    nsContentUtils::AppendNativeAnonymousChildren(aNode.AsContent(), anonKids,
                                                  nsIContent::eAllChildren);
    for (nsIContent* anonKid : anonKids) {
      AddSizeOfNodeTree(*anonKid, aWindowSizes);
    }

    if (auto* element = Element::FromNode(aNode)) {
      if (ShadowRoot* shadow = element->GetShadowRoot()) {
        AddSizeOfNodeTree(*shadow, aWindowSizes);
      }
    }
  }

  // NOTE(emilio): If you feel smart and want to change this function to use
  // GetNextNode(), think twice, since you'd need to handle <xbl:content> in a
  // sane way, and kids of <content> won't point to the parent, so we'd never
  // find the root node where we should stop at.
  for (nsIContent* kid = aNode.GetFirstChild(); kid;
       kid = kid->GetNextSibling()) {
    AddSizeOfNodeTree(*kid, aWindowSizes);
  }
}

already_AddRefed<Document> Document::Constructor(const GlobalObject& aGlobal,
                                                 ErrorResult& rv) {
  nsCOMPtr<nsIScriptGlobalObject> global =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> prin =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!prin) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank");
  if (!uri) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsCOMPtr<Document> doc;
  nsresult res = NS_NewDOMDocument(
      getter_AddRefs(doc), VoidString(), EmptyString(), nullptr, uri, uri,
      prin->GetPrincipal(), true, global, DocumentFlavorPlain);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return nullptr;
  }

  doc->SetReadyStateInternal(Document::READYSTATE_COMPLETE);

  return doc.forget();
}

XPathExpression* Document::CreateExpression(const nsAString& aExpression,
                                            XPathNSResolver* aResolver,
                                            ErrorResult& rv) {
  return XPathEvaluator()->CreateExpression(aExpression, aResolver, rv);
}

nsINode* Document::CreateNSResolver(nsINode& aNodeResolver) {
  return XPathEvaluator()->CreateNSResolver(aNodeResolver);
}

already_AddRefed<XPathResult> Document::Evaluate(
    JSContext* aCx, const nsAString& aExpression, nsINode& aContextNode,
    XPathNSResolver* aResolver, uint16_t aType, JS::Handle<JSObject*> aResult,
    ErrorResult& rv) {
  return XPathEvaluator()->Evaluate(aCx, aExpression, aContextNode, aResolver,
                                    aType, aResult, rv);
}

already_AddRefed<nsIAppWindow> Document::GetAppWindowIfToplevelChrome() const {
  nsCOMPtr<nsIDocShellTreeItem> item = GetDocShell();
  if (!item) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShellTreeOwner> owner;
  item->GetTreeOwner(getter_AddRefs(owner));
  nsCOMPtr<nsIAppWindow> appWin = do_GetInterface(owner);
  if (!appWin) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShell> appWinShell;
  appWin->GetDocShell(getter_AddRefs(appWinShell));
  if (!SameCOMIdentity(appWinShell, item)) {
    return nullptr;
  }
  return appWin.forget();
}

Document* Document::GetTopLevelContentDocument() {
  Document* parent;

  if (!mLoadedAsData) {
    parent = this;
  } else {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetScopeObject());
    if (!window) {
      return nullptr;
    }

    parent = window->GetExtantDoc();
    if (!parent) {
      return nullptr;
    }
  }

  do {
    if (parent->IsTopLevelContentDocument()) {
      break;
    }

    // If we ever have a non-content parent before we hit a toplevel content
    // parent, then we're never going to find one.  Just bail.
    if (!parent->IsContentDocument()) {
      return nullptr;
    }

    parent = parent->GetInProcessParentDocument();
  } while (parent);

  return parent;
}

const Document* Document::GetTopLevelContentDocument() const {
  const Document* parent;

  if (!mLoadedAsData) {
    parent = this;
  } else {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetScopeObject());
    if (!window) {
      return nullptr;
    }

    parent = window->GetExtantDoc();
    if (!parent) {
      return nullptr;
    }
  }

  do {
    if (parent->IsTopLevelContentDocument()) {
      break;
    }

    // If we ever have a non-content parent before we hit a toplevel content
    // parent, then we're never going to find one.  Just bail.
    if (!parent->IsContentDocument()) {
      return nullptr;
    }

    parent = parent->GetInProcessParentDocument();
  } while (parent);

  return parent;
}

void Document::PropagateUseCounters(Document* aParentDocument) {
  MOZ_ASSERT(this != aParentDocument);

  // Don't count chrome resources, even in the web content.
  if (NodePrincipal()->SchemeIs("chrome")) {
    return;
  }

  // What really matters here is that our use counters get propagated as
  // high up in the content document hierarchy as possible.  So,
  // starting with aParentDocument, we need to find the toplevel content
  // document, and propagate our use counters into its
  // mChildDocumentUseCounters.
  Document* contentParent = aParentDocument->GetTopLevelContentDocument();
  if (!contentParent) {
    return;
  }

  SetCssUseCounterBits();
  contentParent->mChildDocumentUseCounters |= mUseCounters;
  contentParent->mChildDocumentUseCounters |= mChildDocumentUseCounters;
}

bool Document::HasScriptsBlockedBySandbox() {
  return mSandboxFlags & SANDBOXED_SCRIPTS;
}

// Some use-counter sanity-checking.
static_assert(size_t(eUseCounter_EndCSSProperties) -
                      size_t(eUseCounter_FirstCSSProperty) ==
                  size_t(eCSSProperty_COUNT_with_aliases),
              "We should have the right amount of CSS property use counters");
static_assert(size_t(eUseCounter_Count) -
                      size_t(eUseCounter_FirstCountedUnknownProperty) ==
                  size_t(CountedUnknownProperty::Count),
              "We should have the right amount of counted unknown properties"
              " use counters");
static_assert(size_t(eUseCounter_Count) * 2 ==
                  size_t(Telemetry::HistogramUseCounterCount),
              "There should be two histograms (document and page)"
              " for each use counter");

#define ASSERT_CSS_COUNTER(id_, method_)                        \
  static_assert(size_t(eUseCounter_property_##method_) -        \
                        size_t(eUseCounter_FirstCSSProperty) == \
                    size_t(id_),                                \
                "Order for CSS counters and CSS property id should match");
#define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) privatename_
#define CSS_PROP_LONGHAND(name_, id_, method_, ...) \
  ASSERT_CSS_COUNTER(eCSSProperty_##id_, method_)
#define CSS_PROP_SHORTHAND(name_, id_, method_, ...) \
  ASSERT_CSS_COUNTER(eCSSProperty_##id_, method_)
#define CSS_PROP_ALIAS(name_, aliasid_, id_, method_, ...) \
  ASSERT_CSS_COUNTER(eCSSPropertyAlias_##aliasid_, method_)
#include "mozilla/ServoCSSPropList.h"
#undef CSS_PROP_ALIAS
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LONGHAND
#undef CSS_PROP_PUBLIC_OR_PRIVATE
#undef ASSERT_CSS_COUNTER

void Document::SetCssUseCounterBits() {
  auto* docCounters = mStyleUseCounters.get();
  if (!docCounters) {
    return;
  }

  if (StaticPrefs::layout_css_use_counters_enabled()) {
    for (size_t i = 0; i < eCSSProperty_COUNT_with_aliases; ++i) {
      auto id = nsCSSPropertyID(i);
      if (Servo_IsPropertyIdRecordedInUseCounter(docCounters, id)) {
        SetUseCounter(nsCSSProps::UseCounterFor(id));
      }
    }
  }

  if (StaticPrefs::layout_css_use_counters_unimplemented_enabled()) {
    for (size_t i = 0; i < size_t(CountedUnknownProperty::Count); ++i) {
      auto id = CountedUnknownProperty(i);
      if (Servo_IsUnknownPropertyRecordedInUseCounter(docCounters, id)) {
        SetUseCounter(UseCounter(eUseCounter_FirstCountedUnknownProperty + i));
      }
    }
  }
}

void Document::PropagateUseCountersToPage() {
  if (mDisplayDocument) {
    // If we are a resource document, we won't have a docshell and so we won't
    // record any page use counters on this document.  Instead, we should
    // forward it up to the document that loaded us.
    MOZ_ASSERT(!mDocumentContainer);
    return PropagateUseCounters(mDisplayDocument);
  }

  if (IsBeingUsedAsImage()) {
    // If this is an SVG image document, we also won't have a docshell.
    //
    // But we do report the use counters via PropagateUseCounters() from the
    // image code.
    MOZ_ASSERT(!mDocumentContainer);
    return;
  }

  // We only care about use counters in content.  If we're already a toplevel
  // content document, then we should have already set the use counter on
  // ourselves, and we are done.
  Document* contentParent = GetTopLevelContentDocument();
  if (!contentParent || this == contentParent) {
    return;
  }

  PropagateUseCounters(contentParent);
}

void Document::ReportUseCounters() {
  static const bool kDebugUseCounters = false;

  if (mReportedUseCounters) {
    return;
  }

  mReportedUseCounters = true;
  SetCssUseCounterBits();

  // Call ReportUseCounters in all our outstanding subdocuments and resources
  // and such. This needs to be here so that all our sub documents propagate our
  // counters to us if needed.
  //
  // We need to do this explicitly (rather than, e.g., moving the
  // ReportUseCounters() call after DestroyContent(), which destroys the frame
  // loaders) because subdocument destruction may be (and is generally) async
  // (why?), and thus we have no guarantee of Document::Destroy being called on
  // children before us. Children will just early-return above.
  //
  // This is a bit racy in the sense that we may miss some of the children use
  // counters that happen in between, but that isn't probably a huge deal at
  // this point where we're destroying the parent document already.
  //
  // An alternative would be to move the ReportUseCounters() call to a script
  // runner or something after calling DestroyContent(), but that looks a bit
  // fishy as well.
  {
    if (mSubDocuments) {
      for (auto iter = mSubDocuments->ConstIter(); !iter.Done(); iter.Next()) {
        auto* entry = static_cast<SubDocMapEntry*>(iter.Get());
        entry->mSubDocument->ReportUseCounters();
      }
    }
    if (Document* doc = GetLatestStaticClone()) {
      doc->ReportUseCounters();
    }
    EnumerateExternalResources([](Document& aDoc) {
      aDoc.ReportUseCounters();
      return CallState::Continue;
    });
  }

  PropagateUseCountersToPage();

  if (Telemetry::HistogramUseCounterCount > 0 &&
      (IsContentDocument() || IsResourceDoc())) {
    nsCOMPtr<nsIURI> uri;
    NodePrincipal()->GetURI(getter_AddRefs(uri));
    if (!uri || uri->SchemeIs("about") || uri->SchemeIs("chrome") ||
        uri->SchemeIs("resource")) {
      return;
    }

    if (kDebugUseCounters) {
      nsCString spec = uri->GetSpecOrDefault();

      // URIs can be rather long for data documents, so truncate them to
      // some reasonable length.
      spec.Truncate(std::min(128U, spec.Length()));
      printf("-- Use counters for %s --\n", spec.get());
    }

    // We keep separate counts for individual documents and top-level
    // pages to more accurately track how many web pages might break if
    // certain features were removed.  Consider the case of a single
    // HTML document with several SVG images and/or iframes with
    // sub-documents of their own.  If we maintained a single set of use
    // counters and all the sub-documents use a particular feature, then
    // telemetry would indicate that we would be breaking N documents if
    // that feature were removed.  Whereas with a document/top-level
    // page split, we can see that N documents would be affected, but
    // only a single web page would be affected.

    // The difference between the values of these two histograms and the
    // related use counters below tell us how many pages did *not* use
    // the feature in question.  For instance, if we see that a given
    // session has destroyed 30 content documents, but a particular use
    // counter shows only a count of 5, we can infer that the use
    // counter was *not* used in 25 of those 30 documents.
    //
    // We do things this way, rather than accumulating a boolean flag
    // for each use counter, to avoid sending histograms for features
    // that don't get widely used.  Doing things in this fashion means
    // smaller telemetry payloads and faster processing on the server
    // side.
    Telemetry::Accumulate(Telemetry::CONTENT_DOCUMENTS_DESTROYED, 1);
    if (IsTopLevelContentDocument()) {
      Telemetry::Accumulate(Telemetry::TOP_LEVEL_CONTENT_DOCUMENTS_DESTROYED,
                            1);
    }

    for (int32_t c = 0; c < eUseCounter_Count; ++c) {
      UseCounter uc = static_cast<UseCounter>(c);

      Telemetry::HistogramID id = static_cast<Telemetry::HistogramID>(
          Telemetry::HistogramFirstUseCounter + uc * 2);
      bool value = GetUseCounter(uc);

      if (value) {
        if (kDebugUseCounters) {
          const char* name = Telemetry::GetHistogramName(id);
          if (name) {
            printf("  %s", name);
          } else {
            printf("  #%d", id);
          }
          printf(": %d\n", value);
        }

        Telemetry::Accumulate(id, 1);
      }

      if (IsTopLevelContentDocument()) {
        id = static_cast<Telemetry::HistogramID>(
            Telemetry::HistogramFirstUseCounter + uc * 2 + 1);
        value = GetUseCounter(uc) || GetChildDocumentUseCounter(uc);

        if (value) {
          if (kDebugUseCounters) {
            const char* name = Telemetry::GetHistogramName(id);
            if (name) {
              printf("  %s", name);
            } else {
              printf("  #%d", id);
            }
            printf(": %d\n", value);
          }

          Telemetry::Accumulate(id, 1);
        }
      }
    }
  }

  if (IsTopLevelContentDocument()) {
    CSSIntCoord adjustmentLength =
        CSSPixel::FromAppUnits(mScrollAnchorAdjustmentLength).Rounded();
    Telemetry::Accumulate(Telemetry::SCROLL_ANCHOR_ADJUSTMENT_LENGTH,
                          adjustmentLength);
    Telemetry::Accumulate(Telemetry::SCROLL_ANCHOR_ADJUSTMENT_COUNT,
                          mScrollAnchorAdjustmentCount);
  }
}

void Document::UpdateIntersectionObservations() {
  if (mIntersectionObservers.IsEmpty()) {
    return;
  }

  DOMHighResTimeStamp time = 0;
  if (nsPIDOMWindowInner* window = GetInnerWindow()) {
    Performance* perf = window->GetPerformance();
    if (perf) {
      time = perf->Now();
    }
  }
  nsTArray<RefPtr<DOMIntersectionObserver>> observers(
      mIntersectionObservers.Count());
  for (auto iter = mIntersectionObservers.Iter(); !iter.Done(); iter.Next()) {
    DOMIntersectionObserver* observer = iter.Get()->GetKey();
    observers.AppendElement(observer);
  }
  for (const auto& observer : observers) {
    if (observer) {
      observer->Update(this, time);
    }
  }
}

void Document::ScheduleIntersectionObserverNotification() {
  if (mIntersectionObservers.IsEmpty()) {
    return;
  }
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> notification =
      NewRunnableMethod("Document::NotifyIntersectionObservers", this,
                        &Document::NotifyIntersectionObservers);
  Dispatch(TaskCategory::Other, notification.forget());
}

void Document::NotifyIntersectionObservers() {
  nsTArray<RefPtr<DOMIntersectionObserver>> observers(
      mIntersectionObservers.Count());
  for (auto iter = mIntersectionObservers.Iter(); !iter.Done(); iter.Next()) {
    DOMIntersectionObserver* observer = iter.Get()->GetKey();
    observers.AppendElement(observer);
  }
  for (const auto& observer : observers) {
    if (observer) {
      // MOZ_KnownLive because 'observers' is guaranteed to
      // keep it alive.
      //
      // Even with https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 fixed
      // this might need to stay, because 'observers' is not const, so it's not
      // obvious how to prove via static analysis that it won't change and
      // release us.
      MOZ_KnownLive(observer)->Notify();
    }
  }
}

DOMIntersectionObserver& Document::EnsureLazyLoadImageObserver() {
  if (!mLazyLoadImageObserver) {
    mLazyLoadImageObserver =
        DOMIntersectionObserver::CreateLazyLoadObserver(*this);
  }
  return *mLazyLoadImageObserver;
}

void Document::NotifyLayerManagerRecreated() {
  EnumerateActivityObservers(NotifyActivityChanged);
  EnumerateSubDocuments([](Document& aSubDoc) {
    aSubDoc.NotifyLayerManagerRecreated();
    return CallState::Continue;
  });
}

XPathEvaluator* Document::XPathEvaluator() {
  if (!mXPathEvaluator) {
    mXPathEvaluator.reset(new dom::XPathEvaluator(this));
  }
  return mXPathEvaluator.get();
}

already_AddRefed<nsIDocumentEncoder> Document::GetCachedEncoder() {
  return mCachedEncoder.forget();
}

void Document::SetCachedEncoder(already_AddRefed<nsIDocumentEncoder> aEncoder) {
  mCachedEncoder = aEncoder;
}

void Document::SetContentTypeInternal(const nsACString& aType) {
  if (!IsHTMLOrXHTML() && mDefaultElementType == kNameSpaceID_None &&
      aType.EqualsLiteral("application/xhtml+xml")) {
    mDefaultElementType = kNameSpaceID_XHTML;
  }

  mCachedEncoder = nullptr;
  mContentType = aType;
}

nsILoadContext* Document::GetLoadContext() const { return mDocumentContainer; }

nsIDocShell* Document::GetDocShell() const { return mDocumentContainer; }

void Document::SetStateObject(nsIStructuredCloneContainer* scContainer) {
  mStateObjectContainer = scContainer;
  mStateObjectCached = nullptr;
}

Document::DocumentTheme Document::GetDocumentLWTheme() {
  if (mDocLWTheme == Doc_Theme_Uninitialized) {
    mDocLWTheme = ThreadSafeGetDocumentLWTheme();
  }
  return mDocLWTheme;
}

Document::DocumentTheme Document::ThreadSafeGetDocumentLWTheme() const {
  if (!NodePrincipal()->IsSystemPrincipal()) {
    return Doc_Theme_None;
  }

  if (mDocLWTheme != Doc_Theme_Uninitialized) {
    return mDocLWTheme;
  }

  DocumentTheme theme = Doc_Theme_None;  // No lightweight theme by default
  Element* element = GetRootElement();
  if (element && element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::lwtheme,
                                      nsGkAtoms::_true, eCaseMatters)) {
    theme = Doc_Theme_Neutral;
    nsAutoString lwTheme;
    element->GetAttr(kNameSpaceID_None, nsGkAtoms::lwthemetextcolor, lwTheme);
    if (lwTheme.EqualsLiteral("dark")) {
      theme = Doc_Theme_Dark;
    } else if (lwTheme.EqualsLiteral("bright")) {
      theme = Doc_Theme_Bright;
    }
  }

  return theme;
}

already_AddRefed<Element> Document::CreateHTMLElement(nsAtom* aTag) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(aTag, nullptr, kNameSpaceID_XHTML,
                                           ELEMENT_NODE);
  MOZ_ASSERT(nodeInfo, "GetNodeInfo should never fail");

  nsCOMPtr<Element> element;
  DebugOnly<nsresult> rv =
      NS_NewHTMLElement(getter_AddRefs(element), nodeInfo.forget(),
                        mozilla::dom::NOT_FROM_PARSER);

  MOZ_ASSERT(NS_SUCCEEDED(rv), "NS_NewHTMLElement should never fail");
  return element.forget();
}

static CallState MarkDocumentTreeToBeInSyncOperation(
    Document& aDoc, nsTArray<RefPtr<Document>>& aDocuments) {
  aDoc.SetIsInSyncOperation(true);
  if (nsCOMPtr<nsPIDOMWindowInner> window = aDoc.GetInnerWindow()) {
    window->TimeoutManager().BeginSyncOperation();
  }
  aDocuments.AppendElement(&aDoc);
  auto recurse = [&aDocuments](Document& aSubDoc) {
    return MarkDocumentTreeToBeInSyncOperation(aSubDoc, aDocuments);
  };
  aDoc.EnumerateSubDocuments(recurse);
  return CallState::Continue;
}

nsAutoSyncOperation::nsAutoSyncOperation(Document* aDoc) {
  mMicroTaskLevel = 0;
  CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
  if (ccjs) {
    mMicroTaskLevel = ccjs->MicroTaskLevel();
    ccjs->SetMicroTaskLevel(0);
  }
  if (aDoc) {
    if (nsPIDOMWindowOuter* win = aDoc->GetWindow()) {
      if (nsCOMPtr<nsPIDOMWindowOuter> top = win->GetInProcessTop()) {
        if (RefPtr<Document> doc = top->GetExtantDoc()) {
          MarkDocumentTreeToBeInSyncOperation(*doc, mDocuments);
        }
      }
    }
  }
}

nsAutoSyncOperation::~nsAutoSyncOperation() {
  for (RefPtr<Document>& doc : mDocuments) {
    if (nsCOMPtr<nsPIDOMWindowInner> window = doc->GetInnerWindow()) {
      window->TimeoutManager().EndSyncOperation();
    }
    doc->SetIsInSyncOperation(false);
  }
  CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
  if (ccjs) {
    ccjs->SetMicroTaskLevel(mMicroTaskLevel);
  }
}

gfxUserFontSet* Document::GetUserFontSet() {
  if (!mFontFaceSet) {
    return nullptr;
  }

  return mFontFaceSet->GetUserFontSet();
}

void Document::FlushUserFontSet() {
  if (!mFontFaceSetDirty) {
    return;
  }

  mFontFaceSetDirty = false;

  if (gfxPlatform::GetPlatform()->DownloadableFontsEnabled()) {
    nsTArray<nsFontFaceRuleContainer> rules;
    RefPtr<PresShell> presShell = GetPresShell();
    if (presShell) {
      MOZ_ASSERT(mStyleSetFilled);
      mStyleSet->AppendFontFaceRules(rules);
    }

    if (!mFontFaceSet && !rules.IsEmpty()) {
      nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetScopeObject());
      mFontFaceSet = new FontFaceSet(window, this);
    }

    bool changed = false;
    if (mFontFaceSet) {
      changed = mFontFaceSet->UpdateRules(rules);
    }

    // We need to enqueue a style change reflow (for later) to
    // reflect that we're modifying @font-face rules.  (However,
    // without a reflow, nothing will happen to start any downloads
    // that are needed.)
    if (changed && presShell) {
      if (nsPresContext* presContext = presShell->GetPresContext()) {
        presContext->UserFontSetUpdated();
      }
    }
  }
}

void Document::MarkUserFontSetDirty() {
  if (mFontFaceSetDirty) {
    return;
  }
  mFontFaceSetDirty = true;
  if (PresShell* presShell = GetPresShell()) {
    presShell->EnsureStyleFlush();
  }
}

FontFaceSet* Document::Fonts() {
  if (!mFontFaceSet) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetScopeObject());
    mFontFaceSet = new FontFaceSet(window, this);
    FlushUserFontSet();
  }
  return mFontFaceSet;
}

void Document::ReportHasScrollLinkedEffect() {
  if (mHasScrollLinkedEffect) {
    // We already did this once for this document, don't do it again.
    return;
  }
  mHasScrollLinkedEffect = true;
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Async Pan/Zoom"), this,
      nsContentUtils::eLAYOUT_PROPERTIES, "ScrollLinkedEffectFound2");
}

void Document::SetUserHasInteracted() {
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug,
          ("Document %p has been interacted by user.", this));

  // We maybe need to update the user-interaction permission.
  MaybeStoreUserInteractionAsPermission();

  if (mUserHasInteracted) {
    return;
  }

  mUserHasInteracted = true;

  if (mChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
    loadInfo->SetDocumentHasUserInteracted(true);
  }

  MaybeAllowStorageForOpenerAfterUserInteraction();
}

BrowsingContext* Document::GetBrowsingContext() const {
  nsCOMPtr<nsIDocShell> docshell(mDocumentContainer);
  return docshell ? docshell->GetBrowsingContext() : nullptr;
}

void Document::NotifyUserGestureActivation() {
  if (RefPtr<BrowsingContext> bc = GetBrowsingContext()) {
    bc->PreOrderWalk([&](BrowsingContext* aContext) {
      nsIDocShell* docShell = aContext->GetDocShell();
      if (!docShell) {
        return;
      }

      Document* document = docShell->GetDocument();
      if (!document) {
        return;
      }

      // XXXedgar we probably could just check `IsInProcess()` after fission
      // enable.
      if (NodePrincipal()->Equals(document->NodePrincipal())) {
        aContext->NotifyUserGestureActivation();
      }
    });

    for (bc = bc->GetParent(); bc; bc = bc->GetParent()) {
      bc->NotifyUserGestureActivation();
    }
  }
}

bool Document::HasBeenUserGestureActivated() {
  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  return bc && bc->HasBeenUserGestureActivated();
}

void Document::ClearUserGestureActivation() {
  if (RefPtr<BrowsingContext> bc = GetBrowsingContext()) {
    bc = bc->Top();
    bc->PreOrderWalk([&](BrowsingContext* aContext) {
      aContext->NotifyResetUserGestureActivation();
    });
  }
}

bool Document::HasValidTransientUserGestureActivation() {
  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  return bc && bc->HasValidTransientUserGestureActivation();
}

bool Document::ConsumeTransientUserGestureActivation() {
  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  return bc && bc->ConsumeTransientUserGestureActivation();
}

void Document::SetDocTreeHadAudibleMedia() {
  Document* topLevelDoc = GetTopLevelContentDocument();
  if (!topLevelDoc) {
    return;
  }

  topLevelDoc->mDocTreeHadAudibleMedia = true;
}

void Document::SetDocTreeHadPlayRevoked() {
  Document* topLevelDoc = GetTopLevelContentDocument();
  if (topLevelDoc) {
    topLevelDoc->mDocTreeHadPlayRevoked = true;
  }
}

DocumentAutoplayPolicy Document::AutoplayPolicy() const {
  return AutoplayPolicy::IsAllowedToPlay(*this);
}

void Document::MaybeAllowStorageForOpenerAfterUserInteraction() {
  if (!CookieJarSettings()->GetRejectThirdPartyContexts()) {
    return;
  }

  // This will probably change for project fission, but currently this document
  // and the opener are on the same process. In the future, we should make this
  // part async.
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (NS_WARN_IF(!inner)) {
    return;
  }

  uint32_t cookieBehavior = CookieJarSettings()->GetCookieBehavior();
  if (cookieBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      cookieBehavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    // We care about first-party tracking resources only.
    if (!nsContentUtils::IsFirstPartyTrackingResourceWindow(inner)) {
      return;
    }
  } else {
    MOZ_ASSERT(net::CookieJarSettings::IsRejectThirdPartyWithExceptions(
        cookieBehavior));
  }

  auto* outer = nsGlobalWindowOuter::Cast(inner->GetOuterWindow());
  if (NS_WARN_IF(!outer)) {
    return;
  }

  RefPtr<BrowsingContext> openerBC = outer->GetOpenerBrowsingContext();
  if (!openerBC) {
    // No opener.
    return;
  }

  // We want to ensure the following check works for both fission mode
  // and non-fission mode:
  // "If the opener is not a 3rd party and if this window is not a 3rd
  // party with respect to the opener, we should not continue."
  //
  // In non-fission mode, the opener and the opened window are in the same
  // process, we can use nsContentUtils::IsThirdPartyWindowOrChannel to
  // do the check.
  // In fission mode, if this window is not a 3rd party with respect to
  // the opener, they must be in the same process, so we can still use
  // IsThirdPartyWindowOrChannel(openerInner) to continue to check if
  // the opener is a 3rd party.
  if (openerBC->IsInProcess()) {
    nsCOMPtr<nsPIDOMWindowOuter> outerOpener = openerBC->GetDOMWindow();
    if (NS_WARN_IF(!outerOpener)) {
      return;
    }

    nsCOMPtr<nsPIDOMWindowInner> openerInner =
        outerOpener->GetCurrentInnerWindow();
    if (NS_WARN_IF(!openerInner)) {
      return;
    }

    RefPtr<Document> openerDocument = openerInner->GetExtantDoc();
    if (NS_WARN_IF(!openerDocument)) {
      return;
    }

    nsCOMPtr<nsIURI> openerURI = openerDocument->GetDocumentURI();
    if (NS_WARN_IF(!openerURI)) {
      return;
    }

    // If the opener is not a 3rd party and if this window is not
    // a 3rd party with respect to the opener, we should not continue.
    if (!nsContentUtils::IsThirdPartyWindowOrChannel(inner, nullptr,
                                                     openerURI) &&
        !nsContentUtils::IsThirdPartyWindowOrChannel(openerInner, nullptr,
                                                     nullptr)) {
      return;
    }
  }

  // We don't care when the asynchronous work finishes here.
  Unused << ContentBlocking::AllowAccessFor(
      NodePrincipal(), openerBC,
      ContentBlockingNotifier::eOpenerAfterUserInteraction);
}

namespace {

// Documents can stay alive for days. We don't want to update the permission
// value at any user-interaction, and, using a timer triggered any X seconds
// should be good enough. 'X' is taken from
// privacy.userInteraction.document.interval pref.
//  We also want to store the user-interaction before shutting down, and, for
//  this reason, this class implements nsIAsyncShutdownBlocker interface.
class UserIntractionTimer final : public Runnable,
                                  public nsITimerCallback,
                                  public nsIAsyncShutdownBlocker {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit UserIntractionTimer(Document* aDocument)
      : Runnable("UserIntractionTimer"),
        mPrincipal(aDocument->NodePrincipal()),
        mDocument(do_GetWeakReference(aDocument)) {
    static int32_t userInteractionTimerId = 0;
    // Blocker names must be unique. Let's create it now because when needed,
    // the document could be already gone.
    mBlockerName.AppendPrintf("UserInteractionTimer %d for document %p",
                              ++userInteractionTimerId, aDocument);
  }

  // Runnable interface

  NS_IMETHOD
  Run() override {
    uint32_t interval =
        StaticPrefs::privacy_userInteraction_document_interval();
    if (!interval) {
      return NS_OK;
    }

    RefPtr<UserIntractionTimer> self = this;
    auto raii =
        MakeScopeExit([self] { self->CancelTimerAndStoreUserInteraction(); });

    nsresult rv = NS_NewTimerWithCallback(
        getter_AddRefs(mTimer), this, interval * 1000, nsITimer::TYPE_ONE_SHOT,
        SystemGroup::EventTargetFor(TaskCategory::Other));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    nsCOMPtr<nsIAsyncShutdownClient> phase = GetShutdownPhase();
    NS_ENSURE_TRUE(!!phase, NS_OK);

    rv = phase->AddBlocker(this, NS_LITERAL_STRING(__FILE__), __LINE__,
                           NS_LITERAL_STRING("UserIntractionTimer shutdown"));
    NS_ENSURE_SUCCESS(rv, NS_OK);

    raii.release();
    return NS_OK;
  }

  // nsITimerCallback interface

  NS_IMETHOD
  Notify(nsITimer* aTimer) override {
    StoreUserInteraction();
    return NS_OK;
  }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  using nsINamed::GetName;
#endif

  // nsIAsyncShutdownBlocker interface

  NS_IMETHOD
  GetName(nsAString& aName) override {
    aName = mBlockerName;
    return NS_OK;
  }

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aClient) override {
    CancelTimerAndStoreUserInteraction();
    return NS_OK;
  }

  NS_IMETHOD
  GetState(nsIPropertyBag**) override { return NS_OK; }

 private:
  ~UserIntractionTimer() = default;

  void StoreUserInteraction() {
    // Remove the shutting down blocker
    nsCOMPtr<nsIAsyncShutdownClient> phase = GetShutdownPhase();
    if (phase) {
      phase->RemoveBlocker(this);
    }

    // If the document is not gone, let's reset its timer flag.
    nsCOMPtr<Document> document = do_QueryReferent(mDocument);
    if (document) {
      ContentBlockingUserInteraction::Observe(mPrincipal);
      document->ResetUserInteractionTimer();
    }
  }

  void CancelTimerAndStoreUserInteraction() {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    StoreUserInteraction();
  }

  static already_AddRefed<nsIAsyncShutdownClient> GetShutdownPhase() {
    nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
    NS_ENSURE_TRUE(!!svc, nullptr);

    nsCOMPtr<nsIAsyncShutdownClient> phase;
    nsresult rv = svc->GetXpcomWillShutdown(getter_AddRefs(phase));
    NS_ENSURE_SUCCESS(rv, nullptr);

    return phase.forget();
  }

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsWeakPtr mDocument;

  nsCOMPtr<nsITimer> mTimer;

  nsString mBlockerName;
};

NS_IMPL_ISUPPORTS_INHERITED(UserIntractionTimer, Runnable, nsITimerCallback,
                            nsIAsyncShutdownBlocker)

}  // namespace

void Document::MaybeStoreUserInteractionAsPermission() {
  // We care about user-interaction stored only for top-level documents.
  if (!IsTopLevelContentDocument()) {
    return;
  }

  if (!mUserHasInteracted) {
    // First interaction, let's store this info now.
    ContentBlockingUserInteraction::Observe(NodePrincipal());
    return;
  }

  if (mHasUserInteractionTimerScheduled) {
    return;
  }

  nsCOMPtr<nsIRunnable> task = new UserIntractionTimer(this);
  nsresult rv = NS_DispatchToCurrentThreadQueue(task.forget(), 2500,
                                                EventQueuePriority::Idle);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // This value will be reset by the timer.
  mHasUserInteractionTimerScheduled = true;
}

void Document::ResetUserInteractionTimer() {
  mHasUserInteractionTimerScheduled = false;
}

bool Document::IsExtensionPage() const {
  return Preferences::GetBool("media.autoplay.allow-extension-background-pages",
                              true) &&
         BasePrincipal::Cast(NodePrincipal())->AddonPolicy();
}

void Document::TraceProtos(JSTracer* aTrc) {
  if (mPrototypeDocument) {
    mPrototypeDocument->TraceProtos(aTrc);
  }
}

/**
 * Retrieves the classification of the Flash plugins in the document based on
 * the classification lists. For more information, see
 * toolkit/components/url-classifier/flash-block-lists.rst
 */
FlashClassification Document::DocumentFlashClassification() {
  // Disable flash blocking when fission is enabled(See Bug 1584931).
  const auto fnIsFlashBlockingEnabled = [] {
    return StaticPrefs::plugins_flashBlock_enabled() &&
           !StaticPrefs::fission_autostart();
  };

  // If neither pref is on, skip the null-principal and principal URI checks.
  if (!StaticPrefs::plugins_http_https_only() && !fnIsFlashBlockingEnabled()) {
    return FlashClassification::Unknown;
  }

  if (!NodePrincipal()->GetIsContentPrincipal()) {
    return FlashClassification::Denied;
  }

  if (StaticPrefs::plugins_http_https_only()) {
    // Only allow plugins for documents from an HTTP/HTTPS origin. This should
    // allow dependent data: URIs to load plugins, but not:
    // * chrome documents
    // * "bare" data: loads
    // * FTP/gopher/file

    if (!(NodePrincipal()->SchemeIs("http") ||
          NodePrincipal()->SchemeIs("https"))) {
      return FlashClassification::Denied;
    }
  }

  // If flash blocking is disabled, it is equivalent to all sites being
  // on neither list.
  if (!fnIsFlashBlockingEnabled()) {
    return FlashClassification::Unknown;
  }

  if (mFlashClassification == FlashClassification::Unknown) {
    mFlashClassification = DocumentFlashClassificationInternal();
  }

  return mFlashClassification;
}

void Document::AddResizeObserver(ResizeObserver& aObserver) {
  if (!mResizeObserverController) {
    mResizeObserverController = MakeUnique<ResizeObserverController>(this);
  }
  mResizeObserverController->AddResizeObserver(aObserver);
}

void Document::RemoveResizeObserver(ResizeObserver& aObserver) {
  MOZ_DIAGNOSTIC_ASSERT(mResizeObserverController, "No controller?");
  if (MOZ_UNLIKELY(!mResizeObserverController)) {
    return;
  }
  mResizeObserverController->RemoveResizeObserver(aObserver);
}

PermissionDelegateHandler* Document::GetPermissionDelegateHandler() {
  if (!mPermissionDelegateHandler) {
    mPermissionDelegateHandler =
        mozilla::MakeAndAddRef<PermissionDelegateHandler>(this);
  }

  if (!mPermissionDelegateHandler->Initialize()) {
    mPermissionDelegateHandler = nullptr;
  }

  return mPermissionDelegateHandler;
}

void Document::ScheduleResizeObserversNotification() const {
  if (!mResizeObserverController) {
    return;
  }

  mResizeObserverController->ScheduleNotification();
}

/**
 * Initializes |mIsThirdPartyForFlashClassifier| if necessary and returns its
 * value. The value returned represents whether this document should be
 * considered Third-Party.
 *
 * A top-level document cannot be a considered Third-Party; only subdocuments
 * may. For a subdocument to be considered Third-Party, it must meet ANY ONE
 * of the following requirements:
 *  - The document's parent is Third-Party
 *  - The document has a different scheme (http/https) than its parent document
 *  - The document's domain and subdomain do not match those of its parent
 *    document.
 *
 * If there is an error in determining whether the document is Third-Party,
 * it will be assumed to be Third-Party for security reasons.
 */
bool Document::IsThirdPartyForFlashClassifier() {
  if (mIsThirdPartyForFlashClassifier.isSome()) {
    return mIsThirdPartyForFlashClassifier.value();
  }

  BrowsingContext* browsingContext = this->GetBrowsingContext();
  if (!browsingContext) {
    mIsThirdPartyForFlashClassifier.emplace(true);
    return mIsThirdPartyForFlashClassifier.value();
  }

  if (browsingContext->IsTop()) {
    mIsThirdPartyForFlashClassifier.emplace(false);
    return mIsThirdPartyForFlashClassifier.value();
  }

  nsCOMPtr<Document> parentDocument = GetInProcessParentDocument();
  if (!parentDocument) {
    // Failure
    mIsThirdPartyForFlashClassifier.emplace(true);
    return mIsThirdPartyForFlashClassifier.value();
  }

  if (parentDocument->IsThirdPartyForFlashClassifier()) {
    mIsThirdPartyForFlashClassifier.emplace(true);
    return mIsThirdPartyForFlashClassifier.value();
  }

  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  nsCOMPtr<nsIPrincipal> parentPrincipal = parentDocument->GetPrincipal();

  bool principalsMatch = false;
  nsresult rv = principal->Equals(parentPrincipal, &principalsMatch);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Failure
    mIsThirdPartyForFlashClassifier.emplace(true);
    return mIsThirdPartyForFlashClassifier.value();
  }

  if (!principalsMatch) {
    mIsThirdPartyForFlashClassifier.emplace(true);
    return mIsThirdPartyForFlashClassifier.value();
  }

  // Fall-through. Document is not a Third-Party Document.
  mIsThirdPartyForFlashClassifier.emplace(false);
  return mIsThirdPartyForFlashClassifier.value();
}

FlashClassification Document::DocumentFlashClassificationInternal() {
  FlashClassification classification = FlashClassification::Unknown;

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(GetChannel());
  if (httpChannel) {
    nsIHttpChannel::FlashPluginState state = nsIHttpChannel::FlashPluginUnknown;
    httpChannel->GetFlashPluginState(&state);

    // Allow unknown children to inherit allowed status from parent, but do not
    // allow denied children to do so.

    if (state == nsIHttpChannel::FlashPluginDeniedInSubdocuments &&
        IsThirdPartyForFlashClassifier()) {
      return FlashClassification::Denied;
    }

    if (state == nsIHttpChannel::FlashPluginDenied) {
      return FlashClassification::Denied;
    }

    if (state == nsIHttpChannel::FlashPluginAllowed) {
      classification = FlashClassification::Allowed;
    }
  }

  if (IsTopLevelContentDocument()) {
    return classification;
  }

  Document* parentDocument = GetInProcessParentDocument();
  if (!parentDocument) {
    return FlashClassification::Denied;
  }

  FlashClassification parentClassification =
      parentDocument->DocumentFlashClassification();

  if (parentClassification == FlashClassification::Denied) {
    return FlashClassification::Denied;
  }

  // Allow unknown children to inherit allowed status from parent, but
  // do not allow denied children to do so.
  if (classification == FlashClassification::Unknown &&
      parentClassification == FlashClassification::Allowed) {
    return FlashClassification::Allowed;
  }

  return classification;
}

void Document::ClearStaleServoData() {
  DocumentStyleRootIterator iter(this);
  while (Element* root = iter.GetNextStyleRoot()) {
    RestyleManager::ClearServoDataFromSubtree(root);
  }
}

Selection* Document::GetSelection(ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window = GetInnerWindow();
  if (!window) {
    return nullptr;
  }

  if (!window->IsCurrentInnerWindow()) {
    return nullptr;
  }

  return nsGlobalWindowInner::Cast(window)->GetSelection(aRv);
}

already_AddRefed<mozilla::dom::Promise> Document::HasStorageAccess(
    mozilla::ErrorResult& aRv) {
  nsIGlobalObject* global = GetScopeObject();
  if (!global) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<Promise> promise =
      Promise::Create(global, aRv, Promise::ePropagateUserInteraction);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (NodePrincipal()->GetIsNullPrincipal()) {
    promise->MaybeResolve(false);
    return promise.forget();
  }

  if (IsTopLevelContentDocument()) {
    promise->MaybeResolve(true);
    return promise.forget();
  }

  nsCOMPtr<Document> topLevelDoc = GetTopLevelContentDocument();
  if (!topLevelDoc) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }
  if (topLevelDoc->NodePrincipal()->Equals(NodePrincipal())) {
    promise->MaybeResolve(true);
    return promise.forget();
  }

  nsPIDOMWindowInner* inner = GetInnerWindow();
  nsGlobalWindowOuter* outer = nullptr;
  if (inner) {
    outer = nsGlobalWindowOuter::Cast(inner->GetOuterWindow());
    promise->MaybeResolve(outer->HasStorageAccess());
  } else {
    promise->MaybeRejectWithUndefined();
  }
  return promise.forget();
}

RefPtr<Document::GetContentBlockingEventsPromise>
Document::GetContentBlockingEvents() {
  RefPtr<nsPIDOMWindowInner> inner = GetInnerWindow();
  if (!inner) {
    return nullptr;
  }

  RefPtr<WindowGlobalChild> wgc = inner->GetWindowGlobalChild();
  if (!wgc) {
    return nullptr;
  }

  return wgc->SendGetContentBlockingEvents()->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [](const WindowGlobalChild::GetContentBlockingEventsPromise::
             ResolveOrRejectValue& aValue) {
        if (aValue.IsResolve()) {
          return Document::GetContentBlockingEventsPromise::CreateAndResolve(
              aValue.ResolveValue(), __func__);
        }

        return Document::GetContentBlockingEventsPromise::CreateAndReject(
            false, __func__);
      });
}

already_AddRefed<mozilla::dom::Promise> Document::RequestStorageAccess(
    mozilla::ErrorResult& aRv) {
  nsIGlobalObject* global = GetScopeObject();
  if (!global) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  // Propagate user input event handling to the resolve handler
  RefPtr<Promise> promise =
      Promise::Create(global, aRv, Promise::ePropagateUserInteraction);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = GetInnerWindow();
  if (!inner) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  // Step 1. If the document already has been granted access, resolve.
  RefPtr<nsGlobalWindowOuter> outer =
      nsGlobalWindowOuter::Cast(inner->GetOuterWindow());
  if (!outer) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  if (outer->HasStorageAccess()) {
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  // Step 2. If the document has a null origin, reject.
  if (NodePrincipal()->GetIsNullPrincipal()) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  if (!bc) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  // Only enforce third-party checks when there is a reason to enforce them.
  if (!CookieJarSettings()->GetRejectThirdPartyContexts()) {
    // Step 3. If the document's frame is the main frame, resolve.
    if (IsTopLevelContentDocument()) {
      promise->MaybeResolveWithUndefined();
      return promise.forget();
    }

    // Step 4. If the sub frame's origin is equal to the main frame's, resolve.

    // In fission, if the sub frame's origin differs from the main frame's
    // origin, they will be in different processes. We use IsInProcess()
    // check here to deterimine whether they have the same origin. In
    // non-fission mode, it is always in-process so we need to compare their
    // principals.
    if (bc->Top()->IsInProcess()) {
      nsCOMPtr<nsPIDOMWindowOuter> topOuter = bc->Top()->GetDOMWindow();
      if (!topOuter) {
        promise->MaybeRejectWithUndefined();
        return promise.forget();
      }

      nsCOMPtr<Document> topLevelDoc = topOuter->GetExtantDoc();
      if (!topLevelDoc) {
        promise->MaybeRejectWithUndefined();
        return promise.forget();
      }

      if (topLevelDoc->NodePrincipal()->Equals(NodePrincipal())) {
        promise->MaybeResolveWithUndefined();
        return promise.forget();
      }
    }
  }

  // Step 5. If the sub frame is not sandboxed, skip to step 7.
  // Step 6. If the sub frame doesn't have the token
  //         "allow-storage-access-by-user-activation", reject.
  if (StorageAccessSandboxed()) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  // Step 7. If the sub frame's parent frame is not the top frame, reject.
  RefPtr<BrowsingContext> parentBC = bc->GetParent();
  if (parentBC && !parentBC->IsTopContent()) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  // Step 8. If the browser is not processing a user gesture, reject.
  if (!UserActivation::IsHandlingUserInput()) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  // Step 9. Check any additional rules that the browser has.
  //         Examples: Whitelists, blacklists, on-device classification,
  //         user settings, anti-clickjacking heuristics, or prompting the
  //         user for explicit permission. Reject if some rule is not fulfilled.

  if (CookieJarSettings()->GetRejectThirdPartyContexts()) {
    // Only do something special for third-party tracking content.
    if (StorageDisabledByAntiTracking(this, nullptr)) {
      // Note: If this has returned true, the top-level document is guaranteed
      // to not be on the Content Blocking allow list.
      MOZ_ASSERT(!CookieJarSettings()->GetIsOnContentBlockingAllowList());

      RefPtr<Document> self(this);

      auto performFinalChecks =
          [inner,
           self]() -> RefPtr<ContentBlocking::StorageAccessFinalCheckPromise> {
        RefPtr<ContentBlocking::StorageAccessFinalCheckPromise::Private> p =
            new ContentBlocking::StorageAccessFinalCheckPromise::Private(
                __func__);
        RefPtr<StorageAccessPermissionRequest> sapr =
            StorageAccessPermissionRequest::Create(
                inner,
                // Allow
                [p] {
                  Telemetry::AccumulateCategorical(
                      Telemetry::LABELS_STORAGE_ACCESS_API_UI::Allow);
                  p->Resolve(ContentBlocking::eAllow, __func__);
                },
                // Block
                [p] {
                  Telemetry::AccumulateCategorical(
                      Telemetry::LABELS_STORAGE_ACCESS_API_UI::Deny);
                  p->Reject(false, __func__);
                });

        typedef ContentPermissionRequestBase::PromptResult PromptResult;
        PromptResult pr = sapr->CheckPromptPrefs();

        if (pr == PromptResult::Pending) {
          // We're about to show a prompt, record the request attempt
          Telemetry::AccumulateCategorical(
              Telemetry::LABELS_STORAGE_ACCESS_API_UI::Request);
        }

        self->AutomaticStorageAccessCanBeGranted()->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
            [p, pr, sapr, inner](
                const AutomaticStorageAccessGrantPromise::ResolveOrRejectValue&
                    aValue) -> void {
              // Make a copy because we can't modified copy-captured lambda
              // variables.
              PromptResult pr2 = pr;

              bool storageAccessCanBeGrantedAutomatically =
                  aValue.IsResolve() && aValue.ResolveValue();

              bool autoGrant = false;
              if (pr2 == PromptResult::Pending &&
                  storageAccessCanBeGrantedAutomatically) {
                pr2 = PromptResult::Granted;
                autoGrant = true;

                Telemetry::AccumulateCategorical(
                    Telemetry::LABELS_STORAGE_ACCESS_API_UI::
                        AllowAutomatically);
              }

              if (pr2 != PromptResult::Pending) {
                MOZ_ASSERT_IF(pr2 != PromptResult::Granted,
                              pr2 == PromptResult::Denied);
                if (pr2 == PromptResult::Granted) {
                  ContentBlocking::StorageAccessPromptChoices choice =
                      ContentBlocking::eAllow;
                  if (autoGrant) {
                    choice = ContentBlocking::eAllowAutoGrant;
                  }
                  if (!autoGrant) {
                    p->Resolve(choice, __func__);
                  } else {
                    sapr->MaybeDelayAutomaticGrants()->Then(
                        GetCurrentThreadSerialEventTarget(), __func__,
                        [p, choice] { p->Resolve(choice, __func__); },
                        [p] { p->Reject(false, __func__); });
                  }
                  return;
                }
                p->Reject(false, __func__);
                return;
              }

              sapr->RequestDelayedTask(
                  inner->EventTargetFor(TaskCategory::Other),
                  ContentPermissionRequestBase::DelayedTaskType::Request);
            });

        return std::move(p);
      };
      ContentBlocking::AllowAccessFor(
          NodePrincipal(), bc, ContentBlockingNotifier::eStorageAccessAPI,
          performFinalChecks)
          ->Then(
              GetCurrentThreadSerialEventTarget(), __func__,
              [outer, promise] {
                // Step 10. Grant the document access to cookies and store
                // that fact for
                //          the purposes of future calls to
                //          hasStorageAccess() and requestStorageAccess().
                outer->SetHasStorageAccess(true);
                promise->MaybeResolveWithUndefined();
              },
              [outer, promise] {
                outer->SetHasStorageAccess(false);
                promise->MaybeRejectWithUndefined();
              });

      return promise.forget();
    }
  }

  outer->SetHasStorageAccess(true);
  promise->MaybeResolveWithUndefined();
  return promise.forget();
}

RefPtr<Document::AutomaticStorageAccessGrantPromise>
Document::AutomaticStorageAccessCanBeGranted() {
  if (XRE_IsContentProcess()) {
    // In the content process, we need to ask the parent process to compute
    // this.  The reason is that nsIPermissionManager::GetAllWithTypePrefix()
    // isn't accessible in the content process.
    ContentChild* cc = ContentChild::GetSingleton();
    MOZ_ASSERT(cc);

    return cc
        ->SendAutomaticStorageAccessCanBeGranted(
            IPC::Principal(NodePrincipal()))
        ->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
            [](const ContentChild::AutomaticStorageAccessCanBeGrantedPromise::
                   ResolveOrRejectValue& aValue) {
              if (aValue.IsResolve()) {
                return AutomaticStorageAccessGrantPromise::CreateAndResolve(
                    aValue.ResolveValue(), __func__);
              }

              return AutomaticStorageAccessGrantPromise::CreateAndReject(
                  false, __func__);
            });
  }

  if (XRE_IsParentProcess()) {
    // In the parent process, we can directly compute this.
    return AutomaticStorageAccessGrantPromise::CreateAndResolve(
        AutomaticStorageAccessCanBeGranted(NodePrincipal()), __func__);
  }

  return AutomaticStorageAccessGrantPromise::CreateAndReject(false, __func__);
}

bool Document::AutomaticStorageAccessCanBeGranted(nsIPrincipal* aPrincipal) {
  nsAutoCString prefix;
  AntiTrackingUtils::CreateStoragePermissionKey(aPrincipal, prefix);

  nsPermissionManager* permManager = nsPermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    return false;
  }

  typedef nsTArray<RefPtr<nsIPermission>> Permissions;
  Permissions perms;
  nsresult rv = permManager->GetAllWithTypePrefix(prefix, perms);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoCString prefix2(prefix);
  prefix2.Append('^');
  typedef nsTArray<nsCString> Origins;
  Origins origins;

  for (const auto& perm : perms) {
    nsAutoCString type;
    rv = perm->GetType(type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    // Let's make sure that we're not looking at a permission for
    // https://exampletracker.company when we mean to look for the
    // permission for https://exampletracker.com!
    if (type != prefix && StringHead(type, prefix2.Length()) != prefix2) {
      continue;
    }

    nsCOMPtr<nsIPrincipal> principal;
    rv = perm->GetPrincipal(getter_AddRefs(principal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    nsAutoCString origin;
    rv = principal->GetOrigin(origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    ToLowerCase(origin);

    if (origins.IndexOf(origin) == Origins::NoIndex) {
      origins.AppendElement(origin);
    }
  }

  nsCOMPtr<nsIBrowserUsage> bu =
      do_ImportModule("resource:///modules/BrowserUsageTelemetry.jsm");
  if (NS_WARN_IF(!bu)) {
    return false;
  }

  uint32_t uniqueDomainsVisitedInPast24Hours = 0;
  rv = bu->GetUniqueDomainsVisitedInPast24Hours(
      &uniqueDomainsVisitedInPast24Hours);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // one percent of the number of top-levels origins visited in the current
  // session (but not to exceed 24 hours), or the value of the
  // dom.storage_access.max_concurrent_auto_grants preference, whichever is
  // higher.
  size_t maxConcurrentAutomaticGrants = std::max(
      std::max(int(std::floor(uniqueDomainsVisitedInPast24Hours / 100)),
               StaticPrefs::dom_storage_access_max_concurrent_auto_grants()),
      0);

  size_t originsThirdPartyHasAccessTo = origins.Length();

  return StaticPrefs::dom_storage_access_auto_grants() &&
         originsThirdPartyHasAccessTo < maxConcurrentAutomaticGrants;
}

void Document::RecordNavigationTiming(ReadyState aReadyState) {
  if (!XRE_IsContentProcess()) {
    return;
  }
  if (!IsTopLevelContentDocument()) {
    return;
  }
  // If we dont have the timing yet (mostly because the doc is still loading),
  // get it from docshell.
  RefPtr<nsDOMNavigationTiming> timing = mTiming;
  if (!timing) {
    if (!mDocumentContainer) {
      return;
    }
    timing = mDocumentContainer->GetNavigationTiming();
    if (!timing) {
      return;
    }
  }
  TimeStamp startTime = timing->GetNavigationStartTimeStamp();
  switch (aReadyState) {
    case READYSTATE_LOADING:
      if (!mDOMLoadingSet) {
        Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_LOADING_MS,
                                       startTime);
        mDOMLoadingSet = true;
      }
      break;
    case READYSTATE_INTERACTIVE:
      if (!mDOMInteractiveSet) {
        Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_INTERACTIVE_MS,
                                       startTime);
        mDOMInteractiveSet = true;
      }
      break;
    case READYSTATE_COMPLETE:
      if (!mDOMCompleteSet) {
        Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_COMPLETE_MS,
                                       startTime);
        mDOMCompleteSet = true;
      }
      break;
    default:
      NS_WARNING("Unexpected ReadyState value");
      break;
  }
}

bool Document::ModuleScriptsEnabled() {
  return nsContentUtils::IsChromeDoc(this) ||
         StaticPrefs::dom_moduleScripts_enabled();
}

void Document::ReportShadowDOMUsage() {
  if (mHasReportedShadowDOMUsage) {
    return;
  }

  Document* topLevel = GetTopLevelContentDocument();
  if (topLevel && !topLevel->mHasReportedShadowDOMUsage) {
    topLevel->mHasReportedShadowDOMUsage = true;
    nsString uri;
    Unused << topLevel->GetDocumentURI(uri);
    if (!uri.IsEmpty()) {
      nsAutoString msg = NS_LITERAL_STRING("Shadow DOM used in [") + uri +
                         NS_LITERAL_STRING("] or in some of its subdocuments.");
      nsContentUtils::ReportToConsoleNonLocalized(
          msg, nsIScriptError::infoFlag, NS_LITERAL_CSTRING("DOM"), topLevel);
    }
  }

  mHasReportedShadowDOMUsage = true;
}

// static
bool Document::StorageAccessSandboxed(uint32_t aSandboxFlags) {
  return StaticPrefs::dom_storage_access_enabled() &&
         (aSandboxFlags & SANDBOXED_STORAGE_ACCESS) != 0;
}

bool Document::StorageAccessSandboxed() const {
  return Document::StorageAccessSandboxed(GetSandboxFlags());
}

bool Document::GetCachedSizes(nsTabSizes* aSizes) {
  if (mCachedTabSizeGeneration == 0 ||
      GetGeneration() != mCachedTabSizeGeneration) {
    return false;
  }
  aSizes->mDom += mCachedTabSizes.mDom;
  aSizes->mStyle += mCachedTabSizes.mStyle;
  aSizes->mOther += mCachedTabSizes.mOther;
  return true;
}

void Document::SetCachedSizes(nsTabSizes* aSizes) {
  mCachedTabSizes.mDom = aSizes->mDom;
  mCachedTabSizes.mStyle = aSizes->mStyle;
  mCachedTabSizes.mOther = aSizes->mOther;
  mCachedTabSizeGeneration = GetGeneration();
}

already_AddRefed<nsAtom> Document::GetContentLanguageAsAtomForStyle() const {
  nsAutoString contentLang;
  GetContentLanguage(contentLang);
  contentLang.StripWhitespace();

  // Content-Language may be a comma-separated list of language codes,
  // in which case the HTML5 spec says to treat it as unknown
  if (!contentLang.IsEmpty() && !contentLang.Contains(char16_t(','))) {
    return NS_Atomize(contentLang);
  }

  return nullptr;
}

already_AddRefed<nsAtom> Document::GetLanguageForStyle() const {
  RefPtr<nsAtom> lang = GetContentLanguageAsAtomForStyle();
  if (!lang) {
    lang = mLanguageFromCharset;
  }
  return lang.forget();
}

const LangGroupFontPrefs* Document::GetFontPrefsForLang(
    nsAtom* aLanguage, bool* aNeedsToCache) const {
  nsAtom* lang = aLanguage ? aLanguage : mLanguageFromCharset.get();
  return StaticPresData::Get()->GetFontPrefsForLang(lang, aNeedsToCache);
}

void Document::DoCacheAllKnownLangPrefs() {
  MOZ_ASSERT(mMayNeedFontPrefsUpdate);
  RefPtr<nsAtom> lang = GetLanguageForStyle();
  StaticPresData* data = StaticPresData::Get();
  data->GetFontPrefsForLang(lang ? lang.get() : mLanguageFromCharset.get());
  data->GetFontPrefsForLang(nsGkAtoms::x_math);
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1362599#c12
  data->GetFontPrefsForLang(nsGkAtoms::Unicode);
  for (auto iter = mLanguagesUsed.Iter(); !iter.Done(); iter.Next()) {
    data->GetFontPrefsForLang(iter.Get()->GetKey());
  }
  mMayNeedFontPrefsUpdate = false;
}

void Document::RecomputeLanguageFromCharset() {
  nsLanguageAtomService* service = nsLanguageAtomService::GetService();
  RefPtr<nsAtom> language = service->LookupCharSet(mCharacterSet);
  if (language == nsGkAtoms::Unicode) {
    language = service->GetLocaleLanguage();
  }

  if (language == mLanguageFromCharset) {
    return;
  }

  mMayNeedFontPrefsUpdate = true;
  mLanguageFromCharset = std::move(language);
}

nsICookieJarSettings* Document::CookieJarSettings() {
  // If we are here, this is probably a javascript: URL document. In any case,
  // we must have a nsCookieJarSettings. Let's create it.
  if (!mCookieJarSettings) {
    Document* inProcessParent = GetInProcessParentDocument();

    mCookieJarSettings =
        inProcessParent
            ? net::CookieJarSettings::Create(
                  inProcessParent->CookieJarSettings()->GetCookieBehavior())
            : net::CookieJarSettings::Create();
  }

  return mCookieJarSettings;
}

nsIPrincipal* Document::EffectiveStoragePrincipal() const {
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (!inner) {
    return NodePrincipal();
  }

  // Return our cached storage principal if one exists.
  if (mActiveStoragePrincipal) {
    return mActiveStoragePrincipal;
  }

  // We use the lower-level ContentBlocking API here to ensure this
  // check doesn't send notifications.
  uint32_t rejectedReason = 0;
  if (ContentBlocking::ShouldAllowAccessFor(inner, GetDocumentURI(),
                                            &rejectedReason)) {
    return mActiveStoragePrincipal = NodePrincipal();
  }

  // Let's use the storage principal only if we need to partition the cookie
  // jar. When the permission is granted, access will be different and the
  // normal principal will be used.
  if (ShouldPartitionStorage(rejectedReason) &&
      !StoragePartitioningEnabled(
          rejectedReason, const_cast<Document*>(this)->CookieJarSettings())) {
    return mActiveStoragePrincipal = NodePrincipal();
  }

  return mActiveStoragePrincipal = mIntrinsicStoragePrincipal;
}

void Document::SetIsInitialDocument(bool aIsInitialDocument) {
  mIsInitialDocumentInWindow = aIsInitialDocument;

  // Asynchronously tell the parent process that we are, or are no longer, the
  // initial document. This happens async.
  if (RefPtr<nsPIDOMWindowInner> inner = GetInnerWindow()) {
    if (RefPtr<WindowGlobalChild> wgc = inner->GetWindowGlobalChild()) {
      wgc->SendSetIsInitialDocument(aIsInitialDocument);
    }
  }
}

// static
void Document::AddToplevelLoadingDocument(Document* aDoc) {
  MOZ_ASSERT(aDoc && aDoc->IsTopLevelContentDocument());
  // Currently we're interested in foreground documents only, so bail out early.
  if (aDoc->IsInBackgroundWindow() || !XRE_IsContentProcess()) {
    return;
  }

  if (!sLoadingForegroundTopLevelContentDocument) {
    sLoadingForegroundTopLevelContentDocument = new AutoTArray<Document*, 8>();
    mozilla::ipc::IdleSchedulerChild* idleScheduler =
        mozilla::ipc::IdleSchedulerChild::GetMainThreadIdleScheduler();
    if (idleScheduler) {
      idleScheduler->SendRunningPrioritizedOperation();
    }
  }
  if (!sLoadingForegroundTopLevelContentDocument->Contains(aDoc)) {
    sLoadingForegroundTopLevelContentDocument->AppendElement(aDoc);
  }
}

// static
void Document::RemoveToplevelLoadingDocument(Document* aDoc) {
  MOZ_ASSERT(aDoc && aDoc->IsTopLevelContentDocument());
  if (sLoadingForegroundTopLevelContentDocument) {
    sLoadingForegroundTopLevelContentDocument->RemoveElement(aDoc);
    if (sLoadingForegroundTopLevelContentDocument->IsEmpty()) {
      delete sLoadingForegroundTopLevelContentDocument;
      sLoadingForegroundTopLevelContentDocument = nullptr;

      mozilla::ipc::IdleSchedulerChild* idleScheduler =
          mozilla::ipc::IdleSchedulerChild::GetMainThreadIdleScheduler();
      if (idleScheduler) {
        idleScheduler->SendPrioritizedOperationDone();
      }
    }
  }
}

StylePrefersColorScheme Document::PrefersColorScheme() const {
  if (nsContentUtils::ShouldResistFingerprinting(this)) {
    return StylePrefersColorScheme::Light;
  }

  if (nsPresContext* pc = GetPresContext()) {
    if (auto devtoolsOverride = pc->GetOverridePrefersColorScheme()) {
      return *devtoolsOverride;
    }

    if (pc->IsPrintingOrPrintPreview()) {
      return StylePrefersColorScheme::Light;
    }
  }

  // If LookAndFeel::eIntID_SystemUsesDarkTheme fails then return 2
  // (no-preference)
  switch (LookAndFeel::GetInt(LookAndFeel::eIntID_SystemUsesDarkTheme, 2)) {
    case 0:
      return StylePrefersColorScheme::Light;
    case 1:
      return StylePrefersColorScheme::Dark;
    case 2:
      return StylePrefersColorScheme::NoPreference;
    default:
      // This only occurs if the user has set the ui.systemUsesDarkTheme pref to
      // an invalid value.
      return StylePrefersColorScheme::Light;
  }
}

// static
bool Document::UseOverlayScrollbars(const Document* aDocument) {
  BrowsingContext* bc = aDocument ? aDocument->GetBrowsingContext() : nullptr;
  return LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) ||
         (bc && bc->InRDMPane());
}

bool Document::HasRecentlyStartedForegroundLoads() {
  if (!sLoadingForegroundTopLevelContentDocument) {
    return false;
  }

  for (size_t i = 0; i < sLoadingForegroundTopLevelContentDocument->Length();
       ++i) {
    Document* doc = sLoadingForegroundTopLevelContentDocument->ElementAt(i);
    // A page loaded in foreground could be in background now.
    if (!doc->IsInBackgroundWindow()) {
      nsPIDOMWindowInner* win = doc->GetInnerWindow();
      if (win) {
        Performance* perf = win->GetPerformance();
        if (perf &&
            perf->Now() < StaticPrefs::page_load_deprioritization_period()) {
          return true;
        }
      }
    }
  }

  // Didn't find any loading foreground documents, just clear the array.
  delete sLoadingForegroundTopLevelContentDocument;
  sLoadingForegroundTopLevelContentDocument = nullptr;

  mozilla::ipc::IdleSchedulerChild* idleScheduler =
      mozilla::ipc::IdleSchedulerChild::GetMainThreadIdleScheduler();
  if (idleScheduler) {
    idleScheduler->SendPrioritizedOperationDone();
  }
  return false;
}

}  // namespace dom
}  // namespace mozilla
