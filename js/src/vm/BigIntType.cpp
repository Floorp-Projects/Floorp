/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/BigIntType.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/HashFunctions.h"

#include <gmp.h>
#include <math.h>

#include "jsapi.h"

#include "builtin/BigInt.h"
#include "gc/Allocator.h"
#include "gc/Tracer.h"
#include "js/Initialization.h"
#include "js/Utility.h"
#include "vm/JSContext.h"
#include "vm/SelfHosting.h"

using namespace js;

// The following functions are wrappers for use with
// mp_set_memory_functions. GMP passes extra arguments to the realloc
// and free functions not needed by the JS allocation interface.
// js_malloc has the signature expected for GMP's malloc function, so no
// wrapper is required.

static void*
js_mp_realloc(void* ptr, size_t old_size, size_t new_size)
{
    return js_realloc(ptr, new_size);
}

static void
js_mp_free(void* ptr, size_t size)
{
    return js_free(ptr);
}

static bool memoryFunctionsInitialized = false;

JS_PUBLIC_API(void)
JS::SetGMPMemoryFunctions(JS::GMPAllocFn allocFn,
                          JS::GMPReallocFn reallocFn,
                          JS::GMPFreeFn freeFn)
{
    MOZ_ASSERT(JS::detail::libraryInitState == JS::detail::InitState::Uninitialized);
    memoryFunctionsInitialized = true;
    mp_set_memory_functions(allocFn, reallocFn, freeFn);
}

void
BigInt::init()
{
    // Don't override custom allocation functions if
    // JS::SetGMPMemoryFunctions was called.
    if (!memoryFunctionsInitialized) {
        memoryFunctionsInitialized = true;
        mp_set_memory_functions(js_malloc, js_mp_realloc, js_mp_free);
    }
}

BigInt*
BigInt::create(JSContext* cx)
{
    BigInt* x = Allocate<BigInt>(cx);
    if (!x)
        return nullptr;
    mpz_init(x->num_); // to zero
    return x;
}

BigInt*
BigInt::createFromDouble(JSContext* cx, double d)
{
    BigInt* x = Allocate<BigInt>(cx);
    if (!x)
        return nullptr;
    mpz_init_set_d(x->num_, d);
    return x;
}

BigInt*
BigInt::createFromBoolean(JSContext* cx, bool b)
{
    BigInt* x = Allocate<BigInt>(cx);
    if (!x)
        return nullptr;
    mpz_init_set_ui(x->num_, b);
    return x;
}

// BigInt proposal section 5.1.1
static bool
IsInteger(double d)
{
    // Step 1 is an assertion checked by the caller.
    // Step 2.
    if (!mozilla::IsFinite(d))
        return false;

    // Step 3.
    double i = JS::ToInteger(d);

    // Step 4.
    if (i != d)
        return false;

    // Step 5.
    return true;
}

// BigInt proposal section 5.1.2
BigInt*
js::NumberToBigInt(JSContext* cx, double d)
{
    // Step 1 is an assertion checked by the caller.
    // Step 2.
    if (!IsInteger(d)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_NUMBER_TO_BIGINT);
        return nullptr;
    }

    // Step 3.
    return BigInt::createFromDouble(cx, d);
}

BigInt*
BigInt::copy(JSContext* cx, HandleBigInt x)
{
    BigInt* bi = Allocate<BigInt>(cx);
    if (!bi)
        return nullptr;
    mpz_init_set(bi->num_, x->num_);
    return bi;
}

// BigInt proposal section 7.3
BigInt*
js::ToBigInt(JSContext* cx, HandleValue val)
{
    RootedValue v(cx, val);

    // Step 1.
    if (!ToPrimitive(cx, JSTYPE_NUMBER, &v))
        return nullptr;

    // Step 2.
    // String conversions are not yet supported.
    if (v.isBigInt())
        return v.toBigInt();

    if (v.isBoolean())
        return BigInt::createFromBoolean(cx, v.toBoolean());

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_BIGINT);
    return nullptr;
}

JSLinearString*
BigInt::toString(JSContext* cx, BigInt* x, uint8_t radix)
{
    MOZ_ASSERT(2 <= radix && radix <= 36);
    // We need two extra chars for '\0' and potentially '-'.
    size_t strSize = mpz_sizeinbase(x->num_, 10) + 2;
    UniqueChars str(static_cast<char*>(js_malloc(strSize)));
    if (!str) {
        ReportOutOfMemory(cx);
        return nullptr;
    }
    mpz_get_str(str.get(), radix, x->num_);

    return NewStringCopyZ<CanGC>(cx, str.get());
}

void
BigInt::finalize(js::FreeOp* fop)
{
    mpz_clear(num_);
}

JSAtom*
js::BigIntToAtom(JSContext* cx, BigInt* bi)
{
    JSString* str = BigInt::toString(cx, bi, 10);
    if (!str)
        return nullptr;
    return AtomizeString(cx, str);
}

bool
BigInt::toBoolean()
{
    return mpz_sgn(num_) != 0;
}

js::HashNumber
BigInt::hash()
{
    const mp_limb_t* limbs = mpz_limbs_read(num_);
    size_t limbCount = mpz_size(num_);
    uint32_t hash = mozilla::HashBytes(limbs, limbCount * sizeof(mp_limb_t));
    hash = mozilla::AddToHash(hash, mpz_sgn(num_));
    return hash;
}

size_t
BigInt::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    // Use the total number of limbs allocated when calculating the size
    // (_mp_alloc), not the number of limbs currently in use (_mp_size).
    // See the Info node `(gmp)Integer Internals` for details.
    mpz_srcptr n = static_cast<mpz_srcptr>(num_);
    return sizeof(*n) + sizeof(mp_limb_t) * n->_mp_alloc;
}

JS::ubi::Node::Size
JS::ubi::Concrete<BigInt>::size(mozilla::MallocSizeOf mallocSizeOf) const
{
    BigInt& bi = get();
    MOZ_ASSERT(bi.isTenured());
    size_t size = js::gc::Arena::thingSize(bi.asTenured().getAllocKind());
    size += bi.sizeOfExcludingThis(mallocSizeOf);
    return size;
}
