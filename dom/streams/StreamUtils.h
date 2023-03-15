/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StreamUtils_h
#define mozilla_dom_StreamUtils_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"

class nsIGlobalObject;

namespace mozilla::dom {

struct QueuingStrategy;

double ExtractHighWaterMark(const QueuingStrategy& aStrategy,
                            double aDefaultHWM, ErrorResult& aRv);

// Promisification algorithm, shared among:
// Step 2 and 3 of https://streams.spec.whatwg.org/#readablestream-set-up
// Step 2 and 3 of
// https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support
// Step 2 and 3 of https://streams.spec.whatwg.org/#writablestream-set-up
// Step 5 and 6 of https://streams.spec.whatwg.org/#transformstream-set-up
template <typename T>
MOZ_CAN_RUN_SCRIPT static already_AddRefed<Promise> PromisifyAlgorithm(
    nsIGlobalObject* aGlobal, T aFunc, mozilla::ErrorResult& aRv) {
  // Step 1. Let result be the result of running (algorithm). If this throws an
  // exception e, return a promise rejected with e.
  RefPtr<Promise> result;
  if constexpr (!std::is_same<decltype(aFunc(aRv)), void>::value) {
    result = aFunc(aRv);
  } else {
    aFunc(aRv);
  }

  if (aRv.IsUncatchableException()) {
    return nullptr;
  }

  if (aRv.Failed()) {
    return Promise::CreateRejectedWithErrorResult(aGlobal, aRv);
  }

  // Step 2. If result is a Promise, then return result.
  if (result) {
    return result.forget();
  }

  // Step 3. Return a promise resolved with undefined.
  return Promise::CreateResolvedWithUndefined(aGlobal, aRv);
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_StreamUtils_h
