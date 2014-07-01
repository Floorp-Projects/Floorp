/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

#ifdef MOZ_WIDGET_QT
#include "nsQAppInstance.h"
#endif

#include "ContentChild.h"
#include "CrashReporterChild.h"
#include "FileDescriptorSetChild.h"
#include "TabChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ContentBridgeChild.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/DOMStorageIPC.h"
#include "mozilla/dom/ExternalHelperAppChild.h"
#include "mozilla/dom/PCrashReporterChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/asmjscache/PAsmJSCacheEntryChild.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/PCompositorChild.h"
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "mozilla/net/NeckoChild.h"

#if defined(MOZ_CONTENT_SANDBOX)
#if defined(XP_WIN)
#define TARGET_SANDBOX_EXPORTS
#include "mozilla/sandboxTarget.h"
#elif defined(XP_LINUX)
#include "mozilla/Sandbox.h"
#endif
#endif

#include "mozilla/unused.h"

#include "nsIConsoleListener.h"
#include "nsICycleCollectorListener.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMemoryReporter.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleSheetService.h"
#include "nsXULAppAPI.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsJSEnvironment.h"
#include "SandboxHal.h"
#include "nsDebugImpl.h"
#include "nsHashPropertyBag.h"
#include "nsLayoutStylesheetCache.h"
#include "nsIJSRuntimeService.h"
#include "nsThreadManager.h"
#include "nsAnonymousTemporaryFile.h"

#include "IHistory.h"
#include "nsNetUtil.h"

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/task.h"

#include "nsChromeRegistryContent.h"
#include "nsFrameMessageManager.h"

#include "nsIGeolocationProvider.h"
#include "mozilla/dom/PMemoryReportRequestChild.h"
#include "mozilla/dom/PCycleCollectWithLogsChild.h"

#ifdef MOZ_PERMISSIONS
#include "nsIScriptSecurityManager.h"
#include "nsPermission.h"
#include "nsPermissionManager.h"
#endif

#include "PermissionMessageUtils.h"

#if defined(MOZ_WIDGET_ANDROID)
#include "APKOpen.h"
#endif

#if defined(MOZ_WIDGET_GONK)
#include "nsVolume.h"
#include "nsVolumeService.h"
#include "SpeakerManagerService.h"
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#endif

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif

#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

#ifdef MOZ_NUWA_PROCESS
#include <setjmp.h>
#include "ipc/Nuwa.h"
#endif

#include "mozilla/dom/indexedDB/PIndexedDBChild.h"
#include "mozilla/dom/mobilemessage/SmsChild.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestChild.h"
#include "mozilla/dom/PFileSystemRequestChild.h"
#include "mozilla/dom/FileSystemTaskBase.h"
#include "mozilla/dom/bluetooth/PBluetoothChild.h"
#include "mozilla/dom/PFMRadioChild.h"
#include "mozilla/ipc/InputStreamUtils.h"

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/PSpeechSynthesisChild.h"
#endif

#include "nsDOMFile.h"
#include "nsIRemoteBlob.h"
#include "ProcessUtils.h"
#include "StructuredCloneUtils.h"
#include "URIUtils.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsDeviceStorage.h"
#include "AudioChannelService.h"
#include "JavaScriptChild.h"
#include "mozilla/dom/DataStoreService.h"
#include "mozilla/dom/telephony/PTelephonyChild.h"
#include "mozilla/dom/time/DateCacheCleaner.h"
#include "mozilla/net/NeckoMessageUtils.h"

using namespace base;
using namespace mozilla;
using namespace mozilla::docshell;
using namespace mozilla::dom::bluetooth;
using namespace mozilla::dom::devicestorage;
using namespace mozilla::dom::ipc;
using namespace mozilla::dom::mobilemessage;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::telephony;
using namespace mozilla::hal_sandbox;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;
using namespace mozilla::jsipc;
#if defined(MOZ_WIDGET_GONK)
using namespace mozilla::system;
#endif

#ifdef MOZ_NUWA_PROCESS
static bool sNuwaForking = false;

// The size of the reserved stack (in unsigned ints). It's used to reserve space
// to push sigsetjmp() in NuwaCheckpointCurrentThread() to higher in the stack
// so that after it returns and do other work we don't garble the stack we want
// to preserve in NuwaCheckpointCurrentThread().
#define RESERVED_INT_STACK 128

// A sentinel value for checking whether RESERVED_INT_STACK is large enough.
#define STACK_SENTINEL_VALUE 0xdeadbeef
#endif

namespace mozilla {
namespace dom {

class MemoryReportRequestChild : public PMemoryReportRequestChild,
                                 public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS

    MemoryReportRequestChild(uint32_t aGeneration, bool aAnonymize,
                             const nsAString& aDMDDumpIdent);
    NS_IMETHOD Run();
private:
    virtual ~MemoryReportRequestChild();

    uint32_t mGeneration;
    bool     mAnonymize;
    nsString mDMDDumpIdent;
};

NS_IMPL_ISUPPORTS(MemoryReportRequestChild, nsIRunnable)

MemoryReportRequestChild::MemoryReportRequestChild(
    uint32_t aGeneration, bool aAnonymize, const nsAString& aDMDDumpIdent)
  : mGeneration(aGeneration), mAnonymize(aAnonymize),
    mDMDDumpIdent(aDMDDumpIdent)
{
    MOZ_COUNT_CTOR(MemoryReportRequestChild);
}

MemoryReportRequestChild::~MemoryReportRequestChild()
{
    MOZ_COUNT_DTOR(MemoryReportRequestChild);
}

// IPC sender for remote GC/CC logging.
class CycleCollectWithLogsChild MOZ_FINAL
    : public PCycleCollectWithLogsChild
    , public nsICycleCollectorLogSink
{
public:
    NS_DECL_ISUPPORTS

    CycleCollectWithLogsChild(const FileDescriptor& aGCLog,
                              const FileDescriptor& aCCLog)
    {
        mGCLog = FileDescriptorToFILE(aGCLog, "w");
        mCCLog = FileDescriptorToFILE(aCCLog, "w");
    }

    NS_IMETHOD Open(FILE** aGCLog, FILE** aCCLog) MOZ_OVERRIDE
    {
        if (NS_WARN_IF(!mGCLog) || NS_WARN_IF(!mCCLog)) {
            return NS_ERROR_FAILURE;
        }
        *aGCLog = mGCLog;
        *aCCLog = mCCLog;
        return NS_OK;
    }

    NS_IMETHOD CloseGCLog() MOZ_OVERRIDE
    {
        MOZ_ASSERT(mGCLog);
        fclose(mGCLog);
        mGCLog = nullptr;
        SendCloseGCLog();
        return NS_OK;
    }

    NS_IMETHOD CloseCCLog() MOZ_OVERRIDE
    {
        MOZ_ASSERT(mCCLog);
        fclose(mCCLog);
        mCCLog = nullptr;
        SendCloseCCLog();
        return NS_OK;
    }

    NS_IMETHOD GetFilenameIdentifier(nsAString& aIdentifier) MOZ_OVERRIDE
    {
        return UnimplementedProperty();
    }

    NS_IMETHOD SetFilenameIdentifier(const nsAString& aIdentifier) MOZ_OVERRIDE
    {
        return UnimplementedProperty();
    }

    NS_IMETHOD GetProcessIdentifier(int32_t *aIdentifier) MOZ_OVERRIDE
    {
        return UnimplementedProperty();
    }

    NS_IMETHOD SetProcessIdentifier(int32_t aIdentifier) MOZ_OVERRIDE
    {
        return UnimplementedProperty();
    }

    NS_IMETHOD GetGcLog(nsIFile** aPath) MOZ_OVERRIDE
    {
        return UnimplementedProperty();
    }

    NS_IMETHOD GetCcLog(nsIFile** aPath) MOZ_OVERRIDE
    {
        return UnimplementedProperty();
    }

private:
    ~CycleCollectWithLogsChild()
    {
        if (mGCLog) {
            fclose(mGCLog);
            mGCLog = nullptr;
        }
        if (mCCLog) {
            fclose(mCCLog);
            mCCLog = nullptr;
        }
        // The XPCOM refcount drives the IPC lifecycle; see also
        // DeallocPCycleCollectWithLogsChild.
        unused << Send__delete__(this);
    }

    nsresult UnimplementedProperty()
    {
        MOZ_ASSERT(false, "This object is a remote GC/CC logger;"
                   " this property isn't meaningful.");
        return NS_ERROR_UNEXPECTED;
    }

    FILE* mGCLog;
    FILE* mCCLog;
};

NS_IMPL_ISUPPORTS(CycleCollectWithLogsChild, nsICycleCollectorLogSink);

class AlertObserver
{
public:

    AlertObserver(nsIObserver *aObserver, const nsString& aData)
        : mObserver(aObserver)
        , mData(aData)
    {
    }

    ~AlertObserver() {}

    bool ShouldRemoveFrom(nsIObserver* aObserver,
                          const nsString& aData) const
    {
        return (mObserver == aObserver &&
                mData == aData);
    }

    bool Observes(const nsString& aData) const
    {
        return mData.Equals(aData);
    }

    bool Notify(const nsCString& aType) const
    {
        mObserver->Observe(nullptr, aType.get(), mData.get());
        return true;
    }

private:
    nsCOMPtr<nsIObserver> mObserver;
    nsString mData;
};

class ConsoleListener MOZ_FINAL : public nsIConsoleListener
{
public:
    ConsoleListener(ContentChild* aChild)
    : mChild(aChild) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSICONSOLELISTENER

private:
    ~ConsoleListener() {}

    ContentChild* mChild;
    friend class ContentChild;
};

NS_IMPL_ISUPPORTS(ConsoleListener, nsIConsoleListener)

NS_IMETHODIMP
ConsoleListener::Observe(nsIConsoleMessage* aMessage)
{
    if (!mChild)
        return NS_OK;
    
    nsCOMPtr<nsIScriptError> scriptError = do_QueryInterface(aMessage);
    if (scriptError) {
        nsString msg, sourceName, sourceLine;
        nsXPIDLCString category;
        uint32_t lineNum, colNum, flags;

        nsresult rv = scriptError->GetErrorMessage(msg);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = scriptError->GetSourceName(sourceName);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = scriptError->GetSourceLine(sourceLine);
        NS_ENSURE_SUCCESS(rv, rv);

        // Before we send the error to the parent process (which
        // involves copying the memory), truncate any long lines.  CSS
        // errors in particular share the memory for long lines with
        // repeated errors, but the IPC communication we're about to do
        // will break that sharing, so we better truncate now.
        if (sourceLine.Length() > 1000) {
            sourceLine.Truncate(1000);
        }

        rv = scriptError->GetCategory(getter_Copies(category));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = scriptError->GetLineNumber(&lineNum);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = scriptError->GetColumnNumber(&colNum);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = scriptError->GetFlags(&flags);
        NS_ENSURE_SUCCESS(rv, rv);
        mChild->SendScriptError(msg, sourceName, sourceLine,
                               lineNum, colNum, flags, category);
        return NS_OK;
    }

    nsXPIDLString msg;
    nsresult rv = aMessage->GetMessageMoz(getter_Copies(msg));
    NS_ENSURE_SUCCESS(rv, rv);
    mChild->SendConsoleMessage(msg);
    return NS_OK;
}

class SystemMessageHandledObserver MOZ_FINAL : public nsIObserver
{
    ~SystemMessageHandledObserver() {}

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    void Init();
};

void SystemMessageHandledObserver::Init()
{
    nsCOMPtr<nsIObserverService> os =
        mozilla::services::GetObserverService();

    if (os) {
        os->AddObserver(this, "handle-system-messages-done",
                        /* ownsWeak */ false);
    }
}

NS_IMETHODIMP
SystemMessageHandledObserver::Observe(nsISupports* aSubject,
                                      const char* aTopic,
                                      const char16_t* aData)
{
    if (ContentChild::GetSingleton()) {
        ContentChild::GetSingleton()->SendSystemMessageHandled();
    }
    return NS_OK;
}

NS_IMPL_ISUPPORTS(SystemMessageHandledObserver, nsIObserver)

class BackgroundChildPrimer MOZ_FINAL :
  public nsIIPCBackgroundChildCreateCallback
{
public:
    BackgroundChildPrimer()
    { }

    NS_DECL_ISUPPORTS

private:
    ~BackgroundChildPrimer()
    { }

    virtual void
    ActorCreated(PBackgroundChild* aActor) MOZ_OVERRIDE
    {
        MOZ_ASSERT(aActor, "Failed to create a PBackgroundChild actor!");
    }

    virtual void
    ActorFailed() MOZ_OVERRIDE
    {
        MOZ_CRASH("Failed to create a PBackgroundChild actor!");
    }
};

NS_IMPL_ISUPPORTS(BackgroundChildPrimer, nsIIPCBackgroundChildCreateCallback)

ContentChild* ContentChild::sSingleton;

// Performs initialization that is not fork-safe, i.e. that must be done after
// forking from the Nuwa process.
static void
InitOnContentProcessCreated(bool aAfterNuwaFork)
{
#ifdef MOZ_NUWA_PROCESS
    // Wait until we are forked from Nuwa
    if (!aAfterNuwaFork &&
        Preferences::GetBool("dom.ipc.processPrelaunch.enabled", false)) {
        return;
    }
#else
    unused << aAfterNuwaFork;
#endif
    // This will register cross-process observer.
    mozilla::dom::time::InitializeDateCacheCleaner();
}

ContentChild::ContentChild()
 : mID(uint64_t(-1))
#ifdef ANDROID
   ,mScreenSize(0, 0)
#endif
   , mCanOverrideProcessName(true)
{
    // This process is a content process, so it's clearly running in
    // multiprocess mode!
    nsDebugImpl::SetMultiprocessMode("Child");
}

ContentChild::~ContentChild()
{
}

NS_INTERFACE_MAP_BEGIN(ContentChild)
  NS_INTERFACE_MAP_ENTRY(nsIContentChild)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

bool
ContentChild::Init(MessageLoop* aIOLoop,
                   base::ProcessHandle aParentHandle,
                   IPC::Channel* aChannel)
{
#ifdef MOZ_WIDGET_GTK
    // sigh
    gtk_init(nullptr, nullptr);
#endif

#ifdef MOZ_WIDGET_QT
    // sigh, seriously
    nsQAppInstance::AddRef();
#endif

#ifdef MOZ_X11
    // Do this after initializing GDK, or GDK will install its own handler.
    XRE_InstallX11ErrorHandler();
#endif

#ifdef MOZ_NUWA_PROCESS
    SetTransport(aChannel);
#endif

    NS_ASSERTION(!sSingleton, "only one ContentChild per child");

    // Once we start sending IPC messages, we need the thread manager to be
    // initialized so we can deal with the responses. Do that here before we
    // try to construct the crash reporter.
    nsresult rv = nsThreadManager::get()->Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
    }

    Open(aChannel, aParentHandle, aIOLoop);
    sSingleton = this;

#ifdef MOZ_X11
    // Send the parent our X socket to act as a proxy reference for our X
    // resources.
    int xSocketFd = ConnectionNumber(DefaultXDisplay());
    SendBackUpXResources(FileDescriptor(xSocketFd));
#endif

#ifdef MOZ_CRASHREPORTER
    SendPCrashReporterConstructor(CrashReporter::CurrentThreadId(),
                                  XRE_GetProcessType());
#endif

    GetCPOWManager();

    SendGetProcessAttributes(&mID, &mIsForApp, &mIsForBrowser);
    InitProcessAttributes();

    return true;
}

void
ContentChild::InitProcessAttributes()
{
#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        SetProcessName(NS_LITERAL_STRING("(Nuwa)"), false);
        return;
    }
#endif
    if (mIsForApp && !mIsForBrowser) {
        SetProcessName(NS_LITERAL_STRING("(Preallocated app)"), false);
    } else {
        SetProcessName(NS_LITERAL_STRING("Browser"), false);
    }

}

void
ContentChild::SetProcessName(const nsAString& aName, bool aDontOverride)
{
    if (!mCanOverrideProcessName) {
        return;
    }

    char* name;
    if ((name = PR_GetEnv("MOZ_DEBUG_APP_PROCESS")) &&
        aName.EqualsASCII(name)) {
#ifdef OS_POSIX
        printf_stderr("\n\nCHILDCHILDCHILDCHILD\n  [%s] debug me @%d\n\n", name, getpid());
        sleep(30);
#elif defined(OS_WIN)
        // Windows has a decent JIT debugging story, so NS_DebugBreak does the
        // right thing.
        NS_DebugBreak(NS_DEBUG_BREAK,
                      "Invoking NS_DebugBreak() to debug child process",
                      nullptr, __FILE__, __LINE__);
#endif
    }

    mProcessName = aName;
    mozilla::ipc::SetThisProcessName(NS_LossyConvertUTF16toASCII(aName).get());

    if (aDontOverride) {
        mCanOverrideProcessName = false;
    }
}

void
ContentChild::GetProcessName(nsAString& aName)
{
    aName.Assign(mProcessName);
}

void
ContentChild::GetProcessName(nsACString& aName)
{
    aName.Assign(NS_ConvertUTF16toUTF8(mProcessName));
}

/* static */ void
ContentChild::AppendProcessId(nsACString& aName)
{
    if (!aName.IsEmpty()) {
        aName.Append(' ');
    }
    unsigned pid = getpid();
    aName.Append(nsPrintfCString("(pid %u)", pid));
}

void
ContentChild::InitXPCOM()
{
    // Do this as early as possible to get the parent process to initialize the
    // background thread since we'll likely need database information very soon.
    BackgroundChild::Startup();

    nsCOMPtr<nsIIPCBackgroundChildCreateCallback> callback =
        new BackgroundChildPrimer();
    if (!BackgroundChild::GetOrCreateForCurrentThread(callback)) {
        MOZ_CRASH("Failed to create PBackgroundChild!");
    }

    nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (!svc) {
        NS_WARNING("Couldn't acquire console service");
        return;
    }

    mConsoleListener = new ConsoleListener(this);
    if (NS_FAILED(svc->RegisterListener(mConsoleListener)))
        NS_WARNING("Couldn't register console listener for child process");

    bool isOffline;
    SendGetXPCOMProcessAttributes(&isOffline);
    RecvSetOffline(isOffline);

    DebugOnly<FileUpdateDispatcher*> observer = FileUpdateDispatcher::GetSingleton();
    NS_ASSERTION(observer, "FileUpdateDispatcher is null");

    // This object is held alive by the observer service.
    nsRefPtr<SystemMessageHandledObserver> sysMsgObserver =
        new SystemMessageHandledObserver();
    sysMsgObserver->Init();

    InitOnContentProcessCreated(/* aAfterNuwaFork = */false);
}

PMemoryReportRequestChild*
ContentChild::AllocPMemoryReportRequestChild(const uint32_t& aGeneration,
                                             const bool &aAnonymize,
                                             const bool &aMinimizeMemoryUsage,
                                             const nsString& aDMDDumpIdent)
{
    MemoryReportRequestChild *actor =
        new MemoryReportRequestChild(aGeneration, aAnonymize, aDMDDumpIdent);
    actor->AddRef();
    return actor;
}

// This is just a wrapper for InfallibleTArray<MemoryReport> that implements
// nsISupports, so it can be passed to nsIMemoryReporter::CollectReports.
class MemoryReportsWrapper MOZ_FINAL : public nsISupports {
    ~MemoryReportsWrapper() {}
public:
    NS_DECL_ISUPPORTS
    MemoryReportsWrapper(InfallibleTArray<MemoryReport> *r) : mReports(r) { }
    InfallibleTArray<MemoryReport> *mReports;
};
NS_IMPL_ISUPPORTS0(MemoryReportsWrapper)

class MemoryReportCallback MOZ_FINAL : public nsIMemoryReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    MemoryReportCallback(const nsACString &aProcess)
    : mProcess(aProcess)
    {
    }

    NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
                        int32_t aKind, int32_t aUnits, int64_t aAmount,
                        const nsACString &aDescription,
                        nsISupports *aiWrappedReports)
    {
        MemoryReportsWrapper *wrappedReports =
            static_cast<MemoryReportsWrapper *>(aiWrappedReports);

        MemoryReport memreport(mProcess, nsCString(aPath), aKind, aUnits,
                               aAmount, nsCString(aDescription));
        wrappedReports->mReports->AppendElement(memreport);
        return NS_OK;
    }
private:
    ~MemoryReportCallback() {}

    const nsCString mProcess;
};
NS_IMPL_ISUPPORTS(
  MemoryReportCallback
, nsIMemoryReporterCallback
)

bool
ContentChild::RecvPMemoryReportRequestConstructor(
    PMemoryReportRequestChild* aChild,
    const uint32_t& aGeneration,
    const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage,
    const nsString& aDMDDumpIdent)
{
    MemoryReportRequestChild *actor =
        static_cast<MemoryReportRequestChild*>(aChild);
    nsresult rv;

    if (aMinimizeMemoryUsage) {
        nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
        rv = mgr->MinimizeMemoryUsage(actor);
        // mgr will eventually call actor->Run()
    } else {
        rv = actor->Run();
    }

    return !NS_WARN_IF(NS_FAILED(rv));
}

NS_IMETHODIMP MemoryReportRequestChild::Run()
{
    ContentChild *child = static_cast<ContentChild*>(Manager());
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");

    InfallibleTArray<MemoryReport> reports;

    nsCString process;
    child->GetProcessName(process);
    child->AppendProcessId(process);

    // Run the reporters.  The callback will turn each measurement into a
    // MemoryReport.
    nsRefPtr<MemoryReportsWrapper> wrappedReports =
        new MemoryReportsWrapper(&reports);
    nsRefPtr<MemoryReportCallback> cb = new MemoryReportCallback(process);
    mgr->GetReportsForThisProcessExtended(cb, wrappedReports, mAnonymize,
                                          mDMDDumpIdent);

    bool sent = Send__delete__(this, mGeneration, reports);
    return sent ? NS_OK : NS_ERROR_FAILURE;
}

bool
ContentChild::RecvAudioChannelNotify()
{
    nsRefPtr<AudioChannelService> service =
        AudioChannelService::GetAudioChannelService();
    if (service) {
        service->Notify();
    }
    return true;
}

bool
ContentChild::RecvDataStoreNotify(const uint32_t& aAppId,
                                  const nsString& aName,
                                  const nsString& aManifestURL)
{
  nsRefPtr<DataStoreService> service = DataStoreService::GetOrCreate();
  if (NS_WARN_IF(!service)) {
    return false;
  }

  nsresult rv = service->EnableDataStore(aAppId, aName, aManifestURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

bool
ContentChild::DeallocPMemoryReportRequestChild(PMemoryReportRequestChild* actor)
{
    static_cast<MemoryReportRequestChild*>(actor)->Release();
    return true;
}

PCycleCollectWithLogsChild*
ContentChild::AllocPCycleCollectWithLogsChild(const bool& aDumpAllTraces,
                                              const FileDescriptor& aGCLog,
                                              const FileDescriptor& aCCLog)
{
    CycleCollectWithLogsChild* actor = new CycleCollectWithLogsChild(aGCLog, aCCLog);
    // Return actor with refcount 0, which is safe because it has a non-XPCOM type.
    return actor;
}

bool
ContentChild::RecvPCycleCollectWithLogsConstructor(PCycleCollectWithLogsChild* aActor,
                                                   const bool& aDumpAllTraces,
                                                   const FileDescriptor& aGCLog,
                                                   const FileDescriptor& aCCLog)
{
    // Take a reference here, where the XPCOM type is regained.
    nsRefPtr<CycleCollectWithLogsChild> sink = static_cast<CycleCollectWithLogsChild*>(aActor);
    nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");

    dumper->DumpGCAndCCLogsToSink(aDumpAllTraces, sink);

    // The actor's destructor is called when the last reference goes away...
    return true;
}

bool
ContentChild::DeallocPCycleCollectWithLogsChild(PCycleCollectWithLogsChild* /* aActor */)
{
    // ...so when we get here, there's nothing for us to do.
    //
    // Also, we're already in ~CycleCollectWithLogsChild (q.v.) at
    // this point, so we shouldn't touch the actor in any case.
    return true;
}

PContentBridgeChild*
ContentChild::AllocPContentBridgeChild(mozilla::ipc::Transport* aTransport,
                                       base::ProcessId aOtherProcess)
{
    return ContentBridgeChild::Create(aTransport, aOtherProcess);
}

PContentBridgeParent*
ContentChild::AllocPContentBridgeParent(mozilla::ipc::Transport* aTransport,
                                        base::ProcessId aOtherProcess)
{
    MOZ_ASSERT(!mLastBridge);
    mLastBridge = static_cast<ContentBridgeParent*>(
        ContentBridgeParent::Create(aTransport, aOtherProcess));
    return mLastBridge;
}

PCompositorChild*
ContentChild::AllocPCompositorChild(mozilla::ipc::Transport* aTransport,
                                    base::ProcessId aOtherProcess)
{
    return CompositorChild::Create(aTransport, aOtherProcess);
}

PSharedBufferManagerChild*
ContentChild::AllocPSharedBufferManagerChild(mozilla::ipc::Transport* aTransport,
                                              base::ProcessId aOtherProcess)
{
    return SharedBufferManagerChild::StartUpInChildProcess(aTransport, aOtherProcess);
}

PImageBridgeChild*
ContentChild::AllocPImageBridgeChild(mozilla::ipc::Transport* aTransport,
                                     base::ProcessId aOtherProcess)
{
    return ImageBridgeChild::StartUpInChildProcess(aTransport, aOtherProcess);
}

PBackgroundChild*
ContentChild::AllocPBackgroundChild(Transport* aTransport,
                                    ProcessId aOtherProcess)
{
    return BackgroundChild::Alloc(aTransport, aOtherProcess);
}

bool
ContentChild::RecvSetProcessSandbox()
{
  // We may want to move the sandbox initialization somewhere else
  // at some point; see bug 880808.
#if defined(MOZ_CONTENT_SANDBOX)
#if defined(XP_LINUX)
    SetCurrentProcessSandbox();
#elif defined(XP_WIN)
    mozilla::SandboxTarget::Instance()->StartSandbox();
#endif
#endif
    return true;
}

bool
ContentChild::RecvSpeakerManagerNotify()
{
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<SpeakerManagerService> service =
        SpeakerManagerService::GetSpeakerManagerService();
    if (service) {
        service->Notify();
    }
    return true;
#endif
    return false;
}

static CancelableTask* sFirstIdleTask;

static void FirstIdle(void)
{
    MOZ_ASSERT(sFirstIdleTask);
    sFirstIdleTask = nullptr;
    ContentChild::GetSingleton()->SendFirstIdle();
}

mozilla::jsipc::PJavaScriptChild *
ContentChild::AllocPJavaScriptChild()
{
    MOZ_ASSERT(!ManagedPJavaScriptChild().Length());

    return nsIContentChild::AllocPJavaScriptChild();
}

bool
ContentChild::DeallocPJavaScriptChild(PJavaScriptChild *aChild)
{
    return nsIContentChild::DeallocPJavaScriptChild(aChild);
}

PBrowserChild*
ContentChild::AllocPBrowserChild(const IPCTabContext& aContext,
                                 const uint32_t& aChromeFlags,
                                 const uint64_t& aID,
                                 const bool& aIsForApp,
                                 const bool& aIsForBrowser)
{
    return nsIContentChild::AllocPBrowserChild(aContext,
                                               aChromeFlags,
                                               aID,
                                               aIsForApp,
                                               aIsForBrowser);
}

bool
ContentChild::SendPBrowserConstructor(PBrowserChild* aActor,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const uint64_t& aID,
                                      const bool& aIsForApp,
                                      const bool& aIsForBrowser)
{
    return PContentChild::SendPBrowserConstructor(aActor,
                                                  aContext,
                                                  aChromeFlags,
                                                  aID,
                                                  aIsForApp,
                                                  aIsForBrowser);
}

bool
ContentChild::RecvPBrowserConstructor(PBrowserChild* aActor,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const uint64_t& aID,
                                      const bool& aIsForApp,
                                      const bool& aIsForBrowser)
{
    // This runs after AllocPBrowserChild() returns and the IPC machinery for this
    // PBrowserChild has been set up.

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
        nsITabChild* tc =
            static_cast<nsITabChild*>(static_cast<TabChild*>(aActor));
        os->NotifyObservers(tc, "tab-child-created", nullptr);
    }

    static bool hasRunOnce = false;
    if (!hasRunOnce) {
        hasRunOnce = true;

        MOZ_ASSERT(!sFirstIdleTask);
        sFirstIdleTask = NewRunnableFunction(FirstIdle);
        MessageLoop::current()->PostIdleTask(FROM_HERE, sFirstIdleTask);

        // Redo InitProcessAttributes() when the app or browser is really
        // launching so the attributes will be correct.
        mID = aID;
        mIsForApp = aIsForApp;
        mIsForBrowser = aIsForBrowser;
        InitProcessAttributes();
    }

    return true;
}

PFileDescriptorSetChild*
ContentChild::AllocPFileDescriptorSetChild(const FileDescriptor& aFD)
{
    return new FileDescriptorSetChild(aFD);
}

bool
ContentChild::DeallocPFileDescriptorSetChild(PFileDescriptorSetChild* aActor)
{
    delete static_cast<FileDescriptorSetChild*>(aActor);
    return true;
}

bool
ContentChild::DeallocPBrowserChild(PBrowserChild* aIframe)
{
    return nsIContentChild::DeallocPBrowserChild(aIframe);
}

PBlobChild*
ContentChild::AllocPBlobChild(const BlobConstructorParams& aParams)
{
    return nsIContentChild::AllocPBlobChild(aParams);
}

bool
ContentChild::DeallocPBlobChild(PBlobChild* aActor)
{
    return nsIContentChild::DeallocPBlobChild(aActor);
}

PBlobChild*
ContentChild::SendPBlobConstructor(PBlobChild* aActor,
                                   const BlobConstructorParams& aParams)
{
    return PContentChild::SendPBlobConstructor(aActor, aParams);
}

PCrashReporterChild*
ContentChild::AllocPCrashReporterChild(const mozilla::dom::NativeThreadId& id,
                                       const uint32_t& processType)
{
#ifdef MOZ_CRASHREPORTER
    return new CrashReporterChild();
#else
    return nullptr;
#endif
}

bool
ContentChild::DeallocPCrashReporterChild(PCrashReporterChild* crashreporter)
{
    delete crashreporter;
    return true;
}

PHalChild*
ContentChild::AllocPHalChild()
{
    return CreateHalChild();
}

bool
ContentChild::DeallocPHalChild(PHalChild* aHal)
{
    delete aHal;
    return true;
}

PIndexedDBChild*
ContentChild::AllocPIndexedDBChild()
{
    NS_NOTREACHED("Should never get here!");
    return nullptr;
}

bool
ContentChild::DeallocPIndexedDBChild(PIndexedDBChild* aActor)
{
    delete aActor;
    return true;
}

asmjscache::PAsmJSCacheEntryChild*
ContentChild::AllocPAsmJSCacheEntryChild(
                                    const asmjscache::OpenMode& aOpenMode,
                                    const asmjscache::WriteParams& aWriteParams,
                                    const IPC::Principal& aPrincipal)
{
    NS_NOTREACHED("Should never get here!");
    return nullptr;
}

bool
ContentChild::DeallocPAsmJSCacheEntryChild(PAsmJSCacheEntryChild* aActor)
{
    asmjscache::DeallocEntryChild(aActor);
    return true;
}

PTestShellChild*
ContentChild::AllocPTestShellChild()
{
    return new TestShellChild();
}

bool
ContentChild::DeallocPTestShellChild(PTestShellChild* shell)
{
    delete shell;
    return true;
}

jsipc::JavaScriptChild *
ContentChild::GetCPOWManager()
{
    if (ManagedPJavaScriptChild().Length()) {
        return static_cast<JavaScriptChild*>(ManagedPJavaScriptChild()[0]);
    }
    JavaScriptChild* actor = static_cast<JavaScriptChild*>(SendPJavaScriptConstructor());
    return actor;
}

bool
ContentChild::RecvPTestShellConstructor(PTestShellChild* actor)
{
    return true;
}

PDeviceStorageRequestChild*
ContentChild::AllocPDeviceStorageRequestChild(const DeviceStorageParams& aParams)
{
    return new DeviceStorageRequestChild();
}

bool
ContentChild::DeallocPDeviceStorageRequestChild(PDeviceStorageRequestChild* aDeviceStorage)
{
    delete aDeviceStorage;
    return true;
}

PFileSystemRequestChild*
ContentChild::AllocPFileSystemRequestChild(const FileSystemParams& aParams)
{
    NS_NOTREACHED("Should never get here!");
    return nullptr;
}

bool
ContentChild::DeallocPFileSystemRequestChild(PFileSystemRequestChild* aFileSystem)
{
    mozilla::dom::FileSystemTaskBase* child =
      static_cast<mozilla::dom::FileSystemTaskBase*>(aFileSystem);
    // The reference is increased in FileSystemTaskBase::Start of
    // FileSystemTaskBase.cpp. We should decrease it after IPC.
    NS_RELEASE(child);
    return true;
}

PNeckoChild*
ContentChild::AllocPNeckoChild()
{
    return new NeckoChild();
}

bool
ContentChild::DeallocPNeckoChild(PNeckoChild* necko)
{
    delete necko;
    return true;
}

PExternalHelperAppChild*
ContentChild::AllocPExternalHelperAppChild(const OptionalURIParams& uri,
                                           const nsCString& aMimeContentType,
                                           const nsCString& aContentDisposition,
                                           const uint32_t& aContentDispositionHint,
                                           const nsString& aContentDispositionFilename,
                                           const bool& aForceSave,
                                           const int64_t& aContentLength,
                                           const OptionalURIParams& aReferrer,
                                           PBrowserChild* aBrowser)
{
    ExternalHelperAppChild *child = new ExternalHelperAppChild();
    child->AddRef();
    return child;
}

bool
ContentChild::DeallocPExternalHelperAppChild(PExternalHelperAppChild* aService)
{
    ExternalHelperAppChild *child = static_cast<ExternalHelperAppChild*>(aService);
    child->Release();
    return true;
}

PSmsChild*
ContentChild::AllocPSmsChild()
{
    return new SmsChild();
}

bool
ContentChild::DeallocPSmsChild(PSmsChild* aSms)
{
    delete aSms;
    return true;
}

PTelephonyChild*
ContentChild::AllocPTelephonyChild()
{
    MOZ_CRASH("No one should be allocating PTelephonyChild actors");
}

bool
ContentChild::DeallocPTelephonyChild(PTelephonyChild* aActor)
{
    delete aActor;
    return true;
}

PStorageChild*
ContentChild::AllocPStorageChild()
{
    NS_NOTREACHED("We should never be manually allocating PStorageChild actors");
    return nullptr;
}

bool
ContentChild::DeallocPStorageChild(PStorageChild* aActor)
{
    DOMStorageDBChild* child = static_cast<DOMStorageDBChild*>(aActor);
    child->ReleaseIPDLReference();
    return true;
}

PBluetoothChild*
ContentChild::AllocPBluetoothChild()
{
#ifdef MOZ_B2G_BT
    MOZ_CRASH("No one should be allocating PBluetoothChild actors");
#else
    MOZ_CRASH("No support for bluetooth on this platform!");
#endif
}

bool
ContentChild::DeallocPBluetoothChild(PBluetoothChild* aActor)
{
#ifdef MOZ_B2G_BT
    delete aActor;
    return true;
#else
    MOZ_CRASH("No support for bluetooth on this platform!");
#endif
}

PFMRadioChild*
ContentChild::AllocPFMRadioChild()
{
#ifdef MOZ_B2G_FM
    NS_RUNTIMEABORT("No one should be allocating PFMRadioChild actors");
    return nullptr;
#else
    NS_RUNTIMEABORT("No support for FMRadio on this platform!");
    return nullptr;
#endif
}

bool
ContentChild::DeallocPFMRadioChild(PFMRadioChild* aActor)
{
#ifdef MOZ_B2G_FM
    delete aActor;
    return true;
#else
    NS_RUNTIMEABORT("No support for FMRadio on this platform!");
    return false;
#endif
}

PSpeechSynthesisChild*
ContentChild::AllocPSpeechSynthesisChild()
{
#ifdef MOZ_WEBSPEECH
    MOZ_CRASH("No one should be allocating PSpeechSynthesisChild actors");
#else
    return nullptr;
#endif
}

bool
ContentChild::DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor)
{
#ifdef MOZ_WEBSPEECH
    delete aActor;
    return true;
#else
    return false;
#endif
}

bool
ContentChild::RecvRegisterChrome(const InfallibleTArray<ChromePackage>& packages,
                                 const InfallibleTArray<ResourceMapping>& resources,
                                 const InfallibleTArray<OverrideMapping>& overrides,
                                 const nsCString& locale)
{
    nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
    nsChromeRegistryContent* chromeRegistry =
        static_cast<nsChromeRegistryContent*>(registrySvc.get());
    chromeRegistry->RegisterRemoteChrome(packages, resources, overrides, locale);
    return true;
}

bool
ContentChild::RecvSetOffline(const bool& offline)
{
    nsCOMPtr<nsIIOService> io (do_GetIOService());
    NS_ASSERTION(io, "IO Service can not be null");

    io->SetOffline(offline);

    return true;
}

void
ContentChild::ActorDestroy(ActorDestroyReason why)
{
    if (AbnormalShutdown == why) {
        NS_WARNING("shutting down early because of crash!");
        QuickExit();
    }

#ifndef DEBUG
    // In release builds, there's no point in the content process
    // going through the full XPCOM shutdown path, because it doesn't
    // keep persistent state.
    QuickExit();
#endif

    if (sFirstIdleTask) {
        sFirstIdleTask->Cancel();
    }

    mAlertObservers.Clear();

    mIdleObservers.Clear();

    nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (svc) {
        svc->UnregisterListener(mConsoleListener);
        mConsoleListener->mChild = nullptr;
    }

    XRE_ShutdownChildProcess();
}

void
ContentChild::ProcessingError(Result what)
{
    switch (what) {
    case MsgDropped:
        QuickExit();

    case MsgNotKnown:
        NS_RUNTIMEABORT("aborting because of MsgNotKnown");
    case MsgNotAllowed:
        NS_RUNTIMEABORT("aborting because of MsgNotAllowed");
    case MsgPayloadError:
        NS_RUNTIMEABORT("aborting because of MsgPayloadError");
    case MsgProcessingError:
        NS_RUNTIMEABORT("aborting because of MsgProcessingError");
    case MsgRouteError:
        NS_RUNTIMEABORT("aborting because of MsgRouteError");
    case MsgValueError:
        NS_RUNTIMEABORT("aborting because of MsgValueError");

    default:
        NS_RUNTIMEABORT("not reached");
    }
}

void
ContentChild::QuickExit()
{
    NS_WARNING("content process _exit()ing");
    _exit(0);
}

nsresult
ContentChild::AddRemoteAlertObserver(const nsString& aData,
                                     nsIObserver* aObserver)
{
    NS_ASSERTION(aObserver, "Adding a null observer?");
    mAlertObservers.AppendElement(new AlertObserver(aObserver, aData));
    return NS_OK;
}


bool
ContentChild::RecvSystemMemoryAvailable(const uint64_t& aGetterId,
                                        const uint32_t& aMemoryAvailable)
{
    nsRefPtr<Promise> p = dont_AddRef(reinterpret_cast<Promise*>(aGetterId));

    if (!aMemoryAvailable) {
        p->MaybeReject(NS_ERROR_NOT_AVAILABLE);
        return true;
    }

    p->MaybeResolve((int)aMemoryAvailable);
    return true;
}

bool
ContentChild::RecvPreferenceUpdate(const PrefSetting& aPref)
{
    Preferences::SetPreference(aPref);
    return true;
}

bool
ContentChild::RecvNotifyAlertsObserver(const nsCString& aType, const nsString& aData)
{
    for (uint32_t i = 0; i < mAlertObservers.Length();
         /*we mutate the array during the loop; ++i iff no mutation*/) {
        AlertObserver* observer = mAlertObservers[i];
        if (observer->Observes(aData) && observer->Notify(aType)) {
            // if aType == alertfinished, this alert is done.  we can
            // remove the observer.
            if (aType.Equals(nsDependentCString("alertfinished"))) {
                mAlertObservers.RemoveElementAt(i);
                continue;
            }
        }
        ++i;
    }
    return true;
}

bool
ContentChild::RecvNotifyVisited(const URIParams& aURI)
{
    nsCOMPtr<nsIURI> newURI = DeserializeURI(aURI);
    if (!newURI) {
        return false;
    }
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
        history->NotifyVisited(newURI);
    }
    return true;
}

bool
ContentChild::RecvAsyncMessage(const nsString& aMsg,
                               const ClonedMessageData& aData,
                               const InfallibleTArray<CpowEntry>& aCpows,
                               const IPC::Principal& aPrincipal)
{
    nsRefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::sChildProcessManager;
    if (cpm) {
        StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForChild(aData);
        CpowIdHolder cpows(GetCPOWManager(), aCpows);
        cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()),
                            aMsg, false, &cloneData, &cpows, aPrincipal, nullptr);
    }
    return true;
}

bool
ContentChild::RecvGeolocationUpdate(const GeoPosition& somewhere)
{
    nsCOMPtr<nsIGeolocationUpdate> gs = do_GetService("@mozilla.org/geolocation/service;1");
    if (!gs) {
        return true;
    }
    nsCOMPtr<nsIDOMGeoPosition> position = somewhere;
    gs->Update(position);
    return true;
}

bool
ContentChild::RecvAddPermission(const IPC::Permission& permission)
{
#if MOZ_PERMISSIONS
    nsCOMPtr<nsIPermissionManager> permissionManagerIface =
        services::GetPermissionManager();
    nsPermissionManager* permissionManager =
        static_cast<nsPermissionManager*>(permissionManagerIface.get());
    NS_ABORT_IF_FALSE(permissionManager,
                     "We have no permissionManager in the Content process !");

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("http://") + nsCString(permission.host));
    NS_ENSURE_TRUE(uri, true);

    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(secMan);

    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = secMan->GetAppCodebasePrincipal(uri, permission.appId,
                                                permission.isInBrowserElement,
                                                getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, true);

    permissionManager->AddInternal(principal,
                                   nsCString(permission.type),
                                   permission.capability,
                                   0,
                                   permission.expireType,
                                   permission.expireTime,
                                   nsPermissionManager::eNotify,
                                   nsPermissionManager::eNoDBOperation);
#endif

    return true;
}

bool
ContentChild::RecvScreenSizeChanged(const gfxIntSize& size)
{
#ifdef ANDROID
    mScreenSize = size;
#else
    NS_RUNTIMEABORT("Message currently only expected on android");
#endif
    return true;
}

bool
ContentChild::RecvFlushMemory(const nsString& reason)
{
#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        // Don't flush memory in the nuwa process: the GC thread could be frozen.
        return true;
    }
#endif
    nsCOMPtr<nsIObserverService> os =
        mozilla::services::GetObserverService();
    if (os)
        os->NotifyObservers(nullptr, "memory-pressure", reason.get());
    return true;
}

bool
ContentChild::RecvActivateA11y()
{
#ifdef ACCESSIBILITY
    // Start accessibility in content process if it's running in chrome
    // process.
    nsCOMPtr<nsIAccessibilityService> accService =
        do_GetService("@mozilla.org/accessibilityService;1");
#endif
    return true;
}

bool
ContentChild::RecvGarbageCollect()
{
    // Rebroadcast the "child-gc-request" so that workers will GC.
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->NotifyObservers(nullptr, "child-gc-request", nullptr);
    }
    nsJSContext::GarbageCollectNow(JS::gcreason::DOM_IPC);
    return true;
}

bool
ContentChild::RecvCycleCollect()
{
    // Rebroadcast the "child-cc-request" so that workers will CC.
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->NotifyObservers(nullptr, "child-cc-request", nullptr);
    }
    nsJSContext::CycleCollectNow();
    return true;
}

#ifdef MOZ_NUWA_PROCESS
static void
OnFinishNuwaPreparation ()
{
    MakeNuwaProcess();
}
#endif

static void
PreloadSlowThings()
{
    // This fetches and creates all the built-in stylesheets.
    nsLayoutStylesheetCache::UserContentSheet();

    TabChild::PreloadSlowThings();

}

bool
ContentChild::RecvAppInfo(const nsCString& version, const nsCString& buildID,
                          const nsCString& name, const nsCString& UAName)
{
    mAppInfo.version.Assign(version);
    mAppInfo.buildID.Assign(buildID);
    mAppInfo.name.Assign(name);
    mAppInfo.UAName.Assign(UAName);

    if (!Preferences::GetBool("dom.ipc.processPrelaunch.enabled", false)) {
        return true;
    }

    // If we're part of the mozbrowser machinery, go ahead and start
    // preloading things.  We can only do this for mozbrowser because
    // PreloadSlowThings() may set the docshell of the first TabChild
    // inactive, and we can only safely restore it to active from
    // BrowserElementChild.js.
    if ((mIsForApp || mIsForBrowser)
#ifdef MOZ_NUWA_PROCESS
        && !IsNuwaProcess()
#endif
       ) {
        PreloadSlowThings();
    }

#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        ContentChild::GetSingleton()->RecvGarbageCollect();
        MessageLoop::current()->PostTask(
            FROM_HERE, NewRunnableFunction(OnFinishNuwaPreparation));
    }
#endif

    return true;
}

bool
ContentChild::RecvLastPrivateDocShellDestroyed()
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
    return true;
}

bool
ContentChild::RecvFilePathUpdate(const nsString& aStorageType,
                                 const nsString& aStorageName,
                                 const nsString& aPath,
                                 const nsCString& aReason)
{
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(aStorageType, aStorageName, aPath);

    nsString reason;
    CopyASCIItoUTF16(aReason, reason);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(dsf, "file-watcher-update", reason.get());
    return true;
}

bool
ContentChild::RecvFileSystemUpdate(const nsString& aFsName,
                                   const nsString& aVolumeName,
                                   const int32_t& aState,
                                   const int32_t& aMountGeneration,
                                   const bool& aIsMediaPresent,
                                   const bool& aIsSharing,
                                   const bool& aIsFormatting,
                                   const bool& aIsFake)
{
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<nsVolume> volume = new nsVolume(aFsName, aVolumeName, aState,
                                             aMountGeneration, aIsMediaPresent,
                                             aIsSharing, aIsFormatting, aIsFake);

    nsRefPtr<nsVolumeService> vs = nsVolumeService::GetSingleton();
    if (vs) {
        vs->UpdateVolume(volume);
    }
#else
    // Remove warnings about unused arguments
    unused << aFsName;
    unused << aVolumeName;
    unused << aState;
    unused << aMountGeneration;
    unused << aIsMediaPresent;
    unused << aIsSharing;
    unused << aIsFormatting;
    unused << aIsFake;
#endif
    return true;
}

bool
ContentChild::RecvNotifyProcessPriorityChanged(
    const hal::ProcessPriority& aPriority)
{
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    NS_ENSURE_TRUE(os, true);

    nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
    props->SetPropertyAsInt32(NS_LITERAL_STRING("priority"),
                              static_cast<int32_t>(aPriority));

    os->NotifyObservers(static_cast<nsIPropertyBag2*>(props),
                        "ipc:process-priority-changed",  nullptr);
    return true;
}

bool
ContentChild::RecvMinimizeMemoryUsage()
{
#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        // Don't minimize memory in the nuwa process: it will perform GC, but the
        // GC thread could be frozen.
        return true;
    }
#endif
    nsCOMPtr<nsIMemoryReporterManager> mgr =
        do_GetService("@mozilla.org/memory-reporter-manager;1");
    NS_ENSURE_TRUE(mgr, true);

    mgr->MinimizeMemoryUsage(/* callback = */ nullptr);
    return true;
}

bool
ContentChild::RecvNotifyPhoneStateChange(const nsString& aState)
{
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "phone-state-changed", aState.get());
    }
    return true;
}

void
ContentChild::AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS)
{
    MOZ_ASSERT(aObserver, "null idle observer");
    // Make sure aObserver isn't released while we wait for the parent
    aObserver->AddRef();
    SendAddIdleObserver(reinterpret_cast<uint64_t>(aObserver), aIdleTimeInS);
    mIdleObservers.PutEntry(aObserver);
}

void
ContentChild::RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS)
{
    MOZ_ASSERT(aObserver, "null idle observer");
    SendRemoveIdleObserver(reinterpret_cast<uint64_t>(aObserver), aIdleTimeInS);
    aObserver->Release();
    mIdleObservers.RemoveEntry(aObserver);
}

bool
ContentChild::RecvNotifyIdleObserver(const uint64_t& aObserver,
                                     const nsCString& aTopic,
                                     const nsString& aTimeStr)
{
    nsIObserver* observer = reinterpret_cast<nsIObserver*>(aObserver);
    if (mIdleObservers.Contains(observer)) {
        observer->Observe(nullptr, aTopic.get(), aTimeStr.get());
    } else {
        NS_WARNING("Received notification for an idle observer that was removed.");
    }
    return true;
}

bool
ContentChild::RecvLoadAndRegisterSheet(const URIParams& aURI, const uint32_t& aType)
{
    nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
    if (!uri) {
        return true;
    }

    nsStyleSheetService *sheetService = nsStyleSheetService::GetInstance();
    if (sheetService) {
        sheetService->LoadAndRegisterSheet(uri, aType);
    }

    return true;
}

bool
ContentChild::RecvUnregisterSheet(const URIParams& aURI, const uint32_t& aType)
{
    nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
    if (!uri) {
        return true;
    }

    nsStyleSheetService *sheetService = nsStyleSheetService::GetInstance();
    if (sheetService) {
        sheetService->UnregisterSheet(uri, aType);
    }

    return true;
}

#ifdef MOZ_NUWA_PROCESS
class CallNuwaSpawn : public nsRunnable
{
public:
    NS_IMETHOD Run()
    {
        NuwaSpawn();
        if (IsNuwaProcess()) {
            return NS_OK;
        }

        // In the new process.
        ContentChild* child = ContentChild::GetSingleton();
        child->SetProcessName(NS_LITERAL_STRING("(Preallocated app)"), false);
        mozilla::ipc::Transport* transport = child->GetTransport();
        int fd = transport->GetFileDescriptor();
        transport->ResetFileDescriptor(fd);

        IToplevelProtocol* toplevel = child->GetFirstOpenedActors();
        while (toplevel != nullptr) {
            transport = toplevel->GetTransport();
            fd = transport->GetFileDescriptor();
            transport->ResetFileDescriptor(fd);

            toplevel = toplevel->getNext();
        }

        // Perform other after-fork initializations.
        InitOnContentProcessCreated(/* aAfterNuwaFork = */true);

        return NS_OK;
    }
};

static void
DoNuwaFork()
{
    NS_ASSERTION(NuwaSpawnPrepare != nullptr,
                 "NuwaSpawnPrepare() is not available!");
    NuwaSpawnPrepare();       // NuwaSpawn will be blocked.

    {
        nsCOMPtr<nsIRunnable> callSpawn(new CallNuwaSpawn());
        NS_DispatchToMainThread(callSpawn);
    }

    // IOThread should be blocked here for waiting NuwaSpawn().
    NS_ASSERTION(NuwaSpawnWait != nullptr,
                 "NuwaSpawnWait() is not available!");
    NuwaSpawnWait();        // Now! NuwaSpawn can go.
    // Here, we can make sure the spawning was finished.
}

/**
 * This function should keep IO thread in a stable state and freeze it
 * until the spawning is finished.
 */
static void
RunNuwaFork()
{
    if (NuwaCheckpointCurrentThread()) {
      DoNuwaFork();
    }
}
#endif

bool
ContentChild::RecvNuwaFork()
{
#ifdef MOZ_NUWA_PROCESS
    if (sNuwaForking) {           // No reentry.
        return true;
    }
    sNuwaForking = true;

    // We want to ensure that the PBackground actor gets cloned in the Nuwa
    // process before we freeze. Also, we have to do this to avoid deadlock.
    // Protocols that are "opened" (e.g. PBackground, PCompositor) block the
    // main thread to wait for the IPC thread during the open operation.
    // NuwaSpawnWait() blocks the IPC thread to wait for the main thread when
    // the Nuwa process is forked. Unless we ensure that the two cannot happen
    // at the same time then we risk deadlock. Spinning the event loop here
    // guarantees the ordering is safe for PBackground.
    while (!BackgroundChild::GetForCurrentThread()) {
        if (NS_WARN_IF(!NS_ProcessNextEvent())) {
            return false;
        }
    }

    MessageLoop* ioloop = XRE_GetIOMessageLoop();
    ioloop->PostTask(FROM_HERE, NewRunnableFunction(RunNuwaFork));
    return true;
#else
    return false; // Makes the underlying IPC channel abort.
#endif
}

} // namespace dom
} // namespace mozilla

extern "C" {

#if defined(MOZ_NUWA_PROCESS)
NS_EXPORT void
GetProtoFdInfos(NuwaProtoFdInfo* aInfoList,
                size_t aInfoListSize,
                size_t* aInfoSize)
{
    size_t i = 0;

    mozilla::dom::ContentChild* content =
        mozilla::dom::ContentChild::GetSingleton();
    aInfoList[i].protoId = content->GetProtocolId();
    aInfoList[i].originFd =
        content->GetTransport()->GetFileDescriptor();
    i++;

    for (IToplevelProtocol* actor = content->GetFirstOpenedActors();
         actor != nullptr;
         actor = actor->getNext()) {
        if (i >= aInfoListSize) {
            NS_RUNTIMEABORT("Too many top level protocols!");
        }

        aInfoList[i].protoId = actor->GetProtocolId();
        aInfoList[i].originFd =
            actor->GetTransport()->GetFileDescriptor();
        i++;
    }

    if (i > NUWA_TOPLEVEL_MAX) {
        NS_RUNTIMEABORT("Too many top level protocols!");
    }
    *aInfoSize = i;
}

class RunAddNewIPCProcess : public nsRunnable
{
public:
    RunAddNewIPCProcess(pid_t aPid,
                        nsTArray<mozilla::ipc::ProtocolFdMapping>& aMaps)
        : mPid(aPid)
    {
        mMaps.SwapElements(aMaps);
    }

    NS_IMETHOD Run()
    {
        mozilla::dom::ContentChild::GetSingleton()->
            SendAddNewProcess(mPid, mMaps);

        MOZ_ASSERT(sNuwaForking);
        sNuwaForking = false;

        return NS_OK;
    }

private:
    pid_t mPid;
    nsTArray<mozilla::ipc::ProtocolFdMapping> mMaps;
};

/**
 * AddNewIPCProcess() is called by Nuwa process to tell the parent
 * process that a new process is created.
 *
 * In the newly created process, ResetContentChildTransport() is called to
 * reset fd for the IPC Channel and the session.
 */
NS_EXPORT void
AddNewIPCProcess(pid_t aPid, NuwaProtoFdInfo* aInfoList, size_t aInfoListSize)
{
    nsTArray<mozilla::ipc::ProtocolFdMapping> maps;

    for (size_t i = 0; i < aInfoListSize; i++) {
        int _fd = aInfoList[i].newFds[NUWA_NEWFD_PARENT];
        mozilla::ipc::FileDescriptor fd(_fd);
        mozilla::ipc::ProtocolFdMapping map(aInfoList[i].protoId, fd);
        maps.AppendElement(map);
    }

    nsRefPtr<RunAddNewIPCProcess> runner = new RunAddNewIPCProcess(aPid, maps);
    NS_DispatchToMainThread(runner);
}

NS_EXPORT void
OnNuwaProcessReady()
{
    mozilla::dom::ContentChild* content =
        mozilla::dom::ContentChild::GetSingleton();
    content->SendNuwaReady();
}

NS_EXPORT void
AfterNuwaFork()
{
    SetCurrentProcessPrivileges(base::PRIVILEGES_DEFAULT);
}

#endif // MOZ_NUWA_PROCESS

}
