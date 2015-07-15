/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"

#include "ContentParent.h"

#if defined(ANDROID) || defined(LINUX)
# include <sys/time.h>
# include <sys/resource.h>
#endif

#ifdef MOZ_WIDGET_GONK
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "chrome/common/process_watcher.h"

#include <set>

#include "AppProcessChecker.h"
#include "AudioChannelService.h"
#include "BlobParent.h"
#include "CrashReporterParent.h"
#include "GMPServiceParent.h"
#include "IHistory.h"
#include "imgIContainer.h"
#include "mozIApplication.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/docshell/OfflineCacheUpdateParent.h"
#include "mozilla/dom/DataStoreService.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DOMStorageIPC.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ExternalHelperAppParent.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/GeolocationBinding.h"
#include "mozilla/dom/PContentBridgeParent.h"
#include "mozilla/dom/PContentPermissionRequestParent.h"
#include "mozilla/dom/PCycleCollectWithLogsParent.h"
#include "mozilla/dom/PFMRadioParent.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/bluetooth/PBluetoothParent.h"
#include "mozilla/dom/cellbroadcast/CellBroadcastParent.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestParent.h"
#include "mozilla/dom/icc/IccParent.h"
#include "mozilla/dom/mobileconnection/MobileConnectionParent.h"
#include "mozilla/dom/mobilemessage/SmsParent.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/telephony/TelephonyParent.h"
#include "mozilla/dom/time/DateCacheCleaner.h"
#include "mozilla/dom/voicemail/VoicemailParent.h"
#include "mozilla/embedding/printingui/PrintingParent.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/PFileDescriptorSetParent.h"
#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/media/MediaParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/plugins/PluginBridge.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ProcessHangMonitorIPC.h"
#ifdef MOZ_ENABLE_PROFILER_SPS
#include "mozilla/ProfileGatherer.h"
#endif
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/unused.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsAppRunner.h"
#include "nsAutoPtr.h"
#include "nsCDefaultURIFixup.h"
#include "nsCExternalHandlerService.h"
#include "nsCOMPtr.h"
#include "nsChromeRegistryChrome.h"
#include "nsConsoleMessage.h"
#include "nsConsoleService.h"
#include "nsContentUtils.h"
#include "nsDebugImpl.h"
#include "nsFrameMessageManager.h"
#include "nsGeolocationSettings.h"
#include "nsHashPropertyBag.h"
#include "nsIAlertsService.h"
#include "nsIAppsService.h"
#include "nsIClipboard.h"
#include "nsContentPermissionHelper.h"
#include "nsICycleCollectorListener.h"
#include "nsIDocument.h"
#include "nsIDOMGeoGeolocation.h"
#include "nsIDOMGeoPositionError.h"
#include "nsIDragService.h"
#include "mozilla/dom/WakeLock.h"
#include "nsIDOMWindow.h"
#include "nsIExternalProtocolService.h"
#include "nsIFormProcessor.h"
#include "nsIGfxInfo.h"
#include "nsIIdleService.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMemoryReporter.h"
#include "nsIMozBrowserFrame.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsISiteSecurityService.h"
#include "nsISpellChecker.h"
#include "nsIStyleSheet.h"
#include "nsISupportsPrimitives.h"
#include "nsISystemMessagesInternal.h"
#include "nsITimer.h"
#include "nsIURIFixup.h"
#include "nsIWindowWatcher.h"
#include "nsIXULRuntime.h"
#include "gfxDrawable.h"
#include "ImageOps.h"
#include "nsMemoryInfoDumper.h"
#include "nsMemoryReporterManager.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleSheetService.h"
#include "nsThreadUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsWidgetsCID.h"
#include "PreallocatedProcessManager.h"
#include "ProcessPriorityManager.h"
#include "SandboxHal.h"
#include "ScreenManagerParent.h"
#include "SourceSurfaceRawData.h"
#include "StructuredCloneUtils.h"
#include "TabParent.h"
#include "URIUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShell.h"
#include "nsDocShell.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "gfxPrefs.h"
#include "prio.h"
#include "private/pprio.h"
#include "ContentProcessManager.h"
#include "mozilla/psm/PSMContentListener.h"
#include "nsPluginHost.h"
#include "nsPluginTags.h"
#include "nsIBlocklistService.h"

#include "nsIBidiKeyboard.h"

#ifdef MOZ_WEBRTC
#include "signaling/src/peerconnection/WebrtcGlobalParent.h"
#endif

#if defined(ANDROID) || defined(LINUX)
#include "nsSystemInfo.h"
#endif

#if defined(XP_LINUX)
#include "mozilla/Hal.h"
#endif

#ifdef ANDROID
# include "gfxAndroidPlatform.h"
#endif

#ifdef MOZ_PERMISSIONS
# include "nsPermissionManager.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
# include "AndroidBridge.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "nsIVolume.h"
#include "nsVolumeService.h"
#include "nsIVolumeService.h"
#include "SpeakerManagerService.h"
using namespace mozilla::system;
#endif

#ifdef MOZ_B2G_BT
#include "BluetoothParent.h"
#include "BluetoothService.h"
#endif

#include "mozilla/RemoteSpellCheckEngineParent.h"

#ifdef MOZ_B2G_FM
#include "mozilla/dom/FMRadioParent.h"
#endif

#include "Crypto.h"

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/SpeechSynthesisParent.h"
#endif

#ifdef ENABLE_TESTS
#include "BackgroundChildImpl.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#endif


#if defined(MOZ_CONTENT_SANDBOX) && defined(XP_LINUX)
#include "mozilla/SandboxInfo.h"
#endif

#ifdef MOZ_TOOLKIT_SEARCH
#include "nsIBrowserSearchService.h"
#endif

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "nsIProfiler.h"
#include "nsIProfileSaveEvent.h"
#endif

#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadMonitoring.h"
#endif

static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

using base::ChildPrivileges;
using base::KillProcess;
#ifdef MOZ_ENABLE_PROFILER_SPS
using mozilla::ProfileGatherer;
#endif

#ifdef MOZ_CRASHREPORTER
using namespace CrashReporter;
#endif
using namespace mozilla::dom::bluetooth;
using namespace mozilla::dom::cellbroadcast;
using namespace mozilla::dom::devicestorage;
using namespace mozilla::dom::icc;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::power;
using namespace mozilla::dom::mobileconnection;
using namespace mozilla::dom::mobilemessage;
using namespace mozilla::dom::telephony;
using namespace mozilla::dom::voicemail;
using namespace mozilla::media;
using namespace mozilla::embedding;
using namespace mozilla::gmp;
using namespace mozilla::hal;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;
using namespace mozilla::jsipc;
using namespace mozilla::psm;
using namespace mozilla::widget;

#ifdef ENABLE_TESTS

class BackgroundTester final : public nsIIPCBackgroundChildCreateCallback,
                               public nsIObserver
{
    static uint32_t sCallbackCount;

private:
    ~BackgroundTester()
    { }

    virtual void
    ActorCreated(PBackgroundChild* aActor) override
    {
        MOZ_RELEASE_ASSERT(aActor,
                           "Failed to create a PBackgroundChild actor!");

        NS_NAMED_LITERAL_CSTRING(testStr, "0123456789");

        PBackgroundTestChild* testActor =
            aActor->SendPBackgroundTestConstructor(testStr);
        MOZ_RELEASE_ASSERT(testActor);

        if (!sCallbackCount) {
            PBackgroundChild* existingBackgroundChild =
                BackgroundChild::GetForCurrentThread();

            MOZ_RELEASE_ASSERT(existingBackgroundChild);
            MOZ_RELEASE_ASSERT(existingBackgroundChild == aActor);

            bool ok =
                existingBackgroundChild->
                    SendPBackgroundTestConstructor(testStr);
            MOZ_RELEASE_ASSERT(ok);

            // Callback 3.
            ok = BackgroundChild::GetOrCreateForCurrentThread(this);
            MOZ_RELEASE_ASSERT(ok);
        }

        sCallbackCount++;
    }

    virtual void
    ActorFailed() override
    {
        MOZ_CRASH("Failed to create a PBackgroundChild actor!");
    }

    NS_IMETHOD
    Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
            override
    {
        nsCOMPtr<nsIObserverService> observerService =
            mozilla::services::GetObserverService();
        MOZ_RELEASE_ASSERT(observerService);

        nsresult rv = observerService->RemoveObserver(this, aTopic);
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

        if (!strcmp(aTopic, "profile-after-change")) {
            if (mozilla::Preferences::GetBool("pbackground.testing", false)) {
                rv = observerService->AddObserver(this, "xpcom-shutdown",
                                                  false);
                MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

                // Callback 1.
                bool ok = BackgroundChild::GetOrCreateForCurrentThread(this);
                MOZ_RELEASE_ASSERT(ok);

                BackgroundChildImpl::ThreadLocal* threadLocal =
                  BackgroundChildImpl::GetThreadLocalForCurrentThread();
                MOZ_RELEASE_ASSERT(threadLocal);

                // Callback 2.
                ok = BackgroundChild::GetOrCreateForCurrentThread(this);
                MOZ_RELEASE_ASSERT(ok);
            }

            return NS_OK;
        }

        if (!strcmp(aTopic, "xpcom-shutdown")) {
            MOZ_RELEASE_ASSERT(sCallbackCount == 3);

            return NS_OK;
        }

        MOZ_CRASH("Unknown observer topic!");
    }

public:
    NS_DECL_ISUPPORTS
};

uint32_t BackgroundTester::sCallbackCount = 0;

NS_IMPL_ISUPPORTS(BackgroundTester, nsIIPCBackgroundChildCreateCallback,
                  nsIObserver)

#endif // ENABLE_TESTS

void
MaybeTestPBackground()
{
#ifdef ENABLE_TESTS
    // This test relies on running the event loop and XPCShell does not always
    // do so. Bail out here if we detect that we're running in XPCShell.
    if (PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR")) {
        return;
    }

    // This is called too early at startup to test preferences directly. We have
    // to install an observer to be notified when preferences are available.
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    MOZ_RELEASE_ASSERT(observerService);

    nsCOMPtr<nsIObserver> observer = new BackgroundTester();
    nsresult rv = observerService->AddObserver(observer, "profile-after-change",
                                               false);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
}

// XXX Workaround for bug 986973 to maintain the existing broken semantics
template<>
struct nsIConsoleService::COMTypeInfo<nsConsoleService, void> {
  static const nsIID kIID;
};
const nsIID nsIConsoleService::COMTypeInfo<nsConsoleService, void>::kIID = NS_ICONSOLESERVICE_IID;

namespace mozilla {
namespace dom {

#ifdef MOZ_NUWA_PROCESS
int32_t ContentParent::sNuwaPid = 0;
bool ContentParent::sNuwaReady = false;
#endif

#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"
#define NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC "ipc:network:set-connectivity"

class MemoryReportRequestParent : public PMemoryReportRequestParent
{
public:
    explicit MemoryReportRequestParent(uint32_t aGeneration);

    virtual ~MemoryReportRequestParent();

    virtual void ActorDestroy(ActorDestroyReason aWhy) override;

    virtual bool RecvReport(const MemoryReport& aReport) override;
    virtual bool Recv__delete__() override;

private:
    const uint32_t mGeneration;
    // Non-null if we haven't yet called EndProcessReport() on it.
    nsRefPtr<nsMemoryReporterManager> mReporterManager;

    ContentParent* Owner()
    {
        return static_cast<ContentParent*>(Manager());
    }
};

MemoryReportRequestParent::MemoryReportRequestParent(uint32_t aGeneration)
    : mGeneration(aGeneration)
{
    MOZ_COUNT_CTOR(MemoryReportRequestParent);
    mReporterManager = nsMemoryReporterManager::GetOrCreate();
    NS_WARN_IF(!mReporterManager);
}

bool
MemoryReportRequestParent::RecvReport(const MemoryReport& aReport)
{
    if (mReporterManager) {
        mReporterManager->HandleChildReport(mGeneration, aReport);
    }
    return true;
}

bool
MemoryReportRequestParent::Recv__delete__()
{
    // Notifying the reporter manager is done in ActorDestroy, because
    // it needs to happen even if the child process exits mid-report.
    // (The reporter manager will time out eventually, but let's avoid
    // that if possible.)
    return true;
}

void
MemoryReportRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
    if (mReporterManager) {
        mReporterManager->EndProcessReport(mGeneration, aWhy == Deletion);
        mReporterManager = nullptr;
    }
}

MemoryReportRequestParent::~MemoryReportRequestParent()
{
    MOZ_ASSERT(!mReporterManager);
    MOZ_COUNT_DTOR(MemoryReportRequestParent);
}

// IPC receiver for remote GC/CC logging.
class CycleCollectWithLogsParent final : public PCycleCollectWithLogsParent
{
public:
    ~CycleCollectWithLogsParent()
    {
        MOZ_COUNT_DTOR(CycleCollectWithLogsParent);
    }

    static bool AllocAndSendConstructor(ContentParent* aManager,
                                        bool aDumpAllTraces,
                                        nsICycleCollectorLogSink* aSink,
                                        nsIDumpGCAndCCLogsCallback* aCallback)
    {
        CycleCollectWithLogsParent *actor;
        FILE* gcLog;
        FILE* ccLog;
        nsresult rv;

        actor = new CycleCollectWithLogsParent(aSink, aCallback);
        rv = actor->mSink->Open(&gcLog, &ccLog);
        if (NS_WARN_IF(NS_FAILED(rv))) {
            delete actor;
            return false;
        }

        return aManager->
            SendPCycleCollectWithLogsConstructor(actor,
                                                 aDumpAllTraces,
                                                 FILEToFileDescriptor(gcLog),
                                                 FILEToFileDescriptor(ccLog));
    }

private:
    virtual bool RecvCloseGCLog() override
    {
        unused << mSink->CloseGCLog();
        return true;
    }

    virtual bool RecvCloseCCLog() override
    {
        unused << mSink->CloseCCLog();
        return true;
    }

    virtual bool Recv__delete__() override
    {
        // Report completion to mCallback only on successful
        // completion of the protocol.
        nsCOMPtr<nsIFile> gcLog, ccLog;
        mSink->GetGcLog(getter_AddRefs(gcLog));
        mSink->GetCcLog(getter_AddRefs(ccLog));
        unused << mCallback->OnDump(gcLog, ccLog, /* parent = */ false);
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason aReason) override
    {
        // If the actor is unexpectedly destroyed, we deliberately
        // don't call Close[GC]CLog on the sink, because the logs may
        // be incomplete.  See also the nsCycleCollectorLogSinkToFile
        // implementaiton of those methods, and its destructor.
    }

    CycleCollectWithLogsParent(nsICycleCollectorLogSink *aSink,
                               nsIDumpGCAndCCLogsCallback *aCallback)
        : mSink(aSink), mCallback(aCallback)
    {
        MOZ_COUNT_CTOR(CycleCollectWithLogsParent);
    }

    nsCOMPtr<nsICycleCollectorLogSink> mSink;
    nsCOMPtr<nsIDumpGCAndCCLogsCallback> mCallback;
};

// A memory reporter for ContentParent objects themselves.
class ContentParentsMemoryReporter final : public nsIMemoryReporter
{
    ~ContentParentsMemoryReporter() {}
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORYREPORTER
};

NS_IMPL_ISUPPORTS(ContentParentsMemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
ContentParentsMemoryReporter::CollectReports(nsIMemoryReporterCallback* cb,
                                             nsISupports* aClosure,
                                             bool aAnonymize)
{
    nsAutoTArray<ContentParent*, 16> cps;
    ContentParent::GetAllEvenIfDead(cps);

    for (uint32_t i = 0; i < cps.Length(); i++) {
        ContentParent* cp = cps[i];
        MessageChannel* channel = cp->GetIPCChannel();

        nsString friendlyName;
        cp->FriendlyName(friendlyName, aAnonymize);

        cp->AddRef();
        nsrefcnt refcnt = cp->Release();

        const char* channelStr = "no channel";
        uint32_t numQueuedMessages = 0;
        if (channel) {
            if (channel->Unsound_IsClosed()) {
                channelStr = "closed channel";
            } else {
                channelStr = "open channel";
            }
            numQueuedMessages = channel->Unsound_NumQueuedMessages();
        }

        nsPrintfCString path("queued-ipc-messages/content-parent"
                             "(%s, pid=%d, %s, 0x%p, refcnt=%d)",
                             NS_ConvertUTF16toUTF8(friendlyName).get(),
                             cp->Pid(), channelStr,
                             static_cast<nsIContentParent*>(cp), refcnt);

        NS_NAMED_LITERAL_CSTRING(desc,
            "The number of unset IPC messages held in this ContentParent's "
            "channel.  A large value here might indicate that we're leaking "
            "messages.  Similarly, a ContentParent object for a process that's no "
            "longer running could indicate that we're leaking ContentParents.");

        nsresult rv = cb->Callback(/* process */ EmptyCString(),
                                   path,
                                   KIND_OTHER,
                                   UNITS_COUNT,
                                   numQueuedMessages,
                                   desc,
                                   aClosure);

        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

nsDataHashtable<nsStringHashKey, ContentParent*>* ContentParent::sAppContentParents;
nsTArray<ContentParent*>* ContentParent::sNonAppContentParents;
nsTArray<ContentParent*>* ContentParent::sPrivateContent;
StaticAutoPtr<LinkedList<ContentParent> > ContentParent::sContentParents;

#ifdef MOZ_NUWA_PROCESS
// The pref updates sent to the Nuwa process.
static nsTArray<PrefSetting>* sNuwaPrefUpdates;
#endif

// This is true when subprocess launching is enabled.  This is the
// case between StartUp() and ShutDown() or JoinAllSubprocesses().
static bool sCanLaunchSubprocesses;

// Set to true if the DISABLE_UNSAFE_CPOW_WARNINGS environment variable is
// set.
static bool sDisableUnsafeCPOWWarnings = false;

// The first content child has ID 1, so the chrome process can have ID 0.
static uint64_t gContentChildID = 1;

// We want the prelaunched process to know that it's for apps, but not
// actually for any app in particular.  Use a magic manifest URL.
// Can't be a static constant.
#define MAGIC_PREALLOCATED_APP_MANIFEST_URL NS_LITERAL_STRING("{{template}}")

static const char* sObserverTopics[] = {
    "xpcom-shutdown",
    "profile-before-change",
    NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC,
    NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC,
    "memory-pressure",
    "child-gc-request",
    "child-cc-request",
    "child-mmu-request",
    "last-pb-context-exited",
    "file-watcher-update",
#ifdef MOZ_WIDGET_GONK
    NS_VOLUME_STATE_CHANGED,
    NS_VOLUME_REMOVED,
    "phone-state-changed",
#endif
#ifdef ACCESSIBILITY
    "a11y-init-or-shutdown",
#endif
    "app-theme-changed",
#ifdef MOZ_ENABLE_PROFILER_SPS
    "profiler-started",
    "profiler-stopped",
    "profiler-subprocess-gather",
    "profiler-subprocess",
#endif
};

/* static */ already_AddRefed<ContentParent>
ContentParent::RunNuwaProcess()
{
    MOZ_ASSERT(NS_IsMainThread());
    nsRefPtr<ContentParent> nuwaProcess =
        new ContentParent(/* aApp = */ nullptr,
                          /* aOpener = */ nullptr,
                          /* aIsForBrowser = */ false,
                          /* aIsForPreallocated = */ true,
                          /* aIsNuwaProcess = */ true);

    if (!nuwaProcess->LaunchSubprocess(PROCESS_PRIORITY_BACKGROUND)) {
        return nullptr;
    }

    nuwaProcess->Init();
#ifdef MOZ_NUWA_PROCESS
    sNuwaPid = nuwaProcess->Pid();
    sNuwaReady = false;
#endif
    return nuwaProcess.forget();
}

// PreallocateAppProcess is called by the PreallocatedProcessManager.
// ContentParent then takes this process back within
// GetNewOrPreallocatedAppProcess.
/*static*/ already_AddRefed<ContentParent>
ContentParent::PreallocateAppProcess()
{
    nsRefPtr<ContentParent> process =
        new ContentParent(/* app = */ nullptr,
                          /* aOpener = */ nullptr,
                          /* isForBrowserElement = */ false,
                          /* isForPreallocated = */ true);

    if (!process->LaunchSubprocess(PROCESS_PRIORITY_PREALLOC)) {
        return nullptr;
    }

    process->Init();
    return process.forget();
}

/*static*/ already_AddRefed<ContentParent>
ContentParent::GetNewOrPreallocatedAppProcess(mozIApplication* aApp,
                                              ProcessPriority aInitialPriority,
                                              ContentParent* aOpener,
                                              /*out*/ bool* aTookPreAllocated)
{
    MOZ_ASSERT(aApp);
    nsRefPtr<ContentParent> process = PreallocatedProcessManager::Take();

    if (process) {
        if (!process->SetPriorityAndCheckIsAlive(aInitialPriority)) {
            // Kill the process just in case it's not actually dead; we don't want
            // to "leak" this process!
            process->KillHard("GetNewOrPreallocatedAppProcess");
        }
        else {
            nsAutoString manifestURL;
            if (NS_FAILED(aApp->GetManifestURL(manifestURL))) {
                NS_ERROR("Failed to get manifest URL");
                return nullptr;
            }
            process->TransformPreallocatedIntoApp(aOpener,
                                                  manifestURL);
            process->ForwardKnownInfo();

            if (aTookPreAllocated) {
                *aTookPreAllocated = true;
            }
            return process.forget();
        }
    }

    // XXXkhuey Nuwa wants the frame loader to try again later, but the
    // frame loader is really not set up to do that ...
    NS_WARNING("Unable to use pre-allocated app process");
    process = new ContentParent(aApp,
                                /* aOpener = */ aOpener,
                                /* isForBrowserElement = */ false,
                                /* isForPreallocated = */ false);

    if (!process->LaunchSubprocess(aInitialPriority)) {
        return nullptr;
    }

    process->Init();
    process->ForwardKnownInfo();

    if (aTookPreAllocated) {
        *aTookPreAllocated = false;
    }

    return process.forget();
}

/*static*/ void
ContentParent::StartUp()
{
    // We could launch sub processes from content process
    // FIXME Bug 1023701 - Stop using ContentParent static methods in
    // child process
    sCanLaunchSubprocesses = true;

    if (!XRE_IsParentProcess()) {
        return;
    }

#if defined(MOZ_CONTENT_SANDBOX) && defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 19
    // Require sandboxing on B2G >= KitKat.  This condition must stay
    // in sync with ContentChild::RecvSetProcessSandbox.
    if (!SandboxInfo::Get().CanSandboxContent()) {
        // MOZ_CRASH strings are only for debug builds; make sure the
        // message is clear on non-debug builds as well:
        printf_stderr("Sandboxing support is required on this platform.  "
                      "Recompile kernel with CONFIG_SECCOMP_FILTER=y\n");
        MOZ_CRASH("Sandboxing support is required on this platform.");
    }
#endif

    // Note: This reporter measures all ContentParents.
    RegisterStrongMemoryReporter(new ContentParentsMemoryReporter());

    mozilla::dom::time::InitializeDateCacheCleaner();

    BlobParent::Startup(BlobParent::FriendKey());

    BackgroundChild::Startup();

    // Try to preallocate a process that we can transform into an app later.
    PreallocatedProcessManager::AllocateAfterDelay();

    // Test the PBackground infrastructure on ENABLE_TESTS builds when a special
    // testing preference is set.
    MaybeTestPBackground();

    sDisableUnsafeCPOWWarnings = PR_GetEnv("DISABLE_UNSAFE_CPOW_WARNINGS");
}

/*static*/ void
ContentParent::ShutDown()
{
    // No-op for now.  We rely on normal process shutdown and
    // ClearOnShutdown() to clean up our state.
    sCanLaunchSubprocesses = false;
}

/*static*/ void
ContentParent::JoinProcessesIOThread(const nsTArray<ContentParent*>* aProcesses,
                                     Monitor* aMonitor, bool* aDone)
{
    const nsTArray<ContentParent*>& processes = *aProcesses;
    for (uint32_t i = 0; i < processes.Length(); ++i) {
        if (GeckoChildProcessHost* process = processes[i]->mSubprocess) {
            process->Join();
        }
    }
    {
        MonitorAutoLock lock(*aMonitor);
        *aDone = true;
        lock.Notify();
    }
    // Don't touch any arguments to this function from now on.
}

/*static*/ void
ContentParent::JoinAllSubprocesses()
{
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoTArray<ContentParent*, 8> processes;
    GetAll(processes);
    if (processes.IsEmpty()) {
        printf_stderr("There are no live subprocesses.");
        return;
    }

    printf_stderr("Subprocesses are still alive.  Doing emergency join.\n");

    bool done = false;
    Monitor monitor("mozilla.dom.ContentParent.JoinAllSubprocesses");
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     NewRunnableFunction(
                                         &ContentParent::JoinProcessesIOThread,
                                         &processes, &monitor, &done));
    {
        MonitorAutoLock lock(monitor);
        while (!done) {
            lock.Wait();
        }
    }

    sCanLaunchSubprocesses = false;
}

/*static*/ already_AddRefed<ContentParent>
ContentParent::GetNewOrUsedBrowserProcess(bool aForBrowserElement,
                                          ProcessPriority aPriority,
                                          ContentParent* aOpener)
{
    if (!sNonAppContentParents)
        sNonAppContentParents = new nsTArray<ContentParent*>();

    int32_t maxContentProcesses = Preferences::GetInt("dom.ipc.processCount", 1);
    if (maxContentProcesses < 1)
        maxContentProcesses = 1;

    if (sNonAppContentParents->Length() >= uint32_t(maxContentProcesses)) {
      uint32_t startIdx = rand() % sNonAppContentParents->Length();
      uint32_t currIdx = startIdx;
      do {
        nsRefPtr<ContentParent> p = (*sNonAppContentParents)[currIdx];
        NS_ASSERTION(p->IsAlive(), "Non-alive contentparent in sNonAppContntParents?");
        if (p->mOpener == aOpener) {
          return p.forget();
        }
        currIdx = (currIdx + 1) % sNonAppContentParents->Length();
      } while (currIdx != startIdx);
    }

    // Try to take and transform the preallocated process into browser.
    nsRefPtr<ContentParent> p = PreallocatedProcessManager::Take();
    if (p) {
        p->TransformPreallocatedIntoBrowser(aOpener);
    } else {
      // Failed in using the preallocated process: fork from the chrome process.
        p = new ContentParent(/* app = */ nullptr,
                              aOpener,
                              aForBrowserElement,
                              /* isForPreallocated = */ false);

        if (!p->LaunchSubprocess(aPriority)) {
            return nullptr;
        }

        p->Init();
    }
    p->ForwardKnownInfo();

    sNonAppContentParents->AppendElement(p);
    return p.forget();
}

/*static*/ ProcessPriority
ContentParent::GetInitialProcessPriority(Element* aFrameElement)
{
    // Frames with mozapptype == critical which are expecting a system message
    // get FOREGROUND_HIGH priority.

    if (!aFrameElement) {
        return PROCESS_PRIORITY_FOREGROUND;
    }

    if (aFrameElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::mozapptype,
                                   NS_LITERAL_STRING("inputmethod"), eCaseMatters)) {
        return PROCESS_PRIORITY_FOREGROUND_KEYBOARD;
    } else if (!aFrameElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::mozapptype,
                                           NS_LITERAL_STRING("critical"), eCaseMatters)) {
        return PROCESS_PRIORITY_FOREGROUND;
    }

    nsCOMPtr<nsIMozBrowserFrame> browserFrame =
        do_QueryInterface(aFrameElement);
    if (!browserFrame) {
        return PROCESS_PRIORITY_FOREGROUND;
    }

    return browserFrame->GetIsExpectingSystemMessage() ?
               PROCESS_PRIORITY_FOREGROUND_HIGH :
               PROCESS_PRIORITY_FOREGROUND;
}

#if defined(XP_WIN)
extern const wchar_t* kPluginWidgetContentParentProperty;

/*static*/ void
ContentParent::SendAsyncUpdate(nsIWidget* aWidget)
{
  if (!aWidget || aWidget->Destroyed()) {
    return;
  }
  // Fire off an async request to the plugin to paint its window
  HWND hwnd = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  NS_ASSERTION(hwnd, "Expected valid hwnd value.");
  ContentParent* cp = reinterpret_cast<ContentParent*>(
    ::GetPropW(hwnd, kPluginWidgetContentParentProperty));
  if (cp && !cp->IsDestroyed()) {
    cp->SendUpdateWindow((uintptr_t)hwnd);
  }
}
#endif // defined(XP_WIN)

bool
ContentParent::PreallocatedProcessReady()
{
#ifdef MOZ_NUWA_PROCESS
    return PreallocatedProcessManager::PreallocatedProcessReady();
#else
    return true;
#endif
}

bool
ContentParent::RecvCreateChildProcess(const IPCTabContext& aContext,
                                      const hal::ProcessPriority& aPriority,
                                      const TabId& aOpenerTabId,
                                      ContentParentId* aCpId,
                                      bool* aIsForApp,
                                      bool* aIsForBrowser,
                                      TabId* aTabId)
{
#if 0
    if (!CanOpenBrowser(aContext)) {
        return false;
    }
#endif
    nsRefPtr<ContentParent> cp;
    MaybeInvalidTabContext tc(aContext);
    if (!tc.IsValid()) {
        NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                                 "the child process. (%s)",
                                 tc.GetInvalidReason()).get());
        return false;
    }

    nsCOMPtr<mozIApplication> ownApp = tc.GetTabContext().GetOwnApp();
    if (ownApp) {
        cp = GetNewOrPreallocatedAppProcess(ownApp,
                                            aPriority,
                                            this);
    }
    else {
        cp = GetNewOrUsedBrowserProcess(/* isBrowserElement = */ true,
                                        aPriority,
                                        this);
    }

    if (!cp) {
        *aCpId = 0;
        *aIsForApp = false;
        *aIsForBrowser = false;
        return true;
    }

    *aCpId = cp->ChildID();
    *aIsForApp = cp->IsForApp();
    *aIsForBrowser = cp->IsForBrowser();

    ContentProcessManager *cpm = ContentProcessManager::GetSingleton();
    cpm->AddContentProcess(cp, this->ChildID());

    if (cpm->AddGrandchildProcess(this->ChildID(), cp->ChildID())) {
        // Pre-allocate a TabId here to save one time IPC call at app startup.
        *aTabId = AllocateTabId(aOpenerTabId,
                                aContext,
                                cp->ChildID());
        return (*aTabId != 0);
    }

    return false;
}

bool
ContentParent::RecvBridgeToChildProcess(const ContentParentId& aCpId)
{
    ContentProcessManager *cpm = ContentProcessManager::GetSingleton();
    ContentParent* cp = cpm->GetContentProcessById(aCpId);

    if (cp) {
        ContentParentId parentId;
        if (cpm->GetParentProcessId(cp->ChildID(), &parentId) &&
            parentId == this->ChildID()) {
            return NS_SUCCEEDED(PContentBridge::Bridge(this, cp));
        }
    }

    // You can't bridge to a process you didn't open!
    KillHard("BridgeToChildProcess");
    return false;
}

static nsIDocShell* GetOpenerDocShellHelper(Element* aFrameElement)
{
    // Propagate the private-browsing status of the element's parent
    // docshell to the remote docshell, via the chrome flags.
    nsCOMPtr<Element> frameElement = do_QueryInterface(aFrameElement);
    MOZ_ASSERT(frameElement);
    nsPIDOMWindow* win = frameElement->OwnerDoc()->GetWindow();
    if (!win) {
        NS_WARNING("Remote frame has no window");
        return nullptr;
    }
    nsIDocShell* docShell = win->GetDocShell();
    if (!docShell) {
        NS_WARNING("Remote frame has no docshell");
        return nullptr;
    }

    return docShell;
}

bool
ContentParent::RecvCreateGMPService()
{
    return PGMPService::Open(this);
}

bool
ContentParent::RecvGetGMPPluginVersionForAPI(const nsCString& aAPI,
                                             nsTArray<nsCString>&& aTags,
                                             bool* aHasVersion,
                                             nsCString* aVersion)
{
    return GMPServiceParent::RecvGetGMPPluginVersionForAPI(aAPI, Move(aTags),
                                                           aHasVersion,
                                                           aVersion);
}

bool
ContentParent::RecvLoadPlugin(const uint32_t& aPluginId, nsresult* aRv, uint32_t* aRunID)
{
    *aRv = NS_OK;
    return mozilla::plugins::SetupBridge(aPluginId, this, false, aRv, aRunID);
}

bool
ContentParent::RecvConnectPluginBridge(const uint32_t& aPluginId, nsresult* aRv)
{
    *aRv = NS_OK;
    // We don't need to get the run ID for the plugin, since we already got it
    // in the first call to SetupBridge in RecvLoadPlugin, so we pass in a dummy
    // pointer and just throw it away.
    uint32_t dummy = 0;
    return mozilla::plugins::SetupBridge(aPluginId, this, true, aRv, &dummy);
}

bool
ContentParent::RecvGetBlocklistState(const uint32_t& aPluginId,
                                     uint32_t* aState)
{
    *aState = nsIBlocklistService::STATE_BLOCKED;

    nsRefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
    if (!pluginHost) {
        return false;
    }
    nsPluginTag* tag =  pluginHost->PluginWithId(aPluginId);

    if (!tag) {
        return false;
    }

    return NS_SUCCEEDED(tag->GetBlocklistState(aState));
}

bool
ContentParent::RecvFindPlugins(const uint32_t& aPluginEpoch,
                               nsTArray<PluginTag>* aPlugins,
                               uint32_t* aNewPluginEpoch)
{
    return mozilla::plugins::FindPluginsForContent(aPluginEpoch, aPlugins, aNewPluginEpoch);
}

/*static*/ TabParent*
ContentParent::CreateBrowserOrApp(const TabContext& aContext,
                                  Element* aFrameElement,
                                  ContentParent* aOpenerContentParent)
{
    if (!sCanLaunchSubprocesses) {
        return nullptr;
    }

    if (TabParent* parent = TabParent::GetNextTabParent()) {
        parent->SetOwnerElement(aFrameElement);
        return parent;
    }

    ProcessPriority initialPriority = GetInitialProcessPriority(aFrameElement);
    bool isInContentProcess = !XRE_IsParentProcess();
    TabId tabId;

    nsIDocShell* docShell = GetOpenerDocShellHelper(aFrameElement);
    TabId openerTabId;
    if (docShell) {
        openerTabId = TabParent::GetTabIdFrom(docShell);
    }

    if (aContext.IsBrowserElement() || !aContext.HasOwnApp()) {
        nsRefPtr<TabParent> tp;
        nsRefPtr<nsIContentParent> constructorSender;
        if (isInContentProcess) {
            MOZ_ASSERT(aContext.IsBrowserElement());
            constructorSender = CreateContentBridgeParent(aContext,
                                                          initialPriority,
                                                          openerTabId,
                                                          &tabId);
        } else {
            if (aOpenerContentParent) {
                constructorSender = aOpenerContentParent;
            } else {
                constructorSender =
                    GetNewOrUsedBrowserProcess(aContext.IsBrowserElement(),
                                               initialPriority);
                if (!constructorSender) {
                    return nullptr;
                }
            }
            tabId = AllocateTabId(openerTabId,
                                  aContext.AsIPCTabContext(),
                                  constructorSender->ChildID());
        }
        if (constructorSender) {
            uint32_t chromeFlags = 0;

            nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
            if (loadContext && loadContext->UsePrivateBrowsing()) {
                chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
            }
            bool affectLifetime;
            docShell->GetAffectPrivateSessionLifetime(&affectLifetime);
            if (affectLifetime) {
                chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
            }

            if (tabId == 0) {
                return nullptr;
            }
            nsRefPtr<TabParent> tp(new TabParent(constructorSender, tabId,
                                                 aContext, chromeFlags));
            tp->SetInitedByParent();
            tp->SetOwnerElement(aFrameElement);

            PBrowserParent* browser = constructorSender->SendPBrowserConstructor(
                // DeallocPBrowserParent() releases this ref.
                tp.forget().take(),
                tabId,
                aContext.AsIPCTabContext(),
                chromeFlags,
                constructorSender->ChildID(),
                constructorSender->IsForApp(),
                constructorSender->IsForBrowser());
            return TabParent::GetFrom(browser);
        }
        return nullptr;
    }

    // If we got here, we have an app and we're not a browser element.  ownApp
    // shouldn't be null, because we otherwise would have gone into the
    // !HasOwnApp() branch above.
    nsRefPtr<nsIContentParent> parent;
    bool reused = false;
    bool tookPreallocated = false;
    nsAutoString manifestURL;

    if (isInContentProcess) {
      parent = CreateContentBridgeParent(aContext,
                                         initialPriority,
                                         openerTabId,
                                         &tabId);
    }
    else {
        nsCOMPtr<mozIApplication> ownApp = aContext.GetOwnApp();

        if (!sAppContentParents) {
            sAppContentParents =
                new nsDataHashtable<nsStringHashKey, ContentParent*>();
        }

        // Each app gets its own ContentParent instance unless it shares it with
        // a parent app.
        if (NS_FAILED(ownApp->GetManifestURL(manifestURL))) {
            NS_ERROR("Failed to get manifest URL");
            return nullptr;
        }

        nsRefPtr<ContentParent> p = sAppContentParents->Get(manifestURL);

        if (!p && Preferences::GetBool("dom.ipc.reuse_parent_app")) {
            nsAutoString parentAppManifestURL;
            aFrameElement->GetAttr(kNameSpaceID_None,
                                   nsGkAtoms::parentapp, parentAppManifestURL);
            nsAdoptingString systemAppManifestURL =
                Preferences::GetString("b2g.system_manifest_url");
            nsCOMPtr<nsIAppsService> appsService =
                do_GetService(APPS_SERVICE_CONTRACTID);
            if (!parentAppManifestURL.IsEmpty() &&
                !parentAppManifestURL.Equals(systemAppManifestURL) &&
                appsService) {
                nsCOMPtr<mozIApplication> parentApp;
                nsCOMPtr<mozIApplication> app;
                appsService->GetAppByManifestURL(parentAppManifestURL,
                                                getter_AddRefs(parentApp));
                appsService->GetAppByManifestURL(manifestURL,
                                                getter_AddRefs(app));

                // Only let certified apps re-use the same process.
                unsigned short parentAppStatus = 0;
                unsigned short appStatus = 0;
                if (app &&
                    NS_SUCCEEDED(app->GetAppStatus(&appStatus)) &&
                    appStatus == nsIPrincipal::APP_STATUS_CERTIFIED &&
                    parentApp &&
                    NS_SUCCEEDED(parentApp->GetAppStatus(&parentAppStatus)) &&
                    parentAppStatus == nsIPrincipal::APP_STATUS_CERTIFIED) {
                    // Check if we can re-use the process of the parent app.
                    p = sAppContentParents->Get(parentAppManifestURL);
                }
            }
        }

        if (p) {
            // Check that the process is still alive and set its priority.
            // Hopefully the process won't die after this point, if this call
            // succeeds.
            if (!p->SetPriorityAndCheckIsAlive(initialPriority)) {
                p = nullptr;
            }
        }

        reused = !!p;
        if (!p) {
            p = GetNewOrPreallocatedAppProcess(ownApp,
                                               initialPriority,
                                               nullptr,
                                               &tookPreallocated);
            MOZ_ASSERT(p);
            sAppContentParents->Put(manifestURL, p);
        }
        tabId = AllocateTabId(openerTabId,
                              aContext.AsIPCTabContext(),
                              p->ChildID());
        parent = static_cast<nsIContentParent*>(p);
    }

    if (!parent || (tabId == 0)) {
        return nullptr;
    }

    uint32_t chromeFlags = 0;

    nsRefPtr<TabParent> tp = new TabParent(parent, tabId, aContext, chromeFlags);
    tp->SetInitedByParent();
    tp->SetOwnerElement(aFrameElement);
    PBrowserParent* browser = parent->SendPBrowserConstructor(
        // DeallocPBrowserParent() releases this ref.
        nsRefPtr<TabParent>(tp).forget().take(),
        tabId,
        aContext.AsIPCTabContext(),
        chromeFlags,
        parent->ChildID(),
        parent->IsForApp(),
        parent->IsForBrowser());

    if (isInContentProcess) {
        // Just return directly without the following check in content process.
        return TabParent::GetFrom(browser);
    }

    if (!browser) {
        // We failed to actually start the PBrowser.  This can happen if the
        // other process has already died.
        if (!reused) {
            // Don't leave a broken ContentParent in the hashtable.
            parent->AsContentParent()->KillHard("CreateBrowserOrApp");
            sAppContentParents->Remove(manifestURL);
            parent = nullptr;
        }

        // If we took the preallocated process and it was already dead, try
        // again with a non-preallocated process.  We can be sure this won't
        // loop forever, because the next time through there will be no
        // preallocated process to take.
        if (tookPreallocated) {
          return ContentParent::CreateBrowserOrApp(aContext,
                                                   aFrameElement,
                                                   aOpenerContentParent);
        }

        // Otherwise just give up.
        return nullptr;
    }

    parent->AsContentParent()->MaybeTakeCPUWakeLock(aFrameElement);

    return TabParent::GetFrom(browser);
}

/*static*/ ContentBridgeParent*
ContentParent::CreateContentBridgeParent(const TabContext& aContext,
                                         const hal::ProcessPriority& aPriority,
                                         const TabId& aOpenerTabId,
                                         /*out*/ TabId* aTabId)
{
    MOZ_ASSERT(aTabId);

    ContentChild* child = ContentChild::GetSingleton();
    ContentParentId cpId;
    bool isForApp;
    bool isForBrowser;
    if (!child->SendCreateChildProcess(aContext.AsIPCTabContext(),
                                       aPriority,
                                       aOpenerTabId,
                                       &cpId,
                                       &isForApp,
                                       &isForBrowser,
                                       aTabId)) {
        return nullptr;
    }
    if (cpId == 0) {
        return nullptr;
    }
    if (!child->SendBridgeToChildProcess(cpId)) {
        return nullptr;
    }
    ContentBridgeParent* parent = child->GetLastBridge();
    parent->SetChildID(cpId);
    parent->SetIsForApp(isForApp);
    parent->SetIsForBrowser(isForBrowser);
    return parent;
}

void
ContentParent::GetAll(nsTArray<ContentParent*>& aArray)
{
    aArray.Clear();

    if (!sContentParents) {
        return;
    }

    for (ContentParent* cp = sContentParents->getFirst(); cp;
         cp = cp->LinkedListElement<ContentParent>::getNext()) {
        if (cp->mIsAlive) {
            aArray.AppendElement(cp);
        }
    }
}

void
ContentParent::GetAllEvenIfDead(nsTArray<ContentParent*>& aArray)
{
    aArray.Clear();

    if (!sContentParents) {
        return;
    }

    for (ContentParent* cp = sContentParents->getFirst(); cp;
         cp = cp->LinkedListElement<ContentParent>::getNext()) {
        aArray.AppendElement(cp);
    }
}

void
ContentParent::Init()
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        size_t length = ArrayLength(sObserverTopics);
        for (size_t i = 0; i < length; ++i) {
            obs->AddObserver(this, sObserverTopics[i], false);
        }
    }
    Preferences::AddStrongObserver(this, "");
    if (obs) {
        obs->NotifyObservers(static_cast<nsIObserver*>(this), "ipc:content-created", nullptr);
    }

#ifdef ACCESSIBILITY
    // If accessibility is running in chrome process then start it in content
    // process.
    if (nsIPresShell::IsAccessibilityActive()) {
        unused << SendActivateA11y();
    }
#endif

    DebugOnly<FileUpdateDispatcher*> observer = FileUpdateDispatcher::GetSingleton();
    NS_ASSERTION(observer, "FileUpdateDispatcher is null");
}

void
ContentParent::ForwardKnownInfo()
{
    MOZ_ASSERT(mMetamorphosed);
    if (!mMetamorphosed) {
        return;
    }
#ifdef MOZ_WIDGET_GONK
    InfallibleTArray<VolumeInfo> volumeInfo;
    nsRefPtr<nsVolumeService> vs = nsVolumeService::GetSingleton();
    if (vs && !mIsForBrowser) {
        vs->GetVolumesForIPC(&volumeInfo);
        unused << SendVolumes(volumeInfo);
    }
#endif /* MOZ_WIDGET_GONK */

    nsCOMPtr<nsISystemMessagesInternal> systemMessenger =
        do_GetService("@mozilla.org/system-message-internal;1");
    if (systemMessenger && !mIsForBrowser) {
        nsCOMPtr<nsIURI> manifestURI;
        nsresult rv = NS_NewURI(getter_AddRefs(manifestURI), mAppManifestURL);
        if (NS_SUCCEEDED(rv)) {
            systemMessenger->RefreshCache(mMessageManager, manifestURI);
        }
    }
}

namespace {

class SystemMessageHandledListener final
    : public nsITimerCallback
    , public LinkedListElement<SystemMessageHandledListener>
{
public:
    NS_DECL_ISUPPORTS

    SystemMessageHandledListener() {}

    static void OnSystemMessageHandled()
    {
        if (!sListeners) {
            return;
        }

        SystemMessageHandledListener* listener = sListeners->popFirst();
        if (!listener) {
            return;
        }

        // Careful: ShutDown() may delete |this|.
        listener->ShutDown();
    }

    void Init(WakeLock* aWakeLock)
    {
        MOZ_ASSERT(!mWakeLock);
        MOZ_ASSERT(!mTimer);

        // mTimer keeps a strong reference to |this|.  When this object's
        // destructor runs, it will remove itself from the LinkedList.

        if (!sListeners) {
            sListeners = new LinkedList<SystemMessageHandledListener>();
            ClearOnShutdown(&sListeners);
        }
        sListeners->insertBack(this);

        mWakeLock = aWakeLock;

        mTimer = do_CreateInstance("@mozilla.org/timer;1");

        uint32_t timeoutSec =
            Preferences::GetInt("dom.ipc.systemMessageCPULockTimeoutSec", 30);
        mTimer->InitWithCallback(this, timeoutSec * 1000,
                                 nsITimer::TYPE_ONE_SHOT);
    }

    NS_IMETHOD Notify(nsITimer* aTimer) override
    {
        // Careful: ShutDown() may delete |this|.
        ShutDown();
        return NS_OK;
    }

private:
    ~SystemMessageHandledListener() {}

    static StaticAutoPtr<LinkedList<SystemMessageHandledListener> > sListeners;

    void ShutDown()
    {
        nsRefPtr<SystemMessageHandledListener> kungFuDeathGrip = this;

        ErrorResult rv;
        mWakeLock->Unlock(rv);

        if (mTimer) {
            mTimer->Cancel();
            mTimer = nullptr;
        }
    }

    nsRefPtr<WakeLock> mWakeLock;
    nsCOMPtr<nsITimer> mTimer;
};

StaticAutoPtr<LinkedList<SystemMessageHandledListener> >
    SystemMessageHandledListener::sListeners;

NS_IMPL_ISUPPORTS(SystemMessageHandledListener,
                  nsITimerCallback)

} // namespace

void
ContentParent::MaybeTakeCPUWakeLock(Element* aFrameElement)
{
    // Take the CPU wake lock on behalf of this processs if it's expecting a
    // system message.  We'll release the CPU lock once the message is
    // delivered, or after some period of time, which ever comes first.

    nsCOMPtr<nsIMozBrowserFrame> browserFrame =
        do_QueryInterface(aFrameElement);
    if (!browserFrame ||
        !browserFrame->GetIsExpectingSystemMessage()) {
        return;
    }

    nsRefPtr<PowerManagerService> pms = PowerManagerService::GetInstance();
    nsRefPtr<WakeLock> lock =
        pms->NewWakeLockOnBehalfOfProcess(NS_LITERAL_STRING("cpu"), this);

    // This object's Init() function keeps it alive.
    nsRefPtr<SystemMessageHandledListener> listener =
        new SystemMessageHandledListener();
    listener->Init(lock);
}

bool
ContentParent::SetPriorityAndCheckIsAlive(ProcessPriority aPriority)
{
    ProcessPriorityManager::SetProcessPriority(this, aPriority);

    // Now that we've set this process's priority, check whether the process is
    // still alive.  Hopefully we've set the priority to FOREGROUND*, so the
    // process won't unexpectedly crash after this point!
    //
    // Bug 943174: use waitid() with WNOWAIT so that, if the process
    // did exit, we won't consume its zombie and confuse the
    // GeckoChildProcessHost dtor.  Also, if the process isn't a
    // direct child because of Nuwa this will fail with ECHILD, and we
    // need to assume the child is alive in that case rather than
    // assuming it's dead (as is otherwise a reasonable fallback).
#ifdef MOZ_WIDGET_GONK
    siginfo_t info;
    info.si_pid = 0;
    if (waitid(P_PID, Pid(), &info, WNOWAIT | WNOHANG | WEXITED) == 0
        && info.si_pid != 0) {
        return false;
    }
#endif

    return true;
}

// Helper for ContentParent::TransformPreallocatedIntoApp.
static void
TryGetNameFromManifestURL(const nsAString& aManifestURL,
                          nsAString& aName)
{
    aName.Truncate();
    if (aManifestURL.IsEmpty() ||
        aManifestURL == MAGIC_PREALLOCATED_APP_MANIFEST_URL) {
        return;
    }

    nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE_VOID(appsService);

    nsCOMPtr<mozIApplication> app;
    appsService->GetAppByManifestURL(aManifestURL, getter_AddRefs(app));

    if (!app) {
        return;
    }

    app->GetName(aName);
}

void
ContentParent::TransformPreallocatedIntoApp(ContentParent* aOpener,
                                            const nsAString& aAppManifestURL)
{
    MOZ_ASSERT(IsPreallocated());
    mMetamorphosed = true;
    mOpener = aOpener;
    mAppManifestURL = aAppManifestURL;
    TryGetNameFromManifestURL(aAppManifestURL, mAppName);
}

void
ContentParent::TransformPreallocatedIntoBrowser(ContentParent* aOpener)
{
    // Reset mAppManifestURL, mIsForBrowser and mOSPrivileges for browser.
    mMetamorphosed = true;
    mOpener = aOpener;
    mAppManifestURL.Truncate();
    mIsForBrowser = true;
}

void
ContentParent::ShutDownProcess(ShutDownMethod aMethod)
{
#ifdef MOZ_NUWA_PROCESS
    if (aMethod == SEND_SHUTDOWN_MESSAGE && IsNuwaProcess()) {
        // We shouldn't send shutdown messages to frozen Nuwa processes,
        // so just close the channel.
        aMethod = CLOSE_CHANNEL;
    }
#endif

    // Shutting down by sending a shutdown message works differently than the
    // other methods. We first call Shutdown() in the child. After the child is
    // ready, it calls FinishShutdown() on us. Then we close the channel.
    if (aMethod == SEND_SHUTDOWN_MESSAGE) {
        if (mIPCOpen && !mShutdownPending && SendShutdown()) {
            mShutdownPending = true;
            // Start the force-kill timer if we haven't already.
            StartForceKillTimer();
        }

        // If call was not successful, the channel must have been broken
        // somehow, and we will clean up the error in ActorDestroy.
        return;
    }

    using mozilla::dom::quota::QuotaManager;

    if (QuotaManager* quotaManager = QuotaManager::Get()) {
        quotaManager->AbortOperationsForProcess(mChildID);
    }

    // If Close() fails with an error, we'll end up back in this function, but
    // with aMethod = CLOSE_CHANNEL_WITH_ERROR.  It's important that we call
    // CloseWithError() in this case; see bug 895204.

    if (aMethod == CLOSE_CHANNEL && !mCalledClose) {
        // Close() can only be called once: It kicks off the destruction
        // sequence.
        mCalledClose = true;
        Close();
#ifdef MOZ_NUWA_PROCESS
        // Kill Nuwa process forcibly to break its IPC channels and finalize
        // corresponding parents.
        if (IsNuwaProcess()) {
            KillHard("ShutDownProcess");
        }
#endif
    }

    if (aMethod == CLOSE_CHANNEL_WITH_ERROR && !mCalledCloseWithError) {
        MessageChannel* channel = GetIPCChannel();
        if (channel) {
            mCalledCloseWithError = true;
            channel->CloseWithError();
        }
    }

    const InfallibleTArray<POfflineCacheUpdateParent*>& ocuParents =
        ManagedPOfflineCacheUpdateParent();
    for (uint32_t i = 0; i < ocuParents.Length(); ++i) {
        nsRefPtr<mozilla::docshell::OfflineCacheUpdateParent> ocuParent =
            static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(ocuParents[i]);
        ocuParent->StopSendingMessagesToChild();
    }

    // NB: must MarkAsDead() here so that this isn't accidentally
    // returned from Get*() while in the midst of shutdown.
    MarkAsDead();

    // A ContentParent object might not get freed until after XPCOM shutdown has
    // shut down the cycle collector.  But by then it's too late to release any
    // CC'ed objects, so we need to null them out here, while we still can.  See
    // bug 899761.
    ShutDownMessageManager();
}

bool
ContentParent::RecvFinishShutdown()
{
    // At this point, we already called ShutDownProcess once with
    // SEND_SHUTDOWN_MESSAGE. To actually close the channel, we call
    // ShutDownProcess again with CLOSE_CHANNEL.
    MOZ_ASSERT(mShutdownPending);
    ShutDownProcess(CLOSE_CHANNEL);
    return true;
}

void
ContentParent::ShutDownMessageManager()
{
  if (!mMessageManager) {
    return;
  }

  mMessageManager->ReceiveMessage(
            static_cast<nsIContentFrameMessageManager*>(mMessageManager.get()), nullptr,
            CHILD_PROCESS_SHUTDOWN_MESSAGE, false,
            nullptr, nullptr, nullptr, nullptr);

  mMessageManager->Disconnect();
  mMessageManager = nullptr;
}

void
ContentParent::MarkAsDead()
{
    if (!mAppManifestURL.IsEmpty()) {
        if (sAppContentParents) {
            sAppContentParents->Remove(mAppManifestURL);
            if (!sAppContentParents->Count()) {
                delete sAppContentParents;
                sAppContentParents = nullptr;
            }
        }
    } else if (sNonAppContentParents) {
        sNonAppContentParents->RemoveElement(this);
        if (!sNonAppContentParents->Length()) {
            delete sNonAppContentParents;
            sNonAppContentParents = nullptr;
        }
    }

    if (sPrivateContent) {
        sPrivateContent->RemoveElement(this);
        if (!sPrivateContent->Length()) {
            delete sPrivateContent;
            sPrivateContent = nullptr;
        }
    }

    mIsAlive = false;
}

void
ContentParent::OnChannelError()
{
    nsRefPtr<ContentParent> content(this);
#ifdef MOZ_NUWA_PROCESS
    // Handle app or Nuwa process exit before normal channel error handling.
    PreallocatedProcessManager::MaybeForgetSpare(this);
#endif
    PContentParent::OnChannelError();
}

void
ContentParent::OnBeginSyncTransaction() {
    if (XRE_IsParentProcess()) {
        nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
        JSContext *cx = nsContentUtils::GetCurrentJSContext();
        if (!sDisableUnsafeCPOWWarnings) {
            if (console && cx) {
                nsAutoString filename;
                uint32_t lineno = 0;
                nsJSUtils::GetCallingLocation(cx, filename, &lineno);
                nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
                error->Init(NS_LITERAL_STRING("unsafe CPOW usage"), filename, EmptyString(),
                            lineno, 0, nsIScriptError::warningFlag, "chrome javascript");
                console->LogMessage(error);
            } else {
                NS_WARNING("Unsafe synchronous IPC message");
            }
        }
    }
}

void
ContentParent::OnChannelConnected(int32_t pid)
{
    SetOtherProcessId(pid);

#if defined(ANDROID) || defined(LINUX)
    // Check nice preference
    int32_t nice = Preferences::GetInt("dom.ipc.content.nice", 0);

    // Environment variable overrides preference
    char* relativeNicenessStr = getenv("MOZ_CHILD_PROCESS_RELATIVE_NICENESS");
    if (relativeNicenessStr) {
        nice = atoi(relativeNicenessStr);
    }

    /* make the GUI thread have higher priority on single-cpu devices */
    nsCOMPtr<nsIPropertyBag2> infoService = do_GetService(NS_SYSTEMINFO_CONTRACTID);
    if (infoService) {
        int32_t cpus;
        nsresult rv = infoService->GetPropertyAsInt32(NS_LITERAL_STRING("cpucount"), &cpus);
        if (NS_FAILED(rv)) {
            cpus = 1;
        }
        if (nice != 0 && cpus == 1) {
            setpriority(PRIO_PROCESS, pid, getpriority(PRIO_PROCESS, pid) + nice);
        }
    }
#endif
}

void
ContentParent::ProcessingError(Result aCode, const char* aReason)
{
    if (MsgDropped == aCode) {
        return;
    }
    // Other errors are big deals.
    KillHard(aReason);
}

typedef std::pair<ContentParent*, std::set<uint64_t> > IDPair;

namespace {
std::map<ContentParent*, std::set<uint64_t> >&
NestedBrowserLayerIds()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<ContentParent*, std::set<uint64_t> > sNestedBrowserIds;
  return sNestedBrowserIds;
}
} // namespace

bool
ContentParent::RecvAllocateLayerTreeId(uint64_t* aId)
{
    *aId = CompositorParent::AllocateLayerTreeId();

    auto iter = NestedBrowserLayerIds().find(this);
    if (iter == NestedBrowserLayerIds().end()) {
        std::set<uint64_t> ids;
        ids.insert(*aId);
        NestedBrowserLayerIds().insert(IDPair(this, ids));
    } else {
        iter->second.insert(*aId);
    }
    return true;
}

bool
ContentParent::RecvDeallocateLayerTreeId(const uint64_t& aId)
{
    auto iter = NestedBrowserLayerIds().find(this);
    if (iter != NestedBrowserLayerIds().end() &&
        iter->second.find(aId) != iter->second.end()) {
        CompositorParent::DeallocateLayerTreeId(aId);
    } else {
        // You can't deallocate layer tree ids that you didn't allocate
        KillHard("DeallocateLayerTreeId");
    }
    return true;
}

namespace {

void
DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess)
{
    XRE_GetIOMessageLoop()
        ->PostTask(FROM_HERE,
                   new DeleteTask<GeckoChildProcessHost>(aSubprocess));
}

// This runnable only exists to delegate ownership of the
// ContentParent to this runnable, until it's deleted by the event
// system.
struct DelayedDeleteContentParentTask : public nsRunnable
{
    explicit DelayedDeleteContentParentTask(ContentParent* aObj) : mObj(aObj) { }

    // No-op
    NS_IMETHODIMP Run() { return NS_OK; }

    nsRefPtr<ContentParent> mObj;
};

} // namespace

void
ContentParent::ActorDestroy(ActorDestroyReason why)
{
    if (mForceKillTimer) {
        mForceKillTimer->Cancel();
        mForceKillTimer = nullptr;
    }

    // Signal shutdown completion regardless of error state, so we can
    // finish waiting in the xpcom-shutdown/profile-before-change observer.
    mIPCOpen = false;

    if (mHangMonitorActor) {
        ProcessHangMonitor::RemoveProcess(mHangMonitorActor);
        mHangMonitorActor = nullptr;
    }

    if (why == NormalShutdown && !mCalledClose) {
        // If we shut down normally but haven't called Close, assume somebody
        // else called Close on us. In that case, we still need to call
        // ShutDownProcess below to perform other necessary clean up.
        mCalledClose = true;
    }

    // Make sure we always clean up.
    ShutDownProcess(why == NormalShutdown ? CLOSE_CHANNEL
                                          : CLOSE_CHANNEL_WITH_ERROR);

    nsRefPtr<ContentParent> kungFuDeathGrip(this);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        size_t length = ArrayLength(sObserverTopics);
        for (size_t i = 0; i < length; ++i) {
            obs->RemoveObserver(static_cast<nsIObserver*>(this),
                                sObserverTopics[i]);
        }
    }

    // remove the global remote preferences observers
    Preferences::RemoveObserver(this, "");

#ifdef MOZ_NUWA_PROCESS
    // Remove the pref update requests.
    if (IsNuwaProcess() && sNuwaPrefUpdates) {
        delete sNuwaPrefUpdates;
        sNuwaPrefUpdates = nullptr;
    }
#endif

    RecvRemoveGeolocationListener();

    mConsoleService = nullptr;

    if (obs) {
        nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

        props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), mChildID);

        if (AbnormalShutdown == why) {
            Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT,
                                  NS_LITERAL_CSTRING("content"), 1);

            props->SetPropertyAsBool(NS_LITERAL_STRING("abnormal"), true);

#ifdef MOZ_CRASHREPORTER
            // There's a window in which child processes can crash
            // after IPC is established, but before a crash reporter
            // is created.
            if (ManagedPCrashReporterParent().Length() > 0) {
                CrashReporterParent* crashReporter =
                    static_cast<CrashReporterParent*>(ManagedPCrashReporterParent()[0]);

                // If we're an app process, always stomp the latest URI
                // loaded in the child process with our manifest URL.  We
                // would rather associate the crashes with apps than
                // random child windows loaded in them.
                //
                // XXX would be nice if we could get both ...
                if (!mAppManifestURL.IsEmpty()) {
                    crashReporter->AnnotateCrashReport(NS_LITERAL_CSTRING("URL"),
                                                       NS_ConvertUTF16toUTF8(mAppManifestURL));
                }

                // if mCreatedPairedMinidumps is true, we've already generated
                // parent/child dumps for dekstop crashes.
                if (!mCreatedPairedMinidumps) {
                    crashReporter->GenerateCrashReport(this, nullptr);
                }

                nsAutoString dumpID(crashReporter->ChildDumpID());
                props->SetPropertyAsAString(NS_LITERAL_STRING("dumpID"), dumpID);
            }
#endif
        }
        obs->NotifyObservers((nsIPropertyBag2*) props, "ipc:content-shutdown", nullptr);
    }

    // Remove any and all idle listeners.
    nsCOMPtr<nsIIdleService> idleService =
        do_GetService("@mozilla.org/widget/idleservice;1");
    MOZ_ASSERT(idleService);
    nsRefPtr<ParentIdleListener> listener;
    for (int32_t i = mIdleListeners.Length() - 1; i >= 0; --i) {
        listener = static_cast<ParentIdleListener*>(mIdleListeners[i].get());
        idleService->RemoveIdleObserver(listener, listener->mTime);
    }
    mIdleListeners.Clear();

    MessageLoop::current()->
        PostTask(FROM_HERE,
                 NewRunnableFunction(DelayedDeleteSubprocess, mSubprocess));
    mSubprocess = nullptr;

    // IPDL rules require actors to live on past ActorDestroy, but it
    // may be that the kungFuDeathGrip above is the last reference to
    // |this|.  If so, when we go out of scope here, we're deleted and
    // all hell breaks loose.
    //
    // This runnable ensures that a reference to |this| lives on at
    // least until after the current task finishes running.
    NS_DispatchToCurrentThread(new DelayedDeleteContentParentTask(this));

    // Destroy any processes created by this ContentParent
    ContentProcessManager *cpm = ContentProcessManager::GetSingleton();
    nsTArray<ContentParentId> childIDArray =
        cpm->GetAllChildProcessById(this->ChildID());
    for(uint32_t i = 0; i < childIDArray.Length(); i++) {
        ContentParent* cp = cpm->GetContentProcessById(childIDArray[i]);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            NewRunnableMethod(cp, &ContentParent::ShutDownProcess,
                              SEND_SHUTDOWN_MESSAGE));
    }
    cpm->RemoveContentProcess(this->ChildID());
}

void
ContentParent::NotifyTabDestroying(PBrowserParent* aTab)
{
    // There can be more than one PBrowser for a given app process
    // because of popup windows.  PBrowsers can also destroy
    // concurrently.  When all the PBrowsers are destroying, kick off
    // another task to ensure the child process *really* shuts down,
    // even if the PBrowsers themselves never finish destroying.
    int32_t numLiveTabs = ManagedPBrowserParent().Length();
    ++mNumDestroyingTabs;
    if (mNumDestroyingTabs != numLiveTabs) {
        return;
    }

    // We're dying now, so prevent this content process from being
    // recycled during its shutdown procedure.
    MarkAsDead();
    StartForceKillTimer();
}

void
ContentParent::StartForceKillTimer()
{
    if (mForceKillTimer || !mIPCOpen) {
        return;
    }

    int32_t timeoutSecs = Preferences::GetInt("dom.ipc.tabs.shutdownTimeoutSecs", 0);
    if (timeoutSecs > 0) {
        mForceKillTimer = do_CreateInstance("@mozilla.org/timer;1");
        MOZ_ASSERT(mForceKillTimer);
        mForceKillTimer->InitWithFuncCallback(ContentParent::ForceKillTimerCallback,
                                              this,
                                              timeoutSecs * 1000,
                                              nsITimer::TYPE_ONE_SHOT);
    }
}

void
ContentParent::NotifyTabDestroyed(PBrowserParent* aTab,
                                  bool aNotifiedDestroying)
{
    if (aNotifiedDestroying) {
        --mNumDestroyingTabs;
    }

    TabId id = static_cast<TabParent*>(aTab)->GetTabId();
    nsTArray<PContentPermissionRequestParent*> parentArray =
        nsContentPermissionUtils::GetContentPermissionRequestParentById(id);

    // Need to close undeleted ContentPermissionRequestParents before tab is closed.
    for (auto& permissionRequestParent : parentArray) {
        nsTArray<PermissionChoice> emptyChoices;
        unused << PContentPermissionRequestParent::Send__delete__(permissionRequestParent,
                                                                  false,
                                                                  emptyChoices);
    }

    // There can be more than one PBrowser for a given app process
    // because of popup windows.  When the last one closes, shut
    // us down.
    if (ManagedPBrowserParent().Length() == 1) {
        // In the case of normal shutdown, send a shutdown message to child to
        // allow it to perform shutdown tasks.
        MessageLoop::current()->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &ContentParent::ShutDownProcess,
                              SEND_SHUTDOWN_MESSAGE));
    }
}

jsipc::CPOWManager*
ContentParent::GetCPOWManager()
{
    if (ManagedPJavaScriptParent().Length()) {
        return CPOWManagerFor(ManagedPJavaScriptParent()[0]);
    }
    return nullptr;
}

TestShellParent*
ContentParent::CreateTestShell()
{
  return static_cast<TestShellParent*>(SendPTestShellConstructor());
}

bool
ContentParent::DestroyTestShell(TestShellParent* aTestShell)
{
    return PTestShellParent::Send__delete__(aTestShell);
}

TestShellParent*
ContentParent::GetTestShellSingleton()
{
    if (!ManagedPTestShellParent().Length())
        return nullptr;
    return static_cast<TestShellParent*>(ManagedPTestShellParent()[0]);
}

void
ContentParent::InitializeMembers()
{
    mSubprocess = nullptr;
    mChildID = gContentChildID++;
    mGeolocationWatchID = -1;
    mNumDestroyingTabs = 0;
    mIsAlive = true;
    mMetamorphosed = false;
    mSendPermissionUpdates = false;
    mSendDataStoreInfos = false;
    mCalledClose = false;
    mCalledCloseWithError = false;
    mCalledKillHard = false;
    mCreatedPairedMinidumps = false;
    mShutdownPending = false;
    mIPCOpen = true;
    mHangMonitorActor = nullptr;
}

bool
ContentParent::LaunchSubprocess(ProcessPriority aInitialPriority /* = PROCESS_PRIORITY_FOREGROUND */)
{
    std::vector<std::string> extraArgs;
    if (mIsNuwaProcess) {
        extraArgs.push_back("-nuwa");
    }

    if (!mSubprocess->LaunchAndWaitForProcessHandle(extraArgs)) {
        MarkAsDead();
        return false;
    }

    Open(mSubprocess->GetChannel(),
         base::GetProcId(mSubprocess->GetChildProcessHandle()));

    InitInternal(aInitialPriority,
                 true, /* Setup off-main thread compositing */
                 true  /* Send registered chrome */);

    ContentProcessManager::GetSingleton()->AddContentProcess(this);

    ProcessHangMonitor::AddProcess(this);

    // Set a reply timeout for CPOWs.
    SetReplyTimeoutMs(Preferences::GetInt("dom.ipc.cpow.timeout", 0));

    return true;
}

ContentParent::ContentParent(mozIApplication* aApp,
                             ContentParent* aOpener,
                             bool aIsForBrowser,
                             bool aIsForPreallocated,
                             bool aIsNuwaProcess /* = false */)
    : nsIContentParent()
    , mOpener(aOpener)
    , mIsForBrowser(aIsForBrowser)
    , mIsNuwaProcess(aIsNuwaProcess)
    , mHasGamepadListener(false)
{
    InitializeMembers();  // Perform common initialization.

    // No more than one of !!aApp, aIsForBrowser, aIsForPreallocated should be
    // true.
    MOZ_ASSERT(!!aApp + aIsForBrowser + aIsForPreallocated <= 1);

    // Only the preallocated process uses Nuwa.
    MOZ_ASSERT_IF(aIsNuwaProcess, aIsForPreallocated);

    if (!aIsNuwaProcess && !aIsForPreallocated) {
        mMetamorphosed = true;
    }

    // Insert ourselves into the global linked list of ContentParent objects.
    if (!sContentParents) {
        sContentParents = new LinkedList<ContentParent>();
    }
    if (!aIsNuwaProcess) {
        sContentParents->insertBack(this);
    }

    if (aApp) {
        aApp->GetManifestURL(mAppManifestURL);
        aApp->GetName(mAppName);
    } else if (aIsForPreallocated) {
        mAppManifestURL = MAGIC_PREALLOCATED_APP_MANIFEST_URL;
    }

    // From this point on, NS_WARNING, NS_ASSERTION, etc. should print out the
    // PID along with the warning.
    nsDebugImpl::SetMultiprocessMode("Parent");

#if defined(XP_WIN) && !defined(MOZ_B2G)
    // Request Windows message deferral behavior on our side of the PContent
    // channel. Generally only applies to the situation where we get caught in
    // a deadlock with the plugin process when sending CPOWs.
    GetIPCChannel()->SetChannelFlags(MessageChannel::REQUIRE_DEFERRED_MESSAGE_PROTECTION);
#endif

    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    ChildPrivileges privs = aIsNuwaProcess
        ? base::PRIVILEGES_INHERIT
        : base::PRIVILEGES_DEFAULT;
    mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, privs);

    IToplevelProtocol::SetTransport(mSubprocess->GetChannel());
}

#ifdef MOZ_NUWA_PROCESS
static const mozilla::ipc::FileDescriptor*
FindFdProtocolFdMapping(const nsTArray<ProtocolFdMapping>& aFds,
                        ProtocolId aProtoId)
{
    for (unsigned int i = 0; i < aFds.Length(); i++) {
        if (aFds[i].protocolId() == aProtoId) {
            return &aFds[i].fd();
        }
    }
    return nullptr;
}

/**
 * This constructor is used for new content process cloned from a template.
 *
 * For Nuwa.
 */
ContentParent::ContentParent(ContentParent* aTemplate,
                             const nsAString& aAppManifestURL,
                             base::ProcessHandle aPid,
                             InfallibleTArray<ProtocolFdMapping>&& aFds)
    : mAppManifestURL(aAppManifestURL)
    , mIsForBrowser(false)
    , mIsNuwaProcess(false)
{
    InitializeMembers();  // Perform common initialization.

    sContentParents->insertBack(this);

    // From this point on, NS_WARNING, NS_ASSERTION, etc. should print out the
    // PID along with the warning.
    nsDebugImpl::SetMultiprocessMode("Parent");

    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    const FileDescriptor* fd = FindFdProtocolFdMapping(aFds, GetProtocolId());

    NS_ASSERTION(fd != nullptr, "IPC Channel for PContent is necessary!");
    mSubprocess = new GeckoExistingProcessHost(GeckoProcessType_Content,
                                               aPid,
                                               *fd);

    mSubprocess->LaunchAndWaitForProcessHandle();

    // Clone actors routed by aTemplate for this instance.
    IToplevelProtocol::SetTransport(mSubprocess->GetChannel());
    ProtocolCloneContext cloneContext;
    cloneContext.SetContentParent(this);
    CloneManagees(aTemplate, &cloneContext);
    CloneOpenedToplevels(aTemplate, aFds, aPid, &cloneContext);

    Open(mSubprocess->GetChannel(),
         base::GetProcId(mSubprocess->GetChildProcessHandle()));

    // Set the subprocess's priority (bg if we're a preallocated process, fg
    // otherwise).  We do this first because we're likely /lowering/ its CPU and
    // memory priority, which it has inherited from this process.
    ProcessPriority priority;
    if (IsPreallocated()) {
        priority = PROCESS_PRIORITY_PREALLOC;
    } else {
        priority = PROCESS_PRIORITY_FOREGROUND;
    }

    InitInternal(priority,
                 false, /* Setup Off-main thread compositing */
                 false  /* Send registered chrome */);

    ContentProcessManager::GetSingleton()->AddContentProcess(this);
}
#endif  // MOZ_NUWA_PROCESS

ContentParent::~ContentParent()
{
    if (mForceKillTimer) {
        mForceKillTimer->Cancel();
    }

    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    // We should be removed from all these lists in ActorDestroy.
    MOZ_ASSERT(!sPrivateContent || !sPrivateContent->Contains(this));
    if (mAppManifestURL.IsEmpty()) {
        MOZ_ASSERT(!sNonAppContentParents ||
                   !sNonAppContentParents->Contains(this));
    } else {
        // In general, we expect sAppContentParents->Get(mAppManifestURL) to be
        // nullptr.  But it could be that we created another ContentParent for
        // this app after we did this->ActorDestroy(), so the right check is
        // that sAppContentParents->Get(mAppManifestURL) != this.
        MOZ_ASSERT(!sAppContentParents ||
                   sAppContentParents->Get(mAppManifestURL) != this);
    }

#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        sNuwaReady = false;
        sNuwaPid = 0;
    }
#endif
}

void
ContentParent::InitInternal(ProcessPriority aInitialPriority,
                            bool aSetupOffMainThreadCompositing,
                            bool aSendRegisteredChrome)
{
    if (aSendRegisteredChrome) {
        nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
        nsChromeRegistryChrome* chromeRegistry =
            static_cast<nsChromeRegistryChrome*>(registrySvc.get());
        chromeRegistry->SendRegisteredChrome(this);
    }

    // Initialize the message manager (and load delayed scripts) now that we
    // have established communications with the child.
    mMessageManager->InitWithCallback(this);

    // Set the subprocess's priority.  We do this early on because we're likely
    // /lowering/ the process's CPU and memory priority, which it has inherited
    // from this process.
    //
    // This call can cause us to send IPC messages to the child process, so it
    // must come after the Open() call above.
    ProcessPriorityManager::SetProcessPriority(this, aInitialPriority);

    if (gAppData) {
        nsCString version(gAppData->version);
        nsCString buildID(gAppData->buildID);
        nsCString name(gAppData->name);
        nsCString UAName(gAppData->UAName);
        nsCString ID(gAppData->ID);
        nsCString vendor(gAppData->vendor);

        // Sending all information to content process.
        unused << SendAppInfo(version, buildID, name, UAName, ID, vendor);
    }

    if (aSetupOffMainThreadCompositing) {
        // NB: internally, this will send an IPC message to the child
        // process to get it to create the CompositorChild.  This
        // message goes through the regular IPC queue for this
        // channel, so delivery will happen-before any other messages
        // we send.  The CompositorChild must be created before any
        // PBrowsers are created, because they rely on the Compositor
        // already being around.  (Creation is async, so can't happen
        // on demand.)
        bool useOffMainThreadCompositing = !!CompositorParent::CompositorLoop();
        if (useOffMainThreadCompositing) {
            DebugOnly<bool> opened = PCompositor::Open(this);
            MOZ_ASSERT(opened);

            opened = PImageBridge::Open(this);
            MOZ_ASSERT(opened);
        }
#ifdef MOZ_WIDGET_GONK
        DebugOnly<bool> opened = PSharedBufferManager::Open(this);
        MOZ_ASSERT(opened);
#endif
    }

    if (gAppData) {
        // Sending all information to content process.
        unused << SendAppInit();
    }

    nsStyleSheetService *sheetService = nsStyleSheetService::GetInstance();
    if (sheetService) {
        // This looks like a lot of work, but in a normal browser session we just
        // send two loads.

        nsCOMArray<nsIStyleSheet>& agentSheets = *sheetService->AgentStyleSheets();
        for (uint32_t i = 0; i < agentSheets.Length(); i++) {
            URIParams uri;
            SerializeURI(agentSheets[i]->GetSheetURI(), uri);
            unused << SendLoadAndRegisterSheet(uri, nsIStyleSheetService::AGENT_SHEET);
        }

        nsCOMArray<nsIStyleSheet>& userSheets = *sheetService->UserStyleSheets();
        for (uint32_t i = 0; i < userSheets.Length(); i++) {
            URIParams uri;
            SerializeURI(userSheets[i]->GetSheetURI(), uri);
            unused << SendLoadAndRegisterSheet(uri, nsIStyleSheetService::USER_SHEET);
        }

        nsCOMArray<nsIStyleSheet>& authorSheets = *sheetService->AuthorStyleSheets();
        for (uint32_t i = 0; i < authorSheets.Length(); i++) {
            URIParams uri;
            SerializeURI(authorSheets[i]->GetSheetURI(), uri);
            unused << SendLoadAndRegisterSheet(uri, nsIStyleSheetService::AUTHOR_SHEET);
        }
    }

#ifdef MOZ_CONTENT_SANDBOX
    bool shouldSandbox = true;
#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        shouldSandbox = false;
    }
#endif
    if (shouldSandbox && !SendSetProcessSandbox()) {
        KillHard("SandboxInitFailed");
    }
#endif
}

bool
ContentParent::IsAlive()
{
    return mIsAlive;
}

bool
ContentParent::IsForApp()
{
    return !mAppManifestURL.IsEmpty();
}

#ifdef MOZ_NUWA_PROCESS
bool
ContentParent::IsNuwaProcess()
{
    return mIsNuwaProcess;
}
#endif

int32_t
ContentParent::Pid()
{
    if (!mSubprocess || !mSubprocess->GetChildProcessHandle()) {
        return -1;
    }
    return base::GetProcId(mSubprocess->GetChildProcessHandle());
}

bool
ContentParent::RecvReadPrefsArray(InfallibleTArray<PrefSetting>* aPrefs)
{
    Preferences::GetPreferences(aPrefs);
    return true;
}

bool
ContentParent::RecvReadFontList(InfallibleTArray<FontListEntry>* retValue)
{
#ifdef ANDROID
    gfxAndroidPlatform::GetPlatform()->GetSystemFontList(retValue);
#endif
    return true;
}

bool
ContentParent::RecvReadPermissions(InfallibleTArray<IPC::Permission>* aPermissions)
{
#ifdef MOZ_PERMISSIONS
    nsCOMPtr<nsIPermissionManager> permissionManagerIface =
        services::GetPermissionManager();
    nsPermissionManager* permissionManager =
        static_cast<nsPermissionManager*>(permissionManagerIface.get());
    MOZ_ASSERT(permissionManager,
               "We have no permissionManager in the Chrome process !");

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    DebugOnly<nsresult> rv = permissionManager->GetEnumerator(getter_AddRefs(enumerator));
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Could not get enumerator!");
    while(1) {
        bool hasMore;
        enumerator->HasMoreElements(&hasMore);
        if (!hasMore)
            break;

        nsCOMPtr<nsISupports> supp;
        enumerator->GetNext(getter_AddRefs(supp));
        nsCOMPtr<nsIPermission> perm = do_QueryInterface(supp);

        nsCString host;
        perm->GetHost(host);
        uint32_t appId;
        perm->GetAppId(&appId);
        bool isInBrowserElement;
        perm->GetIsInBrowserElement(&isInBrowserElement);
        nsCString type;
        perm->GetType(type);
        uint32_t capability;
        perm->GetCapability(&capability);
        uint32_t expireType;
        perm->GetExpireType(&expireType);
        int64_t expireTime;
        perm->GetExpireTime(&expireTime);

        aPermissions->AppendElement(IPC::Permission(host, appId,
                                                    isInBrowserElement, type,
                                                    capability, expireType,
                                                    expireTime));
    }

    // Ask for future changes
    mSendPermissionUpdates = true;
#endif

    return true;
}

bool
ContentParent::RecvSetClipboard(const IPCDataTransfer& aDataTransfer,
                                const bool& aIsPrivateData,
                                const int32_t& aWhichClipboard)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, true);
    trans->Init(nullptr);

    const nsTArray<IPCDataTransferItem>& items = aDataTransfer.items();
    for (uint32_t j = 0; j < items.Length(); ++j) {
      const IPCDataTransferItem& item = items[j];

      trans->AddDataFlavor(item.flavor().get());

      if (item.data().type() == IPCDataTransferData::TnsString) {
        nsCOMPtr<nsISupportsString> dataWrapper =
          do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, true);

        nsString text = item.data().get_nsString();
        rv = dataWrapper->SetData(text);
        NS_ENSURE_SUCCESS(rv, true);

        rv = trans->SetTransferData(item.flavor().get(), dataWrapper,
                                    text.Length() * sizeof(char16_t));

        NS_ENSURE_SUCCESS(rv, true);
      } else if (item.data().type() == IPCDataTransferData::TnsCString) {
        if (item.flavor().EqualsLiteral(kNativeImageMime) ||
            item.flavor().EqualsLiteral(kJPEGImageMime) ||
            item.flavor().EqualsLiteral(kJPGImageMime) ||
            item.flavor().EqualsLiteral(kPNGImageMime) ||
            item.flavor().EqualsLiteral(kGIFImageMime)) {
          const IPCDataTransferImage& imageDetails = item.imageDetails();
          const gfxIntSize size(imageDetails.width(), imageDetails.height());
          if (!size.width || !size.height) {
            return true;
          }

          nsCString text = item.data().get_nsCString();
          mozilla::RefPtr<gfx::DataSourceSurface> image =
            new mozilla::gfx::SourceSurfaceRawData();
          mozilla::gfx::SourceSurfaceRawData* raw =
            static_cast<mozilla::gfx::SourceSurfaceRawData*>(image.get());
          raw->InitWrappingData(
            reinterpret_cast<uint8_t*>(const_cast<nsCString&>(text).BeginWriting()),
            size, imageDetails.stride(),
            static_cast<mozilla::gfx::SurfaceFormat>(imageDetails.format()), false);
          raw->GuaranteePersistance();

          nsRefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(image, size);
          nsCOMPtr<imgIContainer> imageContainer(image::ImageOps::CreateFromDrawable(drawable));

          nsCOMPtr<nsISupportsInterfacePointer>
            imgPtr(do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv));

          rv = imgPtr->SetData(imageContainer);
          NS_ENSURE_SUCCESS(rv, true);

          trans->SetTransferData(item.flavor().get(), imgPtr, sizeof(nsISupports*));
        } else {
          nsCOMPtr<nsISupportsCString> dataWrapper =
            do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv, true);

          const nsCString& text = item.data().get_nsCString();
          rv = dataWrapper->SetData(text);
          NS_ENSURE_SUCCESS(rv, true);

          rv = trans->SetTransferData(item.flavor().get(), dataWrapper,
                                      text.Length());

          NS_ENSURE_SUCCESS(rv, true);
        }
      }
    }

    trans->SetIsPrivateData(aIsPrivateData);

    clipboard->SetData(trans, nullptr, aWhichClipboard);
    return true;
}

bool
ContentParent::RecvGetClipboard(nsTArray<nsCString>&& aTypes,
                                const int32_t& aWhichClipboard,
                                IPCDataTransfer* aDataTransfer)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, true);
    trans->Init(nullptr);

    for (uint32_t t = 0; t < aTypes.Length(); t++) {
      trans->AddDataFlavor(aTypes[t].get());
    }

    clipboard->GetData(trans, aWhichClipboard);
    nsContentUtils::TransferableToIPCTransferable(trans, aDataTransfer,
                                                  nullptr, this);
    return true;
}

bool
ContentParent::RecvEmptyClipboard(const int32_t& aWhichClipboard)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->EmptyClipboard(aWhichClipboard);

    return true;
}

bool
ContentParent::RecvClipboardHasType(nsTArray<nsCString>&& aTypes,
                                    const int32_t& aWhichClipboard,
                                    bool* aHasType)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    const char** typesChrs = new const char *[aTypes.Length()];
    for (uint32_t t = 0; t < aTypes.Length(); t++) {
      typesChrs[t] = aTypes[t].get();
    }

    clipboard->HasDataMatchingFlavors(typesChrs, aTypes.Length(),
                                      aWhichClipboard, aHasType);

    delete [] typesChrs;
    return true;
}

bool
ContentParent::RecvGetSystemColors(const uint32_t& colorsCount, InfallibleTArray<uint32_t>* colors)
{
#ifdef MOZ_WIDGET_ANDROID
    NS_ASSERTION(AndroidBridge::Bridge() != nullptr, "AndroidBridge is not available");
    if (AndroidBridge::Bridge() == nullptr) {
        // Do not fail - the colors won't be right, but it's not critical
        return true;
    }

    colors->AppendElements(colorsCount);

    // The array elements correspond to the members of AndroidSystemColors structure,
    // so just pass the pointer to the elements buffer
    AndroidBridge::Bridge()->GetSystemColors((AndroidSystemColors*)colors->Elements());
#endif
    return true;
}

bool
ContentParent::RecvGetIconForExtension(const nsCString& aFileExt, const uint32_t& aIconSize, InfallibleTArray<uint8_t>* bits)
{
#ifdef MOZ_WIDGET_ANDROID
    NS_ASSERTION(AndroidBridge::Bridge() != nullptr, "AndroidBridge is not available");
    if (AndroidBridge::Bridge() == nullptr) {
        // Do not fail - just no icon will be shown
        return true;
    }

    bits->AppendElements(aIconSize * aIconSize * 4);

    AndroidBridge::Bridge()->GetIconForExtension(aFileExt, aIconSize, bits->Elements());
#endif
    return true;
}

bool
ContentParent::RecvGetShowPasswordSetting(bool* showPassword)
{
    // default behavior is to show the last password character
    *showPassword = true;
#ifdef MOZ_WIDGET_ANDROID
    NS_ASSERTION(AndroidBridge::Bridge() != nullptr, "AndroidBridge is not available");

    *showPassword = mozilla::widget::GeckoAppShell::GetShowPasswordSetting();
#endif
    return true;
}

bool
ContentParent::RecvFirstIdle()
{
    // When the ContentChild goes idle, it sends us a FirstIdle message
    // which we use as a good time to prelaunch another process. If we
    // prelaunch any sooner than this, then we'll be competing with the
    // child process and slowing it down.
    PreallocatedProcessManager::AllocateAfterDelay();
    return true;
}

bool
ContentParent::RecvAudioChannelChangeDefVolChannel(const int32_t& aChannel,
                                                   const bool& aHidden)
{
    nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    MOZ_ASSERT(service);
    service->SetDefaultVolumeControlChannelInternal(aChannel,
                                                    aHidden, mChildID);
    return true;
}

bool
ContentParent::RecvAudioChannelServiceStatus(
                                           const bool& aTelephonyChannel,
                                           const bool& aContentOrNormalChannel,
                                           const bool& aAnyChannel)
{
    nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    MOZ_ASSERT(service);

    service->ChildStatusReceived(mChildID, aTelephonyChannel,
                                 aContentOrNormalChannel, aAnyChannel);
    return true;
}

bool
ContentParent::RecvDataStoreGetStores(
                                    const nsString& aName,
                                    const nsString& aOwner,
                                    const IPC::Principal& aPrincipal,
                                    InfallibleTArray<DataStoreSetting>* aValue)
{
  nsRefPtr<DataStoreService> service = DataStoreService::GetOrCreate();
  if (NS_WARN_IF(!service)) {
    return false;
  }

  nsresult rv = service->GetDataStoresFromIPC(aName, aOwner, aPrincipal, aValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  mSendDataStoreInfos = true;
  return true;
}

bool
ContentParent::RecvNuwaReady()
{
#ifdef MOZ_NUWA_PROCESS
    if (!IsNuwaProcess()) {
        NS_ERROR(
            nsPrintfCString(
                "Terminating child process %d for unauthorized IPC message: NuwaReady",
                Pid()).get());

        KillHard("NuwaReady");
        return false;
    }
    sNuwaReady = true;
    PreallocatedProcessManager::OnNuwaReady();
    return true;
#else
    NS_ERROR("ContentParent::RecvNuwaReady() not implemented!");
    return false;
#endif
}

bool
ContentParent::RecvAddNewProcess(const uint32_t& aPid,
                                 InfallibleTArray<ProtocolFdMapping>&& aFds)
{
#ifdef MOZ_NUWA_PROCESS
    if (!IsNuwaProcess()) {
        NS_ERROR(
            nsPrintfCString(
                "Terminating child process %d for unauthorized IPC message: "
                "AddNewProcess(%d)", Pid(), aPid).get());

        KillHard("AddNewProcess");
        return false;
    }
    nsRefPtr<ContentParent> content;
    content = new ContentParent(this,
                                MAGIC_PREALLOCATED_APP_MANIFEST_URL,
                                aPid,
                                Move(aFds));
    content->Init();

    size_t numNuwaPrefUpdates = sNuwaPrefUpdates ?
                                sNuwaPrefUpdates->Length() : 0;
    // Resend pref updates to the forked child.
    for (size_t i = 0; i < numNuwaPrefUpdates; i++) {
        mozilla::unused << content->SendPreferenceUpdate(sNuwaPrefUpdates->ElementAt(i));
    }

    // Update offline settings.
    bool isOffline, isLangRTL;
    bool isConnected;
    InfallibleTArray<nsString> unusedDictionaries;
    ClipboardCapabilities clipboardCaps;
    DomainPolicyClone domainPolicy;
    OwningSerializedStructuredCloneBuffer initialData;

    RecvGetXPCOMProcessAttributes(&isOffline, &isConnected,
                                  &isLangRTL, &unusedDictionaries,
                                  &clipboardCaps, &domainPolicy, &initialData);
    mozilla::unused << content->SendSetOffline(isOffline);
    mozilla::unused << content->SendSetConnectivity(isConnected);
    MOZ_ASSERT(!clipboardCaps.supportsSelectionClipboard() &&
               !clipboardCaps.supportsFindClipboard(),
               "Unexpected values");

    PreallocatedProcessManager::PublishSpareProcess(content);
    return true;
#else
    NS_ERROR("ContentParent::RecvAddNewProcess() not implemented!");
    return false;
#endif
}

// We want ContentParent to show up in CC logs for debugging purposes, but we
// don't actually cycle collect it.
NS_IMPL_CYCLE_COLLECTION_0(ContentParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ContentParent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ContentParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ContentParent)
  NS_INTERFACE_MAP_ENTRY(nsIContentParent)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionCallback)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionErrorCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
ContentParent::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const char16_t* aData)
{
    if (mSubprocess && (!strcmp(aTopic, "profile-before-change") ||
                        !strcmp(aTopic, "xpcom-shutdown"))) {
        // Okay to call ShutDownProcess multiple times.
        ShutDownProcess(SEND_SHUTDOWN_MESSAGE);

        // Wait for shutdown to complete, so that we receive any shutdown
        // data (e.g. telemetry) from the child before we quit.
        // This loop terminate prematurely based on mForceKillTimer.
        while (mIPCOpen && !mCalledKillHard) {
            NS_ProcessNextEvent(nullptr, true);
        }
        NS_ASSERTION(!mSubprocess, "Close should have nulled mSubprocess");
    }

    if (!mIsAlive || !mSubprocess)
        return NS_OK;

    // listening for memory pressure event
    if (!strcmp(aTopic, "memory-pressure") &&
        !StringEndsWith(nsDependentString(aData),
                        NS_LITERAL_STRING("-no-forward"))) {
        unused << SendFlushMemory(nsDependentString(aData));
    }
    // listening for remotePrefs...
    else if (!strcmp(aTopic, "nsPref:changed")) {
        // We know prefs are ASCII here.
        NS_LossyConvertUTF16toASCII strData(aData);

        PrefSetting pref(strData, null_t(), null_t());
        Preferences::GetPreference(&pref);
#ifdef MOZ_NUWA_PROCESS
        if (IsNuwaProcess() && PreallocatedProcessManager::IsNuwaReady()) {
            // Don't send the pref update to the Nuwa process. Save the update
            // to send to the forked child.
            if (!sNuwaPrefUpdates) {
                sNuwaPrefUpdates = new nsTArray<PrefSetting>();
            }
            sNuwaPrefUpdates->AppendElement(pref);
        } else if (!SendPreferenceUpdate(pref)) {
            return NS_ERROR_NOT_AVAILABLE;
        }
#else
        if (!SendPreferenceUpdate(pref)) {
            return NS_ERROR_NOT_AVAILABLE;
        }
#endif
    }
    else if (!strcmp(aTopic, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC)) {
      NS_ConvertUTF16toUTF8 dataStr(aData);
      const char *offline = dataStr.get();
#ifdef MOZ_NUWA_PROCESS
      if (!(IsNuwaReady() && IsNuwaProcess())) {
#endif
          if (!SendSetOffline(!strcmp(offline, "true") ? true : false)) {
              return NS_ERROR_NOT_AVAILABLE;
          }
#ifdef MOZ_NUWA_PROCESS
      }
#endif
    }
    else if (!strcmp(aTopic, NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC)) {
#ifdef MOZ_NUWA_PROCESS
        if (!(IsNuwaReady() && IsNuwaProcess())) {
#endif
            if (!SendSetConnectivity(NS_LITERAL_STRING("true").Equals(aData))) {
                return NS_ERROR_NOT_AVAILABLE;
            }
#ifdef MOZ_NUWA_PROCESS
        }
#endif
    }
    // listening for alert notifications
    else if (!strcmp(aTopic, "alertfinished") ||
             !strcmp(aTopic, "alertclickcallback") ||
             !strcmp(aTopic, "alertshow") ) {
        if (!SendNotifyAlertsObserver(nsDependentCString(aTopic),
                                      nsDependentString(aData)))
            return NS_ERROR_NOT_AVAILABLE;
    }
    else if (!strcmp(aTopic, "child-gc-request")){
        unused << SendGarbageCollect();
    }
    else if (!strcmp(aTopic, "child-cc-request")){
        unused << SendCycleCollect();
    }
    else if (!strcmp(aTopic, "child-mmu-request")){
        unused << SendMinimizeMemoryUsage();
    }
    else if (!strcmp(aTopic, "last-pb-context-exited")) {
        unused << SendLastPrivateDocShellDestroyed();
    }
    else if (!strcmp(aTopic, "file-watcher-update")) {
        nsCString creason;
        CopyUTF16toUTF8(aData, creason);
        DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);

#ifdef MOZ_NUWA_PROCESS
        if (!(IsNuwaReady() && IsNuwaProcess()))
#endif
        {
            unused << SendFilePathUpdate(file->mStorageType, file->mStorageName, file->mPath, creason);
        }
    }
#ifdef MOZ_WIDGET_GONK
    else if(!strcmp(aTopic, NS_VOLUME_STATE_CHANGED)) {
        nsCOMPtr<nsIVolume> vol = do_QueryInterface(aSubject);
        if (!vol) {
            return NS_ERROR_NOT_AVAILABLE;
        }

        nsString volName;
        nsString mountPoint;
        int32_t  state;
        int32_t  mountGeneration;
        bool     isMediaPresent;
        bool     isSharing;
        bool     isFormatting;
        bool     isFake;
        bool     isUnmounting;
        bool     isRemovable;
        bool     isHotSwappable;

        vol->GetName(volName);
        vol->GetMountPoint(mountPoint);
        vol->GetState(&state);
        vol->GetMountGeneration(&mountGeneration);
        vol->GetIsMediaPresent(&isMediaPresent);
        vol->GetIsSharing(&isSharing);
        vol->GetIsFormatting(&isFormatting);
        vol->GetIsFake(&isFake);
        vol->GetIsUnmounting(&isUnmounting);
        vol->GetIsRemovable(&isRemovable);
        vol->GetIsHotSwappable(&isHotSwappable);

#ifdef MOZ_NUWA_PROCESS
        if (!(IsNuwaReady() && IsNuwaProcess()))
#endif
        {
            unused << SendFileSystemUpdate(volName, mountPoint, state,
                                           mountGeneration, isMediaPresent,
                                           isSharing, isFormatting, isFake,
                                           isUnmounting, isRemovable, isHotSwappable);
        }
    } else if (!strcmp(aTopic, "phone-state-changed")) {
        nsString state(aData);
        unused << SendNotifyPhoneStateChange(state);
    }
    else if(!strcmp(aTopic, NS_VOLUME_REMOVED)) {
#ifdef MOZ_NUWA_PROCESS
        if (!(IsNuwaReady() && IsNuwaProcess()))
#endif
        {
            nsString volName(aData);
            unused << SendVolumeRemoved(volName);
        }
    }
#endif
#ifdef ACCESSIBILITY
    // Make sure accessibility is running in content process when accessibility
    // gets initiated in chrome process.
    else if (aData && (*aData == '1') &&
             !strcmp(aTopic, "a11y-init-or-shutdown")) {
        unused << SendActivateA11y();
    }
#endif
    else if (!strcmp(aTopic, "app-theme-changed")) {
        unused << SendOnAppThemeChanged();
    }
#ifdef MOZ_ENABLE_PROFILER_SPS
    else if (!strcmp(aTopic, "profiler-started")) {
        nsCOMPtr<nsIProfilerStartParams> params(do_QueryInterface(aSubject));
        uint32_t entries;
        double interval;
        params->GetEntries(&entries);
        params->GetInterval(&interval);
        const nsTArray<nsCString>& features = params->GetFeatures();
        const nsTArray<nsCString>& threadFilterNames = params->GetThreadFilterNames();
        unused << SendStartProfiler(entries, interval, features, threadFilterNames);
    }
    else if (!strcmp(aTopic, "profiler-stopped")) {
        unused << SendStopProfiler();
    }
    else if (!strcmp(aTopic, "profiler-subprocess-gather")) {
        mGatherer = static_cast<ProfileGatherer*>(aSubject);
        mGatherer->WillGatherOOPProfile();
        unused << SendGatherProfile();
    }
    else if (!strcmp(aTopic, "profiler-subprocess")) {
        nsCOMPtr<nsIProfileSaveEvent> pse = do_QueryInterface(aSubject);
        if (pse) {
            if (!mProfile.IsEmpty()) {
                pse->AddSubProfile(mProfile.get());
                mProfile.Truncate();
            }
        }
    }
#endif
    return NS_OK;
}

PGMPServiceParent*
ContentParent::AllocPGMPServiceParent(mozilla::ipc::Transport* aTransport,
                                      base::ProcessId aOtherProcess)
{
    return GMPServiceParent::Create(aTransport, aOtherProcess);
}

PCompositorParent*
ContentParent::AllocPCompositorParent(mozilla::ipc::Transport* aTransport,
                                      base::ProcessId aOtherProcess)
{
    return CompositorParent::Create(aTransport, aOtherProcess);
}

PImageBridgeParent*
ContentParent::AllocPImageBridgeParent(mozilla::ipc::Transport* aTransport,
                                       base::ProcessId aOtherProcess)
{
    return ImageBridgeParent::Create(aTransport, aOtherProcess);
}

PBackgroundParent*
ContentParent::AllocPBackgroundParent(Transport* aTransport,
                                      ProcessId aOtherProcess)
{
    return BackgroundParent::Alloc(this, aTransport, aOtherProcess);
}

PProcessHangMonitorParent*
ContentParent::AllocPProcessHangMonitorParent(Transport* aTransport,
                                              ProcessId aOtherProcess)
{
    mHangMonitorActor = CreateHangMonitorParent(this, aTransport, aOtherProcess);
    return mHangMonitorActor;
}

PSharedBufferManagerParent*
ContentParent::AllocPSharedBufferManagerParent(mozilla::ipc::Transport* aTransport,
                                                base::ProcessId aOtherProcess)
{
    return SharedBufferManagerParent::Create(aTransport, aOtherProcess);
}

bool
ContentParent::RecvGetProcessAttributes(ContentParentId* aCpId,
                                        bool* aIsForApp, bool* aIsForBrowser)
{
    *aCpId = mChildID;
    *aIsForApp = IsForApp();
    *aIsForBrowser = mIsForBrowser;

    return true;
}

bool
ContentParent::RecvGetXPCOMProcessAttributes(bool* aIsOffline,
                                             bool* aIsConnected,
                                             bool* aIsLangRTL,
                                             InfallibleTArray<nsString>* dictionaries,
                                             ClipboardCapabilities* clipboardCaps,
                                             DomainPolicyClone* domainPolicy,
                                             OwningSerializedStructuredCloneBuffer* initialData)
{
    nsCOMPtr<nsIIOService> io(do_GetIOService());
    MOZ_ASSERT(io, "No IO service?");
    DebugOnly<nsresult> rv = io->GetOffline(aIsOffline);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed getting offline?");

    rv = io->GetConnectivity(aIsConnected);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed getting connectivity?");

    nsIBidiKeyboard* bidi = nsContentUtils::GetBidiKeyboard();

    *aIsLangRTL = false;
    if (bidi) {
        bidi->IsLangRTL(aIsLangRTL);
    }

    nsCOMPtr<nsISpellChecker> spellChecker(do_GetService(NS_SPELLCHECKER_CONTRACTID));
    MOZ_ASSERT(spellChecker, "No spell checker?");

    spellChecker->GetDictionaryList(dictionaries);

    nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1"));
    MOZ_ASSERT(clipboard, "No clipboard?");

    rv = clipboard->SupportsSelectionClipboard(&clipboardCaps->supportsSelectionClipboard());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = clipboard->SupportsFindClipboard(&clipboardCaps->supportsFindClipboard());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Let's copy the domain policy from the parent to the child (if it's active).
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    NS_ENSURE_TRUE(ssm, false);
    ssm->CloneDomainPolicy(domainPolicy);

    if (nsFrameMessageManager* mm = nsFrameMessageManager::sParentProcessManager) {
        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(xpc::PrivilegedJunkScope()))) {
            return false;
        }
        JS::RootedValue init(jsapi.cx());
        nsresult result = mm->GetInitialProcessData(jsapi.cx(), &init);
        if (NS_FAILED(result)) {
            return false;
        }

        JSAutoStructuredCloneBuffer buffer;
        if (!buffer.write(jsapi.cx(), init)) {
            return false;
        }

        buffer.steal(&initialData->data, &initialData->dataLength);
    }

    return true;
}

mozilla::jsipc::PJavaScriptParent *
ContentParent::AllocPJavaScriptParent()
{
    MOZ_ASSERT(!ManagedPJavaScriptParent().Length());
    return nsIContentParent::AllocPJavaScriptParent();
}

bool
ContentParent::DeallocPJavaScriptParent(PJavaScriptParent *parent)
{
    return nsIContentParent::DeallocPJavaScriptParent(parent);
}

PBrowserParent*
ContentParent::AllocPBrowserParent(const TabId& aTabId,
                                   const IPCTabContext& aContext,
                                   const uint32_t& aChromeFlags,
                                   const ContentParentId& aCpId,
                                   const bool& aIsForApp,
                                   const bool& aIsForBrowser)
{
    return nsIContentParent::AllocPBrowserParent(aTabId,
                                                 aContext,
                                                 aChromeFlags,
                                                 aCpId,
                                                 aIsForApp,
                                                 aIsForBrowser);
}

bool
ContentParent::DeallocPBrowserParent(PBrowserParent* frame)
{
    return nsIContentParent::DeallocPBrowserParent(frame);
}

PDeviceStorageRequestParent*
ContentParent::AllocPDeviceStorageRequestParent(const DeviceStorageParams& aParams)
{
    nsRefPtr<DeviceStorageRequestParent> result = new DeviceStorageRequestParent(aParams);
    if (!result->EnsureRequiredPermissions(this)) {
        return nullptr;
    }
    result->Dispatch();
    return result.forget().take();
}

bool
ContentParent::DeallocPDeviceStorageRequestParent(PDeviceStorageRequestParent* doomed)
{
    DeviceStorageRequestParent *parent = static_cast<DeviceStorageRequestParent*>(doomed);
    NS_RELEASE(parent);
    return true;
}

PFileSystemRequestParent*
ContentParent::AllocPFileSystemRequestParent(const FileSystemParams& aParams)
{
    nsRefPtr<FileSystemRequestParent> result = new FileSystemRequestParent();
    if (!result->Dispatch(this, aParams)) {
        return nullptr;
    }
    return result.forget().take();
}

bool
ContentParent::DeallocPFileSystemRequestParent(PFileSystemRequestParent* doomed)
{
    FileSystemRequestParent* parent = static_cast<FileSystemRequestParent*>(doomed);
    NS_RELEASE(parent);
    return true;
}

PBlobParent*
ContentParent::AllocPBlobParent(const BlobConstructorParams& aParams)
{
    return nsIContentParent::AllocPBlobParent(aParams);
}

bool
ContentParent::DeallocPBlobParent(PBlobParent* aActor)
{
    return nsIContentParent::DeallocPBlobParent(aActor);
}

mozilla::PRemoteSpellcheckEngineParent *
ContentParent::AllocPRemoteSpellcheckEngineParent()
{
    mozilla::RemoteSpellcheckEngineParent *parent = new mozilla::RemoteSpellcheckEngineParent();
    return parent;
}

bool
ContentParent::DeallocPRemoteSpellcheckEngineParent(PRemoteSpellcheckEngineParent *parent)
{
    delete parent;
    return true;
}

/* static */ void
ContentParent::ForceKillTimerCallback(nsITimer* aTimer, void* aClosure)
{
    auto self = static_cast<ContentParent*>(aClosure);
    self->KillHard("ShutDownKill");
}

void
ContentParent::KillHard(const char* aReason)
{
    // On Windows, calling KillHard multiple times causes problems - the
    // process handle becomes invalid on the first call, causing a second call
    // to crash our process - more details in bug 890840.
    if (mCalledKillHard) {
        return;
    }
    mCalledKillHard = true;
    mForceKillTimer = nullptr;

#if defined(MOZ_CRASHREPORTER) && !defined(MOZ_B2G)
    // We're about to kill the child process associated with this content.
    // Something has gone wrong to get us here, so we generate a minidump
    // of the parent and child for submission to the crash server.
    if (ManagedPCrashReporterParent().Length() > 0) {
        CrashReporterParent* crashReporter =
            static_cast<CrashReporterParent*>(ManagedPCrashReporterParent()[0]);
        // GeneratePairedMinidump creates two minidumps for us - the main
        // one is for the content process we're about to kill, and the other
        // one is for the main browser process. That second one is the extra
        // minidump tagging along, so we have to tell the crash reporter that
        // it exists and is being appended.
        nsAutoCString additionalDumps("browser");
        crashReporter->AnnotateCrashReport(
            NS_LITERAL_CSTRING("additional_minidumps"),
            additionalDumps);
        if (IsKillHardAnnotationSet()) {
          crashReporter->AnnotateCrashReport(
              NS_LITERAL_CSTRING("kill_hard"),
              GetKillHardAnnotation());
        }
        nsDependentCString reason(aReason);
        crashReporter->AnnotateCrashReport(
            NS_LITERAL_CSTRING("ipc_channel_error"),
            reason);

        // Generate the report and insert into the queue for submittal.
        mCreatedPairedMinidumps = crashReporter->GenerateCompleteMinidump(this);
    }
#endif
    ProcessHandle otherProcessHandle;
    if (!base::OpenProcessHandle(OtherPid(), &otherProcessHandle)) {
        NS_ERROR("Failed to open child process when attempting kill.");
        return;
    }

    if (!KillProcess(otherProcessHandle, base::PROCESS_END_KILLED_BY_USER,
                     false)) {
        NS_WARNING("failed to kill subprocess!");
    }

    if (mSubprocess) {
        mSubprocess->SetAlreadyDead();
    }

    // EnsureProcessTerminated has responsibilty for closing otherProcessHandle.
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&ProcessWatcher::EnsureProcessTerminated,
                            otherProcessHandle, /*force=*/true));
}

bool
ContentParent::IsPreallocated()
{
    return mAppManifestURL == MAGIC_PREALLOCATED_APP_MANIFEST_URL;
}

void
ContentParent::FriendlyName(nsAString& aName, bool aAnonymize)
{
    aName.Truncate();
#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        aName.AssignLiteral("(Nuwa)");
    } else
#endif
    if (IsPreallocated()) {
        aName.AssignLiteral("(Preallocated)");
    } else if (mIsForBrowser) {
        aName.AssignLiteral("Browser");
    } else if (aAnonymize) {
        aName.AssignLiteral("<anonymized-name>");
    } else if (!mAppName.IsEmpty()) {
        aName = mAppName;
    } else if (!mAppManifestURL.IsEmpty()) {
        aName.AssignLiteral("Unknown app: ");
        aName.Append(mAppManifestURL);
    } else {
        aName.AssignLiteral("???");
    }
}

PCrashReporterParent*
ContentParent::AllocPCrashReporterParent(const NativeThreadId& tid,
                                         const uint32_t& processType)
{
#ifdef MOZ_CRASHREPORTER
    return new CrashReporterParent();
#else
    return nullptr;
#endif
}

bool
ContentParent::RecvPCrashReporterConstructor(PCrashReporterParent* actor,
                                             const NativeThreadId& tid,
                                             const uint32_t& processType)
{
    static_cast<CrashReporterParent*>(actor)->SetChildData(tid, processType);
    return true;
}

bool
ContentParent::DeallocPCrashReporterParent(PCrashReporterParent* crashreporter)
{
    delete crashreporter;
    return true;
}

hal_sandbox::PHalParent*
ContentParent::AllocPHalParent()
{
    return hal_sandbox::CreateHalParent();
}

bool
ContentParent::DeallocPHalParent(hal_sandbox::PHalParent* aHal)
{
    delete aHal;
    return true;
}

PIccParent*
ContentParent::AllocPIccParent(const uint32_t& aServiceId)
{
    if (!AssertAppProcessPermission(this, "mobileconnection")) {
        return nullptr;
    }
    IccParent* parent = new IccParent(aServiceId);
    // We release this ref in DeallocPIccParent().
    parent->AddRef();

    return parent;
}

bool
ContentParent::DeallocPIccParent(PIccParent* aActor)
{
    // IccParent is refcounted, must not be freed manually.
    static_cast<IccParent*>(aActor)->Release();
    return true;
}

PMemoryReportRequestParent*
ContentParent::AllocPMemoryReportRequestParent(const uint32_t& aGeneration,
                                               const bool &aAnonymize,
                                               const bool &aMinimizeMemoryUsage,
                                               const MaybeFileDesc &aDMDFile)
{
    MemoryReportRequestParent* parent =
        new MemoryReportRequestParent(aGeneration);
    return parent;
}

bool
ContentParent::DeallocPMemoryReportRequestParent(PMemoryReportRequestParent* actor)
{
    delete actor;
    return true;
}

PCycleCollectWithLogsParent*
ContentParent::AllocPCycleCollectWithLogsParent(const bool& aDumpAllTraces,
                                                const FileDescriptor& aGCLog,
                                                const FileDescriptor& aCCLog)
{
    MOZ_CRASH("Don't call this; use ContentParent::CycleCollectWithLogs");
}

bool
ContentParent::DeallocPCycleCollectWithLogsParent(PCycleCollectWithLogsParent* aActor)
{
    delete aActor;
    return true;
}

bool
ContentParent::CycleCollectWithLogs(bool aDumpAllTraces,
                                    nsICycleCollectorLogSink* aSink,
                                    nsIDumpGCAndCCLogsCallback* aCallback)
{
    return CycleCollectWithLogsParent::AllocAndSendConstructor(this,
                                                               aDumpAllTraces,
                                                               aSink,
                                                               aCallback);
}

PTestShellParent*
ContentParent::AllocPTestShellParent()
{
    return new TestShellParent();
}

bool
ContentParent::DeallocPTestShellParent(PTestShellParent* shell)
{
    delete shell;
    return true;
}

PMobileConnectionParent*
ContentParent::AllocPMobileConnectionParent(const uint32_t& aClientId)
{
#ifdef MOZ_B2G_RIL
    nsRefPtr<MobileConnectionParent> parent = new MobileConnectionParent(aClientId);
    // We release this ref in DeallocPMobileConnectionParent().
    parent->AddRef();

    return parent;
#else
    MOZ_CRASH("No support for mobileconnection on this platform!");
#endif
}

bool
ContentParent::DeallocPMobileConnectionParent(PMobileConnectionParent* aActor)
{
#ifdef MOZ_B2G_RIL
    // MobileConnectionParent is refcounted, must not be freed manually.
    static_cast<MobileConnectionParent*>(aActor)->Release();
    return true;
#else
    MOZ_CRASH("No support for mobileconnection on this platform!");
#endif
}

PNeckoParent*
ContentParent::AllocPNeckoParent()
{
    return new NeckoParent();
}

bool
ContentParent::DeallocPNeckoParent(PNeckoParent* necko)
{
    delete necko;
    return true;
}

PPrintingParent*
ContentParent::AllocPPrintingParent()
{
#ifdef NS_PRINTING
    return new PrintingParent();
#else
    return nullptr;
#endif
}

bool
ContentParent::RecvPPrintingConstructor(PPrintingParent* aActor)
{
    return true;
}

bool
ContentParent::DeallocPPrintingParent(PPrintingParent* printing)
{
    delete printing;
    return true;
}

PScreenManagerParent*
ContentParent::AllocPScreenManagerParent(uint32_t* aNumberOfScreens,
                                         float* aSystemDefaultScale,
                                         bool* aSuccess)
{
    return new ScreenManagerParent(aNumberOfScreens, aSystemDefaultScale, aSuccess);
}

bool
ContentParent::DeallocPScreenManagerParent(PScreenManagerParent* aActor)
{
    delete aActor;
    return true;
}

PPSMContentDownloaderParent*
ContentParent::AllocPPSMContentDownloaderParent(const uint32_t& aCertType)
{
    nsRefPtr<PSMContentDownloaderParent> downloader =
        new PSMContentDownloaderParent(aCertType);
    return downloader.forget().take();
}

bool
ContentParent::DeallocPPSMContentDownloaderParent(PPSMContentDownloaderParent* aListener)
{
    auto* listener = static_cast<PSMContentDownloaderParent*>(aListener);
    nsRefPtr<PSMContentDownloaderParent> downloader = dont_AddRef(listener);
    return true;
}

PExternalHelperAppParent*
ContentParent::AllocPExternalHelperAppParent(const OptionalURIParams& uri,
                                             const nsCString& aMimeContentType,
                                             const nsCString& aContentDisposition,
                                             const uint32_t& aContentDispositionHint,
                                             const nsString& aContentDispositionFilename,
                                             const bool& aForceSave,
                                             const int64_t& aContentLength,
                                             const OptionalURIParams& aReferrer,
                                             PBrowserParent* aBrowser)
{
    ExternalHelperAppParent *parent = new ExternalHelperAppParent(uri, aContentLength);
    parent->AddRef();
    parent->Init(this,
                 aMimeContentType,
                 aContentDisposition,
                 aContentDispositionHint,
                 aContentDispositionFilename,
                 aForceSave,
                 aReferrer,
                 aBrowser);
    return parent;
}

bool
ContentParent::DeallocPExternalHelperAppParent(PExternalHelperAppParent* aService)
{
    ExternalHelperAppParent *parent = static_cast<ExternalHelperAppParent *>(aService);
    parent->Release();
    return true;
}

PCellBroadcastParent*
ContentParent::AllocPCellBroadcastParent()
{
    if (!AssertAppProcessPermission(this, "cellbroadcast")) {
        return nullptr;
    }

    CellBroadcastParent* actor = new CellBroadcastParent();
    actor->AddRef();
    return actor;
}

bool
ContentParent::DeallocPCellBroadcastParent(PCellBroadcastParent* aActor)
{
    static_cast<CellBroadcastParent*>(aActor)->Release();
    return true;
}

bool
ContentParent::RecvPCellBroadcastConstructor(PCellBroadcastParent* aActor)
{
    return static_cast<CellBroadcastParent*>(aActor)->Init();
}

PSmsParent*
ContentParent::AllocPSmsParent()
{
    if (!AssertAppProcessPermission(this, "sms")) {
        return nullptr;
    }

    SmsParent* parent = new SmsParent();
    parent->AddRef();
    return parent;
}

bool
ContentParent::DeallocPSmsParent(PSmsParent* aSms)
{
    static_cast<SmsParent*>(aSms)->Release();
    return true;
}

PTelephonyParent*
ContentParent::AllocPTelephonyParent()
{
    if (!AssertAppProcessPermission(this, "telephony")) {
        return nullptr;
    }

    TelephonyParent* actor = new TelephonyParent();
    NS_ADDREF(actor);
    return actor;
}

bool
ContentParent::DeallocPTelephonyParent(PTelephonyParent* aActor)
{
    static_cast<TelephonyParent*>(aActor)->Release();
    return true;
}

PVoicemailParent*
ContentParent::AllocPVoicemailParent()
{
    if (!AssertAppProcessPermission(this, "voicemail")) {
        return nullptr;
    }

    VoicemailParent* actor = new VoicemailParent();
    actor->AddRef();
    return actor;
}

bool
ContentParent::RecvPVoicemailConstructor(PVoicemailParent* aActor)
{
    return static_cast<VoicemailParent*>(aActor)->Init();
}

bool
ContentParent::DeallocPVoicemailParent(PVoicemailParent* aActor)
{
    static_cast<VoicemailParent*>(aActor)->Release();
    return true;
}

media::PMediaParent*
ContentParent::AllocPMediaParent()
{
  return media::AllocPMediaParent();
}

bool
ContentParent::DeallocPMediaParent(media::PMediaParent *aActor)
{
  return media::DeallocPMediaParent(aActor);
}

PStorageParent*
ContentParent::AllocPStorageParent()
{
    return new DOMStorageDBParent();
}

bool
ContentParent::DeallocPStorageParent(PStorageParent* aActor)
{
    DOMStorageDBParent* child = static_cast<DOMStorageDBParent*>(aActor);
    child->ReleaseIPDLReference();
    return true;
}

PBluetoothParent*
ContentParent::AllocPBluetoothParent()
{
#ifdef MOZ_B2G_BT
    if (!AssertAppProcessPermission(this, "bluetooth")) {
        return nullptr;
    }
    return new mozilla::dom::bluetooth::BluetoothParent();
#else
    MOZ_CRASH("No support for bluetooth on this platform!");
#endif
}

bool
ContentParent::DeallocPBluetoothParent(PBluetoothParent* aActor)
{
#ifdef MOZ_B2G_BT
    delete aActor;
    return true;
#else
    MOZ_CRASH("No support for bluetooth on this platform!");
#endif
}

bool
ContentParent::RecvPBluetoothConstructor(PBluetoothParent* aActor)
{
#ifdef MOZ_B2G_BT
    nsRefPtr<BluetoothService> btService = BluetoothService::Get();
    NS_ENSURE_TRUE(btService, false);

    return static_cast<BluetoothParent*>(aActor)->InitWithService(btService);
#else
    MOZ_CRASH("No support for bluetooth on this platform!");
#endif
}

PFMRadioParent*
ContentParent::AllocPFMRadioParent()
{
#ifdef MOZ_B2G_FM
    if (!AssertAppProcessPermission(this, "fmradio")) {
        return nullptr;
    }
    return new FMRadioParent();
#else
    NS_WARNING("No support for FMRadio on this platform!");
    return nullptr;
#endif
}

bool
ContentParent::DeallocPFMRadioParent(PFMRadioParent* aActor)
{
#ifdef MOZ_B2G_FM
    delete aActor;
    return true;
#else
    NS_WARNING("No support for FMRadio on this platform!");
    return false;
#endif
}

asmjscache::PAsmJSCacheEntryParent*
ContentParent::AllocPAsmJSCacheEntryParent(
                                    const asmjscache::OpenMode& aOpenMode,
                                    const asmjscache::WriteParams& aWriteParams,
                                    const IPC::Principal& aPrincipal)
{
    return asmjscache::AllocEntryParent(aOpenMode, aWriteParams, aPrincipal);
}

bool
ContentParent::DeallocPAsmJSCacheEntryParent(PAsmJSCacheEntryParent* aActor)
{
    asmjscache::DeallocEntryParent(aActor);
    return true;
}

PSpeechSynthesisParent*
ContentParent::AllocPSpeechSynthesisParent()
{
#ifdef MOZ_WEBSPEECH
    return new mozilla::dom::SpeechSynthesisParent();
#else
    return nullptr;
#endif
}

bool
ContentParent::DeallocPSpeechSynthesisParent(PSpeechSynthesisParent* aActor)
{
#ifdef MOZ_WEBSPEECH
    delete aActor;
    return true;
#else
    return false;
#endif
}

bool
ContentParent::RecvPSpeechSynthesisConstructor(PSpeechSynthesisParent* aActor)
{
#ifdef MOZ_WEBSPEECH
    return true;
#else
    return false;
#endif
}

bool
ContentParent::RecvSpeakerManagerGetSpeakerStatus(bool* aValue)
{
#ifdef MOZ_WIDGET_GONK
    *aValue = false;
    nsRefPtr<SpeakerManagerService> service =
      SpeakerManagerService::GetOrCreateSpeakerManagerService();
    MOZ_ASSERT(service);

    *aValue = service->GetSpeakerStatus();
    return true;
#endif
    return false;
}

bool
ContentParent::RecvSpeakerManagerForceSpeaker(const bool& aEnable)
{
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<SpeakerManagerService> service =
      SpeakerManagerService::GetOrCreateSpeakerManagerService();
    MOZ_ASSERT(service);
    service->ForceSpeaker(aEnable, mChildID);

    return true;
#endif
    return false;
}

bool
ContentParent::RecvStartVisitedQuery(const URIParams& aURI)
{
    nsCOMPtr<nsIURI> newURI = DeserializeURI(aURI);
    if (!newURI) {
        return false;
    }
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
        history->RegisterVisitedCallback(newURI, nullptr);
    }
    return true;
}


bool
ContentParent::RecvVisitURI(const URIParams& uri,
                            const OptionalURIParams& referrer,
                            const uint32_t& flags)
{
    nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
    if (!ourURI) {
        return false;
    }
    nsCOMPtr<nsIURI> ourReferrer = DeserializeURI(referrer);
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
        history->VisitURI(ourURI, ourReferrer, flags);
    }
    return true;
}


bool
ContentParent::RecvSetURITitle(const URIParams& uri,
                               const nsString& title)
{
    nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
    if (!ourURI) {
        return false;
    }
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
        history->SetURITitle(ourURI, title);
    }
    return true;
}

bool
ContentParent::RecvGetRandomValues(const uint32_t& length,
                                   InfallibleTArray<uint8_t>* randomValues)
{
    uint8_t* buf = Crypto::GetRandomValues(length);
    if (!buf) {
        return true;
    }

    randomValues->SetCapacity(length);
    randomValues->SetLength(length);

    memcpy(randomValues->Elements(), buf, length);

    free(buf);

    return true;
}

bool
ContentParent::RecvGetSystemMemory(const uint64_t& aGetterId)
{
    uint32_t memoryTotal = 0;

#if defined(XP_LINUX)
    memoryTotal = mozilla::hal::GetTotalSystemMemoryLevel();
#endif

    unused << SendSystemMemoryAvailable(aGetterId, memoryTotal);

    return true;
}

bool
ContentParent::RecvGetLookAndFeelCache(nsTArray<LookAndFeelInt>* aLookAndFeelIntCache)
{
    *aLookAndFeelIntCache = LookAndFeel::GetIntCache();
    return true;
}

bool
ContentParent::RecvIsSecureURI(const uint32_t& type,
                               const URIParams& uri,
                               const uint32_t& flags,
                               bool* isSecureURI)
{
    nsCOMPtr<nsISiteSecurityService> sss(do_GetService(NS_SSSERVICE_CONTRACTID));
    if (!sss) {
        return false;
    }
    nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
    if (!ourURI) {
        return false;
    }
    nsresult rv = sss->IsSecureURI(type, ourURI, flags, isSecureURI);
    return NS_SUCCEEDED(rv);
}

bool
ContentParent::RecvLoadURIExternal(const URIParams& uri)
{
    nsCOMPtr<nsIExternalProtocolService> extProtService(do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
    if (!extProtService) {
        return true;
    }
    nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
    if (!ourURI) {
        return false;
    }
    extProtService->LoadURI(ourURI, nullptr);
    return true;
}

bool
ContentParent::RecvShowAlertNotification(const nsString& aImageUrl, const nsString& aTitle,
                                         const nsString& aText, const bool& aTextClickable,
                                         const nsString& aCookie, const nsString& aName,
                                         const nsString& aBidi, const nsString& aLang,
                                         const nsString& aData,
                                         const IPC::Principal& aPrincipal,
                                         const bool& aInPrivateBrowsing)
{
#ifdef MOZ_CHILD_PERMISSIONS
    uint32_t permission = mozilla::CheckPermission(this, aPrincipal,
                                                   "desktop-notification");
    if (permission != nsIPermissionManager::ALLOW_ACTION) {
        return true;
    }
#endif /* MOZ_CHILD_PERMISSIONS */

    nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_ALERTSERVICE_CONTRACTID));
    if (sysAlerts) {
        sysAlerts->ShowAlertNotification(aImageUrl, aTitle, aText, aTextClickable,
                                         aCookie, this, aName, aBidi, aLang,
                                         aData, aPrincipal, aInPrivateBrowsing);
    }
    return true;
}

bool
ContentParent::RecvCloseAlert(const nsString& aName,
                              const IPC::Principal& aPrincipal)
{
#ifdef MOZ_CHILD_PERMISSIONS
    uint32_t permission = mozilla::CheckPermission(this, aPrincipal,
                                                   "desktop-notification");
    if (permission != nsIPermissionManager::ALLOW_ACTION) {
        return true;
    }
#endif

    nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_ALERTSERVICE_CONTRACTID));
    if (sysAlerts) {
        sysAlerts->CloseAlert(aName, aPrincipal);
    }

    return true;
}

bool
ContentParent::RecvSyncMessage(const nsString& aMsg,
                               const ClonedMessageData& aData,
                               InfallibleTArray<CpowEntry>&& aCpows,
                               const IPC::Principal& aPrincipal,
                               nsTArray<OwningSerializedStructuredCloneBuffer>* aRetvals)
{
    return nsIContentParent::RecvSyncMessage(aMsg, aData, Move(aCpows),
                                             aPrincipal, aRetvals);
}

bool
ContentParent::RecvRpcMessage(const nsString& aMsg,
                              const ClonedMessageData& aData,
                              InfallibleTArray<CpowEntry>&& aCpows,
                              const IPC::Principal& aPrincipal,
                              nsTArray<OwningSerializedStructuredCloneBuffer>* aRetvals)
{
    return nsIContentParent::RecvRpcMessage(aMsg, aData, Move(aCpows), aPrincipal,
                                            aRetvals);
}

bool
ContentParent::RecvAsyncMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                InfallibleTArray<CpowEntry>&& aCpows,
                                const IPC::Principal& aPrincipal)
{
    return nsIContentParent::RecvAsyncMessage(aMsg, aData, Move(aCpows),
                                              aPrincipal);
}

bool
ContentParent::RecvFilePathUpdateNotify(const nsString& aType,
                                        const nsString& aStorageName,
                                        const nsString& aFilePath,
                                        const nsCString& aReason)
{
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(aType,
                                                            aStorageName,
                                                            aFilePath);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (!obs) {
        return false;
    }
    obs->NotifyObservers(dsf, "file-watcher-update",
                         NS_ConvertASCIItoUTF16(aReason).get());
    return true;
}

static int32_t
AddGeolocationListener(nsIDOMGeoPositionCallback* watcher, nsIDOMGeoPositionErrorCallback* errorCallBack, bool highAccuracy)
{
    nsCOMPtr<nsIDOMGeoGeolocation> geo = do_GetService("@mozilla.org/geolocation;1");
    if (!geo) {
        return -1;
    }

    PositionOptions* options = new PositionOptions();
    options->mTimeout = 0;
    options->mMaximumAge = 0;
    options->mEnableHighAccuracy = highAccuracy;
    int32_t retval = 1;
    geo->WatchPosition(watcher, errorCallBack, options, &retval);
    return retval;
}

bool
ContentParent::RecvAddGeolocationListener(const IPC::Principal& aPrincipal,
                                          const bool& aHighAccuracy)
{
#ifdef MOZ_CHILD_PERMISSIONS
    if (!ContentParent::IgnoreIPCPrincipal()) {
        uint32_t permission = mozilla::CheckPermission(this, aPrincipal,
                                                       "geolocation");
        if (permission != nsIPermissionManager::ALLOW_ACTION) {
            return true;
        }
    }
#endif /* MOZ_CHILD_PERMISSIONS */

    // To ensure no geolocation updates are skipped, we always force the
    // creation of a new listener.
    RecvRemoveGeolocationListener();
    mGeolocationWatchID = AddGeolocationListener(this, this, aHighAccuracy);

    // let the the settings cache know the origin of the new listener
    nsAutoCString origin;
    // hint to the compiler to use the conversion operator to nsIPrincipal*
    nsCOMPtr<nsIPrincipal> principal = static_cast<nsIPrincipal*>(aPrincipal);
    if (!principal) {
      return true;
    }
    principal->GetOrigin(origin);
    nsRefPtr<nsGeolocationSettings> gs = nsGeolocationSettings::GetGeolocationSettings();
    if (gs) {
      gs->PutWatchOrigin(mGeolocationWatchID, origin);
    }
    return true;
}

bool
ContentParent::RecvRemoveGeolocationListener()
{
    if (mGeolocationWatchID != -1) {
        nsCOMPtr<nsIDOMGeoGeolocation> geo = do_GetService("@mozilla.org/geolocation;1");
        if (!geo) {
            return true;
        }
        geo->ClearWatch(mGeolocationWatchID);

        nsRefPtr<nsGeolocationSettings> gs = nsGeolocationSettings::GetGeolocationSettings();
        if (gs) {
          gs->RemoveWatchOrigin(mGeolocationWatchID);
        }
        mGeolocationWatchID = -1;
    }
    return true;
}

bool
ContentParent::RecvSetGeolocationHigherAccuracy(const bool& aEnable)
{
    // This should never be called without a listener already present,
    // so this check allows us to forgo securing privileges.
    if (mGeolocationWatchID != -1) {
        nsCString origin;
        nsRefPtr<nsGeolocationSettings> gs = nsGeolocationSettings::GetGeolocationSettings();
        // get the origin stored for the curent watch ID
        if (gs) {
          gs->GetWatchOrigin(mGeolocationWatchID, origin);
        }

        // remove and recreate a new, high-accuracy listener
        RecvRemoveGeolocationListener();
        mGeolocationWatchID = AddGeolocationListener(this, this, aEnable);

        // map the new watch ID to the origin
        if (gs) {
          gs->PutWatchOrigin(mGeolocationWatchID, origin);
        }
    }
    return true;
}

NS_IMETHODIMP
ContentParent::HandleEvent(nsIDOMGeoPosition* postion)
{
    unused << SendGeolocationUpdate(GeoPosition(postion));
    return NS_OK;
}

NS_IMETHODIMP
ContentParent::HandleEvent(nsIDOMGeoPositionError* postionError)
{
    int16_t errorCode;
    nsresult rv;
    rv = postionError->GetCode(&errorCode);
    NS_ENSURE_SUCCESS(rv,rv);
    unused << SendGeolocationError(errorCode);
    return NS_OK;
}

nsConsoleService *
ContentParent::GetConsoleService()
{
    if (mConsoleService) {
        return mConsoleService.get();
    }

    // Get the ConsoleService by CID rather than ContractID, so that we
    // can cast the returned pointer to an nsConsoleService (rather than
    // just an nsIConsoleService). This allows us to call the non-idl function
    // nsConsoleService::LogMessageWithMode.
    NS_DEFINE_CID(consoleServiceCID, NS_CONSOLESERVICE_CID);
    nsCOMPtr<nsConsoleService>  consoleService(do_GetService(consoleServiceCID));
    mConsoleService = consoleService;
    return mConsoleService.get();
}

bool
ContentParent::RecvConsoleMessage(const nsString& aMessage)
{
    nsRefPtr<nsConsoleService> consoleService = GetConsoleService();
    if (!consoleService) {
        return true;
    }

    nsRefPtr<nsConsoleMessage> msg(new nsConsoleMessage(aMessage.get()));
    consoleService->LogMessageWithMode(msg, nsConsoleService::SuppressLog);
    return true;
}

bool
ContentParent::RecvScriptError(const nsString& aMessage,
                                      const nsString& aSourceName,
                                      const nsString& aSourceLine,
                                      const uint32_t& aLineNumber,
                                      const uint32_t& aColNumber,
                                      const uint32_t& aFlags,
                                      const nsCString& aCategory)
{
    nsRefPtr<nsConsoleService> consoleService = GetConsoleService();
    if (!consoleService) {
        return true;
    }

    nsCOMPtr<nsIScriptError> msg(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
    nsresult rv = msg->Init(aMessage, aSourceName, aSourceLine,
                            aLineNumber, aColNumber, aFlags, aCategory.get());
    if (NS_FAILED(rv))
        return true;

    consoleService->LogMessageWithMode(msg, nsConsoleService::SuppressLog);
    return true;
}

bool
ContentParent::RecvPrivateDocShellsExist(const bool& aExist)
{
    if (!sPrivateContent)
        sPrivateContent = new nsTArray<ContentParent*>();
    if (aExist) {
        sPrivateContent->AppendElement(this);
    } else {
        sPrivateContent->RemoveElement(this);

        // Only fire the notification if we have private and non-private
        // windows: if privatebrowsing.autostart is true, all windows are
        // private.
        if (!sPrivateContent->Length() &&
            !Preferences::GetBool("browser.privatebrowsing.autostart")) {
            nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
            obs->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
            delete sPrivateContent;
            sPrivateContent = nullptr;
        }
    }
    return true;
}

bool
ContentParent::DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope)
{
    MOZ_ASSERT(!aRunInGlobalScope);
    return SendLoadProcessScript(nsString(aURL));
}

bool
ContentParent::DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal)
{
    ClonedMessageData data;
    if (!BuildClonedMessageDataForParent(this, aData, data)) {
        return false;
    }
    InfallibleTArray<CpowEntry> cpows;
    jsipc::CPOWManager* mgr = GetCPOWManager();
    if (aCpows && (!mgr || !mgr->Wrap(aCx, aCpows, &cpows))) {
        return false;
    }
#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess() && IsNuwaReady()) {
        // Nuwa won't receive frame messages after it is frozen.
        return true;
    }
#endif
    return SendAsyncMessage(nsString(aMessage), data, cpows, Principal(aPrincipal));
}

bool
ContentParent::CheckPermission(const nsAString& aPermission)
{
    return AssertAppProcessPermission(this, NS_ConvertUTF16toUTF8(aPermission).get());
}

bool
ContentParent::CheckManifestURL(const nsAString& aManifestURL)
{
    return AssertAppProcessManifestURL(this, NS_ConvertUTF16toUTF8(aManifestURL).get());
}

bool
ContentParent::CheckAppHasPermission(const nsAString& aPermission)
{
    return AssertAppHasPermission(this, NS_ConvertUTF16toUTF8(aPermission).get());
}

bool
ContentParent::CheckAppHasStatus(unsigned short aStatus)
{
    return AssertAppHasStatus(this, aStatus);
}

bool
ContentParent::KillChild()
{
  KillHard("KillChild");
  return true;
}

PBlobParent*
ContentParent::SendPBlobConstructor(PBlobParent* aActor,
                                    const BlobConstructorParams& aParams)
{
    return PContentParent::SendPBlobConstructor(aActor, aParams);
}

bool
ContentParent::RecvSystemMessageHandled()
{
    SystemMessageHandledListener::OnSystemMessageHandled();
    return true;
}

PBrowserParent*
ContentParent::SendPBrowserConstructor(PBrowserParent* aActor,
                                       const TabId& aTabId,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const ContentParentId& aCpId,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser)
{
    return PContentParent::SendPBrowserConstructor(aActor,
                                                   aTabId,
                                                   aContext,
                                                   aChromeFlags,
                                                   aCpId,
                                                   aIsForApp,
                                                   aIsForBrowser);
}

bool
ContentParent::RecvCreateFakeVolume(const nsString& fsName, const nsString& mountPoint)
{
#ifdef MOZ_WIDGET_GONK
    nsresult rv;
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID, &rv);
    if (vs) {
        vs->CreateFakeVolume(fsName, mountPoint);
    }
    return true;
#else
    NS_WARNING("ContentParent::RecvCreateFakeVolume shouldn't be called when MOZ_WIDGET_GONK is not defined");
    return false;
#endif
}

bool
ContentParent::RecvSetFakeVolumeState(const nsString& fsName, const int32_t& fsState)
{
#ifdef MOZ_WIDGET_GONK
    nsresult rv;
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID, &rv);
    if (vs) {
        vs->SetFakeVolumeState(fsName, fsState);
    }
    return true;
#else
    NS_WARNING("ContentParent::RecvSetFakeVolumeState shouldn't be called when MOZ_WIDGET_GONK is not defined");
    return false;
#endif
}

bool
ContentParent::RecvRemoveFakeVolume(const nsString& fsName)
{
#ifdef MOZ_WIDGET_GONK
    nsresult rv;
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID, &rv);
    if (vs) {
        vs->RemoveFakeVolume(fsName);
    }
    return true;
#else
    NS_WARNING("ContentParent::RecvRemoveFakeVolume shouldn't be called when MOZ_WIDGET_GONK is not defined");
    return false;
#endif
}

bool
ContentParent::RecvKeywordToURI(const nsCString& aKeyword,
                                nsString* aProviderName,
                                OptionalInputStreamParams* aPostData,
                                OptionalURIParams* aURI)
{
    nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
    if (!fixup) {
        return true;
    }

    nsCOMPtr<nsIInputStream> postData;
    nsCOMPtr<nsIURIFixupInfo> info;

    if (NS_FAILED(fixup->KeywordToURI(aKeyword, getter_AddRefs(postData),
                                      getter_AddRefs(info)))) {
        return true;
    }
    info->GetKeywordProviderName(*aProviderName);

    nsTArray<mozilla::ipc::FileDescriptor> fds;
    SerializeInputStream(postData, *aPostData, fds);
    MOZ_ASSERT(fds.IsEmpty());

    nsCOMPtr<nsIURI> uri;
    info->GetPreferredURI(getter_AddRefs(uri));
    SerializeURI(uri, *aURI);
    return true;
}

bool
ContentParent::RecvNotifyKeywordSearchLoading(const nsString &aProvider,
                                              const nsString &aKeyword) {
#ifdef MOZ_TOOLKIT_SEARCH
    nsCOMPtr<nsIBrowserSearchService> searchSvc = do_GetService("@mozilla.org/browser/search-service;1");
    if (searchSvc) {
        nsCOMPtr<nsISearchEngine> searchEngine;
        searchSvc->GetEngineByName(aProvider, getter_AddRefs(searchEngine));
        if (searchEngine) {
            nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
            if (obsSvc) {
                // Note that "keyword-search" refers to a search via the url
                // bar, not a bookmarks keyword search.
                obsSvc->NotifyObservers(searchEngine, "keyword-search", aKeyword.get());
            }
        }
    }
#endif
    return true;
}

bool
ContentParent::RecvCopyFavicon(const URIParams& aOldURI,
                               const URIParams& aNewURI,
                               const bool& aInPrivateBrowsing)
{
    nsCOMPtr<nsIURI> oldURI = DeserializeURI(aOldURI);
    if (!oldURI) {
        return true;
    }
    nsCOMPtr<nsIURI> newURI = DeserializeURI(aNewURI);
    if (!newURI) {
        return true;
    }

    nsDocShell::CopyFavicon(oldURI, newURI, aInPrivateBrowsing);
    return true;
}

bool
ContentParent::ShouldContinueFromReplyTimeout()
{
    nsRefPtr<ProcessHangMonitor> monitor = ProcessHangMonitor::Get();
    return !monitor || !monitor->ShouldTimeOutCPOWs();
}

bool
ContentParent::RecvRecordingDeviceEvents(const nsString& aRecordingStatus,
                                         const nsString& aPageURL,
                                         const bool& aIsAudio,
                                         const bool& aIsVideo)
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        // recording-device-ipc-events needs to gather more information from content process
        nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
        props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), ChildID());
        props->SetPropertyAsBool(NS_LITERAL_STRING("isApp"), IsForApp());
        props->SetPropertyAsBool(NS_LITERAL_STRING("isAudio"), aIsAudio);
        props->SetPropertyAsBool(NS_LITERAL_STRING("isVideo"), aIsVideo);

        nsString requestURL = IsForApp() ? AppManifestURL() : aPageURL;
        props->SetPropertyAsAString(NS_LITERAL_STRING("requestURL"), requestURL);

        obs->NotifyObservers((nsIPropertyBag2*) props,
                             "recording-device-ipc-events",
                             aRecordingStatus.get());
    } else {
        NS_WARNING("Could not get the Observer service for ContentParent::RecvRecordingDeviceEvents.");
    }
    return true;
}

bool
ContentParent::RecvGetGraphicsFeatureStatus(const int32_t& aFeature,
                                            int32_t* aStatus,
                                            bool* aSuccess)
{
    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    if (!gfxInfo) {
        *aSuccess = false;
        return true;
    }

    *aSuccess = NS_SUCCEEDED(gfxInfo->GetFeatureStatus(aFeature, aStatus));
    return true;
}

bool
ContentParent::RecvAddIdleObserver(const uint64_t& aObserver, const uint32_t& aIdleTimeInS)
{
    nsresult rv;
    nsCOMPtr<nsIIdleService> idleService =
        do_GetService("@mozilla.org/widget/idleservice;1", &rv);
    NS_ENSURE_SUCCESS(rv, false);

    nsRefPtr<ParentIdleListener> listener =
        new ParentIdleListener(this, aObserver, aIdleTimeInS);
    rv = idleService->AddIdleObserver(listener, aIdleTimeInS);
    NS_ENSURE_SUCCESS(rv, false);
    mIdleListeners.AppendElement(listener);
    return true;
}

bool
ContentParent::RecvRemoveIdleObserver(const uint64_t& aObserver, const uint32_t& aIdleTimeInS)
{
    nsRefPtr<ParentIdleListener> listener;
    for (int32_t i = mIdleListeners.Length() - 1; i >= 0; --i) {
        listener = static_cast<ParentIdleListener*>(mIdleListeners[i].get());
        if (listener->mObserver == aObserver &&
            listener->mTime == aIdleTimeInS) {
            nsresult rv;
            nsCOMPtr<nsIIdleService> idleService =
                do_GetService("@mozilla.org/widget/idleservice;1", &rv);
            NS_ENSURE_SUCCESS(rv, false);
            idleService->RemoveIdleObserver(listener, aIdleTimeInS);
            mIdleListeners.RemoveElementAt(i);
            break;
        }
    }
    return true;
}

bool
ContentParent::RecvBackUpXResources(const FileDescriptor& aXSocketFd)
{
#ifndef MOZ_X11
    NS_RUNTIMEABORT("This message only makes sense on X11 platforms");
#else
    MOZ_ASSERT(0 > mChildXSocketFdDup.get(),
               "Already backed up X resources??");
    mChildXSocketFdDup.forget();
    if (aXSocketFd.IsValid()) {
        mChildXSocketFdDup.reset(aXSocketFd.PlatformHandle());
    }
#endif
    return true;
}

bool
ContentParent::RecvOpenAnonymousTemporaryFile(FileDescOrError *aFD)
{
    PRFileDesc *prfd;
    nsresult rv = NS_OpenAnonymousTemporaryFile(&prfd);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        // Returning false will kill the child process; instead
        // propagate the error and let the child handle it.
        *aFD = rv;
        return true;
    }
    *aFD = FileDescriptor(FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(prfd)));
    // The FileDescriptor object owns a duplicate of the file handle; we
    // must close the original (and clean up the NSPR descriptor).
    PR_Close(prfd);
    return true;
}

static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID);

bool
ContentParent::RecvKeygenProcessValue(const nsString& oldValue,
                                      const nsString& challenge,
                                      const nsString& keytype,
                                      const nsString& keyparams,
                                      nsString* newValue)
{
    nsCOMPtr<nsIFormProcessor> formProcessor =
      do_GetService(kFormProcessorCID);
    if (!formProcessor) {
        newValue->Truncate();
        return true;
    }

    formProcessor->ProcessValueIPC(oldValue, challenge, keytype, keyparams,
                                   *newValue);
    return true;
}

bool
ContentParent::RecvKeygenProvideContent(nsString* aAttribute,
                                        nsTArray<nsString>* aContent)
{
    nsCOMPtr<nsIFormProcessor> formProcessor =
      do_GetService(kFormProcessorCID);
    if (!formProcessor) {
        return true;
    }

    formProcessor->ProvideContent(NS_LITERAL_STRING("SELECT"), *aContent,
                                  *aAttribute);
    return true;
}

PFileDescriptorSetParent*
ContentParent::AllocPFileDescriptorSetParent(const FileDescriptor& aFD)
{
    return new FileDescriptorSetParent(aFD);
}

bool
ContentParent::DeallocPFileDescriptorSetParent(PFileDescriptorSetParent* aActor)
{
    delete static_cast<FileDescriptorSetParent*>(aActor);
    return true;
}

bool
ContentParent::RecvGetFileReferences(const PersistenceType& aPersistenceType,
                                     const nsCString& aOrigin,
                                     const nsString& aDatabaseName,
                                     const int64_t& aFileId,
                                     int32_t* aRefCnt,
                                     int32_t* aDBRefCnt,
                                     int32_t* aSliceRefCnt,
                                     bool* aResult)
{
    MOZ_ASSERT(aRefCnt);
    MOZ_ASSERT(aDBRefCnt);
    MOZ_ASSERT(aSliceRefCnt);
    MOZ_ASSERT(aResult);

    if (NS_WARN_IF(aPersistenceType != quota::PERSISTENCE_TYPE_PERSISTENT &&
                   aPersistenceType != quota::PERSISTENCE_TYPE_TEMPORARY &&
                   aPersistenceType != quota::PERSISTENCE_TYPE_DEFAULT)) {
        return false;
    }

    if (NS_WARN_IF(aOrigin.IsEmpty())) {
        return false;
    }

    if (NS_WARN_IF(aDatabaseName.IsEmpty())) {
        return false;
    }

    if (NS_WARN_IF(aFileId < 1)) {
        return false;
    }

    nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();
    if (NS_WARN_IF(!mgr)) {
        return false;
    }

    if (NS_WARN_IF(!mgr->IsMainProcess())) {
        return false;
    }

    nsresult rv =
        mgr->BlockAndGetFileReferences(aPersistenceType,
                                       aOrigin,
                                       aDatabaseName,
                                       aFileId,
                                       aRefCnt,
                                       aDBRefCnt,
                                       aSliceRefCnt,
                                       aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    return true;
}

bool
ContentParent::RecvFlushPendingFileDeletions()
{
    nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();
    if (NS_WARN_IF(!mgr)) {
        return false;
    }

    if (NS_WARN_IF(!mgr->IsMainProcess())) {
        return false;
    }

    nsresult rv = mgr->FlushPendingFileDeletions();
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
    }

    return true;
}

bool
ContentParent::IgnoreIPCPrincipal()
{
  static bool sDidAddVarCache = false;
  static bool sIgnoreIPCPrincipal = false;
  if (!sDidAddVarCache) {
    sDidAddVarCache = true;
    Preferences::AddBoolVarCache(&sIgnoreIPCPrincipal,
                                 "dom.testing.ignore_ipc_principal", false);
  }
  return sIgnoreIPCPrincipal;
}

void
ContentParent::NotifyUpdatedDictionaries()
{
    nsAutoTArray<ContentParent*, 8> processes;
    GetAll(processes);

    nsCOMPtr<nsISpellChecker> spellChecker(do_GetService(NS_SPELLCHECKER_CONTRACTID));
    MOZ_ASSERT(spellChecker, "No spell checker?");

    InfallibleTArray<nsString> dictionaries;
    spellChecker->GetDictionaryList(&dictionaries);

    for (size_t i = 0; i < processes.Length(); ++i) {
        unused << processes[i]->SendUpdateDictionaryList(dictionaries);
    }
}

/*static*/ TabId
ContentParent::AllocateTabId(const TabId& aOpenerTabId,
                             const IPCTabContext& aContext,
                             const ContentParentId& aCpId)
{
    TabId tabId;
    if (XRE_IsParentProcess()) {
        ContentProcessManager *cpm = ContentProcessManager::GetSingleton();
        tabId = cpm->AllocateTabId(aOpenerTabId, aContext, aCpId);
    }
    else {
        ContentChild::GetSingleton()->SendAllocateTabId(aOpenerTabId,
                                                        aContext,
                                                        aCpId,
                                                        &tabId);
    }
    return tabId;
}

/*static*/ void
ContentParent::DeallocateTabId(const TabId& aTabId,
                               const ContentParentId& aCpId)
{
    if (XRE_IsParentProcess()) {
        ContentProcessManager::GetSingleton()->DeallocateTabId(aCpId,
                                                               aTabId);
    }
    else {
        ContentChild::GetSingleton()->SendDeallocateTabId(aTabId);
    }
}

bool
ContentParent::RecvAllocateTabId(const TabId& aOpenerTabId,
                                 const IPCTabContext& aContext,
                                 const ContentParentId& aCpId,
                                 TabId* aTabId)
{
    *aTabId = AllocateTabId(aOpenerTabId, aContext, aCpId);
    if (!(*aTabId)) {
        return false;
    }
    return true;
}

bool
ContentParent::RecvDeallocateTabId(const TabId& aTabId)
{
    DeallocateTabId(aTabId, this->ChildID());
    return true;
}

nsTArray<TabContext>
ContentParent::GetManagedTabContext()
{
    return Move(ContentProcessManager::GetSingleton()->
        GetTabContextByContentProcess(this->ChildID()));
}

mozilla::docshell::POfflineCacheUpdateParent*
ContentParent::AllocPOfflineCacheUpdateParent(const URIParams& aManifestURI,
                                              const URIParams& aDocumentURI,
                                              const bool& aStickDocument,
                                              const TabId& aTabId)
{
    TabContext tabContext;
    if (!ContentProcessManager::GetSingleton()->
        GetTabContextByProcessAndTabId(this->ChildID(), aTabId, &tabContext)) {
        return nullptr;
    }
    nsRefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
        new mozilla::docshell::OfflineCacheUpdateParent(
            tabContext.OwnOrContainingAppId(),
            tabContext.IsBrowserElement());
    // Use this reference as the IPDL reference.
    return update.forget().take();
}

bool
ContentParent::RecvPOfflineCacheUpdateConstructor(POfflineCacheUpdateParent* aActor,
                                                  const URIParams& aManifestURI,
                                                  const URIParams& aDocumentURI,
                                                  const bool& aStickDocument,
                                                  const TabId& aTabId)
{
    MOZ_ASSERT(aActor);

    nsRefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
        static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(aActor);

    nsresult rv = update->Schedule(aManifestURI, aDocumentURI, aStickDocument);
    if (NS_FAILED(rv) && IsAlive()) {
        // Inform the child of failure.
        unused << update->SendFinish(false, false);
    }

    return true;
}

bool
ContentParent::DeallocPOfflineCacheUpdateParent(POfflineCacheUpdateParent* aActor)
{
    // Reclaim the IPDL reference.
    nsRefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
        dont_AddRef(static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(aActor));
    return true;
}

PWebrtcGlobalParent *
ContentParent::AllocPWebrtcGlobalParent()
{
#ifdef MOZ_WEBRTC
    return WebrtcGlobalParent::Alloc();
#else
    return nullptr;
#endif
}

bool
ContentParent::DeallocPWebrtcGlobalParent(PWebrtcGlobalParent *aActor)
{
#ifdef MOZ_WEBRTC
    WebrtcGlobalParent::Dealloc(static_cast<WebrtcGlobalParent*>(aActor));
    return true;
#else
    return false;
#endif
}

bool
ContentParent::RecvSetOfflinePermission(const Principal& aPrincipal)
{
    nsIPrincipal* principal = aPrincipal;
    nsContentUtils::MaybeAllowOfflineAppByDefault(principal, nullptr);
    return true;
}

void
ContentParent::MaybeInvokeDragSession(TabParent* aParent)
{
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService && dragService->MaybeAddChildProcess(this)) {
    // We need to send transferable data to child process.
    nsCOMPtr<nsIDragSession> session;
    dragService->GetCurrentSession(getter_AddRefs(session));
    if (session) {
      nsTArray<IPCDataTransfer> dataTransfers;
      nsCOMPtr<nsIDOMDataTransfer> domTransfer;
      session->GetDataTransfer(getter_AddRefs(domTransfer));
      nsCOMPtr<DataTransfer> transfer = do_QueryInterface(domTransfer);
      if (!transfer) {
        // Pass NS_DRAGDROP_DROP to get DataTransfer with external
        // drag formats cached.
        transfer = new DataTransfer(nullptr, NS_DRAGDROP_DROP, true, -1);
        session->SetDataTransfer(transfer);
      }
      // Note, even though this fills the DataTransfer object with
      // external data, the data is usually transfered over IPC lazily when
      // needed.
      transfer->FillAllExternalData();
      nsCOMPtr<nsILoadContext> lc = aParent ?
                                     aParent->GetLoadContext() : nullptr;
      nsCOMPtr<nsISupportsArray> transferables =
        transfer->GetTransferables(lc);
      nsContentUtils::TransferablesToIPCTransferables(transferables,
                                                      dataTransfers,
                                                      nullptr,
                                                      this);
      uint32_t action;
      session->GetDragAction(&action);
      mozilla::unused << SendInvokeDragSession(dataTransfers, action);
    }
  }
}

bool
ContentParent::RecvUpdateDropEffect(const uint32_t& aDragAction,
                                    const uint32_t& aDropEffect)
{
  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->SetDragAction(aDragAction);
    nsCOMPtr<nsIDOMDataTransfer> dt;
    dragSession->GetDataTransfer(getter_AddRefs(dt));
    if (dt) {
      dt->SetDropEffectInt(aDropEffect);
    }
    dragSession->UpdateDragEffect();
  }
  return true;
}

PContentPermissionRequestParent*
ContentParent::AllocPContentPermissionRequestParent(const InfallibleTArray<PermissionRequest>& aRequests,
                                                    const IPC::Principal& aPrincipal,
                                                    const TabId& aTabId)
{
    ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
    nsRefPtr<TabParent> tp = cpm->GetTopLevelTabParentByProcessAndTabId(this->ChildID(),
                                                                        aTabId);
    if (!tp) {
        return nullptr;
    }

    return nsContentPermissionUtils::CreateContentPermissionRequestParent(aRequests,
                                                                          tp->GetOwnerElement(),
                                                                          aPrincipal,
                                                                          aTabId);
}

bool
ContentParent::DeallocPContentPermissionRequestParent(PContentPermissionRequestParent* actor)
{
    nsContentPermissionUtils::NotifyRemoveContentPermissionRequestParent(actor);
    delete actor;
    return true;
}

bool
ContentParent::RecvGetBrowserConfiguration(const nsCString& aURI, BrowserConfiguration* aConfig)
{
    MOZ_ASSERT(XRE_IsParentProcess());

    return GetBrowserConfiguration(aURI, *aConfig);;
}

/*static*/ bool
ContentParent::GetBrowserConfiguration(const nsCString& aURI, BrowserConfiguration& aConfig)
{
    if (XRE_IsParentProcess()) {
        nsRefPtr<ServiceWorkerRegistrar> swr = ServiceWorkerRegistrar::Get();
        MOZ_ASSERT(swr);

        swr->GetRegistrations(aConfig.serviceWorkerRegistrations());
        return true;
    }

    return ContentChild::GetSingleton()->SendGetBrowserConfiguration(aURI, &aConfig);
}

bool
ContentParent::RecvGamepadListenerAdded()
{
#ifdef MOZ_GAMEPAD
    if (mHasGamepadListener) {
        NS_WARNING("Gamepad listener already started, cannot start again!");
        return false;
    }
    mHasGamepadListener = true;
    StartGamepadMonitoring();
#endif
    return true;
}

bool
ContentParent::RecvGamepadListenerRemoved()
{
#ifdef MOZ_GAMEPAD
    if (!mHasGamepadListener) {
        NS_WARNING("Gamepad listener already stopped, cannot stop again!");
        return false;
    }
    mHasGamepadListener = false;
    MaybeStopGamepadMonitoring();
#endif
    return true;
}

bool
ContentParent::RecvProfile(const nsCString& aProfile)
{
#ifdef MOZ_ENABLE_PROFILER_SPS
    if (NS_WARN_IF(!mGatherer)) {
        return true;
    }
    mProfile = aProfile;
    mGatherer->GatheredOOPProfile();
    mGatherer = nullptr;
#endif
    return true;
}

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(ParentIdleListener, nsIObserver)

NS_IMETHODIMP
ParentIdleListener::Observe(nsISupports*, const char* aTopic, const char16_t* aData) {
    mozilla::unused << mParent->SendNotifyIdleObserver(mObserver,
                                                       nsDependentCString(aTopic),
                                                       nsDependentString(aData));
    return NS_OK;
}
