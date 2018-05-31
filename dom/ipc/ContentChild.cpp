/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

#include "ContentChild.h"

#include "GeckoProfiler.h"
#include "TabChild.h"
#include "HandlerServiceChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessHangMonitorIPC.h"
#include "mozilla/Unused.h"
#include "mozilla/TelemetryIPC.h"
#include "mozilla/devtools/HeapSnapshotTempFileHelperChild.h"
#include "mozilla/docshell/OfflineCacheUpdateChild.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientOpenWindowOpActors.h"
#include "mozilla/dom/ChildProcessMessageManager.h"
#include "mozilla/dom/ContentBridgeChild.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/VideoDecoderManagerChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ExternalHelperAppChild.h"
#include "mozilla/dom/FileCreatorHelper.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/dom/PLoginReputationChild.h"
#include "mozilla/dom/ProcessGlobal.h"
#include "mozilla/dom/PushNotifier.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/URLClassifierChild.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/psm/PSMContentListener.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/jsipc/PJavaScript.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/ContentProcessController.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/loader/ScriptCacheActors.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/CaptivePortalService.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/plugins/PluginInstanceParent.h"
#include "mozilla/plugins/PluginModuleParent.h"
#include "mozilla/widget/ScreenManager.h"
#include "mozilla/widget/WidgetMessageUtils.h"
#include "nsBaseDragService.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/WebBrowserPersistDocumentChild.h"
#include "mozilla/HangDetails.h"
#include "imgLoader.h"
#include "GMPServiceChild.h"
#include "NullPrincipal.h"
#include "nsISimpleEnumerator.h"
#include "nsIWorkerDebuggerManager.h"

#if !defined(XP_WIN)
#include "mozilla/Omnijar.h"
#endif

#ifdef MOZ_GECKO_PROFILER
#include "ChildProfilerController.h"
#endif

#if defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/SandboxSettings.h"
#if defined(XP_WIN)
#include "mozilla/sandboxTarget.h"
#elif defined(XP_LINUX)
#include "mozilla/Sandbox.h"
#include "mozilla/SandboxInfo.h"
#include "CubebUtils.h"
#elif defined(XP_MACOSX)
#include "mozilla/Sandbox.h"
#endif
#endif

#include "mozilla/Unused.h"

#include "mozInlineSpellChecker.h"
#include "nsDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIConsoleListener.h"
#include "nsIContentViewer.h"
#include "nsICycleCollectorListener.h"
#include "nsIIdlePeriod.h"
#include "nsIDragService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMemoryReporter.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsMemoryInfoDumper.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleSheetService.h"
#include "nsVariant.h"
#include "nsXULAppAPI.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsJSEnvironment.h"
#include "SandboxHal.h"
#include "nsDebugImpl.h"
#include "nsHashPropertyBag.h"
#include "nsLayoutStylesheetCache.h"
#include "nsThreadManager.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsISpellChecker.h"
#include "nsClipboardProxy.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsContentPermissionHelper.h"
#include "nsPluginHost.h"
#ifdef NS_PRINTING
#include "nsPrintingProxy.h"
#endif
#include "nsWindowMemoryReporter.h"

#include "IHistory.h"
#include "nsNetUtil.h"

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/task.h"

#include "nsChromeRegistryContent.h"
#include "nsFrameMessageManager.h"

#include "nsIGeolocationProvider.h"
#include "mozilla/dom/PCycleCollectWithLogsChild.h"

#include "nsIScriptSecurityManager.h"
#include "nsHostObjectProtocolHandler.h"

#ifdef MOZ_WEBRTC
#include "signaling/src/peerconnection/WebrtcGlobalChild.h"
#endif

#include "nsPermission.h"
#include "nsPermissionManager.h"

#include "PermissionMessageUtils.h"

#if defined(MOZ_WIDGET_ANDROID)
#include "APKOpen.h"
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#include "mozilla/widget/AudioSession.h"
#include "mozilla/audio/AudioNotificationReceiver.h"
#endif

#if defined(XP_MACOSX)
#include <CoreServices/CoreServices.h>
// Info.plist key associated with the developer repo path
#define MAC_DEV_REPO_KEY "MozillaDeveloperRepoPath"
// Info.plist key associated with the developer repo object directory
#define MAC_DEV_OBJ_KEY "MozillaDeveloperObjPath"
#endif /* XP_MACOSX */

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#ifdef XP_WIN
#include "mozilla/a11y/AccessibleWrap.h"
#endif
#endif

#include "mozilla/dom/File.h"
#include "mozilla/dom/PPresentationChild.h"
#include "mozilla/dom/PresentationIPCService.h"
#include "mozilla/ipc/InputStreamUtils.h"

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/PSpeechSynthesisChild.h"
#endif

#include "ClearOnShutdown.h"
#include "ProcessUtils.h"
#include "URIUtils.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "DomainPolicy.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "mozilla/widget/PuppetBidiKeyboard.h"
#include "mozilla/RemoteSpellCheckEngineChild.h"
#include "GMPServiceChild.h"
#include "GfxInfoBase.h"
#include "gfxPlatform.h"
#include "nscore.h" // for NS_FREE_PERMANENT_DATA
#include "VRManagerChild.h"
#include "private/pprio.h"
#include "nsString.h"

#ifdef MOZ_WIDGET_GTK
#include "nsAppRunner.h"
#endif

#ifdef MOZ_CODE_COVERAGE
#include "mozilla/CodeCoverageHandler.h"
#endif

using namespace mozilla;
using namespace mozilla::docshell;
using namespace mozilla::dom::ipc;
using namespace mozilla::media;
using namespace mozilla::embedding;
using namespace mozilla::gmp;
using namespace mozilla::hal_sandbox;
using namespace mozilla::ipc;
using namespace mozilla::intl;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::net;
using namespace mozilla::jsipc;
using namespace mozilla::psm;
using namespace mozilla::widget;
using mozilla::loader::PScriptCacheChild;

namespace mozilla {

namespace dom {

// IPC sender for remote GC/CC logging.
class CycleCollectWithLogsChild final
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

  NS_IMETHOD Open(FILE** aGCLog, FILE** aCCLog) override
  {
    if (NS_WARN_IF(!mGCLog) || NS_WARN_IF(!mCCLog)) {
      return NS_ERROR_FAILURE;
    }
    *aGCLog = mGCLog;
    *aCCLog = mCCLog;
    return NS_OK;
  }

  NS_IMETHOD CloseGCLog() override
  {
    MOZ_ASSERT(mGCLog);
    fclose(mGCLog);
    mGCLog = nullptr;
    SendCloseGCLog();
    return NS_OK;
  }

  NS_IMETHOD CloseCCLog() override
  {
    MOZ_ASSERT(mCCLog);
    fclose(mCCLog);
    mCCLog = nullptr;
    SendCloseCCLog();
    return NS_OK;
  }

  NS_IMETHOD GetFilenameIdentifier(nsAString& aIdentifier) override
  {
    return UnimplementedProperty();
  }

  NS_IMETHOD SetFilenameIdentifier(const nsAString& aIdentifier) override
  {
    return UnimplementedProperty();
  }

  NS_IMETHOD GetProcessIdentifier(int32_t *aIdentifier) override
  {
    return UnimplementedProperty();
  }

  NS_IMETHOD SetProcessIdentifier(int32_t aIdentifier) override
  {
    return UnimplementedProperty();
  }

  NS_IMETHOD GetGcLog(nsIFile** aPath) override
  {
    return UnimplementedProperty();
  }

  NS_IMETHOD GetCcLog(nsIFile** aPath) override
  {
    return UnimplementedProperty();
  }

private:
  ~CycleCollectWithLogsChild() override
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
    Unused << Send__delete__(this);
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

  ~AlertObserver() = default;

  bool ShouldRemoveFrom(nsIObserver* aObserver,
                        const nsString& aData) const
  {
    return (mObserver == aObserver && mData == aData);
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

class ConsoleListener final : public nsIConsoleListener
{
public:
  explicit ConsoleListener(ContentChild* aChild)
  : mChild(aChild) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONSOLELISTENER

private:
  ~ConsoleListener() = default;

  ContentChild* mChild;
  friend class ContentChild;
};

NS_IMPL_ISUPPORTS(ConsoleListener, nsIConsoleListener)

// Before we send the error to the parent process (which
// involves copying the memory), truncate any long lines.  CSS
// errors in particular share the memory for long lines with
// repeated errors, but the IPC communication we're about to do
// will break that sharing, so we better truncate now.
static void
TruncateString(nsAString& aString)
{
  if (aString.Length() > 1000) {
    aString.Truncate(1000);
  }
}

NS_IMETHODIMP
ConsoleListener::Observe(nsIConsoleMessage* aMessage)
{
  if (!mChild) {
    return NS_OK;
  }

  nsCOMPtr<nsIScriptError> scriptError = do_QueryInterface(aMessage);
  if (scriptError) {
    nsAutoString msg, sourceName, sourceLine;
    nsCString category;
    uint32_t lineNum, colNum, flags;
    bool fromPrivateWindow;

    nsresult rv = scriptError->GetErrorMessage(msg);
    NS_ENSURE_SUCCESS(rv, rv);
    TruncateString(msg);
    rv = scriptError->GetSourceName(sourceName);
    NS_ENSURE_SUCCESS(rv, rv);
    TruncateString(sourceName);
    rv = scriptError->GetSourceLine(sourceLine);
    NS_ENSURE_SUCCESS(rv, rv);
    TruncateString(sourceLine);

    rv = scriptError->GetCategory(getter_Copies(category));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetLineNumber(&lineNum);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetColumnNumber(&colNum);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetIsFromPrivateWindow(&fromPrivateWindow);
    NS_ENSURE_SUCCESS(rv, rv);

    {
      AutoJSAPI jsapi;
      jsapi.Init();
      JSContext* cx = jsapi.cx();

      JS::RootedValue stack(cx);
      rv = scriptError->GetStack(&stack);
      NS_ENSURE_SUCCESS(rv, rv);

      if (stack.isObject()) {
        JSAutoRealm ar(cx, &stack.toObject());

        StructuredCloneData data;
        ErrorResult err;
        data.Write(cx, stack, err);
        if (err.Failed()) {
          return err.StealNSResult();
        }

        ClonedMessageData cloned;
        if (!data.BuildClonedMessageDataForChild(mChild, cloned)) {
          return NS_ERROR_FAILURE;
        }

        mChild->SendScriptErrorWithStack(msg, sourceName, sourceLine,
                                         lineNum, colNum, flags, category,
                                         fromPrivateWindow, cloned);
        return NS_OK;
      }
    }


    mChild->SendScriptError(msg, sourceName, sourceLine,
                            lineNum, colNum, flags, category,
                            fromPrivateWindow);
    return NS_OK;
  }

  nsString msg;
  nsresult rv = aMessage->GetMessageMoz(getter_Copies(msg));
  NS_ENSURE_SUCCESS(rv, rv);
  mChild->SendConsoleMessage(msg);
  return NS_OK;
}

#ifdef NIGHTLY_BUILD
/**
 * The singleton of this class is registered with the HangMonitor as an
 * annotator, so that the hang monitor can record whether or not there were
 * pending input events when the thread hung.
 */
class PendingInputEventHangAnnotator final
  : public HangMonitor::Annotator
{
public:
  virtual void AnnotateHang(HangMonitor::HangAnnotations& aAnnotations) override
  {
    int32_t pending = ContentChild::GetSingleton()->GetPendingInputEvents();
    if (pending > 0) {
      aAnnotations.AddAnnotation(NS_LITERAL_STRING("PendingInput"), pending);
    }
  }

  static PendingInputEventHangAnnotator sSingleton;
};
PendingInputEventHangAnnotator PendingInputEventHangAnnotator::sSingleton;
#endif

class ContentChild::ShutdownCanary final
{ };

ContentChild* ContentChild::sSingleton;
StaticAutoPtr<ContentChild::ShutdownCanary> ContentChild::sShutdownCanary;

ContentChild::ContentChild()
 : mID(uint64_t(-1))
#if defined(XP_WIN) && defined(ACCESSIBILITY)
 , mMainChromeTid(0)
 , mMsaaID(0)
#endif
 , mIsAlive(true)
 , mShuttingDown(false)
{
  // This process is a content process, so it's clearly running in
  // multiprocess mode!
  nsDebugImpl::SetMultiprocessMode("Child");

  // When ContentChild is created, the observer service does not even exist.
  // When ContentChild::RecvSetXPCOMProcessAttributes is called (the first
  // IPDL call made on this object), shutdown may have already happened. Thus
  // we create a canary here that relies upon getting cleared if shutdown
  // happens without requiring the observer service at this time.
  if (!sShutdownCanary) {
    sShutdownCanary = new ShutdownCanary();
    ClearOnShutdown(&sShutdownCanary, ShutdownPhase::Shutdown);
  }
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4722) /* Silence "destructor never returns" warning */
#endif

ContentChild::~ContentChild()
{
#ifndef NS_FREE_PERMANENT_DATA
  MOZ_CRASH("Content Child shouldn't be destroyed.");
#endif
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

NS_INTERFACE_MAP_BEGIN(ContentChild)
  NS_INTERFACE_MAP_ENTRY(nsIContentChild)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentChild)
NS_INTERFACE_MAP_END


mozilla::ipc::IPCResult
ContentChild::RecvSetXPCOMProcessAttributes(const XPCOMInitData& aXPCOMInit,
                                            const StructuredCloneData& aInitialData,
                                            nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache,
                                            nsTArray<SystemFontListEntry>&& aFontList)
{
  if (!sShutdownCanary) {
    return IPC_OK();
  }

  mLookAndFeelCache = Move(aLookAndFeelIntCache);
  mFontList = Move(aFontList);
  gfx::gfxVars::SetValuesForInitialize(aXPCOMInit.gfxNonDefaultVarUpdates());
  InitXPCOM(aXPCOMInit, aInitialData);
  InitGraphicsDeviceData(aXPCOMInit.contentDeviceData());

  return IPC_OK();
}

bool
ContentChild::Init(MessageLoop* aIOLoop,
                   base::ProcessId aParentPid,
                   const char* aParentBuildID,
                   IPC::Channel* aChannel,
                   uint64_t aChildID,
                   bool aIsForBrowser)
{
#ifdef MOZ_WIDGET_GTK
  // When running X11 only build we need to pass a display down
  // to gtk_init because it's not going to use the one from the environment
  // on its own when deciding which backend to use, and when starting under
  // XWayland, it may choose to start with the wayland backend
  // instead of the x11 backend.
  // The DISPLAY environment variable is normally set by the parent process.
  // The MOZ_GDK_DISPLAY environment variable is set from nsAppRunner.cpp
  // when --display is set by the command line.
  if (!gfxPlatform::IsHeadless()) {
    const char* display_name = PR_GetEnv("MOZ_GDK_DISPLAY");
#ifndef MOZ_WAYLAND
    if (!display_name) {
      display_name = PR_GetEnv("DISPLAY");
    }
#endif
    if (display_name) {
      int argc = 3;
      char option_name[] = "--display";
      char* argv[] = {
        // argv0 is unused because g_set_prgname() was called in
        // XRE_InitChildProcess().
        nullptr,
        option_name,
        const_cast<char*>(display_name),
        nullptr
      };
      char** argvp = argv;
      gtk_init(&argc, &argvp);
    } else {
      gtk_init(nullptr, nullptr);
    }
  }
#endif

#ifdef MOZ_X11
  if (!gfxPlatform::IsHeadless()) {
    // Do this after initializing GDK, or GDK will install its own handler.
    XRE_InstallX11ErrorHandler();
  }
#endif

  NS_ASSERTION(!sSingleton, "only one ContentChild per child");

  // Once we start sending IPC messages, we need the thread manager to be
  // initialized so we can deal with the responses. Do that here before we
  // try to construct the crash reporter.
  nsresult rv = nsThreadManager::get().Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (!Open(aChannel, aParentPid, aIOLoop)) {
    return false;
  }
  sSingleton = this;

  // If communications with the parent have broken down, take the process
  // down so it's not hanging around.
  GetIPCChannel()->SetAbortOnError(true);
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  GetIPCChannel()->SetChannelFlags(MessageChannel::REQUIRE_A11Y_REENTRY);
#endif

  // This must be checked before any IPDL message, which may hit sentinel
  // errors due to parent and content processes having different
  // versions.
  MessageChannel* channel = GetIPCChannel();
  if (channel && !channel->SendBuildIDsMatchMessage(aParentBuildID)) {
    // We need to quit this process if the buildID doesn't match the parent's.
    // This can occur when an update occurred in the background.
    ProcessChild::QuickExit();
  }

#ifdef MOZ_X11
  if (!gfxPlatform::IsHeadless()) {
    // Send the parent our X socket to act as a proxy reference for our X
    // resources.
    int xSocketFd = ConnectionNumber(DefaultXDisplay());
    SendBackUpXResources(FileDescriptor(xSocketFd));
  }
#endif

  CrashReporterClient::InitSingleton(this);

  mID = aChildID;
  mIsForBrowser = aIsForBrowser;

#ifdef NS_PRINTING
  // Force the creation of the nsPrintingProxy so that it's IPC counterpart,
  // PrintingParent, is always available for printing initiated from the parent.
  RefPtr<nsPrintingProxy> printingProxy = nsPrintingProxy::GetInstance();
#endif

  SetProcessName(NS_LITERAL_STRING("Web Content"));

#ifdef NIGHTLY_BUILD
  // NOTE: We have to register the annotator on the main thread, as annotators
  // only affect a single thread.
  SystemGroup::Dispatch(TaskCategory::Other,
                        NS_NewRunnableFunction("RegisterPendingInputEventHangAnnotator", [] {
                          HangMonitor::RegisterAnnotator(
                            PendingInputEventHangAnnotator::sSingleton);
                        }));
#endif

  return true;
}

void
ContentChild::SetProcessName(const nsAString& aName)
{
  char* name;
  if ((name = PR_GetEnv("MOZ_DEBUG_APP_PROCESS")) &&
    aName.EqualsASCII(name)) {
#ifdef OS_POSIX
    printf_stderr("\n\nCHILDCHILDCHILDCHILD\n  [%s] debug me @%d\n\n", name,
                  getpid());
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

NS_IMETHODIMP
ContentChild::ProvideWindow(mozIDOMWindowProxy* aParent,
                            uint32_t aChromeFlags,
                            bool aCalledFromJS,
                            bool aPositionSpecified,
                            bool aSizeSpecified,
                            nsIURI* aURI,
                            const nsAString& aName,
                            const nsACString& aFeatures,
                            bool aForceNoOpener,
                            nsIDocShellLoadInfo* aLoadInfo,
                            bool* aWindowIsNew,
                            mozIDOMWindowProxy** aReturn)
{
  return ProvideWindowCommon(nullptr, aParent, false, aChromeFlags,
                             aCalledFromJS, aPositionSpecified,
                             aSizeSpecified, aURI, aName, aFeatures,
                             aForceNoOpener, aLoadInfo, aWindowIsNew, aReturn);
}

static nsresult
GetCreateWindowParams(mozIDOMWindowProxy* aParent,
                      nsIDocShellLoadInfo* aLoadInfo,
                      nsACString& aBaseURIString, float* aFullZoom,
                      uint32_t* aReferrerPolicy,
                      nsIPrincipal** aTriggeringPrincipal)
{
  *aFullZoom = 1.0f;
  auto* opener = nsPIDOMWindowOuter::From(aParent);
  if (!opener) {
    nsCOMPtr<nsIPrincipal> nullPrincipal = NullPrincipal::CreateWithoutOriginAttributes();
    NS_ADDREF(*aTriggeringPrincipal = nullPrincipal);
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = opener->GetDoc();
  NS_ADDREF(*aTriggeringPrincipal = doc->NodePrincipal());
  nsCOMPtr<nsIURI> baseURI = doc->GetDocBaseURI();
  if (!baseURI) {
    NS_ERROR("nsIDocument didn't return a base URI");
    return NS_ERROR_FAILURE;
  }

  baseURI->GetSpec(aBaseURIString);

  bool sendReferrer = true;
  if (aLoadInfo) {
    aLoadInfo->GetSendReferrer(&sendReferrer);
    if (!sendReferrer) {
      *aReferrerPolicy = mozilla::net::RP_No_Referrer;
    } else {
      aLoadInfo->GetReferrerPolicy(aReferrerPolicy);
    }
  }

  RefPtr<nsDocShell> openerDocShell =
    static_cast<nsDocShell*>(opener->GetDocShell());
  if (!openerDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIContentViewer> cv;
  nsresult rv = openerDocShell->GetContentViewer(getter_AddRefs(cv));
  if (NS_SUCCEEDED(rv) && cv) {
    cv->GetFullZoom(aFullZoom);
  }

  return NS_OK;
}

nsresult
ContentChild::ProvideWindowCommon(TabChild* aTabOpener,
                                  mozIDOMWindowProxy* aParent,
                                  bool aIframeMoz,
                                  uint32_t aChromeFlags,
                                  bool aCalledFromJS,
                                  bool aPositionSpecified,
                                  bool aSizeSpecified,
                                  nsIURI* aURI,
                                  const nsAString& aName,
                                  const nsACString& aFeatures,
                                  bool aForceNoOpener,
                                  nsIDocShellLoadInfo* aLoadInfo,
                                  bool* aWindowIsNew,
                                  mozIDOMWindowProxy** aReturn)
{
  *aReturn = nullptr;

  nsAutoPtr<IPCTabContext> ipcContext;
  TabId openerTabId = TabId(0);
  nsAutoCString features(aFeatures);
  nsAutoString name(aName);

  nsresult rv;

  MOZ_ASSERT(!aParent || aTabOpener,
             "If aParent is non-null, we should have an aTabOpener");

  // Cache the boolean preference for allowing noopener windows to open in a
  // separate process.
  static bool sNoopenerNewProcess = false;
  static bool sNoopenerNewProcessInited = false;
  if (!sNoopenerNewProcessInited) {
    Preferences::AddBoolVarCache(&sNoopenerNewProcess,
                                 "dom.noopener.newprocess.enabled");
    sNoopenerNewProcessInited = true;
  }

  // Check if we should load in a different process. We always want to load in a
  // different process if we have noopener set, but we also might if we can't
  // load in the current process.
  bool loadInDifferentProcess = aForceNoOpener && sNoopenerNewProcess;
  if (aTabOpener && !loadInDifferentProcess && aURI) {
    nsCOMPtr<nsIWebBrowserChrome3> browserChrome3;
    rv = aTabOpener->GetWebBrowserChrome(getter_AddRefs(browserChrome3));
    if (NS_SUCCEEDED(rv) && browserChrome3) {
      bool shouldLoad;
      rv = browserChrome3->ShouldLoadURIInThisProcess(aURI, &shouldLoad);
      loadInDifferentProcess = NS_SUCCEEDED(rv) && !shouldLoad;
    }
  }

  // If we're in a content process and we have noopener set, there's no reason
  // to load in our process, so let's load it elsewhere!
  if (loadInDifferentProcess) {
    nsAutoCString baseURIString;
    float fullZoom;
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    uint32_t referrerPolicy = mozilla::net::RP_Unset;
    rv = GetCreateWindowParams(aParent, aLoadInfo, baseURIString, &fullZoom,
                               &referrerPolicy,
                               getter_AddRefs(triggeringPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    OptionalURIParams uriToLoad;
    SerializeURI(aURI, uriToLoad);
    Unused << SendCreateWindowInDifferentProcess(aTabOpener,
                                                 aChromeFlags,
                                                 aCalledFromJS,
                                                 aPositionSpecified,
                                                 aSizeSpecified,
                                                 uriToLoad,
                                                 features,
                                                 baseURIString,
                                                 fullZoom,
                                                 name,
                                                 Principal(triggeringPrincipal),
                                                 referrerPolicy);

    // We return NS_ERROR_ABORT, so that the caller knows that we've abandoned
    // the window open as far as it is concerned.
    return NS_ERROR_ABORT;
  }

  if (aTabOpener) {
    PopupIPCTabContext context;
    openerTabId = aTabOpener->GetTabId();
    context.opener() = openerTabId;
    context.isMozBrowserElement() = aTabOpener->IsMozBrowserElement();
    ipcContext = new IPCTabContext(context);
  } else {
    // It's possible to not have a TabChild opener in the case
    // of ServiceWorker::OpenWindow.
    UnsafeIPCTabContext unsafeTabContext;
    ipcContext = new IPCTabContext(unsafeTabContext);
  }

  MOZ_ASSERT(ipcContext);
  TabId tabId(nsContentUtils::GenerateTabId());

  // We need to assign a TabGroup to the PBrowser actor before we send it to the
  // parent. Otherwise, the parent could send messages to us before we have a
  // proper TabGroup for that actor.
  RefPtr<TabGroup> tabGroup;
  if (aTabOpener && !aForceNoOpener) {
    // The new actor will use the same tab group as the opener.
    tabGroup = aTabOpener->TabGroup();
  } else {
    tabGroup = new TabGroup();
  }

  TabContext newTabContext = aTabOpener ? *aTabOpener : TabContext();
  RefPtr<TabChild> newChild = new TabChild(this, tabId, tabGroup,
                                           newTabContext, aChromeFlags);
  if (NS_FAILED(newChild->Init())) {
    return NS_ERROR_ABORT;
  }

  if (aTabOpener) {
    MOZ_ASSERT(ipcContext->type() == IPCTabContext::TPopupIPCTabContext);
    ipcContext->get_PopupIPCTabContext().opener() = aTabOpener;
  }

  nsCOMPtr<nsIEventTarget> target = tabGroup->EventTargetFor(TaskCategory::Other);
  SetEventTargetForActor(newChild, target);

  Unused << SendPBrowserConstructor(
    // We release this ref in DeallocPBrowserChild
    RefPtr<TabChild>(newChild).forget().take(),
    tabId, TabId(0), *ipcContext, aChromeFlags,
    GetID(), IsForBrowser());


  PRenderFrameChild* renderFrame = newChild->SendPRenderFrameConstructor();

  nsCOMPtr<nsPIDOMWindowInner> parentTopInnerWindow;
  if (aParent) {
    nsCOMPtr<nsPIDOMWindowOuter> parentTopWindow =
      nsPIDOMWindowOuter::From(aParent)->GetTop();
    if (parentTopWindow) {
      parentTopInnerWindow = parentTopWindow->GetCurrentInnerWindow();
    }
  }

  // Set to true when we're ready to return from this function.
  bool ready = false;

  // NOTE: Capturing by reference here is safe, as this function won't return
  // until one of these callbacks is called.
  auto resolve = [&] (const CreatedWindowInfo& info) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    rv = info.rv();
    *aWindowIsNew = info.windowOpened();
    nsTArray<FrameScriptInfo> frameScripts(info.frameScripts());
    nsCString urlToLoad = info.urlToLoad();
    TextureFactoryIdentifier textureFactoryIdentifier = info.textureFactoryIdentifier();
    layers::LayersId layersId = info.layersId();
    CompositorOptions compositorOptions = info.compositorOptions();
    uint32_t maxTouchPoints = info.maxTouchPoints();
    DimensionInfo dimensionInfo = info.dimensions();

    // Once this function exits, we should try to exit the nested event loop.
    ready = true;

    // NOTE: We have to handle this immediately in the resolve callback in order
    // to make sure that we don't process any more IPC messages before returning
    // from ProvideWindowCommon.

    // Handle the error which we got back from the parent process, if we got
    // one.
    if (NS_FAILED(rv)) {
      return;
    }

    if (!*aWindowIsNew) {
      rv = NS_ERROR_ABORT;
      return;
    }

    // If the TabChild has been torn down, we don't need to do this anymore.
    if (NS_WARN_IF(!newChild->IPCOpen() || newChild->IsDestroyed())) {
      rv = NS_ERROR_ABORT;
      return;
    }

    if (!layersId.IsValid()) { // if renderFrame is invalid.
      renderFrame = nullptr;
    }

    ShowInfo showInfo(EmptyString(), false, false, true, false, 0, 0, 0);
    auto* opener = nsPIDOMWindowOuter::From(aParent);
    nsIDocShell* openerShell;
    if (opener && (openerShell = opener->GetDocShell())) {
      nsCOMPtr<nsILoadContext> context = do_QueryInterface(openerShell);
      showInfo = ShowInfo(EmptyString(), false,
                          context->UsePrivateBrowsing(), true, false,
                          aTabOpener->WebWidget()->GetDPI(),
                          aTabOpener->WebWidget()->RoundsWidgetCoordinatesTo(),
                          aTabOpener->WebWidget()->GetDefaultScale().scale);
    }

    newChild->SetMaxTouchPoints(maxTouchPoints);

    // Set the opener window for this window before we start loading the document
    // inside of it. We have to do this before loading the remote scripts, because
    // they can poke at the document and cause the nsDocument to be created before
    // the openerwindow
    nsCOMPtr<mozIDOMWindowProxy> windowProxy = do_GetInterface(newChild->WebNavigation());
    if (!aForceNoOpener && windowProxy && aParent) {
      nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(windowProxy);
      nsPIDOMWindowOuter* parent = nsPIDOMWindowOuter::From(aParent);
      outer->SetOpenerWindow(parent, *aWindowIsNew);
    }

    // Unfortunately we don't get a window unless we've shown the frame.  That's
    // pretty bogus; see bug 763602.
    newChild->DoFakeShow(textureFactoryIdentifier, layersId, compositorOptions,
                        renderFrame, showInfo);

    newChild->RecvUpdateDimensions(dimensionInfo);

    for (size_t i = 0; i < frameScripts.Length(); i++) {
      FrameScriptInfo& info = frameScripts[i];
      if (!newChild->RecvLoadRemoteScript(info.url(), info.runInGlobalScope())) {
        MOZ_CRASH();
      }
    }

    if (!urlToLoad.IsEmpty()) {
      newChild->RecvLoadURL(urlToLoad, showInfo);
    }

    nsCOMPtr<mozIDOMWindowProxy> win = do_GetInterface(newChild->WebNavigation());
    win.forget(aReturn);
  };

  // NOTE: Capturing by reference here is safe, as this function won't return
  // until one of these callbacks is called.
  auto reject = [&] (ResponseRejectReason) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    NS_WARNING("windowCreated promise rejected");
    rv = NS_ERROR_NOT_AVAILABLE;
    ready = true;
  };

  // Send down the request to open the window.
  if (aIframeMoz) {
    MOZ_ASSERT(aTabOpener);
    nsAutoCString url;
    if (aURI) {
      aURI->GetSpec(url);
    } else {
      // We can't actually send a nullptr up as the URI, since IPDL doesn't let us
      // send nullptr's for primitives. We indicate that the nsString for the URI
      // should be converted to a nullptr by voiding the string.
      url.SetIsVoid(true);
    }

    // NOTE: BrowserFrameOpenWindowPromise is the same type as
    // CreateWindowPromise, and this code depends on that fact.
    newChild->SendBrowserFrameOpenWindow(aTabOpener, renderFrame,
                                         NS_ConvertUTF8toUTF16(url),
                                         name, NS_ConvertUTF8toUTF16(features),
                                         Move(resolve), Move(reject));
  } else {
    nsAutoCString baseURIString;
    float fullZoom;
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    uint32_t referrerPolicy = mozilla::net::RP_Unset;
    rv = GetCreateWindowParams(aParent, aLoadInfo, baseURIString, &fullZoom,
                               &referrerPolicy,
                               getter_AddRefs(triggeringPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    OptionalURIParams uriToLoad;
    if (aURI) {
      SerializeURI(aURI, uriToLoad);
    } else {
      uriToLoad = mozilla::void_t();
    }

    SendCreateWindow(aTabOpener, newChild, renderFrame,
                     aChromeFlags, aCalledFromJS, aPositionSpecified,
                     aSizeSpecified, uriToLoad, features, baseURIString,
                     fullZoom, Principal(triggeringPrincipal), referrerPolicy,
                     Move(resolve), Move(reject));
  }

  // =======================
  // Begin Nested Event Loop
  // =======================

  // We have to wait for a response from either SendCreateWindow or
  // SendBrowserFrameOpenWindow with information we're going to need to return
  // from this function, So we spin a nested event loop until they get back to
  // us.

  // Prevent the docshell from becoming active while the nested event loop is
  // spinning.
  newChild->AddPendingDocShellBlocker();
  auto removePendingDocShellBlocker = MakeScopeExit([&] {
    if (newChild) {
      newChild->RemovePendingDocShellBlocker();
    }
  });

  // Suspend our window if we have one to make sure we don't re-enter it.
  if (parentTopInnerWindow) {
    parentTopInnerWindow->Suspend();
  }

  {
    AutoNoJSAPI nojsapi;

    // Spin the event loop until we get a response. Callers of this function
    // already have to guard against an inner event loop spinning in the
    // non-e10s case because of the need to spin one to create a new chrome
    // window.
    SpinEventLoopUntil([&] () { return ready; });
    MOZ_RELEASE_ASSERT(ready,
                       "We are on the main thread, so we should not exit this "
                       "loop without ready being true.");
  }

  if (parentTopInnerWindow) {
    parentTopInnerWindow->Resume();
  }

  // =====================
  // End Nested Event Loop
  // =====================

  // We should have the results already set by the callbacks.
  MOZ_ASSERT_IF(NS_SUCCEEDED(rv), *aReturn);
  return rv;
}

void
ContentChild::GetProcessName(nsAString& aName) const
{
  aName.Assign(mProcessName);
}

bool
ContentChild::IsAlive() const
{
  return mIsAlive;
}

bool
ContentChild::IsShuttingDown() const
{
  return mShuttingDown;
}

void
ContentChild::GetProcessName(nsACString& aName) const
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
ContentChild::InitGraphicsDeviceData(const ContentDeviceData& aData)
{
  gfxPlatform::InitChild(aData);
}

void
ContentChild::InitXPCOM(const XPCOMInitData& aXPCOMInit,
                        const mozilla::dom::ipc::StructuredCloneData& aInitialData)
{
  // Do this as early as possible to get the parent process to initialize the
  // background thread since we'll likely need database information very soon.
  BackgroundChild::Startup();

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    MOZ_ASSERT_UNREACHABLE("PBackground init can't fail at this point");
    return;
  }

  ClientManager::Startup();

  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!svc) {
    NS_WARNING("Couldn't acquire console service");
    return;
  }

  mConsoleListener = new ConsoleListener(this);
  if (NS_FAILED(svc->RegisterListener(mConsoleListener)))
    NS_WARNING("Couldn't register console listener for child process");

  mAvailableDictionaries = aXPCOMInit.dictionaries();

  RecvSetOffline(aXPCOMInit.isOffline());
  RecvSetConnectivity(aXPCOMInit.isConnected());
  LocaleService::GetInstance()->AssignAppLocales(aXPCOMInit.appLocales());
  LocaleService::GetInstance()->AssignRequestedLocales(aXPCOMInit.requestedLocales());

  RecvSetCaptivePortalState(aXPCOMInit.captivePortalState());
  RecvBidiKeyboardNotify(aXPCOMInit.isLangRTL(), aXPCOMInit.haveBidiKeyboards());

  // Create the CPOW manager as soon as possible.
  SendPJavaScriptConstructor();

  if (aXPCOMInit.domainPolicy().active()) {
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);
    ssm->ActivateDomainPolicyInternal(getter_AddRefs(mPolicy));
    if (!mPolicy) {
      MOZ_CRASH("Failed to activate domain policy.");
    }
    mPolicy->ApplyClone(&aXPCOMInit.domainPolicy());
  }

  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1"));
  if (nsCOMPtr<nsIClipboardProxy> clipboardProxy = do_QueryInterface(clipboard)) {
    clipboardProxy->SetCapabilities(aXPCOMInit.clipboardCaps());
  }

  {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(xpc::PrivilegedJunkScope()))) {
      MOZ_CRASH();
    }
    ErrorResult rv;
    JS::RootedValue data(jsapi.cx());
    mozilla::dom::ipc::StructuredCloneData id;
    id.Copy(aInitialData);
    id.Read(jsapi.cx(), &data, rv);
    if (NS_WARN_IF(rv.Failed())) {
      MOZ_CRASH();
    }
    ProcessGlobal* global = ProcessGlobal::Get();
    global->SetInitialProcessData(data);
  }

  // The stylesheet cache is not ready yet. Store this URL for future use.
  nsCOMPtr<nsIURI> ucsURL = DeserializeURI(aXPCOMInit.userContentSheetURL());
  nsLayoutStylesheetCache::SetUserContentCSSURL(ucsURL);

  GfxInfoBase::SetFeatureStatus(aXPCOMInit.gfxFeatureStatus());

  DataStorage::SetCachedStorageEntries(aXPCOMInit.dataStorage());

  // Set the dynamic scalar definitions for this process.
  TelemetryIPC::AddDynamicScalarDefinitions(aXPCOMInit.dynamicScalarDefs());

  DOMPrefs::Initialize();
}

mozilla::ipc::IPCResult
ContentChild::RecvRequestMemoryReport(const uint32_t& aGeneration,
                                      const bool& aAnonymize,
                                      const bool& aMinimizeMemoryUsage,
                                      const MaybeFileDesc& aDMDFile)
{
  nsCString process;
  GetProcessName(process);
  AppendProcessId(process);

  MemoryReportRequestClient::Start(
    aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, process);
  return IPC_OK();
}

PCycleCollectWithLogsChild*
ContentChild::AllocPCycleCollectWithLogsChild(const bool& aDumpAllTraces,
                                              const FileDescriptor& aGCLog,
                                              const FileDescriptor& aCCLog)
{
  auto* actor = new CycleCollectWithLogsChild(aGCLog, aCCLog);
  // Return actor with refcount 0, which is safe because it has a non-XPCOM type.
  return actor;
}

mozilla::ipc::IPCResult
ContentChild::RecvPCycleCollectWithLogsConstructor(PCycleCollectWithLogsChild* aActor,
                                                   const bool& aDumpAllTraces,
                                                   const FileDescriptor& aGCLog,
                                                   const FileDescriptor& aCCLog)
{
  // Take a reference here, where the XPCOM type is regained.
  RefPtr<CycleCollectWithLogsChild> sink = static_cast<CycleCollectWithLogsChild*>(aActor);
  nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");

  dumper->DumpGCAndCCLogsToSink(aDumpAllTraces, sink);

  // The actor's destructor is called when the last reference goes away...
  return IPC_OK();
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

mozilla::ipc::IPCResult
ContentChild::RecvInitContentBridgeChild(Endpoint<PContentBridgeChild>&& aEndpoint)
{
  ContentBridgeChild::Create(Move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvInitGMPService(Endpoint<PGMPServiceChild>&& aGMPService)
{
  if (!GMPServiceChild::Create(Move(aGMPService))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvInitProfiler(Endpoint<PProfilerChild>&& aEndpoint)
{
#ifdef MOZ_GECKO_PROFILER
  mProfilerController = ChildProfilerController::Create(Move(aEndpoint));
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvGMPsChanged(nsTArray<GMPCapabilityData>&& capabilities)
{
  GeckoMediaPluginServiceChild::UpdateGMPCapabilities(Move(capabilities));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvInitProcessHangMonitor(Endpoint<PProcessHangMonitorChild>&& aHangMonitor)
{
  CreateHangMonitorChild(Move(aHangMonitor));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::GetResultForRenderingInitFailure(base::ProcessId aOtherPid)
{
  if (aOtherPid == base::GetCurrentProcId() || aOtherPid == OtherPid()) {
    // If we are talking to ourselves, or the UI process, then that is a fatal
    // protocol error.
    return IPC_FAIL_NO_REASON(this);
  }

  // If we are talking to the GPU process, then we should recover from this on
  // the next ContentChild::RecvReinitRendering call.
  gfxCriticalNote << "Could not initialize rendering with GPU process";
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvRequestPerformanceMetrics()
{
  MOZ_ASSERT(mozilla::dom::DOMPrefs::SchedulerLoggingEnabled());
  nsTArray<PerformanceInfo> info;
  CollectPerformanceInfo(info);
  SendAddPerformanceMetrics(info);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvInitRendering(Endpoint<PCompositorManagerChild>&& aCompositor,
                                Endpoint<PImageBridgeChild>&& aImageBridge,
                                Endpoint<PVRManagerChild>&& aVRBridge,
                                Endpoint<PVideoDecoderManagerChild>&& aVideoManager,
                                nsTArray<uint32_t>&& namespaces)
{
  MOZ_ASSERT(namespaces.Length() == 3);

  // Note that for all of the methods below, if it can fail, it should only
  // return false if the failure is an IPDL error. In such situations,
  // ContentChild can reason about whether or not to wait for
  // RecvReinitRendering (because we surmised the GPU process crashed), or if it
  // should crash itself (because we are actually talking to the UI process). If
  // there are localized failures (e.g. failed to spawn a thread), then it
  // should MOZ_RELEASE_ASSERT or MOZ_CRASH as necessary instead.
  if (!CompositorManagerChild::Init(Move(aCompositor), namespaces[0])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!CompositorManagerChild::CreateContentCompositorBridge(namespaces[1])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!ImageBridgeChild::InitForContent(Move(aImageBridge), namespaces[2])) {
    return GetResultForRenderingInitFailure(aImageBridge.OtherPid());
  }
  if (!gfx::VRManagerChild::InitForContent(Move(aVRBridge))) {
    return GetResultForRenderingInitFailure(aVRBridge.OtherPid());
  }
  VideoDecoderManagerChild::InitForContent(Move(aVideoManager));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvReinitRendering(Endpoint<PCompositorManagerChild>&& aCompositor,
                                  Endpoint<PImageBridgeChild>&& aImageBridge,
                                  Endpoint<PVRManagerChild>&& aVRBridge,
                                  Endpoint<PVideoDecoderManagerChild>&& aVideoManager,
                                  nsTArray<uint32_t>&& namespaces)
{
  MOZ_ASSERT(namespaces.Length() == 3);
  nsTArray<RefPtr<TabChild>> tabs = TabChild::GetAll();

  // Zap all the old layer managers we have lying around.
  for (const auto& tabChild : tabs) {
    if (tabChild->GetLayersId().IsValid()) {
      tabChild->InvalidateLayers();
    }
  }

  // Re-establish singleton bridges to the compositor.
  if (!CompositorManagerChild::Init(Move(aCompositor), namespaces[0])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!CompositorManagerChild::CreateContentCompositorBridge(namespaces[1])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!ImageBridgeChild::ReinitForContent(Move(aImageBridge), namespaces[2])) {
    return GetResultForRenderingInitFailure(aImageBridge.OtherPid());
  }
  if (!gfx::VRManagerChild::ReinitForContent(Move(aVRBridge))) {
    return GetResultForRenderingInitFailure(aVRBridge.OtherPid());
  }
  gfxPlatform::GetPlatform()->CompositorUpdated();

  // Establish new PLayerTransactions.
  for (const auto& tabChild : tabs) {
    if (tabChild->GetLayersId().IsValid()) {
      tabChild->ReinitRendering();
    }
  }

  VideoDecoderManagerChild::InitForContent(Move(aVideoManager));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvAudioDefaultDeviceChange()
{
#ifdef XP_WIN
  audio::AudioNotificationReceiver::NotifyDefaultDeviceChanged();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvReinitRenderingForDeviceReset()
{
  gfxPlatform::GetPlatform()->CompositorUpdated();

  nsTArray<RefPtr<TabChild>> tabs = TabChild::GetAll();
  for (const auto& tabChild : tabs) {
    if (tabChild->GetLayersId().IsValid()) {
      tabChild->ReinitRenderingForDeviceReset();
    }
  }
  return IPC_OK();
}

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)

#include <stdlib.h>

static bool
GetAppPaths(nsCString &aAppPath, nsCString &aAppBinaryPath, nsCString &aAppDir)
{
  nsAutoCString appPath;
  nsAutoCString appBinaryPath(
    (CommandLine::ForCurrentProcess()->argv()[0]).c_str());

  nsAutoCString::const_iterator start, end;
  appBinaryPath.BeginReading(start);
  appBinaryPath.EndReading(end);
  if (RFindInReadable(NS_LITERAL_CSTRING(".app/Contents/MacOS/"), start, end)) {
    end = start;
    ++end; ++end; ++end; ++end;
    appBinaryPath.BeginReading(start);
    appPath.Assign(Substring(start, end));
  } else {
    return false;
  }

  nsCOMPtr<nsIFile> app, appBinary;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appPath),
                                true, getter_AddRefs(app));
  if (NS_FAILED(rv)) {
    return false;
  }
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appBinaryPath),
                       true, getter_AddRefs(appBinary));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIFile> appDir;
  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc) {
    return false;
  }
  rv = dirSvc->Get(NS_GRE_DIR,
                   NS_GET_IID(nsIFile), getter_AddRefs(appDir));
  if (NS_FAILED(rv)) {
    return false;
  }
  bool exists;
  rv = appDir->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    return false;
  }

  // appDir points to .app/Contents/Resources, for our purposes we want
  // .app/Contents.
  nsCOMPtr<nsIFile> appDirParent;
  rv = appDir->GetParent(getter_AddRefs(appDirParent));
  if (NS_FAILED(rv)) {
    return false;
  }

  rv = app->Normalize();
  if (NS_FAILED(rv)) {
    return false;
  }
  app->GetNativePath(aAppPath);

  rv = appBinary->Normalize();
  if (NS_FAILED(rv)) {
    return false;
  }
  appBinary->GetNativePath(aAppBinaryPath);

  rv = appDirParent->Normalize();
  if (NS_FAILED(rv)) {
    return false;
  }
  appDirParent->GetNativePath(aAppDir);

  return true;
}

// This function is only used in an |#ifdef DEBUG| path.
#ifdef DEBUG
// Given a path to a file, return the directory which contains it.
static nsAutoCString
GetDirectoryPath(const char *aPath) {
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  if (!file ||
      NS_FAILED(file->InitWithNativePath(nsDependentCString(aPath)))) {
    MOZ_CRASH("Failed to create or init an nsIFile");
  }
  nsCOMPtr<nsIFile> directoryFile;
  if (NS_FAILED(file->GetParent(getter_AddRefs(directoryFile))) ||
      !directoryFile) {
    MOZ_CRASH("Failed to get parent for an nsIFile");
  }
  directoryFile->Normalize();
  nsAutoCString directoryPath;
  if (NS_FAILED(directoryFile->GetNativePath(directoryPath))) {
    MOZ_CRASH("Failed to get path for an nsIFile");
  }
  return directoryPath;
}
#endif // DEBUG

extern "C" {
void CGSSetDenyWindowServerConnections(bool);
void CGSShutdownServerConnections();
};

static bool
StartMacOSContentSandbox()
{
  int sandboxLevel = GetEffectiveContentSandboxLevel();
  if (sandboxLevel < 1) {
    return false;
  }

  if (!XRE_UseNativeEventProcessing()) {
    // If we've opened a connection to the window server, shut it down now. Forbid
    // future connections as well. We do this for sandboxing, but it also ensures
    // that the Activity Monitor will not label the content process as "Not
    // responding" because it's not running a native event loop. See bug 1384336.
    CGSSetDenyWindowServerConnections(true);
    CGSShutdownServerConnections();
  }

  nsAutoCString appPath, appBinaryPath, appDir;
  if (!GetAppPaths(appPath, appBinaryPath, appDir)) {
    MOZ_CRASH("Error resolving child process path");
  }

  ContentChild* cc = ContentChild::GetSingleton();

  nsresult rv;
  nsCOMPtr<nsIFile> profileDir;
  cc->GetProfileDir(getter_AddRefs(profileDir));
  nsCString profileDirPath;
  if (profileDir) {
    profileDir->Normalize();
    rv = profileDir->GetNativePath(profileDirPath);
    if (NS_FAILED(rv) || profileDirPath.IsEmpty()) {
      MOZ_CRASH("Failed to get profile path");
    }
  }

  bool isFileProcess = cc->GetRemoteType().EqualsLiteral(FILE_REMOTE_TYPE);

  MacSandboxInfo info;
  info.type = MacSandboxType_Content;
  info.level = sandboxLevel;
  info.hasFilePrivileges = isFileProcess;
  info.shouldLog = Preferences::GetBool("security.sandbox.logging.enabled") ||
                   PR_GetEnv("MOZ_SANDBOX_LOGGING");
  info.appPath.assign(appPath.get());
  info.appBinaryPath.assign(appBinaryPath.get());
  info.appDir.assign(appDir.get());
  info.hasAudio = !Preferences::GetBool("media.cubeb.sandbox");

  // These paths are used to whitelist certain directories used by the testing
  // system. They should not be considered a public API, and are only intended
  // for use in automation.
  nsAutoCString testingReadPath1;
  Preferences::GetCString("security.sandbox.content.mac.testing_read_path1",
                          testingReadPath1);
  if (!testingReadPath1.IsEmpty()) {
    info.testingReadPath1.assign(testingReadPath1.get());
  }
  nsAutoCString testingReadPath2;
  Preferences::GetCString("security.sandbox.content.mac.testing_read_path2",
                          testingReadPath2);
  if (!testingReadPath2.IsEmpty()) {
    info.testingReadPath2.assign(testingReadPath2.get());
  }

  if (mozilla::IsDevelopmentBuild()) {
    nsCOMPtr<nsIFile> repoDir;
    rv = mozilla::GetRepoDir(getter_AddRefs(repoDir));
    if (NS_FAILED(rv)) {
      MOZ_CRASH("Failed to get path to repo dir");
    }
    nsCString repoDirPath;
    Unused << repoDir->GetNativePath(repoDirPath);
    info.testingReadPath3.assign(repoDirPath.get());

    nsCOMPtr<nsIFile> objDir;
    rv = mozilla::GetObjDir(getter_AddRefs(objDir));
    if (NS_FAILED(rv)) {
      MOZ_CRASH("Failed to get path to build object dir");
    }

    nsCString objDirPath;
    Unused << objDir->GetNativePath(objDirPath);
    info.testingReadPath4.assign(objDirPath.get());
  }

  if (profileDir) {
    info.hasSandboxedProfile = true;
    info.profileDir.assign(profileDirPath.get());
  } else {
    info.hasSandboxedProfile = false;
  }

#ifdef DEBUG
  // When a content process dies intentionally (|NoteIntentionalCrash|), for
  // tests it wants to log that it did this. Allow writing to this location
  // that the testrunner wants.
  char *bloatLog = PR_GetEnv("XPCOM_MEM_BLOAT_LOG");
  if (bloatLog != nullptr) {
    // |bloatLog| points to a specific file, but we actually write to a sibling
    // of that path.
    nsAutoCString bloatDirectoryPath = GetDirectoryPath(bloatLog);
    info.debugWriteDir.assign(bloatDirectoryPath.get());
  }
#endif // DEBUG

  std::string err;
  if (!mozilla::StartMacSandbox(info, err)) {
    NS_WARNING(err.c_str());
    MOZ_CRASH("sandbox_init() failed");
  }

  return true;
}
#endif

mozilla::ipc::IPCResult
ContentChild::RecvSetProcessSandbox(const MaybeFileDesc& aBroker)
{
  // We may want to move the sandbox initialization somewhere else
  // at some point; see bug 880808.
#if defined(MOZ_CONTENT_SANDBOX)
  bool sandboxEnabled = true;
#if defined(XP_LINUX)
  // On Linux, we have to support systems that can't use any sandboxing.
  if (!SandboxInfo::Get().CanSandboxContent()) {
    sandboxEnabled = false;
  } else {
    // Pre-start audio before sandboxing; see bug 1443612.
    if (!Preferences::GetBool("media.cubeb.sandbox")) {
      Unused << CubebUtils::GetCubebContext();
    }
  }

  if (sandboxEnabled) {
    sandboxEnabled =
      SetContentProcessSandbox(ContentProcessSandboxParams::ForThisProcess(aBroker));
  }
#elif defined(XP_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
#elif defined(XP_MACOSX)
  sandboxEnabled = StartMacOSContentSandbox();
#endif

  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("ContentSandboxEnabled"),
    sandboxEnabled? NS_LITERAL_CSTRING("1") : NS_LITERAL_CSTRING("0"));
#if defined(XP_LINUX) && !defined(OS_ANDROID)
  nsAutoCString flagsString;
  flagsString.AppendInt(SandboxInfo::Get().AsInteger());

  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("ContentSandboxCapabilities"), flagsString);
#endif /* XP_LINUX && !OS_ANDROID */
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("RemoteType"),
                                     NS_ConvertUTF16toUTF8(GetRemoteType()));
#endif /* MOZ_CONTENT_SANDBOX */

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvBidiKeyboardNotify(const bool& aIsLangRTL,
                                     const bool& aHaveBidiKeyboards)
{
  // bidi is always of type PuppetBidiKeyboard* (because in the child, the only
  // possible implementation of nsIBidiKeyboard is PuppetBidiKeyboard).
  PuppetBidiKeyboard* bidi = static_cast<PuppetBidiKeyboard*>(nsContentUtils::GetBidiKeyboard());
  if (bidi) {
    bidi->SetBidiKeyboardInfo(aIsLangRTL, aHaveBidiKeyboards);
  }
  return IPC_OK();
}

static StaticRefPtr<CancelableRunnable> gFirstIdleTask;

static void
FirstIdle(void)
{
  MOZ_ASSERT(gFirstIdleTask);
  gFirstIdleTask = nullptr;
  ContentChild::GetSingleton()->SendFirstIdle();
}

mozilla::jsipc::PJavaScriptChild *
ContentChild::AllocPJavaScriptChild()
{
  MOZ_ASSERT(ManagedPJavaScriptChild().IsEmpty());

  return nsIContentChild::AllocPJavaScriptChild();
}

bool
ContentChild::DeallocPJavaScriptChild(PJavaScriptChild *aChild)
{
  return nsIContentChild::DeallocPJavaScriptChild(aChild);
}

PBrowserChild*
ContentChild::AllocPBrowserChild(const TabId& aTabId,
                                 const TabId& aSameTabGroupAs,
                                 const IPCTabContext& aContext,
                                 const uint32_t& aChromeFlags,
                                 const ContentParentId& aCpID,
                                 const bool& aIsForBrowser)
{
  return nsIContentChild::AllocPBrowserChild(aTabId,
                                             aSameTabGroupAs,
                                             aContext,
                                             aChromeFlags,
                                             aCpID,
                                             aIsForBrowser);
}

bool
ContentChild::SendPBrowserConstructor(PBrowserChild* aActor,
                                      const TabId& aTabId,
                                      const TabId& aSameTabGroupAs,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const ContentParentId& aCpID,
                                      const bool& aIsForBrowser)
{
  if (IsShuttingDown()) {
    return false;
  }

  return PContentChild::SendPBrowserConstructor(aActor,
                                                aTabId,
                                                aSameTabGroupAs,
                                                aContext,
                                                aChromeFlags,
                                                aCpID,
                                                aIsForBrowser);
}

mozilla::ipc::IPCResult
ContentChild::RecvPBrowserConstructor(PBrowserChild* aActor,
                                      const TabId& aTabId,
                                      const TabId& aSameTabGroupAs,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const ContentParentId& aCpID,
                                      const bool& aIsForBrowser)
{
  MOZ_ASSERT(!IsShuttingDown());

  static bool hasRunOnce = false;
  if (!hasRunOnce) {
    hasRunOnce = true;
    MOZ_ASSERT(!gFirstIdleTask);
    RefPtr<CancelableRunnable> firstIdleTask = NewCancelableRunnableFunction("FirstIdleRunnable",
                                                                             FirstIdle);
    gFirstIdleTask = firstIdleTask;
    if (NS_FAILED(NS_IdleDispatchToCurrentThread(firstIdleTask.forget()))) {
      gFirstIdleTask = nullptr;
      hasRunOnce = false;
    }
  }

  return nsIContentChild::RecvPBrowserConstructor(aActor,
                                                  aTabId,
                                                  aSameTabGroupAs,
                                                  aContext,
                                                  aChromeFlags,
                                                  aCpID,
                                                  aIsForBrowser);
}

void
ContentChild::GetAvailableDictionaries(InfallibleTArray<nsString>& aDictionaries)
{
  aDictionaries = mAvailableDictionaries;
}

PFileDescriptorSetChild*
ContentChild::SendPFileDescriptorSetConstructor(const FileDescriptor& aFD)
{
  if (IsShuttingDown()) {
    return nullptr;
  }

  return PContentChild::SendPFileDescriptorSetConstructor(aFD);
}

PFileDescriptorSetChild*
ContentChild::AllocPFileDescriptorSetChild(const FileDescriptor& aFD)
{
  return nsIContentChild::AllocPFileDescriptorSetChild(aFD);
}

bool
ContentChild::DeallocPFileDescriptorSetChild(PFileDescriptorSetChild* aActor)
{
  return nsIContentChild::DeallocPFileDescriptorSetChild(aActor);
}

bool
ContentChild::DeallocPBrowserChild(PBrowserChild* aIframe)
{
  return nsIContentChild::DeallocPBrowserChild(aIframe);
}

PIPCBlobInputStreamChild*
ContentChild::AllocPIPCBlobInputStreamChild(const nsID& aID,
                                            const uint64_t& aSize)
{
  return nsIContentChild::AllocPIPCBlobInputStreamChild(aID, aSize);
}

bool
ContentChild::DeallocPIPCBlobInputStreamChild(PIPCBlobInputStreamChild* aActor)
{
  return nsIContentChild::DeallocPIPCBlobInputStreamChild(aActor);
}

mozilla::PRemoteSpellcheckEngineChild *
ContentChild::AllocPRemoteSpellcheckEngineChild()
{
  MOZ_CRASH("Default Constructor for PRemoteSpellcheckEngineChild should never be called");
  return nullptr;
}

bool
ContentChild::DeallocPRemoteSpellcheckEngineChild(PRemoteSpellcheckEngineChild *child)
{
  delete child;
  return true;
}

PPresentationChild*
ContentChild::AllocPPresentationChild()
{
  MOZ_CRASH("We should never be manually allocating PPresentationChild actors");
  return nullptr;
}

bool
ContentChild::DeallocPPresentationChild(PPresentationChild* aActor)
{
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult
ContentChild::RecvNotifyPresentationReceiverLaunched(PBrowserChild* aIframe,
                                                     const nsString& aSessionId)
{
  nsCOMPtr<nsIDocShell> docShell =
    do_GetInterface(static_cast<TabChild*>(aIframe)->WebNavigation());
  NS_WARNING_ASSERTION(docShell, "WebNavigation failed");

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(service, "presentation service is missing");

  Unused << NS_WARN_IF(NS_FAILED(static_cast<PresentationIPCService*>(service.get())->MonitorResponderLoading(aSessionId, docShell)));

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvNotifyPresentationReceiverCleanUp(const nsString& aSessionId)
{
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(service, "presentation service is missing");

  Unused << NS_WARN_IF(NS_FAILED(service->UntrackSessionInfo(aSessionId, nsIPresentationService::ROLE_RECEIVER)));

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvNotifyEmptyHTTPCache()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(nullptr, "cacheservice:empty-cache", nullptr);
  return IPC_OK();
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

devtools::PHeapSnapshotTempFileHelperChild*
ContentChild::AllocPHeapSnapshotTempFileHelperChild()
{
  return devtools::HeapSnapshotTempFileHelperChild::Create();
}

bool
ContentChild::DeallocPHeapSnapshotTempFileHelperChild(
  devtools::PHeapSnapshotTempFileHelperChild* aHeapSnapshotHelper)
{
  delete aHeapSnapshotHelper;
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

jsipc::CPOWManager*
ContentChild::GetCPOWManager()
{
  if (PJavaScriptChild* c = LoneManagedOrNullAsserts(ManagedPJavaScriptChild())) {
    return CPOWManagerFor(c);
  }
  return CPOWManagerFor(SendPJavaScriptConstructor());
}

mozilla::ipc::IPCResult
ContentChild::RecvPTestShellConstructor(PTestShellChild* actor)
{
  return IPC_OK();
}

void
ContentChild::UpdateCookieStatus(nsIChannel   *aChannel)
{
  RefPtr<CookieServiceChild> csChild =
    CookieServiceChild::GetSingleton();
  NS_ASSERTION(csChild, "Couldn't get CookieServiceChild");

  csChild->TrackCookieLoad(aChannel);
}

PScriptCacheChild*
ContentChild::AllocPScriptCacheChild(const FileDescOrError& cacheFile, const bool& wantCacheData)
{
  return new loader::ScriptCacheChild();
}

bool
ContentChild::DeallocPScriptCacheChild(PScriptCacheChild* cache)
{
  delete static_cast<loader::ScriptCacheChild*>(cache);
  return true;
}

mozilla::ipc::IPCResult
ContentChild::RecvPScriptCacheConstructor(PScriptCacheChild* actor, const FileDescOrError& cacheFile, const bool& wantCacheData)
{
  Maybe<FileDescriptor> fd;
  if (cacheFile.type() == cacheFile.TFileDescriptor) {
    fd.emplace(cacheFile.get_FileDescriptor());
  }

  static_cast<loader::ScriptCacheChild*>(actor)->Init(fd, wantCacheData);
  return IPC_OK();
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

PPrintingChild*
ContentChild::AllocPPrintingChild()
{
  // The ContentParent should never attempt to allocate the nsPrintingProxy,
  // which implements PPrintingChild. Instead, the nsPrintingProxy service is
  // requested and instantiated via XPCOM, and the constructor of
  // nsPrintingProxy sets up the IPC connection.
  MOZ_CRASH("Should never get here!");
  return nullptr;
}

bool
ContentChild::DeallocPPrintingChild(PPrintingChild* printing)
{
  return true;
}

PChildToParentStreamChild*
ContentChild::SendPChildToParentStreamConstructor(PChildToParentStreamChild* aActor)
{
  if (IsShuttingDown()) {
    return nullptr;
  }

  return PContentChild::SendPChildToParentStreamConstructor(aActor);
}

PChildToParentStreamChild*
ContentChild::AllocPChildToParentStreamChild()
{
  return nsIContentChild::AllocPChildToParentStreamChild();
}

bool
ContentChild::DeallocPChildToParentStreamChild(PChildToParentStreamChild* aActor)
{
  return nsIContentChild::DeallocPChildToParentStreamChild(aActor);
}

PParentToChildStreamChild*
ContentChild::AllocPParentToChildStreamChild()
{
  return nsIContentChild::AllocPParentToChildStreamChild();
}

bool
ContentChild::DeallocPParentToChildStreamChild(PParentToChildStreamChild* aActor)
{
  return nsIContentChild::DeallocPParentToChildStreamChild(aActor);
}

PPSMContentDownloaderChild*
ContentChild::AllocPPSMContentDownloaderChild(const uint32_t& aCertType)
{
  // NB: We don't need aCertType in the child actor.
  RefPtr<PSMContentDownloaderChild> child = new PSMContentDownloaderChild();
  return child.forget().take();
}

bool
ContentChild::DeallocPPSMContentDownloaderChild(PPSMContentDownloaderChild* aListener)
{
  auto* listener = static_cast<PSMContentDownloaderChild*>(aListener);
  RefPtr<PSMContentDownloaderChild> child = dont_AddRef(listener);
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
                                           const bool& aWasFileChannel,
                                           const OptionalURIParams& aReferrer,
                                           PBrowserChild* aBrowser)
{
  auto *child = new ExternalHelperAppChild();
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

PHandlerServiceChild*
ContentChild::AllocPHandlerServiceChild()
{
  auto* actor = new HandlerServiceChild();
  actor->AddRef();
  return actor;
}

bool ContentChild::DeallocPHandlerServiceChild(PHandlerServiceChild* aHandlerServiceChild)
{
  static_cast<HandlerServiceChild*>(aHandlerServiceChild)->Release();
  return true;
}

media::PMediaChild*
ContentChild::AllocPMediaChild()
{
  return media::AllocPMediaChild();
}

bool
ContentChild::DeallocPMediaChild(media::PMediaChild *aActor)
{
  return media::DeallocPMediaChild(aActor);
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

PWebrtcGlobalChild *
ContentChild::AllocPWebrtcGlobalChild()
{
#ifdef MOZ_WEBRTC
  auto *child = new WebrtcGlobalChild();
  return child;
#else
  return nullptr;
#endif
}

bool
ContentChild::DeallocPWebrtcGlobalChild(PWebrtcGlobalChild *aActor)
{
#ifdef MOZ_WEBRTC
  delete static_cast<WebrtcGlobalChild*>(aActor);
  return true;
#else
  return false;
#endif
}


mozilla::ipc::IPCResult
ContentChild::RecvRegisterChrome(InfallibleTArray<ChromePackage>&& packages,
                                 InfallibleTArray<SubstitutionMapping>&& resources,
                                 InfallibleTArray<OverrideMapping>&& overrides,
                                 const nsCString& locale,
                                 const bool& reset)
{
  nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
  nsChromeRegistryContent* chromeRegistry =
    static_cast<nsChromeRegistryContent*>(registrySvc.get());
  chromeRegistry->RegisterRemoteChrome(packages, resources, overrides,
                                       locale, reset);
  static bool preloadDone = false;
  if (!preloadDone) {
    preloadDone = true;
    nsContentUtils::AsyncPrecreateStringBundles();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvRegisterChromeItem(const ChromeRegistryItem& item)
{
  nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
  nsChromeRegistryContent* chromeRegistry =
    static_cast<nsChromeRegistryContent*>(registrySvc.get());
  switch (item.type()) {
    case ChromeRegistryItem::TChromePackage:
      chromeRegistry->RegisterPackage(item.get_ChromePackage());
      break;

    case ChromeRegistryItem::TOverrideMapping:
      chromeRegistry->RegisterOverride(item.get_OverrideMapping());
      break;

    case ChromeRegistryItem::TSubstitutionMapping:
      chromeRegistry->RegisterSubstitution(item.get_SubstitutionMapping());
      break;

    default:
      MOZ_ASSERT(false, "bad chrome item");
      return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvClearImageCache(const bool& privateLoader, const bool& chrome)
{
  imgLoader* loader = privateLoader ? imgLoader::PrivateBrowsingLoader() :
                                      imgLoader::NormalLoader();

  loader->ClearCache(chrome);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvSetOffline(const bool& offline)
{
  nsCOMPtr<nsIIOService> io (do_GetIOService());
  NS_ASSERTION(io, "IO Service can not be null");

  io->SetOffline(offline);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvSetConnectivity(const bool& connectivity)
{
  nsCOMPtr<nsIIOService> io(do_GetIOService());
  nsCOMPtr<nsIIOServiceInternal> ioInternal(do_QueryInterface(io));
  NS_ASSERTION(ioInternal, "IO Service can not be null");

  ioInternal->SetConnectivity(connectivity);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvSetCaptivePortalState(const int32_t& aState)
{
  nsCOMPtr<nsICaptivePortalService> cps = do_GetService(NS_CAPTIVEPORTAL_CID);
  if (!cps) {
    return IPC_OK();
  }

  mozilla::net::CaptivePortalService *portal =
    static_cast<mozilla::net::CaptivePortalService*>(cps.get());
  portal->SetStateInChild(aState);

  return IPC_OK();
}

void
ContentChild::ActorDestroy(ActorDestroyReason why)
{
  if (mForceKillTimer) {
    mForceKillTimer->Cancel();
    mForceKillTimer = nullptr;
  }

  if (AbnormalShutdown == why) {
    NS_WARNING("shutting down early because of crash!");
    ProcessChild::QuickExit();
  }

#ifndef NS_FREE_PERMANENT_DATA
  // In release builds, there's no point in the content process
  // going through the full XPCOM shutdown path, because it doesn't
  // keep persistent state.
  ProcessChild::QuickExit();
#else
  if (gFirstIdleTask) {
    gFirstIdleTask->Cancel();
    gFirstIdleTask = nullptr;
  }

  nsHostObjectProtocolHandler::RemoveDataEntries();

  mAlertObservers.Clear();

  mIdleObservers.Clear();

  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (svc) {
    svc->UnregisterListener(mConsoleListener);
    mConsoleListener->mChild = nullptr;
  }
  mIsAlive = false;

  CrashReporterClient::DestroySingleton();

  XRE_ShutdownChildProcess();
#endif // NS_FREE_PERMANENT_DATA
}

void
ContentChild::ProcessingError(Result aCode, const char* aReason)
{
  switch (aCode) {
    case MsgDropped:
      NS_WARNING("MsgDropped in ContentChild");
      return;

    case MsgNotKnown:
    case MsgNotAllowed:
    case MsgPayloadError:
    case MsgProcessingError:
    case MsgRouteError:
    case MsgValueError:
      break;

    default:
      MOZ_CRASH("not reached");
  }

  nsDependentCString reason(aReason);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ipc_channel_error"), reason);

  MOZ_CRASH("Content child abort due to IPC error");
}

nsresult
ContentChild::AddRemoteAlertObserver(const nsString& aData,
                                     nsIObserver* aObserver)
{
  NS_ASSERTION(aObserver, "Adding a null observer?");
  mAlertObservers.AppendElement(new AlertObserver(aObserver, aData));
  return NS_OK;
}

mozilla::ipc::IPCResult
ContentChild::RecvPreferenceUpdate(const Pref& aPref)
{
  Preferences::SetPreference(aPref);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvVarUpdate(const GfxVarUpdate& aVar)
{
  gfx::gfxVars::ApplyUpdate(aVar);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvDataStoragePut(const nsString& aFilename,
                                 const DataStorageItem& aItem)
{
  RefPtr<DataStorage> storage = DataStorage::GetFromRawFileName(aFilename);
  if (storage) {
    storage->Put(aItem.key(), aItem.value(), aItem.type());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvDataStorageRemove(const nsString& aFilename,
                                    const nsCString& aKey,
                                    const DataStorageType& aType)
{
  RefPtr<DataStorage> storage = DataStorage::GetFromRawFileName(aFilename);
  if (storage) {
    storage->Remove(aKey, aType);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvDataStorageClear(const nsString& aFilename)
{
  RefPtr<DataStorage> storage = DataStorage::GetFromRawFileName(aFilename);
  if (storage) {
    storage->Clear();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
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
  return IPC_OK();
}

// NOTE: This method is being run in the SystemGroup, and thus cannot directly
// touch pages. See GetSpecificMessageEventTarget.
mozilla::ipc::IPCResult
ContentChild::RecvNotifyVisited(nsTArray<URIParams>&& aURIs)
{
  for (const URIParams& uri : aURIs) {
    nsCOMPtr<nsIURI> newURI = DeserializeURI(uri);
    if (!newURI) {
      return IPC_FAIL_NO_REASON(this);
    }
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
      history->NotifyVisited(newURI);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvLoadProcessScript(const nsString& aURL)
{
  ProcessGlobal* global = ProcessGlobal::Get();
  global->LoadScript(aURL);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvAsyncMessage(const nsString& aMsg,
                               InfallibleTArray<CpowEntry>&& aCpows,
                               const IPC::Principal& aPrincipal,
                               const ClonedMessageData& aData)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "ContentChild::RecvAsyncMessage", EVENTS, aMsg);

  CrossProcessCpowHolder cpows(this, aCpows);
  RefPtr<nsFrameMessageManager> cpm =
    nsFrameMessageManager::GetChildProcessManager();
  if (cpm) {
    StructuredCloneData data;
    ipc::UnpackClonedMessageDataForChild(aData, data);
    cpm->ReceiveMessage(cpm, nullptr, aMsg, false, &data, &cpows, aPrincipal, nullptr,
                        IgnoreErrors());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvGeolocationUpdate(nsIDOMGeoPosition* aPosition)
{
  nsCOMPtr<nsIGeolocationUpdate> gs =
    do_GetService("@mozilla.org/geolocation/service;1");
  if (!gs) {
    return IPC_OK();
  }
  gs->Update(aPosition);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvGeolocationError(const uint16_t& errorCode)
{
  nsCOMPtr<nsIGeolocationUpdate> gs =
    do_GetService("@mozilla.org/geolocation/service;1");
  if (!gs) {
    return IPC_OK();
  }
  gs->NotifyError(errorCode);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvUpdateDictionaryList(InfallibleTArray<nsString>&& aDictionaries)
{
  mAvailableDictionaries = aDictionaries;
  mozInlineSpellChecker::UpdateCanEnableInlineSpellChecking();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvUpdateFontList(InfallibleTArray<SystemFontListEntry>&& aFontList)
{
  mFontList = Move(aFontList);
  gfxPlatform::GetPlatform()->UpdateFontList();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvUpdateAppLocales(nsTArray<nsCString>&& aAppLocales)
{
  LocaleService::GetInstance()->AssignAppLocales(aAppLocales);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvUpdateRequestedLocales(nsTArray<nsCString>&& aRequestedLocales)
{
  LocaleService::GetInstance()->AssignRequestedLocales(aRequestedLocales);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvAddPermission(const IPC::Permission& permission)
{
  nsCOMPtr<nsIPermissionManager> permissionManagerIface =
    services::GetPermissionManager();
  nsPermissionManager* permissionManager =
    static_cast<nsPermissionManager*>(permissionManagerIface.get());
  MOZ_ASSERT(permissionManager,
         "We have no permissionManager in the Content process !");

  // note we do not need to force mUserContextId to the default here because
  // the permission manager does that internally.
  nsAutoCString originNoSuffix;
  OriginAttributes attrs;
  bool success = attrs.PopulateFromOrigin(permission.origin, originNoSuffix);
  NS_ENSURE_TRUE(success, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsIPrincipal> principal = mozilla::BasePrincipal::CreateCodebasePrincipal(uri, attrs);

  // child processes don't care about modification time.
  int64_t modificationTime = 0;

  permissionManager->AddInternal(principal,
                                 nsCString(permission.type),
                                 permission.capability,
                                 0,
                                 permission.expireType,
                                 permission.expireTime,
                                 modificationTime,
                                 nsPermissionManager::eNotify,
                                 nsPermissionManager::eNoDBOperation);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvFlushMemory(const nsString& reason)
{
  nsCOMPtr<nsIObserverService> os =
    mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "memory-pressure", reason.get());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvActivateA11y(const uint32_t& aMainChromeTid,
                               const uint32_t& aMsaaID)
{
#ifdef ACCESSIBILITY
#ifdef XP_WIN
  MOZ_ASSERT(aMainChromeTid != 0);
  mMainChromeTid = aMainChromeTid;

  MOZ_ASSERT(aMsaaID != 0);
  mMsaaID = aMsaaID;
#endif // XP_WIN

  // Start accessibility in content process if it's running in chrome
  // process.
  GetOrCreateAccService(nsAccessibilityService::eMainProcess);
#endif // ACCESSIBILITY
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvShutdownA11y()
{
#ifdef ACCESSIBILITY
  // Try to shutdown accessibility in content process if it's shutting down in
  // chrome process.
  MaybeShutdownAccService(nsAccessibilityService::eMainProcess);
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvGarbageCollect()
{
  // Rebroadcast the "child-gc-request" so that workers will GC.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "child-gc-request", nullptr);
  }
  nsJSContext::GarbageCollectNow(JS::gcreason::DOM_IPC);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvCycleCollect()
{
  // Rebroadcast the "child-cc-request" so that workers will CC.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "child-cc-request", nullptr);
  }
  nsJSContext::CycleCollectNow();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvUnlinkGhosts()
{
#ifdef DEBUG
  nsWindowMemoryReporter::UnlinkGhostWindows();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvAppInfo(const nsCString& version, const nsCString& buildID,
                          const nsCString& name, const nsCString& UAName,
                          const nsCString& ID, const nsCString& vendor,
                          const nsCString& sourceURL)
{
  mAppInfo.version.Assign(version);
  mAppInfo.buildID.Assign(buildID);
  mAppInfo.name.Assign(name);
  mAppInfo.UAName.Assign(UAName);
  mAppInfo.ID.Assign(ID);
  mAppInfo.vendor.Assign(vendor);
  mAppInfo.sourceURL.Assign(sourceURL);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvRemoteType(const nsString& aRemoteType)
{
  MOZ_ASSERT(DOMStringIsNull(mRemoteType));

  mRemoteType.Assign(aRemoteType);

  // For non-default ("web") types, update the process name so about:memory's
  // process names are more obvious.
  if (aRemoteType.EqualsLiteral(FILE_REMOTE_TYPE)) {
    SetProcessName(NS_LITERAL_STRING("file:// Content"));
  } else if (aRemoteType.EqualsLiteral(EXTENSION_REMOTE_TYPE)) {
    SetProcessName(NS_LITERAL_STRING("WebExtensions"));
  } else if (aRemoteType.EqualsLiteral(LARGE_ALLOCATION_REMOTE_TYPE)) {
    SetProcessName(NS_LITERAL_STRING("Large Allocation Web Content"));
  }

  return IPC_OK();
}

const nsAString&
ContentChild::GetRemoteType() const
{
  return mRemoteType;
}

mozilla::ipc::IPCResult
ContentChild::RecvInitServiceWorkers(const ServiceWorkerConfiguration& aConfig)
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown began
    return IPC_OK();
  }
  swm->LoadRegistrations(aConfig.serviceWorkerRegistrations());
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvInitBlobURLs(nsTArray<BlobURLRegistrationData>&& aRegistrations)
{
  for (uint32_t i = 0; i < aRegistrations.Length(); ++i) {
    BlobURLRegistrationData& registration = aRegistrations[i];
    RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(registration.blob());
    MOZ_ASSERT(blobImpl);

    nsHostObjectProtocolHandler::AddDataEntry(registration.url(),
                                              registration.principal(),
                                              blobImpl);
    // If we have received an already-revoked blobURL, we have to keep it alive
    // for a while (see nsHostObjectProtocolHandler) in order to support pending
    // operations such as navigation, download and so on.
    if (registration.revoked()) {
      nsHostObjectProtocolHandler::RemoveDataEntry(registration.url(),
                                                   false);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvLastPrivateDocShellDestroyed()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvNotifyProcessPriorityChanged(
  const hal::ProcessPriority& aPriority)
{
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  NS_ENSURE_TRUE(os, IPC_OK());

  RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
  props->SetPropertyAsInt32(NS_LITERAL_STRING("priority"),
                            static_cast<int32_t>(aPriority));

  os->NotifyObservers(static_cast<nsIPropertyBag2*>(props),
                      "ipc:process-priority-changed",  nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvMinimizeMemoryUsage()
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  NS_ENSURE_TRUE(mgr, IPC_OK());

  Unused << mgr->MinimizeMemoryUsage(/* callback = */ nullptr);
  return IPC_OK();
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

mozilla::ipc::IPCResult
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
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvLoadAndRegisterSheet(const URIParams& aURI, const uint32_t& aType)
{
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    return IPC_OK();
  }

  nsStyleSheetService *sheetService = nsStyleSheetService::GetInstance();
  if (sheetService) {
    sheetService->LoadAndRegisterSheet(uri, aType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvUnregisterSheet(const URIParams& aURI, const uint32_t& aType)
{
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    return IPC_OK();
  }

  nsStyleSheetService *sheetService = nsStyleSheetService::GetInstance();
  if (sheetService) {
    sheetService->UnregisterSheet(uri, aType);
  }

  return IPC_OK();
}

POfflineCacheUpdateChild*
ContentChild::AllocPOfflineCacheUpdateChild(const URIParams& manifestURI,
                                            const URIParams& documentURI,
                                            const PrincipalInfo& aLoadingPrincipalInfo,
                                            const bool& stickDocument)
{
  MOZ_CRASH("unused");
  return nullptr;
}

bool
ContentChild::DeallocPOfflineCacheUpdateChild(POfflineCacheUpdateChild* actor)
{
  OfflineCacheUpdateChild* offlineCacheUpdate =
    static_cast<OfflineCacheUpdateChild*>(actor);
  NS_RELEASE(offlineCacheUpdate);
  return true;
}

mozilla::ipc::IPCResult
ContentChild::RecvDomainSetChanged(const uint32_t& aSetType,
                                   const uint32_t& aChangeType,
                                   const OptionalURIParams& aDomain)
{
  if (aChangeType == ACTIVATE_POLICY) {
    if (mPolicy) {
      return IPC_OK();
    }
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);
    ssm->ActivateDomainPolicyInternal(getter_AddRefs(mPolicy));
    if (!mPolicy) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }
  if (!mPolicy) {
    MOZ_ASSERT_UNREACHABLE("If the domain policy is not active yet,"
                           " the first message should be ACTIVATE_POLICY");
    return IPC_FAIL_NO_REASON(this);
  }

  NS_ENSURE_TRUE(mPolicy, IPC_FAIL_NO_REASON(this));

  if (aChangeType == DEACTIVATE_POLICY) {
    mPolicy->Deactivate();
    mPolicy = nullptr;
    return IPC_OK();
  }

  nsCOMPtr<nsIDomainSet> set;
  switch(aSetType) {
    case BLACKLIST:
      mPolicy->GetBlacklist(getter_AddRefs(set));
      break;
    case SUPER_BLACKLIST:
      mPolicy->GetSuperBlacklist(getter_AddRefs(set));
      break;
    case WHITELIST:
      mPolicy->GetWhitelist(getter_AddRefs(set));
      break;
    case SUPER_WHITELIST:
      mPolicy->GetSuperWhitelist(getter_AddRefs(set));
      break;
    default:
      NS_NOTREACHED("Unexpected setType");
      return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(set);

  nsCOMPtr<nsIURI> uri = DeserializeURI(aDomain);

  switch(aChangeType) {
    case ADD_DOMAIN:
      NS_ENSURE_TRUE(uri, IPC_FAIL_NO_REASON(this));
      set->Add(uri);
      break;
    case REMOVE_DOMAIN:
      NS_ENSURE_TRUE(uri, IPC_FAIL_NO_REASON(this));
      set->Remove(uri);
      break;
    case CLEAR_DOMAINS:
      set->Clear();
      break;
    default:
      NS_NOTREACHED("Unexpected changeType");
      return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

void
ContentChild::StartForceKillTimer()
{
  if (mForceKillTimer) {
    return;
  }

  int32_t timeoutSecs = Preferences::GetInt("dom.ipc.tabs.shutdownTimeoutSecs", 5);
  if (timeoutSecs > 0) {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mForceKillTimer),
                                ContentChild::ForceKillTimerCallback,
                                this,
                                timeoutSecs * 1000,
                                nsITimer::TYPE_ONE_SHOT,
                                "dom::ContentChild::StartForceKillTimer");
    MOZ_ASSERT(mForceKillTimer);
  }
}

/* static */ void
ContentChild::ForceKillTimerCallback(nsITimer* aTimer, void* aClosure)
{
  ProcessChild::QuickExit();
}

mozilla::ipc::IPCResult
ContentChild::RecvShutdown()
{
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(static_cast<nsIContentChild*>(this),
                          "content-child-will-shutdown", nullptr);
  }

  ShutdownInternal();
  return IPC_OK();
}

void
ContentChild::ShutdownInternal()
{
  // If we receive the shutdown message from within a nested event loop, we want
  // to wait for that event loop to finish. Otherwise we could prematurely
  // terminate an "unload" or "pagehide" event handler (which might be doing a
  // sync XHR, for example).
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IPCShutdownState"),
                                     NS_LITERAL_CSTRING("RecvShutdown"));

  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsThread> mainThread = nsThreadManager::get().GetCurrentThread();
  // Note that we only have to check the recursion count for the current
  // cooperative thread. Since the Shutdown message is not labeled with a
  // SchedulerGroup, there can be no other cooperative threads doing work while
  // we're running.
  if (mainThread && mainThread->RecursionDepth() > 1) {
    // We're in a nested event loop. Let's delay for an arbitrary period of
    // time (100ms) in the hopes that the event loop will have finished by
    // then.
    MessageLoop::current()->PostDelayedTask(
      NewRunnableMethod(
        "dom::ContentChild::RecvShutdown", this,
        &ContentChild::ShutdownInternal),
      100);
    return;
  }

  mShuttingDown = true;

#ifdef NIGHTLY_BUILD
  HangMonitor::UnregisterAnnotator(PendingInputEventHangAnnotator::sSingleton);
#endif

  if (mPolicy) {
    mPolicy->Deactivate();
    mPolicy = nullptr;
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(static_cast<nsIContentChild*>(this),
                          "content-child-shutdown", nullptr);
  }

#if defined(XP_WIN)
    mozilla::widget::StopAudioSession();
#endif

  GetIPCChannel()->SetAbortOnError(false);

#ifdef MOZ_GECKO_PROFILER
  if (mProfilerController) {
    nsCString shutdownProfile = mProfilerController->GrabShutdownProfileAndShutdown();
    mProfilerController = nullptr;
    // Send the shutdown profile to the parent process through our own
    // message channel, which we know will survive for long enough.
    Unused << SendShutdownProfile(shutdownProfile);
  }
#endif

  // Start a timer that will insure we quickly exit after a reasonable
  // period of time. Prevents shutdown hangs after our connection to the
  // parent closes.
  StartForceKillTimer();

  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IPCShutdownState"),
                                     NS_LITERAL_CSTRING("SendFinishShutdown (sending)"));
  bool sent = SendFinishShutdown();
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IPCShutdownState"),
                                     sent ? NS_LITERAL_CSTRING("SendFinishShutdown (sent)")
                                          : NS_LITERAL_CSTRING("SendFinishShutdown (failed)"));
}

PBrowserOrId
ContentChild::GetBrowserOrId(TabChild* aTabChild)
{
  if (!aTabChild ||
    this == aTabChild->Manager()) {
    return PBrowserOrId(aTabChild);
  }
  return PBrowserOrId(aTabChild->GetTabId());
}

mozilla::ipc::IPCResult
ContentChild::RecvUpdateWindow(const uintptr_t& aChildId)
{
#if defined(XP_WIN)
  NS_ASSERTION(aChildId, "Expected child hwnd value for remote plugin instance.");
  mozilla::plugins::PluginInstanceParent* parentInstance =
  mozilla::plugins::PluginInstanceParent::LookupPluginInstanceByID(aChildId);
  if (parentInstance) {
    // sync! update call to the plugin instance that forces the
    // plugin to paint its child window.
    if(!parentInstance->CallUpdateWindow()) {
      return IPC_FAIL_NO_REASON(this);
    }
  }
  return IPC_OK();
#else
  MOZ_ASSERT(false, "ContentChild::RecvUpdateWindow calls unexpected on this platform.");
  return IPC_FAIL_NO_REASON(this);
#endif
}

PContentPermissionRequestChild*
ContentChild::AllocPContentPermissionRequestChild(const InfallibleTArray<PermissionRequest>& aRequests,
                                                  const IPC::Principal& aPrincipal,
                                                  const bool& aIsHandlingUserInput,
                                                  const TabId& aTabId)
{
  MOZ_CRASH("unused");
  return nullptr;
}

bool
ContentChild::DeallocPContentPermissionRequestChild(PContentPermissionRequestChild* actor)
{
  nsContentPermissionUtils::NotifyRemoveContentPermissionRequestChild(actor);
  auto child = static_cast<RemotePermissionRequest*>(actor);
  child->IPDLRelease();
  return true;
}

PWebBrowserPersistDocumentChild*
ContentChild::AllocPWebBrowserPersistDocumentChild(PBrowserChild* aBrowser,
                                                   const uint64_t& aOuterWindowID)
{
  return new WebBrowserPersistDocumentChild();
}

mozilla::ipc::IPCResult
ContentChild::RecvPWebBrowserPersistDocumentConstructor(PWebBrowserPersistDocumentChild *aActor,
                                                        PBrowserChild* aBrowser,
                                                        const uint64_t& aOuterWindowID)
{
  if (NS_WARN_IF(!aBrowser)) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIDocument> rootDoc =
    static_cast<TabChild*>(aBrowser)->GetDocument();
  nsCOMPtr<nsIDocument> foundDoc;
  if (aOuterWindowID) {
    foundDoc = nsContentUtils::GetSubdocumentWithOuterWindowId(rootDoc, aOuterWindowID);
  } else {
    foundDoc = rootDoc;
  }

  if (!foundDoc) {
    aActor->SendInitFailure(NS_ERROR_NO_CONTENT);
  } else {
    static_cast<WebBrowserPersistDocumentChild*>(aActor)->Start(foundDoc);
  }
  return IPC_OK();
}

bool
ContentChild::DeallocPWebBrowserPersistDocumentChild(PWebBrowserPersistDocumentChild* aActor)
{
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult
ContentChild::RecvSetAudioSessionData(const nsID& aId,
                                      const nsString& aDisplayName,
                                      const nsString& aIconPath)
{
#if defined(XP_WIN)
    if (NS_FAILED(mozilla::widget::RecvAudioSessionData(aId, aDisplayName,
                                                        aIconPath))) {
      return IPC_OK();
    }

    // Ignore failures here; we can't really do anything about them
    mozilla::widget::StartAudioSession();
    return IPC_OK();
#else
    MOZ_CRASH("Not Reached!");
#endif
}

// This code goes here rather than nsGlobalWindow.cpp because nsGlobalWindow.cpp
// can't include ContentChild.h since it includes windows.h.

static uint64_t gNextWindowID = 0;

// We use only 53 bits for the window ID so that it can be converted to and from
// a JS value without loss of precision. The upper bits of the window ID hold the
// process ID. The lower bits identify the window.
static const uint64_t kWindowIDTotalBits = 53;
static const uint64_t kWindowIDProcessBits = 22;
static const uint64_t kWindowIDWindowBits = kWindowIDTotalBits - kWindowIDProcessBits;

// Try to return a window ID that is unique across processes and that will never
// be recycled.
uint64_t
NextWindowID()
{
  uint64_t processID = 0;
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    processID = cc->GetID();
  }

  MOZ_RELEASE_ASSERT(processID < (uint64_t(1) << kWindowIDProcessBits));
  uint64_t processBits = processID & ((uint64_t(1) << kWindowIDProcessBits) - 1);

  // Make sure no actual window ends up with mWindowID == 0.
  uint64_t windowID = ++gNextWindowID;

  MOZ_RELEASE_ASSERT(windowID < (uint64_t(1) << kWindowIDWindowBits));
  uint64_t windowBits = windowID & ((uint64_t(1) << kWindowIDWindowBits) - 1);

  return (processBits << kWindowIDWindowBits) | windowBits;
}

mozilla::ipc::IPCResult
ContentChild::RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                                    const uint32_t& aAction)
{
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService) {
    dragService->StartDragSession();
    nsCOMPtr<nsIDragSession> session;
    dragService->GetCurrentSession(getter_AddRefs(session));
    if (session) {
      session->SetDragAction(aAction);
      // Check if we are receiving any file objects. If we are we will want
      // to hide any of the other objects coming in from content.
      bool hasFiles = false;
      for (uint32_t i = 0; i < aTransfers.Length() && !hasFiles; ++i) {
        auto& items = aTransfers[i].items();
        for (uint32_t j = 0; j < items.Length() && !hasFiles; ++j) {
          if (items[j].data().type() == IPCDataTransferData::TIPCBlob) {
            hasFiles = true;
          }
        }
      }

      // Add the entries from the IPC to the new DataTransfer
      nsCOMPtr<DataTransfer> dataTransfer =
        new DataTransfer(nullptr, eDragStart, false, -1);
      for (uint32_t i = 0; i < aTransfers.Length(); ++i) {
        auto& items = aTransfers[i].items();
        for (uint32_t j = 0; j < items.Length(); ++j) {
          const IPCDataTransferItem& item = items[j];
          RefPtr<nsVariantCC> variant = new nsVariantCC();
          if (item.data().type() == IPCDataTransferData::TnsString) {
            const nsString& data = item.data().get_nsString();
            variant->SetAsAString(data);
          } else if (item.data().type() == IPCDataTransferData::TShmem) {
            Shmem data = item.data().get_Shmem();
            variant->SetAsACString(nsDependentCString(data.get<char>(), data.Size<char>()));
            Unused << DeallocShmem(data);
          } else if (item.data().type() == IPCDataTransferData::TIPCBlob) {
            RefPtr<BlobImpl> blobImpl =
              IPCBlobUtils::Deserialize(item.data().get_IPCBlob());
            variant->SetAsISupports(blobImpl);
          } else {
            continue;
          }
          // We should hide this data from content if we have a file, and we aren't a file.
          bool hidden = hasFiles && item.data().type() != IPCDataTransferData::TIPCBlob;
          dataTransfer->SetDataWithPrincipalFromOtherProcess(
            NS_ConvertUTF8toUTF16(item.flavor()), variant, i,
            nsContentUtils::GetSystemPrincipal(), hidden);
        }
      }
      session->SetDataTransfer(dataTransfer);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvEndDragSession(const bool& aDoneDrag,
                                 const bool& aUserCancelled,
                                 const LayoutDeviceIntPoint& aDragEndPoint,
                                 const uint32_t& aKeyModifiers)
{
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService) {
    if (aUserCancelled) {
      nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
      if (dragSession) {
        dragSession->UserCancelled();
      }
    }
    static_cast<nsBaseDragService*>(dragService.get())->SetDragEndPoint(aDragEndPoint);
    dragService->EndDragSession(aDoneDrag, aKeyModifiers);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvPush(const nsCString& aScope,
                       const IPC::Principal& aPrincipal,
                       const nsString& aMessageId)
{
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Nothing());
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvPushWithData(const nsCString& aScope,
                               const IPC::Principal& aPrincipal,
                               const nsString& aMessageId,
                               InfallibleTArray<uint8_t>&& aData)
{
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Some(aData));
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvPushSubscriptionChange(const nsCString& aScope,
                                         const IPC::Principal& aPrincipal)
{
  PushSubscriptionChangeDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvPushError(const nsCString& aScope, const IPC::Principal& aPrincipal,
                            const nsString& aMessage, const uint32_t& aFlags)
{
  PushErrorDispatcher dispatcher(aScope, aPrincipal, aMessage, aFlags);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvNotifyPushSubscriptionModifiedObservers(const nsCString& aScope,
                                                          const IPC::Principal& aPrincipal)
{
  PushSubscriptionModifiedDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObservers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvBlobURLRegistration(const nsCString& aURI,
                                      const IPCBlob& aBlob,
                                      const IPC::Principal& aPrincipal)
{
  RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(aBlob);
  MOZ_ASSERT(blobImpl);

  nsHostObjectProtocolHandler::AddDataEntry(aURI, aPrincipal, blobImpl);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvBlobURLUnregistration(const nsCString& aURI)
{
  nsHostObjectProtocolHandler::RemoveDataEntry(aURI,
                                               /* aBroadcastToOtherProcesses = */ false);
  return IPC_OK();
}

#if defined(XP_WIN) && defined(ACCESSIBILITY)
bool
ContentChild::SendGetA11yContentId()
{
  return PContentChild::SendGetA11yContentId(&mMsaaID);
}
#endif // defined(XP_WIN) && defined(ACCESSIBILITY)

void
ContentChild::CreateGetFilesRequest(const nsAString& aDirectoryPath,
                                    bool aRecursiveFlag,
                                    nsID& aUUID,
                                    GetFilesHelperChild* aChild)
{
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(!mGetFilesPendingRequests.GetWeak(aUUID));

  Unused << SendGetFilesRequest(aUUID, nsString(aDirectoryPath),
                                aRecursiveFlag);
  mGetFilesPendingRequests.Put(aUUID, aChild);
}

void
ContentChild::DeleteGetFilesRequest(nsID& aUUID, GetFilesHelperChild* aChild)
{
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(mGetFilesPendingRequests.GetWeak(aUUID));

  Unused << SendDeleteGetFilesRequest(aUUID);
  mGetFilesPendingRequests.Remove(aUUID);
}

mozilla::ipc::IPCResult
ContentChild::RecvGetFilesResponse(const nsID& aUUID,
                                   const GetFilesResponseResult& aResult)
{
  GetFilesHelperChild* child = mGetFilesPendingRequests.GetWeak(aUUID);
  // This object can already been deleted in case DeleteGetFilesRequest has
  // been called when the response was sending by the parent.
  if (!child) {
    return IPC_OK();
  }

  if (aResult.type() == GetFilesResponseResult::TGetFilesResponseFailure) {
    child->Finished(aResult.get_GetFilesResponseFailure().errorCode());
  } else {
    MOZ_ASSERT(aResult.type() == GetFilesResponseResult::TGetFilesResponseSuccess);

    const nsTArray<IPCBlob>& ipcBlobs =
      aResult.get_GetFilesResponseSuccess().blobs();

    bool succeeded = true;
    for (uint32_t i = 0; succeeded && i < ipcBlobs.Length(); ++i) {
      RefPtr<BlobImpl> impl = IPCBlobUtils::Deserialize(ipcBlobs[i]);
      succeeded = child->AppendBlobImpl(impl);
    }

    child->Finished(succeeded ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
  }

  mGetFilesPendingRequests.Remove(aUUID);
  return IPC_OK();
}

/* static */ void
ContentChild::FatalErrorIfNotUsingGPUProcess(const char* const aErrorMsg,
                                             base::ProcessId aOtherPid)
{
  // If we're communicating with the same process or the UI process then we
  // want to crash normally. Otherwise we want to just warn as the other end
  // must be the GPU process and it crashing shouldn't be fatal for us.
  if (aOtherPid == base::GetCurrentProcId() ||
      (GetSingleton() && GetSingleton()->OtherPid() == aOtherPid)) {
    mozilla::ipc::FatalError(aErrorMsg, false);
  } else {
    nsAutoCString formattedMessage("IPDL error: \"");
    formattedMessage.AppendASCII(aErrorMsg);
    formattedMessage.AppendLiteral(R"(".)");
    NS_WARNING(formattedMessage.get());
  }
}

PURLClassifierChild*
ContentChild::AllocPURLClassifierChild(const Principal& aPrincipal,
                                       const bool& aUseTrackingProtection,
                                       bool* aSuccess)
{
  *aSuccess = true;
  return new URLClassifierChild();
}

bool
ContentChild::DeallocPURLClassifierChild(PURLClassifierChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

PURLClassifierLocalChild*
ContentChild::AllocPURLClassifierLocalChild(const URIParams& aUri,
                                            const nsCString& aTables)
{
  return new URLClassifierLocalChild();
}

bool
ContentChild::DeallocPURLClassifierLocalChild(PURLClassifierLocalChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

PLoginReputationChild*
ContentChild::AllocPLoginReputationChild(const URIParams& aUri)
{
  return new PLoginReputationChild();
}

bool
ContentChild::DeallocPLoginReputationChild(PLoginReputationChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

// The IPC code will call this method asking us to assign an event target to new
// actors created by the ContentParent.
already_AddRefed<nsIEventTarget>
ContentChild::GetConstructedEventTarget(const Message& aMsg)
{
  // Currently we only set targets for PBrowser.
  if (aMsg.type() != PContent::Msg_PBrowserConstructor__ID) {
    return nullptr;
  }

  return nsIContentChild::GetConstructedEventTarget(aMsg);
}

void
ContentChild::FileCreationRequest(nsID& aUUID, FileCreatorHelper* aHelper,
                                  const nsAString& aFullPath,
                                  const nsAString& aType,
                                  const nsAString& aName,
                                  const Optional<int64_t>& aLastModified,
                                  bool aExistenceCheck,
                                  bool aIsFromNsIFile)
{
  MOZ_ASSERT(aHelper);

  bool lastModifiedPassed = false;
  int64_t lastModified = 0;
  if (aLastModified.WasPassed()) {
    lastModifiedPassed = true;
    lastModified = aLastModified.Value();
  }

  Unused << SendFileCreationRequest(aUUID, nsString(aFullPath), nsString(aType),
                                    nsString(aName), lastModifiedPassed,
                                    lastModified, aExistenceCheck,
                                    aIsFromNsIFile);
  mFileCreationPending.Put(aUUID, aHelper);
}

mozilla::ipc::IPCResult
ContentChild::RecvFileCreationResponse(const nsID& aUUID,
                                       const FileCreationResult& aResult)
{
  FileCreatorHelper* helper = mFileCreationPending.GetWeak(aUUID);
  if (!helper) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (aResult.type() == FileCreationResult::TFileCreationErrorResult) {
    helper->ResponseReceived(nullptr,
                             aResult.get_FileCreationErrorResult().errorCode());
  } else {
    MOZ_ASSERT(aResult.type() == FileCreationResult::TFileCreationSuccessResult);

    RefPtr<BlobImpl> impl =
      IPCBlobUtils::Deserialize(aResult.get_FileCreationSuccessResult().blob());
    helper->ResponseReceived(impl, NS_OK);
  }

  mFileCreationPending.Remove(aUUID);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvActivate(PBrowserChild* aTab)
{
  TabChild* tab = static_cast<TabChild*>(aTab);
  return tab->RecvActivate();
}

mozilla::ipc::IPCResult
ContentChild::RecvDeactivate(PBrowserChild* aTab)
{
  TabChild* tab = static_cast<TabChild*>(aTab);
  return tab->RecvDeactivate();
}

mozilla::ipc::IPCResult
ContentChild::RecvProvideAnonymousTemporaryFile(const uint64_t& aID,
                                                const FileDescOrError& aFDOrError)
{
  nsAutoPtr<AnonymousTemporaryFileCallback> callback;
  mPendingAnonymousTemporaryFiles.Remove(aID, &callback);
  MOZ_ASSERT(callback);

  PRFileDesc* prfile = nullptr;
  if (aFDOrError.type() == FileDescOrError::Tnsresult) {
    DebugOnly<nsresult> rv = aFDOrError.get_nsresult();
    MOZ_ASSERT(NS_FAILED(rv));
  } else {
    auto rawFD = aFDOrError.get_FileDescriptor().ClonePlatformHandle();
    prfile = PR_ImportFile(PROsfd(rawFD.release()));
  }
  (*callback)(prfile);
  return IPC_OK();
}

nsresult
ContentChild::AsyncOpenAnonymousTemporaryFile(const AnonymousTemporaryFileCallback& aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  static uint64_t id = 0;
  auto newID = id++;
  if (!SendRequestAnonymousTemporaryFile(newID)) {
    return NS_ERROR_FAILURE;
  }

  // Remember the association with the callback.
  MOZ_ASSERT(!mPendingAnonymousTemporaryFiles.Get(newID));
  mPendingAnonymousTemporaryFiles.LookupOrAdd(newID, aCallback);
  return NS_OK;
}

mozilla::ipc::IPCResult
ContentChild::RecvSetPermissionsWithKey(const nsCString& aPermissionKey,
                                        nsTArray<IPC::Permission>&& aPerms)
{
  nsCOMPtr<nsIPermissionManager> permissionManager =
    services::GetPermissionManager();
  permissionManager->SetPermissionsWithKey(aPermissionKey, aPerms);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvRefreshScreens(nsTArray<ScreenDetails>&& aScreens)
{
  ScreenManager& screenManager = ScreenManager::GetSingleton();
  screenManager.Refresh(Move(aScreens));
  return IPC_OK();
}

already_AddRefed<nsIEventTarget>
ContentChild::GetEventTargetFor(TabChild* aTabChild)
{
  return IToplevelProtocol::GetActorEventTarget(aTabChild);
}

mozilla::ipc::IPCResult
ContentChild::RecvSetPluginList(const uint32_t& aPluginEpoch,
                                nsTArray<plugins::PluginTag>&& aPluginTags,
                                nsTArray<plugins::FakePluginTag>&& aFakePluginTags)
{
  RefPtr<nsPluginHost> host = nsPluginHost::GetInst();
  host->SetPluginsInContent(aPluginEpoch, aPluginTags, aFakePluginTags);
  return IPC_OK();
}

PClientOpenWindowOpChild*
ContentChild::AllocPClientOpenWindowOpChild(const ClientOpenWindowArgs& aArgs)
{
  return AllocClientOpenWindowOpChild();
}

IPCResult
ContentChild::RecvPClientOpenWindowOpConstructor(PClientOpenWindowOpChild* aActor,
                                                 const ClientOpenWindowArgs& aArgs)
{
  InitClientOpenWindowOpChild(aActor, aArgs);
  return IPC_OK();
}

bool
ContentChild::DeallocPClientOpenWindowOpChild(PClientOpenWindowOpChild* aActor)
{
  return DeallocClientOpenWindowOpChild(aActor);
}

mozilla::ipc::IPCResult
ContentChild::RecvShareCodeCoverageMutex(const CrossProcessMutexHandle& aHandle)
{
#ifdef MOZ_CODE_COVERAGE
  CodeCoverageHandler::Init(aHandle);
  return IPC_OK();
#else
  MOZ_CRASH("Shouldn't receive this message in non-code coverage builds!");
#endif
}

mozilla::ipc::IPCResult
ContentChild::RecvDumpCodeCoverageCounters()
{
#ifdef MOZ_CODE_COVERAGE
  CodeCoverageHandler::DumpCounters(0);
  return IPC_OK();
#else
  MOZ_CRASH("Shouldn't receive this message in non-code coverage builds!");
#endif
}

mozilla::ipc::IPCResult
ContentChild::RecvResetCodeCoverageCounters()
{
#ifdef MOZ_CODE_COVERAGE
  CodeCoverageHandler::ResetCounters(0);
  return IPC_OK();
#else
  MOZ_CRASH("Shouldn't receive this message in non-code coverage builds!");
#endif
}

mozilla::ipc::IPCResult
ContentChild::RecvSetInputEventQueueEnabled()
{
  nsThreadManager::get().EnableMainThreadEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvFlushInputEventQueue()
{
  nsThreadManager::get().FlushInputEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvSuspendInputEventQueue()
{
  nsThreadManager::get().SuspendInputEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvResumeInputEventQueue()
{
  nsThreadManager::get().ResumeInputEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvAddDynamicScalars(nsTArray<DynamicScalarDefinition>&& aDefs)
{
  TelemetryIPC::AddDynamicScalarDefinitions(aDefs);
  return IPC_OK();
}

already_AddRefed<nsIEventTarget>
ContentChild::GetSpecificMessageEventTarget(const Message& aMsg)
{
  switch(aMsg.type()) {
    // Javascript
    case PJavaScript::Msg_DropTemporaryStrongReferences__ID:
    case PJavaScript::Msg_DropObject__ID:

    // Navigation
    case PContent::Msg_NotifyVisited__ID:

    // Storage API
    case PContent::Msg_DataStoragePut__ID:
    case PContent::Msg_DataStorageRemove__ID:
    case PContent::Msg_DataStorageClear__ID:

    // Blob and BlobURL
    case PContent::Msg_BlobURLRegistration__ID:
    case PContent::Msg_BlobURLUnregistration__ID:
    case PContent::Msg_InitBlobURLs__ID:
    case PContent::Msg_PIPCBlobInputStreamConstructor__ID:
    case PContent::Msg_StoreAndBroadcastBlobURLRegistration__ID:

      return do_AddRef(SystemGroup::EventTargetFor(TaskCategory::Other));

    default:
      return nullptr;
  }
}

#ifdef NIGHTLY_BUILD
void
ContentChild::OnChannelReceivedMessage(const Message& aMsg)
{
  if (nsContentUtils::IsMessageInputEvent(aMsg)) {
    mPendingInputEvents++;
  }
}

PContentChild::Result
ContentChild::OnMessageReceived(const Message& aMsg)
{
  if (nsContentUtils::IsMessageInputEvent(aMsg)) {
    DebugOnly<uint32_t> prevEvts = mPendingInputEvents--;
    MOZ_ASSERT(prevEvts > 0);
  }

  return PContentChild::OnMessageReceived(aMsg);
}

PContentChild::Result
ContentChild::OnMessageReceived(const Message& aMsg, Message*& aReply)
{
  return PContentChild::OnMessageReceived(aMsg, aReply);
}
#endif

} // namespace dom

#if !defined(XP_WIN)
bool IsDevelopmentBuild()
{
  nsCOMPtr<nsIFile> path = mozilla::Omnijar::GetPath(mozilla::Omnijar::GRE);
  // If the path doesn't exist, we're a dev build.
  return path == nullptr;
}
#endif /* !XP_WIN */

#if defined(XP_MACOSX)
/*
 * Helper function to read a string value for a given key from the .app's
 * Info.plist.
 */
static nsresult
GetStringValueFromBundlePlist(const nsAString& aKey, nsAutoCString& aValue)
{
  CFBundleRef mainBundle = CFBundleGetMainBundle();
  if (mainBundle == nullptr) {
    return NS_ERROR_FAILURE;
  }

  // Read this app's bundle Info.plist as a dictionary
  CFDictionaryRef bundleInfoDict = CFBundleGetInfoDictionary(mainBundle);
  if (bundleInfoDict == nullptr) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString keyAutoCString = NS_ConvertUTF16toUTF8(aKey);
  CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault,
                                              keyAutoCString.get(),
                                              kCFStringEncodingUTF8);
  if (key == nullptr) {
    return NS_ERROR_FAILURE;
  }

  CFStringRef value = (CFStringRef)CFDictionaryGetValue(bundleInfoDict, key);
  CFRelease(key);
  if (value == nullptr) {
    return NS_ERROR_FAILURE;
  }

  CFIndex valueLength = CFStringGetLength(value);
  if (valueLength == 0) {
    return NS_ERROR_FAILURE;
  }

  const char* valueCString = CFStringGetCStringPtr(value,
                                                   kCFStringEncodingUTF8);
  if (valueCString) {
    aValue.Assign(valueCString);
    return NS_OK;
  }

  CFIndex maxLength =
    CFStringGetMaximumSizeForEncoding(valueLength, kCFStringEncodingUTF8) + 1;
  char* valueBuffer = static_cast<char*>(moz_xmalloc(maxLength));
  if (!valueBuffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!CFStringGetCString(value, valueBuffer, maxLength,
                          kCFStringEncodingUTF8)) {
    free(valueBuffer);
    return NS_ERROR_FAILURE;
  }

  aValue.Assign(valueBuffer);
  free(valueBuffer);
  return NS_OK;
}

/*
 * Helper function for reading a path string from the .app's Info.plist
 * and returning a directory object for that path with symlinks resolved.
 */
static nsresult
GetDirFromBundlePlist(const nsAString& aKey, nsIFile **aDir)
{
  nsresult rv;

  nsAutoCString dirPath;
  rv = GetStringValueFromBundlePlist(aKey, dirPath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> dir;
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(dirPath),
                       false,
                       getter_AddRefs(dir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dir->Normalize();
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDirectory = false;
  rv = dir->IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isDirectory) {
    return NS_ERROR_FILE_NOT_DIRECTORY;
  }

  dir.swap(*aDir);
  return NS_OK;
}

nsresult
GetRepoDir(nsIFile **aRepoDir)
{
  MOZ_ASSERT(IsDevelopmentBuild());
  return GetDirFromBundlePlist(NS_LITERAL_STRING(MAC_DEV_REPO_KEY), aRepoDir);
}

nsresult
GetObjDir(nsIFile **aObjDir)
{
  MOZ_ASSERT(IsDevelopmentBuild());
  return GetDirFromBundlePlist(NS_LITERAL_STRING(MAC_DEV_OBJ_KEY), aObjDir);
}
#endif /* XP_MACOSX */

} // namespace mozilla
