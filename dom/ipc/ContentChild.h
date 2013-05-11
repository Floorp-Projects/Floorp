/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/ipc/Blob.h"

#include "nsTArray.h"
#include "nsIConsoleListener.h"

struct ChromePackage;
class nsIDOMBlob;
class nsIObserver;
struct ResourceMapping;
struct OverrideMapping;

namespace mozilla {

namespace ipc {
class OptionalURIParams;
class URIParams;
}// namespace ipc

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
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
    typedef mozilla::ipc::URIParams URIParams;

public:
    ContentChild();
    virtual ~ContentChild();

    struct AppInfo
    {
        nsCString version;
        nsCString buildID;
    };

    bool Init(MessageLoop* aIOLoop,
              base::ProcessHandle aParentHandle,
              IPC::Channel* aChannel);
    void InitXPCOM();

    static ContentChild* GetSingleton() {
        return sSingleton;
    }

    const AppInfo& GetAppInfo() {
        return mAppInfo;
    }

    void SetProcessName(const nsAString& aName);
    const void GetProcessName(nsAString& aName);

    PCompositorChild*
    AllocPCompositor(mozilla::ipc::Transport* aTransport,
                     base::ProcessId aOtherProcess) MOZ_OVERRIDE;
    PImageBridgeChild*
    AllocPImageBridge(mozilla::ipc::Transport* aTransport,
                      base::ProcessId aOtherProcess) MOZ_OVERRIDE;

    virtual bool RecvSetProcessPrivileges(const ChildPrivileges& aPrivs);

    virtual PBrowserChild* AllocPBrowser(const IPCTabContext &aContext,
                                         const uint32_t &chromeFlags);
    virtual bool DeallocPBrowser(PBrowserChild*);

    virtual PDeviceStorageRequestChild* AllocPDeviceStorageRequest(const DeviceStorageParams&);
    virtual bool DeallocPDeviceStorageRequest(PDeviceStorageRequestChild*);

    virtual PBlobChild* AllocPBlob(const BlobConstructorParams& aParams);
    virtual bool DeallocPBlob(PBlobChild*);

    virtual PCrashReporterChild*
    AllocPCrashReporter(const mozilla::dom::NativeThreadId& id,
                        const uint32_t& processType);
    virtual bool
    DeallocPCrashReporter(PCrashReporterChild*);

    virtual PHalChild* AllocPHal() MOZ_OVERRIDE;
    virtual bool DeallocPHal(PHalChild*) MOZ_OVERRIDE;

    virtual PIndexedDBChild* AllocPIndexedDB();
    virtual bool DeallocPIndexedDB(PIndexedDBChild* aActor);

    virtual PMemoryReportRequestChild*
    AllocPMemoryReportRequest();

    virtual bool
    DeallocPMemoryReportRequest(PMemoryReportRequestChild* actor);

    virtual bool
    RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* child);

    virtual bool
    RecvAudioChannelNotify();

    virtual bool
    RecvDumpMemoryInfoToTempDir(const nsString& aIdentifier,
                                const bool& aMinimizeMemoryUsage,
                                const bool& aDumpChildProcesses);
    virtual bool
    RecvDumpGCAndCCLogsToFile(const nsString& aIdentifier,
                              const bool& aDumpChildProcesses);

    virtual PTestShellChild* AllocPTestShell();
    virtual bool DeallocPTestShell(PTestShellChild*);
    virtual bool RecvPTestShellConstructor(PTestShellChild*);

    virtual PNeckoChild* AllocPNecko();
    virtual bool DeallocPNecko(PNeckoChild*);

    virtual PExternalHelperAppChild *AllocPExternalHelperApp(
            const OptionalURIParams& uri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const bool& aForceSave,
            const int64_t& aContentLength,
            const OptionalURIParams& aReferrer);
    virtual bool DeallocPExternalHelperApp(PExternalHelperAppChild *aService);

    virtual PSmsChild* AllocPSms();
    virtual bool DeallocPSms(PSmsChild*);

    virtual PStorageChild* AllocPStorage();
    virtual bool DeallocPStorage(PStorageChild* aActor);

    virtual PBluetoothChild* AllocPBluetooth();
    virtual bool DeallocPBluetooth(PBluetoothChild* aActor);

    virtual PSpeechSynthesisChild* AllocPSpeechSynthesis();
    virtual bool DeallocPSpeechSynthesis(PSpeechSynthesisChild* aActor);

    virtual bool RecvRegisterChrome(const InfallibleTArray<ChromePackage>& packages,
                                    const InfallibleTArray<ResourceMapping>& resources,
                                    const InfallibleTArray<OverrideMapping>& overrides,
                                    const nsCString& locale);

    virtual bool RecvSetOffline(const bool& offline);

    virtual bool RecvNotifyVisited(const URIParams& aURI);
    // auto remove when alertfinished is received.
    nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

    virtual bool RecvPreferenceUpdate(const PrefSetting& aPref);

    virtual bool RecvNotifyAlertsObserver(const nsCString& aType, const nsString& aData);

    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData);

    virtual bool RecvGeolocationUpdate(const GeoPosition& somewhere);

    virtual bool RecvAddPermission(const IPC::Permission& permission);

    virtual bool RecvScreenSizeChanged(const gfxIntSize &size);

    virtual bool RecvFlushMemory(const nsString& reason);

    virtual bool RecvActivateA11y();

    virtual bool RecvGarbageCollect();
    virtual bool RecvCycleCollect();

    virtual bool RecvAppInfo(const nsCString& version, const nsCString& buildID);

    virtual bool RecvLastPrivateDocShellDestroyed();

    virtual bool RecvFilePathUpdate(const nsString& aStorageType,
                                    const nsString& aStorageName,
                                    const nsString& aPath,
                                    const nsCString& aReason);
    virtual bool RecvFileSystemUpdate(const nsString& aFsName,
                                      const nsString& aVolumeName,
                                      const int32_t& aState,
                                      const int32_t& aMountGeneration);

    virtual bool RecvNotifyProcessPriorityChanged(const hal::ProcessPriority& aPriority);
    virtual bool RecvMinimizeMemoryUsage();
    virtual bool RecvCancelMinimizeMemoryUsage();

#ifdef ANDROID
    gfxIntSize GetScreenSize() { return mScreenSize; }
#endif

    // Get the directory for IndexedDB files. We query the parent for this and
    // cache the value
    nsString &GetIndexedDBPath();

    uint64_t GetID() { return mID; }

    bool IsForApp() { return mIsForApp; }
    bool IsForBrowser() { return mIsForBrowser; }

    BlobChild* GetOrCreateActorForBlob(nsIDOMBlob* aBlob);

protected:
    virtual bool RecvPBrowserConstructor(PBrowserChild* actor,
                                         const IPCTabContext& context,
                                         const uint32_t& chromeFlags);

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
    nsString mProcessName;
    nsWeakPtr mMemoryMinimizerRunnable;

    static ContentChild* sSingleton;

    DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

} // namespace dom
} // namespace mozilla

#endif
