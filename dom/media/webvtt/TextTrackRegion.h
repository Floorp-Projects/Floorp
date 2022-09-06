/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackRegion_h
#define mozilla_dom_TextTrackRegion_h

#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/VTTRegionBinding.h"
#include "mozilla/Preferences.h"

namespace mozilla::dom {

class GlobalObject;
class TextTrack;

class TextTrackRegion final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TextTrackRegion)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const { return mParent; }

  explicit TextTrackRegion(nsISupports* aGlobal);

  /** WebIDL Methods. */

  static already_AddRefed<TextTrackRegion> Constructor(
      const GlobalObject& aGlobal, ErrorResult& aRv);

  double Lines() const { return mLines; }

  void SetLines(double aLines, ErrorResult& aRv) {
    if (aLines < 0) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      mLines = aLines;
    }
  }

  double Width() const { return mWidth; }

  void SetWidth(double aWidth, ErrorResult& aRv) {
    if (!InvalidValue(aWidth, aRv)) {
      mWidth = aWidth;
    }
  }

  double RegionAnchorX() const { return mRegionAnchorX; }

  void SetRegionAnchorX(double aVal, ErrorResult& aRv) {
    if (!InvalidValue(aVal, aRv)) {
      mRegionAnchorX = aVal;
    }
  }

  double RegionAnchorY() const { return mRegionAnchorY; }

  void SetRegionAnchorY(double aVal, ErrorResult& aRv) {
    if (!InvalidValue(aVal, aRv)) {
      mRegionAnchorY = aVal;
    }
  }

  double ViewportAnchorX() const { return mViewportAnchorX; }

  void SetViewportAnchorX(double aVal, ErrorResult& aRv) {
    if (!InvalidValue(aVal, aRv)) {
      mViewportAnchorX = aVal;
    }
  }

  double ViewportAnchorY() const { return mViewportAnchorY; }

  void SetViewportAnchorY(double aVal, ErrorResult& aRv) {
    if (!InvalidValue(aVal, aRv)) {
      mViewportAnchorY = aVal;
    }
  }

  ScrollSetting Scroll() const { return mScroll; }

  void SetScroll(const ScrollSetting& aScroll) {
    if (aScroll == ScrollSetting::_empty || aScroll == ScrollSetting::Up) {
      mScroll = aScroll;
    }
  }

  void GetId(nsAString& aId) const { aId = mId; }

  void SetId(const nsAString& aId) { mId = aId; }

  /** end WebIDL Methods. */

  // Helper to aid copying of a given TextTrackRegion's width, lines,
  // anchor, viewport and scroll values.
  void CopyValues(TextTrackRegion& aRegion);

  // -----helpers-------
  const nsAString& Id() const { return mId; }

 private:
  ~TextTrackRegion() = default;

  nsCOMPtr<nsISupports> mParent;
  nsString mId;
  double mWidth;
  long mLines;
  double mRegionAnchorX;
  double mRegionAnchorY;
  double mViewportAnchorX;
  double mViewportAnchorY;
  ScrollSetting mScroll;

  // Helper to ensure new value is in the range: 0.0% - 100.0%; throws
  // an IndexSizeError otherwise.
  inline bool InvalidValue(double aValue, ErrorResult& aRv) {
    if (aValue < 0.0 || aValue > 100.0) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return true;
    }

    return false;
  }
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TextTrackRegion_h
