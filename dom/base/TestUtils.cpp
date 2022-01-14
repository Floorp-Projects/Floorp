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

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestUtils, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TestUtils)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestUtils)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestUtils)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* TestUtils::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return TestUtils_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> TestUtils::Gc(ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mOwner, aRv);

  // TODO(krosylight): Ideally we could use nsJSContext::IncrementalGC to make
  // it actually async, but that's not required right now.

  NS_DispatchToCurrentThread(
      NS_NewCancelableRunnableFunction("TestUtils::Gc", [promise] {
        if (NS_IsMainThread()) {
          nsJSContext::GarbageCollectNow(JS::GCReason::DOM_TESTUTILS,
                                         nsJSContext::NonIncrementalGC,
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
