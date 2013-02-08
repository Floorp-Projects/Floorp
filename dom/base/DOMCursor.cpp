/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCursor.h"
#include "nsIDOMClassInfo.h"
#include "nsError.h"

DOMCI_DATA(DOMCursor, mozilla::dom::DOMCursor)

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
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMCursor)
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
  MOZ_ASSERT(aCallback);
}

void
DOMCursor::Reset()
{
  MOZ_ASSERT(!mFinished);

  // Reset the request state so we can FireSuccess() again.
  if (mRooted) {
    UnrootResultVal();
  }
  mResult = JSVAL_VOID;
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
  *aDone = mFinished;
  return NS_OK;
}

NS_IMETHODIMP
DOMCursor::Continue()
{
  // We need to have a result here because we must be in a 'success' state.
  if (mResult == JSVAL_VOID) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  Reset();
  mCallback->HandleContinue();

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
