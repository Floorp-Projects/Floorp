/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/PContentChild.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsTHashtable.h"

#include "nsWeakPtr.h"


struct ChromePackage;
class nsIDOMBlob;
class nsIObserver;
struct ResourceMapping;
struct OverrideMapping;

namespace mozilla {
class RemoteSpellcheckEngineChild;

namespace ipc {
class OptionalURIParams;
class PFileDescriptorSetChild;
class URIParams;
}// namespace ipc

namespace jsipc {
class JavaScriptShared;
}

namespace layers {
class PCompositorChild;
} // namespace layers

namespace dom {

class AlertObserver;
class PrefObserver;
class ConsoleListener;
class PStorageChild;
class ClonedMessageData;

class ContentChild : public PContentChild
                   , public nsIContentChild
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
    typedef mozilla::ipc::PFileDescriptorSetChild PFileDescriptorSetChild;
    typedef mozilla::ipc::URIParams URIParams;

public:
    ContentChild();
    virtual ~ContentChild();
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
    NS_IMETHOD_(MozExternalRefCountType) AddRef(void) { return 1; }
    NS_IMETHOD_(MozExternalRefCountType) Release(void) { return 1; }

    struct AppInfo
    {
        nsCString version;
        nsCString buildID;
        nsCString name;
        nsCString UAName;
        nsCString ID;
        nsCString vendor;
    };

    bool Init(MessageLoop* aIOLoop,
              base::ProcessHandle aParentHandle,
              IPC::Channel* aChannel);
    void InitProcessAttributes();
    void InitXPCOM();

    static ContentChild* GetSingleton() {
        return sSingleton;
    }

    const AppInfo& GetAppInfo() {
        return mAppInfo;
    }
    void SetProcessName(const nsAString& aName, bool aDontOverride = false);
    void GetProcessName(nsAString& aName);
    void GetProcessName(nsACString& aName);
    bool IsAlive();
    static void AppendProcessId(nsACString& aName);

    ContentBridgeParent* GetLastBridge() {
        MOZ_ASSERT(mLastBridge);
        ContentBridgeParent* parent = mLastBridge;
        mLastBridge = nullptr;
        return parent;
    }
    nsRefPtr<ContentBridgeParent> mLastBridge;

    PContentBridgeParent*
    AllocPContentBridgeParent(mozilla::ipc::Transport* transport,
                              base::ProcessId otherProcess) MOZ_OVERRIDE;
    PContentBridgeChild*
    AllocPContentBridgeChild(mozilla::ipc::Transport* transport,
                             base::ProcessId otherProcess) MOZ_OVERRIDE;

    PCompositorChild*
    AllocPCompositorChild(mozilla::ipc::Transport* aTransport,
                          base::ProcessId aOtherProcess) MOZ_OVERRIDE;

    PSharedBufferManagerChild*
    AllocPSharedBufferManagerChild(mozilla::ipc::Transport* aTransport,
                                    base::ProcessId aOtherProcess) MOZ_OVERRIDE;

    PImageBridgeChild*
    AllocPImageBridgeChild(mozilla::ipc::Transport* aTransport,
                           base::ProcessId aOtherProcess) MOZ_OVERRIDE;

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
    // Cleans up any resources used by the process when sandboxed.
    void CleanUpSandboxEnvironment();
#endif

    virtual bool RecvSetProcessSandbox() MOZ_OVERRIDE;

    PBackgroundChild*
    AllocPBackgroundChild(Transport* aTransport, ProcessId aOtherProcess)
                          MOZ_OVERRIDE;

    virtual PBrowserChild* AllocPBrowserChild(const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const uint64_t& aID,
                                              const bool& aIsForApp,
                                              const bool& aIsForBrowser);
    virtual bool DeallocPBrowserChild(PBrowserChild*);

    virtual PDeviceStorageRequestChild* AllocPDeviceStorageRequestChild(const DeviceStorageParams&);
    virtual bool DeallocPDeviceStorageRequestChild(PDeviceStorageRequestChild*);

    virtual PFileSystemRequestChild* AllocPFileSystemRequestChild(const FileSystemParams&);
    virtual bool DeallocPFileSystemRequestChild(PFileSystemRequestChild*);

    virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams)
                                        MOZ_OVERRIDE;
    virtual bool DeallocPBlobChild(PBlobChild* aActor) MOZ_OVERRIDE;

    virtual PCrashReporterChild*
    AllocPCrashReporterChild(const mozilla::dom::NativeThreadId& id,
                             const uint32_t& processType) MOZ_OVERRIDE;
    virtual bool
    DeallocPCrashReporterChild(PCrashReporterChild*) MOZ_OVERRIDE;

    virtual PHalChild* AllocPHalChild() MOZ_OVERRIDE;
    virtual bool DeallocPHalChild(PHalChild*) MOZ_OVERRIDE;

    virtual PMemoryReportRequestChild*
    AllocPMemoryReportRequestChild(const uint32_t& aGeneration,
                                   const bool& aAnonymize,
                                   const bool& aMinimizeMemoryUsage,
                                   const MaybeFileDesc& aDMDFile) MOZ_OVERRIDE;
    virtual bool
    DeallocPMemoryReportRequestChild(PMemoryReportRequestChild* actor) MOZ_OVERRIDE;

    virtual bool
    RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* aChild,
                                        const uint32_t& aGeneration,
                                        const bool& aAnonymize,
                                        const bool &aMinimizeMemoryUsage,
                                        const MaybeFileDesc &aDMDFile) MOZ_OVERRIDE;

    virtual PCycleCollectWithLogsChild*
    AllocPCycleCollectWithLogsChild(const bool& aDumpAllTraces,
                                    const FileDescriptor& aGCLog,
                                    const FileDescriptor& aCCLog) MOZ_OVERRIDE;
    virtual bool
    DeallocPCycleCollectWithLogsChild(PCycleCollectWithLogsChild* aActor) MOZ_OVERRIDE;
    virtual bool
    RecvPCycleCollectWithLogsConstructor(PCycleCollectWithLogsChild* aChild,
                                         const bool& aDumpAllTraces,
                                         const FileDescriptor& aGCLog,
                                         const FileDescriptor& aCCLog) MOZ_OVERRIDE;

    virtual bool
    RecvAudioChannelNotify() MOZ_OVERRIDE;

    virtual bool
    RecvDataStoreNotify(const uint32_t& aAppId, const nsString& aName,
                        const nsString& aManifestURL) MOZ_OVERRIDE;

    virtual PTestShellChild* AllocPTestShellChild() MOZ_OVERRIDE;
    virtual bool DeallocPTestShellChild(PTestShellChild*) MOZ_OVERRIDE;
    virtual bool RecvPTestShellConstructor(PTestShellChild*) MOZ_OVERRIDE;
    jsipc::JavaScriptShared* GetCPOWManager() MOZ_OVERRIDE;

    PMobileConnectionChild*
    SendPMobileConnectionConstructor(PMobileConnectionChild* aActor,
                                     const uint32_t& aClientId);
    virtual PMobileConnectionChild*
    AllocPMobileConnectionChild(const uint32_t& aClientId) MOZ_OVERRIDE;
    virtual bool
    DeallocPMobileConnectionChild(PMobileConnectionChild* aActor) MOZ_OVERRIDE;

    virtual PNeckoChild* AllocPNeckoChild() MOZ_OVERRIDE;
    virtual bool DeallocPNeckoChild(PNeckoChild*) MOZ_OVERRIDE;

    virtual PScreenManagerChild*
    AllocPScreenManagerChild(uint32_t* aNumberOfScreens,
                             float* aSystemDefaultScale,
                             bool* aSuccess) MOZ_OVERRIDE;
    virtual bool DeallocPScreenManagerChild(PScreenManagerChild*) MOZ_OVERRIDE;

    virtual PExternalHelperAppChild *AllocPExternalHelperAppChild(
            const OptionalURIParams& uri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const uint32_t& aContentDispositionHint,
            const nsString& aContentDispositionFilename,
            const bool& aForceSave,
            const int64_t& aContentLength,
            const OptionalURIParams& aReferrer,
            PBrowserChild* aBrowser) MOZ_OVERRIDE;
    virtual bool DeallocPExternalHelperAppChild(PExternalHelperAppChild *aService) MOZ_OVERRIDE;

    virtual PCellBroadcastChild* AllocPCellBroadcastChild() MOZ_OVERRIDE;
    PCellBroadcastChild* SendPCellBroadcastConstructor(PCellBroadcastChild* aActor);
    virtual bool DeallocPCellBroadcastChild(PCellBroadcastChild* aActor) MOZ_OVERRIDE;

    virtual PSmsChild* AllocPSmsChild() MOZ_OVERRIDE;
    virtual bool DeallocPSmsChild(PSmsChild*) MOZ_OVERRIDE;

    virtual PTelephonyChild* AllocPTelephonyChild() MOZ_OVERRIDE;
    virtual bool DeallocPTelephonyChild(PTelephonyChild*) MOZ_OVERRIDE;

    virtual PVoicemailChild* AllocPVoicemailChild() MOZ_OVERRIDE;
    PVoicemailChild* SendPVoicemailConstructor(PVoicemailChild* aActor);
    virtual bool DeallocPVoicemailChild(PVoicemailChild*) MOZ_OVERRIDE;

    virtual PStorageChild* AllocPStorageChild() MOZ_OVERRIDE;
    virtual bool DeallocPStorageChild(PStorageChild* aActor) MOZ_OVERRIDE;

    virtual PBluetoothChild* AllocPBluetoothChild() MOZ_OVERRIDE;
    virtual bool DeallocPBluetoothChild(PBluetoothChild* aActor) MOZ_OVERRIDE;

    virtual PFMRadioChild* AllocPFMRadioChild() MOZ_OVERRIDE;
    virtual bool DeallocPFMRadioChild(PFMRadioChild* aActor) MOZ_OVERRIDE;

    virtual PAsmJSCacheEntryChild* AllocPAsmJSCacheEntryChild(
                                 const asmjscache::OpenMode& aOpenMode,
                                 const asmjscache::WriteParams& aWriteParams,
                                 const IPC::Principal& aPrincipal) MOZ_OVERRIDE;
    virtual bool DeallocPAsmJSCacheEntryChild(
                                    PAsmJSCacheEntryChild* aActor) MOZ_OVERRIDE;

    virtual PSpeechSynthesisChild* AllocPSpeechSynthesisChild() MOZ_OVERRIDE;
    virtual bool DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor) MOZ_OVERRIDE;

    virtual bool RecvRegisterChrome(const InfallibleTArray<ChromePackage>& packages,
                                    const InfallibleTArray<ResourceMapping>& resources,
                                    const InfallibleTArray<OverrideMapping>& overrides,
                                    const nsCString& locale,
                                    const bool& reset) MOZ_OVERRIDE;
    virtual bool RecvRegisterChromeItem(const ChromeRegistryItem& item) MOZ_OVERRIDE;

    virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() MOZ_OVERRIDE;
    virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) MOZ_OVERRIDE;
    virtual PRemoteSpellcheckEngineChild* AllocPRemoteSpellcheckEngineChild() MOZ_OVERRIDE;
    virtual bool DeallocPRemoteSpellcheckEngineChild(PRemoteSpellcheckEngineChild*) MOZ_OVERRIDE;

    virtual bool RecvSetOffline(const bool& offline) MOZ_OVERRIDE;

    virtual bool RecvSpeakerManagerNotify() MOZ_OVERRIDE;

    virtual bool RecvNotifyVisited(const URIParams& aURI) MOZ_OVERRIDE;
    // auto remove when alertfinished is received.
    nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

    virtual bool RecvSystemMemoryAvailable(const uint64_t& aGetterId,
                                           const uint32_t& aMemoryAvailable) MOZ_OVERRIDE;

    virtual bool RecvPreferenceUpdate(const PrefSetting& aPref) MOZ_OVERRIDE;

    virtual bool RecvNotifyAlertsObserver(const nsCString& aType,
                                          const nsString& aData) MOZ_OVERRIDE;

    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal) MOZ_OVERRIDE;

    virtual bool RecvGeolocationUpdate(const GeoPosition& somewhere) MOZ_OVERRIDE;

    virtual bool RecvUpdateDictionaryList(const InfallibleTArray<nsString>& aDictionaries) MOZ_OVERRIDE;

    virtual bool RecvAddPermission(const IPC::Permission& permission) MOZ_OVERRIDE;

    virtual bool RecvScreenSizeChanged(const gfxIntSize &size) MOZ_OVERRIDE;

    virtual bool RecvFlushMemory(const nsString& reason) MOZ_OVERRIDE;

    virtual bool RecvActivateA11y() MOZ_OVERRIDE;

    virtual bool RecvGarbageCollect() MOZ_OVERRIDE;
    virtual bool RecvCycleCollect() MOZ_OVERRIDE;

    virtual bool RecvAppInfo(const nsCString& version, const nsCString& buildID,
                             const nsCString& name, const nsCString& UAName,
                             const nsCString& ID, const nsCString& vendor) MOZ_OVERRIDE;

    virtual bool RecvLastPrivateDocShellDestroyed() MOZ_OVERRIDE;

    virtual bool RecvFilePathUpdate(const nsString& aStorageType,
                                    const nsString& aStorageName,
                                    const nsString& aPath,
                                    const nsCString& aReason) MOZ_OVERRIDE;
    virtual bool RecvFileSystemUpdate(const nsString& aFsName,
                                      const nsString& aVolumeName,
                                      const int32_t& aState,
                                      const int32_t& aMountGeneration,
                                      const bool& aIsMediaPresent,
                                      const bool& aIsSharing,
                                      const bool& aIsFormatting,
                                      const bool& aIsFake,
                                      const bool& aIsUnmounting) MOZ_OVERRIDE;

    virtual bool RecvNuwaFork() MOZ_OVERRIDE;

    virtual bool
    RecvNotifyProcessPriorityChanged(const hal::ProcessPriority& aPriority) MOZ_OVERRIDE;
    virtual bool RecvMinimizeMemoryUsage() MOZ_OVERRIDE;

    virtual bool RecvLoadAndRegisterSheet(const URIParams& aURI,
                                          const uint32_t& aType) MOZ_OVERRIDE;
    virtual bool RecvUnregisterSheet(const URIParams& aURI, const uint32_t& aType) MOZ_OVERRIDE;

    virtual bool RecvNotifyPhoneStateChange(const nsString& state) MOZ_OVERRIDE;

    void AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);
    void RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);
    virtual bool RecvNotifyIdleObserver(const uint64_t& aObserver,
                                        const nsCString& aTopic,
                                        const nsString& aData) MOZ_OVERRIDE;

    virtual bool RecvOnAppThemeChanged() MOZ_OVERRIDE;

#ifdef ANDROID
    gfxIntSize GetScreenSize() { return mScreenSize; }
#endif

    // Get the directory for IndexedDB files. We query the parent for this and
    // cache the value
    nsString &GetIndexedDBPath();

    uint64_t GetID() { return mID; }

    bool IsForApp() { return mIsForApp; }
    bool IsForBrowser() { return mIsForBrowser; }

    virtual PBlobChild*
    SendPBlobConstructor(PBlobChild* actor,
                         const BlobConstructorParams& params) MOZ_OVERRIDE;

    virtual PFileDescriptorSetChild*
    AllocPFileDescriptorSetChild(const FileDescriptor&) MOZ_OVERRIDE;

    virtual bool
    DeallocPFileDescriptorSetChild(PFileDescriptorSetChild*) MOZ_OVERRIDE;

    virtual bool SendPBrowserConstructor(PBrowserChild* actor,
                                         const IPCTabContext& context,
                                         const uint32_t& chromeFlags,
                                         const uint64_t& aID,
                                         const bool& aIsForApp,
                                         const bool& aIsForBrowser) MOZ_OVERRIDE;

    virtual bool RecvPBrowserConstructor(PBrowserChild* aCctor,
                                         const IPCTabContext& aContext,
                                         const uint32_t& aChromeFlags,
                                         const uint64_t& aID,
                                         const bool& aIsForApp,
                                         const bool& aIsForBrowser) MOZ_OVERRIDE;

    void GetAvailableDictionaries(InfallibleTArray<nsString>& aDictionaries);

private:
    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    virtual void ProcessingError(Result what) MOZ_OVERRIDE;

    /**
     * Exit *now*.  Do not shut down XPCOM, do not pass Go, do not run
     * static destructors, do not collect $200.
     */
    MOZ_NORETURN void QuickExit();

    InfallibleTArray<nsAutoPtr<AlertObserver> > mAlertObservers;
    nsRefPtr<ConsoleListener> mConsoleListener;

    nsTHashtable<nsPtrHashKey<nsIObserver>> mIdleObservers;

    InfallibleTArray<nsString> mAvailableDictionaries;

    /**
     * An ID unique to the process containing our corresponding
     * content parent.
     *
     * We expect our content parent to set this ID immediately after opening a
     * channel to us.
     */
    uint64_t mID;

    AppInfo mAppInfo;

#ifdef ANDROID
    gfxIntSize mScreenSize;
#endif

    bool mIsForApp;
    bool mIsForBrowser;
    bool mCanOverrideProcessName;
    bool mIsAlive;
    nsString mProcessName;

    static ContentChild* sSingleton;

    DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

} // namespace dom
} // namespace mozilla

#endif
