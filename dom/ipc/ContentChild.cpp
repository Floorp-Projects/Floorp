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
#include "mozilla/dom/StorageChild.h"
#include "mozilla/Hal.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/ipc/XPCShellEnvironment.h"
#include "mozilla/jsipc/PContextWrapperChild.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/PCompositorChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/Preferences.h"

#include "nsIMemoryReporter.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIObserverService.h"
#include "nsTObserverArray.h"
#include "nsIObserver.h"
#include "nsIScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsWeakReference.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsJSEnvironment.h"
#include "SandboxHal.h"
#include "nsDebugImpl.h"
#include "nsLayoutStylesheetCache.h"

#include "IHistory.h"
#include "nsDocShellCID.h"
#include "nsNetUtil.h"

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/task.h"

#include "nsChromeRegistryContent.h"
#include "mozilla/chrome/RegistryMessageUtils.h"
#include "nsFrameMessageManager.h"

#include "nsIGeolocationProvider.h"
#include "mozilla/dom/PMemoryReportRequestChild.h"

#ifdef MOZ_PERMISSIONS
#include "nsPermission.h"
#include "nsPermissionManager.h"
#endif

#if defined(MOZ_WIDGET_ANDROID)
#include "APKOpen.h"
#endif

#if defined(MOZ_WIDGET_GONK)
#include "nsVolume.h"
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#endif

#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

#include "mozilla/dom/indexedDB/PIndexedDBChild.h"
#include "mozilla/dom/sms/SmsChild.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestChild.h"
#include "mozilla/dom/bluetooth/PBluetoothChild.h"

#include "nsDOMFile.h"
#include "nsIRemoteBlob.h"
#include "ProcessUtils.h"
#include "StructuredCloneUtils.h"
#include "URIUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsDeviceStorage.h"
#include "AudioChannelService.h"

using namespace base;
using namespace mozilla::docshell;
using namespace mozilla::dom::bluetooth;
using namespace mozilla::dom::devicestorage;
using namespace mozilla::dom::sms;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::hal_sandbox;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;
#if defined(MOZ_WIDGET_GONK)
using namespace mozilla::system;
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

ContentChild* ContentChild::sSingleton;

ContentChild::ContentChild()
 : TabContext()
 , mID(uint64_t(-1))
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
    gtk_init(NULL, NULL);
#endif

#ifdef MOZ_WIDGET_QT
    // sigh, seriously
    nsQAppInstance::AddRef();
#endif

#ifdef MOZ_X11
    // Do this after initializing GDK, or GDK will install its own handler.
    XRE_InstallX11ErrorHandler();
#endif

    NS_ASSERTION(!sSingleton, "only one ContentChild per child");

    Open(aChannel, aParentHandle, aIOLoop);
    sSingleton = this;

#ifdef MOZ_CRASHREPORTER
    SendPCrashReporterConstructor(CrashReporter::CurrentThreadId(),
                                  XRE_GetProcessType());
#if defined(MOZ_WIDGET_ANDROID)
    PCrashReporterChild* crashreporter = ManagedPCrashReporterChild()[0];

    InfallibleTArray<Mapping> mappings;
    const struct mapping_info *info = getLibraryMapping();
    while (info && info->name) {
        mappings.AppendElement(Mapping(nsDependentCString(info->name),
                                       nsDependentCString(info->file_id),
                                       info->base,
                                       info->len,
                                       info->offset));
        info++;
    }
    crashreporter->SendAddLibraryMappings(mappings);
#endif
#endif

    bool startBackground = true;
    SendGetProcessAttributes(&mID, &startBackground,
                             &mIsForApp, &mIsForBrowser);
    hal::SetProcessPriority(
        GetCurrentProcId(),
        startBackground ? hal::PROCESS_PRIORITY_BACKGROUND:
                          hal::PROCESS_PRIORITY_FOREGROUND);
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
        printf_stderr("\n\nCHILDCHILDCHILDCHILD\n  [%s] debug me @%d\n\n", name, _getpid());
        Sleep(30000);
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
}

PMemoryReportRequestChild*
ContentChild::AllocPMemoryReportRequest()
{
    return new MemoryReportRequestChild();
}

// This is just a wrapper for InfallibleTArray<MemoryReport> that implements
// nsISupports, so it can be passed to nsIMemoryMultiReporter::CollectReports.
class MemoryReportsWrapper MOZ_FINAL : public nsISupports {
public:
    NS_DECL_ISUPPORTS
    MemoryReportsWrapper(InfallibleTArray<MemoryReport> *r) : mReports(r) { }
    InfallibleTArray<MemoryReport> *mReports;
};
NS_IMPL_ISUPPORTS0(MemoryReportsWrapper)

class MemoryReportCallback MOZ_FINAL : public nsIMemoryMultiReporterCallback
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
, nsIMemoryMultiReporterCallback
)

bool
ContentChild::RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* child)
{
    
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");

    InfallibleTArray<MemoryReport> reports;

    nsPrintfCString process("Content (%d)", getpid());

    // First do the vanilla memory reporters.
    nsCOMPtr<nsISimpleEnumerator> e;
    mgr->EnumerateReporters(getter_AddRefs(e));
    bool more;
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryReporter> r;
      e->GetNext(getter_AddRefs(r));

      nsCString path;
      int32_t kind;
      int32_t units;
      int64_t amount;
      nsCString desc;

      if (NS_SUCCEEDED(r->GetPath(path)) &&
          NS_SUCCEEDED(r->GetKind(&kind)) &&
          NS_SUCCEEDED(r->GetUnits(&units)) &&
          NS_SUCCEEDED(r->GetAmount(&amount)) &&
          NS_SUCCEEDED(r->GetDescription(desc)))
      {
        MemoryReport memreport(process, path, kind, units, amount, desc);
        reports.AppendElement(memreport);
      }
    }

    // Then do the memory multi-reporters, by calling CollectReports on each
    // one, whereupon the callback will turn each measurement into a
    // MemoryReport.
    mgr->EnumerateMultiReporters(getter_AddRefs(e));
    nsRefPtr<MemoryReportsWrapper> wrappedReports =
        new MemoryReportsWrapper(&reports);
    nsRefPtr<MemoryReportCallback> cb = new MemoryReportCallback(process);
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
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
ContentChild::DeallocPMemoryReportRequest(PMemoryReportRequestChild* actor)
{
    delete actor;
    return true;
}

bool
ContentChild::RecvDumpMemoryReportsToFile(const nsString& aIdentifier,
                                          const bool& aMinimizeMemoryUsage,
                                          const bool& aDumpChildProcesses)
{
    nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");

    dumper->DumpMemoryReportsToFile(
        aIdentifier, aMinimizeMemoryUsage, aDumpChildProcesses);
    return true;
}

bool
ContentChild::RecvDumpGCAndCCLogsToFile(const nsString& aIdentifier,
                                        const bool& aDumpChildProcesses)
{
    nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");

    dumper->DumpGCAndCCLogsToFile(
        aIdentifier, aDumpChildProcesses);
    return true;
}

PCompositorChild*
ContentChild::AllocPCompositor(mozilla::ipc::Transport* aTransport,
                               base::ProcessId aOtherProcess)
{
    return CompositorChild::Create(aTransport, aOtherProcess);
}

PImageBridgeChild*
ContentChild::AllocPImageBridge(mozilla::ipc::Transport* aTransport,
                                base::ProcessId aOtherProcess)
{
    return ImageBridgeChild::StartUpInChildProcess(aTransport, aOtherProcess);
}

static CancelableTask* sFirstIdleTask;

static void FirstIdle(void)
{
    MOZ_ASSERT(sFirstIdleTask);
    sFirstIdleTask = nullptr;
    ContentChild::GetSingleton()->SendFirstIdle();
}

PBrowserChild*
ContentChild::AllocPBrowser(const IPCTabContext& aContext,
                            const uint32_t& aChromeFlags)
{
    static bool firstIdleTaskPosted = false;
    if (!firstIdleTaskPosted) {
        MOZ_ASSERT(!sFirstIdleTask);
        sFirstIdleTask = NewRunnableFunction(FirstIdle);
        MessageLoop::current()->PostIdleTask(FROM_HERE, sFirstIdleTask);
        firstIdleTaskPosted = true;
    }

    // We'll happily accept any kind of IPCTabContext here; we don't need to
    // check that it's of a certain type for security purposes, because we
    // believe whatever the parent process tells us.

    nsRefPtr<TabChild> child = TabChild::Create(TabContext(aContext), aChromeFlags);

    // The ref here is released below.
    return child.forget().get();
}

bool
ContentChild::DeallocPBrowser(PBrowserChild* iframe)
{
    TabChild* child = static_cast<TabChild*>(iframe);
    NS_RELEASE(child);
    return true;
}

PBlobChild*
ContentChild::AllocPBlob(const BlobConstructorParams& aParams)
{
  return BlobChild::Create(aParams);
}

bool
ContentChild::DeallocPBlob(PBlobChild* aActor)
{
  delete aActor;
  return true;
}

BlobChild*
ContentChild::GetOrCreateActorForBlob(nsIDOMBlob* aBlob)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aBlob, "Null pointer!");

  nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(aBlob);
  if (remoteBlob) {
    BlobChild* actor =
      static_cast<BlobChild*>(static_cast<PBlobChild*>(remoteBlob->GetPBlob()));
    NS_ASSERTION(actor, "Null actor?!");

    return actor;
  }

  // XXX This is only safe so long as all blob implementations in our tree
  //     inherit nsDOMFileBase. If that ever changes then this will need to grow
  //     a real interface or something.
  const nsDOMFileBase* blob = static_cast<nsDOMFileBase*>(aBlob);

  BlobConstructorParams params;

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

      rv = file->GetName(fileParams.name());
      NS_ENSURE_SUCCESS(rv, nullptr);

      rv = file->GetMozLastModifiedDate(&fileParams.modDate());
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

  BlobChild* actor = BlobChild::Create(aBlob);
  NS_ENSURE_TRUE(actor, nullptr);

  if (!SendPBlobConstructor(actor, params)) {
    return nullptr;
  }

  return actor;
}

PCrashReporterChild*
ContentChild::AllocPCrashReporter(const mozilla::dom::NativeThreadId& id,
                                  const uint32_t& processType)
{
#ifdef MOZ_CRASHREPORTER
    return new CrashReporterChild();
#else
    return nullptr;
#endif
}

bool
ContentChild::DeallocPCrashReporter(PCrashReporterChild* crashreporter)
{
    delete crashreporter;
    return true;
}

PHalChild*
ContentChild::AllocPHal()
{
    return CreateHalChild();
}

bool
ContentChild::DeallocPHal(PHalChild* aHal)
{
    delete aHal;
    return true;
}

PIndexedDBChild*
ContentChild::AllocPIndexedDB()
{
  NS_NOTREACHED("Should never get here!");
  return NULL;
}

bool
ContentChild::DeallocPIndexedDB(PIndexedDBChild* aActor)
{
  delete aActor;
  return true;
}

PTestShellChild*
ContentChild::AllocPTestShell()
{
    return new TestShellChild();
}

bool
ContentChild::DeallocPTestShell(PTestShellChild* shell)
{
    delete shell;
    return true;
}

bool
ContentChild::RecvPTestShellConstructor(PTestShellChild* actor)
{
    actor->SendPContextWrapperConstructor()->SendPObjectWrapperConstructor(true);
    return true;
}

PDeviceStorageRequestChild*
ContentChild::AllocPDeviceStorageRequest(const DeviceStorageParams& aParams)
{
    return new DeviceStorageRequestChild();
}

bool
ContentChild::DeallocPDeviceStorageRequest(PDeviceStorageRequestChild* aDeviceStorage)
{
    delete aDeviceStorage;
    return true;
}

PNeckoChild* 
ContentChild::AllocPNecko()
{
    return new NeckoChild();
}

bool 
ContentChild::DeallocPNecko(PNeckoChild* necko)
{
    delete necko;
    return true;
}

PExternalHelperAppChild*
ContentChild::AllocPExternalHelperApp(const OptionalURIParams& uri,
                                      const nsCString& aMimeContentType,
                                      const nsCString& aContentDisposition,
                                      const bool& aForceSave,
                                      const int64_t& aContentLength,
                                      const OptionalURIParams& aReferrer)
{
    ExternalHelperAppChild *child = new ExternalHelperAppChild();
    child->AddRef();
    return child;
}

bool
ContentChild::DeallocPExternalHelperApp(PExternalHelperAppChild* aService)
{
    ExternalHelperAppChild *child = static_cast<ExternalHelperAppChild*>(aService);
    child->Release();
    return true;
}

PSmsChild*
ContentChild::AllocPSms()
{
    return new SmsChild();
}

bool
ContentChild::DeallocPSms(PSmsChild* aSms)
{
    delete aSms;
    return true;
}

PStorageChild*
ContentChild::AllocPStorage(const StorageConstructData& aData)
{
    NS_NOTREACHED("We should never be manually allocating PStorageChild actors");
    return nullptr;
}

bool
ContentChild::DeallocPStorage(PStorageChild* aActor)
{
    StorageChild* child = static_cast<StorageChild*>(aActor);
    child->ReleaseIPDLReference();
    return true;
}

PBluetoothChild*
ContentChild::AllocPBluetooth()
{
#ifdef MOZ_B2G_BT
    MOZ_NOT_REACHED("No one should be allocating PBluetoothChild actors");
    return nullptr;
#else
    MOZ_NOT_REACHED("No support for bluetooth on this platform!");
    return nullptr;
#endif
}

bool
ContentChild::DeallocPBluetooth(PBluetoothChild* aActor)
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
    case MsgNotAllowed:
    case MsgPayloadError:
    case MsgProcessingError:
    case MsgRouteError:
    case MsgValueError:
        NS_RUNTIMEABORT("aborting because of fatal error");

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
                                     const ClonedMessageData& aData)
{
  nsRefPtr<nsFrameMessageManager> cpm = nsFrameMessageManager::sChildProcessManager;
  if (cpm) {
    const SerializedStructuredCloneBuffer& buffer = aData.data();
    const InfallibleTArray<PBlobChild*>& blobChildList = aData.blobsChild();
    StructuredCloneData cloneData;
    cloneData.mData = buffer.data;
    cloneData.mDataLength = buffer.dataLength;
    if (!blobChildList.IsEmpty()) {
      uint32_t length = blobChildList.Length();
      cloneData.mClosure.mBlobs.SetCapacity(length);
      for (uint32_t i = 0; i < length; ++i) {
        BlobChild* blobChild = static_cast<BlobChild*>(blobChildList[i]);
        MOZ_ASSERT(blobChild);
        nsCOMPtr<nsIDOMBlob> blob = blobChild->GetBlob();
        MOZ_ASSERT(blob);
        cloneData.mClosure.mBlobs.AppendElement(blob);
      }
    }
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()),
                        aMsg, false, &cloneData, nullptr, nullptr);
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
    nsJSContext::GarbageCollectNow(js::gcreason::DOM_IPC);
    return true;
}

bool
ContentChild::RecvCycleCollect()
{
    nsJSContext::GarbageCollectNow(js::gcreason::DOM_IPC);
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
ContentChild::RecvAppInfo(const nsCString& version, const nsCString& buildID)
{
    mAppInfo.version.Assign(version);
    mAppInfo.buildID.Assign(buildID);

    PreloadSlowThings();
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
ContentChild::RecvFilePathUpdate(const nsString& type, const nsString& path, const nsCString& aReason)
{
    nsCOMPtr<nsIFile> file;
    NS_NewLocalFile(path, false, getter_AddRefs(file));

    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(type, file);

    nsString reason;
    CopyASCIItoUTF16(aReason, reason);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(dsf, "file-watcher-update", reason.get());
    return true;
}

bool
ContentChild::RecvFileSystemUpdate(const nsString& aFsName, const nsString& aName, const int32_t &aState)
{
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<nsVolume> volume = new nsVolume(aFsName, aName, aState);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    nsString stateStr(NS_ConvertUTF8toUTF16(volume->StateStr()));
    obs->NotifyObservers(volume, NS_VOLUME_STATE_CHANGED, stateStr.get());
#endif
    return true;
}

} // namespace dom
} // namespace mozilla
