/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/Script.h"

#include "unicode/uscript.h"

namespace mozilla::intl {

// static
ICUResult Script::GetExtensions(char32_t aCodePoint,
                                ScriptExtensionVector& aExtensions) {
  // Clear the vector first.
  aExtensions.clear();

  // We cannot pass aExtensions to uscript_getScriptExtension as USCriptCode
  // takes 4 bytes, so create a local UScriptCode array to get the extensions.
  UScriptCode ext[kMaxScripts];
  UErrorCode status = U_ZERO_ERROR;
  int32_t len = uscript_getScriptExtensions(static_cast<UChar32>(aCodePoint),
                                            ext, kMaxScripts, &status);
  if (U_FAILURE(status)) {
    // kMaxScripts should be large enough to hold the maximun number of script
    // extensions.
    MOZ_DIAGNOSTIC_ASSERT(status != U_BUFFER_OVERFLOW_ERROR);
    return Err(ToICUError(status));
  }

  if (!aExtensions.reserve(len)) {
    return Err(ICUError::OutOfMemory);
  }

  for (int32_t i = 0; i < len; i++) {
    aExtensions.infallibleAppend(ext[i]);
  }

  return Ok();
}
}  // namespace mozilla::intl
