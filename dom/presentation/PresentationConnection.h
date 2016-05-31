/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationConnection_h
#define mozilla_dom_PresentationConnection_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/PresentationConnectionBinding.h"
#include "mozilla/dom/PresentationConnectionClosedEventBinding.h"
#include "nsIPresentationListener.h"
#include "nsIRequest.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace dom {

class PresentationConnectionList;

class PresentationConnection final : public DOMEventTargetHelper
                                   , public nsIPresentationSessionListener
                                   , public nsIRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationConnection,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER
  NS_DECL_NSIREQUEST

  static already_AddRefed<PresentationConnection>
  Create(nsPIDOMWindowInner* aWindow,
         const nsAString& aId,
         const uint8_t aRole,
         PresentationConnectionList* aList = nullptr);

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

  IMPL_EVENT_HANDLER(connect);
  IMPL_EVENT_HANDLER(close);
  IMPL_EVENT_HANDLER(terminate);
  IMPL_EVENT_HANDLER(message);

private:
  PresentationConnection(nsPIDOMWindowInner* aWindow,
                         const nsAString& aId,
                         const uint8_t aRole,
                         PresentationConnectionList* aList);

  ~PresentationConnection();

  bool Init();

  void Shutdown();

  nsresult ProcessStateChanged(nsresult aReason);

  nsresult DispatchConnectionClosedEvent(PresentationConnectionClosedReason aReason,
                                         const nsAString& aMessage);

  nsresult DispatchMessageEvent(JS::Handle<JS::Value> aData);

  nsresult ProcessConnectionWentAway();

  nsString mId;
  uint8_t mRole;
  PresentationConnectionState mState;
  RefPtr<PresentationConnectionList> mOwningConnectionList;
  nsWeakPtr mWeakLoadGroup;;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationConnection_h
