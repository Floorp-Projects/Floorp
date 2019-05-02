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
#include "xpcprivate.h"

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

static LazyLogModule gBrowsingContextLog("BrowsingContext");

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

  aBrowsingContext->Group()->Register(aBrowsingContext);
}

BrowsingContext* BrowsingContext::TopLevelBrowsingContext() {
  BrowsingContext* bc = this;
  while (bc->mParent) {
    bc = bc->mParent;
  }
  return bc;
}

/* static */
void BrowsingContext::Init() {
  if (!sBrowsingContexts) {
    sBrowsingContexts = new BrowsingContextMap<WeakPtr>();
    ClearOnShutdown(&sBrowsingContexts);
  }

  if (!sCachedBrowsingContexts) {
    sCachedBrowsingContexts = new BrowsingContextMap<RefPtr>();
    ClearOnShutdown(&sCachedBrowsingContexts);
  }
}

/* static */
LogModule* BrowsingContext::GetLog() { return gBrowsingContextLog; }

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::Get(uint64_t aId) {
  if (BrowsingContextMap<WeakPtr>::Ptr abc = sBrowsingContexts->lookup(aId)) {
    return do_AddRef(abc->value().get());
  }

  return nullptr;
}

CanonicalBrowsingContext* BrowsingContext::Canonical() {
  return CanonicalBrowsingContext::Cast(this);
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::Create(
    BrowsingContext* aParent, BrowsingContext* aOpener, const nsAString& aName,
    Type aType) {
  MOZ_DIAGNOSTIC_ASSERT(!aParent || aParent->mType == aType);

  uint64_t id = nsContentUtils::GenerateBrowsingContextId();

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " in %s", id,
           XRE_IsParentProcess() ? "Parent" : "Child"));

  // Determine which BrowsingContextGroup this context should be created in.
  RefPtr<BrowsingContextGroup> group =
      BrowsingContextGroup::Select(aParent, aOpener);

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context = new CanonicalBrowsingContext(aParent, group, id,
                                           /* aProcessId */ 0, aType);
  } else {
    context = new BrowsingContext(aParent, group, id, aType);
  }

  // The name and opener fields need to be explicitly initialized. Don't bother
  // using transactions to set them, as we haven't been attached yet.
  context->mName = aName;
  context->mOpenerId = aOpener ? aOpener->Id() : 0;

  if (aParent) {
    context->mCrossOriginPolicy = aParent->mCrossOriginPolicy;
  } else if (aOpener) {
    context->mCrossOriginPolicy = aOpener->mCrossOriginPolicy;
  } else {
    context->mCrossOriginPolicy = nsILoadInfo::CROSS_ORIGIN_POLICY_NULL;
  }

  Register(context);

  // Attach the browsing context to the tree.
  context->Attach();

  return context.forget();
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
    context = new CanonicalBrowsingContext(parent, aGroup, aInit.mId, originId,
                                           Type::Content);
  } else {
    context = new BrowsingContext(parent, aGroup, aInit.mId, Type::Content);
  }

  Register(context);

  // Initialize all of our fields from IPC. We don't have to worry about
  // mOpenerId, as we won't try to dereference it immediately.
#define MOZ_BC_FIELD(name, ...) context->m##name = aInit.m##name;
#include "mozilla/dom/BrowsingContextFieldList.h"

  // Caller handles attaching us to the tree.

  return context.forget();
}

BrowsingContext::BrowsingContext(BrowsingContext* aParent,
                                 BrowsingContextGroup* aGroup,
                                 uint64_t aBrowsingContextId, Type aType)
    : mType(aType),
      mBrowsingContextId(aBrowsingContextId),
      mGroup(aGroup),
      mParent(aParent),
      mIsInProcess(false) {
  MOZ_RELEASE_ASSERT(!mParent || mParent->Group() == mGroup);
  MOZ_RELEASE_ASSERT(mBrowsingContextId != 0);
  MOZ_RELEASE_ASSERT(mGroup);
}

void BrowsingContext::SetDocShell(nsIDocShell* aDocShell) {
  // XXX(nika): We should communicate that we are now an active BrowsingContext
  // process to the parent & do other validation here.
  MOZ_RELEASE_ASSERT(nsDocShell::Cast(aDocShell)->GetBrowsingContext() == this);
  mDocShell = aDocShell;
  mIsInProcess = true;
}

void BrowsingContext::Attach(bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: %s 0x%08" PRIx64 " to 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child",
           sCachedBrowsingContexts->has(Id()) ? "Re-connecting" : "Connecting",
           Id(), mParent ? mParent->Id() : 0));

  sCachedBrowsingContexts->remove(Id());

  auto* children = mParent ? &mParent->mChildren : &mGroup->Toplevels();
  MOZ_DIAGNOSTIC_ASSERT(!children->Contains(this));

  children->AppendElement(this);

  // Send attach to our parent if we need to.
  if (!aFromIPC && XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->SendAttachBrowsingContext(
        GetIPCInitializer());
  }
}

void BrowsingContext::Detach(bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Detaching 0x%08" PRIx64 " from 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child", Id(),
           mParent ? mParent->Id() : 0));

  RefPtr<BrowsingContext> kungFuDeathGrip(this);

  BrowsingContextMap<RefPtr>::Ptr p;
  if (sCachedBrowsingContexts && (p = sCachedBrowsingContexts->lookup(Id()))) {
    MOZ_DIAGNOSTIC_ASSERT(!mParent || !mParent->mChildren.Contains(this));
    MOZ_DIAGNOSTIC_ASSERT(!mGroup || !mGroup->Toplevels().Contains(this));
    sCachedBrowsingContexts->remove(p);
  } else {
    Children* children = nullptr;
    if (mParent) {
      children = &mParent->mChildren;
    } else if (mGroup) {
      children = &mGroup->Toplevels();
    }

    if (children) {
      // TODO(farre): This assert looks extremely fishy, I know, but
      // what we're actually saying is this: if we're detaching, but our
      // parent doesn't have any children, it is because we're being
      // detached by the cycle collector destroying docshells out of
      // order.
      MOZ_DIAGNOSTIC_ASSERT(children->IsEmpty() || children->Contains(this));

      children->RemoveElement(this);
    }
  }

  if (mGroup) {
    mGroup->Unregister(this);
  }

  // As our nsDocShell is going away, this should implicitly mark us as closed.
  // We directly set our member, rather than using a transaction as we're going
  // to send a `Detach` message to other processes either way.
  mClosed = true;

  if (!aFromIPC && XRE_IsContentProcess()) {
    auto cc = ContentChild::GetSingleton();
    MOZ_DIAGNOSTIC_ASSERT(cc);
    cc->SendDetachBrowsingContext(this, false /* aMoveToBFCache */);
  }
}

void BrowsingContext::CacheChildren(bool aFromIPC) {
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

  if (!aFromIPC && XRE_IsContentProcess()) {
    auto cc = ContentChild::GetSingleton();
    MOZ_DIAGNOSTIC_ASSERT(cc);
    cc->SendDetachBrowsingContext(this, true /* aMoveToBFCache */);
  }
}

bool BrowsingContext::IsCached() { return sCachedBrowsingContexts->has(Id()); }

void BrowsingContext::GetChildren(
    nsTArray<RefPtr<BrowsingContext>>& aChildren) {
  MOZ_ALWAYS_TRUE(aChildren.AppendElements(mChildren));
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
BrowsingContext* BrowsingContext::FindWithName(const nsAString& aName) {
  BrowsingContext* found = nullptr;
  if (aName.IsEmpty()) {
    // You can't find a browsing context with an empty name.
    found = nullptr;
  } else if (BrowsingContext* special = FindWithSpecialName(aName)) {
    found = special;
  } else if (aName.LowerCaseEqualsLiteral("_blank")) {
    // Just return null. Caller must handle creating a new window with
    // a blank name.
    found = nullptr;
  } else if (BrowsingContext* child = FindWithNameInSubtree(aName, this)) {
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
      } else if (parent->NameEquals(aName) && CanAccess(parent) &&
                 parent->IsActive()) {
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
                sibling->FindWithNameInSubtree(aName, this)) {
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
  MOZ_DIAGNOSTIC_ASSERT(!found || CanAccess(found));

  return found;
}

BrowsingContext* BrowsingContext::FindChildWithName(const nsAString& aName) {
  if (aName.IsEmpty()) {
    // You can't find a browsing context with the empty name.
    return nullptr;
  }

  for (BrowsingContext* child : mChildren) {
    if (child->NameEquals(aName) && CanAccess(child) && child->IsActive()) {
      return child;
    }
  }

  return nullptr;
}

BrowsingContext* BrowsingContext::FindWithSpecialName(const nsAString& aName) {
  // TODO(farre): Neither BrowsingContext nor nsDocShell checks if the
  // browsing context pointed to by a special name is active. Should
  // it be? See Bug 1527913.
  if (aName.LowerCaseEqualsLiteral("_self")) {
    return this;
  }

  if (aName.LowerCaseEqualsLiteral("_parent")) {
    return mParent && CanAccess(mParent.get()) ? mParent.get() : this;
  }

  if (aName.LowerCaseEqualsLiteral("_top")) {
    BrowsingContext* top = TopLevelBrowsingContext();

    return CanAccess(top) ? top : nullptr;
  }

  return nullptr;
}

BrowsingContext* BrowsingContext::FindWithNameInSubtree(
    const nsAString& aName, BrowsingContext* aRequestingContext) {
  MOZ_DIAGNOSTIC_ASSERT(!aName.IsEmpty());

  if (NameEquals(aName) && aRequestingContext->CanAccess(this) && IsActive()) {
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

bool BrowsingContext::CanAccess(BrowsingContext* aContext) {
  // TODO(farre): Bouncing this to nsDocShell::CanAccessItem is
  // temporary, we should implement a replacement for this in
  // BrowsingContext. See Bug 151590.
  return aContext && nsDocShell::CanAccessItem(aContext->mDocShell, mDocShell);
}

bool BrowsingContext::IsActive() const {
  // TODO(farre): Mimicking the bahaviour from
  // ItemIsActive(nsIDocShellTreeItem* aItem) is temporary, we should
  // implement a replacement for this using mClosed only. See Bug
  // 1527321.

  if (!mDocShell) {
    return mClosed;
  }

  if (nsCOMPtr<nsPIDOMWindowOuter> window = mDocShell->GetWindow()) {
    auto* win = nsGlobalWindowOuter::Cast(window);
    if (!win->GetClosedOuter()) {
      return true;
    }
  }

  return false;
}

BrowsingContext::~BrowsingContext() {
  MOZ_DIAGNOSTIC_ASSERT(!mParent || !mParent->mChildren.Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!mGroup || !mGroup->Toplevels().Contains(this));
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

void BrowsingContext::NotifyUserGestureActivation() {
  // We would set the user gesture activation flag on the top level browsing
  // context, which would automatically be sync to other top level browsing
  // contexts which are in the different process.
  RefPtr<BrowsingContext> topLevelBC = TopLevelBrowsingContext();
  USER_ACTIVATION_LOG("Get top level browsing context 0x%08" PRIx64,
                      topLevelBC->Id());
  topLevelBC->SetIsActivatedByUserGesture(true);
}

void BrowsingContext::NotifyResetUserGestureActivation() {
  // We would reset the user gesture activation flag on the top level browsing
  // context, which would automatically be sync to other top level browsing
  // contexts which are in the different process.
  RefPtr<BrowsingContext> topLevelBC = TopLevelBrowsingContext();
  USER_ACTIVATION_LOG("Get top level browsing context 0x%08" PRIx64,
                      topLevelBC->Id());
  topLevelBC->SetIsActivatedByUserGesture(false);
}

bool BrowsingContext::GetUserGestureActivation() {
  RefPtr<BrowsingContext> topLevelBC = TopLevelBrowsingContext();
  return topLevelBC->GetIsActivatedByUserGesture();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell, mChildren, mParent, mGroup)
  if (XRE_IsParentProcess()) {
    CanonicalBrowsingContext::Cast(tmp)->Unlink();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell, mChildren, mParent, mGroup)
  if (XRE_IsParentProcess()) {
    CanonicalBrowsingContext::Cast(tmp)->Traverse(cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContext, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContext, Release)

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
const js::Class RemoteLocationProxy::Base::sClass =
    PROXY_CLASS_DEF("Proxy", JSCLASS_HAS_RESERVED_SLOTS(2));

void BrowsingContext::Location(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aLocation,
                               ErrorResult& aError) {
  aError.MightThrowJSException();
  sSingleton.GetProxyObject(aCx, &mLocation, aLocation);
  if (!aLocation) {
    aError.StealExceptionFromJSContext(aCx);
  }
}

void BrowsingContext::Close(CallerType aCallerType, ErrorResult& aError) {
  // FIXME We need to set mClosed, but only once we're sending the
  //       DOMWindowClose event (which happens in the process where the
  //       document for this browsing context is loaded).
  //       See https://bugzilla.mozilla.org/show_bug.cgi?id=1516343.
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowClose(this, aCallerType == CallerType::System);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowClose(this, aCallerType == CallerType::System);
  }
}

void BrowsingContext::Focus(ErrorResult& aError) {
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowFocus(this);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowFocus(this);
  }
}

void BrowsingContext::Blur(ErrorResult& aError) {
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowBlur(this);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowBlur(this);
  }
}

Nullable<WindowProxyHolder> BrowsingContext::GetTop(ErrorResult& aError) {
  // We never return null or throw an error, but the implementation in
  // nsGlobalWindow does and we need to use the same signature.
  return WindowProxyHolder(TopLevelBrowsingContext());
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
  data.source() = sourceBc;
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

void BrowsingContext::Transaction::Commit(BrowsingContext* aBrowsingContext) {
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    cc->SendCommitBrowsingContextTransaction(aBrowsingContext, *this);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
    for (auto iter = aBrowsingContext->Group()->ContentParentsIter();
         !iter.Done(); iter.Next()) {
      nsRefPtrHashKey<ContentParent>* entry = iter.Get();
      Unused << entry->GetKey()->SendCommitBrowsingContextTransaction(
          aBrowsingContext, *this);
    }
  }

  Apply(aBrowsingContext, nullptr);
}

void BrowsingContext::Transaction::Apply(BrowsingContext* aBrowsingContext,
                                         ContentParent* aSource) {
#define MOZ_BC_FIELD(name, ...)                         \
  if (m##name) {                                        \
    aBrowsingContext->WillSet##name(*m##name, aSource); \
    aBrowsingContext->m##name = std::move(*m##name);    \
    aBrowsingContext->DidSet##name(aSource);            \
    m##name.reset();                                    \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"
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
  if (mOpenerId != 0) {
    opener = BrowsingContext::Get(mOpenerId);
    MOZ_RELEASE_ASSERT(opener);
  }
  return opener.forget();
}

void BrowsingContext::LocationProxy::SetHref(const nsAString& aHref,
                                             nsIPrincipal& aSubjectPrincipal,
                                             ErrorResult& aError) {
  nsPIDOMWindowOuter* win = GetBrowsingContext()->GetDOMWindow();
  if (!win || !win->GetLocation()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  win->GetLocation()->SetHref(aHref, aSubjectPrincipal, aError);
}

void BrowsingContext::LocationProxy::Replace(const nsAString& aUrl,
                                             nsIPrincipal& aSubjectPrincipal,
                                             ErrorResult& aError) {
  nsPIDOMWindowOuter* win = GetBrowsingContext()->GetDOMWindow();
  if (!win || !win->GetLocation()) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  win->GetLocation()->Replace(aUrl, aSubjectPrincipal, aError);
}

}  // namespace dom

namespace ipc {

void IPDLParamTraits<dom::BrowsingContext>::Write(
    IPC::Message* aMsg, IProtocol* aActor, dom::BrowsingContext* aParam) {
  uint64_t id = aParam ? aParam->Id() : 0;
  WriteIPDLParam(aMsg, aActor, id);

  // If his is an in-process send. We want to make sure that our BrowsingContext
  // object lives long enough to make it to the other side, so we take an extra
  // reference. This reference is freed in ::Read().
  if (!aActor->GetIPCChannel()->IsCrossProcess()) {
    NS_IF_ADDREF(aParam);
  }
}

bool IPDLParamTraits<dom::BrowsingContext>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    RefPtr<dom::BrowsingContext>* aResult) {
  uint64_t id = 0;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &id)) {
    return false;
  }

  if (id == 0) {
    aResult = nullptr;
    return true;
  }

  *aResult = dom::BrowsingContext::Get(id);
  MOZ_ASSERT(aResult, "Deserialized absent BrowsingContext!");

  // If this is an in-process actor, free the reference taken in ::Write().
  if (!aActor->GetIPCChannel()->IsCrossProcess()) {
    dom::BrowsingContext* bc = *aResult;
    NS_IF_RELEASE(bc);
  }

  return aResult != nullptr;
}

void IPDLParamTraits<dom::BrowsingContext::Transaction>::Write(
    IPC::Message* aMessage, IProtocol* aActor,
    const dom::BrowsingContext::Transaction& aTransaction) {
#define MOZ_BC_FIELD(name, ...) \
  WriteIPDLParam(aMessage, aActor, aTransaction.m##name);
#include "mozilla/dom/BrowsingContextFieldList.h"
}

bool IPDLParamTraits<dom::BrowsingContext::Transaction>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::BrowsingContext::Transaction* aTransaction) {
#define MOZ_BC_FIELD(name, ...)                                              \
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aTransaction->m##name)) { \
    return false;                                                            \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"

  return true;
}

void IPDLParamTraits<dom::BrowsingContext::IPCInitializer>::Write(
    IPC::Message* aMessage, IProtocol* aActor,
    const dom::BrowsingContext::IPCInitializer& aInit) {
  // Write actor ID parameters.
  WriteIPDLParam(aMessage, aActor, aInit.mId);
  WriteIPDLParam(aMessage, aActor, aInit.mParentId);

  // Write other synchronized fields.
#define MOZ_BC_FIELD(name, ...) WriteIPDLParam(aMessage, aActor, aInit.m##name);
#include "mozilla/dom/BrowsingContextFieldList.h"
}

bool IPDLParamTraits<dom::BrowsingContext::IPCInitializer>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::BrowsingContext::IPCInitializer* aInit) {
  // Read actor ID parameters.
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mId) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mParentId)) {
    return false;
  }

  // Read other synchronized fields.
#define MOZ_BC_FIELD(name, ...)                                       \
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aInit->m##name)) { \
    return false;                                                     \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"

  return true;
}

}  // namespace ipc
}  // namespace mozilla
