/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/ThreadUtils.h"

#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsIThreadInternal.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::quota {

namespace {

class RunAfterProcessingCurrentEventHelper final : public nsIThreadObserver {
 public:
  nsresult Init(std::function<void()>&& aCallback);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADOBSERVER

 private:
  ~RunAfterProcessingCurrentEventHelper() = default;

  std::function<void()> mCallback;
};

}  // namespace

nsresult RunAfterProcessingCurrentEventHelper::Init(
    std::function<void()>&& aCallback) {
  nsCOMPtr<nsIThreadInternal> thread = do_QueryInterface(NS_GetCurrentThread());
  MOZ_ASSERT(thread);

  QM_TRY(MOZ_TO_RESULT(thread->AddObserver(this)));

  mCallback = std::move(aCallback);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(RunAfterProcessingCurrentEventHelper, nsIThreadObserver)

NS_IMETHODIMP
RunAfterProcessingCurrentEventHelper::OnDispatchedEvent() {
  MOZ_CRASH("Should never be called!");
}

NS_IMETHODIMP
RunAfterProcessingCurrentEventHelper::OnProcessNextEvent(
    nsIThreadInternal* /* aThread */, bool /* aMayWait */) {
  MOZ_CRASH("Should never be called!");
}

NS_IMETHODIMP
RunAfterProcessingCurrentEventHelper::AfterProcessNextEvent(
    nsIThreadInternal* aThread, bool /* aEventWasProcessed */) {
  MOZ_ASSERT(aThread);

  QM_WARNONLY_TRY(MOZ_TO_RESULT(aThread->RemoveObserver(this)));

  auto callback = std::move(mCallback);
  callback();

  return NS_OK;
}

nsresult RunAfterProcessingCurrentEvent(std::function<void()>&& aCallback) {
  MOZ_DIAGNOSTIC_ASSERT(
      !nsThreadPool::GetCurrentThreadPool(),
      "Call to RunAfterProcessingCurrentEvent() from thread pool!");

  auto helper = MakeRefPtr<RunAfterProcessingCurrentEventHelper>();

  QM_TRY(MOZ_TO_RESULT(helper->Init(std::move(aCallback))));

  return NS_OK;
}

}  // namespace mozilla::dom::quota
