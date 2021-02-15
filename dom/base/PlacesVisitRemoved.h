/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesVisitRemoved_h
#define mozilla_dom_PlacesVisitRemoved_h

#include "mozilla/dom/PlacesEvent.h"

namespace mozilla {
namespace dom {

class PlacesVisitRemoved final : public PlacesEvent {
 public:
  explicit PlacesVisitRemoved() : PlacesEvent(PlacesEventType::Page_removed) {}

  static already_AddRefed<PlacesVisitRemoved> Constructor(
      const GlobalObject& aGlobal, const PlacesVisitRemovedInit& aInitDict) {
    MOZ_ASSERT(
        aInitDict.mReason == PlacesVisitRemoved_Binding::REASON_DELETED ||
            aInitDict.mReason == PlacesVisitRemoved_Binding::REASON_EXPIRED,
        "The reason should be REASON_DELETED or REASON_EXPIRED");
    MOZ_ASSERT(
        !(aInitDict.mIsRemovedFromStore && aInitDict.mIsPartialVisistsRemoval),
        "isRemovedFromStore and isPartialVisistsRemoval are inconsistent");

    RefPtr<PlacesVisitRemoved> event = new PlacesVisitRemoved();
    event->mUrl = aInitDict.mUrl;
    event->mPageGuid = aInitDict.mPageGuid;
    event->mReason = aInitDict.mReason;
    event->mTransitionType = aInitDict.mTransitionType;
    event->mIsRemovedFromStore = aInitDict.mIsRemovedFromStore;
    event->mIsPartialVisistsRemoval = aInitDict.mIsPartialVisistsRemoval;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesVisitRemoved_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesVisitRemoved* AsPlacesVisitRemoved() const override {
    return this;
  }

  void GetUrl(nsString& aUrl) const { aUrl = mUrl; }
  void GetPageGuid(nsCString& aPageGuid) const { aPageGuid = mPageGuid; }
  uint16_t Reason() const { return mReason; }
  uint32_t TransitionType() const { return mTransitionType; }
  bool IsRemovedFromStore() const { return mIsRemovedFromStore; }
  bool IsPartialVisistsRemoval() const { return mIsPartialVisistsRemoval; }

  nsString mUrl;
  nsCString mPageGuid;
  uint16_t mReason;
  uint32_t mTransitionType;
  bool mIsRemovedFromStore;
  bool mIsPartialVisistsRemoval;

 private:
  ~PlacesVisitRemoved() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesVisitRemoved_h
