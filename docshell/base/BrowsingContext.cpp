/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowsingContext.h"

#include "mozilla/dom/ChromeBrowsingContext.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"

#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsIDocShell.h"
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

/* static */ void
BrowsingContext::Init()
{
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

/* static */ LogModule*
BrowsingContext::GetLog()
{
  return gBrowsingContextLog;
}

/* static */ already_AddRefed<BrowsingContext>
BrowsingContext::Get(uint64_t aId)
{
  RefPtr<BrowsingContext> abc = sBrowsingContexts->Get(aId);
  return abc.forget();
}

/* static */ already_AddRefed<BrowsingContext>
BrowsingContext::Create(nsIDocShell* aDocShell)
{
  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context = new ChromeBrowsingContext(aDocShell);
  } else {
    // TODO(farre): will we ever create BrowsingContexts on processes
    // other than content and parent?
    MOZ_ASSERT(XRE_IsContentProcess());
    context = new BrowsingContext(aDocShell);
  }

  return context.forget();
}

BrowsingContext::BrowsingContext(nsIDocShell* aDocShell)
  : mBrowsingContextId(nsContentUtils::GenerateBrowsingContextId())
  , mDocShell(aDocShell)
{
  sBrowsingContexts->Put(mBrowsingContextId, this);
}

BrowsingContext::BrowsingContext(uint64_t aBrowsingContextId,
                const nsAString& aName)
  : mBrowsingContextId(aBrowsingContextId)
  , mName(aName)
{
  sBrowsingContexts->Put(mBrowsingContextId, this);
}

void
BrowsingContext::Attach(BrowsingContext* aParent)
{
  if (isInList()) {
    MOZ_LOG(GetLog(),
            LogLevel::Debug,
            ("%s: Connecting already existing 0x%08" PRIx64 " to 0x%08" PRIx64,
             XRE_IsParentProcess() ? "Parent" : "Child",
             Id(),
             aParent ? aParent->Id() : 0));
    MOZ_DIAGNOSTIC_ASSERT(sBrowsingContexts->Contains(Id()));
    MOZ_DIAGNOSTIC_ASSERT(!IsCached());
    return;
  }

  bool wasCached = sCachedBrowsingContexts->Remove(Id());

  MOZ_LOG(GetLog(),
          LogLevel::Debug,
          ("%s: %s 0x%08" PRIx64 " to 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child",
           wasCached ? "Re-connecting" : "Connecting",
           Id(),
           aParent ? aParent->Id() : 0));

  auto* children = aParent ? &aParent->mChildren : sRootBrowsingContexts.get();
  children->insertBack(this);
  mParent = aParent;

  if (!XRE_IsContentProcess()) {
    return;
  }

  auto cc = ContentChild::GetSingleton();
  MOZ_DIAGNOSTIC_ASSERT(cc);
  cc->SendAttachBrowsingContext(
    BrowsingContextId(mParent ? mParent->Id() : 0),
    BrowsingContextId(Id()),
    mName);
}

void
BrowsingContext::Detach()
{
  RefPtr<BrowsingContext> kungFuDeathGrip(this);

  if (sCachedBrowsingContexts) {
    sCachedBrowsingContexts->Remove(Id());
  }

  if (!isInList()) {
    MOZ_LOG(GetLog(),
            LogLevel::Debug,
            ("%s: Detaching already detached 0x%08" PRIx64,
             XRE_IsParentProcess() ? "Parent" : "Child",
             Id()));
    return;
  }

  MOZ_LOG(GetLog(),
          LogLevel::Debug,
          ("%s: Detaching 0x%08" PRIx64 " from 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child",
           Id(),
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

void
BrowsingContext::CacheChildren()
{
  if (mChildren.isEmpty()) {
    return;
  }

  MOZ_LOG(GetLog(),
          LogLevel::Debug,
          ("%s: Caching children of 0x%08" PRIx64 "",
           XRE_IsParentProcess() ? "Parent" : "Child",
           Id()));

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

bool
BrowsingContext::IsCached()
{
  return sCachedBrowsingContexts->Contains(Id());
}

void
BrowsingContext::GetChildren(nsTArray<RefPtr<BrowsingContext>>& aChildren)
{
  for (BrowsingContext* context : mChildren) {
    aChildren.AppendElement(context);
  }
}

/* static */ void
BrowsingContext::GetRootBrowsingContexts(nsTArray<RefPtr<BrowsingContext>>& aBrowsingContexts)
{
  for (BrowsingContext* context : *sRootBrowsingContexts) {
    aBrowsingContexts.AppendElement(context);
  }
}

BrowsingContext::~BrowsingContext()
{
  MOZ_DIAGNOSTIC_ASSERT(!isInList());

  if (sBrowsingContexts) {
    sBrowsingContexts->Remove(mBrowsingContextId);
  }
}

nsISupports*
BrowsingContext::GetParentObject() const
{
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject*
BrowsingContext::WrapObject(JSContext* aCx,
                            JS::Handle<JSObject*> aGivenProto)
{
  return BrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

static void
ImplCycleCollectionUnlink(BrowsingContext::Children& aField)
{
  aField.clear();
}

static void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            BrowsingContext::Children& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
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

} // namespace dom
} // namespace mozilla
