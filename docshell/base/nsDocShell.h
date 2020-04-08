/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShell_h__
#define nsDocShell_h__

#include <utility>

#include "GeckoProfiler.h"
#include "Units.h"
#include "jsapi.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/HalScreenConfiguration.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/ObservedDocShell.h"
#include "mozilla/ScrollbarPreferences.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/TimelineMarker.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ChildSHistory.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/gfx/Matrix.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCharsetSource.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsDocLoader.h"
#include "nsIAuthPromptProvider.h"
#include "nsIBaseWindow.h"
#include "nsIDeprecationWarner.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsILoadURIDelegate.h"
#include "nsINetworkInterceptController.h"
#include "nsIRefreshURI.h"
#include "nsIWebNavigation.h"
#include "nsIWebPageDescriptor.h"
#include "nsIWebProgressListener.h"
#include "nsPoint.h"  // mCurrent/mDefaultScrollbarPreferences
#include "nsRect.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "prtime.h"

// Interfaces Needed

namespace mozilla {
class Encoding;
class HTMLEditor;
enum class TaskCategory;
namespace dom {
class ClientInfo;
class ClientSource;
class EventTarget;
}  // namespace dom
namespace net {
class LoadInfo;
class DocumentChannelRedirect;
}  // namespace net
}  // namespace mozilla

class nsIContentViewer;
class nsIController;
class nsIDocShellTreeOwner;
class nsIHttpChannel;
class nsIMutableArray;
class nsIPrompt;
class nsIScrollableFrame;
class nsISecureBrowserUI;
class nsISHistory;
class nsIStringBundle;
class nsIURIFixup;
class nsIURIFixupInfo;
class nsIURILoader;
class nsIWebBrowserFind;
class nsIWidget;
class nsIReferrerInfo;

class nsCommandManager;
class nsDocShell;
class nsDocShellEditorData;
class nsDOMNavigationTiming;
class nsDSURIContentListener;
class nsGlobalWindowInner;
class nsGlobalWindowOuter;

class FramingChecker;
class OnLinkClickEvent;

/* internally used ViewMode types */
enum ViewMode { viewNormal = 0x0, viewSource = 0x1 };

enum eCharsetReloadState {
  eCharsetReloadInit,
  eCharsetReloadRequested,
  eCharsetReloadStopOrigional
};

class nsDocShell final : public nsDocLoader,
                         public nsIDocShell,
                         public nsIWebNavigation,
                         public nsIBaseWindow,
                         public nsIRefreshURI,
                         public nsIWebProgressListener,
                         public nsIWebPageDescriptor,
                         public nsIAuthPromptProvider,
                         public nsILoadContext,
                         public nsINetworkInterceptController,
                         public nsIDeprecationWarner,
                         public mozilla::SupportsWeakPtr<nsDocShell> {
 public:
  enum InternalLoad : uint32_t {
    INTERNAL_LOAD_FLAGS_NONE = 0x0,
    INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL = 0x1,
    INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER = 0x2,
    INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP = 0x4,

    // This flag marks the first load in this object
    // @see nsIWebNavigation::LOAD_FLAGS_FIRST_LOAD
    INTERNAL_LOAD_FLAGS_FIRST_LOAD = 0x8,

    // The set of flags that should not be set before calling into
    // nsDocShell::LoadURI and other nsDocShell loading functions.
    INTERNAL_LOAD_FLAGS_LOADURI_SETUP_FLAGS = 0xf,

    INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER = 0x10,
    INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES = 0x20,

    // Whether the load should be treated as srcdoc load, rather than a URI one.
    INTERNAL_LOAD_FLAGS_IS_SRCDOC = 0x40,

    // Whether this is the load of a frame's original src attribute
    INTERNAL_LOAD_FLAGS_ORIGINAL_FRAME_SRC = 0x80,

    INTERNAL_LOAD_FLAGS_NO_OPENER = 0x100,

    // Whether a top-level data URI navigation is allowed for that load
    INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI = 0x200,

    // Whether the load should go through LoadURIDelegate.
    INTERNAL_LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE = 0x2000,
  };

  // Event type dispatched by RestorePresentation
  class RestorePresentationEvent : public mozilla::Runnable {
   public:
    NS_DECL_NSIRUNNABLE
    explicit RestorePresentationEvent(nsDocShell* aDs)
        : mozilla::Runnable("nsDocShell::RestorePresentationEvent"),
          mDocShell(aDs) {}
    void Revoke() { mDocShell = nullptr; }

   private:
    RefPtr<nsDocShell> mDocShell;
  };

  class InterfaceRequestorProxy : public nsIInterfaceRequestor {
   public:
    explicit InterfaceRequestorProxy(nsIInterfaceRequestor* aRequestor);
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR

   private:
    virtual ~InterfaceRequestorProxy();
    InterfaceRequestorProxy() = default;
    nsWeakPtr mWeakPtr;
  };

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(nsDocShell)
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDocShell, nsDocLoader)
  NS_DECL_NSIDOCSHELL
  NS_DECL_NSIDOCSHELLTREEITEM
  NS_DECL_NSIWEBNAVIGATION
  NS_DECL_NSIBASEWINDOW
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIREFRESHURI
  NS_DECL_NSIWEBPAGEDESCRIPTOR
  NS_DECL_NSIAUTHPROMPTPROVIDER
  NS_DECL_NSINETWORKINTERCEPTCONTROLLER
  NS_DECL_NSIDEPRECATIONWARNER

  // Create a new nsDocShell object, initializing it.
  static already_AddRefed<nsDocShell> Create(
      mozilla::dom::BrowsingContext* aBrowsingContext,
      uint64_t aContentWindowID = 0);

  NS_IMETHOD Stop() override {
    // Need this here because otherwise nsIWebNavigation::Stop
    // overrides the docloader's Stop()
    return nsDocLoader::Stop();
  }

  mozilla::ScrollbarPreference ScrollbarPreference() const {
    return mScrollbarPref;
  }
  void SetScrollbarPreference(mozilla::ScrollbarPreference);

  /*
   * The size, in CSS pixels, of the margins for the <body> of an HTML document
   * in this docshell; used to implement the marginwidth attribute on HTML
   * <frame>/<iframe> elements.  A value smaller than zero indicates that the
   * attribute was not set.
   */
  const mozilla::CSSIntSize& GetFrameMargins() const { return mFrameMargins; }

  bool UpdateFrameMargins(const mozilla::CSSIntSize& aMargins) {
    if (mFrameMargins == aMargins) {
      return false;
    }
    mFrameMargins = aMargins;
    return true;
  }

  /**
   * Process a click on a link.
   *
   * @param aContent the content object used for triggering the link.
   * @param aURI a URI object that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (may be an empty
   *        string)
   * @param aFileName non-null when the link should be downloaded as the given
   * file
   * @param aPostDataStream the POST data to send
   * @param aHeadersDataStream ??? (only used for plugins)
   * @param aIsTrusted false if the triggerer is an untrusted DOM event.
   * @param aTriggeringPrincipal, if not passed explicitly we fall back to
   *        the document's principal.
   * @param aCsp, the CSP to be used for the load, that is the CSP of the
   *        entity responsible for causing the load to occur. Most likely
   *        this is the CSP of the document that started the load. In case
   *        aCsp was not passed explicitly we fall back to using
   *        aContent's document's CSP if that document holds any.
   */
  nsresult OnLinkClick(nsIContent* aContent, nsIURI* aURI,
                       const nsAString& aTargetSpec, const nsAString& aFileName,
                       nsIInputStream* aPostDataStream,
                       nsIInputStream* aHeadersDataStream,
                       bool aIsUserTriggered, bool aIsTrusted,
                       nsIPrincipal* aTriggeringPrincipal,
                       nsIContentSecurityPolicy* aCsp);
  /**
   * Process a click on a link.
   *
   * Works the same as OnLinkClick() except it happens immediately rather than
   * through an event.
   *
   * @param aContent the content object used for triggering the link.
   * @param aURI a URI obect that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (may be an empty
   *        string)
   * @param aFileName non-null when the link should be downloaded as the given
   * file
   * @param aPostDataStream the POST data to send
   * @param aHeadersDataStream ??? (only used for plugins)
   * @param aNoOpenerImplied if the link implies "noopener"
   * @param aDocShell (out-param) the DocShell that the request was opened on
   * @param aRequest the request that was opened
   * @param aTriggeringPrincipal, if not passed explicitly we fall back to
   *        the document's principal.
   * @param aCsp, the CSP to be used for the load, that is the CSP of the
   *        entity responsible for causing the load to occur. Most likely
   *        this is the CSP of the document that started the load. In case
   *        aCsp was not passed explicitly we fall back to using
   *        aContent's document's CSP if that document holds any.
   */
  nsresult OnLinkClickSync(
      nsIContent* aContent, nsIURI* aURI, const nsAString& aTargetSpec,
      const nsAString& aFileName, nsIInputStream* aPostDataStream = nullptr,
      nsIInputStream* aHeadersDataStream = nullptr,
      bool aNoOpenerImplied = false, nsIDocShell** aDocShell = nullptr,
      nsIRequest** aRequest = nullptr, bool aIsUserTriggered = false,
      nsIPrincipal* aTriggeringPrincipal = nullptr,
      nsIContentSecurityPolicy* aCsp = nullptr);
  /**
   * Process a mouse-over a link.
   *
   * @param aContent the linked content.
   * @param aURI an URI object that defines the destination for the link
   * @param aTargetSpec indicates where the link is targeted (it may be an empty
   *        string)
   */
  nsresult OnOverLink(nsIContent* aContent, nsIURI* aURI,
                      const nsAString& aTargetSpec);
  /**
   * Process the mouse leaving a link.
   */
  nsresult OnLeaveLink();

  // Don't use NS_DECL_NSILOADCONTEXT because some of nsILoadContext's methods
  // are shared with nsIDocShell and can't be declared twice.
  NS_IMETHOD GetAssociatedWindow(mozIDOMWindowProxy**) override;
  NS_IMETHOD GetTopWindow(mozIDOMWindowProxy**) override;
  NS_IMETHOD GetTopFrameElement(mozilla::dom::Element**) override;
  NS_IMETHOD GetNestedFrameId(uint64_t*) override;
  NS_IMETHOD GetIsContent(bool*) override;
  NS_IMETHOD GetUsePrivateBrowsing(bool*) override;
  NS_IMETHOD SetUsePrivateBrowsing(bool) override;
  NS_IMETHOD SetPrivateBrowsing(bool) override;
  NS_IMETHOD GetUseRemoteTabs(bool*) override;
  NS_IMETHOD SetRemoteTabs(bool) override;
  NS_IMETHOD GetUseRemoteSubframes(bool*) override;
  NS_IMETHOD SetRemoteSubframes(bool) override;
  NS_IMETHOD GetScriptableOriginAttributes(
      JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD_(void)
  GetOriginAttributes(mozilla::OriginAttributes& aAttrs) override;

  // Restores a cached presentation from history (mLSHE).
  // This method swaps out the content viewer and simulates loads for
  // subframes. It then simulates the completion of the toplevel load.
  nsresult RestoreFromHistory();

  // Perform a URI load from a refresh timer. This is just like the
  // ForceRefreshURI method on nsIRefreshURI, but makes sure to take
  // the timer involved out of mRefreshURIList if it's there.
  // aTimer must not be null.
  nsresult ForceRefreshURIFromTimer(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                    int32_t aDelay, bool aMetaRefresh,
                                    nsITimer* aTimer);

  // We need dummy OnLocationChange in some cases to update the UI without
  // updating security info.
  void FireDummyOnLocationChange() {
    FireOnLocationChange(this, nullptr, mCurrentURI,
                         LOCATION_CHANGE_SAME_DOCUMENT);
  }

  // This function is created exclusively for dom.background_loading_iframe is
  // set. As soon as the current DocShell knows itself can be treated as
  // background loading, it triggers the parent docshell to see if the parent
  // document can fire load event earlier.
  void TriggerParentCheckDocShellIsEmpty();

  nsresult HistoryEntryRemoved(int32_t aIndex);

  // Notify Scroll observers when an async panning/zooming transform
  // has started being applied
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void NotifyAsyncPanZoomStarted();

  // Notify Scroll observers when an async panning/zooming transform
  // is no longer applied
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void NotifyAsyncPanZoomStopped();

  void SetInFrameSwap(bool aInSwap) { mInFrameSwap = aInSwap; }
  bool InFrameSwap();

  void SetIsFrame() { mIsFrame = true; };

  const mozilla::Encoding* GetForcedCharset() { return mForcedCharset; }

  mozilla::HTMLEditor* GetHTMLEditorInternal();
  nsresult SetHTMLEditorInternal(mozilla::HTMLEditor* aHTMLEditor);

  // Handle page navigation due to charset changes
  nsresult CharsetChangeReloadDocument(const char* aCharset = nullptr,
                                       int32_t aSource = kCharsetUninitialized);
  nsresult CharsetChangeStopDocumentLoad();

  nsDOMNavigationTiming* GetNavigationTiming() const;

  nsresult SetOriginAttributes(const mozilla::OriginAttributes& aAttrs);

  /**
   * Get the list of ancestor principals for this docshell.  The list is meant
   * to be the list of principals of the documents this docshell is "nested
   * through" in the sense of
   * <https://html.spec.whatwg.org/multipage/browsers.html#browsing-context-nested-through>.
   * In practice, it is defined as follows:
   *
   * If this is an <iframe mozbrowser> or a toplevel content docshell
   * (i.e. toplevel document in spec terms), the list is empty.
   *
   * Otherwise the list is the list for the document we're nested through (again
   * in the spec sense), with the principal of that document prepended.  Note
   * that this matches the ordering specified for Location.ancestorOrigins.
   */
  const nsTArray<nsCOMPtr<nsIPrincipal>>& AncestorPrincipals() const {
    return mAncestorPrincipals;
  }

  /**
   * Set the list of ancestor principals for this docshell.  This is really only
   * needed for use by the frameloader.  We can't do this ourselves, inside
   * docshell, because there's a bunch of state setup that frameloader does
   * (like telling us whether we're a mozbrowser), some of which comes after the
   * docshell is added to the docshell tree, which can affect what the ancestor
   * principals should look like.
   *
   * This method steals the data from the passed-in array.
   */
  void SetAncestorPrincipals(
      nsTArray<nsCOMPtr<nsIPrincipal>>&& aAncestorPrincipals) {
    mAncestorPrincipals = std::move(aAncestorPrincipals);
  }

  /**
   * Get the list of ancestor outerWindowIDs for this docshell.  The list is
   * meant to be the list of outer window IDs that correspond to the
   * ancestorPrincipals above.   For each ancestor principal, we store the
   * parent window ID.
   */
  const nsTArray<uint64_t>& AncestorOuterWindowIDs() const {
    return mAncestorOuterWindowIDs;
  }

  /**
   * Set the list of ancestor outer window IDs for this docshell.  We call this
   * from frameloader as well in order to keep the array matched with the
   * ancestor principals.
   *
   * This method steals the data from the passed-in array.
   */
  void SetAncestorOuterWindowIDs(nsTArray<uint64_t>&& aAncestorOuterWindowIDs) {
    mAncestorOuterWindowIDs = std::move(aAncestorOuterWindowIDs);
  }

  const mozilla::OriginAttributes& GetOriginAttributes() {
    return mBrowsingContext->OriginAttributesRef();
  }

  // Determine whether this docshell corresponds to the given history entry,
  // via having a pointer to it in mOSHE or mLSHE.
  bool HasHistoryEntry(nsISHEntry* aEntry) const {
    return aEntry && (aEntry == mOSHE || aEntry == mLSHE);
  }

  // Update any pointers (mOSHE or mLSHE) to aOldEntry to point to aNewEntry
  void SwapHistoryEntries(nsISHEntry* aOldEntry, nsISHEntry* aNewEntry);

  mozilla::gfx::Matrix5x4* GetColorMatrix() { return mColorMatrix.get(); }

  static bool SandboxFlagsImplyCookies(const uint32_t& aSandboxFlags);

  // Tell the favicon service that aNewURI has the same favicon as aOldURI.
  static void CopyFavicon(nsIURI* aOldURI, nsIURI* aNewURI,
                          nsIPrincipal* aLoadingPrincipal,
                          bool aInPrivateBrowsing);

  static nsDocShell* Cast(nsIDocShell* aDocShell) {
    return static_cast<nsDocShell*>(aDocShell);
  }

  // Returns true if the current load is a force reload (started by holding
  // shift while triggering reload)
  bool IsForceReloading();

  mozilla::dom::WindowProxyHolder GetWindowProxy() {
    EnsureScriptEnvironment();
    return mozilla::dom::WindowProxyHolder(mBrowsingContext);
  }

  /**
   * Loads the given URI. See comments on nsDocShellLoadState members for more
   * information on information used. aDocShell and aRequest come from
   * onLinkClickSync, which is triggered during form submission.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult InternalLoad(nsDocShellLoadState* aLoadState,
                        nsIDocShell** aDocShell, nsIRequest** aRequest);

  // Clear the document's storage access flag if needed.
  void MaybeClearStorageAccessFlag();

  void SetWillChangeProcess() { mWillChangeProcess = true; }

  // Create a content viewer within this nsDocShell for the given
  // `WindowGlobalChild` actor.
  nsresult CreateContentViewerForActor(
      mozilla::dom::WindowGlobalChild* aWindowActor);

  // Creates a real network channel (not a DocumentChannel) using the specified
  // parameters.
  // Used by nsDocShell when not using DocumentChannel, by DocumentLoadListener
  // (parent-process DocumentChannel), and by DocumentChannelChild/ContentChild
  // to transfer the resulting channel into the final process.
  static nsresult CreateRealChannelForDocument(
      nsIChannel** aChannel, nsIURI* aURI, nsILoadInfo* aLoadInfo,
      nsIInterfaceRequestor* aCallbacks, nsLoadFlags aLoadFlags,
      const nsAString& aSrcdoc, nsIURI* aBaseURI);

  // Creates a real (not DocumentChannel) channel, and configures it using the
  // supplied nsDocShellLoadState.
  // Configuration options here are ones that should be applied to only the
  // real channel, especially ones that need to QI to channel subclasses.
  static bool CreateAndConfigureRealChannelForLoadState(
      mozilla::dom::BrowsingContext* aBrowsingContext,
      nsDocShellLoadState* aLoadState, mozilla::net::LoadInfo* aLoadInfo,
      nsIInterfaceRequestor* aCallbacks, nsDocShell* aDocShell,
      const mozilla::OriginAttributes& aOriginAttributes,
      nsLoadFlags aLoadFlags, uint32_t aCacheKey, nsresult& rv,
      nsIChannel** aChannel);

  // Notify consumers of a search being loaded through the observer service:
  static void MaybeNotifyKeywordSearchLoading(const nsString& aProvider,
                                              const nsString& aKeyword);

  nsDocShell* GetInProcessChildAt(int32_t aIndex);

  /**
   * Helper function that finds the last URI and its transition flags for a
   * channel.
   *
   * This method first checks the channel's property bag to see if previous
   * info has been saved. If not, it gives back the referrer of the channel.
   *
   * @param aChannel
   *        The channel we are transitioning to
   * @param aURI
   *        Output parameter with the previous URI, not addref'd
   * @param aChannelRedirectFlags
   *        If a redirect, output parameter with the previous redirect flags
   *        from nsIChannelEventSink
   */
  static void ExtractLastVisit(nsIChannel* aChannel, nsIURI** aURI,
                               uint32_t* aChannelRedirectFlags);

  bool HasContentViewer() const { return !!mContentViewer; }

  static uint32_t ComputeURILoaderFlags(
      mozilla::dom::BrowsingContext* aBrowsingContext, uint32_t aLoadType);

 private:  // member functions
  friend class nsDSURIContentListener;
  friend class FramingChecker;
  friend class OnLinkClickEvent;
  friend class nsIDocShell;
  friend class mozilla::dom::BrowsingContext;

  // It is necessary to allow adding a timeline marker wherever a docshell
  // instance is available. This operation happens frequently and needs to
  // be very fast, so instead of using a Map or having to search for some
  // docshell-specific markers storage, a pointer to an `ObservedDocShell` is
  // is stored on docshells directly.
  friend void mozilla::TimelineConsumers::AddConsumer(nsDocShell*);
  friend void mozilla::TimelineConsumers::RemoveConsumer(nsDocShell*);
  friend void mozilla::TimelineConsumers::AddMarkerForDocShell(
      nsDocShell*, const char*, MarkerTracingType, MarkerStackRequest);
  friend void mozilla::TimelineConsumers::AddMarkerForDocShell(
      nsDocShell*, const char*, const TimeStamp&, MarkerTracingType,
      MarkerStackRequest);
  friend void mozilla::TimelineConsumers::AddMarkerForDocShell(
      nsDocShell*, UniquePtr<AbstractTimelineMarker>&&);
  friend void mozilla::TimelineConsumers::PopMarkers(
      nsDocShell*, JSContext*, nsTArray<dom::ProfileTimelineMarker>&);

  nsDocShell(mozilla::dom::BrowsingContext* aBrowsingContext,
             uint64_t aContentWindowID);

  // Security check to prevent frameset spoofing. See comments at
  // implementation site.
  static bool ValidateOrigin(mozilla::dom::BrowsingContext* aOrigin,
                             mozilla::dom::BrowsingContext* aTarget);

  static inline uint32_t PRTimeToSeconds(PRTime aTimeUsec) {
    return uint32_t(aTimeUsec / PR_USEC_PER_SEC);
  }

  static const nsCString FrameTypeToString(uint32_t aFrameType) {
    switch (aFrameType) {
      case FRAME_TYPE_BROWSER:
        return NS_LITERAL_CSTRING("browser");
      case FRAME_TYPE_REGULAR:
        return NS_LITERAL_CSTRING("regular");
      default:
        NS_ERROR("Unknown frame type");
        return EmptyCString();
    }
  }

  virtual ~nsDocShell();

  //
  // nsDocLoader
  //

  virtual void DestroyChildren() override;

  // Overridden from nsDocLoader, this provides more information than the
  // normal OnStateChange with flags STATE_REDIRECTING
  virtual void OnRedirectStateChange(nsIChannel* aOldChannel,
                                     nsIChannel* aNewChannel,
                                     uint32_t aRedirectFlags,
                                     uint32_t aStateFlags) override;

  // Override the parent setter from nsDocLoader
  virtual nsresult SetDocLoaderParent(nsDocLoader* aLoader) override;

  //
  // Content Viewer Management
  //

  nsresult EnsureContentViewer();

  // aPrincipal can be passed in if the caller wants. If null is
  // passed in, the about:blank principal will end up being used.
  // aCSP, if any, will be used for the new about:blank load.
  nsresult CreateAboutBlankContentViewer(
      nsIPrincipal* aPrincipal, nsIPrincipal* aStoragePrincipal,
      nsIContentSecurityPolicy* aCSP, nsIURI* aBaseURI,
      bool aTryToSaveOldPresentation = true, bool aCheckPermitUnload = true,
      mozilla::dom::WindowGlobalChild* aActor = nullptr);

  nsresult CreateContentViewer(const nsACString& aContentType,
                               nsIRequest* aRequest,
                               nsIStreamListener** aContentHandler);

  nsresult NewContentViewerObj(const nsACString& aContentType,
                               nsIRequest* aRequest, nsILoadGroup* aLoadGroup,
                               nsIStreamListener** aContentHandler,
                               nsIContentViewer** aViewer);

  already_AddRefed<nsILoadURIDelegate> GetLoadURIDelegate();

  nsresult SetupNewViewer(
      nsIContentViewer* aNewViewer,
      mozilla::dom::WindowGlobalChild* aWindowActor = nullptr);

  //
  // Session History
  //

  bool ShouldAddToSessionHistory(nsIURI* aURI, nsIChannel* aChannel);

  // Either aChannel or aOwner must be null. If aChannel is
  // present, the owner should be gotten from it.
  // If aCloneChildren is true, then our current session history's
  // children will be cloned onto the new entry. This should be
  // used when we aren't actually changing the document while adding
  // the new session history entry.
  // aCsp is the CSP to be used for the load. That is *not* the CSP
  // that will be applied to subresource loads within that document
  // but the CSP for the document load itself. E.g. if that CSP
  // includes upgrade-insecure-requests, then the new top-level load
  // will be upgraded to HTTPS.
  nsresult AddToSessionHistory(nsIURI* aURI, nsIChannel* aChannel,
                               nsIPrincipal* aTriggeringPrincipal,
                               nsIPrincipal* aPrincipalToInherit,
                               nsIPrincipal* aStoragePrincipalToInherit,
                               nsIContentSecurityPolicy* aCsp,
                               bool aCloneChildren, nsISHEntry** aNewEntry);

  nsresult AddChildSHEntryToParent(nsISHEntry* aNewEntry, int32_t aChildOffset,
                                   bool aCloneChildren);

  nsresult AddChildSHEntryInternal(nsISHEntry* aCloneRef, nsISHEntry* aNewEntry,
                                   int32_t aChildOffset, uint32_t aLoadType,
                                   bool aCloneChildren);

  // Call this method to swap in a new history entry to m[OL]SHE, rather than
  // setting it directly. This completes the navigation in all docshells
  // in the case of a subframe navigation.
  // Returns old mOSHE/mLSHE.
  already_AddRefed<nsISHEntry> SetHistoryEntry(nsCOMPtr<nsISHEntry>* aPtr,
                                               nsISHEntry* aEntry);

  // This method calls SetHistoryEntry and updates mOSHE and mLSHE in BC to be
  // the same as in docshell
  void SetHistoryEntryAndUpdateBC(const Maybe<nsISHEntry*>& aLSHE,
                                  const Maybe<nsISHEntry*>& aOSHE);

  //
  // URI Load
  //

  // Actually open a channel and perform a URI load. Callers need to pass a
  // non-null aLoadState->TriggeringPrincipal() which initiated the URI load.
  // Please note that the TriggeringPrincipal will be used for performing
  // security checks. If aLoadState->URI() is provided by the web, then please
  // do not pass a SystemPrincipal as the triggeringPrincipal. If
  // aLoadState()->PrincipalToInherit is null, then no inheritance of any sort
  // will happen and the load will get a principal based on the URI being
  // loaded. If the Srcdoc flag is set (INTERNAL_LOAD_FLAGS_IS_SRCDOC), the load
  // will be considered as a srcdoc load, and the contents of Srcdoc will be
  // loaded instead of the URI. aLoadState->OriginalURI() will be set as the
  // originalURI on the channel that does the load. If OriginalURI is null, URI
  // will be set as the originalURI. If LoadReplace is true, LOAD_REPLACE flag
  // will be set on the nsIChannel.
  nsresult DoURILoad(nsDocShellLoadState* aLoadState, nsIDocShell** aDocShell,
                     nsIRequest** aRequest);

  static nsresult AddHeadersToChannel(nsIInputStream* aHeadersData,
                                      nsIChannel* aChannel);

  nsresult DoChannelLoad(nsIChannel* aChannel, nsIURILoader* aURILoader,
                         bool aBypassClassifier);

  nsresult OpenInitializedChannel(nsIChannel* aChannel,
                                  nsIURILoader* aURILoader,
                                  uint32_t aOpenFlags);

  MOZ_CAN_RUN_SCRIPT
  nsresult ScrollToAnchor(bool aCurHasRef, bool aNewHasRef,
                          nsACString& aNewHash, uint32_t aLoadType);

  // Returns true if would have called FireOnLocationChange,
  // but did not because aFireOnLocationChange was false on entry.
  // In this case it is the caller's responsibility to ensure
  // FireOnLocationChange is called.
  // In all other cases false is returned.
  bool OnLoadingSite(nsIChannel* aChannel, bool aFireOnLocationChange,
                     bool aAddToGlobalHistory = true);

  // Returns true if would have called FireOnLocationChange,
  // but did not because aFireOnLocationChange was false on entry.
  // In this case it is the caller's responsibility to ensure
  // FireOnLocationChange is called.
  // In all other cases false is returned.
  // Either aChannel or aTriggeringPrincipal must be null. If aChannel is
  // present, the owner should be gotten from it.
  // If OnNewURI calls AddToSessionHistory, it will pass its
  // aCloneSHChildren argument as aCloneChildren.
  // aCsp is the CSP to be used for the load. That is *not* the CSP
  // that will be applied to subresource loads within that document
  // but the CSP for the document load itself. E.g. if that CSP
  // includes upgrade-insecure-requests, then the new top-level load
  // will be upgraded to HTTPS.
  bool OnNewURI(nsIURI* aURI, nsIChannel* aChannel,
                nsIPrincipal* aTriggeringPrincipal,
                nsIPrincipal* aPrincipalToInherit,
                nsIPrincipal* aStoragePrincipalToInehrit, uint32_t aLoadType,
                nsIContentSecurityPolicy* aCsp, bool aFireOnLocationChange,
                bool aAddToGlobalHistory, bool aCloneSHChildren);

  // Helper method that is called when a new document (including any
  // sub-documents - ie. frames) has been completely loaded.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult EndPageLoad(nsIWebProgress* aProgress, nsIChannel* aChannel,
                       nsresult aResult);

  // Builds an error page URI (e.g. about:neterror?etc) for the given aURI
  // and displays it via the LoadErrorPage() overload below.
  nsresult LoadErrorPage(nsIURI* aURI, const char16_t* aURL,
                         const char* aErrorPage, const char* aErrorType,
                         const char16_t* aDescription, const char* aCSSClass,
                         nsIChannel* aFailedChannel);

  // This method directly loads aErrorURI as an error page. aFailedURI and
  // aFailedChannel come from DisplayLoadError() or the LoadErrorPage() overload
  // above.
  nsresult LoadErrorPage(nsIURI* aErrorURI, nsIURI* aFailedURI,
                         nsIChannel* aFailedChannel);

  bool DisplayLoadError(nsresult aError, nsIURI* aURI, const char16_t* aURL,
                        nsIChannel* aFailedChannel) {
    bool didDisplayLoadError = false;
    DisplayLoadError(aError, aURI, aURL, aFailedChannel, &didDisplayLoadError);
    return didDisplayLoadError;
  }

  //
  // Uncategorized
  //

  // Get the principal that we'll set on the channel if we're inheriting. If
  // aConsiderCurrentDocument is true, we try to use the current document if
  // at all possible. If that fails, we fall back on the parent document.
  // If that fails too, we force creation of a content viewer and use the
  // resulting principal. If aConsiderCurrentDocument is false, we just look
  // at the parent.
  // If aConsiderStoragePrincipal is true, we consider the storage principal
  // instead of the node principal.
  nsIPrincipal* GetInheritedPrincipal(bool aConsiderCurrentDocument,
                                      bool aConsiderStoragePrincipal = false);

  /**
   * Helper function that caches a URI and a transition for saving later.
   *
   * @param aChannel
   *        Channel that will have these properties saved
   * @param aURI
   *        The URI to save for later
   * @param aChannelRedirectFlags
   *        The nsIChannelEventSink redirect flags to save for later
   */
  void SaveLastVisit(nsIChannel* aChannel, nsIURI* aURI,
                     uint32_t aChannelRedirectFlags);

  /**
   * Helper function for adding a URI visit using IHistory.
   *
   * The IHistory API maintains chains of visits, tracking both HTTP referrers
   * and redirects for a user session. VisitURI requires the current URI and
   * the previous URI in the chain.
   *
   * Visits can be saved either during a redirect or when the request has
   * reached its final destination. The previous URI in the visit may be
   * from another redirect.
   *
   * @pre aURI is not null.
   *
   * @param aURI
   *        The URI that was just visited
   * @param aPreviousURI
   *        The previous URI of this visit
   * @param aChannelRedirectFlags
   *        For redirects, the redirect flags from nsIChannelEventSink
   *        (0 otherwise)
   * @param aResponseStatus
   *        For HTTP channels, the response code (0 otherwise).
   */
  void AddURIVisit(nsIURI* aURI, nsIURI* aPreviousURI,
                   uint32_t aChannelRedirectFlags,
                   uint32_t aResponseStatus = 0);

  /**
   * Helper function that will add the redirect chain found in aRedirects using
   * IHistory (see AddURI and SaveLastVisit above for details)
   *
   * @param aChannel
   *        Channel that will have these properties saved
   * @param aURI
   *        The URI that was just visited
   * @param aChannelRedirectFlags
   *        For redirects, the redirect flags from nsIChannelEventSink
   *        (0 otherwise)
   * @param aRedirects
   *        The redirect chain collected by the DocumentChannelParent
   */
  void SavePreviousRedirectsAndLastVisit(
      nsIChannel* aChannel, nsIURI* aURI, uint32_t aChannelRedirectFlags,
      const nsTArray<mozilla::net::DocumentChannelRedirect>& aRedirects);

  already_AddRefed<nsIURIFixupInfo> KeywordToURI(const nsACString& aKeyword,
                                                 bool aIsPrivateContext,
                                                 nsIInputStream** aPostData);

  // Sets the current document's current state object to the given SHEntry's
  // state object. The current state object is eventually given to the page
  // in the PopState event.
  nsresult SetDocCurrentStateObj(nsISHEntry* aShEntry);

  // Returns true if would have called FireOnLocationChange,
  // but did not because aFireOnLocationChange was false on entry.
  // In this case it is the caller's responsibility to ensure
  // FireOnLocationChange is called.
  // In all other cases false is returned.
  bool SetCurrentURI(nsIURI* aURI, nsIRequest* aRequest,
                     bool aFireOnLocationChange, uint32_t aLocationFlags);

  // The following methods deal with saving and restoring content viewers
  // in session history.

  // mContentViewer points to the current content viewer associated with
  // this docshell. When loading a new document, the content viewer is
  // either destroyed or stored into a session history entry. To make sure
  // that destruction happens in a controlled fashion, a given content viewer
  // is always owned in exactly one of these ways:
  //   1) The content viewer is active and owned by a docshell's
  //      mContentViewer.
  //   2) The content viewer is still being displayed while we begin loading
  //      a new document. The content viewer is owned by the _new_
  //      content viewer's mPreviousViewer, and has a pointer to the
  //      nsISHEntry where it will eventually be stored. The content viewer
  //      has been close()d by the docshell, which detaches the document from
  //      the window object.
  //   3) The content viewer is cached in session history. The nsISHEntry
  //      has the only owning reference to the content viewer. The viewer
  //      has released its nsISHEntry pointer to prevent circular ownership.
  //
  // When restoring a content viewer from session history, open() is called
  // to reattach the document to the window object. The content viewer is
  // then placed into mContentViewer and removed from the history entry.
  // (mContentViewer is put into session history as described above, if
  // applicable).

  // Determines whether we can safely cache the current mContentViewer in
  // session history. This checks a number of factors such as cache policy,
  // pending requests, and unload handlers.
  // |aLoadType| should be the load type that will replace the current
  // presentation. |aNewRequest| should be the request for the document to
  // be loaded in place of the current document, or null if such a request
  // has not been created yet. |aNewDocument| should be the document that will
  // replace the current document.
  bool CanSavePresentation(uint32_t aLoadType, nsIRequest* aNewRequest,
                           mozilla::dom::Document* aNewDocument);

  // There are 11 possible reasons to make a request fails to use BFCache
  // (see BFCacheStatus in dom/base/Document.h), and we'd like to record
  // the common combinations for reasons which make requests fail to use
  // BFCache. These combinations are generated based on some local browsings,
  // we need to adjust them when necessary.
  enum BFCacheStatusCombo : uint16_t {
    BFCACHE_SUCCESS,
    SUCCESS_NOT_ONLY_TOPLEVEL =
        mozilla::dom::BFCacheStatus::NOT_ONLY_TOPLEVEL_IN_BCG,
    UNLOAD = mozilla::dom::BFCacheStatus::UNLOAD_LISTENER,
    UNLOAD_REQUEST = mozilla::dom::BFCacheStatus::UNLOAD_LISTENER |
                     mozilla::dom::BFCacheStatus::REQUEST,
    REQUEST = mozilla::dom::BFCacheStatus::REQUEST,
    UNLOAD_REQUEST_PEER = mozilla::dom::BFCacheStatus::UNLOAD_LISTENER |
                          mozilla::dom::BFCacheStatus::REQUEST |
                          mozilla::dom::BFCacheStatus::ACTIVE_PEER_CONNECTION,
    UNLOAD_REQUEST_PEER_MSE =
        mozilla::dom::BFCacheStatus::UNLOAD_LISTENER |
        mozilla::dom::BFCacheStatus::REQUEST |
        mozilla::dom::BFCacheStatus::ACTIVE_PEER_CONNECTION |
        mozilla::dom::BFCacheStatus::CONTAINS_MSE_CONTENT,
    UNLOAD_REQUEST_MSE = mozilla::dom::BFCacheStatus::UNLOAD_LISTENER |
                         mozilla::dom::BFCacheStatus::REQUEST |
                         mozilla::dom::BFCacheStatus::CONTAINS_MSE_CONTENT,
    SUSPENDED_UNLOAD_REQUEST_PEER =
        mozilla::dom::BFCacheStatus::SUSPENDED |
        mozilla::dom::BFCacheStatus::UNLOAD_LISTENER |
        mozilla::dom::BFCacheStatus::REQUEST |
        mozilla::dom::BFCacheStatus::ACTIVE_PEER_CONNECTION,
    REMOTE_SUBFRAMES = mozilla::dom::BFCacheStatus::CONTAINS_REMOTE_SUBFRAMES
  };

  void ReportBFCacheComboTelemetry(uint16_t aCombo);

  // Captures the state of the supporting elements of the presentation
  // (the "window" object, docshell tree, meta-refresh loads, and security
  // state) and stores them on |mOSHE|.
  nsresult CaptureState();

  // Begin the toplevel restore process for |aSHEntry|.
  // This simulates a channel open, and defers the real work until
  // RestoreFromHistory is called from a PLEvent.
  nsresult RestorePresentation(nsISHEntry* aSHEntry, bool* aRestoring);

  // Call BeginRestore(nullptr, false) for each child of this shell.
  nsresult BeginRestoreChildren();

  // Method to get our current position and size without flushing
  void DoGetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aWidth,
                            int32_t* aHeight);

  // Call this when a URI load is handed to us (via OnLinkClick or
  // InternalLoad). This makes sure that we're not inside unload, or that if
  // we are it's still OK to load this URI.
  bool IsOKToLoadURI(nsIURI* aURI);

  // helpers for executing commands
  nsresult GetControllerForCommand(const char* aCommand,
                                   nsIController** aResult);

  // Possibly create a ClientSource object to represent an initial about:blank
  // window that has not been allocated yet.  Normally we try not to create
  // this about:blank window until something calls GetDocument().  We still need
  // the ClientSource to exist for this conceptual window, though.
  //
  // The ClientSource is created with the given principal if specified.  If
  // the principal is not provided we will attempt to inherit it when we
  // are sure it will match what the real about:blank window principal
  // would have been.  There are some corner cases where we cannot easily
  // determine the correct principal and will not create the ClientSource.
  // In these cases the initial about:blank will appear to not exist until
  // its real document and window are created.
  void MaybeCreateInitialClientSource(nsIPrincipal* aPrincipal = nullptr);

  // Determine if a service worker is allowed to control a window in this
  // docshell with the given URL.  If there are any reasons it should not,
  // this will return false.  If true is returned then the window *may* be
  // controlled.  The caller must still consult either the parent controller
  // or the ServiceWorkerManager to determine if a service worker should
  // actually control the window.
  bool ServiceWorkerAllowedToControlWindow(nsIPrincipal* aPrincipal,
                                           nsIURI* aURI);

  // Return the ClientInfo for the initial about:blank window, if it exists
  // or we have speculatively created a ClientSource in
  // MaybeCreateInitialClientSource().  This can return a ClientInfo object
  // even if GetExtantDoc() returns nullptr.
  mozilla::Maybe<mozilla::dom::ClientInfo> GetInitialClientInfo() const;

  /**
   * Initializes mTiming if it isn't yet.
   * After calling this, mTiming is non-null. This method returns true if the
   * initialization of the Timing can be reset (basically this is true if a new
   * Timing object is created).
   * In case the loading is aborted, MaybeResetInitTiming() can be called
   * passing the return value of MaybeInitTiming(): if it's possible to reset
   * the Timing, this method will do it.
   */
  MOZ_MUST_USE bool MaybeInitTiming();
  void MaybeResetInitTiming(bool aReset);

  // Convenience method for getting our parent docshell. Can return null
  already_AddRefed<nsDocShell> GetInProcessParentDocshell();

  // Internal implementation of nsIDocShell::FirePageHideNotification.
  // If aSkipCheckingDynEntries is true, it will not try to remove dynamic
  // subframe entries. This is to avoid redundant RemoveDynEntries calls in all
  // children docshells.
  void FirePageHideNotificationInternal(bool aIsUnload,
                                        bool aSkipCheckingDynEntries);

  nsresult Dispatch(mozilla::TaskCategory aCategory,
                    already_AddRefed<nsIRunnable>&& aRunnable);

  void SetupReferrerInfoFromChannel(nsIChannel* aChannel);
  void SetReferrerInfo(nsIReferrerInfo* aReferrerInfo);
  void ReattachEditorToWindow(nsISHEntry* aSHEntry);
  void RecomputeCanExecuteScripts();
  void ClearFrameHistory(nsISHEntry* aEntry);
  void UpdateGlobalHistoryTitle(nsIURI* aURI);
  bool IsFrame();
  bool CanSetOriginAttributes();
  bool ShouldBlockLoadingForBackButton();
  bool ShouldDiscardLayoutState(nsIHttpChannel* aChannel);
  bool HasUnloadedParent();
  bool JustStartedNetworkLoad();
  bool IsPrintingOrPP(bool aDisplayErrorDialog = true);
  bool IsNavigationAllowed(bool aDisplayPrintErrorDialog = true,
                           bool aCheckIfUnloadFired = true);
  uint32_t GetInheritedFrameType();
  nsIScrollableFrame* GetRootScrollFrame();
  nsIChannel* GetCurrentDocChannel();
  nsresult EnsureScriptEnvironment();
  nsresult EnsureEditorData();
  nsresult EnsureTransferableHookData();
  nsresult EnsureFind();
  nsresult EnsureCommandHandler();
  nsresult RefreshURIFromQueue();
  nsresult Embed(nsIContentViewer* aContentViewer,
                 mozilla::dom::WindowGlobalChild* aWindowActor = nullptr);
  nsPresContext* GetEldestPresContext();
  nsresult CheckLoadingPermissions();
  nsresult PersistLayoutHistoryState();
  nsresult LoadHistoryEntry(nsISHEntry* aEntry, uint32_t aLoadType);
  nsresult GetHttpChannel(nsIChannel* aChannel, nsIHttpChannel** aReturn);
  nsresult ConfirmRepost(bool* aRepost);
  nsresult GetPromptAndStringBundle(nsIPrompt** aPrompt,
                                    nsIStringBundle** aStringBundle);
  nsresult SetCurScrollPosEx(int32_t aCurHorizontalPos,
                             int32_t aCurVerticalPos);
  nsPoint GetCurScrollPos();

  already_AddRefed<mozilla::dom::ChildSHistory> GetRootSessionHistory();

  inline bool UseErrorPages() {
    return (mObserveErrorPages
                ? mozilla::StaticPrefs::browser_xul_error_pages_enabled()
                : mUseErrorPages);
  }

  bool CSSErrorReportingEnabled() const { return mCSSErrorReportingEnabled; }

  // Handles retrieval of subframe session history for nsDocShell::LoadURI. If a
  // load is requested in a subframe of the current DocShell, the subframe
  // loadType may need to reflect the loadType of the parent document, or in
  // some cases (like reloads), the history load may need to be cancelled. See
  // function comments for in-depth logic descriptions.
  void MaybeHandleSubframeHistory(nsDocShellLoadState* aLoadState);

  // If we are passed a named target during InternalLoad, this method handles
  // moving the load to the browsing context the target name resolves to.
  nsresult PerformRetargeting(nsDocShellLoadState* aLoadState,
                              nsIDocShell** aDocShell, nsIRequest** aRequest);

  // Returns one of nsIContentPolicy::TYPE_DOCUMENT,
  // nsIContentPolicy::TYPE_INTERNAL_IFRAME, or
  // nsIContentPolicy::TYPE_INTERNAL_FRAME depending on who is responsible for
  // this docshell.
  uint32_t DetermineContentType();

  // In cases where we have a LoadURIDelegate (loading external links via
  // GeckoView), a load may need to be handled through the delegate. aWindowType
  // is either nsIBrowserDOMWindow::OPEN_CURRENTWINDOW or
  // nsIBrowserDOMWindow::OPEN_NEWWINDOW.
  nsresult MaybeHandleLoadDelegate(nsDocShellLoadState* aLoadState,
                                   uint32_t aWindowType, bool* aDidHandleLoad);

  struct SameDocumentNavigationState {
    nsAutoCString mCurrentHash;
    nsAutoCString mNewHash;
    bool mCurrentURIHasRef = false;
    bool mNewURIHasRef = false;
    bool mSameExceptHashes = false;
    bool mHistoryNavBetweenSameDoc = false;
  };

  // Check to see if we're loading a prior history entry or doing a fragment
  // navigation in the same document.
  bool IsSameDocumentNavigation(nsDocShellLoadState* aLoadState,
                                SameDocumentNavigationState& aState);

  // ... If so, handle the scrolling or other action required instead of
  // continuing with new document navigation.
  MOZ_CAN_RUN_SCRIPT
  nsresult HandleSameDocumentNavigation(nsDocShellLoadState* aLoadState,
                                        SameDocumentNavigationState& aState);

  // Called when the Private Browsing state of a nsDocShell changes.
  void NotifyPrivateBrowsingChanged();

 private:  // data members
#ifdef DEBUG
           // We're counting the number of |nsDocShells| to help find leaks
  static unsigned long gNumberOfDocShells;
#endif /* DEBUG */

  nsID mHistoryID;
  nsString mTitle;
  nsCString mOriginalUriString;
  nsTObserverArray<nsWeakPtr> mPrivacyObservers;
  nsTObserverArray<nsWeakPtr> mReflowObservers;
  nsTObserverArray<nsWeakPtr> mScrollObservers;
  mozilla::UniquePtr<mozilla::dom::ClientSource> mInitialClientSource;
  nsCOMPtr<nsINetworkInterceptController> mInterceptController;
  RefPtr<nsDOMNavigationTiming> mTiming;
  RefPtr<nsDSURIContentListener> mContentListener;
  RefPtr<nsGlobalWindowOuter> mScriptGlobal;
  nsCOMPtr<nsIPrincipal> mParentCharsetPrincipal;
  nsCOMPtr<nsIMutableArray> mRefreshURIList;
  nsCOMPtr<nsIMutableArray> mSavedRefreshURIList;
  uint64_t mContentWindowID;
  nsCOMPtr<nsIContentViewer> mContentViewer;
  nsCOMPtr<nsIWidget> mParentWidget;
  RefPtr<mozilla::dom::ChildSHistory> mSessionHistory;
  nsCOMPtr<nsIWebBrowserFind> mFind;
  RefPtr<nsCommandManager> mCommandManager;
  RefPtr<mozilla::dom::BrowsingContext> mBrowsingContext;

  // Weak reference to our BrowserChild actor.
  nsWeakPtr mBrowserChild;

  // Dimensions of the docshell
  nsIntRect mBounds;

  /**
   * Content-Type Hint of the most-recently initiated load. Used for
   * session history entries.
   */
  nsCString mContentTypeHint;

  // An observed docshell wrapper is created when recording markers is enabled.
  mozilla::UniquePtr<mozilla::ObservedDocShell> mObserved;

  // mCurrentURI should be marked immutable on set if possible.
  nsCOMPtr<nsIURI> mCurrentURI;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;

#ifdef DEBUG
  nsCOMPtr<nsIURI> mLastOpenedURI;
#endif

  // Reference to the SHEntry for this docshell until the page is destroyed.
  // Somebody give me better name
  nsCOMPtr<nsISHEntry> mOSHE;

  // Reference to the SHEntry for this docshell until the page is loaded
  // Somebody give me better name.
  // If mLSHE is non-null, non-pushState subframe loads don't create separate
  // root history entries. That is, frames loaded during the parent page
  // load don't generate history entries the way frame navigation after the
  // parent has loaded does. (This isn't the only purpose of mLSHE.)
  nsCOMPtr<nsISHEntry> mLSHE;

  // Holds a weak pointer to a RestorePresentationEvent object if any that
  // holds a weak pointer back to us. We use this pointer to possibly revoke
  // the event whenever necessary.
  nsRevocableEventPtr<RestorePresentationEvent> mRestorePresentationEvent;

  // Editor data, if this document is designMode or contentEditable.
  mozilla::UniquePtr<nsDocShellEditorData> mEditorData;

  // Secure browser UI object
  nsCOMPtr<nsISecureBrowserUI> mSecurityUI;

  // The URI we're currently loading. This is only relevant during the
  // firing of a pagehide/unload. The caller of FirePageHideNotification()
  // is responsible for setting it and unsetting it. It may be null if the
  // pagehide/unload is happening for some reason other than just loading a
  // new URI.
  nsCOMPtr<nsIURI> mLoadingURI;

  // Our list of ancestor principals.
  nsTArray<nsCOMPtr<nsIPrincipal>> mAncestorPrincipals;

  // Our list of ancestor outerWindowIDs.
  nsTArray<uint64_t> mAncestorOuterWindowIDs;

  // Set in LoadErrorPage from the method argument and used later
  // in CreateContentViewer. We have to delay an shistory entry creation
  // for which these objects are needed.
  nsCOMPtr<nsIURI> mFailedURI;
  nsCOMPtr<nsIChannel> mFailedChannel;

  // Set in DoURILoad when either the LOAD_RELOAD_ALLOW_MIXED_CONTENT flag or
  // the LOAD_NORMAL_ALLOW_MIXED_CONTENT flag is set.
  // Checked in nsMixedContentBlocker, to see if the channels match.
  nsCOMPtr<nsIChannel> mMixedContentChannel;

  mozilla::UniquePtr<mozilla::gfx::Matrix5x4> mColorMatrix;

  const mozilla::Encoding* mForcedCharset;
  const mozilla::Encoding* mParentCharset;

  // WEAK REFERENCES BELOW HERE.
  // Note these are intentionally not addrefd. Doing so will create a cycle.
  // For that reasons don't use nsCOMPtr.

  nsIDocShellTreeOwner* mTreeOwner;  // Weak Reference

  RefPtr<mozilla::dom::EventTarget> mChromeEventHandler;

  mozilla::ScrollbarPreference mScrollbarPref;  // persistent across doc loads

  eCharsetReloadState mCharsetReloadState;

  mozilla::hal::ScreenOrientation mOrientationLock;

  int32_t mParentCharsetSource;
  mozilla::CSSIntSize mFrameMargins;

  // This can either be a content docshell or a chrome docshell.
  const int32_t mItemType;

  // Index into the nsISHEntry array, indicating the previous and current
  // entry at the time that this DocShell begins to load. Consequently
  // root docshell's indices can differ from child docshells'.
  int32_t mPreviousEntryIndex;
  int32_t mLoadedEntryIndex;

  // Offset in the parent's child list.
  // -1 if the docshell is added dynamically to the parent shell.
  int32_t mChildOffset;

  BusyFlags mBusyFlags;
  AppType mAppType;
  uint32_t mLoadType;
  uint32_t mDefaultLoadFlags;
  uint32_t mFailedLoadType;

  // Are we a regular frame, a browser frame, or an app frame?
  FrameType mFrameType;

  // This represents the CSS display-mode we are currently using. This is mostly
  // used for media queries.
  DisplayMode mDisplayMode;

  // A depth count of how many times NotifyRunToCompletionStart
  // has been called without a matching NotifyRunToCompletionStop.
  uint32_t mJSRunToCompletionDepth;

  // Whether or not touch events are overridden. Possible values are defined
  // as constants in the nsIDocShell.idl file.
  TouchEventsOverride mTouchEventsOverride;

  // Whether or not handling of the <meta name="viewport"> tag is overridden.
  // Possible values are defined as constants in nsIDocShell.idl.
  MetaViewportOverride mMetaViewportOverride;

  // mFullscreenAllowed stores how we determine whether fullscreen is allowed
  // when GetFullscreenAllowed() is called. Fullscreen is allowed in a
  // docshell when all containing iframes have the allowfullscreen
  // attribute set to true. When mFullscreenAllowed is CHECK_ATTRIBUTES
  // we check this docshell's containing frame for the allowfullscreen
  // attribute, and recurse onto the parent docshell to ensure all containing
  // frames also have the allowfullscreen attribute. If we find an ancestor
  // docshell with mFullscreenAllowed not equal to CHECK_ATTRIBUTES, we've
  // reached a content boundary, and mFullscreenAllowed denotes whether the
  // parent across the content boundary has allowfullscreen=true in all its
  // containing iframes. mFullscreenAllowed defaults to CHECK_ATTRIBUTES and
  // is set otherwise when docshells which are content boundaries are created.
  enum FullscreenAllowedState : uint8_t {
    CHECK_ATTRIBUTES,
    PARENT_ALLOWS,
    PARENT_PROHIBITS
  };
  FullscreenAllowedState mFullscreenAllowed;

  // The following two fields cannot be declared as bit fields
  // because of uses with AutoRestore.
  bool mCreatingDocument;  // (should be) debugging only
#ifdef DEBUG
  bool mInEnsureScriptEnv;
#endif

  bool mCreated : 1;
  bool mAllowSubframes : 1;
  bool mAllowJavascript : 1;
  bool mAllowMetaRedirects : 1;
  bool mAllowImages : 1;
  bool mAllowMedia : 1;
  bool mAllowDNSPrefetch : 1;
  bool mAllowWindowControl : 1;
  bool mUseErrorPages : 1;
  bool mObserveErrorPages : 1;
  bool mCSSErrorReportingEnabled : 1;
  bool mAllowAuth : 1;
  bool mAllowKeywordFixup : 1;
  bool mIsOffScreenBrowser : 1;
  bool mDisableMetaRefreshWhenInactive : 1;
  bool mIsAppTab : 1;
  bool mUseGlobalHistory : 1;
  bool mDeviceSizeIsPageSize : 1;
  bool mWindowDraggingAllowed : 1;
  bool mInFrameSwap : 1;

  // Because scriptability depends on the mAllowJavascript values of our
  // ancestors, we cache the effective scriptability and recompute it when
  // it might have changed;
  bool mCanExecuteScripts : 1;

  // This boolean is set to true right before we fire pagehide and generally
  // unset when we embed a new content viewer. While it's true no navigation
  // is allowed in this docshell.
  bool mFiredUnloadEvent : 1;

  // this flag is for bug #21358. a docshell may load many urls
  // which don't result in new documents being created (i.e. a new
  // content viewer) we want to make sure we don't call a on load
  // event more than once for a given content viewer.
  bool mEODForCurrentDocument : 1;
  bool mURIResultedInDocument : 1;

  bool mIsBeingDestroyed : 1;

  bool mIsExecutingOnLoadHandler : 1;

  // Indicates that a DocShell in this "docshell tree" is printing
  bool mIsPrintingOrPP : 1;

  // Indicates to CreateContentViewer() that it is safe to cache the old
  // presentation of the page, and to SetupNewViewer() that the old viewer
  // should be passed a SHEntry to save itself into.
  bool mSavingOldViewer : 1;

  // @see nsIDocShellHistory::createdDynamically
  bool mDynamicallyCreated : 1;
  bool mAffectPrivateSessionLifetime : 1;
  bool mInvisible : 1;
  bool mHasLoadedNonBlankURI : 1;

  // This flag means that mTiming has been initialized but nulled out.
  // We will check the innerWin's timing before creating a new one
  // in MaybeInitTiming()
  bool mBlankTiming : 1;

  // This flag indicates when the title is valid for the current URI.
  bool mTitleValidForCurrentURI : 1;

  bool mIsFrame : 1;

  // If mWillChangeProcess is set to true, then when the docshell is destroyed,
  // we prepare the browsing context to change process.
  bool mWillChangeProcess : 1;

  // Set when activity in this docshell is being watched by the developer tools.
  bool mWatchedByDevtools : 1;

  // This flag indicates whether or not the DocShell is currently executing an
  // nsIWebNavigation navigation method.
  bool mIsNavigating : 1;
};

#endif /* nsDocShell_h__ */
