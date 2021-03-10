/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/SyncedContextInlines.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/UserActivationIPCUtils.h"
#include "mozilla/PermissionDelegateIPCUtils.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsGlobalWindowInner.h"
#include "nsIScriptError.h"
#include "nsRefPtrHashtable.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

// Explicit specialization of the `Transaction` type. Required by the `extern
// template class` declaration in the header.
template class syncedcontext::Transaction<WindowContext>;

static LazyLogModule gWindowContextLog("WindowContext");
static LazyLogModule gWindowContextSyncLog("WindowContextSync");

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

using WindowContextByIdMap = nsTHashMap<nsUint64HashKey, WindowContext*>;
static StaticAutoPtr<WindowContextByIdMap> gWindowContexts;

/* static */
LogModule* WindowContext::GetLog() { return gWindowContextLog; }

/* static */
LogModule* WindowContext::GetSyncLog() { return gWindowContextSyncLog; }

/* static */
already_AddRefed<WindowContext> WindowContext::GetById(
    uint64_t aInnerWindowId) {
  if (!gWindowContexts) {
    return nullptr;
  }
  return do_AddRef(gWindowContexts->Get(aInnerWindowId));
}

BrowsingContextGroup* WindowContext::Group() const {
  return mBrowsingContext->Group();
}

WindowGlobalParent* WindowContext::Canonical() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<WindowGlobalParent*>(this);
}

bool WindowContext::IsCached() const {
  return mBrowsingContext->mCurrentWindowContext != this;
}

nsGlobalWindowInner* WindowContext::GetInnerWindow() const {
  if (mInProcess) {
    // FIXME: Replace this with something more efficient.
    return nsGlobalWindowInner::GetInnerWindowWithId(mInnerWindowId);
  }
  return nullptr;
}

Document* WindowContext::GetDocument() const {
  nsGlobalWindowInner* innerWindow = GetInnerWindow();
  return innerWindow ? innerWindow->GetDocument() : nullptr;
}

Document* WindowContext::GetExtantDoc() const {
  nsGlobalWindowInner* innerWindow = GetInnerWindow();
  return innerWindow ? innerWindow->GetExtantDoc() : nullptr;
}

WindowGlobalChild* WindowContext::GetWindowGlobalChild() const {
  MOZ_ASSERT(XRE_IsContentProcess());
  NS_ENSURE_TRUE(XRE_IsContentProcess(), nullptr);
  nsGlobalWindowInner* innerWindow = GetInnerWindow();
  return innerWindow ? innerWindow->GetWindowGlobalChild() : nullptr;
}

WindowContext* WindowContext::GetParentWindowContext() {
  return mBrowsingContext->GetParentWindowContext();
}

WindowContext* WindowContext::TopWindowContext() {
  WindowContext* current = this;
  while (current->GetParentWindowContext()) {
    current = current->GetParentWindowContext();
  }
  return current;
}

bool WindowContext::IsTop() const { return mBrowsingContext->IsTop(); }

bool WindowContext::SameOriginWithTop() const {
  return mBrowsingContext->SameOriginWithTop();
}

nsIGlobalObject* WindowContext::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

void WindowContext::AppendChildBrowsingContext(
    BrowsingContext* aBrowsingContext) {
  MOZ_DIAGNOSTIC_ASSERT(Group() == aBrowsingContext->Group(),
                        "Mismatched groups?");
  MOZ_DIAGNOSTIC_ASSERT(!mChildren.Contains(aBrowsingContext));

  mChildren.AppendElement(aBrowsingContext);

  // If we're the current WindowContext in our BrowsingContext, make sure to
  // clear any cached `children` value.
  if (!IsCached()) {
    BrowsingContext_Binding::ClearCachedChildrenValue(mBrowsingContext);
  }
}

void WindowContext::RemoveChildBrowsingContext(
    BrowsingContext* aBrowsingContext) {
  MOZ_DIAGNOSTIC_ASSERT(Group() == aBrowsingContext->Group(),
                        "Mismatched groups?");

  mChildren.RemoveElement(aBrowsingContext);

  // If we're the current WindowContext in our BrowsingContext, make sure to
  // clear any cached `children` value.
  if (!IsCached()) {
    BrowsingContext_Binding::ClearCachedChildrenValue(mBrowsingContext);
  }
}

void WindowContext::SendCommitTransaction(ContentParent* aParent,
                                          const BaseTransaction& aTxn,
                                          uint64_t aEpoch) {
  Unused << aParent->SendCommitWindowContextTransaction(this, aTxn, aEpoch);
}

void WindowContext::SendCommitTransaction(ContentChild* aChild,
                                          const BaseTransaction& aTxn,
                                          uint64_t aEpoch) {
  aChild->SendCommitWindowContextTransaction(this, aTxn, aEpoch);
}

bool WindowContext::CheckOnlyOwningProcessCanSet(ContentParent* aSource) {
  if (mInProcess) {
    return true;
  }

  if (XRE_IsParentProcess() && aSource) {
    return Canonical()->GetContentParent() == aSource;
  }

  return false;
}

bool WindowContext::CanSet(FieldIndex<IDX_IsSecure>, const bool& aIsSecure,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_AllowMixedContent>,
                           const bool& aAllowMixedContent,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_HasBeforeUnload>,
                           const bool& aHasBeforeUnload,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_CookieBehavior>,
                           const Maybe<uint32_t>& aValue,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_IsOnContentBlockingAllowList>,
                           const bool& aValue, ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_IsThirdPartyWindow>,
                           const bool& IsThirdPartyWindow,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_IsThirdPartyTrackingResourceWindow>,
                           const bool& aIsThirdPartyTrackingResourceWindow,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_IsSecureContext>,
                           const bool& aIsSecureContext,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_IsOriginalFrameSource>,
                           const bool& aIsOriginalFrameSource,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_DocTreeHadMedia>, const bool& aValue,
                           ContentParent* aSource) {
  return IsTop();
}

bool WindowContext::CanSet(FieldIndex<IDX_AutoplayPermission>,
                           const uint32_t& aValue, ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_ShortcutsPermission>,
                           const uint32_t& aValue, ContentParent* aSource) {
  return IsTop() && CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_ActiveMediaSessionContextId>,
                           const Maybe<uint64_t>& aValue,
                           ContentParent* aSource) {
  return IsTop();
}

bool WindowContext::CanSet(FieldIndex<IDX_PopupPermission>, const uint32_t&,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(
    FieldIndex<IDX_DelegatedPermissions>,
    const PermissionDelegateHandler::DelegatedPermissionList& aValue,
    ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(
    FieldIndex<IDX_DelegatedExactHostMatchPermissions>,
    const PermissionDelegateHandler::DelegatedPermissionList& aValue,
    ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_IsLocalIP>, const bool& aValue,
                           ContentParent* aSource) {
  return CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_HadLazyLoadImage>, const bool& aValue,
                           ContentParent* aSource) {
  return IsTop();
}

void WindowContext::DidSet(FieldIndex<IDX_SHEntryHasUserInteraction>,
                           bool aOldValue) {
  MOZ_ASSERT(
      TopWindowContext() == this,
      "SHEntryHasUserInteraction can only be set on the top window context");
  // This field is set when the child notifies us of new user interaction, so we
  // also set the currently active shentry in the parent as having interaction.
  if (XRE_IsParentProcess() && mBrowsingContext) {
    SessionHistoryEntry* activeEntry =
        mBrowsingContext->Canonical()->GetActiveSessionHistoryEntry();
    if (activeEntry && GetSHEntryHasUserInteraction()) {
      activeEntry->SetHasUserInteraction(true);
    }
  }
}

void WindowContext::DidSet(FieldIndex<IDX_UserActivationState>) {
  MOZ_ASSERT_IF(!mInProcess, mUserGestureStart.IsNull());
  USER_ACTIVATION_LOG("Set user gesture activation %" PRIu8
                      " for %s browsing context 0x%08" PRIx64,
                      static_cast<uint8_t>(GetUserActivationState()),
                      XRE_IsParentProcess() ? "Parent" : "Child", Id());
  if (mInProcess) {
    USER_ACTIVATION_LOG(
        "Set user gesture start time for %s browsing context 0x%08" PRIx64,
        XRE_IsParentProcess() ? "Parent" : "Child", Id());
    mUserGestureStart =
        (GetUserActivationState() == UserActivation::State::FullActivated)
            ? TimeStamp::Now()
            : TimeStamp();
  }
}

void WindowContext::DidSet(FieldIndex<IDX_HasReportedShadowDOMUsage>,
                           bool aOldValue) {
  if (!aOldValue && GetHasReportedShadowDOMUsage() && IsInProcess()) {
    MOZ_ASSERT(TopWindowContext() == this);
    if (mBrowsingContext) {
      Document* topLevelDoc = mBrowsingContext->GetDocument();
      if (topLevelDoc) {
        nsAutoString uri;
        Unused << topLevelDoc->GetDocumentURI(uri);
        if (!uri.IsEmpty()) {
          nsAutoString msg = u"Shadow DOM used in ["_ns + uri +
                             u"] or in some of its subdocuments."_ns;
          nsContentUtils::ReportToConsoleNonLocalized(
              msg, nsIScriptError::infoFlag, "DOM"_ns, topLevelDoc);
        }
      }
    }
  }
}

void WindowContext::CreateFromIPC(IPCInitializer&& aInit) {
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess(),
                     "Should be a WindowGlobalParent in the parent");

  RefPtr<BrowsingContext> bc = BrowsingContext::Get(aInit.mBrowsingContextId);
  MOZ_RELEASE_ASSERT(bc);

  if (bc->IsDiscarded()) {
    // If we have already closed our browsing context, the
    // WindowGlobalChild actor is bound to be destroyed soon and it's
    // safe to ignore creating the WindowContext.
    return;
  }

  RefPtr<WindowContext> context =
      new WindowContext(bc, aInit.mInnerWindowId, aInit.mOuterWindowId,
                        /* aInProcess */ false, std::move(aInit.mFields));
  context->Init();
}

void WindowContext::Init() {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Registering 0x%" PRIx64 " (bc=0x%" PRIx64 ")", mInnerWindowId,
           mBrowsingContext->Id()));

  // Register the WindowContext in the `WindowContextByIdMap`.
  if (!gWindowContexts) {
    gWindowContexts = new WindowContextByIdMap();
    ClearOnShutdown(&gWindowContexts);
  }
  auto& entry = gWindowContexts->LookupOrInsert(mInnerWindowId);
  MOZ_RELEASE_ASSERT(!entry, "Duplicate WindowContext for ID!");
  entry = this;

  // Register this to the browsing context.
  mBrowsingContext->RegisterWindowContext(this);
  Group()->Register(this);
}

void WindowContext::Discard() {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Discarding 0x%" PRIx64 " (bc=0x%" PRIx64 ")", mInnerWindowId,
           mBrowsingContext->Id()));
  if (mIsDiscarded) {
    return;
  }

  mIsDiscarded = true;
  if (gWindowContexts) {
    gWindowContexts->Remove(InnerWindowId());
  }
  mBrowsingContext->UnregisterWindowContext(this);
  Group()->Unregister(this);
}

void WindowContext::AddSecurityState(uint32_t aStateFlags) {
  MOZ_ASSERT(TopWindowContext() == this);
  MOZ_ASSERT((aStateFlags &
              (nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT |
               nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT |
               nsIWebProgressListener::STATE_BLOCKED_MIXED_DISPLAY_CONTENT |
               nsIWebProgressListener::STATE_BLOCKED_MIXED_ACTIVE_CONTENT |
               nsIWebProgressListener::STATE_HTTPS_ONLY_MODE_UPGRADED |
               nsIWebProgressListener::STATE_HTTPS_ONLY_MODE_UPGRADE_FAILED)) ==
                 aStateFlags,
             "Invalid flags specified!");

  if (XRE_IsParentProcess()) {
    Canonical()->AddSecurityState(aStateFlags);
  } else {
    ContentChild* child = ContentChild::GetSingleton();
    child->SendAddSecurityState(this, aStateFlags);
  }
}

void WindowContext::NotifyUserGestureActivation() {
  Unused << SetUserActivationState(UserActivation::State::FullActivated);
}

void WindowContext::NotifyResetUserGestureActivation() {
  Unused << SetUserActivationState(UserActivation::State::None);
}

bool WindowContext::HasBeenUserGestureActivated() {
  return GetUserActivationState() != UserActivation::State::None;
}

bool WindowContext::HasValidTransientUserGestureActivation() {
  MOZ_ASSERT(mInProcess);

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

bool WindowContext::ConsumeTransientUserGestureActivation() {
  MOZ_ASSERT(mInProcess);
  MOZ_ASSERT(!IsCached());

  if (!HasValidTransientUserGestureActivation()) {
    return false;
  }

  BrowsingContext* top = mBrowsingContext->Top();
  top->PreOrderWalk([&](BrowsingContext* aBrowsingContext) {
    WindowContext* windowContext = aBrowsingContext->GetCurrentWindowContext();
    if (windowContext && windowContext->GetUserActivationState() ==
                             UserActivation::State::FullActivated) {
      Unused << windowContext->SetUserActivationState(
          UserActivation::State::HasBeenActivated);
    }
  });

  return true;
}

bool WindowContext::CanShowPopup() {
  uint32_t permit = GetPopupPermission();
  if (permit == nsIPermissionManager::ALLOW_ACTION) {
    return true;
  }
  if (permit == nsIPermissionManager::DENY_ACTION) {
    return false;
  }

  return !StaticPrefs::dom_disable_open_during_load();
}

WindowContext::IPCInitializer WindowContext::GetIPCInitializer() {
  IPCInitializer init;
  init.mInnerWindowId = mInnerWindowId;
  init.mOuterWindowId = mOuterWindowId;
  init.mBrowsingContextId = mBrowsingContext->Id();
  init.mFields = mFields.RawValues();
  return init;
}

WindowContext::WindowContext(BrowsingContext* aBrowsingContext,
                             uint64_t aInnerWindowId, uint64_t aOuterWindowId,
                             bool aInProcess, FieldValues&& aInit)
    : mFields(std::move(aInit)),
      mInnerWindowId(aInnerWindowId),
      mOuterWindowId(aOuterWindowId),
      mBrowsingContext(aBrowsingContext),
      mInProcess(aInProcess) {
  MOZ_ASSERT(mBrowsingContext);
  MOZ_ASSERT(mInnerWindowId);
  MOZ_ASSERT(mOuterWindowId);
}

WindowContext::~WindowContext() {
  if (gWindowContexts) {
    gWindowContexts->Remove(InnerWindowId());
  }
}

JSObject* WindowContext::WrapObject(JSContext* cx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return WindowContext_Binding::Wrap(cx, this, aGivenProto);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WindowContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WindowContext)

NS_IMPL_CYCLE_COLLECTION_CLASS(WindowContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WindowContext)
  if (gWindowContexts) {
    gWindowContexts->Remove(tmp->InnerWindowId());
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildren)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WindowContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildren)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(WindowContext)

}  // namespace dom

namespace ipc {

void IPDLParamTraits<dom::MaybeDiscarded<dom::WindowContext>>::Write(
    IPC::Message* aMsg, IProtocol* aActor,
    const dom::MaybeDiscarded<dom::WindowContext>& aParam) {
  uint64_t id = aParam.ContextId();
  WriteIPDLParam(aMsg, aActor, id);
}

bool IPDLParamTraits<dom::MaybeDiscarded<dom::WindowContext>>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    dom::MaybeDiscarded<dom::WindowContext>* aResult) {
  uint64_t id = 0;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &id)) {
    return false;
  }

  if (id == 0) {
    *aResult = nullptr;
  } else if (RefPtr<dom::WindowContext> wc = dom::WindowContext::GetById(id)) {
    *aResult = std::move(wc);
  } else {
    aResult->SetDiscarded(id);
  }
  return true;
}

void IPDLParamTraits<dom::WindowContext::IPCInitializer>::Write(
    IPC::Message* aMessage, IProtocol* aActor,
    const dom::WindowContext::IPCInitializer& aInit) {
  // Write actor ID parameters.
  WriteIPDLParam(aMessage, aActor, aInit.mInnerWindowId);
  WriteIPDLParam(aMessage, aActor, aInit.mOuterWindowId);
  WriteIPDLParam(aMessage, aActor, aInit.mBrowsingContextId);
  WriteIPDLParam(aMessage, aActor, aInit.mFields);
}

bool IPDLParamTraits<dom::WindowContext::IPCInitializer>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::WindowContext::IPCInitializer* aInit) {
  // Read actor ID parameters.
  return ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mInnerWindowId) &&
         ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mOuterWindowId) &&
         ReadIPDLParam(aMessage, aIterator, aActor,
                       &aInit->mBrowsingContextId) &&
         ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mFields);
}

template struct IPDLParamTraits<dom::WindowContext::BaseTransaction>;

}  // namespace ipc
}  // namespace mozilla
