/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_ListFormat_h_
#define intl_components_ListFormat_h_

#include "mozilla/CheckedInt.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Try.h"
#include "mozilla/Vector.h"
#include "unicode/ulistformatter.h"

struct UListFormatter;

namespace mozilla::intl {

static constexpr size_t DEFAULT_LIST_LENGTH = 8;

/**
 * This component is a Mozilla-focused API for the list formatting provided by
 * ICU. It implements the API provided by the ECMA-402 Intl.ListFormat object.
 *
 * https://tc39.es/ecma402/#listformat-objects
 */
class ListFormat final {
 public:
  /**
   * The [[Type]] and [[Style]] properties of ListFormat instances.
   *
   * https://tc39.es/ecma402/#sec-properties-of-intl-listformat-instances
   */
  // [[Type]]
  enum class Type { Conjunction, Disjunction, Unit };
  // [[Style]]
  enum class Style { Long, Short, Narrow };

  /**
   * The 'options' object to create Intl.ListFormat instance.
   *
   * https://tc39.es/ecma402/#sec-Intl.ListFormat
   */
  struct Options {
    // "conjunction" is the default fallback value.
    Type mType = Type::Conjunction;

    // "long" is the default fallback value.
    Style mStyle = Style::Long;
  };

  /**
   * Create a ListFormat object for the provided locale and options.
   *
   * https://tc39.es/ecma402/#sec-Intl.ListFormat
   */
  static Result<UniquePtr<ListFormat>, ICUError> TryCreate(
      mozilla::Span<const char> aLocale, const Options& aOptions);

  ~ListFormat();

  /**
   * The list of String values for FormatList and FormatListToParts.
   *
   * https://tc39.es/ecma402/#sec-formatlist
   * https://tc39.es/ecma402/#sec-formatlisttoparts
   */
  using StringList =
      mozilla::Vector<mozilla::Span<const char16_t>, DEFAULT_LIST_LENGTH>;

  /**
   * Format the list according and write the result in buffer.
   *
   * https://tc39.es/ecma402/#sec-Intl.ListFormat.prototype.format
   * https://tc39.es/ecma402/#sec-formatlist
   */
  template <typename Buffer>
  ICUResult Format(const StringList& list, Buffer& buffer) const {
    static_assert(std::is_same_v<typename Buffer::CharType, char16_t>,
                  "Currently only UTF-16 buffers are supported.");

    mozilla::Vector<const char16_t*, DEFAULT_LIST_LENGTH> u16strings;
    mozilla::Vector<int32_t, DEFAULT_LIST_LENGTH> u16stringLens;
    MOZ_TRY(ConvertStringListToVectors(list, u16strings, u16stringLens));

    int32_t u16stringCount = mozilla::AssertedCast<int32_t>(list.length());
    MOZ_TRY(FillBufferWithICUCall(
        buffer, [this, &u16strings, &u16stringLens, u16stringCount](
                    char16_t* chars, int32_t size, UErrorCode* status) {
          return ulistfmt_format(mListFormatter.GetConst(), u16strings.begin(),
                                 u16stringLens.begin(), u16stringCount, chars,
                                 size, status);
        }));

    return Ok{};
  }

  /**
   * The corresponding list of parts according to the effective locale and the
   * formatting options of ListFormat.
   * Each part has a [[Type]] field, which must be "element" or "literal", and a
   * [[Value]] field.
   *
   * To store Part more efficiently, it doesn't store the ||Value|| of type
   * string in this struct. Instead, it stores the end index of the string in
   * the buffer(which is passed to ListFormat::FormatToParts()). The begin index
   * of the ||Value|| is the index of the previous part.
   *
   *  Buffer
   *  0               i                j
   * +---------------+---------------+---------------+
   * | Part[0].Value | Part[1].Value | Part[2].Value | ....
   * +---------------+---------------+---------------+
   *
   *     Part[0].index is i. Part[0].Value is stored in the Buffer[0..i].
   *     Part[1].index is j. Part[1].Value is stored in the Buffer[i..j].
   *
   * See https://tc39.es/ecma402/#sec-createpartsfromlist
   */
  enum class PartType {
    Element,
    Literal,
  };
  // The 2nd field is the end index to the buffer as mentioned above.
  using Part = std::pair<PartType, size_t>;
  using PartVector = mozilla::Vector<Part, DEFAULT_LIST_LENGTH>;

  /**
   * Format the list to a list of parts, and store the formatted result of
   * UTF-16 string into buffer, and formatted parts into the vector 'parts'.
   *
   * See:
   * https://tc39.es/ecma402/#sec-Intl.ListFormat.prototype.formatToParts
   * https://tc39.es/ecma402/#sec-formatlisttoparts
   */
  template <typename Buffer>
  ICUResult FormatToParts(const StringList& list, Buffer& buffer,
                          PartVector& parts) {
    static_assert(std::is_same_v<typename Buffer::CharType, char16_t>,
                  "Currently only UTF-16 buffers are supported.");

    mozilla::Vector<const char16_t*, DEFAULT_LIST_LENGTH> u16strings;
    mozilla::Vector<int32_t, DEFAULT_LIST_LENGTH> u16stringLens;
    MOZ_TRY(ConvertStringListToVectors(list, u16strings, u16stringLens));

    AutoFormattedList formatted;
    UErrorCode status = U_ZERO_ERROR;
    ulistfmt_formatStringsToResult(
        mListFormatter.GetConst(), u16strings.begin(), u16stringLens.begin(),
        int32_t(list.length()), formatted.GetFormatted(), &status);
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }

    auto spanResult = formatted.ToSpan();
    if (spanResult.isErr()) {
      return spanResult.propagateErr();
    }
    auto formattedSpan = spanResult.unwrap();
    if (!FillBuffer(formattedSpan, buffer)) {
      return Err(ICUError::OutOfMemory);
    }

    const UFormattedValue* value = formatted.Value();
    if (!value) {
      return Err(ICUError::InternalError);
    }
    return FormattedToParts(value, buffer.length(), parts);
  }

 private:
  ListFormat() = delete;
  explicit ListFormat(UListFormatter* fmt) : mListFormatter(fmt) {}
  ListFormat(const ListFormat&) = delete;
  ListFormat& operator=(const ListFormat&) = delete;

  ICUPointer<UListFormatter> mListFormatter =
      ICUPointer<UListFormatter>(nullptr);

  // Convert StringList to an array of type 'const char16_t*' and an array of
  // int32 for ICU-API.
  ICUResult ConvertStringListToVectors(
      const StringList& list,
      mozilla::Vector<const char16_t*, DEFAULT_LIST_LENGTH>& u16strings,
      mozilla::Vector<int32_t, DEFAULT_LIST_LENGTH>& u16stringLens) const {
    // Keep a conservative running count of overall length.
    mozilla::CheckedInt<int32_t> stringLengthTotal(0);
    for (const auto& string : list) {
      if (!u16strings.append(string.data())) {
        return Err(ICUError::InternalError);
      }

      int32_t len = mozilla::AssertedCast<int32_t>(string.size());
      if (!u16stringLens.append(len)) {
        return Err(ICUError::InternalError);
      }

      stringLengthTotal += len;
    }

    // Add space for N unrealistically large conjunctions.
    constexpr int32_t MaxConjunctionLen = 100;
    stringLengthTotal += CheckedInt<int32_t>(list.length()) * MaxConjunctionLen;
    // If the overestimate exceeds ICU length limits, don't try to format.
    if (!stringLengthTotal.isValid()) {
      return Err(ICUError::OverflowError);
    }

    return Ok{};
  }

  using AutoFormattedList =
      AutoFormattedResult<UFormattedList, ulistfmt_openResult,
                          ulistfmt_resultAsValue, ulistfmt_closeResult>;

  ICUResult FormattedToParts(const UFormattedValue* formattedValue,
                             size_t formattedSize, PartVector& parts);

  static UListFormatterType ToUListFormatterType(Type type);
  static UListFormatterWidth ToUListFormatterWidth(Style style);
};

}  // namespace mozilla::intl
#endif  // intl_components_ListFormat_h_
