/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LayerAnimationInfo_h
#define mozilla_LayerAnimationInfo_h

#include "nsChangeHint.h"
#include "nsCSSPropertyID.h"
#include "nsCSSPropertyIDSet.h"
#include "nsDisplayList.h" // For nsDisplayItem::Type

namespace mozilla {

struct LayerAnimationInfo {
#ifdef DEBUG
  static void Initialize();
#endif
  // For CSS properties that may be animated on a separate layer, represents
  // a record of the corresponding layer type and change hint.
  struct Record {
    nsCSSPropertyID mProperty;
    DisplayItemType mDisplayItemType;
    nsChangeHint mChangeHint;
  };

  // Returns the corresponding display item type for |aProperty| when it is
  // animated on the compositor.
  // Returns DisplayItemType::TYPE_ZERO if |aProperty| cannot be animated on the
  // compositor.
  static DisplayItemType
  GetDisplayItemTypeForProperty(nsCSSPropertyID aProperty);

  static const size_t kRecords =
    nsCSSPropertyIDSet::CompositorAnimatableCount();
  static const Record sRecords[kRecords];
};

} // namespace mozilla

#endif /* !defined(mozilla_LayerAnimationInfo_h) */
