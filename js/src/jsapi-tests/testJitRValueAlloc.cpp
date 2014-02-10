/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Snapshots.h"

#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::jit;

// These tests are checking that all slots of the current architecture can all
// be encoded and decoded correctly.  We iterate on all registers and on many
// fake stack locations (Fibonacci).
static RValueAllocation
Read(const RValueAllocation &slot)
{
    CompactBufferWriter writer;
    slot.write(writer);

    CompactBufferReader reader(writer);
    return RValueAllocation::read(reader);
}

BEGIN_TEST(testJitRValueAlloc_Double)
{
    RValueAllocation s;
    for (uint32_t i = 0; i < FloatRegisters::Total; i++) {
        s = RValueAllocation::Double(FloatRegister::FromCode(i));
        CHECK(s == Read(s));
    }
    return true;
}
END_TEST(testJitRValueAlloc_Double)

BEGIN_TEST(testJitRValueAlloc_FloatReg)
{
    RValueAllocation s;
    for (uint32_t i = 0; i < FloatRegisters::Total; i++) {
        s = RValueAllocation::Float32(FloatRegister::FromCode(i));
        CHECK(s == Read(s));
    }
    return true;
}
END_TEST(testJitRValueAlloc_FloatReg)

BEGIN_TEST(testJitRValueAlloc_FloatStack)
{
    RValueAllocation s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = RValueAllocation::Float32(i);
        CHECK(s == Read(s));
    }
    return true;
}
END_TEST(testJitRValueAlloc_FloatStack)

BEGIN_TEST(testJitRValueAlloc_TypedReg)
{
    RValueAllocation s;
    for (uint32_t i = 0; i < Registers::Total; i++) {
#define FOR_EACH_JSVAL(_)                       \
    /* _(JSVAL_TYPE_DOUBLE) */                  \
    _(JSVAL_TYPE_INT32)                         \
    /* _(JSVAL_TYPE_UNDEFINED) */               \
    _(JSVAL_TYPE_BOOLEAN)                       \
    /* _(JSVAL_TYPE_MAGIC) */                   \
    _(JSVAL_TYPE_STRING)                        \
    /* _(JSVAL_TYPE_NULL) */                    \
    _(JSVAL_TYPE_OBJECT)

#define CHECK_WITH_JSVAL(jsval)                                         \
        s = RValueAllocation::Typed(jsval, Register::FromCode(i));      \
        CHECK(s == Read(s));

        FOR_EACH_JSVAL(CHECK_WITH_JSVAL)
#undef CHECK_WITH_JSVAL
#undef FOR_EACH_JSVAL
    }
    return true;
}
END_TEST(testJitRValueAlloc_TypedReg)

BEGIN_TEST(testJitRValueAlloc_TypedStack)
{
    RValueAllocation s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
#define FOR_EACH_JSVAL(_)                       \
    _(JSVAL_TYPE_DOUBLE)                        \
    _(JSVAL_TYPE_INT32)                         \
    /* _(JSVAL_TYPE_UNDEFINED) */               \
    _(JSVAL_TYPE_BOOLEAN)                       \
    /* _(JSVAL_TYPE_MAGIC) */                   \
    _(JSVAL_TYPE_STRING)                        \
    /* _(JSVAL_TYPE_NULL) */                    \
    _(JSVAL_TYPE_OBJECT)

#define CHECK_WITH_JSVAL(jsval)                      \
        s = RValueAllocation::Typed(jsval, i);       \
        CHECK(s == Read(s));

        FOR_EACH_JSVAL(CHECK_WITH_JSVAL)
#undef CHECK_WITH_JSVAL
#undef FOR_EACH_JSVAL
    }
    return true;
}
END_TEST(testJitRValueAlloc_TypedStack)

#if defined(JS_NUNBOX32)

BEGIN_TEST(testJitRValueAlloc_UntypedRegReg)
{
    RValueAllocation s;
    for (uint32_t i = 0; i < Registers::Total; i++) {
        for (uint32_t j = 0; j < Registers::Total; j++) {
            if (i == j)
                continue;
            s = RValueAllocation::Untyped(Register::FromCode(i), Register::FromCode(j));
            MOZ_ASSERT(s == Read(s));
            CHECK(s == Read(s));
        }
    }
    return true;
}
END_TEST(testJitRValueAlloc_UntypedRegReg)

BEGIN_TEST(testJitRValueAlloc_UntypedRegStack)
{
    RValueAllocation s;
    for (uint32_t i = 0; i < Registers::Total; i++) {
        int32_t j, last = 0, tmp;
        for (j = 0; j > 0; tmp = j, j += last, last = tmp) {
            s = RValueAllocation::Untyped(Register::FromCode(i), j);
            CHECK(s == Read(s));
        }
    }
    return true;
}
END_TEST(testJitRValueAlloc_UntypedRegStack)

BEGIN_TEST(testJitRValueAlloc_UntypedStackReg)
{
    RValueAllocation s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        for (uint32_t j = 0; j < Registers::Total; j++) {
            s = RValueAllocation::Untyped(i, Register::FromCode(j));
            CHECK(s == Read(s));
        }
    }
    return true;
}
END_TEST(testJitRValueAlloc_UntypedStackReg)

BEGIN_TEST(testJitRValueAlloc_UntypedStackStack)
{
    RValueAllocation s;
    int32_t i, li = 0, ti;
    for (i = 0; i > 0; ti = i, i += li, li = ti) {
        int32_t j, lj = 0, tj;
        for (j = 0; j > 0; tj = j, j += lj, lj = tj) {
            s = RValueAllocation::Untyped(i, j);
            CHECK(s == Read(s));
        }
    }
    return true;
}
END_TEST(testJitRValueAlloc_UntypedStackStack)

#else

BEGIN_TEST(testJitRValueAlloc_UntypedReg)
{
    RValueAllocation s;
    for (uint32_t i = 0; i < Registers::Total; i++) {
        s = RValueAllocation::Untyped(Register::FromCode(i));
        CHECK(s == Read(s));
    }
    return true;
}
END_TEST(testJitRValueAlloc_UntypedReg)

BEGIN_TEST(testJitRValueAlloc_UntypedStack)
{
    RValueAllocation s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = RValueAllocation::Untyped(i);
        CHECK(s == Read(s));
    }
    return true;
}
END_TEST(testJitRValueAlloc_UntypedStack)

#endif

BEGIN_TEST(testJitRValueAlloc_UndefinedAndNull)
{
    RValueAllocation s;
    s = RValueAllocation::Undefined();
    CHECK(s == Read(s));
    s = RValueAllocation::Null();
    CHECK(s == Read(s));
    return true;
}
END_TEST(testJitRValueAlloc_UndefinedAndNull)

BEGIN_TEST(testJitRValueAlloc_Int32)
{
    RValueAllocation s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = RValueAllocation::Int32(i);
        CHECK(s == Read(s));
    }
    return true;
}
END_TEST(testJitRValueAlloc_Int32)

BEGIN_TEST(testJitRValueAlloc_ConstantPool)
{
    RValueAllocation s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = RValueAllocation::ConstantPool(i);
        CHECK(s == Read(s));
    }
    return true;
}
END_TEST(testJitRValueAlloc_ConstantPool)
