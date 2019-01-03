/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContext.h"

#include "mozilla/dom/ChromeBrowsingContext.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/HashTable.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"

#include "nsDocShell.h"
#include "nsGlobalWindowOuter.h"
#include "nsContentUtils.h"
#include "nsScriptError.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

static LazyLogModule gBrowsingContextLog("BrowsingContext");

static StaticAutoPtr<BrowsingContext::Children> sRootBrowsingContexts;

template <template <typename> class PtrType>
using BrowsingContextMap =
    HashMap<uint64_t, PtrType<BrowsingContext>, DefaultHasher<uint64_t>,
            InfallibleAllocPolicy>;

static StaticAutoPtr<BrowsingContextMap<WeakPtr>> sBrowsingContexts;

// TODO(farre): This duplicates some of the work performed by the
// bfcache. This should be unified. [Bug 1471601]
static StaticAutoPtr<BrowsingContextMap<RefPtr>> sCachedBrowsingContexts;

static void Register(BrowsingContext* aBrowsingContext) {
  MOZ_ALWAYS_TRUE(
      sBrowsingContexts->putNew(aBrowsingContext->Id(), aBrowsingContext));
}

static void Sync(BrowsingContext* aBrowsingContext) {
  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  RefPtr<BrowsingContext> parent = aBrowsingContext->GetParent();
  BrowsingContext* opener = aBrowsingContext->GetOpener();
  cc->SendAttachBrowsingContext(BrowsingContextId(parent ? parent->Id() : 0),
                                BrowsingContextId(opener ? opener->Id() : 0),
                                BrowsingContextId(aBrowsingContext->Id()),
                                aBrowsingContext->Name());
}

/* static */ void BrowsingContext::Init() {
  if (!sRootBrowsingContexts) {
    sRootBrowsingContexts = new BrowsingContext::Children();
    ClearOnShutdown(&sRootBrowsingContexts);
  }

  if (!sBrowsingContexts) {
    sBrowsingContexts = new BrowsingContextMap<WeakPtr>();
    ClearOnShutdown(&sBrowsingContexts);
  }

  if (!sCachedBrowsingContexts) {
    sCachedBrowsingContexts = new BrowsingContextMap<RefPtr>();
    ClearOnShutdown(&sCachedBrowsingContexts);
  }
}

/* static */ LogModule* BrowsingContext::GetLog() {
  return gBrowsingContextLog;
}

/* static */ already_AddRefed<BrowsingContext> BrowsingContext::Get(
    uint64_t aId) {
  if (BrowsingContextMap<WeakPtr>::Ptr abc = sBrowsingContexts->lookup(aId)) {
    return do_AddRef(abc->value().get());
  }

  return nullptr;
}

/* static */ already_AddRefed<BrowsingContext> BrowsingContext::Create(
    BrowsingContext* aParent, BrowsingContext* aOpener, const nsAString& aName,
    Type aType) {
  MOZ_DIAGNOSTIC_ASSERT(!aParent || aParent->mType == aType);

  uint64_t id = nsContentUtils::GenerateBrowsingContextId();

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " in %s", id,
           XRE_IsParentProcess() ? "Parent" : "Child"));

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context = new ChromeBrowsingContext(aParent, aOpener, aName, id,
                                        /* aProcessId */ 0, aType);
  } else {
    context = new BrowsingContext(aParent, aOpener, aName, id, aType);
  }

  Register(context);

  // Attach the browsing context to the tree.
  context->Attach();

  return context.forget();
}

/* static */ already_AddRefed<BrowsingContext> BrowsingContext::CreateFromIPC(
    BrowsingContext* aParent, BrowsingContext* aOpener, const nsAString& aName,
    uint64_t aId, ContentParent* aOriginProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aOriginProcess || XRE_IsContentProcess(),
                        "Parent Process IPC contexts need a Content Process.");
  MOZ_DIAGNOSTIC_ASSERT(!aParent || aParent->IsContent());

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " from IPC (origin=0x%08" PRIx64 ")", aId,
           aOriginProcess ? uint64_t(aOriginProcess->ChildID()) : 0));

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context = new ChromeBrowsingContext(
        aParent, aOpener, aName, aId, aOriginProcess->ChildID(), Type::Content);
  } else {
    context = new BrowsingContext(aParent, aOpener, aName, aId, Type::Content);
  }

  Register(context);

  context->Attach();

  return context.forget();
}

BrowsingContext::BrowsingContext(BrowsingContext* aParent,
                                 BrowsingContext* aOpener,
                                 const nsAString& aName,
                                 uint64_t aBrowsingContextId, Type aType)
    : mType(aType),
      mBrowsingContextId(aBrowsingContextId),
      mParent(aParent),
      mOpener(aOpener),
      mName(aName),
      mClosed(false) {
  if (mParent) {
    mBrowsingContextGroup = mParent->mBrowsingContextGroup;
  } else if (mOpener) {
    mBrowsingContextGroup = mOpener->mBrowsingContextGroup;
  } else {
    mBrowsingContextGroup = new BrowsingContextGroup();
  }

  if (!mParent) {
    // If we don't have a parent we're either a top level or auxiliary
    // BrowsingContext.
    mBrowsingContextGroup->AppendElement(this);
  }
}

void BrowsingContext::SetDocShell(nsIDocShell* aDocShell) {
  // XXX(nika): We should communicate that we are now an active BrowsingContext
  // process to the parent & do other validation here.
  MOZ_RELEASE_ASSERT(nsDocShell::Cast(aDocShell)->GetBrowsingContext() == this);
  mDocShell = aDocShell;
}

void BrowsingContext::Attach() {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: %s 0x%08" PRIx64 " to 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child",
           sCachedBrowsingContexts->has(Id()) ? "Re-connecting" : "Connecting",
           Id(), mParent ? mParent->Id() : 0));

  sCachedBrowsingContexts->remove(Id());

  auto* children = mParent ? &mParent->mChildren : sRootBrowsingContexts.get();
  MOZ_DIAGNOSTIC_ASSERT(!children->Contains(this));

  children->AppendElement(this);

  Sync(this);
}

void BrowsingContext::Detach() {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Detaching 0x%08" PRIx64 " from 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child", Id(),
           mParent ? mParent->Id() : 0));

  RefPtr<BrowsingContext> kungFuDeathGrip(this);

  BrowsingContextMap<RefPtr>::Ptr p;
  if (sCachedBrowsingContexts && (p = sCachedBrowsingContexts->lookup(Id()))) {
    MOZ_DIAGNOSTIC_ASSERT(!mParent || !mParent->mChildren.Contains(this));
    MOZ_DIAGNOSTIC_ASSERT(!sRootBrowsingContexts->Contains(this));
    sCachedBrowsingContexts->remove(p);
  } else {
    auto* children =
        mParent ? &mParent->mChildren : sRootBrowsingContexts.get();
    // TODO(farre): This assert looks extremely fishy, I know, but
    // what we're actually saying is this: if we're detaching, but our
    // parent doesn't have any children, it is because we're being
    // detached by the cycle collector destroying docshells out of
    // order.
    MOZ_DIAGNOSTIC_ASSERT(children->IsEmpty() || children->Contains(this));

    children->RemoveElement(this);
  }

  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  cc->SendDetachBrowsingContext(BrowsingContextId(Id()),
                                false /* aMoveToBFCache */);
}

void BrowsingContext::CacheChildren() {
  if (mChildren.IsEmpty()) {
    return;
  }

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Caching children of 0x%08" PRIx64 "",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  MOZ_ALWAYS_TRUE(sCachedBrowsingContexts->reserve(mChildren.Length()));

  for (BrowsingContext* child : mChildren) {
    MOZ_ALWAYS_TRUE(sCachedBrowsingContexts->putNew(child->Id(), child));
  }
  mChildren.Clear();

  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  cc->SendDetachBrowsingContext(BrowsingContextId(Id()),
                                true /* aMoveToBFCache */);
}

bool BrowsingContext::IsCached() { return sCachedBrowsingContexts->has(Id()); }

void BrowsingContext::GetChildren(
    nsTArray<RefPtr<BrowsingContext>>& aChildren) {
  MOZ_ALWAYS_TRUE(aChildren.AppendElements(mChildren));
}

void BrowsingContext::SetOpener(BrowsingContext* aOpener) {
  if (mOpener == aOpener) {
    return;
  }

  mOpener = aOpener;

  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  cc->SendSetOpenerBrowsingContext(
      BrowsingContextId(Id()), BrowsingContextId(aOpener ? aOpener->Id() : 0));
}

/* static */ void BrowsingContext::GetRootBrowsingContexts(
    nsTArray<RefPtr<BrowsingContext>>& aBrowsingContexts) {
  MOZ_ALWAYS_TRUE(aBrowsingContexts.AppendElements(*sRootBrowsingContexts));
}

BrowsingContext::~BrowsingContext() {
  MOZ_DIAGNOSTIC_ASSERT(!mParent || !mParent->mChildren.Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!sRootBrowsingContexts ||
                        !sRootBrowsingContexts->Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!sCachedBrowsingContexts ||
                        !sCachedBrowsingContexts->has(Id()));

  if (sBrowsingContexts) {
    sBrowsingContexts->remove(Id());
  }
}

nsISupports* BrowsingContext::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContext::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell, mChildren, mParent)
  if (XRE_IsParentProcess()) {
    ChromeBrowsingContext::Cast(tmp)->Unlink();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell, mChildren, mParent)
  if (XRE_IsParentProcess()) {
    ChromeBrowsingContext::Cast(tmp)->Traverse(cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContext, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContext, Release)

void BrowsingContext::Location(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aLocation,
                               OOMReporter& aError) {}

void BrowsingContext::Close(CallerType aCallerType, ErrorResult& aError) {
  // FIXME We need to set mClosed, but only once we're sending the
  //       DOMWindowClose event (which happens in the process where the
  //       document for this browsing context is loaded).
  //       See https://bugzilla.mozilla.org/show_bug.cgi?id=1516343.
  ContentChild* cc = ContentChild::GetSingleton();
  cc->SendWindowClose(BrowsingContextId(mBrowsingContextId),
                      aCallerType == CallerType::System);
}

void BrowsingContext::Focus(ErrorResult& aError) {
  ContentChild* cc = ContentChild::GetSingleton();
  cc->SendWindowFocus(BrowsingContextId(mBrowsingContextId));
}

void BrowsingContext::Blur(ErrorResult& aError) {
  ContentChild* cc = ContentChild::GetSingleton();
  cc->SendWindowBlur(BrowsingContextId(mBrowsingContextId));
}

Nullable<WindowProxyHolder> BrowsingContext::GetTop(ErrorResult& aError) {
  // We never return null or throw an error, but the implementation in
  // nsGlobalWindow does and we need to use the same signature.
  BrowsingContext* bc = this;
  BrowsingContext* parent;
  while ((parent = bc->mParent)) {
    bc = parent;
  }
  return WindowProxyHolder(bc);
}

void BrowsingContext::GetOpener(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aOpener,
                                ErrorResult& aError) const {
  auto* opener = GetOpener();
  if (!opener) {
    aOpener.setNull();
    return;
  }

  if (!ToJSValue(aCx, WindowProxyHolder(opener), aOpener)) {
    aError.NoteJSContextException(aCx);
  }
}

Nullable<WindowProxyHolder> BrowsingContext::GetParent(
    ErrorResult& aError) const {
  // We never throw an error, but the implementation in nsGlobalWindow does and
  // we need to use the same signature.
  if (!mParent) {
    return nullptr;
  }
  return WindowProxyHolder(mParent.get());
}

void BrowsingContext::PostMessageMoz(JSContext* aCx,
                                     JS::Handle<JS::Value> aMessage,
                                     const nsAString& aTargetOrigin,
                                     const Sequence<JSObject*>& aTransfer,
                                     nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  RefPtr<BrowsingContext> sourceBc;
  PostMessageData data;
  data.targetOrigin() = aTargetOrigin;
  data.subjectPrincipal() = &aSubjectPrincipal;
  RefPtr<nsGlobalWindowInner> callerInnerWindow;
  if (!nsGlobalWindowOuter::GatherPostMessageData(
          aCx, aTargetOrigin, getter_AddRefs(sourceBc), data.origin(),
          getter_AddRefs(data.targetOriginURI()),
          getter_AddRefs(data.callerPrincipal()),
          getter_AddRefs(callerInnerWindow),
          getter_AddRefs(data.callerDocumentURI()), aError)) {
    return;
  }
  data.source() = BrowsingContextId(sourceBc->Id());
  data.isFromPrivateWindow() =
      callerInnerWindow &&
      nsScriptErrorBase::ComputeIsFromPrivateWindow(callerInnerWindow);

  JS::Rooted<JS::Value> transferArray(aCx);
  aError = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransfer,
                                                             &transferArray);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  ipc::StructuredCloneData message;
  message.Write(aCx, aMessage, transferArray, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  ClonedMessageData messageData;
  if (!message.BuildClonedMessageDataForChild(cc, messageData)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  cc->SendWindowPostMessage(BrowsingContextId(mBrowsingContextId), messageData,
                            data);
}

void BrowsingContext::PostMessageMoz(JSContext* aCx,
                                     JS::Handle<JS::Value> aMessage,
                                     const WindowPostMessageOptions& aOptions,
                                     nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  PostMessageMoz(aCx, aMessage, aOptions.mTargetOrigin, aOptions.mTransfer,
                 aSubjectPrincipal, aError);
}

already_AddRefed<BrowsingContext> BrowsingContext::FindChildWithName(
    const nsAString& aName) {
  // FIXME https://bugzilla.mozilla.org/show_bug.cgi?id=1515646 will reimplement
  //       this on top of the BC tree.
  MOZ_ASSERT(mDocShell);
  nsCOMPtr<nsIDocShellTreeItem> child;
  mDocShell->FindChildWithName(aName, false, true, nullptr, nullptr,
                               getter_AddRefs(child));
  nsCOMPtr<nsIDocShell> childDS = do_QueryInterface(child);
  RefPtr<BrowsingContext> bc;
  if (childDS) {
    childDS->GetBrowsingContext(getter_AddRefs(bc));
  }
  return bc.forget();
}

}  // namespace dom
}  // namespace mozilla
