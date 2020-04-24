/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContext.h"

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/UserActivationIPCUtils.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/SyncedContextInlines.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/HashTable.h"
#include "mozilla/Logging.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_page_load.h"
#include "mozilla/StaticPtr.h"
#include "nsIURIFixup.h"

#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsGlobalWindowOuter.h"
#include "nsIObserverService.h"
#include "nsContentUtils.h"
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
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::OrientationType,
          mozilla::dom::OrientationType::Portrait_primary,
          mozilla::dom::OrientationType::Landscape_secondary> {};

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

typedef nsDataHashtable<nsUint64HashKey, BrowsingContext*> BrowsingContextMap;

static StaticAutoPtr<BrowsingContextMap> sBrowsingContexts;

static void Register(BrowsingContext* aBrowsingContext) {
  sBrowsingContexts->Put(aBrowsingContext->Id(), aBrowsingContext);

  aBrowsingContext->Group()->Register(aBrowsingContext);
}

BrowsingContext* BrowsingContext::Top() {
  BrowsingContext* bc = this;
  while (bc->mParent) {
    bc = bc->mParent;
  }
  return bc;
}

/* static */
void BrowsingContext::Init() {
  if (!sBrowsingContexts) {
    sBrowsingContexts = new BrowsingContextMap();
    ClearOnShutdown(&sBrowsingContexts);
  }
}

/* static */
LogModule* BrowsingContext::GetLog() { return gBrowsingContextLog; }

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::Get(uint64_t aId) {
  return do_AddRef(sBrowsingContexts->Get(aId));
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::GetFromWindow(
    WindowProxyHolder& aProxy) {
  return do_AddRef(aProxy.get());
}

CanonicalBrowsingContext* BrowsingContext::Canonical() {
  return CanonicalBrowsingContext::Cast(this);
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
    BrowsingContext* aParent, BrowsingContext* aOpener, const nsAString& aName,
    Type aType) {
  MOZ_DIAGNOSTIC_ASSERT(!aParent || aParent->mType == aType);

  MOZ_DIAGNOSTIC_ASSERT(aType != Type::Chrome || XRE_IsParentProcess());

  uint64_t id = nsContentUtils::GenerateBrowsingContextId();

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " in %s", id,
           XRE_IsParentProcess() ? "Parent" : "Child"));

  // Determine which BrowsingContextGroup this context should be created in.
  RefPtr<BrowsingContextGroup> group =
      (aType == Type::Chrome)
          ? do_AddRef(BrowsingContextGroup::GetChromeGroup())
          : BrowsingContextGroup::Select(aParent, aOpener);

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context =
        new CanonicalBrowsingContext(aParent, group, id,
                                     /* aOwnerProcessId */ 0,
                                     /* aEmbedderProcessId */ 0, aType, {});
  } else {
    context = new BrowsingContext(aParent, group, id, aType, {});
  }

  // The name and opener fields need to be explicitly initialized. Don't bother
  // using transactions to set them, as we haven't been attached yet.
  context->mFields.SetWithoutSyncing<IDX_Name>(aName);
  if (aOpener) {
    MOZ_DIAGNOSTIC_ASSERT(aOpener->Group() == context->Group());
    MOZ_DIAGNOSTIC_ASSERT(aOpener->mType == context->mType);
    context->mFields.SetWithoutSyncing<IDX_OpenerId>(aOpener->Id());
    context->mFields.SetWithoutSyncing<IDX_HadOriginalOpener>(true);
  }
  context->mFields.SetWithoutSyncing<IDX_EmbedderPolicy>(
      nsILoadInfo::EMBEDDER_POLICY_NULL);
  context->mFields.SetWithoutSyncing<IDX_OpenerPolicy>(
      nsILoadInfo::OPENER_POLICY_UNSAFE_NONE);

  if (aOpener && aOpener->SameOriginWithTop()) {
    // We inherit the opener policy if there is a creator and if the creator's
    // origin is same origin with the creator's top-level origin.
    // If it is cross origin we should not inherit the CrossOriginOpenerPolicy
    context->mFields.SetWithoutSyncing<IDX_OpenerPolicy>(
        aOpener->Top()->GetOpenerPolicy());
  } else if (aOpener) {
    // They are not same origin
    auto topPolicy = aOpener->Top()->GetOpenerPolicy();
    MOZ_RELEASE_ASSERT(topPolicy == nsILoadInfo::OPENER_POLICY_UNSAFE_NONE ||
                       topPolicy ==
                           nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_ALLOW_POPUPS);
  }

  BrowsingContext* inherit = aParent ? aParent : aOpener;
  if (inherit) {
    context->mPrivateBrowsingId = inherit->mPrivateBrowsingId;
    context->mUseRemoteTabs = inherit->mUseRemoteTabs;
    context->mUseRemoteSubframes = inherit->mUseRemoteSubframes;
    context->mOriginAttributes = inherit->mOriginAttributes;

    // CORPP 3.1.3 https://mikewest.github.io/corpp/#integration-html
    context->mFields.SetWithoutSyncing<IDX_EmbedderPolicy>(
        inherit->GetEmbedderPolicy());
    // if our parent has a parent that's loading, we need it too
    bool ancestorLoading = aParent ? aParent->GetAncestorLoading() : false;
    if (!ancestorLoading && aParent) {
      // XXX(farre): Can/Should we check aParent->IsLoading() here? (Bug
      // 1608448) Check if the parent was itself loading already
      nsPIDOMWindowOuter* outer = aParent->GetDOMWindow();
      if (outer) {
        Document* document = nsGlobalWindowOuter::Cast(outer)->GetDocument();
        auto readystate = document->GetReadyStateEnum();
        if (readystate == Document::ReadyState::READYSTATE_LOADING ||
            readystate == Document::ReadyState::READYSTATE_INTERACTIVE) {
          ancestorLoading = true;
        }
      }
    }
    context->mFields.SetWithoutSyncing<IDX_AncestorLoading>(ancestorLoading);
  }

  nsContentUtils::GenerateUUIDInPlace(
      context->mFields.GetNonSyncingReference<IDX_HistoryID>());

  context->mFields.SetWithoutSyncing<IDX_IsActive>(true);

  context->mFields.SetWithoutSyncing<IDX_FullZoom>(aParent ? aParent->FullZoom()
                                                           : 1.0f);
  context->mFields.SetWithoutSyncing<IDX_TextZoom>(aParent ? aParent->TextZoom()
                                                           : 1.0f);

  const bool allowContentRetargeting =
      inherit ? inherit->GetAllowContentRetargetingOnChildren() : true;
  context->mFields.SetWithoutSyncing<IDX_AllowContentRetargeting>(
      allowContentRetargeting);
  context->mFields.SetWithoutSyncing<IDX_AllowContentRetargetingOnChildren>(
      allowContentRetargeting);

  const bool allowPlugins = inherit ? inherit->GetAllowPlugins() : true;
  context->mFields.SetWithoutSyncing<IDX_AllowPlugins>(allowPlugins);

  return context.forget();
}

already_AddRefed<BrowsingContext> BrowsingContext::Create(
    BrowsingContext* aParent, BrowsingContext* aOpener, const nsAString& aName,
    Type aType) {
  RefPtr<BrowsingContext> bc(CreateDetached(aParent, aOpener, aName, aType));
  bc->EnsureAttached();
  return bc.forget();
}

already_AddRefed<BrowsingContext> BrowsingContext::CreateWindowless(
    BrowsingContext* aParent, BrowsingContext* aOpener, const nsAString& aName,
    Type aType) {
  RefPtr<BrowsingContext> bc(CreateDetached(aParent, aOpener, aName, aType));
  bc->mWindowless = true;
  bc->EnsureAttached();
  return bc.forget();
}

void BrowsingContext::SetWindowless() {
  MOZ_DIAGNOSTIC_ASSERT(!mEverAttached);
  mWindowless = true;
}

void BrowsingContext::EnsureAttached() {
  if (!mEverAttached) {
    Register(this);

    // Attach the browsing context to the tree.
    Attach();
  }
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::CreateFromIPC(
    BrowsingContext::IPCInitializer&& aInit, BrowsingContextGroup* aGroup,
    ContentParent* aOriginProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aOriginProcess || XRE_IsContentProcess());
  MOZ_DIAGNOSTIC_ASSERT(aGroup);

  uint64_t originId = 0;
  if (aOriginProcess) {
    originId = aOriginProcess->ChildID();
    aGroup->EnsureSubscribed(aOriginProcess);
  }

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " from IPC (origin=0x%08" PRIx64 ")",
           aInit.mId, originId));

  RefPtr<BrowsingContext> parent = aInit.GetParent();

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

  // NOTE: Call through the `Set` methods for these values to ensure that any
  // relevant process-local state is also updated.
  context->SetOriginAttributes(aInit.mOriginAttributes);
  context->SetRemoteTabs(aInit.mUseRemoteTabs);
  context->SetRemoteSubframes(aInit.mUseRemoteSubframes);
  // NOTE: Private browsing ID is set by `SetOriginAttributes`.

  Register(context);

  // Caller handles attaching us to the tree.

  if (aInit.mCached) {
    context->mEverAttached = true;
  }

  return context.forget();
}

BrowsingContext::BrowsingContext(BrowsingContext* aParent,
                                 BrowsingContextGroup* aGroup,
                                 uint64_t aBrowsingContextId, Type aType,
                                 FieldTuple&& aFields)
    : mFields(std::move(aFields)),
      mType(aType),
      mBrowsingContextId(aBrowsingContextId),
      mGroup(aGroup),
      mParent(aParent),
      mPrivateBrowsingId(0),
      mEverAttached(false),
      mIsInProcess(false),
      mIsDiscarded(false),
      mWindowless(false),
      mDanglingRemoteOuterProxies(false),
      mPendingInitialization(false),
      mEmbeddedByThisProcess(false),
      mUseRemoteTabs(false),
      mUseRemoteSubframes(false) {
  MOZ_RELEASE_ASSERT(!mParent || mParent->Group() == mGroup);
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

void BrowsingContext::SetEmbedderElement(Element* aEmbedder) {
  mEmbeddedByThisProcess = true;

  // Update embedder-element-specific fields in a shared transaction.
  // this when clearing our embedder, as we're being destroyed either way.
  if (aEmbedder) {
    Transaction txn;
    txn.SetEmbedderElementType(Some(aEmbedder->LocalName()));
    if (nsCOMPtr<nsPIDOMWindowInner> inner =
            do_QueryInterface(aEmbedder->GetOwnerGlobal())) {
      txn.SetEmbedderInnerWindowId(inner->WindowID());
    }
    if (XRE_IsParentProcess() && IsTopContent()) {
      nsAutoString messageManagerGroup;
      if (aEmbedder->IsXULElement()) {
        aEmbedder->GetAttr(kNameSpaceID_None, nsGkAtoms::messagemanagergroup,
                           messageManagerGroup);
      }
      txn.SetMessageManagerGroup(messageManagerGroup);
    }
    txn.Commit(this);
  }

  mEmbedderElement = aEmbedder;
}

void BrowsingContext::Embed() {
  if (auto* frame = HTMLIFrameElement::FromNode(mEmbedderElement)) {
    frame->BindToBrowsingContext(this);
  }
}

void BrowsingContext::Attach(bool aFromIPC) {
  MOZ_DIAGNOSTIC_ASSERT(!mEverAttached);
  mEverAttached = true;

  if (MOZ_LOG_TEST(GetLog(), LogLevel::Debug)) {
    nsAutoCString suffix;
    mOriginAttributes.CreateSuffix(suffix);
    MOZ_LOG(GetLog(), LogLevel::Debug,
            ("%s: Connecting 0x%08" PRIx64 " to 0x%08" PRIx64
             " (private=%d, remote=%d, fission=%d, oa=%s)",
             XRE_IsParentProcess() ? "Parent" : "Child", Id(),
             mParent ? mParent->Id() : 0, (int)mPrivateBrowsingId,
             (int)mUseRemoteTabs, (int)mUseRemoteSubframes, suffix.get()));
  }

  MOZ_DIAGNOSTIC_ASSERT(mGroup);
  MOZ_DIAGNOSTIC_ASSERT(!mGroup->IsContextCached(this));
  MOZ_DIAGNOSTIC_ASSERT(!mIsDiscarded);

  Children* children = nullptr;
  if (mParent) {
    children = &mParent->mChildren;
    BrowsingContext_Binding::ClearCachedChildrenValue(mParent);
  } else {
    children = &mGroup->Toplevels();
  }
  MOZ_DIAGNOSTIC_ASSERT(!children->Contains(this));

  children->AppendElement(this);

  if (GetIsPopupSpam()) {
    PopupBlocker::RegisterOpenPopupSpam();
  }

  if (!aFromIPC) {
    // Send attach to our parent if we need to.
    if (XRE_IsContentProcess()) {
      ContentChild::GetSingleton()->SendAttachBrowsingContext(
          GetIPCInitializer());
    } else if (IsContent()) {
      MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
      mGroup->EachParent([&](ContentParent* aParent) {
        Unused << aParent->SendAttachBrowsingContext(GetIPCInitializer());
      });
    }
  }
}

void BrowsingContext::Detach(bool aFromIPC) {
  MOZ_DIAGNOSTIC_ASSERT(mEverAttached);

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Detaching 0x%08" PRIx64 " from 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child", Id(),
           mParent ? mParent->Id() : 0));

  // Unlinking might remove our group before Detach gets called.
  if (NS_WARN_IF(!mGroup)) {
    return;
  }

  if (!mGroup->EvictCachedContext(this)) {
    Children* children = nullptr;
    if (mParent) {
      children = &mParent->mChildren;
      BrowsingContext_Binding::ClearCachedChildrenValue(mParent);
    } else {
      children = &mGroup->Toplevels();
    }

    children->RemoveElement(this);
  }

  if (!mChildren.IsEmpty()) {
    mGroup->CacheContexts(mChildren);
    mChildren.Clear();
    BrowsingContext_Binding::ClearCachedChildrenValue(this);
  }

  {
    // Hold a strong reference to ourself until the responses comes back to
    // ensure we don't die while messages relating to this context are
    // in-flight.
    RefPtr<BrowsingContext> self(this);
    auto callback = [self](auto) {};
    if (XRE_IsParentProcess()) {
      Group()->EachParent([&](ContentParent* aParent) {
        // Only the embedder process is allowed to initiate a BrowsingContext
        // detach, so if we've gotten here, the host process already knows we've
        // been detached, and there's no need to tell it again.
        // If the owner process is not the same as the embedder process, its
        // BrowsingContext will be detached when its nsWebBrowser instance is
        // destroyed.
        if (!Canonical()->IsEmbeddedInProcess(aParent->ChildID()) &&
            !Canonical()->IsOwnedByProcess(aParent->ChildID())) {
          aParent->SendDetachBrowsingContext(Id(), callback, callback);
        }
      });
    } else if (!aFromIPC) {
      ContentChild::GetSingleton()->SendDetachBrowsingContext(Id(), callback,
                                                              callback);
    }
  }

  mGroup->Unregister(this);
  mIsDiscarded = true;

  if (XRE_IsParentProcess()) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      fm->BrowsingContextDetached(this);
    }
  }

  if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
    obs->NotifyObservers(ToSupports(this), "browsing-context-discarded",
                         nullptr);
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

  if (!mWindowProxy) {
    return;
  }

  // We have to go through mWindowProxy rather than calling GetDOMWindow() on
  // mDocShell because the mDocshell reference gets cleared immediately after
  // the window is closed.
  nsGlobalWindowOuter::PrepareForProcessChange(mWindowProxy);
  MOZ_ASSERT(!mWindowProxy);
}

void BrowsingContext::CacheChildren(bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Caching children of 0x%08" PRIx64 "",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  mGroup->CacheContexts(mChildren);
  mChildren.Clear();
  BrowsingContext_Binding::ClearCachedChildrenValue(this);

  if (!aFromIPC && XRE_IsContentProcess()) {
    auto cc = ContentChild::GetSingleton();
    MOZ_DIAGNOSTIC_ASSERT(cc);
    cc->SendCacheBrowsingContextChildren(this);
  }
}

void BrowsingContext::RestoreChildren(Children&& aChildren, bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Restoring children of 0x%08" PRIx64 "",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  nsTArray<MaybeDiscarded<BrowsingContext>> ipcChildren(aChildren.Length());
  for (BrowsingContext* child : aChildren) {
    MOZ_DIAGNOSTIC_ASSERT(child->GetParent() == this);
    Unused << mGroup->EvictCachedContext(child);
    ipcChildren.AppendElement(child);
  }

  mChildren.AppendElements(aChildren);
  BrowsingContext_Binding::ClearCachedChildrenValue(this);

  if (!aFromIPC && XRE_IsContentProcess()) {
    auto cc = ContentChild::GetSingleton();
    MOZ_DIAGNOSTIC_ASSERT(cc);
    cc->SendRestoreBrowsingContextChildren(this, ipcChildren);
  }
}

bool BrowsingContext::IsCached() { return mGroup->IsContextCached(this); }

bool BrowsingContext::IsTargetable() {
  return !GetClosed() && !mIsDiscarded && !IsCached();
}

bool BrowsingContext::HasOpener() const {
  return sBrowsingContexts->Contains(GetOpenerId());
}

void BrowsingContext::GetChildren(Children& aChildren) {
  MOZ_ALWAYS_TRUE(aChildren.AppendElements(mChildren));
}

void BrowsingContext::GetWindowContexts(
    nsTArray<RefPtr<WindowContext>>& aWindows) {
  aWindows.AppendElements(mWindowContexts);
}

void BrowsingContext::RegisterWindowContext(WindowContext* aWindow) {
  MOZ_ASSERT(!mWindowContexts.Contains(aWindow),
             "WindowContext already registered!");
  mWindowContexts.AppendElement(aWindow);
  if (aWindow->InnerWindowId() == GetCurrentInnerWindowId()) {
    MOZ_ASSERT(aWindow->GetBrowsingContext() == this);
    mCurrentWindowContext = aWindow;
  }
}

void BrowsingContext::UnregisterWindowContext(WindowContext* aWindow) {
  MOZ_ASSERT(mWindowContexts.Contains(aWindow),
             "WindowContext not registered!");
  mWindowContexts.RemoveElement(aWindow);

  // Our current window global should be in our mWindowGlobals set. If it's not
  // anymore, clear that reference.
  // FIXME: There are probably situations where this is wrong. We should
  // double-check.
  if (aWindow == mCurrentWindowContext) {
    mCurrentWindowContext = nullptr;
    if (XRE_IsParentProcess()) {
      BrowserParent::UpdateFocusFromBrowsingContext();
    }
  }
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
      Children* siblings;
      BrowsingContext* parent = current->mParent;

      if (!parent) {
        // We've reached the root of the tree, consider browsing
        // contexts in the same browsing context group.
        siblings = &mGroup->Toplevels();
      } else if (parent->NameEquals(aName) &&
                 requestingContext->CanAccess(parent) &&
                 parent->IsTargetable()) {
        found = parent;
        break;
      } else {
        siblings = &parent->mChildren;
      }

      for (BrowsingContext* sibling : *siblings) {
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

  for (BrowsingContext* child : mChildren) {
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
    if (mParent) {
      return aRequestingContext.CanAccess(mParent) ? mParent.get() : nullptr;
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

  for (BrowsingContext* child : mChildren) {
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

BrowsingContext::~BrowsingContext() {
  MOZ_DIAGNOSTIC_ASSERT(!mParent || !mParent->mChildren.Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!mGroup || !mGroup->Toplevels().Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!mGroup || !mGroup->IsContextCached(this));

  mDeprioritizedLoadRunner.clear();

  if (sBrowsingContexts) {
    sBrowsingContexts->Remove(Id());
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

void BrowsingContext::NotifyUserGestureActivation() {
  SetUserActivationState(UserActivation::State::FullActivated);
}

void BrowsingContext::NotifyResetUserGestureActivation() {
  SetUserActivationState(UserActivation::State::None);
}

bool BrowsingContext::HasBeenUserGestureActivated() {
  return GetUserActivationState() != UserActivation::State::None;
}

bool BrowsingContext::HasValidTransientUserGestureActivation() {
  MOZ_ASSERT(mIsInProcess);

  if (GetUserActivationState() != UserActivation::State::FullActivated) {
    MOZ_ASSERT(mUserGestureStart.IsNull(),
               "mUserGestureStart should be null if the document hasn't ever "
               "been activated by user gesture");
    return false;
  }

  MOZ_ASSERT(!mUserGestureStart.IsNull(),
             "mUserGestureStart shouldn't be null if the document has ever "
             "been activated by user gesture");
  TimeDuration timeout = TimeDuration::FromMilliseconds(
      StaticPrefs::dom_user_activation_transient_timeout());

  return timeout <= TimeDuration() ||
         (TimeStamp::Now() - mUserGestureStart) <= timeout;
}

bool BrowsingContext::ConsumeTransientUserGestureActivation() {
  MOZ_ASSERT(mIsInProcess);

  if (!HasValidTransientUserGestureActivation()) {
    return false;
  }

  BrowsingContext* top = Top();
  top->PreOrderWalk([&](BrowsingContext* aContext) {
    if (aContext->GetUserActivationState() ==
        UserActivation::State::FullActivated) {
      aContext->SetUserActivationState(UserActivation::State::HasBeenActivated);
    }
  });

  return true;
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
  RefPtr<Element> topFrameElement = Top()->GetEmbedderElement();
  topFrameElement.forget(aTopFrameElement);
  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::GetNestedFrameId(uint64_t* aNestedFrameId) {
  // FIXME: nestedFrameId should be removed, as it was only used by B2G.
  *aNestedFrameId = 0;
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

  if (mParent) {
    return mParent->GetUseTrackingProtection(aUseTrackingProtection);
  }

  return NS_OK;
}

NS_IMETHODIMP BrowsingContext::SetUseTrackingProtection(
    bool aUseTrackingProtection) {
  SetForceEnableTrackingProtection(aUseTrackingProtection);
  return NS_OK;
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

  if (tmp->GetIsPopupSpam()) {
    PopupBlocker::UnregisterOpenPopupSpam();
    // NOTE: Doesn't use SetIsPopupSpam, as it will be set all processes
    // automatically.
    tmp->mFields.SetWithoutSyncing<IDX_IsPopupSpam>(false);
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell, mChildren, mParent, mGroup,
                                  mEmbedderElement, mWindowContexts,
                                  mCurrentWindowContext, mSessionStorageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(
      mDocShell, mChildren, mParent, mGroup, mEmbedderElement, mWindowContexts,
      mCurrentWindowContext, mSessionStorageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

class RemoteLocationProxy
    : public RemoteObjectProxy<BrowsingContext::LocationProxy,
                               Location_Binding::sCrossOriginAttributes,
                               Location_Binding::sCrossOriginMethods> {
 public:
  typedef RemoteObjectProxy Base;

  constexpr RemoteLocationProxy()
      : RemoteObjectProxy(prototypes::id::Location) {}

  void NoteChildren(JSObject* aProxy,
                    nsCycleCollectionTraversalCallback& aCb) const override {
    auto location =
        static_cast<BrowsingContext::LocationProxy*>(GetNative(aProxy));
    CycleCollectionNoteChild(aCb, location->GetBrowsingContext(),
                             "js::GetObjectPrivate(obj)->GetBrowsingContext()");
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

nsresult BrowsingContext::CheckSandboxFlags(nsDocShellLoadState* aLoadState) {
  const auto& sourceBC = aLoadState->SourceBrowsingContext();
  if (sourceBC.IsDiscarded() || (sourceBC && sourceBC->IsSandboxedFrom(this))) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
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

  if (mDocShell) {
    return mDocShell->LoadURI(aLoadState, aSetNavigating);
  }

  // Note: We do this check both here and in `nsDocShell::InternalLoad`, since
  // document-specific sandbox flags are only available in the process
  // triggering the load, and we don't want the target process to have to trust
  // the triggering process to do the appropriate checks for the
  // BrowsingContext's sandbox flags.
  MOZ_TRY(CheckSandboxFlags(aLoadState));

  const auto& sourceBC = aLoadState->SourceBrowsingContext();
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
  } else {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
    if (!XRE_IsParentProcess()) {
      return NS_ERROR_UNEXPECTED;
    }

    if (ContentParent* cp = Canonical()->GetContentParent()) {
      cp->TransmitBlobDataIfBlobURL(aLoadState->URI(),
                                    aLoadState->TriggeringPrincipal());
      Unused << cp->SendLoadURI(this, aLoadState, aSetNavigating);
    }
  }
  return NS_OK;
}

nsresult BrowsingContext::InternalLoad(nsDocShellLoadState* aLoadState,
                                       nsIDocShell** aDocShell,
                                       nsIRequest** aRequest) {
  if (IsDiscarded()) {
    return NS_OK;
  }

  const auto& sourceBC = aLoadState->SourceBrowsingContext();
  bool isActive =
      sourceBC && sourceBC->GetIsActive() && !GetIsActive() &&
      !Preferences::GetBool("browser.tabs.loadDivertedInBackground", false);
  if (mDocShell) {
    nsresult rv = nsDocShell::Cast(mDocShell)->InternalLoad(
        aLoadState, aDocShell, aRequest);
    NS_ENSURE_SUCCESS(rv, rv);

    // Switch to target tab if we're currently focused window.
    // Take loadDivertedInBackground into account so the behavior would be
    // the same as how the tab first opened.
    nsCOMPtr<nsPIDOMWindowOuter> domWin = GetDOMWindow();
    if (isActive && domWin) {
      nsFocusManager::FocusWindow(domWin, CallerType::System);
    }

    // Else we ran out of memory, or were a popup and got blocked,
    // or something.

    return rv;
  }

  // Note: We do this check both here and in `nsDocShell::InternalLoad`, since
  // document-specific sandbox flags are only available in the process
  // triggering the load, and we don't want the target process to have to trust
  // the triggering process to do the appropriate checks for the
  // BrowsingContext's sandbox flags.
  MOZ_TRY(CheckSandboxFlags(aLoadState));

  if (XRE_IsParentProcess()) {
    if (ContentParent* cp = Canonical()->GetContentParent()) {
      Unused << cp->SendInternalLoad(this, aLoadState, isActive);
    }
  } else {
    MOZ_DIAGNOSTIC_ASSERT(sourceBC);
    MOZ_DIAGNOSTIC_ASSERT(sourceBC->Group() == Group());

    if (!sourceBC->CanAccess(this)) {
      return NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win(sourceBC->GetDOMWindow());
    if (WindowGlobalChild* wgc =
            win->GetCurrentInnerWindow()->GetWindowGlobalChild()) {
      wgc->SendInternalLoad(this, aLoadState);
    }
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
  // FIXME We need to set the Closed field, but only once we're sending the
  //       DOMWindowClose event (which happens in the process where the
  //       document for this browsing context is loaded).
  //       See https://bugzilla.mozilla.org/show_bug.cgi?id=1516343.
  if (GetDOMWindow()) {
    nsGlobalWindowOuter::Cast(GetDOMWindow())
        ->CloseOuter(aCallerType == CallerType::System);
  } else if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowClose(this, aCallerType == CallerType::System);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowClose(this, aCallerType == CallerType::System);
  }
}

void BrowsingContext::Focus(CallerType aCallerType, ErrorResult& aError) {
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowFocus(this, aCallerType);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowFocus(this, aCallerType);
  }
}

void BrowsingContext::Blur(ErrorResult& aError) {
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowBlur(this);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowBlur(this);
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

Nullable<WindowProxyHolder> BrowsingContext::GetParent(ErrorResult& aError) {
  if (mIsDiscarded) {
    return nullptr;
  }

  // We never throw an error, but the implementation in nsGlobalWindow does and
  // we need to use the same signature.
  if (!mParent) {
    return WindowProxyHolder(this);
  }
  return WindowProxyHolder(mParent.get());
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

  ipc::StructuredCloneData message;
  message.Write(aCx, aMessage, transferArray, JS::CloneDataPolicy(), aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  ClonedMessageData messageData;
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    if (!message.BuildClonedMessageDataForChild(cc, messageData)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    cc->SendWindowPostMessage(this, messageData, data);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    if (!message.BuildClonedMessageDataForParent(cp, messageData)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
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
  init.mParentId = mParent ? mParent->Id() : 0;
  init.mCached = IsCached();
  init.mWindowless = mWindowless;
  init.mUseRemoteTabs = mUseRemoteTabs;
  init.mUseRemoteSubframes = mUseRemoteSubframes;
  init.mOriginAttributes = mOriginAttributes;
  init.mFields = mFields.Fields();
  return init;
}

already_AddRefed<BrowsingContext> BrowsingContext::IPCInitializer::GetParent() {
  RefPtr<BrowsingContext> parent;
  if (mParentId != 0) {
    parent = BrowsingContext::Get(mParentId);
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

void BrowsingContext::ResetGVAutoplayRequestStatus() {
  MOZ_ASSERT(IsTop(),
             "Should only set GVAudibleAutoplayRequestStatus in the top-level "
             "browsing context");
  SetGVAudibleAutoplayRequestStatus(GVAutoplayRequestStatus::eUNKNOWN);
  SetGVInaudibleAutoplayRequestStatus(GVAutoplayRequestStatus::eUNKNOWN);
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

void BrowsingContext::DidSet(FieldIndex<IDX_UserActivationState>) {
  MOZ_ASSERT_IF(!mIsInProcess, mUserGestureStart.IsNull());
  USER_ACTIVATION_LOG("Set user gesture activation %" PRIu8
                      " for %s browsing context 0x%08" PRIx64,
                      static_cast<uint8_t>(GetUserActivationState()),
                      XRE_IsParentProcess() ? "Parent" : "Child", Id());
  if (mIsInProcess) {
    USER_ACTIVATION_LOG(
        "Set user gesture start time for %s browsing context 0x%08" PRIx64,
        XRE_IsParentProcess() ? "Parent" : "Child", Id());
    mUserGestureStart =
        (GetUserActivationState() == UserActivation::State::FullActivated)
            ? TimeStamp::Now()
            : TimeStamp();
  }
}

void BrowsingContext::DidSet(FieldIndex<IDX_Muted>) {
  MOZ_ASSERT(!mParent, "Set muted flag on non top-level context!");
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

void BrowsingContext::SetAllowContentRetargeting(
    bool aAllowContentRetargeting) {
  Transaction txn;
  txn.SetAllowContentRetargeting(aAllowContentRetargeting);
  txn.SetAllowContentRetargetingOnChildren(aAllowContentRetargeting);
  txn.Commit(this);
}

void BrowsingContext::SetCustomUserAgent(const nsAString& aUserAgent) {
  Top()->SetUserAgentOverride(aUserAgent);
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

bool BrowsingContext::CheckOnlyOwningProcessCanSet(ContentParent* aSource) {
  if (aSource) {
    MOZ_ASSERT(XRE_IsParentProcess());

    // Double-check ownership if we aren't the setter.
    if (!Canonical()->IsOwnedByProcess(aSource->ChildID()) &&
        aSource->ChildID() != Canonical()->GetInFlightProcessId()) {
      return false;
    }
  } else if (!IsInProcess() && !XRE_IsParentProcess()) {
    // Don't allow this to be set from content processes that
    // don't own the BrowsingContext.
    return false;
  }

  return true;
}

bool BrowsingContext::CanSet(FieldIndex<IDX_AllowContentRetargeting>,
                             const bool& aAllowContentRetargeting,
                             ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool BrowsingContext::CanSet(FieldIndex<IDX_AllowContentRetargetingOnChildren>,
                             const bool& aAllowContentRetargetingOnChildren,
                             ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool BrowsingContext::CanSet(FieldIndex<IDX_AllowPlugins>,
                             const bool& aAllowPlugins,
                             ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool BrowsingContext::CanSet(FieldIndex<IDX_UserAgentOverride>,
                             const nsString& aUserAgent,
                             ContentParent* aSource) {
  if (!IsTop()) {
    return false;
  }

  return CheckOnlyOwningProcessCanSet(aSource);
}

bool BrowsingContext::CheckOnlyEmbedderCanSet(ContentParent* aSource) {
  if (aSource) {
    // Set by a content process, verify that it's this BC's embedder.
    MOZ_ASSERT(XRE_IsParentProcess());
    return Canonical()->IsEmbeddedInProcess(aSource->ChildID());
  }

  // In-process case, verify that we've been embedded in this process.
  return mEmbeddedByThisProcess;
}

bool BrowsingContext::CanSet(FieldIndex<IDX_EmbedderInnerWindowId>,
                             const uint64_t& aValue, ContentParent* aSource) {
  // Generally allow clearing this. We may want to be more precise about this
  // check in the future.
  if (aValue == 0) {
    return true;
  }

  // If we don't have a specified source, we're the setting process. The window
  // which we're setting this to must be in-process.
  RefPtr<BrowsingContext> impliedParent;
  if (!aSource) {
    nsGlobalWindowInner* innerWindow =
        nsGlobalWindowInner::GetInnerWindowWithId(aValue);
    if (NS_WARN_IF(!innerWindow)) {
      return false;
    }

    impliedParent = innerWindow->GetBrowsingContext();
  }

  // If in the parent process, double-check ownership and WindowGlobalParent as
  // well.
  if (XRE_IsParentProcess()) {
    RefPtr<WindowGlobalParent> wgp =
        WindowGlobalParent::GetByInnerWindowId(aValue);
    if (NS_WARN_IF(!wgp)) {
      return false;
    }

    // Deduce the implied parent from the WindowGlobalParent actor.
    if (impliedParent) {
      MOZ_ASSERT(impliedParent == wgp->BrowsingContext());
    }
    impliedParent = wgp->BrowsingContext();

    // Double-check ownership if we aren't the setter.
    if (aSource &&
        !impliedParent->Canonical()->IsOwnedByProcess(aSource->ChildID()) &&
        aSource->ChildID() !=
            impliedParent->Canonical()->GetInFlightProcessId()) {
      return false;
    }
  }

  // If we would have an invalid implied parent, something has gone wrong.
  MOZ_ASSERT(impliedParent);
  if (NS_WARN_IF(mParent && mParent != impliedParent)) {
    return false;
  }

  return true;
}

bool BrowsingContext::CanSet(FieldIndex<IDX_EmbedderElementType>,
                             const Maybe<nsString>&, ContentParent* aSource) {
  return CheckOnlyEmbedderCanSet(aSource);
}

bool BrowsingContext::CanSet(FieldIndex<IDX_CurrentInnerWindowId>,
                             const uint64_t& aValue, ContentParent* aSource) {
  // Generally allow clearing this. We may want to be more precise about this
  // check in the future.
  if (aValue == 0) {
    return true;
  }

  if (aSource) {
    MOZ_ASSERT(XRE_IsParentProcess());

    // If in the parent process, double-check ownership and WindowGlobalParent
    // as well.
    RefPtr<WindowGlobalParent> wgp =
        WindowGlobalParent::GetByInnerWindowId(aValue);
    if (NS_WARN_IF(!wgp) || NS_WARN_IF(wgp->BrowsingContext() != this)) {
      return false;
    }

    // Double-check ownership if we aren't the setter.
    if (!Canonical()->IsOwnedByProcess(aSource->ChildID()) &&
        aSource->ChildID() != Canonical()->GetInFlightProcessId()) {
      return false;
    }
  }

  // We must have access to the specified context.
  RefPtr<WindowContext> window = WindowContext::GetById(aValue);
  return window && window->GetBrowsingContext() == this;
}

void BrowsingContext::DidSet(FieldIndex<IDX_CurrentInnerWindowId>) {
  mCurrentWindowContext = WindowContext::GetById(GetCurrentInnerWindowId());
  if (XRE_IsParentProcess()) {
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

void BrowsingContext::DidSet(FieldIndex<IDX_TextZoom>, float aOldValue) {
  if (!IsInProcess() || GetTextZoom() == aOldValue) {
    return;
  }

  RefPtr<Document> doc;
  if (auto* win = GetDOMWindow()) {
    doc = win->GetExtantDoc();
  }

  if (nsIDocShell* shell = GetDocShell()) {
    if (nsPresContext* pc = shell->GetPresContext()) {
      pc->RecomputeBrowsingContextDependentData();
    }
  }

  for (BrowsingContext* child : GetChildren()) {
    child->SetTextZoom(GetTextZoom());
  }

  if (doc) {
    nsContentUtils::DispatchChromeEvent(doc, ToSupports(doc),
                                        NS_LITERAL_STRING("TextZoomChange"),
                                        CanBubble::eYes, Cancelable::eYes);
  }
}

// TODO(emilio): It'd be potentially nicer and cheaper to allow to set this only
// on the Top() browsing context, but there are a lot of tests that rely on
// zooming a subframe so...
void BrowsingContext::DidSet(FieldIndex<IDX_FullZoom>, float aOldValue) {
  if (!IsInProcess() || GetFullZoom() == aOldValue) {
    return;
  }

  RefPtr<Document> doc;
  if (auto* win = GetDOMWindow()) {
    doc = win->GetExtantDoc();
  }

  if (doc) {
    nsContentUtils::DispatchChromeEvent(doc, ToSupports(doc),
                                        NS_LITERAL_STRING("PreFullZoomChange"),
                                        CanBubble::eYes, Cancelable::eYes);
  }

  if (nsIDocShell* shell = GetDocShell()) {
    if (nsPresContext* pc = shell->GetPresContext()) {
      pc->RecomputeBrowsingContextDependentData();
    }
  }

  for (BrowsingContext* child : GetChildren()) {
    child->SetFullZoom(GetFullZoom());
  }

  if (doc) {
    nsContentUtils::DispatchChromeEvent(doc, ToSupports(doc),
                                        NS_LITERAL_STRING("FullZoomChange"),
                                        CanBubble::eYes, Cancelable::eYes);
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
  WriteIPDLParam(aMessage, aActor, aInit.mCached);
  WriteIPDLParam(aMessage, aActor, aInit.mWindowless);
  WriteIPDLParam(aMessage, aActor, aInit.mUseRemoteTabs);
  WriteIPDLParam(aMessage, aActor, aInit.mUseRemoteSubframes);
  WriteIPDLParam(aMessage, aActor, aInit.mOriginAttributes);
  WriteIPDLParam(aMessage, aActor, aInit.mFields);
}

bool IPDLParamTraits<dom::BrowsingContext::IPCInitializer>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::BrowsingContext::IPCInitializer* aInit) {
  // Read actor ID parameters.
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mId) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mParentId) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mCached) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mWindowless) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mUseRemoteTabs) ||
      !ReadIPDLParam(aMessage, aIterator, aActor,
                     &aInit->mUseRemoteSubframes) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mOriginAttributes) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mFields)) {
    return false;
  }
  return true;
}

template struct IPDLParamTraits<dom::BrowsingContext::BaseTransaction>;

}  // namespace ipc
}  // namespace mozilla
