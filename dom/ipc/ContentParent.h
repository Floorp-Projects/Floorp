/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentParent_h
#define mozilla_dom_ContentParent_h

#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/MessageManagerCallback.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/dom/JSProcessActorParent.h"
#include "mozilla/dom/ProcessActor.h"
#include "mozilla/gfx/gfxVarReceiver.h"
#include "mozilla/gfx/GPUProcessListener.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/PParentToChildStreamParent.h"
#include "mozilla/ipc/PChildToParentStreamParent.h"
#include "mozilla/Attributes.h"
#include "mozilla/DataMutex.h"
#include "mozilla/FileUtils.h"
#include "mozilla/HalTypes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/MozPromise.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "nsClassHashtable.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"
#include "nsPluginTags.h"
#include "nsHashKeys.h"
#include "nsIAsyncShutdown.h"
#include "nsIDOMProcessParent.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIRemoteTab.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDOMGeoPositionErrorCallback.h"
#include "nsRefPtrHashtable.h"
#include "PermissionMessageUtils.h"
#include "DriverCrashGuard.h"
#include "nsIReferrerInfo.h"

#define CHILD_PROCESS_SHUTDOWN_MESSAGE u"child-process-shutdown"_ns

class nsConsoleService;
class nsIContentProcessInfo;
class nsICycleCollectorLogSink;
class nsIDumpGCAndCCLogsCallback;
class nsIRemoteTab;
class nsITimer;
class ParentIdleListener;
class nsIWidget;

namespace mozilla {
class PRemoteSpellcheckEngineParent;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
class SandboxBroker;
class SandboxBrokerPolicyFactory;
#endif

class PreallocatedProcessManagerImpl;
class BenchmarkStorageParent;

using mozilla::loader::PScriptCacheParent;

namespace embedding {
class PrintingParent;
}

namespace ipc {
class CrashReporterHost;
class PFileDescriptorSetParent;
class TestShellParent;
#ifdef FUZZING
class ProtocolFuzzerHelper;
#endif
class SharedPreferenceSerializer;
}  // namespace ipc

namespace layers {
struct TextureFactoryIdentifier;
}  // namespace layers

namespace dom {

class BrowsingContextGroup;
class Element;
class BrowserParent;
class ClonedMessageData;
class MemoryReport;
class TabContext;
class GetFilesHelper;
class MemoryReportRequestHost;
class RemoteWorkerManager;
struct CancelContentJSOptions;

#define NS_CONTENTPARENT_IID                         \
  {                                                  \
    0xeeec9ebf, 0x8ecf, 0x4e38, {                    \
      0x81, 0xda, 0xb7, 0x34, 0x13, 0x7e, 0xac, 0xf3 \
    }                                                \
  }

class ContentParent final
    : public PContentParent,
      public nsIDOMProcessParent,
      public nsIObserver,
      public nsIDOMGeoPositionCallback,
      public nsIDOMGeoPositionErrorCallback,
      public nsIAsyncShutdownBlocker,
      public nsIInterfaceRequestor,
      public gfx::gfxVarReceiver,
      public mozilla::LinkedListElement<ContentParent>,
      public gfx::GPUProcessListener,
      public mozilla::MemoryReportingProcess,
      public mozilla::dom::ipc::MessageManagerCallback,
      public mozilla::ipc::IShmemAllocator,
      public mozilla::ipc::ParentToChildStreamActorManager,
      public ProcessActor {
  typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;
  typedef mozilla::ipc::PFileDescriptorSetParent PFileDescriptorSetParent;
  typedef mozilla::ipc::TestShellParent TestShellParent;
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
  typedef mozilla::dom::BrowsingContextGroup BrowsingContextGroup;

  friend class mozilla::PreallocatedProcessManagerImpl;
  friend class PContentParent;
  friend class mozilla::dom::RemoteWorkerManager;
#ifdef FUZZING
  friend class mozilla::ipc::ProtocolFuzzerHelper;
#endif

 public:
  using LaunchError = mozilla::ipc::LaunchError;
  using LaunchPromise =
      mozilla::MozPromise<RefPtr<ContentParent>, LaunchError, false>;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CONTENTPARENT_IID)

  static LogModule* GetLog();

  /**
   * Create a ContentParent suitable for use later as a content process.
   */
  static already_AddRefed<ContentParent> MakePreallocProcess();

  /**
   * Start up the content-process machinery.  This might include
   * scheduling pre-launch tasks.
   */
  static void StartUp();

  /** Shut down the content-process machinery. */
  static void ShutDown();

  static uint32_t GetPoolSize(const nsACString& aContentProcessType);

  static uint32_t GetMaxProcessCount(const nsACString& aContentProcessType);

  static bool IsMaxProcessCountReached(const nsACString& aContentProcessType);

  static void ReleaseCachedProcesses();

  static void LogAndAssertFailedPrincipalValidationInfo(
      nsIPrincipal* aPrincipal, const char* aMethod);

  /**
   * Picks a random content parent from |aContentParents| respecting the index
   * limit set by |aMaxContentParents|.
   * Returns null if non available.
   */
  static already_AddRefed<ContentParent> MinTabSelect(
      const nsTArray<ContentParent*>& aContentParents,
      int32_t maxContentParents);

  /**
   * Get or create a content process for:
   * 1. browser iframe
   * 2. remote xul <browser>
   * 3. normal iframe
   */
  static RefPtr<ContentParent::LaunchPromise> GetNewOrUsedBrowserProcessAsync(
      const nsACString& aRemoteType, BrowsingContextGroup* aGroup = nullptr,
      hal::ProcessPriority aPriority =
          hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND,
      bool aPreferUsed = false);
  static already_AddRefed<ContentParent> GetNewOrUsedBrowserProcess(
      const nsACString& aRemoteType, BrowsingContextGroup* aGroup = nullptr,
      hal::ProcessPriority aPriority =
          hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND,
      bool aPreferUsed = false);

  /**
   * Get or create a content process, but without waiting for the process
   * launch to have completed. The returned `ContentParent` may still be in the
   * "Launching" state.
   *
   * Can return `nullptr` in the case of an error.
   *
   * Use the `WaitForLaunchAsync` or `WaitForLaunchSync` methods to wait for
   * the process to be fully launched.
   */
  static already_AddRefed<ContentParent> GetNewOrUsedLaunchingBrowserProcess(
      const nsACString& aRemoteType, BrowsingContextGroup* aGroup = nullptr,
      hal::ProcessPriority aPriority =
          hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND,
      bool aPreferUsed = false);

  RefPtr<ContentParent::LaunchPromise> WaitForLaunchAsync(
      hal::ProcessPriority aPriority =
          hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND);
  bool WaitForLaunchSync(hal::ProcessPriority aPriority =
                             hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND);

  /**
   * Get or create a content process for a JS plugin. aPluginID is the id of the
   * JS plugin
   * (@see nsFakePlugin::mId). There is a maximum of one process per JS plugin.
   */
  static already_AddRefed<ContentParent> GetNewOrUsedJSPluginProcess(
      uint32_t aPluginID, const hal::ProcessPriority& aPriority);

  /**
   * Get or create a content process for the given TabContext.  aFrameElement
   * should be the frame/iframe element with which this process will
   * associated.
   */
  static already_AddRefed<RemoteBrowser> CreateBrowser(
      const TabContext& aContext, Element* aFrameElement,
      const nsACString& aRemoteType, BrowsingContext* aBrowsingContext,
      ContentParent* aOpenerContentParent);

  /**
   * Get all content parents.
   *
   * # Lifetime
   *
   * These pointers are ONLY valid for synchronous use from the main thread.
   *
   * Do NOT attempt to use them after the main thread has had a chance to handle
   * messages or you could end up with dangling pointers.
   */
  static void GetAll(nsTArray<ContentParent*>& aArray);

  static void GetAllEvenIfDead(nsTArray<ContentParent*>& aArray);

  static void BroadcastStringBundle(const StringBundleDescriptor&);

  static void BroadcastFontListChanged();
  static void BroadcastShmBlockAdded(uint32_t aGeneration, uint32_t aIndex);

  static void BroadcastThemeUpdate(widget::ThemeChangeKind);

  static void BroadcastMediaCodecsSupportedUpdate(
      RemoteDecodeIn aLocation,
      const PDMFactory::MediaCodecsSupported& aSupported);

  const nsACString& GetRemoteType() const override;

  virtual void DoGetRemoteType(nsACString& aRemoteType,
                               ErrorResult& aError) const override {
    aRemoteType = GetRemoteType();
  }

  enum CPIteratorPolicy { eLive, eAll };

  class ContentParentIterator {
   private:
    ContentParent* mCurrent;
    CPIteratorPolicy mPolicy;

   public:
    ContentParentIterator(CPIteratorPolicy aPolicy, ContentParent* aCurrent)
        : mCurrent(aCurrent), mPolicy(aPolicy) {}

    ContentParentIterator begin() {
      // Move the cursor to the first element that matches the policy.
      while (mPolicy != eAll && mCurrent && !mCurrent->IsAlive()) {
        mCurrent = mCurrent->LinkedListElement<ContentParent>::getNext();
      }

      return *this;
    }
    ContentParentIterator end() {
      return ContentParentIterator(mPolicy, nullptr);
    }

    const ContentParentIterator& operator++() {
      MOZ_ASSERT(mCurrent);
      do {
        mCurrent = mCurrent->LinkedListElement<ContentParent>::getNext();
      } while (mPolicy != eAll && mCurrent && !mCurrent->IsAlive());

      return *this;
    }

    bool operator!=(const ContentParentIterator& aOther) {
      MOZ_ASSERT(mPolicy == aOther.mPolicy);
      return mCurrent != aOther.mCurrent;
    }

    ContentParent* operator*() { return mCurrent; }
  };

  static ContentParentIterator AllProcesses(CPIteratorPolicy aPolicy) {
    ContentParent* first =
        sContentParents ? sContentParents->getFirst() : nullptr;
    return ContentParentIterator(aPolicy, first);
  }

  static void NotifyUpdatedDictionaries();

  // Tell content processes the font list has changed. If aFullRebuild is true,
  // the shared list has been rebuilt and must be freshly mapped by child
  // processes; if false, existing mappings are still valid but the data has
  // been updated and so full reflows are in order.
  static void NotifyUpdatedFonts(bool aFullRebuild);

#if defined(XP_WIN)
  /**
   * Windows helper for firing off an update window request to a plugin
   * instance.
   *
   * aWidget - the eWindowType_plugin_ipc_chrome widget associated with
   *           this plugin window.
   */
  static void SendAsyncUpdate(nsIWidget* aWidget);
#endif

  mozilla::ipc::IPCResult RecvCreateGMPService();

  mozilla::ipc::IPCResult RecvUngrabPointer(const uint32_t& aTime);

  mozilla::ipc::IPCResult RecvRemovePermission(const IPC::Principal& aPrincipal,
                                               const nsCString& aPermissionType,
                                               nsresult* aRv);

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ContentParent, nsIObserver)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMPROCESSPARENT
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMGEOPOSITIONCALLBACK
  NS_DECL_NSIDOMGEOPOSITIONERRORCALLBACK
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER
  NS_DECL_NSIINTERFACEREQUESTOR

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) override;

  virtual nsresult DoSendAsyncMessage(const nsAString& aMessage,
                                      StructuredCloneData& aData) override;

  /** Notify that a tab is beginning its destruction sequence. */
  void NotifyTabDestroying();

  /** Notify that a tab was destroyed during normal operation. */
  void NotifyTabDestroyed(const TabId& aTabId, bool aNotifiedDestroying);

  // Manage the set of `KeepAlive`s on this ContentParent which are preventing
  // it from being destroyed.
  void AddKeepAlive();
  void RemoveKeepAlive();

  TestShellParent* CreateTestShell();

  bool DestroyTestShell(TestShellParent* aTestShell);

  TestShellParent* GetTestShellSingleton();

  // This method can be called on any thread.
  void RegisterRemoteWorkerActor();

  // This method _must_ be called on main-thread because it can start the
  // shutting down of the content process.
  void UnregisterRemoveWorkerActor();

  void ReportChildAlreadyBlocked();

  bool RequestRunToCompletion();

  void UpdateCookieStatus(nsIChannel* aChannel);

  bool IsLaunching() const {
    return mLifecycleState == LifecycleState::LAUNCHING;
  }
  bool IsAlive() const override;
  bool IsInitialized() const;
  bool IsDead() const { return mLifecycleState == LifecycleState::DEAD; }

  bool IsForBrowser() const { return mIsForBrowser; }
  bool IsForJSPlugin() const {
    return mJSPluginID != nsFakePluginTag::NOT_JSPLUGIN;
  }

  GeckoChildProcessHost* Process() const { return mSubprocess; }

  nsIContentProcessInfo* ScriptableHelper() const { return mScriptableHelper; }

  mozilla::dom::ProcessMessageManager* GetMessageManager() const {
    return mMessageManager;
  }

  bool NeedsPermissionsUpdate(const nsACString& aPermissionKey) const;

  /**
   * Kill our subprocess and make sure it dies.  Should only be used
   * in emergency situations since it bypasses the normal shutdown
   * process.
   *
   * WARNING: aReason appears in telemetry, so any new value passed in requires
   * data review.
   */
  void KillHard(const char* aWhy);

  ContentParentId ChildID() const { return mChildID; }

  /**
   * Get a user-friendly name for this ContentParent.  We make no guarantees
   * about this name: It might not be unique, apps can spoof special names,
   * etc.  So please don't use this name to make any decisions about the
   * ContentParent based on the value returned here.
   */
  void FriendlyName(nsAString& aName, bool aAnonymize = false);

  virtual void OnChannelError() override;

  mozilla::ipc::IPCResult RecvInitCrashReporter(
      const NativeThreadId& aThreadId);

  PNeckoParent* AllocPNeckoParent();

  virtual mozilla::ipc::IPCResult RecvPNeckoConstructor(
      PNeckoParent* aActor) override {
    return PContentParent::RecvPNeckoConstructor(aActor);
  }

  PPrintingParent* AllocPPrintingParent();

  bool DeallocPPrintingParent(PPrintingParent* aActor);

#if defined(NS_PRINTING)
  /**
   * @return the PrintingParent for this ContentParent.
   */
  already_AddRefed<embedding::PrintingParent> GetPrintingParent();
#endif

  mozilla::ipc::IPCResult RecvInitStreamFilter(
      const uint64_t& aChannelId, const nsString& aAddonId,
      InitStreamFilterResolver&& aResolver);

  PChildToParentStreamParent* AllocPChildToParentStreamParent();
  bool DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor);

  PParentToChildStreamParent* AllocPParentToChildStreamParent();
  bool DeallocPParentToChildStreamParent(PParentToChildStreamParent* aActor);

  PHalParent* AllocPHalParent();

  virtual mozilla::ipc::IPCResult RecvPHalConstructor(
      PHalParent* aActor) override {
    return PContentParent::RecvPHalConstructor(aActor);
  }

  PHeapSnapshotTempFileHelperParent* AllocPHeapSnapshotTempFileHelperParent();

  PRemoteSpellcheckEngineParent* AllocPRemoteSpellcheckEngineParent();

  mozilla::ipc::IPCResult RecvRecordingDeviceEvents(
      const nsString& aRecordingStatus, const nsString& aPageURL,
      const bool& aIsAudio, const bool& aIsVideo);

  bool CycleCollectWithLogs(bool aDumpAllTraces,
                            nsICycleCollectorLogSink* aSink,
                            nsIDumpGCAndCCLogsCallback* aCallback);

  mozilla::ipc::IPCResult RecvNotifyTabDestroying(const TabId& aTabId,
                                                  const ContentParentId& aCpId);

  mozilla::ipc::IPCResult RecvSetOfflinePermission(
      const IPC::Principal& principal);

  mozilla::ipc::IPCResult RecvFinishShutdown();

  void MaybeInvokeDragSession(BrowserParent* aParent);

  PContentPermissionRequestParent* AllocPContentPermissionRequestParent(
      const nsTArray<PermissionRequest>& aRequests,
      const IPC::Principal& aPrincipal,
      const IPC::Principal& aTopLevelPrincipal,
      const bool& aIsHandlingUserInput,
      const bool& aMaybeUnsafePermissionDelegate, const TabId& aTabId);

  bool DeallocPContentPermissionRequestParent(
      PContentPermissionRequestParent* actor);

  virtual bool HandleWindowsMessages(const Message& aMsg) const override;

  void ForkNewProcess(bool aBlocking);

  mozilla::ipc::IPCResult RecvCreateWindow(
      PBrowserParent* aThisBrowserParent,
      const MaybeDiscarded<BrowsingContext>& aParent, PBrowserParent* aNewTab,
      const uint32_t& aChromeFlags, const bool& aCalledFromJS,
      const bool& aWidthSpecified, const bool& aForPrinting,
      const bool& aForWindowDotPrint, nsIURI* aURIToLoad,
      const nsCString& aFeatures, const float& aFullZoom,
      const IPC::Principal& aTriggeringPrincipal,
      nsIContentSecurityPolicy* aCsp, nsIReferrerInfo* aReferrerInfo,
      const OriginAttributes& aOriginAttributes,
      CreateWindowResolver&& aResolve);

  mozilla::ipc::IPCResult RecvCreateWindowInDifferentProcess(
      PBrowserParent* aThisTab, const MaybeDiscarded<BrowsingContext>& aParent,
      const uint32_t& aChromeFlags, const bool& aCalledFromJS,
      const bool& aWidthSpecified, nsIURI* aURIToLoad,
      const nsCString& aFeatures, const float& aFullZoom, const nsString& aName,
      nsIPrincipal* aTriggeringPrincipal, nsIContentSecurityPolicy* aCsp,
      nsIReferrerInfo* aReferrerInfo,
      const OriginAttributes& aOriginAttributes);

  static void BroadcastBlobURLRegistration(
      const nsACString& aURI, BlobImpl* aBlobImpl, nsIPrincipal* aPrincipal,
      const Maybe<nsID>& aAgentClusterId,
      ContentParent* aIgnoreThisCP = nullptr);

  static void BroadcastBlobURLUnregistration(
      const nsACString& aURI, nsIPrincipal* aPrincipal,
      ContentParent* aIgnoreThisCP = nullptr);

  mozilla::ipc::IPCResult RecvStoreAndBroadcastBlobURLRegistration(
      const nsCString& aURI, const IPCBlob& aBlob, const Principal& aPrincipal,
      const Maybe<nsID>& aAgentCluster);

  mozilla::ipc::IPCResult RecvUnstoreAndBroadcastBlobURLUnregistration(
      const nsCString& aURI, const Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvGetA11yContentId(uint32_t* aContentId);

  mozilla::ipc::IPCResult RecvA11yHandlerControl(
      const uint32_t& aPid, const IHandlerControlHolder& aHandlerControl);

  virtual int32_t Pid() const override;

  // PURLClassifierParent.
  PURLClassifierParent* AllocPURLClassifierParent(const Principal& aPrincipal,
                                                  bool* aSuccess);
  virtual mozilla::ipc::IPCResult RecvPURLClassifierConstructor(
      PURLClassifierParent* aActor, const Principal& aPrincipal,
      bool* aSuccess) override;

  // PURLClassifierLocalParent.
  PURLClassifierLocalParent* AllocPURLClassifierLocalParent(
      nsIURI* aURI, const nsTArray<IPCURLClassifierFeature>& aFeatures);

  virtual mozilla::ipc::IPCResult RecvPURLClassifierLocalConstructor(
      PURLClassifierLocalParent* aActor, nsIURI* aURI,
      nsTArray<IPCURLClassifierFeature>&& aFeatures) override;

  PLoginReputationParent* AllocPLoginReputationParent(nsIURI* aURI);

  virtual mozilla::ipc::IPCResult RecvPLoginReputationConstructor(
      PLoginReputationParent* aActor, nsIURI* aURI) override;

  bool DeallocPLoginReputationParent(PLoginReputationParent* aActor);

  PSessionStorageObserverParent* AllocPSessionStorageObserverParent();

  virtual mozilla::ipc::IPCResult RecvPSessionStorageObserverConstructor(
      PSessionStorageObserverParent* aActor) override;

  bool DeallocPSessionStorageObserverParent(
      PSessionStorageObserverParent* aActor);

  bool DeallocPURLClassifierLocalParent(PURLClassifierLocalParent* aActor);

  bool DeallocPURLClassifierParent(PURLClassifierParent* aActor);

  // Use the PHangMonitor channel to ask the child to repaint a tab.
  void PaintTabWhileInterruptingJS(BrowserParent* aBrowserParent,
                                   const layers::LayersObserverEpoch& aEpoch);

  void CancelContentJSExecutionIfRunning(
      BrowserParent* aBrowserParent,
      nsIRemoteTab::NavigationType aNavigationType,
      const CancelContentJSOptions& aCancelContentJSOptions);

  // This function is called when we are about to load a document from an
  // HTTP(S) or FTP channel for a content process.  It is a useful place
  // to start to kick off work as early as possible in response to such
  // document loads.
  // aShouldWaitForPermissionCookieUpdate is set to true if main thread IPCs for
  // updating permissions/cookies are sent.
  nsresult AboutToLoadHttpFtpDocumentForChild(
      nsIChannel* aChannel,
      bool* aShouldWaitForPermissionCookieUpdate = nullptr);

  // Send Blob URLs for this aPrincipal if they are not already known to this
  // content process and mark the process to receive any new/revoked Blob URLs
  // to this content process forever.
  void TransmitBlobURLsForPrincipal(nsIPrincipal* aPrincipal);

  nsresult TransmitPermissionsForPrincipal(nsIPrincipal* aPrincipal);

  // Whenever receiving a Principal we need to validate that Principal case
  // by case, where we grant individual callsites to customize the checks!
  enum class ValidatePrincipalOptions {
    AllowNullPtr,  // Not a NullPrincipal but a nullptr as Principal.
    AllowSystem,
    AllowExpanded,
  };
  bool ValidatePrincipal(
      nsIPrincipal* aPrincipal,
      const EnumSet<ValidatePrincipalOptions>& aOptions = {});

  // This function is called in BrowsingContext immediately before IPC call to
  // load a URI. If aURI is a BlobURL, this method transmits all BlobURLs for
  // aURI's principal that were previously not transmitted. This allows for
  // opening a locally created BlobURL in a new tab.
  //
  // The reason all previously untransmitted Blobs are transmitted is that the
  // current BlobURL could contain html code, referring to another untransmitted
  // BlobURL.
  //
  // Should eventually be made obsolete by broader design changes that only
  // store BlobURLs in the parent process.
  void TransmitBlobDataIfBlobURL(nsIURI* aURI);

  void OnCompositorDeviceReset() override;

  // Control the priority of the IPC messages for input events.
  void SetInputPriorityEventEnabled(bool aEnabled);
  bool IsInputPriorityEventEnabled() { return mIsInputPriorityEventEnabled; }

  static bool IsInputEventQueueSupported();

  mozilla::ipc::IPCResult RecvCreateBrowsingContext(
      uint64_t aGroupId, BrowsingContext::IPCInitializer&& aInit);

  mozilla::ipc::IPCResult RecvDiscardBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext,
      DiscardBrowsingContextResolver&& aResolve);

  mozilla::ipc::IPCResult RecvWindowClose(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aTrustedCaller);
  mozilla::ipc::IPCResult RecvWindowFocus(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType,
      uint64_t aActionId);
  mozilla::ipc::IPCResult RecvWindowBlur(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType);
  mozilla::ipc::IPCResult RecvRaiseWindow(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType,
      uint64_t aActionId);
  mozilla::ipc::IPCResult RecvAdjustWindowFocus(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aIsVisible,
      uint64_t aActionId);
  mozilla::ipc::IPCResult RecvClearFocus(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvSetFocusedBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext, uint64_t aActionId);
  mozilla::ipc::IPCResult RecvSetActiveBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext, uint64_t aActionId);
  mozilla::ipc::IPCResult RecvUnsetActiveBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext, uint64_t aActionId);
  mozilla::ipc::IPCResult RecvSetFocusedElement(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aNeedsFocus);
  mozilla::ipc::IPCResult RecvFinalizeFocusOuter(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aCanFocus,
      CallerType aCallerType);
  mozilla::ipc::IPCResult RecvInsertNewFocusActionId(uint64_t aActionId);
  mozilla::ipc::IPCResult RecvBlurToParent(
      const MaybeDiscarded<BrowsingContext>& aFocusedBrowsingContext,
      const MaybeDiscarded<BrowsingContext>& aBrowsingContextToClear,
      const MaybeDiscarded<BrowsingContext>& aAncestorBrowsingContextToFocus,
      bool aIsLeavingDocument, bool aAdjustWidget,
      bool aBrowsingContextToClearHandled,
      bool aAncestorBrowsingContextToFocusHandled, uint64_t aActionId);
  mozilla::ipc::IPCResult RecvMaybeExitFullscreen(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvWindowPostMessage(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const ClonedOrErrorMessageData& aMessage, const PostMessageData& aData);

  FORWARD_SHMEM_ALLOCATOR_TO(PContentParent)

  PParentToChildStreamParent* SendPParentToChildStreamConstructor(
      PParentToChildStreamParent* aActor) override;
  PFileDescriptorSetParent* SendPFileDescriptorSetConstructor(
      const FileDescriptor& aFD) override;

  mozilla::ipc::IPCResult RecvBlobURLDataRequest(
      const nsCString& aBlobURL, nsIPrincipal* pTriggeringPrincipal,
      nsIPrincipal* pLoadingPrincipal,
      const OriginAttributes& aOriginAttributes, uint64_t aInnerWindowId,
      const Maybe<nsID>& aAgentClusterId,
      BlobURLDataRequestResolver&& aResolver);

 protected:
  bool CheckBrowsingContextEmbedder(CanonicalBrowsingContext* aBC,
                                    const char* aOperation) const;

  void ActorDestroy(ActorDestroyReason why) override;
  void ActorDealloc() override;

  bool ShouldContinueFromReplyTimeout() override;

  void OnVarChanged(const GfxVarUpdate& aVar) override;
  void OnCompositorUnexpectedShutdown() override;

 private:
  /**
   * A map of the remote content process type to a list of content parents
   * currently available to host *new* tabs/frames of that type.
   *
   * If a content process is identified as troubled or dead, it will be
   * removed from this list, but will still be in the sContentParents list for
   * the GetAll/GetAllEvenIfDead APIs.
   */
  static nsClassHashtable<nsCStringHashKey, nsTArray<ContentParent*>>*
      sBrowserContentParents;
  static UniquePtr<nsTArray<ContentParent*>> sPrivateContent;
  static UniquePtr<nsTHashMap<nsUint32HashKey, ContentParent*>>
      sJSPluginContentParents;
  static UniquePtr<LinkedList<ContentParent>> sContentParents;

  /**
   * In order to avoid rapidly creating and destroying content processes when
   * running under e10s, we may keep alive a single unused "web" content
   * process if it previously had a very short lifetime.
   *
   * This process will be re-used during process selection, avoiding spawning a
   * new process, if the "web" remote type is being requested.
   */
  static StaticRefPtr<ContentParent> sRecycledE10SProcess;

  void AddShutdownBlockers();
  void RemoveShutdownBlockers();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // Cached Mac sandbox params used when launching content processes.
  static UniquePtr<std::vector<std::string>> sMacSandboxParams;
#endif

  // Set aLoadUri to true to load aURIToLoad and to false to only create the
  // window. aURIToLoad should always be provided, if available, to ensure
  // compatibility with GeckoView.
  mozilla::ipc::IPCResult CommonCreateWindow(
      PBrowserParent* aThisTab, BrowsingContext* aParent, bool aSetOpener,
      const uint32_t& aChromeFlags, const bool& aCalledFromJS,
      const bool& aWidthSpecified, const bool& aForPrinting,
      const bool& aForWindowDotPrint, nsIURI* aURIToLoad,
      const nsCString& aFeatures, const float& aFullZoom,
      BrowserParent* aNextRemoteBrowser, const nsString& aName,
      nsresult& aResult, nsCOMPtr<nsIRemoteTab>& aNewRemoteTab,
      bool* aWindowIsNew, int32_t& aOpenLocation,
      nsIPrincipal* aTriggeringPrincipal, nsIReferrerInfo* aReferrerInfo,
      bool aLoadUri, nsIContentSecurityPolicy* aCsp,
      const OriginAttributes& aOriginAttributes);

  explicit ContentParent(int32_t aPluginID) : ContentParent(""_ns, aPluginID) {}
  explicit ContentParent(const nsACString& aRemoteType)
      : ContentParent(aRemoteType, nsFakePluginTag::NOT_JSPLUGIN) {}

  ContentParent(const nsACString& aRemoteType, int32_t aPluginID);

  // Launch the subprocess and associated initialization.
  // Returns false if the process fails to start.
  // Deprecated in favor of LaunchSubprocessAsync.
  bool LaunchSubprocessSync(hal::ProcessPriority aInitialPriority);

  // Launch the subprocess and associated initialization;
  // returns a promise and signals failure by rejecting.
  // OS-level launching work is dispatched to another thread, but some
  // initialization (creating IPDL actors, etc.; see Init()) is run on
  // the main thread.
  RefPtr<LaunchPromise> LaunchSubprocessAsync(
      hal::ProcessPriority aInitialPriority);

  // Common implementation of LaunchSubprocess{Sync,Async}.
  // Return `true` in case of success, `false` if launch was
  // aborted because of shutdown.
  bool BeginSubprocessLaunch(ProcessPriority aPriority);
  void LaunchSubprocessReject();
  bool LaunchSubprocessResolve(bool aIsSync, ProcessPriority aPriority);

  // Common initialization after sub process launch.
  bool InitInternal(ProcessPriority aPriority);

  // Generate a minidump for the child process and one for the main process
  void GeneratePairedMinidump(const char* aReason);
  void HandleOrphanedMinidump(nsString* aDumpId);

  virtual ~ContentParent();

  void Init();

  // Some information could be sent to content very early, it
  // should be send from this function. This function should only be
  // called after the process has been transformed to browser.
  void ForwardKnownInfo();

  /**
   * We might want to reuse barely used content processes if certain criteria
   * are met.
   */
  bool TryToRecycle();

  /**
   * If this process is currently being recycled, unmark it as the recycled
   * content process.
   * If `aForeground` is true, will also restore the process' foreground
   * priority if it was previously the recycled content process.
   */
  void StopRecycling(bool aForeground = true);

  /**
   * Removing it from the static array so it won't be returned for new tabs in
   * GetNewOrUsedBrowserProcess.
   */
  void RemoveFromList();

  /**
   * Return if the process has an active worker or JSPlugin
   */
  bool HasActiveWorkerOrJSPlugin();

  /**
   * Decide whether the process should be kept alive even when it would normally
   * be shut down, for example when all its tabs are closed.
   */
  bool ShouldKeepProcessAlive();

  /**
   * Mark this ContentParent as dead for the purposes of Get*().
   * This method is idempotent.
   */
  void MarkAsDead();

  /**
   * Check if this process is ready to be shut down, and if it is, begin the
   * shutdown process. Should be called whenever a change occurs which could
   * cause the decisions made by `ShouldKeepProcessAlive` to change.
   *
   * @param aExpectedBrowserCount The number of PBrowser actors which should
   *                              not block shutdown. This should usually be 0.
   * @param aSendShutDown If true, will send the shutdown message in addition
   *                      to marking the process as dead and starting the force
   *                      kill timer.
   */
  void MaybeBeginShutDown(uint32_t aExpectedBrowserCount = 0,
                          bool aSendShutDown = true);

  /**
   * How we will shut down this ContentParent and its subprocess.
   */
  enum ShutDownMethod {
    // Send a shutdown message and wait for FinishShutdown call back.
    SEND_SHUTDOWN_MESSAGE,
    // Close the channel ourselves and let the subprocess clean up itself.
    CLOSE_CHANNEL,
    // Close the channel with error and let the subprocess clean up itself.
    CLOSE_CHANNEL_WITH_ERROR,
  };

  void MaybeAsyncSendShutDownMessage();

  /**
   * Exit the subprocess and vamoose.  After this call IsAlive()
   * will return false and this ContentParent will not be returned
   * by the Get*() funtions.  However, the shutdown sequence itself
   * may be asynchronous.
   *
   * If aMethod is CLOSE_CHANNEL_WITH_ERROR and this is the first call
   * to ShutDownProcess, then we'll close our channel using CloseWithError()
   * rather than vanilla Close().  CloseWithError() indicates to IPC that this
   * is an abnormal shutdown (e.g. a crash).
   */
  void ShutDownProcess(ShutDownMethod aMethod);

  // Perform any steps necesssary to gracefully shtudown the message
  // manager and null out mMessageManager.
  void ShutDownMessageManager();

  // Start the force-kill timer on shutdown.
  void StartForceKillTimer();

  // Ensure that the permissions for the giben Permission key are set in the
  // content process.
  //
  // See nsIPermissionManager::GetPermissionsForKey for more information on
  // these keys.
  void EnsurePermissionsByKey(const nsCString& aKey, const nsCString& aOrigin);

  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);

  bool CanOpenBrowser(const IPCTabContext& aContext);

  /**
   * Get or create the corresponding content parent array to
   * |aContentProcessType|.
   */
  static nsTArray<ContentParent*>& GetOrCreatePool(
      const nsACString& aContentProcessType);

  mozilla::ipc::IPCResult RecvInitBackground(
      Endpoint<mozilla::ipc::PBackgroundParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);
  mozilla::ipc::IPCResult RecvAddPerformanceMetrics(
      const nsID& aID, nsTArray<PerformanceInfo>&& aMetrics);

  bool DeallocPRemoteSpellcheckEngineParent(PRemoteSpellcheckEngineParent*);

  mozilla::ipc::IPCResult RecvCloneDocumentTreeInto(
      const MaybeDiscarded<BrowsingContext>& aSource,
      const MaybeDiscarded<BrowsingContext>& aTarget, PrintData&& aPrintData);

  mozilla::ipc::IPCResult RecvUpdateRemotePrintSettings(
      const MaybeDiscarded<BrowsingContext>& aTarget, PrintData&& aPrintData);

  mozilla::ipc::IPCResult RecvConstructPopupBrowser(
      ManagedEndpoint<PBrowserParent>&& actor,
      ManagedEndpoint<PWindowGlobalParent>&& windowEp, const TabId& tabId,
      const IPCTabContext& context, const WindowGlobalInit& initialWindowInit,
      const uint32_t& chromeFlags);

  mozilla::ipc::IPCResult RecvIsSecureURI(
      nsIURI* aURI, const uint32_t& aFlags,
      const OriginAttributes& aOriginAttributes, bool* aIsSecureURI);

  mozilla::ipc::IPCResult RecvAccumulateMixedContentHSTS(
      nsIURI* aURI, const bool& aActive,
      const OriginAttributes& aOriginAttributes);

  bool DeallocPHalParent(PHalParent*);

  bool DeallocPHeapSnapshotTempFileHelperParent(
      PHeapSnapshotTempFileHelperParent*);

  PCycleCollectWithLogsParent* AllocPCycleCollectWithLogsParent(
      const bool& aDumpAllTraces, const FileDescriptor& aGCLog,
      const FileDescriptor& aCCLog);

  bool DeallocPCycleCollectWithLogsParent(PCycleCollectWithLogsParent* aActor);

  PTestShellParent* AllocPTestShellParent();

  bool DeallocPTestShellParent(PTestShellParent* shell);

  PScriptCacheParent* AllocPScriptCacheParent(const FileDescOrError& cacheFile,
                                              const bool& wantCacheData);

  bool DeallocPScriptCacheParent(PScriptCacheParent* shell);

  bool DeallocPNeckoParent(PNeckoParent* necko);

  already_AddRefed<PExternalHelperAppParent> AllocPExternalHelperAppParent(
      nsIURI* aUri, const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
      const nsCString& aMimeContentType, const nsCString& aContentDisposition,
      const uint32_t& aContentDispositionHint,
      const nsString& aContentDispositionFilename, const bool& aForceSave,
      const int64_t& aContentLength, const bool& aWasFileChannel,
      nsIURI* aReferrer, const MaybeDiscarded<BrowsingContext>& aContext,
      const bool& aShouldCloseWindow);

  mozilla::ipc::IPCResult RecvPExternalHelperAppConstructor(
      PExternalHelperAppParent* actor, nsIURI* uri,
      const Maybe<LoadInfoArgs>& loadInfoArgs,
      const nsCString& aMimeContentType, const nsCString& aContentDisposition,
      const uint32_t& aContentDispositionHint,
      const nsString& aContentDispositionFilename, const bool& aForceSave,
      const int64_t& aContentLength, const bool& aWasFileChannel,
      nsIURI* aReferrer, const MaybeDiscarded<BrowsingContext>& aContext,
      const bool& aShouldCloseWindow) override;

  already_AddRefed<PHandlerServiceParent> AllocPHandlerServiceParent();

  PMediaParent* AllocPMediaParent();

  bool DeallocPMediaParent(PMediaParent* aActor);

  PBenchmarkStorageParent* AllocPBenchmarkStorageParent();

  bool DeallocPBenchmarkStorageParent(PBenchmarkStorageParent* aActor);

#ifdef MOZ_WEBSPEECH
  PSpeechSynthesisParent* AllocPSpeechSynthesisParent();
  bool DeallocPSpeechSynthesisParent(PSpeechSynthesisParent* aActor);

  virtual mozilla::ipc::IPCResult RecvPSpeechSynthesisConstructor(
      PSpeechSynthesisParent* aActor) override;
#endif

  PWebBrowserPersistDocumentParent* AllocPWebBrowserPersistDocumentParent(
      PBrowserParent* aBrowser,
      const MaybeDiscarded<BrowsingContext>& aContext);

  bool DeallocPWebBrowserPersistDocumentParent(
      PWebBrowserPersistDocumentParent* aActor);

  mozilla::ipc::IPCResult RecvGetGfxVars(nsTArray<GfxVarUpdate>* aVars);

  mozilla::ipc::IPCResult RecvSetClipboard(
      const IPCDataTransfer& aDataTransfer, const bool& aIsPrivateData,
      const IPC::Principal& aRequestingPrincipal,
      const nsContentPolicyType& aContentPolicyType,
      const int32_t& aWhichClipboard);

  mozilla::ipc::IPCResult RecvGetClipboard(nsTArray<nsCString>&& aTypes,
                                           const int32_t& aWhichClipboard,
                                           IPCDataTransfer* aDataTransfer);

  mozilla::ipc::IPCResult RecvEmptyClipboard(const int32_t& aWhichClipboard);

  mozilla::ipc::IPCResult RecvClipboardHasType(nsTArray<nsCString>&& aTypes,
                                               const int32_t& aWhichClipboard,
                                               bool* aHasType);

  mozilla::ipc::IPCResult RecvGetExternalClipboardFormats(
      const int32_t& aWhichClipboard, const bool& aPlainTextOnly,
      nsTArray<nsCString>* aTypes);

  mozilla::ipc::IPCResult RecvPlaySound(nsIURI* aURI);
  mozilla::ipc::IPCResult RecvBeep();
  mozilla::ipc::IPCResult RecvPlayEventSound(const uint32_t& aEventId);

  mozilla::ipc::IPCResult RecvGetIconForExtension(const nsCString& aFileExt,
                                                  const uint32_t& aIconSize,
                                                  nsTArray<uint8_t>* bits);

  mozilla::ipc::IPCResult RecvStartVisitedQueries(
      const nsTArray<RefPtr<nsIURI>>&);

  mozilla::ipc::IPCResult RecvSetURITitle(nsIURI* uri, const nsString& title);

  mozilla::ipc::IPCResult RecvShowAlert(nsIAlertNotification* aAlert);

  mozilla::ipc::IPCResult RecvCloseAlert(const nsString& aName);

  mozilla::ipc::IPCResult RecvDisableNotifications(
      const IPC::Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvOpenNotificationSettings(
      const IPC::Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvNotificationEvent(
      const nsString& aType, const NotificationEventData& aData);

  mozilla::ipc::IPCResult RecvLoadURIExternal(
      nsIURI* uri, nsIPrincipal* triggeringPrincipal,
      const MaybeDiscarded<BrowsingContext>& aContext,
      bool aWasExternallyTriggered);
  mozilla::ipc::IPCResult RecvExtProtocolChannelConnectParent(
      const uint64_t& registrarId);

  mozilla::ipc::IPCResult RecvSyncMessage(
      const nsString& aMsg, const ClonedMessageData& aData,
      nsTArray<StructuredCloneData>* aRetvals);

  mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                           const ClonedMessageData& aData);

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we don't have MOZ_CAN_RUN_SCRIPT bits
  // in IPC code yet.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvAddGeolocationListener(const bool& aHighAccuracy);
  mozilla::ipc::IPCResult RecvRemoveGeolocationListener();

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we don't have MOZ_CAN_RUN_SCRIPT bits
  // in IPC code yet.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvSetGeolocationHigherAccuracy(const bool& aEnable);

  mozilla::ipc::IPCResult RecvConsoleMessage(const nsString& aMessage);

  mozilla::ipc::IPCResult RecvScriptError(
      const nsString& aMessage, const nsString& aSourceName,
      const nsString& aSourceLine, const uint32_t& aLineNumber,
      const uint32_t& aColNumber, const uint32_t& aFlags,
      const nsCString& aCategory, const bool& aIsFromPrivateWindow,
      const uint64_t& aInnerWindowId, const bool& aIsFromChromeContext);

  mozilla::ipc::IPCResult RecvReportFrameTimingData(
      uint64_t innerWindowId, const nsString& entryName,
      const nsString& initiatorType, UniquePtr<PerformanceTimingData>&& aData);

  mozilla::ipc::IPCResult RecvScriptErrorWithStack(
      const nsString& aMessage, const nsString& aSourceName,
      const nsString& aSourceLine, const uint32_t& aLineNumber,
      const uint32_t& aColNumber, const uint32_t& aFlags,
      const nsCString& aCategory, const bool& aIsFromPrivateWindow,
      const bool& aIsFromChromeContext, const ClonedMessageData& aStack);

 private:
  mozilla::ipc::IPCResult RecvScriptErrorInternal(
      const nsString& aMessage, const nsString& aSourceName,
      const nsString& aSourceLine, const uint32_t& aLineNumber,
      const uint32_t& aColNumber, const uint32_t& aFlags,
      const nsCString& aCategory, const bool& aIsFromPrivateWindow,
      const bool& aIsFromChromeContext,
      const ClonedMessageData* aStack = nullptr);

 public:
  mozilla::ipc::IPCResult RecvCommitBrowsingContextTransaction(
      const MaybeDiscarded<BrowsingContext>& aContext,
      BrowsingContext::BaseTransaction&& aTransaction, uint64_t aEpoch);

  mozilla::ipc::IPCResult RecvCommitWindowContextTransaction(
      const MaybeDiscarded<WindowContext>& aContext,
      WindowContext::BaseTransaction&& aTransaction, uint64_t aEpoch);

  mozilla::ipc::IPCResult RecvAddSecurityState(
      const MaybeDiscarded<WindowContext>& aContext, uint32_t aStateFlags);

  mozilla::ipc::IPCResult RecvFirstIdle();

  mozilla::ipc::IPCResult RecvDeviceReset();

  mozilla::ipc::IPCResult RecvCopyFavicon(nsIURI* aOldURI, nsIURI* aNewURI,
                                          const bool& aInPrivateBrowsing);

  virtual void ProcessingError(Result aCode, const char* aMsgName) override;

  mozilla::ipc::IPCResult RecvGraphicsError(const nsCString& aError);

  mozilla::ipc::IPCResult RecvBeginDriverCrashGuard(const uint32_t& aGuardType,
                                                    bool* aOutCrashed);

  mozilla::ipc::IPCResult RecvEndDriverCrashGuard(const uint32_t& aGuardType);

  mozilla::ipc::IPCResult RecvAddIdleObserver(const uint64_t& observerId,
                                              const uint32_t& aIdleTimeInS);

  mozilla::ipc::IPCResult RecvRemoveIdleObserver(const uint64_t& observerId,
                                                 const uint32_t& aIdleTimeInS);

  mozilla::ipc::IPCResult RecvBackUpXResources(
      const FileDescriptor& aXSocketFd);

  mozilla::ipc::IPCResult RecvRequestAnonymousTemporaryFile(
      const uint64_t& aID);

  mozilla::ipc::IPCResult RecvCreateAudioIPCConnection(
      CreateAudioIPCConnectionResolver&& aResolver);

  already_AddRefed<extensions::PExtensionsParent> AllocPExtensionsParent();

  PFileDescriptorSetParent* AllocPFileDescriptorSetParent(
      const mozilla::ipc::FileDescriptor&);

  bool DeallocPFileDescriptorSetParent(PFileDescriptorSetParent*);

  PWebrtcGlobalParent* AllocPWebrtcGlobalParent();
  bool DeallocPWebrtcGlobalParent(PWebrtcGlobalParent* aActor);

  mozilla::ipc::IPCResult RecvUpdateDropEffect(const uint32_t& aDragAction,
                                               const uint32_t& aDropEffect);

  mozilla::ipc::IPCResult RecvShutdownProfile(const nsCString& aProfile);

  mozilla::ipc::IPCResult RecvGetGraphicsDeviceInitData(
      ContentDeviceData* aOut);

  mozilla::ipc::IPCResult RecvGetOutputColorProfileData(
      nsTArray<uint8_t>* aOutputColorProfileData);

  mozilla::ipc::IPCResult RecvGetFontListShmBlock(
      const uint32_t& aGeneration, const uint32_t& aIndex,
      base::SharedMemoryHandle* aOut);

  mozilla::ipc::IPCResult RecvInitializeFamily(const uint32_t& aGeneration,
                                               const uint32_t& aFamilyIndex,
                                               const bool& aLoadCmaps);

  mozilla::ipc::IPCResult RecvSetCharacterMap(
      const uint32_t& aGeneration, const mozilla::fontlist::Pointer& aFacePtr,
      const gfxSparseBitSet& aMap);

  mozilla::ipc::IPCResult RecvInitOtherFamilyNames(const uint32_t& aGeneration,
                                                   const bool& aDefer,
                                                   bool* aLoaded);

  mozilla::ipc::IPCResult RecvSetupFamilyCharMap(
      const uint32_t& aGeneration,
      const mozilla::fontlist::Pointer& aFamilyPtr);

  mozilla::ipc::IPCResult RecvStartCmapLoading(const uint32_t& aGeneration,
                                               const uint32_t& aStartIndex);

  mozilla::ipc::IPCResult RecvGetHyphDict(nsIURI* aURIParams,
                                          base::SharedMemoryHandle* aOutHandle,
                                          uint32_t* aOutSize);

  mozilla::ipc::IPCResult RecvNotifyBenchmarkResult(const nsString& aCodecName,
                                                    const uint32_t& aDecodeFPS);

  mozilla::ipc::IPCResult RecvNotifyPushObservers(
      const nsCString& aScope, const IPC::Principal& aPrincipal,
      const nsString& aMessageId);

  mozilla::ipc::IPCResult RecvNotifyPushObserversWithData(
      const nsCString& aScope, const IPC::Principal& aPrincipal,
      const nsString& aMessageId, nsTArray<uint8_t>&& aData);

  mozilla::ipc::IPCResult RecvNotifyPushSubscriptionChangeObservers(
      const nsCString& aScope, const IPC::Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvPushError(const nsCString& aScope,
                                        const IPC::Principal& aPrincipal,
                                        const nsString& aMessage,
                                        const uint32_t& aFlags);

  mozilla::ipc::IPCResult RecvNotifyPushSubscriptionModifiedObservers(
      const nsCString& aScope, const IPC::Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvGetFilesRequest(const nsID& aID,
                                              const nsString& aDirectoryPath,
                                              const bool& aRecursiveFlag);

  mozilla::ipc::IPCResult RecvDeleteGetFilesRequest(const nsID& aID);

  mozilla::ipc::IPCResult RecvAccumulateChildHistograms(
      nsTArray<HistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistograms(
      nsTArray<KeyedHistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvUpdateChildScalars(
      nsTArray<ScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvUpdateChildKeyedScalars(
      nsTArray<KeyedScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvRecordChildEvents(
      nsTArray<ChildEventData>&& events);
  mozilla::ipc::IPCResult RecvRecordDiscardedData(
      const DiscardedData& aDiscardedData);
  mozilla::ipc::IPCResult RecvRecordOrigin(const uint32_t& aMetricId,
                                           const nsCString& aOrigin);
  mozilla::ipc::IPCResult RecvReportContentBlockingLog(
      const IPCStream& aIPCStream);

  mozilla::ipc::IPCResult RecvBHRThreadHang(const HangDetails& aHangDetails);

  mozilla::ipc::IPCResult RecvAddCertException(
      const nsACString& aSerializedCert, uint32_t aFlags,
      const nsACString& aHostName, int32_t aPort,
      const OriginAttributes& aOriginAttributes, bool aIsTemporary,
      AddCertExceptionResolver&& aResolver);

  mozilla::ipc::IPCResult RecvAutomaticStorageAccessPermissionCanBeGranted(
      const Principal& aPrincipal,
      AutomaticStorageAccessPermissionCanBeGrantedResolver&& aResolver);

  mozilla::ipc::IPCResult RecvStorageAccessPermissionGrantedForOrigin(
      uint64_t aTopLevelWindowId,
      const MaybeDiscarded<BrowsingContext>& aParentContext,
      const Principal& aTrackingPrincipal, const nsCString& aTrackingOrigin,
      const int& aAllowMode,
      const Maybe<
          ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
          aReason,
      StorageAccessPermissionGrantedForOriginResolver&& aResolver);

  mozilla::ipc::IPCResult RecvCompleteAllowAccessFor(
      const MaybeDiscarded<BrowsingContext>& aParentContext,
      uint64_t aTopLevelWindowId, const Principal& aTrackingPrincipal,
      const nsCString& aTrackingOrigin, uint32_t aCookieBehavior,
      const ContentBlockingNotifier::StorageAccessPermissionGrantedReason&
          aReason,
      CompleteAllowAccessForResolver&& aResolver);

  mozilla::ipc::IPCResult RecvStoreUserInteractionAsPermission(
      const Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvNotifyMediaPlaybackChanged(
      const MaybeDiscarded<BrowsingContext>& aContext,
      MediaPlaybackState aState);

  mozilla::ipc::IPCResult RecvNotifyMediaAudibleChanged(
      const MaybeDiscarded<BrowsingContext>& aContext,
      MediaAudibleState aState);

  mozilla::ipc::IPCResult RecvNotifyPictureInPictureModeChanged(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aEnabled);

  mozilla::ipc::IPCResult RecvNotifyMediaSessionUpdated(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aIsCreated);

  mozilla::ipc::IPCResult RecvNotifyUpdateMediaMetadata(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const Maybe<MediaMetadataBase>& aMetadata);

  mozilla::ipc::IPCResult RecvNotifyMediaSessionPlaybackStateChanged(
      const MaybeDiscarded<BrowsingContext>& aContext,
      MediaSessionPlaybackState aPlaybackState);

  mozilla::ipc::IPCResult RecvNotifyMediaSessionSupportedActionChanged(
      const MaybeDiscarded<BrowsingContext>& aContext,
      MediaSessionAction aAction, bool aEnabled);

  mozilla::ipc::IPCResult RecvNotifyMediaFullScreenState(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aIsInFullScreen);

  mozilla::ipc::IPCResult RecvNotifyPositionStateChanged(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const PositionState& aState);

  mozilla::ipc::IPCResult RecvAddOrRemovePageAwakeRequest(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const bool& aShouldAddCount);

  mozilla::ipc::IPCResult RecvGetModulesTrust(
      ModulePaths&& aModPaths, bool aRunAtNormalPriority,
      GetModulesTrustResolver&& aResolver);

  mozilla::ipc::IPCResult RecvReportServiceWorkerShutdownProgress(
      uint32_t aShutdownStateId,
      ServiceWorkerShutdownState::Progress aProgress);

  mozilla::ipc::IPCResult RecvRawMessage(
      const JSActorMessageMeta& aMeta, const Maybe<ClonedMessageData>& aData,
      const Maybe<ClonedMessageData>& aStack);

  mozilla::ipc::IPCResult RecvAbortOtherOrientationPendingPromises(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvNotifyOnHistoryReload(
      const MaybeDiscarded<BrowsingContext>& aContext, const bool& aForceReload,
      NotifyOnHistoryReloadResolver&& aResolver);

  mozilla::ipc::IPCResult RecvHistoryCommit(
      const MaybeDiscarded<BrowsingContext>& aContext, const uint64_t& aLoadID,
      const nsID& aChangeID, const uint32_t& aLoadType, const bool& aPersist,
      const bool& aCloneEntryChildren);

  mozilla::ipc::IPCResult RecvHistoryGo(
      const MaybeDiscarded<BrowsingContext>& aContext, int32_t aOffset,
      uint64_t aHistoryEpoch, bool aRequireUserInteraction,
      bool aUserActivation, HistoryGoResolver&& aResolveRequestedIndex);

  mozilla::ipc::IPCResult RecvSynchronizeLayoutHistoryState(
      const MaybeDiscarded<BrowsingContext>& aContext,
      nsILayoutHistoryState* aState);

  mozilla::ipc::IPCResult RecvSessionHistoryEntryTitle(
      const MaybeDiscarded<BrowsingContext>& aContext, const nsString& aTitle);

  mozilla::ipc::IPCResult RecvSessionHistoryEntryScrollRestorationIsManual(
      const MaybeDiscarded<BrowsingContext>& aContext, const bool& aIsManual);

  mozilla::ipc::IPCResult RecvSessionHistoryEntryScrollPosition(
      const MaybeDiscarded<BrowsingContext>& aContext, const int32_t& aX,
      const int32_t& aY);

  mozilla::ipc::IPCResult RecvSessionHistoryEntryCacheKey(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const uint32_t& aCacheKey);

  mozilla::ipc::IPCResult
  RecvSessionHistoryEntryStoreWindowNameInContiguousEntries(
      const MaybeDiscarded<BrowsingContext>& aContext, const nsString& aName);

  mozilla::ipc::IPCResult RecvGetLoadingSessionHistoryInfoFromParent(
      const MaybeDiscarded<BrowsingContext>& aContext,
      GetLoadingSessionHistoryInfoFromParentResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRemoveFromBFCache(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvSetActiveSessionHistoryEntry(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const Maybe<nsPoint>& aPreviousScrollPos, SessionHistoryInfo&& aInfo,
      uint32_t aLoadType, uint32_t aUpdatedCacheKey, const nsID& aChangeID);

  mozilla::ipc::IPCResult RecvReplaceActiveSessionHistoryEntry(
      const MaybeDiscarded<BrowsingContext>& aContext,
      SessionHistoryInfo&& aInfo);

  mozilla::ipc::IPCResult RecvRemoveDynEntriesFromActiveSessionHistoryEntry(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvRemoveFromSessionHistory(
      const MaybeDiscarded<BrowsingContext>& aContext, const nsID& aChangeID);

  mozilla::ipc::IPCResult RecvHistoryReload(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const uint32_t aReloadFlags);

  // Notify the ContentChild to enable the input event prioritization when
  // initializing.
  void MaybeEnableRemoteInputEventQueue();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  void AppendSandboxParams(std::vector<std::string>& aArgs);
  void AppendDynamicSandboxParams(std::vector<std::string>& aArgs);
#endif

  mozilla::ipc::IPCResult RecvFOGData(ByteBuf&& buf);

  mozilla::ipc::IPCResult RecvSetContainerFeaturePolicy(
      const MaybeDiscardedBrowsingContext& aContainerContext,
      FeaturePolicy* aContainerFeaturePolicy);

  mozilla::ipc::IPCResult RecvGetSystemIcon(nsIURI* aURI,
                                            GetSystemIconResolver&& aResolver);

 public:
  void SendGetFilesResponseAndForget(const nsID& aID,
                                     const GetFilesResponseResult& aResult);

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<FileDescriptor>& aDMDFile) override;

  void AddBrowsingContextGroup(BrowsingContextGroup* aGroup);
  void RemoveBrowsingContextGroup(BrowsingContextGroup* aGroup);

  // See `BrowsingContext::mEpochs` for an explanation of this field.
  uint64_t GetBrowsingContextFieldEpoch() const {
    return mBrowsingContextFieldEpoch;
  }

  void UpdateNetworkLinkType();

  static bool ShouldSyncPreference(const char16_t* aData);

  already_AddRefed<JSActor> InitJSActor(JS::HandleObject aMaybeActor,
                                        const nsACString& aName,
                                        ErrorResult& aRv) override;
  mozilla::ipc::IProtocol* AsNativeActor() override { return this; }

 private:
  // Return an existing ContentParent if possible. Otherwise, `nullptr`.
  static already_AddRefed<ContentParent> GetUsedBrowserProcess(
      const nsACString& aRemoteType, nsTArray<ContentParent*>& aContentParents,
      uint32_t aMaxContentParents, bool aPreferUsed, ProcessPriority aPriority);

  void AddToPool(nsTArray<ContentParent*>&);
  void RemoveFromPool(nsTArray<ContentParent*>&);
  void AssertNotInPool();

  void AssertAlive();

  /**
   * Called when a subprocess succesfully launches.
   *
   * May submit telemetry if the new number of content processes is greater
   * than the previous maximum.
   *
   * This will submit telemetry about the time delta between this content
   * process launch and the last content process launch.
   */
  static void DidLaunchSubprocess();

 private:
  // Released in ActorDealloc; deliberately not exposed to the CC.
  RefPtr<ContentParent> mSelfRef;

  // If you add strong pointers to cycle collected objects here, be sure to
  // release these objects in ShutDownProcess.  See the comment there for more
  // details.

  GeckoChildProcessHost* mSubprocess;
  const TimeStamp mLaunchTS;  // used to calculate time to start content process
  TimeStamp mLaunchYieldTS;   // used to calculate async launch main thread time
  TimeStamp mActivateTS;

  bool mIsAPreallocBlocker;  // We called AddBlocker for this ContentParent

  nsCString mRemoteType;
  nsCOMPtr<nsIPrincipal> mRemoteTypeIsolationPrincipal;

  ContentParentId mChildID;
  int32_t mGeolocationWatchID;

  // This contains the id for the JS plugin (@see nsFakePluginTag) if this is
  // the ContentParent for a process containing iframes for that JS plugin. If
  // this is not a ContentParent for a JS plugin then it contains the value
  // nsFakePluginTag::NOT_JSPLUGIN.
  int32_t mJSPluginID;

  // After we initiate shutdown, we also start a timer to ensure
  // that even content processes that are 100% blocked (say from
  // SIGSTOP), are still killed eventually.  This task enforces that
  // timer.
  nsCOMPtr<nsITimer> mForceKillTimer;

  // `mCount` is increased when a RemoteWorkerParent actor is created for this
  // ContentProcess and it is decreased when the actor is destroyed.
  //
  // `mShutdownStarted` is flipped to `true` when a runnable that calls
  // `ShutDownProcess` is dispatched; it's needed because the corresponding
  // Content Process may be shutdown if there's no remote worker actors, and
  // decrementing `mCount` and the call to `ShutDownProcess` are async. So,
  // when a worker is going to be spawned and we see that `mCount` is 0,
  // we can decide whether or not to use that process based on the value of
  // `mShutdownStarted.`
  //
  // It's touched on PBackground thread and on main-thread.
  struct RemoteWorkerActorData {
    uint32_t mCount = 0;
    bool mShutdownStarted = false;
  };

  DataMutex<RemoteWorkerActorData> mRemoteWorkerActorData;

  // How many tabs we're waiting to finish their destruction
  // sequence.  Precisely, how many BrowserParents have called
  // NotifyTabDestroying() but not called NotifyTabDestroyed().
  int32_t mNumDestroyingTabs;

  uint32_t mNumKeepaliveCalls;

  // The process starts in the LAUNCHING state, and transitions to
  // ALIVE once it can accept IPC messages.  It remains ALIVE only
  // while remote content is being actively used from this process.
  // After the state becaomes DEAD, some previously scheduled IPC
  // traffic may still pass through.
  enum class LifecycleState : uint8_t {
    LAUNCHING,
    ALIVE,
    INITIALIZED,
    DEAD,
  };

  LifecycleState mLifecycleState;

  uint8_t mIsForBrowser : 1;

  // These variables track whether we've called Close() and KillHard() on our
  // channel.
  uint8_t mCalledClose : 1;
  uint8_t mCalledKillHard : 1;
  uint8_t mCreatedPairedMinidumps : 1;
  uint8_t mShutdownPending : 1;

  // Whether or not `LaunchSubprocessResolve` has been called, and whether or
  // not it returned `true` when called.
  uint8_t mLaunchResolved : 1;
  uint8_t mLaunchResolvedOk : 1;

  // True if the input event queue on the main thread of the content process is
  // enabled.
  uint8_t mIsRemoteInputEventQueueEnabled : 1;

  // True if we send input events with input priority. Otherwise, we send input
  // events with normal priority.
  uint8_t mIsInputPriorityEventEnabled : 1;

  uint8_t mIsInPool : 1;

  RefPtr<nsConsoleService> mConsoleService;
  nsConsoleService* GetConsoleService();
  nsCOMPtr<nsIContentProcessInfo> mScriptableHelper;

  nsTArray<nsCOMPtr<nsIObserver>> mIdleListeners;

#ifdef MOZ_X11
  // Dup of child's X socket, used to scope its resources to this
  // object instead of the child process's lifetime.
  ScopedClose mChildXSocketFdDup;
#endif

  PProcessHangMonitorParent* mHangMonitorActor;

  UniquePtr<gfx::DriverCrashGuard> mDriverCrashGuard;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  mozilla::UniquePtr<SandboxBroker> mSandboxBroker;
  static mozilla::UniquePtr<SandboxBrokerPolicyFactory>
      sSandboxBrokerPolicyFactory;
#endif

#ifdef NS_PRINTING
  RefPtr<embedding::PrintingParent> mPrintingParent;
#endif

  // This hashtable is used to run GetFilesHelper objects in the parent process.
  // GetFilesHelper can be aborted by receiving RecvDeleteGetFilesRequest.
  nsRefPtrHashtable<nsIDHashKey, GetFilesHelper> mGetFilesPendingRequests;

  nsTHashSet<nsCString> mActivePermissionKeys;

  nsTArray<nsCString> mBlobURLs;

  // This is intended to be a memory and time efficient means of determining
  // whether an origin has ever existed in a process so that Blob URL broadcast
  // doesn't need to transmit every Blob URL to every content process. False
  // positives are acceptable because receiving a Blob URL does not grant access
  // to its contents, and the act of creating/revoking a Blob is currently
  // viewed as an acceptable side-channel leak. In the future bug 1491018 will
  // moot the need for this structure.
  nsTArray<uint64_t> mLoadedOriginHashes;

  UniquePtr<mozilla::ipc::CrashReporterHost> mCrashReporter;

  // Collects any pref changes that occur during process launch (after
  // the initial map is passed in command-line arguments) to be sent
  // when the process can receive IPC messages.
  nsTArray<Pref> mQueuedPrefs;

  RefPtr<mozilla::dom::ProcessMessageManager> mMessageManager;

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // When set to true, indicates that content processes should
  // initialize their sandbox during startup instead of waiting
  // for the SetProcessSandbox IPDL message.
  static bool sEarlySandboxInit;
#endif

  nsTHashSet<RefPtr<BrowsingContextGroup>> mGroups;

  // See `BrowsingContext::mEpochs` for an explanation of this field.
  uint64_t mBrowsingContextFieldEpoch = 0;

  // A preference serializer used to share preferences with the process.
  // Cleared once startup is complete.
  UniquePtr<mozilla::ipc::SharedPreferenceSerializer> mPrefSerializer;

  static uint32_t sMaxContentProcesses;
  static Maybe<TimeStamp> sLastContentProcessLaunch;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ContentParent, NS_CONTENTPARENT_IID)

// This is the C++ version of remoteTypePrefix in E10SUtils.jsm.
const nsDependentCSubstring RemoteTypePrefix(
    const nsACString& aContentProcessType);

// This is based on isWebRemoteType in E10SUtils.jsm.
bool IsWebRemoteType(const nsACString& aContentProcessType);

bool IsWebCoopCoepRemoteType(const nsACString& aContentProcessType);

bool IsPrivilegedMozillaRemoteType(const nsACString& aContentProcessType);

bool IsExtensionRemoteType(const nsACString& aContentProcessType);

inline nsISupports* ToSupports(mozilla::dom::ContentParent* aContentParent) {
  return static_cast<nsIDOMProcessParent*>(aContentParent);
}

}  // namespace dom
}  // namespace mozilla

class ParentIdleListener : public nsIObserver {
  friend class mozilla::dom::ContentParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ParentIdleListener(mozilla::dom::ContentParent* aParent, uint64_t aObserver,
                     uint32_t aTime)
      : mParent(aParent), mObserver(aObserver), mTime(aTime) {}

 private:
  virtual ~ParentIdleListener() = default;

  RefPtr<mozilla::dom::ContentParent> mParent;
  uint64_t mObserver;
  uint32_t mTime;
};

#endif  // mozilla_dom_ContentParent_h
