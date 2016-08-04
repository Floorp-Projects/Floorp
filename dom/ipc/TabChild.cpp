/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabChild.h"

#include "gfxPrefs.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/DocAccessibleChild.h"
#endif
#include "Layers.h"
#include "ContentChild.h"
#include "TabParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/BrowserElementParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/indexedDB/PIndexedDBPermissionRequestChild.h"
#include "mozilla/plugins/PluginWidgetChild.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/ipc/DocumentRendererChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Move.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/unused.h"
#include "mozIApplication.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsEmbedCID.h"
#include "nsGlobalWindow.h"
#include <algorithm>
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "nsFilePickerProxy.h"
#include "mozilla/dom/Element.h"
#include "nsGlobalWindow.h"
#include "nsIBaseWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsICachedFileDescriptorListener.h"
#include "nsIDocumentInlines.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsFocusManager.h"
#include "EventStateManager.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebProgress.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsLayoutUtils.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "nsWindowWatcher.h"
#include "PermissionMessageUtils.h"
#include "PuppetWidget.h"
#include "StructuredCloneData.h"
#include "nsViewportInfo.h"
#include "nsILoadContext.h"
#include "ipc/nsGUIEventIPC.h"
#include "mozilla/gfx/Matrix.h"
#include "UnitTransforms.h"
#include "ClientLayerManager.h"
#include "LayersLogging.h"
#include "nsIOService.h"
#include "nsDOMClassInfoID.h"
#include "nsColorPickerProxy.h"
#include "nsDatePickerProxy.h"
#include "nsContentPermissionHelper.h"
#include "nsPresShell.h"
#include "nsIAppsService.h"
#include "nsNetUtil.h"
#include "nsIPermissionManager.h"
#include "nsIURILoader.h"
#include "nsIScriptError.h"
#include "mozilla/EventForwards.h"
#include "nsDeviceContext.h"
#include "nsSandboxFlags.h"
#include "FrameLayerBuilder.h"

#ifdef NS_PRINTING
#include "nsIPrintSession.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

#define BROWSER_ELEMENT_CHILD_SCRIPT \
    NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js")

#define TABC_LOG(...)
// #define TABC_LOG(...) printf_stderr("TABC: " __VA_ARGS__)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::dom::workers;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;
using namespace mozilla::widget;
using namespace mozilla::jsipc;
using mozilla::layers::GeckoContentController;

NS_IMPL_ISUPPORTS(ContentListener, nsIDOMEventListener)

static const CSSSize kDefaultViewportSize(980, 480);

static const char BEFORE_FIRST_PAINT[] = "before-first-paint";

typedef nsDataHashtable<nsUint64HashKey, TabChild*> TabChildMap;
static TabChildMap* sTabChildren;

TabChildBase::TabChildBase()
  : mTabChildGlobal(nullptr)
{
  mozilla::HoldJSObjects(this);
}

TabChildBase::~TabChildBase()
{
  mAnonymousGlobalScopes.Clear();
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChildBase)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChildGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnonymousGlobalScopes)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChildGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TabChildBase)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TabChildBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TabChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TabChildBase)

already_AddRefed<nsIDocument>
TabChildBase::GetDocument() const
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  WebNavigation()->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  return doc.forget();
}

already_AddRefed<nsIPresShell>
TabChildBase::GetPresShell() const
{
  nsCOMPtr<nsIPresShell> result;
  if (nsCOMPtr<nsIDocument> doc = GetDocument()) {
    result = doc->GetShell();
  }
  return result.forget();
}

void
TabChildBase::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                            const nsAString& aJSONData)
{
    AutoSafeJSContext cx;
    JS::Rooted<JS::Value> json(cx, JS::NullValue());
    StructuredCloneData data;
    if (JS_ParseJSON(cx,
                      static_cast<const char16_t*>(aJSONData.BeginReading()),
                      aJSONData.Length(),
                      &json)) {
        ErrorResult rv;
        data.Write(cx, json, rv);
        if (NS_WARN_IF(rv.Failed())) {
            rv.SuppressException();
            return;
        }
    }

    nsCOMPtr<nsIXPConnectJSObjectHolder> kungFuDeathGrip(GetGlobal());
    // Let the BrowserElementScrolling helper (if it exists) for this
    // content manipulate the frame state.
    RefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal), nullptr,
                       aMessageName, false, &data, nullptr, nullptr, nullptr);
}

bool
TabChildBase::UpdateFrameHandler(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(aFrameMetrics.GetScrollId() != FrameMetrics::NULL_SCROLL_ID);

  if (aFrameMetrics.IsRootContent()) {
    if (nsCOMPtr<nsIPresShell> shell = GetPresShell()) {
      // Guard against stale updates (updates meant for a pres shell which
      // has since been torn down and destroyed).
      if (aFrameMetrics.GetPresShellId() == shell->GetPresShellId()) {
        ProcessUpdateFrame(aFrameMetrics);
        return true;
      }
    }
  } else {
    // aFrameMetrics.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    FrameMetrics newSubFrameMetrics(aFrameMetrics);
    APZCCallbackHelper::UpdateSubFrame(newSubFrameMetrics);
    return true;
  }
  return true;
}

void
TabChildBase::ProcessUpdateFrame(const FrameMetrics& aFrameMetrics)
{
    if (!mGlobal || !mTabChildGlobal) {
        return;
    }

    FrameMetrics newMetrics = aFrameMetrics;
    APZCCallbackHelper::UpdateRootFrame(newMetrics);
}

NS_IMETHODIMP
ContentListener::HandleEvent(nsIDOMEvent* aEvent)
{
  RemoteDOMEvent remoteEvent;
  remoteEvent.mEvent = do_QueryInterface(aEvent);
  NS_ENSURE_STATE(remoteEvent.mEvent);
  mTabChild->SendEvent(remoteEvent);
  return NS_OK;
}

class TabChild::CachedFileDescriptorInfo
{
    struct PathOnlyComparatorHelper
    {
        bool Equals(const nsAutoPtr<CachedFileDescriptorInfo>& a,
                    const CachedFileDescriptorInfo& b) const
        {
            return a->mPath == b.mPath;
        }
    };

    struct PathAndCallbackComparatorHelper
    {
        bool Equals(const nsAutoPtr<CachedFileDescriptorInfo>& a,
                    const CachedFileDescriptorInfo& b) const
        {
            return a->mPath == b.mPath &&
                   a->mCallback == b.mCallback;
        }
    };

public:
    nsString mPath;
    FileDescriptor mFileDescriptor;
    nsCOMPtr<nsICachedFileDescriptorListener> mCallback;
    bool mCanceled;

    explicit CachedFileDescriptorInfo(const nsAString& aPath)
      : mPath(aPath), mCanceled(false)
    { }

    CachedFileDescriptorInfo(const nsAString& aPath,
                             const FileDescriptor& aFileDescriptor)
      : mPath(aPath), mFileDescriptor(aFileDescriptor), mCanceled(false)
    { }

    CachedFileDescriptorInfo(const nsAString& aPath,
                             nsICachedFileDescriptorListener* aCallback)
      : mPath(aPath), mCallback(aCallback), mCanceled(false)
    { }

    PathOnlyComparatorHelper PathOnlyComparator() const
    {
        return PathOnlyComparatorHelper();
    }

    PathAndCallbackComparatorHelper PathAndCallbackComparator() const
    {
        return PathAndCallbackComparatorHelper();
    }

    void FireCallback() const
    {
        mCallback->OnCachedFileDescriptor(mPath, mFileDescriptor);
    }
};

class TabChild::CachedFileDescriptorCallbackRunnable : public Runnable
{
    typedef TabChild::CachedFileDescriptorInfo CachedFileDescriptorInfo;

    nsAutoPtr<CachedFileDescriptorInfo> mInfo;

public:
    explicit CachedFileDescriptorCallbackRunnable(CachedFileDescriptorInfo* aInfo)
      : mInfo(aInfo)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aInfo);
        MOZ_ASSERT(!aInfo->mPath.IsEmpty());
        MOZ_ASSERT(aInfo->mCallback);
    }

    void Dispatch()
    {
        MOZ_ASSERT(NS_IsMainThread());

        nsresult rv = NS_DispatchToCurrentThread(this);
        NS_ENSURE_SUCCESS_VOID(rv);
    }

private:
    NS_IMETHOD Run()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mInfo);

        mInfo->FireCallback();
        return NS_OK;
    }
};

class TabChild::DelayedDeleteRunnable final
  : public Runnable
{
    RefPtr<TabChild> mTabChild;

public:
    explicit DelayedDeleteRunnable(TabChild* aTabChild)
      : mTabChild(aTabChild)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aTabChild);
    }

private:
    ~DelayedDeleteRunnable()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(!mTabChild);
    }

    NS_IMETHOD
    Run()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mTabChild);

        // Check in case ActorDestroy was called after RecvDestroy message.
        if (mTabChild->IPCOpen()) {
            Unused << PBrowserChild::Send__delete__(mTabChild);
        }

        mTabChild = nullptr;
        return NS_OK;
    }
};

namespace {
StaticRefPtr<TabChild> sPreallocatedTab;

std::map<TabId, RefPtr<TabChild>>&
NestedTabChildMap()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<TabId, RefPtr<TabChild>> sNestedTabChildMap;
  return sNestedTabChildMap;
}
} // namespace

already_AddRefed<TabChild>
TabChild::FindTabChild(const TabId& aTabId)
{
  auto iter = NestedTabChildMap().find(aTabId);
  if (iter == NestedTabChildMap().end()) {
    return nullptr;
  }
  RefPtr<TabChild> tabChild = iter->second;
  return tabChild.forget();
}

static void
PreloadSlowThingsPostFork(void* aUnused)
{
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "preload-postfork", nullptr);

    MOZ_ASSERT(sPreallocatedTab);
    // Initialize initial reflow of the PresShell has to happen after fork
    // because about:blank content viewer is created in the above observer
    // notification.
    nsCOMPtr<nsIDocShell> docShell =
      do_GetInterface(sPreallocatedTab->WebNavigation());
    if (nsIPresShell* presShell = docShell->GetPresShell()) {
        // Initialize and do an initial reflow of the about:blank
        // PresShell to let it preload some things for us.
        presShell->Initialize(0, 0);
        nsIDocument* doc = presShell->GetDocument();
        doc->FlushPendingNotifications(Flush_Layout);
        // ... but after it's done, make sure it doesn't do any more
        // work.
        presShell->MakeZombie();
    }

}

static bool sPreloaded = false;

/*static*/ void
TabChild::PreloadSlowThings()
{
    if (sPreloaded) {
        // If we are alredy initialized in Nuwa, don't redo preloading.
        return;
    }
    sPreloaded = true;

    // Pass nullptr to aManager since at this point the TabChild is
    // not connected to any manager. Any attempt to use the TabChild
    // in IPC will crash.
    RefPtr<TabChild> tab(new TabChild(nullptr,
                                        TabId(0),
                                        TabContext(), /* chromeFlags */ 0));
    if (!NS_SUCCEEDED(tab->Init()) ||
        !tab->InitTabChildGlobal(DONT_LOAD_SCRIPTS)) {
        return;
    }

    // Just load and compile these scripts, but don't run them.
    tab->TryCacheLoadAndCompileScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
    // Load, compile, and run these scripts.
    tab->RecvLoadRemoteScript(
        NS_LITERAL_STRING("chrome://global/content/preload.js"),
        true);

    sPreallocatedTab = tab;
    ClearOnShutdown(&sPreallocatedTab);

    PreloadSlowThingsPostFork(nullptr);
}

/*static*/ already_AddRefed<TabChild>
TabChild::Create(nsIContentChild* aManager,
                 const TabId& aTabId,
                 const TabContext &aContext,
                 uint32_t aChromeFlags)
{
    if (sPreallocatedTab &&
        sPreallocatedTab->mChromeFlags == aChromeFlags &&
        aContext.IsMozBrowserOrApp()) {

        RefPtr<TabChild> child = sPreallocatedTab.get();
        sPreallocatedTab = nullptr;

        MOZ_ASSERT(!child->mTriedBrowserInit);

        child->mManager = aManager;
        child->SetTabId(aTabId);
        child->SetTabContext(aContext);
        child->NotifyTabContextUpdated();
        return child.forget();
    }

    RefPtr<TabChild> iframe = new TabChild(aManager, aTabId,
                                             aContext, aChromeFlags);
    return NS_SUCCEEDED(iframe->Init()) ? iframe.forget() : nullptr;
}

TabChild::TabChild(nsIContentChild* aManager,
                   const TabId& aTabId,
                   const TabContext& aContext,
                   uint32_t aChromeFlags)
  : TabContext(aContext)
  , mRemoteFrame(nullptr)
  , mManager(aManager)
  , mChromeFlags(aChromeFlags)
  , mActiveSuppressDisplayport(0)
  , mLayersId(0)
  , mAppPackageFileDescriptorRecved(false)
  , mDidFakeShow(false)
  , mNotified(false)
  , mTriedBrowserInit(false)
  , mOrientation(eScreenOrientation_PortraitPrimary)
  , mUpdateHitRegion(false)
  , mIgnoreKeyPressEvent(false)
  , mHasValidInnerSize(false)
  , mDestroyed(false)
  , mUniqueId(aTabId)
  , mDPI(0)
  , mDefaultScale(0)
  , mIsTransparent(false)
  , mIPCOpen(true)
  , mParentIsActive(false)
  , mDidSetRealShowInfo(false)
  , mDidLoadURLInit(false)
  , mAPZChild(nullptr)
{
  // In the general case having the TabParent tell us if APZ is enabled or not
  // doesn't really work because the TabParent itself may not have a reference
  // to the owning widget during initialization. Instead we assume that this
  // TabChild corresponds to a widget type that would have APZ enabled, and just
  // check the other conditions necessary for enabling APZ.
  mAsyncPanZoomEnabled = gfxPlatform::AsyncPanZoomEnabled();

  nsWeakPtr weakPtrThis(do_GetWeakReference(static_cast<nsITabChild*>(this)));  // for capture by the lambda
  mSetAllowedTouchBehaviorCallback = [weakPtrThis](uint64_t aInputBlockId,
                                                   const nsTArray<TouchBehaviorFlags>& aFlags)
  {
    if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(weakPtrThis)) {
      static_cast<TabChild*>(tabChild.get())->SetAllowedTouchBehavior(aInputBlockId, aFlags);
    }
  };

  // preloaded TabChild should not be added to child map
  if (mUniqueId) {
    MOZ_ASSERT(NestedTabChildMap().find(mUniqueId) == NestedTabChildMap().end());
    NestedTabChildMap()[mUniqueId] = this;
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  if (observerService) {
    const nsAttrValue::EnumTable* table =
      AudioChannelService::GetAudioChannelTable();

    nsAutoCString topic;
    for (uint32_t i = 0; table[i].tag; ++i) {
      topic.Assign("audiochannel-activity-");
      topic.Append(table[i].tag);

      observerService->AddObserver(this, topic.get(), false);
    }
  }

  for (uint32_t idx = 0; idx < NUMBER_OF_AUDIO_CHANNELS; idx++) {
    mAudioChannelsActive.AppendElement(false);
  }
}

NS_IMETHODIMP
TabChild::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const char16_t *aData)
{
  if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    if (AsyncPanZoomEnabled()) {
      nsCOMPtr<nsIDocument> subject(do_QueryInterface(aSubject));
      nsCOMPtr<nsIDocument> doc(GetDocument());

      if (SameCOMIdentity(subject, doc)) {
        nsCOMPtr<nsIPresShell> shell(doc->GetShell());
        if (shell) {
          shell->SetIsFirstPaint(true);
        }

        APZCCallbackHelper::InitializeRootDisplayport(shell);
      }
    }
  }

  const nsAttrValue::EnumTable* table =
    AudioChannelService::GetAudioChannelTable();

  nsAutoCString topic;
  int16_t audioChannel = -1;
  for (uint32_t i = 0; table[i].tag; ++i) {
    topic.Assign("audiochannel-activity-");
    topic.Append(table[i].tag);

    if (topic.Equals(aTopic)) {
      audioChannel = table[i].value;
      break;
    }
  }

  if (audioChannel != -1 && mIPCOpen) {
    // If the subject is not a wrapper, it is sent by the TabParent and we
    // should ignore it.
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    if (!wrapper) {
      return NS_OK;
    }

    // We must have a window in order to compare the windowID contained into the
    // wrapper.
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
    if (!window) {
      return NS_OK;
    }

    uint64_t windowID = 0;
    nsresult rv = wrapper->GetData(&windowID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // In theory a tabChild should contain just 1 top window, but let's double
    // check it comparing the windowID.
    if (window->WindowID() != windowID) {
      MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
              ("TabChild, Observe, different windowID, owner ID = %lld, "
               "ID from wrapper = %lld", window->WindowID(), windowID));
      return NS_OK;
    }

    nsAutoString activeStr(aData);
    bool active = activeStr.EqualsLiteral("active");
    if (active != mAudioChannelsActive[audioChannel]) {
      mAudioChannelsActive[audioChannel] = active;
      Unused << SendAudioChannelActivityNotification(audioChannel, active);
    }
  }

  return NS_OK;
}

void
TabChild::ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                    uint64_t aInputBlockId,
                                    bool aPreventDefault) const
{
  if (mAPZChild) {
    mAPZChild->SendContentReceivedInputBlock(aGuid, aInputBlockId,
                                             aPreventDefault);
  }
}

void
TabChild::SetTargetAPZC(uint64_t aInputBlockId,
                        const nsTArray<ScrollableLayerGuid>& aTargets) const
{
  if (mAPZChild) {
    mAPZChild->SendSetTargetAPZC(aInputBlockId, aTargets);
  }
}

void
TabChild::SetAllowedTouchBehavior(uint64_t aInputBlockId,
                                  const nsTArray<TouchBehaviorFlags>& aTargets) const
{
  if (mAPZChild) {
    mAPZChild->SendSetAllowedTouchBehavior(aInputBlockId, aTargets);
  }
}

bool
TabChild::DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                  const ViewID& aViewId,
                                  const Maybe<ZoomConstraints>& aConstraints)
{
  if (sPreallocatedTab == this) {
    // If we're the preallocated tab, bail out because doing IPC will crash.
    // Once we get used for something we'll get another zoom constraints update
    // and all will be well.
    return true;
  }

  if (!mAPZChild) {
    return false;
  }

  return mAPZChild->SendUpdateZoomConstraints(aPresShellId, aViewId,
                                              aConstraints);
}

nsresult
TabChild::Init()
{
  nsCOMPtr<nsIWebBrowser> webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  if (!webBrowser) {
    NS_ERROR("Couldn't create a nsWebBrowser?");
    return NS_ERROR_FAILURE;
  }

  webBrowser->SetContainerWindow(this);
  mWebNav = do_QueryInterface(webBrowser);
  NS_ASSERTION(mWebNav, "nsWebBrowser doesn't implement nsIWebNavigation?");

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(WebNavigation()));
  docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
  if (!baseWindow) {
    NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(this);
  mPuppetWidget = static_cast<PuppetWidget*>(widget.get());
  if (!mPuppetWidget) {
    NS_ERROR("couldn't create fake widget");
    return NS_ERROR_FAILURE;
  }
  mPuppetWidget->Create(
    nullptr, 0,              // no parents
    LayoutDeviceIntRect(0, 0, 0, 0),
    nullptr                  // HandleWidgetEvent
  );

  baseWindow->InitWindow(0, mPuppetWidget, 0, 0, 0, 0);
  baseWindow->Create();

  // Set the tab context attributes then pass to docShell
  NotifyTabContextUpdated();

  // IPC uses a WebBrowser object for which DNS prefetching is turned off
  // by default. But here we really want it, so enable it explicitly
  nsCOMPtr<nsIWebBrowserSetup> webBrowserSetup =
    do_QueryInterface(baseWindow);
  if (webBrowserSetup) {
    webBrowserSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_DNS_PREFETCH,
                                 true);
  } else {
    NS_WARNING("baseWindow doesn't QI to nsIWebBrowserSetup, skipping "
               "DNS prefetching enable step.");
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  docShell->SetAffectPrivateSessionLifetime(
      mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME);
  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(WebNavigation());
  MOZ_ASSERT(loadContext);
  loadContext->SetPrivateBrowsing(OriginAttributesRef().mPrivateBrowsingId > 0);
  loadContext->SetRemoteTabs(
      mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW);

  // Few lines before, baseWindow->Create() will end up creating a new
  // window root in nsGlobalWindow::SetDocShell.
  // Then this chrome event handler, will be inherited to inner windows.
  // We want to also set it to the docshell so that inner windows
  // and any code that has access to the docshell
  // can all listen to the same chrome event handler.
  // XXX: ideally, we would set a chrome event handler earlier,
  // and all windows, even the root one, will use the docshell one.
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsCOMPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  docShell->SetChromeEventHandler(chromeHandler);

  if (window->GetCurrentInnerWindow()) {
    window->SetKeyboardIndicators(ShowAccelerators(), ShowFocusRings());
  } else {
    // Skip ShouldShowFocusRing check if no inner window is available
    window->SetInitialKeyboardIndicators(ShowAccelerators(), ShowFocusRings());
  }

  // Set prerender flag if necessary.
  if (mIsPrerendered) {
    docShell->SetIsPrerendered();
  }

  nsContentUtils::SetScrollbarsVisibility(window->GetDocShell(),
    !!(mChromeFlags & nsIWebBrowserChrome::CHROME_SCROLLBARS));

  nsWeakPtr weakPtrThis = do_GetWeakReference(static_cast<nsITabChild*>(this));  // for capture by the lambda
  ContentReceivedInputBlockCallback callback(
      [weakPtrThis](const ScrollableLayerGuid& aGuid,
                    uint64_t aInputBlockId,
                    bool aPreventDefault)
      {
        if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(weakPtrThis)) {
          static_cast<TabChild*>(tabChild.get())->ContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
        }
      });
  mAPZEventState = new APZEventState(mPuppetWidget, Move(callback));

  return NS_OK;
}

void
TabChild::NotifyTabContextUpdated()
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  if (!docShell) {
    return;
  }

  UpdateFrameType();
  nsDocShell::Cast(docShell)->SetOriginAttributes(OriginAttributesRef());

  // Set SANDBOXED_AUXILIARY_NAVIGATION flag if this is a receiver page.
  if (!PresentationURL().IsEmpty()) {
    docShell->SetSandboxFlags(SANDBOXED_AUXILIARY_NAVIGATION);
  }
}

void
TabChild::UpdateFrameType()
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  // TODO: Bug 1252794 - remove frameType from nsIDocShell.idl
  docShell->SetFrameType(IsMozBrowserElement() ? nsIDocShell::FRAME_TYPE_BROWSER :
                           HasOwnApp() ? nsIDocShell::FRAME_TYPE_APP :
                             nsIDocShell::FRAME_TYPE_REGULAR);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChild)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsITabChild)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
NS_INTERFACE_MAP_END_INHERITING(TabChildBase)

NS_IMPL_ADDREF_INHERITED(TabChild, TabChildBase);
NS_IMPL_RELEASE_INHERITED(TabChild, TabChildBase);

NS_IMETHODIMP
TabChild::SetStatus(uint32_t aStatusType, const char16_t* aStatus)
{
  return SetStatusWithContext(aStatusType,
      aStatus ? static_cast<const nsString &>(nsDependentString(aStatus))
              : EmptyString(),
      nullptr);
}

NS_IMETHODIMP
TabChild::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
  NS_WARNING("TabChild::GetWebBrowser not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
  NS_WARNING("TabChild::SetWebBrowser not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetChromeFlags(uint32_t* aChromeFlags)
{
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetChromeFlags(uint32_t aChromeFlags)
{
  NS_WARNING("trying to SetChromeFlags from content process?");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::DestroyBrowserWindow()
{
  NS_WARNING("TabChild::DestroyBrowserWindow not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::RemoteSizeShellTo(int32_t aWidth, int32_t aHeight,
                            int32_t aShellItemWidth, int32_t aShellItemHeight)
{
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(ourDocShell));
  int32_t width, height;
  docShellAsWin->GetSize(&width, &height);

  uint32_t flags = 0;
  if (width == aWidth) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX;
  }

  if (height == aHeight) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY;
  }

  bool sent = SendSizeShellTo(flags, aWidth, aHeight, aShellItemWidth, aShellItemHeight);

  return sent ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TabChild::SizeBrowserTo(int32_t aWidth, int32_t aHeight)
{
  NS_WARNING("TabChild::SizeBrowserTo not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::ShowAsModal()
{
  NS_WARNING("TabChild::ShowAsModal not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::IsWindowModal(bool* aRetVal)
{
  *aRetVal = false;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::ExitModalEventLoop(nsresult aStatus)
{
  NS_WARNING("TabChild::ExitModalEventLoop not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetStatusWithContext(uint32_t aStatusType,
                               const nsAString& aStatusText,
                               nsISupports* aStatusContext)
{
  // We can only send the status after the ipc machinery is set up,
  // mRemoteFrame is a good indicator.
  if (mRemoteFrame)
    SendSetStatus(aStatusType, nsString(aStatusText));
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetDimensions(uint32_t aFlags, int32_t aX, int32_t aY,
                        int32_t aCx, int32_t aCy)
{
  // The parent is in charge of the dimension changes. If JS code wants to
  // change the dimensions (moveTo, screenX, etc.) we send a message to the
  // parent about the new requested dimension, the parent does the resize/move
  // then send a message to the child to update itself. For APIs like screenX
  // this function is called with the current value for the non-changed values.
  // In a series of calls like window.screenX = 10; window.screenY = 10; for
  // the second call, since screenX is not yet updated we might accidentally
  // reset back screenX to it's old value. To avoid this if a parameter did not
  // change we want the parent to ignore its value.
  int32_t x, y, cx, cy;
  GetDimensions(aFlags, &x, &y, &cx, &cy);

  if (x == aX) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_X;
  }

  if (y == aY) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_Y;
  }

  if (cx == aCx) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX;
  }

  if (cy == aCy) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY;
  }

  Unused << SendSetDimensions(aFlags, aX, aY, aCx, aCy);

  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetDimensions(uint32_t aFlags, int32_t* aX,
                             int32_t* aY, int32_t* aCx, int32_t* aCy)
{
  ScreenIntRect rect = GetOuterRect();
  if (aX) {
    *aX = rect.x;
  }
  if (aY) {
    *aY = rect.y;
  }
  if (aCx) {
    *aCx = rect.width;
  }
  if (aCy) {
    *aCy = rect.height;
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetFocus()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetVisibility(bool* aVisibility)
{
  *aVisibility = true;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetVisibility(bool aVisibility)
{
  // should the platform support this? Bug 666365
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetTitle(char16_t** aTitle)
{
  NS_WARNING("TabChild::GetTitle not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetTitle(const char16_t* aTitle)
{
  // JavaScript sends the "DOMTitleChanged" event to the parent
  // via the message manager.
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetSiteWindow(void** aSiteWindow)
{
  NS_WARNING("TabChild::GetSiteWindow not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::Blur()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::FocusNextElement(bool aForDocumentNavigation)
{
  SendMoveFocus(true, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::FocusPrevElement(bool aForDocumentNavigation)
{
  SendMoveFocus(false, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetInterface(const nsIID & aIID, void **aSink)
{
    if (aIID.Equals(NS_GET_IID(nsIWebBrowserChrome3))) {
      NS_IF_ADDREF(((nsISupports *) (*aSink = mWebBrowserChrome)));
      return NS_OK;
    }

    // XXXbz should we restrict the set of interfaces we hand out here?
    // See bug 537429
    return QueryInterface(aIID, aSink);
}

NS_IMETHODIMP
TabChild::ProvideWindow(mozIDOMWindowProxy* aParent,
                        uint32_t aChromeFlags,
                        bool aCalledFromJS,
                        bool aPositionSpecified, bool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, bool* aWindowIsNew,
                        mozIDOMWindowProxy** aReturn)
{
    *aReturn = nullptr;

    // If aParent is inside an <iframe mozbrowser> or <iframe mozapp> and this
    // isn't a request to open a modal-type window, we're going to create a new
    // <iframe mozbrowser/mozapp> and return its window here.
    nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
    bool iframeMoz = (docshell && docshell->GetIsInMozBrowserOrApp() &&
                      !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                                        nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                                        nsIWebBrowserChrome::CHROME_OPENAS_CHROME)));

    if (!iframeMoz) {
      int32_t openLocation =
        nsWindowWatcher::GetWindowOpenLocation(nsPIDOMWindowOuter::From(aParent),
                                               aChromeFlags, aCalledFromJS,
                                               aPositionSpecified, aSizeSpecified);

      // If it turns out we're opening in the current browser, just hand over the
      // current browser's docshell.
      if (openLocation == nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
        nsCOMPtr<nsIWebBrowser> browser = do_GetInterface(WebNavigation());
        *aWindowIsNew = false;
        return browser->GetContentDOMWindow(aReturn);
      }
    }

    // Note that ProvideWindowCommon may return NS_ERROR_ABORT if the
    // open window call was canceled.  It's important that we pass this error
    // code back to our caller.
    ContentChild* cc = ContentChild::GetSingleton();
    return cc->ProvideWindowCommon(this,
                                   aParent,
                                   iframeMoz,
                                   aChromeFlags,
                                   aCalledFromJS,
                                   aPositionSpecified,
                                   aSizeSpecified,
                                   aURI,
                                   aName,
                                   aFeatures,
                                   aWindowIsNew,
                                   aReturn);
}

void
TabChild::DestroyWindow()
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (baseWindow)
        baseWindow->Destroy();

    // NB: the order of mPuppetWidget->Destroy() and mRemoteFrame->Destroy()
    // is important: we want to kill off remote layers before their
    // frames
    if (mPuppetWidget) {
        mPuppetWidget->Destroy();
    }

    if (mRemoteFrame) {
        mRemoteFrame->Destroy();
        mRemoteFrame = nullptr;
    }


    if (mLayersId != 0) {
      MOZ_ASSERT(sTabChildren);
      sTabChildren->Remove(mLayersId);
      if (!sTabChildren->Count()) {
        delete sTabChildren;
        sTabChildren = nullptr;
      }
      mLayersId = 0;
    }

    for (uint32_t index = 0, count = mCachedFileDescriptorInfos.Length();
         index < count;
         index++) {
        nsAutoPtr<CachedFileDescriptorInfo>& info =
            mCachedFileDescriptorInfos[index];

        MOZ_ASSERT(!info->mCallback);

        if (info->mFileDescriptor.IsValid()) {
            MOZ_ASSERT(!info->mCanceled);

            RefPtr<CloseFileRunnable> runnable =
                new CloseFileRunnable(info->mFileDescriptor);
            runnable->Dispatch();
        }
    }

    mCachedFileDescriptorInfos.Clear();
}

void
TabChild::ActorDestroy(ActorDestroyReason why)
{
  mIPCOpen = false;

  DestroyWindow();

  if (mTabChildGlobal) {
    // The messageManager relays messages via the TabChild which
    // no longer exists.
    static_cast<nsFrameMessageManager*>
      (mTabChildGlobal->mMessageManager.get())->Disconnect();
    mTabChildGlobal->mMessageManager = nullptr;
  }

  CompositorBridgeChild* compositorChild = static_cast<CompositorBridgeChild*>(CompositorBridgeChild::Get());
  compositorChild->CancelNotifyAfterRemotePaint(this);

  if (GetTabId() != 0) {
    NestedTabChildMap().erase(GetTabId());
  }
}

TabChild::~TabChild()
{
    DestroyWindow();

    nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(WebNavigation());
    if (webBrowser) {
      webBrowser->SetContainerWindow(nullptr);
    }
}

void
TabChild::SetProcessNameToAppName()
{
  nsCOMPtr<mozIApplication> app = GetOwnApp();
  if (!app) {
    return;
  }

  nsAutoString appName;
  nsresult rv = app->GetName(appName);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to retrieve app name");
    return;
  }

  ContentChild::GetSingleton()->SetProcessName(appName, true);
}

bool
TabChild::RecvLoadURL(const nsCString& aURI,
                      const ShowInfo& aInfo)
{
  if (!mDidLoadURLInit) {
    mDidLoadURLInit = true;
    if (!InitTabChildGlobal()) {
      return false;
    }

    ApplyShowInfo(aInfo);

    SetProcessNameToAppName();
  }

  nsresult rv =
    WebNavigation()->LoadURI(NS_ConvertUTF8toUTF16(aURI).get(),
                             nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
                             nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL,
                             nullptr, nullptr, nullptr);
  if (NS_FAILED(rv)) {
      NS_WARNING("WebNavigation()->LoadURI failed. Eating exception, what else can I do?");
  }

#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("URL"), aURI);
#endif

  return true;
}

bool
TabChild::RecvCacheFileDescriptor(const nsString& aPath,
                                  const FileDescriptor& aFileDescriptor)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!aPath.IsEmpty());
    MOZ_ASSERT(!mAppPackageFileDescriptorRecved);

    mAppPackageFileDescriptorRecved = true;

    // aFileDescriptor may be invalid here, but the callback will choose how to
    // handle it.

    // First see if we already have a request for this path.
    const CachedFileDescriptorInfo search(aPath);
    size_t index =
        mCachedFileDescriptorInfos.IndexOf(search, 0,
                                           search.PathOnlyComparator());
    if (index == mCachedFileDescriptorInfos.NoIndex) {
        // We haven't had any requests for this path yet. Assume that we will
        // in a little while and save the file descriptor here.
        mCachedFileDescriptorInfos.AppendElement(
            new CachedFileDescriptorInfo(aPath, aFileDescriptor));
        return true;
    }

    nsAutoPtr<CachedFileDescriptorInfo>& info =
        mCachedFileDescriptorInfos[index];

    MOZ_ASSERT(info);
    MOZ_ASSERT(info->mPath == aPath);
    MOZ_ASSERT(!info->mFileDescriptor.IsValid());
    MOZ_ASSERT(info->mCallback);

    // If this callback has been canceled then we can simply close the file
    // descriptor and forget about the callback.
    if (info->mCanceled) {
        // Only close if this is a valid file descriptor.
        if (aFileDescriptor.IsValid()) {
            RefPtr<CloseFileRunnable> runnable =
                new CloseFileRunnable(aFileDescriptor);
            runnable->Dispatch();
        }
    } else {
        // Not canceled so fire the callback.
        info->mFileDescriptor = aFileDescriptor;

        // We don't need a runnable here because we should already be at the top
        // of the event loop. Just fire immediately.
        info->FireCallback();
    }

    mCachedFileDescriptorInfos.RemoveElementAt(index);
    return true;
}

bool
TabChild::GetCachedFileDescriptor(const nsAString& aPath,
                                  nsICachedFileDescriptorListener* aCallback)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!aPath.IsEmpty());
    MOZ_ASSERT(aCallback);

    // First see if we've already received a cached file descriptor for this
    // path.
    const CachedFileDescriptorInfo search(aPath);
    size_t index =
        mCachedFileDescriptorInfos.IndexOf(search, 0,
                                           search.PathOnlyComparator());
    if (index == mCachedFileDescriptorInfos.NoIndex) {
        // We haven't received a file descriptor for this path yet. Assume that
        // we will in a little while and save the request here.
        if (!mAppPackageFileDescriptorRecved) {
          mCachedFileDescriptorInfos.AppendElement(
              new CachedFileDescriptorInfo(aPath, aCallback));
        }
        return false;
    }

    nsAutoPtr<CachedFileDescriptorInfo>& info =
        mCachedFileDescriptorInfos[index];

    MOZ_ASSERT(info);
    MOZ_ASSERT(info->mPath == aPath);

    // If we got a previous request for this file descriptor that was then
    // canceled, insert the new request ahead of the old in the queue so that
    // it will be serviced first.
    if (info->mCanceled) {
        // This insertion will change the array and invalidate |info|, so
        // be careful not to touch |info| after this.
        mCachedFileDescriptorInfos.InsertElementAt(index,
            new CachedFileDescriptorInfo(aPath, aCallback));
        return false;
    }

    MOZ_ASSERT(!info->mCallback);
    info->mCallback = aCallback;

    RefPtr<CachedFileDescriptorCallbackRunnable> runnable =
        new CachedFileDescriptorCallbackRunnable(info.forget());
    runnable->Dispatch();

    mCachedFileDescriptorInfos.RemoveElementAt(index);
    return true;
}

void
TabChild::CancelCachedFileDescriptorCallback(
                                     const nsAString& aPath,
                                     nsICachedFileDescriptorListener* aCallback)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!aPath.IsEmpty());
    MOZ_ASSERT(aCallback);

    if (mAppPackageFileDescriptorRecved) {
      // Already received cached file descriptor for the app package. Nothing to do here.
      return;
    }

    const CachedFileDescriptorInfo search(aPath, aCallback);
    size_t index =
        mCachedFileDescriptorInfos.IndexOf(search, 0,
                                           search.PathAndCallbackComparator());
    if (index == mCachedFileDescriptorInfos.NoIndex) {
        // Nothing to do here.
        return;
    }

    nsAutoPtr<CachedFileDescriptorInfo>& info =
        mCachedFileDescriptorInfos[index];

    MOZ_ASSERT(info);
    MOZ_ASSERT(info->mPath == aPath);
    MOZ_ASSERT(!info->mFileDescriptor.IsValid());
    MOZ_ASSERT(info->mCallback == aCallback);
    MOZ_ASSERT(!info->mCanceled);

    // No need to hold the callback any longer.
    info->mCallback = nullptr;

    // Set this flag so that we will close the file descriptor when it arrives.
    info->mCanceled = true;
}

void
TabChild::DoFakeShow(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                     const uint64_t& aLayersId,
                     PRenderFrameChild* aRenderFrame, const ShowInfo& aShowInfo)
{
  RecvShow(ScreenIntSize(0, 0), aShowInfo, aTextureFactoryIdentifier,
           aLayersId, aRenderFrame, mParentIsActive, nsSizeMode_Normal);
  mDidFakeShow = true;
}

void
TabChild::ApplyShowInfo(const ShowInfo& aInfo)
{
  if (mDidSetRealShowInfo) {
    return;
  }

  if (!aInfo.fakeShowInfo()) {
    // Once we've got one ShowInfo from parent, no need to update the values
    // anymore.
    mDidSetRealShowInfo = true;
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    nsCOMPtr<nsIDocShellTreeItem> item = do_GetInterface(docShell);
    if (IsMozBrowserOrApp()) {
      // B2G allows window.name to be set by changing the name attribute on the
      // <iframe mozbrowser> element. window.open calls cause this attribute to
      // be set to the correct value. A normal <xul:browser> element has no such
      // attribute. The data we get here comes from reading the attribute, so we
      // shouldn't trust it for <xul:browser> elements.
      item->SetName(aInfo.name());
    }
    docShell->SetFullscreenAllowed(aInfo.fullscreenAllowed());
    if (aInfo.isPrivate()) {
      nsCOMPtr<nsILoadContext> context = do_GetInterface(docShell);
      // No need to re-set private browsing mode.
      if (!context->UsePrivateBrowsing()) {
        bool nonBlank;
        docShell->GetHasLoadedNonBlankURI(&nonBlank);
        if (nonBlank) {
          nsContentUtils::ReportToConsoleNonLocalized(
            NS_LITERAL_STRING("We should not switch to Private Browsing after loading a document."),
            nsIScriptError::warningFlag,
            NS_LITERAL_CSTRING("mozprivatebrowsing"),
            nullptr);
        } else {
          DocShellOriginAttributes attrs(nsDocShell::Cast(docShell)->GetOriginAttributes());
          attrs.SyncAttributesWithPrivateBrowsing(true);
          nsDocShell::Cast(docShell)->SetOriginAttributes(attrs);
        }
      }
    }
  }
  mDPI = aInfo.dpi();
  mDefaultScale = aInfo.defaultScale();
  mIsTransparent = aInfo.isTransparent();
}

#ifdef MOZ_WIDGET_GONK
void
TabChild::MaybeRequestPreinitCamera()
{
    // Check if this tab is an app (not a browser frame) and will use the
    // `camera` permission,
    if (IsIsolatedMozBrowserElement()) {
      return;
    }

    nsCOMPtr<nsIAppsService> appsService = do_GetService("@mozilla.org/AppsService;1");
    if (NS_WARN_IF(!appsService)) {
      return;
    }

    nsCOMPtr<mozIApplication> app;
    nsresult rv = appsService->GetAppByLocalId(OwnAppId(), getter_AddRefs(app));
    if (NS_WARN_IF(NS_FAILED(rv)) || !app) {
      return;
    }

    nsCOMPtr<nsIPrincipal> principal;
    app->GetPrincipal(getter_AddRefs(principal));
    if (NS_WARN_IF(!principal)) {
      return;
    }

    uint16_t status = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
    principal->GetAppStatus(&status);
    bool isCertified = status == nsIPrincipal::APP_STATUS_CERTIFIED;
    if (!isCertified) {
      return;
    }

    nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
    if (NS_WARN_IF(!permMgr)) {
      return;
    }

    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    permMgr->TestPermissionFromPrincipal(principal, "camera", &permission);
    bool hasPermission = permission == nsIPermissionManager::ALLOW_ACTION;
    if (!hasPermission) {
      return;
    }

    nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return;
    }

    observerService->NotifyObservers(nullptr, "init-camera-hw", nullptr);
}
#endif

bool
TabChild::RecvShow(const ScreenIntSize& aSize,
                   const ShowInfo& aInfo,
                   const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                   const uint64_t& aLayersId,
                   PRenderFrameChild* aRenderFrame,
                   const bool& aParentIsActive,
                   const nsSizeMode& aSizeMode)
{
    MOZ_ASSERT((!mDidFakeShow && aRenderFrame) || (mDidFakeShow && !aRenderFrame));

    mPuppetWidget->SetSizeMode(aSizeMode);
    if (mDidFakeShow) {
        ApplyShowInfo(aInfo);
        RecvParentActivated(aParentIsActive);
        return true;
    }

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (!baseWindow) {
        NS_ERROR("WebNavigation() doesn't QI to nsIBaseWindow");
        return false;
    }

    if (!InitRenderingState(aTextureFactoryIdentifier, aLayersId, aRenderFrame)) {
        // We can fail to initialize our widget if the <browser
        // remote> has already been destroyed, and we couldn't hook
        // into the parent-process's layer system.  That's not a fatal
        // error.
        return true;
    }

    baseWindow->SetVisibility(true);

#ifdef MOZ_WIDGET_GONK
    MaybeRequestPreinitCamera();
#endif

    bool res = InitTabChildGlobal();
    ApplyShowInfo(aInfo);
    RecvParentActivated(aParentIsActive);

    return res;
}

bool
TabChild::RecvUpdateDimensions(const CSSRect& rect, const CSSSize& size,
                               const ScreenOrientationInternal& orientation,
                               const LayoutDeviceIntPoint& clientOffset,
                               const LayoutDeviceIntPoint& chromeDisp)
{
    if (!mRemoteFrame) {
        return true;
    }

    mUnscaledOuterRect = rect;
    mClientOffset = clientOffset;
    mChromeDisp = chromeDisp;

    mOrientation = orientation;
    SetUnscaledInnerSize(size);
    if (!mHasValidInnerSize && size.width != 0 && size.height != 0) {
      mHasValidInnerSize = true;
    }

    ScreenIntSize screenSize = GetInnerSize();
    ScreenIntRect screenRect = GetOuterRect();

    // Set the size on the document viewer before we update the widget and
    // trigger a reflow. Otherwise the MobileViewportManager reads the stale
    // size from the content viewer when it computes a new CSS viewport.
    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                                nsIBaseWindow::eRepaint);

    mPuppetWidget->Resize(screenRect.x + clientOffset.x + chromeDisp.x,
                          screenRect.y + clientOffset.y + chromeDisp.y,
                          screenSize.width, screenSize.height, true);

    return true;
}

bool
TabChild::RecvSizeModeChanged(const nsSizeMode& aSizeMode)
{
  mPuppetWidget->SetSizeMode(aSizeMode);
  if (!mPuppetWidget->IsVisible()) {
    return true;
  }
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    nsPresContext* presContext = presShell->GetPresContext();
    if (presContext) {
      presContext->SizeModeChanged(aSizeMode);
    }
  }
  return true;
}

bool
TabChild::UpdateFrame(const FrameMetrics& aFrameMetrics)
{
  return TabChildBase::UpdateFrameHandler(aFrameMetrics);
}

bool
TabChild::RecvSuppressDisplayport(const bool& aEnabled)
{
  if (aEnabled) {
    mActiveSuppressDisplayport++;
  } else {
    mActiveSuppressDisplayport--;
  }

  MOZ_ASSERT(mActiveSuppressDisplayport >= 0);
  APZCCallbackHelper::SuppressDisplayport(aEnabled, GetPresShell());
  return true;
}

void
TabChild::HandleDoubleTap(const CSSPoint& aPoint, const Modifiers& aModifiers,
                          const ScrollableLayerGuid& aGuid)
{
  TABC_LOG("Handling double tap at %s with %p %p\n",
    Stringify(aPoint).c_str(), mGlobal.get(), mTabChildGlobal.get());

  if (!mGlobal || !mTabChildGlobal) {
    return;
  }

  // Note: there is nothing to do with the modifiers here, as we are not
  // synthesizing any sort of mouse event.
  nsCOMPtr<nsIDocument> document = GetDocument();
  CSSRect zoomToRect = CalculateRectToZoomTo(document, aPoint);
  // The double-tap can be dispatched by any scroll frame (so |aGuid| could be
  // the guid of any scroll frame), but the zoom-to-rect operation must be
  // performed by the root content scroll frame, so query its identifiers
  // for the SendZoomToRect() call rather than using the ones from |aGuid|.
  uint32_t presShellId;
  ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
      document->GetDocumentElement(), &presShellId, &viewId) &&
      mAPZChild) {
    mAPZChild->SendZoomToRect(presShellId, viewId, zoomToRect,
                              DEFAULT_BEHAVIOR);
  }
}

void
TabChild::HandleTap(GeckoContentController::TapType aType,
                    const LayoutDevicePoint& aPoint, const Modifiers& aModifiers,
                    const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId,
                    bool aCallTakeFocusForClickFromTap)
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    return;
  }
  if (!presShell->GetPresContext()) {
    return;
  }
  CSSToLayoutDeviceScale scale(presShell->GetPresContext()->CSSToDevPixelScale());
  CSSPoint point = APZCCallbackHelper::ApplyCallbackTransform(aPoint / scale, aGuid);

  switch (aType) {
  case GeckoContentController::TapType::eSingleTap:
    if (aCallTakeFocusForClickFromTap && mRemoteFrame) {
      mRemoteFrame->SendTakeFocusForClickFromTap();
    }
    if (mGlobal && mTabChildGlobal) {
      mAPZEventState->ProcessSingleTap(point, scale, aModifiers, aGuid);
    }
    break;
  case GeckoContentController::TapType::eDoubleTap:
    HandleDoubleTap(point, aModifiers, aGuid);
    break;
  case GeckoContentController::TapType::eLongTap:
    if (mGlobal && mTabChildGlobal) {
      mAPZEventState->ProcessLongTap(presShell, point, scale, aModifiers, aGuid,
          aInputBlockId);
    }
    break;
  case GeckoContentController::TapType::eLongTapUp:
    if (mGlobal && mTabChildGlobal) {
      mAPZEventState->ProcessLongTapUp();
    }
    break;
  case GeckoContentController::TapType::eSentinel:
    // Should never happen, but we need to handle this case to make the compiler
    // happy.
    MOZ_ASSERT(false);
    break;
  }
}

bool
TabChild::NotifyAPZStateChange(const ViewID& aViewId,
                               const layers::GeckoContentController::APZStateChange& aChange,
                               const int& aArg)
{
  mAPZEventState->ProcessAPZStateChange(GetDocument(), aViewId, aChange, aArg);
  if (aChange == layers::GeckoContentController::APZStateChange::eTransformEnd) {
    // This is used by tests to determine when the APZ is done doing whatever
    // it's doing. XXX generify this as needed when writing additional tests.
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "APZ:TransformEnd", nullptr);
  }
  return true;
}

void
TabChild::StartScrollbarDrag(const layers::AsyncDragMetrics& aDragMetrics)
{
  if (mAPZChild) {
    mAPZChild->SendStartScrollbarDrag(aDragMetrics);
  }
}

void
TabChild::ZoomToRect(const uint32_t& aPresShellId,
                     const FrameMetrics::ViewID& aViewId,
                     const CSSRect& aRect,
                     const uint32_t& aFlags)
{
  if (mAPZChild) {
    mAPZChild->SendZoomToRect(aPresShellId, aViewId, aRect, aFlags);
  }
}

bool
TabChild::RecvActivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(WebNavigation());
  browser->Activate();
  return true;
}

bool TabChild::RecvDeactivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(WebNavigation());
  browser->Deactivate();
  return true;
}

bool TabChild::RecvParentActivated(const bool& aActivated)
{
  mParentIsActive = aActivated;

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, true);

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  fm->ParentActivated(window, aActivated);
  return true;
}

bool TabChild::RecvSetKeyboardIndicators(const UIStateChangeType& aShowAccelerators,
                                         const UIStateChangeType& aShowFocusRings)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, true);

  window->SetKeyboardIndicators(aShowAccelerators, aShowFocusRings);
  return true;
}

bool
TabChild::RecvStopIMEStateManagement()
{
  IMEStateManager::StopIMEStateManagement();
  return true;
}

bool
TabChild::RecvMenuKeyboardListenerInstalled(const bool& aInstalled)
{
  IMEStateManager::OnInstalledMenuKeyboardListener(aInstalled);
  return true;
}

bool
TabChild::RecvMouseEvent(const nsString& aType,
                         const float&    aX,
                         const float&    aY,
                         const int32_t&  aButton,
                         const int32_t&  aClickCount,
                         const int32_t&  aModifiers,
                         const bool&     aIgnoreRootScrollFrame)
{
  APZCCallbackHelper::DispatchMouseEvent(GetPresShell(), aType, CSSPoint(aX, aY),
      aButton, aClickCount, aModifiers, aIgnoreRootScrollFrame, nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN);
  return true;
}

bool
TabChild::RecvRealMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId)
{
  return RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
}

bool
TabChild::RecvSynthMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId)
{
  return RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
}

bool
TabChild::RecvRealMouseButtonEvent(const WidgetMouseEvent& aEvent,
                                   const ScrollableLayerGuid& aGuid,
                                   const uint64_t& aInputBlockId)
{
  // Mouse events like eMouseEnterIntoWidget, that are created in the parent
  // process EventStateManager code, have an input block id which they get from
  // the InputAPZContext in the parent process stack. However, they did not
  // actually go through the APZ code and so their mHandledByAPZ flag is false.
  // Since thos events didn't go through APZ, we don't need to send notifications
  // for them.
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<nsIDocument> document(GetDocument());
    APZCCallbackHelper::SendSetTargetAPZCNotification(
      mPuppetWidget, document, aEvent, aGuid, aInputBlockId);
  }

  nsEventStatus unused;
  InputAPZContext context(aGuid, aInputBlockId, unused);

  WidgetMouseEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
      mPuppetWidget->GetDefaultScale());
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessMouseEvent(aEvent, aGuid, aInputBlockId);
  }
  return true;
}

bool
TabChild::RecvMouseWheelEvent(const WidgetWheelEvent& aEvent,
                              const ScrollableLayerGuid& aGuid,
                              const uint64_t& aInputBlockId)
{
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<nsIDocument> document(GetDocument());
    APZCCallbackHelper::SendSetTargetAPZCNotification(
      mPuppetWidget, document, aEvent, aGuid, aInputBlockId);
  }

  WidgetWheelEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
      mPuppetWidget->GetDefaultScale());
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (localEvent.mCanTriggerSwipe) {
    SendRespondStartSwipeEvent(aInputBlockId, localEvent.TriggersSwipe());
  }

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessWheelEvent(localEvent, aGuid, aInputBlockId);
  }
  return true;
}

bool
TabChild::RecvMouseScrollTestEvent(const uint64_t& aLayersId,
                                   const FrameMetrics::ViewID& aScrollId, const nsString& aEvent)
{
  if (aLayersId != mLayersId) {
    RefPtr<TabParent> browser = TabParent::GetTabParentFromLayersId(aLayersId);
    if (!browser) {
      return false;
    }
    NS_DispatchToMainThread(NS_NewRunnableFunction(
      [aLayersId, browser, aScrollId, aEvent] () -> void {
        Unused << browser->SendMouseScrollTestEvent(aLayersId, aScrollId, aEvent);
      }));
    return true;
  }

  APZCCallbackHelper::NotifyMozMouseScrollEvent(aScrollId, aEvent);
  return true;
}

bool
TabChild::RecvRealTouchEvent(const WidgetTouchEvent& aEvent,
                             const ScrollableLayerGuid& aGuid,
                             const uint64_t& aInputBlockId,
                             const nsEventStatus& aApzResponse)
{
  TABC_LOG("Receiving touch event of type %d\n", aEvent.mMessage);

  WidgetTouchEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
      mPuppetWidget->GetDefaultScale());

  if (localEvent.mMessage == eTouchStart && AsyncPanZoomEnabled()) {
    nsCOMPtr<nsIDocument> document = GetDocument();
    if (gfxPrefs::TouchActionEnabled()) {
      APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(mPuppetWidget,
          document, localEvent, aInputBlockId, mSetAllowedTouchBehaviorCallback);
    }
    APZCCallbackHelper::SendSetTargetAPZCNotification(mPuppetWidget, document,
        localEvent, aGuid, aInputBlockId);
  }

  // Dispatch event to content (potentially a long-running operation)
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (!AsyncPanZoomEnabled()) {
    // We shouldn't have any e10s platforms that have touch events enabled
    // without APZ.
    MOZ_ASSERT(false);
    return true;
  }

  mAPZEventState->ProcessTouchEvent(localEvent, aGuid, aInputBlockId,
      aApzResponse, status);
  return true;
}

bool
TabChild::RecvRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId,
                                 const nsEventStatus& aApzResponse)
{
  return RecvRealTouchEvent(aEvent, aGuid, aInputBlockId, aApzResponse);
}

bool
TabChild::RecvRealDragEvent(const WidgetDragEvent& aEvent,
                            const uint32_t& aDragAction,
                            const uint32_t& aDropEffect)
{
  WidgetDragEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->SetDragAction(aDragAction);
    nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
    dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));
    if (initialDataTransfer) {
      initialDataTransfer->SetDropEffectInt(aDropEffect);
    }
  }

  if (aEvent.mMessage == eDrop) {
    bool canDrop = true;
    if (!dragSession || NS_FAILED(dragSession->GetCanDrop(&canDrop)) ||
        !canDrop) {
      localEvent.mMessage = eDragExit;
    }
  } else if (aEvent.mMessage == eDragOver) {
    nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (dragService) {
      // This will dispatch 'drag' event at the source if the
      // drag transaction started in this process.
      dragService->FireDragEventAtSource(eDrag);
    }
  }

  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  return true;
}

bool
TabChild::RecvPluginEvent(const WidgetPluginEvent& aEvent)
{
  WidgetPluginEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  if (status != nsEventStatus_eConsumeNoDefault) {
    // If not consumed, we should call default action
    SendDefaultProcOfPluginEvent(aEvent);
  }
  return true;
}

void
TabChild::RequestNativeKeyBindings(AutoCacheNativeKeyCommands* aAutoCache,
                                   WidgetKeyboardEvent* aEvent)
{
  MaybeNativeKeyBinding maybeBindings;
  if (!SendRequestNativeKeyBindings(*aEvent, &maybeBindings)) {
    return;
  }

  if (maybeBindings.type() == MaybeNativeKeyBinding::TNativeKeyBinding) {
    const NativeKeyBinding& bindings = maybeBindings;
    aAutoCache->Cache(bindings.singleLineCommands(),
                      bindings.multiLineCommands(),
                      bindings.richTextCommands());
  } else {
    aAutoCache->CacheNoCommands();
  }
}

bool
TabChild::RecvNativeSynthesisResponse(const uint64_t& aObserverId,
                                      const nsCString& aResponse)
{
  mozilla::widget::AutoObserverNotifier::NotifySavedObserver(aObserverId, aResponse.get());
  return true;
}

bool
TabChild::RecvRealKeyEvent(const WidgetKeyboardEvent& event,
                           const MaybeNativeKeyBinding& aBindings)
{
  AutoCacheNativeKeyCommands autoCache(mPuppetWidget);

  if (event.mMessage == eKeyPress) {
    // If content code called preventDefault() on a keydown event, then we don't
    // want to process any following keypress events.
    if (mIgnoreKeyPressEvent) {
      return true;
    }
    if (aBindings.type() == MaybeNativeKeyBinding::TNativeKeyBinding) {
      const NativeKeyBinding& bindings = aBindings;
      autoCache.Cache(bindings.singleLineCommands(),
                      bindings.multiLineCommands(),
                      bindings.richTextCommands());
    } else {
      autoCache.CacheNoCommands();
    }
  }

  WidgetKeyboardEvent localEvent(event);
  localEvent.mWidget = mPuppetWidget;
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (event.mMessage == eKeyDown) {
    mIgnoreKeyPressEvent = status == nsEventStatus_eConsumeNoDefault;
  }

  // If a response is desired from the content process, resend the key event.
  // If mAccessKeyForwardedToChild is set, then don't resend the key event yet
  // as RecvHandleAccessKey will do this.
  if (localEvent.mFlags.mWantReplyFromContentProcess) {
    SendReplyKeyEvent(localEvent);
  }

  if (localEvent.mAccessKeyForwardedToChild) {
    SendAccessKeyNotHandled(localEvent);
  }

  if (PresShell::BeforeAfterKeyboardEventEnabled()) {
    SendDispatchAfterKeyboardEvent(localEvent);
  }

  return true;
}

bool
TabChild::RecvKeyEvent(const nsString& aType,
                       const int32_t& aKeyCode,
                       const int32_t& aCharCode,
                       const int32_t& aModifiers,
                       const bool& aPreventDefault)
{
  bool ignored = false;
  nsContentUtils::SendKeyEvent(mPuppetWidget, aType, aKeyCode, aCharCode,
                               aModifiers, aPreventDefault, &ignored);
  return true;
}

bool
TabChild::RecvCompositionEvent(const WidgetCompositionEvent& event)
{
  WidgetCompositionEvent localEvent(event);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  Unused << SendOnEventNeedingAckHandled(event.mMessage);
  return true;
}

bool
TabChild::RecvSelectionEvent(const WidgetSelectionEvent& event)
{
  WidgetSelectionEvent localEvent(event);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  Unused << SendOnEventNeedingAckHandled(event.mMessage);
  return true;
}

a11y::PDocAccessibleChild*
TabChild::AllocPDocAccessibleChild(PDocAccessibleChild*, const uint64_t&)
{
  MOZ_ASSERT(false, "should never call this!");
  return nullptr;
}

bool
TabChild::DeallocPDocAccessibleChild(a11y::PDocAccessibleChild* aChild)
{
#ifdef ACCESSIBILITY
  delete static_cast<mozilla::a11y::DocAccessibleChild*>(aChild);
#endif
  return true;
}

PDocumentRendererChild*
TabChild::AllocPDocumentRendererChild(const nsRect& documentRect,
                                      const mozilla::gfx::Matrix& transform,
                                      const nsString& bgcolor,
                                      const uint32_t& renderFlags,
                                      const bool& flushLayout,
                                      const nsIntSize& renderSize)
{
    return new DocumentRendererChild();
}

bool
TabChild::DeallocPDocumentRendererChild(PDocumentRendererChild* actor)
{
    delete actor;
    return true;
}

bool
TabChild::RecvPDocumentRendererConstructor(PDocumentRendererChild* actor,
                                           const nsRect& documentRect,
                                           const mozilla::gfx::Matrix& transform,
                                           const nsString& bgcolor,
                                           const uint32_t& renderFlags,
                                           const bool& flushLayout,
                                           const nsIntSize& renderSize)
{
    DocumentRendererChild *render = static_cast<DocumentRendererChild *>(actor);

    nsCOMPtr<nsIWebBrowser> browser = do_QueryInterface(WebNavigation());
    if (!browser)
        return true; // silently ignore
    nsCOMPtr<mozIDOMWindowProxy> window;
    if (NS_FAILED(browser->GetContentDOMWindow(getter_AddRefs(window))) ||
        !window)
    {
        return true; // silently ignore
    }

    nsCString data;
    bool ret = render->RenderDocument(nsPIDOMWindowOuter::From(window),
                                      documentRect, transform,
                                      bgcolor,
                                      renderFlags, flushLayout,
                                      renderSize, data);
    if (!ret)
        return true; // silently ignore

    return PDocumentRendererChild::Send__delete__(actor, renderSize, data);
}

PColorPickerChild*
TabChild::AllocPColorPickerChild(const nsString&, const nsString&)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPColorPickerChild(PColorPickerChild* aColorPicker)
{
  nsColorPickerProxy* picker = static_cast<nsColorPickerProxy*>(aColorPicker);
  NS_RELEASE(picker);
  return true;
}

PDatePickerChild*
TabChild::AllocPDatePickerChild(const nsString&, const nsString&)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPDatePickerChild(PDatePickerChild* aDatePicker)
{
  nsDatePickerProxy* picker = static_cast<nsDatePickerProxy*>(aDatePicker);
  NS_RELEASE(picker);
  return true;
}

PFilePickerChild*
TabChild::AllocPFilePickerChild(const nsString&, const int16_t&)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPFilePickerChild(PFilePickerChild* actor)
{
  nsFilePickerProxy* filePicker = static_cast<nsFilePickerProxy*>(actor);
  NS_RELEASE(filePicker);
  return true;
}

auto
TabChild::AllocPIndexedDBPermissionRequestChild(const Principal& aPrincipal)
  -> PIndexedDBPermissionRequestChild*
{
  MOZ_CRASH("PIndexedDBPermissionRequestChild actors should always be created "
            "manually!");
}

bool
TabChild::DeallocPIndexedDBPermissionRequestChild(
                                       PIndexedDBPermissionRequestChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

bool
TabChild::RecvActivateFrameEvent(const nsString& aType, const bool& capture)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, true);
  nsCOMPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, true);
  RefPtr<ContentListener> listener = new ContentListener(this);
  chromeHandler->AddEventListener(aType, listener, capture);
  return true;
}

bool
TabChild::RecvLoadRemoteScript(const nsString& aURL, const bool& aRunInGlobalScope)
{
  if (!mGlobal && !InitTabChildGlobal())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
    return true;

  LoadScriptInternal(aURL, aRunInGlobalScope);
  return true;
}

bool
TabChild::RecvAsyncMessage(const nsString& aMessage,
                           InfallibleTArray<CpowEntry>&& aCpows,
                           const IPC::Principal& aPrincipal,
                           const ClonedMessageData& aData)
{
  if (mTabChildGlobal) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> kungFuDeathGrip(GetGlobal());
    StructuredCloneData data;
    UnpackClonedMessageDataForChild(aData, data);
    RefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    CrossProcessCpowHolder cpows(Manager(), aCpows);
    mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal), nullptr,
                       aMessage, false, &data, &cpows, aPrincipal, nullptr);
  }
  return true;
}

bool
TabChild::RecvAppOfflineStatus(const uint32_t& aId, const bool& aOffline)
{
  // Instantiate the service to make sure gIOService is initialized
  nsCOMPtr<nsIIOService> ioService = mozilla::services::GetIOService();
  if (gIOService && ioService) {
    gIOService->SetAppOfflineInternal(aId, aOffline ?
      nsIAppOfflineInfo::OFFLINE : nsIAppOfflineInfo::ONLINE);
  }
  return true;
}

bool
TabChild::RecvSwappedWithOtherRemoteLoader(const IPCTabContext& aContext)
{
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = ourDocShell->GetWindow();
  if (NS_WARN_IF(!ourWindow)) {
    return true;
  }

  RefPtr<nsDocShell> docShell = static_cast<nsDocShell*>(ourDocShell.get());

  nsCOMPtr<EventTarget> ourEventTarget = ourWindow->GetParentTarget();

  docShell->SetInFrameSwap(true);

  nsContentUtils::FirePageShowEvent(ourDocShell, ourEventTarget, false);
  nsContentUtils::FirePageHideEvent(ourDocShell, ourEventTarget);

  // Owner content type may have changed, so store the possibly updated context
  // and notify others.
  MaybeInvalidTabContext maybeContext(aContext);
  if (!maybeContext.IsValid()) {
    NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                             "the parent process. (%s)",
                             maybeContext.GetInvalidReason()).get());
    MOZ_CRASH("Invalid TabContext received from the parent process.");
  }

  if (!UpdateTabContextAfterSwap(maybeContext.GetTabContext())) {
    MOZ_CRASH("Update to TabContext after swap was denied.");
  }

  // Since mIsMozBrowserElement may change in UpdateTabContextAfterSwap, so we
  // call UpdateFrameType here to make sure the frameType on the docshell is
  // correct.
  UpdateFrameType();

  // Ignore previous value of mTriedBrowserInit since owner content has changed.
  mTriedBrowserInit = true;
  // Initialize the child side of the browser element machinery, if appropriate.
  if (IsMozBrowserOrApp()) {
    RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
  }

  nsContentUtils::FirePageShowEvent(ourDocShell, ourEventTarget, true);

  docShell->SetInFrameSwap(false);

  return true;
}

bool
TabChild::RecvHandleAccessKey(const WidgetKeyboardEvent& aEvent,
                              nsTArray<uint32_t>&& aCharCodes,
                              const int32_t& aModifierMask)
{
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    nsPresContext* pc = presShell->GetPresContext();
    if (pc) {
      if (!pc->EventStateManager()->
                 HandleAccessKey(&(const_cast<WidgetKeyboardEvent&>(aEvent)),
                                 pc, aCharCodes,
                                 aModifierMask, true)) {
        // If no accesskey was found, inform the parent so that accesskeys on
        // menus can be handled.
        WidgetKeyboardEvent localEvent(aEvent);
        localEvent.mWidget = mPuppetWidget;
        SendAccessKeyNotHandled(localEvent);
      }
    }
  }

  return true;
}

bool
TabChild::RecvAudioChannelChangeNotification(const uint32_t& aAudioChannel,
                                             const float& aVolume,
                                             const bool& aMuted)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  if (window) {
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    MOZ_ASSERT(service);

    service->SetAudioChannelVolume(window,
                                   static_cast<AudioChannel>(aAudioChannel),
                                   aVolume);
    service->SetAudioChannelMuted(window,
                                  static_cast<AudioChannel>(aAudioChannel),
                                  aMuted);
  }

  return true;
}

bool
TabChild::RecvSetUseGlobalHistory(const bool& aUse)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  nsresult rv = docShell->SetUseGlobalHistory(aUse);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to set UseGlobalHistory on TabChild docShell");
  }

  return true;
}

bool
TabChild::RecvPrint(const uint64_t& aOuterWindowID, const PrintData& aPrintData)
{
#ifdef NS_PRINTING
  nsGlobalWindow* outerWindow =
    nsGlobalWindow::GetOuterWindowWithId(aOuterWindowID);
  if (NS_WARN_IF(!outerWindow)) {
    return true;
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
    do_GetInterface(outerWindow->AsOuter());
  if (NS_WARN_IF(!webBrowserPrint)) {
    return true;
  }

  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
    do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return true;
  }

  nsCOMPtr<nsIPrintSettings> printSettings;
  nsresult rv =
    printSettingsSvc->GetNewPrintSettings(getter_AddRefs(printSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  nsCOMPtr<nsIPrintSession>  printSession =
    do_CreateInstance("@mozilla.org/gfx/printsession;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

  printSettings->SetPrintSession(printSession);
  printSettingsSvc->DeserializeToPrintSettings(aPrintData, printSettings);
  rv = webBrowserPrint->Print(printSettings, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return true;
  }

#endif
  return true;
}

bool
TabChild::RecvDestroy()
{
  MOZ_ASSERT(mDestroyed == false);
  mDestroyed = true;

  nsTArray<PContentPermissionRequestChild*> childArray =
      nsContentPermissionUtils::GetContentPermissionRequestChildById(GetTabId());

  // Need to close undeleted ContentPermissionRequestChilds before tab is closed.
  for (auto& permissionRequestChild : childArray) {
      auto child = static_cast<RemotePermissionRequest*>(permissionRequestChild);
      child->Destroy();
  }

  while (mActiveSuppressDisplayport > 0) {
    APZCCallbackHelper::SuppressDisplayport(false, nullptr);
    mActiveSuppressDisplayport--;
  }

  if (mTabChildGlobal) {
    // Message handlers are called from the event loop, so it better be safe to
    // run script.
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mTabChildGlobal->DispatchTrustedEvent(NS_LITERAL_STRING("unload"));
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);

  const nsAttrValue::EnumTable* table =
    AudioChannelService::GetAudioChannelTable();

  nsAutoCString topic;
  for (uint32_t i = 0; table[i].tag; ++i) {
    topic.Assign("audiochannel-activity-");
    topic.Append(table[i].tag);

    observerService->RemoveObserver(this, topic.get());
  }

  // XXX what other code in ~TabChild() should we be running here?
  DestroyWindow();

  // Bounce through the event loop once to allow any delayed teardown runnables
  // that were just generated to have a chance to run.
  nsCOMPtr<nsIRunnable> deleteRunnable = new DelayedDeleteRunnable(this);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(deleteRunnable));

  return true;
}

bool
TabChild::RecvSetUpdateHitRegion(const bool& aEnabled)
{
    mUpdateHitRegion = aEnabled;

    // We need to trigger a repaint of the child frame to ensure that it
    // recomputes and sends its region.
    if (!mUpdateHitRegion) {
      return true;
    }

    nsCOMPtr<nsIDocument> document(GetDocument());
    NS_ENSURE_TRUE(document, true);
    nsCOMPtr<nsIPresShell> presShell = document->GetShell();
    NS_ENSURE_TRUE(presShell, true);
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();
    NS_ENSURE_TRUE(presContext, true);
    presContext->InvalidatePaintedLayers();

    return true;
}

bool
TabChild::RecvSetDocShellIsActive(const bool& aIsActive, const bool& aIsHidden)
{
    // docshell is consider prerendered only if not active yet
    mIsPrerendered &= !aIsActive;
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
    if (docShell) {
      if (aIsHidden) {
        docShell->SetIsActive(aIsActive);
      } else {
        docShell->SetIsActiveAndForeground(aIsActive);
      }
    }
    return true;
}

bool
TabChild::RecvNavigateByKey(const bool& aForward, const bool& aForDocumentNavigation)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsCOMPtr<nsIDOMElement> result;
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());

    // Move to the first or last document.
    uint32_t type = aForward ?
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FIRSTDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_ROOT)) :
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LASTDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LAST));
    fm->MoveFocus(window, nullptr, type,
                  nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));

    // No valid root element was found, so move to the first focusable element.
    if (!result && aForward && !aForDocumentNavigation) {
      fm->MoveFocus(window, nullptr, nsIFocusManager::MOVEFOCUS_FIRST,
                  nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
    }

    SendRequestFocus(false);
  }

  return true;
}

bool
TabChild::RecvHandledWindowedPluginKeyEvent(
            const NativeEventData& aKeyEventData,
            const bool& aIsConsumed)
{
  if (NS_WARN_IF(!mPuppetWidget)) {
    return true;
  }
  mPuppetWidget->HandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
  return true;
}

PRenderFrameChild*
TabChild::AllocPRenderFrameChild()
{
    return new RenderFrameChild();
}

bool
TabChild::DeallocPRenderFrameChild(PRenderFrameChild* aFrame)
{
    delete aFrame;
    return true;
}

bool
TabChild::InitTabChildGlobal(FrameScriptLoading aScriptLoading)
{
  if (!mGlobal && !mTabChildGlobal) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
    NS_ENSURE_TRUE(window, false);
    nsCOMPtr<EventTarget> chromeHandler =
      do_QueryInterface(window->GetChromeEventHandler());
    NS_ENSURE_TRUE(chromeHandler, false);

    RefPtr<TabChildGlobal> scope = new TabChildGlobal(this);
    mTabChildGlobal = scope;

    nsISupports* scopeSupports = NS_ISUPPORTS_CAST(EventTarget*, scope);

    NS_NAMED_LITERAL_CSTRING(globalId, "outOfProcessTabChildGlobal");
    NS_ENSURE_TRUE(InitChildGlobalInternal(scopeSupports, globalId), false);

    scope->Init();

    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
    NS_ENSURE_TRUE(root, false);
    root->SetParentTarget(scope);
  }

  if (aScriptLoading != DONT_LOAD_SCRIPTS && !mTriedBrowserInit) {
    mTriedBrowserInit = true;
    // Initialize the child side of the browser element machinery,
    // if appropriate.
    if (IsMozBrowserOrApp()) {
      RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
    }
  }

  return true;
}

bool
TabChild::InitRenderingState(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                             const uint64_t& aLayersId,
                             PRenderFrameChild* aRenderFrame)
{
    mPuppetWidget->InitIMEState();

    RenderFrameChild* remoteFrame = static_cast<RenderFrameChild*>(aRenderFrame);
    if (!remoteFrame) {
        NS_WARNING("failed to construct RenderFrame");
        return false;
    }

    MOZ_ASSERT(aLayersId != 0);
    mTextureFactoryIdentifier = aTextureFactoryIdentifier;

    // Pushing layers transactions directly to a separate
    // compositor context.
    PCompositorBridgeChild* compositorChild = CompositorBridgeChild::Get();
    if (!compositorChild) {
      NS_WARNING("failed to get CompositorBridgeChild instance");
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
    }
    nsTArray<LayersBackend> backends;
    backends.AppendElement(mTextureFactoryIdentifier.mParentBackend);
    bool success;
    PLayerTransactionChild* shadowManager =
        compositorChild->SendPLayerTransactionConstructor(backends,
                                                          aLayersId, &mTextureFactoryIdentifier, &success);
    if (!success) {
      NS_WARNING("failed to properly allocate layer transaction");
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
    }

    if (!shadowManager) {
      NS_WARNING("failed to construct LayersChild");
      // This results in |remoteFrame| being deleted.
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
    }

    ShadowLayerForwarder* lf =
        mPuppetWidget->GetLayerManager(
            shadowManager, mTextureFactoryIdentifier.mParentBackend)
                ->AsShadowForwarder();
    MOZ_ASSERT(lf && lf->HasShadowManager(),
               "PuppetWidget should have shadow manager");
    lf->IdentifyTextureHost(mTextureFactoryIdentifier);
    ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);

    mRemoteFrame = remoteFrame;
    if (aLayersId != 0) {
      if (!sTabChildren) {
        sTabChildren = new TabChildMap;
      }
      MOZ_ASSERT(!sTabChildren->Get(aLayersId));
      sTabChildren->Put(aLayersId, this);
      mLayersId = aLayersId;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    if (observerService) {
        observerService->AddObserver(this,
                                     BEFORE_FIRST_PAINT,
                                     false);
    }
    return true;
}

void
TabChild::GetDPI(float* aDPI)
{
    *aDPI = -1.0;
    if (!mRemoteFrame) {
        return;
    }

    if (mDPI > 0) {
      *aDPI = mDPI;
      return;
    }

    // Fallback to a sync call if needed.
    SendGetDPI(aDPI);
}

void
TabChild::GetDefaultScale(double* aScale)
{
    *aScale = -1.0;
    if (!mRemoteFrame) {
        return;
    }

    if (mDefaultScale > 0) {
      *aScale = mDefaultScale;
      return;
    }

    // Fallback to a sync call if needed.
    SendGetDefaultScale(aScale);
}

void
TabChild::GetMaxTouchPoints(uint32_t* aTouchPoints)
{
  // Fallback to a sync call.
  SendGetMaxTouchPoints(aTouchPoints);
}

void
TabChild::NotifyPainted()
{
    if (!mNotified) {
        mRemoteFrame->SendNotifyCompositorTransaction();
        mNotified = true;
    }
}

void
TabChild::MakeVisible()
{
  if (mPuppetWidget) {
    mPuppetWidget->Show(true);
  }
}

void
TabChild::MakeHidden()
{
  CompositorBridgeChild* compositor = CompositorBridgeChild::Get();

  // Clear cached resources directly. This avoids one extra IPC
  // round-trip from CompositorBridgeChild to CompositorBridgeParent.
  compositor->RecvClearCachedResources(mLayersId);

  if (mPuppetWidget) {
    mPuppetWidget->Show(false);
  }
}

void
TabChild::UpdateHitRegion(const nsRegion& aRegion)
{
  mRemoteFrame->SendUpdateHitRegion(aRegion);
  if (mAPZChild) {
    mAPZChild->SendUpdateHitRegion(aRegion);
  }
}

NS_IMETHODIMP
TabChild::GetMessageManager(nsIContentFrameMessageManager** aResult)
{
  if (mTabChildGlobal) {
    NS_ADDREF(*aResult = mTabChildGlobal);
    return NS_OK;
  }
  *aResult = nullptr;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TabChild::GetWebBrowserChrome(nsIWebBrowserChrome3** aWebBrowserChrome)
{
  NS_IF_ADDREF(*aWebBrowserChrome = mWebBrowserChrome);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetWebBrowserChrome(nsIWebBrowserChrome3* aWebBrowserChrome)
{
  mWebBrowserChrome = aWebBrowserChrome;
  return NS_OK;
}

void
TabChild::SendRequestFocus(bool aCanFocus)
{
  PBrowserChild::SendRequestFocus(aCanFocus);
}

void
TabChild::SendGetTabCount(uint32_t* tabCount)
{
  PBrowserChild::SendGetTabCount(tabCount);
}

void
TabChild::EnableDisableCommands(const nsAString& aAction,
                                nsTArray<nsCString>& aEnabledCommands,
                                nsTArray<nsCString>& aDisabledCommands)
{
  PBrowserChild::SendEnableDisableCommands(PromiseFlatString(aAction),
                                           aEnabledCommands, aDisabledCommands);
}

NS_IMETHODIMP
TabChild::GetTabId(uint64_t* aId)
{
  *aId = GetTabId();
  return NS_OK;
}

void
TabChild::SetTabId(const TabId& aTabId)
{
  MOZ_ASSERT(mUniqueId == 0);

  mUniqueId = aTabId;
  NestedTabChildMap()[mUniqueId] = this;
}

bool
TabChild::DoSendBlockingMessage(JSContext* aCx,
                                const nsAString& aMessage,
                                StructuredCloneData& aData,
                                JS::Handle<JSObject *> aCpows,
                                nsIPrincipal* aPrincipal,
                                nsTArray<StructuredCloneData>* aRetVal,
                                bool aIsSync)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return false;
  }
  InfallibleTArray<CpowEntry> cpows;
  if (aCpows && !Manager()->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
    return false;
  }
  if (aIsSync) {
    return SendSyncMessage(PromiseFlatString(aMessage), data, cpows,
                           Principal(aPrincipal), aRetVal);
  }

  return SendRpcMessage(PromiseFlatString(aMessage), data, cpows,
                        Principal(aPrincipal), aRetVal);
}

nsresult
TabChild::DoSendAsyncMessage(JSContext* aCx,
                             const nsAString& aMessage,
                             StructuredCloneData& aData,
                             JS::Handle<JSObject *> aCpows,
                             nsIPrincipal* aPrincipal)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  InfallibleTArray<CpowEntry> cpows;
  if (aCpows && !Manager()->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
    return NS_ERROR_UNEXPECTED;
  }
  if (!SendAsyncMessage(PromiseFlatString(aMessage), cpows,
                        Principal(aPrincipal), data)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

TabChild*
TabChild::GetFrom(nsIPresShell* aPresShell)
{
  nsIDocument* doc = aPresShell->GetDocument();
  if (!doc) {
      return nullptr;
  }
  nsCOMPtr<nsIDocShell> docShell(doc->GetDocShell());
  return GetFrom(docShell);
}

TabChild*
TabChild::GetFrom(uint64_t aLayersId)
{
  if (!sTabChildren) {
    return nullptr;
  }
  return sTabChildren->Get(aLayersId);
}

void
TabChild::DidComposite(uint64_t aTransactionId,
                       const TimeStamp& aCompositeStart,
                       const TimeStamp& aCompositeEnd)
{
  MOZ_ASSERT(mPuppetWidget);
  MOZ_ASSERT(mPuppetWidget->GetLayerManager());
  MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() ==
               LayersBackend::LAYERS_CLIENT);

  RefPtr<ClientLayerManager> manager = mPuppetWidget->GetLayerManager()->AsClientLayerManager();

  manager->DidComposite(aTransactionId, aCompositeStart, aCompositeEnd);
}

void
TabChild::DidRequestComposite(const TimeStamp& aCompositeReqStart,
                              const TimeStamp& aCompositeReqEnd)
{
  nsCOMPtr<nsIDocShell> docShellComPtr = do_GetInterface(WebNavigation());
  if (!docShellComPtr) {
    return;
  }

  nsDocShell* docShell = static_cast<nsDocShell*>(docShellComPtr.get());
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();

  if (timelines && timelines->HasConsumer(docShell)) {
    timelines->AddMarkerForDocShell(docShell,
      "CompositeForwardTransaction", aCompositeReqStart, MarkerTracingType::START);
    timelines->AddMarkerForDocShell(docShell,
      "CompositeForwardTransaction", aCompositeReqEnd, MarkerTracingType::END);
  }
}

void
TabChild::ClearCachedResources()
{
  MOZ_ASSERT(mPuppetWidget);
  MOZ_ASSERT(mPuppetWidget->GetLayerManager());
  MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() ==
               LayersBackend::LAYERS_CLIENT);

  ClientLayerManager *manager = mPuppetWidget->GetLayerManager()->AsClientLayerManager();
  manager->ClearCachedResources();
}

void
TabChild::InvalidateLayers()
{
  MOZ_ASSERT(mPuppetWidget);
  MOZ_ASSERT(mPuppetWidget->GetLayerManager());
  MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() ==
               LayersBackend::LAYERS_CLIENT);

  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  FrameLayerBuilder::InvalidateAllLayers(lm);
}

void
TabChild::CompositorUpdated(const TextureFactoryIdentifier& aNewIdentifier)
{
  gfxPlatform::GetPlatform()->CompositorUpdated();

  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  ClientLayerManager* clm = lm->AsClientLayerManager();

  mTextureFactoryIdentifier = aNewIdentifier;
  clm->UpdateTextureFactoryIdentifier(aNewIdentifier);
  FrameLayerBuilder::InvalidateAllLayers(clm);
}

NS_IMETHODIMP
TabChild::OnShowTooltip(int32_t aXCoords, int32_t aYCoords, const char16_t *aTipText,
                        const char16_t *aTipDir)
{
    nsString str(aTipText);
    nsString dir(aTipDir);
    SendShowTooltip(aXCoords, aYCoords, str, dir);
    return NS_OK;
}

NS_IMETHODIMP
TabChild::OnHideTooltip()
{
    SendHideTooltip();
    return NS_OK;
}

bool
TabChild::RecvRequestNotifyAfterRemotePaint()
{
  // Get the CompositorBridgeChild instance for this content thread.
  CompositorBridgeChild* compositor = CompositorBridgeChild::Get();

  // Tell the CompositorBridgeChild that, when it gets a RemotePaintIsReady
  // message that it should forward it us so that we can bounce it to our
  // RenderFrameParent.
  compositor->RequestNotifyAfterRemotePaint(this);
  return true;
}

bool
TabChild::RecvUIResolutionChanged(const float& aDpi, const double& aScale)
{
  ScreenIntSize oldScreenSize = GetInnerSize();
  mDPI = 0;
  mDefaultScale = 0;
  static_cast<PuppetWidget*>(mPuppetWidget.get())->UpdateBackingScaleCache(aDpi, aScale);
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();
    if (presContext) {
      presContext->UIResolutionChangedSync();
    }
  }

  ScreenIntSize screenSize = GetInnerSize();
  if (mHasValidInnerSize && oldScreenSize != screenSize) {
    ScreenIntRect screenRect = GetOuterRect();
    mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeDisp.x,
                          screenRect.y + mClientOffset.y + mChromeDisp.y,
                          screenSize.width, screenSize.height, true);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                                nsIBaseWindow::eRepaint);
  }

  return true;
}

bool
TabChild::RecvThemeChanged(nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache)
{
  LookAndFeel::SetIntCache(aLookAndFeelIntCache);
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();
    if (presContext) {
      presContext->ThemeChanged();
    }
  }
  return true;
}

mozilla::plugins::PPluginWidgetChild*
TabChild::AllocPPluginWidgetChild()
{
    return new mozilla::plugins::PluginWidgetChild();
}

bool
TabChild::DeallocPPluginWidgetChild(mozilla::plugins::PPluginWidgetChild* aActor)
{
    delete aActor;
    return true;
}

nsresult
TabChild::CreatePluginWidget(nsIWidget* aParent, nsIWidget** aOut)
{
  *aOut = nullptr;
  mozilla::plugins::PluginWidgetChild* child =
    static_cast<mozilla::plugins::PluginWidgetChild*>(SendPPluginWidgetConstructor());
  if (!child) {
    NS_ERROR("couldn't create PluginWidgetChild");
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIWidget> pluginWidget = nsIWidget::CreatePluginProxyWidget(this, child);
  if (!pluginWidget) {
    NS_ERROR("couldn't create PluginWidgetProxy");
    return NS_ERROR_UNEXPECTED;
  }

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_plugin_ipc_content;
  initData.mUnicode = false;
  initData.clipChildren = true;
  initData.clipSiblings = true;
  nsresult rv = pluginWidget->Create(aParent, nullptr,
                                     LayoutDeviceIntRect(0, 0, 0, 0),
                                     &initData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Creating native plugin widget on the chrome side failed.");
  }
  pluginWidget.forget(aOut);
  return rv;
}

ScreenIntSize
TabChild::GetInnerSize()
{
  LayoutDeviceIntSize innerSize =
    RoundedToInt(mUnscaledInnerSize * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(innerSize, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
};

ScreenIntRect
TabChild::GetOuterRect()
{
  LayoutDeviceIntRect outerRect =
    RoundedToInt(mUnscaledOuterRect * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(outerRect, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
}

TabChildGlobal::TabChildGlobal(TabChildBase* aTabChild)
: mTabChild(aTabChild)
{
  SetIsNotDOMBinding();
}

TabChildGlobal::~TabChildGlobal()
{
}

void
TabChildGlobal::Init()
{
  NS_ASSERTION(!mMessageManager, "Re-initializing?!?");
  mMessageManager = new nsFrameMessageManager(mTabChild,
                                              nullptr,
                                              MM_CHILD);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChildGlobal)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TabChildGlobal,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChild);
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TabChildGlobal,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChild)
  tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TabChildGlobal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TabChildGlobal, DOMEventTargetHelper)

// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
TabChildGlobal::MarkForCC()
{
  if (mTabChild) {
    mTabChild->MarkScopesForCC();
  }
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->MarkForCC();
  }
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

NS_IMETHODIMP
TabChildGlobal::GetContent(mozIDOMWindowProxy** aContent)
{
  *aContent = nullptr;
  if (!mTabChild)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(mTabChild->WebNavigation());
  window.forget(aContent);
  return NS_OK;
}

NS_IMETHODIMP
TabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nullptr;
  if (!mTabChild)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mTabChild->WebNavigation());
  docShell.swap(*aDocShell);
  return NS_OK;
}

nsIPrincipal*
TabChildGlobal::GetPrincipal()
{
  if (!mTabChild)
    return nullptr;
  return mTabChild->GetPrincipal();
}

JSObject*
TabChildGlobal::GetGlobalJSObject()
{
  NS_ENSURE_TRUE(mTabChild, nullptr);
  nsCOMPtr<nsIXPConnectJSObjectHolder> ref = mTabChild->GetGlobal();
  NS_ENSURE_TRUE(ref, nullptr);
  return ref->GetJSObject();
}
