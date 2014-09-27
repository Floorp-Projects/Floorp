/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(JS_ARM_SIMULATOR)
#include "jit/arm/Assembler-arm.h"
#include "jit/arm/MoveEmitter-arm.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/IonLinker.h"
#include "jit/IonMacroAssembler.h"
#include "jit/MoveResolver.h"

#include "jsapi-tests/tests.h"

#include "vm/Runtime.h"

static const int LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 4*1024;

static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s0(0, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s1(1, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s2(2, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s3(3, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s4(4, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s5(5, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s6(6, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s7(7, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s8(8, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s9(9, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s10(10, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s11(11, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s12(12, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s13(13, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s14(14, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s15(15, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s16(16, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s17(17, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s18(18, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s19(19, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s20(20, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s21(21, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s22(22, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s23(23, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s24(24, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s25(25, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s26(26, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s27(27, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s28(28, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s29(29, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s30(30, js::jit::VFPRegister::Single);
static MOZ_CONSTEXPR_VAR js::jit::FloatRegister s31(31, js::jit::VFPRegister::Single);

static js::jit::JitCode *
linkAndAllocate(JSContext *cx, js::jit::MacroAssembler *masm)
{
    using namespace js;
    using namespace js::jit;
    AutoFlushICache afc("test");
    Linker l(*masm);
    return l.newCode<CanGC>(cx, JSC::ION_CODE);
}

BEGIN_TEST(testJitMoveEmitterCycles_simple)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    IonContext ic(cx, &alloc);
    rt->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator *sim = Simulator::Current();
    mr.addMove(MoveOperand(d0), MoveOperand(d2), MoveOp::DOUBLE);
    sim->set_d_register_from_double(0, 2);
    mr.addMove(MoveOperand(d3), MoveOperand(d1), MoveOp::DOUBLE);
    sim->set_d_register_from_double(3, 1);
    mr.addMove(MoveOperand(s4), MoveOperand(s0), MoveOp::FLOAT32);
    sim->set_s_register_from_float(4, 0);
    mr.addMove(MoveOperand(s5), MoveOperand(s6), MoveOp::FLOAT32);
    sim->set_s_register_from_float(5, 6);
    mr.addMove(MoveOperand(s2), MoveOperand(s1), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 1);
    mr.addMove(MoveOperand(s3), MoveOperand(s7), MoveOp::FLOAT32);
    sim->set_s_register_from_float(3, 7);
    // don't explode!
    mr.resolve();
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode *code = linkAndAllocate(cx, &masm);
    sim->call(code->raw(), 1, 1);
    CHECK(sim->get_double_from_d_register(2) == 2);
    CHECK(int(sim->get_double_from_d_register(1)) == 1);
    CHECK(int(sim->get_float_from_s_register(0)) == 0);
    CHECK(int(sim->get_float_from_s_register(6)) == 6);
    CHECK(int(sim->get_float_from_s_register(1)) == 1);
    CHECK(int(sim->get_float_from_s_register(7)) == 7);
    return true;
}
END_TEST(testJitMoveEmitterCycles_simple)
BEGIN_TEST(testJitMoveEmitterCycles_autogen)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    IonContext ic(cx, &alloc);
    rt->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator *sim = Simulator::Current();
    mr.addMove(MoveOperand(d9), MoveOperand(d14), MoveOp::DOUBLE);
    sim->set_d_register_from_double(9, 9);
    mr.addMove(MoveOperand(s24), MoveOperand(s25), MoveOp::FLOAT32);
    sim->set_s_register_from_float(24, 24);
    mr.addMove(MoveOperand(d3), MoveOperand(d0), MoveOp::DOUBLE);
    sim->set_d_register_from_double(3, 3);
    mr.addMove(MoveOperand(s10), MoveOperand(s31), MoveOp::FLOAT32);
    sim->set_s_register_from_float(10, 10);
    mr.addMove(MoveOperand(d1), MoveOperand(d10), MoveOp::DOUBLE);
    sim->set_d_register_from_double(1, 1);
    mr.addMove(MoveOperand(s8), MoveOperand(s10), MoveOp::FLOAT32);
    sim->set_s_register_from_float(8, 8);
    mr.addMove(MoveOperand(d2), MoveOperand(d7), MoveOp::DOUBLE);
    sim->set_d_register_from_double(2, 2);
    mr.addMove(MoveOperand(s20), MoveOperand(s18), MoveOp::FLOAT32);
    sim->set_s_register_from_float(20, 20);
    mr.addMove(MoveOperand(s1), MoveOperand(s3), MoveOp::FLOAT32);
    sim->set_s_register_from_float(1, 1);
    mr.addMove(MoveOperand(s17), MoveOperand(s11), MoveOp::FLOAT32);
    sim->set_s_register_from_float(17, 17);
    mr.addMove(MoveOperand(s22), MoveOperand(s30), MoveOp::FLOAT32);
    sim->set_s_register_from_float(22, 22);
    mr.addMove(MoveOperand(s31), MoveOperand(s7), MoveOp::FLOAT32);
    sim->set_s_register_from_float(31, 31);
    mr.addMove(MoveOperand(d3), MoveOperand(d13), MoveOp::DOUBLE);
    sim->set_d_register_from_double(3, 3);
    mr.addMove(MoveOperand(d9), MoveOperand(d8), MoveOp::DOUBLE);
    sim->set_d_register_from_double(9, 9);
    mr.addMove(MoveOperand(s31), MoveOperand(s23), MoveOp::FLOAT32);
    sim->set_s_register_from_float(31, 31);
    mr.addMove(MoveOperand(s13), MoveOperand(s8), MoveOp::FLOAT32);
    sim->set_s_register_from_float(13, 13);
    mr.addMove(MoveOperand(s28), MoveOperand(s5), MoveOp::FLOAT32);
    sim->set_s_register_from_float(28, 28);
    mr.addMove(MoveOperand(s31), MoveOperand(s19), MoveOp::FLOAT32);
    sim->set_s_register_from_float(31, 31);
    mr.addMove(MoveOperand(s20), MoveOperand(s6), MoveOp::FLOAT32);
    sim->set_s_register_from_float(20, 20);
    mr.addMove(MoveOperand(s0), MoveOperand(s2), MoveOp::FLOAT32);
    sim->set_s_register_from_float(0, 0);
    mr.addMove(MoveOperand(d7), MoveOperand(d6), MoveOp::DOUBLE);
    sim->set_d_register_from_double(7, 7);
    mr.addMove(MoveOperand(s13), MoveOperand(s9), MoveOp::FLOAT32);
    sim->set_s_register_from_float(13, 13);
    mr.addMove(MoveOperand(s1), MoveOperand(s4), MoveOp::FLOAT32);
    sim->set_s_register_from_float(1, 1);
    mr.addMove(MoveOperand(s29), MoveOperand(s22), MoveOp::FLOAT32);
    sim->set_s_register_from_float(29, 29);
    mr.addMove(MoveOperand(s25), MoveOperand(s24), MoveOp::FLOAT32);
    sim->set_s_register_from_float(25, 25);
    // don't explode!
    mr.resolve();
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode *code = linkAndAllocate(cx, &masm);
    sim->skipCalleeSavedRegsCheck = true;
    sim->call(code->raw(), 1, 1);
    CHECK(int(sim->get_double_from_d_register(14)) == 9);
    CHECK(int(sim->get_float_from_s_register(25)) == 24);
    CHECK(int(sim->get_double_from_d_register(0)) == 3);
    CHECK(int(sim->get_float_from_s_register(31)) == 10);
    CHECK(int(sim->get_double_from_d_register(10)) == 1);
    CHECK(int(sim->get_float_from_s_register(10)) == 8);
    CHECK(int(sim->get_double_from_d_register(7)) == 2);
    CHECK(int(sim->get_float_from_s_register(18)) == 20);
    CHECK(int(sim->get_float_from_s_register(3)) == 1);
    CHECK(int(sim->get_float_from_s_register(11)) == 17);
    CHECK(int(sim->get_float_from_s_register(30)) == 22);
    CHECK(int(sim->get_float_from_s_register(7)) == 31);
    CHECK(int(sim->get_double_from_d_register(13)) == 3);
    CHECK(int(sim->get_double_from_d_register(8)) == 9);
    CHECK(int(sim->get_float_from_s_register(23)) == 31);
    CHECK(int(sim->get_float_from_s_register(8)) == 13);
    CHECK(int(sim->get_float_from_s_register(5)) == 28);
    CHECK(int(sim->get_float_from_s_register(19)) == 31);
    CHECK(int(sim->get_float_from_s_register(6)) == 20);
    CHECK(int(sim->get_float_from_s_register(2)) == 0);
    CHECK(int(sim->get_double_from_d_register(6)) == 7);
    CHECK(int(sim->get_float_from_s_register(9)) == 13);
    CHECK(int(sim->get_float_from_s_register(4)) == 1);
    CHECK(int(sim->get_float_from_s_register(22)) == 29);
    CHECK(int(sim->get_float_from_s_register(24)) == 25);
    return true;
}
END_TEST(testJitMoveEmitterCycles_autogen)

BEGIN_TEST(testJitMoveEmitterCycles_autogen2)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    IonContext ic(cx, &alloc);
    rt->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator *sim = Simulator::Current();
    mr.addMove(MoveOperand(d10), MoveOperand(d0), MoveOp::DOUBLE);
    sim->set_d_register_from_double(10, 10);
    mr.addMove(MoveOperand(s15), MoveOperand(s3), MoveOp::FLOAT32);
    sim->set_s_register_from_float(15, 15);
    mr.addMove(MoveOperand(s2), MoveOperand(s28), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 2);
    mr.addMove(MoveOperand(s30), MoveOperand(s25), MoveOp::FLOAT32);
    sim->set_s_register_from_float(30, 30);
    mr.addMove(MoveOperand(s16), MoveOperand(s2), MoveOp::FLOAT32);
    sim->set_s_register_from_float(16, 16);
    mr.addMove(MoveOperand(s2), MoveOperand(s29), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 2);
    mr.addMove(MoveOperand(s17), MoveOperand(s10), MoveOp::FLOAT32);
    sim->set_s_register_from_float(17, 17);
    mr.addMove(MoveOperand(s2), MoveOperand(s19), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 2);
    mr.addMove(MoveOperand(s9), MoveOperand(s26), MoveOp::FLOAT32);
    sim->set_s_register_from_float(9, 9);
    mr.addMove(MoveOperand(s1), MoveOperand(s23), MoveOp::FLOAT32);
    sim->set_s_register_from_float(1, 1);
    mr.addMove(MoveOperand(s8), MoveOperand(s6), MoveOp::FLOAT32);
    sim->set_s_register_from_float(8, 8);
    mr.addMove(MoveOperand(s24), MoveOperand(s16), MoveOp::FLOAT32);
    sim->set_s_register_from_float(24, 24);
    mr.addMove(MoveOperand(s19), MoveOperand(s4), MoveOp::FLOAT32);
    sim->set_s_register_from_float(19, 19);
    mr.addMove(MoveOperand(d5), MoveOperand(d6), MoveOp::DOUBLE);
    sim->set_d_register_from_double(5, 5);
    mr.addMove(MoveOperand(s18), MoveOperand(s15), MoveOp::FLOAT32);
    sim->set_s_register_from_float(18, 18);
    mr.addMove(MoveOperand(s23), MoveOperand(s30), MoveOp::FLOAT32);
    sim->set_s_register_from_float(23, 23);
    mr.addMove(MoveOperand(s27), MoveOperand(s17), MoveOp::FLOAT32);
    sim->set_s_register_from_float(27, 27);
    mr.addMove(MoveOperand(d3), MoveOperand(d4), MoveOp::DOUBLE);
    sim->set_d_register_from_double(3, 3);
    mr.addMove(MoveOperand(s14), MoveOperand(s27), MoveOp::FLOAT32);
    sim->set_s_register_from_float(14, 14);
    mr.addMove(MoveOperand(s2), MoveOperand(s31), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 2);
    mr.addMove(MoveOperand(s2), MoveOperand(s24), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 2);
    mr.addMove(MoveOperand(s31), MoveOperand(s11), MoveOp::FLOAT32);
    sim->set_s_register_from_float(31, 31);
    mr.addMove(MoveOperand(s0), MoveOperand(s18), MoveOp::FLOAT32);
    sim->set_s_register_from_float(0, 0);
    mr.addMove(MoveOperand(s24), MoveOperand(s7), MoveOp::FLOAT32);
    sim->set_s_register_from_float(24, 24);
    mr.addMove(MoveOperand(s0), MoveOperand(s21), MoveOp::FLOAT32);
    sim->set_s_register_from_float(0, 0);
    mr.addMove(MoveOperand(s27), MoveOperand(s20), MoveOp::FLOAT32);
    sim->set_s_register_from_float(27, 27);
    mr.addMove(MoveOperand(s14), MoveOperand(s5), MoveOp::FLOAT32);
    sim->set_s_register_from_float(14, 14);
    mr.addMove(MoveOperand(s2), MoveOperand(s14), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 2);
    mr.addMove(MoveOperand(s12), MoveOperand(s22), MoveOp::FLOAT32);
    sim->set_s_register_from_float(12, 12);
    // don't explode!
    mr.resolve();
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode *code = linkAndAllocate(cx, &masm);
    sim->skipCalleeSavedRegsCheck = true;
    sim->call(code->raw(), 1, 1);
    CHECK(int(sim->get_double_from_d_register(0)) == 10);
    CHECK(int(sim->get_float_from_s_register(3)) == 15);
    CHECK(int(sim->get_float_from_s_register(28)) == 2);
    CHECK(int(sim->get_float_from_s_register(25)) == 30);
    CHECK(int(sim->get_float_from_s_register(2)) == 16);
    CHECK(int(sim->get_float_from_s_register(29)) == 2);
    CHECK(int(sim->get_float_from_s_register(10)) == 17);
    CHECK(int(sim->get_float_from_s_register(19)) == 2);
    CHECK(int(sim->get_float_from_s_register(26)) == 9);
    CHECK(int(sim->get_float_from_s_register(23)) == 1);
    CHECK(int(sim->get_float_from_s_register(6)) == 8);
    CHECK(int(sim->get_float_from_s_register(16)) == 24);
    CHECK(int(sim->get_float_from_s_register(4)) == 19);
    CHECK(int(sim->get_double_from_d_register(6)) == 5);
    CHECK(int(sim->get_float_from_s_register(15)) == 18);
    CHECK(int(sim->get_float_from_s_register(30)) == 23);
    CHECK(int(sim->get_float_from_s_register(17)) == 27);
    CHECK(int(sim->get_double_from_d_register(4)) == 3);
    CHECK(int(sim->get_float_from_s_register(27)) == 14);
    CHECK(int(sim->get_float_from_s_register(31)) == 2);
    CHECK(int(sim->get_float_from_s_register(24)) == 2);
    CHECK(int(sim->get_float_from_s_register(11)) == 31);
    CHECK(int(sim->get_float_from_s_register(18)) == 0);
    CHECK(int(sim->get_float_from_s_register(7)) == 24);
    CHECK(int(sim->get_float_from_s_register(21)) == 0);
    CHECK(int(sim->get_float_from_s_register(20)) == 27);
    CHECK(int(sim->get_float_from_s_register(5)) == 14);
    CHECK(int(sim->get_float_from_s_register(14)) == 2);
    CHECK(int(sim->get_float_from_s_register(22)) == 12);
    return true;
}
END_TEST(testJitMoveEmitterCycles_autogen2)


BEGIN_TEST(testJitMoveEmitterCycles_autogen3)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    IonContext ic(cx, &alloc);
    rt->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator *sim = Simulator::Current();
    mr.addMove(MoveOperand(s0), MoveOperand(s21), MoveOp::FLOAT32);
    sim->set_s_register_from_float(0, 0);
    mr.addMove(MoveOperand(s2), MoveOperand(s26), MoveOp::FLOAT32);
    sim->set_s_register_from_float(2, 2);
    mr.addMove(MoveOperand(s19), MoveOperand(s20), MoveOp::FLOAT32);
    sim->set_s_register_from_float(19, 19);
    mr.addMove(MoveOperand(s4), MoveOperand(s24), MoveOp::FLOAT32);
    sim->set_s_register_from_float(4, 4);
    mr.addMove(MoveOperand(s22), MoveOperand(s9), MoveOp::FLOAT32);
    sim->set_s_register_from_float(22, 22);
    mr.addMove(MoveOperand(s5), MoveOperand(s28), MoveOp::FLOAT32);
    sim->set_s_register_from_float(5, 5);
    mr.addMove(MoveOperand(s15), MoveOperand(s7), MoveOp::FLOAT32);
    sim->set_s_register_from_float(15, 15);
    mr.addMove(MoveOperand(s26), MoveOperand(s14), MoveOp::FLOAT32);
    sim->set_s_register_from_float(26, 26);
    mr.addMove(MoveOperand(s13), MoveOperand(s30), MoveOp::FLOAT32);
    sim->set_s_register_from_float(13, 13);
    mr.addMove(MoveOperand(s26), MoveOperand(s22), MoveOp::FLOAT32);
    sim->set_s_register_from_float(26, 26);
    mr.addMove(MoveOperand(s21), MoveOperand(s6), MoveOp::FLOAT32);
    sim->set_s_register_from_float(21, 21);
    mr.addMove(MoveOperand(s23), MoveOperand(s31), MoveOp::FLOAT32);
    sim->set_s_register_from_float(23, 23);
    mr.addMove(MoveOperand(s7), MoveOperand(s12), MoveOp::FLOAT32);
    sim->set_s_register_from_float(7, 7);
    mr.addMove(MoveOperand(s14), MoveOperand(s10), MoveOp::FLOAT32);
    sim->set_s_register_from_float(14, 14);
    mr.addMove(MoveOperand(d12), MoveOperand(d8), MoveOp::DOUBLE);
    sim->set_d_register_from_double(12, 12);
    mr.addMove(MoveOperand(s5), MoveOperand(s1), MoveOp::FLOAT32);
    sim->set_s_register_from_float(5, 5);
    mr.addMove(MoveOperand(d12), MoveOperand(d2), MoveOp::DOUBLE);
    sim->set_d_register_from_double(12, 12);
    mr.addMove(MoveOperand(s3), MoveOperand(s8), MoveOp::FLOAT32);
    sim->set_s_register_from_float(3, 3);
    mr.addMove(MoveOperand(s14), MoveOperand(s0), MoveOp::FLOAT32);
    sim->set_s_register_from_float(14, 14);
    mr.addMove(MoveOperand(s28), MoveOperand(s29), MoveOp::FLOAT32);
    sim->set_s_register_from_float(28, 28);
    mr.addMove(MoveOperand(d12), MoveOperand(d9), MoveOp::DOUBLE);
    sim->set_d_register_from_double(12, 12);
    mr.addMove(MoveOperand(s29), MoveOperand(s2), MoveOp::FLOAT32);
    sim->set_s_register_from_float(29, 29);
    mr.addMove(MoveOperand(s22), MoveOperand(s27), MoveOp::FLOAT32);
    sim->set_s_register_from_float(22, 22);
    mr.addMove(MoveOperand(s19), MoveOperand(s3), MoveOp::FLOAT32);
    sim->set_s_register_from_float(19, 19);
    mr.addMove(MoveOperand(s21), MoveOperand(s11), MoveOp::FLOAT32);
    sim->set_s_register_from_float(21, 21);
    mr.addMove(MoveOperand(s22), MoveOperand(s13), MoveOp::FLOAT32);
    sim->set_s_register_from_float(22, 22);
    mr.addMove(MoveOperand(s29), MoveOperand(s25), MoveOp::FLOAT32);
    sim->set_s_register_from_float(29, 29);
    mr.addMove(MoveOperand(s29), MoveOperand(s15), MoveOp::FLOAT32);
    sim->set_s_register_from_float(29, 29);
    mr.addMove(MoveOperand(s16), MoveOperand(s23), MoveOp::FLOAT32);
    sim->set_s_register_from_float(16, 16);
    // don't explode!
    mr.resolve();
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode *code = linkAndAllocate(cx, &masm);
    sim->skipCalleeSavedRegsCheck = true;
    sim->call(code->raw(), 1, 1);
    CHECK(int(sim->get_float_from_s_register(21)) == 0);
    CHECK(int(sim->get_float_from_s_register(26)) == 2);
    CHECK(int(sim->get_float_from_s_register(20)) == 19);
    CHECK(int(sim->get_float_from_s_register(24)) == 4);
    CHECK(int(sim->get_float_from_s_register(9)) == 22);
    CHECK(int(sim->get_float_from_s_register(28)) == 5);
    CHECK(int(sim->get_float_from_s_register(7)) == 15);
    CHECK(int(sim->get_float_from_s_register(14)) == 26);
    CHECK(int(sim->get_float_from_s_register(30)) == 13);
    CHECK(int(sim->get_float_from_s_register(22)) == 26);
    CHECK(int(sim->get_float_from_s_register(6)) == 21);
    CHECK(int(sim->get_float_from_s_register(31)) == 23);
    CHECK(int(sim->get_float_from_s_register(12)) == 7);
    CHECK(int(sim->get_float_from_s_register(10)) == 14);
    CHECK(int(sim->get_double_from_d_register(8)) == 12);
    CHECK(int(sim->get_float_from_s_register(1)) == 5);
    CHECK(int(sim->get_double_from_d_register(2)) == 12);
    CHECK(int(sim->get_float_from_s_register(8)) == 3);
    CHECK(int(sim->get_float_from_s_register(0)) == 14);
    CHECK(int(sim->get_float_from_s_register(29)) == 28);
    CHECK(int(sim->get_double_from_d_register(9)) == 12);
    CHECK(int(sim->get_float_from_s_register(2)) == 29);
    CHECK(int(sim->get_float_from_s_register(27)) == 22);
    CHECK(int(sim->get_float_from_s_register(3)) == 19);
    CHECK(int(sim->get_float_from_s_register(11)) == 21);
    CHECK(int(sim->get_float_from_s_register(13)) == 22);
    CHECK(int(sim->get_float_from_s_register(25)) == 29);
    CHECK(int(sim->get_float_from_s_register(15)) == 29);
    CHECK(int(sim->get_float_from_s_register(23)) == 16);
    return true;
}
END_TEST(testJitMoveEmitterCycles_autogen3)

#endif
