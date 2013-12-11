/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PromiseCallback.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(PromiseCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PromiseCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PromiseCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(PromiseCallback)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PromiseCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PromiseCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

PromiseCallback::PromiseCallback()
{
  MOZ_COUNT_CTOR(PromiseCallback);
}

PromiseCallback::~PromiseCallback()
{
  MOZ_COUNT_DTOR(PromiseCallback);
}

static void
EnterCompartment(Maybe<JSAutoCompartment>& aAc, JSContext* aCx,
                 JS::Handle<JS::Value> aValue)
{
  // FIXME Bug 878849
  if (aValue.isObject()) {
    JS::Rooted<JSObject*> rooted(aCx, &aValue.toObject());
    aAc.construct(aCx, rooted);
  }
}

// ResolvePromiseCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(ResolvePromiseCallback,
                                     PromiseCallback,
                                     mPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ResolvePromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(ResolvePromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(ResolvePromiseCallback, PromiseCallback)

ResolvePromiseCallback::ResolvePromiseCallback(Promise* aPromise)
  : mPromise(aPromise)
{
  MOZ_ASSERT(aPromise);
  MOZ_COUNT_CTOR(ResolvePromiseCallback);
}

ResolvePromiseCallback::~ResolvePromiseCallback()
{
  MOZ_COUNT_DTOR(ResolvePromiseCallback);
}

void
ResolvePromiseCallback::Call(JS::Handle<JS::Value> aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.
  JSContext *cx = nsContentUtils::GetDefaultJSContextForThread();

  Maybe<AutoCxPusher> pusher;
  if (NS_IsMainThread()) {
    pusher.construct(cx);
  }

  Maybe<JSAutoCompartment> ac;
  EnterCompartment(ac, cx, aValue);

  mPromise->ResolveInternal(cx, aValue, Promise::SyncTask);
}

// RejectPromiseCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(RejectPromiseCallback,
                                     PromiseCallback,
                                     mPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(RejectPromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(RejectPromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(RejectPromiseCallback, PromiseCallback)

RejectPromiseCallback::RejectPromiseCallback(Promise* aPromise)
  : mPromise(aPromise)
{
  MOZ_ASSERT(aPromise);
  MOZ_COUNT_CTOR(RejectPromiseCallback);
}

RejectPromiseCallback::~RejectPromiseCallback()
{
  MOZ_COUNT_DTOR(RejectPromiseCallback);
}

void
RejectPromiseCallback::Call(JS::Handle<JS::Value> aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.
  JSContext *cx = nsContentUtils::GetDefaultJSContextForThread();

  Maybe<AutoCxPusher> pusher;
  if (NS_IsMainThread()) {
    pusher.construct(cx);
  }

  Maybe<JSAutoCompartment> ac;
  EnterCompartment(ac, cx, aValue);

  mPromise->RejectInternal(cx, aValue, Promise::SyncTask);
}

// WrapperPromiseCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(WrapperPromiseCallback,
                                     PromiseCallback,
                                     mNextPromise, mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WrapperPromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(WrapperPromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(WrapperPromiseCallback, PromiseCallback)

WrapperPromiseCallback::WrapperPromiseCallback(Promise* aNextPromise,
                                               AnyCallback* aCallback)
  : mNextPromise(aNextPromise)
  , mCallback(aCallback)
{
  MOZ_ASSERT(aNextPromise);
  MOZ_COUNT_CTOR(WrapperPromiseCallback);
}

WrapperPromiseCallback::~WrapperPromiseCallback()
{
  MOZ_COUNT_DTOR(WrapperPromiseCallback);
}

void
WrapperPromiseCallback::Call(JS::Handle<JS::Value> aValue)
{
  // AutoCxPusher and co. interact with xpconnect, which crashes on
  // workers. On workers we'll get the right context from
  // GetDefaultJSContextForThread(), and since there is only one context, we
  // don't need to push or pop it from the stack.
  JSContext* cx = nsContentUtils::GetDefaultJSContextForThread();

  Maybe<AutoCxPusher> pusher;
  if (NS_IsMainThread()) {
    pusher.construct(cx);
  }

  Maybe<JSAutoCompartment> ac;
  EnterCompartment(ac, cx, aValue);

  ErrorResult rv;

  // If invoking callback threw an exception, run resolver's reject with the
  // thrown exception as argument and the synchronous flag set.
  JS::Rooted<JS::Value> value(cx,
    mCallback->Call(mNextPromise->GetParentObject(), aValue, rv,
                    CallbackObject::eRethrowExceptions));

  rv.WouldReportJSException();

  if (rv.Failed() && rv.IsJSException()) {
    JS::Rooted<JS::Value> value(cx);
    rv.StealJSException(cx, &value);

    Maybe<JSAutoCompartment> ac2;
    EnterCompartment(ac2, cx, value);
    mNextPromise->RejectInternal(cx, value, Promise::SyncTask);
    return;
  }

  // Otherwise, run resolver's resolve with value and the synchronous flag
  // set.
  Maybe<JSAutoCompartment> ac2;
  EnterCompartment(ac2, cx, value);
  mNextPromise->ResolveInternal(cx, value, Promise::SyncTask);
}

// SimpleWrapperPromiseCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(SimpleWrapperPromiseCallback,
                                     PromiseCallback,
                                     mPromise, mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SimpleWrapperPromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(SimpleWrapperPromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(SimpleWrapperPromiseCallback, PromiseCallback)

SimpleWrapperPromiseCallback::SimpleWrapperPromiseCallback(Promise* aPromise,
                                                           AnyCallback* aCallback)
  : mPromise(aPromise)
  , mCallback(aCallback)
{
  MOZ_ASSERT(aPromise);
  MOZ_COUNT_CTOR(SimpleWrapperPromiseCallback);
}

SimpleWrapperPromiseCallback::~SimpleWrapperPromiseCallback()
{
  MOZ_COUNT_DTOR(SimpleWrapperPromiseCallback);
}

void
SimpleWrapperPromiseCallback::Call(JS::Handle<JS::Value> aValue)
{
  ErrorResult rv;
  mCallback->Call(mPromise, aValue, rv);
}

// NativePromiseCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(NativePromiseCallback,
                                     PromiseCallback, mHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(NativePromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(NativePromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(NativePromiseCallback, PromiseCallback)

NativePromiseCallback::NativePromiseCallback(PromiseNativeHandler* aHandler,
                                             Promise::PromiseState aState)
  : mHandler(aHandler)
  , mState(aState)
{
  MOZ_ASSERT(aHandler);
  MOZ_COUNT_CTOR(NativePromiseCallback);
}

NativePromiseCallback::~NativePromiseCallback()
{
  MOZ_COUNT_DTOR(NativePromiseCallback);
}

void
NativePromiseCallback::Call(JS::Handle<JS::Value> aValue)
{
  if (mState == Promise::Resolved) {
    mHandler->ResolvedCallback(aValue);
    return;
  }

  if (mState == Promise::Rejected) {
    mHandler->RejectedCallback(aValue);
    return;
  }

  NS_NOTREACHED("huh?");
}

/* static */ PromiseCallback*
PromiseCallback::Factory(Promise* aNextPromise, AnyCallback* aCallback,
                         Task aTask)
{
  MOZ_ASSERT(aNextPromise);

  // If we have a callback and a next resolver, we have to exec the callback and
  // then propagate the return value to the next resolver->resolve().
  if (aCallback) {
    return new WrapperPromiseCallback(aNextPromise, aCallback);
  }

  if (aTask == Resolve) {
    return new ResolvePromiseCallback(aNextPromise);
  }

  if (aTask == Reject) {
    return new RejectPromiseCallback(aNextPromise);
  }

  MOZ_ASSERT(false, "This should not happen");
  return nullptr;
}

} // namespace dom
} // namespace mozilla
