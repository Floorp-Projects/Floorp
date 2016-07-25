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
#include "GMPUtils.h"
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
                                 nsPIDOMWindowInner* aParent,
                                 MediaKeys* aKeys,
                                 const nsAString& aKeySystem,
                                 const nsAString& aCDMVersion,
                                 MediaKeySessionType aSessionType,
                                 ErrorResult& aRv)
  : DOMEventTargetHelper(aParent)
  , mKeys(aKeys)
  , mKeySystem(aKeySystem)
  , mCDMVersion(aCDMVersion)
  , mSessionType(aSessionType)
  , mToken(sMediaKeySessionNum++)
  , mIsClosed(false)
  , mUninitialized(true)
  , mKeyStatusMap(new MediaKeyStatusMap(aParent))
  , mExpiration(JS::GenericNaN())
{
  EME_LOG("MediaKeySession[%p,''] session Id set", this);

  MOZ_ASSERT(aParent);
  if (aRv.Failed()) {
    return;
  }
  mClosed = MakePromise(aRv, NS_LITERAL_CSTRING("MediaKeys.createSession"));
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
MediaKeySession::GetKeySystem(nsString& aOutKeySystem) const
{
  aOutKeySystem.Assign(mKeySystem);
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
  return mExpiration;
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
      message.Append(nsPrintfCString(" (%s,%s)", ToBase64(status.mId).get(),
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
  RefPtr<DetailedPromise> promise(MakePromise(aRv,
    NS_LITERAL_CSTRING("MediaKeySession.generateRequest")));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mUninitialized) {
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, uninitialized",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR,
      NS_LITERAL_CSTRING("Session is already initialized in MediaKeySession.generateRequest()"));
    return promise.forget();
  }

  mUninitialized = false;

  if (aInitDataType.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR,
      NS_LITERAL_CSTRING("Empty initDataType passed to MediaKeySession.generateRequest()"));
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, empty initDataType",
      this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  CopyArrayBufferViewOrArrayBufferData(aInitData, data);
  if (data.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR,
      NS_LITERAL_CSTRING("Empty initData passed to MediaKeySession.generateRequest()"));
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, empty initData",
      this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  // Convert initData to base64 for easier logging.
  // Note: CreateSession() Move()s the data out of the array, so we have
  // to copy it here.
  nsAutoCString base64InitData(ToBase64(data));
  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->CreateSession(Token(),
                                      mSessionType,
                                      pid,
                                      aInitDataType, data);

  EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() sent, "
          "promiseId=%d initData(base64)='%s' initDataType='%s'",
          this,
          NS_ConvertUTF16toUTF8(mSessionId).get(),
          pid,
          base64InitData.get(),
          NS_ConvertUTF16toUTF8(aInitDataType).get());

  return promise.forget();
}

already_AddRefed<Promise>
MediaKeySession::Load(const nsAString& aSessionId, ErrorResult& aRv)
{
  RefPtr<DetailedPromise> promise(MakePromise(aRv,
    NS_LITERAL_CSTRING("MediaKeySession.load")));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aSessionId.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR,
                         NS_LITERAL_CSTRING("Trying to load a session with empty session ID"));
    // "The sessionId parameter is empty."
    EME_LOG("MediaKeySession[%p,''] Load() failed, no sessionId", this);
    return promise.forget();
  }

  if (!mUninitialized) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR,
                         NS_LITERAL_CSTRING("Session is already initialized in MediaKeySession.load()"));
    EME_LOG("MediaKeySession[%p,'%s'] Load() failed, uninitialized",
      this, NS_ConvertUTF16toUTF8(aSessionId).get());
    return promise.forget();
  }

  mUninitialized = false;

  // We now know the sessionId being loaded into this session. Remove the
  // session from its owning MediaKey's set of sessions awaiting a sessionId.
  RefPtr<MediaKeySession> session(mKeys->GetPendingSession(Token()));
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
  RefPtr<DetailedPromise> promise(MakePromise(aRv,
    NS_LITERAL_CSTRING("MediaKeySession.update")));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!IsCallable()) {
    // If this object's callable value is false, return a promise rejected
    // with a new DOMException whose name is InvalidStateError.
    EME_LOG("MediaKeySession[%p,''] Update() called before sessionId set by CDM", this);
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("MediaKeySession.Update() called before sessionId set by CDM"));
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                         NS_LITERAL_CSTRING("Session is closed or was not properly initialized"));
    EME_LOG("MediaKeySession[%p,'%s'] Update() failed, session is closed or was not properly initialised.",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }
  CopyArrayBufferViewOrArrayBufferData(aResponse, data);
  if (data.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR,
      NS_LITERAL_CSTRING("Empty response buffer passed to MediaKeySession.update()"));
    EME_LOG("MediaKeySession[%p,'%s'] Update() failed, empty response buffer",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }


  // Convert response to base64 for easier logging.
  // Note: UpdateSession() Move()s the data out of the array, so we have
  // to copy it here.
  nsAutoCString base64Response(ToBase64(data));

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
  RefPtr<DetailedPromise> promise(MakePromise(aRv,
    NS_LITERAL_CSTRING("MediaKeySession.close")));
  if (aRv.Failed()) {
    return nullptr;
  }
  if (!IsCallable()) {
    // If this object's callable value is false, return a promise rejected
    // with a new DOMException whose name is InvalidStateError.
    EME_LOG("MediaKeySession[%p,''] Close() called before sessionId set by CDM", this);
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("MediaKeySession.Close() called before sessionId set by CDM"));
    return promise.forget();
  }
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    EME_LOG("MediaKeySession[%p,'%s'] Close() already closed",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return promise.forget();
  }
  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->CloseSession(mSessionId, pid);

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
  RefPtr<DetailedPromise> promise(MakePromise(aRv,
    NS_LITERAL_CSTRING("MediaKeySession.remove")));
  if (aRv.Failed()) {
    return nullptr;
  }
  if (!IsCallable()) {
    // If this object's callable value is false, return a promise rejected
    // with a new DOMException whose name is InvalidStateError.
    EME_LOG("MediaKeySession[%p,''] Remove() called before sessionId set by CDM", this);
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("MediaKeySession.Remove() called before sessionId set by CDM"));
    return promise.forget();
  }
  if (mSessionType != MediaKeySessionType::Persistent_license) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR,
                         NS_LITERAL_CSTRING("Calling MediaKeySession.remove() on non-persistent session"));
    // "The operation is not supported on session type sessions."
    EME_LOG("MediaKeySession[%p,'%s'] Remove() failed, sesion not persisrtent.",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                         NS_LITERAL_CSTRING("MediaKeySesison.remove() called but session is not active"));
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
    EME_LOG("MediaKeySession[%p,'%s'] DispatchKeyMessage() type=%s message(base64)='%s'",
            this, NS_ConvertUTF16toUTF8(mSessionId).get(),
            MediaKeyMessageTypeValues::strings[uint32_t(aMessageType)].value,
            ToBase64(aMessage).get());
  }

  RefPtr<MediaKeyMessageEvent> event(
    MediaKeyMessageEvent::Constructor(this, aMessageType, aMessage));
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

void
MediaKeySession::DispatchKeyError(uint32_t aSystemCode)
{
  EME_LOG("MediaKeySession[%p,'%s'] DispatchKeyError() systemCode=%u.",
          this, NS_ConvertUTF16toUTF8(mSessionId).get(), aSystemCode);

  RefPtr<MediaKeyError> event(new MediaKeyError(this, aSystemCode));
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
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

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("keystatuseschange"), false);
  asyncDispatcher->PostDOMEvent();
}

uint32_t
MediaKeySession::Token() const
{
  return mToken;
}

already_AddRefed<DetailedPromise>
MediaKeySession::MakePromise(ErrorResult& aRv, const nsACString& aName)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    NS_WARNING("Passed non-global to MediaKeys ctor!");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  return DetailedPromise::Create(global, aRv, aName);
}

void
MediaKeySession::SetExpiration(double aExpiration)
{
  EME_LOG("MediaKeySession[%p,'%s'] SetExpiry(%lf)",
          this,
          NS_ConvertUTF16toUTF8(mSessionId).get(),
          aExpiration);
  mExpiration = aExpiration;
}

} // namespace dom
} // namespace mozilla
