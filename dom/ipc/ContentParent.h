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
#include "mozilla/StaticPtr.h"
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
#include "DriverCrashGuard.h"

#define CHILD_PROCESS_SHUTDOWN_MESSAGE NS_LITERAL_STRING("child-process-shutdown")

class mozIApplication;
class nsConsoleService;
class nsICycleCollectorLogSink;
class nsIDumpGCAndCCLogsCallback;
class nsITimer;
class ParentIdleListener;
class nsIWidget;

namespace mozilla {
class PRemoteSpellcheckEngineParent;
#ifdef MOZ_ENABLE_PROFILER_SPS
class ProfileGatherer;
#endif

#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
class SandboxBroker;
class SandboxBrokerPolicyFactory;
#endif

namespace embedding {
class PrintingParent;
}

namespace ipc {
class OptionalURIParams;
class PFileDescriptorSetParent;
class URIParams;
class TestShellParent;
} // namespace ipc

namespace jsipc {
class PJavaScriptParent;
} // namespace jsipc

namespace layers {
class PSharedBufferManagerParent;
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

class ContentParent final : public PContentParent
                          , public nsIContentParent
                          , public nsIObserver
                          , public nsIDOMGeoPositionCallback
                          , public nsIDOMGeoPositionErrorCallback
                          , public gfx::gfxVarReceiver
                          , public mozilla::LinkedListElement<ContentParent>
                          , public gfx::GPUProcessListener
{
  typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;
  typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
  typedef mozilla::ipc::PFileDescriptorSetParent PFileDescriptorSetParent;
  typedef mozilla::ipc::TestShellParent TestShellParent;
  typedef mozilla::ipc::URIParams URIParams;
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;

public:

  virtual bool IsContentParent() const override { return true; }

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

  static bool PreallocatedProcessReady();

  /**
   * Get or create a content process for:
   * 1. browser iframe
   * 2. remote xul <browser>
   * 3. normal iframe
   */
  static already_AddRefed<ContentParent>
  GetNewOrUsedBrowserProcess(bool aForBrowserElement = false,
                             hal::ProcessPriority aPriority =
                             hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND,
                             ContentParent* aOpener = nullptr);

  /**
   * Create a subprocess suitable for use as a preallocated app process.
   */
  static already_AddRefed<ContentParent> PreallocateAppProcess();

  /**
   * Get or create a content process for the given TabContext.  aFrameElement
   * should be the frame/iframe element with which this process will
   * associated.
   */
  static TabParent*
  CreateBrowserOrApp(const TabContext& aContext,
                     Element* aFrameElement,
                     ContentParent* aOpenerContentParent);

  static void GetAll(nsTArray<ContentParent*>& aArray);

  static void GetAllEvenIfDead(nsTArray<ContentParent*>& aArray);

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

  virtual bool RecvCreateChildProcess(const IPCTabContext& aContext,
                                      const hal::ProcessPriority& aPriority,
                                      const TabId& aOpenerTabId,
                                      ContentParentId* aCpId,
                                      bool* aIsForApp,
                                      bool* aIsForBrowser,
                                      TabId* aTabId) override;

  virtual bool RecvBridgeToChildProcess(const ContentParentId& aCpId) override;

  virtual bool RecvCreateGMPService() override;

  virtual bool RecvGetGMPPluginVersionForAPI(const nsCString& aAPI,
                                             nsTArray<nsCString>&& aTags,
                                             bool* aHasPlugin,
                                             nsCString* aVersion) override;

  virtual bool RecvIsGMPPresentOnDisk(const nsString& aKeySystem,
                                      const nsCString& aVersion,
                                      bool* aIsPresent,
                                      nsCString* aMessage) override;

  virtual bool RecvLoadPlugin(const uint32_t& aPluginId, nsresult* aRv,
                              uint32_t* aRunID) override;

  virtual bool RecvConnectPluginBridge(const uint32_t& aPluginId,
                                       nsresult* aRv) override;

  virtual bool RecvGetBlocklistState(const uint32_t& aPluginId,
                                     uint32_t* aIsBlocklisted) override;

  virtual bool RecvFindPlugins(const uint32_t& aPluginEpoch,
                               nsresult* aRv,
                               nsTArray<PluginTag>* aPlugins,
                               uint32_t* aNewPluginEpoch) override;

  virtual bool RecvUngrabPointer(const uint32_t& aTime) override;

  virtual bool RecvRemovePermission(const IPC::Principal& aPrincipal,
                                    const nsCString& aPermissionType,
                                    nsresult* aRv) override;

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

  virtual bool CheckPermission(const nsAString& aPermission) override;

  virtual bool CheckManifestURL(const nsAString& aManifestURL) override;

  virtual bool CheckAppHasPermission(const nsAString& aPermission) override;

  virtual bool CheckAppHasStatus(unsigned short aStatus) override;

  virtual bool KillChild() override;

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

  static TabId
  AllocateTabId(const TabId& aOpenerTabId,
                const IPCTabContext& aContext,
                const ContentParentId& aCpId);

  static void
  DeallocateTabId(const TabId& aTabId,
                  const ContentParentId& aCpId,
                  bool aMarkedDestroying);

  /*
   * Add the appId's reference count by the given ContentParentId and TabId
   */
  static bool
  PermissionManagerAddref(const ContentParentId& aCpId, const TabId& aTabId);

  /*
   * Release the appId's reference count by the given ContentParentId and TabId
   */
  static bool
  PermissionManagerRelease(const ContentParentId& aCpId, const TabId& aTabId);

  void ReportChildAlreadyBlocked();

  bool RequestRunToCompletion();

  bool IsAlive() const;

  virtual bool IsForApp() const override;

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

  bool NeedsPermissionsUpdate() const
  {
    return mSendPermissionUpdates;
  }

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

  const nsString& AppManifestURL() const { return mAppManifestURL; }

  bool IsPreallocated() const;

  /**
   * Get a user-friendly name for this ContentParent.  We make no guarantees
   * about this name: It might not be unique, apps can spoof special names,
   * etc.  So please don't use this name to make any decisions about the
   * ContentParent based on the value returned here.
   */
  void FriendlyName(nsAString& aName, bool aAnonymize = false);

  virtual void OnChannelError() override;

  virtual PCrashReporterParent*
  AllocPCrashReporterParent(const NativeThreadId& tid,
                            const uint32_t& processType) override;

  virtual bool
  RecvPCrashReporterConstructor(PCrashReporterParent* actor,
                                const NativeThreadId& tid,
                                const uint32_t& processType) override;

  virtual PNeckoParent* AllocPNeckoParent() override;

  virtual bool RecvPNeckoConstructor(PNeckoParent* aActor) override
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

  virtual PSendStreamParent* AllocPSendStreamParent() override;
  virtual bool DeallocPSendStreamParent(PSendStreamParent* aActor) override;

  virtual PScreenManagerParent*
  AllocPScreenManagerParent(uint32_t* aNumberOfScreens,
                            float* aSystemDefaultScale,
                            bool* aSuccess) override;

  virtual bool
  DeallocPScreenManagerParent(PScreenManagerParent* aActor) override;

  virtual PHalParent* AllocPHalParent() override;

  virtual bool RecvPHalConstructor(PHalParent* aActor) override
  {
    return PContentParent::RecvPHalConstructor(aActor);
  }

  virtual PHeapSnapshotTempFileHelperParent*
  AllocPHeapSnapshotTempFileHelperParent() override;

  virtual PStorageParent* AllocPStorageParent() override;

  virtual bool RecvPStorageConstructor(PStorageParent* aActor) override
  {
    return PContentParent::RecvPStorageConstructor(aActor);
  }

  virtual PJavaScriptParent*
  AllocPJavaScriptParent() override;

  virtual bool
  RecvPJavaScriptConstructor(PJavaScriptParent* aActor) override
  {
    return PContentParent::RecvPJavaScriptConstructor(aActor);
  }

  virtual PRemoteSpellcheckEngineParent* AllocPRemoteSpellcheckEngineParent() override;

  virtual bool RecvRecordingDeviceEvents(const nsString& aRecordingStatus,
                                         const nsString& aPageURL,
                                         const bool& aIsAudio,
                                         const bool& aIsVideo) override;

  bool CycleCollectWithLogs(bool aDumpAllTraces,
                            nsICycleCollectorLogSink* aSink,
                            nsIDumpGCAndCCLogsCallback* aCallback);

  virtual PBlobParent*
  SendPBlobConstructor(PBlobParent* aActor,
                       const BlobConstructorParams& aParams) override;

  virtual bool RecvAllocateTabId(const TabId& aOpenerTabId,
                                 const IPCTabContext& aContext,
                                 const ContentParentId& aCpId,
                                 TabId* aTabId) override;

  virtual bool RecvDeallocateTabId(const TabId& aTabId,
                                   const ContentParentId& aCpId,
                                   const bool& aMarkedDestroying) override;

  virtual bool RecvNotifyTabDestroying(const TabId& aTabId,
                                       const ContentParentId& aCpId) override;

  nsTArray<TabContext> GetManagedTabContext();

  virtual POfflineCacheUpdateParent*
  AllocPOfflineCacheUpdateParent(const URIParams& aManifestURI,
                                 const URIParams& aDocumentURI,
                                 const PrincipalInfo& aLoadingPrincipalInfo,
                                 const bool& aStickDocument) override;

  virtual bool
  RecvPOfflineCacheUpdateConstructor(POfflineCacheUpdateParent* aActor,
                                     const URIParams& aManifestURI,
                                     const URIParams& aDocumentURI,
                                     const PrincipalInfo& aLoadingPrincipal,
                                     const bool& stickDocument) override;

  virtual bool
  DeallocPOfflineCacheUpdateParent(POfflineCacheUpdateParent* aActor) override;

  virtual bool RecvSetOfflinePermission(const IPC::Principal& principal) override;

  virtual bool RecvFinishShutdown() override;

  void MaybeInvokeDragSession(TabParent* aParent);

  virtual PContentPermissionRequestParent*
  AllocPContentPermissionRequestParent(const InfallibleTArray<PermissionRequest>& aRequests,
                                       const IPC::Principal& aPrincipal,
                                       const TabId& aTabId) override;

  virtual bool
  DeallocPContentPermissionRequestParent(PContentPermissionRequestParent* actor) override;

  virtual bool HandleWindowsMessages(const Message& aMsg) const override;

  void ForkNewProcess(bool aBlocking);

  virtual bool RecvCreateWindow(PBrowserParent* aThisTabParent,
                                PBrowserParent* aOpener,
                                layout::PRenderFrameParent* aRenderFrame,
                                const uint32_t& aChromeFlags,
                                const bool& aCalledFromJS,
                                const bool& aPositionSpecified,
                                const bool& aSizeSpecified,
                                const nsCString& aFeatures,
                                const nsCString& aBaseURI,
                                const DocShellOriginAttributes& aOpenerOriginAttributes,
                                const float& aFullZoom,
                                nsresult* aResult,
                                bool* aWindowIsNew,
                                InfallibleTArray<FrameScriptInfo>* aFrameScripts,
                                nsCString* aURLToLoad,
                                layers::TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                uint64_t* aLayersId) override;

  static bool AllocateLayerTreeId(TabParent* aTabParent, uint64_t* aId);

  static void
  BroadcastBlobURLRegistration(const nsACString& aURI,
                               BlobImpl* aBlobImpl,
                               nsIPrincipal* aPrincipal,
                               ContentParent* aIgnoreThisCP = nullptr);

  static void
  BroadcastBlobURLUnregistration(const nsACString& aURI,
                                 ContentParent* aIgnoreThisCP = nullptr);

  virtual bool
  RecvStoreAndBroadcastBlobURLRegistration(const nsCString& aURI,
                                           PBlobParent* aBlobParent,
                                           const Principal& aPrincipal) override;

  virtual bool
  RecvUnstoreAndBroadcastBlobURLUnregistration(const nsCString& aURI) override;

  virtual int32_t Pid() const override;

  // Use the PHangMonitor channel to ask the child to repaint a tab.
  void ForceTabPaint(TabParent* aTabParent, uint64_t aLayerObserverEpoch);

protected:
  void OnChannelConnected(int32_t pid) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool ShouldContinueFromReplyTimeout() override;

  void OnVarChanged(const GfxVarUpdate& aVar) override;
  void OnCompositorUnexpectedShutdown() override;

private:
  static nsDataHashtable<nsStringHashKey, ContentParent*> *sAppContentParents;
  static nsTArray<ContentParent*>* sNonAppContentParents;
  static nsTArray<ContentParent*>* sPrivateContent;
  static StaticAutoPtr<LinkedList<ContentParent> > sContentParents;

  static void JoinProcessesIOThread(const nsTArray<ContentParent*>* aProcesses,
                                    Monitor* aMonitor, bool* aDone);

  // Take the preallocated process and transform it into a "real" app process,
  // for the specified manifest URL.  If there is no preallocated process (or
  // if it's dead), create a new one and set aTookPreAllocated to false.
  static already_AddRefed<ContentParent>
  GetNewOrPreallocatedAppProcess(mozIApplication* aApp,
                                 hal::ProcessPriority aInitialPriority,
                                 ContentParent* aOpener,
                                 /*out*/ bool* aTookPreAllocated = nullptr);

  static hal::ProcessPriority GetInitialProcessPriority(Element* aFrameElement);

  static ContentBridgeParent* CreateContentBridgeParent(const TabContext& aContext,
                                                        const hal::ProcessPriority& aPriority,
                                                        const TabId& aOpenerTabId,
                                                        /*out*/ TabId* aTabId);

  // Hide the raw constructor methods since we don't want client code
  // using them.
  virtual PBrowserParent* SendPBrowserConstructor(
      PBrowserParent* actor,
      const TabId& aTabId,
      const IPCTabContext& context,
      const uint32_t& chromeFlags,
      const ContentParentId& aCpId,
      const bool& aIsForApp,
      const bool& aIsForBrowser) override;
  using PContentParent::SendPTestShellConstructor;

  FORWARD_SHMEM_ALLOCATOR_TO(PContentParent)

  // No more than one of !!aApp, aIsForBrowser, and aIsForPreallocated may be
  // true.
  ContentParent(mozIApplication* aApp,
                ContentParent* aOpener,
                bool aIsForBrowser,
                bool aIsForPreallocated);

  // The common initialization for the constructors.
  void InitializeMembers();

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
  // called after the process has been transformed to app or browser.
  void ForwardKnownInfo();

  // Set the child process's priority and then check whether the child is
  // still alive.  Returns true if the process is still alive, and false
  // otherwise.  If you pass a FOREGROUND* priority here, it's (hopefully)
  // unlikely that the process will be killed after this point.
  bool SetPriorityAndCheckIsAlive(hal::ProcessPriority aPriority);

  // Transform a pre-allocated app process into a "real" app
  // process, for the specified manifest URL.
  void TransformPreallocatedIntoApp(ContentParent* aOpener,
                                    const nsAString& aAppManifestURL);

  // Transform a pre-allocated app process into a browser process. If this
  // returns false, the child process has died.
  void TransformPreallocatedIntoBrowser(ContentParent* aOpener);

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

  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);

  static bool AllocateLayerTreeId(ContentParent* aContent,
                                  TabParent* aTopLevel, const TabId& aTabId,
                                  uint64_t* aId);

  PGMPServiceParent*
  AllocPGMPServiceParent(mozilla::ipc::Transport* aTransport,
                         base::ProcessId aOtherProcess) override;

  PSharedBufferManagerParent*
  AllocPSharedBufferManagerParent(mozilla::ipc::Transport* aTranport,
                                   base::ProcessId aOtherProcess) override;

  PBackgroundParent*
  AllocPBackgroundParent(Transport* aTransport, ProcessId aOtherProcess)
                         override;

  PProcessHangMonitorParent*
  AllocPProcessHangMonitorParent(Transport* aTransport,
                                 ProcessId aOtherProcess) override;

  virtual bool RecvGetProcessAttributes(ContentParentId* aCpId,
                                        bool* aIsForApp,
                                        bool* aIsForBrowser) override;

  virtual bool
  RecvGetXPCOMProcessAttributes(bool* aIsOffline,
                                bool* aIsConnected,
                                bool* aIsLangRTL,
                                bool* aHaveBidiKeyboards,
                                InfallibleTArray<nsString>* dictionaries,
                                ClipboardCapabilities* clipboardCaps,
                                DomainPolicyClone* domainPolicy,
                                StructuredCloneData* initialData) override;

  virtual bool
  DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*) override;

  virtual bool
  DeallocPRemoteSpellcheckEngineParent(PRemoteSpellcheckEngineParent*) override;

  virtual PBrowserParent* AllocPBrowserParent(const TabId& aTabId,
                                              const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const ContentParentId& aCpId,
                                              const bool& aIsForApp,
                                              const bool& aIsForBrowser) override;

  virtual bool DeallocPBrowserParent(PBrowserParent* frame) override;

  virtual PDeviceStorageRequestParent*
  AllocPDeviceStorageRequestParent(const DeviceStorageParams&) override;

  virtual bool
  DeallocPDeviceStorageRequestParent(PDeviceStorageRequestParent*) override;

  virtual PBlobParent*
  AllocPBlobParent(const BlobConstructorParams& aParams) override;

  virtual bool DeallocPBlobParent(PBlobParent* aActor) override;

  virtual bool
  RecvPBlobConstructor(PBlobParent* aActor,
                       const BlobConstructorParams& params) override;

  virtual bool
  DeallocPCrashReporterParent(PCrashReporterParent* crashreporter) override;

  virtual bool RecvNSSU2FTokenIsCompatibleVersion(const nsString& aVersion,
                                                  bool* aIsCompatible) override;

  virtual bool RecvNSSU2FTokenIsRegistered(nsTArray<uint8_t>&& aKeyHandle,
                                           bool* aIsValidKeyHandle) override;

  virtual bool RecvNSSU2FTokenRegister(nsTArray<uint8_t>&& aApplication,
                                       nsTArray<uint8_t>&& aChallenge,
                                       nsTArray<uint8_t>* aRegistration) override;

  virtual bool RecvNSSU2FTokenSign(nsTArray<uint8_t>&& aApplication,
                                   nsTArray<uint8_t>&& aChallenge,
                                   nsTArray<uint8_t>&& aKeyHandle,
                                   nsTArray<uint8_t>* aSignature) override;

  virtual bool RecvIsSecureURI(const uint32_t& aType, const URIParams& aURI,
                               const uint32_t& aFlags, bool* aIsSecureURI) override;

  virtual bool RecvAccumulateMixedContentHSTS(const URIParams& aURI,
                                              const bool& aActive) override;

  virtual bool DeallocPHalParent(PHalParent*) override;

  virtual bool
  DeallocPHeapSnapshotTempFileHelperParent(PHeapSnapshotTempFileHelperParent*) override;

  virtual PIccParent* AllocPIccParent(const uint32_t& aServiceId) override;

  virtual bool DeallocPIccParent(PIccParent* aActor) override;

  virtual PMemoryReportRequestParent*
  AllocPMemoryReportRequestParent(const uint32_t& aGeneration,
                                  const bool &aAnonymize,
                                  const bool &aMinimizeMemoryUsage,
                                  const MaybeFileDesc &aDMDFile) override;

  virtual bool
  DeallocPMemoryReportRequestParent(PMemoryReportRequestParent* actor) override;

  virtual PCycleCollectWithLogsParent*
  AllocPCycleCollectWithLogsParent(const bool& aDumpAllTraces,
                                   const FileDescriptor& aGCLog,
                                   const FileDescriptor& aCCLog) override;

  virtual bool
  DeallocPCycleCollectWithLogsParent(PCycleCollectWithLogsParent* aActor) override;

  virtual PTestShellParent* AllocPTestShellParent() override;

  virtual bool DeallocPTestShellParent(PTestShellParent* shell) override;

  virtual PMobileConnectionParent* AllocPMobileConnectionParent(const uint32_t& aClientId) override;

  virtual bool DeallocPMobileConnectionParent(PMobileConnectionParent* aActor) override;

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
                                const OptionalURIParams& aReferrer,
                                PBrowserParent* aBrowser) override;

  virtual bool
  DeallocPExternalHelperAppParent(PExternalHelperAppParent* aService) override;

  virtual PHandlerServiceParent* AllocPHandlerServiceParent() override;

  virtual bool DeallocPHandlerServiceParent(PHandlerServiceParent*) override;

  virtual PCellBroadcastParent* AllocPCellBroadcastParent() override;

  virtual bool DeallocPCellBroadcastParent(PCellBroadcastParent*) override;

  virtual bool RecvPCellBroadcastConstructor(PCellBroadcastParent* aActor) override;

  virtual PSmsParent* AllocPSmsParent() override;

  virtual bool DeallocPSmsParent(PSmsParent*) override;

  virtual PTelephonyParent* AllocPTelephonyParent() override;

  virtual bool DeallocPTelephonyParent(PTelephonyParent*) override;

  virtual PVoicemailParent* AllocPVoicemailParent() override;

  virtual bool RecvPVoicemailConstructor(PVoicemailParent* aActor) override;

  virtual bool DeallocPVoicemailParent(PVoicemailParent* aActor) override;

  virtual PMediaParent* AllocPMediaParent() override;

  virtual bool DeallocPMediaParent(PMediaParent* aActor) override;

  virtual bool DeallocPStorageParent(PStorageParent* aActor) override;

  virtual PBluetoothParent* AllocPBluetoothParent() override;

  virtual bool DeallocPBluetoothParent(PBluetoothParent* aActor) override;

  virtual bool RecvPBluetoothConstructor(PBluetoothParent* aActor) override;

  virtual PFMRadioParent* AllocPFMRadioParent() override;

  virtual bool DeallocPFMRadioParent(PFMRadioParent* aActor) override;

  virtual PPresentationParent* AllocPPresentationParent() override;

  virtual bool DeallocPPresentationParent(PPresentationParent* aActor) override;

  virtual bool RecvPPresentationConstructor(PPresentationParent* aActor) override;

  virtual PFlyWebPublishedServerParent*
    AllocPFlyWebPublishedServerParent(const nsString& name,
                                      const FlyWebPublishOptions& params) override;

  virtual bool DeallocPFlyWebPublishedServerParent(PFlyWebPublishedServerParent* aActor) override;

  virtual PSpeechSynthesisParent* AllocPSpeechSynthesisParent() override;

  virtual bool
  DeallocPSpeechSynthesisParent(PSpeechSynthesisParent* aActor) override;

  virtual bool
  RecvPSpeechSynthesisConstructor(PSpeechSynthesisParent* aActor) override;

  virtual PWebBrowserPersistDocumentParent*
  AllocPWebBrowserPersistDocumentParent(PBrowserParent* aBrowser,
                                        const uint64_t& aOuterWindowID) override;

  virtual bool
  DeallocPWebBrowserPersistDocumentParent(PWebBrowserPersistDocumentParent* aActor) override;

  virtual bool RecvReadPrefsArray(InfallibleTArray<PrefSetting>* aPrefs) override;
  virtual bool RecvGetGfxVars(InfallibleTArray<GfxVarUpdate>* aVars) override;

  virtual bool RecvReadFontList(InfallibleTArray<FontListEntry>* retValue) override;

  virtual bool RecvReadDataStorageArray(const nsString& aFilename,
                                        InfallibleTArray<DataStorageItem>* aValues) override;

  virtual bool RecvReadPermissions(InfallibleTArray<IPC::Permission>* aPermissions) override;

  virtual bool RecvSetClipboard(const IPCDataTransfer& aDataTransfer,
                                const bool& aIsPrivateData,
                                const IPC::Principal& aRequestingPrincipal,
                                const int32_t& aWhichClipboard) override;

  virtual bool RecvGetClipboard(nsTArray<nsCString>&& aTypes,
                                const int32_t& aWhichClipboard,
                                IPCDataTransfer* aDataTransfer) override;

  virtual bool RecvEmptyClipboard(const int32_t& aWhichClipboard) override;

  virtual bool RecvClipboardHasType(nsTArray<nsCString>&& aTypes,
                                    const int32_t& aWhichClipboard,
                                    bool* aHasType) override;

  virtual bool RecvGetSystemColors(const uint32_t& colorsCount,
                                   InfallibleTArray<uint32_t>* colors) override;

  virtual bool RecvGetIconForExtension(const nsCString& aFileExt,
                                       const uint32_t& aIconSize,
                                       InfallibleTArray<uint8_t>* bits) override;

  virtual bool RecvGetShowPasswordSetting(bool* showPassword) override;

  virtual bool RecvStartVisitedQuery(const URIParams& uri) override;

  virtual bool RecvVisitURI(const URIParams& uri,
                            const OptionalURIParams& referrer,
                            const uint32_t& flags) override;

  virtual bool RecvSetURITitle(const URIParams& uri,
                               const nsString& title) override;

  bool HasNotificationPermission(const IPC::Principal& aPrincipal);

  virtual bool RecvShowAlert(const AlertNotificationType& aAlert) override;

  virtual bool RecvCloseAlert(const nsString& aName,
                              const IPC::Principal& aPrincipal) override;

  virtual bool RecvDisableNotifications(const IPC::Principal& aPrincipal) override;

  virtual bool RecvOpenNotificationSettings(const IPC::Principal& aPrincipal) override;

  virtual bool RecvLoadURIExternal(const URIParams& uri,
                                   PBrowserParent* windowContext) override;
  virtual bool RecvExtProtocolChannelConnectParent(const uint32_t& registrarId) override;

  virtual bool RecvSyncMessage(const nsString& aMsg,
                               const ClonedMessageData& aData,
                               InfallibleTArray<CpowEntry>&& aCpows,
                               const IPC::Principal& aPrincipal,
                               nsTArray<StructuredCloneData>* aRetvals) override;

  virtual bool RecvRpcMessage(const nsString& aMsg,
                              const ClonedMessageData& aData,
                              InfallibleTArray<CpowEntry>&& aCpows,
                              const IPC::Principal& aPrincipal,
                              nsTArray<StructuredCloneData>* aRetvals) override;

  virtual bool RecvAsyncMessage(const nsString& aMsg,
                                InfallibleTArray<CpowEntry>&& aCpows,
                                const IPC::Principal& aPrincipal,
                                const ClonedMessageData& aData) override;

  virtual bool RecvFilePathUpdateNotify(const nsString& aType,
                                        const nsString& aStorageName,
                                        const nsString& aFilePath,
                                        const nsCString& aReason) override;

  virtual bool RecvAddGeolocationListener(const IPC::Principal& aPrincipal,
                                          const bool& aHighAccuracy) override;
  virtual bool RecvRemoveGeolocationListener() override;

  virtual bool RecvSetGeolocationHigherAccuracy(const bool& aEnable) override;

  virtual bool RecvConsoleMessage(const nsString& aMessage) override;

  virtual bool RecvScriptError(const nsString& aMessage,
                               const nsString& aSourceName,
                               const nsString& aSourceLine,
                               const uint32_t& aLineNumber,
                               const uint32_t& aColNumber,
                               const uint32_t& aFlags,
                               const nsCString& aCategory) override;

  virtual bool RecvPrivateDocShellsExist(const bool& aExist) override;

  virtual bool RecvFirstIdle() override;

  virtual bool RecvAudioChannelChangeDefVolChannel(const int32_t& aChannel,
                                                   const bool& aHidden) override;

  virtual bool RecvAudioChannelServiceStatus(const bool& aTelephonyChannel,
                                             const bool& aContentOrNormalChannel,
                                             const bool& aAnyChannel) override;

  virtual bool RecvGetLookAndFeelCache(nsTArray<LookAndFeelInt>* aLookAndFeelIntCache) override;

  virtual bool RecvSpeakerManagerGetSpeakerStatus(bool* aValue) override;

  virtual bool RecvSpeakerManagerForceSpeaker(const bool& aEnable) override;

  virtual bool RecvCreateFakeVolume(const nsString& aFsName,
                                    const nsString& aMountPoint) override;

  virtual bool RecvSetFakeVolumeState(const nsString& aFsName,
                                      const int32_t& aFsState) override;

  virtual bool RecvRemoveFakeVolume(const nsString& fsName) override;

  virtual bool RecvKeywordToURI(const nsCString& aKeyword,
                                nsString* aProviderName,
                                OptionalInputStreamParams* aPostData,
                                OptionalURIParams* aURI) override;

  virtual bool RecvNotifyKeywordSearchLoading(const nsString &aProvider,
                                              const nsString &aKeyword) override;

  virtual bool RecvCopyFavicon(const URIParams& aOldURI,
                               const URIParams& aNewURI,
                               const IPC::Principal& aLoadingPrincipal,
                               const bool& aInPrivateBrowsing) override;

  virtual void ProcessingError(Result aCode, const char* aMsgName) override;

  virtual bool RecvAllocateLayerTreeId(const ContentParentId& aCpId,
                                       const TabId& aTabId,
                                       uint64_t* aId) override;

  virtual bool RecvDeallocateLayerTreeId(const uint64_t& aId) override;

  virtual bool RecvGetGraphicsFeatureStatus(const int32_t& aFeature,
                                            int32_t* aStatus,
                                            nsCString* aFailureId,
                                            bool* aSuccess) override;

  virtual bool RecvGraphicsError(const nsCString& aError) override;

  virtual bool
  RecvBeginDriverCrashGuard(const uint32_t& aGuardType,
                            bool* aOutCrashed) override;

  virtual bool RecvEndDriverCrashGuard(const uint32_t& aGuardType) override;

  virtual bool RecvAddIdleObserver(const uint64_t& observerId,
                                   const uint32_t& aIdleTimeInS) override;

  virtual bool RecvRemoveIdleObserver(const uint64_t& observerId,
                                      const uint32_t& aIdleTimeInS) override;

  virtual bool
  RecvBackUpXResources(const FileDescriptor& aXSocketFd) override;

  virtual bool
  RecvOpenAnonymousTemporaryFile(FileDescOrError* aFD) override;

  virtual bool
  RecvKeygenProcessValue(const nsString& oldValue, const nsString& challenge,
                         const nsString& keytype, const nsString& keyparams,
                         nsString* newValue) override;

  virtual bool
  RecvKeygenProvideContent(nsString* aAttribute,
                           nsTArray<nsString>* aContent) override;

  virtual PFileDescriptorSetParent*
  AllocPFileDescriptorSetParent(const mozilla::ipc::FileDescriptor&) override;

  virtual bool
  DeallocPFileDescriptorSetParent(PFileDescriptorSetParent*) override;

  virtual PWebrtcGlobalParent* AllocPWebrtcGlobalParent() override;
  virtual bool DeallocPWebrtcGlobalParent(PWebrtcGlobalParent *aActor) override;


  virtual bool RecvUpdateDropEffect(const uint32_t& aDragAction,
                                    const uint32_t& aDropEffect) override;

  virtual bool RecvProfile(const nsCString& aProfile) override;

  virtual bool RecvGetGraphicsDeviceInitData(ContentDeviceData* aOut) override;

  void StartProfiler(nsIProfilerStartParams* aParams);

  virtual bool RecvGetDeviceStorageLocation(const nsString& aType,
                                            nsString* aPath) override;

  virtual bool RecvGetDeviceStorageLocations(DeviceStorageLocationInfo* info) override;

  virtual bool RecvGetAndroidSystemInfo(AndroidSystemInfo* aInfo) override;

  virtual bool RecvNotifyBenchmarkResult(const nsString& aCodecName,
                                         const uint32_t& aDecodeFPS) override;

  virtual bool RecvNotifyPushObservers(const nsCString& aScope,
                                       const IPC::Principal& aPrincipal,
                                       const nsString& aMessageId) override;

  virtual bool RecvNotifyPushObserversWithData(const nsCString& aScope,
                                               const IPC::Principal& aPrincipal,
                                               const nsString& aMessageId,
                                               InfallibleTArray<uint8_t>&& aData) override;

  virtual bool RecvNotifyPushSubscriptionChangeObservers(const nsCString& aScope,
                                                         const IPC::Principal& aPrincipal) override;

  virtual bool RecvNotifyPushSubscriptionModifiedObservers(const nsCString& aScope,
                                                           const IPC::Principal& aPrincipal) override;

  virtual bool RecvNotifyLowMemory() override;

  virtual bool RecvGetFilesRequest(const nsID& aID,
                                   const nsString& aDirectoryPath,
                                   const bool& aRecursiveFlag) override;

  virtual bool RecvDeleteGetFilesRequest(const nsID& aID) override;

  virtual bool RecvAccumulateChildHistogram(
                  InfallibleTArray<Accumulation>&& aAccumulations) override;
  virtual bool RecvAccumulateChildKeyedHistogram(
                  InfallibleTArray<KeyedAccumulation>&& aAccumulations) override;
public:
  void SendGetFilesResponseAndForget(const nsID& aID,
                                     const GetFilesResponseResult& aResult);

private:

  // If you add strong pointers to cycle collected objects here, be sure to
  // release these objects in ShutDownProcess.  See the comment there for more
  // details.

  GeckoChildProcessHost* mSubprocess;
  ContentParent* mOpener;

  ContentParentId mChildID;
  int32_t mGeolocationWatchID;

  nsString mAppManifestURL;

  nsCString mKillHardAnnotation;

  /**
   * We cache mAppName instead of looking it up using mAppManifestURL when we
   * need it because it turns out that getting an app from the apps service is
   * expensive.
   */
  nsString mAppName;

  // After we initiate shutdown, we also start a timer to ensure
  // that even content processes that are 100% blocked (say from
  // SIGSTOP), are still killed eventually.  This task enforces that
  // timer.
  nsCOMPtr<nsITimer> mForceKillTimer;
  // How many tabs we're waiting to finish their destruction
  // sequence.  Precisely, how many TabParents have called
  // NotifyTabDestroying() but not called NotifyTabDestroyed().
  int32_t mNumDestroyingTabs;
  // True only while this is ready to be used to host remote tabs.
  // This must not be used for new purposes after mIsAlive goes to
  // false, but some previously scheduled IPC traffic may still pass
  // through.
  bool mIsAlive;

  // True only the if process is already a browser or app or has
  // been transformed into one.
  bool mMetamorphosed;

  bool mSendPermissionUpdates;
  bool mIsForBrowser;

  // These variables track whether we've called Close() and KillHard() on our
  // channel.
  bool mCalledClose;
  bool mCalledKillHard;
  bool mCreatedPairedMinidumps;
  bool mShutdownPending;
  bool mIPCOpen;

  friend class CrashReporterParent;

  RefPtr<nsConsoleService>  mConsoleService;
  nsConsoleService* GetConsoleService();

  nsTArray<nsCOMPtr<nsIObserver>> mIdleListeners;

#ifdef MOZ_X11
  // Dup of child's X socket, used to scope its resources to this
  // object instead of the child process's lifetime.
  ScopedClose mChildXSocketFdDup;
#endif

  PProcessHangMonitorParent* mHangMonitorActor;

#ifdef MOZ_ENABLE_PROFILER_SPS
  RefPtr<mozilla::ProfileGatherer> mGatherer;
#endif
  nsCString mProfile;

  UniquePtr<gfx::DriverCrashGuard> mDriverCrashGuard;

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

  nsTArray<nsCString> mBlobURLs;
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
