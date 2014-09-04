/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageCursorCallback.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIMobileMessageCallback.h"
#include "nsServiceManagerUtils.h"      // for do_GetService

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MobileMessageCursor, DOMCursor,
                                   mPendingResults)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MobileMessageCursor)
NS_INTERFACE_MAP_END_INHERITING(DOMCursor)

NS_IMPL_ADDREF_INHERITED(MobileMessageCursor, DOMCursor)
NS_IMPL_RELEASE_INHERITED(MobileMessageCursor, DOMCursor)

MobileMessageCursor::MobileMessageCursor(nsPIDOMWindow* aWindow,
                                         nsICursorContinueCallback* aCallback)
  : DOMCursor(aWindow, aCallback)
{
}

NS_IMETHODIMP
MobileMessageCursor::Continue()
{
  // We have originally:
  //
  //   DOMCursor::Continue()
  //   +-> DOMCursor::Continue(ErrorResult& aRv)
  //
  // Now it becomes:
  //
  //   MobileMessageCursor::Continue()
  //   +-> DOMCursor::Continue()
  //       +-> MobileMessageCursor::Continue(ErrorResult& aRv)
  //           o-> DOMCursor::Continue(ErrorResult& aRv)
  return DOMCursor::Continue();
}

void
MobileMessageCursor::Continue(ErrorResult& aRv)
{
  // An ordinary DOMCursor works in following flow:
  //
  //   DOMCursor::Continue()
  //   +-> DOMCursor::Reset()
  //   +-> nsICursorContinueCallback::HandleContinue()
  //       +-> nsIMobileMessageCursorCallback::NotifyCursorResult()
  //           +-> DOMCursor::FireSuccess()
  //
  // With no pending result, we call to |DOMCursor::Continue()| as usual.
  if (!mPendingResults.Length()) {
    DOMCursor::Continue(aRv);
    return;
  }

  // Otherwise, reset current result and fire a success event with the last
  // pending one.
  Reset();

  nsresult rv = FireSuccessWithNextPendingResult();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

nsresult
MobileMessageCursor::FireSuccessWithNextPendingResult()
{
  // We're going to pop the last element from mPendingResults, so it must not
  // be empty.
  MOZ_ASSERT(mPendingResults.Length());

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwner()))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> val(cx);
  nsresult rv =
    nsContentUtils::WrapNative(cx, mPendingResults.LastElement(), &val);
  NS_ENSURE_SUCCESS(rv, rv);

  mPendingResults.RemoveElementAt(mPendingResults.Length() - 1);

  FireSuccess(val);
  return NS_OK;
}

namespace mobilemessage {

NS_IMPL_CYCLE_COLLECTION(MobileMessageCursorCallback, mDOMCursor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileMessageCursorCallback)
  NS_INTERFACE_MAP_ENTRY(nsIMobileMessageCursorCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileMessageCursorCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileMessageCursorCallback)

// nsIMobileMessageCursorCallback

NS_IMETHODIMP
MobileMessageCursorCallback::NotifyCursorError(int32_t aError)
{
  MOZ_ASSERT(mDOMCursor);

  nsRefPtr<DOMCursor> cursor = mDOMCursor.forget();

  switch (aError) {
    case nsIMobileMessageCallback::NO_SIGNAL_ERROR:
      cursor->FireError(NS_LITERAL_STRING("NoSignalError"));
      break;
    case nsIMobileMessageCallback::NOT_FOUND_ERROR:
      cursor->FireError(NS_LITERAL_STRING("NotFoundError"));
      break;
    case nsIMobileMessageCallback::UNKNOWN_ERROR:
      cursor->FireError(NS_LITERAL_STRING("UnknownError"));
      break;
    case nsIMobileMessageCallback::INTERNAL_ERROR:
      cursor->FireError(NS_LITERAL_STRING("InternalError"));
      break;
    default: // SUCCESS_NO_ERROR is handled above.
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageCursorCallback::NotifyCursorResult(nsISupports** aResults,
                                                uint32_t aSize)
{
  MOZ_ASSERT(mDOMCursor);
  // We should only be notified with valid results. Or, either
  // |NotifyCursorDone()| or |NotifyCursorError()| should be called instead.
  MOZ_ASSERT(aResults && *aResults && aSize);
  // There shouldn't be unexpected notifications before |Continue()| is called.
  nsTArray<nsCOMPtr<nsISupports>>& pending = mDOMCursor->mPendingResults;
  MOZ_ASSERT(pending.Length() == 0);

  // Push pending results in reversed order.
  pending.SetCapacity(pending.Length() + aSize);
  while (aSize) {
    --aSize;
    pending.AppendElement(aResults[aSize]);
  }

  nsresult rv = mDOMCursor->FireSuccessWithNextPendingResult();
  if (NS_FAILED(rv)) {
    NotifyCursorError(nsIMobileMessageCallback::INTERNAL_ERROR);
  }

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageCursorCallback::NotifyCursorDone()
{
  MOZ_ASSERT(mDOMCursor);

  nsRefPtr<DOMCursor> cursor = mDOMCursor.forget();
  cursor->FireDone();

  return NS_OK;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
