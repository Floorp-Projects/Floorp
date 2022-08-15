/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/Locale.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Variant.h"

#include "ICU4CGlue.h"

#include <algorithm>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string.h>
#include <type_traits>
#include <utility>

#include "unicode/uloc.h"
#include "unicode/utypes.h"

namespace mozilla::intl {

using namespace intl::LanguageTagLimits;

template <typename CharT>
bool IsStructurallyValidLanguageTag(Span<const CharT> aLanguage) {
  // unicode_language_subtag = alpha{2,3} | alpha{5,8};
  size_t length = aLanguage.size();
  const CharT* str = aLanguage.data();
  return ((2 <= length && length <= 3) || (5 <= length && length <= 8)) &&
         std::all_of(str, str + length, IsAsciiAlpha<CharT>);
}

template bool IsStructurallyValidLanguageTag(Span<const char> aLanguage);
template bool IsStructurallyValidLanguageTag(Span<const Latin1Char> aLanguage);
template bool IsStructurallyValidLanguageTag(Span<const char16_t> aLanguage);

template <typename CharT>
bool IsStructurallyValidScriptTag(Span<const CharT> aScript) {
  // unicode_script_subtag = alpha{4} ;
  size_t length = aScript.size();
  const CharT* str = aScript.data();
  return length == 4 && std::all_of(str, str + length, IsAsciiAlpha<CharT>);
}

template bool IsStructurallyValidScriptTag(Span<const char> aScript);
template bool IsStructurallyValidScriptTag(Span<const Latin1Char> aScript);
template bool IsStructurallyValidScriptTag(Span<const char16_t> aScript);

template <typename CharT>
bool IsStructurallyValidRegionTag(Span<const CharT> aRegion) {
  // unicode_region_subtag = (alpha{2} | digit{3}) ;
  size_t length = aRegion.size();
  const CharT* str = aRegion.data();
  return (length == 2 && std::all_of(str, str + length, IsAsciiAlpha<CharT>)) ||
         (length == 3 && std::all_of(str, str + length, IsAsciiDigit<CharT>));
}

template bool IsStructurallyValidRegionTag(Span<const char> aRegion);
template bool IsStructurallyValidRegionTag(Span<const Latin1Char> aRegion);
template bool IsStructurallyValidRegionTag(Span<const char16_t> aRegion);

#ifdef DEBUG
bool IsStructurallyValidVariantTag(Span<const char> aVariant) {
  // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
  size_t length = aVariant.size();
  const char* str = aVariant.data();
  return ((5 <= length && length <= 8) ||
          (length == 4 && IsAsciiDigit(str[0]))) &&
         std::all_of(str, str + length, IsAsciiAlphanumeric<char>);
}

bool IsStructurallyValidUnicodeExtensionTag(Span<const char> aExtension) {
  return LocaleParser::CanParseUnicodeExtension(aExtension).isOk();
}

static bool IsStructurallyValidExtensionTag(Span<const char> aExtension) {
  // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
  // NB: Allow any extension, including Unicode and Transform here, because
  // this function is only used for an assertion.

  size_t length = aExtension.size();
  const char* str = aExtension.data();
  const char* const end = aExtension.data() + length;
  if (length <= 2) {
    return false;
  }
  if (!IsAsciiAlphanumeric(str[0]) || str[0] == 'x' || str[0] == 'X') {
    return false;
  }
  str++;
  if (*str++ != '-') {
    return false;
  }
  while (true) {
    const char* sep =
        reinterpret_cast<const char*>(memchr(str, '-', end - str));
    size_t len = (sep ? sep : end) - str;
    if (len < 2 || len > 8 ||
        !std::all_of(str, str + len, IsAsciiAlphanumeric<char>)) {
      return false;
    }
    if (!sep) {
      return true;
    }
    str = sep + 1;
  }
}

bool IsStructurallyValidPrivateUseTag(Span<const char> aPrivateUse) {
  // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;

  size_t length = aPrivateUse.size();
  const char* str = aPrivateUse.data();
  const char* const end = aPrivateUse.data() + length;
  if (length <= 2) {
    return false;
  }
  if (str[0] != 'x' && str[0] != 'X') {
    return false;
  }
  str++;
  if (*str++ != '-') {
    return false;
  }
  while (true) {
    const char* sep =
        reinterpret_cast<const char*>(memchr(str, '-', end - str));
    size_t len = (sep ? sep : end) - str;
    if (len == 0 || len > 8 ||
        !std::all_of(str, str + len, IsAsciiAlphanumeric<char>)) {
      return false;
    }
    if (!sep) {
      return true;
    }
    str = sep + 1;
  }
}
#endif

ptrdiff_t Locale::UnicodeExtensionIndex() const {
  // The extension subtags aren't necessarily sorted, so we can't use binary
  // search here.
  auto p = std::find_if(
      mExtensions.begin(), mExtensions.end(),
      [](const auto& ext) { return ext[0] == 'u' || ext[0] == 'U'; });
  if (p != mExtensions.end()) {
    return std::distance(mExtensions.begin(), p);
  }
  return -1;
}

Maybe<Span<const char>> Locale::GetUnicodeExtension() const {
  ptrdiff_t index = UnicodeExtensionIndex();
  if (index >= 0) {
    return Some(MakeStringSpan(mExtensions[index].get()));
  }
  return Nothing();
}

ICUResult Locale::SetUnicodeExtension(Span<const char> aExtension) {
  MOZ_ASSERT(IsStructurallyValidUnicodeExtensionTag(aExtension));

  auto duplicated = DuplicateStringToUniqueChars(aExtension);

  // Replace the existing Unicode extension subtag or append a new one.
  ptrdiff_t index = UnicodeExtensionIndex();
  if (index >= 0) {
    mExtensions[index] = std::move(duplicated);
    return Ok();
  }
  if (!mExtensions.append(std::move(duplicated))) {
    return Err(ICUError::OutOfMemory);
  }
  return Ok();
}

void Locale::ClearUnicodeExtension() {
  ptrdiff_t index = UnicodeExtensionIndex();
  if (index >= 0) {
    mExtensions.erase(mExtensions.begin() + index);
  }
}

template <size_t InitialCapacity>
static bool SortAlphabetically(Vector<UniqueChars, InitialCapacity>& aSubtags) {
  size_t length = aSubtags.length();

  // Zero or one element lists are already sorted.
  if (length < 2) {
    return true;
  }

  // Handle two element lists inline.
  if (length == 2) {
    if (strcmp(aSubtags[0].get(), aSubtags[1].get()) > 0) {
      aSubtags[0].swap(aSubtags[1]);
    }
    return true;
  }

  Vector<char*, 8> scratch;
  if (!scratch.resizeUninitialized(length)) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    scratch[i] = aSubtags[i].release();
  }

  std::stable_sort(
      scratch.begin(), scratch.end(),
      [](const char* a, const char* b) { return strcmp(a, b) < 0; });

  for (size_t i = 0; i < length; i++) {
    aSubtags[i] = UniqueChars(scratch[i]);
  }
  return true;
}

Result<Ok, Locale::CanonicalizationError> Locale::CanonicalizeBaseName() {
  // Per 6.2.3 CanonicalizeUnicodeLocaleId, the very first step is to
  // canonicalize the syntax by normalizing the case and ordering all subtags.
  // The canonical syntax form is specified in UTS 35, 3.2.1.

  // Language codes need to be in lower case. "JA" -> "ja"
  mLanguage.ToLowerCase();
  MOZ_ASSERT(IsStructurallyValidLanguageTag(Language().Span()));

  // The first character of a script code needs to be capitalized.
  // "hans" -> "Hans"
  mScript.ToTitleCase();
  MOZ_ASSERT(Script().Missing() ||
             IsStructurallyValidScriptTag(Script().Span()));

  // Region codes need to be in upper case. "bu" -> "BU"
  mRegion.ToUpperCase();
  MOZ_ASSERT(Region().Missing() ||
             IsStructurallyValidRegionTag(Region().Span()));

  // The canonical case for variant subtags is lowercase.
  for (UniqueChars& variant : mVariants) {
    char* variantChars = variant.get();
    size_t variantLength = strlen(variantChars);
    AsciiToLowerCase(variantChars, variantLength, variantChars);

    MOZ_ASSERT(IsStructurallyValidVariantTag({variantChars, variantLength}));
  }

  // Extensions and privateuse subtags are case normalized in the
  // |canonicalizeExtensions| method.

  // The second step in UTS 35, 3.2.1, is to order all subtags.

  if (mVariants.length() > 1) {
    // 1. Any variants are in alphabetical order.
    if (!SortAlphabetically(mVariants)) {
      return Err(CanonicalizationError::OutOfMemory);
    }

    // Reject the Locale identifier if a duplicate variant was found, e.g.
    // "en-variant-Variant".
    const UniqueChars* duplicate = std::adjacent_find(
        mVariants.begin(), mVariants.end(), [](const auto& a, const auto& b) {
          return strcmp(a.get(), b.get()) == 0;
        });
    if (duplicate != mVariants.end()) {
      return Err(CanonicalizationError::DuplicateVariant);
    }
  }

  // 2. Any extensions are in alphabetical order by their singleton.
  // 3. All attributes are sorted in alphabetical order.
  // 4. All keywords and tfields are sorted by alphabetical order of their keys,
  //    within their respective extensions.
  // 5. Any type or tfield value "true" is removed.
  // - A subsequent call to canonicalizeExtensions() will perform these steps.

  // 6.2.3 CanonicalizeUnicodeLocaleId, step 2 transforms the locale identifier
  // into its canonical form per UTS 3.2.1.

  // 1. Use the bcp47 data to replace keys, types, tfields, and tvalues by their
  // canonical forms.
  // - A subsequent call to canonicalizeExtensions() will perform this step.

  // 2. Replace aliases in the unicode_language_id and tlang (if any).
  // - tlang is handled in canonicalizeExtensions().

  // Replace deprecated language, region, and variant subtags with their
  // preferred mappings.

  if (!UpdateLegacyMappings()) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  // Replace deprecated language subtags with their preferred values.
  if (!LanguageMapping(mLanguage) && ComplexLanguageMapping(mLanguage)) {
    PerformComplexLanguageMappings();
  }

  // Replace deprecated script subtags with their preferred values.
  if (Script().Present()) {
    ScriptMapping(mScript);
  }

  // Replace deprecated region subtags with their preferred values.
  if (Region().Present()) {
    if (!RegionMapping(mRegion) && ComplexRegionMapping(mRegion)) {
      PerformComplexRegionMappings();
    }
  }

  // Replace deprecated variant subtags with their preferred values.
  if (!PerformVariantMappings()) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  // No extension replacements are currently present.
  // Private use sequences are left as is.

  // 3. Replace aliases in special key values.
  // - A subsequent call to canonicalizeExtensions() will perform this step.

  return Ok();
}

#ifdef DEBUG
static bool IsAsciiLowercaseAlphanumericOrDash(Span<const char> aSpan) {
  const char* ptr = aSpan.data();
  size_t length = aSpan.size();
  return std::all_of(ptr, ptr + length, [](auto c) {
    return IsAsciiLowercaseAlpha(c) || IsAsciiDigit(c) || c == '-';
  });
}
#endif

Result<Ok, Locale::CanonicalizationError> Locale::CanonicalizeExtensions() {
  // The canonical case for all extension subtags is lowercase.
  for (UniqueChars& extension : mExtensions) {
    char* extensionChars = extension.get();
    size_t extensionLength = strlen(extensionChars);
    AsciiToLowerCase(extensionChars, extensionLength, extensionChars);

    MOZ_ASSERT(
        IsStructurallyValidExtensionTag({extensionChars, extensionLength}));
  }

  // Any extensions are in alphabetical order by their singleton.
  // "u-ca-chinese-t-zh-latn" -> "t-zh-latn-u-ca-chinese"
  if (!SortAlphabetically(mExtensions)) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  for (UniqueChars& extension : mExtensions) {
    if (extension[0] == 'u') {
      MOZ_TRY(CanonicalizeUnicodeExtension(extension));
    } else if (extension[0] == 't') {
      MOZ_TRY(CanonicalizeTransformExtension(extension));
    }

    MOZ_ASSERT(
        IsAsciiLowercaseAlphanumericOrDash(MakeStringSpan(extension.get())));
  }

  // The canonical case for privateuse subtags is lowercase.
  if (char* privateuse = mPrivateUse.get()) {
    size_t privateuseLength = strlen(privateuse);
    AsciiToLowerCase(privateuse, privateuseLength, privateuse);

    MOZ_ASSERT(
        IsStructurallyValidPrivateUseTag({privateuse, privateuseLength}));
  }
  return Ok();
}

template <size_t N>
static inline bool AppendSpan(Vector<char, N>& vector, Span<const char> aSpan) {
  return vector.append(aSpan.data(), aSpan.size());
}

/**
 * CanonicalizeUnicodeExtension( attributes, keywords )
 *
 * Canonical syntax per
 * <https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers>:
 *
 * - All attributes and keywords are in lowercase.
 *   - Note: The parser already converted keywords to lowercase.
 * - All attributes are sorted in alphabetical order.
 * - All keywords are sorted by alphabetical order of their keys.
 * - Any type value "true" is removed.
 *
 * Canonical form:
 * - All keys and types use the canonical form (from the name attribute;
 *   see Section 3.6.4 U Extension Data Files).
 */
Result<Ok, Locale::CanonicalizationError> Locale::CanonicalizeUnicodeExtension(
    UniqueChars& aUnicodeExtension) {
  Span<const char> extension = MakeStringSpan(aUnicodeExtension.get());
  MOZ_ASSERT(extension[0] == 'u');
  MOZ_ASSERT(extension[1] == '-');
  MOZ_ASSERT(IsStructurallyValidExtensionTag(extension));

  LocaleParser::AttributesVector attributes;
  LocaleParser::KeywordsVector keywords;

  using Attribute = LocaleParser::AttributesVector::ElementType;
  using Keyword = LocaleParser::KeywordsVector::ElementType;

  if (LocaleParser::ParseUnicodeExtension(extension, attributes, keywords)
          .isErr()) {
    MOZ_ASSERT_UNREACHABLE("unexpected invalid Unicode extension subtag");
    return Err(CanonicalizationError::InternalError);
  }

  auto attributesLess = [extension](const Attribute& a, const Attribute& b) {
    auto astr = extension.Subspan(a.Begin(), a.Length());
    auto bstr = extension.Subspan(b.Begin(), b.Length());
    return astr < bstr;
  };

  // All attributes are sorted in alphabetical order.
  if (attributes.length() > 1) {
    std::stable_sort(attributes.begin(), attributes.end(), attributesLess);
  }

  auto keywordsLess = [extension](const Keyword& a, const Keyword& b) {
    auto astr = extension.Subspan(a.Begin(), UnicodeKeyLength);
    auto bstr = extension.Subspan(b.Begin(), UnicodeKeyLength);
    return astr < bstr;
  };

  // All keywords are sorted by alphabetical order of keys.
  if (keywords.length() > 1) {
    // Using a stable sort algorithm, guarantees that two keywords using the
    // same key are never reordered. That means for example
    // when we have the input "u-nu-thai-kf-false-nu-latn", we are guaranteed to
    // get the result "u-kf-false-nu-thai-nu-latn", i.e. "nu-thai" still occurs
    // before "nu-latn".
    // This is required so that deduplication below preserves the first keyword
    // for a given key and discards the rest.
    std::stable_sort(keywords.begin(), keywords.end(), keywordsLess);
  }

  Vector<char, 32> sb;
  if (!sb.append('u')) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  // Append all Unicode extension attributes.
  for (size_t i = 0; i < attributes.length(); i++) {
    const auto& attribute = attributes[i];
    auto span = extension.Subspan(attribute.Begin(), attribute.Length());

    // Skip duplicate attributes.
    if (i > 0) {
      const auto& lastAttribute = attributes[i - 1];
      if (span ==
          extension.Subspan(lastAttribute.Begin(), lastAttribute.Length())) {
        continue;
      }
      MOZ_ASSERT(attributesLess(lastAttribute, attribute));
    }

    if (!sb.append('-')) {
      return Err(CanonicalizationError::OutOfMemory);
    }
    if (!AppendSpan(sb, span)) {
      return Err(CanonicalizationError::OutOfMemory);
    }
  }

  static constexpr size_t UnicodeKeyWithSepLength = UnicodeKeyLength + 1;

  using StringSpan = Span<const char>;

  static constexpr StringSpan True = MakeStringSpan("true");

  // Append all Unicode extension keywords.
  for (size_t i = 0; i < keywords.length(); i++) {
    const auto& keyword = keywords[i];

    // Skip duplicate keywords.
    if (i > 0) {
      const auto& lastKeyword = keywords[i - 1];
      if (extension.Subspan(keyword.Begin(), UnicodeKeyLength) ==
          extension.Subspan(lastKeyword.Begin(), UnicodeKeyLength)) {
        continue;
      }
      MOZ_ASSERT(keywordsLess(lastKeyword, keyword));
    }

    if (!sb.append('-')) {
      return Err(CanonicalizationError::OutOfMemory);
    }

    StringSpan span = extension.Subspan(keyword.Begin(), keyword.Length());
    if (span.size() == UnicodeKeyLength) {
      // Keyword without type value.
      if (!AppendSpan(sb, span)) {
        return Err(CanonicalizationError::OutOfMemory);
      }
    } else {
      StringSpan key = span.To(UnicodeKeyLength);
      StringSpan type = span.From(UnicodeKeyWithSepLength);

      // Search if there's a replacement for the current Unicode keyword.
      if (const char* replacement = ReplaceUnicodeExtensionType(key, type)) {
        StringSpan repl = MakeStringSpan(replacement);
        if (repl == True) {
          // Elide the type "true" if present in the replacement.
          if (!AppendSpan(sb, key)) {
            return Err(CanonicalizationError::OutOfMemory);
          }
        } else {
          // Otherwise append the Unicode key (including the separator) and the
          // replaced type.
          if (!AppendSpan(sb, span.To(UnicodeKeyWithSepLength))) {
            return Err(CanonicalizationError::OutOfMemory);
          }
          if (!AppendSpan(sb, repl)) {
            return Err(CanonicalizationError::OutOfMemory);
          }
        }
      } else {
        if (type == True) {
          // Elide the Unicode extension type "true".
          if (!AppendSpan(sb, key)) {
            return Err(CanonicalizationError::OutOfMemory);
          }
        } else {
          // Otherwise append the complete Unicode extension keyword.
          if (!AppendSpan(sb, span)) {
            return Err(CanonicalizationError::OutOfMemory);
          }
        }
      }
    }
  }

  // We can keep the previous extension when canonicalization didn't modify it.
  if (static_cast<Span<const char>>(sb) != extension) {
    // Null-terminate the new string and replace the previous extension.
    if (!sb.append('\0')) {
      return Err(CanonicalizationError::OutOfMemory);
    }
    UniqueChars canonical(sb.extractOrCopyRawBuffer());
    if (!canonical) {
      return Err(CanonicalizationError::OutOfMemory);
    }
    aUnicodeExtension = std::move(canonical);
  }

  return Ok();
}

template <class Buffer>
static bool LocaleToString(const Locale& aTag, Buffer& aBuffer) {
  auto appendSubtag = [&aBuffer](const auto& subtag) {
    auto span = subtag.Span();
    MOZ_ASSERT(!span.empty());
    return aBuffer.append(span.data(), span.size());
  };

  auto appendSubtagSpan = [&aBuffer](Span<const char> subtag) {
    MOZ_ASSERT(!subtag.empty());
    return aBuffer.append(subtag.data(), subtag.size());
  };

  auto appendSubtags = [&aBuffer, &appendSubtagSpan](const auto& subtags) {
    for (const auto& subtag : subtags) {
      if (!aBuffer.append('-') || !appendSubtagSpan(subtag)) {
        return false;
      }
    }
    return true;
  };

  // Append the language subtag.
  if (!appendSubtag(aTag.Language())) {
    return false;
  }

  // Append the script subtag if present.
  if (aTag.Script().Present()) {
    if (!aBuffer.append('-') || !appendSubtag(aTag.Script())) {
      return false;
    }
  }

  // Append the region subtag if present.
  if (aTag.Region().Present()) {
    if (!aBuffer.append('-') || !appendSubtag(aTag.Region())) {
      return false;
    }
  }

  // Append the variant subtags if present.
  if (!appendSubtags(aTag.Variants())) {
    return false;
  }

  // Append the extensions subtags if present.
  if (!appendSubtags(aTag.Extensions())) {
    return false;
  }

  // Append the private-use subtag if present.
  if (auto privateuse = aTag.PrivateUse()) {
    if (!aBuffer.append('-') || !appendSubtagSpan(privateuse.value())) {
      return false;
    }
  }

  return true;
}

/**
 * CanonicalizeTransformExtension
 *
 * Canonical form per <https://unicode.org/reports/tr35/#BCP47_T_Extension>:
 *
 * - These subtags are all in lowercase (that is the canonical casing for these
 *   subtags), [...].
 *
 * And per
 * <https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers>:
 *
 * - All keywords and tfields are sorted by alphabetical order of their keys,
 *   within their respective extensions.
 */
Result<Ok, Locale::CanonicalizationError>
Locale::CanonicalizeTransformExtension(UniqueChars& aTransformExtension) {
  Span<const char> extension = MakeStringSpan(aTransformExtension.get());
  MOZ_ASSERT(extension[0] == 't');
  MOZ_ASSERT(extension[1] == '-');
  MOZ_ASSERT(IsStructurallyValidExtensionTag(extension));

  Locale tag;
  LocaleParser::TFieldVector fields;

  using TField = LocaleParser::TFieldVector::ElementType;

  if (LocaleParser::ParseTransformExtension(extension, tag, fields).isErr()) {
    MOZ_ASSERT_UNREACHABLE("unexpected invalid transform extension subtag");
    return Err(CanonicalizationError::InternalError);
  }

  auto tfieldLess = [extension](const TField& a, const TField& b) {
    auto astr = extension.Subspan(a.Begin(), TransformKeyLength);
    auto bstr = extension.Subspan(b.Begin(), TransformKeyLength);
    return astr < bstr;
  };

  // All tfields are sorted by alphabetical order of their keys.
  if (fields.length() > 1) {
    std::stable_sort(fields.begin(), fields.end(), tfieldLess);
  }

  Vector<char, 32> sb;
  if (!sb.append('t')) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  // Append the language subtag if present.
  //
  // Replace aliases in tlang per
  // <https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers>.
  if (tag.Language().Present()) {
    if (!sb.append('-')) {
      return Err(CanonicalizationError::OutOfMemory);
    }

    MOZ_TRY(tag.CanonicalizeBaseName());

    // The canonical case for Transform extensions is lowercase per
    // <https://unicode.org/reports/tr35/#BCP47_T_Extension>. Convert the two
    // subtags which don't use lowercase for their canonical syntax.
    tag.mScript.ToLowerCase();
    tag.mRegion.ToLowerCase();

    if (!LocaleToString(tag, sb)) {
      return Err(CanonicalizationError::OutOfMemory);
    }
  }

  static constexpr size_t TransformKeyWithSepLength = TransformKeyLength + 1;

  using StringSpan = Span<const char>;

  // Append all fields.
  //
  // UTS 35, 3.2.1 specifies:
  // - Any type or tfield value "true" is removed.
  //
  // But the `tvalue` subtag is mandatory in `tfield: tkey tvalue`, so ignore
  // this apparently invalid part of the UTS 35 specification and simply
  // append all `tfield` subtags.
  for (const auto& field : fields) {
    if (!sb.append('-')) {
      return Err(CanonicalizationError::OutOfMemory);
    }

    StringSpan span = extension.Subspan(field.Begin(), field.Length());
    StringSpan key = span.To(TransformKeyLength);
    StringSpan value = span.From(TransformKeyWithSepLength);

    // Search if there's a replacement for the current transform keyword.
    if (const char* replacement = ReplaceTransformExtensionType(key, value)) {
      if (!AppendSpan(sb, span.To(TransformKeyWithSepLength))) {
        return Err(CanonicalizationError::OutOfMemory);
      }
      if (!AppendSpan(sb, MakeStringSpan(replacement))) {
        return Err(CanonicalizationError::OutOfMemory);
      }
    } else {
      if (!AppendSpan(sb, span)) {
        return Err(CanonicalizationError::OutOfMemory);
      }
    }
  }

  // We can keep the previous extension when canonicalization didn't modify it.
  if (static_cast<Span<const char>>(sb) != extension) {
    // Null-terminate the new string and replace the previous extension.
    if (!sb.append('\0')) {
      return Err(CanonicalizationError::OutOfMemory);
    }
    UniqueChars canonical(sb.extractOrCopyRawBuffer());
    if (!canonical) {
      return Err(CanonicalizationError::OutOfMemory);
    }
    aTransformExtension = std::move(canonical);
  }

  return Ok();
}

// Zero-terminated ICU Locale ID.
using LocaleId =
    Vector<char, LanguageLength + 1 + ScriptLength + 1 + RegionLength + 1>;

enum class LikelySubtags : bool { Add, Remove };

// Return true iff the locale is already maximized resp. minimized.
static bool HasLikelySubtags(LikelySubtags aLikelySubtags, const Locale& aTag) {
  // The locale is already maximized if the language, script, and region
  // subtags are present and no placeholder subtags ("und", "Zzzz", "ZZ") are
  // used.
  if (aLikelySubtags == LikelySubtags::Add) {
    return !aTag.Language().EqualTo("und") &&
           (aTag.Script().Present() && !aTag.Script().EqualTo("Zzzz")) &&
           (aTag.Region().Present() && !aTag.Region().EqualTo("ZZ"));
  }

  // The locale is already minimized if it only contains a language
  // subtag whose value is not the placeholder value "und".
  return !aTag.Language().EqualTo("und") && aTag.Script().Missing() &&
         aTag.Region().Missing();
}

// Create an ICU locale ID from the given locale.
static bool CreateLocaleForLikelySubtags(const Locale& aTag,
                                         LocaleId& aLocale) {
  MOZ_ASSERT(aLocale.length() == 0);

  auto appendSubtag = [&aLocale](const auto& subtag) {
    auto span = subtag.Span();
    MOZ_ASSERT(!span.empty());
    return aLocale.append(span.data(), span.size());
  };

  // Append the language subtag.
  if (!appendSubtag(aTag.Language())) {
    return false;
  }

  // Append the script subtag if present.
  if (aTag.Script().Present()) {
    if (!aLocale.append('_') || !appendSubtag(aTag.Script())) {
      return false;
    }
  }

  // Append the region subtag if present.
  if (aTag.Region().Present()) {
    if (!aLocale.append('_') || !appendSubtag(aTag.Region())) {
      return false;
    }
  }

  // Zero-terminated for use with ICU.
  return aLocale.append('\0');
}

static ICUError ParserErrorToICUError(LocaleParser::ParserError aErr) {
  using ParserError = LocaleParser::ParserError;

  switch (aErr) {
    case ParserError::NotParseable:
      return ICUError::InternalError;
    case ParserError::OutOfMemory:
      return ICUError::OutOfMemory;
  }
  MOZ_CRASH("Unexpected parser error");
}

static ICUError CanonicalizationErrorToICUError(
    Locale::CanonicalizationError aErr) {
  using CanonicalizationError = Locale::CanonicalizationError;

  switch (aErr) {
    case CanonicalizationError::DuplicateVariant:
    case CanonicalizationError::InternalError:
      return ICUError::InternalError;
    case CanonicalizationError::OutOfMemory:
      return ICUError::OutOfMemory;
  }
  MOZ_CRASH("Unexpected canonicalization error");
}

// Assign the language, script, and region subtags from an ICU locale ID.
//
// ICU provides |uloc_getLanguage|, |uloc_getScript|, and |uloc_getCountry| to
// retrieve these subtags, but unfortunately these functions are rather slow, so
// we use our own implementation.
static ICUResult AssignFromLocaleId(LocaleId& aLocaleId, Locale& aTag) {
  // Replace the ICU locale ID separator.
  std::replace(aLocaleId.begin(), aLocaleId.end(), '_', '-');

  // ICU replaces "und" with the empty string, which means "und" becomes "" and
  // "und-Latn" becomes "-Latn". Handle this case separately.
  if (aLocaleId.empty() || aLocaleId[0] == '-') {
    static constexpr auto und = MakeStringSpan("und");
    constexpr size_t length = und.size();

    // Insert "und" in front of the locale ID.
    if (!aLocaleId.growBy(length)) {
      return Err(ICUError::OutOfMemory);
    }
    memmove(aLocaleId.begin() + length, aLocaleId.begin(), aLocaleId.length());
    memmove(aLocaleId.begin(), und.data(), length);
  }

  // Retrieve the language, script, and region subtags from the locale ID
  Locale localeTag;
  MOZ_TRY(LocaleParser::TryParseBaseName(aLocaleId, localeTag)
              .mapErr(ParserErrorToICUError));

  aTag.SetLanguage(localeTag.Language());
  aTag.SetScript(localeTag.Script());
  aTag.SetRegion(localeTag.Region());

  return Ok();
}

template <decltype(uloc_addLikelySubtags) likelySubtagsFn>
static ICUResult CallLikelySubtags(const LocaleId& aLocaleId,
                                   LocaleId& aResult) {
  // Locale ID must be zero-terminated before passing it to ICU.
  MOZ_ASSERT(aLocaleId.back() == '\0');
  MOZ_ASSERT(aResult.length() == 0);

  // Ensure there's enough room for the result.
  MOZ_ALWAYS_TRUE(aResult.resize(LocaleId::InlineLength));

  return FillBufferWithICUCall(
      aResult, [&aLocaleId](char* chars, int32_t size, UErrorCode* status) {
        return likelySubtagsFn(aLocaleId.begin(), chars, size, status);
      });
}

// The canonical way to compute the Unicode BCP 47 locale identifier with likely
// subtags is as follows:
//
// 1. Call uloc_forLanguageTag() to transform the locale identifer into an ICU
//    locale ID.
// 2. Call uloc_addLikelySubtags() to add the likely subtags to the locale ID.
// 3. Call uloc_toLanguageTag() to transform the resulting locale ID back into
//    a Unicode BCP 47 locale identifier.
//
// Since uloc_forLanguageTag() and uloc_toLanguageTag() are both kind of slow
// and we know, by construction, that the input Unicode BCP 47 locale identifier
// only contains valid language, script, and region subtags, we can avoid both
// calls if we implement them ourselves, see CreateLocaleForLikelySubtags() and
// AssignFromLocaleId(). (Where "slow" means about 50% of the execution time of
// |Intl.Locale.prototype.maximize|.)
static ICUResult LikelySubtags(LikelySubtags aLikelySubtags, Locale& aTag) {
  // Return early if the input is already maximized/minimized.
  if (HasLikelySubtags(aLikelySubtags, aTag)) {
    return Ok();
  }

  // Create the locale ID for the input argument.
  LocaleId locale;
  if (!CreateLocaleForLikelySubtags(aTag, locale)) {
    return Err(ICUError::OutOfMemory);
  }

  // Either add or remove likely subtags to/from the locale ID.
  LocaleId localeLikelySubtags;
  if (aLikelySubtags == LikelySubtags::Add) {
    MOZ_TRY(
        CallLikelySubtags<uloc_addLikelySubtags>(locale, localeLikelySubtags));
  } else {
    MOZ_TRY(
        CallLikelySubtags<uloc_minimizeSubtags>(locale, localeLikelySubtags));
  }

  // Assign the language, script, and region subtags from the locale ID.
  MOZ_TRY(AssignFromLocaleId(localeLikelySubtags, aTag));

  // Update mappings in case ICU returned a non-canonical locale.
  MOZ_TRY(aTag.CanonicalizeBaseName().mapErr(CanonicalizationErrorToICUError));

  return Ok();
}

ICUResult Locale::AddLikelySubtags() {
  return LikelySubtags(LikelySubtags::Add, *this);
}

ICUResult Locale::RemoveLikelySubtags() {
  return LikelySubtags(LikelySubtags::Remove, *this);
}

UniqueChars Locale::DuplicateStringToUniqueChars(const char* aStr) {
  size_t length = strlen(aStr) + 1;
  auto duplicate = MakeUnique<char[]>(length);
  memcpy(duplicate.get(), aStr, length);
  return duplicate;
}

UniqueChars Locale::DuplicateStringToUniqueChars(Span<const char> aStr) {
  size_t length = aStr.size();
  auto duplicate = MakeUnique<char[]>(length + 1);
  memcpy(duplicate.get(), aStr.data(), length);
  duplicate[length] = '\0';
  return duplicate;
}

size_t Locale::ToStringCapacity() const {
  // This is a bit awkward, the buffer class currently does not support
  // being resized, so we need to calculate the required size up front and
  // reserve it all at once.
  auto lengthSubtag = [](const auto& subtag) {
    auto span = subtag.Span();
    MOZ_ASSERT(!span.empty());
    return span.size();
  };

  auto lengthSubtagZ = [](const char* subtag) {
    size_t length = strlen(subtag);
    MOZ_ASSERT(length > 0);
    return length;
  };

  auto lengthSubtagsZ = [&lengthSubtagZ](const auto& subtags) {
    size_t length = 0;
    for (const auto& subtag : subtags) {
      length += lengthSubtagZ(subtag.get()) + 1;
    }
    return length;
  };

  // First calculate required capacity
  size_t capacity = 0;

  capacity += lengthSubtag(mLanguage);

  if (mScript.Present()) {
    capacity += lengthSubtag(mScript) + 1;
  }

  if (mRegion.Present()) {
    capacity += lengthSubtag(mRegion) + 1;
  }

  capacity += lengthSubtagsZ(mVariants);

  capacity += lengthSubtagsZ(mExtensions);

  if (mPrivateUse.get()) {
    capacity += lengthSubtagZ(mPrivateUse.get()) + 1;
  }

  return capacity;
}

size_t Locale::ToStringAppend(char* aBuffer) const {
  // Current write position inside buffer.
  size_t offset = 0;

  auto appendHyphen = [&offset, &aBuffer]() {
    aBuffer[offset] = '-';
    offset += 1;
  };

  auto appendSubtag = [&offset, &aBuffer](const auto& subtag) {
    auto span = subtag.Span();
    memcpy(aBuffer + offset, span.data(), span.size());
    offset += span.size();
  };

  auto appendSubtagZ = [&offset, &aBuffer](const char* subtag) {
    size_t length = strlen(subtag);
    memcpy(aBuffer + offset, subtag, length);
    offset += length;
  };

  auto appendSubtagsZ = [&appendHyphen, &appendSubtagZ](const auto& subtags) {
    for (const auto& subtag : subtags) {
      appendHyphen();
      appendSubtagZ(subtag.get());
    }
  };

  // Append the language subtag.
  appendSubtag(mLanguage);

  // Append the script subtag if present.
  if (mScript.Present()) {
    appendHyphen();
    appendSubtag(mScript);
  }

  // Append the region subtag if present.
  if (mRegion.Present()) {
    appendHyphen();
    appendSubtag(mRegion);
  }

  // Append the variant subtags if present.
  appendSubtagsZ(mVariants);

  // Append the extensions subtags if present.
  appendSubtagsZ(mExtensions);

  // Append the private-use subtag if present.
  if (mPrivateUse.get()) {
    appendHyphen();
    appendSubtagZ(mPrivateUse.get());
  }

  return offset;
}

LocaleParser::Token LocaleParser::NextToken() {
  MOZ_ASSERT(mIndex <= mLength + 1, "called after 'None' token was read");

  TokenKind kind = TokenKind::None;
  size_t tokenLength = 0;
  for (size_t i = mIndex; i < mLength; i++) {
    // UTS 35, section 3.1.
    // alpha = [A-Z a-z] ;
    // digit = [0-9] ;
    char c = CharAt(i);
    if (IsAsciiAlpha(c)) {
      kind |= TokenKind::Alpha;
    } else if (IsAsciiDigit(c)) {
      kind |= TokenKind::Digit;
    } else if (c == '-' && i > mIndex && i + 1 < mLength) {
      break;
    } else {
      return {TokenKind::Error, 0, 0};
    }
    tokenLength += 1;
  }

  Token token{kind, mIndex, tokenLength};
  mIndex += tokenLength + 1;
  return token;
}

UniqueChars LocaleParser::Chars(size_t aIndex, size_t aLength) const {
  // Add +1 to null-terminate the string.
  auto chars = MakeUnique<char[]>(aLength + 1);
  char* dest = chars.get();
  std::copy_n(mLocale + aIndex, aLength, dest);
  dest[aLength] = '\0';
  return chars;
}

// Parse the `unicode_language_id` production.
//
// unicode_language_id = unicode_language_subtag
//                       (sep unicode_script_subtag)?
//                       (sep unicode_region_subtag)?
//                       (sep unicode_variant_subtag)* ;
//
// sep                 = "-"
//
// Note: Unicode CLDR locale identifier backward compatibility extensions
//       removed from `unicode_language_id`.
//
// |tok| is the current token from |ts|.
//
// All subtags will be added unaltered to |tag|, without canonicalizing their
// case or, in the case of variant subtags, detecting and rejecting duplicate
// variants. Users must subsequently |CanonicalizeBaseName| to perform these
// actions.
//
// Do not use this function directly: use |ParseBaseName| or
// |ParseTlangFromTransformExtension| instead.
Result<Ok, LocaleParser::ParserError> LocaleParser::InternalParseBaseName(
    LocaleParser& aLocaleParser, Locale& aTag, Token& aTok) {
  if (aLocaleParser.IsLanguage(aTok)) {
    aLocaleParser.CopyChars(aTok, aTag.mLanguage);

    aTok = aLocaleParser.NextToken();
  } else {
    // The language subtag is mandatory.
    return Err(ParserError::NotParseable);
  }

  if (aLocaleParser.IsScript(aTok)) {
    aLocaleParser.CopyChars(aTok, aTag.mScript);

    aTok = aLocaleParser.NextToken();
  }

  if (aLocaleParser.IsRegion(aTok)) {
    aLocaleParser.CopyChars(aTok, aTag.mRegion);

    aTok = aLocaleParser.NextToken();
  }

  auto& variants = aTag.mVariants;
  MOZ_ASSERT(variants.length() == 0);
  while (aLocaleParser.IsVariant(aTok)) {
    auto variant = aLocaleParser.Chars(aTok);
    if (!variants.append(std::move(variant))) {
      return Err(ParserError::OutOfMemory);
    }

    aTok = aLocaleParser.NextToken();
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError> LocaleParser::TryParse(
    mozilla::Span<const char> aLocale, Locale& aTag) {
  // |aTag| must be a new, empty Locale.
  MOZ_ASSERT(aTag.Language().Missing());
  MOZ_ASSERT(aTag.Script().Missing());
  MOZ_ASSERT(aTag.Region().Missing());
  MOZ_ASSERT(aTag.Variants().empty());
  MOZ_ASSERT(aTag.Extensions().empty());
  MOZ_ASSERT(aTag.PrivateUse().isNothing());

  // unicode_locale_id = unicode_language_id
  //                     extensions*
  //                     pu_extensions? ;

  LocaleParser ts(aLocale);
  Token tok = ts.NextToken();

  MOZ_TRY(ParseBaseName(ts, aTag, tok));

  // extensions = unicode_locale_extensions
  //            | transformed_extensions
  //            | other_extensions ;

  // Bit set of seen singletons.
  uint64_t seenSingletons = 0;

  auto& extensions = aTag.mExtensions;
  while (ts.IsExtensionStart(tok)) {
    char singleton = ts.SingletonKey(tok);

    // Reject the input if a duplicate singleton was found.
    uint64_t hash = 1ULL << (AsciiAlphanumericToNumber(singleton) + 1);
    if (seenSingletons & hash) {
      return Err(ParserError::NotParseable);
    }
    seenSingletons |= hash;

    Token start = tok;
    tok = ts.NextToken();

    // We'll check for missing non-singleton subtags after this block by
    // comparing |startValue| with the then-current position.
    size_t startValue = tok.Index();

    if (singleton == 'u') {
      while (ts.IsUnicodeExtensionPart(tok)) {
        tok = ts.NextToken();
      }
    } else if (singleton == 't') {
      // transformed_extensions = sep [tT]
      //                          ((sep tlang (sep tfield)*)
      //                           | (sep tfield)+) ;

      // tlang = unicode_language_subtag
      //         (sep unicode_script_subtag)?
      //         (sep unicode_region_subtag)?
      //         (sep unicode_variant_subtag)* ;
      if (ts.IsLanguage(tok)) {
        tok = ts.NextToken();

        if (ts.IsScript(tok)) {
          tok = ts.NextToken();
        }

        if (ts.IsRegion(tok)) {
          tok = ts.NextToken();
        }

        while (ts.IsVariant(tok)) {
          tok = ts.NextToken();
        }
      }

      // tfield = tkey tvalue;
      while (ts.IsTransformExtensionKey(tok)) {
        tok = ts.NextToken();

        size_t startTValue = tok.Index();
        while (ts.IsTransformExtensionPart(tok)) {
          tok = ts.NextToken();
        }

        // `tfield` requires at least one `tvalue`.
        if (tok.Index() <= startTValue) {
          return Err(ParserError::NotParseable);
        }
      }
    } else {
      while (ts.IsOtherExtensionPart(tok)) {
        tok = ts.NextToken();
      }
    }

    // Singletons must be followed by a non-singleton subtag, "en-a-b" is not
    // allowed.
    if (tok.Index() <= startValue) {
      return Err(ParserError::NotParseable);
    }

    UniqueChars extension = ts.Extension(start, tok);
    if (!extensions.append(std::move(extension))) {
      return Err(ParserError::OutOfMemory);
    }
  }

  // Trailing `pu_extension` component of the `unicode_locale_id` production.
  if (ts.IsPrivateUseStart(tok)) {
    Token start = tok;
    tok = ts.NextToken();

    size_t startValue = tok.Index();
    while (ts.IsPrivateUsePart(tok)) {
      tok = ts.NextToken();
    }

    // There must be at least one subtag after the "-x-".
    if (tok.Index() <= startValue) {
      return Err(ParserError::NotParseable);
    }

    UniqueChars privateUse = ts.Extension(start, tok);
    aTag.mPrivateUse = std::move(privateUse);
  }

  if (!tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError> LocaleParser::TryParseBaseName(
    Span<const char> aLocale, Locale& aTag) {
  // |aTag| must be a new, empty Locale.
  MOZ_ASSERT(aTag.Language().Missing());
  MOZ_ASSERT(aTag.Script().Missing());
  MOZ_ASSERT(aTag.Region().Missing());
  MOZ_ASSERT(aTag.Variants().empty());
  MOZ_ASSERT(aTag.Extensions().empty());
  MOZ_ASSERT(aTag.PrivateUse().isNothing());

  LocaleParser ts(aLocale);
  Token tok = ts.NextToken();

  MOZ_TRY(ParseBaseName(ts, aTag, tok));
  if (!tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

// Parse |aExtension|, which must be a valid `transformed_extensions` subtag,
// and fill |aTag| and |aFields| from the `tlang` and `tfield` components.
Result<Ok, LocaleParser::ParserError> LocaleParser::ParseTransformExtension(
    Span<const char> aExtension, Locale& aTag, TFieldVector& aFields) {
  LocaleParser ts(aExtension);
  Token tok = ts.NextToken();

  if (!ts.IsExtensionStart(tok) || ts.SingletonKey(tok) != 't') {
    return Err(ParserError::NotParseable);
  }

  tok = ts.NextToken();

  if (tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  if (ts.IsLanguage(tok)) {
    // We're parsing a possible `tlang` in a known-valid transform extension, so
    // use the special-purpose function that takes advantage of this to compute
    // lowercased |tag| contents in an optimal manner.
    MOZ_TRY(ParseTlangInTransformExtension(ts, aTag, tok));

    // After `tlang` we must have a `tfield` and its `tkey`, or we're at the end
    // of the transform extension.
    MOZ_ASSERT(ts.IsTransformExtensionKey(tok) || tok.IsNone());
  } else {
    // If there's no `tlang` subtag, at least one `tfield` must be present.
    MOZ_ASSERT(ts.IsTransformExtensionKey(tok));
  }

  // Trailing `tfield` subtags. (Any other trailing subtags are an error,
  // because we're guaranteed to only see a valid tranform extension here.)
  while (ts.IsTransformExtensionKey(tok)) {
    size_t begin = tok.Index();
    tok = ts.NextToken();

    size_t startTValue = tok.Index();
    while (ts.IsTransformExtensionPart(tok)) {
      tok = ts.NextToken();
    }

    // `tfield` requires at least one `tvalue`.
    if (tok.Index() <= startTValue) {
      return Err(ParserError::NotParseable);
    }

    size_t length = tok.Index() - 1 - begin;
    if (!aFields.emplaceBack(begin, length)) {
      return Err(ParserError::OutOfMemory);
    }
  }

  if (!tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

// Parse |aExtension|, which must be a valid `unicode_locale_extensions` subtag,
// and fill |aAttributes| and |aKeywords| from the `attribute` and `keyword`
// components.
Result<Ok, LocaleParser::ParserError> LocaleParser::ParseUnicodeExtension(
    Span<const char> aExtension, AttributesVector& aAttributes,
    KeywordsVector& aKeywords) {
  LocaleParser ts(aExtension);
  Token tok = ts.NextToken();

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;

  if (!ts.IsExtensionStart(tok) || ts.SingletonKey(tok) != 'u') {
    return Err(ParserError::NotParseable);
  }

  tok = ts.NextToken();

  if (tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  while (ts.IsUnicodeExtensionAttribute(tok)) {
    if (!aAttributes.emplaceBack(tok.Index(), tok.Length())) {
      return Err(ParserError::OutOfMemory);
    }

    tok = ts.NextToken();
  }

  // keyword = key (sep type)? ;
  while (ts.IsUnicodeExtensionKey(tok)) {
    size_t begin = tok.Index();
    tok = ts.NextToken();

    while (ts.IsUnicodeExtensionType(tok)) {
      tok = ts.NextToken();
    }

    if (tok.IsError()) {
      return Err(ParserError::NotParseable);
    }

    size_t length = tok.Index() - 1 - begin;
    if (!aKeywords.emplaceBack(begin, length)) {
      return Err(ParserError::OutOfMemory);
    }
  }

  if (!tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError> LocaleParser::CanParseUnicodeExtension(
    Span<const char> aExtension) {
  LocaleParser ts(aExtension);
  Token tok = ts.NextToken();

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;

  if (!ts.IsExtensionStart(tok) || ts.SingletonKey(tok) != 'u') {
    return Err(ParserError::NotParseable);
  }

  tok = ts.NextToken();

  if (tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  while (ts.IsUnicodeExtensionAttribute(tok)) {
    tok = ts.NextToken();
  }

  // keyword = key (sep type)? ;
  while (ts.IsUnicodeExtensionKey(tok)) {
    tok = ts.NextToken();

    while (ts.IsUnicodeExtensionType(tok)) {
      tok = ts.NextToken();
    }

    if (tok.IsError()) {
      return Err(ParserError::NotParseable);
    }
  }

  if (!tok.IsNone()) {
    return Err(ParserError::OutOfMemory);
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError>
LocaleParser::CanParseUnicodeExtensionType(Span<const char> aUnicodeType) {
  MOZ_ASSERT(!aUnicodeType.empty(), "caller must exclude empty strings");

  LocaleParser ts(aUnicodeType);
  Token tok = ts.NextToken();

  while (ts.IsUnicodeExtensionType(tok)) {
    tok = ts.NextToken();
  }

  if (!tok.IsNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

}  // namespace mozilla::intl
