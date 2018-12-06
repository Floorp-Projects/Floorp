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
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"

#include "nsDataHashtable.h"
#include "nsDocShell.h"
#include "nsRefPtrHashtable.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

static LazyLogModule gBrowsingContextLog("BrowsingContext");

static StaticAutoPtr<BrowsingContext::Children> sRootBrowsingContexts;

static StaticAutoPtr<nsDataHashtable<nsUint64HashKey, BrowsingContext*>>
    sBrowsingContexts;

// TODO(farre): This duplicates some of the work performed by the
// bfcache. This should be unified. [Bug 1471601]
static StaticAutoPtr<nsRefPtrHashtable<nsUint64HashKey, BrowsingContext>>
    sCachedBrowsingContexts;

static void Register(BrowsingContext* aBrowsingContext) {
  auto entry = sBrowsingContexts->LookupForAdd(aBrowsingContext->Id());
  MOZ_RELEASE_ASSERT(!entry, "Duplicate BrowsingContext ID");
  entry.OrInsert([&] { return aBrowsingContext; });
}

static void Sync(BrowsingContext* aBrowsingContext) {
  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  nsAutoString name;
  aBrowsingContext->GetName(name);
  RefPtr<BrowsingContext> parent = aBrowsingContext->GetParent();
  BrowsingContext* opener = aBrowsingContext->GetOpener();
  cc->SendAttachBrowsingContext(BrowsingContextId(parent ? parent->Id() : 0),
                                BrowsingContextId(opener ? opener->Id() : 0),
                                BrowsingContextId(aBrowsingContext->Id()),
                                name);
}

/* static */ void BrowsingContext::Init() {
  if (!sRootBrowsingContexts) {
    sRootBrowsingContexts = new BrowsingContext::Children();
    ClearOnShutdown(&sRootBrowsingContexts);
  }

  if (!sBrowsingContexts) {
    sBrowsingContexts =
        new nsDataHashtable<nsUint64HashKey, BrowsingContext*>();
    ClearOnShutdown(&sBrowsingContexts);
  }

  if (!sCachedBrowsingContexts) {
    sCachedBrowsingContexts =
        new nsRefPtrHashtable<nsUint64HashKey, BrowsingContext>();
    ClearOnShutdown(&sCachedBrowsingContexts);
  }
}

/* static */ LogModule* BrowsingContext::GetLog() {
  return gBrowsingContextLog;
}

/* static */ already_AddRefed<BrowsingContext> BrowsingContext::Get(
    uint64_t aId) {
  RefPtr<BrowsingContext> abc = sBrowsingContexts->Get(aId);
  return abc.forget();
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
      mName(aName) {}

void BrowsingContext::SetDocShell(nsIDocShell* aDocShell) {
  // XXX(nika): We should communicate that we are now an active BrowsingContext
  // process to the parent & do other validation here.
  MOZ_RELEASE_ASSERT(nsDocShell::Cast(aDocShell)->GetBrowsingContext() == this);
  mDocShell = aDocShell;
}

void BrowsingContext::Attach() {
  if (isInList()) {
    MOZ_LOG(GetLog(), LogLevel::Debug,
            ("%s: Connecting already existing 0x%08" PRIx64 " to 0x%08" PRIx64,
             XRE_IsParentProcess() ? "Parent" : "Child", Id(),
             mParent ? mParent->Id() : 0));
    MOZ_DIAGNOSTIC_ASSERT(sBrowsingContexts->Contains(Id()));
    MOZ_DIAGNOSTIC_ASSERT(!IsCached());
    return;
  }

  bool wasCached = sCachedBrowsingContexts->Remove(Id());

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: %s 0x%08" PRIx64 " to 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child",
           wasCached ? "Re-connecting" : "Connecting", Id(),
           mParent ? mParent->Id() : 0));

  auto* children = mParent ? &mParent->mChildren : sRootBrowsingContexts.get();
  children->insertBack(this);

  Sync(this);
}

void BrowsingContext::Detach() {
  RefPtr<BrowsingContext> kungFuDeathGrip(this);

  if (sCachedBrowsingContexts) {
    sCachedBrowsingContexts->Remove(Id());
  }

  if (!isInList()) {
    MOZ_LOG(GetLog(), LogLevel::Debug,
            ("%s: Detaching already detached 0x%08" PRIx64,
             XRE_IsParentProcess() ? "Parent" : "Child", Id()));
    return;
  }

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Detaching 0x%08" PRIx64 " from 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child", Id(),
           mParent ? mParent->Id() : 0));

  remove();

  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  cc->SendDetachBrowsingContext(BrowsingContextId(Id()),
                                false /* aMoveToBFCache */);
}

void BrowsingContext::CacheChildren() {
  if (mChildren.isEmpty()) {
    return;
  }

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Caching children of 0x%08" PRIx64 "",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  while (!mChildren.isEmpty()) {
    RefPtr<BrowsingContext> child = mChildren.popFirst();
    sCachedBrowsingContexts->Put(child->Id(), child);
  }

  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  cc->SendDetachBrowsingContext(BrowsingContextId(Id()),
                                true /* aMoveToBFCache */);
}

bool BrowsingContext::IsCached() {
  return sCachedBrowsingContexts->Contains(Id());
}

void BrowsingContext::GetChildren(
    nsTArray<RefPtr<BrowsingContext>>& aChildren) {
  for (BrowsingContext* context : mChildren) {
    aChildren.AppendElement(context);
  }
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
  for (BrowsingContext* context : *sRootBrowsingContexts) {
    aBrowsingContexts.AppendElement(context);
  }
}

BrowsingContext::~BrowsingContext() {
  MOZ_DIAGNOSTIC_ASSERT(!isInList());

  if (sBrowsingContexts) {
    sBrowsingContexts->Remove(mBrowsingContextId);
  }
}

nsISupports* BrowsingContext::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContext::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

static void ImplCycleCollectionUnlink(BrowsingContext::Children& aField) {
  aField.clear();
}

static void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    BrowsingContext::Children& aField, const char* aName, uint32_t aFlags = 0) {
  for (BrowsingContext* aContext : aField) {
    aCallback.NoteNativeChild(aContext,
                              NS_CYCLE_COLLECTION_PARTICIPANT(BrowsingContext));
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell, mChildren)
  if (XRE_IsParentProcess()) {
    ChromeBrowsingContext::Cast(tmp)->Unlink();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell, mChildren)
  if (XRE_IsParentProcess()) {
    ChromeBrowsingContext::Cast(tmp)->Traverse(cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContext, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContext, Release)

}  // namespace dom
}  // namespace mozilla
