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
#include "mozilla/dom/MediaKeySessionBinding.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "mozilla/dom/UnionTypes.h"

struct JSContext;

namespace mozilla {

class CDMProxy;

namespace dom {

class MediaKeyError;

class MediaKeySession MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaKeySession,
                                           DOMEventTargetHelper)
public:
  MediaKeySession(nsPIDOMWindow* aParent,
                  MediaKeys* aKeys,
                  const nsAString& aKeySystem,
                  SessionType aSessionType,
                  ErrorResult& aRv);

  void Init(const nsAString& aSessionId);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // Mark this as resultNotAddRefed to return raw pointers
  MediaKeyError* GetError() const;

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

  already_AddRefed<Promise> GetUsableKeyIds(ErrorResult& aRv);

  void DispatchKeyMessage(const nsTArray<uint8_t>& aMessage,
                          const nsAString& aURL);

  void DispatchKeyError(uint32_t system_code);

  void DispatchKeysChange();

  void OnClosed();

  bool IsClosed() const;

private:
  ~MediaKeySession();

  nsRefPtr<Promise> mClosed;

  nsRefPtr<MediaKeyError> mMediaKeyError;
  nsRefPtr<MediaKeys> mKeys;
  const nsString mKeySystem;
  nsString mSessionId;
  const SessionType mSessionType;
  bool mIsClosed;
  bool mUninitialized;
};

} // namespace dom
} // namespace mozilla

#endif
