/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocument_h___
#define nsIDocument_h___

#include "mozilla/FlushType.h"           // for enum
#include "nsAttrAndChildArray.h"
#include "nsAutoPtr.h"                   // for member
#include "nsCOMArray.h"                  // for member
#include "nsCompatibility.h"             // for member
#include "nsCOMPtr.h"                    // for member
#include "nsGkAtoms.h"                   // for static class members
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIContentViewer.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"                // for member (in nsCOMPtr)
#include "nsINode.h"                     // for base class
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIChannelEventSink.h"
#include "nsIProgressEventSink.h"
#include "nsISecurityEventSink.h"
#include "nsIScriptGlobalObject.h"       // for member (in nsCOMPtr)
#include "nsIServiceManager.h"
#include "nsIURI.h"                      // for use in inline functions
#include "nsIUUIDGenerator.h"
#include "nsPIDOMWindow.h"               // for use in inline functions
#include "nsPropertyTable.h"             // for member
#include "nsStringFwd.h"
#include "nsTHashtable.h"                // for member
#include "nsURIHashKey.h"
#include "mozilla/net/ReferrerPolicy.h"  // for member
#include "nsWeakReference.h"
#include "mozilla/UseCounter.h"
#include "mozilla/WeakPtr.h"
#include "Units.h"
#include "nsContentListDeclarations.h"
#include "nsExpirationTracker.h"
#include "nsClassHashtable.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/DispatcherTrait.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/LinkedList.h"
#include "mozilla/NotNull.h"
#include "mozilla/SegmentedVector.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include <bitset>                        // for member

// windows.h #defines CreateEvent
#ifdef CreateEvent
#undef CreateEvent
#endif

#ifdef MOZILLA_INTERNAL_API
#include "mozilla/dom/DocumentBinding.h"
#else
namespace mozilla {
namespace dom {
class ElementCreationOptionsOrString;
} // namespace dom
} // namespace mozilla
#endif // MOZILLA_INTERNAL_API

class gfxUserFontSet;
class imgIRequest;
class nsBindingManager;
class nsCachableElementsByNameNodeList;
class nsIDocShell;
class nsDocShell;
class nsDOMNavigationTiming;
class nsDOMStyleSheetSetList;
class nsFrameLoader;
class nsGlobalWindowInner;
class nsHTMLCSSStyleSheet;
class nsHTMLDocument;
class nsHTMLStyleSheet;
class nsGenericHTMLElement;
class nsAtom;
class nsIBFCacheEntry;
class nsIChannel;
class nsIContent;
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
class nsSMILAnimationController;
class nsSVGElement;
class nsTextNode;
class nsUnblockOnloadEvent;
class nsWindowSizes;
class nsDOMCaretPosition;
class nsViewportInfo;
class nsIGlobalObject;
class nsIXULWindow;

namespace mozilla {
class AbstractThread;
class CSSStyleSheet;
class Encoding;
class ErrorResult;
class EventStates;
class EventListenerManager;
class PendingAnimationTracker;
class ServoStyleSet;
template<typename> class OwningNonNull;
struct URLExtraData;

namespace css {
class Loader;
class ImageLoader;
class Rule;
} // namespace css

namespace dom {
class Animation;
class AnonymousContent;
class Attr;
class BoxObject;
class ClientInfo;
class ClientState;
class CDATASection;
class Comment;
struct CustomElementDefinition;
class DocGroup;
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
class FontFaceSet;
class FrameRequestCallback;
struct FullscreenRequest;
class ImageTracker;
class HTMLBodyElement;
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
class SVGSVGElement;
class Touch;
class TouchList;
class TreeWalker;
class XPathEvaluator;
class XPathExpression;
class XPathNSResolver;
class XPathResult;
class XULDocument;
template<typename> class Sequence;

template<typename, typename> class CallbackObjectHolder;

enum class CallerType : uint32_t;

} // namespace dom
} // namespace mozilla

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_IDOCUMENT_IID \
{ 0xce1f7627, 0x7109, 0x4977, \
  { 0xba, 0x77, 0x49, 0x0f, 0xfd, 0xe0, 0x7a, 0xaa } }

// Enum for requesting a particular type of document when creating a doc
enum DocumentFlavor {
  DocumentFlavorLegacyGuess, // compat with old code until made HTML5-compliant
  DocumentFlavorHTML, // HTMLDocument with HTMLness bit set to true
  DocumentFlavorSVG, // SVGDocument
  DocumentFlavorPlain, // Just a Document
};

// Document states

// RTL locale: specific to the XUL localedir attribute
#define NS_DOCUMENT_STATE_RTL_LOCALE              NS_DEFINE_EVENT_STATE_MACRO(0)
// Window activation status
#define NS_DOCUMENT_STATE_WINDOW_INACTIVE         NS_DEFINE_EVENT_STATE_MACRO(1)

// Some function forward-declarations
class nsContentList;
class nsDocumentOnStack;

class nsDocHeaderData
{
public:
  nsDocHeaderData(nsAtom* aField, const nsAString& aData)
    : mField(aField), mData(aData), mNext(nullptr)
  {
  }

  ~nsDocHeaderData(void)
  {
    delete mNext;
  }

  RefPtr<nsAtom> mField;
  nsString mData;
  nsDocHeaderData* mNext;
};

class nsExternalResourceMap
{
  typedef bool (*nsSubDocEnumFunc)(nsIDocument *aDocument, void *aData);

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
  class ExternalResourceLoad : public nsISupports
  {
  public:
    virtual ~ExternalResourceLoad() {}

    void AddObserver(nsIObserver* aObserver)
    {
      MOZ_ASSERT(aObserver, "Must have observer");
      mObservers.AppendElement(aObserver);
    }

    const nsTArray<nsCOMPtr<nsIObserver>> & Observers()
    {
      return mObservers;
    }
  protected:
    AutoTArray<nsCOMPtr<nsIObserver>, 8> mObservers;
  };

  nsExternalResourceMap();

  /**
   * Request an external resource document.  This does exactly what
   * nsIDocument::RequestExternalResource is documented to do.
   */
  nsIDocument* RequestResource(nsIURI* aURI,
                               nsINode* aRequestingNode,
                               nsIDocument* aDisplayDocument,
                               ExternalResourceLoad** aPendingLoad);

  /**
   * Enumerate the resource documents.  See
   * nsIDocument::EnumerateExternalResources.
   */
  void EnumerateResources(nsSubDocEnumFunc aCallback, void* aData);

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
    explicit PendingLoad(nsIDocument* aDisplayDocument) :
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
    nsCOMPtr<nsIDocument> mDisplayDocument;
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

//----------------------------------------------------------------------

// For classifying a flash document based on its principal.
class PrincipalFlashClassifier;

// Document interface.  This is implemented by all document objects in
// Gecko.
class nsIDocument : public nsINode,
                    public mozilla::dom::DocumentOrShadowRoot,
                    public mozilla::dom::DispatcherTrait
{
  typedef mozilla::dom::GlobalObject GlobalObject;

protected:
  using Encoding = mozilla::Encoding;
  template <typename T> using NotNull = mozilla::NotNull<T>;

public:
  typedef nsExternalResourceMap::ExternalResourceLoad ExternalResourceLoad;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicyEnum;
  typedef mozilla::dom::Element Element;
  typedef mozilla::dom::FullscreenRequest FullscreenRequest;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_IID)

#ifdef MOZILLA_INTERNAL_API
  nsIDocument();
#endif

  // This helper class must be set when we dispatch beforeunload and unload
  // events in order to avoid unterminate sync XHRs.
  class MOZ_RAII PageUnloadingEventTimeStamp
  {
    nsCOMPtr<nsIDocument> mDocument;
    bool mSet;

  public:
    explicit PageUnloadingEventTimeStamp(nsIDocument* aDocument)
      : mDocument(aDocument)
      , mSet(false)
    {
      MOZ_ASSERT(aDocument);
      if (mDocument->mPageUnloadingEventTimeStamp.IsNull()) {
        mDocument->SetPageUnloadingEventTimeStamp();
        mSet = true;
      }
    }

    ~PageUnloadingEventTimeStamp()
    {
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
   */
  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset,
                                     nsIContentSink* aSink = nullptr) = 0;
  virtual void StopDocumentLoad() = 0;

  virtual void SetSuppressParserErrorElement(bool aSuppress) {}
  virtual bool SuppressParserErrorElement() { return false; }

  virtual void SetSuppressParserErrorConsoleMessages(bool aSuppress) {}
  virtual bool SuppressParserErrorConsoleMessages() { return false; }

  // nsINode
  bool IsNodeOfType(uint32_t aFlags) const final;
  nsIContent* GetChildAt_Deprecated(uint32_t aIndex) const final
  {
    return mChildren.GetSafeChildAt(aIndex);
  }

  int32_t ComputeIndexOf(const nsINode* aPossibleChild) const final
  {
    return mChildren.IndexOfChild(aPossibleChild);
  }

  uint32_t GetChildCount() const final
  {
    return mChildren.ChildCount();
  }

  nsresult InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                             bool aNotify) override;
  void RemoveChildNode(nsIContent* aKid, bool aNotify) final;
  nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo,
                 nsINode **aResult,
                 bool aPreallocateChildren) const override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  /**
   * Signal that the document title may have changed
   * (see nsDocument::GetTitle).
   * @param aBoundTitleElement true if an HTML or SVG <title> element
   * has just been bound to the document.
   */
  void NotifyPossibleTitleChange(bool aBoundTitleElement);

  /**
   * Return the URI for the document. May return null.
   *
   * The value returned corresponds to the "document's address" in
   * HTML5.  As such, it may change over the lifetime of the document, for
   * instance as a result of the user navigating to a fragment identifier on
   * the page, or as a result to a call to pushState() or replaceState().
   *
   * https://html.spec.whatwg.org/multipage/dom.html#the-document%27s-address
   */
  nsIURI* GetDocumentURI() const
  {
    return mDocumentURI;
  }

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
  nsIURI* GetOriginalURI() const
  {
    return mOriginalURI;
  }

  /**
   * Set the URI for the document.  This also sets the document's original URI,
   * if it's null.
   */
  void SetDocumentURI(nsIURI* aURI);

  /**
   * Set the URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  void SetChromeXHRDocURI(nsIURI* aURI)
  {
    mChromeXHRDocURI = aURI;
  }

  /**
   * Set the base URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  void SetChromeXHRDocBaseURI(nsIURI* aURI)
  {
    mChromeXHRDocBaseURI = aURI;
  }

  /**
   * Set referrer policy and upgrade-insecure-requests flags
   */
  void ApplySettingsFromCSP(bool aSpeculative);

  already_AddRefed<nsIParser> CreatorParserOrNull()
  {
    nsCOMPtr<nsIParser> parser = mParser;
    return parser.forget();
  }

  /**
   * Return the referrer policy of the document. Return "default" if there's no
   * valid meta referrer tag found in the document.
   */
  ReferrerPolicyEnum GetReferrerPolicy() const
  {
    return mReferrerPolicy;
  }

  /**
   * GetReferrerPolicy() for Document.webidl.
   */
  uint32_t ReferrerPolicy() const
  {
    return GetReferrerPolicy();
  }

  /**
   * If true, this flag indicates that all mixed content subresource
   * loads for this document (and also embeded browsing contexts) will
   * be blocked.
   */
  bool GetBlockAllMixedContent(bool aPreload) const
  {
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
  bool GetUpgradeInsecureRequests(bool aPreload) const
  {
    if (aPreload) {
      return mUpgradeInsecurePreloads;
    }
    return mUpgradeInsecureRequests;
  }

  void SetReferrer(const nsACString& aReferrer) {
    mReferrer = aReferrer;
  }

  /**
   * Set the principal responsible for this document.  Chances are,
   * you do not want to be using this.
   */
  void SetPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Get the list of ancestor principals for a document.  This is the same as
   * the ancestor list for the document's docshell the last time SetContainer()
   * was called with a non-null argument. See the documentation for the
   * corresponding getter in docshell for how this list is determined.  We store
   * a copy of the list, because we may lose the ability to reach our docshell
   * before people stop asking us for this information.
   */
  const nsTArray<nsCOMPtr<nsIPrincipal>>& AncestorPrincipals() const
  {
    return mAncestorPrincipals;
  }

  /**
   * Get the list of ancestor outerWindowIDs for a document that correspond to
   * the ancestor principals (see above for more details).
   */
  const nsTArray<uint64_t>& AncestorOuterWindowIDs() const
  {
    return mAncestorOuterWindowIDs;
  }

  /**
   * Return the LoadGroup for the document. May return null.
   */
  already_AddRefed<nsILoadGroup> GetDocumentLoadGroup() const
  {
    nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);
    return group.forget();
  }

  /**
   * Return the fallback base URL for this document, as defined in the HTML
   * specification.  Note that this can return null if there is no document URI.
   *
   * XXXbz: This doesn't implement the bits for about:blank yet.
   */
  nsIURI* GetFallbackBaseURI() const
  {
    if (mIsSrcdocDocument && mParentDocument) {
      return mParentDocument->GetDocBaseURI();
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
  nsIURI* GetDocBaseURI() const
  {
    if (mDocumentBaseURI) {
      return mDocumentBaseURI;
    }
    return GetFallbackBaseURI();
  }

  already_AddRefed<nsIURI> GetBaseURI(bool aTryUseXHRDocBaseURI = false) const final;

  void SetBaseURI(nsIURI* aURI);

  /**
   * Return the URL data which style system needs for resolving url value.
   * This method attempts to use the cached object in mCachedURLData, but
   * if the base URI, document URI, or principal has changed since last
   * call to this function, or the function is called the first time for
   * the document, a new one is created.
   */
  mozilla::URLExtraData* DefaultStyleAttrURLData();

  /**
   * Get/Set the base target of a link in a document.
   */
  void GetBaseTarget(nsAString& aBaseTarget) const
  {
    aBaseTarget = mBaseTarget;
  }

  void SetBaseTarget(const nsString& aBaseTarget) {
    mBaseTarget = aBaseTarget;
  }

  /**
   * Return a standard name for the document's character set.
   */
  NotNull<const Encoding*> GetDocumentCharacterSet() const
  {
    return mCharacterSet;
  }

  /**
   * Set the document's character encoding.
   */
  virtual void SetDocumentCharacterSet(NotNull<const Encoding*> aEncoding);

  int32_t GetDocumentCharacterSetSource() const
  {
    return mCharacterSetSource;
  }

  // This method MUST be called before SetDocumentCharacterSet if
  // you're planning to call both.
  void SetDocumentCharacterSetSource(int32_t aCharsetSource)
  {
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
  void GetContentLanguage(nsAString& aContentLanguage) const
  {
    CopyASCIItoUTF16(mContentLanguage, aContentLanguage);
  }

  // The states BidiEnabled and MathMLEnabled should persist across multiple views
  // (screen, print) of the same document.

  /**
   * Check if the document contains bidi data.
   * If so, we have to apply the Unicode Bidi Algorithm.
   */
  bool GetBidiEnabled() const
  {
    return mBidiEnabled;
  }

  /**
   * Indicate the document contains bidi data.
   * Currently, we cannot disable bidi, because once bidi is enabled,
   * it affects a frame model irreversibly, and plays even though
   * the document no longer contains bidi data.
   */
  void SetBidiEnabled()
  {
    mBidiEnabled = true;
  }

  void SetMathMLEnabled()
  {
    mMathMLEnabled = true;
  }

  /**
   * Ask this document whether it's the initial document in its window.
   */
  bool IsInitialDocument() const
  {
    return mIsInitialDocumentInWindow;
  }

  /**
   * Tell this document that it's the initial document in its window.  See
   * comments on mIsInitialDocumentInWindow for when this should be called.
   */
  void SetIsInitialDocument(bool aIsInitialDocument)
  {
    mIsInitialDocumentInWindow = aIsInitialDocument;
  }

  /**
   * Normally we assert if a runnable labeled with one DocGroup touches data
   * from another DocGroup. Calling IgnoreDocGroupMismatches() on a document
   * means that we can touch that document from any DocGroup without asserting.
   */
  void IgnoreDocGroupMismatches()
  {
    mIgnoreDocGroupMismatches = true;
  }

  /**
   * Get the bidi options for this document.
   * @see nsBidiUtils.h
   */
  uint32_t GetBidiOptions() const
  {
    return mBidiOptions;
  }

  /**
   * Set the bidi options for this document.  This just sets the bits;
   * callers are expected to take action as needed if they want this
   * change to actually change anything immediately.
   * @see nsBidiUtils.h
   */
  void SetBidiOptions(uint32_t aBidiOptions)
  {
    mBidiOptions = aBidiOptions;
  }

  /**
   * Get the has mixed active content loaded flag for this document.
   */
  bool GetHasMixedActiveContentLoaded()
  {
    return mHasMixedActiveContentLoaded;
  }

  /**
   * Set the has mixed active content loaded flag for this document.
   */
  void SetHasMixedActiveContentLoaded(bool aHasMixedActiveContentLoaded)
  {
    mHasMixedActiveContentLoaded = aHasMixedActiveContentLoaded;
  }

  /**
   * Get mixed active content blocked flag for this document.
   */
  bool GetHasMixedActiveContentBlocked()
  {
    return mHasMixedActiveContentBlocked;
  }

  /**
   * Set the mixed active content blocked flag for this document.
   */
  void SetHasMixedActiveContentBlocked(bool aHasMixedActiveContentBlocked)
  {
    mHasMixedActiveContentBlocked = aHasMixedActiveContentBlocked;
  }

  /**
   * Get the has mixed display content loaded flag for this document.
   */
  bool GetHasMixedDisplayContentLoaded()
  {
    return mHasMixedDisplayContentLoaded;
  }

  /**
   * Set the has mixed display content loaded flag for this document.
   */
  void SetHasMixedDisplayContentLoaded(bool aHasMixedDisplayContentLoaded)
  {
    mHasMixedDisplayContentLoaded = aHasMixedDisplayContentLoaded;
  }

  /**
   * Get mixed display content blocked flag for this document.
   */
  bool GetHasMixedDisplayContentBlocked()
  {
    return mHasMixedDisplayContentBlocked;
  }

  /**
   * Set the mixed display content blocked flag for this document.
   */
  void SetHasMixedDisplayContentBlocked(bool aHasMixedDisplayContentBlocked)
  {
    mHasMixedDisplayContentBlocked = aHasMixedDisplayContentBlocked;
  }

  /**
   * Set the mixed content object subrequest flag for this document.
   */
  void SetHasMixedContentObjectSubrequest(bool aHasMixedContentObjectSubrequest)
  {
    mHasMixedContentObjectSubrequest = aHasMixedContentObjectSubrequest;
  }

  /**
   * Set CSP flag for this document.
   */
  void SetHasCSP(bool aHasCSP)
  {
    mHasCSP = aHasCSP;
  }

  /**
   * Set unsafe-inline CSP flag for this document.
   */
  void SetHasUnsafeInlineCSP(bool aHasUnsafeInlineCSP)
  {
    mHasUnsafeInlineCSP = aHasUnsafeInlineCSP;
  }

  /**
   * Set unsafe-eval CSP flag for this document.
   */
  void SetHasUnsafeEvalCSP(bool aHasUnsafeEvalCSP)
  {
    mHasUnsafeEvalCSP = aHasUnsafeEvalCSP;
  }

  /**
   * Get tracking content blocked flag for this document.
   */
  bool GetHasTrackingContentBlocked()
  {
    return mHasTrackingContentBlocked;
  }

  /**
   * Get tracking cookies blocked flag for this document.
   */
  bool GetHasTrackingCookiesBlocked()
  {
    return mHasTrackingCookiesBlocked;
  }

  /**
   * Set the tracking content blocked flag for this document.
   */
  void SetHasTrackingContentBlocked(bool aHasTrackingContentBlocked)
  {
    mHasTrackingContentBlocked = aHasTrackingContentBlocked;
  }

  /**
   * Set the tracking cookies blocked flag for this document.
   */
  void SetHasTrackingCookiesBlocked(bool aHasTrackingCookiesBlocked)
  {
    mHasTrackingCookiesBlocked = aHasTrackingCookiesBlocked;
  }

  /**
   * Get tracking content loaded flag for this document.
   */
  bool GetHasTrackingContentLoaded()
  {
    return mHasTrackingContentLoaded;
  }

  /**
   * Set the tracking content loaded flag for this document.
   */
  void SetHasTrackingContentLoaded(bool aHasTrackingContentLoaded)
  {
    mHasTrackingContentLoaded = aHasTrackingContentLoaded;
  }

  /**
   * Get the sandbox flags for this document.
   * @see nsSandboxFlags.h for the possible flags
   */
  uint32_t GetSandboxFlags() const
  {
    return mSandboxFlags;
  }

  /**
   * Get string representation of sandbox flags (null if no flags are set)
   */
  void GetSandboxFlagsAsString(nsAString& aFlags);

  /**
   * Set the sandbox flags for this document.
   * @see nsSandboxFlags.h for the possible flags
   */
  void SetSandboxFlags(uint32_t sandboxFlags)
  {
    mSandboxFlags = sandboxFlags;
  }

  /**
   * Called when the document was decoded as UTF-8 and decoder encountered no
   * errors.
   */
  void EnableEncodingMenu()
  {
    mEncodingMenuDisabled = false;
  }

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
  already_AddRefed<nsIPresShell> CreateShell(
    nsPresContext* aContext,
    nsViewManager* aViewManager,
    mozilla::UniquePtr<mozilla::ServoStyleSet> aStyleSet);
  void DeleteShell();

  nsIPresShell* GetShell() const
  {
    return GetBFCacheEntry() ? nullptr : mPresShell;
  }

  nsIPresShell* GetObservingShell() const
  {
    return mPresShell && mPresShell->IsObservingDocument()
      ? mPresShell : nullptr;
  }

  // Return whether the presshell for this document is safe to flush.
  bool IsSafeToFlush() const;

  nsPresContext* GetPresContext() const
  {
    nsIPresShell* shell = GetShell();
    return shell ? shell->GetPresContext() : nullptr;
  }

  bool HasShellOrBFCacheEntry() const
  {
    return mPresShell || mBFCacheEntry;
  }

  // Instead using this method, what you probably want is
  // RemoveFromBFCacheSync() as we do in MessagePort and BroadcastChannel.
  void DisallowBFCaching()
  {
    NS_ASSERTION(!mBFCacheEntry, "We're already in the bfcache!");
    mBFCacheDisallowed = true;
  }

  bool IsBFCachingAllowed() const
  {
    return !mBFCacheDisallowed;
  }

  // Accepts null to clear the BFCache entry too.
  void SetBFCacheEntry(nsIBFCacheEntry* aEntry);

  nsIBFCacheEntry* GetBFCacheEntry() const
  {
    return mBFCacheEntry;
  }

  /**
   * Return the parent document of this document. Will return null
   * unless this document is within a compound document and has a
   * parent. Note that this parent chain may cross chrome boundaries.
   */
  nsIDocument *GetParentDocument() const
  {
    return mParentDocument;
  }

  /**
   * Set the parent document of this document.
   */
  void SetParentDocument(nsIDocument* aParent)
  {
    mParentDocument = aParent;
    if (aParent) {
      mIgnoreDocGroupMismatches = aParent->mIgnoreDocGroupMismatches;
    }
  }

  /**
   * Are plugins allowed in this document ?
   */
  bool GetAllowPlugins();

  /**
   * Set the sub document for aContent to aSubDoc.
   */
  nsresult SetSubDocumentFor(Element* aContent, nsIDocument* aSubDoc);

  /**
   * Get the sub document for aContent
   */
  nsIDocument* GetSubDocumentFor(nsIContent* aContent) const;

  /**
   * Find the content node for which aDocument is a sub document.
   */
  Element* FindContentForSubDocument(nsIDocument* aDocument) const;

  /**
   * Return the doctype for this document.
   */
  mozilla::dom::DocumentType* GetDoctype() const;

  /**
   * Return the root element for this document.
   */
  Element* GetRootElement() const;

  mozilla::dom::Selection* GetSelection(mozilla::ErrorResult& aRv);

  /**
   * Gets the event target to dispatch key events to if there is no focused
   * content in the document.
   */
  virtual nsIContent* GetUnfocusedKeyEventTarget();

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
  nsViewportInfo GetViewportInfo(const mozilla::ScreenIntSize& aDisplaySize);

  /**
   * It updates the viewport overflow type with the given two widths
   * and the viewport setting of the document.
   * This should only be called when there is out-of-reach overflow
   * happens on the viewport, i.e. the viewport should be using
   * `overflow: hidden`. And it should only be called on a top level
   * content document.
   */
  void UpdateViewportOverflowType(nscoord aScrolledWidth,
                                  nscoord aScrollportWidth);

  /**
   * True iff this doc will ignore manual character encoding overrides.
   */
  virtual bool WillIgnoreCharsetOverride() {
    return true;
  }

  /**
   * Return whether the document was created by a srcdoc iframe.
   */
  bool IsSrcdocDocument() const {
    return mIsSrcdocDocument;
  }

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

  bool DidDocumentOpen() {
    return mDidDocumentOpen;
  }

  already_AddRefed<mozilla::dom::AnonymousContent>
  InsertAnonymousContent(mozilla::dom::Element& aElement,
                         mozilla::ErrorResult& aError);
  void RemoveAnonymousContent(mozilla::dom::AnonymousContent& aContent,
                              mozilla::ErrorResult& aError);
  /**
   * If aNode is a descendant of anonymous content inserted by
   * InsertAnonymousContent, this method returns the root element of the
   * inserted anonymous content (in other words, the clone of the aElement
   * that was passed to InsertAnonymousContent).
   */
  Element* GetAnonRootIfInAnonymousContentContainer(nsINode* aNode) const;
  nsTArray<RefPtr<mozilla::dom::AnonymousContent>>& GetAnonymousContents() {
    return mAnonymousContents;
  }

  mozilla::TimeStamp GetPageUnloadingEventTimeStamp() const
  {
    if (!mParentDocument) {
      return mPageUnloadingEventTimeStamp;
    }

    mozilla::TimeStamp parentTimeStamp(mParentDocument->GetPageUnloadingEventTimeStamp());
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
  void ScheduleSVGForPresAttrEvaluation(nsSVGElement* aSVG)
  {
    mLazySVGPresElements.PutEntry(aSVG);
  }

  // Unschedule an element scheduled by ScheduleFrameRequestCallback (e.g. for
  // when it is destroyed)
  void UnscheduleSVGForPresAttrEvaluation(nsSVGElement* aSVG)
  {
    mLazySVGPresElements.RemoveEntry(aSVG);
  }

  // Resolve all SVG pres attrs scheduled in ScheduleSVGForPresAttrEvaluation
  void ResolveScheduledSVGPresAttrs();

  mozilla::Maybe<mozilla::dom::ClientInfo> GetClientInfo() const;
  mozilla::Maybe<mozilla::dom::ClientState> GetClientState() const;
  mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor> GetController() const;

  // Returns the size of the mBlockedTrackingNodes array.
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
  long BlockedTrackingNodeCount() const
  {
    return mBlockedTrackingNodes.Length();
  }

  //
  // Returns strong references to mBlockedTrackingNodes. (nsIDocument.h)
  //
  // This array contains nodes that have been blocked to prevent
  // user tracking. They most likely have had their nsIChannel
  // canceled by the URL classifier (Safebrowsing).
  //
  already_AddRefed<nsSimpleContentList> BlockedTrackingNodes() const;

protected:
  friend class nsUnblockOnloadEvent;

  nsresult InitCSP(nsIChannel* aChannel);

  void PostUnblockOnloadEvent();

  void DoUnblockOnload();

  void ClearAllBoxObjects();

  void MaybeEndOutermostXBLUpdate();

  void DispatchContentLoadedEvents();

  void DispatchPageTransition(mozilla::dom::EventTarget* aDispatchTarget,
                              const nsAString& aType,
                              bool aPersisted);

  // Call this before the document does something that will unbind all content.
  // That will stop us from doing a lot of work as each element is removed.
  void DestroyElementMaps();

  Element* GetRootElementInternal() const;
  void DoNotifyPossibleTitleChange();

  void SetPageUnloadingEventTimeStamp()
  {
    MOZ_ASSERT(!mPageUnloadingEventTimeStamp);
    mPageUnloadingEventTimeStamp = mozilla::TimeStamp::NowLoRes();
  }

  void CleanUnloadEventsTimeStamp()
  {
    MOZ_ASSERT(mPageUnloadingEventTimeStamp);
    mPageUnloadingEventTimeStamp = mozilla::TimeStamp();
  }

  /**
   * Clears any Servo element data stored on Elements in the document.
   */
  void ClearStaleServoData();

private:
  class SelectorCacheKey
  {
    public:
      explicit SelectorCacheKey(const nsAString& aString) : mKey(aString)
      {
        MOZ_COUNT_CTOR(SelectorCacheKey);
      }

      nsString mKey;
      nsExpirationState mState;

      nsExpirationState* GetExpirationState() { return &mState; }

      ~SelectorCacheKey()
      {
        MOZ_COUNT_DTOR(SelectorCacheKey);
      }
  };

  class SelectorCacheKeyDeleter;

public:
  class SelectorCache final
    : public nsExpirationTracker<SelectorCacheKey, 4>
  {
  public:
    using SelectorList = mozilla::UniquePtr<RawServoSelectorList>;

    explicit SelectorCache(nsIEventTarget* aEventTarget);

    void CacheList(const nsAString& aSelector, SelectorList aSelectorList)
    {
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
    SelectorList* GetList(const nsAString& aSelector)
    {
      return mTable.GetValue(aSelector);
    }

    ~SelectorCache();

  private:
    nsDataHashtable<nsStringHashKey, SelectorList> mTable;
  };

  SelectorCache& GetSelectorCache() {
    if (!mSelectorCache) {
      mSelectorCache =
        mozilla::MakeUnique<SelectorCache>(
          EventTargetFor(mozilla::TaskCategory::Other));
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
  mozilla::dom::HTMLBodyElement* GetBodyElement();
  // Get the canonical <head> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <head> isn't there)
  Element* GetHeadElement() {
    return GetHtmlChildElement(nsGkAtoms::head);
  }
  // Get the "body" in the sense of document.body: The first <body> or
  // <frameset> that's a child of a root <html>
  nsGenericHTMLElement* GetBody();
  // Set the "body" in the sense of document.body.
  void SetBody(nsGenericHTMLElement* aBody, mozilla::ErrorResult& rv);
  // Get the "head" element in the sense of document.head.
  mozilla::dom::HTMLSharedElement* GetHead();

  /**
   * Accessors to the collection of stylesheets owned by this document.
   * Style sheets are ordered, most significant last.
   */

  mozilla::dom::StyleSheetList* StyleSheets()
  {
    return &DocumentOrShadowRoot::EnsureDOMStyleSheets();
  }

  void InsertSheetAt(size_t aIndex, mozilla::StyleSheet&);

  /**
   * Replace the stylesheets in aOldSheets with the stylesheets in
   * aNewSheets. The two lists must have equal length, and the sheet
   * at positon J in the first list will be replaced by the sheet at
   * position J in the second list.  Some sheets in the second list
   * may be null; if so the corresponding sheets in the first list
   * will simply be removed.
   */
  void UpdateStyleSheets(
      nsTArray<RefPtr<mozilla::StyleSheet>>& aOldSheets,
      nsTArray<RefPtr<mozilla::StyleSheet>>& aNewSheets);

  /**
   * Add a stylesheet to the document
   *
   * TODO(emilio): This is only used by parts of editor that are no longer in
   * use by m-c or c-c, so remove.
   */
  void AddStyleSheet(mozilla::StyleSheet* aSheet)
  {
    MOZ_ASSERT(aSheet);
    InsertSheetAt(SheetCount(), *aSheet);
  }

  /**
   * Remove a stylesheet from the document
   */
  void RemoveStyleSheet(mozilla::StyleSheet* aSheet);

  /**
   * Notify the document that the applicable state of the sheet changed
   * and that observers should be notified and style sets updated
   */
  void SetStyleSheetApplicableState(mozilla::StyleSheet* aSheet,
                                    bool aApplicable);

  enum additionalSheetType {
    eAgentSheet,
    eUserSheet,
    eAuthorSheet,
    AdditionalSheetTypeCount
  };

  nsresult LoadAdditionalStyleSheet(additionalSheetType aType,
                                    nsIURI* aSheetURI);
  nsresult AddAdditionalStyleSheet(additionalSheetType aType,
                                   mozilla::StyleSheet* aSheet);
  void RemoveAdditionalStyleSheet(additionalSheetType aType, nsIURI* sheetURI);

  mozilla::StyleSheet* GetFirstAdditionalAuthorSheet()
  {
    return mAdditionalSheets[eAuthorSheet].SafeElementAt(0);
  }

  /**
   * Assuming that aDocSheets is an array of document-level style
   * sheets for this document, returns the index that aSheet should
   * be inserted at to maintain document ordering.
   *
   * Type T has to cast to StyleSheet*.
   *
   * Defined in nsIDocumentInlines.h.
   */
  template<typename T>
  size_t FindDocStyleSheetInsertionPoint(const nsTArray<T>& aDocSheets,
                                         const mozilla::StyleSheet& aSheet);

  /**
   * Get this document's CSSLoader.  This is guaranteed to not return null.
   */
  mozilla::css::Loader* CSSLoader() const {
    return mCSSLoader;
  }

  /**
   * Get this document's StyleImageLoader.  This is guaranteed to not return null.
   */
  mozilla::css::ImageLoader* StyleImageLoader() const {
    return mStyleImageLoader;
  }

  /**
   * Get the channel that was passed to StartDocumentLoad or Reset for this
   * document.  Note that this may be null in some cases (eg if
   * StartDocumentLoad or Reset were never called)
   */
  nsIChannel* GetChannel() const
  {
    return mChannel;
  }

  /**
   * Get this document's attribute stylesheet.  May return null if
   * there isn't one.
   */
  nsHTMLStyleSheet* GetAttributeStyleSheet() const {
    return mAttrStyleSheet;
  }

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
  nsIScriptGlobalObject*
    GetScriptHandlingObject(bool& aHasHadScriptHandlingObject) const
  {
    aHasHadScriptHandlingObject = mHasHadScriptHandlingObject;
    return mScriptGlobalObject ? mScriptGlobalObject.get() :
                                 GetScriptHandlingObjectInternal();
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
  nsPIDOMWindowOuter *GetWindow() const
  {
    return mWindow ? mWindow->GetOuterWindow() : GetWindowInternal();
  }

  bool IsInBackgroundWindow() const
  {
    auto* outer = mWindow ? mWindow->GetOuterWindow() : nullptr;
    return outer && outer->IsBackground();
  }

  /**
   * Return the inner window used as the script compilation scope for
   * this document. If you're not absolutely sure you need this, use
   * GetWindow().
   */
  nsPIDOMWindowInner* GetInnerWindow() const
  {
    return mRemovedFromDocShell ? nullptr : mWindow;
  }

  /**
   * Return the outer window ID.
   */
  uint64_t OuterWindowID() const
  {
    nsPIDOMWindowOuter* window = GetWindow();
    return window ? window->WindowID() : 0;
  }

  /**
   * Return the inner window ID.
   */
  uint64_t InnerWindowID() const
  {
    nsPIDOMWindowInner* window = GetInnerWindow();
    return window ? window->WindowID() : 0;
  }

  bool IsTopLevelWindowInactive() const;

  /**
   * Get the script loader for this document
   */
  mozilla::dom::ScriptLoader* ScriptLoader()
  {
    return mScriptLoader;
  }

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
   * returned to fullscreen status by calling RestorePreviousFullScreenState().
   *
   * Note that requesting fullscreen in a document also makes the element which
   * contains this document in this document's parent document fullscreen. i.e.
   * the <iframe> or <browser> that contains this document is also mode
   * fullscreen. This happens recursively in all ancestor documents.
   */
  void AsyncRequestFullScreen(mozilla::UniquePtr<FullscreenRequest>&&);

  // Do the "fullscreen element ready check" from the fullscreen spec.
  // It returns true if the given element is allowed to go into fullscreen.
  bool FullscreenElementReadyCheck(Element* aElement, bool aWasCallerChrome);

  // This is called asynchronously by nsIDocument::AsyncRequestFullScreen()
  // to move this document into full-screen mode if allowed.
  void RequestFullScreen(mozilla::UniquePtr<FullscreenRequest>&& aRequest);

  // Removes all elements from the full-screen stack, removing full-scren
  // styles from the top element in the stack.
  void CleanupFullscreenState();

  // Pushes aElement onto the full-screen stack, and removes full-screen styles
  // from the former full-screen stack top, and its ancestors, and applies the
  // styles to aElement. aElement becomes the new "full-screen element".
  bool FullScreenStackPush(Element* aElement);

  // Remove the top element from the full-screen stack. Removes the full-screen
  // styles from the former top element, and applies them to the new top
  // element, if there is one.
  void FullScreenStackPop();

  /**
   * Called when a frame in a child process has entered fullscreen or when a
   * fullscreen frame in a child process changes to another origin.
   * aFrameElement is the frame element which contains the child-process
   * fullscreen document.
   */
  nsresult RemoteFrameFullscreenChanged(mozilla::dom::Element* aFrameElement);

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
   * Restores the previous full-screen element to full-screen status. If there
   * is no former full-screen element, this exits full-screen, moving the
   * top-level browser window out of full-screen mode.
   */
  void RestorePreviousFullScreenState();

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
  nsIDocument* GetFullscreenRoot();

  /**
   * Sets the fullscreen root to aRoot. This stores a weak reference to aRoot
   * in this document.
   */
  void SetFullscreenRoot(nsIDocument* aRoot);

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
  static void ExitFullscreenInDocTree(nsIDocument* aDocument);

  /**
   * Ask the document to exit fullscreen state asynchronously.
   *
   * Different from ExitFullscreenInDocTree(), this allows the window
   * to perform fullscreen transition first if any.
   *
   * If aDocument is null, it will exit fullscreen from all documents
   * in all windows.
   */
  static void AsyncExitFullscreen(nsIDocument* aDocument);

  /**
   * Handles any pending fullscreen in aDocument or its subdocuments.
   *
   * Returns whether there is any fullscreen request handled.
   */
  static bool HandlePendingFullscreenRequests(nsIDocument* aDocument);

  /**
   * Dispatch fullscreenerror event and report the failure message to
   * the console.
   */
  void DispatchFullscreenError(const char* aMessage);

  void RequestPointerLock(Element* aElement, mozilla::dom::CallerType);
  bool SetPointerLock(Element* aElement, int aCursorStyle);

  static void UnlockPointer(nsIDocument* aDoc = nullptr);

  // ScreenOrientation related APIs

  void SetCurrentOrientation(mozilla::dom::OrientationType aType,
                             uint16_t aAngle)
  {
    mCurrentOrientationType = aType;
    mCurrentOrientationAngle = aAngle;
  }

  uint16_t CurrentOrientationAngle() const
  {
    return mCurrentOrientationAngle;
  }
  mozilla::dom::OrientationType CurrentOrientationType() const
  {
    return mCurrentOrientationType;
  }
  void SetOrientationPendingPromise(mozilla::dom::Promise* aPromise);
  mozilla::dom::Promise* GetOrientationPendingPromise() const
  {
    return mOrientationPendingPromise;
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
  virtual void EndUpdate() = 0;
  uint32_t UpdateNestingLevel() { return mUpdateNestLevel; }

  virtual void BeginLoad() = 0;
  virtual void EndLoad() = 0;

  enum ReadyState { READYSTATE_UNINITIALIZED = 0, READYSTATE_LOADING = 1, READYSTATE_INTERACTIVE = 3, READYSTATE_COMPLETE = 4};
  void SetReadyStateInternal(ReadyState rs);
  ReadyState GetReadyStateEnum()
  {
    return mReadyState;
  }

  // notify that a content node changed state.  This must happen under
  // a scriptblocker but NOT within a begin/end update.
  void ContentStateChanged(
      nsIContent* aContent, mozilla::EventStates aStateMask);

  // Notify that a document state has changed.
  // This should only be called by callers whose state is also reflected in the
  // implementation of nsDocument::GetDocumentState.
  void DocumentStatesChanged(mozilla::EventStates aStateMask);

  // Observation hooks for style data to propagate notifications
  // to document observers
  void StyleRuleChanged(mozilla::StyleSheet* aStyleSheet,
                        mozilla::css::Rule* aStyleRule);
  void StyleRuleAdded(mozilla::StyleSheet* aStyleSheet,
                      mozilla::css::Rule* aStyleRule);
  void StyleRuleRemoved(mozilla::StyleSheet* aStyleSheet,
                        mozilla::css::Rule* aStyleRule);

  /**
   * Flush notifications for this document and its parent documents
   * (since those may affect the layout of this one).
   */
  void FlushPendingNotifications(mozilla::FlushType aType);

  /**
   * Another variant of the above FlushPendingNotifications.  This function
   * takes a ChangesToFlush to specify whether throttled animations are flushed
   * or not.
   * If in doublt, use the above FlushPendingNotifications.
   */
  void FlushPendingNotifications(mozilla::ChangesToFlush aFlush);

  /**
   * Calls FlushPendingNotifications on any external resources this document
   * has. If this document has no external resources or is an external resource
   * itself this does nothing. This should only be called with
   * aType >= FlushType::Style.
   */
  void FlushExternalResources(mozilla::FlushType aType);

  nsBindingManager* BindingManager() const
  {
    return mNodeInfoManager->GetBindingManager();
  }

  /**
   * Only to be used inside Gecko, you can't really do anything with the
   * pointer outside Gecko anyway.
   */
  nsNodeInfoManager* NodeInfoManager() const
  {
    return mNodeInfoManager;
  }

  /**
   * Reset the document using the given channel and loadgroup.  This works
   * like ResetToURI, but also sets the document's channel to aChannel.
   * The principal of the document will be set from the channel.
   */
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);

  /**
   * Reset this document to aURI, aLoadGroup, and aPrincipal.  aURI must not be
   * null.  If aPrincipal is null, a codebase principal based on aURI will be
   * used.
   */
  virtual void ResetToURI(nsIURI* aURI,
                          nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal);

  /**
   * Set the container (docshell) for this document. Virtual so that
   * docshell can call it.
   */
  virtual void SetContainer(nsDocShell* aContainer);

  /**
   * Get the container (docshell) for this document.
   */
  virtual nsISupports* GetContainer() const;

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
  void SetXMLDeclaration(const char16_t* aVersion,
                         const char16_t* aEncoding,
                         const int32_t aStandalone);
  void GetXMLDeclaration(nsAString& aVersion,
                         nsAString& aEncoding,
                         nsAString& Standalone);

  /**
   * Returns true if this is what HTML 5 calls an "HTML document" (for example
   * regular HTML document with Content-Type "text/html", image documents and
   * media documents).  Returns false for XHTML and any other documents parsed
   * by the XML parser.
   */
  bool IsHTMLDocument() const
  {
    return mType == eHTML;
  }
  bool IsHTMLOrXHTML() const
  {
    return mType == eHTML || mType == eXHTML;
  }
  bool IsXMLDocument() const
  {
    return !IsHTMLDocument();
  }
  bool IsSVGDocument() const
  {
    return mType == eSVG;
  }
  bool IsXULDocument() const
  {
    return mType == eXUL;
  }
  bool IsUnstyledDocument()
  {
    return IsLoadedAsData() || IsLoadedAsInteractiveData();
  }
  bool LoadsFullXULStyleSheetUpFront()
  {
    if (IsXULDocument()) {
      return true;
    }
    if (IsSVGDocument()) {
      return false;
    }
    return AllowXULXBL();
  }

  bool IsScriptEnabled();

  bool IsTopLevelContentDocument() const { return mIsTopLevelContentDocument; }
  void SetIsTopLevelContentDocument(bool aIsTopLevelContentDocument)
  {
    mIsTopLevelContentDocument = aIsTopLevelContentDocument;
    // When a document is set as TopLevelContentDocument, it must be
    // allowpaymentrequest. We handle the false case while a document is appended
    // in SetSubDocumentFor
    if (aIsTopLevelContentDocument) {
      SetAllowPaymentRequest(true);
    }
  }

  bool IsContentDocument() const { return mIsContentDocument; }
  void SetIsContentDocument(bool aIsContentDocument)
  {
    mIsContentDocument = aIsContentDocument;
  }

  /**
   * Create an element with the specified name, prefix and namespace ID.
   * Returns null if element name parsing failed.
   */
  already_AddRefed<Element> CreateElem(const nsAString& aName,
                                       nsAtom* aPrefix,
                                       int32_t aNamespaceID,
                                       const nsAString* aIs = nullptr);

  /**
   * Get the security info (i.e. SSL state etc) that the document got
   * from the channel/document that created the content of the
   * document.
   *
   * @see nsIChannel
   */
  nsISupports *GetSecurityInfo()
  {
    return mSecurityInfo;
  }

  /**
   * Get the channel that failed to load and resulted in an error page, if it
   * exists. This is only relevant to error pages.
   */
  nsIChannel* GetFailedChannel() const
  {
    return mFailedChannel;
  }

  /**
   * Set the channel that failed to load and resulted in an error page.
   * This is only relevant to error pages.
   */
  void SetFailedChannel(nsIChannel* aChannel)
  {
    mFailedChannel = aChannel;
  }

  /**
   * Returns the default namespace ID used for elements created in this
   * document.
   */
  int32_t GetDefaultNamespaceID() const
  {
    return mDefaultElementType;
  }

  void DeleteAllProperties();
  void DeleteAllPropertiesFor(nsINode* aNode);

  nsPropertyTable& PropertyTable()
  {
    return mPropertyTable;
  }

  /**
   * Sets the ID used to identify this part of the multipart document
   */
  void SetPartID(uint32_t aID) {
    mPartID = aID;
  }

  /**
   * Return the ID used to identify this part of the multipart document
   */
  uint32_t GetPartID() const {
    return mPartID;
  }

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
  typedef bool (*nsSubDocEnumFunc)(nsIDocument *aDocument, void *aData);
  void EnumerateSubDocuments(nsSubDocEnumFunc aCallback, void *aData);

  /**
   * Collect all the descendant documents for which |aCalback| returns true.
   * The callback function must not mutate any state for the given document.
   */
  typedef bool (*nsDocTestFunc)(const nsIDocument* aDocument);
  void CollectDescendantDocuments(
    nsTArray<nsCOMPtr<nsIDocument>>& aDescendants,
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
   */
  virtual bool CanSavePresentation(nsIRequest* aNewRequest);

  /**
   * Notify the document that its associated ContentViewer is being destroyed.
   * This releases circular references so that the document can go away.
   * Destroy() is only called on documents that have a content viewer.
   */
  virtual void Destroy() = 0;

  /**
   * Notify the document that its associated ContentViewer is no longer
   * the current viewer for the docshell. The document might still
   * be rendered in "zombie state" until the next document is ready.
   * The document should save form control state.
   */
  virtual void RemovedFromDocShell() = 0;

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
  virtual void BlockOnload() = 0;
  /**
   * @param aFireSync whether to fire onload synchronously.  If false,
   * onload will fire asynchronously after all onload blocks have been
   * removed.  It will NOT fire from inside UnblockOnload.  If true,
   * onload may fire from inside UnblockOnload.
   */
  virtual void UnblockOnload(bool aFireSync) = 0;

  void BlockDOMContentLoaded()
  {
    ++mBlockDOMContentLoaded;
  }

  void UnblockDOMContentLoaded();

  /**
   * Notification that the page has been shown, for documents which are loaded
   * into a DOM window.  This corresponds to the completion of document load,
   * or to the page's presentation being restored into an existing DOM window.
   * This notification fires applicable DOM events to the content window.  See
   * PageTransitionEvent.webidl for a description of the |aPersisted|
   * parameter. If aDispatchStartTarget is null, the pageshow event is
   * dispatched on the ScriptGlobalObject for this document, otherwise it's
   * dispatched on aDispatchStartTarget.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  virtual void OnPageShow(bool aPersisted,
                          mozilla::dom::EventTarget* aDispatchStartTarget);

  /**
   * Notification that the page has been hidden, for documents which are loaded
   * into a DOM window.  This corresponds to the unloading of the document, or
   * to the document's presentation being saved but removed from an existing
   * DOM window.  This notification fires applicable DOM events to the content
   * window.  See PageTransitionEvent.webidl for a description of the
   * |aPersisted| parameter. If aDispatchStartTarget is null, the pagehide
   * event is dispatched on the ScriptGlobalObject for this document,
   * otherwise it's dispatched on aDispatchStartTarget.
   * Note: if aDispatchStartTarget isn't null, the showing state of the
   * document won't be altered.
   */
  void OnPageHide(bool aPersisted,
                  mozilla::dom::EventTarget* aDispatchStartTarget);

  /*
   * We record the set of links in the document that are relevant to
   * style.
   */
  /**
   * Notification that an element is a link that is relevant to style.
   */
  void AddStyleRelevantLink(mozilla::dom::Link* aLink)
  {
    NS_ASSERTION(aLink, "Passing in a null link.  Expect crashes RSN!");
#ifdef DEBUG
    nsPtrHashKey<mozilla::dom::Link>* entry = mStyledLinks.GetEntry(aLink);
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
  void ForgetLink(mozilla::dom::Link* aLink)
  {
    NS_ASSERTION(aLink, "Passing in a null link.  Expect crashes RSN!");
#ifdef DEBUG
    nsPtrHashKey<mozilla::dom::Link>* entry = mStyledLinks.GetEntry(aLink);
    NS_ASSERTION(entry || mStyledLinksCleared,
                 "Document knows nothing about this Link!");
#endif
    mStyledLinks.RemoveEntry(aLink);
  }

  // Refreshes the hrefs of all the links in the document.
  void RefreshLinkHrefs();

  /**
   * Resets and removes a box object from the document's box object cache
   *
   * @param aElement canonical nsIContent pointer of the box object's element
   */
  void ClearBoxObjectFor(nsIContent* aContent);

  /**
   * Get the box object for an element. This is not exposed through a
   * scriptable interface except for XUL documents.
   */
  already_AddRefed<mozilla::dom::BoxObject>
    GetBoxObjectFor(mozilla::dom::Element* aElement, mozilla::ErrorResult& aRv);

  /**
   * Support for window.matchMedia()
   */

  already_AddRefed<mozilla::dom::MediaQueryList>
    MatchMedia(const nsAString& aMediaQueryList,
               mozilla::dom::CallerType aCallerType);

  mozilla::LinkedList<mozilla::dom::MediaQueryList>& MediaQueryLists() {
    return mDOMMediaQueryLists;
  }

  /**
   * Get the compatibility mode for this document
   */
  nsCompatibility GetCompatibilityMode() const {
    return mCompatMode;
  }

  /**
   * Check whether we've ever fired a DOMTitleChanged event for this
   * document.
   */
  bool HaveFiredDOMTitleChange() const
  {
    return mHaveFiredTitleChange;
  }

  Element* GetAnonymousElementByAttribute(nsIContent* aElement,
                                          nsAtom* aAttrName,
                                          const nsAString& aAttrValue) const;

  nsresult NodesFromRectHelper(float aX, float aY,
                               float aTopSize, float aRightSize,
                               float aBottomSize, float aLeftSize,
                               bool aIgnoreRootScrollFrame,
                               bool aFlushLayout,
                               nsINodeList** aReturn);

  /**
   * See FlushSkinBindings on nsBindingManager
   */
  void FlushSkinBindings();

  /**
   * To batch DOMSubtreeModified, document needs to be informed when
   * a mutation event might be dispatched, even if the event isn't actually
   * created because there are no listeners for it.
   *
   * @param aTarget is the target for the mutation event.
   */
  void MayDispatchMutationEvent(nsINode* aTarget)
  {
    if (mSubtreeModifiedDepth > 0) {
      mSubtreeModifiedTargets.AppendObject(aTarget);
    }
  }

  /**
   * Marks as not-going-to-be-collected for the given generation of
   * cycle collection.
   */
  void MarkUncollectableForCCGeneration(uint32_t aGeneration)
  {
    mMarkedCCGeneration = aGeneration;
  }

  /**
   * Gets the cycle collector generation this document is marked for.
   */
  uint32_t GetMarkedCCGeneration()
  {
    return mMarkedCCGeneration;
  }

  /**
   * Returns whether this document is cookie averse. See
   * https://html.spec.whatwg.org/multipage/dom.html#cookie-averse-document-object
   */
  bool IsCookieAverse() const
  {
    // If we are a document that "has no browsing context."
    if (!GetInnerWindow()) {
      return true;
    }

    // If we are a document "whose URL's scheme is not a network scheme."
    // NB: Explicitly allow file: URIs to store cookies.
    nsCOMPtr<nsIURI> codebaseURI;
    NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));

    if (!codebaseURI) {
      return true;
    }

    nsAutoCString scheme;
    codebaseURI->GetScheme(scheme);
    return !scheme.EqualsLiteral("http") &&
           !scheme.EqualsLiteral("https") &&
           !scheme.EqualsLiteral("ftp") &&
           !scheme.EqualsLiteral("file");
  }

  bool IsLoadedAsData()
  {
    return mLoadedAsData;
  }

  bool IsLoadedAsInteractiveData()
  {
    return mLoadedAsInteractiveData;
  }

  bool MayStartLayout()
  {
    return mMayStartLayout;
  }

  virtual void SetMayStartLayout(bool aMayStartLayout);

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
  bool IsRootDisplayDocument() const
  {
    return !mParentDocument && !mDisplayDocument;
  }

  bool IsBeingUsedAsImage() const {
    return mIsBeingUsedAsImage;
  }

  void SetIsBeingUsedAsImage() {
    mIsBeingUsedAsImage = true;
  }

  bool IsSVGGlyphsDocument() const
  {
    return mIsSVGGlyphsDocument;
  }

  void SetIsSVGGlyphsDocument()
  {
    mIsSVGGlyphsDocument = true;
  }

  bool IsResourceDoc() const {
    return IsBeingUsedAsImage() || // Are we a helper-doc for an SVG image?
      mHasDisplayDocument;         // Are we an external resource doc?
  }

  /**
   * Get the document for which this document is an external resource.  This
   * will be null if this document is not an external resource.  Otherwise,
   * GetDisplayDocument() will return a non-null document, and
   * GetDisplayDocument()->GetDisplayDocument() is guaranteed to be null.
   */
  nsIDocument* GetDisplayDocument() const
  {
    return mDisplayDocument;
  }

  /**
   * Set the display document for this document.  aDisplayDocument must not be
   * null.
   */
  void SetDisplayDocument(nsIDocument* aDisplayDocument)
  {
    MOZ_ASSERT(!GetShell() &&
               !GetContainer() &&
               !GetWindow(),
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
   * @param aRequestingNode the node making the request
   * @param aPendingLoad the pending load for this request, if any
   */
  nsIDocument* RequestExternalResource(nsIURI* aURI,
                                       nsINode* aRequestingNode,
                                       ExternalResourceLoad** aPendingLoad);

  /**
   * Enumerate the external resource documents associated with this document.
   * The enumerator callback should return true to continue enumerating, or
   * false to stop.  This callback will never get passed a null aDocument.
   */
  void EnumerateExternalResources(nsSubDocEnumFunc aCallback, void* aData);

  nsExternalResourceMap& ExternalResourceMap()
  {
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
   * Return whether the document and all its ancestors are visible in the sense of
   * pageshow / hide.
   */
  bool IsVisibleConsideringAncestors() const;

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
  bool IsCurrentActiveDocument() const
  {
    nsPIDOMWindowInner* inner = GetInnerWindow();
    return inner && inner->IsCurrentInnerWindow() && inner->GetDoc() == this;
  }

  /**
   * Returns whether this document should perform image loads.
   */
  bool ShouldLoadImages() const
  {
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
  typedef void (* ActivityObserverEnumerator)(nsISupports*, void*);
  void EnumerateActivityObservers(ActivityObserverEnumerator aEnumerator,
                                  void* aData);

  // Indicates whether mAnimationController has been (lazily) initialized.
  // If this returns true, we're promising that GetAnimationController()
  // will have a non-null return value.
  bool HasAnimationController()  { return !!mAnimationController; }

  // Getter for this document's SMIL Animation Controller. Performs lazy
  // initialization, if this document supports animation and if
  // mAnimationController isn't yet initialized.
  //
  // If HasAnimationController is true, this is guaranteed to return non-null.
  nsSMILAnimationController* GetAnimationController();

  // Gets the tracker for animations that are waiting to start.
  // Returns nullptr if there is no pending animation tracker for this document
  // which will be the case if there have never been any CSS animations or
  // transitions on elements in the document.
  mozilla::PendingAnimationTracker* GetPendingAnimationTracker()
  {
    return mPendingAnimationTracker;
  }

  // Gets the tracker for animations that are waiting to start and
  // creates it if it doesn't already exist. As a result, the return value
  // will never be nullptr.
  mozilla::PendingAnimationTracker* GetOrCreatePendingAnimationTracker();

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

  void DecreaseEventSuppression()
  {
    MOZ_ASSERT(mEventsSuppressed);
    --mEventsSuppressed;
    UpdateFrameRequestCallbackSchedulingState();
  }

  /**
   * Increment https://html.spec.whatwg.org/#ignore-destructive-writes-counter
   */
  void IncrementIgnoreDestructiveWritesCounter() { ++mIgnoreDestructiveWritesCounter; }

  /**
   * Decrement https://html.spec.whatwg.org/#ignore-destructive-writes-counter
   */
  void DecrementIgnoreDestructiveWritesCounter() { --mIgnoreDestructiveWritesCounter; }

  bool IsDNSPrefetchAllowed() const { return mAllowDNSPrefetch; }

  /**
   * Returns true if this document is allowed to contain XUL element and
   * use non-builtin XBL bindings.
   */
  bool AllowXULXBL() {
    return mAllowXULXBL == eTriTrue ? true :
           mAllowXULXBL == eTriFalse ? false :
           InternalAllowXULXBL();
  }

  void ForceEnableXULXBL() {
    mAllowXULXBL = eTriTrue;
  }

  /**
   * Returns the template content owner document that owns the content of
   * HTMLTemplateElement.
   */
  nsIDocument* GetTemplateContentsOwner();

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
  virtual already_AddRefed<nsIDocument>
  CreateStaticClone(nsIDocShell* aCloneContainer);

  /**
   * If this document is a static clone, this returns the original
   * document.
   */
  nsIDocument* GetOriginalDocument()
  {
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

  void PreloadPictureOpened()
  {
    mPreloadPictureDepth++;
  }

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
  already_AddRefed<nsIURI>
  ResolvePreloadImage(nsIURI *aBaseURI,
                      const nsAString& aSrcAttr,
                      const nsAString& aSrcsetAttr,
                      const nsAString& aSizesAttr,
                      bool *aIsImgSet);
  /**
   * Called by nsParser to preload images. Can be removed and code moved
   * to nsPreloadURIs::PreloadURIs() in file nsParser.cpp whenever the
   * parser-module is linked with gklayout-module.  aCrossOriginAttr should
   * be a void string if the attr is not present.
   * aIsImgSet is the value got from calling ResolvePreloadImage, it is true
   * when this image is for loading <picture> or <img srcset> images.
   */
  void MaybePreLoadImage(nsIURI* uri,
                         const nsAString& aCrossOriginAttr,
                         ReferrerPolicyEnum aReferrerPolicy,
                         bool aIsImgSet);

  /**
   * Called by images to forget an image preload when they start doing
   * the real load.
   */
  void ForgetImagePreload(nsIURI* aURI);

  /**
   * Called by nsParser to preload style sheets.  Can also be merged into the
   * parser if and when the parser is merged with libgklayout.  aCrossOriginAttr
   * should be a void string if the attr is not present.
   */
  void PreloadStyle(nsIURI* aURI,
                    const mozilla::Encoding* aEncoding,
                    const nsAString& aCrossOriginAttr,
                    ReferrerPolicyEnum aReferrerPolicy,
                    const nsAString& aIntegrity);

  /**
   * Called by the chrome registry to load style sheets.  Can be put
   * back there if and when when that module is merged with libgklayout.
   *
   * This always does a synchronous load.  If aIsAgentSheet is true,
   * it also uses the system principal and enables unsafe rules.
   * DO NOT USE FOR UNTRUSTED CONTENT.
   */
  nsresult LoadChromeSheetSync(nsIURI* aURI, bool aIsAgentSheet,
                               RefPtr<mozilla::StyleSheet>* aSheet);

  /**
   * Returns true if the locale used for the document specifies a direction of
   * right to left. For chrome documents, this comes from the chrome registry.
   * This is used to determine the current state for the :-moz-locale-dir pseudoclass
   * so once can know whether a document is expected to be rendered left-to-right
   * or right-to-left.
   */
  virtual bool IsDocumentRightToLeft() { return false; }

  /**
   * Called by Parser for link rel=preconnect
   */
  void MaybePreconnect(nsIURI* uri, mozilla::CORSMode aCORSMode);

  enum DocumentTheme {
    Doc_Theme_Uninitialized, // not determined yet
    Doc_Theme_None,
    Doc_Theme_Neutral,
    Doc_Theme_Dark,
    Doc_Theme_Bright
  };

  /**
   * Set the document's pending state object (as serialized using structured
   * clone).
   */
  void SetStateObject(nsIStructuredCloneContainer *scContainer);

  /**
   * Returns Doc_Theme_None if there is no lightweight theme specified,
   * Doc_Theme_Dark for a dark theme, Doc_Theme_Bright for a light theme, and
   * Doc_Theme_Neutral for any other theme. This is used to determine the state
   * of the pseudoclasses :-moz-lwtheme and :-moz-lwtheme-text.
   */
  virtual DocumentTheme GetDocumentLWTheme() { return Doc_Theme_None; }
  virtual DocumentTheme ThreadSafeGetDocumentLWTheme() const { return Doc_Theme_None; }

  /**
   * Returns the document state.
   * Document state bits have the form NS_DOCUMENT_STATE_* and are declared in
   * nsIDocument.h.
   */
  mozilla::EventStates GetDocumentState() const
  {
    return mDocumentState;
  }

  nsISupports* GetCurrentContentSink();

  void SetAutoFocusElement(Element* aAutoFocusElement);
  void TriggerAutoFocus();

  void SetScrollToRef(nsIURI* aDocumentURI);
  void ScrollToRef();
  void ResetScrolledToRefAlready()
  {
    mScrolledToRefAlready = false;
  }

  void SetChangeScrollPosWhenScrollingToRef(bool aValue)
  {
    mChangeScrollPosWhenScrollingToRef = aValue;
  }

  using mozilla::dom::DocumentOrShadowRoot::GetElementById;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagName;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByTagNameNS;
  using mozilla::dom::DocumentOrShadowRoot::GetElementsByClassName;

  /**
   * Lookup an image element using its associated ID, which is usually provided
   * by |-moz-element()|. Similar to GetElementById, with the difference that
   * elements set using mozSetImageElement have higher priority.
   * @param aId the ID associated the element we want to lookup
   * @return the element associated with |aId|
   */
  Element* LookupImageElement(const nsAString& aElementId);

  mozilla::dom::DocumentTimeline* Timeline();
  mozilla::LinkedList<mozilla::dom::DocumentTimeline>& Timelines()
  {
    return mTimelines;
  }

  void GetAnimations(nsTArray<RefPtr<mozilla::dom::Animation>>& aAnimations);

  mozilla::dom::SVGSVGElement* GetSVGRootElement() const;

  nsresult ScheduleFrameRequestCallback(mozilla::dom::FrameRequestCallback& aCallback,
                                        int32_t *aHandle);
  void CancelFrameRequestCallback(int32_t aHandle);

  typedef nsTArray<RefPtr<mozilla::dom::FrameRequestCallback>> FrameRequestCallbackList;
  /**
   * Put this document's frame request callbacks into the provided
   * list, and forget about them.
   */
  void TakeFrameRequestCallbacks(FrameRequestCallbackList& aCallbacks);

  /**
   * @return true if this document's frame request callbacks should be
   * throttled. We throttle requestAnimationFrame for documents which aren't
   * visible (e.g. scrolled out of the viewport).
   */
  bool ShouldThrottleFrameRequests();

  // This returns true when the document tree is being teared down.
  bool InUnlinkOrDeletion() { return mInUnlinkOrDeletion; }

  mozilla::dom::ImageTracker* ImageTracker();

  // AddPlugin adds a plugin-related element to mPlugins when the element is
  // added to the tree.
  void AddPlugin(nsIObjectLoadingContent* aPlugin)
  {
    MOZ_ASSERT(aPlugin);
    mPlugins.PutEntry(aPlugin);
  }

  // RemovePlugin removes a plugin-related element to mPlugins when the
  // element is removed from the tree.
  void RemovePlugin(nsIObjectLoadingContent* aPlugin)
  {
    MOZ_ASSERT(aPlugin);
    mPlugins.RemoveEntry(aPlugin);
  }

  // GetPlugins returns the plugin-related elements from
  // the frame and any subframes.
  void GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins);

  // Adds an element to mResponsiveContent when the element is
  // added to the tree.
  void AddResponsiveContent(mozilla::dom::HTMLImageElement* aContent)
  {
    MOZ_ASSERT(aContent);
    mResponsiveContent.PutEntry(aContent);
  }

  // Removes an element from mResponsiveContent when the element is
  // removed from the tree.
  void RemoveResponsiveContent(mozilla::dom::HTMLImageElement* aContent)
  {
    MOZ_ASSERT(aContent);
    mResponsiveContent.RemoveEntry(aContent);
  }

  void AddComposedDocShadowRoot(mozilla::dom::ShadowRoot& aShadowRoot)
  {
    mComposedShadowRoots.PutEntry(&aShadowRoot);
  }

  using ShadowRootSet = nsTHashtable<nsPtrHashKey<mozilla::dom::ShadowRoot>>;

  void RemoveComposedDocShadowRoot(mozilla::dom::ShadowRoot& aShadowRoot)
  {
    mComposedShadowRoots.RemoveEntry(&aShadowRoot);
  }

  // If you're considering using this, you probably want to use
  // ShadowRoot::IsComposedDocParticipant instead. This is just for
  // sanity-checking.
  bool IsComposedDocShadowRoot(mozilla::dom::ShadowRoot& aShadowRoot)
  {
    return mComposedShadowRoots.Contains(&aShadowRoot);
  }

  const ShadowRootSet& ComposedShadowRoots() const
  {
    return mComposedShadowRoots;
  }

  // Notifies any responsive content added by AddResponsiveContent upon media
  // features values changing.
  void NotifyMediaFeatureValuesChanged();

  nsresult GetStateObject(nsIVariant** aResult);

  nsDOMNavigationTiming* GetNavigationTiming() const
  {
    return mTiming;
  }

  void SetNavigationTiming(nsDOMNavigationTiming* aTiming);

  nsContentList* ImageMapList();

  // Add aLink to the set of links that need their status resolved.
  void RegisterPendingLinkUpdate(mozilla::dom::Link* aLink);

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
  void WarnOnceAbout(DocumentWarnings aWarning,
                     bool asError = false,
                     const char16_t **aParams = nullptr,
                     uint32_t aParamsLength = 0) const;

  // Posts an event to call UpdateVisibilityState
  void PostVisibilityUpdateEvent();

  bool IsSyntheticDocument() const { return mIsSyntheticDocument; }

  // Note: nsIDocument is a sub-class of nsINode, which has a
  // SizeOfExcludingThis function.  However, because nsIDocument objects can
  // only appear at the top of the DOM tree, we have a specialized measurement
  // function which returns multiple sizes.
  virtual void DocAddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const;
  // DocAddSizeOfIncludingThis doesn't need to be overridden by sub-classes
  // because nsIDocument inherits from nsINode;  see the comment above the
  // declaration of nsINode::SizeOfIncludingThis.
  virtual void DocAddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const;

  bool MayHaveDOMMutationObservers()
  {
    return mMayHaveDOMMutationObservers;
  }

  void SetMayHaveDOMMutationObservers()
  {
    mMayHaveDOMMutationObservers = true;
  }

  bool MayHaveAnimationObservers()
  {
    return mMayHaveAnimationObservers;
  }

  void SetMayHaveAnimationObservers()
  {
    mMayHaveAnimationObservers = true;
  }

  bool IsInSyncOperation()
  {
    return mInSyncOperationCount != 0;
  }

  void SetIsInSyncOperation(bool aSync)
  {
    if (aSync) {
      ++mInSyncOperationCount;
    } else {
      --mInSyncOperationCount;
    }
  }

  bool CreatingStaticClone() const
  {
    return mCreatingStaticClone;
  }

  /**
   * Creates a new element in the HTML namespace with a local name given by
   * aTag.
   */
  already_AddRefed<Element> CreateHTMLElement(nsAtom* aTag);

  // WebIDL API
  nsIGlobalObject* GetParentObject() const
  {
    return GetScopeObject();
  }
  static already_AddRefed<nsIDocument>
    Constructor(const GlobalObject& aGlobal,
                mozilla::ErrorResult& rv);
  mozilla::dom::DOMImplementation* GetImplementation(mozilla::ErrorResult& rv);
  MOZ_MUST_USE nsresult GetURL(nsString& retval) const;
  MOZ_MUST_USE nsresult GetDocumentURI(nsString& retval) const;
  // Return the URI for the document.
  // The returned value may differ if the document is loaded via XHR, and
  // when accessed from chrome privileged script and
  // from content privileged script for compatibility.
  void GetDocumentURIFromJS(nsString& aDocumentURI,
                            mozilla::dom::CallerType aCallerType,
                            mozilla::ErrorResult& aRv) const;
  void GetCompatMode(nsString& retval) const;
  void GetCharacterSet(nsAString& retval) const;
  // Skip GetContentType, because our NS_IMETHOD version above works fine here.
  // GetDoctype defined above
  Element* GetDocumentElement() const
  {
    return GetRootElement();
  }

  enum ElementCallbackType {
    eConnected,
    eDisconnected,
    eAdopted,
    eAttributeChanged
  };

  nsIDocument* GetTopLevelContentDocument();

  // Returns the associated XUL window if this is a top-level chrome document,
  // null otherwise.
  already_AddRefed<nsIXULWindow> GetXULWindowIfToplevelChrome() const;

  already_AddRefed<Element>
  CreateElement(const nsAString& aTagName,
                const mozilla::dom::ElementCreationOptionsOrString& aOptions,
                mozilla::ErrorResult& rv);
  already_AddRefed<Element>
  CreateElementNS(const nsAString& aNamespaceURI,
                  const nsAString& aQualifiedName,
                  const mozilla::dom::ElementCreationOptionsOrString& aOptions,
                  mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::DocumentFragment>
    CreateDocumentFragment() const;
  already_AddRefed<nsTextNode> CreateTextNode(const nsAString& aData) const;
  already_AddRefed<nsTextNode> CreateEmptyTextNode() const;
  already_AddRefed<mozilla::dom::Comment>
    CreateComment(const nsAString& aData) const;
  already_AddRefed<mozilla::dom::ProcessingInstruction>
    CreateProcessingInstruction(const nsAString& target, const nsAString& data,
                                mozilla::ErrorResult& rv) const;
  already_AddRefed<nsINode>
    ImportNode(nsINode& aNode, bool aDeep, mozilla::ErrorResult& rv) const;
  nsINode* AdoptNode(nsINode& aNode, mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::Event>
    CreateEvent(const nsAString& aEventType,
                mozilla::dom::CallerType aCallerType,
                mozilla::ErrorResult& rv) const;
  already_AddRefed<nsRange> CreateRange(mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::NodeIterator>
    CreateNodeIterator(nsINode& aRoot, uint32_t aWhatToShow,
                       mozilla::dom::NodeFilter* aFilter,
                       mozilla::ErrorResult& rv) const;
  already_AddRefed<mozilla::dom::TreeWalker>
    CreateTreeWalker(nsINode& aRoot, uint32_t aWhatToShow,
                     mozilla::dom::NodeFilter* aFilter, mozilla::ErrorResult& rv) const;
  // Deprecated WebIDL bits
  already_AddRefed<mozilla::dom::CDATASection>
    CreateCDATASection(const nsAString& aData, mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::Attr>
    CreateAttribute(const nsAString& aName, mozilla::ErrorResult& rv);
  already_AddRefed<mozilla::dom::Attr>
    CreateAttributeNS(const nsAString& aNamespaceURI,
                      const nsAString& aQualifiedName,
                      mozilla::ErrorResult& rv);
  void GetInputEncoding(nsAString& aInputEncoding) const;
  already_AddRefed<mozilla::dom::Location> GetLocation() const;
  void GetReferrer(nsAString& aReferrer) const;
  void GetLastModified(nsAString& aLastModified) const;
  void GetReadyState(nsAString& aReadyState) const;

  void GetTitle(nsAString& aTitle);
  void SetTitle(const nsAString& aTitle, mozilla::ErrorResult& rv);
  void GetDir(nsAString& aDirection) const;
  void SetDir(const nsAString& aDirection);
  nsIHTMLCollection* Images();
  nsIHTMLCollection* Embeds();
  nsIHTMLCollection* Plugins()
  {
    return Embeds();
  }
  nsIHTMLCollection* Links();
  nsIHTMLCollection* Forms();
  nsIHTMLCollection* Scripts();
  already_AddRefed<nsContentList> GetElementsByName(const nsAString& aName)
  {
    return GetFuncStringContentList<nsCachableElementsByNameNodeList>(this,
                                                                      MatchNameAttribute,
                                                                      nullptr,
                                                                      UseExistingNameString,
                                                                      aName);
  }
  nsPIDOMWindowOuter* GetDefaultView() const
  {
    return GetWindow();
  }
  Element* GetActiveElement();
  bool HasFocus(mozilla::ErrorResult& rv) const;
  nsIHTMLCollection* Applets();
  nsIHTMLCollection* Anchors();
  mozilla::TimeStamp LastFocusTime() const;
  void SetLastFocusTime(const mozilla::TimeStamp& aFocusTime);
  // Event handlers are all on nsINode already
  bool MozSyntheticDocument() const
  {
    return IsSyntheticDocument();
  }
  Element* GetCurrentScript();
  void ReleaseCapture() const;
  void MozSetImageElement(const nsAString& aImageElementId, Element* aElement);
  nsIURI* GetDocumentURIObject() const;
  // Not const because all the full-screen goop is not const
  bool FullscreenEnabled(mozilla::dom::CallerType aCallerType);
  Element* FullScreenStackTop();
  bool Fullscreen()
  {
    return !!GetFullscreenElement();
  }
  void ExitFullscreen();
  void ExitPointerLock()
  {
    UnlockPointer(this);
  }

  static bool IsUnprefixedFullscreenEnabled(JSContext* aCx, JSObject* aObject);

#ifdef MOZILLA_INTERNAL_API
  bool Hidden() const
  {
    return mVisibilityState != mozilla::dom::VisibilityState::Visible;
  }
  mozilla::dom::VisibilityState VisibilityState() const
  {
    return mVisibilityState;
  }
#endif
  void GetSelectedStyleSheetSet(nsAString& aSheetSet);
  void SetSelectedStyleSheetSet(const nsAString& aSheetSet);
  void GetLastStyleSheetSet(nsAString& aSheetSet)
  {
    aSheetSet = mLastStyleSheetSet;
  }
  const nsString& GetCurrentStyleSheetSet() const
  {
    return mLastStyleSheetSet.IsEmpty()
      ? mPreferredStyleSheetSet
      : mLastStyleSheetSet;
  }
  void SetPreferredStyleSheetSet(const nsAString&);
  void GetPreferredStyleSheetSet(nsAString& aSheetSet)
  {
    aSheetSet = mPreferredStyleSheetSet;
  }
  mozilla::dom::DOMStringList* StyleSheetSets();
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
  already_AddRefed<nsDOMCaretPosition>
    CaretPositionFromPoint(float aX, float aY);

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
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& rv);
  mozilla::dom::XPathExpression*
    CreateExpression(const nsAString& aExpression,
                     mozilla::dom::XPathNSResolver* aResolver,
                     mozilla::ErrorResult& rv);
  nsINode* CreateNSResolver(nsINode& aNodeResolver);
  already_AddRefed<mozilla::dom::XPathResult>
    Evaluate(JSContext* aCx, const nsAString& aExpression, nsINode& aContextNode,
             mozilla::dom::XPathNSResolver* aResolver, uint16_t aType,
             JS::Handle<JSObject*> aResult, mozilla::ErrorResult& rv);
  // Touch event handlers already on nsINode
  already_AddRefed<mozilla::dom::Touch>
    CreateTouch(nsGlobalWindowInner* aView, mozilla::dom::EventTarget* aTarget,
                int32_t aIdentifier, int32_t aPageX, int32_t aPageY,
                int32_t aScreenX, int32_t aScreenY, int32_t aClientX,
                int32_t aClientY, int32_t aRadiusX, int32_t aRadiusY,
                float aRotationAngle, float aForce);
  already_AddRefed<mozilla::dom::TouchList> CreateTouchList();
  already_AddRefed<mozilla::dom::TouchList>
    CreateTouchList(mozilla::dom::Touch& aTouch,
                    const mozilla::dom::Sequence<mozilla::OwningNonNull<mozilla::dom::Touch> >& aTouches);
  already_AddRefed<mozilla::dom::TouchList>
    CreateTouchList(const mozilla::dom::Sequence<mozilla::OwningNonNull<mozilla::dom::Touch> >& aTouches);

  void SetStyleSheetChangeEventsEnabled(bool aValue)
  {
    mStyleSheetChangeEventsEnabled = aValue;
  }

  bool StyleSheetChangeEventsEnabled() const
  {
    return mStyleSheetChangeEventsEnabled;
  }

  void ObsoleteSheet(nsIURI *aSheetURI, mozilla::ErrorResult& rv);

  void ObsoleteSheet(const nsAString& aSheetURI, mozilla::ErrorResult& rv);

  already_AddRefed<mozilla::dom::Promise> BlockParsing(mozilla::dom::Promise& aPromise,
                                                       const mozilla::dom::BlockParsingOptions& aOptions,
                                                       mozilla::ErrorResult& aRv);

  already_AddRefed<nsIURI> GetMozDocumentURIIfNotForErrorPages();

  mozilla::dom::Promise* GetDocumentReadyForIdle(mozilla::ErrorResult& aRv);

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
  inline mozilla::dom::SVGDocument* AsSVGDocument();

  /**
   * Asserts IsXULDocument, and can't return null.
   * Defined inline in XULDocument.h
   */
  inline mozilla::dom::XULDocument* AsXULDocument();

  /*
   * Given a node, get a weak reference to it and append that reference to
   * mBlockedTrackingNodes. Can be used later on to look up a node in it.
   * (e.g., by the UI)
   */
  void AddBlockedTrackingNode(nsINode *node)
  {
    if (!node) {
      return;
    }

    nsWeakPtr weakNode = do_GetWeakReference(node);

    if (weakNode) {
      mBlockedTrackingNodes.AppendElement(weakNode);
    }
  }

  gfxUserFontSet* GetUserFontSet(bool aFlushUserFontSet = true);
  void FlushUserFontSet();
  void MarkUserFontSetDirty();
  mozilla::dom::FontFaceSet* GetFonts() { return mFontFaceSet; }

  // FontFaceSource
  mozilla::dom::FontFaceSet* Fonts();

  bool DidFireDOMContentLoaded() const { return mDidFireDOMContentLoaded; }

  bool IsSynthesized();

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

  void SetDocumentUseCounter(mozilla::UseCounter aUseCounter)
  {
    if (!mUseCounters[aUseCounter]) {
      mUseCounters[aUseCounter] = true;
    }
  }

  void SetPageUseCounter(mozilla::UseCounter aUseCounter);

  void SetDocumentAndPageUseCounter(mozilla::UseCounter aUseCounter)
  {
    SetDocumentUseCounter(aUseCounter);
    SetPageUseCounter(aUseCounter);
  }

  void PropagateUseCounters(nsIDocument* aParentDocument);

  // Called to track whether this document has had any interaction.
  // This is used to track whether we should permit "beforeunload".
  void SetUserHasInteracted(bool aUserHasInteracted);
  bool UserHasInteracted()
  {
    return mUserHasInteracted;
  }

  // This should be called when this document receives events which are likely
  // to be user interaction with the document, rather than the byproduct of
  // interaction with the browser (i.e. a keypress to scroll the view port,
  // keyboard shortcuts, etc). This is used to decide whether we should
  // permit autoplay audible media. This also gesture activates all other
  // content documents in this tab.
  void NotifyUserGestureActivation();

  // Return true if NotifyUserGestureActivation() has been called on any
  // document in the document tree.
  bool HasBeenUserGestureActivated();

  bool HasScriptsBlockedBySandbox();

  bool InlineScriptAllowedByCSP();

  void ReportHasScrollLinkedEffect();
  bool HasScrollLinkedEffect() const
  {
    return mHasScrollLinkedEffect;
  }

  mozilla::dom::DocGroup* GetDocGroup() const;

  void AddIntersectionObserver(mozilla::dom::DOMIntersectionObserver* aObserver)
  {
    MOZ_ASSERT(!mIntersectionObservers.Contains(aObserver),
               "Intersection observer already in the list");
    mIntersectionObservers.PutEntry(aObserver);
  }

  void RemoveIntersectionObserver(mozilla::dom::DOMIntersectionObserver* aObserver)
  {
    mIntersectionObservers.RemoveEntry(aObserver);
  }

  bool HasIntersectionObservers() const
  {
    return !mIntersectionObservers.IsEmpty();
  }

  void UpdateIntersectionObservations();
  void ScheduleIntersectionObserverNotification();
  void NotifyIntersectionObservers();

  // Dispatch a runnable related to the document.
  nsresult Dispatch(mozilla::TaskCategory aCategory,
                    already_AddRefed<nsIRunnable>&& aRunnable) final;

  virtual nsISerialEventTarget*
  EventTargetFor(mozilla::TaskCategory aCategory) const override;

  virtual mozilla::AbstractThread*
  AbstractMainThreadFor(mozilla::TaskCategory aCategory) override;

  // The URLs passed to these functions should match what
  // JS::DescribeScriptedCaller() returns, since these APIs are used to
  // determine whether some code is being called from a tracking script.
  void NoteScriptTrackingStatus(const nsACString& aURL, bool isTracking);
  bool IsScriptTracking(const nsACString& aURL) const;

  // For more information on Flash classification, see
  // toolkit/components/url-classifier/flash-block-lists.rst
  mozilla::dom::FlashClassification DocumentFlashClassification();
  bool IsThirdParty();

  bool IsScopedStyleEnabled();

  nsINode* GetServoRestyleRoot() const
  {
    return mServoRestyleRoot;
  }

  uint32_t GetServoRestyleRootDirtyBits() const
  {
    MOZ_ASSERT(mServoRestyleRoot);
    MOZ_ASSERT(mServoRestyleRootDirtyBits);
    return mServoRestyleRootDirtyBits;
  }

  void ClearServoRestyleRoot()
  {
    mServoRestyleRoot = nullptr;
    mServoRestyleRootDirtyBits = 0;
  }

  inline void SetServoRestyleRoot(nsINode* aRoot, uint32_t aDirtyBits);
  inline void SetServoRestyleRootDirtyBits(uint32_t aDirtyBits);

  bool ShouldThrowOnDynamicMarkupInsertion()
  {
    return mThrowOnDynamicMarkupInsertionCounter;
  }

  void IncrementThrowOnDynamicMarkupInsertionCounter()
  {
    ++mThrowOnDynamicMarkupInsertionCounter;
  }

  void DecrementThrowOnDynamicMarkupInsertionCounter()
  {
    MOZ_ASSERT(mThrowOnDynamicMarkupInsertionCounter);
    --mThrowOnDynamicMarkupInsertionCounter;
  }

  bool ShouldIgnoreOpens() const
  {
    return mIgnoreOpensDuringUnloadCounter;
  }

  void IncrementIgnoreOpensDuringUnloadCounter()
  {
    ++mIgnoreOpensDuringUnloadCounter;
  }

  void DecrementIgnoreOpensDuringUnloadCounter()
  {
    MOZ_ASSERT(mIgnoreOpensDuringUnloadCounter);
    --mIgnoreOpensDuringUnloadCounter;
  }

  bool AllowPaymentRequest() const
  {
    return mAllowPaymentRequest;
  }

  void SetAllowPaymentRequest(bool aAllowPaymentRequest)
  {
    mAllowPaymentRequest = aAllowPaymentRequest;
  }

  bool IsShadowDOMEnabled() const
  {
    return mIsShadowDOMEnabled;
  }

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

protected:
  already_AddRefed<nsIPrincipal> MaybeDowngradePrincipal(nsIPrincipal* aPrincipal);

  void EnsureOnloadBlocker();

  void SendToConsole(nsCOMArray<nsISecurityConsoleMessage>& aMessages);

  // Returns true if the scheme for the url for this document is "about".
  bool IsAboutPage() const;

  bool ContainsEMEContent();
  bool ContainsMSEContent();

  void MaybeInitializeFinalizeFrameLoaders();

  /**
   * Returns the title element of the document as defined by the HTML
   * specification, or null if there isn't one.  For documents whose root
   * element is an <svg:svg>, this is the first <svg:title> element that's a
   * child of the root.  For other documents, it's the first HTML title element
   * in the document.
   */
  Element* GetTitleElement();

  // Retrieves the classification of the Flash plugins in the document based on
  // the classification lists.
  mozilla::dom::FlashClassification PrincipalFlashClassification();

  // Attempts to determine the Flash classification of this page based on the
  // the classification lists and the classification of parent documents.
  mozilla::dom::FlashClassification ComputeFlashClassification();

  void RecordNavigationTiming(ReadyState aReadyState);

  // This method may fire a DOM event; if it does so it will happen
  // synchronously.
  void UpdateVisibilityState();

  // Recomputes the visibility state but doesn't set the new value.
  mozilla::dom::VisibilityState ComputeVisibilityState() const;

  // Since we wouldn't automatically play media from non-visited page, we need
  // to notify window when the page was first visited.
  void MaybeActiveMediaComponents();

  // Apply the fullscreen state to the document, and trigger related
  // events. It returns false if the fullscreen element ready check
  // fails and nothing gets changed.
  bool ApplyFullscreen(const FullscreenRequest& aRequest);

  bool GetUseCounter(mozilla::UseCounter aUseCounter)
  {
    return mUseCounters[aUseCounter];
  }

  void SetChildDocumentUseCounter(mozilla::UseCounter aUseCounter)
  {
    if (!mChildDocumentUseCounters[aUseCounter]) {
      mChildDocumentUseCounters[aUseCounter] = true;
    }
  }

  bool GetChildDocumentUseCounter(mozilla::UseCounter aUseCounter)
  {
    return mChildDocumentUseCounters[aUseCounter];
  }

  void UpdateDocumentStates(mozilla::EventStates);

  void RemoveDocStyleSheetsFromStyleSets();
  void RemoveStyleSheetsFromStyleSets(
      const nsTArray<RefPtr<mozilla::StyleSheet>>& aSheets,
      mozilla::SheetType aType);
  void ResetStylesheetsToURI(nsIURI* aURI);
  void FillStyleSet(mozilla::ServoStyleSet* aStyleSet);
  void AddStyleSheetToStyleSets(mozilla::StyleSheet* aSheet);
  void RemoveStyleSheetFromStyleSets(mozilla::StyleSheet* aSheet);
  void NotifyStyleSheetAdded(mozilla::StyleSheet* aSheet, bool aDocumentSheet);
  void NotifyStyleSheetRemoved(mozilla::StyleSheet* aSheet, bool aDocumentSheet);
  void NotifyStyleSheetApplicableStateChanged();
  // Just like EnableStyleSheetsForSet, but doesn't check whether
  // aSheetSet is null and allows the caller to control whether to set
  // aSheetSet as the preferred set in the CSSLoader.
  void EnableStyleSheetsForSetInternal(const nsAString& aSheetSet,
                                       bool aUpdateCSSLoader);

private:
  mutable std::bitset<eDeprecatedOperationCount> mDeprecationWarnedAbout;
  mutable std::bitset<eDocumentWarningCount> mDocWarningWarnedAbout;

  // Lazy-initialization to have mDocGroup initialized in prior to the
  // SelectorCaches.
  mozilla::UniquePtr<SelectorCache> mSelectorCache;

protected:
  friend class nsDocumentOnStack;

  void IncreaseStackRefCnt()
  {
    ++mStackRefCnt;
  }

  void DecreaseStackRefCnt()
  {
    if (--mStackRefCnt == 0 && mNeedsReleaseAfterStackRefCntRelease) {
      mNeedsReleaseAfterStackRefCntRelease = false;
      NS_RELEASE_THIS();
    }
  }

  ~nsIDocument();

  // Never ever call this. Only call GetWindow!
  nsPIDOMWindowOuter* GetWindowInternal() const;

  // Never ever call this. Only call GetScriptHandlingObject!
  nsIScriptGlobalObject* GetScriptHandlingObjectInternal() const;

  // Never ever call this. Only call AllowXULXBL!
  bool InternalAllowXULXBL();

  /**
   * These methods should be called before and after dispatching
   * a mutation event.
   * To make this easy and painless, use the mozAutoSubtreeModified helper class.
   */
  void WillDispatchMutationEvent(nsINode* aTarget);
  void MutationEventDispatched(nsINode* aTarget);
  friend class mozAutoSubtreeModified;

  virtual Element* GetNameSpaceElement() override
  {
    return GetRootElement();
  }

  void SetContentTypeInternal(const nsACString& aType);

  nsCString GetContentTypeInternal() const
  {
    return mContentType;
  }

  mozilla::dom::XPathEvaluator* XPathEvaluator();

  // Update our frame request callback scheduling state, if needed.  This will
  // schedule or unschedule them, if necessary, and update
  // mFrameRequestCallbacksScheduled.  aOldShell should only be passed when
  // mPresShell is becoming null; in that case it will be used to get hold of
  // the relevant refresh driver.
  void UpdateFrameRequestCallbackSchedulingState(nsIPresShell* aOldShell = nullptr);

  // Helper for GetScrollingElement/IsScrollingElement.
  bool IsPotentiallyScrollable(mozilla::dom::HTMLBodyElement* aBody);

  // Return the same type parent docuement if exists, or return null.
  nsIDocument* GetSameTypeParentDocument();

  void MaybeAllowStorageForOpener();

  // Helpers for GetElementsByName.
  static bool MatchNameAttribute(mozilla::dom::Element* aElement,
                                 int32_t aNamespaceID,
                                 nsAtom* aAtom, void* aData);
  static void* UseExistingNameString(nsINode* aRootNode, const nsString* aName);

  void MaybeResolveReadyForIdle();

  nsCString mReferrer;
  nsString mLastModified;

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mChromeXHRDocURI;
  nsCOMPtr<nsIURI> mDocumentBaseURI;
  nsCOMPtr<nsIURI> mChromeXHRDocBaseURI;

  // A lazily-constructed URL data for style system to resolve URL value.
  RefPtr<mozilla::URLExtraData> mCachedURLData;

  nsWeakPtr mDocumentLoadGroup;

  bool mReferrerPolicySet;
  ReferrerPolicyEnum mReferrerPolicy;

  bool mBlockAllMixedContent;
  bool mBlockAllMixedContentPreloads;
  bool mUpgradeInsecureRequests;
  bool mUpgradeInsecurePreloads;

  mozilla::WeakPtr<nsDocShell> mDocumentContainer;

  NotNull<const Encoding*> mCharacterSet;
  int32_t mCharacterSetSource;

  // This is just a weak pointer; the parent document owns its children.
  nsIDocument* mParentDocument;

  // A reference to the element last returned from GetRootElement().
  mozilla::dom::Element* mCachedRootElement;

  // This is a weak reference, but we hold a strong reference to mNodeInfo,
  // which in turn holds a strong reference to this mNodeInfoManager.
  nsNodeInfoManager* mNodeInfoManager;
  RefPtr<mozilla::css::Loader> mCSSLoader;
  RefPtr<mozilla::css::ImageLoader> mStyleImageLoader;
  RefPtr<nsHTMLStyleSheet> mAttrStyleSheet;
  RefPtr<nsHTMLCSSStyleSheet> mStyleAttrStyleSheet;

  // Tracking for images in the document.
  RefPtr<mozilla::dom::ImageTracker> mImageTracker;

  // A hashtable of ShadowRoots belonging to the composed doc.
  //
  // See ShadowRoot::SetIsComposedDocParticipant.
  ShadowRootSet mComposedShadowRoots;

  // The set of all object, embed, video/audio elements or
  // nsIObjectLoadingContent or nsIDocumentActivity for which this is the owner
  // document. (They might not be in the document.)
  //
  // These are non-owning pointers, the elements are responsible for removing
  // themselves when they go away.
  nsAutoPtr<nsTHashtable<nsPtrHashKey<nsISupports> > > mActivityObservers;

  // A hashtable of styled links keyed by address pointer.
  nsTHashtable<nsPtrHashKey<mozilla::dom::Link>> mStyledLinks;
#ifdef DEBUG
  // Indicates whether mStyledLinks was cleared or not.  This is used to track
  // state so we can provide useful assertions to consumers of ForgetLink and
  // AddStyleRelevantLink.
  bool mStyledLinksCleared;
#endif

  // The array of all links that need their status resolved.  Links must add themselves
  // to this set by calling RegisterPendingLinkUpdate when added to a document.
  static const size_t kSegmentSize = 128;

  typedef mozilla::SegmentedVector<nsCOMPtr<mozilla::dom::Link>,
                                   kSegmentSize,
                                   InfallibleAllocPolicy>
    LinksToUpdateList;

  LinksToUpdateList mLinksToUpdate;

  // SMIL Animation Controller, lazily-initialized in GetAnimationController
  RefPtr<nsSMILAnimationController> mAnimationController;

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
  RefPtr<mozilla::dom::FontFaceSet> mFontFaceSet;

  // Last time this document or a one of its sub-documents was focused.  If
  // focus has never occurred then mLastFocusTime.IsNull() will be true.
  mozilla::TimeStamp mLastFocusTime;

  mozilla::EventStates mDocumentState;

  RefPtr<mozilla::dom::Promise> mReadyForIdle;

  // True if BIDI is enabled.
  bool mBidiEnabled : 1;
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
  // flag here so that we can check it in nsDocument::GetAnimationController.
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

  // True is this document is synthetic : stand alone image, video, audio
  // file, etc.
  bool mIsSyntheticDocument : 1;

  // True is there is a pending runnable which will call FlushPendingLinkUpdates().
  bool mHasLinksToUpdateRunnable : 1;

  // True if we're flushing pending link updates.
  bool mFlushingPendingLinkUpdates : 1;

  // True if a DOMMutationObserver is perhaps attached to a node in the document.
  bool mMayHaveDOMMutationObservers : 1;

  // True if an nsIAnimationObserver is perhaps attached to a node in the document.
  bool mMayHaveAnimationObservers : 1;

  // True if a document has loaded Mixed Active Script (see nsMixedContentBlocker.cpp)
  bool mHasMixedActiveContentLoaded : 1;

  // True if a document has blocked Mixed Active Script (see nsMixedContentBlocker.cpp)
  bool mHasMixedActiveContentBlocked : 1;

  // True if a document has loaded Mixed Display/Passive Content (see nsMixedContentBlocker.cpp)
  bool mHasMixedDisplayContentLoaded : 1;

  // True if a document has blocked Mixed Display/Passive Content (see nsMixedContentBlocker.cpp)
  bool mHasMixedDisplayContentBlocked : 1;

  // True if a document loads a plugin object that attempts to load mixed content subresources through necko(see nsMixedContentBlocker.cpp)
  bool mHasMixedContentObjectSubrequest : 1;

  // True if a document load has a CSP attached.
  bool mHasCSP : 1;

  // True if a document load has a CSP with unsafe-eval attached.
  bool mHasUnsafeEvalCSP : 1;

  // True if a document load has a CSP with unsafe-inline attached.
  bool mHasUnsafeInlineCSP : 1;

  // True if a document has blocked Tracking Content
  bool mHasTrackingContentBlocked : 1;

  // True if a document has blocked Tracking Cookies
  bool mHasTrackingCookiesBlocked : 1;

  // True if a document has loaded Tracking Content
  bool mHasTrackingContentLoaded : 1;

  // True if DisallowBFCaching has been called on this document.
  bool mBFCacheDisallowed : 1;

  bool mHasHadDefaultView : 1;

  // Whether style sheet change events will be dispatched for this document
  bool mStyleSheetChangeEventsEnabled : 1;

  // Whether the document was created by a srcdoc iframe.
  bool mIsSrcdocDocument : 1;

  // Records whether we've done a document.open. If this is true, it's possible
  // for nodes from this document to have outdated wrappers in their wrapper
  // caches.
  bool mDidDocumentOpen : 1;

  // Whether this document has a display document and thus is considered to
  // be a resource document.  Normally this is the same as !!mDisplayDocument,
  // but mDisplayDocument is cleared during Unlink.  mHasDisplayDocument is
  // valid in the document's destructor.
  bool mHasDisplayDocument : 1;

  // Is the current mFontFaceSet valid?
  bool mFontFaceSetDirty : 1;

  // Has GetUserFontSet() been called?
  bool mGetUserFontSetCalled : 1;

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

  // True if dom.webcomponents.shadowdom.enabled pref is set when document is
  // created.
  bool mIsShadowDOMEnabled : 1;

  // True if this document is for an SVG-in-OpenType font.
  bool mIsSVGGlyphsDocument : 1;

  // True if the document is being destroyed.
  bool mInDestructor: 1;

  // True if the document has been detached from its content viewer.
  bool mIsGoingAway: 1;

  bool mInXBLUpdate: 1;

  bool mNeedsReleaseAfterStackRefCntRelease: 1;

  // Whether we have filled our pres shell's style set with the document's
  // additional sheets and sheets from the nsStyleSheetService.
  bool mStyleSetFilled: 1;

  // Keeps track of whether we have a pending
  // 'style-sheet-applicable-state-changed' notification.
  bool mSSApplicableStateNotificationPending: 1;

  // True if this document has ever had an HTML or SVG <title> element
  // bound to it
  bool mMayHaveTitleElement: 1;

  bool mDOMLoadingSet: 1;
  bool mDOMInteractiveSet: 1;
  bool mDOMCompleteSet: 1;
  bool mAutoFocusFired: 1;

  bool mScrolledToRefAlready : 1;
  bool mChangeScrollPosWhenScrollingToRef : 1;

  bool mHasWarnedAboutBoxObjects: 1;

  bool mDelayFrameLoaderInitialization: 1;

  bool mSynchronousDOMContentLoaded: 1;

  // Set to true when the document is possibly controlled by the ServiceWorker.
  // Used to prevent multiple requests to ServiceWorkerManager.
  bool mMaybeServiceWorkerControlled: 1;

  // These member variables cache information about the viewport so we don't
  // have to recalculate it each time.
  bool mValidWidth: 1;
  bool mValidHeight: 1;
  bool mAutoSize: 1;
  bool mAllowZoom: 1;
  bool mAllowDoubleTapZoom: 1;
  bool mValidScaleFloat: 1;
  bool mValidMaxScale: 1;
  bool mScaleStrEmpty: 1;
  bool mWidthStrEmpty: 1;

  // Parser aborted. True if the parser of this document was forcibly
  // terminated instead of letting it finish at its own pace.
  bool mParserAborted: 1;

  // Whether we have reported use counters for this document with Telemetry yet.
  // Normally this is only done at document destruction time, but for image
  // documents (SVG documents) that are not guaranteed to be destroyed, we
  // report use counters when the image cache no longer has any imgRequestProxys
  // pointing to them.  We track whether we ever reported use counters so
  // that we only report them once for the document.
  bool mReportedUseCounters: 1;

  bool mHasReportedShadowDOMUsage: 1;

#ifdef DEBUG
public:
  bool mWillReparent: 1;
protected:
#endif

  uint8_t mPendingFullscreenRequests;

  uint8_t mXMLDeclarationBits;

  // Currently active onload blockers.
  uint32_t mOnloadBlockCount;

  // Onload blockers which haven't been activated yet.
  uint32_t mAsyncOnloadBlockCount;

  // Compatibility mode
  nsCompatibility mCompatMode;

  // Our readyState
  ReadyState mReadyState;

#ifdef MOZILLA_INTERNAL_API
  // Our visibility state
  mozilla::dom::VisibilityState mVisibilityState;
  static_assert(sizeof(mozilla::dom::VisibilityState) == sizeof(uint8_t),
                "Error size of mVisibilityState and mDummy");
#else
  uint8_t mDummy;
#endif

  enum Type {
    eUnknown, // should never be used
    eHTML,
    eXHTML,
    eGenericXML,
    eSVG,
    eXUL
  };

  Type mType;

  uint8_t mDefaultElementType;

  enum Tri {
    eTriUnset = 0,
    eTriFalse,
    eTriTrue
  };

  Tri mAllowXULXBL;

  // The document's script global object, the object from which the
  // document can get its script context and scope. This is the
  // *inner* window object.
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobalObject;

  // If mIsStaticDocument is true, mOriginalDocument points to the original
  // document.
  nsCOMPtr<nsIDocument> mOriginalDocument;

  // The bidi options for this document.  What this bitfield means is
  // defined in nsBidiUtils.h
  uint32_t mBidiOptions;

  // The sandbox flags on the document. These reflect the value of the sandbox attribute of the
  // associated IFRAME or CSP-protectable content, if existent. These are set at load time and
  // are immutable - see nsSandboxFlags.h for the possible flags.
  uint32_t mSandboxFlags;

  nsCString mContentLanguage;

  // The channel that got passed to nsDocument::StartDocumentLoad(), if any.
  nsCOMPtr<nsIChannel> mChannel;
private:
  nsCString mContentType;
protected:
  // For document.write() we may need a different content type than mContentType.
  nsCString mContentTypeForWriteCalls;

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

  nsIPresShell* mPresShell;

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
  nsCOMPtr<nsIDocument> mDisplayDocument;

  uint32_t mEventsSuppressed;

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

  // Array of nodes that have been blocked to prevent user tracking.
  // They most likely have had their nsIChannel canceled by the URL
  // classifier. (Safebrowsing)
  //
  // Weak nsINode pointers are used to allow nodes to disappear.
  nsTArray<nsWeakPtr> mBlockedTrackingNodes;

  // Weak reference to mScriptGlobalObject QI:d to nsPIDOMWindow,
  // updated on every set of mScriptGlobalObject.
  nsPIDOMWindowInner* mWindow;

  nsCOMPtr<nsIDocumentEncoder> mCachedEncoder;

  struct FrameRequest;

  nsTArray<FrameRequest> mFrameRequestCallbacks;

  // This object allows us to evict ourself from the back/forward cache.  The
  // pointer is non-null iff we're currently in the bfcache.
  nsIBFCacheEntry *mBFCacheEntry;

  // Our base target.
  nsString mBaseTarget;

  nsCOMPtr<nsIStructuredCloneContainer> mStateObjectContainer;
  nsCOMPtr<nsIVariant> mStateObjectCached;

  uint32_t mInSyncOperationCount;

  mozilla::UniquePtr<mozilla::dom::XPathEvaluator> mXPathEvaluator;

  nsTArray<RefPtr<mozilla::dom::AnonymousContent>> mAnonymousContents;

  uint32_t mBlockDOMContentLoaded;

  // Our live MediaQueryLists
  mozilla::LinkedList<mozilla::dom::MediaQueryList> mDOMMediaQueryLists;

  // Array of observers
  nsTObserverArray<nsIDocumentObserver*> mObservers;

  // Flags for use counters used directly by this document.
  std::bitset<mozilla::eUseCounter_Count> mUseCounters;
  // Flags for use counters used by any child documents of this document.
  std::bitset<mozilla::eUseCounter_Count> mChildDocumentUseCounters;
  // Flags for whether we've notified our top-level "page" of a use counter
  // for this child document.
  std::bitset<mozilla::eUseCounter_Count> mNotifiedPageForUseCounter;

  // Whether the user has interacted with the document or not:
  bool mUserHasInteracted;

  // Whether the user has interacted with the document via a restricted
  // set of gestures which are likely to be interaction with the document,
  // and not events that are fired as a byproduct of the user interacting
  // with the browser (events for like scrolling the page, keyboard short
  // cuts, etc).
  bool mUserGestureActivated;

  mozilla::TimeStamp mPageUnloadingEventTimeStamp;

  RefPtr<mozilla::dom::DocGroup> mDocGroup;

  // The set of all the tracking script URLs.  URLs are added to this set by
  // calling NoteScriptTrackingStatus().  Currently we assume that a URL not
  // existing in the set means the corresponding script isn't a tracking script.
  nsTHashtable<nsCStringHashKey> mTrackingScripts;

  // List of ancestor principals.  This is set at the point a document
  // is connected to a docshell and not mutated thereafter.
  nsTArray<nsCOMPtr<nsIPrincipal>> mAncestorPrincipals;
  // List of ancestor outerWindowIDs that correspond to the ancestor principals.
  nsTArray<uint64_t> mAncestorOuterWindowIDs;

  // Pointer to our parser if we're currently in the process of being
  // parsed into.
  nsCOMPtr<nsIParser> mParser;

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
    Unknown
  };

  ViewportType mViewportType;

  // Enum for how content in this document overflows viewport causing
  // out-of-reach issue. Currently it only takes horizontal overflow
  // into consideration. This enum and the corresponding field is only
  // set and read on a top level content document.
  enum class ViewportOverflowType : uint8_t {
    // Viewport doesn't have out-of-reach overflow content, either
    // because the content doesn't overflow, or the viewport doesn't
    // have "overflow: hidden".
    NoOverflow,

    // All following items indicates that the content overflows the
    // scroll port which causing out-of-reach content.

    // Meta viewport is disabled or the document is in desktop mode.
    Desktop,
    // The content does not overflow the minimum-scale size. When there
    // is no minimum scale specified, the default value used by Blink,
    // 0.25, is used for this matter.
    ButNotMinScaleSize,
    // The content overflows the minimum-scale size.
    MinScaleSize,
  };
  ViewportOverflowType mViewportOverflowType;

  PLDHashTable* mSubDocuments;

  nsDocHeaderData* mHeaderData;

  RefPtr<PrincipalFlashClassifier> mPrincipalFlashClassifier;
  mozilla::dom::FlashClassification mFlashClassification;
  // Do not use this value directly. Call the |IsThirdParty()| method, which
  // caches its result here.
  mozilla::Maybe<bool> mIsThirdParty;

  nsRevocableEventPtr<nsRunnableMethod<nsIDocument, void, false>>
    mPendingTitleChangeEvent;

  RefPtr<nsDOMNavigationTiming> mTiming;

  // Recorded time of change to 'loading' state.
  mozilla::TimeStamp mLoadingTimeStamp;

  nsWeakPtr mAutoFocusElement;

  nsCString mScrollToRef;

  // Weak reference to the scope object (aka the script global object)
  // that, unlike mScriptGlobalObject, is never unset once set. This
  // is a weak reference to avoid leaks due to circular references.
  nsWeakPtr mScopeObject;

  // Array of intersection observers
  nsTHashtable<nsPtrHashKey<mozilla::dom::DOMIntersectionObserver>>
    mIntersectionObservers;

  // Stack of full-screen elements. When we request full-screen we push the
  // full-screen element onto this stack, and when we cancel full-screen we
  // pop one off this stack, restoring the previous full-screen state
  nsTArray<nsWeakPtr> mFullScreenStack;

  // The root of the doc tree in which this document is in. This is only
  // non-null when this document is in fullscreen mode.
  nsWeakPtr mFullscreenRoot;

  RefPtr<mozilla::dom::DOMImplementation> mDOMImplementation;

  RefPtr<nsContentList> mImageMaps;

  // A set of responsive images keyed by address pointer.
  nsTHashtable<nsPtrHashKey<mozilla::dom::HTMLImageElement>> mResponsiveContent;

  // Tracking for plugins in the document.
  nsTHashtable<nsPtrHashKey<nsIObjectLoadingContent>> mPlugins;

  // Array of owning references to all children
  nsAttrAndChildArray mChildren;

  RefPtr<mozilla::dom::DocumentTimeline> mDocumentTimeline;
  mozilla::LinkedList<mozilla::dom::DocumentTimeline> mTimelines;

  RefPtr<mozilla::dom::ScriptLoader> mScriptLoader;

  nsRefPtrHashtable<nsPtrHashKey<nsIContent>, mozilla::dom::BoxObject>*
    mBoxObjectTable;

  // Tracker for animations that are waiting to start.
  // nullptr until GetOrCreatePendingAnimationTracker is called.
  RefPtr<mozilla::PendingAnimationTracker> mPendingAnimationTracker;

  // A document "without a browsing context" that owns the content of
  // HTMLTemplateElement.
  nsCOMPtr<nsIDocument> mTemplateContentsOwner;

  nsExternalResourceMap mExternalResourceMap;

  // ScreenOrientation "pending promise" as described by
  // http://www.w3.org/TR/screen-orientation/
  RefPtr<mozilla::dom::Promise> mOrientationPendingPromise;

  uint16_t mCurrentOrientationAngle;
  mozilla::dom::OrientationType mCurrentOrientationType;

  nsTArray<RefPtr<nsFrameLoader>> mInitializableFrameLoaders;
  nsTArray<nsCOMPtr<nsIRunnable>> mFrameLoaderFinalizers;
  RefPtr<nsRunnableMethod<nsIDocument>> mFrameLoaderRunner;

  // The layout history state that should be used by nodes in this
  // document.  We only actually store a pointer to it when:
  // 1)  We have no script global object.
  // 2)  We haven't had Destroy() called on us yet.
  nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;

  // These member variables cache information about the viewport so we don't
  // have to recalculate it each time.
  mozilla::LayoutDeviceToScreenScale mScaleMinFloat;
  mozilla::LayoutDeviceToScreenScale mScaleMaxFloat;
  mozilla::LayoutDeviceToScreenScale mScaleFloat;
  mozilla::CSSToLayoutDeviceScale mPixelRatio;
  mozilla::CSSSize mViewportSize;

  RefPtr<mozilla::EventListenerManager> mListenerManager;

  nsCOMPtr<nsIRunnable> mMaybeEndOutermostXBLUpdateRunner;
  nsCOMPtr<nsIRequest> mOnloadBlocker;

  nsTArray<RefPtr<mozilla::StyleSheet>> mAdditionalSheets[AdditionalSheetTypeCount];

  // Member to store out last-selected stylesheet set.
  nsString mLastStyleSheetSet;
  nsString mPreferredStyleSheetSet;

  RefPtr<nsDOMStyleSheetSetList> mStyleSheetSetList;

  // We lazily calculate declaration blocks for SVG elements with mapped
  // attributes in Servo mode. This list contains all elements which need lazy
  // resolution.
  nsTHashtable<nsPtrHashKey<nsSVGElement>> mLazySVGPresElements;

  // Restyle root for servo's style system.
  //
  // We store this as an nsINode, rather than as an Element, so that we can store
  // the Document node as the restyle root if the entire document (along with all
  // document-level native-anonymous content) needs to be restyled.
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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocument, NS_IDOCUMENT_IID)

/**
 * mozAutoSubtreeModified batches DOM mutations so that a DOMSubtreeModified
 * event is dispatched, if necessary, when the outermost mozAutoSubtreeModified
 * object is deleted.
 */
class MOZ_STACK_CLASS mozAutoSubtreeModified
{
public:
  /**
   * @param aSubTreeOwner The document in which a subtree will be modified.
   * @param aTarget       The target of the possible DOMSubtreeModified event.
   *                      Can be nullptr, in which case mozAutoSubtreeModified
   *                      is just used to batch DOM mutations.
   */
  mozAutoSubtreeModified(nsIDocument* aSubtreeOwner, nsINode* aTarget)
  {
    UpdateTarget(aSubtreeOwner, aTarget);
  }

  ~mozAutoSubtreeModified()
  {
    UpdateTarget(nullptr, nullptr);
  }

  void UpdateTarget(nsIDocument* aSubtreeOwner, nsINode* aTarget)
  {
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
  nsCOMPtr<nsINode>     mTarget;
  nsCOMPtr<nsIDocument> mSubtreeOwner;
};

class MOZ_STACK_CLASS nsAutoSyncOperation
{
public:
  explicit nsAutoSyncOperation(nsIDocument* aDocument);
  ~nsAutoSyncOperation();
private:
  nsCOMArray<nsIDocument> mDocuments;
  uint32_t                mMicroTaskLevel;
};

class MOZ_RAII AutoSetThrowOnDynamicMarkupInsertionCounter final {
  public:
    explicit AutoSetThrowOnDynamicMarkupInsertionCounter(
      nsIDocument* aDocument)
      : mDocument(aDocument)
    {
      mDocument->IncrementThrowOnDynamicMarkupInsertionCounter();
    }

    ~AutoSetThrowOnDynamicMarkupInsertionCounter() {
      mDocument->DecrementThrowOnDynamicMarkupInsertionCounter();
    }

  private:
    nsIDocument* mDocument;
};

class MOZ_RAII IgnoreOpensDuringUnload final
{
public:
  explicit IgnoreOpensDuringUnload(nsIDocument* aDoc)
    : mDoc(aDoc)
  {
    mDoc->IncrementIgnoreOpensDuringUnloadCounter();
  }

  ~IgnoreOpensDuringUnload()
  {
    mDoc->DecrementIgnoreOpensDuringUnloadCounter();
  }
private:
  nsIDocument* mDoc;
};

// XXX These belong somewhere else
nsresult
NS_NewHTMLDocument(nsIDocument** aInstancePtrResult, bool aLoadedAsData = false);

nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult, bool aLoadedAsData = false,
                  bool aIsPlainDocument = false);

nsresult
NS_NewSVGDocument(nsIDocument** aInstancePtrResult);

nsresult
NS_NewImageDocument(nsIDocument** aInstancePtrResult);

nsresult
NS_NewVideoDocument(nsIDocument** aInstancePtrResult);

// Note: it's the caller's responsibility to create or get aPrincipal as needed
// -- this method will not attempt to get a principal based on aDocumentURI.
// Also, both aDocumentURI and aBaseURI must not be null.
nsresult
NS_NewDOMDocument(nsIDocument** aInstancePtrResult,
                  const nsAString& aNamespaceURI,
                  const nsAString& aQualifiedName,
                  mozilla::dom::DocumentType* aDoctype,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal,
                  bool aLoadedAsData,
                  nsIGlobalObject* aEventObject,
                  DocumentFlavor aFlavor);

// This is used only for xbl documents created from the startup cache.
// Non-cached documents are created in the same manner as xml documents.
nsresult
NS_NewXBLDocument(nsIDocument** aInstancePtrResult,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal);

nsresult
NS_NewPluginDocument(nsIDocument** aInstancePtrResult);

inline nsIDocument*
nsINode::GetOwnerDocument() const
{
  nsIDocument* ownerDoc = OwnerDoc();

  return ownerDoc != this ? ownerDoc : nullptr;
}

inline nsINode*
nsINode::OwnerDocAsNode() const
{
  return OwnerDoc();
}

// ShouldUseXBLScope is defined here as a template so that we can get the faster
// version of IsInAnonymousSubtree if we're statically known to be an
// nsIContent.  we could try defining ShouldUseXBLScope separately on nsINode
// and nsIContent, but then we couldn't put its nsINode implementation here
// (because this header does not include nsIContent) and we can't put it in
// nsIContent.h, because the definition of nsIContent::IsInAnonymousSubtree is
// in nsIContentInlines.h.  And then we get include hell from people trying to
// call nsINode::GetParentObject but not including nsIContentInlines.h and with
// no really good way to include it.
template<typename T>
inline bool ShouldUseXBLScope(const T* aNode)
{
  return aNode->IsInAnonymousSubtree() &&
         !aNode->IsAnonymousContentInSVGUseSubtree();
}

inline mozilla::dom::ParentObject
nsINode::GetParentObject() const
{
  mozilla::dom::ParentObject p(OwnerDoc());
    // Note that mUseXBLScope is a no-op for chrome, and other places where we
    // don't use XBL scopes.
  p.mUseXBLScope = ShouldUseXBLScope(this);
  return p;
}

inline nsIDocument*
nsINode::AsDocument()
{
  MOZ_ASSERT(IsDocument());
  return static_cast<nsIDocument*>(this);
}

inline const nsIDocument*
nsINode::AsDocument() const
{
  MOZ_ASSERT(IsDocument());
  return static_cast<const nsIDocument*>(this);
}

#endif /* nsIDocument_h___ */
