/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"

#include "mozilla/dom/Event.h"
#include "mozilla/dom/AbortSignalBinding.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

// AbortSignalImpl
// ----------------------------------------------------------------------------

AbortSignalImpl::AbortSignalImpl(bool aAborted) : mAborted(aAborted) {}

bool AbortSignalImpl::Aborted() const { return mAborted; }

void AbortSignalImpl::SignalAbort() {
  if (mAborted) {
    return;
  }

  mAborted = true;

  // Let's inform the followers.
  for (RefPtr<AbortFollower> follower : mFollowers.ForwardRange()) {
    follower->RunAbortAlgorithm();
  }
}

void AbortSignalImpl::AddFollower(AbortFollower* aFollower) {
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  if (!mFollowers.Contains(aFollower)) {
    mFollowers.AppendElement(aFollower);
  }
}

void AbortSignalImpl::RemoveFollower(AbortFollower* aFollower) {
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  mFollowers.RemoveElement(aFollower);
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

void AbortSignal::SignalAbort() {
  AbortSignalImpl::SignalAbort();

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

void AbortFollower::Follow(AbortSignalImpl* aSignal) {
  MOZ_DIAGNOSTIC_ASSERT(aSignal);

  Unfollow();

  mFollowingSignal = aSignal;
  aSignal->AddFollower(this);
}

void AbortFollower::Unfollow() {
  if (mFollowingSignal) {
    mFollowingSignal->RemoveFollower(this);
    mFollowingSignal = nullptr;
  }
}

bool AbortFollower::IsFollowing() const { return !!mFollowingSignal; }

/* static */ void AbortFollower::Traverse(
    AbortFollower* aFollower, nsCycleCollectionTraversalCallback& cb) {
  ImplCycleCollectionTraverse(cb, aFollower->mFollowingSignal,
                              "mFollowingSignal", 0);
}

}  // namespace dom
}  // namespace mozilla
