/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"

#include "ContentParent.h"
#include "TabParent.h"

#if defined(ANDROID) || defined(LINUX)
# include <sys/time.h>
# include <sys/resource.h>
#endif

#ifdef MOZ_WIDGET_GONK
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "chrome/common/process_watcher.h"

#include "mozilla/a11y/PDocAccessible.h"
#include "AudioChannelService.h"
#include "BlobParent.h"
#include "CrashReporterParent.h"
#include "DeviceStorageStatics.h"
#include "GMPServiceParent.h"
#include "HandlerServiceParent.h"
#include "IHistory.h"
#include "imgIContainer.h"
#if defined(XP_WIN) && defined(ACCESSIBILITY)
#include "mozilla/a11y/AccessibleWrap.h"
#include "mozilla/WindowsVersion.h"
#endif
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/DataStorage.h"
#include "mozilla/devtools/HeapSnapshotTempFileHelperParent.h"
#include "mozilla/docshell/OfflineCacheUpdateParent.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DOMStorageIPC.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ExternalHelperAppParent.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/dom/GeolocationBinding.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/dom/PContentBridgeParent.h"
#include "mozilla/dom/PContentPermissionRequestParent.h"
#include "mozilla/dom/PCycleCollectWithLogsParent.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestParent.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/Permissions.h"
#include "mozilla/dom/PresentationParent.h"
#include "mozilla/dom/PPresentationParent.h"
#include "mozilla/dom/PushNotifier.h"
#include "mozilla/dom/FlyWebPublishedServerIPC.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "mozilla/dom/time/DateCacheCleaner.h"
#include "mozilla/dom/URLClassifierParent.h"
#include "mozilla/embedding/printingui/PrintingParent.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/PSendStreamParent.h"
#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layers/PAPZParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/media/MediaParent.h"
#include "mozilla/Move.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/plugins/PluginBridge.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ProcessHangMonitorIPC.h"
#ifdef MOZ_ENABLE_PROFILER_SPS
#include "mozilla/ProfileGatherer.h"
#endif
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WebBrowserPersistDocumentParent.h"
#include "mozilla/Unused.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsAppRunner.h"
#include "nsCDefaultURIFixup.h"
#include "nsCExternalHandlerService.h"
#include "nsCOMPtr.h"
#include "nsChromeRegistryChrome.h"
#include "nsConsoleMessage.h"
#include "nsConsoleService.h"
#include "nsContentUtils.h"
#include "nsDebugImpl.h"
#include "nsFrameMessageManager.h"
#include "nsHashPropertyBag.h"
#include "nsIAlertsService.h"
#include "nsIClipboard.h"
#include "nsContentPermissionHelper.h"
#include "nsICycleCollectorListener.h"
#include "nsIDocShellTreeOwner.h"
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
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMemoryReporter.h"
#include "nsIMozBrowserFrame.h"
#include "nsIMutable.h"
#include "nsINSSU2FToken.h"
#include "nsIObserverService.h"
#include "nsIParentChannel.h"
#include "nsIPresShell.h"
#include "nsIRemoteWindowContext.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsISiteSecurityService.h"
#include "nsISpellChecker.h"
#include "nsISupportsPrimitives.h"
#include "nsITimer.h"
#include "nsIURIFixup.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIXULWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsWindowWatcher.h"
#include "nsIXULRuntime.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "nsMemoryInfoDumper.h"
#include "nsMemoryReporterManager.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleSheetService.h"
#include "nsThreadUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsWidgetsCID.h"
#include "ProcessPriorityManager.h"
#include "SandboxHal.h"
#include "ScreenManagerParent.h"
#include "SourceSurfaceRawData.h"
#include "TabParent.h"
#include "URIUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShell.h"
#include "nsDocShell.h"
#include "nsOpenURIInFrameParams.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "gfxPrefs.h"
#include "prio.h"
#include "private/pprio.h"
#include "ContentProcessManager.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/psm/PSMContentListener.h"
#include "nsPluginHost.h"
#include "nsPluginTags.h"
#include "nsIBlocklistService.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsICaptivePortalService.h"

#include "nsIBidiKeyboard.h"

#include "nsLayoutStylesheetCache.h"

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

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#endif

#include "mozilla/RemoteSpellCheckEngineParent.h"

#include "Crypto.h"

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/SpeechSynthesisParent.h"
#endif

#if defined(MOZ_CONTENT_SANDBOX) && defined(XP_LINUX)
#include "mozilla/SandboxInfo.h"
#include "mozilla/SandboxBroker.h"
#include "mozilla/SandboxBrokerPolicyFactory.h"
#endif

#ifdef MOZ_TOOLKIT_SEARCH
#include "nsIBrowserSearchService.h"
#endif

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "nsIProfiler.h"
#include "nsIProfileSaveEvent.h"
#endif

#ifdef XP_WIN
#include "mozilla/widget/AudioSession.h"
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsThread.h"
#endif

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

// For VP9Benchmark::sBenchmarkFpsPref
#include "Benchmark.h"

static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

#if defined(XP_WIN)
// e10s forced enable pref, defined in nsAppRunner.cpp
extern const char* kForceEnableE10sPref;
#endif

using base::ChildPrivileges;
using base::KillProcess;
#ifdef MOZ_ENABLE_PROFILER_SPS
using mozilla::ProfileGatherer;
#endif

#ifdef MOZ_CRASHREPORTER
using namespace CrashReporter;
#endif
using namespace mozilla::dom::devicestorage;
using namespace mozilla::dom::power;
using namespace mozilla::media;
using namespace mozilla::embedding;
using namespace mozilla::gfx;
using namespace mozilla::gmp;
using namespace mozilla::hal;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::net;
using namespace mozilla::jsipc;
using namespace mozilla::psm;
using namespace mozilla::widget;

// XXX Workaround for bug 986973 to maintain the existing broken semantics
template<>
struct nsIConsoleService::COMTypeInfo<nsConsoleService, void> {
  static const nsIID kIID;
};
const nsIID nsIConsoleService::COMTypeInfo<nsConsoleService, void>::kIID = NS_ICONSOLESERVICE_IID;

namespace mozilla {
namespace dom {

#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"
#define NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC "ipc:network:set-connectivity"

class MemoryReportRequestParent : public PMemoryReportRequestParent
{
public:
  explicit MemoryReportRequestParent(uint32_t aGeneration);

  virtual ~MemoryReportRequestParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvReport(const MemoryReport& aReport) override;
  virtual mozilla::ipc::IPCResult Recv__delete__() override;

private:
  const uint32_t mGeneration;
  // Non-null if we haven't yet called EndProcessReport() on it.
  RefPtr<nsMemoryReporterManager> mReporterManager;

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
  NS_WARNING_ASSERTION(mReporterManager, "GetOrCreate failed");
}

mozilla::ipc::IPCResult
MemoryReportRequestParent::RecvReport(const MemoryReport& aReport)
{
  if (mReporterManager) {
    mReporterManager->HandleChildReport(mGeneration, aReport);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
MemoryReportRequestParent::Recv__delete__()
{
  // Notifying the reporter manager is done in ActorDestroy, because
  // it needs to happen even if the child process exits mid-report.
  // (The reporter manager will time out eventually, but let's avoid
  // that if possible.)
  return IPC_OK();
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
  virtual mozilla::ipc::IPCResult RecvCloseGCLog() override
  {
    Unused << mSink->CloseGCLog();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvCloseCCLog() override
  {
    Unused << mSink->CloseCCLog();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult Recv__delete__() override
  {
    // Report completion to mCallback only on successful
    // completion of the protocol.
    nsCOMPtr<nsIFile> gcLog, ccLog;
    mSink->GetGcLog(getter_AddRefs(gcLog));
    mSink->GetCcLog(getter_AddRefs(ccLog));
    Unused << mCallback->OnDump(gcLog, ccLog, /* parent = */ false);
    return IPC_OK();
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
ContentParentsMemoryReporter::CollectReports(
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aData,
  bool aAnonymize)
{
  AutoTArray<ContentParent*, 16> cps;
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

    aHandleReport->Callback(/* process */ EmptyCString(), path,
                            KIND_OTHER, UNITS_COUNT,
                            numQueuedMessages, desc, aData);
  }

  return NS_OK;
}

nsTArray<ContentParent*>* ContentParent::sBrowserContentParents;
nsTArray<ContentParent*>* ContentParent::sLargeAllocationContentParents;
nsTArray<ContentParent*>* ContentParent::sPrivateContent;
StaticAutoPtr<LinkedList<ContentParent> > ContentParent::sContentParents;
#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
UniquePtr<SandboxBrokerPolicyFactory> ContentParent::sSandboxBrokerPolicyFactory;
#endif

// This is true when subprocess launching is enabled.  This is the
// case between StartUp() and ShutDown() or JoinAllSubprocesses().
static bool sCanLaunchSubprocesses;

// Set to true if the DISABLE_UNSAFE_CPOW_WARNINGS environment variable is
// set.
static bool sDisableUnsafeCPOWWarnings = false;

// The first content child has ID 1, so the chrome process can have ID 0.
static uint64_t gContentChildID = 1;

static const char* sObserverTopics[] = {
  "xpcom-shutdown",
  "profile-before-change",
  NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC,
  NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC,
  NS_IPC_CAPTIVE_PORTAL_SET_STATE,
  "memory-pressure",
  "child-gc-request",
  "child-cc-request",
  "child-mmu-request",
  "last-pb-context-exited",
  "file-watcher-update",
#ifdef ACCESSIBILITY
  "a11y-init-or-shutdown",
#endif
#ifdef MOZ_ENABLE_PROFILER_SPS
  "profiler-started",
  "profiler-stopped",
  "profiler-paused",
  "profiler-resumed",
  "profiler-subprocess-gather",
  "profiler-subprocess",
#endif
  "cacheservice:empty-cache",
};

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

  sDisableUnsafeCPOWWarnings = PR_GetEnv("DISABLE_UNSAFE_CPOW_WARNINGS");

#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
  sSandboxBrokerPolicyFactory = MakeUnique<SandboxBrokerPolicyFactory>();
#endif
}

/*static*/ void
ContentParent::ShutDown()
{
  // No-op for now.  We rely on normal process shutdown and
  // ClearOnShutdown() to clean up our state.
  sCanLaunchSubprocesses = false;

#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
  sSandboxBrokerPolicyFactory = nullptr;
#endif
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

  AutoTArray<ContentParent*, 8> processes;
  GetAll(processes);
  if (processes.IsEmpty()) {
    printf_stderr("There are no live subprocesses.");
    return;
  }

  printf_stderr("Subprocesses are still alive.  Doing emergency join.\n");

  bool done = false;
  Monitor monitor("mozilla.dom.ContentParent.JoinAllSubprocesses");
  XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(
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
                                          ContentParent* aOpener,
                                          bool aLargeAllocationProcess)
{
  nsTArray<ContentParent*>* contentParents;
  int32_t maxContentParents;

  // Decide which pool of content parents we are going to be pulling from based
  // on the aLargeAllocationProcess flag.
  if (aLargeAllocationProcess) {
    if (!sLargeAllocationContentParents) {
      sLargeAllocationContentParents = new nsTArray<ContentParent*>();
    }
    contentParents = sLargeAllocationContentParents;

    maxContentParents = Preferences::GetInt("dom.ipc.dedicatedProcessCount", 2);
  } else {
    if (!sBrowserContentParents) {
      sBrowserContentParents = new nsTArray<ContentParent*>();
    }
    contentParents = sBrowserContentParents;

    maxContentParents = Preferences::GetInt("dom.ipc.processCount", 1);
  }

  if (maxContentParents < 1) {
    maxContentParents = 1;
  }

  if (contentParents->Length() >= uint32_t(maxContentParents)) {
    uint32_t maxSelectable = std::min(static_cast<uint32_t>(contentParents->Length()),
                                      static_cast<uint32_t>(maxContentParents));
    uint32_t startIdx = rand() % maxSelectable;
    uint32_t currIdx = startIdx;
    do {
      RefPtr<ContentParent> p = (*contentParents)[currIdx];
      NS_ASSERTION(p->IsAlive(), "Non-alive contentparent in sBrowserContentParents?");
      if (p->mOpener == aOpener) {
        return p.forget();
      }
      currIdx = (currIdx + 1) % maxSelectable;
    } while (currIdx != startIdx);
  }

  RefPtr<ContentParent> p = new ContentParent(aOpener,
                                              aForBrowserElement);

  if (!p->LaunchSubprocess(aPriority)) {
    return nullptr;
  }

  p->Init();

  p->mLargeAllocationProcess = aLargeAllocationProcess;

  contentParents->AppendElement(p);
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

  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(aFrameElement);
  if (!browserFrame) {
    return PROCESS_PRIORITY_FOREGROUND;
  }

  return PROCESS_PRIORITY_FOREGROUND;
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
    Unused << cp->SendUpdateWindow((uintptr_t)hwnd);
  }
}
#endif // defined(XP_WIN)

mozilla::ipc::IPCResult
ContentParent::RecvCreateChildProcess(const IPCTabContext& aContext,
                                      const hal::ProcessPriority& aPriority,
                                      const TabId& aOpenerTabId,
                                      ContentParentId* aCpId,
                                      bool* aIsForBrowser,
                                      TabId* aTabId)
{
#if 0
  if (!CanOpenBrowser(aContext)) {
      return false;
  }
#endif
  RefPtr<ContentParent> cp;
  MaybeInvalidTabContext tc(aContext);
  if (!tc.IsValid()) {
    NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                             "the child process. (%s)",
                             tc.GetInvalidReason()).get());
    return IPC_FAIL_NO_REASON(this);
  }

  cp = GetNewOrUsedBrowserProcess(/* isBrowserElement = */ true,
                                  aPriority, this);

  if (!cp) {
    *aCpId = 0;
    *aIsForBrowser = false;
    return IPC_OK();
  }

  *aCpId = cp->ChildID();
  *aIsForBrowser = cp->IsForBrowser();

  ContentProcessManager *cpm = ContentProcessManager::GetSingleton();
  cpm->AddContentProcess(cp, this->ChildID());

  if (cpm->AddGrandchildProcess(this->ChildID(), cp->ChildID())) {
    // Pre-allocate a TabId here to save one time IPC call at app startup.
    *aTabId = AllocateTabId(aOpenerTabId, aContext, cp->ChildID());
    if (*aTabId == 0) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  return IPC_FAIL_NO_REASON(this);
}

mozilla::ipc::IPCResult
ContentParent::RecvBridgeToChildProcess(const ContentParentId& aCpId)
{
  ContentProcessManager *cpm = ContentProcessManager::GetSingleton();
  ContentParent* cp = cpm->GetContentProcessById(aCpId);

  if (cp) {
    ContentParentId parentId;
    if (cpm->GetParentProcessId(cp->ChildID(), &parentId) &&
      parentId == this->ChildID()) {
      if (NS_FAILED(PContentBridge::Bridge(this, cp))) {
        return IPC_FAIL_NO_REASON(this);
      }
      return IPC_OK();
    }
  }

  // You can't bridge to a process you didn't open!
  KillHard("BridgeToChildProcess");
  return IPC_FAIL_NO_REASON(this);
}

static nsIDocShell* GetOpenerDocShellHelper(Element* aFrameElement)
{
  // Propagate the private-browsing status of the element's parent
  // docshell to the remote docshell, via the chrome flags.
  nsCOMPtr<Element> frameElement = do_QueryInterface(aFrameElement);
  MOZ_ASSERT(frameElement);
  nsPIDOMWindowOuter* win = frameElement->OwnerDoc()->GetWindow();
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

mozilla::ipc::IPCResult
ContentParent::RecvCreateGMPService()
{
  if (!PGMPService::Open(this)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvLoadPlugin(const uint32_t& aPluginId, nsresult* aRv, uint32_t* aRunID)
{
  *aRv = NS_OK;
  if (!mozilla::plugins::SetupBridge(aPluginId, this, false, aRv, aRunID)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvUngrabPointer(const uint32_t& aTime)
{
#if !defined(MOZ_WIDGET_GTK)
  NS_RUNTIMEABORT("This message only makes sense on GTK platforms");
  return IPC_OK();
#else
  gdk_pointer_ungrab(aTime);
  return IPC_OK();
#endif
}

mozilla::ipc::IPCResult
ContentParent::RecvRemovePermission(const IPC::Principal& aPrincipal,
                                    const nsCString& aPermissionType,
                                    nsresult* aRv) {
  *aRv = Permissions::RemovePermission(aPrincipal, aPermissionType.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvConnectPluginBridge(const uint32_t& aPluginId, nsresult* aRv)
{
  *aRv = NS_OK;
  // We don't need to get the run ID for the plugin, since we already got it
  // in the first call to SetupBridge in RecvLoadPlugin, so we pass in a dummy
  // pointer and just throw it away.
  uint32_t dummy = 0;
  if (!mozilla::plugins::SetupBridge(aPluginId, this, true, aRv, &dummy)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetBlocklistState(const uint32_t& aPluginId,
                                     uint32_t* aState)
{
  *aState = nsIBlocklistService::STATE_BLOCKED;

  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  if (!pluginHost) {
    NS_WARNING("Plugin host not found");
    return IPC_FAIL_NO_REASON(this);
  }
  nsPluginTag* tag =  pluginHost->PluginWithId(aPluginId);

  if (!tag) {
    // Default state is blocked anyway
    NS_WARNING("Plugin tag not found. This should never happen, but to avoid a crash we're forcibly blocking it");
    return IPC_OK();
  }

  if (NS_FAILED(tag->GetBlocklistState(aState))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvFindPlugins(const uint32_t& aPluginEpoch,
                               nsresult* aRv,
                               nsTArray<PluginTag>* aPlugins,
                               uint32_t* aNewPluginEpoch)
{
  *aRv = mozilla::plugins::FindPluginsForContent(aPluginEpoch, aPlugins, aNewPluginEpoch);
  return IPC_OK();
}

/*static*/ TabParent*
ContentParent::CreateBrowser(const TabContext& aContext,
                             Element* aFrameElement,
                             ContentParent* aOpenerContentParent,
                             bool aFreshProcess)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);

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

  RefPtr<nsIContentParent> constructorSender;
  if (isInContentProcess) {
    MOZ_ASSERT(aContext.IsMozBrowserElement());
    constructorSender = CreateContentBridgeParent(aContext, initialPriority,
                                                  openerTabId, &tabId);
  } else {
    if (aOpenerContentParent) {
      constructorSender = aOpenerContentParent;
    } else {
      constructorSender =
        GetNewOrUsedBrowserProcess(aContext.IsMozBrowserElement(),
                                   initialPriority, nullptr, aFreshProcess);
      if (!constructorSender) {
        return nullptr;
      }
    }
    tabId = AllocateTabId(openerTabId,
                          aContext.AsIPCTabContext(),
                          constructorSender->ChildID());
  }
  if (constructorSender) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    docShell->GetTreeOwner(getter_AddRefs(treeOwner));
    if (!treeOwner) {
      return nullptr;
    }

    nsCOMPtr<nsIWebBrowserChrome> wbc = do_GetInterface(treeOwner);
    if (!wbc) {
      return nullptr;
    }
    uint32_t chromeFlags = 0;
    wbc->GetChromeFlags(&chromeFlags);

    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
    if (loadContext && loadContext->UsePrivateBrowsing()) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
    }
    if (docShell->GetAffectPrivateSessionLifetime()) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
    }

    if (tabId == 0) {
      return nullptr;
    }
    RefPtr<TabParent> tp(new TabParent(constructorSender, tabId,
                                       aContext, chromeFlags));
    tp->SetInitedByParent();

    PBrowserParent* browser =
    constructorSender->SendPBrowserConstructor(
      // DeallocPBrowserParent() releases this ref.
      tp.forget().take(), tabId,
      aContext.AsIPCTabContext(),
      chromeFlags,
      constructorSender->ChildID(),
      constructorSender->IsForBrowser());

    if (aFreshProcess) {
      Unused << browser->SendSetFreshProcess();
    }

    if (browser) {
      RefPtr<TabParent> constructedTabParent = TabParent::GetFrom(browser);
      constructedTabParent->SetOwnerElement(aFrameElement);
      return constructedTabParent;
    }
  }
  return nullptr;
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
  bool isForBrowser;
  if (!child->SendCreateChildProcess(aContext.AsIPCTabContext(),
                                     aPriority,
                                     aOpenerTabId,
                                     &cpId,
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
  parent->SetIsForBrowser(isForBrowser);
  return parent;
}

void
ContentParent::GetAll(nsTArray<ContentParent*>& aArray)
{
  aArray.Clear();

  for (auto* cp : AllProcesses(eLive)) {
    aArray.AppendElement(cp);
  }
}

void
ContentParent::GetAllEvenIfDead(nsTArray<ContentParent*>& aArray)
{
  aArray.Clear();

  for (auto* cp : AllProcesses(eAll)) {
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
    nsAutoString cpId;
    cpId.AppendInt(static_cast<uint64_t>(this->ChildID()));
    obs->NotifyObservers(static_cast<nsIObserver*>(this), "ipc:content-created", cpId.get());
  }

#ifdef ACCESSIBILITY
  // If accessibility is running in chrome process then start it in content
  // process.
  if (nsIPresShell::IsAccessibilityActive()) {
#if defined(XP_WIN)
    if (IsVistaOrLater()) {
      Unused <<
        SendActivateA11y(a11y::AccessibleWrap::GetContentProcessIdFor(ChildID()));
    }
#else
    Unused << SendActivateA11y(0);
#endif
  }
#endif

#ifdef MOZ_ENABLE_PROFILER_SPS
  nsCOMPtr<nsIProfiler> profiler(do_GetService("@mozilla.org/tools/profiler;1"));
  bool profilerActive = false;
  DebugOnly<nsresult> rv = profiler->IsActive(&profilerActive);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (profilerActive) {
    nsCOMPtr<nsIProfilerStartParams> currentProfilerParams;
    rv = profiler->GetStartParams(getter_AddRefs(currentProfilerParams));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<nsISupports> gatherer;
    rv = profiler->GetProfileGatherer(getter_AddRefs(gatherer));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mGatherer = static_cast<ProfileGatherer*>(gatherer.get());

    StartProfiler(currentProfilerParams);
  }
#endif

  RefPtr<GeckoMediaPluginServiceParent> gmps(GeckoMediaPluginServiceParent::GetSingleton());
  gmps->UpdateContentProcessGMPCapabilities();
}

namespace {

class RemoteWindowContext final : public nsIRemoteWindowContext
                                , public nsIInterfaceRequestor
{
public:
  explicit RemoteWindowContext(TabParent* aTabParent)
  : mTabParent(aTabParent)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREMOTEWINDOWCONTEXT

private:
  ~RemoteWindowContext();
  RefPtr<TabParent> mTabParent;
};

NS_IMPL_ISUPPORTS(RemoteWindowContext, nsIRemoteWindowContext, nsIInterfaceRequestor)

RemoteWindowContext::~RemoteWindowContext()
{
}

NS_IMETHODIMP
RemoteWindowContext::GetInterface(const nsIID& aIID, void** aSink)
{
  return QueryInterface(aIID, aSink);
}

NS_IMETHODIMP
RemoteWindowContext::OpenURI(nsIURI* aURI)
{
  mTabParent->LoadURL(aURI);
  return NS_OK;
}

} // namespace

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
  // GeckoChildProcessHost dtor.
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

void
ContentParent::ShutDownProcess(ShutDownMethod aMethod)
{
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

  using mozilla::dom::quota::QuotaManagerService;

  if (QuotaManagerService* quotaManagerService = QuotaManagerService::Get()) {
    quotaManagerService->AbortOperationsForProcess(mChildID);
  }

  // If Close() fails with an error, we'll end up back in this function, but
  // with aMethod = CLOSE_CHANNEL_WITH_ERROR.

  if (aMethod == CLOSE_CHANNEL && !mCalledClose) {
    // Close() can only be called once: It kicks off the destruction
    // sequence.
    mCalledClose = true;
    Close();
  }

  const ManagedContainer<POfflineCacheUpdateParent>& ocuParents =
    ManagedPOfflineCacheUpdateParent();
  for (auto iter = ocuParents.ConstIter(); !iter.Done(); iter.Next()) {
    RefPtr<mozilla::docshell::OfflineCacheUpdateParent> ocuParent =
      static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(iter.Get()->GetKey());
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

mozilla::ipc::IPCResult
ContentParent::RecvFinishShutdown()
{
  // At this point, we already called ShutDownProcess once with
  // SEND_SHUTDOWN_MESSAGE. To actually close the channel, we call
  // ShutDownProcess again with CLOSE_CHANNEL.
  MOZ_ASSERT(mShutdownPending);
  ShutDownProcess(CLOSE_CHANNEL);
  return IPC_OK();
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
  if (sBrowserContentParents) {
    sBrowserContentParents->RemoveElement(this);
    if (!sBrowserContentParents->Length()) {
      delete sBrowserContentParents;
      sBrowserContentParents = nullptr;
    }
  }

  if (sLargeAllocationContentParents) {
    sLargeAllocationContentParents->RemoveElement(this);
    if (!sLargeAllocationContentParents->Length()) {
      delete sLargeAllocationContentParents;
      sLargeAllocationContentParents = nullptr;
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
  RefPtr<ContentParent> content(this);
  PContentParent::OnChannelError();
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

/* static */
bool
ContentParent::AllocateLayerTreeId(TabParent* aTabParent, uint64_t* aId)
{
  return AllocateLayerTreeId(aTabParent->Manager()->AsContentParent(),
                             aTabParent, aTabParent->GetTabId(), aId);
}

/* static */
bool
ContentParent::AllocateLayerTreeId(ContentParent* aContent,
                                   TabParent* aTopLevel, const TabId& aTabId,
                                   uint64_t* aId)
{
  GPUProcessManager* gpu = GPUProcessManager::Get();

  *aId = gpu->AllocateLayerTreeId();

  if (!aContent || !aTopLevel) {
    return false;
  }

  gpu->MapLayerTreeId(*aId, aContent->OtherPid());

  if (!gfxPlatform::AsyncPanZoomEnabled()) {
    return true;
  }

  return aContent->SendNotifyLayerAllocated(aTabId, *aId);
}

mozilla::ipc::IPCResult
ContentParent::RecvAllocateLayerTreeId(const ContentParentId& aCpId,
                                       const TabId& aTabId, uint64_t* aId)
{
  // Protect against spoofing by a compromised child. aCpId must either
  // correspond to the process that this ContentParent represents or be a
  // child of it.
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  if (ChildID() != aCpId) {
    ContentParentId parent;
    if (!cpm->GetParentProcessId(aCpId, &parent) ||
        ChildID() != parent) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  // GetTopLevelTabParentByProcessAndTabId will make sure that aTabId
  // lives in the process for aCpId.
  RefPtr<ContentParent> contentParent = cpm->GetContentProcessById(aCpId);
  RefPtr<TabParent> browserParent =
    cpm->GetTopLevelTabParentByProcessAndTabId(aCpId, aTabId);
  MOZ_ASSERT(contentParent && browserParent);

  if (!AllocateLayerTreeId(contentParent, browserParent, aTabId, aId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvDeallocateLayerTreeId(const uint64_t& aId)
{
  GPUProcessManager* gpu = GPUProcessManager::Get();

  if (!gpu->IsLayerTreeIdMapped(aId, OtherPid()))
  {
    // You can't deallocate layer tree ids that you didn't allocate
    KillHard("DeallocateLayerTreeId");
  }

  gpu->UnmapLayerTreeId(aId, OtherPid());

  return IPC_OK();
}

namespace {

void
DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess)
{
  RefPtr<DeleteTask<GeckoChildProcessHost>> task = new DeleteTask<GeckoChildProcessHost>(aSubprocess);
  XRE_GetIOMessageLoop()->PostTask(task.forget());
}

// This runnable only exists to delegate ownership of the
// ContentParent to this runnable, until it's deleted by the event
// system.
struct DelayedDeleteContentParentTask : public Runnable
{
  explicit DelayedDeleteContentParentTask(ContentParent* aObj) : mObj(aObj) { }

  // No-op
  NS_IMETHOD Run() override { return NS_OK; }

  RefPtr<ContentParent> mObj;
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

  RefPtr<ContentParent> kungFuDeathGrip(this);
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
  gfxVars::RemoveReceiver(this);

  if (GPUProcessManager* gpu = GPUProcessManager::Get()) {
    // Note: the manager could have shutdown already.
    gpu->RemoveListener(this);
  }

  RecvRemoveGeolocationListener();

  mConsoleService = nullptr;

#ifdef MOZ_ENABLE_PROFILER_SPS
  if (mGatherer && !mProfile.IsEmpty()) {
    mGatherer->OOPExitProfile(mProfile);
  }
#endif

  if (obs) {
    RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

    props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), mChildID);

    if (AbnormalShutdown == why) {
      Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT,
                            NS_LITERAL_CSTRING("content"), 1);

      props->SetPropertyAsBool(NS_LITERAL_STRING("abnormal"), true);

#ifdef MOZ_CRASHREPORTER
      // There's a window in which child processes can crash
      // after IPC is established, but before a crash reporter
      // is created.
      if (PCrashReporterParent* p = LoneManagedOrNullAsserts(ManagedPCrashReporterParent())) {
        CrashReporterParent* crashReporter =
          static_cast<CrashReporterParent*>(p);

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
    nsAutoString cpId;
    cpId.AppendInt(static_cast<uint64_t>(this->ChildID()));
    obs->NotifyObservers((nsIPropertyBag2*) props, "ipc:content-shutdown", cpId.get());
  }

  // Remove any and all idle listeners.
  nsCOMPtr<nsIIdleService> idleService =
    do_GetService("@mozilla.org/widget/idleservice;1");
  MOZ_ASSERT(idleService);
  RefPtr<ParentIdleListener> listener;
  for (int32_t i = mIdleListeners.Length() - 1; i >= 0; --i) {
    listener = static_cast<ParentIdleListener*>(mIdleListeners[i].get());
    idleService->RemoveIdleObserver(listener, listener->mTime);
  }
  mIdleListeners.Clear();

  MessageLoop::current()->
    PostTask(NewRunnableFunction(DelayedDeleteSubprocess, mSubprocess));
  mSubprocess = nullptr;

  // IPDL rules require actors to live on past ActorDestroy, but it
  // may be that the kungFuDeathGrip above is the last reference to
  // |this|.  If so, when we go out of scope here, we're deleted and
  // all hell breaks loose.
  //
  // This runnable ensures that a reference to |this| lives on at
  // least until after the current task finishes running.
  NS_DispatchToCurrentThread(new DelayedDeleteContentParentTask(this));

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  nsTArray<ContentParentId> childIDArray =
    cpm->GetAllChildProcessById(this->ChildID());

  // Destroy any processes created by this ContentParent
  for(uint32_t i = 0; i < childIDArray.Length(); i++) {
    ContentParent* cp = cpm->GetContentProcessById(childIDArray[i]);
    MessageLoop::current()->PostTask(NewRunnableMethod
                                     <ShutDownMethod>(cp,
                                                      &ContentParent::ShutDownProcess,
                                                      SEND_SHUTDOWN_MESSAGE));
  }
  cpm->RemoveContentProcess(this->ChildID());

  if (mDriverCrashGuard) {
    mDriverCrashGuard->NotifyCrashed();
  }

  // Unregister all the BlobURLs registered by the ContentChild.
  for (uint32_t i = 0; i < mBlobURLs.Length(); ++i) {
    nsHostObjectProtocolHandler::RemoveDataEntry(mBlobURLs[i]);
  }

  mBlobURLs.Clear();

#if defined(XP_WIN32) && defined(ACCESSIBILITY)
  a11y::AccessibleWrap::ReleaseContentProcessIdFor(ChildID());
#endif
}

void
ContentParent::NotifyTabDestroying(const TabId& aTabId,
                                   const ContentParentId& aCpId)
{
  if (XRE_IsParentProcess()) {
    // There can be more than one PBrowser for a given app process
    // because of popup windows.  PBrowsers can also destroy
    // concurrently.  When all the PBrowsers are destroying, kick off
    // another task to ensure the child process *really* shuts down,
    // even if the PBrowsers themselves never finish destroying.
    ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
    ContentParent* cp = cpm->GetContentProcessById(aCpId);
    if (!cp) {
        return;
    }
    ++cp->mNumDestroyingTabs;
    nsTArray<TabId> tabIds = cpm->GetTabParentsByProcessId(aCpId);
    if (static_cast<size_t>(cp->mNumDestroyingTabs) != tabIds.Length()) {
        return;
    }

    uint32_t numberOfParents = sBrowserContentParents ? sBrowserContentParents->Length() : 0;
    int32_t processesToKeepAlive = Preferences::GetInt("dom.ipc.keepProcessesAlive", 0);
    if (!cp->mLargeAllocationProcess && static_cast<int32_t>(numberOfParents) <= processesToKeepAlive) {
      return;
    }

    // We're dying now, so prevent this content process from being
    // recycled during its shutdown procedure.
    cp->MarkAsDead();
    cp->StartForceKillTimer();
  } else {
    ContentChild::GetSingleton()->SendNotifyTabDestroying(aTabId, aCpId);
  }
}

void
ContentParent::StartForceKillTimer()
{
  if (mForceKillTimer || !mIPCOpen) {
    return;
  }

  int32_t timeoutSecs = Preferences::GetInt("dom.ipc.tabs.shutdownTimeoutSecs", 5);
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
ContentParent::NotifyTabDestroyed(const TabId& aTabId,
                                  bool aNotifiedDestroying)
{
  if (aNotifiedDestroying) {
    --mNumDestroyingTabs;
  }

  nsTArray<PContentPermissionRequestParent*> parentArray =
    nsContentPermissionUtils::GetContentPermissionRequestParentById(aTabId);

  // Need to close undeleted ContentPermissionRequestParents before tab is closed.
  for (auto& permissionRequestParent : parentArray) {
    Unused << PContentPermissionRequestParent::Send__delete__(permissionRequestParent);
  }

  // There can be more than one PBrowser for a given app process
  // because of popup windows.  When the last one closes, shut
  // us down.
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  nsTArray<TabId> tabIds = cpm->GetTabParentsByProcessId(this->ChildID());

  // We might want to keep alive some content processes for testing, because of performance
  // reasons, but we don't want to alter behavior if the pref is not set.
  uint32_t numberOfParents = sBrowserContentParents ? sBrowserContentParents->Length() : 0;
  int32_t processesToKeepAlive = Preferences::GetInt("dom.ipc.keepProcessesAlive", 0);
  bool shouldKeepAliveAny = !mLargeAllocationProcess && processesToKeepAlive > 0;
  bool shouldKeepAliveThis = shouldKeepAliveAny && static_cast<int32_t>(numberOfParents) <= processesToKeepAlive;

  if (tabIds.Length() == 1 && !shouldKeepAliveThis) {
    // In the case of normal shutdown, send a shutdown message to child to
    // allow it to perform shutdown tasks.
    MessageLoop::current()->PostTask(NewRunnableMethod
                                     <ShutDownMethod>(this,
                                                      &ContentParent::ShutDownProcess,
                                                      SEND_SHUTDOWN_MESSAGE));
  }
}

jsipc::CPOWManager*
ContentParent::GetCPOWManager()
{
  if (PJavaScriptParent* p = LoneManagedOrNullAsserts(ManagedPJavaScriptParent())) {
    return CPOWManagerFor(p);
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
  PTestShellParent* p = LoneManagedOrNullAsserts(ManagedPTestShellParent());
  return static_cast<TestShellParent*>(p);
}

void
ContentParent::InitializeMembers()
{
  mSubprocess = nullptr;
  mChildID = gContentChildID++;
  mGeolocationWatchID = -1;
  mNumDestroyingTabs = 0;
  mIsAlive = true;
  mSendPermissionUpdates = false;
  mCalledClose = false;
  mCalledKillHard = false;
  mCreatedPairedMinidumps = false;
  mShutdownPending = false;
  mIPCOpen = true;
  mHangMonitorActor = nullptr;
}

bool
ContentParent::LaunchSubprocess(ProcessPriority aInitialPriority /* = PROCESS_PRIORITY_FOREGROUND */)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);

  std::vector<std::string> extraArgs;
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

ContentParent::ContentParent(ContentParent* aOpener,
                             bool aIsForBrowser)
  : nsIContentParent()
  , mOpener(aOpener)
  , mIsForBrowser(aIsForBrowser)
  , mLargeAllocationProcess(false)
{
  InitializeMembers();  // Perform common initialization.

  // Insert ourselves into the global linked list of ContentParent objects.
  if (!sContentParents) {
    sContentParents = new LinkedList<ContentParent>();
  }
  sContentParents->insertBack(this);

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
  ChildPrivileges privs = base::PRIVILEGES_DEFAULT;
  mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, privs);
}

ContentParent::~ContentParent()
{
  if (mForceKillTimer) {
    mForceKillTimer->Cancel();
  }

  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // We should be removed from all these lists in ActorDestroy.
  MOZ_ASSERT(!sPrivateContent || !sPrivateContent->Contains(this));
  MOZ_ASSERT((!sBrowserContentParents ||
              !sBrowserContentParents->Contains(this)) &&
             (!sLargeAllocationContentParents ||
              !sLargeAllocationContentParents->Contains(this)));
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

  if (gAppData) {
    nsCString version(gAppData->version);
    nsCString buildID(gAppData->buildID);
    nsCString name(gAppData->name);
    nsCString UAName(gAppData->UAName);
    nsCString ID(gAppData->ID);
    nsCString vendor(gAppData->vendor);

    // Sending all information to content process.
    Unused << SendAppInfo(version, buildID, name, UAName, ID, vendor);
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

  if (aSetupOffMainThreadCompositing) {
    // NB: internally, this will send an IPC message to the child
    // process to get it to create the CompositorBridgeChild.  This
    // message goes through the regular IPC queue for this
    // channel, so delivery will happen-before any other messages
    // we send.  The CompositorBridgeChild must be created before any
    // PBrowsers are created, because they rely on the Compositor
    // already being around.  (Creation is async, so can't happen
    // on demand.)
    bool useOffMainThreadCompositing = !!CompositorThreadHolder::Loop();
    if (useOffMainThreadCompositing) {
      GPUProcessManager* gpm = GPUProcessManager::Get();

      Endpoint<PCompositorBridgeChild> compositor;
      Endpoint<PImageBridgeChild> imageBridge;
      Endpoint<PVRManagerChild> vrBridge;
      Endpoint<PVideoDecoderManagerChild> videoManager;

      DebugOnly<bool> opened = gpm->CreateContentBridges(
        OtherPid(),
        &compositor,
        &imageBridge,
        &vrBridge,
        &videoManager);
      MOZ_ASSERT(opened);

      Unused << SendInitRendering(
        Move(compositor),
        Move(imageBridge),
        Move(vrBridge),
        Move(videoManager));

      gpm->AddListener(this);
    }
  }

  nsStyleSheetService *sheetService = nsStyleSheetService::GetInstance();
  if (sheetService) {
    // This looks like a lot of work, but in a normal browser session we just
    // send two loads.

    for (StyleSheet* sheet : *sheetService->AgentStyleSheets()) {
      URIParams uri;
      SerializeURI(sheet->GetSheetURI(), uri);
      Unused << SendLoadAndRegisterSheet(uri, nsIStyleSheetService::AGENT_SHEET);
    }

    for (StyleSheet* sheet : *sheetService->UserStyleSheets()) {
      URIParams uri;
      SerializeURI(sheet->GetSheetURI(), uri);
      Unused << SendLoadAndRegisterSheet(uri, nsIStyleSheetService::USER_SHEET);
    }

    for (StyleSheet* sheet : *sheetService->AuthorStyleSheets()) {
      URIParams uri;
      SerializeURI(sheet->GetSheetURI(), uri);
      Unused << SendLoadAndRegisterSheet(uri, nsIStyleSheetService::AUTHOR_SHEET);
    }
  }

#ifdef MOZ_CONTENT_SANDBOX
  bool shouldSandbox = true;
  MaybeFileDesc brokerFd = void_t();
#ifdef XP_LINUX
  // XXX: Checking the pref here makes it possible to enable/disable sandboxing
  // during an active session. Currently the pref is only used for testing
  // purpose. If the decision is made to permanently rely on the pref, this
  // should be changed so that it is required to restart firefox for the change
  // of value to take effect.
  shouldSandbox = (Preferences::GetInt("security.sandbox.content.level") > 0) &&
    !PR_GetEnv("MOZ_DISABLE_CONTENT_SANDBOX");

  if (shouldSandbox) {
    MOZ_ASSERT(!mSandboxBroker);
    UniquePtr<SandboxBroker::Policy> policy =
      sSandboxBrokerPolicyFactory->GetContentPolicy(Pid());
    if (policy) {
      brokerFd = FileDescriptor();
      mSandboxBroker = SandboxBroker::Create(Move(policy), Pid(), brokerFd);
      if (!mSandboxBroker) {
        KillHard("SandboxBroker::Create failed");
        return;
      }
      MOZ_ASSERT(static_cast<const FileDescriptor&>(brokerFd).IsValid());
    }
  }
#endif
  if (shouldSandbox && !SendSetProcessSandbox(brokerFd)) {
    KillHard("SandboxInitFailed");
  }
#endif
#if defined(XP_WIN)
  // Send the info needed to join the browser process's audio session.
  nsID id;
  nsString sessionName;
  nsString iconPath;
  if (NS_SUCCEEDED(mozilla::widget::GetAudioSessionData(id, sessionName,
                                                        iconPath))) {
    Unused << SendSetAudioSessionData(id, sessionName, iconPath);
  }
#endif

  {
    RefPtr<ServiceWorkerRegistrar> swr = ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(swr);

    nsTArray<ServiceWorkerRegistrationData> registrations;
    swr->GetRegistrations(registrations);
    Unused << SendInitServiceWorkers(ServiceWorkerConfiguration(registrations));
  }

  {
    nsTArray<BlobURLRegistrationData> registrations;
    if (nsHostObjectProtocolHandler::GetAllBlobURLEntries(registrations,
                                                          this)) {
      Unused << SendInitBlobURLs(registrations);
    }
  }
}

bool
ContentParent::IsAlive() const
{
  return mIsAlive;
}

int32_t
ContentParent::Pid() const
{
  if (!mSubprocess || !mSubprocess->GetChildProcessHandle()) {
    return -1;
  }
  return base::GetProcId(mSubprocess->GetChildProcessHandle());
}

mozilla::ipc::IPCResult
ContentParent::RecvReadPrefsArray(InfallibleTArray<PrefSetting>* aPrefs)
{
  Preferences::GetPreferences(aPrefs);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetGfxVars(InfallibleTArray<GfxVarUpdate>* aVars)
{
  // Ensure gfxVars is initialized (for xpcshell tests).
  gfxVars::Initialize();

  *aVars = gfxVars::FetchNonDefaultVars();

  // Now that content has initialized gfxVars, we can start listening for
  // updates.
  gfxVars::AddReceiver(this);
  return IPC_OK();
}

void
ContentParent::OnCompositorUnexpectedShutdown()
{
  GPUProcessManager* gpm = GPUProcessManager::Get();

  Endpoint<PCompositorBridgeChild> compositor;
  Endpoint<PImageBridgeChild> imageBridge;
  Endpoint<PVRManagerChild> vrBridge;
  Endpoint<PVideoDecoderManagerChild> videoManager;

  DebugOnly<bool> opened = gpm->CreateContentBridges(
    OtherPid(),
    &compositor,
    &imageBridge,
    &vrBridge,
    &videoManager);
  MOZ_ASSERT(opened);

  Unused << SendReinitRendering(
    Move(compositor),
    Move(imageBridge),
    Move(vrBridge),
    Move(videoManager));
}

void
ContentParent::OnVarChanged(const GfxVarUpdate& aVar)
{
  if (!mIPCOpen) {
    return;
  }
  Unused << SendVarUpdate(aVar);
}

mozilla::ipc::IPCResult
ContentParent::RecvReadFontList(InfallibleTArray<FontListEntry>* retValue)
{
#ifdef ANDROID
  gfxAndroidPlatform::GetPlatform()->GetSystemFontList(retValue);
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvReadDataStorageArray(const nsString& aFilename,
                                        InfallibleTArray<DataStorageItem>* aValues)
{
  // Ensure the SSS is initialized before we try to use its storage.
  nsCOMPtr<nsISiteSecurityService> sss = do_GetService("@mozilla.org/ssservice;1");

  RefPtr<DataStorage> storage = DataStorage::Get(aFilename);
  storage->GetAll(aValues);
  return IPC_OK();
}

mozilla::ipc::IPCResult
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

    nsCOMPtr<nsIPrincipal> principal;
    perm->GetPrincipal(getter_AddRefs(principal));
    nsCString origin;
    if (principal) {
      principal->GetOrigin(origin);
    }
    nsCString type;
    perm->GetType(type);
    uint32_t capability;
    perm->GetCapability(&capability);
    uint32_t expireType;
    perm->GetExpireType(&expireType);
    int64_t expireTime;
    perm->GetExpireTime(&expireTime);

    aPermissions->AppendElement(IPC::Permission(origin, type,
                                                capability, expireType,
                                                expireTime));
  }

  // Ask for future changes
  mSendPermissionUpdates = true;
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvSetClipboard(const IPCDataTransfer& aDataTransfer,
                                const bool& aIsPrivateData,
                                const IPC::Principal& aRequestingPrincipal,
                                const int32_t& aWhichClipboard)
{
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsITransferable> trans =
    do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);

  rv = nsContentUtils::IPCTransferableToTransferable(aDataTransfer,
                                                     aIsPrivateData,
                                                     aRequestingPrincipal,
                                                     trans, this, nullptr);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  clipboard->SetData(trans, nullptr, aWhichClipboard);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetClipboard(nsTArray<nsCString>&& aTypes,
                                const int32_t& aWhichClipboard,
                                IPCDataTransfer* aDataTransfer)
{
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);

  for (uint32_t t = 0; t < aTypes.Length(); t++) {
    trans->AddDataFlavor(aTypes[t].get());
  }

  clipboard->GetData(trans, aWhichClipboard);
  nsContentUtils::TransferableToIPCTransferable(trans, aDataTransfer,
                                                true, nullptr, this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvEmptyClipboard(const int32_t& aWhichClipboard)
{
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  clipboard->EmptyClipboard(aWhichClipboard);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvClipboardHasType(nsTArray<nsCString>&& aTypes,
                                    const int32_t& aWhichClipboard,
                                    bool* aHasType)
{
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  const char** typesChrs = new const char *[aTypes.Length()];
  for (uint32_t t = 0; t < aTypes.Length(); t++) {
    typesChrs[t] = aTypes[t].get();
  }

  clipboard->HasDataMatchingFlavors(typesChrs, aTypes.Length(),
                                    aWhichClipboard, aHasType);

  delete [] typesChrs;
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetSystemColors(const uint32_t& colorsCount,
                                   InfallibleTArray<uint32_t>* colors)
{
#ifdef MOZ_WIDGET_ANDROID
  NS_ASSERTION(AndroidBridge::Bridge() != nullptr, "AndroidBridge is not available");
  if (AndroidBridge::Bridge() == nullptr) {
    // Do not fail - the colors won't be right, but it's not critical
    return IPC_OK();
  }

  colors->AppendElements(colorsCount);

  // The array elements correspond to the members of AndroidSystemColors structure,
  // so just pass the pointer to the elements buffer
  AndroidBridge::Bridge()->GetSystemColors((AndroidSystemColors*)colors->Elements());
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetIconForExtension(const nsCString& aFileExt,
                                       const uint32_t& aIconSize,
                                       InfallibleTArray<uint8_t>* bits)
{
#ifdef MOZ_WIDGET_ANDROID
  NS_ASSERTION(AndroidBridge::Bridge() != nullptr, "AndroidBridge is not available");
  if (AndroidBridge::Bridge() == nullptr) {
    // Do not fail - just no icon will be shown
    return IPC_OK();
  }

  bits->AppendElements(aIconSize * aIconSize * 4);

  AndroidBridge::Bridge()->GetIconForExtension(aFileExt, aIconSize, bits->Elements());
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetShowPasswordSetting(bool* showPassword)
{
  // default behavior is to show the last password character
  *showPassword = true;
#ifdef MOZ_WIDGET_ANDROID
  NS_ASSERTION(AndroidBridge::Bridge() != nullptr, "AndroidBridge is not available");

  *showPassword = java::GeckoAppShell::GetShowPasswordSetting();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvAudioChannelChangeDefVolChannel(const int32_t& aChannel,
                                                   const bool& aHidden)
{
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  MOZ_ASSERT(service);
  service->SetDefaultVolumeControlChannelInternal(aChannel, aHidden, mChildID);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvAudioChannelServiceStatus(
                                           const bool& aTelephonyChannel,
                                           const bool& aContentOrNormalChannel,
                                           const bool& aAnyChannel)
{
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  MOZ_ASSERT(service);

  service->ChildStatusReceived(mChildID, aTelephonyChannel,
                               aContentOrNormalChannel, aAnyChannel);
  return IPC_OK();
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

#ifdef MOZ_ENABLE_PROFILER_SPS
  // Need to do this before the mIsAlive check to avoid missing profiles.
  if (!strcmp(aTopic, "profiler-subprocess-gather")) {
    if (mGatherer) {
      mGatherer->WillGatherOOPProfile();
      if (mIsAlive && mSubprocess) {
        Unused << SendGatherProfile();
      }
    }
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

  if (!mIsAlive || !mSubprocess)
    return NS_OK;

  // listening for memory pressure event
  if (!strcmp(aTopic, "memory-pressure") &&
      !StringEndsWith(nsDependentString(aData),
                      NS_LITERAL_STRING("-no-forward"))) {
      Unused << SendFlushMemory(nsDependentString(aData));
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
    if (!SendSetOffline(!strcmp(offline, "true") ? true : false)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
  else if (!strcmp(aTopic, NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC)) {
    if (!SendSetConnectivity(NS_LITERAL_STRING("true").Equals(aData))) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  } else if (!strcmp(aTopic, NS_IPC_CAPTIVE_PORTAL_SET_STATE)) {
    nsCOMPtr<nsICaptivePortalService> cps = do_QueryInterface(aSubject);
    MOZ_ASSERT(cps, "Should QI to a captive portal service");
    if (!cps) {
      return NS_ERROR_FAILURE;
    }
    int32_t state;
    cps->GetState(&state);
    if (!SendSetCaptivePortalState(state)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
  // listening for alert notifications
  else if (!strcmp(aTopic, "alertfinished") ||
       !strcmp(aTopic, "alertclickcallback") ||
       !strcmp(aTopic, "alertshow") ||
       !strcmp(aTopic, "alertdisablecallback") ||
       !strcmp(aTopic, "alertsettingscallback")) {
    if (!SendNotifyAlertsObserver(nsDependentCString(aTopic),
                                  nsDependentString(aData)))
      return NS_ERROR_NOT_AVAILABLE;
  }
  else if (!strcmp(aTopic, "child-gc-request")){
    Unused << SendGarbageCollect();
  }
  else if (!strcmp(aTopic, "child-cc-request")){
    Unused << SendCycleCollect();
  }
  else if (!strcmp(aTopic, "child-mmu-request")){
    Unused << SendMinimizeMemoryUsage();
  }
  else if (!strcmp(aTopic, "last-pb-context-exited")) {
    Unused << SendLastPrivateDocShellDestroyed();
  }
  else if (!strcmp(aTopic, "file-watcher-update")) {
    nsCString creason;
    CopyUTF16toUTF8(aData, creason);
    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    Unused << SendFilePathUpdate(file->mStorageType, file->mStorageName, file->mPath, creason);
  }
#ifdef ACCESSIBILITY
  else if (aData && !strcmp(aTopic, "a11y-init-or-shutdown")) {
    if (*aData == '1') {
      // Make sure accessibility is running in content process when
      // accessibility gets initiated in chrome process.
#if defined(XP_WIN)
      if (IsVistaOrLater()) {
        Unused <<
          SendActivateA11y(a11y::AccessibleWrap::GetContentProcessIdFor(ChildID()));
      }
#else
      Unused << SendActivateA11y(0);
#endif
    } else {
      // If possible, shut down accessibility in content process when
      // accessibility gets shutdown in chrome process.
      Unused << SendShutdownA11y();
    }
  }
#endif
#ifdef MOZ_ENABLE_PROFILER_SPS
  else if (!strcmp(aTopic, "profiler-started")) {
    nsCOMPtr<nsIProfilerStartParams> params(do_QueryInterface(aSubject));
    StartProfiler(params);
  }
  else if (!strcmp(aTopic, "profiler-stopped")) {
    mGatherer = nullptr;
    Unused << SendStopProfiler();
  }
  else if (!strcmp(aTopic, "profiler-paused")) {
    Unused << SendPauseProfiler(true);
  }
  else if (!strcmp(aTopic, "profiler-resumed")) {
    Unused << SendPauseProfiler(false);
  }
#endif
  else if (!strcmp(aTopic, "cacheservice:empty-cache")) {
    Unused << SendNotifyEmptyHTTPCache();
  }
  return NS_OK;
}

PGMPServiceParent*
ContentParent::AllocPGMPServiceParent(mozilla::ipc::Transport* aTransport,
                                      base::ProcessId aOtherProcess)
{
  return GMPServiceParent::Create(aTransport, aOtherProcess);
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

mozilla::ipc::IPCResult
ContentParent::RecvGetProcessAttributes(ContentParentId* aCpId,
                                        bool* aIsForBrowser)
{
  *aCpId = mChildID;
  *aIsForBrowser = mIsForBrowser;

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetXPCOMProcessAttributes(bool* aIsOffline,
                                             bool* aIsConnected,
                                             int32_t* aCaptivePortalState,
                                             bool* aIsLangRTL,
                                             bool* aHaveBidiKeyboards,
                                             InfallibleTArray<nsString>* dictionaries,
                                             ClipboardCapabilities* clipboardCaps,
                                             DomainPolicyClone* domainPolicy,
                                             StructuredCloneData* aInitialData,
                                             InfallibleTArray<FontFamilyListEntry>* fontFamilies,
                                             OptionalURIParams* aUserContentCSSURL,
                                             nsTArray<LookAndFeelInt>* aLookAndFeelIntCache)
{
  nsCOMPtr<nsIIOService> io(do_GetIOService());
  MOZ_ASSERT(io, "No IO service?");
  DebugOnly<nsresult> rv = io->GetOffline(aIsOffline);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed getting offline?");

  rv = io->GetConnectivity(aIsConnected);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed getting connectivity?");

  *aCaptivePortalState = nsICaptivePortalService::UNKNOWN;
  nsCOMPtr<nsICaptivePortalService> cps = do_GetService(NS_CAPTIVEPORTAL_CONTRACTID);
  if (cps) {
    cps->GetState(aCaptivePortalState);
  }

  nsIBidiKeyboard* bidi = nsContentUtils::GetBidiKeyboard();

  *aIsLangRTL = false;
  *aHaveBidiKeyboards = false;
  if (bidi) {
    bidi->IsLangRTL(aIsLangRTL);
    bidi->GetHaveBidiKeyboards(aHaveBidiKeyboards);
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
  NS_ENSURE_TRUE(ssm, IPC_OK());
  ssm->CloneDomainPolicy(domainPolicy);

  if (nsFrameMessageManager* mm = nsFrameMessageManager::sParentProcessManager) {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(xpc::PrivilegedJunkScope()))) {
      return IPC_FAIL_NO_REASON(this);
    }
    JS::RootedValue init(jsapi.cx());
    nsresult result = mm->GetInitialProcessData(jsapi.cx(), &init);
    if (NS_FAILED(result)) {
      return IPC_FAIL_NO_REASON(this);
    }

    ErrorResult rv;
    aInitialData->Write(jsapi.cx(), init, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return IPC_FAIL_NO_REASON(this);
    }
  }

  // This is only implemented (returns a non-empty list) by MacOSX at present.
  gfxPlatform::GetPlatform()->GetSystemFontFamilyList(fontFamilies);
  *aLookAndFeelIntCache = LookAndFeel::GetIntCache();

  // Content processes have no permission to access profile directory, so we
  // send the file URL instead.
  StyleSheet* ucs = nsLayoutStylesheetCache::For(StyleBackendType::Gecko)->UserContentSheet();
  if (ucs) {
    SerializeURI(ucs->GetSheetURI(), *aUserContentCSSURL);
  } else {
    SerializeURI(nullptr, *aUserContentCSSURL);
  }

  return IPC_OK();
}

mozilla::jsipc::PJavaScriptParent *
ContentParent::AllocPJavaScriptParent()
{
  MOZ_ASSERT(ManagedPJavaScriptParent().IsEmpty());
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
                                   const bool& aIsForBrowser)
{
  return nsIContentParent::AllocPBrowserParent(aTabId,
                                               aContext,
                                               aChromeFlags,
                                               aCpId,
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
  RefPtr<DeviceStorageRequestParent> result = new DeviceStorageRequestParent(aParams);
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

mozilla::ipc::IPCResult
ContentParent::RecvPBlobConstructor(PBlobParent* aActor,
                                    const BlobConstructorParams& aParams)
{
  const ParentBlobConstructorParams& params = aParams.get_ParentBlobConstructorParams();
  if (params.blobParams().type() == AnyBlobConstructorParams::TKnownBlobConstructorParams) {
    if (!aActor->SendCreatedFromKnownBlob()) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  return IPC_OK();
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
  // We don't want to time out the content process during XPCShell tests. This
  // is the easiest way to ensure that.
  if (PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR")) {
    return;
  }

  auto self = static_cast<ContentParent*>(aClosure);
  self->KillHard("ShutDownKill");
}

// WARNING: aReason appears in telemetry, so any new value passed in requires
// data review.
void
ContentParent::KillHard(const char* aReason)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);

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
  if (PCrashReporterParent* p = LoneManagedOrNullAsserts(ManagedPCrashReporterParent())) {
    CrashReporterParent* crashReporter =
      static_cast<CrashReporterParent*>(p);
    // GeneratePairedMinidump creates two minidumps for us - the main
    // one is for the content process we're about to kill, and the other
    // one is for the main browser process. That second one is the extra
    // minidump tagging along, so we have to tell the crash reporter that
    // it exists and is being appended.
    nsAutoCString additionalDumps("browser");
    crashReporter->AnnotateCrashReport(
      NS_LITERAL_CSTRING("additional_minidumps"),
      additionalDumps);
    nsDependentCString reason(aReason);
    crashReporter->AnnotateCrashReport(
      NS_LITERAL_CSTRING("ipc_channel_error"),
      reason);

    // Generate the report and insert into the queue for submittal.
    mCreatedPairedMinidumps = crashReporter->GenerateCompleteMinidump(this);

    Telemetry::Accumulate(Telemetry::SUBPROCESS_KILL_HARD, reason, 1);
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
    NewRunnableFunction(&ProcessWatcher::EnsureProcessTerminated,
                        otherProcessHandle, /*force=*/true));
}

void
ContentParent::FriendlyName(nsAString& aName, bool aAnonymize)
{
  aName.Truncate();
  if (mIsForBrowser) {
    aName.AssignLiteral("Browser");
  } else if (aAnonymize) {
    aName.AssignLiteral("<anonymized-name>");
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

mozilla::ipc::IPCResult
ContentParent::RecvPCrashReporterConstructor(PCrashReporterParent* actor,
                                             const NativeThreadId& tid,
                                             const uint32_t& processType)
{
  static_cast<CrashReporterParent*>(actor)->SetChildData(tid, processType);
  return IPC_OK();
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

devtools::PHeapSnapshotTempFileHelperParent*
ContentParent::AllocPHeapSnapshotTempFileHelperParent()
{
  return devtools::HeapSnapshotTempFileHelperParent::Create();
}

bool
ContentParent::DeallocPHeapSnapshotTempFileHelperParent(
  devtools::PHeapSnapshotTempFileHelperParent* aHeapSnapshotHelper)
{
  delete aHeapSnapshotHelper;
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
  MOZ_ASSERT(!mPrintingParent,
             "Only one PrintingParent should be created per process.");

  // Create the printing singleton for this process.
  mPrintingParent = new PrintingParent();
  return mPrintingParent.get();
#else
  MOZ_ASSERT_UNREACHABLE("Should never be created if no printing.");
  return nullptr;
#endif
}

bool
ContentParent::DeallocPPrintingParent(PPrintingParent* printing)
{
#ifdef NS_PRINTING
  MOZ_ASSERT(mPrintingParent == printing,
             "Only one PrintingParent should have been created per process.");

  mPrintingParent = nullptr;
#else
  MOZ_ASSERT_UNREACHABLE("Should never have been created if no printing.");
#endif
  return true;
}

#ifdef NS_PRINTING
already_AddRefed<embedding::PrintingParent>
ContentParent::GetPrintingParent()
{
  MOZ_ASSERT(mPrintingParent);

  RefPtr<embedding::PrintingParent> printingParent = mPrintingParent;
  return printingParent.forget();
}
#endif

PSendStreamParent*
ContentParent::AllocPSendStreamParent()
{
  return nsIContentParent::AllocPSendStreamParent();
}

bool
ContentParent::DeallocPSendStreamParent(PSendStreamParent* aActor)
{
  return nsIContentParent::DeallocPSendStreamParent(aActor);
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
  RefPtr<PSMContentDownloaderParent> downloader =
    new PSMContentDownloaderParent(aCertType);
  return downloader.forget().take();
}

bool
ContentParent::DeallocPPSMContentDownloaderParent(PPSMContentDownloaderParent* aListener)
{
  auto* listener = static_cast<PSMContentDownloaderParent*>(aListener);
  RefPtr<PSMContentDownloaderParent> downloader = dont_AddRef(listener);
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

PHandlerServiceParent*
ContentParent::AllocPHandlerServiceParent()
{
  HandlerServiceParent* actor = new HandlerServiceParent();
  actor->AddRef();
  return actor;
}

bool
ContentParent::DeallocPHandlerServiceParent(PHandlerServiceParent* aHandlerServiceParent)
{
  static_cast<HandlerServiceParent*>(aHandlerServiceParent)->Release();
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

PPresentationParent*
ContentParent::AllocPPresentationParent()
{
  RefPtr<PresentationParent> actor = new PresentationParent();
  return actor.forget().take();
}

bool
ContentParent::DeallocPPresentationParent(PPresentationParent* aActor)
{
  RefPtr<PresentationParent> actor =
  dont_AddRef(static_cast<PresentationParent*>(aActor));
  return true;
}

mozilla::ipc::IPCResult
ContentParent::RecvPPresentationConstructor(PPresentationParent* aActor)
{
  if (!static_cast<PresentationParent*>(aActor)->Init(mChildID)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PFlyWebPublishedServerParent*
ContentParent::AllocPFlyWebPublishedServerParent(const nsString& name,
                                                 const FlyWebPublishOptions& params)
{
  RefPtr<FlyWebPublishedServerParent> actor =
    new FlyWebPublishedServerParent(name, params);
  return actor.forget().take();
}

bool
ContentParent::DeallocPFlyWebPublishedServerParent(PFlyWebPublishedServerParent* aActor)
{
  RefPtr<FlyWebPublishedServerParent> actor =
    dont_AddRef(static_cast<FlyWebPublishedServerParent*>(aActor));
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

mozilla::ipc::IPCResult
ContentParent::RecvPSpeechSynthesisConstructor(PSpeechSynthesisParent* aActor)
{
#ifdef MOZ_WEBSPEECH
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult
ContentParent::RecvStartVisitedQuery(const URIParams& aURI)
{
  nsCOMPtr<nsIURI> newURI = DeserializeURI(aURI);
  if (!newURI) {
  return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<IHistory> history = services::GetHistoryService();
  if (history) {
  history->RegisterVisitedCallback(newURI, nullptr);
  }
  return IPC_OK();
}


mozilla::ipc::IPCResult
ContentParent::RecvVisitURI(const URIParams& uri,
                            const OptionalURIParams& referrer,
                            const uint32_t& flags)
{
  nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
  if (!ourURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIURI> ourReferrer = DeserializeURI(referrer);
  nsCOMPtr<IHistory> history = services::GetHistoryService();
  if (history) {
    history->VisitURI(ourURI, ourReferrer, flags);
  }
  return IPC_OK();
}


mozilla::ipc::IPCResult
ContentParent::RecvSetURITitle(const URIParams& uri,
                               const nsString& title)
{
  nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
  if (!ourURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<IHistory> history = services::GetHistoryService();
  if (history) {
    history->SetURITitle(ourURI, title);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNSSU2FTokenIsCompatibleVersion(const nsString& aVersion,
                                                  bool* aIsCompatible)
{
  MOZ_ASSERT(aIsCompatible);

  nsCOMPtr<nsINSSU2FToken> nssToken(do_GetService(NS_NSSU2FTOKEN_CONTRACTID));
  if (NS_WARN_IF(!nssToken)) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = nssToken->IsCompatibleVersion(aVersion, aIsCompatible);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNSSU2FTokenIsRegistered(nsTArray<uint8_t>&& aKeyHandle,
                                           bool* aIsValidKeyHandle)
{
  MOZ_ASSERT(aIsValidKeyHandle);

  nsCOMPtr<nsINSSU2FToken> nssToken(do_GetService(NS_NSSU2FTOKEN_CONTRACTID));
  if (NS_WARN_IF(!nssToken)) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsresult rv = nssToken->IsRegistered(aKeyHandle.Elements(), aKeyHandle.Length(),
                                       aIsValidKeyHandle);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNSSU2FTokenRegister(nsTArray<uint8_t>&& aApplication,
                                       nsTArray<uint8_t>&& aChallenge,
                                       nsTArray<uint8_t>* aRegistration)
{
  MOZ_ASSERT(aRegistration);

  nsCOMPtr<nsINSSU2FToken> nssToken(do_GetService(NS_NSSU2FTOKEN_CONTRACTID));
  if (NS_WARN_IF(!nssToken)) {
    return IPC_FAIL_NO_REASON(this);
  }
  uint8_t* buffer;
  uint32_t bufferlen;
  nsresult rv = nssToken->Register(aApplication.Elements(), aApplication.Length(),
                                   aChallenge.Elements(), aChallenge.Length(),
                                   &buffer, &bufferlen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(buffer);
  aRegistration->ReplaceElementsAt(0, aRegistration->Length(), buffer, bufferlen);
  free(buffer);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNSSU2FTokenSign(nsTArray<uint8_t>&& aApplication,
                                   nsTArray<uint8_t>&& aChallenge,
                                   nsTArray<uint8_t>&& aKeyHandle,
                                   nsTArray<uint8_t>* aSignature)
{
  MOZ_ASSERT(aSignature);

  nsCOMPtr<nsINSSU2FToken> nssToken(do_GetService(NS_NSSU2FTOKEN_CONTRACTID));
  if (NS_WARN_IF(!nssToken)) {
    return IPC_FAIL_NO_REASON(this);
  }
  uint8_t* buffer;
  uint32_t bufferlen;
  nsresult rv = nssToken->Sign(aApplication.Elements(), aApplication.Length(),
                               aChallenge.Elements(), aChallenge.Length(),
                               aKeyHandle.Elements(), aKeyHandle.Length(),
                               &buffer, &bufferlen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(buffer);
  aSignature->ReplaceElementsAt(0, aSignature->Length(), buffer, bufferlen);
  free(buffer);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvIsSecureURI(const uint32_t& type,
                               const URIParams& uri,
                               const uint32_t& flags,
                               bool* isSecureURI)
{
  nsCOMPtr<nsISiteSecurityService> sss(do_GetService(NS_SSSERVICE_CONTRACTID));
  if (!sss) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
  if (!ourURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsresult rv = sss->IsSecureURI(type, ourURI, flags, nullptr, isSecureURI);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvAccumulateMixedContentHSTS(const URIParams& aURI, const bool& aActive, const bool& aHSTSPriming)
{
  nsCOMPtr<nsIURI> ourURI = DeserializeURI(aURI);
  if (!ourURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsMixedContentBlocker::AccumulateMixedContentHSTS(ourURI, aActive, aHSTSPriming);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvLoadURIExternal(const URIParams& uri,
                                   PBrowserParent* windowContext)
{
  nsCOMPtr<nsIExternalProtocolService> extProtService(do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  if (!extProtService) {
    return IPC_OK();
  }
  nsCOMPtr<nsIURI> ourURI = DeserializeURI(uri);
  if (!ourURI) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<RemoteWindowContext> context =
    new RemoteWindowContext(static_cast<TabParent*>(windowContext));
  extProtService->LoadURI(ourURI, context);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvExtProtocolChannelConnectParent(const uint32_t& registrarId)
{
  nsresult rv;

  // First get the real channel created before redirect on the parent.
  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(registrarId, nullptr, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsIParentChannel> parent = do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  // The channel itself is its own (faked) parent, link it.
  rv = NS_LinkRedirectChannels(registrarId, parent, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  // Signal the parent channel that it's a redirect-to parent.  This will
  // make AsyncOpen on it do nothing (what we want).
  // Yes, this is a bit of a hack, but I don't think it's necessary to invent
  // a new interface just to set this flag on the channel.
  parent->SetParentListener(nullptr);

  return IPC_OK();
}

bool
ContentParent::HasNotificationPermission(const IPC::Principal& aPrincipal)
{
  return true;
}

mozilla::ipc::IPCResult
ContentParent::RecvShowAlert(const AlertNotificationType& aAlert)
{
  nsCOMPtr<nsIAlertNotification> alert(dont_AddRef(aAlert));
  if (NS_WARN_IF(!alert)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = alert->GetPrincipal(getter_AddRefs(principal));
  if (NS_WARN_IF(NS_FAILED(rv)) ||
      !HasNotificationPermission(IPC::Principal(principal))) {

      return IPC_OK();
  }

  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_ALERTSERVICE_CONTRACTID));
  if (sysAlerts) {
      sysAlerts->ShowAlert(alert, this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvCloseAlert(const nsString& aName,
                              const IPC::Principal& aPrincipal)
{
  if (!HasNotificationPermission(aPrincipal)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_ALERTSERVICE_CONTRACTID));
  if (sysAlerts) {
    sysAlerts->CloseAlert(aName, aPrincipal);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvDisableNotifications(const IPC::Principal& aPrincipal)
{
  if (HasNotificationPermission(aPrincipal)) {
    Unused << Notification::RemovePermission(aPrincipal);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvOpenNotificationSettings(const IPC::Principal& aPrincipal)
{
  if (HasNotificationPermission(aPrincipal)) {
    Unused << Notification::OpenSettings(aPrincipal);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvSyncMessage(const nsString& aMsg,
                               const ClonedMessageData& aData,
                               InfallibleTArray<CpowEntry>&& aCpows,
                               const IPC::Principal& aPrincipal,
                               nsTArray<StructuredCloneData>* aRetvals)
{
  return nsIContentParent::RecvSyncMessage(aMsg, aData, Move(aCpows),
                                           aPrincipal, aRetvals);
}

mozilla::ipc::IPCResult
ContentParent::RecvRpcMessage(const nsString& aMsg,
                              const ClonedMessageData& aData,
                              InfallibleTArray<CpowEntry>&& aCpows,
                              const IPC::Principal& aPrincipal,
                              nsTArray<StructuredCloneData>* aRetvals)
{
  return nsIContentParent::RecvRpcMessage(aMsg, aData, Move(aCpows), aPrincipal,
                                          aRetvals);
}

mozilla::ipc::IPCResult
ContentParent::RecvAsyncMessage(const nsString& aMsg,
                                InfallibleTArray<CpowEntry>&& aCpows,
                                const IPC::Principal& aPrincipal,
                                const ClonedMessageData& aData)
{
  return nsIContentParent::RecvAsyncMessage(aMsg, Move(aCpows), aPrincipal,
                                            aData);
}

mozilla::ipc::IPCResult
ContentParent::RecvFilePathUpdateNotify(const nsString& aType,
                                        const nsString& aStorageName,
                                        const nsString& aFilePath,
                                        const nsCString& aReason)
{
  RefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(aType,
                                                        aStorageName,
                                                        aFilePath);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return IPC_FAIL_NO_REASON(this);
  }
  obs->NotifyObservers(dsf, "file-watcher-update",
                       NS_ConvertASCIItoUTF16(aReason).get());
  return IPC_OK();
}

static int32_t
AddGeolocationListener(nsIDOMGeoPositionCallback* watcher,
                       nsIDOMGeoPositionErrorCallback* errorCallBack,
                       bool highAccuracy)
{
  nsCOMPtr<nsIDOMGeoGeolocation> geo = do_GetService("@mozilla.org/geolocation;1");
  if (!geo) {
    return -1;
  }

  UniquePtr<PositionOptions> options = MakeUnique<PositionOptions>();
  options->mTimeout = 0;
  options->mMaximumAge = 0;
  options->mEnableHighAccuracy = highAccuracy;
  int32_t retval = 1;
  geo->WatchPosition(watcher, errorCallBack, Move(options), &retval);
  return retval;
}

mozilla::ipc::IPCResult
ContentParent::RecvAddGeolocationListener(const IPC::Principal& aPrincipal,
                                          const bool& aHighAccuracy)
{
  // To ensure no geolocation updates are skipped, we always force the
  // creation of a new listener.
  RecvRemoveGeolocationListener();
  mGeolocationWatchID = AddGeolocationListener(this, this, aHighAccuracy);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvRemoveGeolocationListener()
{
  if (mGeolocationWatchID != -1) {
    nsCOMPtr<nsIDOMGeoGeolocation> geo = do_GetService("@mozilla.org/geolocation;1");
    if (!geo) {
      return IPC_OK();
    }
    geo->ClearWatch(mGeolocationWatchID);
    mGeolocationWatchID = -1;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvSetGeolocationHigherAccuracy(const bool& aEnable)
{
  // This should never be called without a listener already present,
  // so this check allows us to forgo securing privileges.
  if (mGeolocationWatchID != -1) {
    RecvRemoveGeolocationListener();
    mGeolocationWatchID = AddGeolocationListener(this, this, aEnable);
  }
  return IPC_OK();
}

NS_IMETHODIMP
ContentParent::HandleEvent(nsIDOMGeoPosition* postion)
{
  Unused << SendGeolocationUpdate(GeoPosition(postion));
  return NS_OK;
}

NS_IMETHODIMP
ContentParent::HandleEvent(nsIDOMGeoPositionError* postionError)
{
  int16_t errorCode;
  nsresult rv;
  rv = postionError->GetCode(&errorCode);
  NS_ENSURE_SUCCESS(rv,rv);
  Unused << SendGeolocationError(errorCode);
  return NS_OK;
}

nsConsoleService *
ContentParent::GetConsoleService()
{
  if (mConsoleService) {
    return mConsoleService.get();
  }

  // XXXkhuey everything about this is terrible.
  // Get the ConsoleService by CID rather than ContractID, so that we
  // can cast the returned pointer to an nsConsoleService (rather than
  // just an nsIConsoleService). This allows us to call the non-idl function
  // nsConsoleService::LogMessageWithMode.
  NS_DEFINE_CID(consoleServiceCID, NS_CONSOLESERVICE_CID);
  nsCOMPtr<nsIConsoleService> consoleService(do_GetService(consoleServiceCID));
  mConsoleService = static_cast<nsConsoleService*>(consoleService.get());
  return mConsoleService.get();
}

mozilla::ipc::IPCResult
ContentParent::RecvConsoleMessage(const nsString& aMessage)
{
  RefPtr<nsConsoleService> consoleService = GetConsoleService();
  if (!consoleService) {
    return IPC_OK();
  }

  RefPtr<nsConsoleMessage> msg(new nsConsoleMessage(aMessage.get()));
  consoleService->LogMessageWithMode(msg, nsConsoleService::SuppressLog);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvScriptError(const nsString& aMessage,
                               const nsString& aSourceName,
                               const nsString& aSourceLine,
                               const uint32_t& aLineNumber,
                               const uint32_t& aColNumber,
                               const uint32_t& aFlags,
                               const nsCString& aCategory)
{
  RefPtr<nsConsoleService> consoleService = GetConsoleService();
  if (!consoleService) {
    return IPC_OK();
  }

  nsCOMPtr<nsIScriptError> msg(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  nsresult rv = msg->Init(aMessage, aSourceName, aSourceLine,
                          aLineNumber, aColNumber, aFlags, aCategory.get());
  if (NS_FAILED(rv))
    return IPC_OK();

  consoleService->LogMessageWithMode(msg, nsConsoleService::SuppressLog);
  return IPC_OK();
}

mozilla::ipc::IPCResult
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
  return IPC_OK();
}

bool
ContentParent::DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope)
{
  MOZ_ASSERT(!aRunInGlobalScope);
  return SendLoadProcessScript(nsString(aURL));
}

nsresult
ContentParent::DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  StructuredCloneData& aHelper,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForParent(this, aHelper, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  InfallibleTArray<CpowEntry> cpows;
  jsipc::CPOWManager* mgr = GetCPOWManager();
  if (aCpows && (!mgr || !mgr->Wrap(aCx, aCpows, &cpows))) {
    return NS_ERROR_UNEXPECTED;
  }
  if (!SendAsyncMessage(nsString(aMessage), cpows, Principal(aPrincipal), data)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

PBlobParent*
ContentParent::SendPBlobConstructor(PBlobParent* aActor,
                                    const BlobConstructorParams& aParams)
{
  return PContentParent::SendPBlobConstructor(aActor, aParams);
}

PBrowserParent*
ContentParent::SendPBrowserConstructor(PBrowserParent* aActor,
                                       const TabId& aTabId,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const ContentParentId& aCpId,
                                       const bool& aIsForBrowser)
{
  return PContentParent::SendPBrowserConstructor(aActor,
                                                 aTabId,
                                                 aContext,
                                                 aChromeFlags,
                                                 aCpId,
                                                 aIsForBrowser);
}

mozilla::ipc::IPCResult
ContentParent::RecvKeywordToURI(const nsCString& aKeyword,
                                nsString* aProviderName,
                                OptionalInputStreamParams* aPostData,
                                OptionalURIParams* aURI)
{
  *aPostData = void_t();
  *aURI = void_t();

  nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
  if (!fixup) {
    return IPC_OK();
  }

  nsCOMPtr<nsIInputStream> postData;
  nsCOMPtr<nsIURIFixupInfo> info;

  if (NS_FAILED(fixup->KeywordToURI(aKeyword, getter_AddRefs(postData),
                                    getter_AddRefs(info)))) {
    return IPC_OK();
  }
  info->GetKeywordProviderName(*aProviderName);

  nsTArray<mozilla::ipc::FileDescriptor> fds;
  SerializeInputStream(postData, *aPostData, fds);
  MOZ_ASSERT(fds.IsEmpty());

  nsCOMPtr<nsIURI> uri;
  info->GetPreferredURI(getter_AddRefs(uri));
  SerializeURI(uri, *aURI);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyKeywordSearchLoading(const nsString &aProvider,
                                              const nsString &aKeyword)
{
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
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvCopyFavicon(const URIParams& aOldURI,
                               const URIParams& aNewURI,
                               const IPC::Principal& aLoadingPrincipal,
                               const bool& aInPrivateBrowsing)
{
  nsCOMPtr<nsIURI> oldURI = DeserializeURI(aOldURI);
  if (!oldURI) {
    return IPC_OK();
  }
  nsCOMPtr<nsIURI> newURI = DeserializeURI(aNewURI);
  if (!newURI) {
    return IPC_OK();
  }

  nsDocShell::CopyFavicon(oldURI, newURI, aLoadingPrincipal, aInPrivateBrowsing);
  return IPC_OK();
}

bool
ContentParent::ShouldContinueFromReplyTimeout()
{
  RefPtr<ProcessHangMonitor> monitor = ProcessHangMonitor::Get();
  return !monitor || !monitor->ShouldTimeOutCPOWs();
}

mozilla::ipc::IPCResult
ContentParent::RecvRecordingDeviceEvents(const nsString& aRecordingStatus,
                                         const nsString& aPageURL,
                                         const bool& aIsAudio,
                                         const bool& aIsVideo)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    // recording-device-ipc-events needs to gather more information from content process
    RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
    props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), ChildID());
    props->SetPropertyAsBool(NS_LITERAL_STRING("isAudio"), aIsAudio);
    props->SetPropertyAsBool(NS_LITERAL_STRING("isVideo"), aIsVideo);
    props->SetPropertyAsAString(NS_LITERAL_STRING("requestURL"), aPageURL);

    obs->NotifyObservers((nsIPropertyBag2*) props,
                         "recording-device-ipc-events",
                         aRecordingStatus.get());
  } else {
    NS_WARNING("Could not get the Observer service for ContentParent::RecvRecordingDeviceEvents.");
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetGraphicsFeatureStatus(const int32_t& aFeature,
                                            int32_t* aStatus,
                                            nsCString* aFailureId,
                                            bool* aSuccess)
{
  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  if (!gfxInfo) {
    *aSuccess = false;
    return IPC_OK();
  }

  *aSuccess = NS_SUCCEEDED(gfxInfo->GetFeatureStatus(aFeature, *aFailureId, aStatus));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvAddIdleObserver(const uint64_t& aObserver,
                                   const uint32_t& aIdleTimeInS)
{
  nsresult rv;
  nsCOMPtr<nsIIdleService> idleService =
    do_GetService("@mozilla.org/widget/idleservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  RefPtr<ParentIdleListener> listener =
    new ParentIdleListener(this, aObserver, aIdleTimeInS);
  rv = idleService->AddIdleObserver(listener, aIdleTimeInS);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));
  mIdleListeners.AppendElement(listener);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvRemoveIdleObserver(const uint64_t& aObserver,
                                      const uint32_t& aIdleTimeInS)
{
  RefPtr<ParentIdleListener> listener;
  for (int32_t i = mIdleListeners.Length() - 1; i >= 0; --i) {
    listener = static_cast<ParentIdleListener*>(mIdleListeners[i].get());
    if (listener->mObserver == aObserver &&
      listener->mTime == aIdleTimeInS) {
      nsresult rv;
      nsCOMPtr<nsIIdleService> idleService =
        do_GetService("@mozilla.org/widget/idleservice;1", &rv);
      NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));
      idleService->RemoveIdleObserver(listener, aIdleTimeInS);
      mIdleListeners.RemoveElementAt(i);
      break;
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvBackUpXResources(const FileDescriptor& aXSocketFd)
{
#ifndef MOZ_X11
  NS_RUNTIMEABORT("This message only makes sense on X11 platforms");
#else
  MOZ_ASSERT(0 > mChildXSocketFdDup.get(),
             "Already backed up X resources??");
  if (aXSocketFd.IsValid()) {
    auto rawFD = aXSocketFd.ClonePlatformHandle();
    mChildXSocketFdDup.reset(rawFD.release());
  }
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvOpenAnonymousTemporaryFile(FileDescOrError *aFD)
{
  PRFileDesc *prfd;
  nsresult rv = NS_OpenAnonymousTemporaryFile(&prfd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Returning false will kill the child process; instead
    // propagate the error and let the child handle it.
    *aFD = rv;
    return IPC_OK();
  }
  *aFD = FileDescriptor(FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(prfd)));
  // The FileDescriptor object owns a duplicate of the file handle; we
  // must close the original (and clean up the NSPR descriptor).
  PR_Close(prfd);
  return IPC_OK();
}

static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID);

mozilla::ipc::IPCResult
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
    return IPC_OK();
  }

  formProcessor->ProcessValueIPC(oldValue, challenge, keytype, keyparams,
                                 *newValue);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvKeygenProvideContent(nsString* aAttribute,
                                        nsTArray<nsString>* aContent)
{
  nsCOMPtr<nsIFormProcessor> formProcessor =
    do_GetService(kFormProcessorCID);
  if (!formProcessor) {
    return IPC_OK();
  }

  formProcessor->ProvideContent(NS_LITERAL_STRING("SELECT"), *aContent,
                                *aAttribute);
  return IPC_OK();
}

PFileDescriptorSetParent*
ContentParent::AllocPFileDescriptorSetParent(const FileDescriptor& aFD)
{
  return nsIContentParent::AllocPFileDescriptorSetParent(aFD);
}

bool
ContentParent::DeallocPFileDescriptorSetParent(PFileDescriptorSetParent* aActor)
{
  return nsIContentParent::DeallocPFileDescriptorSetParent(aActor);
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
  nsCOMPtr<nsISpellChecker> spellChecker(do_GetService(NS_SPELLCHECKER_CONTRACTID));
  MOZ_ASSERT(spellChecker, "No spell checker?");

  InfallibleTArray<nsString> dictionaries;
  spellChecker->GetDictionaryList(&dictionaries);

  for (auto* cp : AllProcesses(eLive)) {
    Unused << cp->SendUpdateDictionaryList(dictionaries);
  }
}

/*static*/ TabId
ContentParent::AllocateTabId(const TabId& aOpenerTabId,
                             const IPCTabContext& aContext,
                             const ContentParentId& aCpId)
{
  TabId tabId;
  if (XRE_IsParentProcess()) {
    ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
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
                               const ContentParentId& aCpId,
                               bool aMarkedDestroying)
{
  if (XRE_IsParentProcess()) {
    ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
    ContentParent* cp = cpm->GetContentProcessById(aCpId);

    cp->NotifyTabDestroyed(aTabId, aMarkedDestroying);

    ContentProcessManager::GetSingleton()->DeallocateTabId(aCpId, aTabId);
  } else {
    ContentChild::GetSingleton()->SendDeallocateTabId(aTabId, aCpId,
                                                      aMarkedDestroying);
  }
}

mozilla::ipc::IPCResult
ContentParent::RecvAllocateTabId(const TabId& aOpenerTabId,
                                 const IPCTabContext& aContext,
                                 const ContentParentId& aCpId,
                                 TabId* aTabId)
{
  *aTabId = AllocateTabId(aOpenerTabId, aContext, aCpId);
  if (!(*aTabId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvDeallocateTabId(const TabId& aTabId,
                                   const ContentParentId& aCpId,
                                   const bool& aMarkedDestroying)
{
  DeallocateTabId(aTabId, aCpId, aMarkedDestroying);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyTabDestroying(const TabId& aTabId,
                                       const ContentParentId& aCpId)
{
  NotifyTabDestroying(aTabId, aCpId);
  return IPC_OK();
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
                                              const PrincipalInfo& aLoadingPrincipalInfo,
                                              const bool& aStickDocument)
{
  RefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
        new mozilla::docshell::OfflineCacheUpdateParent();
  // Use this reference as the IPDL reference.
  return update.forget().take();
}

mozilla::ipc::IPCResult
ContentParent::RecvPOfflineCacheUpdateConstructor(POfflineCacheUpdateParent* aActor,
                                                  const URIParams& aManifestURI,
                                                  const URIParams& aDocumentURI,
                                                  const PrincipalInfo& aLoadingPrincipal,
                                                  const bool& aStickDocument)
{
  MOZ_ASSERT(aActor);

  RefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
    static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(aActor);

  nsresult rv = update->Schedule(aManifestURI, aDocumentURI, aLoadingPrincipal, aStickDocument);
  if (NS_FAILED(rv) && IsAlive()) {
    // Inform the child of failure.
    Unused << update->SendFinish(false, false);
  }

  return IPC_OK();
}

bool
ContentParent::DeallocPOfflineCacheUpdateParent(POfflineCacheUpdateParent* aActor)
{
  // Reclaim the IPDL reference.
  RefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
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

mozilla::ipc::IPCResult
ContentParent::RecvSetOfflinePermission(const Principal& aPrincipal)
{
  nsIPrincipal* principal = aPrincipal;
  nsContentUtils::MaybeAllowOfflineAppByDefault(principal);
  return IPC_OK();
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
        // Pass eDrop to get DataTransfer with external
        // drag formats cached.
        transfer = new DataTransfer(nullptr, eDrop, true, -1);
        session->SetDataTransfer(transfer);
      }
      // Note, even though this fills the DataTransfer object with
      // external data, the data is usually transfered over IPC lazily when
      // needed.
      transfer->FillAllExternalData();
      nsCOMPtr<nsILoadContext> lc = aParent ?
                                     aParent->GetLoadContext() : nullptr;
      nsCOMPtr<nsIArray> transferables =
        transfer->GetTransferables(lc);
      nsContentUtils::TransferablesToIPCTransferables(transferables,
                                                      dataTransfers,
                                                      false,
                                                      nullptr,
                                                      this);
      uint32_t action;
      session->GetDragAction(&action);
      mozilla::Unused << SendInvokeDragSession(dataTransfers, action);
    }
  }
}

mozilla::ipc::IPCResult
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
  return IPC_OK();
}

PContentPermissionRequestParent*
ContentParent::AllocPContentPermissionRequestParent(const InfallibleTArray<PermissionRequest>& aRequests,
                                                    const IPC::Principal& aPrincipal,
                                                    const TabId& aTabId)
{
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  RefPtr<TabParent> tp =
    cpm->GetTopLevelTabParentByProcessAndTabId(this->ChildID(), aTabId);
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

PWebBrowserPersistDocumentParent*
ContentParent::AllocPWebBrowserPersistDocumentParent(PBrowserParent* aBrowser,
                                                     const uint64_t& aOuterWindowID)
{
  return new WebBrowserPersistDocumentParent();
}

bool
ContentParent::DeallocPWebBrowserPersistDocumentParent(PWebBrowserPersistDocumentParent* aActor)
{
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult
ContentParent::RecvCreateWindow(PBrowserParent* aThisTab,
                                PBrowserParent* aNewTab,
                                PRenderFrameParent* aRenderFrame,
                                const uint32_t& aChromeFlags,
                                const bool& aCalledFromJS,
                                const bool& aPositionSpecified,
                                const bool& aSizeSpecified,
                                const nsCString& aFeatures,
                                const nsCString& aBaseURI,
                                const DocShellOriginAttributes& aOpenerOriginAttributes,
                                const float& aFullZoom,
                                nsresult* aResult,
                                bool* aWindowIsNew,
                                InfallibleTArray<FrameScriptInfo>* aFrameScripts,
                                nsCString* aURLToLoad,
                                TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                uint64_t* aLayersId)
{
  // We always expect to open a new window here. If we don't, it's an error.
  *aWindowIsNew = true;
  *aResult = NS_OK;

  // The content process should never be in charge of computing whether or
  // not a window should be private or remote - the parent will do that.
  const uint32_t badFlags =
        nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW
      | nsIWebBrowserChrome::CHROME_NON_PRIVATE_WINDOW
      | nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME
      | nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;
  if (!!(aChromeFlags & badFlags)) {
      return IPC_FAIL_NO_REASON(this);
  }

  TabParent* thisTabParent = nullptr;
  if (aThisTab) {
    thisTabParent = TabParent::GetFrom(aThisTab);
  }

  if (NS_WARN_IF(thisTabParent && thisTabParent->IsMozBrowser())) {
    return IPC_FAIL_NO_REASON(this);
  }

  TabParent* newTab = TabParent::GetFrom(aNewTab);
  MOZ_ASSERT(newTab);

  auto destroyNewTabOnError = MakeScopeExit([&] {
    if (!*aWindowIsNew || NS_FAILED(*aResult)) {
      if (newTab) {
        newTab->Destroy();
      }
    }
  });

  // Content has requested that we open this new content window, so
  // we must have an opener.
  newTab->SetHasContentOpener(true);

  nsCOMPtr<nsIContent> frame;
  if (thisTabParent) {
    frame = do_QueryInterface(thisTabParent->GetOwnerElement());
  }

  nsCOMPtr<nsPIDOMWindowOuter> parent;
  if (frame) {
    parent = frame->OwnerDoc()->GetWindow();

    // If our chrome window is in the process of closing, don't try to open a
    // new tab in it.
    if (parent && parent->Closed()) {
      parent = nullptr;
    }
  }

  nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
  if (thisTabParent) {
    browserDOMWin = thisTabParent->GetBrowserDOMWindow();
  }

  // If we haven't found a chrome window to open in, just use the most recently
  // opened one.
  if (!parent) {
    parent = nsContentUtils::GetMostRecentNonPBWindow();
    if (NS_WARN_IF(!parent)) {
      *aResult = NS_ERROR_FAILURE;
      return IPC_OK();
    }

    nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(parent);
    if (rootChromeWin) {
      rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    }
  }

  int32_t openLocation =
    nsWindowWatcher::GetWindowOpenLocation(parent, aChromeFlags, aCalledFromJS,
                                           aPositionSpecified, aSizeSpecified);

  MOZ_ASSERT(openLocation == nsIBrowserDOMWindow::OPEN_NEWTAB ||
             openLocation == nsIBrowserDOMWindow::OPEN_NEWWINDOW);

  // Opening new tabs is the easy case...
  if (openLocation == nsIBrowserDOMWindow::OPEN_NEWTAB) {
    if (NS_WARN_IF(!browserDOMWin)) {
      *aResult = NS_ERROR_ABORT;
      return IPC_OK();
    }

    bool isPrivate = false;
    if (thisTabParent) {
      nsCOMPtr<nsILoadContext> loadContext = thisTabParent->GetLoadContext();
      loadContext->GetUsePrivateBrowsing(&isPrivate);
    }

    nsCOMPtr<nsIOpenURIInFrameParams> params =
      new nsOpenURIInFrameParams(aOpenerOriginAttributes);
    params->SetReferrer(NS_ConvertUTF8toUTF16(aBaseURI));
    params->SetIsPrivate(isPrivate);

    TabParent::AutoUseNewTab aunt(newTab, aWindowIsNew, aURLToLoad);

    nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner;
    browserDOMWin->OpenURIInFrame(nullptr, params,
                                  openLocation,
                                  nsIBrowserDOMWindow::OPEN_NEW,
                                  getter_AddRefs(frameLoaderOwner));
    if (!frameLoaderOwner) {
      *aWindowIsNew = false;
    }

    newTab->SwapFrameScriptsFrom(*aFrameScripts);

    RenderFrameParent* rfp = static_cast<RenderFrameParent*>(aRenderFrame);
    if (!newTab->SetRenderFrame(rfp) ||
        !newTab->GetRenderFrameInfo(aTextureFactoryIdentifier, aLayersId)) {
      *aResult = NS_ERROR_FAILURE;
    }

    return IPC_OK();
  }

  TabParent::AutoUseNewTab aunt(newTab, aWindowIsNew, aURLToLoad);

  nsCOMPtr<nsPIWindowWatcher> pwwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, aResult);

  if (NS_WARN_IF(NS_FAILED(*aResult))) {
    return IPC_OK();
  }

  nsCOMPtr<nsITabParent> newRemoteTab;
  if (!thisTabParent) {
    // Because we weren't passed an opener tab, the content process has asked us
    // to open a new window that is unrelated to a pre-existing tab.
    *aResult = pwwatch->OpenWindowWithoutParent(getter_AddRefs(newRemoteTab));
  } else {
    *aResult = pwwatch->OpenWindowWithTabParent(thisTabParent, aFeatures, aCalledFromJS,
                                                aFullZoom, getter_AddRefs(newRemoteTab));
  }

  if (NS_WARN_IF(NS_FAILED(*aResult))) {
    return IPC_OK();
  }

  MOZ_ASSERT(TabParent::GetFrom(newRemoteTab) == newTab);

  newTab->SwapFrameScriptsFrom(*aFrameScripts);

  RenderFrameParent* rfp = static_cast<RenderFrameParent*>(aRenderFrame);
  if (!newTab->SetRenderFrame(rfp) ||
      !newTab->GetRenderFrameInfo(aTextureFactoryIdentifier, aLayersId)) {
    *aResult = NS_ERROR_FAILURE;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvProfile(const nsCString& aProfile)
{
#ifdef MOZ_ENABLE_PROFILER_SPS
  if (NS_WARN_IF(!mGatherer)) {
    return IPC_OK();
  }
  mProfile = aProfile;
  mGatherer->GatheredOOPProfile();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetGraphicsDeviceInitData(ContentDeviceData* aOut)
{
  gfxPlatform::GetPlatform()->BuildContentDeviceData(aOut);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGraphicsError(const nsCString& aError)
{
  gfx::LogForwarder* lf = gfx::Factory::GetLogForwarder();
  if (lf) {
    std::stringstream message;
    message << "CP+" << aError.get();
    lf->UpdateStringsVector(message.str());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvBeginDriverCrashGuard(const uint32_t& aGuardType, bool* aOutCrashed)
{
  // Only one driver crash guard should be active at a time, per-process.
  MOZ_ASSERT(!mDriverCrashGuard);

  UniquePtr<gfx::DriverCrashGuard> guard;
  switch (gfx::CrashGuardType(aGuardType)) {
  case gfx::CrashGuardType::D3D11Layers:
    guard = MakeUnique<gfx::D3D11LayersCrashGuard>(this);
    break;
  case gfx::CrashGuardType::D3D9Video:
    guard = MakeUnique<gfx::D3D9VideoCrashGuard>(this);
    break;
  case gfx::CrashGuardType::GLContext:
    guard = MakeUnique<gfx::GLContextCrashGuard>(this);
    break;
  case gfx::CrashGuardType::D3D11Video:
    guard = MakeUnique<gfx::D3D11VideoCrashGuard>(this);
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("unknown crash guard type");
    return IPC_FAIL_NO_REASON(this);
  }

  if (guard->Crashed()) {
    *aOutCrashed = true;
    return IPC_OK();
  }

  *aOutCrashed = false;
  mDriverCrashGuard = Move(guard);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvEndDriverCrashGuard(const uint32_t& aGuardType)
{
  mDriverCrashGuard = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetDeviceStorageLocation(const nsString& aType,
                                            nsString* aPath)
{
#ifdef MOZ_WIDGET_ANDROID
  mozilla::AndroidBridge::GetExternalPublicDirectory(aType, *aPath);
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult
ContentParent::RecvGetDeviceStorageLocations(DeviceStorageLocationInfo* info)
{
    DeviceStorageStatics::GetDeviceStorageLocationsForIPC(info);
    return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetAndroidSystemInfo(AndroidSystemInfo* aInfo)
{
#ifdef MOZ_WIDGET_ANDROID
  nsSystemInfo::GetAndroidSystemInfo(aInfo);
  return IPC_OK();
#else
  MOZ_CRASH("wrong platform!");
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyBenchmarkResult(const nsString& aCodecName,
                                         const uint32_t& aDecodeFPS)

{
  if (aCodecName.EqualsLiteral("VP9")) {
    Preferences::SetUint(VP9Benchmark::sBenchmarkFpsPref, aDecodeFPS);
    Preferences::SetUint(VP9Benchmark::sBenchmarkFpsVersionCheck,
                         VP9Benchmark::sBenchmarkVersionID);
  }
  return IPC_OK();
}

void
ContentParent::StartProfiler(nsIProfilerStartParams* aParams)
{
#ifdef MOZ_ENABLE_PROFILER_SPS
  if (NS_WARN_IF(!aParams)) {
    return;
  }

  ProfilerInitParams ipcParams;

  ipcParams.enabled() = true;
  aParams->GetEntries(&ipcParams.entries());
  aParams->GetInterval(&ipcParams.interval());
  ipcParams.features() = aParams->GetFeatures();
  ipcParams.threadFilters() = aParams->GetThreadFilterNames();

  Unused << SendStartProfiler(ipcParams);

  nsCOMPtr<nsIProfiler> profiler(do_GetService("@mozilla.org/tools/profiler;1"));
  if (NS_WARN_IF(!profiler)) {
    return;
  }
  nsCOMPtr<nsISupports> gatherer;
  profiler->GetProfileGatherer(getter_AddRefs(gatherer));
  mGatherer = static_cast<ProfileGatherer*>(gatherer.get());
#endif
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyPushObservers(const nsCString& aScope,
                                       const IPC::Principal& aPrincipal,
                                       const nsString& aMessageId)
{
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Nothing());
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObservers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyPushObserversWithData(const nsCString& aScope,
                                               const IPC::Principal& aPrincipal,
                                               const nsString& aMessageId,
                                               InfallibleTArray<uint8_t>&& aData)
{
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Some(aData));
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObservers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyPushSubscriptionChangeObservers(const nsCString& aScope,
                                                         const IPC::Principal& aPrincipal)
{
  PushSubscriptionChangeDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObservers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyPushSubscriptionModifiedObservers(const nsCString& aScope,
                                                           const IPC::Principal& aPrincipal)
{
  PushSubscriptionModifiedDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObservers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyLowMemory()
{
#ifdef MOZ_CRASHREPORTER
  nsThread::SaveMemoryReportNearOOM(nsThread::ShouldSaveMemoryReport::kForceReport);
#endif
  return IPC_OK();
}

/* static */ void
ContentParent::BroadcastBlobURLRegistration(const nsACString& aURI,
                                            BlobImpl* aBlobImpl,
                                            nsIPrincipal* aPrincipal,
                                            ContentParent* aIgnoreThisCP)
{
  nsCString uri(aURI);
  IPC::Principal principal(aPrincipal);

  for (auto* cp : AllProcesses(eLive)) {
    if (cp != aIgnoreThisCP) {
      PBlobParent* blobParent = cp->GetOrCreateActorForBlobImpl(aBlobImpl);
      if (blobParent) {
        Unused << cp->SendBlobURLRegistration(uri, blobParent, principal);
      }
    }
  }
}

/* static */ void
ContentParent::BroadcastBlobURLUnregistration(const nsACString& aURI,
                                              ContentParent* aIgnoreThisCP)
{
  nsCString uri(aURI);

  for (auto* cp : AllProcesses(eLive)) {
    if (cp != aIgnoreThisCP) {
      Unused << cp->SendBlobURLUnregistration(uri);
    }
  }
}

mozilla::ipc::IPCResult
ContentParent::RecvStoreAndBroadcastBlobURLRegistration(const nsCString& aURI,
                                                        PBlobParent* aBlobParent,
                                                        const Principal& aPrincipal)
{
  RefPtr<BlobImpl> blobImpl =
    static_cast<BlobParent*>(aBlobParent)->GetBlobImpl();
  if (NS_WARN_IF(!blobImpl)) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_SUCCEEDED(nsHostObjectProtocolHandler::AddDataEntry(aURI, aPrincipal,
                                                             blobImpl))) {
    BroadcastBlobURLRegistration(aURI, blobImpl, aPrincipal, this);

    // We want to store this blobURL, so we can unregister it if the child
    // crashes.
    mBlobURLs.AppendElement(aURI);
  }

  BroadcastBlobURLRegistration(aURI, blobImpl, aPrincipal, this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvUnstoreAndBroadcastBlobURLUnregistration(const nsCString& aURI)
{
  nsHostObjectProtocolHandler::RemoveDataEntry(aURI,
                                               false /* Don't broadcast */);
  BroadcastBlobURLUnregistration(aURI, this);
  mBlobURLs.RemoveElement(aURI);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetA11yContentId(uint32_t* aContentId)
{
#if defined(XP_WIN32) && defined(ACCESSIBILITY)
  *aContentId = a11y::AccessibleWrap::GetContentProcessIdFor(ChildID());
  MOZ_ASSERT(*aContentId);
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(ParentIdleListener, nsIObserver)

NS_IMETHODIMP
ParentIdleListener::Observe(nsISupports*, const char* aTopic, const char16_t* aData)
{
  mozilla::Unused << mParent->SendNotifyIdleObserver(mObserver,
                                                     nsDependentCString(aTopic),
                                                     nsDependentString(aData));
  return NS_OK;
}

bool
ContentParent::HandleWindowsMessages(const Message& aMsg) const
{
  MOZ_ASSERT(aMsg.is_sync());

  // a11y messages can be triggered by windows messages, which means if we
  // allow handling windows messages while we wait for the response to a sync
  // a11y message we can reenter the ipc message sending code.
  if (a11y::PDocAccessible::PDocAccessibleStart < aMsg.type() &&
      a11y::PDocAccessible::PDocAccessibleEnd > aMsg.type()) {
    return false;
  }

  return true;
}

mozilla::ipc::IPCResult
ContentParent::RecvGetFilesRequest(const nsID& aUUID,
                                   const nsString& aDirectoryPath,
                                   const bool& aRecursiveFlag)
{
  MOZ_ASSERT(!mGetFilesPendingRequests.GetWeak(aUUID));

  ErrorResult rv;
  RefPtr<GetFilesHelper> helper =
    GetFilesHelperParent::Create(aUUID, aDirectoryPath, aRecursiveFlag, this,
                                 rv);

  if (NS_WARN_IF(rv.Failed())) {
    if (!SendGetFilesResponse(aUUID,
                              GetFilesResponseFailure(rv.StealNSResult()))) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  mGetFilesPendingRequests.Put(aUUID, helper);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvDeleteGetFilesRequest(const nsID& aUUID)
{
  GetFilesHelper* helper = mGetFilesPendingRequests.GetWeak(aUUID);
  if (helper) {
    mGetFilesPendingRequests.Remove(aUUID);
  }

  return IPC_OK();
}

void
ContentParent::SendGetFilesResponseAndForget(const nsID& aUUID,
                                             const GetFilesResponseResult& aResult)
{
  GetFilesHelper* helper = mGetFilesPendingRequests.GetWeak(aUUID);
  if (helper) {
    mGetFilesPendingRequests.Remove(aUUID);
    Unused << SendGetFilesResponse(aUUID, aResult);
  }
}

void
ContentParent::ForceTabPaint(TabParent* aTabParent, uint64_t aLayerObserverEpoch)
{
  if (!mHangMonitorActor) {
    return;
  }
  ProcessHangMonitor::ForcePaint(mHangMonitorActor, aTabParent, aLayerObserverEpoch);
}

mozilla::ipc::IPCResult
ContentParent::RecvAccumulateChildHistogram(
                InfallibleTArray<Accumulation>&& aAccumulations)
{
  Telemetry::AccumulateChild(GeckoProcessType_Content, aAccumulations);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvAccumulateChildKeyedHistogram(
                InfallibleTArray<KeyedAccumulation>&& aAccumulations)
{
  Telemetry::AccumulateChildKeyed(GeckoProcessType_Content, aAccumulations);
  return IPC_OK();
}

PURLClassifierParent*
ContentParent::AllocPURLClassifierParent(const Principal& aPrincipal,
                                         const bool& aUseTrackingProtection,
                                         bool* aSuccess)
{
  MOZ_ASSERT(NS_IsMainThread());

  *aSuccess = true;
  RefPtr<URLClassifierParent> actor = new URLClassifierParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult
ContentParent::RecvPURLClassifierConstructor(PURLClassifierParent* aActor,
                                             const Principal& aPrincipal,
                                             const bool& aUseTrackingProtection,
                                             bool* aSuccess)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  auto* actor = static_cast<URLClassifierParent*>(aActor);
  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  if (!principal) {
    return IPC_FAIL_NO_REASON(this);
  }
  return actor->StartClassify(principal, aUseTrackingProtection, aSuccess);
}

bool
ContentParent::DeallocPURLClassifierParent(PURLClassifierParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  RefPtr<URLClassifierParent> actor =
    dont_AddRef(static_cast<URLClassifierParent*>(aActor));
  return true;
}

mozilla::ipc::IPCResult
ContentParent::RecvClassifyLocal(const URIParams& aURI, const nsCString& aTables,
                                 nsCString* aResults)
{
  MOZ_ASSERT(aResults);
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIURIClassifier> uriClassifier =
    do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID);
  if (!uriClassifier) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsresult rv = uriClassifier->ClassifyLocalWithTables(uri, aTables, *aResults);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL(this, "ClassifyLocalWithTables error");
  }
  return IPC_OK();
}
