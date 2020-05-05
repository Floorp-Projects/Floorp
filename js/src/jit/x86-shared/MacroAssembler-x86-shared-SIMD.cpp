/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/MacroAssembler.h"
#include "jit/x86-shared/MacroAssembler-x86-shared.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::FloatingPoint;
using mozilla::Maybe;
using mozilla::SpecificNaN;

void MacroAssemblerX86Shared::checkedConvertFloat32x4ToInt32x4(
    FloatRegister src, FloatRegister dest, Register temp, Label* oolEntry,
    Label* rejoin) {
  // Does the conversion and jumps to the OOL entry if the result value
  // is the undefined integer pattern.
  static const SimdConstant InvalidResult =
      SimdConstant::SplatX4(int32_t(-2147483648));
  convertFloat32x4ToInt32x4(src, dest);

  ScratchSimd128Scope scratch(asMasm());
  asMasm().loadConstantSimd128Int(InvalidResult, scratch);
  packedEqualInt32x4(Operand(dest), scratch);
  // TODO (bug 1156228): If we have SSE4.1, we can use PTEST here instead of
  // the two following instructions.
  vmovmskps(scratch, temp);
  cmp32(temp, Imm32(0));
  j(Assembler::NotEqual, oolEntry);
  bind(rejoin);
}

void MacroAssemblerX86Shared::oolConvertFloat32x4ToInt32x4(
    FloatRegister src, Register temp, Label* rejoin, Label* onConversionError) {
  static const SimdConstant Int32MaxX4 = SimdConstant::SplatX4(2147483647.f);
  static const SimdConstant Int32MinX4 = SimdConstant::SplatX4(-2147483648.f);

  ScratchSimd128Scope scratch(asMasm());
  asMasm().loadConstantSimd128Float(Int32MinX4, scratch);
  vcmpleps(Operand(src), scratch, scratch);
  vmovmskps(scratch, temp);
  cmp32(temp, Imm32(15));
  j(Assembler::NotEqual, onConversionError);

  asMasm().loadConstantSimd128Float(Int32MaxX4, scratch);
  vcmpleps(Operand(src), scratch, scratch);
  vmovmskps(scratch, temp);
  cmp32(temp, Imm32(0));
  j(Assembler::NotEqual, onConversionError);

  jump(rejoin);
}

void MacroAssemblerX86Shared::checkedConvertFloat32x4ToUint32x4(
    FloatRegister in, FloatRegister out, Register temp, FloatRegister tempF,
    Label* failed) {
  // Classify lane values into 4 disjoint classes:
  //
  //   N-lanes:             in <= -1.0
  //   A-lanes:      -1.0 < in <= 0x0.ffffffp31
  //   B-lanes: 0x1.0p31 <= in <= 0x0.ffffffp32
  //   V-lanes: 0x1.0p32 <= in, or isnan(in)
  //
  // We need to bail out to throw a RangeError if we see any N-lanes or
  // V-lanes.
  //
  // For A-lanes and B-lanes, we make two float -> int32 conversions:
  //
  //   A = cvttps2dq(in)
  //   B = cvttps2dq(in - 0x1.0p31f)
  //
  // Note that the subtraction for the B computation is exact for B-lanes.
  // There is no rounding, so B is the low 31 bits of the correctly converted
  // result.
  //
  // The cvttps2dq instruction produces 0x80000000 when the input is NaN or
  // out of range for a signed int32_t. This conveniently provides the missing
  // high bit for B, so the desired result is A for A-lanes and A|B for
  // B-lanes.

  ScratchSimd128Scope scratch(asMasm());

  // TODO: If the majority of lanes are A-lanes, it could be faster to compute
  // A first, use vmovmskps to check for any non-A-lanes and handle them in
  // ool code. OTOH, we we're wrong about the lane distribution, that would be
  // slower.

  // Compute B in |scratch|.
  static const float Adjust = 0x80000000;  // 0x1.0p31f for the benefit of MSVC.
  static const SimdConstant Bias = SimdConstant::SplatX4(-Adjust);
  asMasm().loadConstantSimd128Float(Bias, scratch);
  packedAddFloat32(Operand(in), scratch);
  convertFloat32x4ToInt32x4(scratch, scratch);

  // Compute A in |out|. This is the last time we use |in| and the first time
  // we use |out|, so we can tolerate if they are the same register.
  convertFloat32x4ToInt32x4(in, out);

  // We can identify A-lanes by the sign bits in A: Any A-lanes will be
  // positive in A, and N, B, and V-lanes will be 0x80000000 in A. Compute a
  // mask of non-A-lanes into |tempF|.
  zeroSimd128Float(tempF);
  packedGreaterThanInt32x4(Operand(out), tempF);

  // Clear the A-lanes in B.
  bitwiseAndSimdInt(scratch, Operand(tempF), scratch);

  // Compute the final result: A for A-lanes, A|B for B-lanes.
  bitwiseOrSimdInt(out, Operand(scratch), out);

  // We still need to filter out the V-lanes. They would show up as 0x80000000
  // in both A and B. Since we cleared the valid A-lanes in B, the V-lanes are
  // the remaining negative lanes in B.
  vmovmskps(scratch, temp);
  cmp32(temp, Imm32(0));
  j(Assembler::NotEqual, failed);
}

void MacroAssemblerX86Shared::createInt32x4(Register lane0, Register lane1,
                                            Register lane2, Register lane3,
                                            FloatRegister dest) {
  if (AssemblerX86Shared::HasSSE41()) {
    vmovd(lane0, dest);
    vpinsrd(1, lane1, dest, dest);
    vpinsrd(2, lane2, dest, dest);
    vpinsrd(3, lane3, dest, dest);
    return;
  }

  asMasm().reserveStack(Simd128DataSize);
  store32(lane0, Address(StackPointer, 0 * sizeof(int32_t)));
  store32(lane1, Address(StackPointer, 1 * sizeof(int32_t)));
  store32(lane2, Address(StackPointer, 2 * sizeof(int32_t)));
  store32(lane3, Address(StackPointer, 3 * sizeof(int32_t)));
  loadAlignedSimd128Int(Address(StackPointer, 0), dest);
  asMasm().freeStack(Simd128DataSize);
}

void MacroAssemblerX86Shared::createFloat32x4(
    FloatRegister lane0, FloatRegister lane1, FloatRegister lane2,
    FloatRegister lane3, FloatRegister temp, FloatRegister output) {
  FloatRegister lane0Copy = reusedInputSimd128Float(lane0, output);
  FloatRegister lane1Copy = reusedInputSimd128Float(lane1, temp);
  vunpcklps(lane3, lane1Copy, temp);
  vunpcklps(lane2, lane0Copy, output);
  vunpcklps(temp, output, output);
}

void MacroAssemblerX86Shared::splatX16(Register input, FloatRegister output) {
  vmovd(input, output);
  if (AssemblerX86Shared::HasSSSE3()) {
    zeroSimd128Int(ScratchSimd128Reg);
    vpshufb(ScratchSimd128Reg, output, output);
  } else {
    // Use two shifts to duplicate the low 8 bits into the low 16 bits.
    vpsllw(Imm32(8), output, output);
    vmovdqa(output, ScratchSimd128Reg);
    vpsrlw(Imm32(8), ScratchSimd128Reg, ScratchSimd128Reg);
    vpor(ScratchSimd128Reg, output, output);
    // Then do an X8 splat.
    vpshuflw(0, output, output);
    vpshufd(0, output, output);
  }
}

void MacroAssemblerX86Shared::splatX8(Register input, FloatRegister output) {
  vmovd(input, output);
  vpshuflw(0, output, output);
  vpshufd(0, output, output);
}

void MacroAssemblerX86Shared::splatX4(Register input, FloatRegister output) {
  vmovd(input, output);
  vpshufd(0, output, output);
}

void MacroAssemblerX86Shared::splatX4(FloatRegister input,
                                      FloatRegister output) {
  FloatRegister inputCopy = reusedInputSimd128Float(input, output);
  vshufps(0, inputCopy, inputCopy, output);
}

void MacroAssemblerX86Shared::splatX2(FloatRegister input,
                                      FloatRegister output) {
  FloatRegister inputCopy = reusedInputSimd128Float(input, output);
  vshufpd(0, inputCopy, inputCopy, output);
}

void MacroAssemblerX86Shared::reinterpretSimd(bool isIntegerLaneType,
                                              FloatRegister input,
                                              FloatRegister output) {
  if (input.aliases(output)) {
    return;
  }
  if (isIntegerLaneType) {
    vmovdqa(input, output);
  } else {
    vmovaps(input, output);
  }
}

void MacroAssemblerX86Shared::extractLaneInt32x4(FloatRegister input,
                                                 Register output,
                                                 unsigned lane) {
  if (lane == 0) {
    // The value we want to extract is in the low double-word
    moveLowInt32(input, output);
  } else if (AssemblerX86Shared::HasSSE41()) {
    vpextrd(lane, input, output);
  } else {
    uint32_t mask = MacroAssembler::ComputeShuffleMask(lane);
    shuffleInt32(mask, input, ScratchSimd128Reg);
    moveLowInt32(ScratchSimd128Reg, output);
  }
}

void MacroAssemblerX86Shared::extractLaneFloat32x4(FloatRegister input,
                                                   FloatRegister output,
                                                   unsigned lane) {
  if (lane == 0) {
    // The value we want to extract is in the low double-word
    if (input != output) {
      moveFloat32(input, output);
    }
  } else if (lane == 2) {
    moveHighPairToLowPairFloat32(input, output);
  } else {
    uint32_t mask = MacroAssembler::ComputeShuffleMask(lane);
    shuffleFloat32(mask, input, output);
  }
}

void MacroAssemblerX86Shared::extractLaneFloat64x2(FloatRegister input,
                                                   FloatRegister output,
                                                   unsigned lane) {
  if (lane == 0) {
    // The value we want to extract is in the low quadword
    if (input != output) {
      moveDouble(input, output);
    }
  } else {
    vpalignr(Operand(input), output, 8);
  }
}

void MacroAssemblerX86Shared::extractLaneInt16x8(FloatRegister input,
                                                 Register output, unsigned lane,
                                                 SimdSign sign) {
  // Unlike pextrd and pextrb, this is available in SSE2.
  vpextrw(lane, input, output);
  if (sign == SimdSign::Signed) {
    movswl(output, output);
  }
}

void MacroAssemblerX86Shared::extractLaneInt8x16(FloatRegister input,
                                                 Register output, unsigned lane,
                                                 SimdSign sign) {
  if (AssemblerX86Shared::HasSSE41()) {
    vpextrb(lane, input, output);
    // vpextrb clears the high bits, so no further extension required.
    if (sign == SimdSign::Unsigned) {
      sign = SimdSign::NotApplicable;
    }
  } else {
    // Extract the relevant 16 bits containing our lane, then shift the
    // right 8 bits into place.
    extractLaneInt16x8(input, output, lane / 2, SimdSign::Unsigned);
    if (lane % 2) {
      shrl(Imm32(8), output);
      // The shrl handles the zero-extension. Don't repeat it.
      if (sign == SimdSign::Unsigned) {
        sign = SimdSign::NotApplicable;
      }
    }
  }

  // We have the right low 8 bits in |output|, but we may need to fix the high
  // bits. Note that this requires |output| to be one of the %eax-%edx
  // registers.
  switch (sign) {
    case SimdSign::Signed:
      movsbl(output, output);
      break;
    case SimdSign::Unsigned:
      movzbl(output, output);
      break;
    case SimdSign::NotApplicable:
      // No adjustment needed.
      break;
  }
}

void MacroAssemblerX86Shared::extractLaneSimdBool(FloatRegister input,
                                                  Register output,
                                                  unsigned numLanes,
                                                  unsigned lane) {
  switch (numLanes) {
    case 4:
      extractLaneInt32x4(input, output, lane);
      break;
    case 8:
      // Get a lane, don't bother fixing the high bits since we'll mask below.
      extractLaneInt16x8(input, output, lane, SimdSign::NotApplicable);
      break;
    case 16:
      extractLaneInt8x16(input, output, lane, SimdSign::NotApplicable);
      break;
    default:
      MOZ_CRASH("Unhandled SIMD number of lanes");
  }
  // We need to generate a 0/1 value. We have 0/-1 and possibly dirty high bits.
  asMasm().and32(Imm32(1), output);
}

void MacroAssemblerX86Shared::insertLaneSimdInt(FloatRegister input,
                                                Register value,
                                                FloatRegister output,
                                                unsigned lane,
                                                unsigned numLanes) {
  if (numLanes == 8) {
    // Available in SSE 2.
    vpinsrw(lane, value, input, output);
    return;
  }

  // Note that, contrarily to float32x4, we cannot use vmovd if the inserted
  // value goes into the first component, as vmovd clears out the higher lanes
  // of the output.
  if (AssemblerX86Shared::HasSSE41()) {
    // TODO: Teach Lowering that we don't need defineReuseInput if we have AVX.
    switch (numLanes) {
      case 4:
        vpinsrd(lane, value, input, output);
        return;
      case 16:
        vpinsrb(lane, value, input, output);
        return;
    }
  }

  asMasm().reserveStack(Simd128DataSize);
  storeAlignedSimd128Int(input, Address(StackPointer, 0));
  switch (numLanes) {
    case 4:
      store32(value, Address(StackPointer, lane * sizeof(int32_t)));
      break;
    case 16:
      // Note that this requires `value` to be in one the registers where the
      // low 8 bits are addressible (%eax - %edx on x86, all of them on x86-64).
      store8(value, Address(StackPointer, lane * sizeof(int8_t)));
      break;
    default:
      MOZ_CRASH("Unsupported SIMD numLanes");
  }
  loadAlignedSimd128Int(Address(StackPointer, 0), output);
  asMasm().freeStack(Simd128DataSize);
}

void MacroAssemblerX86Shared::insertLaneFloat32x4(FloatRegister input,
                                                  FloatRegister value,
                                                  FloatRegister output,
                                                  unsigned lane) {
  // This code can't work if this is not true.  That's probably a bug.
  MOZ_RELEASE_ASSERT(input == output);

  if (lane == 0) {
    if (value != output) {
      vmovss(value, input, output);
    }
    return;
  }

  if (AssemblerX86Shared::HasSSE41()) {
    // The input value is in the low float32 of the 'value' FloatRegister.
    vinsertps(vinsertpsMask(0, lane), value, output, output);
    return;
  }

  asMasm().reserveStack(Simd128DataSize);
  storeAlignedSimd128Float(input, Address(StackPointer, 0));
  asMasm().storeFloat32(value, Address(StackPointer, lane * sizeof(int32_t)));
  loadAlignedSimd128Float(Address(StackPointer, 0), output);
  asMasm().freeStack(Simd128DataSize);
}

void MacroAssemblerX86Shared::insertLaneFloat64x2(FloatRegister input,
                                                  FloatRegister value,
                                                  FloatRegister output,
                                                  unsigned lane) {
  if (input == output && output == value) {
    // No-op
    return;
  }

  if (input != output && value != output) {
    // Merge input and value into output, so make input==output
    vmovapd(input, output);
    input = output;
  }

  if (input == output) {
    // Merge value into output
    if (lane == 0) {
      // move low qword of value into low qword of output
      vmovsd(value, output, output);
    } else {
      // move low qword of value into high qword of output
      vshufpd(0, value, output, output);
    }
  } else {
    MOZ_ASSERT(value == output);
    // Merge input into output
    if (lane == 0) {
      // move high qword of input into high qword of output
      vshufpd(2, input, output, output);
    } else {
      // move low qword of output into high qword of output
      vmovddup(output, output);
      // move low qword of input into low qword of output
      vmovsd(input, output, output);
    }
  }
}

void MacroAssemblerX86Shared::allTrueSimdBool(FloatRegister input,
                                              Register output) {
  // We know that the input lanes are boolean, so they are either 0 or -1.
  // The all-true vector has all 128 bits set, no matter the lane geometry.
  vpmovmskb(input, output);
  cmp32(output, Imm32(0xffff));
  emitSet(Assembler::Zero, output);
}

void MacroAssemblerX86Shared::anyTrueSimdBool(FloatRegister input,
                                              Register output) {
  vpmovmskb(input, output);
  cmp32(output, Imm32(0x0));
  emitSet(Assembler::NonZero, output);
}

void MacroAssemblerX86Shared::swizzleInt32x4(FloatRegister input,
                                             FloatRegister output,
                                             unsigned lanes[4]) {
  uint32_t mask = MacroAssembler::ComputeShuffleMask(lanes[0], lanes[1],
                                                     lanes[2], lanes[3]);
  shuffleInt32(mask, input, output);
}

// For SIMD.js
void MacroAssemblerX86Shared::oldSwizzleInt8x16(FloatRegister input,
                                                FloatRegister output,
                                                const Maybe<Register>& temp,
                                                int8_t lanes[16]) {
  if (AssemblerX86Shared::HasSSSE3()) {
    ScratchSimd128Scope scratch(asMasm());
    asMasm().loadConstantSimd128Int(SimdConstant::CreateX16(lanes), scratch);
    FloatRegister inputCopy = reusedInputInt32x4(input, output);
    vpshufb(scratch, inputCopy, output);
    return;
  }

  // Worst-case fallback for pre-SSSE3 machines. Bounce through memory.
  MOZ_ASSERT(!!temp, "needs a temp for the memory fallback");
  asMasm().reserveStack(2 * Simd128DataSize);
  storeAlignedSimd128Int(input, Address(StackPointer, Simd128DataSize));
  for (unsigned i = 0; i < 16; i++) {
    load8ZeroExtend(Address(StackPointer, Simd128DataSize + lanes[i]), *temp);
    store8(*temp, Address(StackPointer, i));
  }
  loadAlignedSimd128Int(Address(StackPointer, 0), output);
  asMasm().freeStack(2 * Simd128DataSize);
}

static inline bool LanesMatch(unsigned lanes[4], unsigned x, unsigned y,
                              unsigned z, unsigned w) {
  return lanes[0] == x && lanes[1] == y && lanes[2] == z && lanes[3] == w;
}

void MacroAssemblerX86Shared::swizzleFloat32x4(FloatRegister input,
                                               FloatRegister output,
                                               unsigned lanes[4]) {
  if (AssemblerX86Shared::HasSSE3()) {
    if (LanesMatch(lanes, 0, 0, 2, 2)) {
      vmovsldup(input, output);
      return;
    }
    if (LanesMatch(lanes, 1, 1, 3, 3)) {
      vmovshdup(input, output);
      return;
    }
  }

  // TODO Here and below, arch specific lowering could identify this pattern
  // and use defineReuseInput to avoid this move (bug 1084404)
  if (LanesMatch(lanes, 2, 3, 2, 3)) {
    FloatRegister inputCopy = reusedInputSimd128Float(input, output);
    vmovhlps(input, inputCopy, output);
    return;
  }

  if (LanesMatch(lanes, 0, 1, 0, 1)) {
    if (AssemblerX86Shared::HasSSE3() && !AssemblerX86Shared::HasAVX()) {
      vmovddup(input, output);
      return;
    }
    FloatRegister inputCopy = reusedInputSimd128Float(input, output);
    vmovlhps(input, inputCopy, output);
    return;
  }

  if (LanesMatch(lanes, 0, 0, 1, 1)) {
    FloatRegister inputCopy = reusedInputSimd128Float(input, output);
    vunpcklps(input, inputCopy, output);
    return;
  }

  if (LanesMatch(lanes, 2, 2, 3, 3)) {
    FloatRegister inputCopy = reusedInputSimd128Float(input, output);
    vunpckhps(input, inputCopy, output);
    return;
  }

  uint32_t x = lanes[0];
  uint32_t y = lanes[1];
  uint32_t z = lanes[2];
  uint32_t w = lanes[3];

  uint32_t mask = MacroAssembler::ComputeShuffleMask(x, y, z, w);
  shuffleFloat32(mask, input, output);
}

void MacroAssemblerX86Shared::shuffleInt8x16(
    FloatRegister lhs, FloatRegister rhs, FloatRegister output,
    const Maybe<FloatRegister>& maybeFloatTemp,
    const Maybe<Register>& maybeTemp, uint8_t lanes[16]) {
  DebugOnly<bool> hasSSSE3 = AssemblerX86Shared::HasSSSE3();
  MOZ_ASSERT(hasSSSE3 == !!maybeFloatTemp);
  MOZ_ASSERT(!hasSSSE3 == !!maybeTemp);

  // Use pshufb if it is available.
  if (AssemblerX86Shared::HasSSSE3()) {
    ScratchSimd128Scope scratch(asMasm());

    // Use pshufb instructions to gather the lanes from each source vector.
    // A negative index creates a zero lane, so the two vectors can be combined.

    // Set scratch = lanes from lhs.
    int8_t idx[16];
    for (unsigned i = 0; i < 16; i++) {
      idx[i] = lanes[i] < 16 ? lanes[i] : -1;
    }
    asMasm().loadConstantSimd128Int(SimdConstant::CreateX16(idx),
                                    *maybeFloatTemp);
    FloatRegister lhsCopy = reusedInputInt32x4(lhs, scratch);
    vpshufb(*maybeFloatTemp, lhsCopy, scratch);

    // Set output = lanes from rhs.
    for (unsigned i = 0; i < 16; i++) {
      idx[i] = lanes[i] >= 16 ? lanes[i] - 16 : -1;
    }
    asMasm().loadConstantSimd128Int(SimdConstant::CreateX16(idx),
                                    *maybeFloatTemp);
    FloatRegister rhsCopy = reusedInputInt32x4(rhs, output);
    vpshufb(*maybeFloatTemp, rhsCopy, output);

    // Combine.
    vpor(scratch, output, output);
    return;
  }

  // Worst-case fallback for pre-SSE3 machines. Bounce through memory.
  asMasm().reserveStack(3 * Simd128DataSize);
  storeAlignedSimd128Int(lhs, Address(StackPointer, Simd128DataSize));
  storeAlignedSimd128Int(rhs, Address(StackPointer, 2 * Simd128DataSize));
  for (unsigned i = 0; i < 16; i++) {
    load8ZeroExtend(Address(StackPointer, Simd128DataSize + lanes[i]),
                    *maybeTemp);
    store8(*maybeTemp, Address(StackPointer, i));
  }
  loadAlignedSimd128Int(Address(StackPointer, 0), output);
  asMasm().freeStack(3 * Simd128DataSize);
}

void MacroAssemblerX86Shared::shuffleX4(FloatRegister lhs, Operand rhs,
                                        FloatRegister out,
                                        const Maybe<FloatRegister>& maybeTemp,
                                        unsigned lanes[4]) {
  uint32_t x = lanes[0];
  uint32_t y = lanes[1];
  uint32_t z = lanes[2];
  uint32_t w = lanes[3];

  // Check that lanes come from LHS in majority:
  unsigned numLanesFromLHS = (x < 4) + (y < 4) + (z < 4) + (w < 4);
  MOZ_ASSERT(numLanesFromLHS >= 2);

  // When reading this method, remember that vshufps takes the two first
  // inputs of the destination operand (right operand) and the two last
  // inputs of the source operand (left operand).
  //
  // Legend for explanations:
  // - L: LHS
  // - R: RHS
  // - T: temporary

  uint32_t mask;

  // If all lanes came from a single vector, we should use swizzle instead.
  MOZ_ASSERT(numLanesFromLHS < 4);

  // If all values stay in their lane, this is a blend.
  if (AssemblerX86Shared::HasSSE41()) {
    if (x % 4 == 0 && y % 4 == 1 && z % 4 == 2 && w % 4 == 3) {
      vblendps(blendpsMask(x >= 4, y >= 4, z >= 4, w >= 4), rhs, lhs, out);
      return;
    }
  }

  // One element of the second, all other elements of the first
  if (numLanesFromLHS == 3) {
    unsigned firstMask = -1, secondMask = -1;

    // register-register vmovss preserves the high lanes.
    if (LanesMatch(lanes, 4, 1, 2, 3) && rhs.kind() == Operand::FPREG) {
      vmovss(FloatRegister::FromCode(rhs.fpu()), lhs, out);
      return;
    }

    // SSE4.1 vinsertps can handle any single element.
    unsigned numLanesUnchanged = (x == 0) + (y == 1) + (z == 2) + (w == 3);
    if (AssemblerX86Shared::HasSSE41() && numLanesUnchanged == 3) {
      unsigned srcLane;
      unsigned dstLane;
      if (x >= 4) {
        srcLane = x - 4;
        dstLane = 0;
      } else if (y >= 4) {
        srcLane = y - 4;
        dstLane = 1;
      } else if (z >= 4) {
        srcLane = z - 4;
        dstLane = 2;
      } else {
        MOZ_ASSERT(w >= 4);
        srcLane = w - 4;
        dstLane = 3;
      }
      vinsertps(vinsertpsMask(srcLane, dstLane), rhs, lhs, out);
      return;
    }

    MOZ_ASSERT(!!maybeTemp);
    FloatRegister rhsCopy = *maybeTemp;
    loadAlignedSimd128Float(rhs, rhsCopy);

    if (x < 4 && y < 4) {
      if (w >= 4) {
        w %= 4;
        // T = (Rw Rw Lz Lz) = vshufps(firstMask, lhs, rhs, rhsCopy)
        firstMask = MacroAssembler::ComputeShuffleMask(w, w, z, z);
        // (Lx Ly Lz Rw) = (Lx Ly Tz Tx) = vshufps(secondMask, T, lhs, out)
        secondMask = MacroAssembler::ComputeShuffleMask(x, y, 2, 0);
      } else {
        MOZ_ASSERT(z >= 4);
        z %= 4;
        // T = (Rz Rz Lw Lw) = vshufps(firstMask, lhs, rhs, rhsCopy)
        firstMask = MacroAssembler::ComputeShuffleMask(z, z, w, w);
        // (Lx Ly Rz Lw) = (Lx Ly Tx Tz) = vshufps(secondMask, T, lhs, out)
        secondMask = MacroAssembler::ComputeShuffleMask(x, y, 0, 2);
      }

      vshufps(firstMask, lhs, rhsCopy, rhsCopy);
      vshufps(secondMask, rhsCopy, lhs, out);
      return;
    }

    MOZ_ASSERT(z < 4 && w < 4);

    if (y >= 4) {
      y %= 4;
      // T = (Ry Ry Lx Lx) = vshufps(firstMask, lhs, rhs, rhsCopy)
      firstMask = MacroAssembler::ComputeShuffleMask(y, y, x, x);
      // (Lx Ry Lz Lw) = (Tz Tx Lz Lw) = vshufps(secondMask, lhs, T, out)
      secondMask = MacroAssembler::ComputeShuffleMask(2, 0, z, w);
    } else {
      MOZ_ASSERT(x >= 4);
      x %= 4;
      // T = (Rx Rx Ly Ly) = vshufps(firstMask, lhs, rhs, rhsCopy)
      firstMask = MacroAssembler::ComputeShuffleMask(x, x, y, y);
      // (Rx Ly Lz Lw) = (Tx Tz Lz Lw) = vshufps(secondMask, lhs, T, out)
      secondMask = MacroAssembler::ComputeShuffleMask(0, 2, z, w);
    }

    vshufps(firstMask, lhs, rhsCopy, rhsCopy);
    if (AssemblerX86Shared::HasAVX()) {
      vshufps(secondMask, lhs, rhsCopy, out);
    } else {
      vshufps(secondMask, lhs, rhsCopy, rhsCopy);
      moveSimd128Float(rhsCopy, out);
    }
    return;
  }

  // Two elements from one vector, two other elements from the other
  MOZ_ASSERT(numLanesFromLHS == 2);

  // TODO Here and below, symmetric case would be more handy to avoid a move,
  // but can't be reached because operands would get swapped (bug 1084404).
  if (LanesMatch(lanes, 2, 3, 6, 7)) {
    ScratchSimd128Scope scratch(asMasm());
    if (AssemblerX86Shared::HasAVX()) {
      FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, scratch);
      vmovhlps(lhs, rhsCopy, out);
    } else {
      loadAlignedSimd128Float(rhs, scratch);
      vmovhlps(lhs, scratch, scratch);
      moveSimd128Float(scratch, out);
    }
    return;
  }

  if (LanesMatch(lanes, 0, 1, 4, 5)) {
    FloatRegister rhsCopy;
    ScratchSimd128Scope scratch(asMasm());
    if (rhs.kind() == Operand::FPREG) {
      // No need to make an actual copy, since the operand is already
      // in a register, and it won't be clobbered by the vmovlhps.
      rhsCopy = FloatRegister::FromCode(rhs.fpu());
    } else {
      loadAlignedSimd128Float(rhs, scratch);
      rhsCopy = scratch;
    }
    vmovlhps(rhsCopy, lhs, out);
    return;
  }

  if (LanesMatch(lanes, 0, 4, 1, 5)) {
    vunpcklps(rhs, lhs, out);
    return;
  }

  // TODO swapped case would be better (bug 1084404)
  if (LanesMatch(lanes, 4, 0, 5, 1)) {
    ScratchSimd128Scope scratch(asMasm());
    if (AssemblerX86Shared::HasAVX()) {
      FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, scratch);
      vunpcklps(lhs, rhsCopy, out);
    } else {
      loadAlignedSimd128Float(rhs, scratch);
      vunpcklps(lhs, scratch, scratch);
      moveSimd128Float(scratch, out);
    }
    return;
  }

  if (LanesMatch(lanes, 2, 6, 3, 7)) {
    vunpckhps(rhs, lhs, out);
    return;
  }

  // TODO swapped case would be better (bug 1084404)
  if (LanesMatch(lanes, 6, 2, 7, 3)) {
    ScratchSimd128Scope scratch(asMasm());
    if (AssemblerX86Shared::HasAVX()) {
      FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, scratch);
      vunpckhps(lhs, rhsCopy, out);
    } else {
      loadAlignedSimd128Float(rhs, scratch);
      vunpckhps(lhs, scratch, scratch);
      moveSimd128Float(scratch, out);
    }
    return;
  }

  // In one vshufps
  if (x < 4 && y < 4) {
    mask = MacroAssembler::ComputeShuffleMask(x, y, z % 4, w % 4);
    vshufps(mask, rhs, lhs, out);
    return;
  }

  // At creation, we should have explicitly swapped in this case.
  MOZ_ASSERT(!(z >= 4 && w >= 4));

  // In two vshufps, for the most generic case:
  uint32_t firstMask[4], secondMask[4];
  unsigned i = 0, j = 2, k = 0;

#define COMPUTE_MASK(lane)   \
  if (lane >= 4) {           \
    firstMask[j] = lane % 4; \
    secondMask[k++] = j++;   \
  } else {                   \
    firstMask[i] = lane;     \
    secondMask[k++] = i++;   \
  }

  COMPUTE_MASK(x)
  COMPUTE_MASK(y)
  COMPUTE_MASK(z)
  COMPUTE_MASK(w)
#undef COMPUTE_MASK

  MOZ_ASSERT(i == 2 && j == 4 && k == 4);

  mask = MacroAssembler::ComputeShuffleMask(firstMask[0], firstMask[1],
                                            firstMask[2], firstMask[3]);
  vshufps(mask, rhs, lhs, lhs);

  mask = MacroAssembler::ComputeShuffleMask(secondMask[0], secondMask[1],
                                            secondMask[2], secondMask[3]);
  vshufps(mask, lhs, lhs, lhs);
}

static inline FloatRegister ToSimdFloatRegister(const Operand& op) {
  return FloatRegister(op.fpu(), FloatRegister::Codes::ContentType::Simd128);
}

void MacroAssemblerX86Shared::compareInt8x16(FloatRegister lhs, Operand rhs,
                                             Assembler::Condition cond,
                                             FloatRegister output) {
  static const SimdConstant allOnes = SimdConstant::SplatX16(-1);
  ScratchSimd128Scope scratch(asMasm());
  switch (cond) {
    case Assembler::Condition::GreaterThan:
      vpcmpgtb(rhs, lhs, output);
      break;
    case Assembler::Condition::Equal:
      vpcmpeqb(rhs, lhs, output);
      break;
    case Assembler::Condition::LessThan:
      // src := rhs
      if (rhs.kind() == Operand::FPREG) {
        moveSimd128Int(ToSimdFloatRegister(rhs), scratch);
      } else {
        loadAlignedSimd128Int(rhs, scratch);
      }

      // src := src > lhs (i.e. lhs < rhs)
      // Improve by doing custom lowering (rhs is tied to the output register)
      vpcmpgtb(Operand(lhs), scratch, scratch);
      moveSimd128Int(scratch, output);
      break;
    case Assembler::Condition::NotEqual:
      // Ideally for notEqual, greaterThanOrEqual, and lessThanOrEqual, we
      // should invert the comparison by, e.g. swapping the arms of a select
      // if that's what it's used in.
      asMasm().loadConstantSimd128Int(allOnes, scratch);
      vpcmpeqb(rhs, lhs, output);
      bitwiseXorSimdInt(output, Operand(scratch), output);
      break;
    case Assembler::Condition::GreaterThanOrEqual:
      // src := rhs
      if (rhs.kind() == Operand::FPREG) {
        moveSimd128Int(ToSimdFloatRegister(rhs), scratch);
      } else {
        loadAlignedSimd128Int(rhs, scratch);
      }
      vpcmpgtb(Operand(lhs), scratch, scratch);
      asMasm().loadConstantSimd128Int(allOnes, output);
      bitwiseXorSimdInt(output, Operand(scratch), output);
      break;
    case Assembler::Condition::LessThanOrEqual:
      // lhs <= rhs is equivalent to !(rhs < lhs), which we compute here.
      asMasm().loadConstantSimd128Int(allOnes, scratch);
      vpcmpgtb(rhs, lhs, output);
      bitwiseXorSimdInt(output, Operand(scratch), output);
      break;
    default:
      MOZ_CRASH("unexpected condition op");
  }
}

void MacroAssemblerX86Shared::unsignedCompareInt8x16(
    FloatRegister lhs, Operand rhs, Assembler::Condition cond,
    FloatRegister output, FloatRegister tmp1, FloatRegister tmp2) {
  // We widen the inputs to 16 bits, transforming them to nonnegative values;
  // then compare them as signed using the logic from compareInt8x16(); then
  // merge the results (which is surprisingly complicated).  rhs is left
  // untouched.  The logic is open-coded to streamline it.
  //
  // TODO?  Rhs could be in memory (for Ion, anyway), in which case loading it
  // into scratch first would be better than loading it twice from memory.

  MOZ_ASSERT(AssemblerX86Shared::HasSSE41());  // PMOVZX, PMOVSX
  MOZ_ASSERT(lhs == output);
  MOZ_ASSERT(lhs != tmp1 && lhs != tmp2);
  MOZ_ASSERT_IF(rhs.kind() == Operand::FPREG,
                ToSimdFloatRegister(rhs) != lhs &&
                    ToSimdFloatRegister(rhs) != tmp1 &&
                    ToSimdFloatRegister(rhs) != tmp2);
  MOZ_ASSERT(tmp1 != ScratchSimd128Reg && tmp2 != ScratchSimd128Reg);

  bool complement = false;
  switch (cond) {
    case Assembler::Above:
    case Assembler::BelowOrEqual:
      complement = cond == Assembler::BelowOrEqual;

      // Low eight bytes of inputs widened to words
      vpmovzxbw(Operand(lhs), tmp1);
      vpmovzxbw(rhs, tmp2);
      // Compare leaving 16-bit results
      vpcmpgtw(Operand(tmp2), tmp1, tmp1);  // lhs < rhs in tmp1

      // High eight bytes of inputs widened to words
      vpalignr(rhs, tmp2, 8);
      vpmovzxbw(Operand(tmp2), tmp2);
      vpalignr(Operand(lhs), output, 8);
      vpmovzxbw(Operand(output), output);
      // Compare leaving 16-bit results
      vpcmpgtw(Operand(tmp2), output, output);  // lhs < rhs in output

      break;
    case Assembler::Below:
    case Assembler::AboveOrEqual:
      complement = cond == Assembler::AboveOrEqual;

      // Same as above but with operands reversed

      // Low eight bytes of inputs widened to words
      vpmovzxbw(Operand(lhs), tmp2);
      vpmovzxbw(rhs, tmp1);
      // Compare leaving 16-bit results
      vpcmpgtw(Operand(tmp2), tmp1, tmp1);  // rhs < lhs in tmp1

      // High eight bytes of inputs widened to words
      vpalignr(Operand(lhs), tmp2, 8);
      vpmovzxbw(Operand(tmp2), tmp2);
      vpalignr(rhs, output, 8);
      vpmovzxbw(Operand(output), output);
      // Compare leaving 16-bit results
      vpcmpgtw(Operand(tmp2), output, output);  // rhs < lhs in output

      break;
    default:
      MOZ_CRASH("Unsupported condition code");
  }

  // Merge output (results of high byte compares) and tmp1 (results of low byte
  // compares) by truncating word results to bytes (to avoid signed saturation),
  // packing, and then concatenating and shifting.
  vpsrlw(Imm32(8), tmp1, tmp1);
  vpackuswb(Operand(tmp1), tmp1, tmp1);
  vpsrlw(Imm32(8), output, output);
  vpackuswb(Operand(output), output, output);
  vpalignr(Operand(tmp1), output, 8);

  // Complement when needed for opposite sense of the operator.
  if (complement) {
    vpcmpeqd(Operand(tmp1), tmp1, tmp1);
    vpxor(Operand(tmp1), output, output);
  }
}

void MacroAssemblerX86Shared::compareInt16x8(FloatRegister lhs, Operand rhs,
                                             Assembler::Condition cond,
                                             FloatRegister output) {
  static const SimdConstant allOnes = SimdConstant::SplatX8(-1);

  ScratchSimd128Scope scratch(asMasm());
  switch (cond) {
    case Assembler::Condition::GreaterThan:
      vpcmpgtw(rhs, lhs, output);
      break;
    case Assembler::Condition::Equal:
      vpcmpeqw(rhs, lhs, output);
      break;
    case Assembler::Condition::LessThan:
      // src := rhs
      if (rhs.kind() == Operand::FPREG) {
        moveSimd128Int(ToSimdFloatRegister(rhs), scratch);
      } else {
        loadAlignedSimd128Int(rhs, scratch);
      }

      // src := src > lhs (i.e. lhs < rhs)
      // Improve by doing custom lowering (rhs is tied to the output register)
      vpcmpgtw(Operand(lhs), scratch, scratch);
      moveSimd128Int(scratch, output);
      break;
    case Assembler::Condition::NotEqual:
      // Ideally for notEqual, greaterThanOrEqual, and lessThanOrEqual, we
      // should invert the comparison by, e.g. swapping the arms of a select
      // if that's what it's used in.
      asMasm().loadConstantSimd128Int(allOnes, scratch);
      vpcmpeqw(rhs, lhs, output);
      bitwiseXorSimdInt(output, Operand(scratch), output);
      break;
    case Assembler::Condition::GreaterThanOrEqual:
      // src := rhs
      if (rhs.kind() == Operand::FPREG) {
        moveSimd128Int(ToSimdFloatRegister(rhs), scratch);
      } else {
        loadAlignedSimd128Int(rhs, scratch);
      }
      vpcmpgtw(Operand(lhs), scratch, scratch);
      asMasm().loadConstantSimd128Int(allOnes, output);
      bitwiseXorSimdInt(output, Operand(scratch), output);
      break;
    case Assembler::Condition::LessThanOrEqual:
      // lhs <= rhs is equivalent to !(rhs < lhs), which we compute here.
      asMasm().loadConstantSimd128Int(allOnes, scratch);
      vpcmpgtw(rhs, lhs, output);
      bitwiseXorSimdInt(output, Operand(scratch), output);
      break;
    default:
      MOZ_CRASH("unexpected condition op");
  }
}

void MacroAssemblerX86Shared::unsignedCompareInt16x8(
    FloatRegister lhs, Operand rhs, Assembler::Condition cond,
    FloatRegister output, FloatRegister tmp1, FloatRegister tmp2) {
  // See comments at unsignedCompareInt8x16.

  MOZ_ASSERT(AssemblerX86Shared::HasSSE41());  // PMOVZX, PMOVSX
  MOZ_ASSERT(lhs == output);

  bool complement = false;
  switch (cond) {
    case Assembler::Above:
    case Assembler::BelowOrEqual:
      complement = cond == Assembler::BelowOrEqual;

      vpmovzxwd(Operand(lhs), tmp1);
      vpmovzxwd(rhs, tmp2);
      vpcmpgtd(Operand(tmp2), tmp1, tmp1);

      vpalignr(rhs, tmp2, 8);
      vpmovzxwd(Operand(tmp2), tmp2);
      vpalignr(Operand(lhs), output, 8);
      vpmovzxwd(Operand(output), output);
      vpcmpgtd(Operand(tmp2), output, output);

      break;
    case Assembler::Below:
    case Assembler::AboveOrEqual:
      complement = cond == Assembler::AboveOrEqual;

      vpmovzxwd(Operand(lhs), tmp2);
      vpmovzxwd(rhs, tmp1);
      vpcmpgtd(Operand(tmp2), tmp1, tmp1);

      vpalignr(Operand(lhs), tmp2, 8);
      vpmovzxwd(Operand(tmp2), tmp2);
      vpalignr(rhs, output, 8);
      vpmovzxwd(Operand(output), output);
      vpcmpgtd(Operand(tmp2), output, output);

      break;
    default:
      MOZ_CRASH();
  }

  vpsrld(Imm32(16), tmp1, tmp1);
  vpackusdw(Operand(tmp1), tmp1, tmp1);
  vpsrld(Imm32(16), output, output);
  vpackusdw(Operand(output), output, output);
  vpalignr(Operand(tmp1), output, 8);

  if (complement) {
    vpcmpeqd(Operand(tmp1), tmp1, tmp1);
    vpxor(Operand(tmp1), output, output);
  }
}

void MacroAssemblerX86Shared::compareInt32x4(FloatRegister lhs, Operand rhs,
                                             Assembler::Condition cond,
                                             FloatRegister output) {
  static const SimdConstant allOnes = SimdConstant::SplatX4(-1);
  ScratchSimd128Scope scratch(asMasm());
  switch (cond) {
    case Assembler::Condition::GreaterThan:
      packedGreaterThanInt32x4(rhs, lhs);
      break;
    case Assembler::Condition::Equal:
      packedEqualInt32x4(rhs, lhs);
      break;
    case Assembler::Condition::LessThan:
      // src := rhs
      if (rhs.kind() == Operand::FPREG) {
        moveSimd128Int(ToSimdFloatRegister(rhs), scratch);
      } else {
        loadAlignedSimd128Int(rhs, scratch);
      }

      // src := src > lhs (i.e. lhs < rhs)
      // Improve by doing custom lowering (rhs is tied to the output register)
      packedGreaterThanInt32x4(Operand(lhs), scratch);
      moveSimd128Int(scratch, lhs);
      break;
    case Assembler::Condition::NotEqual:
      // Ideally for notEqual, greaterThanOrEqual, and lessThanOrEqual, we
      // should invert the comparison by, e.g. swapping the arms of a select
      // if that's what it's used in.
      asMasm().loadConstantSimd128Int(allOnes, scratch);
      packedEqualInt32x4(rhs, lhs);
      bitwiseXorSimdInt(lhs, Operand(scratch), lhs);
      break;
    case Assembler::Condition::GreaterThanOrEqual:
      // src := rhs
      if (rhs.kind() == Operand::FPREG) {
        moveSimd128Int(ToSimdFloatRegister(rhs), scratch);
      } else {
        loadAlignedSimd128Int(rhs, scratch);
      }
      packedGreaterThanInt32x4(Operand(lhs), scratch);
      asMasm().loadConstantSimd128Int(allOnes, lhs);
      bitwiseXorSimdInt(lhs, Operand(scratch), lhs);
      break;
    case Assembler::Condition::LessThanOrEqual:
      // lhs <= rhs is equivalent to !(rhs < lhs), which we compute here.
      asMasm().loadConstantSimd128Int(allOnes, scratch);
      packedGreaterThanInt32x4(rhs, lhs);
      bitwiseXorSimdInt(lhs, Operand(scratch), lhs);
      break;
    default:
      MOZ_CRASH("unexpected condition op");
  }
}

void MacroAssemblerX86Shared::unsignedCompareInt32x4(
    FloatRegister lhs, Operand rhs, Assembler::Condition cond,
    FloatRegister output, FloatRegister tmp1, FloatRegister tmp2) {
  // See comments at unsignedCompareInt8x16, the logic is similar.  However we
  // only have PCMPGTQ on SSE4.2 or later, so for SSE4.1 we need to use subtract
  // to compute the flags.

  MOZ_ASSERT(AssemblerX86Shared::HasSSE41());  // PMOVZX, PMOVSX
  MOZ_ASSERT(lhs == output);

  bool complement = false;
  switch (cond) {
    case Assembler::Below:
    case Assembler::AboveOrEqual:
      complement = cond == Assembler::AboveOrEqual;

      // The effect of the subtract is that the high doubleword of each quadword
      // becomes either 0 (ge) or -1 (lt).

      vpmovzxdq(Operand(lhs), tmp1);
      vpmovzxdq(rhs, tmp2);
      vpsubq(Operand(tmp2), tmp1, tmp1);  // flag1 junk flag0 junk
      vpsrlq(Imm32(32), tmp1, tmp1);      // zero flag1 zero flag0
      vpshufd(MacroAssembler::ComputeShuffleMask(0, 2, 3, 3), tmp1,
              tmp1);  // zero zero flag1 flag0

      vpalignr(rhs, tmp2, 8);
      vpmovzxdq(Operand(tmp2), tmp2);
      vpalignr(Operand(lhs), output, 8);
      vpmovzxdq(Operand(output), output);
      vpsubq(Operand(tmp2), output, output);  // flag3 junk flag2 junk
      vpsrlq(Imm32(32), output, output);      // zero flag3 zero flag2
      vpshufd(MacroAssembler::ComputeShuffleMask(3, 3, 0, 2), output,
              output);  // flag3 flag2 zero zero

      vpor(Operand(tmp1), output, output);
      break;

    case Assembler::Above:
    case Assembler::BelowOrEqual:
      complement = cond == Assembler::BelowOrEqual;

      // The effect of the subtract is that the high doubleword of each quadword
      // becomes either 0 (le) or -1 (gt).

      vpmovzxdq(Operand(lhs), tmp2);
      vpmovzxdq(rhs, tmp1);
      vpsubq(Operand(tmp2), tmp1, tmp1);  // flag1 junk flag0 junk
      vpsrlq(Imm32(32), tmp1, tmp1);      // zero flag1 zero flag0
      vpshufd(MacroAssembler::ComputeShuffleMask(0, 2, 3, 3), tmp1,
              tmp1);  // zero zero flag1 flag0

      vpalignr(Operand(lhs), tmp2, 8);
      vpmovzxdq(Operand(tmp2), tmp2);
      vpalignr(rhs, output, 8);
      vpmovzxdq(Operand(output), output);
      vpsubq(Operand(tmp2), output, output);  // flag3 junk flag2 junk
      vpsrlq(Imm32(32), output, output);      // zero flag3 zero flag2
      vpshufd(MacroAssembler::ComputeShuffleMask(3, 3, 0, 2), output,
              output);  // flag3 flag2 zero zero

      vpor(Operand(tmp1), output, output);
      break;

    default:
      MOZ_CRASH();
  }

  if (complement) {
    vpcmpeqd(Operand(tmp1), tmp1, tmp1);
    vpxor(Operand(tmp1), output, output);
  }
}

void MacroAssemblerX86Shared::compareFloat32x4(FloatRegister lhs, Operand rhs,
                                               Assembler::Condition cond,
                                               FloatRegister output) {
  switch (cond) {
    case Assembler::Condition::Equal:
      vcmpeqps(rhs, lhs, output);
      break;
    case Assembler::Condition::LessThan:
      vcmpltps(rhs, lhs, output);
      break;
    case Assembler::Condition::LessThanOrEqual:
      vcmpleps(rhs, lhs, output);
      break;
    case Assembler::Condition::NotEqual:
      vcmpneqps(rhs, lhs, output);
      break;
    case Assembler::Condition::GreaterThanOrEqual:
    case Assembler::Condition::GreaterThan:
      // We reverse these before register allocation so that we don't have to
      // copy into and out of temporaries after codegen.
      MOZ_CRASH("should have reversed this");
    default:
      MOZ_CRASH("unexpected condition op");
  }
}

void MacroAssemblerX86Shared::compareFloat64x2(FloatRegister lhs, Operand rhs,
                                               Assembler::Condition cond,
                                               FloatRegister output) {
  switch (cond) {
    case Assembler::Condition::Equal:
      vcmpeqpd(rhs, lhs, output);
      break;
    case Assembler::Condition::LessThan:
      vcmpltpd(rhs, lhs, output);
      break;
    case Assembler::Condition::LessThanOrEqual:
      vcmplepd(rhs, lhs, output);
      break;
    case Assembler::Condition::NotEqual:
      vcmpneqpd(rhs, lhs, output);
      break;
    case Assembler::Condition::GreaterThanOrEqual:
    case Assembler::Condition::GreaterThan:
      // We reverse these before register allocation so that we don't have to
      // copy into and out of temporaries after codegen.
      MOZ_CRASH("should have reversed this");
    default:
      MOZ_CRASH("unexpected condition op");
  }
}

void MacroAssemblerX86Shared::mulInt32x4(FloatRegister lhs, Operand rhs,
                                         const Maybe<FloatRegister>& temp,
                                         FloatRegister output) {
  if (AssemblerX86Shared::HasSSE41()) {
    vpmulld(rhs, lhs, output);
    return;
  }

  ScratchSimd128Scope scratch(asMasm());
  loadAlignedSimd128Int(rhs, scratch);
  vpmuludq(lhs, scratch, scratch);
  // scratch contains (Rx, _, Rz, _) where R is the resulting vector.

  MOZ_ASSERT(!!temp);
  vpshufd(MacroAssembler::ComputeShuffleMask(1, 1, 3, 3), lhs, lhs);
  vpshufd(MacroAssembler::ComputeShuffleMask(1, 1, 3, 3), rhs, *temp);
  vpmuludq(*temp, lhs, lhs);
  // lhs contains (Ry, _, Rw, _) where R is the resulting vector.

  vshufps(MacroAssembler::ComputeShuffleMask(0, 2, 0, 2), scratch, lhs, lhs);
  // lhs contains (Ry, Rw, Rx, Rz)
  vshufps(MacroAssembler::ComputeShuffleMask(2, 0, 3, 1), lhs, lhs, lhs);
}

/* clang-format off */

// NaN is s111 1111 1qxx ... where the q indicates quiet or
// signaling NaN; q=1 means quiet.
//
// If the values are both zero the second value is returned from min/max, this
// matters if one of them is signed.
//
// If one value is a NaN then the second value is returned.
//
// Generally for min/max, the sign of zero matters (-0 < 0) and NaN inputs are
// always quiet and we want to propagate NaN.

// For min:
// - we compute x=min(a,b) and y=min(b,a) and then OR them together
// - if the values are not NaN but not both zero then x==y, the OR
//   matters not
// - if the values are -0 and 0 then we will get -0 from the OR
// - if one of the values is NaN then x or y will be NaN and we will
//   get a NaN from the OR, with some arbitrary sign, and since
//   the input NaN is quiet the output NaN will be quiet.

// For max:
// - the UNORD comparison will create a mask of ~0 in scratch if at least
//   one value is NaN, otherwise 0
// - we compute x=max(a,b) and y=max(b,a) and then AND them together
// - if the values are not NaN but not both zero then x==y, the
//   AND matters not
// - if the values are -0 and 0 then the AND will create 0
// - if one of the values is NaN then the AND will select the wrong
//   value
// - a final OR of the result with the mask from the UNORD comparison
//   will leave valid results alone and create NaN where the UNORD
//   was true.

/* clang-format on */

void MacroAssemblerX86Shared::minFloat32x4(FloatRegister lhs, Operand rhs,
                                           FloatRegister output) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, scratch);
  vminps(Operand(lhs), rhsCopy, scratch);
  vminps(rhs, lhs, output);
  vorps(scratch, output, output);  // NaN or'd with arbitrary bits is NaN
}

void MacroAssemblerX86Shared::maxFloat32x4(FloatRegister lhs, Operand rhs,
                                           FloatRegister temp,
                                           FloatRegister output) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister lhsCopy = reusedInputSimd128Float(lhs, scratch);
  vcmpunordps(rhs, lhsCopy, scratch);

  FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, temp);
  vmaxps(Operand(lhs), rhsCopy, temp);
  vmaxps(rhs, lhs, output);

  vandps(temp, output, output);
  vorps(scratch, output, output);  // or in the all-ones NaNs
}

void MacroAssemblerX86Shared::minFloat64x2(FloatRegister lhs, Operand rhs,
                                           FloatRegister output) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, scratch);
  vminpd(Operand(lhs), rhsCopy, scratch);
  vminpd(rhs, lhs, output);
  vorpd(scratch, output, output);  // NaN or'd with arbitrary bits is NaN
}

void MacroAssemblerX86Shared::maxFloat64x2(FloatRegister lhs, Operand rhs,
                                           FloatRegister temp,
                                           FloatRegister output) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister lhsCopy = reusedInputSimd128Float(lhs, scratch);
  vcmpunordpd(rhs, lhsCopy, scratch);

  FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, temp);
  vmaxpd(Operand(lhs), rhsCopy, temp);
  vmaxpd(rhs, lhs, output);

  vandpd(temp, output, output);
  vorpd(scratch, output, output);  // or in the all-ones NaNs
}

void MacroAssemblerX86Shared::minNumFloat32x4(FloatRegister lhs, Operand rhs,
                                              FloatRegister temp,
                                              FloatRegister output) {
  ScratchSimd128Scope scratch(asMasm());
  asMasm().loadConstantSimd128Int(SimdConstant::SplatX4(int32_t(0x80000000)),
                                  temp);

  FloatRegister mask = scratch;
  FloatRegister tmpCopy = reusedInputSimd128Float(temp, scratch);
  vpcmpeqd(Operand(lhs), tmpCopy, mask);
  vandps(temp, mask, mask);

  FloatRegister lhsCopy = reusedInputSimd128Float(lhs, temp);
  vminps(rhs, lhsCopy, temp);
  vorps(mask, temp, temp);

  FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, mask);
  vcmpneqps(rhs, rhsCopy, mask);

  if (AssemblerX86Shared::HasAVX()) {
    vblendvps(mask, lhs, temp, output);
  } else {
    // Emulate vblendvps.
    // With SSE.4.1 we could use blendvps, however it's awkward since
    // it requires the mask to be in xmm0.
    if (lhs != output) {
      moveSimd128Float(lhs, output);
    }
    vandps(Operand(mask), output, output);
    vandnps(Operand(temp), mask, mask);
    vorps(Operand(mask), output, output);
  }
}

void MacroAssemblerX86Shared::maxNumFloat32x4(FloatRegister lhs, Operand rhs,
                                              FloatRegister temp,
                                              FloatRegister output) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister mask = scratch;

  asMasm().loadConstantSimd128Int(SimdConstant::SplatX4(0), mask);
  vpcmpeqd(Operand(lhs), mask, mask);

  asMasm().loadConstantSimd128Int(SimdConstant::SplatX4(int32_t(0x80000000)),
                                  temp);
  vandps(temp, mask, mask);

  FloatRegister lhsCopy = reusedInputSimd128Float(lhs, temp);
  vmaxps(rhs, lhsCopy, temp);
  vandnps(Operand(temp), mask, mask);

  // Ensure temp always contains the temporary result
  mask = temp;
  temp = scratch;

  FloatRegister rhsCopy = reusedInputAlignedSimd128Float(rhs, mask);
  vcmpneqps(rhs, rhsCopy, mask);

  if (AssemblerX86Shared::HasAVX()) {
    vblendvps(mask, lhs, temp, output);
  } else {
    // Emulate vblendvps.
    // With SSE.4.1 we could use blendvps, however it's awkward since
    // it requires the mask to be in xmm0.
    if (lhs != output) {
      moveSimd128Float(lhs, output);
    }
    vandps(Operand(mask), output, output);
    vandnps(Operand(temp), mask, mask);
    vorps(Operand(mask), output, output);
  }
}

void MacroAssemblerX86Shared::negFloat32x4(Operand in, FloatRegister out) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister result = out;
  if (in.kind() == Operand::FPREG && ToSimdFloatRegister(in) == out) {
    result = scratch;
  }
  // All zeros but the sign bit
  static const SimdConstant minusZero = SimdConstant::SplatX4(-0.f);
  asMasm().loadConstantSimd128Float(minusZero, result);
  bitwiseXorFloat32x4(result, in, result);
  if (result == scratch) {
    moveSimd128Float(result, out);
  }
}

void MacroAssemblerX86Shared::negFloat64x2(Operand in, FloatRegister out) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister result = out;
  if (in.kind() == Operand::FPREG && ToSimdFloatRegister(in) == out) {
    result = scratch;
  }
  // All zeros but the sign bit
  static const SimdConstant minusZero = SimdConstant::SplatX2(-0.0);
  asMasm().loadConstantSimd128Float(minusZero, result);
  vxorpd(ToSimdFloatRegister(in), result, result);
  if (result == scratch) {
    moveSimd128Float(result, out);
  }
}

void MacroAssemblerX86Shared::notInt8x16(Operand in, FloatRegister out) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister result = out;
  if (in.kind() == Operand::FPREG && ToSimdFloatRegister(in) == out) {
    result = scratch;
  }
  static const SimdConstant allOnes = SimdConstant::SplatX16(-1);
  asMasm().loadConstantSimd128Int(allOnes, result);
  bitwiseXorSimdInt(result, in, result);
  if (result == scratch) {
    moveSimd128Float(result, out);
  }
}

void MacroAssemblerX86Shared::notInt16x8(Operand in, FloatRegister out) {
  // Bug, really
  MOZ_ASSERT_IF(in.kind() == Operand::FPREG, in.fpu() != out.encoding());
  static const SimdConstant allOnes = SimdConstant::SplatX8(-1);
  asMasm().loadConstantSimd128Int(allOnes, out);
  bitwiseXorSimdInt(out, in, out);
}

void MacroAssemblerX86Shared::notInt32x4(Operand in, FloatRegister out) {
  // Bug, really
  MOZ_ASSERT_IF(in.kind() == Operand::FPREG, in.fpu() != out.encoding());
  static const SimdConstant allOnes = SimdConstant::SplatX4(-1);
  asMasm().loadConstantSimd128Int(allOnes, out);
  bitwiseXorSimdInt(out, in, out);
}

void MacroAssemblerX86Shared::notFloat32x4(Operand in, FloatRegister out) {
  // Bug, really
  MOZ_ASSERT_IF(in.kind() == Operand::FPREG, in.fpu() != out.encoding());
  float ones = SpecificNaN<float>(1, FloatingPoint<float>::kSignificandBits);
  static const SimdConstant allOnes = SimdConstant::SplatX4(ones);
  asMasm().loadConstantSimd128Float(allOnes, out);
  bitwiseXorFloat32x4(out, in, out);
}

void MacroAssemblerX86Shared::absFloat32x4(Operand in, FloatRegister out) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister result = out;
  if (in.kind() == Operand::FPREG && ToSimdFloatRegister(in) == out) {
    result = scratch;
  }
  // All ones but the sign bit
  float signMask =
      SpecificNaN<float>(0, FloatingPoint<float>::kSignificandBits);
  static const SimdConstant signMasks = SimdConstant::SplatX4(signMask);
  asMasm().loadConstantSimd128Float(signMasks, result);
  bitwiseAndFloat32x4(result, in, result);
  if (result == scratch) {
    moveSimd128Float(result, out);
  }
}

void MacroAssemblerX86Shared::absFloat64x2(Operand in, FloatRegister out) {
  ScratchSimd128Scope scratch(asMasm());
  FloatRegister result = out;
  if (in.kind() == Operand::FPREG && ToSimdFloatRegister(in) == out) {
    result = scratch;
  }
  // All ones but the sign bit
  double signMask =
      SpecificNaN<double>(0, FloatingPoint<double>::kSignificandBits);
  static const SimdConstant signMasks = SimdConstant::SplatX2(signMask);
  asMasm().loadConstantSimd128Float(signMasks, result);
  vandpd(ToSimdFloatRegister(in), result, result);
  if (result == scratch) {
    moveSimd128Float(result, out);
  }
}

static inline void MaskSimdShiftCount(MacroAssembler& masm, unsigned shiftmask,
                                      Register count, Register temp,
                                      FloatRegister dest) {
  masm.mov(count, temp);
  masm.andl(Imm32(shiftmask), temp);
  masm.vmovd(temp, dest);
}

void MacroAssemblerX86Shared::packedShiftByScalarInt8x16(
    FloatRegister in, Register count, Register temp, FloatRegister xtmp,
    FloatRegister dest,
    void (MacroAssemblerX86Shared::*shift)(FloatRegister, FloatRegister,
                                           FloatRegister),
    void (MacroAssemblerX86Shared::*extend)(const Operand&, FloatRegister)) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 7, count, temp, scratch);

  // High bytes
  vpalignr(Operand(in), xtmp, 8);
  (this->*extend)(Operand(xtmp), xtmp);
  (this->*shift)(scratch, xtmp, xtmp);

  // Low bytes
  (this->*extend)(Operand(dest), dest);
  (this->*shift)(scratch, dest, dest);

  // Mask off garbage to avoid saturation during packing
  asMasm().loadConstantSimd128Int(SimdConstant::SplatX4(int32_t(0x00FF00FF)),
                                  scratch);
  vpand(Operand(scratch), xtmp, xtmp);
  vpand(Operand(scratch), dest, dest);

  vpackuswb(Operand(xtmp), dest, dest);
}

void MacroAssemblerX86Shared::packedLeftShiftByScalarInt8x16(
    FloatRegister in, Register count, Register temp, FloatRegister xtmp,
    FloatRegister dest) {
  packedShiftByScalarInt8x16(in, count, temp, xtmp, dest,
                             &MacroAssemblerX86Shared::vpsllw,
                             &MacroAssemblerX86Shared::vpmovzxbw);
}

void MacroAssemblerX86Shared::packedRightShiftByScalarInt8x16(
    FloatRegister in, Register count, Register temp, FloatRegister xtmp,
    FloatRegister dest) {
  packedShiftByScalarInt8x16(in, count, temp, xtmp, dest,
                             &MacroAssemblerX86Shared::vpsraw,
                             &MacroAssemblerX86Shared::vpmovsxbw);
}

void MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt8x16(
    FloatRegister in, Register count, Register temp, FloatRegister xtmp,
    FloatRegister dest) {
  packedShiftByScalarInt8x16(in, count, temp, xtmp, dest,
                             &MacroAssemblerX86Shared::vpsrlw,
                             &MacroAssemblerX86Shared::vpmovzxbw);
}

void MacroAssemblerX86Shared::packedLeftShiftByScalarInt16x8(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 15, count, temp, scratch);
  vpsllw(scratch, in, dest);
}

void MacroAssemblerX86Shared::packedRightShiftByScalarInt16x8(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 15, count, temp, scratch);
  vpsraw(scratch, in, dest);
}

void MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt16x8(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 15, count, temp, scratch);
  vpsrlw(scratch, in, dest);
}

void MacroAssemblerX86Shared::packedLeftShiftByScalarInt32x4(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 31, count, temp, scratch);
  vpslld(scratch, in, dest);
}

void MacroAssemblerX86Shared::packedRightShiftByScalarInt32x4(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 31, count, temp, scratch);
  vpsrad(scratch, in, dest);
}

void MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt32x4(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 31, count, temp, scratch);
  vpsrld(scratch, in, dest);
}

void MacroAssemblerX86Shared::packedLeftShiftByScalarInt64x2(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 63, count, temp, scratch);
  vpsllq(scratch, in, dest);
}

void MacroAssemblerX86Shared::packedRightShiftByScalarInt64x2(
    FloatRegister in, Register count, Register temp1, FloatRegister temp2,
    FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  movl(count, temp1);                   // temp1 is zero-extended shift count
  andl(Imm32(63), temp1);               // temp1 is masked shift count
  vmovd(temp1, scratch);                //   and scratch 64-bit ditto
  vpxor(Operand(temp2), temp2, temp2);  // temp2=0
  vpcmpgtq(Operand(in), temp2, temp2);  // temp2=~0 where `in` negative
  vpsrlq(scratch, in, dest);            // dest shifted, maybe wrong sign
  negl(temp1);                          // temp1 is - masked count
  addl(Imm32(63), temp1);               // temp1 is 63 - masked count
  vmovd(temp1, scratch);                //   and scratch ditto
  vpsllq(scratch, temp2, temp2);        // temp2 has the sign bits
  vpor(Operand(temp2), dest, dest);     // dest has right sign
}

void MacroAssemblerX86Shared::packedUnsignedRightShiftByScalarInt64x2(
    FloatRegister in, Register count, Register temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  MaskSimdShiftCount(asMasm(), 63, count, temp, scratch);
  vpsrlq(scratch, in, dest);
}

void MacroAssemblerX86Shared::selectSimd128(FloatRegister mask,
                                            FloatRegister onTrue,
                                            FloatRegister onFalse,
                                            FloatRegister temp,
                                            FloatRegister output) {
  if (onTrue != output) {
    vmovaps(onTrue, output);
  }
  if (mask != temp) {
    vmovaps(mask, temp);
  }

  // SSE4.1 has plain blendvps which can do this, but it is awkward
  // to use because it requires the mask to be in xmm0.

  bitwiseAndSimdInt(output, Operand(temp), output);
  bitwiseAndNotSimdInt(temp, Operand(onFalse), temp);
  bitwiseOrSimdInt(output, Operand(temp), output);
}

// Code sequences for int32x4<->float32x4 culled from v8; commentary added.

void MacroAssemblerX86Shared::unsignedConvertInt32x4ToFloat32x4(
    FloatRegister src, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  if (src != dest) {
    vmovaps(src, dest);
  }
  vpxor(Operand(scratch), scratch, scratch);  // extract low bits
  vpblendw(0x55, dest, scratch, scratch);     //   into scratch
  vpsubd(Operand(scratch), dest, dest);       //     and high bits into dest
  vcvtdq2ps(scratch, scratch);                // convert low bits
  vpsrld(Imm32(1), dest, dest);               // get high into unsigned range
  vcvtdq2ps(dest, dest);                      //   convert
  vaddps(Operand(dest), dest, dest);          //     and back into signed
  vaddps(Operand(scratch), dest, dest);       // combine high+low: may round
}

void MacroAssemblerX86Shared::truncSatFloat32x4ToInt32x4(FloatRegister src,
                                                         FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  if (src != dest) {
    vmovaps(src, dest);
  }

  // The cvttps2dq instruction is the workhorse but does not handle NaN or out
  // of range values as we need it to.  We want to saturate too-large positive
  // values to 7FFFFFFFh and too-large negative values to 80000000h.  NaN and -0
  // become 0.

  // Convert NaN to 0 by masking away values that compare unordered to itself.
  vmovaps(dest, scratch);
  vcmpeqps(Operand(scratch), scratch, scratch);
  vpand(Operand(scratch), dest, dest);

  // Compute the complement of each non-NaN lane's sign bit, we'll need this to
  // correct the result of cvttps2dq.  All other output bits are garbage.
  vpxor(Operand(dest), scratch, scratch);

  // Convert.  This will make the output 80000000h if the input is out of range.
  vcvttps2dq(dest, dest);

  // Preserve the computed complemented sign bit if the output was 80000000h.
  // The sign bit will be 1 precisely for nonnegative values that overflowed.
  vpand(Operand(dest), scratch, scratch);

  // Create a mask with that sign bit.  Now a lane is either FFFFFFFFh if there
  // was a positive overflow, otherwise zero.
  vpsrad(Imm32(31), scratch, scratch);

  // Convert overflow lanes to 0x7FFFFFFF.
  vpxor(Operand(scratch), dest, dest);
}

void MacroAssemblerX86Shared::unsignedTruncSatFloat32x4ToInt32x4(
    FloatRegister src, FloatRegister temp, FloatRegister dest) {
  ScratchSimd128Scope scratch(asMasm());
  if (src != dest) {
    vmovaps(src, dest);
  }

  // The cvttps2dq instruction is the workhorse but does not handle NaN or out
  // of range values as we need it to.  We want to saturate too-large positive
  // values to FFFFFFFFh and negative values to zero.  NaN and -0 become 0.

  // Convert NaN and negative values to zeroes in dest.
  vpxor(Operand(scratch), scratch, scratch);
  vmaxps(Operand(scratch), dest, dest);

  // Compute the float value of 0x7FFFFFFF (the largest positive signed integer
  // value) in all lanes in scratch.  We use it to bias the conversion to handle
  // edge cases.
  vpcmpeqd(Operand(scratch), scratch, scratch);
  vpsrld(Imm32(1), scratch, scratch);
  vcvtdq2ps(scratch, scratch);

  // temp = dest - 7FFFFFFFh (as floating), this brings integers in the unsigned
  // range but above the signed range into the signed range; 0 => -7FFFFFFFh.
  vmovaps(dest, temp);
  vsubps(Operand(scratch), temp, temp);

  // scratch = mask of biased values that are greater than 7FFFFFFFh.
  vcmpleps(Operand(temp), scratch, scratch);

  // Convert the biased values to integer.  Positive values above 7FFFFFFFh will
  // have been converted to 80000000h, all others become the expected integer.
  vcvttps2dq(temp, temp);

  // As lanes of scratch are ~0 where the result overflows, this computes
  // 7FFFFFFF in lanes of temp that are 80000000h, and leaves other lanes
  // untouched as the biased integer.
  vpxor(Operand(scratch), temp, temp);

  // Convert negative biased lanes in temp to zero.  After this, temp will be
  // zero where the result should be zero or is less than 80000000h, 7FFFFFFF
  // where the result overflows, and will have the converted biased result in
  // other lanes (for input values >= 80000000h).
  vpxor(Operand(scratch), scratch, scratch);
  vpmaxsd(Operand(scratch), temp, temp);

  // Convert. Overflow lanes above 7FFFFFFFh will be 80000000h, other lanes will
  // be what they should be.
  vcvttps2dq(dest, dest);

  // Add temp to the result.  Overflow lanes with 80000000h becomes FFFFFFFFh,
  // biased high-value unsigned lanes become unbiased, everything else is left
  // unchanged.
  vpaddd(Operand(temp), dest, dest);
}
