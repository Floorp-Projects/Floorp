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

#ifdef XP_WIN
#undef PostMessage
#endif

class nsIGlobalObject;

namespace mozilla {
namespace dom {

namespace ipc {
class StructuredCloneData;
} // namespace ipc

#define NS_DOM_SERVICEWORKER_IID \
  {0xd42e0611, 0x3647, 0x4319, {0xae, 0x05, 0x19, 0x89, 0x59, 0xba, 0x99, 0x5e}}

bool
ServiceWorkerVisible(JSContext* aCx, JSObject* aObj);

class ServiceWorker final : public DOMEventTargetHelper
{
public:
  // Abstract interface for the internal representation of the
  // ServiceWorker object.
  class Inner
  {
  public:
    // This will be called when a DOM ServiceWorker object is
    // created and takes a strong ref to the Inner object.
    // RemoveServiceWorker() is guaranteed to be called on the
    // current thread before the ServiceWorker is destroyed.
    //
    // In addition, the Inner object should check to see if
    // the ServiceWorker's state is correct.  If not, it should
    // be updated automatically by calling SetState().  This is
    // necessary to handle race conditions where the DOM
    // ServiceWorker object is created while the state is being
    // updated in another process.
    virtual void
    AddServiceWorker(ServiceWorker* aWorker) = 0;

    // This is called when the DOM ServiceWorker object is
    // destroyed and drops its ref to the Inner object.
    virtual void
    RemoveServiceWorker(ServiceWorker* aWorker) = 0;

    virtual void
    PostMessage(ipc::StructuredCloneData&& aData,
                const ClientInfo& aClientInfo,
                const ClientState& aClientState) = 0;

    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
  };

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_SERVICEWORKER_IID)
  NS_DECL_ISUPPORTS_INHERITED

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(error)

  static already_AddRefed<ServiceWorker>
  Create(nsIGlobalObject* aOwner, const ServiceWorkerDescriptor& aDescriptor);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  ServiceWorkerState
  State() const;

  void
  SetState(ServiceWorkerState aState);

  void
  GetScriptURL(nsString& aURL) const;

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

  const ServiceWorkerDescriptor&
  Descriptor() const;

  void
  DisconnectFromOwner() override;

private:
  ServiceWorker(nsIGlobalObject* aWindow,
                const ServiceWorkerDescriptor& aDescriptor,
                Inner* aInner);

  // This class is reference-counted and will be destroyed from Release().
  ~ServiceWorker();

  ServiceWorkerDescriptor mDescriptor;

  RefPtr<Inner> mInner;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ServiceWorker, NS_DOM_SERVICEWORKER_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_serviceworker_h__
