/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworker_h__
#define mozilla_dom_serviceworker_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ServiceWorkerBinding.h" // For ServiceWorkerState.

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class ServiceWorkerInfo;
class ServiceWorkerManager;
class SharedWorker;

bool
ServiceWorkerVisible(JSContext* aCx, JSObject* aObj);

class ServiceWorker final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(error)

  static already_AddRefed<ServiceWorker>
  Create(nsIGlobalObject* aOwner, const ServiceWorkerDescriptor& aDescriptor);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  ServiceWorkerState
  State() const
  {
    return mState;
  }

  void
  SetState(ServiceWorkerState aState)
  {
    mState = aState;
  }

  void
  GetScriptURL(nsString& aURL) const;

  void
  DispatchStateChange(ServiceWorkerState aState)
  {
    DOMEventTargetHelper::DispatchTrustedEvent(NS_LITERAL_STRING("statechange"));
  }

#ifdef XP_WIN
#undef PostMessage
#endif

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

private:
  ServiceWorker(nsIGlobalObject* aWindow, ServiceWorkerInfo* aInfo);

  // This class is reference-counted and will be destroyed from Release().
  ~ServiceWorker();

  ServiceWorkerState mState;
  const RefPtr<ServiceWorkerInfo> mInfo;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_serviceworker_h__
