/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

#include "ContentChild.h"

#include "CrashReporterChild.h"
#include "GeckoProfiler.h"
#include "TabChild.h"
#include "HandlerServiceChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessHangMonitorIPC.h"
#include "mozilla/Unused.h"
#include "mozilla/devtools/HeapSnapshotTempFileHelperChild.h"
#include "mozilla/docshell/OfflineCacheUpdateChild.h"
#include "mozilla/dom/ContentBridgeChild.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/VideoDecoderManagerChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ExternalHelperAppChild.h"
#include "mozilla/dom/FlyWebPublishedServerIPC.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/dom/PCrashReporterChild.h"
#include "mozilla/dom/ProcessGlobal.h"
#include "mozilla/dom/PushNotifier.h"
#include "mozilla/dom/StorageIPC.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/URLClassifierChild.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/psm/PSMContentListener.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/PSendStreamChild.h"
#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ContentProcessController.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/CaptivePortalService.h"
#include "mozilla/plugins/PluginInstanceParent.h"
#include "mozilla/plugins/PluginModuleParent.h"
#include "mozilla/widget/WidgetMessageUtils.h"
#include "nsBaseDragService.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/WebBrowserPersistDocumentChild.h"
#include "imgLoader.h"
#include "GMPServiceChild.h"

#if defined(MOZ_CONTENT_SANDBOX)
#if defined(XP_WIN)
#define TARGET_SANDBOX_EXPORTS
#include "mozilla/sandboxTarget.h"
#elif defined(XP_LINUX)
#include "mozilla/Sandbox.h"
#include "mozilla/SandboxInfo.h"

// Remove this include with Bug 1104619
#include "CubebUtils.h"
#elif defined(XP_MACOSX)
#include "mozilla/Sandbox.h"
#endif
#endif

#include "mozilla/Unused.h"

#include "mozInlineSpellChecker.h"
#include "nsDocShell.h"
#include "nsIConsoleListener.h"
#include "nsICycleCollectorListener.h"
#include "nsIIdlePeriod.h"
#include "nsIDragService.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMemoryReporter.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsScreenManagerProxy.h"
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
#ifdef NS_PRINTING
#include "nsPrintingProxy.h"
#endif

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

#include "nsIScriptSecurityManager.h"
#include "nsHostObjectProtocolHandler.h"

#ifdef MOZ_WEBRTC
#include "signaling/src/peerconnection/WebrtcGlobalChild.h"
#endif

#ifdef MOZ_PERMISSIONS
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
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#include "mozilla/widget/AudioSession.h"
#endif

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#include "mozilla/dom/File.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestChild.h"
#include "mozilla/dom/PPresentationChild.h"
#include "mozilla/dom/PresentationIPCService.h"
#include "mozilla/ipc/InputStreamUtils.h"

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/PSpeechSynthesisChild.h"
#endif

#include "ProcessUtils.h"
#include "URIUtils.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsDeviceStorage.h"
#include "DomainPolicy.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/time/DateCacheCleaner.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "mozilla/widget/PuppetBidiKeyboard.h"
#include "mozilla/RemoteSpellCheckEngineChild.h"
#include "GMPServiceChild.h"
#include "gfxPlatform.h"
#include "nscore.h" // for NS_FREE_PERMANENT_DATA
#include "VRManagerChild.h"

using namespace mozilla;
using namespace mozilla::docshell;
using namespace mozilla::dom::devicestorage;
using namespace mozilla::dom::ipc;
using namespace mozilla::dom::workers;
using namespace mozilla::media;
using namespace mozilla::embedding;
using namespace mozilla::gmp;
using namespace mozilla::hal_sandbox;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::net;
using namespace mozilla::jsipc;
using namespace mozilla::psm;
using namespace mozilla::widget;
#if defined(MOZ_WIDGET_GONK)
using namespace mozilla::system;
#endif
using namespace mozilla::widget;

namespace mozilla {
namespace dom {

class MemoryReportRequestChild : public PMemoryReportRequestChild,
                                 public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  MemoryReportRequestChild(bool aAnonymize,
                           const MaybeFileDesc& aDMDFile);
  NS_IMETHOD Run() override;

private:
  ~MemoryReportRequestChild() override;

  bool     mAnonymize;
  FileDescriptor mDMDFile;
};

NS_IMPL_ISUPPORTS(MemoryReportRequestChild, nsIRunnable)

MemoryReportRequestChild::MemoryReportRequestChild(
  bool aAnonymize, const MaybeFileDesc& aDMDFile)
: mAnonymize(aAnonymize)
{
  if (aDMDFile.type() == MaybeFileDesc::TFileDescriptor) {
    mDMDFile = aDMDFile.get_FileDescriptor();
  }
}

MemoryReportRequestChild::~MemoryReportRequestChild()
{
}

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
    nsXPIDLCString category;
    uint32_t lineNum, colNum, flags;

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

class BackgroundChildPrimer final :
  public nsIIPCBackgroundChildCreateCallback
{
public:
  BackgroundChildPrimer()
  { }

  NS_DECL_ISUPPORTS

private:
  ~BackgroundChildPrimer() = default;

  void
  ActorCreated(PBackgroundChild* aActor) override
  {
    MOZ_ASSERT(aActor, "Failed to create a PBackgroundChild actor!");
  }

  void
  ActorFailed() override
  {
    MOZ_CRASH("Failed to create a PBackgroundChild actor!");
  }
};

NS_IMPL_ISUPPORTS(BackgroundChildPrimer, nsIIPCBackgroundChildCreateCallback)

ContentChild* ContentChild::sSingleton;

ContentChild::ContentChild()
 : mID(uint64_t(-1))
#if defined(XP_WIN) && defined(ACCESSIBILITY)
 , mMsaaID(0)
#endif
 , mCanOverrideProcessName(true)
 , mIsAlive(true)
 , mShuttingDown(false)
{
  // This process is a content process, so it's clearly running in
  // multiprocess mode!
  nsDebugImpl::SetMultiprocessMode("Child");
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

bool
ContentChild::Init(MessageLoop* aIOLoop,
                   base::ProcessId aParentPid,
                   IPC::Channel* aChannel)
{
#ifdef MOZ_WIDGET_GTK
  // We need to pass a display down to gtk_init because it's not going to
  // use the one from the environment on its own when deciding which backend
  // to use, and when starting under XWayland, it may choose to start with
  // the wayland backend instead of the x11 backend.
  // The DISPLAY environment variable is normally set by the parent process.
  char* display_name = PR_GetEnv("DISPLAY");
  if (display_name) {
    int argc = 3;
    char option_name[] = "--display";
    char* argv[] = {
      // argv0 is unused because g_set_prgname() was called in
      // XRE_InitChildProcess().
      nullptr,
      option_name,
      display_name,
      nullptr
    };
    char** argvp = argv;
    gtk_init(&argc, &argvp);
  } else {
    gtk_init(nullptr, nullptr);
  }
#endif

#ifdef MOZ_X11
  // Do this after initializing GDK, or GDK will install its own handler.
  XRE_InstallX11ErrorHandler();
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

  SendGetProcessAttributes(&mID, &mIsForBrowser);

#ifdef NS_PRINTING
  // Force the creation of the nsPrintingProxy so that it's IPC counterpart,
  // PrintingParent, is always available for printing initiated from the parent.
  RefPtr<nsPrintingProxy> printingProxy = nsPrintingProxy::GetInstance();
#endif

  SetProcessName(NS_LITERAL_STRING("Web Content"), true);

  return true;
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

  if (aDontOverride) {
    mCanOverrideProcessName = false;
  }
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
                            bool* aWindowIsNew,
                            mozIDOMWindowProxy** aReturn)
{
  return ProvideWindowCommon(nullptr, aParent, false, aChromeFlags,
                             aCalledFromJS, aPositionSpecified,
                             aSizeSpecified, aURI, aName, aFeatures,
                             aForceNoOpener, aWindowIsNew, aReturn);
}

static nsresult
GetWindowParamsFromParent(mozIDOMWindowProxy* aParent,
                          nsACString& aBaseURIString, float* aFullZoom,
                          DocShellOriginAttributes& aOriginAttributes)
{
  *aFullZoom = 1.0f;
  auto* opener = nsPIDOMWindowOuter::From(aParent);
  if (!opener) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = opener->GetDoc();
  nsCOMPtr<nsIURI> baseURI = doc->GetDocBaseURI();
  if (!baseURI) {
    NS_ERROR("nsIDocument didn't return a base URI");
    return NS_ERROR_FAILURE;
  }

  baseURI->GetSpec(aBaseURIString);

  RefPtr<nsDocShell> openerDocShell =
    static_cast<nsDocShell*>(opener->GetDocShell());
  if (!openerDocShell) {
    return NS_OK;
  }

  aOriginAttributes = openerDocShell->GetOriginAttributes();

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
                                  bool* aWindowIsNew,
                                  mozIDOMWindowProxy** aReturn)
{
  *aReturn = nullptr;

  nsAutoPtr<IPCTabContext> ipcContext;
  TabId openerTabId = TabId(0);
  nsAutoCString features(aFeatures);

  nsresult rv;
  if (aTabOpener) {
    // Check to see if the target URI can be loaded in this process.
    // If not create and load it in an unrelated tab/window.
    nsCOMPtr<nsIWebBrowserChrome3> browserChrome3;
    rv = aTabOpener->GetWebBrowserChrome(getter_AddRefs(browserChrome3));
    if (NS_SUCCEEDED(rv) && browserChrome3) {
      bool shouldLoad;
      rv = browserChrome3->ShouldLoadURIInThisProcess(aURI, &shouldLoad);
      if (NS_SUCCEEDED(rv) && !shouldLoad) {
        nsAutoCString baseURIString;
        float fullZoom;
        DocShellOriginAttributes originAttributes;
        rv = GetWindowParamsFromParent(aParent, baseURIString, &fullZoom,
                                       originAttributes);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        URIParams uriToLoad;
        SerializeURI(aURI, uriToLoad);
        Unused << SendCreateWindowInDifferentProcess(aTabOpener, aChromeFlags,
                                                     aCalledFromJS,
                                                     aPositionSpecified,
                                                     aSizeSpecified,
                                                     uriToLoad, features,
                                                     baseURIString,
                                                     originAttributes, fullZoom);

        // We return NS_ERROR_ABORT, so that the caller knows that we've abandoned
        // the window open as far as it is concerned.
        return NS_ERROR_ABORT;
      }
    }

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
  TabId tabId;
  SendAllocateTabId(openerTabId,
                    *ipcContext,
                    GetID(),
                    &tabId);

  TabContext newTabContext = aTabOpener ? *aTabOpener : TabContext();
  RefPtr<TabChild> newChild = new TabChild(this, tabId,
                                           newTabContext, aChromeFlags);
  if (NS_FAILED(newChild->Init())) {
    return NS_ERROR_ABORT;
  }

  if (aTabOpener) {
    MOZ_ASSERT(ipcContext->type() == IPCTabContext::TPopupIPCTabContext);
    ipcContext->get_PopupIPCTabContext().opener() = aTabOpener;
  }

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
  nsCOMPtr<nsIEventTarget> target = tabGroup->EventTargetFor(TaskCategory::Other);
  SetEventTargetForActor(newChild, target);

  Unused << SendPBrowserConstructor(
    // We release this ref in DeallocPBrowserChild
    RefPtr<TabChild>(newChild).forget().take(),
    tabId, *ipcContext, aChromeFlags,
    GetID(), IsForBrowser());

  nsString name(aName);
  nsTArray<FrameScriptInfo> frameScripts;
  nsCString urlToLoad;

  PRenderFrameChild* renderFrame = newChild->SendPRenderFrameConstructor();
  TextureFactoryIdentifier textureFactoryIdentifier;
  uint64_t layersId = 0;

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

    newChild->SendBrowserFrameOpenWindow(aTabOpener, renderFrame, NS_ConvertUTF8toUTF16(url),
                                         name, NS_ConvertUTF8toUTF16(features),
                                         aWindowIsNew, &textureFactoryIdentifier,
                                         &layersId);
  } else {
    nsAutoCString baseURIString;
    float fullZoom;
    DocShellOriginAttributes originAttributes;
    rv = GetWindowParamsFromParent(aParent, baseURIString, &fullZoom,
                                   originAttributes);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!SendCreateWindow(aTabOpener, newChild, renderFrame,
                          aChromeFlags, aCalledFromJS, aPositionSpecified,
                          aSizeSpecified,
                          features,
                          baseURIString,
                          originAttributes,
                          fullZoom,
                          &rv,
                          aWindowIsNew,
                          &frameScripts,
                          &urlToLoad,
                          &textureFactoryIdentifier,
                          &layersId)) {
      PRenderFrameChild::Send__delete__(renderFrame);
      return NS_ERROR_NOT_AVAILABLE;
    }

    if (NS_FAILED(rv)) {
      PRenderFrameChild::Send__delete__(renderFrame);
      return rv;
    }
  }
  if (!*aWindowIsNew) {
    PRenderFrameChild::Send__delete__(renderFrame);
    return NS_ERROR_ABORT;
  }

  if (layersId == 0) { // if renderFrame is invalid.
    PRenderFrameChild::Send__delete__(renderFrame);
    renderFrame = nullptr;
  }

  ShowInfo showInfo(EmptyString(), false, false, true, false, 0, 0, 0);
  auto* opener = nsPIDOMWindowOuter::From(aParent);
  nsIDocShell* openerShell;
  if (opener && (openerShell = opener->GetDocShell())) {
    nsCOMPtr<nsILoadContext> context = do_QueryInterface(openerShell);
    showInfo = ShowInfo(EmptyString(), false,
                        context->UsePrivateBrowsing(), true, false,
                        aTabOpener->mDPI, aTabOpener->mRounding,
                        aTabOpener->mDefaultScale);
  }

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
  newChild->DoFakeShow(textureFactoryIdentifier, layersId, renderFrame,
                       showInfo);

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
  return NS_OK;
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
ContentChild::InitGraphicsDeviceData()
{
  // Initialize the graphics platform. This may contact the parent process
  // to read device preferences.
  gfxPlatform::GetPlatform();
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

  BlobChild::Startup(BlobChild::FriendKey());

  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!svc) {
    NS_WARNING("Couldn't acquire console service");
    return;
  }

  mConsoleListener = new ConsoleListener(this);
  if (NS_FAILED(svc->RegisterListener(mConsoleListener)))
    NS_WARNING("Couldn't register console listener for child process");

  bool isOffline, isLangRTL, haveBidiKeyboards;
  bool isConnected;
  int32_t captivePortalState;
  ClipboardCapabilities clipboardCaps;
  DomainPolicyClone domainPolicy;
  StructuredCloneData initialData;
  OptionalURIParams userContentSheetURL;

  SendGetXPCOMProcessAttributes(&isOffline, &isConnected, &captivePortalState,
                                &isLangRTL, &haveBidiKeyboards,
                                &mAvailableDictionaries,
                                &clipboardCaps, &domainPolicy, &initialData,
                                &mFontFamilies, &userContentSheetURL,
                                &mLookAndFeelCache);

  RecvSetOffline(isOffline);
  RecvSetConnectivity(isConnected);
  RecvSetCaptivePortalState(captivePortalState);
  RecvBidiKeyboardNotify(isLangRTL, haveBidiKeyboards);

  // Create the CPOW manager as soon as possible.
  SendPJavaScriptConstructor();

  if (domainPolicy.active()) {
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);
    ssm->ActivateDomainPolicyInternal(getter_AddRefs(mPolicy));
    if (!mPolicy) {
      MOZ_CRASH("Failed to activate domain policy.");
    }
    mPolicy->ApplyClone(&domainPolicy);
  }

  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1"));
  if (nsCOMPtr<nsIClipboardProxy> clipboardProxy = do_QueryInterface(clipboard)) {
    clipboardProxy->SetCapabilities(clipboardCaps);
  }

  {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(xpc::PrivilegedJunkScope()))) {
      MOZ_CRASH();
    }
    ErrorResult rv;
    JS::RootedValue data(jsapi.cx());
    initialData.Read(jsapi.cx(), &data, rv);
    if (NS_WARN_IF(rv.Failed())) {
      MOZ_CRASH();
    }
    ProcessGlobal* global = ProcessGlobal::Get();
    global->SetInitialProcessData(data);
  }

  // The stylesheet cache is not ready yet. Store this URL for future use.
  nsCOMPtr<nsIURI> ucsURL = DeserializeURI(userContentSheetURL);
  nsLayoutStylesheetCache::SetUserContentCSSURL(ucsURL);

  // This will register cross-process observer.
  mozilla::dom::time::InitializeDateCacheCleaner();
}

PMemoryReportRequestChild*
ContentChild::AllocPMemoryReportRequestChild(const uint32_t& aGeneration,
                                             const bool &aAnonymize,
                                             const bool &aMinimizeMemoryUsage,
                                             const MaybeFileDesc& aDMDFile)
{
  auto *actor =
    new MemoryReportRequestChild(aAnonymize, aDMDFile);
  actor->AddRef();
  return actor;
}

class HandleReportCallback final : public nsIHandleReportCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit HandleReportCallback(MemoryReportRequestChild* aActor,
                                const nsACString& aProcess)
  : mActor(aActor)
  , mProcess(aProcess)
  { }

  NS_IMETHOD Callback(const nsACString& aProcess, const nsACString &aPath,
                      int32_t aKind, int32_t aUnits, int64_t aAmount,
                      const nsACString& aDescription,
                      nsISupports* aUnused) override
  {
    MemoryReport memreport(mProcess, nsCString(aPath), aKind, aUnits,
                           aAmount, nsCString(aDescription));
    mActor->SendReport(memreport);
    return NS_OK;
  }
private:
  ~HandleReportCallback() = default;

  RefPtr<MemoryReportRequestChild> mActor;
  const nsCString mProcess;
};

NS_IMPL_ISUPPORTS(
  HandleReportCallback
, nsIHandleReportCallback
)

class FinishReportingCallback final : public nsIFinishReportingCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit FinishReportingCallback(MemoryReportRequestChild* aActor)
  : mActor(aActor)
  {
  }

  NS_IMETHOD Callback(nsISupports* aUnused) override
  {
    bool sent = PMemoryReportRequestChild::Send__delete__(mActor);
    return sent ? NS_OK : NS_ERROR_FAILURE;
  }

private:
  ~FinishReportingCallback() = default;

  RefPtr<MemoryReportRequestChild> mActor;
};

NS_IMPL_ISUPPORTS(
  FinishReportingCallback
, nsIFinishReportingCallback
)

mozilla::ipc::IPCResult
ContentChild::RecvPMemoryReportRequestConstructor(
  PMemoryReportRequestChild* aChild,
  const uint32_t& aGeneration,
  const bool& aAnonymize,
  const bool& aMinimizeMemoryUsage,
  const MaybeFileDesc& aDMDFile)
{
  MemoryReportRequestChild *actor =
    static_cast<MemoryReportRequestChild*>(aChild);
  DebugOnly<nsresult> rv;

  if (aMinimizeMemoryUsage) {
    nsCOMPtr<nsIMemoryReporterManager> mgr =
      do_GetService("@mozilla.org/memory-reporter-manager;1");
    rv = mgr->MinimizeMemoryUsage(actor);
    // mgr will eventually call actor->Run()
  } else {
    rv = actor->Run();
  }

  // Bug 1295622: don't kill the process just because this failed.
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "actor operation failed");
  return IPC_OK();
}

NS_IMETHODIMP MemoryReportRequestChild::Run()
{
  ContentChild *child = static_cast<ContentChild*>(Manager());
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");

  nsCString process;
  child->GetProcessName(process);
  child->AppendProcessId(process);

  // Run the reporters.  The callback will turn each measurement into a
  // MemoryReport.
  RefPtr<HandleReportCallback> handleReport =
    new HandleReportCallback(this, process);
  RefPtr<FinishReportingCallback> finishReporting =
    new FinishReportingCallback(this);

  nsresult rv =
    mgr->GetReportsForThisProcessExtended(handleReport, nullptr, mAnonymize,
                                          FileDescriptorToFILE(mDMDFile, "wb"),
                                          finishReporting, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "GetReportsForThisProcessExtended failed");
  return rv;
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

mozilla::plugins::PPluginModuleParent*
ContentChild::AllocPPluginModuleParent(mozilla::ipc::Transport* aTransport,
                                       base::ProcessId aOtherProcess)
{
  return plugins::PluginModuleContentParent::Initialize(aTransport, aOtherProcess);
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

PGMPServiceChild*
ContentChild::AllocPGMPServiceChild(mozilla::ipc::Transport* aTransport,
                                    base::ProcessId aOtherProcess)
{
  return GMPServiceChild::Create(aTransport, aOtherProcess);
}

mozilla::ipc::IPCResult
ContentChild::RecvGMPsChanged(nsTArray<GMPCapabilityData>&& capabilities)
{
  GeckoMediaPluginServiceChild::UpdateGMPCapabilities(Move(capabilities));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvInitRendering(Endpoint<PCompositorBridgeChild>&& aCompositor,
                                Endpoint<PImageBridgeChild>&& aImageBridge,
                                Endpoint<PVRManagerChild>&& aVRBridge,
                                Endpoint<PVideoDecoderManagerChild>&& aVideoManager)
{
  if (!CompositorBridgeChild::InitForContent(Move(aCompositor))) {
    return IPC_FAIL_NO_REASON(this);
  }
  if (!ImageBridgeChild::InitForContent(Move(aImageBridge))) {
    return IPC_FAIL_NO_REASON(this);
  }
  if (!gfx::VRManagerChild::InitForContent(Move(aVRBridge))) {
    return IPC_FAIL_NO_REASON(this);
  }
  VideoDecoderManagerChild::InitForContent(Move(aVideoManager));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvReinitRendering(Endpoint<PCompositorBridgeChild>&& aCompositor,
                                  Endpoint<PImageBridgeChild>&& aImageBridge,
                                  Endpoint<PVRManagerChild>&& aVRBridge,
                                  Endpoint<PVideoDecoderManagerChild>&& aVideoManager)
{
  nsTArray<RefPtr<TabChild>> tabs = TabChild::GetAll();

  // Zap all the old layer managers we have lying around.
  for (const auto& tabChild : tabs) {
    if (tabChild->LayersId()) {
      tabChild->InvalidateLayers();
    }
  }

  // Re-establish singleton bridges to the compositor.
  if (!CompositorBridgeChild::ReinitForContent(Move(aCompositor))) {
    return IPC_FAIL_NO_REASON(this);
  }
  if (!ImageBridgeChild::ReinitForContent(Move(aImageBridge))) {
    return IPC_FAIL_NO_REASON(this);
  }
  if (!gfx::VRManagerChild::ReinitForContent(Move(aVRBridge))) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Establish new PLayerTransactions.
  for (const auto& tabChild : tabs) {
    if (tabChild->LayersId()) {
      tabChild->ReinitRendering();
    }
  }

  VideoDecoderManagerChild::InitForContent(Move(aVideoManager));
  return IPC_OK();
}

PBackgroundChild*
ContentChild::AllocPBackgroundChild(Transport* aTransport,
                                    ProcessId aOtherProcess)
{
  return BackgroundChild::Alloc(aTransport, aOtherProcess);
}

PProcessHangMonitorChild*
ContentChild::AllocPProcessHangMonitorChild(Transport* aTransport,
                                            ProcessId aOtherProcess)
{
  return CreateHangMonitorChild(aTransport, aOtherProcess);
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
  rv = dirSvc->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                   NS_GET_IID(nsIFile), getter_AddRefs(appDir));
  if (NS_FAILED(rv)) {
    return false;
  }
  bool exists;
  rv = appDir->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    return false;
  }

  bool isLink;
  app->IsSymlink(&isLink);
  if (isLink) {
    app->GetNativeTarget(aAppPath);
  } else {
    app->GetNativePath(aAppPath);
  }
  appBinary->IsSymlink(&isLink);
  if (isLink) {
    appBinary->GetNativeTarget(aAppBinaryPath);
  } else {
    appBinary->GetNativePath(aAppBinaryPath);
  }
  appDir->IsSymlink(&isLink);
  if (isLink) {
    appDir->GetNativeTarget(aAppDir);
  } else {
    appDir->GetNativePath(aAppDir);
  }

  return true;
}

static bool
StartMacOSContentSandbox()
{
  int sandboxLevel = Preferences::GetInt("security.sandbox.content.level");
  if (sandboxLevel < 1) {
    return false;
  }

  nsAutoCString appPath, appBinaryPath, appDir;
  if (!GetAppPaths(appPath, appBinaryPath, appDir)) {
    MOZ_CRASH("Error resolving child process path");
  }

  // During sandboxed content process startup, before reaching
  // this point, NS_OS_TEMP_DIR is modified to refer to a sandbox-
  // writable temporary directory
  nsCOMPtr<nsIFile> tempDir;
  nsresult rv = nsDirectoryService::gService->Get(NS_OS_TEMP_DIR,
      NS_GET_IID(nsIFile), getter_AddRefs(tempDir));
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Failed to get NS_OS_TEMP_DIR");
  }

  nsAutoCString tempDirPath;
  rv = tempDir->GetNativePath(tempDirPath);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Failed to get NS_OS_TEMP_DIR path");
  }

  nsCOMPtr<nsIFile> profileDir;
  ContentChild::GetSingleton()->GetProfileDir(getter_AddRefs(profileDir));
  nsCString profileDirPath;
  if (profileDir) {
    rv = profileDir->GetNativePath(profileDirPath);
    if (NS_FAILED(rv) || profileDirPath.IsEmpty()) {
      MOZ_CRASH("Failed to get profile path");
    }
  }

  MacSandboxInfo info;
  info.type = MacSandboxType_Content;
  info.level = info.level = sandboxLevel;
  info.appPath.assign(appPath.get());
  info.appBinaryPath.assign(appBinaryPath.get());
  info.appDir.assign(appDir.get());
  info.appTempDir.assign(tempDirPath.get());

  if (profileDir) {
    info.hasSandboxedProfile = true;
    info.profileDir.assign(profileDirPath.get());
  } else {
    info.hasSandboxedProfile = false;
  }

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
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 19
  // For B2G >= KitKat, sandboxing is mandatory; this has already
  // been enforced by ContentParent::StartUp().
  MOZ_ASSERT(SandboxInfo::Get().CanSandboxContent());
#else
  // Otherwise, sandboxing is best-effort.
  if (!SandboxInfo::Get().CanSandboxContent()) {
       sandboxEnabled = false;
   } else {
       // This triggers the initialization of cubeb, which needs to happen
       // before seccomp is enabled (Bug 1259508). It also increases the startup
       // time of the content process, because cubeb is usually initialized
       // when it is actually needed. This call here is no longer required
       // once Bug 1104619 (remoting audio) is resolved.
       Unused << CubebUtils::GetCubebContext();
  }

#endif /* MOZ_WIDGET_GONK && ANDROID_VERSION >= 19 */
  if (sandboxEnabled) {
    int brokerFd = -1;
    if (aBroker.type() == MaybeFileDesc::TFileDescriptor) {
      auto fd = aBroker.get_FileDescriptor().ClonePlatformHandle();
      brokerFd = fd.release();
      // brokerFd < 0 means to allow direct filesystem access, so
      // make absolutely sure that doesn't happen if the parent
      // didn't intend it.
      MOZ_RELEASE_ASSERT(brokerFd >= 0);
    }
    sandboxEnabled = SetContentProcessSandbox(brokerFd);
  }
#elif defined(XP_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
#elif defined(XP_MACOSX)
  sandboxEnabled = StartMacOSContentSandbox();
#endif

#if defined(MOZ_CRASHREPORTER)
  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("ContentSandboxEnabled"),
    sandboxEnabled? NS_LITERAL_CSTRING("1") : NS_LITERAL_CSTRING("0"));
#if defined(XP_LINUX) && !defined(OS_ANDROID)
  nsAutoCString flagsString;
  flagsString.AppendInt(SandboxInfo::Get().AsInteger());

  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("ContentSandboxCapabilities"), flagsString);
#endif /* XP_LINUX && !OS_ANDROID */
#endif /* MOZ_CRASHREPORTER */
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
                                 const IPCTabContext& aContext,
                                 const uint32_t& aChromeFlags,
                                 const ContentParentId& aCpID,
                                 const bool& aIsForBrowser)
{
  return nsIContentChild::AllocPBrowserChild(aTabId,
                                             aContext,
                                             aChromeFlags,
                                             aCpID,
                                             aIsForBrowser);
}

bool
ContentChild::SendPBrowserConstructor(PBrowserChild* aActor,
                                      const TabId& aTabId,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const ContentParentId& aCpID,
                                      const bool& aIsForBrowser)
{
  return PContentChild::SendPBrowserConstructor(aActor,
                                                aTabId,
                                                aContext,
                                                aChromeFlags,
                                                aCpID,
                                                aIsForBrowser);
}

mozilla::ipc::IPCResult
ContentChild::RecvPBrowserConstructor(PBrowserChild* aActor,
                                      const TabId& aTabId,
                                      const IPCTabContext& aContext,
                                      const uint32_t& aChromeFlags,
                                      const ContentParentId& aCpID,
                                      const bool& aIsForBrowser)
{
  return nsIContentChild::RecvPBrowserConstructor(aActor, aTabId, aContext,
                                                  aChromeFlags, aCpID, aIsForBrowser);
}

void
ContentChild::GetAvailableDictionaries(InfallibleTArray<nsString>& aDictionaries)
{
  aDictionaries = mAvailableDictionaries;
}

PFileDescriptorSetChild*
ContentChild::SendPFileDescriptorSetConstructor(const FileDescriptor& aFD)
{
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

PBlobChild*
ContentChild::AllocPBlobChild(const BlobConstructorParams& aParams)
{
  return nsIContentChild::AllocPBlobChild(aParams);
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

PFlyWebPublishedServerChild*
ContentChild::AllocPFlyWebPublishedServerChild(const nsString& name,
                                               const FlyWebPublishOptions& params)
{
  MOZ_CRASH("We should never be manually allocating PFlyWebPublishedServerChild actors");
  return nullptr;
}

bool
ContentChild::DeallocPFlyWebPublishedServerChild(PFlyWebPublishedServerChild* aActor)
{
  RefPtr<FlyWebPublishedServerChild> actor =
    dont_AddRef(static_cast<FlyWebPublishedServerChild*>(aActor));
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

PSendStreamChild*
ContentChild::SendPSendStreamConstructor(PSendStreamChild* aActor)
{
  return PContentChild::SendPSendStreamConstructor(aActor);
}

PSendStreamChild*
ContentChild::AllocPSendStreamChild()
{
  return nsIContentChild::AllocPSendStreamChild();
}

bool
ContentChild::DeallocPSendStreamChild(PSendStreamChild* aActor)
{
  return nsIContentChild::DeallocPSendStreamChild(aActor);
}

PScreenManagerChild*
ContentChild::AllocPScreenManagerChild(uint32_t* aNumberOfScreens,
                                       float* aSystemDefaultScale,
                                       bool* aSuccess)
{
  // The ContentParent should never attempt to allocate the
  // nsScreenManagerProxy. Instead, the nsScreenManagerProxy
  // service is requested and instantiated via XPCOM, and the
  // constructor of nsScreenManagerProxy sets up the IPC connection.
  MOZ_CRASH("Should never get here!");
  return nullptr;
}

bool
ContentChild::DeallocPScreenManagerChild(PScreenManagerChild* aService)
{
  // nsScreenManagerProxy is AddRef'd in its constructor.
  nsScreenManagerProxy *child = static_cast<nsScreenManagerProxy*>(aService);
  child->Release();
  return true;
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

PStorageChild*
ContentChild::AllocPStorageChild()
{
  MOZ_CRASH("We should never be manually allocating PStorageChild actors");
  return nullptr;
}

bool
ContentChild::DeallocPStorageChild(PStorageChild* aActor)
{
  StorageDBChild* child = static_cast<StorageDBChild*>(aActor);
  child->ReleaseIPDLReference();
  return true;
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

  nsHostObjectProtocolHandler::RemoveDataEntries();

  mAlertObservers.Clear();

  mIdleObservers.Clear();

  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (svc) {
    svc->UnregisterListener(mConsoleListener);
    mConsoleListener->mChild = nullptr;
  }
  mIsAlive = false;

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

#if defined(MOZ_CRASHREPORTER) && !defined(MOZ_B2G)
  if (PCrashReporterChild* c = LoneManagedOrNullAsserts(ManagedPCrashReporterChild())) {
    CrashReporterChild* crashReporter =
      static_cast<CrashReporterChild*>(c);
    nsDependentCString reason(aReason);
    crashReporter->SendAnnotateCrashReport(
        NS_LITERAL_CSTRING("ipc_channel_error"),
        reason);
  }
#endif
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
ContentChild::RecvPreferenceUpdate(const PrefSetting& aPref)
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
  RefPtr<DataStorage> storage = DataStorage::GetIfExists(aFilename);
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
  RefPtr<DataStorage> storage = DataStorage::GetIfExists(aFilename);
  if (storage) {
    storage->Remove(aKey, aType);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvDataStorageClear(const nsString& aFilename)
{
  RefPtr<DataStorage> storage = DataStorage::GetIfExists(aFilename);
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

mozilla::ipc::IPCResult
ContentChild::RecvNotifyVisited(const URIParams& aURI)
{
  nsCOMPtr<nsIURI> newURI = DeserializeURI(aURI);
  if (!newURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<IHistory> history = services::GetHistoryService();
  if (history) {
    history->NotifyVisited(newURI);
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
  RefPtr<nsFrameMessageManager> cpm =
    nsFrameMessageManager::GetChildProcessManager();
  if (cpm) {
    StructuredCloneData data;
    ipc::UnpackClonedMessageDataForChild(aData, data);
    CrossProcessCpowHolder cpows(this, aCpows);
    cpm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(cpm.get()),
                        nullptr, aMsg, false, &data, &cpows, aPrincipal,
                        nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvGeolocationUpdate(const GeoPosition& somewhere)
{
  nsCOMPtr<nsIGeolocationUpdate> gs =
    do_GetService("@mozilla.org/geolocation/service;1");
  if (!gs) {
    return IPC_OK();
  }
  nsCOMPtr<nsIDOMGeoPosition> position = somewhere;
  gs->Update(position);
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
ContentChild::RecvAddPermission(const IPC::Permission& permission)
{
#if MOZ_PERMISSIONS
  nsCOMPtr<nsIPermissionManager> permissionManagerIface =
    services::GetPermissionManager();
  nsPermissionManager* permissionManager =
    static_cast<nsPermissionManager*>(permissionManagerIface.get());
  MOZ_ASSERT(permissionManager,
         "We have no permissionManager in the Content process !");

  // note we do not need to force mUserContextId to the default here because
  // the permission manager does that internally.
  nsAutoCString originNoSuffix;
  PrincipalOriginAttributes attrs;
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
#endif

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
ContentChild::RecvActivateA11y(const uint32_t& aMsaaID)
{
#ifdef ACCESSIBILITY
#ifdef XP_WIN
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
ContentChild::RecvAppInfo(const nsCString& version, const nsCString& buildID,
                          const nsCString& name, const nsCString& UAName,
                          const nsCString& ID, const nsCString& vendor)
{
  mAppInfo.version.Assign(version);
  mAppInfo.buildID.Assign(buildID);
  mAppInfo.name.Assign(name);
  mAppInfo.UAName.Assign(UAName);
  mAppInfo.ID.Assign(ID);
  mAppInfo.vendor.Assign(vendor);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvRemoteType(const nsString& aRemoteType)
{
  MOZ_ASSERT(DOMStringIsNull(mRemoteType));

  mRemoteType.Assign(aRemoteType);
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
  MOZ_ASSERT(swm);
  swm->LoadRegistrations(aConfig.serviceWorkerRegistrations());
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvInitBlobURLs(nsTArray<BlobURLRegistrationData>&& aRegistrations)
{
  for (uint32_t i = 0; i < aRegistrations.Length(); ++i) {
    BlobURLRegistrationData& registration = aRegistrations[i];
    RefPtr<BlobImpl> blobImpl =
      static_cast<BlobChild*>(registration.blobChild())->GetBlobImpl();
    MOZ_ASSERT(blobImpl);

    nsHostObjectProtocolHandler::AddDataEntry(registration.url(),
                                              registration.principal(),
                                              blobImpl);
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
ContentChild::RecvFilePathUpdate(const nsString& aStorageType,
                                 const nsString& aStorageName,
                                 const nsString& aPath,
                                 const nsCString& aReason)
{
  if (nsDOMDeviceStorage::InstanceCount() == 0) {
    // No device storage instances in this process. Don't try and
    // and create a DeviceStorageFile since it will fail.

    return IPC_OK();
  }

  RefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(aStorageType, aStorageName, aPath);

  nsString reason;
  CopyASCIItoUTF16(aReason, reason);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(dsf, "file-watcher-update", reason.get());
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
ContentChild::RecvStartProfiler(const ProfilerInitParams& params)
{
  nsTArray<const char*> featureArray;
  for (size_t i = 0; i < params.features().Length(); ++i) {
    featureArray.AppendElement(params.features()[i].get());
  }

  nsTArray<const char*> threadNameFilterArray;
  for (size_t i = 0; i < params.threadFilters().Length(); ++i) {
    threadNameFilterArray.AppendElement(params.threadFilters()[i].get());
  }

  profiler_start(params.entries(), params.interval(),
                 featureArray.Elements(), featureArray.Length(),
                 threadNameFilterArray.Elements(),
                 threadNameFilterArray.Length());

 return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvStopProfiler()
{
  profiler_stop();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvPauseProfiler(const bool& aPause)
{
  if (aPause) {
    profiler_pause();
  } else {
    profiler_resume();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvGatherProfile()
{
  nsCString profileCString;
  UniquePtr<char[]> profile = profiler_get_profile();
  if (profile) {
    profileCString = nsCString(profile.get(), strlen(profile.get()));
  } else {
    profileCString = EmptyCString();
  }

  Unused << SendProfile(profileCString);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvLoadPluginResult(const uint32_t& aPluginId,
                                   const bool& aResult)
{
  nsresult rv;
  bool finalResult = aResult && SendConnectPluginBridge(aPluginId, &rv) &&
                     NS_SUCCEEDED(rv);
  plugins::PluginModuleContentParent::OnLoadPluginResult(aPluginId,
                                                         finalResult);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvAssociatePluginId(const uint32_t& aPluginId,
                                    const base::ProcessId& aProcessId)
{
  plugins::PluginModuleContentParent::AssociatePluginId(aPluginId, aProcessId);
  return IPC_OK();
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
  } else if (!mPolicy) {
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
    mForceKillTimer = do_CreateInstance("@mozilla.org/timer;1");
    MOZ_ASSERT(mForceKillTimer);
    mForceKillTimer->InitWithFuncCallback(ContentChild::ForceKillTimerCallback,
      this,
      timeoutSecs * 1000,
      nsITimer::TYPE_ONE_SHOT);
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
  // If we receive the shutdown message from within a nested event loop, we want
  // to wait for that event loop to finish. Otherwise we could prematurely
  // terminate an "unload" or "pagehide" event handler (which might be doing a
  // sync XHR, for example).
#if defined(MOZ_CRASHREPORTER)
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IPCShutdownState"),
                                     NS_LITERAL_CSTRING("RecvShutdown"));
#endif
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  if (NS_SUCCEEDED(rv) && thread) {
    RefPtr<nsThread> mainThread(thread.forget().downcast<nsThread>());
    if (mainThread->RecursionDepth() > 1) {
      // We're in a nested event loop. Let's delay for an arbitrary period of
      // time (100ms) in the hopes that the event loop will have finished by
      // then.
      MessageLoop::current()->PostDelayedTask(
        NewRunnableMethod(this, &ContentChild::RecvShutdown), 100);
      return IPC_OK();
    }
  }

  mShuttingDown = true;

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

#ifdef MOZ_ENABLE_PROFILER_SPS
  if (profiler_is_active()) {
    // We're shutting down while we were profiling. Send the
    // profile up to the parent so that we don't lose this
    // information.
    Unused << RecvGatherProfile();
  }
#endif

  // Start a timer that will insure we quickly exit after a reasonable
  // period of time. Prevents shutdown hangs after our connection to the
  // parent closes.
  StartForceKillTimer();

#if defined(MOZ_CRASHREPORTER)
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IPCShutdownState"),
                                     NS_LITERAL_CSTRING("SendFinishShutdown"));
#endif
  // Ignore errors here. If this fails, the parent will kill us after a
  // timeout.
  Unused << SendFinishShutdown();
  return IPC_OK();
}

PBrowserOrId
ContentChild::GetBrowserOrId(TabChild* aTabChild)
{
  if (!aTabChild ||
    this == aTabChild->Manager()) {
    return PBrowserOrId(aTabChild);
  }
  else {
    return PBrowserOrId(aTabChild->GetTabId());
  }
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
  parentInstance->CallUpdateWindow();
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
    NS_RUNTIMEABORT("Not Reached!");
    return IPC_FAIL_NO_REASON(this);
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
          if (items[j].data().type() == IPCDataTransferData::TPBlobChild) {
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
          } else if (item.data().type() == IPCDataTransferData::TPBlobChild) {
            BlobChild* blob = static_cast<BlobChild*>(item.data().get_PBlobChild());
            RefPtr<BlobImpl> blobImpl = blob->GetBlobImpl();
            variant->SetAsISupports(blobImpl);
          } else {
            continue;
          }
          // We should hide this data from content if we have a file, and we aren't a file.
          bool hidden = hasFiles && item.data().type() != IPCDataTransferData::TPBlobChild;
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
                                 const LayoutDeviceIntPoint& aDragEndPoint)
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
    dragService->EndDragSession(aDoneDrag);
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
ContentChild::RecvBlobURLRegistration(const nsCString& aURI, PBlobChild* aBlobChild,
                                      const IPC::Principal& aPrincipal)
{
  RefPtr<BlobImpl> blobImpl = static_cast<BlobChild*>(aBlobChild)->GetBlobImpl();
  MOZ_ASSERT(blobImpl);

  nsHostObjectProtocolHandler::AddDataEntry(aURI, aPrincipal, blobImpl);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvBlobURLUnregistration(const nsCString& aURI)
{
  nsHostObjectProtocolHandler::RemoveDataEntry(aURI);
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

    const nsTArray<PBlobChild*>& blobs =
      aResult.get_GetFilesResponseSuccess().blobsChild();

    bool succeeded = true;
    for (uint32_t i = 0; succeeded && i < blobs.Length(); ++i) {
      RefPtr<BlobImpl> impl = static_cast<BlobChild*>(blobs[i])->GetBlobImpl();
      succeeded = child->AppendBlobImpl(impl);
    }

    child->Finished(succeeded ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
  }

  mGetFilesPendingRequests.Remove(aUUID);
  return IPC_OK();
}

/* static */ void
ContentChild::FatalErrorIfNotUsingGPUProcess(const char* const aProtocolName,
                                             const char* const aErrorMsg,
                                             base::ProcessId aOtherPid)
{
  // If we're communicating with the same process or the UI process then we
  // want to crash normally. Otherwise we want to just warn as the other end
  // must be the GPU process and it crashing shouldn't be fatal for us.
  if (aOtherPid == base::GetCurrentProcId() ||
      (GetSingleton() && GetSingleton()->OtherPid() == aOtherPid)) {
    mozilla::ipc::FatalError(aProtocolName, aErrorMsg, false);
  } else {
    nsAutoCString formattedMessage("IPDL error [");
    formattedMessage.AppendASCII(aProtocolName);
    formattedMessage.AppendLiteral(R"(]: ")");
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

// The IPC code will call this method asking us to assign an event target to new
// actors created by the ContentParent.
already_AddRefed<nsIEventTarget>
ContentChild::GetConstructedEventTarget(const Message& aMsg)
{
  // Currently we only set targets for PBrowser.
  if (aMsg.type() != PContent::Msg_PBrowserConstructor__ID) {
    return nullptr;
  }

  // If the request for a new TabChild is coming from the parent process, then
  // there is no opener. Therefore, we create a fresh TabGroup.
  RefPtr<TabGroup> tabGroup = new TabGroup();
  nsCOMPtr<nsIEventTarget> target = tabGroup->EventTargetFor(TaskCategory::Other);
  return target.forget();
}

} // namespace dom
} // namespace mozilla
