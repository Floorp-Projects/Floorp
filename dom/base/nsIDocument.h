/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocument_h___
#define nsIDocument_h___

#include "mozilla/FlushType.h"           // for enum
#include "nsAutoPtr.h"                   // for member
#include "nsCOMArray.h"                  // for member
#include "nsCompatibility.h"             // for member
#include "nsCOMPtr.h"                    // for member
#include "nsGkAtoms.h"                   // for static class members
#include "nsIDocumentObserver.h"         // for typedef (nsUpdateType)
#include "nsILoadGroup.h"                // for member (in nsCOMPtr)
#include "nsINode.h"                     // for base class
#include "nsIParser.h"
#include "nsIScriptGlobalObject.h"       // for member (in nsCOMPtr)
#include "nsIServiceManager.h"
#include "nsIUUIDGenerator.h"
#include "nsPIDOMWindow.h"               // for use in inline functions
#include "nsPropertyTable.h"             // for member
#include "nsDataHashtable.h"             // for member
#include "nsURIHashKey.h"                // for member
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
#include "mozilla/LinkedList.h"
#include "mozilla/SegmentedVector.h"
#include "mozilla/StyleBackendType.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include <bitset>                        // for member

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
class nsAString;
class nsBindingManager;
class nsIDocShell;
class nsDocShell;
class nsDOMNavigationTiming;
class nsFrameLoader;
class nsHTMLCSSStyleSheet;
class nsHTMLDocument;
class nsHTMLStyleSheet;
class nsIAtom;
class nsIBFCacheEntry;
class nsIChannel;
class nsIContent;
class nsIContentSink;
class nsIDocShell;
class nsIDocShellTreeItem;
class nsIDocumentEncoder;
class nsIDocumentObserver;
class nsIDOMDocument;
class nsIDOMDocumentType;
class nsIDOMElement;
class nsIDOMNodeFilter;
class nsIDOMNodeList;
class nsIHTMLCollection;
class nsILayoutHistoryState;
class nsILoadContext;
class nsIObjectLoadingContent;
class nsIObserver;
class nsIPresShell;
class nsIPrincipal;
class nsIRequest;
class nsIRunnable;
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
class nsWindowSizes;
class nsDOMCaretPosition;
class nsViewportInfo;
class nsIGlobalObject;
struct nsCSSSelectorList;

namespace mozilla {
class AbstractThread;
class CSSStyleSheet;
class ErrorResult;
class EventStates;
class PendingAnimationTracker;
class StyleSetHandle;
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
struct ElementRegistrationOptions;
class Event;
class EventTarget;
class FontFaceSet;
class FrameRequestCallback;
struct FullscreenRequest;
class ImageTracker;
class HTMLBodyElement;
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
template<typename> class Sequence;

template<typename, typename> class CallbackObjectHolder;
typedef CallbackObjectHolder<NodeFilter, nsIDOMNodeFilter> NodeFilterHolder;

enum class CallerType : uint32_t;

} // namespace dom
} // namespace mozilla

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

// Enum for HSTS priming states
enum class HSTSPrimingState {
  eNO_HSTS_PRIMING = 0,    // don't do HSTS Priming
  eHSTS_PRIMING_ALLOW = 1, // if HSTS priming fails, allow the load to proceed
  eHSTS_PRIMING_BLOCK = 2  // if HSTS priming fails, block the load
};

// Document states

// RTL locale: specific to the XUL localedir attribute
#define NS_DOCUMENT_STATE_RTL_LOCALE              NS_DEFINE_EVENT_STATE_MACRO(0)
// Window activation status
#define NS_DOCUMENT_STATE_WINDOW_INACTIVE         NS_DEFINE_EVENT_STATE_MACRO(1)

// Some function forward-declarations
class nsContentList;

//----------------------------------------------------------------------

// Document interface.  This is implemented by all document objects in
// Gecko.
class nsIDocument : public nsINode,
                    public mozilla::dom::DispatcherTrait
{
  typedef mozilla::dom::GlobalObject GlobalObject;

public:
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

  /**
   * Signal that the document title may have changed
   * (see nsDocument::GetTitle).
   * @param aBoundTitleElement true if an HTML or SVG <title> element
   * has just been bound to the document.
   */
  virtual void NotifyPossibleTitleChange(bool aBoundTitleElement) = 0;

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
  virtual void SetDocumentURI(nsIURI* aURI) = 0;

  /**
   * Set the URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  virtual void SetChromeXHRDocURI(nsIURI* aURI) = 0;

  /**
   * Set the base URI for the document loaded via XHR, when accessed from
   * chrome privileged script.
   */
  virtual void SetChromeXHRDocBaseURI(nsIURI* aURI) = 0;

  /**
   * Set referrer policy and upgrade-insecure-requests flags
   */
  virtual void ApplySettingsFromCSP(bool aSpeculative) = 0;

  virtual already_AddRefed<nsIParser> CreatorParserOrNull() = 0;

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
   * Check to see if a subresource we want to load requires HSTS priming
   * to be done.
   */
  HSTSPrimingState GetHSTSPrimingStateForLocation(nsIURI* aContentLocation) const
  {
    HSTSPrimingState state;
    if (mHSTSPrimingURIList.Get(aContentLocation, &state)) {
      return state;
    }
    return HSTSPrimingState::eNO_HSTS_PRIMING;
  }

  /**
   * Add a subresource to the HSTS priming list. If this URI is
   * not in the HSTS cache, it will trigger an HSTS priming request
   * when we try to load it.
   */
  void AddHSTSPrimingLocation(nsIURI* aContentLocation, HSTSPrimingState aState)
  {
    mHSTSPrimingURIList.Put(aContentLocation, aState);
  }

  void ClearHSTSPrimingLocation(nsIURI* aContentLocation)
  {
    mHSTSPrimingURIList.Remove(aContentLocation);
  }

  /**
   * Set the principal responsible for this document.
   */
  virtual void SetPrincipal(nsIPrincipal *aPrincipal) = 0;

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
  virtual already_AddRefed<nsIURI> GetBaseURI(bool aTryUseXHRDocBaseURI = false) const override;

  virtual void SetBaseURI(nsIURI* aURI) = 0;

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
  virtual void GetBaseTarget(nsAString &aBaseTarget) = 0;
  void SetBaseTarget(const nsString& aBaseTarget) {
    mBaseTarget = aBaseTarget;
  }

  /**
   * Return a standard name for the document's character set.
   */
  const nsCString& GetDocumentCharacterSet() const
  {
    return mCharacterSet;
  }

  /**
   * Set the document's character encoding. |aCharSetID| should be canonical.
   * That is, callers are responsible for the charset alias resolution.
   */
  virtual void SetDocumentCharacterSet(const nsACString& aCharSetID) = 0;

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
   * Add an observer that gets notified whenever the charset changes.
   */
  virtual nsresult AddCharSetObserver(nsIObserver* aObserver) = 0;

  /**
   * Remove a charset observer.
   */
  virtual void RemoveCharSetObserver(nsIObserver* aObserver) = 0;

  /**
   * This gets fired when the element that an id refers to changes.
   * This fires at difficult times. It is generally not safe to do anything
   * which could modify the DOM in any way. Use
   * nsContentUtils::AddScriptRunner.
   * @return true to keep the callback in the callback set, false
   * to remove it.
   */
  typedef bool (* IDTargetObserver)(Element* aOldElement,
                                      Element* aNewelement, void* aData);

  /**
   * Add an IDTargetObserver for a specific ID. The IDTargetObserver
   * will be fired whenever the content associated with the ID changes
   * in the future. If aForImage is true, mozSetImageElement can override
   * what content is associated with the ID. In that case the IDTargetObserver
   * will be notified at those times when the result of LookupImageElement
   * changes.
   * At most one (aObserver, aData, aForImage) triple can be
   * registered for each ID.
   * @return the content currently associated with the ID.
   */
  virtual Element* AddIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                       void* aData, bool aForImage) = 0;
  /**
   * Remove the (aObserver, aData, aForImage) triple for a specific ID, if
   * registered.
   */
  virtual void RemoveIDTargetObserver(nsIAtom* aID, IDTargetObserver aObserver,
                                      void* aData, bool aForImage) = 0;

  /**
   * Get the Content-Type of this document.
   * (This will always return NS_OK, but has this signature to be compatible
   *  with nsIDOMDocument::GetContentType())
   */
  NS_IMETHOD GetContentType(nsAString& aContentType) = 0;

  /**
   * Set the Content-Type of this document.
   */
  virtual void SetContentType(const nsAString& aContentType) = 0;

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

  /**
   * Check if the document contains (or has contained) any MathML elements.
   */
  bool GetMathMLEnabled() const
  {
    return mMathMLEnabled;
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
  * Set referrer policy CSP flag for this document.
  */
  void SetHasReferrerPolicyCSP(bool aHasReferrerPolicyCSP)
  {
    mHasReferrerPolicyCSP = aHasReferrerPolicyCSP;
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
   * Set the tracking content blocked flag for this document.
   */
  void SetHasTrackingContentBlocked(bool aHasTrackingContentBlocked)
  {
    mHasTrackingContentBlocked = aHasTrackingContentBlocked;
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
   * Access HTTP header data (this may also get set from other
   * sources, like HTML META tags).
   */
  virtual void GetHeaderData(nsIAtom* aHeaderField, nsAString& aData) const = 0;
  virtual void SetHeaderData(nsIAtom* aheaderField, const nsAString& aData) = 0;

  /**
   * Create a new presentation shell that will use aContext for its
   * presentation context (presentation contexts <b>must not</b> be
   * shared among multiple presentation shells). The caller of this
   * method is responsible for calling BeginObservingDocument() on the
   * presshell if the presshell should observe document mutations.
   */
  virtual already_AddRefed<nsIPresShell> CreateShell(
      nsPresContext* aContext,
      nsViewManager* aViewManager,
      mozilla::StyleSetHandle aStyleSet) = 0;
  virtual void DeleteShell() = 0;

  nsIPresShell* GetShell() const
  {
    return GetBFCacheEntry() ? nullptr : mPresShell;
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

  void SetBFCacheEntry(nsIBFCacheEntry* aEntry)
  {
    NS_ASSERTION(IsBFCachingAllowed() || !aEntry,
                 "You should have checked!");

    mBFCacheEntry = aEntry;
  }

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
  virtual bool GetAllowPlugins () = 0;

  /**
   * Set the sub document for aContent to aSubDoc.
   */
  virtual nsresult SetSubDocumentFor(Element* aContent,
                                     nsIDocument* aSubDoc) = 0;

  /**
   * Get the sub document for aContent
   */
  virtual nsIDocument *GetSubDocumentFor(nsIContent *aContent) const = 0;

  /**
   * Find the content node for which aDocument is a sub document.
   */
  virtual Element* FindContentForSubDocument(nsIDocument* aDocument) const = 0;

  /**
   * Return the doctype for this document.
   */
  mozilla::dom::DocumentType* GetDoctype() const;

  /**
   * Return the root element for this document.
   */
  Element* GetRootElement() const;

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
  virtual nsViewportInfo GetViewportInfo(const mozilla::ScreenIntSize& aDisplaySize) = 0;

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

  static nsresult GenerateDocumentId(nsAString& aId);
  nsresult GetOrCreateId(nsAString& aId);
  void SetId(const nsAString& aId);

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

  virtual void NotifyLayerManagerRecreated() = 0;

  /**
   * Add an SVG element to the list of elements that need
   * their mapped attributes resolved to a Servo declaration block.
   *
   * These are weak pointers, please manually unschedule them when an element
   * is removed.
   */
  virtual void ScheduleSVGForPresAttrEvaluation(nsSVGElement* aSVG) = 0;
  // Unschedule an element scheduled by ScheduleFrameRequestCallback (e.g. for when it is destroyed)
  virtual void UnscheduleSVGForPresAttrEvaluation(nsSVGElement* aSVG) = 0;

  // Resolve all SVG pres attrs scheduled in ScheduleSVGForPresAttrEvaluation
  virtual void ResolveScheduledSVGPresAttrs() = 0;

protected:
  virtual Element *GetRootElementInternal() const = 0;

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
      explicit SelectorCache(nsIEventTarget* aEventTarget);

      // CacheList takes ownership of aSelectorList.
      void CacheList(const nsAString& aSelector, nsCSSSelectorList* aSelectorList);

      virtual void NotifyExpired(SelectorCacheKey* aSelector) override;

      // We do not call MarkUsed because it would just slow down lookups and
      // because we're OK expiring things after a few seconds even if they're
      // being used.  Returns whether we actually had an entry for aSelector.
      // If we have an entry and *aList is null, that indicates that aSelector
      // has already been parsed and is not a syntactically valid selector.
      bool GetList(const nsAString& aSelector, nsCSSSelectorList** aList)
      {
        return mTable.Get(aSelector, aList);
      }

      ~SelectorCache();

    private:
      nsClassHashtable<nsStringHashKey, nsCSSSelectorList> mTable;
  };

  SelectorCache& GetSelectorCache() {
    if (!mSelectorCache) {
      mSelectorCache =
        new SelectorCache(EventTargetFor(mozilla::TaskCategory::Other));
    }
    return *mSelectorCache;
  }
  // Get the root <html> element, or return null if there isn't one (e.g.
  // if the root isn't <html>)
  Element* GetHtmlElement() const;
  // Returns the first child of GetHtmlContent which has the given tag,
  // or nullptr if that doesn't exist.
  Element* GetHtmlChildElement(nsIAtom* aTag);
  // Get the canonical <body> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <body> isn't there)
  mozilla::dom::HTMLBodyElement* GetBodyElement();
  // Get the canonical <head> element, or return null if there isn't one (e.g.
  // if the root isn't <html> or if the <head> isn't there)
  Element* GetHeadElement() {
    return GetHtmlChildElement(nsGkAtoms::head);
  }

  /**
   * Accessors to the collection of stylesheets owned by this document.
   * Style sheets are ordered, most significant last.
   */

  /**
   * These exists to allow us to on-demand load user-agent style sheets that
   * would otherwise be loaded by nsDocumentViewer::CreateStyleSet. This allows
   * us to keep the memory used by a document's rule cascade data (the stuff in
   * its nsStyleSet's nsCSSRuleProcessors) - which can be considerable - lower
   * than it would be if we loaded all built-in user-agent style sheets up
   * front.
   *
   * By "built-in" user-agent style sheets we mean the user-agent style sheets
   * that gecko itself supplies (such as html.css and svg.css) as opposed to
   * user-agent level style sheets inserted by add-ons or the like.
   *
   * This function prepends the given style sheet to the document's style set
   * in order to make sure that it does not override user-agent style sheets
   * supplied by add-ons or by the app (Firefox OS or Firefox Mobile, for
   * example), since their sheets should override built-in sheets.
   *
   * TODO We can get rid of the whole concept of delayed loading if we fix
   * bug 77999.
   */
  virtual void EnsureOnDemandBuiltInUASheet(mozilla::StyleSheet* aSheet) = 0;

  /**
   * Get the number of (document) stylesheets
   *
   * @return the number of stylesheets
   * @throws no exceptions
   */
  virtual int32_t GetNumberOfStyleSheets() const = 0;

  /**
   * Get a particular stylesheet
   * @param aIndex the index the stylesheet lives at.  This is zero-based
   * @return the stylesheet at aIndex.  Null if aIndex is out of range.
   * @throws no exceptions
   */
  virtual mozilla::StyleSheet* GetStyleSheetAt(int32_t aIndex) const = 0;

  /**
   * Insert a sheet at a particular spot in the stylesheet list (zero-based)
   * @param aSheet the sheet to insert
   * @param aIndex the index to insert at.  This index will be
   *   adjusted for the "special" sheets.
   * @throws no exceptions
   */
  virtual void InsertStyleSheetAt(mozilla::StyleSheet* aSheet,
                                  int32_t aIndex) = 0;

  /**
   * Get the index of a particular stylesheet.  This will _always_
   * consider the "special" sheets as part of the sheet list.
   * @param aSheet the sheet to get the index of
   * @return aIndex the index of the sheet in the full list
   */
  virtual int32_t GetIndexOfStyleSheet(
      const mozilla::StyleSheet* aSheet) const = 0;

  /**
   * Replace the stylesheets in aOldSheets with the stylesheets in
   * aNewSheets. The two lists must have equal length, and the sheet
   * at positon J in the first list will be replaced by the sheet at
   * position J in the second list.  Some sheets in the second list
   * may be null; if so the corresponding sheets in the first list
   * will simply be removed.
   */
  virtual void UpdateStyleSheets(
      nsTArray<RefPtr<mozilla::StyleSheet>>& aOldSheets,
      nsTArray<RefPtr<mozilla::StyleSheet>>& aNewSheets) = 0;

  /**
   * Add a stylesheet to the document
   */
  virtual void AddStyleSheet(mozilla::StyleSheet* aSheet) = 0;

  /**
   * Remove a stylesheet from the document
   */
  virtual void RemoveStyleSheet(mozilla::StyleSheet* aSheet) = 0;

  /**
   * Notify the document that the applicable state of the sheet changed
   * and that observers should be notified and style sets updated
   */
  virtual void SetStyleSheetApplicableState(mozilla::StyleSheet* aSheet,
                                            bool aApplicable) = 0;

  enum additionalSheetType {
    eAgentSheet,
    eUserSheet,
    eAuthorSheet,
    AdditionalSheetTypeCount
  };

  virtual nsresult LoadAdditionalStyleSheet(additionalSheetType aType,
                                            nsIURI* aSheetURI) = 0;
  virtual nsresult AddAdditionalStyleSheet(additionalSheetType aType,
                                           mozilla::StyleSheet* aSheet) = 0;
  virtual void RemoveAdditionalStyleSheet(additionalSheetType aType,
                                          nsIURI* sheetURI) = 0;
  virtual mozilla::StyleSheet* GetFirstAdditionalAuthorSheet() = 0;

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
                                         mozilla::StyleSheet* aSheet);

  /**
   * Get this document's CSSLoader.  This is guaranteed to not return null.
   */
  mozilla::css::Loader* CSSLoader() const {
    return mCSSLoader;
  }

  mozilla::StyleBackendType GetStyleBackendType() const {
    if (mStyleBackendType == mozilla::StyleBackendType::None) {
      const_cast<nsIDocument*>(this)->UpdateStyleBackendType();
    }
    MOZ_ASSERT(mStyleBackendType != mozilla::StyleBackendType::None);
    return mStyleBackendType;
  }

  /**
   * Documents generally decide their style backend type themselves, and
   * this is only used for XBL documents to set their style backend type to
   * their bounding document's.
   */
  void SetStyleBackendType(mozilla::StyleBackendType aStyleBackendType) {
    // We cannot assert mStyleBackendType == mozilla::StyleBackendType::None
    // because NS_NewXBLDocument() might result GetStyleBackendType() being
    // called.
    MOZ_ASSERT(aStyleBackendType != mozilla::StyleBackendType::None,
               "The StyleBackendType should be set to either Gecko or Servo!");
    mStyleBackendType = aStyleBackendType;
  }

  /**
   * Decide this document's own style backend type.
   */
  void UpdateStyleBackendType();

  bool IsStyledByServo() const {
    return GetStyleBackendType() == mozilla::StyleBackendType::Servo;
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
  virtual nsIChannel* GetChannel() const = 0;

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

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject) = 0;

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
  virtual void SetScriptHandlingObject(nsIScriptGlobalObject* aScriptObject) = 0;

  /**
   * Get the object that is used as the scope for all of the content
   * wrappers whose owner document is this document. Unlike the script global
   * object, this will only return null when the global object for this
   * document is truly gone. Use this object when you're trying to find a
   * content wrapper in XPConnect.
   */
  virtual nsIGlobalObject* GetScopeObject() const = 0;
  virtual void SetScopeObject(nsIGlobalObject* aGlobal) = 0;

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

  /**
   * Get the script loader for this document
   */
  virtual mozilla::dom::ScriptLoader* ScriptLoader() = 0;

  /**
   * Add/Remove an element to the document's id and name hashes
   */
  virtual void AddToIdTable(Element* aElement, nsIAtom* aId) = 0;
  virtual void RemoveFromIdTable(Element* aElement, nsIAtom* aId) = 0;
  virtual void AddToNameTable(Element* aElement, nsIAtom* aName) = 0;
  virtual void RemoveFromNameTable(Element* aElement, nsIAtom* aName) = 0;

  /**
   * Returns all elements in the fullscreen stack in the insertion order.
   */
  virtual nsTArray<Element*> GetFullscreenStack() const = 0;

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
  virtual void AsyncRequestFullScreen(
    mozilla::UniquePtr<FullscreenRequest>&& aRequest) = 0;

  /**
   * Called when a frame in a child process has entered fullscreen or when a
   * fullscreen frame in a child process changes to another origin.
   * aFrameElement is the frame element which contains the child-process
   * fullscreen document.
   */
  virtual nsresult
    RemoteFrameFullscreenChanged(nsIDOMElement* aFrameElement) = 0;

  /**
   * Called when a frame in a remote child document has rolled back fullscreen
   * so that all its fullscreen element stacks are empty; we must continue the
   * rollback in this parent process' doc tree branch which is fullscreen.
   * Note that only one branch of the document tree can have its documents in
   * fullscreen state at one time. We're in inconsistent state if a
   * fullscreen document has a parent and that parent isn't fullscreen. We
   * preserve this property across process boundaries.
   */
   virtual nsresult RemoteFrameFullscreenReverted() = 0;

  /**
   * Restores the previous full-screen element to full-screen status. If there
   * is no former full-screen element, this exits full-screen, moving the
   * top-level browser window out of full-screen mode.
   */
  virtual void RestorePreviousFullScreenState() = 0;

  /**
   * Returns true if this document is a fullscreen leaf document, i.e. it
   * is in fullscreen mode and has no fullscreen children.
   */
  virtual bool IsFullscreenLeaf() = 0;

  /**
   * Returns the document which is at the root of this document's branch
   * in the in-process document tree. Returns nullptr if the document isn't
   * fullscreen.
   */
  virtual nsIDocument* GetFullscreenRoot() = 0;

  /**
   * Sets the fullscreen root to aRoot. This stores a weak reference to aRoot
   * in this document.
   */
  virtual void SetFullscreenRoot(nsIDocument* aRoot) = 0;

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

  virtual void RequestPointerLock(Element* aElement,
                                  mozilla::dom::CallerType aCallerType) = 0;

  static void UnlockPointer(nsIDocument* aDoc = nullptr);

  // ScreenOrientation related APIs

  virtual void SetCurrentOrientation(mozilla::dom::OrientationType aType,
                                     uint16_t aAngle) = 0;
  virtual uint16_t CurrentOrientationAngle() const = 0;
  virtual mozilla::dom::OrientationType CurrentOrientationType() const = 0;
  virtual void SetOrientationPendingPromise(mozilla::dom::Promise* aPromise) = 0;
  virtual mozilla::dom::Promise* GetOrientationPendingPromise() const = 0;

  //----------------------------------------------------------------------

  // Document notification API's

  /**
   * Add a new observer of document change notifications. Whenever
   * content is changed, appended, inserted or removed the observers are
   * informed.  An observer that is already observing the document must
   * not be added without being removed first.
   */
  virtual void AddObserver(nsIDocumentObserver* aObserver) = 0;

  /**
   * Remove an observer of document change notifications. This will
   * return false if the observer cannot be found.
   */
  virtual bool RemoveObserver(nsIDocumentObserver* aObserver) = 0;

  // Observation hooks used to propagate notifications to document observers.
  // BeginUpdate must be called before any batch of modifications of the
  // content model or of style data, EndUpdate must be called afterward.
  // To make this easy and painless, use the mozAutoDocUpdate helper class.
  virtual void BeginUpdate(nsUpdateType aUpdateType) = 0;
  virtual void EndUpdate(nsUpdateType aUpdateType) = 0;
  virtual void BeginLoad() = 0;
  virtual void EndLoad() = 0;

  enum ReadyState { READYSTATE_UNINITIALIZED = 0, READYSTATE_LOADING = 1, READYSTATE_INTERACTIVE = 3, READYSTATE_COMPLETE = 4};
  virtual void SetReadyStateInternal(ReadyState rs) = 0;
  ReadyState GetReadyStateEnum()
  {
    return mReadyState;
  }

  // notify that a content node changed state.  This must happen under
  // a scriptblocker but NOT within a begin/end update.
  virtual void ContentStateChanged(nsIContent* aContent,
                                   mozilla::EventStates aStateMask) = 0;

  // Notify that a document state has changed.
  // This should only be called by callers whose state is also reflected in the
  // implementation of nsDocument::GetDocumentState.
  virtual void DocumentStatesChanged(mozilla::EventStates aStateMask) = 0;

  // Observation hooks for style data to propagate notifications
  // to document observers
  virtual void StyleRuleChanged(mozilla::StyleSheet* aStyleSheet,
                                mozilla::css::Rule* aStyleRule) = 0;
  virtual void StyleRuleAdded(mozilla::StyleSheet* aStyleSheet,
                              mozilla::css::Rule* aStyleRule) = 0;
  virtual void StyleRuleRemoved(mozilla::StyleSheet* aStyleSheet,
                                mozilla::css::Rule* aStyleRule) = 0;

  /**
   * Flush notifications for this document and its parent documents
   * (since those may affect the layout of this one).
   */
  virtual void FlushPendingNotifications(mozilla::FlushType aType) = 0;

  /**
   * Calls FlushPendingNotifications on any external resources this document
   * has. If this document has no external resources or is an external resource
   * itself this does nothing. This should only be called with
   * aType >= FlushType::Style.
   */
  virtual void FlushExternalResources(mozilla::FlushType aType) = 0;

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
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) = 0;

  /**
   * Reset this document to aURI, aLoadGroup, and aPrincipal.  aURI must not be
   * null.  If aPrincipal is null, a codebase principal based on aURI will be
   * used.
   */
  virtual void ResetToURI(nsIURI *aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal) = 0;

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
  virtual void SetXMLDeclaration(const char16_t *aVersion,
                                 const char16_t *aEncoding,
                                 const int32_t aStandalone) = 0;
  virtual void GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& Standalone) = 0;

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
    return IsXULDocument() || AllowXULXBL();
  }

  virtual bool IsScriptEnabled() = 0;

  bool IsTopLevelContentDocument() const { return mIsTopLevelContentDocument; }
  void SetIsTopLevelContentDocument(bool aIsTopLevelContentDocument)
  {
    mIsTopLevelContentDocument = aIsTopLevelContentDocument;
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
  virtual already_AddRefed<Element> CreateElem(const nsAString& aName,
                                               nsIAtom* aPrefix,
                                               int32_t aNamespaceID,
                                               const nsAString* aIs = nullptr) = 0;

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
  virtual nsIChannel* GetFailedChannel() const = 0;

  /**
   * Set the channel that failed to load and resulted in an error page.
   * This is only relevant to error pages.
   */
  virtual void SetFailedChannel(nsIChannel* aChannel) = 0;

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

  nsPropertyTable* PropertyTable(uint16_t aCategory) {
    if (aCategory == 0)
      return &mPropertyTable;
    return GetExtraPropertyTable(aCategory);
  }
  uint32_t GetPropertyTableCount()
  { return mExtraPropertyTables.Length() + 1; }

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
  virtual void Sanitize() = 0;

  /**
   * Enumerate all subdocuments.
   * The enumerator callback should return true to continue enumerating, or
   * false to stop.  This will never get passed a null aDocument.
   */
  typedef bool (*nsSubDocEnumFunc)(nsIDocument *aDocument, void *aData);
  virtual void EnumerateSubDocuments(nsSubDocEnumFunc aCallback,
                                     void *aData) = 0;

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
  virtual bool CanSavePresentation(nsIRequest *aNewRequest) = 0;

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
  virtual already_AddRefed<nsILayoutHistoryState> GetLayoutHistoryState() const = 0;

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

  virtual void UnblockDOMContentLoaded() = 0;

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
                          mozilla::dom::EventTarget* aDispatchStartTarget) = 0;

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
  virtual void OnPageHide(bool aPersisted,
                          mozilla::dom::EventTarget* aDispatchStartTarget) = 0;

  /*
   * We record the set of links in the document that are relevant to
   * style.
   */
  /**
   * Notification that an element is a link that is relevant to style.
   */
  virtual void AddStyleRelevantLink(mozilla::dom::Link* aLink) = 0;
  /**
   * Notification that an element is a link and its URI might have been
   * changed or the element removed. If the element is still a link relevant
   * to style, then someone must ensure that AddStyleRelevantLink is
   * (eventually) called on it again.
   */
  virtual void ForgetLink(mozilla::dom::Link* aLink) = 0;

  /**
   * Resets and removes a box object from the document's box object cache
   *
   * @param aElement canonical nsIContent pointer of the box object's element
   */
  virtual void ClearBoxObjectFor(nsIContent *aContent) = 0;

  /**
   * Get the box object for an element. This is not exposed through a
   * scriptable interface except for XUL documents.
   */
  virtual already_AddRefed<mozilla::dom::BoxObject>
    GetBoxObjectFor(mozilla::dom::Element* aElement,
                    mozilla::ErrorResult& aRv) = 0;

  /**
   * Support for window.matchMedia()
   */

  already_AddRefed<mozilla::dom::MediaQueryList>
    MatchMedia(const nsAString& aMediaQueryList);

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
  bool HaveFiredDOMTitleChange() const {
    return mHaveFiredTitleChange;
  }

  /**
   * See GetAnonymousElementByAttribute on nsIDOMDocumentXBL.
   */
  virtual Element*
    GetAnonymousElementByAttribute(nsIContent* aElement,
                                   nsIAtom* aAttrName,
                                   const nsAString& aAttrValue) const = 0;

  /**
   * Helper for nsIDOMDocument::elementFromPoint implementation that allows
   * ignoring the scroll frame and/or avoiding layout flushes.
   *
   * @see nsIDOMWindowUtils::elementFromPoint
   */
  virtual Element* ElementFromPointHelper(float aX, float aY,
                                          bool aIgnoreRootScrollFrame,
                                          bool aFlushLayout) = 0;

  enum ElementsFromPointFlags {
    IGNORE_ROOT_SCROLL_FRAME = 1,
    FLUSH_LAYOUT = 2,
    IS_ELEMENT_FROM_POINT = 4
  };

  virtual void ElementsFromPointHelper(float aX, float aY,
                                       uint32_t aFlags,
                                       nsTArray<RefPtr<mozilla::dom::Element>>& aElements) = 0;

  virtual nsresult NodesFromRectHelper(float aX, float aY,
                                       float aTopSize, float aRightSize,
                                       float aBottomSize, float aLeftSize,
                                       bool aIgnoreRootScrollFrame,
                                       bool aFlushLayout,
                                       nsIDOMNodeList** aReturn) = 0;

  /**
   * See FlushSkinBindings on nsBindingManager
   */
  virtual void FlushSkinBindings() = 0;

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

  virtual void SetMayStartLayout(bool aMayStartLayout)
  {
    mMayStartLayout = aMayStartLayout;
  }

  already_AddRefed<nsIDocumentEncoder> GetCachedEncoder();

  void SetCachedEncoder(already_AddRefed<nsIDocumentEncoder> aEncoder);

  // In case of failure, the document really can't initialize the frame loader.
  virtual nsresult InitializeFrameLoader(nsFrameLoader* aLoader) = 0;
  // In case of failure, the caller must handle the error, for example by
  // finalizing frame loader asynchronously.
  virtual nsresult FinalizeFrameLoader(nsFrameLoader* aLoader, nsIRunnable* aFinalizer) = 0;
  // Removes the frame loader of aShell from the initialization list.
  virtual void TryCancelFrameLoaderInitialization(nsIDocShell* aShell) = 0;

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

    void AddObserver(nsIObserver* aObserver) {
      MOZ_ASSERT(aObserver, "Must have observer");
      mObservers.AppendElement(aObserver);
    }

    const nsTArray< nsCOMPtr<nsIObserver> > & Observers() {
      return mObservers;
    }
  protected:
    AutoTArray< nsCOMPtr<nsIObserver>, 8 > mObservers;
  };

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
  virtual nsIDocument*
    RequestExternalResource(nsIURI* aURI,
                            nsINode* aRequestingNode,
                            ExternalResourceLoad** aPendingLoad) = 0;

  /**
   * Enumerate the external resource documents associated with this document.
   * The enumerator callback should return true to continue enumerating, or
   * false to stop.  This callback will never get passed a null aDocument.
   */
  virtual void EnumerateExternalResources(nsSubDocEnumFunc aCallback,
                                          void* aData) = 0;

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
  virtual nsSMILAnimationController* GetAnimationController() = 0;

  // Gets the tracker for animations that are waiting to start.
  // Returns nullptr if there is no pending animation tracker for this document
  // which will be the case if there have never been any CSS animations or
  // transitions on elements in the document.
  virtual mozilla::PendingAnimationTracker* GetPendingAnimationTracker() = 0;

  // Gets the tracker for animations that are waiting to start and
  // creates it if it doesn't already exist. As a result, the return value
  // will never be nullptr.
  virtual mozilla::PendingAnimationTracker*
  GetOrCreatePendingAnimationTracker() = 0;

  /**
   * Prevents user initiated events from being dispatched to the document and
   * subdocuments.
   */
  virtual void SuppressEventHandling(uint32_t aIncrease = 1) = 0;

  /**
   * Unsuppress event handling.
   * @param aFireEvents If true, delayed events (focus/blur) will be fired
   *                    asynchronously.
   */
  virtual void UnsuppressEventHandlingAndFireEvents(bool aFireEvents) = 0;

  uint32_t EventHandlingSuppressed() const { return mEventsSuppressed; }

  bool IsEventHandlingEnabled() {
    return !EventHandlingSuppressed() && mScriptGlobalObject;
  }

  /**
   * Increment the number of external scripts being evaluated.
   */
  void BeginEvaluatingExternalScript() { ++mExternalScriptsBeingEvaluated; }

  /**
   * Decrement the number of external scripts being evaluated.
   */
  void EndEvaluatingExternalScript() { --mExternalScriptsBeingEvaluated; }

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
  virtual nsIDocument* GetTemplateContentsOwner() = 0;

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

  virtual void PreloadPictureOpened() = 0;

  virtual void PreloadPictureClosed() = 0;

  virtual void PreloadPictureImageSource(const nsAString& aSrcsetAttr,
                                         const nsAString& aSizesAttr,
                                         const nsAString& aTypeAttr,
                                         const nsAString& aMediaAttr) = 0;

  /**
   * Called by the parser to resolve an image for preloading. The parser will
   * call the PreloadPicture* functions to inform us of possible <picture>
   * nesting and possible sources, which are used to inform URL selection
   * responsive <picture> or <img srcset> images.  Unset attributes are expected
   * to be marked void.
   */
  virtual already_AddRefed<nsIURI>
    ResolvePreloadImage(nsIURI *aBaseURI,
                        const nsAString& aSrcAttr,
                        const nsAString& aSrcsetAttr,
                        const nsAString& aSizesAttr) = 0;
  /**
   * Called by nsParser to preload images. Can be removed and code moved
   * to nsPreloadURIs::PreloadURIs() in file nsParser.cpp whenever the
   * parser-module is linked with gklayout-module.  aCrossOriginAttr should
   * be a void string if the attr is not present.
   */
  virtual void MaybePreLoadImage(nsIURI* uri,
                                 const nsAString& aCrossOriginAttr,
                                 ReferrerPolicyEnum aReferrerPolicy) = 0;

  /**
   * Called by images to forget an image preload when they start doing
   * the real load.
   */
  virtual void ForgetImagePreload(nsIURI* aURI) = 0;

  /**
   * Called by nsParser to preload style sheets.  Can also be merged into the
   * parser if and when the parser is merged with libgklayout.  aCrossOriginAttr
   * should be a void string if the attr is not present.
   */
  virtual void PreloadStyle(nsIURI* aURI, const nsAString& aCharset,
                            const nsAString& aCrossOriginAttr,
                            ReferrerPolicyEnum aReferrerPolicy,
                            const nsAString& aIntegrity) = 0;

  /**
   * Called by the chrome registry to load style sheets.  Can be put
   * back there if and when when that module is merged with libgklayout.
   *
   * This always does a synchronous load.  If aIsAgentSheet is true,
   * it also uses the system principal and enables unsafe rules.
   * DO NOT USE FOR UNTRUSTED CONTENT.
   */
  virtual nsresult LoadChromeSheetSync(nsIURI* aURI, bool aIsAgentSheet,
                                       RefPtr<mozilla::StyleSheet>* aSheet) = 0;

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
  virtual void MaybePreconnect(nsIURI* uri,
                               mozilla::CORSMode aCORSMode) = 0;

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
  virtual int GetDocumentLWTheme() { return Doc_Theme_None; }

  /**
   * Returns the document state.
   * Document state bits have the form NS_DOCUMENT_STATE_* and are declared in
   * nsIDocument.h.
   */
  virtual mozilla::EventStates GetDocumentState() = 0;
  virtual mozilla::EventStates ThreadSafeGetDocumentState() const = 0;

  virtual nsISupports* GetCurrentContentSink() = 0;

  virtual void SetScrollToRef(nsIURI *aDocumentURI) = 0;
  virtual void ScrollToRef() = 0;
  virtual void ResetScrolledToRefAlready() = 0;
  virtual void SetChangeScrollPosWhenScrollingToRef(bool aValue) = 0;

  /**
   * This method is similar to GetElementById() from nsIDOMDocument but it
   * returns a mozilla::dom::Element instead of a nsIDOMElement.
   * It prevents converting nsIDOMElement to mozilla::dom::Element which is
   * already converted from mozilla::dom::Element.
   */
  virtual Element* GetElementById(const nsAString& aElementId) = 0;

  /**
   * This method returns _all_ the elements in this document which
   * have id aElementId, if there are any.  Otherwise it returns null.
   */
  virtual const nsTArray<Element*>* GetAllElementsForId(const nsAString& aElementId) const = 0;

  /**
   * Lookup an image element using its associated ID, which is usually provided
   * by |-moz-element()|. Similar to GetElementById, with the difference that
   * elements set using mozSetImageElement have higher priority.
   * @param aId the ID associated the element we want to lookup
   * @return the element associated with |aId|
   */
  virtual Element* LookupImageElement(const nsAString& aElementId) = 0;

  virtual mozilla::dom::DocumentTimeline* Timeline() = 0;
  virtual mozilla::LinkedList<mozilla::dom::DocumentTimeline>& Timelines() = 0;

  virtual void GetAnimations(
      nsTArray<RefPtr<mozilla::dom::Animation>>& aAnimations) = 0;

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

  virtual nsresult AddPlugin(nsIObjectLoadingContent* aPlugin) = 0;
  virtual void RemovePlugin(nsIObjectLoadingContent* aPlugin) = 0;
  virtual void GetPlugins(nsTArray<nsIObjectLoadingContent*>& aPlugins) = 0;

  virtual nsresult AddResponsiveContent(nsIContent* aContent) = 0;
  virtual void RemoveResponsiveContent(nsIContent* aContent) = 0;
  virtual void NotifyMediaFeatureValuesChanged() = 0;

  virtual nsresult GetStateObject(nsIVariant** aResult) = 0;

  virtual nsDOMNavigationTiming* GetNavigationTiming() const = 0;

  virtual nsresult SetNavigationTiming(nsDOMNavigationTiming* aTiming) = 0;

  virtual Element* FindImageMap(const nsAString& aNormalizedMapName) = 0;

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

  virtual void PostVisibilityUpdateEvent() = 0;

  bool IsSyntheticDocument() const { return mIsSyntheticDocument; }

  // Note: nsIDocument is a sub-class of nsINode, which has a
  // SizeOfExcludingThis function.  However, because nsIDocument objects can
  // only appear at the top of the DOM tree, we have a specialized measurement
  // function which returns multiple sizes.
  virtual void DocAddSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const;
  // DocAddSizeOfIncludingThis doesn't need to be overridden by sub-classes
  // because nsIDocument inherits from nsINode;  see the comment above the
  // declaration of nsINode::SizeOfIncludingThis.
  virtual void DocAddSizeOfIncludingThis(nsWindowSizes* aWindowSizes) const;

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
  already_AddRefed<Element> CreateHTMLElement(nsIAtom* aTag);

  // WebIDL API
  nsIGlobalObject* GetParentObject() const
  {
    return GetScopeObject();
  }
  static already_AddRefed<nsIDocument>
    Constructor(const GlobalObject& aGlobal,
                mozilla::ErrorResult& rv);
  virtual mozilla::dom::DOMImplementation*
    GetImplementation(mozilla::ErrorResult& rv) = 0;
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
    eCreated,
    eAttached,
    eDetached,
    eAttributeChanged
  };

  nsIDocument* GetTopLevelContentDocument();

  virtual void
    RegisterElement(JSContext* aCx, const nsAString& aName,
                    const mozilla::dom::ElementRegistrationOptions& aOptions,
                    JS::MutableHandle<JSObject*> aRetval,
                    mozilla::ErrorResult& rv) = 0;
  virtual already_AddRefed<mozilla::dom::CustomElementRegistry>
    GetCustomElementRegistry() = 0;

  already_AddRefed<nsContentList>
  GetElementsByTagName(const nsAString& aTagName)
  {
    return NS_GetContentList(this, kNameSpaceID_Unknown, aTagName);
  }
  already_AddRefed<nsContentList>
    GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName,
                           mozilla::ErrorResult& aResult);
  already_AddRefed<nsContentList>
    GetElementsByClassName(const nsAString& aClasses);
  // GetElementById defined above
  virtual already_AddRefed<Element>
    CreateElement(const nsAString& aTagName,
                  const mozilla::dom::ElementCreationOptionsOrString& aOptions,
                  mozilla::ErrorResult& rv) = 0;
  virtual already_AddRefed<Element>
    CreateElementNS(const nsAString& aNamespaceURI,
                    const nsAString& aQualifiedName,
                    const mozilla::dom::ElementCreationOptionsOrString& aOptions,
                    mozilla::ErrorResult& rv) = 0;
  already_AddRefed<mozilla::dom::DocumentFragment>
    CreateDocumentFragment() const;
  already_AddRefed<nsTextNode> CreateTextNode(const nsAString& aData) const;
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
  already_AddRefed<mozilla::dom::NodeIterator>
    CreateNodeIterator(nsINode& aRoot, uint32_t aWhatToShow,
                       mozilla::dom::NodeFilterHolder aFilter,
                       mozilla::ErrorResult& rv) const;
  already_AddRefed<mozilla::dom::TreeWalker>
    CreateTreeWalker(nsINode& aRoot, uint32_t aWhatToShow,
                     mozilla::dom::NodeFilter* aFilter, mozilla::ErrorResult& rv) const;
  already_AddRefed<mozilla::dom::TreeWalker>
    CreateTreeWalker(nsINode& aRoot, uint32_t aWhatToShow,
                     mozilla::dom::NodeFilterHolder aFilter,
                     mozilla::ErrorResult& rv) const;

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
  // Not const because otherwise the compiler can't figure out whether to call
  // this GetTitle or the nsAString version from non-const methods, since
  // neither is an exact match.
  virtual void GetTitle(nsString& aTitle) = 0;
  virtual void SetTitle(const nsAString& aTitle, mozilla::ErrorResult& rv) = 0;
  void GetDir(nsAString& aDirection) const;
  void SetDir(const nsAString& aDirection);
  nsPIDOMWindowOuter* GetDefaultView() const
  {
    return GetWindow();
  }
  Element* GetActiveElement();
  bool HasFocus(mozilla::ErrorResult& rv) const;
  mozilla::TimeStamp LastFocusTime() const;
  void SetLastFocusTime(const mozilla::TimeStamp& aFocusTime);
  // Event handlers are all on nsINode already
  bool MozSyntheticDocument() const
  {
    return IsSyntheticDocument();
  }
  Element* GetCurrentScript();
  void ReleaseCapture() const;
  virtual void MozSetImageElement(const nsAString& aImageElementId,
                                  Element* aElement) = 0;
  nsIURI* GetDocumentURIObject() const;
  // Not const because all the full-screen goop is not const
  virtual bool FullscreenEnabled(mozilla::dom::CallerType aCallerType) = 0;
  virtual Element* GetFullscreenElement() = 0;
  bool Fullscreen()
  {
    return !!GetFullscreenElement();
  }
  void ExitFullscreen();
  Element* GetPointerLockElement();
  void ExitPointerLock()
  {
    UnlockPointer(this);
  }
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
  virtual mozilla::dom::StyleSheetList* StyleSheets() = 0;
  void GetSelectedStyleSheetSet(nsAString& aSheetSet);
  virtual void SetSelectedStyleSheetSet(const nsAString& aSheetSet) = 0;
  virtual void GetLastStyleSheetSet(nsString& aSheetSet) = 0;
  void GetPreferredStyleSheetSet(nsAString& aSheetSet);
  virtual mozilla::dom::DOMStringList* StyleSheetSets() = 0;
  virtual void EnableStyleSheetsForSet(const nsAString& aSheetSet) = 0;
  Element* ElementFromPoint(float aX, float aY);
  void ElementsFromPoint(float aX,
                         float aY,
                         nsTArray<RefPtr<mozilla::dom::Element>>& aElements);

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
  void LoadBindingDocument(const nsAString& aURI,
                           const mozilla::Maybe<nsIPrincipal*>& aSubjectPrincipal,
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
    CreateTouch(nsGlobalWindow* aView, mozilla::dom::EventTarget* aTarget,
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
                                                       mozilla::ErrorResult& aRv);

  already_AddRefed<nsIURI> GetMozDocumentURIIfNotForErrorPages();

  // ParentNode
  nsIHTMLCollection* Children();
  uint32_t ChildElementCount();

  virtual nsHTMLDocument* AsHTMLDocument() { return nullptr; }
  virtual mozilla::dom::SVGDocument* AsSVGDocument() { return nullptr; }

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
  void RebuildUserFontSet(); // asynchronously
  mozilla::dom::FontFaceSet* GetFonts() { return mFontFaceSet; }

  // FontFaceSource
  mozilla::dom::FontFaceSet* Fonts();

  bool DidFireDOMContentLoaded() const { return mDidFireDOMContentLoaded; }

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

  void SetDocumentIncCounter(mozilla::IncCounter aIncCounter, uint32_t inc = 1)
  {
    mIncCounters[aIncCounter] += inc;
  }

  void SetUserHasInteracted(bool aUserHasInteracted)
  {
    mUserHasInteracted = aUserHasInteracted;
  }

  bool UserHasInteracted()
  {
    return mUserHasInteracted;
  }

  bool HasScriptsBlockedBySandbox();

  bool InlineScriptAllowedByCSP();

  void ReportHasScrollLinkedEffect();
  bool HasScrollLinkedEffect() const
  {
    return mHasScrollLinkedEffect;
  }

  mozilla::dom::DocGroup* GetDocGroup() const;

  virtual void AddIntersectionObserver(
    mozilla::dom::DOMIntersectionObserver* aObserver) = 0;
  virtual void RemoveIntersectionObserver(
    mozilla::dom::DOMIntersectionObserver* aObserver) = 0;

  virtual void UpdateIntersectionObservations() = 0;
  virtual void ScheduleIntersectionObserverNotification() = 0;
  virtual void NotifyIntersectionObservers() = 0;

  // Dispatch a runnable related to the document.
  virtual nsresult Dispatch(const char* aName,
                            mozilla::TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) override;

  virtual nsISerialEventTarget*
  EventTargetFor(mozilla::TaskCategory aCategory) const override;

  virtual mozilla::AbstractThread*
  AbstractMainThreadFor(mozilla::TaskCategory aCategory) override;

  // The URLs passed to these functions should match what
  // JS::DescribeScriptedCaller() returns, since these APIs are used to
  // determine whether some code is being called from a tracking script.
  void NoteScriptTrackingStatus(const nsACString& aURL, bool isTracking);
  bool IsScriptTracking(const nsACString& aURL) const;

  bool PrerenderHref(nsIURI* aHref);

  // For more information on Flash classification, see
  // toolkit/components/url-classifier/flash-block-lists.rst
  virtual mozilla::dom::FlashClassification DocumentFlashClassification() = 0;
  virtual bool IsThirdParty() = 0;

protected:
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

private:
  mutable std::bitset<eDeprecatedOperationCount> mDeprecationWarnedAbout;
  mutable std::bitset<eDocumentWarningCount> mDocWarningWarnedAbout;
  // Lazy-initialization to have mDocGroup initialized in prior to mSelectorCache.
  nsAutoPtr<SelectorCache> mSelectorCache;

protected:
  ~nsIDocument();
  nsPropertyTable* GetExtraPropertyTable(uint16_t aCategory);

  // Never ever call this. Only call GetWindow!
  virtual nsPIDOMWindowOuter* GetWindowInternal() const = 0;

  // Never ever call this. Only call GetScriptHandlingObject!
  virtual nsIScriptGlobalObject* GetScriptHandlingObjectInternal() const = 0;

  // Never ever call this. Only call AllowXULXBL!
  virtual bool InternalAllowXULXBL() = 0;

  /**
   * These methods should be called before and after dispatching
   * a mutation event.
   * To make this easy and painless, use the mozAutoSubtreeModified helper class.
   */
  virtual void WillDispatchMutationEvent(nsINode* aTarget) = 0;
  virtual void MutationEventDispatched(nsINode* aTarget) = 0;
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

  void HandleRebuildUserFontSet() {
    mPostedFlushUserFontSet = false;
    FlushUserFontSet();
  }

  const nsString& GetId() const
  {
    return mId;
  }

  // Update our frame request callback scheduling state, if needed.  This will
  // schedule or unschedule them, if necessary, and update
  // mFrameRequestCallbacksScheduled.  aOldShell should only be passed when
  // mPresShell is becoming null; in that case it will be used to get hold of
  // the relevant refresh driver.
  void UpdateFrameRequestCallbackSchedulingState(nsIPresShell* aOldShell = nullptr);

  // Helper for GetScrollingElement/IsScrollingElement.
  bool IsPotentiallyScrollable(mozilla::dom::HTMLBodyElement* aBody);

  nsCString mReferrer;
  nsString mLastModified;

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mChromeXHRDocURI;
  nsCOMPtr<nsIURI> mDocumentBaseURI;
  nsCOMPtr<nsIURI> mChromeXHRDocBaseURI;

#ifdef MOZ_STYLO
  // A lazily-constructed URL data for style system to resolve URL value.
  RefPtr<mozilla::URLExtraData> mCachedURLData;
#endif

  nsWeakPtr mDocumentLoadGroup;

  bool mReferrerPolicySet;
  ReferrerPolicyEnum mReferrerPolicy;

  bool mBlockAllMixedContent;
  bool mBlockAllMixedContentPreloads;
  bool mUpgradeInsecureRequests;
  bool mUpgradeInsecurePreloads;

  // if nsMixedContentBlocker requires sending an HSTS priming request,
  // temporarily store that in the document so that it can be propogated to the
  // LoadInfo and eventually the HTTP Channel
  nsDataHashtable<nsURIHashKey, HSTSPrimingState> mHSTSPrimingURIList;

  mozilla::WeakPtr<nsDocShell> mDocumentContainer;

  nsCString mCharacterSet;
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

  // The set of all object, embed, applet, video/audio elements or
  // nsIObjectLoadingContent or nsIDocumentActivity for which this is the
  // owner document. (They might not be in the document.)
  // These are non-owning pointers, the elements are responsible for removing
  // themselves when they go away.
  nsAutoPtr<nsTHashtable<nsPtrHashKey<nsISupports> > > mActivityObservers;

  // The array of all links that need their status resolved.  Links must add themselves
  // to this set by calling RegisterPendingLinkUpdate when added to a document.
  static const size_t kSegmentSize = 128;
  mozilla::SegmentedVector<nsCOMPtr<mozilla::dom::Link>,
                           kSegmentSize,
                           InfallibleAllocPolicy>
    mLinksToUpdate;

  // SMIL Animation Controller, lazily-initialized in GetAnimationController
  RefPtr<nsSMILAnimationController> mAnimationController;

  // Table of element properties for this document.
  nsPropertyTable mPropertyTable;
  nsTArray<nsAutoPtr<nsPropertyTable> > mExtraPropertyTables;

  // Our cached .children collection
  nsCOMPtr<nsIHTMLCollection> mChildrenCollection;

  // container for per-context fonts (downloadable, SVG, etc.)
  RefPtr<mozilla::dom::FontFaceSet> mFontFaceSet;

  // Last time this document or a one of its sub-documents was focused.  If
  // focus has never occurred then mLastFocusTime.IsNull() will be true.
  mozilla::TimeStamp mLastFocusTime;

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

  // True if a document load has a CSP with referrer attached.
  bool mHasReferrerPolicyCSP : 1;

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

  // True if this document has links whose state needs updating
  bool mHasLinksToUpdate : 1;

  // True is there is a pending runnable which will call FlushPendingLinkUpdates().
  bool mHasLinksToUpdateRunnable : 1;

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

  // Do we currently have an event posted to call FlushUserFontSet?
  bool mPostedFlushUserFontSet : 1;

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

  // Compatibility mode
  nsCompatibility mCompatMode;

  // Our readyState
  ReadyState mReadyState;

  // Whether this document has (or will have, once we have a pres shell) a
  // Gecko- or Servo-backed style system.
  mozilla::StyleBackendType mStyleBackendType;

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

#ifdef DEBUG
  /**
   * This is true while FlushPendingLinkUpdates executes.  Calls to
   * [Un]RegisterPendingLinkUpdate will assert when this is true.
   */
  bool mIsLinkUpdateRegistrationsForbidden;
#endif

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
  nsString mId;
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

  nsIPresShell* mPresShell;

  nsCOMArray<nsINode> mSubtreeModifiedTargets;
  uint32_t            mSubtreeModifiedDepth;

  // If we're an external resource document, this will be non-null and will
  // point to our "display document": the one that all resource lookups should
  // go to.
  nsCOMPtr<nsIDocument> mDisplayDocument;

  uint32_t mEventsSuppressed;

  /**
   * The number number of external scripts (ones with the src attribute) that
   * have this document as their owner and that are being evaluated right now.
   */
  uint32_t mExternalScriptsBeingEvaluated;

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

  RefPtr<mozilla::dom::XPathEvaluator> mXPathEvaluator;

  nsTArray<RefPtr<mozilla::dom::AnonymousContent>> mAnonymousContents;

  uint32_t mBlockDOMContentLoaded;

  // Our live MediaQueryLists
  mozilla::LinkedList<mozilla::dom::MediaQueryList> mDOMMediaQueryLists;

  // Flags for use counters used directly by this document.
  std::bitset<mozilla::eUseCounter_Count> mUseCounters;
  // Flags for use counters used by any child documents of this document.
  std::bitset<mozilla::eUseCounter_Count> mChildDocumentUseCounters;
  // Flags for whether we've notified our top-level "page" of a use counter
  // for this child document.
  std::bitset<mozilla::eUseCounter_Count> mNotifiedPageForUseCounter;

  // Count the number of times something is seen in a document.
  mozilla::Array<uint16_t, mozilla::eIncCounter_Count> mIncCounters;

  // Whether the user has interacted with the document or not:
  bool mUserHasInteracted;

  mozilla::TimeStamp mPageUnloadingEventTimeStamp;

  RefPtr<mozilla::dom::DocGroup> mDocGroup;

  // The set of all the tracking script URLs.  URLs are added to this set by
  // calling NoteScriptTrackingStatus().  Currently we assume that a URL not
  // existing in the set means the corresponding script isn't a tracking script.
  nsTHashtable<nsCStringHashKey> mTrackingScripts;
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
NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                  const nsAString& aNamespaceURI,
                  const nsAString& aQualifiedName,
                  nsIDOMDocumentType* aDoctype,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal,
                  bool aLoadedAsData,
                  nsIGlobalObject* aEventObject,
                  DocumentFlavor aFlavor);

// This is used only for xbl documents created from the startup cache.
// Non-cached documents are created in the same manner as xml documents.
nsresult
NS_NewXBLDocument(nsIDOMDocument** aInstancePtrResult,
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

inline mozilla::dom::ParentObject
nsINode::GetParentObject() const
{
  mozilla::dom::ParentObject p(OwnerDoc());
    // Note that mUseXBLScope is a no-op for chrome, and other places where we
    // don't use XBL scopes.
  p.mUseXBLScope = IsInAnonymousSubtree() && !IsAnonymousContentInSVGUseSubtree();
  return p;
}

#endif /* nsIDocument_h___ */
