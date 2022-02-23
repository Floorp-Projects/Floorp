/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"

#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/AbortSignalBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {

// AbortSignalImpl
// ----------------------------------------------------------------------------

AbortSignalImpl::AbortSignalImpl(bool aAborted, JS::Handle<JS::Value> aReason)
    : mReason(aReason), mAborted(aAborted) {
  MOZ_ASSERT_IF(!mReason.isUndefined(), mAborted);
}

bool AbortSignalImpl::Aborted() const { return mAborted; }

void AbortSignalImpl::GetReason(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aReason) {
  if (!mAborted) {
    return;
  }
  MaybeAssignAbortError(aCx);
  aReason.set(mReason);
}

JS::Value AbortSignalImpl::RawReason() const { return mReason.get(); }

// https://dom.spec.whatwg.org/#abortsignal-signal-abort steps 1-4
void AbortSignalImpl::SignalAbort(JS::Handle<JS::Value> aReason) {
  // Step 1.
  if (mAborted) {
    return;
  }

  // Step 2.
  mAborted = true;
  mReason = aReason;

  // Step 3.
  // When there are multiple followers, the follower removal algorithm
  // https://dom.spec.whatwg.org/#abortsignal-remove could be invoked in an
  // earlier algorithm to remove a later algorithm, so |mFollowers| must be a
  // |nsTObserverArray| to defend against mutation.
  for (RefPtr<AbortFollower>& follower : mFollowers.ForwardRange()) {
    MOZ_ASSERT(follower->mFollowingSignal == this);
    follower->RunAbortAlgorithm();
  }

  // Step 4.
  UnlinkFollowers();
}

void AbortSignalImpl::Traverse(AbortSignalImpl* aSignal,
                               nsCycleCollectionTraversalCallback& cb) {
  ImplCycleCollectionTraverse(cb, aSignal->mFollowers, "mFollowers", 0);
}

void AbortSignalImpl::Unlink(AbortSignalImpl* aSignal) {
  aSignal->mReason.setUndefined();
  aSignal->UnlinkFollowers();
}

void AbortSignalImpl::MaybeAssignAbortError(JSContext* aCx) {
  MOZ_ASSERT(mAborted);
  if (!mReason.isUndefined()) {
    return;
  }

  JS::Rooted<JS::Value> exception(aCx);
  RefPtr<DOMException> dom = DOMException::Create(NS_ERROR_DOM_ABORT_ERR);

  if (NS_WARN_IF(!ToJSValue(aCx, dom, &exception))) {
    return;
  }

  mReason.set(exception);
}

void AbortSignalImpl::UnlinkFollowers() {
  // Manually unlink all followers before destructing the array, or otherwise
  // the array will be accessed by Unfollow() while being destructed.
  for (RefPtr<AbortFollower>& follower : mFollowers.ForwardRange()) {
    follower->mFollowingSignal = nullptr;
  }
  mFollowers.Clear();
}

// AbortSignal
// ----------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AbortSignal,
                                                  DOMEventTargetHelper)
  AbortSignalImpl::Traverse(static_cast<AbortSignalImpl*>(tmp), cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AbortSignal,
                                                DOMEventTargetHelper)
  AbortSignalImpl::Unlink(static_cast<AbortSignalImpl*>(tmp));
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(AbortSignal,
                                               DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReason)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(AbortSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AbortSignal, DOMEventTargetHelper)

AbortSignal::AbortSignal(nsIGlobalObject* aGlobalObject, bool aAborted,
                         JS::Handle<JS::Value> aReason)
    : DOMEventTargetHelper(aGlobalObject), AbortSignalImpl(aAborted, aReason) {
  mozilla::HoldJSObjects(this);
}

JSObject* AbortSignal::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return AbortSignal_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<AbortSignal> AbortSignal::Abort(GlobalObject& aGlobal,
                                                 JS::Handle<JS::Value> aReason,
                                                 ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<AbortSignal> abortSignal = new AbortSignal(global, true, aReason);
  return abortSignal.forget();
}

// https://dom.spec.whatwg.org/#dom-abortsignal-throwifaborted
void AbortSignal::ThrowIfAborted(JSContext* aCx, ErrorResult& aRv) {
  aRv.MightThrowJSException();

  if (Aborted()) {
    JS::Rooted<JS::Value> reason(aCx);
    GetReason(aCx, &reason);
    aRv.ThrowJSException(aCx, reason);
  }
}

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
void AbortSignal::SignalAbort(JS::Handle<JS::Value> aReason) {
  // Step 1, in case "signal abort" algorithm is called directly
  if (Aborted()) {
    return;
  }

  // Steps 1-4.
  AbortSignalImpl::SignalAbort(aReason);

  // Step 5.
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, u"abort"_ns, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

void AbortSignal::RunAbortAlgorithm() {
  JS::Rooted<JS::Value> reason(RootingCx(), Signal()->RawReason());
  SignalAbort(reason);
}

AbortSignal::~AbortSignal() { mozilla::DropJSObjects(this); }

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

}  // namespace mozilla::dom
