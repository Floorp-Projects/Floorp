/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fs/FileSystemAsyncCopy.h"

#include "fs/FileSystemConstants.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::fs {

nsresult AsyncCopy(nsIInputStream* aSource, nsIOutputStream* aSink,
                   nsISerialEventTarget* aIOTarget, const nsAsyncCopyMode aMode,
                   const bool aCloseSource, const bool aCloseSink,
                   std::function<void(uint32_t)>&& aProgressCallback,
                   MoveOnlyFunction<void(nsresult)>&& aCompleteCallback) {
  struct CallbackClosure {
    CallbackClosure(std::function<void(uint32_t)>&& aProgressCallback,
                    MoveOnlyFunction<void(nsresult)>&& aCompleteCallback) {
      mProgressCallbackWrapper = MakeUnique<std::function<void(uint32_t)>>(
          [progressCallback = std::move(aProgressCallback)](uint32_t count) {
            progressCallback(count);
          });

      mCompleteCallbackWrapper = MakeUnique<MoveOnlyFunction<void(nsresult)>>(
          [completeCallback =
               std::move(aCompleteCallback)](nsresult rv) mutable {
            auto callback = std::move(completeCallback);
            callback(rv);
          });
    }

    UniquePtr<std::function<void(uint32_t)>> mProgressCallbackWrapper;
    UniquePtr<MoveOnlyFunction<void(nsresult)>> mCompleteCallbackWrapper;
  };

  auto* callbackClosure = new CallbackClosure(std::move(aProgressCallback),
                                              std::move(aCompleteCallback));

  QM_TRY(
      MOZ_TO_RESULT(NS_AsyncCopy(
          aSource, aSink, aIOTarget, aMode, kStreamCopyBlockSize,
          [](void* aClosure, nsresult aRv) {
            auto* callbackClosure = static_cast<CallbackClosure*>(aClosure);
            (*callbackClosure->mCompleteCallbackWrapper)(aRv);
            delete callbackClosure;
          },
          callbackClosure, aCloseSource, aCloseSink, /* aCopierCtx */ nullptr,
          [](void* aClosure, uint32_t aCount) {
            auto* callbackClosure = static_cast<CallbackClosure*>(aClosure);
            (*callbackClosure->mProgressCallbackWrapper)(aCount);
          })),
      [callbackClosure](nsresult rv) {
        delete callbackClosure;
        return rv;
      });

  return NS_OK;
}

}  // namespace mozilla::dom::fs
