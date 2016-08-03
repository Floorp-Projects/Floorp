/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(JS_SIMULATOR_ARM)
#include "jit/arm/Assembler-arm.h"
#include "jit/arm/MoveEmitter-arm.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/Linker.h"
#include "jit/MacroAssembler.h"
#include "jit/MoveResolver.h"

#include "jsapi-tests/tests.h"

#include "vm/Runtime.h"

static const int LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 4*1024;

static constexpr js::jit::FloatRegister s0(0, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s1(1, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s2(2, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s3(3, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s4(4, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s5(5, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s6(6, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s7(7, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s8(8, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s9(9, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s10(10, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s11(11, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s12(12, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s13(13, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s14(14, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s15(15, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s16(16, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s17(17, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s18(18, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s19(19, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s20(20, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s21(21, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s22(22, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s23(23, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s24(24, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s25(25, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s26(26, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s27(27, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s28(28, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s29(29, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s30(30, js::jit::VFPRegister::Single);
static constexpr js::jit::FloatRegister s31(31, js::jit::VFPRegister::Single);

static js::jit::JitCode*
linkAndAllocate(JSContext* cx, js::jit::MacroAssembler* masm)
{
    using namespace js;
    using namespace js::jit;
    AutoFlushICache afc("test");
    Linker l(*masm);
    return l.newCode<CanGC>(cx, ION_CODE);
}

#define TRY(x) if (!(x)) return false;

BEGIN_TEST(testJitMoveEmitterCycles_simple)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    JitContext jc(cx, &alloc);
    cx->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator* sim = Simulator::Current();
    TRY(mr.addMove(MoveOperand(d0), MoveOperand(d2), MoveOp::DOUBLE));
    sim->set_d_register_from_double(0, 2);
    TRY(mr.addMove(MoveOperand(d3), MoveOperand(d1), MoveOp::DOUBLE));
    sim->set_d_register_from_double(3, 1);
    TRY(mr.addMove(MoveOperand(s4), MoveOperand(s0), MoveOp::FLOAT32));
    sim->set_s_register_from_float(4, 0);
    TRY(mr.addMove(MoveOperand(s5), MoveOperand(s6), MoveOp::FLOAT32));
    sim->set_s_register_from_float(5, 6);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s1), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 1);
    TRY(mr.addMove(MoveOperand(s3), MoveOperand(s7), MoveOp::FLOAT32));
    sim->set_s_register_from_float(3, 7);
    // don't explode!
    TRY(mr.resolve());
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode* code = linkAndAllocate(cx, &masm);
    sim->call(code->raw(), 1, 1);
    float f;
    double d;
    sim->get_double_from_d_register(2, &d);
    CHECK(d == 2);
    sim->get_double_from_d_register(1, &d);
    CHECK(int(d) == 1);
    sim->get_float_from_s_register(0, &f);
    CHECK(int(f) == 0);
    sim->get_float_from_s_register(6, &f);
    CHECK(int(f) == 6);
    sim->get_float_from_s_register(1, &f);
    CHECK(int(f) == 1);
    sim->get_float_from_s_register(7, &f);
    CHECK(int(f) == 7);
    return true;
}
END_TEST(testJitMoveEmitterCycles_simple)
BEGIN_TEST(testJitMoveEmitterCycles_autogen)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    JitContext jc(cx, &alloc);
    cx->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator* sim = Simulator::Current();
    TRY(mr.addMove(MoveOperand(d9), MoveOperand(d14), MoveOp::DOUBLE));
    sim->set_d_register_from_double(9, 9);
    TRY(mr.addMove(MoveOperand(s24), MoveOperand(s25), MoveOp::FLOAT32));
    sim->set_s_register_from_float(24, 24);
    TRY(mr.addMove(MoveOperand(d3), MoveOperand(d0), MoveOp::DOUBLE));
    sim->set_d_register_from_double(3, 3);
    TRY(mr.addMove(MoveOperand(s10), MoveOperand(s31), MoveOp::FLOAT32));
    sim->set_s_register_from_float(10, 10);
    TRY(mr.addMove(MoveOperand(d1), MoveOperand(d10), MoveOp::DOUBLE));
    sim->set_d_register_from_double(1, 1);
    TRY(mr.addMove(MoveOperand(s8), MoveOperand(s10), MoveOp::FLOAT32));
    sim->set_s_register_from_float(8, 8);
    TRY(mr.addMove(MoveOperand(d2), MoveOperand(d7), MoveOp::DOUBLE));
    sim->set_d_register_from_double(2, 2);
    TRY(mr.addMove(MoveOperand(s20), MoveOperand(s18), MoveOp::FLOAT32));
    sim->set_s_register_from_float(20, 20);
    TRY(mr.addMove(MoveOperand(s1), MoveOperand(s3), MoveOp::FLOAT32));
    sim->set_s_register_from_float(1, 1);
    TRY(mr.addMove(MoveOperand(s17), MoveOperand(s11), MoveOp::FLOAT32));
    sim->set_s_register_from_float(17, 17);
    TRY(mr.addMove(MoveOperand(s22), MoveOperand(s30), MoveOp::FLOAT32));
    sim->set_s_register_from_float(22, 22);
    TRY(mr.addMove(MoveOperand(s31), MoveOperand(s7), MoveOp::FLOAT32));
    sim->set_s_register_from_float(31, 31);
    TRY(mr.addMove(MoveOperand(d3), MoveOperand(d13), MoveOp::DOUBLE));
    sim->set_d_register_from_double(3, 3);
    TRY(mr.addMove(MoveOperand(d9), MoveOperand(d8), MoveOp::DOUBLE));
    sim->set_d_register_from_double(9, 9);
    TRY(mr.addMove(MoveOperand(s31), MoveOperand(s23), MoveOp::FLOAT32));
    sim->set_s_register_from_float(31, 31);
    TRY(mr.addMove(MoveOperand(s13), MoveOperand(s8), MoveOp::FLOAT32));
    sim->set_s_register_from_float(13, 13);
    TRY(mr.addMove(MoveOperand(s28), MoveOperand(s5), MoveOp::FLOAT32));
    sim->set_s_register_from_float(28, 28);
    TRY(mr.addMove(MoveOperand(s31), MoveOperand(s19), MoveOp::FLOAT32));
    sim->set_s_register_from_float(31, 31);
    TRY(mr.addMove(MoveOperand(s20), MoveOperand(s6), MoveOp::FLOAT32));
    sim->set_s_register_from_float(20, 20);
    TRY(mr.addMove(MoveOperand(s0), MoveOperand(s2), MoveOp::FLOAT32));
    sim->set_s_register_from_float(0, 0);
    TRY(mr.addMove(MoveOperand(d7), MoveOperand(d6), MoveOp::DOUBLE));
    sim->set_d_register_from_double(7, 7);
    TRY(mr.addMove(MoveOperand(s13), MoveOperand(s9), MoveOp::FLOAT32));
    sim->set_s_register_from_float(13, 13);
    TRY(mr.addMove(MoveOperand(s1), MoveOperand(s4), MoveOp::FLOAT32));
    sim->set_s_register_from_float(1, 1);
    TRY(mr.addMove(MoveOperand(s29), MoveOperand(s22), MoveOp::FLOAT32));
    sim->set_s_register_from_float(29, 29);
    TRY(mr.addMove(MoveOperand(s25), MoveOperand(s24), MoveOp::FLOAT32));
    sim->set_s_register_from_float(25, 25);
    // don't explode!
    TRY(mr.resolve());
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode* code = linkAndAllocate(cx, &masm);
    sim->skipCalleeSavedRegsCheck = true;
    sim->call(code->raw(), 1, 1);
    double d;
    float f;
    sim->get_double_from_d_register(14, &d);
    CHECK(int(d) == 9);
    sim->get_float_from_s_register(25, &f);
    CHECK(int(f) == 24);
    sim->get_double_from_d_register(0, &d);
    CHECK(int(d) == 3);
    sim->get_float_from_s_register(31, &f);
    CHECK(int(f) == 10);
    sim->get_double_from_d_register(10, &d);
    CHECK(int(d) == 1);
    sim->get_float_from_s_register(10, &f);
    CHECK(int(f) == 8);
    sim->get_double_from_d_register(7, &d);
    CHECK(int(d) == 2);
    sim->get_float_from_s_register(18, &f);
    CHECK(int(f) == 20);
    sim->get_float_from_s_register(3, &f);
    CHECK(int(f) == 1);
    sim->get_float_from_s_register(11, &f);
    CHECK(int(f) == 17);
    sim->get_float_from_s_register(30, &f);
    CHECK(int(f) == 22);
    sim->get_float_from_s_register(7, &f);
    CHECK(int(f) == 31);
    sim->get_double_from_d_register(13, &d);
    CHECK(int(d) == 3);
    sim->get_double_from_d_register(8, &d);
    CHECK(int(d) == 9);
    sim->get_float_from_s_register(23, &f);
    CHECK(int(f) == 31);
    sim->get_float_from_s_register(8, &f);
    CHECK(int(f) == 13);
    sim->get_float_from_s_register(5, &f);
    CHECK(int(f) == 28);
    sim->get_float_from_s_register(19, &f);
    CHECK(int(f) == 31);
    sim->get_float_from_s_register(6, &f);
    CHECK(int(f) == 20);
    sim->get_float_from_s_register(2, &f);
    CHECK(int(f) == 0);
    sim->get_double_from_d_register(6, &d);
    CHECK(int(d) == 7);
    sim->get_float_from_s_register(9, &f);
    CHECK(int(f) == 13);
    sim->get_float_from_s_register(4, &f);
    CHECK(int(f) == 1);
    sim->get_float_from_s_register(22, &f);
    CHECK(int(f) == 29);
    sim->get_float_from_s_register(24, &f);
    CHECK(int(f) == 25);
    return true;
}
END_TEST(testJitMoveEmitterCycles_autogen)

BEGIN_TEST(testJitMoveEmitterCycles_autogen2)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    JitContext jc(cx, &alloc);
    cx->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator* sim = Simulator::Current();
    TRY(mr.addMove(MoveOperand(d10), MoveOperand(d0), MoveOp::DOUBLE));
    sim->set_d_register_from_double(10, 10);
    TRY(mr.addMove(MoveOperand(s15), MoveOperand(s3), MoveOp::FLOAT32));
    sim->set_s_register_from_float(15, 15);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s28), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 2);
    TRY(mr.addMove(MoveOperand(s30), MoveOperand(s25), MoveOp::FLOAT32));
    sim->set_s_register_from_float(30, 30);
    TRY(mr.addMove(MoveOperand(s16), MoveOperand(s2), MoveOp::FLOAT32));
    sim->set_s_register_from_float(16, 16);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s29), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 2);
    TRY(mr.addMove(MoveOperand(s17), MoveOperand(s10), MoveOp::FLOAT32));
    sim->set_s_register_from_float(17, 17);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s19), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 2);
    TRY(mr.addMove(MoveOperand(s9), MoveOperand(s26), MoveOp::FLOAT32));
    sim->set_s_register_from_float(9, 9);
    TRY(mr.addMove(MoveOperand(s1), MoveOperand(s23), MoveOp::FLOAT32));
    sim->set_s_register_from_float(1, 1);
    TRY(mr.addMove(MoveOperand(s8), MoveOperand(s6), MoveOp::FLOAT32));
    sim->set_s_register_from_float(8, 8);
    TRY(mr.addMove(MoveOperand(s24), MoveOperand(s16), MoveOp::FLOAT32));
    sim->set_s_register_from_float(24, 24);
    TRY(mr.addMove(MoveOperand(s19), MoveOperand(s4), MoveOp::FLOAT32));
    sim->set_s_register_from_float(19, 19);
    TRY(mr.addMove(MoveOperand(d5), MoveOperand(d6), MoveOp::DOUBLE));
    sim->set_d_register_from_double(5, 5);
    TRY(mr.addMove(MoveOperand(s18), MoveOperand(s15), MoveOp::FLOAT32));
    sim->set_s_register_from_float(18, 18);
    TRY(mr.addMove(MoveOperand(s23), MoveOperand(s30), MoveOp::FLOAT32));
    sim->set_s_register_from_float(23, 23);
    TRY(mr.addMove(MoveOperand(s27), MoveOperand(s17), MoveOp::FLOAT32));
    sim->set_s_register_from_float(27, 27);
    TRY(mr.addMove(MoveOperand(d3), MoveOperand(d4), MoveOp::DOUBLE));
    sim->set_d_register_from_double(3, 3);
    TRY(mr.addMove(MoveOperand(s14), MoveOperand(s27), MoveOp::FLOAT32));
    sim->set_s_register_from_float(14, 14);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s31), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 2);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s24), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 2);
    TRY(mr.addMove(MoveOperand(s31), MoveOperand(s11), MoveOp::FLOAT32));
    sim->set_s_register_from_float(31, 31);
    TRY(mr.addMove(MoveOperand(s0), MoveOperand(s18), MoveOp::FLOAT32));
    sim->set_s_register_from_float(0, 0);
    TRY(mr.addMove(MoveOperand(s24), MoveOperand(s7), MoveOp::FLOAT32));
    sim->set_s_register_from_float(24, 24);
    TRY(mr.addMove(MoveOperand(s0), MoveOperand(s21), MoveOp::FLOAT32));
    sim->set_s_register_from_float(0, 0);
    TRY(mr.addMove(MoveOperand(s27), MoveOperand(s20), MoveOp::FLOAT32));
    sim->set_s_register_from_float(27, 27);
    TRY(mr.addMove(MoveOperand(s14), MoveOperand(s5), MoveOp::FLOAT32));
    sim->set_s_register_from_float(14, 14);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s14), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 2);
    TRY(mr.addMove(MoveOperand(s12), MoveOperand(s22), MoveOp::FLOAT32));
    sim->set_s_register_from_float(12, 12);
    // don't explode!
    TRY(mr.resolve());
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode* code = linkAndAllocate(cx, &masm);
    sim->skipCalleeSavedRegsCheck = true;
    sim->call(code->raw(), 1, 1);

    double d;
    float f;
    sim->get_double_from_d_register(0, &d);
    CHECK(int(d) == 10);
    sim->get_float_from_s_register(3, &f);
    CHECK(int(f) == 15);
    sim->get_float_from_s_register(28, &f);
    CHECK(int(f) == 2);
    sim->get_float_from_s_register(25, &f);
    CHECK(int(f) == 30);
    sim->get_float_from_s_register(2, &f);
    CHECK(int(f) == 16);
    sim->get_float_from_s_register(29, &f);
    CHECK(int(f) == 2);
    sim->get_float_from_s_register(10, &f);
    CHECK(int(f) == 17);
    sim->get_float_from_s_register(19, &f);
    CHECK(int(f) == 2);
    sim->get_float_from_s_register(26, &f);
    CHECK(int(f) == 9);
    sim->get_float_from_s_register(23, &f);
    CHECK(int(f) == 1);
    sim->get_float_from_s_register(6, &f);
    CHECK(int(f) == 8);
    sim->get_float_from_s_register(16, &f);
    CHECK(int(f) == 24);
    sim->get_float_from_s_register(4, &f);
    CHECK(int(f) == 19);
    sim->get_double_from_d_register(6, &d);
    CHECK(int(d) == 5);
    sim->get_float_from_s_register(15, &f);
    CHECK(int(f) == 18);
    sim->get_float_from_s_register(30, &f);
    CHECK(int(f) == 23);
    sim->get_float_from_s_register(17, &f);
    CHECK(int(f) == 27);
    sim->get_double_from_d_register(4, &d);
    CHECK(int(d) == 3);
    sim->get_float_from_s_register(27, &f);
    CHECK(int(f) == 14);
    sim->get_float_from_s_register(31, &f);
    CHECK(int(f) == 2);
    sim->get_float_from_s_register(24, &f);
    CHECK(int(f) == 2);
    sim->get_float_from_s_register(11, &f);
    CHECK(int(f) == 31);
    sim->get_float_from_s_register(18, &f);
    CHECK(int(f) == 0);
    sim->get_float_from_s_register(7, &f);
    CHECK(int(f) == 24);
    sim->get_float_from_s_register(21, &f);
    CHECK(int(f) == 0);
    sim->get_float_from_s_register(20, &f);
    CHECK(int(f) == 27);
    sim->get_float_from_s_register(5, &f);
    CHECK(int(f) == 14);
    sim->get_float_from_s_register(14, &f);
    CHECK(int(f) == 2);
    sim->get_float_from_s_register(22, &f);
    CHECK(int(f) == 12);
    return true;
}
END_TEST(testJitMoveEmitterCycles_autogen2)


BEGIN_TEST(testJitMoveEmitterCycles_autogen3)
{
    using namespace js;
    using namespace js::jit;
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    JitContext jc(cx, &alloc);
    cx->getJitRuntime(cx);
    MacroAssembler masm;
    MoveEmitter mover(masm);
    MoveResolver mr;
    mr.setAllocator(alloc);
    Simulator* sim = Simulator::Current();
    TRY(mr.addMove(MoveOperand(s0), MoveOperand(s21), MoveOp::FLOAT32));
    sim->set_s_register_from_float(0, 0);
    TRY(mr.addMove(MoveOperand(s2), MoveOperand(s26), MoveOp::FLOAT32));
    sim->set_s_register_from_float(2, 2);
    TRY(mr.addMove(MoveOperand(s19), MoveOperand(s20), MoveOp::FLOAT32));
    sim->set_s_register_from_float(19, 19);
    TRY(mr.addMove(MoveOperand(s4), MoveOperand(s24), MoveOp::FLOAT32));
    sim->set_s_register_from_float(4, 4);
    TRY(mr.addMove(MoveOperand(s22), MoveOperand(s9), MoveOp::FLOAT32));
    sim->set_s_register_from_float(22, 22);
    TRY(mr.addMove(MoveOperand(s5), MoveOperand(s28), MoveOp::FLOAT32));
    sim->set_s_register_from_float(5, 5);
    TRY(mr.addMove(MoveOperand(s15), MoveOperand(s7), MoveOp::FLOAT32));
    sim->set_s_register_from_float(15, 15);
    TRY(mr.addMove(MoveOperand(s26), MoveOperand(s14), MoveOp::FLOAT32));
    sim->set_s_register_from_float(26, 26);
    TRY(mr.addMove(MoveOperand(s13), MoveOperand(s30), MoveOp::FLOAT32));
    sim->set_s_register_from_float(13, 13);
    TRY(mr.addMove(MoveOperand(s26), MoveOperand(s22), MoveOp::FLOAT32));
    sim->set_s_register_from_float(26, 26);
    TRY(mr.addMove(MoveOperand(s21), MoveOperand(s6), MoveOp::FLOAT32));
    sim->set_s_register_from_float(21, 21);
    TRY(mr.addMove(MoveOperand(s23), MoveOperand(s31), MoveOp::FLOAT32));
    sim->set_s_register_from_float(23, 23);
    TRY(mr.addMove(MoveOperand(s7), MoveOperand(s12), MoveOp::FLOAT32));
    sim->set_s_register_from_float(7, 7);
    TRY(mr.addMove(MoveOperand(s14), MoveOperand(s10), MoveOp::FLOAT32));
    sim->set_s_register_from_float(14, 14);
    TRY(mr.addMove(MoveOperand(d12), MoveOperand(d8), MoveOp::DOUBLE));
    sim->set_d_register_from_double(12, 12);
    TRY(mr.addMove(MoveOperand(s5), MoveOperand(s1), MoveOp::FLOAT32));
    sim->set_s_register_from_float(5, 5);
    TRY(mr.addMove(MoveOperand(d12), MoveOperand(d2), MoveOp::DOUBLE));
    sim->set_d_register_from_double(12, 12);
    TRY(mr.addMove(MoveOperand(s3), MoveOperand(s8), MoveOp::FLOAT32));
    sim->set_s_register_from_float(3, 3);
    TRY(mr.addMove(MoveOperand(s14), MoveOperand(s0), MoveOp::FLOAT32));
    sim->set_s_register_from_float(14, 14);
    TRY(mr.addMove(MoveOperand(s28), MoveOperand(s29), MoveOp::FLOAT32));
    sim->set_s_register_from_float(28, 28);
    TRY(mr.addMove(MoveOperand(d12), MoveOperand(d9), MoveOp::DOUBLE));
    sim->set_d_register_from_double(12, 12);
    TRY(mr.addMove(MoveOperand(s29), MoveOperand(s2), MoveOp::FLOAT32));
    sim->set_s_register_from_float(29, 29);
    TRY(mr.addMove(MoveOperand(s22), MoveOperand(s27), MoveOp::FLOAT32));
    sim->set_s_register_from_float(22, 22);
    TRY(mr.addMove(MoveOperand(s19), MoveOperand(s3), MoveOp::FLOAT32));
    sim->set_s_register_from_float(19, 19);
    TRY(mr.addMove(MoveOperand(s21), MoveOperand(s11), MoveOp::FLOAT32));
    sim->set_s_register_from_float(21, 21);
    TRY(mr.addMove(MoveOperand(s22), MoveOperand(s13), MoveOp::FLOAT32));
    sim->set_s_register_from_float(22, 22);
    TRY(mr.addMove(MoveOperand(s29), MoveOperand(s25), MoveOp::FLOAT32));
    sim->set_s_register_from_float(29, 29);
    TRY(mr.addMove(MoveOperand(s29), MoveOperand(s15), MoveOp::FLOAT32));
    sim->set_s_register_from_float(29, 29);
    TRY(mr.addMove(MoveOperand(s16), MoveOperand(s23), MoveOp::FLOAT32));
    sim->set_s_register_from_float(16, 16);
    // don't explode!
    TRY(mr.resolve());
    mover.emit(mr);
    mover.finish();
    masm.abiret();
    JitCode* code = linkAndAllocate(cx, &masm);
    sim->skipCalleeSavedRegsCheck = true;
    sim->call(code->raw(), 1, 1);

    float f;
    double d;
    sim->get_float_from_s_register(21, &f);
    CHECK(int(f) == 0);
    sim->get_float_from_s_register(26, &f);
    CHECK(int(f) == 2);
    sim->get_float_from_s_register(20, &f);
    CHECK(int(f) == 19);
    sim->get_float_from_s_register(24, &f);
    CHECK(int(f) == 4);
    sim->get_float_from_s_register(9, &f);
    CHECK(int(f) == 22);
    sim->get_float_from_s_register(28, &f);
    CHECK(int(f) == 5);
    sim->get_float_from_s_register(7, &f);
    CHECK(int(f) == 15);
    sim->get_float_from_s_register(14, &f);
    CHECK(int(f) == 26);
    sim->get_float_from_s_register(30, &f);
    CHECK(int(f) == 13);
    sim->get_float_from_s_register(22, &f);
    CHECK(int(f) == 26);
    sim->get_float_from_s_register(6, &f);
    CHECK(int(f) == 21);
    sim->get_float_from_s_register(31, &f);
    CHECK(int(f) == 23);
    sim->get_float_from_s_register(12, &f);
    CHECK(int(f) == 7);
    sim->get_float_from_s_register(10, &f);
    CHECK(int(f) == 14);
    sim->get_double_from_d_register(8, &d);
    CHECK(int(d) == 12);
    sim->get_float_from_s_register(1, &f);
    CHECK(int(f) == 5);
    sim->get_double_from_d_register(2, &d);
    CHECK(int(d) == 12);
    sim->get_float_from_s_register(8, &f);
    CHECK(int(f) == 3);
    sim->get_float_from_s_register(0, &f);
    CHECK(int(f) == 14);
    sim->get_float_from_s_register(29, &f);
    CHECK(int(f) == 28);
    sim->get_double_from_d_register(9, &d);
    CHECK(int(d) == 12);
    sim->get_float_from_s_register(2, &f);
    CHECK(int(f) == 29);
    sim->get_float_from_s_register(27, &f);
    CHECK(int(f) == 22);
    sim->get_float_from_s_register(3, &f);
    CHECK(int(f) == 19);
    sim->get_float_from_s_register(11, &f);
    CHECK(int(f) == 21);
    sim->get_float_from_s_register(13, &f);
    CHECK(int(f) == 22);
    sim->get_float_from_s_register(25, &f);
    CHECK(int(f) == 29);
    sim->get_float_from_s_register(15, &f);
    CHECK(int(f) == 29);
    sim->get_float_from_s_register(23, &f);
    CHECK(int(f) == 16);
    return true;
}
END_TEST(testJitMoveEmitterCycles_autogen3)

#endif
