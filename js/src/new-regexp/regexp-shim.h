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
#include "new-regexp/util/vector.h"
#include "new-regexp/util/zone.h"
#include "vm/NativeObject.h"

// Forward declaration of classes
namespace v8 {
namespace internal {

class Isolate;

}  // namespace internal
}  // namespace v8

#define V8_WARN_UNUSED_RESULT MOZ_MUST_USE
#define V8_EXPORT_PRIVATE MOZ_EXPORT
#define V8_FALLTHROUGH MOZ_FALLTHROUGH

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

}  // namespace internal
}  // namespace v8

#endif  // RegexpShim_h
