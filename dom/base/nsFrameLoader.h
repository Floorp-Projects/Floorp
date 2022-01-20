/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#ifndef nsFrameLoader_h_
#define nsFrameLoader_h_

#include <cstdint>
#include "ErrorList.h"
#include "Units.h"
#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDocShell.h"
#include "mozilla/dom/MessageManagerCallback.h"
#include "nsID.h"
#include "nsIFrame.h"
#include "nsIMutationObserver.h"
#include "nsISupports.h"
#include "nsRect.h"
#include "nsStringFwd.h"
#include "nsStubMutationObserver.h"
#include "nsWrapperCache.h"

class nsIURI;
class nsSubDocumentFrame;
class AutoResetInShow;
class AutoResetInFrameSwap;
class nsFrameLoaderOwner;
class nsIRemoteTab;
class nsIDocShellTreeItem;
class nsIDocShellTreeOwner;
class nsILoadContext;
class nsIPrintSettings;
class nsIWebBrowserPersistDocumentReceiver;
class nsIWebProgressListener;
class nsIOpenWindowInfo;

namespace mozilla {

class OriginAttributes;

namespace dom {
class ChromeMessageSender;
class ContentParent;
class Document;
class Element;
class TabListener;
class InProcessBrowserChildMessageManager;
class MessageSender;
class ProcessMessageManager;
class BrowserParent;
class MutableTabContext;
class BrowserBridgeChild;
class RemoteBrowser;
struct RemotenessOptions;
struct NavigationIsolationOptions;
class SessionStoreChangeListener;

namespace ipc {
class StructuredCloneData;
}  // namespace ipc

}  // namespace dom

namespace ipc {
class MessageChannel;
}  // namespace ipc
}  // namespace mozilla

#if defined(MOZ_WIDGET_GTK)
typedef struct _GtkWidget GtkWidget;
#endif

// IID for nsFrameLoader, because some places want to QI to it.
#define NS_FRAMELOADER_IID                           \
  {                                                  \
    0x297fd0ea, 0x1b4a, 0x4c9a, {                    \
      0xa4, 0x04, 0xe5, 0x8b, 0xe8, 0x95, 0x10, 0x50 \
    }                                                \
  }

class nsFrameLoader final : public nsStubMutationObserver,
                            public mozilla::dom::ipc::MessageManagerCallback,
                            public nsWrapperCache,
                            public mozilla::LinkedListElement<nsFrameLoader> {
  friend class AutoResetInShow;
  friend class AutoResetInFrameSwap;
  friend class nsFrameLoaderOwner;
  using Document = mozilla::dom::Document;
  using Element = mozilla::dom::Element;
  using BrowserParent = mozilla::dom::BrowserParent;
  using BrowserBridgeChild = mozilla::dom::BrowserBridgeChild;
  using BrowsingContext = mozilla::dom::BrowsingContext;
  using BrowsingContextGroup = mozilla::dom::BrowsingContextGroup;
  using Promise = mozilla::dom::Promise;

 public:
  // Called by Frame Elements to create a new FrameLoader.
  static already_AddRefed<nsFrameLoader> Create(
      Element* aOwner, bool aNetworkCreated,
      nsIOpenWindowInfo* aOpenWindowInfo = nullptr);

  // Called by nsFrameLoaderOwner::ChangeRemoteness when switching out
  // FrameLoaders.
  static already_AddRefed<nsFrameLoader> Recreate(
      Element* aOwner, BrowsingContext* aContext, BrowsingContextGroup* aGroup,
      const mozilla::dom::NavigationIsolationOptions& aRemotenessOptions,
      bool aIsRemote, bool aNetworkCreated, bool aPreserveContext);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_FRAMELOADER_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsFrameLoader)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  nsresult CheckForRecursiveLoad(nsIURI* aURI);
  nsresult ReallyStartLoading();
  void StartDestroy(bool aForProcessSwitch);
  void DestroyDocShell();
  void DestroyComplete();
  nsDocShell* GetExistingDocShell() const { return mDocShell; }
  mozilla::dom::InProcessBrowserChildMessageManager*
  GetBrowserChildMessageManager() const {
    return mChildMessageManager;
  }
  nsresult UpdatePositionAndSize(nsSubDocumentFrame* aIFrame);
  void SendIsUnderHiddenEmbedderElement(bool aIsUnderHiddenEmbedderElement);
  void PropagateIsUnderHiddenEmbedderElement(
      bool aIsUnderHiddenEmbedderElement);

  // When creating a nsFrameLoaderOwner which is a static clone, a
  // `nsFrameLoader` is not immediately attached to it. Instead, it is added to
  // the static clone document's `PendingFrameStaticClones` list.
  //
  // After the parent document has been fully cloned, a new frameloader will be
  // created for the cloned iframe, and `FinishStaticClone` will be called on
  // it, which will clone the inner document of the source nsFrameLoader.
  nsresult FinishStaticClone(nsFrameLoader* aStaticCloneOf,
                             nsIPrintSettings* aPrintSettings,
                             bool* aOutHasInProcessPrintCallbacks);

  nsresult DoRemoteStaticClone(nsFrameLoader* aStaticCloneOf,
                               nsIPrintSettings* aPrintSettings);

  // WebIDL methods

  nsDocShell* GetDocShell(mozilla::ErrorResult& aRv);

  already_AddRefed<nsIRemoteTab> GetRemoteTab();

  already_AddRefed<nsILoadContext> LoadContext();

  mozilla::dom::BrowsingContext* GetBrowsingContext();
  mozilla::dom::BrowsingContext* GetExtantBrowsingContext();
  mozilla::dom::BrowsingContext* GetMaybePendingBrowsingContext() {
    return mPendingBrowsingContext;
  }

  /**
   * Start loading the frame. This method figures out what to load
   * from the owner content in the frame loader.
   */
  void LoadFrame(bool aOriginalSrc);

  /**
   * Loads the specified URI in this frame. Behaves identically to loadFrame,
   * except that this method allows specifying the URI to load.
   *
   * @param aURI The URI to load.
   * @param aTriggeringPrincipal The triggering principal for the load. May be
   *        null, in which case the node principal of the owner content will be
   *        used.
   * @param aCsp The CSP to be used for the load. That is not the CSP to be
   *        applied to subresources within the frame, but to the iframe load
   *        itself. E.g. if the CSP holds upgrade-insecure-requests the the
   *        frame load is upgraded from http to https.
   */
  nsresult LoadURI(nsIURI* aURI, nsIPrincipal* aTriggeringPrincipal,
                   nsIContentSecurityPolicy* aCsp, bool aOriginalSrc);

  /**
   * Resume a redirected load within this frame.
   *
   * @param aPendingSwitchID ID of a process-switching load to be reusmed
   *        within this frame.
   */
  void ResumeLoad(uint64_t aPendingSwitchID);

  /**
   * Destroy the frame loader and everything inside it. This will
   * clear the weak owner content reference.
   */
  void Destroy(bool aForProcessSwitch = false);

  void ActivateRemoteFrame(mozilla::ErrorResult& aRv);

  void DeactivateRemoteFrame(mozilla::ErrorResult& aRv);

  void ActivateFrameEvent(const nsAString& aType, bool aCapture,
                          mozilla::ErrorResult& aRv);

  void RequestUpdatePosition(mozilla::ErrorResult& aRv);

  already_AddRefed<Promise> RequestTabStateFlush(mozilla::ErrorResult& aRv);

  void RequestEpochUpdate(uint32_t aEpoch);

  void RequestSHistoryUpdate();

  already_AddRefed<Promise> PrintPreview(nsIPrintSettings* aPrintSettings,
                                         BrowsingContext* aSourceBC,
                                         mozilla::ErrorResult& aRv);

  void ExitPrintPreview();

  void StartPersistence(BrowsingContext* aContext,
                        nsIWebBrowserPersistDocumentReceiver* aRecv,
                        mozilla::ErrorResult& aRv);

  // WebIDL getters

  already_AddRefed<mozilla::dom::MessageSender> GetMessageManager();

  already_AddRefed<Element> GetOwnerElement();

  uint32_t LazyWidth() const;

  uint32_t LazyHeight() const;

  uint64_t ChildID() const { return mChildID; }

  bool DepthTooGreat() const { return mDepthTooGreat; }

  bool IsDead() const { return mDestroyCalled; }

  bool IsNetworkCreated() const { return mNetworkCreated; }

  /**
   * Is this a frame loader for a bona fide <iframe mozbrowser>?
   * <xul:browser> is not a mozbrowser, so this is false for that case.
   */
  bool OwnerIsMozBrowserFrame();

  nsIContent* GetParentObject() const;

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) override;
  virtual nsresult DoSendAsyncMessage(
      const nsAString& aMessage,
      mozilla::dom::ipc::StructuredCloneData& aData) override;

  /**
   * Called from the layout frame associated with this frame loader;
   * this notifies us to hook up with the widget and view.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool Show(nsSubDocumentFrame*);

  void MaybeShowFrame();

  /**
   * Called when the margin properties of the containing frame are changed.
   */
  void MarginsChanged();

  /**
   * Called from the layout frame associated with this frame loader, when
   * the frame is being torn down; this notifies us that out widget and view
   * are going away and we should unhook from them.
   */
  void Hide();

  // Used when content is causing a FrameLoader to be created, and
  // needs to try forcing layout to flush in order to get accurate
  // dimensions for the content area.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void ForceLayoutIfNecessary();

  // The guts of an nsFrameLoaderOwner::SwapFrameLoader implementation.  A
  // frame loader owner needs to call this, and pass in the two references to
  // nsRefPtrs for frame loaders that need to be swapped.
  nsresult SwapWithOtherLoader(nsFrameLoader* aOther,
                               nsFrameLoaderOwner* aThisOwner,
                               nsFrameLoaderOwner* aOtherOwner);

  nsresult SwapWithOtherRemoteLoader(nsFrameLoader* aOther,
                                     nsFrameLoaderOwner* aThisOwner,
                                     nsFrameLoaderOwner* aOtherOwner);

  /**
   * Return the primary frame for our owning content, or null if it
   * can't be found.
   */
  nsIFrame* GetPrimaryFrameOfOwningContent() const;

  /**
   * Return the document that owns this, or null if we don't have
   * an owner.
   */
  Document* GetOwnerDoc() const;

  /**
   * Returns whether this frame is a remote frame.
   *
   * This is true for either a top-level remote browser in the parent process,
   * or a remote subframe in the child process.
   */
  bool IsRemoteFrame();

  mozilla::dom::RemoteBrowser* GetRemoteBrowser() const;

  /**
   * Returns the IPDL actor used if this is a top-level remote browser, or null
   * otherwise.
   */
  BrowserParent* GetBrowserParent() const;

  /**
   * Returns the IPDL actor used if this is an out-of-process iframe, or null
   * otherwise.
   */
  BrowserBridgeChild* GetBrowserBridgeChild() const;

  /**
   * Returns the layers ID that this remote frame is using to render.
   *
   * This must only be called if this is a remote frame.
   */
  mozilla::layers::LayersId GetLayersId() const;

  mozilla::dom::ChromeMessageSender* GetFrameMessageManager() {
    return mMessageManager;
  }

  mozilla::dom::Element* GetOwnerContent() { return mOwnerContent; }

  /**
   * Stashes a detached nsIFrame on the frame loader. We do this when we're
   * destroying the nsSubDocumentFrame. If the nsSubdocumentFrame is
   * being reframed we'll restore the detached nsIFrame when it's recreated,
   * otherwise we'll discard the old presentation and set the detached
   * subdoc nsIFrame to null. aContainerDoc is the document containing the
   * the subdoc frame. This enables us to detect when the containing
   * document has changed during reframe, so we can discard the presentation
   * in that case.
   */
  void SetDetachedSubdocFrame(nsIFrame* aDetachedFrame,
                              Document* aContainerDoc);

  /**
   * Retrieves the detached nsIFrame and the document containing the nsIFrame,
   * as set by SetDetachedSubdocFrame().
   */
  nsIFrame* GetDetachedSubdocFrame(Document** aContainerDoc) const;

  /**
   * Applies a new set of sandbox flags. These are merged with the sandbox
   * flags from our owning content's owning document with a logical OR, this
   * ensures that we can only add restrictions and never remove them.
   */
  void ApplySandboxFlags(uint32_t sandboxFlags);

  void GetURL(nsString& aURL, nsIPrincipal** aTriggeringPrincipal,
              nsIContentSecurityPolicy** aCsp);

  // Properly retrieves documentSize of any subdocument type.
  nsresult GetWindowDimensions(nsIntRect& aRect);

  virtual mozilla::dom::ProcessMessageManager* GetProcessMessageManager()
      const override;

  // public because a callback needs these.
  RefPtr<mozilla::dom::ChromeMessageSender> mMessageManager;
  RefPtr<mozilla::dom::InProcessBrowserChildMessageManager>
      mChildMessageManager;

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void SetWillChangeProcess();

  // Configure which remote process should be used to host the remote browser
  // created in `TryRemoteBrowser`. This method _must_ be called before
  // `TryRemoteBrowser`, and a script blocker must be on the stack.
  //
  // |aContentParent|, if set, must have the remote type |aRemoteType|.
  void ConfigRemoteProcess(const nsACString& aRemoteType,
                           mozilla::dom::ContentParent* aContentParent);

  void MaybeNotifyCrashed(mozilla::dom::BrowsingContext* aBrowsingContext,
                          mozilla::dom::ContentParentId aChildID,
                          mozilla::ipc::MessageChannel* aChannel);

  void FireErrorEvent();

 private:
  nsFrameLoader(mozilla::dom::Element* aOwner,
                mozilla::dom::BrowsingContext* aBrowsingContext, bool aIsRemote,
                bool aNetworkCreated);
  ~nsFrameLoader();

  void SetOwnerContent(mozilla::dom::Element* aContent);

  /**
   * Get our owning element's app manifest URL, or return the empty string if
   * our owning element doesn't have an app manifest URL.
   */
  void GetOwnerAppManifestURL(nsAString& aOut);

  /**
   * If we are an IPC frame, set mRemoteFrame. Otherwise, create and
   * initialize mDocShell.
   */
  nsresult MaybeCreateDocShell();
  nsresult EnsureMessageManager();
  nsresult ReallyLoadFrameScripts();
  nsDocShell* GetDocShell() const { return mDocShell; }

  void AssertSafeToInit();

  // Updates the subdocument position and size. This gets called only
  // when we have our own in-process DocShell.
  void UpdateBaseWindowPositionAndSize(nsSubDocumentFrame* aIFrame);

  /**
   * Checks whether a load of the given URI should be allowed, and returns an
   * error result if it should not.
   *
   * @param aURI The URI to check.
   * @param aTriggeringPrincipal The triggering principal for the load. May be
   *        null, in which case the node principal of the owner content is used.
   */
  nsresult CheckURILoad(nsIURI* aURI, nsIPrincipal* aTriggeringPrincipal);
  nsresult ReallyStartLoadingInternal();

  // Returns true if we have a remote browser or else attempts to create a
  // remote browser and returns true if successful.
  bool EnsureRemoteBrowser();

  // Return true if remote browser created; nothing else to do
  bool TryRemoteBrowser();
  bool TryRemoteBrowserInternal();

  // Tell the remote browser that it's now "virtually visible"
  bool ShowRemoteFrame(const mozilla::ScreenIntSize& size,
                       nsSubDocumentFrame* aFrame = nullptr);

  void AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem,
                              nsIDocShellTreeOwner* aOwner);

  void InitializeBrowserAPI();
  void DestroyBrowserFrameScripts();

  nsresult GetNewTabContext(mozilla::dom::MutableTabContext* aTabContext,
                            nsIURI* aURI = nullptr);

  enum BrowserParentChange { eBrowserParentRemoved, eBrowserParentChanged };
  void MaybeUpdatePrimaryBrowserParent(BrowserParentChange aChange);

  nsresult PopulateOriginContextIdsFromAttributes(
      mozilla::OriginAttributes& aAttr);

  bool EnsureBrowsingContextAttached();

  // Invoke the callback from nsOpenWindowInfo to indicate that a
  // browsing context for a newly opened tab/window is ready.
  void InvokeBrowsingContextReadyCallback();

  void RequestFinalTabStateFlush();

  RefPtr<mozilla::dom::BrowsingContext> mPendingBrowsingContext;
  nsCOMPtr<nsIURI> mURIToLoad;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;
  nsCOMPtr<nsIOpenWindowInfo> mOpenWindowInfo;
  mozilla::dom::Element* mOwnerContent;  // WEAK

  // After the frameloader has been removed from the DOM but before all of the
  // messages from the frame have been received, we keep a strong reference to
  // our <browser> element.
  RefPtr<mozilla::dom::Element> mOwnerContentStrong;

  // Stores the root frame of the subdocument while the subdocument is being
  // reframed. Used to restore the presentation after reframing.
  WeakFrame mDetachedSubdocFrame;
  // Stores the containing document of the frame corresponding to this
  // frame loader. This is reference is kept valid while the subframe's
  // presentation is detached and stored in mDetachedSubdocFrame. This
  // enables us to detect whether the frame has moved documents during
  // a reframe, so that we know not to restore the presentation.
  RefPtr<Document> mContainerDocWhileDetached;

  // When performing a process switch, this value is used rather than mURIToLoad
  // to identify the process-switching load which should be resumed in the
  // target process.
  uint64_t mPendingSwitchID;

  uint64_t mChildID;
  RefPtr<mozilla::dom::RemoteBrowser> mRemoteBrowser;
  RefPtr<nsDocShell> mDocShell;

  // Holds the last known size of the frame.
  mozilla::ScreenIntSize mLazySize;

  RefPtr<mozilla::dom::TabListener> mSessionStoreListener;

  RefPtr<mozilla::dom::SessionStoreChangeListener> mSessionStoreChangeListener;

  nsCString mRemoteType;

  bool mInitialized : 1;
  bool mDepthTooGreat : 1;
  bool mIsTopLevelContent : 1;
  bool mDestroyCalled : 1;
  bool mNeedsAsyncDestroy : 1;
  bool mInSwap : 1;
  bool mInShow : 1;
  bool mHideCalled : 1;
  // True when the object is created for an element which the parser has
  // created using NS_FROM_PARSER_NETWORK flag. If the element is modified,
  // it may lose the flag.
  bool mNetworkCreated : 1;

  // True if a pending load corresponds to the original src (or srcdoc)
  // attribute of the frame element.
  bool mLoadingOriginalSrc : 1;

  bool mRemoteBrowserShown : 1;
  bool mIsRemoteFrame : 1;
  // If true, the FrameLoader will be re-created with the same BrowsingContext,
  // but for a different process, after it is destroyed.
  bool mWillChangeProcess : 1;
  bool mObservingOwnerContent : 1;

  // When an out-of-process nsFrameLoader crashes, an event is fired on the
  // frame. To ensure this is only fired once, this bit is checked.
  bool mTabProcessCrashFired : 1;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsFrameLoader, NS_FRAMELOADER_IID)

inline nsISupports* ToSupports(nsFrameLoader* aFrameLoader) {
  return aFrameLoader;
}

#endif
