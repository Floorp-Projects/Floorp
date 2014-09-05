/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkercontainer_h__
#define mozilla_dom_serviceworkercontainer_h__

#include "mozilla/DOMEventTargetHelper.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Promise;
struct RegistrationOptionList;

namespace workers {
class ServiceWorker;
}

// Lightweight serviceWorker APIs collection.
class ServiceWorkerContainer MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(controllerchange)
  IMPL_EVENT_HANDLER(reloadpage)
  IMPL_EVENT_HANDLER(error)

  explicit ServiceWorkerContainer(nsPIDOMWindow* aWindow);

  JSObject*
  WrapObject(JSContext* aCx);

  already_AddRefed<Promise>
  Register(const nsAString& aScriptURL,
           const RegistrationOptionList& aOptions,
           ErrorResult& aRv);

  already_AddRefed<workers::ServiceWorker>
  GetController();

  already_AddRefed<Promise>
  GetRegistration(const nsAString& aDocumentURL,
                  ErrorResult& aRv);

  already_AddRefed<Promise>
  GetRegistrations(ErrorResult& aRv);

  Promise*
  GetReady(ErrorResult& aRv);

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

  // DOMEventTargetHelper
  void DisconnectFromOwner() MOZ_OVERRIDE;

private:
  ~ServiceWorkerContainer();

  void RemoveReadyPromise();

  // This only changes when a worker hijacks everything in its scope by calling
  // replace().
  // FIXME(nsm): Bug 982711. Provide API to let SWM invalidate this.
  nsRefPtr<workers::ServiceWorker> mControllerWorker;

  nsRefPtr<Promise> mReadyPromise;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_workers_serviceworkercontainer_h__ */
