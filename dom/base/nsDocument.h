/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all our document implementations.
 */

#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsIDocument.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCRT.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsTArray.h"
#include "nsIdentifierMapEntry.h"
#include "nsIDOMDocument.h"
#include "nsStubDocumentObserver.h"
#include "nsIScriptGlobalObject.h"
#include "nsIContent.h"
#include "nsIPrincipal.h"
#include "nsIParser.h"
#include "nsBindingManager.h"
#include "nsRefPtrHashtable.h"
#include "nsJSThingHashtable.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsIRadioGroupContainer.h"
#include "nsILayoutHistoryState.h"
#include "nsIRequest.h"
#include "nsILoadGroup.h"
#include "nsTObserverArray.h"
#include "nsStubMutationObserver.h"
#include "nsIChannel.h"
#include "nsCycleCollectionParticipant.h"
#include "nsContentList.h"
#include "nsGkAtoms.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "mozilla/StyleSetHandle.h"
#include "PLDHashTable.h"
#include "nsAttrAndChildArray.h"
#include "nsDOMAttributeMap.h"
#include "nsIContentViewer.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsIProgressEventSink.h"
#include "nsISecurityEventSink.h"
#include "nsIChannelEventSink.h"
#include "imgIRequest.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/dom/BoxObject.h"
#include "mozilla/dom/DOMImplementation.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/StyleSheetList.h"
#include "nsDataHashtable.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "CustomElementRegistry.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/Maybe.h"
#include "nsIURIClassifier.h"

#define XML_DECLARATION_BITS_DECLARATION_EXISTS   (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS      (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS    (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES       (1 << 3)


class nsDOMStyleSheetSetList;
class nsDocument;
class nsIRadioVisitor;
class nsIFormControl;
struct nsRadioGroupStruct;
class nsOnloadBlocker;
class nsUnblockOnloadEvent;
class nsDOMNavigationTiming;
class nsWindowSizes;
class nsHtml5TreeOpExecutor;
class nsDocumentOnStack;
class nsISecurityConsoleMessage;

namespace mozilla {
class EventChainPreVisitor;
namespace dom {
class ImageTracker;
struct LifecycleCallbacks;
class CallbackFunction;
class DOMIntersectionObserver;
class Performance;

struct FullscreenRequest : public LinkedListElement<FullscreenRequest>
{
  explicit FullscreenRequest(Element* aElement);
  FullscreenRequest(const FullscreenRequest&) = delete;
  ~FullscreenRequest();

  Element* GetElement() const { return mElement; }
  nsIDocument* GetDocument() const { return mDocument; }

private:
  RefPtr<Element> mElement;
  RefPtr<nsIDocument> mDocument;

public:
  // This value should be true if the fullscreen request is
  // originated from chrome code.
  bool mIsCallerChrome = false;
  // This value denotes whether we should trigger a NewOrigin event if
  // requesting fullscreen in its document causes the origin which is
  // fullscreen to change. We may want *not* to trigger that event if
  // we're calling RequestFullScreen() as part of a continuation of a
  // request in a subdocument in different process, whereupon the caller
  // need to send some notification itself with the real origin.
  bool mShouldNotifyNewOrigin = true;
};

} // namespace dom
} // namespace mozilla

class nsOnloadBlocker final : public nsIRequest
{
public:
  nsOnloadBlocker() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST

private:
  ~nsOnloadBlocker() {}
};

class nsExternalResourceMap
{
public:
  typedef nsIDocument::ExternalResourceLoad ExternalResourceLoad;
  nsExternalResourceMap();

  /**
   * Request an external resource document.  This does exactly what
   * nsIDocument::RequestExternalResource is documented to do.
   */
  nsIDocument* RequestResource(nsIURI* aURI,
                               nsINode* aRequestingNode,
                               nsDocument* aDisplayDocument,
                               ExternalResourceLoad** aPendingLoad);

  /**
   * Enumerate the resource documents.  See
   * nsIDocument::EnumerateExternalResources.
   */
  void EnumerateResources(nsIDocument::nsSubDocEnumFunc aCallback, void* aData);

  /**
   * Traverse ourselves for cycle-collection
   */
  void Traverse(nsCycleCollectionTraversalCallback* aCallback) const;

  /**
   * Shut ourselves down (used for cycle-collection unlink), as well
   * as for document destruction.
   */
  void Shutdown()
  {
    mPendingLoads.Clear();
    mMap.Clear();
    mHaveShutDown = true;
  }

  bool HaveShutDown() const
  {
    return mHaveShutDown;
  }

  // Needs to be public so we can traverse them sanely
  struct ExternalResource
  {
    ~ExternalResource();
    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsIContentViewer> mViewer;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
  };

  // Hide all our viewers
  void HideViewers();

  // Show all our viewers
  void ShowViewers();

protected:
  class PendingLoad : public ExternalResourceLoad,
                      public nsIStreamListener
  {
    ~PendingLoad() {}

  public:
    explicit PendingLoad(nsDocument* aDisplayDocument) :
      mDisplayDocument(aDisplayDocument)
    {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    /**
     * Start aURI loading.  This will perform the necessary security checks and
     * so forth.
     */
    nsresult StartLoad(nsIURI* aURI, nsINode* aRequestingNode);

    /**
     * Set up an nsIContentViewer based on aRequest.  This is guaranteed to
     * put null in *aViewer and *aLoadGroup on all failures.
     */
    nsresult SetupViewer(nsIRequest* aRequest, nsIContentViewer** aViewer,
                         nsILoadGroup** aLoadGroup);

  private:
    RefPtr<nsDocument> mDisplayDocument;
    nsCOMPtr<nsIStreamListener> mTargetListener;
    nsCOMPtr<nsIURI> mURI;
  };
  friend class PendingLoad;

  class LoadgroupCallbacks final : public nsIInterfaceRequestor
  {
    ~LoadgroupCallbacks() {}
  public:
    explicit LoadgroupCallbacks(nsIInterfaceRequestor* aOtherCallbacks)
      : mCallbacks(aOtherCallbacks)
    {}
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
  private:
    // The only reason it's safe to hold a strong ref here without leaking is
    // that the notificationCallbacks on a loadgroup aren't the docshell itself
    // but a shim that holds a weak reference to the docshell.
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

    // Use shims for interfaces that docshell implements directly so that we
    // don't hand out references to the docshell.  The shims should all allow
    // getInterface back on us, but other than that each one should only
    // implement one interface.

    // XXXbz I wish we could just derive the _allcaps thing from _i
#define DECL_SHIM(_i, _allcaps)                                              \
    class _i##Shim final : public nsIInterfaceRequestor,                     \
                           public _i                                         \
    {                                                                        \
      ~_i##Shim() {}                                                         \
    public:                                                                  \
      _i##Shim(nsIInterfaceRequestor* aIfreq, _i* aRealPtr)                  \
        : mIfReq(aIfreq), mRealPtr(aRealPtr)                                 \
      {                                                                      \
        NS_ASSERTION(mIfReq, "Expected non-null here");                      \
        NS_ASSERTION(mRealPtr, "Expected non-null here");                    \
      }                                                                      \
      NS_DECL_ISUPPORTS                                                      \
      NS_FORWARD_NSIINTERFACEREQUESTOR(mIfReq->)                             \
      NS_FORWARD_##_allcaps(mRealPtr->)                                      \
    private:                                                                 \
      nsCOMPtr<nsIInterfaceRequestor> mIfReq;                                \
      nsCOMPtr<_i> mRealPtr;                                                 \
    };

    DECL_SHIM(nsILoadContext, NSILOADCONTEXT)
    DECL_SHIM(nsIProgressEventSink, NSIPROGRESSEVENTSINK)
    DECL_SHIM(nsIChannelEventSink, NSICHANNELEVENTSINK)
    DECL_SHIM(nsISecurityEventSink, NSISECURITYEVENTSINK)
    DECL_SHIM(nsIApplicationCacheContainer, NSIAPPLICATIONCACHECONTAINER)
#undef DECL_SHIM
  };

  /**
   * Add an ExternalResource for aURI.  aViewer and aLoadGroup might be null
   * when this is called if the URI didn't result in an XML document.  This
   * function makes sure to remove the pending load for aURI, if any, from our
   * hashtable, and to notify its observers, if any.
   */
  nsresult AddExternalResource(nsIURI* aURI, nsIContentViewer* aViewer,
                               nsILoadGroup* aLoadGroup,
                               nsIDocument* aDisplayDocument);

  nsClassHashtable<nsURIHashKey, ExternalResource> mMap;
  nsRefPtrHashtable<nsURIHashKey, PendingLoad> mPendingLoads;
  bool mHaveShutDown;
};

// Base class for our document implementations.
class nsDocument : public nsIDocument,
                   public nsIDOMDocument,
                   public nsSupportsWeakReference,
                   public nsIScriptObjectPrincipal,
                   public nsIRadioGroupContainer,
                   public nsIApplicationCacheContainer,
                   public nsStubMutationObserver
{
  friend class nsIDocument;

public:
  typedef mozilla::dom::Element Element;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  virtual void Reset(nsIChannel *aChannel, nsILoadGroup *aLoadGroup) override;
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                          nsIPrincipal* aPrincipal) override;

  already_AddRefed<nsIPrincipal> MaybeDowngradePrincipal(nsIPrincipal* aPrincipal);

  // StartDocumentLoad is pure virtual so that subclasses must override it.
  // The nsDocument StartDocumentLoad does some setup, but does NOT set
  // *aDocListener; this is the job of subclasses.
  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aContentSink = nullptr) override = 0;

  virtual void StopDocumentLoad() override;

  /**
   * Set the Content-Type of this document.
   */
  virtual void SetContentType(const nsAString& aContentType) override;

  /**
   * Set the document's character encoding. This will
   * trigger a startDocumentLoad if necessary to answer the question.
   */
  virtual void
    SetDocumentCharacterSet(NotNull<const Encoding*> aEncoding) override;

  /**
   * Create a new presentation shell that will use aContext for
   * its presentation context (presentation contexts <b>must not</b> be
   * shared among multiple presentation shells).
   */
  already_AddRefed<nsIPresShell> CreateShell(nsPresContext* aContext,
                                             nsViewManager* aViewManager,
                                             mozilla::StyleSetHandle aStyleSet)
    final;
  virtual void DeleteShell() override;

  static bool CallerIsTrustedAboutPage(JSContext* aCx, JSObject* aObject);
  static bool IsElementAnimateEnabled(JSContext* aCx, JSObject* aObject);
  static bool IsWebAnimationsEnabled(JSContext* aCx, JSObject* aObject);
  static bool IsWebAnimationsEnabled(mozilla::dom::CallerType aCallerType);
  virtual mozilla::dom::DocumentTimeline* Timeline() override;
  virtual void GetAnimations(
      nsTArray<RefPtr<mozilla::dom::Animation>>& aAnimations) override;
  mozilla::LinkedList<mozilla::dom::DocumentTimeline>& Timelines() override
  {
    return mTimelines;
  }

  virtual Element* GetRootElementInternal() const override;

  virtual nsIChannel* GetChannel() const override {
    return mChannel;
  }

  virtual nsIChannel* GetFailedChannel() const override {
    return mFailedChannel;
  }
  virtual void SetFailedChannel(nsIChannel* aChannel) override {
    mFailedChannel = aChannel;
  }

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject) override;

  /**
   * Get the script loader for this document
   */
  virtual mozilla::dom::ScriptLoader* ScriptLoader() override;

  virtual void EndUpdate(nsUpdateType aUpdateType) override;
  virtual void BeginLoad() override;
  virtual void EndLoad() override;

  virtual void FlushExternalResources(mozilla::FlushType aType) override;
  virtual void SetXMLDeclaration(const char16_t *aVersion,
                                 const char16_t *aEncoding,
                                 const int32_t aStandalone) override;
  virtual void GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& Standalone) override;

  virtual void OnPageShow(bool aPersisted, mozilla::dom::EventTarget* aDispatchStartTarget) override;
  virtual void OnPageHide(bool aPersisted, mozilla::dom::EventTarget* aDispatchStartTarget) override;

  virtual void WillDispatchMutationEvent(nsINode* aTarget) override;
  virtual void MutationEventDispatched(nsINode* aTarget) override;

  // nsINode
  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual nsIContent *GetChildAt_Deprecated(uint32_t aIndex) const override;
  virtual int32_t ComputeIndexOf(const nsINode* aPossibleChild) const override;
  virtual uint32_t GetChildCount() const override;
  virtual nsresult InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                     bool aNotify) override;
  virtual nsresult InsertChildAt_Deprecated(nsIContent* aKid, uint32_t aIndex,
                                            bool aNotify) override;
  virtual void RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify) override;
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName,
                            nsIRadioVisitor* aVisitor,
                            bool aFlushContent) override;
  virtual void
    SetCurrentRadioButton(const nsAString& aName,
                          mozilla::dom::HTMLInputElement* aRadio) override;
  virtual mozilla::dom::HTMLInputElement*
    GetCurrentRadioButton(const nsAString& aName) override;
  NS_IMETHOD
    GetNextRadioButton(const nsAString& aName,
                       const bool aPrevious,
                       mozilla::dom::HTMLInputElement*  aFocusedRadio,
                       mozilla::dom::HTMLInputElement** aRadioOut) override;
  virtual void AddToRadioGroup(const nsAString& aName,
                               mozilla::dom::HTMLInputElement* aRadio) override;
  virtual void RemoveFromRadioGroup(const nsAString& aName,
                                    mozilla::dom::HTMLInputElement* aRadio) override;
  virtual uint32_t GetRequiredRadioCount(const nsAString& aName) const override;
  virtual void RadioRequiredWillChange(const nsAString& aName,
                                       bool aRequiredAdded) override;
  virtual bool GetValueMissingState(const nsAString& aName) const override;
  virtual void SetValueMissingState(const nsAString& aName, bool aValue) override;

  // for radio group
  nsRadioGroupStruct* GetRadioGroup(const nsAString& aName) const;
  nsRadioGroupStruct* GetOrCreateRadioGroup(const nsAString& aName);

  virtual nsViewportInfo GetViewportInfo(const mozilla::ScreenIntSize& aDisplaySize) override;

  enum class UseCounterReportKind {
    // Flush the document's use counters only; the use counters for any
    // external resource documents will be flushed when the external
    // resource documents themselves are destroyed.
    eDefault,

    // Flush use counters for the document and for its external resource
    // documents. (Should only be necessary for tests, where we need
    // flushing to happen synchronously and deterministically.)
    eIncludeExternalResources,
  };

  void ReportUseCounters(UseCounterReportKind aKind = UseCounterReportKind::eDefault);

  bool IsSynthesized();

  // Check whether shadow DOM is enabled for the global of aObject.
  static bool IsShadowDOMEnabled(JSContext* aCx, JSObject* aObject);
  // Check whether shadow DOM is enabled for the document this node belongs to.
  static bool IsShadowDOMEnabled(const nsINode* aNode);
private:
  void SendToConsole(nsCOMArray<nsISecurityConsoleMessage>& aMessages);

public:
  // nsIDOMDocument
  NS_DECL_NSIDOMDOCUMENT

  using mozilla::dom::DocumentOrShadowRoot::GetElementById;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagName;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagNameNS;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByClassName;

  // nsIDOMEventTarget
  virtual nsresult GetEventTargetParent(
                     mozilla::EventChainPreVisitor& aVisitor) override;
  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() override;
  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const override;

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal() override;

  // nsIApplicationCacheContainer
  NS_DECL_NSIAPPLICATIONCACHECONTAINER

  virtual nsresult Init();

  virtual already_AddRefed<Element> CreateElem(const nsAString& aName,
                                               nsAtom* aPrefix,
                                               int32_t aNamespaceID,
                                               const nsAString* aIs = nullptr) override;

  virtual void Sanitize() override;

  virtual void EnumerateSubDocuments(nsSubDocEnumFunc aCallback,
                                                 void *aData) override;
  virtual void CollectDescendantDocuments(
    nsTArray<nsCOMPtr<nsIDocument>>& aDescendants,
    nsDocTestFunc aCallback) const override;

  virtual bool CanSavePresentation(nsIRequest *aNewRequest) override;
  virtual void Destroy() override;
  virtual void RemovedFromDocShell() override;
  virtual already_AddRefed<nsILayoutHistoryState> GetLayoutHistoryState() const override;

  virtual void BlockOnload() override;
  virtual void UnblockOnload(bool aFireSync) override;

  virtual void ClearBoxObjectFor(nsIContent* aContent) override;

  virtual already_AddRefed<mozilla::dom::BoxObject>
  GetBoxObjectFor(mozilla::dom::Element* aElement,
                  mozilla::ErrorResult& aRv) override;

  virtual Element*
    GetAnonymousElementByAttribute(nsIContent* aElement,
                                   nsAtom* aAttrName,
                                   const nsAString& aAttrValue) const override;

  virtual nsresult NodesFromRectHelper(float aX, float aY,
                                                   float aTopSize, float aRightSize,
                                                   float aBottomSize, float aLeftSize,
                                                   bool aIgnoreRootScrollFrame,
                                                   bool aFlushLayout,
                                                   nsIDOMNodeList** aReturn) override;

  virtual void FlushSkinBindings() override;

  virtual nsresult InitializeFrameLoader(nsFrameLoader* aLoader) override;
  virtual nsresult FinalizeFrameLoader(nsFrameLoader* aLoader, nsIRunnable* aFinalizer) override;
  virtual void TryCancelFrameLoaderInitialization(nsIDocShell* aShell) override;
  virtual nsIDocument*
    RequestExternalResource(nsIURI* aURI,
                            nsINode* aRequestingNode,
                            ExternalResourceLoad** aPendingLoad) override;
  virtual void
    EnumerateExternalResources(nsSubDocEnumFunc aCallback, void* aData) override;

  // Returns our (lazily-initialized) animation controller.
  // If HasAnimationController is true, this is guaranteed to return non-null.
  nsSMILAnimationController* GetAnimationController() override;

  virtual mozilla::PendingAnimationTracker*
  GetPendingAnimationTracker() final
  {
    return mPendingAnimationTracker;
  }

  virtual mozilla::PendingAnimationTracker*
  GetOrCreatePendingAnimationTracker() override;

  virtual void SuppressEventHandling(uint32_t aIncrease) override;

  virtual void UnsuppressEventHandlingAndFireEvents(bool aFireEvents) override;

  void DecreaseEventSuppression() {
    MOZ_ASSERT(mEventsSuppressed);
    --mEventsSuppressed;
    UpdateFrameRequestCallbackSchedulingState();
  }

  virtual nsIDocument* GetTemplateContentsOwner() override;

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDocument,
                                                                   nsIDocument)

  nsExternalResourceMap& ExternalResourceMap()
  {
    return mExternalResourceMap;
  }

  void SetLoadedAsData(bool aLoadedAsData) { mLoadedAsData = aLoadedAsData; }
  void SetLoadedAsInteractiveData(bool aLoadedAsInteractiveData)
  {
    mLoadedAsInteractiveData = aLoadedAsInteractiveData;
  }

  nsresult CloneDocHelper(nsDocument* clone, bool aPreallocateChildren) const;

  void MaybeInitializeFinalizeFrameLoaders();

  void MaybeEndOutermostXBLUpdate();

  virtual void PreloadPictureOpened() override;
  virtual void PreloadPictureClosed() override;

  virtual void
    PreloadPictureImageSource(const nsAString& aSrcsetAttr,
                              const nsAString& aSizesAttr,
                              const nsAString& aTypeAttr,
                              const nsAString& aMediaAttr) override;

  virtual already_AddRefed<nsIURI>
    ResolvePreloadImage(nsIURI *aBaseURI,
                        const nsAString& aSrcAttr,
                        const nsAString& aSrcsetAttr,
                        const nsAString& aSizesAttr,
                        bool *aIsImgSet) override;

  virtual void MaybePreLoadImage(nsIURI* uri,
                                 const nsAString &aCrossOriginAttr,
                                 ReferrerPolicy aReferrerPolicy,
                                 bool aIsImgSet) override;

  virtual void ForgetImagePreload(nsIURI* aURI) override;

  virtual void MaybePreconnect(nsIURI* uri,
                               mozilla::CORSMode aCORSMode) override;

  virtual nsISupports* GetCurrentContentSink() override;

  // Only BlockOnload should call this!
  void AsyncBlockOnload();

  virtual void SetScrollToRef(nsIURI *aDocumentURI) override;
  virtual void ScrollToRef() override;
  virtual void ResetScrolledToRefAlready() override;
  virtual void SetChangeScrollPosWhenScrollingToRef(bool aValue) override;

  // AddPlugin adds a plugin-related element to mPlugins when the element is
  // added to the tree.
  virtual nsresult AddPlugin(nsIObjectLoadingContent* aPlugin) override;
  // RemovePlugin removes a plugin-related element to mPlugins when the
  // element is removed from the tree.
  virtual void RemovePlugin(nsIObjectLoadingContent* aPlugin) override;
  // GetPlugins returns the plugin-related elements from
  // the frame and any subframes.
  virtual void GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins) override;

  // Returns the size of the mBlockedTrackingNodes array. (nsIDocument.h)
  //
  // This array contains nodes that have been blocked to prevent
  // user tracking. They most likely have had their nsIChannel
  // canceled by the URL classifier (Safebrowsing).
  //
  // A script can subsequently use GetBlockedTrackingNodes()
  // to get a list of references to these nodes.
  //
  // Note:
  // This expresses how many tracking nodes have been blocked for this
  // document since its beginning, not how many of them are still around
  // in the DOM tree. Weak references to blocked nodes are added in the
  // mBlockedTrackingNodesArray but they are not removed when those nodes
  // are removed from the tree or even garbage collected.
  long BlockedTrackingNodeCount() const;

  //
  // Returns strong references to mBlockedTrackingNodes. (nsIDocument.h)
  //
  // This array contains nodes that have been blocked to prevent
  // user tracking. They most likely have had their nsIChannel
  // canceled by the URL classifier (Safebrowsing).
  //
  already_AddRefed<nsSimpleContentList> BlockedTrackingNodes() const;

  void RequestPointerLock(Element* aElement,
                          mozilla::dom::CallerType aCallerType) override;
  bool SetPointerLock(Element* aElement, int aCursorStyle);
  static void UnlockPointer(nsIDocument* aDoc = nullptr);

  void SetCurrentOrientation(mozilla::dom::OrientationType aType,
                             uint16_t aAngle) override;
  uint16_t CurrentOrientationAngle() const override;
  mozilla::dom::OrientationType CurrentOrientationType() const override;
  void SetOrientationPendingPromise(mozilla::dom::Promise* aPromise) override;
  mozilla::dom::Promise* GetOrientationPendingPromise() const override;

  virtual void DocAddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const override;
  // DocAddSizeOfIncludingThis is inherited from nsIDocument.

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  virtual void UnblockDOMContentLoaded() override;

protected:
  friend class nsNodeUtils;

  void DispatchContentLoadedEvents();

  void RetrieveRelevantHeaders(nsIChannel *aChannel);

  void TryChannelCharset(nsIChannel *aChannel,
                         int32_t& aCharsetSource,
                         NotNull<const Encoding*>& aEncoding,
                         nsHtml5TreeOpExecutor* aExecutor);

  // Call this before the document does something that will unbind all content.
  // That will stop us from doing a lot of work as each element is removed.
  void DestroyElementMaps();

  nsIContent* GetFirstBaseNodeWithHref();
  nsresult SetFirstBaseNodeWithHref(nsIContent *node);

public:

  bool ContainsEMEContent();

  bool ContainsMSEContent();

protected:

  void DispatchPageTransition(mozilla::dom::EventTarget* aDispatchTarget,
                              const nsAString& aType,
                              bool aPersisted);

  void UpdateScreenOrientation();

#define NS_DOCUMENT_NOTIFY_OBSERVERS(func_, params_) do {                     \
    NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers, nsIDocumentObserver, \
                                             func_, params_);                 \
    /* FIXME(emilio): Apparently we can keep observing from the BFCache? That \
       looks bogus. */                                                        \
    if (nsIPresShell* shell = GetObservingShell()) {                          \
      shell->func_ params_;                                                   \
    }                                                                         \
  } while(0)

#ifdef DEBUG
  void VerifyRootContentState();
#endif

  explicit nsDocument(const char* aContentType);
  virtual ~nsDocument();

  void EnsureOnloadBlocker();

  // Array of owning references to all children
  nsAttrAndChildArray mChildren;

  // Tracker for animations that are waiting to start.
  // nullptr until GetOrCreatePendingAnimationTracker is called.
  RefPtr<mozilla::PendingAnimationTracker> mPendingAnimationTracker;

public:
  RefPtr<mozilla::EventListenerManager> mListenerManager;
  RefPtr<mozilla::dom::ScriptLoader> mScriptLoader;

  nsClassHashtable<nsStringHashKey, nsRadioGroupStruct> mRadioGroups;

  bool mHasWarnedAboutBoxObjects:1;

  bool mDelayFrameLoaderInitialization:1;

  bool mSynchronousDOMContentLoaded:1;

  // Parser aborted. True if the parser of this document was forcibly
  // terminated instead of letting it finish at its own pace.
  bool mParserAborted:1;

  friend class nsCallRequestFullScreen;

  // ScreenOrientation "pending promise" as described by
  // http://www.w3.org/TR/screen-orientation/
  RefPtr<mozilla::dom::Promise> mOrientationPendingPromise;

  uint16_t mCurrentOrientationAngle;
  mozilla::dom::OrientationType mCurrentOrientationType;

  // Whether we have reported use counters for this document with Telemetry yet.
  // Normally this is only done at document destruction time, but for image
  // documents (SVG documents) that are not guaranteed to be destroyed, we
  // report use counters when the image cache no longer has any imgRequestProxys
  // pointing to them.  We track whether we ever reported use counters so
  // that we only report them once for the document.
  bool mReportedUseCounters:1;

  uint8_t mXMLDeclarationBits;

  nsRefPtrHashtable<nsPtrHashKey<nsIContent>, mozilla::dom::BoxObject>* mBoxObjectTable;

  // A document "without a browsing context" that owns the content of
  // HTMLTemplateElement.
  nsCOMPtr<nsIDocument> mTemplateContentsOwner;

  // The application cache that this document is associated with, if
  // any.  This can change during the lifetime of the document.
  nsCOMPtr<nsIApplicationCache> mApplicationCache;

  nsCOMPtr<nsIContent> mFirstBaseNodeWithHref;
private:
  friend class nsUnblockOnloadEvent;

  void PostUnblockOnloadEvent();
  void DoUnblockOnload();

  nsresult InitCSP(nsIChannel* aChannel);

  void ClearAllBoxObjects();

  // Returns true if the scheme for the url for this document is "about"
  bool IsAboutPage() const;

  // These are not implemented and not supported.
  nsDocument(const nsDocument& aOther);
  nsDocument& operator=(const nsDocument& aOther);

  // The layout history state that should be used by nodes in this
  // document.  We only actually store a pointer to it when:
  // 1)  We have no script global object.
  // 2)  We haven't had Destroy() called on us yet.
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;

  // Currently active onload blockers
  uint32_t mOnloadBlockCount;
  // Onload blockers which haven't been activated yet
  uint32_t mAsyncOnloadBlockCount;
  nsCOMPtr<nsIRequest> mOnloadBlocker;

  nsTArray<RefPtr<nsFrameLoader> > mInitializableFrameLoaders;
  nsTArray<nsCOMPtr<nsIRunnable> > mFrameLoaderFinalizers;
  RefPtr<nsRunnableMethod<nsDocument> > mFrameLoaderRunner;

  nsCOMPtr<nsIRunnable> mMaybeEndOutermostXBLUpdateRunner;

  nsExternalResourceMap mExternalResourceMap;

  // All images in process of being preloaded.  This is a hashtable so
  // we can remove them as the real image loads start; that way we
  // make sure to not keep the image load going when no one cares
  // about it anymore.
  nsRefPtrHashtable<nsURIHashKey, imgIRequest> mPreloadingImages;

  // A list of preconnects initiated by the preloader. This prevents
  // the same uri from being used more than once, and allows the dom
  // builder to not repeat the work of the preloader.
  nsDataHashtable<nsURIHashKey, bool> mPreloadedPreconnects;

  // Current depth of picture elements from parser
  int32_t mPreloadPictureDepth;

  // Set if we've found a URL for the current picture
  nsString mPreloadPictureFoundSource;

  nsCString mScrollToRef;
  uint8_t mScrolledToRefAlready : 1;
  uint8_t mChangeScrollPosWhenScrollingToRef : 1;

  // Tracking for plugins in the document.
  nsTHashtable< nsPtrHashKey<nsIObjectLoadingContent> > mPlugins;

  RefPtr<mozilla::dom::DocumentTimeline> mDocumentTimeline;
  mozilla::LinkedList<mozilla::dom::DocumentTimeline> mTimelines;

  // These member variables cache information about the viewport so we don't have to
  // recalculate it each time.
  bool mValidWidth, mValidHeight;
  mozilla::LayoutDeviceToScreenScale mScaleMinFloat;
  mozilla::LayoutDeviceToScreenScale mScaleMaxFloat;
  mozilla::LayoutDeviceToScreenScale mScaleFloat;
  mozilla::CSSToLayoutDeviceScale mPixelRatio;
  bool mAutoSize, mAllowZoom, mAllowDoubleTapZoom, mValidScaleFloat, mValidMaxScale, mScaleStrEmpty, mWidthStrEmpty;
  mozilla::CSSSize mViewportSize;

  // Set to true when the document is possibly controlled by the ServiceWorker.
  // Used to prevent multiple requests to ServiceWorkerManager.
  bool mMaybeServiceWorkerControlled;

#ifdef DEBUG
public:
  bool mWillReparent;
#endif
};

class nsDocumentOnStack
{
public:
  explicit nsDocumentOnStack(nsIDocument* aDoc) : mDoc(aDoc)
  {
    mDoc->IncreaseStackRefCnt();
  }
  ~nsDocumentOnStack()
  {
    mDoc->DecreaseStackRefCnt();
  }
private:
  nsIDocument* mDoc;
};

#endif /* nsDocument_h___ */
