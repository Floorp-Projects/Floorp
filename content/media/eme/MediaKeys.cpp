/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeyError.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/EMELog.h"
#include "nsContentUtils.h"
#include "nsIScriptObjectPrincipal.h"
#include "mozilla/Preferences.h"
#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#endif

namespace mozilla {

namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaKeys,
                                      mParent,
                                      mKeySessions,
                                      mPromises,
                                      mPendingSessions);
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaKeys)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaKeys)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaKeys)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MediaKeys::MediaKeys(nsPIDOMWindow* aParent, const nsAString& aKeySystem)
  : mParent(aParent)
  , mKeySystem(aKeySystem)
  , mCreatePromiseId(0)
{
}

static PLDHashOperator
RejectPromises(const uint32_t& aKey,
               nsRefPtr<dom::Promise>& aPromise,
               void* aClosure)
{
  aPromise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  return PL_DHASH_NEXT;
}

MediaKeys::~MediaKeys()
{
  Shutdown();
}

void
MediaKeys::Shutdown()
{
  if (mProxy) {
    mProxy->Shutdown();
    mProxy = nullptr;
  }

  mPromises.Enumerate(&RejectPromises, nullptr);
  mPromises.Clear();
}

nsPIDOMWindow*
MediaKeys::GetParentObject() const
{
  return mParent;
}

JSObject*
MediaKeys::WrapObject(JSContext* aCx)
{
  return MediaKeysBinding::Wrap(aCx, this);
}

void
MediaKeys::GetKeySystem(nsString& retval) const
{
  retval = mKeySystem;
}

already_AddRefed<Promise>
MediaKeys::SetServerCertificate(const ArrayBufferViewOrArrayBuffer& aCert, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }

  nsTArray<uint8_t> data;
  if (!CopyArrayBufferViewOrArrayBufferData(aCert, data)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  mProxy->SetServerCertificate(StorePromise(promise), data);
  return promise.forget();
}

static bool
IsSupportedKeySystem(const nsAString& aKeySystem)
{
  return aKeySystem.EqualsASCII("org.w3.clearkey") ||
#ifdef XP_WIN
         (aKeySystem.EqualsASCII("com.adobe.access") &&
          IsVistaOrLater() &&
          Preferences::GetBool("media.eme.adobe-access.enabled", false)) ||
#endif
         false;
}

/* static */
IsTypeSupportedResult
MediaKeys::IsTypeSupported(const GlobalObject& aGlobal,
                           const nsAString& aKeySystem,
                           const Optional<nsAString>& aInitDataType,
                           const Optional<nsAString>& aContentType,
                           const Optional<nsAString>& aCapability)
{
  // TODO: Should really get spec changed to this is async, so we can wait
  //       for user to consent to running plugin.
  return IsSupportedKeySystem(aKeySystem) ? IsTypeSupportedResult::Maybe
                                          : IsTypeSupportedResult::_empty;
}

already_AddRefed<Promise>
MediaKeys::MakePromise(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    NS_WARNING("Passed non-global to MediaKeys ctor!");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  return Promise::Create(global, aRv);
}

PromiseId
MediaKeys::StorePromise(Promise* aPromise)
{
  static uint32_t sEMEPromiseCount = 1;
  MOZ_ASSERT(aPromise);
  uint32_t id = sEMEPromiseCount++;
  mPromises.Put(id, aPromise);
  return id;
}

already_AddRefed<Promise>
MediaKeys::RetrievePromise(PromiseId aId)
{
  MOZ_ASSERT(mPromises.Contains(aId));
  nsRefPtr<Promise> promise;
  mPromises.Remove(aId, getter_AddRefs(promise));
  return promise.forget();
}

void
MediaKeys::RejectPromise(PromiseId aId, nsresult aExceptionCode)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to reject a non-existent promise");
    return;
  }
  if (mPendingSessions.Contains(aId)) {
    // This promise could be a createSession or loadSession promise,
    // so we might have a pending session waiting to be resolved into
    // the promise on success. We've been directed to reject to promise,
    // so we can throw away the corresponding session object.
    mPendingSessions.Remove(aId);
  }

  MOZ_ASSERT(NS_FAILED(aExceptionCode));
  promise->MaybeReject(aExceptionCode);

  if (mCreatePromiseId == aId) {
    // Note: This will probably destroy the MediaKeys object!
    Release();
  }
}

void
MediaKeys::ResolvePromise(PromiseId aId)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
  if (mPendingSessions.Contains(aId)) {
    // We should only resolve LoadSession calls via this path,
    // not CreateSession() promises.
    nsRefPtr<MediaKeySession> session;
    if (!mPendingSessions.Get(aId, getter_AddRefs(session)) ||
        !session ||
        session->GetSessionId().IsEmpty()) {
      NS_WARNING("Received activation for non-existent session!");
      promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      mPendingSessions.Remove(aId);
      return;
    }
    mPendingSessions.Remove(aId);
    mKeySessions.Put(session->GetSessionId(), session);
    promise->MaybeResolve(session);
  } else {
    promise->MaybeResolve(JS::UndefinedHandleValue);
  }
}

/* static */
already_AddRefed<Promise>
MediaKeys::Create(const GlobalObject& aGlobal,
                  const nsAString& aKeySystem,
                  ErrorResult& aRv)
{
  // CDMProxy keeps MediaKeys alive until it resolves the promise and thus
  // returns the MediaKeys object to JS.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<MediaKeys> keys = new MediaKeys(window, aKeySystem);
  nsRefPtr<Promise> promise(keys->MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!IsSupportedKeySystem(aKeySystem)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  keys->mProxy = new CDMProxy(keys, aKeySystem);

  // The CDMProxy's initialization is asynchronous. The MediaKeys is
  // refcounted, and its instance is returned to JS by promise once
  // it's been initialized. No external refs exist to the MediaKeys while
  // we're waiting for the promise to be resolved, so we must hold a
  // reference to the new MediaKeys object until it's been created,
  // or its creation has failed. Store the id of the promise returned
  // here, and hold a self-reference until that promise is resolved or
  // rejected.
  MOZ_ASSERT(!keys->mCreatePromiseId, "Should only be created once!");
  keys->mCreatePromiseId = keys->StorePromise(promise);
  keys->AddRef();
  keys->mProxy->Init(keys->mCreatePromiseId);

  return promise.forget();
}

void
MediaKeys::OnCDMCreated(PromiseId aId)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
  nsRefPtr<MediaKeys> keys(this);
  promise->MaybeResolve(keys);
  if (mCreatePromiseId == aId) {
    Release();
  }
}

already_AddRefed<MediaKeySession>
MediaKeys::CreateSession(SessionType aSessionType,
                         ErrorResult& aRv)
{
  nsRefPtr<MediaKeySession> session = new MediaKeySession(GetParentObject(),
                                                          this,
                                                          mKeySystem,
                                                          aSessionType,
                                                          aRv);

  return session.forget();
}

void
MediaKeys::OnSessionPending(PromiseId aId, MediaKeySession* aSession)
{
  MOZ_ASSERT(mPromises.Contains(aId));
  MOZ_ASSERT(!mPendingSessions.Contains(aId));
  mPendingSessions.Put(aId, aSession);
}

void
MediaKeys::OnSessionCreated(PromiseId aId, const nsAString& aSessionId)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
  MOZ_ASSERT(mPendingSessions.Contains(aId));

  nsRefPtr<MediaKeySession> session;
  bool gotSession = mPendingSessions.Get(aId, getter_AddRefs(session));
  // Session has completed creation/loading, remove it from mPendingSessions,
  // and resolve the promise with it. We store it in mKeySessions, so we can
  // find it again if we need to send messages to it etc.
  mPendingSessions.Remove(aId);
  if (!gotSession || !session) {
    NS_WARNING("Received activation for non-existent session!");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  session->Init(aSessionId);
  mKeySessions.Put(aSessionId, session);
  promise->MaybeResolve(session);
}

void
MediaKeys::OnSessionLoaded(PromiseId aId, bool aSuccess)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
  MOZ_ASSERT(mPendingSessions.Contains(aId));

  nsRefPtr<MediaKeySession> session;
  bool gotSession = mPendingSessions.Get(aId, getter_AddRefs(session));
  // Session has completed creation/loading, remove it from mPendingSessions,
  // and resolve the promise with it. We store it in mKeySessions, so we can
  // find it again if we need to send messages to it etc.
  mPendingSessions.Remove(aId);
  if (!gotSession || !session) {
    NS_WARNING("Received activation for non-existent session!");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  MOZ_ASSERT(!session->GetSessionId().IsEmpty() &&
             !mKeySessions.Contains(session->GetSessionId()));

  mKeySessions.Put(session->GetSessionId(), session);
  promise->MaybeResolve(aSuccess);
}

void
MediaKeys::OnSessionClosed(MediaKeySession* aSession)
{
  nsAutoString id;
  aSession->GetSessionId(id);
  mKeySessions.Remove(id);
}

already_AddRefed<MediaKeySession>
MediaKeys::GetSession(const nsAString& aSessionId)
{
  nsRefPtr<MediaKeySession> session;
  mKeySessions.Get(aSessionId, getter_AddRefs(session));
  return session.forget();
}

const nsCString&
MediaKeys::GetNodeId()
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Bug 1035637, return a combination of origin and URL bar origin.

  if (!mNodeId.IsEmpty()) {
    return mNodeId;
  }

  nsIPrincipal* principal = nullptr;
  nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(GetParentObject());
  nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
    do_QueryInterface(pWindow);
  if (scriptPrincipal) {
    principal = scriptPrincipal->GetPrincipal();
  }
  nsAutoString id;
  if (principal && NS_SUCCEEDED(nsContentUtils::GetUTFOrigin(principal, id))) {
    CopyUTF16toUTF8(id, mNodeId);
    EME_LOG("EME Origin = '%s'", mNodeId.get());
  }

  return mNodeId;
}

bool
CopyArrayBufferViewOrArrayBufferData(const ArrayBufferViewOrArrayBuffer& aBufferOrView,
                                     nsTArray<uint8_t>& aOutData)
{
  if (aBufferOrView.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aBufferOrView.GetAsArrayBuffer();
    buffer.ComputeLengthAndData();
    aOutData.AppendElements(buffer.Data(), buffer.Length());
  } else if (aBufferOrView.IsArrayBufferView()) {
    const ArrayBufferView& bufferview = aBufferOrView.GetAsArrayBufferView();
    bufferview.ComputeLengthAndData();
    aOutData.AppendElements(bufferview.Data(), bufferview.Length());
  } else {
    return false;
  }
  return true;
}

} // namespace dom
} // namespace mozilla
