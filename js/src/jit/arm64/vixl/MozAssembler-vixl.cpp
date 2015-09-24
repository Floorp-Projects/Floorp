// Copyright 2013, ARM Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "jsutil.h"

#include "jit/arm64/vixl/Assembler-vixl.h"
#include "jit/Label.h"

namespace vixl {


// Assembler
void Assembler::Reset() {
#ifdef DEBUG
    finalized_ = false;
#endif
    pc_ = nullptr;
}

void Assembler::FinalizeCode() {
#ifdef DEBUG
  finalized_ = true;
#endif
}


// A common implementation for the LinkAndGet<Type>OffsetTo helpers.
//
// If the label is bound, returns the offset as a multiple of element_size.
// Otherwise, links the instruction to the label and returns the offset to encode
// as a multiple of kInstructionSize.
//
// The offset is calculated by aligning the PC and label addresses down to a
// multiple of element_size, then calculating the (scaled) offset between them.
// This matches the semantics of adrp, for example.
template <int element_size>
ptrdiff_t Assembler::LinkAndGetOffsetTo(BufferOffset branch, Label* label) {
  if (armbuffer_.oom())
    return js::jit::LabelBase::INVALID_OFFSET;

  if (label->bound()) {
    // The label is bound: all uses are already linked.
    ptrdiff_t branch_offset = ptrdiff_t(branch.getOffset() / element_size);
    ptrdiff_t label_offset = ptrdiff_t(label->offset() / element_size);
    return label_offset - branch_offset;
  }

  if (!label->used()) {
    // The label is unbound and unused: store the offset in the label itself
    // for patching by bind().
    label->use(branch.getOffset());
    return js::jit::LabelBase::INVALID_OFFSET;
  }

  // The label is unbound but used. Create an implicit linked list between
  // the branches, and update the linked list head in the label struct.
  ptrdiff_t prevHeadOffset = static_cast<ptrdiff_t>(label->offset());
  label->use(branch.getOffset());
  VIXL_ASSERT(prevHeadOffset - branch.getOffset() != js::jit::LabelBase::INVALID_OFFSET);
  return prevHeadOffset - branch.getOffset();
}


ptrdiff_t Assembler::LinkAndGetByteOffsetTo(BufferOffset branch, Label* label) {
  return LinkAndGetOffsetTo<1>(branch, label);
}


ptrdiff_t Assembler::LinkAndGetInstructionOffsetTo(BufferOffset branch, Label* label) {
  return LinkAndGetOffsetTo<kInstructionSize>(branch, label);
}


ptrdiff_t Assembler::LinkAndGetPageOffsetTo(BufferOffset branch, Label* label) {
  return LinkAndGetOffsetTo<kPageSize>(branch, label);
}


BufferOffset Assembler::b(int imm26) {
  return EmitBranch(B | ImmUncondBranch(imm26));
}


void Assembler::b(Instruction* at, int imm26) {
  return EmitBranch(at, B | ImmUncondBranch(imm26));
}


BufferOffset Assembler::b(int imm19, Condition cond) {
  return EmitBranch(B_cond | ImmCondBranch(imm19) | cond);
}


void Assembler::b(Instruction* at, int imm19, Condition cond) {
  EmitBranch(at, B_cond | ImmCondBranch(imm19) | cond);
}


BufferOffset Assembler::b(Label* label) {
  // Encode the relative offset from the inserted branch to the label.
  return b(LinkAndGetInstructionOffsetTo(nextInstrOffset(), label));
}


BufferOffset Assembler::b(Label* label, Condition cond) {
  // Encode the relative offset from the inserted branch to the label.
  return b(LinkAndGetInstructionOffsetTo(nextInstrOffset(), label), cond);
}


void Assembler::bl(int imm26) {
  EmitBranch(BL | ImmUncondBranch(imm26));
}


void Assembler::bl(Instruction* at, int imm26) {
  EmitBranch(at, BL | ImmUncondBranch(imm26));
}


void Assembler::bl(Label* label) {
  // Encode the relative offset from the inserted branch to the label.
  return bl(LinkAndGetInstructionOffsetTo(nextInstrOffset(), label));
}


void Assembler::cbz(const Register& rt, int imm19) {
  EmitBranch(SF(rt) | CBZ | ImmCmpBranch(imm19) | Rt(rt));
}


void Assembler::cbz(Instruction* at, const Register& rt, int imm19) {
  EmitBranch(at, SF(rt) | CBZ | ImmCmpBranch(imm19) | Rt(rt));
}


void Assembler::cbz(const Register& rt, Label* label) {
  // Encode the relative offset from the inserted branch to the label.
  return cbz(rt, LinkAndGetInstructionOffsetTo(nextInstrOffset(), label));
}


void Assembler::cbnz(const Register& rt, int imm19) {
  EmitBranch(SF(rt) | CBNZ | ImmCmpBranch(imm19) | Rt(rt));
}


void Assembler::cbnz(Instruction* at, const Register& rt, int imm19) {
  EmitBranch(at, SF(rt) | CBNZ | ImmCmpBranch(imm19) | Rt(rt));
}


void Assembler::cbnz(const Register& rt, Label* label) {
  // Encode the relative offset from the inserted branch to the label.
  return cbnz(rt, LinkAndGetInstructionOffsetTo(nextInstrOffset(), label));
}


void Assembler::tbz(const Register& rt, unsigned bit_pos, int imm14) {
  VIXL_ASSERT(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSize)));
  EmitBranch(TBZ | ImmTestBranchBit(bit_pos) | ImmTestBranch(imm14) | Rt(rt));
}


void Assembler::tbz(Instruction* at, const Register& rt, unsigned bit_pos, int imm14) {
  VIXL_ASSERT(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSize)));
  EmitBranch(at, TBZ | ImmTestBranchBit(bit_pos) | ImmTestBranch(imm14) | Rt(rt));
}


void Assembler::tbz(const Register& rt, unsigned bit_pos, Label* label) {
  // Encode the relative offset from the inserted branch to the label.
  return tbz(rt, bit_pos, LinkAndGetInstructionOffsetTo(nextInstrOffset(), label));
}


void Assembler::tbnz(const Register& rt, unsigned bit_pos, int imm14) {
  VIXL_ASSERT(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSize)));
  EmitBranch(TBNZ | ImmTestBranchBit(bit_pos) | ImmTestBranch(imm14) | Rt(rt));
}


void Assembler::tbnz(Instruction* at, const Register& rt, unsigned bit_pos, int imm14) {
  VIXL_ASSERT(rt.Is64Bits() || (rt.Is32Bits() && (bit_pos < kWRegSize)));
  EmitBranch(at, TBNZ | ImmTestBranchBit(bit_pos) | ImmTestBranch(imm14) | Rt(rt));
}


void Assembler::tbnz(const Register& rt, unsigned bit_pos, Label* label) {
  // Encode the relative offset from the inserted branch to the label.
  return tbnz(rt, bit_pos, LinkAndGetInstructionOffsetTo(nextInstrOffset(), label));
}


void Assembler::adr(const Register& rd, int imm21) {
  VIXL_ASSERT(rd.Is64Bits());
  EmitBranch(ADR | ImmPCRelAddress(imm21) | Rd(rd));
}


void Assembler::adr(Instruction* at, const Register& rd, int imm21) {
  VIXL_ASSERT(rd.Is64Bits());
  EmitBranch(at, ADR | ImmPCRelAddress(imm21) | Rd(rd));
}


void Assembler::adr(const Register& rd, Label* label) {
  // Encode the relative offset from the inserted adr to the label.
  return adr(rd, LinkAndGetByteOffsetTo(nextInstrOffset(), label));
}


void Assembler::adrp(const Register& rd, int imm21) {
  VIXL_ASSERT(rd.Is64Bits());
  EmitBranch(ADRP | ImmPCRelAddress(imm21) | Rd(rd));
}


void Assembler::adrp(Instruction* at, const Register& rd, int imm21) {
  VIXL_ASSERT(rd.Is64Bits());
  EmitBranch(at, ADRP | ImmPCRelAddress(imm21) | Rd(rd));
}


void Assembler::adrp(const Register& rd, Label* label) {
  VIXL_ASSERT(AllowPageOffsetDependentCode());
  // Encode the relative offset from the inserted adr to the label.
  return adrp(rd, LinkAndGetPageOffsetTo(nextInstrOffset(), label));
}


BufferOffset Assembler::ands(const Register& rd, const Register& rn, const Operand& operand) {
  return Logical(rd, rn, operand, ANDS);
}


BufferOffset Assembler::tst(const Register& rn, const Operand& operand) {
  return ands(AppropriateZeroRegFor(rn), rn, operand);
}


void Assembler::ldr(Instruction* at, const CPURegister& rt, int imm19) {
  LoadLiteralOp op = LoadLiteralOpFor(rt);
  Emit(at, op | ImmLLiteral(imm19) | Rt(rt));
}


BufferOffset Assembler::hint(SystemHint code) {
  return Emit(HINT | ImmHint(code) | Rt(xzr));
}


void Assembler::hint(Instruction* at, SystemHint code) {
  Emit(at, HINT | ImmHint(code) | Rt(xzr));
}


void Assembler::svc(Instruction* at, int code) {
  VIXL_ASSERT(is_uint16(code));
  Emit(at, SVC | ImmException(code));
}


void Assembler::nop(Instruction* at) {
  hint(at, NOP);
}


BufferOffset Assembler::Logical(const Register& rd, const Register& rn,
                                const Operand& operand, LogicalOp op)
{
  VIXL_ASSERT(rd.size() == rn.size());
  if (operand.IsImmediate()) {
    int64_t immediate = operand.immediate();
    unsigned reg_size = rd.size();

    VIXL_ASSERT(immediate != 0);
    VIXL_ASSERT(immediate != -1);
    VIXL_ASSERT(rd.Is64Bits() || is_uint32(immediate));

    // If the operation is NOT, invert the operation and immediate.
    if ((op & NOT) == NOT) {
      op = static_cast<LogicalOp>(op & ~NOT);
      immediate = rd.Is64Bits() ? ~immediate : (~immediate & kWRegMask);
    }

    unsigned n, imm_s, imm_r;
    if (IsImmLogical(immediate, reg_size, &n, &imm_s, &imm_r)) {
      // Immediate can be encoded in the instruction.
      return LogicalImmediate(rd, rn, n, imm_s, imm_r, op);
    } else {
      // This case is handled in the macro assembler.
      VIXL_UNREACHABLE();
    }
  } else {
    VIXL_ASSERT(operand.IsShiftedRegister());
    VIXL_ASSERT(operand.reg().size() == rd.size());
    Instr dp_op = static_cast<Instr>(op | LogicalShiftedFixed);
    return DataProcShiftedRegister(rd, rn, operand, LeaveFlags, dp_op);
  }
}


BufferOffset Assembler::LogicalImmediate(const Register& rd, const Register& rn,
                                         unsigned n, unsigned imm_s, unsigned imm_r, LogicalOp op)
{
    unsigned reg_size = rd.size();
    Instr dest_reg = (op == ANDS) ? Rd(rd) : RdSP(rd);
    return Emit(SF(rd) | LogicalImmediateFixed | op | BitN(n, reg_size) |
                ImmSetBits(imm_s, reg_size) | ImmRotate(imm_r, reg_size) | dest_reg | Rn(rn));
}


BufferOffset Assembler::DataProcShiftedRegister(const Register& rd, const Register& rn,
                                                const Operand& operand, FlagsUpdate S, Instr op)
{
  VIXL_ASSERT(operand.IsShiftedRegister());
  VIXL_ASSERT(rn.Is64Bits() || (rn.Is32Bits() && is_uint5(operand.shift_amount())));
  return Emit(SF(rd) | op | Flags(S) |
              ShiftDP(operand.shift()) | ImmDPShift(operand.shift_amount()) |
              Rm(operand.reg()) | Rn(rn) | Rd(rd));
}


void MozBaseAssembler::InsertIndexIntoTag(uint8_t* load, uint32_t index) {
  // Store the js::jit::PoolEntry index into the instruction.
  // finishPool() will walk over all literal load instructions
  // and use PatchConstantPoolLoad() to patch to the final relative offset.
  *((uint32_t*)load) |= Assembler::ImmLLiteral(index);
}


bool MozBaseAssembler::PatchConstantPoolLoad(void* loadAddr, void* constPoolAddr) {
  Instruction* load = reinterpret_cast<Instruction*>(loadAddr);

  // The load currently contains the js::jit::PoolEntry's index,
  // as written by InsertIndexIntoTag().
  uint32_t index = load->ImmLLiteral();

  // Each entry in the literal pool is uint32_t-sized,
  // but literals may use multiple entries.
  uint32_t* constPool = reinterpret_cast<uint32_t*>(constPoolAddr);
  Instruction* source = reinterpret_cast<Instruction*>(&constPool[index]);

  load->SetImmLLiteral(source);
  return false; // Nothing uses the return value.
}


uint32_t MozBaseAssembler::PlaceConstantPoolBarrier(int offset) {
  MOZ_CRASH("PlaceConstantPoolBarrier");
}


struct PoolHeader {
  uint32_t data;

  struct Header {
    // The size should take into account the pool header.
    // The size is in units of Instruction (4bytes), not byte.
    union {
      struct {
        uint32_t size : 15;

	// "Natural" guards are part of the normal instruction stream,
	// while "non-natural" guards are inserted for the sole purpose
	// of skipping around a pool.
        bool isNatural : 1;
        uint32_t ONES : 16;
      };
      uint32_t data;
    };

    Header(int size_, bool isNatural_)
      : size(size_),
        isNatural(isNatural_),
        ONES(0xffff)
    { }

    Header(uint32_t data)
      : data(data)
    {
      JS_STATIC_ASSERT(sizeof(Header) == sizeof(uint32_t));
      VIXL_ASSERT(ONES == 0xffff);
    }

    uint32_t raw() const {
      JS_STATIC_ASSERT(sizeof(Header) == sizeof(uint32_t));
      return data;
    }
  };

  PoolHeader(int size_, bool isNatural_)
    : data(Header(size_, isNatural_).raw())
  { }

  uint32_t size() const {
    Header tmp(data);
    return tmp.size;
  }

  uint32_t isNatural() const {
    Header tmp(data);
    return tmp.isNatural;
  }
};


void MozBaseAssembler::WritePoolHeader(uint8_t* start, js::jit::Pool* p, bool isNatural) {
  JS_STATIC_ASSERT(sizeof(PoolHeader) == 4);

  // Get the total size of the pool.
  const uintptr_t totalPoolSize = sizeof(PoolHeader) + p->getPoolSize();
  const uintptr_t totalPoolInstructions = totalPoolSize / sizeof(Instruction);

  VIXL_ASSERT((totalPoolSize & 0x3) == 0);
  VIXL_ASSERT(totalPoolInstructions < (1 << 15));

  PoolHeader header(totalPoolInstructions, isNatural);
  *(PoolHeader*)start = header;
}


void MozBaseAssembler::WritePoolFooter(uint8_t* start, js::jit::Pool* p, bool isNatural) {
  return;
}


void MozBaseAssembler::WritePoolGuard(BufferOffset branch, Instruction* inst, BufferOffset dest) {
  int byteOffset = dest.getOffset() - branch.getOffset();
  VIXL_ASSERT(byteOffset % kInstructionSize == 0);

  int instOffset = byteOffset >> kInstructionSizeLog2;
  Assembler::b(inst, instOffset);
}


ptrdiff_t MozBaseAssembler::GetBranchOffset(const Instruction* ins) {
  // Branch instructions use an instruction offset.
  if (ins->BranchType() != UnknownBranchType)
    return ins->ImmPCRawOffset() * kInstructionSize;

  // ADR and ADRP encode relative offsets and therefore require patching as if they were branches.
  // ADR uses a byte offset.
  if (ins->IsADR())
    return ins->ImmPCRawOffset();

  // ADRP uses a page offset.
  if (ins->IsADRP())
    return ins->ImmPCRawOffset() * kPageSize;

  MOZ_CRASH("Unsupported branch type");
}


void MozBaseAssembler::RetargetNearBranch(Instruction* i, int offset, Condition cond, bool final) {
  if (i->IsCondBranchImm()) {
    VIXL_ASSERT(i->IsCondB());
    Assembler::b(i, offset, cond);
    return;
  }
  MOZ_CRASH("Unsupported branch type");
}


void MozBaseAssembler::RetargetNearBranch(Instruction* i, int byteOffset, bool final) {
  const int instOffset = byteOffset >> kInstructionSizeLog2;

  // The only valid conditional instruction is B.
  if (i->IsCondBranchImm()) {
    VIXL_ASSERT(byteOffset % kInstructionSize == 0);
    VIXL_ASSERT(i->IsCondB());
    Condition cond = static_cast<Condition>(i->ConditionBranch());
    Assembler::b(i, instOffset, cond);
    return;
  }

  // Valid unconditional branches are B and BL.
  if (i->IsUncondBranchImm()) {
    VIXL_ASSERT(byteOffset % kInstructionSize == 0);
    if (i->IsUncondB()) {
      Assembler::b(i, instOffset);
    } else {
      VIXL_ASSERT(i->IsBL());
      Assembler::bl(i, instOffset);
    }

    VIXL_ASSERT(i->ImmUncondBranch() == instOffset);
    return;
  }

  // Valid compare branches are CBZ and CBNZ.
  if (i->IsCompareBranch()) {
    VIXL_ASSERT(byteOffset % kInstructionSize == 0);
    Register rt = i->SixtyFourBits() ? Register::XRegFromCode(i->Rt())
                                     : Register::WRegFromCode(i->Rt());

    if (i->IsCBZ()) {
      Assembler::cbz(i, rt, instOffset);
    } else {
      VIXL_ASSERT(i->IsCBNZ());
      Assembler::cbnz(i, rt, instOffset);
    }

    VIXL_ASSERT(i->ImmCmpBranch() == instOffset);
    return;
  }

  // Valid test branches are TBZ and TBNZ.
  if (i->IsTestBranch()) {
    VIXL_ASSERT(byteOffset % kInstructionSize == 0);
    // Opposite of ImmTestBranchBit(): MSB in bit 5, 0:5 at bit 40.
    unsigned bit_pos = (i->ImmTestBranchBit5() << 5) | (i->ImmTestBranchBit40());
    VIXL_ASSERT(is_uint6(bit_pos));

    // Register size doesn't matter for the encoding.
    Register rt = Register::XRegFromCode(i->Rt());

    if (i->IsTBZ()) {
      Assembler::tbz(i, rt, bit_pos, instOffset);
    } else {
      VIXL_ASSERT(i->IsTBNZ());
      Assembler::tbnz(i, rt, bit_pos, instOffset);
    }

    VIXL_ASSERT(i->ImmTestBranch() == instOffset);
    return;
  }

  if (i->IsADR()) {
    Register rd = Register::XRegFromCode(i->Rd());
    Assembler::adr(i, rd, byteOffset);
    return;
  }

  if (i->IsADRP()) {
    const int pageOffset = byteOffset >> kPageSizeLog2;
    Register rd = Register::XRegFromCode(i->Rd());
    Assembler::adrp(i, rd, pageOffset);
    return;
  }

  MOZ_CRASH("Unsupported branch type");
}


void MozBaseAssembler::RetargetFarBranch(Instruction* i, uint8_t** slot, uint8_t* dest, Condition cond) {
  MOZ_CRASH("RetargetFarBranch()");
}


}  // namespace vixl

