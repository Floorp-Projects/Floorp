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

#include "chrome/common/process_watcher.h"

#include "AppProcessChecker.h"
#include "AudioChannelService.h"
#include "CrashReporterParent.h"
#include "IHistory.h"
#include "IDBFactory.h"
#include "IndexedDBParent.h"
#include "IndexedDatabaseManager.h"
#include "mozIApplication.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ExternalHelperAppParent.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/DOMStorageIPC.h"
#include "mozilla/dom/bluetooth/PBluetoothParent.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestParent.h"
#include "SmsParent.h"
#include "mozilla/Hal.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsAutoPtr.h"
#include "nsCExternalHandlerService.h"
#include "nsCOMPtr.h"
#include "nsChromeRegistryChrome.h"
#include "nsConsoleMessage.h"
#include "nsConsoleService.h"
#include "nsDebugImpl.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDOMFile.h"
#include "nsExternalHelperAppService.h"
#include "nsFrameMessageManager.h"
#include "nsHashPropertyBag.h"
#include "nsIAlertsService.h"
#include "nsIClipboard.h"
#include "nsIDOMApplicationRegistry.h"
#include "nsIDOMGeoGeolocation.h"
#include "nsIDOMWakeLock.h"
#include "nsIDOMWindow.h"
#include "nsIFilePicker.h"
#include "nsIMemoryReporter.h"
#include "nsIMozBrowserFrame.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIRemoteBlob.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsServiceManagerUtils.h"
#include "nsSystemInfo.h"
#include "nsThreadUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsWidgetsCID.h"
#include "SandboxHal.h"
#include "StructuredCloneUtils.h"
#include "TabParent.h"
#include "URIUtils.h"

#ifdef ANDROID
# include "gfxAndroidPlatform.h"
#endif

#ifdef MOZ_CRASHREPORTER
# include "nsExceptionHandler.h"
# include "nsICrashReporter.h"
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
using namespace mozilla::system;
#endif

#ifdef MOZ_B2G_BT
#include "BluetoothParent.h"
#include "BluetoothService.h"
#endif

#include "Crypto.h"

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/SpeechSynthesisParent.h"
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
using namespace mozilla::hal;
using namespace mozilla::idl;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;

namespace mozilla {
namespace dom {

#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"

// This represents a single measurement taken by a memory reporter in a child
// process and passed to this one.  Its process is non-empty, and its amount is
// fixed.
class ChildMemoryReporter MOZ_FINAL : public MemoryReporterBase
{
public:
    ChildMemoryReporter(const char* aProcess, const char* aPath, int32_t aKind,
                        int32_t aUnits, int64_t aAmount,
                        const char* aDescription)
      : MemoryReporterBase(aPath, aKind, aUnits, aDescription)
      , mProcess(aProcess)
      , mAmount(aAmount)
    {
    }

private:
    int64_t Amount() { return mAmount; }

    nsCString mProcess;
    int64_t   mAmount;
};

class MemoryReportRequestParent : public PMemoryReportRequestParent
{
public:
    MemoryReportRequestParent();
    virtual ~MemoryReportRequestParent();

    virtual bool Recv__delete__(const InfallibleTArray<MemoryReport>& report);
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

bool
MemoryReportRequestParent::Recv__delete__(const InfallibleTArray<MemoryReport>& report)
{
    Owner()->SetChildMemoryReporters(report);
    return true;
}

MemoryReportRequestParent::~MemoryReportRequestParent()
{
    MOZ_COUNT_DTOR(MemoryReportRequestParent);
}

nsDataHashtable<nsStringHashKey, ContentParent*>* ContentParent::gAppContentParents;
nsTArray<ContentParent*>* ContentParent::gNonAppContentParents;
nsTArray<ContentParent*>* ContentParent::gPrivateContent;

// This is true when subprocess launching is enabled.  This is the
// case between StartUp() and ShutDown() or JoinAllSubprocesses().
static bool sCanLaunchSubprocesses;

// The first content child has ID 1, so the chrome process can have ID 0.
static uint64_t gContentChildID = 1;

// Try to keep an app process always preallocated, to get
// initialization off the critical path of app startup.
static bool sKeepAppProcessPreallocated;
static StaticRefPtr<ContentParent> sPreallocatedAppProcess;
static CancelableTask* sPreallocateAppProcessTask;
// This number is fairly arbitrary ... the intention is to put off
// launching another app process until the last one has finished
// loading its content, to reduce CPU/memory/IO contention.
static int sPreallocateDelayMs;
// We want the prelaunched process to know that it's for apps, but not
// actually for any app in particular.  Use a magic manifest URL.
// Can't be a static constant.
#define MAGIC_PREALLOCATED_APP_MANIFEST_URL NS_LITERAL_STRING("{{template}}")

/*static*/ void
ContentParent::PreallocateAppProcess()
{
    MOZ_ASSERT(!sPreallocatedAppProcess);

    if (sPreallocateAppProcessTask) {
        // We were called directly while a delayed task was scheduled.
        sPreallocateAppProcessTask->Cancel();
        sPreallocateAppProcessTask = nullptr;
    }

    sPreallocatedAppProcess =
        new ContentParent(MAGIC_PREALLOCATED_APP_MANIFEST_URL,
                          /*isBrowserElement=*/false,
                          // Final privileges are set when we
                          // transform into our app.
                          base::PRIVILEGES_INHERIT,
                          PROCESS_PRIORITY_BACKGROUND);
    sPreallocatedAppProcess->Init();
}

/*static*/ void
ContentParent::DelayedPreallocateAppProcess()
{
    sPreallocateAppProcessTask = nullptr;
    if (!sPreallocatedAppProcess) {
        PreallocateAppProcess();
    }
}

/*static*/ void
ContentParent::ScheduleDelayedPreallocateAppProcess()
{
    if (!sKeepAppProcessPreallocated || sPreallocateAppProcessTask) {
        return;
    }
    sPreallocateAppProcessTask =
        NewRunnableFunction(DelayedPreallocateAppProcess);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, sPreallocateAppProcessTask, sPreallocateDelayMs);
}

/*static*/ already_AddRefed<ContentParent>
ContentParent::MaybeTakePreallocatedAppProcess(const nsAString& aAppManifestURL,
                                               ChildPrivileges aPrivs,
                                               ProcessPriority aInitialPriority)
{
    nsRefPtr<ContentParent> process = sPreallocatedAppProcess.get();
    sPreallocatedAppProcess = nullptr;

    if (!process) {
        return nullptr;
    }

    if (!process->SetPriorityAndCheckIsAlive(aInitialPriority) ||
        !process->TransformPreallocatedIntoApp(aAppManifestURL, aPrivs)) {
        // Kill the process just in case it's not actually dead; we don't want
        // to "leak" this process!
        process->KillHard();
        return nullptr;
    }

    return process.forget();
}

/*static*/ void
ContentParent::FirstIdle(void)
{
    // The parent has gone idle for the first time. This would be a good
    // time to preallocate an app process.
    ScheduleDelayedPreallocateAppProcess();
}

/*static*/ void
ContentParent::StartUp()
{
    if (XRE_GetProcessType() != GeckoProcessType_Default) {
        return;
    }

    sKeepAppProcessPreallocated =
        Preferences::GetBool("dom.ipc.processPrelaunch.enabled", false);
    if (sKeepAppProcessPreallocated) {
        ClearOnShutdown(&sPreallocatedAppProcess);

        sPreallocateDelayMs = Preferences::GetUint(
            "dom.ipc.processPrelaunch.delayMs", 1000);

        MOZ_ASSERT(!sPreallocateAppProcessTask);

        // Let's not slow down the main process initialization. Wait until
        // the main process goes idle before we preallocate a process
        MessageLoop::current()->PostIdleTask(FROM_HERE, NewRunnableFunction(FirstIdle));
    }

    sCanLaunchSubprocesses = true;
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
ContentParent::GetNewOrUsed(bool aForBrowserElement)
{
    if (!gNonAppContentParents)
        gNonAppContentParents = new nsTArray<ContentParent*>();

    int32_t maxContentProcesses = Preferences::GetInt("dom.ipc.processCount", 1);
    if (maxContentProcesses < 1)
        maxContentProcesses = 1;

    if (gNonAppContentParents->Length() >= uint32_t(maxContentProcesses)) {
        uint32_t idx = rand() % gNonAppContentParents->Length();
        nsRefPtr<ContentParent> p = (*gNonAppContentParents)[idx];
        NS_ASSERTION(p->IsAlive(), "Non-alive contentparent in gNonAppContentParents?");
        return p.forget();
    }

    nsRefPtr<ContentParent> p =
        new ContentParent(/* appManifestURL = */ EmptyString(),
                          aForBrowserElement,
                          base::PRIVILEGES_DEFAULT,
                          PROCESS_PRIORITY_FOREGROUND);
    p->Init();
    gNonAppContentParents->AppendElement(p);
    return p.forget();
}

namespace {
struct SpecialPermission {
    const char* perm;           // an app permission
    ChildPrivileges privs;      // the OS privilege it requires
};
}

static ChildPrivileges
PrivilegesForApp(mozIApplication* aApp)
{
    const SpecialPermission specialPermissions[] = {
        // FIXME/bug 785592: implement a CameraBridge so we don't have
        // to hack around with OS permissions
        { "camera", base::PRIVILEGES_CAMERA }
    };
    for (size_t i = 0; i < ArrayLength(specialPermissions); ++i) {
        const char* const permission = specialPermissions[i].perm;
        bool hasPermission = false;
        if (NS_FAILED(aApp->HasPermission(permission, &hasPermission))) {
            NS_WARNING("Unable to check permissions.  Breakage may follow.");
            break;
        } else if (hasPermission) {
            return specialPermissions[i].privs;
        }
    }
    return base::PRIVILEGES_DEFAULT;
}

/*static*/ ProcessPriority
ContentParent::GetInitialProcessPriority(nsIDOMElement* aFrameElement)
{
    // Frames with mozapptype == critical which are expecting a system message
    // get FOREGROUND_HIGH priority.  All other frames get FOREGROUND priority.

    if (!aFrameElement) {
        return PROCESS_PRIORITY_FOREGROUND;
    }

    nsAutoString appType;
    nsCOMPtr<Element> frameElement = do_QueryInterface(aFrameElement);
    frameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::mozapptype, appType);
    if (appType != NS_LITERAL_STRING("critical")) {
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

/*static*/ TabParent*
ContentParent::CreateBrowserOrApp(const TabContext& aContext,
                                  nsIDOMElement* aFrameElement)
{
    if (!sCanLaunchSubprocesses) {
        return nullptr;
    }

    if (aContext.IsBrowserElement() || !aContext.HasOwnApp()) {
        if (nsRefPtr<ContentParent> cp = GetNewOrUsed(aContext.IsBrowserElement())) {
            nsRefPtr<TabParent> tp(new TabParent(aContext));
            tp->SetOwnerElement(aFrameElement);
            PBrowserParent* browser = cp->SendPBrowserConstructor(
                tp.forget().get(), // DeallocPBrowserParent() releases this ref.
                aContext.AsIPCTabContext(),
                /* chromeFlags */ 0);
            return static_cast<TabParent*>(browser);
        }
        return nullptr;
    }

    // If we got here, we have an app and we're not a browser element.  ownApp
    // shouldn't be null, because we otherwise would have gone into the
    // !HasOwnApp() branch above.
    nsCOMPtr<mozIApplication> ownApp = aContext.GetOwnApp();

    if (!gAppContentParents) {
        gAppContentParents =
            new nsDataHashtable<nsStringHashKey, ContentParent*>();
        gAppContentParents->Init();
    }

    // Each app gets its own ContentParent instance.
    nsAutoString manifestURL;
    if (NS_FAILED(ownApp->GetManifestURL(manifestURL))) {
        NS_ERROR("Failed to get manifest URL");
        return nullptr;
    }

    ProcessPriority initialPriority = GetInitialProcessPriority(aFrameElement);

    nsRefPtr<ContentParent> p = gAppContentParents->Get(manifestURL);
    if (p) {
        // Check that the process is still alive and set its priority.
        // Hopefully the process won't die after this point, if this call
        // succeeds.
        if (!p->SetPriorityAndCheckIsAlive(initialPriority)) {
            p = nullptr;
        }
    }

    if (!p) {
        ChildPrivileges privs = PrivilegesForApp(ownApp);
        p = MaybeTakePreallocatedAppProcess(manifestURL, privs,
                                            initialPriority);
        if (!p) {
            NS_WARNING("Unable to use pre-allocated app process");
            p = new ContentParent(manifestURL, /* isBrowserElement = */ false,
                                  privs, initialPriority);
            p->Init();
        }
        gAppContentParents->Put(manifestURL, p);
    }

    nsRefPtr<TabParent> tp = new TabParent(aContext);
    tp->SetOwnerElement(aFrameElement);
    PBrowserParent* browser = p->SendPBrowserConstructor(
        tp.forget().get(), // DeallocPBrowserParent() releases this ref.
        aContext.AsIPCTabContext(),
        /* chromeFlags */ 0);

    // Send the frame element's mozapptype down to the child process.  This ends
    // up in TabChild::GetAppType().  We have to do this /before/ we acquire the
    // CPU wake lock for this process, because if the child sees that it has a
    // CPU wake lock but its TabChild doesn't have the right mozapptype, it
    // might downgrade its process priority.
    nsCOMPtr<Element> frameElement = do_QueryInterface(aFrameElement);
    if (frameElement) {
      nsAutoString appType;
      frameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::mozapptype, appType);
      unused << browser->SendSetAppType(appType);
    }

    p->MaybeTakeCPUWakeLock(aFrameElement);

    return static_cast<TabParent*>(browser);
}

static PLDHashOperator
AppendToTArray(const nsAString& aKey, ContentParent* aValue, void* aArray)
{
    nsTArray<ContentParent*> *array =
        static_cast<nsTArray<ContentParent*>*>(aArray);
    array->AppendElement(aValue);
    return PL_DHASH_NEXT;
}

void
ContentParent::GetAll(nsTArray<ContentParent*>& aArray)
{
    aArray.Clear();

    if (gNonAppContentParents) {
        aArray.AppendElements(*gNonAppContentParents);
    }

    if (gAppContentParents) {
        gAppContentParents->EnumerateRead(&AppendToTArray, &aArray);
    }

    if (sPreallocatedAppProcess) {
        aArray.AppendElement(sPreallocatedAppProcess);
    }
}

void
ContentParent::Init()
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->AddObserver(this, "xpcom-shutdown", false);
        obs->AddObserver(this, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC, false);
        obs->AddObserver(this, "child-memory-reporter-request", false);
        obs->AddObserver(this, "memory-pressure", false);
        obs->AddObserver(this, "child-gc-request", false);
        obs->AddObserver(this, "child-cc-request", false);
        obs->AddObserver(this, "last-pb-context-exited", false);
        obs->AddObserver(this, "file-watcher-update", false);
#ifdef MOZ_WIDGET_GONK
        obs->AddObserver(this, NS_VOLUME_STATE_CHANGED, false);
#endif
#ifdef ACCESSIBILITY
        obs->AddObserver(this, "a11y-init-or-shutdown", false);
#endif
    }
    Preferences::AddStrongObserver(this, "");
    nsCOMPtr<nsIThreadInternal>
            threadInt(do_QueryInterface(NS_GetCurrentThread()));
    if (threadInt) {
        threadInt->AddObserver(this);
    }
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

    void Init(nsIDOMMozWakeLock* aWakeLock)
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
    static StaticAutoPtr<LinkedList<SystemMessageHandledListener> > sListeners;

    void ShutDown()
    {
        nsRefPtr<SystemMessageHandledListener> kungFuDeathGrip = this;

        mWakeLock->Unlock();

        if (mTimer) {
            mTimer->Cancel();
            mTimer = nullptr;
        }
    }

    nsCOMPtr<nsIDOMMozWakeLock> mWakeLock;
    nsCOMPtr<nsITimer> mTimer;
};

StaticAutoPtr<LinkedList<SystemMessageHandledListener> >
    SystemMessageHandledListener::sListeners;

NS_IMPL_ISUPPORTS1(SystemMessageHandledListener,
                   nsITimerCallback)

} // anonymous namespace

void
ContentParent::SetProcessPriority(ProcessPriority aPriority)
{
    if (!Preferences::GetBool("dom.ipc.processPriorityManager.enabled")) {
        return;
    }

    hal::SetProcessPriority(base::GetProcId(mSubprocess->GetChildProcessHandle()),
                            aPriority);
}

void
ContentParent::MaybeTakeCPUWakeLock(nsIDOMElement* aFrameElement)
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
    nsCOMPtr<nsIDOMMozWakeLock> lock =
        pms->NewWakeLockOnBehalfOfProcess(NS_LITERAL_STRING("cpu"), this);

    // This object's Init() function keeps it alive.
    nsRefPtr<SystemMessageHandledListener> listener =
        new SystemMessageHandledListener();
    listener->Init(lock);
}

bool
ContentParent::SetPriorityAndCheckIsAlive(ProcessPriority aPriority)
{
    SetProcessPriority(aPriority);

    // Now that we've set this process's priority, check whether the process is
    // still alive.  Hopefully we've set the priority to FOREGROUND*, so the
    // process won't unexpectedly crash after this point!
    //
    // It's not legal to call DidProcessCrash on Windows if the process has not
    // terminated yet, so we have to skip this check here.
#ifndef XP_WIN
    bool exited = false;
    base::DidProcessCrash(&exited, mSubprocess->GetChildProcessHandle());
    if (exited) {
        return false;
    }
#endif

    return true;
}

bool
ContentParent::TransformPreallocatedIntoApp(const nsAString& aAppManifestURL,
                                            ChildPrivileges aPrivs)
{
    MOZ_ASSERT(mAppManifestURL == MAGIC_PREALLOCATED_APP_MANIFEST_URL);
    // Clients should think of mAppManifestURL as const ... we're
    // bending the rules here just for the preallocation hack.
    const_cast<nsString&>(mAppManifestURL) = aAppManifestURL;

    return SendSetProcessPrivileges(aPrivs);
}

void
ContentParent::ShutDownProcess()
{
  if (!mIsDestroyed) {
    const InfallibleTArray<PIndexedDBParent*>& idbParents =
      ManagedPIndexedDBParent();
    for (uint32_t i = 0; i < idbParents.Length(); ++i) {
      static_cast<IndexedDBParent*>(idbParents[i])->Disconnect();
    }

    // Close() can only be called once.  It kicks off the
    // destruction sequence.
    Close();
    mIsDestroyed = true;
  }
  // NB: must MarkAsDead() here so that this isn't accidentally
  // returned from Get*() while in the midst of shutdown.
  MarkAsDead();
}

void
ContentParent::MarkAsDead()
{
    if (!mAppManifestURL.IsEmpty()) {
        if (gAppContentParents) {
            gAppContentParents->Remove(mAppManifestURL);
            if (!gAppContentParents->Count()) {
                delete gAppContentParents;
                gAppContentParents = NULL;
            }
        }
    } else if (gNonAppContentParents) {
        gNonAppContentParents->RemoveElement(this);
        if (!gNonAppContentParents->Length()) {
            delete gNonAppContentParents;
            gNonAppContentParents = NULL;
        }
    }

    if (gPrivateContent) {
        gPrivateContent->RemoveElement(this);
        if (!gPrivateContent->Length()) {
            delete gPrivateContent;
            gPrivateContent = NULL;
        }
    }

    mIsAlive = false;
}

void
ContentParent::OnChannelConnected(int32_t pid)
{
    ProcessHandle handle;
    if (!base::OpenPrivilegedProcessHandle(pid, &handle)) {
        NS_WARNING("Can't open handle to child process.");
    }
    else {
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

    nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
    if (ppm) {
      ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                          CHILD_PROCESS_SHUTDOWN_MESSAGE, false,
                          nullptr, nullptr, nullptr);
    }
    nsCOMPtr<nsIThreadObserver>
        kungFuDeathGrip(static_cast<nsIThreadObserver*>(this));
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "xpcom-shutdown");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "memory-pressure");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "child-memory-reporter-request");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC);
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "child-gc-request");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "child-cc-request");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "last-pb-context-exited");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "file-watcher-update");
#ifdef MOZ_WIDGET_GONK
        obs->RemoveObserver(static_cast<nsIObserver*>(this), NS_VOLUME_STATE_CHANGED);
#endif
#ifdef ACCESSIBILITY
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "a11y-init-or-shutdown");
#endif
    }

    if (sPreallocatedAppProcess == this) {
        sPreallocatedAppProcess = nullptr;
    }

    mMessageManager->Disconnect();

    // clear the child memory reporters
    InfallibleTArray<MemoryReport> empty;
    SetChildMemoryReporters(empty);

    // remove the global remote preferences observers
    Preferences::RemoveObserver(this, "");

    RecvRemoveGeolocationListener();

    mConsoleService = nullptr;

    nsCOMPtr<nsIThreadInternal>
        threadInt(do_QueryInterface(NS_GetCurrentThread()));
    if (threadInt)
        threadInt->RemoveObserver(this);
    if (mRunToCompletionDepth)
        mRunToCompletionDepth = 0;

    MarkAsDead();

    if (obs) {
        nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
        props->Init();

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

                crashReporter->GenerateCrashReport(this, NULL);

                nsAutoString dumpID(crashReporter->ChildDumpID());
                props->SetPropertyAsAString(NS_LITERAL_STRING("dumpID"), dumpID);
            }
#endif
        }
        obs->NotifyObservers((nsIPropertyBag2*) props, "ipc:content-shutdown", nullptr);
    }

    MessageLoop::current()->
        PostTask(FROM_HERE,
                 NewRunnableFunction(DelayedDeleteSubprocess, mSubprocess));
    mSubprocess = NULL;

    // IPDL rules require actors to live on past ActorDestroy, but it
    // may be that the kungFuDeathGrip above is the last reference to
    // |this|.  If so, when we go out of scope here, we're deleted and
    // all hell breaks loose.
    //
    // This runnable ensures that a reference to |this| lives on at
    // least until after the current task finishes running.
    NS_DispatchToCurrentThread(new DelayedDeleteContentParentTask(this));
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
            NewRunnableMethod(this, &ContentParent::ShutDownProcess));
    }
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

ContentParent::ContentParent(const nsAString& aAppManifestURL,
                             bool aIsForBrowser,
                             ChildPrivileges aOSPrivileges,
                             ProcessPriority aInitialPriority /* = PROCESS_PRIORITY_FOREGROUND */)
    : mSubprocess(nullptr)
    , mOSPrivileges(aOSPrivileges)
    , mChildID(gContentChildID++)
    , mGeolocationWatchID(-1)
    , mRunToCompletionDepth(0)
    , mShouldCallUnblockChild(false)
    , mAppManifestURL(aAppManifestURL)
    , mForceKillTask(nullptr)
    , mNumDestroyingTabs(0)
    , mIsAlive(true)
    , mIsDestroyed(false)
    , mSendPermissionUpdates(false)
    , mIsForBrowser(aIsForBrowser)
{
    // From this point on, NS_WARNING, NS_ASSERTION, etc. should print out the
    // PID along with the warning.
    nsDebugImpl::SetMultiprocessMode("Parent");

    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content,
                                            aOSPrivileges);

    mSubprocess->LaunchAndWaitForProcessHandle();

    // Set the subprocess's priority.  We do this first because we're likely
    // /lowering/ its CPU and memory priority, which it has inherited from this
    // process.
    SetProcessPriority(aInitialPriority);

    Open(mSubprocess->GetChannel(), mSubprocess->GetChildProcessHandle());

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

        if (Preferences::GetBool("layers.async-video.enabled",false)) {
            opened = PImageBridge::Open(this);
            MOZ_ASSERT(opened);
        }
    }

    nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
    nsChromeRegistryChrome* chromeRegistry =
        static_cast<nsChromeRegistryChrome*>(registrySvc.get());
    chromeRegistry->SendRegisteredChrome(this);
    mMessageManager = nsFrameMessageManager::NewProcessMessageManager(this);

    if (gAppData) {
        nsCString version(gAppData->version);
        nsCString buildID(gAppData->buildID);

        //Sending all information to content process
        unused << SendAppInfo(version, buildID);
    }
}

ContentParent::~ContentParent()
{
    if (mForceKillTask) {
        mForceKillTask->Cancel();
    }

    if (OtherProcess())
        base::CloseProcessHandle(OtherProcess());

    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    // We should be removed from all these lists in ActorDestroy.
    MOZ_ASSERT(!gPrivateContent || !gPrivateContent->Contains(this));
    if (mAppManifestURL.IsEmpty()) {
        MOZ_ASSERT(!gNonAppContentParents ||
                   !gNonAppContentParents->Contains(this));
    } else {
        // In general, we expect gAppContentParents->Get(mAppManifestURL) to be
        // NULL.  But it could be that we created another ContentParent for this
        // app after we did this->ActorDestroy(), so the right check is that
        // gAppContentParent->Get(mAppManifestURL) != this.
        MOZ_ASSERT(!gAppContentParents ||
                   gAppContentParents->Get(mAppManifestURL) != this);
    }
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
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
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
                                text.Length() * sizeof(PRUnichar));
    NS_ENSURE_SUCCESS(rv, true);
    
    clipboard->SetData(trans, NULL, whichClipboard);
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
    
    clipboard->GetData(trans, whichClipboard);
    nsCOMPtr<nsISupports> tmp;
    uint32_t len;
    rv = trans->GetTransferData(kUnicodeMime, getter_AddRefs(tmp), &len);
    if (NS_FAILED(rv))
        return false;

    nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(tmp);
    // No support for non-text data
    if (!supportsString)
        return false;
    supportsString->GetData(*text);
    return true;
}

bool
ContentParent::RecvEmptyClipboard()
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);

    return true;
}

bool
ContentParent::RecvClipboardHasText(bool* hasText)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->HasDataMatchingFlavors(sClipboardTextFlavors, 1, 
                                      nsIClipboard::kGlobalClipboard, hasText);
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
    if (AndroidBridge::Bridge() != nullptr)
        *showPassword = AndroidBridge::Bridge()->GetShowPasswordSetting();
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
    ScheduleDelayedPreallocateAppProcess();
    return true;
}

bool
ContentParent::RecvAudioChannelGetMuted(const AudioChannelType& aType,
                                        const bool& aElementHidden,
                                        const bool& aElementWasHidden,
                                        bool* aValue)
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    *aValue = false;
    if (service) {
        *aValue = service->GetMutedInternal(aType, mChildID,
                                            aElementHidden, aElementWasHidden);
    }
    return true;
}

bool
ContentParent::RecvAudioChannelRegisterType(const AudioChannelType& aType)
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
        service->RegisterType(aType, mChildID);
    }
    return true;
}

bool
ContentParent::RecvAudioChannelUnregisterType(const AudioChannelType& aType,
                                              const bool& aElementHidden)
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
        service->UnregisterType(aType, aElementHidden, mChildID);
    }
    return true;
}

bool
ContentParent::RecvAudioChannelChangedNotification()
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
       service->SendAudioChannelChangedNotification();
    }
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
ContentParent::RecvRecordingDeviceEvents(const nsString& aRecordingStatus)
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->NotifyObservers(nullptr, "recording-device-events", aRecordingStatus.get());
    } else {
        NS_WARNING("Could not get the Observer service for ContentParent::RecvRecordingDeviceEvents.");
    }
    return true;
}

NS_IMPL_THREADSAFE_ISUPPORTS3(ContentParent,
                              nsIObserver,
                              nsIThreadObserver,
                              nsIDOMGeoPositionCallback)

NS_IMETHODIMP
ContentParent::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const PRUnichar* aData)
{
    if (!strcmp(aTopic, "xpcom-shutdown") && mSubprocess) {
        ShutDownProcess();
        NS_ASSERTION(!mSubprocess, "Close should have nulled mSubprocess");
    }

    if (!mIsAlive || !mSubprocess)
        return NS_OK;

    // listening for memory pressure event
    if (!strcmp(aTopic, "memory-pressure") &&
        !NS_LITERAL_STRING("low-memory-no-forward").Equals(aData)) {
        unused << SendFlushMemory(nsDependentString(aData));
    }
    // listening for remotePrefs...
    else if (!strcmp(aTopic, "nsPref:changed")) {
        // We know prefs are ASCII here.
        NS_LossyConvertUTF16toASCII strData(aData);

        PrefSetting pref(strData, null_t(), null_t());
        Preferences::GetPreference(&pref);
        if (!SendPreferenceUpdate(pref)) {
            return NS_ERROR_NOT_AVAILABLE;
        }
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
        unused << SendPMemoryReportRequestConstructor();
    }
    else if (!strcmp(aTopic, "child-gc-request")){
        unused << SendGarbageCollect();
    }
    else if (!strcmp(aTopic, "child-cc-request")){
        unused << SendCycleCollect();
    }
    else if (!strcmp(aTopic, "last-pb-context-exited")) {
        unused << SendLastPrivateDocShellDestroyed();
    }
    else if (!strcmp(aTopic, "file-watcher-update")) {
        nsCString creason;
        CopyUTF16toUTF8(aData, creason);
        DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);

        unused << SendFilePathUpdate(file->mStorageType, file->mPath, creason);
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

        vol->GetName(volName);
        vol->GetMountPoint(mountPoint);
        vol->GetState(&state);
        vol->GetMountGeneration(&mountGeneration);

        unused << SendFileSystemUpdate(volName, mountPoint, state,
                                       mountGeneration);
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
ContentParent::AllocPCompositor(mozilla::ipc::Transport* aTransport,
                                base::ProcessId aOtherProcess)
{
    return CompositorParent::Create(aTransport, aOtherProcess);
}

PImageBridgeParent*
ContentParent::AllocPImageBridge(mozilla::ipc::Transport* aTransport,
                                 base::ProcessId aOtherProcess)
{
    return ImageBridgeParent::Create(aTransport, aOtherProcess);
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

PBrowserParent*
ContentParent::AllocPBrowser(const IPCTabContext& aContext,
                             const uint32_t &aChromeFlags)
{
    unused << aChromeFlags;

    const IPCTabAppBrowserContext& appBrowser = aContext.appBrowserContext();

    // We don't trust the IPCTabContext we receive from the child, so we'll bail
    // if we receive an IPCTabContext that's not a PopupIPCTabContext.
    // (PopupIPCTabContext lets the child process prove that it has access to
    // the app it's trying to open.)
    if (appBrowser.type() != IPCTabAppBrowserContext::TPopupIPCTabContext) {
        NS_ERROR("Unexpected IPCTabContext type.  Aborting AllocPBrowser.");
        return nullptr;
    }

    const PopupIPCTabContext& popupContext = appBrowser.get_PopupIPCTabContext();
    TabParent* opener = static_cast<TabParent*>(popupContext.openerParent());
    if (!opener) {
        NS_ERROR("Got null opener from child; aborting AllocPBrowser.");
        return nullptr;
    }

    // Popup windows of isBrowser frames must be isBrowser if the parent
    // isBrowser.  Allocating a !isBrowser frame with same app ID would allow
    // the content to access data it's not supposed to.
    if (!popupContext.isBrowserElement() && opener->IsBrowserElement()) {
        NS_ERROR("Child trying to escalate privileges!  Aborting AllocPBrowser.");
        return nullptr;
    }

    TabParent* parent = new TabParent(TabContext(aContext));

    // We release this ref in DeallocPBrowser()
    NS_ADDREF(parent);
    return parent;
}

bool
ContentParent::DeallocPBrowser(PBrowserParent* frame)
{
    TabParent* parent = static_cast<TabParent*>(frame);
    NS_RELEASE(parent);
    return true;
}

PDeviceStorageRequestParent*
ContentParent::AllocPDeviceStorageRequest(const DeviceStorageParams& aParams)
{
  nsRefPtr<DeviceStorageRequestParent> result = new DeviceStorageRequestParent(aParams);
  if (!result->EnsureRequiredPermissions(this)) {
      return nullptr;
  }
  result->Dispatch();
  return result.forget().get();
}

bool
ContentParent::DeallocPDeviceStorageRequest(PDeviceStorageRequestParent* doomed)
{
  DeviceStorageRequestParent *parent = static_cast<DeviceStorageRequestParent*>(doomed);
  NS_RELEASE(parent);
  return true;
}

PBlobParent*
ContentParent::AllocPBlob(const BlobConstructorParams& aParams)
{
  return BlobParent::Create(aParams);
}

bool
ContentParent::DeallocPBlob(PBlobParent* aActor)
{
  delete aActor;
  return true;
}

BlobParent*
ContentParent::GetOrCreateActorForBlob(nsIDOMBlob* aBlob)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aBlob, "Null pointer!");

  // XXX This is only safe so long as all blob implementations in our tree
  //     inherit nsDOMFileBase. If that ever changes then this will need to grow
  //     a real interface or something.
  const nsDOMFileBase* blob = static_cast<nsDOMFileBase*>(aBlob);

  // All blobs shared between processes must be immutable.
  nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(aBlob);
  if (!mutableBlob || NS_FAILED(mutableBlob->SetMutable(false))) {
    NS_WARNING("Failed to make blob immutable!");
    return nullptr;
  }

  nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlob);
  if (remoteBlob) {
    BlobParent* actor =
      static_cast<BlobParent*>(
        static_cast<PBlobParent*>(remoteBlob->GetPBlob()));
    NS_ASSERTION(actor, "Null actor?!");

    if (static_cast<ContentParent*>(actor->Manager()) == this) {
      return actor;
    }
  }

  ChildBlobConstructorParams params;

  if (blob->IsSizeUnknown() || blob->IsDateUnknown()) {
    // We don't want to call GetSize or GetLastModifiedDate
    // yet since that may stat a file on the main thread
    // here. Instead we'll learn the size lazily from the
    // other process.
    params = MysteryBlobConstructorParams();
  }
  else {
    nsString contentType;
    nsresult rv = aBlob->GetType(contentType);
    NS_ENSURE_SUCCESS(rv, nullptr);

    uint64_t length;
    rv = aBlob->GetSize(&length);
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
    if (file) {
      FileBlobConstructorParams fileParams;

      rv = file->GetMozLastModifiedDate(&fileParams.modDate());
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = file->GetName(fileParams.name());
      NS_ENSURE_SUCCESS(rv, nullptr);

      fileParams.contentType() = contentType;
      fileParams.length() = length;

      params = fileParams;
    } else {
      NormalBlobConstructorParams blobParams;
      blobParams.contentType() = contentType;
      blobParams.length() = length;
      params = blobParams;
    }
      }

  BlobParent* actor = BlobParent::Create(aBlob);
  NS_ENSURE_TRUE(actor, nullptr);

  if (!SendPBlobConstructor(actor, params)) {
    return nullptr;
  }

  return actor;
}

void
ContentParent::KillHard()
{
    mForceKillTask = nullptr;
    // This ensures the process is eventually killed, but doesn't
    // immediately KILLITWITHFIRE because we want to get a minidump if
    // possible.  After a timeout though, the process is forceably
    // killed.
    if (!KillProcess(OtherProcess(), 1, false)) {
        NS_WARNING("failed to kill subprocess!");
    }
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&ProcessWatcher::EnsureProcessTerminated,
                            OtherProcess(), /*force=*/true));
}

PCrashReporterParent*
ContentParent::AllocPCrashReporter(const NativeThreadId& tid,
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
ContentParent::DeallocPCrashReporter(PCrashReporterParent* crashreporter)
{
  delete crashreporter;
  return true;
}

hal_sandbox::PHalParent*
ContentParent::AllocPHal()
{
    return hal_sandbox::CreateHalParent();
}

bool
ContentParent::DeallocPHal(hal_sandbox::PHalParent* aHal)
{
    delete aHal;
    return true;
}

PIndexedDBParent*
ContentParent::AllocPIndexedDB()
{
  return new IndexedDBParent(this);
}

bool
ContentParent::DeallocPIndexedDB(PIndexedDBParent* aActor)
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
ContentParent::AllocPMemoryReportRequest()
{
  MemoryReportRequestParent* parent = new MemoryReportRequestParent();
  return parent;
}

bool
ContentParent::DeallocPMemoryReportRequest(PMemoryReportRequestParent* actor)
{
  delete actor;
  return true;
}

void
ContentParent::SetChildMemoryReporters(const InfallibleTArray<MemoryReport>& report)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr =
        do_GetService("@mozilla.org/memory-reporter-manager;1");
    for (int32_t i = 0; i < mMemoryReporters.Count(); i++)
        mgr->UnregisterReporter(mMemoryReporters[i]);

    for (uint32_t i = 0; i < report.Length(); i++) {
        nsCString process  = report[i].process();
        nsCString path     = report[i].path();
        int32_t   kind     = report[i].kind();
        int32_t   units    = report[i].units();
        int64_t   amount   = report[i].amount();
        nsCString desc     = report[i].desc();

        nsRefPtr<ChildMemoryReporter> r =
            new ChildMemoryReporter(process.get(), path.get(), kind, units,
                                    amount, desc.get());

        mMemoryReporters.AppendObject(r);
        mgr->RegisterReporter(r);
    }

    nsCOMPtr<nsIObserverService> obs =
        do_GetService("@mozilla.org/observer-service;1");
    if (obs)
        obs->NotifyObservers(nullptr, "child-memory-reporter-update", nullptr);
}

PTestShellParent*
ContentParent::AllocPTestShell()
{
  return new TestShellParent();
}

bool
ContentParent::DeallocPTestShell(PTestShellParent* shell)
{
  delete shell;
  return true;
}
 
PNeckoParent* 
ContentParent::AllocPNecko()
{
    return new NeckoParent();
}

bool 
ContentParent::DeallocPNecko(PNeckoParent* necko)
{
    delete necko;
    return true;
}

PExternalHelperAppParent*
ContentParent::AllocPExternalHelperApp(const OptionalURIParams& uri,
                                       const nsCString& aMimeContentType,
                                       const nsCString& aContentDisposition,
                                       const bool& aForceSave,
                                       const int64_t& aContentLength,
                                       const OptionalURIParams& aReferrer)
{
    ExternalHelperAppParent *parent = new ExternalHelperAppParent(uri, aContentLength);
    parent->AddRef();
    parent->Init(this, aMimeContentType, aContentDisposition, aForceSave, aReferrer);
    return parent;
}

bool
ContentParent::DeallocPExternalHelperApp(PExternalHelperAppParent* aService)
{
    ExternalHelperAppParent *parent = static_cast<ExternalHelperAppParent *>(aService);
    parent->Release();
    return true;
}

PSmsParent*
ContentParent::AllocPSms()
{
    if (!AssertAppProcessPermission(this, "sms")) {
        return nullptr;
    }

    SmsParent* parent = new SmsParent();
    parent->AddRef();
    return parent;
}

bool
ContentParent::DeallocPSms(PSmsParent* aSms)
{
    static_cast<SmsParent*>(aSms)->Release();
    return true;
}

PStorageParent*
ContentParent::AllocPStorage()
{
    return new DOMStorageDBParent();
}

bool
ContentParent::DeallocPStorage(PStorageParent* aActor)
{
    DOMStorageDBParent* child = static_cast<DOMStorageDBParent*>(aActor);
    child->ReleaseIPDLReference();
    return true;
}

PBluetoothParent*
ContentParent::AllocPBluetooth()
{
#ifdef MOZ_B2G_BT
    if (!AssertAppProcessPermission(this, "bluetooth")) {
        return nullptr;
    }
    return new mozilla::dom::bluetooth::BluetoothParent();
#else
    MOZ_NOT_REACHED("No support for bluetooth on this platform!");
    return nullptr;
#endif
}

bool
ContentParent::DeallocPBluetooth(PBluetoothParent* aActor)
{
#ifdef MOZ_B2G_BT
    delete aActor;
    return true;
#else
    MOZ_NOT_REACHED("No support for bluetooth on this platform!");
    return false;
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
    MOZ_NOT_REACHED("No support for bluetooth on this platform!");
    return false;
#endif
}

PSpeechSynthesisParent*
ContentParent::AllocPSpeechSynthesis()
{
#ifdef MOZ_WEBSPEECH
    return new mozilla::dom::SpeechSynthesisParent();
#else
    return nullptr;
#endif
}

bool
ContentParent::DeallocPSpeechSynthesis(PSpeechSynthesisParent* aActor)
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

void
ContentParent::ReportChildAlreadyBlocked()
{
    if (!mRunToCompletionDepth) {
#ifdef DEBUG
        printf("Running to completion...\n");
#endif
        mRunToCompletionDepth = 1;
        mShouldCallUnblockChild = false;
    }
}
    
bool
ContentParent::RequestRunToCompletion()
{
    if (!mRunToCompletionDepth &&
        BlockChild()) {
#ifdef DEBUG
        printf("Running to completion...\n");
#endif
        mRunToCompletionDepth = 1;
        mShouldCallUnblockChild = true;
    }
    return !!mRunToCompletionDepth;
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
ContentParent::RecvShowFilePicker(const int16_t& mode,
                                  const int16_t& selectedType,
                                  const bool& addToRecentDocs,
                                  const nsString& title,
                                  const nsString& defaultFile,
                                  const nsString& defaultExtension,
                                  const InfallibleTArray<nsString>& filters,
                                  const InfallibleTArray<nsString>& filterNames,
                                  InfallibleTArray<nsString>* files,
                                  int16_t* retValue,
                                  nsresult* result)
{
    nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
    if (!filePicker) {
        *result = NS_ERROR_NOT_AVAILABLE;
        return true;
    }

    // as the parent given to the content process would be meaningless in this
    // process, always use active window as the parent
    nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    nsCOMPtr<nsIDOMWindow> window;
    ww->GetActiveWindow(getter_AddRefs(window));

    // initialize the "real" picker with all data given
    *result = filePicker->Init(window, title, mode);
    if (NS_FAILED(*result))
        return true;

    filePicker->SetAddToRecentDocs(addToRecentDocs);

    uint32_t count = filters.Length();
    for (uint32_t i = 0; i < count; ++i) {
        filePicker->AppendFilter(filterNames[i], filters[i]);
    }

    filePicker->SetDefaultString(defaultFile);
    filePicker->SetDefaultExtension(defaultExtension);
    filePicker->SetFilterIndex(selectedType);

    // and finally open the dialog
    *result = filePicker->Show(retValue);
    if (NS_FAILED(*result))
        return true;

    if (mode == nsIFilePicker::modeOpenMultiple) {
        nsCOMPtr<nsISimpleEnumerator> fileIter;
        *result = filePicker->GetFiles(getter_AddRefs(fileIter));

        nsCOMPtr<nsIFile> singleFile;
        bool loop = true;
        while (NS_SUCCEEDED(fileIter->HasMoreElements(&loop)) && loop) {
            fileIter->GetNext(getter_AddRefs(singleFile));
            if (singleFile) {
                nsAutoString filePath;
                singleFile->GetPath(filePath);
                files->AppendElement(filePath);
            }
        }
        return true;
    }
    nsCOMPtr<nsIFile> file;
    filePicker->GetFile(getter_AddRefs(file));

    // even with NS_OK file can be null if nothing was selected 
    if (file) {                                 
        nsAutoString filePath;
        file->GetPath(filePath);
        files->AppendElement(filePath);
    }

    return true;
}

bool
ContentParent::RecvGetRandomValues(const uint32_t& length,
                                   InfallibleTArray<uint8_t>* randomValues)
{
    uint8_t* buf = Crypto::GetRandomValues(length);

    randomValues->SetCapacity(length);
    randomValues->SetLength(length);

    memcpy(randomValues->Elements(), buf, length);

    NS_Free(buf);

    return true;
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

/* void onDispatchedEvent (in nsIThreadInternal thread); */
NS_IMETHODIMP
ContentParent::OnDispatchedEvent(nsIThreadInternal *thread)
{
   NS_NOTREACHED("OnDispatchedEvent unimplemented");
   return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onProcessNextEvent (in nsIThreadInternal thread, in boolean mayWait, in unsigned long recursionDepth); */
NS_IMETHODIMP
ContentParent::OnProcessNextEvent(nsIThreadInternal *thread,
                                  bool mayWait,
                                  uint32_t recursionDepth)
{
    if (mRunToCompletionDepth)
        ++mRunToCompletionDepth;

    return NS_OK;
}

/* void afterProcessNextEvent (in nsIThreadInternal thread, in unsigned long recursionDepth); */
NS_IMETHODIMP
ContentParent::AfterProcessNextEvent(nsIThreadInternal *thread,
                                     uint32_t recursionDepth)
{
    if (mRunToCompletionDepth &&
        !--mRunToCompletionDepth) {
#ifdef DEBUG
            printf("... ran to completion.\n");
#endif
            if (mShouldCallUnblockChild) {
                mShouldCallUnblockChild = false;
                UnblockChild();
            }
    }

    return NS_OK;
}

bool
ContentParent::RecvShowAlertNotification(const nsString& aImageUrl, const nsString& aTitle,
                                         const nsString& aText, const bool& aTextClickable,
                                         const nsString& aCookie, const nsString& aName,
                                         const nsString& aBidi, const nsString& aLang)
{
    if (!AssertAppProcessPermission(this, "desktop-notification")) {
        return false;
    }
    nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_ALERTSERVICE_CONTRACTID));
    if (sysAlerts) {
        sysAlerts->ShowAlertNotification(aImageUrl, aTitle, aText, aTextClickable,
                                         aCookie, this, aName, aBidi, aLang);
    }

    return true;
}

bool
ContentParent::RecvCloseAlert(const nsString& aName)
{
    if (!AssertAppProcessPermission(this, "desktop-notification")) {
        return false;
    }
    nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_ALERTSERVICE_CONTRACTID));
    if (sysAlerts) {
        sysAlerts->CloseAlert(aName);
    }

    return true;
}

bool
ContentParent::RecvTestPermissionFromPrincipal(const IPC::Principal& aPrincipal,
                                               const nsCString& aType,
                                               uint32_t* permission)
{
    nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(permissionManager, false);

    nsresult rv = permissionManager->TestPermissionFromPrincipal(aPrincipal,
                                                                 aType.get(),
                                                                 permission);
    NS_ENSURE_SUCCESS(rv, false);

    return true;
}

bool
ContentParent::RecvSyncMessage(const nsString& aMsg,
                               const ClonedMessageData& aData,
                               InfallibleTArray<nsString>* aRetvals)
{
  nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForParent(aData);
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                        aMsg, true, &cloneData, nullptr, aRetvals);
  }
  return true;
}

bool
ContentParent::RecvAsyncMessage(const nsString& aMsg,
                                      const ClonedMessageData& aData)
{
  nsRefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForParent(aData);
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                        aMsg, false, &cloneData, nullptr, nullptr);
  }
  return true;
}

bool
ContentParent::RecvFilePathUpdateNotify(const nsString& aType,
                                        const nsString& aFilePath,
                                        const nsCString& aReason)
{
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(aType, aFilePath);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (!obs) {
        return false;
    }
    obs->NotifyObservers(dsf, "file-watcher-update", NS_ConvertASCIItoUTF16(aReason).get());
    return true;
}

static int32_t
AddGeolocationListener(nsIDOMGeoPositionCallback* watcher, bool highAccuracy)
{
  nsCOMPtr<nsIDOMGeoGeolocation> geo = do_GetService("@mozilla.org/geolocation;1");
  if (!geo) {
    return -1;
  }

  GeoPositionOptions* options = new GeoPositionOptions();
  options->enableHighAccuracy = highAccuracy;
  int32_t retval = 1;
  geo->WatchPosition(watcher, nullptr, options, &retval);
  return retval;
}

bool
ContentParent::RecvAddGeolocationListener(const IPC::Principal& aPrincipal,
                                          const bool& aHighAccuracy)
{
#ifdef MOZ_PERMISSIONS
  if (Preferences::GetBool("geo.testing.ignore_ipc_principal", false) == false) {
    nsIPrincipal* principal = aPrincipal;
    if (principal == nullptr) {
      KillHard();
      return true;
    }

    uint32_t principalAppId;
    nsresult rv = principal->GetAppId(&principalAppId);
    if (NS_FAILED(rv)) {
      return true;
    }

    bool found = false;
    const InfallibleTArray<PBrowserParent*>& browsers = ManagedPBrowserParent();
    for (uint32_t i = 0; i < browsers.Length(); ++i) {
      TabParent* tab = static_cast<TabParent*>(browsers[i]);
      nsCOMPtr<mozIApplication> app = tab->GetOwnOrContainingApp();
      uint32_t appId;
      app->GetLocalId(&appId);
      if (appId == principalAppId) {
        found = true;
        break;
      }
    }

    if (!found) {
      return true;
    }

    // We need to ensure that this permission has been set.
    // If it hasn't, just noop
    nsCOMPtr<nsIPermissionManager> pm = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
    if (!pm) {
      return false;
    }

    uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
    rv = pm->TestPermissionFromPrincipal(principal, "geolocation", &permission);
    if (NS_FAILED(rv) || permission != nsIPermissionManager::ALLOW_ACTION) {
      KillHard();
      return true;
    }
  }
#endif

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
  if (!gPrivateContent)
    gPrivateContent = new nsTArray<ContentParent*>();
  if (aExist) {
    gPrivateContent->AppendElement(this);
  } else {
    gPrivateContent->RemoveElement(this);
    if (!gPrivateContent->Length()) {
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      obs->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
      delete gPrivateContent;
      gPrivateContent = NULL;
    }
  }
  return true;
}

bool
ContentParent::DoSendAsyncMessage(const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForParent(this, aData, data)) {
    return false;
  }
  return SendAsyncMessage(nsString(aMessage), data);
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
ContentParent::RecvSystemMessageHandled()
{
    SystemMessageHandledListener::OnSystemMessageHandled();
    return true;
}

} // namespace dom
} // namespace mozilla
