/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EffectsInfo_h
#define mozilla_dom_EffectsInfo_h

namespace mozilla {
namespace dom {

/**
 * An EffectsInfo contains information for a remote browser about the graphical
 * effects that are being applied to it by ancestor browsers in different
 * processes.
 *
 * TODO: This struct currently only reports visibility, and should be extended
 *       with information on clipping and scaling.
 */
class EffectsInfo {
 public:
  EffectsInfo() { *this = EffectsInfo::FullyHidden(); }
  static EffectsInfo FullyVisible() { return EffectsInfo{true}; }
  static EffectsInfo FullyHidden() { return EffectsInfo{false}; }

  bool operator==(const EffectsInfo& aOther) {
    return mVisible == aOther.mVisible;
  }
  bool operator!=(const EffectsInfo& aOther) { return !(*this == aOther); }

  // If you add new state here, you must also update operator==
  bool mVisible;
  /*
   * TODO: Add information for ancestor scaling and clipping.
   */

 private:
  explicit EffectsInfo(bool aVisible) : mVisible(aVisible) {}
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EffectsInfo_h
