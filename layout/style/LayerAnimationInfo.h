/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LayerAnimationInfo_h
#define mozilla_LayerAnimationInfo_h

#include "nsChangeHint.h"
#include "nsCSSProperty.h"
#include "nsDisplayList.h" // For nsDisplayItem::Type

namespace mozilla {

struct LayerAnimationInfo {
#ifdef DEBUG
  static void Initialize();
#endif
  // For CSS properties that may be animated on a separate layer, represents
  // a record of the corresponding layer type and change hint.
  struct Record {
    nsCSSProperty mProperty;
    nsDisplayItem::Type mLayerType;
    nsChangeHint mChangeHint;
  };

  static const size_t kRecords = 2;
  static const Record sRecords[kRecords];
};

} // namespace mozilla

#endif /* !defined(mozilla_LayerAnimationInfo_h) */
