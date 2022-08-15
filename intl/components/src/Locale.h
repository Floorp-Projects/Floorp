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
bool IsStructurallyValidLanguageTag(mozilla::Span<const CharT> aLanguage);

/**
 * Return true if |script| is a valid script subtag.
 */
template <typename CharT>
bool IsStructurallyValidScriptTag(mozilla::Span<const CharT> aScript);

/**
 * Return true if |region| is a valid region subtag.
 */
template <typename CharT>
bool IsStructurallyValidRegionTag(mozilla::Span<const CharT> aRegion);

#ifdef DEBUG
/**
 * Return true if |variant| is a valid variant subtag.
 */
bool IsStructurallyValidVariantTag(mozilla::Span<const char> aVariant);

/**
 * Return true if |extension| is a valid Unicode extension subtag.
 */
bool IsStructurallyValidUnicodeExtensionTag(
    mozilla::Span<const char> aExtension);

/**
 * Return true if |privateUse| is a valid private-use subtag.
 */
bool IsStructurallyValidPrivateUseTag(mozilla::Span<const char> aPrivateUse);

#endif

template <typename CharT>
char AsciiToLowerCase(CharT aChar) {
  MOZ_ASSERT(mozilla::IsAscii(aChar));
  return mozilla::IsAsciiUppercaseAlpha(aChar) ? (aChar + 0x20) : aChar;
}

template <typename CharT>
char AsciiToUpperCase(CharT aChar) {
  MOZ_ASSERT(mozilla::IsAscii(aChar));
  return mozilla::IsAsciiLowercaseAlpha(aChar) ? (aChar - 0x20) : aChar;
}

template <typename CharT>
void AsciiToLowerCase(CharT* aChars, size_t aLength, char* aDest) {
  char (&fn)(CharT) = AsciiToLowerCase;
  std::transform(aChars, aChars + aLength, aDest, fn);
}

template <typename CharT>
void AsciiToUpperCase(CharT* aChars, size_t aLength, char* aDest) {
  char (&fn)(CharT) = AsciiToUpperCase;
  std::transform(aChars, aChars + aLength, aDest, fn);
}

template <typename CharT>
void AsciiToTitleCase(CharT* aChars, size_t aLength, char* aDest) {
  if (aLength > 0) {
    AsciiToUpperCase(aChars, 1, aDest);
    AsciiToLowerCase(aChars + 1, aLength - 1, aDest + 1);
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
template <size_t SubtagLength>
class LanguageTagSubtag final {
  uint8_t mLength = 0;
  char mChars[SubtagLength] = {};  // zero initialize

 public:
  LanguageTagSubtag() = default;

  LanguageTagSubtag(const LanguageTagSubtag& aOther) {
    std::copy_n(aOther.mChars, SubtagLength, mChars);
    mLength = aOther.mLength;
  }

  LanguageTagSubtag& operator=(const LanguageTagSubtag& aOther) {
    std::copy_n(aOther.mChars, SubtagLength, mChars);
    mLength = aOther.mLength;
    return *this;
  }

  size_t Length() const { return mLength; }
  bool Missing() const { return mLength == 0; }
  bool Present() const { return mLength > 0; }

  mozilla::Span<const char> Span() const { return {mChars, mLength}; }

  template <typename CharT>
  void Set(mozilla::Span<const CharT> str) {
    MOZ_ASSERT(str.size() <= SubtagLength);
    std::copy_n(str.data(), str.size(), mChars);
    mLength = str.size();
  }

  // The toXYZCase() methods are using |SubtagLength| instead of |length()|,
  // because current compilers (tested GCC and Clang) can't infer the maximum
  // string length - even when using hints like |std::min| - and instead are
  // emitting SIMD optimized code. Using a fixed sized length avoids emitting
  // the SIMD code. (Emitting SIMD code doesn't make sense here, because the
  // SIMD code only kicks in for long strings.) A fixed length will
  // additionally ensure the compiler unrolls the loop in the case conversion
  // code.

  void ToLowerCase() { AsciiToLowerCase(mChars, SubtagLength, mChars); }

  void ToUpperCase() { AsciiToUpperCase(mChars, SubtagLength, mChars); }

  void ToTitleCase() { AsciiToTitleCase(mChars, SubtagLength, mChars); }

  template <size_t N>
  bool EqualTo(const char (&str)[N]) const {
    static_assert(N - 1 <= SubtagLength,
                  "subtag literals must not exceed the maximum subtag length");

    return mLength == N - 1 && memcmp(mChars, str, N - 1) == 0;
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
  LanguageSubtag mLanguage = {};
  ScriptSubtag mScript = {};
  RegionSubtag mRegion = {};

  using VariantsVector = Vector<UniqueChars, 2>;
  using ExtensionsVector = Vector<UniqueChars, 2>;

  VariantsVector mVariants;
  ExtensionsVector mExtensions;
  UniqueChars mPrivateUse = nullptr;

  friend class LocaleParser;

 public:
  enum class CanonicalizationError : uint8_t {
    DuplicateVariant,
    InternalError,
    OutOfMemory,
  };

 private:
  Result<Ok, CanonicalizationError> CanonicalizeUnicodeExtension(
      UniqueChars& unicodeExtension);

  Result<Ok, CanonicalizationError> CanonicalizeTransformExtension(
      UniqueChars& transformExtension);

 public:
  static bool LanguageMapping(LanguageSubtag& aLanguage);
  static bool ComplexLanguageMapping(const LanguageSubtag& aLanguage);

 private:
  static bool ScriptMapping(ScriptSubtag& aScript);
  static bool RegionMapping(RegionSubtag& aRegion);
  static bool ComplexRegionMapping(const RegionSubtag& aRegion);

  void PerformComplexLanguageMappings();
  void PerformComplexRegionMappings();
  [[nodiscard]] bool PerformVariantMappings();

  [[nodiscard]] bool UpdateLegacyMappings();

  static bool SignLanguageMapping(LanguageSubtag& aLanguage,
                                  const RegionSubtag& aRegion);

  static const char* ReplaceTransformExtensionType(
      mozilla::Span<const char> aKey, mozilla::Span<const char> aType);

 public:
  /**
   * Given a Unicode key and type, return the null-terminated preferred
   * replacement for that type if there is one, or null if there is none, e.g.
   * in effect
   * |ReplaceUnicodeExtensionType("ca", "islamicc") == "islamic-civil"|
   * and
   * |ReplaceUnicodeExtensionType("ca", "islamic-civil") == nullptr|.
   */
  static const char* ReplaceUnicodeExtensionType(
      mozilla::Span<const char> aKey, mozilla::Span<const char> aType);

 public:
  Locale() = default;
  Locale(const Locale&) = delete;
  Locale& operator=(const Locale&) = delete;
  Locale(Locale&&) = default;
  Locale& operator=(Locale&&) = default;

  template <class Vec>
  class SubtagIterator {
    using Iter = decltype(std::declval<const Vec>().begin());

    Iter mIter;

   public:
    explicit SubtagIterator(Iter iter) : mIter(iter) {}

    // std::iterator traits.
    using iterator_category = std::input_iterator_tag;
    using value_type = Span<const char>;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    SubtagIterator& operator++() {
      mIter++;
      return *this;
    }

    SubtagIterator operator++(int) {
      SubtagIterator result = *this;
      ++(*this);
      return result;
    }

    bool operator==(const SubtagIterator& aOther) const {
      return mIter == aOther.mIter;
    }

    bool operator!=(const SubtagIterator& aOther) const {
      return !(*this == aOther);
    }

    value_type operator*() const { return MakeStringSpan(mIter->get()); }
  };

  template <size_t N>
  class SubtagEnumeration {
    using Vec = Vector<UniqueChars, N>;

    const Vec& mVector;

   public:
    explicit SubtagEnumeration(const Vec& aVector) : mVector(aVector) {}

    size_t length() const { return mVector.length(); }
    bool empty() const { return mVector.empty(); }

    auto begin() const { return SubtagIterator<Vec>(mVector.begin()); }
    auto end() const { return SubtagIterator<Vec>(mVector.end()); }

    Span<const char> operator[](size_t aIndex) const {
      return MakeStringSpan(mVector[aIndex].get());
    }
  };

  const LanguageSubtag& Language() const { return mLanguage; }
  const ScriptSubtag& Script() const { return mScript; }
  const RegionSubtag& Region() const { return mRegion; }
  auto Variants() const { return SubtagEnumeration(mVariants); }
  auto Extensions() const { return SubtagEnumeration(mExtensions); }
  Maybe<Span<const char>> PrivateUse() const {
    if (const char* p = mPrivateUse.get()) {
      return Some(MakeStringSpan(p));
    }
    return Nothing();
  }

  /**
   * Return the Unicode extension subtag or Nothing if not present.
   */
  Maybe<Span<const char>> GetUnicodeExtension() const;

 private:
  ptrdiff_t UnicodeExtensionIndex() const;

 public:
  /**
   * Set the language subtag. The input must be a valid language subtag.
   */
  template <size_t N>
  void SetLanguage(const char (&aLanguage)[N]) {
    mozilla::Span<const char> span(aLanguage, N - 1);
    MOZ_ASSERT(IsStructurallyValidLanguageTag(span));
    mLanguage.Set(span);
  }

  /**
   * Set the language subtag. The input must be a valid language subtag.
   */
  void SetLanguage(const LanguageSubtag& aLanguage) {
    MOZ_ASSERT(IsStructurallyValidLanguageTag(aLanguage.Span()));
    mLanguage.Set(aLanguage.Span());
  }

  /**
   * Set the script subtag. The input must be a valid script subtag.
   */
  template <size_t N>
  void SetScript(const char (&aScript)[N]) {
    mozilla::Span<const char> span(aScript, N - 1);
    MOZ_ASSERT(IsStructurallyValidScriptTag(span));
    mScript.Set(span);
  }

  /**
   * Set the script subtag. The input must be a valid script subtag or the empty
   * string.
   */
  void SetScript(const ScriptSubtag& aScript) {
    MOZ_ASSERT(aScript.Missing() ||
               IsStructurallyValidScriptTag(aScript.Span()));
    mScript.Set(aScript.Span());
  }

  /**
   * Set the region subtag. The input must be a valid region subtag.
   */
  template <size_t N>
  void SetRegion(const char (&aRegion)[N]) {
    mozilla::Span<const char> span(aRegion, N - 1);
    MOZ_ASSERT(IsStructurallyValidRegionTag(span));
    mRegion.Set(span);
  }

  /**
   * Set the region subtag. The input must be a valid region subtag or the empty
   * empty string.
   */
  void SetRegion(const RegionSubtag& aRegion) {
    MOZ_ASSERT(aRegion.Missing() ||
               IsStructurallyValidRegionTag(aRegion.Span()));
    mRegion.Set(aRegion.Span());
  }

  /**
   * Removes all variant subtags.
   */
  void ClearVariants() { mVariants.clearAndFree(); }

  /**
   * Set the Unicode extension subtag. The input must be a valid Unicode
   * extension subtag.
   */
  ICUResult SetUnicodeExtension(Span<const char> aExtension);

  /**
   * Remove any Unicode extension subtag if present.
   */
  void ClearUnicodeExtension();

  /** Canonicalize the base-name (language, script, region, variant) subtags. */
  Result<Ok, CanonicalizationError> CanonicalizeBaseName();

  /**
   * Canonicalize all extension subtags.
   */
  Result<Ok, CanonicalizationError> CanonicalizeExtensions();

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
  Result<Ok, CanonicalizationError> Canonicalize() {
    MOZ_TRY(CanonicalizeBaseName());
    return CanonicalizeExtensions();
  }

  /**
   * Fill the buffer with a string representation of the locale.
   */
  template <typename B>
  ICUResult ToString(B& aBuffer) const {
    static_assert(std::is_same_v<typename B::CharType, char>);

    size_t capacity = ToStringCapacity();

    // Attempt to reserve needed capacity
    if (!aBuffer.reserve(capacity)) {
      return Err(ICUError::OutOfMemory);
    }

    size_t offset = ToStringAppend(aBuffer.data());

    MOZ_ASSERT(capacity == offset);
    aBuffer.written(offset);

    return Ok();
  }

  /**
   * Add likely-subtags to the locale.
   *
   * Spec: <https://www.unicode.org/reports/tr35/#Likely_Subtags>
   */
  ICUResult AddLikelySubtags();

  /**
   * Remove likely-subtags from the locale.
   *
   * Spec: <https://www.unicode.org/reports/tr35/#Likely_Subtags>
   */
  ICUResult RemoveLikelySubtags();

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
  static UniqueChars DuplicateStringToUniqueChars(const char* aStr);
  static UniqueChars DuplicateStringToUniqueChars(Span<const char> aStr);
  size_t ToStringCapacity() const;
  size_t ToStringAppend(char* aBuffer) const;
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
    size_t mIndex;
    size_t mLength;
    TokenKind mKind;

   public:
    Token(TokenKind aKind, size_t aIndex, size_t aLength)
        : mIndex(aIndex), mLength(aLength), mKind(aKind) {}

    TokenKind Kind() const { return mKind; }
    size_t Index() const { return mIndex; }
    size_t Length() const { return mLength; }

    bool IsError() const { return mKind == TokenKind::Error; }
    bool IsNone() const { return mKind == TokenKind::None; }
    bool IsAlpha() const { return mKind == TokenKind::Alpha; }
    bool IsDigit() const { return mKind == TokenKind::Digit; }
    bool IsAlphaDigit() const { return mKind == TokenKind::AlphaDigit; }
  };

  const char* mLocale;
  size_t mLength;
  size_t mIndex = 0;

  explicit LocaleParser(Span<const char> aLocale)
      : mLocale(aLocale.data()), mLength(aLocale.size()) {}

  char CharAt(size_t aIndex) const { return mLocale[aIndex]; }

  // Copy the token characters into |subtag|.
  template <size_t N>
  void CopyChars(const Token& aTok, LanguageTagSubtag<N>& aSubtag) const {
    aSubtag.Set(mozilla::Span(mLocale + aTok.Index(), aTok.Length()));
  }

  // Create a string copy of |length| characters starting at |index|.
  UniqueChars Chars(size_t aIndex, size_t aLength) const;

  // Create a string copy of the token characters.
  UniqueChars Chars(const Token& aTok) const {
    return Chars(aTok.Index(), aTok.Length());
  }

  UniqueChars Extension(const Token& aStart, const Token& aEnd) const {
    MOZ_ASSERT(aStart.Index() < aEnd.Index());

    size_t length = aEnd.Index() - 1 - aStart.Index();
    return Chars(aStart.Index(), length);
  }

  Token NextToken();

  // unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
  //
  // Four character language subtags are not allowed in Unicode BCP 47 locale
  // identifiers. Also see the comparison to Unicode CLDR locale identifiers in
  // <https://unicode.org/reports/tr35/#BCP_47_Conformance>.
  bool IsLanguage(const Token& aTok) const {
    return aTok.IsAlpha() && ((2 <= aTok.Length() && aTok.Length() <= 3) ||
                              (5 <= aTok.Length() && aTok.Length() <= 8));
  }

  // unicode_script_subtag = alpha{4} ;
  bool IsScript(const Token& aTok) const {
    return aTok.IsAlpha() && aTok.Length() == 4;
  }

  // unicode_region_subtag = (alpha{2} | digit{3}) ;
  bool IsRegion(const Token& aTok) const {
    return (aTok.IsAlpha() && aTok.Length() == 2) ||
           (aTok.IsDigit() && aTok.Length() == 3);
  }

  // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
  bool IsVariant(const Token& aTok) const {
    return (5 <= aTok.Length() && aTok.Length() <= 8) ||
           (aTok.Length() == 4 && mozilla::IsAsciiDigit(CharAt(aTok.Index())));
  }

  // Returns the code unit of the first character at the given singleton token.
  // Always returns the lower case form of an alphabetical character.
  char SingletonKey(const Token& aTok) const {
    MOZ_ASSERT(aTok.Length() == 1);
    return AsciiToLowerCase(CharAt(aTok.Index()));
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
  bool IsExtensionStart(const Token& aTok) const {
    return aTok.Length() == 1 && SingletonKey(aTok) != 'x';
  }

  // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
  bool IsOtherExtensionPart(const Token& aTok) const {
    return 2 <= aTok.Length() && aTok.Length() <= 8;
  }

  // unicode_locale_extensions = sep [uU] ((sep keyword)+ |
  //                                       (sep attribute)+ (sep keyword)*) ;
  // keyword = key (sep type)? ;
  bool IsUnicodeExtensionPart(const Token& aTok) const {
    return IsUnicodeExtensionKey(aTok) || IsUnicodeExtensionType(aTok) ||
           IsUnicodeExtensionAttribute(aTok);
  }

  // attribute = alphanum{3,8} ;
  bool IsUnicodeExtensionAttribute(const Token& aTok) const {
    return 3 <= aTok.Length() && aTok.Length() <= 8;
  }

  // key = alphanum alpha ;
  bool IsUnicodeExtensionKey(const Token& aTok) const {
    return aTok.Length() == 2 &&
           mozilla::IsAsciiAlpha(CharAt(aTok.Index() + 1));
  }

  // type = alphanum{3,8} (sep alphanum{3,8})* ;
  bool IsUnicodeExtensionType(const Token& aTok) const {
    return 3 <= aTok.Length() && aTok.Length() <= 8;
  }

  // tkey = alpha digit ;
  bool IsTransformExtensionKey(const Token& aTok) const {
    return aTok.Length() == 2 && mozilla::IsAsciiAlpha(CharAt(aTok.Index())) &&
           mozilla::IsAsciiDigit(CharAt(aTok.Index() + 1));
  }

  // tvalue = (sep alphanum{3,8})+ ;
  bool IsTransformExtensionPart(const Token& aTok) const {
    return 3 <= aTok.Length() && aTok.Length() <= 8;
  }

  // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
  bool IsPrivateUseStart(const Token& aTok) const {
    return aTok.Length() == 1 && SingletonKey(aTok) == 'x';
  }

  // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
  bool IsPrivateUsePart(const Token& aTok) const {
    return 1 <= aTok.Length() && aTok.Length() <= 8;
  }

  // Helper function for use in |ParseBaseName| and
  // |ParseTlangInTransformExtension|.  Do not use this directly!
  static Result<Ok, ParserError> InternalParseBaseName(
      LocaleParser& aLocaleParser, Locale& aTag, Token& aTok);

  // Parse the `unicode_language_id` production, i.e. the
  // language/script/region/variants portion of a locale, into |aTag|.
  // |aTok| must be the current token.
  static Result<Ok, ParserError> ParseBaseName(LocaleParser& aLocaleParser,
                                               Locale& aTag, Token& aTok) {
    return InternalParseBaseName(aLocaleParser, aTag, aTok);
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
  static Result<Ok, ParserError> ParseTlangInTransformExtension(
      LocaleParser& aLocaleParser, Locale& aTag, Token& aTok) {
    MOZ_ASSERT(aLocaleParser.IsLanguage(aTok));
    return InternalParseBaseName(aLocaleParser, aTag, aTok);
  }

  friend class Locale;

  class Range final {
    size_t mBegin;
    size_t mLength;

   public:
    Range(size_t aBegin, size_t aLength) : mBegin(aBegin), mLength(aLength) {}

    size_t Begin() const { return mBegin; }
    size_t Length() const { return mLength; }
  };

  using TFieldVector = Vector<Range, 8>;
  using AttributesVector = Vector<Range, 8>;
  using KeywordsVector = Vector<Range, 8>;

  // Parse |extension|, which must be a validated, fully lowercase
  // `transformed_extensions` subtag, and fill |tag| and |fields| from the
  // `tlang` and `tfield` components. Data in |tag| is lowercase, consistent
  // with |extension|.
  static Result<Ok, ParserError> ParseTransformExtension(
      mozilla::Span<const char> aExtension, Locale& aTag,
      TFieldVector& aFields);

  // Parse |extension|, which must be a validated, fully lowercase
  // `unicode_locale_extensions` subtag, and fill |attributes| and |keywords|
  // from the `attribute` and `keyword` components.
  static Result<Ok, ParserError> ParseUnicodeExtension(
      mozilla::Span<const char> aExtension, AttributesVector& aAttributes,
      KeywordsVector& aKeywords);

 public:
  // Parse the input string as a locale.
  //
  // NOTE: |aTag| must be a new, empty Locale.
  static Result<Ok, ParserError> TryParse(Span<const char> aLocale,
                                          Locale& aTag);

  // Parse the input string as the base-name parts (language, script, region,
  // variants) of a locale.
  //
  // NOTE: |aTag| must be a new, empty Locale.
  static Result<Ok, ParserError> TryParseBaseName(Span<const char> aLocale,
                                                  Locale& aTag);

  // Return Ok() iff |extension| can be parsed as a Unicode extension subtag.
  static Result<Ok, ParserError> CanParseUnicodeExtension(
      Span<const char> aExtension);

  // Return Ok() iff |unicodeType| can be parsed as a Unicode extension type.
  static Result<Ok, ParserError> CanParseUnicodeExtensionType(
      Span<const char> aUnicodeType);
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(LocaleParser::TokenKind)

}  // namespace mozilla::intl

#endif /* intl_components_Locale_h */
