/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x86/Assembler-x86.h"

#include "gc/Marking.h"

using namespace js;
using namespace js::jit;

ABIArgGenerator::ABIArgGenerator()
  : stackOffset_(0),
    current_()
{}

ABIArg
ABIArgGenerator::next(MIRType type)
{
    switch (type) {
      case MIRType_Int32:
      case MIRType_Pointer:
        current_ = ABIArg(stackOffset_);
        stackOffset_ += sizeof(uint32_t);
        break;
      case MIRType_Float32: // Float32 moves are actually double moves
      case MIRType_Double:
        current_ = ABIArg(stackOffset_);
        stackOffset_ += sizeof(uint64_t);
        break;
      case MIRType_Int32x4:
      case MIRType_Float32x4:
        // SIMD values aren't passed in or out of C++, so we can make up
        // whatever internal ABI we like. visitAsmJSPassArg assumes
        // SimdMemoryAlignment.
        stackOffset_ = AlignBytes(stackOffset_, SimdMemoryAlignment);
        current_ = ABIArg(stackOffset_);
        stackOffset_ += Simd128DataSize;
        break;
      default:
        MOZ_CRASH("Unexpected argument type");
    }
    return current_;
}

const Register ABIArgGenerator::NonArgReturnReg0 = ecx;
const Register ABIArgGenerator::NonArgReturnReg1 = edx;
const Register ABIArgGenerator::NonVolatileReg = ebx;
const Register ABIArgGenerator::NonArg_VolatileReg = eax;
const Register ABIArgGenerator::NonReturn_VolatileReg0 = ecx;

void
Assembler::executableCopy(uint8_t* buffer)
{
    AssemblerX86Shared::executableCopy(buffer);

    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch& rp = jumps_[i];
        X86Encoding::SetRel32(buffer + rp.offset, rp.target);
    }
}

class RelocationIterator
{
    CompactBufferReader reader_;
    uint32_t offset_;

  public:
    RelocationIterator(CompactBufferReader& reader)
      : reader_(reader)
    { }

    bool read() {
        if (!reader_.more())
            return false;
        offset_ = reader_.readUnsigned();
        return true;
    }

    uint32_t offset() const {
        return offset_;
    }
};

static inline JitCode*
CodeFromJump(uint8_t* jump)
{
    uint8_t* target = (uint8_t*)X86Encoding::GetRel32Target(jump);
    return JitCode::FromExecutable(target);
}

void
Assembler::TraceJumpRelocations(JSTracer* trc, JitCode* code, CompactBufferReader& reader)
{
    RelocationIterator iter(reader);
    while (iter.read()) {
        JitCode* child = CodeFromJump(code->raw() + iter.offset());
        MarkJitCodeUnbarriered(trc, &child, "rel32");
        MOZ_ASSERT(child == CodeFromJump(code->raw() + iter.offset()));
    }
}

FloatRegisterSet
FloatRegister::ReduceSetForPush(const FloatRegisterSet& s)
{
    SetType bits = s.bits();

    // Ignore all SIMD register, if not supported.
    if (!JitSupportsSimd())
        bits &= Codes::AllPhysMask * Codes::SpreadScalar;

    // Exclude registers which are already pushed with a larger type. High bits
    // are associated with larger register types. Thus we keep the set of
    // registers which are not included in larger type.
    bits &= ~(bits >> (1 * Codes::TotalPhys));
    bits &= ~(bits >> (2 * Codes::TotalPhys));
    bits &= ~(bits >> (3 * Codes::TotalPhys));

    return FloatRegisterSet(bits);
}

uint32_t
FloatRegister::GetPushSizeInBytes(const FloatRegisterSet& s)
{
    SetType all = s.bits();
    SetType float32x4Set =
        (all >> (uint32_t(Codes::Float32x4) * Codes::TotalPhys)) & Codes::AllPhysMask;
    SetType int32x4Set =
        (all >> (uint32_t(Codes::Int32x4) * Codes::TotalPhys)) & Codes::AllPhysMask;
    SetType doubleSet =
        (all >> (uint32_t(Codes::Double) * Codes::TotalPhys)) & Codes::AllPhysMask;
    SetType singleSet =
        (all >> (uint32_t(Codes::Single) * Codes::TotalPhys)) & Codes::AllPhysMask;

    // PushRegsInMask pushes the largest register first, and thus avoids pushing
    // aliased registers. So we have to filter out the physical registers which
    // are already pushed as part of larger registers.
    SetType set128b = int32x4Set | float32x4Set;
    SetType set64b = doubleSet & ~set128b;
    SetType set32b = singleSet & ~set64b  & ~set128b;

    static_assert(Codes::AllPhysMask <= 0xffff, "We can safely use CountPopulation32");
    return mozilla::CountPopulation32(set128b) * (4 * sizeof(int32_t))
        + mozilla::CountPopulation32(set64b) * sizeof(double)
        + mozilla::CountPopulation32(set32b) * sizeof(float);
}
uint32_t
FloatRegister::getRegisterDumpOffsetInBytes()
{
    return uint32_t(encoding()) * sizeof(FloatRegisters::RegisterContent);
}
