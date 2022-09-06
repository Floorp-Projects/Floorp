/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigationPreloadManager_h
#define mozilla_dom_NavigationPreloadManager_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"
#include "mozilla/RefPtr.h"

class nsIGlobalObject;

namespace mozilla::dom {

class Promise;

class NavigationPreloadManager final : public nsISupports,
                                       public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(NavigationPreloadManager)

  static bool IsValidHeader(const nsACString& aHeader);

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  NavigationPreloadManager(nsCOMPtr<nsIGlobalObject>&& aGlobal,
                           RefPtr<ServiceWorkerRegistration::Inner>& aInner);

  // Webidl binding
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIdl implementation
  already_AddRefed<Promise> Enable(ErrorResult& aError);

  already_AddRefed<Promise> Disable(ErrorResult& aError);

  already_AddRefed<Promise> SetHeaderValue(const nsACString& aHeader,
                                           ErrorResult& aError);

  already_AddRefed<Promise> GetState(ErrorResult& aError);

 private:
  ~NavigationPreloadManager() = default;

  // General method for Enable()/Disable()
  already_AddRefed<Promise> SetEnabled(bool aEnabled, ErrorResult& aError);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ServiceWorkerRegistration::Inner> mInner;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigationPreloadManager_h
