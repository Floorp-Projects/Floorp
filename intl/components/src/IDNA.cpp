/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/IDNA.h"

namespace mozilla::intl {

// static
Result<UniquePtr<IDNA>, ICUError> IDNA::TryCreate(ProcessingType aProcessing) {
  uint32_t IDNAOptions = UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ;
  if (aProcessing == ProcessingType::NonTransitional) {
    IDNAOptions |= UIDNA_NONTRANSITIONAL_TO_UNICODE;
  }

  UErrorCode status = U_ZERO_ERROR;
  UIDNA* idna = uidna_openUTS46(IDNAOptions, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return UniquePtr<IDNA>(new IDNA(idna));
}

IDNA::~IDNA() { uidna_close(mIDNA.GetMut()); }
}  // namespace mozilla::intl
