/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworker_h__
#define mozilla_dom_serviceworker_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ServiceWorkerUtils.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

class nsIGlobalObject;

namespace mozilla::dom {

class ServiceWorkerChild;
class ServiceWorkerCloneData;
struct StructuredSerializeOptions;

#define NS_DOM_SERVICEWORKER_IID                     \
  {                                                  \
    0xd42e0611, 0x3647, 0x4319, {                    \
      0xae, 0x05, 0x19, 0x89, 0x59, 0xba, 0x99, 0x5e \
    }                                                \
  }

class ServiceWorker final : public DOMEventTargetHelper {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_SERVICEWORKER_IID)
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorker, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(error)

  static already_AddRefed<ServiceWorker> Create(
      nsIGlobalObject* aOwner, const ServiceWorkerDescriptor& aDescriptor);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  ServiceWorkerState State() const;

  void SetState(ServiceWorkerState aState);

  void MaybeDispatchStateChangeEvent();

  void GetScriptURL(nsString& aURL) const;

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const StructuredSerializeOptions& aOptions,
                   ErrorResult& aRv);

  const ServiceWorkerDescriptor& Descriptor() const;

  void DisconnectFromOwner() override;

  void RevokeActor(ServiceWorkerChild* aActor);

 private:
  ServiceWorker(nsIGlobalObject* aWindow,
                const ServiceWorkerDescriptor& aDescriptor);

  // This class is reference-counted and will be destroyed from Release().
  ~ServiceWorker();

  void MaybeAttachToRegistration(ServiceWorkerRegistration* aRegistration);

  void Shutdown();

  ServiceWorkerDescriptor mDescriptor;

  RefPtr<ServiceWorkerChild> mActor;
  bool mShutdown;

  RefPtr<ServiceWorkerRegistration> mRegistration;
  ServiceWorkerState mLastNotifiedState;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ServiceWorker, NS_DOM_SERVICEWORKER_IID)

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworker_h__
