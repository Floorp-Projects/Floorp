/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all our document implementations.
 */

#include "AudioChannelService.h"
#include "nsDocument.h"
#include "nsIDocumentInlines.h"
#include "mozilla/AnimationComparator.h"
#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/CSSEnabledState.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EnumSet.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Likely.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/URLExtraData.h"
#include <algorithm>

#include "mozilla/Logging.h"
#include "plstr.h"
#include "mozilla/Sprintf.h"

#include "mozilla/Telemetry.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsITextControlFrame.h"
#include "nsNumberControlFrame.h"
#include "nsUnicharUtils.h"
#include "nsContentList.h"
#include "nsCSSPseudoElements.h"
#include "nsIObserver.h"
#include "nsIBaseWindow.h"
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

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"

#include "mozilla/dom/Attr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FramingChecker.h"
#include "mozilla/dom/HTMLSharedElement.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/CDATASection.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "nsDOMString.h"
#include "nsNodeUtils.h"
#include "nsLayoutUtils.h" // for GetFrameForPoint
#include "nsIFrame.h"
#include "nsITabChild.h"

#include "nsRange.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/NodeIterator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/TreeWalker.h"

#include "nsIServiceManager.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "imgLoader.h"

#include "nsAboutProtocolUtils.h"
#include "nsCanvasFrame.h"
#include "nsContentCID.h"
#include "nsError.h"
#include "nsPresContext.h"
#include "nsThreadUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIFileChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIRefreshURI.h"
#include "nsIWebNavigation.h"
#include "nsIScriptError.h"
#include "nsISimpleEnumerator.h"
#include "nsIRequestContext.h"
#include "nsStyleSheetService.h"

#include "nsNetUtil.h"     // for NS_NewURI
#include "nsIInputStreamChannel.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"

#include "nsIScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "ExpandedPrincipal.h"
#include "NullPrincipal.h"

#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsFocusManager.h"

// for radio group stuff
#include "nsIRadioVisitor.h"
#include "nsIFormControl.h"

#include "nsBidiUtils.h"

#include "nsContentCreatorFunctions.h"

#include "nsIScriptContext.h"
#include "nsBindingManager.h"
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
#include "nsIXPConnect.h"
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
#include "mozilla/dom/StyleRuleChangeEvent.h"
#include "mozilla/dom/StyleSheetChangeEvent.h"
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

#include "nsSMILAnimationController.h"
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

#include "imgILoader.h"
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
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/WebComponentsBinding.h"
#include "mozilla/dom/CustomElementRegistryBinding.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/ExtensionPolicyService.h"
#include "nsFrame.h"
#include "nsDOMCaretPosition.h"
#include "nsViewportInfo.h"
#include "mozilla/StaticPtr.h"
#include "nsITextControlElement.h"
#include "nsIEditor.h"
#include "nsIHttpChannelInternal.h"
#include "nsISecurityConsoleMessage.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/dom/XPathEvaluator.h"
#include "mozilla/dom/XPathNSResolverBinding.h"
#include "mozilla/dom/XPathResult.h"
#include "nsIDocumentEncoder.h"
#include "nsIDocumentActivity.h"
#include "nsIStructuredCloneContainer.h"
#include "nsIMutableArray.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsWindowSizes.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/FontFaceSet.h"
#include "gfxPrefs.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/dom/SVGDocument.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"
#ifdef MOZ_XUL
#include "mozilla/dom/ListBoxObject.h"
#include "mozilla/dom/MenuBoxObject.h"
#include "mozilla/dom/TreeBoxObject.h"
#include "nsIXULWindow.h"
#include "nsIDocShellTreeOwner.h"
#endif
#include "nsIPresShellInlines.h"

#include "mozilla/DocLoadingTimelineMarker.h"

#include "nsISpeculativeConnect.h"

#include "mozilla/MediaManager.h"

#include "nsIURIClassifier.h"
#include "nsIURIMutator.h"
#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsHTMLTags.h"

using namespace mozilla;
using namespace mozilla::dom;

typedef nsTArray<Link*> LinkArray;

static LazyLogModule gDocumentLeakPRLog("DocumentLeak");
static LazyLogModule gCspPRLog("CSP");
static LazyLogModule gUserInteractionPRLog("UserInteraction");

static nsresult
GetHttpChannelHelper(nsIChannel* aChannel, nsIHttpChannel** aHttpChannel)
{
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

////////////////////////////////////////////////////////////////////
// PrincipalFlashClassifier

// Classify the flash based on the document principal.
// The usage of this class is as follows:
//
// 1) Call AsyncClassify() as early as possible to asynchronously do
//    classification against all the flash blocking related tables
//    via nsIURIClassifier.asyncClassifyLocalWithTables.
//
// 2) At any time you need the classification result, call Result()
//    and it is guaranteed to give you the result. Note that you have
//    to specify "aIsThirdParty" to the function so please make sure
//    you can already correctly decide if the document is third-party.
//
//    Behind the scenes, the sync classification API
//    (nsIURIClassifier.classifyLocalWithTable) may be called as a fallback to
//    synchronously get the result if the asyncClassifyLocalWithTables hasn't
//    been done yet.
//
// 3) You can call Result() as many times as you want and only the first time
//    it may unfortunately call the blocking sync API. The subsequent call
//    will just return the result that came out at the first time.
//
class PrincipalFlashClassifier final : public nsIURIClassifierCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURICLASSIFIERCALLBACK

  PrincipalFlashClassifier();

  // Fire async classification based on the given principal.
  void AsyncClassify(nsIPrincipal* aPrincipal);

  // Would block if the result hasn't come out.
  mozilla::dom::FlashClassification ClassifyMaybeSync(nsIPrincipal* aPrincipal,
                                                      bool aIsThirdParty);

private:
 ~PrincipalFlashClassifier() = default;

  void Reset();
  bool EnsureUriClassifier();
  mozilla::dom::FlashClassification CheckIfClassifyNeeded(nsIPrincipal* aPrincipal);
  mozilla::dom::FlashClassification Resolve(bool aIsThirdParty);
  mozilla::dom::FlashClassification AsyncClassifyInternal(nsIPrincipal* aPrincipal);
  void GetClassificationTables(bool aIsThirdParty, nsACString& aTables);

  // For the fallback sync classification.
  nsCOMPtr<nsIURI> mClassificationURI;

  nsCOMPtr<nsIURIClassifier> mUriClassifier;
  bool mAsyncClassified;
  nsTArray<nsCString> mMatchedTables;
  mozilla::dom::FlashClassification mResult;
};


#define NAME_NOT_VALID ((nsSimpleContentList*)1)

nsIdentifierMapEntry::nsIdentifierMapEntry(const nsIdentifierMapEntry::AtomOrString& aKey)
  : mKey(aKey)
{}

nsIdentifierMapEntry::nsIdentifierMapEntry(const nsIdentifierMapEntry::AtomOrString* aKey)
  : mKey(aKey ? *aKey : nullptr)
{}

nsIdentifierMapEntry::~nsIdentifierMapEntry()
{}

nsIdentifierMapEntry::nsIdentifierMapEntry(nsIdentifierMapEntry&& aOther)
  : mKey(std::move(aOther.mKey))
  , mIdContentList(std::move(aOther.mIdContentList))
  , mNameContentList(std::move(aOther.mNameContentList))
  , mChangeCallbacks(std::move(aOther.mChangeCallbacks))
  , mImageElement(std::move(aOther.mImageElement))
{}

void
nsIdentifierMapEntry::Traverse(nsCycleCollectionTraversalCallback* aCallback)
{
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

bool
nsIdentifierMapEntry::IsEmpty()
{
  return mIdContentList.IsEmpty() && !mNameContentList &&
         !mChangeCallbacks && !mImageElement;
}

bool
nsIdentifierMapEntry::HasNameElement() const
{
  return mNameContentList && mNameContentList->Length() != 0;
}

Element*
nsIdentifierMapEntry::GetIdElement()
{
  return mIdContentList.SafeElementAt(0);
}

Element*
nsIdentifierMapEntry::GetImageIdElement()
{
  return mImageElement ? mImageElement.get() : GetIdElement();
}

void
nsIdentifierMapEntry::AddContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                               void* aData, bool aForImage)
{
  if (!mChangeCallbacks) {
    mChangeCallbacks = new nsTHashtable<ChangeCallbackEntry>;
  }

  ChangeCallback cc = { aCallback, aData, aForImage };
  mChangeCallbacks->PutEntry(cc);
}

void
nsIdentifierMapEntry::RemoveContentChangeCallback(nsIDocument::IDTargetObserver aCallback,
                                                  void* aData, bool aForImage)
{
  if (!mChangeCallbacks)
    return;
  ChangeCallback cc = { aCallback, aData, aForImage };
  mChangeCallbacks->RemoveEntry(cc);
  if (mChangeCallbacks->Count() == 0) {
    mChangeCallbacks = nullptr;
  }
}

void
nsIdentifierMapEntry::FireChangeCallbacks(Element* aOldElement,
                                          Element* aNewElement,
                                          bool aImageOnly)
{
  if (!mChangeCallbacks)
    return;

  for (auto iter = mChangeCallbacks->ConstIter(); !iter.Done(); iter.Next()) {
    nsIdentifierMapEntry::ChangeCallbackEntry* entry = iter.Get();
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

namespace {

struct PositionComparator
{
  Element* const mElement;
  explicit PositionComparator(Element* const aElement) : mElement(aElement) {}

  int operator()(void* aElement) const {
    Element* curElement = static_cast<Element*>(aElement);
    if (mElement == curElement) {
      return 0;
    }
    if (nsContentUtils::PositionIsBefore(mElement, curElement)) {
      return -1;
    }
    return 1;
  }
};

} // namespace

bool
nsIdentifierMapEntry::AddIdElement(Element* aElement)
{
  MOZ_ASSERT(aElement, "Must have element");
  MOZ_ASSERT(!mIdContentList.Contains(nullptr),
                  "Why is null in our list?");

#ifdef DEBUG
  Element* currentElement = mIdContentList.SafeElementAt(0);
#endif

  // Common case
  if (mIdContentList.IsEmpty()) {
    if (!mIdContentList.AppendElement(aElement))
      return false;
    NS_ASSERTION(currentElement == nullptr, "How did that happen?");
    FireChangeCallbacks(nullptr, aElement);
    return true;
  }

  // We seem to have multiple content nodes for the same id, or XUL is messing
  // with us.  Search for the right place to insert the content.

  size_t idx;
  if (BinarySearchIf(mIdContentList, 0, mIdContentList.Length(),
                     PositionComparator(aElement), &idx)) {
    // Already in the list, so already in the right spot.  Get out of here.
    // XXXbz this only happens because XUL does all sorts of random
    // UpdateIdTableEntry calls.  Hate, hate, hate!
    return true;
  }

  if (!mIdContentList.InsertElementAt(idx, aElement)) {
    return false;
  }

  if (idx == 0) {
    Element* oldElement = mIdContentList.SafeElementAt(1);
    NS_ASSERTION(currentElement == oldElement, "How did that happen?");
    FireChangeCallbacks(oldElement, aElement);
  }
  return true;
}

void
nsIdentifierMapEntry::RemoveIdElement(Element* aElement)
{
  MOZ_ASSERT(aElement, "Missing element");

  // This should only be called while the document is in an update.
  // Assertions near the call to this method guarantee this.

  // This could fire in OOM situations
  // Only assert this in HTML documents for now as XUL does all sorts of weird
  // crap.
  NS_ASSERTION(!aElement->OwnerDoc()->IsHTMLDocument() ||
               mIdContentList.Contains(aElement),
               "Removing id entry that doesn't exist");

  // XXXbz should this ever Compact() I guess when all the content is gone
  // we'll just get cleaned up in the natural order of things...
  Element* currentElement = mIdContentList.SafeElementAt(0);
  mIdContentList.RemoveElement(aElement);
  if (currentElement == aElement) {
    FireChangeCallbacks(currentElement, mIdContentList.SafeElementAt(0));
  }
}

void
nsIdentifierMapEntry::SetImageElement(Element* aElement)
{
  Element* oldElement = GetImageIdElement();
  mImageElement = aElement;
  Element* newElement = GetImageIdElement();
  if (oldElement != newElement) {
    FireChangeCallbacks(oldElement, newElement, true);
  }
}

namespace mozilla {
namespace dom {
class SimpleHTMLCollection final : public nsSimpleContentList
                                 , public nsIHTMLCollection
{
public:
  explicit SimpleHTMLCollection(nsINode* aRoot) : nsSimpleContentList(aRoot) {}

  NS_DECL_ISUPPORTS_INHERITED

  virtual nsINode* GetParentObject() override
  {
    return nsSimpleContentList::GetParentObject();
  }
  virtual uint32_t Length() override
  {
    return nsSimpleContentList::Length();
  }
  virtual Element* GetElementAt(uint32_t aIndex) override
  {
    return mElements.SafeElementAt(aIndex)->AsElement();
  }

  virtual Element* GetFirstNamedElement(const nsAString& aName,
                                        bool& aFound) override
  {
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

  virtual void GetSupportedNames(nsTArray<nsString>& aNames) override
  {
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

  virtual JSObject* GetWrapperPreserveColorInternal() override
  {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
  virtual void PreserveWrapperInternal(nsISupports* aScriptObjectHolder) override
  {
    nsWrapperCache::PreserveWrapper(aScriptObjectHolder);
  }
  virtual JSObject* WrapObject(JSContext *aCx,
                               JS::Handle<JSObject*> aGivenProto) override
  {
    return HTMLCollection_Binding::Wrap(aCx, this, aGivenProto);
  }

  using nsBaseContentList::Item;

private:
  virtual ~SimpleHTMLCollection() {}
};

NS_IMPL_ISUPPORTS_INHERITED(SimpleHTMLCollection, nsSimpleContentList,
                            nsIHTMLCollection)

} // namespace dom
} // namespace mozilla

void
nsIdentifierMapEntry::AddNameElement(nsINode* aNode, Element* aElement)
{
  if (!mNameContentList) {
    mNameContentList = new SimpleHTMLCollection(aNode);
  }

  mNameContentList->AppendElement(aElement);
}

void
nsIdentifierMapEntry::RemoveNameElement(Element* aElement)
{
  if (mNameContentList) {
    mNameContentList->RemoveElement(aElement);
  }
}

bool
nsIdentifierMapEntry::HasIdElementExposedAsHTMLDocumentProperty()
{
  Element* idElement = GetIdElement();
  return idElement &&
         nsGenericHTMLElement::ShouldExposeIdAsHTMLDocumentProperty(idElement);
}

size_t
nsIdentifierMapEntry::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mKey.mString.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

// Helper structs for the content->subdoc map

class SubDocMapEntry : public PLDHashEntryHdr
{
public:
  // Both of these are strong references
  Element *mKey; // must be first, to look like PLDHashEntryStub
  nsIDocument *mSubDocument;
};


/**
 * A struct that holds all the information about a radio group.
 */
struct nsRadioGroupStruct
{
  nsRadioGroupStruct()
    : mRequiredRadioCount(0)
    , mGroupSuffersFromValueMissing(false)
  {}

  /**
   * A strong pointer to the currently selected radio button.
   */
  RefPtr<HTMLInputElement> mSelectedRadioButton;
  nsCOMArray<nsIFormControl> mRadioButtons;
  uint32_t mRequiredRadioCount;
  bool mGroupSuffersFromValueMissing;
};

// nsOnloadBlocker implementation
NS_IMPL_ISUPPORTS(nsOnloadBlocker, nsIRequest)

NS_IMETHODIMP
nsOnloadBlocker::GetName(nsACString &aResult)
{
  aResult.AssignLiteral("about:document-onload-blocker");
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::IsPending(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::GetStatus(nsresult *status)
{
  *status = NS_OK;
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::Cancel(nsresult status)
{
  return NS_OK;
}
NS_IMETHODIMP
nsOnloadBlocker::Suspend(void)
{
  return NS_OK;
}
NS_IMETHODIMP
nsOnloadBlocker::Resume(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  *aLoadGroup = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = nsIRequest::LOAD_NORMAL;
  return NS_OK;
}

NS_IMETHODIMP
nsOnloadBlocker::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  return NS_OK;
}

// ==================================================================

nsExternalResourceMap::nsExternalResourceMap()
  : mHaveShutDown(false)
{
}

nsIDocument*
nsExternalResourceMap::RequestResource(nsIURI* aURI,
                                       nsINode* aRequestingNode,
                                       nsIDocument* aDisplayDocument,
                                       ExternalResourceLoad** aPendingLoad)
{
  // If we ever start allowing non-same-origin loads here, we might need to do
  // something interesting with aRequestingPrincipal even for the hashtable
  // gets.
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aRequestingNode, "Must have a node");
  *aPendingLoad = nullptr;
  if (mHaveShutDown) {
    return nullptr;
  }

  // First, make sure we strip the ref from aURI.
  nsCOMPtr<nsIURI> clone;
  nsresult rv = aURI->CloneIgnoringRef(getter_AddRefs(clone));
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

  if (NS_FAILED(load->StartLoad(clone, aRequestingNode))) {
    // Make sure we don't thrash things by trying this load again, since
    // chances are it failed for good reasons (security check, etc).
    AddExternalResource(clone, nullptr, nullptr, aDisplayDocument);
  } else {
    load.forget(aPendingLoad);
  }

  return nullptr;
}

void
nsExternalResourceMap::EnumerateResources(nsIDocument::nsSubDocEnumFunc aCallback,
                                          void* aData)
{
  for (auto iter = mMap.Iter(); !iter.Done(); iter.Next()) {
    nsExternalResourceMap::ExternalResource* resource = iter.UserData();
    if (resource->mDocument && !aCallback(resource->mDocument, aData)) {
      break;
    }
  }
}

void
nsExternalResourceMap::Traverse(nsCycleCollectionTraversalCallback* aCallback) const
{
  // mPendingLoads will get cleared out as the requests complete, so
  // no need to worry about those here.
  for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
    nsExternalResourceMap::ExternalResource* resource = iter.UserData();

    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback,
                                       "mExternalResourceMap.mMap entry"
                                       "->mDocument");
    aCallback->NoteXPCOMChild(resource->mDocument);

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

void
nsExternalResourceMap::HideViewers()
{
  for (auto iter = mMap.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsIContentViewer> viewer = iter.UserData()->mViewer;
    if (viewer) {
      viewer->Hide();
    }
  }
}

void
nsExternalResourceMap::ShowViewers()
{
  for (auto iter = mMap.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsIContentViewer> viewer = iter.UserData()->mViewer;
    if (viewer) {
      viewer->Show();
    }
  }
}

void
TransferZoomLevels(nsIDocument* aFromDoc,
                   nsIDocument* aToDoc)
{
  MOZ_ASSERT(aFromDoc && aToDoc,
             "transferring zoom levels from/to null doc");

  nsPresContext* fromCtxt = aFromDoc->GetPresContext();
  if (!fromCtxt)
    return;

  nsPresContext* toCtxt = aToDoc->GetPresContext();
  if (!toCtxt)
    return;

  toCtxt->SetFullZoom(fromCtxt->GetFullZoom());
  toCtxt->SetBaseMinFontSize(fromCtxt->BaseMinFontSize());
  toCtxt->SetTextZoom(fromCtxt->TextZoom());
  toCtxt->SetOverrideDPPX(fromCtxt->GetOverrideDPPX());
}

void
TransferShowingState(nsIDocument* aFromDoc, nsIDocument* aToDoc)
{
  MOZ_ASSERT(aFromDoc && aToDoc,
             "transferring showing state from/to null doc");

  if (aFromDoc->IsShowing()) {
    aToDoc->OnPageShow(true, nullptr);
  }
}

nsresult
nsExternalResourceMap::AddExternalResource(nsIURI* aURI,
                                           nsIContentViewer* aViewer,
                                           nsILoadGroup* aLoadGroup,
                                           nsIDocument* aDisplayDocument)
{
  MOZ_ASSERT(aURI, "Unexpected call");
  MOZ_ASSERT((aViewer && aLoadGroup) || (!aViewer && !aLoadGroup),
             "Must have both or neither");

  RefPtr<PendingLoad> load;
  mPendingLoads.Remove(aURI, getter_AddRefs(load));

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocument> doc;
  if (aViewer) {
    doc = aViewer->GetDocument();
    NS_ASSERTION(doc, "Must have a document");

    if (doc->IsXULDocument()) {
      // We don't handle XUL stuff here yet.
      rv = NS_ERROR_NOT_AVAILABLE;
    } else {
      doc->SetDisplayDocument(aDisplayDocument);

      // Make sure that hiding our viewer will tear down its presentation.
      aViewer->SetSticky(false);

      rv = aViewer->Init(nullptr, nsIntRect(0, 0, 0, 0));
      if (NS_SUCCEEDED(rv)) {
        rv = aViewer->Open(nullptr, nullptr);
      }
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

  const nsTArray< nsCOMPtr<nsIObserver> > & obs = load->Observers();
  for (uint32_t i = 0; i < obs.Length(); ++i) {
    obs[i]->Observe(doc, "external-resource-document-created", nullptr);
  }

  return rv;
}

NS_IMPL_ISUPPORTS(nsExternalResourceMap::PendingLoad,
                  nsIStreamListener,
                  nsIRequestObserver)

NS_IMETHODIMP
nsExternalResourceMap::PendingLoad::OnStartRequest(nsIRequest *aRequest,
                                                   nsISupports *aContext)
{
  nsExternalResourceMap& map = mDisplayDocument->ExternalResourceMap();
  if (map.HaveShutDown()) {
    return NS_BINDING_ABORTED;
  }

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = SetupViewer(aRequest, getter_AddRefs(viewer),
                            getter_AddRefs(loadGroup));

  // Make sure to do this no matter what
  nsresult rv2 = map.AddExternalResource(mURI, viewer, loadGroup,
                                         mDisplayDocument);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (NS_FAILED(rv2)) {
    mTargetListener = nullptr;
    return rv2;
  }

  return mTargetListener->OnStartRequest(aRequest, aContext);
}

nsresult
nsExternalResourceMap::PendingLoad::SetupViewer(nsIRequest* aRequest,
                                                nsIContentViewer** aViewer,
                                                nsILoadGroup** aLoadGroup)
{
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
  nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", type.get(),
                                         getter_Copies(contractId));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
    do_GetService(contractId.get());
  NS_ENSURE_TRUE(docLoaderFactory, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsIStreamListener> listener;
  rv = docLoaderFactory->CreateInstance("external-resource", chan, newLoadGroup,
                                        type, nullptr, nullptr,
                                        getter_AddRefs(listener),
                                        getter_AddRefs(viewer));
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
nsExternalResourceMap::PendingLoad::OnDataAvailable(nsIRequest* aRequest,
                                                    nsISupports* aContext,
                                                    nsIInputStream* aStream,
                                                    uint64_t aOffset,
                                                    uint32_t aCount)
{
  // mTargetListener might be null if SetupViewer or AddExternalResource failed.
  NS_ENSURE_TRUE(mTargetListener, NS_ERROR_FAILURE);
  if (mDisplayDocument->ExternalResourceMap().HaveShutDown()) {
    return NS_BINDING_ABORTED;
  }
  return mTargetListener->OnDataAvailable(aRequest, aContext, aStream, aOffset,
                                          aCount);
}

NS_IMETHODIMP
nsExternalResourceMap::PendingLoad::OnStopRequest(nsIRequest* aRequest,
                                                  nsISupports* aContext,
                                                  nsresult aStatus)
{
  // mTargetListener might be null if SetupViewer or AddExternalResource failed
  if (mTargetListener) {
    nsCOMPtr<nsIStreamListener> listener;
    mTargetListener.swap(listener);
    return listener->OnStopRequest(aRequest, aContext, aStatus);
  }

  return NS_OK;
}

nsresult
nsExternalResourceMap::PendingLoad::StartLoad(nsIURI* aURI,
                                              nsINode* aRequestingNode)
{
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aRequestingNode, "Must have a node");

  nsCOMPtr<nsILoadGroup> loadGroup =
    aRequestingNode->OwnerDoc()->GetDocumentLoadGroup();

  nsresult rv = NS_OK;
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aURI,
                     aRequestingNode,
                     nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
                     nsIContentPolicy::TYPE_OTHER,
                     nullptr, // aPerformanceStorage
                     loadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  mURI = aURI;

  return channel->AsyncOpen2(this);
}

NS_IMPL_ISUPPORTS(nsExternalResourceMap::LoadgroupCallbacks,
                  nsIInterfaceRequestor)

#define IMPL_SHIM(_i) \
  NS_IMPL_ISUPPORTS(nsExternalResourceMap::LoadgroupCallbacks::_i##Shim, _i)

IMPL_SHIM(nsILoadContext)
IMPL_SHIM(nsIProgressEventSink)
IMPL_SHIM(nsIChannelEventSink)
IMPL_SHIM(nsISecurityEventSink)
IMPL_SHIM(nsIApplicationCacheContainer)

#undef IMPL_SHIM

#define IID_IS(_i) aIID.Equals(NS_GET_IID(_i))

#define TRY_SHIM(_i)                                                       \
  PR_BEGIN_MACRO                                                           \
    if (IID_IS(_i)) {                                                      \
      nsCOMPtr<_i> real = do_GetInterface(mCallbacks);                     \
      if (!real) {                                                         \
        return NS_NOINTERFACE;                                             \
      }                                                                    \
      nsCOMPtr<_i> shim = new _i##Shim(this, real);                        \
      shim.forget(aSink);                                                  \
      return NS_OK;                                                        \
    }                                                                      \
  PR_END_MACRO

NS_IMETHODIMP
nsExternalResourceMap::LoadgroupCallbacks::GetInterface(const nsIID & aIID,
                                                        void **aSink)
{
  if (mCallbacks &&
      (IID_IS(nsIPrompt) || IID_IS(nsIAuthPrompt) || IID_IS(nsIAuthPrompt2) ||
       IID_IS(nsITabChild))) {
    return mCallbacks->GetInterface(aIID, aSink);
  }

  *aSink = nullptr;

  TRY_SHIM(nsILoadContext);
  TRY_SHIM(nsIProgressEventSink);
  TRY_SHIM(nsIChannelEventSink);
  TRY_SHIM(nsISecurityEventSink);
  TRY_SHIM(nsIApplicationCacheContainer);

  return NS_NOINTERFACE;
}

#undef TRY_SHIM
#undef IID_IS

nsExternalResourceMap::ExternalResource::~ExternalResource()
{
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
class nsDOMStyleSheetSetList final : public DOMStringList
{
public:
  explicit nsDOMStyleSheetSetList(nsIDocument* aDocument);

  void Disconnect()
  {
    mDocument = nullptr;
  }

  virtual void EnsureFresh() override;

protected:
  nsIDocument* mDocument;  // Our document; weak ref.  It'll let us know if it
                           // dies.
};

nsDOMStyleSheetSetList::nsDOMStyleSheetSetList(nsIDocument* aDocument)
  : mDocument(aDocument)
{
  NS_ASSERTION(mDocument, "Must have document!");
}

void
nsDOMStyleSheetSetList::EnsureFresh()
{
  MOZ_ASSERT(NS_IsMainThread());

  mNames.Clear();

  if (!mDocument) {
    return; // Spec says "no exceptions", and we have no style sets if we have
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
nsIDocument::SelectorCache::SelectorCache(nsIEventTarget* aEventTarget)
  : nsExpirationTracker<SelectorCacheKey, 4>(
      1000, "nsIDocument::SelectorCache", aEventTarget)
{ }

nsIDocument::SelectorCache::~SelectorCache()
{
  AgeAllGenerations();
}

void nsIDocument::SelectorCache::NotifyExpired(SelectorCacheKey* aSelector)
{
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

struct nsIDocument::FrameRequest
{
  FrameRequest(FrameRequestCallback& aCallback,
               int32_t aHandle) :
    mCallback(&aCallback),
    mHandle(aHandle)
  {}

  // Conversion operator so that we can append these to a
  // FrameRequestCallbackList
  operator const RefPtr<FrameRequestCallback>& () const {
    return mCallback;
  }

  // Comparator operators to allow RemoveElementSorted with an
  // integer argument on arrays of FrameRequest
  bool operator==(int32_t aHandle) const {
    return mHandle == aHandle;
  }
  bool operator<(int32_t aHandle) const {
    return mHandle < aHandle;
  }

  RefPtr<FrameRequestCallback> mCallback;
  int32_t mHandle;
};

static already_AddRefed<mozilla::dom::NodeInfo> nullNodeInfo;

// ==================================================================
// =
// ==================================================================
nsIDocument::nsIDocument()
  : nsINode(nullNodeInfo),
    DocumentOrShadowRoot(*this),
    mReferrerPolicySet(false),
    mReferrerPolicy(mozilla::net::RP_Unset),
    mBlockAllMixedContent(false),
    mBlockAllMixedContentPreloads(false),
    mUpgradeInsecureRequests(false),
    mUpgradeInsecurePreloads(false),
    mCharacterSet(WINDOWS_1252_ENCODING),
    mCharacterSetSource(0),
    mParentDocument(nullptr),
    mCachedRootElement(nullptr),
    mNodeInfoManager(nullptr),
#ifdef DEBUG
    mStyledLinksCleared(false),
#endif
    mBidiEnabled(false),
    mMathMLEnabled(false),
    mIsInitialDocumentInWindow(false),
    mIgnoreDocGroupMismatches(false),
    mLoadedAsData(false),
    mLoadedAsInteractiveData(false),
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
    mIsSyntheticDocument(false),
    mHasLinksToUpdateRunnable(false),
    mFlushingPendingLinkUpdates(false),
    mMayHaveDOMMutationObservers(false),
    mMayHaveAnimationObservers(false),
    mHasMixedActiveContentLoaded(false),
    mHasMixedActiveContentBlocked(false),
    mHasMixedDisplayContentLoaded(false),
    mHasMixedDisplayContentBlocked(false),
    mHasMixedContentObjectSubrequest(false),
    mHasCSP(false),
    mHasUnsafeEvalCSP(false),
    mHasUnsafeInlineCSP(false),
    mHasTrackingContentBlocked(false),
    mHasTrackingContentLoaded(false),
    mBFCacheDisallowed(false),
    mHasHadDefaultView(false),
    mStyleSheetChangeEventsEnabled(false),
    mIsSrcdocDocument(false),
    mDidDocumentOpen(false),
    mHasDisplayDocument(false),
    mFontFaceSetDirty(true),
    mGetUserFontSetCalled(false),
    mDidFireDOMContentLoaded(true),
    mHasScrollLinkedEffect(false),
    mFrameRequestCallbacksScheduled(false),
    mIsTopLevelContentDocument(false),
    mIsContentDocument(false),
    mDidCallBeginLoad(false),
    mAllowPaymentRequest(false),
    mEncodingMenuDisabled(false),
    mIsShadowDOMEnabled(false),
    mIsSVGGlyphsDocument(false),
    mInDestructor(false),
    mIsGoingAway(false),
    mInXBLUpdate(false),
    mNeedsReleaseAfterStackRefCntRelease(false),
    mStyleSetFilled(false),
    mSSApplicableStateNotificationPending(false),
    mMayHaveTitleElement(false),
    mDOMLoadingSet(false),
    mDOMInteractiveSet(false),
    mDOMCompleteSet(false),
    mAutoFocusFired(false),
    mScrolledToRefAlready(false),
    mChangeScrollPosWhenScrollingToRef(false),
    mHasWarnedAboutBoxObjects(false),
    mDelayFrameLoaderInitialization(false),
    mSynchronousDOMContentLoaded(false),
    mMaybeServiceWorkerControlled(false),
    mValidWidth(false),
    mValidHeight(false),
    mAutoSize(false),
    mAllowZoom(false),
    mAllowDoubleTapZoom(false),
    mValidScaleFloat(false),
    mValidMaxScale(false),
    mScaleStrEmpty(false),
    mWidthStrEmpty(false),
    mParserAborted(false),
    mReportedUseCounters(false),
    mHasReportedShadowDOMUsage(false),
#ifdef DEBUG
    mWillReparent(false),
#endif
    mPendingFullscreenRequests(0),
    mXMLDeclarationBits(0),
    mOnloadBlockCount(0),
    mAsyncOnloadBlockCount(0),
    mCompatMode(eCompatibility_FullStandards),
    mReadyState(ReadyState::READYSTATE_UNINITIALIZED),
#ifdef MOZILLA_INTERNAL_API
    mVisibilityState(dom::VisibilityState::Hidden),
#else
    mDummy(0),
#endif
    mType(eUnknown),
    mDefaultElementType(0),
    mAllowXULXBL(eTriUnset),
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
    mNotifiedPageForUseCounter(0),
    mUserHasInteracted(false),
    mUserGestureActivated(false),
    mStackRefCnt(0),
    mUpdateNestLevel(0),
    mViewportType(Unknown),
    mViewportOverflowType(ViewportOverflowType::NoOverflow),
    mSubDocuments(nullptr),
    mHeaderData(nullptr),
    mFlashClassification(FlashClassification::Unclassified),
    mBoxObjectTable(nullptr),
    mCurrentOrientationAngle(0),
    mCurrentOrientationType(OrientationType::Portrait_primary),
    mServoRestyleRootDirtyBits(0),
    mThrowOnDynamicMarkupInsertionCounter(0),
    mIgnoreOpensDuringUnloadCounter(0)
{
  SetIsInDocument();
}

nsDocument::nsDocument(const char* aContentType)
  : nsIDocument()
{
  SetContentTypeInternal(nsDependentCString(aContentType));

  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug, ("DOCUMENT %p created", this));

  // Start out mLastStyleSheetSet as null, per spec
  SetDOMStringToNull(mLastStyleSheetSet);

  // void state used to differentiate an empty source from an unselected source
  mPreloadPictureFoundSource.SetIsVoid(true);
  // For determining if this is a flash document which should be
  // blocked based on its principal.
  mPrincipalFlashClassifier = new PrincipalFlashClassifier();
}

void
nsIDocument::ClearAllBoxObjects()
{
  if (mBoxObjectTable) {
    for (auto iter = mBoxObjectTable->Iter(); !iter.Done(); iter.Next()) {
      nsPIBoxObject* boxObject = iter.UserData();
      if (boxObject) {
        boxObject->Clear();
      }
    }
    delete mBoxObjectTable;
    mBoxObjectTable = nullptr;
  }
}

nsIDocument::~nsIDocument()
{
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

bool
nsIDocument::IsAboutPage() const
{
  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  nsCOMPtr<nsIURI> uri;
  principal->GetURI(getter_AddRefs(uri));
  bool isAboutScheme = true;
  if (uri) {
    uri->SchemeIs("about", &isAboutScheme);
  }
  return isAboutScheme;
}

nsDocument::~nsDocument()
{
  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug, ("DOCUMENT %p destroyed", this));

  NS_ASSERTION(!mIsShowing, "Destroying a currently-showing document");

  if (IsTopLevelContentDocument()) {
    //don't report for about: pages
    if (!IsAboutPage()) {
      // Record the page load
      uint32_t pageLoaded = 1;
      Accumulate(Telemetry::MIXED_CONTENT_UNBLOCK_COUNTER, pageLoaded);
      // Record the mixed content status of the docshell in Telemetry
      enum {
        NO_MIXED_CONTENT = 0, // There is no Mixed Content on the page
        MIXED_DISPLAY_CONTENT = 1, // The page attempted to load Mixed Display Content
        MIXED_ACTIVE_CONTENT = 2, // The page attempted to load Mixed Active Content
        MIXED_DISPLAY_AND_ACTIVE_CONTENT = 3 // The page attempted to load Mixed Display & Mixed Active Content
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
      } else if (hasMixedActive){
        mixedContentLevel = MIXED_ACTIVE_CONTENT;
      } else if (hasMixedDisplay) {
        mixedContentLevel = MIXED_DISPLAY_CONTENT;
      }
      Accumulate(Telemetry::MIXED_CONTENT_PAGE_LOAD, mixedContentLevel);

      // record mixed object subrequest telemetry
      if (mHasMixedContentObjectSubrequest) {
        /* mixed object subrequest loaded on page*/
        Accumulate(Telemetry::MIXED_CONTENT_OBJECT_SUBREQUEST, 1);
      } else {
        /* no mixed object subrequests loaded on page*/
        Accumulate(Telemetry::MIXED_CONTENT_OBJECT_SUBREQUEST, 0);
      }

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
    }
  }

  ReportUseCounters();

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

  // Destroy link map now so we don't waste time removing
  // links one by one
  DestroyElementMaps();

  nsAutoScriptBlocker scriptBlocker;

  // Invalidate cached array of child nodes
  InvalidateChildNodes();

  for (uint32_t indx = mChildren.ChildCount(); indx-- != 0; ) {
    mChildren.ChildAt(indx)->UnbindFromTree();
    mChildren.RemoveChildAt(indx);
  }
  mFirstChild = nullptr;
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

  delete mHeaderData;

  ClearAllBoxObjects();

  mPendingTitleChangeEvent.Revoke();

  mPlugins.Clear();
}

NS_INTERFACE_TABLE_HEAD(nsDocument)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsDocument, nsISupports, nsINode)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsINode)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIDocument)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIScriptObjectPrincipal)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, mozilla::dom::EventTarget)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsISupportsWeakReference)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIRadioGroupContainer)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIMutationObserver)
    NS_INTERFACE_TABLE_ENTRY(nsDocument, nsIApplicationCacheContainer)
  NS_INTERFACE_TABLE_END
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsDocument)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDocument)
NS_IMETHODIMP_(MozExternalRefCountType)
nsDocument::Release()
{
  MOZ_ASSERT(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(nsDocument);
  nsISupports* base = NS_CYCLE_COLLECTION_CLASSNAME(nsDocument)::Upcast(this);
  bool shouldDelete = false;
  nsrefcnt count = mRefCnt.decr(base, &shouldDelete);
  NS_LOG_RELEASE(this, count, "nsDocument");
  if (count == 0) {
    if (mStackRefCnt && !mNeedsReleaseAfterStackRefCntRelease) {
      mNeedsReleaseAfterStackRefCntRelease = true;
      NS_ADDREF_THIS();
      return mRefCnt.get();
    }
    mRefCnt.incr(base);
    nsNodeUtils::LastRelease(this);
    mRefCnt.decr(base);
    if (shouldDelete) {
      mRefCnt.stabilizeForDeletion();
      DeleteCycleCollectable();
    }
  }
  return count;
}

NS_IMETHODIMP_(void)
nsDocument::DeleteCycleCollectable()
{
  delete this;
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsDocument)
  if (Element::CanSkip(tmp, aRemovingAllowed)) {
    EventListenerManager* elm = tmp->GetExistingListenerManager();
    if (elm) {
      elm->MarkForCC();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsDocument)
  return Element::CanSkipInCC(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsDocument)
  return Element::CanSkipThis(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

static const char* kNSURIs[] = {
  "([none])",
  "(xmlns)",
  "(xml)",
  "(xhtml)",
  "(XLink)",
  "(XSLT)",
  "(XBL)",
  "(MathML)",
  "(RDF)",
  "(XUL)"
};

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsDocument)
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
    if (tmp->mDocumentURI)
      uri = tmp->mDocumentURI->GetSpecOrDefault();
    if (nsid < ArrayLength(kNSURIs)) {
      SprintfLiteral(name, "nsDocument %s %s %s",
                     loadedAsData.get(), kNSURIs[nsid], uri.get());
    }
    else {
      SprintfLiteral(name, "nsDocument %s %s",
                     loadedAsData.get(), uri.get());
    }
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsDocument, tmp->mRefCnt.get())
  }

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  if (tmp->mMaybeEndOutermostXBLUpdateRunner) {
    // The cached runnable keeps a reference to the document object..
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                       "mMaybeEndOutermostXBLUpdateRunner.mObj");
    cb.NoteXPCOMChild(ToSupports(tmp));
  }

  for (auto iter = tmp->mIdentifierMap.ConstIter(); !iter.Done();
       iter.Next()) {
    iter.Get()->Traverse(&cb);
  }

  tmp->mExternalResourceMap.Traverse(&cb);

  // Traverse the mChildren nsAttrAndChildArray.
  for (int32_t indx = int32_t(tmp->mChildren.ChildCount()); indx > 0; --indx) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildren[i]");
    cb.NoteXPCOMChild(tmp->mChildren.ChildAt(indx - 1));
  }

  // Traverse all nsIDocument pointer members.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSecurityInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDisplayDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFontFaceSet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadyForIdle)

  // Traverse all nsDocument nsCOMPtrs.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMStyleSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheetSetList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptLoader)

  for (auto iter = tmp->mRadioGroups.Iter(); !iter.Done(); iter.Next()) {
    nsRadioGroupStruct* radioGroup = iter.UserData();
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
      cb, "mRadioGroups entry->mSelectedRadioButton");
    cb.NoteXPCOMChild(ToSupports(radioGroup->mSelectedRadioButton));

    uint32_t i, count = radioGroup->mRadioButtons.Count();
    for (i = 0; i < count; ++i) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
        cb, "mRadioGroups entry->mRadioButtons[i]");
      cb.NoteXPCOMChild(radioGroup->mRadioButtons[i]);
    }
  }

  // The boxobject for an element will only exist as long as it's in the
  // document, so we'll traverse the table here instead of from the element.
  if (tmp->mBoxObjectTable) {
    for (auto iter = tmp->mBoxObjectTable->Iter(); !iter.Done(); iter.Next()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mBoxObjectTable entry");
      cb.NoteXPCOMChild(iter.UserData());
    }
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLayoutHistoryState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOnloadBlocker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFirstBaseNodeWithHref)
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

  // Traverse all our nsCOMArrays.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheets)
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

      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                         "mSubDocuments entry->mKey");
      cb.NoteXPCOMChild(entry->mKey);
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                         "mSubDocuments entry->mSubDocument");
      cb.NoteXPCOMChild(entry->mSubDocument);
    }
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCSSLoader)

  // We own only the items in mDOMMediaQueryLists that have listeners;
  // this reference is managed by their AddListener and RemoveListener
  // methods.
  for (MediaQueryList* mql = tmp->mDOMMediaQueryLists.getFirst(); mql;
       mql = static_cast<LinkedListElement<MediaQueryList>*>(mql)->getNext()) {
    if (mql->HasListeners()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mDOMMediaQueryLists item");
      cb.NoteXPCOMChild(mql);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDocument)

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsDocument)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDocument)
  tmp->mInUnlinkOrDeletion = true;

  // Clear out our external resources
  tmp->mExternalResourceMap.Shutdown();

  nsAutoScriptBlocker scriptBlocker;

  nsINode::Unlink(tmp);

  // Unlink the mChildren nsAttrAndChildArray.
  uint32_t childCount = tmp->mChildren.ChildCount();
  if (childCount) {
    while (childCount-- > 0) {
      // Hold a strong ref to the node when we remove it, because we may be
      // the last reference to it.  We need to call TakeChildAt() and
      // update mFirstChild before calling UnbindFromTree, since this last
      // can notify various observers and they should really see consistent
      // tree state.
      // If this code changes, change the corresponding code in
      // FragmentOrElement's unlink impl and ContentUnbinder::UnbindSubtree.
      nsCOMPtr<nsIContent> child = tmp->mChildren.TakeChildAt(childCount);
      if (childCount == 0) {
        tmp->mFirstChild = nullptr;
      }
      child->UnbindFromTree();
    }
  }
  tmp->mFirstChild = nullptr;

  tmp->UnlinkOriginalDocumentIfStatic();

  tmp->mCachedRootElement = nullptr; // Avoid a dangling pointer
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDisplayDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFirstBaseNodeWithHref)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMaybeEndOutermostXBLUpdateRunner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMImplementation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mImageMaps)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedEncoder)
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOrientationPendingPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFontFaceSet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadyForIdle);

  tmp->mParentDocument = nullptr;

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPreloadingImages)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIntersectionObservers)

  tmp->ClearAllBoxObjects();

  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    tmp->UnsetFlags(NODE_HAS_LISTENERMANAGER);
    tmp->mListenerManager = nullptr;
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMStyleSheets)

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

  tmp->mRadioGroups.Clear();

  // nsDocument has a pretty complex destructor, so we're going to
  // assume that *most* cycles you actually want to break somewhere
  // else, and not unlink an awful lot here.

  tmp->mIdentifierMap.Clear();
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
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsresult
nsDocument::Init()
{
  if (mCSSLoader || mStyleImageLoader || mNodeInfoManager || mScriptLoader) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // Force initialization.
  nsINode::nsSlots* slots = Slots();

  // Prepend self as mutation-observer whether we need it or not (some
  // subclasses currently do, other don't). This is because the code in
  // nsNodeUtils always notifies the first observer first, expecting the
  // first observer to be the document.
  NS_ENSURE_TRUE(slots->mMutationObservers.PrependElementUnlessExists(static_cast<nsIMutationObserver*>(this)),
                 NS_ERROR_OUT_OF_MEMORY);


  mOnloadBlocker = new nsOnloadBlocker();
  mCSSLoader = new mozilla::css::Loader(this);
  // Assume we're not quirky, until we know otherwise
  mCSSLoader->SetCompatibilityMode(eCompatibility_FullStandards);

  mStyleImageLoader = new mozilla::css::ImageLoader(this);

  mNodeInfoManager = new nsNodeInfoManager();
  nsresult rv = mNodeInfoManager->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // mNodeInfo keeps NodeInfoManager alive!
  mNodeInfo = mNodeInfoManager->GetDocumentNodeInfo();
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_OUT_OF_MEMORY);
  MOZ_ASSERT(mNodeInfo->NodeType() == DOCUMENT_NODE,
             "Bad NodeType in aNodeInfo");

  NS_ASSERTION(OwnerDoc() == this, "Our nodeinfo is busted!");

  // Set this when document is initialized and value stays the same for the
  // lifetime of the document.
  mIsShadowDOMEnabled = nsContentUtils::IsShadowDOMEnabled();

  // If after creation the owner js global is not set for a document
  // we use the default compartment for this document, instead of creating
  // wrapper in some random compartment when the document is exposed to js
  // via some events.
  nsCOMPtr<nsIGlobalObject> global = xpc::NativeGlobal(xpc::PrivilegedJunkScope());
  NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);
  mScopeObject = do_GetWeakReference(global);
  MOZ_ASSERT(mScopeObject);

  mScriptLoader = new dom::ScriptLoader(this);

  mozilla::HoldJSObjects(this);

  return NS_OK;
}

void
nsIDocument::DeleteAllProperties()
{
  PropertyTable().DeleteAllProperties();
}

void
nsIDocument::DeleteAllPropertiesFor(nsINode* aNode)
{
  PropertyTable().DeleteAllPropertiesFor(aNode);
}

bool
nsIDocument::IsVisibleConsideringAncestors() const
{
  const nsIDocument *parent = this;
  do {
    if (!parent->IsVisible()) {
      return false;
    }
  } while ((parent = parent->GetParentDocument()));

  return true;
      }

void
nsIDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIPrincipal> principal;
  if (aChannel) {
    // Note: this code is duplicated in XULDocument::StartDocumentLoad and
    // nsScriptSecurityManager::GetChannelResultPrincipal.
    // Note: this should match nsDocShell::OnLoadingSite
    NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));

    bool isWyciwyg = false;
    uri->SchemeIs("wyciwyg", &isWyciwyg);
    if (isWyciwyg) {
      nsCOMPtr<nsIURI> cleanURI;
      nsresult rv =
        nsContentUtils::RemoveWyciwygScheme(uri, getter_AddRefs(cleanURI));
      if (NS_SUCCEEDED(rv)) {
        uri = cleanURI;
      }
    }

    nsIScriptSecurityManager *securityManager =
      nsContentUtils::GetSecurityManager();
    if (securityManager) {
      securityManager->GetChannelResultPrincipal(aChannel,
                                                 getter_AddRefs(principal));
    }
  }

  principal = MaybeDowngradePrincipal(principal);

  ResetToURI(uri, aLoadGroup, principal);

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

void
nsIDocument::ResetToURI(nsIURI* aURI,
                        nsILoadGroup* aLoadGroup,
                        nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(aURI, "Null URI passed to ResetToURI");

  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug,
          ("DOCUMENT %p ResetToURI %s", this, aURI->GetSpecOrDefault().get()));

  mSecurityInfo = nullptr;

  mDocumentLoadGroup = nullptr;

  // Delete references to sub-documents and kill the subdocument map,
  // if any. It holds strong references
  delete mSubDocuments;
  mSubDocuments = nullptr;

  // Destroy link map now so we don't waste time removing
  // links one by one
  DestroyElementMaps();

  bool oldVal = mInUnlinkOrDeletion;
  mInUnlinkOrDeletion = true;
  uint32_t count = mChildren.ChildCount();
  { // Scope for update
    MOZ_AUTO_DOC_UPDATE(this, true);

    // Invalidate cached array of child nodes
    InvalidateChildNodes();

    for (int32_t i = int32_t(count) - 1; i >= 0; i--) {
      nsCOMPtr<nsIContent> content = mChildren.ChildAt(i);

      nsIContent* previousSibling = content->GetPreviousSibling();

      if (nsINode::GetFirstChild() == content) {
        mFirstChild = content->GetNextSibling();
      }
      mChildren.RemoveChildAt(i);
      if (content == mCachedRootElement) {
        // Immediately clear mCachedRootElement, now that it's been removed
        // from mChildren, so that GetRootElement() will stop returning this
        // now-stale value.
        mCachedRootElement = nullptr;
      }
      nsNodeUtils::ContentRemoved(this, content, previousSibling);
      content->UnbindFromTree();
    }
    MOZ_ASSERT(!mCachedRootElement,
               "After removing all children, there should be no root elem");
  }
  mInUnlinkOrDeletion = oldVal;

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
  // This ensures that, during teardown, the document and the dying window (which
  // already nulled out its document pointer and cached the principal) have
  // matching principals.
  SetPrincipal(nullptr);

  // Clear the original URI so SetDocumentURI sets it.
  mOriginalURI = nullptr;

  SetDocumentURI(aURI);
  mChromeXHRDocURI = nullptr;
  // If mDocumentBaseURI is null, nsIDocument::GetBaseURI() returns
  // mDocumentURI.
  mDocumentBaseURI = nullptr;
  mChromeXHRDocBaseURI = nullptr;

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
        do_GetService("@mozilla.org/network/request-context-service;1");
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
  mReferrer.Truncate();

  mXMLDeclarationBits = 0;

  // Now get our new principal
  if (aPrincipal) {
    SetPrincipal(aPrincipal);
  } else {
    nsIScriptSecurityManager *securityManager =
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
      nsresult rv = securityManager->
        GetLoadContextCodebasePrincipal(mDocumentURI, loadContext,
                                        getter_AddRefs(principal));
      if (NS_SUCCEEDED(rv)) {
        SetPrincipal(principal);
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

already_AddRefed<nsIPrincipal>
nsIDocument::MaybeDowngradePrincipal(nsIPrincipal* aPrincipal)
{
  if (!aPrincipal) {
    return nullptr;
  }

  // We can't load a document with an expanded principal. If we're given one,
  // automatically downgrade it to the last principal it subsumes (which is the
  // extension principal, in the case of extension content scripts).
  auto* basePrin = BasePrincipal::Cast(aPrincipal);
  if (basePrin->Is<ExpandedPrincipal>()) {
    MOZ_DIAGNOSTIC_ASSERT(false, "Should never try to create a document with "
                                 "an expanded principal");

    auto* expanded = basePrin->As<ExpandedPrincipal>();
    return do_AddRef(expanded->WhiteList().LastElement());
  }

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    // We basically want the parent document here, but because this is very
    // early in the load, GetParentDocument() returns null, so we use the
    // docshell hierarchy to get this information instead.
    if (mDocumentContainer) {
      nsCOMPtr<nsIDocShellTreeItem> parentDocShellItem;
      mDocumentContainer->GetParent(getter_AddRefs(parentDocShellItem));
      nsCOMPtr<nsIDocShell> parentDocShell = do_QueryInterface(parentDocShellItem);
      if (parentDocShell) {
        nsCOMPtr<nsIDocument> parentDoc;
        parentDoc = parentDocShell->GetDocument();
        if (!parentDoc ||
            !nsContentUtils::IsSystemPrincipal(parentDoc->NodePrincipal())) {
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

void
nsIDocument::RemoveDocStyleSheetsFromStyleSets()
{
  // The stylesheets should forget us
  for (StyleSheet* sheet : Reversed(mStyleSheets)) {
    sheet->ClearAssociatedDocumentOrShadowRoot();

    if (sheet->IsApplicable()) {
      nsCOMPtr<nsIPresShell> shell = GetShell();
      if (shell) {
        shell->StyleSet()->RemoveDocStyleSheet(sheet);
      }
    }
    // XXX Tell observers?
  }
}

void
nsIDocument::RemoveStyleSheetsFromStyleSets(
    const nsTArray<RefPtr<StyleSheet>>& aSheets,
    SheetType aType)
{
  // The stylesheets should forget us
  for (StyleSheet* sheet : Reversed(aSheets)) {
    sheet->ClearAssociatedDocumentOrShadowRoot();

    if (sheet->IsApplicable()) {
      nsCOMPtr<nsIPresShell> shell = GetShell();
      if (shell) {
        shell->StyleSet()->RemoveStyleSheet(aType, sheet);
      }
    }
    // XXX Tell observers?
  }
}

void
nsIDocument::ResetStylesheetsToURI(nsIURI* aURI)
{
  MOZ_ASSERT(aURI);

  if (mStyleSetFilled) {
    // Skip removing style sheets from the style set if we know we haven't
    // filled the style set.  (This allows us to avoid calling
    // GetStyleBackendType() too early.)
    RemoveDocStyleSheetsFromStyleSets();
    RemoveStyleSheetsFromStyleSets(mAdditionalSheets[eAgentSheet], SheetType::Agent);
    RemoveStyleSheetsFromStyleSets(mAdditionalSheets[eUserSheet], SheetType::User);
    RemoveStyleSheetsFromStyleSets(mAdditionalSheets[eAuthorSheet], SheetType::Doc);

    if (nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance()) {
      RemoveStyleSheetsFromStyleSets(
        *sheetService->AuthorStyleSheets(), SheetType::Doc);
    }

    mStyleSetFilled = false;
  }

  // Release all the sheets
  mStyleSheets.Clear();
  for (auto& sheets : mAdditionalSheets) {
    sheets.Clear();
  }

  // NOTE:  We don't release the catalog sheets.  It doesn't really matter
  // now, but it could in the future -- in which case not releasing them
  // is probably the right thing to do.

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

  // Now set up our style sets
  if (nsIPresShell* shell = GetShell()) {
    FillStyleSet(shell->StyleSet());
    if (shell->StyleSet()->StyleSheetsHaveChanged()) {
      shell->ApplicableStylesChanged();
    }
  }
}

static void
AppendSheetsToStyleSet(ServoStyleSet* aStyleSet,
                       const nsTArray<RefPtr<StyleSheet>>& aSheets,
                       SheetType aType)
{
  for (StyleSheet* sheet : Reversed(aSheets)) {
    aStyleSet->AppendStyleSheet(aType, sheet);
  }
}


void
nsIDocument::FillStyleSet(ServoStyleSet* aStyleSet)
{
  MOZ_ASSERT(aStyleSet, "Must have a style set");
  MOZ_ASSERT(aStyleSet->SheetCount(SheetType::Doc) == 0,
             "Style set already has document sheets?");

  MOZ_ASSERT(!mStyleSetFilled);

  for (StyleSheet* sheet : Reversed(mStyleSheets)) {
    if (sheet->IsApplicable()) {
      aStyleSet->AddDocStyleSheet(sheet, this);
    }
  }

  if (nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance()) {
    nsTArray<RefPtr<StyleSheet>>& sheets =
      *sheetService->AuthorStyleSheets();
    for (StyleSheet* sheet : sheets) {
      aStyleSet->AppendStyleSheet(SheetType::Doc, sheet);
    }
  }

  AppendSheetsToStyleSet(aStyleSet, mAdditionalSheets[eAgentSheet],
                         SheetType::Agent);
  AppendSheetsToStyleSet(aStyleSet, mAdditionalSheets[eUserSheet],
                         SheetType::User);
  AppendSheetsToStyleSet(aStyleSet, mAdditionalSheets[eAuthorSheet],
                         SheetType::Doc);

  mStyleSetFilled = true;
}

static void
WarnIfSandboxIneffective(nsIDocShell* aDocShell,
                         uint32_t aSandboxFlags,
                         nsIChannel* aChannel)
{
  // If the document is sandboxed (via the HTML5 iframe sandbox
  // attribute) and both the allow-scripts and allow-same-origin
  // keywords are supplied, the sandboxed document can call into its
  // parent document and remove its sandboxing entirely - we print a
  // warning to the web console in this case.
  if (aSandboxFlags & SANDBOXED_NAVIGATION &&
      !(aSandboxFlags & SANDBOXED_SCRIPTS) &&
      !(aSandboxFlags & SANDBOXED_ORIGIN)) {
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
    aDocShell->GetSameTypeParent(getter_AddRefs(parentAsItem));
    nsCOMPtr<nsIDocShell> parentDocShell = do_QueryInterface(parentAsItem);
    if (!parentDocShell) {
      return;
    }

    // Don't warn if our parent is not the top-level document.
    nsCOMPtr<nsIDocShellTreeItem> grandParentAsItem;
    parentDocShell->GetSameTypeParent(getter_AddRefs(grandParentAsItem));
    if (grandParentAsItem) {
      return;
    }

    nsCOMPtr<nsIChannel> parentChannel;
    parentDocShell->GetCurrentDocumentChannel(getter_AddRefs(parentChannel));
    if (!parentChannel) {
      return;
    }
    nsresult rv = nsContentUtils::CheckSameOrigin(aChannel, parentChannel);
    if (NS_FAILED(rv)) {
      return;
    }

    nsCOMPtr<nsIDocument> parentDocument = parentDocShell->GetDocument();
    nsCOMPtr<nsIURI> iframeUri;
    parentChannel->GetURI(getter_AddRefs(iframeUri));
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Iframe Sandbox"),
                                    parentDocument,
                                    nsContentUtils::eSECURITY_PROPERTIES,
                                    "BothAllowScriptsAndSameOriginPresent",
                                    nullptr, 0, iframeUri);
  }
}

bool
nsIDocument::IsSynthesized() {
  nsCOMPtr<nsILoadInfo> loadInfo = mChannel ? mChannel->GetLoadInfo() : nullptr;
  return loadInfo && loadInfo->GetServiceWorkerTaintingSynthesized();
}

bool
nsDocument::IsShadowDOMEnabled(JSContext* aCx, JSObject* aGlobal)
{
  MOZ_DIAGNOSTIC_ASSERT(JS_IsGlobalObject(aGlobal));
  nsCOMPtr<nsPIDOMWindowInner> window = xpc::WindowOrNull(aGlobal);

  nsIDocument* doc = window ? window->GetExtantDoc() : nullptr;
  if (!doc) {
    return false;
  }

  return doc->IsShadowDOMEnabled();
}

// static
bool
nsDocument::IsShadowDOMEnabledAndCallerIsChromeOrAddon(JSContext* aCx,
                                                       JSObject* aObject)
{
  if (IsShadowDOMEnabled(aCx, aObject)) {
    nsIPrincipal* principal = nsContentUtils::SubjectPrincipal(aCx);
    return principal &&
      (nsContentUtils::IsSystemPrincipal(principal) ||
       principal->GetIsAddonOrExpandedAddonPrincipal());
  }

  return false;
}

bool
nsDocument::IsShadowDOMEnabled(const nsINode* aNode)
{
  return aNode->OwnerDoc()->IsShadowDOMEnabled();
}

nsresult
nsDocument::StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                              nsILoadGroup* aLoadGroup,
                              nsISupports* aContainer,
                              nsIStreamListener **aDocListener,
                              bool aReset, nsIContentSink* aSink)
{
  if (MOZ_LOG_TEST(gDocumentLeakPRLog, LogLevel::Debug)) {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug,
            ("DOCUMENT %p StartDocumentLoad %s",
             this, uri ? uri->GetSpecOrDefault().get() : ""));
  }

  MOZ_ASSERT(NodePrincipal()->GetAppId() != nsIScriptSecurityManager::UNKNOWN_APP_ID,
             "Document should never have UNKNOWN_APP_ID");

  MOZ_ASSERT(GetReadyStateEnum() == nsIDocument::READYSTATE_UNINITIALIZED,
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
    CSSLoader()->SetEnabled(false); // Do not load/process styles when loading as data
  } else if (nsCRT::strcmp("external-resource", aCommand) == 0) {
    // Allow CSS, but not scripts
    ScriptLoader()->SetEnabled(false);
  }

  mMayStartLayout = false;
  MOZ_ASSERT(!mReadyForIdle, "We should never hit DOMContentLoaded before this point");

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
    if (loadFlags & nsIRequest::LOAD_DOCUMENT_NEEDS_COOKIE &&
        isDocument &&
        IsSynthesized() &&
        XRE_IsContentProcess()) {
      ContentChild::UpdateCookieStatus(mChannel);
    }
  }

  // If this document is being loaded by a docshell, copy its sandbox flags
  // to the document, and store the fullscreen enabled flag. These are
  // immutable after being set here.
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aContainer);

  // If this is an error page, don't inherit sandbox flags from docshell
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (docShell && !(loadInfo && loadInfo->GetLoadErrorPage())) {
    nsresult rv = docShell->GetSandboxFlags(&mSandboxFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    WarnIfSandboxIneffective(docShell, mSandboxFlags, GetChannel());
  }

  // The CSP directive upgrade-insecure-requests not only applies to the
  // toplevel document, but also to nested documents. Let's propagate that
  // flag from the parent to the nested document.
  nsCOMPtr<nsIDocShellTreeItem> treeItem = this->GetDocShell();
  if (treeItem) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeParent;
    treeItem->GetSameTypeParent(getter_AddRefs(sameTypeParent));
    if (sameTypeParent) {
      nsIDocument* doc = sameTypeParent->GetDocument();
      mBlockAllMixedContent = doc->GetBlockAllMixedContent(false);
      // if the parent document makes use of block-all-mixed-content
      // then subdocument preloads should always be blocked.
      mBlockAllMixedContentPreloads =
        mBlockAllMixedContent || doc->GetBlockAllMixedContent(true);

      mUpgradeInsecureRequests = doc->GetUpgradeInsecureRequests(false);
      // if the parent document makes use of upgrade-insecure-requests
      // then subdocument preloads should always be upgraded.
      mUpgradeInsecurePreloads =
        mUpgradeInsecureRequests || doc->GetUpgradeInsecureRequests(true);
    }
  }

  // If this is not a data document, set CSP.
  if (!mLoadedAsData) {
    nsresult rv = InitCSP(aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // XFO needs to be checked after CSP because it is ignored if
  // the CSP defines frame-ancestors.
  if (!FramingChecker::CheckFrameOptions(aChannel, docShell, NodePrincipal())) {
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
            ("XFO doesn't like frame's ancestry, not loading."));
    // stop!  ERROR page!
    aChannel->Cancel(NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION);
  }

  // Perform a async flash classification based on the doc principal
  // in an early stage to reduce the blocking time.
  mFlashClassification = FlashClassification::Unclassified;
  mPrincipalFlashClassifier->AsyncClassify(GetPrincipal());

  return NS_OK;
}

void
nsIDocument::SendToConsole(nsCOMArray<nsISecurityConsoleMessage>& aMessages)
{
  for (uint32_t i = 0; i < aMessages.Length(); ++i) {
    nsAutoString messageTag;
    aMessages[i]->GetTag(messageTag);

    nsAutoString category;
    aMessages[i]->GetCategory(category);

    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_ConvertUTF16toUTF8(category),
                                    this, nsContentUtils::eSECURITY_PROPERTIES,
                                    NS_ConvertUTF16toUTF8(messageTag).get());
  }
}

void
nsIDocument::ApplySettingsFromCSP(bool aSpeculative)
{
  nsresult rv = NS_OK;
  if (!aSpeculative) {
    // 1) apply settings from regular CSP
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = NodePrincipal()->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS_VOID(rv);
    if (csp) {
      // Set up 'block-all-mixed-content' if not already inherited
      // from the parent context or set by any other CSP.
      if (!mBlockAllMixedContent) {
        rv = csp->GetBlockAllMixedContent(&mBlockAllMixedContent);
        NS_ENSURE_SUCCESS_VOID(rv);
      }
      if (!mBlockAllMixedContentPreloads) {
        mBlockAllMixedContentPreloads = mBlockAllMixedContent;
     }

      // Set up 'upgrade-insecure-requests' if not already inherited
      // from the parent context or set by any other CSP.
      if (!mUpgradeInsecureRequests) {
        rv = csp->GetUpgradeInsecureRequests(&mUpgradeInsecureRequests);
        NS_ENSURE_SUCCESS_VOID(rv);
      }
      if (!mUpgradeInsecurePreloads) {
        mUpgradeInsecurePreloads = mUpgradeInsecureRequests;
      }
    }
    return;
  }

  // 2) apply settings from speculative csp
  nsCOMPtr<nsIContentSecurityPolicy> preloadCsp;
  rv = NodePrincipal()->GetPreloadCsp(getter_AddRefs(preloadCsp));
  NS_ENSURE_SUCCESS_VOID(rv);
  if (preloadCsp) {
    if (!mBlockAllMixedContentPreloads) {
      rv = preloadCsp->GetBlockAllMixedContent(&mBlockAllMixedContentPreloads);
      NS_ENSURE_SUCCESS_VOID(rv);
    }
    if (!mUpgradeInsecurePreloads) {
      rv = preloadCsp->GetUpgradeInsecureRequests(&mUpgradeInsecurePreloads);
      NS_ENSURE_SUCCESS_VOID(rv);
    }
  }
}

nsresult
nsIDocument::InitCSP(nsIChannel* aChannel)
{
  MOZ_ASSERT(!mScriptGlobalObject,
             "CSP must be initialized before mScriptGlobalObject is set!");
  if (!CSPService::sCSPEnabled) {
    MOZ_LOG(gCspPRLog, LogLevel::Debug,
           ("CSP is disabled, skipping CSP init for document %p", this));
    return NS_OK;
  }

  nsAutoCString tCspHeaderValue, tCspROHeaderValue;

  nsCOMPtr<nsIHttpChannel> httpChannel;
  nsresult rv = GetHttpChannelHelper(aChannel, getter_AddRefs(httpChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (httpChannel) {
    Unused << httpChannel->GetResponseHeader(
        NS_LITERAL_CSTRING("content-security-policy"),
        tCspHeaderValue);

    Unused << httpChannel->GetResponseHeader(
        NS_LITERAL_CSTRING("content-security-policy-report-only"),
        tCspROHeaderValue);
  }
  NS_ConvertASCIItoUTF16 cspHeaderValue(tCspHeaderValue);
  NS_ConvertASCIItoUTF16 cspROHeaderValue(tCspROHeaderValue);

  // Check if this is a document from a WebExtension.
  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  auto addonPolicy = BasePrincipal::Cast(principal)->AddonPolicy();

  // Check if this is a signed content to apply default CSP.
  bool applySignedContentCSP = false;
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (loadInfo && loadInfo->GetVerifySignedContent()) {
    applySignedContentCSP = true;
  }

  // If there's no CSP to apply, go ahead and return early
  if (!addonPolicy &&
      !applySignedContentCSP &&
      cspHeaderValue.IsEmpty() &&
      cspROHeaderValue.IsEmpty()) {
    if (MOZ_LOG_TEST(gCspPRLog, LogLevel::Debug)) {
      nsCOMPtr<nsIURI> chanURI;
      aChannel->GetURI(getter_AddRefs(chanURI));
      nsAutoCString aspec;
      chanURI->GetAsciiSpec(aspec);
      MOZ_LOG(gCspPRLog, LogLevel::Debug,
             ("no CSP for document, %s",
              aspec.get()));
    }

    return NS_OK;
  }

  MOZ_LOG(gCspPRLog, LogLevel::Debug, ("Document is an add-on or CSP header specified %p", this));

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = principal->EnsureCSP(static_cast<nsDocument*>(this), getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);

  // ----- if the doc is an addon, apply its CSP.
  if (addonPolicy) {
    nsCOMPtr<nsIAddonPolicyService> aps = do_GetService("@mozilla.org/addons/policy-service;1");

    nsAutoString addonCSP;
    Unused << ExtensionPolicyService::GetSingleton().GetBaseCSP(addonCSP);
    csp->AppendPolicy(addonCSP, false, false);

    csp->AppendPolicy(addonPolicy->ContentSecurityPolicy(), false, false);
  }

  // ----- if the doc is a signed content, apply the default CSP.
  // Note that when the content signing becomes a standard, we might have
  // to restrict this enforcement to "remote content" only.
  if (applySignedContentCSP) {
    nsAutoString signedContentCSP;
    Preferences::GetString("security.signed_content.CSP.default",
                           signedContentCSP);
    csp->AppendPolicy(signedContentCSP, false, false);
  }

  // ----- if there's a full-strength CSP header, apply it.
  if (!cspHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspHeaderValue, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- if there's a report-only CSP header, apply it.
  if (!cspROHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspROHeaderValue, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ----- Enforce sandbox policy if supplied in CSP header
  // The document may already have some sandbox flags set (e.g. if the document
  // is an iframe with the sandbox attribute set). If we have a CSP sandbox
  // directive, intersect the CSP sandbox flags with the existing flags. This
  // corresponds to the _least_ permissive policy.
  uint32_t cspSandboxFlags = SANDBOXED_NONE;
  rv = csp->GetCSPSandboxFlags(&cspSandboxFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Probably the iframe sandbox attribute already caused the creation of a
  // new NullPrincipal. Only create a new NullPrincipal if CSP requires so
  // and no one has been created yet.
  bool needNewNullPrincipal =
    (cspSandboxFlags & SANDBOXED_ORIGIN) && !(mSandboxFlags & SANDBOXED_ORIGIN);

  mSandboxFlags |= cspSandboxFlags;

  if (needNewNullPrincipal) {
    principal = NullPrincipal::CreateWithInheritedAttributes(principal);
    principal->SetCsp(csp);
    SetPrincipal(principal);
  }

  // ----- Enforce frame-ancestor policy on any applied policies
  nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
  if (docShell) {
    bool safeAncestry = false;

    // PermitsAncestry sends violation reports when necessary
    rv = csp->PermitsAncestry(docShell, &safeAncestry);

    if (NS_FAILED(rv) || !safeAncestry) {
      MOZ_LOG(gCspPRLog, LogLevel::Debug,
              ("CSP doesn't like frame's ancestry, not loading."));
      // stop!  ERROR page!
      aChannel->Cancel(NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION);
    }
  }
  ApplySettingsFromCSP(false);
  return NS_OK;
}

void
nsDocument::StopDocumentLoad()
{
  if (mParser) {
    mParserAborted = true;
    mParser->Terminate();
  }
}

void
nsIDocument::SetDocumentURI(nsIURI* aURI)
{
  nsCOMPtr<nsIURI> oldBase = GetDocBaseURI();
  mDocumentURI = aURI;
  nsIURI* newBase = GetDocBaseURI();

  bool equalBases = false;
  // Changing just the ref of a URI does not change how relative URIs would
  // resolve wrt to it, so we can treat the bases as equal as long as they're
  // equal ignoring the ref.
  if (oldBase && newBase) {
    oldBase->EqualsExceptRef(newBase, &equalBases);
  }
  else {
    equalBases = !oldBase && !newBase;
  }

  // If this is the first time we're setting the document's URI, set the
  // document's original URI.
  if (!mOriginalURI)
    mOriginalURI = mDocumentURI;

  // If changing the document's URI changed the base URI of the document, we
  // need to refresh the hrefs of all the links on the page.
  if (!equalBases) {
    RefreshLinkHrefs();
  }
}

static void
GetFormattedTimeString(PRTime aTime, nsAString& aFormattedTimeString)
{
  PRExplodedTime prtime;
  PR_ExplodeTime(aTime, PR_LocalTimeParameters, &prtime);
  // "MM/DD/YYYY hh:mm:ss"
  char formatedTime[24];
  if (SprintfLiteral(formatedTime, "%02d/%02d/%04d %02d:%02d:%02d",
                     prtime.tm_month + 1, prtime.tm_mday, int(prtime.tm_year),
                     prtime.tm_hour     ,  prtime.tm_min,  prtime.tm_sec)) {
    CopyASCIItoUTF16(nsDependentCString(formatedTime), aFormattedTimeString);
  } else {
    // If we for whatever reason failed to find the last modified time
    // (or even the current time), fall back to what NS4.x returned.
    aFormattedTimeString.AssignLiteral(u"01/01/1970 00:00:00");
  }
}

void
nsIDocument::GetLastModified(nsAString& aLastModified) const
{
  if (!mLastModified.IsEmpty()) {
    aLastModified.Assign(mLastModified);
  } else {
    GetFormattedTimeString(PR_Now(), aLastModified);
  }
}

static void
IncrementExpandoGeneration(nsIDocument& aDoc)
{
  ++static_cast<nsDocument&>(aDoc).mExpandoAndGeneration.generation;
}

void
nsIDocument::AddToNameTable(Element* aElement, nsAtom* aName)
{
  MOZ_ASSERT(nsGenericHTMLElement::ShouldExposeNameAsHTMLDocumentProperty(aElement),
             "Only put elements that need to be exposed as document['name'] in "
             "the named table.");

  nsIdentifierMapEntry* entry = mIdentifierMap.PutEntry(aName);

  // Null for out-of-memory
  if (entry) {
    if (!entry->HasNameElement() &&
        !entry->HasIdElementExposedAsHTMLDocumentProperty()) {
      IncrementExpandoGeneration(*this);
    }
    entry->AddNameElement(this, aElement);
  }
}

void
nsIDocument::RemoveFromNameTable(Element* aElement, nsAtom* aName)
{
  // Speed up document teardown
  if (mIdentifierMap.Count() == 0)
    return;

  nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(aName);
  if (!entry) // Could be false if the element was anonymous, hence never added
    return;

  entry->RemoveNameElement(aElement);
  if (!entry->HasNameElement() &&
      !entry->HasIdElementExposedAsHTMLDocumentProperty()) {
    IncrementExpandoGeneration(*this);
  }
}

void
nsIDocument::AddToIdTable(Element* aElement, nsAtom* aId)
{
  nsIdentifierMapEntry* entry = mIdentifierMap.PutEntry(aId);

  if (entry) { /* True except on OOM */
    if (nsGenericHTMLElement::ShouldExposeIdAsHTMLDocumentProperty(aElement) &&
        !entry->HasNameElement() &&
        !entry->HasIdElementExposedAsHTMLDocumentProperty()) {
      IncrementExpandoGeneration(*this);
    }
    entry->AddIdElement(aElement);
  }
}

void
nsIDocument::RemoveFromIdTable(Element* aElement, nsAtom* aId)
{
  NS_ASSERTION(aId, "huhwhatnow?");

  // Speed up document teardown
  if (mIdentifierMap.Count() == 0) {
    return;
  }

  nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(aId);
  if (!entry) // Can be null for XML elements with changing ids.
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

nsIPrincipal*
nsDocument::GetPrincipal()
{
  return NodePrincipal();
}

extern bool sDisablePrefetchHTTPSPref;

void
nsIDocument::SetPrincipal(nsIPrincipal *aNewPrincipal)
{
  if (aNewPrincipal && mAllowDNSPrefetch && sDisablePrefetchHTTPSPref) {
    nsCOMPtr<nsIURI> uri;
    aNewPrincipal->GetURI(getter_AddRefs(uri));
    bool isHTTPS;
    if (!uri || NS_FAILED(uri->SchemeIs("https", &isHTTPS)) ||
        isHTTPS) {
      mAllowDNSPrefetch = false;
    }
  }
  mNodeInfoManager->SetDocumentPrincipal(aNewPrincipal);

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

mozilla::dom::DocGroup*
nsIDocument::GetDocGroup() const
{
#ifdef DEBUG
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
#endif

  return mDocGroup;
}

nsresult
nsIDocument::Dispatch(TaskCategory aCategory,
                      already_AddRefed<nsIRunnable>&& aRunnable)
{
  // Note that this method may be called off the main thread.
  if (mDocGroup) {
    return mDocGroup->Dispatch(aCategory, std::move(aRunnable));
  }
  return DispatcherTrait::Dispatch(aCategory, std::move(aRunnable));
}

nsISerialEventTarget*
nsIDocument::EventTargetFor(TaskCategory aCategory) const
{
  if (mDocGroup) {
    return mDocGroup->EventTargetFor(aCategory);
  }
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread*
nsIDocument::AbstractMainThreadFor(mozilla::TaskCategory aCategory)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDocGroup) {
    return mDocGroup->AbstractMainThreadFor(aCategory);
  }
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
}

void
nsIDocument::NoteScriptTrackingStatus(const nsACString& aURL, bool aIsTracking)
{
  if (aIsTracking) {
    mTrackingScripts.PutEntry(aURL);
  } else {
    MOZ_ASSERT(!mTrackingScripts.Contains(aURL));
  }
}

bool
nsIDocument::IsScriptTracking(const nsACString& aURL) const
{
  return mTrackingScripts.Contains(aURL);
}

NS_IMETHODIMP
nsDocument::GetApplicationCache(nsIApplicationCache **aApplicationCache)
{
  NS_IF_ADDREF(*aApplicationCache = mApplicationCache);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetApplicationCache(nsIApplicationCache *aApplicationCache)
{
  mApplicationCache = aApplicationCache;

  return NS_OK;
}

void
nsIDocument::GetContentType(nsAString& aContentType)
{
  CopyUTF8toUTF16(GetContentTypeInternal(), aContentType);
}

void
nsIDocument::SetContentType(const nsAString& aContentType)
{
  SetContentTypeInternal(NS_ConvertUTF16toUTF8(aContentType));
}

bool
nsIDocument::GetAllowPlugins()
{
  // First, we ask our docshell if it allows plugins.
  nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);

  if (docShell) {
    bool allowPlugins = false;
    docShell->GetAllowPlugins(&allowPlugins);
    if (!allowPlugins) {
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

bool
nsDocument::IsWebAnimationsEnabled(JSContext* aCx, JSObject* /*unused*/)
{
  MOZ_ASSERT(NS_IsMainThread());

  return nsContentUtils::IsSystemCaller(aCx) ||
         nsContentUtils::AnimationsAPICoreEnabled();
}

bool
nsDocument::IsWebAnimationsEnabled(CallerType aCallerType)
{
  MOZ_ASSERT(NS_IsMainThread());

  return aCallerType == dom::CallerType::System ||
         nsContentUtils::AnimationsAPICoreEnabled();
}

bool
nsDocument::AreWebAnimationsImplicitKeyframesEnabled(JSContext* aCx,
                                                     JSObject* /*unused*/
)
{
  MOZ_ASSERT(NS_IsMainThread());

  return nsContentUtils::IsSystemCaller(aCx) ||
         StaticPrefs::dom_animations_api_implicit_keyframes_enabled();
}

bool
nsDocument::AreWebAnimationsTimelinesEnabled(JSContext* aCx,
                                             JSObject* /*unused*/
)
{
  MOZ_ASSERT(NS_IsMainThread());

  return nsContentUtils::IsSystemCaller(aCx) ||
         StaticPrefs::dom_animations_api_timelines_enabled();
}

DocumentTimeline*
nsIDocument::Timeline()
{
  if (!mDocumentTimeline) {
    mDocumentTimeline = new DocumentTimeline(this, TimeDuration(0));
  }

  return mDocumentTimeline;
}

void
nsIDocument::GetAnimations(nsTArray<RefPtr<Animation>>& aAnimations)
{
  // Hold a strong ref for the root element since Element::GetAnimations() calls
  // FlushPendingNotifications() which may destroy the element.
  RefPtr<Element> root = GetRootElement();
  if (!root) {
    return;
  }
  AnimationFilter filter;
  filter.mSubtree = true;
  root->GetAnimations(filter, aAnimations);
}

SVGSVGElement*
nsIDocument::GetSVGRootElement() const
{
  Element* root = GetRootElement();
  if (!root || !root->IsSVGElement(nsGkAtoms::svg)) {
    return nullptr;
  }
  return static_cast<SVGSVGElement*>(root);
}

/* Return true if the document is in the focused top-level window, and is an
 * ancestor of the focused DOMWindow. */
bool
nsIDocument::HasFocus(ErrorResult& rv) const
{
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
  for (nsIDocument* currentDoc = piWindow->GetDoc(); currentDoc;
       currentDoc = currentDoc->GetParentDocument()) {
    if (currentDoc == this) {
      // Yes, we are an ancestor
      return true;
    }
  }

  return false;
}

TimeStamp
nsIDocument::LastFocusTime() const
{
  return mLastFocusTime;
}

void
nsIDocument::SetLastFocusTime(const TimeStamp& aFocusTime)
{
  MOZ_DIAGNOSTIC_ASSERT(!aFocusTime.IsNull());
  MOZ_DIAGNOSTIC_ASSERT(mLastFocusTime.IsNull() ||
                        aFocusTime >= mLastFocusTime);
  mLastFocusTime = aFocusTime;
}

void
nsIDocument::GetReferrer(nsAString& aReferrer) const
{
  if (mIsSrcdocDocument && mParentDocument)
      mParentDocument->GetReferrer(aReferrer);
  else
    CopyUTF8toUTF16(mReferrer, aReferrer);
}

nsresult
nsIDocument::GetSrcdocData(nsAString &aSrcdocData)
{
  if (mIsSrcdocDocument) {
    nsCOMPtr<nsIInputStreamChannel> inStrmChan = do_QueryInterface(mChannel);
    if (inStrmChan) {
      return inStrmChan->GetSrcdocData(aSrcdocData);
    }
  }
  aSrcdocData = VoidString();
  return NS_OK;
}

Element*
nsIDocument::GetActiveElement()
{
  // Get the focused element.
  Element* focusedElement = GetRetargetedFocusedElement();
  if (focusedElement) {
    return focusedElement;
  }

  // No focused element anywhere in this document.  Try to get the BODY.
  if (IsHTMLOrXHTML()) {
    // Because of IE compatibility, return null when html document doesn't have
    // a body.
    return AsHTMLDocument()->GetBody();
  }

  // If we couldn't get a BODY, return the root element.
  return GetDocumentElement();
}

Element*
nsIDocument::GetCurrentScript()
{
  nsCOMPtr<Element> el(do_QueryInterface(ScriptLoader()->GetCurrentScript()));
  return el;
}

nsresult
nsIDocument::NodesFromRectHelper(float aX, float aY,
                                 float aTopSize, float aRightSize,
                                 float aBottomSize, float aLeftSize,
                                 bool aIgnoreRootScrollFrame,
                                 bool aFlushLayout,
                                 nsINodeList** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsSimpleContentList* elements = new nsSimpleContentList(this);
  NS_ADDREF(elements);
  *aReturn = elements;

  // Following the same behavior of elementFromPoint,
  // we don't return anything if either coord is negative
  if (!aIgnoreRootScrollFrame && (aX < 0 || aY < 0))
    return NS_OK;

  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX - aLeftSize);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY - aTopSize);
  nscoord w = nsPresContext::CSSPixelsToAppUnits(aLeftSize + aRightSize) + 1;
  nscoord h = nsPresContext::CSSPixelsToAppUnits(aTopSize + aBottomSize) + 1;

  nsRect rect(x, y, w, h);

  // Make sure the layout information we get is up-to-date, and
  // ensure we get a root frame (for everything but XUL)
  if (aFlushLayout) {
    FlushPendingNotifications(FlushType::Layout);
  }

  nsIPresShell *ps = GetShell();
  NS_ENSURE_STATE(ps);
  nsIFrame *rootFrame = ps->GetRootFrame();

  // XUL docs, unlike HTML, have no frame tree until everything's done loading
  if (!rootFrame)
    return NS_OK; // return nothing to premature XUL callers as a reminder to wait

  AutoTArray<nsIFrame*,8> outFrames;
  nsLayoutUtils::GetFramesForArea(rootFrame, rect, outFrames,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC |
    (aIgnoreRootScrollFrame ? nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME : 0));

  // Used to filter out repeated elements in sequence.
  nsIContent* lastAdded = nullptr;

  for (uint32_t i = 0; i < outFrames.Length(); i++) {
    nsIContent* node = GetContentInThisDocument(outFrames[i]);

    if (node && !node->IsElement() && !node->IsText()) {
      // We have a node that isn't an element or a text node,
      // use its parent content instead.
      node = node->GetParent();
    }
    if (node && node != lastAdded) {
      elements->AppendElement(node);
      lastAdded = node;
    }
  }

  return NS_OK;
}

void
nsIDocument::ReleaseCapture() const
{
  // only release the capture if the caller can access it. This prevents a
  // page from stopping a scrollbar grab for example.
  nsCOMPtr<nsINode> node = nsIPresShell::GetCapturingContent();
  if (node && nsContentUtils::CanCallerAccess(node)) {
    nsIPresShell::SetCapturingContent(nullptr, 0);
  }
}

already_AddRefed<nsIURI>
nsIDocument::GetBaseURI(bool aTryUseXHRDocBaseURI) const
{
  nsCOMPtr<nsIURI> uri;
  if (aTryUseXHRDocBaseURI && mChromeXHRDocBaseURI) {
    uri = mChromeXHRDocBaseURI;
  } else {
    uri = GetDocBaseURI();
  }

  return uri.forget();
}

void
nsIDocument::SetBaseURI(nsIURI* aURI)
{
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

URLExtraData*
nsIDocument::DefaultStyleAttrURLData()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsIURI* baseURI = GetDocBaseURI();
  nsIURI* docURI = GetDocumentURI();
  nsIPrincipal* principal = NodePrincipal();
  if (!mCachedURLData ||
      mCachedURLData->BaseURI() != baseURI ||
      mCachedURLData->GetReferrer() != docURI ||
      mCachedURLData->GetPrincipal() != principal) {
    mCachedURLData = new URLExtraData(baseURI, docURI, principal);
  }
  return mCachedURLData;
}

void
nsIDocument::SetDocumentCharacterSet(NotNull<const Encoding*> aEncoding)
{
  if (mCharacterSet != aEncoding) {
    mCharacterSet = aEncoding;
    mEncodingMenuDisabled = aEncoding == UTF_8_ENCODING;

    if (nsPresContext* context = GetPresContext()) {
      context->DispatchCharSetChange(aEncoding);
    }
  }
}

void
nsIDocument::GetSandboxFlagsAsString(nsAString& aFlags)
{
  nsContentUtils::SandboxFlagsToString(mSandboxFlags, aFlags);
}

void
nsIDocument::GetHeaderData(nsAtom* aHeaderField, nsAString& aData) const
{
  aData.Truncate();
  const nsDocHeaderData* data = mHeaderData;
  while (data) {
    if (data->mField == aHeaderField) {
      aData = data->mData;

      break;
    }
    data = data->mNext;
  }
}

void
nsIDocument::SetHeaderData(nsAtom* aHeaderField, const nsAString& aData)
{
  if (!aHeaderField) {
    NS_ERROR("null headerField");
    return;
  }

  if (!mHeaderData) {
    if (!aData.IsEmpty()) { // don't bother storing empty string
      mHeaderData = new nsDocHeaderData(aHeaderField, aData);
    }
  }
  else {
    nsDocHeaderData* data = mHeaderData;
    nsDocHeaderData** lastPtr = &mHeaderData;
    bool found = false;
    do {  // look for existing and replace
      if (data->mField == aHeaderField) {
        if (!aData.IsEmpty()) {
          data->mData.Assign(aData);
        }
        else {  // don't store empty string
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
      *lastPtr = new nsDocHeaderData(aHeaderField, aData);
    }
  }

  if (aHeaderField == nsGkAtoms::headerContentLanguage) {
    CopyUTF16toUTF8(aData, mContentLanguage);
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
                                           NS_ConvertUTF16toUTF8(aData));
    }
  }

  if (aHeaderField == nsGkAtoms::headerDNSPrefetchControl &&
      mAllowDNSPrefetch) {
    // Chromium treats any value other than 'on' (case insensitive) as 'off'.
    mAllowDNSPrefetch = aData.IsEmpty() || aData.LowerCaseEqualsLiteral("on");
  }

  if (aHeaderField == nsGkAtoms::viewport ||
      aHeaderField == nsGkAtoms::handheldFriendly ||
      aHeaderField == nsGkAtoms::viewport_minimum_scale ||
      aHeaderField == nsGkAtoms::viewport_maximum_scale ||
      aHeaderField == nsGkAtoms::viewport_initial_scale ||
      aHeaderField == nsGkAtoms::viewport_height ||
      aHeaderField == nsGkAtoms::viewport_width ||
      aHeaderField ==  nsGkAtoms::viewport_user_scalable) {
    mViewportType = Unknown;
    mViewportOverflowType = ViewportOverflowType::NoOverflow;
  }

  // Referrer policy spec says to ignore any empty referrer policies.
  if (aHeaderField == nsGkAtoms::referrer && !aData.IsEmpty()) {
     enum mozilla::net::ReferrerPolicy policy = mozilla::net::ReferrerPolicyFromString(aData);
    // If policy is not the empty string, then set element's node document's
    // referrer policy to policy
    if (policy != mozilla::net::RP_Unset) {
      // Referrer policy spec (section 6.1) says that we always use the newest
      // referrer policy we find
      mReferrerPolicy = policy;
      mReferrerPolicySet = true;
    }
  }

  if (aHeaderField == nsGkAtoms::headerReferrerPolicy && !aData.IsEmpty()) {
     enum mozilla::net::ReferrerPolicy policy = nsContentUtils::GetReferrerPolicyFromHeader(aData);
    if (policy != mozilla::net::RP_Unset) {
      mReferrerPolicy = policy;
      mReferrerPolicySet = true;
    }
  }

}
void
nsDocument::TryChannelCharset(nsIChannel *aChannel,
                              int32_t& aCharsetSource,
                              NotNull<const Encoding*>& aEncoding,
                              nsHtml5TreeOpExecutor* aExecutor)
{
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

static inline void
AssertNoStaleServoDataIn(const nsINode& aSubtreeRoot)
{
#ifdef DEBUG
  for (const nsINode* node = &aSubtreeRoot;
       node;
       node = node->GetNextNode(&aSubtreeRoot)) {
    const Element* element = Element::FromNode(node);
    if (!element) {
      continue;
    }
    MOZ_ASSERT(!element->HasServoData());
    if (auto* shadow = element->GetShadowRoot()) {
      AssertNoStaleServoDataIn(*shadow);
    }
    if (nsXBLBinding* binding = element->GetXBLBinding()) {
      if (nsXBLBinding* bindingWithContent = binding->GetBindingWithContent()) {
        nsIContent* content = bindingWithContent->GetAnonymousContent();
        MOZ_ASSERT(!content->AsElement()->HasServoData());
        for (nsINode* child = content->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
          AssertNoStaleServoDataIn(*child);
        }
      }
    }
  }
#endif
}

already_AddRefed<nsIPresShell>
nsIDocument::CreateShell(nsPresContext* aContext,
                         nsViewManager* aViewManager,
                         UniquePtr<ServoStyleSet> aStyleSet)
{
  NS_ASSERTION(!mPresShell, "We have a presshell already!");

  NS_ENSURE_FALSE(GetBFCacheEntry(), nullptr);

  FillStyleSet(aStyleSet.get());
  AssertNoStaleServoDataIn(static_cast<nsINode&>(*this));

  RefPtr<PresShell> shell = new PresShell;
  // Note: we don't hold a ref to the shell (it holds a ref to us)
  mPresShell = shell;
  shell->Init(this, aContext, aViewManager, std::move(aStyleSet));

  // Make sure to never paint if we belong to an invisible DocShell.
  nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
  if (docShell && docShell->IsInvisible())
    shell->SetNeverPainting(true);

  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug, ("DOCUMENT %p with PressShell %p and DocShell %p",
                                                this, shell.get(), docShell.get()));

  mExternalResourceMap.ShowViewers();

  UpdateFrameRequestCallbackSchedulingState();

  // Now that we have a shell, we might have @font-face rules (the presence of a
  // shell may change which rules apply to us). We don't need to do anything
  // like EnsureStyleFlush or such, there's nothing to update yet and when stuff
  // is ready to update we'll flush the font set.
  MarkUserFontSetDirty();

  return shell.forget();
}

void
nsIDocument::UpdateFrameRequestCallbackSchedulingState(nsIPresShell* aOldShell)
{
  // If the condition for shouldBeScheduled changes to depend on some other
  // variable, add UpdateFrameRequestCallbackSchedulingState() calls to the
  // places where that variable can change.
  bool shouldBeScheduled =
    mPresShell && IsEventHandlingEnabled() && !mFrameRequestCallbacks.IsEmpty();
  if (shouldBeScheduled == mFrameRequestCallbacksScheduled) {
    // nothing to do
    return;
  }

  nsIPresShell* presShell = aOldShell ? aOldShell : mPresShell;
  MOZ_RELEASE_ASSERT(presShell);

  nsRefreshDriver* rd = presShell->GetPresContext()->RefreshDriver();
  if (shouldBeScheduled) {
    rd->ScheduleFrameRequestCallbacks(this);
  } else {
    rd->RevokeFrameRequestCallbacks(this);
  }

  mFrameRequestCallbacksScheduled = shouldBeScheduled;
}

void
nsIDocument::TakeFrameRequestCallbacks(FrameRequestCallbackList& aCallbacks)
{
  aCallbacks.AppendElements(mFrameRequestCallbacks);
  mFrameRequestCallbacks.Clear();
  // No need to manually remove ourselves from the refresh driver; it will
  // handle that part.  But we do have to update our state.
  mFrameRequestCallbacksScheduled = false;
}

bool
nsIDocument::ShouldThrottleFrameRequests()
{
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

void
nsIDocument::DeleteShell()
{
  mExternalResourceMap.HideViewers();
  if (nsPresContext* presContext = mPresShell->GetPresContext()) {
    presContext->RefreshDriver()->CancelPendingEvents(this);
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

  nsIPresShell* oldShell = mPresShell;
  mPresShell = nullptr;
  UpdateFrameRequestCallbackSchedulingState(oldShell);
  mStyleSetFilled = false;

  ClearStaleServoData();
  AssertNoStaleServoDataIn(static_cast<nsINode&>(*this));
}

void
nsIDocument::SetBFCacheEntry(nsIBFCacheEntry* aEntry)
{
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

static void
SubDocClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  SubDocMapEntry *e = static_cast<SubDocMapEntry *>(entry);

  NS_RELEASE(e->mKey);
  if (e->mSubDocument) {
    e->mSubDocument->SetParentDocument(nullptr);
    NS_RELEASE(e->mSubDocument);
  }
}

static void
SubDocInitEntry(PLDHashEntryHdr *entry, const void *key)
{
  SubDocMapEntry *e =
    const_cast<SubDocMapEntry *>
              (static_cast<const SubDocMapEntry *>(entry));

  e->mKey = const_cast<Element*>(static_cast<const Element*>(key));
  NS_ADDREF(e->mKey);

  e->mSubDocument = nullptr;
}

nsresult
nsIDocument::SetSubDocumentFor(Element* aElement, nsIDocument* aSubDoc)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_UNEXPECTED);

  if (!aSubDoc) {
    // aSubDoc is nullptr, remove the mapping

    if (mSubDocuments) {
      nsIDocument* subDoc = GetSubDocumentFor(aElement);
      if (subDoc) {
        subDoc->SetAllowPaymentRequest(false);
      }
      mSubDocuments->Remove(aElement);
    }
  } else {
    if (!mSubDocuments) {
      // Create a new hashtable

      static const PLDHashTableOps hash_table_ops =
      {
        PLDHashTable::HashVoidPtrKeyStub,
        PLDHashTable::MatchEntryStub,
        PLDHashTable::MoveEntryStub,
        SubDocClearEntry,
        SubDocInitEntry
      };

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

nsIDocument*
nsIDocument::GetSubDocumentFor(nsIContent *aContent) const
{
  if (mSubDocuments && aContent->IsElement()) {
    auto entry = static_cast<SubDocMapEntry*>
                            (mSubDocuments->Search(aContent->AsElement()));

    if (entry) {
      return entry->mSubDocument;
    }
  }

  return nullptr;
}

Element*
nsIDocument::FindContentForSubDocument(nsIDocument *aDocument) const
{
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

bool
nsIDocument::IsNodeOfType(uint32_t aFlags) const
{
  return false;
}

Element*
nsIDocument::GetRootElement() const
{
  return (mCachedRootElement && mCachedRootElement->GetParentNode() == this) ?
         mCachedRootElement : GetRootElementInternal();
}

nsIContent*
nsIDocument::GetUnfocusedKeyEventTarget()
{
  return GetRootElement();
}

Element*
nsIDocument::GetRootElementInternal() const
{
  // We invoke GetRootElement() immediately before the servo traversal, so we
  // should always have a cache hit from Servo.
  MOZ_ASSERT(NS_IsMainThread());

  // Loop backwards because any non-elements, such as doctypes and PIs
  // are likely to appear before the root element.
  for (nsIContent* child = GetLastChild(); child;
       child = child->GetPreviousSibling()) {
    if (Element* element = Element::FromNode(child)) {
      const_cast<nsIDocument*>(this)->mCachedRootElement = element;
      return element;
    }
  }

  const_cast<nsIDocument*>(this)->mCachedRootElement = nullptr;
  return nullptr;
}

nsresult
nsIDocument::InsertChildBefore(nsIContent* aKid,
                               nsIContent* aBeforeThis,
                               bool aNotify)
{
  if (aKid->IsElement() && GetRootElement()) {
    NS_WARNING("Inserting root element when we already have one");
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  int32_t index = aBeforeThis ? ComputeIndexOf(aBeforeThis) : GetChildCount();
  MOZ_ASSERT(index >= 0);

  return doInsertChildAt(aKid, index, aNotify, mChildren);
}

void
nsIDocument::RemoveChildNode(nsIContent* aKid, bool aNotify)
{
  if (aKid->IsElement()) {
    // Destroy the link map up front before we mess with the child list.
    DestroyElementMaps();
  }

  // Preemptively clear mCachedRootElement, since we may be about to remove it
  // from our child list, and we don't want to return this maybe-obsolete value
  // from any GetRootElement() calls that happen inside of doRemoveChildAt().
  // (NOTE: for this to be useful, doRemoveChildAt() must NOT trigger any
  // GetRootElement() calls until after it's removed the child from mChildren.
  // Any call before that point would restore this soon-to-be-obsolete cached
  // answer, and our clearing here would be fruitless.)
  mCachedRootElement = nullptr;
  doRemoveChildAt(ComputeIndexOf(aKid), aNotify, aKid, mChildren);
  MOZ_ASSERT(mCachedRootElement != aKid,
             "Stale pointer in mCachedRootElement, after we tried to clear it "
             "(maybe somebody called GetRootElement() too early?)");
}

void
nsIDocument::AddStyleSheetToStyleSets(StyleSheet* aSheet)
{
  if (nsIPresShell* shell = GetShell()) {
    shell->StyleSet()->AddDocStyleSheet(aSheet, this);
    shell->ApplicableStylesChanged();
  }
}

#define DO_STYLESHEET_NOTIFICATION(className, type, memberName, argName)      \
  do {                                                                        \
    className##Init init;                                                     \
    init.mBubbles = true;                                                     \
    init.mCancelable = true;                                                  \
    init.mStylesheet = aSheet;                                                \
    init.memberName = argName;                                                \
                                                                              \
    RefPtr<className> event =                                                 \
      className::Constructor(this, NS_LITERAL_STRING(type), init);            \
    event->SetTrusted(true);                                                  \
    event->SetTarget(this);                                                   \
    RefPtr<AsyncEventDispatcher> asyncDispatcher =                            \
      new AsyncEventDispatcher(this, event);                                  \
    asyncDispatcher->mOnlyChromeDispatch = ChromeOnlyDispatch::eYes;           \
    asyncDispatcher->PostDOMEvent();                                          \
  } while (0);

void
nsIDocument::NotifyStyleSheetAdded(StyleSheet* aSheet, bool aDocumentSheet)
{
  if (StyleSheetChangeEventsEnabled()) {
    DO_STYLESHEET_NOTIFICATION(StyleSheetChangeEvent,
                               "StyleSheetAdded",
                               mDocumentSheet,
                               aDocumentSheet);
  }
}

void
nsIDocument::NotifyStyleSheetRemoved(StyleSheet* aSheet, bool aDocumentSheet)
{
  if (StyleSheetChangeEventsEnabled()) {
    DO_STYLESHEET_NOTIFICATION(StyleSheetChangeEvent,
                               "StyleSheetRemoved",
                               mDocumentSheet,
                               aDocumentSheet);
  }
}

void
nsIDocument::RemoveStyleSheetFromStyleSets(StyleSheet* aSheet)
{
  if (nsIPresShell* shell = GetShell()) {
    shell->StyleSet()->RemoveDocStyleSheet(aSheet);
    shell->ApplicableStylesChanged();
  }
}

void
nsIDocument::RemoveStyleSheet(StyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  RefPtr<StyleSheet> sheet = DocumentOrShadowRoot::RemoveSheet(*aSheet);

  if (!sheet) {
    NS_ASSERTION(mInUnlinkOrDeletion, "stylesheet not found");
    return;
  }

  if (!mIsGoingAway) {
    if (sheet->IsApplicable()) {
      RemoveStyleSheetFromStyleSets(sheet);
    }

    NotifyStyleSheetRemoved(sheet, true);
  }

  sheet->ClearAssociatedDocumentOrShadowRoot();
}

void
nsIDocument::UpdateStyleSheets(nsTArray<RefPtr<StyleSheet>>& aOldSheets,
                               nsTArray<RefPtr<StyleSheet>>& aNewSheets)
{
  // XXX Need to set the sheet on the ownernode, if any
  MOZ_ASSERT(aOldSheets.Length() == aNewSheets.Length(),
             "The lists must be the same length!");
  int32_t count = aOldSheets.Length();

  RefPtr<StyleSheet> oldSheet;
  int32_t i;
  for (i = 0; i < count; ++i) {
    oldSheet = aOldSheets[i];

    // First remove the old sheet.
    NS_ASSERTION(oldSheet, "None of the old sheets should be null");
    int32_t oldIndex = mStyleSheets.IndexOf(oldSheet);
    RemoveStyleSheet(oldSheet);  // This does the right notifications

    // Now put the new one in its place.  If it's null, just ignore it.
    StyleSheet* newSheet = aNewSheets[i];
    if (newSheet) {
      DocumentOrShadowRoot::InsertSheetAt(oldIndex, *newSheet);
      if (newSheet->IsApplicable()) {
        AddStyleSheetToStyleSets(newSheet);
      }

      NotifyStyleSheetAdded(newSheet, true);
    }
  }
}

void
nsIDocument::InsertSheetAt(size_t aIndex, StyleSheet& aSheet)
{
  DocumentOrShadowRoot::InsertSheetAt(aIndex, aSheet);

  if (aSheet.IsApplicable()) {
    AddStyleSheetToStyleSets(&aSheet);
  }

  NotifyStyleSheetAdded(&aSheet, true);
}


void
nsIDocument::SetStyleSheetApplicableState(StyleSheet* aSheet, bool aApplicable)
{
  MOZ_ASSERT(aSheet, "null arg");

  // If we're actually in the document style sheet list
  if (mStyleSheets.IndexOf(aSheet) != mStyleSheets.NoIndex) {
    if (aApplicable) {
      AddStyleSheetToStyleSets(aSheet);
    } else {
      RemoveStyleSheetFromStyleSets(aSheet);
    }
  }

  if (StyleSheetChangeEventsEnabled()) {
    DO_STYLESHEET_NOTIFICATION(StyleSheetApplicableStateChangeEvent,
                               "StyleSheetApplicableStateChanged",
                               mApplicable,
                               aApplicable);
  }

  if (!mSSApplicableStateNotificationPending) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIRunnable> notification =
      NewRunnableMethod("nsIDocument::NotifyStyleSheetApplicableStateChanged",
                        this,
                        &nsIDocument::NotifyStyleSheetApplicableStateChanged);
    mSSApplicableStateNotificationPending =
      NS_SUCCEEDED(
        Dispatch(TaskCategory::Other, notification.forget()));
  }
}

void
nsIDocument::NotifyStyleSheetApplicableStateChanged()
{
  mSSApplicableStateNotificationPending = false;
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(this,
                                     "style-sheet-applicable-state-changed",
                                     nullptr);
  }
}

static SheetType
ConvertAdditionalSheetType(nsIDocument::additionalSheetType aType)
{
  switch(aType) {
    case nsIDocument::eAgentSheet:
      return SheetType::Agent;
    case nsIDocument::eUserSheet:
      return SheetType::User;
    case nsIDocument::eAuthorSheet:
      return SheetType::Doc;
    default:
      MOZ_ASSERT(false, "wrong type");
      // we must return something although this should never happen
      return SheetType::Count;
  }
}

static int32_t
FindSheet(const nsTArray<RefPtr<StyleSheet>>& aSheets, nsIURI* aSheetURI)
{
  for (int32_t i = aSheets.Length() - 1; i >= 0; i-- ) {
    bool bEqual;
    nsIURI* uri = aSheets[i]->GetSheetURI();

    if (uri && NS_SUCCEEDED(uri->Equals(aSheetURI, &bEqual)) && bEqual)
      return i;
  }

  return -1;
}

nsresult
nsIDocument::LoadAdditionalStyleSheet(additionalSheetType aType,
                                      nsIURI* aSheetURI)
{
  MOZ_ASSERT(aSheetURI, "null arg");

  // Checking if we have loaded this one already.
  if (FindSheet(mAdditionalSheets[aType], aSheetURI) >= 0)
    return NS_ERROR_INVALID_ARG;

  // Loading the sheet sync.
  RefPtr<css::Loader> loader = new css::Loader(GetDocGroup());

  css::SheetParsingMode parsingMode;
  switch (aType) {
    case nsIDocument::eAgentSheet:
      parsingMode = css::eAgentSheetFeatures;
      break;

    case nsIDocument::eUserSheet:
      parsingMode = css::eUserSheetFeatures;
      break;

    case nsIDocument::eAuthorSheet:
      parsingMode = css::eAuthorSheetFeatures;
      break;

    default:
      MOZ_CRASH("impossible value for aType");
  }

  RefPtr<StyleSheet> sheet;
  nsresult rv = loader->LoadSheetSync(aSheetURI, parsingMode, true, &sheet);
  NS_ENSURE_SUCCESS(rv, rv);

  sheet->SetAssociatedDocumentOrShadowRoot(
     this, StyleSheet::OwnedByDocumentOrShadowRoot);
  MOZ_ASSERT(sheet->IsApplicable());

  return AddAdditionalStyleSheet(aType, sheet);
}

nsresult
nsIDocument::AddAdditionalStyleSheet(additionalSheetType aType, StyleSheet* aSheet)
{
  if (mAdditionalSheets[aType].Contains(aSheet))
    return NS_ERROR_INVALID_ARG;

  if (!aSheet->IsApplicable())
    return NS_ERROR_INVALID_ARG;

  mAdditionalSheets[aType].AppendElement(aSheet);

  if (nsIPresShell* shell = GetShell()) {
    SheetType type = ConvertAdditionalSheetType(aType);
    shell->StyleSet()->AppendStyleSheet(type, aSheet);
    shell->ApplicableStylesChanged();
  }

  // Passing false, so documet.styleSheets.length will not be affected by
  // these additional sheets.
  NotifyStyleSheetAdded(aSheet, false);
  return NS_OK;
}

void
nsIDocument::RemoveAdditionalStyleSheet(additionalSheetType aType, nsIURI* aSheetURI)
{
  MOZ_ASSERT(aSheetURI);

  nsTArray<RefPtr<StyleSheet>>& sheets = mAdditionalSheets[aType];

  int32_t i = FindSheet(mAdditionalSheets[aType], aSheetURI);
  if (i >= 0) {
    RefPtr<StyleSheet> sheetRef = sheets[i];
    sheets.RemoveElementAt(i);

    if (!mIsGoingAway) {
      MOZ_ASSERT(sheetRef->IsApplicable());
      if (nsIPresShell* shell = GetShell()) {
        SheetType type = ConvertAdditionalSheetType(aType);
        shell->StyleSet()->RemoveStyleSheet(type, sheetRef);
        shell->ApplicableStylesChanged();
      }
    }

    // Passing false, so documet.styleSheets.length will not be affected by
    // these additional sheets.
    NotifyStyleSheetRemoved(sheetRef, false);
    sheetRef->ClearAssociatedDocumentOrShadowRoot();
  }
}

nsIGlobalObject*
nsIDocument::GetScopeObject() const
{
  nsCOMPtr<nsIGlobalObject> scope(do_QueryReferent(mScopeObject));
  return scope;
}

void
nsIDocument::SetScopeObject(nsIGlobalObject* aGlobal)
{
  mScopeObject = do_GetWeakReference(aGlobal);
  if (aGlobal) {
    mHasHadScriptHandlingObject = true;

    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (window) {
      // We want to get the tabgroup unconditionally, such that we can make
      // certain that it is cached in the inner window early enough.
      mozilla::dom::TabGroup* tabgroup = window->TabGroup();
      // We should already have the principal, and now that we have been added to a
      // window, we should be able to join a DocGroup!
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
    }
  }
}

static void
CheckIfContainsEMEContent(nsISupports* aSupports, void* aContainsEME)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (auto mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
    bool* contains = static_cast<bool*>(aContainsEME);
    if (mediaElem->GetMediaKeys()) {
      *contains = true;
    }
  }
}

bool
nsIDocument::ContainsEMEContent()
{
  bool containsEME = false;
  EnumerateActivityObservers(CheckIfContainsEMEContent,
                             static_cast<void*>(&containsEME));
  return containsEME;
}

static void
CheckIfContainsMSEContent(nsISupports* aSupports, void* aContainsMSE)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (auto mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
    bool* contains = static_cast<bool*>(aContainsMSE);
    RefPtr<MediaSource> ms = mediaElem->GetMozMediaSourceObject();
    if (ms) {
      *contains = true;
    }
  }
}

bool
nsIDocument::ContainsMSEContent()
{
  bool containsMSE = false;
  EnumerateActivityObservers(CheckIfContainsMSEContent,
                             static_cast<void*>(&containsMSE));
  return containsMSE;
}

static void
NotifyActivityChanged(nsISupports *aSupports, void *aUnused)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (auto mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
    mediaElem->NotifyOwnerDocumentActivityChanged();
  }
  nsCOMPtr<nsIObjectLoadingContent> objectLoadingContent(do_QueryInterface(aSupports));
  if (objectLoadingContent) {
    nsObjectLoadingContent* olc = static_cast<nsObjectLoadingContent*>(objectLoadingContent.get());
    olc->NotifyOwnerDocumentActivityChanged();
  }
  nsCOMPtr<nsIDocumentActivity> objectDocumentActivity(do_QueryInterface(aSupports));
  if (objectDocumentActivity) {
    objectDocumentActivity->NotifyOwnerDocumentActivityChanged();
  }
}

bool
nsIDocument::IsTopLevelWindowInactive() const
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem = GetDocShell();
  if (!treeItem) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  if (!rootItem) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> domWindow = rootItem->GetWindow();
  return domWindow && !domWindow->IsActive();
}

void
nsIDocument::SetContainer(nsDocShell* aContainer)
{
  if (aContainer) {
    mDocumentContainer = aContainer;
  } else {
    mDocumentContainer = WeakPtr<nsDocShell>();
  }

  EnumerateActivityObservers(NotifyActivityChanged, nullptr);

  // IsTopLevelWindowInactive depends on the docshell, so
  // update the cached value now that it's available.
  UpdateDocumentStates(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
  if (!aContainer) {
    return;
  }

  // Get the Docshell
  if (aContainer->ItemType() == nsIDocShellTreeItem::typeContent) {
    // check if same type root
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    aContainer->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    NS_ASSERTION(sameTypeRoot, "No document shell root tree item from document shell tree item!");

    if (sameTypeRoot == aContainer) {
      static_cast<nsDocument*>(this)->SetIsTopLevelContentDocument(true);
    }

    static_cast<nsDocument*>(this)->SetIsContentDocument(true);
  }

  mAncestorPrincipals = aContainer->AncestorPrincipals();
  mAncestorOuterWindowIDs = aContainer->AncestorOuterWindowIDs();
}

nsISupports*
nsIDocument::GetContainer() const
{
  return static_cast<nsIDocShell*>(mDocumentContainer);
}

void
nsIDocument::SetScriptGlobalObject(nsIScriptGlobalObject *aScriptGlobalObject)
{
  MOZ_ASSERT(aScriptGlobalObject || !mAnimationController ||
             mAnimationController->IsPausedByType(
               nsSMILTimeContainer::PAUSE_PAGEHIDE |
               nsSMILTimeContainer::PAUSE_BEGIN),
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
#ifdef DEBUG
    if (!mWillReparent) {
      // We really shouldn't have a wrapper here but if we do we need to make sure
      // it has the correct parent.
      JSObject *obj = GetWrapperPreserveColor();
      if (obj) {
        JSObject *newScope = aScriptGlobalObject->GetGlobalJSObject();
        NS_ASSERTION(JS::GetNonCCWObjectGlobal(obj) == newScope,
                     "Wrong scope, this is really bad!");
      }
    }
#endif

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
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  NodePrincipal()->GetCsp(getter_AddRefs(csp));
  if (csp) {
    static_cast<nsCSPContext*>(csp.get())->flushConsoleMessages();
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
  // creation are already done before nsDocument::SetScriptGlobalObject() call.
  // MediaDecoder decides whether starting decoding is decided based on
  // document's visibility. When the MediaDecoder is created,
  // nsDocument::SetScriptGlobalObject() is not yet called and document is
  // hidden state. Therefore the MediaDecoder decides that decoding is
  // not yet necessary. But soon after nsDocument::SetScriptGlobalObject()
  // call, the document becomes not hidden. At the time, MediaDecoder needs
  // to know it and needs to start updating decoding.
  if (oldState != mVisibilityState) {
    EnumerateActivityObservers(NotifyActivityChanged, nullptr);
  }

  // The global in the template contents owner document should be the same.
  if (mTemplateContentsOwner && mTemplateContentsOwner != this) {
    mTemplateContentsOwner->SetScriptGlobalObject(aScriptGlobalObject);
  }

  if (!mMaybeServiceWorkerControlled && mDocumentContainer && mScriptGlobalObject && GetChannel()) {

    // If we are shift-reloaded, don't associate with a ServiceWorker.
    if (mDocumentContainer->IsForceReloading()) {
      NS_WARNING("Page was shift reloaded, skipping ServiceWorker control");
      return;
    }

    mMaybeServiceWorkerControlled = true;
  }
}

nsIScriptGlobalObject*
nsIDocument::GetScriptHandlingObjectInternal() const
{
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
void
nsIDocument::SetScriptHandlingObject(nsIScriptGlobalObject* aScriptObject)
{
  NS_ASSERTION(!mScriptGlobalObject ||
               mScriptGlobalObject == aScriptObject,
               "Wrong script object!");
  if (aScriptObject) {
    SetScopeObject(aScriptObject);
    mHasHadDefaultView = false;
  }
}

nsPIDOMWindowOuter*
nsIDocument::GetWindowInternal() const
{
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
    if (nsCOMPtr<nsPIDOMWindowInner> inner = do_QueryInterface(mScriptGlobalObject)) {
      // mScriptGlobalObject is always the inner window, let's get the outer.
      win = inner->GetOuterWindow();
    }
  }

  return win;
}

bool
nsIDocument::InternalAllowXULXBL()
{
  if (nsContentUtils::AllowXULXBLForPrincipal(NodePrincipal())) {
    mAllowXULXBL = eTriTrue;
    return true;
  }

  mAllowXULXBL = eTriFalse;
  return false;
}

// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void
nsIDocument::AddObserver(nsIDocumentObserver* aObserver)
{
  NS_ASSERTION(mObservers.IndexOf(aObserver) == nsTArray<int>::NoIndex,
               "Observer already in the list");
  mObservers.AppendElement(aObserver);
  AddMutationObserver(aObserver);
}

bool
nsIDocument::RemoveObserver(nsIDocumentObserver* aObserver)
{
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

void
nsIDocument::MaybeEndOutermostXBLUpdate()
{
  // Only call BindingManager()->EndOutermostUpdate() when
  // we're not in an update and it is safe to run scripts.
  if (mUpdateNestLevel == 0 && mInXBLUpdate) {
    if (nsContentUtils::IsSafeToRunScript()) {
      mInXBLUpdate = false;
      BindingManager()->EndOutermostUpdate();
    } else if (!mInDestructor) {
      if (!mMaybeEndOutermostXBLUpdateRunner) {
        mMaybeEndOutermostXBLUpdateRunner =
          NewRunnableMethod("nsDocument::MaybeEndOutermostXBLUpdate",
                            this,
                            &nsDocument::MaybeEndOutermostXBLUpdate);
      }
      nsContentUtils::AddScriptRunner(mMaybeEndOutermostXBLUpdateRunner);
    }
  }
}

void
nsIDocument::BeginUpdate()
{
  // If the document is going away, then it's probably okay to do things to it
  // in the wrong DocGroup. We're unlikely to run JS or do anything else
  // observable at this point. We reach this point when cycle collecting a
  // <link> element and the unlink code removes a style sheet.
  //
  // TODO(emilio): Style updates are gone, can this happen now?
  if (mDocGroup && !mIsGoingAway && !mInUnlinkOrDeletion && !mIgnoreDocGroupMismatches) {
    mDocGroup->ValidateAccess();
  }

  if (mUpdateNestLevel == 0 && !mInXBLUpdate) {
    mInXBLUpdate = true;
    BindingManager()->BeginOutermostUpdate();
  }

  ++mUpdateNestLevel;
  nsContentUtils::AddScriptBlocker();
  NS_DOCUMENT_NOTIFY_OBSERVERS(BeginUpdate, (this));
}

void
nsDocument::EndUpdate()
{
  NS_DOCUMENT_NOTIFY_OBSERVERS(EndUpdate, (this));

  nsContentUtils::RemoveScriptBlocker();

  --mUpdateNestLevel;

  // This set of updates may have created XBL bindings.  Let the
  // binding manager know we're done.
  MaybeEndOutermostXBLUpdate();

  MaybeInitializeFinalizeFrameLoaders();
}

void
nsDocument::BeginLoad()
{
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

void
nsIDocument::MozSetImageElement(const nsAString& aImageElementId,
                               Element* aElement)
{
  if (aImageElementId.IsEmpty())
    return;

  // Hold a script blocker while calling SetImageElement since that can call
  // out to id-observers
  nsAutoScriptBlocker scriptBlocker;

  nsIdentifierMapEntry* entry = mIdentifierMap.PutEntry(aImageElementId);
  if (entry) {
    entry->SetImageElement(aElement);
    if (entry->IsEmpty()) {
      mIdentifierMap.RemoveEntry(entry);
    }
  }
}

Element*
nsIDocument::LookupImageElement(const nsAString& aId)
{
  if (aId.IsEmpty())
    return nullptr;

  nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(aId);
  return entry ? entry->GetImageIdElement() : nullptr;
}

void
nsIDocument::DispatchContentLoadedEvents()
{
  // If you add early returns from this method, make sure you're
  // calling UnblockOnload properly.

  // Unpin references to preloaded images
  mPreloadingImages.Clear();

  // DOM manipulation after content loaded should not care if the element
  // came from the preloader.
  mPreloadedPreconnects.Clear();

  if (mTiming) {
    mTiming->NotifyDOMContentLoadedStart(nsIDocument::GetDocumentURI());
  }

  // Dispatch observer notification to notify observers document is interactive.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    nsIPrincipal* principal = NodePrincipal();
    os->NotifyObservers(this,
                        nsContentUtils::IsSystemPrincipal(principal) ?
                          "chrome-document-interactive" :
                          "content-document-interactive",
                        nullptr);
  }

  // Fire a DOM event notifying listeners that this document has been
  // loaded (excluding images and other loads initiated by this
  // document).
  nsContentUtils::DispatchTrustedEvent(this, this,
                                       NS_LITERAL_STRING("DOMContentLoaded"),
                                       CanBubble::eYes, Cancelable::eNo);

  if (MayStartLayout()) {
    MaybeResolveReadyForIdle();
  }

  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  nsIDocShell* docShell = this->GetDocShell();

  if (timelines && timelines->HasConsumer(docShell)) {
    timelines->AddMarkerForDocShell(docShell,
      MakeUnique<DocLoadingTimelineMarker>("document::DOMContentLoaded"));
  }

  if (mTiming) {
    mTiming->NotifyDOMContentLoadedEnd(nsIDocument::GetDocumentURI());
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
    nsCOMPtr<nsIDocument> parent = mParentDocument;
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

          RefPtr<nsPresContext> context = parent->GetPresContext();

          if (context) {
            EventDispatcher::Dispatch(parent, context, innerEvent, event,
                                      &status);
          }
        }
      }

      parent = parent->GetParentDocument();
    } while (parent);
  }

  // If the document has a manifest attribute, fire a MozApplicationManifest
  // event.
  Element* root = GetRootElement();
  if (root && root->HasAttr(kNameSpaceID_None, nsGkAtoms::manifest)) {
    nsContentUtils::DispatchChromeEvent(this, this,
                                        NS_LITERAL_STRING("MozApplicationManifest"),
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

#if defined(DEBUG) && !defined(ANDROID)
// We want to get to a point where all about: pages ship with a CSP. This
// assertion ensures that we can not deploy new about: pages without a CSP.
// Initially we will whitelist legacy about: pages which not yet have a CSP
// attached, but ultimately that whitelist should disappear.
// Please note that any about: page should not use inline JS or inline CSS,
// and instead should load JS and CSS from an external file (*.js, *.css)
// which allows us to apply a strong CSP omitting 'unsafe-inline'. Ideally,
// the CSP allows precisely the resources that need to be loaded; but it
// should at least be as strong as:
// <meta http-equiv="Content-Security-Policy" content="default-src chrome:"/>
static void
AssertContentPrivilegedAboutPageHasCSP(nsIURI* aDocumentURI, nsIPrincipal* aPrincipal)
{
  // Curently we can't serialize the CSP, hence we only assert if
  // running in the content process.
  if (!XRE_IsContentProcess()) {
    return;
  }

  // Check if we are loading an about: URI at all
  bool isAboutURI =
    (NS_SUCCEEDED(aDocumentURI->SchemeIs("about", &isAboutURI)) && isAboutURI);

  if (!isAboutURI) {
    return;
  }

  // Check if we are loading a content-privileged about: URI
  nsCOMPtr<nsIAboutModule> aboutModule;
  nsresult rv = NS_GetAboutModule(aDocumentURI, getter_AddRefs(aboutModule));
  NS_ENSURE_SUCCESS_VOID(rv);

  uint32_t aboutModuleFlags = 0;
  rv = aboutModule->GetURIFlags(aDocumentURI, &aboutModuleFlags);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (!(aboutModuleFlags & nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT)) {
    return;
  }

  // Potentially init the legacy whitelist of about URIs without a CSP.
  static StaticAutoPtr<nsTArray<nsCString>> sLegacyAboutPagesWithNoCSP;
  if (!sLegacyAboutPagesWithNoCSP) {
    sLegacyAboutPagesWithNoCSP = new nsTArray<nsCString>();
    nsAutoCString legacyAboutPages;
    Preferences::GetCString("csp.content_privileged_about_uris_without_csp",
      legacyAboutPages);
    for (const nsACString& hostString : legacyAboutPages.Split(',')) {
      // please note that for the actual whitelist we only store the path of
      // about: URI. Let's reassemble the full about URI here so we don't
      // have to remove query arguments later.
      nsCString aboutURI;
      aboutURI.AppendLiteral("about:");
      aboutURI.Append(hostString);
      sLegacyAboutPagesWithNoCSP->AppendElement(aboutURI);
    }
    ClearOnShutdown(&sLegacyAboutPagesWithNoCSP);
  }

  // Check if the about URI is whitelisted
  nsAutoCString aboutSpec;
  aDocumentURI->GetSpec(aboutSpec);
  for (auto& legacyPageEntry : *sLegacyAboutPagesWithNoCSP) {
    // please note that we perform a substring match here on purpose,
    // so we don't have to deal and parse out all the query arguments
    // the various about pages rely on.
    if (aboutSpec.Find(legacyPageEntry) == 0) {
      return;
    }
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  aPrincipal->GetCsp(getter_AddRefs(csp));
  nsAutoString parsedPolicyStr;
  if (csp) {
    uint32_t policyCount = 0;
     csp->GetPolicyCount(&policyCount);
     if (policyCount > 0) {
       csp->GetPolicyString(0, parsedPolicyStr);
     }
  }
  MOZ_ASSERT(parsedPolicyStr.Find("default-src") >= 0,
    "about: page must contain a CSP including default-src");
}
#endif

void
nsDocument::EndLoad()
{
#if defined(DEBUG) && !defined(ANDROID)
  AssertContentPrivilegedAboutPageHasCSP(mDocumentURI, NodePrincipal());
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

  NS_DOCUMENT_NOTIFY_OBSERVERS(EndLoad, (this));

  // Part 2: Code that only executes when this EndLoad matches a BeginLoad.

  if (!mDidCallBeginLoad) {
    return;
  }
  mDidCallBeginLoad = false;

  UnblockDOMContentLoaded();
}

void
nsIDocument::UnblockDOMContentLoaded()
{
  MOZ_ASSERT(mBlockDOMContentLoaded);
  if (--mBlockDOMContentLoaded != 0 || mDidFireDOMContentLoaded) {
    return;
  }

  MOZ_LOG(gDocumentLeakPRLog, LogLevel::Debug, ("DOCUMENT %p UnblockDOMContentLoaded", this));

  mDidFireDOMContentLoaded = true;
  if (nsIPresShell* shell = GetShell()) {
    shell->GetRefreshDriver()->NotifyDOMContentLoaded();
  }


  MOZ_ASSERT(mReadyState == READYSTATE_INTERACTIVE);
  if (!mSynchronousDOMContentLoaded) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIRunnable> ev =
      NewRunnableMethod("nsIDocument::DispatchContentLoadedEvents",
                        this,
                        &nsIDocument::DispatchContentLoadedEvents);
    Dispatch(TaskCategory::Other, ev.forget());
  } else {
    DispatchContentLoadedEvents();
  }
}

void
nsIDocument::ContentStateChanged(nsIContent* aContent, EventStates aStateMask)
{
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(),
             "Someone forgot a scriptblocker");
  NS_DOCUMENT_NOTIFY_OBSERVERS(ContentStateChanged,
                               (this, aContent, aStateMask));
}

void
nsIDocument::DocumentStatesChanged(EventStates aStateMask)
{
  UpdateDocumentStates(aStateMask);
  NS_DOCUMENT_NOTIFY_OBSERVERS(DocumentStatesChanged, (this, aStateMask));
}

void
nsIDocument::StyleRuleChanged(StyleSheet* aSheet, css::Rule* aStyleRule)
{
  if (nsIPresShell* shell = GetShell()) {
    shell->ApplicableStylesChanged();
  }

  if (!StyleSheetChangeEventsEnabled()) {
    return;
  }

  DO_STYLESHEET_NOTIFICATION(StyleRuleChangeEvent,
                             "StyleRuleChanged",
                             mRule,
                             aStyleRule);
}

void
nsIDocument::StyleRuleAdded(StyleSheet* aSheet, css::Rule* aStyleRule)
{
  if (nsIPresShell* shell = GetShell()) {
    shell->ApplicableStylesChanged();
  }

  if (!StyleSheetChangeEventsEnabled()) {
    return;
  }

  DO_STYLESHEET_NOTIFICATION(StyleRuleChangeEvent,
                             "StyleRuleAdded",
                             mRule,
                             aStyleRule);
}

void
nsIDocument::StyleRuleRemoved(StyleSheet* aSheet, css::Rule* aStyleRule)
{
  if (nsIPresShell* shell = GetShell()) {
    shell->ApplicableStylesChanged();
  }

  if (!StyleSheetChangeEventsEnabled()) {
    return;
  }

  DO_STYLESHEET_NOTIFICATION(StyleRuleChangeEvent,
                             "StyleRuleRemoved",
                             mRule,
                             aStyleRule);
}

#undef DO_STYLESHEET_NOTIFICATION

already_AddRefed<AnonymousContent>
nsIDocument::InsertAnonymousContent(Element& aElement, ErrorResult& aRv)
{
  nsIPresShell* shell = GetShell();
  if (!shell || !shell->GetCanvasFrame()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsAutoScriptBlocker scriptBlocker;
  nsCOMPtr<Element> container = shell->GetCanvasFrame()
                                     ->GetCustomContentContainer();
  if (!container) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Clone the node to avoid returning a direct reference
  nsCOMPtr<nsINode> clonedElement = aElement.CloneNode(true, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Insert the element into the container
  nsresult rv;
  rv = container->AppendChildTo(clonedElement->AsContent(), true);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  RefPtr<AnonymousContent> anonymousContent =
    new AnonymousContent(clonedElement->AsElement());
  mAnonymousContents.AppendElement(anonymousContent);

  shell->GetCanvasFrame()->ShowCustomContentContainer();

  return anonymousContent.forget();
}

void
nsIDocument::RemoveAnonymousContent(AnonymousContent& aContent,
                                    ErrorResult& aRv)
{
  nsIPresShell* shell = GetShell();
  if (!shell || !shell->GetCanvasFrame()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsAutoScriptBlocker scriptBlocker;
  nsCOMPtr<Element> container = shell->GetCanvasFrame()
                                     ->GetCustomContentContainer();
  if (!container) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  // Iterate over mAnonymousContents to find and remove the given node.
  for (size_t i = 0, len = mAnonymousContents.Length(); i < len; ++i) {
    if (mAnonymousContents[i] == &aContent) {
      // Get the node from the customContent
      nsCOMPtr<Element> node = aContent.GetContentNode();

      // Remove the entry in mAnonymousContents
      mAnonymousContents.RemoveElementAt(i);

      // Remove the node from its container
      container->RemoveChild(*node, aRv);
      if (aRv.Failed()) {
        return;
      }

      break;
    }
  }
  if (mAnonymousContents.IsEmpty()) {
    shell->GetCanvasFrame()->HideCustomContentContainer();
  }
}

Element*
nsIDocument::GetAnonRootIfInAnonymousContentContainer(nsINode* aNode) const
{
  if (!aNode->IsInNativeAnonymousSubtree()) {
    return nullptr;
  }

  nsIPresShell* shell = GetShell();
  if (!shell || !shell->GetCanvasFrame()) {
    return nullptr;
  }

  nsAutoScriptBlocker scriptBlocker;
  nsCOMPtr<Element> customContainer = shell->GetCanvasFrame()
                                           ->GetCustomContentContainer();
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

Maybe<ClientInfo>
nsIDocument::GetClientInfo() const
{
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    return inner->GetClientInfo();
  }
  return Maybe<ClientInfo>();
}

Maybe<ClientState>
nsIDocument::GetClientState() const
{
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    return inner->GetClientState();
  }
  return Maybe<ClientState>();
}

Maybe<ServiceWorkerDescriptor>
nsIDocument::GetController() const
{
  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    return inner->GetController();
  }
  return Maybe<ServiceWorkerDescriptor>();
}

//
// nsIDocument interface
//
DocumentType*
nsIDocument::GetDoctype() const
{
  for (nsIContent* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->NodeType() == DOCUMENT_TYPE_NODE) {
      return static_cast<DocumentType*>(child);
    }
  }
  return nullptr;
}

DOMImplementation*
nsIDocument::GetImplementation(ErrorResult& rv)
{
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
    mDOMImplementation = new DOMImplementation(this,
      scriptObject ? scriptObject : GetScopeObject(), uri, uri);
  }

  return mDOMImplementation;
}

bool IsLowercaseASCII(const nsAString& aValue)
{
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
static CSSPseudoElementType
GetPseudoElementType(const nsString& aString, ErrorResult& aRv)
{
  MOZ_ASSERT(!aString.IsEmpty(), "GetPseudoElementType aString should be non-null");
  if (aString.Length() <= 2 || aString[0] != ':' || aString[1] != ':') {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return CSSPseudoElementType::NotPseudo;
  }
  RefPtr<nsAtom> pseudo = NS_Atomize(Substring(aString, 1));
  return nsCSSPseudoElements::GetPseudoType(pseudo,
      CSSEnabledState::eInUASheets);
}

already_AddRefed<Element>
nsIDocument::CreateElement(const nsAString& aTagName,
                           const ElementCreationOptionsOrString& aOptions,
                           ErrorResult& rv)
{
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
  CSSPseudoElementType pseudoType = CSSPseudoElementType::NotPseudo;
  if (aOptions.IsElementCreationOptions()) {
    const ElementCreationOptions& options =
      aOptions.GetAsElementCreationOptions();

    if (CustomElementRegistry::IsCustomElementEnabled(this) &&
        options.mIs.WasPassed()) {
      is = &options.mIs.Value();
    }

    // Check 'pseudo' and throw an exception if it's not one allowed
    // with CSS_PSEUDO_ELEMENT_IS_JS_CREATED_NAC.
    if (options.mPseudo.WasPassed()) {
      pseudoType = GetPseudoElementType(options.mPseudo.Value(), rv);
      if (rv.Failed() ||
          pseudoType == CSSPseudoElementType::NotPseudo ||
          !nsCSSPseudoElements::PseudoElementIsJSCreatedNAC(pseudoType)) {
        rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }
    }
  }

  RefPtr<Element> elem = CreateElem(
    needsLowercase ? lcTagName : aTagName, nullptr, mDefaultElementType, is);

  if (pseudoType != CSSPseudoElementType::NotPseudo) {
    elem->SetPseudoElementType(pseudoType);
  }

  return elem.forget();
}

already_AddRefed<Element>
nsIDocument::CreateElementNS(const nsAString& aNamespaceURI,
                             const nsAString& aQualifiedName,
                             const ElementCreationOptionsOrString& aOptions,
                             ErrorResult& rv)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI,
                                            aQualifiedName,
                                            mNodeInfoManager,
                                            ELEMENT_NODE,
                                            getter_AddRefs(nodeInfo));
  if (rv.Failed()) {
    return nullptr;
  }

  const nsString* is = nullptr;
  if (CustomElementRegistry::IsCustomElementEnabled(this) &&
      aOptions.IsElementCreationOptions()) {
    const ElementCreationOptions& options = aOptions.GetAsElementCreationOptions();
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

already_AddRefed<nsTextNode>
nsIDocument::CreateEmptyTextNode() const
{
  RefPtr<nsTextNode> text = new nsTextNode(mNodeInfoManager);
  return text.forget();
}

already_AddRefed<nsTextNode>
nsIDocument::CreateTextNode(const nsAString& aData) const
{
  RefPtr<nsTextNode> text = new nsTextNode(mNodeInfoManager);
  // Don't notify; this node is still being created.
  text->SetText(aData, false);
  return text.forget();
}

already_AddRefed<DocumentFragment>
nsIDocument::CreateDocumentFragment() const
{
  RefPtr<DocumentFragment> frag = new DocumentFragment(mNodeInfoManager);
  return frag.forget();
}

// Unfortunately, bareword "Comment" is ambiguous with some Mac system headers.
already_AddRefed<dom::Comment>
nsIDocument::CreateComment(const nsAString& aData) const
{
  RefPtr<dom::Comment> comment = new dom::Comment(mNodeInfoManager);

  // Don't notify; this node is still being created.
  comment->SetText(aData, false);
  return comment.forget();
}

already_AddRefed<CDATASection>
nsIDocument::CreateCDATASection(const nsAString& aData,
                                ErrorResult& rv)
{
  if (IsHTMLDocument()) {
    rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (FindInReadable(NS_LITERAL_STRING("]]>"), aData)) {
    rv.Throw(NS_ERROR_DOM_INVALID_CHARACTER_ERR);
    return nullptr;
  }

  RefPtr<CDATASection> cdata = new CDATASection(mNodeInfoManager);

  // Don't notify; this node is still being created.
  cdata->SetText(aData, false);

  return cdata.forget();
}

already_AddRefed<ProcessingInstruction>
nsIDocument::CreateProcessingInstruction(const nsAString& aTarget,
                                         const nsAString& aData,
                                         ErrorResult& rv) const
{
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

already_AddRefed<Attr>
nsIDocument::CreateAttribute(const nsAString& aName, ErrorResult& rv)
{
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

  RefPtr<Attr> attribute = new Attr(nullptr, nodeInfo.forget(),
                                    EmptyString());
  return attribute.forget();
}

already_AddRefed<Attr>
nsIDocument::CreateAttributeNS(const nsAString& aNamespaceURI,
                               const nsAString& aQualifiedName,
                               ErrorResult& rv)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  rv = nsContentUtils::GetNodeInfoFromQName(aNamespaceURI,
                                            aQualifiedName,
                                            mNodeInfoManager,
                                            ATTRIBUTE_NODE,
                                            getter_AddRefs(nodeInfo));
  if (rv.Failed()) {
    return nullptr;
  }

  RefPtr<Attr> attribute = new Attr(nullptr, nodeInfo.forget(),
                                    EmptyString());
  return attribute.forget();
}

void
nsIDocument::ResolveScheduledSVGPresAttrs()
{
  for (auto iter = mLazySVGPresElements.Iter(); !iter.Done(); iter.Next()) {
    nsSVGElement* svg = iter.Get()->GetKey();
    svg->UpdateContentDeclarationBlock();
  }
  mLazySVGPresElements.Clear();
}

already_AddRefed<nsSimpleContentList>
nsIDocument::BlockedTrackingNodes() const
{
  RefPtr<nsSimpleContentList> list = new nsSimpleContentList(nullptr);

  nsTArray<nsWeakPtr> blockedTrackingNodes;
  blockedTrackingNodes = mBlockedTrackingNodes;

  for (unsigned long i = 0; i < blockedTrackingNodes.Length(); i++) {
    nsWeakPtr weakNode = blockedTrackingNodes[i];
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

void
nsIDocument::GetSelectedStyleSheetSet(nsAString& aSheetSet)
{
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

void
nsIDocument::SetSelectedStyleSheetSet(const nsAString& aSheetSet)
{
  if (DOMStringIsNull(aSheetSet)) {
    return;
  }

  // Must update mLastStyleSheetSet before doing anything else with stylesheets
  // or CSSLoaders.
  mLastStyleSheetSet = aSheetSet;
  EnableStyleSheetsForSetInternal(aSheetSet, true);
}

void
nsIDocument::SetPreferredStyleSheetSet(const nsAString& aSheetSet)
{
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

DOMStringList*
nsIDocument::StyleSheetSets()
{
  if (!mStyleSheetSetList) {
    mStyleSheetSetList = new nsDOMStyleSheetSetList(this);
  }
  return mStyleSheetSetList;
}

void
nsIDocument::EnableStyleSheetsForSet(const nsAString& aSheetSet)
{
  // Per spec, passing in null is a no-op.
  if (!DOMStringIsNull(aSheetSet)) {
    // Note: must make sure to not change the CSSLoader's preferred sheet --
    // that value should be equal to either our lastStyleSheetSet (if that's
    // non-null) or to our preferredStyleSheetSet.  And this method doesn't
    // change either of those.
    EnableStyleSheetsForSetInternal(aSheetSet, false);
  }
}

void
nsIDocument::EnableStyleSheetsForSetInternal(const nsAString& aSheetSet,
                                             bool aUpdateCSSLoader)
{
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
  if (nsIPresShell* shell = GetShell()) {
    if (shell->StyleSet()->StyleSheetsHaveChanged()) {
      shell->ApplicableStylesChanged();
    }
  }
}

void
nsIDocument::GetCharacterSet(nsAString& aCharacterSet) const
{
  nsAutoCString charset;
  GetDocumentCharacterSet()->Name(charset);
  CopyASCIItoUTF16(charset, aCharacterSet);
}

already_AddRefed<nsINode>
nsIDocument::ImportNode(nsINode& aNode, bool aDeep, ErrorResult& rv) const
{
  nsINode* imported = &aNode;

  switch (imported->NodeType()) {
    case DOCUMENT_NODE:
    {
      break;
    }
    case DOCUMENT_FRAGMENT_NODE:
    case ATTRIBUTE_NODE:
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case COMMENT_NODE:
    case DOCUMENT_TYPE_NODE:
    {
      return nsNodeUtils::Clone(imported, aDeep, mNodeInfoManager, nullptr, rv);
    }
    default:
    {
      NS_WARNING("Don't know how to clone this nodetype for importNode.");
    }
  }

  rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

void
nsIDocument::LoadBindingDocument(const nsAString& aURI,
                                 nsIPrincipal& aSubjectPrincipal,
                                 ErrorResult& rv)
{
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aURI, mCharacterSet, GetDocBaseURI());
  if (rv.Failed()) {
    return;
  }

  BindingManager()->LoadBindingDocument(this, uri, &aSubjectPrincipal);
}

Element*
nsIDocument::GetBindingParent(nsINode& aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(&aNode));
  if (!content)
    return nullptr;

  nsIContent* bindingParent = content->GetBindingParent();
  return bindingParent ? bindingParent->AsElement() : nullptr;
}

static Element*
GetElementByAttribute(Element* aElement, nsAtom* aAttrName,
                      const nsAString& aAttrValue, bool aUniversalMatch)
{
  if (aUniversalMatch ? aElement->HasAttr(kNameSpaceID_None, aAttrName) :
                        aElement->AttrValueIs(kNameSpaceID_None, aAttrName,
                                              aAttrValue, eCaseMatters)) {
    return aElement;
  }

  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (!child->IsElement()) {
      continue;
    }

    Element* matchedElement =
      GetElementByAttribute(child->AsElement(), aAttrName, aAttrValue,
                            aUniversalMatch);
    if (matchedElement)
      return matchedElement;
  }

  return nullptr;
}

Element*
nsIDocument::GetAnonymousElementByAttribute(nsIContent* aElement,
                                            nsAtom* aAttrName,
                                            const nsAString& aAttrValue) const
{
  nsINodeList* nodeList = BindingManager()->GetAnonymousNodesFor(aElement);
  if (!nodeList)
    return nullptr;

  uint32_t length = nodeList->Length();

  bool universalMatch = aAttrValue.EqualsLiteral("*");

  for (uint32_t i = 0; i < length; ++i) {
    Element* current = Element::FromNode(nodeList->Item(i));
    if (!current) {
      continue;
    }

    Element* matchedElm =
      GetElementByAttribute(current, aAttrName, aAttrValue, universalMatch);
    if (matchedElm)
      return matchedElm;
  }

  return nullptr;
}

Element*
nsIDocument::GetAnonymousElementByAttribute(Element& aElement,
                                            const nsAString& aAttrName,
                                            const nsAString& aAttrValue)
{
  RefPtr<nsAtom> attribute = NS_Atomize(aAttrName);

  return GetAnonymousElementByAttribute(&aElement, attribute, aAttrValue);
}

nsINodeList*
nsIDocument::GetAnonymousNodes(Element& aElement)
{
  return BindingManager()->GetAnonymousNodesFor(&aElement);
}

already_AddRefed<nsRange>
nsIDocument::CreateRange(ErrorResult& rv)
{
  RefPtr<nsRange> range = new nsRange(this);
  nsresult res = range->CollapseTo(this, 0);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return nullptr;
  }

  return range.forget();
}

already_AddRefed<NodeIterator>
nsIDocument::CreateNodeIterator(nsINode& aRoot, uint32_t aWhatToShow,
                                NodeFilter* aFilter,
                                ErrorResult& rv) const
{
  RefPtr<NodeIterator> iterator = new NodeIterator(&aRoot, aWhatToShow,
                                                   aFilter);
  return iterator.forget();
}

already_AddRefed<TreeWalker>
nsIDocument::CreateTreeWalker(nsINode& aRoot, uint32_t aWhatToShow,
                              NodeFilter* aFilter,
                              ErrorResult& rv) const
{
  RefPtr<TreeWalker> walker = new TreeWalker(&aRoot, aWhatToShow, aFilter);
  return walker.forget();
}


already_AddRefed<Location>
nsIDocument::GetLocation() const
{
  nsCOMPtr<nsPIDOMWindowInner> w = do_QueryInterface(mScriptGlobalObject);

  if (!w) {
    return nullptr;
  }

  nsGlobalWindowInner* window = nsGlobalWindowInner::Cast(w);
  RefPtr<Location> loc = window->GetLocation();
  return loc.forget();
}

Element*
nsIDocument::GetHtmlElement() const
{
  Element* rootElement = GetRootElement();
  if (rootElement && rootElement->IsHTMLElement(nsGkAtoms::html))
    return rootElement;
  return nullptr;
}

Element*
nsIDocument::GetHtmlChildElement(nsAtom* aTag)
{
  Element* html = GetHtmlElement();
  if (!html)
    return nullptr;

  // Look for the element with aTag inside html. This needs to run
  // forwards to find the first such element.
  for (nsIContent* child = html->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(aTag))
      return child->AsElement();
  }
  return nullptr;
}

nsGenericHTMLElement*
nsIDocument::GetBody()
{
  Element* html = GetHtmlElement();
  if (!html) {
    return nullptr;
  }

  for (nsIContent* child = html->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::body) ||
        child->IsHTMLElement(nsGkAtoms::frameset)) {
      return static_cast<nsGenericHTMLElement*>(child);
    }
  }

  return nullptr;
}

void
nsIDocument::SetBody(nsGenericHTMLElement* newBody, ErrorResult& rv)
{
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

HTMLSharedElement*
nsIDocument::GetHead()
{
  return static_cast<HTMLSharedElement*>(GetHeadElement());
}

Element*
nsIDocument::GetTitleElement()
{
  // mMayHaveTitleElement will have been set to true if any HTML or SVG
  // <title> element has been bound to this document. So if it's false,
  // we know there is nothing to do here. This avoids us having to search
  // the whole DOM if someone calls document.title on a large document
  // without a title.
  if (!mMayHaveTitleElement)
    return nullptr;

  Element* root = GetRootElement();
  if (root && root->IsSVGElement(nsGkAtoms::svg)) {
    // In SVG, the document's title must be a child
    for (nsIContent* child = root->GetFirstChild();
         child; child = child->GetNextSibling()) {
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
  RefPtr<nsContentList> list = new nsContentList(this, kNameSpaceID_XHTML,
                                                 nsGkAtoms::title, nsGkAtoms::title,
                                                 /* aDeep = */ true,
                                                 /* aLiveList = */ false);

  nsIContent* first = list->Item(0, false);

  return first ? first->AsElement() : nullptr;
}

void
nsIDocument::GetTitle(nsAString& aTitle)
{
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

void
nsIDocument::SetTitle(const nsAString& aTitle, ErrorResult& aRv)
{
  Element* rootElement = GetRootElement();
  if (!rootElement) {
    return;
  }

#ifdef MOZ_XUL
  if (rootElement->IsXULElement()) {
    aRv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::title,
                               aTitle, true);
    return;
  }
#endif

  // Batch updates so that mutation events don't change "the title
  // element" under us
  mozAutoDocUpdate updateBatch(this, true);

  nsCOMPtr<Element> title = GetTitleElement();
  if (rootElement->IsSVGElement(nsGkAtoms::svg)) {
    if (!title) {
      RefPtr<mozilla::dom::NodeInfo> titleInfo =
        mNodeInfoManager->GetNodeInfo(nsGkAtoms::title, nullptr,
                                      kNameSpaceID_SVG,
                                      ELEMENT_NODE);
      NS_NewSVGElement(getter_AddRefs(title), titleInfo.forget(),
                       NOT_FROM_PARSER);
      if (!title) {
        return;
      }
      rootElement->InsertChildBefore(title, rootElement->GetFirstChild(), true);
    }
  } else if (rootElement->IsHTMLElement()) {
    if (!title) {
      Element* head = GetHeadElement();
      if (!head) {
        return;
      }

      RefPtr<mozilla::dom::NodeInfo> titleInfo;
      titleInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::title, nullptr,
          kNameSpaceID_XHTML, ELEMENT_NODE);
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

void
nsIDocument::NotifyPossibleTitleChange(bool aBoundTitleElement)
{
  NS_ASSERTION(!mInUnlinkOrDeletion || !aBoundTitleElement,
               "Setting a title while unlinking or destroying the element?");
  if (mInUnlinkOrDeletion) {
    return;
  }

  if (aBoundTitleElement) {
    mMayHaveTitleElement = true;
  }
  if (mPendingTitleChangeEvent.IsPending())
    return;

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  RefPtr<nsRunnableMethod<nsIDocument, void, false>> event =
    NewNonOwningRunnableMethod("nsIDocument::DoNotifyPossibleTitleChange",
                               this,
                               &nsIDocument::DoNotifyPossibleTitleChange);
  nsresult rv = Dispatch(TaskCategory::Other, do_AddRef(event));
  if (NS_SUCCEEDED(rv)) {
    mPendingTitleChangeEvent = std::move(event);
  }
}

void
nsIDocument::DoNotifyPossibleTitleChange()
{
  mPendingTitleChangeEvent.Forget();
  mHaveFiredTitleChange = true;

  nsAutoString title;
  GetTitle(title);

  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    nsCOMPtr<nsISupports> container =
      shell->GetPresContext()->GetContainerWeak();
    if (container) {
      nsCOMPtr<nsIBaseWindow> docShellWin = do_QueryInterface(container);
      if (docShellWin) {
        docShellWin->SetTitle(title);
      }
    }
  }

  // Fire a DOM event for the title change.
  nsContentUtils::DispatchChromeEvent(this, static_cast<nsIDocument*>(this),
                                      NS_LITERAL_STRING("DOMTitleChanged"),
                                      CanBubble::eYes, Cancelable::eYes);
}

already_AddRefed<BoxObject>
nsIDocument::GetBoxObjectFor(Element* aElement, ErrorResult& aRv)
{
  if (!aElement) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsIDocument* doc = aElement->OwnerDoc();
  if (doc != this) {
    aRv.Throw(NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
    return nullptr;
  }

  if (!mHasWarnedAboutBoxObjects && !aElement->IsXULElement()) {
    mHasWarnedAboutBoxObjects = true;
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("BoxObjects"), this,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "UseOfGetBoxObjectForWarning");
  }

  if (!mBoxObjectTable) {
    mBoxObjectTable = new nsRefPtrHashtable<nsPtrHashKey<nsIContent>, BoxObject>(6);
  }

  RefPtr<BoxObject> boxObject;
  auto entry = mBoxObjectTable->LookupForAdd(aElement);
  if (entry) {
    boxObject = entry.Data();
    return boxObject.forget();
  }

  int32_t namespaceID;
  RefPtr<nsAtom> tag = BindingManager()->ResolveTag(aElement, &namespaceID);
#ifdef MOZ_XUL
  if (namespaceID == kNameSpaceID_XUL) {
    if (tag == nsGkAtoms::menu) {
      boxObject = new MenuBoxObject();
    } else if (tag == nsGkAtoms::tree) {
      boxObject = new TreeBoxObject();
    } else if (tag == nsGkAtoms::listbox) {
      boxObject = new ListBoxObject();
    } else {
      boxObject = new BoxObject();
    }
  } else
#endif // MOZ_XUL
  {
    boxObject = new BoxObject();
  }

  boxObject->Init(aElement);
  entry.OrInsert([&boxObject]() { return boxObject; });

  return boxObject.forget();
}

void
nsIDocument::ClearBoxObjectFor(nsIContent* aContent)
{
  if (mBoxObjectTable) {
    if (auto entry = mBoxObjectTable->Lookup(aContent)) {
      nsPIBoxObject* boxObject = entry.Data();
      boxObject->Clear();
      entry.Remove();
    }
  }
}

already_AddRefed<MediaQueryList>
nsIDocument::MatchMedia(const nsAString& aMediaQueryList,
                        CallerType aCallerType)
{
  RefPtr<MediaQueryList> result =
    new MediaQueryList(this, aMediaQueryList, aCallerType);

  mDOMMediaQueryLists.insertBack(result);

  return result.forget();
}

void
nsIDocument::FlushSkinBindings()
{
  BindingManager()->FlushSkinBindings();
}

void
nsIDocument::SetMayStartLayout(bool aMayStartLayout)
{
  mMayStartLayout = aMayStartLayout;
  if (MayStartLayout()) {
    // Before starting layout, check whether we're a toplevel chrome
    // window.  If we are, setup some state so that we don't have to restyle
    // the whole tree after StartLayout.
    if (nsCOMPtr<nsIXULWindow> win = GetXULWindowIfToplevelChrome()) {
        // We're the chrome document!
        win->BeforeStartLayout();
    }
    ReadyState state = GetReadyStateEnum();
    if (state >= READYSTATE_INTERACTIVE) {
      // DOMContentLoaded has fired already.
      MaybeResolveReadyForIdle();
    }
  }
}

nsresult
nsIDocument::InitializeFrameLoader(nsFrameLoader* aLoader)
{
  mInitializableFrameLoaders.RemoveElement(aLoader);
  // Don't even try to initialize.
  if (mInDestructor) {
    NS_WARNING("Trying to initialize a frame loader while"
               "document is being deleted");
    return NS_ERROR_FAILURE;
  }

  mInitializableFrameLoaders.AppendElement(aLoader);
  if (!mFrameLoaderRunner) {
    mFrameLoaderRunner =
      NewRunnableMethod("nsIDocument::MaybeInitializeFinalizeFrameLoaders",
                        this,
                        &nsIDocument::MaybeInitializeFinalizeFrameLoaders);
    NS_ENSURE_TRUE(mFrameLoaderRunner, NS_ERROR_OUT_OF_MEMORY);
    nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
  }
  return NS_OK;
}

nsresult
nsIDocument::FinalizeFrameLoader(nsFrameLoader* aLoader, nsIRunnable* aFinalizer)
{
  mInitializableFrameLoaders.RemoveElement(aLoader);
  if (mInDestructor) {
    return NS_ERROR_FAILURE;
  }

  mFrameLoaderFinalizers.AppendElement(aFinalizer);
  if (!mFrameLoaderRunner) {
    mFrameLoaderRunner =
      NewRunnableMethod("nsIDocument::MaybeInitializeFinalizeFrameLoaders",
                        this,
                        &nsIDocument::MaybeInitializeFinalizeFrameLoaders);
    NS_ENSURE_TRUE(mFrameLoaderRunner, NS_ERROR_OUT_OF_MEMORY);
    nsContentUtils::AddScriptRunner(mFrameLoaderRunner);
  }
  return NS_OK;
}

void
nsIDocument::MaybeInitializeFinalizeFrameLoaders()
{
  if (mDelayFrameLoaderInitialization || mUpdateNestLevel != 0) {
    // This method will be recalled when mUpdateNestLevel drops to 0,
    // or when !mDelayFrameLoaderInitialization.
    mFrameLoaderRunner = nullptr;
    return;
  }

  // We're not in an update, but it is not safe to run scripts, so
  // postpone frameloader initialization and finalization.
  if (!nsContentUtils::IsSafeToRunScript()) {
    if (!mInDestructor && !mFrameLoaderRunner &&
        (mInitializableFrameLoaders.Length() ||
         mFrameLoaderFinalizers.Length())) {
      mFrameLoaderRunner =
        NewRunnableMethod("nsIDocument::MaybeInitializeFinalizeFrameLoaders",
                          this,
                          &nsIDocument::MaybeInitializeFinalizeFrameLoaders);
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
    nsTArray<nsCOMPtr<nsIRunnable> > finalizers;
    mFrameLoaderFinalizers.SwapElements(finalizers);
    for (uint32_t i = 0; i < length; ++i) {
      finalizers[i]->Run();
    }
  }
}

void
nsIDocument::TryCancelFrameLoaderInitialization(nsIDocShell* aShell)
{
  uint32_t length = mInitializableFrameLoaders.Length();
  for (uint32_t i = 0; i < length; ++i) {
    if (mInitializableFrameLoaders[i]->GetExistingDocShell() == aShell) {
      mInitializableFrameLoaders.RemoveElementAt(i);
      return;
    }
  }
}

nsIDocument*
nsIDocument::RequestExternalResource(nsIURI* aURI,
                                     nsINode* aRequestingNode,
                                     ExternalResourceLoad** aPendingLoad)
{
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aRequestingNode, "Must have a node");
  if (mDisplayDocument) {
    return mDisplayDocument->RequestExternalResource(aURI,
                                                     aRequestingNode,
                                                     aPendingLoad);
  }

  return mExternalResourceMap.RequestResource(aURI, aRequestingNode,
                                              this, aPendingLoad);
}

void
nsIDocument::EnumerateExternalResources(nsSubDocEnumFunc aCallback, void* aData)
{
  mExternalResourceMap.EnumerateResources(aCallback, aData);
}

nsSMILAnimationController*
nsIDocument::GetAnimationController()
{
  // We create the animation controller lazily because most documents won't want
  // one and only SVG documents and the like will call this
  if (mAnimationController)
    return mAnimationController;
  // Refuse to create an Animation Controller for data documents.
  if (mLoadedAsData || mLoadedAsInteractiveData)
    return nullptr;

  mAnimationController = new nsSMILAnimationController(this);

  // If there's a presContext then check the animation mode and pause if
  // necessary.
  nsPresContext* context = GetPresContext();
  if (mAnimationController && context &&
      context->ImageAnimationMode() == imgIContainer::kDontAnimMode) {
    mAnimationController->Pause(nsSMILTimeContainer::PAUSE_USERPREF);
  }

  // If we're hidden (or being hidden), notify the newly-created animation
  // controller. (Skip this check for SVG-as-an-image documents, though,
  // because they don't get OnPageShow / OnPageHide calls).
  if (!mIsShowing && !mIsBeingUsedAsImage) {
    mAnimationController->OnPageHide();
  }

  return mAnimationController;
}

PendingAnimationTracker*
nsIDocument::GetOrCreatePendingAnimationTracker()
{
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
void
nsIDocument::GetDir(nsAString& aDirection) const
{
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
void
nsIDocument::SetDir(const nsAString& aDirection)
{
  Element* rootElement = GetHtmlElement();
  if (rootElement) {
    rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir,
                         aDirection, true);
  }
}

nsIHTMLCollection*
nsIDocument::Images()
{
  if (!mImages) {
    mImages = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::img, nsGkAtoms::img);
  }
  return mImages;
}

nsIHTMLCollection*
nsIDocument::Embeds()
{
  if (!mEmbeds) {
    mEmbeds = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::embed, nsGkAtoms::embed);
  }
  return mEmbeds;
}

static bool
MatchLinks(Element* aElement, int32_t aNamespaceID,
           nsAtom* aAtom, void* aData)
{
  return aElement->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area) &&
         aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::href);
}

nsIHTMLCollection*
nsIDocument::Links()
{
  if (!mLinks) {
    mLinks = new nsContentList(this, MatchLinks, nullptr, nullptr);
  }
  return mLinks;
}

nsIHTMLCollection*
nsIDocument::Forms()
{
  if (!mForms) {
    // Please keep this in sync with nsHTMLDocument::GetFormsAndFormControls.
    mForms = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::form, nsGkAtoms::form);
  }

  return mForms;
}

nsIHTMLCollection*
nsIDocument::Scripts()
{
  if (!mScripts) {
    mScripts = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::script, nsGkAtoms::script);
  }
  return mScripts;
}

nsIHTMLCollection*
nsIDocument::Applets()
{
  if (!mApplets) {
    mApplets = new nsEmptyContentList(this);
  }
  return mApplets;
}

static bool
MatchAnchors(Element* aElement, int32_t aNamespaceID,
             nsAtom* aAtom, void* aData)
{
  return aElement->IsHTMLElement(nsGkAtoms::a) &&
         aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::name);
}

nsIHTMLCollection*
nsIDocument::Anchors()
{
  if (!mAnchors) {
    mAnchors = new nsContentList(this, MatchAnchors, nullptr, nullptr);
  }
  return mAnchors;
}

/* static */
bool
nsIDocument::MatchNameAttribute(Element* aElement, int32_t aNamespaceID,
                                nsAtom* aAtom, void* aData)
{
  MOZ_ASSERT(aElement, "Must have element to work with!");

  if (!aElement->HasName()) {
    return false;
  }

  nsString* elementName = static_cast<nsString*>(aData);
  return
    aElement->GetNameSpaceID() == kNameSpaceID_XHTML &&
    aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                          *elementName, eCaseMatters);
}

/* static */
void*
nsIDocument::UseExistingNameString(nsINode* aRootNode, const nsString* aName)
{
  return const_cast<nsString*>(aName);
}

nsresult
nsIDocument::GetDocumentURI(nsString& aDocumentURI) const
{
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
nsresult
nsIDocument::GetURL(nsString& aURL) const
{
  return GetDocumentURI(aURL);
}

void
nsIDocument::GetDocumentURIFromJS(nsString& aDocumentURI, CallerType aCallerType,
                                  ErrorResult& aRv) const
{
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

nsIURI*
nsIDocument::GetDocumentURIObject() const
{
  if (!mChromeXHRDocURI) {
    return GetDocumentURI();
  }

  return mChromeXHRDocURI;
}

void
nsIDocument::GetCompatMode(nsString& aCompatMode) const
{
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

void
nsDOMAttributeMap::BlastSubtreeToPieces(nsINode* aNode)
{
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

        DebugOnly<nsresult> rv =
          element->UnsetAttr(attr->NodeInfo()->NamespaceID(),
                             attr->NodeInfo()->NameAtom(),
                             false);

        // XXX Should we abort here?
        NS_ASSERTION(NS_SUCCEEDED(rv), "Uh-oh, UnsetAttr shouldn't fail!");
      }
    }
  }

  while (aNode->HasChildren()) {
    nsIContent* node = aNode->GetFirstChild();
    BlastSubtreeToPieces(node);
    aNode->RemoveChildNode(node, false);
  }
}

nsINode*
nsIDocument::AdoptNode(nsINode& aAdoptedNode, ErrorResult& rv)
{
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
    case ATTRIBUTE_NODE:
    {
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
    case DOCUMENT_FRAGMENT_NODE:
    {
      if (adoptedNode->IsShadowRoot()) {
        rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
        return nullptr;
      }
      MOZ_FALLTHROUGH;
    }
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case COMMENT_NODE:
    case DOCUMENT_TYPE_NODE:
    {
      // Don't allow adopting a node's anonymous subtree out from under it.
      if (adoptedNode->AsContent()->IsRootOfAnonymousSubtree()) {
        rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }

      // We don't want to adopt an element into its own contentDocument or into
      // a descendant contentDocument, so we check if the frameElement of this
      // document or any of its parents is the adopted node or one of its
      // descendants.
      nsIDocument *doc = this;
      do {
        if (nsPIDOMWindowOuter *win = doc->GetWindow()) {
          nsCOMPtr<nsINode> node = win->GetFrameElementInternal();
          if (node &&
              nsContentUtils::ContentIsDescendantOf(node, adoptedNode)) {
            rv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
            return nullptr;
          }
        }
      } while ((doc = doc->GetParentDocument()));

      // Remove from parent.
      nsCOMPtr<nsINode> parent = adoptedNode->GetParentNode();
      if (parent) {
        parent->RemoveChildNode(adoptedNode->AsContent(), true);
      } else {
        MOZ_ASSERT(!adoptedNode->IsInUncomposedDoc());

        // If we're adopting a node that's not in a document, it might still
        // have a binding applied. Remove the binding from the element now
        // that it's getting adopted into a new document.
        // TODO Fully tear down the binding.
        if (Element* element = Element::FromNode(adoptedNode)) {
          element->SetXBLBinding(nullptr);
        }
      }

      break;
    }
    case DOCUMENT_NODE:
    {
      rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
    default:
    {
      NS_WARNING("Don't know how to adopt this nodetype for adoptNode.");

      rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  }

  nsCOMPtr<nsIDocument> oldDocument = adoptedNode->OwnerDoc();
  bool sameDocument = oldDocument == this;

  AutoJSContext cx;
  JS::Rooted<JSObject*> newScope(cx, nullptr);
  if (!sameDocument) {
    newScope = GetWrapper();
    if (!newScope && GetScopeObject() && GetScopeObject()->GetGlobalJSObject()) {
      // Make sure cx is in a semi-sane compartment before we call WrapNative.
      // It's kind of irrelevant, given that we're passing aAllowWrapping =
      // false, and documents should always insist on being wrapped in an
      // canonical scope. But we try to pass something sane anyway.
      JSAutoRealm ar(cx, GetScopeObject()->GetGlobalJSObject());
      JS::Rooted<JS::Value> v(cx);
      rv = nsContentUtils::WrapNative(cx, this, this, &v,
                                      /* aAllowWrapping = */ false);
      if (rv.Failed())
        return nullptr;
      newScope = &v.toObject();
    }
  }

  nsCOMArray<nsINode> nodesWithProperties;
  nsNodeUtils::Adopt(adoptedNode, sameDocument ? nullptr : mNodeInfoManager,
                     newScope, nodesWithProperties, rv);
  if (rv.Failed()) {
    // Disconnect all nodes from their parents, since some have the old document
    // as their ownerDocument and some have this as their ownerDocument.
    nsDOMAttributeMap::BlastSubtreeToPieces(adoptedNode);

    if (!sameDocument && oldDocument) {
      for (nsINode* node : nodesWithProperties) {
        // Remove all properties.
        oldDocument->PropertyTable().DeleteAllPropertiesFor(node);
      }
    }

    return nullptr;
  }

  if (!sameDocument && oldDocument) {
    nsPropertyTable& oldTable = oldDocument->PropertyTable();
    nsPropertyTable& newTable = PropertyTable();
    for (nsINode* node : nodesWithProperties) {
      rv = oldTable.TransferOrDeleteAllPropertiesFor(node, newTable);
    }

    if (rv.Failed()) {
      // Disconnect all nodes from their parents.
      nsDOMAttributeMap::BlastSubtreeToPieces(adoptedNode);

      return nullptr;
    }
  }

  NS_ASSERTION(adoptedNode->OwnerDoc() == this,
               "Should still be in the document we just got adopted into");

  return adoptedNode;
}

nsViewportInfo
nsIDocument::GetViewportInfo(const ScreenIntSize& aDisplaySize)
{
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
  CSSToLayoutDeviceScale layoutDeviceScale = context ? context->CSSToDevPixelScale() : CSSToLayoutDeviceScale(1);

  CSSToScreenScale defaultScale = layoutDeviceScale
                                * LayoutDeviceToScreenScale(1.0);

  // Special behaviour for desktop mode, provided we are not on an about: page
  nsPIDOMWindowOuter* win = GetWindow();
  if (win && win->IsDesktopModeViewport() && !IsAboutPage()) {
    CSSCoord viewportWidth = gfxPrefs::DesktopViewportWidth() / fullZoom;
    CSSToScreenScale scaleToFit(aDisplaySize.width / viewportWidth);
    float aspectRatio = (float)aDisplaySize.height / aDisplaySize.width;
    CSSSize viewportSize(viewportWidth, viewportWidth * aspectRatio);
    ScreenIntSize fakeDesktopSize = RoundedToInt(viewportSize * scaleToFit);
    return nsViewportInfo(fakeDesktopSize,
                          scaleToFit,
                          /*allowZoom*/ true);
  }

  if (!gfxPrefs::MetaViewportEnabled()) {
    return nsViewportInfo(aDisplaySize,
                          defaultScale,
                          /*allowZoom*/ false);
  }

  // In cases where the width of the CSS viewport is less than or equal to the width
  // of the display (i.e. width <= device-width) then we disable double-tap-to-zoom
  // behaviour. See bug 941995 for details.

  switch (mViewportType) {
  case DisplayWidthHeight:
    return nsViewportInfo(aDisplaySize,
                          defaultScale,
                          /*allowZoom*/ true);
  case Unknown:
  {
    nsAutoString viewport;
    GetHeaderData(nsGkAtoms::viewport, viewport);
    if (viewport.IsEmpty()) {
      // If the docType specifies that we are on a site optimized for mobile,
      // then we want to return specially crafted defaults for the viewport info.
      RefPtr<DocumentType> docType = GetDoctype();
      if (docType) {
        nsAutoString docId;
        docType->GetPublicId(docId);
        if ((docId.Find("WAP") != -1) ||
            (docId.Find("Mobile") != -1) ||
            (docId.Find("WML") != -1))
        {
          // We're making an assumption that the docType can't change here
          mViewportType = DisplayWidthHeight;
          return nsViewportInfo(aDisplaySize,
                                defaultScale,
                                /*allowZoom*/true);
        }
      }

      nsAutoString handheldFriendly;
      GetHeaderData(nsGkAtoms::handheldFriendly, handheldFriendly);
      if (handheldFriendly.EqualsLiteral("true")) {
        mViewportType = DisplayWidthHeight;
        return nsViewportInfo(aDisplaySize, defaultScale,
                              /*allowZoom*/true);
      }
    }

    nsAutoString minScaleStr;
    GetHeaderData(nsGkAtoms::viewport_minimum_scale, minScaleStr);

    nsresult errorCode;
    mScaleMinFloat = LayoutDeviceToScreenScale(minScaleStr.ToFloat(&errorCode));

    if (NS_FAILED(errorCode)) {
      mScaleMinFloat = kViewportMinScale;
    }

    mScaleMinFloat = mozilla::clamped(
        mScaleMinFloat, kViewportMinScale, kViewportMaxScale);

    nsAutoString maxScaleStr;
    GetHeaderData(nsGkAtoms::viewport_maximum_scale, maxScaleStr);

    // We define a special error code variable for the scale and max scale,
    // because they are used later (see the width calculations).
    nsresult scaleMaxErrorCode;
    mScaleMaxFloat = LayoutDeviceToScreenScale(maxScaleStr.ToFloat(&scaleMaxErrorCode));

    if (NS_FAILED(scaleMaxErrorCode)) {
      mScaleMaxFloat = kViewportMaxScale;
    }

    mScaleMaxFloat = mozilla::clamped(
        mScaleMaxFloat, kViewportMinScale, kViewportMaxScale);

    nsAutoString scaleStr;
    GetHeaderData(nsGkAtoms::viewport_initial_scale, scaleStr);

    nsresult scaleErrorCode;
    mScaleFloat = LayoutDeviceToScreenScale(scaleStr.ToFloat(&scaleErrorCode));

    nsAutoString widthStr, heightStr;

    GetHeaderData(nsGkAtoms::viewport_height, heightStr);
    GetHeaderData(nsGkAtoms::viewport_width, widthStr);

    mAutoSize = false;

    if (widthStr.EqualsLiteral("device-width")) {
      mAutoSize = true;
    }

    if (widthStr.IsEmpty() &&
        (heightStr.EqualsLiteral("device-height") ||
         (mScaleFloat.scale == 1.0)))
    {
      mAutoSize = true;
    }

    nsresult widthErrorCode, heightErrorCode;
    mViewportSize.width = widthStr.ToInteger(&widthErrorCode);
    mViewportSize.height = heightStr.ToInteger(&heightErrorCode);

    // If width or height has not been set to a valid number by this point,
    // fall back to a default value.
    mValidWidth = (!widthStr.IsEmpty() && NS_SUCCEEDED(widthErrorCode) && mViewportSize.width > 0);
    mValidHeight = (!heightStr.IsEmpty() && NS_SUCCEEDED(heightErrorCode) && mViewportSize.height > 0);

    mAllowZoom = true;
    nsAutoString userScalable;
    GetHeaderData(nsGkAtoms::viewport_user_scalable, userScalable);

    if ((userScalable.EqualsLiteral("0")) ||
        (userScalable.EqualsLiteral("no")) ||
        (userScalable.EqualsLiteral("false"))) {
      mAllowZoom = false;
    }

    mScaleStrEmpty = scaleStr.IsEmpty();
    mWidthStrEmpty = widthStr.IsEmpty();
    mValidScaleFloat = !scaleStr.IsEmpty() && NS_SUCCEEDED(scaleErrorCode);
    mValidMaxScale = !maxScaleStr.IsEmpty() && NS_SUCCEEDED(scaleMaxErrorCode);

    mViewportType = Specified;
    mViewportOverflowType = ViewportOverflowType::NoOverflow;
    MOZ_FALLTHROUGH;
  }
  case Specified:
  default:
    LayoutDeviceToScreenScale effectiveMinScale = mScaleMinFloat;
    LayoutDeviceToScreenScale effectiveMaxScale = mScaleMaxFloat;
    bool effectiveValidMaxScale = mValidMaxScale;
    bool effectiveAllowZoom = mAllowZoom;
    if (gfxPrefs::ForceUserScalable()) {
      // If the pref to force user-scalable is enabled, we ignore the values
      // from the meta-viewport tag for these properties and just assume they
      // allow the page to be scalable. Note in particular that this code is
      // in the "Specified" branch of the enclosing switch statement, so that
      // calls to GetViewportInfo always use the latest value of the
      // ForceUserScalable pref. Other codepaths that return nsViewportInfo
      // instances are all consistent with ForceUserScalable() already.
      effectiveMinScale = kViewportMinScale;
      effectiveMaxScale = kViewportMaxScale;
      effectiveValidMaxScale = true;
      effectiveAllowZoom = true;
    }

    CSSSize size = mViewportSize;

    if (!mValidWidth) {
      if (mValidHeight && !aDisplaySize.IsEmpty()) {
        size.width = size.height * aDisplaySize.width / aDisplaySize.height;
      } else {
        // Stretch CSS pixel size of viewport to keep device pixel size
        // unchanged after full zoom applied.
        // See bug 974242.
        size.width = gfxPrefs::DesktopViewportWidth() / fullZoom;
      }
    }

    if (!mValidHeight) {
      if (!aDisplaySize.IsEmpty()) {
        size.height = size.width * aDisplaySize.height / aDisplaySize.width;
      } else {
        size.height = size.width;
      }
    }

    CSSToScreenScale scaleFloat = mScaleFloat * layoutDeviceScale;
    CSSToScreenScale scaleMinFloat = effectiveMinScale * layoutDeviceScale;
    CSSToScreenScale scaleMaxFloat = effectiveMaxScale * layoutDeviceScale;

    if (mAutoSize) {
      // aDisplaySize is in screen pixels; convert them to CSS pixels for the viewport size.
      CSSToScreenScale defaultPixelScale = layoutDeviceScale * LayoutDeviceToScreenScale(1.0f);
      size = ScreenSize(aDisplaySize) / defaultPixelScale;
    }

    size.width = clamped(size.width, float(kViewportMinSize.width), float(kViewportMaxSize.width));

    // Also recalculate the default zoom, if it wasn't specified in the metadata,
    // and the width is specified.
    if (mScaleStrEmpty && !mWidthStrEmpty) {
      CSSToScreenScale defaultScale(float(aDisplaySize.width) / size.width);
      scaleFloat = (scaleFloat > defaultScale) ? scaleFloat : defaultScale;
    }

    size.height = clamped(size.height, float(kViewportMinSize.height), float(kViewportMaxSize.height));

    // We need to perform a conversion, but only if the initial or maximum
    // scale were set explicitly by the user.
    if (mValidScaleFloat && scaleFloat >= scaleMinFloat && scaleFloat <= scaleMaxFloat) {
      CSSSize displaySize = ScreenSize(aDisplaySize) / scaleFloat;
      size.width = std::max(size.width, displaySize.width);
      size.height = std::max(size.height, displaySize.height);
    } else if (effectiveValidMaxScale) {
      CSSSize displaySize = ScreenSize(aDisplaySize) / scaleMaxFloat;
      size.width = std::max(size.width, displaySize.width);
      size.height = std::max(size.height, displaySize.height);
    }

    return nsViewportInfo(scaleFloat, scaleMinFloat, scaleMaxFloat, size,
                          mAutoSize, effectiveAllowZoom);
  }
}

void
nsIDocument::UpdateViewportOverflowType(nscoord aScrolledWidth,
                                        nscoord aScrollportWidth)
{
#ifdef DEBUG
  MOZ_ASSERT(mPresShell);
  nsPresContext* pc = GetPresContext();
  MOZ_ASSERT(pc->GetViewportScrollbarStylesOverride().mHorizontal ==
             NS_STYLE_OVERFLOW_HIDDEN,
             "Should only be called when viewport has overflow-x: hidden");
  MOZ_ASSERT(aScrolledWidth > aScrollportWidth,
             "Should only be called when viewport is overflowed");
  MOZ_ASSERT(IsTopLevelContentDocument(),
             "Should only be called for top-level content document");
#endif // DEBUG

  if (!gfxPrefs::MetaViewportEnabled() ||
      (GetWindow() && GetWindow()->IsDesktopModeViewport())) {
    mViewportOverflowType = ViewportOverflowType::Desktop;
    return;
  }

  if (mViewportType == Unknown) {
    // The viewport info hasn't been initialized yet. Suppose we would
    // get here again at some point after it's initialized.
    return;
  }

  static const LayoutDeviceToScreenScale
    kBlinkDefaultMinScale = LayoutDeviceToScreenScale(0.25f);
  LayoutDeviceToScreenScale minScale;
  if (mViewportType == DisplayWidthHeight) {
    minScale = kBlinkDefaultMinScale;
  } else {
    if (mScaleMinFloat == kViewportMinScale) {
      minScale = kBlinkDefaultMinScale;
    } else {
      minScale = mScaleMinFloat;
    }
  }

  // If the content has overflowed with minimum scale applied, don't
  // change it, otherwise update the overflow type.
  if (mViewportOverflowType != ViewportOverflowType::MinScaleSize) {
    if (aScrolledWidth * minScale.scale < aScrollportWidth) {
      mViewportOverflowType = ViewportOverflowType::ButNotMinScaleSize;
    } else {
      mViewportOverflowType = ViewportOverflowType::MinScaleSize;
    }
  }
}

EventListenerManager*
nsDocument::GetOrCreateListenerManager()
{
  if (!mListenerManager) {
    mListenerManager =
      new EventListenerManager(static_cast<EventTarget*>(this));
    SetFlags(NODE_HAS_LISTENERMANAGER);
  }

  return mListenerManager;
}

EventListenerManager*
nsDocument::GetExistingListenerManager() const
{
  return mListenerManager;
}

void
nsDocument::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  if (mDocGroup && aVisitor.mEvent->mMessage != eVoidEvent &&
      !mIgnoreDocGroupMismatches) {
    mDocGroup->ValidateAccess();
  }

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

already_AddRefed<Event>
nsIDocument::CreateEvent(const nsAString& aEventType, CallerType aCallerType,
                         ErrorResult& rv) const
{
  nsPresContext* presContext = GetPresContext();

  // Create event even without presContext.
  RefPtr<Event> ev =
    EventDispatcher::CreateEvent(const_cast<nsIDocument*>(this), presContext,
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

void
nsIDocument::FlushPendingNotifications(FlushType aType)
{
 mozilla::ChangesToFlush flush(aType, aType >= FlushType::Style);
  FlushPendingNotifications(flush);
}

void
nsIDocument::FlushPendingNotifications(mozilla::ChangesToFlush aFlush)
{
  FlushType flushType = aFlush.mFlushType;

  nsDocumentOnStack dos(this);

  // We need to flush the sink for non-HTML documents (because the XML
  // parser still does insertion with deferred notifications).  We
  // also need to flush the sink if this is a layout-related flush, to
  // make sure that layout is started as needed.  But we can skip that
  // part if we have no presshell or if it's already done an initial
  // reflow.
  if ((!IsHTMLDocument() ||
       (flushType > FlushType::ContentAndNotify && mPresShell &&
        !mPresShell->DidInitialize())) &&
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
  // container is reflowed if its size was changed.  But if it's not safe to
  // flush ourselves, then don't flush the parent, since that can cause things
  // like resizes of our frame's widget, which we can't handle while flushing
  // is unsafe.
  // Since media queries mean that a size change of our container can
  // affect style, we need to promote a style flush on ourself to a
  // layout flush on our parent, since we need our container to be the
  // correct size to determine the correct style.
  if (mParentDocument && IsSafeToFlush()) {
    mozilla::ChangesToFlush parentFlush = aFlush;
    if (flushType >= FlushType::Style) {
      parentFlush.mFlushType = std::max(FlushType::Layout, flushType);
    }
    mParentDocument->FlushPendingNotifications(parentFlush);
  }

  if (nsIPresShell* shell = GetShell()) {
    shell->FlushPendingNotifications(aFlush);
  }
}

static bool
Copy(nsIDocument* aDocument, void* aData)
{
  nsTArray<nsCOMPtr<nsIDocument> >* resources =
    static_cast<nsTArray<nsCOMPtr<nsIDocument> >* >(aData);
  resources->AppendElement(aDocument);
  return true;
}

void
nsIDocument::FlushExternalResources(FlushType aType)
{
  NS_ASSERTION(aType >= FlushType::Style,
    "should only need to flush for style or higher in external resources");
  if (GetDisplayDocument()) {
    return;
  }
  nsTArray<nsCOMPtr<nsIDocument> > resources;
  EnumerateExternalResources(Copy, &resources);

  for (uint32_t i = 0; i < resources.Length(); i++) {
    resources[i]->FlushPendingNotifications(aType);
  }
}

void
nsIDocument::SetXMLDeclaration(const char16_t* aVersion,
                               const char16_t* aEncoding,
                               const int32_t aStandalone)
{
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
  }
  else if (aStandalone == 0) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS;
  }
}

void
nsIDocument::GetXMLDeclaration(nsAString& aVersion,
                               nsAString& aEncoding,
                               nsAString& aStandalone)
{
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

bool
nsIDocument::IsScriptEnabled()
{
  // If this document is sandboxed without 'allow-scripts'
  // script is not enabled
  if (HasScriptsBlockedBySandbox()) {
    return false;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = do_QueryInterface(GetInnerWindow());
  if (!globalObject || !globalObject->GetGlobalJSObject()) {
    return false;
  }

  return xpc::Scriptability::Get(globalObject->GetGlobalJSObject()).Allowed();
}

nsRadioGroupStruct*
nsDocument::GetRadioGroup(const nsAString& aName) const
{
  nsRadioGroupStruct* radioGroup = nullptr;
  mRadioGroups.Get(aName, &radioGroup);
  return radioGroup;
}

nsRadioGroupStruct*
nsDocument::GetOrCreateRadioGroup(const nsAString& aName)
{
  return mRadioGroups.LookupForAdd(aName).OrInsert(
    [] () { return new nsRadioGroupStruct(); });
}

void
nsDocument::SetCurrentRadioButton(const nsAString& aName,
                                  HTMLInputElement* aRadio)
{
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mSelectedRadioButton = aRadio;
}

HTMLInputElement*
nsDocument::GetCurrentRadioButton(const nsAString& aName)
{
  return GetOrCreateRadioGroup(aName)->mSelectedRadioButton;
}

NS_IMETHODIMP
nsDocument::GetNextRadioButton(const nsAString& aName,
                               const bool aPrevious,
                               HTMLInputElement* aFocusedRadio,
                               HTMLInputElement** aRadioOut)
{
  // XXX Can we combine the HTML radio button method impls of
  //     nsDocument and nsHTMLFormControl?
  // XXX Why is HTML radio button stuff in nsDocument, as
  //     opposed to nsHTMLDocument?
  *aRadioOut = nullptr;

  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  // Return the radio button relative to the focused radio button.
  // If no radio is focused, get the radio relative to the selected one.
  RefPtr<HTMLInputElement> currentRadio;
  if (aFocusedRadio) {
    currentRadio = aFocusedRadio;
  }
  else {
    currentRadio = radioGroup->mSelectedRadioButton;
    if (!currentRadio) {
      return NS_ERROR_FAILURE;
    }
  }
  int32_t index = radioGroup->mRadioButtons.IndexOf(currentRadio);
  if (index < 0) {
    return NS_ERROR_FAILURE;
  }

  int32_t numRadios = radioGroup->mRadioButtons.Count();
  RefPtr<HTMLInputElement> radio;
  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios -1;
      }
    }
    else if (++index >= numRadios) {
      index = 0;
    }
    NS_ASSERTION(static_cast<nsGenericHTMLFormElement*>(radioGroup->mRadioButtons[index])->IsHTMLElement(nsGkAtoms::input),
                 "mRadioButtons holding a non-radio button");
    radio = static_cast<HTMLInputElement*>(radioGroup->mRadioButtons[index]);
  } while (radio->Disabled() && radio != currentRadio);

  radio.forget(aRadioOut);
  return NS_OK;
}

void
nsDocument::AddToRadioGroup(const nsAString& aName,
                            HTMLInputElement* aRadio)
{
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mRadioButtons.AppendObject(aRadio);

  if (aRadio->IsRequired()) {
    radioGroup->mRequiredRadioCount++;
  }
}

void
nsDocument::RemoveFromRadioGroup(const nsAString& aName,
                                 HTMLInputElement* aRadio)
{
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mRadioButtons.RemoveObject(aRadio);

  if (aRadio->IsRequired()) {
    NS_ASSERTION(radioGroup->mRequiredRadioCount != 0,
                 "mRequiredRadioCount about to wrap below 0!");
    radioGroup->mRequiredRadioCount--;
  }
}

NS_IMETHODIMP
nsDocument::WalkRadioGroup(const nsAString& aName,
                           nsIRadioVisitor* aVisitor,
                           bool aFlushContent)
{
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  for (int i = 0; i < radioGroup->mRadioButtons.Count(); i++) {
    if (!aVisitor->Visit(radioGroup->mRadioButtons[i])) {
      return NS_OK;
    }
  }

  return NS_OK;
}

uint32_t
nsDocument::GetRequiredRadioCount(const nsAString& aName) const
{
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup ? radioGroup->mRequiredRadioCount : 0;
}

void
nsDocument::RadioRequiredWillChange(const nsAString& aName, bool aRequiredAdded)
{
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  if (aRequiredAdded) {
    radioGroup->mRequiredRadioCount++;
  } else {
    NS_ASSERTION(radioGroup->mRequiredRadioCount != 0,
                 "mRequiredRadioCount about to wrap below 0!");
    radioGroup->mRequiredRadioCount--;
  }
}

bool
nsDocument::GetValueMissingState(const nsAString& aName) const
{
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup && radioGroup->mGroupSuffersFromValueMissing;
}

void
nsDocument::SetValueMissingState(const nsAString& aName, bool aValue)
{
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mGroupSuffersFromValueMissing = aValue;
}

void
nsDocument::RetrieveRelevantHeaders(nsIChannel *aChannel)
{
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

    // The misspelled key 'referer' is as per the HTTP spec
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("referer"),
                                       mReferrer);

    static const char *const headers[] = {
      "default-style",
      "content-style-type",
      "content-language",
      "content-disposition",
      "refresh",
      "x-dns-prefetch-control",
      "x-frame-options",
      "referrer-policy",
      // add more http headers if you need
      // XXXbz don't add content-location support without reading bug
      // 238654 and its dependencies/dups first.
      0
    };

    nsAutoCString headerVal;
    const char *const *name = headers;
    while (*name) {
      rv =
        httpChannel->GetResponseHeader(nsDependentCString(*name), headerVal);
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

already_AddRefed<Element>
nsIDocument::CreateElem(const nsAString& aName, nsAtom *aPrefix,
                        int32_t aNamespaceID, const nsAString* aIs)
{
#ifdef DEBUG
  nsAutoString qName;
  if (aPrefix) {
    aPrefix->ToString(qName);
    qName.Append(':');
  }
  qName.Append(aName);

  // Note: "a:b:c" is a valid name in non-namespaces XML, and
  // nsIDocument::CreateElement can call us with such a name and no prefix,
  // which would cause an error if we just used true here.
  bool nsAware = aPrefix != nullptr || aNamespaceID != GetDefaultNamespaceID();
  NS_ASSERTION(NS_SUCCEEDED(nsContentUtils::CheckQName(qName, nsAware)),
               "Don't pass invalid prefixes to nsDocument::CreateElem, "
               "check caller.");
#endif

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  mNodeInfoManager->GetNodeInfo(aName, aPrefix, aNamespaceID,
                                ELEMENT_NODE, getter_AddRefs(nodeInfo));
  NS_ENSURE_TRUE(nodeInfo, nullptr);

  nsCOMPtr<Element> element;
  nsresult rv = NS_NewElement(getter_AddRefs(element), nodeInfo.forget(),
                              NOT_FROM_PARSER, aIs);
  return NS_SUCCEEDED(rv) ? element.forget() : nullptr;
}

bool
nsIDocument::IsSafeToFlush() const
{
  nsIPresShell* shell = GetShell();
  if (!shell)
    return true;

  return shell->IsSafeToFlush();
}

void
nsIDocument::Sanitize()
{
  // Sanitize the document by resetting all password fields and any form
  // fields with autocomplete=off to their default values.  We do this now,
  // instead of when the presentation is restored, to offer some protection
  // in case there is ever an exploit that allows a cached document to be
  // accessed from a different document.

  // First locate all input elements, regardless of whether they are
  // in a form, and reset the password and autocomplete=off elements.

  RefPtr<nsContentList> nodes = GetElementsByTagName(NS_LITERAL_STRING("input"));

  nsAutoString value;

  uint32_t length = nodes->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    NS_ASSERTION(nodes->Item(i), "null item in node list!");

    RefPtr<HTMLInputElement> input = HTMLInputElement::FromNodeOrNull(nodes->Item(i));
    if (!input)
      continue;

    bool resetValue = false;

    input->GetAttribute(NS_LITERAL_STRING("autocomplete"), value);
    if (value.LowerCaseEqualsLiteral("off")) {
      resetValue = true;
    } else {
      input->GetType(value);
      if (value.LowerCaseEqualsLiteral("password"))
        resetValue = true;
    }

    if (resetValue) {
      input->Reset();
    }
  }

  // Now locate all _form_ elements that have autocomplete=off and reset them
  nodes = GetElementsByTagName(NS_LITERAL_STRING("form"));

  length = nodes->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    NS_ASSERTION(nodes->Item(i), "null item in nodelist");

    HTMLFormElement* form = HTMLFormElement::FromNode(nodes->Item(i));
    if (!form)
      continue;

    form->GetAttr(kNameSpaceID_None, nsGkAtoms::autocomplete, value);
    if (value.LowerCaseEqualsLiteral("off"))
      form->Reset();
  }
}

void
nsIDocument::EnumerateSubDocuments(nsSubDocEnumFunc aCallback, void *aData)
{
  if (!mSubDocuments) {
    return;
  }

  // PLDHashTable::Iterator can't handle modifications while iterating so we
  // copy all entries to an array first before calling any callbacks.
  AutoTArray<nsCOMPtr<nsIDocument>, 8> subdocs;
  for (auto iter = mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<SubDocMapEntry*>(iter.Get());
    nsIDocument* subdoc = entry->mSubDocument;
    if (subdoc) {
      subdocs.AppendElement(subdoc);
    }
  }
  for (auto subdoc : subdocs) {
    if (!aCallback(subdoc, aData)) {
      break;
    }
  }
}

void
nsIDocument::CollectDescendantDocuments(
  nsTArray<nsCOMPtr<nsIDocument>>& aDescendants,
  nsDocTestFunc aCallback) const
{
  if (!mSubDocuments) {
    return;
  }

  for (auto iter = mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<SubDocMapEntry*>(iter.Get());
    const nsIDocument* subdoc = entry->mSubDocument;
    if (subdoc) {
      if (aCallback(subdoc)) {
        aDescendants.AppendElement(entry->mSubDocument);
      }
      subdoc->CollectDescendantDocuments(aDescendants, aCallback);
    }
  }
}

#ifdef DEBUG_bryner
#define DEBUG_PAGE_CACHE
#endif

bool
nsIDocument::CanSavePresentation(nsIRequest *aNewRequest)
{
  if (!IsBFCachingAllowed()) {
    return false;
  }

  if (EventHandlingSuppressed()) {
    return false;
  }

  // Do not allow suspended windows to be placed in the
  // bfcache.  This method is also used to verify a document
  // coming out of the bfcache is ok to restore, though.  So
  // we only want to block suspend windows that aren't also
  // frozen.
  nsPIDOMWindowInner* win = GetInnerWindow();
  if (win && win->IsSuspended() && !win->IsFrozen()) {
    return false;
  }

  // Check our event listener manager for unload/beforeunload listeners.
  nsCOMPtr<EventTarget> piTarget = do_QueryInterface(mScriptGlobalObject);
  if (piTarget) {
    EventListenerManager* manager = piTarget->GetExistingListenerManager();
    if (manager && manager->HasUnloadListeners()) {
      return false;
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
          nsCOMPtr<nsILoadInfo> li;
          channel->GetLoadInfo(getter_AddRefs(li));
          if (li) {
            if (li->InternalContentPolicyType() == nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON) {
              continue;
            }
          }
        }
#ifdef DEBUG_PAGE_CACHE
        nsAutoCString requestName, docSpec;
        request->GetName(requestName);
        if (mDocumentURI)
          mDocumentURI->GetSpec(docSpec);

        printf("document %s has request %s\n",
               docSpec.get(), requestName.get());
#endif
        return false;
      }
    }
  }

  // Check if we have active GetUserMedia use
  if (MediaManager::Exists() && win &&
      MediaManager::Get()->IsWindowStillActive(win->WindowID())) {
    return false;
  }

#ifdef MOZ_WEBRTC
  // Check if we have active PeerConnections
  if (win && win->HasActivePeerConnections()) {
    return false;
  }
#endif // MOZ_WEBRTC

  // Don't save presentations for documents containing EME content, so that
  // CDMs reliably shutdown upon user navigation.
  if (ContainsEMEContent()) {
    return false;
  }

  // Don't save presentations for documents containing MSE content, to
  // reduce memory usage.
  if (ContainsMSEContent()) {
    return false;
  }

  if (mSubDocuments) {
    for (auto iter = mSubDocuments->Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<SubDocMapEntry*>(iter.Get());
      nsIDocument* subdoc = entry->mSubDocument;

      // The aIgnoreRequest we were passed is only for us, so don't pass it on.
      bool canCache = subdoc ? subdoc->CanSavePresentation(nullptr) : false;
      if (!canCache) {
        return false;
      }
    }
  }


  if (win) {
    auto* globalWindow = nsGlobalWindowInner::Cast(win);
#ifdef MOZ_WEBSPEECH
    if (globalWindow->HasActiveSpeechSynthesis()) {
      return false;
    }
#endif
    if (globalWindow->HasUsedVR()) {
      return false;
    }
  }

  return true;
}

void
nsDocument::Destroy()
{
  // The ContentViewer wants to release the document now.  So, tell our content
  // to drop any references to the document so that it can be destroyed.
  if (mIsGoingAway)
    return;

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

  mInUnlinkOrDeletion = oldVal;

  mLayoutHistoryState = nullptr;

  // Shut down our external resource map.  We might not need this for
  // leak-fixing if we fix nsDocumentViewer to do cycle-collection, but
  // tearing down all those frame trees right now is the right thing to do.
  mExternalResourceMap.Shutdown();
}

void
nsDocument::RemovedFromDocShell()
{
  if (mRemovedFromDocShell)
    return;

  mRemovedFromDocShell = true;
  EnumerateActivityObservers(NotifyActivityChanged, nullptr);

  for (nsIContent* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    child->SaveSubtreeState();
  }
}

already_AddRefed<nsILayoutHistoryState>
nsIDocument::GetLayoutHistoryState() const
{
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

void
nsIDocument::EnsureOnloadBlocker()
{
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

void
nsDocument::AsyncBlockOnload()
{
  while (mAsyncOnloadBlockCount) {
    --mAsyncOnloadBlockCount;
    BlockOnload();
  }
}

void
nsDocument::BlockOnload()
{
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
          "nsDocument::AsyncBlockOnload", this, &nsDocument::AsyncBlockOnload));
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

void
nsDocument::UnblockOnload(bool aFireSync)
{
  if (mDisplayDocument) {
    mDisplayDocument->UnblockOnload(aFireSync);
    return;
  }

  if (mOnloadBlockCount == 0 && mAsyncOnloadBlockCount == 0) {
    MOZ_ASSERT_UNREACHABLE("More UnblockOnload() calls than BlockOnload() "
                           "calls; dropping call");
    return;
  }

  --mOnloadBlockCount;

  if (mOnloadBlockCount == 0) {
    if (mScriptGlobalObject) {
      // Only manipulate the loadgroup in this case, because if mScriptGlobalObject
      // is null, it's not ours.
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
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(this,
                                 NS_LITERAL_STRING("MozSVGAsImageDocumentLoad"),
                                 CanBubble::eNo,
                                 ChromeOnlyDispatch::eNo);
      asyncDispatcher->PostDOMEvent();
    }
  }
}

class nsUnblockOnloadEvent : public Runnable {
public:
  explicit nsUnblockOnloadEvent(nsIDocument* aDoc)
    : mozilla::Runnable("nsUnblockOnloadEvent")
    , mDoc(aDoc)
  {
  }
  NS_IMETHOD Run() override {
    mDoc->DoUnblockOnload();
    return NS_OK;
  }
private:
  RefPtr<nsIDocument> mDoc;
};

void
nsIDocument::PostUnblockOnloadEvent()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> evt = new nsUnblockOnloadEvent(this);
  nsresult rv =
    Dispatch(TaskCategory::Other, evt.forget());
  if (NS_SUCCEEDED(rv)) {
    // Stabilize block count so we don't post more events while this one is up
    ++mOnloadBlockCount;
  } else {
    NS_WARNING("failed to dispatch nsUnblockOnloadEvent");
  }
}

void
nsIDocument::DoUnblockOnload()
{
  MOZ_ASSERT(!mDisplayDocument,
                  "Shouldn't get here for resource document");
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

nsIContent*
nsIDocument::GetContentInThisDocument(nsIFrame* aFrame) const
{
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    nsIContent* content = f->GetContent();
    if (!content || content->IsInAnonymousSubtree())
      continue;

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

void
nsIDocument::DispatchPageTransition(EventTarget* aDispatchTarget,
                                    const nsAString& aType,
                                    bool aPersisted)
{
  if (!aDispatchTarget) {
    return;
  }

  PageTransitionEventInit init;
  init.mBubbles = true;
  init.mCancelable = true;
  init.mPersisted = aPersisted;

  nsDocShell* docShell = mDocumentContainer.get();
  init.mInFrameSwap = docShell && docShell->InFrameSwap();

  RefPtr<PageTransitionEvent> event =
    PageTransitionEvent::Constructor(this, aType, init);

  event->SetTrusted(true);
  event->SetTarget(this);
  EventDispatcher::DispatchDOMEvent(aDispatchTarget, nullptr, event,
                                    nullptr, nullptr);
}

static bool
NotifyPageShow(nsIDocument* aDocument, void* aData)
{
  const bool* aPersistedPtr = static_cast<const bool*>(aData);
  aDocument->OnPageShow(*aPersistedPtr, nullptr);
  return true;
}

void
nsIDocument::OnPageShow(bool aPersisted, EventTarget* aDispatchStartTarget)
{
  mVisible = true;

  EnumerateActivityObservers(NotifyActivityChanged, nullptr);
  EnumerateExternalResources(NotifyPageShow, &aPersisted);

  Element* root = GetRootElement();
  if (aPersisted && root) {
    // Send out notifications that our <link> elements are attached.
    RefPtr<nsContentList> links = NS_GetContentList(root,
                                                      kNameSpaceID_XHTML,
                                                      NS_LITERAL_STRING("link"));

    uint32_t linkCount = links->Length(true);
    for (uint32_t i = 0; i < linkCount; ++i) {
      static_cast<HTMLLinkElement*>(links->Item(i, false))->LinkAdded();
    }
  }

  // See nsIDocument
  if (!aDispatchStartTarget) {
    // Set mIsShowing before firing events, in case those event handlers
    // move us around.
    mIsShowing = true;
  }

  if (mAnimationController) {
    mAnimationController->OnPageShow();
  }

  if (aPersisted) {
    ImageTracker()->SetAnimatingState(true);
  }

  UpdateVisibilityState();

  if (!mIsBeingUsedAsImage) {
    // Dispatch observer notification to notify observers page is shown.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      nsIPrincipal* principal = NodePrincipal();
      os->NotifyObservers(this,
                          nsContentUtils::IsSystemPrincipal(principal) ?
                            "chrome-page-shown" :
                            "content-page-shown",
                          nullptr);
    }

    nsCOMPtr<EventTarget> target = aDispatchStartTarget;
    if (!target) {
      target = do_QueryInterface(GetWindow());
    }
    DispatchPageTransition(target, NS_LITERAL_STRING("pageshow"), aPersisted);
  }
}

static bool
NotifyPageHide(nsIDocument* aDocument, void* aData)
{
  const bool* aPersistedPtr = static_cast<const bool*>(aData);
  aDocument->OnPageHide(*aPersistedPtr, nullptr);
  return true;
}

static void
DispatchCustomEventWithFlush(nsINode* aTarget, const nsAString& aEventType,
                             bool aBubbles, bool aOnlyChromeDispatch)
{
  RefPtr<Event> event = NS_NewDOMEvent(aTarget, nullptr, nullptr);
  event->InitEvent(aEventType, aBubbles, false);
  event->SetTrusted(true);
  if (aOnlyChromeDispatch) {
    event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  }
  if (nsPresContext* presContext = aTarget->OwnerDoc()->GetPresContext()) {
    presContext->RefreshDriver()->ScheduleEventDispatch(aTarget, event);
  }
}

static void
DispatchFullScreenChange(nsIDocument* aTarget)
{
  DispatchCustomEventWithFlush(
    aTarget, NS_LITERAL_STRING("fullscreenchange"),
    /* Bubbles */ true, /* OnlyChrome */ false);
}

static void ClearPendingFullscreenRequests(nsIDocument* aDoc);

static bool
HasHttpScheme(nsIURI* aURI)
{
  bool isHttpish = false;
  return aURI &&
         ((NS_SUCCEEDED(aURI->SchemeIs("http", &isHttpish)) && isHttpish) ||
          (NS_SUCCEEDED(aURI->SchemeIs("https", &isHttpish)) && isHttpish));
}

void
nsIDocument::OnPageHide(bool aPersisted, EventTarget* aDispatchStartTarget)
{
  if (IsTopLevelContentDocument() && GetDocGroup() &&
      Telemetry::CanRecordExtended()) {
    TabGroup* tabGroup = mDocGroup->GetTabGroup();

    if (tabGroup) {
      uint32_t active = tabGroup->Count(true /* aActiveOnly */);
      uint32_t total = tabGroup->Count();

      if (HasHttpScheme(GetDocumentURI())) {
        Telemetry::Accumulate(Telemetry::ACTIVE_HTTP_DOCGROUPS_PER_TABGROUP,
                              active);
        Telemetry::Accumulate(Telemetry::TOTAL_HTTP_DOCGROUPS_PER_TABGROUP,
                              total);
      }
    }
  }

  // Send out notifications that our <link> elements are detached,
  // but only if this is not a full unload.
  Element* root = GetRootElement();
  if (aPersisted && root) {
    RefPtr<nsContentList> links = NS_GetContentList(root,
                                                    kNameSpaceID_XHTML,
                                                    NS_LITERAL_STRING("link"));

    uint32_t linkCount = links->Length(true);
    for (uint32_t i = 0; i < linkCount; ++i) {
      static_cast<HTMLLinkElement*>(links->Item(i, false))->LinkRemoved();
    }
  }

  // See nsIDocument
  if (!aDispatchStartTarget) {
    // Set mIsShowing before firing events, in case those event handlers
    // move us around.
    mIsShowing = false;
  }

  if (mAnimationController) {
    mAnimationController->OnPageHide();
  }

  // We do not stop the animations (bug 1024343)
  // when the page is refreshing while being dragged out
  nsDocShell* docShell = mDocumentContainer.get();
  if (aPersisted && !(docShell && docShell->InFrameSwap())) {
    ImageTracker()->SetAnimatingState(false);
  }

  ExitPointerLock();

  if (!mIsBeingUsedAsImage) {
    // Dispatch observer notification to notify observers page is hidden.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      nsIPrincipal* principal = NodePrincipal();
      os->NotifyObservers(this,
                          nsContentUtils::IsSystemPrincipal(principal) ?
                            "chrome-page-hidden" :
                            "content-page-hidden",
                          nullptr);
    }

    // Now send out a PageHide event.
    nsCOMPtr<EventTarget> target = aDispatchStartTarget;
    if (!target) {
      target = do_QueryInterface(GetWindow());
    }
    {
      PageUnloadingEventTimeStamp timeStamp(this);
      DispatchPageTransition(target, NS_LITERAL_STRING("pagehide"), aPersisted);
    }
  }

  mVisible = false;

  UpdateVisibilityState();

  EnumerateExternalResources(NotifyPageHide, &aPersisted);
  EnumerateActivityObservers(NotifyActivityChanged, nullptr);

  ClearPendingFullscreenRequests(this);
  if (FullScreenStackTop()) {
    // If this document was fullscreen, we should exit fullscreen in this
    // doctree branch. This ensures that if the user navigates while in
    // fullscreen mode we don't leave its still visible ancestor documents
    // in fullscreen mode. So exit fullscreen in the document's fullscreen
    // root document, as this will exit fullscreen in all the root's
    // descendant documents. Note that documents are removed from the
    // doctree by the time OnPageHide() is called, so we must store a
    // reference to the root (in nsDocument::mFullscreenRoot) since we can't
    // just traverse the doctree to get the root.
    nsIDocument::ExitFullscreenInDocTree(this);

    // Since the document is removed from the doctree before OnPageHide() is
    // called, ExitFullscreen() can't traverse from the root down to *this*
    // document, so we must manually call CleanupFullscreenState() below too.
    // Note that CleanupFullscreenState() clears nsDocument::mFullscreenRoot,
    // so we *must* call it after ExitFullscreen(), not before.
    // OnPageHide() is called in every hidden (i.e. descendant) document,
    // so calling CleanupFullscreenState() here will ensure all hidden
    // documents have their fullscreen state reset.
    CleanupFullscreenState();

    // If anyone was listening to this document's state, advertizing the state
    // change would be the least of the politeness.
    DispatchFullScreenChange(this);
  }
}

void
nsIDocument::WillDispatchMutationEvent(nsINode* aTarget)
{
  NS_ASSERTION(mSubtreeModifiedDepth != 0 ||
               mSubtreeModifiedTargets.Count() == 0,
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

void
nsIDocument::MutationEventDispatched(nsINode* aTarget)
{
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
        commonAncestor =
          nsContentUtils::GetCommonAncestor(possibleTarget, realTargets[j]);
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
      (new AsyncEventDispatcher(realTargets[k], mutation))->
        RunDOMEventWhenSafe();
    }
  }
}

void
nsIDocument::DestroyElementMaps()
{
#ifdef DEBUG
  mStyledLinksCleared = true;
#endif
  mStyledLinks.Clear();
  mIdentifierMap.Clear();
  mComposedShadowRoots.Clear();
  mResponsiveContent.Clear();
  IncrementExpandoGeneration(*this);
}

void
nsIDocument::RefreshLinkHrefs()
{
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

nsresult
nsDocument::CloneDocHelper(nsDocument* clone, bool aPreallocateChildren) const
{
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
      uri = nsIDocument::GetDocumentURI();
    }
    clone->mChannel = channel;
    if (uri) {
      clone->ResetToURI(uri, loadGroup, NodePrincipal());
    }

    clone->SetContainer(mDocumentContainer);
  }

  // Now ensure that our clone has the same URI, base URI, and principal as us.
  // We do this after the mCreatingStaticClone block above, because that block
  // can set the base URI to an incorrect value in cases when base URI
  // information came from the channel.  So we override explicitly, and do it
  // for all these properties, in case ResetToURI messes with any of the rest of
  // them.
  clone->nsDocument::SetDocumentURI(nsIDocument::GetDocumentURI());
  clone->SetChromeXHRDocURI(mChromeXHRDocURI);
  clone->SetPrincipal(NodePrincipal());
  clone->mDocumentBaseURI = mDocumentBaseURI;
  clone->SetChromeXHRDocBaseURI(mChromeXHRDocBaseURI);

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

  // State from nsIDocument
  clone->mCharacterSet = mCharacterSet;
  clone->mCharacterSetSource = mCharacterSetSource;
  clone->mCompatMode = mCompatMode;
  clone->mBidiOptions = mBidiOptions;
  clone->mContentLanguage = mContentLanguage;
  clone->SetContentTypeInternal(GetContentTypeInternal());
  clone->mSecurityInfo = mSecurityInfo;

  // State from nsDocument
  clone->mType = mType;
  clone->mXMLDeclarationBits = mXMLDeclarationBits;
  clone->mBaseTarget = mBaseTarget;

  // Preallocate attributes and child arrays
  rv = clone->mChildren.EnsureCapacityToClone(mChildren, aPreallocateChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsIDocument::SetReadyStateInternal(ReadyState rs)
{
  mReadyState = rs;
  if (rs == READYSTATE_UNINITIALIZED) {
    // Transition back to uninitialized happens only to keep assertions happy
    // right before readyState transitions to something else. Make this
    // transition undetectable by Web content.
    return;
  }
  if (mTiming) {
    switch (rs) {
      case READYSTATE_LOADING:
        mTiming->NotifyDOMLoading(nsIDocument::GetDocumentURI());
        break;
      case READYSTATE_INTERACTIVE:
        mTiming->NotifyDOMInteractive(nsIDocument::GetDocumentURI());
        break;
      case READYSTATE_COMPLETE:
        mTiming->NotifyDOMComplete(nsIDocument::GetDocumentURI());
        break;
      default:
        NS_WARNING("Unexpected ReadyState value");
        break;
    }
  }
  // At the time of loading start, we don't have timing object, record time.
  if (READYSTATE_LOADING == rs) {
    mLoadingTimeStamp = mozilla::TimeStamp::Now();
  }

  RecordNavigationTiming(rs);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this,
                             NS_LITERAL_STRING("readystatechange"),
                             CanBubble::eNo,
                             ChromeOnlyDispatch::eNo);
  asyncDispatcher->RunDOMEventWhenSafe();
}

void
nsIDocument::GetReadyState(nsAString& aReadyState) const
{
  switch(mReadyState) {
  case READYSTATE_LOADING :
    aReadyState.AssignLiteral(u"loading");
    break;
  case READYSTATE_INTERACTIVE :
    aReadyState.AssignLiteral(u"interactive");
    break;
  case READYSTATE_COMPLETE :
    aReadyState.AssignLiteral(u"complete");
    break;
  default:
    aReadyState.AssignLiteral(u"uninitialized");
  }
}

static bool
SuppressEventHandlingInDocument(nsIDocument* aDocument, void* aData)
{
  aDocument->SuppressEventHandling(*static_cast<uint32_t*>(aData));

  return true;
}

void
nsIDocument::SuppressEventHandling(uint32_t aIncrease)
{
  mEventsSuppressed += aIncrease;
  UpdateFrameRequestCallbackSchedulingState();
  for (uint32_t i = 0; i < aIncrease; ++i) {
    ScriptLoader()->AddExecuteBlocker();
  }

  EnumerateSubDocuments(SuppressEventHandlingInDocument, &aIncrease);
}

static void
FireOrClearDelayedEvents(nsTArray<nsCOMPtr<nsIDocument>>& aDocuments,
                         bool aFireEvents)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return;

  for (uint32_t i = 0; i < aDocuments.Length(); ++i) {
    // NB: Don't bother trying to fire delayed events on documents that were
    // closed before this event ran.
    if (!aDocuments[i]->EventHandlingSuppressed()) {
      fm->FireDelayedEvents(aDocuments[i]);
      nsCOMPtr<nsIPresShell> shell = aDocuments[i]->GetShell();
      if (shell) {
        // Only fire events for active documents.
        bool fire = aFireEvents &&
                    aDocuments[i]->GetInnerWindow() &&
                    aDocuments[i]->GetInnerWindow()->IsCurrentInnerWindow();
        shell->FireOrClearDelayedEvents(fire);
      }
    }
  }
}

void
nsIDocument::PreloadPictureClosed()
{
  MOZ_ASSERT(mPreloadPictureDepth > 0);
  mPreloadPictureDepth--;
  if (mPreloadPictureDepth == 0) {
    mPreloadPictureFoundSource.SetIsVoid(true);
  }
}

void
nsIDocument::PreloadPictureImageSource(const nsAString& aSrcsetAttr,
                                       const nsAString& aSizesAttr,
                                       const nsAString& aTypeAttr,
                                       const nsAString& aMediaAttr)
{
  // Nested pictures are not valid syntax, so while we'll eventually load them,
  // it's not worth tracking sources mixed between nesting levels to preload
  // them effectively.
  if (mPreloadPictureDepth == 1 && mPreloadPictureFoundSource.IsVoid()) {
    // <picture> selects the first matching source, so if this returns a URI we
    // needn't consider new sources until a new <picture> is encountered.
    bool found =
      HTMLImageElement::SelectSourceForTagWithAttrs(this, true, VoidString(),
                                                    aSrcsetAttr, aSizesAttr,
                                                    aTypeAttr, aMediaAttr,
                                                    mPreloadPictureFoundSource);
    if (found && mPreloadPictureFoundSource.IsVoid()) {
      // Found an empty source, which counts
      mPreloadPictureFoundSource.SetIsVoid(false);
    }
  }
}

already_AddRefed<nsIURI>
nsIDocument::ResolvePreloadImage(nsIURI *aBaseURI,
                                 const nsAString& aSrcAttr,
                                 const nsAString& aSrcsetAttr,
                                 const nsAString& aSizesAttr,
                                 bool *aIsImgSet)
{
  nsString sourceURL;
  bool isImgSet;
  if (mPreloadPictureDepth == 1 && !mPreloadPictureFoundSource.IsVoid()) {
    // We're in a <picture> element and found a URI from a source previous to
    // this image, use it.
    sourceURL = mPreloadPictureFoundSource;
    isImgSet = true;
  } else {
    // Otherwise try to use this <img> as a source
    HTMLImageElement::SelectSourceForTagWithAttrs(this, false, aSrcAttr,
                                                  aSrcsetAttr, aSizesAttr,
                                                  VoidString(), VoidString(),
                                                  sourceURL);
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

void
nsIDocument::MaybePreLoadImage(nsIURI* uri,
                               const nsAString &aCrossOriginAttr,
                               enum mozilla::net::ReferrerPolicy aReferrerPolicy,
                               bool aIsImgSet)
{
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
    aIsImgSet ? nsIContentPolicy::TYPE_IMAGESET :
                nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD;

  // Image not in cache - trigger preload
  RefPtr<imgRequestProxy> request;
  nsresult rv =
    nsContentUtils::LoadImage(uri,
                              static_cast<nsINode*>(this),
                              this,
                              NodePrincipal(),
                              0,
                              mDocumentURI, // uri of document used as referrer
                              aReferrerPolicy,
                              nullptr,       // no observer
                              loadFlags,
                              NS_LITERAL_STRING("img"),
                              getter_AddRefs(request),
                              policyType);

  // Pin image-reference to avoid evicting it from the img-cache before
  // the "real" load occurs. Unpinned in DispatchContentLoadedEvents and
  // unlink
  if (NS_SUCCEEDED(rv)) {
    mPreloadingImages.Put(uri, request.forget());
  }
}

void
nsIDocument::MaybePreconnect(nsIURI* aOrigURI, mozilla::CORSMode aCORSMode)
{
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
    return; // we found an existing entry
  }
  entry.OrInsert([] () { return true; });

  nsCOMPtr<nsISpeculativeConnect>
    speculator(do_QueryInterface(nsContentUtils::GetIOService()));
  if (!speculator) {
    return;
  }

  if (aCORSMode == CORS_ANONYMOUS) {
    speculator->SpeculativeAnonymousConnect2(uri, NodePrincipal(), nullptr);
  } else {
    speculator->SpeculativeConnect2(uri, NodePrincipal(), nullptr);
  }
}

void
nsIDocument::ForgetImagePreload(nsIURI* aURI)
{
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

void
nsIDocument::UpdateDocumentStates(EventStates aChangedStates)
{
  if (aChangedStates.HasState(NS_DOCUMENT_STATE_RTL_LOCALE)) {
    if (IsDocumentRightToLeft()) {
      mDocumentState |= NS_DOCUMENT_STATE_RTL_LOCALE;
    } else {
      mDocumentState &= ~NS_DOCUMENT_STATE_RTL_LOCALE;
    }
  }

  if (aChangedStates.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    if (IsTopLevelWindowInactive()) {
      mDocumentState |= NS_DOCUMENT_STATE_WINDOW_INACTIVE;
    } else {
      mDocumentState &= ~NS_DOCUMENT_STATE_WINDOW_INACTIVE;
    }
  }
}

namespace {

/**
 * Stub for LoadSheet(), since all we want is to get the sheet into
 * the CSSLoader's style cache
 */
class StubCSSLoaderObserver final : public nsICSSLoaderObserver {
  ~StubCSSLoaderObserver() {}
public:
  NS_IMETHOD
  StyleSheetLoaded(StyleSheet*, bool, nsresult) override
  {
    return NS_OK;
  }
  NS_DECL_ISUPPORTS
};
NS_IMPL_ISUPPORTS(StubCSSLoaderObserver, nsICSSLoaderObserver)

} // namespace

void
nsIDocument::PreloadStyle(nsIURI* uri,
                          const Encoding* aEncoding,
                          const nsAString& aCrossOriginAttr,
                          const enum mozilla::net::ReferrerPolicy aReferrerPolicy,
                          const nsAString& aIntegrity)
{
  // The CSSLoader will retain this object after we return.
  nsCOMPtr<nsICSSLoaderObserver> obs = new StubCSSLoaderObserver();

  // Charset names are always ASCII.
  CSSLoader()->LoadSheet(uri,
                         true,
                         NodePrincipal(),
                         aEncoding,
                         obs,
                         Element::StringToCORSMode(aCrossOriginAttr),
                         aReferrerPolicy,
                         aIntegrity);
}

nsresult
nsIDocument::LoadChromeSheetSync(nsIURI* uri, bool isAgentSheet,
                                RefPtr<mozilla::StyleSheet>* aSheet)
{
  css::SheetParsingMode mode =
    isAgentSheet ? css::eAgentSheetFeatures
                 : css::eAuthorSheetFeatures;
  return CSSLoader()->LoadSheetSync(uri, mode, isAgentSheet, aSheet);
}

class nsDelayedEventDispatcher : public Runnable
{
public:
  explicit nsDelayedEventDispatcher(nsTArray<nsCOMPtr<nsIDocument>>& aDocuments)
    : mozilla::Runnable("nsDelayedEventDispatcher")
  {
    mDocuments.SwapElements(aDocuments);
  }
  virtual ~nsDelayedEventDispatcher() {}

  NS_IMETHOD Run() override
  {
    FireOrClearDelayedEvents(mDocuments, true);
    return NS_OK;
  }

private:
  nsTArray<nsCOMPtr<nsIDocument>> mDocuments;
};

static bool
GetAndUnsuppressSubDocuments(nsIDocument* aDocument, void* aData)
{
  if (aDocument->EventHandlingSuppressed() > 0) {
    aDocument->DecreaseEventSuppression();
    aDocument->ScriptLoader()->RemoveExecuteBlocker();
  }

  auto* docs = static_cast<nsTArray<nsCOMPtr<nsIDocument>>*>(aData);

  docs->AppendElement(aDocument);
  aDocument->EnumerateSubDocuments(GetAndUnsuppressSubDocuments, aData);
  return true;
}

void
nsIDocument::UnsuppressEventHandlingAndFireEvents(bool aFireEvents)
{
  nsTArray<nsCOMPtr<nsIDocument>> documents;
  GetAndUnsuppressSubDocuments(this, &documents);

  if (aFireEvents) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIRunnable> ded = new nsDelayedEventDispatcher(documents);
    Dispatch(TaskCategory::Other, ded.forget());
  } else {
    FireOrClearDelayedEvents(documents, false);
  }
}

nsISupports*
nsIDocument::GetCurrentContentSink()
{
  return mParser ? mParser->GetContentSink() : nullptr;
}

nsIDocument*
nsIDocument::GetTemplateContentsOwner()
{
  if (!mTemplateContentsOwner) {
    bool hasHadScriptObject = true;
    nsIScriptGlobalObject* scriptObject =
      GetScriptHandlingObject(hasHadScriptObject);

    nsCOMPtr<nsIDocument> document;
    nsresult rv = NS_NewDOMDocument(getter_AddRefs(document),
                                    EmptyString(), // aNamespaceURI
                                    EmptyString(), // aQualifiedName
                                    nullptr, // aDoctype
                                    nsIDocument::GetDocumentURI(),
                                    nsIDocument::GetDocBaseURI(),
                                    NodePrincipal(),
                                    true, // aLoadedAsData
                                    scriptObject, // aEventObject
                                    DocumentFlavorHTML);
    NS_ENSURE_SUCCESS(rv, nullptr);

    mTemplateContentsOwner = document;
    NS_ENSURE_TRUE(mTemplateContentsOwner, nullptr);

    nsDocument* doc = static_cast<nsDocument*>(mTemplateContentsOwner.get());

    if (!scriptObject) {
      mTemplateContentsOwner->SetScopeObject(GetScopeObject());
    }

    doc->mHasHadScriptHandlingObject = hasHadScriptObject;

    // Set |doc| as the template contents owner of itself so that
    // |doc| is the template contents owner of template elements created
    // by |doc|.
    doc->mTemplateContentsOwner = doc;
  }

  return mTemplateContentsOwner;
}

static already_AddRefed<nsPIDOMWindowOuter>
FindTopWindowForElement(Element* element)
{
  nsIDocument* document = element->OwnerDoc();
  if (!document) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = document->GetWindow();
  if (!window) {
    return nullptr;
  }

  // Trying to find the top window (equivalent to window.top).
  if (nsCOMPtr<nsPIDOMWindowOuter> top = window->GetTop()) {
    window = top.forget();
  }
  return window.forget();
}

/**
 * nsAutoFocusEvent is used to dispatch a focus event for an
 * nsGenericHTMLFormElement with the autofocus attribute enabled.
 */
class nsAutoFocusEvent : public Runnable
{
public:
  explicit nsAutoFocusEvent(already_AddRefed<Element>&& aElement,
                            already_AddRefed<nsPIDOMWindowOuter>&& aTopWindow)
    : mozilla::Runnable("nsAutoFocusEvent")
    , mElement(aElement)
    , mTopWindow(aTopWindow)
  {
  }

  NS_IMETHOD Run() override
  {
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

    mozilla::ErrorResult rv;
    mElement->Focus(rv);
    return rv.StealNSResult();
  }
private:
  nsCOMPtr<Element> mElement;
  nsCOMPtr<nsPIDOMWindowOuter> mTopWindow;
};

void
nsIDocument::SetAutoFocusElement(Element* aAutoFocusElement)
{
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

void
nsIDocument::TriggerAutoFocus()
{
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
    nsCOMPtr<nsIDocument> topDoc = topWindow->GetExtantDoc();
    if (topDoc && topDoc->GetReadyStateEnum() == nsIDocument::READYSTATE_COMPLETE) {
      return;
    }

    nsCOMPtr<nsIRunnable> event =
      new nsAutoFocusEvent(autoFocusElement.forget(), topWindow.forget());
    nsresult rv = NS_DispatchToCurrentThread(event.forget());
    NS_ENSURE_SUCCESS_VOID(rv);
  }
}

void
nsIDocument::SetScrollToRef(nsIURI* aDocumentURI)
{
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
    ++start; // Skip over the '#'

    mScrollToRef = Substring(start, end);
  }
}

void
nsIDocument::ScrollToRef()
{
  if (mScrolledToRefAlready) {
    nsCOMPtr<nsIPresShell> shell = GetShell();
    if (shell) {
      shell->ScrollToAnchor();
    }
    return;
  }

  if (mScrollToRef.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIPresShell> shell = GetShell();
  if (shell) {
    nsresult rv = NS_ERROR_FAILURE;
    // We assume that the bytes are in UTF-8, as it says in the spec:
    // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
    NS_ConvertUTF8toUTF16 ref(mScrollToRef);
    // Check an empty string which might be caused by the UTF-8 conversion
    if (!ref.IsEmpty()) {
      // Note that GoToAnchor will handle flushing layout as needed.
      rv = shell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
    } else {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_FAILED(rv)) {
      char* tmpstr = ToNewCString(mScrollToRef);
      if (!tmpstr) {
        return;
      }
      nsUnescape(tmpstr);
      nsAutoCString unescapedRef;
      unescapedRef.Assign(tmpstr);
      free(tmpstr);

      NS_ConvertUTF8toUTF16 utf16Str(unescapedRef);
      if (!utf16Str.IsEmpty()) {
        rv = shell->GoToAnchor(utf16Str, mChangeScrollPosWhenScrollingToRef);
      }

      // If UTF-8 URI failed then try to assume the string as a
      // document's charset.
      if (NS_FAILED(rv)) {
        const Encoding* encoding = GetDocumentCharacterSet();
        rv = encoding->DecodeWithoutBOMHandling(unescapedRef, ref);
        if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
          rv = shell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
        }
      }
    }
    if (NS_SUCCEEDED(rv)) {
      mScrolledToRefAlready = true;
    }
  }
}

void
nsIDocument::RegisterActivityObserver(nsISupports* aSupports)
{
  if (!mActivityObservers) {
    mActivityObservers = new nsTHashtable<nsPtrHashKey<nsISupports> >();
  }
  mActivityObservers->PutEntry(aSupports);
}

bool
nsIDocument::UnregisterActivityObserver(nsISupports* aSupports)
{
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

void
nsIDocument::EnumerateActivityObservers(ActivityObserverEnumerator aEnumerator,
                                        void* aData)
{
  if (!mActivityObservers)
    return;

  for (auto iter = mActivityObservers->ConstIter(); !iter.Done();
       iter.Next()) {
    aEnumerator(iter.Get()->GetKey(), aData);
  }
}

void
nsIDocument::RegisterPendingLinkUpdate(Link* aLink)
{
  if (aLink->HasPendingLinkUpdate()) {
    return;
  }

  aLink->SetHasPendingLinkUpdate();

  if (!mHasLinksToUpdateRunnable && !mFlushingPendingLinkUpdates) {
    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod("nsIDocument::FlushPendingLinkUpdatesFromRunnable",
                        this,
                        &nsIDocument::FlushPendingLinkUpdatesFromRunnable);
    // Do this work in a second in the worst case.
    nsresult rv =
      NS_IdleDispatchToCurrentThread(event.forget(), 1000);
    if (NS_FAILED(rv)) {
      // If during shutdown posting a runnable doesn't succeed, we probably
      // don't need to update link states.
      return;
    }
    mHasLinksToUpdateRunnable = true;
  }

  mLinksToUpdate.InfallibleAppend(aLink);
}

void
nsIDocument::FlushPendingLinkUpdatesFromRunnable()
{
  MOZ_ASSERT(mHasLinksToUpdateRunnable);
  mHasLinksToUpdateRunnable = false;
  FlushPendingLinkUpdates();
}

void
nsIDocument::FlushPendingLinkUpdates()
{
  if (mFlushingPendingLinkUpdates) {
    return;
  }

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

already_AddRefed<nsIDocument>
nsIDocument::CreateStaticClone(nsIDocShell* aCloneContainer)
{
  nsDocument* thisAsDoc = static_cast<nsDocument*>(this);
  mCreatingStaticClone = true;

  // Make document use different container during cloning.
  RefPtr<nsDocShell> originalShell = mDocumentContainer.get();
  SetContainer(static_cast<nsDocShell*>(aCloneContainer));
  ErrorResult rv;
  nsCOMPtr<nsINode> clonedNode = thisAsDoc->CloneNode(true, rv);
  SetContainer(originalShell);

  RefPtr<nsDocument> clonedDoc;
  if (rv.Failed()) {
    // Don't return yet; we need to reset mCreatingStaticClone
    rv.SuppressException();
  } else {
    nsCOMPtr<nsIDocument> tmp = do_QueryInterface(clonedNode);
    if (tmp) {
      clonedDoc = static_cast<nsDocument*>(tmp.get());
      if (IsStaticDocument()) {
        clonedDoc->mOriginalDocument = mOriginalDocument;
      } else {
        clonedDoc->mOriginalDocument = this;
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
    }
  }
  mCreatingStaticClone = false;
  return clonedDoc.forget();
}

void
nsIDocument::UnlinkOriginalDocumentIfStatic()
{
  if (IsStaticDocument() && mOriginalDocument) {
    MOZ_ASSERT(mOriginalDocument->mStaticCloneCount > 0);
    mOriginalDocument->mStaticCloneCount--;
    mOriginalDocument = nullptr;
  }
  MOZ_ASSERT(!mOriginalDocument);
}

nsresult
nsIDocument::ScheduleFrameRequestCallback(FrameRequestCallback& aCallback,
                                          int32_t *aHandle)
{
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

void
nsIDocument::CancelFrameRequestCallback(int32_t aHandle)
{
  // mFrameRequestCallbacks is stored sorted by handle
  if (mFrameRequestCallbacks.RemoveElementSorted(aHandle)) {
    UpdateFrameRequestCallbackSchedulingState();
  }
}

nsresult
nsIDocument::GetStateObject(nsIVariant** aState)
{
  // Get the document's current state object. This is the object backing both
  // history.state and popStateEvent.state.
  //
  // mStateObjectContainer may be null; this just means that there's no
  // current state object.

  if (!mStateObjectCached && mStateObjectContainer) {
    AutoJSContext cx;
    nsIGlobalObject* sgo = GetScopeObject();
    NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);
    JS::Rooted<JSObject*> global(cx, sgo->GetGlobalJSObject());
    NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);
    JSAutoRealm ar(cx, global);

    mStateObjectContainer->
      DeserializeToVariant(cx, getter_AddRefs(mStateObjectCached));
  }

  NS_IF_ADDREF(*aState = mStateObjectCached);
  return NS_OK;
}

void
nsIDocument::SetNavigationTiming(nsDOMNavigationTiming* aTiming)
{
  mTiming = aTiming;
  if (!mLoadingTimeStamp.IsNull() && mTiming) {
    mTiming->SetDOMLoadingTimeStamp(GetDocumentURI(), mLoadingTimeStamp);
  }
}

nsContentList*
nsIDocument::ImageMapList()
{
  if (!mImageMaps) {
    mImageMaps = new nsContentList(this, kNameSpaceID_XHTML,
                                   nsGkAtoms::map, nsGkAtoms::map);
  }

  return mImageMaps;
}

#define DEPRECATED_OPERATION(_op) #_op "Warning",
static const char* kDeprecationWarnings[] = {
#include "nsDeprecatedOperationList.h"
  nullptr
};
#undef DEPRECATED_OPERATION

#define DOCUMENT_WARNING(_op) #_op "Warning",
static const char* kDocumentWarnings[] = {
#include "nsDocumentWarningList.h"
  nullptr
};
#undef DOCUMENT_WARNING

static UseCounter
OperationToUseCounter(nsIDocument::DeprecatedOperations aOperation)
{
  switch(aOperation) {
#define DEPRECATED_OPERATION(_op) \
    case nsIDocument::e##_op: return eUseCounter_##_op;
#include "nsDeprecatedOperationList.h"
#undef DEPRECATED_OPERATION
  default:
    MOZ_CRASH();
  }
}

bool
nsIDocument::HasWarnedAbout(DeprecatedOperations aOperation) const
{
  return mDeprecationWarnedAbout[aOperation];
}

void
nsIDocument::WarnOnceAbout(DeprecatedOperations aOperation,
                           bool asError /* = false */) const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (HasWarnedAbout(aOperation)) {
    return;
  }
  mDeprecationWarnedAbout[aOperation] = true;
  // Don't count deprecated operations for about pages since those pages
  // are almost in our control, and we always need to remove uses there
  // before we remove the operation itself anyway.
  if (!IsAboutPage()) {
    const_cast<nsIDocument*>(this)->
      SetDocumentAndPageUseCounter(OperationToUseCounter(aOperation));
  }
  uint32_t flags = asError ? nsIScriptError::errorFlag
                           : nsIScriptError::warningFlag;
  nsContentUtils::ReportToConsole(flags,
                                  NS_LITERAL_CSTRING("DOM Core"), this,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  kDeprecationWarnings[aOperation]);
}

bool
nsIDocument::HasWarnedAbout(DocumentWarnings aWarning) const
{
  return mDocWarningWarnedAbout[aWarning];
}

void
nsIDocument::WarnOnceAbout(DocumentWarnings aWarning,
                           bool asError /* = false */,
                           const char16_t **aParams /* = nullptr */,
                           uint32_t aParamsLength /* = 0 */) const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (HasWarnedAbout(aWarning)) {
    return;
  }
  mDocWarningWarnedAbout[aWarning] = true;
  uint32_t flags = asError ? nsIScriptError::errorFlag
                           : nsIScriptError::warningFlag;
  nsContentUtils::ReportToConsole(flags,
                                  NS_LITERAL_CSTRING("DOM Core"), this,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  kDocumentWarnings[aWarning],
                                  aParams,
                                  aParamsLength);
}

mozilla::dom::ImageTracker*
nsIDocument::ImageTracker()
{
  if (!mImageTracker) {
    mImageTracker = new mozilla::dom::ImageTracker;
  }
  return mImageTracker;
}

static bool
AllSubDocumentPluginEnum(nsIDocument* aDocument, void* userArg)
{
  nsTArray<nsIObjectLoadingContent*>* plugins =
    reinterpret_cast<nsTArray<nsIObjectLoadingContent*>*>(userArg);
  MOZ_ASSERT(plugins);
  aDocument->GetPlugins(*plugins);
  return true;
}

void
nsIDocument::GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins)
{
  aPlugins.SetCapacity(aPlugins.Length() + mPlugins.Count());
  for (auto iter = mPlugins.ConstIter(); !iter.Done(); iter.Next()) {
    aPlugins.AppendElement(iter.Get()->GetKey());
  }
  EnumerateSubDocuments(AllSubDocumentPluginEnum, &aPlugins);
}

void
nsIDocument::NotifyMediaFeatureValuesChanged()
{
  for (auto iter = mResponsiveContent.ConstIter(); !iter.Done();
       iter.Next()) {
    RefPtr<HTMLImageElement> imageElement = iter.Get()->GetKey();
    imageElement->MediaFeatureValuesChanged();
  }
}

already_AddRefed<Touch>
nsIDocument::CreateTouch(nsGlobalWindowInner* aView,
                         EventTarget* aTarget,
                         int32_t aIdentifier,
                         int32_t aPageX, int32_t aPageY,
                         int32_t aScreenX, int32_t aScreenY,
                         int32_t aClientX, int32_t aClientY,
                         int32_t aRadiusX, int32_t aRadiusY,
                         float aRotationAngle,
                         float aForce)
{
  RefPtr<Touch> touch = new Touch(aTarget,
                                  aIdentifier,
                                  aPageX, aPageY,
                                  aScreenX, aScreenY,
                                  aClientX, aClientY,
                                  aRadiusX, aRadiusY,
                                  aRotationAngle,
                                  aForce);
  return touch.forget();
}

already_AddRefed<TouchList>
nsIDocument::CreateTouchList()
{
  RefPtr<TouchList> retval = new TouchList(ToSupports(this));
  return retval.forget();
}

already_AddRefed<TouchList>
nsIDocument::CreateTouchList(Touch& aTouch,
                             const Sequence<OwningNonNull<Touch> >& aTouches)
{
  RefPtr<TouchList> retval = new TouchList(ToSupports(this));
  retval->Append(&aTouch);
  for (uint32_t i = 0; i < aTouches.Length(); ++i) {
    retval->Append(aTouches[i].get());
  }
  return retval.forget();
}

already_AddRefed<TouchList>
nsIDocument::CreateTouchList(const Sequence<OwningNonNull<Touch> >& aTouches)
{
  RefPtr<TouchList> retval = new TouchList(ToSupports(this));
  for (uint32_t i = 0; i < aTouches.Length(); ++i) {
    retval->Append(aTouches[i].get());
  }
  return retval.forget();
}

already_AddRefed<nsDOMCaretPosition>
nsIDocument::CaretPositionFromPoint(float aX, float aY)
{
  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY);
  nsPoint pt(x, y);

  FlushPendingNotifications(FlushType::Layout);

  nsIPresShell *ps = GetShell();
  if (!ps) {
    return nullptr;
  }

  nsIFrame *rootFrame = ps->GetRootFrame();

  // XUL docs, unlike HTML, have no frame tree until everything's done loading
  if (!rootFrame) {
    return nullptr;
  }

  nsIFrame *ptFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, pt,
      nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC);
  if (!ptFrame) {
    return nullptr;
  }

  // We require frame-relative coordinates for GetContentOffsetsFromPoint.
  nsPoint aOffset;
  nsCOMPtr<nsIWidget> widget = nsContentUtils::GetWidget(ps, &aOffset);
  LayoutDeviceIntPoint refPoint =
    nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), aOffset, GetPresContext());
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
    nsNumberControlFrame* numberFrame = do_QueryFrame(nonanon->GetPrimaryFrame());
    if (textFrame || numberFrame) {
      // If the anonymous content node has a child, then we need to make sure
      // that we get the appropriate child, as otherwise the offset may not be
      // correct when we construct a range for it.
      nsCOMPtr<nsIContent> firstChild = anonNode->GetFirstChild();
      if (firstChild) {
        anonNode = firstChild;
      }

      if (textArea) {
        offset = nsContentUtils::GetAdjustedOffsetInTextControl(ptFrame, offset);
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

bool
nsIDocument::IsPotentiallyScrollable(HTMLBodyElement* aBody)
{
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
      parentFrame->StyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE &&
      parentFrame->StyleDisplay()->mOverflowY == NS_STYLE_OVERFLOW_VISIBLE) {
    return false;
  }

  // The element's used value of the overflow-x or overflow-y properties is not
  // visible.
  if (bodyFrame->StyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE &&
      bodyFrame->StyleDisplay()->mOverflowY == NS_STYLE_OVERFLOW_VISIBLE) {
    return false;
  }

  return true;
}

Element*
nsIDocument::GetScrollingElement()
{
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

bool
nsIDocument::IsScrollingElement(Element* aElement)
{
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

void
nsIDocument::ObsoleteSheet(nsIURI *aSheetURI, ErrorResult& rv)
{
  nsresult res = CSSLoader()->ObsoleteSheet(aSheetURI);
  if (NS_FAILED(res)) {
    rv.Throw(res);
  }
}

void
nsIDocument::ObsoleteSheet(const nsAString& aSheetURI, ErrorResult& rv)
{
  nsCOMPtr<nsIURI> uri;
  nsresult res = NS_NewURI(getter_AddRefs(uri), aSheetURI);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return;
  }
  res = CSSLoader()->ObsoleteSheet(uri);
  if (NS_FAILED(res)) {
    rv.Throw(res);
  }
}

class UnblockParsingPromiseHandler final : public PromiseNativeHandler
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UnblockParsingPromiseHandler)

  explicit UnblockParsingPromiseHandler(nsIDocument* aDocument, Promise* aPromise,
                                        const BlockParsingOptions& aOptions)
    : mPromise(aPromise)
  {
    nsCOMPtr<nsIParser> parser = aDocument->CreatorParserOrNull();
    if (parser && (aOptions.mBlockScriptCreated || !parser->IsScriptCreated())) {
      parser->BlockParser();
      mParser = do_GetWeakReference(parser);
      mDocument = aDocument;
    }
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    MaybeUnblockParser();

    mPromise->MaybeResolve(aCx, aValue);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    MaybeUnblockParser();

    mPromise->MaybeReject(aCx, aValue);
  }

protected:
  virtual ~UnblockParsingPromiseHandler()
  {
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
    mParser = nullptr;
    mDocument = nullptr;
  }

  nsWeakPtr mParser;
  RefPtr<Promise> mPromise;
  RefPtr<nsIDocument> mDocument;
};

NS_IMPL_CYCLE_COLLECTION(UnblockParsingPromiseHandler, mDocument, mPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnblockParsingPromiseHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UnblockParsingPromiseHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnblockParsingPromiseHandler)

already_AddRefed<Promise>
nsIDocument::BlockParsing(Promise& aPromise, const BlockParsingOptions& aOptions, ErrorResult& aRv)
{
  RefPtr<Promise> resultPromise = Promise::Create(aPromise.GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<PromiseNativeHandler> promiseHandler = new UnblockParsingPromiseHandler(this, resultPromise,
                                                                                 aOptions);
  aPromise.AppendNativeHandler(promiseHandler);

  return resultPromise.forget();
}

already_AddRefed<nsIURI>
nsIDocument::GetMozDocumentURIIfNotForErrorPages()
{
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

Promise*
nsIDocument::GetDocumentReadyForIdle(ErrorResult& aRv)
{
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

void
nsIDocument::MaybeResolveReadyForIdle()
{
  IgnoredErrorResult rv;
  Promise* readyPromise = GetDocumentReadyForIdle(rv);
  if (readyPromise) {
    readyPromise->MaybeResolve(this);
  }
}

nsIHTMLCollection*
nsIDocument::Children()
{
  if (!mChildrenCollection) {
    mChildrenCollection = new nsContentList(this, kNameSpaceID_Wildcard,
                                            nsGkAtoms::_asterisk,
                                            nsGkAtoms::_asterisk,
                                            false);
  }

  return mChildrenCollection;
}

uint32_t
nsIDocument::ChildElementCount()
{
  return Children()->Length();
}

namespace mozilla {

// Singleton class to manage the list of fullscreen documents which are the
// root of a branch which contains fullscreen documents. We maintain this list
// so that we can easily exit all windows from fullscreen when the user
// presses the escape key.
class FullscreenRoots {
public:
  // Adds the root of given document to the manager. Calling this method
  // with a document whose root is already contained has no effect.
  static void Add(nsIDocument* aDoc);

  // Iterates over every root in the root list, and calls aFunction, passing
  // each root once to aFunction. It is safe to call Add() and Remove() while
  // iterating over the list (i.e. in aFunction). Documents that are removed
  // from the manager during traversal are not traversed, and documents that
  // are added to the manager during traversal are also not traversed.
  static void ForEach(void(*aFunction)(nsIDocument* aDoc));

  // Removes the root of a specific document from the manager.
  static void Remove(nsIDocument* aDoc);

  // Returns true if all roots added to the list have been removed.
  static bool IsEmpty();

private:

  FullscreenRoots() {
    MOZ_COUNT_CTOR(FullscreenRoots);
  }
  ~FullscreenRoots() {
    MOZ_COUNT_DTOR(FullscreenRoots);
  }

  enum {
    NotFound = uint32_t(-1)
  };
  // Looks in mRoots for aRoot. Returns the index if found, otherwise NotFound.
  static uint32_t Find(nsIDocument* aRoot);

  // Returns true if aRoot is in the list of fullscreen roots.
  static bool Contains(nsIDocument* aRoot);

  // Singleton instance of the FullscreenRoots. This is instantiated when a
  // root is added, and it is deleted when the last root is removed.
  static FullscreenRoots* sInstance;

  // List of weak pointers to roots.
  nsTArray<nsWeakPtr> mRoots;
};

FullscreenRoots* FullscreenRoots::sInstance = nullptr;

/* static */
void
FullscreenRoots::ForEach(void(*aFunction)(nsIDocument* aDoc))
{
  if (!sInstance) {
    return;
  }
  // Create a copy of the roots array, and iterate over the copy. This is so
  // that if an element is removed from mRoots we don't mess up our iteration.
  nsTArray<nsWeakPtr> roots(sInstance->mRoots);
  // Call aFunction on all entries.
  for (uint32_t i = 0; i < roots.Length(); i++) {
    nsCOMPtr<nsIDocument> root = do_QueryReferent(roots[i]);
    // Check that the root isn't in the manager. This is so that new additions
    // while we were running don't get traversed.
    if (root && FullscreenRoots::Contains(root)) {
      aFunction(root);
    }
  }
}

/* static */
bool
FullscreenRoots::Contains(nsIDocument* aRoot)
{
  return FullscreenRoots::Find(aRoot) != NotFound;
}

/* static */
void
FullscreenRoots::Add(nsIDocument* aDoc)
{
  nsCOMPtr<nsIDocument> root = nsContentUtils::GetRootDocument(aDoc);
  if (!FullscreenRoots::Contains(root)) {
    if (!sInstance) {
      sInstance = new FullscreenRoots();
    }
    sInstance->mRoots.AppendElement(do_GetWeakReference(root));
  }
}

/* static */
uint32_t
FullscreenRoots::Find(nsIDocument* aRoot)
{
  if (!sInstance) {
    return NotFound;
  }
  nsTArray<nsWeakPtr>& roots = sInstance->mRoots;
  for (uint32_t i = 0; i < roots.Length(); i++) {
    nsCOMPtr<nsIDocument> otherRoot(do_QueryReferent(roots[i]));
    if (otherRoot == aRoot) {
      return i;
    }
  }
  return NotFound;
}

/* static */
void
FullscreenRoots::Remove(nsIDocument* aDoc)
{
  nsCOMPtr<nsIDocument> root = nsContentUtils::GetRootDocument(aDoc);
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
bool
FullscreenRoots::IsEmpty()
{
  return !sInstance;
}

} // end namespace mozilla.
using mozilla::FullscreenRoots;

nsIDocument*
nsIDocument::GetFullscreenRoot()
{
  nsCOMPtr<nsIDocument> root = do_QueryReferent(mFullscreenRoot);
  return root;
}

void
nsIDocument::SetFullscreenRoot(nsIDocument* aRoot)
{
  mFullscreenRoot = do_GetWeakReference(aRoot);
}

void
nsIDocument::ExitFullscreen()
{
  RestorePreviousFullScreenState();
}

static void
AskWindowToExitFullscreen(nsIDocument* aDoc)
{
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

class nsCallExitFullscreen : public Runnable
{
public:
  explicit nsCallExitFullscreen(nsIDocument* aDoc)
    : mozilla::Runnable("nsCallExitFullscreen")
    , mDoc(aDoc)
  {
  }

  NS_IMETHOD Run() final
  {
    if (!mDoc) {
      FullscreenRoots::ForEach(&AskWindowToExitFullscreen);
    } else {
      AskWindowToExitFullscreen(mDoc);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDocument> mDoc;
};

/* static */ void
nsIDocument::AsyncExitFullscreen(nsIDocument* aDoc)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> exit = new nsCallExitFullscreen(aDoc);
  if (aDoc) {
    aDoc->Dispatch(TaskCategory::Other, exit.forget());
  } else {
    NS_DispatchToCurrentThread(exit.forget());
  }
}

static bool
CountFullscreenSubDocuments(nsIDocument* aDoc, void* aData)
{
  if (aDoc->FullScreenStackTop()) {
    uint32_t* count = static_cast<uint32_t*>(aData);
    (*count)++;
  }
  return true;
}

static uint32_t
CountFullscreenSubDocuments(nsIDocument* aDoc)
{
  uint32_t count = 0;
  aDoc->EnumerateSubDocuments(CountFullscreenSubDocuments, &count);
  return count;
}

bool
nsIDocument::IsFullscreenLeaf()
{
  // A fullscreen leaf document is fullscreen, and has no fullscreen
  // subdocuments.
  if (!FullScreenStackTop()) {
    return false;
  }
  return CountFullscreenSubDocuments(this) == 0;
}

static bool
ResetFullScreen(nsIDocument* aDocument, void* aData)
{
  if (aDocument->FullScreenStackTop()) {
    NS_ASSERTION(CountFullscreenSubDocuments(aDocument) <= 1,
        "Should have at most 1 fullscreen subdocument.");
    aDocument->CleanupFullscreenState();
    NS_ASSERTION(!aDocument->FullScreenStackTop(),
                 "Should reset full-screen");
    auto changed = reinterpret_cast<nsCOMArray<nsIDocument>*>(aData);
    changed->AppendElement(aDocument);
    aDocument->EnumerateSubDocuments(ResetFullScreen, aData);
  }
  return true;
}

// Since nsIDocument::ExitFullscreenInDocTree() could be called from
// Element::UnbindFromTree() where it is not safe to synchronously run
// script. This runnable is the script part of that function.
class ExitFullscreenScriptRunnable : public Runnable
{
public:
  explicit ExitFullscreenScriptRunnable(nsCOMArray<nsIDocument>&& aDocuments)
    : mozilla::Runnable("ExitFullscreenScriptRunnable")
    , mDocuments(std::move(aDocuments))
  {
  }

  NS_IMETHOD Run() override
  {
    // Dispatch MozDOMFullscreen:Exited to the last document in
    // the list since we want this event to follow the same path
    // MozDOMFullscreen:Entered dispatched.
    nsIDocument* lastDocument = mDocuments[mDocuments.Length() - 1];
    nsContentUtils::DispatchEventOnlyToChrome(
      lastDocument, ToSupports(lastDocument),
      NS_LITERAL_STRING("MozDOMFullscreen:Exited"),
      CanBubble::eYes, Cancelable::eNo, /* DefaultAction */ nullptr);
    // Ensure the window exits fullscreen.
    if (nsPIDOMWindowOuter* win = mDocuments[0]->GetWindow()) {
      win->SetFullscreenInternal(FullscreenReason::ForForceExitFullscreen, false);
    }
    return NS_OK;
  }

private:
  nsCOMArray<nsIDocument> mDocuments;
};

/* static */ void
nsIDocument::ExitFullscreenInDocTree(nsIDocument* aMaybeNotARootDoc)
{
  MOZ_ASSERT(aMaybeNotARootDoc);

  // Unlock the pointer
  UnlockPointer();

  nsCOMPtr<nsIDocument> root = aMaybeNotARootDoc->GetFullscreenRoot();
  if (!root || !root->FullScreenStackTop()) {
    // If a document was detached before exiting from fullscreen, it is
    // possible that the root had left fullscreen state. In this case,
    // we would not get anything from the ResetFullScreen() call. Root's
    // not being a fullscreen doc also means the widget should have
    // exited fullscreen state. It means even if we do not return here,
    // we would actually do nothing below except crashing ourselves via
    // dispatching the "MozDOMFullscreen:Exited" event to an nonexistent
    // document.
    return;
  }

  // Stores a list of documents to which we must dispatch "fullscreenchange".
  // We're required by the spec to dispatch the events in leaf-to-root
  // order when exiting fullscreen, but we traverse the doctree in a
  // root-to-leaf order, so we save references to the documents we must
  // dispatch to so that we dispatch in the specified order.
  nsCOMArray<nsIDocument> changed;

  // Walk the tree of fullscreen documents, and reset their fullscreen state.
  ResetFullScreen(root, static_cast<void*>(&changed));

  // Dispatch "fullscreenchange" events. Note this loop is in reverse
  // order so that the events for the leaf document arrives before the root
  // document, as required by the spec.
  for (uint32_t i = 0; i < changed.Length(); ++i) {
    DispatchFullScreenChange(changed[changed.Length() - i - 1]);
  }

  NS_ASSERTION(!root->FullScreenStackTop(),
    "Fullscreen root should no longer be a fullscreen doc...");

  // Move the top-level window out of fullscreen mode.
  FullscreenRoots::Remove(root);

  nsContentUtils::AddScriptRunner(
    new ExitFullscreenScriptRunnable(std::move(changed)));
}

bool
GetFullscreenLeaf(nsIDocument* aDoc, void* aData)
{
  if (aDoc->IsFullscreenLeaf()) {
    nsIDocument** result = static_cast<nsIDocument**>(aData);
    *result = aDoc;
    return false;
  } else if (aDoc->FullScreenStackTop()) {
    aDoc->EnumerateSubDocuments(GetFullscreenLeaf, aData);
  }
  return true;
}

static nsIDocument*
GetFullscreenLeaf(nsIDocument* aDoc)
{
  nsIDocument* leaf = nullptr;
  GetFullscreenLeaf(aDoc, &leaf);
  if (leaf) {
    return leaf;
  }
  // Otherwise we could be either in a non-fullscreen doc tree, or we're
  // below the fullscreen doc. Start the search from the root.
  nsIDocument* root = nsContentUtils::GetRootDocument(aDoc);
  // Check that the root is actually fullscreen so we don't waste time walking
  // around its descendants.
  if (!root->FullScreenStackTop()) {
    return nullptr;
  }
  GetFullscreenLeaf(root, &leaf);
  return leaf;
}

void
nsIDocument::RestorePreviousFullScreenState()
{
  NS_ASSERTION(!FullScreenStackTop() || !FullscreenRoots::IsEmpty(),
    "Should have at least 1 fullscreen root when fullscreen!");

  if (!FullScreenStackTop() || !GetWindow() || FullscreenRoots::IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIDocument> fullScreenDoc = GetFullscreenLeaf(this);
  AutoTArray<nsIDocument*, 8> exitDocs;

  nsIDocument* doc = fullScreenDoc;
  // Collect all subdocuments.
  for (; doc != this; doc = doc->GetParentDocument()) {
    exitDocs.AppendElement(doc);
  }
  MOZ_ASSERT(doc == this, "Must have reached this doc");
  // Collect all ancestor documents which we are going to change.
  for (; doc; doc = doc->GetParentDocument()) {
    MOZ_ASSERT(!doc->mFullScreenStack.IsEmpty(),
               "Ancestor of fullscreen document must also be in fullscreen");
    if (doc != this) {
      Element* top = doc->FullScreenStackTop();
      if (top->IsHTMLElement(nsGkAtoms::iframe)) {
        if (static_cast<HTMLIFrameElement*>(top)->FullscreenFlag()) {
          // If this is an iframe, and it explicitly requested
          // fullscreen, don't rollback it automatically.
          break;
        }
      }
    }
    exitDocs.AppendElement(doc);
    if (doc->mFullScreenStack.Length() > 1) {
      break;
    }
  }

  nsIDocument* lastDoc = exitDocs.LastElement();
  if (!lastDoc->GetParentDocument() &&
      lastDoc->mFullScreenStack.Length() == 1) {
    // If we are fully exiting fullscreen, don't touch anything here,
    // just wait for the window to get out from fullscreen first.
    AskWindowToExitFullscreen(this);
    return;
  }

  // If fullscreen mode is updated the pointer should be unlocked
  UnlockPointer();
  // All documents listed in the array except the last one are going to
  // completely exit from the fullscreen state.
  for (auto i : IntegerRange(exitDocs.Length() - 1)) {
    exitDocs[i]->CleanupFullscreenState();
  }
  // The last document will either rollback one fullscreen element, or
  // completely exit from the fullscreen state as well.
  nsIDocument* newFullscreenDoc;
  if (lastDoc->mFullScreenStack.Length() > 1) {
    lastDoc->FullScreenStackPop();
    newFullscreenDoc = lastDoc;
  } else {
    lastDoc->CleanupFullscreenState();
    newFullscreenDoc = lastDoc->GetParentDocument();
  }
  // Dispatch the fullscreenchange event to all document listed.
  for (nsIDocument* d : exitDocs) {
    DispatchFullScreenChange(d);
  }

  MOZ_ASSERT(newFullscreenDoc, "If we were going to exit from fullscreen on "
             "all documents in this doctree, we should've asked the window to "
             "exit first instead of reaching here.");
  if (fullScreenDoc != newFullscreenDoc &&
      !nsContentUtils::HaveEqualPrincipals(fullScreenDoc, newFullscreenDoc)) {
    // We've popped so enough off the stack that we've rolled back to
    // a fullscreen element in a parent document. If this document is
    // cross origin, dispatch an event to chrome so it knows to show
    // the warning UI.
    DispatchCustomEventWithFlush(
      newFullscreenDoc, NS_LITERAL_STRING("MozDOMFullscreen:NewOrigin"),
      /* Bubbles */ true, /* ChromeOnly */ true);
  }
}

class nsCallRequestFullScreen : public Runnable
{
public:
  explicit nsCallRequestFullScreen(UniquePtr<FullscreenRequest>&& aRequest)
    : mozilla::Runnable("nsCallRequestFullScreen")
    , mRequest(std::move(aRequest))
  {
  }

  NS_IMETHOD Run() override
  {
    mRequest->GetDocument()->RequestFullScreen(std::move(mRequest));
    return NS_OK;
  }

  UniquePtr<FullscreenRequest> mRequest;
};

void
nsIDocument::AsyncRequestFullScreen(UniquePtr<FullscreenRequest>&& aRequest)
{
  if (!aRequest->GetElement()) {
    MOZ_ASSERT_UNREACHABLE(
      "Must pass non-null element to nsDocument::AsyncRequestFullScreen");
    return;
  }

  // Request full-screen asynchronously.
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> event = new nsCallRequestFullScreen(std::move(aRequest));
  Dispatch(TaskCategory::Other, event.forget());
}

void
nsIDocument::DispatchFullscreenError(const char* aMessage)
{
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this,
                             NS_LITERAL_STRING("fullscreenerror"),
                             CanBubble::eYes,
                             ChromeOnlyDispatch::eNo);
  asyncDispatcher->PostDOMEvent();
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DOM"), this,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aMessage);
}

static void
UpdateViewportScrollbarOverrideForFullscreen(nsIDocument* aDoc)
{
  if (nsPresContext* presContext = aDoc->GetPresContext()) {
    presContext->UpdateViewportScrollbarStylesOverride();
  }
}

static void
ClearFullscreenStateOnElement(Element* aElement)
{
  // Remove styles from existing top element.
  EventStateManager::SetFullScreenState(aElement, false);
  // Reset iframe fullscreen flag.
  if (aElement->IsHTMLElement(nsGkAtoms::iframe)) {
    static_cast<HTMLIFrameElement*>(aElement)->SetFullscreenFlag(false);
  }
}

void
nsIDocument::CleanupFullscreenState()
{
  // Iterate the fullscreen stack and clear the fullscreen states.
  // Since we also need to clear the fullscreen-ancestor state, and
  // currently fullscreen elements can only be placed in hierarchy
  // order in the stack, reversely iterating the stack could be more
  // efficient. NOTE that fullscreen-ancestor state would be removed
  // in bug 1199529, and the elements may not in hierarchy order
  // after bug 1195213.
  for (nsWeakPtr& weakPtr : Reversed(mFullScreenStack)) {
    if (nsCOMPtr<Element> element = do_QueryReferent(weakPtr)) {
      ClearFullscreenStateOnElement(element);
    }
  }
  mFullScreenStack.Clear();
  mFullscreenRoot = nullptr;
  UpdateViewportScrollbarOverrideForFullscreen(this);
}

bool
nsIDocument::FullScreenStackPush(Element* aElement)
{
  NS_ASSERTION(aElement, "Must pass non-null to FullScreenStackPush()");
  Element* top = FullScreenStackTop();
  if (top == aElement || !aElement) {
    return false;
  }
  EventStateManager::SetFullScreenState(aElement, true);
  mFullScreenStack.AppendElement(do_GetWeakReference(aElement));
  NS_ASSERTION(FullScreenStackTop() == aElement, "Should match");
  UpdateViewportScrollbarOverrideForFullscreen(this);
  return true;
}

void
nsIDocument::FullScreenStackPop()
{
  if (mFullScreenStack.IsEmpty()) {
    return;
  }

  ClearFullscreenStateOnElement(FullScreenStackTop());

  // Remove top element. Note the remaining top element in the stack
  // will not have full-screen style bits set, so we will need to restore
  // them on the new top element before returning.
  uint32_t last = mFullScreenStack.Length() - 1;
  mFullScreenStack.RemoveElementAt(last);

  // Pop from the stack null elements (references to elements which have
  // been GC'd since they were added to the stack) and elements which are
  // no longer in this document.
  while (!mFullScreenStack.IsEmpty()) {
    Element* element = FullScreenStackTop();
    if (!element || !element->IsInUncomposedDoc() || element->OwnerDoc() != this) {
      NS_ASSERTION(!element->State().HasState(NS_EVENT_STATE_FULL_SCREEN),
                   "Should have already removed full-screen styles");
      uint32_t last = mFullScreenStack.Length() - 1;
      mFullScreenStack.RemoveElementAt(last);
    } else {
      // The top element of the stack is now an in-doc element. Return here.
      break;
    }
  }

  UpdateViewportScrollbarOverrideForFullscreen(this);
}

Element*
nsIDocument::FullScreenStackTop()
{
  if (mFullScreenStack.IsEmpty()) {
    return nullptr;
  }
  uint32_t last = mFullScreenStack.Length() - 1;
  nsCOMPtr<Element> element(do_QueryReferent(mFullScreenStack[last]));
  NS_ASSERTION(element, "Should have full-screen element!");
  NS_ASSERTION(element->IsInComposedDoc(), "Full-screen element should be in doc");
  NS_ASSERTION(element->OwnerDoc() == this, "Full-screen element should be in this doc");
  return element;
}

nsTArray<Element*>
nsIDocument::GetFullscreenStack() const
{
  nsTArray<Element*> elements;
  for (const nsWeakPtr& ptr : mFullScreenStack) {
    if (nsCOMPtr<Element> elem = do_QueryReferent(ptr)) {
      MOZ_ASSERT(elem->State().HasState(NS_EVENT_STATE_FULL_SCREEN));
      elements.AppendElement(elem);
    }
  }
  return elements;
}

// Returns true if aDoc is in the focused tab in the active window.
static bool
IsInActiveTab(nsIDocument* aDoc)
{
  nsCOMPtr<nsIDocShell> docshell = aDoc->GetDocShell();
  if (!docshell) {
    return false;
  }

  bool isActive = false;
  docshell->GetIsActive(&isActive);
  if (!isActive) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  docshell->GetRootTreeItem(getter_AddRefs(rootItem));
  if (!rootItem) {
    return false;
  }
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem->GetWindow();
  if (!rootWin) {
    return false;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return false;
  }

  nsCOMPtr<mozIDOMWindowProxy> activeWindow;
  fm->GetActiveWindow(getter_AddRefs(activeWindow));
  if (!activeWindow) {
    return false;
  }

  return activeWindow == rootWin;
}

nsresult nsIDocument::RemoteFrameFullscreenChanged(Element* aFrameElement)
{
  // Ensure the frame element is the fullscreen element in this document.
  // If the frame element is already the fullscreen element in this document,
  // this has no effect.
  auto request = MakeUnique<FullscreenRequest>(aFrameElement);
  request->mIsCallerChrome = false;
  request->mShouldNotifyNewOrigin = false;
  RequestFullScreen(std::move(request));

  return NS_OK;
}

nsresult nsIDocument::RemoteFrameFullscreenReverted()
{
  RestorePreviousFullScreenState();
  return NS_OK;
}

/* static */ bool
nsIDocument::IsUnprefixedFullscreenEnabled(JSContext* aCx, JSObject* aObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  return nsContentUtils::IsSystemCaller(aCx) ||
         nsContentUtils::IsUnprefixedFullscreenApiEnabled();
}

static bool
HasFullScreenSubDocument(nsIDocument* aDoc)
{
  uint32_t count = CountFullscreenSubDocuments(aDoc);
  NS_ASSERTION(count <= 1, "Fullscreen docs should have at most 1 fullscreen child!");
  return count >= 1;
}

// Returns nullptr if a request for Fullscreen API is currently enabled
// in the given document. Returns a static string indicates the reason
// why it is not enabled otherwise.
static const char*
GetFullscreenError(nsIDocument* aDoc, bool aCallerIsChrome)
{
  bool apiEnabled = nsContentUtils::IsFullScreenApiEnabled();
  if (apiEnabled && aCallerIsChrome) {
    // Chrome code can always use the full-screen API, provided it's not
    // explicitly disabled.
    return nullptr;
  }

  if (!apiEnabled) {
    return "FullscreenDeniedDisabled";
  }

  // Ensure that all containing elements are <iframe> and have
  // allowfullscreen attribute set.
  nsCOMPtr<nsIDocShell> docShell(aDoc->GetDocShell());
  if (!docShell || !docShell->GetFullscreenAllowed()) {
    return "FullscreenDeniedContainerNotAllowed";
  }
  return nullptr;
}

bool
nsIDocument::FullscreenElementReadyCheck(Element* aElement,
                                         bool aWasCallerChrome)
{
  NS_ASSERTION(aElement,
    "Must pass non-null element to nsDocument::RequestFullScreen");
  if (!aElement || aElement == FullScreenStackTop()) {
    return false;
  }
  if (!aElement->IsInComposedDoc()) {
    DispatchFullscreenError("FullscreenDeniedNotInDocument");
    return false;
  }
  if (aElement->OwnerDoc() != this) {
    DispatchFullscreenError("FullscreenDeniedMovedDocument");
    return false;
  }
  if (!GetWindow()) {
    DispatchFullscreenError("FullscreenDeniedLostWindow");
    return false;
  }
  if (const char* msg = GetFullscreenError(this, aWasCallerChrome)) {
    DispatchFullscreenError(msg);
    return false;
  }
  if (!IsVisible()) {
    DispatchFullscreenError("FullscreenDeniedHidden");
    return false;
  }
  if (HasFullScreenSubDocument(this)) {
    DispatchFullscreenError("FullscreenDeniedSubDocFullScreen");
    return false;
  }
  //XXXsmaug Note, we don't follow the latest fullscreen spec here.
  //         This whole check could be probably removed.
  if (FullScreenStackTop() &&
      !nsContentUtils::ContentIsHostIncludingDescendantOf(aElement,
                                                          FullScreenStackTop())) {
    // If this document is full-screen, only grant full-screen requests from
    // a descendant of the current full-screen element.
    DispatchFullscreenError("FullscreenDeniedNotDescendant");
    return false;
  }
  if (!nsContentUtils::IsChromeDoc(this) && !IsInActiveTab(this)) {
    DispatchFullscreenError("FullscreenDeniedNotFocusedTab");
    return false;
  }
  // Deny requests when a windowed plugin is focused.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    NS_WARNING("Failed to retrieve focus manager in full-screen request.");
    return false;
  }
  if (nsContentUtils::HasPluginWithUncontrolledEventDispatch(fm->GetFocusedElement())) {
    DispatchFullscreenError("FullscreenDeniedFocusedPlugin");
    return false;
  }
  return true;
}

FullscreenRequest::FullscreenRequest(Element* aElement)
  : mElement(aElement)
  , mDocument(static_cast<nsDocument*>(aElement->OwnerDoc()))
{
  MOZ_COUNT_CTOR(FullscreenRequest);
}

FullscreenRequest::~FullscreenRequest()
{
  MOZ_COUNT_DTOR(FullscreenRequest);
}

// Any fullscreen request waiting for the widget to finish being full-
// screen is queued here. This is declared static instead of a member
// of nsDocument because in the majority of time, there would be at most
// one document requesting fullscreen. We shouldn't waste the space to
// hold for it in every document.
class PendingFullscreenRequestList
{
public:
  static void Add(UniquePtr<FullscreenRequest>&& aRequest)
  {
    sList.insertBack(aRequest.release());
  }

  static const FullscreenRequest* GetLast()
  {
    return sList.getLast();
  }

  enum IteratorOption
  {
    // When we are committing fullscreen changes or preparing for
    // that, we generally want to iterate all requests in the same
    // window with eDocumentsWithSameRoot option.
    eDocumentsWithSameRoot,
    // If we are removing a document from the tree, we would only
    // want to remove the requests from the given document and its
    // descendants. For that case, use eInclusiveDescendants.
    eInclusiveDescendants
  };

  class Iterator
  {
  public:
    explicit Iterator(nsIDocument* aDoc, IteratorOption aOption)
      : mCurrent(PendingFullscreenRequestList::sList.getFirst())
      , mRootShellForIteration(aDoc->GetDocShell())
    {
      if (mCurrent) {
        if (mRootShellForIteration && aOption == eDocumentsWithSameRoot) {
          mRootShellForIteration->
            GetRootTreeItem(getter_AddRefs(mRootShellForIteration));
        }
        SkipToNextMatch();
      }
    }

    void DeleteAndNext()
    {
      DeleteAndNextInternal();
      SkipToNextMatch();
    }
    bool AtEnd() const { return mCurrent == nullptr; }
    const FullscreenRequest& Get() const { return *mCurrent; }

  private:
    void DeleteAndNextInternal()
    {
      FullscreenRequest* thisRequest = mCurrent;
      mCurrent = mCurrent->getNext();
      delete thisRequest;
    }
    void SkipToNextMatch()
    {
      while (mCurrent) {
        nsCOMPtr<nsIDocShellTreeItem>
          docShell = mCurrent->GetDocument()->GetDocShell();
        if (!docShell) {
          // Always automatically drop documents which has been
          // detached from the doc shell.
          DeleteAndNextInternal();
        } else {
          while (docShell && docShell != mRootShellForIteration) {
            docShell->GetParent(getter_AddRefs(docShell));
          }
          if (!docShell) {
            // We've gone over the root, but haven't find the target
            // ancestor, so skip this item.
            mCurrent = mCurrent->getNext();
          } else {
            break;
          }
        }
      }
    }

    FullscreenRequest* mCurrent;
    nsCOMPtr<nsIDocShellTreeItem> mRootShellForIteration;
  };

private:
  PendingFullscreenRequestList() = delete;

  static LinkedList<FullscreenRequest> sList;
};

/* static */ LinkedList<FullscreenRequest> PendingFullscreenRequestList::sList;

static nsCOMPtr<nsPIDOMWindowOuter>
GetRootWindow(nsIDocument* aDoc)
{
  nsIDocShell* docShell = aDoc->GetDocShell();
  if (!docShell) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  docShell->GetRootTreeItem(getter_AddRefs(rootItem));
  return rootItem ? rootItem->GetWindow() : nullptr;
}

static bool
ShouldApplyFullscreenDirectly(nsIDocument* aDoc,
                              nsPIDOMWindowOuter* aRootWin)
{
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
    PendingFullscreenRequestList::Iterator
      iter(aDoc, PendingFullscreenRequestList::eDocumentsWithSameRoot);
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

void
nsIDocument::RequestFullScreen(UniquePtr<FullscreenRequest>&& aRequest)
{
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = GetRootWindow(this);
  if (!rootWin) {
    return;
  }

  if (ShouldApplyFullscreenDirectly(this, rootWin)) {
    ApplyFullscreen(*aRequest);
    return;
  }

  // Per spec only HTML, <svg>, and <math> should be allowed, but
  // we also need to allow XUL elements right now.
  Element* elem = aRequest->GetElement();
  if (!elem->IsHTMLElement() && !elem->IsXULElement() &&
      !elem->IsSVGElement(nsGkAtoms::svg) &&
      !elem->IsMathMLElement(nsGkAtoms::math)) {
    DispatchFullscreenError("FullscreenDeniedNotHTMLSVGOrMathML");
    return;
  }

  // We don't need to check element ready before this point, because
  // if we called ApplyFullscreen, it would check that for us.
  if (!FullscreenElementReadyCheck(elem, aRequest->mIsCallerChrome)) {
    return;
  }

  PendingFullscreenRequestList::Add(std::move(aRequest));
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

/* static */ bool
nsIDocument::HandlePendingFullscreenRequests(nsIDocument* aDoc)
{
  bool handled = false;
  PendingFullscreenRequestList::Iterator iter(
    aDoc, PendingFullscreenRequestList::eDocumentsWithSameRoot);
  while (!iter.AtEnd()) {
    const FullscreenRequest& request = iter.Get();
    if (request.GetDocument()->ApplyFullscreen(request)) {
      handled = true;
    }
    iter.DeleteAndNext();
  }
  return handled;
}

static void
ClearPendingFullscreenRequests(nsIDocument* aDoc)
{
  PendingFullscreenRequestList::Iterator iter(
    aDoc, PendingFullscreenRequestList::eInclusiveDescendants);
  while (!iter.AtEnd()) {
    iter.DeleteAndNext();
  }
}

bool
nsIDocument::ApplyFullscreen(const FullscreenRequest& aRequest)
{
  Element* elem = aRequest.GetElement();
  if (!FullscreenElementReadyCheck(elem, aRequest.mIsCallerChrome)) {
    return false;
  }

  // Stash a reference to any existing fullscreen doc, we'll use this later
  // to detect if the origin which is fullscreen has changed.
  nsCOMPtr<nsIDocument> previousFullscreenDoc = GetFullscreenLeaf(this);

  // Stores a list of documents which we must dispatch "fullscreenchange"
  // too. We're required by the spec to dispatch the events in root-to-leaf
  // order, but we traverse the doctree in a leaf-to-root order, so we save
  // references to the documents we must dispatch to so that we get the order
  // as specified.
  AutoTArray<nsIDocument*, 8> changed;

  // Remember the root document, so that if a full-screen document is hidden
  // we can reset full-screen state in the remaining visible full-screen documents.
  nsIDocument* fullScreenRootDoc = nsContentUtils::GetRootDocument(this);

  // If a document is already in fullscreen, then unlock the mouse pointer
  // before setting a new document to fullscreen
  UnlockPointer();

  // Set the full-screen element. This sets the full-screen style on the
  // element, and the full-screen-ancestor styles on ancestors of the element
  // in this document.
  DebugOnly<bool> x = FullScreenStackPush(elem);
  NS_ASSERTION(x, "Full-screen state of requesting doc should always change!");
  // Set the iframe fullscreen flag.
  if (elem->IsHTMLElement(nsGkAtoms::iframe)) {
    static_cast<HTMLIFrameElement*>(elem)->SetFullscreenFlag(true);
  }
  changed.AppendElement(this);

  // Propagate up the document hierarchy, setting the full-screen element as
  // the element's container in ancestor documents. This also sets the
  // appropriate css styles as well. Note we don't propagate down the
  // document hierarchy, the full-screen element (or its container) is not
  // visible there. Stop when we reach the root document.
  nsIDocument* child = this;
  while (true) {
    child->SetFullscreenRoot(fullScreenRootDoc);
    NS_ASSERTION(child->GetFullscreenRoot() == fullScreenRootDoc,
        "Fullscreen root should be set!");
    if (child == fullScreenRootDoc) {
      break;
    }
    nsIDocument* parent = child->GetParentDocument();
    Element* element = parent->FindContentForSubDocument(child);
    if (static_cast<nsDocument*>(parent)->FullScreenStackPush(element)) {
      changed.AppendElement(parent);
      child = parent;
    } else {
      // We've reached either the root, or a point in the doctree where the
      // new full-screen element container is the same as the previous
      // full-screen element's container. No more changes need to be made
      // to the full-screen stacks of documents further up the tree.
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
  if (aRequest.mShouldNotifyNewOrigin &&
      !nsContentUtils::HaveEqualPrincipals(previousFullscreenDoc, this)) {
    DispatchCustomEventWithFlush(
      this, NS_LITERAL_STRING("MozDOMFullscreen:NewOrigin"),
      /* Bubbles */ true, /* ChromeOnly */ true);
  }

  // Dispatch "fullscreenchange" events. Note this loop is in reverse
  // order so that the events for the root document arrives before the leaf
  // document, as required by the spec.
  for (uint32_t i = 0; i < changed.Length(); ++i) {
    DispatchFullScreenChange(changed[changed.Length() - i - 1]);
  }
  return true;
}

bool
nsIDocument::FullscreenEnabled(CallerType aCallerType)
{
  return !GetFullscreenError(this, aCallerType == CallerType::System);
}

void
nsIDocument::SetOrientationPendingPromise(Promise* aPromise)
{
  mOrientationPendingPromise = aPromise;
}

static void
DispatchPointerLockChange(nsIDocument* aTarget)
{
  if (!aTarget) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(aTarget,
                             NS_LITERAL_STRING("pointerlockchange"),
                             CanBubble::eYes,
                             ChromeOnlyDispatch::eNo);
  asyncDispatcher->PostDOMEvent();
}

static void
DispatchPointerLockError(nsIDocument* aTarget, const char* aMessage)
{
  if (!aTarget) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(aTarget,
                             NS_LITERAL_STRING("pointerlockerror"),
                             CanBubble::eYes,
                             ChromeOnlyDispatch::eNo);
  asyncDispatcher->PostDOMEvent();
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DOM"), aTarget,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aMessage);
}

class PointerLockRequest final : public Runnable
{
public:
  PointerLockRequest(Element* aElement, bool aUserInputOrChromeCaller)
    : mozilla::Runnable("PointerLockRequest")
    , mElement(do_GetWeakReference(aElement))
    , mDocument(do_GetWeakReference(aElement->OwnerDoc()))
    , mUserInputOrChromeCaller(aUserInputOrChromeCaller)
  {}

  NS_IMETHOD Run() final;

private:
  nsWeakPtr mElement;
  nsWeakPtr mDocument;
  bool mUserInputOrChromeCaller;
};

static const char*
GetPointerLockError(Element* aElement, Element* aCurrentLock,
                    bool aNoFocusCheck = false)
{
  // Check if pointer lock pref is enabled
  if (!Preferences::GetBool("full-screen-api.pointer-lock.enabled")) {
    return "PointerLockDeniedDisabled";
  }

  nsCOMPtr<nsIDocument> ownerDoc = aElement->OwnerDoc();
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

  nsCOMPtr<nsPIDOMWindowOuter> top = ownerWindow->GetScriptableTop();
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

static void
ChangePointerLockedElement(Element* aElement, nsIDocument* aDocument,
                           Element* aPointerLockedElement)
{
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
  nsIPresShell::SetCapturingContent(aElement, CAPTURE_POINTERLOCK);
  DispatchPointerLockChange(aDocument);
}

NS_IMETHODIMP
PointerLockRequest::Run()
{
  nsCOMPtr<Element> e = do_QueryReferent(mElement);
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
  nsDocument* d = static_cast<nsDocument*>(doc.get());
  const char* error = nullptr;
  if (!e || !d || !e->GetComposedDoc()) {
    error = "PointerLockDeniedNotInDocument";
  } else if (e->GetComposedDoc() != d) {
    error = "PointerLockDeniedMovedDocument";
  }
  if (!error) {
    nsCOMPtr<Element> pointerLockedElement =
      do_QueryReferent(EventStateManager::sPointerLockedElement);
    if (e == pointerLockedElement) {
      DispatchPointerLockChange(d);
      return NS_OK;
    }
    // Note, we must bypass focus change, so pass true as the last parameter!
    error = GetPointerLockError(e, pointerLockedElement, true);
    // Another element in the same document is requesting pointer lock,
    // just grant it without user input check.
    if (!error && pointerLockedElement) {
      ChangePointerLockedElement(e, d, pointerLockedElement);
      return NS_OK;
    }
  }
  // If it is neither user input initiated, nor requested in fullscreen,
  // it should be rejected.
  if (!error && !mUserInputOrChromeCaller && !doc->GetFullscreenElement()) {
    error = "PointerLockDeniedNotInputDriven";
  }
  if (!error && !d->SetPointerLock(e, NS_STYLE_CURSOR_NONE)) {
    error = "PointerLockDeniedFailedToLock";
  }
  if (error) {
    DispatchPointerLockError(d, error);
    return NS_OK;
  }

  ChangePointerLockedElement(e, d, nullptr);
  nsContentUtils::DispatchEventOnlyToChrome(
    doc, ToSupports(e), NS_LITERAL_STRING("MozDOMPointerLock:Entered"),
    CanBubble::eYes, Cancelable::eNo, /* DefaultAction */ nullptr);
  return NS_OK;
}

void
nsIDocument::RequestPointerLock(Element* aElement, CallerType aCallerType)
{
  NS_ASSERTION(aElement,
    "Must pass non-null element to nsDocument::RequestPointerLock");

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

  bool userInputOrSystemCaller = EventStateManager::IsHandlingUserInput() ||
                                 aCallerType == CallerType::System;
  nsCOMPtr<nsIRunnable> request =
    new PointerLockRequest(aElement, userInputOrSystemCaller);
  Dispatch(TaskCategory::Other, request.forget());
}

bool
nsIDocument::SetPointerLock(Element* aElement, int aCursorStyle)
{
  MOZ_ASSERT(!aElement || aElement->OwnerDoc() == this,
             "We should be either unlocking pointer (aElement is nullptr), "
             "or locking pointer to an element in this document");
#ifdef DEBUG
  if (!aElement) {
    nsCOMPtr<nsIDocument> pointerLockedDoc =
      do_QueryReferent(EventStateManager::sPointerLockedDoc);
    MOZ_ASSERT(pointerLockedDoc == this);
  }
#endif

  nsIPresShell* shell = GetShell();
  if (!shell) {
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
  nsPresContext* presContext = shell->GetPresContext();
  if (!presContext) {
    NS_WARNING("SetPointerLock(): Unable to get PresContext");
    return false;
  }

  nsCOMPtr<nsIWidget> widget;
  nsIFrame* rootFrame = shell->GetRootFrame();
  if (!NS_WARN_IF(!rootFrame)) {
    widget = rootFrame->GetNearestWidget();
    NS_WARNING_ASSERTION(
      widget,
      "SetPointerLock(): Unable to find widget in "
      "shell->GetRootFrame()->GetNearestWidget();");
    if (aElement && !widget) {
      return false;
    }
  }

  // Hide the cursor and set pointer lock for future mouse events
  RefPtr<EventStateManager> esm = presContext->EventStateManager();
  esm->SetCursor(aCursorStyle, nullptr, false,
                 0.0f, 0.0f, widget, true);
  EventStateManager::SetPointerLock(widget, aElement);

  return true;
}

void
nsIDocument::UnlockPointer(nsIDocument* aDoc)
{
  if (!EventStateManager::sIsPointerLocked) {
    return;
  }

  nsCOMPtr<nsIDocument> pointerLockedDoc =
    do_QueryReferent(EventStateManager::sPointerLockedDoc);
  if (!pointerLockedDoc || (aDoc && aDoc != pointerLockedDoc)) {
    return;
  }
  if (!pointerLockedDoc->SetPointerLock(nullptr, NS_STYLE_CURSOR_AUTO)) {
    return;
  }

  nsCOMPtr<Element> pointerLockedElement =
    do_QueryReferent(EventStateManager::sPointerLockedElement);
  ChangePointerLockedElement(nullptr, pointerLockedDoc, pointerLockedElement);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(pointerLockedElement,
                             NS_LITERAL_STRING("MozDOMPointerLock:Exited"),
                             CanBubble::eYes,
                             ChromeOnlyDispatch::eYes);
  asyncDispatcher->RunDOMEventWhenSafe();
}

void
nsIDocument::UpdateVisibilityState()
{
  dom::VisibilityState oldState = mVisibilityState;
  mVisibilityState = ComputeVisibilityState();
  if (oldState != mVisibilityState) {
    nsContentUtils::DispatchTrustedEvent(this, static_cast<nsIDocument*>(this),
                                         NS_LITERAL_STRING("visibilitychange"),
                                         CanBubble::eYes,
                                         Cancelable::eNo);
    EnumerateActivityObservers(NotifyActivityChanged, nullptr);
  }

  if (mVisibilityState == dom::VisibilityState::Visible) {
    MaybeActiveMediaComponents();
  }
}

VisibilityState
nsIDocument::ComputeVisibilityState() const
{
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

void
nsIDocument::PostVisibilityUpdateEvent()
{
  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod("nsIDocument::UpdateVisibilityState",
                      this,
                      &nsIDocument::UpdateVisibilityState);
  Dispatch(TaskCategory::Other, event.forget());
}

void
nsIDocument::MaybeActiveMediaComponents()
{
  if (!mWindow) {
    return;
  }

  GetWindow()->MaybeActiveMediaComponents();
}

/* virtual */ void
nsIDocument::DocAddSizeOfExcludingThis(nsWindowSizes& aSizes) const
{
  nsINode::AddSizeOfExcludingThis(aSizes, &aSizes.mDOMOtherSize);

  if (mPresShell) {
    mPresShell->AddSizeOfIncludingThis(aSizes);
  }

  aSizes.mPropertyTablesSize +=
    mPropertyTable.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  if (EventListenerManager* elm = GetExistingListenerManager()) {
    aSizes.mDOMEventListenersCount += elm->ListenerCount();
  }

  if (mNodeInfoManager) {
    mNodeInfoManager->AddSizeOfIncludingThis(aSizes);
  }

  aSizes.mDOMMediaQueryLists +=
    mDOMMediaQueryLists.sizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  for (const MediaQueryList* mql : mDOMMediaQueryLists) {
    aSizes.mDOMMediaQueryLists +=
      mql->SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - many!
}

void
nsIDocument::DocAddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const
{
  aWindowSizes.mDOMOtherSize += aWindowSizes.mState.mMallocSizeOf(this);
  DocAddSizeOfExcludingThis(aWindowSizes);
}

static size_t
SizeOfOwnedSheetArrayExcludingThis(const nsTArray<RefPtr<StyleSheet>>& aSheets,
                                   MallocSizeOf aMallocSizeOf)
{
  size_t n = 0;
  n += aSheets.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (StyleSheet* sheet : aSheets) {
    if (!sheet->GetAssociatedDocumentOrShadowRoot()) {
      // Avoid over-reporting shared sheets.
      continue;
    }
    n += sheet->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

void
nsDocument::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                   size_t* aNodeSize) const
{
  // This AddSizeOfExcludingThis() overrides the one from nsINode.  But
  // nsDocuments can only appear at the top of the DOM tree, and we use the
  // specialized DocAddSizeOfExcludingThis() in that case.  So this should never
  // be called.
  MOZ_CRASH();
}

static void
AddSizeOfNodeTree(nsIContent* aNode, nsWindowSizes& aWindowSizes)
{
  size_t nodeSize = 0;
  aNode->AddSizeOfIncludingThis(aWindowSizes, &nodeSize);

  // This is where we transfer the nodeSize obtained from
  // nsINode::AddSizeOfIncludingThis() to a value in nsWindowSizes.
  switch (aNode->NodeType()) {
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

  if (EventListenerManager* elm = aNode->GetExistingListenerManager()) {
    aWindowSizes.mDOMEventListenersCount += elm->ListenerCount();
  }

  AllChildrenIterator iter(aNode, nsIContent::eAllChildren);
  for (nsIContent* n = iter.GetNextChild(); n; n = iter.GetNextChild()) {
    AddSizeOfNodeTree(n, aWindowSizes);
  }
}

void
nsDocument::DocAddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const
{
  // We use AllChildrenIterator to iterate over DOM nodes in
  // AddSizeOfNodeTree(). The obvious place to start is at the document's root
  // element, using GetRootElement(). However, that will miss comment nodes
  // that are siblings of the root element. Instead we use
  // GetFirstChild()/GetNextSibling() to traverse the document's immediate
  // child nodes, calling AddSizeOfNodeTree() on each to measure them and then
  // all their descendants. (The comment nodes won't have any descendants).
  for (nsIContent* node = nsINode::GetFirstChild();
       node;
       node = node->GetNextSibling()) {
    AddSizeOfNodeTree(node, aWindowSizes);
  }

  // IMPORTANT: for our ComputedValues measurements, we want to measure
  // ComputedValues accessible from DOM elements before ComputedValues not
  // accessible from DOM elements (i.e. accessible only from the frame tree).
  //
  // Therefore, the measurement of the nsIDocument superclass must happen after
  // the measurement of DOM nodes (above), because nsIDocument contains the
  // PresShell, which contains the frame tree.
  nsIDocument::DocAddSizeOfExcludingThis(aWindowSizes);

  aWindowSizes.mLayoutStyleSheetsSize +=
    SizeOfOwnedSheetArrayExcludingThis(mStyleSheets,
                                       aWindowSizes.mState.mMallocSizeOf);
  for (auto& sheetArray : mAdditionalSheets) {
    aWindowSizes.mLayoutStyleSheetsSize +=
      SizeOfOwnedSheetArrayExcludingThis(sheetArray,
                                         aWindowSizes.mState.mMallocSizeOf);
  }
  // Lumping in the loader with the style-sheets size is not ideal,
  // but most of the things in there are in fact stylesheets, so it
  // doesn't seem worthwhile to separate it out.
  aWindowSizes.mLayoutStyleSheetsSize +=
    CSSLoader()->SizeOfIncludingThis(aWindowSizes.mState.mMallocSizeOf);

  aWindowSizes.mDOMOtherSize += mAttrStyleSheet
                              ? mAttrStyleSheet->DOMSizeOfIncludingThis(
                                  aWindowSizes.mState.mMallocSizeOf)
                              : 0;

  aWindowSizes.mDOMOtherSize +=
    mStyledLinks.ShallowSizeOfExcludingThis(aWindowSizes.mState.mMallocSizeOf);

  aWindowSizes.mDOMOtherSize +=
    mIdentifierMap.SizeOfExcludingThis(aWindowSizes.mState.mMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - many!
}

already_AddRefed<nsIDocument>
nsIDocument::Constructor(const GlobalObject& aGlobal,
                         ErrorResult& rv)
{
  nsCOMPtr<nsIScriptGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> prin = do_QueryInterface(aGlobal.GetAsSupports());
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

  nsCOMPtr<nsIDocument> doc;
  nsresult res =
    NS_NewDOMDocument(getter_AddRefs(doc),
                      VoidString(),
                      EmptyString(),
                      nullptr,
                      uri,
                      uri,
                      prin->GetPrincipal(),
                      true,
                      global,
                      DocumentFlavorPlain);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return nullptr;
  }

  doc->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);

  return doc.forget();
}

XPathExpression*
nsIDocument::CreateExpression(const nsAString& aExpression,
                              XPathNSResolver* aResolver,
                              ErrorResult& rv)
{
  return XPathEvaluator()->CreateExpression(aExpression, aResolver, rv);
}

nsINode*
nsIDocument::CreateNSResolver(nsINode& aNodeResolver)
{
  return XPathEvaluator()->CreateNSResolver(aNodeResolver);
}

already_AddRefed<XPathResult>
nsIDocument::Evaluate(JSContext* aCx, const nsAString& aExpression,
                      nsINode& aContextNode, XPathNSResolver* aResolver,
                      uint16_t aType, JS::Handle<JSObject*> aResult,
                      ErrorResult& rv)
{
  return XPathEvaluator()->Evaluate(aCx, aExpression, aContextNode, aResolver,
                                    aType, aResult, rv);
}

already_AddRefed<nsIXULWindow>
nsIDocument::GetXULWindowIfToplevelChrome() const
{
  nsCOMPtr<nsIDocShellTreeItem> item = GetDocShell();
  if (!item) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShellTreeOwner> owner;
  item->GetTreeOwner(getter_AddRefs(owner));
  nsCOMPtr<nsIXULWindow> xulWin = do_GetInterface(owner);
  if (!xulWin) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShell> xulWinShell;
  xulWin->GetDocShell(getter_AddRefs(xulWinShell));
  if (!SameCOMIdentity(xulWinShell, item)) {
    return nullptr;
  }
  return xulWin.forget();
}

nsIDocument*
nsIDocument::GetTopLevelContentDocument()
{
  nsIDocument* parent;

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

    nsIDocument* candidate = parent->GetParentDocument();
    parent = static_cast<nsDocument*>(candidate);
  } while (parent);

  return parent;
}

static bool
MightBeChromeScheme(nsIURI* aURI)
{
  MOZ_ASSERT(aURI);
  bool isChrome = true;
  aURI->SchemeIs("chrome", &isChrome);
  return isChrome;
}

static bool
MightBeAboutOrChromeScheme(nsIURI* aURI)
{
  MOZ_ASSERT(aURI);
  bool isAbout = true;
  aURI->SchemeIs("about", &isAbout);
  return isAbout || MightBeChromeScheme(aURI);
}

void
nsIDocument::PropagateUseCounters(nsIDocument* aParentDocument)
{
  MOZ_ASSERT(this != aParentDocument);

  // Don't count chrome resources, even in the web content.
  nsCOMPtr<nsIURI> uri;
  NodePrincipal()->GetURI(getter_AddRefs(uri));
  if (!uri || MightBeChromeScheme(uri)) {
    return;
  }

  // What really matters here is that our use counters get propagated as
  // high up in the content document hierarchy as possible.  So,
  // starting with aParentDocument, we need to find the toplevel content
  // document, and propagate our use counters into its
  // mChildDocumentUseCounters.
  nsIDocument* contentParent = aParentDocument->GetTopLevelContentDocument();

  if (!contentParent) {
    return;
  }

  contentParent->mChildDocumentUseCounters |= mUseCounters;
  contentParent->mChildDocumentUseCounters |= mChildDocumentUseCounters;
}

void
nsIDocument::SetPageUseCounter(UseCounter aUseCounter)
{
  // We want to set the use counter on the "page" that owns us; the definition
  // of "page" depends on what kind of document we are.  See the comments below
  // for details.  In any event, checking all the conditions below is
  // reasonably expensive, so we cache whether we've notified our owning page.
  if (mNotifiedPageForUseCounter[aUseCounter]) {
    return;
  }
  mNotifiedPageForUseCounter[aUseCounter] = true;

  if (mDisplayDocument) {
    // If we are a resource document, we won't have a docshell and so we won't
    // record any page use counters on this document.  Instead, we should
    // forward it up to the document that loaded us.
    MOZ_ASSERT(!mDocumentContainer);
    mDisplayDocument->SetChildDocumentUseCounter(aUseCounter);
    return;
  }

  if (IsBeingUsedAsImage()) {
    // If this is an SVG image document, we also won't have a docshell.
    MOZ_ASSERT(!mDocumentContainer);
    return;
  }

  // We only care about use counters in content.  If we're already a toplevel
  // content document, then we should have already set the use counter on
  // ourselves, and we are done.
  nsIDocument* contentParent = GetTopLevelContentDocument();
  if (!contentParent) {
    return;
  }

  if (this == contentParent) {
    MOZ_ASSERT(GetUseCounter(aUseCounter));
    return;
  }

  contentParent->SetChildDocumentUseCounter(aUseCounter);
}

bool
nsIDocument::HasScriptsBlockedBySandbox()
{
  return mSandboxFlags & SANDBOXED_SCRIPTS;
}

bool
nsIDocument::InlineScriptAllowedByCSP()
{
  // this function assumes the inline script is parser created
  //  (e.g., before setting attribute(!) event handlers)
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv = NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, true);
  bool allowsInlineScript = true;
  if (csp) {
    nsresult rv = csp->GetAllowsInline(nsIContentPolicy::TYPE_SCRIPT,
                                       EmptyString(), // aNonce
                                       true,          // aParserCreated
                                       nullptr,       // aTriggeringElement
                                       EmptyString(), // FIXME get script sample (bug 1314567)
                                       0,             // aLineNumber
                                       0,             // aColumnNumber
                                       &allowsInlineScript);
    NS_ENSURE_SUCCESS(rv, true);
  }
  return allowsInlineScript;
}

static bool
ReportExternalResourceUseCounters(nsIDocument* aDocument, void* aData)
{
  const auto reportKind
    = nsDocument::UseCounterReportKind::eIncludeExternalResources;
  static_cast<nsDocument*>(aDocument)->ReportUseCounters(reportKind);
  return true;
}

void
nsIDocument::ReportUseCounters(UseCounterReportKind aKind)
{
  static const bool sDebugUseCounters = false;
  if (mReportedUseCounters) {
    return;
  }

  mReportedUseCounters = true;

  if (aKind == UseCounterReportKind::eIncludeExternalResources) {
    EnumerateExternalResources(ReportExternalResourceUseCounters, nullptr);
  }

  if (Telemetry::HistogramUseCounterCount > 0 &&
      (IsContentDocument() || IsResourceDoc())) {
    nsCOMPtr<nsIURI> uri;
    NodePrincipal()->GetURI(getter_AddRefs(uri));
    if (!uri || MightBeAboutOrChromeScheme(uri)) {
      return;
    }

    if (sDebugUseCounters) {
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
      Telemetry::Accumulate(Telemetry::TOP_LEVEL_CONTENT_DOCUMENTS_DESTROYED, 1);
    }

    for (int32_t c = 0;
         c < eUseCounter_Count; ++c) {
      UseCounter uc = static_cast<UseCounter>(c);

      Telemetry::HistogramID id =
        static_cast<Telemetry::HistogramID>(Telemetry::HistogramFirstUseCounter + uc * 2);
      bool value = GetUseCounter(uc);

      if (value) {
        if (sDebugUseCounters) {
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
        id = static_cast<Telemetry::HistogramID>(Telemetry::HistogramFirstUseCounter +
                                                 uc * 2 + 1);
        value = GetUseCounter(uc) || GetChildDocumentUseCounter(uc);

        if (value) {
          if (sDebugUseCounters) {
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

  if (IsTopLevelContentDocument() && !IsResourceDoc()) {
    using mozilla::Telemetry::LABELS_HIDDEN_VIEWPORT_OVERFLOW_TYPE;
    LABELS_HIDDEN_VIEWPORT_OVERFLOW_TYPE label;
    switch (mViewportOverflowType) {
#define CASE_OVERFLOW_TYPE(t_)                            \
      case ViewportOverflowType::t_:                      \
        label = LABELS_HIDDEN_VIEWPORT_OVERFLOW_TYPE::t_; \
        break;
      CASE_OVERFLOW_TYPE(NoOverflow)
      CASE_OVERFLOW_TYPE(Desktop)
      CASE_OVERFLOW_TYPE(ButNotMinScaleSize)
      CASE_OVERFLOW_TYPE(MinScaleSize)
#undef CASE_OVERFLOW_TYPE
    }
    Telemetry::AccumulateCategorical(label);
  }
}

void
nsIDocument::UpdateIntersectionObservations()
{
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
  nsTArray<RefPtr<DOMIntersectionObserver>> observers(mIntersectionObservers.Count());
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

void
nsIDocument::ScheduleIntersectionObserverNotification()
{
  if (mIntersectionObservers.IsEmpty()) {
    return;
  }
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> notification =
    NewRunnableMethod("nsDocument::NotifyIntersectionObservers",
                      this,
                      &nsDocument::NotifyIntersectionObservers);
  Dispatch(TaskCategory::Other, notification.forget());
}

void
nsIDocument::NotifyIntersectionObservers()
{
  nsTArray<RefPtr<DOMIntersectionObserver>> observers(mIntersectionObservers.Count());
  for (auto iter = mIntersectionObservers.Iter(); !iter.Done(); iter.Next()) {
    DOMIntersectionObserver* observer = iter.Get()->GetKey();
    observers.AppendElement(observer);
  }
  for (const auto& observer : observers) {
    if (observer) {
      observer->Notify();
    }
  }
}

static bool
NotifyLayerManagerRecreatedCallback(nsIDocument* aDocument, void* aData)
{
  aDocument->NotifyLayerManagerRecreated();
  return true;
}

void
nsIDocument::NotifyLayerManagerRecreated()
{
  EnumerateActivityObservers(NotifyActivityChanged, nullptr);
  EnumerateSubDocuments(NotifyLayerManagerRecreatedCallback, nullptr);
}

XPathEvaluator*
nsIDocument::XPathEvaluator()
{
  if (!mXPathEvaluator) {
    mXPathEvaluator.reset(new dom::XPathEvaluator(this));
  }
  return mXPathEvaluator.get();
}

already_AddRefed<nsIDocumentEncoder>
nsIDocument::GetCachedEncoder()
{
  return mCachedEncoder.forget();
}

void
nsIDocument::SetCachedEncoder(already_AddRefed<nsIDocumentEncoder> aEncoder)
{
  mCachedEncoder = aEncoder;
}

void
nsIDocument::SetContentTypeInternal(const nsACString& aType)
{
  if (!IsHTMLOrXHTML() && mDefaultElementType == kNameSpaceID_None &&
      aType.EqualsLiteral("application/xhtml+xml")) {
    mDefaultElementType = kNameSpaceID_XHTML;
  }

  mCachedEncoder = nullptr;
  mContentType = aType;
  mContentTypeForWriteCalls = aType;
}

nsILoadContext*
nsIDocument::GetLoadContext() const
{
  return mDocumentContainer;
}

nsIDocShell*
nsIDocument::GetDocShell() const
{
  return mDocumentContainer;
}

void
nsIDocument::SetStateObject(nsIStructuredCloneContainer *scContainer)
{
  mStateObjectContainer = scContainer;
  mStateObjectCached = nullptr;
}

already_AddRefed<Element>
nsIDocument::CreateHTMLElement(nsAtom* aTag)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(aTag, nullptr, kNameSpaceID_XHTML,
                                           ELEMENT_NODE);
  MOZ_ASSERT(nodeInfo, "GetNodeInfo should never fail");

  nsCOMPtr<Element> element;
  DebugOnly<nsresult> rv = NS_NewHTMLElement(getter_AddRefs(element),
                                             nodeInfo.forget(),
                                             mozilla::dom::NOT_FROM_PARSER);

  MOZ_ASSERT(NS_SUCCEEDED(rv), "NS_NewHTMLElement should never fail");
  return element.forget();
}

bool
MarkDocumentTreeToBeInSyncOperation(nsIDocument* aDoc, void* aData)
{
  nsCOMArray<nsIDocument>* documents =
    static_cast<nsCOMArray<nsIDocument>*>(aData);
  if (aDoc) {
    aDoc->SetIsInSyncOperation(true);
    if (nsCOMPtr<nsPIDOMWindowInner> window = aDoc->GetInnerWindow()) {
      window->TimeoutManager().BeginSyncOperation();
    }
    documents->AppendObject(aDoc);
    aDoc->EnumerateSubDocuments(MarkDocumentTreeToBeInSyncOperation, aData);
  }
  return true;
}

nsAutoSyncOperation::nsAutoSyncOperation(nsIDocument* aDoc)
{
  mMicroTaskLevel = 0;
  CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
  if (ccjs) {
    mMicroTaskLevel = ccjs->MicroTaskLevel();
    ccjs->SetMicroTaskLevel(0);
  }
  if (aDoc) {
    if (nsPIDOMWindowOuter* win = aDoc->GetWindow()) {
      if (nsCOMPtr<nsPIDOMWindowOuter> top = win->GetTop()) {
        nsCOMPtr<nsIDocument> doc = top->GetExtantDoc();
        MarkDocumentTreeToBeInSyncOperation(doc, &mDocuments);
      }
    }
  }
}

nsAutoSyncOperation::~nsAutoSyncOperation()
{
  for (int32_t i = 0; i < mDocuments.Count(); ++i) {
    if (nsCOMPtr<nsPIDOMWindowInner> window = mDocuments[i]->GetInnerWindow()) {
      window->TimeoutManager().EndSyncOperation();
    }
    mDocuments[i]->SetIsInSyncOperation(false);
  }
  CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
  if (ccjs) {
    ccjs->SetMicroTaskLevel(mMicroTaskLevel);
  }
}

gfxUserFontSet*
nsIDocument::GetUserFontSet(bool aFlushUserFontSet)
{
  // We want to initialize the user font set lazily the first time the
  // user asks for it, rather than building it too early and forcing
  // rule cascade creation.  Thus we try to enforce the invariant that
  // we *never* build the user font set until the first call to
  // GetUserFontSet.  However, once it's been requested, we can't wait
  // for somebody to call GetUserFontSet in order to rebuild it (see
  // comments below in MarkUserFontSetDirty for why).
#ifdef DEBUG
  bool userFontSetGottenBefore = mGetUserFontSetCalled;
#endif
  // Set mGetUserFontSetCalled up front, so that FlushUserFontSet will actually
  // flush.
  mGetUserFontSetCalled = true;
  if (mFontFaceSetDirty && aFlushUserFontSet) {
    // If this assertion fails, and there have actually been changes to
    // @font-face rules, then we will call StyleChangeReflow in
    // FlushUserFontSet.  If we're in the middle of reflow,
    // that's a bad thing to do, and the caller was responsible for
    // flushing first.  If we're not (e.g., in frame construction), it's
    // ok.
    NS_ASSERTION(!userFontSetGottenBefore ||
                 !GetShell() ||
                 !GetShell()->IsReflowLocked(),
                 "FlushUserFontSet should have been called first");
    FlushUserFontSet();
  }

  if (!mFontFaceSet) {
    return nullptr;
  }

  return mFontFaceSet->GetUserFontSet();
}

void
nsIDocument::FlushUserFontSet()
{
  if (!mGetUserFontSetCalled) {
    return; // No one cares about this font set yet, but we want to be careful
            // to not unset our mFontFaceSetDirty bit, so when someone really
            // does we'll create it.
  }

  if (!mFontFaceSetDirty) {
    return;
  }

  mFontFaceSetDirty = false;

  if (gfxPlatform::GetPlatform()->DownloadableFontsEnabled()) {
    nsTArray<nsFontFaceRuleContainer> rules;
    nsIPresShell* shell = GetShell();
    if (shell && !shell->StyleSet()->AppendFontFaceRules(rules)) {
      return;
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
    if (changed && shell) {
      if (nsPresContext* presContext = shell->GetPresContext()) {
        presContext->UserFontSetUpdated();
      }
    }
  }
}

void
nsIDocument::MarkUserFontSetDirty()
{
  if (!mGetUserFontSetCalled) {
    // We want to lazily build the user font set the first time it's
    // requested (so we don't force creation of rule cascades too
    // early), so don't do anything now.
    return;
  }

  mFontFaceSetDirty = true;
}

FontFaceSet*
nsIDocument::Fonts()
{
  if (!mFontFaceSet) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetScopeObject());
    mFontFaceSet = new FontFaceSet(window, this);
    GetUserFontSet();  // this will cause the user font set to be created/updated
  }
  return mFontFaceSet;
}

void
nsIDocument::ReportHasScrollLinkedEffect()
{
  if (mHasScrollLinkedEffect) {
    // We already did this once for this document, don't do it again.
    return;
  }
  mHasScrollLinkedEffect = true;
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Async Pan/Zoom"),
                                  this, nsContentUtils::eLAYOUT_PROPERTIES,
                                  "ScrollLinkedEffectFound2");
}

void
nsIDocument::SetUserHasInteracted(bool aUserHasInteracted)
{
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug,
          ("Document %p has been interacted by user.", this));
  mUserHasInteracted = aUserHasInteracted;
}

void
nsIDocument::NotifyUserGestureActivation()
{
  // Activate this document and all documents up to the top level
  // content document.
  nsIDocument* doc = this;
  while (doc && !doc->mUserGestureActivated) {
    MOZ_LOG(gUserInteractionPRLog,
            LogLevel::Debug,
            ("Document %p has been activated by user.", this));
    doc->mUserGestureActivated = true;
    doc->MaybeAllowStorageForOpener();
    doc = doc->GetSameTypeParentDocument();
  }
}

void
nsIDocument::MaybeAllowStorageForOpener()
{
  if (!StaticPrefs::privacy_restrict3rdpartystorage_enabled()) {
    return;
  }

  // This will probably change for project fission, but currently this document
  // and the opener are on the same process. In the future, we should make this
  // part async.

  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (NS_WARN_IF(!inner)) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> outer = inner->GetOuterWindow();
  if (NS_WARN_IF(!outer)) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> outerOpener = outer->GetOpener();
  if (NS_WARN_IF(!outerOpener)) {
    return;
  }

  nsPIDOMWindowInner* openerInner = outerOpener->GetCurrentInnerWindow();
  if (NS_WARN_IF(!openerInner)) {
    return;
  }

  // No 3rd party or no tracking resource.
  if (!nsContentUtils::IsThirdPartyWindowOrChannel(openerInner, nullptr,
                                                   nullptr) ||
      !nsContentUtils::IsTrackingResourceWindow(openerInner)) {
    return;
  }

  nsCOMPtr<nsIURI> uri = GetDocumentURI();
  if (NS_WARN_IF(!uri)) {
    return;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(uri, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  AntiTrackingCommon::AddFirstPartyStorageAccessGrantedFor(origin,
                                                           openerInner);
}

bool
nsIDocument::HasBeenUserGestureActivated()
{
  if (mUserGestureActivated) {
    return true;
  }

  // If any ancestor document is activated, so are we.
  nsIDocument* doc = GetSameTypeParentDocument();
  while (doc) {
    if (doc->mUserGestureActivated) {
      // An ancestor is also activated. Record activation on the unactivated
      // sub-branch to speed up future queries.
      NotifyUserGestureActivation();
      break;
    }
    doc = doc->GetSameTypeParentDocument();
  }

  return mUserGestureActivated;
}

nsIDocument*
nsIDocument::GetSameTypeParentDocument()
{
  nsCOMPtr<nsIDocShellTreeItem> current = GetDocShell();
  if (!current) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  current->GetSameTypeParent(getter_AddRefs(parent));
  if (!parent) {
    return nullptr;
  }

  return parent->GetDocument();
}

/**
 * Retrieves the classification of the Flash plugins in the document based on
 * the classification lists. We perform AsyncInitFlashClassification on
 * StartDocumentLoad() and the result may not be initialized when this function
 * gets called. In that case, We can only unfortunately have a blocking wait.
 *
 * For more information, see
 * toolkit/components/url-classifier/flash-block-lists.rst
 */
FlashClassification
nsIDocument::PrincipalFlashClassification()
{
  MOZ_ASSERT(mPrincipalFlashClassifier);
  return mPrincipalFlashClassifier->ClassifyMaybeSync(NodePrincipal(),
                                                      IsThirdParty());
}

/**
 * Helper function for |nsDocument::PrincipalFlashClassification|
 *
 * Adds a table name string to a table list (a comma separated string). The
 * table will not be added if the name is an empty string.
 */
static void
MaybeAddTableToTableList(const nsACString& aTableNames,
                         nsACString& aTableList)
{
  if (aTableNames.IsEmpty()) {
    return;
  }
  if (!aTableList.IsEmpty()) {
    aTableList.AppendLiteral(",");
  }
  aTableList.Append(aTableNames);
}

/**
 * Helper function for |nsDocument::PrincipalFlashClassification|
 *
 * Takes an array of table names and a comma separated list of table names
 * Returns |true| if any table name in the array matches a table name in the
 * comma separated list.
 */
static bool
ArrayContainsTable(const nsTArray<nsCString>& aTableArray,
                   const nsACString& aTableNames)
{
  for (const nsCString& table : aTableArray) {
    // This check is sufficient because table names cannot contain commas and
    // cannot contain another existing table name.
    if (FindInReadable(table, aTableNames)) {
      return true;
    }
  }
  return false;
}

namespace {

// An object to store all preferences we need for flash blocking feature.
struct PrefStore
{
  PrefStore()
    : mFlashBlockEnabled(false)
    , mPluginsHttpOnly(false)
  {
    Preferences::AddBoolVarCache(&mFlashBlockEnabled,
                                 "plugins.flashBlock.enabled");
    Preferences::AddBoolVarCache(&mPluginsHttpOnly,
                                 "plugins.http_https_only");

    // We only need to register string-typed preferences.
    Preferences::RegisterCallback(UpdateStringPrefs, "urlclassifier.flashAllowTable", this);
    Preferences::RegisterCallback(UpdateStringPrefs, "urlclassifier.flashAllowExceptTable", this);
    Preferences::RegisterCallback(UpdateStringPrefs, "urlclassifier.flashTable", this);
    Preferences::RegisterCallback(UpdateStringPrefs, "urlclassifier.flashExceptTable", this);
    Preferences::RegisterCallback(UpdateStringPrefs, "urlclassifier.flashSubDocTable", this);
    Preferences::RegisterCallback(UpdateStringPrefs, "urlclassifier.flashSubDocExceptTable", this);

    UpdateStringPrefs();
  }

  ~PrefStore()
  {
    Preferences::UnregisterCallback(UpdateStringPrefs, "urlclassifier.flashAllowTable", this);
    Preferences::UnregisterCallback(UpdateStringPrefs, "urlclassifier.flashAllowExceptTable", this);
    Preferences::UnregisterCallback(UpdateStringPrefs, "urlclassifier.flashTable", this);
    Preferences::UnregisterCallback(UpdateStringPrefs, "urlclassifier.flashExceptTable", this);
    Preferences::UnregisterCallback(UpdateStringPrefs, "urlclassifier.flashSubDocTable", this);
    Preferences::UnregisterCallback(UpdateStringPrefs, "urlclassifier.flashSubDocExceptTable", this);
  }

  void UpdateStringPrefs()
  {
    Preferences::GetCString("urlclassifier.flashAllowTable", mAllowTables);
    Preferences::GetCString("urlclassifier.flashAllowExceptTable", mAllowExceptionsTables);
    Preferences::GetCString("urlclassifier.flashTable", mDenyTables);
    Preferences::GetCString("urlclassifier.flashExceptTable", mDenyExceptionsTables);
    Preferences::GetCString("urlclassifier.flashSubDocTable", mSubDocDenyTables);
    Preferences::GetCString("urlclassifier.flashSubDocExceptTable", mSubDocDenyExceptionsTables);
  }

  static void UpdateStringPrefs(const char*, void* aClosure)
  {
    static_cast<PrefStore*>(aClosure)->UpdateStringPrefs();
  }

  bool mFlashBlockEnabled;
  bool mPluginsHttpOnly;

  nsCString mAllowTables;
  nsCString mAllowExceptionsTables;
  nsCString mDenyTables;
  nsCString mDenyExceptionsTables;
  nsCString mSubDocDenyTables;
  nsCString mSubDocDenyExceptionsTables;
};

static const
PrefStore& GetPrefStore()
{
  static UniquePtr<PrefStore> sPrefStore;
  if (!sPrefStore) {
    sPrefStore.reset(new PrefStore());
    ClearOnShutdown(&sPrefStore);
  }
  return *sPrefStore;
}

} // end of unnamed namespace.

////////////////////////////////////////////////////////////////////
// PrincipalFlashClassifier implementation.

NS_IMPL_ISUPPORTS(PrincipalFlashClassifier, nsIURIClassifierCallback)

PrincipalFlashClassifier::PrincipalFlashClassifier()
{
  Reset();
}

void
PrincipalFlashClassifier::Reset()
{
  mAsyncClassified = false;
  mMatchedTables.Clear();
  mResult = FlashClassification::Unclassified;
}

void
PrincipalFlashClassifier::GetClassificationTables(bool aIsThirdParty,
                                                  nsACString& aTables)
{
  aTables.Truncate();
  auto& prefs = GetPrefStore();

  MaybeAddTableToTableList(prefs.mAllowTables, aTables);
  MaybeAddTableToTableList(prefs.mAllowExceptionsTables, aTables);
  MaybeAddTableToTableList(prefs.mDenyTables, aTables);
  MaybeAddTableToTableList(prefs.mDenyExceptionsTables, aTables);

  if (aIsThirdParty) {
    MaybeAddTableToTableList(prefs.mSubDocDenyTables, aTables);
    MaybeAddTableToTableList(prefs.mSubDocDenyExceptionsTables, aTables);
  }
}

bool
PrincipalFlashClassifier::EnsureUriClassifier()
{
  if (!mUriClassifier) {
    mUriClassifier = do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID);
  }

  return !!mUriClassifier;
}

FlashClassification
PrincipalFlashClassifier::ClassifyMaybeSync(nsIPrincipal* aPrincipal, bool aIsThirdParty)
{
  if (FlashClassification::Unclassified != mResult) {
    // We already have the result. Just return it.
    return mResult;
  }

  // TODO: Bug 1342333 - Entirely remove the use of the sync API
  // (ClassifyLocalWithTables).
  if (!mAsyncClassified) {

    //
    // We may
    //   1) have called AsyncClassifyLocalWithTables but OnClassifyComplete
    //      hasn't been called.
    //   2) haven't even called AsyncClassifyLocalWithTables.
    //
    // In both cases we need to do the synchronous classification as the fallback.
    //

    if (!EnsureUriClassifier()) {
      return FlashClassification::Denied;
    }
    mResult = CheckIfClassifyNeeded(aPrincipal);
    if (FlashClassification::Unclassified != mResult) {
      return mResult;
    }

    nsresult rv;
    nsAutoCString classificationTables;
    GetClassificationTables(aIsThirdParty, classificationTables);

    if (!mClassificationURI) {
      rv = aPrincipal->GetURI(getter_AddRefs(mClassificationURI));
      if (NS_FAILED(rv) || !mClassificationURI) {
        mResult = FlashClassification::Denied;
        return mResult;
      }
    }

    rv = mUriClassifier->ClassifyLocalWithTables(mClassificationURI,
                                                 classificationTables,
                                                 mMatchedTables);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      if (rv == NS_ERROR_MALFORMED_URI) {
        // This means that the URI had no hostname (ex: file://doc.html). In this
        // case, we allow the default (Unknown plugin) behavior.
        mResult = FlashClassification::Unknown;
      } else {
        mResult = FlashClassification::Denied;
      }
      return mResult;
    }
  }

  // Resolve the result based on mMatchedTables and aIsThirdParty.
  mResult = Resolve(aIsThirdParty);
  MOZ_ASSERT(FlashClassification::Unclassified != mResult);

  // The subsequent call of Result() will return the resolved result
  // and never reach here until Reset() is called.
  return mResult;
}

/*virtual*/ nsresult
PrincipalFlashClassifier::OnClassifyComplete(nsresult /*aErrorCode*/,
                                             const nsACString& aLists, // Only this matters.
                                             const nsACString& /*aProvider*/,
                                             const nsACString& /*aPrefix*/)
{
  mAsyncClassified = true;

  if (FlashClassification::Unclassified != mResult) {
    // Result() has been called prior to this callback.
    return NS_OK;
  }

  // TODO: Bug 1364804 - We should use a callback type which notifies
  // the result as a string array rather than a formatted string.

  // We only populate the matched list without resolving the classification
  // result because we are not sure if the parent doc has been properly set.
  // We also parse the comma-separated tables to array. (the code is copied
  // from Classifier::SplitTables.)
  nsACString::const_iterator begin, iter, end;
  aLists.BeginReading(begin);
  aLists.EndReading(end);
  while (begin != end) {
    iter = begin;
    FindCharInReadable(',', iter, end);
    nsDependentCSubstring table = Substring(begin,iter);
    if (!table.IsEmpty()) {
      mMatchedTables.AppendElement(Substring(begin, iter));
    }
    begin = iter;
    if (begin != end) {
      begin++;
    }
  }

  return NS_OK;
}

// We resolve the classification result based on aIsThirdParty
// and the matched tables we got ealier on (via either sync or async API).
FlashClassification
PrincipalFlashClassifier::Resolve(bool aIsThirdParty)
{
  MOZ_ASSERT(FlashClassification::Unclassified == mResult,
             "We already have resolved classification result.");

  if (mMatchedTables.IsEmpty()) {
    return FlashClassification::Unknown;
  }

  auto& prefs = GetPrefStore();
  if (ArrayContainsTable(mMatchedTables, prefs.mDenyTables) &&
      !ArrayContainsTable(mMatchedTables, prefs.mDenyExceptionsTables)) {
    return FlashClassification::Denied;
  } else if (ArrayContainsTable(mMatchedTables, prefs.mAllowTables) &&
             !ArrayContainsTable(mMatchedTables, prefs.mAllowExceptionsTables)) {
    return FlashClassification::Allowed;
  }

  if (aIsThirdParty && ArrayContainsTable(mMatchedTables, prefs.mSubDocDenyTables) &&
      !ArrayContainsTable(mMatchedTables, prefs.mSubDocDenyExceptionsTables)) {
    return FlashClassification::Denied;
  }

  return FlashClassification::Unknown;
}

void
PrincipalFlashClassifier::AsyncClassify(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(FlashClassification::Unclassified == mResult,
             "The old classification result should be reset first.");
  Reset();
  mResult = AsyncClassifyInternal(aPrincipal);
}

FlashClassification
PrincipalFlashClassifier::CheckIfClassifyNeeded(nsIPrincipal* aPrincipal)
{
  nsresult rv;

  auto& prefs = GetPrefStore();

  // If neither pref is on, skip the null-principal and principal URI checks.
  if (prefs.mPluginsHttpOnly && !prefs.mFlashBlockEnabled) {
   return FlashClassification::Unknown;
  }

  nsCOMPtr<nsIPrincipal> principal = aPrincipal;
  if (principal->GetIsNullPrincipal()) {
    return FlashClassification::Denied;
  }

  nsCOMPtr<nsIURI> classificationURI;
  rv = principal->GetURI(getter_AddRefs(classificationURI));
  if (NS_FAILED(rv) || !classificationURI) {
    return FlashClassification::Denied;
  }

  if (prefs.mPluginsHttpOnly) {
    // Only allow plugins for documents from an HTTP/HTTPS origin. This should
    // allow dependent data: URIs to load plugins, but not:
    // * chrome documents
    // * "bare" data: loads
    // * FTP/gopher/file
    nsAutoCString scheme;
    rv = classificationURI->GetScheme(scheme);
    if (NS_WARN_IF(NS_FAILED(rv)) ||
        !(scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https"))) {
      return FlashClassification::Denied;
    }
  }

  // If flash blocking is disabled, it is equivalent to all sites being
  // on neither list.
  if (!prefs.mFlashBlockEnabled) {
    return FlashClassification::Unknown;
  }

  return FlashClassification::Unclassified;
}

// Using nsIURIClassifier.asyncClassifyLocalWithTables to do classification
// against the flash related tables based on the given principal.
FlashClassification
PrincipalFlashClassifier::AsyncClassifyInternal(nsIPrincipal* aPrincipal)
{
  auto result = CheckIfClassifyNeeded(aPrincipal);
  if (FlashClassification::Unclassified != result) {
    return result;
  }

  // We haven't been able to decide if it's a third party document
  // since determining if a document is third-party may depend on its
  // parent document. At the time we call AsyncClassifyInternal
  // (i.e. StartDocumentLoad) the parent document may not have been
  // set. As a result, we wait until Resolve() to be called to
  // take "is third party" into account. At this point, we just assume
  // it's third-party to include every list.
  nsAutoCString tables;
  GetClassificationTables(true, tables);

  if (tables.IsEmpty()) {
    return FlashClassification::Unknown;
  }

  if (!EnsureUriClassifier()) {
    return FlashClassification::Denied;
  }

  nsresult rv = aPrincipal->GetURI(getter_AddRefs(mClassificationURI));
  if (NS_FAILED(rv) || !mClassificationURI) {
    return FlashClassification::Denied;
  }

  rv = mUriClassifier->AsyncClassifyLocalWithTables(mClassificationURI,
                                                    tables,
                                                    this);

  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      // This means that the URI had no hostname (ex: file://doc.html). In this
      // case, we allow the default (Unknown plugin) behavior.
      return FlashClassification::Unknown;
    } else {
      return FlashClassification::Denied;
    }
  }

  return FlashClassification::Unclassified;
}

FlashClassification
nsIDocument::ComputeFlashClassification()
{
  nsCOMPtr<nsIDocShellTreeItem> current = this->GetDocShell();
  if (!current) {
    return FlashClassification::Denied;
  }
  nsCOMPtr<nsIDocShellTreeItem> parent;
  DebugOnly<nsresult> rv = current->GetSameTypeParent(getter_AddRefs(parent));
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "nsIDocShellTreeItem::GetSameTypeParent should never fail");

  bool isTopLevel = !parent;
  FlashClassification classification;
  if (isTopLevel) {
    classification = PrincipalFlashClassification();
  } else {
    nsCOMPtr<nsIDocument> parentDocument = GetParentDocument();
    if (!parentDocument) {
      return FlashClassification::Denied;
    }
    FlashClassification parentClassification =
      parentDocument->DocumentFlashClassification();

    if (parentClassification == FlashClassification::Denied) {
      classification = FlashClassification::Denied;
    } else {
      classification = PrincipalFlashClassification();

      // Allow unknown children to inherit allowed status from parent, but
      // do not allow denied children to do so.
      if (classification == FlashClassification::Unknown &&
          parentClassification == FlashClassification::Allowed) {
        classification = FlashClassification::Allowed;
      }
    }
  }

  return classification;
}

/**
 * Retrieves the classification of plugins in this document. This is dependent
 * on the classification of this document and all parent documents.
 * This function is infallible - It must return some classification that
 * callers can act on.
 *
 * This function will NOT return FlashClassification::Unclassified
 */
FlashClassification
nsIDocument::DocumentFlashClassification()
{
  if (mFlashClassification == FlashClassification::Unclassified) {
    FlashClassification result = ComputeFlashClassification();
    mFlashClassification = result;
    MOZ_ASSERT(result != FlashClassification::Unclassified,
      "nsDocument::GetPluginClassification should never return Unclassified");
  }

  return mFlashClassification;
}

/**
 * Initializes |mIsThirdParty| if necessary and returns its value. The value
 * returned represents whether this document should be considered Third-Party.
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
bool
nsIDocument::IsThirdParty()
{
  if (mIsThirdParty.isSome()) {
    return mIsThirdParty.value();
  }

  nsCOMPtr<nsIDocShellTreeItem> docshell = this->GetDocShell();
  if (!docshell) {
    mIsThirdParty.emplace(true);
    return mIsThirdParty.value();
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  nsresult rv = docshell->GetSameTypeParent(getter_AddRefs(parent));
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "nsIDocShellTreeItem::GetSameTypeParent should never fail");
  bool isTopLevel = !parent;

  if (isTopLevel) {
    mIsThirdParty.emplace(false);
    return mIsThirdParty.value();
  }

  nsCOMPtr<nsIDocument> parentDocument = GetParentDocument();
  if (!parentDocument) {
    // Failure
    mIsThirdParty.emplace(true);
    return mIsThirdParty.value();
  }

  if (parentDocument->IsThirdParty()) {
    mIsThirdParty.emplace(true);
    return mIsThirdParty.value();
  }

  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(parentDocument,
                                                             &rv);
  if (NS_WARN_IF(NS_FAILED(rv) || !sop)) {
    // Failure
    mIsThirdParty.emplace(true);
    return mIsThirdParty.value();
  }
  nsCOMPtr<nsIPrincipal> parentPrincipal = sop->GetPrincipal();

  bool principalsMatch = false;
  rv = principal->Equals(parentPrincipal, &principalsMatch);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Failure
    mIsThirdParty.emplace(true);
    return mIsThirdParty.value();
  }

  if (!principalsMatch) {
    mIsThirdParty.emplace(true);
    return mIsThirdParty.value();
  }

  // Fall-through. Document is not a Third-Party Document.
  mIsThirdParty.emplace(false);
  return mIsThirdParty.value();
}

void
nsIDocument::ClearStaleServoData()
{
  DocumentStyleRootIterator iter(this);
  while (Element* root = iter.GetNextStyleRoot()) {
    RestyleManager::ClearServoDataFromSubtree(root);
  }
}

Selection*
nsIDocument::GetSelection(ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = GetInnerWindow();
  if (!window) {
    return nullptr;
  }

  if (!window->IsCurrentInnerWindow()) {
    return nullptr;
  }

  return nsGlobalWindowInner::Cast(window)->GetSelection(aRv);
}

void
nsIDocument::RecordNavigationTiming(ReadyState aReadyState)
{
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

bool
nsIDocument::ModuleScriptsEnabled()
{
  static bool sEnabledForContent = false;
  static bool sCachedPref = false;
  if (!sCachedPref) {
    sCachedPref = true;
    Preferences::AddBoolVarCache(&sEnabledForContent, "dom.moduleScripts.enabled", false);
  }

  return nsContentUtils::IsChromeDoc(this) || sEnabledForContent;
}

void
nsIDocument::ReportShadowDOMUsage()
{
  if (mHasReportedShadowDOMUsage) {
    return;
  }

  nsIDocument* topLevel = GetTopLevelContentDocument();
  if (topLevel && !topLevel->mHasReportedShadowDOMUsage) {
    topLevel->mHasReportedShadowDOMUsage = true;
    nsString uri;
    Unused << topLevel->GetDocumentURI(uri);
    if (!uri.IsEmpty()) {
      nsAutoString msg = NS_LITERAL_STRING("Shadow DOM used in [") + uri +
        NS_LITERAL_STRING("] or in some of its subdocuments.");
      nsContentUtils::ReportToConsoleNonLocalized(msg, nsIScriptError::infoFlag,
                                                  NS_LITERAL_CSTRING("DOM"),
                                                  topLevel);
    }
  }

  mHasReportedShadowDOMUsage = true;
}
