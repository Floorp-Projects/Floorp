/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TabFocusModel_h
#define mozilla_TabFocusModel_h

#include "mozilla/StaticPrefs_accessibility.h"

namespace mozilla {

enum class TabFocusableType : uint8_t {
  TextControls = 1,       // Textboxes and lists always tabbable
  FormElements = 1 << 1,  // Non-text form elements
  Links = 1 << 2,         // Links
  Any = TextControls | FormElements | Links,
};

class TabFocusModel final {
 public:
  static bool AppliesToXUL() {
    return StaticPrefs::accessibility_tabfocus_applies_to_xul();
  }

  static bool IsTabFocusable(TabFocusableType aType) {
    return StaticPrefs::accessibility_tabfocus() & int32_t(aType);
  }
};

}  // namespace mozilla

#endif
