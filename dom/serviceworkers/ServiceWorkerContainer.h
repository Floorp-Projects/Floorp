/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkercontainer_h__
#define mozilla_dom_serviceworkercontainer_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ServiceWorkerUtils.h"

class nsIGlobalWindow;

namespace mozilla {
namespace dom {

class Promise;
struct RegistrationOptions;
class ServiceWorker;

// Lightweight serviceWorker APIs collection.
class ServiceWorkerContainer final : public DOMEventTargetHelper
{
public:
  class Inner
  {
  public:
    virtual void
    AddContainer(ServiceWorkerContainer* aOuter) = 0;

    virtual void
    RemoveContainer(ServiceWorkerContainer* aOuter) = 0;

    virtual void
    Register(const ClientInfo& aClientInfo,
             const nsACString& aScopeURL,
             const nsACString& aScriptURL,
             ServiceWorkerUpdateViaCache aUpdateViaCache,
             ServiceWorkerRegistrationCallback&& aSuccessCB,
             ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    virtual void
    GetRegistration(const ClientInfo& aClientInfo,
                    const nsACString& aURL,
                    ServiceWorkerRegistrationCallback&& aSuccessCB,
                    ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    virtual void
    GetRegistrations(const ClientInfo& aClientInfo,
                     ServiceWorkerRegistrationListCallback&& aSuccessCB,
                     ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    virtual void
    GetReady(const ClientInfo& aClientInfo,
             ServiceWorkerRegistrationCallback&& aSuccessCB,
             ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(controllerchange)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(message)

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<ServiceWorkerContainer>
  Create(nsIGlobalObject* aGlobal);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  Register(const nsAString& aScriptURL,
           const RegistrationOptions& aOptions,
           ErrorResult& aRv);

  already_AddRefed<ServiceWorker>
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
  ServiceWorkerContainer(nsIGlobalObject* aGlobal,
                         already_AddRefed<ServiceWorkerContainer::Inner> aInner);

  ~ServiceWorkerContainer();

  // Utility method to get the global if its present and if certain
  // additional validaty checks pass.  One of these additional checks
  // verifies the global can access storage.  Since storage access can
  // vary based on user settings we want to often provide some error
  // message if the storage check fails.  This method takes an optional
  // callback that can be used to report the storage failure to the
  // devtools console.
  nsIGlobalObject*
  GetGlobalIfValid(ErrorResult& aRv,
                   const std::function<void(nsIDocument*)>&& aStorageFailureCB = nullptr) const;

  RefPtr<Inner> mInner;

  // This only changes when a worker hijacks everything in its scope by calling
  // claim.
  RefPtr<ServiceWorker> mControllerWorker;

  RefPtr<Promise> mReadyPromise;
  MozPromiseRequestHolder<ServiceWorkerRegistrationPromise> mReadyPromiseHolder;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_serviceworkercontainer_h__ */
