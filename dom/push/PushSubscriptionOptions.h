/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PushSubscriptionOptions_h
#define mozilla_dom_PushSubscriptionOptions_h

#include "nsCycleCollectionParticipant.h"
#include "nsContentUtils.h" // Required for nsContentUtils::PushEnabled
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class PushSubscriptionOptions final : public nsISupports
                                    , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PushSubscriptionOptions)

  PushSubscriptionOptions(nsIGlobalObject* aGlobal,
                          nsTArray<uint8_t>&& aRawAppServerKey);

  nsIGlobalObject*
  GetParentObject() const
  {
    return mGlobal;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetApplicationServerKey(JSContext* aCx,
                          JS::MutableHandle<JSObject*> aKey,
                          ErrorResult& aRv);

private:
  ~PushSubscriptionOptions();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsTArray<uint8_t> mRawAppServerKey;
  JS::Heap<JSObject*> mAppServerKey;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PushSubscriptionOptions_h
