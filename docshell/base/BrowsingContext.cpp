/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContext.h"

#include "ipc/IPCMessageUtils.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/DocAccessibleParent.h"
#  include "mozilla/a11y/Platform.h"
#  include "mozilla/a11y/RemoteAccessibleBase.h"
#  include "nsAccessibilityService.h"
#  if defined(XP_WIN)
#    include "mozilla/a11y/AccessibleWrap.h"
#    include "mozilla/a11y/Compatibility.h"
#    include "mozilla/a11y/nsWinUtils.h"
#  endif
#endif
#include "mozilla/AppShutdown.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLEmbedElement.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/MediaDevices.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/dom/SessionStoreDataCollector.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/UserActivationIPCUtils.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/SyncedContextInlines.h"
#include "mozilla/dom/XULFrameElement.h"
#include "mozilla/net/DocumentLoadListener.h"
#include "mozilla/net/RequestContextService.h"
#include "mozilla/Assertions.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/HashTable.h"
#include "mozilla/Logging.h"
#include "mozilla/MediaFeatureChange.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPrefs_page_load.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/URLQueryStringStripper.h"
#include "nsIURIFixup.h"
#include "nsIXULRuntime.h"

#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsFocusManager.h"
#include "nsGlobalWindowOuter.h"
#include "nsIObserverService.h"
#include "nsISHistory.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"
#include "nsSandboxFlags.h"
#include "nsScriptError.h"
#include "nsThreadUtils.h"
#include "xpcprivate.h"

#include "AutoplayPolicy.h"
#include "GVAutoplayRequestStatusIPC.h"

extern mozilla::LazyLogModule gAutoplayPermissionLog;
extern mozilla::LazyLogModule gTimeoutDeferralLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace IPC {
// Allow serialization and deserialization of OrientationType over IPC
template <>
struct ParamTraits<mozilla::dom::OrientationType>
    : public ContiguousEnumSerializer<
          mozilla::dom::OrientationType,
          mozilla::dom::OrientationType::Portrait_primary,
          mozilla::dom::OrientationType::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::DisplayMode>
    : public ContiguousEnumSerializer<mozilla::dom::DisplayMode,
                                      mozilla::dom::DisplayMode::Browser,
                                      mozilla::dom::DisplayMode::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::PrefersColorSchemeOverride>
    : public ContiguousEnumSerializer<
          mozilla::dom::PrefersColorSchemeOverride,
          mozilla::dom::PrefersColorSchemeOverride::None,
          mozilla::dom::PrefersColorSchemeOverride::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::ExplicitActiveStatus>
    : public ContiguousEnumSerializer<
          mozilla::dom::ExplicitActiveStatus,
          mozilla::dom::ExplicitActiveStatus::None,
          mozilla::dom::ExplicitActiveStatus::EndGuard_> {};

// Allow serialization and deserialization of TouchEventsOverride over IPC
template <>
struct ParamTraits<mozilla::dom::TouchEventsOverride>
    : public ContiguousEnumSerializer<
          mozilla::dom::TouchEventsOverride,
          mozilla::dom::TouchEventsOverride::Disabled,
          mozilla::dom::TouchEventsOverride::EndGuard_> {};
}  // namespace IPC

namespace mozilla {
namespace dom {

// Explicit specialization of the `Transaction` type. Required by the `extern
// template class` declaration in the header.
template class syncedcontext::Transaction<BrowsingContext>;

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

static LazyLogModule gBrowsingContextLog("BrowsingContext");
static LazyLogModule gBrowsingContextSyncLog("BrowsingContextSync");

typedef nsTHashMap<nsUint64HashKey, BrowsingContext*> BrowsingContextMap;

// All BrowsingContexts indexed by Id
static StaticAutoPtr<BrowsingContextMap> sBrowsingContexts;
// Top-level Content BrowsingContexts only, indexed by BrowserId instead of Id
static StaticAutoPtr<BrowsingContextMap> sCurrentTopByBrowserId;

static void UnregisterBrowserId(BrowsingContext* aBrowsingContext) {
  if (!aBrowsingContext->IsTopContent() || !sCurrentTopByBrowserId) {
    return;
  }

  // Avoids an extra lookup
  auto browserIdEntry =
      sCurrentTopByBrowserId->Lookup(aBrowsingContext->BrowserId());
  if (browserIdEntry && browserIdEntry.Data() == aBrowsingContext) {
    browserIdEntry.Remove();
  }
}

static void Register(BrowsingContext* aBrowsingContext) {
  sBrowsingContexts->InsertOrUpdate(aBrowsingContext->Id(), aBrowsingContext);
  if (aBrowsingContext->IsTopContent()) {
    sCurrentTopByBrowserId->InsertOrUpdate(aBrowsingContext->BrowserId(),
                                           aBrowsingContext);
  }

  aBrowsingContext->Group()->Register(aBrowsingContext);
}

BrowsingContext* BrowsingContext::GetParent() const {
  return mParentWindow ? mParentWindow->GetBrowsingContext() : nullptr;
}

bool BrowsingContext::IsInSubtreeOf(BrowsingContext* aContext) {
  BrowsingContext* bc = this;
  do {
    if (bc == aContext) {
      return true;
    }
  } while ((bc = bc->GetParent()));
  return false;
}

BrowsingContext* BrowsingContext::Top() {
  BrowsingContext* bc = this;
  while (bc->mParentWindow) {
    bc = bc->GetParent();
  }
  return bc;
}

const BrowsingContext* BrowsingContext::Top() const {
  const BrowsingContext* bc = this;
  while (bc->mParentWindow) {
    bc = bc->GetParent();
  }
  return bc;
}

int32_t BrowsingContext::IndexOf(BrowsingContext* aChild) {
  int32_t index = -1;
  for (BrowsingContext* child : Children()) {
    ++index;
    if (child == aChild) {
      break;
    }
  }
  return index;
}

WindowContext* BrowsingContext::GetTopWindowContext() const {
  if (mParentWindow) {
    return mParentWindow->TopWindowContext();
  }
  return mCurrentWindowContext;
}

/* static */
void BrowsingContext::Init() {
  if (!sBrowsingContexts) {
    sBrowsingContexts = new BrowsingContextMap();
    sCurrentTopByBrowserId = new BrowsingContextMap();
    ClearOnShutdown(&sBrowsingContexts);
    ClearOnShutdown(&sCurrentTopByBrowserId);
  }
}

/* static */
LogModule* BrowsingContext::GetLog() { return gBrowsingContextLog; }

/* static */
LogModule* BrowsingContext::GetSyncLog() { return gBrowsingContextSyncLog; }

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::Get(uint64_t aId) {
  return do_AddRef(sBrowsingContexts->Get(aId));
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::GetCurrentTopByBrowserId(
    uint64_t aBrowserId) {
  return do_AddRef(sCurrentTopByBrowserId->Get(aBrowserId));
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::GetFromWindow(
    WindowProxyHolder& aProxy) {
  return do_AddRef(aProxy.get());
}

CanonicalBrowsingContext* BrowsingContext::Canonical() {
  return CanonicalBrowsingContext::Cast(this);
}

bool BrowsingContext::IsOwnedByProcess() const {
  return mIsInProcess && mDocShell &&
         !nsDocShell::Cast(mDocShell)->WillChangeProcess();
}

bool BrowsingContext::SameOriginWithTop() {
  MOZ_ASSERT(IsInProcess());
  // If the top BrowsingContext is not same-process to us, it is cross-origin
  if (!Top()->IsInProcess()) {
    return false;
  }

  nsIDocShell* docShell = GetDocShell();
  if (!docShell) {
    return false;
  }
  Document* doc = docShell->GetDocument();
  if (!doc) {
    return false;
  }
  nsIPrincipal* principal = doc->NodePrincipal();

  nsIDocShell* topDocShell = Top()->GetDocShell();
  if (!topDocShell) {
    return false;
  }
  Document* topDoc = topDocShell->GetDocument();
  if (!topDoc) {
    return false;
  }
  nsIPrincipal* topPrincipal = topDoc->NodePrincipal();

  return principal->Equals(topPrincipal);
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::CreateDetached(
    nsGlobalWindowInner* aParent, BrowsingContext* aOpener,
    BrowsingContextGroup* aSpecificGroup, const nsAString& aName, Type aType,
    bool aIsPopupRequested, bool aCreatedDynamically) {
  if (aParent) {
    MOZ_DIAGNOSTIC_ASSERT(aParent->GetWindowContext());
    MOZ_DIAGNOSTIC_ASSERT(aParent->GetBrowsingContext()->mType == aType);
    MOZ_DIAGNOSTIC_ASSERT(aParent->GetBrowsingContext()->GetBrowserId() != 0);
  }

  MOZ_DIAGNOSTIC_ASSERT(aType != Type::Chrome || XRE_IsParentProcess());

  uint64_t id = nsContentUtils::GenerateBrowsingContextId();

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " in %s", id,
           XRE_IsParentProcess() ? "Parent" : "Child"));

  RefPtr<BrowsingContext> parentBC =
      aParent ? aParent->GetBrowsingContext() : nullptr;
  RefPtr<WindowContext> parentWC =
      aParent ? aParent->GetWindowContext() : nullptr;
  BrowsingContext* inherit = parentBC ? parentBC.get() : aOpener;

  // Determine which BrowsingContextGroup this context should be created in.
  RefPtr<BrowsingContextGroup> group = aSpecificGroup;
  if (aType == Type::Chrome) {
    MOZ_DIAGNOSTIC_ASSERT(!group);
    group = BrowsingContextGroup::GetChromeGroup();
  } else if (!group) {
    group = BrowsingContextGroup::Select(parentWC, aOpener);
  }

  // Configure initial values for synced fields.
  FieldValues fields;
  fields.mName = aName;

  if (aOpener) {
    MOZ_DIAGNOSTIC_ASSERT(!aParent,
                          "new BC with both initial opener and parent");
    MOZ_DIAGNOSTIC_ASSERT(aOpener->Group() == group);
    MOZ_DIAGNOSTIC_ASSERT(aOpener->mType == aType);
    fields.mOpenerId = aOpener->Id();
    fields.mHadOriginalOpener = true;
  }

  if (aParent) {
    MOZ_DIAGNOSTIC_ASSERT(parentBC->Group() == group);
    MOZ_DIAGNOSTIC_ASSERT(parentBC->mType == aType);
    fields.mEmbedderInnerWindowId = aParent->WindowID();

    // XXX(farre): Can/Should we check aParent->IsLoading() here? (Bug
    // 1608448) Check if the parent was itself loading already
    auto readystate = aParent->GetDocument()->GetReadyStateEnum();
    fields.mAncestorLoading =
        parentBC->GetAncestorLoading() ||
        readystate == Document::ReadyState::READYSTATE_LOADING ||
        readystate == Document::ReadyState::READYSTATE_INTERACTIVE;
  }

  fields.mBrowserId =
      parentBC ? parentBC->GetBrowserId() : nsContentUtils::GenerateBrowserId();

  fields.mOpenerPolicy = nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;
  if (aOpener && aOpener->SameOriginWithTop()) {
    // We inherit the opener policy if there is a creator and if the creator's
    // origin is same origin with the creator's top-level origin.
    // If it is cross origin we should not inherit the CrossOriginOpenerPolicy
    fields.mOpenerPolicy = aOpener->Top()->GetOpenerPolicy();
  } else if (aOpener) {
    // They are not same origin
    auto topPolicy = aOpener->Top()->GetOpenerPolicy();
    MOZ_RELEASE_ASSERT(topPolicy == nsILoadInfo::OPENER_POLICY_UNSAFE_NONE ||
                       topPolicy ==
                           nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_ALLOW_POPUPS);
  }

  nsContentUtils::GenerateUUIDInPlace(fields.mHistoryID);
  fields.mExplicitActive = [&] {
    if (parentBC) {
      // Non-root browsing-contexts inherit their status from its parent.
      return ExplicitActiveStatus::None;
    }
    if (aType == Type::Content) {
      // Content gets managed by the chrome front-end / embedder element and
      // starts as inactive.
      return ExplicitActiveStatus::Inactive;
    }
    // Chrome starts as active.
    return ExplicitActiveStatus::Active;
  }();

  fields.mFullZoom = parentBC ? parentBC->FullZoom() : 1.0f;
  fields.mTextZoom = parentBC ? parentBC->TextZoom() : 1.0f;

  bool allowContentRetargeting =
      inherit ? inherit->GetAllowContentRetargetingOnChildren() : true;
  fields.mAllowContentRetargeting = allowContentRetargeting;
  fields.mAllowContentRetargetingOnChildren = allowContentRetargeting;

  // Assume top allows fullscreen for its children unless otherwise stated.
  // Subframes start with it false unless otherwise noted in SetEmbedderElement.
  fields.mFullscreenAllowedByOwner = !aParent;

  fields.mAllowPlugins = inherit ? inherit->GetAllowPlugins() : true;

  fields.mDefaultLoadFlags =
      inherit ? inherit->GetDefaultLoadFlags() : nsIRequest::LOAD_NORMAL;

  fields.mOrientationLock = mozilla::hal::eScreenOrientation_None;

  fields.mUseGlobalHistory = inherit ? inherit->GetUseGlobalHistory() : false;

  fields.mUseErrorPages = true;

  fields.mTouchEventsOverrideInternal = TouchEventsOverride::None;

  fields.mAllowJavascript = inherit ? inherit->GetAllowJavascript() : true;

  fields.mIsPopupRequested = aIsPopupRequested;

  if (!parentBC) {
    fields.mShouldDelayMediaFromStart =
        StaticPrefs::media_block_autoplay_until_in_foreground();
  }

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context = new CanonicalBrowsingContext(parentWC, group, id,
                                           /* aOwnerProcessId */ 0,
                                           /* aEmbedderProcessId */ 0, aType,
                                           std::move(fields));
  } else {
    context =
        new BrowsingContext(parentWC, group, id, aType, std::move(fields));
  }

  context->mEmbeddedByThisProcess = XRE_IsParentProcess() || aParent;
  context->mCreatedDynamically = aCreatedDynamically;
  if (inherit) {
    context->mPrivateBrowsingId = inherit->mPrivateBrowsingId;
    context->mUseRemoteTabs = inherit->mUseRemoteTabs;
    context->mUseRemoteSubframes = inherit->mUseRemoteSubframes;
    context->mOriginAttributes = inherit->mOriginAttributes;
  }

  nsCOMPtr<nsIRequestContextService> rcsvc =
      net::RequestContextService::GetOrCreate();
  if (rcsvc) {
    nsCOMPtr<nsIRequestContext> requestContext;
    nsresult rv = rcsvc->NewRequestContext(getter_AddRefs(requestContext));
    if (NS_SUCCEEDED(rv) && requestContext) {
      context->mRequestContextId = requestContext->GetID();
    }
  }

  return context.forget();
}

already_AddRefed<BrowsingContext> BrowsingContext::CreateIndependent(
    Type aType) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "BCs created in the content process must be related to "
                        "some BrowserChild");
  RefPtr<BrowsingContext> bc(
      CreateDetached(nullptr, nullptr, nullptr, u""_ns, aType, false));
  bc->mWindowless = bc->IsContent();
  bc->mEmbeddedByThisProcess = true;
  bc->EnsureAttached();
  return bc.forget();
}

void BrowsingContext::EnsureAttached() {
  if (!mEverAttached) {
    Register(this);

    // Attach the browsing context to the tree.
    Attach(/* aFromIPC */ false, /* aOriginProcess */ nullptr);
  }
}

/* static */
void BrowsingContext::CreateFromIPC(BrowsingContext::IPCInitializer&& aInit,
                                    BrowsingContextGroup* aGroup,
                                    ContentParent* aOriginProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aOriginProcess || XRE_IsContentProcess());
  MOZ_DIAGNOSTIC_ASSERT(aGroup);

  uint64_t originId = 0;
  if (aOriginProcess) {
    originId = aOriginProcess->ChildID();
    aGroup->EnsureHostProcess(aOriginProcess);
  }

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " from IPC (origin=0x%08" PRIx64 ")",
           aInit.mId, originId));

  RefPtr<WindowContext> parent = aInit.GetParent();

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    // If the new BrowsingContext has a parent, it is a sub-frame embedded in
    // whatever process sent the message. If it doesn't, and is not windowless,
    // it is a new window or tab, and will be embedded in the parent process.
    uint64_t embedderProcessId = (aInit.mWindowless || parent) ? originId : 0;
    context = new CanonicalBrowsingContext(parent, aGroup, aInit.mId, originId,
                                           embedderProcessId, Type::Content,
                                           std::move(aInit.mFields));
  } else {
    context = new BrowsingContext(parent, aGroup, aInit.mId, Type::Content,
                                  std::move(aInit.mFields));
  }

  context->mWindowless = aInit.mWindowless;
  context->mCreatedDynamically = aInit.mCreatedDynamically;
  context->mChildOffset = aInit.mChildOffset;
  if (context->GetHasSessionHistory()) {
    context->CreateChildSHistory();
    if (mozilla::SessionHistoryInParent()) {
      context->GetChildSessionHistory()->SetIndexAndLength(
          aInit.mSessionHistoryIndex, aInit.mSessionHistoryCount, nsID());
    }
  }

  // NOTE: Call through the `Set` methods for these values to ensure that any
  // relevant process-local state is also updated.
  context->SetOriginAttributes(aInit.mOriginAttributes);
  context->SetRemoteTabs(aInit.mUseRemoteTabs);
  context->SetRemoteSubframes(aInit.mUseRemoteSubframes);
  context->mRequestContextId = aInit.mRequestContextId;
  // NOTE: Private browsing ID is set by `SetOriginAttributes`.

  Register(context);

  context->Attach(/* aFromIPC */ true, aOriginProcess);
}

BrowsingContext::BrowsingContext(WindowContext* aParentWindow,
                                 BrowsingContextGroup* aGroup,
                                 uint64_t aBrowsingContextId, Type aType,
                                 FieldValues&& aInit)
    : mFields(std::move(aInit)),
      mType(aType),
      mBrowsingContextId(aBrowsingContextId),
      mGroup(aGroup),
      mParentWindow(aParentWindow),
      mPrivateBrowsingId(0),
      mEverAttached(false),
      mIsInProcess(false),
      mIsDiscarded(false),
      mWindowless(false),
      mDanglingRemoteOuterProxies(false),
      mEmbeddedByThisProcess(false),
      mUseRemoteTabs(false),
      mUseRemoteSubframes(false),
      mCreatedDynamically(false),
      mIsInBFCache(false),
      mCanExecuteScripts(true),
      mChildOffset(0) {
  MOZ_RELEASE_ASSERT(!mParentWindow || mParentWindow->Group() == mGroup);
  MOZ_RELEASE_ASSERT(mBrowsingContextId != 0);
  MOZ_RELEASE_ASSERT(mGroup);
}

void BrowsingContext::SetDocShell(nsIDocShell* aDocShell) {
  // XXX(nika): We should communicate that we are now an active BrowsingContext
  // process to the parent & do other validation here.
  MOZ_DIAGNOSTIC_ASSERT(mEverAttached);
  MOZ_RELEASE_ASSERT(aDocShell->GetBrowsingContext() == this);
  mDocShell = aDocShell;
  mDanglingRemoteOuterProxies = !mIsInProcess;
  mIsInProcess = true;
  if (mChildSessionHistory) {
    mChildSessionHistory->SetIsInProcess(true);
  }
}

// This class implements a callback that will return the remote window proxy for
// mBrowsingContext in that compartment, if it has one. It also removes the
// proxy from the map, because the object will be transplanted into another kind
// of object.
class MOZ_STACK_CLASS CompartmentRemoteProxyTransplantCallback
    : public js::CompartmentTransplantCallback {
 public:
  explicit CompartmentRemoteProxyTransplantCallback(
      BrowsingContext* aBrowsingContext)
      : mBrowsingContext(aBrowsingContext) {}

  virtual JSObject* getObjectToTransplant(
      JS::Compartment* compartment) override {
    auto* priv = xpc::CompartmentPrivate::Get(compartment);
    if (!priv) {
      return nullptr;
    }

    auto& map = priv->GetRemoteProxyMap();
    auto result = map.lookup(mBrowsingContext);
    if (!result) {
      return nullptr;
    }
    JSObject* resultObject = result->value();
    map.remove(result);

    return resultObject;
  }

 private:
  BrowsingContext* mBrowsingContext;
};

void BrowsingContext::CleanUpDanglingRemoteOuterWindowProxies(
    JSContext* aCx, JS::MutableHandle<JSObject*> aOuter) {
  if (!mDanglingRemoteOuterProxies) {
    return;
  }
  mDanglingRemoteOuterProxies = false;

  CompartmentRemoteProxyTransplantCallback cb(this);
  js::RemapRemoteWindowProxies(aCx, &cb, aOuter);
}

bool BrowsingContext::IsActive() const {
  const BrowsingContext* current = this;
  do {
    auto explicit_ = current->GetExplicitActive();
    if (explicit_ != ExplicitActiveStatus::None) {
      return explicit_ == ExplicitActiveStatus::Active;
    }
    if (mParentWindow && !mParentWindow->IsCurrent()) {
      return false;
    }
  } while ((current = current->GetParent()));

  return false;
}

bool BrowsingContext::GetIsActiveBrowserWindow() {
  if (!XRE_IsParentProcess()) {
    return Top()->GetIsActiveBrowserWindowInternal();
  }

  // chrome:// urls loaded in the parent won't receive
  // their own activation so we defer to the top chrome
  // Browsing Context when in the parent process.
  RefPtr<CanonicalBrowsingContext> chromeTop =
      Canonical()->TopCrossChromeBoundary();
  return chromeTop->GetIsActiveBrowserWindowInternal();
}

void BrowsingContext::SetIsActiveBrowserWindow(bool aActive) {
  Unused << SetIsActiveBrowserWindowInternal(aActive);
}

bool BrowsingContext::FullscreenAllowed() const {
  for (auto* current = this; current; current = current->GetParent()) {
    if (!current->GetFullscreenAllowedByOwner()) {
      return false;
    }
  }
  return true;
}

static bool OwnerAllowsFullscreen(const Element& aEmbedder) {
  if (aEmbedder.IsXULElement()) {
    return !aEmbedder.HasAttr(nsGkAtoms::disablefullscreen);
  }
  if (aEmbedder.IsHTMLElement(nsGkAtoms::iframe)) {
    // This is controlled by feature policy.
    return true;
  }
  if (const auto* embed = HTMLEmbedElement::FromNode(aEmbedder)) {
    return embed->AllowFullscreen();
  }
  return false;
}

void BrowsingContext::SetEmbedderElement(Element* aEmbedder) {
  mEmbeddedByThisProcess = true;

  // Update embedder-element-specific fields in a shared transaction.
  // Don't do this when clearing our embedder, as we're being destroyed either
  // way.
  if (aEmbedder) {
    Transaction txn;
    txn.SetEmbedderElementType(Some(aEmbedder->LocalName()));
    if (nsCOMPtr<nsPIDOMWindowInner> inner =
            do_QueryInterface(aEmbedder->GetOwnerGlobal())) {
      txn.SetEmbedderInnerWindowId(inner->WindowID());
    }
    txn.SetFullscreenAllowedByOwner(OwnerAllowsFullscreen(*aEmbedder));
    if (XRE_IsParentProcess() && IsTopContent()) {
      nsAutoString messageManagerGroup;
      if (aEmbedder->IsXULElement()) {
        aEmbedder->GetAttr(nsGkAtoms::messagemanagergroup, messageManagerGroup);
        if (!aEmbedder->AttrValueIs(kNameSpaceID_None,
                                    nsGkAtoms::initiallyactive,
                                    nsGkAtoms::_false, eIgnoreCase)) {
          txn.SetExplicitActive(ExplicitActiveStatus::Active);
        }
      }
      txn.SetMessageManagerGroup(messageManagerGroup);

      bool useGlobalHistory = !aEmbedder->HasAttr(
          kNameSpaceID_None, nsGkAtoms::disableglobalhistory);
      txn.SetUseGlobalHistory(useGlobalHistory);
    }

    MOZ_ALWAYS_SUCCEEDS(txn.Commit(this));
  }

  if (XRE_IsParentProcess() && IsTopContent()) {
    Canonical()->MaybeSetPermanentKey(aEmbedder);
  }

  mEmbedderElement = aEmbedder;

  if (mEmbedderElement) {
    if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
      obs->NotifyWhenScriptSafe(ToSupports(this),
                                "browsing-context-did-set-embedder", nullptr);
    }
  }
}

void BrowsingContext::Embed() {
  if (auto* frame = HTMLIFrameElement::FromNode(mEmbedderElement)) {
    frame->BindToBrowsingContext(this);
  }
}

void BrowsingContext::Attach(bool aFromIPC, ContentParent* aOriginProcess) {
  MOZ_DIAGNOSTIC_ASSERT(!mEverAttached);
  MOZ_DIAGNOSTIC_ASSERT_IF(aFromIPC, aOriginProcess || XRE_IsContentProcess());
  mEverAttached = true;

  if (MOZ_LOG_TEST(GetLog(), LogLevel::Debug)) {
    nsAutoCString suffix;
    mOriginAttributes.CreateSuffix(suffix);
    MOZ_LOG(GetLog(), LogLevel::Debug,
            ("%s: Connecting 0x%08" PRIx64 " to 0x%08" PRIx64
             " (private=%d, remote=%d, fission=%d, oa=%s)",
             XRE_IsParentProcess() ? "Parent" : "Child", Id(),
             GetParent() ? GetParent()->Id() : 0, (int)mPrivateBrowsingId,
             (int)mUseRemoteTabs, (int)mUseRemoteSubframes, suffix.get()));
  }

  MOZ_DIAGNOSTIC_ASSERT(mGroup);
  MOZ_DIAGNOSTIC_ASSERT(!mIsDiscarded);

  AssertCoherentLoadContext();

  // Add ourselves either to our parent or BrowsingContextGroup's child list.
  if (mParentWindow) {
    if (!aFromIPC) {
      MOZ_DIAGNOSTIC_ASSERT(!mParentWindow->IsDiscarded(),
                            "local attach in discarded window");
      MOZ_DIAGNOSTIC_ASSERT(!GetParent()->IsDiscarded(),
                            "local attach call in discarded bc");
      MOZ_DIAGNOSTIC_ASSERT(mParentWindow->GetWindowGlobalChild(),
                            "local attach call with oop parent window");
      MOZ_DIAGNOSTIC_ASSERT(mParentWindow->GetWindowGlobalChild()->CanSend(),
                            "local attach call with dead parent window");
    }
    mChildOffset =
        mCreatedDynamically ? -1 : mParentWindow->Children().Length();
    mParentWindow->AppendChildBrowsingContext(this);
    RecomputeCanExecuteScripts();
  } else {
    mGroup->Toplevels().AppendElement(this);
  }

  if (GetIsPopupSpam()) {
    PopupBlocker::RegisterOpenPopupSpam();
  }

  if (IsTop() && GetHasSessionHistory() && !mChildSessionHistory) {
    CreateChildSHistory();
  }

  if (XRE_IsContentProcess() && !aFromIPC) {
    // Send attach to our parent if we need to.
    ContentChild::GetSingleton()->SendCreateBrowsingContext(
        mGroup->Id(), GetIPCInitializer());
  } else if (XRE_IsParentProcess()) {
    // If this window was created as a subframe by a content process, it must be
    // being hosted within the same BrowserParent as its mParentWindow.
    // Toplevel BrowsingContexts created by content have their BrowserParent
    // configured during `RecvConstructPopupBrowser`.
    if (mParentWindow && aOriginProcess) {
      MOZ_DIAGNOSTIC_ASSERT(
          mParentWindow->Canonical()->GetContentParent() == aOriginProcess,
          "Creator process isn't the same as our embedder?");
      Canonical()->SetCurrentBrowserParent(
          mParentWindow->Canonical()->GetBrowserParent());
    }

    mGroup->EachOtherParent(aOriginProcess, [&](ContentParent* aParent) {
      MOZ_DIAGNOSTIC_ASSERT(IsContent(),
                            "chrome BCG cannot be synced to content process");
      if (!Canonical()->IsEmbeddedInProcess(aParent->ChildID())) {
        Unused << aParent->SendCreateBrowsingContext(mGroup->Id(),
                                                     GetIPCInitializer());
      }
    });

    // We want to create a BrowsingContextWebProgress for all content
    // BrowsingContexts.
    if (IsContent() && !Canonical()->mWebProgress) {
      Canonical()->mWebProgress = new BrowsingContextWebProgress(Canonical());
    }
  }

  if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
    obs->NotifyWhenScriptSafe(ToSupports(this), "browsing-context-attached",
                              nullptr);
  }

  if (XRE_IsParentProcess()) {
    Canonical()->CanonicalAttach();
  }
}

void BrowsingContext::Detach(bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Detaching 0x%08" PRIx64 " from 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child", Id(),
           GetParent() ? GetParent()->Id() : 0));

  MOZ_DIAGNOSTIC_ASSERT(mEverAttached);
  MOZ_DIAGNOSTIC_ASSERT(!mIsDiscarded);

  if (XRE_IsParentProcess()) {
    Canonical()->AddPendingDiscard();
  }
  auto callListeners =
      MakeScopeExit([&, listeners = std::move(mDiscardListeners), id = Id()] {
        for (const auto& listener : listeners) {
          listener(id);
        }
        if (XRE_IsParentProcess()) {
          Canonical()->RemovePendingDiscard();
        }
      });

  nsCOMPtr<nsIRequestContextService> rcsvc =
      net::RequestContextService::GetOrCreate();
  if (rcsvc) {
    rcsvc->RemoveRequestContext(GetRequestContextId());
  }

  // This will only ever be null if the cycle-collector has unlinked us. Don't
  // try to detach ourselves in that case.
  if (NS_WARN_IF(!mGroup)) {
    MOZ_ASSERT_UNREACHABLE();
    return;
  }

  if (mParentWindow) {
    mParentWindow->RemoveChildBrowsingContext(this);
  } else {
    mGroup->Toplevels().RemoveElement(this);
  }

  if (XRE_IsParentProcess()) {
    RefPtr<CanonicalBrowsingContext> self{Canonical()};
    Group()->EachParent([&](ContentParent* aParent) {
      // Only the embedder process is allowed to initiate a BrowsingContext
      // detach, so if we've gotten here, the host process already knows we've
      // been detached, and there's no need to tell it again.
      //
      // If the owner process is not the same as the embedder process, its
      // BrowsingContext will be detached when its nsWebBrowser instance is
      // destroyed.
      bool doDiscard = !Canonical()->IsEmbeddedInProcess(aParent->ChildID()) &&
                       !Canonical()->IsOwnedByProcess(aParent->ChildID());

      // Hold a strong reference to ourself, and keep our BrowsingContextGroup
      // alive, until the responses comes back to ensure we don't die while
      // messages relating to this context are in-flight.
      //
      // When the callback is called, the keepalive on our group will be
      // destroyed, and the reference to the BrowsingContext will be dropped,
      // which may cause it to be fully destroyed.
      mGroup->AddKeepAlive();
      self->AddPendingDiscard();
      auto callback = [self](auto) {
        self->mGroup->RemoveKeepAlive();
        self->RemovePendingDiscard();
      };

      aParent->SendDiscardBrowsingContext(this, doDiscard, callback, callback);
    });
  } else {
    // Hold a strong reference to ourself until the responses come back to
    // ensure the BrowsingContext isn't cleaned up before the parent process
    // acknowledges the discard request.
    auto callback = [self = RefPtr{this}](auto) {};
    ContentChild::GetSingleton()->SendDiscardBrowsingContext(
        this, !aFromIPC, callback, callback);
  }

  mGroup->Unregister(this);
  UnregisterBrowserId(this);
  mIsDiscarded = true;

  if (XRE_IsParentProcess()) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      fm->BrowsingContextDetached(this);
    }
  }

  if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
    // Why the context is being discarded. This will always be "discard" in the
    // content process, but may be "replace" if it's known the context being
    // replaced in the parent process.
    const char16_t* why = u"discard";
    if (XRE_IsParentProcess() && IsTop() && !Canonical()->GetWebProgress()) {
      why = u"replace";
    }
    obs->NotifyObservers(ToSupports(this), "browsing-context-discarded", why);
  }

  // NOTE: Doesn't use SetClosed, as it will be set in all processes
  // automatically by calls to Detach()
  mFields.SetWithoutSyncing<IDX_Closed>(true);

  if (GetIsPopupSpam()) {
    PopupBlocker::UnregisterOpenPopupSpam();
    // NOTE: Doesn't use SetIsPopupSpam, as it will be set all processes
    // automatically.
    mFields.SetWithoutSyncing<IDX_IsPopupSpam>(false);
  }

  AssertOriginAttributesMatchPrivateBrowsing();

  if (XRE_IsParentProcess()) {
    Canonical()->CanonicalDiscard();
  }
}

void BrowsingContext::AddDiscardListener(
    std::function<void(uint64_t)>&& aListener) {
  if (mIsDiscarded) {
    aListener(Id());
    return;
  }
  mDiscardListeners.AppendElement(std::move(aListener));
}

void BrowsingContext::PrepareForProcessChange() {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Preparing 0x%08" PRIx64 " for a process change",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  MOZ_ASSERT(mIsInProcess, "Must currently be an in-process frame");
  MOZ_ASSERT(!mIsDiscarded, "We're already closed?");

  mIsInProcess = false;
  mUserGestureStart = TimeStamp();

  // NOTE: For now, clear our nsDocShell reference, as we're primarily in a
  // different process now. This may need to change in the future with
  // Cross-Process BFCache.
  mDocShell = nullptr;
  if (mChildSessionHistory) {
    // This can be removed once session history is stored exclusively in the
    // parent process.
    mChildSessionHistory->SetIsInProcess(false);
  }

  if (!mWindowProxy) {
    return;
  }

  // We have to go through mWindowProxy rather than calling GetDOMWindow() on
  // mDocShell because the mDocshell reference gets cleared immediately after
  // the window is closed.
  nsGlobalWindowOuter::PrepareForProcessChange(mWindowProxy);
  MOZ_ASSERT(!mWindowProxy);
}

bool BrowsingContext::IsTargetable() const {
  return !GetClosed() && AncestorsAreCurrent();
}

bool BrowsingContext::HasOpener() const {
  return sBrowsingContexts->Contains(GetOpenerId());
}

bool BrowsingContext::AncestorsAreCurrent() const {
  const BrowsingContext* bc = this;
  while (true) {
    if (bc->IsDiscarded()) {
      return false;
    }

    if (WindowContext* wc = bc->GetParentWindowContext()) {
      if (!wc->IsCurrent() || wc->IsDiscarded()) {
        return false;
      }

      bc = wc->GetBrowsingContext();
    } else {
      return true;
    }
  }
}

bool BrowsingContext::IsInBFCache() const {
  if (mozilla::SessionHistoryInParent()) {
    return mIsInBFCache;
  }
  return mParentWindow &&
         mParentWindow->TopWindowContext()->GetWindowStateSaved();
}

Span<RefPtr<BrowsingContext>> BrowsingContext::Children() const {
  if (WindowContext* current = mCurrentWindowContext) {
    return current->Children();
  }
  return Span<RefPtr<BrowsingContext>>();
}

void BrowsingContext::GetChildren(
    nsTArray<RefPtr<BrowsingContext>>& aChildren) {
  aChildren.AppendElements(Children());
}

void BrowsingContext::GetWindowContexts(
    nsTArray<RefPtr<WindowContext>>& aWindows) {
  aWindows.AppendElements(mWindowContexts);
}

void BrowsingContext::RegisterWindowContext(WindowContext* aWindow) {
  MOZ_ASSERT(!mWindowContexts.Contains(aWindow),
             "WindowContext already registered!");
  MOZ_ASSERT(aWindow->GetBrowsingContext() == this);

  mWindowContexts.AppendElement(aWindow);

  // If the newly registered WindowContext is for our current inner window ID,
  // re-run the `DidSet` handler to re-establish the relationship.
  if (aWindow->InnerWindowId() == GetCurrentInnerWindowId()) {
    DidSet(FieldIndex<IDX_CurrentInnerWindowId>());
    MOZ_DIAGNOSTIC_ASSERT(mCurrentWindowContext == aWindow);
  }
}

void BrowsingContext::UnregisterWindowContext(WindowContext* aWindow) {
  MOZ_ASSERT(mWindowContexts.Contains(aWindow),
             "WindowContext not registered!");
  mWindowContexts.RemoveElement(aWindow);

  // If our currently active window was unregistered, clear our reference to it.
  if (aWindow == mCurrentWindowContext) {
    // Re-read our `CurrentInnerWindowId` value and use it to set
    // `mCurrentWindowContext`. As `aWindow` is now unregistered and discarded,
    // we won't find it, and the value will be cleared back to `nullptr`.
    DidSet(FieldIndex<IDX_CurrentInnerWindowId>());
    MOZ_DIAGNOSTIC_ASSERT(mCurrentWindowContext == nullptr);
  }
}

void BrowsingContext::PreOrderWalkVoid(
    const std::function<void(BrowsingContext*)>& aCallback) {
  aCallback(this);

  AutoTArray<RefPtr<BrowsingContext>, 8> children;
  children.AppendElements(Children());

  for (auto& child : children) {
    child->PreOrderWalkVoid(aCallback);
  }
}

BrowsingContext::WalkFlag BrowsingContext::PreOrderWalkFlag(
    const std::function<WalkFlag(BrowsingContext*)>& aCallback) {
  switch (aCallback(this)) {
    case WalkFlag::Skip:
      return WalkFlag::Next;
    case WalkFlag::Stop:
      return WalkFlag::Stop;
    case WalkFlag::Next:
    default:
      break;
  }

  AutoTArray<RefPtr<BrowsingContext>, 8> children;
  children.AppendElements(Children());

  for (auto& child : children) {
    switch (child->PreOrderWalkFlag(aCallback)) {
      case WalkFlag::Stop:
        return WalkFlag::Stop;
      default:
        break;
    }
  }

  return WalkFlag::Next;
}

void BrowsingContext::PostOrderWalk(
    const std::function<void(BrowsingContext*)>& aCallback) {
  AutoTArray<RefPtr<BrowsingContext>, 8> children;
  children.AppendElements(Children());

  for (auto& child : children) {
    child->PostOrderWalk(aCallback);
  }

  aCallback(this);
}

void BrowsingContext::GetAllBrowsingContextsInSubtree(
    nsTArray<RefPtr<BrowsingContext>>& aBrowsingContexts) {
  PreOrderWalk([&](BrowsingContext* aContext) {
    aBrowsingContexts.AppendElement(aContext);
  });
}

// FindWithName follows the rules for choosing a browsing context,
// with the exception of sandboxing for iframes. The implementation
// for arbitrarily choosing between two browsing contexts with the
// same name is as follows:
//
// 1) The start browsing context, i.e. 'this'
// 2) Descendants in insertion order
// 3) The parent
// 4) Siblings and their children, both in insertion order
// 5) After this we iteratively follow the parent chain, repeating 3
//    and 4 until
// 6) If there is no parent, consider all other top level browsing
//    contexts and their children, both in insertion order
//
// See
// https://html.spec.whatwg.org/multipage/browsers.html#the-rules-for-choosing-a-browsing-context-given-a-browsing-context-name
BrowsingContext* BrowsingContext::FindWithName(
    const nsAString& aName, bool aUseEntryGlobalForAccessCheck) {
  RefPtr<BrowsingContext> requestingContext = this;
  if (aUseEntryGlobalForAccessCheck) {
    if (nsCOMPtr<nsIDocShell> caller = do_GetInterface(GetEntryGlobal())) {
      if (caller->GetBrowsingContext()) {
        requestingContext = caller->GetBrowsingContext();
      }
    }
  }

  BrowsingContext* found = nullptr;
  if (aName.IsEmpty()) {
    // You can't find a browsing context with an empty name.
    found = nullptr;
  } else if (aName.LowerCaseEqualsLiteral("_blank")) {
    // Just return null. Caller must handle creating a new window with
    // a blank name.
    found = nullptr;
  } else if (nsContentUtils::IsSpecialName(aName)) {
    found = FindWithSpecialName(aName, *requestingContext);
  } else if (BrowsingContext* child =
                 FindWithNameInSubtree(aName, *requestingContext)) {
    found = child;
  } else {
    BrowsingContext* current = this;

    do {
      Span<RefPtr<BrowsingContext>> siblings;
      BrowsingContext* parent = current->GetParent();

      if (!parent) {
        // We've reached the root of the tree, consider browsing
        // contexts in the same browsing context group.
        siblings = mGroup->Toplevels();
      } else if (parent->NameEquals(aName) &&
                 requestingContext->CanAccess(parent) &&
                 parent->IsTargetable()) {
        found = parent;
        break;
      } else {
        siblings = parent->Children();
      }

      for (BrowsingContext* sibling : siblings) {
        if (sibling == current) {
          continue;
        }

        if (BrowsingContext* relative =
                sibling->FindWithNameInSubtree(aName, *requestingContext)) {
          found = relative;
          // Breaks the outer loop
          parent = nullptr;
          break;
        }
      }

      current = parent;
    } while (current);
  }

  // Helpers should perform access control checks, which means that we
  // only need to assert that we can access found.
  MOZ_DIAGNOSTIC_ASSERT(!found || requestingContext->CanAccess(found));

  return found;
}

BrowsingContext* BrowsingContext::FindChildWithName(
    const nsAString& aName, BrowsingContext& aRequestingContext) {
  if (aName.IsEmpty()) {
    // You can't find a browsing context with the empty name.
    return nullptr;
  }

  for (BrowsingContext* child : Children()) {
    if (child->NameEquals(aName) && aRequestingContext.CanAccess(child) &&
        child->IsTargetable()) {
      return child;
    }
  }

  return nullptr;
}

BrowsingContext* BrowsingContext::FindWithSpecialName(
    const nsAString& aName, BrowsingContext& aRequestingContext) {
  // TODO(farre): Neither BrowsingContext nor nsDocShell checks if the
  // browsing context pointed to by a special name is active. Should
  // it be? See Bug 1527913.
  if (aName.LowerCaseEqualsLiteral("_self")) {
    return this;
  }

  if (aName.LowerCaseEqualsLiteral("_parent")) {
    if (BrowsingContext* parent = GetParent()) {
      return aRequestingContext.CanAccess(parent) ? parent : nullptr;
    }
    return this;
  }

  if (aName.LowerCaseEqualsLiteral("_top")) {
    BrowsingContext* top = Top();

    return aRequestingContext.CanAccess(top) ? top : nullptr;
  }

  return nullptr;
}

BrowsingContext* BrowsingContext::FindWithNameInSubtree(
    const nsAString& aName, BrowsingContext& aRequestingContext) {
  MOZ_DIAGNOSTIC_ASSERT(!aName.IsEmpty());

  if (NameEquals(aName) && aRequestingContext.CanAccess(this) &&
      IsTargetable()) {
    return this;
  }

  for (BrowsingContext* child : Children()) {
    if (BrowsingContext* found =
            child->FindWithNameInSubtree(aName, aRequestingContext)) {
      return found;
    }
  }

  return nullptr;
}

// For historical context, see:
//
// Bug 13871:   Prevent frameset spoofing
// Bug 103638:  Targets with same name in different windows open in wrong
//              window with javascript
// Bug 408052:  Adopt "ancestor" frame navigation policy
// Bug 1570207: Refactor logic to rely on BrowsingContextGroups to enforce
//              origin attribute isolation.
bool BrowsingContext::CanAccess(BrowsingContext* aTarget,
                                bool aConsiderOpener) {
  MOZ_ASSERT(
      mDocShell,
      "CanAccess() may only be called in the process of the accessing window");
  MOZ_ASSERT(aTarget, "Must have a target");

  MOZ_DIAGNOSTIC_ASSERT(
      Group() == aTarget->Group(),
      "A BrowsingContext should never see a context from a different group");

  // A frame can navigate itself and its own root.
  if (aTarget == this || aTarget == Top()) {
    return true;
  }

  // A frame can navigate any frame with a same-origin ancestor.
  for (BrowsingContext* bc = aTarget; bc; bc = bc->GetParent()) {
    if (bc->mDocShell && nsDocShell::ValidateOrigin(this, bc)) {
      return true;
    }
  }

  // If the target is a top-level document, a frame can navigate it if it can
  // navigate its opener.
  if (aConsiderOpener && !aTarget->GetParent()) {
    if (RefPtr<BrowsingContext> opener = aTarget->GetOpener()) {
      return CanAccess(opener, false);
    }
  }

  return false;
}

bool BrowsingContext::IsSandboxedFrom(BrowsingContext* aTarget) {
  // If no target then not sandboxed.
  if (!aTarget) {
    return false;
  }

  // We cannot be sandboxed from ourselves.
  if (aTarget == this) {
    return false;
  }

  // Default the sandbox flags to our flags, so that if we can't retrieve the
  // active document, we will still enforce our own.
  uint32_t sandboxFlags = GetSandboxFlags();
  if (mDocShell) {
    if (RefPtr<Document> doc = mDocShell->GetExtantDocument()) {
      sandboxFlags = doc->GetSandboxFlags();
    }
  }

  // If no flags, we are not sandboxed at all.
  if (!sandboxFlags) {
    return false;
  }

  // If aTarget has an ancestor, it is not top level.
  if (RefPtr<BrowsingContext> ancestorOfTarget = aTarget->GetParent()) {
    do {
      // We are not sandboxed if we are an ancestor of target.
      if (ancestorOfTarget == this) {
        return false;
      }
      ancestorOfTarget = ancestorOfTarget->GetParent();
    } while (ancestorOfTarget);

    // Otherwise, we are sandboxed from aTarget.
    return true;
  }

  // aTarget is top level, are we the "one permitted sandboxed
  // navigator", i.e. did we open aTarget?
  if (aTarget->GetOnePermittedSandboxedNavigatorId() == Id()) {
    return false;
  }

  // If SANDBOXED_TOPLEVEL_NAVIGATION flag is not on, we are not sandboxed
  // from our top.
  if (!(sandboxFlags & SANDBOXED_TOPLEVEL_NAVIGATION) && aTarget == Top()) {
    return false;
  }

  // If SANDBOXED_TOPLEVEL_NAVIGATION_USER_ACTIVATION flag is not on, we are not
  // sandboxed from our top if we have user interaction. We assume there is a
  // valid transient user gesture interaction if this check happens in the
  // target process given that we have checked in the triggering process
  // already.
  if (!(sandboxFlags & SANDBOXED_TOPLEVEL_NAVIGATION_USER_ACTIVATION) &&
      mCurrentWindowContext &&
      (!mCurrentWindowContext->IsInProcess() ||
       mCurrentWindowContext->HasValidTransientUserGestureActivation()) &&
      aTarget == Top()) {
    return false;
  }

  // Otherwise, we are sandboxed from aTarget.
  return true;
}

RefPtr<SessionStorageManager> BrowsingContext::GetSessionStorageManager() {
  RefPtr<SessionStorageManager>& manager = Top()->mSessionStorageManager;
  if (!manager) {
    manager = new SessionStorageManager(this);
  }
  return manager;
}

bool BrowsingContext::CrossOriginIsolated() {
  MOZ_ASSERT(NS_IsMainThread());

  return StaticPrefs::
             dom_postMessage_sharedArrayBuffer_withCOOP_COEP_AtStartup() &&
         Top()->GetOpenerPolicy() ==
             nsILoadInfo::
                 OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP &&
         XRE_IsContentProcess() &&
         StringBeginsWith(ContentChild::GetSingleton()->GetRemoteType(),
                          WITH_COOP_COEP_REMOTE_TYPE_PREFIX);
}

void BrowsingContext::SetTriggeringAndInheritPrincipals(
    nsIPrincipal* aTriggeringPrincipal, nsIPrincipal* aPrincipalToInherit,
    uint64_t aLoadIdentifier) {
  mTriggeringPrincipal = Some(
      PrincipalWithLoadIdentifierTuple(aTriggeringPrincipal, aLoadIdentifier));
  if (aPrincipalToInherit) {
    mPrincipalToInherit = Some(
        PrincipalWithLoadIdentifierTuple(aPrincipalToInherit, aLoadIdentifier));
  }
}

Tuple<nsCOMPtr<nsIPrincipal>, nsCOMPtr<nsIPrincipal>>
BrowsingContext::GetTriggeringAndInheritPrincipalsForCurrentLoad() {
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
      GetSavedPrincipal(mTriggeringPrincipal);
  nsCOMPtr<nsIPrincipal> principalToInherit =
      GetSavedPrincipal(mPrincipalToInherit);
  return MakeTuple(triggeringPrincipal, principalToInherit);
}

nsIPrincipal* BrowsingContext::GetSavedPrincipal(
    Maybe<PrincipalWithLoadIdentifierTuple> aPrincipalTuple) {
  if (aPrincipalTuple) {
    nsCOMPtr<nsIPrincipal> principal;
    uint64_t loadIdentifier;
    Tie(principal, loadIdentifier) = *aPrincipalTuple;
    // We want to return a principal only if the load identifier for it
    // matches the current one for this BC.
    if (auto current = GetCurrentLoadIdentifier();
        current && *current == loadIdentifier) {
      return principal;
    }
  }
  return nullptr;
}

BrowsingContext::~BrowsingContext() {
  MOZ_DIAGNOSTIC_ASSERT(!mParentWindow ||
                        !mParentWindow->mChildren.Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!mGroup || !mGroup->Toplevels().Contains(this));

  mDeprioritizedLoadRunner.clear();

  if (sBrowsingContexts) {
    sBrowsingContexts->Remove(Id());
  }
  UnregisterBrowserId(this);
}

/* static */
void BrowsingContext::DiscardFromContentParent(ContentParent* aCP) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (sBrowsingContexts) {
    AutoTArray<RefPtr<BrowsingContext>, 8> toDiscard;
    for (const auto& data : sBrowsingContexts->Values()) {
      auto* bc = data->Canonical();
      if (!bc->IsDiscarded() && bc->IsEmbeddedInProcess(aCP->ChildID())) {
        toDiscard.AppendElement(bc);
      }
    }

    for (BrowsingContext* bc : toDiscard) {
      bc->Detach(/* aFromIPC */ true);
    }
  }
}

nsISupports* BrowsingContext::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContext::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

bool BrowsingContext::WriteStructuredClone(JSContext* aCx,
                                           JSStructuredCloneWriter* aWriter,
                                           StructuredCloneHolder* aHolder) {
  MOZ_DIAGNOSTIC_ASSERT(mEverAttached);
  return (JS_WriteUint32Pair(aWriter, SCTAG_DOM_BROWSING_CONTEXT, 0) &&
          JS_WriteUint32Pair(aWriter, uint32_t(Id()), uint32_t(Id() >> 32)));
}

/* static */
JSObject* BrowsingContext::ReadStructuredClone(JSContext* aCx,
                                               JSStructuredCloneReader* aReader,
                                               StructuredCloneHolder* aHolder) {
  uint32_t idLow = 0;
  uint32_t idHigh = 0;
  if (!JS_ReadUint32Pair(aReader, &idLow, &idHigh)) {
    return nullptr;
  }
  uint64_t id = uint64_t(idHigh) << 32 | idLow;

  // Note: Do this check after reading our ID data. Returning null will abort
  // the decode operation anyway, but we should at least be as safe as possible.
  if (NS_WARN_IF(!NS_IsMainThread())) {
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "We shouldn't be trying to decode a BrowsingContext "
                          "on a background thread.");
    return nullptr;
  }

  JS::RootedValue val(aCx, JS::NullValue());
  // We'll get rooting hazard errors from the RefPtr destructor if it isn't
  // destroyed before we try to return a raw JSObject*, so create it in its own
  // scope.
  if (RefPtr<BrowsingContext> context = Get(id)) {
    if (!GetOrCreateDOMReflector(aCx, context, &val) || !val.isObject()) {
      return nullptr;
    }
  }
  return val.toObjectOrNull();
}

bool BrowsingContext::CanSetOriginAttributes() {
  // A discarded BrowsingContext has already been destroyed, and cannot modify
  // its OriginAttributes.
  if (NS_WARN_IF(IsDiscarded())) {
    return false;
  }

  // Before attaching is the safest time to set OriginAttributes, and the only
  // allowed time for content BrowsingContexts.
  if (!EverAttached()) {
    return true;
  }

  // Attached content BrowsingContexts may have been synced to other processes.
  if (NS_WARN_IF(IsContent())) {
    MOZ_CRASH();
    return false;
  }
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  // Cannot set OriginAttributes after we've created our child BrowsingContext.
  if (NS_WARN_IF(!Children().IsEmpty())) {
    return false;
  }

  // Only allow setting OriginAttributes if we have no associated document, or
  // the document is still `about:blank`.
  // TODO: Bug 1273058 - should have no document when setting origin attributes.
  if (WindowGlobalParent* window = Canonical()->GetCurrentWindowGlobal()) {
    if (nsIURI* uri = window->GetDocumentURI()) {
      MOZ_ASSERT(NS_IsAboutBlank(uri));
      return NS_IsAboutBlank(uri);
    }
  }
  return true;
}

Nullable<WindowProxyHolder> BrowsingContext::GetAssociatedWindow() {
  // nsILoadContext usually only returns same-process windows,
  // so we intentionally return nullptr if this BC is out of
  // process.
  if (IsInProcess()) {
    return WindowProxyHolder(this);
  }
  return nullptr;
}

Nullable<WindowProxyHolder> BrowsingContext::GetTopWindow() {
  return Top()->GetAssociatedWindow();
}

Element* BrowsingContext::GetTopFrameElement() {
  return Top()->GetEmbedderElement();
}

void BrowsingContext::SetUsePrivateBrowsing(bool aUsePrivateBrowsing,
                                            ErrorResult& aError) {
  nsresult rv = SetUsePrivateBrowsing(aUsePrivateBrowsing);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

void BrowsingContext::SetUseTrackingProtectionWebIDL(
    bool aUseTrackingProtection, ErrorResult& aRv) {
  SetForceEnableTrackingProtection(aUseTrackingProtection, aRv);
}

void BrowsingContext::GetOriginAttributes(JSContext* aCx,
                                          JS::MutableHandle<JS::Value> aVal,
                                          ErrorResult& aError) {
  AssertOriginAttributesMatchPrivateBrowsing();

  if (!ToJSValue(aCx, mOriginAttributes, aVal)) {
    aError.NoteJSContextException(aCx);
  }
}

NS_IMETHODIMP BrowsingContext::GetAssociatedWindow(
    mozIDOMWindowProxy** aAssociatedWindow) {
  nsCOMPtr<mozIDOMWindowProxy> win = GetDOMWindow();
  win.forget(aAssociatedWindow);
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::GetTopWindow(mozIDOMWindowProxy** aTopWindow) {
  return Top()->GetAssociatedWindow(aTopWindow);
}

NS_IMETHODIMP BrowsingContext::GetTopFrameElement(Element** aTopFrameElement) {
  RefPtr<Element> topFrameElement = GetTopFrameElement();
  topFrameElement.forget(aTopFrameElement);
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::GetIsContent(bool* aIsContent) {
  *aIsContent = IsContent();
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::GetUsePrivateBrowsing(
    bool* aUsePrivateBrowsing) {
  *aUsePrivateBrowsing = mPrivateBrowsingId > 0;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::SetUsePrivateBrowsing(bool aUsePrivateBrowsing) {
  if (!CanSetOriginAttributes()) {
    bool changed = aUsePrivateBrowsing != (mPrivateBrowsingId > 0);
    if (changed) {
      NS_WARNING("SetUsePrivateBrowsing when !CanSetOriginAttributes()");
    }
    return changed ? NS_ERROR_FAILURE : NS_OK;
  }

  return SetPrivateBrowsing(aUsePrivateBrowsing);
}

NS_IMETHODIMP BrowsingContext::SetPrivateBrowsing(bool aPrivateBrowsing) {
  if (!CanSetOriginAttributes()) {
    NS_WARNING("Attempt to set PrivateBrowsing when !CanSetOriginAttributes");
    return NS_ERROR_FAILURE;
  }

  bool changed = aPrivateBrowsing != (mPrivateBrowsingId > 0);
  if (changed) {
    mPrivateBrowsingId = aPrivateBrowsing ? 1 : 0;
    if (IsContent()) {
      mOriginAttributes.SyncAttributesWithPrivateBrowsing(aPrivateBrowsing);
    }

    if (XRE_IsParentProcess()) {
      Canonical()->AdjustPrivateBrowsingCount(aPrivateBrowsing);
    }
  }
  AssertOriginAttributesMatchPrivateBrowsing();

  if (changed && mDocShell) {
    nsDocShell::Cast(mDocShell)->NotifyPrivateBrowsingChanged();
  }
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::GetUseRemoteTabs(bool* aUseRemoteTabs) {
  *aUseRemoteTabs = mUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::SetRemoteTabs(bool aUseRemoteTabs) {
  if (!CanSetOriginAttributes()) {
    NS_WARNING("Attempt to set RemoteTabs when !CanSetOriginAttributes");
    return NS_ERROR_FAILURE;
  }

  static bool annotated = false;
  if (aUseRemoteTabs && !annotated) {
    annotated = true;
    CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::DOMIPCEnabled,
                                       true);
  }

  // Don't allow non-remote tabs with remote subframes.
  if (NS_WARN_IF(!aUseRemoteTabs && mUseRemoteSubframes)) {
    return NS_ERROR_UNEXPECTED;
  }

  mUseRemoteTabs = aUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::GetUseRemoteSubframes(
    bool* aUseRemoteSubframes) {
  *aUseRemoteSubframes = mUseRemoteSubframes;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::SetRemoteSubframes(bool aUseRemoteSubframes) {
  if (!CanSetOriginAttributes()) {
    NS_WARNING("Attempt to set RemoteSubframes when !CanSetOriginAttributes");
    return NS_ERROR_FAILURE;
  }

  static bool annotated = false;
  if (aUseRemoteSubframes && !annotated) {
    annotated = true;
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::DOMFissionEnabled, true);
  }

  // Don't allow non-remote tabs with remote subframes.
  if (NS_WARN_IF(aUseRemoteSubframes && !mUseRemoteTabs)) {
    return NS_ERROR_UNEXPECTED;
  }

  mUseRemoteSubframes = aUseRemoteSubframes;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::GetUseTrackingProtection(
    bool* aUseTrackingProtection) {
  *aUseTrackingProtection = false;

  if (GetForceEnableTrackingProtection() ||
      StaticPrefs::privacy_trackingprotection_enabled() ||
      (UsePrivateBrowsing() &&
       StaticPrefs::privacy_trackingprotection_pbmode_enabled())) {
    *aUseTrackingProtection = true;
    return NS_OK;
  }

  if (GetParent()) {
    return GetParent()->GetUseTrackingProtection(aUseTrackingProtection);
  }

  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::SetUseTrackingProtection(
    bool aUseTrackingProtection) {
  return SetForceEnableTrackingProtection(aUseTrackingProtection);
}

NS_IMETHODIMP BrowsingContext::GetScriptableOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aVal) {
  AssertOriginAttributesMatchPrivateBrowsing();

  bool ok = ToJSValue(aCx, mOriginAttributes, aVal);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP_(void)
BrowsingContext::GetOriginAttributes(OriginAttributes& aAttrs) {
  aAttrs = mOriginAttributes;
  AssertOriginAttributesMatchPrivateBrowsing();
}

nsresult BrowsingContext::SetOriginAttributes(const OriginAttributes& aAttrs) {
  if (!CanSetOriginAttributes()) {
    NS_WARNING("Attempt to set OriginAttributes when !CanSetOriginAttributes");
    return NS_ERROR_FAILURE;
  }

  AssertOriginAttributesMatchPrivateBrowsing();
  mOriginAttributes = aAttrs;

  bool isPrivate = mOriginAttributes.mPrivateBrowsingId !=
                   nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID;
  // Chrome Browsing Context can not contain OriginAttributes.mPrivateBrowsingId
  if (IsChrome() && isPrivate) {
    mOriginAttributes.mPrivateBrowsingId =
        nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID;
  }
  SetPrivateBrowsing(isPrivate);
  AssertOriginAttributesMatchPrivateBrowsing();

  return NS_OK;
}

void BrowsingContext::AssertCoherentLoadContext() {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // LoadContext should generally match our opener or parent.
  if (IsContent()) {
    if (RefPtr<BrowsingContext> opener = GetOpener()) {
      MOZ_DIAGNOSTIC_ASSERT(opener->mType == mType);
      MOZ_DIAGNOSTIC_ASSERT(opener->mGroup == mGroup);
      MOZ_DIAGNOSTIC_ASSERT(opener->mUseRemoteTabs == mUseRemoteTabs);
      MOZ_DIAGNOSTIC_ASSERT(opener->mUseRemoteSubframes == mUseRemoteSubframes);
      MOZ_DIAGNOSTIC_ASSERT(opener->mPrivateBrowsingId == mPrivateBrowsingId);
      MOZ_DIAGNOSTIC_ASSERT(
          opener->mOriginAttributes.EqualsIgnoringFPD(mOriginAttributes));
    }
  }
  if (RefPtr<BrowsingContext> parent = GetParent()) {
    MOZ_DIAGNOSTIC_ASSERT(parent->mType == mType);
    MOZ_DIAGNOSTIC_ASSERT(parent->mGroup == mGroup);
    MOZ_DIAGNOSTIC_ASSERT(parent->mUseRemoteTabs == mUseRemoteTabs);
    MOZ_DIAGNOSTIC_ASSERT(parent->mUseRemoteSubframes == mUseRemoteSubframes);
    MOZ_DIAGNOSTIC_ASSERT(parent->mPrivateBrowsingId == mPrivateBrowsingId);
    MOZ_DIAGNOSTIC_ASSERT(
        parent->mOriginAttributes.EqualsIgnoringFPD(mOriginAttributes));
  }

  // UseRemoteSubframes and UseRemoteTabs must match.
  MOZ_DIAGNOSTIC_ASSERT(
      !mUseRemoteSubframes || mUseRemoteTabs,
      "Cannot set useRemoteSubframes without also setting useRemoteTabs");

  // Double-check OriginAttributes/Private Browsing
  AssertOriginAttributesMatchPrivateBrowsing();
#endif
}

void BrowsingContext::AssertOriginAttributesMatchPrivateBrowsing() {
  // Chrome browsing contexts must not have a private browsing OriginAttribute
  // Content browsing contexts must maintain the equality:
  // mOriginAttributes.mPrivateBrowsingId == mPrivateBrowsingId
  if (IsChrome()) {
    MOZ_DIAGNOSTIC_ASSERT(mOriginAttributes.mPrivateBrowsingId == 0);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(mOriginAttributes.mPrivateBrowsingId ==
                          mPrivateBrowsingId);
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowsingContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsILoadContext)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowsingContext)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowsingContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowsingContext)
  if (sBrowsingContexts) {
    sBrowsingContexts->Remove(tmp->Id());
  }
  UnregisterBrowserId(tmp);

  if (tmp->GetIsPopupSpam()) {
    PopupBlocker::UnregisterOpenPopupSpam();
    // NOTE: Doesn't use SetIsPopupSpam, as it will be set all processes
    // automatically.
    tmp->mFields.SetWithoutSyncing<IDX_IsPopupSpam>(false);
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(
      mDocShell, mParentWindow, mGroup, mEmbedderElement, mWindowContexts,
      mCurrentWindowContext, mSessionStorageManager, mChildSessionHistory)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(
      mDocShell, mParentWindow, mGroup, mEmbedderElement, mWindowContexts,
      mCurrentWindowContext, mSessionStorageManager, mChildSessionHistory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

static bool IsCertainlyAliveForCC(BrowsingContext* aContext) {
  return aContext->HasKnownLiveWrapper() ||
         (AppShutdown::GetCurrentShutdownPhase() ==
              ShutdownPhase::NotInShutdown &&
          aContext->EverAttached() && !aContext->IsDiscarded());
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(BrowsingContext)
  if (IsCertainlyAliveForCC(tmp)) {
    if (tmp->PreservingWrapper()) {
      tmp->MarkWrapperLive();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(BrowsingContext)
  return IsCertainlyAliveForCC(tmp) && tmp->HasNothingToTrace(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(BrowsingContext)
  return IsCertainlyAliveForCC(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

class RemoteLocationProxy
    : public RemoteObjectProxy<BrowsingContext::LocationProxy,
                               Location_Binding::sCrossOriginProperties> {
 public:
  typedef RemoteObjectProxy Base;

  constexpr RemoteLocationProxy()
      : RemoteObjectProxy(prototypes::id::Location) {}

  void NoteChildren(JSObject* aProxy,
                    nsCycleCollectionTraversalCallback& aCb) const override {
    auto location =
        static_cast<BrowsingContext::LocationProxy*>(GetNative(aProxy));
    CycleCollectionNoteChild(aCb, location->GetBrowsingContext(),
                             "JS::GetPrivate(obj)->GetBrowsingContext()");
  }
};

static const RemoteLocationProxy sSingleton;

// Give RemoteLocationProxy 2 reserved slots, like the other wrappers,
// so JSObject::swap can swap it with CrossCompartmentWrappers without requiring
// malloc.
template <>
const JSClass RemoteLocationProxy::Base::sClass =
    PROXY_CLASS_DEF("Proxy", JSCLASS_HAS_RESERVED_SLOTS(2));

void BrowsingContext::Location(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aLocation,
                               ErrorResult& aError) {
  aError.MightThrowJSException();
  sSingleton.GetProxyObject(aCx, &mLocation, /* aTransplantTo = */ nullptr,
                            aLocation);
  if (!aLocation) {
    aError.StealExceptionFromJSContext(aCx);
  }
}

bool BrowsingContext::RemoveRootFromBFCacheSync() {
  if (WindowContext* wc = GetParentWindowContext()) {
    if (RefPtr<Document> doc = wc->TopWindowContext()->GetDocument()) {
      return doc->RemoveFromBFCacheSync();
    }
  }
  return false;
}

nsresult BrowsingContext::CheckSandboxFlags(nsDocShellLoadState* aLoadState) {
  const auto& sourceBC = aLoadState->SourceBrowsingContext();
  if (sourceBC.IsNull()) {
    return NS_OK;
  }

  // We might be called after the source BC has been discarded, but before we've
  // destroyed our in-process instance of the BrowsingContext object in some
  // situations (e.g. after creating a new pop-up with window.open while the
  // window is being closed). In these situations we want to still perform the
  // sandboxing check against our in-process copy. If we've forgotten about the
  // context already, assume it is sanboxed. (bug 1643450)
  BrowsingContext* bc = sourceBC.GetMaybeDiscarded();
  if (!bc || bc->IsSandboxedFrom(this)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  return NS_OK;
}

nsresult BrowsingContext::LoadURI(nsDocShellLoadState* aLoadState,
                                  bool aSetNavigating) {
  // Per spec, most load attempts are silently ignored when a BrowsingContext is
  // null (which in our code corresponds to discarded), so we simply fail
  // silently in those cases. Regardless, we cannot trigger loads in/from
  // discarded BrowsingContexts via IPC, so we need to abort in any case.
  if (IsDiscarded()) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(aLoadState->TargetBrowsingContext().IsNull(),
                        "Targeting occurs in InternalLoad");

  if (mDocShell) {
    return mDocShell->LoadURI(aLoadState, aSetNavigating);
  }

  // Note: We do this check both here and in `nsDocShell::InternalLoad`, since
  // document-specific sandbox flags are only available in the process
  // triggering the load, and we don't want the target process to have to trust
  // the triggering process to do the appropriate checks for the
  // BrowsingContext's sandbox flags.
  MOZ_TRY(CheckSandboxFlags(aLoadState));
  SetTriggeringAndInheritPrincipals(aLoadState->TriggeringPrincipal(),
                                    aLoadState->PrincipalToInherit(),
                                    aLoadState->GetLoadIdentifier());

  const auto& sourceBC = aLoadState->SourceBrowsingContext();

  if (net::SchemeIsJavascript(aLoadState->URI())) {
    if (!XRE_IsParentProcess()) {
      // Web content should only be able to load javascript: URIs into documents
      // whose principals the caller principal subsumes, which by definition
      // excludes any document in a cross-process BrowsingContext.
      return NS_ERROR_DOM_BAD_CROSS_ORIGIN_URI;
    }
    MOZ_DIAGNOSTIC_ASSERT(!sourceBC,
                          "Should never see a cross-process javascript: load "
                          "triggered from content");
  }

  MOZ_DIAGNOSTIC_ASSERT(!sourceBC || sourceBC->Group() == Group());
  if (sourceBC && sourceBC->IsInProcess()) {
    if (!sourceBC->CanAccess(this)) {
      return NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win(sourceBC->GetDOMWindow());
    if (WindowGlobalChild* wgc =
            win->GetCurrentInnerWindow()->GetWindowGlobalChild()) {
      wgc->SendLoadURI(this, aLoadState, aSetNavigating);
    }
  } else if (XRE_IsParentProcess()) {
    // Strip the target query parameters before loading the URI in the parent.
    // The loading in content will be handled in nsDocShell.
    aLoadState->MaybeStripTrackerQueryStrings(this);

    if (Canonical()->LoadInParent(aLoadState, aSetNavigating)) {
      return NS_OK;
    }

    if (ContentParent* cp = Canonical()->GetContentParent()) {
      // Attempt to initiate this load immediately in the parent, if it succeeds
      // it'll return a unique identifier so that we can find it later.
      uint64_t loadIdentifier = 0;
      if (Canonical()->AttemptSpeculativeLoadInParent(aLoadState)) {
        MOZ_DIAGNOSTIC_ASSERT(GetCurrentLoadIdentifier().isSome());
        loadIdentifier = GetCurrentLoadIdentifier().value();
        aLoadState->SetChannelInitialized(true);
      }

      cp->TransmitBlobDataIfBlobURL(aLoadState->URI());

      // Setup a confirmation callback once the content process receives this
      // load. Normally we'd expect a PDocumentChannel actor to have been
      // created to claim the load identifier by that time. If not, then it
      // won't be coming, so make sure we clean up and deregister.
      cp->SendLoadURI(this, aLoadState, aSetNavigating)
          ->Then(GetMainThreadSerialEventTarget(), __func__,
                 [loadIdentifier](
                     const PContentParent::LoadURIPromise::ResolveOrRejectValue&
                         aValue) {
                   if (loadIdentifier) {
                     net::DocumentLoadListener::CleanupParentLoadAttempt(
                         loadIdentifier);
                   }
                 });
    }
  } else {
    MOZ_DIAGNOSTIC_ASSERT(sourceBC);
    if (!sourceBC) {
      return NS_ERROR_UNEXPECTED;
    }
    // If we're in a content process and the source BC is no longer in-process,
    // just fail silently.
  }
  return NS_OK;
}

nsresult BrowsingContext::InternalLoad(nsDocShellLoadState* aLoadState) {
  if (IsDiscarded()) {
    return NS_OK;
  }
  SetTriggeringAndInheritPrincipals(aLoadState->TriggeringPrincipal(),
                                    aLoadState->PrincipalToInherit(),
                                    aLoadState->GetLoadIdentifier());

  MOZ_DIAGNOSTIC_ASSERT(aLoadState->Target().IsEmpty(),
                        "should already have retargeted");
  MOZ_DIAGNOSTIC_ASSERT(!aLoadState->TargetBrowsingContext().IsNull(),
                        "should have target bc set");
  MOZ_DIAGNOSTIC_ASSERT(aLoadState->TargetBrowsingContext() == this,
                        "must be targeting this BrowsingContext");

  if (mDocShell) {
    return nsDocShell::Cast(mDocShell)->InternalLoad(aLoadState);
  }

  // Note: We do this check both here and in `nsDocShell::InternalLoad`, since
  // document-specific sandbox flags are only available in the process
  // triggering the load, and we don't want the target process to have to trust
  // the triggering process to do the appropriate checks for the
  // BrowsingContext's sandbox flags.
  MOZ_TRY(CheckSandboxFlags(aLoadState));

  const auto& sourceBC = aLoadState->SourceBrowsingContext();

  if (net::SchemeIsJavascript(aLoadState->URI())) {
    if (!XRE_IsParentProcess()) {
      // Web content should only be able to load javascript: URIs into documents
      // whose principals the caller principal subsumes, which by definition
      // excludes any document in a cross-process BrowsingContext.
      return NS_ERROR_DOM_BAD_CROSS_ORIGIN_URI;
    }
    MOZ_DIAGNOSTIC_ASSERT(!sourceBC,
                          "Should never see a cross-process javascript: load "
                          "triggered from content");
  }

  if (XRE_IsParentProcess()) {
    ContentParent* cp = Canonical()->GetContentParent();
    if (!cp || !cp->CanSend()) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ALWAYS_SUCCEEDS(
        SetCurrentLoadIdentifier(Some(aLoadState->GetLoadIdentifier())));
    Unused << cp->SendInternalLoad(aLoadState);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(sourceBC);
    MOZ_DIAGNOSTIC_ASSERT(sourceBC->Group() == Group());

    if (!sourceBC->CanAccess(this)) {
      return NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win(sourceBC->GetDOMWindow());
    WindowGlobalChild* wgc =
        win->GetCurrentInnerWindow()->GetWindowGlobalChild();
    if (!wgc || !wgc->CanSend()) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ALWAYS_SUCCEEDS(
        SetCurrentLoadIdentifier(Some(aLoadState->GetLoadIdentifier())));
    wgc->SendInternalLoad(aLoadState);
  }

  return NS_OK;
}

void BrowsingContext::DisplayLoadError(const nsAString& aURI) {
  MOZ_LOG(GetLog(), LogLevel::Debug, ("DisplayLoadError"));
  MOZ_DIAGNOSTIC_ASSERT(!IsDiscarded());
  MOZ_DIAGNOSTIC_ASSERT(mDocShell || XRE_IsParentProcess());

  if (mDocShell) {
    bool didDisplayLoadError = false;
    mDocShell->DisplayLoadError(NS_ERROR_MALFORMED_URI, nullptr,
                                PromiseFlatString(aURI).get(), nullptr,
                                &didDisplayLoadError);
  } else {
    if (ContentParent* cp = Canonical()->GetContentParent()) {
      Unused << cp->SendDisplayLoadError(this, PromiseFlatString(aURI));
    }
  }
}

WindowProxyHolder BrowsingContext::Window() {
  return WindowProxyHolder(Self());
}

WindowProxyHolder BrowsingContext::GetFrames(ErrorResult& aError) {
  return Window();
}

void BrowsingContext::Close(CallerType aCallerType, ErrorResult& aError) {
  if (mIsDiscarded) {
    return;
  }

  if (IsSubframe()) {
    // .close() on frames is a no-op.
    return;
  }

  if (GetDOMWindow()) {
    nsGlobalWindowOuter::Cast(GetDOMWindow())
        ->CloseOuter(aCallerType == CallerType::System);
    return;
  }

  // This is a bit of a hack for webcompat. Content needs to see an updated
  // |window.closed| value as early as possible, so we set this before we
  // actually send the DOMWindowClose event, which happens in the process where
  // the document for this browsing context is loaded.
  MOZ_ALWAYS_SUCCEEDS(SetClosed(true));

  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowClose(this, aCallerType == CallerType::System);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowClose(this, aCallerType == CallerType::System);
  }
}

/*
 * Examine the current document state to see if we're in a way that is
 * typically abused by web designers. The window.open code uses this
 * routine to determine whether to allow the new window.
 * Returns a value from the PopupControlState enum.
 */
PopupBlocker::PopupControlState BrowsingContext::RevisePopupAbuseLevel(
    PopupBlocker::PopupControlState aControl) {
  if (!IsContent()) {
    return PopupBlocker::openAllowed;
  }

  RefPtr<Document> doc = GetExtantDocument();
  PopupBlocker::PopupControlState abuse = aControl;
  switch (abuse) {
    case PopupBlocker::openControlled:
    case PopupBlocker::openBlocked:
    case PopupBlocker::openOverridden:
      if (IsPopupAllowed()) {
        abuse = PopupBlocker::PopupControlState(abuse - 1);
      }
      break;
    case PopupBlocker::openAbused:
      if (IsPopupAllowed() ||
          (doc && doc->HasValidTransientUserGestureActivation())) {
        // Skip PopupBlocker::openBlocked
        abuse = PopupBlocker::openControlled;
      }
      break;
    case PopupBlocker::openAllowed:
      break;
    default:
      NS_WARNING("Strange PopupControlState!");
  }

  // limit the number of simultaneously open popups
  if (abuse == PopupBlocker::openAbused || abuse == PopupBlocker::openBlocked ||
      abuse == PopupBlocker::openControlled) {
    int32_t popupMax = StaticPrefs::dom_popup_maximum();
    if (popupMax >= 0 &&
        PopupBlocker::GetOpenPopupSpamCount() >= (uint32_t)popupMax) {
      abuse = PopupBlocker::openOverridden;
    }
  }

  // If we're currently in-process, attempt to consume transient user gesture
  // activations.
  if (doc) {
    // HACK: Some pages using bogus library + UA sniffing call window.open()
    // from a blank iframe, only on Firefox, see bug 1685056.
    //
    // This is a hack-around to preserve behavior in that particular and
    // specific case, by consuming activation on the parent document, so we
    // don't care about the InProcessParent bits not being fission-safe or what
    // not.
    auto ConsumeTransientUserActivationForMultiplePopupBlocking =
        [&]() -> bool {
      if (doc->ConsumeTransientUserGestureActivation()) {
        return true;
      }
      if (!doc->IsInitialDocument()) {
        return false;
      }
      Document* parentDoc = doc->GetInProcessParentDocument();
      if (!parentDoc ||
          !parentDoc->NodePrincipal()->Equals(doc->NodePrincipal())) {
        return false;
      }
      return parentDoc->ConsumeTransientUserGestureActivation();
    };

    // If this popup is allowed, let's block any other for this event, forcing
    // PopupBlocker::openBlocked state.
    if ((abuse == PopupBlocker::openAllowed ||
         abuse == PopupBlocker::openControlled) &&
        StaticPrefs::dom_block_multiple_popups() && !IsPopupAllowed() &&
        !ConsumeTransientUserActivationForMultiplePopupBlocking()) {
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns,
                                      doc, nsContentUtils::eDOM_PROPERTIES,
                                      "MultiplePopupsBlockedNoUserActivation");
      abuse = PopupBlocker::openBlocked;
    }
  }

  return abuse;
}

void BrowsingContext::IncrementHistoryEntryCountForBrowsingContext() {
  Unused << SetHistoryEntryCount(GetHistoryEntryCount() + 1);
}

void BrowsingContext::FlushSessionStore() {
  nsTArray<RefPtr<BrowserChild>> nestedBrowserChilds;

  PreOrderWalk([&](BrowsingContext* aContext) {
    BrowserChild* browserChild = BrowserChild::GetFrom(aContext->GetDocShell());
    if (browserChild && browserChild->GetBrowsingContext() == aContext) {
      nestedBrowserChilds.AppendElement(browserChild);
    }

    if (aContext->CreatedDynamically()) {
      return WalkFlag::Skip;
    }

    WindowContext* windowContext = aContext->GetCurrentWindowContext();
    if (!windowContext) {
      return WalkFlag::Skip;
    }

    WindowGlobalChild* windowChild = windowContext->GetWindowGlobalChild();
    if (!windowChild) {
      return WalkFlag::Next;
    }

    RefPtr<SessionStoreDataCollector> collector =
        windowChild->GetSessionStoreDataCollector();
    if (!collector) {
      return WalkFlag::Next;
    }

    collector->Flush();
    return WalkFlag::Next;
  });

  for (auto& child : nestedBrowserChilds) {
    child->UpdateSessionStore();
  }
}

std::tuple<bool, bool> BrowsingContext::CanFocusCheck(CallerType aCallerType) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return {false, false};
  }

  nsCOMPtr<nsPIDOMWindowInner> caller = do_QueryInterface(GetEntryGlobal());
  BrowsingContext* callerBC = caller ? caller->GetBrowsingContext() : nullptr;
  RefPtr<BrowsingContext> openerBC = GetOpener();
  MOZ_DIAGNOSTIC_ASSERT(!openerBC || openerBC->Group() == Group());

  // Enforce dom.disable_window_flip (for non-chrome), but still allow the
  // window which opened us to raise us at times when popups are allowed
  // (bugs 355482 and 369306).
  bool canFocus = aCallerType == CallerType::System ||
                  !Preferences::GetBool("dom.disable_window_flip", true);
  if (!canFocus && openerBC == callerBC) {
    canFocus =
        (callerBC ? callerBC : this)
            ->RevisePopupAbuseLevel(PopupBlocker::GetPopupControlState()) <
        PopupBlocker::openBlocked;
  }

  bool isActive = false;
  if (XRE_IsParentProcess()) {
    RefPtr<CanonicalBrowsingContext> chromeTop =
        Canonical()->TopCrossChromeBoundary();
    nsCOMPtr<nsPIDOMWindowOuter> activeWindow = fm->GetActiveWindow();
    isActive = (activeWindow == chromeTop->GetDOMWindow());
  } else {
    isActive = (fm->GetActiveBrowsingContext() == Top());
  }

  return {canFocus, isActive};
}

void BrowsingContext::Focus(CallerType aCallerType, ErrorResult& aError) {
  // These checks need to happen before the RequestFrameFocus call, which
  // is why they are done in an untrusted process. If we wanted to enforce
  // these in the parent, we'd need to do the checks there _also_.
  // These should be kept in sync with nsGlobalWindowOuter::FocusOuter.

  auto [canFocus, isActive] = CanFocusCheck(aCallerType);

  if (!(canFocus || isActive)) {
    return;
  }

  // Permission check passed

  if (mEmbedderElement) {
    // Make the activeElement in this process update synchronously.
    nsContentUtils::RequestFrameFocus(*mEmbedderElement, true, aCallerType);
  }
  uint64_t actionId = nsFocusManager::GenerateFocusActionId();
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowFocus(this, aCallerType, actionId);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowFocus(this, aCallerType, actionId);
  }
}

bool BrowsingContext::CanBlurCheck(CallerType aCallerType) {
  // If dom.disable_window_flip == true, then content should not be allowed
  // to do blur (this would allow popunders, bug 369306)
  return aCallerType == CallerType::System ||
         !Preferences::GetBool("dom.disable_window_flip", true);
}

void BrowsingContext::Blur(CallerType aCallerType, ErrorResult& aError) {
  if (!CanBlurCheck(aCallerType)) {
    return;
  }

  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowBlur(this, aCallerType);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowBlur(this, aCallerType);
  }
}

Nullable<WindowProxyHolder> BrowsingContext::GetWindow() {
  if (XRE_IsParentProcess() && !IsInProcess()) {
    return nullptr;
  }
  return WindowProxyHolder(this);
}

Nullable<WindowProxyHolder> BrowsingContext::GetTop(ErrorResult& aError) {
  if (mIsDiscarded) {
    return nullptr;
  }

  // We never return null or throw an error, but the implementation in
  // nsGlobalWindow does and we need to use the same signature.
  return WindowProxyHolder(Top());
}

void BrowsingContext::GetOpener(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aOpener,
                                ErrorResult& aError) const {
  RefPtr<BrowsingContext> opener = GetOpener();
  if (!opener) {
    aOpener.setNull();
    return;
  }

  if (!ToJSValue(aCx, WindowProxyHolder(opener), aOpener)) {
    aError.NoteJSContextException(aCx);
  }
}

// We never throw an error, but the implementation in nsGlobalWindow does and
// we need to use the same signature.
Nullable<WindowProxyHolder> BrowsingContext::GetParent(ErrorResult& aError) {
  if (mIsDiscarded) {
    return nullptr;
  }

  if (GetParent()) {
    return WindowProxyHolder(GetParent());
  }
  return WindowProxyHolder(this);
}

void BrowsingContext::PostMessageMoz(JSContext* aCx,
                                     JS::Handle<JS::Value> aMessage,
                                     const nsAString& aTargetOrigin,
                                     const Sequence<JSObject*>& aTransfer,
                                     nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  if (mIsDiscarded) {
    return;
  }

  RefPtr<BrowsingContext> sourceBc;
  PostMessageData data;
  data.targetOrigin() = aTargetOrigin;
  data.subjectPrincipal() = &aSubjectPrincipal;
  RefPtr<nsGlobalWindowInner> callerInnerWindow;
  nsAutoCString scriptLocation;
  // We don't need to get the caller's agentClusterId since that is used for
  // checking whether it's okay to sharing memory (and it's not allowed to share
  // memory cross processes)
  if (!nsGlobalWindowOuter::GatherPostMessageData(
          aCx, aTargetOrigin, getter_AddRefs(sourceBc), data.origin(),
          getter_AddRefs(data.targetOriginURI()),
          getter_AddRefs(data.callerPrincipal()),
          getter_AddRefs(callerInnerWindow), getter_AddRefs(data.callerURI()),
          /* aCallerAgentClusterId */ nullptr, &scriptLocation, aError)) {
    return;
  }
  if (sourceBc && sourceBc->IsDiscarded()) {
    return;
  }
  data.source() = sourceBc;
  data.isFromPrivateWindow() =
      callerInnerWindow &&
      nsScriptErrorBase::ComputeIsFromPrivateWindow(callerInnerWindow);
  data.innerWindowId() = callerInnerWindow ? callerInnerWindow->WindowID() : 0;
  data.scriptLocation() = scriptLocation;
  JS::Rooted<JS::Value> transferArray(aCx);
  aError = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransfer,
                                                             &transferArray);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  JS::CloneDataPolicy clonePolicy;
  if (callerInnerWindow && callerInnerWindow->IsSharedMemoryAllowed()) {
    clonePolicy.allowSharedMemoryObjects();
  }

  // We will see if the message is required to be in the same process or it can
  // be in the different process after Write().
  ipc::StructuredCloneData message = ipc::StructuredCloneData(
      StructuredCloneHolder::StructuredCloneScope::UnknownDestination,
      StructuredCloneHolder::TransferringSupported);
  message.Write(aCx, aMessage, transferArray, clonePolicy, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  ClonedOrErrorMessageData messageData;
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    // The clone scope gets set when we write the message data based on the
    // requirements of that data that we're writing.
    // If the message data contins a shared memory object, then CloneScope would
    // return SameProcess. Otherwise, it returns DifferentProcess.
    if (message.CloneScope() ==
        StructuredCloneHolder::StructuredCloneScope::DifferentProcess) {
      ClonedMessageData clonedMessageData;
      if (!message.BuildClonedMessageDataForChild(cc, clonedMessageData)) {
        aError.Throw(NS_ERROR_FAILURE);
        return;
      }

      messageData = std::move(clonedMessageData);
    } else {
      MOZ_ASSERT(message.CloneScope() ==
                 StructuredCloneHolder::StructuredCloneScope::SameProcess);

      messageData = ErrorMessageData();

      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, "DOM Window"_ns,
          callerInnerWindow ? callerInnerWindow->GetDocument() : nullptr,
          nsContentUtils::eDOM_PROPERTIES,
          "PostMessageSharedMemoryObjectToCrossOriginWarning");
    }

    cc->SendWindowPostMessage(this, messageData, data);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    if (message.CloneScope() ==
        StructuredCloneHolder::StructuredCloneScope::DifferentProcess) {
      ClonedMessageData clonedMessageData;
      if (!message.BuildClonedMessageDataForParent(cp, clonedMessageData)) {
        aError.Throw(NS_ERROR_FAILURE);
        return;
      }

      messageData = std::move(clonedMessageData);
    } else {
      MOZ_ASSERT(message.CloneScope() ==
                 StructuredCloneHolder::StructuredCloneScope::SameProcess);

      messageData = ErrorMessageData();

      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, "DOM Window"_ns,
          callerInnerWindow ? callerInnerWindow->GetDocument() : nullptr,
          nsContentUtils::eDOM_PROPERTIES,
          "PostMessageSharedMemoryObjectToCrossOriginWarning");
    }

    Unused << cp->SendWindowPostMessage(this, messageData, data);
  }
}

void BrowsingContext::PostMessageMoz(JSContext* aCx,
                                     JS::Handle<JS::Value> aMessage,
                                     const WindowPostMessageOptions& aOptions,
                                     nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  PostMessageMoz(aCx, aMessage, aOptions.mTargetOrigin, aOptions.mTransfer,
                 aSubjectPrincipal, aError);
}

void BrowsingContext::SendCommitTransaction(ContentParent* aParent,
                                            const BaseTransaction& aTxn,
                                            uint64_t aEpoch) {
  Unused << aParent->SendCommitBrowsingContextTransaction(this, aTxn, aEpoch);
}

void BrowsingContext::SendCommitTransaction(ContentChild* aChild,
                                            const BaseTransaction& aTxn,
                                            uint64_t aEpoch) {
  aChild->SendCommitBrowsingContextTransaction(this, aTxn, aEpoch);
}

BrowsingContext::IPCInitializer BrowsingContext::GetIPCInitializer() {
  MOZ_DIAGNOSTIC_ASSERT(mEverAttached);
  MOZ_DIAGNOSTIC_ASSERT(mType == Type::Content);

  IPCInitializer init;
  init.mId = Id();
  init.mParentId = mParentWindow ? mParentWindow->Id() : 0;
  init.mWindowless = mWindowless;
  init.mUseRemoteTabs = mUseRemoteTabs;
  init.mUseRemoteSubframes = mUseRemoteSubframes;
  init.mCreatedDynamically = mCreatedDynamically;
  init.mChildOffset = mChildOffset;
  init.mOriginAttributes = mOriginAttributes;
  if (mChildSessionHistory && mozilla::SessionHistoryInParent()) {
    init.mSessionHistoryIndex = mChildSessionHistory->Index();
    init.mSessionHistoryCount = mChildSessionHistory->Count();
  }
  init.mRequestContextId = mRequestContextId;
  init.mFields = mFields.RawValues();
  return init;
}

already_AddRefed<WindowContext> BrowsingContext::IPCInitializer::GetParent() {
  RefPtr<WindowContext> parent;
  if (mParentId != 0) {
    parent = WindowContext::GetById(mParentId);
    MOZ_RELEASE_ASSERT(parent);
  }
  return parent.forget();
}

already_AddRefed<BrowsingContext> BrowsingContext::IPCInitializer::GetOpener() {
  RefPtr<BrowsingContext> opener;
  if (GetOpenerId() != 0) {
    opener = BrowsingContext::Get(GetOpenerId());
    MOZ_RELEASE_ASSERT(opener);
  }
  return opener.forget();
}

void BrowsingContext::StartDelayedAutoplayMediaComponents() {
  if (!mDocShell) {
    return;
  }
  AUTOPLAY_LOG("%s : StartDelayedAutoplayMediaComponents for bc 0x%08" PRIx64,
               XRE_IsParentProcess() ? "Parent" : "Child", Id());
  mDocShell->StartDelayedAutoplayMediaComponents();
}

nsresult BrowsingContext::ResetGVAutoplayRequestStatus() {
  MOZ_ASSERT(IsTop(),
             "Should only set GVAudibleAutoplayRequestStatus in the top-level "
             "browsing context");

  Transaction txn;
  txn.SetGVAudibleAutoplayRequestStatus(GVAutoplayRequestStatus::eUNKNOWN);
  txn.SetGVInaudibleAutoplayRequestStatus(GVAutoplayRequestStatus::eUNKNOWN);
  return txn.Commit(this);
}

void BrowsingContext::DidSet(FieldIndex<IDX_GVAudibleAutoplayRequestStatus>) {
  MOZ_ASSERT(IsTop(),
             "Should only set GVAudibleAutoplayRequestStatus in the top-level "
             "browsing context");
}

void BrowsingContext::DidSet(FieldIndex<IDX_GVInaudibleAutoplayRequestStatus>) {
  MOZ_ASSERT(IsTop(),
             "Should only set GVAudibleAutoplayRequestStatus in the top-level "
             "browsing context");
}

void BrowsingContext::DidSet(FieldIndex<IDX_ExplicitActive>,
                             ExplicitActiveStatus aOldValue) {
  const bool isActive = IsActive();
  const bool wasActive = [&] {
    if (aOldValue != ExplicitActiveStatus::None) {
      return aOldValue == ExplicitActiveStatus::Active;
    }
    return GetParent() && GetParent()->IsActive();
  }();

  if (isActive == wasActive) {
    return;
  }

  if (IsTop()) {
    Group()->UpdateToplevelsSuspendedIfNeeded();

#if defined(XP_WIN) && defined(ACCESSIBILITY)
    if (XRE_IsParentProcess() && a11y::Compatibility::IsDolphin()) {
      // update active accessible documents on windows
      if (BrowserParent* bp = Canonical()->GetBrowserParent()) {
        if (a11y::DocAccessibleParent* tabDoc =
                bp->GetTopLevelDocAccessible()) {
          HWND window = tabDoc->GetEmulatedWindowHandle();
          MOZ_ASSERT(window);
          if (window) {
            if (isActive) {
              a11y::nsWinUtils::ShowNativeWindow(window);
            } else {
              a11y::nsWinUtils::HideNativeWindow(window);
            }
          }
        }
      }
    }
#endif
  }

  PreOrderWalk([&](BrowsingContext* aContext) {
    if (nsCOMPtr<nsIDocShell> ds = aContext->GetDocShell()) {
      nsDocShell::Cast(ds)->ActivenessMaybeChanged();
    }
  });
}

void BrowsingContext::DidSet(FieldIndex<IDX_InRDMPane>, bool aOldValue) {
  MOZ_ASSERT(IsTop(),
             "Should only set InRDMPane in the top-level browsing context");
  if (GetInRDMPane() == aOldValue) {
    return;
  }

  PreOrderWalk([&](BrowsingContext* aContext) {
    if (nsIDocShell* shell = aContext->GetDocShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->RecomputeTheme();

        // This is a bit of a lie, but this affects the overlay-scrollbars
        // media query and it's the code-path that gets taken for regular system
        // metrics changes via ThemeChanged().
        pc->MediaFeatureValuesChanged(
            {MediaFeatureChangeReason::SystemMetricsChange},
            MediaFeatureChangePropagation::JustThisDocument);
      }
    }
  });
}

bool BrowsingContext::CanSet(FieldIndex<IDX_PageAwakeRequestCount>,
                             uint32_t aNewValue, ContentParent* aSource) {
  return IsTop() && XRE_IsParentProcess() && !aSource;
}

void BrowsingContext::DidSet(FieldIndex<IDX_PageAwakeRequestCount>,
                             uint32_t aOldValue) {
  if (!IsTop() || aOldValue == GetPageAwakeRequestCount()) {
    return;
  }
  Group()->UpdateToplevelsSuspendedIfNeeded();
}

auto BrowsingContext::CanSet(FieldIndex<IDX_AllowJavascript>, bool aValue,
                             ContentParent* aSource) -> CanSetResult {
  if (mozilla::SessionHistoryInParent()) {
    return XRE_IsParentProcess() && !aSource ? CanSetResult::Allow
                                             : CanSetResult::Deny;
  }

  // Without Session History in Parent, session restore code still needs to set
  // this from content processes.
  return LegacyRevertIfNotOwningOrParentProcess(aSource);
}

void BrowsingContext::DidSet(FieldIndex<IDX_AllowJavascript>, bool aOldValue) {
  RecomputeCanExecuteScripts();
}

void BrowsingContext::RecomputeCanExecuteScripts() {
  const bool old = mCanExecuteScripts;
  if (!AllowJavascript()) {
    // Scripting has been explicitly disabled on our BrowsingContext.
    mCanExecuteScripts = false;
  } else if (GetParentWindowContext()) {
    // Otherwise, inherit parent.
    mCanExecuteScripts = GetParentWindowContext()->CanExecuteScripts();
  } else {
    // Otherwise, we're the root of the tree, and we haven't explicitly disabled
    // script. Allow.
    mCanExecuteScripts = true;
  }

  if (old != mCanExecuteScripts) {
    for (WindowContext* wc : GetWindowContexts()) {
      wc->RecomputeCanExecuteScripts();
    }
  }
}

bool BrowsingContext::InactiveForSuspend() const {
  if (!StaticPrefs::dom_suspend_inactive_enabled()) {
    return false;
  }
  // We should suspend a page only when it's inactive and doesn't have any awake
  // request that is used to prevent page from being suspended because web page
  // might still need to run their script. Eg. waiting for media keys to resume
  // media, playing web audio, waiting in a video call conference room.
  return !IsActive() && GetPageAwakeRequestCount() == 0;
}

bool BrowsingContext::CanSet(FieldIndex<IDX_TouchEventsOverrideInternal>,
                             dom::TouchEventsOverride, ContentParent* aSource) {
  return XRE_IsParentProcess() && !aSource;
}

void BrowsingContext::DidSet(FieldIndex<IDX_PrefersColorSchemeOverride>,
                             dom::PrefersColorSchemeOverride aOldValue) {
  MOZ_ASSERT(IsTop());
  if (PrefersColorSchemeOverride() == aOldValue) {
    return;
  }
  PreOrderWalk([&](BrowsingContext* aContext) {
    if (nsIDocShell* shell = aContext->GetDocShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->RecomputeBrowsingContextDependentData();
      }
    }
  });
}

void BrowsingContext::DidSet(FieldIndex<IDX_MediumOverride>,
                             nsString&& aOldValue) {
  MOZ_ASSERT(IsTop());
  if (GetMediumOverride() == aOldValue) {
    return;
  }
  PreOrderWalk([&](BrowsingContext* aContext) {
    if (nsIDocShell* shell = aContext->GetDocShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->RecomputeBrowsingContextDependentData();
      }
    }
  });
}

void BrowsingContext::DidSet(FieldIndex<IDX_DisplayMode>,
                             enum DisplayMode aOldValue) {
  MOZ_ASSERT(IsTop());

  if (GetDisplayMode() == aOldValue) {
    return;
  }

  PreOrderWalk([&](BrowsingContext* aContext) {
    if (nsIDocShell* shell = aContext->GetDocShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->MediaFeatureValuesChanged(
            {MediaFeatureChangeReason::DisplayModeChange},
            // We're already iterating through sub documents, so we don't need
            // to propagate the change again.
            //
            // Images and other resources don't change their display-mode
            // evaluation, display-mode is a property of the browsing context.
            MediaFeatureChangePropagation::JustThisDocument);
      }
    }
  });
}

void BrowsingContext::DidSet(FieldIndex<IDX_Muted>) {
  MOZ_ASSERT(IsTop(), "Set muted flag on non top-level context!");
  USER_ACTIVATION_LOG("Set audio muted %d for %s browsing context 0x%08" PRIx64,
                      GetMuted(), XRE_IsParentProcess() ? "Parent" : "Child",
                      Id());
  PreOrderWalk([&](BrowsingContext* aContext) {
    nsPIDOMWindowOuter* win = aContext->GetDOMWindow();
    if (win) {
      win->RefreshMediaElementsVolume();
    }
  });
}

bool BrowsingContext::CanSet(FieldIndex<IDX_ShouldDelayMediaFromStart>,
                             const bool& aValue, ContentParent* aSource) {
  return IsTop();
}

void BrowsingContext::DidSet(FieldIndex<IDX_ShouldDelayMediaFromStart>,
                             bool aOldValue) {
  MOZ_ASSERT(IsTop(), "Set attribute on non top-level context!");
  if (aOldValue == GetShouldDelayMediaFromStart()) {
    return;
  }
  if (!GetShouldDelayMediaFromStart()) {
    PreOrderWalk([&](BrowsingContext* aContext) {
      if (nsPIDOMWindowOuter* win = aContext->GetDOMWindow()) {
        win->ActivateMediaComponents();
      }
    });
  }
}

bool BrowsingContext::CanSet(FieldIndex<IDX_OverrideDPPX>, const float& aValue,
                             ContentParent* aSource) {
  return XRE_IsParentProcess() && !aSource && IsTop();
}

void BrowsingContext::DidSet(FieldIndex<IDX_OverrideDPPX>, float aOldValue) {
  MOZ_ASSERT(IsTop());
  if (GetOverrideDPPX() == aOldValue) {
    return;
  }

  PreOrderWalk([&](BrowsingContext* aContext) {
    if (nsIDocShell* shell = aContext->GetDocShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->RecomputeBrowsingContextDependentData();
      }
    }
  });
}

void BrowsingContext::SetCustomUserAgent(const nsAString& aUserAgent,
                                         ErrorResult& aRv) {
  Top()->SetUserAgentOverride(aUserAgent, aRv);
}

nsresult BrowsingContext::SetCustomUserAgent(const nsAString& aUserAgent) {
  return Top()->SetUserAgentOverride(aUserAgent);
}

void BrowsingContext::DidSet(FieldIndex<IDX_UserAgentOverride>) {
  MOZ_ASSERT(IsTop());

  PreOrderWalk([&](BrowsingContext* aContext) {
    nsIDocShell* shell = aContext->GetDocShell();
    if (shell) {
      shell->ClearCachedUserAgent();
    }
  });
}

bool BrowsingContext::CanSet(FieldIndex<IDX_IsInBFCache>, bool,
                             ContentParent* aSource) {
  return IsTop() && !aSource && mozilla::BFCacheInParent();
}

void BrowsingContext::DidSet(FieldIndex<IDX_IsInBFCache>) {
  MOZ_RELEASE_ASSERT(mozilla::BFCacheInParent());
  MOZ_DIAGNOSTIC_ASSERT(IsTop());

  const bool isInBFCache = GetIsInBFCache();
  if (!isInBFCache) {
    PreOrderWalk([&](BrowsingContext* aContext) {
      aContext->mIsInBFCache = false;
      nsCOMPtr<nsIDocShell> shell = aContext->GetDocShell();
      if (shell) {
        nsDocShell::Cast(shell)->ThawFreezeNonRecursive(true);
      }
    });
  }

  if (isInBFCache && XRE_IsContentProcess() && mDocShell) {
    nsDocShell::Cast(mDocShell)->MaybeDisconnectChildListenersOnPageHide();
  }

  if (isInBFCache) {
    PreOrderWalk([&](BrowsingContext* aContext) {
      nsCOMPtr<nsIDocShell> shell = aContext->GetDocShell();
      if (shell) {
        nsDocShell::Cast(shell)->FirePageHideShowNonRecursive(false);
      }
    });
  } else {
    PostOrderWalk([&](BrowsingContext* aContext) {
      nsCOMPtr<nsIDocShell> shell = aContext->GetDocShell();
      if (shell) {
        nsDocShell::Cast(shell)->FirePageHideShowNonRecursive(true);
      }
    });
  }

  if (isInBFCache) {
    PreOrderWalk([&](BrowsingContext* aContext) {
      nsCOMPtr<nsIDocShell> shell = aContext->GetDocShell();
      if (shell) {
        nsDocShell::Cast(shell)->ThawFreezeNonRecursive(false);
      }
      aContext->mIsInBFCache = true;
      Document* doc = aContext->GetDocument();
      if (doc) {
        // Notifying needs to happen after mIsInBFCache is set to true.
        doc->NotifyActivityChanged();
      }
    });
  }
}

void BrowsingContext::SetCustomPlatform(const nsAString& aPlatform,
                                        ErrorResult& aRv) {
  Top()->SetPlatformOverride(aPlatform, aRv);
}

void BrowsingContext::DidSet(FieldIndex<IDX_PlatformOverride>) {
  MOZ_ASSERT(IsTop());

  PreOrderWalk([&](BrowsingContext* aContext) {
    nsIDocShell* shell = aContext->GetDocShell();
    if (shell) {
      shell->ClearCachedPlatform();
    }
  });
}

auto BrowsingContext::LegacyRevertIfNotOwningOrParentProcess(
    ContentParent* aSource) -> CanSetResult {
  if (aSource) {
    MOZ_ASSERT(XRE_IsParentProcess());

    if (!Canonical()->IsOwnedByProcess(aSource->ChildID())) {
      return CanSetResult::Revert;
    }
  } else if (!IsInProcess() && !XRE_IsParentProcess()) {
    // Don't allow this to be set from content processes that
    // don't own the BrowsingContext.
    return CanSetResult::Deny;
  }

  return CanSetResult::Allow;
}

bool BrowsingContext::CanSet(FieldIndex<IDX_IsActiveBrowserWindowInternal>,
                             const bool& aValue, ContentParent* aSource) {
  // Should only be set in the parent process.
  return XRE_IsParentProcess() && !aSource && IsTop();
}

void BrowsingContext::DidSet(FieldIndex<IDX_IsActiveBrowserWindowInternal>,
                             bool aOldValue) {
  bool isActivateEvent = GetIsActiveBrowserWindowInternal();
  // The browser window containing this context has changed
  // activation state so update window inactive document states
  // for all in-process documents.
  PreOrderWalk([isActivateEvent](BrowsingContext* aContext) {
    if (RefPtr<Document> doc = aContext->GetExtantDocument()) {
      doc->UpdateDocumentStates(NS_DOCUMENT_STATE_WINDOW_INACTIVE, true);

      RefPtr<nsPIDOMWindowInner> win = doc->GetInnerWindow();
      RefPtr<MediaDevices> devices;
      if (isActivateEvent && (devices = win->GetExtantMediaDevices())) {
        devices->BrowserWindowBecameActive();
      }

      if (XRE_IsContentProcess() &&
          (!aContext->GetParent() || !aContext->GetParent()->IsInProcess())) {
        // Send the inner window an activate/deactivate event if
        // the context is the top of a sub-tree of in-process
        // contexts.
        nsContentUtils::DispatchEventOnlyToChrome(
            doc, win, isActivateEvent ? u"activate"_ns : u"deactivate"_ns,
            CanBubble::eYes, Cancelable::eYes, nullptr);
      }
    }
  });
}

auto BrowsingContext::CanSet(FieldIndex<IDX_AllowContentRetargeting>,
                             const bool& aAllowContentRetargeting,
                             ContentParent* aSource) -> CanSetResult {
  return LegacyRevertIfNotOwningOrParentProcess(aSource);
}

auto BrowsingContext::CanSet(FieldIndex<IDX_AllowContentRetargetingOnChildren>,
                             const bool& aAllowContentRetargetingOnChildren,
                             ContentParent* aSource) -> CanSetResult {
  return LegacyRevertIfNotOwningOrParentProcess(aSource);
}

auto BrowsingContext::CanSet(FieldIndex<IDX_AllowPlugins>,
                             const bool& aAllowPlugins, ContentParent* aSource)
    -> CanSetResult {
  return LegacyRevertIfNotOwningOrParentProcess(aSource);
}

bool BrowsingContext::CanSet(FieldIndex<IDX_FullscreenAllowedByOwner>,
                             const bool& aAllowed, ContentParent* aSource) {
  return CheckOnlyEmbedderCanSet(aSource);
}

bool BrowsingContext::CanSet(FieldIndex<IDX_UseErrorPages>,
                             const bool& aUseErrorPages,
                             ContentParent* aSource) {
  return CheckOnlyEmbedderCanSet(aSource);
}

mozilla::dom::TouchEventsOverride BrowsingContext::TouchEventsOverride() const {
  const BrowsingContext* bc = this;
  while (bc) {
    mozilla::dom::TouchEventsOverride tev =
        bc->GetTouchEventsOverrideInternal();
    if (tev != mozilla::dom::TouchEventsOverride::None) {
      return tev;
    }

    bc = bc->GetParent();
  }

  return mozilla::dom::TouchEventsOverride::None;
}

// We map `watchedByDevTools` WebIDL attribute to `watchedByDevToolsInternal`
// BC field. And we map it to the top level BrowsingContext.
bool BrowsingContext::WatchedByDevTools() {
  return Top()->GetWatchedByDevToolsInternal();
}

// Enforce that the watchedByDevTools BC field can only be set on the top level
// Browsing Context.
bool BrowsingContext::CanSet(FieldIndex<IDX_WatchedByDevToolsInternal>,
                             const bool& aWatchedByDevTools,
                             ContentParent* aSource) {
  return IsTop();
}
void BrowsingContext::SetWatchedByDevTools(bool aWatchedByDevTools,
                                           ErrorResult& aRv) {
  if (!IsTop()) {
    aRv.ThrowInvalidModificationError(
        "watchedByDevTools can only be set on top BrowsingContext");
    return;
  }
  SetWatchedByDevToolsInternal(aWatchedByDevTools, aRv);
}

auto BrowsingContext::CanSet(FieldIndex<IDX_DefaultLoadFlags>,
                             const uint32_t& aDefaultLoadFlags,
                             ContentParent* aSource) -> CanSetResult {
  // Bug 1623565 - Are these flags only used by the debugger, which makes it
  // possible that this field can only be settable by the parent process?
  return LegacyRevertIfNotOwningOrParentProcess(aSource);
}

void BrowsingContext::DidSet(FieldIndex<IDX_DefaultLoadFlags>) {
  auto loadFlags = GetDefaultLoadFlags();
  if (GetDocShell()) {
    nsDocShell::Cast(GetDocShell())->SetLoadGroupDefaultLoadFlags(loadFlags);
  }

  if (XRE_IsParentProcess()) {
    PreOrderWalk([&](BrowsingContext* aContext) {
      if (aContext != this) {
        // Setting load flags on a discarded context has no effect.
        Unused << aContext->SetDefaultLoadFlags(loadFlags);
      }
    });
  }
}

bool BrowsingContext::CanSet(FieldIndex<IDX_UseGlobalHistory>,
                             const bool& aUseGlobalHistory,
                             ContentParent* aSource) {
  // Should only be set in the parent process.
  //  return XRE_IsParentProcess() && !aSource;
  return true;
}

auto BrowsingContext::CanSet(FieldIndex<IDX_UserAgentOverride>,
                             const nsString& aUserAgent, ContentParent* aSource)
    -> CanSetResult {
  if (!IsTop()) {
    return CanSetResult::Deny;
  }

  return LegacyRevertIfNotOwningOrParentProcess(aSource);
}

auto BrowsingContext::CanSet(FieldIndex<IDX_PlatformOverride>,
                             const nsString& aPlatform, ContentParent* aSource)
    -> CanSetResult {
  if (!IsTop()) {
    return CanSetResult::Deny;
  }

  return LegacyRevertIfNotOwningOrParentProcess(aSource);
}

bool BrowsingContext::CheckOnlyEmbedderCanSet(ContentParent* aSource) {
  if (XRE_IsParentProcess()) {
    uint64_t childId = aSource ? aSource->ChildID() : 0;
    return Canonical()->IsEmbeddedInProcess(childId);
  }
  return mEmbeddedByThisProcess;
}

bool BrowsingContext::CanSet(FieldIndex<IDX_EmbedderInnerWindowId>,
                             const uint64_t& aValue, ContentParent* aSource) {
  // If we have a parent window, our embedder inner window ID must match it.
  if (mParentWindow) {
    return mParentWindow->Id() == aValue;
  }

  // For toplevel BrowsingContext instances, this value may only be set by the
  // parent process, or initialized to `0`.
  return CheckOnlyEmbedderCanSet(aSource);
}

bool BrowsingContext::CanSet(FieldIndex<IDX_EmbedderElementType>,
                             const Maybe<nsString>&, ContentParent* aSource) {
  return CheckOnlyEmbedderCanSet(aSource);
}

auto BrowsingContext::CanSet(FieldIndex<IDX_CurrentInnerWindowId>,
                             const uint64_t& aValue, ContentParent* aSource)
    -> CanSetResult {
  // Generally allow clearing this. We may want to be more precise about this
  // check in the future.
  if (aValue == 0) {
    return CanSetResult::Allow;
  }

  // We must have access to the specified context.
  RefPtr<WindowContext> window = WindowContext::GetById(aValue);
  if (!window || window->GetBrowsingContext() != this) {
    return CanSetResult::Deny;
  }

  if (aSource) {
    // If the sending process is no longer the current owner, revert
    MOZ_ASSERT(XRE_IsParentProcess());
    if (!Canonical()->IsOwnedByProcess(aSource->ChildID())) {
      return CanSetResult::Revert;
    }
  } else if (XRE_IsContentProcess() && !IsOwnedByProcess()) {
    return CanSetResult::Deny;
  }

  return CanSetResult::Allow;
}

void BrowsingContext::DidSet(FieldIndex<IDX_CurrentInnerWindowId>) {
  RefPtr<WindowContext> prevWindowContext = mCurrentWindowContext.forget();
  mCurrentWindowContext = WindowContext::GetById(GetCurrentInnerWindowId());
  MOZ_ASSERT(
      !mCurrentWindowContext || mWindowContexts.Contains(mCurrentWindowContext),
      "WindowContext not registered?");

  // Clear our cached `children` value, to ensure that JS sees the up-to-date
  // value.
  BrowsingContext_Binding::ClearCachedChildrenValue(this);

  if (XRE_IsParentProcess()) {
    if (prevWindowContext != mCurrentWindowContext) {
      if (prevWindowContext) {
        prevWindowContext->Canonical()->DidBecomeCurrentWindowGlobal(false);
      }
      if (mCurrentWindowContext) {
        // We set a timer when we set the current inner window. This
        // will then flush the session storage to session store to
        // make sure that we don't miss to store session storage to
        // session store that is a result of navigation. This is due
        // to Bug 1700623. We wish to fix this in Bug 1711886, where
        // making sure to store everything would make this timer
        // unnecessary.
        Canonical()->MaybeScheduleSessionStoreUpdate();
        mCurrentWindowContext->Canonical()->DidBecomeCurrentWindowGlobal(true);
      }
    }
    BrowserParent::UpdateFocusFromBrowsingContext();
  }
}

bool BrowsingContext::CanSet(FieldIndex<IDX_IsPopupSpam>, const bool& aValue,
                             ContentParent* aSource) {
  // Ensure that we only mark a browsing context as popup spam once and never
  // unmark it.
  return aValue && !GetIsPopupSpam();
}

void BrowsingContext::DidSet(FieldIndex<IDX_IsPopupSpam>) {
  if (GetIsPopupSpam()) {
    PopupBlocker::RegisterOpenPopupSpam();
  }
}

bool BrowsingContext::CanSet(FieldIndex<IDX_MessageManagerGroup>,
                             const nsString& aMessageManagerGroup,
                             ContentParent* aSource) {
  // Should only be set in the parent process on toplevel.
  return XRE_IsParentProcess() && !aSource && IsTopContent();
}

bool BrowsingContext::CanSet(
    FieldIndex<IDX_OrientationLock>,
    const mozilla::hal::ScreenOrientation& aOrientationLock,
    ContentParent* aSource) {
  return IsTop();
}

bool BrowsingContext::IsLoading() {
  if (GetLoading()) {
    return true;
  }

  // If we're in the same process as the page, we're possibly just
  // updating the flag.
  nsIDocShell* shell = GetDocShell();
  if (shell) {
    Document* doc = shell->GetDocument();
    return doc && doc->GetReadyStateEnum() < Document::READYSTATE_COMPLETE;
  }

  return false;
}

void BrowsingContext::DidSet(FieldIndex<IDX_Loading>) {
  if (mFields.Get<IDX_Loading>()) {
    return;
  }

  while (!mDeprioritizedLoadRunner.isEmpty()) {
    nsCOMPtr<nsIRunnable> runner = mDeprioritizedLoadRunner.popFirst();
    NS_DispatchToCurrentThread(runner.forget());
  }

  if (StaticPrefs::dom_separate_event_queue_for_post_message_enabled() &&
      Top() == this) {
    Group()->FlushPostMessageEvents();
  }
}

// Inform the Document for this context of the (potential) change in
// loading state
void BrowsingContext::DidSet(FieldIndex<IDX_AncestorLoading>) {
  nsPIDOMWindowOuter* outer = GetDOMWindow();
  if (!outer) {
    MOZ_LOG(gTimeoutDeferralLog, mozilla::LogLevel::Debug,
            ("DidSetAncestorLoading BC: %p -- No outer window", (void*)this));
    return;
  }
  Document* document = nsGlobalWindowOuter::Cast(outer)->GetExtantDoc();
  if (document) {
    MOZ_LOG(gTimeoutDeferralLog, mozilla::LogLevel::Debug,
            ("DidSetAncestorLoading BC: %p -- NotifyLoading(%d, %d, %d)",
             (void*)this, GetAncestorLoading(), document->GetReadyStateEnum(),
             document->GetReadyStateEnum()));
    document->NotifyLoading(GetAncestorLoading(), document->GetReadyStateEnum(),
                            document->GetReadyStateEnum());
  }
}

void BrowsingContext::DidSet(FieldIndex<IDX_AuthorStyleDisabledDefault>) {
  MOZ_ASSERT(IsTop(),
             "Should only set AuthorStyleDisabledDefault in the top "
             "browsing context");

  // We don't need to handle changes to this field, since PageStyleChild.jsm
  // will respond to the PageStyle:Disable message in all content processes.
  //
  // But we store the state here on the top BrowsingContext so that the
  // docshell has somewhere to look for the current author style disabling
  // state when new iframes are inserted.
}

void BrowsingContext::DidSet(FieldIndex<IDX_TextZoom>, float aOldValue) {
  if (GetTextZoom() == aOldValue) {
    return;
  }

  if (IsInProcess()) {
    if (nsIDocShell* shell = GetDocShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->RecomputeBrowsingContextDependentData();
      }
    }

    for (BrowsingContext* child : Children()) {
      // Setting text zoom on a discarded context has no effect.
      Unused << child->SetTextZoom(GetTextZoom());
    }
  }

  if (IsTop() && XRE_IsParentProcess()) {
    if (Element* element = GetEmbedderElement()) {
      auto dispatcher = MakeRefPtr<AsyncEventDispatcher>(
          element, u"TextZoomChange"_ns, CanBubble::eYes,
          ChromeOnlyDispatch::eYes);
      dispatcher->RunDOMEventWhenSafe();
    }
  }
}

// TODO(emilio): It'd be potentially nicer and cheaper to allow to set this only
// on the Top() browsing context, but there are a lot of tests that rely on
// zooming a subframe so...
void BrowsingContext::DidSet(FieldIndex<IDX_FullZoom>, float aOldValue) {
  if (GetFullZoom() == aOldValue) {
    return;
  }

  if (IsInProcess()) {
    if (nsIDocShell* shell = GetDocShell()) {
      if (nsPresContext* pc = shell->GetPresContext()) {
        pc->RecomputeBrowsingContextDependentData();
      }
    }

    for (BrowsingContext* child : Children()) {
      // Setting full zoom on a discarded context has no effect.
      Unused << child->SetFullZoom(GetFullZoom());
    }
  }

  if (IsTop() && XRE_IsParentProcess()) {
    if (Element* element = GetEmbedderElement()) {
      auto dispatcher = MakeRefPtr<AsyncEventDispatcher>(
          element, u"FullZoomChange"_ns, CanBubble::eYes,
          ChromeOnlyDispatch::eYes);
      dispatcher->RunDOMEventWhenSafe();
    }
  }
}

void BrowsingContext::AddDeprioritizedLoadRunner(nsIRunnable* aRunner) {
  MOZ_ASSERT(IsLoading());
  MOZ_ASSERT(Top() == this);

  RefPtr<DeprioritizedLoadRunner> runner = new DeprioritizedLoadRunner(aRunner);
  mDeprioritizedLoadRunner.insertBack(runner);
  NS_DispatchToCurrentThreadQueue(
      runner.forget(), StaticPrefs::page_load_deprioritization_period(),
      EventQueuePriority::Idle);
}

bool BrowsingContext::GetOffsetPath(nsTArray<uint32_t>& aPath) const {
  for (const BrowsingContext* current = this; current && current->GetParent();
       current = current->GetParent()) {
    if (current->CreatedDynamically()) {
      return false;
    }
    aPath.AppendElement(current->ChildOffset());
  }
  return true;
}

void BrowsingContext::GetHistoryID(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal,
                                   ErrorResult& aError) {
  if (!xpc::ID2JSValue(aCx, GetHistoryID(), aVal)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void BrowsingContext::InitSessionHistory() {
  MOZ_ASSERT(!IsDiscarded());
  MOZ_ASSERT(IsTop());
  MOZ_ASSERT(EverAttached());

  if (!GetHasSessionHistory()) {
    MOZ_ALWAYS_SUCCEEDS(SetHasSessionHistory(true));
  }
}

ChildSHistory* BrowsingContext::GetChildSessionHistory() {
  if (!mozilla::SessionHistoryInParent()) {
    // For now we're checking that the session history object for the child
    // process is available before returning the ChildSHistory object, because
    // it is the actual implementation that ChildSHistory forwards to. This can
    // be removed once session history is stored exclusively in the parent
    // process.
    return mChildSessionHistory && mChildSessionHistory->IsInProcess()
               ? mChildSessionHistory.get()
               : nullptr;
  }

  return mChildSessionHistory;
}

void BrowsingContext::CreateChildSHistory() {
  MOZ_ASSERT(IsTop());
  MOZ_ASSERT(GetHasSessionHistory());
  MOZ_DIAGNOSTIC_ASSERT(!mChildSessionHistory);

  // Because session history is global in a browsing context tree, every process
  // that has access to a browsing context tree needs access to its session
  // history. That is why we create the ChildSHistory object in every process
  // where we have access to this browsing context (which is the top one).
  mChildSessionHistory = new ChildSHistory(this);

  // If the top browsing context (this one) is loaded in this process then we
  // also create the session history implementation for the child process.
  // This can be removed once session history is stored exclusively in the
  // parent process.
  mChildSessionHistory->SetIsInProcess(IsInProcess());
}

void BrowsingContext::DidSet(FieldIndex<IDX_HasSessionHistory>,
                             bool aOldValue) {
  MOZ_ASSERT(GetHasSessionHistory() || !aOldValue,
             "We don't support turning off session history.");

  if (GetHasSessionHistory() && !aOldValue) {
    CreateChildSHistory();
  }
}

bool BrowsingContext::CanSet(FieldIndex<IDX_BrowserId>, const uint32_t& aValue,
                             ContentParent* aSource) {
  // We should only be able to set this for toplevel contexts which don't have
  // an ID yet.
  return GetBrowserId() == 0 && IsTop() && Children().IsEmpty();
}

bool BrowsingContext::CanSet(FieldIndex<IDX_PendingInitialization>,
                             bool aNewValue, ContentParent* aSource) {
  // Can only be cleared from `true` to `false`, and should only ever be set on
  // the toplevel BrowsingContext.
  return IsTop() && GetPendingInitialization() && !aNewValue;
}

bool BrowsingContext::CanSet(FieldIndex<IDX_HasRestoreData>, bool aNewValue,
                             ContentParent* aSource) {
  return IsTop();
}

bool BrowsingContext::IsPopupAllowed() {
  for (auto* context = GetCurrentWindowContext(); context;
       context = context->GetParentWindowContext()) {
    if (context->CanShowPopup()) {
      return true;
    }
  }

  return false;
}

/* static */
bool BrowsingContext::ShouldAddEntryForRefresh(
    nsIURI* aCurrentURI, const SessionHistoryInfo& aInfo) {
  if (aInfo.GetPostData()) {
    return true;
  }

  bool equalsURI = false;
  if (aCurrentURI) {
    aCurrentURI->Equals(aInfo.GetURI(), &equalsURI);
  }
  return !equalsURI;
}

void BrowsingContext::SessionHistoryCommit(
    const LoadingSessionHistoryInfo& aInfo, uint32_t aLoadType,
    nsIURI* aCurrentURI, bool aHadActiveEntry, bool aPersist,
    bool aCloneEntryChildren, bool aChannelExpired) {
  nsID changeID = {};
  if (XRE_IsContentProcess()) {
    RefPtr<ChildSHistory> rootSH = Top()->GetChildSessionHistory();
    if (rootSH) {
      if (!aInfo.mLoadIsFromSessionHistory) {
        // We try to mimic as closely as possible what will happen in
        // CanonicalBrowsingContext::SessionHistoryCommit. We'll be
        // incrementing the session history length if we're not replacing,
        // this is a top-level load or it's not the initial load in an iframe,
        // ShouldUpdateSessionHistory(loadType) returns true and it's not a
        // refresh for which ShouldAddEntryForRefresh returns false.
        // It is possible that this leads to wrong length temporarily, but
        // so would not having the check for replace.
        if (!LOAD_TYPE_HAS_FLAGS(
                aLoadType, nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY) &&
            (IsTop() || aHadActiveEntry) &&
            ShouldUpdateSessionHistory(aLoadType) &&
            (!LOAD_TYPE_HAS_FLAGS(aLoadType,
                                  nsIWebNavigation::LOAD_FLAGS_IS_REFRESH) ||
             ShouldAddEntryForRefresh(aCurrentURI, aInfo.mInfo))) {
          changeID = rootSH->AddPendingHistoryChange();
        }
      } else {
        // History load doesn't change the length, only index.
        changeID = rootSH->AddPendingHistoryChange(aInfo.mOffset, 0);
      }
    }
    ContentChild* cc = ContentChild::GetSingleton();
    mozilla::Unused << cc->SendHistoryCommit(
        this, aInfo.mLoadId, changeID, aLoadType, aPersist, aCloneEntryChildren,
        aChannelExpired);
  } else {
    Canonical()->SessionHistoryCommit(aInfo.mLoadId, changeID, aLoadType,
                                      aPersist, aCloneEntryChildren,
                                      aChannelExpired);
  }
}

void BrowsingContext::SetActiveSessionHistoryEntry(
    const Maybe<nsPoint>& aPreviousScrollPos, SessionHistoryInfo* aInfo,
    uint32_t aLoadType, uint32_t aUpdatedCacheKey) {
  if (XRE_IsContentProcess()) {
    // XXX Why we update cache key only in content process case?
    if (aUpdatedCacheKey != 0) {
      aInfo->SetCacheKey(aUpdatedCacheKey);
    }

    nsID changeID = {};
    RefPtr<ChildSHistory> shistory = Top()->GetChildSessionHistory();
    if (shistory) {
      changeID = shistory->AddPendingHistoryChange();
    }
    ContentChild::GetSingleton()->SendSetActiveSessionHistoryEntry(
        this, aPreviousScrollPos, *aInfo, aLoadType, aUpdatedCacheKey,
        changeID);
  } else {
    Canonical()->SetActiveSessionHistoryEntry(
        aPreviousScrollPos, aInfo, aLoadType, aUpdatedCacheKey, nsID());
  }
}

void BrowsingContext::ReplaceActiveSessionHistoryEntry(
    SessionHistoryInfo* aInfo) {
  if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->SendReplaceActiveSessionHistoryEntry(this,
                                                                       *aInfo);
  } else {
    Canonical()->ReplaceActiveSessionHistoryEntry(aInfo);
  }
}

void BrowsingContext::RemoveDynEntriesFromActiveSessionHistoryEntry() {
  if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()
        ->SendRemoveDynEntriesFromActiveSessionHistoryEntry(this);
  } else {
    Canonical()->RemoveDynEntriesFromActiveSessionHistoryEntry();
  }
}

void BrowsingContext::RemoveFromSessionHistory(const nsID& aChangeID) {
  if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->SendRemoveFromSessionHistory(this, aChangeID);
  } else {
    Canonical()->RemoveFromSessionHistory(aChangeID);
  }
}

void BrowsingContext::HistoryGo(int32_t aOffset, uint64_t aHistoryEpoch,
                                bool aRequireUserInteraction,
                                bool aUserActivation,
                                std::function<void(int32_t&&)>&& aResolver) {
  if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->SendHistoryGo(
        this, aOffset, aHistoryEpoch, aRequireUserInteraction, aUserActivation,
        std::move(aResolver),
        [](mozilla::ipc::
               ResponseRejectReason) { /* FIXME Is ignoring this fine? */ });
  } else {
    Canonical()->HistoryGo(
        aOffset, aHistoryEpoch, aRequireUserInteraction, aUserActivation,
        Canonical()->GetContentParent()
            ? Some(Canonical()->GetContentParent()->ChildID())
            : Nothing(),
        std::move(aResolver));
  }
}

void BrowsingContext::SetChildSHistory(ChildSHistory* aChildSHistory) {
  mChildSessionHistory = aChildSHistory;
  mChildSessionHistory->SetBrowsingContext(this);
  mFields.SetWithoutSyncing<IDX_HasSessionHistory>(true);
}

bool BrowsingContext::ShouldUpdateSessionHistory(uint32_t aLoadType) {
  // We don't update session history on reload unless we're loading
  // an iframe in shift-reload case.
  return nsDocShell::ShouldUpdateGlobalHistory(aLoadType) &&
         (!(aLoadType & nsIDocShell::LOAD_CMD_RELOAD) ||
          (IsForceReloadType(aLoadType) && IsSubframe()));
}

nsresult BrowsingContext::CheckLocationChangeRateLimit(CallerType aCallerType) {
  // We only rate limit non system callers
  if (aCallerType == CallerType::System) {
    return NS_OK;
  }

  // Fetch rate limiting preferences
  uint32_t limitCount =
      StaticPrefs::dom_navigation_locationChangeRateLimit_count();
  uint32_t timeSpanSeconds =
      StaticPrefs::dom_navigation_locationChangeRateLimit_timespan();

  // Disable throttling if either of the preferences is set to 0.
  if (limitCount == 0 || timeSpanSeconds == 0) {
    return NS_OK;
  }

  TimeDuration throttleSpan = TimeDuration::FromSeconds(timeSpanSeconds);

  if (mLocationChangeRateLimitSpanStart.IsNull() ||
      ((TimeStamp::Now() - mLocationChangeRateLimitSpanStart) > throttleSpan)) {
    // Initial call or timespan exceeded, reset counter and timespan.
    mLocationChangeRateLimitSpanStart = TimeStamp::Now();
    mLocationChangeRateLimitCount = 1;
    return NS_OK;
  }

  if (mLocationChangeRateLimitCount >= limitCount) {
    // Rate limit reached

    Document* doc = GetDocument();
    if (doc) {
      nsContentUtils::ReportToConsole(nsIScriptError::errorFlag, "DOM"_ns, doc,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "LocChangeFloodingPrevented");
    }

    return NS_ERROR_DOM_SECURITY_ERR;
  }

  mLocationChangeRateLimitCount++;
  return NS_OK;
}

void BrowsingContext::ResetLocationChangeRateLimit() {
  // Resetting the timestamp object will cause the check function to
  // init again and reset the rate limit.
  mLocationChangeRateLimitSpanStart = TimeStamp();
}

}  // namespace dom

namespace ipc {

void IPDLParamTraits<dom::MaybeDiscarded<dom::BrowsingContext>>::Write(
    IPC::Message* aMsg, IProtocol* aActor,
    const dom::MaybeDiscarded<dom::BrowsingContext>& aParam) {
  MOZ_DIAGNOSTIC_ASSERT(!aParam.GetMaybeDiscarded() ||
                        aParam.GetMaybeDiscarded()->EverAttached());
  uint64_t id = aParam.ContextId();
  WriteIPDLParam(aMsg, aActor, id);
}

bool IPDLParamTraits<dom::MaybeDiscarded<dom::BrowsingContext>>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    dom::MaybeDiscarded<dom::BrowsingContext>* aResult) {
  uint64_t id = 0;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &id)) {
    return false;
  }

  if (id == 0) {
    *aResult = nullptr;
  } else if (RefPtr<dom::BrowsingContext> bc = dom::BrowsingContext::Get(id)) {
    *aResult = std::move(bc);
  } else {
    aResult->SetDiscarded(id);
  }
  return true;
}

void IPDLParamTraits<dom::BrowsingContext::IPCInitializer>::Write(
    IPC::Message* aMessage, IProtocol* aActor,
    const dom::BrowsingContext::IPCInitializer& aInit) {
  // Write actor ID parameters.
  WriteIPDLParam(aMessage, aActor, aInit.mId);
  WriteIPDLParam(aMessage, aActor, aInit.mParentId);
  WriteIPDLParam(aMessage, aActor, aInit.mWindowless);
  WriteIPDLParam(aMessage, aActor, aInit.mUseRemoteTabs);
  WriteIPDLParam(aMessage, aActor, aInit.mUseRemoteSubframes);
  WriteIPDLParam(aMessage, aActor, aInit.mCreatedDynamically);
  WriteIPDLParam(aMessage, aActor, aInit.mChildOffset);
  WriteIPDLParam(aMessage, aActor, aInit.mOriginAttributes);
  WriteIPDLParam(aMessage, aActor, aInit.mRequestContextId);
  WriteIPDLParam(aMessage, aActor, aInit.mSessionHistoryIndex);
  WriteIPDLParam(aMessage, aActor, aInit.mSessionHistoryCount);
  WriteIPDLParam(aMessage, aActor, aInit.mFields);
}

bool IPDLParamTraits<dom::BrowsingContext::IPCInitializer>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::BrowsingContext::IPCInitializer* aInit) {
  // Read actor ID parameters.
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mId) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mParentId) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mWindowless) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mUseRemoteTabs) ||
      !ReadIPDLParam(aMessage, aIterator, aActor,
                     &aInit->mUseRemoteSubframes) ||
      !ReadIPDLParam(aMessage, aIterator, aActor,
                     &aInit->mCreatedDynamically) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mChildOffset) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mOriginAttributes) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mRequestContextId) ||
      !ReadIPDLParam(aMessage, aIterator, aActor,
                     &aInit->mSessionHistoryIndex) ||
      !ReadIPDLParam(aMessage, aIterator, aActor,
                     &aInit->mSessionHistoryCount) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mFields)) {
    return false;
  }
  return true;
}

template struct IPDLParamTraits<dom::BrowsingContext::BaseTransaction>;

}  // namespace ipc
}  // namespace mozilla
