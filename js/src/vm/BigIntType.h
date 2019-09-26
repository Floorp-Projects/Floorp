/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BigIntType_h
#define vm_BigIntType_h

#include "mozilla/Range.h"
#include "mozilla/Span.h"

#include "gc/Barrier.h"
#include "gc/GC.h"
#include "gc/Heap.h"
#include "js/AllocPolicy.h"
#include "js/GCHashTable.h"
#include "js/Result.h"
#include "js/RootingAPI.h"
#include "js/TraceKind.h"
#include "js/TypeDecls.h"
#include "vm/StringType.h"
#include "vm/Xdr.h"

namespace JS {

class BigInt;

}  // namespace JS

namespace js {

template <XDRMode mode>
XDRResult XDRBigInt(XDRState<mode>* xdr, MutableHandle<JS::BigInt*> bi);

}  // namespace js

namespace JS {

class BigInt final
    : public js::gc::CellWithLengthAndFlags<js::gc::TenuredCell> {
  using Base = js::gc::CellWithLengthAndFlags<js::gc::TenuredCell>;

 public:
  using Digit = uintptr_t;

 private:
  // The low NumFlagBitsReservedForGC flag bits are reserved.
  static constexpr uintptr_t SignBit = JS_BIT(Base::NumFlagBitsReservedForGC);
  static constexpr size_t InlineDigitsLength =
      (js::gc::MinCellSize - sizeof(Base)) / sizeof(Digit);

  // Note: 32-bit length and flags fields are inherited from
  // CellWithLengthAndFlags.

  // The digit storage starts with the least significant digit (little-endian
  // digit order).  Byte order within a digit is of course native endian.
  union {
    Digit* heapDigits_;
    Digit inlineDigits_[InlineDigitsLength];
  };

 public:
  static const JS::TraceKind TraceKind = JS::TraceKind::BigInt;

  size_t digitLength() const { return lengthField(); }

  // Offset for direct access from JIT code.
  static constexpr size_t offsetOfDigitLength() {
    return Base::offsetOfLength();
  }

  bool hasInlineDigits() const { return digitLength() <= InlineDigitsLength; }
  bool hasHeapDigits() const { return !hasInlineDigits(); }

  using Digits = mozilla::Span<Digit>;
  Digits digits() {
    return Digits(hasInlineDigits() ? inlineDigits_ : heapDigits_,
                  digitLength());
  }
  Digit digit(size_t idx) { return digits()[idx]; }
  void setDigit(size_t idx, Digit digit) { digits()[idx] = digit; }

  bool isZero() const { return digitLength() == 0; }
  bool isNegative() const { return flagsField() & SignBit; }

  void initializeDigitsToZero();

  void traceChildren(JSTracer* trc);
  void finalize(JSFreeOp* fop);
  js::HashNumber hash();
  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  static BigInt* createUninitialized(JSContext* cx, size_t digitLength,
                                     bool isNegative);
  static BigInt* createFromDouble(JSContext* cx, double d);
  static BigInt* createFromUint64(JSContext* cx, uint64_t n);
  static BigInt* createFromInt64(JSContext* cx, int64_t n);
  static BigInt* createFromDigit(JSContext* cx, Digit d, bool isNegative);
  // FIXME: Cache these values.
  static BigInt* zero(JSContext* cx);
  static BigInt* one(JSContext* cx);
  static BigInt* negativeOne(JSContext* cx);

  static BigInt* copy(JSContext* cx, Handle<BigInt*> x);
  static BigInt* add(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* sub(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* mul(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* div(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* mod(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* pow(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* neg(JSContext* cx, Handle<BigInt*> x);
  static BigInt* inc(JSContext* cx, Handle<BigInt*> x);
  static BigInt* dec(JSContext* cx, Handle<BigInt*> x);
  static BigInt* lsh(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* rsh(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitAnd(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitXor(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitOr(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
  static BigInt* bitNot(JSContext* cx, Handle<BigInt*> x);

  static int64_t toInt64(BigInt* x);
  static uint64_t toUint64(BigInt* x);

  // Return true if the BigInt is without loss of precision representable as an
  // int64 and store the int64 value in the output. Otherwise return false and
  // leave the value of the output parameter unspecified.
  static bool isInt64(BigInt* x, int64_t* result);

  static BigInt* asIntN(JSContext* cx, Handle<BigInt*> x, uint64_t bits);
  static BigInt* asUintN(JSContext* cx, Handle<BigInt*> x, uint64_t bits);

  // Type-checking versions of arithmetic operations. These methods
  // must be called with at least one BigInt operand. Binary
  // operations will throw a TypeError if one of the operands is not a
  // BigInt value.
  static bool add(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool sub(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool mul(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool div(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool mod(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool pow(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool neg(JSContext* cx, Handle<Value> operand,
                  MutableHandle<Value> res);
  static bool inc(JSContext* cx, Handle<Value> operand,
                  MutableHandle<Value> res);
  static bool dec(JSContext* cx, Handle<Value> operand,
                  MutableHandle<Value> res);
  static bool lsh(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool rsh(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                  MutableHandle<Value> res);
  static bool bitAnd(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                     MutableHandle<Value> res);
  static bool bitXor(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                     MutableHandle<Value> res);
  static bool bitOr(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs,
                    MutableHandle<Value> res);
  static bool bitNot(JSContext* cx, Handle<Value> operand,
                     MutableHandle<Value> res);

  static double numberValue(BigInt* x);

  template <js::AllowGC allowGC>
  static JSLinearString* toString(JSContext* cx, Handle<BigInt*> x,
                                  uint8_t radix);
  template <typename CharT>
  static BigInt* parseLiteral(JSContext* cx,
                              const mozilla::Range<const CharT> chars,
                              bool* haveParseError);
  template <typename CharT>
  static BigInt* parseLiteralDigits(JSContext* cx,
                                    const mozilla::Range<const CharT> chars,
                                    unsigned radix, bool isNegative,
                                    bool* haveParseError);

  static int8_t compare(BigInt* lhs, BigInt* rhs);
  static bool equal(BigInt* lhs, BigInt* rhs);
  static JS::Result<bool> looselyEqual(JSContext* cx, Handle<BigInt*> lhs,
                                       HandleValue rhs);

  static bool lessThan(BigInt* x, BigInt* y);
  // These methods return Nothing when the non-BigInt operand is NaN
  // or a string that can't be interpreted as a BigInt.
  static mozilla::Maybe<bool> lessThan(BigInt* lhs, double rhs);
  static mozilla::Maybe<bool> lessThan(double lhs, BigInt* rhs);
  static bool lessThan(JSContext* cx, Handle<BigInt*> lhs, HandleString rhs,
                       mozilla::Maybe<bool>& res);
  static bool lessThan(JSContext* cx, HandleString lhs, Handle<BigInt*> rhs,
                       mozilla::Maybe<bool>& res);
  static bool lessThan(JSContext* cx, HandleValue lhs, HandleValue rhs,
                       mozilla::Maybe<bool>& res);

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();  // Debugger-friendly stderr dump.
  void dump(js::GenericPrinter& out);
#endif

 private:
  static constexpr size_t DigitBits = sizeof(Digit) * CHAR_BIT;
  static constexpr size_t HalfDigitBits = DigitBits / 2;
  static constexpr Digit HalfDigitMask = (1ull << HalfDigitBits) - 1;

  static_assert(DigitBits == 32 || DigitBits == 64,
                "Unexpected BigInt Digit size");

  // Limit the size of bigint values to 1 million bits, to prevent excessive
  // memory usage.  This limit may be raised in the future if needed.  Note
  // however that there are many parts of the implementation that rely on being
  // able to count and index bits using a 32-bit signed ints, so until those
  // sites are fixed, the practical limit is 0x7fffffff bits.
  static constexpr size_t MaxBitLength = 1024 * 1024;
  static constexpr size_t MaxDigitLength = MaxBitLength / DigitBits;

  // BigInts can be serialized to strings of radix between 2 and 36.  For a
  // given bigint, radix 2 will take the most characters (one per bit).
  // Ensure that the max bigint size is small enough so that we can fit the
  // corresponding character count into a size_t, with space for a possible
  // sign prefix.
  static_assert(MaxBitLength <= std::numeric_limits<size_t>::max() - 1,
                "BigInt max length must be small enough to be serialized as a "
                "binary string");

  static size_t calculateMaximumCharactersRequired(HandleBigInt x,
                                                   unsigned radix);
  static MOZ_MUST_USE bool calculateMaximumDigitsRequired(JSContext* cx,
                                                          uint8_t radix,
                                                          size_t charCount,
                                                          size_t* result);

  static bool absoluteDivWithDigitDivisor(
      JSContext* cx, Handle<BigInt*> x, Digit divisor,
      const mozilla::Maybe<MutableHandle<BigInt*>>& quotient, Digit* remainder,
      bool quotientNegative);
  static void internalMultiplyAdd(BigInt* source, Digit factor, Digit summand,
                                  unsigned, BigInt* result);
  static void multiplyAccumulate(BigInt* multiplicand, Digit multiplier,
                                 BigInt* accumulator,
                                 unsigned accumulatorIndex);
  static bool absoluteDivWithBigIntDivisor(
      JSContext* cx, Handle<BigInt*> dividend, Handle<BigInt*> divisor,
      const mozilla::Maybe<MutableHandle<BigInt*>>& quotient,
      const mozilla::Maybe<MutableHandle<BigInt*>>& remainder,
      bool quotientNegative);

  enum class LeftShiftMode { SameSizeResult, AlwaysAddOneDigit };

  static BigInt* absoluteLeftShiftAlwaysCopy(JSContext* cx, Handle<BigInt*> x,
                                             unsigned shift, LeftShiftMode);
  static bool productGreaterThan(Digit factor1, Digit factor2, Digit high,
                                 Digit low);
  static BigInt* lshByAbsolute(JSContext* cx, HandleBigInt x, HandleBigInt y);
  static BigInt* rshByAbsolute(JSContext* cx, HandleBigInt x, HandleBigInt y);
  static BigInt* rshByMaximum(JSContext* cx, bool isNegative);
  static BigInt* truncateAndSubFromPowerOfTwo(JSContext* cx, HandleBigInt x,
                                              uint64_t bits,
                                              bool resultNegative);

  Digit absoluteInplaceAdd(BigInt* summand, unsigned startIndex);
  Digit absoluteInplaceSub(BigInt* subtrahend, unsigned startIndex);
  void inplaceRightShiftLowZeroBits(unsigned shift);
  void inplaceMultiplyAdd(Digit multiplier, Digit part);

  // The result of an SymmetricTrim bitwise op has as many digits as the
  // smaller operand.  A SymmetricFill bitwise op result has as many digits as
  // the larger operand, with high digits (if any) copied from the larger
  // operand.  AsymmetricFill is like SymmetricFill, except the result has as
  // many digits as the first operand; this kind is used for the and-not
  // operation.
  enum class BitwiseOpKind { SymmetricTrim, SymmetricFill, AsymmetricFill };

  template <BitwiseOpKind kind, typename BitwiseOp>
  static BigInt* absoluteBitwiseOp(JSContext* cx, Handle<BigInt*> x,
                                   Handle<BigInt*> y, BitwiseOp&& op);

  // Return `|x| & |y|`.
  static BigInt* absoluteAnd(JSContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y);

  // Return `|x| | |y|`.
  static BigInt* absoluteOr(JSContext* cx, Handle<BigInt*> x,
                            Handle<BigInt*> y);

  // Return `|x| & ~|y|`.
  static BigInt* absoluteAndNot(JSContext* cx, Handle<BigInt*> x,
                                Handle<BigInt*> y);

  // Return `|x| ^ |y|`.
  static BigInt* absoluteXor(JSContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y);

  // Return `(|x| + 1) * (resultNegative ? -1 : +1)`.
  static BigInt* absoluteAddOne(JSContext* cx, Handle<BigInt*> x,
                                bool resultNegative);

  // Return `(|x| - 1) * (resultNegative ? -1 : +1)`, with the precondition that
  // |x| != 0.
  static BigInt* absoluteSubOne(JSContext* cx, Handle<BigInt*> x,
                                bool resultNegative = false);

  // Return `a + b`, incrementing `*carry` if the addition overflows.
  static inline Digit digitAdd(Digit a, Digit b, Digit* carry) {
    Digit result = a + b;
    *carry += static_cast<Digit>(result < a);
    return result;
  }

  // Return `left - right`, incrementing `*borrow` if the addition overflows.
  static inline Digit digitSub(Digit left, Digit right, Digit* borrow) {
    Digit result = left - right;
    *borrow += static_cast<Digit>(result > left);
    return result;
  }

  // Compute `a * b`, returning the low half of the result and putting the
  // high half in `*high`.
  static Digit digitMul(Digit a, Digit b, Digit* high);

  // Divide `(high << DigitBits) + low` by `divisor`, returning the quotient
  // and storing the remainder in `*remainder`, with the precondition that
  // `high < divisor` so that the result fits in a Digit.
  static Digit digitDiv(Digit high, Digit low, Digit divisor, Digit* remainder);

  // Return `(|x| + |y|) * (resultNegative ? -1 : +1)`.
  static BigInt* absoluteAdd(JSContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y, bool resultNegative);

  // Return `(|x| - |y|) * (resultNegative ? -1 : +1)`, with the precondition
  // that |x| >= |y|.
  static BigInt* absoluteSub(JSContext* cx, Handle<BigInt*> x,
                             Handle<BigInt*> y, bool resultNegative);

  // If `|x| < |y|` return -1; if `|x| == |y|` return 0; otherwise return 1.
  static int8_t absoluteCompare(BigInt* lhs, BigInt* rhs);

  static int8_t compare(BigInt* lhs, double rhs);

  static bool equal(BigInt* lhs, double rhs);

  template <js::AllowGC allowGC>
  static JSLinearString* toStringBasePowerOfTwo(JSContext* cx, Handle<BigInt*>,
                                                unsigned radix);
  template <js::AllowGC allowGC>
  static JSLinearString* toStringSingleDigitBaseTen(JSContext* cx, Digit digit,
                                                    bool isNegative);
  static JSLinearString* toStringGeneric(JSContext* cx, Handle<BigInt*>,
                                         unsigned radix);

  static BigInt* trimHighZeroDigits(JSContext* cx, Handle<BigInt*> x);
  static BigInt* destructivelyTrimHighZeroDigits(JSContext* cx,
                                                 Handle<BigInt*> x);

  friend struct ::JSStructuredCloneReader;
  friend struct ::JSStructuredCloneWriter;
  template <js::XDRMode mode>
  friend js::XDRResult js::XDRBigInt(js::XDRState<mode>* xdr,
                                     MutableHandle<BigInt*> bi);

  BigInt() = delete;
  BigInt(const BigInt& other) = delete;
  void operator=(const BigInt& other) = delete;
};

static_assert(
    sizeof(BigInt) >= js::gc::MinCellSize,
    "sizeof(BigInt) must be greater than the minimum allocation size");

static_assert(
    sizeof(BigInt) == js::gc::MinCellSize,
    "sizeof(BigInt) intended to be the same as the minimum allocation size");

}  // namespace JS

namespace js {

template <AllowGC allowGC>
extern JSAtom* BigIntToAtom(JSContext* cx, JS::HandleBigInt bi);

extern JS::BigInt* NumberToBigInt(JSContext* cx, double d);

// Parse a BigInt from a string, using the method specified for StringToBigInt.
// Used by the BigInt constructor among other places.
extern JS::Result<JS::BigInt*, JS::OOM&> StringToBigInt(
    JSContext* cx, JS::Handle<JSString*> str);

// Parse a BigInt from an already-validated numeric literal.  Used by the
// parser.  Can only fail in out-of-memory situations.
extern JS::BigInt* ParseBigIntLiteral(
    JSContext* cx, const mozilla::Range<const char16_t>& chars);

extern JS::BigInt* ToBigInt(JSContext* cx, JS::Handle<JS::Value> v);
extern JS::Result<int64_t> ToBigInt64(JSContext* cx, JS::Handle<JS::Value> v);
extern JS::Result<uint64_t> ToBigUint64(JSContext* cx, JS::Handle<JS::Value> v);

}  // namespace js

#endif
