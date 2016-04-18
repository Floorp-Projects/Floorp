/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationConnection_h
#define mozilla_dom_PresentationConnection_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/PresentationConnectionBinding.h"
#include "nsIPresentationListener.h"

namespace mozilla {
namespace dom {

class PresentationConnection final : public DOMEventTargetHelper
                                   , public nsIPresentationSessionListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationConnection,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER

  static already_AddRefed<PresentationConnection> Create(nsPIDOMWindowInner* aWindow,
                                                         const nsAString& aId,
                                                         const uint8_t aRole,
                                                         PresentationConnectionState aState);

  virtual void DisconnectFromOwner() override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  void GetId(nsAString& aId) const;

  PresentationConnectionState State() const;

  void Send(const nsAString& aData,
            ErrorResult& aRv);

  void Close(ErrorResult& aRv);

  void Terminate(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(statechange);
  IMPL_EVENT_HANDLER(message);

private:
  PresentationConnection(nsPIDOMWindowInner* aWindow,
                         const nsAString& aId,
                         const uint8_t aRole,
                         PresentationConnectionState aState);

  ~PresentationConnection();

  bool Init();

  void Shutdown();

  nsresult DispatchStateChangeEvent();

  nsresult DispatchMessageEvent(JS::Handle<JS::Value> aData);

  nsString mId;
  uint8_t mRole;
  PresentationConnectionState mState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationConnection_h
