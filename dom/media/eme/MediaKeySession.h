/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaKeySession_h
#define mozilla_dom_MediaKeySession_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/Date.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/DetailedPromise.h"
#include "mozilla/dom/MediaKeySessionBinding.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"

struct JSContext;

namespace mozilla {
namespace dom {

class ArrayBufferViewOrArrayBuffer;
class MediaKeyError;
class MediaKeyStatusMap;

class MediaKeySession final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaKeySession,
                                           DOMEventTargetHelper)
public:
  MediaKeySession(JSContext* aCx,
                  nsPIDOMWindow* aParent,
                  MediaKeys* aKeys,
                  const nsAString& aKeySystem,
                  const nsAString& aCDMVersion,
                  SessionType aSessionType,
                  ErrorResult& aRv);

  void SetSessionId(const nsAString& aSessionId);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Mark this as resultNotAddRefed to return raw pointers
  MediaKeyError* GetError() const;

  MediaKeyStatusMap* KeyStatuses() const;

  void GetKeySystem(nsString& aRetval) const;

  void GetSessionId(nsString& aRetval) const;

  const nsString& GetSessionId() const;

  // Number of ms since epoch at which expiration occurs, or NaN if unknown.
  // TODO: The type of this attribute is still under contention.
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=25902
  double Expiration() const;

  Promise* Closed() const;

  already_AddRefed<Promise> GenerateRequest(const nsAString& aInitDataType,
                                            const ArrayBufferViewOrArrayBuffer& aInitData,
                                            ErrorResult& aRv);

  already_AddRefed<Promise> Load(const nsAString& aSessionId,
                                 ErrorResult& aRv);

  already_AddRefed<Promise> Update(const ArrayBufferViewOrArrayBuffer& response,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> Close(ErrorResult& aRv);

  already_AddRefed<Promise> Remove(ErrorResult& aRv);

  void DispatchKeyMessage(MediaKeyMessageType aMessageType,
                          const nsTArray<uint8_t>& aMessage);

  void DispatchKeyError(uint32_t system_code);

  void DispatchKeyStatusesChange();

  void OnClosed();

  bool IsClosed() const;

  void SetExpiration(double aExpiry);

  // Process-unique identifier.
  uint32_t Token() const;

private:
  ~MediaKeySession();

  void UpdateKeyStatusMap();
  already_AddRefed<DetailedPromise> MakePromise(ErrorResult& aRv,
                                                const nsACString& aName);

  RefPtr<DetailedPromise> mClosed;

  RefPtr<MediaKeyError> mMediaKeyError;
  RefPtr<MediaKeys> mKeys;
  const nsString mKeySystem;
  const nsString mCDMVersion;
  nsString mSessionId;
  const SessionType mSessionType;
  const uint32_t mToken;
  bool mIsClosed;
  bool mUninitialized;
  RefPtr<MediaKeyStatusMap> mKeyStatusMap;
  double mExpiration;
};

} // namespace dom
} // namespace mozilla

#endif
