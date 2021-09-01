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

#include "wasm/WasmBCFrame.h"

#include "wasm/WasmBaselineCompile.h"  // For BaseLocalIter
#include "wasm/WasmBCClass.h"

#include "jit/MacroAssembler-inl.h"
#include "wasm/WasmBCClass-inl.h"
#include "wasm/WasmBCCodegen-inl.h"
#include "wasm/WasmBCRegDefs-inl.h"
#include "wasm/WasmBCRegMgmt-inl.h"
#include "wasm/WasmBCStkMgmt-inl.h"

namespace js {
namespace wasm {

BaseLocalIter::BaseLocalIter(const ValTypeVector& locals,
                             const ArgTypeVector& args, bool debugEnabled)
    : locals_(locals),
      args_(args),
      argsIter_(args_),
      index_(0),
      frameSize_(0),
      nextFrameSize_(debugEnabled ? DebugFrame::offsetOfFrame() : 0),
      frameOffset_(INT32_MAX),
      stackResultPointerOffset_(INT32_MAX),
      mirType_(MIRType::Undefined),
      done_(false) {
  MOZ_ASSERT(args.lengthWithoutStackResults() <= locals.length());
  settle();
}

int32_t BaseLocalIter::pushLocal(size_t nbytes) {
  MOZ_ASSERT(nbytes % 4 == 0 && nbytes <= 16);
  nextFrameSize_ = AlignBytes(frameSize_, nbytes) + nbytes;
  return nextFrameSize_;  // Locals grow down so capture base address.
}

void BaseLocalIter::settle() {
  MOZ_ASSERT(!done_);
  frameSize_ = nextFrameSize_;

  if (!argsIter_.done()) {
    mirType_ = argsIter_.mirType();
    MIRType concreteType = mirType_;
    switch (mirType_) {
      case MIRType::StackResults:
        // The pointer to stack results is handled like any other argument:
        // either addressed in place if it is passed on the stack, or we spill
        // it in the frame if it's in a register.
        MOZ_ASSERT(args_.isSyntheticStackResultPointerArg(index_));
        concreteType = MIRType::Pointer;
        [[fallthrough]];
      case MIRType::Int32:
      case MIRType::Int64:
      case MIRType::Double:
      case MIRType::Float32:
      case MIRType::RefOrNull:
#ifdef ENABLE_WASM_SIMD
      case MIRType::Simd128:
#endif
        if (argsIter_->argInRegister()) {
          frameOffset_ = pushLocal(MIRTypeToSize(concreteType));
        } else {
          frameOffset_ = -(argsIter_->offsetFromArgBase() + sizeof(Frame));
        }
        break;
      default:
        MOZ_CRASH("Argument type");
    }
    if (mirType_ == MIRType::StackResults) {
      stackResultPointerOffset_ = frameOffset();
      // Advance past the synthetic stack result pointer argument and fall
      // through to the next case.
      argsIter_++;
      frameSize_ = nextFrameSize_;
      MOZ_ASSERT(argsIter_.done());
    } else {
      return;
    }
  }

  if (index_ < locals_.length()) {
    switch (locals_[index_].kind()) {
      case ValType::I32:
      case ValType::I64:
      case ValType::F32:
      case ValType::F64:
#ifdef ENABLE_WASM_SIMD
      case ValType::V128:
#endif
      case ValType::Ref:
        // TODO/AnyRef-boxing: With boxed immediates and strings, the
        // debugger must be made aware that AnyRef != Pointer.
        ASSERT_ANYREF_IS_JSOBJECT;
        mirType_ = ToMIRType(locals_[index_]);
        frameOffset_ = pushLocal(MIRTypeToSize(mirType_));
        break;
      default:
        MOZ_CRASH("Compiler bug: Unexpected local type");
    }
    return;
  }

  done_ = true;
}

void BaseLocalIter::operator++(int) {
  MOZ_ASSERT(!done_);
  index_++;
  if (!argsIter_.done()) {
    argsIter_++;
  }
  settle();
}

bool BaseCompiler::createStackMap(const char* who) {
  const ExitStubMapVector noExtras;
  return stackMapGenerator_.createStackMap(who, noExtras, masm.currentOffset(),
                                           HasDebugFrameWithLiveRefs::No, stk_);
}

bool BaseCompiler::createStackMap(const char* who, CodeOffset assemblerOffset) {
  const ExitStubMapVector noExtras;
  return stackMapGenerator_.createStackMap(who, noExtras,
                                           assemblerOffset.offset(),
                                           HasDebugFrameWithLiveRefs::No, stk_);
}

bool BaseCompiler::createStackMap(
    const char* who, HasDebugFrameWithLiveRefs debugFrameWithLiveRefs) {
  const ExitStubMapVector noExtras;
  return stackMapGenerator_.createStackMap(who, noExtras, masm.currentOffset(),
                                           debugFrameWithLiveRefs, stk_);
}

bool BaseCompiler::createStackMap(
    const char* who, const ExitStubMapVector& extras, uint32_t assemblerOffset,
    HasDebugFrameWithLiveRefs debugFrameWithLiveRefs) {
  return stackMapGenerator_.createStackMap(who, extras, assemblerOffset,
                                           debugFrameWithLiveRefs, stk_);
}

void BaseStackFrame::zeroLocals(BaseRegAlloc* ra) {
  MOZ_ASSERT(varLow_ != UINT32_MAX);

  if (varLow_ == varHigh_) {
    return;
  }

  static const uint32_t wordSize = sizeof(void*);

  // The adjustments to 'low' by the size of the item being stored compensates
  // for the fact that locals offsets are the offsets from Frame to the bytes
  // directly "above" the locals in the locals area.  See comment at Local.

  // On 64-bit systems we may have 32-bit alignment for the local area as it
  // may be preceded by parameters and prologue/debug data.

  uint32_t low = varLow_;
  if (low % wordSize) {
    masm.store32(Imm32(0), Address(sp_, localOffset(low + 4)));
    low += 4;
  }
  MOZ_ASSERT(low % wordSize == 0);

  const uint32_t high = AlignBytes(varHigh_, wordSize);

  // An UNROLL_LIMIT of 16 is chosen so that we only need an 8-bit signed
  // immediate to represent the offset in the store instructions in the loop
  // on x64.

  const uint32_t UNROLL_LIMIT = 16;
  const uint32_t initWords = (high - low) / wordSize;
  const uint32_t tailWords = initWords % UNROLL_LIMIT;
  const uint32_t loopHigh = high - (tailWords * wordSize);

  // With only one word to initialize, just store an immediate zero.

  if (initWords == 1) {
    masm.storePtr(ImmWord(0), Address(sp_, localOffset(low + wordSize)));
    return;
  }

  // For other cases, it's best to have a zero in a register.
  //
  // One can do more here with SIMD registers (store 16 bytes at a time) or
  // with instructions like STRD on ARM (store 8 bytes at a time), but that's
  // for another day.

  RegI32 zero = ra->needI32();
  masm.mov(ImmWord(0), zero);

  // For the general case we want to have a loop body of UNROLL_LIMIT stores
  // and then a tail of less than UNROLL_LIMIT stores.  When initWords is less
  // than 2*UNROLL_LIMIT the loop trip count is at most 1 and there is no
  // benefit to having the pointer calculations and the compare-and-branch.
  // So we completely unroll when we have initWords < 2 * UNROLL_LIMIT.  (In
  // this case we'll end up using 32-bit offsets on x64 for up to half of the
  // stores, though.)

  // Fully-unrolled case.

  if (initWords < 2 * UNROLL_LIMIT) {
    for (uint32_t i = low; i < high; i += wordSize) {
      masm.storePtr(zero, Address(sp_, localOffset(i + wordSize)));
    }
    ra->freeI32(zero);
    return;
  }

  // Unrolled loop with a tail. Stores will use negative offsets. That's OK
  // for x86 and ARM, at least.

  // Compute pointer to the highest-addressed slot on the frame.
  RegI32 p = ra->needI32();
  masm.computeEffectiveAddress(Address(sp_, localOffset(low + wordSize)), p);

  // Compute pointer to the lowest-addressed slot on the frame that will be
  // initialized by the loop body.
  RegI32 lim = ra->needI32();
  masm.computeEffectiveAddress(Address(sp_, localOffset(loopHigh + wordSize)),
                               lim);

  // The loop body.  Eventually we'll have p == lim and exit the loop.
  Label again;
  masm.bind(&again);
  for (uint32_t i = 0; i < UNROLL_LIMIT; ++i) {
    masm.storePtr(zero, Address(p, -(wordSize * i)));
  }
  masm.subPtr(Imm32(UNROLL_LIMIT * wordSize), p);
  masm.branchPtr(Assembler::LessThan, lim, p, &again);

  // The tail.
  for (uint32_t i = 0; i < tailWords; ++i) {
    masm.storePtr(zero, Address(p, -(wordSize * i)));
  }

  ra->freeI32(p);
  ra->freeI32(lim);
  ra->freeI32(zero);
}

}  // namespace wasm
}  // namespace js
