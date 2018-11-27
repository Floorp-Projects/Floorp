/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChromeBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"

namespace mozilla {
namespace dom {

ChromeBrowsingContext::ChromeBrowsingContext(BrowsingContext* aParent,
                                             BrowsingContext* aOpener,
                                             const nsAString& aName,
                                             uint64_t aBrowsingContextId,
                                             uint64_t aProcessId,
                                             BrowsingContext::Type aType)
    : BrowsingContext(aParent, aOpener, aName, aBrowsingContextId, aType),
      mProcessId(aProcessId) {
  // You are only ever allowed to create ChromeBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

// TODO(farre): ChromeBrowsingContext::CleanupContexts starts from the
// list of root BrowsingContexts. This isn't enough when separate
// BrowsingContext nodes of a BrowsingContext tree, not in a crashing
// child process, are from that process and thus needs to be
// cleaned. [Bug 1472108]
/* static */ void ChromeBrowsingContext::CleanupContexts(uint64_t aProcessId) {
  nsTArray<RefPtr<BrowsingContext>> roots;
  BrowsingContext::GetRootBrowsingContexts(roots);

  for (RefPtr<BrowsingContext> context : roots) {
    if (Cast(context)->IsOwnedByProcess(aProcessId)) {
      context->Detach();
    }
  }
}

/* static */ already_AddRefed<ChromeBrowsingContext> ChromeBrowsingContext::Get(
    uint64_t aId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return BrowsingContext::Get(aId).downcast<ChromeBrowsingContext>();
}

/* static */ ChromeBrowsingContext* ChromeBrowsingContext::Cast(
    BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<ChromeBrowsingContext*>(aContext);
}

/* static */ const ChromeBrowsingContext* ChromeBrowsingContext::Cast(
    const BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<const ChromeBrowsingContext*>(aContext);
}

void ChromeBrowsingContext::GetWindowGlobals(
    nsTArray<RefPtr<WindowGlobalParent>>& aWindows) {
  aWindows.SetCapacity(mWindowGlobals.Count());
  for (auto iter = mWindowGlobals.Iter(); !iter.Done(); iter.Next()) {
    aWindows.AppendElement(iter.Get()->GetKey());
  }
}

void ChromeBrowsingContext::RegisterWindowGlobal(WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(!mWindowGlobals.Contains(aGlobal), "Global already registered!");
  mWindowGlobals.PutEntry(aGlobal);
}

void ChromeBrowsingContext::UnregisterWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(mWindowGlobals.Contains(aGlobal), "Global not registered!");
  mWindowGlobals.RemoveEntry(aGlobal);

  // Our current window global should be in our mWindowGlobals set. If it's not
  // anymore, clear that reference.
  if (aGlobal == mCurrentWindowGlobal) {
    mCurrentWindowGlobal = nullptr;
  }
}

void ChromeBrowsingContext::SetCurrentWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(mWindowGlobals.Contains(aGlobal), "Global not registered!");

  // TODO: This should probably assert that the processes match.
  mCurrentWindowGlobal = aGlobal;
}

JSObject* ChromeBrowsingContext::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return ChromeBrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

void ChromeBrowsingContext::Traverse(nsCycleCollectionTraversalCallback& cb) {
  ChromeBrowsingContext* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindowGlobals);
}

void ChromeBrowsingContext::Unlink() {
  ChromeBrowsingContext* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindowGlobals);
}

}  // namespace dom
}  // namespace mozilla
