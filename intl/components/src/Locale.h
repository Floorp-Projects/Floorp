/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Structured representation of Unicode locale IDs used with Intl functions. */

#ifndef intl_components_Locale_h
#define intl_components_Locale_h

#include "mozilla/Assertions.h"
#include "mozilla/intl/ICUError.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "mozilla/Result.h"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>

#include "unicode/uloc.h"

namespace mozilla::intl {

/**
 * Return true if |language| is a valid language subtag.
 */
template <typename CharT>
bool IsStructurallyValidLanguageTag(mozilla::Span<const CharT> language);

/**
 * Return true if |script| is a valid script subtag.
 */
template <typename CharT>
bool IsStructurallyValidScriptTag(mozilla::Span<const CharT> script);

/**
 * Return true if |region| is a valid region subtag.
 */
template <typename CharT>
bool IsStructurallyValidRegionTag(mozilla::Span<const CharT> region);

#ifdef DEBUG
/**
 * Return true if |variant| is a valid variant subtag.
 */
bool IsStructurallyValidVariantTag(mozilla::Span<const char> variant);

/**
 * Return true if |extension| is a valid Unicode extension subtag.
 */
bool IsStructurallyValidUnicodeExtensionTag(
    mozilla::Span<const char> extension);

/**
 * Return true if |privateUse| is a valid private-use subtag.
 */
bool IsStructurallyValidPrivateUseTag(mozilla::Span<const char> privateUse);

#endif

template <typename CharT>
char AsciiToLowerCase(CharT c) {
  MOZ_ASSERT(mozilla::IsAscii(c));
  return mozilla::IsAsciiUppercaseAlpha(c) ? (c + 0x20) : c;
}

template <typename CharT>
char AsciiToUpperCase(CharT c) {
  MOZ_ASSERT(mozilla::IsAscii(c));
  return mozilla::IsAsciiLowercaseAlpha(c) ? (c - 0x20) : c;
}

template <typename CharT>
void AsciiToLowerCase(CharT* chars, size_t length, char* dest) {
  char (&fn)(CharT) = AsciiToLowerCase;
  std::transform(chars, chars + length, dest, fn);
}

template <typename CharT>
void AsciiToUpperCase(CharT* chars, size_t length, char* dest) {
  char (&fn)(CharT) = AsciiToUpperCase;
  std::transform(chars, chars + length, dest, fn);
}

template <typename CharT>
void AsciiToTitleCase(CharT* chars, size_t length, char* dest) {
  if (length > 0) {
    AsciiToUpperCase(chars, 1, dest);
    AsciiToLowerCase(chars + 1, length - 1, dest + 1);
  }
}

// Constants for language subtag lengths.
namespace LanguageTagLimits {

// unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
static constexpr size_t LanguageLength = 8;

// unicode_script_subtag = alpha{4} ;
static constexpr size_t ScriptLength = 4;

// unicode_region_subtag = (alpha{2} | digit{3}) ;
static constexpr size_t RegionLength = 3;
static constexpr size_t AlphaRegionLength = 2;
static constexpr size_t DigitRegionLength = 3;

// key = alphanum alpha ;
static constexpr size_t UnicodeKeyLength = 2;

// tkey = alpha digit ;
static constexpr size_t TransformKeyLength = 2;

}  // namespace LanguageTagLimits

// Fixed size language subtag which is stored inline in Locale.
template <size_t Length>
class LanguageTagSubtag final {
  uint8_t length_ = 0;
  char chars_[Length] = {};  // zero initialize

 public:
  LanguageTagSubtag() = default;

  LanguageTagSubtag(const LanguageTagSubtag&) = delete;
  LanguageTagSubtag& operator=(const LanguageTagSubtag&) = delete;

  size_t length() const { return length_; }
  bool missing() const { return length_ == 0; }
  bool present() const { return length_ > 0; }

  mozilla::Span<const char> span() const { return {chars_, length_}; }

  template <typename CharT>
  void set(mozilla::Span<const CharT> str) {
    MOZ_ASSERT(str.size() <= Length);
    std::copy_n(str.data(), str.size(), chars_);
    length_ = str.size();
  }

  // The toXYZCase() methods are using |Length| instead of |length()|, because
  // current compilers (tested GCC and Clang) can't infer the maximum string
  // length - even when using hints like |std::min| - and instead are emitting
  // SIMD optimized code. Using a fixed sized length avoids emitting the SIMD
  // code. (Emitting SIMD code doesn't make sense here, because the SIMD code
  // only kicks in for long strings.) A fixed length will additionally ensure
  // the compiler unrolls the loop in the case conversion code.

  void toLowerCase() { AsciiToLowerCase(chars_, Length, chars_); }

  void toUpperCase() { AsciiToUpperCase(chars_, Length, chars_); }

  void toTitleCase() { AsciiToTitleCase(chars_, Length, chars_); }

  template <size_t N>
  bool equalTo(const char (&str)[N]) const {
    static_assert(N - 1 <= Length,
                  "subtag literals must not exceed the maximum subtag length");

    return length_ == N - 1 && memcmp(chars_, str, N - 1) == 0;
  }
};

using LanguageSubtag = LanguageTagSubtag<LanguageTagLimits::LanguageLength>;
using ScriptSubtag = LanguageTagSubtag<LanguageTagLimits::ScriptLength>;
using RegionSubtag = LanguageTagSubtag<LanguageTagLimits::RegionLength>;

using Latin1Char = unsigned char;
using UniqueChars = UniquePtr<char[]>;

/**
 * Object representing a Unicode BCP 47 locale identifier.
 *
 * All subtags are already in canonicalized case.
 */
class MOZ_STACK_CLASS Locale final {
  LanguageSubtag language_ = {};
  ScriptSubtag script_ = {};
  RegionSubtag region_ = {};

  using VariantsVector = Vector<UniqueChars, 2>;
  using ExtensionsVector = Vector<UniqueChars, 2>;

  VariantsVector variants_;
  ExtensionsVector extensions_;
  UniqueChars privateuse_ = nullptr;

  friend class LocaleParser;

 public:
  enum class CanonicalizationError : uint8_t {
    DuplicateVariant,
    InternalError,
    OutOfMemory,
  };

 private:
  Result<Ok, CanonicalizationError> canonicalizeUnicodeExtension(
      UniqueChars& unicodeExtension);

  Result<Ok, CanonicalizationError> canonicalizeTransformExtension(
      UniqueChars& transformExtension);

 public:
  static bool languageMapping(LanguageSubtag& language);
  static bool complexLanguageMapping(const LanguageSubtag& language);

 private:
  static bool scriptMapping(ScriptSubtag& script);
  static bool regionMapping(RegionSubtag& region);
  static bool complexRegionMapping(const RegionSubtag& region);

  void performComplexLanguageMappings();
  void performComplexRegionMappings();
  [[nodiscard]] bool performVariantMappings();

  [[nodiscard]] bool updateLegacyMappings();

  static bool signLanguageMapping(LanguageSubtag& language,
                                  const RegionSubtag& region);

  static const char* replaceTransformExtensionType(
      mozilla::Span<const char> key, mozilla::Span<const char> type);

 public:
  /**
   * Given a Unicode key and type, return the null-terminated preferred
   * replacement for that type if there is one, or null if there is none, e.g.
   * in effect
   * |replaceUnicodeExtensionType("ca", "islamicc") == "islamic-civil"|
   * and
   * |replaceUnicodeExtensionType("ca", "islamic-civil") == nullptr|.
   */
  static const char* replaceUnicodeExtensionType(
      mozilla::Span<const char> key, mozilla::Span<const char> type);

 public:
  Locale() = default;
  Locale(const Locale&) = delete;
  Locale& operator=(const Locale&) = delete;

  template <class Vec>
  class SubtagIterator {
    using Iter = decltype(std::declval<const Vec>().begin());

    Iter iter_;

   public:
    explicit SubtagIterator(Iter iter) : iter_(iter) {}

    // std::iterator traits.
    using iterator_category = std::input_iterator_tag;
    using value_type = Span<const char>;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    SubtagIterator& operator++() {
      iter_++;
      return *this;
    }

    SubtagIterator operator++(int) {
      SubtagIterator result = *this;
      ++(*this);
      return result;
    }

    bool operator==(const SubtagIterator& aOther) const {
      return iter_ == aOther.iter_;
    }

    bool operator!=(const SubtagIterator& aOther) const {
      return !(*this == aOther);
    }

    value_type operator*() const { return MakeStringSpan(iter_->get()); }
  };

  template <size_t N>
  class SubtagEnumeration {
    using Vec = Vector<UniqueChars, N>;

    const Vec& vector_;

   public:
    explicit SubtagEnumeration(const Vec& vector) : vector_(vector) {}

    size_t length() const { return vector_.length(); }
    bool empty() const { return vector_.empty(); }

    auto begin() const { return SubtagIterator<Vec>(vector_.begin()); }
    auto end() const { return SubtagIterator<Vec>(vector_.end()); }

    Span<const char> operator[](size_t index) const {
      return MakeStringSpan(vector_[index].get());
    }
  };

  const LanguageSubtag& language() const { return language_; }
  const ScriptSubtag& script() const { return script_; }
  const RegionSubtag& region() const { return region_; }
  auto variants() const { return SubtagEnumeration(variants_); }
  auto extensions() const { return SubtagEnumeration(extensions_); }
  Maybe<Span<const char>> privateuse() const {
    if (const char* p = privateuse_.get()) {
      return Some(MakeStringSpan(p));
    }
    return Nothing();
  }

  /**
   * Return the Unicode extension subtag or Nothing if not present.
   */
  Maybe<Span<const char>> unicodeExtension() const;

 private:
  ptrdiff_t unicodeExtensionIndex() const;

 public:
  /**
   * Set the language subtag. The input must be a valid language subtag.
   */
  template <size_t N>
  void setLanguage(const char (&language)[N]) {
    mozilla::Span<const char> span(language, N - 1);
    MOZ_ASSERT(IsStructurallyValidLanguageTag(span));
    language_.set(span);
  }

  /**
   * Set the language subtag. The input must be a valid language subtag.
   */
  void setLanguage(const LanguageSubtag& language) {
    MOZ_ASSERT(IsStructurallyValidLanguageTag(language.span()));
    language_.set(language.span());
  }

  /**
   * Set the script subtag. The input must be a valid script subtag.
   */
  template <size_t N>
  void setScript(const char (&script)[N]) {
    mozilla::Span<const char> span(script, N - 1);
    MOZ_ASSERT(IsStructurallyValidScriptTag(span));
    script_.set(span);
  }

  /**
   * Set the script subtag. The input must be a valid script subtag or the empty
   * string.
   */
  void setScript(const ScriptSubtag& script) {
    MOZ_ASSERT(script.missing() || IsStructurallyValidScriptTag(script.span()));
    script_.set(script.span());
  }

  /**
   * Set the region subtag. The input must be a valid region subtag.
   */
  template <size_t N>
  void setRegion(const char (&region)[N]) {
    mozilla::Span<const char> span(region, N - 1);
    MOZ_ASSERT(IsStructurallyValidRegionTag(span));
    region_.set(span);
  }

  /**
   * Set the region subtag. The input must be a valid region subtag or the empty
   * empty string.
   */
  void setRegion(const RegionSubtag& region) {
    MOZ_ASSERT(region.missing() || IsStructurallyValidRegionTag(region.span()));
    region_.set(region.span());
  }

  /**
   * Removes all variant subtags.
   */
  void clearVariants() { variants_.clearAndFree(); }

  /**
   * Set the Unicode extension subtag. The input must be a valid Unicode
   * extension subtag.
   */
  ICUResult setUnicodeExtension(Span<const char> extension);

  /**
   * Remove any Unicode extension subtag if present.
   */
  void clearUnicodeExtension();

  /** Canonicalize the base-name (language, script, region, variant) subtags. */
  Result<Ok, CanonicalizationError> canonicalizeBaseName();

  /**
   * Canonicalize all extension subtags.
   */
  Result<Ok, CanonicalizationError> canonicalizeExtensions();

  /**
   * Canonicalizes the given structurally valid Unicode BCP 47 locale
   * identifier, including regularized case of subtags. For example, the
   * locale Zh-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE,
   * where
   *
   *     Zh             ; 2*3ALPHA
   *     -haNS          ; ["-" script]
   *     -bu            ; ["-" region]
   *     -variant2      ; *("-" variant)
   *     -Variant1
   *     -u-ca-chinese  ; *("-" extension)
   *     -t-Zh-laTN
   *     -x-PRIVATE     ; ["-" privateuse]
   *
   * becomes zh-Hans-MM-variant1-variant2-t-zh-latn-u-ca-chinese-x-private
   *
   * Spec: ECMAScript Internationalization API Specification, 6.2.3.
   */
  Result<Ok, CanonicalizationError> canonicalize() {
    MOZ_TRY(canonicalizeBaseName());
    return canonicalizeExtensions();
  }

  /**
   * Fill the buffer with a string representation of the locale.
   */
  template <typename B>
  ICUResult toString(B& buffer) const {
    static_assert(std::is_same_v<typename B::CharType, char>);

    size_t capacity = toStringCapacity();

    // Attempt to reserve needed capacity
    if (!buffer.reserve(capacity)) {
      return Err(ICUError::OutOfMemory);
    }

    size_t offset = toStringAppend(buffer.data());

    MOZ_ASSERT(capacity == offset);
    buffer.written(offset);

    return Ok();
  }

  /**
   * Add likely-subtags to the locale.
   *
   * Spec: <https://www.unicode.org/reports/tr35/#Likely_Subtags>
   */
  ICUResult addLikelySubtags();

  /**
   * Remove likely-subtags from the locale.
   *
   * Spec: <https://www.unicode.org/reports/tr35/#Likely_Subtags>
   */
  ICUResult removeLikelySubtags();

  /**
   * Returns the default locale as an ICU locale identifier. The returned string
   * is NOT a valid BCP 47 locale!
   *
   * Also see <https://unicode-org.github.io/icu/userguide/locale>.
   */
  static const char* GetDefaultLocale() { return uloc_getDefault(); }

  /**
   * Returns an iterator over all supported locales.
   *
   * The returned strings are ICU locale identifiers and NOT BCP 47 language
   * tags.
   *
   * Also see <https://unicode-org.github.io/icu/userguide/locale>.
   */
  static auto GetAvailableLocales() {
    return AvailableLocalesEnumeration<uloc_countAvailable,
                                       uloc_getAvailable>();
  }

 private:
  static UniqueChars DuplicateStringToUniqueChars(const char* s);
  static UniqueChars DuplicateStringToUniqueChars(Span<const char> s);
  size_t toStringCapacity() const;
  size_t toStringAppend(char* buffer) const;
};

/**
 * Parser for Unicode BCP 47 locale identifiers.
 *
 * <https://unicode.org/reports/tr35/#Unicode_Language_and_Locale_Identifiers>
 */
class MOZ_STACK_CLASS LocaleParser final {
 public:
  enum class ParserError : uint8_t {
    // Input was not parseable as a locale, subtag or extension.
    NotParseable,
    // Unable to allocate memory for the parser to operate.
    OutOfMemory,
  };

  // Exposed as |public| for |MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS|.
  enum class TokenKind : uint8_t {
    None = 0b000,
    Alpha = 0b001,
    Digit = 0b010,
    AlphaDigit = 0b011,
    Error = 0b100
  };

 private:
  class Token final {
    size_t index_;
    size_t length_;
    TokenKind kind_;

   public:
    Token(TokenKind kind, size_t index, size_t length)
        : index_(index), length_(length), kind_(kind) {}

    TokenKind kind() const { return kind_; }
    size_t index() const { return index_; }
    size_t length() const { return length_; }

    bool isError() const { return kind_ == TokenKind::Error; }
    bool isNone() const { return kind_ == TokenKind::None; }
    bool isAlpha() const { return kind_ == TokenKind::Alpha; }
    bool isDigit() const { return kind_ == TokenKind::Digit; }
    bool isAlphaDigit() const { return kind_ == TokenKind::AlphaDigit; }
  };

  const char* locale_;
  size_t length_;
  size_t index_ = 0;

  explicit LocaleParser(Span<const char> locale)
      : locale_(locale.data()), length_(locale.size()) {}

  char charAt(size_t index) const { return locale_[index]; }

  // Copy the token characters into |subtag|.
  template <size_t N>
  void copyChars(const Token& tok, LanguageTagSubtag<N>& subtag) const {
    subtag.set(mozilla::Span(locale_ + tok.index(), tok.length()));
  }

  // Create a string copy of |length| characters starting at |index|.
  UniqueChars chars(size_t index, size_t length) const;

  // Create a string copy of the token characters.
  UniqueChars chars(const Token& tok) const {
    return chars(tok.index(), tok.length());
  }

  UniqueChars extension(const Token& start, const Token& end) const {
    MOZ_ASSERT(start.index() < end.index());

    size_t length = end.index() - 1 - start.index();
    return chars(start.index(), length);
  }

  Token nextToken();

  // unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
  //
  // Four character language subtags are not allowed in Unicode BCP 47 locale
  // identifiers. Also see the comparison to Unicode CLDR locale identifiers in
  // <https://unicode.org/reports/tr35/#BCP_47_Conformance>.
  bool isLanguage(const Token& tok) const {
    return tok.isAlpha() && ((2 <= tok.length() && tok.length() <= 3) ||
                             (5 <= tok.length() && tok.length() <= 8));
  }

  // unicode_script_subtag = alpha{4} ;
  bool isScript(const Token& tok) const {
    return tok.isAlpha() && tok.length() == 4;
  }

  // unicode_region_subtag = (alpha{2} | digit{3}) ;
  bool isRegion(const Token& tok) const {
    return (tok.isAlpha() && tok.length() == 2) ||
           (tok.isDigit() && tok.length() == 3);
  }

  // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
  bool isVariant(const Token& tok) const {
    return (5 <= tok.length() && tok.length() <= 8) ||
           (tok.length() == 4 && mozilla::IsAsciiDigit(charAt(tok.index())));
  }

  // Returns the code unit of the first character at the given singleton token.
  // Always returns the lower case form of an alphabetical character.
  char singletonKey(const Token& tok) const {
    MOZ_ASSERT(tok.length() == 1);
    return AsciiToLowerCase(charAt(tok.index()));
  }

  // extensions = unicode_locale_extensions |
  //              transformed_extensions |
  //              other_extensions ;
  //
  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;
  //
  // transformed_extensions = sep [tT] ((sep tlang (sep tfield)*) |
  //                                    (sep tfield)+) ;
  //
  // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
  bool isExtensionStart(const Token& tok) const {
    return tok.length() == 1 && singletonKey(tok) != 'x';
  }

  // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
  bool isOtherExtensionPart(const Token& tok) const {
    return 2 <= tok.length() && tok.length() <= 8;
  }

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;
  // keyword = key (sep type)? ;
  bool isUnicodeExtensionPart(const Token& tok) const {
    return isUnicodeExtensionKey(tok) || isUnicodeExtensionType(tok) ||
           isUnicodeExtensionAttribute(tok);
  }

  // attribute = alphanum{3,8} ;
  bool isUnicodeExtensionAttribute(const Token& tok) const {
    return 3 <= tok.length() && tok.length() <= 8;
  }

  // key = alphanum alpha ;
  bool isUnicodeExtensionKey(const Token& tok) const {
    return tok.length() == 2 && mozilla::IsAsciiAlpha(charAt(tok.index() + 1));
  }

  // type = alphanum{3,8} (sep alphanum{3,8})* ;
  bool isUnicodeExtensionType(const Token& tok) const {
    return 3 <= tok.length() && tok.length() <= 8;
  }

  // tkey = alpha digit ;
  bool isTransformExtensionKey(const Token& tok) const {
    return tok.length() == 2 && mozilla::IsAsciiAlpha(charAt(tok.index())) &&
           mozilla::IsAsciiDigit(charAt(tok.index() + 1));
  }

  // tvalue = (sep alphanum{3,8})+ ;
  bool isTransformExtensionPart(const Token& tok) const {
    return 3 <= tok.length() && tok.length() <= 8;
  }

  // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
  bool isPrivateUseStart(const Token& tok) const {
    return tok.length() == 1 && singletonKey(tok) == 'x';
  }

  // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
  bool isPrivateUsePart(const Token& tok) const {
    return 1 <= tok.length() && tok.length() <= 8;
  }

  // Helper function for use in |parseBaseName| and
  // |parseTlangInTransformExtension|.  Do not use this directly!
  static Result<Ok, ParserError> internalParseBaseName(LocaleParser& ts,
                                                       Locale& tag, Token& tok);

  // Parse the `unicode_language_id` production, i.e. the
  // language/script/region/variants portion of a locale, into |tag|.
  // |tok| must be the current token.
  static Result<Ok, ParserError> parseBaseName(LocaleParser& ts, Locale& tag,
                                               Token& tok) {
    return internalParseBaseName(ts, tag, tok);
  }

  // Parse the `tlang` production within a parsed 't' transform extension.
  // The precise requirements for "previously parsed" are:
  //
  //   * the input begins from current token |tok| with a valid `tlang`
  //   * the `tlang` is wholly lowercase (*not* canonical case)
  //   * variant subtags in the `tlang` may contain duplicates and be
  //     unordered
  //
  // Return an error on internal failure. Otherwise, return a success value. If
  // there was no `tlang`, then |tag.language().missing()|. But if there was a
  // `tlang`, then |tag| is filled with subtags exactly as they appeared in the
  // parse input.
  static Result<Ok, ParserError> parseTlangInTransformExtension(
      LocaleParser& ts, Locale& tag, Token& tok) {
    MOZ_ASSERT(ts.isLanguage(tok));
    return internalParseBaseName(ts, tag, tok);
  }

  friend class Locale;

  class Range final {
    size_t begin_;
    size_t length_;

   public:
    Range(size_t begin, size_t length) : begin_(begin), length_(length) {}

    size_t begin() const { return begin_; }
    size_t length() const { return length_; }
  };

  using TFieldVector = Vector<Range, 8>;
  using AttributesVector = Vector<Range, 8>;
  using KeywordsVector = Vector<Range, 8>;

  // Parse |extension|, which must be a validated, fully lowercase
  // `transformed_extensions` subtag, and fill |tag| and |fields| from the
  // `tlang` and `tfield` components. Data in |tag| is lowercase, consistent
  // with |extension|.
  static Result<Ok, ParserError> parseTransformExtension(
      mozilla::Span<const char> extension, Locale& tag, TFieldVector& fields);

  // Parse |extension|, which must be a validated, fully lowercase
  // `unicode_locale_extensions` subtag, and fill |attributes| and |keywords|
  // from the `attribute` and `keyword` components.
  static Result<Ok, ParserError> parseUnicodeExtension(
      mozilla::Span<const char> extension, AttributesVector& attributes,
      KeywordsVector& keywords);

 public:
  // Parse the input string as a locale.
  static Result<Ok, ParserError> tryParse(Span<const char> locale, Locale& tag);

  // Parse the input string as the base-name parts (language, script, region,
  // variants) of a locale.
  static Result<Ok, ParserError> tryParseBaseName(Span<const char> locale,
                                                  Locale& tag);

  // Return Ok() iff |extension| can be parsed as a Unicode extension subtag.
  static Result<Ok, ParserError> canParseUnicodeExtension(
      Span<const char> extension);

  // Return Ok() iff |unicodeType| can be parsed as a Unicode extension type.
  static Result<Ok, ParserError> canParseUnicodeExtensionType(
      Span<const char> unicodeType);
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(LocaleParser::TokenKind)

}  // namespace mozilla::intl

#endif /* intl_components_Locale_h */
