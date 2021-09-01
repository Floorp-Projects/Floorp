/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmBCMemory.h"

#include "wasm/WasmBCClass.h"
#include "wasm/WasmBCDefs.h"
#include "wasm/WasmBCRegDefs.h"

#include "jit/MacroAssembler-inl.h"

#include "wasm/WasmBCClass-inl.h"
#include "wasm/WasmBCCodegen-inl.h"
#include "wasm/WasmBCRegDefs-inl.h"
#include "wasm/WasmBCRegMgmt-inl.h"
#include "wasm/WasmBCStkMgmt-inl.h"

namespace js {
namespace wasm {

template <size_t Count>
void Atomic32Temps<Count>::allocate(BaseCompiler* bc, size_t allocate) {
  static_assert(Count != 0);
  for (size_t i = 0; i < allocate; ++i) {
    this->operator[](i) = bc->needI32();
  }
}

template <size_t Count>
void Atomic32Temps<Count>::maybeFree(BaseCompiler* bc) {
  for (size_t i = 0; i < Count; ++i) {
    bc->maybeFree(this->operator[](i));
  }
}

// Bounds check elimination.
//
// We perform BCE on two kinds of address expressions: on constant heap pointers
// that are known to be in the heap or will be handled by the out-of-bounds trap
// handler; and on local variables that have been checked in dominating code
// without being updated since.
//
// For an access through a constant heap pointer + an offset we can eliminate
// the bounds check if the sum of the address and offset is below the sum of the
// minimum memory length and the offset guard length.
//
// For an access through a local variable + an offset we can eliminate the
// bounds check if the local variable has already been checked and has not been
// updated since, and the offset is less than the guard limit.
//
// To track locals for which we can eliminate checks we use a bit vector
// bceSafe_ that has a bit set for those locals whose bounds have been checked
// and which have not subsequently been set.  Initially this vector is zero.
//
// In straight-line code a bit is set when we perform a bounds check on an
// access via the local and is reset when the variable is updated.
//
// In control flow, the bit vector is manipulated as follows.  Each ControlItem
// has a value bceSafeOnEntry, which is the value of bceSafe_ on entry to the
// item, and a value bceSafeOnExit, which is initially ~0.  On a branch (br,
// brIf, brTable), we always AND the branch target's bceSafeOnExit with the
// value of bceSafe_ at the branch point.  On exiting an item by falling out of
// it, provided we're not in dead code, we AND the current value of bceSafe_
// into the item's bceSafeOnExit.  Additional processing depends on the item
// type:
//
//  - After a block, set bceSafe_ to the block's bceSafeOnExit.
//
//  - On loop entry, after pushing the ControlItem, set bceSafe_ to zero; the
//    back edges would otherwise require us to iterate to a fixedpoint.
//
//  - After a loop, the bceSafe_ is left unchanged, because only fallthrough
//    control flow will reach that point and the bceSafe_ value represents the
//    correct state of the fallthrough path.
//
//  - Set bceSafe_ to the ControlItem's bceSafeOnEntry at both the 'then' branch
//    and the 'else' branch.
//
//  - After an if-then-else, set bceSafe_ to the if-then-else's bceSafeOnExit.
//
//  - After an if-then, set bceSafe_ to the if-then's bceSafeOnExit AND'ed with
//    the if-then's bceSafeOnEntry.
//
// Finally, when the debugger allows locals to be mutated we must disable BCE
// for references via a local, by returning immediately from bceCheckLocal if
// compilerEnv_.debugEnabled() is true.
//
//
// Alignment check elimination.
//
// Alignment checks for atomic operations can be omitted if the pointer is a
// constant and the pointer + offset is aligned.  Alignment checking that can't
// be omitted can still be simplified by checking only the pointer if the offset
// is aligned.
//
// (In addition, alignment checking of the pointer can be omitted if the pointer
// has been checked in dominating code, but we don't do that yet.)

void BaseCompiler::bceCheckLocal(MemoryAccessDesc* access, AccessCheck* check,
                                 uint32_t local) {
  if (local >= sizeof(BCESet) * 8) {
    return;
  }

  uint32_t offsetGuardLimit =
      GetMaxOffsetGuardLimit(moduleEnv_.hugeMemoryEnabled());

  if ((bceSafe_ & (BCESet(1) << local)) &&
      access->offset() < offsetGuardLimit) {
    check->omitBoundsCheck = true;
  }

  // The local becomes safe even if the offset is beyond the guard limit.
  bceSafe_ |= (BCESet(1) << local);
}

void BaseCompiler::bceLocalIsUpdated(uint32_t local) {
  if (local >= sizeof(BCESet) * 8) {
    return;
  }

  bceSafe_ &= ~(BCESet(1) << local);
}

// TODO / OPTIMIZE (bug 1329576): There are opportunities to generate better
// code by not moving a constant address with a zero offset into a register.

RegI32 BaseCompiler::popMemory32Access(MemoryAccessDesc* access,
                                       AccessCheck* check) {
  check->onlyPointerAlignment =
      (access->offset() & (access->byteSize() - 1)) == 0;

  int32_t addrTemp;
  if (popConst(&addrTemp)) {
    uint32_t addr = addrTemp;

    uint32_t offsetGuardLimit =
        GetMaxOffsetGuardLimit(moduleEnv_.hugeMemoryEnabled());

    uint64_t ea = uint64_t(addr) + uint64_t(access->offset());
    uint64_t limit = moduleEnv_.memory->initialLength32() + offsetGuardLimit;

    check->omitBoundsCheck = ea < limit;
    check->omitAlignmentCheck = (ea & (access->byteSize() - 1)) == 0;

    // Fold the offset into the pointer if we can, as this is always
    // beneficial.

    if (ea <= UINT32_MAX) {
      addr = uint32_t(ea);
      access->clearOffset();
    }

    RegI32 r = needI32();
    moveImm32(int32_t(addr), r);
    return r;
  }

  uint32_t local;
  if (peekLocalI32(&local)) {
    bceCheckLocal(access, check, local);
  }

  return popI32();
}

void BaseCompiler::pushHeapBase() {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS64)
  RegI64 heapBase = needI64();
  moveI64(RegI64(Register64(HeapReg)), heapBase);
  pushI64(heapBase);
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32)
  RegI32 heapBase = needI32();
  moveI32(RegI32(HeapReg), heapBase);
  pushI32(heapBase);
#elif defined(JS_CODEGEN_X86)
  RegI32 heapBase = needI32();
  fr.loadTlsPtr(heapBase);
  masm.loadPtr(Address(heapBase, offsetof(TlsData, memoryBase)), heapBase);
  pushI32(heapBase);
#else
  MOZ_CRASH("BaseCompiler platform hook: pushHeapBase");
#endif
}

void BaseCompiler::prepareMemoryAccess(MemoryAccessDesc* access,
                                       AccessCheck* check, RegI32 tls,
                                       RegI32 ptr) {
  uint32_t offsetGuardLimit =
      GetMaxOffsetGuardLimit(moduleEnv_.hugeMemoryEnabled());

  // Fold offset if necessary for further computations.
  if (access->offset() >= offsetGuardLimit ||
      (access->isAtomic() && !check->omitAlignmentCheck &&
       !check->onlyPointerAlignment)) {
    Label ok;
    masm.branchAdd32(Assembler::CarryClear, Imm32(access->offset()), ptr, &ok);
    masm.wasmTrap(Trap::OutOfBounds, bytecodeOffset());
    masm.bind(&ok);
    access->clearOffset();
    check->onlyPointerAlignment = true;
  }

  // Alignment check if required.

  if (access->isAtomic() && !check->omitAlignmentCheck) {
    MOZ_ASSERT(check->onlyPointerAlignment);
    // We only care about the low pointer bits here.
    Label ok;
    masm.branchTest32(Assembler::Zero, ptr, Imm32(access->byteSize() - 1), &ok);
    masm.wasmTrap(Trap::UnalignedAccess, bytecodeOffset());
    masm.bind(&ok);
  }

  // Ensure no tls if we don't need it.

  if (moduleEnv_.hugeMemoryEnabled()) {
    // We have HeapReg and no bounds checking and need load neither
    // memoryBase nor boundsCheckLimit from tls.
    MOZ_ASSERT_IF(check->omitBoundsCheck, tls.isInvalid());
  }
#ifdef JS_CODEGEN_ARM
  // We have HeapReg on ARM and don't need to load the memoryBase from tls.
  MOZ_ASSERT_IF(check->omitBoundsCheck, tls.isInvalid());
#endif

  // Bounds check if required.

  if (!moduleEnv_.hugeMemoryEnabled() && !check->omitBoundsCheck) {
    Label ok;
#ifdef JS_64BIT
    // If the bounds check uses the full 64 bits of the bounds check limit,
    // then the index must be zero-extended to 64 bits before checking and
    // wrapped back to 32-bits after Spectre masking.  (And it's important
    // that the value we end up with has flowed through the Spectre mask.)
    //
    // If the memory's max size is known to be smaller than 64K pages exactly,
    // we can use a 32-bit check and avoid extension and wrapping.
    if (!moduleEnv_.memory->boundsCheckLimitIs32Bits() &&
        ArrayBufferObject::maxBufferByteLength() >= 0x100000000) {
      // Note, ptr and ptr64 are the same register.
      RegI64 ptr64 = fromI32(ptr);

      // In principle there may be non-zero bits in the upper bits of the
      // register; clear them.
#  if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM64)
      // The canonical value is zero-extended (see comment block "64-bit GPRs
      // carrying 32-bit values" in MacroAssembler.h); we already have that.
      masm.assertCanonicalInt32(ptr);
#  else
      MOZ_CRASH("Platform code needed here");
#  endif

      // Any Spectre mitigation will appear to update the ptr64 register.
      masm.wasmBoundsCheck64(Assembler::Below, ptr64,
                             Address(tls, offsetof(TlsData, boundsCheckLimit)),
                             &ok);

      // Restore the value to the canonical form for a 32-bit value in a
      // 64-bit register and/or the appropriate form for further use in the
      // indexing instruction.
#  if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM64)
      // The canonical value is zero-extended; we already have that.
#  else
      MOZ_CRASH("Platform code needed here");
#  endif
    } else {
      masm.wasmBoundsCheck32(Assembler::Below, ptr,
                             Address(tls, offsetof(TlsData, boundsCheckLimit)),
                             &ok);
    }
#else
    masm.wasmBoundsCheck32(Assembler::Below, ptr,
                           Address(tls, offsetof(TlsData, boundsCheckLimit)),
                           &ok);
#endif
    masm.wasmTrap(Trap::OutOfBounds, bytecodeOffset());
    masm.bind(&ok);
  }
}

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM) ||      \
    defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_MIPS32) || \
    defined(JS_CODEGEN_MIPS64)
BaseIndex BaseCompiler::prepareAtomicMemoryAccess(MemoryAccessDesc* access,
                                                  AccessCheck* check,
                                                  RegI32 tls, RegI32 ptr) {
  MOZ_ASSERT(needTlsForAccess(*check) == tls.isValid());
  prepareMemoryAccess(access, check, tls, ptr);
  return BaseIndex(HeapReg, ptr, TimesOne, access->offset());
}
#elif defined(JS_CODEGEN_X86)
// Some consumers depend on the address not retaining tls, as tls may be the
// scratch register.

Address BaseCompiler::prepareAtomicMemoryAccess(MemoryAccessDesc* access,
                                                AccessCheck* check, RegI32 tls,
                                                RegI32 ptr) {
  MOZ_ASSERT(needTlsForAccess(*check) == tls.isValid());
  prepareMemoryAccess(access, check, tls, ptr);
  masm.addPtr(Address(tls, offsetof(TlsData, memoryBase)), ptr);
  return Address(ptr, access->offset());
}
#else
Address BaseCompiler::prepareAtomicMemoryAccess(MemoryAccessDesc* access,
                                                AccessCheck* check, RegI32 tls,
                                                RegI32 ptr) {
  MOZ_CRASH("BaseCompiler platform hook: prepareAtomicMemoryAccess");
}
#endif

void BaseCompiler::computeEffectiveAddress(MemoryAccessDesc* access) {
  if (access->offset()) {
    Label ok;
    RegI32 ptr = popI32();
    masm.branchAdd32(Assembler::CarryClear, Imm32(access->offset()), ptr, &ok);
    masm.wasmTrap(Trap::OutOfBounds, bytecodeOffset());
    masm.bind(&ok);
    access->clearOffset();
    pushI32(ptr);
  }
}

[[nodiscard]] bool BaseCompiler::needTlsForAccess(const AccessCheck& check) {
#if defined(JS_CODEGEN_X86)
  // x86 requires Tls for memory base
  return true;
#else
  return !moduleEnv_.hugeMemoryEnabled() && !check.omitBoundsCheck;
#endif
}

// ptr and dest may be the same iff dest is I32.
// This may destroy ptr even if ptr and dest are not the same.
[[nodiscard]] bool BaseCompiler::load(MemoryAccessDesc* access,
                                      AccessCheck* check, RegI32 tls,
                                      RegI32 ptr, AnyReg dest, RegI32 temp) {
  prepareMemoryAccess(access, check, tls, ptr);

#if defined(JS_CODEGEN_X64)
  MOZ_ASSERT(temp.isInvalid());
  Operand srcAddr(HeapReg, ptr, TimesOne, access->offset());

  if (dest.tag == AnyReg::I64) {
    masm.wasmLoadI64(*access, srcAddr, dest.i64());
  } else {
    masm.wasmLoad(*access, srcAddr, dest.any());
  }
#elif defined(JS_CODEGEN_X86)
  MOZ_ASSERT(temp.isInvalid());
  masm.addPtr(Address(tls, offsetof(TlsData, memoryBase)), ptr);
  Operand srcAddr(ptr, access->offset());

  if (dest.tag == AnyReg::I64) {
    MOZ_ASSERT(dest.i64() == specific_.abiReturnRegI64);
    masm.wasmLoadI64(*access, srcAddr, dest.i64());
  } else {
    // For 8 bit loads, this will generate movsbl or movzbl, so
    // there's no constraint on what the output register may be.
    masm.wasmLoad(*access, srcAddr, dest.any());
  }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  if (IsUnaligned(*access)) {
    switch (dest.tag) {
      case AnyReg::I64:
        masm.wasmUnalignedLoadI64(*access, HeapReg, ptr, ptr, dest.i64(), temp);
        break;
      case AnyReg::F32:
        masm.wasmUnalignedLoadFP(*access, HeapReg, ptr, ptr, dest.f32(), temp);
        break;
      case AnyReg::F64:
        masm.wasmUnalignedLoadFP(*access, HeapReg, ptr, ptr, dest.f64(), temp);
        break;
      case AnyReg::I32:
        masm.wasmUnalignedLoad(*access, HeapReg, ptr, ptr, dest.i32(), temp);
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }
  } else {
    if (dest.tag == AnyReg::I64) {
      masm.wasmLoadI64(*access, HeapReg, ptr, ptr, dest.i64());
    } else {
      masm.wasmLoad(*access, HeapReg, ptr, ptr, dest.any());
    }
  }
#elif defined(JS_CODEGEN_ARM)
  MOZ_ASSERT(temp.isInvalid());
  if (dest.tag == AnyReg::I64) {
    masm.wasmLoadI64(*access, HeapReg, ptr, ptr, dest.i64());
  } else {
    masm.wasmLoad(*access, HeapReg, ptr, ptr, dest.any());
  }
#elif defined(JS_CODEGEN_ARM64)
  MOZ_ASSERT(temp.isInvalid());
  if (dest.tag == AnyReg::I64) {
    masm.wasmLoadI64(*access, HeapReg, ptr, dest.i64());
  } else {
    masm.wasmLoad(*access, HeapReg, ptr, dest.any());
  }
#else
  MOZ_CRASH("BaseCompiler platform hook: load");
#endif

  return true;
}

// ptr and src must not be the same register.
// This may destroy ptr and src.
[[nodiscard]] bool BaseCompiler::store(MemoryAccessDesc* access,
                                       AccessCheck* check, RegI32 tls,
                                       RegI32 ptr, AnyReg src, RegI32 temp) {
  prepareMemoryAccess(access, check, tls, ptr);

  // Emit the store
#if defined(JS_CODEGEN_X64)
  MOZ_ASSERT(temp.isInvalid());
  Operand dstAddr(HeapReg, ptr, TimesOne, access->offset());

  masm.wasmStore(*access, src.any(), dstAddr);
#elif defined(JS_CODEGEN_X86)
  MOZ_ASSERT(temp.isInvalid());
  masm.addPtr(Address(tls, offsetof(TlsData, memoryBase)), ptr);
  Operand dstAddr(ptr, access->offset());

  if (access->type() == Scalar::Int64) {
    masm.wasmStoreI64(*access, src.i64(), dstAddr);
  } else {
    AnyRegister value;
    ScratchI8 scratch(*this);
    if (src.tag == AnyReg::I64) {
      if (access->byteSize() == 1 && !ra.isSingleByteI32(src.i64().low)) {
        masm.mov(src.i64().low, scratch);
        value = AnyRegister(scratch);
      } else {
        value = AnyRegister(src.i64().low);
      }
    } else if (access->byteSize() == 1 && !ra.isSingleByteI32(src.i32())) {
      masm.mov(src.i32(), scratch);
      value = AnyRegister(scratch);
    } else {
      value = src.any();
    }

    masm.wasmStore(*access, value, dstAddr);
  }
#elif defined(JS_CODEGEN_ARM)
  MOZ_ASSERT(temp.isInvalid());
  if (access->type() == Scalar::Int64) {
    masm.wasmStoreI64(*access, src.i64(), HeapReg, ptr, ptr);
  } else if (src.tag == AnyReg::I64) {
    masm.wasmStore(*access, AnyRegister(src.i64().low), HeapReg, ptr, ptr);
  } else {
    masm.wasmStore(*access, src.any(), HeapReg, ptr, ptr);
  }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  if (IsUnaligned(*access)) {
    switch (src.tag) {
      case AnyReg::I64:
        masm.wasmUnalignedStoreI64(*access, src.i64(), HeapReg, ptr, ptr, temp);
        break;
      case AnyReg::F32:
        masm.wasmUnalignedStoreFP(*access, src.f32(), HeapReg, ptr, ptr, temp);
        break;
      case AnyReg::F64:
        masm.wasmUnalignedStoreFP(*access, src.f64(), HeapReg, ptr, ptr, temp);
        break;
      case AnyReg::I32:
        masm.wasmUnalignedStore(*access, src.i32(), HeapReg, ptr, ptr, temp);
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }
  } else {
    if (src.tag == AnyReg::I64) {
      masm.wasmStoreI64(*access, src.i64(), HeapReg, ptr, ptr);
    } else {
      masm.wasmStore(*access, src.any(), HeapReg, ptr, ptr);
    }
  }
#elif defined(JS_CODEGEN_ARM64)
  MOZ_ASSERT(temp.isInvalid());
  if (access->type() == Scalar::Int64) {
    masm.wasmStoreI64(*access, src.i64(), HeapReg, ptr);
  } else {
    masm.wasmStore(*access, src.any(), HeapReg, ptr);
  }
#else
  MOZ_CRASH("BaseCompiler platform hook: store");
#endif

  return true;
}

template <typename T>
void BaseCompiler::atomicRMW32(const MemoryAccessDesc& access, T srcAddr,
                               AtomicOp op, RegI32 rv, RegI32 rd,
                               const AtomicRMW32Temps& temps) {
  switch (access.type()) {
    case Scalar::Uint8:
#ifdef JS_CODEGEN_X86
    {
      RegI32 temp = temps[0];
      // The temp, if used, must be a byte register.
      MOZ_ASSERT(temp.isInvalid());
      ScratchI8 scratch(*this);
      if (op != AtomicFetchAddOp && op != AtomicFetchSubOp) {
        temp = scratch;
      }
      masm.wasmAtomicFetchOp(access, op, rv, srcAddr, temp, rd);
      break;
    }
#endif
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
      masm.wasmAtomicFetchOp(access, op, rv, srcAddr, temps[0], temps[1],
                             temps[2], rd);
#else
      masm.wasmAtomicFetchOp(access, op, rv, srcAddr, temps[0], rd);
#endif
      break;
    default: {
      MOZ_CRASH("Bad type for atomic operation");
    }
  }
}

// On x86, V is Address.  On other platforms, it is Register64.
// T is BaseIndex or Address.
template <typename T, typename V>
void BaseCompiler::atomicRMW64(const MemoryAccessDesc& access, const T& srcAddr,
                               AtomicOp op, V value, Register64 temp,
                               Register64 rd) {
  masm.wasmAtomicFetchOp64(access, op, value, srcAddr, temp, rd);
}

template <typename T>
void BaseCompiler::atomicCmpXchg32(const MemoryAccessDesc& access, T srcAddr,
                                   RegI32 rexpect, RegI32 rnew, RegI32 rd,
                                   const AtomicCmpXchg32Temps& temps) {
  switch (access.type()) {
    case Scalar::Uint8:
#if defined(JS_CODEGEN_X86)
    {
      ScratchI8 scratch(*this);
      MOZ_ASSERT(rd == specific_.eax);
      if (!ra.isSingleByteI32(rnew)) {
        // The replacement value must have a byte persona.
        masm.movl(rnew, scratch);
        rnew = scratch;
      }
      masm.wasmCompareExchange(access, srcAddr, rexpect, rnew, rd);
      break;
    }
#endif
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
      masm.wasmCompareExchange(access, srcAddr, rexpect, rnew, temps[0],
                               temps[1], temps[2], rd);
#else
      masm.wasmCompareExchange(access, srcAddr, rexpect, rnew, rd);
#endif
      break;
    default:
      MOZ_CRASH("Bad type for atomic operation");
  }
}

template <typename T>
void BaseCompiler::atomicXchg32(const MemoryAccessDesc& access, T srcAddr,
                                RegI32 rv, RegI32 rd,
                                const AtomicXchg32Temps& temps) {
  switch (access.type()) {
    case Scalar::Uint8:
#if defined(JS_CODEGEN_X86)
    {
      if (!ra.isSingleByteI32(rd)) {
        ScratchI8 scratch(*this);
        // The output register must have a byte persona.
        masm.wasmAtomicExchange(access, srcAddr, rv, scratch);
        masm.movl(scratch, rd);
      } else {
        masm.wasmAtomicExchange(access, srcAddr, rv, rd);
      }
      break;
    }
#endif
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
      masm.wasmAtomicExchange(access, srcAddr, rv, temps[0], temps[1], temps[2],
                              rd);
#else
      masm.wasmAtomicExchange(access, srcAddr, rv, rd);
#endif
      break;
    default:
      MOZ_CRASH("Bad type for atomic operation");
  }
}

// The RAII wrappers are used because we sometimes have to free partial
// registers, as when part of a register is the scratch register that has
// been temporarily used, or not free a register at all, as when the
// register is the same as the destination register (but only on some
// platforms, not on all).  These are called PopX{32,64}Regs where X is the
// operation being targeted.

// Utility struct that holds the BaseCompiler and the destination, and frees
// the destination if it has not been extracted.

template <typename T>
class PopBase {
  T rd_;

  void maybeFree(RegI32 r) { bc->maybeFree(r); }
  void maybeFree(RegI64 r) { bc->maybeFree(r); }

 protected:
  BaseCompiler* const bc;

  void setRd(T r) {
    MOZ_ASSERT(rd_.isInvalid());
    rd_ = r;
  }
  T getRd() const {
    MOZ_ASSERT(rd_.isValid());
    return rd_;
  }

 public:
  explicit PopBase(BaseCompiler* bc) : bc(bc) {}
  ~PopBase() { maybeFree(rd_); }

  // Take and clear the Rd - use this when pushing Rd.
  T takeRd() {
    MOZ_ASSERT(rd_.isValid());
    T r = rd_;
    rd_ = T::Invalid();
    return r;
  }
};

class PopAtomicCmpXchg32Regs : public PopBase<RegI32> {
  using Base = PopBase<RegI32>;
  RegI32 rexpect, rnew;
  AtomicCmpXchg32Temps temps;

 public:
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
  explicit PopAtomicCmpXchg32Regs(BaseCompiler* bc, ValType type,
                                  Scalar::Type viewType)
      : Base(bc) {
    // For cmpxchg, the expected value and the result are both in eax.
    bc->needI32(bc->specific_.eax);
    if (type == ValType::I64) {
      rnew = bc->popI64ToI32();
      rexpect = bc->popI64ToSpecificI32(bc->specific_.eax);
    } else {
      rnew = bc->popI32();
      rexpect = bc->popI32ToSpecific(bc->specific_.eax);
    }
    setRd(rexpect);
  }
  ~PopAtomicCmpXchg32Regs() { bc->freeI32(rnew); }
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
  explicit PopAtomicCmpXchg32Regs(BaseCompiler* bc, ValType type,
                                  Scalar::Type viewType)
      : Base(bc) {
    if (type == ValType::I64) {
      rnew = bc->popI64ToI32();
      rexpect = bc->popI64ToI32();
    } else {
      rnew = bc->popI32();
      rexpect = bc->popI32();
    }
    setRd(bc->needI32());
  }
  ~PopAtomicCmpXchg32Regs() {
    bc->freeI32(rnew);
    bc->freeI32(rexpect);
  }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  explicit PopAtomicCmpXchg32Regs(BaseCompiler* bc, ValType type,
                                  Scalar::Type viewType)
      : Base(bc) {
    if (type == ValType::I64) {
      rnew = bc->popI64ToI32();
      rexpect = bc->popI64ToI32();
    } else {
      rnew = bc->popI32();
      rexpect = bc->popI32();
    }
    if (Scalar::byteSize(viewType) < 4) {
      temps.allocate(bc);
    }
    setRd(bc->needI32());
  }
  ~PopAtomicCmpXchg32Regs() {
    bc->freeI32(rnew);
    bc->freeI32(rexpect);
    temps.maybeFree(bc);
  }
#else
  explicit PopAtomicCmpXchg32Regs(BaseCompiler* bc, ValType type,
                                  Scalar::Type viewType)
      : Base(bc) {
    MOZ_CRASH("BaseCompiler porting interface: PopAtomicCmpXchg32Regs");
  }
#endif

  template <typename T>
  void atomicCmpXchg32(const MemoryAccessDesc& access, T srcAddr) {
    bc->atomicCmpXchg32(access, srcAddr, rexpect, rnew, getRd(), temps);
  }
};

class PopAtomicCmpXchg64Regs : public PopBase<RegI64> {
  using Base = PopBase<RegI64>;
  RegI64 rexpect, rnew;

 public:
#ifdef JS_CODEGEN_X64
  explicit PopAtomicCmpXchg64Regs(BaseCompiler* bc) : Base(bc) {
    // For cmpxchg, the expected value and the result are both in rax.
    bc->needI64(bc->specific_.rax);
    rnew = bc->popI64();
    rexpect = bc->popI64ToSpecific(bc->specific_.rax);
    setRd(rexpect);
  }
  ~PopAtomicCmpXchg64Regs() { bc->freeI64(rnew); }
#elif defined(JS_CODEGEN_X86)
  explicit PopAtomicCmpXchg64Regs(BaseCompiler* bc) : Base(bc) {
    // For cmpxchg8b, the expected value and the result are both in
    // edx:eax, and the replacement value is in ecx:ebx.  But we can't
    // allocate ebx here, so instead we allocate a temp to hold the low
    // word of 'new'.
    bc->needI64(bc->specific_.edx_eax);
    bc->needI32(bc->specific_.ecx);

    rnew = bc->popI64ToSpecific(
        RegI64(Register64(bc->specific_.ecx, bc->needI32())));
    rexpect = bc->popI64ToSpecific(bc->specific_.edx_eax);
    setRd(rexpect);
  }
  ~PopAtomicCmpXchg64Regs() { bc->freeI64(rnew); }
#elif defined(JS_CODEGEN_ARM)
  explicit PopAtomicCmpXchg64Regs(BaseCompiler* bc) : Base(bc) {
    // The replacement value and the result must both be odd/even pairs.
    rnew = bc->popI64Pair();
    rexpect = bc->popI64();
    setRd(bc->needI64Pair());
  }
  ~PopAtomicCmpXchg64Regs() {
    bc->freeI64(rexpect);
    bc->freeI64(rnew);
  }
#elif defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_MIPS32) || \
    defined(JS_CODEGEN_MIPS64)
  explicit PopAtomicCmpXchg64Regs(BaseCompiler* bc) : Base(bc) {
    rnew = bc->popI64();
    rexpect = bc->popI64();
    setRd(bc->needI64());
  }
  ~PopAtomicCmpXchg64Regs() {
    bc->freeI64(rexpect);
    bc->freeI64(rnew);
  }
#else
  explicit PopAtomicCmpXchg64Regs(BaseCompiler* bc) : Base(bc) {
    MOZ_CRASH("BaseCompiler porting interface: PopAtomicCmpXchg64Regs");
  }
#endif

#ifdef JS_CODEGEN_X86
  template <typename T>
  void atomicCmpXchg64(const MemoryAccessDesc& access, T srcAddr, RegI32 ebx) {
    MOZ_ASSERT(ebx == js::jit::ebx);
    bc->masm.move32(rnew.low, ebx);
    bc->masm.wasmCompareExchange64(access, srcAddr, rexpect,
                                   bc->specific_.ecx_ebx, getRd());
  }
#else
  template <typename T>
  void atomicCmpXchg64(const MemoryAccessDesc& access, T srcAddr) {
    bc->masm.wasmCompareExchange64(access, srcAddr, rexpect, rnew, getRd());
  }
#endif
};

#ifndef JS_64BIT
class PopAtomicLoad64Regs : public PopBase<RegI64> {
  using Base = PopBase<RegI64>;

 public:
#  if defined(JS_CODEGEN_X86)
  explicit PopAtomicLoad64Regs(BaseCompiler* bc) : Base(bc) {
    // The result is in edx:eax, and we need ecx:ebx as a temp.  But we
    // can't reserve ebx yet, so we'll accept it as an argument to the
    // operation (below).
    bc->needI32(bc->specific_.ecx);
    bc->needI64(bc->specific_.edx_eax);
    setRd(bc->specific_.edx_eax);
  }
  ~PopAtomicLoad64Regs() { bc->freeI32(bc->specific_.ecx); }
#  elif defined(JS_CODEGEN_ARM)
  explicit PopAtomicLoad64Regs(BaseCompiler* bc) : Base(bc) {
    setRd(bc->needI64Pair());
  }
#  elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  explicit PopAtomicLoad64Regs(BaseCompiler* bc) : Base(bc) {
    setRd(bc->needI64());
  }
#  else
  explicit PopAtomicLoad64Regs(BaseCompiler* bc) : Base(bc) {
    MOZ_CRASH("BaseCompiler porting interface: PopAtomicLoad64Regs");
  }
#  endif

#  ifdef JS_CODEGEN_X86
  template <typename T>
  void atomicLoad64(const MemoryAccessDesc& access, T srcAddr, RegI32 ebx) {
    MOZ_ASSERT(ebx == js::jit::ebx);
    bc->masm.wasmAtomicLoad64(access, srcAddr, bc->specific_.ecx_ebx, getRd());
  }
#  else  // ARM, MIPS32
  template <typename T>
  void atomicLoad64(const MemoryAccessDesc& access, T srcAddr) {
    bc->masm.wasmAtomicLoad64(access, srcAddr, RegI64::Invalid(), getRd());
  }
#  endif
};
#endif  // JS_64BIT

class PopAtomicRMW32Regs : public PopBase<RegI32> {
  using Base = PopBase<RegI32>;
  RegI32 rv;
  AtomicRMW32Temps temps;

 public:
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
  explicit PopAtomicRMW32Regs(BaseCompiler* bc, ValType type,
                              Scalar::Type viewType, AtomicOp op)
      : Base(bc) {
    bc->needI32(bc->specific_.eax);
    if (op == AtomicFetchAddOp || op == AtomicFetchSubOp) {
      // We use xadd, so source and destination are the same.  Using
      // eax here is overconstraining, but for byte operations on x86
      // we do need something with a byte register.
      if (type == ValType::I64) {
        rv = bc->popI64ToSpecificI32(bc->specific_.eax);
      } else {
        rv = bc->popI32ToSpecific(bc->specific_.eax);
      }
      setRd(rv);
    } else {
      // We use a cmpxchg loop.  The output must be eax; the input
      // must be in a separate register since it may be used several
      // times.
      if (type == ValType::I64) {
        rv = bc->popI64ToI32();
      } else {
        rv = bc->popI32();
      }
      setRd(bc->specific_.eax);
#  if defined(JS_CODEGEN_X86)
      // Single-byte is a special case handled very locally with
      // ScratchReg, see atomicRMW32 above.
      if (Scalar::byteSize(viewType) > 1) {
        temps.allocate(bc);
      }
#  else
      temps.allocate(bc);
#  endif
    }
  }
  ~PopAtomicRMW32Regs() {
    if (rv != bc->specific_.eax) {
      bc->freeI32(rv);
    }
    temps.maybeFree(bc);
  }
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
  explicit PopAtomicRMW32Regs(BaseCompiler* bc, ValType type,
                              Scalar::Type viewType, AtomicOp op)
      : Base(bc) {
    rv = type == ValType::I64 ? bc->popI64ToI32() : bc->popI32();
    temps.allocate(bc);
    setRd(bc->needI32());
  }
  ~PopAtomicRMW32Regs() {
    bc->freeI32(rv);
    temps.maybeFree(bc);
  }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  explicit PopAtomicRMW32Regs(BaseCompiler* bc, ValType type,
                              Scalar::Type viewType, AtomicOp op)
      : Base(bc) {
    rv = type == ValType::I64 ? bc->popI64ToI32() : bc->popI32();
    if (Scalar::byteSize(viewType) < 4) {
      temps.allocate(bc);
    }

    setRd(bc->needI32());
  }
  ~PopAtomicRMW32Regs() {
    bc->freeI32(rv);
    temps.maybeFree(bc);
  }
#else
  explicit PopAtomicRMW32Regs(BaseCompiler* bc, ValType type,
                              Scalar::Type viewType, AtomicOp op)
      : Base(bc) {
    MOZ_CRASH("BaseCompiler porting interface: PopAtomicRMW32Regs");
  }
#endif

  template <typename T>
  void atomicRMW32(const MemoryAccessDesc& access, T srcAddr, AtomicOp op) {
    bc->atomicRMW32(access, srcAddr, op, rv, getRd(), temps);
  }
};

class PopAtomicRMW64Regs : public PopBase<RegI64> {
  using Base = PopBase<RegI64>;
#if defined(JS_CODEGEN_X64)
  AtomicOp op;
#endif
  RegI64 rv, temp;

 public:
#if defined(JS_CODEGEN_X64)
  explicit PopAtomicRMW64Regs(BaseCompiler* bc, AtomicOp op)
      : Base(bc), op(op) {
    if (op == AtomicFetchAddOp || op == AtomicFetchSubOp) {
      // We use xaddq, so input and output must be the same register.
      rv = bc->popI64();
      setRd(rv);
    } else {
      // We use a cmpxchgq loop, so the output must be rax.
      bc->needI64(bc->specific_.rax);
      rv = bc->popI64();
      temp = bc->needI64();
      setRd(bc->specific_.rax);
    }
  }
  ~PopAtomicRMW64Regs() {
    bc->maybeFree(temp);
    if (op != AtomicFetchAddOp && op != AtomicFetchSubOp) {
      bc->freeI64(rv);
    }
  }
#elif defined(JS_CODEGEN_X86)
  // We'll use cmpxchg8b, so rv must be in ecx:ebx, and rd must be
  // edx:eax.  But we can't reserve ebx here because we need it later, so
  // use a separate temp and set up ebx when we perform the operation.
  explicit PopAtomicRMW64Regs(BaseCompiler* bc, AtomicOp) : Base(bc) {
    bc->needI32(bc->specific_.ecx);
    bc->needI64(bc->specific_.edx_eax);

    temp = RegI64(Register64(bc->specific_.ecx, bc->needI32()));
    bc->popI64ToSpecific(temp);

    setRd(bc->specific_.edx_eax);
  }
  ~PopAtomicRMW64Regs() { bc->freeI64(temp); }
  RegI32 valueHigh() const { return RegI32(temp.high); }
  RegI32 valueLow() const { return RegI32(temp.low); }
#elif defined(JS_CODEGEN_ARM)
  explicit PopAtomicRMW64Regs(BaseCompiler* bc, AtomicOp) : Base(bc) {
    // We use a ldrex/strexd loop so the temp and the output must be
    // odd/even pairs.
    rv = bc->popI64();
    temp = bc->needI64Pair();
    setRd(bc->needI64Pair());
  }
  ~PopAtomicRMW64Regs() {
    bc->freeI64(rv);
    bc->freeI64(temp);
  }
#elif defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_MIPS32) || \
    defined(JS_CODEGEN_MIPS64)
  explicit PopAtomicRMW64Regs(BaseCompiler* bc, AtomicOp) : Base(bc) {
    rv = bc->popI64();
    temp = bc->needI64();
    setRd(bc->needI64());
  }
  ~PopAtomicRMW64Regs() {
    bc->freeI64(rv);
    bc->freeI64(temp);
  }
#else
  explicit PopAtomicRMW64Regs(BaseCompiler* bc, AtomicOp) : Base(bc) {
    MOZ_CRASH("BaseCompiler porting interface: PopAtomicRMW64Regs");
  }
#endif

#ifdef JS_CODEGEN_X86
  template <typename T, typename V>
  void atomicRMW64(const MemoryAccessDesc& access, T srcAddr, AtomicOp op,
                   const V& value, RegI32 ebx) {
    MOZ_ASSERT(ebx == js::jit::ebx);
    bc->atomicRMW64(access, srcAddr, op, value, bc->specific_.ecx_ebx, getRd());
  }
#else
  template <typename T>
  void atomicRMW64(const MemoryAccessDesc& access, T srcAddr, AtomicOp op) {
    bc->atomicRMW64(access, srcAddr, op, rv, temp, getRd());
  }
#endif
};

class PopAtomicXchg32Regs : public PopBase<RegI32> {
  using Base = PopBase<RegI32>;
  RegI32 rv;
  AtomicXchg32Temps temps;

 public:
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
  explicit PopAtomicXchg32Regs(BaseCompiler* bc, ValType type,
                               Scalar::Type viewType)
      : Base(bc) {
    // The xchg instruction reuses rv as rd.
    rv = (type == ValType::I64) ? bc->popI64ToI32() : bc->popI32();
    setRd(rv);
  }
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
  explicit PopAtomicXchg32Regs(BaseCompiler* bc, ValType type,
                               Scalar::Type viewType)
      : Base(bc) {
    rv = (type == ValType::I64) ? bc->popI64ToI32() : bc->popI32();
    setRd(bc->needI32());
  }
  ~PopAtomicXchg32Regs() { bc->freeI32(rv); }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  explicit PopAtomicXchg32Regs(BaseCompiler* bc, ValType type,
                               Scalar::Type viewType)
      : Base(bc) {
    rv = (type == ValType::I64) ? bc->popI64ToI32() : bc->popI32();
    if (Scalar::byteSize(viewType) < 4) {
      temps.allocate(bc);
    }
    setRd(bc->needI32());
  }
  ~PopAtomicXchg32Regs() {
    temps.maybeFree(bc);
    bc->freeI32(rv);
  }
#else
  explicit PopAtomicXchg32Regs(BaseCompiler* bc, ValType type,
                               Scalar::Type viewType)
      : Base(bc) {
    MOZ_CRASH("BaseCompiler porting interface: PopAtomicXchg32Regs");
  }
#endif

  template <typename T>
  void atomicXchg32(const MemoryAccessDesc& access, T srcAddr) {
    bc->atomicXchg32(access, srcAddr, rv, getRd(), temps);
  }
};

class PopAtomicXchg64Regs : public PopBase<RegI64> {
  using Base = PopBase<RegI64>;
  RegI64 rv;

 public:
#if defined(JS_CODEGEN_X64)
  explicit PopAtomicXchg64Regs(BaseCompiler* bc) : Base(bc) {
    rv = bc->popI64();
    setRd(rv);
  }
#elif defined(JS_CODEGEN_ARM64)
  explicit PopAtomicXchg64Regs(BaseCompiler* bc) : Base(bc) {
    rv = bc->popI64();
    setRd(bc->needI64());
  }
  ~PopAtomicXchg64Regs() { bc->freeI64(rv); }
#elif defined(JS_CODEGEN_X86)
  // We'll use cmpxchg8b, so rv must be in ecx:ebx, and rd must be
  // edx:eax.  But we can't reserve ebx here because we need it later, so
  // use a separate temp and set up ebx when we perform the operation.
  explicit PopAtomicXchg64Regs(BaseCompiler* bc) : Base(bc) {
    bc->needI32(bc->specific_.ecx);
    bc->needI64(bc->specific_.edx_eax);

    rv = RegI64(Register64(bc->specific_.ecx, bc->needI32()));
    bc->popI64ToSpecific(rv);

    setRd(bc->specific_.edx_eax);
  }
  ~PopAtomicXchg64Regs() { bc->freeI64(rv); }
#elif defined(JS_CODEGEN_ARM)
  // Both rv and rd must be odd/even pairs.
  explicit PopAtomicXchg64Regs(BaseCompiler* bc) : Base(bc) {
    rv = bc->popI64ToSpecific(bc->needI64Pair());
    setRd(bc->needI64Pair());
  }
  ~PopAtomicXchg64Regs() { bc->freeI64(rv); }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  explicit PopAtomicXchg64Regs(BaseCompiler* bc) : Base(bc) {
    rv = bc->popI64ToSpecific(bc->needI64());
    setRd(bc->needI64());
  }
  ~PopAtomicXchg64Regs() { bc->freeI64(rv); }
#else
  explicit PopAtomicXchg64Regs(BaseCompiler* bc) : Base(bc) {
    MOZ_CRASH("BaseCompiler porting interface: xchg64");
  }
#endif

#ifdef JS_CODEGEN_X86
  template <typename T>
  void atomicXchg64(const MemoryAccessDesc& access, T srcAddr,
                    RegI32 ebx) const {
    MOZ_ASSERT(ebx == js::jit::ebx);
    bc->masm.move32(rv.low, ebx);
    bc->masm.wasmAtomicExchange64(access, srcAddr, bc->specific_.ecx_ebx,
                                  getRd());
  }
#else
  template <typename T>
  void atomicXchg64(const MemoryAccessDesc& access, T srcAddr) const {
    bc->masm.wasmAtomicExchange64(access, srcAddr, rv, getRd());
  }
#endif
};

bool BaseCompiler::atomicCmpXchg(MemoryAccessDesc* access, ValType type) {
  Scalar::Type viewType = access->type();
  if (Scalar::byteSize(viewType) <= 4) {
    PopAtomicCmpXchg32Regs regs(this, type, viewType);

    AccessCheck check;
    RegI32 rp = popMemory32Access(access, &check);
    RegI32 tls = maybeLoadTlsForAccess(check);

    auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
    regs.atomicCmpXchg32(*access, memaddr);

    maybeFree(tls);
    freeI32(rp);

    if (type == ValType::I64) {
      pushU32AsI64(regs.takeRd());
    } else {
      pushI32(regs.takeRd());
    }

    return true;
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

  PopAtomicCmpXchg64Regs regs(this);

  AccessCheck check;
  RegI32 rp = popMemory32Access(access, &check);

#ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicCmpXchg64(*access, memaddr, ebx);
#else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicCmpXchg64(*access, memaddr);
  maybeFree(tls);
#endif

  freeI32(rp);

  pushI64(regs.takeRd());
  return true;
}

bool BaseCompiler::atomicLoad(MemoryAccessDesc* access, ValType type) {
  Scalar::Type viewType = access->type();
  if (Scalar::byteSize(viewType) <= sizeof(void*)) {
    return loadCommon(access, AccessCheck(), type);
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

#if defined(JS_64BIT)
  MOZ_CRASH("Should not happen");
#else
  PopAtomicLoad64Regs regs(this);

  AccessCheck check;
  RegI32 rp = popMemory32Access(access, &check);

#  ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicLoad64(*access, memaddr, ebx);
#  else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicLoad64(*access, memaddr);
  maybeFree(tls);
#  endif

  freeI32(rp);

  pushI64(regs.takeRd());
  return true;
#endif  // JS_64BIT
}

bool BaseCompiler::atomicRMW(MemoryAccessDesc* access, ValType type,
                             AtomicOp op) {
  Scalar::Type viewType = access->type();

  if (Scalar::byteSize(viewType) <= 4) {
    PopAtomicRMW32Regs regs(this, type, viewType, op);

    AccessCheck check;
    RegI32 rp = popMemory32Access(access, &check);
    RegI32 tls = maybeLoadTlsForAccess(check);

    auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
    regs.atomicRMW32(*access, memaddr, op);

    maybeFree(tls);
    freeI32(rp);

    if (type == ValType::I64) {
      pushU32AsI64(regs.takeRd());
    } else {
      pushI32(regs.takeRd());
    }
    return true;
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

  PopAtomicRMW64Regs regs(this, op);

  AccessCheck check;
  RegI32 rp = popMemory32Access(access, &check);

#ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);

  fr.pushGPR(regs.valueHigh());
  fr.pushGPR(regs.valueLow());
  Address value(esp, 0);

  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicRMW64(*access, memaddr, op, value, ebx);

  fr.popBytes(8);
#else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicRMW64(*access, memaddr, op);
  maybeFree(tls);
#endif

  freeI32(rp);

  pushI64(regs.takeRd());
  return true;
}

bool BaseCompiler::atomicStore(MemoryAccessDesc* access, ValType type) {
  Scalar::Type viewType = access->type();

  if (Scalar::byteSize(viewType) <= sizeof(void*)) {
    return storeCommon(access, AccessCheck(), type);
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

#ifdef JS_64BIT
  MOZ_CRASH("Should not happen");
#else
  emitAtomicXchg64(access, WantResult(false));
  return true;
#endif
}

bool BaseCompiler::atomicXchg(MemoryAccessDesc* access, ValType type) {
  Scalar::Type viewType = access->type();

  AccessCheck check;

  if (Scalar::byteSize(viewType) <= 4) {
    PopAtomicXchg32Regs regs(this, type, viewType);
    RegI32 rp = popMemory32Access(access, &check);
    RegI32 tls = maybeLoadTlsForAccess(check);

    auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
    regs.atomicXchg32(*access, memaddr);

    maybeFree(tls);
    freeI32(rp);

    if (type == ValType::I64) {
      pushU32AsI64(regs.takeRd());
    } else {
      pushI32(regs.takeRd());
    }
    return true;
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

  emitAtomicXchg64(access, WantResult(true));
  return true;
}

void BaseCompiler::emitAtomicXchg64(MemoryAccessDesc* access,
                                    WantResult wantResult) {
  PopAtomicXchg64Regs regs(this);

  AccessCheck check;
  RegI32 rp = popMemory32Access(access, &check);

#ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicXchg64(*access, memaddr, ebx);
#else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicXchg64(*access, memaddr);
  maybeFree(tls);
#endif

  freeI32(rp);

  if (wantResult) {
    pushI64(regs.takeRd());
  }
}

RegI32 BaseCompiler::maybeLoadTlsForAccess(const AccessCheck& check) {
  RegI32 tls;
  if (needTlsForAccess(check)) {
    tls = needI32();
    fr.loadTlsPtr(tls);
  }
  return tls;
}

RegI32 BaseCompiler::maybeLoadTlsForAccess(const AccessCheck& check,
                                           RegI32 specific) {
  if (needTlsForAccess(check)) {
    fr.loadTlsPtr(specific);
    return specific;
  }
  return RegI32::Invalid();
}

bool BaseCompiler::loadCommon(MemoryAccessDesc* access, AccessCheck check,
                              ValType type) {
  RegI32 tls, temp;
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  temp = needI32();
#endif

  switch (type.kind()) {
    case ValType::I32: {
      RegI32 rp = popMemory32Access(access, &check);
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rp), temp)) {
        return false;
      }
      pushI32(rp);
      break;
    }
    case ValType::I64: {
      RegI64 rv;
      RegI32 rp;
#ifdef JS_CODEGEN_X86
      rv = specific_.abiReturnRegI64;
      needI64(rv);
      rp = popMemory32Access(access, &check);
#else
      rp = popMemory32Access(access, &check);
      rv = needI64();
#endif
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      pushI64(rv);
      freeI32(rp);
      break;
    }
    case ValType::F32: {
      RegI32 rp = popMemory32Access(access, &check);
      RegF32 rv = needF32();
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      pushF32(rv);
      freeI32(rp);
      break;
    }
    case ValType::F64: {
      RegI32 rp = popMemory32Access(access, &check);
      RegF64 rv = needF64();
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      pushF64(rv);
      freeI32(rp);
      break;
    }
#ifdef ENABLE_WASM_SIMD
    case ValType::V128: {
      RegI32 rp = popMemory32Access(access, &check);
      RegV128 rv = needV128();
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      pushV128(rv);
      freeI32(rp);
      break;
    }
#endif
    default:
      MOZ_CRASH("load type");
      break;
  }

  maybeFree(tls);
  maybeFree(temp);

  return true;
}

bool BaseCompiler::storeCommon(MemoryAccessDesc* access, AccessCheck check,
                               ValType resultType) {
  RegI32 tls, temp;
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  temp = needI32();
#endif

  switch (resultType.kind()) {
    case ValType::I32: {
      RegI32 rv = popI32();
      RegI32 rp = popMemory32Access(access, &check);
      tls = maybeLoadTlsForAccess(check);
      if (!store(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      freeI32(rp);
      freeI32(rv);
      break;
    }
    case ValType::I64: {
      RegI64 rv = popI64();
      RegI32 rp = popMemory32Access(access, &check);
      tls = maybeLoadTlsForAccess(check);
      if (!store(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      freeI32(rp);
      freeI64(rv);
      break;
    }
    case ValType::F32: {
      RegF32 rv = popF32();
      RegI32 rp = popMemory32Access(access, &check);
      tls = maybeLoadTlsForAccess(check);
      if (!store(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      freeI32(rp);
      freeF32(rv);
      break;
    }
    case ValType::F64: {
      RegF64 rv = popF64();
      RegI32 rp = popMemory32Access(access, &check);
      tls = maybeLoadTlsForAccess(check);
      if (!store(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      freeI32(rp);
      freeF64(rv);
      break;
    }
#ifdef ENABLE_WASM_SIMD
    case ValType::V128: {
      RegV128 rv = popV128();
      RegI32 rp = popMemory32Access(access, &check);
      tls = maybeLoadTlsForAccess(check);
      if (!store(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      freeI32(rp);
      freeV128(rv);
      break;
    }
#endif
    default:
      MOZ_CRASH("store type");
      break;
  }

  maybeFree(tls);
  maybeFree(temp);

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Bulk memory.

bool BaseCompiler::emitMemCopyInline() {
  MOZ_ASSERT(MaxInlineMemoryCopyLength != 0);

  int32_t signedLength;
  MOZ_ALWAYS_TRUE(popConst(&signedLength));
  uint32_t length = signedLength;
  MOZ_ASSERT(length != 0 && length <= MaxInlineMemoryCopyLength);

  RegI32 src = popI32();
  RegI32 dest = popI32();

  // Compute the number of copies of each width we will need to do
  size_t remainder = length;
#ifdef ENABLE_WASM_SIMD
  size_t numCopies16 = 0;
  if (MacroAssembler::SupportsFastUnalignedFPAccesses()) {
    numCopies16 = remainder / sizeof(V128);
    remainder %= sizeof(V128);
  }
#endif
#ifdef JS_64BIT
  size_t numCopies8 = remainder / sizeof(uint64_t);
  remainder %= sizeof(uint64_t);
#endif
  size_t numCopies4 = remainder / sizeof(uint32_t);
  remainder %= sizeof(uint32_t);
  size_t numCopies2 = remainder / sizeof(uint16_t);
  remainder %= sizeof(uint16_t);
  size_t numCopies1 = remainder;

  // Load all source bytes onto the value stack from low to high using the
  // widest transfer width we can for the system. We will trap without writing
  // anything if any source byte is out-of-bounds.
  bool omitBoundsCheck = false;
  size_t offset = 0;

#ifdef ENABLE_WASM_SIMD
  for (uint32_t i = 0; i < numCopies16; i++) {
    RegI32 temp = needI32();
    moveI32(src, temp);
    pushI32(temp);

    MemoryAccessDesc access(Scalar::Simd128, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!loadCommon(&access, check, ValType::V128)) {
      return false;
    }

    offset += sizeof(V128);
    omitBoundsCheck = true;
  }
#endif

#ifdef JS_64BIT
  for (uint32_t i = 0; i < numCopies8; i++) {
    RegI32 temp = needI32();
    moveI32(src, temp);
    pushI32(temp);

    MemoryAccessDesc access(Scalar::Int64, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!loadCommon(&access, check, ValType::I64)) {
      return false;
    }

    offset += sizeof(uint64_t);
    omitBoundsCheck = true;
  }
#endif

  for (uint32_t i = 0; i < numCopies4; i++) {
    RegI32 temp = needI32();
    moveI32(src, temp);
    pushI32(temp);

    MemoryAccessDesc access(Scalar::Uint32, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!loadCommon(&access, check, ValType::I32)) {
      return false;
    }

    offset += sizeof(uint32_t);
    omitBoundsCheck = true;
  }

  if (numCopies2) {
    RegI32 temp = needI32();
    moveI32(src, temp);
    pushI32(temp);

    MemoryAccessDesc access(Scalar::Uint16, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!loadCommon(&access, check, ValType::I32)) {
      return false;
    }

    offset += sizeof(uint16_t);
    omitBoundsCheck = true;
  }

  if (numCopies1) {
    RegI32 temp = needI32();
    moveI32(src, temp);
    pushI32(temp);

    MemoryAccessDesc access(Scalar::Uint8, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!loadCommon(&access, check, ValType::I32)) {
      return false;
    }
  }

  // Store all source bytes from the value stack to the destination from
  // high to low. We will trap without writing anything on the first store
  // if any dest byte is out-of-bounds.
  offset = length;
  omitBoundsCheck = false;

  if (numCopies1) {
    offset -= sizeof(uint8_t);

    RegI32 value = popI32();
    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI32(value);

    MemoryAccessDesc access(Scalar::Uint8, 1, offset, bytecodeOffset());
    AccessCheck check;
    if (!storeCommon(&access, check, ValType::I32)) {
      return false;
    }

    omitBoundsCheck = true;
  }

  if (numCopies2) {
    offset -= sizeof(uint16_t);

    RegI32 value = popI32();
    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI32(value);

    MemoryAccessDesc access(Scalar::Uint16, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::I32)) {
      return false;
    }

    omitBoundsCheck = true;
  }

  for (uint32_t i = 0; i < numCopies4; i++) {
    offset -= sizeof(uint32_t);

    RegI32 value = popI32();
    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI32(value);

    MemoryAccessDesc access(Scalar::Uint32, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::I32)) {
      return false;
    }

    omitBoundsCheck = true;
  }

#ifdef JS_64BIT
  for (uint32_t i = 0; i < numCopies8; i++) {
    offset -= sizeof(uint64_t);

    RegI64 value = popI64();
    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI64(value);

    MemoryAccessDesc access(Scalar::Int64, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::I64)) {
      return false;
    }

    omitBoundsCheck = true;
  }
#endif

#ifdef ENABLE_WASM_SIMD
  for (uint32_t i = 0; i < numCopies16; i++) {
    offset -= sizeof(V128);

    RegV128 value = popV128();
    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushV128(value);

    MemoryAccessDesc access(Scalar::Simd128, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::V128)) {
      return false;
    }

    omitBoundsCheck = true;
  }
#endif

  freeI32(dest);
  freeI32(src);
  return true;
}

bool BaseCompiler::emitMemFillInline() {
  MOZ_ASSERT(MaxInlineMemoryFillLength != 0);

  int32_t signedLength;
  int32_t signedValue;
  MOZ_ALWAYS_TRUE(popConst(&signedLength));
  MOZ_ALWAYS_TRUE(popConst(&signedValue));
  uint32_t length = uint32_t(signedLength);
  uint32_t value = uint32_t(signedValue);
  MOZ_ASSERT(length != 0 && length <= MaxInlineMemoryFillLength);

  RegI32 dest = popI32();

  // Compute the number of copies of each width we will need to do
  size_t remainder = length;
#ifdef ENABLE_WASM_SIMD
  size_t numCopies16 = 0;
  if (MacroAssembler::SupportsFastUnalignedFPAccesses()) {
    numCopies16 = remainder / sizeof(V128);
    remainder %= sizeof(V128);
  }
#endif
#ifdef JS_64BIT
  size_t numCopies8 = remainder / sizeof(uint64_t);
  remainder %= sizeof(uint64_t);
#endif
  size_t numCopies4 = remainder / sizeof(uint32_t);
  remainder %= sizeof(uint32_t);
  size_t numCopies2 = remainder / sizeof(uint16_t);
  remainder %= sizeof(uint16_t);
  size_t numCopies1 = remainder;

  MOZ_ASSERT(numCopies2 <= 1 && numCopies1 <= 1);

  // Generate splatted definitions for wider fills as needed
#ifdef ENABLE_WASM_SIMD
  V128 val16(value);
#endif
#ifdef JS_64BIT
  uint64_t val8 = SplatByteToUInt<uint64_t>(value, 8);
#endif
  uint32_t val4 = SplatByteToUInt<uint32_t>(value, 4);
  uint32_t val2 = SplatByteToUInt<uint32_t>(value, 2);
  uint32_t val1 = value;

  // Store the fill value to the destination from high to low. We will trap
  // without writing anything on the first store if any dest byte is
  // out-of-bounds.
  size_t offset = length;
  bool omitBoundsCheck = false;

  if (numCopies1) {
    offset -= sizeof(uint8_t);

    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI32(val1);

    MemoryAccessDesc access(Scalar::Uint8, 1, offset, bytecodeOffset());
    AccessCheck check;
    if (!storeCommon(&access, check, ValType::I32)) {
      return false;
    }

    omitBoundsCheck = true;
  }

  if (numCopies2) {
    offset -= sizeof(uint16_t);

    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI32(val2);

    MemoryAccessDesc access(Scalar::Uint16, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::I32)) {
      return false;
    }

    omitBoundsCheck = true;
  }

  for (uint32_t i = 0; i < numCopies4; i++) {
    offset -= sizeof(uint32_t);

    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI32(val4);

    MemoryAccessDesc access(Scalar::Uint32, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::I32)) {
      return false;
    }

    omitBoundsCheck = true;
  }

#ifdef JS_64BIT
  for (uint32_t i = 0; i < numCopies8; i++) {
    offset -= sizeof(uint64_t);

    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushI64(val8);

    MemoryAccessDesc access(Scalar::Int64, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::I64)) {
      return false;
    }

    omitBoundsCheck = true;
  }
#endif

#ifdef ENABLE_WASM_SIMD
  for (uint32_t i = 0; i < numCopies16; i++) {
    offset -= sizeof(V128);

    RegI32 temp = needI32();
    moveI32(dest, temp);
    pushI32(temp);
    pushV128(val16);

    MemoryAccessDesc access(Scalar::Simd128, 1, offset, bytecodeOffset());
    AccessCheck check;
    check.omitBoundsCheck = omitBoundsCheck;
    if (!storeCommon(&access, check, ValType::V128)) {
      return false;
    }

    omitBoundsCheck = true;
  }
#endif

  freeI32(dest);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// SIMD and Relaxed SIMD.

#ifdef ENABLE_WASM_SIMD
bool BaseCompiler::loadSplat(MemoryAccessDesc* access) {
  // We can implement loadSplat mostly as load + splat because the push of the
  // result onto the value stack in loadCommon normally will not generate any
  // code, it will leave the value in a register which we will consume.

  // We use uint types when we can on the general assumption that unsigned loads
  // might be smaller/faster on some platforms, because no sign extension needs
  // to be done after the sub-register load.
  RegV128 rd = needV128();
  switch (access->type()) {
    case Scalar::Uint8: {
      if (!loadCommon(access, AccessCheck(), ValType::I32)) {
        return false;
      }
      RegI32 rs = popI32();
      masm.splatX16(rs, rd);
      free(rs);
      break;
    }
    case Scalar::Uint16: {
      if (!loadCommon(access, AccessCheck(), ValType::I32)) {
        return false;
      }
      RegI32 rs = popI32();
      masm.splatX8(rs, rd);
      free(rs);
      break;
    }
    case Scalar::Uint32: {
      if (!loadCommon(access, AccessCheck(), ValType::I32)) {
        return false;
      }
      RegI32 rs = popI32();
      masm.splatX4(rs, rd);
      free(rs);
      break;
    }
    case Scalar::Int64: {
      if (!loadCommon(access, AccessCheck(), ValType::I64)) {
        return false;
      }
      RegI64 rs = popI64();
      masm.splatX2(rs, rd);
      free(rs);
      break;
    }
    default:
      MOZ_CRASH();
  }
  pushV128(rd);
  return true;
}

bool BaseCompiler::loadZero(MemoryAccessDesc* access) {
  access->setZeroExtendSimd128Load();
  return loadCommon(access, AccessCheck(), ValType::V128);
}

bool BaseCompiler::loadExtend(MemoryAccessDesc* access, Scalar::Type viewType) {
  if (!loadCommon(access, AccessCheck(), ValType::I64)) {
    return false;
  }

  RegI64 rs = popI64();
  RegV128 rd = needV128();
  masm.moveGPR64ToDouble(rs, rd);
  switch (viewType) {
    case Scalar::Int8:
      masm.widenLowInt8x16(rd, rd);
      break;
    case Scalar::Uint8:
      masm.unsignedWidenLowInt8x16(rd, rd);
      break;
    case Scalar::Int16:
      masm.widenLowInt16x8(rd, rd);
      break;
    case Scalar::Uint16:
      masm.unsignedWidenLowInt16x8(rd, rd);
      break;
    case Scalar::Int32:
      masm.widenLowInt32x4(rd, rd);
      break;
    case Scalar::Uint32:
      masm.unsignedWidenLowInt32x4(rd, rd);
      break;
    default:
      MOZ_CRASH();
  }
  freeI64(rs);
  pushV128(rd);

  return true;
}

bool BaseCompiler::loadLane(MemoryAccessDesc* access, uint32_t laneIndex) {
  ValType type = access->type() == Scalar::Int64 ? ValType::I64 : ValType::I32;

  RegV128 rsd = popV128();
  if (!loadCommon(access, AccessCheck(), type)) {
    return false;
  }

  if (type == ValType::I32) {
    RegI32 rs = popI32();
    switch (access->type()) {
      case Scalar::Uint8:
        masm.replaceLaneInt8x16(laneIndex, rs, rsd);
        break;
      case Scalar::Uint16:
        masm.replaceLaneInt16x8(laneIndex, rs, rsd);
        break;
      case Scalar::Int32:
        masm.replaceLaneInt32x4(laneIndex, rs, rsd);
        break;
      default:
        MOZ_CRASH("unsupported access type");
    }
    freeI32(rs);
  } else {
    MOZ_ASSERT(type == ValType::I64);
    RegI64 rs = popI64();
    masm.replaceLaneInt64x2(laneIndex, rs, rsd);
    freeI64(rs);
  }

  pushV128(rsd);

  return true;
}

bool BaseCompiler::storeLane(MemoryAccessDesc* access, uint32_t laneIndex) {
  ValType type = access->type() == Scalar::Int64 ? ValType::I64 : ValType::I32;

  RegV128 rs = popV128();
  if (type == ValType::I32) {
    RegI32 tmp = needI32();
    switch (access->type()) {
      case Scalar::Uint8:
        masm.extractLaneInt8x16(laneIndex, rs, tmp);
        break;
      case Scalar::Uint16:
        masm.extractLaneInt16x8(laneIndex, rs, tmp);
        break;
      case Scalar::Int32:
        masm.extractLaneInt32x4(laneIndex, rs, tmp);
        break;
      default:
        MOZ_CRASH("unsupported laneSize");
    }
    pushI32(tmp);
  } else {
    MOZ_ASSERT(type == ValType::I64);
    RegI64 tmp = needI64();
    masm.extractLaneInt64x2(laneIndex, rs, tmp);
    pushI64(tmp);
  }
  freeV128(rs);

  return storeCommon(access, AccessCheck(), type);
}
#endif  // ENABLE_WASM_SIMD

}  // namespace wasm
}  // namespace js
