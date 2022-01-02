/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(JS_SIMULATOR_MIPS32)
#  include "jit/Linker.h"
#  include "jit/MacroAssembler.h"
#  include "jit/mips32/Assembler-mips32.h"
#  include "jit/mips32/MoveEmitter-mips32.h"
#  include "jit/mips32/Simulator-mips32.h"
#  include "jit/MoveResolver.h"

#  include "jsapi-tests/tests.h"

#  include "vm/Runtime.h"

static const int LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 4 * 1024;

static constexpr js::jit::FloatRegister single0(0,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single1(1,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single2(2,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single3(3,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single4(4,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single5(5,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single6(6,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single7(7,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single8(8,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single9(9,
                                                js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single10(
    10, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single11(
    11, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single12(
    12, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single13(
    13, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single14(
    14, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single15(
    15, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single16(
    16, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single17(
    17, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single18(
    18, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single19(
    19, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single20(
    20, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single21(
    21, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single22(
    22, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single23(
    23, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single24(
    24, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single25(
    25, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single26(
    26, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single27(
    27, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single28(
    28, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single29(
    29, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single30(
    30, js::jit::FloatRegister::Single);
static constexpr js::jit::FloatRegister single31(
    31, js::jit::FloatRegister::Single);

static constexpr js::jit::FloatRegister double0(0,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double1(2,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double2(4,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double3(6,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double4(8,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double5(10,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double6(12,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double7(14,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double8(16,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double9(18,
                                                js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double10(
    20, js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double11(
    22, js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double12(
    24, js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double13(
    26, js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double14(
    28, js::jit::FloatRegister::Double);
static constexpr js::jit::FloatRegister double15(
    30, js::jit::FloatRegister::Double);

static js::jit::JitCode* linkAndAllocate(JSContext* cx,
                                         js::jit::MacroAssembler* masm) {
  using namespace js;
  using namespace js::jit;
  Linker l(*masm);
  return l.newCode(cx, CodeKind::Ion);
}

#  define TRY(x) \
    if (!(x)) return false;

BEGIN_TEST(testJitMoveEmitterCycles_simple) {
  using namespace js;
  using namespace js::jit;
  LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
  TempAllocator alloc(&lifo);
  JitContext jc(cx, &alloc);

  StackMacroAssembler masm;
  MoveEmitter mover(masm);
  MoveResolver mr;
  mr.setAllocator(alloc);
  Simulator* sim = Simulator::Current();
  TRY(mr.addMove(MoveOperand(double0), MoveOperand(double2), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double0.id(), 2.0);
  TRY(mr.addMove(MoveOperand(double3), MoveOperand(double1), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double3.id(), 1.0);
  TRY(mr.addMove(MoveOperand(single4), MoveOperand(single0), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single4.id(), 0.0f);
  TRY(mr.addMove(MoveOperand(single5), MoveOperand(single6), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single5.id(), 6.0f);
  TRY(mr.addMove(MoveOperand(single2), MoveOperand(single1), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single2.id(), 1.0f);
  TRY(mr.addMove(MoveOperand(single3), MoveOperand(single7), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single3.id(), 7.0f);
  // don't explode!
  TRY(mr.resolve());
  mover.emit(mr);
  mover.finish();
  masm.abiret();
  JitCode* code = linkAndAllocate(cx, &masm);
  sim->call(code->raw(), 1, 1);
  CHECK(sim->getFpuRegisterDouble(double2.id()) == 2.0);
  CHECK(int(sim->getFpuRegisterDouble(double1.id())) == 1.0);
  CHECK(int(sim->getFpuRegisterFloat(single0.id())) == 0.0);
  CHECK(int(sim->getFpuRegisterFloat(single6.id())) == 6.0);
  CHECK(int(sim->getFpuRegisterFloat(single1.id())) == 1.0);
  CHECK(int(sim->getFpuRegisterFloat(single7.id())) == 7.0);
  return true;
}
END_TEST(testJitMoveEmitterCycles_simple)
BEGIN_TEST(testJitMoveEmitterCycles_autogen) {
  using namespace js;
  using namespace js::jit;
  LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
  TempAllocator alloc(&lifo);
  JitContext jc(cx, &alloc);
  StackMacroAssembler masm;
  MoveEmitter mover(masm);
  MoveResolver mr;
  mr.setAllocator(alloc);
  Simulator* sim = Simulator::Current();
  sim->setFpuRegisterDouble(double9.id(), 9.0);
  TRY(mr.addMove(MoveOperand(single24), MoveOperand(single25),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single24.id(), 24.0f);
  TRY(mr.addMove(MoveOperand(double3), MoveOperand(double0), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double3.id(), 3.0);
  TRY(mr.addMove(MoveOperand(single10), MoveOperand(single31),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single10.id(), 10.0f);
  TRY(mr.addMove(MoveOperand(double1), MoveOperand(double10), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double1.id(), 1.0);
  TRY(mr.addMove(MoveOperand(single8), MoveOperand(single10), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single8.id(), 8.0f);
  TRY(mr.addMove(MoveOperand(double2), MoveOperand(double7), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double2.id(), 2.0);
  TRY(mr.addMove(MoveOperand(single1), MoveOperand(single3), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single1.id(), 1.0f);
  TRY(mr.addMove(MoveOperand(single17), MoveOperand(single11),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single17.id(), 17.0f);
  TRY(mr.addMove(MoveOperand(single22), MoveOperand(single30),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single22.id(), 22.0f);
  TRY(mr.addMove(MoveOperand(single31), MoveOperand(single7), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single31.id(), 31.0f);
  TRY(mr.addMove(MoveOperand(double3), MoveOperand(double13), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double3.id(), 3.0);
  TRY(mr.addMove(MoveOperand(single31), MoveOperand(single23),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single31.id(), 31.0f);
  TRY(mr.addMove(MoveOperand(single13), MoveOperand(single8), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single13.id(), 13.0f);
  TRY(mr.addMove(MoveOperand(single28), MoveOperand(single5), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single28.id(), 28.0f);
  TRY(mr.addMove(MoveOperand(single20), MoveOperand(single6), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single20.id(), 20.0f);
  TRY(mr.addMove(MoveOperand(single0), MoveOperand(single2), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single0.id(), 0.0f);
  TRY(mr.addMove(MoveOperand(double7), MoveOperand(double6), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double7.id(), 7.0);
  TRY(mr.addMove(MoveOperand(single13), MoveOperand(single9), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single13.id(), 13.0f);
  TRY(mr.addMove(MoveOperand(single1), MoveOperand(single4), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single1.id(), 1.0f);
  TRY(mr.addMove(MoveOperand(single29), MoveOperand(single22),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single29.id(), 29.0f);
  TRY(mr.addMove(MoveOperand(single25), MoveOperand(single24),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single25.id(), 25.0f);
  TRY(mr.resolve());
  mover.emit(mr);
  mover.finish();
  masm.abiret();
  JitCode* code = linkAndAllocate(cx, &masm);
  sim->call(code->raw(), 1, 1);
  CHECK(int(sim->getFpuRegisterFloat(single25.id())) == 24.0);
  CHECK(int(sim->getFpuRegisterDouble(double0.id())) == 3.0);
  CHECK(int(sim->getFpuRegisterFloat(single31.id())) == 10.0);
  CHECK(int(sim->getFpuRegisterDouble(double10.id())) == 1.0);
  CHECK(int(sim->getFpuRegisterFloat(single10.id())) == 8.0);
  CHECK(int(sim->getFpuRegisterDouble(double7.id())) == 2.0);
  CHECK(int(sim->getFpuRegisterFloat(single3.id())) == 1.0);
  CHECK(int(sim->getFpuRegisterFloat(single11.id())) == 17.0);
  CHECK(int(sim->getFpuRegisterFloat(single30.id())) == 22.0);
  CHECK(int(sim->getFpuRegisterFloat(single7.id())) == 31.0);
  CHECK(int(sim->getFpuRegisterDouble(double13.id())) == 3.0);
  CHECK(int(sim->getFpuRegisterFloat(single23.id())) == 31.0);
  CHECK(int(sim->getFpuRegisterFloat(single8.id())) == 13.0);
  CHECK(int(sim->getFpuRegisterFloat(single5.id())) == 28.0);
  CHECK(int(sim->getFpuRegisterFloat(single6.id())) == 20.0);
  CHECK(int(sim->getFpuRegisterFloat(single2.id())) == 0.0);
  CHECK(int(sim->getFpuRegisterDouble(double6.id())) == 7.0);
  CHECK(int(sim->getFpuRegisterFloat(single9.id())) == 13.0);
  CHECK(int(sim->getFpuRegisterFloat(single4.id())) == 1.0);
  CHECK(int(sim->getFpuRegisterFloat(single22.id())) == 29.0);
  CHECK(int(sim->getFpuRegisterFloat(single24.id())) == 25.0);
  return true;
}
END_TEST(testJitMoveEmitterCycles_autogen)

BEGIN_TEST(testJitMoveEmitterCycles_autogen2) {
  using namespace js;
  using namespace js::jit;
  LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
  TempAllocator alloc(&lifo);
  JitContext jc(cx, &alloc);
  StackMacroAssembler masm;
  MoveEmitter mover(masm);
  MoveResolver mr;
  mr.setAllocator(alloc);
  Simulator* sim = Simulator::Current();
  TRY(mr.addMove(MoveOperand(double10), MoveOperand(double0), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double10.id(), 10.0);
  TRY(mr.addMove(MoveOperand(single15), MoveOperand(single3), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single15.id(), 15.0f);
  TRY(mr.addMove(MoveOperand(single2), MoveOperand(single28), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single2.id(), 2.0f);
  TRY(mr.addMove(MoveOperand(single30), MoveOperand(single25),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single30.id(), 30.0f);
  TRY(mr.addMove(MoveOperand(single16), MoveOperand(single2), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single16.id(), 16.0f);
  TRY(mr.addMove(MoveOperand(single2), MoveOperand(single29), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single2.id(), 2.0f);
  TRY(mr.addMove(MoveOperand(single17), MoveOperand(single10),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single17.id(), 17.0f);
  TRY(mr.addMove(MoveOperand(single9), MoveOperand(single26), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single9.id(), 9.0f);
  TRY(mr.addMove(MoveOperand(single1), MoveOperand(single23), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single1.id(), 1.0f);
  TRY(mr.addMove(MoveOperand(single8), MoveOperand(single6), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single8.id(), 8.0f);
  TRY(mr.addMove(MoveOperand(single24), MoveOperand(single16),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single24.id(), 24.0f);
  TRY(mr.addMove(MoveOperand(double5), MoveOperand(double6), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double5.id(), 5.0f);
  TRY(mr.addMove(MoveOperand(single23), MoveOperand(single30),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single23.id(), 23.0f);
  TRY(mr.addMove(MoveOperand(single27), MoveOperand(single17),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single27.id(), 27.0f);
  TRY(mr.addMove(MoveOperand(double3), MoveOperand(double4), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double3.id(), 3.0f);
  TRY(mr.addMove(MoveOperand(single14), MoveOperand(single27),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single14.id(), 14.0f);
  TRY(mr.addMove(MoveOperand(single2), MoveOperand(single31), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single2.id(), 2.0f);
  TRY(mr.addMove(MoveOperand(single2), MoveOperand(single24), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single2.id(), 2.0f);
  TRY(mr.addMove(MoveOperand(single31), MoveOperand(single11),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single31.id(), 31.0f);
  TRY(mr.addMove(MoveOperand(single24), MoveOperand(single7), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single24.id(), 24.0f);
  TRY(mr.addMove(MoveOperand(single0), MoveOperand(single21), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single0.id(), 0.0f);
  TRY(mr.addMove(MoveOperand(single27), MoveOperand(single20),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single27.id(), 27.0f);
  TRY(mr.addMove(MoveOperand(single14), MoveOperand(single5), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single14.id(), 14.0f);
  TRY(mr.addMove(MoveOperand(single2), MoveOperand(single14), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single2.id(), 2.0f);
  TRY(mr.addMove(MoveOperand(single12), MoveOperand(single22),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single12.id(), 12.0f);
  TRY(mr.resolve());
  mover.emit(mr);
  mover.finish();
  masm.abiret();
  JitCode* code = linkAndAllocate(cx, &masm);
  sim->call(code->raw(), 1, 1);
  CHECK(int(sim->getFpuRegisterDouble(double0.id())) == 10);
  CHECK(int(sim->getFpuRegisterFloat(single3.id())) == 15);
  CHECK(int(sim->getFpuRegisterFloat(single28.id())) == 2);
  CHECK(int(sim->getFpuRegisterFloat(single25.id())) == 30);
  CHECK(int(sim->getFpuRegisterFloat(single2.id())) == 16);
  CHECK(int(sim->getFpuRegisterFloat(single29.id())) == 2);
  CHECK(int(sim->getFpuRegisterFloat(single10.id())) == 17);
  CHECK(int(sim->getFpuRegisterFloat(single26.id())) == 9);
  CHECK(int(sim->getFpuRegisterFloat(single23.id())) == 1);
  CHECK(int(sim->getFpuRegisterFloat(single6.id())) == 8);
  CHECK(int(sim->getFpuRegisterFloat(single16.id())) == 24);
  CHECK(int(sim->getFpuRegisterDouble(double6.id())) == 5);
  CHECK(int(sim->getFpuRegisterFloat(single30.id())) == 23);
  CHECK(int(sim->getFpuRegisterFloat(single17.id())) == 27);
  CHECK(int(sim->getFpuRegisterDouble(double4.id())) == 3);
  CHECK(int(sim->getFpuRegisterFloat(single27.id())) == 14);
  CHECK(int(sim->getFpuRegisterFloat(single31.id())) == 2);
  CHECK(int(sim->getFpuRegisterFloat(single24.id())) == 2);
  CHECK(int(sim->getFpuRegisterFloat(single11.id())) == 31);
  CHECK(int(sim->getFpuRegisterFloat(single7.id())) == 24);
  CHECK(int(sim->getFpuRegisterFloat(single21.id())) == 0);
  CHECK(int(sim->getFpuRegisterFloat(single20.id())) == 27);
  CHECK(int(sim->getFpuRegisterFloat(single5.id())) == 14);
  CHECK(int(sim->getFpuRegisterFloat(single14.id())) == 2);
  CHECK(int(sim->getFpuRegisterFloat(single22.id())) == 12);
  return true;
}
END_TEST(testJitMoveEmitterCycles_autogen2)

BEGIN_TEST(testJitMoveEmitterCycles_autogen3) {
  using namespace js;
  using namespace js::jit;
  LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
  TempAllocator alloc(&lifo);
  JitContext jc(cx, &alloc);
  StackMacroAssembler masm;
  MoveEmitter mover(masm);
  MoveResolver mr;
  mr.setAllocator(alloc);
  Simulator* sim = Simulator::Current();
  TRY(mr.addMove(MoveOperand(single0), MoveOperand(single21), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single0.id(), 0.0f);
  TRY(mr.addMove(MoveOperand(single2), MoveOperand(single26), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single2.id(), 2.0f);
  TRY(mr.addMove(MoveOperand(single4), MoveOperand(single24), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single4.id(), 4.0f);
  TRY(mr.addMove(MoveOperand(single22), MoveOperand(single9), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single22.id(), 22.0f);
  TRY(mr.addMove(MoveOperand(single5), MoveOperand(single28), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single5.id(), 5.0f);
  TRY(mr.addMove(MoveOperand(single15), MoveOperand(single7), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single15.id(), 15.0f);
  TRY(mr.addMove(MoveOperand(single26), MoveOperand(single14),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single26.id(), 26.0f);
  TRY(mr.addMove(MoveOperand(single13), MoveOperand(single30),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single13.id(), 13.0f);
  TRY(mr.addMove(MoveOperand(single26), MoveOperand(single22),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single26.id(), 26.0f);
  TRY(mr.addMove(MoveOperand(single21), MoveOperand(single6), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single21.id(), 21.0f);
  TRY(mr.addMove(MoveOperand(single23), MoveOperand(single31),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single23.id(), 23.0f);
  TRY(mr.addMove(MoveOperand(single7), MoveOperand(single12), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single7.id(), 7.0f);
  TRY(mr.addMove(MoveOperand(single14), MoveOperand(single10),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single14.id(), 14.0f);
  TRY(mr.addMove(MoveOperand(double12), MoveOperand(double8), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double12.id(), 12.0);
  TRY(mr.addMove(MoveOperand(single5), MoveOperand(single1), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single5.id(), 5.0f);
  TRY(mr.addMove(MoveOperand(double12), MoveOperand(double2), MoveOp::DOUBLE));
  sim->setFpuRegisterDouble(double12.id(), 12.0);
  TRY(mr.addMove(MoveOperand(single3), MoveOperand(single8), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single3.id(), 3.0f);
  TRY(mr.addMove(MoveOperand(single14), MoveOperand(single0), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single14.id(), 14.0f);
  TRY(mr.addMove(MoveOperand(single28), MoveOperand(single29),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single28.id(), 28.0f);
  TRY(mr.addMove(MoveOperand(single29), MoveOperand(single2), MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single29.id(), 29.0f);
  TRY(mr.addMove(MoveOperand(single22), MoveOperand(single27),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single22.id(), 22.0f);
  TRY(mr.addMove(MoveOperand(single21), MoveOperand(single11),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single21.id(), 21.0f);
  TRY(mr.addMove(MoveOperand(single22), MoveOperand(single13),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single22.id(), 22.0f);
  TRY(mr.addMove(MoveOperand(single29), MoveOperand(single25),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single29.id(), 29.0f);
  TRY(mr.addMove(MoveOperand(single29), MoveOperand(single15),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single29.id(), 29.0f);
  TRY(mr.addMove(MoveOperand(single16), MoveOperand(single23),
                 MoveOp::FLOAT32));
  sim->setFpuRegisterFloat(single16.id(), 16.0f);
  TRY(mr.resolve());
  mover.emit(mr);
  mover.finish();
  masm.abiret();
  JitCode* code = linkAndAllocate(cx, &masm);
  sim->call(code->raw(), 1, 1);
  CHECK(int(sim->getFpuRegisterFloat(single21.id())) == 0);
  CHECK(int(sim->getFpuRegisterFloat(single26.id())) == 2);
  CHECK(int(sim->getFpuRegisterFloat(single24.id())) == 4);
  CHECK(int(sim->getFpuRegisterFloat(single9.id())) == 22);
  CHECK(int(sim->getFpuRegisterFloat(single28.id())) == 5);
  CHECK(int(sim->getFpuRegisterFloat(single7.id())) == 15);
  CHECK(int(sim->getFpuRegisterFloat(single14.id())) == 26);
  CHECK(int(sim->getFpuRegisterFloat(single30.id())) == 13);
  CHECK(int(sim->getFpuRegisterFloat(single22.id())) == 26);
  CHECK(int(sim->getFpuRegisterFloat(single6.id())) == 21);
  CHECK(int(sim->getFpuRegisterFloat(single31.id())) == 23);
  CHECK(int(sim->getFpuRegisterFloat(single12.id())) == 7);
  CHECK(int(sim->getFpuRegisterFloat(single10.id())) == 14);
  CHECK(int(sim->getFpuRegisterDouble(double8.id())) == 12);
  CHECK(int(sim->getFpuRegisterFloat(single1.id())) == 5);
  CHECK(int(sim->getFpuRegisterDouble(double2.id())) == 12);
  CHECK(int(sim->getFpuRegisterFloat(single8.id())) == 3);
  CHECK(int(sim->getFpuRegisterFloat(single0.id())) == 14);
  CHECK(int(sim->getFpuRegisterFloat(single29.id())) == 28);
  CHECK(int(sim->getFpuRegisterFloat(single2.id())) == 29);
  CHECK(int(sim->getFpuRegisterFloat(single27.id())) == 22);
  CHECK(int(sim->getFpuRegisterFloat(single3.id())) == 3);
  CHECK(int(sim->getFpuRegisterFloat(single11.id())) == 21);
  CHECK(int(sim->getFpuRegisterFloat(single13.id())) == 22);
  CHECK(int(sim->getFpuRegisterFloat(single25.id())) == 29);
  CHECK(int(sim->getFpuRegisterFloat(single15.id())) == 29);
  CHECK(int(sim->getFpuRegisterFloat(single23.id())) == 16);
  return true;
}
END_TEST(testJitMoveEmitterCycles_autogen3)

#endif
