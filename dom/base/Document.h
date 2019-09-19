/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_Document_h___
#define mozilla_dom_Document_h___

#include "mozilla/EventStates.h"  // for EventStates
#include "mozilla/FlushType.h"    // for enum
#include "mozilla/MozPromise.h"   // for MozPromise
#include "mozilla/Pair.h"         // for Pair
#include "mozilla/Saturate.h"     // for SaturateUint32
#include "nsAutoPtr.h"            // for member
#include "nsCOMArray.h"           // for member
#include "nsCompatibility.h"      // for member
#include "nsCOMPtr.h"             // for member
#include "nsICookieSettings.h"
#include "nsGkAtoms.h"  // for static class members
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIContentViewer.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"  // for member (in nsCOMPtr)
#include "nsINode.h"       // for base class
#include "nsIParser.h"
#include "nsIChannelEventSink.h"
#include "nsIProgressEventSink.h"
#include "nsIRadioGroupContainer.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptGlobalObject.h"  // for member (in nsCOMPtr)
#include "nsIServiceManager.h"
#include "nsIURI.h"  // for use in inline functions
#include "nsIUUIDGenerator.h"
#include "nsIWebProgressListener.h"  // for nsIWebProgressListener
#include "nsIWeakReferenceUtils.h"   // for nsWeakPtr
#include "nsPIDOMWindow.h"           // for use in inline functions
#include "nsPropertyTable.h"         // for member
#include "nsStringFwd.h"
#include "nsStubMutationObserver.h"
#include "nsTHashtable.h"  // for member
#include "nsURIHashKey.h"
#include "mozilla/UseCounter.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/StaticPresData.h"
#include "Units.h"
#include "nsContentListDeclarations.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"
#include "ReferrerInfo.h"
#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentBlockingLog.h"
#include "mozilla/dom/DispatcherTrait.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/dom/ViewportMetaData.h"
#include "mozilla/HashTable.h"
#include "mozilla/LinkedList.h"
#include "mozilla/NotNull.h"
#include "mozilla/SegmentedVector.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/FailedCertSecurityInfoBinding.h"
#include "mozilla/dom/NetErrorInfoBinding.h"
#include <bitset>  // for member

// windows.h #defines CreateEvent
#ifdef CreateEvent
#  undef CreateEvent
#endif

#ifdef MOZILLA_INTERNAL_API
#  include "mozilla/dom/DocumentBinding.h"
#else
namespace mozilla {
namespace dom {
class ElementCreationOptionsOrString;
}  // namespace dom
}  // namespace mozilla
#endif  // MOZILLA_INTERNAL_API

class gfxUserFontSet;
class imgIRequest;
class nsBindingManager;
class nsCachableElementsByNameNodeList;
class nsContentList;
class nsIDocShell;
class nsDocShell;
class nsDOMNavigationTiming;
class nsFrameLoader;
class nsGlobalWindowInner;
class nsHtml5TreeOpExecutor;
class nsHTMLCSSStyleSheet;
class nsHTMLDocument;
class nsHTMLStyleSheet;
class nsGenericHTMLElement;
class nsAtom;
class nsIBFCacheEntry;
class nsIChannel;
class nsIContent;
class nsIContentSecurityPolicy;
class nsIContentSink;
class nsIDocShell;
class nsIDocShellTreeItem;
class nsIDocumentEncoder;
class nsIDocumentObserver;
class nsIHTMLCollection;
class nsILayoutHistoryState;
class nsILoadContext;
class nsIObjectLoadingContent;
class nsIObserver;
class nsIPrincipal;
class nsIRequest;
class nsIRunnable;
class nsISecurityConsoleMessage;
class nsIStreamListener;
class nsIStructuredCloneContainer;
class nsIURI;
class nsIVariant;
class nsViewManager;
class nsPresContext;
class nsRange;
class nsSimpleContentList;
class nsTextNode;
class nsWindowSizes;
class nsDOMCaretPosition;
class nsViewportInfo;
class nsIGlobalObject;
class nsIXULWindow;
class nsXULPrototypeDocument;
class nsXULPrototypeElement;
struct nsFont;

namespace mozilla {
class AbstractThread;
class CSSStyleSheet;
class EditorCommand;
class Encoding;
class ErrorResult;
class EventStates;
class EventListenerManager;
class FullscreenExit;
class FullscreenRequest;
class PendingAnimationTracker;
class PresShell;
class ServoStyleSet;
class SMILAnimationController;
enum class StyleCursorKind : uint8_t;
template <typename>
class OwningNonNull;
struct URLExtraData;

namespace css {
class Loader;
class ImageLoader;
class Rule;
}  // namespace css

namespace dom {
class Animation;
class AnonymousContent;
class Attr;
class XULBroadcastManager;
class XULPersist;
class ClientInfo;
class ClientState;
class CDATASection;
class Comment;
struct CustomElementDefinition;
class DocGroup;
class DocumentL10n;
class DocumentFragment;
class DocumentTimeline;
class DocumentType;
class DOMImplementation;
class DOMIntersectionObserver;
class DOMStringList;
class Element;
struct ElementCreationOptions;
class Event;
class EventTarget;
class FeaturePolicy;
class FontFaceSet;
class FrameRequestCallback;
class ImageTracker;
class HTMLAllCollection;
class HTMLBodyElement;
class HTMLMetaElement;
class HTMLSharedElement;
class HTMLImageElement;
struct LifecycleCallbackArgs;
class Link;
class Location;
class MediaQueryList;
class GlobalObject;
class NodeFilter;
class NodeIterator;
enum class OrientationType : uint8_t;
class ProcessingInstruction;
class Promise;
class ScriptLoader;
class Selection;
class ServiceWorkerDescriptor;
class StyleSheetList;
class SVGDocument;
class SVGElement;
class SVGSVGElement;
class SVGUseElement;
class Touch;
class TouchList;
class TreeWalker;
class XPathEvaluator;
class XPathExpression;
class XPathNSResolver;
class XPathResult;
template <typename>
class Sequence;

class nsDocumentOnStack;
class nsUnblockOnloadEvent;

template <typename, typename>
class CallbackObjectHolder;

enum class CallerType : uint32_t;

enum BFCacheStatus {
  NOT_ALLOWED = 1 << 0,                  // Status 0
  EVENT_HANDLING_SUPPRESSED = 1 << 1,    // Status 1
  SUSPENDED = 1 << 2,                    // Status 2
  UNLOAD_LISTENER = 1 << 3,              // Status 3
  REQUEST = 1 << 4,                      // Status 4
  ACTIVE_GET_USER_MEDIA = 1 << 5,        // Status 5
  ACTIVE_PEER_CONNECTION = 1 << 6,       // Status 6
  CONTAINS_EME_CONTENT = 1 << 7,         // Status 7
  CONTAINS_MSE_CONTENT = 1 << 8,         // Status 8
  HAS_ACTIVE_SPEECH_SYNTHESIS = 1 << 9,  // Status 9
  HAS_USED_VR = 1 << 10,                 // Status 10
};

}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace net {
class ChannelEventQueue;
}  // namespace net
}  // namespace mozilla

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_IDOCUMENT_IID                             \
  {                                                  \
    0xce1f7627, 0x7109, 0x4977, {                    \
      0xba, 0x77, 0x49, 0x0f, 0xfd, 0xe0, 0x7a, 0xaa \
    }                                                \
  }

namespace mozilla {
namespace dom {

class Document;
class DOMStyleSheetSetList;
class ResizeObserver;
class ResizeObserverController;
class PostMessageEvent;

// Document states

// RTL locale: specific to the XUL localedir attribute
#define NS_DOCUMENT_STATE_RTL_LOCALE NS_DEFINE_EVENT_STATE_MACRO(0)
// Window activation status
#define NS_DOCUMENT_STATE_WINDOW_INACTIVE NS_DEFINE_EVENT_STATE_MACRO(1)

class DocHeaderData {
 public:
  DocHeaderData(nsAtom* aField, const nsAString& aData)
      : mField(aField), mData(aData), mNext(nullptr) {}

  ~DocHeaderData(void) { delete mNext; }

  RefPtr<nsAtom> mField;
  nsString mData;
  DocHeaderData* mNext;
};

class ExternalResourceMap {
  typedef bool (*SubDocEnumFunc)(Document* aDocument, void* aData);

 public:
  /**
   * A class that represents an external resource load that has begun but
   * doesn't have a document yet.  Observers can be registered on this object,
   * and will be notified after the document is created.  Observers registered
   * after the document has been created will NOT be notified.  When observers
   * are notified, the subject will be the newly-created document, the topic
   * will be "external-resource-document-created", and the data will be null.
   * If document creation fails for some reason, observers will still be
   * notified, with a null document pointer.
   */
  class ExternalResourceLoad : public nsISupports {
   public:
    virtual ~ExternalResourceLoad() = default;

    void AddObserver(nsIObserver* aObserver) {
      MOZ_ASSERT(aObserver, "Must have observer");
      mObservers.AppendElement(aObserver);
    }

    const nsTArray<nsCOMPtr<nsIObserver>>& Observers() { return mObservers; }

   protected:
    AutoTArray<nsCOMPtr<nsIObserver>, 8> mObservers;
  };

  ExternalResourceMap();

  /**
   * Request an external resource document.  This does exactly what
   * Document::RequestExternalResource is documented to do.
   */
  Document* RequestResource(nsIURI* aURI, nsIReferrerInfo* aReferrerInfo,
                            nsINode* aRequestingNode,
                            Document* aDisplayDocument,
                            ExternalResourceLoad** aPendingLoad);

  /**
   * Enumerate the resource documents.  See
   * Document::EnumerateExternalResources.
   */
  void EnumerateResources(SubDocEnumFunc aCallback, void* aData);

  /**
   * Traverse ourselves for cycle-collection
   */
  void Traverse(nsCycleCollectionTraversalCallback* aCallback) const;

  /**
   * Shut ourselves down (used for cycle-collection unlink), as well
   * as for document destruction.
   */
  void Shutdown() {
    mPendingLoads.Clear();
    mMap.Clear();
    mHaveShutDown = true;
  }

  bool HaveShutDown() const { return mHaveShutDown; }

  // Needs to be public so we can traverse them sanely
  struct ExternalResource {
    ~ExternalResource();
    RefPtr<Document> mDocument;
    nsCOMPtr<nsIContentViewer> mViewer;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
  };

  // Hide all our viewers
  void HideViewers();

  // Show all our viewers
  void ShowViewers();

 protected:
  class PendingLoad : public ExternalResourceLoad, public nsIStreamListener {
    ~PendingLoad() {}

   public:
    explicit PendingLoad(Document* aDisplayDocument)
        : mDisplayDocument(aDisplayDocument) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    /**
     * Start aURI loading.  This will perform the necessary security checks and
     * so forth.
     */
    nsresult StartLoad(nsIURI* aURI, nsIReferrerInfo* aReferrerInfo,
                       nsINode* aRequestingNode);
    /**
     * Set up an nsIContentViewer based on aRequest.  This is guaranteed to
     * put null in *aViewer and *aLoadGroup on all failures.
     */
    nsresult SetupViewer(nsIRequest* aRequest, nsIContentViewer** aViewer,
                         nsILoadGroup** aLoadGroup);

   private:
    RefPtr<Document> mDisplayDocument;
    nsCOMPtr<nsIStreamListener> mTargetListener;
    nsCOMPtr<nsIURI> mURI;
  };
  friend class PendingLoad;

  class LoadgroupCallbacks final : public nsIInterfaceRequestor {
    ~LoadgroupCallbacks() {}

   public:
    explicit LoadgroupCallbacks(nsIInterfaceRequestor* aOtherCallbacks)
        : mCallbacks(aOtherCallbacks) {}
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
#define DECL_SHIM(_i, _allcaps)                                    \
  class _i##Shim final : public nsIInterfaceRequestor, public _i { \
    ~_i##Shim() {}                                                 \
                                                                   \
   public:                                                         \
    _i##Shim(nsIInterfaceRequestor* aIfreq, _i* aRealPtr)          \
        : mIfReq(aIfreq), mRealPtr(aRealPtr) {                     \
      NS_ASSERTION(mIfReq, "Expected non-null here");              \
      NS_ASSERTION(mRealPtr, "Expected non-null here");            \
    }                                                              \
    NS_DECL_ISUPPORTS                                              \
    NS_FORWARD_NSIINTERFACEREQUESTOR(mIfReq->)                     \
    NS_FORWARD_##_allcaps(mRealPtr->) private                      \
        : nsCOMPtr<nsIInterfaceRequestor> mIfReq;                  \
    nsCOMPtr<_i> mRealPtr;                                         \
  };

    DECL_SHIM(nsILoadContext, NSILOADCONTEXT)
    DECL_SHIM(nsIProgressEventSink, NSIPROGRESSEVENTSINK)
    DECL_SHIM(nsIChannelEventSink, NSICHANNELEVENTSINK)
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
                               Document* aDisplayDocument);

  nsClassHashtable<nsURIHashKey, ExternalResource> mMap;
  nsRefPtrHashtable<nsURIHashKey, PendingLoad> mPendingLoads;
  bool mHaveShutDown;
};

//----------------------------------------------------------------------

// Document interface.  This is implemented by all document objects in
// Gecko.
class Document : public nsINode,
                 public DocumentOrShadowRoot,
                 public nsSupportsWeakReference,
                 public nsIRadioGroupContainer,
                 public nsIScriptObjectPrincipal,
                 public nsIApplicationCacheContainer,
                 public nsStubMutationObserver,
                 public DispatcherTrait,
                 public SupportsWeakPtr<Document> {
 protected:
  explicit Document(const char* aContentType);
  virtual ~Document();

  Document(const Document&) = delete;
  Document& operator=(const Document&) = delete;

 public:
  typedef dom::ExternalResourceMap::ExternalResourceLoad ExternalResourceLoad;
  typedef dom::ReferrerPolicy ReferrerPolicyEnum;

  /**
   * Called when XPCOM shutdown.
   */
  static void Shutdown();

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(Document)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Document,
                                                                   nsINode)

#define NS_DOCUMENT_NOTIFY_OBSERVERS(func_, params_)                          \
  do {                                                                        \
    NS_OBSERVER_ARRAY_NOTIFY_XPCOM_OBSERVERS(mObservers, nsIDocumentObserver, \
                                             func_, params_);                 \
    /* FIXME(emilio): Apparently we can keep observing from the BFCache? That \
       looks bogus. */                                                        \
    if (PresShell* presShell = GetObservingPresShell()) {                     \
      presShell->func_ params_;                                               \
    }                                                                         \
  } while (0)

  // nsIApplicationCacheContainer
  NS_DECL_NSIAPPLICATIONCACHECONTAINER

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor,
                            bool aFlushContent) final {
    return DocumentOrShadowRoot::WalkRadioGroup(aName, aVisitor, aFlushContent);
  }

  void SetCurrentRadioButton(const nsAString& aName,
                             HTMLInputElement* aRadio) final {
    DocumentOrShadowRoot::SetCurrentRadioButton(aName, aRadio);
  }

  HTMLInputElement* GetCurrentRadioButton(const nsAString& aName) final {
    return DocumentOrShadowRoot::GetCurrentRadioButton(aName);
  }

  NS_IMETHOD
  GetNextRadioButton(const nsAString& aName, const bool aPrevious,
                     HTMLInputElement* aFocusedRadio,
                     HTMLInputElement** aRadioOut) final {
    return DocumentOrShadowRoot::GetNextRadioButton(aName, aPrevious,
                                                    aFocusedRadio, aRadioOut);
  }
  void AddToRadioGroup(const nsAString& aName, HTMLInputElement* aRadio) final {
    DocumentOrShadowRoot::AddToRadioGroup(aName, aRadio);
  }
  void RemoveFromRadioGroup(const nsAString& aName,
                            HTMLInputElement* aRadio) final {
    DocumentOrShadowRoot::RemoveFromRadioGroup(aName, aRadio);
  }
  uint32_t GetRequiredRadioCount(const nsAString& aName) const final {
    return DocumentOrShadowRoot::GetRequiredRadioCount(aName);
  }
  void RadioRequiredWillChange(const nsAString& aName,
                               bool aRequiredAdded) final {
    DocumentOrShadowRoot::RadioRequiredWillChange(aName, aRequiredAdded);
  }
  bool GetValueMissingState(const nsAString& aName) const final {
    return DocumentOrShadowRoot::GetValueMissingState(aName);
  }
  void SetValueMissingState(const nsAString& aName, bool aValue) final {
    return DocumentOrShadowRoot::SetValueMissingState(aName, aValue);
  }

  nsIPrincipal* EffectiveStoragePrincipal() const;

  // nsIScriptObjectPrincipal
  nsIPrincipal* GetPrincipal() final { return NodePrincipal(); }

  nsIPrincipal* GetEffectiveStoragePrincipal() final {
    return EffectiveStoragePrincipal();
  }

  // You should probably not be using this function, since it performs no
  // checks to ensure that the intrinsic storage principal should really be
  // used here.  It is only designed to be used in very specific circumstances,
  // such as when inheriting the document/storage principal.
  nsIPrincipal* IntrinsicStoragePrincipal() const {
    return mIntrinsicStoragePrincipal;
  }

  nsIPrincipal* GetContentBlockingAllowListPrincipal() const {
    return mContentBlockingAllowListPrincipal;
  }

  already_AddRefed<nsIPrincipal> RecomputeContentBlockingAllowListPrincipal(
      nsIURI* aURIBeingLoaded, const OriginAttributes& aAttrs);

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  EventListenerManager* GetOrCreateListenerManager() override;
  EventListenerManager* GetExistingListenerManager() const override;

  // This helper class must be set when we dispatch beforeunload and unload
  // events in order to avoid unterminate sync XHRs.
  class MOZ_RAII PageUnloadingEventTimeStamp {
    RefPtr<Document> mDocument;
    bool mSet;

   public:
    explicit PageUnloadingEventTimeStamp(Document* aDocument)
        : mDocument(aDocument), mSet(false) {
      MOZ_ASSERT(aDocument);
      if (mDocument->mPageUnloadingEventTimeStamp.IsNull()) {
        mDocument->SetPageUnloadingEventTimeStamp();
        mSet = true;
      }
    }

    ~PageUnloadingEventTimeStamp() {
      if (mSet) {
        mDocument->CleanUnloadEventsTimeStamp();
      }
    }
  };

  /**
   * Let the document know that we're starting to load data into it.
   * @param aCommand The parser command. Must not be null.
   *                 XXXbz It's odd to have that here.
   * @param aChannel The channel the data will come from. The channel must be
   *                 able to report its Content-Type.
   * @param aLoadGroup The loadgroup this document should use from now on.
   *                   Note that the document might not be the only thing using
   *                   this loadgroup.
   * @param aContainer The container this document is in.  This may be null.
   *                   XXXbz maybe we should make it more explicit (eg make the
   *                   container an nsIWebNavigation or nsIDocShell or
   *                   something)?
   * @param [out] aDocListener the listener to pump data from the channel into.
   *                           Generally this will be the parser this document
   *                           sets up, or some sort of data-handler for media
   *                           documents.
   * @param aReset whether the document should call Reset() on itself.  If this
   *               is false, the document will NOT set its principal to the
   *               channel's owner, will not clear any event listeners that are
   *               already set on it, etc.
   * @param aSink The content sink to use for the data.  If this is null and
   *              the document needs a content sink, it will create one based
   *              on whatever it knows about the data it's going to load.
   *              This MUST be null if the underlying document is an HTML
   *              document. Even in the XML case, please don't add new calls
   *              with non-null sink.
   *
   * Once this has been called, the document will return false for
   * MayStartLayout() until SetMayStartLayout(true) is called on it.  Making
   * sure this happens is the responsibility of the caller of
   * StartDocumentLoad().
   *
   * This function has an implementation, and does some setup, but does NOT set
   * *aDocListener; this is the job of subclasses.
   */
  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool aReset,
                                     nsIContentSink* aSink = nullptr) = 0;
  void StopDocumentLoad();

  virtual void SetSuppressParserErrorElement(bool aSuppress) {}
  virtual bool SuppressParserErrorElement() { return false; }

  virtual void SetSuppressParserErrorConsoleMessages(bool aSuppress) {}
  virtual bool SuppressParserErrorConsoleMessages() { return false; }

  // nsINode
  bool IsNodeOfType(uint32_t aFlags) const final;
  nsresult InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                             bool aNotify) override;
  void RemoveChildNode(nsIContent* aKid, bool aNotify) final;
  nsresult Clone(dom::NodeInfo* aNodeInfo, nsINode** aResult) const override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult CloneDocHelper(Document* clone) const;

  Document* GetLatestStaticClone() const { return mLatestStaticClone; }

  /**
   * Signal that the document title may have changed
   * (see Document::GetTitle).
   * @param aBoundTitleElement true if an HTML or SVG <title> element
   * has just been bound to the document.
   */
  virtual void NotifyPossibleTitleChange(bool aBoundTitleElement);

  /**
   * Return the URI for the document. May return null.  If it ever stops being
   * able to return null, we can make sure nsINode::GetBaseURI/GetBaseURIObject
   * also never return null.
   *
   * The value returned corresponds to the "document's address" in
   * HTML5.  As such, it may change over the lifetime of the document, for
   * instance as a result of the user navigating to a fragment identifier on
   * the page, or as a result to a call to pushState() or replaceState().
   *
   * https://html.spec.whatwg.org/multipage/dom.html#the-document%27s-address
   */
  nsIURI* GetDocumentURI() const { return mDocumentURI; }

  /**
   * Return the original URI of the document.  This is the same as the
   * document's URI unless that has changed from its original value (for
   * example, due to history.pushState() or replaceState() being invoked on the
   * document).
   *
   * This method corresponds to the "creation URL" in HTML5 and, once set,
   * doesn't change over the lifetime of the document.
   *
   * https://html.spec.whatwg.org/multipage/webappapis.html#creation-url
   */
  nsIURI* GetOriginalURI() const { return mOriginalURI; }

  /**
   * Return the base domain of the document.  This has been computed using
   * mozIThirdPartyUtil::GetBaseDomain() and can be used for third-party
   * checks.  When the URI of the document changes, this value is recomputed.
   */
  nsCString GetBaseDomain() const { return mBaseDomain; }

  /**
   * Set the URI for the document.  This also sets the document's original URI,
   * if it's null.
   */
  void SetDocumentURI(nsIURI* aURI);

  /**
   * Set the URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  void SetChromeXHRDocURI(nsIURI* aURI) { mChromeXHRDocURI = aURI; }

  /**
   * Set the base URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  void SetChromeXHRDocBaseURI(nsIURI* aURI) { mChromeXHRDocBaseURI = aURI; }

  /**
   * The CSP in general is stored in the ClientInfo, but we also cache
   * the CSP on the document so subresources loaded within a document
   * can query that cached CSP instead of having to deserialize the CSP
   * from the Client.
   *
   * Please note that at the time of CSP parsing the Client is not
   * available yet, hence we sync CSP of document and Client when the
   * Client becomes available within nsGlobalWindowInner::EnsureClientSource().
   */
  nsIContentSecurityPolicy* GetCsp() const;
  void SetCsp(nsIContentSecurityPolicy* aCSP);

  nsIContentSecurityPolicy* GetPreloadCsp() const;
  void SetPreloadCsp(nsIContentSecurityPolicy* aPreloadCSP);

  void GetCspJSON(nsString& aJSON);

  /**
   * Set referrer policy and upgrade-insecure-requests flags
   */
  void ApplySettingsFromCSP(bool aSpeculative);

  already_AddRefed<nsIParser> CreatorParserOrNull() {
    nsCOMPtr<nsIParser> parser = mParser;
    return parser.forget();
  }

  /**
   * ReferrerInfo getter for Document.webidl.
   */
  nsIReferrerInfo* ReferrerInfo() const { return GetReferrerInfo(); }

  nsIReferrerInfo* GetReferrerInfo() const { return mReferrerInfo; }

  nsIReferrerInfo* GetPreloadReferrerInfo() const {
    return mPreloadReferrerInfo;
  }
  /**
   * Return the referrer policy of the document. Return "default" if there's no
   * valid meta referrer tag found in the document.
   * Referrer policy should be inherited from parent if the iframe is srcdoc
   */
  ReferrerPolicyEnum GetReferrerPolicy() const;

  /**
   * GetReferrerPolicy() for Document.webidl.
   */
  ReferrerPolicyEnum ReferrerPolicy() const { return GetReferrerPolicy(); }

  /**
   * If true, this flag indicates that all mixed content subresource
   * loads for this document (and also embeded browsing contexts) will
   * be blocked.
   */
  bool GetBlockAllMixedContent(bool aPreload) const {
    if (aPreload) {
      return mBlockAllMixedContentPreloads;
    }
    return mBlockAllMixedContent;
  }

  /**
   * If true, this flag indicates that all subresource loads for this
   * document need to be upgraded from http to https.
   * This flag becomes true if the CSP of the document itself, or any
   * of the document's ancestors up to the toplevel document makes use
   * of the CSP directive 'upgrade-insecure-requests'.
   */
  bool GetUpgradeInsecureRequests(bool aPreload) const {
    if (aPreload) {
      return mUpgradeInsecurePreloads;
    }
    return mUpgradeInsecureRequests;
  }

  void SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
    mReferrerInfo = aReferrerInfo;
  }

  /*
   * Referrer policy from <meta name="referrer" content=`policy`>
   * will have higher priority than referrer policy from Referrer-Policy
   * header. So override the old ReferrerInfo if we get one from meta
   */
  void UpdateReferrerInfoFromMeta(const nsAString& aMetaReferrer,
                                  bool aPreload) {
    ReferrerPolicyEnum policy =
        ReferrerInfo::ReferrerPolicyFromMetaString(aMetaReferrer);
    // The empty string "" corresponds to no referrer policy, causing a fallback
    // to a referrer policy defined elsewhere.
    if (policy == ReferrerPolicy::_empty) {
      return;
    }

    MOZ_ASSERT(mReferrerInfo);
    MOZ_ASSERT(mPreloadReferrerInfo);

    if (aPreload) {
      mPreloadReferrerInfo =
          static_cast<mozilla::dom::ReferrerInfo*>((mPreloadReferrerInfo).get())
              ->CloneWithNewPolicy(policy);
    } else {
      mReferrerInfo =
          static_cast<mozilla::dom::ReferrerInfo*>((mReferrerInfo).get())
              ->CloneWithNewPolicy(policy);
    }
  }

  /**
   * Set the principals responsible for this document.  Chances are, you do not
   * want to be using this.
   */
  void SetPrincipals(nsIPrincipal* aPrincipal, nsIPrincipal* aStoragePrincipal);

  /**
   * Get the list of ancestor principals for a document.  This is the same as
   * the ancestor list for the document's docshell the last time SetContainer()
   * was called with a non-null argument. See the documentation for the
   * corresponding getter in docshell for how this list is determined.  We store
   * a copy of the list, because we may lose the ability to reach our docshell
   * before people stop asking us for this information.
   */
  const nsTArray<nsCOMPtr<nsIPrincipal>>& AncestorPrincipals() const {
    return mAncestorPrincipals;
  }

  /**
   * Get the list of ancestor outerWindowIDs for a document that correspond to
   * the ancestor principals (see above for more details).
   */
  const nsTArray<uint64_t>& AncestorOuterWindowIDs() const {
    return mAncestorOuterWindowIDs;
  }

  /**
   * Return the LoadGroup for the document. May return null.
   */
  already_AddRefed<nsILoadGroup> GetDocumentLoadGroup() const {
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
    return group.forget();
  }

  /**
   * Return the fallback base URL for this document, as defined in the HTML
   * specification.  Note that this can return null if there is no document URI.
   *
   * XXXbz: This doesn't implement the bits for about:blank yet.
   */
  nsIURI* GetFallbackBaseURI() const {
    if (mIsSrcdocDocument && mParentDocument) {
      return mParentDocument->GetDocBaseURI();
    }
    return mDocumentURI;
  }

  /**
   * Return the referrer from document URI as defined in the Referrer Policy
   * specification.
   * https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
   * While document is an iframe srcdoc document, let document be document???s
   * browsing context???s browsing context container???s node document.
   * Then referrer should be document's URL
   */

  nsIURI* GetDocumentURIAsReferrer() const {
    if (mIsSrcdocDocument && mParentDocument) {
      return mParentDocument->GetDocumentURIAsReferrer();
    }
    return mDocumentURI;
  }

  /**
   * Return the base URI for relative URIs in the document (the document uri
   * unless it's overridden by SetBaseURI, HTML <base> tags, etc.).  The
   * returned URI could be null if there is no document URI.  If the document is
   * a srcdoc document and has no explicit base URL, return the parent
   * document's base URL.
   */
  nsIURI* GetDocBaseURI() const {
    if (mDocumentBaseURI) {
      return mDocumentBaseURI;
    }
    return GetFallbackBaseURI();
  }

  nsIURI* GetBaseURI(bool aTryUseXHRDocBaseURI = false) const final;

  void SetBaseURI(nsIURI* aURI);

  /**
   * Resolves a URI based on the document's base URI.
   */
  Result<nsCOMPtr<nsIURI>, nsresult> ResolveWithBaseURI(const nsAString& aURI);

  /**
   * Return the URL data which style system needs for resolving url value.
   * This method attempts to use the cached object in mCachedURLData, but
   * if the base URI, document URI, or principal has changed since last
   * call to this function, or the function is called the first time for
   * the document, a new one is created.
   */
  URLExtraData* DefaultStyleAttrURLData();

  /**
   * Get/Set the base target of a link in a document.
   */
  void GetBaseTarget(nsAString& aBaseTarget) const {
    aBaseTarget = mBaseTarget;
  }

  void SetBaseTarget(const nsString& aBaseTarget) { mBaseTarget = aBaseTarget; }

  /**
   * Return a standard name for the document's character set.
   */
  NotNull<const Encoding*> GetDocumentCharacterSet() const {
    return mCharacterSet;
  }

  /**
   * Set the document's character encoding.
   */
  void SetDocumentCharacterSet(NotNull<const Encoding*> aEncoding);

  int32_t GetDocumentCharacterSetSource() const { return mCharacterSetSource; }

  // This method MUST be called before SetDocumentCharacterSet if
  // you're planning to call both.
  void SetDocumentCharacterSetSource(int32_t aCharsetSource) {
    mCharacterSetSource = aCharsetSource;
  }

  /**
   * Get the Content-Type of this document.
   */
  void GetContentType(nsAString& aContentType);

  /**
   * Set the Content-Type of this document.
   */
  virtual void SetContentType(const nsAString& aContentType);

  /**
   * Return the language of this document.
   */
  void GetContentLanguage(nsAString& aContentLanguage) const {
    CopyASCIItoUTF16(mContentLanguage, aContentLanguage);
  }

  // The states BidiEnabled and MathMLEnabled should persist across multiple
  // views (screen, print) of the same document.

  /**
   * Check if the document contains bidi data.
   * If so, we have to apply the Unicode Bidi Algorithm.
   */
  bool GetBidiEnabled() const { return mBidiEnabled; }

  /**
   * Indicate the document contains bidi data.
   * Currently, we cannot disable bidi, because once bidi is enabled,
   * it affects a frame model irreversibly, and plays even though
   * the document no longer contains bidi data.
   */
  void SetBidiEnabled() { mBidiEnabled = true; }

  void SetMathMLEnabled() { mMathMLEnabled = true; }

  /**
   * Ask this document whether it's the initial document in its window.
   */
  bool IsInitialDocument() const { return mIsInitialDocumentInWindow; }

  /**
   * Tell this document that it's the initial document in its window.  See
   * comments on mIsInitialDocumentInWindow for when this should be called.
   */
  void SetIsInitialDocument(bool aIsInitialDocument);

  void SetLoadedAsData(bool aLoadedAsData) { mLoadedAsData = aLoadedAsData; }
  void SetLoadedAsInteractiveData(bool aLoadedAsInteractiveData) {
    mLoadedAsInteractiveData = aLoadedAsInteractiveData;
  }

  /**
   * Normally we assert if a runnable labeled with one DocGroup touches data
   * from another DocGroup. Calling IgnoreDocGroupMismatches() on a document
   * means that we can touch that document from any DocGroup without asserting.
   */
  void IgnoreDocGroupMismatches() { mIgnoreDocGroupMismatches = true; }

  /**
   * Get the bidi options for this document.
   * @see nsBidiUtils.h
   */
  uint32_t GetBidiOptions() const { return mBidiOptions; }

  /**
   * Set the bidi options for this document.  This just sets the bits;
   * callers are expected to take action as needed if they want this
   * change to actually change anything immediately.
   * @see nsBidiUtils.h
   */
  void SetBidiOptions(uint32_t aBidiOptions) { mBidiOptions = aBidiOptions; }

  /**
   * Get the has mixed active content loaded flag for this document.
   */
  bool GetHasMixedActiveContentLoaded() { return mHasMixedActiveContentLoaded; }

  /**
   * Set the has mixed active content loaded flag for this document.
   */
  void SetHasMixedActiveContentLoaded(bool aHasMixedActiveContentLoaded) {
    mHasMixedActiveContentLoaded = aHasMixedActiveContentLoaded;
  }

  /**
   * Get mixed active content blocked flag for this document.
   */
  bool GetHasMixedActiveContentBlocked() {
    return mHasMixedActiveContentBlocked;
  }

  /**
   * Set the mixed active content blocked flag for this document.
   */
  void SetHasMixedActiveContentBlocked(bool aHasMixedActiveContentBlocked) {
    mHasMixedActiveContentBlocked = aHasMixedActiveContentBlocked;
  }

  /**
   * Get the has mixed display content loaded flag for this document.
   */
  bool GetHasMixedDisplayContentLoaded() {
    return mHasMixedDisplayContentLoaded;
  }

  /**
   * Set the has mixed display content loaded flag for this document.
   */
  void SetHasMixedDisplayContentLoaded(bool aHasMixedDisplayContentLoaded) {
    mHasMixedDisplayContentLoaded = aHasMixedDisplayContentLoaded;
  }

  /**
   * Get mixed display content blocked flag for this document.
   */
  bool GetHasMixedDisplayContentBlocked() {
    return mHasMixedDisplayContentBlocked;
  }

  /**
   * Set the mixed display content blocked flag for this document.
   */
  void SetHasMixedDisplayContentBlocked(bool aHasMixedDisplayContentBlocked) {
    mHasMixedDisplayContentBlocked = aHasMixedDisplayContentBlocked;
  }

  /**
   * Set the mixed content object subrequest flag for this document.
   */
  void SetHasMixedContentObjectSubrequest(
      bool aHasMixedContentObjectSubrequest) {
    mHasMixedContentObjectSubrequest = aHasMixedContentObjectSubrequest;
  }

  /**
   * Set CSP flag for this document.
   */
  void SetHasCSP(bool aHasCSP) { mHasCSP = aHasCSP; }

  /**
   * Set unsafe-inline CSP flag for this document.
   */
  void SetHasUnsafeInlineCSP(bool aHasUnsafeInlineCSP) {
    mHasUnsafeInlineCSP = aHasUnsafeInlineCSP;
  }

  /**
   * Set unsafe-eval CSP flag for this document.
   */
  void SetHasUnsafeEvalCSP(bool aHasUnsafeEvalCSP) {
    mHasUnsafeEvalCSP = aHasUnsafeEvalCSP;
  }

  /**
   * Get the content blocking log.
   */
  ContentBlockingLog* GetContentBlockingLog() { return &mContentBlockingLog; }

  /**
   * Get tracking content blocked flag for this document.
   */
  bool GetHasTrackingContentBlocked() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT);
  }

  /**
   * Get fingerprinting content blocked flag for this document.
   */
  bool GetHasFingerprintingContentBlocked() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_BLOCKED_FINGERPRINTING_CONTENT);
  }

  /**
   * Get cryptomining content blocked flag for this document.
   */
  bool GetHasCryptominingContentBlocked() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_BLOCKED_CRYPTOMINING_CONTENT);
  }

  /**
   * Get socialtracking content blocked flag for this document.
   */
  bool GetHasSocialTrackingContentBlocked() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_BLOCKED_SOCIALTRACKING_CONTENT);
  }

  /**
   * Get all cookies blocked flag for this document.
   */
  bool GetHasAllCookiesBlocked() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL);
  }

  /**
   * Get tracking cookies blocked flag for this document.
   */
  bool GetHasTrackingCookiesBlocked() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER);
  }

  /**
   * Get third-party cookies blocked flag for this document.
   */
  bool GetHasForeignCookiesBlocked() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);
  }

  /**
   * Get cookies blocked by site permission flag for this document.
   */
  bool GetHasCookiesBlockedByPermission() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION);
  }

  /**
   * Set the tracking content blocked flag for this document.
   */
  void SetHasTrackingContentBlocked(bool aHasTrackingContentBlocked,
                                    const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked, nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT,
        aHasTrackingContentBlocked);
  }

  /**
   * Set the fingerprinting content blocked flag for this document.
   */
  void SetHasFingerprintingContentBlocked(bool aHasFingerprintingContentBlocked,
                                          const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked,
        nsIWebProgressListener::STATE_BLOCKED_FINGERPRINTING_CONTENT,
        aHasFingerprintingContentBlocked);
  }

  /**
   * Set the cryptomining content blocked flag for this document.
   */
  void SetHasCryptominingContentBlocked(bool aHasCryptominingContentBlocked,
                                        const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked,
        nsIWebProgressListener::STATE_BLOCKED_CRYPTOMINING_CONTENT,
        aHasCryptominingContentBlocked);
  }

  /**
   * Set the socialtracking content blocked flag for this document.
   */
  void SetHasSocialTrackingContentBlocked(bool aHasSocialTrackingContentBlocked,
                                          const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked,
        nsIWebProgressListener::STATE_BLOCKED_SOCIALTRACKING_CONTENT,
        aHasSocialTrackingContentBlocked);
  }

  /**
   * Set the all cookies blocked flag for this document.
   */
  void SetHasAllCookiesBlocked(bool aHasAllCookiesBlocked,
                               const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(aOriginBlocked,
                             nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL,
                             aHasAllCookiesBlocked);
  }

  /**
   * Set the tracking cookies blocked flag for this document.
   */
  void SetHasTrackingCookiesBlocked(
      bool aHasTrackingCookiesBlocked, const nsACString& aOriginBlocked,
      const Maybe<AntiTrackingCommon::StorageAccessGrantedReason>& aReason,
      const nsTArray<nsCString>& aTrackingFullHashes) {
    RecordContentBlockingLog(
        aOriginBlocked, nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER,
        aHasTrackingCookiesBlocked, aReason, aTrackingFullHashes);
  }

  /**
   * Set the third-party cookies blocked flag for this document.
   */
  void SetHasForeignCookiesBlocked(bool aHasForeignCookiesBlocked,
                                   const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked, nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN,
        aHasForeignCookiesBlocked);
  }

  /**
   * Set the cookies blocked by site permission flag for this document.
   */
  void SetHasCookiesBlockedByPermission(bool aHasCookiesBlockedByPermission,
                                        const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked,
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION,
        aHasCookiesBlockedByPermission);
  }

  /**
   * Set the cookies loaded flag for this document.
   */
  void SetHasCookiesLoaded(bool aHasCookiesLoaded,
                           const nsACString& aOriginLoaded) {
    RecordContentBlockingLog(aOriginLoaded,
                             nsIWebProgressListener::STATE_COOKIES_LOADED,
                             aHasCookiesLoaded);
  }

  /**
   * Get the cookies loaded flag for this document.
   */
  bool GetHasCookiesLoaded() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_COOKIES_LOADED);
  }

  /**
   * Set the tracker cookies loaded flag for this document.
   */
  void SetHasTrackerCookiesLoaded(bool aHasTrackerCookiesLoaded,
                                  const nsACString& aOriginLoaded) {
    RecordContentBlockingLog(
        aOriginLoaded, nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER,
        aHasTrackerCookiesLoaded);
  }

  /**
   * Get the tracker cookies loaded flag for this document.
   */
  bool GetHasTrackerCookiesLoaded() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER);
  }

  /**
   * Set the social tracker cookies loaded flag for this document.
   */
  void SetHasSocialTrackerCookiesLoaded(bool aHasSocialTrackerCookiesLoaded,
                                        const nsACString& aOriginLoaded) {
    RecordContentBlockingLog(
        aOriginLoaded,
        nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER,
        aHasSocialTrackerCookiesLoaded);
  }

  /**
   * Get the social tracker cookies loaded flag for this document.
   */
  bool GetHasSocialTrackerCookiesLoaded() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER);
  }

  /**
   * Get tracking content loaded flag for this document.
   */
  bool GetHasTrackingContentLoaded() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT);
  }

  /**
   * Set the tracking content loaded flag for this document.
   */
  void SetHasTrackingContentLoaded(bool aHasTrackingContentLoaded,
                                   const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked, nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT,
        aHasTrackingContentLoaded);
  }

  /**
   * Get fingerprinting content loaded flag for this document.
   */
  bool GetHasFingerprintingContentLoaded() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_LOADED_FINGERPRINTING_CONTENT);
  }

  /**
   * Set the fingerprinting content loaded flag for this document.
   */
  void SetHasFingerprintingContentLoaded(bool aHasFingerprintingContentLoaded,
                                         const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked,
        nsIWebProgressListener::STATE_LOADED_FINGERPRINTING_CONTENT,
        aHasFingerprintingContentLoaded);
  }

  /**
   * Get cryptomining content loaded flag for this document.
   */
  bool GetHasCryptominingContentLoaded() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_LOADED_CRYPTOMINING_CONTENT);
  }

  /**
   * Set the cryptomining content loaded flag for this document.
   */
  void SetHasCryptominingContentLoaded(bool aHasCryptominingContentLoaded,
                                       const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked,
        nsIWebProgressListener::STATE_LOADED_CRYPTOMINING_CONTENT,
        aHasCryptominingContentLoaded);
  }

  /**
   * Get socialtracking content loaded flag for this document.
   */
  bool GetHasSocialTrackingContentLoaded() {
    return mContentBlockingLog.HasBlockedAnyOfType(
        nsIWebProgressListener::STATE_LOADED_SOCIALTRACKING_CONTENT);
  }

  /**
   * Set the socialtracking content loaded flag for this document.
   */
  void SetHasSocialTrackingContentLoaded(bool aHasSocialTrackingContentLoaded,
                                         const nsACString& aOriginBlocked) {
    RecordContentBlockingLog(
        aOriginBlocked,
        nsIWebProgressListener::STATE_LOADED_SOCIALTRACKING_CONTENT,
        aHasSocialTrackingContentLoaded);
  }

  /**
   * Get the sandbox flags for this document.
   * @see nsSandboxFlags.h for the possible flags
   */
  uint32_t GetSandboxFlags() const { return mSandboxFlags; }

  /**
   * Get string representation of sandbox flags (null if no flags are set)
   */
  void GetSandboxFlagsAsString(nsAString& aFlags);

  /**
   * Set the sandbox flags for this document.
   * @see nsSandboxFlags.h for the possible flags
   */
  void SetSandboxFlags(uint32_t sandboxFlags) { mSandboxFlags = sandboxFlags; }

  /**
   * Called when the document was decoded as UTF-8 and decoder encountered no
   * errors.
   */
  void EnableEncodingMenu() { mEncodingMenuDisabled = false; }

  /**
   * Called to disable client access to cookies through the document.cookie API
   * from user JavaScript code.
   */
  void DisableCookieAccess() { mDisableCookieAccess = true; }

  void SetLinkHandlingEnabled(bool aValue) { mLinksEnabled = aValue; }
  bool LinkHandlingEnabled() { return mLinksEnabled; }

  /**
   * Set compatibility mode for this document
   */
  void SetCompatibilityMode(nsCompatibility aMode);

  /**
   * Called to disable client access to document.write() API from user
   * JavaScript code.
   */
  void SetDocWriteDisabled(bool aDisabled) { mDisableDocWrite = aDisabled; }

  /**
   * Whether a document.write() call is in progress.
   */
  bool IsWriting() const { return mWriteLevel != uint32_t(0); }

  /**
   * Access HTTP header data (this may also get set from other
   * sources, like HTML META tags).
   */
  void GetHeaderData(nsAtom* aHeaderField, nsAString& aData) const;
  void SetHeaderData(nsAtom* aheaderField, const nsAString& aData);

  /**
   * Create a new presentation shell that will use aContext for its
   * presentation context (presentation contexts <b>must not</b> be
   * shared among multiple presentation shells). The caller of this
   * method is responsible for calling BeginObservingDocument() on the
   * presshell if the presshell should observe document mutations.
   */
  already_AddRefed<PresShell> CreatePresShell(nsPresContext* aContext,
                                              nsViewManager* aViewManager);
  void DeletePresShell();

  PresShell* GetPresShell() const {
    return GetBFCacheEntry() ? nullptr : mPresShell;
  }

  inline PresShell* GetObservingPresShell() const;

  // Return whether the presshell for this document is safe to flush.
  bool IsSafeToFlush() const;

  inline nsPresContext* GetPresContext() const;

  bool HasShellOrBFCacheEntry() const { return mPresShell || mBFCacheEntry; }

  // Instead using this method, what you probably want is
  // RemoveFromBFCacheSync() as we do in MessagePort and BroadcastChannel.
  void DisallowBFCaching() {
    NS_ASSERTION(!mBFCacheEntry, "We're already in the bfcache!");
    mBFCacheDisallowed = true;
  }

  bool IsBFCachingAllowed() const { return !mBFCacheDisallowed; }

  // Accepts null to clear the BFCache entry too.
  void SetBFCacheEntry(nsIBFCacheEntry* aEntry);

  nsIBFCacheEntry* GetBFCacheEntry() const { return mBFCacheEntry; }

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a
   * parent. Note that this parent chain may cross chrome boundaries.
   */
  Document* GetInProcessParentDocument() const { return mParentDocument; }

  /**
   * Set the parent document of this document.
   */
  void SetParentDocument(Document* aParent) {
    mParentDocument = aParent;
    if (aParent) {
      mIgnoreDocGroupMismatches = aParent->mIgnoreDocGroupMismatches;
      if (!mIsDevToolsDocument) {
        mIsDevToolsDocument = mParentDocument->IsDevToolsDocument();
      }
    }
  }

  /**
   * Are plugins allowed in this document ?
   */
  bool GetAllowPlugins();

  /**
   * Set the sub document for aContent to aSubDoc.
   */
  nsresult SetSubDocumentFor(Element* aContent, Document* aSubDoc);

  /**
   * Get the sub document for aContent
   */
  Document* GetSubDocumentFor(nsIContent* aContent) const;

  /**
   * Find the content node for which aDocument is a sub document.
   */
  Element* FindContentForSubDocument(Document* aDocument) const;

  /**
   * Return the doctype for this document.
   */
  DocumentType* GetDoctype() const;

  /**
   * Return the root element for this document.
   */
  Element* GetRootElement() const;

  Selection* GetSelection(ErrorResult& aRv);

  already_AddRefed<Promise> HasStorageAccess(ErrorResult& aRv);
  already_AddRefed<Promise> RequestStorageAccess(ErrorResult& aRv);

  /**
   * Gets the event target to dispatch key events to if there is no focused
   * content in the document.
   */
  virtual Element* GetUnfocusedKeyEventTarget();

  /**
   * Retrieve information about the viewport as a data structure.
   * This will return information in the viewport META data section
   * of the document. This can be used in lieu of ProcessViewportInfo(),
   * which places the viewport information in the document header instead
   * of returning it directly.
   *
   * @param aDisplaySize size of the on-screen display area for this
   * document, in device pixels.
   *
   * NOTE: If the site is optimized for mobile (via the doctype), this
   * will return viewport information that specifies default information.
   */
  nsViewportInfo GetViewportInfo(const ScreenIntSize& aDisplaySize);

  void AddMetaViewportElement(HTMLMetaElement* aElement,
                              ViewportMetaData&& aData);
  void RemoveMetaViewportElement(HTMLMetaElement* aElement);

  void UpdateForScrollAnchorAdjustment(nscoord aLength);

  /**
   * True iff this doc will ignore manual character encoding overrides.
   */
  virtual bool WillIgnoreCharsetOverride() { return true; }

  /**
   * Return whether the document was created by a srcdoc iframe.
   */
  bool IsSrcdocDocument() const { return mIsSrcdocDocument; }

  /**
   * Sets whether the document was created by a srcdoc iframe.
   */
  void SetIsSrcdocDocument(bool aIsSrcdocDocument) {
    mIsSrcdocDocument = aIsSrcdocDocument;
  }

  /*
   * Gets the srcdoc string from within the channel (assuming both exist).
   * Returns a void string if this isn't a srcdoc document or if
   * the channel has not been set.
   */
  nsresult GetSrcdocData(nsAString& aSrcdocData);

  already_AddRefed<AnonymousContent> InsertAnonymousContent(
      Element& aElement, ErrorResult& aError);
  void RemoveAnonymousContent(AnonymousContent& aContent, ErrorResult& aError);
  /**
   * If aNode is a descendant of anonymous content inserted by
   * InsertAnonymousContent, this method returns the root element of the
   * inserted anonymous content (in other words, the clone of the aElement
   * that was passed to InsertAnonymousContent).
   */
  Element* GetAnonRootIfInAnonymousContentContainer(nsINode* aNode) const;
  nsTArray<RefPtr<AnonymousContent>>& GetAnonymousContents() {
    return mAnonymousContents;
  }

  TimeStamp GetPageUnloadingEventTimeStamp() const {
    if (!mParentDocument) {
      return mPageUnloadingEventTimeStamp;
    }

    TimeStamp parentTimeStamp(
        mParentDocument->GetPageUnloadingEventTimeStamp());
    if (parentTimeStamp.IsNull()) {
      return mPageUnloadingEventTimeStamp;
    }

    if (!mPageUnloadingEventTimeStamp ||
        parentTimeStamp < mPageUnloadingEventTimeStamp) {
      return parentTimeStamp;
    }

    return mPageUnloadingEventTimeStamp;
  }

  void NotifyLayerManagerRecreated();

  /**
   * Add an SVG element to the list of elements that need
   * their mapped attributes resolved to a Servo declaration block.
   *
   * These are weak pointers, please manually unschedule them when an element
   * is removed.
   */
  void ScheduleSVGForPresAttrEvaluation(SVGElement* aSVG) {
    mLazySVGPresElements.PutEntry(aSVG);
  }

  // Unschedule an element scheduled by ScheduleFrameRequestCallback (e.g. for
  // when it is destroyed)
  void UnscheduleSVGForPresAttrEvaluation(SVGElement* aSVG) {
    mLazySVGPresElements.RemoveEntry(aSVG);
  }

  // Resolve all SVG pres attrs scheduled in ScheduleSVGForPresAttrEvaluation
  void ResolveScheduledSVGPresAttrs();

  Maybe<ClientInfo> GetClientInfo() const;
  Maybe<ClientState> GetClientState() const;
  Maybe<ServiceWorkerDescriptor> GetController() const;

  // Returns the size of the mBlockedNodesByClassifier array.
  //
  // This array contains nodes that have been blocked to prevent user tracking,
  // fingerprinting, cryptomining, etc. They most likely have had their
  // nsIChannel canceled by the URL classifier (Safebrowsing).
  //
  // A script can subsequently use GetBlockedNodesByClassifier()
  // to get a list of references to these nodes.
  //
  // Note:
  // This expresses how many tracking nodes have been blocked for this document
  // since its beginning, not how many of them are still around in the DOM tree.
  // Weak references to blocked nodes are added in the mBlockedNodesByClassifier
  // array but they are not removed when those nodes are removed from the tree
  // or even garbage collected.
  long BlockedNodeByClassifierCount() const {
    return mBlockedNodesByClassifier.Length();
  }

  //
  // Returns strong references to mBlockedNodesByClassifier. (Document.h)
  //
  // This array contains nodes that have been blocked to prevent
  // user tracking. They most likely have had their nsIChannel
  // canceled by the URL classifier (Safebrowsing).
  //
  already_AddRefed<nsSimpleContentList> BlockedNodesByClassifier() const;

  // Helper method that returns true if the document has storage-access sandbox
  // flag.
  bool StorageAccessSandboxed() const;

  // Returns the cookie settings for this and sub contexts.
  nsICookieSettings* CookieSettings();

  // Increments the document generation.
  inline void Changed() { ++mGeneration; }

  // Returns the current generation.
  inline int32_t GetGeneration() const { return mGeneration; }

  // Adds cached sizes values to aSizes if there's any
  // cached value and if the document generation hasn't
  // changed since the cache was created.
  // Returns true if sizes were added.
  bool GetCachedSizes(nsTabSizes* aSizes);

  // Sets the cache sizes for the current generation.
  void SetCachedSizes(nsTabSizes* aSizes);

  /**
   * Should be called when an element's editable changes as a result of
   * changing its contentEditable attribute/property.
   *
   * @param aElement the element for which the contentEditable
   *                 attribute/property was changed
   * @param aChange +1 if the contentEditable attribute/property was changed to
   *                true, -1 if it was changed to false
   */
  nsresult ChangeContentEditableCount(nsIContent* aElement, int32_t aChange);
  void DeferredContentEditableCountChange(nsIContent* aElement);

  enum class EditingState : int8_t {
    eTearingDown = -2,
    eSettingUp = -1,
    eOff = 0,
    eDesignMode,
    eContentEditable
  };

  /**
   * Returns the editing state of the document (not editable, contentEditable or
   * designMode).
   */
  EditingState GetEditingState() const { return mEditingState; }

  /**
   * Returns whether the document is editable.
   */
  bool IsEditingOn() const {
    return GetEditingState() == EditingState::eDesignMode ||
           GetEditingState() == EditingState::eContentEditable;
  }

  class MOZ_STACK_CLASS nsAutoEditingState {
   public:
    nsAutoEditingState(Document* aDoc, EditingState aState)
        : mDoc(aDoc), mSavedState(aDoc->mEditingState) {
      aDoc->mEditingState = aState;
    }
    ~nsAutoEditingState() { mDoc->mEditingState = mSavedState; }

   private:
    RefPtr<Document> mDoc;
    EditingState mSavedState;
  };
  friend class nsAutoEditingState;

  /**
   * Set the editing state of the document. Don't use this if you want
   * to enable/disable editing, call EditingStateChanged() or
   * SetDesignMode().
   */
  nsresult SetEditingState(EditingState aState);

  /**
   * Called when this Document's editor is destroyed.
   */
  void TearingDownEditor();

  void SetKeyPressEventModel(uint16_t aKeyPressEventModel);

  // Gets the next form number.
  //
  // Used by nsContentUtils::GenerateStateKey to get a unique number for each
  // parser inserted form element.
  int32_t GetNextFormNumber() { return mNextFormNumber++; }

  // Gets the next form control number.
  //
  // Used by nsContentUtils::GenerateStateKey to get a unique number for each
  // parser inserted form control element.
  int32_t GetNextControlNumber() { return mNextControlNumber++; }

 protected:
  friend class nsUnblockOnloadEvent;

  nsresult InitCSP(nsIChannel* aChannel);

  nsresult InitFeaturePolicy(nsIChannel* aChannel);

  nsresult InitReferrerInfo(nsIChannel* aChannel);

  void PostUnblockOnloadEvent();

  void DoUnblockOnload();

  void MaybeEndOutermostXBLUpdate();

  void RetrieveRelevantHeaders(nsIChannel* aChannel);

  void TryChannelCharset(nsIChannel* aChannel, int32_t& aCharsetSource,
                         NotNull<const Encoding*>& aEncoding,
                         nsHtml5TreeOpExecutor* aExecutor);

  void DispatchContentLoadedEvents();

  void DispatchPageTransition(EventTarget* aDispatchTarget,
                              const nsAString& aType, bool aPersisted,
                              bool aOnlySystemGroup = false);

  // Call this before the document does something that will unbind all content.
  // That will stop us from doing a lot of work as each element is removed.
  void DestroyElementMaps();

  Element* GetRootElementInternal() const;
  void DoNotifyPossibleTitleChange();

  void SetPageUnloadingEventTimeStamp() {
    MOZ_ASSERT(!mPageUnloadingEventTimeStamp);
    mPageUnloadingEventTimeStamp = TimeStamp::NowLoRes();
  }

  void CleanUnloadEventsTimeStamp() {
    MOZ_ASSERT(mPageUnloadingEventTimeStamp);
    mPageUnloadingEventTimeStamp = TimeStamp();
  }

  /**
   * Clears any Servo element data stored on Elements in the document.
   */
  void ClearStaleServoData();

  /**
   * Returns the top window root from the outer window.
   */
  already_AddRefed<nsPIWindowRoot> GetWindowRoot();

  /**
   * Do the tree-disconnection that ResetToURI and document.open need to do.
   */
  void DisconnectNodeTree();

  /**
   * Like IsEditingOn(), but will flush as needed first.
   */
  bool IsEditingOnAfterFlush();

  /**
   * MaybeDispatchCheckKeyPressEventModelEvent() dispatches
   * "CheckKeyPressEventModel" event to check whether we should dispatch
   * keypress events in confluent model or split model.  This should be
   * called only when mEditingState is changed to eDesignMode or
   * eConentEditable at first time.
   */
  void MaybeDispatchCheckKeyPressEventModelEvent();

  /* Midas implementation */
  nsCommandManager* GetMidasCommandManager();

  nsresult TurnEditingOff();

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because this is called from all sorts
  // of places, and I'm pretty sure the exact ExecCommand call it
  // makes cannot actually run script.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult EditingStateChanged();

  void MaybeEditingStateChanged();

 private:
  class SelectorCacheKey {
   public:
    explicit SelectorCacheKey(const nsAString& aString) : mKey(aString) {
      MOZ_COUNT_CTOR(SelectorCacheKey);
    }

    nsString mKey;
    nsExpirationState mState;

    nsExpirationState* GetExpirationState() { return &mState; }

    ~SelectorCacheKey() { MOZ_COUNT_DTOR(SelectorCacheKey); }
  };

  class SelectorCacheKeyDeleter;

 public:
  class SelectorCache final : public nsExpirationTracker<SelectorCacheKey, 4> {
   public:
    using SelectorList = UniquePtr<RawServoSelectorList>;

    explicit SelectorCache(nsIEventTarget* aEventTarget);

    void CacheList(const nsAString& aSelector, SelectorList aSelectorList) {
      MOZ_ASSERT(NS_IsMainThread());
      SelectorCacheKey* key = new SelectorCacheKey(aSelector);
      mTable.Put(key->mKey, std::move(aSelectorList));
      AddObject(key);
    }

    void NotifyExpired(SelectorCacheKey* aSelector) final;

    // We do not call MarkUsed because it would just slow down lookups and
    // because we're OK expiring things after a few seconds even if they're
    // being used.  Returns whether we actually had an entry for aSelector.
    //
    // If we have an entry and the selector list returned has a null
    // RawServoSelectorList*, that indicates that aSelector has already been
    // parsed and is not a syntactically valid selector.
    SelectorList* GetList(const nsAString& aSelector) {
      return mTable.GetValue(aSelector);
    }

    ~SelectorCache();

   private:
    nsDataHashtable<nsStringHashKey, SelectorList> mTable;
  };

  SelectorCache& GetSelectorCache() {
    if (!mSelectorCache) {
      mSelectorCache =
          MakeUnique<SelectorCache>(EventTargetFor(TaskCategory::Other));
    }
    return *mSelectorCache;
  }
  // Get the root <html> element, or return null if there isn't one (e.g.
  // if the root isn't <html>)
  Element* GetHtmlElement() const;
  // Returns the first child of GetHtmlContent which has the given tag,
  // or nullptr if that doesn't exist.
  Element* GetHtmlChildElement(nsAtom* aTag);
  // Get the canonical <body> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <body> isn't there)
  HTMLBodyElement* GetBodyElement();
  // Get the canonical <head> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <head> isn't there)
  Element* GetHeadElement() { return GetHtmlChildElement(nsGkAtoms::head); }
  // Get the "body" in the sense of document.body: The first <body> or
  // <frameset> that's a child of a root <html>
  nsGenericHTMLElement* GetBody();
  // Set the "body" in the sense of document.body.
  void SetBody(nsGenericHTMLElement* aBody, ErrorResult& rv);
  // Get the "head" element in the sense of document.head.
  HTMLSharedElement* GetHead();

  ServoStyleSet* StyleSetForPresShellOrMediaQueryEvaluation() const {
    return mStyleSet.get();
  }

  // ShadowRoot has APIs that can change styles. This notifies the shell that
  // stlyes applicable in the shadow tree have potentially changed.
  void RecordShadowStyleChange(ShadowRoot&);

  // Needs to be called any time the applicable style can has changed, in order
  // to schedule a style flush and setup all the relevant state.
  void ApplicableStylesChanged();

  // Whether we filled the style set with any style sheet. Only meant to be used
  // from DocumentOrShadowRoot::Traverse.
  bool StyleSetFilled() const { return mStyleSetFilled; }

  /**
   * Accessors to the collection of stylesheets owned by this document.
   * Style sheets are ordered, most significant last.
   */

  StyleSheetList* StyleSheets() {
    return &DocumentOrShadowRoot::EnsureDOMStyleSheets();
  }

  void InsertSheetAt(size_t aIndex, StyleSheet&);

  /**
   * Add a stylesheet to the document
   *
   * TODO(emilio): This is only used by parts of editor that are no longer in
   * use by m-c or c-c, so remove.
   */
  void AddStyleSheet(StyleSheet* aSheet) {
    MOZ_ASSERT(aSheet);
    InsertSheetAt(SheetCount(), *aSheet);
  }

  /**
   * Remove a stylesheet from the document
   */
  void RemoveStyleSheet(StyleSheet* aSheet);

  /**
   * Notify the document that the applicable state of the sheet changed
   * and that observers should be notified and style sets updated
   */
  void SetStyleSheetApplicableState(StyleSheet* aSheet, bool aApplicable);

  enum additionalSheetType {
    eAgentSheet,
    eUserSheet,
    eAuthorSheet,
    AdditionalSheetTypeCount
  };

  nsresult LoadAdditionalStyleSheet(additionalSheetType aType,
                                    nsIURI* aSheetURI);
  nsresult AddAdditionalStyleSheet(additionalSheetType aType,
                                   StyleSheet* aSheet);
  void RemoveAdditionalStyleSheet(additionalSheetType aType, nsIURI* sheetURI);

  StyleSheet* GetFirstAdditionalAuthorSheet() {
    return mAdditionalSheets[eAuthorSheet].SafeElementAt(0);
  }

  /**
   * Returns the index that aSheet should be inserted at to maintain document
   * ordering.
   */
  size_t FindDocStyleSheetInsertionPoint(const StyleSheet& aSheet);

  /**
   * Get this document's CSSLoader.  This is guaranteed to not return null.
   */
  css::Loader* CSSLoader() const { return mCSSLoader; }

  /**
   * Get this document's StyleImageLoader.  This is guaranteed to not return
   * null.
   */
  css::ImageLoader* StyleImageLoader() const { return mStyleImageLoader; }

  /**
   * Get the channel that was passed to StartDocumentLoad or Reset for this
   * document.  Note that this may be null in some cases (eg if
   * StartDocumentLoad or Reset were never called)
   */
  nsIChannel* GetChannel() const { return mChannel; }

  /**
   * Get this document's attribute stylesheet.  May return null if
   * there isn't one.
   */
  nsHTMLStyleSheet* GetAttributeStyleSheet() const { return mAttrStyleSheet; }

  /**
   * Get this document's inline style sheet.  May return null if there
   * isn't one
   */
  nsHTMLCSSStyleSheet* GetInlineStyleSheet() const {
    return mStyleAttrStyleSheet;
  }

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject);

  /**
   * Get/set the object from which the context for the event/script handling can
   * be got. Normally GetScriptHandlingObject() returns the same object as
   * GetScriptGlobalObject(), but if the document is loaded as data,
   * non-null may be returned, even if GetScriptGlobalObject() returns null.
   * aHasHadScriptHandlingObject is set true if document has had the object
   * for event/script handling. Do not process any events/script if the method
   * returns null, but aHasHadScriptHandlingObject is true.
   */
  nsIScriptGlobalObject* GetScriptHandlingObject(
      bool& aHasHadScriptHandlingObject) const {
    aHasHadScriptHandlingObject = mHasHadScriptHandlingObject;
    return mScriptGlobalObject ? mScriptGlobalObject.get()
                               : GetScriptHandlingObjectInternal();
  }
  void SetScriptHandlingObject(nsIScriptGlobalObject* aScriptObject);

  /**
   * Get the object that is used as the scope for all of the content
   * wrappers whose owner document is this document. Unlike the script global
   * object, this will only return null when the global object for this
   * document is truly gone. Use this object when you're trying to find a
   * content wrapper in XPConnect.
   */
  nsIGlobalObject* GetScopeObject() const;
  void SetScopeObject(nsIGlobalObject* aGlobal);

  /**
   * Return the window containing the document (the outer window).
   */
  nsPIDOMWindowOuter* GetWindow() const {
    return mWindow ? mWindow->GetOuterWindow() : GetWindowInternal();
  }

  bool IsInBackgroundWindow() const {
    auto* outer = mWindow ? mWindow->GetOuterWindow() : nullptr;
    return outer && outer->IsBackground();
  }

  /**
   * Return the inner window used as the script compilation scope for
   * this document. If you're not absolutely sure you need this, use
   * GetWindow().
   */
  nsPIDOMWindowInner* GetInnerWindow() const {
    return mRemovedFromDocShell ? nullptr : mWindow;
  }

  /**
   * Return the outer window ID.
   */
  uint64_t OuterWindowID() const {
    nsPIDOMWindowOuter* window = GetWindow();
    return window ? window->WindowID() : 0;
  }

  /**
   * Return the inner window ID.
   */
  uint64_t InnerWindowID() const {
    nsPIDOMWindowInner* window = GetInnerWindow();
    return window ? window->WindowID() : 0;
  }

  bool IsTopLevelWindowInactive() const;

  /**
   * Get the script loader for this document
   */
  dom::ScriptLoader* ScriptLoader() { return mScriptLoader; }

  /**
   * Add/Remove an element to the document's id and name hashes
   */
  void AddToIdTable(Element* aElement, nsAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsAtom* aId);
  void AddToNameTable(Element* aElement, nsAtom* aName);
  void RemoveFromNameTable(Element* aElement, nsAtom* aName);

  /**
   * Returns all elements in the fullscreen stack in the insertion order.
   */
  nsTArray<Element*> GetFullscreenStack() const;

  /**
   * Asynchronously requests that the document make aElement the fullscreen
   * element, and move into fullscreen mode. The current fullscreen element
   * (if any) is pushed onto the fullscreen element stack, and it can be
   * returned to fullscreen status by calling RestorePreviousFullscreenState().
   *
   * Note that requesting fullscreen in a document also makes the element which
   * contains this document in this document's parent document fullscreen. i.e.
   * the <iframe> or <browser> that contains this document is also mode
   * fullscreen. This happens recursively in all ancestor documents.
   */
  void AsyncRequestFullscreen(UniquePtr<FullscreenRequest>);

  // Do the "fullscreen element ready check" from the fullscreen spec.
  // It returns true if the given element is allowed to go into fullscreen.
  // It is responsive to dispatch "fullscreenerror" event when necessary.
  bool FullscreenElementReadyCheck(const FullscreenRequest&);

  // This is called asynchronously by Document::AsyncRequestFullscreen()
  // to move this document into fullscreen mode if allowed.
  void RequestFullscreen(UniquePtr<FullscreenRequest> aRequest);

  // Removes all elements from the fullscreen stack, removing full-scren
  // styles from the top element in the stack.
  void CleanupFullscreenState();

  // Pushes aElement onto the fullscreen stack, and removes fullscreen styles
  // from the former fullscreen stack top, and its ancestors, and applies the
  // styles to aElement. aElement becomes the new "fullscreen element".
  bool FullscreenStackPush(Element* aElement);

  // Remove the top element from the fullscreen stack. Removes the fullscreen
  // styles from the former top element, and applies them to the new top
  // element, if there is one.
  void FullscreenStackPop();

  /**
   * Called when a frame in a child process has entered fullscreen or when a
   * fullscreen frame in a child process changes to another origin.
   * aFrameElement is the frame element which contains the child-process
   * fullscreen document.
   */
  nsresult RemoteFrameFullscreenChanged(Element* aFrameElement);

  /**
   * Called when a frame in a remote child document has rolled back fullscreen
   * so that all its fullscreen element stacks are empty; we must continue the
   * rollback in this parent process' doc tree branch which is fullscreen.
   * Note that only one branch of the document tree can have its documents in
   * fullscreen state at one time. We're in inconsistent state if a
   * fullscreen document has a parent and that parent isn't fullscreen. We
   * preserve this property across process boundaries.
   */
  nsresult RemoteFrameFullscreenReverted();

  /**
   * Restores the previous fullscreen element to fullscreen status. If there
   * is no former fullscreen element, this exits fullscreen, moving the
   * top-level browser window out of fullscreen mode.
   */
  void RestorePreviousFullscreenState(UniquePtr<FullscreenExit>);

  /**
   * Returns true if this document is a fullscreen leaf document, i.e. it
   * is in fullscreen mode and has no fullscreen children.
   */
  bool IsFullscreenLeaf();

  /**
   * Returns the document which is at the root of this document's branch
   * in the in-process document tree. Returns nullptr if the document isn't
   * fullscreen.
   */
  Document* GetFullscreenRoot();

  /**
   * Sets the fullscreen root to aRoot. This stores a weak reference to aRoot
   * in this document.
   */
  void SetFullscreenRoot(Document* aRoot);

  /**
   * Synchronously cleans up the fullscreen state on the given document.
   *
   * Calling this without performing fullscreen transition could lead
   * to undesired effect (the transition happens after document state
   * flips), hence it should only be called either by nsGlobalWindow
   * when we have performed the transition, or when it is necessary to
   * clean up the state immediately. Otherwise, AsyncExitFullscreen()
   * should be called instead.
   *
   * aDocument must not be null.
   */
  static void ExitFullscreenInDocTree(Document* aDocument);

  /**
   * Ask the document to exit fullscreen state asynchronously.
   *
   * Different from ExitFullscreenInDocTree(), this allows the window
   * to perform fullscreen transition first if any.
   *
   * If aDocument is null, it will exit fullscreen from all documents
   * in all windows.
   */
  static void AsyncExitFullscreen(Document* aDocument);

  /**
   * Handles any pending fullscreen in aDocument or its subdocuments.
   *
   * Returns whether there is any fullscreen request handled.
   */
  static bool HandlePendingFullscreenRequests(Document* aDocument);

  void RequestPointerLock(Element* aElement, CallerType);
  MOZ_CAN_RUN_SCRIPT bool SetPointerLock(Element* aElement, StyleCursorKind);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static void UnlockPointer(Document* aDoc = nullptr);

  // ScreenOrientation related APIs

  void SetCurrentOrientation(OrientationType aType, uint16_t aAngle) {
    mCurrentOrientationType = aType;
    mCurrentOrientationAngle = aAngle;
  }

  uint16_t CurrentOrientationAngle() const { return mCurrentOrientationAngle; }
  OrientationType CurrentOrientationType() const {
    return mCurrentOrientationType;
  }
  void SetOrientationPendingPromise(Promise* aPromise);
  Promise* GetOrientationPendingPromise() const {
    return mOrientationPendingPromise;
  }

  void SetRDMPaneOrientation(OrientationType aType, uint16_t aAngle) {
    if (mInRDMPane) {
      SetCurrentOrientation(aType, aAngle);
    }
  }

  //----------------------------------------------------------------------

  // Document notification API's

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.  An observer that is already observing the document must
   * not be added without being removed first.
   */
  void AddObserver(nsIDocumentObserver* aObserver);

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  bool RemoveObserver(nsIDocumentObserver* aObserver);

  // Observation hooks used to propagate notifications to document observers.
  // BeginUpdate must be called before any batch of modifications of the
  // content model or of style data, EndUpdate must be called afterward.
  // To make this easy and painless, use the mozAutoDocUpdate helper class.
  void BeginUpdate();
  void EndUpdate();
  uint32_t UpdateNestingLevel() { return mUpdateNestLevel; }

  void BeginLoad();
  virtual void EndLoad();

  enum ReadyState {
    READYSTATE_UNINITIALIZED = 0,
    READYSTATE_LOADING = 1,
    READYSTATE_INTERACTIVE = 3,
    READYSTATE_COMPLETE = 4
  };
  // Set the readystate of the document.  If updateTimingInformation is true,
  // this will record relevant timestamps in the document's performance timing.
  // Some consumers (document.open is the only one right now, actually) don't
  // want to do that, though.
  void SetReadyStateInternal(ReadyState rs,
                             bool updateTimingInformation = true);
  ReadyState GetReadyStateEnum() { return mReadyState; }

  void SetAncestorLoading(bool aAncestorIsLoading);
  void NotifyLoading(const bool& aCurrentParentIsLoading,
                     bool aNewParentIsLoading, const ReadyState& aCurrentState,
                     ReadyState aNewState);

  // notify that a content node changed state.  This must happen under
  // a scriptblocker but NOT within a begin/end update.
  void ContentStateChanged(nsIContent* aContent, EventStates aStateMask);

  // Update a set of document states that may have changed.
  // This should only be called by callers whose state is also reflected in the
  // implementation of Document::GetDocumentState.
  //
  // aNotify controls whether we notify our DocumentStatesChanged observers.
  void UpdateDocumentStates(EventStates aStateMask, bool aNotify);

  void ResetDocumentDirection();

  // Observation hooks for style data to propagate notifications
  // to document observers
  void StyleRuleChanged(StyleSheet* aStyleSheet, css::Rule* aStyleRule);
  void StyleRuleAdded(StyleSheet* aStyleSheet, css::Rule* aStyleRule);
  void StyleRuleRemoved(StyleSheet* aStyleSheet, css::Rule* aStyleRule);

  /**
   * Flush notifications for this document and its parent documents
   * (since those may affect the layout of this one).
   */
  void FlushPendingNotifications(FlushType aType);

  /**
   * Another variant of the above FlushPendingNotifications.  This function
   * takes a ChangesToFlush to specify whether throttled animations are flushed
   * or not.
   * If in doublt, use the above FlushPendingNotifications.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void FlushPendingNotifications(ChangesToFlush aFlush);

  /**
   * Calls FlushPendingNotifications on any external resources this document
   * has. If this document has no external resources or is an external resource
   * itself this does nothing. This should only be called with
   * aType >= FlushType::Style.
   */
  void FlushExternalResources(FlushType aType);

  // Triggers an update of <svg:use> element shadow trees.
  void UpdateSVGUseElementShadowTrees() {
    if (mSVGUseElementsNeedingShadowTreeUpdate.IsEmpty()) {
      return;
    }
    DoUpdateSVGUseElementShadowTrees();
  }

  nsBindingManager* BindingManager() const {
    return mNodeInfoManager->GetBindingManager();
  }

  /**
   * Only to be used inside Gecko, you can't really do anything with the
   * pointer outside Gecko anyway.
   */
  nsNodeInfoManager* NodeInfoManager() const { return mNodeInfoManager; }

  /**
   * Reset the document using the given channel and loadgroup.  This works
   * like ResetToURI, but also sets the document's channel to aChannel.
   * The principal of the document will be set from the channel.
   */
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);

  /**
   * Reset this document to aURI, aLoadGroup, aPrincipal and aStoragePrincipal.
   * aURI must not be null.  If aPrincipal is null, a content principal based
   * on aURI will be used.
   */
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal,
                          nsIPrincipal* aStoragePrincipal);

  /**
   * Set the container (docshell) for this document. Virtual so that
   * docshell can call it.
   */
  virtual void SetContainer(nsDocShell* aContainer);

  /**
   * Get the container (docshell) for this document.
   */
  nsISupports* GetContainer() const;

  /**
   * Get the container's load context for this document.
   */
  nsILoadContext* GetLoadContext() const;

  /**
   * Get docshell the for this document.
   */
  nsIDocShell* GetDocShell() const;

  /**
   * Set and get XML declaration. If aVersion is null there is no declaration.
   * aStandalone takes values -1, 0 and 1 indicating respectively that there
   * was no standalone parameter in the declaration, that it was given as no,
   * or that it was given as yes.
   */
  void SetXMLDeclaration(const char16_t* aVersion, const char16_t* aEncoding,
                         const int32_t aStandalone);
  void GetXMLDeclaration(nsAString& aVersion, nsAString& aEncoding,
                         nsAString& Standalone);

  /**
   * Returns true if this is what HTML 5 calls an "HTML document" (for example
   * regular HTML document with Content-Type "text/html", image documents and
   * media documents).  Returns false for XHTML and any other documents parsed
   * by the XML parser.
   */
  bool IsHTMLDocument() const { return mType == eHTML; }
  bool IsHTMLOrXHTML() const { return mType == eHTML || mType == eXHTML; }
  bool IsXMLDocument() const { return !IsHTMLDocument(); }
  bool IsSVGDocument() const { return mType == eSVG; }
  bool IsUnstyledDocument() {
    return IsLoadedAsData() || IsLoadedAsInteractiveData();
  }
  bool LoadsFullXULStyleSheetUpFront() {
    if (IsSVGDocument()) {
      return false;
    }
    return AllowXULXBL();
  }

  bool IsScriptEnabled();

  /**
   * Returns true if this document was created from a nsXULPrototypeDocument.
   */
  bool LoadedFromPrototype() const { return mPrototypeDocument; }
  /**
   * Returns the prototype the document was created from, or null if it was not
   * created from a prototype.
   */
  nsXULPrototypeDocument* GetPrototype() const { return mPrototypeDocument; }

  bool IsTopLevelContentDocument() const { return mIsTopLevelContentDocument; }
  void SetIsTopLevelContentDocument(bool aIsTopLevelContentDocument) {
    mIsTopLevelContentDocument = aIsTopLevelContentDocument;
    // When a document is set as TopLevelContentDocument, it must be
    // allowpaymentrequest. We handle the false case while a document is
    // appended in SetSubDocumentFor
    if (aIsTopLevelContentDocument) {
      SetAllowPaymentRequest(true);
    }
  }

  bool IsContentDocument() const { return mIsContentDocument; }
  void SetIsContentDocument(bool aIsContentDocument) {
    mIsContentDocument = aIsContentDocument;
  }

  /**
   * Create an element with the specified name, prefix and namespace ID.
   * Returns null if element name parsing failed.
   */
  already_AddRefed<Element> CreateElem(const nsAString& aName, nsAtom* aPrefix,
                                       int32_t aNamespaceID,
                                       const nsAString* aIs = nullptr);

  /**
   * Get the security info (i.e. SSL state etc) that the document got
   * from the channel/document that created the content of the
   * document.
   *
   * @see nsIChannel
   */
  nsISupports* GetSecurityInfo() { return mSecurityInfo; }

  /**
   * Get the channel that failed to load and resulted in an error page, if it
   * exists. This is only relevant to error pages.
   */
  nsIChannel* GetFailedChannel() const { return mFailedChannel; }

  /**
   * This function checks if the document that is trying to access
   * GetNetErrorInfo is a trusted about net error page or not.
   */
  static bool CallerIsTrustedAboutNetError(JSContext* aCx, JSObject* aObject);

  /**
   * Get security info like error code for a failed channel. This
   * property is only exposed to about:neterror documents.
   */
  void GetNetErrorInfo(mozilla::dom::NetErrorInfo& aInfo, ErrorResult& aRv);

  /**
   * This function checks if the document that is trying to access
   * GetFailedCertSecurityInfo is a trusted cert error page or not.
   */
  static bool CallerIsTrustedAboutCertError(JSContext* aCx, JSObject* aObject);

  /**
   * Get the security info (i.e. certificate validity, errorCode, etc) for a
   * failed Channel. This property is only exposed for about:certerror
   * documents.
   */
  void GetFailedCertSecurityInfo(mozilla::dom::FailedCertSecurityInfo& aInfo,
                                 ErrorResult& aRv);

  /**
   * Set the channel that failed to load and resulted in an error page.
   * This is only relevant to error pages.
   */
  void SetFailedChannel(nsIChannel* aChannel) { mFailedChannel = aChannel; }

  /**
   * Returns the default namespace ID used for elements created in this
   * document.
   */
  int32_t GetDefaultNamespaceID() const { return mDefaultElementType; }

  void DeleteAllProperties();
  void DeleteAllPropertiesFor(nsINode* aNode);

  nsPropertyTable& PropertyTable() { return mPropertyTable; }

  /**
   * Sets the ID used to identify this part of the multipart document
   */
  void SetPartID(uint32_t aID) { mPartID = aID; }

  /**
   * Return the ID used to identify this part of the multipart document
   */
  uint32_t GetPartID() const { return mPartID; }

  /**
   * Sanitize the document by resetting all input elements and forms that have
   * autocomplete=off to their default values.
   */
  void Sanitize();

  /**
   * Enumerate all subdocuments.
   * The enumerator callback should return true to continue enumerating, or
   * false to stop.  This will never get passed a null aDocument.
   */
  typedef bool (*SubDocEnumFunc)(Document* aDocument, void* aData);
  void EnumerateSubDocuments(SubDocEnumFunc aCallback, void* aData);

  /**
   * Collect all the descendant documents for which |aCalback| returns true.
   * The callback function must not mutate any state for the given document.
   */
  typedef bool (*nsDocTestFunc)(const Document* aDocument);
  void CollectDescendantDocuments(nsTArray<RefPtr<Document>>& aDescendants,
                                  nsDocTestFunc aCallback) const;

  /**
   * Check whether it is safe to cache the presentation of this document
   * and all of its subdocuments. This method checks the following conditions
   * recursively:
   *  - Some document types, such as plugin documents, cannot be safely cached.
   *  - If there are any pending requests, we don't allow the presentation
   *    to be cached.  Ideally these requests would be suspended and resumed,
   *    but that is difficult in some cases, such as XMLHttpRequest.
   *  - If there are any beforeunload or unload listeners, we must fire them
   *    for correctness, but this likely puts the document into a state where
   *    it would not function correctly if restored.
   *
   * |aNewRequest| should be the request for a new document which will
   * replace this document in the docshell.  The new document's request
   * will be ignored when checking for active requests.  If there is no
   * request associated with the new document, this parameter may be null.
   *
   * |aBFCacheCombo| is used as a bitmask to indicate what the status
   * combination is when we try to BFCache aNewRequest
   */
  virtual bool CanSavePresentation(nsIRequest* aNewRequest,
                                   uint16_t& aBFCacheCombo);

  virtual nsresult Init();

  /**
   * Notify the document that its associated ContentViewer is being destroyed.
   * This releases circular references so that the document can go away.
   * Destroy() is only called on documents that have a content viewer.
   */
  virtual void Destroy();

  /**
   * Notify the document that its associated ContentViewer is no longer
   * the current viewer for the docshell. The document might still
   * be rendered in "zombie state" until the next document is ready.
   * The document should save form control state.
   */
  void RemovedFromDocShell();

  /**
   * Get the layout history state that should be used to save and restore state
   * for nodes in this document.  This may return null; if that happens state
   * saving and restoration is not possible.
   */
  already_AddRefed<nsILayoutHistoryState> GetLayoutHistoryState() const;

  /**
   * Methods that can be used to prevent onload firing while an event that
   * should block onload is posted.  onload is guaranteed to not fire until
   * either all calls to BlockOnload() have been matched by calls to
   * UnblockOnload() or the load has been stopped altogether (by the user
   * pressing the Stop button, say).
   */
  void BlockOnload();
  /**
   * @param aFireSync whether to fire onload synchronously.  If false,
   * onload will fire asynchronously after all onload blocks have been
   * removed.  It will NOT fire from inside UnblockOnload.  If true,
   * onload may fire from inside UnblockOnload.
   */
  void UnblockOnload(bool aFireSync);

  // Only BlockOnload should call this!
  void AsyncBlockOnload();

  void BlockDOMContentLoaded() { ++mBlockDOMContentLoaded; }

  void UnblockDOMContentLoaded();

  /**
   * Notification that the page has been shown, for documents which are loaded
   * into a DOM window.  This corresponds to the completion of document load,
   * or to the page's presentation being restored into an existing DOM window.
   * This notification fires applicable DOM events to the content window.  See
   * PageTransitionEvent.webidl for a description of the |aPersisted|
   * parameter. If aDispatchStartTarget is null, the pageshow event is
   * dispatched on the ScriptGlobalObject for this document, otherwise it's
   * dispatched on aDispatchStartTarget. If |aOnlySystemGroup| is true, the
   * event is only dispatched to listeners in the system group.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  virtual void OnPageShow(bool aPersisted, EventTarget* aDispatchStartTarget,
                          bool aOnlySystemGroup = false);

  /**
   * Notification that the page has been hidden, for documents which are loaded
   * into a DOM window.  This corresponds to the unloading of the document, or
   * to the document's presentation being saved but removed from an existing
   * DOM window.  This notification fires applicable DOM events to the content
   * window.  See PageTransitionEvent.webidl for a description of the
   * |aPersisted| parameter. If aDispatchStartTarget is null, the pagehide
   * event is dispatched on the ScriptGlobalObject for this document,
   * otherwise it's dispatched on aDispatchStartTarget. If |aOnlySystemGroup| is
   * true, the event is only dispatched to listeners in the system group.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  void OnPageHide(bool aPersisted, EventTarget* aDispatchStartTarget,
                  bool aOnlySystemGroup = false);

  /*
   * We record the set of links in the document that are relevant to
   * style.
   */
  /**
   * Notification that an element is a link that is relevant to style.
   */
  void AddStyleRelevantLink(Link* aLink) {
    NS_ASSERTION(aLink, "Passing in a null link.  Expect crashes RSN!");
#ifdef DEBUG
    nsPtrHashKey<Link>* entry = mStyledLinks.GetEntry(aLink);
    NS_ASSERTION(!entry, "Document already knows about this Link!");
    mStyledLinksCleared = false;
#endif
    mStyledLinks.PutEntry(aLink);
  }

  /**
   * Notification that an element is a link and its URI might have been
   * changed or the element removed. If the element is still a link relevant
   * to style, then someone must ensure that AddStyleRelevantLink is
   * (eventually) called on it again.
   */
  void ForgetLink(Link* aLink) {
    NS_ASSERTION(aLink, "Passing in a null link.  Expect crashes RSN!");
#ifdef DEBUG
    nsPtrHashKey<Link>* entry = mStyledLinks.GetEntry(aLink);
    NS_ASSERTION(entry || mStyledLinksCleared,
                 "Document knows nothing about this Link!");
#endif
    mStyledLinks.RemoveEntry(aLink);
  }

  // Refreshes the hrefs of all the links in the document.
  void RefreshLinkHrefs();

  /**
   * Support for window.matchMedia()
   */

  already_AddRefed<MediaQueryList> MatchMedia(const nsAString& aMediaQueryList,
                                              CallerType aCallerType);

  LinkedList<MediaQueryList>& MediaQueryLists() { return mDOMMediaQueryLists; }

  /**
   * Get the compatibility mode for this document
   */
  nsCompatibility GetCompatibilityMode() const { return mCompatMode; }

  /**
   * Check whether we've ever fired a DOMTitleChanged event for this
   * document.
   */
  bool HaveFiredDOMTitleChange() const { return mHaveFiredTitleChange; }

  Element* GetAnonymousElementByAttribute(nsIContent* aElement,
                                          nsAtom* aAttrName,
                                          const nsAString& aAttrValue) const;

  /**
   * To batch DOMSubtreeModified, document needs to be informed when
   * a mutation event might be dispatched, even if the event isn't actually
   * created because there are no listeners for it.
   *
   * @param aTarget is the target for the mutation event.
   */
  void MayDispatchMutationEvent(nsINode* aTarget) {
    if (mSubtreeModifiedDepth > 0) {
      mSubtreeModifiedTargets.AppendObject(aTarget);
    }
  }

  /**
   * Marks as not-going-to-be-collected for the given generation of
   * cycle collection.
   */
  void MarkUncollectableForCCGeneration(uint32_t aGeneration) {
    mMarkedCCGeneration = aGeneration;
  }

  /**
   * Gets the cycle collector generation this document is marked for.
   */
  uint32_t GetMarkedCCGeneration() { return mMarkedCCGeneration; }

  /**
   * Returns whether this document is cookie averse. See
   * https://html.spec.whatwg.org/multipage/dom.html#cookie-averse-document-object
   */
  bool IsCookieAverse() const {
    // If we are a document that "has no browsing context."
    if (!GetInnerWindow()) {
      return true;
    }

    // If we are a document "whose URL's scheme is not a network scheme."
    // NB: Explicitly allow file: URIs to store cookies.
    nsCOMPtr<nsIURI> contentURI;
    NodePrincipal()->GetURI(getter_AddRefs(contentURI));

    if (!contentURI) {
      return true;
    }

    nsAutoCString scheme;
    contentURI->GetScheme(scheme);
    return !scheme.EqualsLiteral("http") && !scheme.EqualsLiteral("https") &&
           !scheme.EqualsLiteral("ftp") && !scheme.EqualsLiteral("file");
  }

  bool IsLoadedAsData() { return mLoadedAsData; }

  bool IsLoadedAsInteractiveData() { return mLoadedAsInteractiveData; }

  bool MayStartLayout() { return mMayStartLayout; }

  void SetMayStartLayout(bool aMayStartLayout);

  already_AddRefed<nsIDocumentEncoder> GetCachedEncoder();

  void SetCachedEncoder(already_AddRefed<nsIDocumentEncoder> aEncoder);

  // In case of failure, the document really can't initialize the frame loader.
  nsresult InitializeFrameLoader(nsFrameLoader* aLoader);
  // In case of failure, the caller must handle the error, for example by
  // finalizing frame loader asynchronously.
  nsresult FinalizeFrameLoader(nsFrameLoader* aLoader, nsIRunnable* aFinalizer);
  // Removes the frame loader of aShell from the initialization list.
  void TryCancelFrameLoaderInitialization(nsIDocShell* aShell);

  /**
   * Check whether this document is a root document that is not an
   * external resource.
   */
  bool IsRootDisplayDocument() const {
    return !mParentDocument && !mDisplayDocument;
  }

  bool IsDocumentURISchemeChrome() const { return mDocURISchemeIsChrome; }

  bool IsInChromeDocShell() const {
    const Document* root = this;
    while (const Document* displayDoc = root->GetDisplayDocument()) {
      root = displayDoc;
    }
    return root->mInChromeDocShell;
  }

  bool IsDevToolsDocument() const { return mIsDevToolsDocument; }

  bool IsBeingUsedAsImage() const { return mIsBeingUsedAsImage; }

  void SetIsBeingUsedAsImage() { mIsBeingUsedAsImage = true; }

  bool IsSVGGlyphsDocument() const { return mIsSVGGlyphsDocument; }

  void SetIsSVGGlyphsDocument() { mIsSVGGlyphsDocument = true; }

  bool IsResourceDoc() const {
    return IsBeingUsedAsImage() ||  // Are we a helper-doc for an SVG image?
           mHasDisplayDocument;     // Are we an external resource doc?
  }

  /**
   * Get the document for which this document is an external resource.  This
   * will be null if this document is not an external resource.  Otherwise,
   * GetDisplayDocument() will return a non-null document, and
   * GetDisplayDocument()->GetDisplayDocument() is guaranteed to be null.
   */
  Document* GetDisplayDocument() const { return mDisplayDocument; }

  /**
   * Set the display document for this document.  aDisplayDocument must not be
   * null.
   */
  void SetDisplayDocument(Document* aDisplayDocument) {
    MOZ_ASSERT(!GetPresShell() && !GetContainer() && !GetWindow(),
               "Shouldn't set mDisplayDocument on documents that already "
               "have a presentation or a docshell or a window");
    MOZ_ASSERT(aDisplayDocument, "Must not be null");
    MOZ_ASSERT(aDisplayDocument != this, "Should be different document");
    MOZ_ASSERT(!aDisplayDocument->GetDisplayDocument(),
               "Display documents should not nest");
    mDisplayDocument = aDisplayDocument;
    mHasDisplayDocument = !!aDisplayDocument;
  }

  /**
   * Request an external resource document for aURI.  This will return the
   * resource document if available.  If one is not available yet, it will
   * start loading as needed, and the pending load object will be returned in
   * aPendingLoad so that the caller can register an observer to wait for the
   * load.  If this function returns null and doesn't return a pending load,
   * that means that there is no resource document for this URI and won't be
   * one in the future.
   *
   * @param aURI the URI to get
   * @param aReferrerInfo the referrerInfo of the request
   * @param aRequestingNode the node making the request
   * @param aPendingLoad the pending load for this request, if any
   */
  Document* RequestExternalResource(nsIURI* aURI,
                                    nsIReferrerInfo* aReferrerInfo,
                                    nsINode* aRequestingNode,
                                    ExternalResourceLoad** aPendingLoad);

  /**
   * Enumerate the external resource documents associated with this document.
   * The enumerator callback should return true to continue enumerating, or
   * false to stop.  This callback will never get passed a null aDocument.
   */
  void EnumerateExternalResources(SubDocEnumFunc aCallback, void* aData);

  dom::ExternalResourceMap& ExternalResourceMap() {
    return mExternalResourceMap;
  }

  /**
   * Return whether the document is currently showing (in the sense of
   * OnPageShow() having been called already and OnPageHide() not having been
   * called yet.
   */
  bool IsShowing() const { return mIsShowing; }
  /**
   * Return whether the document is currently visible (in the sense of
   * OnPageHide having been called and OnPageShow not yet having been called)
   */
  bool IsVisible() const { return mVisible; }

  /**
   * Return whether the document and all its ancestors are visible in the sense
   * of pageshow / hide.
   */
  bool IsVisibleConsideringAncestors() const;

  void SetSuppressedEventListener(EventListener* aListener);

  EventListener* GetSuppressedEventListener() {
    return mSuppressedEventListener;
  }

  /**
   * Return true when this document is active, i.e., an active document
   * in a content viewer.  Note that this will return true for bfcached
   * documents, so this does NOT match the "active document" concept in
   * the WHATWG spec - see IsCurrentActiveDocument.
   */
  bool IsActive() const { return mDocumentContainer && !mRemovedFromDocShell; }

  /**
   * Return true if this is the current active document for its
   * docshell. Note that a docshell may have multiple active documents
   * due to the bfcache -- this should be used when you need to
   * differentiate the *current* active document from any active
   * documents.
   */
  bool IsCurrentActiveDocument() const {
    nsPIDOMWindowInner* inner = GetInnerWindow();
    return inner && inner->IsCurrentInnerWindow() && inner->GetDoc() == this;
  }

  /**
   * Returns whether this document should perform image loads.
   */
  bool ShouldLoadImages() const {
    // We check IsBeingUsedAsImage() so that SVG documents loaded as
    // images can themselves have data: URL image references.
    return IsCurrentActiveDocument() || IsBeingUsedAsImage();
  }

  /**
   * Register/Unregister the ActivityObserver into mActivityObservers to listen
   * the document's activity changes such as OnPageHide, visibility, activity.
   * The ActivityObserver objects can be nsIObjectLoadingContent or
   * nsIDocumentActivity or HTMLMEdiaElement.
   */
  void RegisterActivityObserver(nsISupports* aSupports);
  bool UnregisterActivityObserver(nsISupports* aSupports);
  // Enumerate all the observers in mActivityObservers by the aEnumerator.
  typedef void (*ActivityObserverEnumerator)(nsISupports*, void*);
  void EnumerateActivityObservers(ActivityObserverEnumerator aEnumerator,
                                  void* aData);

  // Indicates whether mAnimationController has been (lazily) initialized.
  // If this returns true, we're promising that GetAnimationController()
  // will have a non-null return value.
  bool HasAnimationController() { return !!mAnimationController; }

  // Getter for this document's SMIL Animation Controller. Performs lazy
  // initialization, if this document supports animation and if
  // mAnimationController isn't yet initialized.
  //
  // If HasAnimationController is true, this is guaranteed to return non-null.
  SMILAnimationController* GetAnimationController();

  // Gets the tracker for animations that are waiting to start.
  // Returns nullptr if there is no pending animation tracker for this document
  // which will be the case if there have never been any CSS animations or
  // transitions on elements in the document.
  PendingAnimationTracker* GetPendingAnimationTracker() {
    return mPendingAnimationTracker;
  }

  // Gets the tracker for animations that are waiting to start and
  // creates it if it doesn't already exist. As a result, the return value
  // will never be nullptr.
  PendingAnimationTracker* GetOrCreatePendingAnimationTracker();

  /**
   * Prevents user initiated events from being dispatched to the document and
   * subdocuments.
   */
  void SuppressEventHandling(uint32_t aIncrease = 1);

  /**
   * Unsuppress event handling.
   * @param aFireEvents If true, delayed events (focus/blur) will be fired
   *                    asynchronously.
   */
  void UnsuppressEventHandlingAndFireEvents(bool aFireEvents);

  uint32_t EventHandlingSuppressed() const { return mEventsSuppressed; }

  bool IsEventHandlingEnabled() {
    return !EventHandlingSuppressed() && mScriptGlobalObject;
  }

  void DecreaseEventSuppression() {
    MOZ_ASSERT(mEventsSuppressed);
    --mEventsSuppressed;
    UpdateFrameRequestCallbackSchedulingState();
  }

  /**
   * Note a ChannelEventQueue which has been suspended on the document's behalf
   * to prevent XHRs from running content scripts while event handling is
   * suppressed. The document is responsible for resuming the queue after
   * event handling is unsuppressed.
   */
  void AddSuspendedChannelEventQueue(net::ChannelEventQueue* aQueue);

  /**
   * Returns true if a postMessage event should be suspended instead of running.
   * The document is responsible for running the event later, in the order they
   * were received.
   */
  bool SuspendPostMessageEvent(PostMessageEvent* aEvent);

  /**
   * Run any suspended postMessage events, or clear them.
   */
  void FireOrClearPostMessageEvents(bool aFireEvents);

  void SetHasDelayedRefreshEvent() { mHasDelayedRefreshEvent = true; }

  /**
   * Flag whether we're about to fire the window's load event for this document.
   */
  void SetLoadEventFiring(bool aFiring) { mLoadEventFiring = aFiring; }

  /**
   * Test whether we should be firing a load event for this document after a
   * document.close().  This is public and on Document, instead of being private
   * to Document, because we need to go through the normal docloader logic
   * for the readystate change to READYSTATE_COMPLETE with the normal timing and
   * semantics of firing the load event; we just don't want to fire the load
   * event if this tests true.  So we need the docloader to be able to access
   * this state.
   *
   * This method should only be called at the point when the load event is about
   * to be fired.  It resets the "skip" flag, so it is not idempotent.
   */
  bool SkipLoadEventAfterClose() {
    bool skip = mSkipLoadEventAfterClose;
    mSkipLoadEventAfterClose = false;
    return skip;
  }

  /**
   * Increment https://html.spec.whatwg.org/#ignore-destructive-writes-counter
   */
  void IncrementIgnoreDestructiveWritesCounter() {
    ++mIgnoreDestructiveWritesCounter;
  }

  /**
   * Decrement https://html.spec.whatwg.org/#ignore-destructive-writes-counter
   */
  void DecrementIgnoreDestructiveWritesCounter() {
    --mIgnoreDestructiveWritesCounter;
  }

  bool IsDNSPrefetchAllowed() const { return mAllowDNSPrefetch; }

  /**
   * Returns true if this document is allowed to contain XUL element and
   * use non-builtin XBL bindings.
   */
  bool AllowXULXBL() {
    return mAllowXULXBL == eTriTrue
               ? true
               : mAllowXULXBL == eTriFalse ? false : InternalAllowXULXBL();
  }

  /**
   * Returns true if this document is allowed to load DTDs from UI resources
   * no matter what.
   */
  bool SkipDTDSecurityChecks() { return mSkipDTDSecurityChecks; }

  void ForceEnableXULXBL() { mAllowXULXBL = eTriTrue; }

  void ForceSkipDTDSecurityChecks() { mSkipDTDSecurityChecks = true; }

  /**
   * Returns the template content owner document that owns the content of
   * HTMLTemplateElement.
   */
  Document* GetTemplateContentsOwner();

  /**
   * Returns true if this document is a static clone of a normal document.
   *
   * We create static clones for print preview and printing (possibly other
   * things in future).
   *
   * Note that static documents are also "loaded as data" (if this method
   * returns true, IsLoadedAsData() will also return true).
   */
  bool IsStaticDocument() { return mIsStaticDocument; }

  /**
   * Clones the document along with any subdocuments, stylesheet, etc.
   *
   * The resulting document and everything it contains (including any
   * sub-documents) are created purely via cloning.  The returned documents and
   * any sub-documents are "loaded as data" documents to preserve the state as
   * it was during the clone process (we don't want external resources to load
   * and replace the cloned resources).
   *
   * @param aCloneContainer The container for the clone document.
   */
  virtual already_AddRefed<Document> CreateStaticClone(
      nsIDocShell* aCloneContainer);

  /**
   * If this document is a static clone, this returns the original
   * document.
   */
  Document* GetOriginalDocument() {
    MOZ_ASSERT(!mOriginalDocument || !mOriginalDocument->GetOriginalDocument());
    return mOriginalDocument;
  }

  /**
   * If this document is a static clone, let the original document know that
   * we're going away and then release our reference to it.
   */
  void UnlinkOriginalDocumentIfStatic();

  /**
   * These are called by the parser as it encounters <picture> tags, the end of
   * said tags, and possible picture <source srcset> sources respectively. These
   * are used to inform ResolvePreLoadImage() calls.  Unset attributes are
   * expected to be marked void.
   *
   * NOTE that the parser does not attempt to track the current picture nesting
   * level or whether the given <source> tag is within a picture -- it is only
   * guaranteed to order these calls properly with respect to
   * ResolvePreLoadImage.
   */

  void PreloadPictureOpened() { mPreloadPictureDepth++; }

  void PreloadPictureClosed();

  void PreloadPictureImageSource(const nsAString& aSrcsetAttr,
                                 const nsAString& aSizesAttr,
                                 const nsAString& aTypeAttr,
                                 const nsAString& aMediaAttr);

  /**
   * Called by the parser to resolve an image for preloading. The parser will
   * call the PreloadPicture* functions to inform us of possible <picture>
   * nesting and possible sources, which are used to inform URL selection
   * responsive <picture> or <img srcset> images.  Unset attributes are expected
   * to be marked void.
   * If this image is for <picture> or <img srcset>, aIsImgSet will be set to
   * true, false otherwise.
   */
  already_AddRefed<nsIURI> ResolvePreloadImage(nsIURI* aBaseURI,
                                               const nsAString& aSrcAttr,
                                               const nsAString& aSrcsetAttr,
                                               const nsAString& aSizesAttr,
                                               bool* aIsImgSet);
  /**
   * Called by nsParser to preload images. Can be removed and code moved
   * to nsPreloadURIs::PreloadURIs() in file nsParser.cpp whenever the
   * parser-module is linked with gklayout-module.  aCrossOriginAttr should
   * be a void string if the attr is not present.
   * aIsImgSet is the value got from calling ResolvePreloadImage, it is true
   * when this image is for loading <picture> or <img srcset> images.
   */
  void MaybePreLoadImage(nsIURI* uri, const nsAString& aCrossOriginAttr,
                         ReferrerPolicyEnum aReferrerPolicy, bool aIsImgSet);

  /**
   * Called by images to forget an image preload when they start doing
   * the real load.
   */
  void ForgetImagePreload(nsIURI* aURI);

  /**
   * Called by nsParser to preload style sheets.  aCrossOriginAttr should be a
   * void string if the attr is not present.
   */
  void PreloadStyle(nsIURI* aURI, const Encoding* aEncoding,
                    const nsAString& aCrossOriginAttr,
                    ReferrerPolicyEnum aReferrerPolicy,
                    const nsAString& aIntegrity);

  /**
   * Called by the chrome registry to load style sheets.
   *
   * This always does a synchronous load, and parses as a normal document sheet.
   */
  RefPtr<StyleSheet> LoadChromeSheetSync(nsIURI* aURI);

  /**
   * Returns true if the locale used for the document specifies a direction of
   * right to left. For chrome documents, this comes from the chrome registry.
   * This is used to determine the current state for the :-moz-locale-dir
   * pseudoclass so once can know whether a document is expected to be rendered
   * left-to-right or right-to-left.
   */
  bool IsDocumentRightToLeft();

  /**
   * Called by Parser for link rel=preconnect
   */
  void MaybePreconnect(nsIURI* uri, CORSMode aCORSMode);

  enum DocumentTheme {
    Doc_Theme_Uninitialized,  // not determined yet
    Doc_Theme_None,
    Doc_Theme_Neutral,
    Doc_Theme_Dark,
    Doc_Theme_Bright
  };

  /**
   * Set the document's pending state object (as serialized using structured
   * clone).
   */
  void SetStateObject(nsIStructuredCloneContainer* scContainer);

  /**
   * Set the document's pending state object to the same state object as
   * aDocument.
   */
  void SetStateObjectFrom(Document* aDocument) {
    SetStateObject(aDocument->mStateObjectContainer);
  }

  /**
   * Returns Doc_Theme_None if there is no lightweight theme specified,
   * Doc_Theme_Dark for a dark theme, Doc_Theme_Bright for a light theme, and
   * Doc_Theme_Neutral for any other theme. This is used to determine the state
   * of the pseudoclasses :-moz-lwtheme and :-moz-lwtheme-text.
   */
  DocumentTheme GetDocumentLWTheme();
  DocumentTheme ThreadSafeGetDocumentLWTheme() const;
  void ResetDocumentLWTheme() { mDocLWTheme = Doc_Theme_Uninitialized; }

  // Whether we're a media document or not.
  enum class MediaDocumentKind {
    NotMedia,
    Video,
    Image,
    Plugin,
  };

  virtual enum MediaDocumentKind MediaDocumentKind() const {
    return MediaDocumentKind::NotMedia;
  }

  /**
   * Returns the document state.
   * Document state bits have the form NS_DOCUMENT_STATE_* and are declared in
   * Document.h.
   */
  EventStates GetDocumentState() const { return mDocumentState; }

  nsISupports* GetCurrentContentSink();

  void SetAutoFocusElement(Element* aAutoFocusElement);
  void TriggerAutoFocus();

  void SetScrollToRef(nsIURI* aDocumentURI);
  MOZ_CAN_RUN_SCRIPT void ScrollToRef();
  void ResetScrolledToRefAlready() { mScrolledToRefAlready = false; }

  void SetChangeScrollPosWhenScrollingToRef(bool aValue) {
    mChangeScrollPosWhenScrollingToRef = aValue;
  }

  using DocumentOrShadowRoot::GetElementById;
  using DocumentOrShadowRoot::GetElementsByClassName;
  using DocumentOrShadowRoot::GetElementsByTagName;
  using DocumentOrShadowRoot::GetElementsByTagNameNS;

  DocumentTimeline* Timeline();
  LinkedList<DocumentTimeline>& Timelines() { return mTimelines; }

  void GetAnimations(nsTArray<RefPtr<Animation>>& aAnimations);

  SVGSVGElement* GetSVGRootElement() const;

  struct FrameRequest {
    FrameRequest(FrameRequestCallback& aCallback, int32_t aHandle);

    // Comparator operators to allow RemoveElementSorted with an
    // integer argument on arrays of FrameRequest
    bool operator==(int32_t aHandle) const { return mHandle == aHandle; }
    bool operator<(int32_t aHandle) const { return mHandle < aHandle; }

    RefPtr<FrameRequestCallback> mCallback;
    int32_t mHandle;
  };

  nsresult ScheduleFrameRequestCallback(FrameRequestCallback& aCallback,
                                        int32_t* aHandle);
  void CancelFrameRequestCallback(int32_t aHandle);

  /**
   * Returns true if the handle refers to a callback that was canceled that
   * we did not find in our list of callbacks (e.g. because it is one of those
   * in the set of callbacks currently queued to be run).
   */
  bool IsCanceledFrameRequestCallback(int32_t aHandle) const;

  /**
   * Put this document's frame request callbacks into the provided
   * list, and forget about them.
   */
  void TakeFrameRequestCallbacks(nsTArray<FrameRequest>& aCallbacks);

  /**
   * @return true if this document's frame request callbacks should be
   * throttled. We throttle requestAnimationFrame for documents which aren't
   * visible (e.g. scrolled out of the viewport).
   */
  bool ShouldThrottleFrameRequests();

  // This returns true when the document tree is being teared down.
  bool InUnlinkOrDeletion() { return mInUnlinkOrDeletion; }

  dom::ImageTracker* ImageTracker();

  // AddPlugin adds a plugin-related element to mPlugins when the element is
  // added to the tree.
  void AddPlugin(nsIObjectLoadingContent* aPlugin) {
    MOZ_ASSERT(aPlugin);
    mPlugins.PutEntry(aPlugin);
  }

  // RemovePlugin removes a plugin-related element to mPlugins when the
  // element is removed from the tree.
  void RemovePlugin(nsIObjectLoadingContent* aPlugin) {
    MOZ_ASSERT(aPlugin);
    mPlugins.RemoveEntry(aPlugin);
  }

  // GetPlugins returns the plugin-related elements from
  // the frame and any subframes.
  void GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins);

  // Adds an element to mResponsiveContent when the element is
  // added to the tree.
  void AddResponsiveContent(HTMLImageElement* aContent) {
    MOZ_ASSERT(aContent);
    mResponsiveContent.PutEntry(aContent);
  }

  // Removes an element from mResponsiveContent when the element is
  // removed from the tree.
  void RemoveResponsiveContent(HTMLImageElement* aContent) {
    MOZ_ASSERT(aContent);
    mResponsiveContent.RemoveEntry(aContent);
  }

  void ScheduleSVGUseElementShadowTreeUpdate(SVGUseElement&);
  void UnscheduleSVGUseElementShadowTreeUpdate(SVGUseElement& aElement) {
    mSVGUseElementsNeedingShadowTreeUpdate.RemoveEntry(&aElement);
  }

  bool SVGUseElementNeedsShadowTreeUpdate(SVGUseElement& aElement) const {
    return mSVGUseElementsNeedingShadowTreeUpdate.GetEntry(&aElement);
  }

  using ShadowRootSet = nsTHashtable<nsPtrHashKey<ShadowRoot>>;

  void AddComposedDocShadowRoot(ShadowRoot& aShadowRoot) {
    mComposedShadowRoots.PutEntry(&aShadowRoot);
  }

  void RemoveComposedDocShadowRoot(ShadowRoot& aShadowRoot) {
    mComposedShadowRoots.RemoveEntry(&aShadowRoot);
  }

  // If you're considering using this, you probably want to use
  // ShadowRoot::IsComposedDocParticipant instead. This is just for
  // sanity-checking.
  bool IsComposedDocShadowRoot(ShadowRoot& aShadowRoot) {
    return mComposedShadowRoots.Contains(&aShadowRoot);
  }

  const ShadowRootSet& ComposedShadowRoots() const {
    return mComposedShadowRoots;
  }

  // Notifies any responsive content added by AddResponsiveContent upon media
  // features values changing.
  void NotifyMediaFeatureValuesChanged();

  nsresult GetStateObject(nsIVariant** aResult);

  nsDOMNavigationTiming* GetNavigationTiming() const { return mTiming; }

  void SetNavigationTiming(nsDOMNavigationTiming* aTiming);

  nsContentList* ImageMapList();

  // Add aLink to the set of links that need their status resolved.
  void RegisterPendingLinkUpdate(Link* aLink);

  // Update state on links in mLinksToUpdate.  This function must be called
  // prior to selector matching that needs to differentiate between :link and
  // :visited.  In particular, it does _not_ need to be called before doing any
  // selector matching that uses TreeMatchContext::eNeverMatchVisited.  The only
  // reason we haven't moved all calls to this function entirely inside the
  // TreeMatchContext constructor is to not call it all the time during various
  // style system and frame construction operations (though it would likely be a
  // no-op for all but the first call).
  //
  // XXXbz Does this really need to be called before selector matching?  All it
  // will do is ensure all the links involved are registered to observe history,
  // which won't synchronously change their state to :visited anyway!  So
  // calling this won't affect selector matching done immediately afterward, as
  // far as I can tell.
  void FlushPendingLinkUpdates();

  void FlushPendingLinkUpdatesFromRunnable();

#define DEPRECATED_OPERATION(_op) e##_op,
  enum DeprecatedOperations {
#include "nsDeprecatedOperationList.h"
    eDeprecatedOperationCount
  };
#undef DEPRECATED_OPERATION
  bool HasWarnedAbout(DeprecatedOperations aOperation) const;
  void WarnOnceAbout(DeprecatedOperations aOperation,
                     bool asError = false) const;

#define DOCUMENT_WARNING(_op) e##_op,
  enum DocumentWarnings {
#include "nsDocumentWarningList.h"
    eDocumentWarningCount
  };
#undef DOCUMENT_WARNING
  bool HasWarnedAbout(DocumentWarnings aWarning) const;
  void WarnOnceAbout(
      DocumentWarnings aWarning, bool asError = false,
      const nsTArray<nsString>& aParams = nsTArray<nsString>()) const;

  // Posts an event to call UpdateVisibilityState
  void PostVisibilityUpdateEvent();

  bool IsSyntheticDocument() const { return mIsSyntheticDocument; }

  // Adds the size of a given node, which must not be a document node, to the
  // window sizes passed-in.
  static void AddSizeOfNodeTree(nsINode&, nsWindowSizes&);

  // Note: Document is a sub-class of nsINode, which has a
  // SizeOfExcludingThis function.  However, because Document objects can
  // only appear at the top of the DOM tree, we have a specialized measurement
  // function which returns multiple sizes.
  virtual void DocAddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const;
  // DocAddSizeOfIncludingThis doesn't need to be overridden by sub-classes
  // because Document inherits from nsINode;  see the comment above the
  // declaration of nsINode::SizeOfIncludingThis.
  virtual void DocAddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const;

  void ConstructUbiNode(void* storage) override;

  bool MayHaveDOMMutationObservers() { return mMayHaveDOMMutationObservers; }

  void SetMayHaveDOMMutationObservers() { mMayHaveDOMMutationObservers = true; }

  bool MayHaveAnimationObservers() { return mMayHaveAnimationObservers; }

  void SetMayHaveAnimationObservers() { mMayHaveAnimationObservers = true; }

  bool IsInSyncOperation() { return mInSyncOperationCount != 0; }

  void SetIsInSyncOperation(bool aSync) {
    if (aSync) {
      ++mInSyncOperationCount;
    } else {
      --mInSyncOperationCount;
    }
  }

  bool CreatingStaticClone() const { return mCreatingStaticClone; }

  /**
   * Creates a new element in the HTML namespace with a local name given by
   * aTag.
   */
  already_AddRefed<Element> CreateHTMLElement(nsAtom* aTag);

  // WebIDL API
  nsIGlobalObject* GetParentObject() const { return GetScopeObject(); }
  static already_AddRefed<Document> Constructor(const GlobalObject& aGlobal,
                                                ErrorResult& rv);
  DOMImplementation* GetImplementation(ErrorResult& rv);
  MOZ_MUST_USE nsresult GetURL(nsString& retval) const;
  MOZ_MUST_USE nsresult GetDocumentURI(nsString& retval) const;
  // Return the URI for the document.
  // The returned value may differ if the document is loaded via XHR, and
  // when accessed from chrome privileged script and
  // from content privileged script for compatibility.
  void GetDocumentURIFromJS(nsString& aDocumentURI, CallerType aCallerType,
                            ErrorResult& aRv) const;
  void GetCompatMode(nsString& retval) const;
  void GetCharacterSet(nsAString& retval) const;
  // Skip GetContentType, because our NS_IMETHOD version above works fine here.
  // GetDoctype defined above
  Element* GetDocumentElement() const { return GetRootElement(); }

  enum ElementCallbackType {
    eConnected,
    eDisconnected,
    eAdopted,
    eAttributeChanged,
    eGetCustomInterface
  };

  Document* GetTopLevelContentDocument();
  const Document* GetTopLevelContentDocument() const;

  // Returns the associated XUL window if this is a top-level chrome document,
  // null otherwise.
  already_AddRefed<nsIXULWindow> GetXULWindowIfToplevelChrome() const;

  already_AddRefed<Element> CreateElement(
      const nsAString& aTagName, const ElementCreationOptionsOrString& aOptions,
      ErrorResult& rv);
  already_AddRefed<Element> CreateElementNS(
      const nsAString& aNamespaceURI, const nsAString& aQualifiedName,
      const ElementCreationOptionsOrString& aOptions, ErrorResult& rv);
  already_AddRefed<Element> CreateXULElement(
      const nsAString& aTagName, const ElementCreationOptionsOrString& aOptions,
      ErrorResult& aRv);
  already_AddRefed<DocumentFragment> CreateDocumentFragment() const;
  already_AddRefed<nsTextNode> CreateTextNode(const nsAString& aData) const;
  already_AddRefed<nsTextNode> CreateEmptyTextNode() const;
  already_AddRefed<Comment> CreateComment(const nsAString& aData) const;
  already_AddRefed<ProcessingInstruction> CreateProcessingInstruction(
      const nsAString& target, const nsAString& data, ErrorResult& rv) const;
  already_AddRefed<nsINode> ImportNode(nsINode& aNode, bool aDeep,
                                       ErrorResult& rv) const;
  nsINode* AdoptNode(nsINode& aNode, ErrorResult& rv);
  already_AddRefed<Event> CreateEvent(const nsAString& aEventType,
                                      CallerType aCallerType,
                                      ErrorResult& rv) const;
  already_AddRefed<nsRange> CreateRange(ErrorResult& rv);
  already_AddRefed<NodeIterator> CreateNodeIterator(nsINode& aRoot,
                                                    uint32_t aWhatToShow,
                                                    NodeFilter* aFilter,
                                                    ErrorResult& rv) const;
  already_AddRefed<TreeWalker> CreateTreeWalker(nsINode& aRoot,
                                                uint32_t aWhatToShow,
                                                NodeFilter* aFilter,
                                                ErrorResult& rv) const;
  // Deprecated WebIDL bits
  already_AddRefed<CDATASection> CreateCDATASection(const nsAString& aData,
                                                    ErrorResult& rv);
  already_AddRefed<Attr> CreateAttribute(const nsAString& aName,
                                         ErrorResult& rv);
  already_AddRefed<Attr> CreateAttributeNS(const nsAString& aNamespaceURI,
                                           const nsAString& aQualifiedName,
                                           ErrorResult& rv);
  void GetInputEncoding(nsAString& aInputEncoding) const;
  already_AddRefed<Location> GetLocation() const;
  void GetDomain(nsAString& aDomain);
  void SetDomain(const nsAString& aDomain, mozilla::ErrorResult& rv);
  void GetCookie(nsAString& aCookie, mozilla::ErrorResult& rv);
  void SetCookie(const nsAString& aCookie, mozilla::ErrorResult& rv);
  void GetReferrer(nsAString& aReferrer) const;
  void GetLastModified(nsAString& aLastModified) const;
  void GetReadyState(nsAString& aReadyState) const;

  void GetTitle(nsAString& aTitle);
  void SetTitle(const nsAString& aTitle, ErrorResult& rv);
  void GetDir(nsAString& aDirection) const;
  void SetDir(const nsAString& aDirection);
  nsIHTMLCollection* Images();
  nsIHTMLCollection* Embeds();
  nsIHTMLCollection* Plugins() { return Embeds(); }
  nsIHTMLCollection* Links();
  nsIHTMLCollection* Forms();
  nsIHTMLCollection* Scripts();
  already_AddRefed<nsContentList> GetElementsByName(const nsAString& aName) {
    return GetFuncStringContentList<nsCachableElementsByNameNodeList>(
        this, MatchNameAttribute, nullptr, UseExistingNameString, aName);
  }
  Document* Open(const mozilla::dom::Optional<nsAString>& /* unused */,
                 const mozilla::dom::Optional<nsAString>& /* unused */,
                 mozilla::ErrorResult& aError);
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> Open(
      const nsAString& aURL, const nsAString& aName, const nsAString& aFeatures,
      mozilla::ErrorResult& rv);
  void Close(mozilla::ErrorResult& rv);
  void Write(const mozilla::dom::Sequence<nsString>& aText,
             mozilla::ErrorResult& rv);
  void Writeln(const mozilla::dom::Sequence<nsString>& aText,
               mozilla::ErrorResult& rv);
  Nullable<WindowProxyHolder> GetDefaultView() const;
  Element* GetActiveElement();
  bool HasFocus(ErrorResult& rv) const;
  void GetDesignMode(nsAString& aDesignMode);
  void SetDesignMode(const nsAString& aDesignMode,
                     nsIPrincipal& aSubjectPrincipal, mozilla::ErrorResult& rv);
  void SetDesignMode(const nsAString& aDesignMode,
                     const mozilla::Maybe<nsIPrincipal*>& aSubjectPrincipal,
                     mozilla::ErrorResult& rv);
  MOZ_CAN_RUN_SCRIPT
  bool ExecCommand(const nsAString& aHTMLCommandName, bool aShowUI,
                   const nsAString& aValue, nsIPrincipal& aSubjectPrincipal,
                   mozilla::ErrorResult& aRv);
  bool QueryCommandEnabled(const nsAString& aHTMLCommandName,
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& aRv);
  bool QueryCommandIndeterm(const nsAString& aHTMLCommandName,
                            mozilla::ErrorResult& aRv);
  bool QueryCommandState(const nsAString& aHTMLCommandName,
                         mozilla::ErrorResult& aRv);
  bool QueryCommandSupported(const nsAString& aHTMLCommandName,
                             mozilla::dom::CallerType aCallerType,
                             mozilla::ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT
  void QueryCommandValue(const nsAString& aHTMLCommandName, nsAString& aValue,
                         mozilla::ErrorResult& aRv);
  nsIHTMLCollection* Applets();
  nsIHTMLCollection* Anchors();
  TimeStamp LastFocusTime() const;
  void SetLastFocusTime(const TimeStamp& aFocusTime);
  // Event handlers are all on nsINode already
  bool MozSyntheticDocument() const { return IsSyntheticDocument(); }
  Element* GetCurrentScript();
  void ReleaseCapture() const;
  void MozSetImageElement(const nsAString& aImageElementId, Element* aElement);
  nsIURI* GetDocumentURIObject() const;
  // Not const because all the fullscreen goop is not const
  bool FullscreenEnabled(CallerType aCallerType);
  Element* FullscreenStackTop();
  bool Fullscreen() { return !!GetFullscreenElement(); }
  already_AddRefed<Promise> ExitFullscreen(ErrorResult&);
  void ExitPointerLock() { UnlockPointer(this); }
  void GetFgColor(nsAString& aFgColor);
  void SetFgColor(const nsAString& aFgColor);
  void GetLinkColor(nsAString& aLinkColor);
  void SetLinkColor(const nsAString& aLinkColor);
  void GetVlinkColor(nsAString& aAvlinkColor);
  void SetVlinkColor(const nsAString& aVlinkColor);
  void GetAlinkColor(nsAString& aAlinkColor);
  void SetAlinkColor(const nsAString& aAlinkColor);
  void GetBgColor(nsAString& aBgColor);
  void SetBgColor(const nsAString& aBgColor);
  void Clear() const {
    // Deprecated
  }
  void CaptureEvents();
  void ReleaseEvents();

  mozilla::dom::HTMLAllCollection* All();

  static bool IsUnprefixedFullscreenEnabled(JSContext* aCx, JSObject* aObject);
  static bool DocumentSupportsL10n(JSContext* aCx, JSObject* aObject);
  static bool IsWebAnimationsEnabled(JSContext* aCx, JSObject* aObject);
  static bool IsWebAnimationsEnabled(CallerType aCallerType);
  static bool IsWebAnimationsGetAnimationsEnabled(JSContext* aCx,
                                                  JSObject* aObject);
  static bool AreWebAnimationsImplicitKeyframesEnabled(JSContext* aCx,
                                                       JSObject* aObject);
  static bool AreWebAnimationsTimelinesEnabled(JSContext* aCx,
                                               JSObject* aObject);
  // Checks that the caller is either chrome or some addon.
  static bool IsCallerChromeOrAddon(JSContext* aCx, JSObject* aObject);

  bool Hidden() const { return mVisibilityState != VisibilityState::Visible; }
  dom::VisibilityState VisibilityState() const { return mVisibilityState; }

  void GetSelectedStyleSheetSet(nsAString& aSheetSet);
  void SetSelectedStyleSheetSet(const nsAString& aSheetSet);
  void GetLastStyleSheetSet(nsAString& aSheetSet) {
    aSheetSet = mLastStyleSheetSet;
  }
  const nsString& GetCurrentStyleSheetSet() const {
    return mLastStyleSheetSet.IsEmpty() ? mPreferredStyleSheetSet
                                        : mLastStyleSheetSet;
  }
  void SetPreferredStyleSheetSet(const nsAString&);
  void GetPreferredStyleSheetSet(nsAString& aSheetSet) {
    aSheetSet = mPreferredStyleSheetSet;
  }
  DOMStringList* StyleSheetSets();
  void EnableStyleSheetsForSet(const nsAString& aSheetSet);

  /**
   * Retrieve the location of the caret position (DOM node and character
   * offset within that node), given a point.
   *
   * @param aX Horizontal point at which to determine the caret position, in
   *           page coordinates.
   * @param aY Vertical point at which to determine the caret position, in
   *           page coordinates.
   */
  already_AddRefed<nsDOMCaretPosition> CaretPositionFromPoint(float aX,
                                                              float aY);

  Element* GetScrollingElement();
  // A way to check whether a given element is what would get returned from
  // GetScrollingElement.  It can be faster than comparing to the return value
  // of GetScrollingElement() due to being able to avoid flushes in various
  // cases.  This method assumes that null is NOT passed.
  bool IsScrollingElement(Element* aElement);

  // QuerySelector and QuerySelectorAll already defined on nsINode
  nsINodeList* GetAnonymousNodes(Element& aElement);
  Element* GetAnonymousElementByAttribute(Element& aElement,
                                          const nsAString& aAttrName,
                                          const nsAString& aAttrValue);
  Element* GetBindingParent(nsINode& aNode);
  void LoadBindingDocument(const nsAString& aURI,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& rv);
  XPathExpression* CreateExpression(const nsAString& aExpression,
                                    XPathNSResolver* aResolver,
                                    ErrorResult& rv);
  nsINode* CreateNSResolver(nsINode& aNodeResolver);
  already_AddRefed<XPathResult> Evaluate(
      JSContext* aCx, const nsAString& aExpression, nsINode& aContextNode,
      XPathNSResolver* aResolver, uint16_t aType, JS::Handle<JSObject*> aResult,
      ErrorResult& rv);
  // Touch event handlers already on nsINode
  already_AddRefed<Touch> CreateTouch(nsGlobalWindowInner* aView,
                                      EventTarget* aTarget, int32_t aIdentifier,
                                      int32_t aPageX, int32_t aPageY,
                                      int32_t aScreenX, int32_t aScreenY,
                                      int32_t aClientX, int32_t aClientY,
                                      int32_t aRadiusX, int32_t aRadiusY,
                                      float aRotationAngle, float aForce);
  already_AddRefed<TouchList> CreateTouchList();
  already_AddRefed<TouchList> CreateTouchList(
      Touch& aTouch, const Sequence<OwningNonNull<Touch>>& aTouches);
  already_AddRefed<TouchList> CreateTouchList(
      const Sequence<OwningNonNull<Touch>>& aTouches);

  void SetStyleSheetChangeEventsEnabled(bool aValue) {
    mStyleSheetChangeEventsEnabled = aValue;
  }

  bool StyleSheetChangeEventsEnabled() const {
    return mStyleSheetChangeEventsEnabled;
  }

  already_AddRefed<Promise> BlockParsing(Promise& aPromise,
                                         const BlockParsingOptions& aOptions,
                                         ErrorResult& aRv);

  already_AddRefed<nsIURI> GetMozDocumentURIIfNotForErrorPages();

  Promise* GetDocumentReadyForIdle(ErrorResult& aRv);

  nsIDOMXULCommandDispatcher* GetCommandDispatcher();
  bool HasXULBroadcastManager() const { return mXULBroadcastManager; };
  void InitializeXULBroadcastManager();
  XULBroadcastManager* GetXULBroadcastManager() const {
    return mXULBroadcastManager;
  }
  already_AddRefed<nsINode> GetPopupNode();
  void SetPopupNode(nsINode* aNode);
  nsINode* GetPopupRangeParent(ErrorResult& aRv);
  int32_t GetPopupRangeOffset(ErrorResult& aRv);
  already_AddRefed<nsINode> GetTooltipNode();
  void SetTooltipNode(nsINode* aNode) { /* do nothing */
  }

  bool DontWarnAboutMutationEventsAndAllowSlowDOMMutations() {
    return mDontWarnAboutMutationEventsAndAllowSlowDOMMutations;
  }
  void SetDontWarnAboutMutationEventsAndAllowSlowDOMMutations(
      bool aDontWarnAboutMutationEventsAndAllowSlowDOMMutations) {
    mDontWarnAboutMutationEventsAndAllowSlowDOMMutations =
        aDontWarnAboutMutationEventsAndAllowSlowDOMMutations;
  }

  // ParentNode
  nsIHTMLCollection* Children();
  uint32_t ChildElementCount();

  /**
   * Asserts IsHTMLOrXHTML, and can't return null.
   * Defined inline in nsHTMLDocument.h
   */
  inline nsHTMLDocument* AsHTMLDocument();

  /**
   * Asserts IsSVGDocument, and can't return null.
   * Defined inline in SVGDocument.h
   */
  inline SVGDocument* AsSVGDocument();

  /*
   * Given a node, get a weak reference to it and append that reference to
   * mBlockedNodesByClassifier. Can be used later on to look up a node in it.
   * (e.g., by the UI)
   */
  void AddBlockedNodeByClassifier(nsINode* node) {
    if (!node) {
      return;
    }

    nsWeakPtr weakNode = do_GetWeakReference(node);

    if (weakNode) {
      mBlockedNodesByClassifier.AppendElement(weakNode);
    }
  }

  gfxUserFontSet* GetUserFontSet();
  void FlushUserFontSet();
  void MarkUserFontSetDirty();
  FontFaceSet* GetFonts() { return mFontFaceSet; }

  // FontFaceSource
  FontFaceSet* Fonts();

  bool DidFireDOMContentLoaded() const { return mDidFireDOMContentLoaded; }

  bool IsSynthesized();

  void ReportUseCounters();

  void SetUseCounter(UseCounter aUseCounter) {
    mUseCounters[aUseCounter] = true;
  }

  const StyleUseCounters* GetStyleUseCounters() {
    return mStyleUseCounters.get();
  }

  void PropagateUseCountersToPage();
  void PropagateUseCounters(Document* aParentDocument);

  void AddToVisibleContentHeuristic(uint32_t aNumber) {
    mVisibleContentHeuristic += aNumber;
  }

  uint32_t GetVisibleContentHeuristic() const {
    return mVisibleContentHeuristic.value();
  }

  // Called to track whether this document has had any interaction.
  // This is used to track whether we should permit "beforeunload".
  void SetUserHasInteracted();
  bool UserHasInteracted() { return mUserHasInteracted; }
  void ResetUserInteractionTimer();

  // This method would return current autoplay policy, it would be "allowed"
  // , "allowed-muted" or "disallowed".
  DocumentAutoplayPolicy AutoplayPolicy() const;

  // This should be called when this document receives events which are likely
  // to be user interaction with the document, rather than the byproduct of
  // interaction with the browser (i.e. a keypress to scroll the view port,
  // keyboard shortcuts, etc). This is used to decide whether we should
  // permit autoplay audible media. This also gesture activates all other
  // content documents in this tab.
  void NotifyUserGestureActivation();

  // This function is used for mochitest only.
  void ClearUserGestureActivation();

  // Return true if NotifyUserGestureActivation() has been called on any
  // document in the document tree.
  bool HasBeenUserGestureActivated();

  // Return true if there is transient user gesture activation and it hasn't yet
  // timed out.
  bool HasValidTransientUserGestureActivation();

  BrowsingContext* GetBrowsingContext() const;

  // This document is a WebExtension page, it might be a background page, a
  // popup, a visible tab, a visible iframe ...e.t.c.
  bool IsExtensionPage() const;

  bool HasScriptsBlockedBySandbox();

  bool InlineScriptAllowedByCSP();

  void ReportHasScrollLinkedEffect();
  bool HasScrollLinkedEffect() const { return mHasScrollLinkedEffect; }

#ifdef DEBUG
  void AssertDocGroupMatchesKey() const;
#endif

  DocGroup* GetDocGroup() const {
#ifdef DEBUG
    AssertDocGroupMatchesKey();
#endif
    return mDocGroup;
  }

  /**
   * If we're a sub-document, the parent document's layout can affect our style
   * and layout (due to the viewport size, viewport units, media queries...).
   *
   * This function returns true if our parent document and our child document
   * can observe each other. If they cannot, then we don't need to synchronously
   * update the parent document layout every time the child document may need
   * up-to-date layout information.
   */
  bool StyleOrLayoutObservablyDependsOnParentDocumentLayout() const {
    return GetInProcessParentDocument() &&
           GetDocGroup() == GetInProcessParentDocument()->GetDocGroup();
  }

  void AddIntersectionObserver(DOMIntersectionObserver* aObserver) {
    MOZ_ASSERT(!mIntersectionObservers.Contains(aObserver),
               "Intersection observer already in the list");
    mIntersectionObservers.PutEntry(aObserver);
  }

  void RemoveIntersectionObserver(DOMIntersectionObserver* aObserver) {
    mIntersectionObservers.RemoveEntry(aObserver);
  }

  bool HasIntersectionObservers() const {
    return !mIntersectionObservers.IsEmpty();
  }

  void UpdateIntersectionObservations();
  void ScheduleIntersectionObserverNotification();
  MOZ_CAN_RUN_SCRIPT void NotifyIntersectionObservers();

  // Dispatch a runnable related to the document.
  nsresult Dispatch(TaskCategory aCategory,
                    already_AddRefed<nsIRunnable>&& aRunnable) final;

  virtual nsISerialEventTarget* EventTargetFor(
      TaskCategory aCategory) const override;

  virtual AbstractThread* AbstractMainThreadFor(
      TaskCategory aCategory) override;

  // The URLs passed to this function should match what
  // JS::DescribeScriptedCaller() returns, since this API is used to
  // determine whether some code is being called from a tracking script.
  void NoteScriptTrackingStatus(const nsACString& aURL, bool isTracking);
  // The JSContext passed to this method represents the context that we want to
  // determine if it belongs to a tracker.
  bool IsScriptTracking(JSContext* aCx) const;

  // For more information on Flash classification, see
  // toolkit/components/url-classifier/flash-block-lists.rst
  FlashClassification DocumentFlashClassification();

  // ResizeObserver usage.
  void AddResizeObserver(ResizeObserver* aResizeObserver);
  void ScheduleResizeObserversNotification() const;

  /**
   * Localization
   *
   * For more information on DocumentL10n see
   * intl/l10n/docs/fluent_tutorial.rst
   */

 public:
  /**
   * This is a public method exposed on Document WebIDL
   * to chrome only documents.
   */
  DocumentL10n* GetL10n();

  /**
   * This method should be called when the container
   * of l10n resources parsing is completed.
   *
   * It triggers initial async fetch of the resources
   * as early as possible.
   *
   * In HTML case this is </head>.
   * In XUL case this is </linkset>.
   */
  void OnL10nResourceContainerParsed();

  /**
   * This method should be called when a link element
   * with rel="localization" is being added to the
   * l10n resource container element.
   */
  void LocalizationLinkAdded(Element* aLinkElement);

  /**
   * This method should be called when a link element
   * with rel="localization" is being removed.
   */
  void LocalizationLinkRemoved(Element* aLinkElement);

  /**
   * This method should be called as soon as the
   * parsing of the document is completed.
   *
   * In HTML/XHTML this happens when we finish parsing
   * the document element.
   * In XUL it happens at `DoneWalking`, during
   * `MozBeforeInitialXULLayout`.
   *
   * It triggers the initial translation of the
   * document.
   */
  void TriggerInitialDocumentTranslation();

  /**
   * This method is called when the initial translation
   * of the document is completed.
   *
   * It unblocks the load event if translation was blocking it.
   */
  void InitialDocumentTranslationCompleted();

 protected:
  RefPtr<DocumentL10n> mDocumentL10n;

  /**
   * Return true when you want a document without explicitly specified viewport
   * dimensions/scale to be treated as if "width=device-width" had in fact been
   * specified.
   */
  virtual bool UseWidthDeviceWidthFallbackViewport() const;

 private:
  void InitializeLocalization(nsTArray<nsString>& aResourceIds);

  // Takes the bits from mStyleUseCounters if appropriate, and sets them in
  // mUseCounters.
  void SetCssUseCounterBits();

  // Returns true if there is any valid value in the viewport meta tag.
  bool ParseWidthAndHeightInMetaViewport(const nsAString& aWidthString,
                                         const nsAString& aHeightString,
                                         bool aIsAutoScale);

  // Parse scale values in viewport meta tag for a given |aHeaderField| which
  // represents the scale property and returns the scale value if it's valid.
  Maybe<LayoutDeviceToScreenScale> ParseScaleInHeader(nsAtom* aHeaderField);

  // Parse scale values in |aViewportMetaData| and set the values in
  // mScaleMinFloat, mScaleMaxFloat and mScaleFloat respectively.
  // Returns true if there is any valid scale value in the |aViewportMetaData|.
  bool ParseScalesInViewportMetaData(const ViewportMetaData& aViewportMetaData);

  // Returns a ViewportMetaData for this document.
  ViewportMetaData GetViewportMetaData() const;

  FlashClassification DocumentFlashClassificationInternal();

  nsTArray<nsString> mL10nResources;

  // The application cache that this document is associated with, if
  // any.  This can change during the lifetime of the document.
  nsCOMPtr<nsIApplicationCache> mApplicationCache;

 public:
  bool IsThirdPartyForFlashClassifier();

 private:
  void DoCacheAllKnownLangPrefs();
  void RecomputeLanguageFromCharset();

 public:
  void SetMayNeedFontPrefsUpdate() { mMayNeedFontPrefsUpdate = true; }

  bool MayNeedFontPrefsUpdate() { return mMayNeedFontPrefsUpdate; }

  already_AddRefed<nsAtom> GetContentLanguageAsAtomForStyle() const;
  already_AddRefed<nsAtom> GetLanguageForStyle() const;

  /**
   * Fetch the user's font preferences for the given aLanguage's
   * language group.
   */
  const LangGroupFontPrefs* GetFontPrefsForLang(
      nsAtom* aLanguage, bool* aNeedsToCache = nullptr) const;

  void ForceCacheLang(nsAtom* aLanguage) {
    if (!mLanguagesUsed.EnsureInserted(aLanguage)) {
      return;
    }
    GetFontPrefsForLang(aLanguage);
  }

  void CacheAllKnownLangPrefs() {
    if (!mMayNeedFontPrefsUpdate) {
      return;
    }
    DoCacheAllKnownLangPrefs();
  }

  nsINode* GetServoRestyleRoot() const { return mServoRestyleRoot; }

  uint32_t GetServoRestyleRootDirtyBits() const {
    MOZ_ASSERT(mServoRestyleRoot);
    MOZ_ASSERT(mServoRestyleRootDirtyBits);
    return mServoRestyleRootDirtyBits;
  }

  void ClearServoRestyleRoot() {
    mServoRestyleRoot = nullptr;
    mServoRestyleRootDirtyBits = 0;
  }

  inline void SetServoRestyleRoot(nsINode* aRoot, uint32_t aDirtyBits);
  inline void SetServoRestyleRootDirtyBits(uint32_t aDirtyBits);

  bool ShouldThrowOnDynamicMarkupInsertion() {
    return mThrowOnDynamicMarkupInsertionCounter;
  }

  void IncrementThrowOnDynamicMarkupInsertionCounter() {
    ++mThrowOnDynamicMarkupInsertionCounter;
  }

  void DecrementThrowOnDynamicMarkupInsertionCounter() {
    MOZ_ASSERT(mThrowOnDynamicMarkupInsertionCounter);
    --mThrowOnDynamicMarkupInsertionCounter;
  }

  bool ShouldIgnoreOpens() const { return mIgnoreOpensDuringUnloadCounter; }

  void IncrementIgnoreOpensDuringUnloadCounter() {
    ++mIgnoreOpensDuringUnloadCounter;
  }

  void DecrementIgnoreOpensDuringUnloadCounter() {
    MOZ_ASSERT(mIgnoreOpensDuringUnloadCounter);
    --mIgnoreOpensDuringUnloadCounter;
  }

  bool AllowPaymentRequest() const { return mAllowPaymentRequest; }

  void SetAllowPaymentRequest(bool aAllowPaymentRequest) {
    mAllowPaymentRequest = aAllowPaymentRequest;
  }

  mozilla::dom::FeaturePolicy* FeaturePolicy() const;

  bool ModuleScriptsEnabled();

  /**
   * Find the (non-anonymous) content in this document for aFrame. It will
   * be aFrame's content node if that content is in this document and not
   * anonymous. Otherwise, when aFrame is in a subdocument, we use the frame
   * element containing the subdocument containing aFrame, and/or find the
   * nearest non-anonymous ancestor in this document.
   * Returns null if there is no such element.
   */
  nsIContent* GetContentInThisDocument(nsIFrame* aFrame) const;

  void ReportShadowDOMUsage();

  // Sets flags for media autoplay telemetry.
  void SetDocTreeHadAudibleMedia();
  void SetDocTreeHadPlayRevoked();

  dom::XPathEvaluator* XPathEvaluator();

  void MaybeInitializeFinalizeFrameLoaders();

  void SetDelayFrameLoaderInitialization(bool aDelayFrameLoaderInitialization) {
    mDelayFrameLoaderInitialization = aDelayFrameLoaderInitialization;
  }

  void SetPrototypeDocument(nsXULPrototypeDocument* aPrototype);

  bool InRDMPane() const { return mInRDMPane; }
  void SetInRDMPane(bool aInRDMPane) { mInRDMPane = aInRDMPane; }

  // Returns true if we use overlay scrollbars on the system wide or on the
  // given document.
  static bool UseOverlayScrollbars(const Document* aDocument);

  static bool HasRecentlyStartedForegroundLoads();

  static bool AutomaticStorageAccessCanBeGranted(nsIPrincipal* aPrincipal);

 protected:
  void DoUpdateSVGUseElementShadowTrees();

  already_AddRefed<nsIPrincipal> MaybeDowngradePrincipal(
      nsIPrincipal* aPrincipal);

  void EnsureOnloadBlocker();

  void SendToConsole(nsCOMArray<nsISecurityConsoleMessage>& aMessages);

  // Returns true if the scheme for the url for this document is "about".
  bool IsAboutPage() const;

  bool ContainsEMEContent();
  bool ContainsMSEContent();

  /**
   * Returns the title element of the document as defined by the HTML
   * specification, or null if there isn't one.  For documents whose root
   * element is an <svg:svg>, this is the first <svg:title> element that's a
   * child of the root.  For other documents, it's the first HTML title element
   * in the document.
   */
  Element* GetTitleElement();

  void RecordNavigationTiming(ReadyState aReadyState);

  // This method may fire a DOM event; if it does so it will happen
  // synchronously.
  void UpdateVisibilityState();

  // Recomputes the visibility state but doesn't set the new value.
  dom::VisibilityState ComputeVisibilityState() const;

  // Since we wouldn't automatically play media from non-visited page, we need
  // to notify window when the page was first visited.
  void MaybeActiveMediaComponents();

  // Apply the fullscreen state to the document, and trigger related
  // events. It returns false if the fullscreen element ready check
  // fails and nothing gets changed.
  bool ApplyFullscreen(UniquePtr<FullscreenRequest>);

  bool GetUseCounter(UseCounter aUseCounter) {
    return mUseCounters[aUseCounter];
  }

  void SetChildDocumentUseCounter(UseCounter aUseCounter) {
    if (!mChildDocumentUseCounters[aUseCounter]) {
      mChildDocumentUseCounters[aUseCounter] = true;
    }
  }

  bool GetChildDocumentUseCounter(UseCounter aUseCounter) {
    return mChildDocumentUseCounters[aUseCounter];
  }

  void RemoveDocStyleSheetsFromStyleSets();
  void RemoveStyleSheetsFromStyleSets(
      const nsTArray<RefPtr<StyleSheet>>& aSheets, StyleOrigin);
  void ResetStylesheetsToURI(nsIURI* aURI);
  void FillStyleSet();
  void FillStyleSetUserAndUASheets();
  void FillStyleSetDocumentSheets();
  void CompatibilityModeChanged();
  bool NeedsQuirksSheet() const {
    // SVG documents never load quirk.css.
    // FIXME(emilio): Can SVG documents be in quirks mode anyway?
    return mCompatMode == eCompatibility_NavQuirks && !IsSVGDocument();
  }
  void AddContentEditableStyleSheetsToStyleSet(bool aDesignMode);
  void RemoveContentEditableStyleSheets();
  void AddStyleSheetToStyleSets(StyleSheet* aSheet);
  void RemoveStyleSheetFromStyleSets(StyleSheet* aSheet);
  void NotifyStyleSheetAdded(StyleSheet* aSheet, bool aDocumentSheet);
  void NotifyStyleSheetRemoved(StyleSheet* aSheet, bool aDocumentSheet);
  void NotifyStyleSheetApplicableStateChanged();
  // Just like EnableStyleSheetsForSet, but doesn't check whether
  // aSheetSet is null and allows the caller to control whether to set
  // aSheetSet as the preferred set in the CSSLoader.
  void EnableStyleSheetsForSetInternal(const nsAString& aSheetSet,
                                       bool aUpdateCSSLoader);

  already_AddRefed<nsIURI> GetDomainURI();
  already_AddRefed<nsIURI> CreateInheritingURIForHost(
      const nsACString& aHostString);
  already_AddRefed<nsIURI> RegistrableDomainSuffixOfInternal(
      const nsAString& aHostSuffixString, nsIURI* aOrigHost);

  void WriteCommon(const nsAString& aText, bool aNewlineTerminate,
                   mozilla::ErrorResult& aRv);
  // A version of WriteCommon used by WebIDL bindings
  void WriteCommon(const mozilla::dom::Sequence<nsString>& aText,
                   bool aNewlineTerminate, mozilla::ErrorResult& rv);

  void* GenerateParserKey(void);

 private:
  // ExecCommandParam indicates how HTMLDocument.execCommand() treats given the
  // parameter.
  enum class ExecCommandParam : uint8_t {
    // Always ignore it.
    Ignore,
    // Treat the given parameter as-is.  If the command requires it, use it.
    // Otherwise, ignore it.
    String,
    // Always treat it as boolean parameter.
    Boolean,
    // Always treat it as boolean, but inverted.
    InvertedBoolean,
  };

  typedef mozilla::EditorCommand*(GetEditorCommandFunc)();

  struct InternalCommandData {
    const char* mXULCommandName;
    mozilla::Command mCommand;  // uint8_t
    // How ConvertToInternalCommand() to treats aValue.
    // Its callers don't need to check this.
    ExecCommandParam mExecCommandParam;  // uint8_t
    GetEditorCommandFunc* mGetEditorCommandFunc;

    InternalCommandData()
        : mXULCommandName(nullptr),
          mCommand(mozilla::Command::DoNothing),
          mExecCommandParam(ExecCommandParam::Ignore),
          mGetEditorCommandFunc(nullptr) {}
    InternalCommandData(const char* aXULCommandName, mozilla::Command aCommand,
                        ExecCommandParam aExecCommandParam,
                        GetEditorCommandFunc aGetEditorCommandFunc)
        : mXULCommandName(aXULCommandName),
          mCommand(aCommand),
          mExecCommandParam(aExecCommandParam),
          mGetEditorCommandFunc(aGetEditorCommandFunc) {}

    bool IsAvailableOnlyWhenEditable() const {
      return mCommand != mozilla::Command::Cut &&
             mCommand != mozilla::Command::Copy &&
             mCommand != mozilla::Command::Paste;
    }
    bool IsCutOrCopyCommand() const {
      return mCommand == mozilla::Command::Cut ||
             mCommand == mozilla::Command::Copy;
    }
    bool IsPasteCommand() const { return mCommand == mozilla::Command::Paste; }
  };

  /**
   * Helper method to initialize sInternalCommandDataHashtable.
   */
  static void EnsureInitializeInternalCommandDataHashtable();

  /**
   * ConvertToInternalCommand() returns a copy of InternalCommandData instance.
   * Note that if aAdjustedValue is non-nullptr, this method checks whether
   * aValue is proper value or not unless InternalCommandData::mExecCommandParam
   * is ExecCommandParam::Ignore.  For example, if aHTMLCommandName is
   * "defaultParagraphSeparator", the value has to be one of "div", "p" or
   * "br".  If aValue is invalid value for InternalCommandData::mCommand, this
   * returns a copy of instance created with default constructor.  I.e., its
   * mCommand is set to Command::DoNothing.  So, this treats aHTMLCommandName
   * is unsupported in such case.
   *
   * @param aHTMLCommandName    Command name in HTML, e.g., used by
   *                            execCommand().
   * @param aValue              The value which is set to the 3rd parameter
   *                            of execCommand().
   * @param aAdjustedValue      [out] Must be empty string if set non-nullptr.
   *                            Will be set to adjusted value for executing
   *                            the internal command.
   * @return                    Returns a copy of instance created with the
   *                            default constructor if there is no
   *                            corresponding internal command for
   *                            aHTMLCommandName or aValue is invalid for
   *                            found internal command when aAdjustedValue
   *                            is not nullptr.  Otherwise, returns a copy of
   *                            instance registered in
   *                            sInternalCommandDataHashtable.
   */
  static InternalCommandData ConvertToInternalCommand(
      const nsAString& aHTMLCommandName,
      const nsAString& aValue = EmptyString(),
      nsAString* aAdjustedValue = nullptr);

  // Mapping table from HTML command name to internal command.
  typedef nsDataHashtable<nsStringCaseInsensitiveHashKey, InternalCommandData>
      InternalCommandDataHashtable;
  static InternalCommandDataHashtable* sInternalCommandDataHashtable;

  void RecordContentBlockingLog(
      const nsACString& aOrigin, uint32_t aType, bool aBlocked,
      const Maybe<AntiTrackingCommon::StorageAccessGrantedReason>& aReason =
          Nothing(),
      const nsTArray<nsCString>& aTrackingFullHashes = nsTArray<nsCString>()) {
    mContentBlockingLog.RecordLog(aOrigin, aType, aBlocked, aReason,
                                  aTrackingFullHashes);
  }

  mutable std::bitset<eDeprecatedOperationCount> mDeprecationWarnedAbout;
  mutable std::bitset<eDocumentWarningCount> mDocWarningWarnedAbout;

  // Lazy-initialization to have mDocGroup initialized in prior to the
  // SelectorCaches.
  UniquePtr<SelectorCache> mSelectorCache;
  UniquePtr<ServoStyleSet> mStyleSet;

 protected:
  friend class nsDocumentOnStack;

  void IncreaseStackRefCnt() { ++mStackRefCnt; }

  void DecreaseStackRefCnt() {
    if (--mStackRefCnt == 0 && mNeedsReleaseAfterStackRefCntRelease) {
      mNeedsReleaseAfterStackRefCntRelease = false;
      NS_RELEASE_THIS();
    }
  }

  // Never ever call this. Only call GetWindow!
  nsPIDOMWindowOuter* GetWindowInternal() const;

  // Never ever call this. Only call GetScriptHandlingObject!
  nsIScriptGlobalObject* GetScriptHandlingObjectInternal() const;

  // Never ever call this. Only call AllowXULXBL!
  bool InternalAllowXULXBL();

  /**
   * These methods should be called before and after dispatching
   * a mutation event.
   * To make this easy and painless, use the mozAutoSubtreeModified helper
   * class.
   */
  void WillDispatchMutationEvent(nsINode* aTarget);
  void MutationEventDispatched(nsINode* aTarget);
  friend class mozAutoSubtreeModified;

  virtual Element* GetNameSpaceElement() override { return GetRootElement(); }

  void SetContentTypeInternal(const nsACString& aType);

  nsCString GetContentTypeInternal() const { return mContentType; }

  // Update our frame request callback scheduling state, if needed.  This will
  // schedule or unschedule them, if necessary, and update
  // mFrameRequestCallbacksScheduled.  aOldShell should only be passed when
  // mPresShell is becoming null; in that case it will be used to get hold of
  // the relevant refresh driver.
  void UpdateFrameRequestCallbackSchedulingState(
      PresShell* aOldPresShell = nullptr);

  // Helper for GetScrollingElement/IsScrollingElement.
  bool IsPotentiallyScrollable(HTMLBodyElement* aBody);

  // Return the same type parent docuement if exists, or return null.
  Document* GetSameTypeParentDocument();

  void MaybeAllowStorageForOpenerAfterUserInteraction();

  void MaybeStoreUserInteractionAsPermission();

  // Helpers for GetElementsByName.
  static bool MatchNameAttribute(Element* aElement, int32_t aNamespaceID,
                                 nsAtom* aAtom, void* aData);
  static void* UseExistingNameString(nsINode* aRootNode, const nsString* aName);

  void MaybeResolveReadyForIdle();

  typedef MozPromise<bool, bool, true> AutomaticStorageAccessGrantPromise;
  MOZ_MUST_USE RefPtr<AutomaticStorageAccessGrantPromise>
  AutomaticStorageAccessCanBeGranted();

  // This should *ONLY* be used in GetCookie/SetCookie.
  already_AddRefed<nsIChannel> CreateDummyChannelForCookies(
      nsIURI* aContentURI);

  void AddToplevelLoadingDocument(Document* aDoc);
  void RemoveToplevelLoadingDocument(Document* aDoc);
  static AutoTArray<Document*, 8>* sLoadingForegroundTopLevelContentDocument;

  nsCOMPtr<nsIReferrerInfo> mPreloadReferrerInfo;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;

  nsString mLastModified;

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mChromeXHRDocURI;
  nsCOMPtr<nsIURI> mDocumentBaseURI;
  nsCOMPtr<nsIURI> mChromeXHRDocBaseURI;

  // The base domain of the document for third-party checks.
  nsCString mBaseDomain;

  // A lazily-constructed URL data for style system to resolve URL value.
  RefPtr<URLExtraData> mCachedURLData;
  nsCOMPtr<nsIReferrerInfo> mCachedReferrerInfo;

  nsWeakPtr mDocumentLoadGroup;

  bool mBlockAllMixedContent;
  bool mBlockAllMixedContentPreloads;
  bool mUpgradeInsecureRequests;
  bool mUpgradeInsecurePreloads;

  bool mDontWarnAboutMutationEventsAndAllowSlowDOMMutations;

  WeakPtr<nsDocShell> mDocumentContainer;

  NotNull<const Encoding*> mCharacterSet;
  int32_t mCharacterSetSource;

  // This is just a weak pointer; the parent document owns its children.
  Document* mParentDocument;

  // A reference to the element last returned from GetRootElement().
  Element* mCachedRootElement;

  // This is a weak reference, but we hold a strong reference to mNodeInfo,
  // which in turn holds a strong reference to this mNodeInfoManager.
  nsNodeInfoManager* mNodeInfoManager;
  RefPtr<css::Loader> mCSSLoader;
  RefPtr<css::ImageLoader> mStyleImageLoader;
  RefPtr<nsHTMLStyleSheet> mAttrStyleSheet;
  RefPtr<nsHTMLCSSStyleSheet> mStyleAttrStyleSheet;

  // Tracking for images in the document.
  RefPtr<dom::ImageTracker> mImageTracker;

  // A hashtable of ShadowRoots belonging to the composed doc.
  //
  // See ShadowRoot::Bind and ShadowRoot::Unbind.
  ShadowRootSet mComposedShadowRoots;

  using SVGUseElementSet = nsTHashtable<nsPtrHashKey<SVGUseElement>>;

  // The set of <svg:use> elements that need a shadow tree reclone because the
  // tree they map to has changed.
  SVGUseElementSet mSVGUseElementsNeedingShadowTreeUpdate;

  // The set of all object, embed, video/audio elements or
  // nsIObjectLoadingContent or DocumentActivity for which this is
  // the owner document. (They might not be in the document.)
  //
  // These are non-owning pointers, the elements are responsible for removing
  // themselves when they go away.
  nsAutoPtr<nsTHashtable<nsPtrHashKey<nsISupports>>> mActivityObservers;

  // A hashtable of styled links keyed by address pointer.
  nsTHashtable<nsPtrHashKey<Link>> mStyledLinks;
#ifdef DEBUG
  // Indicates whether mStyledLinks was cleared or not.  This is used to track
  // state so we can provide useful assertions to consumers of ForgetLink and
  // AddStyleRelevantLink.
  bool mStyledLinksCleared;
#endif

  // The array of all links that need their status resolved.  Links must add
  // themselves to this set by calling RegisterPendingLinkUpdate when added to a
  // document.
  static const size_t kSegmentSize = 128;

  typedef SegmentedVector<nsCOMPtr<Link>, kSegmentSize, InfallibleAllocPolicy>
      LinksToUpdateList;

  LinksToUpdateList mLinksToUpdate;

  // SMIL Animation Controller, lazily-initialized in GetAnimationController
  RefPtr<SMILAnimationController> mAnimationController;

  // Table of element properties for this document.
  nsPropertyTable mPropertyTable;

  // Our cached .children collection
  nsCOMPtr<nsIHTMLCollection> mChildrenCollection;

  // Various DOM lists
  RefPtr<nsContentList> mImages;
  RefPtr<nsContentList> mEmbeds;
  RefPtr<nsContentList> mLinks;
  RefPtr<nsContentList> mForms;
  RefPtr<nsContentList> mScripts;
  nsCOMPtr<nsIHTMLCollection> mApplets;
  RefPtr<nsContentList> mAnchors;

  // container for per-context fonts (downloadable, SVG, etc.)
  RefPtr<FontFaceSet> mFontFaceSet;

  // Last time this document or a one of its sub-documents was focused.  If
  // focus has never occurred then mLastFocusTime.IsNull() will be true.
  TimeStamp mLastFocusTime;

  EventStates mDocumentState;

  RefPtr<Promise> mReadyForIdle;

  RefPtr<mozilla::dom::FeaturePolicy> mFeaturePolicy;

  UniquePtr<ResizeObserverController> mResizeObserverController;

  // True if BIDI is enabled.
  bool mBidiEnabled : 1;
  // True if we may need to recompute the language prefs for this document.
  bool mMayNeedFontPrefsUpdate : 1;
  // True if a MathML element has ever been owned by this document.
  bool mMathMLEnabled : 1;

  // True if this document is the initial document for a window.  This should
  // basically be true only for documents that exist in newly-opened windows or
  // documents created to satisfy a GetDocument() on a window when there's no
  // document in it.
  bool mIsInitialDocumentInWindow : 1;

  bool mIgnoreDocGroupMismatches : 1;

  // True if we're loaded as data and therefor has any dangerous stuff, such
  // as scripts and plugins, disabled.
  bool mLoadedAsData : 1;

  // This flag is only set in XMLDocument, for e.g. documents used in XBL. We
  // don't want animations to play in such documents, so we need to store the
  // flag here so that we can check it in Document::GetAnimationController.
  bool mLoadedAsInteractiveData : 1;

  // If true, whoever is creating the document has gotten it to the
  // point where it's safe to start layout on it.
  bool mMayStartLayout : 1;

  // True iff we've ever fired a DOMTitleChanged event for this document
  bool mHaveFiredTitleChange : 1;

  // State for IsShowing(). mIsShowing starts off false. It becomes true when
  // OnPageShow happens and becomes false when OnPageHide happens. So it's false
  // before the initial load completes and when we're in bfcache or unloaded,
  // true otherwise.
  bool mIsShowing : 1;

  // State for IsVisible(). mVisible starts off true. It becomes false when
  // OnPageHide happens, and becomes true again when OnPageShow happens.  So
  // it's false only when we're in bfcache or unloaded.
  bool mVisible : 1;

  // True if our content viewer has been removed from the docshell
  // (it may still be displayed, but in zombie state). Form control data
  // has been saved.
  bool mRemovedFromDocShell : 1;

  // True iff DNS prefetch is allowed for this document.  Note that if the
  // document has no window, DNS prefetch won't be performed no matter what.
  bool mAllowDNSPrefetch : 1;

  // True when this document is a static clone of a normal document
  bool mIsStaticDocument : 1;

  // True while this document is being cloned to a static document.
  bool mCreatingStaticClone : 1;

  // True iff the document is being unlinked or deleted.
  bool mInUnlinkOrDeletion : 1;

  // True if document has ever had script handling object.
  bool mHasHadScriptHandlingObject : 1;

  // True if we're an SVG document being used as an image.
  bool mIsBeingUsedAsImage : 1;

  // True if our current document URI's scheme is chrome://
  bool mDocURISchemeIsChrome : 1;

  // True if we're loaded in a chrome docshell.
  bool mInChromeDocShell : 1;

  // True if our current document is a DevTools document. Either the url is
  // about:devtools-toolbox or the parent document already has
  // mIsDevToolsDocument set to true.
  // This is used to avoid applying High Contrast mode to DevTools documents.
  // See Bug 1575766.
  bool mIsDevToolsDocument : 1;

  // True is this document is synthetic : stand alone image, video, audio
  // file, etc.
  bool mIsSyntheticDocument : 1;

  // True is there is a pending runnable which will call
  // FlushPendingLinkUpdates().
  bool mHasLinksToUpdateRunnable : 1;

  // True if we're flushing pending link updates.
  bool mFlushingPendingLinkUpdates : 1;

  // True if a DOMMutationObserver is perhaps attached to a node in the
  // document.
  bool mMayHaveDOMMutationObservers : 1;

  // True if an nsIAnimationObserver is perhaps attached to a node in the
  // document.
  bool mMayHaveAnimationObservers : 1;

  // True if a document has loaded Mixed Active Script (see
  // nsMixedContentBlocker.cpp)
  bool mHasMixedActiveContentLoaded : 1;

  // True if a document has blocked Mixed Active Script (see
  // nsMixedContentBlocker.cpp)
  bool mHasMixedActiveContentBlocked : 1;

  // True if a document has loaded Mixed Display/Passive Content (see
  // nsMixedContentBlocker.cpp)
  bool mHasMixedDisplayContentLoaded : 1;

  // True if a document has blocked Mixed Display/Passive Content (see
  // nsMixedContentBlocker.cpp)
  bool mHasMixedDisplayContentBlocked : 1;

  // True if a document loads a plugin object that attempts to load mixed
  // content subresources through necko(see nsMixedContentBlocker.cpp)
  bool mHasMixedContentObjectSubrequest : 1;

  // True if a document load has a CSP attached.
  bool mHasCSP : 1;

  // True if a document load has a CSP with unsafe-eval attached.
  bool mHasUnsafeEvalCSP : 1;

  // True if a document load has a CSP with unsafe-inline attached.
  bool mHasUnsafeInlineCSP : 1;

  // True if DisallowBFCaching has been called on this document.
  bool mBFCacheDisallowed : 1;

  bool mHasHadDefaultView : 1;

  // Whether style sheet change events will be dispatched for this document
  bool mStyleSheetChangeEventsEnabled : 1;

  // Whether the document was created by a srcdoc iframe.
  bool mIsSrcdocDocument : 1;

  // Whether this document has a display document and thus is considered to
  // be a resource document.  Normally this is the same as !!mDisplayDocument,
  // but mDisplayDocument is cleared during Unlink.  mHasDisplayDocument is
  // valid in the document's destructor.
  bool mHasDisplayDocument : 1;

  // Is the current mFontFaceSet valid?
  bool mFontFaceSetDirty : 1;

  // True if we have fired the DOMContentLoaded event, or don't plan to fire one
  // (e.g. we're not being parsed at all).
  bool mDidFireDOMContentLoaded : 1;

  // True if ReportHasScrollLinkedEffect() has been called.
  bool mHasScrollLinkedEffect : 1;

  // True if we have frame request callbacks scheduled with the refresh driver.
  // This should generally be updated only via
  // UpdateFrameRequestCallbackSchedulingState.
  bool mFrameRequestCallbacksScheduled : 1;

  bool mIsTopLevelContentDocument : 1;

  bool mIsContentDocument : 1;

  // True if we have called BeginLoad and are expecting a paired EndLoad call.
  bool mDidCallBeginLoad : 1;

  // True if the document is allowed to use PaymentRequest.
  bool mAllowPaymentRequest : 1;

  // True if the encoding menu should be disabled.
  bool mEncodingMenuDisabled : 1;

  // False if we've disabled link handling for elements inside this document,
  // true otherwise.
  bool mLinksEnabled : 1;

  // True if this document is for an SVG-in-OpenType font.
  bool mIsSVGGlyphsDocument : 1;

  // True if the document is being destroyed.
  bool mInDestructor : 1;

  // True if the document has been detached from its content viewer.
  bool mIsGoingAway : 1;

  bool mInXBLUpdate : 1;

  bool mNeedsReleaseAfterStackRefCntRelease : 1;

  // Whether we have filled our style set with all the stylesheets.
  bool mStyleSetFilled : 1;

  // Whether we have a quirks mode stylesheet in the style set.
  bool mQuirkSheetAdded : 1;

  // Whether we have a contenteditable.css stylesheet in the style set.
  bool mContentEditableSheetAdded : 1;

  // Whether we have a designmode.css stylesheet in the style set.
  bool mDesignModeSheetAdded : 1;

  // Keeps track of whether we have a pending
  // 'style-sheet-applicable-state-changed' notification.
  bool mSSApplicableStateNotificationPending : 1;

  // True if this document has ever had an HTML or SVG <title> element
  // bound to it
  bool mMayHaveTitleElement : 1;

  bool mDOMLoadingSet : 1;
  bool mDOMInteractiveSet : 1;
  bool mDOMCompleteSet : 1;
  bool mAutoFocusFired : 1;

  bool mScrolledToRefAlready : 1;
  bool mChangeScrollPosWhenScrollingToRef : 1;

  bool mDelayFrameLoaderInitialization : 1;

  bool mSynchronousDOMContentLoaded : 1;

  // Set to true when the document is possibly controlled by the ServiceWorker.
  // Used to prevent multiple requests to ServiceWorkerManager.
  bool mMaybeServiceWorkerControlled : 1;

  // These member variables cache information about the viewport so we don't
  // have to recalculate it each time.
  bool mAllowZoom : 1;
  bool mValidScaleFloat : 1;
  bool mValidMinScale : 1;
  bool mValidMaxScale : 1;
  bool mWidthStrEmpty : 1;

  // Parser aborted. True if the parser of this document was forcibly
  // terminated instead of letting it finish at its own pace.
  bool mParserAborted : 1;

  // Whether we have reported use counters for this document with Telemetry yet.
  // Normally this is only done at document destruction time, but for image
  // documents (SVG documents) that are not guaranteed to be destroyed, we
  // report use counters when the image cache no longer has any imgRequestProxys
  // pointing to them.  We track whether we ever reported use counters so
  // that we only report them once for the document.
  bool mReportedUseCounters : 1;

  bool mHasReportedShadowDOMUsage : 1;

  // True if this document contained, either directly or in a subdocument,
  // an HTMLMediaElement that played audibly. This should only be set on
  // top level content documents.
  bool mDocTreeHadAudibleMedia : 1;
  // True if this document contained, either directly or in a subdocument,
  // an HTMLMediaElement that was playing inaudibly and became audible and we
  // paused the HTMLMediaElement because it wasn't allowed to autoplay audibly.
  // This should only be set on top level content documents.
  bool mDocTreeHadPlayRevoked : 1;

  // Whether an event triggered by the refresh driver was delayed because this
  // document has suppressed events.
  bool mHasDelayedRefreshEvent : 1;

  // The HTML spec has a "iframe load in progress" flag, but that doesn't seem
  // to have the right semantics.  See
  // <https://github.com/whatwg/html/issues/4292>. What we have instead is a
  // flag that is set while the window's 'load' event is firing if this document
  // is the window's document.
  bool mLoadEventFiring : 1;

  // The HTML spec has a "mute iframe load" flag, but that doesn't seem to have
  // the right semantics.  See <https://github.com/whatwg/html/issues/4292>.
  // What we have instead is a flag that is set if completion of our document
  // via document.close() should skip firing the load event.  Note that this
  // flag is only relevant for HTML documents, but lives here for reasons that
  // are documented above on SkipLoadEventAfterClose().
  bool mSkipLoadEventAfterClose : 1;

  // When false, the .cookies property is completely disabled
  bool mDisableCookieAccess : 1;

  // When false, the document.write() API is disabled.
  bool mDisableDocWrite : 1;

  // Has document.write() been called with a recursion depth higher than
  // allowed?
  bool mTooDeepWriteRecursion : 1;

  /**
   * Temporary flag that is set in EndUpdate() to ignore
   * MaybeEditingStateChanged() script runners from a nested scope.
   */
  bool mPendingMaybeEditingStateChanged : 1;

  // mHasBeenEditable is set to true when mEditingState is firstly set to
  // eDesignMode or eContentEditable.
  bool mHasBeenEditable : 1;

  uint8_t mPendingFullscreenRequests;

  uint8_t mXMLDeclarationBits;

  // Currently active onload blockers.
  uint32_t mOnloadBlockCount;

  // Onload blockers which haven't been activated yet.
  uint32_t mAsyncOnloadBlockCount;

  // Tracks if we are currently processing any document.write calls (either
  // implicit or explicit). Note that if a write call writes out something which
  // would block the parser, then mWriteLevel will be incorrect until the parser
  // finishes processing that script.
  uint32_t mWriteLevel;

  uint32_t mContentEditableCount;
  EditingState mEditingState;

  // Compatibility mode
  nsCompatibility mCompatMode;

  // Our readyState
  ReadyState mReadyState;

  // Ancestor's loading state
  bool mAncestorIsLoading;

  // Our visibility state
  dom::VisibilityState mVisibilityState;

  enum Type {
    eUnknown,  // should never be used
    eHTML,
    eXHTML,
    eGenericXML,
    eSVG
  };

  Type mType;

  uint8_t mDefaultElementType;

  enum Tri { eTriUnset = 0, eTriFalse, eTriTrue };

  Tri mAllowXULXBL;

  bool mSkipDTDSecurityChecks;

  // The document's script global object, the object from which the
  // document can get its script context and scope. This is the
  // *inner* window object.
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;

  // If mIsStaticDocument is true, mOriginalDocument points to the original
  // document.
  RefPtr<Document> mOriginalDocument;

  // The bidi options for this document.  What this bitfield means is
  // defined in nsBidiUtils.h
  uint32_t mBidiOptions;

  // The sandbox flags on the document. These reflect the value of the sandbox
  // attribute of the associated IFRAME or CSP-protectable content, if existent.
  // These are set at load time and are immutable - see nsSandboxFlags.h for the
  // possible flags.
  uint32_t mSandboxFlags;

  nsCString mContentLanguage;

  // The channel that got passed to Document::StartDocumentLoad(), if any.
  nsCOMPtr<nsIChannel> mChannel;

  // The CSP for every load lives in the Client within the LoadInfo. For all
  // document-initiated subresource loads we can use that cached version of the
  // CSP so we do not have to deserialize the CSP from the Client all the time.
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIContentSecurityPolicy> mPreloadCSP;

 private:
  nsCString mContentType;

 protected:
  // The document's security info
  nsCOMPtr<nsISupports> mSecurityInfo;

  // The channel that failed to load and resulted in an error page.
  // This only applies to error pages. Might be null.
  nsCOMPtr<nsIChannel> mFailedChannel;

  // if this document is part of a multipart document,
  // the ID can be used to distinguish it from the other parts.
  uint32_t mPartID;

  // Cycle collector generation in which we're certain that this document
  // won't be collected
  uint32_t mMarkedCCGeneration;

  PresShell* mPresShell;

  nsCOMArray<nsINode> mSubtreeModifiedTargets;
  uint32_t mSubtreeModifiedDepth;

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
  uint32_t mPreloadPictureDepth;

  // Set if we've found a URL for the current picture
  nsString mPreloadPictureFoundSource;

  // If we're an external resource document, this will be non-null and will
  // point to our "display document": the one that all resource lookups should
  // go to.
  RefPtr<Document> mDisplayDocument;

  uint32_t mEventsSuppressed;

  // Any XHR ChannelEventQueues that were suspended on this document while
  // events were suppressed.
  nsTArray<RefPtr<net::ChannelEventQueue>> mSuspendedQueues;

  // Any postMessage events that were suspended on this document while events
  // were suppressed.
  nsTArray<RefPtr<PostMessageEvent>> mSuspendedPostMessageEvents;

  RefPtr<EventListener> mSuppressedEventListener;

  /**
   * https://html.spec.whatwg.org/#ignore-destructive-writes-counter
   */
  uint32_t mIgnoreDestructiveWritesCounter;

  /**
   * The current frame request callback handle
   */
  int32_t mFrameRequestCallbackCounter;

  // Count of live static clones of this document.
  uint32_t mStaticCloneCount;

  // If the document is currently printing (or in print preview) this will point
  // to the current static clone of this document. This is weak since the clone
  // also has a reference to this document.
  WeakPtr<Document> mLatestStaticClone;

  // Array of nodes that have been blocked to prevent user tracking.
  // They most likely have had their nsIChannel canceled by the URL
  // classifier. (Safebrowsing)
  //
  // Weak nsINode pointers are used to allow nodes to disappear.
  nsTArray<nsWeakPtr> mBlockedNodesByClassifier;

  // Weak reference to mScriptGlobalObject QI:d to nsPIDOMWindow,
  // updated on every set of mScriptGlobalObject.
  nsPIDOMWindowInner* mWindow;

  nsCOMPtr<nsIDocumentEncoder> mCachedEncoder;

  nsTArray<FrameRequest> mFrameRequestCallbacks;

  // The set of frame request callbacks that were canceled but which we failed
  // to find in mFrameRequestCallbacks.
  HashSet<int32_t> mCanceledFrameRequestCallbacks;

  // This object allows us to evict ourself from the back/forward cache.  The
  // pointer is non-null iff we're currently in the bfcache.
  nsIBFCacheEntry* mBFCacheEntry;

  // Our base target.
  nsString mBaseTarget;

  nsCOMPtr<nsIStructuredCloneContainer> mStateObjectContainer;
  nsCOMPtr<nsIVariant> mStateObjectCached;

  uint32_t mInSyncOperationCount;

  UniquePtr<dom::XPathEvaluator> mXPathEvaluator;

  nsTArray<RefPtr<AnonymousContent>> mAnonymousContents;

  uint32_t mBlockDOMContentLoaded;

  // Our live MediaQueryLists
  LinkedList<MediaQueryList> mDOMMediaQueryLists;

  // Array of observers
  nsTObserverArray<nsIDocumentObserver*> mObservers;

  // Flags for use counters used directly by this document.
  std::bitset<eUseCounter_Count> mUseCounters;
  // Flags for use counters used by any child documents of this document.
  std::bitset<eUseCounter_Count> mChildDocumentUseCounters;

  // The CSS property use counters.
  UniquePtr<StyleUseCounters> mStyleUseCounters;

  // An ever-increasing heuristic number that is higher the more content is
  // likely to be visible in the page.
  //
  // Right now it effectively measures amount of text content that has ever been
  // connected to the document in some way, and is not under a <script> or
  // <style>.
  //
  // Note that this is only measured during load.
  SaturateUint32 mVisibleContentHeuristic{0};

  // Whether the user has interacted with the document or not:
  bool mUserHasInteracted;

  // We constantly update the user-interaction anti-tracking permission at any
  // user-interaction using a timer. This boolean value is set to true when this
  // timer is scheduled.
  bool mHasUserInteractionTimerScheduled;

  TimeStamp mPageUnloadingEventTimeStamp;

  RefPtr<DocGroup> mDocGroup;

  RefPtr<nsCommandManager> mMidasCommandManager;

  // The set of all the tracking script URLs.  URLs are added to this set by
  // calling NoteScriptTrackingStatus().  Currently we assume that a URL not
  // existing in the set means the corresponding script isn't a tracking script.
  nsTHashtable<nsCStringHashKey> mTrackingScripts;

  // The log of all content blocking actions taken on this document.  This is
  // only stored on top-level documents and includes the activity log for all of
  // the nested subdocuments as well.
  ContentBlockingLog mContentBlockingLog;

  // List of ancestor principals.  This is set at the point a document
  // is connected to a docshell and not mutated thereafter.
  nsTArray<nsCOMPtr<nsIPrincipal>> mAncestorPrincipals;
  // List of ancestor outerWindowIDs that correspond to the ancestor principals.
  nsTArray<uint64_t> mAncestorOuterWindowIDs;

  // Pointer to our parser if we're currently in the process of being
  // parsed into.
  nsCOMPtr<nsIParser> mParser;

  // If the document was created from the the prototype cache there will be a
  // reference to the prototype document to allow tracing.
  RefPtr<nsXULPrototypeDocument> mPrototypeDocument;

  nsrefcnt mStackRefCnt;

  // Weak reference to our sink for in case we no longer have a parser.  This
  // will allow us to flush out any pending stuff from the sink even if
  // EndLoad() has already happened.
  nsWeakPtr mWeakSink;

  // Our update nesting level
  uint32_t mUpdateNestLevel;

  enum ViewportType : uint8_t {
    DisplayWidthHeight,
    Specified,
    Unknown,
    NoValidContent,
  };

  ViewportType mViewportType;

  PLDHashTable* mSubDocuments;

  DocHeaderData* mHeaderData;

  // For determining if this is a flash document which should be
  // blocked based on its principal.
  FlashClassification mFlashClassification;

  // Do not use this value directly. Call the |IsThirdPartyForFlashClassifier()|
  // method, which caches its result here.
  Maybe<bool> mIsThirdPartyForFlashClassifier;

  nsRevocableEventPtr<nsRunnableMethod<Document, void, false>>
      mPendingTitleChangeEvent;

  RefPtr<nsDOMNavigationTiming> mTiming;

  // Recorded time of change to 'loading' state.
  TimeStamp mLoadingTimeStamp;

  nsWeakPtr mAutoFocusElement;

  nsCString mScrollToRef;

  nscoord mScrollAnchorAdjustmentLength;
  int32_t mScrollAnchorAdjustmentCount;

  // Weak reference to the scope object (aka the script global object)
  // that, unlike mScriptGlobalObject, is never unset once set. This
  // is a weak reference to avoid leaks due to circular references.
  nsWeakPtr mScopeObject;

  // Array of intersection observers
  nsTHashtable<nsPtrHashKey<DOMIntersectionObserver>> mIntersectionObservers;

  // Stack of fullscreen elements. When we request fullscreen we push the
  // fullscreen element onto this stack, and when we cancel fullscreen we
  // pop one off this stack, restoring the previous fullscreen state
  nsTArray<nsWeakPtr> mFullscreenStack;

  // The root of the doc tree in which this document is in. This is only
  // non-null when this document is in fullscreen mode.
  nsWeakPtr mFullscreenRoot;

  RefPtr<DOMImplementation> mDOMImplementation;

  RefPtr<nsContentList> mImageMaps;

  // A set of responsive images keyed by address pointer.
  nsTHashtable<nsPtrHashKey<HTMLImageElement>> mResponsiveContent;

  // Tracking for plugins in the document.
  nsTHashtable<nsPtrHashKey<nsIObjectLoadingContent>> mPlugins;

  RefPtr<DocumentTimeline> mDocumentTimeline;
  LinkedList<DocumentTimeline> mTimelines;

  RefPtr<dom::ScriptLoader> mScriptLoader;

  // Tracker for animations that are waiting to start.
  // nullptr until GetOrCreatePendingAnimationTracker is called.
  RefPtr<PendingAnimationTracker> mPendingAnimationTracker;

  // A document "without a browsing context" that owns the content of
  // HTMLTemplateElement.
  RefPtr<Document> mTemplateContentsOwner;

  dom::ExternalResourceMap mExternalResourceMap;

  // ScreenOrientation "pending promise" as described by
  // http://www.w3.org/TR/screen-orientation/
  RefPtr<Promise> mOrientationPendingPromise;

  uint16_t mCurrentOrientationAngle;
  OrientationType mCurrentOrientationType;

  nsTArray<RefPtr<nsFrameLoader>> mInitializableFrameLoaders;
  nsTArray<nsCOMPtr<nsIRunnable>> mFrameLoaderFinalizers;
  RefPtr<nsRunnableMethod<Document>> mFrameLoaderRunner;

  // The layout history state that should be used by nodes in this
  // document.  We only actually store a pointer to it when:
  // 1)  We have no script global object.
  // 2)  We haven't had Destroy() called on us yet.
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;

  struct MetaViewportElementAndData {
    RefPtr<HTMLMetaElement> mElement;
    ViewportMetaData mData;

    bool operator==(const MetaViewportElementAndData& aOther) const {
      return mElement == aOther.mElement && mData == aOther.mData;
    }
  };
  // An array of <meta name="viewport"> elements and their data.
  nsTArray<MetaViewportElementAndData> mMetaViewports;

  // These member variables cache information about the viewport so we don't
  // have to recalculate it each time.
  LayoutDeviceToScreenScale mScaleMinFloat;
  LayoutDeviceToScreenScale mScaleMaxFloat;
  LayoutDeviceToScreenScale mScaleFloat;
  CSSToLayoutDeviceScale mPixelRatio;

  CSSCoord mMinWidth;
  CSSCoord mMaxWidth;
  CSSCoord mMinHeight;
  CSSCoord mMaxHeight;

  RefPtr<EventListenerManager> mListenerManager;

  nsCOMPtr<nsIRunnable> mMaybeEndOutermostXBLUpdateRunner;
  nsCOMPtr<nsIRequest> mOnloadBlocker;

  nsTArray<RefPtr<StyleSheet>> mAdditionalSheets[AdditionalSheetTypeCount];

  // Member to store out last-selected stylesheet set.
  nsString mLastStyleSheetSet;
  nsString mPreferredStyleSheetSet;

  RefPtr<DOMStyleSheetSetList> mStyleSheetSetList;

  // We lazily calculate declaration blocks for SVG elements with mapped
  // attributes in Servo mode. This list contains all elements which need lazy
  // resolution.
  nsTHashtable<nsPtrHashKey<SVGElement>> mLazySVGPresElements;

  // Most documents will only use one (or very few) language groups. Rather
  // than have the overhead of a hash lookup, we simply look along what will
  // typically be a very short (usually of length 1) linked list. There are 31
  // language groups, so in the worst case scenario we'll need to traverse 31
  // link items.
  LangGroupFontPrefs mLangGroupFontPrefs;

  nsTHashtable<nsRefPtrHashKey<nsAtom>> mLanguagesUsed;

  // TODO(emilio): Is this hot enough to warrant to be cached?
  RefPtr<nsAtom> mLanguageFromCharset;

  // Restyle root for servo's style system.
  //
  // We store this as an nsINode, rather than as an Element, so that we can
  // store the Document node as the restyle root if the entire document (along
  // with all document-level native-anonymous content) needs to be restyled.
  //
  // We also track which "descendant" bits (normal/animation-only/lazy-fc) the
  // root corresponds to.
  nsCOMPtr<nsINode> mServoRestyleRoot;
  uint32_t mServoRestyleRootDirtyBits;

  // Used in conjunction with the create-an-element-for-the-token algorithm to
  // prevent custom element constructors from being able to use document.open(),
  // document.close(), and document.write() when they are invoked by the parser.
  uint32_t mThrowOnDynamicMarkupInsertionCounter;

  // Count of unload/beforeunload/pagehide operations in progress.
  uint32_t mIgnoreOpensDuringUnloadCounter;

  nsCOMPtr<nsIDOMXULCommandDispatcher>
      mCommandDispatcher;  // [OWNER] of the focus tracker

  RefPtr<XULBroadcastManager> mXULBroadcastManager;
  RefPtr<XULPersist> mXULPersist;

  RefPtr<mozilla::dom::HTMLAllCollection> mAll;

  // document lightweight theme for use with :-moz-lwtheme,
  // :-moz-lwtheme-brighttext and :-moz-lwtheme-darktext
  DocumentTheme mDocLWTheme;

  // Pres shell resolution saved before entering fullscreen mode.
  float mSavedResolution;

  bool mPendingInitialTranslation;

  nsCOMPtr<nsICookieSettings> mCookieSettings;

  // Document generation. Gets incremented everytime it changes.
  int32_t mGeneration;

  // Cached TabSizes values for the document.
  int32_t mCachedTabSizeGeneration;
  nsTabSizes mCachedTabSizes;

  bool mInRDMPane;

  // The principal to use for the storage area of this document.
  nsCOMPtr<nsIPrincipal> mIntrinsicStoragePrincipal;

  // The principal to use for the content blocking allow list.
  nsCOMPtr<nsIPrincipal> mContentBlockingAllowListPrincipal;

  // See GetNextFormNumber and GetNextControlNumber.
  int32_t mNextFormNumber;
  int32_t mNextControlNumber;

 public:
  // Needs to be public because the bindings code pokes at it.
  js::ExpandoAndGeneration mExpandoAndGeneration;

  bool HasPendingInitialTranslation() { return mPendingInitialTranslation; }

  nsRefPtrHashtable<nsRefPtrHashKey<Element>, nsXULPrototypeElement>
      mL10nProtoElements;

  void TraceProtos(JSTracer* aTrc);
};

NS_DEFINE_STATIC_IID_ACCESSOR(Document, NS_IDOCUMENT_IID)

/**
 * mozAutoSubtreeModified batches DOM mutations so that a DOMSubtreeModified
 * event is dispatched, if necessary, when the outermost mozAutoSubtreeModified
 * object is deleted.
 */
class MOZ_STACK_CLASS mozAutoSubtreeModified {
 public:
  /**
   * @param aSubTreeOwner The document in which a subtree will be modified.
   * @param aTarget       The target of the possible DOMSubtreeModified event.
   *                      Can be nullptr, in which case mozAutoSubtreeModified
   *                      is just used to batch DOM mutations.
   */
  mozAutoSubtreeModified(Document* aSubtreeOwner, nsINode* aTarget) {
    UpdateTarget(aSubtreeOwner, aTarget);
  }

  ~mozAutoSubtreeModified() { UpdateTarget(nullptr, nullptr); }

  void UpdateTarget(Document* aSubtreeOwner, nsINode* aTarget) {
    if (mSubtreeOwner) {
      mSubtreeOwner->MutationEventDispatched(mTarget);
    }

    mTarget = aTarget;
    mSubtreeOwner = aSubtreeOwner;
    if (mSubtreeOwner) {
      mSubtreeOwner->WillDispatchMutationEvent(mTarget);
    }
  }

 private:
  nsCOMPtr<nsINode> mTarget;
  RefPtr<Document> mSubtreeOwner;
};

class MOZ_STACK_CLASS nsAutoSyncOperation {
 public:
  explicit nsAutoSyncOperation(Document* aDocument);
  ~nsAutoSyncOperation();

 private:
  nsTArray<RefPtr<Document>> mDocuments;
  uint32_t mMicroTaskLevel;
};

class MOZ_RAII AutoSetThrowOnDynamicMarkupInsertionCounter final {
 public:
  explicit AutoSetThrowOnDynamicMarkupInsertionCounter(Document* aDocument)
      : mDocument(aDocument) {
    mDocument->IncrementThrowOnDynamicMarkupInsertionCounter();
  }

  ~AutoSetThrowOnDynamicMarkupInsertionCounter() {
    mDocument->DecrementThrowOnDynamicMarkupInsertionCounter();
  }

 private:
  Document* mDocument;
};

class MOZ_RAII IgnoreOpensDuringUnload final {
 public:
  explicit IgnoreOpensDuringUnload(Document* aDoc) : mDoc(aDoc) {
    mDoc->IncrementIgnoreOpensDuringUnloadCounter();
  }

  ~IgnoreOpensDuringUnload() {
    mDoc->DecrementIgnoreOpensDuringUnloadCounter();
  }

 private:
  Document* mDoc;
};

}  // namespace dom
}  // namespace mozilla

// XXX These belong somewhere else
nsresult NS_NewHTMLDocument(mozilla::dom::Document** aInstancePtrResult,
                            bool aLoadedAsData = false);

nsresult NS_NewXMLDocument(mozilla::dom::Document** aInstancePtrResult,
                           bool aLoadedAsData = false,
                           bool aIsPlainDocument = false);

nsresult NS_NewSVGDocument(mozilla::dom::Document** aInstancePtrResult);

nsresult NS_NewImageDocument(mozilla::dom::Document** aInstancePtrResult);

nsresult NS_NewVideoDocument(mozilla::dom::Document** aInstancePtrResult);

// Enum for requesting a particular type of document when creating a doc
enum DocumentFlavor {
  DocumentFlavorLegacyGuess,  // compat with old code until made HTML5-compliant
  DocumentFlavorHTML,         // HTMLDocument with HTMLness bit set to true
  DocumentFlavorSVG,          // SVGDocument
  DocumentFlavorPlain,        // Just a Document
};

// Note: it's the caller's responsibility to create or get aPrincipal as needed
// -- this method will not attempt to get a principal based on aDocumentURI.
// Also, both aDocumentURI and aBaseURI must not be null.
nsresult NS_NewDOMDocument(
    mozilla::dom::Document** aInstancePtrResult, const nsAString& aNamespaceURI,
    const nsAString& aQualifiedName, mozilla::dom::DocumentType* aDoctype,
    nsIURI* aDocumentURI, nsIURI* aBaseURI, nsIPrincipal* aPrincipal,
    bool aLoadedAsData, nsIGlobalObject* aEventObject, DocumentFlavor aFlavor);

// This is used only for xbl documents created from the startup cache.
// Non-cached documents are created in the same manner as xml documents.
nsresult NS_NewXBLDocument(mozilla::dom::Document** aInstancePtrResult,
                           nsIURI* aDocumentURI, nsIURI* aBaseURI,
                           nsIPrincipal* aPrincipal);

nsresult NS_NewPluginDocument(mozilla::dom::Document** aInstancePtrResult);

inline mozilla::dom::Document* nsINode::GetOwnerDocument() const {
  mozilla::dom::Document* ownerDoc = OwnerDoc();

  return ownerDoc != this ? ownerDoc : nullptr;
}

inline nsINode* nsINode::OwnerDocAsNode() const { return OwnerDoc(); }

inline bool ShouldUseNACScope(const nsINode* aNode) {
  return aNode->IsInNativeAnonymousSubtree();
}

inline bool ShouldUseUAWidgetScope(const nsINode* aNode) {
  return aNode->HasBeenInUAWidget();
}

inline mozilla::dom::ParentObject nsINode::GetParentObject() const {
  mozilla::dom::ParentObject p(OwnerDoc());
  // Note that mReflectionScope is a no-op for chrome, and other places
  // where we don't check this value.
  if (ShouldUseNACScope(this)) {
    p.mReflectionScope = mozilla::dom::ReflectionScope::NAC;
  } else if (ShouldUseUAWidgetScope(this)) {
    p.mReflectionScope = mozilla::dom::ReflectionScope::UAWidget;
  }
  return p;
}

inline mozilla::dom::Document* nsINode::AsDocument() {
  MOZ_ASSERT(IsDocument());
  return static_cast<mozilla::dom::Document*>(this);
}

inline const mozilla::dom::Document* nsINode::AsDocument() const {
  MOZ_ASSERT(IsDocument());
  return static_cast<const mozilla::dom::Document*>(this);
}

inline nsISupports* ToSupports(mozilla::dom::Document* aDoc) {
  return static_cast<nsINode*>(aDoc);
}

#endif /* mozilla_dom_Document_h___ */
