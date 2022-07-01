/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollSnapTargetId_h_
#define mozilla_ScrollSnapTargetId_h_

#include <cstdint>

namespace mozilla {

// The id for each scroll snap target element to track the last snapped element.
// 0 means it wasn't snapped on the last scroll operation.
enum class ScrollSnapTargetId : uintptr_t {
  None = 0,
};

}  // namespace mozilla

#endif  // mozilla_ScrollSnapTargetId_h_
