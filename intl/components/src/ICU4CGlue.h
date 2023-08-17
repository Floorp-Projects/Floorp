/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef intl_components_ICUUtils_h
#define intl_components_ICUUtils_h

#include "unicode/uenum.h"
#include "unicode/utypes.h"
#include "mozilla/Buffer.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"
#include "mozilla/intl/ICUError.h"

// When building standalone js shell, it will include headers from
// intl/components if JS_HAS_INTL_API is true (the default value), but js shell
// won't include headers from XPCOM, so don't include nsTArray.h when building
// standalone js shell.
#ifndef JS_STANDALONE
#  include "nsTArray.h"
#endif

#include <cstring>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>

struct UFormattedValue;
namespace mozilla::intl {

template <typename CharType>
static inline CharType* AssertNullTerminatedString(Span<CharType> aSpan) {
  // Intentionally check one past the last character, because we expect that the
  // NUL character isn't part of the string.
  MOZ_ASSERT(*(aSpan.data() + aSpan.size()) == '\0');

  // Also ensure there aren't any other NUL characters within the string.
  MOZ_ASSERT(std::char_traits<std::remove_const_t<CharType>>::length(
                 aSpan.data()) == aSpan.size());

  return aSpan.data();
}

static inline const char* AssertNullTerminatedString(std::string_view aView) {
  // Intentionally check one past the last character, because we expect that the
  // NUL character isn't part of the string.
  MOZ_ASSERT(*(aView.data() + aView.size()) == '\0');

  // Also ensure there aren't any other NUL characters within the string.
  MOZ_ASSERT(std::strlen(aView.data()) == aView.size());

  return aView.data();
}

/**
 * Map the "und" locale to an empty string, which ICU uses internally.
 */
static inline const char* IcuLocale(const char* aLocale) {
  // Return the empty string if the input is exactly equal to the string "und".
  const char* locale = aLocale;
  if (!std::strcmp(locale, "und")) {
    locale = "";  // ICU root locale
  }
  return locale;
}

/**
 * Ensure a locale is null-terminated, and map the "und" locale to an empty
 * string, which ICU uses internally.
 */
static inline const char* IcuLocale(Span<const char> aLocale) {
  return IcuLocale(AssertNullTerminatedString(aLocale));
}

/**
 * Ensure a locale in the buffer is null-terminated, and map the "und" locale to
 * an empty string, which ICU uses internally.
 */
static inline const char* IcuLocale(const Buffer<char>& aLocale) {
  return IcuLocale(Span(aLocale.begin(), aLocale.Length() - 1));
}

using ICUResult = Result<Ok, ICUError>;

/**
 * Convert a UErrorCode to ICUError. This will correctly apply the OutOfMemory
 * case.
 */
ICUError ToICUError(UErrorCode status);

/**
 * Convert a UErrorCode to ICUResult. This will correctly apply the OutOfMemory
 * case.
 */
ICUResult ToICUResult(UErrorCode status);

/**
 * The ICU status can complain about a string not being terminated, but this
 * is fine for this API, as it deals with the mozilla::Span that has a pointer
 * and a length.
 */
static inline bool ICUSuccessForStringSpan(UErrorCode status) {
  return U_SUCCESS(status) || status == U_STRING_NOT_TERMINATED_WARNING;
}

/**
 * This class enforces that the unified mozilla::intl methods match the
 * const-ness of the underlying ICU4C API calls. const ICU4C APIs take a const
 * pointer, while mutable ones take a non-const pointer.
 *
 * For const ICU4C calls use:
 *   ICUPointer::GetConst().
 *
 * For non-const ICU4C calls use:
 *   ICUPointer::GetMut().
 *
 * This will propagate the `const` specifier from the ICU4C API call to the
 * unified method, and it will be enforced by the compiler. This helps ensures
 * a consistence and correct implementation.
 */
template <typename T>
class ICUPointer {
 public:
  explicit ICUPointer(T* aPointer) : mPointer(aPointer) {}

  // Only allow moves of ICUPointers, no copies.
  ICUPointer(ICUPointer&& other) noexcept = default;
  ICUPointer& operator=(ICUPointer&& other) noexcept = default;

  // Implicitly take ownership of a raw pointer through copy assignment.
  ICUPointer& operator=(T* aPointer) noexcept {
    mPointer = aPointer;
    return *this;
  };

  const T* GetConst() const { return const_cast<const T*>(mPointer); }
  T* GetMut() { return mPointer; }

  explicit operator bool() const { return !!mPointer; }

 private:
  T* mPointer;
};

/**
 * Calling into ICU with the C-API can be a bit tricky. This function wraps up
 * the relatively risky operations involving pointers, lengths, and buffers into
 * a simpler call. This function accepts a lambda that performs the ICU call,
 * and returns the length of characters in the buffer. When using a temporary
 * stack-based buffer, the calls can often be done in one trip. However, if
 * additional memory is needed, this function will call the C-API twice, in
 * order to first get the size of the result, and then second to copy the result
 * over to the buffer.
 */
template <typename ICUStringFunction, typename Buffer>
static ICUResult FillBufferWithICUCall(Buffer& buffer,
                                       const ICUStringFunction& strFn) {
  static_assert(std::is_same_v<typename Buffer::CharType, char16_t> ||
                std::is_same_v<typename Buffer::CharType, char> ||
                std::is_same_v<typename Buffer::CharType, uint8_t>);

  UErrorCode status = U_ZERO_ERROR;
  int32_t length = strFn(buffer.data(), buffer.capacity(), &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    MOZ_ASSERT(length >= 0);

    if (!buffer.reserve(length)) {
      return Err(ICUError::OutOfMemory);
    }

    status = U_ZERO_ERROR;
    mozilla::DebugOnly<int32_t> length2 = strFn(buffer.data(), length, &status);
    MOZ_ASSERT(length == length2);
  }
  if (!ICUSuccessForStringSpan(status)) {
    return Err(ToICUError(status));
  }

  buffer.written(length);

  return Ok{};
}

/**
 * Adaptor for mozilla::Vector to implement the Buffer interface.
 */
template <typename T, size_t N>
class VectorToBufferAdaptor {
  mozilla::Vector<T, N>& vector;

 public:
  using CharType = T;

  explicit VectorToBufferAdaptor(mozilla::Vector<T, N>& vector)
      : vector(vector) {}

  T* data() { return vector.begin(); }

  size_t capacity() const { return vector.capacity(); }

  bool reserve(size_t length) { return vector.reserve(length); }

  void written(size_t length) {
    mozilla::DebugOnly<bool> result = vector.resizeUninitialized(length);
    MOZ_ASSERT(result);
  }
};

/**
 * An overload of FillBufferWithICUCall that accepts a mozilla::Vector rather
 * than a Buffer.
 */
template <typename ICUStringFunction, size_t InlineSize, typename CharType>
static ICUResult FillBufferWithICUCall(Vector<CharType, InlineSize>& vector,
                                       const ICUStringFunction& strFn) {
  VectorToBufferAdaptor buffer(vector);
  return FillBufferWithICUCall(buffer, strFn);
}

#ifndef JS_STANDALONE
/**
 * mozilla::intl APIs require sizeable buffers. This class abstracts over
 * the nsTArray.
 */
template <typename T>
class nsTArrayToBufferAdapter {
 public:
  using CharType = T;

  // Do not allow copy or move. Move could be added in the future if needed.
  nsTArrayToBufferAdapter(const nsTArrayToBufferAdapter&) = delete;
  nsTArrayToBufferAdapter& operator=(const nsTArrayToBufferAdapter&) = delete;

  explicit nsTArrayToBufferAdapter(nsTArray<CharType>& aArray)
      : mArray(aArray) {}

  /**
   * Ensures the buffer has enough space to accommodate |size| elements.
   */
  [[nodiscard]] bool reserve(size_t size) {
    // Use fallible behavior here.
    return mArray.SetCapacity(size, fallible);
  }

  /**
   * Returns the raw data inside the buffer.
   */
  CharType* data() { return mArray.Elements(); }

  /**
   * Returns the count of elements written into the buffer.
   */
  size_t length() const { return mArray.Length(); }

  /**
   * Returns the buffer's overall capacity.
   */
  size_t capacity() const { return mArray.Capacity(); }

  /**
   * Resizes the buffer to the given amount of written elements.
   */
  void written(size_t amount) {
    MOZ_ASSERT(amount <= mArray.Capacity());
    // This sets |mArray|'s internal size so that it matches how much was
    // written. This is necessary because the write happens across FFI
    // boundaries.
    mArray.SetLengthAndRetainStorage(amount);
  }

 private:
  nsTArray<CharType>& mArray;
};

template <typename T, size_t N>
class AutoTArrayToBufferAdapter : public nsTArrayToBufferAdapter<T> {
  using nsTArrayToBufferAdapter<T>::nsTArrayToBufferAdapter;
};

/**
 * An overload of FillBufferWithICUCall that accepts a nsTArray.
 */
template <typename ICUStringFunction, typename CharType>
static ICUResult FillBufferWithICUCall(nsTArray<CharType>& array,
                                       const ICUStringFunction& strFn) {
  nsTArrayToBufferAdapter<CharType> buffer(array);
  return FillBufferWithICUCall(buffer, strFn);
}

template <typename ICUStringFunction, typename CharType, size_t N>
static ICUResult FillBufferWithICUCall(AutoTArray<CharType, N>& array,
                                       const ICUStringFunction& strFn) {
  AutoTArrayToBufferAdapter<CharType, N> buffer(array);
  return FillBufferWithICUCall(buffer, strFn);
}
#endif

/**
 * Fill a UTF-8 or a UTF-16 buffer with a UTF-16 span. ICU4C mostly uses UTF-16
 * internally, but different consumers may have different situations with their
 * buffers.
 */
template <typename Buffer>
[[nodiscard]] bool FillBuffer(Span<const char16_t> utf16Span,
                              Buffer& targetBuffer) {
  static_assert(std::is_same_v<typename Buffer::CharType, char> ||
                std::is_same_v<typename Buffer::CharType, unsigned char> ||
                std::is_same_v<typename Buffer::CharType, char16_t>);

  if constexpr (std::is_same_v<typename Buffer::CharType, char> ||
                std::is_same_v<typename Buffer::CharType, unsigned char>) {
    if (utf16Span.Length() & mozilla::tl::MulOverflowMask<3>::value) {
      // Tripling the size of the buffer overflows the size_t.
      return false;
    }

    if (!targetBuffer.reserve(3 * utf16Span.Length())) {
      return false;
    }

    size_t amount = ConvertUtf16toUtf8(
        utf16Span, Span(reinterpret_cast<char*>(targetBuffer.data()),
                        targetBuffer.capacity()));

    targetBuffer.written(amount);
  }
  if constexpr (std::is_same_v<typename Buffer::CharType, char16_t>) {
    size_t amount = utf16Span.Length();
    if (!targetBuffer.reserve(amount)) {
      return false;
    }
    for (size_t i = 0; i < amount; i++) {
      targetBuffer.data()[i] = utf16Span[i];
    }
    targetBuffer.written(amount);
  }

  return true;
}

/**
 * Fill a UTF-8 or a UTF-16 buffer with a UTF-8 span. ICU4C mostly uses UTF-16
 * internally, but different consumers may have different situations with their
 * buffers.
 */
template <typename Buffer>
[[nodiscard]] bool FillBuffer(Span<const char> utf8Span, Buffer& targetBuffer) {
  static_assert(std::is_same_v<typename Buffer::CharType, char> ||
                std::is_same_v<typename Buffer::CharType, unsigned char> ||
                std::is_same_v<typename Buffer::CharType, char16_t>);

  if constexpr (std::is_same_v<typename Buffer::CharType, char> ||
                std::is_same_v<typename Buffer::CharType, unsigned char>) {
    size_t amount = utf8Span.Length();
    if (!targetBuffer.reserve(amount)) {
      return false;
    }
    for (size_t i = 0; i < amount; i++) {
      targetBuffer.data()[i] =
          // Static cast in case of a mismatch between `unsigned char` and
          // `char`
          static_cast<typename Buffer::CharType>(utf8Span[i]);
    }
    targetBuffer.written(amount);
  }
  if constexpr (std::is_same_v<typename Buffer::CharType, char16_t>) {
    if (!targetBuffer.reserve(utf8Span.Length() + 1)) {
      return false;
    }

    size_t amount = ConvertUtf8toUtf16(
        utf8Span, Span(targetBuffer.data(), targetBuffer.capacity()));

    targetBuffer.written(amount);
  }

  return true;
}

/**
 * It is convenient for callers to be able to pass in UTF-8 strings to the API.
 * This function can be used to convert that to a stack-allocated UTF-16
 * mozilla::Vector that can then be passed into ICU calls. The string will be
 * null terminated.
 */
template <size_t StackSize>
[[nodiscard]] static bool FillUTF16Vector(
    Span<const char> utf8Span,
    mozilla::Vector<char16_t, StackSize>& utf16TargetVec) {
  // Per ConvertUtf8toUtf16: The length of aDest must be at least one greater
  // than the length of aSource. This additional length will be used for null
  // termination.
  if (!utf16TargetVec.reserve(utf8Span.Length() + 1)) {
    return false;
  }

  // ConvertUtf8toUtf16 fills the buffer with the data, but the length of the
  // vector is unchanged.
  size_t length = ConvertUtf8toUtf16(
      utf8Span, Span(utf16TargetVec.begin(), utf16TargetVec.capacity()));

  // Assert that the last element is free for writing a null terminator.
  MOZ_ASSERT(length < utf16TargetVec.capacity());
  utf16TargetVec.begin()[length] = '\0';

  // The call to resizeUninitialized notifies the vector of how much was written
  // exclusive of the null terminated character.
  return utf16TargetVec.resizeUninitialized(length);
}

/**
 * An iterable class that wraps calls to the ICU UEnumeration C API.
 *
 * Usage:
 *
 *  // Make sure the range expression is non-temporary, otherwise there is a
 *  // risk of undefined behavior:
 *  auto result = Calendar::GetBcp47KeywordValuesForLocale("en-US");
 *
 *  for (auto name : result.unwrap()) {
 *    MOZ_ASSERT(name.unwrap(), "An iterable value exists".);
 *  }
 */
template <typename CharType, typename T, T(Mapper)(const CharType*, int32_t)>
class Enumeration {
 public:
  class Iterator;
  friend class Iterator;

  // Transfer ownership of the UEnumeration in the move constructor.
  Enumeration(Enumeration&& other) noexcept
      : mUEnumeration(other.mUEnumeration) {
    other.mUEnumeration = nullptr;
  }

  // Transfer ownership of the UEnumeration in the move assignment operator.
  Enumeration& operator=(Enumeration&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    if (mUEnumeration) {
      uenum_close(mUEnumeration);
    }
    mUEnumeration = other.mUEnumeration;
    other.mUEnumeration = nullptr;
    return *this;
  }

  class Iterator {
    Enumeration& mEnumeration;
    // `Nothing` signifies that no enumeration has been loaded through ICU yet.
    Maybe<int32_t> mIteration = Nothing{};
    const CharType* mNext = nullptr;
    int32_t mNextLength = 0;

   public:
    using value_type = const CharType*;
    using reference = T;
    using iterator_category = std::input_iterator_tag;

    explicit Iterator(Enumeration& aEnumeration, bool aIsBegin)
        : mEnumeration(aEnumeration) {
      if (aIsBegin) {
        AdvanceUEnum();
      }
    }

    Iterator& operator++() {
      AdvanceUEnum();
      return *this;
    }

    Iterator operator++(int) {
      Iterator retval = *this;
      ++(*this);
      return retval;
    }

    bool operator==(Iterator other) const {
      return mIteration == other.mIteration;
    }

    bool operator!=(Iterator other) const { return !(*this == other); }

    T operator*() const {
      // Map the iterated value to something new.
      return Mapper(mNext, mNextLength);
    }

   private:
    void AdvanceUEnum() {
      if (mIteration.isNothing()) {
        mIteration = Some(-1);
      }
      UErrorCode status = U_ZERO_ERROR;
      if constexpr (std::is_same_v<CharType, char16_t>) {
        mNext = uenum_unext(mEnumeration.mUEnumeration, &mNextLength, &status);
      } else {
        static_assert(std::is_same_v<CharType, char>,
                      "Only char16_t and char are supported by "
                      "mozilla::intl::Enumeration.");
        mNext = uenum_next(mEnumeration.mUEnumeration, &mNextLength, &status);
      }
      if (U_FAILURE(status)) {
        mNext = nullptr;
      }

      if (mNext) {
        (*mIteration)++;
      } else {
        // The iterator is complete.
        mIteration = Nothing{};
      }
    }
  };

  Iterator begin() { return Iterator(*this, true); }
  Iterator end() { return Iterator(*this, false); }

  explicit Enumeration(UEnumeration* aUEnumeration)
      : mUEnumeration(aUEnumeration) {}

  ~Enumeration() {
    if (mUEnumeration) {
      // Only close when the object is being destructed, not moved.
      uenum_close(mUEnumeration);
    }
  }

 private:
  UEnumeration* mUEnumeration = nullptr;
};

template <typename CharType>
Result<Span<const CharType>, InternalError> SpanMapper(const CharType* string,
                                                       int32_t length) {
  // Return the raw value from this Iterator.
  if (string == nullptr) {
    return Err(InternalError{});
  }
  MOZ_ASSERT(length >= 0);
  return Span<const CharType>(string, static_cast<size_t>(length));
}

template <typename CharType>
using SpanResult = Result<Span<const CharType>, InternalError>;

template <typename CharType>
using SpanEnumeration = Enumeration<CharType, SpanResult<CharType>, SpanMapper>;

/**
 * An iterable class that wraps calls to ICU's available locales API.
 */
template <int32_t(CountAvailable)(), const char*(GetAvailable)(int32_t)>
class AvailableLocalesEnumeration final {
  // The overall count of available locales.
  int32_t mLocalesCount = 0;

 public:
  AvailableLocalesEnumeration() { mLocalesCount = CountAvailable(); }

  class Iterator {
   public:
    // std::iterator traits.
    using iterator_category = std::input_iterator_tag;
    using value_type = const char*;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

   private:
    // The current position in the list of available locales.
    int32_t mLocalesPos = 0;

   public:
    explicit Iterator(int32_t aLocalesPos) : mLocalesPos(aLocalesPos) {}

    Iterator& operator++() {
      mLocalesPos++;
      return *this;
    }

    Iterator operator++(int) {
      Iterator result = *this;
      ++(*this);
      return result;
    }

    bool operator==(const Iterator& aOther) const {
      return mLocalesPos == aOther.mLocalesPos;
    }

    bool operator!=(const Iterator& aOther) const { return !(*this == aOther); }

    value_type operator*() const { return GetAvailable(mLocalesPos); }
  };

  // std::iterator begin() and end() methods.

  /**
   * Return an iterator pointing to the first available locale.
   */
  Iterator begin() const { return Iterator(0); }

  /**
   * Return an iterator pointing to one past the last available locale.
   */
  Iterator end() const { return Iterator(mLocalesCount); }
};

/**
 * A helper class to wrap calling ICU function in cpp file so we don't have to
 * include the ICU header here.
 */
class FormattedResult {
 protected:
  static Result<Span<const char16_t>, ICUError> ToSpanImpl(
      const UFormattedValue* value);
};

/**
 * A RAII class to hold the formatted value of format result.
 *
 * The caller will need to create this AutoFormattedResult on the stack, with
 * the following parameters:
 * 1. Native ICU type.
 * 2. An ICU function which opens the result.
 * 3. An ICU function which can get the result as UFormattedValue.
 * 4. An ICU function which closes the result.
 *
 * After the object is created, caller needs to call IsValid() method to check
 * if the native object has been created properly, and then passes this
 * object to other format interfaces.
 * The format result will be stored in this object, the caller can use ToSpan()
 * method to get the formatted string.
 *
 * The methods GetFormatted() and Value() are private methods since they expose
 * native ICU types. If the caller wants to call these methods, the caller needs
 * to register itself as a friend class in AutoFormattedResult.
 *
 * The formatted value and the native ICU object will be released once this
 * class is destructed.
 */
template <typename T, T*(Open)(UErrorCode*),
          const UFormattedValue*(GetValue)(const T*, UErrorCode*),
          void(Close)(T*)>
class MOZ_RAII AutoFormattedResult : FormattedResult {
 public:
  AutoFormattedResult() {
    mFormatted = Open(&mError);
    if (U_FAILURE(mError)) {
      mFormatted = nullptr;
    }
  }
  ~AutoFormattedResult() {
    if (mFormatted) {
      Close(mFormatted);
    }
  }

  AutoFormattedResult(const AutoFormattedResult& other) = delete;
  AutoFormattedResult& operator=(const AutoFormattedResult& other) = delete;

  AutoFormattedResult(AutoFormattedResult&& other) = delete;
  AutoFormattedResult& operator=(AutoFormattedResult&& other) = delete;

  /**
   * Check if the native UFormattedDateInterval was created successfully.
   */
  bool IsValid() const { return !!mFormatted; }

  /**
   *  Get error code if IsValid() returns false.
   */
  ICUError GetError() const { return ToICUError(mError); }

  /**
   * Get the formatted result.
   */
  Result<Span<const char16_t>, ICUError> ToSpan() const {
    if (!IsValid()) {
      return Err(GetError());
    }

    const UFormattedValue* value = Value();
    if (!value) {
      return Err(ICUError::InternalError);
    }

    return ToSpanImpl(value);
  }

 private:
  friend class DateIntervalFormat;
  friend class ListFormat;
  T* GetFormatted() const { return mFormatted; }

  const UFormattedValue* Value() const {
    if (!IsValid()) {
      return nullptr;
    }

    UErrorCode status = U_ZERO_ERROR;
    const UFormattedValue* value = GetValue(mFormatted, &status);
    if (U_FAILURE(status)) {
      return nullptr;
    }

    return value;
  };

  T* mFormatted = nullptr;
  UErrorCode mError = U_ZERO_ERROR;
};
}  // namespace mozilla::intl

#endif /* intl_components_ICUUtils_h */
