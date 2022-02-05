/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestUtils.h"
#include "mozilla/dom/TestUtilsBinding.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsJSEnvironment.h"
#include "xpcpublic.h"

namespace mozilla::dom {

already_AddRefed<Promise> TestUtils::Gc(const GlobalObject& aGlobal,
                                        ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // TODO(krosylight): Ideally we could use nsJSContext::IncrementalGC to make
  // it actually async, but that's not required right now.

  NS_DispatchToCurrentThread(
      NS_NewCancelableRunnableFunction("TestUtils::Gc", [promise] {
        if (NS_IsMainThread()) {
          nsJSContext::GarbageCollectNow(JS::GCReason::DOM_TESTUTILS,
                                         nsJSContext::NonShrinkingGC);
          nsJSContext::CycleCollectNow(CCReason::API);
        } else {
          WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
          workerPrivate->GarbageCollectInternal(workerPrivate->GetJSContext(),
                                                false /* shrinking */,
                                                false /* collect children */);
          workerPrivate->CycleCollectInternal(false);
        }

        promise->MaybeResolveWithUndefined();
      }));

  return promise.forget();
}

}  // namespace mozilla::dom
