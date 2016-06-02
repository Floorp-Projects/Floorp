/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/FlyWebFetchEventBinding.h"
#include "mozilla/dom/FlyWebPublishedServer.h"
#include "mozilla/dom/FlyWebServerEvents.h"
#include "mozilla/dom/FlyWebWebSocketEventBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"

#include "js/GCAPI.h"

namespace mozilla {
namespace dom {


NS_IMPL_CYCLE_COLLECTION_CLASS(FlyWebFetchEvent)

NS_IMPL_ADDREF_INHERITED(FlyWebFetchEvent, Event)
NS_IMPL_RELEASE_INHERITED(FlyWebFetchEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FlyWebFetchEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(FlyWebFetchEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FlyWebFetchEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRequest)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FlyWebFetchEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

FlyWebFetchEvent::FlyWebFetchEvent(FlyWebPublishedServer* aServer,
                                   class Request* aRequest,
                                   InternalRequest* aInternalRequest)
  : Event(aServer, nullptr, nullptr)
  , mRequest(aRequest)
  , mInternalRequest(aInternalRequest)
  , mServer(aServer)
  , mResponded(false)
{
  MOZ_ASSERT(aServer);
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aInternalRequest);
}

FlyWebFetchEvent::~FlyWebFetchEvent()
{
}

JSObject*
FlyWebFetchEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlyWebFetchEventBinding::Wrap(aCx, this, aGivenProto);
}

void
FlyWebFetchEvent::RespondWith(Promise& aArg, ErrorResult& aRv)
{
  if (mResponded) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mResponded = true;

  aArg.AppendNativeHandler(this);
}

void
FlyWebFetchEvent::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  RefPtr<Response> response;
  if (aValue.isObject()) {
    UNWRAP_OBJECT(Response, &aValue.toObject(), response);
  }

  RefPtr<InternalResponse> intResponse;
  if (response && response->Type() != ResponseType::Opaque) {
    intResponse = response->GetInternalResponse();
  }

  if (!intResponse) {
    intResponse = InternalResponse::NetworkError();
  }

  NotifyServer(intResponse);
}

void
FlyWebFetchEvent::NotifyServer(InternalResponse* aResponse)
{
  mServer->OnFetchResponse(mInternalRequest, aResponse);
}

void
FlyWebFetchEvent::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  RefPtr<InternalResponse> err = InternalResponse::NetworkError();

  NotifyServer(err);
}

JSObject*
FlyWebWebSocketEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlyWebWebSocketEventBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<WebSocket>
FlyWebWebSocketEvent::Accept(const Optional<nsAString>& aProtocol,
                             ErrorResult& aRv)
{
  if (mResponded) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  mResponded = true;

  return mServer->OnWebSocketAccept(mInternalRequest, aProtocol, aRv);
}


void
FlyWebWebSocketEvent::NotifyServer(InternalResponse* aResponse)
{
  mServer->OnWebSocketResponse(mInternalRequest, aResponse);
}


} // namespace dom
} // namespace mozilla
