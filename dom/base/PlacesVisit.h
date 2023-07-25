/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesVisit_h
#define mozilla_dom_PlacesVisit_h

#include "mozilla/dom/PlacesEvent.h"

namespace mozilla {
namespace dom {

class PlacesVisit final : public PlacesEvent {
 public:
  explicit PlacesVisit() : PlacesEvent(PlacesEventType::Page_visited) {}

  static already_AddRefed<PlacesVisit> Constructor(const GlobalObject& aGlobal,
                                                   ErrorResult& aRv) {
    RefPtr<PlacesVisit> event = new PlacesVisit();
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesVisit_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesVisit* AsPlacesVisit() const override { return this; }

  void GetUrl(nsString& aUrl) { aUrl = mUrl; }
  uint64_t VisitId() { return mVisitId; }
  uint64_t VisitTime() { return mVisitTime; }
  uint64_t ReferringVisitId() { return mReferringVisitId; }
  uint64_t TransitionType() { return mTransitionType; }
  void GetPageGuid(nsTString<char>& aPageGuid) { aPageGuid = mPageGuid; }
  uint64_t Frecency() { return mFrecency; }
  bool Hidden() { return mHidden; }
  uint32_t VisitCount() { return mVisitCount; }
  uint32_t TypedCount() { return mTypedCount; }
  void GetLastKnownTitle(nsString& aLastKnownTitle) {
    aLastKnownTitle = mLastKnownTitle;
  }

  // It's convenient for these to be directly available in C++, so just expose
  // them. These are generally passed around with const qualifiers anyway, so
  // it shouldn't be a problem.
  nsString mUrl;
  uint64_t mVisitId;
  uint64_t mVisitTime;
  uint64_t mReferringVisitId;
  uint32_t mTransitionType;
  nsCString mPageGuid;
  int64_t mFrecency;
  bool mHidden;
  uint32_t mVisitCount;
  uint32_t mTypedCount;
  nsString mLastKnownTitle;

 private:
  ~PlacesVisit() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesVisit_h
