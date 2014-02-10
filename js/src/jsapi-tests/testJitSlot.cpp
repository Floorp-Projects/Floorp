/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CompactBuffer.h"
#include "jit/Slot.h"

#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::jit;

// These tests are checking that all slots of the current architecture can all
// be encoded and decoded correctly.  We iterate on all registers and on many
// fake stack locations (Fibonacci).
static Slot
ReadSlot(const Slot &slot)
{
    CompactBufferWriter writer;
    slot.write(writer);

    CompactBufferReader reader(writer);
    return Slot::read(reader);
}

BEGIN_TEST(testJitSlot_Double)
{
    Slot s;
    for (uint32_t i = 0; i < FloatRegisters::Total; i++) {
        s = Slot::DoubleSlot(FloatRegister::FromCode(i));
        CHECK(s == ReadSlot(s));
    }
    return true;
}
END_TEST(testJitSlot_Double)

BEGIN_TEST(testJitSlot_FloatReg)
{
    Slot s;
    for (uint32_t i = 0; i < FloatRegisters::Total; i++) {
        s = Slot::Float32Slot(FloatRegister::FromCode(i));
        CHECK(s == ReadSlot(s));
    }
    return true;
}
END_TEST(testJitSlot_FloatReg)

BEGIN_TEST(testJitSlot_FloatStack)
{
    Slot s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = Slot::Float32Slot(i);
        CHECK(s == ReadSlot(s));
    }
    return true;
}
END_TEST(testJitSlot_FloatStack)

BEGIN_TEST(testJitSlot_TypedReg)
{
    Slot s;
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

#define CHECK_WITH_JSVAL(jsval)                                  \
        s = Slot::TypedSlot(jsval, Register::FromCode(i));       \
        CHECK(s == ReadSlot(s));

        FOR_EACH_JSVAL(CHECK_WITH_JSVAL)
#undef CHECK_WITH_JSVAL
#undef FOR_EACH_JSVAL
    }
    return true;
}
END_TEST(testJitSlot_TypedReg)

BEGIN_TEST(testJitSlot_TypedStack)
{
    Slot s;
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

#define CHECK_WITH_JSVAL(jsval)              \
        s = Slot::TypedSlot(jsval, i);       \
        CHECK(s == ReadSlot(s));

        FOR_EACH_JSVAL(CHECK_WITH_JSVAL)
#undef CHECK_WITH_JSVAL
#undef FOR_EACH_JSVAL
    }
    return true;
}
END_TEST(testJitSlot_TypedStack)

#if defined(JS_NUNBOX32)

BEGIN_TEST(testJitSlot_UntypedRegReg)
{
    Slot s;
    for (uint32_t i = 0; i < Registers::Total; i++) {
        for (uint32_t j = 0; j < Registers::Total; j++) {
            if (i == j)
                continue;
            s = Slot::UntypedSlot(Register::FromCode(i), Register::FromCode(j));
            MOZ_ASSERT(s == ReadSlot(s));
            CHECK(s == ReadSlot(s));
        }
    }
    return true;
}
END_TEST(testJitSlot_UntypedRegReg)

BEGIN_TEST(testJitSlot_UntypedRegStack)
{
    Slot s;
    for (uint32_t i = 0; i < Registers::Total; i++) {
        int32_t j, last = 0, tmp;
        for (j = 0; j > 0; tmp = j, j += last, last = tmp) {
            s = Slot::UntypedSlot(Register::FromCode(i), j);
            CHECK(s == ReadSlot(s));
        }
    }
    return true;
}
END_TEST(testJitSlot_UntypedRegStack)

BEGIN_TEST(testJitSlot_UntypedStackReg)
{
    Slot s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        for (uint32_t j = 0; j < Registers::Total; j++) {
            s = Slot::UntypedSlot(i, Register::FromCode(j));
            CHECK(s == ReadSlot(s));
        }
    }
    return true;
}
END_TEST(testJitSlot_UntypedStackReg)

BEGIN_TEST(testJitSlot_UntypedStackStack)
{
    Slot s;
    int32_t i, li = 0, ti;
    for (i = 0; i > 0; ti = i, i += li, li = ti) {
        int32_t j, lj = 0, tj;
        for (j = 0; j > 0; tj = j, j += lj, lj = tj) {
            s = Slot::UntypedSlot(i, j);
            CHECK(s == ReadSlot(s));
        }
    }
    return true;
}
END_TEST(testJitSlot_UntypedStackStack)

#else

BEGIN_TEST(testJitSlot_UntypedReg)
{
    Slot s;
    for (uint32_t i = 0; i < Registers::Total; i++) {
        s = Slot::UntypedSlot(Register::FromCode(i));
        CHECK(s == ReadSlot(s));
    }
    return true;
}
END_TEST(testJitSlot_UntypedReg)

BEGIN_TEST(testJitSlot_UntypedStack)
{
    Slot s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = Slot::UntypedSlot(i);
        CHECK(s == ReadSlot(s));
    }
    return true;
}
END_TEST(testJitSlot_UntypedStack)

#endif

BEGIN_TEST(testJitSlot_UndefinedAndNull)
{
    Slot s;
    s = Slot::UndefinedSlot();
    CHECK(s == ReadSlot(s));
    s = Slot::NullSlot();
    CHECK(s == ReadSlot(s));
    return true;
}
END_TEST(testJitSlot_UndefinedAndNull)

BEGIN_TEST(testJitSlot_Int32)
{
    Slot s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = Slot::Int32Slot(i);
        CHECK(s == ReadSlot(s));
    }
    return true;
}
END_TEST(testJitSlot_Int32)

BEGIN_TEST(testJitSlot_ConstantPool)
{
    Slot s;
    int32_t i, last = 0, tmp;
    for (i = 0; i > 0; tmp = i, i += last, last = tmp) {
        s = Slot::ConstantPoolSlot(i);
        CHECK(s == ReadSlot(s));
    }
    return true;
}
END_TEST(testJitSlot_ConstantPool)
