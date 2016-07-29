/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonAnalysis.h"
#include "jit/Linker.h"
#include "jit/MacroAssembler.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/ValueNumbering.h"
#include "js/Value.h"

#include "jsapi-tests/tests.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::PositiveInfinity;
using mozilla::NegativeInfinity;

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)

typedef void (*EnterTest)();

static bool Prepare(MacroAssembler& masm)
{
    AllocatableRegisterSet regs(RegisterSet::Volatile());
    LiveRegisterSet save(regs.asLiveSet());
    masm.PushRegsInMask(save);
    return true;
}

static bool Execute(JSContext* cx, MacroAssembler& masm)
{
    AllocatableRegisterSet regs(RegisterSet::Volatile());
    LiveRegisterSet save(regs.asLiveSet());
    masm.PopRegsInMask(save);
    masm.ret(); // Add return statement to be sure.

    if (masm.oom())
        return false;

    Linker linker(masm);
    JitCode* code = linker.newCode<CanGC>(cx, OTHER_CODE);
    if (!code)
        return false;
    if (!ExecutableAllocator::makeExecutable(code->raw(), code->bufferSize()))
        return false;

    EnterTest test = code->as<EnterTest>();
    test();
    return true;
}

BEGIN_TEST(testJitMacroAssembler_truncateDoubleToInt64)
{
    MacroAssembler masm(cx);

    if (!Prepare(masm))
        return false;

    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    AllocatableFloatRegisterSet allFloatRegs(FloatRegisterSet::All());
    FloatRegister input = allFloatRegs.takeAny();
#ifdef JS_NUNBOX32
    Register64 output(allRegs.takeAny(), allRegs.takeAny());
#else
    Register64 output(allRegs.takeAny());
#endif
    Register temp = allRegs.takeAny();

    masm.reserveStack(sizeof(int32_t));

#define TEST(INPUT, OUTPUT)                                                     \
    {                                                                           \
        Label next;                                                             \
        masm.loadConstantDouble(double(INPUT), input);                          \
        masm.storeDouble(input, Operand(esp, 0));                               \
        masm.truncateDoubleToInt64(Address(esp, 0), Address(esp, 0), temp);     \
        masm.branch64(Assembler::Equal, Address(esp, 0), Imm64(OUTPUT), &next); \
        masm.printf("truncateDoubleToInt64("#INPUT") failed\n");                \
        masm.breakpoint();                                                      \
        masm.bind(&next);                                                       \
    }

    TEST(0, 0);
    TEST(-0, 0);
    TEST(1, 1);
    TEST(9223372036854774784.0, 9223372036854774784);
    TEST(-9223372036854775808.0, 0x8000000000000000);
    TEST(9223372036854775808.0, 0x8000000000000000);
    TEST(JS::GenericNaN(), 0x8000000000000000);
    TEST(PositiveInfinity<double>(), 0x8000000000000000);
    TEST(NegativeInfinity<double>(), 0x8000000000000000);
#undef TEST

    masm.freeStack(sizeof(int32_t));

    return Execute(cx, masm);
}
END_TEST(testJitMacroAssembler_truncateDoubleToInt64)

BEGIN_TEST(testJitMacroAssembler_truncateDoubleToUInt64)
{
    MacroAssembler masm(cx);

    if (!Prepare(masm))
        return false;

    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    AllocatableFloatRegisterSet allFloatRegs(FloatRegisterSet::All());
    FloatRegister input = allFloatRegs.takeAny();
    FloatRegister floatTemp = allFloatRegs.takeAny();
#ifdef JS_NUNBOX32
    Register64 output(allRegs.takeAny(), allRegs.takeAny());
#else
    Register64 output(allRegs.takeAny());
#endif
    Register temp = allRegs.takeAny();

    masm.reserveStack(sizeof(int32_t));

#define TEST(INPUT, OUTPUT)                                                                \
    {                                                                                      \
        Label next;                                                                        \
        masm.loadConstantDouble(double(INPUT), input);                                     \
        masm.storeDouble(input, Operand(esp, 0));                                          \
        masm.truncateDoubleToUInt64(Address(esp, 0), Address(esp, 0), temp, floatTemp);    \
        masm.branch64(Assembler::Equal, Address(esp, 0), Imm64(OUTPUT), &next);            \
        masm.printf("truncateDoubleToUInt64("#INPUT") failed\n");                          \
        masm.breakpoint();                                                                 \
        masm.bind(&next);                                                                  \
    }

    TEST(0, 0);
    TEST(1, 1);
    TEST(9223372036854774784.0, 9223372036854774784);
    TEST((uint64_t)0x8000000000000000, 0x8000000000000000);
    TEST((uint64_t)0x8000000000000001, 0x8000000000000000);
    TEST((uint64_t)0x8006004000000001, 0x8006004000000000);
    TEST(-0.0, 0);
    TEST(-0.5, 0);
    TEST(-0.99, 0);
    TEST(JS::GenericNaN(), 0x8000000000000000);
    TEST(PositiveInfinity<double>(), 0x8000000000000000);
    TEST(NegativeInfinity<double>(), 0x8000000000000000);
#undef TEST

    masm.freeStack(sizeof(int32_t));

    return Execute(cx, masm);
}
END_TEST(testJitMacroAssembler_truncateDoubleToUInt64)

BEGIN_TEST(testJitMacroAssembler_branchDoubleNotInInt64Range)
{
    MacroAssembler masm(cx);

    if (!Prepare(masm))
        return false;

    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    AllocatableFloatRegisterSet allFloatRegs(FloatRegisterSet::All());
    FloatRegister input = allFloatRegs.takeAny();
#ifdef JS_NUNBOX32
    Register64 output(allRegs.takeAny(), allRegs.takeAny());
#else
    Register64 output(allRegs.takeAny());
#endif
    Register temp = allRegs.takeAny();

    masm.reserveStack(sizeof(int32_t));

#define TEST(INPUT, OUTPUT)                                                                 \
    {                                                                                       \
        Label next;                                                                         \
        masm.loadConstantDouble(double(INPUT), input);                                      \
        masm.storeDouble(input, Operand(esp, 0));                                           \
        if (OUTPUT) {                                                                       \
            masm.branchDoubleNotInInt64Range(Address(esp, 0), temp, &next);                 \
        } else {                                                                            \
            Label fail;                                                                     \
            masm.branchDoubleNotInInt64Range(Address(esp, 0), temp, &fail);                 \
            masm.jump(&next);                                                               \
            masm.bind(&fail);                                                               \
        }                                                                                   \
        masm.printf("branchDoubleNotInInt64Range("#INPUT") failed\n");                      \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }

    TEST(0, false);
    TEST(-0, false);
    TEST(1, false);
    TEST(9223372036854774784.0, false);
    TEST(-9223372036854775808.0, true);
    TEST(9223372036854775808.0, true);
    TEST(JS::GenericNaN(), true);
    TEST(PositiveInfinity<double>(), true);
    TEST(NegativeInfinity<double>(), true);
#undef TEST

    masm.freeStack(sizeof(int32_t));

    return Execute(cx, masm);
}
END_TEST(testJitMacroAssembler_branchDoubleNotInInt64Range)

BEGIN_TEST(testJitMacroAssembler_branchDoubleNotInUInt64Range)
{
    MacroAssembler masm(cx);

    if (!Prepare(masm))
        return false;

    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    AllocatableFloatRegisterSet allFloatRegs(FloatRegisterSet::All());
    FloatRegister input = allFloatRegs.takeAny();
#ifdef JS_NUNBOX32
    Register64 output(allRegs.takeAny(), allRegs.takeAny());
#else
    Register64 output(allRegs.takeAny());
#endif
    Register temp = allRegs.takeAny();

    masm.reserveStack(sizeof(int32_t));

#define TEST(INPUT, OUTPUT)                                                                 \
    {                                                                                       \
        Label next;                                                                         \
        masm.loadConstantDouble(double(INPUT), input);                                      \
        masm.storeDouble(input, Operand(esp, 0));                                           \
        if (OUTPUT) {                                                                       \
            masm.branchDoubleNotInUInt64Range(Address(esp, 0), temp, &next);                \
        } else {                                                                            \
            Label fail;                                                                     \
            masm.branchDoubleNotInUInt64Range(Address(esp, 0), temp, &fail);                \
            masm.jump(&next);                                                               \
            masm.bind(&fail);                                                               \
        }                                                                                   \
        masm.printf("branchDoubleNotInUInt64Range("#INPUT") failed\n");                     \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }

    TEST(0, false);
    TEST(1, false);
    TEST(9223372036854774784.0, false);
    TEST((uint64_t)0x8000000000000000, false);
    TEST((uint64_t)0x8000000000000001, false);
    TEST((uint64_t)0x8006004000000001, false);
    TEST(-0.0, true);
    TEST(-0.5, true);
    TEST(-0.99, true);
    TEST(JS::GenericNaN(), true);
    TEST(PositiveInfinity<double>(), true);
    TEST(NegativeInfinity<double>(), true);
#undef TEST

    masm.freeStack(sizeof(int32_t));

    return Execute(cx, masm);
}
END_TEST(testJitMacroAssembler_branchDoubleNotInUInt64Range)

BEGIN_TEST(testJitMacroAssembler_lshift64)
{
    MacroAssembler masm(cx);

    if (!Prepare(masm))
        return false;

    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    AllocatableFloatRegisterSet allFloatRegs(FloatRegisterSet::All());
#if defined(JS_CODEGEN_X86)
    Register shift = ecx;
    allRegs.take(shift);
#elif defined(JS_CODEGEN_X64)
    Register shift = rcx;
    allRegs.take(shift);
#else
    Register shift = allRegs.takeAny();
#endif

#ifdef JS_NUNBOX32
    Register64 input(allRegs.takeAny(), allRegs.takeAny());
#else
    Register64 input(allRegs.takeAny());
#endif

    masm.reserveStack(sizeof(int32_t));

#define TEST(SHIFT, INPUT, OUTPUT)                                                          \
    {                                                                                       \
        Label next;                                                                         \
        masm.move64(Imm64(INPUT), input);                                                   \
        masm.move32(Imm32(SHIFT), shift);                                                   \
        masm.lshift64(shift, input);                                                        \
        masm.branch64(Assembler::Equal, input, Imm64(OUTPUT), &next);                       \
        masm.printf("lshift64("#SHIFT", "#INPUT") failed\n");                               \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }                                                                                       \
    {                                                                                       \
        Label next;                                                                         \
        masm.move64(Imm64(INPUT), input);                                                   \
        masm.lshift64(Imm32(SHIFT & 0x3f), input);                                          \
        masm.branch64(Assembler::Equal, input, Imm64(OUTPUT), &next);                       \
        masm.printf("lshift64(Imm32("#SHIFT"&0x3f), "#INPUT") failed\n");                   \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }

    TEST(0, 1, 1);
    TEST(1, 1, 2);
    TEST(2, 1, 4);
    TEST(32, 1, 0x0000000100000000);
    TEST(33, 1, 0x0000000200000000);
    TEST(0, -1, 0xffffffffffffffff);
    TEST(1, -1, 0xfffffffffffffffe);
    TEST(2, -1, 0xfffffffffffffffc);
    TEST(32, -1, 0xffffffff00000000);
    TEST(0xffffffff, 1, 0x8000000000000000);
    TEST(0xfffffffe, 1, 0x4000000000000000);
    TEST(0xfffffffd, 1, 0x2000000000000000);
    TEST(0x80000001, 1, 2);
#undef TEST

    masm.freeStack(sizeof(int32_t));

    return Execute(cx, masm);
}
END_TEST(testJitMacroAssembler_lshift64)

BEGIN_TEST(testJitMacroAssembler_rshift64Arithmetic)
{
    MacroAssembler masm(cx);

    if (!Prepare(masm))
        return false;

    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    AllocatableFloatRegisterSet allFloatRegs(FloatRegisterSet::All());
#if defined(JS_CODEGEN_X86)
    Register shift = ecx;
    allRegs.take(shift);
#elif defined(JS_CODEGEN_X64)
    Register shift = rcx;
    allRegs.take(shift);
#else
    Register shift = allRegs.takeAny();
#endif

#ifdef JS_NUNBOX32
    Register64 input(allRegs.takeAny(), allRegs.takeAny());
#else
    Register64 input(allRegs.takeAny());
#endif

    masm.reserveStack(sizeof(int32_t));

#define TEST(SHIFT, INPUT, OUTPUT)                                                          \
    {                                                                                       \
        Label next;                                                                         \
        masm.move64(Imm64(INPUT), input);                                                   \
        masm.move32(Imm32(SHIFT), shift);                                                   \
        masm.rshift64Arithmetic(shift, input);                                              \
        masm.branch64(Assembler::Equal, input, Imm64(OUTPUT), &next);                       \
        masm.printf("rshift64Arithmetic("#SHIFT", "#INPUT") failed\n");                     \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }                                                                                       \
    {                                                                                       \
        Label next;                                                                         \
        masm.move64(Imm64(INPUT), input);                                                   \
        masm.rshift64Arithmetic(Imm32(SHIFT & 0x3f), input);                                \
        masm.branch64(Assembler::Equal, input, Imm64(OUTPUT), &next);                       \
        masm.printf("rshift64Arithmetic(Imm32("#SHIFT"&0x3f), "#INPUT") failed\n");         \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }

    TEST(0, 0x4000000000000000, 0x4000000000000000);
    TEST(1, 0x4000000000000000, 0x2000000000000000);
    TEST(2, 0x4000000000000000, 0x1000000000000000);
    TEST(32, 0x4000000000000000, 0x0000000040000000);
    TEST(0, 0x8000000000000000, 0x8000000000000000);
    TEST(1, 0x8000000000000000, 0xc000000000000000);
    TEST(2, 0x8000000000000000, 0xe000000000000000);
    TEST(32, 0x8000000000000000, 0xffffffff80000000);
    TEST(0xffffffff, 0x8000000000000000, 0xffffffffffffffff);
    TEST(0xfffffffe, 0x8000000000000000, 0xfffffffffffffffe);
    TEST(0xfffffffd, 0x8000000000000000, 0xfffffffffffffffc);
    TEST(0x80000001, 0x8000000000000000, 0xc000000000000000);
#undef TEST

    masm.freeStack(sizeof(int32_t));

    return Execute(cx, masm);
}
END_TEST(testJitMacroAssembler_rshift64Arithmetic)

BEGIN_TEST(testJitMacroAssembler_rshift64)
{
    MacroAssembler masm(cx);

    if (!Prepare(masm))
        return false;

    AllocatableGeneralRegisterSet allRegs(GeneralRegisterSet::All());
    AllocatableFloatRegisterSet allFloatRegs(FloatRegisterSet::All());
#if defined(JS_CODEGEN_X86)
    Register shift = ecx;
    allRegs.take(shift);
#elif defined(JS_CODEGEN_X64)
    Register shift = rcx;
    allRegs.take(shift);
#else
    Register shift = allRegs.takeAny();
#endif

#ifdef JS_NUNBOX32
    Register64 input(allRegs.takeAny(), allRegs.takeAny());
#else
    Register64 input(allRegs.takeAny());
#endif

    masm.reserveStack(sizeof(int32_t));

#define TEST(SHIFT, INPUT, OUTPUT)                                                          \
    {                                                                                       \
        Label next;                                                                         \
        masm.move64(Imm64(INPUT), input);                                                   \
        masm.move32(Imm32(SHIFT), shift);                                                   \
        masm.rshift64(shift, input);                                                        \
        masm.branch64(Assembler::Equal, input, Imm64(OUTPUT), &next);                       \
        masm.printf("rshift64("#SHIFT", "#INPUT") failed\n");                               \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }                                                                                       \
    {                                                                                       \
        Label next;                                                                         \
        masm.move64(Imm64(INPUT), input);                                                   \
        masm.rshift64(Imm32(SHIFT & 0x3f), input);                                          \
        masm.branch64(Assembler::Equal, input, Imm64(OUTPUT), &next);                       \
        masm.printf("rshift64(Imm32("#SHIFT"&0x3f), "#INPUT") failed\n");                   \
        masm.breakpoint();                                                                  \
        masm.bind(&next);                                                                   \
    }

    TEST(0, 0x4000000000000000, 0x4000000000000000);
    TEST(1, 0x4000000000000000, 0x2000000000000000);
    TEST(2, 0x4000000000000000, 0x1000000000000000);
    TEST(32, 0x4000000000000000, 0x0000000040000000);
    TEST(0, 0x8000000000000000, 0x8000000000000000);
    TEST(1, 0x8000000000000000, 0x4000000000000000);
    TEST(2, 0x8000000000000000, 0x2000000000000000);
    TEST(32, 0x8000000000000000, 0x0000000080000000);
    TEST(0xffffffff, 0x8000000000000000, 0x0000000000000001);
    TEST(0xfffffffe, 0x8000000000000000, 0x0000000000000002);
    TEST(0xfffffffd, 0x8000000000000000, 0x0000000000000004);
    TEST(0x80000001, 0x8000000000000000, 0x4000000000000000);
#undef TEST

    masm.freeStack(sizeof(int32_t));

    return Execute(cx, masm);
}
END_TEST(testJitMacroAssembler_rshift64)

#endif
