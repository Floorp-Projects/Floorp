/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlyWebFetchEvent_h
#define mozilla_dom_FlyWebFetchEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FlyWebFetchEventBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WebSocket.h"

struct JSContext;
namespace mozilla {
namespace dom {

class Request;
class Response;
class FlyWebPublishedServer;
class InternalRequest;
class InternalResponse;

class FlyWebFetchEvent : public Event
                       , public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(FlyWebFetchEvent, Event)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;
  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  class Request* Request() const
  {
    return mRequest;
  }

  void RespondWith(Promise& aArg, ErrorResult& aRv);

  FlyWebFetchEvent(FlyWebPublishedServer* aServer,
                   class Request* aRequest,
                   InternalRequest* aInternalRequest);

protected:
  virtual ~FlyWebFetchEvent();

  virtual void NotifyServer(InternalResponse* aResponse);

  RefPtr<class Request> mRequest;
  RefPtr<InternalRequest> mInternalRequest;
  RefPtr<FlyWebPublishedServer> mServer;

  bool mResponded;
};

class FlyWebWebSocketEvent final : public FlyWebFetchEvent
{
public:
  FlyWebWebSocketEvent(FlyWebPublishedServer* aServer,
                       class Request* aRequest,
                       InternalRequest* aInternalRequest)
    : FlyWebFetchEvent(aServer, aRequest, aInternalRequest)
  {}

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<WebSocket> Accept(const Optional<nsAString>& aProtocol,
                                     ErrorResult& aRv);

private:
  ~FlyWebWebSocketEvent() {};

  virtual void NotifyServer(InternalResponse* aResponse) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FlyWebFetchEvent_h
