/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"
#include "base/shared_memory.h"

#include "ContentParent.h"
#include "ProcessUtils.h"
#include "BrowserParent.h"

#include "chrome/common/process_watcher.h"
#include "mozilla/Result.h"
#include "mozilla/XREAppData.h"
#include "nsComponentManagerUtils.h"
#include "nsIBrowserDOMWindow.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/PDocAccessible.h"
#endif
#include "GMPServiceParent.h"
#include "HandlerServiceParent.h"
#include "IHistory.h"
#if defined(XP_WIN) && defined(ACCESSIBILITY)
#  include "mozilla/a11y/AccessibleWrap.h"
#  include "mozilla/a11y/Compatibility.h"
#endif
#include <map>
#include <utility>

#include "BrowserParent.h"
#include "ContentProcessManager.h"
#include "Geolocation.h"
#include "GfxInfoBase.h"
#include "MMPrinter.h"
#include "PreallocatedProcessManager.h"
#include "ProcessPriorityManager.h"
#include "SandboxHal.h"
#include "SourceSurfaceRawData.h"
#include "URIUtils.h"
#include "gfxPlatform.h"
#include "gfxPlatformFontList.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/ContentBlocking.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/BenchmarkStorageParent.h"
#include "mozilla/ContentBlockingUserInteraction.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DataStorage.h"
#include "mozilla/FOGIPC.h"
#include "mozilla/GlobalStyleSheetCache.h"
#include "mozilla/HangDetails.h"
#include "mozilla/LoginReputationIPC.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Maybe.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/PerformanceMetricsCollector.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ProcessHangMonitorIPC.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/Components.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryIPC.h"
#include "mozilla/Unused.h"
#include "mozilla/WebBrowserPersistDocumentParent.h"
#include "mozilla/devtools/HeapSnapshotTempFileHelperParent.h"
#include "mozilla/docshell/OfflineCacheUpdateParent.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/CancelContentJSOptionsBinding.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ExternalHelperAppParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemSecurity.h"
#include "mozilla/dom/GeolocationBinding.h"
#include "mozilla/dom/GeolocationPositionError.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/JSProcessActorBinding.h"
#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/MediaController.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/dom/MediaStatusManager.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/dom/PContentPermissionRequestParent.h"
#include "mozilla/dom/PCycleCollectWithLogsParent.h"
#include "mozilla/dom/PPresentationParent.h"
#include "mozilla/dom/ParentProcessMessageManager.h"
#include "mozilla/dom/Permissions.h"
#include "mozilla/dom/PresentationParent.h"
#include "mozilla/dom/ProcessMessageManager.h"
#include "mozilla/dom/PushNotifier.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/dom/StorageIPC.h"
#include "mozilla/dom/URLClassifierParent.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ipc/SharedMap.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "mozilla/embedding/printingui/PrintingParent.h"
#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/hal_sandbox/PHalParent.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/CrashReporterHost.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/PChildToParentStreamParent.h"
#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/PAPZParent.h"
#include "mozilla/loader/ScriptCacheActors.h"
#include "mozilla/media/MediaParent.h"
#include "mozilla/mozSpellChecker.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/PCookieServiceParent.h"
#include "mozilla/plugins/PluginBridge.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"
#include "mozilla/TelemetryEventEnums.h"
#include "mozilla/RemoteLazyInputStreamParent.h"
#include "mozilla/widget/RemoteLookAndFeel.h"
#include "mozilla/widget/ScreenManager.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsAppRunner.h"
#include "nsCExternalHandlerService.h"
#include "nsCOMPtr.h"
#include "nsChromeRegistryChrome.h"
#include "nsConsoleMessage.h"
#include "nsConsoleService.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsDebugImpl.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDocShell.h"
#include "nsEmbedCID.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsFrameMessageManager.h"
#include "nsHashPropertyBag.h"
#include "nsHyphenationManager.h"
#include "nsIAlertsService.h"
#include "nsIAppStartup.h"
#include "nsIAppWindow.h"
#include "nsIAsyncInputStream.h"
#include "nsIBidiKeyboard.h"
#include "nsICaptivePortalService.h"
#include "nsICertOverrideService.h"
#include "nsIClipboard.h"
#include "nsIContentProcess.h"
#include "nsIContentSecurityPolicy.h"
#include "nsICookie.h"
#include "nsICrashService.h"
#include "nsICycleCollectorListener.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDragService.h"
#include "nsIExternalProtocolService.h"
#include "nsIGfxInfo.h"
#include "nsIUserIdleService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILocalStorageManager.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMemoryReporter.h"
#include "nsIMozBrowserFrame.h"
#include "nsINetworkLinkService.h"
#include "nsIObserverService.h"
#include "nsIParentChannel.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceWorkerManager.h"
#include "nsISiteSecurityService.h"
#include "nsISound.h"
#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsIURL.h"
#include "nsIWebBrowserChrome.h"
#include "nsIX509Cert.h"
#include "nsIXULRuntime.h"
#include "nsMemoryInfoDumper.h"
#include "nsMemoryReporterManager.h"
#include "nsOpenURIInFrameParams.h"
#include "nsPIWindowWatcher.h"
#include "nsPluginHost.h"
#include "nsPluginTags.h"
#include "nsQueryObject.h"
#include "nsReadableUtils.h"
#include "nsSHistory.h"
#include "nsScriptError.h"
#include "nsSerializationHelper.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsStyleSheetService.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsWidgetsCID.h"
#include "nsWindowWatcher.h"
#include "prio.h"
#include "private/pprio.h"
#include "xpcpublic.h"
#include "nsOpenWindowInfo.h"

#ifdef MOZ_WEBRTC
#  include "jsapi/WebrtcGlobalParent.h"
#endif

#if defined(XP_MACOSX)
#  include "nsMacUtilsImpl.h"
#endif

#if defined(ANDROID) || defined(LINUX)
#  include "nsSystemInfo.h"
#endif

#if defined(XP_LINUX)
#  include "mozilla/Hal.h"
#endif

#ifdef ANDROID
#  include "gfxAndroidPlatform.h"
#endif

#include "mozilla/PermissionManager.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBridge.h"
#  include "mozilla/java/GeckoProcessManagerWrappers.h"
#  include "mozilla/java/GeckoProcessTypeWrappers.h"
#endif

#ifdef MOZ_WIDGET_GTK
#  include <gdk/gdk.h>
#endif

#include "mozilla/RemoteSpellCheckEngineParent.h"

#include "Crypto.h"

#ifdef MOZ_WEBSPEECH
#  include "mozilla/dom/SpeechSynthesisParent.h"
#endif

#if defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  if defined(XP_LINUX)
#    include "mozilla/SandboxInfo.h"
#    include "mozilla/SandboxBroker.h"
#    include "mozilla/SandboxBrokerPolicyFactory.h"
#  endif
#  if defined(XP_MACOSX)
#    include "mozilla/Sandbox.h"
#  endif
#endif

#ifdef XP_WIN
#  include "mozilla/widget/AudioSession.h"
#  include "mozilla/widget/WinContentSystemParameters.h"
#  include "mozilla/WinDllServices.h"
#endif

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif

#ifdef MOZ_GECKO_PROFILER
#  include "nsIProfiler.h"
#  include "ProfilerParent.h"
#endif

#ifdef MOZ_CODE_COVERAGE
#  include "mozilla/CodeCoverageHandler.h"
#endif

// For VP9Benchmark::sBenchmarkFpsPref
#include "Benchmark.h"

// XXX need another bug to move this to a common header.
#ifdef DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

using base::KillProcess;

using namespace CrashReporter;
using namespace mozilla::dom::power;
using namespace mozilla::media;
using namespace mozilla::embedding;
using namespace mozilla::gfx;
using namespace mozilla::gmp;
using namespace mozilla::hal;
using namespace mozilla::ipc;
using namespace mozilla::intl;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::net;
using namespace mozilla::psm;
using namespace mozilla::widget;
using namespace mozilla::Telemetry;
using mozilla::loader::PScriptCacheParent;
using mozilla::Telemetry::ProcessID;

extern mozilla::LazyLogModule gFocusLog;

#define LOGFOCUS(args) MOZ_LOG(gFocusLog, mozilla::LogLevel::Debug, args)

// XXX Workaround for bug 986973 to maintain the existing broken semantics
template <>
struct nsIConsoleService::COMTypeInfo<nsConsoleService, void> {
  static const nsIID kIID;
};
const nsIID nsIConsoleService::COMTypeInfo<nsConsoleService, void>::kIID =
    NS_ICONSOLESERVICE_IID;

namespace mozilla {
namespace CubebUtils {
extern FileDescriptor CreateAudioIPCConnection();
}

namespace dom {

LazyLogModule gProcessLog("Process");

static std::map<RemoteDecodeIn, PDMFactory::MediaCodecsSupported>
    sCodecsSupported;

/* static */
uint32_t ContentParent::sMaxContentProcesses = 0;

/* static */
Maybe<TimeStamp> ContentParent::sLastContentProcessLaunch = Nothing();

/* static */
LogModule* ContentParent::GetLog() { return gProcessLog; }

#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"
#define NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC "ipc:network:set-connectivity"

// IPC receiver for remote GC/CC logging.
class CycleCollectWithLogsParent final : public PCycleCollectWithLogsParent {
 public:
  MOZ_COUNTED_DTOR(CycleCollectWithLogsParent)

  static bool AllocAndSendConstructor(ContentParent* aManager,
                                      bool aDumpAllTraces,
                                      nsICycleCollectorLogSink* aSink,
                                      nsIDumpGCAndCCLogsCallback* aCallback) {
    CycleCollectWithLogsParent* actor;
    FILE* gcLog;
    FILE* ccLog;
    nsresult rv;

    actor = new CycleCollectWithLogsParent(aSink, aCallback);
    rv = actor->mSink->Open(&gcLog, &ccLog);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      delete actor;
      return false;
    }

    return aManager->SendPCycleCollectWithLogsConstructor(
        actor, aDumpAllTraces, FILEToFileDescriptor(gcLog),
        FILEToFileDescriptor(ccLog));
  }

 private:
  virtual mozilla::ipc::IPCResult RecvCloseGCLog() override {
    Unused << mSink->CloseGCLog();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvCloseCCLog() override {
    Unused << mSink->CloseCCLog();
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult Recv__delete__() override {
    // Report completion to mCallback only on successful
    // completion of the protocol.
    nsCOMPtr<nsIFile> gcLog, ccLog;
    mSink->GetGcLog(getter_AddRefs(gcLog));
    mSink->GetCcLog(getter_AddRefs(ccLog));
    Unused << mCallback->OnDump(gcLog, ccLog, /* parent = */ false);
    return IPC_OK();
  }

  virtual void ActorDestroy(ActorDestroyReason aReason) override {
    // If the actor is unexpectedly destroyed, we deliberately
    // don't call Close[GC]CLog on the sink, because the logs may
    // be incomplete.  See also the nsCycleCollectorLogSinkToFile
    // implementaiton of those methods, and its destructor.
  }

  CycleCollectWithLogsParent(nsICycleCollectorLogSink* aSink,
                             nsIDumpGCAndCCLogsCallback* aCallback)
      : mSink(aSink), mCallback(aCallback) {
    MOZ_COUNT_CTOR(CycleCollectWithLogsParent);
  }

  nsCOMPtr<nsICycleCollectorLogSink> mSink;
  nsCOMPtr<nsIDumpGCAndCCLogsCallback> mCallback;
};

// A memory reporter for ContentParent objects themselves.
class ContentParentsMemoryReporter final : public nsIMemoryReporter {
  ~ContentParentsMemoryReporter() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER
};

NS_IMPL_ISUPPORTS(ContentParentsMemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
ContentParentsMemoryReporter::CollectReports(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    bool aAnonymize) {
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

    nsPrintfCString path(
        "queued-ipc-messages/content-parent"
        "(%s, pid=%d, %s, 0x%p, refcnt=%" PRIuPTR ")",
        NS_ConvertUTF16toUTF8(friendlyName).get(), cp->Pid(), channelStr,
        static_cast<nsIObserver*>(cp), refcnt);

    constexpr auto desc =
        "The number of unset IPC messages held in this ContentParent's "
        "channel.  A large value here might indicate that we're leaking "
        "messages.  Similarly, a ContentParent object for a process that's no "
        "longer running could indicate that we're leaking ContentParents."_ns;

    aHandleReport->Callback(/* process */ ""_ns, path, KIND_OTHER, UNITS_COUNT,
                            numQueuedMessages, desc, aData);
  }

  return NS_OK;
}

// A hashtable (by type) of processes/ContentParents.  This includes
// processes that are in the Preallocator cache (which would be type
// 'prealloc'), and recycled processes ('web' and in the future
// eTLD+1-locked) processes).
nsClassHashtable<nsCStringHashKey, nsTArray<ContentParent*>>*
    ContentParent::sBrowserContentParents;

namespace {

uint64_t ComputeLoadedOriginHash(nsIPrincipal* aPrincipal) {
  uint32_t originNoSuffix =
      BasePrincipal::Cast(aPrincipal)->GetOriginNoSuffixHash();
  uint32_t originSuffix =
      BasePrincipal::Cast(aPrincipal)->GetOriginSuffixHash();

  return ((uint64_t)originNoSuffix) << 32 | originSuffix;
}

class ScriptableCPInfo final : public nsIContentProcessInfo {
 public:
  explicit ScriptableCPInfo(ContentParent* aParent) : mContentParent(aParent) {
    MOZ_ASSERT(mContentParent);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPROCESSINFO

  void ProcessDied() { mContentParent = nullptr; }

 private:
  ~ScriptableCPInfo() { MOZ_ASSERT(!mContentParent, "must call ProcessDied"); }

  ContentParent* mContentParent;
};

NS_IMPL_ISUPPORTS(ScriptableCPInfo, nsIContentProcessInfo)

NS_IMETHODIMP
ScriptableCPInfo::GetIsAlive(bool* aIsAlive) {
  *aIsAlive = mContentParent != nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ScriptableCPInfo::GetProcessId(int32_t* aPID) {
  if (!mContentParent) {
    *aPID = -1;
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aPID = mContentParent->Pid();
  if (*aPID == -1) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
ScriptableCPInfo::GetTabCount(int32_t* aTabCount) {
  if (!mContentParent) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  *aTabCount = cpm->GetBrowserParentCountByProcessId(mContentParent->ChildID());

  return NS_OK;
}

NS_IMETHODIMP
ScriptableCPInfo::GetMessageManager(nsISupports** aMessenger) {
  *aMessenger = nullptr;
  if (!mContentParent) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<ProcessMessageManager> manager = mContentParent->GetMessageManager();
  manager.forget(aMessenger);
  return NS_OK;
}

ProcessID GetTelemetryProcessID(const nsACString& remoteType) {
  // OOP WebExtensions run in a content process.
  // For Telemetry though we want to break out collected data from the
  // WebExtensions process into a separate bucket, to make sure we can analyze
  // it separately and avoid skewing normal content process metrics.
  return remoteType == EXTENSION_REMOTE_TYPE ? ProcessID::Extension
                                             : ProcessID::Content;
}

}  // anonymous namespace

UniquePtr<nsTHashMap<nsUint32HashKey, ContentParent*>>
    ContentParent::sJSPluginContentParents;
UniquePtr<nsTArray<ContentParent*>> ContentParent::sPrivateContent;
UniquePtr<LinkedList<ContentParent>> ContentParent::sContentParents;
StaticRefPtr<ContentParent> ContentParent::sRecycledE10SProcess;
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
UniquePtr<SandboxBrokerPolicyFactory>
    ContentParent::sSandboxBrokerPolicyFactory;
#endif
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
UniquePtr<std::vector<std::string>> ContentParent::sMacSandboxParams;
#endif

// Whether a private docshell has been seen before.
static bool sHasSeenPrivateDocShell = false;

// This is true when subprocess launching is enabled.  This is the
// case between StartUp() and ShutDown().
static bool sCanLaunchSubprocesses;

// Set to true when the first content process gets created.
static bool sCreatedFirstContentProcess = false;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
// True when we're running the process selection code, and do not expect to
// enter code paths where processes may die.
static bool sInProcessSelector = false;
#endif

// The first content child has ID 1, so the chrome process can have ID 0.
static uint64_t gContentChildID = 1;

static const char* sObserverTopics[] = {
    NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC,
    NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC,
    NS_IPC_CAPTIVE_PORTAL_SET_STATE,
    "application-background",
    "application-foreground",
    "memory-pressure",
    "child-gc-request",
    "child-cc-request",
    "child-mmu-request",
    "child-ghost-request",
    "last-pb-context-exited",
    "file-watcher-update",
#ifdef ACCESSIBILITY
    "a11y-init-or-shutdown",
#endif
    "cacheservice:empty-cache",
    "intl:app-locales-changed",
    "intl:requested-locales-changed",
    "cookie-changed",
    "private-cookie-changed",
    NS_NETWORK_LINK_TYPE_TOPIC,
};

// PreallocateProcess is called by the PreallocatedProcessManager.
// ContentParent then takes this process back within GetNewOrUsedBrowserProcess.
/*static*/ RefPtr<ContentParent::LaunchPromise>
ContentParent::PreallocateProcess() {
  RefPtr<ContentParent> process = new ContentParent(PREALLOC_REMOTE_TYPE);

  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("Preallocating process of type prealloc"));
  return process->LaunchSubprocessAsync(PROCESS_PRIORITY_PREALLOC);
}

/*static*/
void ContentParent::StartUp() {
  // We could launch sub processes from content process
  // FIXME Bug 1023701 - Stop using ContentParent static methods in
  // child process
  sCanLaunchSubprocesses = true;

  if (!XRE_IsParentProcess()) {
    return;
  }

  // From this point on, NS_WARNING, NS_ASSERTION, etc. should print out the
  // PID along with the warning.
  nsDebugImpl::SetMultiprocessMode("Parent");

  // Note: This reporter measures all ContentParents.
  RegisterStrongMemoryReporter(new ContentParentsMemoryReporter());

  BackgroundChild::Startup();
  ClientManager::Startup();

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  sSandboxBrokerPolicyFactory = MakeUnique<SandboxBrokerPolicyFactory>();
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  sMacSandboxParams = MakeUnique<std::vector<std::string>>();
#endif
}

/*static*/
void ContentParent::ShutDown() {
  // No-op for now.  We rely on normal process shutdown and
  // ClearOnShutdown() to clean up our state.
  sCanLaunchSubprocesses = false;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  sSandboxBrokerPolicyFactory = nullptr;
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  sMacSandboxParams = nullptr;
#endif
}

/*static*/
uint32_t ContentParent::GetPoolSize(const nsACString& aContentProcessType) {
  if (!sBrowserContentParents) {
    return 0;
  }

  nsTArray<ContentParent*>* parents =
      sBrowserContentParents->Get(aContentProcessType);

  return parents ? parents->Length() : 0;
}

/*static*/ nsTArray<ContentParent*>& ContentParent::GetOrCreatePool(
    const nsACString& aContentProcessType) {
  if (!sBrowserContentParents) {
    sBrowserContentParents =
        new nsClassHashtable<nsCStringHashKey, nsTArray<ContentParent*>>;
  }

  return *sBrowserContentParents->GetOrInsertNew(aContentProcessType);
}

const nsDependentCSubstring RemoteTypePrefix(
    const nsACString& aContentProcessType) {
  // The suffix after a `=` in a remoteType is dynamic, and used to control the
  // process pool to use.
  int32_t equalIdx = aContentProcessType.FindChar(L'=');
  if (equalIdx == kNotFound) {
    equalIdx = aContentProcessType.Length();
  }
  return StringHead(aContentProcessType, equalIdx);
}

bool IsWebRemoteType(const nsACString& aContentProcessType) {
  // Note: matches webIsolated as well as web (and webLargeAllocation, and
  // webCOOP+COEP)
  return StringBeginsWith(aContentProcessType, DEFAULT_REMOTE_TYPE);
}

bool IsWebCoopCoepRemoteType(const nsACString& aContentProcessType) {
  return StringBeginsWith(aContentProcessType,
                          WITH_COOP_COEP_REMOTE_TYPE_PREFIX);
}

bool IsPrivilegedMozillaRemoteType(const nsACString& aContentProcessType) {
  return aContentProcessType == PRIVILEGEDMOZILLA_REMOTE_TYPE;
}

bool IsExtensionRemoteType(const nsACString& aContentProcessType) {
  return aContentProcessType == EXTENSION_REMOTE_TYPE;
}

/*static*/
uint32_t ContentParent::GetMaxProcessCount(
    const nsACString& aContentProcessType) {
  // Max process count is based only on the prefix.
  const nsDependentCSubstring processTypePrefix =
      RemoteTypePrefix(aContentProcessType);

  // Check for the default remote type of "web", as it uses different prefs.
  if (processTypePrefix == DEFAULT_REMOTE_TYPE) {
    return GetMaxWebProcessCount();
  }

  // Read the pref controling this remote type. `dom.ipc.processCount` is not
  // used as a fallback, as it is intended to control the number of "web"
  // content processes, checked in `mozilla::GetMaxWebProcessCount()`.
  nsAutoCString processCountPref("dom.ipc.processCount.");
  processCountPref.Append(processTypePrefix);

  int32_t maxContentParents = Preferences::GetInt(processCountPref.get(), 1);
  if (maxContentParents < 1) {
    maxContentParents = 1;
  }

  return static_cast<uint32_t>(maxContentParents);
}

/*static*/
bool ContentParent::IsMaxProcessCountReached(
    const nsACString& aContentProcessType) {
  return GetPoolSize(aContentProcessType) >=
         GetMaxProcessCount(aContentProcessType);
}

// Really more ReleaseUnneededProcesses()
/*static*/
void ContentParent::ReleaseCachedProcesses() {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("ReleaseCachedProcesses:"));
  if (!sBrowserContentParents) {
    return;
  }

#ifdef DEBUG
  int num = 0;
  for (auto iter = sBrowserContentParents->Iter(); !iter.Done(); iter.Next()) {
    nsTArray<ContentParent*>* contentParents = iter.Data().get();
    num += contentParents->Length();
    for (auto* cp : *contentParents) {
      MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
              ("%s: %zu processes", cp->mRemoteType.get(),
               contentParents->Length()));
      break;
    }
  }
#endif
  // We process the toRelease array outside of the iteration to avoid modifying
  // the list (via RemoveFromList()) while we're iterating it.
  nsTArray<ContentParent*> toRelease;
  for (auto iter = sBrowserContentParents->Iter(); !iter.Done(); iter.Next()) {
    nsTArray<ContentParent*>* contentParents = iter.Data().get();

    // Shutting down these processes will change the array so let's use another
    // array for the removal.
    for (auto* cp : *contentParents) {
      if (cp->ManagedPBrowserParent().Count() == 0 &&
          !cp->HasActiveWorkerOrJSPlugin() &&
          cp->mRemoteType == DEFAULT_REMOTE_TYPE) {
        toRelease.AppendElement(cp);
      } else {
        MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
                ("  Skipping %p (%s), count %d, HasActiveWorkerOrJSPlugin %d",
                 cp, cp->mRemoteType.get(), cp->ManagedPBrowserParent().Count(),
                 cp->HasActiveWorkerOrJSPlugin()));
      }
    }
  }

  for (auto* cp : toRelease) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("  Shutdown %p (%s)", cp, cp->mRemoteType.get()));
    PreallocatedProcessManager::Erase(cp);
    // Start a soft shutdown.
    cp->ShutDownProcess(SEND_SHUTDOWN_MESSAGE);
    // Make sure we don't select this process for new tabs.
    cp->MarkAsDead();
    // Make sure that this process is no longer accessible from JS by its
    // message manager.
    cp->ShutDownMessageManager();
  }
}

/*static*/
already_AddRefed<ContentParent> ContentParent::MinTabSelect(
    const nsTArray<ContentParent*>& aContentParents,
    int32_t aMaxContentParents) {
  uint32_t maxSelectable =
      std::min(static_cast<uint32_t>(aContentParents.Length()),
               static_cast<uint32_t>(aMaxContentParents));
  uint32_t min = INT_MAX;
  RefPtr<ContentParent> candidate;
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();

  for (uint32_t i = 0; i < maxSelectable; i++) {
    ContentParent* p = aContentParents[i];
    MOZ_DIAGNOSTIC_ASSERT(!p->IsDead());

    uint32_t tabCount = cpm->GetBrowserParentCountByProcessId(p->ChildID());
    if (tabCount < min) {
      candidate = p;
      min = tabCount;
    }
  }

  // If all current processes have at least one tab and we have not yet reached
  // the maximum, use a new process.
  if (min > 0 &&
      aContentParents.Length() < static_cast<uint32_t>(aMaxContentParents)) {
    return nullptr;
  }

  // Otherwise we return candidate.
  return candidate.forget();
}

static already_AddRefed<nsIPrincipal> CreateRemoteTypeIsolationPrincipal(
    const nsACString& aRemoteType) {
  if ((RemoteTypePrefix(aRemoteType) != FISSION_WEB_REMOTE_TYPE) &&
      !StringBeginsWith(aRemoteType, WITH_COOP_COEP_REMOTE_TYPE_PREFIX)) {
    return nullptr;
  }

  int32_t offset = aRemoteType.FindChar('=') + 1;
  MOZ_ASSERT(offset > 1, "can not extract origin from that remote type");
  nsAutoCString origin(
      Substring(aRemoteType, offset, aRemoteType.Length() - offset));

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> principal;
  ssm->CreateContentPrincipalFromOrigin(origin, getter_AddRefs(principal));
  return principal.forget();
}

/*static*/
already_AddRefed<ContentParent> ContentParent::GetUsedBrowserProcess(
    const nsACString& aRemoteType, nsTArray<ContentParent*>& aContentParents,
    uint32_t aMaxContentParents, bool aPreferUsed) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  AutoRestore ar(sInProcessSelector);
  sInProcessSelector = true;
#endif

  uint32_t numberOfParents = aContentParents.Length();
  nsTArray<RefPtr<nsIContentProcessInfo>> infos(numberOfParents);
  for (auto* cp : aContentParents) {
    infos.AppendElement(cp->mScriptableHelper);
  }

  if (aPreferUsed && numberOfParents) {
    // For the preloaded browser we don't want to create a new process but
    // reuse an existing one.
    aMaxContentParents = numberOfParents;
  }

  nsCOMPtr<nsIContentProcessProvider> cpp =
      do_GetService("@mozilla.org/ipc/processselector;1");
  int32_t index;
  if (cpp && NS_SUCCEEDED(cpp->ProvideProcess(aRemoteType, infos,
                                              aMaxContentParents, &index))) {
    // If the provider returned an existing ContentParent, use that one.
    if (0 <= index && static_cast<uint32_t>(index) <= aMaxContentParents) {
      RefPtr<ContentParent> retval = aContentParents[index];
#ifdef MOZ_GECKO_PROFILER
      if (profiler_thread_is_being_profiled()) {
        nsPrintfCString marker("Reused process %u",
                               (unsigned int)retval->ChildID());
        PROFILER_MARKER_TEXT("Process", DOM, {}, marker);
      }
#endif
      MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
              ("GetUsedProcess: Reused process %p (%u) for %s", retval.get(),
               (unsigned int)retval->ChildID(),
               PromiseFlatCString(aRemoteType).get()));
      retval->AssertAlive();
      retval->StopRecycling();
      return retval.forget();
    }
  } else {
    // If there was a problem with the JS chooser, fall back to a random
    // selection.
    NS_WARNING("nsIContentProcessProvider failed to return a process");
    RefPtr<ContentParent> random;
    if ((random = MinTabSelect(aContentParents, aMaxContentParents))) {
      MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
              ("GetUsedProcess: Reused random process %p (%d) for %s",
               random.get(), (unsigned int)random->ChildID(),
               PromiseFlatCString(aRemoteType).get()));
      random->AssertAlive();
      random->StopRecycling();
      return random.forget();
    }
  }

  // If we are loading into the "web" remote type, are choosing to launch a new
  // tab, and have a recycled E10S process, we should launch into that process.
  if (aRemoteType == DEFAULT_REMOTE_TYPE && sRecycledE10SProcess) {
    RefPtr<ContentParent> recycled = sRecycledE10SProcess;
    MOZ_DIAGNOSTIC_ASSERT(recycled->GetRemoteType() == DEFAULT_REMOTE_TYPE);
    recycled->AssertAlive();
    recycled->StopRecycling();

#ifdef MOZ_GECKO_PROFILER
    if (profiler_thread_is_being_profiled()) {
      nsPrintfCString marker("Recycled process %u (%p)",
                             (unsigned int)recycled->ChildID(), recycled.get());
      PROFILER_MARKER_TEXT("Process", DOM, {}, marker);
    }
#endif
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Recycled process %p", recycled.get()));

    return recycled.forget();
  }

  // Try to take a preallocated process except for certain remote types.
  RefPtr<ContentParent> preallocated;
  if (aRemoteType != FILE_REMOTE_TYPE &&
      aRemoteType != EXTENSION_REMOTE_TYPE &&  // Bug 1638119
      (preallocated = PreallocatedProcessManager::Take(aRemoteType))) {
    MOZ_DIAGNOSTIC_ASSERT(preallocated->GetRemoteType() ==
                          PREALLOC_REMOTE_TYPE);
    MOZ_DIAGNOSTIC_ASSERT(sRecycledE10SProcess != preallocated);
    preallocated->AssertAlive();

#ifdef MOZ_GECKO_PROFILER
    if (profiler_thread_is_being_profiled()) {
      nsPrintfCString marker("Assigned preallocated process %u",
                             (unsigned int)preallocated->ChildID());
      PROFILER_MARKER_TEXT("Process", DOM, {}, marker);
    }
#endif
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Adopted preallocated process %p for type %s", preallocated.get(),
             PromiseFlatCString(aRemoteType).get()));

    // Specialize this process for the appropriate remote type, and activate it.
    preallocated->mActivateTS = TimeStamp::Now();
    preallocated->AddToPool(aContentParents);

    preallocated->mRemoteType.Assign(aRemoteType);
    preallocated->mRemoteTypeIsolationPrincipal =
        CreateRemoteTypeIsolationPrincipal(aRemoteType);
    Unused << preallocated->SendRemoteType(preallocated->mRemoteType);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      nsAutoString cpId;
      cpId.AppendInt(static_cast<uint64_t>(preallocated->ChildID()));
      obs->NotifyObservers(static_cast<nsIObserver*>(preallocated),
                           "process-type-set", cpId.get());
      preallocated->AssertAlive();
    }
    return preallocated.forget();
  }

  return nullptr;
}

/*static*/
already_AddRefed<ContentParent>
ContentParent::GetNewOrUsedLaunchingBrowserProcess(
    const nsACString& aRemoteType, BrowsingContextGroup* aGroup,
    ProcessPriority aPriority, bool aPreferUsed) {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("GetNewOrUsedProcess for type %s",
           PromiseFlatCString(aRemoteType).get()));

  // If we have an existing host process attached to this BrowsingContextGroup,
  // always return it, as we can never have multiple host processes within a
  // single BrowsingContextGroup.
  RefPtr<ContentParent> contentParent;
  if (aGroup) {
    contentParent = aGroup->GetHostProcess(aRemoteType);
    if (contentParent) {
      MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
              ("GetNewOrUsedProcess: Existing host process %p (launching %d)",
               contentParent.get(), contentParent->IsLaunching()));
      contentParent->AssertAlive();
      contentParent->StopRecycling();
      return contentParent.forget();
    }
  }

  nsTArray<ContentParent*>& contentParents = GetOrCreatePool(aRemoteType);
  uint32_t maxContentParents = GetMaxProcessCount(aRemoteType);
  // We never want to re-use Large-Allocation processes.
  if (aRemoteType == LARGE_ALLOCATION_REMOTE_TYPE &&
      contentParents.Length() >= maxContentParents) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("GetNewOrUsedProcess: returning Large Used process"));
    return GetNewOrUsedLaunchingBrowserProcess(DEFAULT_REMOTE_TYPE, aGroup,
                                               aPriority,
                                               /*aPreferUsed =*/false);
  }

  // Let's try and reuse an existing process.
  contentParent = GetUsedBrowserProcess(aRemoteType, contentParents,
                                        maxContentParents, aPreferUsed);

  if (contentParent) {
    // We have located a process. It may not have finished initializing,
    // this will be for the caller to handle.
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("GetNewOrUsedProcess: Used process %p (launching %d)",
             contentParent.get(), contentParent->IsLaunching()));
    contentParent->AssertAlive();
    contentParent->StopRecycling();
    if (aGroup) {
      aGroup->EnsureHostProcess(contentParent);
    }
    return contentParent.forget();
  }

  // No reusable process. Let's create and launch one.
  // The life cycle will be set to `LifecycleState::LAUNCHING`.
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("Launching new process immediately for type %s",
           PromiseFlatCString(aRemoteType).get()));

  contentParent = new ContentParent(aRemoteType);
  if (!contentParent->BeginSubprocessLaunch(aPriority)) {
    // Launch aborted because of shutdown. Bailout.
    contentParent->LaunchSubprocessReject();
    return nullptr;
  }
  // Store this process for future reuse.
  contentParent->AddToPool(contentParents);

  // Until the new process is ready let's not allow to start up any
  // preallocated processes. The blocker will be removed once we receive
  // the first idle message.
  contentParent->mIsAPreallocBlocker = true;
  PreallocatedProcessManager::AddBlocker(aRemoteType, contentParent);

  MOZ_ASSERT(contentParent->IsLaunching());
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("GetNewOrUsedProcess: new process %p", contentParent.get()));
  contentParent->AssertAlive();
  contentParent->StopRecycling();
  if (aGroup) {
    aGroup->EnsureHostProcess(contentParent);
  }
  return contentParent.forget();
}

/*static*/
RefPtr<ContentParent::LaunchPromise>
ContentParent::GetNewOrUsedBrowserProcessAsync(const nsACString& aRemoteType,
                                               BrowsingContextGroup* aGroup,
                                               ProcessPriority aPriority,
                                               bool aPreferUsed) {
  // Obtain a `ContentParent` launched asynchronously.
  RefPtr<ContentParent> contentParent = GetNewOrUsedLaunchingBrowserProcess(
      aRemoteType, aGroup, aPriority, aPreferUsed);
  if (!contentParent) {
    // In case of launch error, stop here.
    return LaunchPromise::CreateAndReject(LaunchError(), __func__);
  }
  return contentParent->WaitForLaunchAsync(aPriority);
}

/*static*/
already_AddRefed<ContentParent> ContentParent::GetNewOrUsedBrowserProcess(
    const nsACString& aRemoteType, BrowsingContextGroup* aGroup,
    ProcessPriority aPriority, bool aPreferUsed) {
  RefPtr<ContentParent> contentParent = GetNewOrUsedLaunchingBrowserProcess(
      aRemoteType, aGroup, aPriority, aPreferUsed);
  if (!contentParent || !contentParent->WaitForLaunchSync(aPriority)) {
    // In case of launch error, stop here.
    return nullptr;
  }
  return contentParent.forget();
}

RefPtr<ContentParent::LaunchPromise> ContentParent::WaitForLaunchAsync(
    ProcessPriority aPriority) {
  MOZ_DIAGNOSTIC_ASSERT(!IsDead());
  if (!IsLaunching()) {
    return LaunchPromise::CreateAndResolve(this, __func__);
  }

  // We've started an async content process launch.
  Telemetry::Accumulate(Telemetry::CONTENT_PROCESS_LAUNCH_IS_SYNC, 0);

  // We have located a process that hasn't finished initializing, then attempt
  // to finish initializing. Both `LaunchSubprocessResolve` and
  // `LaunchSubprocessReject` are safe to call multiple times if we race with
  // other `WaitForLaunchAsync` callbacks.
  return mSubprocess->WhenProcessHandleReady()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, aPriority] {
        if (self->LaunchSubprocessResolve(/* aIsSync = */ false, aPriority)) {
          self->mActivateTS = TimeStamp::Now();
          return LaunchPromise::CreateAndResolve(self, __func__);
        }

        self->LaunchSubprocessReject();
        return LaunchPromise::CreateAndReject(LaunchError(), __func__);
      },
      [self = RefPtr{this}] {
        self->LaunchSubprocessReject();
        return LaunchPromise::CreateAndReject(LaunchError(), __func__);
      });
}

bool ContentParent::WaitForLaunchSync(ProcessPriority aPriority) {
  MOZ_DIAGNOSTIC_ASSERT(!IsDead());
  if (!IsLaunching()) {
    return true;
  }

  // We've started a sync content process launch.
  Telemetry::Accumulate(Telemetry::CONTENT_PROCESS_LAUNCH_IS_SYNC, 1);

  // We're a process which hasn't finished initializing. We may be racing
  // against whoever launched it (and whoever else is already racing). Since
  // we're sync, we win the race and finish the initialization.
  bool launchSuccess = mSubprocess->WaitForProcessHandle();
  if (launchSuccess &&
      LaunchSubprocessResolve(/* aIsSync = */ true, aPriority)) {
    mActivateTS = TimeStamp::Now();
    return true;
  }
  // In case of failure.
  LaunchSubprocessReject();
  return false;
}

/*static*/
already_AddRefed<ContentParent> ContentParent::GetNewOrUsedJSPluginProcess(
    uint32_t aPluginID, const hal::ProcessPriority& aPriority) {
  RefPtr<ContentParent> p;
  if (sJSPluginContentParents) {
    p = sJSPluginContentParents->Get(aPluginID);
  } else {
    sJSPluginContentParents =
        MakeUnique<nsTHashMap<nsUint32HashKey, ContentParent*>>();
  }

  if (p) {
    return p.forget();
  }

  p = new ContentParent(aPluginID);

  if (!p->LaunchSubprocessSync(aPriority)) {
    return nullptr;
  }

  sJSPluginContentParents->InsertOrUpdate(aPluginID, p);

  return p.forget();
}

#if defined(XP_WIN)
extern const wchar_t* kPluginWidgetContentParentProperty;

/*static*/
void ContentParent::SendAsyncUpdate(nsIWidget* aWidget) {
  if (!aWidget || aWidget->Destroyed()) {
    return;
  }
  // Fire off an async request to the plugin to paint its window
  HWND hwnd = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  NS_ASSERTION(hwnd, "Expected valid hwnd value.");
  ContentParent* cp = reinterpret_cast<ContentParent*>(
      ::GetPropW(hwnd, kPluginWidgetContentParentProperty));
  if (cp && cp->CanSend()) {
    Unused << cp->SendUpdateWindow((uintptr_t)hwnd);
  }
}
#endif  // defined(XP_WIN)

static nsIDocShell* GetOpenerDocShellHelper(Element* aFrameElement) {
  // Propagate the private-browsing status of the element's parent
  // docshell to the remote docshell, via the chrome flags.
  MOZ_ASSERT(aFrameElement);
  nsPIDOMWindowOuter* win = aFrameElement->OwnerDoc()->GetWindow();
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

mozilla::ipc::IPCResult ContentParent::RecvCreateGMPService() {
  Endpoint<PGMPServiceParent> parent;
  Endpoint<PGMPServiceChild> child;

  nsresult rv;
  rv = PGMPService::CreateEndpoints(base::GetCurrentProcId(), OtherPid(),
                                    &parent, &child);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "CreateEndpoints failed");
    return IPC_FAIL_NO_REASON(this);
  }

  if (!GMPServiceParent::Create(std::move(parent))) {
    MOZ_ASSERT(false, "GMPServiceParent::Create failed");
    return IPC_FAIL_NO_REASON(this);
  }

  if (!SendInitGMPService(std::move(child))) {
    MOZ_ASSERT(false, "SendInitGMPService failed");
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvLoadPlugin(
    const uint32_t& aPluginId, nsresult* aRv, uint32_t* aRunID,
    Endpoint<PPluginModuleParent>* aEndpoint) {
  *aRv = NS_OK;
  if (!mozilla::plugins::SetupBridge(aPluginId, this, aRv, aRunID, aEndpoint)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvUngrabPointer(
    const uint32_t& aTime) {
#if !defined(MOZ_WIDGET_GTK)
  MOZ_CRASH("This message only makes sense on GTK platforms");
#else
  gdk_pointer_ungrab(aTime);
  return IPC_OK();
#endif
}

Atomic<bool, mozilla::Relaxed> sContentParentTelemetryEventEnabled(false);

static void LogAndAssertFailedPrincipalValidationInfo(nsIPrincipal* aPrincipal,
                                                      const char* aMethod) {
  // nsContentSecurityManager may also enable this same event, but that's okay
  if (!sContentParentTelemetryEventEnabled.exchange(true)) {
    sContentParentTelemetryEventEnabled = true;
    Telemetry::SetEventRecordingEnabled("security"_ns, true);
  }

  // Send Telemetry
  nsAutoCString principalScheme, principalType, spec;
  CopyableTArray<EventExtraEntry> extra(2);

  if (!aPrincipal) {
    principalType.AssignLiteral("NullPtr");
  } else if (aPrincipal->IsSystemPrincipal()) {
    principalType.AssignLiteral("SystemPrincipal");
  } else if (aPrincipal->GetIsExpandedPrincipal()) {
    principalType.AssignLiteral("ExpandedPrincipal");
  } else if (aPrincipal->GetIsContentPrincipal()) {
    principalType.AssignLiteral("ContentPrincipal");
    aPrincipal->GetSpec(spec);
    aPrincipal->GetScheme(principalScheme);

    extra.AppendElement(EventExtraEntry{"scheme"_ns, principalScheme});
  } else {
    principalType.AssignLiteral("Unknown");
  }

  extra.AppendElement(EventExtraEntry{"principalType"_ns, principalType});

  Telemetry::EventID eventType =
      Telemetry::EventID::Security_Fissionprincipals_Contentparent;
  Telemetry::RecordEvent(eventType, mozilla::Some(aMethod),
                         mozilla::Some(extra));

  // And log it
  MOZ_LOG(
      ContentParent::GetLog(), LogLevel::Error,
      ("  Receiving unexpected Principal (%s) within %s",
       aPrincipal && aPrincipal->GetIsContentPrincipal() ? spec.get()
                                                         : principalType.get(),
       aMethod));

#ifdef DEBUG
  // Not only log but also ensure we do not receive an unexpected
  // principal when running in debug mode.
  MOZ_ASSERT(false, "Receiving unexpected Principal");
#endif
}

bool ContentParent::ValidatePrincipal(
    nsIPrincipal* aPrincipal,
    const EnumSet<ValidatePrincipalOptions>& aOptions) {
  // If there is no principal, then there is nothing to validate!
  if (!aPrincipal) {
    return aOptions.contains(ValidatePrincipalOptions::AllowNullPtr);
  }

  // We currently do not track relationships between specific null principals
  // and content processes, so we can not validate much here - just allow all
  // null principals we see because they are generally safe anyway!
  if (aPrincipal->GetIsNullPrincipal()) {
    return true;
  }

  // Only allow the system principal if the passed in options flags
  // request permitting the system principal.
  if (aPrincipal->IsSystemPrincipal()) {
    return aOptions.contains(ValidatePrincipalOptions::AllowSystem);
  }

  // XXXckerschb: we should eliminate the resource carve-out here and always
  // validate the Principal, see Bug 1686200: Investigate Principal for pdf.js
  if (aPrincipal->SchemeIs("resource")) {
    return true;
  }

  // Validate each inner principal individually, allowing us to catch expanded
  // principals containing the system principal, etc.
  if (aPrincipal->GetIsExpandedPrincipal()) {
    if (!aOptions.contains(ValidatePrincipalOptions::AllowExpanded)) {
      return false;
    }
    // FIXME: There are more constraints on expanded principals in-practice,
    // such as the structure of extension expanded principals. This may need
    // to be investigated more in the future.
    nsCOMPtr<nsIExpandedPrincipal> expandedPrincipal =
        do_QueryInterface(aPrincipal);
    const auto& allowList = expandedPrincipal->AllowList();
    for (const auto& innerPrincipal : allowList) {
      if (!ValidatePrincipal(innerPrincipal, aOptions)) {
        return false;
      }
    }
    return true;
  }

  // A URI with a file:// scheme can never load in a non-file content process
  // due to sandboxing.
  if (aPrincipal->SchemeIs("file")) {
    return mRemoteType == FILE_REMOTE_TYPE;
  }

  if (aPrincipal->SchemeIs("about")) {
    uint32_t flags = 0;
    if (NS_FAILED(aPrincipal->GetAboutModuleFlags(&flags))) {
      return false;
    }

    // Block principals for about: URIs which can't load in this process.
    if (!(flags & (nsIAboutModule::URI_CAN_LOAD_IN_CHILD |
                   nsIAboutModule::URI_MUST_LOAD_IN_CHILD))) {
      return false;
    }
    if (flags & nsIAboutModule::URI_MUST_LOAD_IN_EXTENSION_PROCESS) {
      return mRemoteType == EXTENSION_REMOTE_TYPE;
    }
    return true;
  }

  if (!mRemoteTypeIsolationPrincipal ||
      RemoteTypePrefix(mRemoteType) != FISSION_WEB_REMOTE_TYPE) {
    return true;
  }

  // Web content can contain extension content frames, so a content process may
  // send us an extension's principal.
  auto* addonPolicy = BasePrincipal::Cast(aPrincipal)->AddonPolicy();
  if (addonPolicy) {
    return true;
  }

  // Ensure that the expected site-origin matches the one specified by our
  // mRemoteTypeIsolationPrincipal.
  nsAutoCString siteOriginNoSuffix;
  if (NS_FAILED(aPrincipal->GetSiteOriginNoSuffix(siteOriginNoSuffix))) {
    return false;
  }
  nsAutoCString remoteTypeSiteOriginNoSuffix;
  if (NS_FAILED(mRemoteTypeIsolationPrincipal->GetSiteOriginNoSuffix(
          remoteTypeSiteOriginNoSuffix))) {
    return false;
  }

  return remoteTypeSiteOriginNoSuffix.Equals(siteOriginNoSuffix);
}

mozilla::ipc::IPCResult ContentParent::RecvRemovePermission(
    const IPC::Principal& aPrincipal, const nsCString& aPermissionType,
    nsresult* aRv) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  *aRv = Permissions::RemovePermission(aPrincipal, aPermissionType);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvConnectPluginBridge(
    const uint32_t& aPluginId, nsresult* aRv,
    Endpoint<PPluginModuleParent>* aEndpoint) {
  *aRv = NS_OK;
  // We don't need to get the run ID for the plugin, since we already got it
  // in the first call to SetupBridge in RecvLoadPlugin, so we pass in a dummy
  // pointer and just throw it away.
  uint32_t dummy = 0;
  if (!mozilla::plugins::SetupBridge(aPluginId, this, aRv, &dummy, aEndpoint)) {
    return IPC_FAIL(this, "SetupBridge failed");
  }
  return IPC_OK();
}

/*static*/
already_AddRefed<RemoteBrowser> ContentParent::CreateBrowser(
    const TabContext& aContext, Element* aFrameElement,
    const nsACString& aRemoteType, BrowsingContext* aBrowsingContext,
    ContentParent* aOpenerContentParent) {
  AUTO_PROFILER_LABEL("ContentParent::CreateBrowser", OTHER);

  if (!sCanLaunchSubprocesses) {
    return nullptr;
  }

  nsAutoCString remoteType(aRemoteType);
  if (remoteType.IsEmpty()) {
    remoteType = DEFAULT_REMOTE_TYPE;
  }

  TabId tabId(nsContentUtils::GenerateTabId());

  nsIDocShell* docShell = GetOpenerDocShellHelper(aFrameElement);
  TabId openerTabId;
  if (docShell) {
    openerTabId = BrowserParent::GetTabIdFrom(docShell);
  }

  bool isPreloadBrowser = false;
  nsAutoString isPreloadBrowserStr;
  if (aFrameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::preloadedState,
                             isPreloadBrowserStr)) {
    isPreloadBrowser = isPreloadBrowserStr.EqualsLiteral("preloaded");
  }

  RefPtr<ContentParent> constructorSender;
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess(),
                     "Cannot allocate BrowserParent in content process");
  if (aOpenerContentParent && aOpenerContentParent->IsAlive()) {
    constructorSender = aOpenerContentParent;
  } else {
    if (aContext.IsJSPlugin()) {
      constructorSender = GetNewOrUsedJSPluginProcess(
          aContext.JSPluginId(), PROCESS_PRIORITY_FOREGROUND);
    } else {
      constructorSender = GetNewOrUsedBrowserProcess(
          remoteType, aBrowsingContext->Group(), PROCESS_PRIORITY_FOREGROUND,
          isPreloadBrowser);
    }
    if (!constructorSender) {
      return nullptr;
    }
  }

  aBrowsingContext->SetEmbedderElement(aFrameElement);

  // Ensure that the process which we're using to launch is set as the host
  // process for this BrowsingContextGroup.
  aBrowsingContext->Group()->EnsureHostProcess(constructorSender);

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
  if (loadContext && loadContext->UseRemoteTabs()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;
  }
  if (loadContext && loadContext->UseRemoteSubframes()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_FISSION_WINDOW;
  }
  if (docShell->GetAffectPrivateSessionLifetime()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }

  if (tabId == 0) {
    return nullptr;
  }

  aBrowsingContext->Canonical()->SetOwnerProcessId(
      constructorSender->ChildID());

  RefPtr<BrowserParent> browserParent =
      new BrowserParent(constructorSender, tabId, aContext,
                        aBrowsingContext->Canonical(), chromeFlags);

  // Open a remote endpoint for our PBrowser actor.
  ManagedEndpoint<PBrowserChild> childEp =
      constructorSender->OpenPBrowserEndpoint(browserParent);
  if (NS_WARN_IF(!childEp.IsValid())) {
    return nullptr;
  }

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  cpm->RegisterRemoteFrame(browserParent);

  nsCOMPtr<nsIPrincipal> initialPrincipal =
      NullPrincipal::Create(aBrowsingContext->OriginAttributesRef());
  WindowGlobalInit windowInit = WindowGlobalActor::AboutBlankInitializer(
      aBrowsingContext, initialPrincipal);

  RefPtr<WindowGlobalParent> windowParent =
      WindowGlobalParent::CreateDisconnected(windowInit);
  if (NS_WARN_IF(!windowParent)) {
    return nullptr;
  }

  // Open a remote endpoint for the initial PWindowGlobal actor.
  ManagedEndpoint<PWindowGlobalChild> windowEp =
      browserParent->OpenPWindowGlobalEndpoint(windowParent);
  if (NS_WARN_IF(!windowEp.IsValid())) {
    return nullptr;
  }

  // Tell the content process to set up its PBrowserChild.
  bool ok = constructorSender->SendConstructBrowser(
      std::move(childEp), std::move(windowEp), tabId,
      aContext.AsIPCTabContext(), windowInit, chromeFlags,
      constructorSender->ChildID(), constructorSender->IsForBrowser(),
      /* aIsTopLevel */ true);
  if (NS_WARN_IF(!ok)) {
    return nullptr;
  }

  windowParent->Init();

  RefPtr<BrowserHost> browserHost = new BrowserHost(browserParent);
  browserParent->SetOwnerElement(aFrameElement);
  return browserHost.forget();
}

void ContentParent::GetAll(nsTArray<ContentParent*>& aArray) {
  aArray.Clear();

  for (auto* cp : AllProcesses(eLive)) {
    aArray.AppendElement(cp);
  }
}

void ContentParent::GetAllEvenIfDead(nsTArray<ContentParent*>& aArray) {
  aArray.Clear();

  for (auto* cp : AllProcesses(eAll)) {
    aArray.AppendElement(cp);
  }
}

void ContentParent::BroadcastStringBundle(
    const StringBundleDescriptor& aBundle) {
  AutoTArray<StringBundleDescriptor, 1> array;
  array.AppendElement(aBundle);

  for (auto* cp : AllProcesses(eLive)) {
    Unused << cp->SendRegisterStringBundles(array);
  }
}

void ContentParent::BroadcastFontListChanged() {
  for (auto* cp : AllProcesses(eLive)) {
    Unused << cp->SendFontListChanged();
  }
}

static LookAndFeelData GetLookAndFeelData() {
  if (StaticPrefs::widget_remote_look_and_feel_AtStartup()) {
    return *RemoteLookAndFeel::ExtractData();
  }
  return LookAndFeel::GetCache();
}

void ContentParent::BroadcastThemeUpdate(widget::ThemeChangeKind aKind) {
  LookAndFeelData lnfData = GetLookAndFeelData();

  for (auto* cp : AllProcesses(eLive)) {
    Unused << cp->SendThemeChanged(lnfData, aKind);
  }
}

/*static */
void ContentParent::BroadcastMediaCodecsSupportedUpdate(
    RemoteDecodeIn aLocation,
    const PDMFactory::MediaCodecsSupported& aSupported) {
  sCodecsSupported[aLocation] = aSupported;
  for (auto* cp : AllProcesses(eAll)) {
    Unused << cp->SendUpdateMediaCodecsSupported(aLocation, aSupported);
  }
}

const nsACString& ContentParent::GetRemoteType() const { return mRemoteType; }

void ContentParent::Init() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    size_t length = ArrayLength(sObserverTopics);
    for (size_t i = 0; i < length; ++i) {
      obs->AddObserver(this, sObserverTopics[i], false);
    }
  }

  AddShutdownBlockers();

  if (obs) {
    nsAutoString cpId;
    cpId.AppendInt(static_cast<uint64_t>(this->ChildID()));
    obs->NotifyObservers(static_cast<nsIObserver*>(this), "ipc:content-created",
                         cpId.get());
  }

#ifdef ACCESSIBILITY
  // If accessibility is running in chrome process then start it in content
  // process.
  if (PresShell::IsAccessibilityActive()) {
#  if defined(XP_WIN)
    // Don't init content a11y if we detect an incompat version of JAWS in use.
    if (!mozilla::a11y::Compatibility::IsOldJAWS()) {
      Unused << SendActivateA11y(
          ::GetCurrentThreadId(),
          a11y::AccessibleWrap::GetContentProcessIdFor(ChildID()));
    }
#  else
    Unused << SendActivateA11y(0, 0);
#  endif
  }
#endif  // #ifdef ACCESSIBILITY

#ifdef MOZ_GECKO_PROFILER
  Unused << SendInitProfiler(ProfilerParent::CreateForProcess(OtherPid()));
#endif

  // Ensure that the default set of permissions are avaliable in the content
  // process before we try to load any URIs in it.
  EnsurePermissionsByKey(""_ns, ""_ns);

  RefPtr<GeckoMediaPluginServiceParent> gmps(
      GeckoMediaPluginServiceParent::GetSingleton());
  gmps->UpdateContentProcessGMPCapabilities();

  // Flush any pref updates that happened during launch and weren't
  // included in the blobs set up in BeginSubprocessLaunch.
  for (const Pref& pref : mQueuedPrefs) {
    Unused << NS_WARN_IF(!SendPreferenceUpdate(pref));
  }
  mQueuedPrefs.Clear();
}

void ContentParent::MaybeBeginShutDown(uint32_t aExpectedBrowserCount,
                                       bool aSendShutDown) {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Verbose,
          ("MaybeBeginShutdown %p", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (ManagedPBrowserParent().Count() != aExpectedBrowserCount ||
      ShouldKeepProcessAlive() || TryToRecycle()) {
    return;
  }

  MOZ_LOG(
      ContentParent::GetLog(), LogLevel::Debug,
      ("Beginning ContentParent Shutdown %p (%s)", this, mRemoteType.get()));

  // We're dying now, prevent anything from re-using this process.
  MarkAsDead();
  StartForceKillTimer();

  if (aSendShutDown) {
    MaybeAsyncSendShutDownMessage();
  }
}

void ContentParent::MaybeAsyncSendShutDownMessage() {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Verbose,
          ("MaybeAsyncSendShutDownMessage %p", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sRecycledE10SProcess != this);

#ifdef DEBUG
  // Calling this below while the lock is acquired will deadlock.
  bool shouldKeepProcessAlive = ShouldKeepProcessAlive();
#endif

  auto lock = mRemoteWorkerActorData.Lock();
  MOZ_ASSERT_IF(!lock->mCount, !shouldKeepProcessAlive);

  if (lock->mCount) {
    return;
  }

  MOZ_ASSERT(!lock->mShutdownStarted);
  lock->mShutdownStarted = true;

  // In the case of normal shutdown, send a shutdown message to child to
  // allow it to perform shutdown tasks.
  GetCurrentSerialEventTarget()->Dispatch(NewRunnableMethod<ShutDownMethod>(
      "dom::ContentParent::ShutDownProcess", this,
      &ContentParent::ShutDownProcess, SEND_SHUTDOWN_MESSAGE));
}

void ContentParent::ShutDownProcess(ShutDownMethod aMethod) {
  // NB: must MarkAsDead() here so that this isn't accidentally
  // returned from Get*() while in the midst of shutdown.
  MarkAsDead();

  // Shutting down by sending a shutdown message works differently than the
  // other methods. We first call Shutdown() in the child. After the child is
  // ready, it calls FinishShutdown() on us. Then we close the channel.
  if (aMethod == SEND_SHUTDOWN_MESSAGE) {
    if (CanSend() && !mShutdownPending) {
      // Stop sending input events with input priority when shutting down.
      SetInputPriorityEventEnabled(false);
      if (SendShutdown()) {
        mShutdownPending = true;
        // Start the force-kill timer if we haven't already.
        StartForceKillTimer();
      }
    }
    // If call was not successful, the channel must have been broken
    // somehow, and we will clean up the error in ActorDestroy.
    return;
  }

  using mozilla::dom::quota::QuotaManagerService;

  if (QuotaManagerService* qms = QuotaManagerService::GetOrCreate()) {
    qms->AbortOperationsForProcess(mChildID);
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
        static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(
            iter.Get()->GetKey());
    ocuParent->StopSendingMessagesToChild();
  }

  // A ContentParent object might not get freed until after XPCOM shutdown has
  // shut down the cycle collector.  But by then it's too late to release any
  // CC'ed objects, so we need to null them out here, while we still can.  See
  // bug 899761.
  ShutDownMessageManager();
}

mozilla::ipc::IPCResult ContentParent::RecvFinishShutdown() {
  // At this point, we already called ShutDownProcess once with
  // SEND_SHUTDOWN_MESSAGE. To actually close the channel, we call
  // ShutDownProcess again with CLOSE_CHANNEL.
  MOZ_ASSERT(mShutdownPending);
  ShutDownProcess(CLOSE_CHANNEL);
  return IPC_OK();
}

void ContentParent::ShutDownMessageManager() {
  if (!mMessageManager) {
    return;
  }

  mMessageManager->ReceiveMessage(mMessageManager, nullptr,
                                  CHILD_PROCESS_SHUTDOWN_MESSAGE, false,
                                  nullptr, nullptr, IgnoreErrors());

  mMessageManager->SetOsPid(-1);
  mMessageManager->Disconnect();
  mMessageManager = nullptr;
}

void ContentParent::AddToPool(nsTArray<ContentParent*>& aPool) {
  MOZ_DIAGNOSTIC_ASSERT(!mIsInPool);
  AssertAlive();
  MOZ_DIAGNOSTIC_ASSERT(!mCalledKillHard);
  aPool.AppendElement(this);
  mIsInPool = true;
}

void ContentParent::RemoveFromPool(nsTArray<ContentParent*>& aPool) {
  MOZ_DIAGNOSTIC_ASSERT(mIsInPool);
  aPool.RemoveElement(this);
  mIsInPool = false;
}

void ContentParent::AssertNotInPool() {
  MOZ_RELEASE_ASSERT(!mIsInPool);

  MOZ_RELEASE_ASSERT(!sPrivateContent || !sPrivateContent->Contains(this));
  MOZ_RELEASE_ASSERT(sRecycledE10SProcess != this);
  if (IsForJSPlugin()) {
    MOZ_RELEASE_ASSERT(!sJSPluginContentParents ||
                       !sJSPluginContentParents->Get(mJSPluginID));
  } else {
    MOZ_RELEASE_ASSERT(
        !sBrowserContentParents ||
        !sBrowserContentParents->Contains(mRemoteType) ||
        !sBrowserContentParents->Get(mRemoteType)->Contains(this) ||
        !sCanLaunchSubprocesses);  // aka in shutdown - avoid timing issues

    for (auto& group : mGroups) {
      MOZ_RELEASE_ASSERT(group.GetKey()->GetHostProcess(mRemoteType) != this,
                         "still a host process for one of our groups?");
    }
  }
}

void ContentParent::AssertAlive() { MOZ_DIAGNOSTIC_ASSERT(!IsDead()); }

void ContentParent::RemoveFromList() {
  if (IsForJSPlugin()) {
    if (sJSPluginContentParents) {
      sJSPluginContentParents->Remove(mJSPluginID);
      if (!sJSPluginContentParents->Count()) {
        sJSPluginContentParents = nullptr;
      }
    }
    return;
  }

  if (sPrivateContent) {
    sPrivateContent->RemoveElement(this);
    if (!sPrivateContent->Length()) {
      sPrivateContent = nullptr;
    }
  }

  if (!mIsInPool) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    AssertNotInPool();
#endif
    return;
  }

  // Ensure that this BrowsingContextGroup is no longer used to host new
  // documents from any associated BrowsingContextGroups. It may become a host
  // again in the future, if it is restored to the pool.
  for (auto& group : mGroups) {
    group.GetKey()->RemoveHostProcess(this);
  }

  StopRecycling(/* aForeground */ false);

  if (sBrowserContentParents) {
    if (auto entry = sBrowserContentParents->Lookup(mRemoteType)) {
      const auto& contentParents = entry.Data();
      RemoveFromPool(*contentParents);
      if (contentParents->IsEmpty()) {
        entry.Remove();
      }
    }
    if (sBrowserContentParents->IsEmpty()) {
      delete sBrowserContentParents;
      sBrowserContentParents = nullptr;
    }
  }
}

void ContentParent::MarkAsDead() {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Verbose,
          ("Marking ContentProcess %p as dead", this));
  MOZ_DIAGNOSTIC_ASSERT(!sInProcessSelector);
  RemoveFromList();

  PreallocatedProcessManager::Erase(this);

#ifdef MOZ_WIDGET_ANDROID
  if (IsAlive()) {
    // We're intentionally killing the content process at this point to ensure
    // that we never have a "dead" content process sitting around and occupying
    // an Android Service.
    nsCOMPtr<nsIEventTarget> launcherThread(GetIPCLauncher());
    MOZ_ASSERT(launcherThread);

    auto procType = java::GeckoProcessType::CONTENT();
    auto selector =
        java::GeckoProcessManager::Selector::New(procType, OtherPid());

    launcherThread->Dispatch(NS_NewRunnableFunction(
        "ContentParent::MarkAsDead",
        [selector =
             java::GeckoProcessManager::Selector::GlobalRef(selector)]() {
          java::GeckoProcessManager::ShutdownProcess(selector);
        }));
  }
#endif

  if (mScriptableHelper) {
    static_cast<ScriptableCPInfo*>(mScriptableHelper.get())->ProcessDied();
    mScriptableHelper = nullptr;
  }

  mLifecycleState = LifecycleState::DEAD;
}

void ContentParent::OnChannelError() {
  RefPtr<ContentParent> kungFuDeathGrip(this);
  PContentParent::OnChannelError();
}

void ContentParent::OnChannelConnected(int32_t pid) {
  MOZ_ASSERT(NS_IsMainThread());

  SetOtherProcessId(pid);
}

void ContentParent::ProcessingError(Result aCode, const char* aReason) {
  if (MsgDropped == aCode) {
    return;
  }
#ifndef FUZZING
  // Other errors are big deals.
  KillHard(aReason);
#endif
}

void ContentParent::ActorDestroy(ActorDestroyReason why) {
  MOZ_RELEASE_ASSERT(mSelfRef);

  if (mForceKillTimer) {
    mForceKillTimer->Cancel();
    mForceKillTimer = nullptr;
  }

  // Signal shutdown completion regardless of error state, so we can
  // finish waiting in the xpcom-shutdown/profile-before-change observer.
  RemoveShutdownBlockers();

  if (mHangMonitorActor) {
    ProcessHangMonitor::RemoveProcess(mHangMonitorActor);
    mHangMonitorActor = nullptr;
  }

  RefPtr<FileSystemSecurity> fss = FileSystemSecurity::Get();
  if (fss) {
    fss->Forget(ChildID());
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

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    size_t length = ArrayLength(sObserverTopics);
    for (size_t i = 0; i < length; ++i) {
      obs->RemoveObserver(static_cast<nsIObserver*>(this), sObserverTopics[i]);
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

  // Destroy our JSProcessActors, and reject any pending queries.
  JSActorDidDestroy();

  if (obs) {
    RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

    props->SetPropertyAsUint64(u"childID"_ns, mChildID);

    if (AbnormalShutdown == why) {
      Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT, "content"_ns,
                            1);

      props->SetPropertyAsBool(u"abnormal"_ns, true);

      nsAutoString dumpID;
      // There's a window in which child processes can crash
      // after IPC is established, but before a crash reporter
      // is created.
      if (mCrashReporter) {
        // if mCreatedPairedMinidumps is true, we've already generated
        // parent/child dumps for desktop crashes.
        if (!mCreatedPairedMinidumps) {
          if (mCrashReporter->GenerateCrashReport(OtherPid())) {
            // Propagate `isLikelyOOM`.
            Unused << props->SetPropertyAsBool(u"isLikelyOOM"_ns,
                                               mCrashReporter->IsLikelyOOM());
          }
        }

        if (mCrashReporter->HasMinidump()) {
          dumpID = mCrashReporter->MinidumpID();
        }
      } else {
        HandleOrphanedMinidump(&dumpID);
      }

      if (!dumpID.IsEmpty()) {
        props->SetPropertyAsAString(u"dumpID"_ns, dumpID);
      }
    }
    nsAutoString cpId;
    cpId.AppendInt(static_cast<uint64_t>(this->ChildID()));
    obs->NotifyObservers((nsIPropertyBag2*)props, "ipc:content-shutdown",
                         cpId.get());
  }

  // Remove any and all idle listeners.
  nsCOMPtr<nsIUserIdleService> idleService =
      do_GetService("@mozilla.org/widget/useridleservice;1");
  MOZ_ASSERT(idleService);
  RefPtr<ParentIdleListener> listener;
  for (int32_t i = mIdleListeners.Length() - 1; i >= 0; --i) {
    listener = static_cast<ParentIdleListener*>(mIdleListeners[i].get());
    idleService->RemoveIdleObserver(listener, listener->mTime);
  }
  mIdleListeners.Clear();

  MOZ_LOG(ContentParent::GetLog(), LogLevel::Verbose,
          ("destroying Subprocess in ActorDestroy: ContentParent %p "
           "mSubprocess %p handle %" PRIuPTR,
           this, mSubprocess,
           mSubprocess ? (uintptr_t)mSubprocess->GetChildProcessHandle() : -1));
  // FIXME (bug 1520997): does this really need an additional dispatch?
  GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "DelayedDeleteSubprocessRunnable", [subprocess = mSubprocess] {
        MOZ_LOG(
            ContentParent::GetLog(), LogLevel::Debug,
            ("destroyed Subprocess in ActorDestroy: Subprocess %p handle "
             "%" PRIuPTR,
             subprocess,
             subprocess ? (uintptr_t)subprocess->GetChildProcessHandle() : -1));
        subprocess->Destroy();
      }));
  mSubprocess = nullptr;

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  cpm->RemoveContentProcess(this->ChildID());

  if (mDriverCrashGuard) {
    mDriverCrashGuard->NotifyCrashed();
  }

  // Unregister all the BlobURLs registered by the ContentChild.
  for (uint32_t i = 0; i < mBlobURLs.Length(); ++i) {
    BlobURLProtocolHandler::RemoveDataEntry(mBlobURLs[i]);
  }

  mBlobURLs.Clear();

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  a11y::AccessibleWrap::ReleaseContentProcessIdFor(ChildID());
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  AssertNotInPool();
#endif

  // As this process is going away, ensure that every BrowsingContext hosted by
  // it has been detached, and every BrowsingContextGroup has been fully
  // unsubscribed.
  BrowsingContext::DiscardFromContentParent(this);

  nsTHashtable<nsRefPtrHashKey<BrowsingContextGroup>> groups;
  mGroups.SwapElements(groups);
  for (auto& group : groups) {
    group.GetKey()->Unsubscribe(this);
  }
  MOZ_DIAGNOSTIC_ASSERT(mGroups.IsEmpty());
}

void ContentParent::ActorDealloc() { mSelfRef = nullptr; }

bool ContentParent::TryToRecycle() {
  // Only try to recycle "web" content processes, as other remote types are
  // generally more unique, and cannot be effectively re-used. This is disabled
  // with Fission, as "web" content processes are no longer frequently used.
  //
  // Disabling the process pre-allocator will also disable process recycling,
  // allowing for more consistent process counts under testing.
  if (mRemoteType != DEFAULT_REMOTE_TYPE || mozilla::FissionAutostart() ||
      !PreallocatedProcessManager::Enabled()) {
    return false;
  }

  // This life time check should be replaced by a memory health check (memory
  // usage + fragmentation).

  // Note that this is specifically to help with edge cases that rapidly
  // create-and-destroy processes
  const double kMaxLifeSpan = 5;
  MOZ_LOG(
      ContentParent::GetLog(), LogLevel::Debug,
      ("TryToRecycle ContentProcess %p (%u) with lifespan %f seconds", this,
       (unsigned int)ChildID(), (TimeStamp::Now() - mActivateTS).ToSeconds()));

  if (mCalledKillHard || !IsAlive() ||
      (TimeStamp::Now() - mActivateTS).ToSeconds() > kMaxLifeSpan) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("TryToRecycle did not recycle %p", this));

    // It's possible that the process was already cached, and we're being called
    // from a different path, and we're now past kMaxLifeSpan (or some other).
    // Ensure that if we're going to kill this process we don't recycle it.
    StopRecycling(/* aForeground */ false);
    return false;
  }

  if (!sRecycledE10SProcess) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("TryToRecycle began recycling %p", this));
    sRecycledE10SProcess = this;

    ProcessPriorityManager::SetProcessPriority(this,
                                               PROCESS_PRIORITY_BACKGROUND);
    return true;
  }

  if (sRecycledE10SProcess == this) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("TryToRecycle continue recycling %p", this));
    return true;
  }

  // Some other process is already being recycled, just shut this one down.
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("TryToRecycle did not recycle %p (already recycling %p)", this,
           sRecycledE10SProcess.get()));
  return false;
}

void ContentParent::StopRecycling(bool aForeground) {
  if (sRecycledE10SProcess != this) {
    return;
  }

  sRecycledE10SProcess = nullptr;
  if (aForeground) {
    ProcessPriorityManager::SetProcessPriority(this,
                                               PROCESS_PRIORITY_FOREGROUND);
  }
}

bool ContentParent::HasActiveWorkerOrJSPlugin() {
  if (IsForJSPlugin()) {
    return true;
  }

  // If we have active workers, we need to stay alive.
  {
    const auto lock = mRemoteWorkerActorData.Lock();
    if (lock->mCount) {
      return true;
    }
  }
  return false;
}

bool ContentParent::ShouldKeepProcessAlive() {
  if (HasActiveWorkerOrJSPlugin()) {
    return true;
  }

  if (mNumKeepaliveCalls > 0) {
    return true;
  }

  if (IsLaunching()) {
    return true;
  }

  // If we have already been marked as dead, don't prevent shutdown.
  if (IsDead()) {
    return false;
  }

  if (!sBrowserContentParents) {
    return false;
  }

  auto contentParents = sBrowserContentParents->Get(mRemoteType);
  if (!contentParents) {
    return false;
  }

  // We might want to keep some content processes alive for performance reasons.
  // e.g. test runs and privileged content process for some about: pages.
  // We don't want to alter behavior if the pref is not set, so default to 0.
  int32_t processesToKeepAlive = 0;

  nsAutoCString keepAlivePref("dom.ipc.keepProcessesAlive.");

  if (StringBeginsWith(mRemoteType, FISSION_WEB_REMOTE_TYPE) &&
      xpc::IsInAutomation()) {
    keepAlivePref.Append(FISSION_WEB_REMOTE_TYPE);
    keepAlivePref.AppendLiteral(".perOrigin");
  } else {
    keepAlivePref.Append(mRemoteType);
  }
  if (NS_FAILED(
          Preferences::GetInt(keepAlivePref.get(), &processesToKeepAlive))) {
    return false;
  }

  int32_t numberOfAliveProcesses = contentParents->Length();

  return numberOfAliveProcesses <= processesToKeepAlive;
}

void ContentParent::NotifyTabDestroying() {
  // There can be more than one PBrowser for a given app process
  // because of popup windows.  PBrowsers can also destroy
  // concurrently.  When all the PBrowsers are destroying, kick off
  // another task to ensure the child process *really* shuts down,
  // even if the PBrowsers themselves never finish destroying.
  ++mNumDestroyingTabs;

  /**
   * We intentionally skip this code on Android:
   * 1. Android has a fixed upper bound on the number of content processes, so
   *    we prefer to re-use them whenever possible (as opposed to letting an
   *    old process wind down while we launch a new one).
   * 2. GeckoView always hard-kills content processes (and if it does not,
   *    Android itself will), so we don't concern ourselves with the ForceKill
   *    timer either.
   */
#if !defined(MOZ_WIDGET_ANDROID)
  MaybeBeginShutDown(/* aExpectedBrowserCount */ mNumDestroyingTabs,
                     /* aSendShutDown */ false);
#endif  // !defined(MOZ_WIDGET_ANDROID)
}

void ContentParent::AddKeepAlive() {
  // Something wants to keep this content process alive.
  ++mNumKeepaliveCalls;
}

void ContentParent::RemoveKeepAlive() {
  MOZ_DIAGNOSTIC_ASSERT(mNumKeepaliveCalls > 0);
  --mNumKeepaliveCalls;

  MaybeBeginShutDown();
}

void ContentParent::StartForceKillTimer() {
  if (mForceKillTimer || !CanSend()) {
    return;
  }

  NotifyImpendingShutdown();

  int32_t timeoutSecs = StaticPrefs::dom_ipc_tabs_shutdownTimeoutSecs();
  if (timeoutSecs > 0) {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mForceKillTimer),
                                ContentParent::ForceKillTimerCallback, this,
                                timeoutSecs * 1000, nsITimer::TYPE_ONE_SHOT,
                                "dom::ContentParent::StartForceKillTimer");
    MOZ_ASSERT(mForceKillTimer);
  }
}

void ContentParent::NotifyTabDestroyed(const TabId& aTabId,
                                       bool aNotifiedDestroying) {
  if (aNotifiedDestroying) {
    --mNumDestroyingTabs;
  }

  nsTArray<PContentPermissionRequestParent*> parentArray =
      nsContentPermissionUtils::GetContentPermissionRequestParentById(aTabId);

  // Need to close undeleted ContentPermissionRequestParents before tab is
  // closed.
  for (auto& permissionRequestParent : parentArray) {
    Unused << PContentPermissionRequestParent::Send__delete__(
        permissionRequestParent);
  }

  // There can be more than one PBrowser for a given app process
  // because of popup windows.  When the last one closes, shut
  // us down.
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Verbose,
          ("NotifyTabDestroyed %p", this));

  MaybeBeginShutDown(/* aExpectedBrowserCount */ 1);
}

TestShellParent* ContentParent::CreateTestShell() {
  return static_cast<TestShellParent*>(SendPTestShellConstructor());
}

bool ContentParent::DestroyTestShell(TestShellParent* aTestShell) {
  return PTestShellParent::Send__delete__(aTestShell);
}

TestShellParent* ContentParent::GetTestShellSingleton() {
  PTestShellParent* p = LoneManagedOrNullAsserts(ManagedPTestShellParent());
  return static_cast<TestShellParent*>(p);
}

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
// Append the sandbox command line parameters that are not static. i.e.,
// parameters that can be different for different child processes.
void ContentParent::AppendDynamicSandboxParams(
    std::vector<std::string>& aArgs) {
  // For file content processes
  if (GetRemoteType() == FILE_REMOTE_TYPE) {
    MacSandboxInfo::AppendFileAccessParam(aArgs, true);
  }
}

// Generate the static sandbox command line parameters and store
// them in the provided params vector to be used each time a new
// content process is launched.
static void CacheSandboxParams(std::vector<std::string>& aCachedParams) {
  // This must only be called once and we should
  // be starting with an empty list of parameters.
  MOZ_ASSERT(aCachedParams.empty());

  MacSandboxInfo info;
  info.type = MacSandboxType_Content;
  info.level = GetEffectiveContentSandboxLevel();

  // Sandbox logging
  if (Preferences::GetBool("security.sandbox.logging.enabled") ||
      PR_GetEnv("MOZ_SANDBOX_LOGGING")) {
    info.shouldLog = true;
  }

  // Audio access
  if (!StaticPrefs::media_cubeb_sandbox()) {
    info.hasAudio = true;
  }

  // Windowserver access
  if (!Preferences::GetBool(
          "security.sandbox.content.mac.disconnect-windowserver")) {
    info.hasWindowServer = true;
  }

  // .app path (normalized)
  nsAutoCString appPath;
  if (!nsMacUtilsImpl::GetAppPath(appPath)) {
    MOZ_CRASH("Failed to get app dir paths");
  }
  info.appPath = appPath.get();

  // TESTING_READ_PATH1
  nsAutoCString testingReadPath1;
  Preferences::GetCString("security.sandbox.content.mac.testing_read_path1",
                          testingReadPath1);
  if (!testingReadPath1.IsEmpty()) {
    info.testingReadPath1 = testingReadPath1.get();
  }

  // TESTING_READ_PATH2
  nsAutoCString testingReadPath2;
  Preferences::GetCString("security.sandbox.content.mac.testing_read_path2",
                          testingReadPath2);
  if (!testingReadPath2.IsEmpty()) {
    info.testingReadPath2 = testingReadPath2.get();
  }

  // TESTING_READ_PATH3, TESTING_READ_PATH4. In development builds,
  // these are used to whitelist the repo dir and object dir respectively.
  nsresult rv;
  if (mozilla::IsDevelopmentBuild()) {
    // Repo dir
    nsCOMPtr<nsIFile> repoDir;
    rv = nsMacUtilsImpl::GetRepoDir(getter_AddRefs(repoDir));
    if (NS_FAILED(rv)) {
      MOZ_CRASH("Failed to get path to repo dir");
    }
    nsCString repoDirPath;
    Unused << repoDir->GetNativePath(repoDirPath);
    info.testingReadPath3 = repoDirPath.get();

    // Object dir
    nsCOMPtr<nsIFile> objDir;
    rv = nsMacUtilsImpl::GetObjDir(getter_AddRefs(objDir));
    if (NS_FAILED(rv)) {
      MOZ_CRASH("Failed to get path to build object dir");
    }
    nsCString objDirPath;
    Unused << objDir->GetNativePath(objDirPath);
    info.testingReadPath4 = objDirPath.get();
  }

  // DEBUG_WRITE_DIR
#  ifdef DEBUG
  // For bloat/leak logging or when a content process dies intentionally
  // (|NoteIntentionalCrash|) for tests, it wants to log that it did this.
  // Allow writing to this location.
  nsAutoCString bloatLogDirPath;
  if (NS_SUCCEEDED(nsMacUtilsImpl::GetBloatLogDir(bloatLogDirPath))) {
    info.debugWriteDir = bloatLogDirPath.get();
  }
#  endif  // DEBUG

  info.AppendAsParams(aCachedParams);
}

// Append sandboxing command line parameters.
void ContentParent::AppendSandboxParams(std::vector<std::string>& aArgs) {
  MOZ_ASSERT(sMacSandboxParams != nullptr);

  // An empty sMacSandboxParams indicates this is the
  // first invocation and we don't have cached params yet.
  if (sMacSandboxParams->empty()) {
    CacheSandboxParams(*sMacSandboxParams);
    MOZ_ASSERT(!sMacSandboxParams->empty());
  }

  // Append cached arguments.
  aArgs.insert(aArgs.end(), sMacSandboxParams->begin(),
               sMacSandboxParams->end());

  // Append remaining arguments.
  AppendDynamicSandboxParams(aArgs);
}
#endif  // XP_MACOSX && MOZ_SANDBOX

bool ContentParent::BeginSubprocessLaunch(ProcessPriority aPriority) {
  AUTO_PROFILER_LABEL("ContentParent::LaunchSubprocess", OTHER);

  if (!ContentProcessManager::GetSingleton()) {
    // Shutdown has begun, we shouldn't spawn any more child processes.
    return false;
  }

  std::vector<std::string> extraArgs;
  extraArgs.push_back("-childID");
  char idStr[21];
  SprintfLiteral(idStr, "%" PRId64, static_cast<uint64_t>(mChildID));
  extraArgs.push_back(idStr);
  extraArgs.push_back(IsForBrowser() ? "-isForBrowser" : "-notForBrowser");

  // Prefs information is passed via anonymous shared memory to avoid bloating
  // the command line.

  // Instantiate the pref serializer. It will be cleaned up in
  // `LaunchSubprocessReject`/`LaunchSubprocessResolve`.
  mPrefSerializer = MakeUnique<mozilla::ipc::SharedPreferenceSerializer>();
  if (!mPrefSerializer->SerializeToSharedMemory()) {
    MarkAsDead();
    return false;
  }
  mPrefSerializer->AddSharedPrefCmdLineArgs(*mSubprocess, extraArgs);

  // Register ContentParent as an observer for changes to any pref
  // whose prefix matches the empty string, i.e. all of them.  The
  // observation starts here in order to capture pref updates that
  // happen during async launch.
  Preferences::AddStrongObserver(this, "");

  if (gSafeMode) {
    extraArgs.push_back("-safeMode");
  }

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  if (IsContentSandboxEnabled()) {
    AppendSandboxParams(extraArgs);
    mSubprocess->DisableOSActivityMode();
  }
#endif

  nsCString parentBuildID(mozilla::PlatformBuildID());
  extraArgs.push_back("-parentBuildID");
  extraArgs.push_back(parentBuildID.get());

  // See also ActorDealloc.
  mSelfRef = this;
  mLaunchYieldTS = TimeStamp::Now();
  return mSubprocess->AsyncLaunch(std::move(extraArgs));
}

void ContentParent::LaunchSubprocessReject() {
  NS_ERROR("failed to launch child in the parent");
  // Now that communication with the child is complete, we can cleanup
  // the preference serializer.
  mPrefSerializer = nullptr;
  if (mIsAPreallocBlocker) {
    PreallocatedProcessManager::RemoveBlocker(mRemoteType, this);
    mIsAPreallocBlocker = false;
  }
  MarkAsDead();
}

bool ContentParent::LaunchSubprocessResolve(bool aIsSync,
                                            ProcessPriority aPriority) {
  AUTO_PROFILER_LABEL("ContentParent::LaunchSubprocess::resolve", OTHER);

  // Take the pending IPC channel. This channel will be used to open the raw IPC
  // connection between this process and the launched content process.
  UniquePtr<IPC::Channel> channel = mSubprocess->TakeChannel();
  if (!channel) {
    // We don't have a channel, so this method must've been called already.
    MOZ_ASSERT(sCreatedFirstContentProcess);
    MOZ_ASSERT(!mPrefSerializer);
    MOZ_ASSERT(mLifecycleState != LifecycleState::LAUNCHING);
    return true;
  }

  // Now that communication with the child is complete, we can cleanup
  // the preference serializer.
  mPrefSerializer = nullptr;

  const auto launchResumeTS = TimeStamp::Now();
#ifdef MOZ_GECKO_PROFILER
  if (profiler_thread_is_being_profiled()) {
    nsPrintfCString marker("Process start%s for %u",
                           mIsAPreallocBlocker ? " (immediate)" : "",
                           (unsigned int)ChildID());
    PROFILER_MARKER_TEXT(
        mIsAPreallocBlocker ? ProfilerString8View("Process Immediate Launch")
                            : ProfilerString8View("Process Launch"),
        DOM, MarkerTiming::Interval(mLaunchTS, launchResumeTS), marker);
  }
#endif

  if (!sCreatedFirstContentProcess) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(nullptr, "ipc:first-content-process-created", nullptr);
    sCreatedFirstContentProcess = true;
  }

  base::ProcessId procId =
      base::GetProcId(mSubprocess->GetChildProcessHandle());
  Open(std::move(channel), procId);

  ContentProcessManager::GetSingleton()->AddContentProcess(this);

#ifdef MOZ_CODE_COVERAGE
  Unused << SendShareCodeCoverageMutex(
      CodeCoverageHandler::Get()->GetMutexHandle(procId));
#endif

  // We must be in the LAUNCHING state still. If we've somehow already been
  // marked as DEAD, fail the process launch, and immediately begin tearing down
  // the content process.
  if (IsDead()) {
    ShutDownProcess(SEND_SHUTDOWN_MESSAGE);
    return false;
  }
  MOZ_ASSERT(mLifecycleState == LifecycleState::LAUNCHING);
  mLifecycleState = LifecycleState::ALIVE;

  if (!InitInternal(aPriority)) {
    NS_WARNING("failed to initialize child in the parent");
    // We've already called Open() by this point, so we need to close the
    // channel to avoid leaking the process.
    ShutDownProcess(SEND_SHUTDOWN_MESSAGE);
    return false;
  }

  mHangMonitorActor = ProcessHangMonitor::AddProcess(this);

  // Set a reply timeout for CPOWs.
  SetReplyTimeoutMs(StaticPrefs::dom_ipc_cpow_timeout());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    nsAutoString cpId;
    cpId.AppendInt(static_cast<uint64_t>(this->ChildID()));
    obs->NotifyObservers(static_cast<nsIObserver*>(this),
                         "ipc:content-initializing", cpId.get());
  }

  Init();

  mLifecycleState = LifecycleState::INITIALIZED;

  if (aIsSync) {
    Telemetry::AccumulateTimeDelta(Telemetry::CONTENT_PROCESS_SYNC_LAUNCH_MS,
                                   mLaunchTS);
  } else {
    Telemetry::AccumulateTimeDelta(Telemetry::CONTENT_PROCESS_LAUNCH_TOTAL_MS,
                                   mLaunchTS);

    Telemetry::Accumulate(
        Telemetry::CONTENT_PROCESS_LAUNCH_MAINTHREAD_MS,
        static_cast<uint32_t>(
            ((mLaunchYieldTS - mLaunchTS) + (TimeStamp::Now() - launchResumeTS))
                .ToMilliseconds()));
  }
  return true;
}

bool ContentParent::LaunchSubprocessSync(
    hal::ProcessPriority aInitialPriority) {
  // We've started a sync content process launch.
  Telemetry::Accumulate(Telemetry::CONTENT_PROCESS_LAUNCH_IS_SYNC, 1);

  if (!BeginSubprocessLaunch(aInitialPriority)) {
    return false;
  }
  const bool ok = mSubprocess->WaitForProcessHandle();
  if (ok && LaunchSubprocessResolve(/* aIsSync = */ true, aInitialPriority)) {
    ContentParent::DidLaunchSubprocess();
    return true;
  }
  LaunchSubprocessReject();
  return false;
}

RefPtr<ContentParent::LaunchPromise> ContentParent::LaunchSubprocessAsync(
    hal::ProcessPriority aInitialPriority) {
  // We've started an async content process launch.
  Telemetry::Accumulate(Telemetry::CONTENT_PROCESS_LAUNCH_IS_SYNC, 0);

  if (!BeginSubprocessLaunch(aInitialPriority)) {
    // Launch aborted because of shutdown. Bailout.
    LaunchSubprocessReject();
    return LaunchPromise::CreateAndReject(LaunchError(), __func__);
  }

  // Otherwise, wait until the process is ready.
  RefPtr<ProcessHandlePromise> ready = mSubprocess->WhenProcessHandleReady();
  RefPtr<ContentParent> self = this;
  mLaunchYieldTS = TimeStamp::Now();

  return ready->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self, aInitialPriority](
          const ProcessHandlePromise::ResolveOrRejectValue& aValue) {
        if (aValue.IsResolve() &&
            self->LaunchSubprocessResolve(/* aIsSync = */ false,
                                          aInitialPriority)) {
          ContentParent::DidLaunchSubprocess();
          return LaunchPromise::CreateAndResolve(self, __func__);
        }
        self->LaunchSubprocessReject();
        return LaunchPromise::CreateAndReject(LaunchError(), __func__);
      });
}

ContentParent::ContentParent(const nsACString& aRemoteType, int32_t aJSPluginID)
    : mSelfRef(nullptr),
      mSubprocess(nullptr),
      mLaunchTS(TimeStamp::Now()),
      mLaunchYieldTS(mLaunchTS),
      mActivateTS(mLaunchTS),
      mIsAPreallocBlocker(false),
      mRemoteType(aRemoteType),
      mChildID(gContentChildID++),
      mGeolocationWatchID(-1),
      mJSPluginID(aJSPluginID),
      mRemoteWorkerActorData("ContentParent::mRemoteWorkerActorData"),
      mNumDestroyingTabs(0),
      mNumKeepaliveCalls(0),
      mLifecycleState(LifecycleState::LAUNCHING),
      mIsForBrowser(!mRemoteType.IsEmpty()),
      mCalledClose(false),
      mCalledKillHard(false),
      mCreatedPairedMinidumps(false),
      mShutdownPending(false),
      mIsRemoteInputEventQueueEnabled(false),
      mIsInputPriorityEventEnabled(false),
      mIsInPool(false),
      mHangMonitorActor(nullptr) {
  MOZ_DIAGNOSTIC_ASSERT(!IsForJSPlugin(),
                        "XXX(nika): How are we creating a JSPlugin?");

  mRemoteTypeIsolationPrincipal =
      CreateRemoteTypeIsolationPrincipal(aRemoteType);

  // Insert ourselves into the global linked list of ContentParent objects.
  if (!sContentParents) {
    sContentParents = MakeUnique<LinkedList<ContentParent>>();
  }
  sContentParents->insertBack(this);

  mMessageManager = nsFrameMessageManager::NewProcessMessageManager(true);

#if defined(XP_WIN)
  // Request Windows message deferral behavior on our side of the PContent
  // channel. Generally only applies to the situation where we get caught in
  // a deadlock with the plugin process when sending CPOWs.
  GetIPCChannel()->SetChannelFlags(
      MessageChannel::REQUIRE_DEFERRED_MESSAGE_PROTECTION);
#endif

  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  bool isFile = mRemoteType == FILE_REMOTE_TYPE;
  mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, isFile);
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Verbose,
          ("CreateSubprocess: ContentParent %p mSubprocess %p handle %" PRIuPTR,
           this, mSubprocess,
           mSubprocess ? (uintptr_t)mSubprocess->GetChildProcessHandle() : -1));

  // This is safe to do in the constructor, as it doesn't take a strong
  // reference.
  mScriptableHelper = new ScriptableCPInfo(this);
}

ContentParent::~ContentParent() {
  if (mForceKillTimer) {
    mForceKillTimer->Cancel();
  }

  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mIsAPreallocBlocker) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Removing blocker on ContentProcess destruction"));
    PreallocatedProcessManager::RemoveBlocker(mRemoteType, this);
    mIsAPreallocBlocker = false;
  }

  // We should be removed from all these lists in ActorDestroy.
  AssertNotInPool();

  // Normally mSubprocess is destroyed in ActorDestroy, but that won't
  // happen if the process wasn't launched or if it failed to launch.
  if (mSubprocess) {
    MOZ_LOG(
        ContentParent::GetLog(), LogLevel::Verbose,
        ("DestroySubprocess: ContentParent %p mSubprocess %p handle %" PRIuPTR,
         this, mSubprocess,
         mSubprocess ? (uintptr_t)mSubprocess->GetChildProcessHandle() : -1));
    mSubprocess->Destroy();
  }

  // Make sure to clear the connection from `mScriptableHelper` if it hasn't
  // been cleared yet.
  if (mScriptableHelper) {
    static_cast<ScriptableCPInfo*>(mScriptableHelper.get())->ProcessDied();
    mScriptableHelper = nullptr;
  }
}

bool ContentParent::InitInternal(ProcessPriority aInitialPriority) {
  // We can't access the locale service after shutdown has started. Since we
  // can't init the process without it, and since we're going to be canceling
  // whatever load attempt that initiated this process creation anyway, just
  // bail out now if shutdown has already started.
  if (PastShutdownPhase(ShutdownPhase::XPCOMShutdown)) {
    return false;
  }

  XPCOMInitData xpcomInit;

  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("ContentParent::InitInternal: %p", (void*)this));
  nsCOMPtr<nsIIOService> io(do_GetIOService());
  MOZ_ASSERT(io, "No IO service?");
  DebugOnly<nsresult> rv = io->GetOffline(&xpcomInit.isOffline());
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed getting offline?");

  rv = io->GetConnectivity(&xpcomInit.isConnected());
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed getting connectivity?");

  xpcomInit.captivePortalState() = nsICaptivePortalService::UNKNOWN;
  nsCOMPtr<nsICaptivePortalService> cps =
      do_GetService(NS_CAPTIVEPORTAL_CONTRACTID);
  if (cps) {
    cps->GetState(&xpcomInit.captivePortalState());
  }

  nsIBidiKeyboard* bidi = nsContentUtils::GetBidiKeyboard();

  xpcomInit.isLangRTL() = false;
  xpcomInit.haveBidiKeyboards() = false;
  if (bidi) {
    bidi->IsLangRTL(&xpcomInit.isLangRTL());
    bidi->GetHaveBidiKeyboards(&xpcomInit.haveBidiKeyboards());
  }

  RefPtr<mozSpellChecker> spellChecker(mozSpellChecker::Create());
  MOZ_ASSERT(spellChecker, "No spell checker?");

  spellChecker->GetDictionaryList(&xpcomInit.dictionaries());

  LocaleService::GetInstance()->GetAppLocalesAsBCP47(xpcomInit.appLocales());
  LocaleService::GetInstance()->GetRequestedLocales(
      xpcomInit.requestedLocales());

  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1"));
  MOZ_ASSERT(clipboard, "No clipboard?");

  rv = clipboard->SupportsSelectionClipboard(
      &xpcomInit.clipboardCaps().supportsSelectionClipboard());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = clipboard->SupportsFindClipboard(
      &xpcomInit.clipboardCaps().supportsFindClipboard());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Let's copy the domain policy from the parent to the child (if it's active).
  StructuredCloneData initialData;
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (ssm) {
    ssm->CloneDomainPolicy(&xpcomInit.domainPolicy());

    if (ParentProcessMessageManager* mm =
            nsFrameMessageManager::sParentProcessManager) {
      AutoJSAPI jsapi;
      if (NS_WARN_IF(!jsapi.Init(xpc::PrivilegedJunkScope()))) {
        MOZ_CRASH();
      }
      JS::RootedValue init(jsapi.cx());
      // We'll crash on failure, so use a IgnoredErrorResult (which also
      // auto-suppresses exceptions).
      IgnoredErrorResult rv;
      mm->GetInitialProcessData(jsapi.cx(), &init, rv);
      if (NS_WARN_IF(rv.Failed())) {
        MOZ_CRASH();
      }

      initialData.Write(jsapi.cx(), init, rv);
      if (NS_WARN_IF(rv.Failed())) {
        MOZ_CRASH();
      }
    }
  }
  // This is only implemented (returns a non-empty list) by MacOSX and Linux
  // at present.
  nsTArray<SystemFontListEntry> fontList;
  gfxPlatform::GetPlatform()->ReadSystemFontList(&fontList);

  LookAndFeelData lnfData = GetLookAndFeelData();

  // If the shared fontlist is in use, collect its shmem block handles to pass
  // to the child.
  nsTArray<SharedMemoryHandle> sharedFontListBlocks;
  gfxPlatformFontList::PlatformFontList()->ShareFontListToProcess(
      &sharedFontListBlocks, OtherPid());

  // Content processes have no permission to access profile directory, so we
  // send the file URL instead.
  auto* sheetCache = GlobalStyleSheetCache::Singleton();
  if (StyleSheet* ucs = sheetCache->GetUserContentSheet()) {
    xpcomInit.userContentSheetURL() = ucs->GetSheetURI();
  } else {
    xpcomInit.userContentSheetURL() = nullptr;
  }

  // 1. Build ContentDeviceData first, as it may affect some gfxVars.
  gfxPlatform::GetPlatform()->BuildContentDeviceData(
      &xpcomInit.contentDeviceData());
  // 2. Gather non-default gfxVars.
  xpcomInit.gfxNonDefaultVarUpdates() = gfxVars::FetchNonDefaultVars();
  // 3. Start listening for gfxVars updates, to notify content process later on.
  gfxVars::AddReceiver(this);

  nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
  if (gfxInfo) {
    GfxInfoBase* gfxInfoRaw = static_cast<GfxInfoBase*>(gfxInfo.get());
    xpcomInit.gfxFeatureStatus() = gfxInfoRaw->GetAllFeatures();
  }

#ifdef XP_WIN
  xpcomInit.systemParameters() =
      widget::WinContentSystemParameters::GetSingleton()->GetParentValues();
#endif

  DataStorage::GetAllChildProcessData(xpcomInit.dataStorage());

  // Send the dynamic scalar definitions to the new process.
  TelemetryIPC::GetDynamicScalarDefinitions(xpcomInit.dynamicScalarDefs());

  for (auto const& [location, supported] : sCodecsSupported) {
    Unused << SendUpdateMediaCodecsSupported(location, supported);
  }

  // Must send screen info before send initialData
  ScreenManager& screenManager = ScreenManager::GetSingleton();
  screenManager.CopyScreensToRemote(this);

  // Send the UA sheet shared memory buffer and the address it is mapped at.
  Maybe<SharedMemoryHandle> sharedUASheetHandle;
  uintptr_t sharedUASheetAddress = sheetCache->GetSharedMemoryAddress();

  SharedMemoryHandle handle;
  if (sheetCache->ShareToProcess(OtherPid(), &handle)) {
    sharedUASheetHandle.emplace(handle);
  } else {
    sharedUASheetAddress = 0;
  }

  Unused << SendSetXPCOMProcessAttributes(
      xpcomInit, initialData, lnfData, fontList, sharedUASheetHandle,
      sharedUASheetAddress, sharedFontListBlocks);

  ipc::WritableSharedMap* sharedData =
      nsFrameMessageManager::sParentProcessManager->SharedData();
  sharedData->Flush();
  sharedData->SendTo(this);

  nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
  nsChromeRegistryChrome* chromeRegistry =
      static_cast<nsChromeRegistryChrome*>(registrySvc.get());
  chromeRegistry->SendRegisteredChrome(this);

  nsCOMPtr<nsIStringBundleService> stringBundleService =
      components::StringBundle::Service();
  stringBundleService->SendContentBundles(this);

  if (gAppData) {
    nsCString version(gAppData->version);
    nsCString buildID(gAppData->buildID);
    nsCString name(gAppData->name);
    nsCString UAName(gAppData->UAName);
    nsCString ID(gAppData->ID);
    nsCString vendor(gAppData->vendor);
    nsCString sourceURL(gAppData->sourceURL);
    nsCString updateURL(gAppData->updateURL);

    // Sending all information to content process.
    Unused << SendAppInfo(version, buildID, name, UAName, ID, vendor, sourceURL,
                          updateURL);
  }

  // Send the child its remote type. On Mac, this needs to be sent prior
  // to the message we send to enable the Sandbox (SendStartProcessSandbox)
  // because different remote types require different sandbox privileges.
  Unused << SendRemoteType(mRemoteType);

  ScriptPreloader::InitContentChild(*this);

  // Initialize the message manager (and load delayed scripts) now that we
  // have established communications with the child.
  mMessageManager->InitWithCallback(this);
  mMessageManager->SetOsPid(Pid());

  // Set the subprocess's priority.  We do this early on because we're likely
  // /lowering/ the process's CPU and memory priority, which it has inherited
  // from this process.
  //
  // This call can cause us to send IPC messages to the child process, so it
  // must come after the Open() call above.
  ProcessPriorityManager::SetProcessPriority(this, aInitialPriority);

  // NB: internally, this will send an IPC message to the child
  // process to get it to create the CompositorBridgeChild.  This
  // message goes through the regular IPC queue for this
  // channel, so delivery will happen-before any other messages
  // we send.  The CompositorBridgeChild must be created before any
  // PBrowsers are created, because they rely on the Compositor
  // already being around.  (Creation is async, so can't happen
  // on demand.)
  GPUProcessManager* gpm = GPUProcessManager::Get();

  Endpoint<PCompositorManagerChild> compositor;
  Endpoint<PImageBridgeChild> imageBridge;
  Endpoint<PVRManagerChild> vrBridge;
  Endpoint<PRemoteDecoderManagerChild> videoManager;
  AutoTArray<uint32_t, 3> namespaces;

  if (!gpm->CreateContentBridges(OtherPid(), &compositor, &imageBridge,
                                 &vrBridge, &videoManager, &namespaces)) {
    // This can fail if we've already started shutting down the compositor
    // thread. See Bug 1562763 comment 8.
    return false;
  }

  Unused << SendInitRendering(std::move(compositor), std::move(imageBridge),
                              std::move(vrBridge), std::move(videoManager),
                              namespaces);

  gpm->AddListener(this);

  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();
  if (sheetService) {
    // This looks like a lot of work, but in a normal browser session we just
    // send two loads.
    //
    // The URIs of the Gecko and Servo sheets should be the same, so it
    // shouldn't matter which we look at.

    for (StyleSheet* sheet : *sheetService->AgentStyleSheets()) {
      Unused << SendLoadAndRegisterSheet(sheet->GetSheetURI(),
                                         nsIStyleSheetService::AGENT_SHEET);
    }

    for (StyleSheet* sheet : *sheetService->UserStyleSheets()) {
      Unused << SendLoadAndRegisterSheet(sheet->GetSheetURI(),
                                         nsIStyleSheetService::USER_SHEET);
    }

    for (StyleSheet* sheet : *sheetService->AuthorStyleSheets()) {
      Unused << SendLoadAndRegisterSheet(sheet->GetSheetURI(),
                                         nsIStyleSheetService::AUTHOR_SHEET);
    }
  }

#ifdef MOZ_SANDBOX
  bool shouldSandbox = true;
  Maybe<FileDescriptor> brokerFd;
  // XXX: Checking the pref here makes it possible to enable/disable sandboxing
  // during an active session. Currently the pref is only used for testing
  // purpose. If the decision is made to permanently rely on the pref, this
  // should be changed so that it is required to restart firefox for the change
  // of value to take effect. Always send SetProcessSandbox message on macOS.
#  if !defined(XP_MACOSX)
  shouldSandbox = IsContentSandboxEnabled();
#  endif

#  ifdef XP_LINUX
  if (shouldSandbox) {
    MOZ_ASSERT(!mSandboxBroker);
    bool isFileProcess = mRemoteType == FILE_REMOTE_TYPE;
    UniquePtr<SandboxBroker::Policy> policy =
        sSandboxBrokerPolicyFactory->GetContentPolicy(Pid(), isFileProcess);
    if (policy) {
      brokerFd = Some(FileDescriptor());
      mSandboxBroker =
          SandboxBroker::Create(std::move(policy), Pid(), brokerFd.ref());
      if (!mSandboxBroker) {
        KillHard("SandboxBroker::Create failed");
        return false;
      }
      MOZ_ASSERT(brokerFd.ref().IsValid());
    }
  }
#  endif
  if (shouldSandbox && !SendSetProcessSandbox(brokerFd)) {
    KillHard("SandboxInitFailed");
  }
#endif

  if (!ServiceWorkerParentInterceptEnabled()) {
    RefPtr<ServiceWorkerRegistrar> swr = ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(swr);

    nsTArray<ServiceWorkerRegistrationData> registrations;
    swr->GetRegistrations(registrations);

    // Send down to the content process the permissions for each of the
    // registered service worker scopes.
    for (auto& registration : registrations) {
      auto principalOrErr = PrincipalInfoToPrincipal(registration.principal());
      if (principalOrErr.isOk()) {
        nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();
        TransmitPermissionsForPrincipal(principal);
      }
    }

    Unused << SendInitServiceWorkers(ServiceWorkerConfiguration(registrations));
  }

  {
    nsTArray<BlobURLRegistrationData> registrations;
    BlobURLProtocolHandler::ForEachBlobURL(
        [&](BlobImpl* aBlobImpl, nsIPrincipal* aPrincipal,
            const Maybe<nsID>& aAgentClusterId, const nsACString& aURI,
            bool aRevoked) {
          nsAutoCString origin;
          nsresult rv = aPrincipal->GetOrigin(origin);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return false;
          }

          // We send all moz-extension Blob URL's to all content processes
          // because content scripts mean that a moz-extension can live in any
          // process. Same thing for system principal Blob URLs. Content Blob
          // URL's are sent for content principals on-demand by
          // AboutToLoadHttpFtpDocumentForChild and RemoteWorkerManager.
          if (!StringBeginsWith(origin, "moz-extension://"_ns) &&
              !aPrincipal->IsSystemPrincipal()) {
            return true;
          }

          IPCBlob ipcBlob;
          rv = IPCBlobUtils::Serialize(aBlobImpl, this, ipcBlob);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return false;
          }

          registrations.AppendElement(BlobURLRegistrationData(
              nsCString(aURI), ipcBlob, aPrincipal, aAgentClusterId, aRevoked));

          rv = TransmitPermissionsForPrincipal(aPrincipal);
          Unused << NS_WARN_IF(NS_FAILED(rv));
          return true;
        });

    if (!registrations.IsEmpty()) {
      Unused << SendInitBlobURLs(registrations);
    }
  }

  // Send down { Parent, Window }ActorOptions at startup to content process.
  RefPtr<JSActorService> actorSvc = JSActorService::GetSingleton();
  if (actorSvc) {
    nsTArray<JSProcessActorInfo> contentInfos;
    actorSvc->GetJSProcessActorInfos(contentInfos);

    nsTArray<JSWindowActorInfo> windowInfos;
    actorSvc->GetJSWindowActorInfos(windowInfos);

    Unused << SendInitJSActorInfos(contentInfos, windowInfos);
  }

  // Begin subscribing to any BrowsingContextGroups which were hosted by this
  // process before it finished launching.
  for (auto& group : mGroups) {
    group.GetKey()->Subscribe(this);
  }

  // Start up nsPluginHost and run FindPlugins to cache the plugin list.
  // If this isn't our first content process, just send over cached list.
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  pluginHost->SendPluginsToContent(this);
  MaybeEnableRemoteInputEventQueue();

  return true;
}

bool ContentParent::IsAlive() const {
  return mLifecycleState == LifecycleState::ALIVE ||
         mLifecycleState == LifecycleState::INITIALIZED;
}

bool ContentParent::IsInitialized() const {
  return mLifecycleState == LifecycleState::INITIALIZED;
}

int32_t ContentParent::Pid() const {
  if (!mSubprocess || !mSubprocess->GetChildProcessHandle()) {
    return -1;
  }
  return base::GetProcId(mSubprocess->GetChildProcessHandle());
}

mozilla::ipc::IPCResult ContentParent::RecvGetGfxVars(
    nsTArray<GfxVarUpdate>* aVars) {
  // Ensure gfxVars is initialized (for xpcshell tests).
  gfxVars::Initialize();

  *aVars = gfxVars::FetchNonDefaultVars();

  // Now that content has initialized gfxVars, we can start listening for
  // updates.
  gfxVars::AddReceiver(this);
  return IPC_OK();
}

void ContentParent::OnCompositorUnexpectedShutdown() {
  GPUProcessManager* gpm = GPUProcessManager::Get();

  Endpoint<PCompositorManagerChild> compositor;
  Endpoint<PImageBridgeChild> imageBridge;
  Endpoint<PVRManagerChild> vrBridge;
  Endpoint<PRemoteDecoderManagerChild> videoManager;
  AutoTArray<uint32_t, 3> namespaces;

  DebugOnly<bool> opened =
      gpm->CreateContentBridges(OtherPid(), &compositor, &imageBridge,
                                &vrBridge, &videoManager, &namespaces);
  MOZ_ASSERT(opened);

  Unused << SendReinitRendering(std::move(compositor), std::move(imageBridge),
                                std::move(vrBridge), std::move(videoManager),
                                namespaces);
}

void ContentParent::OnCompositorDeviceReset() {
  Unused << SendReinitRenderingForDeviceReset();
}

void ContentParent::MaybeEnableRemoteInputEventQueue() {
  MOZ_ASSERT(!mIsRemoteInputEventQueueEnabled);
  if (!IsInputEventQueueSupported()) {
    return;
  }
  mIsRemoteInputEventQueueEnabled = true;
  Unused << SendSetInputEventQueueEnabled();
  SetInputPriorityEventEnabled(true);
}

void ContentParent::SetInputPriorityEventEnabled(bool aEnabled) {
  if (!IsInputEventQueueSupported() || !mIsRemoteInputEventQueueEnabled ||
      mIsInputPriorityEventEnabled == aEnabled) {
    return;
  }
  mIsInputPriorityEventEnabled = aEnabled;
  // Send IPC messages to flush the pending events in the input event queue and
  // the normal event queue. See PContent.ipdl for more details.
  Unused << SendSuspendInputEventQueue();
  Unused << SendFlushInputEventQueue();
  Unused << SendResumeInputEventQueue();
}

/*static*/
bool ContentParent::IsInputEventQueueSupported() {
  static bool sSupported = false;
  static bool sInitialized = false;
  if (!sInitialized) {
    MOZ_ASSERT(Preferences::IsServiceAvailable());
    sSupported = Preferences::GetBool("input_event_queue.supported", false);
    sInitialized = true;
  }
  return sSupported;
}

void ContentParent::OnVarChanged(const GfxVarUpdate& aVar) {
  if (!CanSend()) {
    return;
  }
  Unused << SendVarUpdate(aVar);
}

mozilla::ipc::IPCResult ContentParent::RecvSetClipboard(
    const IPCDataTransfer& aDataTransfer, const bool& aIsPrivateData,
    const IPC::Principal& aRequestingPrincipal,
    const nsContentPolicyType& aContentPolicyType,
    const int32_t& aWhichClipboard) {
  if (!ValidatePrincipal(aRequestingPrincipal,
                         {ValidatePrincipalOptions::AllowNullPtr})) {
    LogAndAssertFailedPrincipalValidationInfo(aRequestingPrincipal, __func__);
  }

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);

  rv = nsContentUtils::IPCTransferableToTransferable(
      aDataTransfer, aIsPrivateData, aRequestingPrincipal, aContentPolicyType,
      trans, this, nullptr);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  clipboard->SetData(trans, nullptr, aWhichClipboard);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetClipboard(
    nsTArray<nsCString>&& aTypes, const int32_t& aWhichClipboard,
    IPCDataTransfer* aDataTransfer) {
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);
  // The private flag is only used to prevent the data from being cached to the
  // disk. The flag is not exported to the IPCDataTransfer object.
  // The flag is set because we are not sure whether the clipboard data is used
  // in a private browsing context. The transferable is only used in this scope,
  // so the cache would not reduce memory consumption anyway.
  trans->SetIsPrivateData(true);

  for (uint32_t t = 0; t < aTypes.Length(); t++) {
    trans->AddDataFlavor(aTypes[t].get());
  }

  clipboard->GetData(trans, aWhichClipboard);
  nsContentUtils::TransferableToIPCTransferable(trans, aDataTransfer, true,
                                                nullptr, this);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvEmptyClipboard(
    const int32_t& aWhichClipboard) {
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  clipboard->EmptyClipboard(aWhichClipboard);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvClipboardHasType(
    nsTArray<nsCString>&& aTypes, const int32_t& aWhichClipboard,
    bool* aHasType) {
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  clipboard->HasDataMatchingFlavors(aTypes, aWhichClipboard, aHasType);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetExternalClipboardFormats(
    const int32_t& aWhichClipboard, const bool& aPlainTextOnly,
    nsTArray<nsCString>* aTypes) {
  MOZ_ASSERT(aTypes);
  DataTransfer::GetExternalClipboardFormats(aWhichClipboard, aPlainTextOnly,
                                            aTypes);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvPlaySound(nsIURI* aURI) {
  // If the check here fails, it can only mean that this message was spoofed.
  if (!aURI || !aURI->SchemeIs("chrome")) {
    // PlaySound only accepts a valid chrome URI.
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIURL> soundURL(do_QueryInterface(aURI));
  if (!soundURL) {
    return IPC_OK();
  }

  nsresult rv;
  nsCOMPtr<nsISound> sound(do_GetService(NS_SOUND_CID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  sound->Play(soundURL);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvBeep() {
  nsresult rv;
  nsCOMPtr<nsISound> sound(do_GetService(NS_SOUND_CID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  sound->Beep();

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvPlayEventSound(
    const uint32_t& aEventId) {
  nsresult rv;
  nsCOMPtr<nsISound> sound(do_GetService(NS_SOUND_CID, &rv));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  sound->PlayEventSound(aEventId);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetIconForExtension(
    const nsCString& aFileExt, const uint32_t& aIconSize,
    nsTArray<uint8_t>* bits) {
#ifdef MOZ_WIDGET_ANDROID
  NS_ASSERTION(AndroidBridge::Bridge() != nullptr,
               "AndroidBridge is not available");
  if (AndroidBridge::Bridge() == nullptr) {
    // Do not fail - just no icon will be shown
    return IPC_OK();
  }

  bits->AppendElements(aIconSize * aIconSize * 4);

  AndroidBridge::Bridge()->GetIconForExtension(aFileExt, aIconSize,
                                               bits->Elements());
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvFirstIdle() {
  // When the ContentChild goes idle, it sends us a FirstIdle message
  // which we use as a good time to signal the PreallocatedProcessManager
  // that it can start allocating processes from now on.
  if (mIsAPreallocBlocker) {
    PreallocatedProcessManager::RemoveBlocker(mRemoteType, this);
    mIsAPreallocBlocker = false;
  }
  return IPC_OK();
}

// We want ContentParent to show up in CC logs for debugging purposes, but we
// don't actually cycle collect it.
NS_IMPL_CYCLE_COLLECTION_0(ContentParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ContentParent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ContentParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ContentParent)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(ContentParent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMProcessParent)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionCallback)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionErrorCallback)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncShutdownBlocker)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMProcessParent)
NS_INTERFACE_MAP_END

// Async shutdown blocker
NS_IMETHODIMP
ContentParent::BlockShutdown(nsIAsyncShutdownClient* aClient) {
  // Make sure that our process will get scheduled.
  ProcessPriorityManager::SetProcessPriority(this, PROCESS_PRIORITY_FOREGROUND);

  // Okay to call ShutDownProcess multiple times.
  ShutDownProcess(SEND_SHUTDOWN_MESSAGE);
  MarkAsDead();

  return NS_OK;
}

NS_IMETHODIMP
ContentParent::GetName(nsAString& aName) {
  aName.AssignLiteral("ContentParent:");
  aName.AppendPrintf(" id=%p", this);
  return NS_OK;
}

NS_IMETHODIMP
ContentParent::GetState(nsIPropertyBag** aResult) {
  auto props = MakeRefPtr<nsHashPropertyBag>();
  props->SetPropertyAsACString(u"remoteTypePrefix"_ns,
                               RemoteTypePrefix(mRemoteType));
  *aResult = props.forget().downcast<nsIWritablePropertyBag>().take();
  return NS_OK;
}

static StaticRefPtr<nsIAsyncShutdownClient> sXPCOMShutdownClient;
static StaticRefPtr<nsIAsyncShutdownClient> sProfileBeforeChangeClient;

static void InitClients() {
  if (!sXPCOMShutdownClient) {
    nsresult rv;
    nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdownService();

    nsCOMPtr<nsIAsyncShutdownClient> client;
    rv = svc->GetXpcomWillShutdown(getter_AddRefs(client));
    sXPCOMShutdownClient = client.forget();
    ClearOnShutdown(&sXPCOMShutdownClient);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv), "XPCOMShutdown shutdown blocker");

    rv = svc->GetProfileBeforeChange(getter_AddRefs(client));
    sProfileBeforeChangeClient = client.forget();
    ClearOnShutdown(&sProfileBeforeChangeClient);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv),
                       "profileBeforeChange shutdown blocker");
  }
}

void ContentParent::AddShutdownBlockers() {
  InitClients();

  sXPCOMShutdownClient->AddBlocker(
      this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__, u""_ns);
  sProfileBeforeChangeClient->AddBlocker(
      this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__, u""_ns);
}

void ContentParent::RemoveShutdownBlockers() {
  Unused << sXPCOMShutdownClient->RemoveBlocker(this);
  Unused << sProfileBeforeChangeClient->RemoveBlocker(this);
}

NS_IMETHODIMP
ContentParent::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
  if (IsDead() || !mSubprocess) {
    return NS_OK;
  }

  if (!strcmp(aTopic, "nsPref:changed")) {
    // A pref changed. If it is useful to do so, inform child processes.
    if (!ShouldSyncPreference(aData)) {
      return NS_OK;
    }

    // We know prefs are ASCII here.
    NS_LossyConvertUTF16toASCII strData(aData);

    Pref pref(strData, /* isLocked */ false, Nothing(), Nothing());
    Preferences::GetPreference(&pref);
    if (IsInitialized()) {
      MOZ_ASSERT(mQueuedPrefs.IsEmpty());
      if (!SendPreferenceUpdate(pref)) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    } else {
      MOZ_ASSERT(!IsDead());
      mQueuedPrefs.AppendElement(pref);
    }
  }

  if (!IsAlive()) {
    return NS_OK;
  }

  // listening for memory pressure event
  if (!strcmp(aTopic, "memory-pressure")) {
    Unused << SendFlushMemory(nsDependentString(aData));
  } else if (!strcmp(aTopic, "application-background")) {
    Unused << SendApplicationBackground();
  } else if (!strcmp(aTopic, "application-foreground")) {
    Unused << SendApplicationForeground();
  } else if (!strcmp(aTopic, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC)) {
    NS_ConvertUTF16toUTF8 dataStr(aData);
    const char* offline = dataStr.get();
    if (!SendSetOffline(!strcmp(offline, "true"))) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  } else if (!strcmp(aTopic, NS_IPC_IOSERVICE_SET_CONNECTIVITY_TOPIC)) {
    if (!SendSetConnectivity(u"true"_ns.Equals(aData))) {
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
  } else if (!strcmp(aTopic, "child-gc-request")) {
    Unused << SendGarbageCollect();
  } else if (!strcmp(aTopic, "child-cc-request")) {
    Unused << SendCycleCollect();
  } else if (!strcmp(aTopic, "child-mmu-request")) {
    Unused << SendMinimizeMemoryUsage();
  } else if (!strcmp(aTopic, "child-ghost-request")) {
    Unused << SendUnlinkGhosts();
  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    Unused << SendLastPrivateDocShellDestroyed();
  }
#ifdef ACCESSIBILITY
  else if (aData && !strcmp(aTopic, "a11y-init-or-shutdown")) {
    if (*aData == '1') {
      // Make sure accessibility is running in content process when
      // accessibility gets initiated in chrome process.
#  if defined(XP_WIN)
      // Don't init content a11y if we detect an incompat version of JAWS in
      // use.
      if (!mozilla::a11y::Compatibility::IsOldJAWS()) {
        Unused << SendActivateA11y(
            ::GetCurrentThreadId(),
            a11y::AccessibleWrap::GetContentProcessIdFor(ChildID()));
      }
#  else
      Unused << SendActivateA11y(0, 0);
#  endif
    } else {
      // If possible, shut down accessibility in content process when
      // accessibility gets shutdown in chrome process.
      Unused << SendShutdownA11y();
    }
  }
#endif
  else if (!strcmp(aTopic, "cacheservice:empty-cache")) {
    Unused << SendNotifyEmptyHTTPCache();
  } else if (!strcmp(aTopic, "intl:app-locales-changed")) {
    nsTArray<nsCString> appLocales;
    LocaleService::GetInstance()->GetAppLocalesAsBCP47(appLocales);
    Unused << SendUpdateAppLocales(appLocales);
  } else if (!strcmp(aTopic, "intl:requested-locales-changed")) {
    nsTArray<nsCString> requestedLocales;
    LocaleService::GetInstance()->GetRequestedLocales(requestedLocales);
    Unused << SendUpdateRequestedLocales(requestedLocales);
  } else if (!strcmp(aTopic, "cookie-changed") ||
             !strcmp(aTopic, "private-cookie-changed")) {
    if (!aData) {
      return NS_ERROR_UNEXPECTED;
    }
    PNeckoParent* neckoParent = LoneManagedOrNullAsserts(ManagedPNeckoParent());
    if (!neckoParent) {
      return NS_OK;
    }
    PCookieServiceParent* csParent =
        LoneManagedOrNullAsserts(neckoParent->ManagedPCookieServiceParent());
    if (!csParent) {
      return NS_OK;
    }
    auto* cs = static_cast<CookieServiceParent*>(csParent);
    // Do not push these cookie updates to the same process they originated
    // from.
    if (cs->ProcessingCookie()) {
      return NS_OK;
    }
    if (!nsCRT::strcmp(aData, u"batch-deleted")) {
      nsCOMPtr<nsIArray> cookieList = do_QueryInterface(aSubject);
      NS_ASSERTION(cookieList, "couldn't get cookie list");
      cs->RemoveBatchDeletedCookies(cookieList);
      return NS_OK;
    }

    if (!nsCRT::strcmp(aData, u"cleared")) {
      cs->RemoveAll();
      return NS_OK;
    }

    nsCOMPtr<nsICookie> xpcCookie = do_QueryInterface(aSubject);
    NS_ASSERTION(xpcCookie, "couldn't get cookie");
    if (!nsCRT::strcmp(aData, u"deleted")) {
      cs->RemoveCookie(xpcCookie);
    } else if ((!nsCRT::strcmp(aData, u"added")) ||
               (!nsCRT::strcmp(aData, u"changed"))) {
      cs->AddCookie(xpcCookie);
    }
  } else if (!strcmp(aTopic, NS_NETWORK_LINK_TYPE_TOPIC)) {
    UpdateNetworkLinkType();
  }

  return NS_OK;
}

/* static */
bool ContentParent::ShouldSyncPreference(const char16_t* aData) {
#define PARENT_ONLY_PREF_LIST_ENTRY(s) \
  { s, (sizeof(s) / sizeof(char16_t)) - 1 }
  struct ParentOnlyPrefListEntry {
    const char16_t* mPrefBranch;
    size_t mLen;
  };
  // These prefs are not useful in child processes.
  static const ParentOnlyPrefListEntry sParentOnlyPrefBranchList[] = {
      PARENT_ONLY_PREF_LIST_ENTRY(u"app.update.lastUpdateTime."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"datareporting.policy."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"browser.safebrowsing.provider."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"browser.shell."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"browser.slowStartup."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"browser.startup."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"extensions.getAddons.cache."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"media.gmp-manager."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"media.gmp-gmpopenh264."),
      PARENT_ONLY_PREF_LIST_ENTRY(u"privacy.sanitize."),
  };
#undef PARENT_ONLY_PREF_LIST_ENTRY

  for (const auto& entry : sParentOnlyPrefBranchList) {
    if (NS_strncmp(entry.mPrefBranch, aData, entry.mLen) == 0) {
      return false;
    }
  }
  return true;
}

void ContentParent::UpdateNetworkLinkType() {
  nsresult rv;
  nsCOMPtr<nsINetworkLinkService> nls =
      do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  uint32_t linkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  rv = nls->GetLinkType(&linkType);
  if (NS_FAILED(rv)) {
    return;
  }

  Unused << SendNetworkLinkTypeChange(linkType);
}

NS_IMETHODIMP
ContentParent::GetInterface(const nsIID& aIID, void** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIID.Equals(NS_GET_IID(nsIMessageSender))) {
    nsCOMPtr<nsIMessageSender> mm = GetMessageManager();
    mm.forget(aResult);
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

mozilla::ipc::IPCResult ContentParent::RecvInitBackground(
    Endpoint<PBackgroundParent>&& aEndpoint) {
  if (!BackgroundParent::Alloc(this, std::move(aEndpoint))) {
    NS_WARNING("BackgroundParent::Alloc failed");
  }

  return IPC_OK();
}

bool ContentParent::CanOpenBrowser(const IPCTabContext& aContext) {
  // (PopupIPCTabContext lets the child process prove that it has access to
  // the app it's trying to open.)
  // On e10s we also allow UnsafeTabContext to allow service workers to open
  // windows. This is enforced in MaybeInvalidTabContext.
  if (aContext.type() != IPCTabContext::TPopupIPCTabContext) {
    ASSERT_UNLESS_FUZZING(
        "Unexpected IPCTabContext type.  Aborting AllocPBrowserParent.");
    return false;
  }

  if (aContext.type() == IPCTabContext::TPopupIPCTabContext) {
    const PopupIPCTabContext& popupContext = aContext.get_PopupIPCTabContext();

    auto opener = BrowserParent::GetFrom(popupContext.openerParent());
    if (!opener) {
      ASSERT_UNLESS_FUZZING(
          "Got null opener from child; aborting AllocPBrowserParent.");
      return false;
    }
  }

  MaybeInvalidTabContext tc(aContext);
  if (!tc.IsValid()) {
    NS_ERROR(nsPrintfCString("Child passed us an invalid TabContext.  (%s)  "
                             "Aborting AllocPBrowserParent.",
                             tc.GetInvalidReason())
                 .get());
    return false;
  }

  return true;
}

static bool CloneIsLegal(ContentParent* aCp, CanonicalBrowsingContext& aSource,
                         CanonicalBrowsingContext& aTarget) {
  // Source and target must be in the same BCG
  if (NS_WARN_IF(aSource.Group() != aTarget.Group())) {
    return false;
  }
  // The source and target must be in different toplevel <browser>s
  if (NS_WARN_IF(aSource.Top() == aTarget.Top())) {
    return false;
  }

  // Neither source nor target must be toplevel.
  if (NS_WARN_IF(aSource.IsTop()) || NS_WARN_IF(aTarget.IsTop())) {
    return false;
  }

  // Both should be embedded by the same process.
  auto* sourceEmbedder = aSource.GetParentWindowContext();
  if (NS_WARN_IF(!sourceEmbedder) ||
      NS_WARN_IF(sourceEmbedder->GetContentParent() != aCp)) {
    return false;
  }

  auto* targetEmbedder = aSource.GetParentWindowContext();
  if (NS_WARN_IF(!targetEmbedder) ||
      NS_WARN_IF(targetEmbedder->GetContentParent() != aCp)) {
    return false;
  }

  // All seems sane.
  return true;
}

mozilla::ipc::IPCResult ContentParent::RecvCloneDocumentTreeInto(
    const MaybeDiscarded<BrowsingContext>& aSource,
    const MaybeDiscarded<BrowsingContext>& aTarget) {
  if (aSource.IsNullOrDiscarded() || aTarget.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  auto* source = aSource.get_canonical();
  auto* target = aTarget.get_canonical();

  if (!CloneIsLegal(this, *source, *target)) {
    return IPC_FAIL(this, "Illegal subframe clone");
  }

  ContentParent* cp = source->GetContentParent();
  if (NS_WARN_IF(!cp)) {
    return IPC_OK();
  }

  if (NS_WARN_IF(cp->GetRemoteType() == GetRemoteType())) {
    // Wanted to switch to a target browsing context that's already local again.
    // See bug 1676996 for how this can happen.
    //
    // Dropping the switch on the floor seems fine for this case, though we
    // could also try to clone the local document.
    //
    // If the remote type matches & it's in the same group (which was confirmed
    // by CloneIsLegal), it must be the exact same process.
    MOZ_DIAGNOSTIC_ASSERT(cp == this);
    return IPC_OK();
  }

  RemotenessChangeOptions options;
  options.mRemoteType = cp->GetRemoteType();
  target->ChangeRemoteness(options, /* aPendingSwitchId = */ 0)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [source = RefPtr{source}](BrowserParent* aBp) {
            Unused << aBp->SendCloneDocumentTreeIntoSelf(source);
          },
          [](nsresult aRv) {
            NS_WARNING(
                nsPrintfCString("Remote clone failed: %x\n", unsigned(aRv))
                    .get());
          });
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvConstructPopupBrowser(
    ManagedEndpoint<PBrowserParent>&& aBrowserEp,
    ManagedEndpoint<PWindowGlobalParent>&& aWindowEp, const TabId& aTabId,
    const IPCTabContext& aContext, const WindowGlobalInit& aInitialWindowInit,
    const uint32_t& aChromeFlags) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!CanOpenBrowser(aContext)) {
    return IPC_FAIL(this, "CanOpenBrowser Failed");
  }

  RefPtr<CanonicalBrowsingContext> browsingContext =
      CanonicalBrowsingContext::Get(
          aInitialWindowInit.context().mBrowsingContextId);
  if (!browsingContext || browsingContext->IsDiscarded()) {
    return IPC_FAIL(this, "Null or discarded initial BrowsingContext");
  }
  if (!aInitialWindowInit.principal()) {
    return IPC_FAIL(this, "Cannot create without valid initial principal");
  }

  uint32_t chromeFlags = aChromeFlags;
  TabId openerTabId(0);
  ContentParentId openerCpId(0);
  if (aContext.type() == IPCTabContext::TPopupIPCTabContext) {
    // CanOpenBrowser has ensured that the IPCTabContext is of
    // type PopupIPCTabContext, and that the opener BrowserParent is
    // reachable.
    const PopupIPCTabContext& popupContext = aContext.get_PopupIPCTabContext();
    auto opener = BrowserParent::GetFrom(popupContext.openerParent());
    openerTabId = opener->GetTabId();
    openerCpId = opener->Manager()->ChildID();

    // We must ensure that the private browsing and remoteness flags
    // match those of the opener.
    nsCOMPtr<nsILoadContext> loadContext = opener->GetLoadContext();
    if (!loadContext) {
      return IPC_FAIL(this, "Missing Opener LoadContext");
    }
    if (loadContext->UsePrivateBrowsing()) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
    }
    if (loadContext->UseRemoteSubframes()) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_FISSION_WINDOW;
    }
  }

  // And because we're allocating a remote browser, of course the
  // window is remote.
  chromeFlags |= nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;

  if (NS_WARN_IF(!browsingContext->IsOwnedByProcess(ChildID()))) {
    return IPC_FAIL(this, "BrowsingContext Owned by Incorrect Process!");
  }

  MaybeInvalidTabContext tc(aContext);
  MOZ_ASSERT(tc.IsValid());

  RefPtr<WindowGlobalParent> initialWindow =
      WindowGlobalParent::CreateDisconnected(aInitialWindowInit);
  if (!initialWindow) {
    return IPC_FAIL(this, "Failed to create WindowGlobalParent");
  }

  auto parent = MakeRefPtr<BrowserParent>(this, aTabId, tc.GetTabContext(),
                                          browsingContext, chromeFlags);

  // Bind the created BrowserParent to IPC to actually link the actor.
  if (NS_WARN_IF(!BindPBrowserEndpoint(std::move(aBrowserEp), parent))) {
    return IPC_FAIL(this, "BindPBrowserEndpoint failed");
  }

  // XXX: Why are we checking these requirements? It seems we should register
  // the created frame unconditionally?
  if (openerTabId > 0) {
    // The creation of PBrowser was triggered from content process through
    // window.open().
    // We need to register remote frame with the child generated tab id.
    auto* cpm = ContentProcessManager::GetSingleton();
    if (!cpm->RegisterRemoteFrame(parent)) {
      return IPC_FAIL(this, "RegisterRemoteFrame Failed");
    }
  }

  if (NS_WARN_IF(!parent->BindPWindowGlobalEndpoint(std::move(aWindowEp),
                                                    initialWindow))) {
    return IPC_FAIL(this, "BindPWindowGlobalEndpoint failed");
  }

  initialWindow->Init();

  // When enabling input event prioritization, input events may preempt other
  // normal priority IPC messages. To prevent the input events preempt
  // PBrowserConstructor, we use an IPC 'RemoteIsReadyToHandleInputEvents' to
  // notify parent that BrowserChild is created. In this case, PBrowser is
  // initiated from content so that we can set BrowserParent as ready to handle
  // input
  parent->SetReadyToHandleInputEvents();
  return IPC_OK();
}

mozilla::PRemoteSpellcheckEngineParent*
ContentParent::AllocPRemoteSpellcheckEngineParent() {
  mozilla::RemoteSpellcheckEngineParent* parent =
      new mozilla::RemoteSpellcheckEngineParent();
  return parent;
}

bool ContentParent::DeallocPRemoteSpellcheckEngineParent(
    PRemoteSpellcheckEngineParent* parent) {
  delete parent;
  return true;
}

/* static */
void ContentParent::ForceKillTimerCallback(nsITimer* aTimer, void* aClosure) {
  // We don't want to time out the content process during XPCShell tests. This
  // is the easiest way to ensure that.
  if (PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR")) {
    return;
  }

  auto self = static_cast<ContentParent*>(aClosure);
  self->KillHard("ShutDownKill");
}

void ContentParent::GeneratePairedMinidump(const char* aReason) {
  // We're about to kill the child process associated with this content.
  // Something has gone wrong to get us here, so we generate a minidump
  // of the parent and child for submission to the crash server unless we're
  // already shutting down.
  nsCOMPtr<nsIAppStartup> appStartup = components::AppStartup::Service();
  if (mCrashReporter && !appStartup->GetShuttingDown() &&
      StaticPrefs::dom_ipc_tabs_createKillHardCrashReports_AtStartup()) {
    // GeneratePairedMinidump creates two minidumps for us - the main
    // one is for the content process we're about to kill, and the other
    // one is for the main browser process. That second one is the extra
    // minidump tagging along, so we have to tell the crash reporter that
    // it exists and is being appended.
    nsAutoCString additionalDumps("browser");
    mCrashReporter->AddAnnotation(
        CrashReporter::Annotation::additional_minidumps, additionalDumps);
    nsDependentCString reason(aReason);
    mCrashReporter->AddAnnotation(CrashReporter::Annotation::ipc_channel_error,
                                  reason);

    // Generate the report and insert into the queue for submittal.
    if (mCrashReporter->GenerateMinidumpAndPair(this, nullptr, "browser"_ns)) {
      mCreatedPairedMinidumps = mCrashReporter->FinalizeCrashReport();
    }
  }
}

void ContentParent::HandleOrphanedMinidump(nsString* aDumpId) {
  if (CrashReporter::FinalizeOrphanedMinidump(
          OtherPid(), GeckoProcessType_Content, aDumpId)) {
    CrashReporterHost::RecordCrash(GeckoProcessType_Content,
                                   nsICrashService::CRASH_TYPE_CRASH, *aDumpId);
  } else {
    NS_WARNING(nsPrintfCString("content process pid = %d crashed without "
                               "leaving a minidump behind",
                               OtherPid())
                   .get());
  }
}

// WARNING: aReason appears in telemetry, so any new value passed in requires
// data review.
void ContentParent::KillHard(const char* aReason) {
  AUTO_PROFILER_LABEL("ContentParent::KillHard", OTHER);

  // On Windows, calling KillHard multiple times causes problems - the
  // process handle becomes invalid on the first call, causing a second call
  // to crash our process - more details in bug 890840.
  if (mCalledKillHard) {
    return;
  }
  mCalledKillHard = true;
  mForceKillTimer = nullptr;

  RemoveShutdownBlockers();

  GeneratePairedMinidump(aReason);

  nsDependentCString reason(aReason);
  Telemetry::Accumulate(Telemetry::SUBPROCESS_KILL_HARD, reason, 1);

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
    MOZ_LOG(
        ContentParent::GetLog(), LogLevel::Verbose,
        ("KillHard Subprocess: ContentParent %p mSubprocess %p handle "
         "%" PRIuPTR,
         this, mSubprocess,
         mSubprocess ? (uintptr_t)mSubprocess->GetChildProcessHandle() : -1));
    mSubprocess->SetAlreadyDead();
  }

  // EnsureProcessTerminated has responsibilty for closing otherProcessHandle.
  XRE_GetIOMessageLoop()->PostTask(
      NewRunnableFunction("EnsureProcessTerminatedRunnable",
                          &ProcessWatcher::EnsureProcessTerminated,
                          otherProcessHandle, /*force=*/true));
}

void ContentParent::FriendlyName(nsAString& aName, bool aAnonymize) {
  aName.Truncate();
  if (mIsForBrowser) {
    aName.AssignLiteral("Browser");
  } else if (aAnonymize) {
    aName.AssignLiteral("<anonymized-name>");
  } else {
    aName.AssignLiteral("???");
  }
}

mozilla::ipc::IPCResult ContentParent::RecvInitCrashReporter(
    const NativeThreadId& aThreadId) {
  mCrashReporter =
      MakeUnique<CrashReporterHost>(GeckoProcessType_Content, aThreadId);

  return IPC_OK();
}

hal_sandbox::PHalParent* ContentParent::AllocPHalParent() {
  return hal_sandbox::CreateHalParent();
}

bool ContentParent::DeallocPHalParent(hal_sandbox::PHalParent* aHal) {
  delete aHal;
  return true;
}

devtools::PHeapSnapshotTempFileHelperParent*
ContentParent::AllocPHeapSnapshotTempFileHelperParent() {
  return devtools::HeapSnapshotTempFileHelperParent::Create();
}

bool ContentParent::DeallocPHeapSnapshotTempFileHelperParent(
    devtools::PHeapSnapshotTempFileHelperParent* aHeapSnapshotHelper) {
  delete aHeapSnapshotHelper;
  return true;
}

bool ContentParent::SendRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage, const Maybe<FileDescriptor>& aDMDFile) {
  // This automatically cancels the previous request.
  mMemoryReportRequest = MakeUnique<MemoryReportRequestHost>(aGeneration);
  // If we run the callback in response to a reply, then by definition |this|
  // is still alive, so the ref pointer is redundant, but it seems easier
  // to hold a strong reference than to worry about that.
  RefPtr<ContentParent> self(this);
  PContentParent::SendRequestMemoryReport(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile,
      [&, self](const uint32_t& aGeneration2) {
        if (self->mMemoryReportRequest) {
          self->mMemoryReportRequest->Finish(aGeneration2);
          self->mMemoryReportRequest = nullptr;
        }
      },
      [&, self](mozilla::ipc::ResponseRejectReason) {
        self->mMemoryReportRequest = nullptr;
      });
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAddMemoryReport(
    const MemoryReport& aReport) {
  if (mMemoryReportRequest) {
    mMemoryReportRequest->RecvReport(aReport);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAddPerformanceMetrics(
    const nsID& aID, nsTArray<PerformanceInfo>&& aMetrics) {
  nsresult rv = PerformanceMetricsCollector::DataReceived(aID, aMetrics);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  return IPC_OK();
}

PCycleCollectWithLogsParent* ContentParent::AllocPCycleCollectWithLogsParent(
    const bool& aDumpAllTraces, const FileDescriptor& aGCLog,
    const FileDescriptor& aCCLog) {
  MOZ_CRASH("Don't call this; use ContentParent::CycleCollectWithLogs");
}

bool ContentParent::DeallocPCycleCollectWithLogsParent(
    PCycleCollectWithLogsParent* aActor) {
  delete aActor;
  return true;
}

bool ContentParent::CycleCollectWithLogs(
    bool aDumpAllTraces, nsICycleCollectorLogSink* aSink,
    nsIDumpGCAndCCLogsCallback* aCallback) {
  return CycleCollectWithLogsParent::AllocAndSendConstructor(
      this, aDumpAllTraces, aSink, aCallback);
}

PTestShellParent* ContentParent::AllocPTestShellParent() {
  return new TestShellParent();
}

bool ContentParent::DeallocPTestShellParent(PTestShellParent* shell) {
  delete shell;
  return true;
}

PScriptCacheParent* ContentParent::AllocPScriptCacheParent(
    const FileDescOrError& cacheFile, const bool& wantCacheData) {
  return new loader::ScriptCacheParent(wantCacheData);
}

bool ContentParent::DeallocPScriptCacheParent(PScriptCacheParent* cache) {
  delete static_cast<loader::ScriptCacheParent*>(cache);
  return true;
}

PNeckoParent* ContentParent::AllocPNeckoParent() { return new NeckoParent(); }

bool ContentParent::DeallocPNeckoParent(PNeckoParent* necko) {
  delete necko;
  return true;
}

PPrintingParent* ContentParent::AllocPPrintingParent() {
#ifdef NS_PRINTING
  if (mPrintingParent) {
    // Only one PrintingParent should be created per process.
    return nullptr;
  }

  // Create the printing singleton for this process.
  mPrintingParent = new PrintingParent();

  // Take another reference for IPDL code.
  mPrintingParent.get()->AddRef();

  return mPrintingParent.get();
#else
  MOZ_ASSERT_UNREACHABLE("Should never be created if no printing.");
  return nullptr;
#endif
}

bool ContentParent::DeallocPPrintingParent(PPrintingParent* printing) {
#ifdef NS_PRINTING
  MOZ_RELEASE_ASSERT(
      mPrintingParent == printing,
      "Only one PrintingParent should have been created per process.");

  // Release reference taken for IPDL code.
  static_cast<PrintingParent*>(printing)->Release();

  mPrintingParent = nullptr;
#else
  MOZ_ASSERT_UNREACHABLE("Should never have been created if no printing.");
#endif
  return true;
}

#ifdef NS_PRINTING
already_AddRefed<embedding::PrintingParent> ContentParent::GetPrintingParent() {
  MOZ_ASSERT(mPrintingParent);

  RefPtr<embedding::PrintingParent> printingParent = mPrintingParent;
  return printingParent.forget();
}
#endif

mozilla::ipc::IPCResult ContentParent::RecvInitStreamFilter(
    const uint64_t& aChannelId, const nsString& aAddonId,
    InitStreamFilterResolver&& aResolver) {
  extensions::StreamFilterParent::Create(this, aChannelId, aAddonId)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aResolver](mozilla::ipc::Endpoint<PStreamFilterChild>&& aEndpoint) {
            aResolver(std::move(aEndpoint));
          },
          [aResolver](bool aDummy) {
            aResolver(mozilla::ipc::Endpoint<PStreamFilterChild>());
          });

  return IPC_OK();
}

PChildToParentStreamParent* ContentParent::AllocPChildToParentStreamParent() {
  return mozilla::ipc::AllocPChildToParentStreamParent();
}

bool ContentParent::DeallocPChildToParentStreamParent(
    PChildToParentStreamParent* aActor) {
  delete aActor;
  return true;
}

PParentToChildStreamParent* ContentParent::AllocPParentToChildStreamParent() {
  MOZ_CRASH(
      "PParentToChildStreamParent actors should be manually constructed!");
}

bool ContentParent::DeallocPParentToChildStreamParent(
    PParentToChildStreamParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult ContentParent::RecvAddSecurityState(
    const MaybeDiscarded<WindowContext>& aContext, uint32_t aStateFlags) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  aContext.get()->AddSecurityState(aStateFlags);
  return IPC_OK();
}

already_AddRefed<PExternalHelperAppParent>
ContentParent::AllocPExternalHelperAppParent(
    nsIURI* uri, const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
    const nsCString& aMimeContentType, const nsCString& aContentDisposition,
    const uint32_t& aContentDispositionHint,
    const nsString& aContentDispositionFilename, const bool& aForceSave,
    const int64_t& aContentLength, const bool& aWasFileChannel,
    nsIURI* aReferrer, const MaybeDiscarded<BrowsingContext>& aContext,
    const bool& aShouldCloseWindow) {
  RefPtr<ExternalHelperAppParent> parent = new ExternalHelperAppParent(
      uri, aContentLength, aWasFileChannel, aContentDisposition,
      aContentDispositionHint, aContentDispositionFilename);
  return parent.forget();
}

mozilla::ipc::IPCResult ContentParent::RecvPExternalHelperAppConstructor(
    PExternalHelperAppParent* actor, nsIURI* uri,
    const Maybe<LoadInfoArgs>& loadInfoArgs, const nsCString& aMimeContentType,
    const nsCString& aContentDisposition,
    const uint32_t& aContentDispositionHint,
    const nsString& aContentDispositionFilename, const bool& aForceSave,
    const int64_t& aContentLength, const bool& aWasFileChannel,
    nsIURI* aReferrer, const MaybeDiscarded<BrowsingContext>& aContext,
    const bool& aShouldCloseWindow) {
  BrowsingContext* context = aContext.IsDiscarded() ? nullptr : aContext.get();
  static_cast<ExternalHelperAppParent*>(actor)->Init(
      loadInfoArgs, aMimeContentType, aForceSave, aReferrer, context,
      aShouldCloseWindow);
  return IPC_OK();
}

already_AddRefed<PHandlerServiceParent>
ContentParent::AllocPHandlerServiceParent() {
  RefPtr<HandlerServiceParent> actor = new HandlerServiceParent();
  return actor.forget();
}

media::PMediaParent* ContentParent::AllocPMediaParent() {
  return media::AllocPMediaParent();
}

bool ContentParent::DeallocPMediaParent(media::PMediaParent* aActor) {
  return media::DeallocPMediaParent(aActor);
}

PBenchmarkStorageParent* ContentParent::AllocPBenchmarkStorageParent() {
  return new BenchmarkStorageParent;
}

bool ContentParent::DeallocPBenchmarkStorageParent(
    PBenchmarkStorageParent* aActor) {
  delete aActor;
  return true;
}

PPresentationParent* ContentParent::AllocPPresentationParent() {
  RefPtr<PresentationParent> actor = new PresentationParent();
  return actor.forget().take();
}

bool ContentParent::DeallocPPresentationParent(PPresentationParent* aActor) {
  RefPtr<PresentationParent> actor =
      dont_AddRef(static_cast<PresentationParent*>(aActor));
  return true;
}

mozilla::ipc::IPCResult ContentParent::RecvPPresentationConstructor(
    PPresentationParent* aActor) {
  if (!static_cast<PresentationParent*>(aActor)->Init(mChildID)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

#ifdef MOZ_WEBSPEECH
PSpeechSynthesisParent* ContentParent::AllocPSpeechSynthesisParent() {
  return new mozilla::dom::SpeechSynthesisParent();
}

bool ContentParent::DeallocPSpeechSynthesisParent(
    PSpeechSynthesisParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult ContentParent::RecvPSpeechSynthesisConstructor(
    PSpeechSynthesisParent* aActor) {
  if (!static_cast<SpeechSynthesisParent*>(aActor)->SendInit()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}
#endif

mozilla::ipc::IPCResult ContentParent::RecvStartVisitedQueries(
    const nsTArray<RefPtr<nsIURI>>& aUris) {
  nsCOMPtr<IHistory> history = components::History::Service();
  if (!history) {
    return IPC_OK();
  }
  for (const auto& params : aUris) {
    if (NS_WARN_IF(!params)) {
      continue;
    }
    history->RegisterVisitedCallback(params, nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetURITitle(nsIURI* uri,
                                                       const nsString& title) {
  if (!uri) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<IHistory> history = components::History::Service();
  if (history) {
    history->SetURITitle(uri, title);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvIsSecureURI(
    const uint32_t& aType, nsIURI* aURI, const uint32_t& aFlags,
    const OriginAttributes& aOriginAttributes, bool* aIsSecureURI) {
  nsCOMPtr<nsISiteSecurityService> sss(do_GetService(NS_SSSERVICE_CONTRACTID));
  if (!sss) {
    return IPC_FAIL_NO_REASON(this);
  }
  if (!aURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsresult rv = sss->IsSecureURI(aType, aURI, aFlags, aOriginAttributes,
                                 nullptr, nullptr, aIsSecureURI);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAccumulateMixedContentHSTS(
    nsIURI* aURI, const bool& aActive,
    const OriginAttributes& aOriginAttributes) {
  if (!aURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsMixedContentBlocker::AccumulateMixedContentHSTS(aURI, aActive,
                                                    aOriginAttributes);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvLoadURIExternal(
    nsIURI* uri, nsIPrincipal* aTriggeringPrincipal,
    const MaybeDiscarded<BrowsingContext>& aContext,
    bool aWasExternallyTriggered) {
  if (aContext.IsDiscarded()) {
    return IPC_OK();
  }

  nsCOMPtr<nsIExternalProtocolService> extProtService(
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  if (!extProtService) {
    return IPC_OK();
  }

  if (!uri) {
    return IPC_FAIL_NO_REASON(this);
  }

  BrowsingContext* bc = aContext.get();
  extProtService->LoadURI(uri, aTriggeringPrincipal, bc,
                          aWasExternallyTriggered);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvExtProtocolChannelConnectParent(
    const uint64_t& registrarId) {
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

mozilla::ipc::IPCResult ContentParent::RecvShowAlert(
    nsIAlertNotification* aAlert) {
  if (!aAlert) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIAlertsService> sysAlerts(components::Alerts::Service());
  if (sysAlerts) {
    sysAlerts->ShowAlert(aAlert, this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvCloseAlert(const nsString& aName) {
  nsCOMPtr<nsIAlertsService> sysAlerts(components::Alerts::Service());
  if (sysAlerts) {
    sysAlerts->CloseAlert(aName);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvDisableNotifications(
    const IPC::Principal& aPrincipal) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  Unused << Notification::RemovePermission(aPrincipal);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvOpenNotificationSettings(
    const IPC::Principal& aPrincipal) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  Unused << Notification::OpenSettings(aPrincipal);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotificationEvent(
    const nsString& aType, const NotificationEventData& aData) {
  nsCOMPtr<nsIServiceWorkerManager> swm =
      mozilla::components::ServiceWorkerManager::Service();
  if (NS_WARN_IF(!swm)) {
    // Probably shouldn't happen, but no need to crash the child process.
    return IPC_OK();
  }

  if (aType.EqualsLiteral("click")) {
    nsresult rv = swm->SendNotificationClickEvent(
        aData.originSuffix(), aData.scope(), aData.ID(), aData.title(),
        aData.dir(), aData.lang(), aData.body(), aData.tag(), aData.icon(),
        aData.data(), aData.behavior());
    Unused << NS_WARN_IF(NS_FAILED(rv));
  } else {
    MOZ_ASSERT(aType.EqualsLiteral("close"));
    nsresult rv = swm->SendNotificationCloseEvent(
        aData.originSuffix(), aData.scope(), aData.ID(), aData.title(),
        aData.dir(), aData.lang(), aData.body(), aData.tag(), aData.icon(),
        aData.data(), aData.behavior());
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSyncMessage(
    const nsString& aMsg, const ClonedMessageData& aData,
    nsTArray<StructuredCloneData>* aRetvals) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("ContentParent::RecvSyncMessage",
                                             OTHER, aMsg);
  MMPrinter::Print("ContentParent::RecvSyncMessage", aMsg, aData);

  RefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForParent(aData, data);

    ppm->ReceiveMessage(ppm, nullptr, aMsg, true, &data, aRetvals,
                        IgnoreErrors());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAsyncMessage(
    const nsString& aMsg, const ClonedMessageData& aData) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("ContentParent::RecvAsyncMessage",
                                             OTHER, aMsg);
  MMPrinter::Print("ContentParent::RecvAsyncMessage", aMsg, aData);

  RefPtr<nsFrameMessageManager> ppm = mMessageManager;
  if (ppm) {
    ipc::StructuredCloneData data;
    ipc::UnpackClonedMessageDataForParent(aData, data);

    ppm->ReceiveMessage(ppm, nullptr, aMsg, false, &data, nullptr,
                        IgnoreErrors());
  }
  return IPC_OK();
}

MOZ_CAN_RUN_SCRIPT
static int32_t AddGeolocationListener(
    nsIDOMGeoPositionCallback* watcher,
    nsIDOMGeoPositionErrorCallback* errorCallBack, bool highAccuracy) {
  RefPtr<Geolocation> geo = Geolocation::NonWindowSingleton();

  UniquePtr<PositionOptions> options = MakeUnique<PositionOptions>();
  options->mTimeout = 0;
  options->mMaximumAge = 0;
  options->mEnableHighAccuracy = highAccuracy;
  return geo->WatchPosition(watcher, errorCallBack, std::move(options));
}

mozilla::ipc::IPCResult ContentParent::RecvAddGeolocationListener(
    const bool& aHighAccuracy) {
  // To ensure no geolocation updates are skipped, we always force the
  // creation of a new listener.
  RecvRemoveGeolocationListener();
  mGeolocationWatchID = AddGeolocationListener(this, this, aHighAccuracy);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvRemoveGeolocationListener() {
  if (mGeolocationWatchID != -1) {
    RefPtr<Geolocation> geo = Geolocation::NonWindowSingleton();
    geo->ClearWatch(mGeolocationWatchID);
    mGeolocationWatchID = -1;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetGeolocationHigherAccuracy(
    const bool& aEnable) {
  // This should never be called without a listener already present,
  // so this check allows us to forgo securing privileges.
  if (mGeolocationWatchID != -1) {
    RecvRemoveGeolocationListener();
    mGeolocationWatchID = AddGeolocationListener(this, this, aEnable);
  }
  return IPC_OK();
}

NS_IMETHODIMP
ContentParent::HandleEvent(nsIDOMGeoPosition* postion) {
  Unused << SendGeolocationUpdate(postion);
  return NS_OK;
}

NS_IMETHODIMP
ContentParent::HandleEvent(GeolocationPositionError* positionError) {
  Unused << SendGeolocationError(positionError->Code());
  return NS_OK;
}

nsConsoleService* ContentParent::GetConsoleService() {
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

mozilla::ipc::IPCResult ContentParent::RecvConsoleMessage(
    const nsString& aMessage) {
  RefPtr<nsConsoleService> consoleService = GetConsoleService();
  if (!consoleService) {
    return IPC_OK();
  }

  RefPtr<nsConsoleMessage> msg(new nsConsoleMessage(aMessage.get()));
  msg->SetIsForwardedFromContentProcess(true);
  consoleService->LogMessageWithMode(msg, nsConsoleService::SuppressLog);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvReportFrameTimingData(
    uint64_t aInnerWindowId, const nsString& entryName,
    const nsString& initiatorType, UniquePtr<PerformanceTimingData>&& aData) {
  if (!aData) {
    return IPC_FAIL(this, "aData should not be null");
  }

  RefPtr<WindowGlobalParent> parent =
      WindowGlobalParent::GetByInnerWindowId(aInnerWindowId);
  if (!parent || !parent->GetContentParent()) {
    return IPC_OK();
  }

  MOZ_ASSERT(parent->GetContentParent() != this,
             "No need to bounce around if in the same process");

  Unused << parent->GetContentParent()->SendReportFrameTimingData(
      aInnerWindowId, entryName, initiatorType, std::move(aData));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvScriptError(
    const nsString& aMessage, const nsString& aSourceName,
    const nsString& aSourceLine, const uint32_t& aLineNumber,
    const uint32_t& aColNumber, const uint32_t& aFlags,
    const nsCString& aCategory, const bool& aFromPrivateWindow,
    const uint64_t& aInnerWindowId, const bool& aFromChromeContext) {
  return RecvScriptErrorInternal(aMessage, aSourceName, aSourceLine,
                                 aLineNumber, aColNumber, aFlags, aCategory,
                                 aFromPrivateWindow, aFromChromeContext);
}

mozilla::ipc::IPCResult ContentParent::RecvScriptErrorWithStack(
    const nsString& aMessage, const nsString& aSourceName,
    const nsString& aSourceLine, const uint32_t& aLineNumber,
    const uint32_t& aColNumber, const uint32_t& aFlags,
    const nsCString& aCategory, const bool& aFromPrivateWindow,
    const bool& aFromChromeContext, const ClonedMessageData& aFrame) {
  return RecvScriptErrorInternal(
      aMessage, aSourceName, aSourceLine, aLineNumber, aColNumber, aFlags,
      aCategory, aFromPrivateWindow, aFromChromeContext, &aFrame);
}

mozilla::ipc::IPCResult ContentParent::RecvScriptErrorInternal(
    const nsString& aMessage, const nsString& aSourceName,
    const nsString& aSourceLine, const uint32_t& aLineNumber,
    const uint32_t& aColNumber, const uint32_t& aFlags,
    const nsCString& aCategory, const bool& aFromPrivateWindow,
    const bool& aFromChromeContext, const ClonedMessageData* aStack) {
  RefPtr<nsConsoleService> consoleService = GetConsoleService();
  if (!consoleService) {
    return IPC_OK();
  }

  nsCOMPtr<nsIScriptError> msg;

  if (aStack) {
    StructuredCloneData data;
    UnpackClonedMessageDataForParent(*aStack, data);

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(xpc::PrivilegedJunkScope()))) {
      MOZ_CRASH();
    }
    JSContext* cx = jsapi.cx();

    JS::RootedValue stack(cx);
    ErrorResult rv;
    data.Read(cx, &stack, rv);
    if (rv.Failed() || !stack.isObject()) {
      rv.SuppressException();
      return IPC_OK();
    }

    JS::RootedObject stackObj(cx, &stack.toObject());
    MOZ_ASSERT(JS::IsUnwrappedSavedFrame(stackObj));

    JS::RootedObject stackGlobal(cx, JS::GetNonCCWObjectGlobal(stackObj));
    msg = new nsScriptErrorWithStack(JS::NothingHandleValue, stackObj,
                                     stackGlobal);
  } else {
    msg = new nsScriptError();
  }

  nsresult rv = msg->Init(aMessage, aSourceName, aSourceLine, aLineNumber,
                          aColNumber, aFlags, aCategory.get(),
                          aFromPrivateWindow, aFromChromeContext);
  if (NS_FAILED(rv)) return IPC_OK();

  msg->SetIsForwardedFromContentProcess(true);

  consoleService->LogMessageWithMode(msg, nsConsoleService::SuppressLog);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvPrivateDocShellsExist(
    const bool& aExist) {
  if (!sPrivateContent) {
    sPrivateContent = MakeUnique<nsTArray<ContentParent*>>();
    if (!sHasSeenPrivateDocShell) {
      sHasSeenPrivateDocShell = true;
      Telemetry::ScalarSet(
          Telemetry::ScalarID::DOM_PARENTPROCESS_PRIVATE_WINDOW_USED, true);
    }
  }
  if (aExist) {
    sPrivateContent->AppendElement(this);
  } else {
    sPrivateContent->RemoveElement(this);

    // Only fire the notification if we have private and non-private
    // windows: if privatebrowsing.autostart is true, all windows are
    // private.
    if (!sPrivateContent->Length() &&
        !Preferences::GetBool("browser.privatebrowsing.autostart")) {
      nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService();
      obs->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
      sPrivateContent = nullptr;
    }
  }
  return IPC_OK();
}

bool ContentParent::DoLoadMessageManagerScript(const nsAString& aURL,
                                               bool aRunInGlobalScope) {
  MOZ_ASSERT(!aRunInGlobalScope);
  return SendLoadProcessScript(nsString(aURL));
}

nsresult ContentParent::DoSendAsyncMessage(const nsAString& aMessage,
                                           StructuredCloneData& aHelper) {
  ClonedMessageData data;
  if (!BuildClonedMessageDataForParent(this, aHelper, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  if (!SendAsyncMessage(nsString(aMessage), data)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

mozilla::ipc::IPCResult ContentParent::RecvCopyFavicon(
    nsIURI* aOldURI, nsIURI* aNewURI, const bool& aInPrivateBrowsing) {
  if (!aOldURI) {
    return IPC_FAIL(this, "aOldURI should not be null");
  }
  if (!aNewURI) {
    return IPC_FAIL(this, "aNewURI should not be null");
  }

  nsDocShell::CopyFavicon(aOldURI, aNewURI, aInPrivateBrowsing);
  return IPC_OK();
}

bool ContentParent::ShouldContinueFromReplyTimeout() {
  RefPtr<ProcessHangMonitor> monitor = ProcessHangMonitor::Get();
  return !monitor || !monitor->ShouldTimeOutCPOWs();
}

mozilla::ipc::IPCResult ContentParent::RecvRecordingDeviceEvents(
    const nsString& aRecordingStatus, const nsString& aPageURL,
    const bool& aIsAudio, const bool& aIsVideo) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    // recording-device-ipc-events needs to gather more information from content
    // process
    RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
    props->SetPropertyAsUint64(u"childID"_ns, ChildID());
    props->SetPropertyAsBool(u"isAudio"_ns, aIsAudio);
    props->SetPropertyAsBool(u"isVideo"_ns, aIsVideo);
    props->SetPropertyAsAString(u"requestURL"_ns, aPageURL);

    obs->NotifyObservers((nsIPropertyBag2*)props, "recording-device-ipc-events",
                         aRecordingStatus.get());
  } else {
    NS_WARNING(
        "Could not get the Observer service for "
        "ContentParent::RecvRecordingDeviceEvents.");
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAddIdleObserver(
    const uint64_t& aObserver, const uint32_t& aIdleTimeInS) {
  nsresult rv;
  nsCOMPtr<nsIUserIdleService> idleService =
      do_GetService("@mozilla.org/widget/useridleservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  RefPtr<ParentIdleListener> listener =
      new ParentIdleListener(this, aObserver, aIdleTimeInS);
  rv = idleService->AddIdleObserver(listener, aIdleTimeInS);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));
  mIdleListeners.AppendElement(listener);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvRemoveIdleObserver(
    const uint64_t& aObserver, const uint32_t& aIdleTimeInS) {
  RefPtr<ParentIdleListener> listener;
  for (int32_t i = mIdleListeners.Length() - 1; i >= 0; --i) {
    listener = static_cast<ParentIdleListener*>(mIdleListeners[i].get());
    if (listener->mObserver == aObserver && listener->mTime == aIdleTimeInS) {
      nsresult rv;
      nsCOMPtr<nsIUserIdleService> idleService =
          do_GetService("@mozilla.org/widget/useridleservice;1", &rv);
      NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));
      idleService->RemoveIdleObserver(listener, aIdleTimeInS);
      mIdleListeners.RemoveElementAt(i);
      break;
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvBackUpXResources(
    const FileDescriptor& aXSocketFd) {
#ifndef MOZ_X11
  MOZ_CRASH("This message only makes sense on X11 platforms");
#else
  MOZ_ASSERT(0 > mChildXSocketFdDup.get(), "Already backed up X resources??");
  if (aXSocketFd.IsValid()) {
    auto rawFD = aXSocketFd.ClonePlatformHandle();
    mChildXSocketFdDup.reset(rawFD.release());
  }
#endif
  return IPC_OK();
}

class AnonymousTemporaryFileRequestor final : public Runnable {
 public:
  AnonymousTemporaryFileRequestor(ContentParent* aCP, const uint64_t& aID)
      : Runnable("dom::AnonymousTemporaryFileRequestor"),
        mCP(aCP),
        mID(aID),
        mRv(NS_OK),
        mPRFD(nullptr) {}

  NS_IMETHOD Run() override {
    if (NS_IsMainThread()) {
      FileDescOrError result;
      if (NS_WARN_IF(NS_FAILED(mRv))) {
        // Returning false will kill the child process; instead
        // propagate the error and let the child handle it.
        result = mRv;
      } else {
        result = FileDescriptor(FileDescriptor::PlatformHandleType(
            PR_FileDesc2NativeHandle(mPRFD)));
        // The FileDescriptor object owns a duplicate of the file handle; we
        // must close the original (and clean up the NSPR descriptor).
        PR_Close(mPRFD);
      }
      Unused << mCP->SendProvideAnonymousTemporaryFile(mID, result);
      // It's important to release this reference while wr're on the main
      // thread!
      mCP = nullptr;
    } else {
      mRv = NS_OpenAnonymousTemporaryFile(&mPRFD);
      NS_DispatchToMainThread(this);
    }
    return NS_OK;
  }

 private:
  RefPtr<ContentParent> mCP;
  uint64_t mID;
  nsresult mRv;
  PRFileDesc* mPRFD;
};

mozilla::ipc::IPCResult ContentParent::RecvRequestAnonymousTemporaryFile(
    const uint64_t& aID) {
  // Make sure to send a callback to the child if we bail out early.
  nsresult rv = NS_OK;
  RefPtr<ContentParent> self(this);
  auto autoNotifyChildOnError = MakeScopeExit([&, self]() {
    if (NS_FAILED(rv)) {
      FileDescOrError result(rv);
      Unused << self->SendProvideAnonymousTemporaryFile(aID, result);
    }
  });

  // We use a helper runnable to open the anonymous temporary file on the IO
  // thread.  The same runnable will call us back on the main thread when the
  // file has been opened.
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (!target) {
    return IPC_OK();
  }

  rv = target->Dispatch(new AnonymousTemporaryFileRequestor(this, aID),
                        NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

  rv = NS_OK;
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvCreateAudioIPCConnection(
    CreateAudioIPCConnectionResolver&& aResolver) {
  FileDescriptor fd = CubebUtils::CreateAudioIPCConnection();
  FileDescOrError result;
  if (fd.IsValid()) {
    result = fd;
  } else {
    result = NS_ERROR_FAILURE;
  }
  aResolver(std::move(result));
  return IPC_OK();
}

PFileDescriptorSetParent* ContentParent::AllocPFileDescriptorSetParent(
    const FileDescriptor& aFD) {
  return new FileDescriptorSetParent(aFD);
}

bool ContentParent::DeallocPFileDescriptorSetParent(
    PFileDescriptorSetParent* aActor) {
  delete static_cast<FileDescriptorSetParent*>(aActor);
  return true;
}

void ContentParent::NotifyUpdatedDictionaries() {
  RefPtr<mozSpellChecker> spellChecker(mozSpellChecker::Create());
  MOZ_ASSERT(spellChecker, "No spell checker?");

  nsTArray<nsCString> dictionaries;
  spellChecker->GetDictionaryList(&dictionaries);

  for (auto* cp : AllProcesses(eLive)) {
    Unused << cp->SendUpdateDictionaryList(dictionaries);
  }
}

void ContentParent::NotifyUpdatedFonts(bool aFullRebuild) {
  if (gfxPlatformFontList::PlatformFontList()->SharedFontList()) {
    for (auto* cp : AllProcesses(eLive)) {
      Unused << cp->SendRebuildFontList(aFullRebuild);
    }
    return;
  }

  nsTArray<SystemFontListEntry> fontList;
  gfxPlatform::GetPlatform()->ReadSystemFontList(&fontList);

  for (auto* cp : AllProcesses(eLive)) {
    Unused << cp->SendUpdateFontList(fontList);
  }
}

already_AddRefed<mozilla::docshell::POfflineCacheUpdateParent>
ContentParent::AllocPOfflineCacheUpdateParent(
    nsIURI* aManifestURI, nsIURI* aDocumentURI,
    const PrincipalInfo& aLoadingPrincipalInfo, const bool& aStickDocument,
    const CookieJarSettingsArgs& aCookieJarSettingsArgs) {
  RefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
      new mozilla::docshell::OfflineCacheUpdateParent();
  return update.forget();
}

mozilla::ipc::IPCResult ContentParent::RecvPOfflineCacheUpdateConstructor(
    POfflineCacheUpdateParent* aActor, nsIURI* aManifestURI,
    nsIURI* aDocumentURI, const PrincipalInfo& aLoadingPrincipal,
    const bool& aStickDocument,
    const CookieJarSettingsArgs& aCookieJarSettingsArgs) {
  MOZ_ASSERT(aActor);

  RefPtr<mozilla::docshell::OfflineCacheUpdateParent> update =
      static_cast<mozilla::docshell::OfflineCacheUpdateParent*>(aActor);

  nsresult rv = update->Schedule(aManifestURI, aDocumentURI, aLoadingPrincipal,
                                 aStickDocument, aCookieJarSettingsArgs);
  if (NS_FAILED(rv) && IsAlive()) {
    // Inform the child of failure.
    Unused << update->SendFinish(false, false);
  }

  return IPC_OK();
}

PWebrtcGlobalParent* ContentParent::AllocPWebrtcGlobalParent() {
#ifdef MOZ_WEBRTC
  return WebrtcGlobalParent::Alloc();
#else
  return nullptr;
#endif
}

bool ContentParent::DeallocPWebrtcGlobalParent(PWebrtcGlobalParent* aActor) {
#ifdef MOZ_WEBRTC
  WebrtcGlobalParent::Dealloc(static_cast<WebrtcGlobalParent*>(aActor));
  return true;
#else
  return false;
#endif
}

mozilla::ipc::IPCResult ContentParent::RecvSetOfflinePermission(
    const Principal& aPrincipal) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
      components::OfflineCacheUpdate::Service();
  if (!updateService) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsresult rv = updateService->AllowOfflineApp(aPrincipal);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  return IPC_OK();
}

void ContentParent::MaybeInvokeDragSession(BrowserParent* aParent) {
  // dnd uses IPCBlob to transfer data to the content process and the IPC
  // message is sent as normal priority. When sending input events with input
  // priority, the message may be preempted by the later dnd events. To make
  // sure the input events and the blob message are processed in time order
  // on the content process, we temporarily send the input events with normal
  // priority when there is an active dnd session.
  SetInputPriorityEventEnabled(false);

  nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService && dragService->MaybeAddChildProcess(this)) {
    // We need to send transferable data to child process.
    nsCOMPtr<nsIDragSession> session;
    dragService->GetCurrentSession(getter_AddRefs(session));
    if (session) {
      nsTArray<IPCDataTransfer> dataTransfers;
      RefPtr<DataTransfer> transfer = session->GetDataTransfer();
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
      nsCOMPtr<nsILoadContext> lc =
          aParent ? aParent->GetLoadContext() : nullptr;
      nsCOMPtr<nsIArray> transferables = transfer->GetTransferables(lc);
      nsContentUtils::TransferablesToIPCTransferables(
          transferables, dataTransfers, false, nullptr, this);
      uint32_t action;
      session->GetDragAction(&action);
      mozilla::Unused << SendInvokeDragSession(dataTransfers, action);
    }
  }
}

mozilla::ipc::IPCResult ContentParent::RecvUpdateDropEffect(
    const uint32_t& aDragAction, const uint32_t& aDropEffect) {
  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->SetDragAction(aDragAction);
    RefPtr<DataTransfer> dt = dragSession->GetDataTransfer();
    if (dt) {
      dt->SetDropEffectInt(aDropEffect);
    }
    dragSession->UpdateDragEffect();
  }
  return IPC_OK();
}

PContentPermissionRequestParent*
ContentParent::AllocPContentPermissionRequestParent(
    const nsTArray<PermissionRequest>& aRequests,
    const IPC::Principal& aPrincipal, const IPC::Principal& aTopLevelPrincipal,
    const bool& aIsHandlingUserInput,
    const bool& aMaybeUnsafePermissionDelegate, const TabId& aTabId) {
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  RefPtr<BrowserParent> tp =
      cpm->GetTopLevelBrowserParentByProcessAndTabId(this->ChildID(), aTabId);
  if (!tp) {
    return nullptr;
  }

  nsIPrincipal* topPrincipal = aTopLevelPrincipal;
  if (!topPrincipal) {
    nsCOMPtr<nsIPrincipal> principal = tp->GetContentPrincipal();
    topPrincipal = principal;
  }
  return nsContentPermissionUtils::CreateContentPermissionRequestParent(
      aRequests, tp->GetOwnerElement(), aPrincipal, topPrincipal,
      aIsHandlingUserInput, aMaybeUnsafePermissionDelegate, aTabId);
}

bool ContentParent::DeallocPContentPermissionRequestParent(
    PContentPermissionRequestParent* actor) {
  nsContentPermissionUtils::NotifyRemoveContentPermissionRequestParent(actor);
  delete actor;
  return true;
}

PWebBrowserPersistDocumentParent*
ContentParent::AllocPWebBrowserPersistDocumentParent(
    PBrowserParent* aBrowser, const MaybeDiscarded<BrowsingContext>& aContext) {
  return new WebBrowserPersistDocumentParent();
}

bool ContentParent::DeallocPWebBrowserPersistDocumentParent(
    PWebBrowserPersistDocumentParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult ContentParent::CommonCreateWindow(
    PBrowserParent* aThisTab, BrowsingContext* aParent, bool aSetOpener,
    const uint32_t& aChromeFlags, const bool& aCalledFromJS,
    const bool& aWidthSpecified, const bool& aForPrinting,
    const bool& aForWindowDotPrint, nsIURI* aURIToLoad,
    const nsCString& aFeatures, const float& aFullZoom,
    BrowserParent* aNextRemoteBrowser, const nsString& aName, nsresult& aResult,
    nsCOMPtr<nsIRemoteTab>& aNewRemoteTab, bool* aWindowIsNew,
    int32_t& aOpenLocation, nsIPrincipal* aTriggeringPrincipal,
    nsIReferrerInfo* aReferrerInfo, bool aLoadURI,
    nsIContentSecurityPolicy* aCsp, const OriginAttributes& aOriginAttributes) {
  // The content process should never be in charge of computing whether or
  // not a window should be private - the parent will do that.
  const uint32_t badFlags = nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW |
                            nsIWebBrowserChrome::CHROME_NON_PRIVATE_WINDOW |
                            nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  if (!!(aChromeFlags & badFlags)) {
    return IPC_FAIL(this, "Forbidden aChromeFlags passed");
  }

  RefPtr<nsOpenWindowInfo> openInfo = new nsOpenWindowInfo();
  openInfo->mForceNoOpener = !aSetOpener;
  openInfo->mParent = aParent;
  openInfo->mIsRemote = true;
  openInfo->mIsForPrinting = aForPrinting;
  openInfo->mIsForWindowDotPrint = aForWindowDotPrint;
  openInfo->mNextRemoteBrowser = aNextRemoteBrowser;
  openInfo->mOriginAttributes = aOriginAttributes;

  MOZ_ASSERT_IF(aForWindowDotPrint, aForPrinting);

  RefPtr<BrowserParent> topParent = BrowserParent::GetFrom(aThisTab);
  while (topParent && topParent->GetBrowserBridgeParent()) {
    topParent = topParent->GetBrowserBridgeParent()->Manager();
  }
  RefPtr<BrowserHost> thisBrowserHost =
      topParent ? topParent->GetBrowserHost() : nullptr;
  MOZ_ASSERT_IF(topParent, thisBrowserHost);
  RefPtr<BrowsingContext> topBC =
      topParent ? topParent->GetBrowsingContext() : nullptr;
  MOZ_ASSERT_IF(topParent, topBC);

  // The content process should have set its remote and fission flags correctly.
  if (topBC) {
    if ((!!(aChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW) !=
         topBC->UseRemoteTabs()) ||
        (!!(aChromeFlags & nsIWebBrowserChrome::CHROME_FISSION_WINDOW) !=
         topBC->UseRemoteSubframes())) {
      return IPC_FAIL(this, "Unexpected aChromeFlags passed");
    }

    if (!aOriginAttributes.EqualsIgnoringFPD(topBC->OriginAttributesRef())) {
      return IPC_FAIL(this, "Passed-in OriginAttributes does not match opener");
    }
  }

  nsCOMPtr<nsIContent> frame;
  if (topParent) {
    frame = topParent->GetOwnerElement();
  }

  nsCOMPtr<nsPIDOMWindowOuter> outerWin;
  if (frame) {
    outerWin = frame->OwnerDoc()->GetWindow();

    // If our chrome window is in the process of closing, don't try to open a
    // new tab in it.
    if (outerWin && outerWin->Closed()) {
      outerWin = nullptr;
    }
  }

  nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
  if (topParent) {
    browserDOMWin = topParent->GetBrowserDOMWindow();
  }

  // If we haven't found a chrome window to open in, just use the most recently
  // opened one.
  if (!outerWin) {
    outerWin = nsContentUtils::GetMostRecentNonPBWindow();
    if (NS_WARN_IF(!outerWin)) {
      aResult = NS_ERROR_FAILURE;
      return IPC_OK();
    }

    nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(outerWin);
    if (rootChromeWin) {
      rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    }
  }

  aOpenLocation = nsWindowWatcher::GetWindowOpenLocation(
      outerWin, aChromeFlags, aCalledFromJS, aWidthSpecified, aForPrinting);

  MOZ_ASSERT(aOpenLocation == nsIBrowserDOMWindow::OPEN_NEWTAB ||
             aOpenLocation == nsIBrowserDOMWindow::OPEN_NEWWINDOW ||
             aOpenLocation == nsIBrowserDOMWindow::OPEN_PRINT_BROWSER);

  if (NS_WARN_IF(!browserDOMWin)) {
    // Opening in the same window or headless requires an nsIBrowserDOMWindow.
    aOpenLocation = nsIBrowserDOMWindow::OPEN_NEWWINDOW;
  }

  if (aOpenLocation == nsIBrowserDOMWindow::OPEN_NEWTAB ||
      aOpenLocation == nsIBrowserDOMWindow::OPEN_PRINT_BROWSER) {
    RefPtr<Element> openerElement = do_QueryObject(frame);

    nsCOMPtr<nsIOpenURIInFrameParams> params =
        new nsOpenURIInFrameParams(openInfo, openerElement);
    params->SetReferrerInfo(aReferrerInfo);
    MOZ_ASSERT(aTriggeringPrincipal, "need a valid triggeringPrincipal");
    params->SetTriggeringPrincipal(aTriggeringPrincipal);
    params->SetCsp(aCsp);

    RefPtr<Element> el;

    if (aLoadURI) {
      aResult = browserDOMWin->OpenURIInFrame(aURIToLoad, params, aOpenLocation,
                                              nsIBrowserDOMWindow::OPEN_NEW,
                                              aName, getter_AddRefs(el));
    } else {
      aResult = browserDOMWin->CreateContentWindowInFrame(
          aURIToLoad, params, aOpenLocation, nsIBrowserDOMWindow::OPEN_NEW,
          aName, getter_AddRefs(el));
    }
    RefPtr<nsFrameLoaderOwner> frameLoaderOwner = do_QueryObject(el);
    if (NS_SUCCEEDED(aResult) && frameLoaderOwner) {
      RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
      if (frameLoader) {
        aNewRemoteTab = frameLoader->GetRemoteTab();
        // At this point, it's possible the inserted frameloader hasn't gone
        // through layout yet. To ensure that the dimensions that we send down
        // when telling the frameloader to display will be correct (instead of
        // falling back to a 10x10 default), we force layout if necessary to get
        // the most up-to-date dimensions. See bug 1358712 for details.
        frameLoader->ForceLayoutIfNecessary();
      }
    } else if (NS_SUCCEEDED(aResult) && !frameLoaderOwner) {
      // Fall through to the normal window opening code path when there is no
      // window which we can open a new tab in.
      aOpenLocation = nsIBrowserDOMWindow::OPEN_NEWWINDOW;
    } else {
      *aWindowIsNew = false;
    }

    // If we didn't retarget our window open into a new window, we should return
    // now.
    if (aOpenLocation != nsIBrowserDOMWindow::OPEN_NEWWINDOW) {
      return IPC_OK();
    }
  }

  nsCOMPtr<nsPIWindowWatcher> pwwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &aResult);
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return IPC_OK();
  }

  aResult = pwwatch->OpenWindowWithRemoteTab(thisBrowserHost, aFeatures,
                                             aCalledFromJS, aFullZoom, openInfo,
                                             getter_AddRefs(aNewRemoteTab));
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return IPC_OK();
  }

  MOZ_ASSERT(aNewRemoteTab);
  RefPtr<BrowserHost> newBrowserHost = BrowserHost::GetFrom(aNewRemoteTab);
  RefPtr<BrowserParent> newBrowserParent = newBrowserHost->GetActor();

  // At this point, it's possible the inserted frameloader hasn't gone through
  // layout yet. To ensure that the dimensions that we send down when telling
  // the frameloader to display will be correct (instead of falling back to a
  // 10x10 default), we force layout if necessary to get the most up-to-date
  // dimensions. See bug 1358712 for details.
  //
  // This involves doing a bit of gymnastics in order to get at the FrameLoader,
  // so we scope this to avoid polluting the main function scope.
  {
    nsCOMPtr<Element> frameElement = newBrowserHost->GetOwnerElement();
    MOZ_ASSERT(frameElement);
    RefPtr<nsFrameLoaderOwner> frameLoaderOwner = do_QueryObject(frameElement);
    MOZ_ASSERT(frameLoaderOwner);
    RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    MOZ_ASSERT(frameLoader);
    frameLoader->ForceLayoutIfNecessary();
  }

  // If we were passed a name for the window which would override the default,
  // we should send it down to the new tab.
  if (nsContentUtils::IsOverridingWindowName(aName)) {
    MOZ_ALWAYS_SUCCEEDS(newBrowserHost->GetBrowsingContext()->SetName(aName));
  }

  MOZ_ASSERT(newBrowserHost->GetBrowsingContext()->OriginAttributesRef() ==
             aOriginAttributes);

  if (aURIToLoad && aLoadURI) {
    nsCOMPtr<mozIDOMWindowProxy> openerWindow;
    if (aSetOpener && topParent) {
      openerWindow = topParent->GetParentWindowOuter();
    }
    nsCOMPtr<nsIBrowserDOMWindow> newBrowserDOMWin =
        newBrowserParent->GetBrowserDOMWindow();
    if (NS_WARN_IF(!newBrowserDOMWin)) {
      aResult = NS_ERROR_ABORT;
      return IPC_OK();
    }
    RefPtr<BrowsingContext> bc;
    aResult = newBrowserDOMWin->OpenURI(
        aURIToLoad, openInfo, nsIBrowserDOMWindow::OPEN_CURRENTWINDOW,
        nsIBrowserDOMWindow::OPEN_NEW, aTriggeringPrincipal, aCsp,
        getter_AddRefs(bc));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvCreateWindow(
    PBrowserParent* aThisTab, const MaybeDiscarded<BrowsingContext>& aParent,
    PBrowserParent* aNewTab, const uint32_t& aChromeFlags,
    const bool& aCalledFromJS, const bool& aWidthSpecified,
    const bool& aForPrinting, const bool& aForPrintPreview, nsIURI* aURIToLoad,
    const nsCString& aFeatures, const float& aFullZoom,
    const IPC::Principal& aTriggeringPrincipal, nsIContentSecurityPolicy* aCsp,
    nsIReferrerInfo* aReferrerInfo, const OriginAttributes& aOriginAttributes,
    CreateWindowResolver&& aResolve) {
  if (!ValidatePrincipal(aTriggeringPrincipal,
                         {ValidatePrincipalOptions::AllowSystem})) {
    LogAndAssertFailedPrincipalValidationInfo(aTriggeringPrincipal, __func__);
  }

  nsresult rv = NS_OK;
  CreatedWindowInfo cwi;

  // We always expect to open a new window here. If we don't, it's an error.
  cwi.windowOpened() = true;
  cwi.maxTouchPoints() = 0;
  cwi.hasSiblings() = false;

  // Make sure to resolve the resolver when this function exits, even if we
  // failed to generate a valid response.
  auto resolveOnExit = MakeScopeExit([&] {
    // Copy over the nsresult, and then resolve.
    cwi.rv() = rv;
    aResolve(cwi);
  });

  RefPtr<BrowserParent> thisTab = BrowserParent::GetFrom(aThisTab);
  RefPtr<BrowserParent> newTab = BrowserParent::GetFrom(aNewTab);
  MOZ_ASSERT(newTab);

  auto destroyNewTabOnError = MakeScopeExit([&] {
    // We always expect to open a new window here. If we don't, it's an error.
    if (!cwi.windowOpened() || NS_FAILED(rv)) {
      if (newTab) {
        newTab->Destroy();
      }
    }
  });

  // Don't continue to try to create a new window if we've been fully discarded.
  RefPtr<BrowsingContext> parent = aParent.GetMaybeDiscarded();
  if (NS_WARN_IF(!parent)) {
    rv = NS_ERROR_FAILURE;
    return IPC_OK();
  }

  // Validate that our new BrowsingContext looks as we would expect it.
  RefPtr<BrowsingContext> newBC = newTab->GetBrowsingContext();
  if (!newBC) {
    return IPC_FAIL(this, "Missing BrowsingContext for new tab");
  }

  uint64_t newBCOpenerId = newBC->GetOpenerId();
  if (newBCOpenerId != 0 && parent->Id() != newBCOpenerId) {
    return IPC_FAIL(this, "Invalid opener BrowsingContext for new tab");
  }
  if (newBC->GetParent() != nullptr) {
    return IPC_FAIL(this,
                    "Unexpected non-toplevel BrowsingContext for new tab");
  }
  if (!!(aChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW) !=
          newBC->UseRemoteTabs() ||
      !!(aChromeFlags & nsIWebBrowserChrome::CHROME_FISSION_WINDOW) !=
          newBC->UseRemoteSubframes()) {
    return IPC_FAIL(this, "Unexpected aChromeFlags passed");
  }
  if (!aOriginAttributes.EqualsIgnoringFPD(newBC->OriginAttributesRef())) {
    return IPC_FAIL(this, "Opened tab has mismatched OriginAttributes");
  }

  if (thisTab && BrowserParent::GetFrom(thisTab)->GetBrowsingContext()) {
    BrowsingContext* thisTabBC = thisTab->GetBrowsingContext();
    if (thisTabBC->UseRemoteTabs() != newBC->UseRemoteTabs() ||
        thisTabBC->UseRemoteSubframes() != newBC->UseRemoteSubframes() ||
        thisTabBC->UsePrivateBrowsing() != newBC->UsePrivateBrowsing()) {
      return IPC_FAIL(this, "New BrowsingContext has mismatched LoadContext");
    }
  }

  BrowserParent::AutoUseNewTab aunt(newTab);

  nsCOMPtr<nsIRemoteTab> newRemoteTab;
  int32_t openLocation = nsIBrowserDOMWindow::OPEN_NEWWINDOW;
  mozilla::ipc::IPCResult ipcResult = CommonCreateWindow(
      aThisTab, parent, newBCOpenerId != 0, aChromeFlags, aCalledFromJS,
      aWidthSpecified, aForPrinting, aForPrintPreview, aURIToLoad, aFeatures,
      aFullZoom, newTab, VoidString(), rv, newRemoteTab, &cwi.windowOpened(),
      openLocation, aTriggeringPrincipal, aReferrerInfo, /* aLoadUri = */ false,
      aCsp, aOriginAttributes);
  if (!ipcResult) {
    return ipcResult;
  }

  if (NS_WARN_IF(NS_FAILED(rv)) || !newRemoteTab) {
    return IPC_OK();
  }

  MOZ_ASSERT(BrowserHost::GetFrom(newRemoteTab.get()) ==
             newTab->GetBrowserHost());

  newTab->SwapFrameScriptsFrom(cwi.frameScripts());
  newTab->MaybeShowFrame();

  nsCOMPtr<nsIWidget> widget = newTab->GetWidget();
  if (widget) {
    cwi.dimensions() = newTab->GetDimensionInfo();
  }

  cwi.maxTouchPoints() = newTab->GetMaxTouchPoints();
  cwi.hasSiblings() = (openLocation == nsIBrowserDOMWindow::OPEN_NEWTAB);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvCreateWindowInDifferentProcess(
    PBrowserParent* aThisTab, const MaybeDiscarded<BrowsingContext>& aParent,
    const uint32_t& aChromeFlags, const bool& aCalledFromJS,
    const bool& aWidthSpecified, nsIURI* aURIToLoad, const nsCString& aFeatures,
    const float& aFullZoom, const nsString& aName,
    nsIPrincipal* aTriggeringPrincipal, nsIContentSecurityPolicy* aCsp,
    nsIReferrerInfo* aReferrerInfo, const OriginAttributes& aOriginAttributes) {
  MOZ_DIAGNOSTIC_ASSERT(!nsContentUtils::IsSpecialName(aName));

  // Don't continue to try to create a new window if we've been fully discarded.
  RefPtr<BrowsingContext> parent = aParent.GetMaybeDiscarded();
  if (NS_WARN_IF(!parent)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIRemoteTab> newRemoteTab;
  bool windowIsNew;
  int32_t openLocation = nsIBrowserDOMWindow::OPEN_NEWWINDOW;

  // If we have enough data, check the schemes of the loader and loadee
  // to make sure they make sense.
  if (aURIToLoad && aURIToLoad->SchemeIs("file") &&
      GetRemoteType() != FILE_REMOTE_TYPE &&
      Preferences::GetBool("browser.tabs.remote.enforceRemoteTypeRestrictions",
                           false)) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
#  ifdef DEBUG
    nsAutoCString uriToLoadStr;
    nsAutoCString triggeringUriStr;
    aURIToLoad->GetAsciiSpec(uriToLoadStr);
    aTriggeringPrincipal->GetAsciiSpec(triggeringUriStr);

    NS_WARNING(nsPrintfCString(
                   "RecvCreateWindowInDifferentProcess blocked loading file "
                   "scheme from non-file remotetype: %s tried to load %s",
                   triggeringUriStr.get(), uriToLoadStr.get())
                   .get());
#  endif
    MOZ_CRASH(
        "RecvCreateWindowInDifferentProcess blocked loading improper scheme");
#endif
    return IPC_OK();
  }

  nsresult rv;
  mozilla::ipc::IPCResult ipcResult = CommonCreateWindow(
      aThisTab, parent, /* aSetOpener = */ false, aChromeFlags, aCalledFromJS,
      aWidthSpecified, /* aForPrinting = */ false,
      /* aForPrintPreview = */ false, aURIToLoad, aFeatures, aFullZoom,
      /* aNextRemoteBrowser = */ nullptr, aName, rv, newRemoteTab, &windowIsNew,
      openLocation, aTriggeringPrincipal, aReferrerInfo,
      /* aLoadUri = */ true, aCsp, aOriginAttributes);
  if (!ipcResult) {
    return ipcResult;
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Call to CommonCreateWindow failed.");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvShutdownProfile(
    const nsCString& aProfile) {
#ifdef MOZ_GECKO_PROFILER
  nsCOMPtr<nsIProfiler> profiler(
      do_GetService("@mozilla.org/tools/profiler;1"));
  profiler->ReceiveShutdownProfile(aProfile);
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetGraphicsDeviceInitData(
    ContentDeviceData* aOut) {
  gfxPlatform::GetPlatform()->BuildContentDeviceData(aOut);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetOutputColorProfileData(
    nsTArray<uint8_t>* aOutputColorProfileData) {
  (*aOutputColorProfileData) =
      gfxPlatform::GetPlatform()->GetPlatformCMSOutputProfileData();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetFontListShmBlock(
    const uint32_t& aGeneration, const uint32_t& aIndex,
    base::SharedMemoryHandle* aOut) {
  auto* fontList = gfxPlatformFontList::PlatformFontList();
  MOZ_RELEASE_ASSERT(fontList, "gfxPlatformFontList not initialized?");
  fontList->ShareFontListShmBlockToProcess(aGeneration, aIndex, Pid(), aOut);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvInitializeFamily(
    const uint32_t& aGeneration, const uint32_t& aFamilyIndex,
    const bool& aLoadCmaps) {
  auto* fontList = gfxPlatformFontList::PlatformFontList();
  MOZ_RELEASE_ASSERT(fontList, "gfxPlatformFontList not initialized?");
  fontList->InitializeFamily(aGeneration, aFamilyIndex, aLoadCmaps);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetCharacterMap(
    const uint32_t& aGeneration, const mozilla::fontlist::Pointer& aFacePtr,
    const gfxSparseBitSet& aMap) {
  auto* fontList = gfxPlatformFontList::PlatformFontList();
  MOZ_RELEASE_ASSERT(fontList, "gfxPlatformFontList not initialized?");
  fontList->SetCharacterMap(aGeneration, aFacePtr, aMap);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvInitOtherFamilyNames(
    const uint32_t& aGeneration, const bool& aDefer, bool* aLoaded) {
  auto* fontList = gfxPlatformFontList::PlatformFontList();
  MOZ_RELEASE_ASSERT(fontList, "gfxPlatformFontList not initialized?");
  *aLoaded = fontList->InitOtherFamilyNames(aGeneration, aDefer);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetupFamilyCharMap(
    const uint32_t& aGeneration, const mozilla::fontlist::Pointer& aFamilyPtr) {
  auto* fontList = gfxPlatformFontList::PlatformFontList();
  MOZ_RELEASE_ASSERT(fontList, "gfxPlatformFontList not initialized?");
  fontList->SetupFamilyCharMap(aGeneration, aFamilyPtr);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvStartCmapLoading(
    const uint32_t& aGeneration, const uint32_t& aStartIndex) {
  auto* fontList = gfxPlatformFontList::PlatformFontList();
  MOZ_RELEASE_ASSERT(fontList, "gfxPlatformFontList not initialized?");
  fontList->StartCmapLoading(aGeneration, aStartIndex);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetHyphDict(
    nsIURI* aURI, base::SharedMemoryHandle* aOutHandle, uint32_t* aOutSize) {
  if (!aURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsHyphenationManager::Instance()->ShareHyphDictToProcess(
      aURI, Pid(), aOutHandle, aOutSize);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGraphicsError(
    const nsCString& aError) {
  gfx::LogForwarder* lf = gfx::Factory::GetLogForwarder();
  if (lf) {
    std::stringstream message;
    message << "CP+" << aError.get();
    lf->UpdateStringsVector(message.str());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvBeginDriverCrashGuard(
    const uint32_t& aGuardType, bool* aOutCrashed) {
  // Only one driver crash guard should be active at a time, per-process.
  MOZ_ASSERT(!mDriverCrashGuard);

  UniquePtr<gfx::DriverCrashGuard> guard;
  switch (gfx::CrashGuardType(aGuardType)) {
    case gfx::CrashGuardType::D3D11Layers:
      guard = MakeUnique<gfx::D3D11LayersCrashGuard>(this);
      break;
    case gfx::CrashGuardType::GLContext:
      guard = MakeUnique<gfx::GLContextCrashGuard>(this);
      break;
    case gfx::CrashGuardType::WMFVPXVideo:
      guard = MakeUnique<gfx::WMFVPXVideoCrashGuard>(this);
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
  mDriverCrashGuard = std::move(guard);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvEndDriverCrashGuard(
    const uint32_t& aGuardType) {
  mDriverCrashGuard = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyBenchmarkResult(
    const nsString& aCodecName, const uint32_t& aDecodeFPS)

{
  if (aCodecName.EqualsLiteral("VP9")) {
    Preferences::SetUint(VP9Benchmark::sBenchmarkFpsPref, aDecodeFPS);
    Preferences::SetUint(VP9Benchmark::sBenchmarkFpsVersionCheck,
                         VP9Benchmark::sBenchmarkVersionID);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyPushObservers(
    const nsCString& aScope, const IPC::Principal& aPrincipal,
    const nsString& aMessageId) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Nothing());
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyPushObserversWithData(
    const nsCString& aScope, const IPC::Principal& aPrincipal,
    const nsString& aMessageId, nsTArray<uint8_t>&& aData) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId,
                                   Some(std::move(aData)));
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyPushSubscriptionChangeObservers(
    const nsCString& aScope, const IPC::Principal& aPrincipal) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  PushSubscriptionChangeDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvPushError(
    const nsCString& aScope, const IPC::Principal& aPrincipal,
    const nsString& aMessage, const uint32_t& aFlags) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  PushErrorDispatcher dispatcher(aScope, aPrincipal, aMessage, aFlags);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyPushSubscriptionModifiedObservers(
    const nsCString& aScope, const IPC::Principal& aPrincipal) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  PushSubscriptionModifiedDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObservers()));
  return IPC_OK();
}

/* static */
void ContentParent::BroadcastBlobURLRegistration(
    const nsACString& aURI, BlobImpl* aBlobImpl, nsIPrincipal* aPrincipal,
    const Maybe<nsID>& aAgentClusterId, ContentParent* aIgnoreThisCP) {
  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  NS_ENSURE_SUCCESS_VOID(rv);

  uint64_t originHash = ComputeLoadedOriginHash(aPrincipal);

  bool toBeSent = StringBeginsWith(origin, "moz-extension://"_ns) ||
                  aPrincipal->IsSystemPrincipal();

  nsCString uri(aURI);
  IPC::Principal principal(aPrincipal);

  for (auto* cp : AllProcesses(eLive)) {
    if (cp != aIgnoreThisCP) {
      if (!toBeSent && !cp->mLoadedOriginHashes.Contains(originHash)) {
        continue;
      }

      nsresult rv = cp->TransmitPermissionsForPrincipal(principal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }

      IPCBlob ipcBlob;
      rv = IPCBlobUtils::Serialize(aBlobImpl, cp, ipcBlob);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }

      Unused << cp->SendBlobURLRegistration(uri, ipcBlob, principal,
                                            aAgentClusterId);
    }
  }
}

/* static */
void ContentParent::BroadcastBlobURLUnregistration(
    const nsACString& aURI, nsIPrincipal* aPrincipal,
    ContentParent* aIgnoreThisCP) {
  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  NS_ENSURE_SUCCESS_VOID(rv);

  uint64_t originHash = ComputeLoadedOriginHash(aPrincipal);

  bool toBeSent = StringBeginsWith(origin, "moz-extension://"_ns) ||
                  aPrincipal->IsSystemPrincipal();

  nsCString uri(aURI);

  for (auto* cp : AllProcesses(eLive)) {
    if (cp != aIgnoreThisCP &&
        (toBeSent || cp->mLoadedOriginHashes.Contains(originHash))) {
      Unused << cp->SendBlobURLUnregistration(uri);
    }
  }
}

mozilla::ipc::IPCResult ContentParent::RecvStoreAndBroadcastBlobURLRegistration(
    const nsCString& aURI, const IPCBlob& aBlob, const Principal& aPrincipal,
    const Maybe<nsID>& aAgentClusterId) {
  if (!ValidatePrincipal(aPrincipal, {ValidatePrincipalOptions::AllowSystem})) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(aBlob);
  if (NS_WARN_IF(!blobImpl)) {
    return IPC_FAIL_NO_REASON(this);
  }

  BlobURLProtocolHandler::AddDataEntry(aURI, aPrincipal, aAgentClusterId,
                                       blobImpl);
  BroadcastBlobURLRegistration(aURI, blobImpl, aPrincipal, aAgentClusterId,
                               this);

  // We want to store this blobURL, so we can unregister it if the child
  // crashes.
  mBlobURLs.AppendElement(aURI);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvUnstoreAndBroadcastBlobURLUnregistration(
    const nsCString& aURI, const Principal& aPrincipal) {
  if (!ValidatePrincipal(aPrincipal, {ValidatePrincipalOptions::AllowSystem})) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  BlobURLProtocolHandler::RemoveDataEntry(aURI, false /* Don't broadcast */);
  BroadcastBlobURLUnregistration(aURI, aPrincipal, this);
  mBlobURLs.RemoveElement(aURI);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetA11yContentId(
    uint32_t* aContentId) {
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  *aContentId = a11y::AccessibleWrap::GetContentProcessIdFor(ChildID());
  MOZ_ASSERT(*aContentId);
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult ContentParent::RecvA11yHandlerControl(
    const uint32_t& aPid, const IHandlerControlHolder& aHandlerControl) {
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  MOZ_ASSERT(!aHandlerControl.IsNull());
  RefPtr<IHandlerControl> proxy(aHandlerControl.Get());
  a11y::AccessibleWrap::SetHandlerControl(aPid, std::move(proxy));
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

bool ContentParent::HandleWindowsMessages(const Message& aMsg) const {
  MOZ_ASSERT(aMsg.is_sync());

#ifdef ACCESSIBILITY
  // a11y messages can be triggered by windows messages, which means if we
  // allow handling windows messages while we wait for the response to a sync
  // a11y message we can reenter the ipc message sending code.
  if (a11y::PDocAccessible::PDocAccessibleStart < aMsg.type() &&
      a11y::PDocAccessible::PDocAccessibleEnd > aMsg.type()) {
    return false;
  }
#endif

  return true;
}

mozilla::ipc::IPCResult ContentParent::RecvGetFilesRequest(
    const nsID& aUUID, const nsString& aDirectoryPath,
    const bool& aRecursiveFlag) {
  MOZ_ASSERT(!mGetFilesPendingRequests.GetWeak(aUUID));

  if (!mozilla::Preferences::GetBool("dom.filesystem.pathcheck.disabled",
                                     false)) {
    RefPtr<FileSystemSecurity> fss = FileSystemSecurity::Get();
    if (NS_WARN_IF(!fss || !fss->ContentProcessHasAccessTo(ChildID(),
                                                           aDirectoryPath))) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  ErrorResult rv;
  RefPtr<GetFilesHelper> helper = GetFilesHelperParent::Create(
      aUUID, aDirectoryPath, aRecursiveFlag, this, rv);

  if (NS_WARN_IF(rv.Failed())) {
    if (!SendGetFilesResponse(aUUID,
                              GetFilesResponseFailure(rv.StealNSResult()))) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  mGetFilesPendingRequests.InsertOrUpdate(aUUID, std::move(helper));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvDeleteGetFilesRequest(
    const nsID& aUUID) {
  mGetFilesPendingRequests.Remove(aUUID);
  return IPC_OK();
}

void ContentParent::SendGetFilesResponseAndForget(
    const nsID& aUUID, const GetFilesResponseResult& aResult) {
  if (mGetFilesPendingRequests.Remove(aUUID)) {
    Unused << SendGetFilesResponse(aUUID, aResult);
  }
}

void ContentParent::PaintTabWhileInterruptingJS(
    BrowserParent* aBrowserParent, const layers::LayersObserverEpoch& aEpoch) {
  if (!mHangMonitorActor) {
    return;
  }
  ProcessHangMonitor::PaintWhileInterruptingJS(mHangMonitorActor,
                                               aBrowserParent, aEpoch);
}

void ContentParent::CancelContentJSExecutionIfRunning(
    BrowserParent* aBrowserParent, nsIRemoteTab::NavigationType aNavigationType,
    const CancelContentJSOptions& aCancelContentJSOptions) {
  if (!mHangMonitorActor) {
    return;
  }

  ProcessHangMonitor::CancelContentJSExecutionIfRunning(
      mHangMonitorActor, aBrowserParent, aNavigationType,
      aCancelContentJSOptions);
}

void ContentParent::UpdateCookieStatus(nsIChannel* aChannel) {
  PNeckoParent* neckoParent = LoneManagedOrNullAsserts(ManagedPNeckoParent());
  PCookieServiceParent* csParent =
      LoneManagedOrNullAsserts(neckoParent->ManagedPCookieServiceParent());
  if (csParent) {
    auto* cs = static_cast<CookieServiceParent*>(csParent);
    cs->TrackCookieLoad(aChannel);
  }
}

nsresult ContentParent::AboutToLoadHttpFtpDocumentForChild(
    nsIChannel* aChannel, bool* aShouldWaitForPermissionCookieUpdate) {
  MOZ_ASSERT(aChannel);

  if (aShouldWaitForPermissionCookieUpdate) {
    *aShouldWaitForPermissionCookieUpdate = false;
  }

  nsresult rv;
  bool isDocument = aChannel->IsDocument();
  if (!isDocument) {
    // We may be looking at a nsIHttpChannel which has isMainDocumentChannel set
    // (e.g. the internal http channel for a view-source: load.).
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
    if (httpChannel) {
      rv = httpChannel->GetIsMainDocumentChannel(&isDocument);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  if (!isDocument) {
    return NS_OK;
  }

  // Get the principal for the channel result, so that we can get the permission
  // key for the document which will be created from this response.
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (NS_WARN_IF(!ssm)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Let the caller know we're going to send main thread IPC for updating
  // permisssions/cookies.
  if (aShouldWaitForPermissionCookieUpdate) {
    *aShouldWaitForPermissionCookieUpdate = true;
  }

  TransmitBlobURLsForPrincipal(principal);

  rv = TransmitPermissionsForPrincipal(principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsLoadFlags newLoadFlags;
  aChannel->GetLoadFlags(&newLoadFlags);
  if (newLoadFlags & nsIRequest::LOAD_DOCUMENT_NEEDS_COOKIE) {
    UpdateCookieStatus(aChannel);
  }

  RefPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  RefPtr<BrowsingContext> browsingContext;
  rv = loadInfo->GetTargetBrowsingContext(getter_AddRefs(browsingContext));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!NextGenLocalStorageEnabled()) {
    return NS_OK;
  }

  if (principal->GetIsContentPrincipal()) {
    nsCOMPtr<nsILocalStorageManager> lsm =
        do_GetService("@mozilla.org/dom/localStorage-manager;1");
    if (NS_WARN_IF(!lsm)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIPrincipal> storagePrincipal;
    rv = ssm->GetChannelResultStoragePrincipal(
        aChannel, getter_AddRefs(storagePrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<Promise> dummy;
    rv = lsm->Preload(storagePrincipal, nullptr, getter_AddRefs(dummy));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to preload local storage!");
    }
  }

  return NS_OK;
}

nsresult ContentParent::TransmitPermissionsForPrincipal(
    nsIPrincipal* aPrincipal) {
  // Create the key, and send it down to the content process.
  nsTArray<std::pair<nsCString, nsCString>> pairs =
      PermissionManager::GetAllKeysForPrincipal(aPrincipal);
  MOZ_ASSERT(pairs.Length() >= 1);
  for (auto& pair : pairs) {
    EnsurePermissionsByKey(pair.first, pair.second);
  }

  return NS_OK;
}

void ContentParent::TransmitBlobURLsForPrincipal(nsIPrincipal* aPrincipal) {
  uint64_t originHash = ComputeLoadedOriginHash(aPrincipal);

  if (!mLoadedOriginHashes.Contains(originHash)) {
    mLoadedOriginHashes.AppendElement(originHash);

    nsTArray<BlobURLRegistrationData> registrations;
    BlobURLProtocolHandler::ForEachBlobURL(
        [&](BlobImpl* aBlobImpl, nsIPrincipal* aBlobPrincipal,
            const Maybe<nsID>& aAgentClusterId, const nsACString& aURI,
            bool aRevoked) {
          if (!aPrincipal->Subsumes(aBlobPrincipal)) {
            return true;
          }

          IPCBlob ipcBlob;
          nsresult rv = IPCBlobUtils::Serialize(aBlobImpl, this, ipcBlob);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return false;
          }

          registrations.AppendElement(BlobURLRegistrationData(
              nsCString(aURI), ipcBlob, aPrincipal, aAgentClusterId, aRevoked));

          rv = TransmitPermissionsForPrincipal(aPrincipal);
          Unused << NS_WARN_IF(NS_FAILED(rv));
          return true;
        });

    if (!registrations.IsEmpty()) {
      Unused << SendInitBlobURLs(registrations);
    }
  }
}

void ContentParent::TransmitBlobDataIfBlobURL(nsIURI* aURI,
                                              nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aPrincipal);

  if (!IsBlobURI(aURI)) {
    return;
  }

  TransmitBlobURLsForPrincipal(aPrincipal);
}

void ContentParent::EnsurePermissionsByKey(const nsCString& aKey,
                                           const nsCString& aOrigin) {
  // NOTE: Make sure to initialize the permission manager before updating the
  // mActivePermissionKeys list. If the permission manager is being initialized
  // by this call to GetPermissionManager, and we've added the key to
  // mActivePermissionKeys, then the permission manager will send down a
  // SendAddPermission before receiving the SendSetPermissionsWithKey message.
  RefPtr<PermissionManager> permManager = PermissionManager::GetInstance();
  if (!permManager) {
    return;
  }

  if (mActivePermissionKeys.Contains(aKey)) {
    return;
  }
  mActivePermissionKeys.PutEntry(aKey);

  nsTArray<IPC::Permission> perms;
  if (permManager->GetPermissionsFromOriginOrKey(aOrigin, aKey, perms)) {
    Unused << SendSetPermissionsWithKey(aKey, perms);
  }
}

bool ContentParent::NeedsPermissionsUpdate(
    const nsACString& aPermissionKey) const {
  return mActivePermissionKeys.Contains(aPermissionKey);
}

mozilla::ipc::IPCResult ContentParent::RecvAccumulateChildHistograms(
    nsTArray<HistogramAccumulation>&& aAccumulations) {
  TelemetryIPC::AccumulateChildHistograms(GetTelemetryProcessID(mRemoteType),
                                          aAccumulations);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAccumulateChildKeyedHistograms(
    nsTArray<KeyedHistogramAccumulation>&& aAccumulations) {
  TelemetryIPC::AccumulateChildKeyedHistograms(
      GetTelemetryProcessID(mRemoteType), aAccumulations);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvUpdateChildScalars(
    nsTArray<ScalarAction>&& aScalarActions) {
  TelemetryIPC::UpdateChildScalars(GetTelemetryProcessID(mRemoteType),
                                   aScalarActions);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvUpdateChildKeyedScalars(
    nsTArray<KeyedScalarAction>&& aScalarActions) {
  TelemetryIPC::UpdateChildKeyedScalars(GetTelemetryProcessID(mRemoteType),
                                        aScalarActions);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvRecordChildEvents(
    nsTArray<mozilla::Telemetry::ChildEventData>&& aEvents) {
  TelemetryIPC::RecordChildEvents(GetTelemetryProcessID(mRemoteType), aEvents);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvRecordDiscardedData(
    const mozilla::Telemetry::DiscardedData& aDiscardedData) {
  TelemetryIPC::RecordDiscardedData(GetTelemetryProcessID(mRemoteType),
                                    aDiscardedData);
  return IPC_OK();
}

//////////////////////////////////////////////////////////////////
// PURLClassifierParent

PURLClassifierParent* ContentParent::AllocPURLClassifierParent(
    const Principal& aPrincipal, bool* aSuccess) {
  MOZ_ASSERT(NS_IsMainThread());

  *aSuccess = true;
  RefPtr<URLClassifierParent> actor = new URLClassifierParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult ContentParent::RecvPURLClassifierConstructor(
    PURLClassifierParent* aActor, const Principal& aPrincipal, bool* aSuccess) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);
  *aSuccess = false;

  auto* actor = static_cast<URLClassifierParent*>(aActor);
  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  if (!principal) {
    actor->ClassificationFailed();
    return IPC_OK();
  }
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  return actor->StartClassify(principal, aSuccess);
}

bool ContentParent::DeallocPURLClassifierParent(PURLClassifierParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  RefPtr<URLClassifierParent> actor =
      dont_AddRef(static_cast<URLClassifierParent*>(aActor));
  return true;
}

//////////////////////////////////////////////////////////////////
// PURLClassifierLocalParent

PURLClassifierLocalParent* ContentParent::AllocPURLClassifierLocalParent(
    nsIURI* aURI, const nsTArray<IPCURLClassifierFeature>& aFeatures) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<URLClassifierLocalParent> actor = new URLClassifierLocalParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult ContentParent::RecvPURLClassifierLocalConstructor(
    PURLClassifierLocalParent* aActor, nsIURI* aURI,
    nsTArray<IPCURLClassifierFeature>&& aFeatures) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  nsTArray<IPCURLClassifierFeature> features = std::move(aFeatures);

  if (!aURI) {
    NS_WARNING("aURI should not be null");
    return IPC_FAIL_NO_REASON(this);
  }

  auto* actor = static_cast<URLClassifierLocalParent*>(aActor);
  return actor->StartClassify(aURI, features);
}

bool ContentParent::DeallocPURLClassifierLocalParent(
    PURLClassifierLocalParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  RefPtr<URLClassifierLocalParent> actor =
      dont_AddRef(static_cast<URLClassifierLocalParent*>(aActor));
  return true;
}

PLoginReputationParent* ContentParent::AllocPLoginReputationParent(
    nsIURI* aURI) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<LoginReputationParent> actor = new LoginReputationParent();
  return actor.forget().take();
}

mozilla::ipc::IPCResult ContentParent::RecvPLoginReputationConstructor(
    PLoginReputationParent* aActor, nsIURI* aURI) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  if (!aURI) {
    return IPC_FAIL_NO_REASON(this);
  }

  auto* actor = static_cast<LoginReputationParent*>(aActor);
  return actor->QueryReputation(aURI);
}

bool ContentParent::DeallocPLoginReputationParent(
    PLoginReputationParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  RefPtr<LoginReputationParent> actor =
      dont_AddRef(static_cast<LoginReputationParent*>(aActor));
  return true;
}

PSessionStorageObserverParent*
ContentParent::AllocPSessionStorageObserverParent() {
  MOZ_ASSERT(NS_IsMainThread());

  return mozilla::dom::AllocPSessionStorageObserverParent();
}

mozilla::ipc::IPCResult ContentParent::RecvPSessionStorageObserverConstructor(
    PSessionStorageObserverParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::RecvPSessionStorageObserverConstructor(aActor)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool ContentParent::DeallocPSessionStorageObserverParent(
    PSessionStorageObserverParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  return mozilla::dom::DeallocPSessionStorageObserverParent(aActor);
}

mozilla::ipc::IPCResult ContentParent::RecvMaybeReloadPlugins() {
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  pluginHost->ReloadPlugins();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvDeviceReset() {
  GPUProcessManager* pm = GPUProcessManager::Get();
  if (pm) {
    pm->SimulateDeviceReset();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvBHRThreadHang(
    const HangDetails& aDetails) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    // Copy the HangDetails recieved over the network into a nsIHangDetails, and
    // then fire our own observer notification.
    // XXX: We should be able to avoid this potentially expensive copy here by
    // moving our deserialized argument.
    nsCOMPtr<nsIHangDetails> hangDetails =
        new nsHangDetails(HangDetails(aDetails), PersistedToDisk::No);
    obs->NotifyObservers(hangDetails, "bhr-thread-hang", nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAddCertException(
    const nsACString& aSerializedCert, uint32_t aFlags,
    const nsACString& aHostName, int32_t aPort, bool aIsTemporary,
    AddCertExceptionResolver&& aResolver) {
  nsCOMPtr<nsISupports> certObj;
  nsresult rv = NS_DeserializeObject(aSerializedCert, getter_AddRefs(certObj));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIX509Cert> cert = do_QueryInterface(certObj);
    if (!cert) {
      rv = NS_ERROR_INVALID_ARG;
    } else {
      nsCOMPtr<nsICertOverrideService> overrideService =
          do_GetService(NS_CERTOVERRIDE_CONTRACTID);
      if (!overrideService) {
        rv = NS_ERROR_FAILURE;
      } else {
        rv = overrideService->RememberValidityOverride(aHostName, aPort, cert,
                                                       aFlags, aIsTemporary);
      }
    }
  }
  aResolver(rv);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvAutomaticStorageAccessPermissionCanBeGranted(
    const Principal& aPrincipal,
    AutomaticStorageAccessPermissionCanBeGrantedResolver&& aResolver) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  aResolver(Document::AutomaticStorageAccessPermissionCanBeGranted(aPrincipal));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvStorageAccessPermissionGrantedForOrigin(
    uint64_t aTopLevelWindowId,
    const MaybeDiscarded<BrowsingContext>& aParentContext,
    const Principal& aTrackingPrincipal, const nsCString& aTrackingOrigin,
    const int& aAllowMode,
    const Maybe<ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
        aReason,
    StorageAccessPermissionGrantedForOriginResolver&& aResolver) {
  if (aParentContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  // We only report here if we cannot report the console directly in the content
  // process. In that case, the `aReason` would be given a value. Otherwise, it
  // will be nothing.
  if (aReason) {
    ContentBlockingNotifier::ReportUnblockingToConsole(
        aParentContext.get_canonical(), NS_ConvertUTF8toUTF16(aTrackingOrigin),
        aReason.value());
  }

  ContentBlocking::SaveAccessForOriginOnParentProcess(
      aTopLevelWindowId, aParentContext.get_canonical(), aTrackingPrincipal,
      aTrackingOrigin, aAllowMode)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aResolver = std::move(aResolver)](
              ContentBlocking::ParentAccessGrantPromise::ResolveOrRejectValue&&
                  aValue) {
            bool success =
                aValue.IsResolve() && NS_SUCCEEDED(aValue.ResolveValue());
            aResolver(success);
          });
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvCompleteAllowAccessFor(
    const MaybeDiscarded<BrowsingContext>& aParentContext,
    uint64_t aTopLevelWindowId, const Principal& aTrackingPrincipal,
    const nsCString& aTrackingOrigin, uint32_t aCookieBehavior,
    const ContentBlockingNotifier::StorageAccessPermissionGrantedReason&
        aReason,
    CompleteAllowAccessForResolver&& aResolver) {
  if (aParentContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  ContentBlocking::CompleteAllowAccessFor(
      aParentContext.get_canonical(), aTopLevelWindowId, aTrackingPrincipal,
      aTrackingOrigin, aCookieBehavior, aReason, nullptr)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver = std::move(aResolver)](
                 ContentBlocking::StorageAccessPermissionGrantPromise::
                     ResolveOrRejectValue&& aValue) {
               Maybe<StorageAccessPromptChoices> choice;
               if (aValue.IsResolve()) {
                 choice.emplace(static_cast<StorageAccessPromptChoices>(
                     aValue.ResolveValue()));
               }
               aResolver(choice);
             });
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvStoreUserInteractionAsPermission(
    const Principal& aPrincipal) {
  if (!ValidatePrincipal(aPrincipal)) {
    LogAndAssertFailedPrincipalValidationInfo(aPrincipal, __func__);
  }
  ContentBlockingUserInteraction::Observe(aPrincipal);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyMediaPlaybackChanged(
    const MaybeDiscarded<BrowsingContext>& aContext,
    MediaPlaybackState aState) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  if (RefPtr<IMediaInfoUpdater> updater =
          aContext.get_canonical()->GetMediaController()) {
    updater->NotifyMediaPlaybackChanged(aContext.ContextId(), aState);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyMediaAudibleChanged(
    const MaybeDiscarded<BrowsingContext>& aContext, MediaAudibleState aState) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  if (RefPtr<IMediaInfoUpdater> updater =
          aContext.get_canonical()->GetMediaController()) {
    updater->NotifyMediaAudibleChanged(aContext.ContextId(), aState);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyPictureInPictureModeChanged(
    const MaybeDiscarded<BrowsingContext>& aContext, bool aEnabled) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  if (RefPtr<MediaController> controller =
          aContext.get_canonical()->GetMediaController()) {
    controller->SetIsInPictureInPictureMode(aContext.ContextId(), aEnabled);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAbortOtherOrientationPendingPromises(
    const MaybeDiscarded<BrowsingContext>& aContext) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  CanonicalBrowsingContext* context = aContext.get_canonical();

  context->Group()->EachOtherParent(this, [&](ContentParent* aParent) {
    Unused << aParent->SendAbortOrientationPendingPromises(context);
  });

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyMediaSessionUpdated(
    const MaybeDiscarded<BrowsingContext>& aContext, bool aIsCreated) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  RefPtr<IMediaInfoUpdater> updater =
      aContext.get_canonical()->GetMediaController();
  if (!updater) {
    return IPC_OK();
  }
  if (aIsCreated) {
    updater->NotifySessionCreated(aContext->Id());
  } else {
    updater->NotifySessionDestroyed(aContext->Id());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyUpdateMediaMetadata(
    const MaybeDiscarded<BrowsingContext>& aContext,
    const Maybe<MediaMetadataBase>& aMetadata) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  if (RefPtr<IMediaInfoUpdater> updater =
          aContext.get_canonical()->GetMediaController()) {
    updater->UpdateMetadata(aContext.ContextId(), aMetadata);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyMediaSessionPlaybackStateChanged(
    const MaybeDiscarded<BrowsingContext>& aContext,
    MediaSessionPlaybackState aPlaybackState) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  if (RefPtr<IMediaInfoUpdater> updater =
          aContext.get_canonical()->GetMediaController()) {
    updater->SetDeclaredPlaybackState(aContext.ContextId(), aPlaybackState);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvNotifyMediaSessionSupportedActionChanged(
    const MaybeDiscarded<BrowsingContext>& aContext, MediaSessionAction aAction,
    bool aEnabled) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  RefPtr<IMediaInfoUpdater> updater =
      aContext.get_canonical()->GetMediaController();
  if (!updater) {
    return IPC_OK();
  }
  if (aEnabled) {
    updater->EnableAction(aContext.ContextId(), aAction);
  } else {
    updater->DisableAction(aContext.ContextId(), aAction);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyMediaFullScreenState(
    const MaybeDiscarded<BrowsingContext>& aContext, bool aIsInFullScreen) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  if (RefPtr<IMediaInfoUpdater> updater =
          aContext.get_canonical()->GetMediaController()) {
    updater->NotifyMediaFullScreenState(aContext.ContextId(), aIsInFullScreen);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyPositionStateChanged(
    const MaybeDiscarded<BrowsingContext>& aContext,
    const PositionState& aState) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }
  if (RefPtr<IMediaInfoUpdater> updater =
          aContext.get_canonical()->GetMediaController()) {
    updater->UpdatePositionState(aContext.ContextId(), aState);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvGetModulesTrust(
    ModulePaths&& aModPaths, bool aRunAtNormalPriority,
    GetModulesTrustResolver&& aResolver) {
#if defined(XP_WIN)
  RefPtr<DllServices> dllSvc(DllServices::Get());
  dllSvc->GetModulesTrust(std::move(aModPaths), aRunAtNormalPriority)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aResolver](ModulesMapResult&& aResult) {
            aResolver(Some(ModulesMapResult(std::move(aResult))));
          },
          [aResolver](nsresult aRv) { aResolver(Nothing()); });
  return IPC_OK();
#else
  return IPC_FAIL(this, "Unsupported on this platform");
#endif  // defined(XP_WIN)
}

mozilla::ipc::IPCResult ContentParent::RecvCreateBrowsingContext(
    uint64_t aGroupId, BrowsingContext::IPCInitializer&& aInit) {
  RefPtr<WindowGlobalParent> parent;
  if (aInit.mParentId != 0) {
    parent = WindowGlobalParent::GetByInnerWindowId(aInit.mParentId);
    if (!parent) {
      return IPC_FAIL(this, "Parent doesn't exist in parent process");
    }
  }

  if (parent && parent->GetContentParent() != this) {
    // We're trying attach a child BrowsingContext to a parent
    // WindowContext in another process. This is illegal since the
    // only thing that could create that child BrowsingContext is the parent
    // window's process.
    return IPC_FAIL(this,
                    "Must create BrowsingContext from the parent's process");
  }

  RefPtr<BrowsingContext> opener;
  if (aInit.GetOpenerId() != 0) {
    opener = BrowsingContext::Get(aInit.GetOpenerId());
    if (!opener) {
      return IPC_FAIL(this, "Opener doesn't exist in parent process");
    }
  }

  RefPtr<BrowsingContext> child = BrowsingContext::Get(aInit.mId);
  if (child) {
    // This is highly suspicious. BrowsingContexts should only be created once,
    // so finding one indicates that someone is doing something they shouldn't.
    return IPC_FAIL(this, "A BrowsingContext with this ID already exists");
  }

  // Ensure that the passed-in BrowsingContextGroup is valid.
  RefPtr<BrowsingContextGroup> group =
      BrowsingContextGroup::GetOrCreate(aGroupId);
  if (parent && parent->Group() != group) {
    if (parent->Group()->Id() != aGroupId) {
      return IPC_FAIL(this, "Parent has different group ID");
    } else {
      return IPC_FAIL(this, "Parent has different group object");
    }
  }
  if (opener && opener->Group() != group) {
    if (opener->Group()->Id() != aGroupId) {
      return IPC_FAIL(this, "Opener has different group ID");
    } else {
      return IPC_FAIL(this, "Opener has different group object");
    }
  }
  if (!parent && !opener && !group->Toplevels().IsEmpty()) {
    return IPC_FAIL(this, "Unrelated context from child in stale group");
  }

  BrowsingContext::CreateFromIPC(std::move(aInit), group, this);
  return IPC_OK();
}

bool ContentParent::CheckBrowsingContextEmbedder(CanonicalBrowsingContext* aBC,
                                                 const char* aOperation) const {
  if (!aBC->IsEmbeddedInProcess(ChildID())) {
    MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Warning,
            ("ParentIPC: Trying to %s out of process context 0x%08" PRIx64,
             aOperation, aBC->Id()));
    return false;
  }
  return true;
}

mozilla::ipc::IPCResult ContentParent::RecvDiscardBrowsingContext(
    const MaybeDiscarded<BrowsingContext>& aContext,
    DiscardBrowsingContextResolver&& aResolve) {
  if (!aContext.IsNullOrDiscarded()) {
    RefPtr<CanonicalBrowsingContext> context = aContext.get_canonical();
    if (!CheckBrowsingContextEmbedder(context, "discard")) {
      return IPC_FAIL(this, "Illegal Discard attempt");
    }

    context->Detach(/* aFromIPC */ true);
  }

  // Resolve the promise, as we've received and handled the message. This will
  // allow the content process to fully-discard references to this BC.
  aResolve(true);
  return IPC_OK();
}

void ContentParent::RegisterRemoteWorkerActor() {
  auto lock = mRemoteWorkerActorData.Lock();
  ++lock->mCount;
}

void ContentParent::UnregisterRemoveWorkerActor() {
  MOZ_ASSERT(NS_IsMainThread());

  {
    auto lock = mRemoteWorkerActorData.Lock();
    if (--lock->mCount) {
      return;
    }
  }

  MOZ_LOG(ContentParent::GetLog(), LogLevel::Verbose,
          ("UnregisterRemoveWorkerActor %p", this));
  MaybeBeginShutDown();
}

mozilla::ipc::IPCResult ContentParent::RecvWindowClose(
    const MaybeDiscarded<BrowsingContext>& aContext, bool aTrustedCaller) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  // FIXME Need to check that the sending process has access to the unit of
  // related
  //       browsing contexts of bc.

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->OwnerProcessId()));
  Unused << cp->SendWindowClose(context, aTrustedCaller);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvWindowFocus(
    const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType,
    uint64_t aActionId) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->OwnerProcessId()));
  Unused << cp->SendWindowFocus(context, aCallerType, aActionId);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvWindowBlur(
    const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->OwnerProcessId()));
  Unused << cp->SendWindowBlur(context, aCallerType);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvRaiseWindow(
    const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType,
    uint64_t aActionId) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  CanonicalBrowsingContext* context = aContext.get_canonical();

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->OwnerProcessId()));
  Unused << cp->SendRaiseWindow(context, aCallerType, aActionId);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvAdjustWindowFocus(
    const MaybeDiscarded<BrowsingContext>& aContext, bool aIsVisible,
    uint64_t aActionId) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  nsTHashMap<nsPtrHashKey<ContentParent>, bool> processes(2);
  processes.InsertOrUpdate(this, true);

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  CanonicalBrowsingContext* context = aContext.get_canonical();
  while (context) {
    BrowsingContext* parent = context->GetParent();
    if (!parent) {
      break;
    }

    CanonicalBrowsingContext* canonicalParent = parent->Canonical();
    ContentParent* cp = cpm->GetContentProcessById(
        ContentParentId(canonicalParent->OwnerProcessId()));
    if (!processes.Get(cp)) {
      Unused << cp->SendAdjustWindowFocus(context, aIsVisible, aActionId);
      processes.InsertOrUpdate(cp, true);
    }
    context = canonicalParent;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvClearFocus(
    const MaybeDiscarded<BrowsingContext>& aContext) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->OwnerProcessId()));
  Unused << cp->SendClearFocus(context);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetFocusedBrowsingContext(
    const MaybeDiscarded<BrowsingContext>& aContext) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->SetFocusedBrowsingContextInChrome(context);
    BrowserParent::UpdateFocusFromBrowsingContext();
  }

  context->Group()->EachOtherParent(this, [&](ContentParent* aParent) {
    Unused << aParent->SendSetFocusedBrowsingContext(context);
  });

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetActiveBrowsingContext(
    const MaybeDiscarded<BrowsingContext>& aContext, uint64_t aActionId) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return IPC_OK();
  }

  if (!fm->SetActiveBrowsingContextInChrome(context, aActionId)) {
    LOGFOCUS(
        ("Ignoring out-of-sequence attempt [%p] to set active browsing context "
         "in parent.",
         context));
    Unused << SendReviseActiveBrowsingContext(
        aActionId, fm->GetActiveBrowsingContextInChrome(),
        fm->GetActionIdForActiveBrowsingContextInChrome());
    return IPC_OK();
  }

  context->Group()->EachOtherParent(this, [&](ContentParent* aParent) {
    Unused << aParent->SendSetActiveBrowsingContext(context, aActionId);
  });

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvUnsetActiveBrowsingContext(
    const MaybeDiscarded<BrowsingContext>& aContext, uint64_t aActionId) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return IPC_OK();
  }

  if (!fm->SetActiveBrowsingContextInChrome(nullptr, aActionId)) {
    LOGFOCUS(
        ("Ignoring out-of-sequence attempt to unset active browsing context in "
         "parent [%p].",
         context));
    Unused << SendReviseActiveBrowsingContext(
        aActionId, fm->GetActiveBrowsingContextInChrome(),
        fm->GetActionIdForActiveBrowsingContextInChrome());
    return IPC_OK();
  }

  context->Group()->EachOtherParent(this, [&](ContentParent* aParent) {
    Unused << aParent->SendUnsetActiveBrowsingContext(context, aActionId);
  });

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetFocusedElement(
    const MaybeDiscarded<BrowsingContext>& aContext, bool aNeedsFocus) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();

  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->OwnerProcessId()));
  Unused << cp->SendSetFocusedElement(context, aNeedsFocus);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvFinalizeFocusOuter(
    const MaybeDiscarded<BrowsingContext>& aContext, bool aCanFocus,
    CallerType aCallerType) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();

  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->EmbedderProcessId()));
  if (cp) {
    Unused << cp->SendFinalizeFocusOuter(context, aCanFocus, aCallerType);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvInsertNewFocusActionId(
    uint64_t aActionId) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->InsertNewFocusActionId(aActionId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvBlurToParent(
    const MaybeDiscarded<BrowsingContext>& aFocusedBrowsingContext,
    const MaybeDiscarded<BrowsingContext>& aBrowsingContextToClear,
    const MaybeDiscarded<BrowsingContext>& aAncestorBrowsingContextToFocus,
    bool aIsLeavingDocument, bool aAdjustWidget,
    bool aBrowsingContextToClearHandled,
    bool aAncestorBrowsingContextToFocusHandled, uint64_t aActionId) {
  if (aFocusedBrowsingContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  CanonicalBrowsingContext* focusedBrowsingContext =
      aFocusedBrowsingContext.get_canonical();

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();

  // If aBrowsingContextToClear and aAncestorBrowsingContextToFocusHandled
  // didn't get handled in the process that sent this IPC message and they
  // aren't in the same process as aFocusedBrowsingContext, we need to split
  // off their handling here and use SendSetFocusedElement to send them
  // elsewhere than the blurring itself.

  bool ancestorDifferent =
      (!aAncestorBrowsingContextToFocusHandled &&
       !aAncestorBrowsingContextToFocus.IsNullOrDiscarded() &&
       (focusedBrowsingContext->OwnerProcessId() !=
        aAncestorBrowsingContextToFocus.get_canonical()->OwnerProcessId()));
  if (!aBrowsingContextToClearHandled &&
      !aBrowsingContextToClear.IsNullOrDiscarded() &&
      (focusedBrowsingContext->OwnerProcessId() !=
       aBrowsingContextToClear.get_canonical()->OwnerProcessId())) {
    MOZ_RELEASE_ASSERT(!ancestorDifferent,
                       "This combination is not supposed to happen.");
    ContentParent* cp = cpm->GetContentProcessById(ContentParentId(
        aBrowsingContextToClear.get_canonical()->OwnerProcessId()));
    Unused << cp->SendSetFocusedElement(aBrowsingContextToClear, false);
  } else if (ancestorDifferent) {
    ContentParent* cp = cpm->GetContentProcessById(ContentParentId(
        aAncestorBrowsingContextToFocus.get_canonical()->OwnerProcessId()));
    Unused << cp->SendSetFocusedElement(aAncestorBrowsingContextToFocus, true);
  }

  ContentParent* cp = cpm->GetContentProcessById(
      ContentParentId(focusedBrowsingContext->OwnerProcessId()));
  Unused << cp->SendBlurToChild(aFocusedBrowsingContext,
                                aBrowsingContextToClear,
                                aAncestorBrowsingContextToFocus,
                                aIsLeavingDocument, aAdjustWidget, aActionId);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvMaybeExitFullscreen(
    const MaybeDiscarded<BrowsingContext>& aContext) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();

  ContentParent* cp =
      cpm->GetContentProcessById(ContentParentId(context->OwnerProcessId()));
  Unused << cp->SendMaybeExitFullscreen(context);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvWindowPostMessage(
    const MaybeDiscarded<BrowsingContext>& aContext,
    const ClonedOrErrorMessageData& aMessage, const PostMessageData& aData) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* context = aContext.get_canonical();

  if (aData.source().IsDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message from dead or detached context"));
    return IPC_OK();
  }

  RefPtr<ContentParent> cp = context->GetContentParent();
  if (!cp) {
    MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
            ("ParentIPC: Trying to send PostMessage to dead content process"));
    return IPC_OK();
  }

  ClonedOrErrorMessageData message;
  StructuredCloneData messageFromChild;
  if (aMessage.type() == ClonedOrErrorMessageData::TClonedMessageData) {
    UnpackClonedMessageDataForParent(aMessage, messageFromChild);

    ClonedMessageData clonedMessageData;
    if (BuildClonedMessageDataForParent(cp, messageFromChild,
                                        clonedMessageData)) {
      message = std::move(clonedMessageData);
    } else {
      // FIXME Logging?
      message = ErrorMessageData();
    }
  } else {
    MOZ_ASSERT(aMessage.type() == ClonedOrErrorMessageData::TErrorMessageData);
    message = ErrorMessageData();
  }

  Unused << cp->SendWindowPostMessage(context, message, aData);
  return IPC_OK();
}

void ContentParent::AddBrowsingContextGroup(BrowsingContextGroup* aGroup) {
  MOZ_DIAGNOSTIC_ASSERT(aGroup);
  // Ensure that the group has been inserted, and if we're not launching
  // anymore, also begin subscribing. Launching processes will be subscribed if
  // they finish launching in `LaunchSubprocessResolve`.
  if (mGroups.EnsureInserted(aGroup) && !IsLaunching()) {
    aGroup->Subscribe(this);
  }
}

void ContentParent::RemoveBrowsingContextGroup(BrowsingContextGroup* aGroup) {
  MOZ_DIAGNOSTIC_ASSERT(aGroup);
  // Remove the group from our list. This is called from the
  // BrowisngContextGroup when unsubscribing, so we don't need to do it here.
  mGroups.RemoveEntry(aGroup);
}

mozilla::ipc::IPCResult ContentParent::RecvCommitBrowsingContextTransaction(
    const MaybeDiscarded<BrowsingContext>& aContext,
    BrowsingContext::BaseTransaction&& aTransaction, uint64_t aEpoch) {
  // Record the new BrowsingContextFieldEpoch associated with this transaction.
  // This should be done unconditionally, so that we're always in-sync.
  //
  // The order the parent process receives transactions is considered the
  // "canonical" ordering, so we don't need to worry about doing any
  // epoch-related validation.
  MOZ_ASSERT(aEpoch == mBrowsingContextFieldEpoch + 1,
             "Child process skipped an epoch?");
  mBrowsingContextFieldEpoch = aEpoch;

  return aTransaction.CommitFromIPC(aContext, this);
}

PParentToChildStreamParent* ContentParent::SendPParentToChildStreamConstructor(
    PParentToChildStreamParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  return PContentParent::SendPParentToChildStreamConstructor(aActor);
}

PFileDescriptorSetParent* ContentParent::SendPFileDescriptorSetConstructor(
    const FileDescriptor& aFD) {
  MOZ_ASSERT(NS_IsMainThread());
  return PContentParent::SendPFileDescriptorSetConstructor(aFD);
}

mozilla::ipc::IPCResult ContentParent::RecvBlobURLDataRequest(
    const nsCString& aBlobURL, nsIPrincipal* aTriggeringPrincipal,
    nsIPrincipal* aLoadingPrincipal, const OriginAttributes& aOriginAttributes,
    uint64_t aInnerWindowId, const Maybe<nsID>& aAgentClusterId,
    BlobURLDataRequestResolver&& aResolver) {
  RefPtr<BlobImpl> blobImpl;

  // Since revoked blobs are also retrieved, it is possible that the blob no
  // longer exists (due to the 5 second timeout) when execution reaches here
  if (!BlobURLProtocolHandler::GetDataEntry(
          aBlobURL, getter_AddRefs(blobImpl), aLoadingPrincipal,
          aTriggeringPrincipal, aOriginAttributes, aInnerWindowId,
          aAgentClusterId, true /* AlsoIfRevoked */)) {
    aResolver(NS_ERROR_DOM_BAD_URI);
    return IPC_OK();
  }

  IPCBlob ipcBlob;
  nsresult rv = IPCBlobUtils::Serialize(blobImpl, this, ipcBlob);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(rv);
    return IPC_OK();
  }

  aResolver(ipcBlob);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvReportServiceWorkerShutdownProgress(
    uint32_t aShutdownStateId, ServiceWorkerShutdownState::Progress aProgress) {
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_RELEASE_ASSERT(swm, "ServiceWorkers should shutdown before SWM.");

  swm->ReportServiceWorkerShutdownProgress(aShutdownStateId, aProgress);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvNotifyOnHistoryReload(
    const MaybeDiscarded<BrowsingContext>& aContext, const bool& aForceReload,
    NotifyOnHistoryReloadResolver&& aResolver) {
  bool canReload = false;
  Maybe<RefPtr<nsDocShellLoadState>> loadState;
  Maybe<bool> reloadActiveEntry;
  if (!aContext.IsDiscarded()) {
    aContext.get_canonical()->NotifyOnHistoryReload(
        aForceReload, canReload, loadState, reloadActiveEntry);
  }
  aResolver(Tuple<const bool&, const Maybe<RefPtr<nsDocShellLoadState>>&,
                  const Maybe<bool>&>(canReload, loadState, reloadActiveEntry));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvHistoryCommit(
    const MaybeDiscarded<BrowsingContext>& aContext, const uint64_t& aLoadID,
    const nsID& aChangeID, const uint32_t& aLoadType, const bool& aPersist,
    const bool& aCloneEntryChildren) {
  if (!aContext.IsDiscarded()) {
    aContext.get_canonical()->SessionHistoryCommit(
        aLoadID, aChangeID, aLoadType, aPersist, aCloneEntryChildren);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvHistoryGo(
    const MaybeDiscarded<BrowsingContext>& aContext, int32_t aOffset,
    uint64_t aHistoryEpoch, bool aRequireUserInteraction,
    HistoryGoResolver&& aResolveRequestedIndex) {
  if (!aContext.IsDiscarded()) {
    aContext.get_canonical()->HistoryGo(
        aOffset, aHistoryEpoch, aRequireUserInteraction, Some(ChildID()),
        std::move(aResolveRequestedIndex));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSessionHistoryUpdate(
    const MaybeDiscarded<BrowsingContext>& aContext, const int32_t& aIndex,
    const int32_t& aLength, const nsID& aChangeID) {
  if (aContext.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  CanonicalBrowsingContext* context = aContext.get_canonical();
  context->Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendHistoryCommitIndexAndLength(aContext, aIndex,
                                                       aLength, aChangeID);
  });
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSynchronizeLayoutHistoryState(
    const MaybeDiscarded<BrowsingContext>& aContext,
    nsILayoutHistoryState* aState) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  SessionHistoryEntry* entry =
      aContext.get_canonical()->GetActiveSessionHistoryEntry();
  if (entry) {
    entry->SetLayoutHistoryState(aState);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSessionHistoryEntryTitle(
    const MaybeDiscarded<BrowsingContext>& aContext, const nsString& aTitle) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  SessionHistoryEntry* entry =
      aContext.get_canonical()->GetActiveSessionHistoryEntry();
  if (entry) {
    entry->SetTitle(aTitle);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvSessionHistoryEntryScrollRestorationIsManual(
    const MaybeDiscarded<BrowsingContext>& aContext, const bool& aIsManual) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  SessionHistoryEntry* entry =
      aContext.get_canonical()->GetActiveSessionHistoryEntry();
  if (entry) {
    entry->SetScrollRestorationIsManual(aIsManual);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvSessionHistoryEntryStoreWindowNameInContiguousEntries(
    const MaybeDiscarded<BrowsingContext>& aContext, const nsString& aName) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  // Per https://html.spec.whatwg.org/#history-traversal 4.2.1, we need to set
  // the name to all contiguous entries. This has to be called before
  // CanonicalBrowsingContext::SessionHistoryCommit(), so the active entry is
  // still the old entry that we want to set.

  SessionHistoryEntry* entry =
      aContext.get_canonical()->GetActiveSessionHistoryEntry();

  if (entry) {
    nsSHistory::WalkContiguousEntries(
        entry, [&](nsISHEntry* aEntry) { aEntry->SetName(aName); });
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSessionHistoryEntryCacheKey(
    const MaybeDiscarded<BrowsingContext>& aContext,
    const uint32_t& aCacheKey) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  SessionHistoryEntry* entry =
      aContext.get_canonical()->GetActiveSessionHistoryEntry();
  if (entry) {
    entry->SetCacheKey(aCacheKey);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvGetLoadingSessionHistoryInfoFromParent(
    const MaybeDiscarded<BrowsingContext>& aContext,
    GetLoadingSessionHistoryInfoFromParentResolver&& aResolver) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  Maybe<LoadingSessionHistoryInfo> info;
  int32_t requestedIndex = -1;
  int32_t sessionHistoryLength = 0;
  aContext.get_canonical()->GetLoadingSessionHistoryInfoFromParent(
      info, &requestedIndex, &sessionHistoryLength);
  aResolver(
      Tuple<const mozilla::Maybe<LoadingSessionHistoryInfo>&, const int32_t&,
            const int32_t&>(info, requestedIndex, sessionHistoryLength));

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetActiveSessionHistoryEntry(
    const MaybeDiscarded<BrowsingContext>& aContext,
    const Maybe<nsPoint>& aPreviousScrollPos, SessionHistoryInfo&& aInfo,
    uint32_t aLoadType, uint32_t aUpdatedCacheKey, const nsID& aChangeID) {
  if (!aContext.IsDiscarded()) {
    aContext.get_canonical()->SetActiveSessionHistoryEntry(
        aPreviousScrollPos, &aInfo, aLoadType, aUpdatedCacheKey, aChangeID);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvReplaceActiveSessionHistoryEntry(
    const MaybeDiscarded<BrowsingContext>& aContext,
    SessionHistoryInfo&& aInfo) {
  if (!aContext.IsDiscarded()) {
    aContext.get_canonical()->ReplaceActiveSessionHistoryEntry(&aInfo);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentParent::RecvRemoveDynEntriesFromActiveSessionHistoryEntry(
    const MaybeDiscarded<BrowsingContext>& aContext) {
  if (!aContext.IsDiscarded()) {
    aContext.get_canonical()->RemoveDynEntriesFromActiveSessionHistoryEntry();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvRemoveFromSessionHistory(
    const MaybeDiscarded<BrowsingContext>& aContext, const nsID& aChangeID) {
  if (!aContext.IsDiscarded()) {
    aContext.get_canonical()->RemoveFromSessionHistory(aChangeID);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvHistoryReload(
    const MaybeDiscarded<BrowsingContext>& aContext,
    const uint32_t aReloadFlags) {
  if (!aContext.IsDiscarded()) {
    nsISHistory* shistory = aContext.get_canonical()->GetSessionHistory();
    if (shistory) {
      shistory->Reload(aReloadFlags);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvCommitWindowContextTransaction(
    const MaybeDiscarded<WindowContext>& aContext,
    WindowContext::BaseTransaction&& aTransaction, uint64_t aEpoch) {
  // Record the new BrowsingContextFieldEpoch associated with this transaction.
  // This should be done unconditionally, so that we're always in-sync.
  //
  // The order the parent process receives transactions is considered the
  // "canonical" ordering, so we don't need to worry about doing any
  // epoch-related validation.
  MOZ_ASSERT(aEpoch == mBrowsingContextFieldEpoch + 1,
             "Child process skipped an epoch?");
  mBrowsingContextFieldEpoch = aEpoch;

  return aTransaction.CommitFromIPC(aContext, this);
}

NS_IMETHODIMP ContentParent::GetChildID(uint64_t* aOut) {
  *aOut = this->ChildID();
  return NS_OK;
}

NS_IMETHODIMP ContentParent::GetOsPid(int32_t* aOut) {
  *aOut = Pid();
  return NS_OK;
}

NS_IMETHODIMP ContentParent::GetRemoteType(nsACString& aRemoteType) {
  aRemoteType = GetRemoteType();
  return NS_OK;
}

IPCResult ContentParent::RecvRawMessage(
    const JSActorMessageMeta& aMeta, const Maybe<ClonedMessageData>& aData,
    const Maybe<ClonedMessageData>& aStack) {
  Maybe<StructuredCloneData> data;
  if (aData) {
    data.emplace();
    data->BorrowFromClonedMessageDataForParent(*aData);
  }
  Maybe<StructuredCloneData> stack;
  if (aStack) {
    stack.emplace();
    stack->BorrowFromClonedMessageDataForParent(*aStack);
  }
  ReceiveRawMessage(aMeta, std::move(data), std::move(stack));
  return IPC_OK();
}

NS_IMETHODIMP ContentParent::GetActor(const nsACString& aName, JSContext* aCx,
                                      JSProcessActorParent** retval) {
  ErrorResult error;
  RefPtr<JSProcessActorParent> actor =
      JSActorManager::GetActor(aCx, aName, error)
          .downcast<JSProcessActorParent>();
  if (error.MaybeSetPendingException(aCx)) {
    return NS_ERROR_FAILURE;
  }
  actor.forget(retval);
  return NS_OK;
}

already_AddRefed<JSActor> ContentParent::InitJSActor(
    JS::HandleObject aMaybeActor, const nsACString& aName, ErrorResult& aRv) {
  RefPtr<JSProcessActorParent> actor;
  if (aMaybeActor.get()) {
    aRv = UNWRAP_OBJECT(JSProcessActorParent, aMaybeActor.get(), actor);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    actor = new JSProcessActorParent();
  }

  MOZ_RELEASE_ASSERT(!actor->Manager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  return actor.forget();
}

IPCResult ContentParent::RecvFOGData(ByteBuf&& buf) {
  glean::FOGData(std::move(buf));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentParent::RecvSetContainerFeaturePolicy(
    const MaybeDiscardedBrowsingContext& aContainerContext,
    FeaturePolicy* aContainerFeaturePolicy) {
  if (aContainerContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  auto* context = aContainerContext.get_canonical();
  context->SetContainerFeaturePolicy(aContainerFeaturePolicy);

  return IPC_OK();
}

NS_IMETHODIMP ContentParent::GetCanSend(bool* aCanSend) {
  *aCanSend = CanSend();
  return NS_OK;
}

ContentParent* ContentParent::AsContentParent() { return this; }

JSActorManager* ContentParent::AsJSActorManager() { return this; }

/* static */
void ContentParent::DidLaunchSubprocess() {
  TimeStamp now = TimeStamp::Now();
  uint32_t count = 0;
  for (auto* parent : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << parent;
    count += 1;
  }

  if (count > sMaxContentProcesses) {
    Telemetry::Accumulate(Telemetry::CONTENT_PROCESS_MAX, count);
  }

  if (sLastContentProcessLaunch) {
    TimeStamp last = *sLastContentProcessLaunch;

    Telemetry::AccumulateTimeDelta(
        Telemetry::CONTENT_PROCESS_TIME_SINCE_LAST_LAUNCH_MS, last, now);
  }
  sLastContentProcessLaunch = Some(now);
}

}  // namespace dom
}  // namespace mozilla

NS_IMPL_ISUPPORTS(ParentIdleListener, nsIObserver)

NS_IMETHODIMP
ParentIdleListener::Observe(nsISupports*, const char* aTopic,
                            const char16_t* aData) {
  mozilla::Unused << mParent->SendNotifyIdleObserver(
      mObserver, nsDependentCString(aTopic), nsDependentString(aData));
  return NS_OK;
}
