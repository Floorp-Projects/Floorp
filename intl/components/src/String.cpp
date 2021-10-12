/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/String.h"

namespace mozilla::intl {

Span<const char> String::GetUnicodeVersion() {
  return MakeStringSpan(U_UNICODE_VERSION);
}

}  // namespace mozilla::intl
