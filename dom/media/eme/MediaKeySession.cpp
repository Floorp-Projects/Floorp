/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeySession.h"

#include <ctime>
#include <utility>

#include "GMPUtils.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/Encoding.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/KeyIdsInitDataBinding.h"
#include "mozilla/dom/MediaEncryptedEvent.h"
#include "mozilla/dom/MediaKeyError.h"
#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeyStatusMap.h"
#include "mozilla/dom/MediaKeySystemAccess.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPrintfCString.h"
#include "psshparser/PsshParser.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaKeySession, DOMEventTargetHelper,
                                   mMediaKeyError, mKeys, mKeyStatusMap,
                                   mClosed)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeySession)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaKeySession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaKeySession, DOMEventTargetHelper)

// Count of number of instances. Used to give each instance a
// unique token.
static uint32_t sMediaKeySessionNum = 0;

// Max length of keyId in EME "keyIds" or WebM init data format, as enforced
// by web platform tests.
static const uint32_t MAX_KEY_ID_LENGTH = 512;

// Max length of CENC PSSH init data tolerated, as enforced by web
// platform tests.
static const uint32_t MAX_CENC_INIT_DATA_LENGTH = 64 * 1024;

MediaKeySession::MediaKeySession(nsPIDOMWindowInner* aParent, MediaKeys* aKeys,
                                 const nsAString& aKeySystem,
                                 MediaKeySessionType aSessionType,
                                 ErrorResult& aRv)
    : DOMEventTargetHelper(aParent),
      mKeys(aKeys),
      mKeySystem(aKeySystem),
      mSessionType(aSessionType),
      mToken(sMediaKeySessionNum++),
      mIsClosed(false),
      mUninitialized(true),
      mKeyStatusMap(new MediaKeyStatusMap(aParent)),
      mExpiration(JS::GenericNaN()) {
  EME_LOG("MediaKeySession[%p,''] ctor", this);

  MOZ_ASSERT(aParent);
  if (aRv.Failed()) {
    return;
  }
  mClosed = MakePromise(aRv, "MediaKeys.createSession"_ns);
}

void MediaKeySession::SetSessionId(const nsAString& aSessionId) {
  EME_LOG("MediaKeySession[%p,'%s'] session Id set", this,
          NS_ConvertUTF16toUTF8(aSessionId).get());

  if (NS_WARN_IF(!mSessionId.IsEmpty())) {
    return;
  }
  mSessionId = aSessionId;
  mKeys->OnSessionIdReady(this);
}

MediaKeySession::~MediaKeySession() {
  EME_LOG("MediaKeySession[%p,'%s'] dtor", this,
          NS_ConvertUTF16toUTF8(mSessionId).get());
}

MediaKeyError* MediaKeySession::GetError() const { return mMediaKeyError; }

void MediaKeySession::GetSessionId(nsString& aSessionId) const {
  aSessionId = GetSessionId();
}

const nsString& MediaKeySession::GetSessionId() const { return mSessionId; }

JSObject* MediaKeySession::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return MediaKeySession_Binding::Wrap(aCx, this, aGivenProto);
}

double MediaKeySession::Expiration() const { return mExpiration; }

Promise* MediaKeySession::Closed() const { return mClosed; }

void MediaKeySession::UpdateKeyStatusMap() {
  MOZ_ASSERT(!IsClosed());
  if (!mKeys->GetCDMProxy()) {
    return;
  }

  nsTArray<CDMCaps::KeyStatus> keyStatuses;
  {
    auto caps = mKeys->GetCDMProxy()->Capabilites().Lock();
    caps->GetKeyStatusesForSession(mSessionId, keyStatuses);
  }

  mKeyStatusMap->Update(keyStatuses);

  if (EME_LOG_ENABLED()) {
    nsAutoCString message(
        nsPrintfCString("MediaKeySession[%p,'%s'] key statuses change {", this,
                        NS_ConvertUTF16toUTF8(mSessionId).get()));
    for (const CDMCaps::KeyStatus& status : keyStatuses) {
      message.AppendPrintf(
          " (%s,%s)", ToHexString(status.mId).get(),
          nsCString(MediaKeyStatusValues::GetString(status.mStatus)).get());
    }
    message.AppendLiteral(" }");
    // Use %s so we aren't exposing random strings to printf interpolation.
    EME_LOG("%s", message.get());
  }
}

MediaKeyStatusMap* MediaKeySession::KeyStatuses() const {
  return mKeyStatusMap;
}

// The user agent MUST thoroughly validate the Initialization Data before
// passing it to the CDM. This includes verifying that the length and
// values of fields are reasonable, verifying that values are within
// reasonable limits, and stripping irrelevant, unsupported, or unknown
// data or fields. It is RECOMMENDED that user agents pre-parse, sanitize,
// and/or generate a fully sanitized version of the Initialization Data.
// If the Initialization Data format specified by initDataType supports
// multiple entries, the user agent SHOULD remove entries that are not
// needed by the CDM. The user agent MUST NOT re-order entries within
// the Initialization Data.
static bool ValidateInitData(const nsTArray<uint8_t>& aInitData,
                             const nsAString& aInitDataType) {
  if (aInitDataType.LowerCaseEqualsLiteral("webm")) {
    // WebM initData consists of a single keyId. Ensure it's of reasonable
    // length.
    return aInitData.Length() <= MAX_KEY_ID_LENGTH;
  } else if (aInitDataType.LowerCaseEqualsLiteral("cenc")) {
    // Limit initData to less than 64KB.
    if (aInitData.Length() > MAX_CENC_INIT_DATA_LENGTH) {
      return false;
    }
    std::vector<std::vector<uint8_t>> keyIds;
    return ParseCENCInitData(aInitData.Elements(), aInitData.Length(), keyIds);
  } else if (aInitDataType.LowerCaseEqualsLiteral("keyids")) {
    if (aInitData.Length() > MAX_KEY_ID_LENGTH) {
      return false;
    }
    // Ensure that init data matches the expected JSON format.
    mozilla::dom::KeyIdsInitData keyIds;
    nsString json;
    nsDependentCSubstring raw(
        reinterpret_cast<const char*>(aInitData.Elements()),
        aInitData.Length());
    if (NS_FAILED(UTF_8_ENCODING->DecodeWithBOMRemoval(raw, json))) {
      return false;
    }
    if (!keyIds.Init(json)) {
      return false;
    }
    if (keyIds.mKids.Length() == 0) {
      return false;
    }
    for (const auto& kid : keyIds.mKids) {
      if (kid.IsEmpty()) {
        return false;
      }
    }
  }
  return true;
}

// Generates a license request based on the initData. A message of type
// "license-request" or "individualization-request" will always be queued
// if the algorithm succeeds and the promise is resolved.
already_AddRefed<Promise> MediaKeySession::GenerateRequest(
    const nsAString& aInitDataType,
    const ArrayBufferViewOrArrayBuffer& aInitData, ErrorResult& aRv) {
  RefPtr<DetailedPromise> promise(
      MakePromise(aRv, "MediaKeySession.generateRequest"_ns));
  if (aRv.Failed()) {
    return nullptr;
  }

  // If this object is closed, return a promise rejected with an
  // InvalidStateError.
  if (IsClosed()) {
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, closed", this,
            NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeRejectWithInvalidStateError(
        "Session is closed in MediaKeySession.generateRequest()");
    return promise.forget();
  }

  // If this object's uninitialized value is false, return a promise rejected
  // with an InvalidStateError.
  if (!mUninitialized) {
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, uninitialized",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeRejectWithInvalidStateError(
        "Session is already initialized in MediaKeySession.generateRequest()");
    return promise.forget();
  }

  // Let this object's uninitialized value be false.
  mUninitialized = false;

  // If initDataType is the empty string, return a promise rejected
  // with a newly created TypeError.
  if (aInitDataType.IsEmpty()) {
    promise->MaybeRejectWithTypeError(
        "Empty initDataType passed to MediaKeySession.generateRequest()");
    EME_LOG(
        "MediaKeySession[%p,'%s'] GenerateRequest() failed, empty initDataType",
        this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  // If initData is an empty array, return a promise rejected with
  // a newly created TypeError.
  nsTArray<uint8_t> data;
  CopyArrayBufferViewOrArrayBufferData(aInitData, data);
  if (data.IsEmpty()) {
    promise->MaybeRejectWithTypeError(
        "Empty initData passed to MediaKeySession.generateRequest()");
    EME_LOG("MediaKeySession[%p,'%s'] GenerateRequest() failed, empty initData",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  // If the Key System implementation represented by this object's
  // cdm implementation value does not support initDataType as an
  // Initialization Data Type, return a promise rejected with a
  // NotSupportedError. String comparison is case-sensitive.
  if (!MediaKeySystemAccess::KeySystemSupportsInitDataType(mKeySystem,
                                                           aInitDataType)) {
    promise->MaybeRejectWithNotSupportedError(
        "Unsupported initDataType passed to MediaKeySession.generateRequest()");
    EME_LOG(
        "MediaKeySession[%p,'%s'] GenerateRequest() failed, unsupported "
        "initDataType",
        this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  // Let init data be a copy of the contents of the initData parameter.
  // Note: Handled by the CopyArrayBufferViewOrArrayBufferData call above.

  // Let session type be this object's session type.

  // Let promise be a new promise.

  // Run the following steps in parallel:

  // If the init data is not valid for initDataType, reject promise with
  // a newly created TypeError.
  if (!ValidateInitData(data, aInitDataType)) {
    // If the preceding step failed, reject promise with a newly created
    // TypeError.
    promise->MaybeRejectWithTypeError(
        "initData sanitization failed in MediaKeySession.generateRequest()");
    EME_LOG(
        "MediaKeySession[%p,'%s'] GenerateRequest() initData sanitization "
        "failed",
        this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  // Let sanitized init data be a validated and sanitized version of init data.

  // If sanitized init data is empty, reject promise with a NotSupportedError.

  // Note: Remaining steps of generateRequest method continue in CDM.

  // Convert initData to hex for easier logging.
  // Note: CreateSession() std::move()s the data out of the array, so we have
  // to copy it here.
  nsAutoCString hexInitData(ToHexString(data));
  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->ConnectPendingPromiseIdWithToken(pid, Token());
  mKeys->GetCDMProxy()->CreateSession(Token(), mSessionType, pid, aInitDataType,
                                      data);

  EME_LOG(
      "MediaKeySession[%p,'%s'] GenerateRequest() sent, "
      "promiseId=%d initData='%s' initDataType='%s'",
      this, NS_ConvertUTF16toUTF8(mSessionId).get(), pid, hexInitData.get(),
      NS_ConvertUTF16toUTF8(aInitDataType).get());

  return promise.forget();
}

already_AddRefed<Promise> MediaKeySession::Load(const nsAString& aSessionId,
                                                ErrorResult& aRv) {
  RefPtr<DetailedPromise> promise(MakePromise(aRv, "MediaKeySession.load"_ns));
  if (aRv.Failed()) {
    return nullptr;
  }

  // 1. If this object is closed, return a promise rejected with an
  // InvalidStateError.
  if (IsClosed()) {
    promise->MaybeRejectWithInvalidStateError(
        "Session is closed in MediaKeySession.load()");
    EME_LOG("MediaKeySession[%p,'%s'] Load() failed, closed", this,
            NS_ConvertUTF16toUTF8(aSessionId).get());
    return promise.forget();
  }

  // 2.If this object's uninitialized value is false, return a promise rejected
  // with an InvalidStateError.
  if (!mUninitialized) {
    promise->MaybeRejectWithInvalidStateError(
        "Session is already initialized in MediaKeySession.load()");
    EME_LOG("MediaKeySession[%p,'%s'] Load() failed, uninitialized", this,
            NS_ConvertUTF16toUTF8(aSessionId).get());
    return promise.forget();
  }

  // 3.Let this object's uninitialized value be false.
  mUninitialized = false;

  // 4. If sessionId is the empty string, return a promise rejected with a newly
  // created TypeError.
  if (aSessionId.IsEmpty()) {
    promise->MaybeRejectWithTypeError(
        "Trying to load a session with empty session ID");
    // "The sessionId parameter is empty."
    EME_LOG("MediaKeySession[%p,''] Load() failed, no sessionId", this);
    return promise.forget();
  }

  // 5. If the result of running the Is persistent session type? algorithm
  // on this object's session type is false, return a promise rejected with
  // a newly created TypeError.
  if (mSessionType == MediaKeySessionType::Temporary) {
    promise->MaybeRejectWithTypeError(
        "Trying to load() into a non-persistent session");
    EME_LOG(
        "MediaKeySession[%p,''] Load() failed, can't load in a non-persistent "
        "session",
        this);
    return promise.forget();
  }

  // Note: We don't support persistent sessions in any keysystem, so all calls
  // to Load() should reject with a TypeError in the preceding check. Omitting
  // implementing the rest of the specified MediaKeySession::Load() algorithm.

  // We now know the sessionId being loaded into this session. Remove the
  // session from its owning MediaKey's set of sessions awaiting a sessionId.
  RefPtr<MediaKeySession> session(mKeys->GetPendingSession(Token()));
  MOZ_ASSERT(session == this, "Session should be awaiting id on its own token");

  // Associate with the known sessionId.
  SetSessionId(aSessionId);

  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->LoadSession(pid, mSessionType, aSessionId);

  EME_LOG("MediaKeySession[%p,'%s'] Load() sent to CDM, promiseId=%d", this,
          NS_ConvertUTF16toUTF8(mSessionId).get(), pid);

  return promise.forget();
}

already_AddRefed<Promise> MediaKeySession::Update(
    const ArrayBufferViewOrArrayBuffer& aResponse, ErrorResult& aRv) {
  RefPtr<DetailedPromise> promise(
      MakePromise(aRv, "MediaKeySession.update"_ns));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!IsCallable()) {
    // If this object's callable value is false, return a promise rejected
    // with a new DOMException whose name is InvalidStateError.
    EME_LOG(
        "MediaKeySession[%p,''] Update() called before sessionId set by CDM",
        this);
    promise->MaybeRejectWithInvalidStateError(
        "MediaKeySession.Update() called before sessionId set by CDM");
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    promise->MaybeRejectWithInvalidStateError(
        "Session is closed or was not properly initialized");
    EME_LOG(
        "MediaKeySession[%p,'%s'] Update() failed, session is closed or was "
        "not properly initialised.",
        this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }
  CopyArrayBufferViewOrArrayBufferData(aResponse, data);
  if (data.IsEmpty()) {
    promise->MaybeRejectWithTypeError(
        "Empty response buffer passed to MediaKeySession.update()");
    EME_LOG("MediaKeySession[%p,'%s'] Update() failed, empty response buffer",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }

  // Convert response to hex for easier logging.
  // Note: UpdateSession() std::move()s the data out of the array, so we have
  // to copy it here.
  nsAutoCString hexResponse(ToHexString(data));

  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->UpdateSession(mSessionId, pid, data);

  EME_LOG(
      "MediaKeySession[%p,'%s'] Update() sent to CDM, "
      "promiseId=%d Response='%s'",
      this, NS_ConvertUTF16toUTF8(mSessionId).get(), pid, hexResponse.get());

  return promise.forget();
}

already_AddRefed<Promise> MediaKeySession::Close(ErrorResult& aRv) {
  RefPtr<DetailedPromise> promise(MakePromise(aRv, "MediaKeySession.close"_ns));
  if (aRv.Failed()) {
    return nullptr;
  }
  // 1. Let session be the associated MediaKeySession object.
  // 2. If session is closed, return a resolved promise.
  if (IsClosed()) {
    EME_LOG("MediaKeySession[%p,'%s'] Close() already closed", this,
            NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }
  // 3. If session's callable value is false, return a promise rejected
  // with an InvalidStateError.
  if (!IsCallable()) {
    EME_LOG("MediaKeySession[%p,''] Close() called before sessionId set by CDM",
            this);
    promise->MaybeRejectWithInvalidStateError(
        "MediaKeySession.Close() called before sessionId set by CDM");
    return promise.forget();
  }
  if (!mKeys->GetCDMProxy()) {
    EME_LOG("MediaKeySession[%p,'%s'] Close() null CDMProxy", this,
            NS_ConvertUTF16toUTF8(mSessionId).get());
    promise->MaybeRejectWithInvalidStateError(
        "MediaKeySession.Close() lost reference to CDM");
    return promise.forget();
  }
  // 4. Let promise be a new promise.
  PromiseId pid = mKeys->StorePromise(promise);
  // 5. Run the following steps in parallel:
  // 5.1 Let cdm be the CDM instance represented by session's cdm instance
  // value. 5.2 Use cdm to close the session associated with session.
  mKeys->GetCDMProxy()->CloseSession(mSessionId, pid);

  EME_LOG("MediaKeySession[%p,'%s'] Close() sent to CDM, promiseId=%d", this,
          NS_ConvertUTF16toUTF8(mSessionId).get(), pid);

  // Session Closed algorithm is run when CDM causes us to run
  // OnSessionClosed().

  // 6. Return promise.
  return promise.forget();
}

void MediaKeySession::OnClosed() {
  if (IsClosed()) {
    return;
  }
  EME_LOG("MediaKeySession[%p,'%s'] session close operation complete.", this,
          NS_ConvertUTF16toUTF8(mSessionId).get());
  mIsClosed = true;
  mKeys->OnSessionClosed(this);
  mKeys = nullptr;
  mClosed->MaybeResolveWithUndefined();
}

bool MediaKeySession::IsClosed() const { return mIsClosed; }

already_AddRefed<Promise> MediaKeySession::Remove(ErrorResult& aRv) {
  RefPtr<DetailedPromise> promise(
      MakePromise(aRv, "MediaKeySession.remove"_ns));
  if (aRv.Failed()) {
    return nullptr;
  }
  if (!IsCallable()) {
    // If this object's callable value is false, return a promise rejected
    // with a new DOMException whose name is InvalidStateError.
    EME_LOG(
        "MediaKeySession[%p,''] Remove() called before sessionId set by CDM",
        this);
    promise->MaybeRejectWithInvalidStateError(
        "MediaKeySession.Remove() called before sessionId set by CDM");
    return promise.forget();
  }
  if (mSessionType != MediaKeySessionType::Persistent_license) {
    promise->MaybeRejectWithInvalidAccessError(
        "Calling MediaKeySession.remove() on non-persistent session");
    // "The operation is not supported on session type sessions."
    EME_LOG("MediaKeySession[%p,'%s'] Remove() failed, sesion not persisrtent.",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }
  if (IsClosed() || !mKeys->GetCDMProxy()) {
    promise->MaybeRejectWithInvalidStateError(
        "MediaKeySession.remove() called but session is not active");
    // "The session is closed."
    EME_LOG("MediaKeySession[%p,'%s'] Remove() failed, already session closed.",
            this, NS_ConvertUTF16toUTF8(mSessionId).get());
    return promise.forget();
  }
  PromiseId pid = mKeys->StorePromise(promise);
  mKeys->GetCDMProxy()->RemoveSession(mSessionId, pid);
  EME_LOG("MediaKeySession[%p,'%s'] Remove() sent to CDM, promiseId=%d.", this,
          NS_ConvertUTF16toUTF8(mSessionId).get(), pid);

  return promise.forget();
}

void MediaKeySession::DispatchKeyMessage(MediaKeyMessageType aMessageType,
                                         const nsTArray<uint8_t>& aMessage) {
  if (EME_LOG_ENABLED()) {
    EME_LOG(
        "MediaKeySession[%p,'%s'] DispatchKeyMessage() type=%s message='%s'",
        this, NS_ConvertUTF16toUTF8(mSessionId).get(),
        nsCString(MediaKeyMessageTypeValues::GetString(aMessageType)).get(),
        ToHexString(aMessage).get());
  }

  RefPtr<MediaKeyMessageEvent> event(
      MediaKeyMessageEvent::Constructor(this, aMessageType, aMessage));
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, event.forget());
  asyncDispatcher->PostDOMEvent();
}

void MediaKeySession::DispatchKeyError(uint32_t aSystemCode) {
  EME_LOG("MediaKeySession[%p,'%s'] DispatchKeyError() systemCode=%u.", this,
          NS_ConvertUTF16toUTF8(mSessionId).get(), aSystemCode);

  auto event = MakeRefPtr<MediaKeyError>(this, aSystemCode);
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, event.forget());
  asyncDispatcher->PostDOMEvent();
}

void MediaKeySession::DispatchKeyStatusesChange() {
  if (IsClosed()) {
    return;
  }

  UpdateKeyStatusMap();

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, u"keystatuseschange"_ns, CanBubble::eNo);
  asyncDispatcher->PostDOMEvent();
}

uint32_t MediaKeySession::Token() const { return mToken; }

already_AddRefed<DetailedPromise> MediaKeySession::MakePromise(
    ErrorResult& aRv, const nsACString& aName) {
  nsCOMPtr<nsIGlobalObject> global = GetParentObject();
  if (!global) {
    NS_WARNING("Passed non-global to MediaKeys ctor!");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  return DetailedPromise::Create(global, aRv, aName);
}

void MediaKeySession::SetExpiration(double aExpiration) {
  EME_LOG("MediaKeySession[%p,'%s'] SetExpiry(%.12lf) (%.2lf hours from now)",
          this, NS_ConvertUTF16toUTF8(mSessionId).get(), aExpiration,
          (aExpiration - 1000.0 * double(time(0))) / (1000.0 * 60 * 60));
  mExpiration = aExpiration;
}

EventHandlerNonNull* MediaKeySession::GetOnkeystatuseschange() {
  return GetEventHandler(nsGkAtoms::onkeystatuseschange);
}

void MediaKeySession::SetOnkeystatuseschange(EventHandlerNonNull* aCallback) {
  SetEventHandler(nsGkAtoms::onkeystatuseschange, aCallback);
}

EventHandlerNonNull* MediaKeySession::GetOnmessage() {
  return GetEventHandler(nsGkAtoms::onmessage);
}

void MediaKeySession::SetOnmessage(EventHandlerNonNull* aCallback) {
  SetEventHandler(nsGkAtoms::onmessage, aCallback);
}

nsCString ToCString(MediaKeySessionType aType) {
  return nsCString(MediaKeySessionTypeValues::GetString(aType));
}

nsString ToString(MediaKeySessionType aType) {
  return NS_ConvertUTF8toUTF16(ToCString(aType));
}

}  // namespace mozilla::dom
