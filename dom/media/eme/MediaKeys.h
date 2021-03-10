/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediakeys_h__
#define mozilla_dom_mediakeys_h__

#include "DecoderDoctorLogger.h"
#include "mozilla/Attributes.h"
#include "mozilla/DetailedPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h"  // For MediaKeyStatus
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"

namespace mozilla {

class CDMProxy;

namespace dom {
class MediaKeys;
}  // namespace dom
DDLoggedTypeName(dom::MediaKeys);

namespace dom {

class ArrayBufferViewOrArrayBuffer;
class MediaKeySession;
struct MediaKeysPolicy;
class HTMLMediaElement;

typedef nsRefPtrHashtable<nsStringHashKey, MediaKeySession> KeySessionHashMap;
typedef nsRefPtrHashtable<nsUint32HashKey, dom::DetailedPromise> PromiseHashMap;
typedef nsRefPtrHashtable<nsUint32HashKey, MediaKeySession>
    PendingKeySessionsHashMap;
typedef nsTHashMap<nsUint32HashKey, uint32_t> PendingPromiseIdTokenHashMap;
typedef uint32_t PromiseId;

// This class is used on the main thread only.
// Note: its addref/release is not (and can't be) thread safe!
class MediaKeys final : public nsISupports,
                        public nsWrapperCache,
                        public SupportsWeakPtr,
                        public DecoderDoctorLifeLogger<MediaKeys> {
  ~MediaKeys();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaKeys)

  MediaKeys(nsPIDOMWindowInner* aParentWindow, const nsAString& aKeySystem,
            const MediaKeySystemConfiguration& aConfig);

  already_AddRefed<DetailedPromise> Init(ErrorResult& aRv);

  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsresult Bind(HTMLMediaElement* aElement);
  void Unbind();

  // Javascript: readonly attribute DOMString keySystem;
  void GetKeySystem(nsString& retval) const;

  // JavaScript: MediaKeys.createSession()
  already_AddRefed<MediaKeySession> CreateSession(
      MediaKeySessionType aSessionType, ErrorResult& aRv);

  // JavaScript: MediaKeys.SetServerCertificate()
  already_AddRefed<DetailedPromise> SetServerCertificate(
      const ArrayBufferViewOrArrayBuffer& aServerCertificate, ErrorResult& aRv);

  already_AddRefed<MediaKeySession> GetSession(const nsAString& aSessionId);

  // Removes and returns MediaKeySession from the set of sessions awaiting
  // their sessionId to be assigned.
  already_AddRefed<MediaKeySession> GetPendingSession(uint32_t aToken);

  // Called once a Init() operation succeeds.
  void OnCDMCreated(PromiseId aId, const uint32_t aPluginId);

  // Called once the CDM generates a sessionId while servicing a
  // MediaKeySession.generateRequest() or MediaKeySession.load() call,
  // once the sessionId of a MediaKeySession is known.
  void OnSessionIdReady(MediaKeySession* aSession);

  // Called once a LoadSession succeeds.
  void OnSessionLoaded(PromiseId aId, bool aSuccess);

  // Called once a session has closed.
  void OnSessionClosed(MediaKeySession* aSession);

  CDMProxy* GetCDMProxy() { return mProxy; }

  // Makes a new promise, or nullptr on failure.
  already_AddRefed<DetailedPromise> MakePromise(ErrorResult& aRv,
                                                const nsACString& aName);
  // Stores promise in mPromises, returning an ID that can be used to retrieve
  // it later. The ID is passed to the CDM, so that it can signal specific
  // promises to be resolved.
  PromiseId StorePromise(DetailedPromise* aPromise);

  // Stores a map from promise id to pending session token. Using this
  // mapping, when a promise is rejected via its ID, we can check if the
  // promise corresponds to a pending session and retrieve that session
  // via the mapped-to token, and remove the pending session from the
  // list of sessions awaiting a session id.
  void ConnectPendingPromiseIdWithToken(PromiseId aId, uint32_t aToken);

  // Reject promise with the given exception.
  void RejectPromise(PromiseId aId, ErrorResult&& aException,
                     const nsCString& aReason);
  // Resolves promise with "undefined".
  void ResolvePromise(PromiseId aId);

  void Shutdown();

  // Called by CDMProxy when CDM crashes or shuts down. It is different from
  // Shutdown which is called from the script/dom side.
  void Terminated();

  // Returns true if this MediaKeys has been bound to a media element.
  bool IsBoundToMediaElement() const;

  // Indicates to a MediaKeys instance that the inner window parent of that
  // instance is being destroyed, this should prompt the keys to shutdown.
  void OnInnerWindowDestroy();

  void GetSessionsInfo(nsString& sessionsInfo);

  // JavaScript: MediaKeys.GetStatusForPolicy()
  already_AddRefed<Promise> GetStatusForPolicy(const MediaKeysPolicy& aPolicy,
                                               ErrorResult& aR);
  // Called by CDMProxy when CDM successfully GetStatusForPolicy.
  void ResolvePromiseWithKeyStatus(PromiseId aId,
                                   dom::MediaKeyStatus aMediaKeyStatus);

  template <typename T>
  void ResolvePromiseWithResult(PromiseId aId, const T& aResult) {
    RefPtr<DetailedPromise> promise(RetrievePromise(aId));
    if (!promise) {
      return;
    }
    promise->MaybeResolve(aResult);
  }

 private:
  // Instantiate CDMProxy instance.
  // It could be MediaDrmCDMProxy (Widevine on Fennec) or ChromiumCDMProxy (the
  // rest).
  already_AddRefed<CDMProxy> CreateCDMProxy();

  // Removes promise from mPromises, and returns it.
  already_AddRefed<DetailedPromise> RetrievePromise(PromiseId aId);

  // Helpers to connect and disconnect to the parent inner window. An inner
  // window should track (via weak ptr) MediaKeys created within it so we can
  // ensure MediaKeys are shutdown if that window is destroyed.
  void ConnectInnerWindow();
  void DisconnectInnerWindow();

  // Owning ref to proxy. The proxy has a weak reference back to the MediaKeys,
  // and the MediaKeys destructor clears the proxy's reference to the MediaKeys.
  RefPtr<CDMProxy> mProxy;

  // The HTMLMediaElement the MediaKeys are associated with. Note that a
  // MediaKeys instance may not be associated with any HTMLMediaElement so
  // this can be null (we also cannot rely on a media element to drive shutdown
  // for this reason).
  RefPtr<HTMLMediaElement> mElement;

  // The  inner window associated with an instance of MediaKeys. We will
  // shutdown the media keys when this Window is destroyed. We do this from the
  // window rather than a document to address the case where media keys can be
  // created in an about:blank document that then performs an async load -- this
  // recreates the document, but the inner window is preserved in such a case.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1675360 for more info.
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  const nsString mKeySystem;
  KeySessionHashMap mKeySessions;
  PromiseHashMap mPromises;
  PendingKeySessionsHashMap mPendingSessions;
  PromiseId mCreatePromiseId;

  // The principal of the relevant settings object.
  RefPtr<nsIPrincipal> mPrincipal;
  // The principal of the top level page. This can differ from mPrincipal if
  // we're in an iframe.
  RefPtr<nsIPrincipal> mTopLevelPrincipal;

  const MediaKeySystemConfiguration mConfig;

  PendingPromiseIdTokenHashMap mPromiseIdToken;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mediakeys_h__
