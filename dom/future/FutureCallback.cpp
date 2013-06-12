/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FutureCallback.h"
#include "mozilla/dom/Future.h"
#include "mozilla/dom/FutureResolver.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(FutureCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FutureCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FutureCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FutureCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FutureCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

FutureCallback::FutureCallback()
{
  MOZ_COUNT_CTOR(FutureCallback);
}

FutureCallback::~FutureCallback()
{
  MOZ_COUNT_DTOR(FutureCallback);
}

// ResolveFutureCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(ResolveFutureCallback,
                                     FutureCallback,
                                     mResolver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ResolveFutureCallback)
NS_INTERFACE_MAP_END_INHERITING(FutureCallback)

NS_IMPL_ADDREF_INHERITED(ResolveFutureCallback, FutureCallback)
NS_IMPL_RELEASE_INHERITED(ResolveFutureCallback, FutureCallback)

ResolveFutureCallback::ResolveFutureCallback(FutureResolver* aResolver)
  : mResolver(aResolver)
{
  MOZ_ASSERT(aResolver);
  MOZ_COUNT_CTOR(ResolveFutureCallback);
}

ResolveFutureCallback::~ResolveFutureCallback()
{
  MOZ_COUNT_DTOR(ResolveFutureCallback);
}

void
ResolveFutureCallback::Call(const Optional<JS::Handle<JS::Value> >& aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.
  AutoJSContext cx;
  // FIXME Bug 878849
  Maybe<JSAutoCompartment> ac;
  if (aValue.WasPassed() && aValue.Value().isObject()) {
    JS::Rooted<JSObject*> rooted(cx, &aValue.Value().toObject());
    ac.construct(cx, rooted);
  }

  mResolver->ResolveInternal(cx, aValue, FutureResolver::SyncTask);
}

// RejectFutureCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(RejectFutureCallback,
                                     FutureCallback,
                                     mResolver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(RejectFutureCallback)
NS_INTERFACE_MAP_END_INHERITING(FutureCallback)

NS_IMPL_ADDREF_INHERITED(RejectFutureCallback, FutureCallback)
NS_IMPL_RELEASE_INHERITED(RejectFutureCallback, FutureCallback)

RejectFutureCallback::RejectFutureCallback(FutureResolver* aResolver)
  : mResolver(aResolver)
{
  MOZ_ASSERT(aResolver);
  MOZ_COUNT_CTOR(RejectFutureCallback);
}

RejectFutureCallback::~RejectFutureCallback()
{
  MOZ_COUNT_DTOR(RejectFutureCallback);
}

void
RejectFutureCallback::Call(const Optional<JS::Handle<JS::Value> >& aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.
  AutoJSContext cx;
  // FIXME Bug 878849
  Maybe<JSAutoCompartment> ac;
  if (aValue.WasPassed() && aValue.Value().isObject()) {
    JS::Rooted<JSObject*> rooted(cx, &aValue.Value().toObject());
    ac.construct(cx, rooted);
  }

  mResolver->RejectInternal(cx, aValue, FutureResolver::SyncTask);
}

// WrapperFutureCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(WrapperFutureCallback,
                                     FutureCallback,
                                     mNextResolver, mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WrapperFutureCallback)
NS_INTERFACE_MAP_END_INHERITING(FutureCallback)

NS_IMPL_ADDREF_INHERITED(WrapperFutureCallback, FutureCallback)
NS_IMPL_RELEASE_INHERITED(WrapperFutureCallback, FutureCallback)

WrapperFutureCallback::WrapperFutureCallback(FutureResolver* aNextResolver,
                                             AnyCallback* aCallback)
  : mNextResolver(aNextResolver)
  , mCallback(aCallback)
{
  MOZ_ASSERT(aNextResolver);
  MOZ_COUNT_CTOR(WrapperFutureCallback);
}

WrapperFutureCallback::~WrapperFutureCallback()
{
  MOZ_COUNT_DTOR(WrapperFutureCallback);
}

void
WrapperFutureCallback::Call(const Optional<JS::Handle<JS::Value> >& aValue)
{
  AutoJSContext cx;
  // FIXME Bug 878849
  Maybe<JSAutoCompartment> ac;
  if (aValue.WasPassed() && aValue.Value().isObject()) {
    JS::Rooted<JSObject*> rooted(cx, &aValue.Value().toObject());
    ac.construct(cx, rooted);
  }

  ErrorResult rv;

  // If invoking callback threw an exception, run resolver's reject with the
  // thrown exception as argument and the synchronous flag set.
  Optional<JS::Handle<JS::Value> > value(cx,
    mCallback->Call(mNextResolver->GetParentObject(), aValue, rv,
                    CallbackObject::eRethrowExceptions));

  rv.WouldReportJSException();

  if (rv.Failed() && rv.IsJSException()) {
    Optional<JS::Handle<JS::Value> > value(cx);
    rv.StealJSException(cx, &value.Value());
    mNextResolver->RejectInternal(cx, value, FutureResolver::SyncTask);
    return;
  }

  // Otherwise, run resolver's resolve with value and the synchronous flag
  // set.
  mNextResolver->ResolveInternal(cx, value, FutureResolver::SyncTask);
}

// SimpleWrapperFutureCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(SimpleWrapperFutureCallback,
                                     FutureCallback,
                                     mFuture, mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SimpleWrapperFutureCallback)
NS_INTERFACE_MAP_END_INHERITING(FutureCallback)

NS_IMPL_ADDREF_INHERITED(SimpleWrapperFutureCallback, FutureCallback)
NS_IMPL_RELEASE_INHERITED(SimpleWrapperFutureCallback, FutureCallback)

SimpleWrapperFutureCallback::SimpleWrapperFutureCallback(Future* aFuture,
                                                         AnyCallback* aCallback)
  : mFuture(aFuture)
  , mCallback(aCallback)
{
  MOZ_ASSERT(aFuture);
  MOZ_COUNT_CTOR(SimpleWrapperFutureCallback);
}

SimpleWrapperFutureCallback::~SimpleWrapperFutureCallback()
{
  MOZ_COUNT_DTOR(SimpleWrapperFutureCallback);
}

void
SimpleWrapperFutureCallback::Call(const Optional<JS::Handle<JS::Value> >& aValue)
{
  ErrorResult rv;
  mCallback->Call(mFuture, aValue, rv);
}

/* static */ FutureCallback*
FutureCallback::Factory(FutureResolver* aNextResolver,
                        AnyCallback* aCallback, Task aTask)
{
  MOZ_ASSERT(aNextResolver);

  // If we have a callback and a next resolver, we have to exec the callback and
  // then propagate the return value to the next resolver->resolve().
  if (aCallback) {
    return new WrapperFutureCallback(aNextResolver, aCallback);
  }

  if (aTask == Resolve) {
    return new ResolveFutureCallback(aNextResolver);
  }

  if (aTask == Reject) {
    return new RejectFutureCallback(aNextResolver);
  }

  MOZ_ASSERT(false, "This should not happen");
  return nullptr;
}

} // namespace dom
} // namespace mozilla
