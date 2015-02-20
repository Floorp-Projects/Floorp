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
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/EMEUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptObjectPrincipal.h"
#include "mozilla/Preferences.h"
#include "nsContentTypeParser.h"
#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#endif
#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#endif
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/dom/MediaKeySystemAccess.h"

namespace mozilla {

namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaKeys,
                                      mElement,
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
  ((MediaKeys*)aClosure)->Release();
  return PL_DHASH_NEXT;
}

MediaKeys::~MediaKeys()
{
  Shutdown();
}

static PLDHashOperator
CopySessions(const nsAString& aKey,
             nsRefPtr<MediaKeySession>& aSession,
             void* aClosure)
{
  KeySessionHashMap* p = static_cast<KeySessionHashMap*>(aClosure);
  p->Put(aSession->GetSessionId(), aSession);
  return PL_DHASH_NEXT;
}

static PLDHashOperator
CloseSessions(const nsAString& aKey,
              nsRefPtr<MediaKeySession>& aSession,
              void* aClosure)
{
  aSession->OnClosed();
  return PL_DHASH_NEXT;
}

void
MediaKeys::Terminated()
{
  KeySessionHashMap keySessions;
  // Remove entries during iteration will screw it. Make a copy first.
  mKeySessions.Enumerate(&CopySessions, &keySessions);
  keySessions.Enumerate(&CloseSessions, nullptr);
  keySessions.Clear();
  MOZ_ASSERT(mKeySessions.Count() == 0);

  // Notify the element about that CDM has terminated.
  if (mElement) {
    mElement->DecodeError();
  }

  Shutdown();
}

void
MediaKeys::Shutdown()
{
  if (mProxy) {
    mProxy->Shutdown();
    mProxy = nullptr;
  }

  nsRefPtr<MediaKeys> kungFuDeathGrip = this;

  mPromises.Enumerate(&RejectPromises, this);
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

  // Keep MediaKeys alive for the lifetime of its promises. Any still-pending
  // promises are rejected in Shutdown().
  AddRef();

  mPromises.Put(id, aPromise);
  return id;
}

already_AddRefed<Promise>
MediaKeys::RetrievePromise(PromiseId aId)
{
  MOZ_ASSERT(mPromises.Contains(aId));
  nsRefPtr<Promise> promise;
  mPromises.Remove(aId, getter_AddRefs(promise));
  Release();
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
MediaKeys::OnSessionIdReady(MediaKeySession* aSession)
{
  if (!aSession) {
    NS_WARNING("Invalid MediaKeySession passed to OnSessionIdReady()");
    return;
  }
  if (mKeySessions.Contains(aSession->GetSessionId())) {
    NS_WARNING("MediaKeySession's made ready multiple times!");
    return;
  }
  if (mPendingSessions.Contains(aSession->Token())) {
    NS_WARNING("MediaKeySession made ready when it wasn't waiting to be ready!");
    return;
  }
  if (aSession->GetSessionId().IsEmpty()) {
    NS_WARNING("MediaKeySession with invalid sessionId passed to OnSessionIdReady()");
    return;
  }
  mKeySessions.Put(aSession->GetSessionId(), aSession);
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

already_AddRefed<Promise>
MediaKeys::Init(ErrorResult& aRv)
{
  nsRefPtr<Promise> promise(MakePromise(aRv));
  if (aRv.Failed()) {
    return nullptr;
  }

  mProxy = new CDMProxy(this, mKeySystem);

  // Determine principal (at creation time) of the MediaKeys object.
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(GetParentObject());
  if (!sop) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }
  mPrincipal = sop->GetPrincipal();

  // Determine principal of the "top-level" window; the principal of the
  // page that will display in the URL bar.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(GetParentObject());
  if (!window) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }
  nsCOMPtr<nsIDOMWindow> topWindow;
  window->GetTop(getter_AddRefs(topWindow));
  nsCOMPtr<nsPIDOMWindow> top = do_QueryInterface(topWindow);
  if (!top || !top->GetExtantDoc()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  mTopLevelPrincipal = top->GetExtantDoc()->NodePrincipal();

  if (!mPrincipal || !mTopLevelPrincipal) {
    NS_WARNING("Failed to get principals when creating MediaKeys");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(mPrincipal, origin);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }
  nsAutoString topLevelOrigin;
  rv = nsContentUtils::GetUTFOrigin(mTopLevelPrincipal, topLevelOrigin);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  if (!window) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }
  nsIDocument* doc = window->GetExtantDoc();
  const bool inPrivateBrowsing = nsContentUtils::IsInPrivateBrowsing(doc);

  EME_LOG("MediaKeys::Create() (%s, %s), %s",
          NS_ConvertUTF16toUTF8(origin).get(),
          NS_ConvertUTF16toUTF8(topLevelOrigin).get(),
          (inPrivateBrowsing ? "PrivateBrowsing" : "NonPrivateBrowsing"));

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
  AddRef();
  mProxy->Init(mCreatePromiseId,
               origin,
               topLevelOrigin,
               inPrivateBrowsing);

  return promise.forget();
}

void
MediaKeys::OnCDMCreated(PromiseId aId, const nsACString& aNodeId)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
  mNodeId = aNodeId;
  nsRefPtr<MediaKeys> keys(this);
  promise->MaybeResolve(keys);
  if (mCreatePromiseId == aId) {
    Release();
  }

  MediaKeySystemAccess::NotifyObservers(mParent,
                                        mKeySystem,
                                        MediaKeySystemStatus::Cdm_created);
}

already_AddRefed<MediaKeySession>
MediaKeys::CreateSession(JSContext* aCx,
                         SessionType aSessionType,
                         ErrorResult& aRv)
{
  nsRefPtr<MediaKeySession> session = new MediaKeySession(aCx,
                                                          GetParentObject(),
                                                          this,
                                                          mKeySystem,
                                                          aSessionType,
                                                          aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  // Add session to the set of sessions awaiting their sessionId being ready.
  mPendingSessions.Put(session->Token(), session);

  return session.forget();
}

void
MediaKeys::OnSessionLoaded(PromiseId aId, bool aSuccess)
{
  nsRefPtr<Promise> promise(RetrievePromise(aId));
  if (!promise) {
    NS_WARNING("MediaKeys tried to resolve a non-existent promise");
    return;
  }
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

already_AddRefed<MediaKeySession>
MediaKeys::GetPendingSession(uint32_t aToken)
{
  nsRefPtr<MediaKeySession> session;
  mPendingSessions.Get(aToken, getter_AddRefs(session));
  mPendingSessions.Remove(aToken);
  return session.forget();
}

const nsCString&
MediaKeys::GetNodeId() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mNodeId;
}

bool
MediaKeys::IsBoundToMediaElement() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mElement != nullptr;
}

nsresult
MediaKeys::Bind(HTMLMediaElement* aElement)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (IsBoundToMediaElement()) {
    return NS_ERROR_FAILURE;
  }

  mElement = aElement;

  return NS_OK;
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
