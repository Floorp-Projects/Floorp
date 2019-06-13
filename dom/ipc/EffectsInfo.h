/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EffectsInfo_h
#define mozilla_dom_EffectsInfo_h

#include "nsRect.h"

namespace mozilla {
namespace dom {

/**
 * An EffectsInfo contains information for a remote browser about the graphical
 * effects that are being applied to it by ancestor browsers in different
 * processes.
 */
class EffectsInfo {
 public:
  EffectsInfo() { *this = EffectsInfo::FullyHidden(); }

  static EffectsInfo VisibleWithinRect(const nsRect& aVisibleRect,
                                       float aScaleX, float aScaleY) {
    return EffectsInfo{aVisibleRect, aScaleX, aScaleY};
  }
  static EffectsInfo FullyHidden() { return EffectsInfo{nsRect(), 1.0f, 1.0f}; }

  bool operator==(const EffectsInfo& aOther) {
    return mVisibleRect == aOther.mVisibleRect && mScaleX == aOther.mScaleX &&
           mScaleY == aOther.mScaleY;
  }
  bool operator!=(const EffectsInfo& aOther) { return !(*this == aOther); }

  bool IsVisible() const { return !mVisibleRect.IsEmpty(); }

  // The visible rect of this browser relative to the root frame. If this is
  // empty then the browser can be considered invisible.
  nsRect mVisibleRect;
  // The desired scale factors to apply to rasterized content to match
  // transforms applied in ancestor browsers.
  float mScaleX;
  float mScaleY;
  // If you add new fields here, you must also update operator==

 private:
  EffectsInfo(const nsRect& aVisibleRect, float aScaleX, float aScaleY)
      : mVisibleRect(aVisibleRect), mScaleX(aScaleX), mScaleY(aScaleY) {}
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EffectsInfo_h
