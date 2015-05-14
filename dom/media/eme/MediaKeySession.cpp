/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/MediaKeyError.h"
#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaEncryptedEvent.h"
#include "mozilla/dom/MediaKeyStatusMap.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Move.h"
#include "nsContentUtils.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/Base64.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaKeySession,
                                   DOMEventTargetHelper,
                                   mMediaKeyError,
                                   mKeys,
                                   mKeyStatusMap,
                                   mClosed)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaKeySession)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaKeySession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaKeySession, DOMEventTargetHelper)

// Count of number of instances. Used to give each instance a
// unique token.
static uint32_t sMediaKeySessionNum = 0;

MediaKeySession::MediaKeySession(JSContext* aCx,
                                 nsPIDOMWindow* aParent,
                                 MediaKeys* aKeys,
                                 const nsAString& aKeySystem,
                                 SessionType aSessionType,
                                 ErrorResult& aRv)
  : DOMEventTargetHelper(aParent)
  , mKeys(aKeys)
  , mKeySystem(aKeySystem)
  , mSessionType(aSessionType)
  , mToken(sMediaKeySessionNum++)
  , mIsClosed(false)
  , mUninitialized(true)
  , mKeyStatusMap(new MediaKeyStatusMap(aCx, aParent, aRv))
{
  EME_LOG("MediaKeySession[%p,''] session Id set", this);

  MOZ_ASSERT(aParent);
  if (aRv.Failed()) {
    return;
  }
  mClosed = mKeys->MakePromise(aRv);
}

void MediaKeySession::SetSessionId(const nsAString& aSessionId)
{
  EME_LOG("MediaKeySession[%p,'%s'] session Id set",
          this, NS_ConvertUTF16toUTF8(aSessionId).get());

  if (NS_WARN_IF(!mSessionId.IsEmpty())) {
    return;
  }
  mSessionId = aSessionId;
  mKeys->OnSessionIdReady(this);
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
  aSessionId = GetSessionId();
}

const nsString&
MediaKeySession::GetSessionId() const
{
  return mSessionId;
}

JSObject*
MediaKeySession::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaKeySessionBinding::Wrap(aCx, this, aGivenProto);
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


void
MediaKeySession::UpdateKeyStatusMap()
{
  MOZ_ASSERT(!IsClosed());
  if (!mKeys->GetCDMProxy()) {
    return;
  }

  nsTArray<CDMCaps::KeyStatus> keyStatuses;
  {
    CDMCaps::AutoLock caps(mKeys->GetCDMProxy()->Capabilites());
    caps.GetKeyStatusesForSession(mSessionId, keyStatuses);
  }

  mKeyStatusMap->Update(keyStatuses);

  if (EME_LOG_ENABLED()) {
    nsAutoCString message(
      nsPrintfCString("MediaKeySession[%p,'%s'] key statuses change {",
                      this, NS_ConvertUTF16toUTF8(mSessionId).get()));
    for (const CDMCaps::KeyStatus& status : keyStatuses) {
      nsAutoCString base64KeyId;
      nsDependentCSubstring rawKeyId(reinterpret_cast<const char*>(status.mId.Elements()),
                                     status.mId.Length());
      nsresult rv = Base64Encode(rawKeyId, base64KeyId);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }
      message.Append(nsPrintfCString(" (%s,%s)", base64KeyId.get(),
        MediaKeyStatusValues::strings[status.mStatus].value));
    }
    message.Append(" }");
    EME_LOG(message.get());
  }
}

MediaKeyStatusMap*
MediaKeySession::KeyStatuses() const
{
  return mKeyStatusMap;
}

already_AddRefed<Promise>
MediaKeySession::GenerateRequest(const nsAString& aInitDataType,
                                 const ArrayBufferViewOrArrayBuffer& aInitData,
                                 ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(mKeys->MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mUninitialized) {
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, uninitialized",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  mUninitialized = false;

  nsTArray<uint8_t> data;
  if (aInitDataType.IsEmpty() ||
      !CopyArrayBufferViewOrArrayBufferData(aInitData, data)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, "
            "invalid initData or initDataType",
      this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  // Convert initData to base64 for easier logging.
  // Note: UpdateSession() Move()s the data out of the array, so we have
  // to copy it here.
  nsAutoCString base64InitData;
  if (EME_LOG_ENABLED()) {
    nsDependentCSubstring rawInitData(reinterpret_cast<const char*>(data.Elements()),
      data.Length());
    if (NS_FAILED(Base64Encode(rawInitData, base64InitData))) {
      NS_WARNING("Failed to base64 encode initData for logging");
    }
  }

  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->CreateSession(Token(),
                                      mSessionType,
                                      pid,
                                      aInitDataType, data);

  EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() sent, "
          "promiseId=%d initData(base64)='%s'",
          this,
          NS_ConvertUTF16toUTF8(mSessionId).get(),
          pid,
          base64InitData.get());

  return promise.forget();
}

already_AddRefed<Promise>
MediaKeySession::Load(const nsAString& aSessionId, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(mKeys->MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aSessionId.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    // "The sessionId parameter is empty."
    EME_LOG("MediaKeySession[%p,''] Load() failed, no sessionId", this);
    return promise.forget();
  }

  if (!mUninitialized) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    EME_LOG("MediaKeySession[%p,'%s'] Load() failed, uninitialized",
      this, NS_ConvertUTF16toUTF8(aSessionId).get());
    return promise.forget();
  }

  mUninitialized = false;

  // We now know the sessionId being loaded into this session. Remove the
  // session from its owning MediaKey's set of sessions awaiting a sessionId.
  nsRefPtr<MediaKeySession> session(mKeys->GetPendingSession(Token()));
  MOZ_ASSERT(session == this, "Session should be awaiting id on its own token");

  // Associate with the known sessionId.
  SetSessionId(aSessionId);

  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->LoadSession(pid, aSessionId);

  EME_LOG("MediaKeySession[%p,'%s'] Load() sent to CDM, promiseId=%d",
    this, NS_ConvertUTF16toUTF8(mSessionId).get(), pid);

  return promise.forget();
}

already_AddRefed<Promise>
MediaKeySession::Update(const ArrayBufferViewOrArrayBuffer& aResponse, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(mKeys->MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }
  nsTArray<uint8_t> data;
  if (IsClosed() ||
      !mKeys->GetCDMProxy() ||
      !CopyArrayBufferViewOrArrayBufferData(aResponse, data)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    EME_LOG("MediaKeySession[%p,'%s'] Update() failed, invalid response buffer",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }


  // Convert response to base64 for easier logging.
  // Note: UpdateSession() Move()s the data out of the array, so we have
  // to copy it here.
  nsAutoCString base64Response;
  if (EME_LOG_ENABLED()) {
    nsDependentCSubstring rawResponse(reinterpret_cast<const char*>(data.Elements()),
      data.Length());
    if (NS_FAILED(Base64Encode(rawResponse, base64Response))) {
      NS_WARNING("Failed to base64 encode response for logging");
    }
  }

  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->UpdateSession(mSessionId,
                                      pid,
                                      data);

  EME_LOG("MediaKeySession[%p,'%s'] Update() sent to CDM, "
          "promiseId=%d Response(base64)='%s'",
           this,
           NS_ConvertUTF16toUTF8(mSessionId).get(),
           pid,
           base64Response.get());

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
    EME_LOG("MediaKeySession[%p,'%s'] Close() already closed",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return promise.forget();
  }
  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->CloseSession(mSessionId, mKeys->StorePromise(promise));

  EME_LOG("MediaKeySession[%p,'%s'] Close() sent to CDM, promiseId=%d",
          this, NS_ConvertUTF16toUTF8(mSessionId).get(), pid);

  return promise.forget();
}

void
MediaKeySession::OnClosed()
{
  if (IsClosed()) {
    return;
  }
  EME_LOG("MediaKeySession[%p,'%s'] session close operation complete.",
          this, NS_ConvertUTF16toUTF8(mSessionId).get());
  mIsClosed = true;
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
    EME_LOG("MediaKeySession[%p,'%s'] Remove() failed, sesion not persisrtent.",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    // "The session is closed."
    EME_LOG("MediaKeySession[%p,'%s'] Remove() failed, already session closed.",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }
  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->RemoveSession(mSessionId, pid);
  EME_LOG("MediaKeySession[%p,'%s'] Remove() sent to CDM, promiseId=%d.",
          this, NS_ConvertUTF16toUTF8(mSessionId).get(), pid);

  return promise.forget();
}

void
MediaKeySession::DispatchKeyMessage(MediaKeyMessageType aMessageType,
                                    const nsTArray<uint8_t>& aMessage)
{
  if (EME_LOG_ENABLED()) {
    nsAutoCString base64MsgData;
    nsDependentCSubstring rawMsgData(reinterpret_cast<const char*>(aMessage.Elements()),
                                     aMessage.Length());
    if (NS_FAILED(Base64Encode(rawMsgData, base64MsgData))) {
      NS_WARNING("Failed to base64 encode message for logging");
    }
    EME_LOG("MediaKeySession[%p,'%s'] DispatchKeyMessage() type=%s message(base64)='%s'",
            this, NS_ConvertUTF16toUTF8(mSessionId).get(),
            MediaKeyMessageTypeValues::strings[uint32_t(aMessageType)].value,
            base64MsgData.get());
  }

  nsRefPtr<MediaKeyMessageEvent> event(
    MediaKeyMessageEvent::Constructor(this, aMessageType, aMessage));
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

void
MediaKeySession::DispatchKeyError(uint32_t aSystemCode)
{
  EME_LOG("MediaKeySession[%p,'%s'] DispatchKeyError() systemCode=%u.",
          this, NS_ConvertUTF16toUTF8(mSessionId).get(), aSystemCode);

  RefPtr<MediaKeyError> event(new MediaKeyError(this, aSystemCode));
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

void
MediaKeySession::DispatchKeyStatusesChange()
{
  if (IsClosed()) {
    return;
  }

  UpdateKeyStatusMap();

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("keystatuseschange"), false);
  asyncDispatcher->PostDOMEvent();
}

uint32_t
MediaKeySession::Token() const
{
  return mToken;
}

} // namespace dom
} // namespace mozilla
