/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_RESOLVABLENORMALORIGINOP_H_
#define DOM_QUOTA_RESOLVABLENORMALORIGINOP_H_

#include "NormalOriginOperationBase.h"

#include "mozilla/MozPromise.h"

namespace mozilla::dom::quota {

template <typename T>
class ResolvableNormalOriginOp : public NormalOriginOperationBase {
 public:
  using PromiseType = MozPromise<T, nsresult, false>;

  RefPtr<PromiseType> OnResults() {
    AssertIsOnOwningThread();

    return mPromiseHolder.Ensure(__func__);
  }

 protected:
  ResolvableNormalOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                           const char* aName)
      : NormalOriginOperationBase(std::move(aQuotaManager), aName) {
    AssertIsOnOwningThread();
  }

  virtual ~ResolvableNormalOriginOp() = default;

  virtual T GetResolveValue() = 0;

 private:
  void SendResults() override {
#ifdef DEBUG
    NoteActorDestroyed();
#endif

    if (NS_SUCCEEDED(mResultCode)) {
      mPromiseHolder.ResolveIfExists(GetResolveValue(), __func__);
    } else {
      mPromiseHolder.RejectIfExists(mResultCode, __func__);
    }
  }

  MozPromiseHolder<PromiseType> mPromiseHolder;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_RESOLVABLENORMALORIGINOP_H_
