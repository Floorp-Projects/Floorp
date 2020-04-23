/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/SyncedContextInlines.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace dom {

// Explicit specialization of the `Transaction` type. Required by the `extern
// template class` declaration in the header.
template class syncedcontext::Transaction<WindowContext>;

static LazyLogModule gWindowContextLog("WindowContext");

using WindowContextByIdMap = nsDataHashtable<nsUint64HashKey, WindowContext*>;
static StaticAutoPtr<WindowContextByIdMap> gWindowContexts;

/* static */
LogModule* WindowContext::GetLog() { return gWindowContextLog; }

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

nsIGlobalObject* WindowContext::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
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

bool WindowContext::CanSet(FieldIndex<IDX_IsThirdPartyWindow>,
                           const bool& IsThirdPartyWindow,
                           ContentParent* aSource) {
  return mBrowsingContext->CheckOnlyOwningProcessCanSet(aSource);
}

bool WindowContext::CanSet(FieldIndex<IDX_IsThirdPartyTrackingResourceWindow>,
                           const bool& aIsThirdPartyTrackingResourceWindow,
                           ContentParent* aSource) {
  return mBrowsingContext->CheckOnlyOwningProcessCanSet(aSource);
}

already_AddRefed<WindowContext> WindowContext::Create(
    WindowGlobalChild* aWindow) {
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess(),
                     "Should be a WindowGlobalParent in the parent");

  FieldTuple init;
  mozilla::Get<IDX_OuterWindowId>(init) = aWindow->OuterWindowId();
  RefPtr<WindowContext> context = new WindowContext(
      aWindow->BrowsingContext(), aWindow->InnerWindowId(), std::move(init));
  context->Init();
  return context.forget();
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
      new WindowContext(bc, aInit.mInnerWindowId, std::move(aInit.mFields));
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
  auto& entry = gWindowContexts->GetOrInsert(mInnerWindowId);
  MOZ_RELEASE_ASSERT(!entry, "Duplicate WindowContext for ID!");
  entry = this;

  // Register this to the browsing context.
  mBrowsingContext->RegisterWindowContext(this);
}

void WindowContext::Discard() {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Discarding 0x%" PRIx64 " (bc=0x%" PRIx64 ")", mInnerWindowId,
           mBrowsingContext->Id()));
  if (mIsDiscarded) {
    return;
  }

  mBrowsingContext->UnregisterWindowContext(this);
  gWindowContexts->Remove(InnerWindowId());
  mIsDiscarded = true;
}

WindowContext::WindowContext(BrowsingContext* aBrowsingContext,
                             uint64_t aInnerWindowId, FieldTuple&& aFields)
    : mFields(std::move(aFields)),
      mInnerWindowId(aInnerWindowId),
      mBrowsingContext(aBrowsingContext) {
  MOZ_ASSERT(mBrowsingContext);
  MOZ_ASSERT(mInnerWindowId);
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WindowContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContext)
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
  WriteIPDLParam(aMessage, aActor, aInit.mBrowsingContextId);
  WriteIPDLParam(aMessage, aActor, aInit.mFields);
}

bool IPDLParamTraits<dom::WindowContext::IPCInitializer>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::WindowContext::IPCInitializer* aInit) {
  // Read actor ID parameters.
  return ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mInnerWindowId) &&
         ReadIPDLParam(aMessage, aIterator, aActor,
                       &aInit->mBrowsingContextId) &&
         ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mFields);
}

template struct IPDLParamTraits<dom::WindowContext::BaseTransaction>;

}  // namespace ipc
}  // namespace mozilla
