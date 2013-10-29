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
#include "TabChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/ExternalHelperAppChild.h"
#include "mozilla/dom/PCrashReporterChild.h"
#include "mozilla/dom/DOMStorageIPC.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/PCompositorChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/Preferences.h"
#if defined(MOZ_CONTENT_SANDBOX) && defined(XP_UNIX)
#include "mozilla/Sandbox.h"
#endif
#include "mozilla/unused.h"

#include "nsIConsoleListener.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMemoryReporter.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
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

#include "IHistory.h"
#include "nsNetUtil.h"

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/task.h"

#include "nsChromeRegistryContent.h"
#include "nsFrameMessageManager.h"

#include "nsIGeolocationProvider.h"
#include "mozilla/dom/PMemoryReportRequestChild.h"

#ifdef MOZ_PERMISSIONS
#include "nsIScriptSecurityManager.h"
#include "nsPermission.h"
#include "nsPermissionManager.h"
#endif

#if defined(MOZ_WIDGET_ANDROID)
#include "APKOpen.h"
#endif

#if defined(MOZ_WIDGET_GONK)
#include "nsVolume.h"
#include "nsVolumeService.h"
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
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
#include "mozilla/dom/telephony/PTelephonyChild.h"

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

class MemoryReportRequestChild : public PMemoryReportRequestChild
{
public:
    MemoryReportRequestChild();
    virtual ~MemoryReportRequestChild();
};

MemoryReportRequestChild::MemoryReportRequestChild()
{
    MOZ_COUNT_CTOR(MemoryReportRequestChild);
}

MemoryReportRequestChild::~MemoryReportRequestChild()
{
    MOZ_COUNT_DTOR(MemoryReportRequestChild);
}

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
    ContentChild* mChild;
    friend class ContentChild;
};

NS_IMPL_ISUPPORTS1(ConsoleListener, nsIConsoleListener)

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
                                      const PRUnichar* aData)
{
    if (ContentChild::GetSingleton()) {
        ContentChild::GetSingleton()->SendSystemMessageHandled();
    }
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(SystemMessageHandledObserver, nsIObserver)

ContentChild* ContentChild::sSingleton;

ContentChild::ContentChild()
 : mID(uint64_t(-1))
#ifdef ANDROID
   ,mScreenSize(0, 0)
#endif
{
    // This process is a content process, so it's clearly running in
    // multiprocess mode!
    nsDebugImpl::SetMultiprocessMode("Child");
}

ContentChild::~ContentChild()
{
}

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

    Open(aChannel, aParentHandle, aIOLoop);
    sSingleton = this;

#ifdef MOZ_CRASHREPORTER
    SendPCrashReporterConstructor(CrashReporter::CurrentThreadId(),
                                  XRE_GetProcessType());
#endif

    SendGetProcessAttributes(&mID, &mIsForApp, &mIsForBrowser);

    GetCPOWManager();

#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        SetProcessName(NS_LITERAL_STRING("(Nuwa)"));
        return true;
    }
#endif
    if (mIsForApp && !mIsForBrowser) {
        SetProcessName(NS_LITERAL_STRING("(Preallocated app)"));
    } else {
        SetProcessName(NS_LITERAL_STRING("Browser"));
    }

    return true;
}

void
ContentChild::SetProcessName(const nsAString& aName)
{
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
}

const void
ContentChild::GetProcessName(nsAString& aName)
{
    aName.Assign(mProcessName);
}

void
ContentChild::InitXPCOM()
{
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
}

PMemoryReportRequestChild*
ContentChild::AllocPMemoryReportRequestChild()
{
    return new MemoryReportRequestChild();
}

// This is just a wrapper for InfallibleTArray<MemoryReport> that implements
// nsISupports, so it can be passed to nsIMemoryReporter::CollectReports.
class MemoryReportsWrapper MOZ_FINAL : public nsISupports {
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
    const nsCString mProcess;
};
NS_IMPL_ISUPPORTS1(
  MemoryReportCallback
, nsIMemoryReporterCallback
)

bool
ContentChild::RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* child)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");

    InfallibleTArray<MemoryReport> reports;

    nsPrintfCString process("Content (%d)", getpid());

    // Run each reporter.  The callback will turn each measurement into a
    // MemoryReport.
    nsCOMPtr<nsISimpleEnumerator> e;
    mgr->EnumerateReporters(getter_AddRefs(e));
    nsRefPtr<MemoryReportsWrapper> wrappedReports =
        new MemoryReportsWrapper(&reports);
    nsRefPtr<MemoryReportCallback> cb = new MemoryReportCallback(process);
    bool more;
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryReporter> r;
      e->GetNext(getter_AddRefs(r));
      r->CollectReports(cb, wrappedReports);
    }

    child->Send__delete__(child, reports);
    return true;
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
ContentChild::DeallocPMemoryReportRequestChild(PMemoryReportRequestChild* actor)
{
    delete actor;
    return true;
}

bool
ContentChild::RecvDumpMemoryInfoToTempDir(const nsString& aIdentifier,
                                          const bool& aMinimizeMemoryUsage,
                                          const bool& aDumpChildProcesses)
{
    nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");

    dumper->DumpMemoryInfoToTempDir(aIdentifier, aMinimizeMemoryUsage,
                                    aDumpChildProcesses);
    return true;
}

bool
ContentChild::RecvDumpGCAndCCLogsToFile(const nsString& aIdentifier,
                                        const bool& aDumpAllTraces,
                                        const bool& aDumpChildProcesses)
{
    nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");

    dumper->DumpGCAndCCLogsToFile(aIdentifier, aDumpAllTraces,
                                  aDumpChildProcesses);
    return true;
}

PCompositorChild*
ContentChild::AllocPCompositorChild(mozilla::ipc::Transport* aTransport,
                                    base::ProcessId aOtherProcess)
{
    return CompositorChild::Create(aTransport, aOtherProcess);
}

PImageBridgeChild*
ContentChild::AllocPImageBridgeChild(mozilla::ipc::Transport* aTransport,
                                     base::ProcessId aOtherProcess)
{
    return ImageBridgeChild::StartUpInChildProcess(aTransport, aOtherProcess);
}

bool
ContentChild::RecvSetProcessPrivileges(const ChildPrivileges& aPrivs)
{
  ChildPrivileges privs = (aPrivs == PRIVILEGES_DEFAULT) ?
                          GeckoChildProcessHost::DefaultChildPrivileges() :
                          aPrivs;
  // If this fails, we die.
  SetCurrentProcessPrivileges(privs);
#if defined(MOZ_CONTENT_SANDBOX) && defined(XP_UNIX)
  // SetCurrentProcessSandbox should be moved close to process initialization
  // time if/when possible. SetCurrentProcessPrivileges should probably be
  // moved as well. Right now this is set ONLY if we receive the
  // RecvSetProcessPrivileges message. See bug 880808.
  SetCurrentProcessSandbox();
#endif
  return true;
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
    nsCOMPtr<nsIJSRuntimeService> svc = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
    NS_ENSURE_TRUE(svc, nullptr);

    JSRuntime *rt;
    svc->GetRuntime(&rt);
    NS_ENSURE_TRUE(svc, nullptr);

    mozilla::jsipc::JavaScriptChild *child = new mozilla::jsipc::JavaScriptChild(rt);
    if (!child->init()) {
        delete child;
        return nullptr;
    }
    return child;
}

bool
ContentChild::DeallocPJavaScriptChild(PJavaScriptChild *child)
{
    delete child;
    return true;
}

PBrowserChild*
ContentChild::AllocPBrowserChild(const IPCTabContext& aContext,
                                 const uint32_t& aChromeFlags)
{
    // We'll happily accept any kind of IPCTabContext here; we don't need to
    // check that it's of a certain type for security purposes, because we
    // believe whatever the parent process tells us.

    MaybeInvalidTabContext tc(aContext);
    if (!tc.IsValid()) {
        NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                                 "the parent process. (%s)  Crashing...",
                                 tc.GetInvalidReason()).get());
        MOZ_CRASH("Invalid TabContext received from the parent process.");
    }

    nsRefPtr<TabChild> child = TabChild::Create(this, tc.GetTabContext(), aChromeFlags);

    // The ref here is released in DeallocPBrowserChild.
    return child.forget().get();
}

bool
ContentChild::RecvPBrowserConstructor(PBrowserChild* actor,
                                      const IPCTabContext& context,
                                      const uint32_t& chromeFlags)
{
    // This runs after AllocPBrowserChild() returns and the IPC machinery for this
    // PBrowserChild has been set up.

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
        nsITabChild* tc =
            static_cast<nsITabChild*>(static_cast<TabChild*>(actor));
        os->NotifyObservers(tc, "tab-child-created", nullptr);
    }

    static bool hasRunOnce = false;
    if (!hasRunOnce) {
        hasRunOnce = true;

        MOZ_ASSERT(!sFirstIdleTask);
        sFirstIdleTask = NewRunnableFunction(FirstIdle);
        MessageLoop::current()->PostIdleTask(FROM_HERE, sFirstIdleTask);
    }

    return true;
}


bool
ContentChild::DeallocPBrowserChild(PBrowserChild* iframe)
{
    TabChild* child = static_cast<TabChild*>(iframe);
    NS_RELEASE(child);
    return true;
}

PBlobChild*
ContentChild::AllocPBlobChild(const BlobConstructorParams& aParams)
{
  return BlobChild::Create(this, aParams);
}

bool
ContentChild::DeallocPBlobChild(PBlobChild* aActor)
{
  delete aActor;
  return true;
}

BlobChild*
ContentChild::GetOrCreateActorForBlob(nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBlob);

  // If the blob represents a remote blob then we can simply pass its actor back
  // here.
  if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlob)) {
    BlobChild* actor =
      static_cast<BlobChild*>(
        static_cast<PBlobChild*>(remoteBlob->GetPBlob()));
    MOZ_ASSERT(actor);
    return actor;
  }

  // XXX This is only safe so long as all blob implementations in our tree
  //     inherit nsDOMFileBase. If that ever changes then this will need to grow
  //     a real interface or something.
  const nsDOMFileBase* blob = static_cast<nsDOMFileBase*>(aBlob);

  // We often pass blobs that are multipart but that only contain one sub-blob
  // (WebActivities does this a bunch). Unwrap to reduce the number of actors
  // that we have to maintain.
  const nsTArray<nsCOMPtr<nsIDOMBlob> >* subBlobs = blob->GetSubBlobs();
  if (subBlobs && subBlobs->Length() == 1) {
    const nsCOMPtr<nsIDOMBlob>& subBlob = subBlobs->ElementAt(0);
    MOZ_ASSERT(subBlob);

    // We can only take this shortcut if the multipart and the sub-blob are both
    // Blob objects or both File objects.
    nsCOMPtr<nsIDOMFile> multipartBlobAsFile = do_QueryInterface(aBlob);
    nsCOMPtr<nsIDOMFile> subBlobAsFile = do_QueryInterface(subBlob);
    if (!multipartBlobAsFile == !subBlobAsFile) {
      // The wrapping was unnecessary, see if we can simply pass an existing
      // remote blob.
      if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(subBlob)) {
        BlobChild* actor =
          static_cast<BlobChild*>(
            static_cast<PBlobChild*>(remoteBlob->GetPBlob()));
        MOZ_ASSERT(actor);
        return actor;
      }

      // No need to add a reference here since the original blob must have a
      // strong reference in the caller and it must also have a strong reference
      // to this sub-blob.
      aBlob = subBlob;
      blob = static_cast<nsDOMFileBase*>(aBlob);
      subBlobs = blob->GetSubBlobs();
    }
  }

  // All blobs shared between processes must be immutable.
  nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(aBlob);
  if (!mutableBlob || NS_FAILED(mutableBlob->SetMutable(false))) {
    NS_WARNING("Failed to make blob immutable!");
    return nullptr;
  }

  ParentBlobConstructorParams params;

  if (blob->IsSizeUnknown() || blob->IsDateUnknown()) {
    // We don't want to call GetSize or GetLastModifiedDate
    // yet since that may stat a file on the main thread
    // here. Instead we'll learn the size lazily from the
    // other process.
    params.blobParams() = MysteryBlobConstructorParams();
    params.optionalInputStreamParams() = void_t();
  }
  else {
    nsString contentType;
    nsresult rv = aBlob->GetType(contentType);
    NS_ENSURE_SUCCESS(rv, nullptr);

    uint64_t length;
    rv = aBlob->GetSize(&length);
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsCOMPtr<nsIInputStream> stream;
    rv = aBlob->GetInternalStream(getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, nullptr);

    InputStreamParams inputStreamParams;
    SerializeInputStream(stream, inputStreamParams);

    params.optionalInputStreamParams() = inputStreamParams;

    nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
    if (file) {
      FileBlobConstructorParams fileParams;

      rv = file->GetName(fileParams.name());
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = file->GetMozLastModifiedDate(&fileParams.modDate());
      NS_ENSURE_SUCCESS(rv, nullptr);

      fileParams.contentType() = contentType;
      fileParams.length() = length;

      params.blobParams() = fileParams;
    } else {
      NormalBlobConstructorParams blobParams;
      blobParams.contentType() = contentType;
      blobParams.length() = length;
      params.blobParams() = blobParams;
    }
  }

  BlobChild* actor = BlobChild::Create(this, aBlob);
  NS_ENSURE_TRUE(actor, nullptr);

  return SendPBlobConstructor(actor, params) ? actor : nullptr;
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
                               const InfallibleTArray<CpowEntry>& aCpows)
{
  nsRefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::sChildProcessManager;
  if (cpm) {
    StructuredCloneData cloneData = ipc::UnpackClonedMessageDataForChild(aData);
    CpowIdHolder cpows(GetCPOWManager(), aCpows);
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()),
                        aMsg, false, &cloneData, &cpows, nullptr);
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
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
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
    nsJSContext::GarbageCollectNow(JS::gcreason::DOM_IPC);
    return true;
}

bool
ContentChild::RecvCycleCollect()
{
    nsJSContext::GarbageCollectNow(JS::gcreason::DOM_IPC);
    nsJSContext::CycleCollectNow();
    return true;
}

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
    // If we're part of the mozbrowser machinery, go ahead and start
    // preloading things.  We can only do this for mozbrowser because
    // PreloadSlowThings() may set the docshell of the first TabChild
    // inactive, and we can only safely restore it to active from
    // BrowserElementChild.js.
    if ((mIsForApp || mIsForBrowser) &&
        Preferences::GetBool("dom.ipc.processPrelaunch.enabled", false)) {
        PreloadSlowThings();
    }
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
                                   const bool& aIsSharing)
{
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<nsVolume> volume = new nsVolume(aFsName, aVolumeName, aState,
                                             aMountGeneration, aIsMediaPresent,
                                             aIsSharing);

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

    nsCOMPtr<nsICancelableRunnable> runnable =
        do_QueryReferent(mMemoryMinimizerRunnable);

    // Cancel the previous task if it's still pending.
    if (runnable) {
        runnable->Cancel();
        runnable = nullptr;
    }

    mgr->MinimizeMemoryUsage(/* callback = */ nullptr,
                             getter_AddRefs(runnable));
    mMemoryMinimizerRunnable = do_GetWeakReference(runnable);
    return true;
}

bool
ContentChild::RecvCancelMinimizeMemoryUsage()
{
    nsCOMPtr<nsICancelableRunnable> runnable =
        do_QueryReferent(mMemoryMinimizerRunnable);
    if (runnable) {
        runnable->Cancel();
        mMemoryMinimizerRunnable = nullptr;
    }

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
        child->SetProcessName(NS_LITERAL_STRING("(Preallocated app)"));
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
#endif // MOZ_NUWA_PROCESS

}
