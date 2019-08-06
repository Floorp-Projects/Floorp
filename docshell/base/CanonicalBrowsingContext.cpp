/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CanonicalBrowsingContext.h"

#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ContentProcessManager.h"

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

CanonicalBrowsingContext::CanonicalBrowsingContext(BrowsingContext* aParent,
                                                   BrowsingContextGroup* aGroup,
                                                   uint64_t aBrowsingContextId,
                                                   uint64_t aProcessId,
                                                   BrowsingContext::Type aType)
    : BrowsingContext(aParent, aGroup, aBrowsingContextId, aType),
      mProcessId(aProcessId) {
  // You are only ever allowed to create CanonicalBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

/* static */
already_AddRefed<CanonicalBrowsingContext> CanonicalBrowsingContext::Get(
    uint64_t aId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return BrowsingContext::Get(aId).downcast<CanonicalBrowsingContext>();
}

/* static */
CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<CanonicalBrowsingContext*>(aContext);
}

/* static */
const CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    const BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<const CanonicalBrowsingContext*>(aContext);
}

ContentParent* CanonicalBrowsingContext::GetContentParent() const {
  if (mProcessId == 0) {
    return nullptr;
  }

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  return cpm->GetContentProcessById(ContentParentId(mProcessId));
}

void CanonicalBrowsingContext::GetCurrentRemoteType(nsAString& aRemoteType,
                                                    ErrorResult& aRv) const {
  // If we're in the parent process, dump out the void string.
  if (mProcessId == 0) {
    aRemoteType.Assign(VoidString());
    return;
  }

  ContentParent* cp = GetContentParent();
  if (!cp) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRemoteType.Assign(cp->GetRemoteType());
}

void CanonicalBrowsingContext::SetOwnerProcessId(uint64_t aProcessId) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("SetOwnerProcessId for 0x%08" PRIx64 " (0x%08" PRIx64
           " -> 0x%08" PRIx64 ")",
           Id(), mProcessId, aProcessId));

  mProcessId = aProcessId;
}

void CanonicalBrowsingContext::GetWindowGlobals(
    nsTArray<RefPtr<WindowGlobalParent>>& aWindows) {
  aWindows.SetCapacity(mWindowGlobals.Count());
  for (auto iter = mWindowGlobals.Iter(); !iter.Done(); iter.Next()) {
    aWindows.AppendElement(iter.Get()->GetKey());
  }
}

void CanonicalBrowsingContext::RegisterWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(!mWindowGlobals.Contains(aGlobal), "Global already registered!");
  mWindowGlobals.PutEntry(aGlobal);
}

void CanonicalBrowsingContext::UnregisterWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(mWindowGlobals.Contains(aGlobal), "Global not registered!");
  mWindowGlobals.RemoveEntry(aGlobal);

  // Our current window global should be in our mWindowGlobals set. If it's not
  // anymore, clear that reference.
  if (aGlobal == mCurrentWindowGlobal) {
    mCurrentWindowGlobal = nullptr;
  }
}

void CanonicalBrowsingContext::SetCurrentWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(mWindowGlobals.Contains(aGlobal), "Global not registered!");

  // TODO: This should probably assert that the processes match.
  mCurrentWindowGlobal = aGlobal;
}

void CanonicalBrowsingContext::SetEmbedderWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_RELEASE_ASSERT(aGlobal, "null embedder");
  if (RefPtr<BrowsingContext> parent = GetParent()) {
    MOZ_RELEASE_ASSERT(aGlobal->BrowsingContext() == parent,
                       "Embedder has incorrect browsing context");
  }

  mEmbedderWindowGlobal = aGlobal;
}

bool CanonicalBrowsingContext::ValidateTransaction(
    const Transaction& aTransaction, ContentParent* aProcess) {
  if (MOZ_LOG_TEST(GetLog(), LogLevel::Debug)) {
#define MOZ_BC_FIELD(name, ...)                                               \
  if (aTransaction.m##name.isSome()) {                                        \
    MOZ_LOG(GetLog(), LogLevel::Debug,                                        \
            ("Validate Transaction 0x%08" PRIx64 " set " #name                \
             " (from: 0x%08" PRIx64 " owner: 0x%08" PRIx64 ")",               \
             Id(), aProcess ? static_cast<uint64_t>(aProcess->ChildID()) : 0, \
             mProcessId));                                                    \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"
  }

  // Check that the correct process is performing sets for transactions with
  // non-racy fields.
  if (aTransaction.HasNonRacyField()) {
    if (NS_WARN_IF(aProcess && mProcessId != aProcess->ChildID())) {
      return false;
    }
  }

  return true;
}

JSObject* CanonicalBrowsingContext::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CanonicalBrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

void CanonicalBrowsingContext::Traverse(
    nsCycleCollectionTraversalCallback& cb) {
  CanonicalBrowsingContext* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindowGlobals, mCurrentWindowGlobal,
                                    mEmbedderWindowGlobal);
}

void CanonicalBrowsingContext::Unlink() {
  CanonicalBrowsingContext* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindowGlobals, mCurrentWindowGlobal,
                                  mEmbedderWindowGlobal);
}

void CanonicalBrowsingContext::NotifyStartDelayedAutoplayMedia() {
  if (!mCurrentWindowGlobal) {
    return;
  }

  // As this function would only be called when user click the play icon on the
  // tab bar. That's clear user intent to play, so gesture activate the browsing
  // context so that the block-autoplay logic allows the media to autoplay.
  NotifyUserGestureActivation();
  AUTOPLAY_LOG("NotifyStartDelayedAutoplayMedia for chrome bc 0x%08" PRIx64,
               Id());
  StartDelayedAutoplayMediaComponents();
  // Notfiy all content browsing contexts which are related with the canonical
  // browsing content tree to start delayed autoplay media.

  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendStartDelayedAutoplayMediaComponents(this);
  });
}

void CanonicalBrowsingContext::NotifyMediaMutedChanged(bool aMuted) {
  nsPIDOMWindowOuter* window = GetDOMWindow();
  if (window) {
    window->SetAudioMuted(aMuted);
  }
  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendSetMediaMuted(this, aMuted);
  });
}

void CanonicalBrowsingContext::SetFieldEpochsForChild(
    ContentParent* aChild, const BrowsingContext::FieldEpochs& aEpochs) {
  mChildFieldEpochs.Put(aChild->ChildID(), aEpochs);
}

const BrowsingContext::FieldEpochs&
CanonicalBrowsingContext::GetFieldEpochsForChild(ContentParent* aChild) {
  static const BrowsingContext::FieldEpochs sDefaultFieldEpochs;

  if (auto entry = mChildFieldEpochs.Lookup(aChild->ChildID())) {
    return entry.Data();
  }
  return sDefaultFieldEpochs;
}

}  // namespace dom
}  // namespace mozilla
