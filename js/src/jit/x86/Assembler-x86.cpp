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
    current_ = ABIArg(stackOffset_);
    switch (type) {
      case MIRType_Int32:
      case MIRType_Pointer:
        stackOffset_ += sizeof(uint32_t);
        break;
      case MIRType_Float32: // Float32 moves are actually double moves
      case MIRType_Double:
        stackOffset_ += sizeof(uint64_t);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected argument type");
    }
    return current_;
}

const Register ABIArgGenerator::NonArgReturnVolatileReg0 = ecx;
const Register ABIArgGenerator::NonArgReturnVolatileReg1 = edx;
const Register ABIArgGenerator::NonVolatileReg = ebx;

void
Assembler::executableCopy(uint8_t *buffer)
{
    AssemblerX86Shared::executableCopy(buffer);

    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        JSC::X86Assembler::setRel32(buffer + rp.offset, rp.target);
    }
}

class RelocationIterator
{
    CompactBufferReader reader_;
    uint32_t offset_;

  public:
    RelocationIterator(CompactBufferReader &reader)
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

static inline JitCode *
CodeFromJump(uint8_t *jump)
{
    uint8_t *target = (uint8_t *)JSC::X86Assembler::getRel32Target(jump);
    return JitCode::FromExecutable(target);
}

void
Assembler::TraceJumpRelocations(JSTracer *trc, JitCode *code, CompactBufferReader &reader)
{
    RelocationIterator iter(reader);
    while (iter.read()) {
        JitCode *child = CodeFromJump(code->raw() + iter.offset());
        MarkJitCodeUnbarriered(trc, &child, "rel32");
        JS_ASSERT(child == CodeFromJump(code->raw() + iter.offset()));
    }
}

uint32_t
FloatRegister::GetSizeInBytes(const FloatRegisterSet &s)
{
    uint32_t ret = s.size() * sizeof(double);
    return ret;
}

FloatRegisterSet
FloatRegister::ReduceSetForPush(const FloatRegisterSet &s)
{
    return s;
}
uint32_t
FloatRegister::GetPushSizeInBytes(const FloatRegisterSet &s)
{
    return s.size() * sizeof(double);
}
uint32_t
FloatRegister::getRegisterDumpOffsetInBytes()
{
    return code() * sizeof(double);
}
