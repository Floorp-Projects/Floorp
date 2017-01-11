/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnAsync_h
#define mozilla_dom_WebAuthnAsync_h

#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gWebauthLog; // defined in U2F.cpp

// WebAuthnRequest tracks the completion of a single WebAuthn request that
// may run on multiple kinds of authenticators, and be subject to a deadline.
template<class Success>
class WebAuthnRequest {
public:
  WebAuthnRequest()
    : mCancelled(false)
    , mSuccess(false)
    , mCountTokens(0)
    , mTokensFailed(0)
    , mReentrantMonitor("WebAuthnRequest")
  {}

  void AddActiveToken(const char* aCallSite)
  {
    MOZ_LOG(gWebauthLog, LogLevel::Debug,
           ("WebAuthnRequest is tracking a new token, called from [%s]",
            aCallSite));
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    MOZ_ASSERT(!IsComplete());
    mCountTokens += 1;
  }

  bool IsComplete()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mCancelled || mSuccess ||
      (mCountTokens > 0 && mTokensFailed == mCountTokens);
  }

  void CancelNow()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // It's possible for a race to cause CancelNow to get called after
    // a success or a cancel. We only complete once.
    if (IsComplete()) {
      return;
    }

    mCancelled = true;
    mPromise.Reject(NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
  }

  void SetFailure(nsresult aError)
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // It's possible for a race to cause SetFailure to get called after
    // a success or a cancel. We only complete once.
    if (IsComplete()) {
      return;
    }

    mTokensFailed += 1;
    MOZ_ASSERT(mTokensFailed <= mCountTokens);

    if (mTokensFailed == mCountTokens) {
      // Provide the final error as being indicitive of the whole set.
      mPromise.Reject(aError, __func__);
    }
  }

  void SetSuccess(const Success& aResult)
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // It's possible for a race to cause multiple calls to SetSuccess
    // in succession. We will only select the earliest.
    if (IsComplete()) {
      return;
    }

    mSuccess = true;
    mPromise.Resolve(aResult, __func__);
  }

  void SetDeadline(TimeDuration aDeadline)
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    MOZ_ASSERT(!IsComplete());
    // TODO: Monitor the deadline and stop with a timeout error if it expires.
  }

  RefPtr<MozPromise<Success, nsresult, false>> Ensure()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    MOZ_ASSERT(!IsComplete());
    return mPromise.Ensure(__func__);
  }

  void CompleteTask()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    if (mCountTokens == 0) {
      // Special case for there being no tasks to complete
      mPromise.Reject(NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
    }
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebAuthnRequest)

private:
  ~WebAuthnRequest() {};

  bool mCancelled;
  bool mSuccess;
  int mCountTokens;
  int mTokensFailed;
  ReentrantMonitor mReentrantMonitor;
  MozPromiseHolder<MozPromise<Success, nsresult, false>> mPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthnAsync_h
