/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/MediaKeyError.h"
#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeyNeededEvent.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/AsyncEventDispatcher.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaKeySession,
                                   DOMEventTargetHelper,
                                   mMediaKeyError,
                                   mKeys,
                                   mClosed)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaKeySession)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaKeySession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaKeySession, DOMEventTargetHelper)

MediaKeySession::MediaKeySession(nsPIDOMWindow* aParent,
                                 MediaKeys* aKeys,
                                 const nsAString& aKeySystem,
                                 SessionType aSessionType,
                                 ErrorResult& aRv)
  : DOMEventTargetHelper(aParent)
  , mKeys(aKeys)
  , mKeySystem(aKeySystem)
  , mSessionType(aSessionType)
  , mIsClosed(false)
{
  MOZ_ASSERT(aParent);
  mClosed = mKeys->MakePromise(aRv);
}

void MediaKeySession::Init(const nsAString& aSessionId)
{
  mSessionId = aSessionId;
}

MediaKeySession::~MediaKeySession()
{
}

MediaKeyError*
MediaKeySession::GetError() const
{
  return mMediaKeyError;
}

void
MediaKeySession::GetKeySystem(nsString& aKeySystem) const
{
  aKeySystem = mKeySystem;
}

void
MediaKeySession::GetSessionId(nsString& aSessionId) const
{
  aSessionId = mSessionId;
}

JSObject*
MediaKeySession::WrapObject(JSContext* aCx)
{
  return MediaKeySessionBinding::Wrap(aCx, this);
}

double
MediaKeySession::Expiration() const
{
  return JS::GenericNaN();
}

Promise*
MediaKeySession::Closed() const
{
  return mClosed;
}

already_AddRefed<Promise>
MediaKeySession::Update(const Uint8Array& aResponse, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(mKeys->MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }
  aResponse.ComputeLengthAndData();
  if (IsClosed() ||
      !mKeys->GetCDMProxy() ||
      !aResponse.Length()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }
  mKeys->GetCDMProxy()->UpdateSession(mSessionId,
                                      mKeys->StorePromise(promise),
                                      aResponse);
  return promise.forget();
}

already_AddRefed<Promise>
MediaKeySession::Close(ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(mKeys->MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return promise.forget();
  }
  mKeys->GetCDMProxy()->CloseSession(mSessionId, mKeys->StorePromise(promise));

  return promise.forget();
}

void
MediaKeySession::OnClosed()
{
  if (IsClosed()) {
    return;
  }
  mIsClosed = true;
  // TODO: reset usableKeyIds
  mKeys->OnSessionClosed(this);
  mKeys = nullptr;
  mClosed->MaybeResolve(JS::UndefinedHandleValue);
}

bool
MediaKeySession::IsClosed() const
{
  return mIsClosed;
}

already_AddRefed<Promise>
MediaKeySession::Remove(ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(mKeys->MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }
  if (mSessionType != SessionType::Persistent) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    // "The operation is not supported on session type sessions."
    return promise.forget();
  }
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    // "The session is closed."
    return promise.forget();
  }
  mKeys->GetCDMProxy()->RemoveSession(mSessionId, mKeys->StorePromise(promise));
  return promise.forget();
}

void
MediaKeySession::DispatchKeyMessage(const nsTArray<uint8_t>& aMessage,
                                    const nsString& aURL)
{
  nsRefPtr<MediaKeyMessageEvent> event(
    MediaKeyMessageEvent::Constructor(this, aURL, aMessage));
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

void
MediaKeySession::DispatchKeyError(uint32_t aSystemCode)
{
  RefPtr<MediaKeyError> event(new MediaKeyError(this, aSystemCode));
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

} // namespace dom
} // namespace mozilla
