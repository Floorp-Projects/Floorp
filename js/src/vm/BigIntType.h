/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BigIntType_h
#define vm_BigIntType_h

#include "mozilla/Range.h"
#include "mozilla/RangedPtr.h"

#include <gmp.h>

#include "gc/Barrier.h"
#include "gc/GC.h"
#include "gc/Heap.h"
#include "js/AllocPolicy.h"
#include "js/GCHashTable.h"
#include "js/Result.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "vm/StringType.h"
#include "vm/Xdr.h"

namespace js {

template <typename CharT>
static bool StringToBigIntImpl(const mozilla::Range<const CharT>& chars,
                               uint8_t radix, Handle<JS::BigInt*> res);

template<XDRMode mode>
XDRResult XDRBigInt(XDRState<mode>* xdr, MutableHandleBigInt bi);

} // namespace js

namespace JS {

class BigInt final : public js::gc::TenuredCell
{
    // StringToBigIntImpl modifies the num_ field of the res argument.
    template <typename CharT>
    friend bool js::StringToBigIntImpl(const mozilla::Range<const CharT>& chars,
                                       uint8_t radix, Handle<BigInt*> res);
    template <js::XDRMode mode>
    friend js::XDRResult js::XDRBigInt(js::XDRState<mode>* xdr, MutableHandleBigInt bi);

  protected:
    // Reserved word for Cell GC invariants. This also ensures minimum
    // structure size.
    uintptr_t reserved_;

  private:
    mpz_t num_;

  protected:
    BigInt() : reserved_(0) { }

  public:
    // Allocate and initialize a BigInt value
    static BigInt* create(JSContext* cx);

    static BigInt* createFromDouble(JSContext* cx, double d);

    static BigInt* createFromBoolean(JSContext* cx, bool b);

    // Read a BigInt value from a little-endian byte array.
    static BigInt* createFromBytes(JSContext* cx, int sign, void* bytes, size_t nbytes);

    static BigInt* createFromInt64(JSContext* cx, int64_t n);
    static BigInt* createFromUint64(JSContext* cx, uint64_t n);

    static const JS::TraceKind TraceKind = JS::TraceKind::BigInt;

    void traceChildren(JSTracer* trc);

    void finalize(js::FreeOp* fop);

    js::HashNumber hash();

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    bool toBoolean();
    int8_t sign();

    static void init();

    static BigInt* copy(JSContext* cx, Handle<BigInt*> x);
    static BigInt* add(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* sub(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* mul(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* div(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* mod(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* pow(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* neg(JSContext* cx, Handle<BigInt*> x);
    static BigInt* lsh(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* rsh(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* bitAnd(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* bitXor(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* bitOr(JSContext* cx, Handle<BigInt*> x, Handle<BigInt*> y);
    static BigInt* bitNot(JSContext* cx, Handle<BigInt*> x);

    static int64_t toInt64(BigInt* x);
    static uint64_t toUint64(BigInt* x);

    static BigInt* asIntN(JSContext* cx, HandleBigInt x, uint64_t bits);
    static BigInt* asUintN(JSContext* cx, HandleBigInt x, uint64_t bits);

    // Type-checking versions of arithmetic operations. These methods
    // must be called with at least one BigInt operand. Binary
    // operations with throw a TypeError if one of the operands is not a
    // BigInt value.
    static bool add(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool sub(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool mul(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool div(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool mod(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool pow(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool neg(JSContext* cx, Handle<Value> operand, MutableHandle<Value> res);
    static bool lsh(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool rsh(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool bitAnd(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool bitXor(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool bitOr(JSContext* cx, Handle<Value> lhs, Handle<Value> rhs, MutableHandle<Value> res);
    static bool bitNot(JSContext* cx, Handle<Value> operand, MutableHandle<Value> res);

    static double numberValue(BigInt* x);
    static JSLinearString* toString(JSContext* cx, BigInt* x, uint8_t radix);

    static bool equal(BigInt* lhs, BigInt* rhs);
    static bool equal(BigInt* lhs, double rhs);
    static JS::Result<bool> looselyEqual(JSContext* cx, HandleBigInt lhs, HandleValue rhs);

    static bool lessThan(BigInt* x, BigInt* y);

    // These methods return Nothing when the non-BigInt operand is NaN
    // or a string that can't be interpreted as a BigInt.
    static mozilla::Maybe<bool> lessThan(BigInt* lhs, double rhs);
    static mozilla::Maybe<bool> lessThan(double lhs, BigInt* rhs);
    static bool lessThan(JSContext* cx, HandleBigInt lhs, HandleString rhs, mozilla::Maybe<bool>& res);
    static bool lessThan(JSContext* cx, HandleString lhs, HandleBigInt rhs, mozilla::Maybe<bool>& res);
    static bool lessThan(JSContext* cx, HandleValue lhs, HandleValue rhs, mozilla::Maybe<bool>& res);

    // Return the length in bytes of the representation used by
    // writeBytes.
    static size_t byteLength(BigInt* x);

    // Write a little-endian representation of a BigInt's absolute value
    // to a byte array.
    static void writeBytes(BigInt* x, mozilla::RangedPtr<uint8_t> buffer);
};

static_assert(sizeof(BigInt) >= js::gc::MinCellSize,
              "sizeof(BigInt) must be greater than the minimum allocation size");

} // namespace JS

namespace js {

extern JSAtom*
BigIntToAtom(JSContext* cx, JS::BigInt* bi);

extern JS::BigInt*
NumberToBigInt(JSContext* cx, double d);

// Convert a string to a BigInt, returning nullptr if parsing fails.
extern JS::Result<JS::BigInt*, JS::OOM&>
StringToBigInt(JSContext* cx, JS::Handle<JSString*> str, uint8_t radix);

// Same.
extern JS::BigInt*
StringToBigInt(JSContext* cx, const mozilla::Range<const char16_t>& chars);

extern JS::BigInt*
ToBigInt(JSContext* cx, JS::Handle<JS::Value> v);

} // namespace js

#endif
