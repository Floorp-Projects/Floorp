/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"

#include "mozilla/dom/AbortSignalBinding.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"

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

already_AddRefed<AbortSignal> AbortSignal::Abort(
    GlobalObject& aGlobal, JS::Handle<JS::Value> aReason) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<AbortSignal> abortSignal = new AbortSignal(global, true, aReason);
  return abortSignal.forget();
}

class AbortSignalTimeoutHandler final : public TimeoutHandler {
 public:
  AbortSignalTimeoutHandler(JSContext* aCx, AbortSignal* aSignal)
      : TimeoutHandler(aCx), mSignal(aSignal) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AbortSignalTimeoutHandler)

  // https://dom.spec.whatwg.org/#dom-abortsignal-timeout
  // Step 3
  MOZ_CAN_RUN_SCRIPT bool Call(const char* /* unused */) override {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mSignal->GetParentObject()))) {
      // (false is only for setInterval, see
      // nsGlobalWindowInner::RunTimeoutHandler)
      return true;
    }

    // Step 1. Queue a global task on the timer task source given global to
    // signal abort given signal and a new "TimeoutError" DOMException.
    JS::Rooted<JS::Value> exception(jsapi.cx());
    RefPtr<DOMException> dom = DOMException::Create(NS_ERROR_DOM_TIMEOUT_ERR);
    if (NS_WARN_IF(!ToJSValue(jsapi.cx(), dom, &exception))) {
      return true;
    }

    mSignal->SignalAbort(exception);
    return true;
  }

 private:
  ~AbortSignalTimeoutHandler() override = default;

  RefPtr<AbortSignal> mSignal;
};

NS_IMPL_CYCLE_COLLECTION(AbortSignalTimeoutHandler, mSignal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AbortSignalTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbortSignalTimeoutHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortSignalTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static void SetTimeoutForGlobal(GlobalObject& aGlobal, TimeoutHandler& aHandler,
                                int32_t timeout, ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> innerWindow =
        do_QueryInterface(aGlobal.GetAsSupports());
    if (!innerWindow) {
      aRv.ThrowInvalidStateError("Could not find window.");
      return;
    }

    int32_t handle;
    nsresult rv = innerWindow->TimeoutManager().SetTimeout(
        &aHandler, timeout, /* aIsInterval */ false,
        Timeout::Reason::eAbortSignalTimeout, &handle);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  } else {
    WorkerPrivate* workerPrivate =
        GetWorkerPrivateFromContext(aGlobal.Context());
    workerPrivate->SetTimeout(aGlobal.Context(), &aHandler, timeout,
                              /* aIsInterval */ false,
                              Timeout::Reason::eAbortSignalTimeout, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

// https://dom.spec.whatwg.org/#dom-abortsignal-timeout
already_AddRefed<AbortSignal> AbortSignal::Timeout(GlobalObject& aGlobal,
                                                   uint64_t aMilliseconds,
                                                   ErrorResult& aRv) {
  // Step 2. Let global be signal’s relevant global object.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  // Step 1. Let signal be a new AbortSignal object.
  RefPtr<AbortSignal> signal =
      new AbortSignal(global, false, JS::UndefinedHandleValue);

  // Step 3. Run steps after a timeout given global, "AbortSignal-timeout",
  // milliseconds, and the following step: ...
  RefPtr<TimeoutHandler> handler =
      new AbortSignalTimeoutHandler(aGlobal.Context(), signal);

  // Note: We only supports int32_t range intervals
  int32_t timeout =
      aMilliseconds > uint64_t(std::numeric_limits<int32_t>::max())
          ? std::numeric_limits<int32_t>::max()
          : static_cast<int32_t>(aMilliseconds);

  SetTimeoutForGlobal(aGlobal, *handler, timeout, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 4. Return signal.
  return signal.forget();
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
