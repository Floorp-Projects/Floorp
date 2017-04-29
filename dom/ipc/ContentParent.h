/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentParent_h
#define mozilla_dom_ContentParent_h

#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/gfx/gfxVarReceiver.h"
#include "mozilla/gfx/GPUProcessListener.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/Attributes.h"
#include "mozilla/FileUtils.h"
#include "mozilla/HalTypes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "nsDataHashtable.h"
#include "nsFrameMessageManager.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsIThreadInternal.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDOMGeoPositionErrorCallback.h"
#include "nsRefPtrHashtable.h"
#include "PermissionMessageUtils.h"
#include "ProfilerControllingProcess.h"
#include "DriverCrashGuard.h"

#define CHILD_PROCESS_SHUTDOWN_MESSAGE NS_LITERAL_STRING("child-process-shutdown")

#define NO_REMOTE_TYPE ""

// These must match the similar ones in E10SUtils.jsm.
#define DEFAULT_REMOTE_TYPE "web"
#define FILE_REMOTE_TYPE "file"
#define EXTENSION_REMOTE_TYPE "extension"

// This must start with the DEFAULT_REMOTE_TYPE above.
#define LARGE_ALLOCATION_REMOTE_TYPE "webLargeAllocation"

class nsConsoleService;
class nsIContentProcessInfo;
class nsICycleCollectorLogSink;
class nsIDumpGCAndCCLogsCallback;
class nsITabParent;
class nsITimer;
class ParentIdleListener;
class nsIWidget;

namespace mozilla {
class PRemoteSpellcheckEngineParent;
#ifdef MOZ_GECKO_PROFILER
class CrossProcessProfilerController;
#endif

#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
class SandboxBroker;
class SandboxBrokerPolicyFactory;
#endif

class PreallocatedProcessManagerImpl;

namespace embedding {
class PrintingParent;
}

namespace ipc {
class OptionalURIParams;
class PFileDescriptorSetParent;
class URIParams;
class TestShellParent;
class CrashReporterHost;
} // namespace ipc

namespace jsipc {
class PJavaScriptParent;
} // namespace jsipc

namespace layers {
struct TextureFactoryIdentifier;
} // namespace layers

namespace layout {
class PRenderFrameParent;
} // namespace layout

namespace dom {

class Element;
class TabParent;
class PStorageParent;
class ClonedMessageData;
class MemoryReport;
class TabContext;
class ContentBridgeParent;
class GetFilesHelper;
class MemoryReportRequestHost;

class ContentParent final : public PContentParent
                          , public nsIContentParent
                          , public nsIObserver
                          , public nsIDOMGeoPositionCallback
                          , public nsIDOMGeoPositionErrorCallback
                          , public gfx::gfxVarReceiver
                          , public mozilla::LinkedListElement<ContentParent>
                          , public gfx::GPUProcessListener
                          , public mozilla::MemoryReportingProcess
                          , public mozilla::ProfilerControllingProcess
{
  typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;
  typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
  typedef mozilla::ipc::PFileDescriptorSetParent PFileDescriptorSetParent;
  typedef mozilla::ipc::TestShellParent TestShellParent;
  typedef mozilla::ipc::URIParams URIParams;
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;

  friend class mozilla::PreallocatedProcessManagerImpl;

public:

  virtual bool IsContentParent() const override { return true; }

  /**
   * Create a subprocess suitable for use later as a content process.
   */
  static already_AddRefed<ContentParent> PreallocateProcess();

  /**
   * Start up the content-process machinery.  This might include
   * scheduling pre-launch tasks.
   */
  static void StartUp();

  /** Shut down the content-process machinery. */
  static void ShutDown();

  /**
   * Ensure that all subprocesses are terminated and their OS
   * resources have been reaped.  This is synchronous and can be
   * very expensive in general.  It also bypasses the normal
   * shutdown process.
   */
  static void JoinAllSubprocesses();

  static uint32_t GetPoolSize(const nsAString& aContentProcessType);

  static uint32_t GetMaxProcessCount(const nsAString& aContentProcessType);

  static bool IsMaxProcessCountReached(const nsAString& aContentProcessType);

  static void ReleaseCachedProcesses();

  /**
   * Picks a random content parent from |aContentParents| with a given |aOpener|
   * respecting the index limit set by |aMaxContentParents|.
   * Returns null if non available.
   */
  static already_AddRefed<ContentParent>
  MinTabSelect(const nsTArray<ContentParent*>& aContentParents,
               ContentParent* aOpener,
               int32_t maxContentParents);

  /**
   * Get or create a content process for:
   * 1. browser iframe
   * 2. remote xul <browser>
   * 3. normal iframe
   */
  static already_AddRefed<ContentParent>
  GetNewOrUsedBrowserProcess(const nsAString& aRemoteType = NS_LITERAL_STRING(NO_REMOTE_TYPE),
                             hal::ProcessPriority aPriority =
                             hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND,
                             ContentParent* aOpener = nullptr);

  /**
   * Get or create a content process for the given TabContext.  aFrameElement
   * should be the frame/iframe element with which this process will
   * associated.
   */
  static TabParent*
  CreateBrowser(const TabContext& aContext,
                Element* aFrameElement,
                ContentParent* aOpenerContentParent,
                TabParent* aSameTabGroupAs,
                uint64_t aNextTabParentId);

  static void GetAll(nsTArray<ContentParent*>& aArray);

  static void GetAllEvenIfDead(nsTArray<ContentParent*>& aArray);

  const nsAString& GetRemoteType() const;

  enum CPIteratorPolicy {
    eLive,
    eAll
  };

  class ContentParentIterator {
  private:
    ContentParent* mCurrent;
    CPIteratorPolicy mPolicy;

  public:
    ContentParentIterator(CPIteratorPolicy aPolicy, ContentParent* aCurrent)
      : mCurrent(aCurrent),
        mPolicy(aPolicy)
    {
    }

    ContentParentIterator begin()
    {
      // Move the cursor to the first element that matches the policy.
      while (mPolicy != eAll && mCurrent && !mCurrent->mIsAlive) {
        mCurrent = mCurrent->LinkedListElement<ContentParent>::getNext();
      }

      return *this;
    }
    ContentParentIterator end()
    {
      return ContentParentIterator(mPolicy, nullptr);
    }

    const ContentParentIterator& operator++()
    {
      MOZ_ASSERT(mCurrent);
      do {
        mCurrent = mCurrent->LinkedListElement<ContentParent>::getNext();
      } while (mPolicy != eAll && mCurrent && !mCurrent->mIsAlive);

      return *this;
    }

    bool operator!=(const ContentParentIterator& aOther)
    {
      MOZ_ASSERT(mPolicy == aOther.mPolicy);
      return mCurrent != aOther.mCurrent;
    }

    ContentParent* operator*()
    {
      return mCurrent;
    }
  };

  static ContentParentIterator AllProcesses(CPIteratorPolicy aPolicy)
  {
    ContentParent* first =
      sContentParents ? sContentParents->getFirst() : nullptr;
    return ContentParentIterator(aPolicy, first);
  }

  static bool IgnoreIPCPrincipal();

  static void NotifyUpdatedDictionaries();

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

  // Let managees query if it is safe to send messages.
  bool IsDestroyed() const { return !mIPCOpen; }

  virtual mozilla::ipc::IPCResult RecvCreateChildProcess(const IPCTabContext& aContext,
                                                         const hal::ProcessPriority& aPriority,
                                                         const TabId& aOpenerTabId,
                                                         const TabId& aTabId,
                                                         ContentParentId* aCpId,
                                                         bool* aIsForBrowser) override;

  virtual mozilla::ipc::IPCResult RecvBridgeToChildProcess(const ContentParentId& aCpId,
                                                           Endpoint<PContentBridgeParent>* aEndpoint) override;

  virtual mozilla::ipc::IPCResult RecvCreateGMPService() override;

  virtual mozilla::ipc::IPCResult RecvLoadPlugin(const uint32_t& aPluginId, nsresult* aRv,
                                                 uint32_t* aRunID,
                                                 Endpoint<PPluginModuleParent>* aEndpoint) override;

  virtual mozilla::ipc::IPCResult RecvConnectPluginBridge(const uint32_t& aPluginId,
                                                          nsresult* aRv,
                                                          Endpoint<PPluginModuleParent>* aEndpoint) override;

  virtual mozilla::ipc::IPCResult RecvGetBlocklistState(const uint32_t& aPluginId,
                                                        uint32_t* aIsBlocklisted) override;

  virtual mozilla::ipc::IPCResult RecvFindPlugins(const uint32_t& aPluginEpoch,
                                                  nsresult* aRv,
                                                  nsTArray<PluginTag>* aPlugins,
                                                  uint32_t* aNewPluginEpoch) override;

  virtual mozilla::ipc::IPCResult RecvUngrabPointer(const uint32_t& aTime) override;

  virtual mozilla::ipc::IPCResult RecvRemovePermission(const IPC::Principal& aPrincipal,
                                                       const nsCString& aPermissionType,
                                                       nsresult* aRv) override;

  void SendStartProfiler(const ProfilerInitParams& aParams) override;
  void SendStopProfiler() override;
  void SendPauseProfiler(const bool& aPause) override;
  void SendGatherProfile() override;

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ContentParent, nsIObserver)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMGEOPOSITIONCALLBACK
  NS_DECL_NSIDOMGEOPOSITIONERRORCALLBACK

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) override;

  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal) override;

  /** Notify that a tab is beginning its destruction sequence. */
  static void NotifyTabDestroying(const TabId& aTabId,
                                  const ContentParentId& aCpId);

  /** Notify that a tab was destroyed during normal operation. */
  void NotifyTabDestroyed(const TabId& aTabId,
                          bool aNotifiedDestroying);

  TestShellParent* CreateTestShell();

  bool DestroyTestShell(TestShellParent* aTestShell);

  TestShellParent* GetTestShellSingleton();

  jsipc::CPOWManager* GetCPOWManager() override;

  static void
  UnregisterRemoteFrame(const TabId& aTabId,
                        const ContentParentId& aCpId,
                        bool aMarkedDestroying);

  void ReportChildAlreadyBlocked();

  bool RequestRunToCompletion();

  bool IsAvailable() const
  {
    return mIsAvailable;
  }
  bool IsAlive() const override;

  virtual bool IsForBrowser() const override
  {
    return mIsForBrowser;
  }

  GeckoChildProcessHost* Process() const
  {
    return mSubprocess;
  }

  ContentParent* Opener() const
  {
    return mOpener;
  }
  nsIContentProcessInfo* ScriptableHelper() const
  {
    return mScriptableHelper;
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

  ContentParentId ChildID() const override { return mChildID; }

  /**
   * Get a user-friendly name for this ContentParent.  We make no guarantees
   * about this name: It might not be unique, apps can spoof special names,
   * etc.  So please don't use this name to make any decisions about the
   * ContentParent based on the value returned here.
   */
  void FriendlyName(nsAString& aName, bool aAnonymize = false);

  virtual void OnChannelError() override;

  virtual mozilla::ipc::IPCResult
  RecvInitCrashReporter(Shmem&& aShmem, const NativeThreadId& aThreadId) override;

  virtual PNeckoParent* AllocPNeckoParent() override;

  virtual mozilla::ipc::IPCResult RecvPNeckoConstructor(PNeckoParent* aActor) override
  {
    return PContentParent::RecvPNeckoConstructor(aActor);
  }

  virtual PPrintingParent* AllocPPrintingParent() override;

  virtual bool DeallocPPrintingParent(PPrintingParent* aActor) override;

#if defined(NS_PRINTING)
  /**
   * @return the PrintingParent for this ContentParent.
   */
  already_AddRefed<embedding::PrintingParent> GetPrintingParent();
#endif

  virtual PChildToParentStreamParent* AllocPChildToParentStreamParent() override;
  virtual bool
  DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor) override;

  virtual PParentToChildStreamParent*
  SendPParentToChildStreamConstructor(PParentToChildStreamParent*) override;

  virtual PFileDescriptorSetParent*
  SendPFileDescriptorSetConstructor(const FileDescriptor&) override;

  virtual PParentToChildStreamParent* AllocPParentToChildStreamParent() override;
  virtual bool
  DeallocPParentToChildStreamParent(PParentToChildStreamParent* aActor) override;

  virtual PHalParent* AllocPHalParent() override;

  virtual mozilla::ipc::IPCResult RecvPHalConstructor(PHalParent* aActor) override
  {
    return PContentParent::RecvPHalConstructor(aActor);
  }

  virtual PHeapSnapshotTempFileHelperParent*
  AllocPHeapSnapshotTempFileHelperParent() override;

  virtual PStorageParent* AllocPStorageParent() override;

  virtual mozilla::ipc::IPCResult RecvPStorageConstructor(PStorageParent* aActor) override
  {
    return PContentParent::RecvPStorageConstructor(aActor);
  }

  virtual PJavaScriptParent*
  AllocPJavaScriptParent() override;

  virtual mozilla::ipc::IPCResult
  RecvPJavaScriptConstructor(PJavaScriptParent* aActor) override
  {
    return PContentParent::RecvPJavaScriptConstructor(aActor);
  }

  virtual PRemoteSpellcheckEngineParent* AllocPRemoteSpellcheckEngineParent() override;

  virtual mozilla::ipc::IPCResult RecvRecordingDeviceEvents(const nsString& aRecordingStatus,
                                                            const nsString& aPageURL,
                                                            const bool& aIsAudio,
                                                            const bool& aIsVideo) override;

  bool CycleCollectWithLogs(bool aDumpAllTraces,
                            nsICycleCollectorLogSink* aSink,
                            nsIDumpGCAndCCLogsCallback* aCallback);

  virtual PBlobParent*
  SendPBlobConstructor(PBlobParent* aActor,
                       const BlobConstructorParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvUnregisterRemoteFrame(const TabId& aTabId,
                                                            const ContentParentId& aCpId,
                                                            const bool& aMarkedDestroying) override;

  virtual mozilla::ipc::IPCResult RecvNotifyTabDestroying(const TabId& aTabId,
                                                          const ContentParentId& aCpId) override;

  virtual mozilla::ipc::IPCResult RecvTabChildNotReady(const TabId& aTabId) override;

  nsTArray<TabContext> GetManagedTabContext();

  virtual POfflineCacheUpdateParent*
  AllocPOfflineCacheUpdateParent(const URIParams& aManifestURI,
                                 const URIParams& aDocumentURI,
                                 const PrincipalInfo& aLoadingPrincipalInfo,
                                 const bool& aStickDocument) override;

  virtual mozilla::ipc::IPCResult
  RecvPOfflineCacheUpdateConstructor(POfflineCacheUpdateParent* aActor,
                                     const URIParams& aManifestURI,
                                     const URIParams& aDocumentURI,
                                     const PrincipalInfo& aLoadingPrincipal,
                                     const bool& stickDocument) override;

  virtual bool
  DeallocPOfflineCacheUpdateParent(POfflineCacheUpdateParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvSetOfflinePermission(const IPC::Principal& principal) override;

  virtual mozilla::ipc::IPCResult RecvFinishShutdown() override;

  void MaybeInvokeDragSession(TabParent* aParent);

  virtual PContentPermissionRequestParent*
  AllocPContentPermissionRequestParent(const InfallibleTArray<PermissionRequest>& aRequests,
                                       const IPC::Principal& aPrincipal,
                                       const TabId& aTabId) override;

  virtual bool
  DeallocPContentPermissionRequestParent(PContentPermissionRequestParent* actor) override;

  virtual bool HandleWindowsMessages(const Message& aMsg) const override;

  void ForkNewProcess(bool aBlocking);

  virtual mozilla::ipc::IPCResult
  RecvCreateWindow(PBrowserParent* aThisTabParent,
                   PBrowserParent* aNewTab,
                   layout::PRenderFrameParent* aRenderFrame,
                   const uint32_t& aChromeFlags,
                   const bool& aCalledFromJS,
                   const bool& aPositionSpecified,
                   const bool& aSizeSpecified,
                   const nsCString& aFeatures,
                   const nsCString& aBaseURI,
                   const OriginAttributes& aOpenerOriginAttributes,
                   const float& aFullZoom,
                   nsresult* aResult,
                   bool* aWindowIsNew,
                   InfallibleTArray<FrameScriptInfo>* aFrameScripts,
                   nsCString* aURLToLoad,
                   layers::TextureFactoryIdentifier* aTextureFactoryIdentifier,
                   uint64_t* aLayersId,
                   mozilla::layers::CompositorOptions* aCompositorOptions) override;

  virtual mozilla::ipc::IPCResult RecvCreateWindowInDifferentProcess(
    PBrowserParent* aThisTab,
    const uint32_t& aChromeFlags,
    const bool& aCalledFromJS,
    const bool& aPositionSpecified,
    const bool& aSizeSpecified,
    const URIParams& aURIToLoad,
    const nsCString& aFeatures,
    const nsCString& aBaseURI,
    const OriginAttributes& aOpenerOriginAttributes,
    const float& aFullZoom) override;

  static bool AllocateLayerTreeId(TabParent* aTabParent, uint64_t* aId);

  static void
  BroadcastBlobURLRegistration(const nsACString& aURI,
                               BlobImpl* aBlobImpl,
                               nsIPrincipal* aPrincipal,
                               ContentParent* aIgnoreThisCP = nullptr);

  static void
  BroadcastBlobURLUnregistration(const nsACString& aURI,
                                 ContentParent* aIgnoreThisCP = nullptr);

  virtual mozilla::ipc::IPCResult
  RecvStoreAndBroadcastBlobURLRegistration(const nsCString& aURI,
                                           const IPCBlob& aBlob,
                                           const Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult
  RecvUnstoreAndBroadcastBlobURLUnregistration(const nsCString& aURI) override;

  virtual mozilla::ipc::IPCResult
  RecvBroadcastLocalStorageChange(const nsString& aDocumentURI,
                                  const nsString& aKey,
                                  const nsString& aOldValue,
                                  const nsString& aNewValue,
                                  const IPC::Principal& aPrincipal,
                                  const bool& aIsPrivate) override;

  virtual mozilla::ipc::IPCResult
  RecvGetA11yContentId(uint32_t* aContentId) override;

  virtual mozilla::ipc::IPCResult
  RecvA11yHandlerControl(const uint32_t& aPid,
                         const IHandlerControlHolder& aHandlerControl) override;

  virtual int32_t Pid() const override;

  // PURLClassifierParent.
  virtual PURLClassifierParent*
  AllocPURLClassifierParent(const Principal& aPrincipal,
                            const bool& aUseTrackingProtection,
                            bool* aSuccess) override;
  virtual mozilla::ipc::IPCResult
  RecvPURLClassifierConstructor(PURLClassifierParent* aActor,
                                const Principal& aPrincipal,
                                const bool& aUseTrackingProtection,
                                bool* aSuccess) override;

  // PURLClassifierLocalParent.
  virtual PURLClassifierLocalParent*
  AllocPURLClassifierLocalParent(const URIParams& aURI,
                                 const nsCString& aTables) override;
  virtual mozilla::ipc::IPCResult
  RecvPURLClassifierLocalConstructor(PURLClassifierLocalParent* aActor,
                                     const URIParams& aURI,
                                     const nsCString& aTables) override;

  virtual bool SendActivate(PBrowserParent* aTab) override
  {
    return PContentParent::SendActivate(aTab);
  }

  virtual bool SendDeactivate(PBrowserParent* aTab) override
  {
    return PContentParent::SendDeactivate(aTab);
  }

  virtual bool SendParentActivated(PBrowserParent* aTab,
                                   const bool& aActivated) override
  {
    return PContentParent::SendParentActivated(aTab, aActivated);
  }

  virtual bool
  DeallocPURLClassifierLocalParent(PURLClassifierLocalParent* aActor) override;

  virtual bool
  DeallocPURLClassifierParent(PURLClassifierParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvClassifyLocal(const URIParams& aURI,
                    const nsCString& aTables,
                    nsresult* aRv,
                    nsTArray<nsCString>* aResults) override;

  virtual mozilla::ipc::IPCResult
  RecvAllocPipelineId(RefPtr<AllocPipelineIdPromise>&& aPromise) override;

  // Use the PHangMonitor channel to ask the child to repaint a tab.
  void ForceTabPaint(TabParent* aTabParent, uint64_t aLayerObserverEpoch);

  // This function is called when we are about to load a document from an
  // HTTP(S), FTP or wyciwyg channel for a content process.  It is a useful
  // place to start to kick off work as early as possible in response to such
  // document loads.
  nsresult AboutToLoadDocumentForChild(nsIChannel* aChannel);

  nsresult TransmitPermissionsForPrincipal(nsIPrincipal* aPrincipal);

protected:
  void OnChannelConnected(int32_t pid) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

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
  static nsClassHashtable<nsStringHashKey, nsTArray<ContentParent*>>* sBrowserContentParents;
  static nsTArray<ContentParent*>* sPrivateContent;
  static StaticAutoPtr<LinkedList<ContentParent> > sContentParents;

  static void JoinProcessesIOThread(const nsTArray<ContentParent*>* aProcesses,
                                    Monitor* aMonitor, bool* aDone);

  static hal::ProcessPriority GetInitialProcessPriority(Element* aFrameElement);

  static ContentBridgeParent* CreateContentBridgeParent(const TabContext& aContext,
                                                        const hal::ProcessPriority& aPriority,
                                                        const TabId& aOpenerTabId,
                                                        const TabId& aTabId);

  // Hide the raw constructor methods since we don't want client code
  // using them.
  virtual PBrowserParent* SendPBrowserConstructor(
      PBrowserParent* actor,
      const TabId& aTabId,
      const TabId& aSameTabGroupsAs,
      const IPCTabContext& context,
      const uint32_t& chromeFlags,
      const ContentParentId& aCpId,
      const bool& aIsForBrowser) override;
  using PContentParent::SendPTestShellConstructor;

  mozilla::ipc::IPCResult
  CommonCreateWindow(PBrowserParent* aThisTab,
                     bool aSetOpener,
                     const uint32_t& aChromeFlags,
                     const bool& aCalledFromJS,
                     const bool& aPositionSpecified,
                     const bool& aSizeSpecified,
                     nsIURI* aURIToLoad,
                     const nsCString& aFeatures,
                     const nsCString& aBaseURI,
                     const OriginAttributes& aOpenerOriginAttributes,
                     const float& aFullZoom,
                     uint64_t aNextTabParentId,
                     nsresult& aResult,
                     nsCOMPtr<nsITabParent>& aNewTabParent,
                     bool* aWindowIsNew);

  FORWARD_SHMEM_ALLOCATOR_TO(PContentParent)

  ContentParent(ContentParent* aOpener,
                const nsAString& aRemoteType);

  // Launch the subprocess and associated initialization.
  // Returns false if the process fails to start.
  bool LaunchSubprocess(hal::ProcessPriority aInitialPriority = hal::PROCESS_PRIORITY_FOREGROUND);

  // Common initialization after sub process launch or adoption.
  void InitInternal(ProcessPriority aPriority,
                    bool aSetupOffMainThreadCompositing,
                    bool aSendRegisteredChrome);

  virtual ~ContentParent();

  void Init();

  // Some information could be sent to content very early, it
  // should be send from this function. This function should only be
  // called after the process has been transformed to browser.
  void ForwardKnownInfo();

  // Set the child process's priority and then check whether the child is
  // still alive.  Returns true if the process is still alive, and false
  // otherwise.  If you pass a FOREGROUND* priority here, it's (hopefully)
  // unlikely that the process will be killed after this point.
  bool SetPriorityAndCheckIsAlive(hal::ProcessPriority aPriority);

  /**
   * Decide whether the process should be kept alive even when it would normally
   * be shut down, for example when all its tabs are closed.
   */
  bool ShouldKeepProcessAlive() const;

  /**
   * Mark this ContentParent as "troubled". This means that it is still alive,
   * but it won't be returned for new tabs in GetNewOrUsedBrowserProcess.
   */
  void MarkAsTroubled();

  /**
   * Mark this ContentParent as dead for the purposes of Get*().
   * This method is idempotent.
   */
  void MarkAsDead();

  /**
   * How we will shut down this ContentParent and its subprocess.
   */
  enum ShutDownMethod
  {
    // Send a shutdown message and wait for FinishShutdown call back.
    SEND_SHUTDOWN_MESSAGE,
    // Close the channel ourselves and let the subprocess clean up itself.
    CLOSE_CHANNEL,
    // Close the channel with error and let the subprocess clean up itself.
    CLOSE_CHANNEL_WITH_ERROR,
  };

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
  void EnsurePermissionsByKey(const nsCString& aKey);

  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);

  static bool AllocateLayerTreeId(ContentParent* aContent,
                                  TabParent* aTopLevel, const TabId& aTabId,
                                  uint64_t* aId);

  /**
   * Get or create the corresponding content parent array to |aContentProcessType|.
   */
  static nsTArray<ContentParent*>& GetOrCreatePool(const nsAString& aContentProcessType);

  virtual mozilla::ipc::IPCResult RecvInitBackground(Endpoint<mozilla::ipc::PBackgroundParent>&& aEndpoint) override;

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport) override;
  mozilla::ipc::IPCResult RecvFinishMemoryReport(const uint32_t& aGeneration) override;

  virtual bool
  DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*) override;

  virtual bool
  DeallocPRemoteSpellcheckEngineParent(PRemoteSpellcheckEngineParent*) override;

  virtual PBrowserParent* AllocPBrowserParent(const TabId& aTabId,
                                              const TabId& aSameTabGroupAs,
                                              const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const ContentParentId& aCpId,
                                              const bool& aIsForBrowser) override;

  virtual bool DeallocPBrowserParent(PBrowserParent* frame) override;

  virtual PBlobParent*
  AllocPBlobParent(const BlobConstructorParams& aParams) override;

  virtual bool DeallocPBlobParent(PBlobParent* aActor) override;

  virtual PMemoryStreamParent*
  AllocPMemoryStreamParent(const uint64_t& aSize) override;

  virtual bool DeallocPMemoryStreamParent(PMemoryStreamParent* aActor) override;

  virtual PIPCBlobInputStreamParent*
  SendPIPCBlobInputStreamConstructor(PIPCBlobInputStreamParent* aActor,
                                     const nsID& aID,
                                     const uint64_t& aSize) override;

  virtual PIPCBlobInputStreamParent*
  AllocPIPCBlobInputStreamParent(const nsID& aID,
                                 const uint64_t& aSize) override;

  virtual bool
  DeallocPIPCBlobInputStreamParent(PIPCBlobInputStreamParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvPBlobConstructor(PBlobParent* aActor,
                       const BlobConstructorParams& params) override;

  virtual mozilla::ipc::IPCResult RecvNSSU2FTokenIsCompatibleVersion(const nsString& aVersion,
                                                                     bool* aIsCompatible) override;

  virtual mozilla::ipc::IPCResult RecvNSSU2FTokenIsRegistered(nsTArray<uint8_t>&& aKeyHandle,
                                                              nsTArray<uint8_t>&& aApplication,
                                                              bool* aIsValidKeyHandle) override;

  virtual mozilla::ipc::IPCResult RecvNSSU2FTokenRegister(nsTArray<uint8_t>&& aApplication,
                                                          nsTArray<uint8_t>&& aChallenge,
                                                          nsTArray<uint8_t>* aRegistration) override;

  virtual mozilla::ipc::IPCResult RecvNSSU2FTokenSign(nsTArray<uint8_t>&& aApplication,
                                                      nsTArray<uint8_t>&& aChallenge,
                                                      nsTArray<uint8_t>&& aKeyHandle,
                                                      nsTArray<uint8_t>* aSignature) override;

  virtual mozilla::ipc::IPCResult RecvIsSecureURI(const uint32_t& aType, const URIParams& aURI,
                                                  const uint32_t& aFlags,
                                                  const OriginAttributes& aOriginAttributes,
                                                  bool* aIsSecureURI) override;

  virtual mozilla::ipc::IPCResult RecvAccumulateMixedContentHSTS(const URIParams& aURI,
                                                                 const bool& aActive,
                                                                 const bool& aHSTSPriming,
                                                                 const OriginAttributes& aOriginAttributes) override;

  virtual bool DeallocPHalParent(PHalParent*) override;

  virtual bool
  DeallocPHeapSnapshotTempFileHelperParent(PHeapSnapshotTempFileHelperParent*) override;

  virtual PCycleCollectWithLogsParent*
  AllocPCycleCollectWithLogsParent(const bool& aDumpAllTraces,
                                   const FileDescriptor& aGCLog,
                                   const FileDescriptor& aCCLog) override;

  virtual bool
  DeallocPCycleCollectWithLogsParent(PCycleCollectWithLogsParent* aActor) override;

  virtual PTestShellParent* AllocPTestShellParent() override;

  virtual bool DeallocPTestShellParent(PTestShellParent* shell) override;

  virtual bool DeallocPNeckoParent(PNeckoParent* necko) override;

  virtual PPSMContentDownloaderParent*
  AllocPPSMContentDownloaderParent(const uint32_t& aCertType) override;

  virtual bool
  DeallocPPSMContentDownloaderParent(PPSMContentDownloaderParent* aDownloader) override;

  virtual PExternalHelperAppParent*
  AllocPExternalHelperAppParent(const OptionalURIParams& aUri,
                                const nsCString& aMimeContentType,
                                const nsCString& aContentDisposition,
                                const uint32_t& aContentDispositionHint,
                                const nsString& aContentDispositionFilename,
                                const bool& aForceSave,
                                const int64_t& aContentLength,
                                const bool& aWasFileChannel,
                                const OptionalURIParams& aReferrer,
                                PBrowserParent* aBrowser) override;

  virtual bool
  DeallocPExternalHelperAppParent(PExternalHelperAppParent* aService) override;

  virtual PHandlerServiceParent* AllocPHandlerServiceParent() override;

  virtual bool DeallocPHandlerServiceParent(PHandlerServiceParent*) override;

  virtual PMediaParent* AllocPMediaParent() override;

  virtual bool DeallocPMediaParent(PMediaParent* aActor) override;

  virtual bool DeallocPStorageParent(PStorageParent* aActor) override;

  virtual PPresentationParent* AllocPPresentationParent() override;

  virtual bool DeallocPPresentationParent(PPresentationParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvPPresentationConstructor(PPresentationParent* aActor) override;

  virtual PFlyWebPublishedServerParent*
    AllocPFlyWebPublishedServerParent(const nsString& name,
                                      const FlyWebPublishOptions& params) override;

  virtual bool DeallocPFlyWebPublishedServerParent(PFlyWebPublishedServerParent* aActor) override;

  virtual PSpeechSynthesisParent* AllocPSpeechSynthesisParent() override;

  virtual bool
  DeallocPSpeechSynthesisParent(PSpeechSynthesisParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvPSpeechSynthesisConstructor(PSpeechSynthesisParent* aActor) override;

  virtual PWebBrowserPersistDocumentParent*
  AllocPWebBrowserPersistDocumentParent(PBrowserParent* aBrowser,
                                        const uint64_t& aOuterWindowID) override;

  virtual bool
  DeallocPWebBrowserPersistDocumentParent(PWebBrowserPersistDocumentParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvGetGfxVars(InfallibleTArray<GfxVarUpdate>* aVars) override;

  virtual mozilla::ipc::IPCResult RecvReadFontList(InfallibleTArray<FontListEntry>* retValue) override;

  virtual mozilla::ipc::IPCResult RecvSetClipboard(const IPCDataTransfer& aDataTransfer,
                                                   const bool& aIsPrivateData,
                                                   const IPC::Principal& aRequestingPrincipal,
                                                   const int32_t& aWhichClipboard) override;

  virtual mozilla::ipc::IPCResult RecvGetClipboard(nsTArray<nsCString>&& aTypes,
                                                   const int32_t& aWhichClipboard,
                                                   IPCDataTransfer* aDataTransfer) override;

  virtual mozilla::ipc::IPCResult RecvEmptyClipboard(const int32_t& aWhichClipboard) override;

  virtual mozilla::ipc::IPCResult RecvClipboardHasType(nsTArray<nsCString>&& aTypes,
                                                       const int32_t& aWhichClipboard,
                                                       bool* aHasType) override;

  virtual mozilla::ipc::IPCResult RecvGetSystemColors(const uint32_t& colorsCount,
                                                      InfallibleTArray<uint32_t>* colors) override;

  virtual mozilla::ipc::IPCResult RecvGetIconForExtension(const nsCString& aFileExt,
                                                          const uint32_t& aIconSize,
                                                          InfallibleTArray<uint8_t>* bits) override;

  virtual mozilla::ipc::IPCResult RecvGetShowPasswordSetting(bool* showPassword) override;

  virtual mozilla::ipc::IPCResult RecvStartVisitedQuery(const URIParams& uri) override;

  virtual mozilla::ipc::IPCResult RecvVisitURI(const URIParams& uri,
                                               const OptionalURIParams& referrer,
                                               const uint32_t& flags) override;

  virtual mozilla::ipc::IPCResult RecvSetURITitle(const URIParams& uri,
                                                  const nsString& title) override;

  bool HasNotificationPermission(const IPC::Principal& aPrincipal);

  virtual mozilla::ipc::IPCResult RecvShowAlert(const AlertNotificationType& aAlert) override;

  virtual mozilla::ipc::IPCResult RecvCloseAlert(const nsString& aName,
                                                 const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvDisableNotifications(const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvOpenNotificationSettings(const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvLoadURIExternal(const URIParams& uri,
                                                      PBrowserParent* windowContext) override;
  virtual mozilla::ipc::IPCResult RecvExtProtocolChannelConnectParent(const uint32_t& registrarId) override;

  virtual mozilla::ipc::IPCResult RecvSyncMessage(const nsString& aMsg,
                                                  const ClonedMessageData& aData,
                                                  InfallibleTArray<CpowEntry>&& aCpows,
                                                  const IPC::Principal& aPrincipal,
                                                  nsTArray<StructuredCloneData>* aRetvals) override;

  virtual mozilla::ipc::IPCResult RecvRpcMessage(const nsString& aMsg,
                                                 const ClonedMessageData& aData,
                                                 InfallibleTArray<CpowEntry>&& aCpows,
                                                 const IPC::Principal& aPrincipal,
                                                 nsTArray<StructuredCloneData>* aRetvals) override;

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                                   InfallibleTArray<CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData) override;

  virtual mozilla::ipc::IPCResult RecvAddGeolocationListener(const IPC::Principal& aPrincipal,
                                                             const bool& aHighAccuracy) override;
  virtual mozilla::ipc::IPCResult RecvRemoveGeolocationListener() override;

  virtual mozilla::ipc::IPCResult RecvSetGeolocationHigherAccuracy(const bool& aEnable) override;

  virtual mozilla::ipc::IPCResult RecvConsoleMessage(const nsString& aMessage) override;

  virtual mozilla::ipc::IPCResult RecvScriptError(const nsString& aMessage,
                                                  const nsString& aSourceName,
                                                  const nsString& aSourceLine,
                                                  const uint32_t& aLineNumber,
                                                  const uint32_t& aColNumber,
                                                  const uint32_t& aFlags,
                                                  const nsCString& aCategory) override;

  virtual mozilla::ipc::IPCResult RecvPrivateDocShellsExist(const bool& aExist) override;

  virtual mozilla::ipc::IPCResult RecvFirstIdle() override;

  virtual mozilla::ipc::IPCResult RecvAudioChannelChangeDefVolChannel(const int32_t& aChannel,
                                                                      const bool& aHidden) override;

  virtual mozilla::ipc::IPCResult RecvAudioChannelServiceStatus(const bool& aTelephonyChannel,
                                                                const bool& aContentOrNormalChannel,
                                                                const bool& aAnyChannel) override;

  virtual mozilla::ipc::IPCResult RecvKeywordToURI(const nsCString& aKeyword,
                                                   nsString* aProviderName,
                                                   OptionalIPCStream* aPostData,
                                                   OptionalURIParams* aURI) override;

  virtual mozilla::ipc::IPCResult RecvNotifyKeywordSearchLoading(const nsString &aProvider,
                                                                 const nsString &aKeyword) override;

  virtual mozilla::ipc::IPCResult RecvCopyFavicon(const URIParams& aOldURI,
                                                  const URIParams& aNewURI,
                                                  const IPC::Principal& aLoadingPrincipal,
                                                  const bool& aInPrivateBrowsing) override;

  virtual void ProcessingError(Result aCode, const char* aMsgName) override;

  virtual mozilla::ipc::IPCResult RecvAllocateLayerTreeId(const ContentParentId& aCpId,
                                                          const TabId& aTabId,
                                                          uint64_t* aId) override;

  virtual mozilla::ipc::IPCResult RecvDeallocateLayerTreeId(const uint64_t& aId) override;

  virtual mozilla::ipc::IPCResult RecvGraphicsError(const nsCString& aError) override;

  virtual mozilla::ipc::IPCResult
  RecvBeginDriverCrashGuard(const uint32_t& aGuardType,
                            bool* aOutCrashed) override;

  virtual mozilla::ipc::IPCResult RecvEndDriverCrashGuard(const uint32_t& aGuardType) override;

  virtual mozilla::ipc::IPCResult RecvAddIdleObserver(const uint64_t& observerId,
                                                      const uint32_t& aIdleTimeInS) override;

  virtual mozilla::ipc::IPCResult RecvRemoveIdleObserver(const uint64_t& observerId,
                                                         const uint32_t& aIdleTimeInS) override;

  virtual mozilla::ipc::IPCResult
  RecvBackUpXResources(const FileDescriptor& aXSocketFd) override;

  virtual mozilla::ipc::IPCResult
  RecvRequestAnonymousTemporaryFile(const uint64_t& aID) override;

  virtual mozilla::ipc::IPCResult
  RecvKeygenProcessValue(const nsString& oldValue, const nsString& challenge,
                         const nsString& keytype, const nsString& keyparams,
                         nsString* newValue) override;

  virtual mozilla::ipc::IPCResult
  RecvKeygenProvideContent(nsString* aAttribute,
                           nsTArray<nsString>* aContent) override;

  virtual PFileDescriptorSetParent*
  AllocPFileDescriptorSetParent(const mozilla::ipc::FileDescriptor&) override;

  virtual bool
  DeallocPFileDescriptorSetParent(PFileDescriptorSetParent*) override;

  virtual PWebrtcGlobalParent* AllocPWebrtcGlobalParent() override;
  virtual bool DeallocPWebrtcGlobalParent(PWebrtcGlobalParent *aActor) override;


  virtual mozilla::ipc::IPCResult RecvUpdateDropEffect(const uint32_t& aDragAction,
                                                       const uint32_t& aDropEffect) override;

  virtual mozilla::ipc::IPCResult RecvProfile(const nsCString& aProfile,
                                              const bool& aIsExitProfile) override;

  virtual mozilla::ipc::IPCResult RecvGetGraphicsDeviceInitData(ContentDeviceData* aOut) override;

  virtual mozilla::ipc::IPCResult RecvGetAndroidSystemInfo(AndroidSystemInfo* aInfo) override;

  virtual mozilla::ipc::IPCResult RecvNotifyBenchmarkResult(const nsString& aCodecName,
                                                            const uint32_t& aDecodeFPS) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushObservers(const nsCString& aScope,
                                                          const IPC::Principal& aPrincipal,
                                                          const nsString& aMessageId) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushObserversWithData(const nsCString& aScope,
                                                                  const IPC::Principal& aPrincipal,
                                                                  const nsString& aMessageId,
                                                                  InfallibleTArray<uint8_t>&& aData) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushSubscriptionChangeObservers(const nsCString& aScope,
                                                                            const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushSubscriptionModifiedObservers(const nsCString& aScope,
                                                                              const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvNotifyLowMemory() override;

  virtual mozilla::ipc::IPCResult RecvGetFilesRequest(const nsID& aID,
                                                      const nsString& aDirectoryPath,
                                                      const bool& aRecursiveFlag) override;

  virtual mozilla::ipc::IPCResult RecvDeleteGetFilesRequest(const nsID& aID) override;

  virtual mozilla::ipc::IPCResult
  RecvFileCreationRequest(const nsID& aID, const nsString& aFullPath,
                          const nsString& aType, const nsString& aName,
                          const bool& aLastModifiedPassed,
                          const int64_t& aLastModified,
                          const bool& aExistenceCheck,
                          const bool& aIsFromNsIFile) override;

  virtual mozilla::ipc::IPCResult RecvAccumulateChildHistograms(
    InfallibleTArray<Accumulation>&& aAccumulations) override;
  virtual mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistograms(
    InfallibleTArray<KeyedAccumulation>&& aAccumulations) override;
  virtual mozilla::ipc::IPCResult RecvUpdateChildScalars(
    InfallibleTArray<ScalarAction>&& aScalarActions) override;
  virtual mozilla::ipc::IPCResult RecvUpdateChildKeyedScalars(
    InfallibleTArray<KeyedScalarAction>&& aScalarActions) override;
  virtual mozilla::ipc::IPCResult RecvRecordChildEvents(
    nsTArray<ChildEventData>&& events) override;
public:
  void SendGetFilesResponseAndForget(const nsID& aID,
                                     const GetFilesResponseResult& aResult);

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const MaybeFileDesc& aDMDFile) override;

private:

  // If you add strong pointers to cycle collected objects here, be sure to
  // release these objects in ShutDownProcess.  See the comment there for more
  // details.

  GeckoChildProcessHost* mSubprocess;
  const TimeStamp mLaunchTS; // used to calculate time to start content process
  ContentParent* mOpener;

  nsString mRemoteType;

  ContentParentId mChildID;
  int32_t mGeolocationWatchID;

  nsCString mKillHardAnnotation;

  // After we initiate shutdown, we also start a timer to ensure
  // that even content processes that are 100% blocked (say from
  // SIGSTOP), are still killed eventually.  This task enforces that
  // timer.
  nsCOMPtr<nsITimer> mForceKillTimer;
  // How many tabs we're waiting to finish their destruction
  // sequence.  Precisely, how many TabParents have called
  // NotifyTabDestroying() but not called NotifyTabDestroyed().
  int32_t mNumDestroyingTabs;
  // True only while this process is in "good health" and may be used for
  // new remote tabs.
  bool mIsAvailable;
  // True only while remote content is being actively used from this process.
  // After mIsAlive goes to false, some previously scheduled IPC traffic may
  // still pass through.
  bool mIsAlive;

  bool mIsForBrowser;

  // These variables track whether we've called Close() and KillHard() on our
  // channel.
  bool mCalledClose;
  bool mCalledKillHard;
  bool mCreatedPairedMinidumps;
  bool mShutdownPending;
  bool mIPCOpen;

  RefPtr<nsConsoleService>  mConsoleService;
  nsConsoleService* GetConsoleService();
  nsCOMPtr<nsIContentProcessInfo> mScriptableHelper;

  nsTArray<nsCOMPtr<nsIObserver>> mIdleListeners;

#ifdef MOZ_X11
  // Dup of child's X socket, used to scope its resources to this
  // object instead of the child process's lifetime.
  ScopedClose mChildXSocketFdDup;
#endif

  PProcessHangMonitorParent* mHangMonitorActor;

#ifdef MOZ_GECKO_PROFILER
  UniquePtr<mozilla::CrossProcessProfilerController> mProfilerController;
#endif

  UniquePtr<gfx::DriverCrashGuard> mDriverCrashGuard;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;

#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
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

  nsTHashtable<nsCStringHashKey> mActivePermissionKeys;

  nsTArray<nsCString> mBlobURLs;
#ifdef MOZ_CRASHREPORTER
  UniquePtr<mozilla::ipc::CrashReporterHost> mCrashReporter;
#endif

  static uint64_t sNextTabParentId;
  static nsDataHashtable<nsUint64HashKey, TabParent*> sNextTabParents;
};

} // namespace dom
} // namespace mozilla

class ParentIdleListener : public nsIObserver
{
  friend class mozilla::dom::ContentParent;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ParentIdleListener(mozilla::dom::ContentParent* aParent,
                     uint64_t aObserver, uint32_t aTime)
    : mParent(aParent), mObserver(aObserver), mTime(aTime)
  {}

private:
  virtual ~ParentIdleListener() {}

  RefPtr<mozilla::dom::ContentParent> mParent;
  uint64_t mObserver;
  uint32_t mTime;
};

#endif // mozilla_dom_ContentParent_h
