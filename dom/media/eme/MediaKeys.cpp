/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaKeys.h"
#include "GMPCrashHelper.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeyError.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/MediaKeyStatusMap.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/Telemetry.h"
#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/MediaDrmCDMProxy.h"
#endif
#include "mozilla/EMEUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsContentTypeParser.h"
#ifdef XP_WIN
#  include "mozilla/WindowsVersion.h"
#endif
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/MediaKeySystemAccess.h"
#include "nsPrintfCString.h"
#include "ChromiumCDMProxy.h"

namespace mozilla {

namespace dom {

// We don't use NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE because we need to
// unregister our MediaKeys from mDocument's activity listeners. If we don't do
// this then cycle collection can null mDocument before our dtor runs and the
// observer ptr held by mDocument will dangle.
NS_IMPL_CYCLE_COLLECTION_CLASS(MediaKeys)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaKeys)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mKeySessions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromises)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingSessions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(MediaKeys)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaKeys)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mKeySessions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromises)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingSessions)
  tmp->UnregisterActivityObserver();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeys)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeys)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeys)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
NS_INTERFACE_MAP_END

MediaKeys::MediaKeys(nsPIDOMWindowInner* aParent, const nsAString& aKeySystem,
                     const MediaKeySystemConfiguration& aConfig)
    : mParent(aParent),
      mKeySystem(aKeySystem),
      mCreatePromiseId(0),
      mConfig(aConfig) {
  EME_LOG("MediaKeys[%p] constructed keySystem=%s", this,
          NS_ConvertUTF16toUTF8(mKeySystem).get());
}

MediaKeys::~MediaKeys() {
  UnregisterActivityObserver();
  mDocument = nullptr;
  Shutdown();
  EME_LOG("MediaKeys[%p] destroyed", this);
}

void MediaKeys::RegisterActivityObserver() {
  MOZ_ASSERT(mDocument);
  if (mDocument) {
    mDocument->RegisterActivityObserver(this);
  }
}

void MediaKeys::UnregisterActivityObserver() {
  if (mDocument) {
    mDocument->UnregisterActivityObserver(this);
  }
}

// NS_DECL_NSIDOCUMENTACTIVITY
void MediaKeys::NotifyOwnerDocumentActivityChanged() {
  EME_LOG("MediaKeys[%p] NotifyOwnerDocumentActivityChanged()", this);
  // If our owning document is no longer active we should shutdown.
  if (!mDocument->IsCurrentActiveDocument()) {
    EME_LOG(
        "MediaKeys[%p] NotifyOwnerDocumentActivityChanged() owning document is "
        "not active, shutting down!",
        this);
    Shutdown();
  }
}

void MediaKeys::Terminated() {
  EME_LOG("MediaKeys[%p] CDM crashed unexpectedly", this);

  KeySessionHashMap keySessions;
  // Remove entries during iteration will screw it. Make a copy first.
  for (auto iter = mKeySessions.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<MediaKeySession>& session = iter.Data();
    keySessions.Put(session->GetSessionId(), session);
  }
  for (auto iter = keySessions.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<MediaKeySession>& session = iter.Data();
    session->OnClosed();
  }
  keySessions.Clear();
  MOZ_ASSERT(mKeySessions.Count() == 0);

  // Notify the element about that CDM has terminated.
  if (mElement) {
    mElement->DecodeError(NS_ERROR_DOM_MEDIA_CDM_ERR);
  }

  Shutdown();
}

void MediaKeys::Shutdown() {
  EME_LOG("MediaKeys[%p]::Shutdown()", this);
  if (mProxy) {
    mProxy->Shutdown();
    mProxy = nullptr;
  }

  RefPtr<MediaKeys> kungFuDeathGrip = this;

  for (auto iter = mPromises.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<dom::DetailedPromise>& promise = iter.Data();
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Promise still outstanding at MediaKeys shutdown"));
    Release();
  }
  mPromises.Clear();
}

nsPIDOMWindowInner* MediaKeys::GetParentObject() const { return mParent; }

JSObject* MediaKeys::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return MediaKeys_Binding::Wrap(aCx, this, aGivenProto);
}

void MediaKeys::GetKeySystem(nsString& aOutKeySystem) const {
  aOutKeySystem.Assign(mKeySystem);
}

already_AddRefed<DetailedPromise> MediaKeys::SetServerCertificate(
    const ArrayBufferViewOrArrayBuffer& aCert, ErrorResult& aRv) {
  RefPtr<DetailedPromise> promise(
      MakePromise(aRv, NS_LITERAL_CSTRING("MediaKeys.setServerCertificate")));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mProxy) {
    NS_WARNING("Tried to use a MediaKeys without a CDM");
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Null CDM in MediaKeys.setServerCertificate()"));
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  CopyArrayBufferViewOrArrayBufferData(aCert, data);
  if (data.IsEmpty()) {
    promise->MaybeReject(
        NS_ERROR_DOM_TYPE_ERR,
        NS_LITERAL_CSTRING(
            "Empty certificate passed to MediaKeys.setServerCertificate()"));
    return promise.forget();
  }

  mProxy->SetServerCertificate(StorePromise(promise), data);
  return promise.forget();
}

already_AddRefed<DetailedPromise> MediaKeys::MakePromise(
    ErrorResult& aRv, const nsACString& aName) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    NS_WARNING("Passed non-global to MediaKeys ctor!");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  return DetailedPromise::Create(global, aRv, aName);
}

PromiseId MediaKeys::StorePromise(DetailedPromise* aPromise) {
  static uint32_t sEMEPromiseCount = 1;
  MOZ_ASSERT(aPromise);
  uint32_t id = sEMEPromiseCount++;

  EME_LOG("MediaKeys[%p]::StorePromise() id=%" PRIu32, this, id);

  // Keep MediaKeys alive for the lifetime of its promises. Any still-pending
  // promises are rejected in Shutdown().
  EME_LOG("MediaKeys[%p]::StorePromise() calling AddRef()", this);
  AddRef();

#ifdef DEBUG
  // We should not have already stored this promise!
  for (auto iter = mPromises.ConstIter(); !iter.Done(); iter.Next()) {
    MOZ_ASSERT(iter.Data() != aPromise);
  }
#endif

  mPromises.Put(id, aPromise);
  return id;
}

void MediaKeys::ConnectPendingPromiseIdWithToken(PromiseId aId,
                                                 uint32_t aToken) {
  // Should only be called from MediaKeySession::GenerateRequest.
  mPromiseIdToken.Put(aId, aToken);
  EME_LOG(
      "MediaKeys[%p]::ConnectPendingPromiseIdWithToken() id=%u => token(%u)",
      this, aId, aToken);
}

already_AddRefed<DetailedPromise> MediaKeys::RetrievePromise(PromiseId aId) {
  EME_LOG("MediaKeys[%p]::RetrievePromise(aId=%" PRIu32 ")", this, aId);
  if (!mPromises.Contains(aId)) {
    EME_LOG("MediaKeys[%p]::RetrievePromise(aId=%" PRIu32
            ") tried to retrieve non-existent promise!",
            this, aId);
    NS_WARNING(nsPrintfCString(
                   "Tried to retrieve a non-existent promise id=%" PRIu32, aId)
                   .get());
    return nullptr;
  }
  RefPtr<DetailedPromise> promise;
  mPromises.Remove(aId, getter_AddRefs(promise));
  EME_LOG("MediaKeys[%p]::RetrievePromise(aId=%" PRIu32 ") calling Release()",
          this, aId);
  Release();
  return promise.forget();
}

void MediaKeys::RejectPromise(PromiseId aId, nsresult aExceptionCode,
                              const nsCString& aReason) {
  EME_LOG("MediaKeys[%p]::RejectPromise(%" PRIu32 ", 0x%" PRIx32 ")", this, aId,
          static_cast<uint32_t>(aExceptionCode));

  RefPtr<DetailedPromise> promise(RetrievePromise(aId));
  if (!promise) {
    EME_LOG("MediaKeys[%p]::RejectPromise(%" PRIu32 ", 0x%" PRIx32
            ") couldn't retrieve promise! Bailing!",
            this, aId, static_cast<uint32_t>(aExceptionCode));
    return;
  }

  // This promise could be a createSession or loadSession promise,
  // so we might have a pending session waiting to be resolved into
  // the promise on success. We've been directed to reject to promise,
  // so we can throw away the corresponding session object.
  uint32_t token = 0;
  if (mPromiseIdToken.Get(aId, &token)) {
    MOZ_ASSERT(mPendingSessions.Contains(token));
    mPendingSessions.Remove(token);
    mPromiseIdToken.Remove(aId);
  }

  MOZ_ASSERT(NS_FAILED(aExceptionCode));
  promise->MaybeReject(aExceptionCode, aReason);

  if (mCreatePromiseId == aId) {
    // Note: This will probably destroy the MediaKeys object!
    EME_LOG("MediaKeys[%p]::RejectPromise(%" PRIu32 ", 0x%" PRIx32
            ") calling Release()",
            this, aId, static_cast<uint32_t>(aExceptionCode));
    Release();
  }
}

void MediaKeys::OnSessionIdReady(MediaKeySession* aSession) {
  if (!aSession) {
    NS_WARNING("Invalid MediaKeySession passed to OnSessionIdReady()");
    return;
  }
  if (mKeySessions.Contains(aSession->GetSessionId())) {
    NS_WARNING("MediaKeySession's made ready multiple times!");
    return;
  }
  if (mPendingSessions.Contains(aSession->Token())) {
    NS_WARNING(
        "MediaKeySession made ready when it wasn't waiting to be ready!");
    return;
  }
  if (aSession->GetSessionId().IsEmpty()) {
    NS_WARNING(
        "MediaKeySession with invalid sessionId passed to OnSessionIdReady()");
    return;
  }
  mKeySessions.Put(aSession->GetSessionId(), aSession);
}

void MediaKeys::ResolvePromise(PromiseId aId) {
  EME_LOG("MediaKeys[%p]::ResolvePromise(%" PRIu32 ")", this, aId);

  RefPtr<DetailedPromise> promise(RetrievePromise(aId));
  MOZ_ASSERT(!mPromises.Contains(aId));
  if (!promise) {
    return;
  }

  uint32_t token = 0;
  if (!mPromiseIdToken.Get(aId, &token)) {
    promise->MaybeResolveWithUndefined();
    return;
  } else if (!mPendingSessions.Contains(token)) {
    // Pending session for CreateSession() should be removed when sessionId
    // is ready.
    promise->MaybeResolveWithUndefined();
    mPromiseIdToken.Remove(aId);
    return;
  }
  mPromiseIdToken.Remove(aId);

  // We should only resolve LoadSession calls via this path,
  // not CreateSession() promises.
  RefPtr<MediaKeySession> session;
  mPendingSessions.Remove(token, getter_AddRefs(session));
  if (!session || session->GetSessionId().IsEmpty()) {
    NS_WARNING("Received activation for non-existent session!");
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_ACCESS_ERR,
        NS_LITERAL_CSTRING("CDM LoadSession() returned a different session ID "
                           "than requested"));
    return;
  }
  mKeySessions.Put(session->GetSessionId(), session);
  promise->MaybeResolve(session);
}

class MediaKeysGMPCrashHelper : public GMPCrashHelper {
 public:
  explicit MediaKeysGMPCrashHelper(MediaKeys* aMediaKeys)
      : mMediaKeys(aMediaKeys) {
    MOZ_ASSERT(NS_IsMainThread());  // WeakPtr isn't thread safe.
  }
  already_AddRefed<nsPIDOMWindowInner> GetPluginCrashedEventTarget() override {
    MOZ_ASSERT(NS_IsMainThread());  // WeakPtr isn't thread safe.
    EME_LOG("MediaKeysGMPCrashHelper::GetPluginCrashedEventTarget()");
    return (mMediaKeys && mMediaKeys->GetParentObject())
               ? do_AddRef(mMediaKeys->GetParentObject())
               : nullptr;
  }

 private:
  WeakPtr<MediaKeys> mMediaKeys;
};

already_AddRefed<CDMProxy> MediaKeys::CreateCDMProxy(
    nsISerialEventTarget* aMainThread) {
  EME_LOG("MediaKeys[%p]::CreateCDMProxy()", this);
  RefPtr<CDMProxy> proxy;
#ifdef MOZ_WIDGET_ANDROID
  if (IsWidevineKeySystem(mKeySystem)) {
    proxy = new MediaDrmCDMProxy(
        this, mKeySystem,
        mConfig.mDistinctiveIdentifier == MediaKeysRequirement::Required,
        mConfig.mPersistentState == MediaKeysRequirement::Required,
        aMainThread);
  } else
#endif
  {
    proxy = new ChromiumCDMProxy(
        this, mKeySystem, new MediaKeysGMPCrashHelper(this),
        mConfig.mDistinctiveIdentifier == MediaKeysRequirement::Required,
        mConfig.mPersistentState == MediaKeysRequirement::Required,
        aMainThread);
  }
  return proxy.forget();
}

already_AddRefed<DetailedPromise> MediaKeys::Init(ErrorResult& aRv) {
  EME_LOG("MediaKeys[%p]::Init()", this);
  RefPtr<DetailedPromise> promise(
      MakePromise(aRv, NS_LITERAL_CSTRING("MediaKeys::Init()")));
  if (aRv.Failed()) {
    return nullptr;
  }

  // Determine principal (at creation time) of the MediaKeys object.
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(GetParentObject());
  if (!sop) {
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Couldn't get script principal in MediaKeys::Init"));
    return promise.forget();
  }
  mPrincipal = sop->GetPrincipal();

  // Determine principal of the "top-level" window; the principal of the
  // page that will display in the URL bar.
  nsCOMPtr<nsPIDOMWindowInner> window = GetParentObject();
  if (!window) {
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Couldn't get top-level window in MediaKeys::Init"));
    return promise.forget();
  }
  nsCOMPtr<nsPIDOMWindowOuter> top =
      window->GetOuterWindow()->GetInProcessTop();
  if (!top || !top->GetExtantDoc()) {
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Couldn't get document in MediaKeys::Init"));
    return promise.forget();
  }

  mDocument = top->GetExtantDoc();

  mTopLevelPrincipal = mDocument->NodePrincipal();

  if (!mPrincipal || !mTopLevelPrincipal) {
    NS_WARNING("Failed to get principals when creating MediaKeys");
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Couldn't get principal(s) in MediaKeys::Init"));
    return promise.forget();
  }

  nsAutoCString origin;
  nsresult rv = mPrincipal->GetOrigin(origin);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING(
            "Couldn't get principal origin string in MediaKeys::Init"));
    return promise.forget();
  }
  nsAutoCString topLevelOrigin;
  rv = mTopLevelPrincipal->GetOrigin(topLevelOrigin);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Couldn't get top-level principal origin string in "
                           "MediaKeys::Init"));
    return promise.forget();
  }

  EME_LOG("MediaKeys[%p]::Create() (%s, %s)", this, origin.get(),
          topLevelOrigin.get());

  mProxy =
      CreateCDMProxy(top->GetExtantDoc()->EventTargetFor(TaskCategory::Other));

  // The CDMProxy's initialization is asynchronous. The MediaKeys is
  // refcounted, and its instance is returned to JS by promise once
  // it's been initialized. No external refs exist to the MediaKeys while
  // we're waiting for the promise to be resolved, so we must hold a
  // reference to the new MediaKeys object until it's been created,
  // or its creation has failed. Store the id of the promise returned
  // here, and hold a self-reference until that promise is resolved or
  // rejected.
  MOZ_ASSERT(!mCreatePromiseId, "Should only be created once!");
  mCreatePromiseId = StorePromise(promise);
  EME_LOG("MediaKeys[%p]::Init() calling AddRef()", this);
  AddRef();
  mProxy->Init(mCreatePromiseId, NS_ConvertUTF8toUTF16(origin),
               NS_ConvertUTF8toUTF16(topLevelOrigin),
               KeySystemToGMPName(mKeySystem));

  RegisterActivityObserver();

  return promise.forget();
}

void MediaKeys::OnCDMCreated(PromiseId aId, const uint32_t aPluginId) {
  EME_LOG("MediaKeys[%p]::OnCDMCreated(aId=%" PRIu32 ", aPluginId=%" PRIu32 ")",
          this, aId, aPluginId);
  RefPtr<DetailedPromise> promise(RetrievePromise(aId));
  if (!promise) {
    return;
  }
  RefPtr<MediaKeys> keys(this);

  promise->MaybeResolve(keys);
  if (mCreatePromiseId == aId) {
    EME_LOG("MediaKeys[%p]::OnCDMCreated(aId=%" PRIu32 ", aPluginId=%" PRIu32
            ") calling Release()",
            this, aId, aPluginId);
    Release();
  }

  MediaKeySystemAccess::NotifyObservers(mParent, mKeySystem,
                                        MediaKeySystemStatus::Cdm_created);
}

static bool IsSessionTypeSupported(const MediaKeySessionType aSessionType,
                                   const MediaKeySystemConfiguration& aConfig) {
  if (aSessionType == MediaKeySessionType::Temporary) {
    // Temporary is always supported.
    return true;
  }
  if (!aConfig.mSessionTypes.WasPassed()) {
    // No other session types supported.
    return false;
  }
  return aConfig.mSessionTypes.Value().Contains(ToString(aSessionType));
}

already_AddRefed<MediaKeySession> MediaKeys::CreateSession(
    JSContext* aCx, MediaKeySessionType aSessionType, ErrorResult& aRv) {
  EME_LOG("MediaKeys[%p]::CreateSession(aCx=%p, aSessionType=%" PRIu8 ")", this,
          aCx, static_cast<uint8_t>(aSessionType));
  if (!IsSessionTypeSupported(aSessionType, mConfig)) {
    EME_LOG("MediaKeys[%p]::CreateSession() failed, unsupported session type",
            this);
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (!mProxy) {
    NS_WARNING("Tried to use a MediaKeys which lost its CDM");
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  EME_LOG("MediaKeys[%p] Creating session", this);

  RefPtr<MediaKeySession> session = new MediaKeySession(
      aCx, GetParentObject(), this, mKeySystem, aSessionType, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }
  DDLINKCHILD("session", session.get());

  // Add session to the set of sessions awaiting their sessionId being ready.
  EME_LOG("MediaKeys[%p]::CreateSession(aCx=%p, aSessionType=%" PRIu8
          ") putting session with token=%" PRIu32 " into mPendingSessions",
          this, aCx, static_cast<uint8_t>(aSessionType), session->Token());
  mPendingSessions.Put(session->Token(), session);

  return session.forget();
}

void MediaKeys::OnSessionLoaded(PromiseId aId, bool aSuccess) {
  EME_LOG("MediaKeys[%p]::OnSessionLoaded() resolve promise id=%" PRIu32, this,
          aId);

  ResolvePromiseWithResult(aId, aSuccess);
}

template <typename T>
void MediaKeys::ResolvePromiseWithResult(PromiseId aId, const T& aResult) {
  RefPtr<DetailedPromise> promise(RetrievePromise(aId));
  if (!promise) {
    return;
  }

  promise->MaybeResolve(aResult);
}

void MediaKeys::OnSessionClosed(MediaKeySession* aSession) {
  nsAutoString id;
  aSession->GetSessionId(id);
  mKeySessions.Remove(id);
}

already_AddRefed<MediaKeySession> MediaKeys::GetSession(
    const nsAString& aSessionId) {
  RefPtr<MediaKeySession> session;
  mKeySessions.Get(aSessionId, getter_AddRefs(session));
  return session.forget();
}

already_AddRefed<MediaKeySession> MediaKeys::GetPendingSession(
    uint32_t aToken) {
  EME_LOG("MediaKeys[%p]::GetPendingSession(aToken=%" PRIu32 ")", this, aToken);
  RefPtr<MediaKeySession> session;
  mPendingSessions.Get(aToken, getter_AddRefs(session));
  mPendingSessions.Remove(aToken);
  return session.forget();
}

bool MediaKeys::IsBoundToMediaElement() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mElement != nullptr;
}

nsresult MediaKeys::Bind(HTMLMediaElement* aElement) {
  MOZ_ASSERT(NS_IsMainThread());
  if (IsBoundToMediaElement()) {
    return NS_ERROR_FAILURE;
  }

  mElement = aElement;

  return NS_OK;
}

void MediaKeys::Unbind() {
  MOZ_ASSERT(NS_IsMainThread());
  mElement = nullptr;
}

void MediaKeys::GetSessionsInfo(nsString& sessionsInfo) {
  for (KeySessionHashMap::Iterator it = mKeySessions.Iter(); !it.Done();
       it.Next()) {
    MediaKeySession* keySession = it.Data();
    nsString sessionID;
    keySession->GetSessionId(sessionID);
    sessionsInfo.AppendLiteral("(sid=");
    sessionsInfo.Append(sessionID);
    MediaKeyStatusMap* keyStatusMap = keySession->KeyStatuses();
    for (uint32_t i = 0; i < keyStatusMap->GetIterableLength(); i++) {
      nsString keyID = keyStatusMap->GetKeyIDAsHexString(i);
      sessionsInfo.AppendLiteral("(kid=");
      sessionsInfo.Append(keyID);
      using IntegerType = typename std::underlying_type<MediaKeyStatus>::type;
      auto idx = static_cast<IntegerType>(keyStatusMap->GetValueAtIndex(i));
      const char* keyStatus = MediaKeyStatusValues::strings[idx].value;
      sessionsInfo.AppendLiteral(" status=");
      sessionsInfo.Append(
          NS_ConvertUTF8toUTF16((nsDependentCString(keyStatus))));
      sessionsInfo.AppendLiteral(")");
    }
    sessionsInfo.AppendLiteral(")");
  }
}

already_AddRefed<Promise> MediaKeys::GetStatusForPolicy(
    const MediaKeysPolicy& aPolicy, ErrorResult& aRv) {
  RefPtr<DetailedPromise> promise(
      MakePromise(aRv, NS_LITERAL_CSTRING("MediaKeys::GetStatusForPolicy()")));
  if (aRv.Failed()) {
    return nullptr;
  }

  // Currently, only widevine CDM supports for this API.
  if (!IsWidevineKeySystem(mKeySystem)) {
    EME_LOG(
        "MediaKeys[%p]::GetStatusForPolicy() HDCP policy check on unsupported "
        "keysystem ",
        this);
    NS_WARNING("Tried to query without a CDM");
    promise->MaybeReject(
        NS_ERROR_DOM_NOT_SUPPORTED_ERR,
        NS_LITERAL_CSTRING("HDCP policy check on unsupported keysystem"));
    return promise.forget();
  }

  if (!mProxy) {
    NS_WARNING("Tried to use a MediaKeys without a CDM");
    promise->MaybeReject(
        NS_ERROR_DOM_INVALID_STATE_ERR,
        NS_LITERAL_CSTRING("Null CDM in MediaKeys.GetStatusForPolicy()"));
    return promise.forget();
  }

  EME_LOG("GetStatusForPolicy minHdcpVersion = %s.",
          NS_ConvertUTF16toUTF8(aPolicy.mMinHdcpVersion).get());
  mProxy->GetStatusForPolicy(StorePromise(promise), aPolicy.mMinHdcpVersion);
  return promise.forget();
}

void MediaKeys::ResolvePromiseWithKeyStatus(PromiseId aId,
                                            MediaKeyStatus aMediaKeyStatus) {
  RefPtr<DetailedPromise> promise(RetrievePromise(aId));
  if (!promise) {
    return;
  }
  RefPtr<MediaKeys> keys(this);
  EME_LOG(
      "MediaKeys[%p]::ResolvePromiseWithKeyStatus() resolve promise id=%" PRIu32
      ", keystatus=%" PRIu8,
      this, aId, static_cast<uint8_t>(aMediaKeyStatus));
  promise->MaybeResolve(aMediaKeyStatus);
}

}  // namespace dom
}  // namespace mozilla
