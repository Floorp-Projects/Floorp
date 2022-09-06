/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PushSubscription_h
#define mozilla_dom_PushSubscription_h

#include "js/RootingAPI.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PushSubscriptionBinding.h"
#include "mozilla/dom/PushSubscriptionOptionsBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "domstubs.h"

class nsIGlobalObject;

namespace mozilla {
class ErrorResult;

namespace dom {

class Promise;

class PushSubscription final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(PushSubscription)

  PushSubscription(nsIGlobalObject* aGlobal, const nsAString& aEndpoint,
                   const nsAString& aScope,
                   Nullable<EpochTimeStamp>&& aExpirationTime,
                   nsTArray<uint8_t>&& aP256dhKey,
                   nsTArray<uint8_t>&& aAuthSecret,
                   nsTArray<uint8_t>&& aAppServerKey);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  void GetEndpoint(nsAString& aEndpoint) const { aEndpoint = mEndpoint; }

  void GetKey(JSContext* cx, PushEncryptionKeyName aType,
              JS::MutableHandle<JSObject*> aKey, ErrorResult& aRv);

  Nullable<EpochTimeStamp> GetExpirationTime() { return mExpirationTime; };

  static already_AddRefed<PushSubscription> Constructor(
      GlobalObject& aGlobal, const PushSubscriptionInit& aInitDict,
      ErrorResult& aRv);

  already_AddRefed<Promise> Unsubscribe(ErrorResult& aRv);

  void ToJSON(PushSubscriptionJSON& aJSON, ErrorResult& aRv);

  already_AddRefed<PushSubscriptionOptions> Options();

 private:
  ~PushSubscription();

  already_AddRefed<Promise> UnsubscribeFromWorker(ErrorResult& aRv);

  nsString mEndpoint;
  nsString mScope;
  Nullable<EpochTimeStamp> mExpirationTime;
  nsTArray<uint8_t> mRawP256dhKey;
  nsTArray<uint8_t> mAuthSecret;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<PushSubscriptionOptions> mOptions;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PushSubscription_h
