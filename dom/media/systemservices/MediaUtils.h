/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MediaUtils_h
#define mozilla_MediaUtils_h

#include "nsAutoPtr.h"

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
  explicit Pledge();

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
private:
  bool mDone;
  nsresult mResult;
  nsAutoPtr<FunctorsBase> mFunctors;
};

}
}

#endif // mozilla_MediaUtils_h
