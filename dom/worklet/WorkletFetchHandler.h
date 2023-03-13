/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkletFetchHandler_h
#define mozilla_dom_WorkletFetchHandler_h

#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsIStreamLoader.h"

namespace mozilla::dom {
class Worklet;
struct WorkletOptions;
class WorkletScriptHandler;

// WorkletFetchHandler is used to fetch the module scripts on the main thread,
// and notifies the result of addModule back to |aWorklet|.
class WorkletFetchHandler final : public nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static already_AddRefed<Promise> Fetch(Worklet* aWorklet, JSContext* aCx,
                                         const nsAString& aModuleURL,
                                         const WorkletOptions& aOptions,
                                         ErrorResult& aRv);

  const nsCString& URL() const { return mURL; }

  void ExecutionFailed(nsresult aRv);

  void ExecutionSucceeded();

 private:
  WorkletFetchHandler(Worklet* aWorklet, const nsACString& aURL,
                      Promise* aPromise);

  ~WorkletFetchHandler() = default;

  void AddPromise(Promise* aPromise);

  void RejectPromises(nsresult aResult);

  void ResolvePromises();

  RefPtr<Worklet> mWorklet;
  nsTArray<RefPtr<Promise>> mPromises;

  enum { ePending, eRejected, eResolved } mStatus;

  nsresult mErrorStatus;

  nsCString mURL;
};

// WorkletScriptHandler is used to handle the result of fetching the module
// script.
class WorkletScriptHandler final : public PromiseNativeHandler,
                                   public nsIStreamLoaderObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WorkletScriptHandler(Worklet* aWorklet);

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;

  void HandleFailure(nsresult aResult);

 private:
  ~WorkletScriptHandler() = default;

  RefPtr<Worklet> mWorklet;
};

}  // namespace mozilla::dom
#endif  // mozilla_dom_WorkletFetchHandler_h
