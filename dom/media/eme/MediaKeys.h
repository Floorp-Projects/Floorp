/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediakeys_h__
#define mozilla_dom_mediakeys_h__

#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/DetailedPromise.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {

class CDMProxy;

namespace dom {

class ArrayBufferViewOrArrayBuffer;
class MediaKeySession;
class HTMLMediaElement;

typedef nsRefPtrHashtable<nsStringHashKey, MediaKeySession> KeySessionHashMap;
typedef nsRefPtrHashtable<nsUint32HashKey, dom::DetailedPromise> PromiseHashMap;
typedef nsRefPtrHashtable<nsUint32HashKey, MediaKeySession> PendingKeySessionsHashMap;
typedef nsDataHashtable<nsUint32HashKey, uint32_t> PendingPromiseIdTokenHashMap;
typedef uint32_t PromiseId;

// This class is used on the main thread only.
// Note: its addref/release is not (and can't be) thread safe!
class MediaKeys final : public nsISupports,
                        public nsWrapperCache,
                        public SupportsWeakPtr<MediaKeys>
{
  ~MediaKeys();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaKeys)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(MediaKeys)

  MediaKeys(nsPIDOMWindowInner* aParentWindow,
            const nsAString& aKeySystem,
            const MediaKeySystemConfiguration& aConfig);

  already_AddRefed<DetailedPromise> Init(ErrorResult& aRv);

  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsresult Bind(HTMLMediaElement* aElement);
  void Unbind();

  // Javascript: readonly attribute DOMString keySystem;
  void GetKeySystem(nsString& retval) const;

  // JavaScript: MediaKeys.createSession()
  already_AddRefed<MediaKeySession> CreateSession(JSContext* aCx,
                                                  MediaKeySessionType aSessionType,
                                                  ErrorResult& aRv);

  // JavaScript: MediaKeys.SetServerCertificate()
  already_AddRefed<DetailedPromise>
    SetServerCertificate(const ArrayBufferViewOrArrayBuffer& aServerCertificate,
                         ErrorResult& aRv);

  already_AddRefed<MediaKeySession> GetSession(const nsAString& aSessionId);

  // Removes and returns MediaKeySession from the set of sessions awaiting
  // their sessionId to be assigned.
  already_AddRefed<MediaKeySession> GetPendingSession(uint32_t aToken);

  // Called once a Init() operation succeeds.
  void OnCDMCreated(PromiseId aId,
                    const nsACString& aNodeId, const uint32_t aPluginId);

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

  // Reject promise with DOMException corresponding to aExceptionCode.
  void RejectPromise(PromiseId aId, nsresult aExceptionCode,
                     const nsCString& aReason);
  // Resolves promise with "undefined".
  void ResolvePromise(PromiseId aId);

  const nsCString& GetNodeId() const;

  void Shutdown();

  // Called by CDMProxy when CDM crashes or shuts down. It is different from
  // Shutdown which is called from the script/dom side.
  void Terminated();

  // Returns true if this MediaKeys has been bound to a media element.
  bool IsBoundToMediaElement() const;

private:

  // Removes promise from mPromises, and returns it.
  already_AddRefed<DetailedPromise> RetrievePromise(PromiseId aId);

  // Owning ref to proxy. The proxy has a weak reference back to the MediaKeys,
  // and the MediaKeys destructor clears the proxy's reference to the MediaKeys.
  RefPtr<CDMProxy> mProxy;

  RefPtr<HTMLMediaElement> mElement;

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  const nsString mKeySystem;
  nsCString mNodeId;
  KeySessionHashMap mKeySessions;
  PromiseHashMap mPromises;
  PendingKeySessionsHashMap mPendingSessions;
  PromiseId mCreatePromiseId;

  RefPtr<nsIPrincipal> mPrincipal;
  RefPtr<nsIPrincipal> mTopLevelPrincipal;

  const MediaKeySystemConfiguration mConfig;

  PendingPromiseIdTokenHashMap mPromiseIdToken;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mediakeys_h__
