/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkercontainer_h__
#define mozilla_dom_serviceworkercontainer_h__

#include "mozilla/DOMEventTargetHelper.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Promise;
struct RegistrationOptions;

namespace workers {
class ServiceWorker;
} // namespace workers

// Lightweight serviceWorker APIs collection.
class ServiceWorkerContainer final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(controllerchange)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(message)

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  explicit ServiceWorkerContainer(nsPIDOMWindowInner* aWindow);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  Register(const nsAString& aScriptURL,
           const RegistrationOptions& aOptions,
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
  void
  GetScopeForUrl(const nsAString& aUrl, nsString& aScope, ErrorResult& aRv);

  // DOMEventTargetHelper
  void DisconnectFromOwner() override;

  // Invalidates |mControllerWorker| and dispatches a "controllerchange"
  // event.
  void
  ControllerChanged(ErrorResult& aRv);

private:
  ~ServiceWorkerContainer();

  void RemoveReadyPromise();

  // This only changes when a worker hijacks everything in its scope by calling
  // claim.
  RefPtr<workers::ServiceWorker> mControllerWorker;

  RefPtr<Promise> mReadyPromise;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_workers_serviceworkercontainer_h__ */
