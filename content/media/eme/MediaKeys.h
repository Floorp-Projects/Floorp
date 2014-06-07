/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediakeys_h__
#define mozilla_dom_mediakeys_h__

#include "nsIDOMMediaError.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/MediaKeysBinding.h"

namespace mozilla {

class CDMProxy;

namespace dom {

class MediaKeySession;

typedef nsRefPtrHashtable<nsStringHashKey, MediaKeySession> KeySessionHashMap;
typedef nsRefPtrHashtable<nsUint32HashKey, dom::Promise> PromiseHashMap;
typedef nsRefPtrHashtable<nsUint32HashKey, MediaKeySession> PendingKeySessionsHashMap;
typedef uint32_t PromiseId;

// This class is used on the main thread only.
// Note: it's addref/release is not (and can't be) thread safe!
class MediaKeys MOZ_FINAL : public nsISupports,
                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaKeys)

  MediaKeys(nsPIDOMWindow* aParentWindow, const nsAString& aKeySystem);

  ~MediaKeys();

  nsPIDOMWindow* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // Javascript: readonly attribute DOMString keySystem;
  void GetKeySystem(nsString& retval) const;

  // JavaScript: MediaKeys.createSession()
  already_AddRefed<Promise> CreateSession(const nsAString& aInitDataType,
                                          const Uint8Array& aInitData,
                                          SessionType aSessionType);

  // JavaScript: MediaKeys.loadSession()
  already_AddRefed<Promise> LoadSession(const nsAString& aSessionId);

  // JavaScript: MediaKeys.SetServerCertificate()
  already_AddRefed<Promise> SetServerCertificate(const Uint8Array& aServerCertificate);

  // JavaScript: MediaKeys.create()
  static
  already_AddRefed<Promise> Create(const GlobalObject& aGlobal,
                                   const nsAString& aKeySystem,
                                   ErrorResult& aRv);

  // JavaScript: MediaKeys.IsTypeSupported()
  static IsTypeSupportedResult IsTypeSupported(const GlobalObject& aGlobal,
                                               const nsAString& aKeySystem,
                                               const Optional<nsAString>& aInitDataType,
                                               const Optional<nsAString>& aContentType,
                                               const Optional<nsAString>& aCapability);

  already_AddRefed<MediaKeySession> GetSession(const nsAString& aSessionId);

  // Called once a Create() operation succeeds.
  void OnCDMCreated(PromiseId aId);
  // Called once a CreateSession or LoadSession succeeds.
  void OnSessionActivated(PromiseId aId, const nsAString& aSessionId);
  // Called once a session has closed.
  void OnSessionClosed(MediaKeySession* aSession);

  CDMProxy* GetCDMProxy() { return mProxy; }

  // Makes a new promise, or nullptr on failure.
  already_AddRefed<Promise> MakePromise();
  // Stores promise in mPromises, returning an ID that can be used to retrieve
  // it later. The ID is passed to the CDM, so that it can signal specific
  // promises to be resolved.
  PromiseId StorePromise(Promise* aPromise);

  // Reject promise with DOMException corresponding to aExceptionCode.
  void RejectPromise(PromiseId aId, nsresult aExceptionCode);
  // Resolves promise with "undefined".
  void ResolvePromise(PromiseId aId);

private:

  // Removes promise from mPromises, and returns it.
  already_AddRefed<Promise> RetrievePromise(PromiseId aId);

  // Owning ref to proxy. The proxy has a weak reference back to the MediaKeys,
  // and the MediaKeys destructor clears the proxy's reference to the MediaKeys.
  nsRefPtr<CDMProxy> mProxy;

  nsCOMPtr<nsPIDOMWindow> mParent;
  nsString mKeySystem;
  KeySessionHashMap mKeySessions;
  PromiseHashMap mPromises;
  PendingKeySessionsHashMap mPendingSessions;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mediakeys_h__
