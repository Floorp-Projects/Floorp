/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GeolocationPositionError_h
#define mozilla_dom_GeolocationPositionError_h

#include "nsWrapperCache.h"
#include "nsISupportsImpl.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/CallbackObject.h"

class nsIDOMGeoPositionErrorCallback;

namespace mozilla::dom {
class PositionErrorCallback;
class Geolocation;
typedef CallbackObjectHolder<PositionErrorCallback,
                             nsIDOMGeoPositionErrorCallback>
    GeoPositionErrorCallback;

class GeolocationPositionError final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(GeolocationPositionError)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(GeolocationPositionError)

  GeolocationPositionError(Geolocation* aParent, int16_t aCode);

  nsWrapperCache* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  int16_t Code() const { return mCode; }

  void GetMessage(nsAString& aMessage) const;

  MOZ_CAN_RUN_SCRIPT
  void NotifyCallback(const GeoPositionErrorCallback& callback);

 private:
  ~GeolocationPositionError();
  int16_t mCode;
  RefPtr<Geolocation> mParent;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_GeolocationPositionError_h */
