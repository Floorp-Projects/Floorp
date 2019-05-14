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

/*
 * [SMDOC] WebAssembly baseline compiler (RabaldrMonkey)
 *
 * General assumptions for 32-bit vs 64-bit code:
 *
 * - A 32-bit register can be extended in-place to a 64-bit register on 64-bit
 *   systems.
 *
 * - Code that knows that Register64 has a '.reg' member on 64-bit systems and
 *   '.high' and '.low' members on 32-bit systems, or knows the implications
 *   thereof, is #ifdef JS_PUNBOX64.  All other code is #if(n)?def JS_64BIT.
 *
 *
 * Coding standards:
 *
 * - In "small" code generating functions (eg emitMultiplyF64, emitQuotientI32,
 *   and surrounding functions; most functions fall into this class) where the
 *   meaning is obvious:
 *
 *   - if there is a single source + destination register, it is called 'r'
 *   - if there is one source and a different destination, they are called 'rs'
 *     and 'rd'
 *   - if there is one source + destination register and another source register
 *     they are called 'r' and 'rs'
 *   - if there are two source registers and a destination register they are
 *     called 'rs0', 'rs1', and 'rd'.
 *
 * - Generic temp registers are named /temp[0-9]?/ not /tmp[0-9]?/.
 *
 * - Registers can be named non-generically for their function ('rp' for the
 *   'pointer' register and 'rv' for the 'value' register are typical) and those
 *   names may or may not have an 'r' prefix.
 *
 * - "Larger" code generating functions make their own rules.
 *
 *
 * General status notes:
 *
 * "FIXME" indicates a known or suspected bug.  Always has a bug#.
 *
 * "TODO" indicates an opportunity for a general improvement, with an additional
 * tag to indicate the area of improvement.  Usually has a bug#.
 *
 * There are lots of machine dependencies here but they are pretty well isolated
 * to a segment of the compiler.  Many dependencies will eventually be factored
 * into the MacroAssembler layer and shared with other code generators.
 *
 *
 * High-value compiler performance improvements:
 *
 * - (Bug 1316802) The specific-register allocator (the needI32(r), needI64(r)
 *   etc methods) can avoid syncing the value stack if the specific register is
 *   in use but there is a free register to shuffle the specific register into.
 *   (This will also improve the generated code.)  The sync happens often enough
 *   here to show up in profiles, because it is triggered by integer multiply
 *   and divide.
 *
 *
 * High-value code generation improvements:
 *
 * - (Bug 1316804) brTable pessimizes by always dispatching to code that pops
 *   the stack and then jumps to the code for the target case.  If no cleanup is
 *   needed we could just branch conditionally to the target; if the same amount
 *   of cleanup is needed for all cases then the cleanup can be done before the
 *   dispatch.  Both are highly likely.
 *
 * - (Bug 1316806) Register management around calls: At the moment we sync the
 *   value stack unconditionally (this is simple) but there are probably many
 *   common cases where we could instead save/restore live caller-saves
 *   registers and perform parallel assignment into argument registers.  This
 *   may be important if we keep some locals in registers.
 *
 * - (Bug 1316808) Allocate some locals to registers on machines where there are
 *   enough registers.  This is probably hard to do well in a one-pass compiler
 *   but it might be that just keeping register arguments and the first few
 *   locals in registers is a viable strategy; another (more general) strategy
 *   is caching locals in registers in straight-line code.  Such caching could
 *   also track constant values in registers, if that is deemed valuable.  A
 *   combination of techniques may be desirable: parameters and the first few
 *   locals could be cached on entry to the function but not statically assigned
 *   to registers throughout.
 *
 *   (On a large corpus of code it should be possible to compute, for every
 *   signature comprising the types of parameters and locals, and using a static
 *   weight for loops, a list in priority order of which parameters and locals
 *   that should be assigned to registers.  Or something like that.  Wasm makes
 *   this simple.  Static assignments are desirable because they are not flushed
 *   to memory by the pre-block sync() call.)
 */

#include "wasm/WasmBaselineCompile.h"

#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"

#include <utility>

#include "jit/AtomicOp.h"
#include "jit/IonTypes.h"
#include "jit/JitAllocPolicy.h"
#include "jit/Label.h"
#include "jit/MacroAssembler.h"
#include "jit/MIR.h"
#include "jit/RegisterAllocator.h"
#include "jit/Registers.h"
#include "jit/RegisterSets.h"
#if defined(JS_CODEGEN_ARM)
#  include "jit/arm/Assembler-arm.h"
#endif
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
#  include "jit/x86-shared/Architecture-x86-shared.h"
#  include "jit/x86-shared/Assembler-x86-shared.h"
#endif
#if defined(JS_CODEGEN_MIPS32)
#  include "jit/mips-shared/Assembler-mips-shared.h"
#  include "jit/mips32/Assembler-mips32.h"
#endif
#if defined(JS_CODEGEN_MIPS64)
#  include "jit/mips-shared/Assembler-mips-shared.h"
#  include "jit/mips64/Assembler-mips64.h"
#endif

#include "wasm/WasmGC.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmOpIter.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmValidate.h"

#include "jit/MacroAssembler-inl.h"

using mozilla::DebugOnly;
using mozilla::FloorLog2;
using mozilla::IsPowerOfTwo;
using mozilla::Maybe;

namespace js {
namespace wasm {

using namespace js::jit;

using HandleNaNSpecially = bool;
using InvertBranch = bool;
using IsKnownNotZero = bool;
using IsUnsigned = bool;
using NeedsBoundsCheck = bool;
using PopStack = bool;
using WantResult = bool;
using ZeroOnOverflow = bool;

class BaseStackFrame;

// Two flags, useABI and interModule, control how calls are made.
//
// UseABI::Wasm implies that the Tls/Heap/Global registers are nonvolatile,
// except when InterModule::True is also set, when they are volatile.
//
// UseABI::Builtin implies that the Tls/Heap/Global registers are volatile.
// In this case, we require InterModule::False.  The calling convention
// is otherwise like UseABI::Wasm.
//
// UseABI::System implies that the Tls/Heap/Global registers are volatile.
// Additionally, the parameter passing mechanism may be slightly different from
// the UseABI::Wasm convention.
//
// When the Tls/Heap/Global registers are not volatile, the baseline compiler
// will restore the Tls register from its save slot before the call, since the
// baseline compiler uses the Tls register for other things.
//
// When those registers are volatile, the baseline compiler will reload them
// after the call (it will restore the Tls register from the save slot and load
// the other two from the Tls data).

enum class UseABI { Wasm, Builtin, System };
enum class InterModule { False = false, True = true };

#if defined(JS_CODEGEN_NONE)
#  define RABALDR_SCRATCH_I32
#  define RABALDR_SCRATCH_F32
#  define RABALDR_SCRATCH_F64

static const Register RabaldrScratchI32 = Register::Invalid();
static const FloatRegister RabaldrScratchF32 = InvalidFloatReg;
static const FloatRegister RabaldrScratchF64 = InvalidFloatReg;
#endif

#ifdef JS_CODEGEN_ARM64
#  define RABALDR_CHUNKY_STACK
#  define RABALDR_SCRATCH_I32
#  define RABALDR_SCRATCH_F32
#  define RABALDR_SCRATCH_F64
#  define RABALDR_SCRATCH_F32_ALIASES_F64

static const Register RabaldrScratchI32 = Register::FromCode(15);

// Note, the float scratch regs cannot be registers that are used for parameter
// passing in any ABI we use.  Argregs tend to be low-numbered; register 30
// should be safe.

static constexpr FloatRegister RabaldrScratchF32 =
    FloatRegister(30, FloatRegisters::Single);
static constexpr FloatRegister RabaldrScratchF64 =
    FloatRegister(30, FloatRegisters::Double);

static_assert(RabaldrScratchF32 != ScratchFloat32Reg, "Too busy");
static_assert(RabaldrScratchF64 != ScratchDoubleReg, "Too busy");
#endif

#ifdef JS_CODEGEN_X86
// The selection of EBX here steps gingerly around: the need for EDX
// to be allocatable for multiply/divide; ECX to be allocatable for
// shift/rotate; EAX (= ReturnReg) to be allocatable as the joinreg;
// EBX not being one of the WasmTableCall registers; and needing a
// temp register for load/store that has a single-byte persona.
//
// The compiler assumes that RabaldrScratchI32 has a single-byte
// persona.  Code for 8-byte atomic operations assumes that
// RabaldrScratchI32 is in fact ebx.

#  define RABALDR_SCRATCH_I32
static const Register RabaldrScratchI32 = ebx;

#  define RABALDR_INT_DIV_I64_CALLOUT
#endif

#ifdef JS_CODEGEN_ARM
// We use our own scratch register, because the macro assembler uses
// the regular scratch register(s) pretty liberally.  We could
// work around that in several cases but the mess does not seem
// worth it yet.  CallTempReg2 seems safe.

#  define RABALDR_SCRATCH_I32
static const Register RabaldrScratchI32 = CallTempReg2;

#  define RABALDR_INT_DIV_I64_CALLOUT
#  define RABALDR_I64_TO_FLOAT_CALLOUT
#  define RABALDR_FLOAT_TO_I64_CALLOUT
#endif

#ifdef JS_CODEGEN_MIPS32
#  define RABALDR_SCRATCH_I32
static const Register RabaldrScratchI32 = CallTempReg2;

#  define RABALDR_INT_DIV_I64_CALLOUT
#  define RABALDR_I64_TO_FLOAT_CALLOUT
#  define RABALDR_FLOAT_TO_I64_CALLOUT
#endif

#ifdef JS_CODEGEN_MIPS64
#  define RABALDR_SCRATCH_I32
static const Register RabaldrScratchI32 = CallTempReg2;
#endif

#ifdef RABALDR_SCRATCH_F32_ALIASES_F64
#  if !defined(RABALDR_SCRATCH_F32) || !defined(RABALDR_SCRATCH_F64)
#    error "Bad configuration"
#  endif
#endif

template <MIRType t>
struct RegTypeOf {
  static_assert(t == MIRType::Float32 || t == MIRType::Double,
                "Float mask type");
};

template <>
struct RegTypeOf<MIRType::Float32> {
  static constexpr RegTypeName value = RegTypeName::Float32;
};
template <>
struct RegTypeOf<MIRType::Double> {
  static constexpr RegTypeName value = RegTypeName::Float64;
};

// The strongly typed register wrappers are especially useful to distinguish
// float registers from double registers, but they also clearly distinguish
// 32-bit registers from 64-bit register pairs on 32-bit systems.

struct RegI32 : public Register {
  RegI32() : Register(Register::Invalid()) {}
  explicit RegI32(Register reg) : Register(reg) {}
  bool isValid() const { return *this != Invalid(); }
  bool isInvalid() const { return !isValid(); }
  static RegI32 Invalid() { return RegI32(Register::Invalid()); }
};

struct RegI64 : public Register64 {
  RegI64() : Register64(Register64::Invalid()) {}
  explicit RegI64(Register64 reg) : Register64(reg) {}
  bool isValid() const { return *this != Invalid(); }
  bool isInvalid() const { return !isValid(); }
  static RegI64 Invalid() { return RegI64(Register64::Invalid()); }
};

struct RegPtr : public Register {
  RegPtr() : Register(Register::Invalid()) {}
  explicit RegPtr(Register reg) : Register(reg) {}
  bool isValid() const { return *this != Invalid(); }
  bool isInvalid() const { return !isValid(); }
  static RegPtr Invalid() { return RegPtr(Register::Invalid()); }
};

struct RegF32 : public FloatRegister {
  RegF32() : FloatRegister() {}
  explicit RegF32(FloatRegister reg) : FloatRegister(reg) {}
  bool isValid() const { return *this != Invalid(); }
  bool isInvalid() const { return !isValid(); }
  static RegF32 Invalid() { return RegF32(InvalidFloatReg); }
};

struct RegF64 : public FloatRegister {
  RegF64() : FloatRegister() {}
  explicit RegF64(FloatRegister reg) : FloatRegister(reg) {}
  bool isValid() const { return *this != Invalid(); }
  bool isInvalid() const { return !isValid(); }
  static RegF64 Invalid() { return RegF64(InvalidFloatReg); }
};

struct AnyReg {
  union {
    RegI32 i32_;
    RegI64 i64_;
    RegPtr ref_;
    RegF32 f32_;
    RegF64 f64_;
  };

  enum { I32, I64, REF, F32, F64 } tag;

  explicit AnyReg(RegI32 r) {
    tag = I32;
    i32_ = r;
  }
  explicit AnyReg(RegI64 r) {
    tag = I64;
    i64_ = r;
  }
  explicit AnyReg(RegF32 r) {
    tag = F32;
    f32_ = r;
  }
  explicit AnyReg(RegF64 r) {
    tag = F64;
    f64_ = r;
  }
  explicit AnyReg(RegPtr r) {
    tag = REF;
    ref_ = r;
  }

  RegI32 i32() const {
    MOZ_ASSERT(tag == I32);
    return i32_;
  }
  RegI64 i64() const {
    MOZ_ASSERT(tag == I64);
    return i64_;
  }
  RegF32 f32() const {
    MOZ_ASSERT(tag == F32);
    return f32_;
  }
  RegF64 f64() const {
    MOZ_ASSERT(tag == F64);
    return f64_;
  }
  RegPtr ref() const {
    MOZ_ASSERT(tag == REF);
    return ref_;
  }

  AnyRegister any() const {
    switch (tag) {
      case F32:
        return AnyRegister(f32_);
      case F64:
        return AnyRegister(f64_);
      case I32:
        return AnyRegister(i32_);
      case I64:
#ifdef JS_PUNBOX64
        return AnyRegister(i64_.reg);
#else
        // The compiler is written so that this is never needed: any() is
        // called on arbitrary registers for asm.js but asm.js does not have
        // 64-bit ints.  For wasm, any() is called on arbitrary registers
        // only on 64-bit platforms.
        MOZ_CRASH("AnyReg::any() on 32-bit platform");
#endif
      case REF:
        MOZ_CRASH("AnyReg::any() not implemented for ref types");
      default:
        MOZ_CRASH();
    }
    // Work around GCC 5 analysis/warning bug.
    MOZ_CRASH("AnyReg::any(): impossible case");
  }
};

// Platform-specific registers.
//
// All platforms must define struct SpecificRegs.  All 32-bit platforms must
// have an abiReturnRegI64 member in that struct.

#if defined(JS_CODEGEN_X64)
struct SpecificRegs {
  RegI32 eax, ecx, edx, edi, esi;
  RegI64 rax, rcx, rdx;

  SpecificRegs()
      : eax(RegI32(js::jit::eax)),
        ecx(RegI32(js::jit::ecx)),
        edx(RegI32(js::jit::edx)),
        edi(RegI32(js::jit::edi)),
        esi(RegI32(js::jit::esi)),
        rax(RegI64(Register64(js::jit::rax))),
        rcx(RegI64(Register64(js::jit::rcx))),
        rdx(RegI64(Register64(js::jit::rdx))) {}
};
#elif defined(JS_CODEGEN_X86)
struct SpecificRegs {
  RegI32 eax, ecx, edx, edi, esi;
  RegI64 ecx_ebx, edx_eax, abiReturnRegI64;

  SpecificRegs()
      : eax(RegI32(js::jit::eax)),
        ecx(RegI32(js::jit::ecx)),
        edx(RegI32(js::jit::edx)),
        edi(RegI32(js::jit::edi)),
        esi(RegI32(js::jit::esi)),
        ecx_ebx(RegI64(Register64(js::jit::ecx, js::jit::ebx))),
        edx_eax(RegI64(Register64(js::jit::edx, js::jit::eax))),
        abiReturnRegI64(edx_eax) {}
};
#elif defined(JS_CODEGEN_ARM)
struct SpecificRegs {
  RegI64 abiReturnRegI64;

  SpecificRegs() : abiReturnRegI64(ReturnReg64) {}
};
#elif defined(JS_CODEGEN_ARM64)
struct SpecificRegs {};
#elif defined(JS_CODEGEN_MIPS32)
struct SpecificRegs {
  RegI64 abiReturnRegI64;

  SpecificRegs() : abiReturnRegI64(ReturnReg64) {}
};
#elif defined(JS_CODEGEN_MIPS64)
struct SpecificRegs {};
#else
struct SpecificRegs {
#  ifndef JS_64BIT
  RegI64 abiReturnRegI64;
#  endif

  SpecificRegs() { MOZ_CRASH("BaseCompiler porting interface: SpecificRegs"); }
};
#endif

class BaseCompilerInterface {
 public:
  // Spill all spillable registers.
  //
  // TODO / OPTIMIZE (Bug 1316802): It's possible to do better here by
  // spilling only enough registers to satisfy current needs.
  virtual void sync() = 0;
};

// Register allocator.

class BaseRegAlloc {
  // Notes on float register allocation.
  //
  // The general rule in SpiderMonkey is that float registers can alias double
  // registers, but there are predicates to handle exceptions to that rule:
  // hasUnaliasedDouble() and hasMultiAlias().  The way aliasing actually
  // works is platform dependent and exposed through the aliased(n, &r)
  // predicate, etc.
  //
  //  - hasUnaliasedDouble(): on ARM VFPv3-D32 there are double registers that
  //    cannot be treated as float.
  //  - hasMultiAlias(): on ARM and MIPS a double register aliases two float
  //    registers.
  //
  // On some platforms (x86, x64, ARM64) but not all (ARM)
  // ScratchFloat32Register is the same as ScratchDoubleRegister.
  //
  // It's a basic invariant of the AllocatableRegisterSet that it deals
  // properly with aliasing of registers: if s0 or s1 are allocated then d0 is
  // not allocatable; if s0 and s1 are freed individually then d0 becomes
  // allocatable.

  BaseCompilerInterface& bc;
  AllocatableGeneralRegisterSet availGPR;
  AllocatableFloatRegisterSet availFPU;
#ifdef DEBUG
  AllocatableGeneralRegisterSet
      allGPR;  // The registers available to the compiler
  AllocatableFloatRegisterSet
      allFPU;  //   after removing ScratchReg, HeapReg, etc
  uint32_t scratchTaken;
#endif
#ifdef JS_CODEGEN_X86
  AllocatableGeneralRegisterSet singleByteRegs;
#endif

  bool hasGPR() { return !availGPR.empty(); }

  bool hasGPR64() {
#ifdef JS_PUNBOX64
    return !availGPR.empty();
#else
    if (availGPR.empty()) {
      return false;
    }
    Register r = allocGPR();
    bool available = !availGPR.empty();
    freeGPR(r);
    return available;
#endif
  }

  template <MIRType t>
  bool hasFPU() {
    return availFPU.hasAny<RegTypeOf<t>::value>();
  }

  bool isAvailableGPR(Register r) { return availGPR.has(r); }

  bool isAvailableFPU(FloatRegister r) { return availFPU.has(r); }

  void allocGPR(Register r) {
    MOZ_ASSERT(isAvailableGPR(r));
    availGPR.take(r);
  }

  Register allocGPR() {
    MOZ_ASSERT(hasGPR());
    return availGPR.takeAny();
  }

  void allocInt64(Register64 r) {
#ifdef JS_PUNBOX64
    allocGPR(r.reg);
#else
    allocGPR(r.low);
    allocGPR(r.high);
#endif
  }

  Register64 allocInt64() {
    MOZ_ASSERT(hasGPR64());
#ifdef JS_PUNBOX64
    return Register64(availGPR.takeAny());
#else
    Register high = availGPR.takeAny();
    Register low = availGPR.takeAny();
    return Register64(high, low);
#endif
  }

#ifdef JS_CODEGEN_ARM
  // r12 is normally the ScratchRegister and r13 is always the stack pointer,
  // so the highest possible pair has r10 as the even-numbered register.

  static constexpr uint32_t PAIR_LIMIT = 10;

  bool hasGPRPair() {
    for (uint32_t i = 0; i <= PAIR_LIMIT; i += 2) {
      if (isAvailableGPR(Register::FromCode(i)) &&
          isAvailableGPR(Register::FromCode(i + 1))) {
        return true;
      }
    }
    return false;
  }

  void allocGPRPair(Register* low, Register* high) {
    MOZ_ASSERT(hasGPRPair());
    for (uint32_t i = 0; i <= PAIR_LIMIT; i += 2) {
      if (isAvailableGPR(Register::FromCode(i)) &&
          isAvailableGPR(Register::FromCode(i + 1))) {
        *low = Register::FromCode(i);
        *high = Register::FromCode(i + 1);
        allocGPR(*low);
        allocGPR(*high);
        return;
      }
    }
    MOZ_CRASH("No pair");
  }
#endif

  void allocFPU(FloatRegister r) {
    MOZ_ASSERT(isAvailableFPU(r));
    availFPU.take(r);
  }

  template <MIRType t>
  FloatRegister allocFPU() {
    return availFPU.takeAny<RegTypeOf<t>::value>();
  }

  void freeGPR(Register r) { availGPR.add(r); }

  void freeInt64(Register64 r) {
#ifdef JS_PUNBOX64
    freeGPR(r.reg);
#else
    freeGPR(r.low);
    freeGPR(r.high);
#endif
  }

  void freeFPU(FloatRegister r) { availFPU.add(r); }

 public:
  explicit BaseRegAlloc(BaseCompilerInterface& bc)
      : bc(bc),
        availGPR(GeneralRegisterSet::All()),
        availFPU(FloatRegisterSet::All())
#ifdef DEBUG
        ,
        scratchTaken(0)
#endif
#ifdef JS_CODEGEN_X86
        ,
        singleByteRegs(GeneralRegisterSet(Registers::SingleByteRegs))
#endif
  {
    RegisterAllocator::takeWasmRegisters(availGPR);

    // Allocate any private scratch registers.
#if defined(RABALDR_SCRATCH_I32)
    if (RabaldrScratchI32 != RegI32::Invalid()) {
      availGPR.take(RabaldrScratchI32);
    }
#endif

#ifdef RABALDR_SCRATCH_F32_ALIASES_F64
    MOZ_ASSERT(RabaldrScratchF32 != InvalidFloatReg, "Float reg definition");
    MOZ_ASSERT(RabaldrScratchF64 != InvalidFloatReg, "Float reg definition");
#endif

#if defined(RABALDR_SCRATCH_F32) && !defined(RABALDR_SCRATCH_F32_ALIASES_F64)
    if (RabaldrScratchF32 != RegF32::Invalid()) {
      availFPU.take(RabaldrScratchF32);
    }
#endif

#if defined(RABALDR_SCRATCH_F64)
#  ifdef RABALDR_SCRATCH_F32_ALIASES_F64
    MOZ_ASSERT(availFPU.has(RabaldrScratchF32));
#  endif
    if (RabaldrScratchF64 != RegF64::Invalid()) {
      availFPU.take(RabaldrScratchF64);
    }
#  ifdef RABALDR_SCRATCH_F32_ALIASES_F64
    MOZ_ASSERT(!availFPU.has(RabaldrScratchF32));
#  endif
#endif

#ifdef DEBUG
    allGPR = availGPR;
    allFPU = availFPU;
#endif
  }

  enum class ScratchKind { I32 = 1, F32 = 2, F64 = 4 };

#ifdef DEBUG
  bool isScratchRegisterTaken(ScratchKind s) const {
    return (scratchTaken & uint32_t(s)) != 0;
  }

  void setScratchRegisterTaken(ScratchKind s, bool state) {
    if (state) {
      scratchTaken |= uint32_t(s);
    } else {
      scratchTaken &= ~uint32_t(s);
    }
  }
#endif

#ifdef JS_CODEGEN_X86
  bool isSingleByteI32(Register r) { return singleByteRegs.has(r); }
#endif

  bool isAvailableI32(RegI32 r) { return isAvailableGPR(r); }

  bool isAvailableI64(RegI64 r) {
#ifdef JS_PUNBOX64
    return isAvailableGPR(r.reg);
#else
    return isAvailableGPR(r.low) && isAvailableGPR(r.high);
#endif
  }

  bool isAvailablePtr(RegPtr r) { return isAvailableGPR(r); }

  bool isAvailableF32(RegF32 r) { return isAvailableFPU(r); }

  bool isAvailableF64(RegF64 r) { return isAvailableFPU(r); }

  // TODO / OPTIMIZE (Bug 1316802): Do not sync everything on allocation
  // failure, only as much as we need.

  MOZ_MUST_USE RegI32 needI32() {
    if (!hasGPR()) {
      bc.sync();
    }
    return RegI32(allocGPR());
  }

  void needI32(RegI32 specific) {
    if (!isAvailableI32(specific)) {
      bc.sync();
    }
    allocGPR(specific);
  }

  MOZ_MUST_USE RegI64 needI64() {
    if (!hasGPR64()) {
      bc.sync();
    }
    return RegI64(allocInt64());
  }

  void needI64(RegI64 specific) {
    if (!isAvailableI64(specific)) {
      bc.sync();
    }
    allocInt64(specific);
  }

  MOZ_MUST_USE RegPtr needPtr() {
    if (!hasGPR()) {
      bc.sync();
    }
    return RegPtr(allocGPR());
  }

  void needPtr(RegPtr specific) {
    if (!isAvailablePtr(specific)) {
      bc.sync();
    }
    allocGPR(specific);
  }

  MOZ_MUST_USE RegF32 needF32() {
    if (!hasFPU<MIRType::Float32>()) {
      bc.sync();
    }
    return RegF32(allocFPU<MIRType::Float32>());
  }

  void needF32(RegF32 specific) {
    if (!isAvailableF32(specific)) {
      bc.sync();
    }
    allocFPU(specific);
  }

  MOZ_MUST_USE RegF64 needF64() {
    if (!hasFPU<MIRType::Double>()) {
      bc.sync();
    }
    return RegF64(allocFPU<MIRType::Double>());
  }

  void needF64(RegF64 specific) {
    if (!isAvailableF64(specific)) {
      bc.sync();
    }
    allocFPU(specific);
  }

  void freeI32(RegI32 r) { freeGPR(r); }

  void freeI64(RegI64 r) { freeInt64(r); }

  void freePtr(RegPtr r) { freeGPR(r); }

  void freeF64(RegF64 r) { freeFPU(r); }

  void freeF32(RegF32 r) { freeFPU(r); }

#ifdef JS_CODEGEN_ARM
  MOZ_MUST_USE RegI64 needI64Pair() {
    if (!hasGPRPair()) {
      bc.sync();
    }
    Register low, high;
    allocGPRPair(&low, &high);
    return RegI64(Register64(high, low));
  }
#endif

#ifdef DEBUG
  friend class LeakCheck;

  class MOZ_RAII LeakCheck {
   private:
    const BaseRegAlloc& ra;
    AllocatableGeneralRegisterSet knownGPR_;
    AllocatableFloatRegisterSet knownFPU_;

   public:
    explicit LeakCheck(const BaseRegAlloc& ra) : ra(ra) {
      knownGPR_ = ra.availGPR;
      knownFPU_ = ra.availFPU;
    }

    ~LeakCheck() {
      MOZ_ASSERT(knownGPR_.bits() == ra.allGPR.bits());
      MOZ_ASSERT(knownFPU_.bits() == ra.allFPU.bits());
    }

    void addKnownI32(RegI32 r) { knownGPR_.add(r); }

    void addKnownI64(RegI64 r) {
#  ifdef JS_PUNBOX64
      knownGPR_.add(r.reg);
#  else
      knownGPR_.add(r.high);
      knownGPR_.add(r.low);
#  endif
    }

    void addKnownF32(RegF32 r) { knownFPU_.add(r); }

    void addKnownF64(RegF64 r) { knownFPU_.add(r); }

    void addKnownRef(RegPtr r) { knownGPR_.add(r); }
  };
#endif
};

// Scratch register abstractions.
//
// We define our own scratch registers when the platform doesn't provide what we
// need.  A notable use case is that we will need a private scratch register
// when the platform masm uses its scratch register very frequently (eg, ARM).

class BaseScratchRegister {
#ifdef DEBUG
  BaseRegAlloc& ra;
  BaseRegAlloc::ScratchKind kind_;

 public:
  explicit BaseScratchRegister(BaseRegAlloc& ra, BaseRegAlloc::ScratchKind kind)
      : ra(ra), kind_(kind) {
    MOZ_ASSERT(!ra.isScratchRegisterTaken(kind_));
    ra.setScratchRegisterTaken(kind_, true);
  }
  ~BaseScratchRegister() {
    MOZ_ASSERT(ra.isScratchRegisterTaken(kind_));
    ra.setScratchRegisterTaken(kind_, false);
  }
#else
 public:
  explicit BaseScratchRegister(BaseRegAlloc& ra,
                               BaseRegAlloc::ScratchKind kind) {}
#endif
};

#ifdef RABALDR_SCRATCH_F64
class ScratchF64 : public BaseScratchRegister {
 public:
  explicit ScratchF64(BaseRegAlloc& ra)
      : BaseScratchRegister(ra, BaseRegAlloc::ScratchKind::F64) {}
  operator RegF64() const { return RegF64(RabaldrScratchF64); }
};
#else
class ScratchF64 : public ScratchDoubleScope {
 public:
  explicit ScratchF64(MacroAssembler& m) : ScratchDoubleScope(m) {}
  operator RegF64() const { return RegF64(FloatRegister(*this)); }
};
#endif

#ifdef RABALDR_SCRATCH_F32
class ScratchF32 : public BaseScratchRegister {
 public:
  explicit ScratchF32(BaseRegAlloc& ra)
      : BaseScratchRegister(ra, BaseRegAlloc::ScratchKind::F32) {}
  operator RegF32() const { return RegF32(RabaldrScratchF32); }
};
#else
class ScratchF32 : public ScratchFloat32Scope {
 public:
  explicit ScratchF32(MacroAssembler& m) : ScratchFloat32Scope(m) {}
  operator RegF32() const { return RegF32(FloatRegister(*this)); }
};
#endif

#ifdef RABALDR_SCRATCH_I32
template <class RegType>
class ScratchGPR : public BaseScratchRegister {
 public:
  explicit ScratchGPR(BaseRegAlloc& ra)
      : BaseScratchRegister(ra, BaseRegAlloc::ScratchKind::I32) {}
  operator RegType() const { return RegType(RabaldrScratchI32); }
};
#else
template <class RegType>
class ScratchGPR : public ScratchRegisterScope {
 public:
  explicit ScratchGPR(MacroAssembler& m) : ScratchRegisterScope(m) {}
  operator RegType() const { return RegType(Register(*this)); }
};
#endif

using ScratchI32 = ScratchGPR<RegI32>;
using ScratchPtr = ScratchGPR<RegPtr>;

#if defined(JS_CODEGEN_X86)
// ScratchEBX is a mnemonic device: For some atomic ops we really need EBX,
// no other register will do.  And we would normally have to allocate that
// register using ScratchI32 since normally the scratch register is EBX.
// But the whole point of ScratchI32 is to hide that relationship.  By using
// the ScratchEBX alias, we document that at that point we require the
// scratch register to be EBX.
using ScratchEBX = ScratchI32;

// ScratchI8 is a mnemonic device: For some ops we need a register with a
// byte subregister.
using ScratchI8 = ScratchI32;
#endif

// The stack frame.
//
// The stack frame has four parts ("below" means at lower addresses):
//
//  - the Frame element;
//  - the Local area, including the DebugFrame element; allocated below the
//    header with various forms of alignment;
//  - the Dynamic area, comprising the temporary storage the compiler uses for
//    register spilling, allocated below the Local area;
//  - the Arguments area, comprising memory allocated for outgoing calls,
//    allocated below the Dynamic area.
//
//                 +============================+
//                 |    Incoming arg            |
//                 |    ...                     |
// --------------  +============================+
//                 |    Frame (fixed size)      |
// --------------  +============================+ <-------------------- FP
//          ^      |    DebugFrame (optional)   |    ^                ^^
//          |      +----------------------------+    |                ||
//    localSize    |    Local (static size)     |    |                ||
//          |      |    ...                     |    |        framePushed
//          v      |    (padding)               |    |                ||
// --------------  +============================+ currentStackHeight  ||
//          ^      |    Dynamic (variable size) |    |                ||
//   dynamicSize   |    ...                     |    |                ||
//          v      |    ...                     |    v                ||
// --------------  |    (free space, sometimes) | ---------           v|
//                 +============================+ <----- SP not-during calls
//                 |    Arguments (sometimes)   |                      |
//                 |    ...                     |                      v
//                 +============================+ <----- SP during calls
//
// The Frame is addressed off the stack pointer.  masm.framePushed() is always
// correct, and masm.getStackPointer() + masm.framePushed() always addresses the
// Frame, with the DebugFrame optionally below it.
//
// The Local area (including the DebugFrame) is laid out by BaseLocalIter and is
// allocated and deallocated by standard prologue and epilogue functions that
// manipulate the stack pointer, but it is accessed via BaseStackFrame.
//
// The Dynamic area is maintained by and accessed via BaseStackFrame.  On some
// systems (such as ARM64), the Dynamic memory may be allocated in chunks
// because the SP needs a specific alignment, and in this case there will
// normally be some free space directly above the SP.  The stack height does not
// include the free space, it reflects the logically used space only.
//
// The Arguments area is allocated and deallocated via BaseStackFrame (see
// comments later) but is accessed directly off the stack pointer.

// BaseLocalIter iterates over a vector of types of locals and provides offsets
// from the Frame address for those locals, and associated data.
//
// The implementation of BaseLocalIter is the property of the BaseStackFrame.
// But it is also exposed for eg the debugger to use.

BaseLocalIter::BaseLocalIter(const ValTypeVector& locals, size_t argsLength,
                             bool debugEnabled)
    : locals_(locals),
      argsLength_(argsLength),
      argsRange_(locals.begin(), argsLength),
      argsIter_(argsRange_),
      index_(0),
      localSize_(debugEnabled ? DebugFrame::offsetOfFrame() : 0),
      reservedSize_(localSize_),
      frameOffset_(UINT32_MAX),
      mirType_(MIRType::Undefined),
      done_(false) {
  MOZ_ASSERT(argsLength <= locals.length());
  settle();
}

int32_t BaseLocalIter::pushLocal(size_t nbytes) {
  MOZ_ASSERT(nbytes % 4 == 0 && nbytes <= 16);
  localSize_ = AlignBytes(localSize_, nbytes) + nbytes;
  return localSize_;  // Locals grow down so capture base address.
}

void BaseLocalIter::settle() {
  if (index_ < argsLength_) {
    MOZ_ASSERT(!argsIter_.done());
    mirType_ = argsIter_.mirType();
    switch (mirType_) {
      case MIRType::Int32:
      case MIRType::Int64:
      case MIRType::Double:
      case MIRType::Float32:
      case MIRType::RefOrNull:
        if (argsIter_->argInRegister()) {
          frameOffset_ = pushLocal(MIRTypeToSize(mirType_));
        } else {
          frameOffset_ = -(argsIter_->offsetFromArgBase() + sizeof(Frame));
        }
        break;
      default:
        MOZ_CRASH("Argument type");
    }
    return;
  }

  MOZ_ASSERT(argsIter_.done());
  if (index_ < locals_.length()) {
    switch (locals_[index_].code()) {
      case ValType::I32:
      case ValType::I64:
      case ValType::F32:
      case ValType::F64:
      case ValType::Ref:
      case ValType::FuncRef:
      case ValType::AnyRef:
        // TODO/AnyRef-boxing: With boxed immediates and strings, the
        // debugger must be made aware that AnyRef != Pointer.
        ASSERT_ANYREF_IS_JSOBJECT;
        mirType_ = ToMIRType(locals_[index_]);
        frameOffset_ = pushLocal(MIRTypeToSize(mirType_));
        break;
      case ValType::NullRef:
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

// Abstraction of the height of the stack frame, to avoid type confusion.

class StackHeight {
  friend class BaseStackFrameAllocator;

  uint32_t height;

 public:
  explicit StackHeight(uint32_t h) : height(h) {}
  static StackHeight Invalid() { return StackHeight(UINT32_MAX); }
  bool isValid() const { return height != UINT32_MAX; }
};

// Abstraction of the baseline compiler's stack frame (except for the Frame /
// DebugFrame parts).  See comments above for more.  Remember, "below" on the
// stack means at lower addresses.
//
// The abstraction is split into two parts: BaseStackFrameAllocator is
// responsible for allocating and deallocating space on the stack and for
// performing computations that are affected by how the allocation is performed;
// BaseStackFrame then provides a pleasant interface for stack frame management.

class BaseStackFrameAllocator {
  MacroAssembler& masm;

#ifdef RABALDR_CHUNKY_STACK
  // On platforms that require the stack pointer to be aligned on a boundary
  // greater than the typical stack item (eg, ARM64 requires 16-byte alignment
  // but items are 8 bytes), allocate stack memory in chunks, and use a
  // separate stack height variable to track the effective stack pointer
  // within the allocated area.  Effectively, there's a variable amount of
  // free space directly above the stack pointer.  See diagram above.

  // The following must be true in order for the stack height to be
  // predictable at control flow joins:
  //
  // - The Local area is always aligned according to WasmStackAlignment, ie,
  //   masm.framePushed() % WasmStackAlignment is zero after allocating
  //   locals.
  //
  // - ChunkSize is always a multiple of WasmStackAlignment.
  //
  // - Pushing and popping are always in units of ChunkSize (hence preserving
  //   alignment).
  //
  // - The free space on the stack (masm.framePushed() - currentStackHeight_)
  //   is a predictable (nonnegative) amount.

  // As an optimization, we pre-allocate some space on the stack, the size of
  // this allocation is InitialChunk and it must be a multiple of ChunkSize.
  // It is allocated as part of the function prologue and deallocated as part
  // of the epilogue, along with the locals.
  //
  // If ChunkSize is too large then we risk overflowing the stack on simple
  // recursions with few live values where stack overflow should not be a
  // risk; if it is too small we spend too much time adjusting the stack
  // pointer.
  //
  // Good values for ChunkSize are the subject of future empirical analysis;
  // eight words is just an educated guess.

  static constexpr uint32_t ChunkSize = 8 * sizeof(void*);
  static constexpr uint32_t InitialChunk = ChunkSize;

  // The current logical height of the frame is
  //   currentStackHeight_ = localSize_ + dynamicSize
  // where dynamicSize is not accounted for explicitly and localSize_ also
  // includes size for the DebugFrame.
  //
  // The allocated size of the frame, provided by masm.framePushed(), is usually
  // larger than currentStackHeight_, notably at the beginning of execution when
  // we've allocated InitialChunk extra space.

  uint32_t currentStackHeight_;
#endif

  // Size of the Local area in bytes (stable after BaseCompiler::init() has
  // called BaseStackFrame::setupLocals(), which in turn calls
  // BaseStackFrameAllocator::setLocalSize()), always rounded to the proper
  // stack alignment.  The Local area is then allocated in beginFunction(),
  // following the allocation of the Header.  See onFixedStackAllocated()
  // below.

  uint32_t localSize_;

 protected:
  ///////////////////////////////////////////////////////////////////////////
  //
  // Initialization

  explicit BaseStackFrameAllocator(MacroAssembler& masm)
      : masm(masm),
#ifdef RABALDR_CHUNKY_STACK
        currentStackHeight_(0),
#endif
        localSize_(UINT32_MAX) {
  }

 protected:
  //////////////////////////////////////////////////////////////////////
  //
  // The Local area - the static part of the frame.

  // Record the size of the Local area, once it is known.

  void setLocalSize(uint32_t localSize) {
    MOZ_ASSERT(localSize == AlignBytes(localSize, sizeof(void*)),
               "localSize_ should be aligned to at least a pointer");
    MOZ_ASSERT(localSize_ == UINT32_MAX);
    localSize_ = localSize;
  }

  // Record the current stack height, after it has become stable in
  // beginFunction().  See also BaseStackFrame::onFixedStackAllocated().

  void onFixedStackAllocated() {
    MOZ_ASSERT(localSize_ != UINT32_MAX);
#ifdef RABALDR_CHUNKY_STACK
    currentStackHeight_ = localSize_;
#endif
  }

 public:
  // The fixed amount of memory, in bytes, allocated on the stack below the
  // Header for purposes such as locals and other fixed values.  Includes all
  // necessary alignment, and on ARM64 also the initial chunk for the working
  // stack memory.

  uint32_t fixedAllocSize() const {
    MOZ_ASSERT(localSize_ != UINT32_MAX);
#ifdef RABALDR_CHUNKY_STACK
    return localSize_ + InitialChunk;
#else
    return localSize_;
#endif
  }

#ifdef RABALDR_CHUNKY_STACK
  // The allocated frame size is frequently larger than the logical stack
  // height; we round up to a chunk boundary, and special case the initial
  // chunk.
  uint32_t framePushedForHeight(uint32_t logicalHeight) {
    if (logicalHeight <= fixedAllocSize()) {
      return fixedAllocSize();
    }
    return fixedAllocSize() +
           AlignBytes(logicalHeight - fixedAllocSize(), ChunkSize);
  }
#endif

 protected:
  //////////////////////////////////////////////////////////////////////
  //
  // The Dynamic area - the dynamic part of the frame, for spilling and saving
  // intermediate values.

  // Offset off of sp_ for the slot at stack area location `offset`.

  int32_t stackOffset(int32_t offset) { return masm.framePushed() - offset; }

#ifdef RABALDR_CHUNKY_STACK
  void pushChunkyBytes(uint32_t bytes) {
    MOZ_ASSERT(bytes <= ChunkSize);
    checkChunkyInvariants();
    if (masm.framePushed() - currentStackHeight_ < bytes) {
      masm.reserveStack(ChunkSize);
    }
    currentStackHeight_ += bytes;
    checkChunkyInvariants();
  }

  void popChunkyBytes(uint32_t bytes) {
    checkChunkyInvariants();
    currentStackHeight_ -= bytes;
    // Sometimes, popChunkyBytes() is used to pop a larger area, as when we drop
    // values consumed by a call, and we may need to drop several chunks.  But
    // never drop the initial chunk.  Crucially, the amount we drop is always an
    // integral number of chunks.
    uint32_t freeSpace = masm.framePushed() - currentStackHeight_;
    if (freeSpace >= ChunkSize) {
      uint32_t targetAllocSize = framePushedForHeight(currentStackHeight_);
      uint32_t amountToFree = masm.framePushed() - targetAllocSize;
      MOZ_ASSERT(amountToFree % ChunkSize == 0);
      if (amountToFree) {
        masm.freeStack(amountToFree);
      }
    }
    checkChunkyInvariants();
  }
#endif

  uint32_t currentStackHeight() const {
#ifdef RABALDR_CHUNKY_STACK
    return currentStackHeight_;
#else
    return masm.framePushed();
#endif
  }

 private:
#ifdef RABALDR_CHUNKY_STACK
  void checkChunkyInvariants() {
    MOZ_ASSERT(masm.framePushed() >= fixedAllocSize());
    MOZ_ASSERT(masm.framePushed() >= currentStackHeight_);
    MOZ_ASSERT(masm.framePushed() == fixedAllocSize() ||
               masm.framePushed() - currentStackHeight_ < ChunkSize);
    MOZ_ASSERT((masm.framePushed() - localSize_) % ChunkSize == 0);
  }
#endif

  // For a given stack height, return the appropriate size of the allocated
  // frame.

  uint32_t framePushedForHeight(StackHeight stackHeight) {
#ifdef RABALDR_CHUNKY_STACK
    // A more complicated adjustment is needed.
    return framePushedForHeight(stackHeight.height);
#else
    // The allocated frame size equals the stack height.
    return stackHeight.height;
#endif
  }

 public:
  // The current height of the stack area, not necessarily zero-based, in a
  // type-safe way.

  StackHeight stackHeight() const { return StackHeight(currentStackHeight()); }

  // Set the frame height to a previously recorded value.

  void setStackHeight(StackHeight amount) {
#ifdef RABALDR_CHUNKY_STACK
    currentStackHeight_ = amount.height;
    masm.setFramePushed(framePushedForHeight(amount));
    checkChunkyInvariants();
#else
    masm.setFramePushed(amount.height);
#endif
  }

  // The current height of the dynamic part of the stack area (ie, the backing
  // store for the evaluation stack), zero-based.

  uint32_t dynamicHeight() const { return currentStackHeight() - localSize_; }

  // Before branching to an outer control label, pop the execution stack to
  // the level expected by that region, but do not update masm.framePushed()
  // as that will happen as compilation leaves the block.
  //
  // Note these operate directly on the stack pointer register.

  void popStackBeforeBranch(StackHeight destStackHeight) {
    uint32_t framePushedHere = masm.framePushed();
    uint32_t framePushedThere = framePushedForHeight(destStackHeight);
    if (framePushedHere > framePushedThere) {
      masm.addToStackPtr(Imm32(framePushedHere - framePushedThere));
    }
  }

  bool willPopStackBeforeBranch(StackHeight destStackHeight) {
    uint32_t framePushedHere = masm.framePushed();
    uint32_t framePushedThere = framePushedForHeight(destStackHeight);
    return framePushedHere > framePushedThere;
  }

  // Before exiting a nested control region, pop the execution stack
  // to the level expected by the nesting region, and free the
  // stack.
  //
  // Note this operates on the stack height, which is not the same as the
  // stack pointer on chunky-stack systems; the stack pointer may or may not
  // change on such systems.

  void popStackOnBlockExit(StackHeight destStackHeight, bool deadCode) {
    uint32_t stackHeightHere = currentStackHeight();
    uint32_t stackHeightThere = destStackHeight.height;
    if (stackHeightHere > stackHeightThere) {
#ifdef RABALDR_CHUNKY_STACK
      if (deadCode) {
        setStackHeight(destStackHeight);
      } else {
        popChunkyBytes(stackHeightHere - stackHeightThere);
      }
#else
      if (deadCode) {
        masm.setFramePushed(stackHeightThere);
      } else {
        masm.freeStack(stackHeightHere - stackHeightThere);
      }
#endif
    }
  }

 public:
  //////////////////////////////////////////////////////////////////////
  //
  // The Argument area - for outgoing calls.
  //
  // We abstract these operations as an optimization: we can merge the freeing
  // of the argument area and dropping values off the stack after a call.  But
  // they always amount to manipulating the real stack pointer by some amount.
  //
  // Note that we do not update currentStackHeight_ for this; the frame does
  // not know about outgoing arguments.  But we do update framePushed(), so we
  // can still index into the frame below the outgoing arguments area.

  // This is always equivalent to a masm.reserveStack() call.

  void allocArgArea(size_t argSize) {
    if (argSize) {
      masm.reserveStack(argSize);
    }
  }

  // This frees the argument area allocated by allocArgArea(), and `argSize`
  // must be equal to the `argSize` argument to allocArgArea().  In addition
  // we drop some values from the frame, corresponding to the values that were
  // consumed by the call.

  void freeArgAreaAndPopBytes(size_t argSize, size_t dropSize) {
#ifdef RABALDR_CHUNKY_STACK
    // Freeing the outgoing arguments and freeing the consumed values have
    // different semantics here, which is why the operation is split.
    if (argSize) {
      masm.freeStack(argSize);
    }
    popChunkyBytes(dropSize);
#else
    if (argSize + dropSize) {
      masm.freeStack(argSize + dropSize);
    }
#endif
  }
};

class BaseStackFrame final : public BaseStackFrameAllocator {
  MacroAssembler& masm;

  // The largest observed value of masm.framePushed(), ie, the size of the
  // stack frame.  Read this for its true value only when code generation is
  // finished.
  uint32_t maxFramePushed_;

  // Patch point where we check for stack overflow.
  CodeOffset stackAddOffset_;

  // Low byte offset of local area for true locals (not parameters).
  uint32_t varLow_;

  // High byte offset + 1 of local area for true locals.
  uint32_t varHigh_;

  // The stack pointer, cached for brevity.
  RegisterOrSP sp_;

 public:
  explicit BaseStackFrame(MacroAssembler& masm)
      : BaseStackFrameAllocator(masm),
        masm(masm),
        maxFramePushed_(0),
        stackAddOffset_(0),
        varLow_(UINT32_MAX),
        varHigh_(UINT32_MAX),
        sp_(masm.getStackPointer()) {}

  ///////////////////////////////////////////////////////////////////////////
  //
  // Stack management and overflow checking

  // This must be called once beginFunction has allocated space for the Header
  // (the Frame and DebugFrame) and the Local area, and will record the current
  // frame size for internal use by the stack abstractions.

  void onFixedStackAllocated() {
    maxFramePushed_ = masm.framePushed();
    BaseStackFrameAllocator::onFixedStackAllocated();
  }

  // We won't know until after we've generated code how big the frame will be
  // (we may need arbitrary spill slots and outgoing param slots) so emit a
  // patchable add that is patched in endFunction().
  //
  // Note the platform scratch register may be used by branchPtr(), so
  // generally tmp must be something else.

  void checkStack(Register tmp, BytecodeOffset trapOffset) {
    stackAddOffset_ = masm.sub32FromStackPtrWithPatch(tmp);
    Label ok;
    masm.branchPtr(Assembler::Below,
                   Address(WasmTlsReg, offsetof(wasm::TlsData, stackLimit)),
                   tmp, &ok);
    masm.wasmTrap(Trap::StackOverflow, trapOffset);
    masm.bind(&ok);
  }

  void patchCheckStack() {
    masm.patchSub32FromStackPtr(stackAddOffset_,
                                Imm32(int32_t(maxFramePushed_)));
  }

  // Very large frames are implausible, probably an attack.

  bool checkStackHeight() {
    // 512KiB should be enough, considering how Rabaldr uses the stack and
    // what the standard limits are:
    //
    // - 1,000 parameters
    // - 50,000 locals
    // - 10,000 values on the eval stack (not an official limit)
    //
    // At sizeof(int64) bytes per slot this works out to about 480KiB.
    return maxFramePushed_ <= 512 * 1024;
  }

  ///////////////////////////////////////////////////////////////////////////
  //
  // Local area

  struct Local {
    // Type of the value.
    const MIRType type;

    // Byte offset from Frame "into" the locals, ie positive for true locals
    // and negative for incoming args that read directly from the arg area.
    // It assumes the stack is growing down and that locals are on the stack
    // at lower addresses than Frame, and is the offset from Frame of the
    // lowest-addressed byte of the local.
    const int32_t offs;

    Local(MIRType type, int32_t offs) : type(type), offs(offs) {}
  };

  // Profiling shows that the number of parameters and locals frequently
  // touches or exceeds 8.  So 16 seems like a reasonable starting point.
  using LocalVector = Vector<Local, 16, SystemAllocPolicy>;

  // Initialize `localInfo` based on the types of `locals` and `args`.
  bool setupLocals(const ValTypeVector& locals, const ValTypeVector& args,
                   bool debugEnabled, LocalVector* localInfo) {
    if (!localInfo->reserve(locals.length())) {
      return false;
    }

    DebugOnly<uint32_t> index = 0;
    BaseLocalIter i(locals, args.length(), debugEnabled);
    varLow_ = i.reservedSize();
    for (; !i.done() && i.index() < args.length(); i++) {
      MOZ_ASSERT(i.isArg());
      MOZ_ASSERT(i.index() == index);
      localInfo->infallibleEmplaceBack(i.mirType(), i.frameOffset());
      varLow_ = i.currentLocalSize();
      index++;
    }

    varHigh_ = varLow_;
    for (; !i.done(); i++) {
      MOZ_ASSERT(!i.isArg());
      MOZ_ASSERT(i.index() == index);
      localInfo->infallibleEmplaceBack(i.mirType(), i.frameOffset());
      varHigh_ = i.currentLocalSize();
      index++;
    }

    setLocalSize(AlignBytes(varHigh_, WasmStackAlignment));

    return true;
  }

  void zeroLocals(BaseRegAlloc* ra);

  void loadLocalI32(const Local& src, RegI32 dest) {
    masm.load32(Address(sp_, localOffset(src)), dest);
  }

#ifndef JS_PUNBOX64
  void loadLocalI64Low(const Local& src, RegI32 dest) {
    masm.load32(Address(sp_, localOffset(src) + INT64LOW_OFFSET), dest);
  }

  void loadLocalI64High(const Local& src, RegI32 dest) {
    masm.load32(Address(sp_, localOffset(src) + INT64HIGH_OFFSET), dest);
  }
#endif

  void loadLocalI64(const Local& src, RegI64 dest) {
    masm.load64(Address(sp_, localOffset(src)), dest);
  }

  void loadLocalPtr(const Local& src, RegPtr dest) {
    masm.loadPtr(Address(sp_, localOffset(src)), dest);
  }

  void loadLocalF64(const Local& src, RegF64 dest) {
    masm.loadDouble(Address(sp_, localOffset(src)), dest);
  }

  void loadLocalF32(const Local& src, RegF32 dest) {
    masm.loadFloat32(Address(sp_, localOffset(src)), dest);
  }

  void storeLocalI32(RegI32 src, const Local& dest) {
    masm.store32(src, Address(sp_, localOffset(dest)));
  }

  void storeLocalI64(RegI64 src, const Local& dest) {
    masm.store64(src, Address(sp_, localOffset(dest)));
  }

  void storeLocalPtr(Register src, const Local& dest) {
    masm.storePtr(src, Address(sp_, localOffset(dest)));
  }

  void storeLocalF64(RegF64 src, const Local& dest) {
    masm.storeDouble(src, Address(sp_, localOffset(dest)));
  }

  void storeLocalF32(RegF32 src, const Local& dest) {
    masm.storeFloat32(src, Address(sp_, localOffset(dest)));
  }

  // Offset off of sp_ for `local`.
  int32_t localOffset(const Local& local) { return localOffset(local.offs); }

 private:
  // Offset off of sp_ for a local with offset `offset` from Frame.
  int32_t localOffset(int32_t offset) { return masm.framePushed() - offset; }

 public:
  ///////////////////////////////////////////////////////////////////////////
  //
  // Dynamic area

  // Sizes of items in the stack area.
  //
  // The size values come from the implementations of Push() in
  // MacroAssembler-x86-shared.cpp and MacroAssembler-arm-shared.cpp, and from
  // VFPRegister::size() in Architecture-arm.h.
  //
  // On ARM unlike on x86 we push a single for float.

  static const size_t StackSizeOfPtr = sizeof(intptr_t);
  static const size_t StackSizeOfInt64 = sizeof(int64_t);
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32)
  static const size_t StackSizeOfFloat = sizeof(float);
#else
  static const size_t StackSizeOfFloat = sizeof(double);
#endif
  static const size_t StackSizeOfDouble = sizeof(double);

  uint32_t pushPtr(Register r) {
    DebugOnly<uint32_t> stackBefore = currentStackHeight();
#ifdef RABALDR_CHUNKY_STACK
    pushChunkyBytes(StackSizeOfPtr);
    masm.storePtr(r, Address(sp_, stackOffset(currentStackHeight())));
#else
    masm.Push(r);
#endif
    maxFramePushed_ = Max(maxFramePushed_, masm.framePushed());
    MOZ_ASSERT(stackBefore + StackSizeOfPtr == currentStackHeight());
    return currentStackHeight();
  }

  uint32_t pushFloat32(FloatRegister r) {
    DebugOnly<uint32_t> stackBefore = currentStackHeight();
#ifdef RABALDR_CHUNKY_STACK
    pushChunkyBytes(StackSizeOfFloat);
    masm.storeFloat32(r, Address(sp_, stackOffset(currentStackHeight())));
#else
    masm.Push(r);
#endif
    maxFramePushed_ = Max(maxFramePushed_, masm.framePushed());
    MOZ_ASSERT(stackBefore + StackSizeOfFloat == currentStackHeight());
    return currentStackHeight();
  }

  uint32_t pushDouble(FloatRegister r) {
    DebugOnly<uint32_t> stackBefore = currentStackHeight();
#ifdef RABALDR_CHUNKY_STACK
    pushChunkyBytes(StackSizeOfDouble);
    masm.storeDouble(r, Address(sp_, stackOffset(currentStackHeight())));
#else
    masm.Push(r);
#endif
    maxFramePushed_ = Max(maxFramePushed_, masm.framePushed());
    MOZ_ASSERT(stackBefore + StackSizeOfDouble == currentStackHeight());
    return currentStackHeight();
  }

  void popPtr(Register r) {
    DebugOnly<uint32_t> stackBefore = currentStackHeight();
#ifdef RABALDR_CHUNKY_STACK
    masm.loadPtr(Address(sp_, stackOffset(currentStackHeight())), r);
    popChunkyBytes(StackSizeOfPtr);
#else
    masm.Pop(r);
#endif
    MOZ_ASSERT(stackBefore - StackSizeOfPtr == currentStackHeight());
  }

  void popFloat32(FloatRegister r) {
    DebugOnly<uint32_t> stackBefore = currentStackHeight();
#ifdef RABALDR_CHUNKY_STACK
    masm.loadFloat32(Address(sp_, stackOffset(currentStackHeight())), r);
    popChunkyBytes(StackSizeOfFloat);
#else
    masm.Pop(r);
#endif
    MOZ_ASSERT(stackBefore - StackSizeOfFloat == currentStackHeight());
  }

  void popDouble(FloatRegister r) {
    DebugOnly<uint32_t> stackBefore = currentStackHeight();
#ifdef RABALDR_CHUNKY_STACK
    masm.loadDouble(Address(sp_, stackOffset(currentStackHeight())), r);
    popChunkyBytes(StackSizeOfDouble);
#else
    masm.Pop(r);
#endif
    MOZ_ASSERT(stackBefore - StackSizeOfDouble == currentStackHeight());
  }

  void popBytes(size_t bytes) {
    if (bytes > 0) {
#ifdef RABALDR_CHUNKY_STACK
      popChunkyBytes(bytes);
#else
      masm.freeStack(bytes);
#endif
    }
  }

  void loadStackI32(int32_t offset, RegI32 dest) {
    masm.load32(Address(sp_, stackOffset(offset)), dest);
  }

  void loadStackI64(int32_t offset, RegI64 dest) {
    masm.load64(Address(sp_, stackOffset(offset)), dest);
  }

#ifndef JS_PUNBOX64
  void loadStackI64Low(int32_t offset, RegI32 dest) {
    masm.load32(Address(sp_, stackOffset(offset - INT64LOW_OFFSET)), dest);
  }

  void loadStackI64High(int32_t offset, RegI32 dest) {
    masm.load32(Address(sp_, stackOffset(offset - INT64HIGH_OFFSET)), dest);
  }
#endif

  // Disambiguation: this loads a "Ptr" value from the stack, it does not load
  // the "StackPtr".

  void loadStackPtr(int32_t offset, RegPtr dest) {
    masm.loadPtr(Address(sp_, stackOffset(offset)), dest);
  }

  void loadStackF64(int32_t offset, RegF64 dest) {
    masm.loadDouble(Address(sp_, stackOffset(offset)), dest);
  }

  void loadStackF32(int32_t offset, RegF32 dest) {
    masm.loadFloat32(Address(sp_, stackOffset(offset)), dest);
  }
};

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

// Value stack: stack elements

struct Stk {
 private:
  Stk() : kind_(Unknown), i64val_(0) {}

 public:
  enum Kind {
    // The Mem opcodes are all clustered at the beginning to
    // allow for a quick test within sync().
    MemI32,  // 32-bit integer stack value ("offs")
    MemI64,  // 64-bit integer stack value ("offs")
    MemF32,  // 32-bit floating stack value ("offs")
    MemF64,  // 64-bit floating stack value ("offs")
    MemRef,  // reftype (pointer wide) stack value ("offs")

    // The Local opcodes follow the Mem opcodes for a similar
    // quick test within hasLocal().
    LocalI32,  // Local int32 var ("slot")
    LocalI64,  // Local int64 var ("slot")
    LocalF32,  // Local float32 var ("slot")
    LocalF64,  // Local double var ("slot")
    LocalRef,  // Local reftype (pointer wide) var ("slot")

    RegisterI32,  // 32-bit integer register ("i32reg")
    RegisterI64,  // 64-bit integer register ("i64reg")
    RegisterF32,  // 32-bit floating register ("f32reg")
    RegisterF64,  // 64-bit floating register ("f64reg")
    RegisterRef,  // reftype (pointer wide) register ("refReg")

    ConstI32,  // 32-bit integer constant ("i32val")
    ConstI64,  // 64-bit integer constant ("i64val")
    ConstF32,  // 32-bit floating constant ("f32val")
    ConstF64,  // 64-bit floating constant ("f64val")
    ConstRef,  // reftype (pointer wide) constant ("refval")

    Unknown,
  };

  Kind kind_;

  static const Kind MemLast = MemRef;
  static const Kind LocalLast = LocalRef;

  union {
    RegI32 i32reg_;
    RegI64 i64reg_;
    RegPtr refReg_;
    RegF32 f32reg_;
    RegF64 f64reg_;
    int32_t i32val_;
    int64_t i64val_;
    intptr_t refval_;
    float f32val_;
    double f64val_;
    uint32_t slot_;
    uint32_t offs_;
  };

  explicit Stk(RegI32 r) : kind_(RegisterI32), i32reg_(r) {}
  explicit Stk(RegI64 r) : kind_(RegisterI64), i64reg_(r) {}
  explicit Stk(RegPtr r) : kind_(RegisterRef), refReg_(r) {}
  explicit Stk(RegF32 r) : kind_(RegisterF32), f32reg_(r) {}
  explicit Stk(RegF64 r) : kind_(RegisterF64), f64reg_(r) {}
  explicit Stk(int32_t v) : kind_(ConstI32), i32val_(v) {}
  explicit Stk(int64_t v) : kind_(ConstI64), i64val_(v) {}
  explicit Stk(float v) : kind_(ConstF32), f32val_(v) {}
  explicit Stk(double v) : kind_(ConstF64), f64val_(v) {}
  explicit Stk(Kind k, uint32_t v) : kind_(k), slot_(v) {
    MOZ_ASSERT(k > MemLast && k <= LocalLast);
  }
  static Stk StkRef(intptr_t v) {
    Stk s;
    s.kind_ = ConstRef;
    s.refval_ = v;
    return s;
  }

  void setOffs(Kind k, uint32_t v) {
    MOZ_ASSERT(k <= MemLast);
    kind_ = k;
    offs_ = v;
  }

  Kind kind() const { return kind_; }
  bool isMem() const { return kind_ <= MemLast; }

  RegI32 i32reg() const {
    MOZ_ASSERT(kind_ == RegisterI32);
    return i32reg_;
  }
  RegI64 i64reg() const {
    MOZ_ASSERT(kind_ == RegisterI64);
    return i64reg_;
  }
  RegPtr refReg() const {
    MOZ_ASSERT(kind_ == RegisterRef);
    return refReg_;
  }
  RegF32 f32reg() const {
    MOZ_ASSERT(kind_ == RegisterF32);
    return f32reg_;
  }
  RegF64 f64reg() const {
    MOZ_ASSERT(kind_ == RegisterF64);
    return f64reg_;
  }

  int32_t i32val() const {
    MOZ_ASSERT(kind_ == ConstI32);
    return i32val_;
  }
  int64_t i64val() const {
    MOZ_ASSERT(kind_ == ConstI64);
    return i64val_;
  }
  intptr_t refval() const {
    MOZ_ASSERT(kind_ == ConstRef);
    return refval_;
  }

  // For these two, use an out-param instead of simply returning, to
  // use the normal stack and not the x87 FP stack (which has effect on
  // NaNs with the signaling bit set).

  void f32val(float* out) const {
    MOZ_ASSERT(kind_ == ConstF32);
    *out = f32val_;
  }
  void f64val(double* out) const {
    MOZ_ASSERT(kind_ == ConstF64);
    *out = f64val_;
  }

  uint32_t slot() const {
    MOZ_ASSERT(kind_ > MemLast && kind_ <= LocalLast);
    return slot_;
  }
  uint32_t offs() const {
    MOZ_ASSERT(isMem());
    return offs_;
  }
};

typedef Vector<Stk, 0, SystemAllocPolicy> StkVector;

// MachineStackTracker, used for stack-slot pointerness tracking.

class MachineStackTracker {
  // Simulates the machine's stack, with one bool per word.  Index zero in
  // this vector corresponds to the highest address in the machine stack.  The
  // last entry corresponds to what SP currently points at.  This all assumes
  // a grow-down stack.
  //
  // numPtrs_ contains the number of "true" values in vec_, and is therefore
  // redundant.  But it serves as a constant-time way to detect the common
  // case where vec_ holds no "true" values.
  size_t numPtrs_;
  Vector<bool, 64, SystemAllocPolicy> vec_;

 public:
  MachineStackTracker() : numPtrs_(0) {}

  ~MachineStackTracker() {
#ifdef DEBUG
    size_t n = 0;
    for (bool b : vec_) {
      n += (b ? 1 : 0);
    }
    MOZ_ASSERT(n == numPtrs_);
#endif
  }

  // Clone this MachineStackTracker, writing the result at |dst|.
  MOZ_MUST_USE bool cloneTo(MachineStackTracker* dst) {
    MOZ_ASSERT(dst->vec_.empty());
    if (!dst->vec_.appendAll(vec_)) {
      return false;
    }
    dst->numPtrs_ = numPtrs_;
    return true;
  }

  // Notionally push |n| non-pointers on the stack.
  MOZ_MUST_USE bool pushNonGCPointers(size_t n) {
    return vec_.appendN(false, n);
  }

  // Mark the stack slot |offsetFromSP| up from the bottom as holding a
  // pointer.
  void setGCPointer(size_t offsetFromSP) {
    // offsetFromSP == 0 denotes the most recently pushed item, == 1 the
    // second most recently pushed item, etc.
    MOZ_ASSERT(offsetFromSP < vec_.length());

    size_t offsetFromTop = vec_.length() - 1 - offsetFromSP;
    numPtrs_ = numPtrs_ + 1 - (vec_[offsetFromTop] ? 1 : 0);
    vec_[offsetFromTop] = true;
  }

  // Query the pointerness of the slot |offsetFromSP| up from the bottom.
  bool isGCPointer(size_t offsetFromSP) {
    MOZ_ASSERT(offsetFromSP < vec_.length());

    size_t offsetFromTop = vec_.length() - 1 - offsetFromSP;
    return vec_[offsetFromTop];
  }

  // Return the number of words tracked by this MachineStackTracker.
  size_t length() { return vec_.length(); }

  // Return the number of pointer-typed words tracked by this
  // MachineStackTracker.
  size_t numPtrs() {
    MOZ_ASSERT(numPtrs_ <= length());
    return numPtrs_;
  }

  // Discard all contents, but (per mozilla::Vector::clear semantics) don't
  // free or reallocate any dynamic storage associated with |vec_|.
  void clear() {
    vec_.clear();
    numPtrs_ = 0;
  }
};

// StackMapGenerator, which carries all state needed to create stack maps.

enum class HasRefTypedDebugFrame { No, Yes };

struct StackMapGenerator {
 private:
  // --- These are constant for the life of the function's compilation ---

  // For generating stack maps, we'll need to know the offsets of registers
  // as saved by the trap exit stub.
  const MachineState& trapExitLayout_;
  const size_t trapExitLayoutNumWords_;

  // Completed stackmaps are added here
  StackMaps* stackMaps_;

  // So as to be able to get current offset when creating stack maps
  const MacroAssembler& masm_;

 public:
  // --- These are constant once we've completed beginFunction() ---

  // The number of words of arguments passed to this function in memory.
  size_t numStackArgWords;

  MachineStackTracker machineStackTracker;  // tracks machine stack pointerness

  // This holds masm.framePushed at entry to the function's body.  It is a
  // Maybe because createStackMap needs to know whether or not we're still
  // in the prologue.  It makes a Nothing-to-Some transition just once per
  // function.
  Maybe<uint32_t> framePushedAtEntryToBody;

  // --- These can change at any point ---

  // This holds masm.framePushed at it would be be for a function call
  // instruction, but excluding the stack area used to pass arguments in
  // memory.  That is, for an upcoming function call, this will hold
  //
  //   masm.framePushed() at the call instruction -
  //      StackArgAreaSizeUnaligned(argumentTypes)
  //
  // This value denotes the lowest-addressed stack word covered by the current
  // function's stackmap.  Words below this point form the highest-addressed
  // area of the callee's stackmap.  Note that all alignment padding above the
  // arguments-in-memory themselves belongs to the caller's stack map, which
  // is why this is defined in terms of StackArgAreaSizeUnaligned() rather than
  // StackArgAreaSizeAligned().
  //
  // When not inside a function call setup/teardown sequence, it is Nothing.
  // It can make Nothing-to/from-Some transitions arbitrarily as we progress
  // through the function body.
  Maybe<uint32_t> framePushedExcludingOutboundCallArgs;

  // The number of memory-resident, ref-typed entries on the containing
  // BaseCompiler::stk_.
  size_t memRefsOnStk;

  // This is a copy of machineStackTracker that is used only within individual
  // calls to createStackMap. It is here only to avoid possible heap allocation
  // costs resulting from making it local to createStackMap().
  MachineStackTracker augmentedMst;

  StackMapGenerator(StackMaps* stackMaps, const MachineState& trapExitLayout,
                    const size_t trapExitLayoutNumWords,
                    const MacroAssembler& masm)
      : trapExitLayout_(trapExitLayout),
        trapExitLayoutNumWords_(trapExitLayoutNumWords),
        stackMaps_(stackMaps),
        masm_(masm),
        numStackArgWords(0),
        memRefsOnStk(0) {}

  // At the beginning of a function, we may have live roots in registers (as
  // arguments) at the point where we perform a stack overflow check.  This
  // method generates the "extra" stackmap entries to describe that, in the
  // case that the check fails and we wind up calling into the wasm exit
  // stub, as generated by GenerateTrapExit().
  //
  // The resulting map must correspond precisely with the stack layout
  // created for the integer registers as saved by (code generated by)
  // GenerateTrapExit().  To do that we use trapExitLayout_ and
  // trapExitLayoutNumWords_, which together comprise a description of the
  // layout and are created by GenerateTrapExitMachineState().
  MOZ_MUST_USE bool generateStackmapEntriesForTrapExit(
      const ValTypeVector& args, ExitStubMapVector* extras) {
    return GenerateStackmapEntriesForTrapExit(args, trapExitLayout_,
                                              trapExitLayoutNumWords_, extras);
  }

  // Creates a stackmap associated with the instruction denoted by
  // |assemblerOffset|, incorporating pointers from the current operand
  // stack |stk|, incorporating possible extra pointers in |extra| at the
  // lower addressed end, and possibly with the associated frame having a
  // ref-typed DebugFrame as indicated by |refDebugFrame|.
  MOZ_MUST_USE bool createStackMap(const char* who,
                                   const ExitStubMapVector& extras,
                                   uint32_t assemblerOffset,
                                   HasRefTypedDebugFrame refDebugFrame,
                                   const StkVector& stk) {
    size_t countedPointers = machineStackTracker.numPtrs() + memRefsOnStk;
#ifndef DEBUG
    // An important optimization.  If there are obviously no pointers, as
    // we expect in the majority of cases, exit quickly.
    if (countedPointers == 0 && refDebugFrame == HasRefTypedDebugFrame::No) {
      // We can skip creating the map if there are no |true| elements in
      // |extras|.
      bool extrasHasRef = false;
      for (bool b : extras) {
        if (b) {
          extrasHasRef = true;
          break;
        }
      }
      if (!extrasHasRef) {
        return true;
      }
    }
#else
    // In the debug case, create the stack map regardless, and cross-check
    // the pointer-counting below.  We expect the final map to have
    // |countedPointers| in total.  This doesn't include those in the
    // DebugFrame, but they do not appear in the map's bitmap.  Note that
    // |countedPointers| is debug-only from this point onwards.
    for (bool b : extras) {
      countedPointers += (b ? 1 : 0);
    }
#endif

    // Start with the frame-setup map, and add operand-stack information to
    // that.  augmentedMst holds live data only within individual calls to
    // createStackMap.
    augmentedMst.clear();
    if (!machineStackTracker.cloneTo(&augmentedMst)) {
      return false;
    }

    // At this point, augmentedMst only contains entries covering the
    // incoming argument area (if any) and for the area allocated by this
    // function's prologue.  We now need to calculate how far the machine's
    // stack pointer is below where it was at the start of the body.  But we
    // must take care not to include any words pushed as arguments to an
    // upcoming function call, since those words "belong" to the stackmap of
    // the callee, not to the stackmap of this function.  Note however that
    // any alignment padding pushed prior to pushing the args *does* belong to
    // this function.
    //
    // That padding is taken into account at the point where
    // framePushedExcludingOutboundCallArgs is set, viz, in startCallArgs(),
    // and comprises two components:
    //
    // * call->frameAlignAdjustment
    // * the padding applied to the stack arg area itself.  That is:
    //   StackArgAreaSize(argTys) - StackArgAreaSizeUnpadded(argTys)
    Maybe<uint32_t> framePushedExcludingArgs;
    if (framePushedAtEntryToBody.isNothing()) {
      // Still in the prologue.  framePushedExcludingArgs remains Nothing.
      MOZ_ASSERT(framePushedExcludingOutboundCallArgs.isNothing());
    } else {
      // In the body.
      MOZ_ASSERT(masm_.framePushed() >= framePushedAtEntryToBody.value());
      if (framePushedExcludingOutboundCallArgs.isSome()) {
        // In the body, and we've potentially pushed some args onto the stack.
        // We must ignore them when sizing the stackmap.
        MOZ_ASSERT(masm_.framePushed() >=
                   framePushedExcludingOutboundCallArgs.value());
        MOZ_ASSERT(framePushedExcludingOutboundCallArgs.value() >=
                   framePushedAtEntryToBody.value());
        framePushedExcludingArgs =
            Some(framePushedExcludingOutboundCallArgs.value());
      } else {
        // In the body, but not with call args on the stack.  The stackmap
        // must be sized so as to extend all the way "down" to
        // masm_.framePushed().
        framePushedExcludingArgs = Some(masm_.framePushed());
      }
    }

    if (framePushedExcludingArgs.isSome()) {
      uint32_t bodyPushedBytes =
          framePushedExcludingArgs.value() - framePushedAtEntryToBody.value();
      MOZ_ASSERT(0 == bodyPushedBytes % sizeof(void*));
      if (!augmentedMst.pushNonGCPointers(bodyPushedBytes / sizeof(void*))) {
        return false;
      }
    }

    // Scan the operand stack, marking pointers in the just-added new
    // section.
    MOZ_ASSERT_IF(framePushedAtEntryToBody.isNothing(), stk.empty());
    MOZ_ASSERT_IF(framePushedExcludingArgs.isNothing(), stk.empty());

    for (const Stk& v : stk) {
#ifndef DEBUG
      // We don't track roots in registers, per rationale below, so if this
      // doesn't hold, something is seriously wrong, and we're likely to get a
      // GC-related crash.
      MOZ_RELEASE_ASSERT(v.kind() != Stk::RegisterRef);
      if (v.kind() != Stk::MemRef) {
        continue;
      }
#else
      // Take the opportunity to check everything we reasonably can about
      // operand stack elements.
      switch (v.kind()) {
        case Stk::MemI32:
        case Stk::MemI64:
        case Stk::MemF32:
        case Stk::MemF64:
        case Stk::ConstI32:
        case Stk::ConstI64:
        case Stk::ConstF32:
        case Stk::ConstF64:
          // All of these have uninteresting type.
          continue;
        case Stk::LocalI32:
        case Stk::LocalI64:
        case Stk::LocalF32:
        case Stk::LocalF64:
          // These also have uninteresting type.  Check that they live in the
          // section of stack set up by beginFunction().  The unguarded use of
          // |value()| here is safe due to the assertion above this loop.
          MOZ_ASSERT(v.offs() <= framePushedAtEntryToBody.value());
          continue;
        case Stk::RegisterI32:
        case Stk::RegisterI64:
        case Stk::RegisterF32:
        case Stk::RegisterF64:
          // These also have uninteresting type, but more to the point: all
          // registers holding live values should have been flushed to the
          // machine stack immediately prior to the instruction to which this
          // stackmap pertains.  So these can't happen.
          MOZ_CRASH("createStackMap: operand stack has Register-non-Ref");
        case Stk::MemRef:
          // This is the only case we care about.  We'll handle it after the
          // switch.
          break;
        case Stk::LocalRef:
          // We need the stackmap to mention this pointer, but it should
          // already be in the machineStackTracker section created by
          // beginFunction().
          MOZ_ASSERT(v.offs() <= framePushedAtEntryToBody.value());
          continue;
        case Stk::ConstRef:
          // This can currently only be a null pointer.
          MOZ_ASSERT(v.refval() == 0);
          continue;
        case Stk::RegisterRef:
          // This can't happen, per rationale above.
          MOZ_CRASH("createStackMap: operand stack contains RegisterRef");
        default:
          MOZ_CRASH("createStackMap: unknown operand stack element");
      }
#endif
      // v.offs() holds masm.framePushed() at the point immediately after it
      // was pushed on the stack.  Since it's still on the stack,
      // masm.framePushed() can't be less.
      MOZ_ASSERT(v.offs() <= framePushedExcludingArgs.value());
      uint32_t offsFromMapLowest = framePushedExcludingArgs.value() - v.offs();
      MOZ_ASSERT(0 == offsFromMapLowest % sizeof(void*));
      augmentedMst.setGCPointer(offsFromMapLowest / sizeof(void*));
    }

    // Create the final StackMap.  The initial map is zeroed out, so there's
    // no need to write zero bits in it.
    const uint32_t extraWords = extras.length();
    const uint32_t augmentedMstWords = augmentedMst.length();
    const uint32_t numMappedWords = extraWords + augmentedMstWords;
    StackMap* stackMap = StackMap::create(numMappedWords);
    if (!stackMap) {
      return false;
    }

    {
      // First the exit stub extra words, if any.
      uint32_t i = 0;
      for (bool b : extras) {
        if (b) {
          stackMap->setBit(i);
        }
        i++;
      }
    }
    // Followed by the "main" part of the map.
    for (uint32_t i = 0; i < augmentedMstWords; i++) {
      if (augmentedMst.isGCPointer(i)) {
        stackMap->setBit(extraWords + i);
      }
    }

    stackMap->setExitStubWords(extraWords);

    // Record in the map, how far down from the highest address the Frame* is.
    // Take the opportunity to check that we haven't marked any part of the
    // Frame itself as a pointer.
    stackMap->setFrameOffsetFromTop(numStackArgWords +
                                    sizeof(Frame) / sizeof(void*));
#ifdef DEBUG
    for (uint32_t i = 0; i < sizeof(Frame) / sizeof(void*); i++) {
      MOZ_ASSERT(stackMap->getBit(stackMap->numMappedWords -
                                  stackMap->frameOffsetFromTop + i) == 0);
    }
#endif

    // Note the presence of a ref-typed DebugFrame, if any.
    if (refDebugFrame == HasRefTypedDebugFrame::Yes) {
      stackMap->setHasRefTypedDebugFrame();
    }

    // Add the completed map to the running collection thereof.
    if (!stackMaps_->add((uint8_t*)(uintptr_t)assemblerOffset, stackMap)) {
      return false;
    }

#ifdef DEBUG
    {
      // Crosscheck the map pointer counting.
      uint32_t nw = stackMap->numMappedWords;
      uint32_t np = 0;
      for (uint32_t i = 0; i < nw; i++) {
        np += stackMap->getBit(i);
      }
      MOZ_ASSERT(size_t(np) == countedPointers);
    }
#endif

    return true;
  }
};

// The baseline compiler proper.

class BaseCompiler final : public BaseCompilerInterface {
  using Local = BaseStackFrame::Local;
  using LabelVector = Vector<NonAssertingLabel, 8, SystemAllocPolicy>;

  // Bit set used for simple bounds check elimination.  Capping this at 64
  // locals makes sense; even 32 locals would probably be OK in practice.
  //
  // For more information about BCE, see the block comment above
  // popMemoryAccess(), below.

  using BCESet = uint64_t;

  // Control node, representing labels and stack heights at join points.

  struct Control {
    NonAssertingLabel label;       // The "exit" label
    NonAssertingLabel otherLabel;  // Used for the "else" branch of if-then-else
    StackHeight stackHeight;       // From BaseStackFrame
    uint32_t stackSize;            // Value stack height
    BCESet bceSafeOnEntry;         // Bounds check info flowing into the item
    BCESet bceSafeOnExit;          // Bounds check info flowing out of the item
    bool deadOnArrival;            // deadCode_ was set on entry to the region
    bool deadThenBranch;           // deadCode_ was set on exit from "then"

    Control()
        : stackHeight(StackHeight::Invalid()),
          stackSize(UINT32_MAX),
          bceSafeOnEntry(0),
          bceSafeOnExit(~BCESet(0)),
          deadOnArrival(false),
          deadThenBranch(false) {}
  };

  struct BaseCompilePolicy {
    // The baseline compiler tracks values on a stack of its own -- it
    // needs to scan that stack for spilling -- and thus has no need
    // for the values maintained by the iterator.
    using Value = Nothing;

    // The baseline compiler uses the iterator's control stack, attaching
    // its own control information.
    using ControlItem = Control;
  };

  using BaseOpIter = OpIter<BaseCompilePolicy>;

  // The baseline compiler will use OOL code more sparingly than
  // Baldr since our code is not high performance and frills like
  // code density and branch prediction friendliness will be less
  // important.

  class OutOfLineCode : public TempObject {
   private:
    NonAssertingLabel entry_;
    NonAssertingLabel rejoin_;
    StackHeight stackHeight_;

   public:
    OutOfLineCode() : stackHeight_(StackHeight::Invalid()) {}

    Label* entry() { return &entry_; }
    Label* rejoin() { return &rejoin_; }

    void setStackHeight(StackHeight stackHeight) {
      MOZ_ASSERT(!stackHeight_.isValid());
      stackHeight_ = stackHeight;
    }

    void bind(BaseStackFrame* fr, MacroAssembler* masm) {
      MOZ_ASSERT(stackHeight_.isValid());
      masm->bind(&entry_);
      fr->setStackHeight(stackHeight_);
    }

    // The generate() method must be careful about register use
    // because it will be invoked when there is a register
    // assignment in the BaseCompiler that does not correspond
    // to the available registers when the generated OOL code is
    // executed.  The register allocator *must not* be called.
    //
    // The best strategy is for the creator of the OOL object to
    // allocate all temps that the OOL code will need.
    //
    // Input, output, and temp registers are embedded in the OOL
    // object and are known to the code generator.
    //
    // Scratch registers are available to use in OOL code.
    //
    // All other registers must be explicitly saved and restored
    // by the OOL code before being used.

    virtual void generate(MacroAssembler* masm) = 0;
  };

  enum class LatentOp { None, Compare, Eqz };

  struct AccessCheck {
    AccessCheck()
        : omitBoundsCheck(false),
          omitAlignmentCheck(false),
          onlyPointerAlignment(false) {}

    // If `omitAlignmentCheck` is true then we need check neither the
    // pointer nor the offset.  Otherwise, if `onlyPointerAlignment` is true
    // then we need check only the pointer.  Otherwise, check the sum of
    // pointer and offset.

    bool omitBoundsCheck;
    bool omitAlignmentCheck;
    bool onlyPointerAlignment;
  };

  const ModuleEnvironment& env_;
  BaseOpIter iter_;
  const FuncCompileInput& func_;
  size_t lastReadCallSite_;
  TempAllocator& alloc_;
  const ValTypeVector& locals_;  // Types of parameters and locals
  bool deadCode_;  // Flag indicating we should decode & discard the opcode
  BCESet
      bceSafe_;  // Locals that have been bounds checked and not updated since
  ValTypeVector SigD_;
  ValTypeVector SigF_;
  NonAssertingLabel returnLabel_;

  LatentOp latentOp_;   // Latent operation for branch (seen next)
  ValType latentType_;  // Operand type, if latentOp_ is true
  Assembler::Condition
      latentIntCmp_;  // Comparison operator, if latentOp_ == Compare, int types
  Assembler::DoubleCondition
      latentDoubleCmp_;  // Comparison operator, if latentOp_ == Compare, float
                         // types

  FuncOffsets offsets_;
  MacroAssembler& masm;  // No '_' suffix - too tedious...
  BaseRegAlloc ra;       // Ditto
  BaseStackFrame fr;

  StackMapGenerator stackMapGenerator_;

  BaseStackFrame::LocalVector localInfo_;
  Vector<OutOfLineCode*, 8, SystemAllocPolicy> outOfLine_;

  // On specific platforms we sometimes need to use specific registers.

  SpecificRegs specific_;

  // The join registers are used to carry values out of blocks.
  // JoinRegI32 and joinRegI64_ must overlap: emitBrIf and
  // emitBrTable assume that.

  RegI32 joinRegI32_;
  RegI64 joinRegI64_;
  RegPtr joinRegPtr_;
  RegF32 joinRegF32_;
  RegF64 joinRegF64_;

  // There are more members scattered throughout.

 public:
  BaseCompiler(const ModuleEnvironment& env, const FuncCompileInput& input,
               const ValTypeVector& locals, const MachineState& trapExitLayout,
               size_t trapExitLayoutNumWords, Decoder& decoder,
               StkVector& stkSource, TempAllocator* alloc, MacroAssembler* masm,
               StackMaps* stackMaps);
  ~BaseCompiler();

  MOZ_MUST_USE bool init();

  FuncOffsets finish();

  MOZ_MUST_USE bool emitFunction();
  void emitInitStackLocals();

  const FuncTypeWithId& funcType() const {
    return *env_.funcTypes[func_.index];
  }

  // Used by some of the ScratchRegister implementations.
  operator MacroAssembler&() const { return masm; }
  operator BaseRegAlloc&() { return ra; }

 private:
  ////////////////////////////////////////////////////////////
  //
  // Out of line code management.

  MOZ_MUST_USE OutOfLineCode* addOutOfLineCode(OutOfLineCode* ool) {
    if (!ool || !outOfLine_.append(ool)) {
      return nullptr;
    }
    ool->setStackHeight(fr.stackHeight());
    return ool;
  }

  MOZ_MUST_USE bool generateOutOfLineCode() {
    for (uint32_t i = 0; i < outOfLine_.length(); i++) {
      OutOfLineCode* ool = outOfLine_[i];
      ool->bind(&fr, &masm);
      ool->generate(&masm);
    }

    return !masm.oom();
  }

  // Utility.

  const Local& localFromSlot(uint32_t slot, MIRType type) {
    MOZ_ASSERT(localInfo_[slot].type == type);
    return localInfo_[slot];
  }

  ////////////////////////////////////////////////////////////
  //
  // High-level register management.

  bool isAvailableI32(RegI32 r) { return ra.isAvailableI32(r); }
  bool isAvailableI64(RegI64 r) { return ra.isAvailableI64(r); }
  bool isAvailableRef(RegPtr r) { return ra.isAvailablePtr(r); }
  bool isAvailableF32(RegF32 r) { return ra.isAvailableF32(r); }
  bool isAvailableF64(RegF64 r) { return ra.isAvailableF64(r); }

  MOZ_MUST_USE RegI32 needI32() { return ra.needI32(); }
  MOZ_MUST_USE RegI64 needI64() { return ra.needI64(); }
  MOZ_MUST_USE RegPtr needRef() { return ra.needPtr(); }
  MOZ_MUST_USE RegF32 needF32() { return ra.needF32(); }
  MOZ_MUST_USE RegF64 needF64() { return ra.needF64(); }

  void needI32(RegI32 specific) { ra.needI32(specific); }
  void needI64(RegI64 specific) { ra.needI64(specific); }
  void needRef(RegPtr specific) { ra.needPtr(specific); }
  void needF32(RegF32 specific) { ra.needF32(specific); }
  void needF64(RegF64 specific) { ra.needF64(specific); }

#if defined(JS_CODEGEN_ARM)
  MOZ_MUST_USE RegI64 needI64Pair() { return ra.needI64Pair(); }
#endif

  void freeI32(RegI32 r) { ra.freeI32(r); }
  void freeI64(RegI64 r) { ra.freeI64(r); }
  void freeRef(RegPtr r) { ra.freePtr(r); }
  void freeF32(RegF32 r) { ra.freeF32(r); }
  void freeF64(RegF64 r) { ra.freeF64(r); }

  void freeI64Except(RegI64 r, RegI32 except) {
#ifdef JS_PUNBOX64
    MOZ_ASSERT(r.reg == except);
#else
    MOZ_ASSERT(r.high == except || r.low == except);
    freeI64(r);
    needI32(except);
#endif
  }

  void maybeFreeI32(RegI32 r) {
    if (r.isValid()) {
      freeI32(r);
    }
  }

  void maybeFreeI64(RegI64 r) {
    if (r.isValid()) {
      freeI64(r);
    }
  }

  void maybeFreeF64(RegF64 r) {
    if (r.isValid()) {
      freeF64(r);
    }
  }

  void needI32NoSync(RegI32 r) {
    MOZ_ASSERT(isAvailableI32(r));
    needI32(r);
  }

  // TODO / OPTIMIZE: need2xI32() can be optimized along with needI32()
  // to avoid sync(). (Bug 1316802)

  void need2xI32(RegI32 r0, RegI32 r1) {
    needI32(r0);
    needI32(r1);
  }

  void need2xI64(RegI64 r0, RegI64 r1) {
    needI64(r0);
    needI64(r1);
  }

  RegI32 fromI64(RegI64 r) { return RegI32(lowPart(r)); }

#ifdef JS_PUNBOX64
  RegI64 fromI32(RegI32 r) { return RegI64(Register64(r)); }
#endif

  RegI64 widenI32(RegI32 r) {
    MOZ_ASSERT(!isAvailableI32(r));
#ifdef JS_PUNBOX64
    return fromI32(r);
#else
    RegI32 high = needI32();
    return RegI64(Register64(high, r));
#endif
  }

  RegI32 narrowI64(RegI64 r) {
#ifdef JS_PUNBOX64
    return RegI32(r.reg);
#else
    freeI32(RegI32(r.high));
    return RegI32(r.low);
#endif
  }

  RegI32 narrowPtr(RegPtr r) { return RegI32(r); }

  RegI32 lowPart(RegI64 r) {
#ifdef JS_PUNBOX64
    return RegI32(r.reg);
#else
    return RegI32(r.low);
#endif
  }

  RegI32 maybeHighPart(RegI64 r) {
#ifdef JS_PUNBOX64
    return RegI32::Invalid();
#else
    return RegI32(r.high);
#endif
  }

  void maybeClearHighPart(RegI64 r) {
#if !defined(JS_PUNBOX64)
    moveImm32(0, RegI32(r.high));
#endif
  }

  void moveI32(RegI32 src, RegI32 dest) {
    if (src != dest) {
      masm.move32(src, dest);
    }
  }

  void moveI64(RegI64 src, RegI64 dest) {
    if (src != dest) {
      masm.move64(src, dest);
    }
  }

  void moveRef(RegPtr src, RegPtr dest) {
    if (src != dest) {
      masm.movePtr(src, dest);
    }
  }

  void moveF64(RegF64 src, RegF64 dest) {
    if (src != dest) {
      masm.moveDouble(src, dest);
    }
  }

  void moveF32(RegF32 src, RegF32 dest) {
    if (src != dest) {
      masm.moveFloat32(src, dest);
    }
  }

  void maybeReserveJoinRegI(ExprType type) {
    switch (type.code()) {
      case ExprType::I32:
        needI32(joinRegI32_);
        break;
      case ExprType::I64:
        needI64(joinRegI64_);
        break;
      case ExprType::FuncRef:
      case ExprType::AnyRef:
      case ExprType::NullRef:
      case ExprType::Ref:
        needRef(joinRegPtr_);
        break;
      default:;
    }
  }

  void maybeUnreserveJoinRegI(ExprType type) {
    switch (type.code()) {
      case ExprType::I32:
        freeI32(joinRegI32_);
        break;
      case ExprType::I64:
        freeI64(joinRegI64_);
        break;
      case ExprType::FuncRef:
      case ExprType::AnyRef:
      case ExprType::NullRef:
      case ExprType::Ref:
        freeRef(joinRegPtr_);
        break;
      default:;
    }
  }

  void maybeReserveJoinReg(ExprType type) {
    switch (type.code()) {
      case ExprType::I32:
        needI32(joinRegI32_);
        break;
      case ExprType::I64:
        needI64(joinRegI64_);
        break;
      case ExprType::F32:
        needF32(joinRegF32_);
        break;
      case ExprType::F64:
        needF64(joinRegF64_);
        break;
      case ExprType::Ref:
      case ExprType::NullRef:
      case ExprType::FuncRef:
      case ExprType::AnyRef:
        needRef(joinRegPtr_);
        break;
      default:
        break;
    }
  }

  void maybeUnreserveJoinReg(ExprType type) {
    switch (type.code()) {
      case ExprType::I32:
        freeI32(joinRegI32_);
        break;
      case ExprType::I64:
        freeI64(joinRegI64_);
        break;
      case ExprType::F32:
        freeF32(joinRegF32_);
        break;
      case ExprType::F64:
        freeF64(joinRegF64_);
        break;
      case ExprType::Ref:
      case ExprType::NullRef:
      case ExprType::FuncRef:
      case ExprType::AnyRef:
        freeRef(joinRegPtr_);
        break;
      default:
        break;
    }
  }

  ////////////////////////////////////////////////////////////
  //
  // Value stack and spilling.
  //
  // The value stack facilitates some on-the-fly register allocation
  // and immediate-constant use.  It tracks constants, latent
  // references to locals, register contents, and values on the CPU
  // stack.
  //
  // The stack can be flushed to memory using sync().  This is handy
  // to avoid problems with control flow and messy register usage
  // patterns.

  // This is the value stack actually used during compilation.  It is a
  // StkVector rather than a StkVector& since constantly dereferencing a
  // StkVector& adds about 0.5% or more to the compiler's dynamic instruction
  // count.
  StkVector stk_;

  // BaselineCompileFunctions() "lends" us the StkVector to use in this
  // BaseCompiler object, and that is installed in |stk_| in our constructor.
  // This is so as to avoid having to malloc/free the vector's contents at
  // each creation/destruction of a BaseCompiler object.  It does however mean
  // that we need to hold on to a reference to BaselineCompileFunctions()'s
  // vector, so we can swap (give) its contents back when this BaseCompiler
  // object is destroyed.  This significantly reduces the heap turnover of the
  // baseline compiler.  See bug 1532592.
  StkVector& stkSource_;

#ifdef DEBUG
  size_t countMemRefsOnStk() {
    size_t nRefs = 0;
    for (Stk& v : stk_) {
      if (v.kind() == Stk::MemRef) {
        nRefs++;
      }
    }
    return nRefs;
  }
#endif

  template <typename T>
  void push(T item) {
    // None of the single-arg Stk constructors create a Stk::MemRef, so
    // there's no need to increment stackMapGenerator_.memRefsOnStk here.
    stk_.infallibleEmplaceBack(Stk(item));
  }

  void pushConstRef(intptr_t v) { stk_.infallibleEmplaceBack(Stk::StkRef(v)); }

  void loadConstI32(const Stk& src, RegI32 dest) {
    moveImm32(src.i32val(), dest);
  }

  void loadMemI32(const Stk& src, RegI32 dest) {
    fr.loadStackI32(src.offs(), dest);
  }

  void loadLocalI32(const Stk& src, RegI32 dest) {
    fr.loadLocalI32(localFromSlot(src.slot(), MIRType::Int32), dest);
  }

  void loadRegisterI32(const Stk& src, RegI32 dest) {
    moveI32(src.i32reg(), dest);
  }

  void loadConstI64(const Stk& src, RegI64 dest) {
    moveImm64(src.i64val(), dest);
  }

  void loadMemI64(const Stk& src, RegI64 dest) {
    fr.loadStackI64(src.offs(), dest);
  }

  void loadLocalI64(const Stk& src, RegI64 dest) {
    fr.loadLocalI64(localFromSlot(src.slot(), MIRType::Int64), dest);
  }

  void loadRegisterI64(const Stk& src, RegI64 dest) {
    moveI64(src.i64reg(), dest);
  }

  void loadConstRef(const Stk& src, RegPtr dest) {
    moveImmRef(src.refval(), dest);
  }

  void loadMemRef(const Stk& src, RegPtr dest) {
    fr.loadStackPtr(src.offs(), dest);
  }

  void loadLocalRef(const Stk& src, RegPtr dest) {
    fr.loadLocalPtr(localFromSlot(src.slot(), MIRType::RefOrNull), dest);
  }

  void loadRegisterRef(const Stk& src, RegPtr dest) {
    moveRef(src.refReg(), dest);
  }

  void loadConstF64(const Stk& src, RegF64 dest) {
    double d;
    src.f64val(&d);
    masm.loadConstantDouble(d, dest);
  }

  void loadMemF64(const Stk& src, RegF64 dest) {
    fr.loadStackF64(src.offs(), dest);
  }

  void loadLocalF64(const Stk& src, RegF64 dest) {
    fr.loadLocalF64(localFromSlot(src.slot(), MIRType::Double), dest);
  }

  void loadRegisterF64(const Stk& src, RegF64 dest) {
    moveF64(src.f64reg(), dest);
  }

  void loadConstF32(const Stk& src, RegF32 dest) {
    float f;
    src.f32val(&f);
    masm.loadConstantFloat32(f, dest);
  }

  void loadMemF32(const Stk& src, RegF32 dest) {
    fr.loadStackF32(src.offs(), dest);
  }

  void loadLocalF32(const Stk& src, RegF32 dest) {
    fr.loadLocalF32(localFromSlot(src.slot(), MIRType::Float32), dest);
  }

  void loadRegisterF32(const Stk& src, RegF32 dest) {
    moveF32(src.f32reg(), dest);
  }

  void loadI32(const Stk& src, RegI32 dest) {
    switch (src.kind()) {
      case Stk::ConstI32:
        loadConstI32(src, dest);
        break;
      case Stk::MemI32:
        loadMemI32(src, dest);
        break;
      case Stk::LocalI32:
        loadLocalI32(src, dest);
        break;
      case Stk::RegisterI32:
        loadRegisterI32(src, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: Expected I32 on stack");
    }
  }

  void loadI64(const Stk& src, RegI64 dest) {
    switch (src.kind()) {
      case Stk::ConstI64:
        loadConstI64(src, dest);
        break;
      case Stk::MemI64:
        loadMemI64(src, dest);
        break;
      case Stk::LocalI64:
        loadLocalI64(src, dest);
        break;
      case Stk::RegisterI64:
        loadRegisterI64(src, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: Expected I64 on stack");
    }
  }

#if !defined(JS_PUNBOX64)
  void loadI64Low(const Stk& src, RegI32 dest) {
    switch (src.kind()) {
      case Stk::ConstI64:
        moveImm32(int32_t(src.i64val()), dest);
        break;
      case Stk::MemI64:
        fr.loadStackI64Low(src.offs(), dest);
        break;
      case Stk::LocalI64:
        fr.loadLocalI64Low(localFromSlot(src.slot(), MIRType::Int64), dest);
        break;
      case Stk::RegisterI64:
        moveI32(RegI32(src.i64reg().low), dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: Expected I64 on stack");
    }
  }

  void loadI64High(const Stk& src, RegI32 dest) {
    switch (src.kind()) {
      case Stk::ConstI64:
        moveImm32(int32_t(src.i64val() >> 32), dest);
        break;
      case Stk::MemI64:
        fr.loadStackI64High(src.offs(), dest);
        break;
      case Stk::LocalI64:
        fr.loadLocalI64High(localFromSlot(src.slot(), MIRType::Int64), dest);
        break;
      case Stk::RegisterI64:
        moveI32(RegI32(src.i64reg().high), dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: Expected I64 on stack");
    }
  }
#endif

  void loadF64(const Stk& src, RegF64 dest) {
    switch (src.kind()) {
      case Stk::ConstF64:
        loadConstF64(src, dest);
        break;
      case Stk::MemF64:
        loadMemF64(src, dest);
        break;
      case Stk::LocalF64:
        loadLocalF64(src, dest);
        break;
      case Stk::RegisterF64:
        loadRegisterF64(src, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected F64 on stack");
    }
  }

  void loadF32(const Stk& src, RegF32 dest) {
    switch (src.kind()) {
      case Stk::ConstF32:
        loadConstF32(src, dest);
        break;
      case Stk::MemF32:
        loadMemF32(src, dest);
        break;
      case Stk::LocalF32:
        loadLocalF32(src, dest);
        break;
      case Stk::RegisterF32:
        loadRegisterF32(src, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected F32 on stack");
    }
  }

  void loadRef(const Stk& src, RegPtr dest) {
    switch (src.kind()) {
      case Stk::ConstRef:
        loadConstRef(src, dest);
        break;
      case Stk::MemRef:
        loadMemRef(src, dest);
        break;
      case Stk::LocalRef:
        loadLocalRef(src, dest);
        break;
      case Stk::RegisterRef:
        loadRegisterRef(src, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected ref on stack");
    }
  }

  // Flush all local and register value stack elements to memory.
  //
  // TODO / OPTIMIZE: As this is fairly expensive and causes worse
  // code to be emitted subsequently, it is useful to avoid calling
  // it.  (Bug 1316802)
  //
  // Some optimization has been done already.  Remaining
  // opportunities:
  //
  //  - It would be interesting to see if we can specialize it
  //    before calls with particularly simple signatures, or where
  //    we can do parallel assignment of register arguments, or
  //    similar.  See notes in emitCall().
  //
  //  - Operations that need specific registers: multiply, quotient,
  //    remainder, will tend to sync because the registers we need
  //    will tend to be allocated.  We may be able to avoid that by
  //    prioritizing registers differently (takeLast instead of
  //    takeFirst) but we may also be able to allocate an unused
  //    register on demand to free up one we need, thus avoiding the
  //    sync.  That type of fix would go into needI32().

  void sync() final {
    size_t start = 0;
    size_t lim = stk_.length();

    for (size_t i = lim; i > 0; i--) {
      // Memory opcodes are first in the enum, single check against MemLast is
      // fine.
      if (stk_[i - 1].kind() <= Stk::MemLast) {
        start = i;
        break;
      }
    }

    for (size_t i = start; i < lim; i++) {
      Stk& v = stk_[i];
      switch (v.kind()) {
        case Stk::LocalI32: {
          ScratchI32 scratch(*this);
          loadLocalI32(v, scratch);
          uint32_t offs = fr.pushPtr(scratch);
          v.setOffs(Stk::MemI32, offs);
          break;
        }
        case Stk::RegisterI32: {
          uint32_t offs = fr.pushPtr(v.i32reg());
          freeI32(v.i32reg());
          v.setOffs(Stk::MemI32, offs);
          break;
        }
        case Stk::LocalI64: {
          ScratchI32 scratch(*this);
#ifdef JS_PUNBOX64
          loadI64(v, fromI32(scratch));
          uint32_t offs = fr.pushPtr(scratch);
#else
          fr.loadLocalI64High(localFromSlot(v.slot(), MIRType::Int64), scratch);
          fr.pushPtr(scratch);
          fr.loadLocalI64Low(localFromSlot(v.slot(), MIRType::Int64), scratch);
          uint32_t offs = fr.pushPtr(scratch);
#endif
          v.setOffs(Stk::MemI64, offs);
          break;
        }
        case Stk::RegisterI64: {
#ifdef JS_PUNBOX64
          uint32_t offs = fr.pushPtr(v.i64reg().reg);
          freeI64(v.i64reg());
#else
          fr.pushPtr(v.i64reg().high);
          uint32_t offs = fr.pushPtr(v.i64reg().low);
          freeI64(v.i64reg());
#endif
          v.setOffs(Stk::MemI64, offs);
          break;
        }
        case Stk::LocalF64: {
          ScratchF64 scratch(*this);
          loadF64(v, scratch);
          uint32_t offs = fr.pushDouble(scratch);
          v.setOffs(Stk::MemF64, offs);
          break;
        }
        case Stk::RegisterF64: {
          uint32_t offs = fr.pushDouble(v.f64reg());
          freeF64(v.f64reg());
          v.setOffs(Stk::MemF64, offs);
          break;
        }
        case Stk::LocalF32: {
          ScratchF32 scratch(*this);
          loadF32(v, scratch);
          uint32_t offs = fr.pushFloat32(scratch);
          v.setOffs(Stk::MemF32, offs);
          break;
        }
        case Stk::RegisterF32: {
          uint32_t offs = fr.pushFloat32(v.f32reg());
          freeF32(v.f32reg());
          v.setOffs(Stk::MemF32, offs);
          break;
        }
        case Stk::LocalRef: {
          ScratchPtr scratch(*this);
          loadLocalRef(v, scratch);
          uint32_t offs = fr.pushPtr(scratch);
          v.setOffs(Stk::MemRef, offs);
          stackMapGenerator_.memRefsOnStk++;
          break;
        }
        case Stk::RegisterRef: {
          uint32_t offs = fr.pushPtr(v.refReg());
          freeRef(v.refReg());
          v.setOffs(Stk::MemRef, offs);
          stackMapGenerator_.memRefsOnStk++;
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  // Various methods for creating a stack map.  Stack maps are indexed by the
  // lowest address of the instruction immediately *after* the instruction of
  // interest.  In practice that means either: the return point of a call, the
  // instruction immediately after a trap instruction (the "resume"
  // instruction), or the instruction immediately following a no-op (when
  // debugging is enabled).

  // Create a vanilla stack map.
  MOZ_MUST_USE bool createStackMap(const char* who) {
    const ExitStubMapVector noExtras;
    return stackMapGenerator_.createStackMap(
        who, noExtras, masm.currentOffset(), HasRefTypedDebugFrame::No, stk_);
  }

  // Create a stack map as vanilla, but for a custom assembler offset.
  MOZ_MUST_USE bool createStackMap(const char* who,
                                   CodeOffset assemblerOffset) {
    const ExitStubMapVector noExtras;
    return stackMapGenerator_.createStackMap(who, noExtras,
                                             assemblerOffset.offset(),
                                             HasRefTypedDebugFrame::No, stk_);
  }

  // Create a stack map as vanilla, and note the presence of a ref-typed
  // DebugFrame on the stack.
  MOZ_MUST_USE bool createStackMap(const char* who,
                                   HasRefTypedDebugFrame refDebugFrame) {
    const ExitStubMapVector noExtras;
    return stackMapGenerator_.createStackMap(
        who, noExtras, masm.currentOffset(), refDebugFrame, stk_);
  }

  // The most general stack map construction.
  MOZ_MUST_USE bool createStackMap(const char* who,
                                   const ExitStubMapVector& extras,
                                   uint32_t assemblerOffset,
                                   HasRefTypedDebugFrame refDebugFrame) {
    return stackMapGenerator_.createStackMap(who, extras, assemblerOffset,
                                             refDebugFrame, stk_);
  }

  // This is an optimization used to avoid calling sync() for
  // setLocal(): if the local does not exist unresolved on the stack
  // then we can skip the sync.

  bool hasLocal(uint32_t slot) {
    for (size_t i = stk_.length(); i > 0; i--) {
      // Memory opcodes are first in the enum, single check against MemLast is
      // fine.
      Stk::Kind kind = stk_[i - 1].kind();
      if (kind <= Stk::MemLast) {
        return false;
      }

      // Local opcodes follow memory opcodes in the enum, single check against
      // LocalLast is sufficient.
      if (kind <= Stk::LocalLast && stk_[i - 1].slot() == slot) {
        return true;
      }
    }
    return false;
  }

  void syncLocal(uint32_t slot) {
    if (hasLocal(slot)) {
      sync();  // TODO / OPTIMIZE: Improve this?  (Bug 1316817)
    }
  }

  // Push the register r onto the stack.

  void pushI32(RegI32 r) {
    MOZ_ASSERT(!isAvailableI32(r));
    push(Stk(r));
  }

  void pushI64(RegI64 r) {
    MOZ_ASSERT(!isAvailableI64(r));
    push(Stk(r));
  }

  void pushRef(RegPtr r) {
    MOZ_ASSERT(!isAvailableRef(r));
    push(Stk(r));
  }

  void pushF64(RegF64 r) {
    MOZ_ASSERT(!isAvailableF64(r));
    push(Stk(r));
  }

  void pushF32(RegF32 r) {
    MOZ_ASSERT(!isAvailableF32(r));
    push(Stk(r));
  }

  // Push the value onto the stack.

  void pushI32(int32_t v) { push(Stk(v)); }

  void pushI64(int64_t v) { push(Stk(v)); }

  void pushRef(intptr_t v) { pushConstRef(v); }

  void pushF64(double v) { push(Stk(v)); }

  void pushF32(float v) { push(Stk(v)); }

  // Push the local slot onto the stack.  The slot will not be read
  // here; it will be read when it is consumed, or when a side
  // effect to the slot forces its value to be saved.

  void pushLocalI32(uint32_t slot) {
    stk_.infallibleEmplaceBack(Stk(Stk::LocalI32, slot));
  }

  void pushLocalI64(uint32_t slot) {
    stk_.infallibleEmplaceBack(Stk(Stk::LocalI64, slot));
  }

  void pushLocalRef(uint32_t slot) {
    stk_.infallibleEmplaceBack(Stk(Stk::LocalRef, slot));
  }

  void pushLocalF64(uint32_t slot) {
    stk_.infallibleEmplaceBack(Stk(Stk::LocalF64, slot));
  }

  void pushLocalF32(uint32_t slot) {
    stk_.infallibleEmplaceBack(Stk(Stk::LocalF32, slot));
  }

  // Call only from other popI32() variants.
  // v must be the stack top.  May pop the CPU stack.

  void popI32(const Stk& v, RegI32 dest) {
    MOZ_ASSERT(&v == &stk_.back());
    switch (v.kind()) {
      case Stk::ConstI32:
        loadConstI32(v, dest);
        break;
      case Stk::LocalI32:
        loadLocalI32(v, dest);
        break;
      case Stk::MemI32:
        fr.popPtr(dest);
        break;
      case Stk::RegisterI32:
        loadRegisterI32(v, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected int on stack");
    }
  }

  MOZ_MUST_USE RegI32 popI32() {
    Stk& v = stk_.back();
    RegI32 r;
    if (v.kind() == Stk::RegisterI32) {
      r = v.i32reg();
    } else {
      popI32(v, (r = needI32()));
    }
    stk_.popBack();
    return r;
  }

  RegI32 popI32(RegI32 specific) {
    Stk& v = stk_.back();

    if (!(v.kind() == Stk::RegisterI32 && v.i32reg() == specific)) {
      needI32(specific);
      popI32(v, specific);
      if (v.kind() == Stk::RegisterI32) {
        freeI32(v.i32reg());
      }
    }

    stk_.popBack();
    return specific;
  }

  // Call only from other popI64() variants.
  // v must be the stack top.  May pop the CPU stack.

  void popI64(const Stk& v, RegI64 dest) {
    MOZ_ASSERT(&v == &stk_.back());
    switch (v.kind()) {
      case Stk::ConstI64:
        loadConstI64(v, dest);
        break;
      case Stk::LocalI64:
        loadLocalI64(v, dest);
        break;
      case Stk::MemI64:
#ifdef JS_PUNBOX64
        fr.popPtr(dest.reg);
#else
        fr.popPtr(dest.low);
        fr.popPtr(dest.high);
#endif
        break;
      case Stk::RegisterI64:
        loadRegisterI64(v, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected long on stack");
    }
  }

  MOZ_MUST_USE RegI64 popI64() {
    Stk& v = stk_.back();
    RegI64 r;
    if (v.kind() == Stk::RegisterI64) {
      r = v.i64reg();
    } else {
      popI64(v, (r = needI64()));
    }
    stk_.popBack();
    return r;
  }

  // Note, the stack top can be in one half of "specific" on 32-bit
  // systems.  We can optimize, but for simplicity, if the register
  // does not match exactly, then just force the stack top to memory
  // and then read it back in.

  RegI64 popI64(RegI64 specific) {
    Stk& v = stk_.back();

    if (!(v.kind() == Stk::RegisterI64 && v.i64reg() == specific)) {
      needI64(specific);
      popI64(v, specific);
      if (v.kind() == Stk::RegisterI64) {
        freeI64(v.i64reg());
      }
    }

    stk_.popBack();
    return specific;
  }

  // Call only from other popRef() variants.
  // v must be the stack top.  May pop the CPU stack.

  void popRef(const Stk& v, RegPtr dest) {
    MOZ_ASSERT(&v == &stk_.back());
    switch (v.kind()) {
      case Stk::ConstRef:
        loadConstRef(v, dest);
        break;
      case Stk::LocalRef:
        loadLocalRef(v, dest);
        break;
      case Stk::MemRef:
        fr.popPtr(dest);
        break;
      case Stk::RegisterRef:
        loadRegisterRef(v, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected ref on stack");
    }
  }

  RegPtr popRef(RegPtr specific) {
    Stk& v = stk_.back();

    if (!(v.kind() == Stk::RegisterRef && v.refReg() == specific)) {
      needRef(specific);
      popRef(v, specific);
      if (v.kind() == Stk::RegisterRef) {
        freeRef(v.refReg());
      }
    }

    stk_.popBack();
    if (v.kind() == Stk::MemRef) {
      stackMapGenerator_.memRefsOnStk--;
    }
    return specific;
  }

  MOZ_MUST_USE RegPtr popRef() {
    Stk& v = stk_.back();
    RegPtr r;
    if (v.kind() == Stk::RegisterRef) {
      r = v.refReg();
    } else {
      popRef(v, (r = needRef()));
    }
    stk_.popBack();
    if (v.kind() == Stk::MemRef) {
      stackMapGenerator_.memRefsOnStk--;
    }
    return r;
  }

  // Call only from other popF64() variants.
  // v must be the stack top.  May pop the CPU stack.

  void popF64(const Stk& v, RegF64 dest) {
    MOZ_ASSERT(&v == &stk_.back());
    switch (v.kind()) {
      case Stk::ConstF64:
        loadConstF64(v, dest);
        break;
      case Stk::LocalF64:
        loadLocalF64(v, dest);
        break;
      case Stk::MemF64:
        fr.popDouble(dest);
        break;
      case Stk::RegisterF64:
        loadRegisterF64(v, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected double on stack");
    }
  }

  MOZ_MUST_USE RegF64 popF64() {
    Stk& v = stk_.back();
    RegF64 r;
    if (v.kind() == Stk::RegisterF64) {
      r = v.f64reg();
    } else {
      popF64(v, (r = needF64()));
    }
    stk_.popBack();
    return r;
  }

  RegF64 popF64(RegF64 specific) {
    Stk& v = stk_.back();

    if (!(v.kind() == Stk::RegisterF64 && v.f64reg() == specific)) {
      needF64(specific);
      popF64(v, specific);
      if (v.kind() == Stk::RegisterF64) {
        freeF64(v.f64reg());
      }
    }

    stk_.popBack();
    return specific;
  }

  // Call only from other popF32() variants.
  // v must be the stack top.  May pop the CPU stack.

  void popF32(const Stk& v, RegF32 dest) {
    MOZ_ASSERT(&v == &stk_.back());
    switch (v.kind()) {
      case Stk::ConstF32:
        loadConstF32(v, dest);
        break;
      case Stk::LocalF32:
        loadLocalF32(v, dest);
        break;
      case Stk::MemF32:
        fr.popFloat32(dest);
        break;
      case Stk::RegisterF32:
        loadRegisterF32(v, dest);
        break;
      default:
        MOZ_CRASH("Compiler bug: expected float on stack");
    }
  }

  MOZ_MUST_USE RegF32 popF32() {
    Stk& v = stk_.back();
    RegF32 r;
    if (v.kind() == Stk::RegisterF32) {
      r = v.f32reg();
    } else {
      popF32(v, (r = needF32()));
    }
    stk_.popBack();
    return r;
  }

  RegF32 popF32(RegF32 specific) {
    Stk& v = stk_.back();

    if (!(v.kind() == Stk::RegisterF32 && v.f32reg() == specific)) {
      needF32(specific);
      popF32(v, specific);
      if (v.kind() == Stk::RegisterF32) {
        freeF32(v.f32reg());
      }
    }

    stk_.popBack();
    return specific;
  }

  MOZ_MUST_USE bool popConstI32(int32_t* c) {
    Stk& v = stk_.back();
    if (v.kind() != Stk::ConstI32) {
      return false;
    }
    *c = v.i32val();
    stk_.popBack();
    return true;
  }

  MOZ_MUST_USE bool popConstI64(int64_t* c) {
    Stk& v = stk_.back();
    if (v.kind() != Stk::ConstI64) {
      return false;
    }
    *c = v.i64val();
    stk_.popBack();
    return true;
  }

  MOZ_MUST_USE bool peekConstI32(int32_t* c) {
    Stk& v = stk_.back();
    if (v.kind() != Stk::ConstI32) {
      return false;
    }
    *c = v.i32val();
    return true;
  }

  MOZ_MUST_USE bool peekConstI64(int64_t* c) {
    Stk& v = stk_.back();
    if (v.kind() != Stk::ConstI64) {
      return false;
    }
    *c = v.i64val();
    return true;
  }

  MOZ_MUST_USE bool popConstPositivePowerOfTwoI32(int32_t* c,
                                                  uint_fast8_t* power,
                                                  int32_t cutoff) {
    Stk& v = stk_.back();
    if (v.kind() != Stk::ConstI32) {
      return false;
    }
    *c = v.i32val();
    if (*c <= cutoff || !IsPowerOfTwo(static_cast<uint32_t>(*c))) {
      return false;
    }
    *power = FloorLog2(*c);
    stk_.popBack();
    return true;
  }

  MOZ_MUST_USE bool popConstPositivePowerOfTwoI64(int64_t* c,
                                                  uint_fast8_t* power,
                                                  int64_t cutoff) {
    Stk& v = stk_.back();
    if (v.kind() != Stk::ConstI64) {
      return false;
    }
    *c = v.i64val();
    if (*c <= cutoff || !IsPowerOfTwo(static_cast<uint64_t>(*c))) {
      return false;
    }
    *power = FloorLog2(*c);
    stk_.popBack();
    return true;
  }

  MOZ_MUST_USE bool peekLocalI32(uint32_t* local) {
    Stk& v = stk_.back();
    if (v.kind() != Stk::LocalI32) {
      return false;
    }
    *local = v.slot();
    return true;
  }

  // TODO / OPTIMIZE (Bug 1316818): At the moment we use ReturnReg
  // for JoinReg.  It is possible other choices would lead to better
  // register allocation, as ReturnReg is often first in the
  // register set and will be heavily wanted by the register
  // allocator that uses takeFirst().
  //
  // Obvious options:
  //  - pick a register at the back of the register set
  //  - pick a random register per block (different blocks have
  //    different join regs)
  //
  // On the other hand, we sync() before every block and only the
  // JoinReg is live out of the block.  But on the way out, we
  // currently pop the JoinReg before freeing regs to be discarded,
  // so there is a real risk of some pointless shuffling there.  If
  // we instead integrate the popping of the join reg into the
  // popping of the stack we can just use the JoinReg as it will
  // become available in that process.

  MOZ_MUST_USE Maybe<AnyReg> popJoinRegUnlessVoid(ExprType type) {
    switch (type.code()) {
      case ExprType::Void: {
        return Nothing();
      }
      case ExprType::I32: {
        DebugOnly<Stk::Kind> k(stk_.back().kind());
        MOZ_ASSERT(k == Stk::RegisterI32 || k == Stk::ConstI32 ||
                   k == Stk::MemI32 || k == Stk::LocalI32);
        return Some(AnyReg(popI32(joinRegI32_)));
      }
      case ExprType::I64: {
        DebugOnly<Stk::Kind> k(stk_.back().kind());
        MOZ_ASSERT(k == Stk::RegisterI64 || k == Stk::ConstI64 ||
                   k == Stk::MemI64 || k == Stk::LocalI64);
        return Some(AnyReg(popI64(joinRegI64_)));
      }
      case ExprType::F64: {
        DebugOnly<Stk::Kind> k(stk_.back().kind());
        MOZ_ASSERT(k == Stk::RegisterF64 || k == Stk::ConstF64 ||
                   k == Stk::MemF64 || k == Stk::LocalF64);
        return Some(AnyReg(popF64(joinRegF64_)));
      }
      case ExprType::F32: {
        DebugOnly<Stk::Kind> k(stk_.back().kind());
        MOZ_ASSERT(k == Stk::RegisterF32 || k == Stk::ConstF32 ||
                   k == Stk::MemF32 || k == Stk::LocalF32);
        return Some(AnyReg(popF32(joinRegF32_)));
      }
      case ExprType::Ref:
      case ExprType::NullRef:
      case ExprType::FuncRef:
      case ExprType::AnyRef: {
        DebugOnly<Stk::Kind> k(stk_.back().kind());
        MOZ_ASSERT(k == Stk::RegisterRef || k == Stk::ConstRef ||
                   k == Stk::MemRef || k == Stk::LocalRef);
        return Some(AnyReg(popRef(joinRegPtr_)));
      }
      default: {
        MOZ_CRASH("Compiler bug: unexpected expression type");
      }
    }
  }

  // If we ever start not sync-ing on entry to Block (but instead try to sync
  // lazily) then this may start asserting because it does not spill the
  // joinreg if the joinreg is already allocated.  Note, it *can't* spill the
  // joinreg in the contexts it's being used, so some other solution will need
  // to be found.

  MOZ_MUST_USE Maybe<AnyReg> captureJoinRegUnlessVoid(ExprType type) {
    switch (type.code()) {
      case ExprType::I32:
        MOZ_ASSERT(isAvailableI32(joinRegI32_));
        needI32(joinRegI32_);
        return Some(AnyReg(joinRegI32_));
      case ExprType::I64:
        MOZ_ASSERT(isAvailableI64(joinRegI64_));
        needI64(joinRegI64_);
        return Some(AnyReg(joinRegI64_));
      case ExprType::F32:
        MOZ_ASSERT(isAvailableF32(joinRegF32_));
        needF32(joinRegF32_);
        return Some(AnyReg(joinRegF32_));
      case ExprType::F64:
        MOZ_ASSERT(isAvailableF64(joinRegF64_));
        needF64(joinRegF64_);
        return Some(AnyReg(joinRegF64_));
      case ExprType::Ref:
      case ExprType::NullRef:
      case ExprType::FuncRef:
      case ExprType::AnyRef:
        MOZ_ASSERT(isAvailableRef(joinRegPtr_));
        needRef(joinRegPtr_);
        return Some(AnyReg(joinRegPtr_));
      case ExprType::Void:
        return Nothing();
      default:
        MOZ_CRASH("Compiler bug: unexpected type");
    }
  }

  void pushJoinRegUnlessVoid(const Maybe<AnyReg>& r) {
    if (!r) {
      return;
    }
    switch (r->tag) {
      case AnyReg::I32:
        pushI32(r->i32());
        break;
      case AnyReg::I64:
        pushI64(r->i64());
        break;
      case AnyReg::F64:
        pushF64(r->f64());
        break;
      case AnyReg::F32:
        pushF32(r->f32());
        break;
      case AnyReg::REF:
        pushRef(r->ref());
        break;
    }
  }

  void freeJoinRegUnlessVoid(const Maybe<AnyReg>& r) {
    if (!r) {
      return;
    }
    switch (r->tag) {
      case AnyReg::I32:
        freeI32(r->i32());
        break;
      case AnyReg::I64:
        freeI64(r->i64());
        break;
      case AnyReg::F64:
        freeF64(r->f64());
        break;
      case AnyReg::F32:
        freeF32(r->f32());
        break;
      case AnyReg::REF:
        freeRef(r->ref());
        break;
    }
  }

  // Return the amount of execution stack consumed by the top numval
  // values on the value stack.

  size_t stackConsumed(size_t numval) {
    size_t size = 0;
    MOZ_ASSERT(numval <= stk_.length());
    for (uint32_t i = stk_.length() - 1; numval > 0; numval--, i--) {
      Stk& v = stk_[i];
      switch (v.kind()) {
        case Stk::MemRef:
          size += BaseStackFrame::StackSizeOfPtr;
          break;
        case Stk::MemI32:
          size += BaseStackFrame::StackSizeOfPtr;
          break;
        case Stk::MemI64:
          size += BaseStackFrame::StackSizeOfInt64;
          break;
        case Stk::MemF64:
          size += BaseStackFrame::StackSizeOfDouble;
          break;
        case Stk::MemF32:
          size += BaseStackFrame::StackSizeOfFloat;
          break;
        default:
          break;
      }
    }
    return size;
  }

  void popValueStackTo(uint32_t stackSize) {
    for (uint32_t i = stk_.length(); i > stackSize; i--) {
      Stk& v = stk_[i - 1];
      switch (v.kind()) {
        case Stk::RegisterI32:
          freeI32(v.i32reg());
          break;
        case Stk::RegisterI64:
          freeI64(v.i64reg());
          break;
        case Stk::RegisterF64:
          freeF64(v.f64reg());
          break;
        case Stk::RegisterF32:
          freeF32(v.f32reg());
          break;
        case Stk::RegisterRef:
          freeRef(v.refReg());
          break;
        case Stk::MemRef:
          stackMapGenerator_.memRefsOnStk--;
          break;
        default:
          break;
      }
    }
    stk_.shrinkTo(stackSize);
  }

  void popValueStackBy(uint32_t items) {
    popValueStackTo(stk_.length() - items);
  }

  void dropValue() {
    if (peek(0).isMem()) {
      fr.popBytes(stackConsumed(1));
    }
    popValueStackBy(1);
  }

  // Peek at the stack, for calls.

  Stk& peek(uint32_t relativeDepth) {
    return stk_[stk_.length() - 1 - relativeDepth];
  }

#ifdef DEBUG
  // Check that we're not leaking registers by comparing the
  // state of the stack + available registers with the set of
  // all available registers.

  // Call this between opcodes.
  void performRegisterLeakCheck() {
    BaseRegAlloc::LeakCheck check(ra);
    for (size_t i = 0; i < stk_.length(); i++) {
      Stk& item = stk_[i];
      switch (item.kind_) {
        case Stk::RegisterI32:
          check.addKnownI32(item.i32reg());
          break;
        case Stk::RegisterI64:
          check.addKnownI64(item.i64reg());
          break;
        case Stk::RegisterF32:
          check.addKnownF32(item.f32reg());
          break;
        case Stk::RegisterF64:
          check.addKnownF64(item.f64reg());
          break;
        case Stk::RegisterRef:
          check.addKnownRef(item.refReg());
          break;
        default:
          break;
      }
    }
  }

  void assertStackInvariants() const {
    size_t size = 0;
    for (const Stk& v : stk_) {
      switch (v.kind()) {
        case Stk::MemRef:
          size += BaseStackFrame::StackSizeOfPtr;
          break;
        case Stk::MemI32:
          size += BaseStackFrame::StackSizeOfPtr;
          break;
        case Stk::MemI64:
          size += BaseStackFrame::StackSizeOfInt64;
          break;
        case Stk::MemF64:
          size += BaseStackFrame::StackSizeOfDouble;
          break;
        case Stk::MemF32:
          size += BaseStackFrame::StackSizeOfFloat;
          break;
        default:
          MOZ_ASSERT(!v.isMem());
          break;
      }
    }
    MOZ_ASSERT(size == fr.dynamicHeight());
  }

#endif

  ////////////////////////////////////////////////////////////
  //
  // Control stack

  void initControl(Control& item) {
    // Make sure the constructor was run properly
    MOZ_ASSERT(!item.stackHeight.isValid() && item.stackSize == UINT32_MAX);

    item.stackHeight = fr.stackHeight();
    item.stackSize = stk_.length();
    item.deadOnArrival = deadCode_;
    item.bceSafeOnEntry = bceSafe_;
  }

  Control& controlItem() { return iter_.controlItem(); }

  Control& controlItem(uint32_t relativeDepth) {
    return iter_.controlItem(relativeDepth);
  }

  Control& controlOutermost() { return iter_.controlOutermost(); }

  ////////////////////////////////////////////////////////////
  //
  // Labels

  void insertBreakablePoint(CallSiteDesc::Kind kind) {
    // The debug trap exit requires WasmTlsReg be loaded. However, since we
    // are emitting millions of these breakable points inline, we push this
    // loading of TLS into the FarJumpIsland created by linkCallSites.
    masm.nopPatchableToCall(CallSiteDesc(iter_.lastOpcodeOffset(), kind));
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Function prologue and epilogue.

  MOZ_MUST_USE bool beginFunction() {
    JitSpew(JitSpew_Codegen, "# ========================================");
    JitSpew(JitSpew_Codegen, "# Emitting wasm baseline code");
    JitSpew(JitSpew_Codegen,
            "# beginFunction: start of function prologue for index %d",
            (int)func_.index);

    // Make a start on the stack map for this function.  Inspect the args so
    // as to determine which of them are both in-memory and pointer-typed, and
    // add entries to machineStackTracker as appropriate.

    const ValTypeVector& argTys = env_.funcTypes[func_.index]->args();

    size_t nInboundStackArgBytes = StackArgAreaSizeUnaligned(argTys);
    MOZ_ASSERT(nInboundStackArgBytes % sizeof(void*) == 0);
    stackMapGenerator_.numStackArgWords = nInboundStackArgBytes / sizeof(void*);

    MOZ_ASSERT(stackMapGenerator_.machineStackTracker.length() == 0);
    if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
            stackMapGenerator_.numStackArgWords)) {
      return false;
    }

    for (ABIArgIter<const ValTypeVector> i(argTys); !i.done(); i++) {
      ABIArg argLoc = *i;
      const ValType& ty = argTys[i.index()];
      MOZ_ASSERT(ToMIRType(ty) != MIRType::Pointer);
      if (argLoc.kind() != ABIArg::Stack || !ty.isReference()) {
        continue;
      }
      uint32_t offset = argLoc.offsetFromArgBase();
      MOZ_ASSERT(offset < nInboundStackArgBytes);
      MOZ_ASSERT(offset % sizeof(void*) == 0);
      stackMapGenerator_.machineStackTracker.setGCPointer(offset /
                                                          sizeof(void*));
    }

    GenerateFunctionPrologue(
        masm, env_.funcTypes[func_.index]->id,
        env_.mode() == CompileMode::Tier1 ? Some(func_.index) : Nothing(),
        &offsets_);

    // GenerateFunctionPrologue pushes exactly one wasm::Frame's worth of
    // stuff, and none of the values are GC pointers.  Hence:
    if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
            sizeof(Frame) / sizeof(void*))) {
      return false;
    }

    // Initialize DebugFrame fields before the stack overflow trap so that
    // we have the invariant that all observable Frames in a debugEnabled
    // Module have valid DebugFrames.
    if (env_.debugEnabled()) {
#ifdef JS_CODEGEN_ARM64
      static_assert(DebugFrame::offsetOfFrame() % WasmStackAlignment == 0,
                    "aligned");
#endif
      masm.reserveStack(DebugFrame::offsetOfFrame());
      if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
              DebugFrame::offsetOfFrame() / sizeof(void*))) {
        return false;
      }

      masm.store32(
          Imm32(func_.index),
          Address(masm.getStackPointer(), DebugFrame::offsetOfFuncIndex()));
      masm.storePtr(ImmWord(0), Address(masm.getStackPointer(),
                                        DebugFrame::offsetOfFlagsWord()));
      // Zero out pointer values for safety, since it's not easy to establish
      // whether they will always be defined before a GC.

      // DebugFrame::resultRef_ and ::resultAnyRef_
      masm.storePtr(ImmWord(0), Address(masm.getStackPointer(),
                                        DebugFrame::offsetOfResults()));

      // DebugFrame::cachedReturnJSValue_
      for (size_t i = 0; i < sizeof(js::Value) / sizeof(void*); i++) {
        masm.storePtr(ImmWord(0),
                      Address(masm.getStackPointer(),
                              DebugFrame::offsetOfCachedReturnJSValue() +
                                  i * sizeof(void*)));
      }
    }

    // Generate a stack-overflow check and its associated stack map.

    fr.checkStack(ABINonArgReg0, BytecodeOffset(func_.lineOrBytecode));

    const ValTypeVector& args = funcType().args();
    ExitStubMapVector extras;
    if (!stackMapGenerator_.generateStackmapEntriesForTrapExit(args, &extras)) {
      return false;
    }
    if (!createStackMap("stack check", extras, masm.currentOffset(),
                        HasRefTypedDebugFrame::No)) {
      return false;
    }

    size_t reservedBytes = fr.fixedAllocSize() - masm.framePushed();
    MOZ_ASSERT(0 == (reservedBytes % sizeof(void*)));

    masm.reserveStack(reservedBytes);
    fr.onFixedStackAllocated();
    if (!stackMapGenerator_.machineStackTracker.pushNonGCPointers(
            reservedBytes / sizeof(void*))) {
      return false;
    }

    // Locals are stack allocated.  Mark ref-typed ones in the stackmap
    // accordingly.
    for (const Local& l : localInfo_) {
      if (l.type == MIRType::RefOrNull) {
        uint32_t offs = fr.localOffset(l);
        MOZ_ASSERT(0 == (offs % sizeof(void*)));
        stackMapGenerator_.machineStackTracker.setGCPointer(offs /
                                                            sizeof(void*));
      }
    }

    // Copy arguments from registers to stack.
    for (ABIArgIter<const ValTypeVector> i(args); !i.done(); i++) {
      if (!i->argInRegister()) {
        continue;
      }
      Local& l = localInfo_[i.index()];
      switch (i.mirType()) {
        case MIRType::Int32:
          fr.storeLocalI32(RegI32(i->gpr()), l);
          break;
        case MIRType::Int64:
          fr.storeLocalI64(RegI64(i->gpr64()), l);
          break;
        case MIRType::RefOrNull: {
          DebugOnly<uint32_t> offs = fr.localOffset(l);
          MOZ_ASSERT(0 == (offs % sizeof(void*)));
          fr.storeLocalPtr(RegPtr(i->gpr()), l);
          // We should have just visited this local in the preceding loop.
          MOZ_ASSERT(stackMapGenerator_.machineStackTracker.isGCPointer(
              offs / sizeof(void*)));
          break;
        }
        case MIRType::Double:
          fr.storeLocalF64(RegF64(i->fpu()), l);
          break;
        case MIRType::Float32:
          fr.storeLocalF32(RegF32(i->fpu()), l);
          break;
        default:
          MOZ_CRASH("Function argument type");
      }
    }

    fr.zeroLocals(&ra);

    if (env_.debugEnabled()) {
      insertBreakablePoint(CallSiteDesc::EnterFrame);
      if (!createStackMap("debug: breakable point")) {
        return false;
      }
    }

    JitSpew(JitSpew_Codegen,
            "# beginFunction: enter body with masm.framePushed = %u",
            masm.framePushed());
    MOZ_ASSERT(stackMapGenerator_.framePushedAtEntryToBody.isNothing());
    stackMapGenerator_.framePushedAtEntryToBody.emplace(masm.framePushed());

    return true;
  }

  void saveResult() {
    MOZ_ASSERT(env_.debugEnabled());
    size_t debugFrameOffset = masm.framePushed() - DebugFrame::offsetOfFrame();
    Address resultsAddress(masm.getStackPointer(),
                           debugFrameOffset + DebugFrame::offsetOfResults());
    switch (funcType().ret().code()) {
      case ExprType::Void:
        break;
      case ExprType::I32:
        masm.store32(RegI32(ReturnReg), resultsAddress);
        break;
      case ExprType::I64:
        masm.store64(RegI64(ReturnReg64), resultsAddress);
        break;
      case ExprType::F64:
        masm.storeDouble(RegF64(ReturnDoubleReg), resultsAddress);
        break;
      case ExprType::F32:
        masm.storeFloat32(RegF32(ReturnFloat32Reg), resultsAddress);
        break;
      case ExprType::Ref:
      case ExprType::FuncRef:
      case ExprType::AnyRef:
        masm.storePtr(RegPtr(ReturnReg), resultsAddress);
        break;
      case ExprType::NullRef:
      default:
        MOZ_CRASH("Function return type");
    }
  }

  void restoreResult() {
    MOZ_ASSERT(env_.debugEnabled());
    size_t debugFrameOffset = masm.framePushed() - DebugFrame::offsetOfFrame();
    Address resultsAddress(masm.getStackPointer(),
                           debugFrameOffset + DebugFrame::offsetOfResults());
    switch (funcType().ret().code()) {
      case ExprType::Void:
        break;
      case ExprType::I32:
        masm.load32(resultsAddress, RegI32(ReturnReg));
        break;
      case ExprType::I64:
        masm.load64(resultsAddress, RegI64(ReturnReg64));
        break;
      case ExprType::F64:
        masm.loadDouble(resultsAddress, RegF64(ReturnDoubleReg));
        break;
      case ExprType::F32:
        masm.loadFloat32(resultsAddress, RegF32(ReturnFloat32Reg));
        break;
      case ExprType::Ref:
      case ExprType::FuncRef:
      case ExprType::AnyRef:
        masm.loadPtr(resultsAddress, RegPtr(ReturnReg));
        break;
      case ExprType::NullRef:
      default:
        MOZ_CRASH("Function return type");
    }
  }

  MOZ_MUST_USE bool endFunction() {
    JitSpew(JitSpew_Codegen, "# endFunction: start of function epilogue");

    // Always branch to returnLabel_.
    masm.breakpoint();

    // Patch the add in the prologue so that it checks against the correct
    // frame size. Flush the constant pool in case it needs to be patched.
    masm.flush();

    // Precondition for patching.
    if (masm.oom()) {
      return false;
    }

    fr.patchCheckStack();

    masm.bind(&returnLabel_);

    if (env_.debugEnabled()) {
      // If the return type is a ref, we need to note that in the stack maps
      // generated here.  Note that this assumes that DebugFrame::result* and
      // DebugFrame::cachedReturnJSValue_ are either both ref-typed or they
      // are both not ref-typed.  It can't represent the situation where one
      // is and the other isn't.
      HasRefTypedDebugFrame refDebugFrame = funcType().ret().isReference()
                                                ? HasRefTypedDebugFrame::Yes
                                                : HasRefTypedDebugFrame::No;

      // Store and reload the return value from DebugFrame::return so that
      // it can be clobbered, and/or modified by the debug trap.
      saveResult();
      insertBreakablePoint(CallSiteDesc::Breakpoint);
      if (!createStackMap("debug: breakpoint", refDebugFrame)) {
        return false;
      }
      insertBreakablePoint(CallSiteDesc::LeaveFrame);
      if (!createStackMap("debug: leave frame", refDebugFrame)) {
        return false;
      }
      restoreResult();
    }

    GenerateFunctionEpilogue(masm, fr.fixedAllocSize(), &offsets_);

#if defined(JS_ION_PERF)
    // FIXME - profiling code missing.  No bug for this.

    // Note the end of the inline code and start of the OOL code.
    // gen->perfSpewer().noteEndInlineCode(masm);
#endif

    JitSpew(JitSpew_Codegen, "# endFunction: end of function epilogue");
    JitSpew(JitSpew_Codegen, "# endFunction: start of OOL code");
    if (!generateOutOfLineCode()) {
      return false;
    }

    offsets_.end = masm.currentOffset();

    if (!fr.checkStackHeight()) {
      return false;
    }

    JitSpew(JitSpew_Codegen, "# endFunction: end of OOL code for index %d",
            (int)func_.index);
    return !masm.oom();
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Calls.

  struct FunctionCall {
    explicit FunctionCall(uint32_t lineOrBytecode)
        : lineOrBytecode(lineOrBytecode),
          isInterModule(false),
          usesSystemAbi(false),
#ifdef JS_CODEGEN_ARM
          hardFP(true),
#endif
          frameAlignAdjustment(0),
          stackArgAreaSize(0) {
    }

    uint32_t lineOrBytecode;
    ABIArgGenerator abi;
    bool isInterModule;
    bool usesSystemAbi;
#ifdef JS_CODEGEN_ARM
    bool hardFP;
#endif
    size_t frameAlignAdjustment;
    size_t stackArgAreaSize;
  };

  void beginCall(FunctionCall& call, UseABI useABI, InterModule interModule) {
    MOZ_ASSERT_IF(useABI == UseABI::Builtin, interModule == InterModule::False);

    call.isInterModule = interModule == InterModule::True;
    call.usesSystemAbi = useABI == UseABI::System;

    if (call.usesSystemAbi) {
      // Call-outs need to use the appropriate system ABI.
#if defined(JS_CODEGEN_ARM)
      call.hardFP = UseHardFpABI();
      call.abi.setUseHardFp(call.hardFP);
#elif defined(JS_CODEGEN_MIPS32)
      call.abi.enforceO32ABI();
#endif
    } else {
#if defined(JS_CODEGEN_ARM)
      MOZ_ASSERT(call.hardFP,
                 "All private ABIs pass FP arguments in registers");
#endif
    }

    // Use masm.framePushed() because the value we want here does not depend
    // on the height of the frame's stack area, but the actual size of the
    // allocated frame.
    call.frameAlignAdjustment = ComputeByteAlignment(
        masm.framePushed() + sizeof(Frame), JitStackAlignment);
  }

  void endCall(FunctionCall& call, size_t stackSpace) {
    size_t adjustment = call.stackArgAreaSize + call.frameAlignAdjustment;
    fr.freeArgAreaAndPopBytes(adjustment, stackSpace);

    MOZ_ASSERT(
        stackMapGenerator_.framePushedExcludingOutboundCallArgs.isSome());
    stackMapGenerator_.framePushedExcludingOutboundCallArgs.reset();

    if (call.isInterModule) {
      masm.loadWasmTlsRegFromFrame();
      masm.loadWasmPinnedRegsFromTls();
      masm.switchToWasmTlsRealm(ABINonArgReturnReg0, ABINonArgReturnReg1);
    } else if (call.usesSystemAbi) {
      // On x86 there are no pinned registers, so don't waste time
      // reloading the Tls.
#ifndef JS_CODEGEN_X86
      masm.loadWasmTlsRegFromFrame();
      masm.loadWasmPinnedRegsFromTls();
#endif
    }
  }

  void startCallArgs(size_t stackArgAreaSizeUnaligned, FunctionCall* call) {
    size_t stackArgAreaSizeAligned =
        AlignStackArgAreaSize(stackArgAreaSizeUnaligned);
    MOZ_ASSERT(stackArgAreaSizeUnaligned <= stackArgAreaSizeAligned);

    // Record the masm.framePushed() value at this point, before we push args
    // for the call, but including the alignment space placed above the args.
    // This defines the lower limit of the stackmap that will be created for
    // this call.
    MOZ_ASSERT(
        stackMapGenerator_.framePushedExcludingOutboundCallArgs.isNothing());
    stackMapGenerator_.framePushedExcludingOutboundCallArgs.emplace(
        // However much we've pushed so far
        masm.framePushed() +
        // Extra space we'll push to get the frame aligned
        call->frameAlignAdjustment +
        // Extra space we'll push to get the outbound arg area 16-aligned
        (stackArgAreaSizeAligned - stackArgAreaSizeUnaligned));

    call->stackArgAreaSize = stackArgAreaSizeAligned;

    size_t adjustment = call->stackArgAreaSize + call->frameAlignAdjustment;
    fr.allocArgArea(adjustment);
  }

  const ABIArg reservePointerArgument(FunctionCall* call) {
    return call->abi.next(MIRType::Pointer);
  }

  // TODO / OPTIMIZE (Bug 1316821): Note passArg is used only in one place.
  // (Or it was, until Luke wandered through, but that can be fixed again.)
  // I'm not saying we should manually inline it, but we could hoist the
  // dispatch into the caller and have type-specific implementations of
  // passArg: passArgI32(), etc.  Then those might be inlined, at least in PGO
  // builds.
  //
  // The bulk of the work here (60%) is in the next() call, though.
  //
  // Notably, since next() is so expensive, StackArgAreaSizeUnaligned()
  // becomes expensive too.
  //
  // Somehow there could be a trick here where the sequence of argument types
  // (read from the input stream) leads to a cached entry for
  // StackArgAreaSizeUnaligned() and for how to pass arguments...
  //
  // But at least we could reduce the cost of StackArgAreaSizeUnaligned() by
  // first reading the argument types into a (reusable) vector, then we have
  // the outgoing size at low cost, and then we can pass args based on the
  // info we read.

  void passArg(ValType type, const Stk& arg, FunctionCall* call) {
    switch (type.code()) {
      case ValType::I32: {
        ABIArg argLoc = call->abi.next(MIRType::Int32);
        if (argLoc.kind() == ABIArg::Stack) {
          ScratchI32 scratch(*this);
          loadI32(arg, scratch);
          masm.store32(scratch, Address(masm.getStackPointer(),
                                        argLoc.offsetFromArgBase()));
        } else {
          loadI32(arg, RegI32(argLoc.gpr()));
        }
        break;
      }
      case ValType::I64: {
        ABIArg argLoc = call->abi.next(MIRType::Int64);
        if (argLoc.kind() == ABIArg::Stack) {
          ScratchI32 scratch(*this);
#ifdef JS_PUNBOX64
          loadI64(arg, fromI32(scratch));
          masm.storePtr(scratch, Address(masm.getStackPointer(),
                                         argLoc.offsetFromArgBase()));
#else
          loadI64Low(arg, scratch);
          masm.store32(scratch, LowWord(Address(masm.getStackPointer(),
                                                argLoc.offsetFromArgBase())));
          loadI64High(arg, scratch);
          masm.store32(scratch, HighWord(Address(masm.getStackPointer(),
                                                 argLoc.offsetFromArgBase())));
#endif
        } else {
          loadI64(arg, RegI64(argLoc.gpr64()));
        }
        break;
      }
      case ValType::F64: {
        ABIArg argLoc = call->abi.next(MIRType::Double);
        switch (argLoc.kind()) {
          case ABIArg::Stack: {
            ScratchF64 scratch(*this);
            loadF64(arg, scratch);
            masm.storeDouble(scratch, Address(masm.getStackPointer(),
                                              argLoc.offsetFromArgBase()));
            break;
          }
#if defined(JS_CODEGEN_REGISTER_PAIR)
          case ABIArg::GPR_PAIR: {
#  if defined(JS_CODEGEN_ARM)
            ScratchF64 scratch(*this);
            loadF64(arg, scratch);
            masm.ma_vxfer(scratch, argLoc.evenGpr(), argLoc.oddGpr());
            break;
#  elif defined(JS_CODEGEN_MIPS32)
            ScratchF64 scratch(*this);
            loadF64(arg, scratch);
            MOZ_ASSERT(MOZ_LITTLE_ENDIAN);
            masm.moveFromDoubleLo(scratch, argLoc.evenGpr());
            masm.moveFromDoubleHi(scratch, argLoc.oddGpr());
            break;
#  else
            MOZ_CRASH("BaseCompiler platform hook: passArg F64 pair");
#  endif
          }
#endif
          case ABIArg::FPU: {
            loadF64(arg, RegF64(argLoc.fpu()));
            break;
          }
          case ABIArg::GPR: {
            MOZ_CRASH("Unexpected parameter passing discipline");
          }
          case ABIArg::Uninitialized:
            MOZ_CRASH("Uninitialized ABIArg kind");
        }
        break;
      }
      case ValType::F32: {
        ABIArg argLoc = call->abi.next(MIRType::Float32);
        switch (argLoc.kind()) {
          case ABIArg::Stack: {
            ScratchF32 scratch(*this);
            loadF32(arg, scratch);
            masm.storeFloat32(scratch, Address(masm.getStackPointer(),
                                               argLoc.offsetFromArgBase()));
            break;
          }
          case ABIArg::GPR: {
            ScratchF32 scratch(*this);
            loadF32(arg, scratch);
            masm.moveFloat32ToGPR(scratch, argLoc.gpr());
            break;
          }
          case ABIArg::FPU: {
            loadF32(arg, RegF32(argLoc.fpu()));
            break;
          }
#if defined(JS_CODEGEN_REGISTER_PAIR)
          case ABIArg::GPR_PAIR: {
            MOZ_CRASH("Unexpected parameter passing discipline");
          }
#endif
          case ABIArg::Uninitialized:
            MOZ_CRASH("Uninitialized ABIArg kind");
        }
        break;
      }
      case ValType::Ref:
      case ValType::FuncRef:
      case ValType::AnyRef: {
        ABIArg argLoc = call->abi.next(MIRType::RefOrNull);
        if (argLoc.kind() == ABIArg::Stack) {
          ScratchPtr scratch(*this);
          loadRef(arg, scratch);
          masm.storePtr(scratch, Address(masm.getStackPointer(),
                                         argLoc.offsetFromArgBase()));
        } else {
          loadRef(arg, RegPtr(argLoc.gpr()));
        }
        break;
      }
      case ValType::NullRef:
        MOZ_CRASH("NullRef not expressible");
      default:
        MOZ_CRASH("Function argument type");
    }
  }

  CodeOffset callDefinition(uint32_t funcIndex, const FunctionCall& call) {
    CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Func);
    return masm.call(desc, funcIndex);
  }

  CodeOffset callSymbolic(SymbolicAddress callee, const FunctionCall& call) {
    CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Symbolic);
    return masm.call(desc, callee);
  }

  // Precondition: sync()

  CodeOffset callIndirect(uint32_t funcTypeIndex, uint32_t tableIndex,
                          const Stk& indexVal, const FunctionCall& call) {
    const FuncTypeWithId& funcType = env_.types[funcTypeIndex].funcType();
    MOZ_ASSERT(funcType.id.kind() != FuncTypeIdDescKind::None);

    const TableDesc& table = env_.tables[tableIndex];

    loadI32(indexVal, RegI32(WasmTableCallIndexReg));

    CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Dynamic);
    CalleeDesc callee = CalleeDesc::wasmTable(table, funcType.id);
    return masm.wasmCallIndirect(desc, callee, NeedsBoundsCheck(true));
  }

  // Precondition: sync()

  CodeOffset callImport(unsigned globalDataOffset, const FunctionCall& call) {
    CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Dynamic);
    CalleeDesc callee = CalleeDesc::import(globalDataOffset);
    return masm.wasmCallImport(desc, callee);
  }

  CodeOffset builtinCall(SymbolicAddress builtin, const FunctionCall& call) {
    return callSymbolic(builtin, call);
  }

  CodeOffset builtinInstanceMethodCall(const SymbolicAddressSignature& builtin,
                                       const ABIArg& instanceArg,
                                       const FunctionCall& call) {
    // Builtin method calls assume the TLS register has been set.
    masm.loadWasmTlsRegFromFrame();

    CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Symbolic);
    return masm.wasmCallBuiltinInstanceMethod(
        desc, instanceArg, builtin.identity, builtin.failureMode);
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Sundry low-level code generators.

  // The compiler depends on moveImm32() clearing the high bits of a 64-bit
  // register on 64-bit systems except MIPS64 where high bits are sign extended
  // from lower bits.

  void moveImm32(int32_t v, RegI32 dest) { masm.move32(Imm32(v), dest); }

  void moveImm64(int64_t v, RegI64 dest) { masm.move64(Imm64(v), dest); }

  void moveImmRef(intptr_t v, RegPtr dest) { masm.movePtr(ImmWord(v), dest); }

  void moveImmF32(float f, RegF32 dest) { masm.loadConstantFloat32(f, dest); }

  void moveImmF64(double d, RegF64 dest) { masm.loadConstantDouble(d, dest); }

  MOZ_MUST_USE bool addInterruptCheck() {
    ScratchI32 tmp(*this);
    masm.loadWasmTlsRegFromFrame(tmp);
    masm.wasmInterruptCheck(tmp, bytecodeOffset());
    return createStackMap("addInterruptCheck");
  }

  void jumpTable(const LabelVector& labels, Label* theTable) {
    // Flush constant pools to ensure that the table is never interrupted by
    // constant pool entries.
    masm.flush();

#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
    // Prevent nop sequences to appear in the jump table.
    AutoForbidNops afn(&masm);
#endif
    masm.bind(theTable);

    for (uint32_t i = 0; i < labels.length(); i++) {
      CodeLabel cl;
      masm.writeCodePointer(&cl);
      cl.target()->bind(labels[i].offset());
      masm.addCodeLabel(cl);
    }
  }

  void tableSwitch(Label* theTable, RegI32 switchValue, Label* dispatchCode) {
    masm.bind(dispatchCode);

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
    ScratchI32 scratch(*this);
    CodeLabel tableCl;

    masm.mov(&tableCl, scratch);

    tableCl.target()->bind(theTable->offset());
    masm.addCodeLabel(tableCl);

    masm.jmp(Operand(scratch, switchValue, ScalePointer));
#elif defined(JS_CODEGEN_ARM)
    // Flush constant pools: offset must reflect the distance from the MOV
    // to the start of the table; as the address of the MOV is given by the
    // label, nothing must come between the bind() and the ma_mov().
    AutoForbidPoolsAndNops afp(&masm,
                               /* number of instructions in scope = */ 5);

    ScratchI32 scratch(*this);

    // Compute the offset from the ma_mov instruction to the jump table.
    Label here;
    masm.bind(&here);
    uint32_t offset = here.offset() - theTable->offset();

    // Read PC+8
    masm.ma_mov(pc, scratch);

    // ARM scratch register is required by ma_sub.
    ScratchRegisterScope arm_scratch(*this);

    // Compute the absolute table base pointer into `scratch`, offset by 8
    // to account for the fact that ma_mov read PC+8.
    masm.ma_sub(Imm32(offset + 8), scratch, arm_scratch);

    // Jump indirect via table element.
    masm.ma_ldr(DTRAddr(scratch, DtrRegImmShift(switchValue, LSL, 2)), pc,
                Offset, Assembler::Always);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    ScratchI32 scratch(*this);
    CodeLabel tableCl;

    masm.ma_li(scratch, &tableCl);

    tableCl.target()->bind(theTable->offset());
    masm.addCodeLabel(tableCl);

    masm.branchToComputedAddress(BaseIndex(scratch, switchValue, ScalePointer));
#elif defined(JS_CODEGEN_ARM64)
    AutoForbidPoolsAndNops afp(&masm,
                               /* number of instructions in scope = */ 4);

    ScratchI32 scratch(*this);

    ARMRegister s(scratch, 64);
    ARMRegister v(switchValue, 64);
    masm.Adr(s, theTable);
    masm.Add(s, s, Operand(v, vixl::LSL, 3));
    masm.Ldr(s, MemOperand(s, 0));
    masm.Br(s);
#else
    MOZ_CRASH("BaseCompiler platform hook: tableSwitch");
#endif
  }

  RegI32 captureReturnedI32() {
    RegI32 r = RegI32(ReturnReg);
    MOZ_ASSERT(isAvailableI32(r));
    needI32(r);
    return r;
  }

  RegI64 captureReturnedI64() {
    RegI64 r = RegI64(ReturnReg64);
    MOZ_ASSERT(isAvailableI64(r));
    needI64(r);
    return r;
  }

  RegF32 captureReturnedF32(const FunctionCall& call) {
    RegF32 r = RegF32(ReturnFloat32Reg);
    MOZ_ASSERT(isAvailableF32(r));
    needF32(r);
#if defined(JS_CODEGEN_ARM)
    if (call.usesSystemAbi && !call.hardFP) {
      masm.ma_vxfer(ReturnReg, r);
    }
#endif
    return r;
  }

  RegF64 captureReturnedF64(const FunctionCall& call) {
    RegF64 r = RegF64(ReturnDoubleReg);
    MOZ_ASSERT(isAvailableF64(r));
    needF64(r);
#if defined(JS_CODEGEN_ARM)
    if (call.usesSystemAbi && !call.hardFP) {
      masm.ma_vxfer(ReturnReg64.low, ReturnReg64.high, r);
    }
#endif
    return r;
  }

  RegPtr captureReturnedRef() {
    RegPtr r = RegPtr(ReturnReg);
    MOZ_ASSERT(isAvailableRef(r));
    needRef(r);
    return r;
  }

  void returnCleanup(bool popStack) {
    if (popStack) {
      fr.popStackBeforeBranch(controlOutermost().stackHeight);
    }
    masm.jump(&returnLabel_);
  }

  void checkDivideByZeroI32(RegI32 rhs, RegI32 srcDest, Label* done) {
    Label nonZero;
    masm.branchTest32(Assembler::NonZero, rhs, rhs, &nonZero);
    trap(Trap::IntegerDivideByZero);
    masm.bind(&nonZero);
  }

  void checkDivideByZeroI64(RegI64 r) {
    Label nonZero;
    ScratchI32 scratch(*this);
    masm.branchTest64(Assembler::NonZero, r, r, scratch, &nonZero);
    trap(Trap::IntegerDivideByZero);
    masm.bind(&nonZero);
  }

  void checkDivideSignedOverflowI32(RegI32 rhs, RegI32 srcDest, Label* done,
                                    bool zeroOnOverflow) {
    Label notMin;
    masm.branch32(Assembler::NotEqual, srcDest, Imm32(INT32_MIN), &notMin);
    if (zeroOnOverflow) {
      masm.branch32(Assembler::NotEqual, rhs, Imm32(-1), &notMin);
      moveImm32(0, srcDest);
      masm.jump(done);
    } else {
      masm.branch32(Assembler::NotEqual, rhs, Imm32(-1), &notMin);
      trap(Trap::IntegerOverflow);
    }
    masm.bind(&notMin);
  }

  void checkDivideSignedOverflowI64(RegI64 rhs, RegI64 srcDest, Label* done,
                                    bool zeroOnOverflow) {
    Label notmin;
    masm.branch64(Assembler::NotEqual, srcDest, Imm64(INT64_MIN), &notmin);
    masm.branch64(Assembler::NotEqual, rhs, Imm64(-1), &notmin);
    if (zeroOnOverflow) {
      masm.xor64(srcDest, srcDest);
      masm.jump(done);
    } else {
      trap(Trap::IntegerOverflow);
    }
    masm.bind(&notmin);
  }

#ifndef RABALDR_INT_DIV_I64_CALLOUT
  void quotientI64(RegI64 rhs, RegI64 srcDest, RegI64 reserved,
                   IsUnsigned isUnsigned, bool isConst, int64_t c) {
    Label done;

    if (!isConst || c == 0) {
      checkDivideByZeroI64(rhs);
    }

    if (!isUnsigned && (!isConst || c == -1)) {
      checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(false));
    }

#  if defined(JS_CODEGEN_X64)
    // The caller must set up the following situation.
    MOZ_ASSERT(srcDest.reg == rax);
    MOZ_ASSERT(reserved == specific_.rdx);
    if (isUnsigned) {
      masm.xorq(rdx, rdx);
      masm.udivq(rhs.reg);
    } else {
      masm.cqo();
      masm.idivq(rhs.reg);
    }
#  elif defined(JS_CODEGEN_MIPS64)
    if (isUnsigned) {
      masm.as_ddivu(srcDest.reg, rhs.reg);
    } else {
      masm.as_ddiv(srcDest.reg, rhs.reg);
    }
    masm.as_mflo(srcDest.reg);
#  elif defined(JS_CODEGEN_ARM64)
    ARMRegister sd(srcDest.reg, 64);
    ARMRegister r(rhs.reg, 64);
    if (isUnsigned) {
      masm.Udiv(sd, sd, r);
    } else {
      masm.Sdiv(sd, sd, r);
    }
#  else
    MOZ_CRASH("BaseCompiler platform hook: quotientI64");
#  endif
    masm.bind(&done);
  }

  void remainderI64(RegI64 rhs, RegI64 srcDest, RegI64 reserved,
                    IsUnsigned isUnsigned, bool isConst, int64_t c) {
    Label done;

    if (!isConst || c == 0) {
      checkDivideByZeroI64(rhs);
    }

    if (!isUnsigned && (!isConst || c == -1)) {
      checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(true));
    }

#  if defined(JS_CODEGEN_X64)
    // The caller must set up the following situation.
    MOZ_ASSERT(srcDest.reg == rax);
    MOZ_ASSERT(reserved == specific_.rdx);

    if (isUnsigned) {
      masm.xorq(rdx, rdx);
      masm.udivq(rhs.reg);
    } else {
      masm.cqo();
      masm.idivq(rhs.reg);
    }
    masm.movq(rdx, rax);
#  elif defined(JS_CODEGEN_MIPS64)
    if (isUnsigned) {
      masm.as_ddivu(srcDest.reg, rhs.reg);
    } else {
      masm.as_ddiv(srcDest.reg, rhs.reg);
    }
    masm.as_mfhi(srcDest.reg);
#  elif defined(JS_CODEGEN_ARM64)
    MOZ_ASSERT(reserved.isInvalid());
    ARMRegister sd(srcDest.reg, 64);
    ARMRegister r(rhs.reg, 64);
    ScratchI32 temp(*this);
    ARMRegister t(temp, 64);
    if (isUnsigned) {
      masm.Udiv(t, sd, r);
    } else {
      masm.Sdiv(t, sd, r);
    }
    masm.Mul(t, t, r);
    masm.Sub(sd, sd, t);
#  else
    MOZ_CRASH("BaseCompiler platform hook: remainderI64");
#  endif
    masm.bind(&done);
  }
#endif  // RABALDR_INT_DIV_I64_CALLOUT

  RegI32 needRotate64Temp() {
#if defined(JS_CODEGEN_X86)
    return needI32();
#elif defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM) ||    \
    defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_MIPS32) || \
    defined(JS_CODEGEN_MIPS64)
    return RegI32::Invalid();
#else
    MOZ_CRASH("BaseCompiler platform hook: needRotate64Temp");
#endif
  }

  void maskShiftCount32(RegI32 r) {
#if defined(JS_CODEGEN_ARM)
    masm.and32(Imm32(31), r);
#endif
  }

  RegI32 needPopcnt32Temp() {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    return AssemblerX86Shared::HasPOPCNT() ? RegI32::Invalid() : needI32();
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    return needI32();
#else
    MOZ_CRASH("BaseCompiler platform hook: needPopcnt32Temp");
#endif
  }

  RegI32 needPopcnt64Temp() {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    return AssemblerX86Shared::HasPOPCNT() ? RegI32::Invalid() : needI32();
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    return needI32();
#else
    MOZ_CRASH("BaseCompiler platform hook: needPopcnt64Temp");
#endif
  }

  class OutOfLineTruncateCheckF32OrF64ToI32 : public OutOfLineCode {
    AnyReg src;
    RegI32 dest;
    TruncFlags flags;
    BytecodeOffset off;

   public:
    OutOfLineTruncateCheckF32OrF64ToI32(AnyReg src, RegI32 dest,
                                        TruncFlags flags, BytecodeOffset off)
        : src(src), dest(dest), flags(flags), off(off) {}

    virtual void generate(MacroAssembler* masm) override {
      if (src.tag == AnyReg::F32) {
        masm->oolWasmTruncateCheckF32ToI32(src.f32(), dest, flags, off,
                                           rejoin());
      } else if (src.tag == AnyReg::F64) {
        masm->oolWasmTruncateCheckF64ToI32(src.f64(), dest, flags, off,
                                           rejoin());
      } else {
        MOZ_CRASH("unexpected type");
      }
    }
  };

  MOZ_MUST_USE bool truncateF32ToI32(RegF32 src, RegI32 dest,
                                     TruncFlags flags) {
    BytecodeOffset off = bytecodeOffset();
    OutOfLineCode* ool =
        addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI32(
            AnyReg(src), dest, flags, off));
    if (!ool) {
      return false;
    }
    bool isSaturating = flags & TRUNC_SATURATING;
    if (flags & TRUNC_UNSIGNED) {
      masm.wasmTruncateFloat32ToUInt32(src, dest, isSaturating, ool->entry());
    } else {
      masm.wasmTruncateFloat32ToInt32(src, dest, isSaturating, ool->entry());
    }
    masm.bind(ool->rejoin());
    return true;
  }

  MOZ_MUST_USE bool truncateF64ToI32(RegF64 src, RegI32 dest,
                                     TruncFlags flags) {
    BytecodeOffset off = bytecodeOffset();
    OutOfLineCode* ool =
        addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI32(
            AnyReg(src), dest, flags, off));
    if (!ool) {
      return false;
    }
    bool isSaturating = flags & TRUNC_SATURATING;
    if (flags & TRUNC_UNSIGNED) {
      masm.wasmTruncateDoubleToUInt32(src, dest, isSaturating, ool->entry());
    } else {
      masm.wasmTruncateDoubleToInt32(src, dest, isSaturating, ool->entry());
    }
    masm.bind(ool->rejoin());
    return true;
  }

  class OutOfLineTruncateCheckF32OrF64ToI64 : public OutOfLineCode {
    AnyReg src;
    RegI64 dest;
    TruncFlags flags;
    BytecodeOffset off;

   public:
    OutOfLineTruncateCheckF32OrF64ToI64(AnyReg src, RegI64 dest,
                                        TruncFlags flags, BytecodeOffset off)
        : src(src), dest(dest), flags(flags), off(off) {}

    virtual void generate(MacroAssembler* masm) override {
      if (src.tag == AnyReg::F32) {
        masm->oolWasmTruncateCheckF32ToI64(src.f32(), dest, flags, off,
                                           rejoin());
      } else if (src.tag == AnyReg::F64) {
        masm->oolWasmTruncateCheckF64ToI64(src.f64(), dest, flags, off,
                                           rejoin());
      } else {
        MOZ_CRASH("unexpected type");
      }
    }
  };

#ifndef RABALDR_FLOAT_TO_I64_CALLOUT
  MOZ_MUST_USE RegF64 needTempForFloatingToI64(TruncFlags flags) {
#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    if (flags & TRUNC_UNSIGNED) {
      return needF64();
    }
#  endif
    return RegF64::Invalid();
  }

  MOZ_MUST_USE bool truncateF32ToI64(RegF32 src, RegI64 dest, TruncFlags flags,
                                     RegF64 temp) {
    OutOfLineCode* ool =
        addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(
            AnyReg(src), dest, flags, bytecodeOffset()));
    if (!ool) {
      return false;
    }
    bool isSaturating = flags & TRUNC_SATURATING;
    if (flags & TRUNC_UNSIGNED) {
      masm.wasmTruncateFloat32ToUInt64(src, dest, isSaturating, ool->entry(),
                                       ool->rejoin(), temp);
    } else {
      masm.wasmTruncateFloat32ToInt64(src, dest, isSaturating, ool->entry(),
                                      ool->rejoin(), temp);
    }
    return true;
  }

  MOZ_MUST_USE bool truncateF64ToI64(RegF64 src, RegI64 dest, TruncFlags flags,
                                     RegF64 temp) {
    OutOfLineCode* ool =
        addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(
            AnyReg(src), dest, flags, bytecodeOffset()));
    if (!ool) {
      return false;
    }
    bool isSaturating = flags & TRUNC_SATURATING;
    if (flags & TRUNC_UNSIGNED) {
      masm.wasmTruncateDoubleToUInt64(src, dest, isSaturating, ool->entry(),
                                      ool->rejoin(), temp);
    } else {
      masm.wasmTruncateDoubleToInt64(src, dest, isSaturating, ool->entry(),
                                     ool->rejoin(), temp);
    }
    return true;
  }
#endif  // RABALDR_FLOAT_TO_I64_CALLOUT

#ifndef RABALDR_I64_TO_FLOAT_CALLOUT
  RegI32 needConvertI64ToFloatTemp(ValType to, bool isUnsigned) {
    bool needs = false;
    if (to == ValType::F64) {
      needs = isUnsigned && masm.convertUInt64ToDoubleNeedsTemp();
    } else {
#  if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
      needs = true;
#  endif
    }
    return needs ? needI32() : RegI32::Invalid();
  }

  void convertI64ToF32(RegI64 src, bool isUnsigned, RegF32 dest, RegI32 temp) {
    if (isUnsigned) {
      masm.convertUInt64ToFloat32(src, dest, temp);
    } else {
      masm.convertInt64ToFloat32(src, dest);
    }
  }

  void convertI64ToF64(RegI64 src, bool isUnsigned, RegF64 dest, RegI32 temp) {
    if (isUnsigned) {
      masm.convertUInt64ToDouble(src, dest, temp);
    } else {
      masm.convertInt64ToDouble(src, dest);
    }
  }
#endif  // RABALDR_I64_TO_FLOAT_CALLOUT

  void cmp64Set(Assembler::Condition cond, RegI64 lhs, RegI64 rhs,
                RegI32 dest) {
#if defined(JS_PUNBOX64)
    masm.cmpPtrSet(cond, lhs.reg, rhs.reg, dest);
#elif defined(JS_CODEGEN_MIPS32)
    masm.cmp64Set(cond, lhs, rhs, dest);
#else
    // TODO / OPTIMIZE (Bug 1316822): This is pretty branchy, we should be
    // able to do better.
    Label done, condTrue;
    masm.branch64(cond, lhs, rhs, &condTrue);
    moveImm32(0, dest);
    masm.jump(&done);
    masm.bind(&condTrue);
    moveImm32(1, dest);
    masm.bind(&done);
#endif
  }

  void eqz64(RegI64 src, RegI32 dest) {
#ifdef JS_PUNBOX64
    masm.cmpPtrSet(Assembler::Equal, src.reg, ImmWord(0), dest);
#else
    masm.or32(src.high, src.low);
    masm.cmp32Set(Assembler::Equal, src.low, Imm32(0), dest);
#endif
  }

  MOZ_MUST_USE bool supportsRoundInstruction(RoundingMode mode) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
    return Assembler::HasRoundInstruction(mode);
#else
    return false;
#endif
  }

  void roundF32(RoundingMode roundingMode, RegF32 f0) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
    masm.vroundss(Assembler::ToX86RoundingMode(roundingMode), f0, f0, f0);
#else
    MOZ_CRASH("NYI");
#endif
  }

  void roundF64(RoundingMode roundingMode, RegF64 f0) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
    masm.vroundsd(Assembler::ToX86RoundingMode(roundingMode), f0, f0, f0);
#else
    MOZ_CRASH("NYI");
#endif
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Global variable access.

  Address addressOfGlobalVar(const GlobalDesc& global, RegI32 tmp) {
    uint32_t globalToTlsOffset =
        offsetof(TlsData, globalArea) + global.offset();
    masm.loadWasmTlsRegFromFrame(tmp);
    if (global.isIndirect()) {
      masm.loadPtr(Address(tmp, globalToTlsOffset), tmp);
      return Address(tmp, 0);
    }
    return Address(tmp, globalToTlsOffset);
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Heap access.

  void bceCheckLocal(MemoryAccessDesc* access, AccessCheck* check,
                     uint32_t local) {
    if (local >= sizeof(BCESet) * 8) {
      return;
    }

    if ((bceSafe_ & (BCESet(1) << local)) &&
        access->offset() < wasm::OffsetGuardLimit) {
      check->omitBoundsCheck = true;
    }

    // The local becomes safe even if the offset is beyond the guard limit.
    bceSafe_ |= (BCESet(1) << local);
  }

  void bceLocalIsUpdated(uint32_t local) {
    if (local >= sizeof(BCESet) * 8) {
      return;
    }

    bceSafe_ &= ~(BCESet(1) << local);
  }

  void prepareMemoryAccess(MemoryAccessDesc* access, AccessCheck* check,
                           RegI32 tls, RegI32 ptr) {
    // Fold offset if necessary for further computations.

    if (access->offset() >= OffsetGuardLimit ||
        (access->isAtomic() && !check->omitAlignmentCheck &&
         !check->onlyPointerAlignment)) {
      Label ok;
      masm.branchAdd32(Assembler::CarryClear, Imm32(access->offset()), ptr,
                       &ok);
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
      masm.branchTest32(Assembler::Zero, ptr, Imm32(access->byteSize() - 1),
                        &ok);
      masm.wasmTrap(Trap::UnalignedAccess, bytecodeOffset());
      masm.bind(&ok);
    }

    // Ensure no tls if we don't need it.

#ifdef WASM_HUGE_MEMORY
    // We have HeapReg and no bounds checking and need load neither
    // memoryBase nor boundsCheckLimit from tls.
    MOZ_ASSERT_IF(check->omitBoundsCheck, tls.isInvalid());
#endif
#ifdef JS_CODEGEN_ARM
    // We have HeapReg on ARM and don't need to load the memoryBase from tls.
    MOZ_ASSERT_IF(check->omitBoundsCheck, tls.isInvalid());
#endif

    // Bounds check if required.

#ifndef WASM_HUGE_MEMORY
    if (!check->omitBoundsCheck) {
      Label ok;
      masm.wasmBoundsCheck(Assembler::Below, ptr,
                           Address(tls, offsetof(TlsData, boundsCheckLimit)),
                           &ok);
      masm.wasmTrap(Trap::OutOfBounds, bytecodeOffset());
      masm.bind(&ok);
    }
#endif
  }

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM) ||      \
    defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_MIPS32) || \
    defined(JS_CODEGEN_MIPS64)
  BaseIndex prepareAtomicMemoryAccess(MemoryAccessDesc* access,
                                      AccessCheck* check, RegI32 tls,
                                      RegI32 ptr) {
    MOZ_ASSERT(needTlsForAccess(*check) == tls.isValid());
    prepareMemoryAccess(access, check, tls, ptr);
    return BaseIndex(HeapReg, ptr, TimesOne, access->offset());
  }
#elif defined(JS_CODEGEN_X86)
  // Some consumers depend on the address not retaining tls, as tls may be the
  // scratch register.

  Address prepareAtomicMemoryAccess(MemoryAccessDesc* access,
                                    AccessCheck* check, RegI32 tls,
                                    RegI32 ptr) {
    MOZ_ASSERT(needTlsForAccess(*check) == tls.isValid());
    prepareMemoryAccess(access, check, tls, ptr);
    masm.addPtr(Address(tls, offsetof(TlsData, memoryBase)), ptr);
    return Address(ptr, access->offset());
  }
#else
  Address prepareAtomicMemoryAccess(MemoryAccessDesc* access,
                                    AccessCheck* check, RegI32 tls,
                                    RegI32 ptr) {
    MOZ_CRASH("BaseCompiler platform hook: prepareAtomicMemoryAccess");
  }
#endif

  void needLoadTemps(const MemoryAccessDesc& access, RegI32* temp1,
                     RegI32* temp2, RegI32* temp3) {
#if defined(JS_CODEGEN_ARM)
    if (IsUnaligned(access)) {
      switch (access.type()) {
        case Scalar::Float64:
          *temp3 = needI32();
          MOZ_FALLTHROUGH;
        case Scalar::Float32:
          *temp2 = needI32();
          MOZ_FALLTHROUGH;
        default:
          *temp1 = needI32();
          break;
      }
    }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    *temp1 = needI32();
#endif
  }

  MOZ_MUST_USE bool needTlsForAccess(const AccessCheck& check) {
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || \
    defined(JS_CODEGEN_MIPS64)
    return !check.omitBoundsCheck;
#elif defined(JS_CODEGEN_X86)
    return true;
#else
    return false;
#endif
  }

  // ptr and dest may be the same iff dest is I32.
  // This may destroy ptr even if ptr and dest are not the same.
  MOZ_MUST_USE bool load(MemoryAccessDesc* access, AccessCheck* check,
                         RegI32 tls, RegI32 ptr, AnyReg dest, RegI32 temp1,
                         RegI32 temp2, RegI32 temp3) {
    prepareMemoryAccess(access, check, tls, ptr);

#if defined(JS_CODEGEN_X64)
    Operand srcAddr(HeapReg, ptr, TimesOne, access->offset());

    if (dest.tag == AnyReg::I64) {
      masm.wasmLoadI64(*access, srcAddr, dest.i64());
    } else {
      masm.wasmLoad(*access, srcAddr, dest.any());
    }
#elif defined(JS_CODEGEN_X86)
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
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || \
    defined(JS_CODEGEN_MIPS64)
    if (IsUnaligned(*access)) {
      switch (dest.tag) {
        case AnyReg::I64:
          masm.wasmUnalignedLoadI64(*access, HeapReg, ptr, ptr, dest.i64(),
                                    temp1);
          break;
        case AnyReg::F32:
          masm.wasmUnalignedLoadFP(*access, HeapReg, ptr, ptr, dest.f32(),
                                   temp1, temp2, RegI32::Invalid());
          break;
        case AnyReg::F64:
          masm.wasmUnalignedLoadFP(*access, HeapReg, ptr, ptr, dest.f64(),
                                   temp1, temp2, temp3);
          break;
        default:
          masm.wasmUnalignedLoad(*access, HeapReg, ptr, ptr, dest.i32(), temp1);
          break;
      }
    } else {
      if (dest.tag == AnyReg::I64) {
        masm.wasmLoadI64(*access, HeapReg, ptr, ptr, dest.i64());
      } else {
        masm.wasmLoad(*access, HeapReg, ptr, ptr, dest.any());
      }
    }
#elif defined(JS_CODEGEN_ARM64)
    if (dest.tag == AnyReg::I64) {
      masm.wasmLoadI64(*access, HeapReg, ptr, ptr, dest.i64());
    } else {
      masm.wasmLoad(*access, HeapReg, ptr, ptr, dest.any());
    }
#else
    MOZ_CRASH("BaseCompiler platform hook: load");
#endif

    return true;
  }

  RegI32 needStoreTemp(const MemoryAccessDesc& access, ValType srcType) {
#if defined(JS_CODEGEN_ARM)
    if (IsUnaligned(access) && srcType != ValType::I32) {
      return needI32();
    }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    return needI32();
#endif
    return RegI32::Invalid();
  }

  // ptr and src must not be the same register.
  // This may destroy ptr and src.
  MOZ_MUST_USE bool store(MemoryAccessDesc* access, AccessCheck* check,
                          RegI32 tls, RegI32 ptr, AnyReg src, RegI32 temp) {
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
    if (IsUnaligned(*access)) {
      switch (src.tag) {
        case AnyReg::I64:
          masm.wasmUnalignedStoreI64(*access, src.i64(), HeapReg, ptr, ptr,
                                     temp);
          break;
        case AnyReg::F32:
          masm.wasmUnalignedStoreFP(*access, src.f32(), HeapReg, ptr, ptr,
                                    temp);
          break;
        case AnyReg::F64:
          masm.wasmUnalignedStoreFP(*access, src.f64(), HeapReg, ptr, ptr,
                                    temp);
          break;
        default:
          MOZ_ASSERT(temp.isInvalid());
          masm.wasmUnalignedStore(*access, src.i32(), HeapReg, ptr, ptr, temp);
          break;
      }
    } else {
      MOZ_ASSERT(temp.isInvalid());
      if (access->type() == Scalar::Int64) {
        masm.wasmStoreI64(*access, src.i64(), HeapReg, ptr, ptr);
      } else if (src.tag == AnyReg::I64) {
        masm.wasmStore(*access, AnyRegister(src.i64().low), HeapReg, ptr, ptr);
      } else {
        masm.wasmStore(*access, src.any(), HeapReg, ptr, ptr);
      }
    }
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    if (IsUnaligned(*access)) {
      switch (src.tag) {
        case AnyReg::I64:
          masm.wasmUnalignedStoreI64(*access, src.i64(), HeapReg, ptr, ptr,
                                     temp);
          break;
        case AnyReg::F32:
          masm.wasmUnalignedStoreFP(*access, src.f32(), HeapReg, ptr, ptr,
                                    temp);
          break;
        case AnyReg::F64:
          masm.wasmUnalignedStoreFP(*access, src.f64(), HeapReg, ptr, ptr,
                                    temp);
          break;
        default:
          masm.wasmUnalignedStore(*access, src.i32(), HeapReg, ptr, ptr, temp);
          break;
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
      masm.wasmStoreI64(*access, src.i64(), HeapReg, ptr, ptr);
    } else {
      masm.wasmStore(*access, src.any(), HeapReg, ptr, ptr);
    }
#else
    MOZ_CRASH("BaseCompiler platform hook: store");
#endif

    return true;
  }

  template <size_t Count>
  struct Atomic32Temps : mozilla::Array<RegI32, Count> {
    // Allocate all temp registers if 'allocate' is not specified.
    void allocate(BaseCompiler* bc, size_t allocate = Count) {
      MOZ_ASSERT(Count != 0);
      for (size_t i = 0; i < allocate; ++i) {
        this->operator[](i) = bc->needI32();
      }
    }
    void maybeFree(BaseCompiler* bc) {
      for (size_t i = 0; i < Count; ++i) {
        bc->maybeFreeI32(this->operator[](i));
      }
    }
  };

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  using AtomicRMW32Temps = Atomic32Temps<3>;
#else
  using AtomicRMW32Temps = Atomic32Temps<1>;
#endif

  template <typename T>
  void atomicRMW32(const MemoryAccessDesc& access, T srcAddr, AtomicOp op,
                   RegI32 rv, RegI32 rd, const AtomicRMW32Temps& temps) {
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
  void atomicRMW64(const MemoryAccessDesc& access, const T& srcAddr,
                   AtomicOp op, V value, Register64 temp, Register64 rd) {
    masm.wasmAtomicFetchOp64(access, op, value, srcAddr, temp, rd);
  }

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  using AtomicCmpXchg32Temps = Atomic32Temps<3>;
#else
  using AtomicCmpXchg32Temps = Atomic32Temps<0>;
#endif

  template <typename T>
  void atomicCmpXchg32(const MemoryAccessDesc& access, T srcAddr,
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

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  using AtomicXchg32Temps = Atomic32Temps<3>;
#else
  using AtomicXchg32Temps = Atomic32Temps<0>;
#endif

  template <typename T>
  void atomicXchg32(const MemoryAccessDesc& access, T srcAddr, RegI32 rv,
                    RegI32 rd, const AtomicXchg32Temps& temps) {
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
        masm.wasmAtomicExchange(access, srcAddr, rv, temps[0], temps[1],
                                temps[2], rd);
#else
        masm.wasmAtomicExchange(access, srcAddr, rv, rd);
#endif
        break;
      default:
        MOZ_CRASH("Bad type for atomic operation");
    }
  }

  ////////////////////////////////////////////////////////////
  //
  // Generally speaking, ABOVE this point there should be no
  // value stack manipulation (calls to popI32 etc).
  //
  ////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////
  //
  // Platform-specific popping and register targeting.
  //
  // These fall into two groups, popping methods for simple needs, and RAII
  // wrappers for more complex behavior.

  // The simple popping methods pop values into targeted registers; the caller
  // can free registers using standard functions.  These are always called
  // popXForY where X says something about types and Y something about the
  // operation being targeted.

  void pop2xI32ForMulDivI32(RegI32* r0, RegI32* r1, RegI32* reserved) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // r0 must be eax, and edx will be clobbered.
    need2xI32(specific_.eax, specific_.edx);
    *r1 = popI32();
    *r0 = popI32ToSpecific(specific_.eax);
    *reserved = specific_.edx;
#else
    pop2xI32(r0, r1);
#endif
  }

  void pop2xI64ForMulI64(RegI64* r0, RegI64* r1, RegI32* temp,
                         RegI64* reserved) {
#if defined(JS_CODEGEN_X64)
    // r0 must be rax, and rdx will be clobbered.
    need2xI64(specific_.rax, specific_.rdx);
    *r1 = popI64();
    *r0 = popI64ToSpecific(specific_.rax);
    *reserved = specific_.rdx;
#elif defined(JS_CODEGEN_X86)
    // As for x64, though edx is part of r0.
    need2xI32(specific_.eax, specific_.edx);
    *r1 = popI64();
    *r0 = popI64ToSpecific(specific_.edx_eax);
    *temp = needI32();
#elif defined(JS_CODEGEN_MIPS64)
    pop2xI64(r0, r1);
#elif defined(JS_CODEGEN_MIPS32)
    pop2xI64(r0, r1);
    *temp = needI32();
#elif defined(JS_CODEGEN_ARM)
    pop2xI64(r0, r1);
    *temp = needI32();
#elif defined(JS_CODEGEN_ARM64)
    pop2xI64(r0, r1);
#else
    MOZ_CRASH("BaseCompiler porting interface: pop2xI64ForMulI64");
#endif
  }

  void pop2xI64ForDivI64(RegI64* r0, RegI64* r1, RegI64* reserved) {
#if defined(JS_CODEGEN_X64)
    // r0 must be rax, and rdx will be clobbered.
    need2xI64(specific_.rax, specific_.rdx);
    *r1 = popI64();
    *r0 = popI64ToSpecific(specific_.rax);
    *reserved = specific_.rdx;
#else
    pop2xI64(r0, r1);
#endif
  }

  void pop2xI32ForShiftOrRotate(RegI32* r0, RegI32* r1) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // r1 must be ecx for a variable shift.
    *r1 = popI32(specific_.ecx);
    *r0 = popI32();
#else
    pop2xI32(r0, r1);
#endif
  }

  void pop2xI64ForShiftOrRotate(RegI64* r0, RegI64* r1) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // r1 must be ecx for a variable shift.
    needI32(specific_.ecx);
    *r1 = popI64ToSpecific(widenI32(specific_.ecx));
    *r0 = popI64();
#else
    pop2xI64(r0, r1);
#endif
  }

  void popI32ForSignExtendI64(RegI64* r0) {
#if defined(JS_CODEGEN_X86)
    // r0 must be edx:eax for cdq
    need2xI32(specific_.edx, specific_.eax);
    *r0 = specific_.edx_eax;
    popI32ToSpecific(specific_.eax);
#else
    *r0 = widenI32(popI32());
#endif
  }

  void popI64ForSignExtendI64(RegI64* r0) {
#if defined(JS_CODEGEN_X86)
    // r0 must be edx:eax for cdq
    need2xI32(specific_.edx, specific_.eax);
    // Low on top, high underneath
    *r0 = popI64ToSpecific(specific_.edx_eax);
#else
    *r0 = popI64();
#endif
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

    void maybeFree(RegI32 r) { bc->maybeFreeI32(r); }
    void maybeFree(RegI64 r) { bc->maybeFreeI64(r); }

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

  friend class PopAtomicCmpXchg32Regs;
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

  friend class PopAtomicCmpXchg64Regs;
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
    void atomicCmpXchg64(const MemoryAccessDesc& access, T srcAddr,
                         RegI32 ebx) {
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
      bc->masm.wasmAtomicLoad64(access, srcAddr, bc->specific_.ecx_ebx,
                                getRd());
    }
#  else  // ARM, MIPS32
    template <typename T>
    void atomicLoad64(const MemoryAccessDesc& access, T srcAddr) {
      bc->masm.wasmAtomicLoad64(access, srcAddr, RegI64::Invalid(), getRd());
    }
#  endif
  };
#endif  // JS_64BIT

  friend class PopAtomicRMW32Regs;
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

  friend class PopAtomicRMW64Regs;
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
      bc->maybeFreeI64(temp);
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
      bc->atomicRMW64(access, srcAddr, op, value, bc->specific_.ecx_ebx,
                      getRd());
    }
#else
    template <typename T>
    void atomicRMW64(const MemoryAccessDesc& access, T srcAddr, AtomicOp op) {
      bc->atomicRMW64(access, srcAddr, op, rv, temp, getRd());
    }
#endif
  };

  friend class PopAtomicXchg32Regs;
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

  friend class PopAtomicXchg64Regs;
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

  ////////////////////////////////////////////////////////////
  //
  // Generally speaking, BELOW this point there should be no
  // platform dependencies.  We make very occasional exceptions
  // when it doesn't become messy and further abstraction is
  // not desirable.
  //
  ////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////
  //
  // Sundry wrappers.

  void pop2xI32(RegI32* r0, RegI32* r1) {
    *r1 = popI32();
    *r0 = popI32();
  }

  RegI32 popI32ToSpecific(RegI32 specific) {
    freeI32(specific);
    return popI32(specific);
  }

  void pop2xI64(RegI64* r0, RegI64* r1) {
    *r1 = popI64();
    *r0 = popI64();
  }

  RegI64 popI64ToSpecific(RegI64 specific) {
    freeI64(specific);
    return popI64(specific);
  }

#ifdef JS_CODEGEN_ARM
  RegI64 popI64Pair() {
    RegI64 r = needI64Pair();
    popI64ToSpecific(r);
    return r;
  }
#endif

  void pop2xF32(RegF32* r0, RegF32* r1) {
    *r1 = popF32();
    *r0 = popF32();
  }

  void pop2xF64(RegF64* r0, RegF64* r1) {
    *r1 = popF64();
    *r0 = popF64();
  }

  void pop2xRef(RegPtr* r0, RegPtr* r1) {
    *r1 = popRef();
    *r0 = popRef();
  }

  RegI32 popI64ToI32() {
    RegI64 r = popI64();
    return narrowI64(r);
  }

  RegI32 popI64ToSpecificI32(RegI32 specific) {
    RegI64 rd = widenI32(specific);
    popI64ToSpecific(rd);
    return narrowI64(rd);
  }

  void pushU32AsI64(RegI32 rs) {
    RegI64 rd = widenI32(rs);
    masm.move32To64ZeroExtend(rs, rd);
    pushI64(rd);
  }

  RegI32 popMemoryAccess(MemoryAccessDesc* access, AccessCheck* check);

  ////////////////////////////////////////////////////////////
  //
  // Sundry helpers.

  uint32_t readCallSiteLineOrBytecode() {
    if (!func_.callSiteLineNums.empty()) {
      return func_.callSiteLineNums[lastReadCallSite_++];
    }
    return iter_.lastOpcodeOffset();
  }

  bool done() const { return iter_.done(); }

  BytecodeOffset bytecodeOffset() const { return iter_.bytecodeOffset(); }

  void trap(Trap t) const { masm.wasmTrap(t, bytecodeOffset()); }

  ////////////////////////////////////////////////////////////
  //
  // Object support.

  // This emits a GC pre-write barrier.  The pre-barrier is needed when we
  // replace a member field with a new value, and the previous field value
  // might have no other referents, and incremental GC is ongoing. The field
  // might belong to an object or be a stack slot or a register or a heap
  // allocated value.
  //
  // let obj = { field: previousValue };
  // obj.field = newValue; // previousValue must be marked with a pre-barrier.
  //
  // The `valueAddr` is the address of the location that we are about to
  // update.  This function preserves that register.

  void emitPreBarrier(RegPtr valueAddr) {
    Label skipBarrier;
    ScratchPtr scratch(*this);

    masm.loadWasmTlsRegFromFrame(scratch);
    EmitWasmPreBarrierGuard(masm, scratch, scratch, valueAddr, &skipBarrier);

    masm.loadWasmTlsRegFromFrame(scratch);
#ifdef JS_CODEGEN_ARM64
    // The prebarrier stub assumes the PseudoStackPointer is set up.  It is OK
    // to just move the sp to x28 here because x28 is not being used by the
    // baseline compiler and need not be saved or restored.
    MOZ_ASSERT(!GeneralRegisterSet::All().hasRegisterIndex(x28.asUnsized()));
    masm.Mov(x28, sp);
#endif
    EmitWasmPreBarrierCall(masm, scratch, scratch, valueAddr);

    masm.bind(&skipBarrier);
  }

  // This frees the register `valueAddr`.

  MOZ_MUST_USE bool emitPostBarrierCall(RegPtr valueAddr) {
    uint32_t bytecodeOffset = iter_.lastOpcodeOffset();

    // The `valueAddr` is a raw pointer to the cell within some GC object or
    // TLS area, and we guarantee that the GC will not run while the
    // postbarrier call is active, so push a uintptr_t value.
#ifdef JS_64BIT
    pushI64(RegI64(Register64(valueAddr)));
#else
    pushI32(RegI32(valueAddr));
#endif
    if (!emitInstanceCall(bytecodeOffset, SASigPostBarrier,
                          /*pushReturnedValue=*/false)) {
      return false;
    }
    return true;
  }

  MOZ_MUST_USE bool emitBarrieredStore(const Maybe<RegPtr>& object,
                                       RegPtr valueAddr, RegPtr value,
                                       ValType valueType) {
    // TODO/AnyRef-boxing: With boxed immediates and strings, the write
    // barrier is going to have to be more complicated.
    ASSERT_ANYREF_IS_JSOBJECT;

    emitPreBarrier(valueAddr);  // Preserves valueAddr
    masm.storePtr(value, Address(valueAddr, 0));

    Label skipBarrier;
    sync();

    RegPtr otherScratch = needRef();
    EmitWasmPostBarrierGuard(masm, object, otherScratch, value, &skipBarrier);
    freeRef(otherScratch);

    if (!emitPostBarrierCall(valueAddr)) {
      return false;
    }
    masm.bind(&skipBarrier);
    return true;
  }

  ////////////////////////////////////////////////////////////
  //
  // Machinery for optimized conditional branches.
  //
  // To disable this optimization it is enough always to return false from
  // sniffConditionalControl{Cmp,Eqz}.

  struct BranchState {
    union {
      struct {
        RegI32 lhs;
        RegI32 rhs;
        int32_t imm;
        bool rhsImm;
      } i32;
      struct {
        RegI64 lhs;
        RegI64 rhs;
        int64_t imm;
        bool rhsImm;
      } i64;
      struct {
        RegF32 lhs;
        RegF32 rhs;
      } f32;
      struct {
        RegF64 lhs;
        RegF64 rhs;
      } f64;
    };

    Label* const label;             // The target of the branch, never NULL
    const StackHeight stackHeight;  // The value to pop to along the taken edge,
                                    // unless !hasPop()
    const bool invertBranch;        // If true, invert the sense of the branch
    const ExprType
        resultType;  // The result propagated along the edges, or Void

    explicit BranchState(Label* label)
        : label(label),
          stackHeight(StackHeight::Invalid()),
          invertBranch(false),
          resultType(ExprType::Void) {}

    BranchState(Label* label, bool invertBranch)
        : label(label),
          stackHeight(StackHeight::Invalid()),
          invertBranch(invertBranch),
          resultType(ExprType::Void) {}

    BranchState(Label* label, StackHeight stackHeight, bool invertBranch,
                ExprType resultType)
        : label(label),
          stackHeight(stackHeight),
          invertBranch(invertBranch),
          resultType(resultType) {}

    bool hasPop() const { return stackHeight.isValid(); }
  };

  void setLatentCompare(Assembler::Condition compareOp, ValType operandType) {
    latentOp_ = LatentOp::Compare;
    latentType_ = operandType;
    latentIntCmp_ = compareOp;
  }

  void setLatentCompare(Assembler::DoubleCondition compareOp,
                        ValType operandType) {
    latentOp_ = LatentOp::Compare;
    latentType_ = operandType;
    latentDoubleCmp_ = compareOp;
  }

  void setLatentEqz(ValType operandType) {
    latentOp_ = LatentOp::Eqz;
    latentType_ = operandType;
  }

  void resetLatentOp() { latentOp_ = LatentOp::None; }

  void branchTo(Assembler::DoubleCondition c, RegF64 lhs, RegF64 rhs,
                Label* l) {
    masm.branchDouble(c, lhs, rhs, l);
  }

  void branchTo(Assembler::DoubleCondition c, RegF32 lhs, RegF32 rhs,
                Label* l) {
    masm.branchFloat(c, lhs, rhs, l);
  }

  void branchTo(Assembler::Condition c, RegI32 lhs, RegI32 rhs, Label* l) {
    masm.branch32(c, lhs, rhs, l);
  }

  void branchTo(Assembler::Condition c, RegI32 lhs, Imm32 rhs, Label* l) {
    masm.branch32(c, lhs, rhs, l);
  }

  void branchTo(Assembler::Condition c, RegI64 lhs, RegI64 rhs, Label* l) {
    masm.branch64(c, lhs, rhs, l);
  }

  void branchTo(Assembler::Condition c, RegI64 lhs, Imm64 rhs, Label* l) {
    masm.branch64(c, lhs, rhs, l);
  }

  // Emit a conditional branch that optionally and optimally cleans up the CPU
  // stack before we branch.
  //
  // Cond is either Assembler::Condition or Assembler::DoubleCondition.
  //
  // Lhs is RegI32, RegI64, or RegF32, or RegF64.
  //
  // Rhs is either the same as Lhs, or an immediate expression compatible with
  // Lhs "when applicable".

  template <typename Cond, typename Lhs, typename Rhs>
  void jumpConditionalWithJoinReg(BranchState* b, Cond cond, Lhs lhs, Rhs rhs) {
    Maybe<AnyReg> r = popJoinRegUnlessVoid(b->resultType);

    if (b->hasPop() && fr.willPopStackBeforeBranch(b->stackHeight)) {
      Label notTaken;
      branchTo(b->invertBranch ? cond : Assembler::InvertCondition(cond), lhs,
               rhs, &notTaken);
      fr.popStackBeforeBranch(b->stackHeight);
      masm.jump(b->label);
      masm.bind(&notTaken);
    } else {
      branchTo(b->invertBranch ? Assembler::InvertCondition(cond) : cond, lhs,
               rhs, b->label);
    }

    pushJoinRegUnlessVoid(r);
  }

  // sniffConditionalControl{Cmp,Eqz} may modify the latentWhatever_ state in
  // the BaseCompiler so that a subsequent conditional branch can be compiled
  // optimally.  emitBranchSetup() and emitBranchPerform() will consume that
  // state.  If the latter methods are not called because deadCode_ is true
  // then the compiler MUST instead call resetLatentOp() to reset the state.

  template <typename Cond>
  bool sniffConditionalControlCmp(Cond compareOp, ValType operandType);
  bool sniffConditionalControlEqz(ValType operandType);
  void emitBranchSetup(BranchState* b);
  void emitBranchPerform(BranchState* b);

  //////////////////////////////////////////////////////////////////////

  MOZ_MUST_USE bool emitBody();
  MOZ_MUST_USE bool emitBlock();
  MOZ_MUST_USE bool emitLoop();
  MOZ_MUST_USE bool emitIf();
  MOZ_MUST_USE bool emitElse();
  MOZ_MUST_USE bool emitEnd();
  MOZ_MUST_USE bool emitBr();
  MOZ_MUST_USE bool emitBrIf();
  MOZ_MUST_USE bool emitBrTable();
  MOZ_MUST_USE bool emitDrop();
  MOZ_MUST_USE bool emitReturn();
  MOZ_MUST_USE bool emitCallArgs(const ValTypeVector& args,
                                 FunctionCall* baselineCall);
  MOZ_MUST_USE bool emitCall();
  MOZ_MUST_USE bool emitCallIndirect();
  MOZ_MUST_USE bool emitUnaryMathBuiltinCall(SymbolicAddress callee,
                                             ValType operandType);
  MOZ_MUST_USE bool emitGetLocal();
  MOZ_MUST_USE bool emitSetLocal();
  MOZ_MUST_USE bool emitTeeLocal();
  MOZ_MUST_USE bool emitGetGlobal();
  MOZ_MUST_USE bool emitSetGlobal();
  MOZ_MUST_USE RegI32 maybeLoadTlsForAccess(const AccessCheck& check);
  MOZ_MUST_USE RegI32 maybeLoadTlsForAccess(const AccessCheck& check,
                                            RegI32 specific);
  MOZ_MUST_USE bool emitLoad(ValType type, Scalar::Type viewType);
  MOZ_MUST_USE bool loadCommon(MemoryAccessDesc* access, ValType type);
  MOZ_MUST_USE bool emitStore(ValType resultType, Scalar::Type viewType);
  MOZ_MUST_USE bool storeCommon(MemoryAccessDesc* access, ValType resultType);
  MOZ_MUST_USE bool emitSelect();

  template <bool isSetLocal>
  MOZ_MUST_USE bool emitSetOrTeeLocal(uint32_t slot);

  void endBlock(ExprType type);
  void endLoop(ExprType type);
  void endIfThen();
  void endIfThenElse(ExprType type);

  void doReturn(ExprType returnType, bool popStack);
  void pushReturnValueOfCall(const FunctionCall& call, ExprType type);
  void pushReturnValueOfCall(const FunctionCall& call, MIRType type);

  void emitCompareI32(Assembler::Condition compareOp, ValType compareType);
  void emitCompareI64(Assembler::Condition compareOp, ValType compareType);
  void emitCompareF32(Assembler::DoubleCondition compareOp,
                      ValType compareType);
  void emitCompareF64(Assembler::DoubleCondition compareOp,
                      ValType compareType);
  void emitCompareRef(Assembler::Condition compareOp, ValType compareType);

  void emitAddI32();
  void emitAddI64();
  void emitAddF64();
  void emitAddF32();
  void emitSubtractI32();
  void emitSubtractI64();
  void emitSubtractF32();
  void emitSubtractF64();
  void emitMultiplyI32();
  void emitMultiplyI64();
  void emitMultiplyF32();
  void emitMultiplyF64();
  void emitQuotientI32();
  void emitQuotientU32();
  void emitRemainderI32();
  void emitRemainderU32();
#ifdef RABALDR_INT_DIV_I64_CALLOUT
  MOZ_MUST_USE bool emitDivOrModI64BuiltinCall(SymbolicAddress callee,
                                               ValType operandType);
#else
  void emitQuotientI64();
  void emitQuotientU64();
  void emitRemainderI64();
  void emitRemainderU64();
#endif
  void emitDivideF32();
  void emitDivideF64();
  void emitMinF32();
  void emitMaxF32();
  void emitMinF64();
  void emitMaxF64();
  void emitCopysignF32();
  void emitCopysignF64();
  void emitOrI32();
  void emitOrI64();
  void emitAndI32();
  void emitAndI64();
  void emitXorI32();
  void emitXorI64();
  void emitShlI32();
  void emitShlI64();
  void emitShrI32();
  void emitShrI64();
  void emitShrU32();
  void emitShrU64();
  void emitRotrI32();
  void emitRotrI64();
  void emitRotlI32();
  void emitRotlI64();
  void emitEqzI32();
  void emitEqzI64();
  void emitClzI32();
  void emitClzI64();
  void emitCtzI32();
  void emitCtzI64();
  void emitPopcntI32();
  void emitPopcntI64();
  void emitAbsF32();
  void emitAbsF64();
  void emitNegateF32();
  void emitNegateF64();
  void emitSqrtF32();
  void emitSqrtF64();
  template <TruncFlags flags>
  MOZ_MUST_USE bool emitTruncateF32ToI32();
  template <TruncFlags flags>
  MOZ_MUST_USE bool emitTruncateF64ToI32();
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
  MOZ_MUST_USE bool emitConvertFloatingToInt64Callout(SymbolicAddress callee,
                                                      ValType operandType,
                                                      ValType resultType);
#else
  template <TruncFlags flags>
  MOZ_MUST_USE bool emitTruncateF32ToI64();
  template <TruncFlags flags>
  MOZ_MUST_USE bool emitTruncateF64ToI64();
#endif
  void emitWrapI64ToI32();
  void emitExtendI32_8();
  void emitExtendI32_16();
  void emitExtendI64_8();
  void emitExtendI64_16();
  void emitExtendI64_32();
  void emitExtendI32ToI64();
  void emitExtendU32ToI64();
  void emitReinterpretF32AsI32();
  void emitReinterpretF64AsI64();
  void emitConvertF64ToF32();
  void emitConvertI32ToF32();
  void emitConvertU32ToF32();
  void emitConvertF32ToF64();
  void emitConvertI32ToF64();
  void emitConvertU32ToF64();
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
  MOZ_MUST_USE bool emitConvertInt64ToFloatingCallout(SymbolicAddress callee,
                                                      ValType operandType,
                                                      ValType resultType);
#else
  void emitConvertI64ToF32();
  void emitConvertU64ToF32();
  void emitConvertI64ToF64();
  void emitConvertU64ToF64();
#endif
  void emitReinterpretI32AsF32();
  void emitReinterpretI64AsF64();
  void emitRound(RoundingMode roundingMode, ValType operandType);
  MOZ_MUST_USE bool emitInstanceCall(uint32_t lineOrBytecode,
                                     const SymbolicAddressSignature& builtin,
                                     bool pushReturnedValue = true);
  MOZ_MUST_USE bool emitMemoryGrow();
  MOZ_MUST_USE bool emitMemorySize();

  MOZ_MUST_USE bool emitRefNull();
  void emitRefIsNull();

  MOZ_MUST_USE bool emitAtomicCmpXchg(ValType type, Scalar::Type viewType);
  MOZ_MUST_USE bool emitAtomicLoad(ValType type, Scalar::Type viewType);
  MOZ_MUST_USE bool emitAtomicRMW(ValType type, Scalar::Type viewType,
                                  AtomicOp op);
  MOZ_MUST_USE bool emitAtomicStore(ValType type, Scalar::Type viewType);
  MOZ_MUST_USE bool emitWait(ValType type, uint32_t byteSize);
  MOZ_MUST_USE bool emitWake();
  MOZ_MUST_USE bool emitAtomicXchg(ValType type, Scalar::Type viewType);
  void emitAtomicXchg64(MemoryAccessDesc* access, ValType type,
                        WantResult wantResult);
#ifdef ENABLE_WASM_BULKMEM_OPS
  MOZ_MUST_USE bool emitMemOrTableCopy(bool isMem);
  MOZ_MUST_USE bool emitDataOrElemDrop(bool isData);
  MOZ_MUST_USE bool emitMemFill();
  MOZ_MUST_USE bool emitMemOrTableInit(bool isMem);
#endif
  MOZ_MUST_USE bool emitTableFill();
  MOZ_MUST_USE bool emitTableGet();
  MOZ_MUST_USE bool emitTableGrow();
  MOZ_MUST_USE bool emitTableSet();
  MOZ_MUST_USE bool emitTableSize();
  MOZ_MUST_USE bool emitStructNew();
  MOZ_MUST_USE bool emitStructGet();
  MOZ_MUST_USE bool emitStructSet();
  MOZ_MUST_USE bool emitStructNarrow();
};

void BaseCompiler::emitAddI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.add32(Imm32(c), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32(&r, &rs);
    masm.add32(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitAddI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.add64(Imm64(c), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64(&r, &rs);
    masm.add64(rs, r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitAddF64() {
  RegF64 r, rs;
  pop2xF64(&r, &rs);
  masm.addDouble(rs, r);
  freeF64(rs);
  pushF64(r);
}

void BaseCompiler::emitAddF32() {
  RegF32 r, rs;
  pop2xF32(&r, &rs);
  masm.addFloat32(rs, r);
  freeF32(rs);
  pushF32(r);
}

void BaseCompiler::emitSubtractI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.sub32(Imm32(c), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32(&r, &rs);
    masm.sub32(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitSubtractI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.sub64(Imm64(c), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64(&r, &rs);
    masm.sub64(rs, r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitSubtractF32() {
  RegF32 r, rs;
  pop2xF32(&r, &rs);
  masm.subFloat32(rs, r);
  freeF32(rs);
  pushF32(r);
}

void BaseCompiler::emitSubtractF64() {
  RegF64 r, rs;
  pop2xF64(&r, &rs);
  masm.subDouble(rs, r);
  freeF64(rs);
  pushF64(r);
}

void BaseCompiler::emitMultiplyI32() {
  RegI32 r, rs, reserved;
  pop2xI32ForMulDivI32(&r, &rs, &reserved);
  masm.mul32(rs, r);
  maybeFreeI32(reserved);
  freeI32(rs);
  pushI32(r);
}

void BaseCompiler::emitMultiplyI64() {
  RegI64 r, rs, reserved;
  RegI32 temp;
  pop2xI64ForMulI64(&r, &rs, &temp, &reserved);
  masm.mul64(rs, r, temp);
  maybeFreeI64(reserved);
  maybeFreeI32(temp);
  freeI64(rs);
  pushI64(r);
}

void BaseCompiler::emitMultiplyF32() {
  RegF32 r, rs;
  pop2xF32(&r, &rs);
  masm.mulFloat32(rs, r);
  freeF32(rs);
  pushF32(r);
}

void BaseCompiler::emitMultiplyF64() {
  RegF64 r, rs;
  pop2xF64(&r, &rs);
  masm.mulDouble(rs, r);
  freeF64(rs);
  pushF64(r);
}

void BaseCompiler::emitQuotientI32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI32(&c, &power, 0)) {
    if (power != 0) {
      RegI32 r = popI32();
      Label positive;
      masm.branchTest32(Assembler::NotSigned, r, r, &positive);
      masm.add32(Imm32(c - 1), r);
      masm.bind(&positive);

      masm.rshift32Arithmetic(Imm32(power & 31), r);
      pushI32(r);
    }
  } else {
    bool isConst = peekConstI32(&c);
    RegI32 r, rs, reserved;
    pop2xI32ForMulDivI32(&r, &rs, &reserved);

    Label done;
    if (!isConst || c == 0) {
      checkDivideByZeroI32(rs, r, &done);
    }
    if (!isConst || c == -1) {
      checkDivideSignedOverflowI32(rs, r, &done, ZeroOnOverflow(false));
    }
    masm.quotient32(rs, r, IsUnsigned(false));
    masm.bind(&done);

    maybeFreeI32(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitQuotientU32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI32(&c, &power, 0)) {
    if (power != 0) {
      RegI32 r = popI32();
      masm.rshift32(Imm32(power & 31), r);
      pushI32(r);
    }
  } else {
    bool isConst = peekConstI32(&c);
    RegI32 r, rs, reserved;
    pop2xI32ForMulDivI32(&r, &rs, &reserved);

    Label done;
    if (!isConst || c == 0) {
      checkDivideByZeroI32(rs, r, &done);
    }
    masm.quotient32(rs, r, IsUnsigned(true));
    masm.bind(&done);

    maybeFreeI32(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitRemainderI32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI32(&c, &power, 1)) {
    RegI32 r = popI32();
    RegI32 temp = needI32();
    moveI32(r, temp);

    Label positive;
    masm.branchTest32(Assembler::NotSigned, temp, temp, &positive);
    masm.add32(Imm32(c - 1), temp);
    masm.bind(&positive);

    masm.rshift32Arithmetic(Imm32(power & 31), temp);
    masm.lshift32(Imm32(power & 31), temp);
    masm.sub32(temp, r);
    freeI32(temp);

    pushI32(r);
  } else {
    bool isConst = peekConstI32(&c);
    RegI32 r, rs, reserved;
    pop2xI32ForMulDivI32(&r, &rs, &reserved);

    Label done;
    if (!isConst || c == 0) {
      checkDivideByZeroI32(rs, r, &done);
    }
    if (!isConst || c == -1) {
      checkDivideSignedOverflowI32(rs, r, &done, ZeroOnOverflow(true));
    }
    masm.remainder32(rs, r, IsUnsigned(false));
    masm.bind(&done);

    maybeFreeI32(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitRemainderU32() {
  int32_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI32(&c, &power, 1)) {
    RegI32 r = popI32();
    masm.and32(Imm32(c - 1), r);
    pushI32(r);
  } else {
    bool isConst = peekConstI32(&c);
    RegI32 r, rs, reserved;
    pop2xI32ForMulDivI32(&r, &rs, &reserved);

    Label done;
    if (!isConst || c == 0) {
      checkDivideByZeroI32(rs, r, &done);
    }
    masm.remainder32(rs, r, IsUnsigned(true));
    masm.bind(&done);

    maybeFreeI32(reserved);
    freeI32(rs);
    pushI32(r);
  }
}

#ifndef RABALDR_INT_DIV_I64_CALLOUT
void BaseCompiler::emitQuotientI64() {
#  ifdef JS_64BIT
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI64(&c, &power, 0)) {
    if (power != 0) {
      RegI64 r = popI64();
      Label positive;
      masm.branchTest64(Assembler::NotSigned, r, r, RegI32::Invalid(),
                        &positive);
      masm.add64(Imm64(c - 1), r);
      masm.bind(&positive);

      masm.rshift64Arithmetic(Imm32(power & 63), r);
      pushI64(r);
    }
  } else {
    bool isConst = peekConstI64(&c);
    RegI64 r, rs, reserved;
    pop2xI64ForDivI64(&r, &rs, &reserved);
    quotientI64(rs, r, reserved, IsUnsigned(false), isConst, c);
    maybeFreeI64(reserved);
    freeI64(rs);
    pushI64(r);
  }
#  else
  MOZ_CRASH("BaseCompiler platform hook: emitQuotientI64");
#  endif
}

void BaseCompiler::emitQuotientU64() {
#  ifdef JS_64BIT
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI64(&c, &power, 0)) {
    if (power != 0) {
      RegI64 r = popI64();
      masm.rshift64(Imm32(power & 63), r);
      pushI64(r);
    }
  } else {
    bool isConst = peekConstI64(&c);
    RegI64 r, rs, reserved;
    pop2xI64ForDivI64(&r, &rs, &reserved);
    quotientI64(rs, r, reserved, IsUnsigned(true), isConst, c);
    maybeFreeI64(reserved);
    freeI64(rs);
    pushI64(r);
  }
#  else
  MOZ_CRASH("BaseCompiler platform hook: emitQuotientU64");
#  endif
}

void BaseCompiler::emitRemainderI64() {
#  ifdef JS_64BIT
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI64(&c, &power, 1)) {
    RegI64 r = popI64();
    RegI64 temp = needI64();
    moveI64(r, temp);

    Label positive;
    masm.branchTest64(Assembler::NotSigned, temp, temp, RegI32::Invalid(),
                      &positive);
    masm.add64(Imm64(c - 1), temp);
    masm.bind(&positive);

    masm.rshift64Arithmetic(Imm32(power & 63), temp);
    masm.lshift64(Imm32(power & 63), temp);
    masm.sub64(temp, r);
    freeI64(temp);

    pushI64(r);
  } else {
    bool isConst = peekConstI64(&c);
    RegI64 r, rs, reserved;
    pop2xI64ForDivI64(&r, &rs, &reserved);
    remainderI64(rs, r, reserved, IsUnsigned(false), isConst, c);
    maybeFreeI64(reserved);
    freeI64(rs);
    pushI64(r);
  }
#  else
  MOZ_CRASH("BaseCompiler platform hook: emitRemainderI64");
#  endif
}

void BaseCompiler::emitRemainderU64() {
#  ifdef JS_64BIT
  int64_t c;
  uint_fast8_t power;
  if (popConstPositivePowerOfTwoI64(&c, &power, 1)) {
    RegI64 r = popI64();
    masm.and64(Imm64(c - 1), r);
    pushI64(r);
  } else {
    bool isConst = peekConstI64(&c);
    RegI64 r, rs, reserved;
    pop2xI64ForDivI64(&r, &rs, &reserved);
    remainderI64(rs, r, reserved, IsUnsigned(true), isConst, c);
    maybeFreeI64(reserved);
    freeI64(rs);
    pushI64(r);
  }
#  else
  MOZ_CRASH("BaseCompiler platform hook: emitRemainderU64");
#  endif
}
#endif  // RABALDR_INT_DIV_I64_CALLOUT

void BaseCompiler::emitDivideF32() {
  RegF32 r, rs;
  pop2xF32(&r, &rs);
  masm.divFloat32(rs, r);
  freeF32(rs);
  pushF32(r);
}

void BaseCompiler::emitDivideF64() {
  RegF64 r, rs;
  pop2xF64(&r, &rs);
  masm.divDouble(rs, r);
  freeF64(rs);
  pushF64(r);
}

void BaseCompiler::emitMinF32() {
  RegF32 r, rs;
  pop2xF32(&r, &rs);
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): Don't do this if one of the operands
  // is known to be a constant.
  ScratchF32 zero(*this);
  moveImmF32(0.f, zero);
  masm.subFloat32(zero, r);
  masm.subFloat32(zero, rs);
  masm.minFloat32(rs, r, HandleNaNSpecially(true));
  freeF32(rs);
  pushF32(r);
}

void BaseCompiler::emitMaxF32() {
  RegF32 r, rs;
  pop2xF32(&r, &rs);
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): see comment in emitMinF32.
  ScratchF32 zero(*this);
  moveImmF32(0.f, zero);
  masm.subFloat32(zero, r);
  masm.subFloat32(zero, rs);
  masm.maxFloat32(rs, r, HandleNaNSpecially(true));
  freeF32(rs);
  pushF32(r);
}

void BaseCompiler::emitMinF64() {
  RegF64 r, rs;
  pop2xF64(&r, &rs);
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): see comment in emitMinF32.
  ScratchF64 zero(*this);
  moveImmF64(0, zero);
  masm.subDouble(zero, r);
  masm.subDouble(zero, rs);
  masm.minDouble(rs, r, HandleNaNSpecially(true));
  freeF64(rs);
  pushF64(r);
}

void BaseCompiler::emitMaxF64() {
  RegF64 r, rs;
  pop2xF64(&r, &rs);
  // Convert signaling NaN to quiet NaNs.
  //
  // TODO / OPTIMIZE (bug 1316824): see comment in emitMinF32.
  ScratchF64 zero(*this);
  moveImmF64(0, zero);
  masm.subDouble(zero, r);
  masm.subDouble(zero, rs);
  masm.maxDouble(rs, r, HandleNaNSpecially(true));
  freeF64(rs);
  pushF64(r);
}

void BaseCompiler::emitCopysignF32() {
  RegF32 r, rs;
  pop2xF32(&r, &rs);
  RegI32 temp0 = needI32();
  RegI32 temp1 = needI32();
  masm.moveFloat32ToGPR(r, temp0);
  masm.moveFloat32ToGPR(rs, temp1);
  masm.and32(Imm32(INT32_MAX), temp0);
  masm.and32(Imm32(INT32_MIN), temp1);
  masm.or32(temp1, temp0);
  masm.moveGPRToFloat32(temp0, r);
  freeI32(temp0);
  freeI32(temp1);
  freeF32(rs);
  pushF32(r);
}

void BaseCompiler::emitCopysignF64() {
  RegF64 r, rs;
  pop2xF64(&r, &rs);
  RegI64 temp0 = needI64();
  RegI64 temp1 = needI64();
  masm.moveDoubleToGPR64(r, temp0);
  masm.moveDoubleToGPR64(rs, temp1);
  masm.and64(Imm64(INT64_MAX), temp0);
  masm.and64(Imm64(INT64_MIN), temp1);
  masm.or64(temp1, temp0);
  masm.moveGPR64ToDouble(temp0, r);
  freeI64(temp0);
  freeI64(temp1);
  freeF64(rs);
  pushF64(r);
}

void BaseCompiler::emitOrI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.or32(Imm32(c), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32(&r, &rs);
    masm.or32(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitOrI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.or64(Imm64(c), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64(&r, &rs);
    masm.or64(rs, r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitAndI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.and32(Imm32(c), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32(&r, &rs);
    masm.and32(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitAndI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.and64(Imm64(c), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64(&r, &rs);
    masm.and64(rs, r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitXorI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.xor32(Imm32(c), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32(&r, &rs);
    masm.xor32(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitXorI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.xor64(Imm64(c), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64(&r, &rs);
    masm.xor64(rs, r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitShlI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.lshift32(Imm32(c & 31), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32ForShiftOrRotate(&r, &rs);
    maskShiftCount32(rs);
    masm.lshift32(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitShlI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.lshift64(Imm32(c & 63), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64ForShiftOrRotate(&r, &rs);
    masm.lshift64(lowPart(rs), r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitShrI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.rshift32Arithmetic(Imm32(c & 31), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32ForShiftOrRotate(&r, &rs);
    maskShiftCount32(rs);
    masm.rshift32Arithmetic(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitShrI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.rshift64Arithmetic(Imm32(c & 63), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64ForShiftOrRotate(&r, &rs);
    masm.rshift64Arithmetic(lowPart(rs), r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitShrU32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.rshift32(Imm32(c & 31), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32ForShiftOrRotate(&r, &rs);
    maskShiftCount32(rs);
    masm.rshift32(rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitShrU64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    masm.rshift64(Imm32(c & 63), r);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64ForShiftOrRotate(&r, &rs);
    masm.rshift64(lowPart(rs), r);
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitRotrI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.rotateRight(Imm32(c & 31), r, r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32ForShiftOrRotate(&r, &rs);
    masm.rotateRight(rs, r, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitRotrI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    RegI32 temp = needRotate64Temp();
    masm.rotateRight64(Imm32(c & 63), r, r, temp);
    maybeFreeI32(temp);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64ForShiftOrRotate(&r, &rs);
    masm.rotateRight64(lowPart(rs), r, r, maybeHighPart(rs));
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitRotlI32() {
  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.rotateLeft(Imm32(c & 31), r, r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32ForShiftOrRotate(&r, &rs);
    masm.rotateLeft(rs, r, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitRotlI64() {
  int64_t c;
  if (popConstI64(&c)) {
    RegI64 r = popI64();
    RegI32 temp = needRotate64Temp();
    masm.rotateLeft64(Imm32(c & 63), r, r, temp);
    maybeFreeI32(temp);
    pushI64(r);
  } else {
    RegI64 r, rs;
    pop2xI64ForShiftOrRotate(&r, &rs);
    masm.rotateLeft64(lowPart(rs), r, r, maybeHighPart(rs));
    freeI64(rs);
    pushI64(r);
  }
}

void BaseCompiler::emitEqzI32() {
  if (sniffConditionalControlEqz(ValType::I32)) {
    return;
  }

  RegI32 r = popI32();
  masm.cmp32Set(Assembler::Equal, r, Imm32(0), r);
  pushI32(r);
}

void BaseCompiler::emitEqzI64() {
  if (sniffConditionalControlEqz(ValType::I64)) {
    return;
  }

  RegI64 rs = popI64();
  RegI32 rd = fromI64(rs);
  eqz64(rs, rd);
  freeI64Except(rs, rd);
  pushI32(rd);
}

void BaseCompiler::emitClzI32() {
  RegI32 r = popI32();
  masm.clz32(r, r, IsKnownNotZero(false));
  pushI32(r);
}

void BaseCompiler::emitClzI64() {
  RegI64 r = popI64();
  masm.clz64(r, lowPart(r));
  maybeClearHighPart(r);
  pushI64(r);
}

void BaseCompiler::emitCtzI32() {
  RegI32 r = popI32();
  masm.ctz32(r, r, IsKnownNotZero(false));
  pushI32(r);
}

void BaseCompiler::emitCtzI64() {
  RegI64 r = popI64();
  masm.ctz64(r, lowPart(r));
  maybeClearHighPart(r);
  pushI64(r);
}

void BaseCompiler::emitPopcntI32() {
  RegI32 r = popI32();
  RegI32 temp = needPopcnt32Temp();
  masm.popcnt32(r, r, temp);
  maybeFreeI32(temp);
  pushI32(r);
}

void BaseCompiler::emitPopcntI64() {
  RegI64 r = popI64();
  RegI32 temp = needPopcnt64Temp();
  masm.popcnt64(r, r, temp);
  maybeFreeI32(temp);
  pushI64(r);
}

void BaseCompiler::emitAbsF32() {
  RegF32 r = popF32();
  masm.absFloat32(r, r);
  pushF32(r);
}

void BaseCompiler::emitAbsF64() {
  RegF64 r = popF64();
  masm.absDouble(r, r);
  pushF64(r);
}

void BaseCompiler::emitNegateF32() {
  RegF32 r = popF32();
  masm.negateFloat(r);
  pushF32(r);
}

void BaseCompiler::emitNegateF64() {
  RegF64 r = popF64();
  masm.negateDouble(r);
  pushF64(r);
}

void BaseCompiler::emitSqrtF32() {
  RegF32 r = popF32();
  masm.sqrtFloat32(r, r);
  pushF32(r);
}

void BaseCompiler::emitSqrtF64() {
  RegF64 r = popF64();
  masm.sqrtDouble(r, r);
  pushF64(r);
}

template <TruncFlags flags>
bool BaseCompiler::emitTruncateF32ToI32() {
  RegF32 rs = popF32();
  RegI32 rd = needI32();
  if (!truncateF32ToI32(rs, rd, flags)) {
    return false;
  }
  freeF32(rs);
  pushI32(rd);
  return true;
}

template <TruncFlags flags>
bool BaseCompiler::emitTruncateF64ToI32() {
  RegF64 rs = popF64();
  RegI32 rd = needI32();
  if (!truncateF64ToI32(rs, rd, flags)) {
    return false;
  }
  freeF64(rs);
  pushI32(rd);
  return true;
}

#ifndef RABALDR_FLOAT_TO_I64_CALLOUT
template <TruncFlags flags>
bool BaseCompiler::emitTruncateF32ToI64() {
  RegF32 rs = popF32();
  RegI64 rd = needI64();
  RegF64 temp = needTempForFloatingToI64(flags);
  if (!truncateF32ToI64(rs, rd, flags, temp)) {
    return false;
  }
  maybeFreeF64(temp);
  freeF32(rs);
  pushI64(rd);
  return true;
}

template <TruncFlags flags>
bool BaseCompiler::emitTruncateF64ToI64() {
  RegF64 rs = popF64();
  RegI64 rd = needI64();
  RegF64 temp = needTempForFloatingToI64(flags);
  if (!truncateF64ToI64(rs, rd, flags, temp)) {
    return false;
  }
  maybeFreeF64(temp);
  freeF64(rs);
  pushI64(rd);
  return true;
}
#endif  // RABALDR_FLOAT_TO_I64_CALLOUT

void BaseCompiler::emitWrapI64ToI32() {
  RegI64 rs = popI64();
  RegI32 rd = fromI64(rs);
  masm.move64To32(rs, rd);
  freeI64Except(rs, rd);
  pushI32(rd);
}

void BaseCompiler::emitExtendI32_8() {
  RegI32 r = popI32();
#ifdef JS_CODEGEN_X86
  if (!ra.isSingleByteI32(r)) {
    ScratchI8 scratch(*this);
    moveI32(r, scratch);
    masm.move8SignExtend(scratch, r);
    pushI32(r);
    return;
  }
#endif
  masm.move8SignExtend(r, r);
  pushI32(r);
}

void BaseCompiler::emitExtendI32_16() {
  RegI32 r = popI32();
  masm.move16SignExtend(r, r);
  pushI32(r);
}

void BaseCompiler::emitExtendI64_8() {
  RegI64 r;
  popI64ForSignExtendI64(&r);
  masm.move8To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendI64_16() {
  RegI64 r;
  popI64ForSignExtendI64(&r);
  masm.move16To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendI64_32() {
  RegI64 r;
  popI64ForSignExtendI64(&r);
  masm.move32To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendI32ToI64() {
  RegI64 r;
  popI32ForSignExtendI64(&r);
  masm.move32To64SignExtend(lowPart(r), r);
  pushI64(r);
}

void BaseCompiler::emitExtendU32ToI64() {
  RegI32 rs = popI32();
  RegI64 rd = widenI32(rs);
  masm.move32To64ZeroExtend(rs, rd);
  pushI64(rd);
}

void BaseCompiler::emitReinterpretF32AsI32() {
  RegF32 rs = popF32();
  RegI32 rd = needI32();
  masm.moveFloat32ToGPR(rs, rd);
  freeF32(rs);
  pushI32(rd);
}

void BaseCompiler::emitReinterpretF64AsI64() {
  RegF64 rs = popF64();
  RegI64 rd = needI64();
  masm.moveDoubleToGPR64(rs, rd);
  freeF64(rs);
  pushI64(rd);
}

void BaseCompiler::emitConvertF64ToF32() {
  RegF64 rs = popF64();
  RegF32 rd = needF32();
  masm.convertDoubleToFloat32(rs, rd);
  freeF64(rs);
  pushF32(rd);
}

void BaseCompiler::emitConvertI32ToF32() {
  RegI32 rs = popI32();
  RegF32 rd = needF32();
  masm.convertInt32ToFloat32(rs, rd);
  freeI32(rs);
  pushF32(rd);
}

void BaseCompiler::emitConvertU32ToF32() {
  RegI32 rs = popI32();
  RegF32 rd = needF32();
  masm.convertUInt32ToFloat32(rs, rd);
  freeI32(rs);
  pushF32(rd);
}

#ifndef RABALDR_I64_TO_FLOAT_CALLOUT
void BaseCompiler::emitConvertI64ToF32() {
  RegI64 rs = popI64();
  RegF32 rd = needF32();
  convertI64ToF32(rs, IsUnsigned(false), rd, RegI32());
  freeI64(rs);
  pushF32(rd);
}

void BaseCompiler::emitConvertU64ToF32() {
  RegI64 rs = popI64();
  RegF32 rd = needF32();
  RegI32 temp = needConvertI64ToFloatTemp(ValType::F32, IsUnsigned(true));
  convertI64ToF32(rs, IsUnsigned(true), rd, temp);
  maybeFreeI32(temp);
  freeI64(rs);
  pushF32(rd);
}
#endif

void BaseCompiler::emitConvertF32ToF64() {
  RegF32 rs = popF32();
  RegF64 rd = needF64();
  masm.convertFloat32ToDouble(rs, rd);
  freeF32(rs);
  pushF64(rd);
}

void BaseCompiler::emitConvertI32ToF64() {
  RegI32 rs = popI32();
  RegF64 rd = needF64();
  masm.convertInt32ToDouble(rs, rd);
  freeI32(rs);
  pushF64(rd);
}

void BaseCompiler::emitConvertU32ToF64() {
  RegI32 rs = popI32();
  RegF64 rd = needF64();
  masm.convertUInt32ToDouble(rs, rd);
  freeI32(rs);
  pushF64(rd);
}

#ifndef RABALDR_I64_TO_FLOAT_CALLOUT
void BaseCompiler::emitConvertI64ToF64() {
  RegI64 rs = popI64();
  RegF64 rd = needF64();
  convertI64ToF64(rs, IsUnsigned(false), rd, RegI32());
  freeI64(rs);
  pushF64(rd);
}

void BaseCompiler::emitConvertU64ToF64() {
  RegI64 rs = popI64();
  RegF64 rd = needF64();
  RegI32 temp = needConvertI64ToFloatTemp(ValType::F64, IsUnsigned(true));
  convertI64ToF64(rs, IsUnsigned(true), rd, temp);
  maybeFreeI32(temp);
  freeI64(rs);
  pushF64(rd);
}
#endif  // RABALDR_I64_TO_FLOAT_CALLOUT

void BaseCompiler::emitReinterpretI32AsF32() {
  RegI32 rs = popI32();
  RegF32 rd = needF32();
  masm.moveGPRToFloat32(rs, rd);
  freeI32(rs);
  pushF32(rd);
}

void BaseCompiler::emitReinterpretI64AsF64() {
  RegI64 rs = popI64();
  RegF64 rd = needF64();
  masm.moveGPR64ToDouble(rs, rd);
  freeI64(rs);
  pushF64(rd);
}

template <typename Cond>
bool BaseCompiler::sniffConditionalControlCmp(Cond compareOp,
                                              ValType operandType) {
  MOZ_ASSERT(latentOp_ == LatentOp::None,
             "Latent comparison state not properly reset");

#ifdef JS_CODEGEN_X86
  // On x86, latent i64 binary comparisons use too many registers: the
  // reserved join register and the lhs and rhs operands require six, but we
  // only have five.
  if (operandType == ValType::I64) {
    return false;
  }
#endif

  // No optimization for pointer compares yet.
  if (operandType.isReference()) {
    return false;
  }

  OpBytes op;
  iter_.peekOp(&op);
  switch (op.b0) {
    case uint16_t(Op::BrIf):
    case uint16_t(Op::If):
    case uint16_t(Op::Select):
      setLatentCompare(compareOp, operandType);
      return true;
    default:
      return false;
  }
}

bool BaseCompiler::sniffConditionalControlEqz(ValType operandType) {
  MOZ_ASSERT(latentOp_ == LatentOp::None,
             "Latent comparison state not properly reset");

  OpBytes op;
  iter_.peekOp(&op);
  switch (op.b0) {
    case uint16_t(Op::BrIf):
    case uint16_t(Op::Select):
    case uint16_t(Op::If):
      setLatentEqz(operandType);
      return true;
    default:
      return false;
  }
}

void BaseCompiler::emitBranchSetup(BranchState* b) {
  maybeReserveJoinReg(b->resultType);

  // Set up fields so that emitBranchPerform() need not switch on latentOp_.
  switch (latentOp_) {
    case LatentOp::None: {
      latentIntCmp_ = Assembler::NotEqual;
      latentType_ = ValType::I32;
      b->i32.lhs = popI32();
      b->i32.rhsImm = true;
      b->i32.imm = 0;
      break;
    }
    case LatentOp::Compare: {
      switch (latentType_.code()) {
        case ValType::I32: {
          if (popConstI32(&b->i32.imm)) {
            b->i32.lhs = popI32();
            b->i32.rhsImm = true;
          } else {
            pop2xI32(&b->i32.lhs, &b->i32.rhs);
            b->i32.rhsImm = false;
          }
          break;
        }
        case ValType::I64: {
          pop2xI64(&b->i64.lhs, &b->i64.rhs);
          b->i64.rhsImm = false;
          break;
        }
        case ValType::F32: {
          pop2xF32(&b->f32.lhs, &b->f32.rhs);
          break;
        }
        case ValType::F64: {
          pop2xF64(&b->f64.lhs, &b->f64.rhs);
          break;
        }
        default: {
          MOZ_CRASH("Unexpected type for LatentOp::Compare");
        }
      }
      break;
    }
    case LatentOp::Eqz: {
      switch (latentType_.code()) {
        case ValType::I32: {
          latentIntCmp_ = Assembler::Equal;
          b->i32.lhs = popI32();
          b->i32.rhsImm = true;
          b->i32.imm = 0;
          break;
        }
        case ValType::I64: {
          latentIntCmp_ = Assembler::Equal;
          b->i64.lhs = popI64();
          b->i64.rhsImm = true;
          b->i64.imm = 0;
          break;
        }
        default: {
          MOZ_CRASH("Unexpected type for LatentOp::Eqz");
        }
      }
      break;
    }
  }

  maybeUnreserveJoinReg(b->resultType);
}

void BaseCompiler::emitBranchPerform(BranchState* b) {
  switch (latentType_.code()) {
    case ValType::I32: {
      if (b->i32.rhsImm) {
        jumpConditionalWithJoinReg(b, latentIntCmp_, b->i32.lhs,
                                   Imm32(b->i32.imm));
      } else {
        jumpConditionalWithJoinReg(b, latentIntCmp_, b->i32.lhs, b->i32.rhs);
        freeI32(b->i32.rhs);
      }
      freeI32(b->i32.lhs);
      break;
    }
    case ValType::I64: {
      if (b->i64.rhsImm) {
        jumpConditionalWithJoinReg(b, latentIntCmp_, b->i64.lhs,
                                   Imm64(b->i64.imm));
      } else {
        jumpConditionalWithJoinReg(b, latentIntCmp_, b->i64.lhs, b->i64.rhs);
        freeI64(b->i64.rhs);
      }
      freeI64(b->i64.lhs);
      break;
    }
    case ValType::F32: {
      jumpConditionalWithJoinReg(b, latentDoubleCmp_, b->f32.lhs, b->f32.rhs);
      freeF32(b->f32.lhs);
      freeF32(b->f32.rhs);
      break;
    }
    case ValType::F64: {
      jumpConditionalWithJoinReg(b, latentDoubleCmp_, b->f64.lhs, b->f64.rhs);
      freeF64(b->f64.lhs);
      freeF64(b->f64.rhs);
      break;
    }
    default: {
      MOZ_CRASH("Unexpected type for LatentOp::Compare");
    }
  }
  resetLatentOp();
}

// For blocks and loops and ifs:
//
//  - Sync the value stack before going into the block in order to simplify exit
//    from the block: all exits from the block can assume that there are no
//    live registers except the one carrying the exit value.
//  - The block can accumulate a number of dead values on the stacks, so when
//    branching out of the block or falling out at the end be sure to
//    pop the appropriate stacks back to where they were on entry, while
//    preserving the exit value.
//  - A continue branch in a loop is much like an exit branch, but the branch
//    value must not be preserved.
//  - The exit value is always in a designated join register (type dependent).

bool BaseCompiler::emitBlock() {
  if (!iter_.readBlock()) {
    return false;
  }

  if (!deadCode_) {
    sync();  // Simplifies branching out from block
  }

  initControl(controlItem());

  return true;
}

void BaseCompiler::endBlock(ExprType type) {
  Control& block = controlItem();

  // Save the value.
  Maybe<AnyReg> r;
  if (!deadCode_) {
    r = popJoinRegUnlessVoid(type);
    block.bceSafeOnExit &= bceSafe_;
  }

  // Leave the block.
  fr.popStackOnBlockExit(block.stackHeight, deadCode_);
  popValueStackTo(block.stackSize);

  // Bind after cleanup: branches out will have popped the stack.
  if (block.label.used()) {
    masm.bind(&block.label);
    // No value was provided by the fallthrough but the branch out will
    // have stored one in joinReg, so capture that.
    if (deadCode_) {
      r = captureJoinRegUnlessVoid(type);
    }
    deadCode_ = false;
  }

  bceSafe_ = block.bceSafeOnExit;

  // Retain the value stored in joinReg by all paths, if there are any.
  if (!deadCode_) {
    pushJoinRegUnlessVoid(r);
  }
}

bool BaseCompiler::emitLoop() {
  if (!iter_.readLoop()) {
    return false;
  }

  if (!deadCode_) {
    sync();  // Simplifies branching out from block
  }

  initControl(controlItem());
  bceSafe_ = 0;

  if (!deadCode_) {
    masm.nopAlign(CodeAlignment);
    masm.bind(&controlItem(0).label);
    if (!addInterruptCheck()) {
      return false;
    }
  }

  return true;
}

void BaseCompiler::endLoop(ExprType type) {
  Control& block = controlItem();

  Maybe<AnyReg> r;
  if (!deadCode_) {
    r = popJoinRegUnlessVoid(type);
    // block.bceSafeOnExit need not be updated because it won't be used for
    // the fallthrough path.
  }

  fr.popStackOnBlockExit(block.stackHeight, deadCode_);
  popValueStackTo(block.stackSize);

  // bceSafe_ stays the same along the fallthrough path because branches to
  // loops branch to the top.

  // Retain the value stored in joinReg by all paths.
  if (!deadCode_) {
    pushJoinRegUnlessVoid(r);
  }
}

// The bodies of the "then" and "else" arms can be arbitrary sequences
// of expressions, they push control and increment the nesting and can
// even be targeted by jumps.  A branch to the "if" block branches to
// the exit of the if, ie, it's like "break".  Consider:
//
//      (func (result i32)
//       (if (i32.const 1)
//           (begin (br 1) (unreachable))
//           (begin (unreachable)))
//       (i32.const 1))
//
// The branch causes neither of the unreachable expressions to be
// evaluated.

bool BaseCompiler::emitIf() {
  Nothing unused_cond;
  if (!iter_.readIf(&unused_cond)) {
    return false;
  }

  BranchState b(&controlItem().otherLabel, InvertBranch(true));
  if (!deadCode_) {
    emitBranchSetup(&b);
    sync();
  } else {
    resetLatentOp();
  }

  initControl(controlItem());

  if (!deadCode_) {
    emitBranchPerform(&b);
  }

  return true;
}

void BaseCompiler::endIfThen() {
  Control& ifThen = controlItem();

  fr.popStackOnBlockExit(ifThen.stackHeight, deadCode_);
  popValueStackTo(ifThen.stackSize);

  if (ifThen.otherLabel.used()) {
    masm.bind(&ifThen.otherLabel);
  }

  if (ifThen.label.used()) {
    masm.bind(&ifThen.label);
  }

  if (!deadCode_) {
    ifThen.bceSafeOnExit &= bceSafe_;
  }

  deadCode_ = ifThen.deadOnArrival;

  bceSafe_ = ifThen.bceSafeOnExit & ifThen.bceSafeOnEntry;
}

bool BaseCompiler::emitElse() {
  ExprType thenType;
  Nothing unused_thenValue;

  if (!iter_.readElse(&thenType, &unused_thenValue)) {
    return false;
  }

  Control& ifThenElse = controlItem(0);

  // See comment in endIfThenElse, below.

  // Exit the "then" branch.

  ifThenElse.deadThenBranch = deadCode_;

  Maybe<AnyReg> r;
  if (!deadCode_) {
    r = popJoinRegUnlessVoid(thenType);
  }

  fr.popStackOnBlockExit(ifThenElse.stackHeight, deadCode_);
  popValueStackTo(ifThenElse.stackSize);

  if (!deadCode_) {
    masm.jump(&ifThenElse.label);
  }

  if (ifThenElse.otherLabel.used()) {
    masm.bind(&ifThenElse.otherLabel);
  }

  // Reset to the "else" branch.

  if (!deadCode_) {
    freeJoinRegUnlessVoid(r);
    ifThenElse.bceSafeOnExit &= bceSafe_;
  }

  deadCode_ = ifThenElse.deadOnArrival;
  bceSafe_ = ifThenElse.bceSafeOnEntry;

  return true;
}

void BaseCompiler::endIfThenElse(ExprType type) {
  Control& ifThenElse = controlItem();

  // The expression type is not a reliable guide to what we'll find
  // on the stack, we could have (if E (i32.const 1) (unreachable))
  // in which case the "else" arm is AnyType but the type of the
  // full expression is I32.  So restore whatever's there, not what
  // we want to find there.  The "then" arm has the same constraint.

  Maybe<AnyReg> r;
  if (!deadCode_) {
    r = popJoinRegUnlessVoid(type);
    ifThenElse.bceSafeOnExit &= bceSafe_;
  }

  fr.popStackOnBlockExit(ifThenElse.stackHeight, deadCode_);
  popValueStackTo(ifThenElse.stackSize);

  if (ifThenElse.label.used()) {
    masm.bind(&ifThenElse.label);
  }

  bool joinLive =
      !ifThenElse.deadOnArrival &&
      (!ifThenElse.deadThenBranch || !deadCode_ || ifThenElse.label.bound());

  if (joinLive) {
    // No value was provided by the "then" path but capture the one
    // provided by the "else" path.
    if (deadCode_) {
      r = captureJoinRegUnlessVoid(type);
    }
    deadCode_ = false;
  }

  bceSafe_ = ifThenElse.bceSafeOnExit;

  if (!deadCode_) {
    pushJoinRegUnlessVoid(r);
  }
}

bool BaseCompiler::emitEnd() {
  LabelKind kind;
  ExprType type;
  Nothing unused_value;
  if (!iter_.readEnd(&kind, &type, &unused_value)) {
    return false;
  }

  switch (kind) {
    case LabelKind::Body:
      endBlock(type);
      iter_.popEnd();
      MOZ_ASSERT(iter_.controlStackEmpty());
      doReturn(type, PopStack(false));
      return iter_.readFunctionEnd(iter_.end());
    case LabelKind::Block:
      endBlock(type);
      break;
    case LabelKind::Loop:
      endLoop(type);
      break;
    case LabelKind::Then:
      endIfThen();
      break;
    case LabelKind::Else:
      endIfThenElse(type);
      break;
  }

  iter_.popEnd();

  return true;
}

bool BaseCompiler::emitBr() {
  uint32_t relativeDepth;
  ExprType type;
  Nothing unused_value;
  if (!iter_.readBr(&relativeDepth, &type, &unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  Control& target = controlItem(relativeDepth);
  target.bceSafeOnExit &= bceSafe_;

  // Save any value in the designated join register, where the
  // normal block exit code will also leave it.

  Maybe<AnyReg> r = popJoinRegUnlessVoid(type);

  fr.popStackBeforeBranch(target.stackHeight);
  masm.jump(&target.label);

  // The register holding the join value is free for the remainder
  // of this block.

  freeJoinRegUnlessVoid(r);

  deadCode_ = true;

  return true;
}

bool BaseCompiler::emitBrIf() {
  uint32_t relativeDepth;
  ExprType type;
  Nothing unused_value, unused_condition;
  if (!iter_.readBrIf(&relativeDepth, &type, &unused_value,
                      &unused_condition)) {
    return false;
  }

  if (deadCode_) {
    resetLatentOp();
    return true;
  }

  Control& target = controlItem(relativeDepth);
  target.bceSafeOnExit &= bceSafe_;

  BranchState b(&target.label, target.stackHeight, InvertBranch(false), type);
  emitBranchSetup(&b);
  emitBranchPerform(&b);

  return true;
}

bool BaseCompiler::emitBrTable() {
  Uint32Vector depths;
  uint32_t defaultDepth;
  ExprType branchValueType;
  Nothing unused_value, unused_index;
  if (!iter_.readBrTable(&depths, &defaultDepth, &branchValueType,
                         &unused_value, &unused_index)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // Don't use joinReg for rc
  maybeReserveJoinRegI(branchValueType);

  // Table switch value always on top.
  RegI32 rc = popI32();

  maybeUnreserveJoinRegI(branchValueType);

  Maybe<AnyReg> r = popJoinRegUnlessVoid(branchValueType);

  Label dispatchCode;
  masm.branch32(Assembler::Below, rc, Imm32(depths.length()), &dispatchCode);

  // This is the out-of-range stub.  rc is dead here but we don't need it.

  fr.popStackBeforeBranch(controlItem(defaultDepth).stackHeight);
  controlItem(defaultDepth).bceSafeOnExit &= bceSafe_;
  masm.jump(&controlItem(defaultDepth).label);

  // Emit stubs.  rc is dead in all of these but we don't need it.
  //
  // The labels in the vector are in the TempAllocator and will
  // be freed by and by.
  //
  // TODO / OPTIMIZE (Bug 1316804): Branch directly to the case code if we
  // can, don't emit an intermediate stub.

  LabelVector stubs;
  if (!stubs.reserve(depths.length())) {
    return false;
  }

  for (uint32_t depth : depths) {
    stubs.infallibleEmplaceBack(NonAssertingLabel());
    masm.bind(&stubs.back());
    fr.popStackBeforeBranch(controlItem(depth).stackHeight);
    controlItem(depth).bceSafeOnExit &= bceSafe_;
    masm.jump(&controlItem(depth).label);
  }

  // Emit table.

  Label theTable;
  jumpTable(stubs, &theTable);

  // Emit indirect jump.  rc is live here.

  tableSwitch(&theTable, rc, &dispatchCode);

  deadCode_ = true;

  // Clean up.

  freeI32(rc);
  freeJoinRegUnlessVoid(r);

  return true;
}

bool BaseCompiler::emitDrop() {
  if (!iter_.readDrop()) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  dropValue();
  return true;
}

void BaseCompiler::doReturn(ExprType type, bool popStack) {
  if (deadCode_) {
    return;
  }
  switch (type.code()) {
    case ExprType::Void: {
      returnCleanup(popStack);
      break;
    }
    case ExprType::I32: {
      RegI32 rv = popI32(RegI32(ReturnReg));
      returnCleanup(popStack);
      freeI32(rv);
      break;
    }
    case ExprType::I64: {
      RegI64 rv = popI64(RegI64(ReturnReg64));
      returnCleanup(popStack);
      freeI64(rv);
      break;
    }
    case ExprType::F64: {
      RegF64 rv = popF64(RegF64(ReturnDoubleReg));
      returnCleanup(popStack);
      freeF64(rv);
      break;
    }
    case ExprType::F32: {
      RegF32 rv = popF32(RegF32(ReturnFloat32Reg));
      returnCleanup(popStack);
      freeF32(rv);
      break;
    }
    case ExprType::Ref:
    case ExprType::NullRef:
    case ExprType::FuncRef:
    case ExprType::AnyRef: {
      RegPtr rv = popRef(RegPtr(ReturnReg));
      returnCleanup(popStack);
      freeRef(rv);
      break;
    }
    default: {
      MOZ_CRASH("Function return type");
    }
  }
}

bool BaseCompiler::emitReturn() {
  Nothing unused_value;
  if (!iter_.readReturn(&unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  doReturn(funcType().ret(), PopStack(true));
  deadCode_ = true;

  return true;
}

bool BaseCompiler::emitCallArgs(const ValTypeVector& argTypes,
                                FunctionCall* baselineCall) {
  MOZ_ASSERT(!deadCode_);

  startCallArgs(StackArgAreaSizeUnaligned(argTypes), baselineCall);

  uint32_t numArgs = argTypes.length();
  for (size_t i = 0; i < numArgs; ++i) {
    passArg(argTypes[i], peek(numArgs - 1 - i), baselineCall);
  }

  masm.loadWasmTlsRegFromFrame();
  return true;
}

void BaseCompiler::pushReturnValueOfCall(const FunctionCall& call,
                                         MIRType type) {
  switch (type) {
    case MIRType::Int32: {
      RegI32 rv = captureReturnedI32();
      pushI32(rv);
      break;
    }
    case MIRType::Int64: {
      RegI64 rv = captureReturnedI64();
      pushI64(rv);
      break;
    }
    case MIRType::Float32: {
      RegF32 rv = captureReturnedF32(call);
      pushF32(rv);
      break;
    }
    case MIRType::Double: {
      RegF64 rv = captureReturnedF64(call);
      pushF64(rv);
      break;
    }
    case MIRType::RefOrNull: {
      RegPtr rv = captureReturnedRef();
      pushRef(rv);
      break;
    }
    default:
      // In particular, passing |type| as MIRType::Void or MIRType::Pointer to
      // this function is an error.
      MOZ_CRASH("Function return type");
  }
}

void BaseCompiler::pushReturnValueOfCall(const FunctionCall& call,
                                         ExprType type) {
  MOZ_ASSERT(type.code() != ExprType::NullRef);
  pushReturnValueOfCall(call, ToMIRType(type));
}

// For now, always sync() at the beginning of the call to easily save live
// values.
//
// TODO / OPTIMIZE (Bug 1316806): We may be able to avoid a full sync(), since
// all we want is to save live registers that won't be saved by the callee or
// that we need for outgoing args - we don't need to sync the locals.  We can
// just push the necessary registers, it'll be like a lightweight sync.
//
// Even some of the pushing may be unnecessary if the registers will be consumed
// by the call, because then what we want is parallel assignment to the argument
// registers or onto the stack for outgoing arguments.  A sync() is just
// simpler.

bool BaseCompiler::emitCall() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t funcIndex;
  BaseOpIter::ValueVector args_;
  if (!iter_.readCall(&funcIndex, &args_)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  sync();

  const FuncType& funcType = *env_.funcTypes[funcIndex];
  bool import = env_.funcIsImport(funcIndex);

  uint32_t numArgs = funcType.args().length();
  size_t stackSpace = stackConsumed(numArgs);

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::Wasm,
            import ? InterModule::True : InterModule::False);

  if (!emitCallArgs(funcType.args(), &baselineCall)) {
    return false;
  }

  CodeOffset raOffset;
  if (import) {
    raOffset =
        callImport(env_.funcImportGlobalDataOffsets[funcIndex], baselineCall);
  } else {
    raOffset = callDefinition(funcIndex, baselineCall);
  }

  if (!createStackMap("emitCall", raOffset)) {
    return false;
  }

  endCall(baselineCall, stackSpace);

  popValueStackBy(numArgs);

  if (funcType.ret() != ExprType::Void) {
    pushReturnValueOfCall(baselineCall, funcType.ret());
  }

  return true;
}

bool BaseCompiler::emitCallIndirect() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t funcTypeIndex;
  uint32_t tableIndex;
  Nothing callee_;
  BaseOpIter::ValueVector args_;
  if (!iter_.readCallIndirect(&funcTypeIndex, &tableIndex, &callee_, &args_)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  sync();

  const FuncTypeWithId& funcType = env_.types[funcTypeIndex].funcType();

  // Stack: ... arg1 .. argn callee

  uint32_t numArgs = funcType.args().length();
  size_t stackSpace = stackConsumed(numArgs + 1);

  // The arguments must be at the stack top for emitCallArgs, so pop the
  // callee if it is on top.  Note this only pops the compiler's stack,
  // not the CPU stack.

  Stk callee = stk_.popCopy();

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::Wasm, InterModule::True);

  if (!emitCallArgs(funcType.args(), &baselineCall)) {
    return false;
  }

  CodeOffset raOffset =
      callIndirect(funcTypeIndex, tableIndex, callee, baselineCall);
  if (!createStackMap("emitCallIndirect", raOffset)) {
    return false;
  }

  endCall(baselineCall, stackSpace);

  popValueStackBy(numArgs);

  if (funcType.ret() != ExprType::Void) {
    pushReturnValueOfCall(baselineCall, funcType.ret());
  }

  return true;
}

void BaseCompiler::emitRound(RoundingMode roundingMode, ValType operandType) {
  if (operandType == ValType::F32) {
    RegF32 f0 = popF32();
    roundF32(roundingMode, f0);
    pushF32(f0);
  } else if (operandType == ValType::F64) {
    RegF64 f0 = popF64();
    roundF64(roundingMode, f0);
    pushF64(f0);
  } else {
    MOZ_CRASH("unexpected type");
  }
}

bool BaseCompiler::emitUnaryMathBuiltinCall(SymbolicAddress callee,
                                            ValType operandType) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing operand_;
  if (!iter_.readUnary(operandType, &operand_)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  RoundingMode roundingMode;
  if (IsRoundingFunction(callee, &roundingMode) &&
      supportsRoundInstruction(roundingMode)) {
    emitRound(roundingMode, operandType);
    return true;
  }

  sync();

  ValTypeVector& signature = operandType == ValType::F32 ? SigF_ : SigD_;
  ExprType retType =
      operandType == ValType::F32 ? ExprType::F32 : ExprType::F64;
  uint32_t numArgs = signature.length();
  size_t stackSpace = stackConsumed(numArgs);

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::Builtin, InterModule::False);

  if (!emitCallArgs(signature, &baselineCall)) {
    return false;
  }

  CodeOffset raOffset = builtinCall(callee, baselineCall);
  if (!createStackMap("emitUnaryMathBuiltin[..]", raOffset)) {
    return false;
  }

  endCall(baselineCall, stackSpace);

  popValueStackBy(numArgs);

  // We know retType isn't ExprType::Void here, so there's no need to check it.
  pushReturnValueOfCall(baselineCall, retType);

  return true;
}

#ifdef RABALDR_INT_DIV_I64_CALLOUT
bool BaseCompiler::emitDivOrModI64BuiltinCall(SymbolicAddress callee,
                                              ValType operandType) {
  MOZ_ASSERT(operandType == ValType::I64);
  MOZ_ASSERT(!deadCode_);

  sync();

  needI64(specific_.abiReturnRegI64);

  RegI64 rhs = popI64();
  RegI64 srcDest = popI64ToSpecific(specific_.abiReturnRegI64);

  Label done;

  checkDivideByZeroI64(rhs);

  if (callee == SymbolicAddress::DivI64) {
    checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(false));
  } else if (callee == SymbolicAddress::ModI64) {
    checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(true));
  }

  masm.setupWasmABICall();
  masm.passABIArg(srcDest.high);
  masm.passABIArg(srcDest.low);
  masm.passABIArg(rhs.high);
  masm.passABIArg(rhs.low);
  CodeOffset raOffset = masm.callWithABI(bytecodeOffset(), callee);
  if (!createStackMap("emitDivOrModI64Bui[..]", raOffset)) {
    return false;
  }

  masm.bind(&done);

  freeI64(rhs);
  pushI64(srcDest);
  return true;
}
#endif  // RABALDR_INT_DIV_I64_CALLOUT

#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
bool BaseCompiler::emitConvertInt64ToFloatingCallout(SymbolicAddress callee,
                                                     ValType operandType,
                                                     ValType resultType) {
  sync();

  RegI64 input = popI64();

  FunctionCall call(0);

  masm.setupWasmABICall();
#  ifdef JS_PUNBOX64
  MOZ_CRASH("BaseCompiler platform hook: emitConvertInt64ToFloatingCallout");
#  else
  masm.passABIArg(input.high);
  masm.passABIArg(input.low);
#  endif
  CodeOffset raOffset = masm.callWithABI(
      bytecodeOffset(), callee,
      resultType == ValType::F32 ? MoveOp::FLOAT32 : MoveOp::DOUBLE);
  if (!createStackMap("emitConvertInt64To[..]", raOffset)) {
    return false;
  }

  freeI64(input);

  if (resultType == ValType::F32) {
    pushF32(captureReturnedF32(call));
  } else {
    pushF64(captureReturnedF64(call));
  }

  return true;
}
#endif  // RABALDR_I64_TO_FLOAT_CALLOUT

#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
// `Callee` always takes a double, so a float32 input must be converted.
bool BaseCompiler::emitConvertFloatingToInt64Callout(SymbolicAddress callee,
                                                     ValType operandType,
                                                     ValType resultType) {
  RegF64 doubleInput;
  if (operandType == ValType::F32) {
    doubleInput = needF64();
    RegF32 input = popF32();
    masm.convertFloat32ToDouble(input, doubleInput);
    freeF32(input);
  } else {
    doubleInput = popF64();
  }

  // We may need the value after the call for the ool check.
  RegF64 otherReg = needF64();
  moveF64(doubleInput, otherReg);
  pushF64(otherReg);

  sync();

  FunctionCall call(0);

  masm.setupWasmABICall();
  masm.passABIArg(doubleInput, MoveOp::DOUBLE);
  CodeOffset raOffset = masm.callWithABI(bytecodeOffset(), callee);
  if (!createStackMap("emitConvertFloatin[..]", raOffset)) {
    return false;
  }

  freeF64(doubleInput);

  RegI64 rv = captureReturnedI64();

  RegF64 inputVal = popF64();

  TruncFlags flags = 0;
  if (callee == SymbolicAddress::TruncateDoubleToUint64) {
    flags |= TRUNC_UNSIGNED;
  }
  if (callee == SymbolicAddress::SaturatingTruncateDoubleToInt64 ||
      callee == SymbolicAddress::SaturatingTruncateDoubleToUint64) {
    flags |= TRUNC_SATURATING;
  }

  // If we're saturating, the callout will always produce the final result
  // value. Otherwise, the callout value will return 0x8000000000000000
  // and we need to produce traps.
  OutOfLineCode* ool = nullptr;
  if (!(flags & TRUNC_SATURATING)) {
    // The OOL check just succeeds or fails, it does not generate a value.
    ool = addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(
        AnyReg(inputVal), rv, flags, bytecodeOffset()));
    if (!ool) {
      return false;
    }

    masm.branch64(Assembler::Equal, rv, Imm64(0x8000000000000000),
                  ool->entry());
    masm.bind(ool->rejoin());
  }

  pushI64(rv);
  freeF64(inputVal);

  return true;
}
#endif  // RABALDR_FLOAT_TO_I64_CALLOUT

bool BaseCompiler::emitGetLocal() {
  uint32_t slot;
  if (!iter_.readGetLocal(locals_, &slot)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // Local loads are pushed unresolved, ie, they may be deferred
  // until needed, until they may be affected by a store, or until a
  // sync.  This is intended to reduce register pressure.

  switch (locals_[slot].code()) {
    case ValType::I32:
      pushLocalI32(slot);
      break;
    case ValType::I64:
      pushLocalI64(slot);
      break;
    case ValType::F64:
      pushLocalF64(slot);
      break;
    case ValType::F32:
      pushLocalF32(slot);
      break;
    case ValType::Ref:
    case ValType::FuncRef:
    case ValType::AnyRef:
      pushLocalRef(slot);
      break;
    case ValType::NullRef:
    default:
      MOZ_CRASH("Local variable type");
  }

  return true;
}

template <bool isSetLocal>
bool BaseCompiler::emitSetOrTeeLocal(uint32_t slot) {
  if (deadCode_) {
    return true;
  }

  bceLocalIsUpdated(slot);
  switch (locals_[slot].code()) {
    case ValType::I32: {
      RegI32 rv = popI32();
      syncLocal(slot);
      fr.storeLocalI32(rv, localFromSlot(slot, MIRType::Int32));
      if (isSetLocal) {
        freeI32(rv);
      } else {
        pushI32(rv);
      }
      break;
    }
    case ValType::I64: {
      RegI64 rv = popI64();
      syncLocal(slot);
      fr.storeLocalI64(rv, localFromSlot(slot, MIRType::Int64));
      if (isSetLocal) {
        freeI64(rv);
      } else {
        pushI64(rv);
      }
      break;
    }
    case ValType::F64: {
      RegF64 rv = popF64();
      syncLocal(slot);
      fr.storeLocalF64(rv, localFromSlot(slot, MIRType::Double));
      if (isSetLocal) {
        freeF64(rv);
      } else {
        pushF64(rv);
      }
      break;
    }
    case ValType::F32: {
      RegF32 rv = popF32();
      syncLocal(slot);
      fr.storeLocalF32(rv, localFromSlot(slot, MIRType::Float32));
      if (isSetLocal) {
        freeF32(rv);
      } else {
        pushF32(rv);
      }
      break;
    }
    case ValType::Ref:
    case ValType::FuncRef:
    case ValType::AnyRef: {
      RegPtr rv = popRef();
      syncLocal(slot);
      fr.storeLocalPtr(rv, localFromSlot(slot, MIRType::RefOrNull));
      if (isSetLocal) {
        freeRef(rv);
      } else {
        pushRef(rv);
      }
      break;
    }
    case ValType::NullRef:
    default:
      MOZ_CRASH("Local variable type");
  }

  return true;
}

bool BaseCompiler::emitSetLocal() {
  uint32_t slot;
  Nothing unused_value;
  if (!iter_.readSetLocal(locals_, &slot, &unused_value)) {
    return false;
  }
  return emitSetOrTeeLocal<true>(slot);
}

bool BaseCompiler::emitTeeLocal() {
  uint32_t slot;
  Nothing unused_value;
  if (!iter_.readTeeLocal(locals_, &slot, &unused_value)) {
    return false;
  }
  return emitSetOrTeeLocal<false>(slot);
}

bool BaseCompiler::emitGetGlobal() {
  uint32_t id;
  if (!iter_.readGetGlobal(&id)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const GlobalDesc& global = env_.globals[id];

  if (global.isConstant()) {
    LitVal value = global.constantValue();
    switch (value.type().code()) {
      case ValType::I32:
        pushI32(value.i32());
        break;
      case ValType::I64:
        pushI64(value.i64());
        break;
      case ValType::F32:
        pushF32(value.f32());
        break;
      case ValType::F64:
        pushF64(value.f64());
        break;
      case ValType::Ref:
      case ValType::FuncRef:
      case ValType::AnyRef:
        pushRef(intptr_t(value.ref().forCompiledCode()));
        break;
      case ValType::NullRef:
        MOZ_CRASH("NullRef not expressible");
      default:
        MOZ_CRASH("Global constant type");
    }
    return true;
  }

  switch (global.type().code()) {
    case ValType::I32: {
      RegI32 rv = needI32();
      ScratchI32 tmp(*this);
      masm.load32(addressOfGlobalVar(global, tmp), rv);
      pushI32(rv);
      break;
    }
    case ValType::I64: {
      RegI64 rv = needI64();
      ScratchI32 tmp(*this);
      masm.load64(addressOfGlobalVar(global, tmp), rv);
      pushI64(rv);
      break;
    }
    case ValType::F32: {
      RegF32 rv = needF32();
      ScratchI32 tmp(*this);
      masm.loadFloat32(addressOfGlobalVar(global, tmp), rv);
      pushF32(rv);
      break;
    }
    case ValType::F64: {
      RegF64 rv = needF64();
      ScratchI32 tmp(*this);
      masm.loadDouble(addressOfGlobalVar(global, tmp), rv);
      pushF64(rv);
      break;
    }
    case ValType::Ref:
    case ValType::FuncRef:
    case ValType::AnyRef: {
      RegPtr rv = needRef();
      ScratchI32 tmp(*this);
      masm.loadPtr(addressOfGlobalVar(global, tmp), rv);
      pushRef(rv);
      break;
    }
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
    default:
      MOZ_CRASH("Global variable type");
      break;
  }
  return true;
}

bool BaseCompiler::emitSetGlobal() {
  uint32_t id;
  Nothing unused_value;
  if (!iter_.readSetGlobal(&id, &unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const GlobalDesc& global = env_.globals[id];

  switch (global.type().code()) {
    case ValType::I32: {
      RegI32 rv = popI32();
      ScratchI32 tmp(*this);
      masm.store32(rv, addressOfGlobalVar(global, tmp));
      freeI32(rv);
      break;
    }
    case ValType::I64: {
      RegI64 rv = popI64();
      ScratchI32 tmp(*this);
      masm.store64(rv, addressOfGlobalVar(global, tmp));
      freeI64(rv);
      break;
    }
    case ValType::F32: {
      RegF32 rv = popF32();
      ScratchI32 tmp(*this);
      masm.storeFloat32(rv, addressOfGlobalVar(global, tmp));
      freeF32(rv);
      break;
    }
    case ValType::F64: {
      RegF64 rv = popF64();
      ScratchI32 tmp(*this);
      masm.storeDouble(rv, addressOfGlobalVar(global, tmp));
      freeF64(rv);
      break;
    }
    case ValType::Ref:
    case ValType::FuncRef:
    case ValType::AnyRef: {
      RegPtr valueAddr(PreBarrierReg);
      needRef(valueAddr);
      {
        ScratchI32 tmp(*this);
        masm.computeEffectiveAddress(addressOfGlobalVar(global, tmp),
                                     valueAddr);
      }
      RegPtr rv = popRef();
      // emitBarrieredStore consumes valueAddr
      if (!emitBarrieredStore(Nothing(), valueAddr, rv, global.type())) {
        return false;
      }
      freeRef(rv);
      break;
    }
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
    default:
      MOZ_CRASH("Global variable type");
      break;
  }
  return true;
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
// env_.debugEnabled() is true.
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

// TODO / OPTIMIZE (bug 1329576): There are opportunities to generate better
// code by not moving a constant address with a zero offset into a register.

RegI32 BaseCompiler::popMemoryAccess(MemoryAccessDesc* access,
                                     AccessCheck* check) {
  check->onlyPointerAlignment =
      (access->offset() & (access->byteSize() - 1)) == 0;

  int32_t addrTemp;
  if (popConstI32(&addrTemp)) {
    uint32_t addr = addrTemp;

    uint64_t ea = uint64_t(addr) + uint64_t(access->offset());
    uint64_t limit =
        uint64_t(env_.minMemoryLength) + uint64_t(wasm::OffsetGuardLimit);

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

RegI32 BaseCompiler::maybeLoadTlsForAccess(const AccessCheck& check) {
  RegI32 tls;
  if (needTlsForAccess(check)) {
    tls = needI32();
    masm.loadWasmTlsRegFromFrame(tls);
  }
  return tls;
}

RegI32 BaseCompiler::maybeLoadTlsForAccess(const AccessCheck& check,
                                           RegI32 specific) {
  if (needTlsForAccess(check)) {
    masm.loadWasmTlsRegFromFrame(specific);
    return specific;
  }
  return RegI32::Invalid();
}

bool BaseCompiler::loadCommon(MemoryAccessDesc* access, ValType type) {
  AccessCheck check;

  RegI32 tls, temp1, temp2, temp3;
  needLoadTemps(*access, &temp1, &temp2, &temp3);

  switch (type.code()) {
    case ValType::I32: {
      RegI32 rp = popMemoryAccess(access, &check);
#ifdef JS_CODEGEN_ARM
      RegI32 rv = IsUnaligned(*access) ? needI32() : rp;
#else
      RegI32 rv = rp;
#endif
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp1, temp2, temp3)) {
        return false;
      }
      pushI32(rv);
      if (rp != rv) {
        freeI32(rp);
      }
      break;
    }
    case ValType::I64: {
      RegI64 rv;
      RegI32 rp;
#ifdef JS_CODEGEN_X86
      rv = specific_.abiReturnRegI64;
      needI64(rv);
      rp = popMemoryAccess(access, &check);
#else
      rp = popMemoryAccess(access, &check);
      rv = needI64();
#endif
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp1, temp2, temp3)) {
        return false;
      }
      pushI64(rv);
      freeI32(rp);
      break;
    }
    case ValType::F32: {
      RegI32 rp = popMemoryAccess(access, &check);
      RegF32 rv = needF32();
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp1, temp2, temp3)) {
        return false;
      }
      pushF32(rv);
      freeI32(rp);
      break;
    }
    case ValType::F64: {
      RegI32 rp = popMemoryAccess(access, &check);
      RegF64 rv = needF64();
      tls = maybeLoadTlsForAccess(check);
      if (!load(access, &check, tls, rp, AnyReg(rv), temp1, temp2, temp3)) {
        return false;
      }
      pushF64(rv);
      freeI32(rp);
      break;
    }
    default:
      MOZ_CRASH("load type");
      break;
  }

  maybeFreeI32(tls);
  maybeFreeI32(temp1);
  maybeFreeI32(temp2);
  maybeFreeI32(temp3);

  return true;
}

bool BaseCompiler::emitLoad(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readLoad(type, Scalar::byteSize(viewType), &addr)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  return loadCommon(&access, type);
}

bool BaseCompiler::storeCommon(MemoryAccessDesc* access, ValType resultType) {
  AccessCheck check;

  RegI32 tls;
  RegI32 temp = needStoreTemp(*access, resultType);

  switch (resultType.code()) {
    case ValType::I32: {
      RegI32 rv = popI32();
      RegI32 rp = popMemoryAccess(access, &check);
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
      RegI32 rp = popMemoryAccess(access, &check);
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
      RegI32 rp = popMemoryAccess(access, &check);
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
      RegI32 rp = popMemoryAccess(access, &check);
      tls = maybeLoadTlsForAccess(check);
      if (!store(access, &check, tls, rp, AnyReg(rv), temp)) {
        return false;
      }
      freeI32(rp);
      freeF64(rv);
      break;
    }
    default:
      MOZ_CRASH("store type");
      break;
  }

  maybeFreeI32(tls);
  maybeFreeI32(temp);

  return true;
}

bool BaseCompiler::emitStore(ValType resultType, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readStore(resultType, Scalar::byteSize(viewType), &addr,
                       &unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset());
  return storeCommon(&access, resultType);
}

bool BaseCompiler::emitSelect() {
  StackType type;
  Nothing unused_trueValue;
  Nothing unused_falseValue;
  Nothing unused_condition;
  if (!iter_.readSelect(&type, &unused_trueValue, &unused_falseValue,
                        &unused_condition)) {
    return false;
  }

  if (deadCode_) {
    resetLatentOp();
    return true;
  }

  // I32 condition on top, then false, then true.

  Label done;
  BranchState b(&done);
  emitBranchSetup(&b);

  switch (NonTVarToValType(type).code()) {
    case ValType::I32: {
      RegI32 r, rs;
      pop2xI32(&r, &rs);
      emitBranchPerform(&b);
      moveI32(rs, r);
      masm.bind(&done);
      freeI32(rs);
      pushI32(r);
      break;
    }
    case ValType::I64: {
#ifdef JS_CODEGEN_X86
      // There may be as many as four Int64 values in registers at a time: two
      // for the latent branch operands, and two for the true/false values we
      // normally pop before executing the branch.  On x86 this is one value
      // too many, so we need to generate more complicated code here, and for
      // simplicity's sake we do so even if the branch operands are not Int64.
      // However, the resulting control flow diamond is complicated since the
      // arms of the diamond will have to stay synchronized with respect to
      // their evaluation stack and regalloc state.  To simplify further, we
      // use a double branch and a temporary boolean value for now.
      RegI32 temp = needI32();
      moveImm32(0, temp);
      emitBranchPerform(&b);
      moveImm32(1, temp);
      masm.bind(&done);

      Label trueValue;
      RegI64 r, rs;
      pop2xI64(&r, &rs);
      masm.branch32(Assembler::Equal, temp, Imm32(0), &trueValue);
      moveI64(rs, r);
      masm.bind(&trueValue);
      freeI32(temp);
      freeI64(rs);
      pushI64(r);
#else
      RegI64 r, rs;
      pop2xI64(&r, &rs);
      emitBranchPerform(&b);
      moveI64(rs, r);
      masm.bind(&done);
      freeI64(rs);
      pushI64(r);
#endif
      break;
    }
    case ValType::F32: {
      RegF32 r, rs;
      pop2xF32(&r, &rs);
      emitBranchPerform(&b);
      moveF32(rs, r);
      masm.bind(&done);
      freeF32(rs);
      pushF32(r);
      break;
    }
    case ValType::F64: {
      RegF64 r, rs;
      pop2xF64(&r, &rs);
      emitBranchPerform(&b);
      moveF64(rs, r);
      masm.bind(&done);
      freeF64(rs);
      pushF64(r);
      break;
    }
    case ValType::Ref:
    case ValType::NullRef:
    case ValType::FuncRef:
    case ValType::AnyRef: {
      RegPtr r, rs;
      pop2xRef(&r, &rs);
      emitBranchPerform(&b);
      moveRef(rs, r);
      masm.bind(&done);
      freeRef(rs);
      pushRef(r);
      break;
    }
    default: {
      MOZ_CRASH("select type");
    }
  }

  return true;
}

void BaseCompiler::emitCompareI32(Assembler::Condition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::I32);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  int32_t c;
  if (popConstI32(&c)) {
    RegI32 r = popI32();
    masm.cmp32Set(compareOp, r, Imm32(c), r);
    pushI32(r);
  } else {
    RegI32 r, rs;
    pop2xI32(&r, &rs);
    masm.cmp32Set(compareOp, r, rs, r);
    freeI32(rs);
    pushI32(r);
  }
}

void BaseCompiler::emitCompareI64(Assembler::Condition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::I64);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  RegI64 rs0, rs1;
  pop2xI64(&rs0, &rs1);
  RegI32 rd(fromI64(rs0));
  cmp64Set(compareOp, rs0, rs1, rd);
  freeI64(rs1);
  freeI64Except(rs0, rd);
  pushI32(rd);
}

void BaseCompiler::emitCompareF32(Assembler::DoubleCondition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::F32);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  Label across;
  RegF32 rs0, rs1;
  pop2xF32(&rs0, &rs1);
  RegI32 rd = needI32();
  moveImm32(1, rd);
  masm.branchFloat(compareOp, rs0, rs1, &across);
  moveImm32(0, rd);
  masm.bind(&across);
  freeF32(rs0);
  freeF32(rs1);
  pushI32(rd);
}

void BaseCompiler::emitCompareF64(Assembler::DoubleCondition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(compareType == ValType::F64);

  if (sniffConditionalControlCmp(compareOp, compareType)) {
    return;
  }

  Label across;
  RegF64 rs0, rs1;
  pop2xF64(&rs0, &rs1);
  RegI32 rd = needI32();
  moveImm32(1, rd);
  masm.branchDouble(compareOp, rs0, rs1, &across);
  moveImm32(0, rd);
  masm.bind(&across);
  freeF64(rs0);
  freeF64(rs1);
  pushI32(rd);
}

void BaseCompiler::emitCompareRef(Assembler::Condition compareOp,
                                  ValType compareType) {
  MOZ_ASSERT(!sniffConditionalControlCmp(compareOp, compareType));

  RegPtr rs1, rs2;
  pop2xRef(&rs1, &rs2);
  RegI32 rd = needI32();
  masm.cmpPtrSet(compareOp, rs1, rs2, rd);
  freeRef(rs1);
  freeRef(rs2);
  pushI32(rd);
}

bool BaseCompiler::emitInstanceCall(uint32_t lineOrBytecode,
                                    const SymbolicAddressSignature& builtin,
                                    bool pushReturnedValue /*=true*/) {
  const MIRType* argTypes = builtin.argTypes;
  MOZ_ASSERT(argTypes[0] == MIRType::Pointer);

  sync();

  uint32_t numNonInstanceArgs = builtin.numArgs - 1 /* instance */;
  size_t stackSpace = stackConsumed(numNonInstanceArgs);

  FunctionCall baselineCall(lineOrBytecode);
  beginCall(baselineCall, UseABI::System, InterModule::True);

  ABIArg instanceArg = reservePointerArgument(&baselineCall);

  startCallArgs(StackArgAreaSizeUnaligned(builtin), &baselineCall);
  for (uint32_t i = 1; i < builtin.numArgs; i++) {
    ValType t;
    switch (argTypes[i]) {
      case MIRType::Int32:
        t = ValType::I32;
        break;
      case MIRType::Int64:
        t = ValType::I64;
        break;
      case MIRType::RefOrNull:
        t = ValType::AnyRef;
        break;
      case MIRType::Pointer:
        // Instance function args can now be uninterpreted pointers (eg, for
        // the cases PostBarrier and PostBarrierFilter) so we simply treat
        // them like the equivalently sized integer.
        t = sizeof(void*) == 4 ? ValType::I32 : ValType::I64;
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }
    passArg(t, peek(numNonInstanceArgs - i), &baselineCall);
  }
  CodeOffset raOffset =
      builtinInstanceMethodCall(builtin, instanceArg, baselineCall);
  if (!createStackMap("emitInstanceCall", raOffset)) {
    return false;
  }

  endCall(baselineCall, stackSpace);

  popValueStackBy(numNonInstanceArgs);

  // Note, many clients of emitInstanceCall currently assume that pushing the
  // result here does not destroy ReturnReg.
  //
  // Furthermore, clients assume that if builtin.retType != MIRType::None, the
  // callee will have returned a result and left it in ReturnReg for us to
  // find, and that that register will not be destroyed here (or above).

  if (pushReturnedValue) {
    // For the return type only, MIRType::None is used to indicate that the
    // call doesn't return a result, that is, returns a C/C++ "void".
    MOZ_ASSERT(builtin.retType != MIRType::None);
    pushReturnValueOfCall(baselineCall, builtin.retType);
  }
  return true;
}

bool BaseCompiler::emitMemoryGrow() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing arg;
  if (!iter_.readMemoryGrow(&arg)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  return emitInstanceCall(lineOrBytecode, SASigMemoryGrow);
}

bool BaseCompiler::emitMemorySize() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  if (!iter_.readMemorySize()) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  return emitInstanceCall(lineOrBytecode, SASigMemorySize);
}

bool BaseCompiler::emitRefNull() {
  if (!iter_.readRefNull()) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  pushRef(NULLREF_VALUE);
  return true;
}

void BaseCompiler::emitRefIsNull() {
  RegPtr r = popRef();
  RegI32 rd = narrowPtr(r);

  masm.cmpPtrSet(Assembler::Equal, r, ImmWord(NULLREF_VALUE), rd);
  pushI32(rd);
}

bool BaseCompiler::emitAtomicCmpXchg(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused;

  if (!iter_.readAtomicCmpXchg(&addr, type, Scalar::byteSize(viewType), &unused,
                               &unused)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Full());

  if (Scalar::byteSize(viewType) <= 4) {
    PopAtomicCmpXchg32Regs regs(this, type, viewType);

    AccessCheck check;
    RegI32 rp = popMemoryAccess(&access, &check);
    RegI32 tls = maybeLoadTlsForAccess(check);

    auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
    regs.atomicCmpXchg32(access, memaddr);

    maybeFreeI32(tls);
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
  RegI32 rp = popMemoryAccess(&access, &check);

#ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);
  auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
  regs.atomicCmpXchg64(access, memaddr, ebx);
#else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
  regs.atomicCmpXchg64(access, memaddr);
  maybeFreeI32(tls);
#endif

  freeI32(rp);

  pushI64(regs.takeRd());
  return true;
}

bool BaseCompiler::emitAtomicLoad(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readAtomicLoad(&addr, type, Scalar::byteSize(viewType))) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Load());

  if (Scalar::byteSize(viewType) <= sizeof(void*)) {
    return loadCommon(&access, type);
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

#if defined(JS_64BIT)
  MOZ_CRASH("Should not happen");
#else
  PopAtomicLoad64Regs regs(this);

  AccessCheck check;
  RegI32 rp = popMemoryAccess(&access, &check);

#  ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);
  auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
  regs.atomicLoad64(access, memaddr, ebx);
#  else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
  regs.atomicLoad64(access, memaddr);
  maybeFreeI32(tls);
#  endif

  freeI32(rp);

  pushI64(regs.takeRd());
  return true;
#endif  // JS_64BIT
}

bool BaseCompiler::emitAtomicRMW(ValType type, Scalar::Type viewType,
                                 AtomicOp op) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readAtomicRMW(&addr, type, Scalar::byteSize(viewType),
                           &unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Full());

  if (Scalar::byteSize(viewType) <= 4) {
    PopAtomicRMW32Regs regs(this, type, viewType, op);

    AccessCheck check;
    RegI32 rp = popMemoryAccess(&access, &check);
    RegI32 tls = maybeLoadTlsForAccess(check);

    auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
    regs.atomicRMW32(access, memaddr, op);

    maybeFreeI32(tls);
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
  RegI32 rp = popMemoryAccess(&access, &check);

#ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);

  fr.pushPtr(regs.valueHigh());
  fr.pushPtr(regs.valueLow());
  Address value(esp, 0);

  auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
  regs.atomicRMW64(access, memaddr, op, value, ebx);

  fr.popBytes(8);
#else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
  regs.atomicRMW64(access, memaddr, op);
  maybeFreeI32(tls);
#endif

  freeI32(rp);

  pushI64(regs.takeRd());
  return true;
}

bool BaseCompiler::emitAtomicStore(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readAtomicStore(&addr, type, Scalar::byteSize(viewType),
                             &unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Store());

  if (Scalar::byteSize(viewType) <= sizeof(void*)) {
    return storeCommon(&access, type);
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

#ifdef JS_64BIT
  MOZ_CRASH("Should not happen");
#else
  emitAtomicXchg64(&access, type, WantResult(false));
  return true;
#endif
}

bool BaseCompiler::emitAtomicXchg(ValType type, Scalar::Type viewType) {
  LinearMemoryAddress<Nothing> addr;
  Nothing unused_value;
  if (!iter_.readAtomicRMW(&addr, type, Scalar::byteSize(viewType),
                           &unused_value)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  AccessCheck check;
  MemoryAccessDesc access(viewType, addr.align, addr.offset, bytecodeOffset(),
                          Synchronization::Full());

  if (Scalar::byteSize(viewType) <= 4) {
    PopAtomicXchg32Regs regs(this, type, viewType);
    RegI32 rp = popMemoryAccess(&access, &check);
    RegI32 tls = maybeLoadTlsForAccess(check);

    auto memaddr = prepareAtomicMemoryAccess(&access, &check, tls, rp);
    regs.atomicXchg32(access, memaddr);

    maybeFreeI32(tls);
    freeI32(rp);

    if (type == ValType::I64) {
      pushU32AsI64(regs.takeRd());
    } else {
      pushI32(regs.takeRd());
    }
    return true;
  }

  MOZ_ASSERT(type == ValType::I64 && Scalar::byteSize(viewType) == 8);

  emitAtomicXchg64(&access, type, WantResult(true));
  return true;
}

void BaseCompiler::emitAtomicXchg64(MemoryAccessDesc* access, ValType type,
                                    WantResult wantResult) {
  PopAtomicXchg64Regs regs(this);

  AccessCheck check;
  RegI32 rp = popMemoryAccess(access, &check);

#ifdef JS_CODEGEN_X86
  ScratchEBX ebx(*this);
  RegI32 tls = maybeLoadTlsForAccess(check, ebx);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicXchg64(*access, memaddr, ebx);
#else
  RegI32 tls = maybeLoadTlsForAccess(check);
  auto memaddr = prepareAtomicMemoryAccess(access, &check, tls, rp);
  regs.atomicXchg64(*access, memaddr);
  maybeFreeI32(tls);
#endif

  freeI32(rp);

  if (wantResult) {
    pushI64(regs.takeRd());
  }
}

bool BaseCompiler::emitWait(ValType type, uint32_t byteSize) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing nothing;
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readWait(&addr, type, byteSize, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  switch (type.code()) {
    case ValType::I32:
      if (!emitInstanceCall(lineOrBytecode, SASigWaitI32)) {
        return false;
      }
      break;
    case ValType::I64:
      if (!emitInstanceCall(lineOrBytecode, SASigWaitI64)) {
        return false;
      }
      break;
    default:
      MOZ_CRASH();
  }

  return true;
}

bool BaseCompiler::emitWake() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing nothing;
  LinearMemoryAddress<Nothing> addr;
  if (!iter_.readWake(&addr, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  return emitInstanceCall(lineOrBytecode, SASigWake);
}

#ifdef ENABLE_WASM_BULKMEM_OPS
bool BaseCompiler::emitMemOrTableCopy(bool isMem) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t dstMemOrTableIndex = 0;
  uint32_t srcMemOrTableIndex = 0;
  Nothing nothing;
  if (!iter_.readMemOrTableCopy(isMem, &dstMemOrTableIndex, &nothing,
                                &srcMemOrTableIndex, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  if (isMem) {
    MOZ_ASSERT(srcMemOrTableIndex == 0);
    MOZ_ASSERT(dstMemOrTableIndex == 0);
    if (!emitInstanceCall(lineOrBytecode, SASigMemCopy,
                          /*pushReturnedValue=*/false)) {
      return false;
    }
  } else {
    pushI32(dstMemOrTableIndex);
    pushI32(srcMemOrTableIndex);
    if (!emitInstanceCall(lineOrBytecode, SASigTableCopy,
                          /*pushReturnedValue=*/false)) {
      return false;
    }
  }

  return true;
}

bool BaseCompiler::emitDataOrElemDrop(bool isData) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t segIndex = 0;
  if (!iter_.readDataOrElemDrop(isData, &segIndex)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // Despite the cast to int32_t, the callee regards the value as unsigned.
  pushI32(int32_t(segIndex));

  return emitInstanceCall(lineOrBytecode,
                          isData ? SASigDataDrop : SASigElemDrop,
                          /*pushReturnedValue=*/false);
}

bool BaseCompiler::emitMemFill() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing nothing;
  if (!iter_.readMemFill(&nothing, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  return emitInstanceCall(lineOrBytecode, SASigMemFill,
                          /*pushReturnedValue=*/false);
}

bool BaseCompiler::emitMemOrTableInit(bool isMem) {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t segIndex = 0;
  uint32_t dstTableIndex = 0;
  Nothing nothing;
  if (!iter_.readMemOrTableInit(isMem, &segIndex, &dstTableIndex, &nothing,
                                &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  pushI32(int32_t(segIndex));
  if (isMem) {
    if (!emitInstanceCall(lineOrBytecode, SASigMemInit,
                          /*pushReturnedValue=*/false)) {
      return false;
    }
  } else {
    pushI32(dstTableIndex);
    if (!emitInstanceCall(lineOrBytecode, SASigTableInit,
                          /*pushReturnedValue=*/false)) {
      return false;
    }
  }

  return true;
}
#endif

MOZ_MUST_USE
bool BaseCompiler::emitTableFill() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  Nothing nothing;
  uint32_t tableIndex;
  if (!iter_.readTableFill(&tableIndex, &nothing, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // fill(start:u32, val:ref, len:u32, table:u32) -> u32
  pushI32(tableIndex);
  return emitInstanceCall(lineOrBytecode, SASigTableFill,
                          /*pushReturnedValue=*/false);
}

MOZ_MUST_USE
bool BaseCompiler::emitTableGet() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  Nothing index;
  uint32_t tableIndex;
  if (!iter_.readTableGet(&tableIndex, &index)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  // get(index:u32, table:u32) -> uintptr_t(AnyRef)
  pushI32(tableIndex);
  if (!emitInstanceCall(lineOrBytecode, SASigTableGet,
                        /*pushReturnedValue=*/false)) {
    return false;
  }

  // Push the resulting anyref back on the eval stack.  NOTE: needRef() must
  // not kill the value in the register.
  RegPtr r = RegPtr(ReturnReg);
  needRef(r);
  pushRef(r);

  return true;
}

MOZ_MUST_USE
bool BaseCompiler::emitTableGrow() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  Nothing delta;
  Nothing initValue;
  uint32_t tableIndex;
  if (!iter_.readTableGrow(&tableIndex, &initValue, &delta)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  // grow(initValue:anyref, delta:u32, table:u32) -> u32
  pushI32(tableIndex);
  return emitInstanceCall(lineOrBytecode, SASigTableGrow);
}

MOZ_MUST_USE
bool BaseCompiler::emitTableSet() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  Nothing index, value;
  uint32_t tableIndex;
  if (!iter_.readTableSet(&tableIndex, &index, &value)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  // set(index:u32, value:ref, table:u32) -> i32
  pushI32(tableIndex);
  return emitInstanceCall(lineOrBytecode, SASigTableSet,
                          /*pushReturnedValue=*/false);
}

MOZ_MUST_USE
bool BaseCompiler::emitTableSize() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();
  uint32_t tableIndex;
  if (!iter_.readTableSize(&tableIndex)) {
    return false;
  }
  if (deadCode_) {
    return true;
  }
  // size(table:u32) -> u32
  pushI32(tableIndex);
  return emitInstanceCall(lineOrBytecode, SASigTableSize);
}

bool BaseCompiler::emitStructNew() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  uint32_t typeIndex;
  BaseOpIter::ValueVector args;
  if (!iter_.readStructNew(&typeIndex, &args)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // Allocate zeroed storage.  The parameter to StructNew is an index into a
  // descriptor table that the instance has.
  //
  // Returns null on OOM.

  const StructType& structType = env_.types[typeIndex].structType();

  pushI32(structType.moduleIndex_);
  if (!emitInstanceCall(lineOrBytecode, SASigStructNew)) {
    return false;
  }

  // As many arguments as there are fields.

  MOZ_ASSERT(args.length() == structType.fields_.length());

  // Optimization opportunity: Iterate backward to pop arguments off the
  // stack.  This will generate more instructions than we want, since we
  // really only need to pop the stack once at the end, not for every element,
  // but to do better we need a bit more machinery to load elements off the
  // stack into registers.

  RegPtr rp = popRef();
  RegPtr rdata = rp;

  if (!structType.isInline_) {
    rdata = needRef();
    masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rdata);
  }

  // Optimization opportunity: when the value being stored is a known
  // zero/null we need store nothing.  This case may be somewhat common
  // because struct.new forces a value to be specified for every field.

  uint32_t fieldNo = structType.fields_.length();
  while (fieldNo-- > 0) {
    uint32_t offs = structType.fields_[fieldNo].offset;
    switch (structType.fields_[fieldNo].type.code()) {
      case ValType::I32: {
        RegI32 r = popI32();
        masm.store32(r, Address(rdata, offs));
        freeI32(r);
        break;
      }
      case ValType::I64: {
        RegI64 r = popI64();
        masm.store64(r, Address(rdata, offs));
        freeI64(r);
        break;
      }
      case ValType::F32: {
        RegF32 r = popF32();
        masm.storeFloat32(r, Address(rdata, offs));
        freeF32(r);
        break;
      }
      case ValType::F64: {
        RegF64 r = popF64();
        masm.storeDouble(r, Address(rdata, offs));
        freeF64(r);
        break;
      }
      case ValType::Ref:
      case ValType::FuncRef:
      case ValType::AnyRef: {
        RegPtr value = popRef();
        masm.storePtr(value, Address(rdata, offs));

        // A write barrier is needed here for the extremely unlikely case
        // that the object is allocated in the tenured area - a result of
        // a GC artifact.

        Label skipBarrier;

        sync();

        RegPtr rowner = rp;
        if (!structType.isInline_) {
          rowner = needRef();
          masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfOwner()),
                       rowner);
        }

        RegPtr otherScratch = needRef();
        EmitWasmPostBarrierGuard(masm, Some(rowner), otherScratch, value,
                                 &skipBarrier);
        freeRef(otherScratch);

        if (!structType.isInline_) {
          freeRef(rowner);
        }

        freeRef(value);

        // TODO/AnyRef-boxing: With boxed immediates and strings, the write
        // barrier is going to have to be more complicated.
        ASSERT_ANYREF_IS_JSOBJECT;

        pushRef(rp);  // Save rp across the call
        RegPtr valueAddr = needRef();
        masm.computeEffectiveAddress(Address(rdata, offs), valueAddr);
        if (!emitPostBarrierCall(valueAddr)) {  // Consumes valueAddr
          return false;
        }
        popRef(rp);  // Restore rp
        if (!structType.isInline_) {
          masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rdata);
        }

        masm.bind(&skipBarrier);
        break;
      }
      case ValType::NullRef:
        MOZ_CRASH("NullRef not expressible");
      default: {
        MOZ_CRASH("Unexpected field type");
      }
    }
  }

  if (!structType.isInline_) {
    freeRef(rdata);
  }

  pushRef(rp);

  return true;
}

bool BaseCompiler::emitStructGet() {
  uint32_t typeIndex;
  uint32_t fieldIndex;
  Nothing nothing;
  if (!iter_.readStructGet(&typeIndex, &fieldIndex, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const StructType& structType = env_.types[typeIndex].structType();

  RegPtr rp = popRef();

  Label ok;
  masm.branchTestPtr(Assembler::NonZero, rp, rp, &ok);
  trap(Trap::NullPointerDereference);
  masm.bind(&ok);

  if (!structType.isInline_) {
    masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rp);
  }

  uint32_t offs = structType.fields_[fieldIndex].offset;
  switch (structType.fields_[fieldIndex].type.code()) {
    case ValType::I32: {
      RegI32 r = needI32();
      masm.load32(Address(rp, offs), r);
      pushI32(r);
      break;
    }
    case ValType::I64: {
      RegI64 r = needI64();
      masm.load64(Address(rp, offs), r);
      pushI64(r);
      break;
    }
    case ValType::F32: {
      RegF32 r = needF32();
      masm.loadFloat32(Address(rp, offs), r);
      pushF32(r);
      break;
    }
    case ValType::F64: {
      RegF64 r = needF64();
      masm.loadDouble(Address(rp, offs), r);
      pushF64(r);
      break;
    }
    case ValType::Ref:
    case ValType::FuncRef:
    case ValType::AnyRef: {
      RegPtr r = needRef();
      masm.loadPtr(Address(rp, offs), r);
      pushRef(r);
      break;
    }
    case ValType::NullRef: {
      MOZ_CRASH("NullRef not expressible");
    }
    default: {
      MOZ_CRASH("Unexpected field type");
    }
  }

  freeRef(rp);

  return true;
}

bool BaseCompiler::emitStructSet() {
  uint32_t typeIndex;
  uint32_t fieldIndex;
  Nothing nothing;
  if (!iter_.readStructSet(&typeIndex, &fieldIndex, &nothing, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  const StructType& structType = env_.types[typeIndex].structType();

  RegI32 ri;
  RegI64 rl;
  RegF32 rf;
  RegF64 rd;
  RegPtr rr;

  // Reserve this register early if we will need it so that it is not taken by
  // rr or rp.
  RegPtr valueAddr;
  if (structType.fields_[fieldIndex].type.isReference()) {
    valueAddr = RegPtr(PreBarrierReg);
    needRef(valueAddr);
  }

  switch (structType.fields_[fieldIndex].type.code()) {
    case ValType::I32:
      ri = popI32();
      break;
    case ValType::I64:
      rl = popI64();
      break;
    case ValType::F32:
      rf = popF32();
      break;
    case ValType::F64:
      rd = popF64();
      break;
    case ValType::Ref:
    case ValType::FuncRef:
    case ValType::AnyRef:
      rr = popRef();
      break;
    case ValType::NullRef:
      MOZ_CRASH("NullRef not expressible");
    default:
      MOZ_CRASH("Unexpected field type");
  }

  RegPtr rp = popRef();

  Label ok;
  masm.branchTestPtr(Assembler::NonZero, rp, rp, &ok);
  trap(Trap::NullPointerDereference);
  masm.bind(&ok);

  if (!structType.isInline_) {
    masm.loadPtr(Address(rp, OutlineTypedObject::offsetOfData()), rp);
  }

  uint32_t offs = structType.fields_[fieldIndex].offset;
  switch (structType.fields_[fieldIndex].type.code()) {
    case ValType::I32: {
      masm.store32(ri, Address(rp, offs));
      freeI32(ri);
      break;
    }
    case ValType::I64: {
      masm.store64(rl, Address(rp, offs));
      freeI64(rl);
      break;
    }
    case ValType::F32: {
      masm.storeFloat32(rf, Address(rp, offs));
      freeF32(rf);
      break;
    }
    case ValType::F64: {
      masm.storeDouble(rd, Address(rp, offs));
      freeF64(rd);
      break;
    }
    case ValType::Ref:
    case ValType::FuncRef:
    case ValType::AnyRef: {
      masm.computeEffectiveAddress(Address(rp, offs), valueAddr);
      // emitBarrieredStore consumes valueAddr
      if (!emitBarrieredStore(Some(rp), valueAddr, rr,
                              structType.fields_[fieldIndex].type)) {
        return false;
      }
      freeRef(rr);
      break;
    }
    case ValType::NullRef: {
      MOZ_CRASH("NullRef not expressible");
    }
    default: {
      MOZ_CRASH("Unexpected field type");
    }
  }

  freeRef(rp);

  return true;
}

bool BaseCompiler::emitStructNarrow() {
  uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

  ValType inputType, outputType;
  Nothing nothing;
  if (!iter_.readStructNarrow(&inputType, &outputType, &nothing)) {
    return false;
  }

  if (deadCode_) {
    return true;
  }

  // Currently not supported by struct.narrow validation.
  MOZ_ASSERT(inputType != ValType::FuncRef);
  MOZ_ASSERT(outputType != ValType::FuncRef);

  // AnyRef -> AnyRef is a no-op, just leave the value on the stack.

  if (inputType == ValType::AnyRef && outputType == ValType::AnyRef) {
    return true;
  }

  RegPtr rp = popRef();

  // AnyRef -> (ref T) must first unbox; leaves rp or null

  bool mustUnboxAnyref = inputType == ValType::AnyRef;

  // Dynamic downcast (ref T) -> (ref U), leaves rp or null
  const StructType& outputStruct =
      env_.types[outputType.refTypeIndex()].structType();

  pushI32(mustUnboxAnyref);
  pushI32(outputStruct.moduleIndex_);
  pushRef(rp);
  return emitInstanceCall(lineOrBytecode, SASigStructNarrow);
}

bool BaseCompiler::emitBody() {
  MOZ_ASSERT(stackMapGenerator_.framePushedAtEntryToBody.isSome());

  if (!iter_.readFunctionStart(funcType().ret())) {
    return false;
  }

  initControl(controlItem());

  uint32_t overhead = 0;

  for (;;) {
    Nothing unused_a, unused_b;

#ifdef DEBUG
    performRegisterLeakCheck();
    assertStackInvariants();
#endif

#define emitBinary(doEmit, type)                  \
  iter_.readBinary(type, &unused_a, &unused_b) && \
      (deadCode_ || (doEmit(), true))

#define emitUnary(doEmit, type) \
  iter_.readUnary(type, &unused_a) && (deadCode_ || (doEmit(), true))

#define emitComparison(doEmit, operandType, compareOp)       \
  iter_.readComparison(operandType, &unused_a, &unused_b) && \
      (deadCode_ || (doEmit(compareOp, operandType), true))

#define emitConversion(doEmit, inType, outType)       \
  iter_.readConversion(inType, outType, &unused_a) && \
      (deadCode_ || (doEmit(), true))

#define emitConversionOOM(doEmit, inType, outType) \
  iter_.readConversion(inType, outType, &unused_a) && (deadCode_ || doEmit())

#define emitCalloutConversionOOM(doEmit, symbol, inType, outType) \
  iter_.readConversion(inType, outType, &unused_a) &&             \
      (deadCode_ || doEmit(symbol, inType, outType))

#define emitIntDivCallout(doEmit, symbol, type)   \
  iter_.readBinary(type, &unused_a, &unused_b) && \
      (deadCode_ || doEmit(symbol, type))

#ifdef DEBUG
    // Check that the number of ref-typed entries in the operand stack matches
    // reality.
#  define CHECK_POINTER_COUNT                                             \
    do {                                                                  \
      MOZ_ASSERT(countMemRefsOnStk() == stackMapGenerator_.memRefsOnStk); \
    } while (0)
#else
#  define CHECK_POINTER_COUNT \
    do {                      \
    } while (0)
#endif

#define CHECK(E) \
  if (!(E)) return false
#define NEXT()           \
  {                      \
    CHECK_POINTER_COUNT; \
    continue;            \
  }
#define CHECK_NEXT(E)     \
  if (!(E)) return false; \
  {                       \
    CHECK_POINTER_COUNT;  \
    continue;             \
  }

    // TODO / EVALUATE (bug 1316845): Not obvious that this attempt at
    // reducing overhead is really paying off relative to making the check
    // every iteration.

    if (overhead == 0) {
      // Check every 50 expressions -- a happy medium between
      // memory usage and checking overhead.
      overhead = 50;

      // Checking every 50 expressions should be safe, as the
      // baseline JIT does very little allocation per expression.
      CHECK(alloc_.ensureBallast());

      // The pushiest opcode is LOOP, which pushes two values
      // per instance.
      CHECK(stk_.reserve(stk_.length() + overhead * 2));
    }

    overhead--;

    OpBytes op;
    CHECK(iter_.readOp(&op));

    // When env_.debugEnabled(), every operator has breakpoint site but Op::End.
    if (env_.debugEnabled() && op.b0 != (uint16_t)Op::End) {
      // TODO sync only registers that can be clobbered by the exit
      // prologue/epilogue or disable these registers for use in
      // baseline compiler when env_.debugEnabled() is set.
      sync();

      insertBreakablePoint(CallSiteDesc::Breakpoint);
      if (!createStackMap("debug: per insn")) {
        return false;
      }
    }

    // Going below framePushedAtEntryToBody would imply that we've
    // popped off the machine stack, part of the frame created by
    // beginFunction().
    MOZ_ASSERT(masm.framePushed() >=
               stackMapGenerator_.framePushedAtEntryToBody.value());

    // At this point we're definitely not generating code for a function call.
    MOZ_ASSERT(
        stackMapGenerator_.framePushedExcludingOutboundCallArgs.isNothing());

    switch (op.b0) {
      case uint16_t(Op::End):
        if (!emitEnd()) {
          return false;
        }
        if (iter_.controlStackEmpty()) {
          return true;
        }
        NEXT();

      // Control opcodes
      case uint16_t(Op::Nop):
        CHECK_NEXT(iter_.readNop());
      case uint16_t(Op::Drop):
        CHECK_NEXT(emitDrop());
      case uint16_t(Op::Block):
        CHECK_NEXT(emitBlock());
      case uint16_t(Op::Loop):
        CHECK_NEXT(emitLoop());
      case uint16_t(Op::If):
        CHECK_NEXT(emitIf());
      case uint16_t(Op::Else):
        CHECK_NEXT(emitElse());
      case uint16_t(Op::Br):
        CHECK_NEXT(emitBr());
      case uint16_t(Op::BrIf):
        CHECK_NEXT(emitBrIf());
      case uint16_t(Op::BrTable):
        CHECK_NEXT(emitBrTable());
      case uint16_t(Op::Return):
        CHECK_NEXT(emitReturn());
      case uint16_t(Op::Unreachable):
        CHECK(iter_.readUnreachable());
        if (!deadCode_) {
          trap(Trap::Unreachable);
          deadCode_ = true;
        }
        NEXT();

      // Calls
      case uint16_t(Op::Call):
        CHECK_NEXT(emitCall());
      case uint16_t(Op::CallIndirect):
        CHECK_NEXT(emitCallIndirect());

      // Locals and globals
      case uint16_t(Op::GetLocal):
        CHECK_NEXT(emitGetLocal());
      case uint16_t(Op::SetLocal):
        CHECK_NEXT(emitSetLocal());
      case uint16_t(Op::TeeLocal):
        CHECK_NEXT(emitTeeLocal());
      case uint16_t(Op::GetGlobal):
        CHECK_NEXT(emitGetGlobal());
      case uint16_t(Op::SetGlobal):
        CHECK_NEXT(emitSetGlobal());
#ifdef ENABLE_WASM_REFTYPES
      case uint16_t(Op::TableGet):
        CHECK_NEXT(emitTableGet());
      case uint16_t(Op::TableSet):
        CHECK_NEXT(emitTableSet());
#endif

      // Select
      case uint16_t(Op::Select):
        CHECK_NEXT(emitSelect());

      // I32
      case uint16_t(Op::I32Const): {
        int32_t i32;
        CHECK(iter_.readI32Const(&i32));
        if (!deadCode_) {
          pushI32(i32);
        }
        NEXT();
      }
      case uint16_t(Op::I32Add):
        CHECK_NEXT(emitBinary(emitAddI32, ValType::I32));
      case uint16_t(Op::I32Sub):
        CHECK_NEXT(emitBinary(emitSubtractI32, ValType::I32));
      case uint16_t(Op::I32Mul):
        CHECK_NEXT(emitBinary(emitMultiplyI32, ValType::I32));
      case uint16_t(Op::I32DivS):
        CHECK_NEXT(emitBinary(emitQuotientI32, ValType::I32));
      case uint16_t(Op::I32DivU):
        CHECK_NEXT(emitBinary(emitQuotientU32, ValType::I32));
      case uint16_t(Op::I32RemS):
        CHECK_NEXT(emitBinary(emitRemainderI32, ValType::I32));
      case uint16_t(Op::I32RemU):
        CHECK_NEXT(emitBinary(emitRemainderU32, ValType::I32));
      case uint16_t(Op::I32Eqz):
        CHECK_NEXT(emitConversion(emitEqzI32, ValType::I32, ValType::I32));
      case uint16_t(Op::I32TruncSF32):
        CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI32<0>, ValType::F32,
                                     ValType::I32));
      case uint16_t(Op::I32TruncUF32):
        CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI32<TRUNC_UNSIGNED>,
                                     ValType::F32, ValType::I32));
      case uint16_t(Op::I32TruncSF64):
        CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI32<0>, ValType::F64,
                                     ValType::I32));
      case uint16_t(Op::I32TruncUF64):
        CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI32<TRUNC_UNSIGNED>,
                                     ValType::F64, ValType::I32));
      case uint16_t(Op::I32WrapI64):
        CHECK_NEXT(
            emitConversion(emitWrapI64ToI32, ValType::I64, ValType::I32));
      case uint16_t(Op::I32ReinterpretF32):
        CHECK_NEXT(emitConversion(emitReinterpretF32AsI32, ValType::F32,
                                  ValType::I32));
      case uint16_t(Op::I32Clz):
        CHECK_NEXT(emitUnary(emitClzI32, ValType::I32));
      case uint16_t(Op::I32Ctz):
        CHECK_NEXT(emitUnary(emitCtzI32, ValType::I32));
      case uint16_t(Op::I32Popcnt):
        CHECK_NEXT(emitUnary(emitPopcntI32, ValType::I32));
      case uint16_t(Op::I32Or):
        CHECK_NEXT(emitBinary(emitOrI32, ValType::I32));
      case uint16_t(Op::I32And):
        CHECK_NEXT(emitBinary(emitAndI32, ValType::I32));
      case uint16_t(Op::I32Xor):
        CHECK_NEXT(emitBinary(emitXorI32, ValType::I32));
      case uint16_t(Op::I32Shl):
        CHECK_NEXT(emitBinary(emitShlI32, ValType::I32));
      case uint16_t(Op::I32ShrS):
        CHECK_NEXT(emitBinary(emitShrI32, ValType::I32));
      case uint16_t(Op::I32ShrU):
        CHECK_NEXT(emitBinary(emitShrU32, ValType::I32));
      case uint16_t(Op::I32Load8S):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int8));
      case uint16_t(Op::I32Load8U):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Uint8));
      case uint16_t(Op::I32Load16S):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int16));
      case uint16_t(Op::I32Load16U):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Uint16));
      case uint16_t(Op::I32Load):
        CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int32));
      case uint16_t(Op::I32Store8):
        CHECK_NEXT(emitStore(ValType::I32, Scalar::Int8));
      case uint16_t(Op::I32Store16):
        CHECK_NEXT(emitStore(ValType::I32, Scalar::Int16));
      case uint16_t(Op::I32Store):
        CHECK_NEXT(emitStore(ValType::I32, Scalar::Int32));
      case uint16_t(Op::I32Rotr):
        CHECK_NEXT(emitBinary(emitRotrI32, ValType::I32));
      case uint16_t(Op::I32Rotl):
        CHECK_NEXT(emitBinary(emitRotlI32, ValType::I32));

      // I64
      case uint16_t(Op::I64Const): {
        int64_t i64;
        CHECK(iter_.readI64Const(&i64));
        if (!deadCode_) {
          pushI64(i64);
        }
        NEXT();
      }
      case uint16_t(Op::I64Add):
        CHECK_NEXT(emitBinary(emitAddI64, ValType::I64));
      case uint16_t(Op::I64Sub):
        CHECK_NEXT(emitBinary(emitSubtractI64, ValType::I64));
      case uint16_t(Op::I64Mul):
        CHECK_NEXT(emitBinary(emitMultiplyI64, ValType::I64));
      case uint16_t(Op::I64DivS):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(emitIntDivCallout(emitDivOrModI64BuiltinCall,
                                     SymbolicAddress::DivI64, ValType::I64));
#else
        CHECK_NEXT(emitBinary(emitQuotientI64, ValType::I64));
#endif
      case uint16_t(Op::I64DivU):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(emitIntDivCallout(emitDivOrModI64BuiltinCall,
                                     SymbolicAddress::UDivI64, ValType::I64));
#else
        CHECK_NEXT(emitBinary(emitQuotientU64, ValType::I64));
#endif
      case uint16_t(Op::I64RemS):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(emitIntDivCallout(emitDivOrModI64BuiltinCall,
                                     SymbolicAddress::ModI64, ValType::I64));
#else
        CHECK_NEXT(emitBinary(emitRemainderI64, ValType::I64));
#endif
      case uint16_t(Op::I64RemU):
#ifdef RABALDR_INT_DIV_I64_CALLOUT
        CHECK_NEXT(emitIntDivCallout(emitDivOrModI64BuiltinCall,
                                     SymbolicAddress::UModI64, ValType::I64));
#else
        CHECK_NEXT(emitBinary(emitRemainderU64, ValType::I64));
#endif
      case uint16_t(Op::I64TruncSF32):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(
            emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                     SymbolicAddress::TruncateDoubleToInt64,
                                     ValType::F32, ValType::I64));
#else
        CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI64<0>, ValType::F32,
                                     ValType::I64));
#endif
      case uint16_t(Op::I64TruncUF32):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(
            emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                     SymbolicAddress::TruncateDoubleToUint64,
                                     ValType::F32, ValType::I64));
#else
        CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI64<TRUNC_UNSIGNED>,
                                     ValType::F32, ValType::I64));
#endif
      case uint16_t(Op::I64TruncSF64):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(
            emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                     SymbolicAddress::TruncateDoubleToInt64,
                                     ValType::F64, ValType::I64));
#else
        CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI64<0>, ValType::F64,
                                     ValType::I64));
#endif
      case uint16_t(Op::I64TruncUF64):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
        CHECK_NEXT(
            emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                     SymbolicAddress::TruncateDoubleToUint64,
                                     ValType::F64, ValType::I64));
#else
        CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI64<TRUNC_UNSIGNED>,
                                     ValType::F64, ValType::I64));
#endif
      case uint16_t(Op::I64ExtendSI32):
        CHECK_NEXT(
            emitConversion(emitExtendI32ToI64, ValType::I32, ValType::I64));
      case uint16_t(Op::I64ExtendUI32):
        CHECK_NEXT(
            emitConversion(emitExtendU32ToI64, ValType::I32, ValType::I64));
      case uint16_t(Op::I64ReinterpretF64):
        CHECK_NEXT(emitConversion(emitReinterpretF64AsI64, ValType::F64,
                                  ValType::I64));
      case uint16_t(Op::I64Or):
        CHECK_NEXT(emitBinary(emitOrI64, ValType::I64));
      case uint16_t(Op::I64And):
        CHECK_NEXT(emitBinary(emitAndI64, ValType::I64));
      case uint16_t(Op::I64Xor):
        CHECK_NEXT(emitBinary(emitXorI64, ValType::I64));
      case uint16_t(Op::I64Shl):
        CHECK_NEXT(emitBinary(emitShlI64, ValType::I64));
      case uint16_t(Op::I64ShrS):
        CHECK_NEXT(emitBinary(emitShrI64, ValType::I64));
      case uint16_t(Op::I64ShrU):
        CHECK_NEXT(emitBinary(emitShrU64, ValType::I64));
      case uint16_t(Op::I64Rotr):
        CHECK_NEXT(emitBinary(emitRotrI64, ValType::I64));
      case uint16_t(Op::I64Rotl):
        CHECK_NEXT(emitBinary(emitRotlI64, ValType::I64));
      case uint16_t(Op::I64Clz):
        CHECK_NEXT(emitUnary(emitClzI64, ValType::I64));
      case uint16_t(Op::I64Ctz):
        CHECK_NEXT(emitUnary(emitCtzI64, ValType::I64));
      case uint16_t(Op::I64Popcnt):
        CHECK_NEXT(emitUnary(emitPopcntI64, ValType::I64));
      case uint16_t(Op::I64Eqz):
        CHECK_NEXT(emitConversion(emitEqzI64, ValType::I64, ValType::I32));
      case uint16_t(Op::I64Load8S):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int8));
      case uint16_t(Op::I64Load16S):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int16));
      case uint16_t(Op::I64Load32S):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int32));
      case uint16_t(Op::I64Load8U):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint8));
      case uint16_t(Op::I64Load16U):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint16));
      case uint16_t(Op::I64Load32U):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint32));
      case uint16_t(Op::I64Load):
        CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int64));
      case uint16_t(Op::I64Store8):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int8));
      case uint16_t(Op::I64Store16):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int16));
      case uint16_t(Op::I64Store32):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int32));
      case uint16_t(Op::I64Store):
        CHECK_NEXT(emitStore(ValType::I64, Scalar::Int64));

      // F32
      case uint16_t(Op::F32Const): {
        float f32;
        CHECK(iter_.readF32Const(&f32));
        if (!deadCode_) {
          pushF32(f32);
        }
        NEXT();
      }
      case uint16_t(Op::F32Add):
        CHECK_NEXT(emitBinary(emitAddF32, ValType::F32));
      case uint16_t(Op::F32Sub):
        CHECK_NEXT(emitBinary(emitSubtractF32, ValType::F32));
      case uint16_t(Op::F32Mul):
        CHECK_NEXT(emitBinary(emitMultiplyF32, ValType::F32));
      case uint16_t(Op::F32Div):
        CHECK_NEXT(emitBinary(emitDivideF32, ValType::F32));
      case uint16_t(Op::F32Min):
        CHECK_NEXT(emitBinary(emitMinF32, ValType::F32));
      case uint16_t(Op::F32Max):
        CHECK_NEXT(emitBinary(emitMaxF32, ValType::F32));
      case uint16_t(Op::F32Neg):
        CHECK_NEXT(emitUnary(emitNegateF32, ValType::F32));
      case uint16_t(Op::F32Abs):
        CHECK_NEXT(emitUnary(emitAbsF32, ValType::F32));
      case uint16_t(Op::F32Sqrt):
        CHECK_NEXT(emitUnary(emitSqrtF32, ValType::F32));
      case uint16_t(Op::F32Ceil):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::CeilF, ValType::F32));
      case uint16_t(Op::F32Floor):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::FloorF, ValType::F32));
      case uint16_t(Op::F32DemoteF64):
        CHECK_NEXT(
            emitConversion(emitConvertF64ToF32, ValType::F64, ValType::F32));
      case uint16_t(Op::F32ConvertSI32):
        CHECK_NEXT(
            emitConversion(emitConvertI32ToF32, ValType::I32, ValType::F32));
      case uint16_t(Op::F32ConvertUI32):
        CHECK_NEXT(
            emitConversion(emitConvertU32ToF32, ValType::I32, ValType::F32));
      case uint16_t(Op::F32ConvertSI64):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                            SymbolicAddress::Int64ToFloat32,
                                            ValType::I64, ValType::F32));
#else
        CHECK_NEXT(
            emitConversion(emitConvertI64ToF32, ValType::I64, ValType::F32));
#endif
      case uint16_t(Op::F32ConvertUI64):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                            SymbolicAddress::Uint64ToFloat32,
                                            ValType::I64, ValType::F32));
#else
        CHECK_NEXT(
            emitConversion(emitConvertU64ToF32, ValType::I64, ValType::F32));
#endif
      case uint16_t(Op::F32ReinterpretI32):
        CHECK_NEXT(emitConversion(emitReinterpretI32AsF32, ValType::I32,
                                  ValType::F32));
      case uint16_t(Op::F32Load):
        CHECK_NEXT(emitLoad(ValType::F32, Scalar::Float32));
      case uint16_t(Op::F32Store):
        CHECK_NEXT(emitStore(ValType::F32, Scalar::Float32));
      case uint16_t(Op::F32CopySign):
        CHECK_NEXT(emitBinary(emitCopysignF32, ValType::F32));
      case uint16_t(Op::F32Nearest):
        CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::NearbyIntF,
                                            ValType::F32));
      case uint16_t(Op::F32Trunc):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::TruncF, ValType::F32));

      // F64
      case uint16_t(Op::F64Const): {
        double f64;
        CHECK(iter_.readF64Const(&f64));
        if (!deadCode_) {
          pushF64(f64);
        }
        NEXT();
      }
      case uint16_t(Op::F64Add):
        CHECK_NEXT(emitBinary(emitAddF64, ValType::F64));
      case uint16_t(Op::F64Sub):
        CHECK_NEXT(emitBinary(emitSubtractF64, ValType::F64));
      case uint16_t(Op::F64Mul):
        CHECK_NEXT(emitBinary(emitMultiplyF64, ValType::F64));
      case uint16_t(Op::F64Div):
        CHECK_NEXT(emitBinary(emitDivideF64, ValType::F64));
      case uint16_t(Op::F64Min):
        CHECK_NEXT(emitBinary(emitMinF64, ValType::F64));
      case uint16_t(Op::F64Max):
        CHECK_NEXT(emitBinary(emitMaxF64, ValType::F64));
      case uint16_t(Op::F64Neg):
        CHECK_NEXT(emitUnary(emitNegateF64, ValType::F64));
      case uint16_t(Op::F64Abs):
        CHECK_NEXT(emitUnary(emitAbsF64, ValType::F64));
      case uint16_t(Op::F64Sqrt):
        CHECK_NEXT(emitUnary(emitSqrtF64, ValType::F64));
      case uint16_t(Op::F64Ceil):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::CeilD, ValType::F64));
      case uint16_t(Op::F64Floor):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::FloorD, ValType::F64));
      case uint16_t(Op::F64PromoteF32):
        CHECK_NEXT(
            emitConversion(emitConvertF32ToF64, ValType::F32, ValType::F64));
      case uint16_t(Op::F64ConvertSI32):
        CHECK_NEXT(
            emitConversion(emitConvertI32ToF64, ValType::I32, ValType::F64));
      case uint16_t(Op::F64ConvertUI32):
        CHECK_NEXT(
            emitConversion(emitConvertU32ToF64, ValType::I32, ValType::F64));
      case uint16_t(Op::F64ConvertSI64):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                            SymbolicAddress::Int64ToDouble,
                                            ValType::I64, ValType::F64));
#else
        CHECK_NEXT(
            emitConversion(emitConvertI64ToF64, ValType::I64, ValType::F64));
#endif
      case uint16_t(Op::F64ConvertUI64):
#ifdef RABALDR_I64_TO_FLOAT_CALLOUT
        CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                            SymbolicAddress::Uint64ToDouble,
                                            ValType::I64, ValType::F64));
#else
        CHECK_NEXT(
            emitConversion(emitConvertU64ToF64, ValType::I64, ValType::F64));
#endif
      case uint16_t(Op::F64Load):
        CHECK_NEXT(emitLoad(ValType::F64, Scalar::Float64));
      case uint16_t(Op::F64Store):
        CHECK_NEXT(emitStore(ValType::F64, Scalar::Float64));
      case uint16_t(Op::F64ReinterpretI64):
        CHECK_NEXT(emitConversion(emitReinterpretI64AsF64, ValType::I64,
                                  ValType::F64));
      case uint16_t(Op::F64CopySign):
        CHECK_NEXT(emitBinary(emitCopysignF64, ValType::F64));
      case uint16_t(Op::F64Nearest):
        CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::NearbyIntD,
                                            ValType::F64));
      case uint16_t(Op::F64Trunc):
        CHECK_NEXT(
            emitUnaryMathBuiltinCall(SymbolicAddress::TruncD, ValType::F64));

      // Comparisons
      case uint16_t(Op::I32Eq):
        CHECK_NEXT(
            emitComparison(emitCompareI32, ValType::I32, Assembler::Equal));
      case uint16_t(Op::I32Ne):
        CHECK_NEXT(
            emitComparison(emitCompareI32, ValType::I32, Assembler::NotEqual));
      case uint16_t(Op::I32LtS):
        CHECK_NEXT(
            emitComparison(emitCompareI32, ValType::I32, Assembler::LessThan));
      case uint16_t(Op::I32LeS):
        CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32,
                                  Assembler::LessThanOrEqual));
      case uint16_t(Op::I32GtS):
        CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32,
                                  Assembler::GreaterThan));
      case uint16_t(Op::I32GeS):
        CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32,
                                  Assembler::GreaterThanOrEqual));
      case uint16_t(Op::I32LtU):
        CHECK_NEXT(
            emitComparison(emitCompareI32, ValType::I32, Assembler::Below));
      case uint16_t(Op::I32LeU):
        CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32,
                                  Assembler::BelowOrEqual));
      case uint16_t(Op::I32GtU):
        CHECK_NEXT(
            emitComparison(emitCompareI32, ValType::I32, Assembler::Above));
      case uint16_t(Op::I32GeU):
        CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32,
                                  Assembler::AboveOrEqual));
      case uint16_t(Op::I64Eq):
        CHECK_NEXT(
            emitComparison(emitCompareI64, ValType::I64, Assembler::Equal));
      case uint16_t(Op::I64Ne):
        CHECK_NEXT(
            emitComparison(emitCompareI64, ValType::I64, Assembler::NotEqual));
      case uint16_t(Op::I64LtS):
        CHECK_NEXT(
            emitComparison(emitCompareI64, ValType::I64, Assembler::LessThan));
      case uint16_t(Op::I64LeS):
        CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64,
                                  Assembler::LessThanOrEqual));
      case uint16_t(Op::I64GtS):
        CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64,
                                  Assembler::GreaterThan));
      case uint16_t(Op::I64GeS):
        CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64,
                                  Assembler::GreaterThanOrEqual));
      case uint16_t(Op::I64LtU):
        CHECK_NEXT(
            emitComparison(emitCompareI64, ValType::I64, Assembler::Below));
      case uint16_t(Op::I64LeU):
        CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64,
                                  Assembler::BelowOrEqual));
      case uint16_t(Op::I64GtU):
        CHECK_NEXT(
            emitComparison(emitCompareI64, ValType::I64, Assembler::Above));
      case uint16_t(Op::I64GeU):
        CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64,
                                  Assembler::AboveOrEqual));
      case uint16_t(Op::F32Eq):
        CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32,
                                  Assembler::DoubleEqual));
      case uint16_t(Op::F32Ne):
        CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32,
                                  Assembler::DoubleNotEqualOrUnordered));
      case uint16_t(Op::F32Lt):
        CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32,
                                  Assembler::DoubleLessThan));
      case uint16_t(Op::F32Le):
        CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32,
                                  Assembler::DoubleLessThanOrEqual));
      case uint16_t(Op::F32Gt):
        CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32,
                                  Assembler::DoubleGreaterThan));
      case uint16_t(Op::F32Ge):
        CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32,
                                  Assembler::DoubleGreaterThanOrEqual));
      case uint16_t(Op::F64Eq):
        CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64,
                                  Assembler::DoubleEqual));
      case uint16_t(Op::F64Ne):
        CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64,
                                  Assembler::DoubleNotEqualOrUnordered));
      case uint16_t(Op::F64Lt):
        CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64,
                                  Assembler::DoubleLessThan));
      case uint16_t(Op::F64Le):
        CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64,
                                  Assembler::DoubleLessThanOrEqual));
      case uint16_t(Op::F64Gt):
        CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64,
                                  Assembler::DoubleGreaterThan));
      case uint16_t(Op::F64Ge):
        CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64,
                                  Assembler::DoubleGreaterThanOrEqual));

      // Sign extensions
      case uint16_t(Op::I32Extend8S):
        CHECK_NEXT(emitConversion(emitExtendI32_8, ValType::I32, ValType::I32));
      case uint16_t(Op::I32Extend16S):
        CHECK_NEXT(
            emitConversion(emitExtendI32_16, ValType::I32, ValType::I32));
      case uint16_t(Op::I64Extend8S):
        CHECK_NEXT(emitConversion(emitExtendI64_8, ValType::I64, ValType::I64));
      case uint16_t(Op::I64Extend16S):
        CHECK_NEXT(
            emitConversion(emitExtendI64_16, ValType::I64, ValType::I64));
      case uint16_t(Op::I64Extend32S):
        CHECK_NEXT(
            emitConversion(emitExtendI64_32, ValType::I64, ValType::I64));

      // Memory Related
      case uint16_t(Op::MemoryGrow):
        CHECK_NEXT(emitMemoryGrow());
      case uint16_t(Op::MemorySize):
        CHECK_NEXT(emitMemorySize());

#ifdef ENABLE_WASM_GC
      case uint16_t(Op::RefEq):
        if (!env_.gcTypesEnabled()) {
          return iter_.unrecognizedOpcode(&op);
        }
        CHECK_NEXT(
            emitComparison(emitCompareRef, ValType::AnyRef, Assembler::Equal));
#endif
#ifdef ENABLE_WASM_REFTYPES
      case uint16_t(Op::RefNull):
        CHECK_NEXT(emitRefNull());
        break;
      case uint16_t(Op::RefIsNull):
        CHECK_NEXT(
            emitConversion(emitRefIsNull, ValType::AnyRef, ValType::I32));
        break;
#endif

      // "Miscellaneous" operations
      case uint16_t(Op::MiscPrefix): {
        switch (op.b1) {
          case uint32_t(MiscOp::I32TruncSSatF32):
            CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI32<TRUNC_SATURATING>,
                                         ValType::F32, ValType::I32));
          case uint32_t(MiscOp::I32TruncUSatF32):
            CHECK_NEXT(emitConversionOOM(
                emitTruncateF32ToI32<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F32, ValType::I32));
          case uint32_t(MiscOp::I32TruncSSatF64):
            CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI32<TRUNC_SATURATING>,
                                         ValType::F64, ValType::I32));
          case uint32_t(MiscOp::I32TruncUSatF64):
            CHECK_NEXT(emitConversionOOM(
                emitTruncateF64ToI32<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F64, ValType::I32));
          case uint32_t(MiscOp::I64TruncSSatF32):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToInt64, ValType::F32,
                ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI64<TRUNC_SATURATING>,
                                         ValType::F32, ValType::I64));
#endif
          case uint32_t(MiscOp::I64TruncUSatF32):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToUint64, ValType::F32,
                ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(
                emitTruncateF32ToI64<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F32, ValType::I64));
#endif
          case uint32_t(MiscOp::I64TruncSSatF64):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToInt64, ValType::F64,
                ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI64<TRUNC_SATURATING>,
                                         ValType::F64, ValType::I64));
#endif
          case uint32_t(MiscOp::I64TruncUSatF64):
#ifdef RABALDR_FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(
                emitConvertFloatingToInt64Callout,
                SymbolicAddress::SaturatingTruncateDoubleToUint64, ValType::F64,
                ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(
                emitTruncateF64ToI64<TRUNC_UNSIGNED | TRUNC_SATURATING>,
                ValType::F64, ValType::I64));
#endif
#ifdef ENABLE_WASM_BULKMEM_OPS
          case uint32_t(MiscOp::MemCopy):
            CHECK_NEXT(emitMemOrTableCopy(/*isMem=*/true));
          case uint32_t(MiscOp::DataDrop):
            CHECK_NEXT(emitDataOrElemDrop(/*isData=*/true));
          case uint32_t(MiscOp::MemFill):
            CHECK_NEXT(emitMemFill());
          case uint32_t(MiscOp::MemInit):
            CHECK_NEXT(emitMemOrTableInit(/*isMem=*/true));
          case uint32_t(MiscOp::TableCopy):
            CHECK_NEXT(emitMemOrTableCopy(/*isMem=*/false));
          case uint32_t(MiscOp::ElemDrop):
            CHECK_NEXT(emitDataOrElemDrop(/*isData=*/false));
          case uint32_t(MiscOp::TableInit):
            CHECK_NEXT(emitMemOrTableInit(/*isMem=*/false));
#endif  // ENABLE_WASM_BULKMEM_OPS
#ifdef ENABLE_WASM_REFTYPES
          case uint32_t(MiscOp::TableFill):
            CHECK_NEXT(emitTableFill());
          case uint32_t(MiscOp::TableGrow):
            CHECK_NEXT(emitTableGrow());
          case uint32_t(MiscOp::TableSize):
            CHECK_NEXT(emitTableSize());
#endif
#ifdef ENABLE_WASM_GC
          case uint32_t(MiscOp::StructNew):
            if (!env_.gcTypesEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(emitStructNew());
          case uint32_t(MiscOp::StructGet):
            if (!env_.gcTypesEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(emitStructGet());
          case uint32_t(MiscOp::StructSet):
            if (!env_.gcTypesEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(emitStructSet());
          case uint32_t(MiscOp::StructNarrow):
            if (!env_.gcTypesEnabled()) {
              return iter_.unrecognizedOpcode(&op);
            }
            CHECK_NEXT(emitStructNarrow());
#endif
          default:
            break;
        }  // switch (op.b1)
        return iter_.unrecognizedOpcode(&op);
      }

      // Thread operations
      case uint16_t(Op::ThreadPrefix): {
        switch (op.b1) {
          case uint32_t(ThreadOp::Wake):
            CHECK_NEXT(emitWake());

          case uint32_t(ThreadOp::I32Wait):
            CHECK_NEXT(emitWait(ValType::I32, 4));
          case uint32_t(ThreadOp::I64Wait):
            CHECK_NEXT(emitWait(ValType::I64, 8));

          case uint32_t(ThreadOp::I32AtomicLoad):
            CHECK_NEXT(emitAtomicLoad(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicLoad):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicLoad8U):
            CHECK_NEXT(emitAtomicLoad(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicLoad16U):
            CHECK_NEXT(emitAtomicLoad(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicLoad8U):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicLoad16U):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicLoad32U):
            CHECK_NEXT(emitAtomicLoad(ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicStore):
            CHECK_NEXT(emitAtomicStore(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicStore):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicStore8U):
            CHECK_NEXT(emitAtomicStore(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicStore16U):
            CHECK_NEXT(emitAtomicStore(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicStore8U):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicStore16U):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicStore32U):
            CHECK_NEXT(emitAtomicStore(ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicAdd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I32AtomicAdd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I32AtomicAdd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchAddOp));
          case uint32_t(ThreadOp::I64AtomicAdd32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchAddOp));

          case uint32_t(ThreadOp::I32AtomicSub):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I32AtomicSub8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I32AtomicSub16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchSubOp));
          case uint32_t(ThreadOp::I64AtomicSub32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchSubOp));

          case uint32_t(ThreadOp::I32AtomicAnd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I32AtomicAnd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I32AtomicAnd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchAndOp));
          case uint32_t(ThreadOp::I64AtomicAnd32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchAndOp));

          case uint32_t(ThreadOp::I32AtomicOr):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I32AtomicOr8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I32AtomicOr16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchOrOp));
          case uint32_t(ThreadOp::I64AtomicOr32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchOrOp));

          case uint32_t(ThreadOp::I32AtomicXor):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Int32, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Int64, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I32AtomicXor8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint8, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I32AtomicXor16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I32, Scalar::Uint16, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor8U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint8, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor16U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint16, AtomicFetchXorOp));
          case uint32_t(ThreadOp::I64AtomicXor32U):
            CHECK_NEXT(
                emitAtomicRMW(ValType::I64, Scalar::Uint32, AtomicFetchXorOp));

          case uint32_t(ThreadOp::I32AtomicXchg):
            CHECK_NEXT(emitAtomicXchg(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicXchg):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicXchg8U):
            CHECK_NEXT(emitAtomicXchg(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicXchg16U):
            CHECK_NEXT(emitAtomicXchg(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicXchg8U):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicXchg16U):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicXchg32U):
            CHECK_NEXT(emitAtomicXchg(ValType::I64, Scalar::Uint32));

          case uint32_t(ThreadOp::I32AtomicCmpXchg):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I32, Scalar::Int32));
          case uint32_t(ThreadOp::I64AtomicCmpXchg):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Int64));
          case uint32_t(ThreadOp::I32AtomicCmpXchg8U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I32, Scalar::Uint8));
          case uint32_t(ThreadOp::I32AtomicCmpXchg16U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I32, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicCmpXchg8U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Uint8));
          case uint32_t(ThreadOp::I64AtomicCmpXchg16U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Uint16));
          case uint32_t(ThreadOp::I64AtomicCmpXchg32U):
            CHECK_NEXT(emitAtomicCmpXchg(ValType::I64, Scalar::Uint32));

          default:
            return iter_.unrecognizedOpcode(&op);
        }
        break;
      }

      // asm.js and other private operations
      case uint16_t(Op::MozPrefix):
        return iter_.unrecognizedOpcode(&op);

      default:
        return iter_.unrecognizedOpcode(&op);
    }

#undef CHECK
#undef NEXT
#undef CHECK_NEXT
#undef CHECK_POINTER_COUNT
#undef emitBinary
#undef emitUnary
#undef emitComparison
#undef emitConversion
#undef emitConversionOOM
#undef emitCalloutConversionOOM

    MOZ_CRASH("unreachable");
  }

  MOZ_CRASH("unreachable");
}

bool BaseCompiler::emitFunction() {
  if (!beginFunction()) {
    return false;
  }

  if (!emitBody()) {
    return false;
  }

  if (!endFunction()) {
    return false;
  }

  return true;
}

BaseCompiler::BaseCompiler(const ModuleEnvironment& env,
                           const FuncCompileInput& func,
                           const ValTypeVector& locals,
                           const MachineState& trapExitLayout,
                           size_t trapExitLayoutNumWords, Decoder& decoder,
                           StkVector& stkSource, TempAllocator* alloc,
                           MacroAssembler* masm, StackMaps* stackMaps)
    : env_(env),
      iter_(env, decoder),
      func_(func),
      lastReadCallSite_(0),
      alloc_(*alloc),
      locals_(locals),
      deadCode_(false),
      bceSafe_(0),
      latentOp_(LatentOp::None),
      latentType_(ValType::I32),
      latentIntCmp_(Assembler::Equal),
      latentDoubleCmp_(Assembler::DoubleEqual),
      masm(*masm),
      ra(*this),
      fr(*masm),
      stackMapGenerator_(stackMaps, trapExitLayout, trapExitLayoutNumWords,
                         *masm),
      joinRegI32_(RegI32(ReturnReg)),
      joinRegI64_(RegI64(ReturnReg64)),
      joinRegPtr_(RegPtr(ReturnReg)),
      joinRegF32_(RegF32(ReturnFloat32Reg)),
      joinRegF64_(RegF64(ReturnDoubleReg)),
      stkSource_(stkSource) {
  // Our caller, BaselineCompileFunctions, will lend us the vector contents to
  // use for the eval stack.  To get hold of those contents, we'll temporarily
  // installing an empty one in its place.
  MOZ_ASSERT(stk_.empty());
  stk_.swap(stkSource_);

  // Assuming that previously processed wasm functions are well formed, the
  // eval stack should now be empty.  But empty it anyway; any non-emptyness
  // at this point will cause chaos.
  stk_.clear();
}

BaseCompiler::~BaseCompiler() {
  stk_.swap(stkSource_);
  // We've returned the eval stack vector contents to our caller,
  // BaselineCompileFunctions.  We expect the vector we get in return to be
  // empty since that's what we swapped for the stack vector in our
  // constructor.
  MOZ_ASSERT(stk_.empty());
}

bool BaseCompiler::init() {
  if (!SigD_.append(ValType::F64)) {
    return false;
  }
  if (!SigF_.append(ValType::F32)) {
    return false;
  }

  if (!fr.setupLocals(locals_, funcType().args(), env_.debugEnabled(),
                      &localInfo_)) {
    return false;
  }

  return true;
}

FuncOffsets BaseCompiler::finish() {
  MOZ_ASSERT(done(), "all bytes must be consumed");
  MOZ_ASSERT(func_.callSiteLineNums.length() == lastReadCallSite_);

  MOZ_ASSERT(stk_.empty());
  MOZ_ASSERT(stackMapGenerator_.memRefsOnStk == 0);

  masm.flushBuffer();

  return offsets_;
}

}  // namespace wasm
}  // namespace js

bool js::wasm::BaselineCanCompile() {
#if defined(JS_CODEGEN_ARM)
  // Simplifying assumption: require SDIV and UDIV.
  //
  // I have no good data on ARM populations allowing me to say that
  // X% of devices in the market implement SDIV and UDIV.  However,
  // they are definitely implemented on the Cortex-A7 and Cortex-A15
  // and on all ARMv8 systems.
  if (!HasIDIV()) {
    return false;
  }
#endif

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) ||   \
    defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
  return true;
#else
  return false;
#endif
}

bool js::wasm::BaselineCompileFunctions(const ModuleEnvironment& env,
                                        LifoAlloc& lifo,
                                        const FuncCompileInputVector& inputs,
                                        CompiledCode* code,
                                        UniqueChars* error) {
  MOZ_ASSERT(env.tier() == Tier::Baseline);
  MOZ_ASSERT(env.kind == ModuleKind::Wasm);

  // The MacroAssembler will sometimes access the jitContext.

  TempAllocator alloc(&lifo);
  JitContext jitContext(&alloc);
  MOZ_ASSERT(IsCompilingWasm());
  WasmMacroAssembler masm(alloc);

  // Swap in already-allocated empty vectors to avoid malloc/free.
  MOZ_ASSERT(code->empty());
  if (!code->swap(masm)) {
    return false;
  }

  // Create a description of the stack layout created by GenerateTrapExit().
  MachineState trapExitLayout;
  size_t trapExitLayoutNumWords;
  GenerateTrapExitMachineState(&trapExitLayout, &trapExitLayoutNumWords);

  // The compiler's operand stack.  We reuse it across all functions so as to
  // avoid malloc/free.  Presize it to 128 elements in the hope of avoiding
  // reallocation later.
  StkVector stk;
  if (!stk.reserve(128)) {
    return false;
  }

  for (const FuncCompileInput& func : inputs) {
    Decoder d(func.begin, func.end, func.lineOrBytecode, error);

    // Build the local types vector.

    ValTypeVector locals;
    if (!locals.appendAll(env.funcTypes[func.index]->args())) {
      return false;
    }
    if (!DecodeLocalEntries(d, env.types, env.gcTypesEnabled(), &locals)) {
      return false;
    }

    // One-pass baseline compilation.

    BaseCompiler f(env, func, locals, trapExitLayout, trapExitLayoutNumWords, d,
                   stk, &alloc, &masm, &code->stackMaps);
    if (!f.init()) {
      return false;
    }
    if (!f.emitFunction()) {
      return false;
    }
    if (!code->codeRanges.emplaceBack(func.index, func.lineOrBytecode,
                                      f.finish())) {
      return false;
    }
  }

  masm.finish();
  if (masm.oom()) {
    return false;
  }

  return code->swap(masm);
}

#ifdef DEBUG
bool js::wasm::IsValidStackMapKey(bool debugEnabled, const uint8_t* nextPC) {
#  if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
  const uint8_t* insn = nextPC;
  return (insn[-2] == 0x0F && insn[-1] == 0x0B) ||  // ud2
         (insn[-2] == 0xFF && insn[-1] == 0xD0) ||  // call *%{rax,eax}
         insn[-5] == 0xE8 ||                        // call simm32
         (debugEnabled && insn[-5] == 0x0F && insn[-4] == 0x1F &&
          insn[-3] == 0x44 && insn[-2] == 0x00 &&
          insn[-1] == 0x00);  // nop_five

#  elif defined(JS_CODEGEN_ARM)
  const uint32_t* insn = (const uint32_t*)nextPC;
  return ((uintptr_t(insn) & 3) == 0) &&              // must be ARM, not Thumb
         (insn[-1] == 0xe7f000f0 ||                   // udf
          (insn[-1] & 0xfffffff0) == 0xe12fff30 ||    // blx reg (ARM, enc A1)
          (insn[-1] & 0xff000000) == 0xeb000000 ||    // bl simm24 (ARM, enc A1)
          (debugEnabled && insn[-1] == 0xe320f000));  // "as_nop"

#  elif defined(JS_CODEGEN_ARM64)
#    ifdef JS_SIMULATOR_ARM64
  const uint32_t hltInsn = 0xd45bd600;
#    else
  const uint32_t hltInsn = 0xd4a00000;
#    endif
  const uint32_t* insn = (const uint32_t*)nextPC;
  return ((uintptr_t(insn) & 3) == 0) &&
         (insn[-1] == hltInsn ||                      // hlt
          (insn[-1] & 0xfffffc1f) == 0xd63f0000 ||    // blr reg
          (insn[-1] & 0xfc000000) == 0x94000000 ||    // bl simm26
          (debugEnabled && insn[-1] == 0xd503201f));  // nop

#  else
  MOZ_CRASH("IsValidStackMapKey: requires implementation on this platform");
#  endif
}
#endif

#undef RABALDR_INT_DIV_I64_CALLOUT
#undef RABALDR_I64_TO_FLOAT_CALLOUT
#undef RABALDR_FLOAT_TO_I64_CALLOUT
