/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkercontainer_h__
#define mozilla_dom_workers_serviceworkercontainer_h__

#include "mozilla/DOMEventTargetHelper.h"

#include "ServiceWorkerManager.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Promise;
struct RegistrationOptionList;

namespace workers {

class ServiceWorker;

// Lightweight serviceWorker APIs collection.
class ServiceWorkerContainer MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(updatefound)
  IMPL_EVENT_HANDLER(controllerchange)
  IMPL_EVENT_HANDLER(reloadpage)
  IMPL_EVENT_HANDLER(error)

  explicit ServiceWorkerContainer(nsPIDOMWindow* aWindow);

  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  JSObject*
  WrapObject(JSContext* aCx);

  already_AddRefed<Promise>
  Register(const nsAString& aScriptURL,
           const RegistrationOptionList& aOptions,
           ErrorResult& aRv);

  already_AddRefed<Promise>
  Unregister(const nsAString& scope, ErrorResult& aRv);

  already_AddRefed<ServiceWorker>
  GetInstalling();

  already_AddRefed<ServiceWorker>
  GetWaiting();

  already_AddRefed<ServiceWorker>
  GetActive();

  already_AddRefed<ServiceWorker>
  GetController();

  already_AddRefed<Promise>
  GetAll(ErrorResult& aRv);

  already_AddRefed<Promise>
  GetReady(ErrorResult& aRv);

  nsIURI*
  GetDocumentURI() const
  {
    return mWindow->GetDocumentURI();
  }

  void
  InvalidateWorkerReference(WhichServiceWorker aWhichOnes);

  already_AddRefed<workers::ServiceWorker>
  GetWorkerReference(WhichServiceWorker aWhichOne);

  // Testing only.
  already_AddRefed<Promise>
  ClearAllServiceWorkerData(ErrorResult& aRv);

  // Testing only.
  void
  GetScopeForUrl(const nsAString& aUrl, nsString& aScope, ErrorResult& aRv);

  // Testing only.
  void
  GetControllingWorkerScriptURLForPath(const nsAString& aPath,
                                       nsString& aScriptURL,
                                       ErrorResult& aRv);
private:
  ~ServiceWorkerContainer();

  void
  StartListeningForEvents();

  void
  StopListeningForEvents();

  nsCOMPtr<nsPIDOMWindow> mWindow;

  // The following properties are cached here to ensure JS equality is satisfied
  // instead of acquiring a new worker instance from the ServiceWorkerManager
  // for every access. A null value is considered a cache miss.
  // These three may change to a new worker at any time.
  nsRefPtr<ServiceWorker> mInstallingWorker;
  nsRefPtr<ServiceWorker> mWaitingWorker;
  nsRefPtr<ServiceWorker> mActiveWorker;
  // This only changes when a worker hijacks everything in its scope by calling
  // replace().
  // FIXME(nsm): Bug 982711. Provide API to let SWM invalidate this.
  nsRefPtr<ServiceWorker> mControllerWorker;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_workers_serviceworkercontainer_h__ */
