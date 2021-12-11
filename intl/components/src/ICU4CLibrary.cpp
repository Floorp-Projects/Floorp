/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/ICU4CLibrary.h"

#include "unicode/putil.h"
#include "unicode/uclean.h"
#include "unicode/utypes.h"
#include "unicode/uversion.h"

namespace mozilla::intl {

ICUResult ICU4CLibrary::Initialize() {
#if !MOZ_SYSTEM_ICU
  // Explicitly set the data directory to its default value, but only when we're
  // sure that we use our in-tree ICU copy. See bug 1527879 and ICU bug
  // report <https://unicode-org.atlassian.net/browse/ICU-20491>.
  u_setDataDirectory("");
#endif

  UErrorCode status = U_ZERO_ERROR;
  u_init(&status);
  return ToICUResult(status);
}

void ICU4CLibrary::Cleanup() { u_cleanup(); }

ICUResult ICU4CLibrary::SetMemoryFunctions(MemoryFunctions aMemoryFunctions) {
  UErrorCode status = U_ZERO_ERROR;
  u_setMemoryFunctions(/* context = */ nullptr, aMemoryFunctions.mAllocFn,
                       aMemoryFunctions.mReallocFn, aMemoryFunctions.mFreeFn,
                       &status);
  return ToICUResult(status);
}

Span<const char> ICU4CLibrary::GetVersion() {
  return MakeStringSpan(U_ICU_VERSION);
}

}  // namespace mozilla::intl
