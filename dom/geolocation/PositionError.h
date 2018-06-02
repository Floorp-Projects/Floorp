/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PositionError_h
#define mozilla_dom_PositionError_h

#include "nsWrapperCache.h"
#include "nsISupportsImpl.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/CallbackObject.h"

class nsIDOMGeoPositionErrorCallback;

namespace mozilla {
namespace dom {
class PositionErrorCallback;
class Geolocation;
typedef CallbackObjectHolder<PositionErrorCallback, nsIDOMGeoPositionErrorCallback> GeoPositionErrorCallback;

class PositionError final : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PositionError)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PositionError)

  PositionError(Geolocation* aParent, int16_t aCode);

  nsWrapperCache* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  int16_t Code() const {
    return mCode;
  }

  void GetMessage(nsAString& aMessage) const;

  void NotifyCallback(const GeoPositionErrorCallback& callback);
private:
  ~PositionError();
  int16_t mCode;
  RefPtr<Geolocation> mParent;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PositionError_h */
