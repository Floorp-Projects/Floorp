/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCursor.h"
#include "nsError.h"
#include "mozilla/dom/DOMCursorBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMCursor,
                                                  DOMRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMCursor,
                                                DOMRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMCursor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMCursor)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(DOMCursor, DOMRequest)
NS_IMPL_RELEASE_INHERITED(DOMCursor, DOMRequest)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DOMCursor, DOMRequest)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCursor::DOMCursor(nsIDOMWindow* aWindow, nsICursorContinueCallback* aCallback)
  : DOMRequest(aWindow)
  , mCallback(aCallback)
  , mFinished(false)
{
}

void
DOMCursor::Reset()
{
  MOZ_ASSERT(!mFinished);

  // Reset the request state so we can FireSuccess() again.
  if (mRooted) {
    UnrootResultVal();
  }
  mDone = false;
}

void
DOMCursor::FireDone()
{
  Reset();
  mFinished = true;
  FireSuccess(JSVAL_VOID);
}

NS_IMETHODIMP
DOMCursor::GetDone(bool *aDone)
{
  *aDone = Done();
  return NS_OK;
}

NS_IMETHODIMP
DOMCursor::Continue()
{
  ErrorResult rv;
  Continue(rv);
  return rv.ErrorCode();
}

void
DOMCursor::Continue(ErrorResult& aRv)
{
  MOZ_ASSERT(mCallback, "If you're creating your own cursor class with no callback, you should override Continue()");

  // We need to have a result here because we must be in a 'success' state.
  if (mResult == JSVAL_VOID) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  Reset();
  mCallback->HandleContinue();
}

/* virtual */ JSObject*
DOMCursor::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return DOMCursorBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
