/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RegexpShim_h
#define RegexpShim_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/Types.h"

#include <algorithm>

#include "jit/Label.h"
#include "js/Value.h"
#include "new-regexp/util/flags.h"
#include "new-regexp/util/vector.h"
#include "new-regexp/util/zone.h"
#include "vm/NativeObject.h"

// Forward declaration of classes
namespace v8 {
namespace internal {

class Heap;
class Isolate;
class RegExpMatchInfo;
class RegExpStack;

}  // namespace internal
}  // namespace v8

#define V8_WARN_UNUSED_RESULT MOZ_MUST_USE
#define V8_EXPORT_PRIVATE MOZ_EXPORT
#define V8_FALLTHROUGH [[fallthrough]]

#define FATAL(x) MOZ_CRASH(x)
#define UNREACHABLE() MOZ_CRASH("unreachable code")
#define UNIMPLEMENTED() MOZ_CRASH("unimplemented code")
#define STATIC_ASSERT(exp) static_assert(exp, #exp)

#define DCHECK MOZ_ASSERT
#define DCHECK_EQ(lhs, rhs) MOZ_ASSERT((lhs) == (rhs))
#define DCHECK_NE(lhs, rhs) MOZ_ASSERT((lhs) != (rhs))
#define DCHECK_GT(lhs, rhs) MOZ_ASSERT((lhs) > (rhs))
#define DCHECK_GE(lhs, rhs) MOZ_ASSERT((lhs) >= (rhs))
#define DCHECK_LT(lhs, rhs) MOZ_ASSERT((lhs) < (rhs))
#define DCHECK_LE(lhs, rhs) MOZ_ASSERT((lhs) <= (rhs))
#define DCHECK_NULL(val) MOZ_ASSERT((val) == nullptr)
#define DCHECK_NOT_NULL(val) MOZ_ASSERT((val) != nullptr)
#define DCHECK_IMPLIES(lhs, rhs) MOZ_ASSERT_IF(lhs, rhs)
#define CHECK MOZ_RELEASE_ASSERT

template <class T>
static constexpr inline T Min(T t1, T t2) {
  return t1 < t2 ? t1 : t2;
}

template <class T>
static constexpr inline T Max(T t1, T t2) {
  return t1 > t2 ? t1 : t2;
}
#define MemCopy memcpy

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/base/macros.h#L310-L319
// ptrdiff_t is 't' according to the standard, but MSVC uses 'I'.
#ifdef _MSC_VER
#  define V8PRIxPTRDIFF "Ix"
#  define V8PRIdPTRDIFF "Id"
#  define V8PRIuPTRDIFF "Iu"
#else
#  define V8PRIxPTRDIFF "tx"
#  define V8PRIdPTRDIFF "td"
#  define V8PRIuPTRDIFF "tu"
#endif

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/base/macros.h#L27-L38
// The arraysize(arr) macro returns the # of elements in an array arr.
// The expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.  If you use arraysize on
// a pointer by mistake, you will get a compile-time error.
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

// Explicitly declare the assignment operator as deleted.
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete

// Explicitly declare the copy constructor and assignment operator as deleted.
// This also deletes the implicit move constructor and implicit move assignment
// operator, but still allows to manually define them.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  DISALLOW_ASSIGN(TypeName)

// Explicitly declare all implicit constructors as deleted, namely the
// default constructor, copy constructor and operator= functions.
// This is especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

namespace v8 {

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/base/macros.h#L364-L367
template <typename T, typename U>
constexpr inline bool IsAligned(T value, U alignment) {
  return (value & (alignment - 1)) == 0;
}

using byte = uint8_t;
using Address = uintptr_t;
static const Address kNullAddress = 0;

namespace base {

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/base/macros.h#L247-L258
// The USE(x, ...) template is used to silence C++ compiler warnings
// issued for (yet) unused variables (typically parameters).
// The arguments are guaranteed to be evaluated from left to right.
struct Use {
  template <typename T>
  Use(T&&) {}  // NOLINT(runtime/explicit)
};
#define USE(...)                                                   \
  do {                                                             \
    ::v8::base::Use unused_tmp_array_for_use_macro[]{__VA_ARGS__}; \
    (void)unused_tmp_array_for_use_macro;                          \
  } while (false)

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/base/safe_conversions.h#L35-L39
// saturated_cast<> is analogous to static_cast<> for numeric types, except
// that the specified numeric conversion will saturate rather than overflow or
// underflow.
template <typename Dst, typename Src>
inline Dst saturated_cast(Src value);

// This is the only specialization that is needed for regexp code.
// Instead of pulling in dozens of lines of template goo
// to derive it, I used the implementation from uint8_clamped in
// ArrayBufferObject.h.
template <>
inline uint8_t saturated_cast<uint8_t, int>(int x) {
  return (x >= 0) ? ((x < 255) ? uint8_t(x) : 255) : 0;
}

namespace bits {

inline uint64_t CountTrailingZeros(uint64_t value) {
  return mozilla::CountTrailingZeroes64(value);
}

}  // namespace bits
}  // namespace base

namespace unibrow {

using uchar = unsigned int;

// Origin:
// https://github.com/v8/v8/blob/1f1e4cdb04c75eab77adbecd5f5514ddc3eb56cf/src/strings/unicode.h#L133-L150
class Latin1 {
 public:
  static const uint16_t kMaxChar = 0xff;

  // Convert the character to Latin-1 case equivalent if possible.
  static inline uint16_t TryConvertToLatin1(uint16_t c) {
    // "GREEK CAPITAL LETTER MU" case maps to "MICRO SIGN".
    // "GREEK SMALL LETTER MU" case maps to "MICRO SIGN".
    if (c == 0x039C || c == 0x03BC) {
      return 0xB5;
    }
    // "LATIN CAPITAL LETTER Y WITH DIAERESIS" case maps to "LATIN SMALL LETTER
    // Y WITH DIAERESIS".
    if (c == 0x0178) {
      return 0xFF;
    }
    return c;
  }
};

// Origin:
// https://github.com/v8/v8/blob/b4bfbce6f91fc2cc72178af42bb3172c5f5eaebb/src/strings/unicode.h#L99-L131
class Utf16 {
 public:
  static inline bool IsLeadSurrogate(int code) {
    return js::unicode::IsLeadSurrogate(code);
  }
  static inline bool IsTrailSurrogate(int code) {
    return js::unicode::IsTrailSurrogate(code);
  }
  static inline uint16_t LeadSurrogate(uint32_t char_code) {
    return js::unicode::LeadSurrogate(char_code);
  }
  static inline uint16_t TrailSurrogate(uint32_t char_code) {
    return js::unicode::TrailSurrogate(char_code);
  }
  static inline uint32_t CombineSurrogatePair(char16_t lead, char16_t trail) {
    return js::unicode::UTF16Decode(lead, trail);
  }
  static const uchar kMaxNonSurrogateCharCode = 0xffff;
};

// A cache used in case conversion.  It caches the value for characters
// that either have no mapping or map to a single character independent
// of context.  Characters that map to more than one character or that
// map differently depending on context are always looked up.
// Origin:
// https://github.com/v8/v8/blob/b4bfbce6f91fc2cc72178af42bb3172c5f5eaebb/src/strings/unicode.h#L64-L88
template <class T, int size = 256>
class Mapping {
 public:
  inline Mapping() = default;
  int get(uchar c, uchar n, uchar* result);

 private:
  friend class Test;
  int CalculateValue(uchar c, uchar n, uchar* result);
  struct CacheEntry {
    inline CacheEntry() : code_point_(kNoChar), offset_(0) {}
    inline CacheEntry(uchar code_point, signed offset)
        : code_point_(code_point), offset_(offset) {}
    uchar code_point_;
    signed offset_;
    static const int kNoChar = (1 << 21) - 1;
  };
  static const int kSize = size;
  static const int kMask = kSize - 1;
  CacheEntry entries_[kSize];
};

// Origin:
// https://github.com/v8/v8/blob/b4bfbce6f91fc2cc72178af42bb3172c5f5eaebb/src/strings/unicode.h#L241-L252
struct Ecma262Canonicalize {
  static const int kMaxWidth = 1;
  static int Convert(uchar c, uchar n, uchar* result, bool* allow_caching_ptr);
};
struct Ecma262UnCanonicalize {
  static const int kMaxWidth = 1;
  static int Convert(uchar c, uchar n, uchar* result, bool* allow_caching_ptr);
};
struct CanonicalizationRange {
  static const int kMaxWidth = 1;
  static int Convert(uchar c, uchar n, uchar* result, bool* allow_caching_ptr);
};

}  // namespace unibrow

namespace internal {

#define PRINTF_FORMAT(x, y) MOZ_FORMAT_PRINTF(x, y)
void PRINTF_FORMAT(1, 2) PrintF(const char* format, ...);
void PRINTF_FORMAT(2, 3) PrintF(FILE* out, const char* format, ...);

// Superclass for classes only using static method functions.
// The subclass of AllStatic cannot be instantiated at all.
class AllStatic {
#ifdef DEBUG
 public:
  AllStatic() = delete;
#endif
};

// Superclass for classes managed with new and delete.
// In irregexp, this is only AlternativeGeneration (in regexp-compiler.cc)
// Compare:
// https://github.com/v8/v8/blob/7b3332844212d78ee87a9426f3a6f7f781a8fbfa/src/utils/allocation.cc#L88-L96
class Malloced {
 public:
  static void* operator new(size_t size) {
    js::AutoEnterOOMUnsafeRegion oomUnsafe;
    void* result = js_malloc(size);
    if (!result) {
      oomUnsafe.crash("Irregexp Malloced shim");
    }
    return result;
  }
  static void operator delete(void* p) { js_free(p); }
};

constexpr int32_t KB = 1024;
constexpr int32_t MB = 1024 * 1024;

#define kMaxInt JSVAL_INT_MAX
#define kMinInt JSVAL_INT_MIN
constexpr int kSystemPointerSize = sizeof(void*);

// The largest integer n such that n and n + 1 are both exactly
// representable as a Number value.  ES6 section 20.1.2.6
constexpr double kMaxSafeInteger = 9007199254740991.0;  // 2^53-1

// Latin1/UTF-16 constants
// Code-point values in Unicode 4.0 are 21 bits wide.
// Code units in UTF-16 are 16 bits wide.
using uc16 = uint16_t;
using uc32 = int32_t;

constexpr int kBitsPerByte = 8;
constexpr int kBitsPerByteLog2 = 3;
constexpr int kUInt32Size = sizeof(uint32_t);
constexpr int kInt64Size = sizeof(int64_t);
constexpr int kUC16Size = sizeof(uc16);

inline constexpr bool IsDecimalDigit(uc32 c) { return c >= '0' && c <= '9'; }
inline bool is_uint24(int val) { return (val & 0x00ffffff) == val; }

inline bool IsIdentifierStart(uc32 c) {
  return js::unicode::IsIdentifierStart(uint32_t(c));
}
inline bool IsIdentifierPart(uc32 c) {
  return js::unicode::IsIdentifierPart(uint32_t(c));
}

// Wrappers to disambiguate uint16_t and uc16.
struct AsUC16 {
  explicit AsUC16(uint16_t v) : value(v) {}
  uint16_t value;
};

struct AsUC32 {
  explicit AsUC32(int32_t v) : value(v) {}
  int32_t value;
};

class StdoutStream : public std::ostream {};

std::ostream& operator<<(std::ostream& os, const AsUC16& c);
std::ostream& operator<<(std::ostream& os, const AsUC32& c);

// Reuse existing Maybe implementation
using mozilla::Maybe;

template <typename T>
Maybe<T> Just(const T& value) {
  return mozilla::Some(value);
}

template <typename T>
mozilla::Nothing Nothing() {
  return mozilla::Nothing();
}

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/utils/utils.h#L600-L642
// Compare 8bit/16bit chars to 8bit/16bit chars.
// Used indirectly by regexp-interpreter.cc
template <typename lchar, typename rchar>
inline int CompareCharsUnsigned(const lchar* lhs, const rchar* rhs,
                                size_t chars) {
  const lchar* limit = lhs + chars;
  if (sizeof(*lhs) == sizeof(char) && sizeof(*rhs) == sizeof(char)) {
    // memcmp compares byte-by-byte, yielding wrong results for two-byte
    // strings on little-endian systems.
    return memcmp(lhs, rhs, chars);
  }
  while (lhs < limit) {
    int r = static_cast<int>(*lhs) - static_cast<int>(*rhs);
    if (r != 0) return r;
    ++lhs;
    ++rhs;
  }
  return 0;
}
template <typename lchar, typename rchar>
inline int CompareChars(const lchar* lhs, const rchar* rhs, size_t chars) {
  DCHECK_LE(sizeof(lchar), 2);
  DCHECK_LE(sizeof(rchar), 2);
  if (sizeof(lchar) == 1) {
    if (sizeof(rchar) == 1) {
      return CompareCharsUnsigned(reinterpret_cast<const uint8_t*>(lhs),
                                  reinterpret_cast<const uint8_t*>(rhs), chars);
    } else {
      return CompareCharsUnsigned(reinterpret_cast<const uint8_t*>(lhs),
                                  reinterpret_cast<const uint16_t*>(rhs),
                                  chars);
    }
  } else {
    if (sizeof(rchar) == 1) {
      return CompareCharsUnsigned(reinterpret_cast<const uint16_t*>(lhs),
                                  reinterpret_cast<const uint8_t*>(rhs), chars);
    } else {
      return CompareCharsUnsigned(reinterpret_cast<const uint16_t*>(lhs),
                                  reinterpret_cast<const uint16_t*>(rhs),
                                  chars);
    }
  }
}

// Origin:
// https://github.com/v8/v8/blob/855591a54d160303349a5f0a32fab15825c708d1/src/utils/utils.h#L40-L48
// Returns the value (0 .. 15) of a hexadecimal character c.
// If c is not a legal hexadecimal character, returns a value < 0.
// Used in regexp-parser.cc
inline int HexValue(uc32 c) {
  c -= '0';
  if (static_cast<unsigned>(c) <= 9) return c;
  c = (c | 0x20) - ('a' - '0');  // detect 0x11..0x16 and 0x31..0x36.
  if (static_cast<unsigned>(c) <= 5) return c + 10;
  return -1;
}

// V8::Object ~= JS::Value
class Object {
 public:
  // The default object constructor in V8 stores a nullptr,
  // which has its low bit clear and is interpreted as Smi(0).
  constexpr Object() : value_(JS::Int32Value(0)) {}

  // Conversions to/from SpiderMonkey types
  constexpr Object(JS::Value value) : value_(value) {}
  operator JS::Value() const { return value_; }

  // Used in regexp-macro-assembler.cc and regexp-interpreter.cc to
  // check the return value of isolate->stack_guard()->HandleInterrupts()
  // In V8, this will be either an exception object or undefined.
  // In SM, we store the exception in the context, so we can use our normal
  // idiom: return false iff we are throwing an exception.
  inline bool IsException(Isolate*) const { return !value_.toBoolean(); }

  // SpiderMonkey tries to avoid leaking the internal representation of its
  // objects. V8 is not so strict. These functions are used when calling /
  // being called by native code: objects are converted to Addresses for the
  // call, then cast back to objects on the other side.
  // We might be able to upstream a patch that eliminates the need for these.
  Object(Address bits);
  Address ptr() const;

 protected:
  JS::Value value_;
};

class Smi : public Object {
 public:
  static Smi FromInt(int32_t value) {
    Smi smi;
    smi.value_ = JS::Int32Value(value);
    return smi;
  }
  static inline int32_t ToInt(const Object object) {
    return JS::Value(object).toInt32();
  }
};

// V8::HeapObject ~= JSObject
class HeapObject : public Object {
public:
  // Only used for bookkeeping of total code generated in regexp-compiler.
  // We may be able to refactor this away.
  int Size() const;
};

// A fixed-size array with Objects (aka Values) as element types
// Implemented as a wrapper around a regular native object with dense elements.
class FixedArray : public HeapObject {
 public:
  inline void set(uint32_t index, Object value) {
    JS::Value(*this).toObject().as<js::NativeObject>().setDenseElement(index,
                                                                       value);
  }
};

// A fixed-size array of bytes.
// TODO: figure out the best implementation for this. Uint8Array might work,
// but it's not currently visible outside of TypedArrayObject.cpp.
class ByteArray : public HeapObject {
 public:
  uint8_t get(uint32_t index);
  void set(uint32_t index, uint8_t val);
  uint32_t length();
  byte* GetDataStartAddress();
  byte* GetDataEndAddress();

  static ByteArray cast(Object object);
};

// Like Handles in SM, V8 handles are references to marked pointers.
// Unlike SM, where Rooted pointers are created individually on the
// stack, the target of a V8 handle lives in a HandleScope.
// HandleScopes are created on the stack and register themselves with
// the isolate (~= JSContext). Whenever a Handle is created, the
// outermost HandleScope is retrieved from the isolate, and a new root
// is created in that HandleScope. The Handle remains valid for the
// lifetime of the HandleScope.
class HandleScope {
 public:
  HandleScope(Isolate* isolate);
};

// Origin:
// https://github.com/v8/v8/blob/5792f3587116503fc047d2f68c951c72dced08a5/src/handles/handles.h#L88-L171
template <typename T>
class Handle {
 public:
  Handle();
  Handle(T object, Isolate* isolate);

  // Constructor for handling automatic up casting.
  template <typename S, typename = typename std::enable_if<
                            std::is_convertible<S*, T*>::value>::type>
  /*inline*/ Handle(Handle<S> handle);

  template <typename S>
  /*inline*/ static const Handle<T> cast(Handle<S> that);

  T* operator->() const;
  T operator*() const;

  bool is_null() const;

  Address address();

 private:
  template <typename>
  friend class Handle;
  template <typename>
  friend class MaybeHandle;
};

// A Handle can be converted into a MaybeHandle. Converting a MaybeHandle
// into a Handle requires checking that it does not point to nullptr.  This
// ensures nullptr checks before use.
//
// Also note that Handles do not provide default equality comparison or hashing
// operators on purpose. Such operators would be misleading, because intended
// semantics is ambiguous between Handle location and object identity.
// Origin:
// https://github.com/v8/v8/blob/5792f3587116503fc047d2f68c951c72dced08a5/src/handles/maybe-handles.h#L15-L78
template <typename T>
class MaybeHandle final {
 public:
  MaybeHandle() = default;

  // Constructor for handling automatic up casting from Handle.
  // Ex. Handle<JSArray> can be passed when MaybeHandle<Object> is expected.
  template <typename S, typename = typename std::enable_if<
                            std::is_convertible<S*, T*>::value>::type>
  MaybeHandle(Handle<S> handle);

  /*inline*/ Handle<T> ToHandleChecked() const;

  // Convert to a Handle with a type that can be upcasted to.
  template <typename S>
  /*inline*/ bool ToHandle(Handle<S>* out) const;
};

// From v8/src/handles/handles-inl.h

template <typename T>
inline Handle<T> handle(T object, Isolate* isolate) {
  return Handle<T>(object, isolate);
}

// RAII Guard classes

class DisallowHeapAllocation {
 public:
  DisallowHeapAllocation() {}
  operator const JS::AutoAssertNoGC&() const { return no_gc_; }

 private:
  const JS::AutoAssertNoGC no_gc_;
};

// This is used inside DisallowHeapAllocation regions to enable
// allocation just before throwing an exception, to allocate the
// exception object. Specifically, it only ever guards:
// - isolate->stack_guard()->HandleInterrupts()
// - isolate->StackOverflow()
// Those cases don't allocate in SpiderMonkey, so this can be a no-op.
class AllowHeapAllocation {
 public:
  // Empty constructor to avoid unused_variable warnings
  AllowHeapAllocation() {}
};

class DisallowJavascriptExecution {
 public:
  DisallowJavascriptExecution(Isolate* isolate);

 private:
  js::AutoAssertNoContentJS nojs_;
};

// Origin:
// https://github.com/v8/v8/blob/84f3877c15bc7f8956d21614da4311337525a3c8/src/objects/string.h#L83-L474
class String : public HeapObject {
 private:
  JSString* str() const { return value_.toString(); }

 public:
  operator JSString*() const { return str(); }

  // Max char codes.
  static const int32_t kMaxOneByteCharCode = unibrow::Latin1::kMaxChar;
  static const uint32_t kMaxOneByteCharCodeU = unibrow::Latin1::kMaxChar;
  static const int kMaxUtf16CodeUnit = 0xffff;
  static const uc32 kMaxCodePoint = 0x10ffff;

  MOZ_ALWAYS_INLINE int length() const { return str()->length(); }
  uint16_t Get(uint32_t index);
  bool IsFlat() { return str()->isLinear(); };

  // These are only used in V8 functions that I want to rewrite.
  // TODO: Rewrite those functions and delete this
  bool IsConsString();
  bool IsExternalString();
  bool IsExternalOneByteString();
  bool IsExternalTwoByteString();
  bool IsSeqString();
  bool IsSeqOneByteString();
  bool IsSeqTwoByteString();
  bool IsSlicedString();
  bool IsThinString();

  // Origin:
  // https://github.com/v8/v8/blob/84f3877c15bc7f8956d21614da4311337525a3c8/src/objects/string.h#L95-L152
  class FlatContent {
   public:
    FlatContent(JSLinearString* string, const DisallowHeapAllocation& no_gc)
        : string_(string), no_gc_(no_gc) {}
    inline bool IsOneByte() const { return string_->hasLatin1Chars(); }
    inline bool IsTwoByte() const { return !string_->hasLatin1Chars(); }

    Vector<const uint8_t> ToOneByteVector() const {
      MOZ_ASSERT(IsOneByte());
      return Vector<const uint8_t>(string_->latin1Chars(no_gc_),
                                   string_->length());
    }
    Vector<const uc16> ToUC16Vector() const;
    // TODO: twoByteChars returns char16_t*, but uc16 is uint16_t, which is
    // not compatible :( :( :(
    // {
    //   MOZ_ASSERT(IsTwoByte());
    //   return Vector<const uc16>(string_->twoByteChars(no_gc_),
    //   string_->length());
    // }
   private:
    const JSLinearString* string_;
    const JS::AutoAssertNoGC& no_gc_;
  };
  FlatContent GetFlatContent(const DisallowHeapAllocation& no_gc) {
    MOZ_ASSERT(IsFlat());
    return FlatContent(&str()->asLinear(), no_gc);
  }

  static Handle<String> Flatten(Isolate* isolate, Handle<String> string);

  inline static String cast(Object object) {
    String s;
    s.value_ = JS::StringValue(JS::Value(object).toString());
    return s;
  }

  inline static bool IsOneByteRepresentationUnderneath(String string) {
    return string.str()->hasLatin1Chars();
  }
  inline bool IsOneByteRepresentation() const {
    return str()->hasLatin1Chars();
  }

  std::unique_ptr<char[]> ToCString();

  template <typename Char>
  Vector<const Char> GetCharVector(const DisallowHeapAllocation& no_gc);
};

// A flat string reader provides random access to the contents of a
// string independent of the character width of the string.  The handle
// must be valid as long as the reader is being used.
// Origin:
// https://github.com/v8/v8/blob/84f3877c15bc7f8956d21614da4311337525a3c8/src/objects/string.h#L807-L825
class MOZ_STACK_CLASS FlatStringReader {
 public:
  FlatStringReader(JSAtom* string) : string_(string) {}
  int length() { return string_->length(); }

  inline char16_t Get(size_t index) {
    return string_->latin1OrTwoByteChar(index);
  }

 private:
  JSAtom* string_;
  JS::AutoCheckCannotGC nogc;
};

//////////////////////////////////////////////////
// TODO: Refactor NativeRegExpMacroAssembler and delete all of these:
class ConsString : public String {
 public:
  String first();
  String second();

  static ConsString cast(Object object);
};
class ExternalOneByteString : public String {
 public:
  const uint8_t* GetChars();
  static ExternalOneByteString cast(Object object);
};
class ExternalTwoByteString : public String {
 public:
  const uc16* GetChars();
  static ExternalTwoByteString cast(Object object);
};
class SeqOneByteString : public String {
 public:
  uint8_t* GetChars(const DisallowHeapAllocation& no_gc);
  static SeqOneByteString cast(Object object);
};
class SeqTwoByteString : public String {
 public:
  uc16* GetChars(const DisallowHeapAllocation& no_gc);
  static SeqTwoByteString cast(Object object);

  static constexpr size_t kMaxCharsSize = JSString::MAX_LENGTH * 2;
};
class SlicedString : public String {
 public:
  String parent();
  int offset();
  static SlicedString cast(Object object);
};
class ThinString : public String {
 public:
  String actual();
  static ThinString cast(Object object);
};
class StringShape {
 public:
  explicit StringShape(const String s);
  bool IsCons();
  bool IsSliced();
  bool IsThin();
};
// End of "TODO: Delete all of these"
//////////////////////////////////////////////////

class JSRegExp : public HeapObject {
 public:
  // ******************************************************
  // Methods that are called from inside the implementation
  // ******************************************************
  void TierUpTick();
  bool MarkedForTierUp() const;

  Object Code(bool is_latin1) const;
  Object Bytecode(bool is_latin1) const;

  uint32_t BacktrackLimit() const;

  static JSRegExp cast(Object object);

  // ******************************
  // Static constants
  // ******************************

  // Meaning of Type:
  // NOT_COMPILED: Initial value. No data has been stored in the JSRegExp yet.
  // ATOM: A simple string to match against using an indexOf operation.
  // IRREGEXP: Compiled with Irregexp.
  enum Type { NOT_COMPILED, ATOM, IRREGEXP };

  // Maximum number of captures allowed.
  static constexpr int kMaxCaptures = 1 << 16;

  // **************************************************
  // JSRegExp::Flags
  // **************************************************

  struct FlagShiftBit {
    static constexpr int kGlobal = 0;
    static constexpr int kIgnoreCase = 1;
    static constexpr int kMultiline = 2;
    static constexpr int kSticky = 3;
    static constexpr int kUnicode = 4;
    static constexpr int kDotAll = 5;
    static constexpr int kInvalid = 6;
  };
  enum Flag : uint8_t {
    kNone = 0,
    kGlobal = 1 << FlagShiftBit::kGlobal,
    kIgnoreCase = 1 << FlagShiftBit::kIgnoreCase,
    kMultiline = 1 << FlagShiftBit::kMultiline,
    kSticky = 1 << FlagShiftBit::kSticky,
    kUnicode = 1 << FlagShiftBit::kUnicode,
    kDotAll = 1 << FlagShiftBit::kDotAll,
    kInvalid = 1 << FlagShiftBit::kInvalid,  // Not included in FlagCount.
  };
  using Flags = base::Flags<Flag>;
  static constexpr int kFlagCount = 6;

  static constexpr int kNoBacktrackLimit = 0;
};

class Histogram {
 public:
  inline void AddSample(int sample) {}
};

class Counters {
 public:
  Histogram* regexp_backtracks() { return &regexp_backtracks_; }

 private:
  Histogram regexp_backtracks_;
};

#define PROFILE(isolate, call) \
  do {                         \
  } while (false);

enum class AllocationType : uint8_t {
  kYoung,  // Allocate in the nursery
  kOld,    // Allocate in the tenured heap
};

using StackGuard = Isolate;
using Factory = Isolate;

class Isolate {
 public:
  //********** Isolate code **********//
  RegExpStack* regexp_stack() const { return regexp_stack_; }
  bool has_pending_exception() { return cx()->isExceptionPending(); }
  void StackOverflow() { js::ReportOverRecursed(cx()); }

  unibrow::Mapping<unibrow::Ecma262UnCanonicalize>* jsregexp_uncanonicalize();
  unibrow::Mapping<unibrow::Ecma262Canonicalize>*
  regexp_macro_assembler_canonicalize();
  unibrow::Mapping<unibrow::CanonicalizationRange>* jsregexp_canonrange();

  // An empty stub for telemetry we don't support
  void IncreaseTotalRegexpCodeGenerated(int size) {}

  Counters* counters() { return &counters_; }

  //********** Factory code **********//
  inline Factory* factory() { return this; }

  Handle<ByteArray> NewByteArray(
      int length, AllocationType allocation = AllocationType::kYoung);
  MOZ_MUST_USE MaybeHandle<String> NewStringFromOneByte(
      const Vector<const uint8_t>& str,
      AllocationType allocation = AllocationType::kYoung);

  // Allocates a fixed array initialized with undefined values.
  Handle<FixedArray> NewFixedArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  template <typename Char>
  Handle<String> InternalizeString(const Vector<const Char>& str);

  //********** Stack guard code **********//
  inline StackGuard* stack_guard() { return this; }
  Object HandleInterrupts() {
    return Object(JS::BooleanValue(cx()->handleInterrupt()));
  }

  JSContext* cx() const { return cx_; }

 private:
  JSContext* cx_;
  RegExpStack* regexp_stack_;
  Counters counters_;
};

// Origin:
// https://github.com/v8/v8/blob/50dcf2af54ce27801a71c47c1be1d2c5e36b0dd6/src/execution/isolate.h#L1909-L1931
class StackLimitCheck {
 public:
  StackLimitCheck(Isolate* isolate) : cx_(isolate->cx()) {}

  // Use this to check for stack-overflows in C++ code.
  bool HasOverflowed() { return !CheckRecursionLimitDontReport(cx_); }

  // Use this to check for interrupt request in C++ code.
  bool InterruptRequested() { return cx_->hasAnyPendingInterrupt(); }

  // Use this to check for stack-overflow when entering runtime from JS code.
  bool JsHasOverflowed() {
    return !CheckRecursionLimitConservativeDontReport(cx_);
  }

 private:
  JSContext* cx_;
};

class Code {
 public:
  bool operator!=(Code& other) const;

  Address raw_instruction_start();
  Address raw_instruction_end();
  Address address();

  static Code cast(Object object);
};

// GeneratedCode provides an interface for calling into jit code.
// It will probably require additional work to hook this up to the
// arm simulator.
// Origin:
// https://github.com/v8/v8/blob/abfbe7687edb5b2dffe0b33b24e0a41bb86a8214/src/execution/simulator.h#L96-L164
template <typename Return, typename... Args>
class GeneratedCode {
 public:
  using Signature = Return(Args...);

  static GeneratedCode FromCode(Code code);  // TODO: implement
  Return Call(Args... args) { return fn_ptr_(args...); }

 private:
  friend class GeneratedCode<Return(Args...)>;
  Isolate* isolate_;
  Signature* fn_ptr_;
  GeneratedCode(Isolate* isolate, Signature* fn_ptr)
      : isolate_(isolate), fn_ptr_(fn_ptr) {}
};

// Allow to use {GeneratedCode<ret(arg1, arg2)>} instead of
// {GeneratedCode<ret, arg1, arg2>}.
template <typename Return, typename... Args>
class GeneratedCode<Return(Args...)> : public GeneratedCode<Return, Args...> {
 public:
  // Automatically convert from {GeneratedCode<ret, arg1, arg2>} to
  // {GeneratedCode<ret(arg1, arg2)>}.
  GeneratedCode(GeneratedCode<Return, Args...> other)
      : GeneratedCode<Return, Args...>(other.isolate_, other.fn_ptr_) {}
};

enum class MessageTemplate { kStackOverflow };

class MessageFormatter {
 public:
  static const char* TemplateString(MessageTemplate index) {
    switch (index) {
      case MessageTemplate::kStackOverflow:
        return "too much recursion";
    }
  }
};

// Origin: https://github.com/v8/v8/blob/master/src/codegen/label.h
class Label {
 public:
  Label() : inner_(js::jit::Label()) {}

  operator js::jit::Label*() { return &inner_; }

  void Unuse() { inner_.reset(); }

  bool is_linked() { return inner_.used(); }
  bool is_bound() { return inner_.bound(); }
  bool is_unused() { return !inner_.used() && !inner_.bound(); }

  int pos() { return inner_.offset(); }
  void link_to(int pos) { inner_.use(pos); }
  void bind_to(int pos) { inner_.bind(pos); }

 private:
  js::jit::Label inner_;
};

// TODO: Map flags to jitoptions
extern bool FLAG_correctness_fuzzer_suppressions;
extern bool FLAG_enable_regexp_unaligned_accesses;
extern bool FLAG_harmony_regexp_sequence;
extern bool FLAG_regexp_interpret_all;
extern bool FLAG_regexp_mode_modifiers;
extern bool FLAG_regexp_optimization;
extern bool FLAG_regexp_peephole_optimization;
extern bool FLAG_regexp_possessive_quantifier;
extern bool FLAG_regexp_tier_up;
extern bool FLAG_trace_regexp_assembler;
extern bool FLAG_trace_regexp_bytecodes;
extern bool FLAG_trace_regexp_parser;
extern bool FLAG_trace_regexp_peephole_optimization;

}  // namespace internal
}  // namespace v8

#endif  // RegexpShim_h
