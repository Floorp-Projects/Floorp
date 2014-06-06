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
#include "nsContentUtils.h"
#include "EMELog.h"

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
  : mParent(aParent),
    mKeySystem(aKeySystem)
{
  SetIsDOMBinding();
}

MediaKeys::~MediaKeys()
{
  if (mProxy) {
    mProxy->Shutdown();
    mProxy = nullptr;
  }
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
MediaKeys::SetServerCertificate(const Uint8Array& aCert)
{
  aCert.ComputeLengthAndData();
  nsRefPtr<Promise> promise(MakePromise());
  mProxy->SetServerCertificate(StorePromise(promise), aCert);
  return promise.forget();
}

/* static */
IsTypeSupportedResult
MediaKeys::IsTypeSupported(const GlobalObject& aGlobal,
                           const nsAString& aKeySystem,
                           const Optional<nsAString>& aInitDataType,
                           const Optional<nsAString>& aContentType,
                           const Optional<nsAString>& aCapability)
{
  // TODO: Query list of known CDMs and their supported content types.
  // TODO: Should really get spec changed to this is async, so we can wait
  //       for user to consent to running plugin.
  return IsTypeSupportedResult::Maybe;
}

already_AddRefed<Promise>
MediaKeys::MakePromise()
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    NS_WARNING("Passed non-global to MediaKeys ctor!");
    return nullptr;
  }
  nsRefPtr<Promise> promise = new Promise(global);
  return promise.forget();
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
}

void
MediaKeys::ResolvePromise(PromiseId aId)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
  // We should not resolve CreateSession or LoadSession calls via this path,
  // OnSessionActivated() should be called instead.
  MOZ_ASSERT(!mPendingSessions.Contains(aId));
  promise->MaybeResolve(JS::UndefinedHandleValue);
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
  nsRefPtr<Promise> promise(keys->MakePromise());
  if (!promise) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!aKeySystem.EqualsASCII("org.w3.clearkey")) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  keys->mProxy = new CDMProxy(keys, aKeySystem);
  keys->mProxy->Init(keys->StorePromise(promise));

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
}

already_AddRefed<Promise>
MediaKeys::LoadSession(const nsAString& aSessionId)
{
  nsRefPtr<Promise> promise(MakePromise());

  if (aSessionId.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    // "The sessionId parameter is empty."
    return promise.forget();
  }

  // TODO: The spec doesn't specify what to do in this case...
  if (mKeySessions.Contains(aSessionId)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  // Create session.
  nsRefPtr<MediaKeySession> session(
    new MediaKeySession(GetParentObject(), this, mKeySystem, SessionType::Persistent));

  // Proxy owns session object until resolving promise.
  mProxy->LoadSession(StorePromise(promise),
                      aSessionId);

  return promise.forget();
}

already_AddRefed<Promise>
MediaKeys::CreateSession(const nsAString& initDataType,
                         const Uint8Array& aInitData,
                         SessionType aSessionType)
{
  aInitData.ComputeLengthAndData();
  nsRefPtr<Promise> promise(MakePromise());
  nsRefPtr<MediaKeySession> session = new MediaKeySession(GetParentObject(),
                                                          this,
                                                          mKeySystem,
                                                          aSessionType);
  auto pid = StorePromise(promise);
  // Hang onto session until the CDM has finished setting it up.
  mPendingSessions.Put(pid, session);
  mProxy->CreateSession(aSessionType,
                        pid,
                        initDataType,
                        aInitData);

  return promise.forget();
}

void
MediaKeys::OnSessionActivated(PromiseId aId, const nsAString& aSessionId)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
  MOZ_ASSERT(mPendingSessions.Contains(aId));

  nsRefPtr<MediaKeySession> session;
  if (!mPendingSessions.Get(aId, getter_AddRefs(session)) || !session) {
    NS_WARNING("Received activation for non-existent session!");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  // Session has completed creation/loading, remove it from mPendingSessions,
  // and resolve the promise with it. We store it in mKeySessions, so we can
  // find it again if we need to send messages to it etc.
  mPendingSessions.Remove(aId);
  session->Init(aSessionId);
  mKeySessions.Put(aSessionId, session);
  promise->MaybeResolve(session);
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

} // namespace dom
} // namespace mozilla
