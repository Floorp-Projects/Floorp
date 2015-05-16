/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MediaUtils_h
#define mozilla_MediaUtils_h

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace media {

// A media::Pledge is a promise-like pattern for c++ that takes lambda functions.
//
// Asynchronous APIs that proxy to the chrome process and back, may return a
// pledge to allow callers to use pledge.Then() to specify a lambda function to
// invoke with the result once the asynchronous API resolves later back on the
// same thread.
//
// An advantage of this pattern is that lambdas allow "capturing" of local
// variables, much like closures in JavaScript.

template<typename ValueType>
class Pledge
{
  // TODO: Remove workaround once mozilla allows std::function from <functional>
  class FunctorsBase
  {
  public:
    FunctorsBase() {}
    virtual void Succeed(const ValueType& result) = 0;
    virtual void Fail(nsresult rv) = 0;
    virtual ~FunctorsBase() {};
  };

public:
  explicit Pledge() : mDone(false), mResult(NS_OK) {}

  template<typename OnSuccessType>
  void Then(OnSuccessType aOnSuccess)
  {
    Then(aOnSuccess, [](nsresult){});
  }

  template<typename OnSuccessType, typename OnFailureType>
  void Then(OnSuccessType aOnSuccess, OnFailureType aOnFailure)
  {
    class F : public FunctorsBase
    {
    public:
      F(OnSuccessType& aOnSuccess, OnFailureType& aOnFailure)
        : mOnSuccess(aOnSuccess), mOnFailure(aOnFailure) {}

      void Succeed(const ValueType& result)
      {
        mOnSuccess(result);
      }
      void Fail(nsresult rv)
      {
        mOnFailure(rv);
      };

      OnSuccessType mOnSuccess;
      OnFailureType mOnFailure;
    };
    mFunctors = new F(aOnSuccess, aOnFailure);

    if (mDone) {
      if (mResult == NS_OK) {
        mFunctors->Succeed(mValue);
      } else {
        mFunctors->Fail(mResult);
      }
    }
  }

protected:
  void Resolve(const ValueType& aValue)
  {
    mValue = aValue;
    Resolve();
  }

  void Resolve()
  {
    if (!mDone) {
      mDone = true;
      MOZ_ASSERT(mResult == NS_OK);
      if (mFunctors) {
        mFunctors->Succeed(mValue);
      }
    }
  }

  void Reject(nsresult rv)
  {
    if (!mDone) {
      mDone = true;
      mResult = rv;
      if (mFunctors) {
        mFunctors->Fail(mResult);
      }
    }
  }

  ValueType mValue;
protected:
  bool mDone;
  nsresult mResult;
private:
  nsAutoPtr<FunctorsBase> mFunctors;
};

// General purpose runnable that also acts as a Pledge for the resulting value.
// Use PledgeRunnable<>::New() factory function to use with lambdas.

template<typename ValueType>
class PledgeRunnable : public Pledge<ValueType>, public nsRunnable
{
public:
  template<typename OnRunType>
  static PledgeRunnable<ValueType>*
  New(OnRunType aOnRun)
  {
    class P : public PledgeRunnable<ValueType>
    {
    public:
      explicit P(OnRunType& aOnRun)
      : mOriginThread(NS_GetCurrentThread())
      , mOnRun(aOnRun)
      , mHasRun(false) {}
    private:
      virtual ~P() {}
      NS_IMETHODIMP
      Run()
      {
        if (!mHasRun) {
          P::mResult = mOnRun(P::mValue);
          mHasRun = true;
          return mOriginThread->Dispatch(this, NS_DISPATCH_NORMAL);
        }
        bool on;
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(mOriginThread->IsOnCurrentThread(&on)));
        MOZ_RELEASE_ASSERT(on);

        if (NS_SUCCEEDED(P::mResult)) {
          P::Resolve();
        } else {
          P::Reject(P::mResult);
        }
        return NS_OK;
      }
      nsCOMPtr<nsIThread> mOriginThread;
      OnRunType mOnRun;
      bool mHasRun;
    };

    return new P(aOnRun);
  }

protected:
  virtual ~PledgeRunnable() {}
};

// General purpose runnable with an eye toward lambdas

namespace CallbackRunnable
{
template<typename OnRunType>
class Impl : public nsRunnable
{
public:
  explicit Impl(OnRunType& aOnRun) : mOnRun(aOnRun) {}
private:
  NS_IMETHODIMP
  Run()
  {
    return mOnRun();
  }
  OnRunType mOnRun;
};

template<typename OnRunType>
Impl<OnRunType>*
New(OnRunType aOnRun)
{
  return new Impl<OnRunType>(aOnRun);
}
}

}
}

#endif // mozilla_MediaUtils_h
