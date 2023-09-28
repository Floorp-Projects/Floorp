/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_IDNA_h_
#define intl_components_IDNA_h_

#include "mozilla/Try.h"
#include "mozilla/intl/ICU4CGlue.h"

#include "unicode/uidna.h"

namespace mozilla::intl {

/**
 * This component is a Mozilla-focused API for the Internationalizing Domain
 * Names in Applications (IDNA).
 *
 * See UTS #46 for details.
 * http://unicode.org/reports/tr46/
 */
class IDNA final {
 public:
  ~IDNA();

  /**
   * UTS #46 specifies two specific types of processing: Transitional Processing
   * and NonTransitional Processing.
   *
   * See http://unicode.org/reports/tr46/#Compatibility_Processing
   */
  enum class ProcessingType {
    Transitional,
    NonTransitional,
  };

  /**
   * Create an IDNA object, with specifying the type of processing by enum
   * ProcessingType.
   *
   * Currently the implementation enables CheckBidi flag and CheckJoiners by
   * default.
   *
   * See UTS #46, '4 Processing' for details.
   * http://unicode.org/reports/tr46/#Processing
   */
  static Result<UniquePtr<IDNA>, ICUError> TryCreate(
      ProcessingType aProcessing);

  /**
   * This class contains the error code information of IDNA processing.
   */
  class Info final {
   public:
    /**
     * Check if there's any error.
     */
    bool HasErrors() const { return mErrorCode != 0; }

    /**
     * If the domain name label starts with "xn--", then the label contains
     * Punycode. This checks if the domain name label has invalid Punycode.
     *
     * See https://www.rfc-editor.org/rfc/rfc3492.html
     */
    bool HasInvalidPunycode() const {
      return (mErrorCode & UIDNA_ERROR_PUNYCODE) != 0;
    }

    /* The label was successfully ACE (Punycode) decoded but the resulting
     * string had severe validation errors. For example,
     * it might contain characters that are not allowed in ACE labels,
     * or it might not be normalized.
     */
    bool HasInvalidAceLabel() const {
      return (mErrorCode & UIDNA_ERROR_INVALID_ACE_LABEL) != 0;
    }

    /**
     * Checks if the domain name label has any invalid hyphen characters.
     *
     * See CheckHyphens flag for details in UTS #46[1].
     * - The label must not contain a U+002D HYPHEN-MINUS character in both the
     *   third and fourth positions.
     * - The label must neither begin nor end with a U+002D HYPHEN-MINUS
     *   character.
     *
     * [1]: http://unicode.org/reports/tr46/#Validity_Criteria
     */
    bool HasInvalidHyphen() const {
      uint32_t hyphenErrors = UIDNA_ERROR_LEADING_HYPHEN |
                              UIDNA_ERROR_TRAILING_HYPHEN |
                              UIDNA_ERROR_HYPHEN_3_4;
      return (mErrorCode & hyphenErrors) != 0;
    }

    bool HasErrorsIgnoringInvalidHyphen() const {
      uint32_t hyphenErrors = UIDNA_ERROR_LEADING_HYPHEN |
                              UIDNA_ERROR_TRAILING_HYPHEN |
                              UIDNA_ERROR_HYPHEN_3_4;
      return (mErrorCode & ~hyphenErrors) != 0;
    }

   private:
    friend class IDNA;
    explicit Info(const UIDNAInfo* aUinfo) : mErrorCode(aUinfo->errors) {}

    uint32_t mErrorCode = 0;
  };

  /**
   * Converts a domain name label to its Unicode form for human-readable
   * display, and writes the Unicode form into buffer, and returns IDNA::Info
   * object.
   * The IDNA::Info object contains the detail information about the processing
   * result of IDNA call, caller should check the result by calling
   * IDNA::Info::HasErrors() as well.
   */
  template <typename Buffer>
  Result<Info, ICUError> LabelToUnicode(Span<const char16_t> aLabel,
                                        Buffer& aBuffer) {
    UIDNAInfo uinfo = UIDNA_INFO_INITIALIZER;
    MOZ_TRY(FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          return uidna_labelToUnicode(mIDNA.GetConst(), aLabel.data(),
                                      aLabel.size(), target, length, &uinfo,
                                      status);
        }));

    return Info{&uinfo};
  }

 private:
  explicit IDNA(UIDNA* aIDNA) : mIDNA(aIDNA) {}

  ICUPointer<UIDNA> mIDNA = ICUPointer<UIDNA>(nullptr);
};
}  // namespace mozilla::intl
#endif  // intl_components_IDNA_h_
