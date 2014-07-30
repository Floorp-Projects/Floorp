/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentParent_h
#define mozilla_dom_ContentParent_h

#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/dom/ipc/Blob.h"
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
#include "PermissionMessageUtils.h"

#define CHILD_PROCESS_SHUTDOWN_MESSAGE NS_LITERAL_STRING("child-process-shutdown")

class mozIApplication;
class nsConsoleService;
class nsICycleCollectorLogSink;
class nsIDOMBlob;
class nsIDumpGCAndCCLogsCallback;
class nsIMemoryReporter;
class ParentIdleListener;

namespace mozilla {
class PRemoteSpellcheckEngineParent;

namespace ipc {
class OptionalURIParams;
class URIParams;
class TestShellParent;
} // namespace ipc

namespace jsipc {
class JavaScriptParent;
class PJavaScriptParent;
}

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
class PFileDescriptorSetParent;

class ContentParent : public PContentParent
                    , public nsIContentParent
                    , public nsIObserver
                    , public nsIDOMGeoPositionCallback
                    , public mozilla::LinkedListElement<ContentParent>
{
    typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
    typedef mozilla::ipc::TestShellParent TestShellParent;
    typedef mozilla::ipc::URIParams URIParams;
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;

public:
    virtual bool IsContentParent() MOZ_OVERRIDE { return true; }
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
    static void RunAfterPreallocatedProcessReady(nsIRunnable* aRequest);

    static already_AddRefed<ContentParent>
    GetNewOrUsed(bool aForBrowserElement = false,
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

    virtual bool RecvCreateChildProcess(const IPCTabContext& aContext,
                                        const hal::ProcessPriority& aPriority,
                                        uint64_t* aId,
                                        bool* aIsForApp,
                                        bool* aIsForBrowser) MOZ_OVERRIDE;
    virtual bool AnswerBridgeToChildProcess(const uint64_t& id) MOZ_OVERRIDE;

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ContentParent, nsIObserver)

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIDOMGEOPOSITIONCALLBACK

    /**
     * MessageManagerCallback methods that we override.
     */
    virtual bool DoSendAsyncMessage(JSContext* aCx,
                                    const nsAString& aMessage,
                                    const mozilla::dom::StructuredCloneData& aData,
                                    JS::Handle<JSObject *> aCpows,
                                    nsIPrincipal* aPrincipal) MOZ_OVERRIDE;
    virtual bool CheckPermission(const nsAString& aPermission) MOZ_OVERRIDE;
    virtual bool CheckManifestURL(const nsAString& aManifestURL) MOZ_OVERRIDE;
    virtual bool CheckAppHasPermission(const nsAString& aPermission) MOZ_OVERRIDE;
    virtual bool CheckAppHasStatus(unsigned short aStatus) MOZ_OVERRIDE;

    /** Notify that a tab is beginning its destruction sequence. */
    void NotifyTabDestroying(PBrowserParent* aTab);
    /** Notify that a tab was destroyed during normal operation. */
    void NotifyTabDestroyed(PBrowserParent* aTab,
                            bool aNotifiedDestroying);

    TestShellParent* CreateTestShell();
    bool DestroyTestShell(TestShellParent* aTestShell);
    TestShellParent* GetTestShellSingleton();
    jsipc::JavaScriptParent *GetCPOWManager();

    void ReportChildAlreadyBlocked();
    bool RequestRunToCompletion();

    bool IsAlive();
    virtual bool IsForApp() MOZ_OVERRIDE;
    virtual bool IsForBrowser() MOZ_OVERRIDE
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
    void KillHard();

    uint64_t ChildID() MOZ_OVERRIDE { return mChildID; }
    const nsString& AppManifestURL() const { return mAppManifestURL; }

    bool IsPreallocated();

    /**
     * Get a user-friendly name for this ContentParent.  We make no guarantees
     * about this name: It might not be unique, apps can spoof special names,
     * etc.  So please don't use this name to make any decisions about the
     * ContentParent based on the value returned here.
     */
    void FriendlyName(nsAString& aName, bool aAnonymize = false);

    virtual void OnChannelError() MOZ_OVERRIDE;

    virtual PIndexedDBParent* AllocPIndexedDBParent() MOZ_OVERRIDE;
    virtual bool
    RecvPIndexedDBConstructor(PIndexedDBParent* aActor) MOZ_OVERRIDE;

    virtual PCrashReporterParent*
    AllocPCrashReporterParent(const NativeThreadId& tid,
                              const uint32_t& processType) MOZ_OVERRIDE;
    virtual bool
    RecvPCrashReporterConstructor(PCrashReporterParent* actor,
                                  const NativeThreadId& tid,
                                  const uint32_t& processType) MOZ_OVERRIDE;

    virtual PNeckoParent* AllocPNeckoParent() MOZ_OVERRIDE;
    virtual bool RecvPNeckoConstructor(PNeckoParent* aActor) MOZ_OVERRIDE {
        return PContentParent::RecvPNeckoConstructor(aActor);
    }

    virtual PScreenManagerParent*
    AllocPScreenManagerParent(uint32_t* aNumberOfScreens,
                              float* aSystemDefaultScale,
                              bool* aSuccess) MOZ_OVERRIDE;
    virtual bool DeallocPScreenManagerParent(PScreenManagerParent* aActor) MOZ_OVERRIDE;

    virtual PHalParent* AllocPHalParent() MOZ_OVERRIDE;
    virtual bool RecvPHalConstructor(PHalParent* aActor) MOZ_OVERRIDE {
        return PContentParent::RecvPHalConstructor(aActor);
    }

    virtual PStorageParent* AllocPStorageParent() MOZ_OVERRIDE;
    virtual bool RecvPStorageConstructor(PStorageParent* aActor) MOZ_OVERRIDE {
        return PContentParent::RecvPStorageConstructor(aActor);
    }

    virtual PJavaScriptParent*
    AllocPJavaScriptParent() MOZ_OVERRIDE;
    virtual bool
    RecvPJavaScriptConstructor(PJavaScriptParent* aActor) MOZ_OVERRIDE {
        return PContentParent::RecvPJavaScriptConstructor(aActor);
    }
    virtual PRemoteSpellcheckEngineParent* AllocPRemoteSpellcheckEngineParent() MOZ_OVERRIDE;

    virtual bool RecvRecordingDeviceEvents(const nsString& aRecordingStatus,
                                           const nsString& aPageURL,
                                           const bool& aIsAudio,
                                           const bool& aIsVideo) MOZ_OVERRIDE;

    bool CycleCollectWithLogs(bool aDumpAllTraces,
                              nsICycleCollectorLogSink* aSink,
                              nsIDumpGCAndCCLogsCallback* aCallback);

    virtual PBlobParent* SendPBlobConstructor(
        PBlobParent* aActor,
        const BlobConstructorParams& aParams) MOZ_OVERRIDE;

protected:
    void OnChannelConnected(int32_t pid) MOZ_OVERRIDE;
    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
    void OnNuwaForkTimeout();

    bool ShouldContinueFromReplyTimeout() MOZ_OVERRIDE;

private:
    static nsDataHashtable<nsStringHashKey, ContentParent*> *sAppContentParents;
    static nsTArray<ContentParent*>* sNonAppContentParents;
    static nsTArray<ContentParent*>* sPrivateContent;
    static StaticAutoPtr<LinkedList<ContentParent> > sContentParents;

    static void JoinProcessesIOThread(const nsTArray<ContentParent*>* aProcesses,
                                      Monitor* aMonitor, bool* aDone);

    // Take the preallocated process and transform it into a "real" app process,
    // for the specified manifest URL.  If there is no preallocated process (or
    // if it's dead), this returns false.
    static already_AddRefed<ContentParent>
    MaybeTakePreallocatedAppProcess(const nsAString& aAppManifestURL,
                                    hal::ProcessPriority aInitialPriority);

    static hal::ProcessPriority GetInitialProcessPriority(Element* aFrameElement);

    // Hide the raw constructor methods since we don't want client code
    // using them.
    virtual PBrowserParent* SendPBrowserConstructor(
        PBrowserParent* actor,
        const IPCTabContext& context,
        const uint32_t& chromeFlags,
        const uint64_t& aId,
        const bool& aIsForApp,
        const bool& aIsForBrowser) MOZ_OVERRIDE;
    using PContentParent::SendPTestShellConstructor;

    // No more than one of !!aApp, aIsForBrowser, and aIsForPreallocated may be
    // true.
    ContentParent(mozIApplication* aApp,
                  ContentParent* aOpener,
                  bool aIsForBrowser,
                  bool aIsForPreallocated,
                  hal::ProcessPriority aInitialPriority = hal::PROCESS_PRIORITY_FOREGROUND,
                  bool aIsNuwaProcess = false);

#ifdef MOZ_NUWA_PROCESS
    ContentParent(ContentParent* aTemplate,
                  const nsAString& aAppManifestURL,
                  base::ProcessHandle aPid,
                  const nsTArray<ProtocolFdMapping>& aFds);
#endif

    // The common initialization for the constructors.
    void InitializeMembers();

    // The common initialization logic shared by all constuctors.
    void InitInternal(ProcessPriority aPriority,
                      bool aSetupOffMainThreadCompositing,
                      bool aSendRegisteredChrome);

    virtual ~ContentParent();

    void Init();

    // Set the child process's priority and then check whether the child is
    // still alive.  Returns true if the process is still alive, and false
    // otherwise.  If you pass a FOREGROUND* priority here, it's (hopefully)
    // unlikely that the process will be killed after this point.
    bool SetPriorityAndCheckIsAlive(hal::ProcessPriority aPriority);

    // Transform a pre-allocated app process into a "real" app
    // process, for the specified manifest URL.
    void TransformPreallocatedIntoApp(const nsAString& aAppManifestURL);

    // Transform a pre-allocated app process into a browser process. If this
    // returns false, the child process has died.
    void TransformPreallocatedIntoBrowser();

    /**
     * Mark this ContentParent as dead for the purposes of Get*().
     * This method is idempotent.
     */
    void MarkAsDead();

    /**
     * Exit the subprocess and vamoose.  After this call IsAlive()
     * will return false and this ContentParent will not be returned
     * by the Get*() funtions.  However, the shutdown sequence itself
     * may be asynchronous.
     *
     * If aCloseWithError is true and this is the first call to
     * ShutDownProcess, then we'll close our channel using CloseWithError()
     * rather than vanilla Close().  CloseWithError() indicates to IPC that this
     * is an abnormal shutdown (e.g. a crash).
     */
    void ShutDownProcess(bool aCloseWithError);

    // Perform any steps necesssary to gracefully shtudown the message
    // manager and null out mMessageManager.
    void ShutDownMessageManager();

    PCompositorParent*
    AllocPCompositorParent(mozilla::ipc::Transport* aTransport,
                           base::ProcessId aOtherProcess) MOZ_OVERRIDE;
    PImageBridgeParent*
    AllocPImageBridgeParent(mozilla::ipc::Transport* aTransport,
                            base::ProcessId aOtherProcess) MOZ_OVERRIDE;

    PSharedBufferManagerParent*
    AllocPSharedBufferManagerParent(mozilla::ipc::Transport* aTranport,
                                     base::ProcessId aOtherProcess) MOZ_OVERRIDE;
    PBackgroundParent*
    AllocPBackgroundParent(Transport* aTransport, ProcessId aOtherProcess)
                           MOZ_OVERRIDE;

    virtual bool RecvGetProcessAttributes(uint64_t* aId,
                                          bool* aIsForApp,
                                          bool* aIsForBrowser) MOZ_OVERRIDE;
    virtual bool RecvGetXPCOMProcessAttributes(bool* aIsOffline) MOZ_OVERRIDE;

    virtual bool DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*) MOZ_OVERRIDE;

    virtual bool DeallocPRemoteSpellcheckEngineParent(PRemoteSpellcheckEngineParent*) MOZ_OVERRIDE;
    virtual PBrowserParent* AllocPBrowserParent(const IPCTabContext& aContext,
                                                const uint32_t& aChromeFlags,
                                                const uint64_t& aId,
                                                const bool& aIsForApp,
                                                const bool& aIsForBrowser) MOZ_OVERRIDE;
    virtual bool DeallocPBrowserParent(PBrowserParent* frame) MOZ_OVERRIDE;

    virtual PDeviceStorageRequestParent*
    AllocPDeviceStorageRequestParent(const DeviceStorageParams&) MOZ_OVERRIDE;
    virtual bool DeallocPDeviceStorageRequestParent(PDeviceStorageRequestParent*) MOZ_OVERRIDE;

    virtual PFileSystemRequestParent*
    AllocPFileSystemRequestParent(const FileSystemParams&) MOZ_OVERRIDE;

    virtual bool
    DeallocPFileSystemRequestParent(PFileSystemRequestParent*) MOZ_OVERRIDE;

    virtual PBlobParent* AllocPBlobParent(const BlobConstructorParams& aParams) MOZ_OVERRIDE;
    virtual bool DeallocPBlobParent(PBlobParent*) MOZ_OVERRIDE;

    virtual bool DeallocPCrashReporterParent(PCrashReporterParent* crashreporter) MOZ_OVERRIDE;

    virtual bool RecvGetRandomValues(const uint32_t& length,
                                     InfallibleTArray<uint8_t>* randomValues) MOZ_OVERRIDE;

    virtual bool RecvIsSecureURI(const uint32_t& type, const URIParams& uri,
                                 const uint32_t& flags, bool* isSecureURI);

    virtual bool DeallocPHalParent(PHalParent*) MOZ_OVERRIDE;

    virtual bool DeallocPIndexedDBParent(PIndexedDBParent* aActor) MOZ_OVERRIDE;

    virtual PMemoryReportRequestParent*
    AllocPMemoryReportRequestParent(const uint32_t& aGeneration,
                                    const bool &aAnonymize,
                                    const bool &aMinimizeMemoryUsage,
                                    const FileDescriptor &aDMDFile) MOZ_OVERRIDE;
    virtual bool DeallocPMemoryReportRequestParent(PMemoryReportRequestParent* actor) MOZ_OVERRIDE;

    virtual PCycleCollectWithLogsParent*
    AllocPCycleCollectWithLogsParent(const bool& aDumpAllTraces,
                                     const FileDescriptor& aGCLog,
                                     const FileDescriptor& aCCLog) MOZ_OVERRIDE;
    virtual bool
    DeallocPCycleCollectWithLogsParent(PCycleCollectWithLogsParent* aActor) MOZ_OVERRIDE;

    virtual PTestShellParent* AllocPTestShellParent() MOZ_OVERRIDE;
    virtual bool DeallocPTestShellParent(PTestShellParent* shell) MOZ_OVERRIDE;

    virtual bool DeallocPNeckoParent(PNeckoParent* necko) MOZ_OVERRIDE;

    virtual PExternalHelperAppParent* AllocPExternalHelperAppParent(
            const OptionalURIParams& aUri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const uint32_t& aContentDispositionHint,
            const nsString& aContentDispositionFilename, 
            const bool& aForceSave,
            const int64_t& aContentLength,
            const OptionalURIParams& aReferrer,
            PBrowserParent* aBrowser) MOZ_OVERRIDE;
    virtual bool DeallocPExternalHelperAppParent(PExternalHelperAppParent* aService) MOZ_OVERRIDE;

    virtual PSmsParent* AllocPSmsParent() MOZ_OVERRIDE;
    virtual bool DeallocPSmsParent(PSmsParent*) MOZ_OVERRIDE;

    virtual PTelephonyParent* AllocPTelephonyParent() MOZ_OVERRIDE;
    virtual bool DeallocPTelephonyParent(PTelephonyParent*) MOZ_OVERRIDE;

    virtual bool DeallocPStorageParent(PStorageParent* aActor) MOZ_OVERRIDE;

    virtual PBluetoothParent* AllocPBluetoothParent() MOZ_OVERRIDE;
    virtual bool DeallocPBluetoothParent(PBluetoothParent* aActor) MOZ_OVERRIDE;
    virtual bool RecvPBluetoothConstructor(PBluetoothParent* aActor) MOZ_OVERRIDE;

    virtual PFMRadioParent* AllocPFMRadioParent() MOZ_OVERRIDE;
    virtual bool DeallocPFMRadioParent(PFMRadioParent* aActor) MOZ_OVERRIDE;

    virtual PAsmJSCacheEntryParent* AllocPAsmJSCacheEntryParent(
                                 const asmjscache::OpenMode& aOpenMode,
                                 const asmjscache::WriteParams& aWriteParams,
                                 const IPC::Principal& aPrincipal) MOZ_OVERRIDE;
    virtual bool DeallocPAsmJSCacheEntryParent(
                                   PAsmJSCacheEntryParent* aActor) MOZ_OVERRIDE;

    virtual PSpeechSynthesisParent* AllocPSpeechSynthesisParent() MOZ_OVERRIDE;
    virtual bool DeallocPSpeechSynthesisParent(PSpeechSynthesisParent* aActor) MOZ_OVERRIDE;
    virtual bool RecvPSpeechSynthesisConstructor(PSpeechSynthesisParent* aActor) MOZ_OVERRIDE;

    virtual bool RecvReadPrefsArray(InfallibleTArray<PrefSetting>* aPrefs) MOZ_OVERRIDE;
    virtual bool RecvReadFontList(InfallibleTArray<FontListEntry>* retValue) MOZ_OVERRIDE;

    virtual bool RecvReadPermissions(InfallibleTArray<IPC::Permission>* aPermissions) MOZ_OVERRIDE;

    virtual bool RecvSetClipboardText(const nsString& text,
                                      const bool& isPrivateData,
                                      const int32_t& whichClipboard) MOZ_OVERRIDE;
    virtual bool RecvGetClipboardText(const int32_t& whichClipboard, nsString* text) MOZ_OVERRIDE;
    virtual bool RecvEmptyClipboard(const int32_t& whichClipboard) MOZ_OVERRIDE;
    virtual bool RecvClipboardHasText(const int32_t& whichClipboard, bool* hasText) MOZ_OVERRIDE;

    virtual bool RecvGetSystemColors(const uint32_t& colorsCount,
                                     InfallibleTArray<uint32_t>* colors) MOZ_OVERRIDE;
    virtual bool RecvGetIconForExtension(const nsCString& aFileExt,
                                         const uint32_t& aIconSize,
                                         InfallibleTArray<uint8_t>* bits) MOZ_OVERRIDE;
    virtual bool RecvGetShowPasswordSetting(bool* showPassword) MOZ_OVERRIDE;

    virtual bool RecvStartVisitedQuery(const URIParams& uri) MOZ_OVERRIDE;

    virtual bool RecvVisitURI(const URIParams& uri,
                              const OptionalURIParams& referrer,
                              const uint32_t& flags) MOZ_OVERRIDE;

    virtual bool RecvSetURITitle(const URIParams& uri,
                                 const nsString& title) MOZ_OVERRIDE;

    virtual bool RecvShowAlertNotification(const nsString& aImageUrl, const nsString& aTitle,
                                           const nsString& aText, const bool& aTextClickable,
                                           const nsString& aCookie, const nsString& aName,
                                           const nsString& aBidi, const nsString& aLang,
                                           const IPC::Principal& aPrincipal) MOZ_OVERRIDE;

    virtual bool RecvCloseAlert(const nsString& aName,
                                const IPC::Principal& aPrincipal) MOZ_OVERRIDE;

    virtual bool RecvLoadURIExternal(const URIParams& uri) MOZ_OVERRIDE;

    virtual bool RecvSyncMessage(const nsString& aMsg,
                                 const ClonedMessageData& aData,
                                 const InfallibleTArray<CpowEntry>& aCpows,
                                 const IPC::Principal& aPrincipal,
                                 InfallibleTArray<nsString>* aRetvals) MOZ_OVERRIDE;
    virtual bool AnswerRpcMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal,
                                  InfallibleTArray<nsString>* aRetvals) MOZ_OVERRIDE;
    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal) MOZ_OVERRIDE;

    virtual bool RecvFilePathUpdateNotify(const nsString& aType,
                                          const nsString& aStorageName,
                                          const nsString& aFilePath,
                                          const nsCString& aReason) MOZ_OVERRIDE;

    virtual bool RecvAddGeolocationListener(const IPC::Principal& aPrincipal,
                                            const bool& aHighAccuracy) MOZ_OVERRIDE;
    virtual bool RecvRemoveGeolocationListener() MOZ_OVERRIDE;
    virtual bool RecvSetGeolocationHigherAccuracy(const bool& aEnable) MOZ_OVERRIDE;

    virtual bool RecvConsoleMessage(const nsString& aMessage) MOZ_OVERRIDE;
    virtual bool RecvScriptError(const nsString& aMessage,
                                 const nsString& aSourceName,
                                 const nsString& aSourceLine,
                                 const uint32_t& aLineNumber,
                                 const uint32_t& aColNumber,
                                 const uint32_t& aFlags,
                                 const nsCString& aCategory) MOZ_OVERRIDE;

    virtual bool RecvPrivateDocShellsExist(const bool& aExist) MOZ_OVERRIDE;

    virtual bool RecvFirstIdle() MOZ_OVERRIDE;

    virtual bool RecvAudioChannelGetState(const AudioChannel& aChannel,
                                          const bool& aElementHidden,
                                          const bool& aElementWasHidden,
                                          AudioChannelState* aValue) MOZ_OVERRIDE;

    virtual bool RecvAudioChannelRegisterType(const AudioChannel& aChannel,
                                              const bool& aWithVideo) MOZ_OVERRIDE;
    virtual bool RecvAudioChannelUnregisterType(const AudioChannel& aChannel,
                                                const bool& aElementHidden,
                                                const bool& aWithVideo) MOZ_OVERRIDE;

    virtual bool RecvAudioChannelChangedNotification() MOZ_OVERRIDE;

    virtual bool RecvAudioChannelChangeDefVolChannel(const int32_t& aChannel,
                                                     const bool& aHidden) MOZ_OVERRIDE;
    virtual bool RecvGetSystemMemory(const uint64_t& getterId) MOZ_OVERRIDE;
    virtual bool RecvBroadcastVolume(const nsString& aVolumeName) MOZ_OVERRIDE;

    virtual bool RecvDataStoreGetStores(
                       const nsString& aName,
                       const IPC::Principal& aPrincipal,
                       InfallibleTArray<DataStoreSetting>* aValue) MOZ_OVERRIDE;

    virtual bool RecvSpeakerManagerGetSpeakerStatus(bool* aValue) MOZ_OVERRIDE;

    virtual bool RecvSpeakerManagerForceSpeaker(const bool& aEnable) MOZ_OVERRIDE;

    virtual bool RecvSystemMessageHandled() MOZ_OVERRIDE;

    virtual bool RecvNuwaReady() MOZ_OVERRIDE;

    virtual bool RecvAddNewProcess(const uint32_t& aPid,
                                   const InfallibleTArray<ProtocolFdMapping>& aFds) MOZ_OVERRIDE;

    virtual bool RecvCreateFakeVolume(const nsString& fsName, const nsString& mountPoint) MOZ_OVERRIDE;

    virtual bool RecvSetFakeVolumeState(const nsString& fsName, const int32_t& fsState) MOZ_OVERRIDE;

    virtual bool RecvKeywordToURI(const nsCString& aKeyword, OptionalInputStreamParams* aPostData,
                                  OptionalURIParams* aURI) MOZ_OVERRIDE;

    virtual void ProcessingError(Result what) MOZ_OVERRIDE;

    virtual bool RecvAllocateLayerTreeId(uint64_t* aId) MOZ_OVERRIDE;
    virtual bool RecvDeallocateLayerTreeId(const uint64_t& aId) MOZ_OVERRIDE;

    virtual bool RecvGetGraphicsFeatureStatus(const int32_t& aFeature,
                                              int32_t* aStatus,
                                              bool* aSuccess) MOZ_OVERRIDE;

    virtual bool RecvAddIdleObserver(const uint64_t& observerId,
                                     const uint32_t& aIdleTimeInS) MOZ_OVERRIDE;
    virtual bool RecvRemoveIdleObserver(const uint64_t& observerId,
                                        const uint32_t& aIdleTimeInS) MOZ_OVERRIDE;

    virtual bool
    RecvBackUpXResources(const FileDescriptor& aXSocketFd) MOZ_OVERRIDE;

    virtual bool
    RecvOpenAnonymousTemporaryFile(FileDescriptor* aFD) MOZ_OVERRIDE;

    virtual PFileDescriptorSetParent*
    AllocPFileDescriptorSetParent(const mozilla::ipc::FileDescriptor&) MOZ_OVERRIDE;

    virtual bool
    DeallocPFileDescriptorSetParent(PFileDescriptorSetParent*) MOZ_OVERRIDE;

    // If you add strong pointers to cycle collected objects here, be sure to
    // release these objects in ShutDownProcess.  See the comment there for more
    // details.

    GeckoChildProcessHost* mSubprocess;
    ContentParent* mOpener;

    uint64_t mChildID;
    int32_t mGeolocationWatchID;

    nsString mAppManifestURL;

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
    CancelableTask* mForceKillTask;
    // How many tabs we're waiting to finish their destruction
    // sequence.  Precisely, how many TabParents have called
    // NotifyTabDestroying() but not called NotifyTabDestroyed().
    int32_t mNumDestroyingTabs;
    // True only while this is ready to be used to host remote tabs.
    // This must not be used for new purposes after mIsAlive goes to
    // false, but some previously scheduled IPC traffic may still pass
    // through.
    bool mIsAlive;

    bool mSendPermissionUpdates;
    bool mSendDataStoreInfos;
    bool mIsForBrowser;
    bool mIsNuwaProcess;

    // These variables track whether we've called Close(), CloseWithError()
    // and KillHard() on our channel.
    bool mCalledClose;
    bool mCalledCloseWithError;
    bool mCalledKillHard;

    friend class CrashReporterParent;

    nsRefPtr<nsConsoleService>  mConsoleService;
    nsConsoleService* GetConsoleService();

    nsDataHashtable<nsUint64HashKey, nsRefPtr<ParentIdleListener> > mIdleListeners;

#ifdef MOZ_X11
    // Dup of child's X socket, used to scope its resources to this
    // object instead of the child process's lifetime.
    ScopedClose mChildXSocketFdDup;
#endif
};

} // namespace dom
} // namespace mozilla

class ParentIdleListener : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ParentIdleListener(mozilla::dom::ContentParent* aParent, uint64_t aObserver)
    : mParent(aParent), mObserver(aObserver)
  {}
private:
  virtual ~ParentIdleListener() {}
  nsRefPtr<mozilla::dom::ContentParent> mParent;
  uint64_t mObserver;
};

#endif
