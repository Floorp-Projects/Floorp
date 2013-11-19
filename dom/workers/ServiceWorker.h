/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworker_h__
#define mozilla_dom_workers_serviceworker_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ServiceWorkerBinding.h" // For ServiceWorkerState.

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Promise;

namespace workers {

class SharedWorker;

class ServiceWorker MOZ_FINAL : public DOMEventTargetHelper
{
  friend class RuntimeService;
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorker, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(error)

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  ServiceWorkerState
  State() const
  {
    return mState;
  }

  void
  GetScope(nsString& aScope) const
  {
    aScope = mScope;
  }

  void
  GetUrl(nsString& aURL) const
  {
    aURL = mURL;
  }

  WorkerPrivate*
  GetWorkerPrivate() const;

private:
  // This class can only be created from the RuntimeService.
  ServiceWorker(nsPIDOMWindow* aWindow, SharedWorker* aSharedWorker);

  // This class is reference-counted and will be destroyed from Release().
  ~ServiceWorker();

  ServiceWorkerState mState;
  nsString mScope;
  nsString mURL;

  // To allow ServiceWorkers to potentially drop the backing DOMEventTargetHelper and
  // re-instantiate it later, they simply own a SharedWorker member that
  // can be released and recreated as required rather than re-implement some of
  // the SharedWorker logic.
  nsRefPtr<SharedWorker> mSharedWorker;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworker_h__
