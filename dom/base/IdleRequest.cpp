/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdleRequest.h"

#include "mozilla/Function.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/IdleDeadline.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsComponentManagerUtils.h"
#include "nsGlobalWindow.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

IdleRequest::IdleRequest(JSContext* aCx, nsPIDOMWindowInner* aWindow,
                         IdleRequestCallback& aCallback, uint32_t aHandle)
  : mWindow(aWindow)
  , mCallback(&aCallback)
  , mHandle(aHandle)
  , mTimeoutHandle(Nothing())
{
  MOZ_ASSERT(aWindow);

  // Get the calling location.
  nsJSUtils::GetCallingLocation(aCx, mFileName, &mLineNo, &mColumn);
}

IdleRequest::~IdleRequest()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IdleRequest)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IdleRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IdleRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IdleRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IdleRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsICancelableRunnable)
  NS_INTERFACE_MAP_ENTRY(nsIIncrementalRunnable)
  NS_INTERFACE_MAP_ENTRY(nsITimeoutHandler)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimeoutHandler)
NS_INTERFACE_MAP_END

nsresult
IdleRequest::SetTimeout(uint32_t aTimeout)
{
  int32_t handle;
  nsresult rv = nsGlobalWindow::Cast(mWindow)->SetTimeoutOrInterval(
    this, aTimeout, false, Timeout::Reason::eIdleCallbackTimeout, &handle);
  mTimeoutHandle = Some(handle);

  return rv;
}

nsresult
IdleRequest::Run()
{
  if (mCallback) {
    RunIdleRequestCallback(false);
  }

  return NS_OK;
}

nsresult
IdleRequest::Cancel()
{
  mCallback = nullptr;
  CancelTimeout();
  if (isInList()) {
    remove();
  }
  Release();

  return NS_OK;
}

void
IdleRequest::SetDeadline(TimeStamp aDeadline)
{
  mozilla::dom::Performance* perf = mWindow->GetPerformance();
  mDeadline =
    perf ? perf->GetDOMTiming()->TimeStampToDOMHighRes(aDeadline) : 0.0;
}

nsresult
IdleRequest::RunIdleRequestCallback(bool aDidTimeout)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aDidTimeout) {
    CancelTimeout();
  }

  remove();
  ErrorResult error;
  RefPtr<IdleDeadline> deadline =
    new IdleDeadline(mWindow, aDidTimeout, mDeadline);
  mCallback->Call(*deadline, error, "requestIdleCallback handler");
  mCallback = nullptr;
  Release();

  return error.StealNSResult();
}

void
IdleRequest::CancelTimeout()
{
  if (mTimeoutHandle.isSome()) {
    nsGlobalWindow::Cast(mWindow)->ClearTimeoutOrInterval(
      mTimeoutHandle.value(), Timeout::Reason::eIdleCallbackTimeout);
  }
}

nsresult
IdleRequest::Call()
{
  SetDeadline(TimeStamp::Now());
  return RunIdleRequestCallback(true);
}

void
IdleRequest::GetLocation(const char** aFileName, uint32_t* aLineNo,
                         uint32_t* aColumn)
{
  *aFileName = mFileName.get();
  *aLineNo = mLineNo;
  *aColumn = mColumn;
}

void
IdleRequest::MarkForCC()
{
  mCallback->MarkForCC();
}

} // namespace dom
} // namespace mozilla
