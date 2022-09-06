/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkercontainer_h__
#define mozilla_dom_serviceworkercontainer_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/ServiceWorkerUtils.h"

class nsIGlobalWindow;

namespace mozilla::dom {

class ClientPostMessageArgs;
struct MessageEventInit;
class Promise;
struct RegistrationOptions;
class ServiceWorker;

// Lightweight serviceWorker APIs collection.
class ServiceWorkerContainer final : public DOMEventTargetHelper {
 public:
  class Inner {
   public:
    virtual void AddContainer(ServiceWorkerContainer* aOuter) = 0;

    virtual void RemoveContainer(ServiceWorkerContainer* aOuter) = 0;

    virtual void Register(const ClientInfo& aClientInfo,
                          const nsACString& aScopeURL,
                          const nsACString& aScriptURL,
                          ServiceWorkerUpdateViaCache aUpdateViaCache,
                          ServiceWorkerRegistrationCallback&& aSuccessCB,
                          ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    virtual void GetRegistration(
        const ClientInfo& aClientInfo, const nsACString& aURL,
        ServiceWorkerRegistrationCallback&& aSuccessCB,
        ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    virtual void GetRegistrations(
        const ClientInfo& aClientInfo,
        ServiceWorkerRegistrationListCallback&& aSuccessCB,
        ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    virtual void GetReady(const ClientInfo& aClientInfo,
                          ServiceWorkerRegistrationCallback&& aSuccessCB,
                          ServiceWorkerFailureCallback&& aFailureCB) const = 0;

    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerContainer,
                                           DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(controllerchange)
  IMPL_EVENT_HANDLER(messageerror)

  // Almost a manual expansion of IMPL_EVENT_HANDLER(message), but
  // with the additional StartMessages() when setting the handler, as
  // required by the spec.
  inline mozilla::dom::EventHandlerNonNull* GetOnmessage() {
    return GetEventHandler(nsGkAtoms::onmessage);
  }
  inline void SetOnmessage(mozilla::dom::EventHandlerNonNull* aCallback) {
    SetEventHandler(nsGkAtoms::onmessage, aCallback);
    StartMessages();
  }

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<ServiceWorkerContainer> Create(
      nsIGlobalObject* aGlobal);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Register(const nsAString& aScriptURL,
                                     const RegistrationOptions& aOptions,
                                     const CallerType aCallerType,
                                     ErrorResult& aRv);

  already_AddRefed<ServiceWorker> GetController();

  already_AddRefed<Promise> GetRegistration(const nsAString& aDocumentURL,
                                            ErrorResult& aRv);

  already_AddRefed<Promise> GetRegistrations(ErrorResult& aRv);

  void StartMessages();

  Promise* GetReady(ErrorResult& aRv);

  // Testing only.
  void GetScopeForUrl(const nsAString& aUrl, nsString& aScope,
                      ErrorResult& aRv);

  // DOMEventTargetHelper
  void DisconnectFromOwner() override;

  // Invalidates |mControllerWorker| and dispatches a "controllerchange"
  // event.
  void ControllerChanged(ErrorResult& aRv);

  void ReceiveMessage(const ClientPostMessageArgs& aArgs);

 private:
  ServiceWorkerContainer(
      nsIGlobalObject* aGlobal,
      already_AddRefed<ServiceWorkerContainer::Inner> aInner);

  ~ServiceWorkerContainer();

  // Utility method to get the global if its present and if certain
  // additional validaty checks pass.  One of these additional checks
  // verifies the global can access storage.  Since storage access can
  // vary based on user settings we want to often provide some error
  // message if the storage check fails.  This method takes an optional
  // callback that can be used to report the storage failure to the
  // devtools console.
  nsIGlobalObject* GetGlobalIfValid(
      ErrorResult& aRv,
      const std::function<void(Document*)>&& aStorageFailureCB = nullptr) const;

  struct ReceivedMessage;

  // Dispatch a Runnable that dispatches the given message on this
  // object. When the owner of this object is a Window, the Runnable
  // is dispatched on the corresponding TabGroup.
  void EnqueueReceivedMessageDispatch(RefPtr<ReceivedMessage> aMessage);

  template <typename F>
  void RunWithJSContext(F&& aCallable);

  void DispatchMessage(RefPtr<ReceivedMessage> aMessage);

  // When it fails, returning boolean means whether it's because deserailization
  // failed or not.
  static Result<Ok, bool> FillInMessageEventInit(JSContext* aCx,
                                                 nsIGlobalObject* aGlobal,
                                                 ReceivedMessage& aMessage,
                                                 MessageEventInit& aInit,
                                                 ErrorResult& aRv);

  RefPtr<Inner> mInner;

  // This only changes when a worker hijacks everything in its scope by calling
  // claim.
  RefPtr<ServiceWorker> mControllerWorker;

  RefPtr<Promise> mReadyPromise;
  MozPromiseRequestHolder<ServiceWorkerRegistrationPromise> mReadyPromiseHolder;

  // Set after StartMessages() has been called.
  bool mMessagesStarted = false;

  // Queue holding messages posted from service worker as long as
  // StartMessages() hasn't been called.
  nsTArray<RefPtr<ReceivedMessage>> mPendingMessages;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_serviceworkercontainer_h__ */
