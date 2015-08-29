/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentParent_h
#define mozilla_dom_ContentParent_h

#include "mozilla/dom/NuwaParent.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/Attributes.h"
#include "mozilla/FileUtils.h"
#include "mozilla/HalTypes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/StaticPtr.h"

#include "nsDataHashtable.h"
#include "nsFrameMessageManager.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsIThreadInternal.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDOMGeoPositionErrorCallback.h"
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
class PCompositorParent;
class PSharedBufferManagerParent;
} // namespace layers

namespace dom {

class Element;
class TabParent;
class PStorageParent;
class ClonedMessageData;
class MemoryReport;
class TabContext;
class ContentBridgeParent;

class ContentParent final : public PContentParent
                          , public nsIContentParent
                          , public nsIObserver
                          , public nsIDOMGeoPositionCallback
                          , public nsIDOMGeoPositionErrorCallback
                          , public mozilla::LinkedListElement<ContentParent>
{
    typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
    typedef mozilla::ipc::PFileDescriptorSetParent PFileDescriptorSetParent;
    typedef mozilla::ipc::TestShellParent TestShellParent;
    typedef mozilla::ipc::URIParams URIParams;
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::OwningSerializedStructuredCloneBuffer OwningSerializedStructuredCloneBuffer;

public:
#ifdef MOZ_NUWA_PROCESS
    static int32_t NuwaPid() {
        return sNuwaPid;
    }

    static bool IsNuwaReady() {
        return sNuwaReady;
    }
#endif
    virtual bool IsContentParent() override { return true; }
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

    static already_AddRefed<ContentParent> RunNuwaProcess();

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
    bool IsDestroyed() { return !mIPCOpen; }

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

    virtual bool RecvLoadPlugin(const uint32_t& aPluginId, nsresult* aRv, uint32_t* aRunID) override;
    virtual bool RecvConnectPluginBridge(const uint32_t& aPluginId, nsresult* aRv) override;
    virtual bool RecvGetBlocklistState(const uint32_t& aPluginId, uint32_t* aIsBlocklisted) override;
    virtual bool RecvFindPlugins(const uint32_t& aPluginEpoch,
                                 nsTArray<PluginTag>* aPlugins,
                                 uint32_t* aNewPluginEpoch) override;

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
    virtual bool DoSendAsyncMessage(JSContext* aCx,
                                    const nsAString& aMessage,
                                    const mozilla::dom::StructuredCloneData& aData,
                                    JS::Handle<JSObject *> aCpows,
                                    nsIPrincipal* aPrincipal) override;
    virtual bool CheckPermission(const nsAString& aPermission) override;
    virtual bool CheckManifestURL(const nsAString& aManifestURL) override;
    virtual bool CheckAppHasPermission(const nsAString& aPermission) override;
    virtual bool CheckAppHasStatus(unsigned short aStatus) override;
    virtual bool KillChild() override;

    /** Notify that a tab is beginning its destruction sequence. */
    void NotifyTabDestroying(PBrowserParent* aTab);
    /** Notify that a tab was destroyed during normal operation. */
    void NotifyTabDestroyed(PBrowserParent* aTab,
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
    DeallocateTabId(const TabId& aTabId, const ContentParentId& aCpId);

    static bool
    GetBrowserConfiguration(const nsCString& aURI, BrowserConfiguration& aConfig);

    void ReportChildAlreadyBlocked();
    bool RequestRunToCompletion();

    bool IsAlive();
    virtual bool IsForApp() override;
    virtual bool IsForBrowser() override
    {
      return mIsForBrowser;
    }
#ifdef MOZ_NUWA_PROCESS
    bool IsNuwaProcess();
#endif

    GeckoChildProcessHost* Process() {
        return mSubprocess;
    }

    int32_t Pid();

    ContentParent* Opener() {
        return mOpener;
    }

    bool NeedsPermissionsUpdate() const {
        return mSendPermissionUpdates;
    }

    bool NeedsDataStoreInfos() const {
        return mSendDataStoreInfos;
    }

    /**
     * Kill our subprocess and make sure it dies.  Should only be used
     * in emergency situations since it bypasses the normal shutdown
     * process.
     */
    void KillHard(const char* aWhy);

    /**
     * API for adding a crash reporter annotation that provides a reason
     * for a listener request to abort the child.
     */
    bool IsKillHardAnnotationSet() { return mKillHardAnnotation.IsEmpty(); }
    const nsCString& GetKillHardAnnotation() { return mKillHardAnnotation; }
    void SetKillHardAnnotation(const nsACString& aReason) {
      mKillHardAnnotation = aReason;
    }

    ContentParentId ChildID() override { return mChildID; }
    const nsString& AppManifestURL() const { return mAppManifestURL; }

    bool IsPreallocated();

    /**
     * Get a user-friendly name for this ContentParent.  We make no guarantees
     * about this name: It might not be unique, apps can spoof special names,
     * etc.  So please don't use this name to make any decisions about the
     * ContentParent based on the value returned here.
     */
    void FriendlyName(nsAString& aName, bool aAnonymize = false);

    virtual void OnChannelError() override;

    virtual void OnBeginSyncTransaction() override;

    virtual PCrashReporterParent*
    AllocPCrashReporterParent(const NativeThreadId& tid,
                              const uint32_t& processType) override;
    virtual bool
    RecvPCrashReporterConstructor(PCrashReporterParent* actor,
                                  const NativeThreadId& tid,
                                  const uint32_t& processType) override;

    virtual PNeckoParent* AllocPNeckoParent() override;
    virtual bool RecvPNeckoConstructor(PNeckoParent* aActor) override {
        return PContentParent::RecvPNeckoConstructor(aActor);
    }

    virtual PPrintingParent* AllocPPrintingParent() override;
    virtual bool RecvPPrintingConstructor(PPrintingParent* aActor) override;
    virtual bool DeallocPPrintingParent(PPrintingParent* aActor) override;

    virtual PScreenManagerParent*
    AllocPScreenManagerParent(uint32_t* aNumberOfScreens,
                              float* aSystemDefaultScale,
                              bool* aSuccess) override;
    virtual bool DeallocPScreenManagerParent(PScreenManagerParent* aActor) override;

    virtual PHalParent* AllocPHalParent() override;
    virtual bool RecvPHalConstructor(PHalParent* aActor) override {
        return PContentParent::RecvPHalConstructor(aActor);
    }

    virtual PStorageParent* AllocPStorageParent() override;
    virtual bool RecvPStorageConstructor(PStorageParent* aActor) override {
        return PContentParent::RecvPStorageConstructor(aActor);
    }

    virtual PJavaScriptParent*
    AllocPJavaScriptParent() override;
    virtual bool
    RecvPJavaScriptConstructor(PJavaScriptParent* aActor) override {
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

    virtual PBlobParent* SendPBlobConstructor(
        PBlobParent* aActor,
        const BlobConstructorParams& aParams) override;

    virtual bool RecvAllocateTabId(const TabId& aOpenerTabId,
                                   const IPCTabContext& aContext,
                                   const ContentParentId& aCpId,
                                   TabId* aTabId) override;

    virtual bool RecvDeallocateTabId(const TabId& aTabId) override;

    nsTArray<TabContext> GetManagedTabContext();

    virtual POfflineCacheUpdateParent*
    AllocPOfflineCacheUpdateParent(const URIParams& aManifestURI,
                                   const URIParams& aDocumentURI,
                                   const bool& aStickDocument,
                                   const TabId& aTabId) override;
    virtual bool
    RecvPOfflineCacheUpdateConstructor(POfflineCacheUpdateParent* aActor,
                                       const URIParams& aManifestURI,
                                       const URIParams& aDocumentURI,
                                       const bool& stickDocument,
                                       const TabId& aTabId) override;
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

    bool HasGamepadListener() const { return mHasGamepadListener; }

    void SetNuwaParent(NuwaParent* aNuwaParent) { mNuwaParent = aNuwaParent; }
    void ForkNewProcess(bool aBlocking);

protected:
    void OnChannelConnected(int32_t pid) override;
    virtual void ActorDestroy(ActorDestroyReason why) override;
    void OnNuwaForkTimeout();

    bool ShouldContinueFromReplyTimeout() override;

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

    // No more than one of !!aApp, aIsForBrowser, and aIsForPreallocated may be
    // true.
    ContentParent(mozIApplication* aApp,
                  ContentParent* aOpener,
                  bool aIsForBrowser,
                  bool aIsForPreallocated,
                  bool aIsNuwaProcess = false);

#ifdef MOZ_NUWA_PROCESS
    ContentParent(ContentParent* aTemplate,
                  const nsAString& aAppManifestURL,
                  base::ProcessHandle aPid,
                  InfallibleTArray<ProtocolFdMapping>&& aFds);
#endif

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

    // If the frame element indicates that the child process is "critical" and
    // has a pending system message, this function acquires the CPU wake lock on
    // behalf of the child.  We'll release the lock when the system message is
    // handled or after a timeout, whichever comes first.
    void MaybeTakeCPUWakeLock(Element* aFrameElement);

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
    enum ShutDownMethod {
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

    PGMPServiceParent*
    AllocPGMPServiceParent(mozilla::ipc::Transport* aTransport,
                           base::ProcessId aOtherProcess) override;
    PCompositorParent*
    AllocPCompositorParent(mozilla::ipc::Transport* aTransport,
                           base::ProcessId aOtherProcess) override;
    PImageBridgeParent*
    AllocPImageBridgeParent(mozilla::ipc::Transport* aTransport,
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
    virtual bool RecvGetXPCOMProcessAttributes(bool* aIsOffline,
                                               bool* aIsConnected,
                                               bool* aIsLangRTL,
                                               InfallibleTArray<nsString>* dictionaries,
                                               ClipboardCapabilities* clipboardCaps,
                                               DomainPolicyClone* domainPolicy,
                                               OwningSerializedStructuredCloneBuffer* initialData) override;

    virtual bool DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*) override;

    virtual bool DeallocPRemoteSpellcheckEngineParent(PRemoteSpellcheckEngineParent*) override;
    virtual PBrowserParent* AllocPBrowserParent(const TabId& aTabId,
                                                const IPCTabContext& aContext,
                                                const uint32_t& aChromeFlags,
                                                const ContentParentId& aCpId,
                                                const bool& aIsForApp,
                                                const bool& aIsForBrowser) override;
    virtual bool DeallocPBrowserParent(PBrowserParent* frame) override;

    virtual PDeviceStorageRequestParent*
    AllocPDeviceStorageRequestParent(const DeviceStorageParams&) override;
    virtual bool DeallocPDeviceStorageRequestParent(PDeviceStorageRequestParent*) override;

    virtual PFileSystemRequestParent*
    AllocPFileSystemRequestParent(const FileSystemParams&) override;

    virtual bool
    DeallocPFileSystemRequestParent(PFileSystemRequestParent*) override;

    virtual PBlobParent* AllocPBlobParent(const BlobConstructorParams& aParams)
                                          override;
    virtual bool DeallocPBlobParent(PBlobParent* aActor) override;

    virtual bool DeallocPCrashReporterParent(PCrashReporterParent* crashreporter) override;

    virtual bool RecvGetRandomValues(const uint32_t& length,
                                     InfallibleTArray<uint8_t>* randomValues) override;

    virtual bool RecvIsSecureURI(const uint32_t& aType, const URIParams& aURI,
                                 const uint32_t& aFlags, bool* aIsSecureURI) override;

    virtual bool DeallocPHalParent(PHalParent*) override;

    virtual PIccParent* AllocPIccParent(const uint32_t& aServiceId) override;
    virtual bool DeallocPIccParent(PIccParent* aActor) override;

    virtual PMemoryReportRequestParent*
    AllocPMemoryReportRequestParent(const uint32_t& aGeneration,
                                    const bool &aAnonymize,
                                    const bool &aMinimizeMemoryUsage,
                                    const MaybeFileDesc &aDMDFile) override;
    virtual bool DeallocPMemoryReportRequestParent(PMemoryReportRequestParent* actor) override;

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

    virtual PPSMContentDownloaderParent* AllocPPSMContentDownloaderParent(
            const uint32_t& aCertType) override;
    virtual bool DeallocPPSMContentDownloaderParent(PPSMContentDownloaderParent* aDownloader) override;

    virtual PExternalHelperAppParent* AllocPExternalHelperAppParent(
            const OptionalURIParams& aUri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const uint32_t& aContentDispositionHint,
            const nsString& aContentDispositionFilename,
            const bool& aForceSave,
            const int64_t& aContentLength,
            const OptionalURIParams& aReferrer,
            PBrowserParent* aBrowser) override;
    virtual bool DeallocPExternalHelperAppParent(PExternalHelperAppParent* aService) override;

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

    virtual PSpeechSynthesisParent* AllocPSpeechSynthesisParent() override;
    virtual bool DeallocPSpeechSynthesisParent(PSpeechSynthesisParent* aActor) override;
    virtual bool RecvPSpeechSynthesisConstructor(PSpeechSynthesisParent* aActor) override;

    virtual bool RecvReadPrefsArray(InfallibleTArray<PrefSetting>* aPrefs) override;
    virtual bool RecvReadFontList(InfallibleTArray<FontListEntry>* retValue) override;

    virtual bool RecvReadPermissions(InfallibleTArray<IPC::Permission>* aPermissions) override;

    virtual bool RecvSetClipboard(const IPCDataTransfer& aDataTransfer,
                                  const bool& aIsPrivateData,
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

    virtual bool RecvShowAlertNotification(const nsString& aImageUrl, const nsString& aTitle,
                                           const nsString& aText, const bool& aTextClickable,
                                           const nsString& aCookie, const nsString& aName,
                                           const nsString& aBidi, const nsString& aLang,
                                           const nsString& aData,
                                           const IPC::Principal& aPrincipal,
                                           const bool& aInPrivateBrowsing) override;

    virtual bool RecvCloseAlert(const nsString& aName,
                                const IPC::Principal& aPrincipal) override;

    virtual bool RecvLoadURIExternal(const URIParams& uri) override;

    virtual bool RecvSyncMessage(const nsString& aMsg,
                                 const ClonedMessageData& aData,
                                 InfallibleTArray<CpowEntry>&& aCpows,
                                 const IPC::Principal& aPrincipal,
                                 nsTArray<OwningSerializedStructuredCloneBuffer>* aRetvals) override;
    virtual bool RecvRpcMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                InfallibleTArray<CpowEntry>&& aCpows,
                                const IPC::Principal& aPrincipal,
                                nsTArray<OwningSerializedStructuredCloneBuffer>* aRetvals) override;
    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal) override;

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

    virtual bool RecvGetSystemMemory(const uint64_t& getterId) override;

    virtual bool RecvGetLookAndFeelCache(nsTArray<LookAndFeelInt>* aLookAndFeelIntCache) override;

    virtual bool RecvDataStoreGetStores(
                       const nsString& aName,
                       const nsString& aOwner,
                       const IPC::Principal& aPrincipal,
                       InfallibleTArray<DataStoreSetting>* aValue) override;

    virtual bool RecvSpeakerManagerGetSpeakerStatus(bool* aValue) override;

    virtual bool RecvSpeakerManagerForceSpeaker(const bool& aEnable) override;

    virtual bool RecvSystemMessageHandled() override;

    // Callbacks from NuwaParent.
    void OnNuwaReady();
    void OnNewProcessCreated(uint32_t aPid,
                             UniquePtr<nsTArray<ProtocolFdMapping>>&& aFds);

    virtual bool RecvCreateFakeVolume(const nsString& fsName, const nsString& mountPoint) override;

    virtual bool RecvSetFakeVolumeState(const nsString& fsName, const int32_t& fsState) override;

    virtual bool RecvRemoveFakeVolume(const nsString& fsName) override;

    virtual bool RecvKeywordToURI(const nsCString& aKeyword,
                                  nsString* aProviderName,
                                  OptionalInputStreamParams* aPostData,
                                  OptionalURIParams* aURI) override;

    virtual bool RecvNotifyKeywordSearchLoading(const nsString &aProvider,
                                                const nsString &aKeyword) override;

    virtual bool RecvCopyFavicon(const URIParams& aOldURI,
                                 const URIParams& aNewURI,
                                 const bool& aInPrivateBrowsing) override;

    virtual void ProcessingError(Result aCode, const char* aMsgName) override;

    virtual bool RecvAllocateLayerTreeId(uint64_t* aId) override;
    virtual bool RecvDeallocateLayerTreeId(const uint64_t& aId) override;

    virtual bool RecvGetGraphicsFeatureStatus(const int32_t& aFeature,
                                              int32_t* aStatus,
                                              bool* aSuccess) override;
    virtual bool RecvBeginDriverCrashGuard(const uint32_t& aGuardType, bool* aOutCrashed) override;
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

    virtual bool
    RecvGetFileReferences(const PersistenceType& aPersistenceType,
                          const nsCString& aOrigin,
                          const nsString& aDatabaseName,
                          const int64_t& aFileId,
                          int32_t* aRefCnt,
                          int32_t* aDBRefCnt,
                          int32_t* aSliceRefCnt,
                          bool* aResult) override;

    virtual bool
    RecvFlushPendingFileDeletions() override;

    virtual PWebrtcGlobalParent* AllocPWebrtcGlobalParent() override;
    virtual bool DeallocPWebrtcGlobalParent(PWebrtcGlobalParent *aActor) override;


    virtual bool RecvUpdateDropEffect(const uint32_t& aDragAction,
                                      const uint32_t& aDropEffect) override;

    virtual bool RecvGetBrowserConfiguration(const nsCString& aURI, BrowserConfiguration* aConfig) override;

    virtual bool RecvGamepadListenerAdded() override;
    virtual bool RecvGamepadListenerRemoved() override;
    virtual bool RecvProfile(const nsCString& aProfile) override;
    virtual bool RecvGetGraphicsDeviceInitData(DeviceInitData* aOut) override;

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
    bool mSendDataStoreInfos;
    bool mIsForBrowser;
    bool mIsNuwaProcess;
    bool mHasGamepadListener;

    // These variables track whether we've called Close(), CloseWithError()
    // and KillHard() on our channel.
    bool mCalledClose;
    bool mCalledCloseWithError;
    bool mCalledKillHard;
    bool mCreatedPairedMinidumps;
    bool mShutdownPending;
    bool mIPCOpen;

    friend class CrashReporterParent;

    // Allows NuwaParent to access OnNuwaReady() and OnNewProcessCreated().
    friend class NuwaParent;

    nsRefPtr<nsConsoleService>  mConsoleService;
    nsConsoleService* GetConsoleService();

    nsTArray<nsCOMPtr<nsIObserver>> mIdleListeners;

#ifdef MOZ_X11
    // Dup of child's X socket, used to scope its resources to this
    // object instead of the child process's lifetime.
    ScopedClose mChildXSocketFdDup;
#endif

#ifdef MOZ_NUWA_PROCESS
    static int32_t sNuwaPid;
    static bool sNuwaReady;
#endif

    PProcessHangMonitorParent* mHangMonitorActor;

    // NuwaParent and ContentParent hold strong references to each other. The
    // cycle will be broken when either actor is destroyed.
    nsRefPtr<NuwaParent> mNuwaParent;

#ifdef MOZ_ENABLE_PROFILER_SPS
    nsRefPtr<mozilla::ProfileGatherer> mGatherer;
#endif
    nsCString mProfile;

    UniquePtr<gfx::DriverCrashGuard> mDriverCrashGuard;
};

} // namespace dom
} // namespace mozilla

class ParentIdleListener : public nsIObserver {
  friend class mozilla::dom::ContentParent;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ParentIdleListener(mozilla::dom::ContentParent* aParent, uint64_t aObserver, uint32_t aTime)
    : mParent(aParent), mObserver(aObserver), mTime(aTime)
  {}
private:
  virtual ~ParentIdleListener() {}
  nsRefPtr<mozilla::dom::ContentParent> mParent;
  uint64_t mObserver;
  uint32_t mTime;
};

#endif
