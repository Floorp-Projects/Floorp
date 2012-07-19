/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PContentChild.h"

#include "nsTArray.h"
#include "nsIConsoleListener.h"

struct ChromePackage;
class nsIObserver;
struct ResourceMapping;
struct OverrideMapping;

namespace mozilla {

namespace layers {
class PCompositorChild;
}

namespace dom {

class AlertObserver;
class PrefObserver;
class ConsoleListener;
class PStorageChild;

class ContentChild : public PContentChild
{
    typedef layers::PCompositorChild PCompositorChild;

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
        NS_ASSERTION(sSingleton, "not initialized");
        return sSingleton;
    }

    const AppInfo& GetAppInfo() {
        return mAppInfo;
    }

    PCompositorChild* AllocPCompositor(ipc::Transport* aTransport,
                                       base::ProcessId aOtherProcess) MOZ_OVERRIDE;

    virtual PBrowserChild* AllocPBrowser(const PRUint32& aChromeFlags,
                                         const bool& aIsBrowserFrame);
    virtual bool DeallocPBrowser(PBrowserChild*);

    virtual PDeviceStorageRequestChild* AllocPDeviceStorageRequest(const DeviceStorageParams&);
    virtual bool DeallocPDeviceStorageRequest(PDeviceStorageRequestChild*);

    virtual PCrashReporterChild*
    AllocPCrashReporter(const mozilla::dom::NativeThreadId& id,
                        const PRUint32& processType);
    virtual bool
    DeallocPCrashReporter(PCrashReporterChild*);

    NS_OVERRIDE virtual PHalChild* AllocPHal();
    NS_OVERRIDE virtual bool DeallocPHal(PHalChild*);

    virtual PIndexedDBChild* AllocPIndexedDB();
    virtual bool DeallocPIndexedDB(PIndexedDBChild* aActor);

    virtual PMemoryReportRequestChild*
    AllocPMemoryReportRequest();

    virtual bool
    DeallocPMemoryReportRequest(PMemoryReportRequestChild* actor);

    virtual bool
    RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* child);

    virtual PTestShellChild* AllocPTestShell();
    virtual bool DeallocPTestShell(PTestShellChild*);
    virtual bool RecvPTestShellConstructor(PTestShellChild*);

    virtual PAudioChild* AllocPAudio(const PRInt32&,
                                     const PRInt32&,
                                     const PRInt32&);
    virtual bool DeallocPAudio(PAudioChild*);

    virtual PNeckoChild* AllocPNecko();
    virtual bool DeallocPNecko(PNeckoChild*);

    virtual PExternalHelperAppChild *AllocPExternalHelperApp(
            const IPC::URI& uri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const bool& aForceSave,
            const PRInt64& aContentLength,
            const IPC::URI& aReferrer);
    virtual bool DeallocPExternalHelperApp(PExternalHelperAppChild *aService);

    virtual PSmsChild* AllocPSms();
    virtual bool DeallocPSms(PSmsChild*);

    virtual PStorageChild* AllocPStorage(const StorageConstructData& aData);
    virtual bool DeallocPStorage(PStorageChild* aActor);

    virtual bool RecvRegisterChrome(const InfallibleTArray<ChromePackage>& packages,
                                    const InfallibleTArray<ResourceMapping>& resources,
                                    const InfallibleTArray<OverrideMapping>& overrides,
                                    const nsCString& locale);

    virtual bool RecvSetOffline(const bool& offline);

    virtual bool RecvNotifyVisited(const IPC::URI& aURI);
    // auto remove when alertfinished is received.
    nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

    virtual bool RecvPreferenceUpdate(const PrefTuple& aPref);
    virtual bool RecvClearUserPreference(const nsCString& aPrefName);

    virtual bool RecvNotifyAlertsObserver(const nsCString& aType, const nsString& aData);

    virtual bool RecvAsyncMessage(const nsString& aMsg, const nsString& aJSON);

    virtual bool RecvGeolocationUpdate(const GeoPosition& somewhere);

    virtual bool RecvAddPermission(const IPC::Permission& permission);

    virtual bool RecvScreenSizeChanged(const gfxIntSize &size);

    virtual bool RecvFlushMemory(const nsString& reason);

    virtual bool RecvActivateA11y();

    virtual bool RecvGarbageCollect();
    virtual bool RecvCycleCollect();

    virtual bool RecvAppInfo(const nsCString& version, const nsCString& buildID);
    virtual bool RecvSetID(const PRUint64 &id);

    virtual bool RecvLastPrivateDocShellDestroyed();

#ifdef ANDROID
    gfxIntSize GetScreenSize() { return mScreenSize; }
#endif

    // Get the directory for IndexedDB files. We query the parent for this and
    // cache the value
    nsString &GetIndexedDBPath();

    PRUint64 GetID() { return mID; }

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
#ifdef ANDROID
    gfxIntSize mScreenSize;
#endif

    /**
     * An ID unique to the process containing our corresponding
     * content parent.
     *
     * We expect our content parent to set this ID immediately after opening a
     * channel to us.
     */
    PRUint64 mID;

    AppInfo mAppInfo;

    static ContentChild* sSingleton;

    DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

} // namespace dom
} // namespace mozilla

#endif
