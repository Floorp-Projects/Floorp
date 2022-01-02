/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"

#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/AbortSignalBinding.h"
#include "mozilla/RefPtr.h"

namespace mozilla::dom {

// AbortSignalImpl
// ----------------------------------------------------------------------------

AbortSignalImpl::AbortSignalImpl(bool aAborted) : mAborted(aAborted) {}

bool AbortSignalImpl::Aborted() const { return mAborted; }

// https://dom.spec.whatwg.org/#abortsignal-signal-abort steps 1-4
void AbortSignalImpl::SignalAbort() {
  // Step 1.
  if (mAborted) {
    return;
  }

  // Step 2.
  mAborted = true;

  // Step 3.
  // When there are multiple followers, the follower removal algorithm
  // https://dom.spec.whatwg.org/#abortsignal-remove could be invoked in an
  // earlier algorithm to remove a later algorithm, so |mFollowers| must be a
  // |nsTObserverArray| to defend against mutation.
  for (RefPtr<AbortFollower> follower : mFollowers.ForwardRange()) {
    MOZ_ASSERT(follower->mFollowingSignal == this);
    follower->RunAbortAlgorithm();
  }

  // Step 4.
  // Clear follower->signal links, then clear signal->follower links.
  for (AbortFollower* follower : mFollowers.ForwardRange()) {
    follower->mFollowingSignal = nullptr;
  }
  mFollowers.Clear();
}

/* static */ void AbortSignalImpl::Traverse(
    AbortSignalImpl* aSignal, nsCycleCollectionTraversalCallback& cb) {
  // To be filled in shortly.
}

// AbortSignal
// ----------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AbortSignal,
                                                  DOMEventTargetHelper)
  AbortSignalImpl::Traverse(static_cast<AbortSignalImpl*>(tmp), cb);
  AbortFollower::Traverse(static_cast<AbortFollower*>(tmp), cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AbortSignal,
                                                DOMEventTargetHelper)
  AbortSignalImpl::Unlink(static_cast<AbortSignalImpl*>(tmp));
  AbortFollower::Unlink(static_cast<AbortFollower*>(tmp));
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(AbortSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AbortSignal, DOMEventTargetHelper)

AbortSignal::AbortSignal(nsIGlobalObject* aGlobalObject, bool aAborted)
    : DOMEventTargetHelper(aGlobalObject), AbortSignalImpl(aAborted) {}

JSObject* AbortSignal::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return AbortSignal_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<AbortSignal> AbortSignal::Abort(GlobalObject& aGlobal) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<AbortSignal> abortSignal = new AbortSignal(global, true);
  return abortSignal.forget();
}

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
void AbortSignal::SignalAbort() {
  // Steps 1-4.
  AbortSignalImpl::SignalAbort();

  // Step 5.
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, u"abort"_ns, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

// AbortFollower
// ----------------------------------------------------------------------------

AbortFollower::~AbortFollower() { Unfollow(); }

// https://dom.spec.whatwg.org/#abortsignal-add
void AbortFollower::Follow(AbortSignalImpl* aSignal) {
  // Step 1.
  if (aSignal->mAborted) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(aSignal);

  Unfollow();

  // Step 2.
  mFollowingSignal = aSignal;
  MOZ_ASSERT(!aSignal->mFollowers.Contains(this));
  aSignal->mFollowers.AppendElement(this);
}

// https://dom.spec.whatwg.org/#abortsignal-remove
void AbortFollower::Unfollow() {
  if (mFollowingSignal) {
    // |Unfollow| is called by cycle-collection unlink code that runs in no
    // guaranteed order.  So we can't, symmetric with |Follow| above, assert
    // that |this| will be found in |mFollowingSignal->mFollowers|.
    mFollowingSignal->mFollowers.RemoveElement(this);
    mFollowingSignal = nullptr;
  }
}

bool AbortFollower::IsFollowing() const { return !!mFollowingSignal; }

/* static */ void AbortFollower::Traverse(
    AbortFollower* aFollower, nsCycleCollectionTraversalCallback& cb) {
  ImplCycleCollectionTraverse(cb, aFollower->mFollowingSignal,
                              "mFollowingSignal", 0);
}

}  // namespace mozilla::dom
