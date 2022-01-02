/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_Script_h_
#define intl_components_Script_h_

#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/Vector.h"

namespace mozilla::intl {

// The code point which has the most script extensions is 0x0965, which has 21
// script extensions, so choose the vector size as 32 to prevent heap
// allocation.
constexpr size_t kMaxScripts = 32;

// The list of script extensions, it consists of one or more script codes from
// ISO 15924, or mozilla::unicode::Script.
//
// Choose the element type as int16_t to have the same size of
// mozilla::unicode::Script.
// We didn't use mozilla::unicode::Script directly here because we cannot
// include the header in standalone JS shell build.
using ScriptExtensionVector = Vector<int16_t, kMaxScripts>;

/**
 * This component is a Mozilla-focused API for working with Unicode scripts.
 */
class Script final {
 public:
  /**
   * Get the script extensions for the given code point, and write the script
   * extensions to aExtensions vector. If the code point has script extensions,
   * the script code (Script::COMMON or Script::INHERITED) will be excluded.
   *
   * If the code point doesn't have any script extension, then its script code
   * will be written to aExtensions vector.
   *
   * If the code point is invalid, Script::UNKNOWN will be written to
   * aExtensions vector.
   *
   * Note: aExtensions will be cleared after calling this method regardless of
   * failure.
   *
   * See [1] for the script code of the code point, [2] for the script
   * extensions.
   *
   * https://www.unicode.org/Public/UNIDATA/Scripts.txt
   * https://www.unicode.org/Public/UNIDATA/ScriptExtensions.txt
   */
  static ICUResult GetExtensions(char32_t aCodePoint,
                                 ScriptExtensionVector& aExtensions);
};
}  // namespace mozilla::intl
#endif  // intl_components_Script_h_
