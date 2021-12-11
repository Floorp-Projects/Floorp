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
bool IsStructurallyValidLanguageTag(Span<const CharT> language) {
  // unicode_language_subtag = alpha{2,3} | alpha{5,8};
  size_t length = language.size();
  const CharT* str = language.data();
  return ((2 <= length && length <= 3) || (5 <= length && length <= 8)) &&
         std::all_of(str, str + length, IsAsciiAlpha<CharT>);
}

template bool IsStructurallyValidLanguageTag(Span<const char> language);
template bool IsStructurallyValidLanguageTag(Span<const Latin1Char> language);
template bool IsStructurallyValidLanguageTag(Span<const char16_t> language);

template <typename CharT>
bool IsStructurallyValidScriptTag(Span<const CharT> script) {
  // unicode_script_subtag = alpha{4} ;
  size_t length = script.size();
  const CharT* str = script.data();
  return length == 4 && std::all_of(str, str + length, IsAsciiAlpha<CharT>);
}

template bool IsStructurallyValidScriptTag(Span<const char> script);
template bool IsStructurallyValidScriptTag(Span<const Latin1Char> script);
template bool IsStructurallyValidScriptTag(Span<const char16_t> script);

template <typename CharT>
bool IsStructurallyValidRegionTag(Span<const CharT> region) {
  // unicode_region_subtag = (alpha{2} | digit{3}) ;
  size_t length = region.size();
  const CharT* str = region.data();
  return (length == 2 && std::all_of(str, str + length, IsAsciiAlpha<CharT>)) ||
         (length == 3 && std::all_of(str, str + length, IsAsciiDigit<CharT>));
}

template bool IsStructurallyValidRegionTag(Span<const char> region);
template bool IsStructurallyValidRegionTag(Span<const Latin1Char> region);
template bool IsStructurallyValidRegionTag(Span<const char16_t> region);

#ifdef DEBUG
bool IsStructurallyValidVariantTag(Span<const char> variant) {
  // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
  size_t length = variant.size();
  const char* str = variant.data();
  return ((5 <= length && length <= 8) ||
          (length == 4 && IsAsciiDigit(str[0]))) &&
         std::all_of(str, str + length, IsAsciiAlphanumeric<char>);
}

bool IsStructurallyValidUnicodeExtensionTag(Span<const char> extension) {
  return LocaleParser::canParseUnicodeExtension(extension).isOk();
}

static bool IsStructurallyValidExtensionTag(Span<const char> extension) {
  // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
  // NB: Allow any extension, including Unicode and Transform here, because
  // this function is only used for an assertion.

  size_t length = extension.size();
  const char* str = extension.data();
  const char* const end = extension.data() + length;
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

bool IsStructurallyValidPrivateUseTag(Span<const char> privateUse) {
  // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;

  size_t length = privateUse.size();
  const char* str = privateUse.data();
  const char* const end = privateUse.data() + length;
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

ptrdiff_t Locale::unicodeExtensionIndex() const {
  // The extension subtags aren't necessarily sorted, so we can't use binary
  // search here.
  auto p = std::find_if(
      extensions_.begin(), extensions_.end(),
      [](const auto& ext) { return ext[0] == 'u' || ext[0] == 'U'; });
  if (p != extensions_.end()) {
    return std::distance(extensions_.begin(), p);
  }
  return -1;
}

Maybe<Span<const char>> Locale::unicodeExtension() const {
  ptrdiff_t index = unicodeExtensionIndex();
  if (index >= 0) {
    return Some(MakeStringSpan(extensions_[index].get()));
  }
  return Nothing();
}

ICUResult Locale::setUnicodeExtension(Span<const char> extension) {
  MOZ_ASSERT(IsStructurallyValidUnicodeExtensionTag(extension));

  auto duplicated = DuplicateStringToUniqueChars(extension);

  // Replace the existing Unicode extension subtag or append a new one.
  ptrdiff_t index = unicodeExtensionIndex();
  if (index >= 0) {
    extensions_[index] = std::move(duplicated);
    return Ok();
  }
  if (!extensions_.append(std::move(duplicated))) {
    return Err(ICUError::OutOfMemory);
  }
  return Ok();
}

void Locale::clearUnicodeExtension() {
  ptrdiff_t index = unicodeExtensionIndex();
  if (index >= 0) {
    extensions_.erase(extensions_.begin() + index);
  }
}

template <size_t InitialCapacity>
static bool SortAlphabetically(Vector<UniqueChars, InitialCapacity>& subtags) {
  size_t length = subtags.length();

  // Zero or one element lists are already sorted.
  if (length < 2) {
    return true;
  }

  // Handle two element lists inline.
  if (length == 2) {
    if (strcmp(subtags[0].get(), subtags[1].get()) > 0) {
      subtags[0].swap(subtags[1]);
    }
    return true;
  }

  Vector<char*, 8> scratch;
  if (!scratch.resizeUninitialized(length)) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    scratch[i] = subtags[i].release();
  }

  std::stable_sort(
      scratch.begin(), scratch.end(),
      [](const char* a, const char* b) { return strcmp(a, b) < 0; });

  for (size_t i = 0; i < length; i++) {
    subtags[i] = UniqueChars(scratch[i]);
  }
  return true;
}

Result<Ok, Locale::CanonicalizationError> Locale::canonicalizeBaseName() {
  // Per 6.2.3 CanonicalizeUnicodeLocaleId, the very first step is to
  // canonicalize the syntax by normalizing the case and ordering all subtags.
  // The canonical syntax form is specified in UTS 35, 3.2.1.

  // Language codes need to be in lower case. "JA" -> "ja"
  language_.toLowerCase();
  MOZ_ASSERT(IsStructurallyValidLanguageTag(language().span()));

  // The first character of a script code needs to be capitalized.
  // "hans" -> "Hans"
  script_.toTitleCase();
  MOZ_ASSERT(script().missing() ||
             IsStructurallyValidScriptTag(script().span()));

  // Region codes need to be in upper case. "bu" -> "BU"
  region_.toUpperCase();
  MOZ_ASSERT(region().missing() ||
             IsStructurallyValidRegionTag(region().span()));

  // The canonical case for variant subtags is lowercase.
  for (UniqueChars& variant : variants_) {
    char* variantChars = variant.get();
    size_t variantLength = strlen(variantChars);
    AsciiToLowerCase(variantChars, variantLength, variantChars);

    MOZ_ASSERT(IsStructurallyValidVariantTag({variantChars, variantLength}));
  }

  // Extensions and privateuse subtags are case normalized in the
  // |canonicalizeExtensions| method.

  // The second step in UTS 35, 3.2.1, is to order all subtags.

  if (variants_.length() > 1) {
    // 1. Any variants are in alphabetical order.
    if (!SortAlphabetically(variants_)) {
      return Err(CanonicalizationError::OutOfMemory);
    }

    // Reject the Locale identifier if a duplicate variant was found, e.g.
    // "en-variant-Variant".
    const UniqueChars* duplicate = std::adjacent_find(
        variants_.begin(), variants_.end(), [](const auto& a, const auto& b) {
          return strcmp(a.get(), b.get()) == 0;
        });
    if (duplicate != variants_.end()) {
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

  if (!updateLegacyMappings()) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  // Replace deprecated language subtags with their preferred values.
  if (!languageMapping(language_) && complexLanguageMapping(language_)) {
    performComplexLanguageMappings();
  }

  // Replace deprecated script subtags with their preferred values.
  if (script().present()) {
    scriptMapping(script_);
  }

  // Replace deprecated region subtags with their preferred values.
  if (region().present()) {
    if (!regionMapping(region_) && complexRegionMapping(region_)) {
      performComplexRegionMappings();
    }
  }

  // Replace deprecated variant subtags with their preferred values.
  if (!performVariantMappings()) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  // No extension replacements are currently present.
  // Private use sequences are left as is.

  // 3. Replace aliases in special key values.
  // - A subsequent call to canonicalizeExtensions() will perform this step.

  return Ok();
}

#ifdef DEBUG
static bool IsAsciiLowercaseAlphanumericOrDash(Span<const char> span) {
  const char* ptr = span.data();
  size_t length = span.size();
  return std::all_of(ptr, ptr + length, [](auto c) {
    return IsAsciiLowercaseAlpha(c) || IsAsciiDigit(c) || c == '-';
  });
}
#endif

Result<Ok, Locale::CanonicalizationError> Locale::canonicalizeExtensions() {
  // The canonical case for all extension subtags is lowercase.
  for (UniqueChars& extension : extensions_) {
    char* extensionChars = extension.get();
    size_t extensionLength = strlen(extensionChars);
    AsciiToLowerCase(extensionChars, extensionLength, extensionChars);

    MOZ_ASSERT(
        IsStructurallyValidExtensionTag({extensionChars, extensionLength}));
  }

  // Any extensions are in alphabetical order by their singleton.
  // "u-ca-chinese-t-zh-latn" -> "t-zh-latn-u-ca-chinese"
  if (!SortAlphabetically(extensions_)) {
    return Err(CanonicalizationError::OutOfMemory);
  }

  for (UniqueChars& extension : extensions_) {
    if (extension[0] == 'u') {
      MOZ_TRY(canonicalizeUnicodeExtension(extension));
    } else if (extension[0] == 't') {
      MOZ_TRY(canonicalizeTransformExtension(extension));
    }

    MOZ_ASSERT(
        IsAsciiLowercaseAlphanumericOrDash(MakeStringSpan(extension.get())));
  }

  // The canonical case for privateuse subtags is lowercase.
  if (char* privateuse = privateuse_.get()) {
    size_t privateuseLength = strlen(privateuse);
    AsciiToLowerCase(privateuse, privateuseLength, privateuse);

    MOZ_ASSERT(
        IsStructurallyValidPrivateUseTag({privateuse, privateuseLength}));
  }
  return Ok();
}

template <size_t N>
static inline bool AppendSpan(Vector<char, N>& vector, Span<const char> span) {
  return vector.append(span.data(), span.size());
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
Result<Ok, Locale::CanonicalizationError> Locale::canonicalizeUnicodeExtension(
    UniqueChars& unicodeExtension) {
  Span<const char> extension = MakeStringSpan(unicodeExtension.get());
  MOZ_ASSERT(extension[0] == 'u');
  MOZ_ASSERT(extension[1] == '-');
  MOZ_ASSERT(IsStructurallyValidExtensionTag(extension));

  LocaleParser::AttributesVector attributes;
  LocaleParser::KeywordsVector keywords;

  using Attribute = LocaleParser::AttributesVector::ElementType;
  using Keyword = LocaleParser::KeywordsVector::ElementType;

  if (LocaleParser::parseUnicodeExtension(extension, attributes, keywords)
          .isErr()) {
    MOZ_ASSERT_UNREACHABLE("unexpected invalid Unicode extension subtag");
    return Err(CanonicalizationError::InternalError);
  }

  auto attributesLess = [extension](const Attribute& a, const Attribute& b) {
    auto astr = extension.Subspan(a.begin(), a.length());
    auto bstr = extension.Subspan(b.begin(), b.length());
    return astr < bstr;
  };

  // All attributes are sorted in alphabetical order.
  if (attributes.length() > 1) {
    std::stable_sort(attributes.begin(), attributes.end(), attributesLess);
  }

  auto keywordsLess = [extension](const Keyword& a, const Keyword& b) {
    auto astr = extension.Subspan(a.begin(), UnicodeKeyLength);
    auto bstr = extension.Subspan(b.begin(), UnicodeKeyLength);
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
    auto span = extension.Subspan(attribute.begin(), attribute.length());

    // Skip duplicate attributes.
    if (i > 0) {
      const auto& lastAttribute = attributes[i - 1];
      if (span ==
          extension.Subspan(lastAttribute.begin(), lastAttribute.length())) {
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
      if (extension.Subspan(keyword.begin(), UnicodeKeyLength) ==
          extension.Subspan(lastKeyword.begin(), UnicodeKeyLength)) {
        continue;
      }
      MOZ_ASSERT(keywordsLess(lastKeyword, keyword));
    }

    if (!sb.append('-')) {
      return Err(CanonicalizationError::OutOfMemory);
    }

    StringSpan span = extension.Subspan(keyword.begin(), keyword.length());
    if (span.size() == UnicodeKeyLength) {
      // Keyword without type value.
      if (!AppendSpan(sb, span)) {
        return Err(CanonicalizationError::OutOfMemory);
      }
    } else {
      StringSpan key = span.To(UnicodeKeyLength);
      StringSpan type = span.From(UnicodeKeyWithSepLength);

      // Search if there's a replacement for the current Unicode keyword.
      if (const char* replacement = replaceUnicodeExtensionType(key, type)) {
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
    unicodeExtension = std::move(canonical);
  }

  return Ok();
}

template <class Buffer>
static bool LocaleToString(const Locale& tag, Buffer& sb) {
  auto appendSubtag = [&sb](const auto& subtag) {
    auto span = subtag.span();
    MOZ_ASSERT(!span.empty());
    return sb.append(span.data(), span.size());
  };

  auto appendSubtagSpan = [&sb](Span<const char> subtag) {
    MOZ_ASSERT(!subtag.empty());
    return sb.append(subtag.data(), subtag.size());
  };

  auto appendSubtags = [&sb, &appendSubtagSpan](const auto& subtags) {
    for (const auto& subtag : subtags) {
      if (!sb.append('-') || !appendSubtagSpan(subtag)) {
        return false;
      }
    }
    return true;
  };

  // Append the language subtag.
  if (!appendSubtag(tag.language())) {
    return false;
  }

  // Append the script subtag if present.
  if (tag.script().present()) {
    if (!sb.append('-') || !appendSubtag(tag.script())) {
      return false;
    }
  }

  // Append the region subtag if present.
  if (tag.region().present()) {
    if (!sb.append('-') || !appendSubtag(tag.region())) {
      return false;
    }
  }

  // Append the variant subtags if present.
  if (!appendSubtags(tag.variants())) {
    return false;
  }

  // Append the extensions subtags if present.
  if (!appendSubtags(tag.extensions())) {
    return false;
  }

  // Append the private-use subtag if present.
  if (auto privateuse = tag.privateuse()) {
    if (!sb.append('-') || !appendSubtagSpan(privateuse.value())) {
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
Locale::canonicalizeTransformExtension(UniqueChars& transformExtension) {
  Span<const char> extension = MakeStringSpan(transformExtension.get());
  MOZ_ASSERT(extension[0] == 't');
  MOZ_ASSERT(extension[1] == '-');
  MOZ_ASSERT(IsStructurallyValidExtensionTag(extension));

  Locale tag;
  LocaleParser::TFieldVector fields;

  using TField = LocaleParser::TFieldVector::ElementType;

  if (LocaleParser::parseTransformExtension(extension, tag, fields).isErr()) {
    MOZ_ASSERT_UNREACHABLE("unexpected invalid transform extension subtag");
    return Err(CanonicalizationError::InternalError);
  }

  auto tfieldLess = [extension](const TField& a, const TField& b) {
    auto astr = extension.Subspan(a.begin(), TransformKeyLength);
    auto bstr = extension.Subspan(b.begin(), TransformKeyLength);
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
  if (tag.language().present()) {
    if (!sb.append('-')) {
      return Err(CanonicalizationError::OutOfMemory);
    }

    MOZ_TRY(tag.canonicalizeBaseName());

    // The canonical case for Transform extensions is lowercase per
    // <https://unicode.org/reports/tr35/#BCP47_T_Extension>. Convert the two
    // subtags which don't use lowercase for their canonical syntax.
    tag.script_.toLowerCase();
    tag.region_.toLowerCase();

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

    StringSpan span = extension.Subspan(field.begin(), field.length());
    StringSpan key = span.To(TransformKeyLength);
    StringSpan value = span.From(TransformKeyWithSepLength);

    // Search if there's a replacement for the current transform keyword.
    if (const char* replacement = replaceTransformExtensionType(key, value)) {
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
    transformExtension = std::move(canonical);
  }

  return Ok();
}

// Zero-terminated ICU Locale ID.
using LocaleId =
    Vector<char, LanguageLength + 1 + ScriptLength + 1 + RegionLength + 1>;

enum class LikelySubtags : bool { Add, Remove };

// Return true iff the locale is already maximized resp. minimized.
static bool HasLikelySubtags(LikelySubtags likelySubtags, const Locale& tag) {
  // The locale is already maximized if the language, script, and region
  // subtags are present and no placeholder subtags ("und", "Zzzz", "ZZ") are
  // used.
  if (likelySubtags == LikelySubtags::Add) {
    return !tag.language().equalTo("und") &&
           (tag.script().present() && !tag.script().equalTo("Zzzz")) &&
           (tag.region().present() && !tag.region().equalTo("ZZ"));
  }

  // The locale is already minimized if it only contains a language
  // subtag whose value is not the placeholder value "und".
  return !tag.language().equalTo("und") && tag.script().missing() &&
         tag.region().missing();
}

// Create an ICU locale ID from the given locale.
static bool CreateLocaleForLikelySubtags(const Locale& tag, LocaleId& locale) {
  MOZ_ASSERT(locale.length() == 0);

  auto appendSubtag = [&locale](const auto& subtag) {
    auto span = subtag.span();
    MOZ_ASSERT(!span.empty());
    return locale.append(span.data(), span.size());
  };

  // Append the language subtag.
  if (!appendSubtag(tag.language())) {
    return false;
  }

  // Append the script subtag if present.
  if (tag.script().present()) {
    if (!locale.append('_') || !appendSubtag(tag.script())) {
      return false;
    }
  }

  // Append the region subtag if present.
  if (tag.region().present()) {
    if (!locale.append('_') || !appendSubtag(tag.region())) {
      return false;
    }
  }

  // Zero-terminated for use with ICU.
  return locale.append('\0');
}

static ICUError ParserErrorToICUError(LocaleParser::ParserError err) {
  using ParserError = LocaleParser::ParserError;

  switch (err) {
    case ParserError::NotParseable:
      return ICUError::InternalError;
    case ParserError::OutOfMemory:
      return ICUError::OutOfMemory;
  }
  MOZ_CRASH("Unexpected parser error");
}

static ICUError CanonicalizationErrorToICUError(
    Locale::CanonicalizationError err) {
  using CanonicalizationError = Locale::CanonicalizationError;

  switch (err) {
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
static ICUResult AssignFromLocaleId(LocaleId& localeId, Locale& tag) {
  // Replace the ICU locale ID separator.
  std::replace(localeId.begin(), localeId.end(), '_', '-');

  // ICU replaces "und" with the empty string, which means "und" becomes "" and
  // "und-Latn" becomes "-Latn". Handle this case separately.
  if (localeId.empty() || localeId[0] == '-') {
    static constexpr auto und = MakeStringSpan("und");
    constexpr size_t length = und.size();

    // Insert "und" in front of the locale ID.
    if (!localeId.growBy(length)) {
      return Err(ICUError::OutOfMemory);
    }
    memmove(localeId.begin() + length, localeId.begin(), localeId.length());
    memmove(localeId.begin(), und.data(), length);
  }

  // Retrieve the language, script, and region subtags from the locale ID
  Locale localeTag;
  MOZ_TRY(LocaleParser::tryParseBaseName(localeId, localeTag)
              .mapErr(ParserErrorToICUError));

  tag.setLanguage(localeTag.language());
  tag.setScript(localeTag.script());
  tag.setRegion(localeTag.region());

  return Ok();
}

template <decltype(uloc_addLikelySubtags) likelySubtagsFn>
static ICUResult CallLikelySubtags(const LocaleId& localeId, LocaleId& result) {
  // Locale ID must be zero-terminated before passing it to ICU.
  MOZ_ASSERT(localeId.back() == '\0');
  MOZ_ASSERT(result.length() == 0);

  // Ensure there's enough room for the result.
  MOZ_ALWAYS_TRUE(result.resize(LocaleId::InlineLength));

  return FillBufferWithICUCall(
      result, [&localeId](char* chars, int32_t size, UErrorCode* status) {
        return likelySubtagsFn(localeId.begin(), chars, size, status);
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
static ICUResult LikelySubtags(LikelySubtags likelySubtags, Locale& tag) {
  // Return early if the input is already maximized/minimized.
  if (HasLikelySubtags(likelySubtags, tag)) {
    return Ok();
  }

  // Create the locale ID for the input argument.
  LocaleId locale;
  if (!CreateLocaleForLikelySubtags(tag, locale)) {
    return Err(ICUError::OutOfMemory);
  }

  // Either add or remove likely subtags to/from the locale ID.
  LocaleId localeLikelySubtags;
  if (likelySubtags == LikelySubtags::Add) {
    MOZ_TRY(
        CallLikelySubtags<uloc_addLikelySubtags>(locale, localeLikelySubtags));
  } else {
    MOZ_TRY(
        CallLikelySubtags<uloc_minimizeSubtags>(locale, localeLikelySubtags));
  }

  // Assign the language, script, and region subtags from the locale ID.
  MOZ_TRY(AssignFromLocaleId(localeLikelySubtags, tag));

  // Update mappings in case ICU returned a non-canonical locale.
  MOZ_TRY(tag.canonicalizeBaseName().mapErr(CanonicalizationErrorToICUError));

  return Ok();
}

ICUResult Locale::addLikelySubtags() {
  return LikelySubtags(LikelySubtags::Add, *this);
}

ICUResult Locale::removeLikelySubtags() {
  return LikelySubtags(LikelySubtags::Remove, *this);
}

UniqueChars Locale::DuplicateStringToUniqueChars(const char* s) {
  size_t length = strlen(s) + 1;
  auto duplicate = MakeUnique<char[]>(length);
  memcpy(duplicate.get(), s, length);
  return duplicate;
}

UniqueChars Locale::DuplicateStringToUniqueChars(Span<const char> s) {
  size_t length = s.size();
  auto duplicate = MakeUnique<char[]>(length + 1);
  memcpy(duplicate.get(), s.data(), length);
  duplicate[length] = '\0';
  return duplicate;
}

size_t Locale::toStringCapacity() const {
  // This is a bit awkward, the buffer class currently does not support
  // being resized, so we need to calculate the required size up front and
  // reserve it all at once.
  auto lengthSubtag = [](const auto& subtag) {
    auto span = subtag.span();
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

  capacity += lengthSubtag(language_);

  if (script_.present()) {
    capacity += lengthSubtag(script_) + 1;
  }

  if (region_.present()) {
    capacity += lengthSubtag(region_) + 1;
  }

  capacity += lengthSubtagsZ(variants_);

  capacity += lengthSubtagsZ(extensions_);

  if (privateuse_.get()) {
    capacity += lengthSubtagZ(privateuse_.get()) + 1;
  }

  return capacity;
}

size_t Locale::toStringAppend(char* buffer) const {
  // Current write position inside buffer.
  size_t offset = 0;

  auto appendHyphen = [&offset, &buffer]() {
    buffer[offset] = '-';
    offset += 1;
  };

  auto appendSubtag = [&offset, &buffer](const auto& subtag) {
    auto span = subtag.span();
    memcpy(buffer + offset, span.data(), span.size());
    offset += span.size();
  };

  auto appendSubtagZ = [&offset, &buffer](const char* subtag) {
    size_t length = strlen(subtag);
    memcpy(buffer + offset, subtag, length);
    offset += length;
  };

  auto appendSubtagsZ = [&appendHyphen, &appendSubtagZ](const auto& subtags) {
    for (const auto& subtag : subtags) {
      appendHyphen();
      appendSubtagZ(subtag.get());
    }
  };

  // Append the language subtag.
  appendSubtag(language_);

  // Append the script subtag if present.
  if (script_.present()) {
    appendHyphen();
    appendSubtag(script_);
  }

  // Append the region subtag if present.
  if (region_.present()) {
    appendHyphen();
    appendSubtag(region_);
  }

  // Append the variant subtags if present.
  appendSubtagsZ(variants_);

  // Append the extensions subtags if present.
  appendSubtagsZ(extensions_);

  // Append the private-use subtag if present.
  if (privateuse_.get()) {
    appendHyphen();
    appendSubtagZ(privateuse_.get());
  }

  return offset;
}

LocaleParser::Token LocaleParser::nextToken() {
  MOZ_ASSERT(index_ <= length_ + 1, "called after 'None' token was read");

  TokenKind kind = TokenKind::None;
  size_t tokenLength = 0;
  for (size_t i = index_; i < length_; i++) {
    // UTS 35, section 3.1.
    // alpha = [A-Z a-z] ;
    // digit = [0-9] ;
    char c = charAt(i);
    if (IsAsciiAlpha(c)) {
      kind |= TokenKind::Alpha;
    } else if (IsAsciiDigit(c)) {
      kind |= TokenKind::Digit;
    } else if (c == '-' && i > index_ && i + 1 < length_) {
      break;
    } else {
      return {TokenKind::Error, 0, 0};
    }
    tokenLength += 1;
  }

  Token token{kind, index_, tokenLength};
  index_ += tokenLength + 1;
  return token;
}

UniqueChars LocaleParser::chars(size_t index, size_t length) const {
  // Add +1 to null-terminate the string.
  auto chars = MakeUnique<char[]>(length + 1);
  char* dest = chars.get();
  std::copy_n(locale_ + index, length, dest);
  dest[length] = '\0';
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
// variants. Users must subsequently |canonicalizeBaseName| to perform these
// actions.
//
// Do not use this function directly: use |parseBaseName| or
// |parseTlangFromTransformExtension| instead.
Result<Ok, LocaleParser::ParserError> LocaleParser::internalParseBaseName(
    LocaleParser& ts, Locale& tag, Token& tok) {
  if (ts.isLanguage(tok)) {
    ts.copyChars(tok, tag.language_);

    tok = ts.nextToken();
  } else {
    // The language subtag is mandatory.
    return Err(ParserError::NotParseable);
  }

  if (ts.isScript(tok)) {
    ts.copyChars(tok, tag.script_);

    tok = ts.nextToken();
  }

  if (ts.isRegion(tok)) {
    ts.copyChars(tok, tag.region_);

    tok = ts.nextToken();
  }

  auto& variants = tag.variants_;
  MOZ_ASSERT(variants.length() == 0);
  while (ts.isVariant(tok)) {
    auto variant = ts.chars(tok);
    if (!variants.append(std::move(variant))) {
      return Err(ParserError::OutOfMemory);
    }

    tok = ts.nextToken();
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError> LocaleParser::tryParse(
    mozilla::Span<const char> locale, Locale& tag) {
  // unicode_locale_id = unicode_language_id
  //                     extensions*
  //                     pu_extensions? ;

  LocaleParser ts(locale);
  Token tok = ts.nextToken();

  MOZ_TRY(parseBaseName(ts, tag, tok));

  // extensions = unicode_locale_extensions
  //            | transformed_extensions
  //            | other_extensions ;

  // Bit set of seen singletons.
  uint64_t seenSingletons = 0;

  auto& extensions = tag.extensions_;
  while (ts.isExtensionStart(tok)) {
    char singleton = ts.singletonKey(tok);

    // Reject the input if a duplicate singleton was found.
    uint64_t hash = 1ULL << (AsciiAlphanumericToNumber(singleton) + 1);
    if (seenSingletons & hash) {
      return Err(ParserError::NotParseable);
    }
    seenSingletons |= hash;

    Token start = tok;
    tok = ts.nextToken();

    // We'll check for missing non-singleton subtags after this block by
    // comparing |startValue| with the then-current position.
    size_t startValue = tok.index();

    if (singleton == 'u') {
      while (ts.isUnicodeExtensionPart(tok)) {
        tok = ts.nextToken();
      }
    } else if (singleton == 't') {
      // transformed_extensions = sep [tT]
      //                          ((sep tlang (sep tfield)*)
      //                           | (sep tfield)+) ;

      // tlang = unicode_language_subtag
      //         (sep unicode_script_subtag)?
      //         (sep unicode_region_subtag)?
      //         (sep unicode_variant_subtag)* ;
      if (ts.isLanguage(tok)) {
        tok = ts.nextToken();

        if (ts.isScript(tok)) {
          tok = ts.nextToken();
        }

        if (ts.isRegion(tok)) {
          tok = ts.nextToken();
        }

        while (ts.isVariant(tok)) {
          tok = ts.nextToken();
        }
      }

      // tfield = tkey tvalue;
      while (ts.isTransformExtensionKey(tok)) {
        tok = ts.nextToken();

        size_t startTValue = tok.index();
        while (ts.isTransformExtensionPart(tok)) {
          tok = ts.nextToken();
        }

        // `tfield` requires at least one `tvalue`.
        if (tok.index() <= startTValue) {
          return Err(ParserError::NotParseable);
        }
      }
    } else {
      while (ts.isOtherExtensionPart(tok)) {
        tok = ts.nextToken();
      }
    }

    // Singletons must be followed by a non-singleton subtag, "en-a-b" is not
    // allowed.
    if (tok.index() <= startValue) {
      return Err(ParserError::NotParseable);
    }

    UniqueChars extension = ts.extension(start, tok);
    if (!extensions.append(std::move(extension))) {
      return Err(ParserError::OutOfMemory);
    }
  }

  // Trailing `pu_extension` component of the `unicode_locale_id` production.
  if (ts.isPrivateUseStart(tok)) {
    Token start = tok;
    tok = ts.nextToken();

    size_t startValue = tok.index();
    while (ts.isPrivateUsePart(tok)) {
      tok = ts.nextToken();
    }

    // There must be at least one subtag after the "-x-".
    if (tok.index() <= startValue) {
      return Err(ParserError::NotParseable);
    }

    UniqueChars privateUse = ts.extension(start, tok);
    tag.privateuse_ = std::move(privateUse);
  }

  if (!tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError> LocaleParser::tryParseBaseName(
    Span<const char> locale, Locale& tag) {
  LocaleParser ts(locale);
  Token tok = ts.nextToken();

  MOZ_TRY(parseBaseName(ts, tag, tok));
  if (!tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

// Parse |extension|, which must be a valid `transformed_extensions` subtag, and
// fill |tag| and |fields| from the `tlang` and `tfield` components.
Result<Ok, LocaleParser::ParserError> LocaleParser::parseTransformExtension(
    Span<const char> extension, Locale& tag, TFieldVector& fields) {
  LocaleParser ts(extension);
  Token tok = ts.nextToken();

  if (!ts.isExtensionStart(tok) || ts.singletonKey(tok) != 't') {
    return Err(ParserError::NotParseable);
  }

  tok = ts.nextToken();

  if (tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  if (ts.isLanguage(tok)) {
    // We're parsing a possible `tlang` in a known-valid transform extension, so
    // use the special-purpose function that takes advantage of this to compute
    // lowercased |tag| contents in an optimal manner.
    MOZ_TRY(parseTlangInTransformExtension(ts, tag, tok));

    // After `tlang` we must have a `tfield` and its `tkey`, or we're at the end
    // of the transform extension.
    MOZ_ASSERT(ts.isTransformExtensionKey(tok) || tok.isNone());
  } else {
    // If there's no `tlang` subtag, at least one `tfield` must be present.
    MOZ_ASSERT(ts.isTransformExtensionKey(tok));
  }

  // Trailing `tfield` subtags. (Any other trailing subtags are an error,
  // because we're guaranteed to only see a valid tranform extension here.)
  while (ts.isTransformExtensionKey(tok)) {
    size_t begin = tok.index();
    tok = ts.nextToken();

    size_t startTValue = tok.index();
    while (ts.isTransformExtensionPart(tok)) {
      tok = ts.nextToken();
    }

    // `tfield` requires at least one `tvalue`.
    if (tok.index() <= startTValue) {
      return Err(ParserError::NotParseable);
    }

    size_t length = tok.index() - 1 - begin;
    if (!fields.emplaceBack(begin, length)) {
      return Err(ParserError::OutOfMemory);
    }
  }

  if (!tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

// Parse |extension|, which must be a valid `unicode_locale_extensions` subtag,
// and fill |attributes| and |keywords| from the `attribute` and `keyword`
// components.
Result<Ok, LocaleParser::ParserError> LocaleParser::parseUnicodeExtension(
    Span<const char> extension, AttributesVector& attributes,
    KeywordsVector& keywords) {
  LocaleParser ts(extension);
  Token tok = ts.nextToken();

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;

  if (!ts.isExtensionStart(tok) || ts.singletonKey(tok) != 'u') {
    return Err(ParserError::NotParseable);
  }

  tok = ts.nextToken();

  if (tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  while (ts.isUnicodeExtensionAttribute(tok)) {
    if (!attributes.emplaceBack(tok.index(), tok.length())) {
      return Err(ParserError::OutOfMemory);
    }

    tok = ts.nextToken();
  }

  // keyword = key (sep type)? ;
  while (ts.isUnicodeExtensionKey(tok)) {
    size_t begin = tok.index();
    tok = ts.nextToken();

    while (ts.isUnicodeExtensionType(tok)) {
      tok = ts.nextToken();
    }

    if (tok.isError()) {
      return Err(ParserError::NotParseable);
    }

    size_t length = tok.index() - 1 - begin;
    if (!keywords.emplaceBack(begin, length)) {
      return Err(ParserError::OutOfMemory);
    }
  }

  if (!tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError> LocaleParser::canParseUnicodeExtension(
    Span<const char> extension) {
  LocaleParser ts(extension);
  Token tok = ts.nextToken();

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;

  if (!ts.isExtensionStart(tok) || ts.singletonKey(tok) != 'u') {
    return Err(ParserError::NotParseable);
  }

  tok = ts.nextToken();

  if (tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  while (ts.isUnicodeExtensionAttribute(tok)) {
    tok = ts.nextToken();
  }

  // keyword = key (sep type)? ;
  while (ts.isUnicodeExtensionKey(tok)) {
    tok = ts.nextToken();

    while (ts.isUnicodeExtensionType(tok)) {
      tok = ts.nextToken();
    }

    if (tok.isError()) {
      return Err(ParserError::NotParseable);
    }
  }

  if (!tok.isNone()) {
    return Err(ParserError::OutOfMemory);
  }

  return Ok();
}

Result<Ok, LocaleParser::ParserError>
LocaleParser::canParseUnicodeExtensionType(Span<const char> unicodeType) {
  MOZ_ASSERT(!unicodeType.empty(), "caller must exclude empty strings");

  LocaleParser ts(unicodeType);
  Token tok = ts.nextToken();

  while (ts.isUnicodeExtensionType(tok)) {
    tok = ts.nextToken();
  }

  if (!tok.isNone()) {
    return Err(ParserError::NotParseable);
  }

  return Ok();
}

}  // namespace mozilla::intl
