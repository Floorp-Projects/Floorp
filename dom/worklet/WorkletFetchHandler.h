/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkletFetchHandler_h
#define mozilla_dom_WorkletFetchHandler_h

#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/RequestBinding.h"  // RequestCredentials
#include "nsIStreamLoader.h"

namespace mozilla::dom {
class Worklet;
struct WorkletOptions;
class WorkletScriptHandler;

namespace loader {
class AddModuleThrowErrorRunnable;
}  // namespace loader

// WorkletFetchHandler is used to fetch the module scripts on the main thread,
// and notifies the result of addModule back to |aWorklet|.
class WorkletFetchHandler final : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WorkletFetchHandler)

  static already_AddRefed<Promise> AddModule(Worklet* aWorklet, JSContext* aCx,
                                             const nsAString& aModuleURL,
                                             const WorkletOptions& aOptions,
                                             ErrorResult& aRv);

  // Load a module script on main thread.
  nsresult StartFetch(JSContext* aCx, nsIURI* aURI, nsIURI* aReferrer);

  void ExecutionFailed();
  void ExecutionFailed(JS::Handle<JS::Value> aError);

  void ExecutionSucceeded();

  void HandleFetchFailed(nsIURI* aURI);

 private:
  WorkletFetchHandler(Worklet* aWorklet, Promise* aPromise,
                      RequestCredentials aCredentials);

  ~WorkletFetchHandler();

  void AddPromise(JSContext* aCx, Promise* aPromise);

  void RejectPromises(nsresult aResult);
  void RejectPromises(JS::Handle<JS::Value> aValue);

  void ResolvePromises();

  friend class StartFetchRunnable;
  friend class loader::AddModuleThrowErrorRunnable;
  RefPtr<Worklet> mWorklet;
  nsTArray<RefPtr<Promise>> mPromises;

  enum { ePending, eRejected, eResolved } mStatus;

  RequestCredentials mCredentials;

  bool mHasError = false;
  JS::Heap<JS::Value> mErrorToRethrow;
};

// A Runnable to call WorkletFetchHandler::StartFetch on the main thread.
class StartFetchRunnable final : public Runnable {
 public:
  StartFetchRunnable(
      const nsMainThreadPtrHandle<WorkletFetchHandler>& aHandlerRef,
      nsIURI* aURI, nsIURI* aReferrer);
  ~StartFetchRunnable() = default;

  NS_IMETHOD
  Run() override;

 private:
  nsMainThreadPtrHandle<WorkletFetchHandler> mHandlerRef;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mReferrer;
};

// WorkletScriptHandler is used to handle the result of fetching the module
// script.
class WorkletScriptHandler final : public PromiseNativeHandler,
                                   public nsIStreamLoaderObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  WorkletScriptHandler(Worklet* aWorklet, nsIURI* aURI);

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

  void DispatchFetchCompleteToWorklet(nsresult aRv);

  RefPtr<Worklet> mWorklet;
  nsCOMPtr<nsIURI> mURI;
};

}  // namespace mozilla::dom
#endif  // mozilla_dom_WorkletFetchHandler_h
