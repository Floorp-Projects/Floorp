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
#include "CrashReporterParent.h"
#include "IHistory.h"
#include "IDBFactory.h"
#include "IndexedDBParent.h"
#include "IndexedDatabaseManager.h"
#include "mozIApplication.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DataStoreService.h"
#include "mozilla/dom/ExternalHelperAppParent.h"
#include "mozilla/dom/PContentBridgeParent.h"
#include "mozilla/dom/PFileDescriptorSetParent.h"
#include "mozilla/dom/PCycleCollectWithLogsParent.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/DOMStorageIPC.h"
#include "mozilla/dom/bluetooth/PBluetoothParent.h"
#include "mozilla/dom/PFMRadioParent.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestParent.h"
#include "mozilla/dom/FileSystemRequestParent.h"
#include "mozilla/dom/GeolocationBinding.h"
#include "mozilla/dom/FileDescriptorSetParent.h"
#include "mozilla/dom/telephony/TelephonyParent.h"
#include "mozilla/dom/time/DateCacheCleaner.h"
#include "SmsParent.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
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
#include "nsDebugImpl.h"
#include "nsDOMFile.h"
#include "nsFrameMessageManager.h"
#include "nsHashPropertyBag.h"
#include "nsIAlertsService.h"
#include "nsIAppsService.h"
#include "nsIClipboard.h"
#include "nsICycleCollectorListener.h"
#include "nsIDOMGeoGeolocation.h"
#include "mozilla/dom/WakeLock.h"
#include "nsIDOMWindow.h"
#include "nsIExternalProtocolService.h"
#include "nsIGfxInfo.h"
#include "nsIIdleService.h"
#include "nsIJSRuntimeService.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMemoryReporter.h"
#include "nsIMozBrowserFrame.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIRemoteBlob.h"
#include "nsIScriptError.h"
#include "nsISiteSecurityService.h"
#include "nsIStyleSheet.h"
#include "nsISupportsPrimitives.h"
#include "nsIURIFixup.h"
#include "nsIWindowWatcher.h"
#include "nsIXULRuntime.h"
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
#include "StructuredCloneUtils.h"
#include "TabParent.h"
#include "URIUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShell.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "gfxPrefs.h"
#include "prio.h"
#include "private/pprio.h"

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
#include "nsIVolumeService.h"
#include "SpeakerManagerService.h"
using namespace mozilla::system;
#endif

#ifdef MOZ_B2G_BT
#include "BluetoothParent.h"
#include "BluetoothService.h"
#endif

#include "JavaScriptParent.h"

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
#include "mozilla/Sandbox.h"
#endif

static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);
static const char* sClipboardTextFlavors[] = { kUnicodeMime };

using base::ChildPrivileges;
using base::KillProcess;
using namespace mozilla::dom::bluetooth;
using namespace mozilla::dom::devicestorage;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::power;
using namespace mozilla::dom::mobilemessage;
using namespace mozilla::dom::telephony;
using namespace mozilla::hal;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;
using namespace mozilla::jsipc;
using namespace mozilla::widget;

#ifdef ENABLE_TESTS

class BackgroundTester MOZ_FINAL : public nsIIPCBackgroundChildCreateCallback,
                                   public nsIObserver
{
    static uint32_t sCallbackCount;

private:
    ~BackgroundTester()
    { }

    virtual void
    ActorCreated(PBackgroundChild* aActor) MOZ_OVERRIDE
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
    ActorFailed() MOZ_OVERRIDE
    {
        MOZ_CRASH("Failed to create a PBackgroundChild actor!");
    }

    NS_IMETHOD
    Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
            MOZ_OVERRIDE
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

#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"

class MemoryReportRequestParent : public PMemoryReportRequestParent
{
public:
    MemoryReportRequestParent();
    virtual ~MemoryReportRequestParent();

    virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

    virtual bool Recv__delete__(const uint32_t& generation, const InfallibleTArray<MemoryReport>& report);
private:
    ContentParent* Owner()
    {
        return static_cast<ContentParent*>(Manager());
    }
};

MemoryReportRequestParent::MemoryReportRequestParent()
{
    MOZ_COUNT_CTOR(MemoryReportRequestParent);
}

void
MemoryReportRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005154
}

bool
MemoryReportRequestParent::Recv__delete__(const uint32_t& generation, const InfallibleTArray<MemoryReport>& childReports)
{
    nsRefPtr<nsMemoryReporterManager> mgr =
        nsMemoryReporterManager::GetOrCreate();
    if (mgr) {
        mgr->HandleChildReports(generation, childReports);
    }
    return true;
}

MemoryReportRequestParent::~MemoryReportRequestParent()
{
    MOZ_COUNT_DTOR(MemoryReportRequestParent);
}

// IPC receiver for remote GC/CC logging.
class CycleCollectWithLogsParent MOZ_FINAL : public PCycleCollectWithLogsParent
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
    virtual bool RecvCloseGCLog() MOZ_OVERRIDE
    {
        unused << mSink->CloseGCLog();
        return true;
    }

    virtual bool RecvCloseCCLog() MOZ_OVERRIDE
    {
        unused << mSink->CloseCCLog();
        return true;
    }

    virtual bool Recv__delete__() MOZ_OVERRIDE
    {
        // Report completion to mCallback only on successful
        // completion of the protocol.
        nsCOMPtr<nsIFile> gcLog, ccLog;
        mSink->GetGcLog(getter_AddRefs(gcLog));
        mSink->GetCcLog(getter_AddRefs(ccLog));
        unused << mCallback->OnDump(gcLog, ccLog, /* parent = */ false);
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE
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
class ContentParentsMemoryReporter MOZ_FINAL : public nsIMemoryReporter
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

// The first content child has ID 1, so the chrome process can have ID 0.
static uint64_t gContentChildID = 1;

// We want the prelaunched process to know that it's for apps, but not
// actually for any app in particular.  Use a magic manifest URL.
// Can't be a static constant.
#define MAGIC_PREALLOCATED_APP_MANIFEST_URL NS_LITERAL_STRING("{{template}}")

static const char* sObserverTopics[] = {
    "xpcom-shutdown",
    NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC,
    "child-memory-reporter-request",
    "memory-pressure",
    "child-gc-request",
    "child-cc-request",
    "child-mmu-request",
    "last-pb-context-exited",
    "file-watcher-update",
#ifdef MOZ_WIDGET_GONK
    NS_VOLUME_STATE_CHANGED,
    "phone-state-changed",
#endif
#ifdef ACCESSIBILITY
    "a11y-init-or-shutdown",
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
                          PROCESS_PRIORITY_BACKGROUND,
                          /* aIsNuwaProcess = */ true);
    nuwaProcess->Init();
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
                          /* isForPreallocated = */ true,
                          PROCESS_PRIORITY_PREALLOC);
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
            process->KillHard();
        }
        else {
            nsAutoString manifestURL;
            if (NS_FAILED(aApp->GetManifestURL(manifestURL))) {
                NS_ERROR("Failed to get manifest URL");
                return nullptr;
            }
            process->TransformPreallocatedIntoApp(manifestURL);
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
                                /* isForPreallocated = */ false,
                                aInitialPriority);
    process->Init();

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

    if (XRE_GetProcessType() != GeckoProcessType_Default) {
        return;
    }

#if defined(MOZ_CONTENT_SANDBOX) && defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 19
    // Require sandboxing on B2G >= KitKat.  This condition must stay
    // in sync with ContentChild::RecvSetProcessSandbox.
    if (!CanSandboxContentProcess()) {
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

    BackgroundChild::Startup();

    // Try to preallocate a process that we can transform into an app later.
    PreallocatedProcessManager::AllocateAfterDelay();

    // Test the PBackground infrastructure on ENABLE_TESTS builds when a special
    // testing preference is set.
    MaybeTestPBackground();
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
        p->TransformPreallocatedIntoBrowser();
    } else {
      // Failed in using the preallocated process: fork from the chrome process.
#ifdef MOZ_NUWA_PROCESS
        if (Preferences::GetBool("dom.ipc.processPrelaunch.enabled", false)) {
            // Wait until the Nuwa process forks a new process.
            return nullptr;
        }
#endif
        p = new ContentParent(/* app = */ nullptr,
                              aOpener,
                              aForBrowserElement,
                              /* isForPreallocated = */ false,
                              aPriority);
        p->Init();
    }

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

bool
ContentParent::PreallocatedProcessReady()
{
#ifdef MOZ_NUWA_PROCESS
    return PreallocatedProcessManager::PreallocatedProcessReady();
#else
    return true;
#endif
}

void
ContentParent::RunAfterPreallocatedProcessReady(nsIRunnable* aRequest)
{
#ifdef MOZ_NUWA_PROCESS
    PreallocatedProcessManager::RunAfterPreallocatedProcessReady(aRequest);
#endif
}

typedef std::map<ContentParent*, std::set<ContentParent*> > GrandchildMap;
static GrandchildMap sGrandchildProcessMap;

std::map<uint64_t, ContentParent*> sContentParentMap;

bool
ContentParent::RecvCreateChildProcess(const IPCTabContext& aContext,
                                      const hal::ProcessPriority& aPriority,
                                      uint64_t* aId,
                                      bool* aIsForApp,
                                      bool* aIsForBrowser)
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
        return false;
    }

    *aId = cp->ChildID();
    *aIsForApp = cp->IsForApp();
    *aIsForBrowser = cp->IsForBrowser();
    sContentParentMap[*aId] = cp;
    auto iter = sGrandchildProcessMap.find(this);
    if (iter == sGrandchildProcessMap.end()) {
        std::set<ContentParent*> children;
        children.insert(cp);
        sGrandchildProcessMap[this] = children;
    } else {
        iter->second.insert(cp);
    }
    return true;
}

bool
ContentParent::AnswerBridgeToChildProcess(const uint64_t& id)
{
    ContentParent* cp = sContentParentMap[id];
    auto iter = sGrandchildProcessMap.find(this);
    if (iter != sGrandchildProcessMap.end() &&
        iter->second.find(cp) != iter->second.end()) {
        return PContentBridge::Bridge(this, cp);
    } else {
        // You can't bridge to a process you didn't open!
        KillHard();
        return false;
    }
}

/*static*/ TabParent*
ContentParent::CreateBrowserOrApp(const TabContext& aContext,
                                  Element* aFrameElement,
                                  ContentParent* aOpenerContentParent)
{
    if (!sCanLaunchSubprocesses) {
        return nullptr;
    }

    ProcessPriority initialPriority = GetInitialProcessPriority(aFrameElement);
    bool isInContentProcess = (XRE_GetProcessType() != GeckoProcessType_Default);

    if (aContext.IsBrowserElement() || !aContext.HasOwnApp()) {
        nsRefPtr<TabParent> tp;
        nsRefPtr<nsIContentParent> constructorSender;
        if (isInContentProcess) {
            MOZ_ASSERT(aContext.IsBrowserElement());
            constructorSender = CreateContentBridgeParent(aContext, initialPriority);
        } else {
          if (aOpenerContentParent) {
            constructorSender = aOpenerContentParent;
          } else {
            constructorSender = GetNewOrUsedBrowserProcess(aContext.IsBrowserElement(),
                                                                    initialPriority);
          }
        }
        if (constructorSender) {
            uint32_t chromeFlags = 0;

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
            nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
            if (loadContext && loadContext->UsePrivateBrowsing()) {
                chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
            }
            bool affectLifetime;
            docShell->GetAffectPrivateSessionLifetime(&affectLifetime);
            if (affectLifetime) {
                chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
            }

            nsRefPtr<TabParent> tp(new TabParent(constructorSender,
                                                 aContext, chromeFlags));
            tp->SetOwnerElement(aFrameElement);

            PBrowserParent* browser = constructorSender->SendPBrowserConstructor(
                // DeallocPBrowserParent() releases this ref.
                tp.forget().take(),
                aContext.AsIPCTabContext(),
                chromeFlags,
                constructorSender->ChildID(),
                constructorSender->IsForApp(),
                constructorSender->IsForBrowser());
            return static_cast<TabParent*>(browser);
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
      parent = CreateContentBridgeParent(aContext, initialPriority);
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
        parent = static_cast<nsIContentParent*>(p);
    }

    if (!parent) {
        return nullptr;
    }

    uint32_t chromeFlags = 0;

    nsRefPtr<TabParent> tp = new TabParent(parent, aContext, chromeFlags);
    tp->SetOwnerElement(aFrameElement);
    PBrowserParent* browser = parent->SendPBrowserConstructor(
        // DeallocPBrowserParent() releases this ref.
        nsRefPtr<TabParent>(tp).forget().take(),
        aContext.AsIPCTabContext(),
        chromeFlags,
        parent->ChildID(),
        parent->IsForApp(),
        parent->IsForBrowser());

    if (isInContentProcess) {
        // Just return directly without the following check in content process.
        return static_cast<TabParent*>(browser);
    }

    if (!browser) {
        // We failed to actually start the PBrowser.  This can happen if the
        // other process has already died.
        if (!reused) {
            // Don't leave a broken ContentParent in the hashtable.
            parent->AsContentParent()->KillHard();
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

    return static_cast<TabParent*>(browser);
}

/*static*/ ContentBridgeParent*
ContentParent::CreateContentBridgeParent(const TabContext& aContext,
                                         const hal::ProcessPriority& aPriority)
{
    ContentChild* child = ContentChild::GetSingleton();
    uint64_t id;
    bool isForApp;
    bool isForBrowser;
    if (!child->SendCreateChildProcess(aContext.AsIPCTabContext(),
                                       aPriority,
                                       &id,
                                       &isForApp,
                                       &isForBrowser)) {
        return nullptr;
    }
    if (!child->CallBridgeToChildProcess(id)) {
        return nullptr;
    }
    ContentBridgeParent* parent = child->GetLastBridge();
    parent->SetChildID(id);
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

namespace {

class SystemMessageHandledListener MOZ_FINAL
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

    NS_IMETHOD Notify(nsITimer* aTimer)
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

} // anonymous namespace

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
ContentParent::TransformPreallocatedIntoApp(const nsAString& aAppManifestURL)
{
    MOZ_ASSERT(IsPreallocated());
    mAppManifestURL = aAppManifestURL;
    TryGetNameFromManifestURL(aAppManifestURL, mAppName);
}

void
ContentParent::TransformPreallocatedIntoBrowser()
{
    // Reset mAppManifestURL, mIsForBrowser and mOSPrivileges for browser.
    mAppManifestURL.Truncate();
    mIsForBrowser = true;
}

void
ContentParent::ShutDownProcess(bool aCloseWithError)
{
    const InfallibleTArray<PIndexedDBParent*>& idbParents =
        ManagedPIndexedDBParent();
    for (uint32_t i = 0; i < idbParents.Length(); ++i) {
        static_cast<IndexedDBParent*>(idbParents[i])->Disconnect();
    }

    // If Close() fails with an error, we'll end up back in this function, but
    // with aCloseWithError = true.  It's important that we call
    // CloseWithError() in this case; see bug 895204.

    if (!aCloseWithError && !mCalledClose) {
        // Close() can only be called once: It kicks off the destruction
        // sequence.
        mCalledClose = true;
        Close();
    }

    if (aCloseWithError && !mCalledCloseWithError) {
        MessageChannel* channel = GetIPCChannel();
        if (channel) {
            mCalledCloseWithError = true;
            channel->CloseWithError();
        }
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

void
ContentParent::ShutDownMessageManager()
{
  if (!mMessageManager) {
    return;
  }

  mMessageManager->ReceiveMessage(
            static_cast<nsIContentFrameMessageManager*>(mMessageManager.get()),
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

    sGrandchildProcessMap.erase(this);
    for (auto iter = sGrandchildProcessMap.begin();
         iter != sGrandchildProcessMap.end();
         iter++) {
        iter->second.erase(this);
    }
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
ContentParent::OnChannelConnected(int32_t pid)
{
    ProcessHandle handle;
    if (!base::OpenPrivilegedProcessHandle(pid, &handle)) {
        NS_WARNING("Can't open handle to child process.");
    }
    else {
        // we need to close the existing handle before setting a new one.
        base::CloseProcessHandle(OtherProcess());
        SetOtherProcess(handle);

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
}

void
ContentParent::ProcessingError(Result what)
{
    if (MsgDropped == what) {
        // Messages sent after crashes etc. are not a big deal.
        return;
    }
    // Other errors are big deals.
    KillHard();
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
} // anonymous namespace

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
        KillHard();
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
    DelayedDeleteContentParentTask(ContentParent* aObj) : mObj(aObj) { }

    // No-op
    NS_IMETHODIMP Run() { return NS_OK; }

    nsRefPtr<ContentParent> mObj;
};

}

void
ContentParent::ActorDestroy(ActorDestroyReason why)
{
    if (mForceKillTask) {
        mForceKillTask->Cancel();
        mForceKillTask = nullptr;
    }

    ShutDownMessageManager();

    nsRefPtr<ContentParent> kungFuDeathGrip(this);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        size_t length = ArrayLength(sObserverTopics);
        for (size_t i = 0; i < length; ++i) {
            obs->RemoveObserver(static_cast<nsIObserver*>(this),
                                sObserverTopics[i]);
        }
    }

    // Tell the memory reporter manager that this ContentParent is going away.
    nsRefPtr<nsMemoryReporterManager> mgr =
        nsMemoryReporterManager::GetOrCreate();
#ifdef MOZ_NUWA_PROCESS
    bool isMemoryChild = !IsNuwaProcess();
#else
    bool isMemoryChild = true;
#endif
    if (mgr && isMemoryChild) {
        mgr->DecrementNumChildProcesses();
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

    MarkAsDead();

    if (obs) {
        nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

        props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), mChildID);

        if (AbnormalShutdown == why) {
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

                crashReporter->GenerateCrashReport(this, nullptr);

                nsAutoString dumpID(crashReporter->ChildDumpID());
                props->SetPropertyAsAString(NS_LITERAL_STRING("dumpID"), dumpID);
            }
#endif
        }
        obs->NotifyObservers((nsIPropertyBag2*) props, "ipc:content-shutdown", nullptr);
    }

    mIdleListeners.Clear();

    // If the child process was terminated due to a SIGKIL, ShutDownProcess
    // might not have been called yet.  We must call it to ensure that our
    // channel is closed, etc.
    ShutDownProcess(/* closeWithError */ true);

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
    auto iter = sGrandchildProcessMap.find(this);
    if (iter != sGrandchildProcessMap.end()) {
        for(auto child = iter->second.begin();
            child != iter->second.end();
            child++) {
            MessageLoop::current()->PostTask(
                FROM_HERE,
                NewRunnableMethod(*child, &ContentParent::ShutDownProcess,
                                  /* closeWithError */ false));
        }
    }
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

    MOZ_ASSERT(!mForceKillTask);
    int32_t timeoutSecs =
        Preferences::GetInt("dom.ipc.tabs.shutdownTimeoutSecs", 5);
    if (timeoutSecs > 0) {
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            mForceKillTask = NewRunnableMethod(this, &ContentParent::KillHard),
            timeoutSecs * 1000);
    }
}

void
ContentParent::NotifyTabDestroyed(PBrowserParent* aTab,
                                  bool aNotifiedDestroying)
{
    if (aNotifiedDestroying) {
        --mNumDestroyingTabs;
    }

    // There can be more than one PBrowser for a given app process
    // because of popup windows.  When the last one closes, shut
    // us down.
    if (ManagedPBrowserParent().Length() == 1) {
        MessageLoop::current()->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &ContentParent::ShutDownProcess,
                              /* force */ false));
    }
}

jsipc::JavaScriptParent*
ContentParent::GetCPOWManager()
{
    if (ManagedPJavaScriptParent().Length()) {
        return static_cast<JavaScriptParent*>(ManagedPJavaScriptParent()[0]);
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
    mForceKillTask = nullptr;
    mNumDestroyingTabs = 0;
    mIsAlive = true;
    mSendPermissionUpdates = false;
    mSendDataStoreInfos = false;
    mCalledClose = false;
    mCalledCloseWithError = false;
    mCalledKillHard = false;
}

ContentParent::ContentParent(mozIApplication* aApp,
                             ContentParent* aOpener,
                             bool aIsForBrowser,
                             bool aIsForPreallocated,
                             ProcessPriority aInitialPriority /* = PROCESS_PRIORITY_FOREGROUND */,
                             bool aIsNuwaProcess /* = false */)
    : nsIContentParent()
    , mOpener(aOpener)
    , mIsForBrowser(aIsForBrowser)
    , mIsNuwaProcess(aIsNuwaProcess)
{
    InitializeMembers();  // Perform common initialization.

    // No more than one of !!aApp, aIsForBrowser, aIsForPreallocated should be
    // true.
    MOZ_ASSERT(!!aApp + aIsForBrowser + aIsForPreallocated <= 1);

    // Only the preallocated process uses Nuwa.
    MOZ_ASSERT_IF(aIsNuwaProcess, aIsForPreallocated);

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

    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    ChildPrivileges privs = aIsNuwaProcess
        ? base::PRIVILEGES_INHERIT
        : base::PRIVILEGES_DEFAULT;
    mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, privs);

    IToplevelProtocol::SetTransport(mSubprocess->GetChannel());

    if (!aIsNuwaProcess) {
        // Tell the memory reporter manager that this ContentParent exists.
        nsRefPtr<nsMemoryReporterManager> mgr =
            nsMemoryReporterManager::GetOrCreate();
        if (mgr) {
            mgr->IncrementNumChildProcesses();
        }
    }

    std::vector<std::string> extraArgs;
    if (aIsNuwaProcess) {
        extraArgs.push_back("-nuwa");
    }
    mSubprocess->LaunchAndWaitForProcessHandle(extraArgs);

    Open(mSubprocess->GetChannel(), mSubprocess->GetOwnedChildProcessHandle());

    InitInternal(aInitialPriority,
                 true, /* Setup off-main thread compositing */
                 true  /* Send registered chrome */);
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
                             const nsTArray<ProtocolFdMapping>& aFds)
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

    // Tell the memory reporter manager that this ContentParent exists.
    nsRefPtr<nsMemoryReporterManager> mgr =
        nsMemoryReporterManager::GetOrCreate();
    if (mgr) {
        mgr->IncrementNumChildProcesses();
    }

    mSubprocess->LaunchAndWaitForProcessHandle();

    // Clone actors routed by aTemplate for this instance.
    IToplevelProtocol::SetTransport(mSubprocess->GetChannel());
    ProtocolCloneContext cloneContext;
    cloneContext.SetContentParent(this);
    CloneManagees(aTemplate, &cloneContext);
    CloneOpenedToplevels(aTemplate, aFds, aPid, &cloneContext);

    Open(mSubprocess->GetChannel(),
         mSubprocess->GetChildProcessHandle());

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
}
#endif  // MOZ_NUWA_PROCESS

ContentParent::~ContentParent()
{
    if (mForceKillTask) {
        mForceKillTask->Cancel();
    }

    if (OtherProcess())
        base::CloseProcessHandle(OtherProcess());

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
}

void
ContentParent::InitInternal(ProcessPriority aInitialPriority,
                            bool aSetupOffMainThreadCompositing,
                            bool aSendRegisteredChrome)
{
    // Set the subprocess's priority.  We do this early on because we're likely
    // /lowering/ the process's CPU and memory priority, which it has inherited
    // from this process.
    //
    // This call can cause us to send IPC messages to the child process, so it
    // must come after the Open() call above.
    ProcessPriorityManager::SetProcessPriority(this, aInitialPriority);

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

#ifndef MOZ_WIDGET_GONK
            if (gfxPrefs::AsyncVideoOOPEnabled()) {
                opened = PImageBridge::Open(this);
                MOZ_ASSERT(opened);
            }
#else
            opened = PImageBridge::Open(this);
            MOZ_ASSERT(opened);
#endif
        }
#ifdef MOZ_WIDGET_GONK
        DebugOnly<bool> opened = PSharedBufferManager::Open(this);
        MOZ_ASSERT(opened);
#endif
    }

    if (aSendRegisteredChrome) {
        nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
        nsChromeRegistryChrome* chromeRegistry =
            static_cast<nsChromeRegistryChrome*>(registrySvc.get());
        chromeRegistry->SendRegisteredChrome(this);
    }

    if (gAppData) {
        nsCString version(gAppData->version);
        nsCString buildID(gAppData->buildID);
        nsCString name(gAppData->name);
        nsCString UAName(gAppData->UAName);

        // Sending all information to content process.
        unused << SendAppInfo(version, buildID, name, UAName);
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
        KillHard();
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
    gfxAndroidPlatform::GetPlatform()->GetFontList(retValue);
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
    NS_ABORT_IF_FALSE(permissionManager,
                 "We have no permissionManager in the Chrome process !");

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    DebugOnly<nsresult> rv = permissionManager->GetEnumerator(getter_AddRefs(enumerator));
    NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "Could not get enumerator!");
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
ContentParent::RecvSetClipboardText(const nsString& text,
                                       const bool& isPrivateData,
                                       const int32_t& whichClipboard)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    nsCOMPtr<nsISupportsString> dataWrapper =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, true);

    rv = dataWrapper->SetData(text);
    NS_ENSURE_SUCCESS(rv, true);

    nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, true);
    trans->Init(nullptr);

    // If our data flavor has already been added, this will fail. But we don't care
    trans->AddDataFlavor(kUnicodeMime);
    trans->SetIsPrivateData(isPrivateData);

    nsCOMPtr<nsISupports> nsisupportsDataWrapper =
        do_QueryInterface(dataWrapper);

    rv = trans->SetTransferData(kUnicodeMime, nsisupportsDataWrapper,
                                text.Length() * sizeof(char16_t));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->SetData(trans, nullptr, whichClipboard);
    return true;
}

bool
ContentParent::RecvGetClipboardText(const int32_t& whichClipboard, nsString* text)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, true);
    trans->Init(nullptr);
    trans->AddDataFlavor(kUnicodeMime);

    clipboard->GetData(trans, whichClipboard);
    nsCOMPtr<nsISupports> tmp;
    uint32_t len;
    rv = trans->GetTransferData(kUnicodeMime, getter_AddRefs(tmp), &len);
    if (NS_FAILED(rv))
        return true;

    nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(tmp);
    // No support for non-text data
    if (!supportsString)
        return true;
    supportsString->GetData(*text);
    return true;
}

bool
ContentParent::RecvEmptyClipboard(const int32_t& whichClipboard)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->EmptyClipboard(whichClipboard);

    return true;
}

bool
ContentParent::RecvClipboardHasText(const int32_t& whichClipboard, bool* hasText)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->HasDataMatchingFlavors(sClipboardTextFlavors, 1,
                                      whichClipboard, hasText);
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

    *showPassword = mozilla::widget::android::GeckoAppShell::GetShowPasswordSetting();
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
ContentParent::RecvAudioChannelGetState(const AudioChannel& aChannel,
                                        const bool& aElementHidden,
                                        const bool& aElementWasHidden,
                                        AudioChannelState* aState)
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    *aState = AUDIO_CHANNEL_STATE_NORMAL;
    if (service) {
        *aState = service->GetStateInternal(aChannel, mChildID,
                                            aElementHidden, aElementWasHidden);
    }
    return true;
}

bool
ContentParent::RecvAudioChannelRegisterType(const AudioChannel& aChannel,
                                            const bool& aWithVideo)
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
        service->RegisterType(aChannel, mChildID, aWithVideo);
    }
    return true;
}

bool
ContentParent::RecvAudioChannelUnregisterType(const AudioChannel& aChannel,
                                              const bool& aElementHidden,
                                              const bool& aWithVideo)
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
        service->UnregisterType(aChannel, aElementHidden, mChildID, aWithVideo);
    }
    return true;
}

bool
ContentParent::RecvAudioChannelChangedNotification()
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
       service->SendAudioChannelChangedNotification(ChildID());
    }
    return true;
}

bool
ContentParent::RecvAudioChannelChangeDefVolChannel(const int32_t& aChannel,
                                                   const bool& aHidden)
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
       service->SetDefaultVolumeControlChannelInternal(aChannel,
                                                       aHidden, mChildID);
    }
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
ContentParent::RecvBroadcastVolume(const nsString& aVolumeName)
{
#ifdef MOZ_WIDGET_GONK
    nsresult rv;
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID, &rv);
    if (vs) {
        vs->BroadcastVolume(aVolumeName);
    }
    return true;
#else
    NS_WARNING("ContentParent::RecvBroadcastVolume shouldn't be called when MOZ_WIDGET_GONK is not defined");
    return false;
#endif
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

        KillHard();
        return false;
    }
    PreallocatedProcessManager::OnNuwaReady();
    return true;
#else
    NS_ERROR("ContentParent::RecvNuwaReady() not implemented!");
    return false;
#endif
}

bool
ContentParent::RecvAddNewProcess(const uint32_t& aPid,
                                 const InfallibleTArray<ProtocolFdMapping>& aFds)
{
#ifdef MOZ_NUWA_PROCESS
    if (!IsNuwaProcess()) {
        NS_ERROR(
            nsPrintfCString(
                "Terminating child process %d for unauthorized IPC message: "
                "AddNewProcess(%d)", Pid(), aPid).get());

        KillHard();
        return false;
    }
    nsRefPtr<ContentParent> content;
    content = new ContentParent(this,
                                MAGIC_PREALLOCATED_APP_MANIFEST_URL,
                                aPid,
                                aFds);
    content->Init();

    size_t numNuwaPrefUpdates = sNuwaPrefUpdates ?
                                sNuwaPrefUpdates->Length() : 0;
    // Resend pref updates to the forked child.
    for (int i = 0; i < numNuwaPrefUpdates; i++) {
        content->SendPreferenceUpdate(sNuwaPrefUpdates->ElementAt(i));
    }
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
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
ContentParent::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const char16_t* aData)
{
    if (!strcmp(aTopic, "xpcom-shutdown") && mSubprocess) {
        ShutDownProcess(/* closeWithError */ false);
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
      if (!SendSetOffline(!strcmp(offline, "true") ? true : false))
          return NS_ERROR_NOT_AVAILABLE;
    }
    // listening for alert notifications
    else if (!strcmp(aTopic, "alertfinished") ||
             !strcmp(aTopic, "alertclickcallback") ||
             !strcmp(aTopic, "alertshow") ) {
        if (!SendNotifyAlertsObserver(nsDependentCString(aTopic),
                                      nsDependentString(aData)))
            return NS_ERROR_NOT_AVAILABLE;
    }
    else if (!strcmp(aTopic, "child-memory-reporter-request")) {
        bool isNuwa = false;
#ifdef MOZ_NUWA_PROCESS
        isNuwa = IsNuwaProcess();
#endif
        if (!isNuwa) {
            unsigned generation;
            int anonymize, minimize, identOffset = -1;
            nsDependentString msg(aData);
            NS_ConvertUTF16toUTF8 cmsg(msg);

            if (sscanf(cmsg.get(),
                       "generation=%x anonymize=%d minimize=%d DMDident=%n",
                       &generation, &anonymize, &minimize, &identOffset) < 3
                || identOffset < 0) {
                return NS_ERROR_INVALID_ARG;
            }
            // The pre-%n part of the string should be all ASCII, so the byte
            // offset in identOffset should be correct as a char offset.
            MOZ_ASSERT(cmsg[identOffset - 1] == '=');
            FileDescriptor dmdFileDesc;
#ifdef MOZ_DMD
            FILE *dmdFile;
            nsAutoString dmdIdent(Substring(msg, identOffset));
            nsresult rv = nsMemoryInfoDumper::OpenDMDFile(dmdIdent, Pid(), &dmdFile);
            if (NS_WARN_IF(NS_FAILED(rv))) {
                // Proceed with the memory report as if DMD were disabled.
                dmdFile = nullptr;
            }
            if (dmdFile) {
                dmdFileDesc = FILEToFileDescriptor(dmdFile);
                fclose(dmdFile);
            }
#endif
            unused << SendPMemoryReportRequestConstructor(
              generation, anonymize, minimize, dmdFileDesc);
        }
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

        unused << SendFilePathUpdate(file->mStorageType, file->mStorageName, file->mPath, creason);
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

        vol->GetName(volName);
        vol->GetMountPoint(mountPoint);
        vol->GetState(&state);
        vol->GetMountGeneration(&mountGeneration);
        vol->GetIsMediaPresent(&isMediaPresent);
        vol->GetIsSharing(&isSharing);
        vol->GetIsFormatting(&isFormatting);
        vol->GetIsFake(&isFake);

        unused << SendFileSystemUpdate(volName, mountPoint, state,
                                       mountGeneration, isMediaPresent,
                                       isSharing, isFormatting, isFake);
    } else if (!strcmp(aTopic, "phone-state-changed")) {
        nsString state(aData);
        unused << SendNotifyPhoneStateChange(state);
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

    return NS_OK;
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

PSharedBufferManagerParent*
ContentParent::AllocPSharedBufferManagerParent(mozilla::ipc::Transport* aTransport,
                                                base::ProcessId aOtherProcess)
{
    return SharedBufferManagerParent::Create(aTransport, aOtherProcess);
}

bool
ContentParent::RecvGetProcessAttributes(uint64_t* aId,
                                        bool* aIsForApp, bool* aIsForBrowser)
{
    *aId = mChildID;
    *aIsForApp = IsForApp();
    *aIsForBrowser = mIsForBrowser;

    return true;
}

bool
ContentParent::RecvGetXPCOMProcessAttributes(bool* aIsOffline)
{
    nsCOMPtr<nsIIOService> io(do_GetIOService());
    NS_ASSERTION(io, "No IO service?");
    DebugOnly<nsresult> rv = io->GetOffline(aIsOffline);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed getting offline?");

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
ContentParent::AllocPBrowserParent(const IPCTabContext& aContext,
                                   const uint32_t& aChromeFlags,
                                   const uint64_t& aId,
                                   const bool& aIsForApp,
                                   const bool& aIsForBrowser)
{
    return nsIContentParent::AllocPBrowserParent(aContext,
                                                 aChromeFlags,
                                                 aId,
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
    delete aActor;
    return true;
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

void
ContentParent::KillHard()
{
    // On Windows, calling KillHard multiple times causes problems - the
    // process handle becomes invalid on the first call, causing a second call
    // to crash our process - more details in bug 890840.
    if (mCalledKillHard) {
        return;
    }
    mCalledKillHard = true;
    mForceKillTask = nullptr;
    // This ensures the process is eventually killed, but doesn't
    // immediately KILLITWITHFIRE because we want to get a minidump if
    // possible.  After a timeout though, the process is forceably
    // killed.
    if (!KillProcess(OtherProcess(), 1, false)) {
        NS_WARNING("failed to kill subprocess!");
    }
    mSubprocess->SetAlreadyDead();
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&ProcessWatcher::EnsureProcessTerminated,
                            OtherProcess(), /*force=*/true));
    //We do clean-up here 
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &ContentParent::ShutDownProcess,
                          /* closeWithError */ true),
        3000);
    // We've now closed the OtherProcess() handle, so must set it to null to
    // prevent our dtor closing it twice.
    SetOtherProcess(0);
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

PIndexedDBParent*
ContentParent::AllocPIndexedDBParent()
{
    return new IndexedDBParent(this);
}

bool
ContentParent::DeallocPIndexedDBParent(PIndexedDBParent* aActor)
{
    delete aActor;
    return true;
}

bool
ContentParent::RecvPIndexedDBConstructor(PIndexedDBParent* aActor)
{
    nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::GetOrCreate();
    NS_ENSURE_TRUE(mgr, false);

    if (!IndexedDatabaseManager::IsMainProcess()) {
        NS_RUNTIMEABORT("Not supported yet!");
    }

    nsRefPtr<IDBFactory> factory;
    nsresult rv = IDBFactory::Create(this, getter_AddRefs(factory));
    NS_ENSURE_SUCCESS(rv, false);

    NS_ASSERTION(factory, "This should never be null!");

    IndexedDBParent* actor = static_cast<IndexedDBParent*>(aActor);
    actor->mFactory = factory;
    actor->mASCIIOrigin = factory->GetASCIIOrigin();

    return true;
}

PMemoryReportRequestParent*
ContentParent::AllocPMemoryReportRequestParent(const uint32_t& aGeneration,
                                               const bool &aAnonymize,
                                               const bool &aMinimizeMemoryUsage,
                                               const FileDescriptor &aDMDFile)
{
    MemoryReportRequestParent* parent = new MemoryReportRequestParent();
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
      SpeakerManagerService::GetSpeakerManagerService();
    if (service) {
        *aValue = service->GetSpeakerStatus();
    }
    return true;
#endif
    return false;
}

bool
ContentParent::RecvSpeakerManagerForceSpeaker(const bool& aEnable)
{
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<SpeakerManagerService> service =
      SpeakerManagerService::GetSpeakerManagerService();
    if (service) {
        service->ForceSpeaker(aEnable, mChildID);
    }
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

    NS_Free(buf);

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
                                         const IPC::Principal& aPrincipal)
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
                                         aCookie, this, aName, aBidi, aLang, aPrincipal);
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
                               const InfallibleTArray<CpowEntry>& aCpows,
                               const IPC::Principal& aPrincipal,
                               InfallibleTArray<nsString>* aRetvals)
{
    return nsIContentParent::RecvSyncMessage(aMsg, aData, aCpows, aPrincipal,
                                             aRetvals);
}

bool
ContentParent::AnswerRpcMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                const InfallibleTArray<CpowEntry>& aCpows,
                                const IPC::Principal& aPrincipal,
                                InfallibleTArray<nsString>* aRetvals)
{
    return nsIContentParent::AnswerRpcMessage(aMsg, aData, aCpows, aPrincipal,
                                              aRetvals);
}

bool
ContentParent::RecvAsyncMessage(const nsString& aMsg,
                                const ClonedMessageData& aData,
                                const InfallibleTArray<CpowEntry>& aCpows,
                                const IPC::Principal& aPrincipal)
{
    return nsIContentParent::RecvAsyncMessage(aMsg, aData, aCpows, aPrincipal);
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
AddGeolocationListener(nsIDOMGeoPositionCallback* watcher, bool highAccuracy)
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
    geo->WatchPosition(watcher, nullptr, options, &retval);
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
    mGeolocationWatchID = AddGeolocationListener(this, aHighAccuracy);
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
        RecvRemoveGeolocationListener();
        mGeolocationWatchID = AddGeolocationListener(this, aEnable);
    }
    return true;
}

NS_IMETHODIMP
ContentParent::HandleEvent(nsIDOMGeoPosition* postion)
{
    unused << SendGeolocationUpdate(GeoPosition(postion));
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
        if (!sPrivateContent->Length()) {
            nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
            obs->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
            delete sPrivateContent;
            sPrivateContent = nullptr;
        }
    }
    return true;
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
    if (aCpows && !GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
        return false;
    }
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
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const uint64_t& aId,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser)
{
    return PContentParent::SendPBrowserConstructor(aActor,
                                                   aContext,
                                                   aChromeFlags,
                                                   aId,
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
ContentParent::RecvKeywordToURI(const nsCString& aKeyword, OptionalInputStreamParams* aPostData,
                                OptionalURIParams* aURI)
{
    nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
    if (!fixup) {
        return true;
    }

    nsCOMPtr<nsIInputStream> postData;
    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(fixup->KeywordToURI(aKeyword, getter_AddRefs(postData),
                                      getter_AddRefs(uri)))) {
        return true;
    }

    nsTArray<mozilla::ipc::FileDescriptor> fds;
    SerializeInputStream(postData, *aPostData, fds);
    MOZ_ASSERT(fds.IsEmpty());

    SerializeURI(uri, *aURI);
    return true;
}

bool
ContentParent::ShouldContinueFromReplyTimeout()
{
    // The only time ContentParent sends blocking messages is for CPOWs, so
    // timeouts should only ever occur in electrolysis-enabled sessions.
    MOZ_ASSERT(BrowserTabsRemote());
    return false;
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
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
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

    nsRefPtr<ParentIdleListener> listener = new ParentIdleListener(this, aObserver);
    mIdleListeners.Put(aObserver, listener);
    idleService->AddIdleObserver(listener, aIdleTimeInS);
    return true;
}

bool
ContentParent::RecvRemoveIdleObserver(const uint64_t& aObserver, const uint32_t& aIdleTimeInS)
{
    nsresult rv;
    nsCOMPtr<nsIIdleService> idleService =
      do_GetService("@mozilla.org/widget/idleservice;1", &rv);
    NS_ENSURE_SUCCESS(rv, false);

    nsRefPtr<ParentIdleListener> listener;
    bool found = mIdleListeners.Get(aObserver, &listener);
    if (found) {
        mIdleListeners.Remove(aObserver);
        idleService->RemoveIdleObserver(listener, aIdleTimeInS);
    }

    return true;
}

bool
ContentParent::RecvBackUpXResources(const FileDescriptor& aXSocketFd)
{
#ifndef MOZ_X11
    NS_RUNTIMEABORT("This message only makes sense on X11 platforms");
#else
    NS_ABORT_IF_FALSE(0 > mChildXSocketFdDup.get(),
                      "Already backed up X resources??");
    mChildXSocketFdDup.forget();
    if (aXSocketFd.IsValid()) {
        mChildXSocketFdDup.reset(aXSocketFd.PlatformHandle());
    }
#endif
    return true;
}

bool
ContentParent::RecvOpenAnonymousTemporaryFile(FileDescriptor *aFD)
{
    PRFileDesc *prfd;
    nsresult rv = NS_OpenAnonymousTemporaryFile(&prfd);
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
    }
    *aFD = FileDescriptor(FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(prfd)));
    // The FileDescriptor object owns a duplicate of the file handle; we
    // must close the original (and clean up the NSPR descriptor).
    PR_Close(prfd);
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
