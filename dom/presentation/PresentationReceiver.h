/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationReceiver_h
#define mozilla_dom_PresentationReceiver_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsIPresentationListener.h"

namespace mozilla {
namespace dom {

class Promise;
class PresentationConnection;

class PresentationReceiver final : public DOMEventTargetHelper
                                 , public nsIPresentationRespondingListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationReceiver,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONRESPONDINGLISTENER

  static already_AddRefed<PresentationReceiver> Create(nsPIDOMWindow* aWindow,
                                                       const nsAString& aSessionId);

  virtual void DisconnectFromOwner() override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  already_AddRefed<Promise> GetConnection(ErrorResult& aRv);

  already_AddRefed<Promise> GetConnections(ErrorResult& aRv) const;

  IMPL_EVENT_HANDLER(connectionavailable);

private:
  explicit PresentationReceiver(nsPIDOMWindow* aWindow);

  ~PresentationReceiver();

  bool Init(const nsAString& aSessionId);

  void Shutdown();

  nsresult DispatchConnectionAvailableEvent();

  // Store the inner window ID for |UnregisterRespondingListener| call in
  // |Shutdown| since the inner window may not exist at that moment.
  uint64_t mWindowId;

  nsTArray<nsRefPtr<PresentationConnection>> mConnections;
  nsTArray<nsRefPtr<Promise>> mPendingGetConnectionPromises;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationReceiver_h
